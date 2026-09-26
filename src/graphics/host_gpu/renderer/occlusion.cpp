#include "graphics/host_gpu/renderer/occlusion.h"
#include "common/hangTrace.h"
#include "common/alignment.h"
#include "common/profiler.h"
#include "gpu_dcc_shaders/gpu_dcc_occlusion_spv.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "kernel/memory.h"
#include "graphics/host_gpu/renderer/gpuOpProfiler.h"
#include <array>
#include <cstdlib>
#include <cstring>

namespace Libs::Graphics {
bool OcclusionCounter::Enabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_GPU_OCCLUSION");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

OcclusionCounter::OcclusionCounter(RenderContext& context): m_context(context) {}
OcclusionCounter::~OcclusionCounter() {
	// RenderContext drains its scheduler before member destruction.
	auto device = m_context.GetGraphics().device;
	if (m_pool) device.destroyQueryPool(m_pool);
	if (m_pipeline) device.destroyPipeline(m_pipeline);
	if (m_layout) device.destroyPipelineLayout(m_layout);
	if (m_descriptors) device.destroyDescriptorSetLayout(m_descriptors);
}

void OcclusionCounter::Initialize() {
	if (m_pool) return;
	auto& graphics = m_context.GetGraphics();
	auto& scheduler = m_context.GetCommandScheduler();
	EXIT_IF(!graphics.precise_occlusion_enabled || graphics.max_push_descriptors < 3);
	vk::QueryPoolCreateInfo query {};
	query.queryType = vk::QueryType::eOcclusion;
	query.queryCount = QueryCapacity;
	RequireVulkanSuccess(graphics.device.createQueryPool(&query, nullptr, &m_pool), "create occlusion pool");
	const std::array<vk::DescriptorSetLayoutBinding, 3> bindings {{
	    {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
	    {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
	    {2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
	}};
	vk::DescriptorSetLayoutCreateInfo descriptor {};
	descriptor.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
	descriptor.bindingCount = static_cast<uint32_t>(bindings.size());
	descriptor.pBindings = bindings.data();
	RequireVulkanSuccess(graphics.device.createDescriptorSetLayout(&descriptor, nullptr, &m_descriptors),
	                     "create occlusion descriptors");
	const vk::PushConstantRange push {vk::ShaderStageFlagBits::eCompute, 0, 12};
	vk::PipelineLayoutCreateInfo layout {};
	layout.setLayoutCount = 1;
	layout.pSetLayouts = &m_descriptors;
	layout.pushConstantRangeCount = 1;
	layout.pPushConstantRanges = &push;
	RequireVulkanSuccess(graphics.device.createPipelineLayout(&layout, nullptr, &m_layout), "create occlusion layout");
	const auto module = CompileSPV(GPU_DCC_OCCLUSION_SPV, graphics.device);
	vk::ComputePipelineCreateInfo pipeline {};
	pipeline.layout = m_layout;
	pipeline.stage.stage = vk::ShaderStageFlagBits::eCompute;
	pipeline.stage.module = module;
	pipeline.stage.pName = "main";
	const auto result = graphics.device.createComputePipelines(nullptr, 1, &pipeline, nullptr, &m_pipeline);
	graphics.device.destroyShaderModule(module);
	RequireVulkanSuccess(result, "create occlusion reduction pipeline");
	const auto usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
	m_counter = std::make_unique<Buffer>(graphics, scheduler, MemoryUsage::DeviceLocal, 0, usage, 256);
	m_result = std::make_unique<Buffer>(graphics, scheduler, MemoryUsage::DeviceLocal, 0, usage,
	                                  QueryCapacity * sizeof(uint64_t));
	m_publish = std::make_unique<Buffer>(graphics, scheduler, MemoryUsage::Download, 0, usage,
	                                   PublishSlots * PublishSlotSize);
	scheduler.Current().Handle().fillBuffer(m_counter->Handle(), 0, 256, 0);
}

void OcclusionCounter::Prepare(uint32_t control) {
	EXIT_IF(m_active || m_pending >= QueryCapacity);
	m_prepared = false;
	if (!Enabled() || (control & 1u) != 0 || (control & 0xf00u) == 0) return;
	// GFX10 ZPASS enable 1 counts all samples. Other counter selectors and slice
	// filtering require additional emulation; never report them as invisibility.
	if ((control & 0x00ffff00u) != 0x100u || (control >> 24u) != 0x11u) {
		EXIT("unsupported occlusion counter mode: DB_COUNT_CONTROL=0x%08x\n", control);
	}
	Initialize();
	m_context.GetCommandScheduler().Current().Handle().resetQueryPool(m_pool, m_pending, 1);
	m_prepared = true;
}

void OcclusionCounter::Begin() {
	if (!m_prepared) return;
	m_context.GetCommandScheduler().Current().Handle().beginQuery(m_pool, m_pending, vk::QueryControlFlagBits::ePrecise);
	m_active = true;
	m_prepared = false;
	Profiler::CountFrameEvent(Profiler::FrameEvent::NativeOcclusionScopes);
}

void OcclusionCounter::End() {
	if (!m_active) return;
	m_context.GetCommandScheduler().Current().Handle().endQuery(m_pool, m_pending);
	m_active = false;
	++m_pending;
}

void OcclusionCounter::Accumulate() {
	// Keep ended queries in distinct slots across native rendering boundaries and
	// submissions. Only a guest snapshot or bounded pool exhaustion needs a sum.
	if (m_pending == QueryCapacity) FlushPending();
}

void OcclusionCounter::FlushPending() {
	KYTY_GPU_OP_SITE("occlusion.flush");
	EXIT_IF(m_active || m_prepared);
	if (!m_pending) return;
	auto native = m_context.GetCommandScheduler().Current().Handle();
	native.copyQueryPoolResults(m_pool, 0, m_pending, m_result->Handle(), 0, 8,
	                            vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
	Dispatch(0, m_counter->Handle(), 0, m_counter->Size());
	m_pending = 0;
	Profiler::CountFrameEvent(Profiler::FrameEvent::NativeOcclusionReductions);
}

void OcclusionCounter::Dispatch(uint32_t mode, vk::Buffer output, uint64_t offset, uint64_t range) {
	KYTY_GPU_OP_SITE("occlusion.reduce");
	auto& command = m_context.GetCommandScheduler().Current();
	auto native = command.Handle();
	const auto alignment = m_context.GetGraphics().StorageMinAlignment();
	const auto aligned = Common::AlignDown(offset, alignment);
	EXIT_IF(offset - aligned > UINT32_MAX || range > UINT32_MAX);
	const vk::DescriptorBufferInfo infos[] {
	    {m_result->Handle(), 0, m_result->Size()}, {m_counter->Handle(), 0, 8}, {output, aligned, range + offset - aligned}};
	std::array<vk::WriteDescriptorSet, 3> writes {};
	for (uint32_t i = 0; i < writes.size(); ++i) {
		writes[i].dstBinding = i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = vk::DescriptorType::eStorageBuffer;
		writes[i].pBufferInfo = &infos[i];
	}
	vk::MemoryBarrier barrier {};
	barrier.srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
	native.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader,
	                       {}, 1, &barrier, 0, nullptr, 0, nullptr);
	command.BindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline);
	command.PushDescriptors(vk::PipelineBindPoint::eCompute, m_layout, 0,
	                        static_cast<uint32_t>(writes.size()), writes.data());
	const uint32_t push[] {mode, static_cast<uint32_t>(offset - aligned), m_pending};
	native.pushConstants(m_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push), push);
	native.dispatch(1, 1, 1);
	barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	native.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eAllCommands,
	                       {}, 1, &barrier, 0, nullptr, 0, nullptr);
}

void OcclusionCounter::Dump(uint64_t address) {
	auto& scheduler = m_context.GetCommandScheduler();
	scheduler.EndRendering();
	Initialize();
	FlushPending();
	// Reduce into a private slot. A slot is reused only after its previous publication's tick
	// has completed (1024 dumps in flight never happens in practice; the wait bounds it).
	const auto slot = static_cast<uint32_t>(m_issued % PublishSlots);
	if (m_issued >= PublishSlots && !scheduler.IsFree(m_slot_ticks[slot])) {
		scheduler.Wait(m_slot_ticks[slot]);
	}
	const uint64_t slot_offset = uint64_t {slot} * PublishSlotSize;
	scheduler.EndRendering();
	Dispatch(1, m_publish->Handle(), slot_offset, 248);
	m_slot_ticks[slot] = scheduler.CurrentTick();
	++m_issued;
	// The shader writes the first qword of each of the 16 interleaved begin/end pairs and leaves
	// the other member untouched; publish exactly those qwords.
	scheduler.DeferOperation([this, address, slot_offset] {
		m_publish->Invalidate(slot_offset, 248);
		const auto* source = m_publish->Mapped().data() + slot_offset;
		for (uint32_t db = 0; db < 16u; db++) {
			(void)LibKernel::Memory::TryWriteBacking(address + db * 16u, source + db * 16u,
			                                         sizeof(uint64_t));
		}
		m_published.fetch_add(1, std::memory_order_release);
	});
	Profiler::CountFrameEvent(Profiler::FrameEvent::NativeOcclusionDumps);
}
}
