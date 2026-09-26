#include "graphics/host_gpu/renderer/image/dccClear.h"

#include "common/alignment.h"
#include "common/assert.h"
#include "common/profiler.h"
#include "gpu_dcc_shaders/gpu_dcc_clear_rgba16f_spv.h"
#include "gpu_dcc_shaders/gpu_dcc_validate_spv.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/cache/streamBuffer.h"
#include "graphics/host_gpu/renderer/commandScheduler.h"
#include "graphics/host_gpu/renderer/image/image.h"
#include "graphics/host_gpu/renderer/gpuOpProfiler.h"

#include <algorithm>
#include <array>
#include <limits>
#include <span>

namespace Libs::Graphics {

DccClearHelper::DccClearHelper(GraphicContext& graphics, CommandScheduler& scheduler)
    : m_graphics(graphics), m_scheduler(scheduler) {
	static_assert(sizeof(Push) == 24);
	const auto& limits = graphics.physical_device_properties.limits;
	const auto features = graphics.GetFormatProperties(vk::Format::eR16G16B16A16Sfloat);
	m_supported = static_cast<bool>(features.optimalTilingFeatures &
	                                vk::FormatFeatureFlagBits::eStorageImage) &&
	              graphics.max_push_descriptors >= 3 &&
	              limits.maxComputeWorkGroupInvocations >= WorkgroupSize &&
	              limits.maxComputeWorkGroupSize[0] >= WorkgroupSize &&
	              limits.maxComputeWorkGroupCount[0] != 0 &&
	              limits.maxComputeSharedMemorySize >= sizeof(uint32_t) &&
	              limits.maxPushConstantsSize >= sizeof(Push) &&
	              graphics.StorageMinAlignment() <= std::numeric_limits<uint32_t>::max();
	if (!m_supported) {
		return;
	}

	const std::array<vk::DescriptorSetLayoutBinding, 3> bindings {{
	    {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr},
	    {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr},
	    {2, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute, nullptr},
	}};
	vk::DescriptorSetLayoutCreateInfo descriptor_info {};
	descriptor_info.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
	descriptor_info.bindingCount = static_cast<uint32_t>(bindings.size());
	descriptor_info.pBindings = bindings.data();
	RequireVulkanSuccess(graphics.device.createDescriptorSetLayout(&descriptor_info, nullptr,
	                                                              &m_descriptor_layout),
	                     "create DCC clear descriptor layout");
	const vk::PushConstantRange push_range {vk::ShaderStageFlagBits::eCompute, 0, sizeof(Push)};
	vk::PipelineLayoutCreateInfo layout_info {};
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &m_descriptor_layout;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = &push_range;
	RequireVulkanSuccess(graphics.device.createPipelineLayout(&layout_info, nullptr,
	                                                         &m_pipeline_layout),
	                     "create DCC clear pipeline layout");
	const auto create_pipeline = [&](std::span<const uint32_t> spirv, vk::Pipeline& pipeline) {
		const auto module = CompileSPV(spirv, graphics.device);
		vk::PipelineShaderStageCreateInfo stage {};
		stage.stage = vk::ShaderStageFlagBits::eCompute;
		stage.module = module;
		stage.pName = "main";
		vk::ComputePipelineCreateInfo create {};
		create.stage = stage;
		create.layout = m_pipeline_layout;
		const auto result =
		    graphics.device.createComputePipelines(nullptr, 1, &create, nullptr, &pipeline);
		graphics.device.destroyShaderModule(module, nullptr);
		RequireVulkanSuccess(result, "create DCC clear compute pipeline");
	};
	create_pipeline(GPU_DCC_VALIDATE_SPV, m_validate_pipeline);
	create_pipeline(GPU_DCC_CLEAR_RGBA16F_SPV, m_clear_pipeline);
	m_scratch = std::make_unique<Buffer>(
	    graphics, scheduler, MemoryUsage::DeviceLocal, 0,
	    vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
	    ScratchSize);
}

DccClearHelper::~DccClearHelper() {
	// The owning TextureCache is destroyed after RenderContext drains its scheduler.
	if (m_validate_pipeline != nullptr) {
		m_graphics.device.destroyPipeline(m_validate_pipeline, nullptr);
	}
	if (m_clear_pipeline != nullptr) {
		m_graphics.device.destroyPipeline(m_clear_pipeline, nullptr);
	}
	if (m_pipeline_layout != nullptr) {
		m_graphics.device.destroyPipelineLayout(m_pipeline_layout, nullptr);
	}
	if (m_descriptor_layout != nullptr) {
		m_graphics.device.destroyDescriptorSetLayout(m_descriptor_layout, nullptr);
	}
}

bool DccClearHelper::Supports(const Image& image, uint64_t metadata_size) const {
	const auto& native = image.backing;
	if (!m_supported || native.image == nullptr ||
	    native.format != vk::Format::eR16G16B16A16Sfloat ||
	    native.image_type != vk::ImageType::e2D || native.samples != 1 || native.layers != 1 ||
	    native.mip_levels != 1 || native.extent.depth != 1 || native.extent.width == 0 ||
	    native.extent.height == 0 || !(native.usage & vk::ImageUsageFlagBits::eStorage) ||
	    metadata_size == 0 || metadata_size > MaxMetadataSize || metadata_size % 4096 != 0) {
		return false;
	}
	const auto& limits = m_graphics.physical_device_properties.limits;
	const uint64_t pixels = static_cast<uint64_t>(native.extent.width) * native.extent.height;
	// Keep the shader's rounded invocation count and index arithmetic within uint32_t.
	const uint64_t max_elements = std::numeric_limits<uint32_t>::max() - (WorkgroupSize - 1u);
	const auto alignment = std::max<uint64_t>(m_graphics.StorageMinAlignment(), 4);
	return pixels <= max_elements &&
	       metadata_size + alignment - 1 <= limits.maxStorageBufferRange;
}

void DccClearHelper::Record(Image& image, vk::Buffer metadata, uint64_t metadata_offset,
                           uint64_t metadata_size, bool alpha_msb) {
	KYTY_GPU_OP_SITE("dcc.clear");
	KYTY_PROFILER_DETAIL_FUNCTION();
	EXIT_IF(!Supports(image, metadata_size) || metadata == nullptr || metadata_offset % 4 != 0 ||
	        metadata_offset > std::numeric_limits<uint64_t>::max() - metadata_size);
	const auto alignment = std::max<uint64_t>(m_graphics.StorageMinAlignment(), 4);
	const auto descriptor_offset = Common::AlignDown(metadata_offset, alignment);
	const auto prefix = metadata_offset - descriptor_offset;
	const auto pixels = static_cast<uint64_t>(image.backing.extent.width) * image.backing.extent.height;
	const auto elements = std::max(pixels, metadata_size / sizeof(uint32_t));
	const auto groups = std::min<uint64_t>(
	    (elements + WorkgroupSize - 1) / WorkgroupSize,
	    m_graphics.physical_device_properties.limits.maxComputeWorkGroupCount[0]);
	const Push push {static_cast<uint32_t>(prefix / sizeof(uint32_t)),
	                 static_cast<uint32_t>(metadata_size / sizeof(uint32_t)),
	                 image.backing.extent.width, image.backing.extent.height,
	                 static_cast<uint32_t>(groups), alpha_msb ? 1u : 0u};
	ImageViewInfo view_info {};
	view_info.format = vk::Format::eR16G16B16A16Sfloat;
	view_info.usage = vk::ImageUsageFlagBits::eStorage;
	const auto view = image.FindView(view_info);

	m_scheduler.EndRendering();
	const auto command = m_scheduler.Current().Handle();
	image.Transit(vk::ImageLayout::eGeneral, vk::AccessFlagBits2::eShaderWrite, std::nullopt,
	              command);
	std::array<vk::BufferMemoryBarrier2, 2> before {};
	before[0].srcStageMask = vk::PipelineStageFlagBits2::eAllCommands |
	                         vk::PipelineStageFlagBits2::eHost;
	before[0].srcAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite |
	                          vk::AccessFlagBits2::eHostWrite;
	before[0].dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
	before[0].dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
	before[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before[0].buffer = metadata;
	before[0].offset = metadata_offset;
	before[0].size = metadata_size;
	before[1] = before[0];
	// One scratch allocation is reused in queue order, including across command buffers.
	before[1].srcStageMask = vk::PipelineStageFlagBits2::eDrawIndirect |
	                         vk::PipelineStageFlagBits2::eComputeShader;
	before[1].srcAccessMask = vk::AccessFlagBits2::eIndirectCommandRead |
	                          vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
	before[1].dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
	before[1].buffer = m_scratch->Handle();
	before[1].offset = 0;
	before[1].size = ScratchSize;
	vk::DependencyInfo dependency {};
	dependency.bufferMemoryBarrierCount = static_cast<uint32_t>(before.size());
	dependency.pBufferMemoryBarriers = before.data();
	command.pipelineBarrier2(dependency);

	const vk::DescriptorBufferInfo metadata_info {metadata, descriptor_offset, prefix + metadata_size};
	const vk::DescriptorBufferInfo scratch_info {m_scratch->Handle(), 0, ScratchSize};
	const vk::DescriptorImageInfo image_info {nullptr, view, vk::ImageLayout::eGeneral};
	std::array<vk::WriteDescriptorSet, 3> writes {};
	for (uint32_t index = 0; index < writes.size(); ++index) {
		writes[index].dstBinding = index;
		writes[index].descriptorCount = 1;
		writes[index].descriptorType =
		    index == 2 ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eStorageBuffer;
	}
	writes[0].pBufferInfo = &metadata_info;
	writes[1].pBufferInfo = &scratch_info;
	writes[2].pImageInfo = &image_info;
	m_scheduler.Current().PushDescriptors(vk::PipelineBindPoint::eCompute, m_pipeline_layout, 0,
	                             static_cast<uint32_t>(writes.size()), writes.data());
	command.pushConstants(m_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push),
	                      &push);
	m_scheduler.Current().BindPipeline(vk::PipelineBindPoint::eCompute, m_validate_pipeline);
	command.dispatch(1, 1, 1);

	// Publish the decision and order every metadata read before conditional metadata writes.
	vk::MemoryBarrier2 decision {};
	decision.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader;
	decision.srcAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
	decision.dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect |
	                        vk::PipelineStageFlagBits2::eComputeShader;
	decision.dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead |
	                         vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
	dependency = {};
	dependency.memoryBarrierCount = 1;
	dependency.pMemoryBarriers = &decision;
	command.pipelineBarrier2(dependency);
	m_scheduler.Current().BindPipeline(vk::PipelineBindPoint::eCompute, m_clear_pipeline);
	command.dispatchIndirect(m_scratch->Handle(), 0);

	// Image state remains GENERAL/shader-write so subsequent transitions also retain this writer.
	vk::MemoryBarrier2 complete {};
	complete.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader;
	complete.srcAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
	complete.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
	complete.dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
	dependency.pMemoryBarriers = &complete;
	command.pipelineBarrier2(dependency);
}

} // namespace Libs::Graphics
