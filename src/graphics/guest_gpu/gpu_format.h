#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GUEST_GPU_GPU_FORMAT_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GUEST_GPU_GPU_FORMAT_H_

#include "common/common.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <span>

namespace Libs::Graphics::Prospero {

struct FormatInfo {
	uint32_t   format                           = 0;
	vk::Format vk_format                        = vk::Format::eUndefined;
	uint32_t   bytes_per_element                = 0;
	uint32_t   block_compressed_bytes_per_block = 0;
	uint32_t   render_target_bytes_per_element  = 0;
	bool       supported_sampled_texture_format = false;
	bool       unsigned_integer_texture_format  = false;
};

std::span<const FormatInfo> Formats();
const FormatInfo*           FindFormatInfo(uint32_t format);
vk::Format                  SurfaceFormat(uint32_t format);
uint32_t                    NumBytesPerElement(uint32_t format);
uint32_t                    BlockCompressedBytesPerBlock(uint32_t format);
uint32_t                    RenderTargetBytesPerElement(uint32_t format);
bool                        IsSupportedTextureFormat(uint32_t format);
bool                        IsUintTextureFormat(uint32_t format);
bool                        IsFmaskTextureFormat(uint32_t format);

} // namespace Libs::Graphics::Prospero

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GUEST_GPU_GPU_FORMAT_H_ */
