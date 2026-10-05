#include "graphics/host_gpu/renderer/image/imageClearRange.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

using namespace Libs::Graphics;

void Check(bool condition, const char* description) {
	if (!condition) {
		std::fprintf(stderr, "ImageClearRangeTests: failed: %s\n", description);
		std::abort();
	}
}

constexpr uint32_t Color        = ClearAspectColor;
constexpr uint32_t DepthStencil = ClearAspectDepth | ClearAspectStencil;

ClearTarget ColourTarget(uint32_t levels = 4, uint32_t layers = 6) {
	return {.levels = levels, .layers = layers, .aspects = Color};
}

ClearTarget DepthTarget(uint32_t aspects = ClearAspectDepth, uint32_t levels = 1,
                        uint32_t layers = 1) {
	return {.levels = levels, .layers = layers, .aspects = aspects};
}

ClearRange Range(uint32_t aspect, uint32_t level, uint32_t level_count, uint32_t layer,
                 uint32_t layer_count) {
	return {aspect, level, level_count, layer, layer_count};
}

void TestValidRanges() {
	const auto colour = ColourTarget();
	Check(CheckClearRange(colour, Range(Color, 0, 4, 0, 6)) == ClearRangeFault::None, "whole colour image");
	Check(CheckClearRange(colour, Range(Color, 3, 1, 5, 1)) == ClearRangeFault::None, "last level and layer");
	Check(CheckClearRange(colour, Range(Color, 1, 3, 2, 4)) == ClearRangeFault::None, "interior range");
	const auto depth = DepthTarget(DepthStencil);
	Check(CheckClearRange(depth, Range(ClearAspectDepth, 0, 1, 0, 1)) == ClearRangeFault::None, "depth only");
	Check(CheckClearRange(depth, Range(ClearAspectStencil, 0, 1, 0, 1)) == ClearRangeFault::None, "stencil only");
	Check(CheckClearRange(depth, Range(DepthStencil, 0, 1, 0, 1)) == ClearRangeFault::None, "depth and stencil");
}

void TestStencilCompanion() {
	// The stencil plane record (Image::depth_id set) has no native image: refused for every range.
	auto target              = DepthTarget(ClearAspectStencil);
	target.stencil_companion = true;
	Check(CheckClearRange(target, Range(ClearAspectStencil, 0, 1, 0, 1)) ==
	          ClearRangeFault::StencilCompanion,
	      "stencil companion is refused before anything else");
	target.levels = 0;
	Check(CheckClearRange(target, Range(0, 9, 0, 9, 0)) == ClearRangeFault::StencilCompanion,
	      "companion fault wins over range faults");
}

void TestAspect() {
	Check(CheckClearRange(ColourTarget(), Range(0, 0, 1, 0, 1)) == ClearRangeFault::NoAspect,
	      "empty aspect mask");
	Check(CheckClearRange(ColourTarget(), Range(ClearAspectDepth, 0, 1, 0, 1)) ==
	          ClearRangeFault::AspectMismatch,
	      "depth clear of a colour image");
	Check(CheckClearRange(ColourTarget(), Range(ClearAspectStencil, 0, 1, 0, 1)) ==
	          ClearRangeFault::AspectMismatch,
	      "stencil clear of a colour image");
	Check(CheckClearRange(DepthTarget(), Range(Color, 0, 1, 0, 1)) == ClearRangeFault::AspectMismatch,
	      "colour clear of a depth image (DCC/CMASK over reused depth memory)");
	Check(CheckClearRange(DepthTarget(DepthStencil), Range(Color, 0, 1, 0, 1)) ==
	          ClearRangeFault::AspectMismatch,
	      "colour clear of a depth-stencil image");
	Check(CheckClearRange(DepthTarget(ClearAspectDepth), Range(DepthStencil, 0, 1, 0, 1)) ==
	          ClearRangeFault::AspectMismatch,
	      "stencil requested from a depth-only image");
	Check(CheckClearRange(DepthTarget(DepthStencil), Range(Color | ClearAspectDepth, 0, 1, 0, 1)) ==
	          ClearRangeFault::AspectMismatch,
	      "one wrong aspect among valid ones");
}

void TestLevels() {
	const auto target = ColourTarget(4, 1);
	Check(CheckClearRange(target, Range(Color, 4, 1, 0, 1)) == ClearRangeFault::BaseLevel,
	      "base level == levels");
	Check(CheckClearRange(target, Range(Color, 99, 1, 0, 1)) == ClearRangeFault::BaseLevel,
	      "base level far outside");
	Check(CheckClearRange(target, Range(Color, 0, 0, 0, 1)) == ClearRangeFault::LevelCount,
	      "zero levels");
	Check(CheckClearRange(target, Range(Color, 0, 5, 0, 1)) == ClearRangeFault::LevelCount,
	      "more levels than the image");
	Check(CheckClearRange(target, Range(Color, 3, 2, 0, 1)) == ClearRangeFault::LevelCount,
	      "levels past the last above the base level");
	Check(CheckClearRange(target, Range(Color, 2, 0xffffffffu, 0, 1)) == ClearRangeFault::LevelCount,
	      "huge level count does not overflow");
	// A view taken from another image's info: base level of a mip chain on a one-level image.
	Check(CheckClearRange(ColourTarget(1, 1), Range(Color, 2, 1, 0, 1)) == ClearRangeFault::BaseLevel,
	      "level of a different image's chain");
	Check(CheckClearRange(ColourTarget(0, 1), Range(Color, 0, 1, 0, 1)) == ClearRangeFault::BaseLevel,
	      "image without levels");
}

