#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GRAPHICSRENDER_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GRAPHICSRENDER_H_

#include "common/abi.h"
#include "common/assert.h"
#include "common/common.h"
#include "graphics/host_gpu/queueSubmission.h"
#include "graphics/host_gpu/renderer/pipeline/descriptors.h"
#include "graphics/host_gpu/renderer/pipeline/pipelineCache.h"
#include "graphics/host_gpu/renderer/renderTarget.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Libs::Graphics {

namespace HW {
class Context;
class UserConfig;
class Shader;
} // namespace HW

struct GraphicContext;
struct ShaderBufferResource;
struct ShaderComputeInputInfo;
struct RenderDepthInfo;
struct RenderColorInfo;
struct DrawCallInfo;
struct DrawEmitInfo;
struct DrawIndexBufferSource;
struct DrawRenderState;
class RenderContext;
class CommandScheduler;
struct RenderExecutorTestAccess;

enum class CommandBufferDebugOp : uint32_t {
	DispatchDirect,
	DrawIndex,
	DrawIndexAuto,
	EopWrite,
	EopInterrupt,
	EopWriteBack,
	EopFlip,
	EopWriteBackFlip,
	EopOnlyFlip,
	DispatchIndirect,
	Unknown,
};

enum class DrawOffsetSource : uint8_t {
	DrawState,
	IndirectArgs,
};

struct DrawIndexArgs {
	uint32_t         index_count                = 0;
	const void*      index_addr                 = nullptr;
	uint32_t         instance_count             = 0;
	uint32_t         index_type_and_size        = 0;
	int32_t          base_vertex                = 0;
	uint32_t         first_instance             = 0;
	DrawOffsetSource offset_source              = DrawOffsetSource::DrawState;
	uint32_t         render_target_slice_offset = 0;
};

// GPU-resident draw arguments for vkCmdDraw*Indirect*. Guest DrawIndexedIndirect (20 bytes) and
// DrawIndirect (16 bytes) records are byte-identical to VkDrawIndexedIndirectCommand and
// VkDrawIndirectCommand, so the guest memory is consumed without CPU interpretation.
struct DrawIndirectSource {
	uint64_t args_addr  = 0;
	uint32_t stride     = 0;
	uint32_t max_count  = 1;
	uint64_t count_addr = 0; // Nonzero: draw min(*count_addr, max_count) records.
	bool     indexed    = false;
	// Index state at the packet (INDEX_BASE, INDEX_BUFFER_SIZE in elements, INDEX_TYPE).
	// firstIndex and vertexOffset come from each record.
	uint64_t index_base_addr     = 0;
	uint32_t index_buffer_size   = 0;
	uint32_t index_type_and_size = 0;

	[[nodiscard]] uint32_t RecordSize() const { return indexed ? 20u : 16u; }
	// Bytes covering every record that max_count allows.
	[[nodiscard]] uint64_t ArgsSize() const {
		return static_cast<uint64_t>(max_count - 1u) * stride + RecordSize();
	}
};

struct DrawAutoArgs {
	uint32_t         vertex_count               = 0;
	uint32_t         instance_count             = 0;
	uint32_t         first_vertex               = 0;
	uint32_t         first_instance             = 0;
	DrawOffsetSource offset_source              = DrawOffsetSource::DrawState;
	uint32_t         render_target_slice_offset = 0;
};

// Barrier batcher (KYTY_BARRIER_BATCH, default on; KYTY_BARRIER_BATCH=0 restores the direct
// per-site barriers). Global memory dependencies, buffer barriers and image layout transitions
// requested through CommandBuffer are accumulated and recorded as ONE vkCmdPipelineBarrier2
// directly before the next command that may access memory:
//   - Handle(): every native recording site obtains the handle through it, so the pending
//     batch is recorded (ending an active rendering instance first) before anything the caller
//     records. StateHandle() is the exception for state-only commands (binds, dynamic state,
//     push constants/descriptors), which barriers do not order.
//   - BeginRendering(): the draw path records its draw right after it.
//   - End() / scheduler submission.
// Consecutive requests with nothing recorded between them are merged (the union of their
// scopes is at least as strong as the sequence). A memory request is elided when nothing was
// recorded since the previous flushed barrier and that barrier's memory dependency covers it.
// KYTY_BARRIER_SINK (default on, needs the batcher) additionally keeps a pending memory-only
// batch across a draw that continues the same rendering instance when the draw is proven
// hazard-free with respect to everything since the last full barrier; see
// CommandBuffer::CanSinkPending() for the exact conditions.
[[nodiscard]] bool BarrierBatchEnabled();
[[nodiscard]] bool BarrierSinkEnabled();

