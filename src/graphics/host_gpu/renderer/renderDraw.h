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

// The union of a draw's scissors in framebuffer pixels, not clamped to the framebuffer (every
// viewport slot when the vertex stage writes the viewport index); empty when it draws nothing.
[[nodiscard]] vk::Rect2D DrawScissorUnion(const HW::Context& ctx, bool indexed_viewports);

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_RENDERDRAW_H_
