#include "common/assert.h"
#include "common/common.h"
#include "common/hangTrace.h"
#include "common/profiler.h"
#include "common/rendererBatch.h"
#include "common/threads.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/host_gpu/renderer/colorRenderTarget.h"
#include "graphics/host_gpu/renderer/debug.h"
#include "graphics/host_gpu/renderer/depthRenderTarget.h"
#include "graphics/host_gpu/renderer/image/imageView.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <algorithm>
#include <bit>
#include <cstring>
namespace Libs::Graphics {

CommandBuffer::CommandBuffer(CommandScheduler& scheduler)
    : m_context(scheduler.Context()), m_graphics(scheduler.Graphics()) {}

bool CommandBuffer::IsInvalid() const {
	return m_buffer == nullptr;
}

vk::CommandBuffer CommandBuffer::Handle() const {
	EXIT_IF(IsInvalid());
	return m_buffer;
}

void CommandBuffer::Begin() {
	EXIT_IF(m_rendering || IsInvalid());
	m_bound_pipelines = {};
	for (auto& state: m_descriptor_states) state.layout = nullptr;
	auto buffer = Handle();

	vk::CommandBufferBeginInfo begin_info {};
	begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	auto result = buffer.begin(&begin_info);

	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess);
}

void CommandBuffer::End() const {
	EndRendering();
	auto buffer = Handle();

	auto result = buffer.end();

	EXIT_NOT_IMPLEMENTED(result != vk::Result::eSuccess);
}

static size_t BindingPointIndex(vk::PipelineBindPoint point) {
	EXIT_IF(point != vk::PipelineBindPoint::eGraphics && point != vk::PipelineBindPoint::eCompute);
	return point == vk::PipelineBindPoint::eCompute ? 1u : 0u;
}

void CommandBuffer::BindPipeline(vk::PipelineBindPoint point, vk::Pipeline pipeline) {
	auto& current = m_bound_pipelines[BindingPointIndex(point)];
	if (Common::RendererBatchEnabled() && current == pipeline) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::PipelineBindsAvoided);
		return;
	}
	Handle().bindPipeline(point, pipeline);
	current = pipeline;
}

void CommandBuffer::InvalidateDescriptors(vk::PipelineBindPoint point) {
	m_descriptor_states[BindingPointIndex(point)].layout = nullptr;
}

void CommandBuffer::PushDescriptors(vk::PipelineBindPoint point, vk::PipelineLayout layout,
                                    uint32_t set, uint32_t count,
                                    const vk::WriteDescriptorSet* writes) {
	auto& state = m_descriptor_states[BindingPointIndex(point)];
	bool supported = Common::RendererBatchEnabled() && set == 0;
	size_t buffer_count = 0, image_count = 0;
	for (uint32_t i = 0; supported && i < count; ++i) {
		const auto& write = writes[i];
		const bool buffer_type = write.descriptorType == vk::DescriptorType::eStorageBuffer ||
		                         write.descriptorType == vk::DescriptorType::eUniformBuffer;
		const bool image_type = write.descriptorType == vk::DescriptorType::eSampler ||
		                        write.descriptorType == vk::DescriptorType::eSampledImage ||
		                        write.descriptorType == vk::DescriptorType::eStorageImage ||
		                        write.descriptorType == vk::DescriptorType::eCombinedImageSampler;
		supported = write.pNext == nullptr && write.pTexelBufferView == nullptr &&
		            ((buffer_type && write.pBufferInfo != nullptr) ||
		             (image_type && write.pImageInfo != nullptr));
		buffer_count += buffer_type ? write.descriptorCount : 0u;
		image_count += image_type ? write.descriptorCount : 0u;
	}
	bool equal = supported && state.layout == layout && state.writes.size() == count &&
	             state.buffers.size() == buffer_count && state.images.size() == image_count;
	size_t buffer_index = 0, image_index = 0;
	for (uint32_t i = 0; equal && i < count; ++i) {
		const auto& write = writes[i];
		const auto& old = state.writes[i];
		equal = write.dstBinding == old.dstBinding && write.dstArrayElement == old.dstArrayElement &&
		        write.descriptorCount == old.descriptorCount && write.descriptorType == old.descriptorType;
		const bool buffer_type = write.descriptorType == vk::DescriptorType::eStorageBuffer ||
		                         write.descriptorType == vk::DescriptorType::eUniformBuffer;
		for (uint32_t j = 0; equal && j < write.descriptorCount; ++j) {
			if (buffer_type) equal = write.pBufferInfo[j] == state.buffers[buffer_index++];
			else equal = write.pImageInfo[j] == state.images[image_index++];
		}
	}
	if (equal) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::DescriptorPushesAvoided);
		return;
	}
	Handle().pushDescriptorSetKHR(point, layout, set, count, writes);
	state.layout = nullptr;
	if (!supported) return;
	state.writes.assign(writes, writes + count);
	state.buffers.clear();
	state.images.clear();
	state.buffers.reserve(buffer_count);
	state.images.reserve(image_count);
	for (auto& write: state.writes) {
		const bool buffer_type = write.descriptorType == vk::DescriptorType::eStorageBuffer ||
		                         write.descriptorType == vk::DescriptorType::eUniformBuffer;
		if (buffer_type) state.buffers.insert(state.buffers.end(), write.pBufferInfo,
		                                     write.pBufferInfo + write.descriptorCount);
		else state.images.insert(state.images.end(), write.pImageInfo,
		                         write.pImageInfo + write.descriptorCount);
		write.pBufferInfo = nullptr;
		write.pImageInfo = nullptr;
	}
	state.layout = layout;
}

