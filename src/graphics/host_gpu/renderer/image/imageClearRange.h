#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGE_IMAGECLEARRANGE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGE_IMAGECLEARRANGE_H_

#include <cstdint>

namespace Libs::Graphics {

// Validation of a TextureCache::ClearImage request against the target image, independent of Vulkan so
// that every condition is unit tested. A request that fails is skipped (and reported), never fatal:
// the guest decides which images and ranges reach a clear.

// Vulkan aspect bits (VK_IMAGE_ASPECT_*), repeated here to keep the header dependency free.
inline constexpr uint32_t ClearAspectColor   = 0x1;
inline constexpr uint32_t ClearAspectDepth   = 0x2;
inline constexpr uint32_t ClearAspectStencil = 0x4;

enum class ClearRangeFault : uint8_t {
	None,
	StencilCompanion, // the target is a stencil plane record: it has no native image, its owner does
	BaseLevel,        // baseMipLevel is not a level of the target
	NoAspect,         // empty aspect mask
	AspectMismatch,   // an aspect the target does not have (colour clear of depth, depth clear of colour)
	LevelCount,       // zero levels, or more than the target has above baseMipLevel
	NoLayers,         // zero layers
	BaseLayer,        // baseArrayLayer is not a layer (or volume slice) of the target
	LayerCount,       // more layers than the target has above baseArrayLayer
	ViewClearShape,   // a clear through an aliased view only supports one colour level
};

struct ClearTarget {
	uint32_t levels            = 0; // image.info.resources.levels
	uint32_t layers            = 0; // array layers, or the volume depth at the base level
	uint32_t aspects           = 0; // aspects the target image has
	bool     stencil_companion = false;
	bool     format_aliased    = false; // the requested clear format differs from the backing format
	bool     volume            = false;
};

struct ClearRange {
	uint32_t aspect_mask = 0;
	uint32_t base_level  = 0;
	uint32_t level_count = 0;
	uint32_t base_layer  = 0;
	uint32_t layer_count = 0;
};

// Layers a clear can address: the depth slices of the base level for a volume, else the native
// array layers.
[[nodiscard]] constexpr uint32_t ClearTargetLayers(bool volume, uint32_t extent_depth,
                                                   uint32_t base_level,
                                                   uint32_t backing_layers) noexcept {
	if (!volume) {
		return backing_layers;
	}
	const uint32_t slices = base_level >= 32 ? 0u : extent_depth >> base_level;
	return slices > 1u ? slices : 1u;
}

// The range covers every level, layer and aspect of the target.
[[nodiscard]] constexpr bool ClearCoversImage(const ClearTarget& target,
                                              const ClearRange&  range) noexcept {
	return range.aspect_mask == target.aspects && range.base_level == 0 &&
	       range.level_count == target.levels && range.base_layer == 0 &&
	       range.layer_count == target.layers;
}

[[nodiscard]] constexpr ClearRangeFault CheckClearRange(const ClearTarget& target,
                                                        const ClearRange&  range) noexcept {
	if (target.stencil_companion) {
		return ClearRangeFault::StencilCompanion;
	}
	if (range.base_level >= target.levels) {
		return ClearRangeFault::BaseLevel;
	}
	if (range.aspect_mask == 0) {
		return ClearRangeFault::NoAspect;
	}
	if ((range.aspect_mask & target.aspects) != range.aspect_mask) {
		return ClearRangeFault::AspectMismatch;
	}
	if (range.level_count == 0 || range.level_count > target.levels - range.base_level) {
		return ClearRangeFault::LevelCount;
	}
	if (range.layer_count == 0) {
		return ClearRangeFault::NoLayers;
	}
	if (range.base_layer >= target.layers) {
		return ClearRangeFault::BaseLayer;
	}
	if (range.layer_count > target.layers - range.base_layer) {
		return ClearRangeFault::LayerCount;
	}
	// Transfer clears use the backing format; an aliased format, or a part of a volume, is cleared
	// through a colour attachment view of one level.
	const bool view_clear = target.format_aliased || (target.volume && !ClearCoversImage(target, range));
	if (view_clear && (range.aspect_mask != ClearAspectColor || range.level_count != 1)) {
		return ClearRangeFault::ViewClearShape;
	}
	return ClearRangeFault::None;
}

[[nodiscard]] constexpr const char* ClearRangeFaultName(ClearRangeFault fault) noexcept {
	switch (fault) {
		case ClearRangeFault::None: return "none";
		case ClearRangeFault::StencilCompanion: return "stencil-companion-target";
		case ClearRangeFault::BaseLevel: return "base-level-out-of-range";
		case ClearRangeFault::NoAspect: return "empty-aspect-mask";
		case ClearRangeFault::AspectMismatch: return "aspect-mismatch";
		case ClearRangeFault::LevelCount: return "level-count";
		case ClearRangeFault::NoLayers: return "zero-layers";
		case ClearRangeFault::BaseLayer: return "base-layer-out-of-range";
		case ClearRangeFault::LayerCount: return "layer-count";
		case ClearRangeFault::ViewClearShape: return "view-clear-needs-one-colour-level";
	}
	return "unknown";
}

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGE_IMAGECLEARRANGE_H_
