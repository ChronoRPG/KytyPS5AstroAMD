#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_COLORRENDERTARGET_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_COLORRENDERTARGET_H_

#include "graphics/guest_gpu/gpu_defs.h"
#include "graphics/host_gpu/renderer/cache/textureCache.h"
#include "graphics/host_gpu/renderer/renderTarget.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <algorithm>
#include <cstdint>

namespace Libs::Graphics {

// KYTY_TARGET_DESC_MEMO=0 disables the color/depth target description memos (render.h).
[[nodiscard]] bool TargetDescMemoEnabled();
// KYTY_CMASK_FAST_CLEAR=0 ignores CMASK fast clears of colour targets (colorRenderTarget.cpp).
[[nodiscard]] bool CmaskFastClearEnabled();

// KYTY_DRAW_SEQUENCE_FAST: a draw reuses what the draws before it resolved when it consumes the
// same state and the reuse is proven exact (colorRenderTarget.cpp). Unset or 1: every part; 0:
// none; otherwise a comma-separated list of parts (targets, textures).
enum class DrawSequencePart : uint32_t {
	// Render/depth target lookups of a slot whose registers did not change
	// (TextureCache::RepeatLookup).
	Targets  = 1u << 0u,
	// A stage's texture bindings when its program and T#/S# words did not change.
	Textures = 1u << 1u,
};
[[nodiscard]] bool DrawSequenceEnabled(DrawSequencePart part);
// KYTY_DRAW_SEQUENCE_VERIFY=1|exit: every reuse is checked against the full path, which then
// provides the result; a difference is counted (DrawSequenceVerifyMismatches; exit stops on the
// first). 0: off.
[[nodiscard]] int  DrawSequenceVerifyMode();
void               ReportDrawSequenceMismatch(const char* what);

struct RenderColorInfo {
	// Discovery keeps guest image information but can remap the view into a larger cache image.
	TextureCache::ImageDesc         desc;
	ImageId                         image_id;
	uint32_t                        target_slot      = 0;
	uint32_t                        guest_mip_level   = 0;
	uint32_t                        guest_array_layer = 0;
	Prospero::ColorComponentMapping export_mapping;

	[[nodiscard]] vk::Extent2D Extent() const {
		return {std::max(desc.info.extent.width >> guest_mip_level, 1u),
		        std::max(desc.info.extent.height >> guest_mip_level, 1u)};
	}
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_COLORRENDERTARGET_H_
