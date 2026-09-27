#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_RENDERDRAW_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_RENDERDRAW_H_

#include <cstdint>
#include <utility>

namespace Libs::Graphics {

struct ShaderVertexInputInfo;

[[nodiscard]] std::pair<int32_t, uint32_t>
ResolveDrawOffsets(uint32_t index_offset, const ShaderVertexInputInfo& vs_input_info);

// KYTY_RENDER_STATE_FAST vertex: copies a vertex input whose entries past its counts hold their
// default values (as every preparation leaves them) as its used prefixes, which gives the bytes
// of a full copy when the destination's entries past its counts are default too. False, copying
// nothing, when a count of either is out of range.
[[nodiscard]] bool CopyVertexInputPrefixes(ShaderVertexInputInfo&       dst,
                                           const ShaderVertexInputInfo& src);

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_RENDERDRAW_H_