void TestLayers() {
	const auto target = ColourTarget(1, 6);
	Check(CheckClearRange(target, Range(Color, 0, 1, 0, 0)) == ClearRangeFault::NoLayers, "zero layers");
	Check(CheckClearRange(target, Range(Color, 0, 1, 6, 1)) == ClearRangeFault::BaseLayer,
	      "base layer == layers");
	Check(CheckClearRange(target, Range(Color, 0, 1, 0xffffffffu, 1)) == ClearRangeFault::BaseLayer,
	      "base layer far outside");
	Check(CheckClearRange(target, Range(Color, 0, 1, 0, 7)) == ClearRangeFault::LayerCount,
	      "more layers than the image");
	Check(CheckClearRange(target, Range(Color, 0, 1, 5, 2)) == ClearRangeFault::LayerCount,
	      "layers past the last above the base layer");
	Check(CheckClearRange(target, Range(Color, 0, 1, 1, 0xffffffffu)) == ClearRangeFault::LayerCount,
	      "huge layer count does not overflow");
	// DCC slice loop: image_first + slice beyond a cached image with fewer layers than the surface.
	Check(CheckClearRange(ColourTarget(1, 1), Range(Color, 0, 1, 1, 1)) == ClearRangeFault::BaseLayer,
	      "slice of a larger surface on a one-layer image");
}

void TestVolumeLayers() {
	Check(ClearTargetLayers(false, 64, 3, 5) == 5, "arrays use the backing layers");
	Check(ClearTargetLayers(true, 64, 0, 1) == 64, "volume base level has every slice");
	Check(ClearTargetLayers(true, 64, 3, 1) == 8, "volume slices halve per level");
	Check(ClearTargetLayers(true, 64, 6, 1) == 1, "last volume level keeps one slice");
	Check(ClearTargetLayers(true, 64, 9, 1) == 1, "levels past the depth keep one slice");
	Check(ClearTargetLayers(true, 64, 32, 1) == 1, "no undefined shift");
	Check(ClearTargetLayers(true, 64, 0xffffffffu, 1) == 1, "huge level, no undefined shift");
	Check(ClearTargetLayers(true, 0, 0, 1) == 1, "empty depth keeps one slice");

	auto volume   = ColourTarget(4, ClearTargetLayers(true, 16, 1, 1));
	volume.volume = true;
	Check(volume.layers == 8, "level 1 of a 16 slice volume");
	Check(CheckClearRange(volume, Range(Color, 1, 1, 0, 8)) == ClearRangeFault::None, "all slices");
	Check(CheckClearRange(volume, Range(Color, 1, 1, 7, 1)) == ClearRangeFault::None, "last slice");
	Check(CheckClearRange(volume, Range(Color, 1, 1, 8, 1)) == ClearRangeFault::BaseLayer,
	      "slice at the level's depth: the depth of level 0 is not valid at level 1");
	Check(CheckClearRange(volume, Range(Color, 1, 1, 4, 5)) == ClearRangeFault::LayerCount,
	      "slices past the level's depth");
}

void TestViewClearShape() {
	// A clear in another format, or of a part of a volume, goes through one colour attachment level.
	auto aliased           = ColourTarget(4, 1);
	aliased.format_aliased = true;
	Check(CheckClearRange(aliased, Range(Color, 0, 1, 0, 1)) == ClearRangeFault::None, "aliased one level");
	Check(CheckClearRange(aliased, Range(Color, 0, 2, 0, 1)) == ClearRangeFault::ViewClearShape,
	      "aliased clear of several levels");
	auto aliased_depth           = DepthTarget(DepthStencil);
	aliased_depth.format_aliased = true;
	Check(CheckClearRange(aliased_depth, Range(ClearAspectDepth, 0, 1, 0, 1)) ==
	          ClearRangeFault::ViewClearShape,
	      "aliased clear of a depth aspect");

	auto volume   = ColourTarget(1, 8);
	volume.volume = true;
	Check(CheckClearRange(volume, Range(Color, 0, 1, 0, 8)) == ClearRangeFault::None,
	      "whole volume uses the transfer clear");
	Check(CheckClearRange(volume, Range(Color, 0, 1, 2, 1)) == ClearRangeFault::None,
	      "one slice of a volume through a view");
	volume.levels = 3;
	Check(CheckClearRange(volume, Range(Color, 0, 2, 2, 1)) == ClearRangeFault::ViewClearShape,
	      "part of a volume over several levels");
	Check(ClearCoversImage(volume, Range(Color, 0, 3, 0, 8)), "covers the whole volume");
	Check(!ClearCoversImage(volume, Range(Color, 0, 3, 0, 7)), "a missing slice is not the whole image");
	Check(!ClearCoversImage(volume, Range(Color, 1, 2, 0, 8)), "a missing level is not the whole image");
}

void TestFaultNames() {
	for (uint32_t fault = 0; fault <= static_cast<uint32_t>(ClearRangeFault::ViewClearShape); ++fault) {
		const char* name = ClearRangeFaultName(static_cast<ClearRangeFault>(fault));
		Check(name != nullptr && name[0] != 0 && std::string_view(name) != "unknown",
		      "every fault has a name");
	}
}

} // namespace

int main() {
	TestValidRanges();
	TestStencilCompanion();
	TestAspect();
	TestLevels();
	TestLayers();
	TestVolumeLayers();
	TestViewClearShape();
	TestFaultNames();
	std::puts("ImageClearRangeTests: all passed");
	return 0;
}