void CommandBuffer::SetDebugInfo(uint32_t op, uint64_t submit_id, uint32_t arg0, uint32_t arg1,
                                 uint32_t arg2, uint32_t arg3, uint64_t arg4) {
	m_debug_op        = op;
	m_debug_submit_id = submit_id;
	m_debug_arg0      = arg0;
	m_debug_arg1      = arg1;
	m_debug_arg2      = arg2;
	m_debug_arg3      = arg3;
	m_debug_arg4      = arg4;
}

void CommandBuffer::BeginRendering(const RenderState& state) const {
	const auto count_control = GetRegisters().GetDepthCountControl();
	if (m_rendering && m_render_state == state && m_occlusion_control == count_control) {
		return;
	}
	EXIT_IF(state.width == 0 || state.height == 0 || state.num_layers == 0 ||
	        state.num_color_attachments > RENDER_COLOR_ATTACHMENTS_MAX);
	EndRendering();
	m_context.GetOcclusionCounter().Prepare(count_control);

	std::array<vk::RenderingAttachmentInfo, RENDER_COLOR_ATTACHMENTS_MAX> colors {};
	for (uint32_t i = 0; i < state.num_color_attachments; i++) {
		const auto& attachment = state.color_attachments[i];
		colors[i].imageView    = attachment.image_view;
		colors[i].imageLayout  = attachment.image_layout;
		colors[i].loadOp =
		    attachment.is_clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
		colors[i].storeOp                 = vk::AttachmentStoreOp::eStore;
		colors[i].clearValue.color.uint32 = attachment.clear_value;
	}

	const auto&                 depth_stencil = state.depth_stencil_attachment;
	vk::RenderingAttachmentInfo depth {};
	depth.imageView   = depth_stencil.image_view;
	depth.imageLayout = depth_stencil.image_layout;
	depth.loadOp =
	    depth_stencil.depth_clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
	depth.storeOp                       = vk::AttachmentStoreOp::eStore;
	depth.clearValue.depthStencil.depth = std::bit_cast<float>(depth_stencil.clear_value[0]);

	vk::RenderingAttachmentInfo stencil {};
	stencil.imageView   = depth_stencil.image_view;
	stencil.imageLayout = depth_stencil.image_layout;
	stencil.loadOp =
	    depth_stencil.stencil_clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
	stencil.storeOp                         = vk::AttachmentStoreOp::eStore;
	stencil.clearValue.depthStencil.stencil = depth_stencil.clear_value[1];

	vk::RenderingInfo rendering {};
	rendering.renderArea.extent    = {state.width, state.height};
	rendering.layerCount           = state.num_layers;
	rendering.colorAttachmentCount = state.num_color_attachments;
	rendering.pColorAttachments    = colors.data();
	rendering.pDepthAttachment     = depth_stencil.has_depth ? &depth : nullptr;
	rendering.pStencilAttachment   = depth_stencil.has_stencil ? &stencil : nullptr;
	Handle().beginRendering(rendering);
	m_context.GetOcclusionCounter().Begin();
	if (m_context.GetOcclusionCounter().Active() && HangTrace::Enabled()) {
		const auto& db = GetRegisters().GetDepthRenderTarget();
		m_context.GetOcclusionCounter().NoteScope(db.z_read_base_addr, state.width, state.height,
		                                          state.num_color_attachments,
		                                          depth_stencil.has_depth,
		                                          static_cast<uint32_t>(db.z_info.format));
	}
	m_render_state = state;
	m_rendering    = true;
	++m_rendering_serial;
	m_occlusion_control = count_control;
}

void CommandBuffer::EndRendering() const {
	if (!m_rendering) {
		return;
	}
	m_context.GetOcclusionCounter().End();
	Handle().endRendering();
	m_rendering    = false;
	m_render_state = {};
	m_context.GetOcclusionCounter().Accumulate();
}

} // namespace Libs::Graphics