// Attribution of batched barrier requests (gpuOpProfiler site of the recorded batch).
enum class BarrierOrigin : uint32_t {
	Guest,             // CP EmitGlobalBarrier: guest RELEASE_MEM / EVENT_WRITE flushes
	ShaderAccess,      // after a dispatch: shader accesses -> everything later
	ShaderWrite,       // after a draw with shader buffer writes
	ShaderWriteHazard, // before a dispatch with shader writes: everything earlier -> it
	IndirectArgs,      // shader/transfer writes -> indirect command fetch
	Gds,               // GDS buffer -> shader stages
	Image,             // Image::Transit layout/access transitions
	Count,
};

class CommandBuffer {
public:
	~CommandBuffer() = default;

	KYTY_CLASS_NO_COPY(CommandBuffer);

	[[nodiscard]] bool IsInvalid() const;

	void SetDebugInfo(uint32_t op, uint64_t submit_id, uint32_t arg0 = 0, uint32_t arg1 = 0,
	                  uint32_t arg2 = 0, uint32_t arg3 = 0, uint64_t arg4 = 0);
	// Begins (or continues) the rendering instance for the draw recorded next. Pending batched
	// barriers are recorded outside rendering first, unless they can be sunk (see above).
	void BeginRendering(const RenderState& state) const;
	void EndRendering() const;

	// Queues a global memory dependency (synchronization2 masks).
	void RequestMemoryBarrier(vk::PipelineStageFlags2 src_stages, vk::AccessFlags2 src_access,
	                          vk::PipelineStageFlags2 dst_stages, vk::AccessFlags2 dst_access,
	                          BarrierOrigin origin) const;
	// Queues a buffer memory barrier.
	void RequestBufferBarrier(const vk::BufferMemoryBarrier2& barrier, BarrierOrigin origin) const;
	// Routes image barriers built for `target` into the batch. Returns false (nothing done) when
	// the batcher is disabled or `target` is not this buffer; the caller then records them
	// itself. deferrable: the caller guarantees that it records no memory-accessing command
	// through a previously obtained native handle before the next flush point; otherwise the
	// batch (with these barriers) is recorded immediately.
	[[nodiscard]] bool BatchImageBarriers(std::span<const vk::ImageMemoryBarrier2> barriers,
	                                      vk::CommandBuffer target, bool deferrable) const;
	// Records the pending batch now (no-op when empty).
	void FlushBarriers() const;

	// Brackets the draw recording path from the state commands through the draw itself. The
	// draw must be recorded right after BeginRendering(). safe: the draw writes only its own
	// color/depth attachments (no storage buffer/image writes, atomics, address writes, GDS,
	// fault/LOD counters, indirect arguments) and samples none of its attachments.
	class DrawScope {
	public:
		DrawScope(const CommandBuffer& buffer, bool safe) noexcept: m_buffer(buffer) {
			m_buffer.m_draw_scope = true;
			m_buffer.m_draw_safe  = safe;
		}
		~DrawScope() {
			m_buffer.m_draw_scope = false;
			m_buffer.m_draw_safe  = false;
		}
		DrawScope(const DrawScope&)            = delete;
		DrawScope& operator=(const DrawScope&) = delete;

	private:
		const CommandBuffer& m_buffer;
	};

