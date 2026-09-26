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

class CommandBuffer {
public:
	~CommandBuffer() = default;

	KYTY_CLASS_NO_COPY(CommandBuffer);

	[[nodiscard]] bool IsInvalid() const;

	void SetDebugInfo(uint32_t op, uint64_t submit_id, uint32_t arg0 = 0, uint32_t arg1 = 0,
	                  uint32_t arg2 = 0, uint32_t arg3 = 0, uint64_t arg4 = 0);
	void BeginRendering(const RenderState& state) const;
	void EndRendering() const;
	// Identifies the active rendering instance; 0 while no instance is active.
	[[nodiscard]] uint64_t ActiveRenderingSerial() const {
		return m_rendering ? m_rendering_serial : 0;
	}
	void BindPipeline(vk::PipelineBindPoint point, vk::Pipeline pipeline);
	void PushDescriptors(vk::PipelineBindPoint point, vk::PipelineLayout layout, uint32_t set,
	                     uint32_t count, const vk::WriteDescriptorSet* writes);
	void InvalidateDescriptors(vk::PipelineBindPoint point);

	[[nodiscard]] vk::CommandBuffer Handle() const;
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

	friend class CommandScheduler;
};

class RenderExecutor {
public:
	explicit RenderExecutor(RenderContext& context): m_context(context) {}
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
