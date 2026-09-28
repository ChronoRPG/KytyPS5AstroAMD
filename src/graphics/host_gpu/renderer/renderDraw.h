#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_RENDERDRAW_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_RENDERDRAW_H_

#include "graphics/host_gpu/vulkanCommon.h"

#include <cstdint>
#include <utility>

namespace Libs::Graphics {

namespace HW {
class Context;
} // namespace HW

struct ShaderVertexInputInfo;

[[nodiscard]] std::pair<int32_t, uint32_t>
ResolveDrawOffsets(uint32_t index_offset, const ShaderVertexInputInfo& vs_input_info);

// KYTY_RENDER_STATE_FAST vertex: copies a vertex input whose entries past its counts hold their
// default values (as every preparation leaves them) as its used prefixes, which gives the bytes
// of a full copy when the destination's entries past its counts are default too. False, copying
// nothing, when a count of either is out of range.
[[nodiscard]] bool CopyVertexInputPrefixes(ShaderVertexInputInfo&       dst,
                                           const ShaderVertexInputInfo& src);

// The union of a draw's scissors in framebuffer pixels, not clamped to the framebuffer (every
// viewport slot when the vertex stage writes the viewport index); empty when it draws nothing.
[[nodiscard]] vk::Rect2D DrawScissorUnion(const HW::Context& ctx, bool indexed_viewports);
// DrawScissorUnion without calc_final_scissor's log (a draw-prep thread): false for a clip-rect
// rule it does not support, which the command processor then reports.
[[nodiscard]] bool DrawScissorUnionQuiet(const HW::Context& ctx, bool indexed_viewports,
                                         vk::Rect2D& result);

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_RENDERDRAW_H_