	// Identifies the active rendering instance; 0 while no instance is active.
	[[nodiscard]] uint64_t ActiveRenderingSerial() const {
		return m_rendering ? m_rendering_serial : 0;
	}
	void BindPipeline(vk::PipelineBindPoint point, vk::Pipeline pipeline);
	void PushDescriptors(vk::PipelineBindPoint point, vk::PipelineLayout layout, uint32_t set,
	                     uint32_t count, const vk::WriteDescriptorSet* writes);
	void InvalidateDescriptors(vk::PipelineBindPoint point);

	// Native handle for recording any command. Records pending batched barriers first.
	[[nodiscard]] vk::CommandBuffer Handle() const;
	// Native handle for state-only commands (binds, dynamic state, push constants/descriptors).
	// Never records pending barriers; nothing that accesses memory may be recorded through it
	// before the next flush point (Handle(), BeginRendering(), End()).
	[[nodiscard]] vk::CommandBuffer StateHandle() const;
	[[nodiscard]] GraphicContext&   GetGraphics() const noexcept { return m_graphics; }
	[[nodiscard]] RenderContext&    GetContext() const noexcept { return m_context; }
	[[nodiscard]] HW::Context&      GetRegisters() const noexcept { return *m_registers; }
	[[nodiscard]] HW::UserConfig&   GetUserConfig() const noexcept { return *m_user_config; }
	[[nodiscard]] HW::Shader&       GetShaders() const noexcept { return *m_shaders; }

private:
	explicit CommandBuffer(CommandScheduler& scheduler);
	void Bind(HW::Context& registers, HW::UserConfig& user_config, HW::Shader& shaders) noexcept {
		m_registers   = &registers;
		m_user_config = &user_config;
		m_shaders     = &shaders;
	}

	void Begin();
	void End() const;

	struct PendingBarriers {
		vk::MemoryBarrier2                    memory;
		bool                                  has_memory = false;
		std::vector<vk::ImageMemoryBarrier2>  images;
		std::vector<vk::BufferMemoryBarrier2> buffers;
		uint32_t                              origins = 0; // bit per BarrierOrigin

		[[nodiscard]] bool Empty() const {
			return !has_memory && images.empty() && buffers.empty();
		}
		void Clear() {
			has_memory = false;
			memory     = vk::MemoryBarrier2 {};
			images.clear();
			buffers.clear();
			origins = 0;
		}
	};
	// Every native command other than the batch itself and the rendering bookkeeping below.
	void NoteForeignCommand() const {
		m_recorded_since_flush = true;
		m_epoch_clean          = false;
	}
	[[nodiscard]] bool CanSinkPending() const;
	void               NoteDrawRecorded() const;
	void               ResetBarrierState() const;

	RenderContext&      m_context;
	GraphicContext&     m_graphics;
	vk::CommandBuffer   m_buffer          = nullptr;
	uint32_t            m_debug_op        = 0;
	uint64_t            m_debug_submit_id = 0;
	uint32_t            m_debug_arg0      = 0;
	uint32_t            m_debug_arg1      = 0;
	uint32_t            m_debug_arg2      = 0;
	uint32_t            m_debug_arg3      = 0;
	uint64_t            m_debug_arg4      = 0;
	mutable RenderState m_render_state;
	mutable bool        m_rendering   = false;
	mutable uint64_t    m_rendering_serial  = 0;
	mutable uint32_t    m_occlusion_control = 0;
	HW::Context*        m_registers   = nullptr;
	HW::UserConfig*     m_user_config = nullptr;
	HW::Shader*         m_shaders     = nullptr;
	std::array<vk::Pipeline, 2> m_bound_pipelines {};
	struct DescriptorState {
		vk::PipelineLayout layout = nullptr;
		std::vector<vk::WriteDescriptorSet> writes;
		std::vector<vk::DescriptorBufferInfo> buffers;
		std::vector<vk::DescriptorImageInfo> images;
	};
	std::array<DescriptorState, 2> m_descriptor_states;

	// Barrier batcher state (see BarrierBatchEnabled()). Owned by the recording producer.
	mutable PendingBarriers m_pending;
	// Memory dependency of the last recorded batch in this command buffer, for elision.
	mutable vk::MemoryBarrier2 m_last_memory;
	mutable bool               m_last_memory_valid = false;
	// A command was recorded (or may have been) since the last recorded batch.
	mutable bool m_recorded_since_flush = true;
	// Sinking epoch: a full ALL_COMMANDS/MEMORY_WRITE -> ALL_COMMANDS/MEMORY_READ|WRITE batch
	// was recorded in this command buffer, and everything recorded since is state commands, the
	// begin of at most one rendering instance (m_epoch_instance) and safe draws inside it.
	mutable bool     m_epoch_clean    = false;
	mutable uint64_t m_epoch_instance = 0;
	// Nonzero while the batcher or the rendering bookkeeping records through Handle().
	mutable uint32_t m_internal_recording = 0;
	mutable bool     m_draw_scope         = false;
	mutable bool     m_draw_safe          = false;

	friend class CommandScheduler;
};

class RenderExecutor {
public:
	explicit RenderExecutor(RenderContext& context);
	~RenderExecutor();
	KYTY_CLASS_NO_COPY(RenderExecutor);

	void DispatchDirect(uint64_t submit_id, CommandBuffer& buffer, uint32_t thread_group_x,
	                    uint32_t thread_group_y, uint32_t thread_group_z, uint32_t mode);
	void DispatchIndirect(uint64_t submit_id, CommandBuffer& buffer, uint64_t args_addr,
	                      uint32_t mode);

	void PrepareBindings(const ShaderStageRuntime& runtime, PreparedBindings& prepared);
	void                           FindBuffers(PreparedBindings& bindings);
	void                           RebindBuffers(PreparedBindings& bindings);
	void                           RebindImages(PreparedBindings& bindings);
	void CommitBindings(CommandBuffer& buffer, vk::PipelineBindPoint pipeline_bind_point,
	                    const PipelineCache::Pipeline&     pipeline,
	                    std::span<PreparedBindings* const> bindings);

private:
	void DrawIndex(uint64_t submit_id, CommandBuffer& buffer, const DrawIndexArgs& args);
	void DrawAuto(uint64_t submit_id, CommandBuffer& buffer, const DrawAutoArgs& args);
	// Records a GPU-sourced indirect draw. Returns false, before recording anything, when the
	// draw needs CPU-visible counts; the caller then reads the arguments and draws directly.
	[[nodiscard]] bool DrawIndirectNative(uint64_t submit_id, CommandBuffer& buffer,
	                                      const DrawIndirectSource& source);

	struct GraphicsBindings {
		std::array<PreparedBindings, 3> vertex;
		std::optional<PreparedBindings> pixel;
	};

	[[nodiscard]] TextureBinding ResolveTexture(const ShaderRecompiler::IR::ImageResource& resource,
	                                            const ShaderRecompiler::IR::DescriptorValue& value);
	void PrepareGraphicsBindings(std::span<PreparedBindings* const> stages,
	                             std::span<RenderColorInfo> colors);
	void ResolveRenderColorTarget(CommandBuffer& buffer, RenderColorInfo& target,
	                              uint32_t render_target_slice_offset, uint32_t render_target_slot,
	                              bool ignore_target_mask = false, bool exact_format = false);
	void ResolveRenderDepthTarget(CommandBuffer& buffer, RenderDepthInfo& target);
	[[nodiscard]] bool DepthStencilCopy(CommandBuffer& buffer);
	[[nodiscard]] bool PrepareDrawRenderState(CommandBuffer& buffer,
	                                          const DrawCallInfo& draw,
	                                          uint32_t            render_target_slice_offset,
	                                          DrawRenderState& state);
	void ExecutePreparedDraw(uint64_t submit_id, CommandBuffer& buffer, const DrawCallInfo& draw,
	                         DrawRenderState& state, vk::PrimitiveTopology topology,
	                         const DrawEmitInfo& emit, const DrawIndexBufferSource& index_source,
	                         bool primitive_restart_enable);
	[[nodiscard]] RenderState AcquireRenderTargets(CommandBuffer& buffer, RenderColorInfo* colors,
	                                               uint32_t color_count, RenderDepthInfo& depth,
	                                               vk::ImageAspectFlags& feedback_aspects,
	                                               std::span<PreparedBindings* const> stages = {});
	[[nodiscard]] bool        ResolveColorTargets(CommandBuffer& buffer,
	                                              uint32_t render_target_slice_offset);
	void                      BindImage(ImageId id, bool storage);
	void                      BindRenderTarget(ImageId id);
	void                      ResetBindings();
	[[nodiscard]] vk::DescriptorBufferInfo UploadShaderData(std::span<const uint32_t> data);
	[[nodiscard]] bool        TryConsumeComputeMetaClear(const ShaderComputeInputInfo& input,
	                                                     const CommandBuffer&          buffer);
	[[nodiscard]] bool TryConsumeComputeImageClear(const ShaderComputeInputInfo& input,
	                                              CommandBuffer& command, uint32_t group_x,
	                                              uint32_t group_y, uint32_t group_z, uint32_t mode);

	RenderContext&                        m_context;
	GraphicsBindings                     m_graphics_bindings;
	PreparedBindings                     m_compute_bindings;
	// Reused by every draw (all draws hold the render mutex); DrawRenderState::Reset restores
	// only what the previous draw changed. Owns the draw's per-stage program preparation.
	std::unique_ptr<DrawRenderState>      m_draw_state;
	// Program preparation output of the current dispatch; its stage runtime points here.
	PipelineCache::StagePrep              m_compute_prep;
	std::vector<ImageId>                  m_bound_images;
	std::vector<vk::DescriptorBufferInfo> m_descriptor_buffers;
	std::vector<vk::DescriptorImageInfo>  m_descriptor_images;
	std::vector<vk::WriteDescriptorSet>   m_descriptor_writes;
	std::vector<uint32_t>                 m_image_occurrences;
	struct ShaderUploadEntry {
		uint64_t tick = 0;
		uint64_t hash = 0;
		vk::DescriptorBufferInfo allocation;
		std::vector<uint32_t> words;
	};
	std::array<ShaderUploadEntry, 64> m_shader_uploads;
	// Rendering instance begun right after the last indirect-argument barrier. Buffer writes
	// are recorded outside rendering, or end it (shader-write barrier), so while this instance
	// stays active the barrier still covers every argument write.
	uint64_t m_indirect_barrier_rendering = 0;
	// The ImageResource fields BuildTextureDescription reads. The shader-specific identity
	// (source slot, first use pc, indirect-image tables) is excluded, so one texture bound from
	// different shaders shares an entry.
	struct TextureDescriptionKey {
		ShaderRecompiler::IR::ImageResourceClass resource_class {};
		Prospero::TextureNumericClass            numeric_class {};
		ShaderRecompiler::Decoder::ImageDimension dimension {};
		ShaderRecompiler::IR::ImageMipMode       mip_mode {};
		uint32_t                                 mip_count         = 0;
		Prospero::BufferFormat                   conversion_format {};
		uint32_t                                 shader_swizzle    = 0;
		bool                                     read              = false;
		bool                                     written           = false;
		bool                                     atomic            = false;
		bool                                     depth_compare     = false;
		bool                                     cube              = false;
		bool                                     r128              = false;

		bool operator==(const TextureDescriptionKey&) const = default;
	};
	struct TextureDescriptionEntry {
		bool valid = false;
		TextureDescriptionKey key;
		std::array<uint32_t, 8> words {};
		TextureCache::ImageDesc desc;
	};
	std::array<TextureDescriptionEntry, 4096> m_texture_descriptions;

	friend class CommandProcessor;
	friend struct RenderExecutorTestAccess;
};

[[nodiscard]] bool ResolveComputeBufferFill(const ShaderComputeInputInfo& input, uint32_t group_x,
                                            uint32_t group_y, uint32_t group_z, uint32_t mode,
                                            ShaderBufferResource& descriptor,
                                            uint32_t& packed_clear, uint64_t& size);

} // namespace Libs::Graphics

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GRAPHICSRENDER_H_ */
