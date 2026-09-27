#include "graphics/host_gpu/renderer/pipeline/descriptors.h"

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common.h"
#include "common/file.h"
#include "common/logging/log.h"
#include "common/hangTrace.h"
#include "common/profiler.h"
#include "common/rendererBatch.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "graphics/guest_gpu/gpu_defs.h"
#include "graphics/guest_gpu/gpu_format.h"
#include "graphics/guest_gpu/graphicsRun.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/guest_gpu/tile.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/hostMemory.h"
#include "graphics/host_gpu/renderer/colorRenderTarget.h"
#include "graphics/host_gpu/renderer/debug.h"
#include "graphics/host_gpu/renderer/depthRenderTarget.h"
#include "graphics/host_gpu/renderer/image/imageView.h"
#include "graphics/host_gpu/renderer/image/textureCommon.h"
#include "graphics/host_gpu/renderer/lodStats.h"
#include "graphics/host_gpu/renderer/pipeline/shaderResourceBarrier.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/vulkanCommon.h"
#include "graphics/shader/recompiler/ir/ShaderIR.h"
#include "graphics/shader/recompiler/ir/passes/BindingLayout.h"
#include "graphics/shader/recompiler/ir/passes/ResourceMaterialization.h"
#include "graphics/shader/recompiler/ir/passes/WriteRangeAnalysis.h"
#include "graphics/shader/shader.h"
#include "kernel/memory.h"
#include "graphics/host_gpu/renderer/gpuOpProfiler.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdlib>
#include <fmt/format.h>
#include <limits>
#include <span>
#include <string>
#include <vector>
#include <xxhash.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Libs::Graphics {

namespace {

using BindingKind = ShaderRecompiler::IR::DescriptorBindingKind;

} // namespace

vk::DescriptorType NativeDescriptorType(BindingKind kind) {
	const auto image_class = ShaderRecompiler::IR::ImageBindingResourceClass(kind);
	if (image_class == ShaderRecompiler::IR::ImageResourceClass::Sampled) {
		return vk::DescriptorType::eSampledImage;
	}
	if (image_class == ShaderRecompiler::IR::ImageResourceClass::Storage) {
		return vk::DescriptorType::eStorageImage;
	}
	switch (kind) {
		case BindingKind::Samplers: return vk::DescriptorType::eSampler;
		case BindingKind::Buffers:
		case BindingKind::Gds:
		case BindingKind::BdaPagetable:
		case BindingKind::FaultBuffer:
		case BindingKind::FlattenedSrt:
		case BindingKind::ShaderData:
		case BindingKind::MipStats: return vk::DescriptorType::eStorageBuffer;
		case BindingKind::Count: EXIT("invalid native descriptor binding kind");
	}
	EXIT("invalid native descriptor binding kind");
}

uint32_t NativeDescriptorCount(const ShaderRecompiler::IR::DescriptorBinding& binding) {
	return binding.resources.empty() ? 1u : static_cast<uint32_t>(binding.resources.size());
}

vk::DescriptorImageInfo MakeImageInfo(const TextureBinding& texture, uint32_t element) {
	vk::ImageView view = nullptr;
	if (texture.mip_views.empty()) {
		if (element == 0u) {
			view = texture.image_view;
		}
	} else if (element < texture.mip_views.size()) {
		view = texture.mip_views[element];
	}
	EXIT_IF(!texture.image_id || view == nullptr || texture.layout == vk::ImageLayout::eUndefined);
	return {nullptr, view, texture.layout};
}

static const char* ShaderStageResourceName(ShaderType stage) {
	switch (stage) {
		case ShaderType::Vertex: return "Vertex";
		case ShaderType::Mesh: return "Mesh";
		case ShaderType::Local: return "Local";
		case ShaderType::TessellationControl: return "Hull";
		case ShaderType::TessellationEvaluation: return "Domain";
		case ShaderType::Pixel: return "Pixel";
		case ShaderType::Compute: return "Compute";
		default: return "Unknown";
	}
}

static Prospero::ImageType TextureType(const ShaderTextureResource& descriptor) {
	const auto type = descriptor.Type();
	return type == Prospero::ImageType::kCube ? Prospero::ImageType::kColor2DArray : type;
}

static Prospero::ImageType TextureBaseType(Prospero::ImageType type) {
	switch (type) {
		case Prospero::ImageType::kColor1DArray: return Prospero::ImageType::kColor1D;
		case Prospero::ImageType::kColor2DArray:
		case Prospero::ImageType::kColor2DMsaa:
		case Prospero::ImageType::kColor2DMsaaArray: return Prospero::ImageType::kColor2D;
		default: return type;
	}
}

static bool IsMultisampledTexture(Prospero::ImageType type) {
	return type == Prospero::ImageType::kColor2DMsaa ||
	       type == Prospero::ImageType::kColor2DMsaaArray;
}

// KYTY_PRECISE_WRITE_RANGES=0 treats every writable storage binding as written in full (the
// behaviour before write-range proofs).
static bool PreciseWriteRangesEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_PRECISE_WRITE_RANGES");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

// KYTY_WRITE_RANGE_LOG=N logs the first N writable storage bindings: shader, guest range, the
// proven written spans (or why the binding stays whole) and the evaluated address intervals.
static uint32_t WriteRangeLogLimit() {
	static const uint32_t limit = [] {
		const auto* value = std::getenv("KYTY_WRITE_RANGE_LOG");
		return value == nullptr ? 0u : static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
	}();
	return limit;
}

// KYTY_WRITE_RANGE_STATS=0 skips the image lookup behind FrameEvent.WriteRangeImagesSpared.
static bool WriteRangeImageStatsEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_WRITE_RANGE_STATS");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

static vk::DescriptorBufferInfo
NativeStorageBuffer(RenderContext& context, const PreparedBindings::BufferSource& source,
                    const ShaderRecompiler::IR::BufferResource& resource, ShaderType stage,
                    uint32_t slot, uint32_t& buffer_offset,
                    const std::vector<GuestRange>* written_ranges) {
	buffer_offset = 0;

	const auto& [address, size, id] = source;
	if (address == 0 || size == 0) {
		return {context.GetBufferCache().GetBuffer(NULL_BUFFER_ID).Handle(), 0, 16};
	}
	const auto& graphics  = context.GetGraphics();
	const auto  alignment = graphics.StorageMinAlignment();
	if (size > graphics.GetPhysicalDeviceProperties().limits.maxStorageBufferRange) {
		EXIT("storage buffer range is unsupported\n");
	}
	// A proven write range narrows what becomes GPU-owned and which images go stale; everything
	// else about the binding (synchronization, descriptor range) is unchanged.
	const bool narrowed = resource.written && written_ranges != nullptr;
	auto [buffer, offset] =
	    narrowed ? context.GetBufferCache().ObtainWrittenBuffer(address, size, *written_ranges, id)
	             : context.GetBufferCache().ObtainBuffer(address, size, resource.written,
	                                                     resource.formatted, id);
	const auto aligned_offset = Common::AlignDown(offset, alignment);
	const auto adjustment     = offset - aligned_offset;
	const auto max_range      = graphics.GetPhysicalDeviceProperties().limits.maxStorageBufferRange;
	if (adjustment % sizeof(uint32_t) != 0 || adjustment >= 256 || size > max_range - adjustment) {
		EXIT("storage buffer offset adjustment is unsupported\n");
	}
	buffer_offset = static_cast<uint32_t>(adjustment);
	// Shaders bounds-check storage buffers in whole dwords (OpArrayLength floors the range), and
	// may leave plain dword loads to robustBufferAccess2 (HostBufferRobustness). NVIDIA then
	// returns data for a dword that is only partly inside the range, so bind whole dwords: a
	// no-op for every shader-side check, and it makes the device check match them.
	const auto range = size + adjustment >= 4u ? Common::AlignDown(size + adjustment, uint64_t {4})
	                                           : size + adjustment;
	const vk::DescriptorBufferInfo result {buffer->Handle(), aligned_offset, range};
	if (narrowed) {
		for (const auto& range: *written_ranges) {
			context.GetTextureCache().InvalidateMemoryFromGPU(range.address, range.size);
		}
	} else if (resource.written) {
		context.GetTextureCache().InvalidateMemoryFromGPU(address, size);
	}
	const char* access = "Read";
	if (resource.written && resource.read) {
		access = "ReadWrite";
	} else if (resource.written) {
		access = "Write";
	}
	SetVulkanObjectNameF(
	    graphics.device, result.buffer,
	    "Kyty.{}.StorageBuffer[slot={} guest=0x{:016x} size=0x{:x} access={} formatted={}]",
	    ShaderStageResourceName(stage), slot, address, size, access, resource.formatted);
	return result;
}

bool IsSupportedDepthTextureEncoding(const ShaderTextureResource& descriptor, bool r128) {
	constexpr uint32_t field1_reserved_mask = 0x200fff00u;
	constexpr uint32_t field2_reserved_mask = 0xf0003000u;
	const uint32_t     field3_expected = descriptor.DstSelXYZW() |
	                                     (static_cast<uint32_t>(descriptor.BaseLevel()) << 12u) |
	                                     (static_cast<uint32_t>(descriptor.LastLevel()) << 16u) |
	                                     (static_cast<uint32_t>(descriptor.TileMode()) << 20u) |
	                                     (static_cast<uint32_t>(descriptor.Type()) << 28u);
	const uint32_t     field4_expected = descriptor.Depth() | (descriptor.BaseArray5() << 16u);
	const uint32_t     field5_expected = (static_cast<uint32_t>(descriptor.PerfMod5()) << 20u) |
	                                     (static_cast<uint32_t>(descriptor.MaxMip()) << 4u);
	const bool         common          = (descriptor.fields[1] & field1_reserved_mask) == 0 &&
	                                     (descriptor.fields[2] & field2_reserved_mask) == 0 &&
	                                     descriptor.fields[3] == field3_expected;
	if (r128) {
		return common && descriptor.fields[4] == 0 && descriptor.fields[5] == 0 &&
		       descriptor.fields[6] == 0 && descriptor.fields[7] == 0;
	}
	const bool full = common && descriptor.fields[4] == field4_expected &&
	                  descriptor.fields[5] == field5_expected;
	if (!full ||
	    (descriptor.MsaaDepth() && !IsMultisampledTexture(descriptor.Type()))) {
		return false;
	}
	const auto metadata_control = descriptor.fields[6] & 0x00ffffffu;
	if (metadata_control == 0) {
		return true;
	}
	constexpr uint32_t htile_control = 0x00280000u;
	const uint32_t expected_control  = htile_control | (descriptor.MsaaDepth() ? (1u << 10u) : 0u);
	const auto     metadata_addr     = descriptor.MetaAddr() << 8u;
	return metadata_control == expected_control && metadata_addr != 0 &&
	       metadata_addr < TRACKER_ADDRESS_SIZE && (metadata_addr & 0x7fffu) == 0 &&
	       descriptor.TileMode() == Prospero::TileMode::kDepth;
}

static void ValidateSampledDepthBinding(const ShaderRecompiler::IR::ImageResource& resource,
                                        const ShaderTextureResource& descriptor, const Image& image,
                                        vk::Format view_format, uint64_t size) {
	const bool resource_ok = IsSupportedSampledDepthResource(resource);
	const bool encoding_ok = IsSupportedDepthTextureEncoding(descriptor, resource.r128);
	const bool view_ok =
	    IsSupportedSampledDepthView(image.info.pixel_format, view_format, descriptor.DstSelXYZW());
	if (resource_ok && encoding_ok && view_ok) {
		return;
	}
	const auto descriptor_pitch =
	    TileGetTexturePitch(descriptor.Format(), static_cast<uint32_t>(descriptor.Width5()) + 1u,
	                        descriptor.TileMode());
	EXIT("unsupported sampled depth image: resource=%d encoding=%d view=%d "
	     "class=%u numeric=%u dimension=%u mip_mode=%u read=%d written=%d atomic=%d compare=%d "
	     "guest_format=%u swizzle=0x%03x image_format=%d view_format=%d image_layers=%u "
	     "descriptor_type=%u base_array=%u depth=%u descriptor_pitch=%u target_pitch=%u "
	     "addr=0x%016" PRIx64 " size=0x%016" PRIx64
	     " dwords=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
	     resource_ok, encoding_ok, view_ok,
	     static_cast<uint32_t>(resource.resource_class),
	     static_cast<uint32_t>(resource.numeric_class), static_cast<uint32_t>(resource.dimension),
	     static_cast<uint32_t>(resource.mip_mode), resource.read, resource.written, resource.atomic,
	     resource.depth_compare, static_cast<uint32_t>(descriptor.Format()),
	     descriptor.DstSelXYZW(), static_cast<int>(image.info.pixel_format),
	     static_cast<int>(view_format), image.info.resources.layers,
	     static_cast<uint32_t>(descriptor.Type()), descriptor.BaseArray5(), descriptor.Depth(),
	     descriptor_pitch, image.info.pitch, descriptor.Base40(), size, descriptor.fields[0],
	     descriptor.fields[1], descriptor.fields[2], descriptor.fields[3], descriptor.fields[4],
	     descriptor.fields[5], descriptor.fields[6], descriptor.fields[7]);
}

static bool IsSupportedStorageTextureDescriptor(const ShaderRecompiler::IR::ImageResource& resource,
                                                const ShaderTextureResource& descriptor) {
	const auto tile              = descriptor.TileMode();
	const bool is_color_1d       = descriptor.Type() == Prospero::ImageType::kColor1D;
	const bool is_color_1d_array = descriptor.Type() == Prospero::ImageType::kColor1DArray;
	const bool valid_1d_slice =
	    (is_color_1d && descriptor.Depth() == 0 && descriptor.BaseArray5() == 0) ||
	    (is_color_1d_array && descriptor.BaseArray5() <= descriptor.Depth());
	const bool is_1d = resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim1D &&
	                   descriptor.Height5() == 0 && valid_1d_slice;
	const bool is_1d_array =
	    resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim1DArray &&
	    is_color_1d_array && descriptor.Height5() == 0 &&
	    descriptor.BaseArray5() <= descriptor.Depth();
	const bool is_color_2d       = descriptor.Type() == Prospero::ImageType::kColor2D;
	const bool is_color_2d_array = descriptor.Type() == Prospero::ImageType::kColor2DArray;
	const bool valid_2d_slice =
	    (is_color_2d && descriptor.Depth() == 0 && descriptor.BaseArray5() == 0) ||
	    (is_color_2d_array && descriptor.BaseArray5() <= descriptor.Depth());
	const bool is_2d =
	    resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim2D && valid_2d_slice;
	// Storage cube coordinates address individual faces, including partial cube views.
	const bool is_cube = resource.cube && descriptor.Type() == Prospero::ImageType::kCube &&
	                     descriptor.Width5() == descriptor.Height5() &&
	                     descriptor.BaseArray5() <= descriptor.Depth();
	const bool is_2d_array =
	    resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim2DArray &&
	    ((!resource.cube && is_color_2d_array && descriptor.BaseArray5() <= descriptor.Depth()) ||
	     is_cube);
	const bool is_3d = resource.dimension == ShaderRecompiler::Decoder::ImageDimension::Dim3D &&
	                   descriptor.Type() == Prospero::ImageType::kColor3D &&
	                   descriptor.BaseArray5() == 0;
	TileTextureBlockLayout tile_layout {};
	bool                   supported_tile = false;
	switch (tile) {
		case Prospero::TileMode::kLinear: supported_tile = true; break;
		case Prospero::TileMode::kDepth:
			supported_tile =
			    !resource.read && !Prospero::IsFmaskTextureFormat(descriptor.Format()) &&
			    (is_2d || is_2d_array) &&
			    TileGetTextureBlockLayout(descriptor.Format(), tile, false, tile_layout);
			break;
		case Prospero::TileMode::kStandard256B:
			supported_tile =
			    (is_2d || is_2d_array) &&
			    TileGetTextureBlockLayout(descriptor.Format(), tile, false, tile_layout);
			break;
		case Prospero::TileMode::kStandard4KB:
		case Prospero::TileMode::kStandard64KB:
			supported_tile =
			    TileGetTextureBlockLayout(descriptor.Format(), tile, is_3d, tile_layout);
			break;
		case Prospero::TileMode::kRenderTarget:
			supported_tile =
			    TileGetTextureBlockLayout(descriptor.Format(), tile, false, tile_layout);
			break;
		default: break;
	}
	const auto swizzle = descriptor.DstSelXYZW();
	const bool supported_swizzle =
	    IsValidImageSwizzle(swizzle) &&
	    (swizzle == DstSel(4, 5, 6, 7) || !resource.read || resource.atomic);
	return (is_1d || is_1d_array || is_2d || is_2d_array || is_3d) && supported_tile &&
	       descriptor.BaseLevel() <= descriptor.LastLevel() &&
	       descriptor.MinLod() == 0 && supported_swizzle && descriptor.BCSwizzle() == 0 &&
	       !descriptor.MsaaDepth();
}

static bool IsSupportedStorageTextureEncoding(const ShaderRecompiler::IR::ImageResource& resource,
                                              const ShaderTextureResource& descriptor) {
	constexpr uint32_t field1_reserved_mask = 0x200fff00u;
	constexpr uint32_t field2_reserved_mask = 0xf0003000u;
	constexpr uint32_t field5_expected      = 0x00700000u;
	constexpr uint32_t field5_max_mip_mask  = 0x000000f0u;
	const uint32_t     expected_field3 = descriptor.DstSelXYZW() |
	                                     (static_cast<uint32_t>(descriptor.BaseLevel()) << 12u) |
	                                     (static_cast<uint32_t>(descriptor.LastLevel()) << 16u) |
	                                     (static_cast<uint32_t>(descriptor.TileMode()) << 20u) |
	                                     (static_cast<uint32_t>(descriptor.Type()) << 28u);
	const uint32_t     expected_field4 =
	    descriptor.Depth() | (static_cast<uint32_t>(descriptor.BaseArray5()) << 16u);
	const bool common = (descriptor.fields[1] & field1_reserved_mask) == 0 &&
	                    (descriptor.fields[2] & field2_reserved_mask) == 0 &&
	                    descriptor.fields[3] == expected_field3;
	if (resource.r128) {
		return common && descriptor.fields[4] == 0 && descriptor.fields[5] == 0 &&
		       descriptor.fields[6] == 0 && descriptor.fields[7] == 0;
	}
	return common && descriptor.fields[4] == expected_field4 &&
	       (descriptor.fields[5] & ~field5_max_mip_mask) == field5_expected;
}

void ValidateStorageTexture(const ShaderRecompiler::IR::ImageResource& resource,
                            const ShaderTextureResource& descriptor, uint64_t size) {
	const auto format        = descriptor.Format();
	const bool resource_ok   = IsSupportedStorageImageResource(resource);
	const bool descriptor_ok = IsSupportedStorageTextureDescriptor(resource, descriptor);
	const bool encoding_ok   = IsSupportedStorageTextureEncoding(resource, descriptor);
	const bool uint_resource    = resource.numeric_class == Prospero::TextureNumericClass::Uint;
	const bool raw_sint_storage = format == Prospero::BufferFormat::k32SInt && uint_resource &&
	                              resource.written && !resource.read && !resource.atomic;
	const auto numeric_class = Prospero::SampledTextureNumericClass(format);
	const bool format_ok =
	    raw_sint_storage ||
	    (numeric_class != Prospero::TextureNumericClass::Unsupported &&
	     numeric_class != Prospero::TextureNumericClass::Sint &&
	     uint_resource == (numeric_class == Prospero::TextureNumericClass::Uint) &&
	     (!resource.atomic || format == Prospero::BufferFormat::k32UInt));
	if (resource_ok && descriptor_ok && encoding_ok && format_ok && size != 0) {
		return;
	}
	EXIT("unsupported storage texture: resource=%d descriptor=%d encoding=%d format=%d "
	     "class=%u numeric=%u dimension=%u mip_mode=%u atomic=%d compare=%d "
	     "base_level=%u last_level=%u max_mip=%u min_lod=%u base_array=%u bc=%u msaa=%d "
	     "depth_tile_bpe=%u swizzle_ok=%d "
	     "addr=0x%016" PRIx64 " size=0x%016" PRIx64
	     " extent=%ux%ux%u type=%u format=%u tile=%u swizzle=0x%03x read=%d written=%d "
	     "dwords=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
	     resource_ok, descriptor_ok, encoding_ok, format_ok,
	     static_cast<uint32_t>(resource.resource_class),
	     static_cast<uint32_t>(resource.numeric_class), static_cast<uint32_t>(resource.dimension),
	     static_cast<uint32_t>(resource.mip_mode), resource.atomic, resource.depth_compare,
	     descriptor.BaseLevel(), descriptor.LastLevel(), descriptor.MaxMip(), descriptor.MinLod(),
	     descriptor.BaseArray5(), descriptor.BCSwizzle(), descriptor.MsaaDepth(),
	     Prospero::RenderTargetBytesPerElement(format),
	     IsValidImageSwizzle(descriptor.DstSelXYZW()), descriptor.Base40(), size,
	     static_cast<uint32_t>(descriptor.Width5()) + 1u,
	     static_cast<uint32_t>(descriptor.Height5()) + 1u,
	     static_cast<uint32_t>(descriptor.Depth()) + 1u, static_cast<uint32_t>(descriptor.Type()),
	     static_cast<uint32_t>(format), static_cast<uint32_t>(descriptor.TileMode()),
	     descriptor.DstSelXYZW(), resource.read, resource.written, descriptor.fields[0],
	     descriptor.fields[1], descriptor.fields[2], descriptor.fields[3], descriptor.fields[4],
	     descriptor.fields[5], descriptor.fields[6], descriptor.fields[7]);
}

static TextureCache::ImageDesc NullTextureDesc(const ShaderRecompiler::IR::ImageResource& resource,
                                               TextureCache::BindingType                  binding) {
	TextureCache::ImageDesc desc {};
	switch (resource.numeric_class) {
		case Prospero::TextureNumericClass::Float:
			desc.info.guest_format = Prospero::BufferFormat::k32Float;
			break;
		case Prospero::TextureNumericClass::Uint:
			desc.info.guest_format = Prospero::BufferFormat::k32UInt;
			break;
		case Prospero::TextureNumericClass::Sint:
			desc.info.guest_format = Prospero::BufferFormat::k32SInt;
			break;
		default: EXIT("null image has unsupported numeric class\n");
	}
	desc.info.pixel_format    = VulkanFormat(desc.info.guest_format);
	desc.info.type            = Prospero::ImageType::kColor2D;
	desc.info.extent          = {1, 1, 1};
	desc.info.resources       = {1, 1};
	desc.info.bytes_per_block = 4;
	desc.info.samples         = 1;
	desc.info.mip_layout[0]   = {0, 0, 1, 1};
	desc.view_info.format     = desc.info.pixel_format;
	desc.view_info.type       = vk::ImageViewType::e2D;
	desc.view_info.aspect     = vk::ImageAspectFlagBits::eColor;
	desc.view_info.usage      = binding == TextureCache::BindingType::Storage
	                                ? vk::ImageUsageFlagBits::eStorage
	                                : vk::ImageUsageFlagBits::eSampled;
	desc.type                 = binding;
	return desc;
}

static void PopulateTextureMipLayout(ImageInfo& info) {
	if (info.IsVolume() && info.tile_mode != Prospero::TileMode::kLinear) {
		TileSurfaceLayout            surface {};
		const TileSurfaceDescription description {
		    info.guest_format,  info.tile_mode,    TileSurfaceDimension::Dim3D, info.extent.width,
		    info.extent.height, info.extent.depth, info.resources.levels,       1};
		if (!TileGetTiledTextureLayout(description, surface)) {
			EXIT("unsupported normalized volume texture layout\n");
		}
		for (uint32_t level = 0; level < info.resources.levels; level++) {
			const auto& mip        = surface.mips[level];
			info.mip_layout[level] = {
			    mip.offset,
			    mip.size,
			    mip.padded_width,
			    mip.padded_height,
			};
		}
		return;
	}

	TileSizeOffset levels[16] {};
	TilePaddedSize padded[16] {};
	TileGetTextureSize(info.guest_format, info.extent.width, info.extent.height,
	                   info.resources.levels, info.tile_mode, nullptr, levels, padded);
	const auto texel_shift = info.IsBlock() ? 2u : 0u;
	for (uint32_t level = 0; level < info.resources.levels; level++) {
		const auto offset =
		    levels[level].src_size != 0 ? levels[level].src_offset : levels[level].offset;
		auto size = static_cast<uint64_t>(levels[level].src_size != 0 ? levels[level].src_size
		                                                              : levels[level].size);
		if (info.IsVolume()) {
			size *= std::max(info.extent.depth >> level, 1u);
		} else {
			size *= info.resources.layers;
		}
		info.mip_layout[level] = {
		    offset,
		    size,
		    padded[level].width >> texel_shift,
		    padded[level].height >> texel_shift,
		};
	}
}

static ImageViewInfo TextureViewInfo(const ShaderRecompiler::IR::ImageResource& resource,
                                     const ShaderTextureResource& descriptor, vk::Format format,
                                     const SurfaceFormatInfo& surface_format, bool storage,
                                     uint32_t view_levels, uint32_t image_layers) {
	ImageViewInfo view {};
	view.format      = format;
	view.aspect      = vk::ImageAspectFlagBits::eColor;
	view.base_level  = descriptor.BaseLevel();
	view.level_count = view_levels;
	if (descriptor.MinLod() > descriptor.LastLevel() * 256u) {
		EXIT("texture minimum LOD exceeds last mip level: min_lod=%u last_level=%u\n",
		     descriptor.MinLod(), descriptor.LastLevel());
	}
	const auto base_lod = view.base_level * 256u;
	if (descriptor.MinLod() > base_lod) {
		view.min_lod = descriptor.MinLod() - base_lod;
	}
	view.usage = storage ? vk::ImageUsageFlagBits::eStorage : vk::ImageUsageFlagBits::eSampled;
	view.mapping =
	    storage || surface_format.conversion_format != Prospero::BufferFormat::kInvalid
	        ? vk::ComponentMapping {}
	        : TextureGetComponentMapping(descriptor.DstSelXYZW(), surface_format.host_to_storage);
	switch (resource.dimension) {
		case ShaderRecompiler::Decoder::ImageDimension::Dim1D:
			view.type       = vk::ImageViewType::e1D;
			view.base_layer = descriptor.BaseArray5();
			if (view.base_layer >= image_layers) {
				EXIT("texture base layer is out of bounds\n");
			}
			view.layer_count = 1;
			break;
		case ShaderRecompiler::Decoder::ImageDimension::Dim1DArray:
			view.type       = vk::ImageViewType::e1DArray;
			view.base_layer = descriptor.BaseArray5();
			if (view.base_layer >= image_layers) {
				EXIT("texture array base layer is out of bounds\n");
			}
			view.layer_count = image_layers - view.base_layer;
			break;
		case ShaderRecompiler::Decoder::ImageDimension::Dim3D:
			view.type        = vk::ImageViewType::e3D;
			view.base_layer  = 0;
			view.layer_count = 1;
			break;
		case ShaderRecompiler::Decoder::ImageDimension::Dim2DArray:
		case ShaderRecompiler::Decoder::ImageDimension::Dim2DMsaaArray:
			view.type       = vk::ImageViewType::e2DArray;
			view.base_layer = descriptor.BaseArray5();
			if (view.base_layer >= image_layers) {
				EXIT("texture array base layer is out of bounds\n");
			}
			view.layer_count = image_layers - view.base_layer;
			break;
		case ShaderRecompiler::Decoder::ImageDimension::Dim2D:
		case ShaderRecompiler::Decoder::ImageDimension::Dim2DMsaa:
			view.type       = vk::ImageViewType::e2D;
			view.base_layer = descriptor.BaseArray5();
			if (view.base_layer >= image_layers) {
				EXIT("texture base layer is out of bounds\n");
			}
			view.layer_count = 1;
			break;
		default: EXIT("unsupported texture view dimension\n");
	}
	return view;
}

static bool TextureViewPreservesMipLayout(const TileSurfaceDescription& description,
                                          uint32_t                      view_levels) {
	TileSurfaceLayout physical {};
	TileSurfaceLayout view {};
	auto              view_description = description;
	view_description.levels            = view_levels;
	return TileGetTiledTextureLayout(description, physical) &&
	       TileGetTiledTextureLayout(view_description, view) &&
	       physical.first_tail_level == view.first_tail_level &&
	       physical.block_slice_size == view.block_slice_size &&
	       physical.total_size == view.total_size &&
	       std::equal(std::begin(physical.mips), std::begin(physical.mips) + description.levels,
	                  std::begin(view.mips));
}

static TextureCache::ImageDesc BuildTextureDescription(
    const ShaderRecompiler::IR::ImageResource& resource, const ShaderTextureResource& descriptor) {
	const bool storage = resource.written;
	const auto address         = descriptor.Base40();
	const auto width           = static_cast<uint32_t>(descriptor.Width5()) + 1u;
	const auto height          = static_cast<uint32_t>(descriptor.Height5()) + 1u;
	const auto base_level      = descriptor.BaseLevel();
	const auto last_level      = descriptor.LastLevel();
	const auto type            = TextureType(descriptor);
	const bool multisampled    = IsMultisampledTexture(type);
	const auto max_mip         = resource.r128 ? last_level : descriptor.MaxMip();
	const auto physical_levels = multisampled ? 1u : static_cast<uint32_t>(max_mip) + 1u;
	// IMAGE_STORE addresses BASE_LEVEL; only IMAGE_STORE_MIP selects other view mips.
	const bool single_storage_mip =
	    storage && resource.mip_mode != ShaderRecompiler::IR::ImageMipMode::DynamicStorage;
	const auto view_levels = multisampled || single_storage_mip
	                             ? 1u
	                             : static_cast<uint32_t>(last_level - base_level) + 1u;
	const auto levels =
	    multisampled ? 1u : std::max(physical_levels, base_level + view_levels);
	const auto tile       = descriptor.TileMode();
	const bool depth_tile = tile == Prospero::TileMode::kDepth;
	const bool msaa_tile  = depth_tile || tile == Prospero::TileMode::kRenderTarget;
	const bool msaa_array = type == Prospero::ImageType::kColor2DMsaaArray;
	if ((!multisampled && base_level > last_level) ||
	    (multisampled &&
	     (base_level != 0 || last_level == 0 || last_level > 3 || max_mip != last_level ||
	      !msaa_tile || (descriptor.MsaaDepth() && !depth_tile) ||
	      (!msaa_array && (descriptor.Depth() != 0 || descriptor.BaseArray5() != 0))))) {
		EXIT("unsupported texture mip view: base=%u last=%u levels=%u max=%u type=%u tile=%u "
		     "class=%u numeric=%u dimension=%u mip_mode=%u read=%d written=%d "
		     "dwords=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
		     base_level, last_level, levels, descriptor.MaxMip(),
		     static_cast<uint32_t>(descriptor.Type()), static_cast<uint32_t>(tile),
		     static_cast<uint32_t>(resource.resource_class),
		     static_cast<uint32_t>(resource.numeric_class),
		     static_cast<uint32_t>(resource.dimension), static_cast<uint32_t>(resource.mip_mode),
		     resource.read, resource.written, descriptor.fields[0], descriptor.fields[1],
		     descriptor.fields[2], descriptor.fields[3], descriptor.fields[4], descriptor.fields[5],
		     descriptor.fields[6], descriptor.fields[7]);
	}
	const auto samples = multisampled ? 1u << last_level : 1u;
	const auto depth          = static_cast<uint32_t>(descriptor.Depth()) + 1u;
	const auto format         = descriptor.Format();
	const auto surface_format = TextureGetSurfaceFormatInfo(format);
	const bool sampled_numeric_class =
	    storage || resource.numeric_class == Prospero::SampledTextureNumericClass(format);
	if (!storage && resource.resource_class == ShaderRecompiler::IR::ImageResourceClass::Sampled &&
	    !sampled_numeric_class) {
		EXIT("sampled image numeric class mismatch: numeric=%u format=%u addr=0x%016" PRIx64 "\n",
		     static_cast<uint32_t>(resource.numeric_class), static_cast<uint32_t>(format), address);
	}

	const bool    volume       = type == Prospero::ImageType::kColor3D;
	const bool    layered      = type == Prospero::ImageType::kColor1DArray ||
	                             type == Prospero::ImageType::kColor2DArray ||
	                             type == Prospero::ImageType::kColor2DMsaaArray;
	const auto    image_layers = layered ? depth : 1u;
	if (levels > physical_levels) {
		const TileSurfaceDescription physical {
		    format, tile, volume ? TileSurfaceDimension::Dim3D : TileSurfaceDimension::Dim2D,
		    width, height, volume ? depth : 1u, physical_levels, image_layers};
		// Texture mip views take precedence over the resource count, but must keep its storage layout.
		if (!TextureViewPreservesMipLayout(physical, levels)) {
			EXIT("unsupported texture mip view changes physical layout: base=%u last=%u max=%u "
			     "extent=%ux%ux%u tile=%u\n",
			     base_level, last_level, max_mip, width, height, depth,
			     static_cast<uint32_t>(tile));
		}
	}
	uint32_t      pitch = 0;
	TileSizeAlign size {};
	if (multisampled) {
		const auto bytes = Prospero::NumBytesPerElement(format);
		pitch            = depth_tile ? TileGetDepthPitch(width, bytes, last_level)
		                              : TileGetRenderTargetPitch(width, bytes, last_level);
		if (pitch == 0 || !TileGetRenderTargetSize(width, height, pitch, bytes, size, last_level) ||
		    size.size > UINT32_MAX / image_layers) {
			EXIT("unsupported multisample texture layout\n");
		}
		size.size *= image_layers;
	} else {
		pitch = TileGetTexturePitch(format, width, tile);
		TileGetTextureTotalSize(format, width, height, volume ? depth : image_layers,
		                        physical_levels, tile, volume, size);
	}
	EXIT_NOT_IMPLEMENTED(size.size == 0 || size.align == 0 ||
	                     (address & (static_cast<uint64_t>(size.align) - 1u)) != 0);
	if (storage) {
		ValidateStorageTexture(resource, descriptor, size.size);
	}

	auto pixel_format = surface_format.vk_format;
	if (resource.depth_compare) {
		if (const auto* depth_format = FindGuestDepthFormatPolicy(format)) {
			pixel_format = depth_format->depth_attachment_format;
		}
	}
	const auto storage_view_format = storage && format == Prospero::BufferFormat::k32SInt
	                                     ? vk::Format::eR32Uint
	                                     : SrgbStorageViewFormat(pixel_format);
	const auto view_format         = storage && storage_view_format != vk::Format::eUndefined
	                                     ? storage_view_format
	                                     : pixel_format;
	const auto block_bytes         = Prospero::BlockCompressedBytesPerBlock(format);
	TextureCache::ImageDesc desc {};
	desc.info.data         = {address, size.size};
	desc.info.pixel_format = pixel_format;
	desc.info.guest_format = format;
	desc.info.type         = TextureBaseType(type);
	desc.info.extent       = {width, height, volume ? depth : 1u};
	desc.info.resources    = {levels, image_layers};
	desc.info.pitch        = pitch;
	desc.info.bytes_per_block =
	    block_bytes != 0 ? block_bytes : Prospero::NumBytesPerElement(format);
	desc.info.samples   = samples;
	desc.info.tile_mode = tile;
	if (!resource.r128 && descriptor.MetaCompress() && tile != Prospero::TileMode::kDepth &&
	    !desc.info.IsDepth()) {
		TileSizeAlign metadata_size {};
		(void)TileGetDccSize(width, height, volume ? depth : image_layers,
		                     desc.info.bytes_per_block, physical_levels, tile, metadata_size,
		                     std::countr_zero(samples));
		desc.info.metadata.kind          = ImageMetadataKind::Dcc;
		desc.info.metadata.range         = {descriptor.MetaAddr() << 8u, metadata_size.size};
		desc.info.metadata.dcc_alpha_msb = descriptor.DccAlphaPos();
	}
	if (samples > 1) {
		desc.info.mip_layout[0] = {0, size.size, pitch, height};
	} else {
		PopulateTextureMipLayout(desc.info);
	}
	desc.view_info = TextureViewInfo(resource, descriptor, view_format, surface_format, storage,
	                                 view_levels, desc.info.resources.layers);
	desc.type = storage ? TextureCache::BindingType::Storage : TextureCache::BindingType::Texture;

	return desc;
}

TextureBinding RenderExecutor::ResolveTexture(const ShaderRecompiler::IR::ImageResource&   resource,
                                              const ShaderRecompiler::IR::DescriptorValue& value) {
	TextureBinding binding;
	ResolveTexture(resource, value, binding);
	return binding;
}

void RenderExecutor::ResolveTexture(const ShaderRecompiler::IR::ImageResource&   resource,
                                    const ShaderRecompiler::IR::DescriptorValue& value,
                                    TextureBinding&                              binding) {
	auto descriptor = DecodeNativeDescriptor<ShaderTextureResource>(value);
	// The same state a freshly returned binding had: no view yet, no mip views (their capacity
	// is kept), undefined layout.
	binding.image_view = nullptr;
	binding.layout     = vk::ImageLayout::eUndefined;
	binding.mip_views.clear();

	auto&      texture_cache     = m_context.GetTextureCache();
	const bool memo              = TextureBindingMemo::Enabled();
	const bool description_cache =
	    Common::RendererBatchEnabled() && resource.indirect_resources.size() <= 256u;
	TextureBindingMemo::Key memo_key;
	uint64_t                hash = 0;
	if (memo || description_cache) {
		memo_key = TextureBindingMemo::MakeKey(resource, descriptor.fields);
		hash     = TextureBindingMemo::Hash(memo_key);
	}
	// Exactly the answer of the full resolution below (see textureBindingMemo.h), including the
	// FindImage access bookkeeping; validation of the key's resource already passed.
	if (memo && m_texture_memo.TryResolve(texture_cache, memo_key, hash, binding)) {
		if (!descriptor.IsNull() && HangTrace::Enabled()) {
			HangTrace::RecordTexture(descriptor.fields);
		}
		if (m_texture_memo.LastHitRevalidated() && TextureBindingMemo::RevalidateVerify()) {
			// KYTY_TEXTURE_MEMO_REVALIDATE_VERIFY: the full resolution (without the memo) must
			// give the same image and description.
			TextureBinding full;
			ResolveTextureFull(resource, descriptor, memo_key, hash, false, full);
			if (full.image_id != binding.image_id || full.desc.type != binding.desc.type ||
			    !(full.desc.view_info == binding.desc.view_info) ||
			    full.desc.info.data != binding.desc.info.data ||
			    full.desc.info.pixel_format != binding.desc.info.pixel_format) {
				TextureBindingMemo::ReportRevalidateMismatch();
				binding = std::move(full);
			}
		}
		return;
	}
	ResolveTextureFull(resource, descriptor, memo_key, hash, memo, binding);
}

void RenderExecutor::ResolveTextureFull(const ShaderRecompiler::IR::ImageResource& resource,
                                        const ShaderTextureResource&               descriptor,
                                        const TextureBindingMemo::Key& memo_key, uint64_t hash,
                                        bool memo, TextureBinding& binding) {
	auto&      texture_cache     = m_context.GetTextureCache();
	const bool storage           = resource.written;
	const bool description_cache =
	    Common::RendererBatchEnabled() && resource.indirect_resources.size() <= 256u;
	if (storage) {
		ValidateStorageImageResource(resource);
	}
	TextureBindingMemo::Forget(binding);
	auto& desc = binding.desc;

	if (descriptor.IsNull()) {
		desc = NullTextureDesc(resource, storage ? TextureCache::BindingType::Storage
		                                         : TextureCache::BindingType::Texture);
		binding.image_id = texture_cache.FindImage(desc);
		if (memo) {
			m_texture_memo.Record(texture_cache, memo_key, hash, binding, binding.image_id, false,
			                      false);
		}
		return;
	}

	if (HangTrace::Enabled()) {
		HangTrace::RecordTexture(descriptor.fields);
	}

	if (description_cache) {
		const TextureDescriptionKey key {resource.resource_class, resource.numeric_class,
		                                 resource.dimension,      resource.mip_mode,
		                                 resource.mip_count,      resource.conversion_format,
		                                 resource.shader_swizzle, resource.read,
		                                 resource.written,        resource.atomic,
		                                 resource.depth_compare,  resource.cube,
		                                 resource.r128};
		// TextureBindingMemo::Hash is this cache's hash of the dwords and key.
		auto& entry = m_texture_descriptions[hash % m_texture_descriptions.size()];
		if (entry.valid && entry.key == key &&
		    std::ranges::equal(entry.words, descriptor.fields)) {
			desc = entry.desc;
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureDescriptionHits);
		} else {
			desc = BuildTextureDescription(resource, descriptor);
			entry.key = key;
			std::ranges::copy(descriptor.fields, entry.words.begin());
			entry.desc = desc;
			entry.valid = true;
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureDescriptionMisses);
		}
	} else {
		desc = BuildTextureDescription(resource, descriptor);
	}
	// Only descriptor-derived geometry is retained. FindImage may alter the local
	// description and must still resolve ownership, aliases, uploads and metadata.
	const auto pixel_format = desc.info.pixel_format;
	const auto view_format = desc.view_info.format;
	const auto byte_size = desc.info.data.size;
	const auto base_level = desc.view_info.base_level;
	const auto base_layer = desc.view_info.base_layer;
	const bool shader_conversion = TextureGetSurfaceFormatInfo(descriptor.Format()).conversion_format !=
	                               Prospero::BufferFormat::kInvalid;
	auto       id                  = texture_cache.FindImage(desc, shader_conversion);
	const auto found               = id;
	auto*      image               = &texture_cache.GetImage(id);
	const bool stencil_association = static_cast<bool>(image->depth_id);
	if (stencil_association) {
		id    = image->depth_id;
		image = &texture_cache.GetImage(id);
	} else if (image->info.IsDepth()) {
		if (storage) {
			EXIT("depth target cannot be bound as a storage image\n");
		}
		ValidateSampledDepthBinding(resource, descriptor, *image, pixel_format, byte_size);
	} else if (storage) {
		ValidateStorageColorView(image->info.pixel_format, view_format, descriptor.DstSelXYZW());
	} else {
		(void)SelectSampledColorView(image->info.pixel_format, pixel_format,
		                             descriptor.DstSelXYZW());
	}
	binding.image_id = id;
	if (memo) {
		const bool view_rebased =
		    desc.view_info.base_level != base_level || desc.view_info.base_layer != base_layer;
		m_texture_memo.Record(texture_cache, memo_key, hash, binding, found, shader_conversion,
		                      view_rebased);
	}
}

static bool SamplerMemoEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_SAMPLER_MEMO");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

vk::Sampler RenderExecutor::NativeSampler(const ShaderRecompiler::IR::CompiledShaderInfo& program,
                                          uint32_t                                        index,
                                          const ShaderRecompiler::IR::DescriptorValue&    value) {
	auto descriptor = DecodeNativeDescriptor<ShaderSamplerResource>(value);
	if (!program.info.samplers[index].depth_compare) {
		descriptor.fields[0] &= ~(0x7u << 12u);
	}
	if (program.info.samplers[index].force_point_filtering) {
		descriptor.SetPointFiltering();
	}
	if (!SamplerMemoEnabled()) {
		return m_context.GetSamplerCache().GetSampler(descriptor);
	}
	// Keyed on exactly the SamplerCache key (the four final dwords).
	static_assert(sizeof(descriptor.fields) == sizeof(SamplerMemoEntry::fields));
	const auto slot = (descriptor.fields[0] * 0x9e3779b1u ^ descriptor.fields[1] * 0x85ebca6bu ^
	                   descriptor.fields[2] * 0xc2b2ae35u ^ descriptor.fields[3]) >>
	                  26u;
	auto& entry = m_sampler_memo[slot % m_sampler_memo.size()];
	if (entry.sampler != nullptr &&
	    std::memcmp(entry.fields.data(), descriptor.fields, sizeof(descriptor.fields)) == 0) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::SamplerMemoHits);
		return entry.sampler;
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::SamplerMemoMisses);
	const auto sampler = m_context.GetSamplerCache().GetSampler(descriptor);
	std::memcpy(entry.fields.data(), descriptor.fields, sizeof(descriptor.fields));
	entry.sampler = sampler;
	return sampler;
}

static vk::DescriptorBufferInfo NativeUpload(RenderContext&            context,
                                             std::span<const uint32_t> data) {
	EXIT_IF(data.empty());
	auto& command_buffer = context.GetCommandScheduler().Current();
	EXIT_IF(command_buffer.IsInvalid());
	auto&      buffer = context.GetBufferCache().GetUtilityBuffer(MemoryUsage::Stream);
	const auto offset = buffer.Copy(data.data(), data.size_bytes(), 256);
	return {buffer.Handle(), offset, data.size_bytes()};
}

// KYTY_UPLOAD_DEDUP=0 restores the previous per-draw upload memo (64 hashed slots, only with
// KYTY_RENDERER_BATCH=1).
static bool UploadDedupEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_UPLOAD_DEDUP");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

vk::DescriptorBufferInfo RenderExecutor::UploadShaderData(std::span<const uint32_t> data,
                                                          uint32_t                  site) {
	// These shader tables are read-only and ring allocations live until their GPU
	// tick retires. A wrap that reuses them must submit/wait and advance that tick.
	if (UploadDedupEnabled()) {
		// Identical bytes uploaded earlier in the same tick (the same command buffer) reuse that
		// allocation: the ring never overwrites a range during the tick that allocated it (a wrap
		// that reaches it first submits and completes that tick, which changes CurrentTick()),
		// and the tables are read-only. Equal contents then give equal descriptors, which the
		// push-descriptor shadow can skip.
		if (data.size_bytes() > 16u * 1024u) {
			return NativeUpload(m_context, data);
		}
		EXIT_IF(data.empty());
		const auto tick = m_context.GetCommandScheduler().CurrentTick();
		auto&      last_slot = m_upload_last_slot[site % m_upload_last_slot.size()];
		if (const auto& last = m_upload_dedup[last_slot];
		    last.allocation.buffer != nullptr && last.tick == tick &&
		    std::ranges::equal(data, last.words)) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderUploadLastHits);
			Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderUploadReuseHits);
			Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderUploadBytesAvoided,
			                          data.size_bytes());
			return last.allocation;
		}
		const auto hash = XXH3_64bits(data.data(), data.size_bytes());
		const auto slot = static_cast<uint32_t>(hash % m_upload_dedup.size());
		auto&      entry = m_upload_dedup[slot];
		last_slot        = slot;
		if (entry.allocation.buffer != nullptr && entry.tick == tick && entry.hash == hash &&
		    std::ranges::equal(data, entry.words)) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderUploadReuseHits);
			Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderUploadBytesAvoided,
			                          data.size_bytes());
			return entry.allocation;
		}
		const auto allocation = NativeUpload(m_context, data);
		entry.words.assign(data.begin(), data.end());
		entry.allocation = allocation;
		entry.hash       = hash;
		// The upload may have submitted (ring wrap): the allocation belongs to the tick after it.
		entry.tick = m_context.GetCommandScheduler().CurrentTick();
		Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderUploadReuseMisses);
		return allocation;
	}
	if (!Common::RendererBatchEnabled() || data.size_bytes() > 16u * 1024u) {
		return NativeUpload(m_context, data);
	}
	EXIT_IF(data.empty());
	auto& scheduler = m_context.GetCommandScheduler();
	const auto tick = scheduler.CurrentTick();
	const auto hash = XXH3_64bits(data.data(), data.size_bytes());
	auto& cached = m_shader_uploads[hash % m_shader_uploads.size()];
	if (cached.allocation.buffer != nullptr && cached.tick == tick && cached.hash == hash &&
	    std::ranges::equal(data, cached.words)) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderUploadReuseHits);
		Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderUploadBytesAvoided, data.size_bytes());
		return cached.allocation;
	}
	const auto allocation = NativeUpload(m_context, data);
	cached.words.assign(data.begin(), data.end());
	cached.allocation = allocation;
	cached.hash = hash;
	cached.tick = scheduler.CurrentTick();
	Profiler::CountFrameEvent(Profiler::FrameEvent::ShaderUploadReuseMisses);
	return allocation;
}

void RenderExecutor::BindImage(ImageId id, bool storage) {
	auto& image = m_context.GetTextureCache().GetImage(id);
	if (image.info.data.Empty()) {
		return;
	}
	if (image.binding.is_bound) {
		image.binding.force_general |= image.binding.shader_write != storage;
	}
	if (!Common::RendererBatchEnabled() || (!image.binding.is_bound && !image.binding.is_target)) {
		m_bound_images.push_back(id);
	}
	image.binding.is_bound = true;
	image.binding.shader_write |= storage;
}

void RenderExecutor::BindRenderTarget(ImageId id) {
	auto& image             = m_context.GetTextureCache().GetImage(id);
	if (!Common::RendererBatchEnabled() || (!image.binding.is_bound && !image.binding.is_target)) {
		m_bound_images.push_back(id);
	}
	image.binding.is_target = true;
}

void RenderExecutor::ResetBindings() {
	for (const auto id: m_bound_images) {
		if (auto* image = m_context.GetTextureCache().m_slot_images.try_get(id); image != nullptr) {
			image->binding = {};
		}
	}
	m_bound_images.clear();
}

void RenderExecutor::PrepareBindings(const ShaderStageRuntime& runtime,
                                     PreparedBindings& prepared) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(!runtime);
	const auto& program  = *runtime.program;
	const auto& snapshot = *runtime.resources;
	prepared.runtime = &runtime;
	prepared.gds = {nullptr, 0, VK_WHOLE_SIZE};
	prepared.flattened_srt = {};
	prepared.shader_data_buffer = {};
	prepared.buffer_sources.clear();
	prepared.buffers.clear();
	prepared.images.resize(program.info.images.size());
	prepared.samplers.clear();
	prepared.shader_data.clear();
	for (uint32_t i = 0; i < program.info.images.size(); i++) {
		auto& binding = prepared.images[i];
		ResolveTexture(program.info.images[i], snapshot.images[i], binding);
		BindImage(binding.image_id, binding.desc.type == TextureCache::BindingType::Storage);
	}
	prepared.samplers.reserve(program.info.samplers.size());
	for (uint32_t i = 0; i < program.info.samplers.size(); i++) {
		prepared.samplers.push_back(NativeSampler(program, i, snapshot.samplers[i]));
	}
	prepared.shader_data.reserve(program.bindings.ShaderDataDwords());
	for (const auto reg: program.bindings.user_data_registers) {
		prepared.shader_data.push_back(snapshot.user_data[reg - program.user_data_base]);
	}
	prepared.shader_data.resize(program.bindings.ShaderDataDwords());
	if (ShaderRecompiler::IR::FindBinding(
	        program.bindings, ShaderRecompiler::IR::DescriptorBindingKind::Gds) != nullptr) {
		prepared.gds.buffer = m_context.GetBufferCache().GetGdsBuffer()->Handle();
	}
}

void RenderExecutor::FindBuffers(PreparedBindings& prepared) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(prepared.runtime == nullptr || !*prepared.runtime);
	const auto& program  = *prepared.runtime->program;
	const auto& snapshot = *prepared.runtime->resources;
	auto&       cache    = m_context.GetBufferCache();

	prepared.buffer_sources.clear();
	prepared.buffer_sources.reserve(program.info.buffers.size());
	for (uint32_t i = 0; i < program.info.buffers.size(); i++) {
		auto descriptor = DecodeNativeDescriptor<ShaderBufferResource>(snapshot.buffers[i]);
		const auto address = descriptor.Base48();
		const auto requested_size = descriptor.GetSize();
		if (address == 0 || requested_size == 0) {
			prepared.buffer_sources.push_back({});
			continue;
		}
		const auto size = Libs::LibKernel::Memory::ClampRangeSize(address, requested_size);
		prepared.buffer_sources.push_back({address, size, cache.FindBuffer(address, size)});
	}
}

namespace {

struct WriteRangeScratch {
	ShaderRecompiler::IR::WriteRangeEvaluator         evaluator;
	std::vector<ShaderRecompiler::IR::WriteRangeSpan> spans;
	std::vector<GuestRange>                           ranges;
};

WriteRangeScratch& ThreadWriteRangeScratch() {
	thread_local WriteRangeScratch scratch;
	return scratch;
}

// The descriptor's stride/swizzle/ADD_TID bits normalized exactly as the resource
// specialization that selected the compiled program (BuildResourceSpecialization).
uint32_t SpecializedPackedStride(const ShaderBufferResource& descriptor) {
	auto       packed  = descriptor.PackedStride();
	const auto stride  = packed & 0x3fffu;
	const bool swizzle = stride != 0u && ((packed >> 14u) & 1u) != 0u;
	if (stride == 0u) {
		packed &= ~((1u << 14u) | (3u << 16u));
	} else if (!swizzle) {
		packed &= ~(3u << 16u);
	}
	return packed;
}

} // namespace

// Guest ranges the shader can write through writable buffer `index`, or nullptr when the whole
// binding must be treated as written (no proof, proof covers everything, or disabled).
static const std::vector<GuestRange>* ResolveWrittenRanges(RenderContext&            context,
                                                           const ShaderStageRuntime& runtime,
                                                           const PreparedBindings&   prepared,
                                                           uint32_t index, bool& evaluated,
                                                           WriteRangeScratch& scratch) {
	const auto& program  = *runtime.program;
	const auto& snapshot = *runtime.resources;
	const auto& resource = program.info.buffers[index];
	const auto& source   = prepared.buffer_sources[index];
	if (!resource.written || source.address == 0 || source.size == 0) {
		return nullptr;
	}
	const auto* entry  = program.write_ranges.Find(index);
	bool        proven = false;
	const char* result = "unprovable";
	if (!PreciseWriteRangesEnabled()) {
		result = "disabled";
	} else if (entry == nullptr || !entry->bounded) {
		result = "unprovable";
	} else if (index >= snapshot.buffers.size() ||
	           SpecializedPackedStride(DecodeNativeDescriptor<ShaderBufferResource>(
	               snapshot.buffers[index])) != entry->packed_stride) {
		result = "stride-mismatch";
	} else {
		if (!evaluated) {
			const ShaderRecompiler::IR::WriteRangeInputs inputs {
			    .user_data     = snapshot.user_data,
			    .flattened_srt = snapshot.flattened_srt,
			    .groups        = prepared.dispatch_groups,
			    .has_groups    = prepared.has_dispatch_groups,
			};
			scratch.evaluator.Evaluate(program.write_ranges, inputs);
			evaluated = true;
		}
		proven = scratch.evaluator.Spans(program.write_ranges, index, source.size, scratch.spans);
		result = proven ? "narrowed" : "unbounded";
	}
	uint64_t written_bytes = source.size;
	scratch.ranges.clear();
	if (proven) {
		written_bytes = 0;
		for (const auto& span: scratch.spans) {
			scratch.ranges.push_back({source.address + span.begin, span.end - span.begin});
			written_bytes += span.end - span.begin;
		}
	}
	const bool narrowed = proven && written_bytes < source.size;
	if (proven && !narrowed) {
		result = "covers-binding";
	}
	uint32_t spared = 0;
	if (narrowed) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::WriteRangeNarrowed);
		Profiler::CountFrameEvent(Profiler::FrameEvent::WriteRangeBytesAvoided,
		                          source.size - written_bytes);
		if (WriteRangeImageStatsEnabled()) {
			spared = context.GetTextureCache().CountImagesOutsideGpuWrite(source.address,
			                                                              source.size, scratch.ranges);
			Profiler::CountFrameEvent(Profiler::FrameEvent::WriteRangeImagesSpared, spared);
		}
	} else {
		Profiler::CountFrameEvent(Profiler::FrameEvent::WriteRangeWhole);
	}

	if (const auto limit = WriteRangeLogLimit(); limit != 0u) {
		static std::atomic<uint32_t> logged {0};
		if (logged.fetch_add(1, std::memory_order_relaxed) < limit) {
			std::string spans;
			for (const auto& range: scratch.ranges) {
				spans += fmt::format(" 0x{:x}+0x{:x}", range.address, range.size);
			}
			const auto detail =
			    entry != nullptr && evaluated
			        ? scratch.evaluator.Describe(program.write_ranges, index)
			        : std::string(entry == nullptr ? "no-proof" : entry->bounded ? "" : "unprovable");
			LOGF("WriteRange: stage=%s shader=0x%016" PRIx64 " slot=%u guest=0x%016" PRIx64
			     " size=0x%" PRIx64 " read=%d atomic=%d groups=%s%ux%ux%u result=%s written=0x%" PRIx64
			     " images_spared=%u spans:%s detail: %s\n",
			     ShaderStageResourceName(program.stage), program.shader_hash, index, source.address,
			     source.size, resource.read ? 1 : 0, resource.atomic ? 1 : 0,
			     prepared.has_dispatch_groups ? "" : "unknown:", prepared.dispatch_groups[0],
			     prepared.dispatch_groups[1], prepared.dispatch_groups[2], result, written_bytes,
			     spared, spans.c_str(), detail.c_str());
		}
	}
	return narrowed ? &scratch.ranges : nullptr;
}

void RenderExecutor::RebindBuffers(PreparedBindings& prepared) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(prepared.runtime == nullptr || !*prepared.runtime);
	const auto& program   = *prepared.runtime->program;
	const auto& snapshot  = *prepared.runtime->resources;
	const auto& layout    = program.bindings;
	EXIT_IF(prepared.buffer_sources.size() != program.info.buffers.size());

	prepared.buffers.clear();
	prepared.buffers.reserve(program.info.buffers.size());
	EXIT_IF(prepared.shader_data.size() != layout.ShaderDataDwords());
	std::fill(prepared.shader_data.begin() + layout.memory_offset_dword,
	          prepared.shader_data.end(), 0);
	auto pack_memory_offset = [&](uint32_t index, uint32_t offset) {
		const auto dword = layout.memory_offset_dword + index / 4u;
		const auto shift = (index % 4u) * 8u;
		prepared.shader_data[dword] |= offset << shift;
	};
	auto& write_scratch = ThreadWriteRangeScratch();
	bool  write_ranges_evaluated = false;
	// All bindings' uploads share one barrier pair (KYTY_UPLOAD_BATCH).
	const BufferCache::UploadBatch upload_batch(m_context.GetBufferCache());
	for (uint32_t i = 0; i < program.info.buffers.size(); i++) {
		uint32_t   buffer_offset = 0;
		const auto* written      = ResolveWrittenRanges(m_context, *prepared.runtime, prepared, i,
		                                                write_ranges_evaluated, write_scratch);
		prepared.buffers.push_back(NativeStorageBuffer(m_context, prepared.buffer_sources[i],
		                                               program.info.buffers[i], program.stage, i,
		                                               buffer_offset, written));
		pack_memory_offset(i, buffer_offset);
	}
	// GET_LOD_STATS field per image, 32 bits (LodStatsReport::ImageField): MipStatsCntId, the
	// BASE_LEVEL added to recorded levels, a no-counter flag and the LOD threshold below which a
	// sample is counted (the T# MIN_LOD with KYTY_LOD_STATS_COUNT=clamp, every sample otherwise).
	// KYTY_MIP_STATS_BASE_LEVEL=0 reports levels relative to BASE_LEVEL (the U27 behaviour).
	static const bool absolute_levels = [] {
		const char* value = std::getenv("KYTY_MIP_STATS_BASE_LEVEL");
		return !(value != nullptr && value[0] == '0');
	}();
	const bool count_clamped = LodStatsCounter::CountClamped();
	prepared.mip_stats_active = false;
	prepared.mip_stats_canary = false;
	for (uint32_t i = 0; i < layout.mip_stats_count; i++) {
		const auto field = LodStatsReport::ImageField(snapshot.images.at(i).dwords.data(),
		                                              absolute_levels, count_clamped);
		prepared.shader_data[layout.MipStatsOffsetDword() + i] = field;
		prepared.mip_stats_active |= (field & LodStatsReport::NoCounterFlag) == 0u;
	}
	// Upload sites: stage type and table kind (the dedup checks the site's last entry first).
	const auto site = static_cast<uint32_t>(program.stage) * 2u;
	if (ShaderRecompiler::IR::FindBinding(
	        layout, ShaderRecompiler::IR::DescriptorBindingKind::FlattenedSrt) != nullptr) {
		prepared.flattened_srt = UploadShaderData(snapshot.flattened_srt, site);
	}
	if (ShaderRecompiler::IR::FindBinding(
	        program.bindings, ShaderRecompiler::IR::DescriptorBindingKind::ShaderData) != nullptr) {
		prepared.shader_data_buffer = UploadShaderData(prepared.shader_data, site + 1u);
	}
}

void RenderExecutor::RebindImages(PreparedBindings& prepared) {
	KYTY_PROFILER_FUNCTION();
	EXIT_IF(prepared.runtime == nullptr || !*prepared.runtime);
	const auto& program  = *prepared.runtime->program;
	const auto& snapshot = *prepared.runtime->resources;
	auto&       images   = prepared.images;
	EXIT_IF(images.size() != program.info.images.size());
	auto& texture_cache = m_context.GetTextureCache();
	for (uint32_t i = 0; i < program.info.images.size(); i++) {
		const auto old_image = texture_cache.m_slot_images.try_get(images[i].image_id);
		if (old_image == nullptr || (!old_image->registered && !old_image->info.data.Empty()) ||
		    old_image->binding.needs_rebind) {
			if (old_image != nullptr) {
				old_image->binding = {};
			}
			ResolveTexture(program.info.images[i], snapshot.images[i], images[i]);
			BindImage(images[i].image_id,
			          images[i].desc.type == TextureCache::BindingType::Storage);
		}
	}
	for (uint32_t i = 0; i < program.info.images.size(); i++) {
		auto& binding = images[i];
		binding.mip_views.clear();
		const auto& resource = program.info.images[i];
		if (resource.mip_mode == ShaderRecompiler::IR::ImageMipMode::DynamicStorage) {
			EXIT_IF(resource.mip_count == 0u ||
			        resource.mip_count != binding.desc.view_info.level_count);
			binding.mip_views.reserve(resource.mip_count);
			for (uint32_t mip = 0; mip < resource.mip_count; mip++) {
				auto desc = binding.desc;
				desc.view_info.base_level += mip;
				desc.view_info.level_count = 1;
				binding.mip_views.push_back(texture_cache.FindTexture(binding.image_id, desc));
			}
			binding.image_view = binding.mip_views.front();
		} else if (!m_texture_memo.TryAcquireView(texture_cache, binding)) {
			binding.image_view = texture_cache.FindTexture(binding.image_id, binding.desc);
			m_texture_memo.RecordView(binding, binding.image_view);
		}
		auto&      image   = texture_cache.GetImage(binding.image_id);
		const bool storage = binding.desc.type == TextureCache::BindingType::Storage;
		image.usage.storage |= storage;
		image.usage.texture |= !storage;
	}
}

void RenderExecutor::PrepareGraphicsBindings(std::span<PreparedBindings* const> stages,
                                             std::span<RenderColorInfo> colors) {
	bool uses_dma = false;
	for (auto* stage: stages) {
		FindBuffers(*stage);
		uses_dma |= stage->runtime->program->info.uses_dma;
	}
	if (uses_dma) {
		m_context.PrepareBda();
	}
	for (auto* stage: stages) {
		RebindImages(*stage);
	}
	auto& cache = m_context.GetTextureCache();
	for (auto& target: colors) {
		EXIT_IF(!target.image_id);
		const auto old_image = cache.m_slot_images.try_get(target.image_id);
		if (old_image == nullptr || (!old_image->registered && !old_image->info.data.Empty()) ||
		    old_image->binding.needs_rebind) {
			if (old_image != nullptr) {
				old_image->binding = {};
			}
			target.desc.view_info.base_level = target.guest_mip_level;
			target.desc.view_info.base_layer = target.guest_array_layer;
			target.image_id = cache.FindImage(target.desc);
			BindRenderTarget(target.image_id);
		}
	}
	// Discovery can read back PS5 metadata and submit the scheduler. Reserve draw buffers only
	// after image identities are final; attachment layout transitions follow buffer alias copies.
	// The uploads of every stage share one barrier pair (KYTY_UPLOAD_BATCH).
	const BufferCache::UploadBatch upload_batch(m_context.GetBufferCache());
	for (auto* stage: stages) {
		RebindBuffers(*stage);
	}
}

// Why a renderer push-descriptor update was recorded (FrameEvent.DescriptorPushMiss*).
static void CountDescriptorPushMiss(int32_t result, std::span<const vk::WriteDescriptorSet> writes) {
	using Event = Profiler::FrameEvent;
	if (result == CommandBuffer::PushAvoided) {
		return;
	}
	if (result == CommandBuffer::PushMissState) {
		Profiler::CountFrameEvent(Event::DescriptorPushMissLayout);
		return;
	}
	if (result == CommandBuffer::PushMissShape || result < 0 ||
	    static_cast<size_t>(result) >= writes.size()) {
		Profiler::CountFrameEvent(Event::DescriptorPushMissShape);
		return;
	}
	// NativeBinding(stage, kind) = kind + stage group * DescriptorBindingKind::Count.
	const auto kind = static_cast<BindingKind>(writes[static_cast<size_t>(result)].dstBinding %
	                                           static_cast<uint32_t>(BindingKind::Count));
	if (ShaderRecompiler::IR::ImageBindingResourceClass(kind) !=
	    ShaderRecompiler::IR::ImageResourceClass::None) {
		Profiler::CountFrameEvent(Event::DescriptorPushMissImage);
		return;
	}
	switch (kind) {
		case BindingKind::Samplers: Profiler::CountFrameEvent(Event::DescriptorPushMissSampler); break;
		case BindingKind::Buffers: Profiler::CountFrameEvent(Event::DescriptorPushMissBuffer); break;
		case BindingKind::FlattenedSrt:
		case BindingKind::ShaderData: Profiler::CountFrameEvent(Event::DescriptorPushMissUpload); break;
		default: Profiler::CountFrameEvent(Event::DescriptorPushMissOther); break;
	}
}

void RenderExecutor::CommitBindings(CommandBuffer&                     buffer,
                                    vk::PipelineBindPoint              pipeline_bind_point,
                                    const PipelineCache::Pipeline&     pipeline,
                                    std::span<PreparedBindings* const> prepared_bindings) {
	KYTY_PROFILER_FUNCTION();
	// Run after resource discovery so an unbounded address writer cannot retain a
	// metadata-inspection memo created during preparation of this same command.
	// Read-only BDA access leaves contents unchanged; tracked descriptor writes carry
	// their own canonical-buffer revisions.
	if (std::ranges::any_of(prepared_bindings, [](const auto* prepared) {
		    return prepared->runtime->program->has_address_writes;
	    })) {
		m_context.GetBufferCache().InvalidateContentRevisions();
	}
	// State commands only: image transitions and the GDS dependency below go through the barrier
	// batcher and are recorded at the caller's next flush point (the draw's BeginRendering, the
	// dispatch's Handle()), before any command that accesses the resources.
	auto   vk_buffer        = buffer.StateHandle();
	size_t descriptor_count = 0;
	size_t write_count      = 0;
	ShaderRecompiler::IR::PushData push_data;
	bool                           has_push_data = false;
	constexpr auto                 GraphicsStages =
	    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eMeshEXT |
	    vk::ShaderStageFlagBits::eTessellationControl |
	    vk::ShaderStageFlagBits::eTessellationEvaluation | vk::ShaderStageFlagBits::eFragment;
	vk::ShaderStageFlags push_stages = pipeline_bind_point == vk::PipelineBindPoint::eGraphics
	                                       ? vk::ShaderStageFlagBits::eFragment
	                                       : vk::ShaderStageFlags {};
	bool mesh_stage = false;
	for (const auto* prepared: prepared_bindings) {
		EXIT_IF(prepared == nullptr || prepared->runtime == nullptr || !*prepared->runtime);
		const auto& program = *prepared->runtime->program;
		mesh_stage |= program.stage == ShaderType::Mesh;
		write_count += program.bindings.descriptors.size();
		for (const auto& binding: program.bindings.descriptors) {
			descriptor_count += NativeDescriptorCount(binding);
		}
		const auto shader_stage = NativeShaderStage(program.stage);
		push_stages |= shader_stage;
		EXIT_IF((pipeline_bind_point == vk::PipelineBindPoint::eGraphics &&
		         (shader_stage & GraphicsStages) == vk::ShaderStageFlags {}) ||
		        (pipeline_bind_point == vk::PipelineBindPoint::eCompute &&
		         shader_stage != vk::ShaderStageFlagBits::eCompute));
	}
	m_descriptor_buffers.clear();
	m_descriptor_images.clear();
	m_descriptor_writes.clear();
	m_descriptor_buffers.reserve(descriptor_count);
	m_descriptor_images.reserve(descriptor_count);
	m_descriptor_writes.reserve(write_count);

	for (auto* prepared: prepared_bindings) {
		const auto& program       = *prepared->runtime->program;
		auto&       descriptors   = *prepared;
		const auto  shader_stage  = NativeShaderStage(program.stage);
		const auto  shader_stages = ShaderPipelineStages(shader_stage);
		if (descriptors.gds.buffer != nullptr) {
			KYTY_GPU_OP_SITE("descriptors.gds_barrier");
			const auto barrier = MakeGdsDependency(descriptors.gds.buffer);
			constexpr auto source_stages =
			    vk::PipelineStageFlagBits::eHost | vk::PipelineStageFlagBits::eTransfer |
			    vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader;
			if (BarrierBatchEnabled()) {
				// Sync1 masks share their bit values with the synchronization2 ones.
				vk::BufferMemoryBarrier2 barrier2 {};
				barrier2.srcStageMask = vk::PipelineStageFlags2(
				    static_cast<VkPipelineStageFlags2>(VkPipelineStageFlags(source_stages)));
				barrier2.srcAccessMask = vk::AccessFlags2(
				    static_cast<VkAccessFlags2>(VkAccessFlags(barrier.srcAccessMask)));
				barrier2.dstStageMask = vk::PipelineStageFlags2(
				    static_cast<VkPipelineStageFlags2>(VkPipelineStageFlags(shader_stages)));
				barrier2.dstAccessMask = vk::AccessFlags2(
				    static_cast<VkAccessFlags2>(VkAccessFlags(barrier.dstAccessMask)));
				barrier2.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex;
				barrier2.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
				barrier2.buffer              = barrier.buffer;
				barrier2.offset              = barrier.offset;
				barrier2.size                = barrier.size;
				buffer.RequestBufferBarrier(barrier2, BarrierOrigin::Gds);
			} else {
				buffer.EndRendering();
				vk_buffer.pipelineBarrier(source_stages, shader_stages, vk::DependencyFlags {}, 0,
				                          nullptr, 1, &barrier, 0, nullptr);
			}
		}

		for (uint32_t i = 0; i < program.info.images.size(); i++) {
			auto& image   = m_context.GetTextureCache().GetImage(descriptors.images[i].image_id);
			auto& binding = descriptors.images[i];
			const auto&                 view = binding.desc.view_info;
			const ImageSubresourceRange range {view.base_level, view.level_count, view.base_layer,
			                                   view.layer_count};
			const bool storage = binding.desc.type == TextureCache::BindingType::Storage;
			if (image.info.data.Empty()) {
				image.Transit(vk::ImageLayout::eGeneral,
				              storage ? vk::AccessFlagBits2::eShaderRead |
				                            vk::AccessFlagBits2::eShaderWrite
				                      : vk::AccessFlagBits2::eShaderRead,
				              range, vk_buffer, true);
			} else if (image.binding.is_target) {
				const auto layout = image.binding.attachment_layout;
				EXIT_IF(layout == vk::ImageLayout::eUndefined);
				Profiler::CountFrameEvent(image.info.IsDepth()
				                              ? Profiler::FrameEvent::SampledDepthAttachmentBindings
				                              : Profiler::FrameEvent::SampledColorAttachmentBindings);
				if (image.info.IsDepth()) {
					const auto host_view =
					    std::ranges::find(image.views, binding.image_view, &CachedImageView::view);
					EXIT_IF(storage || host_view == image.views.end());
					const auto aspect = host_view->info.aspect;
					if (aspect & ~DepthReadableAspects(layout)) {
						EXIT("sampling a writable depth/stencil attachment aspect\n");
					}
				}
				image.Transit(layout,
				              image.binding.attachment_access | vk::AccessFlagBits2::eShaderRead |
				                  (image.binding.shader_write ? vk::AccessFlagBits2::eShaderWrite
				                                              : vk::AccessFlags2 {}),
				              {}, vk_buffer, true);
			} else if (image.binding.force_general && !image.info.IsDepth()) {
				const vk::AccessFlags2 storage_access = image.binding.shader_write
				                                            ? vk::AccessFlagBits2::eShaderWrite
				                                            : vk::AccessFlags2 {};
				image.Transit(vk::ImageLayout::eGeneral,
				              vk::AccessFlagBits2::eShaderRead | storage_access, {}, vk_buffer, true);
			} else if (storage) {
				image.Transit(vk::ImageLayout::eGeneral,
				              vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
				              range, vk_buffer, true);
			} else {
				image.Transit(image.info.IsDepth() ? vk::ImageLayout::eDepthStencilReadOnlyOptimal
				                                   : vk::ImageLayout::eShaderReadOnlyOptimal,
				              vk::AccessFlagBits2::eShaderRead, range, vk_buffer, true);
			}
			binding.layout = image.backing.state.layout;
		}

		m_image_occurrences.assign(descriptors.images.size(), 0);
		for (const auto& binding: program.bindings.descriptors) {
			vk::WriteDescriptorSet write {};
			write.dstBinding     = ShaderRecompiler::IR::NativeBinding(program.stage, binding.kind);
			write.descriptorType = NativeDescriptorType(binding.kind);
			write.descriptorCount   = NativeDescriptorCount(binding);
			const auto buffer_start = m_descriptor_buffers.size();
			const auto image_start  = m_descriptor_images.size();
			if (ShaderRecompiler::IR::ImageBindingResourceClass(binding.kind) !=
			    ShaderRecompiler::IR::ImageResourceClass::None) {
				for (const auto resource: binding.resources) {
					m_descriptor_images.push_back(MakeImageInfo(
					    descriptors.images.at(resource), m_image_occurrences.at(resource)++));
				}
			} else {
				switch (binding.kind) {
					case BindingKind::Buffers:
						for (const auto resource: binding.resources) {
							const auto& view = descriptors.buffers.at(resource);
							EXIT_IF(view.buffer == nullptr);
							m_descriptor_buffers.push_back(view);
						}
						break;
					case BindingKind::BdaPagetable:
					case BindingKind::FaultBuffer: {
						auto&       cache      = m_context.GetBufferCache();
						const auto* bda_buffer = binding.kind == BindingKind::BdaPagetable
						                             ? cache.GetBdaPageTableBuffer()
						                             : cache.GetFaultBuffer();
						m_descriptor_buffers.emplace_back(bda_buffer->Handle(), 0,
						                                  bda_buffer->Size());
						break;
					}
					case BindingKind::MipStats: {
						auto& lod_stats = m_context.GetLodStats();
						auto& counters  = descriptors.mip_stats_canary ? lod_stats.CanaryBuffer()
						                                               : lod_stats.CounterBuffer();
						m_descriptor_buffers.emplace_back(counters.Handle(), 0, counters.Size());
						break;
					}
					case BindingKind::FlattenedSrt:
					case BindingKind::ShaderData:
					case BindingKind::Gds: {
						const vk::DescriptorBufferInfo* view = &descriptors.gds;
						if (binding.kind == BindingKind::FlattenedSrt) {
							view = &descriptors.flattened_srt;
						} else if (binding.kind == BindingKind::ShaderData) {
							view = &descriptors.shader_data_buffer;
						}
						EXIT_IF(view->buffer == nullptr);
						m_descriptor_buffers.push_back(*view);
						break;
					}
					case BindingKind::Samplers:
						for (const auto resource: binding.resources) {
							const auto sampler = descriptors.samplers.at(resource);
							EXIT_IF(sampler == nullptr);
							m_descriptor_images.emplace_back(sampler, nullptr,
							                                 vk::ImageLayout::eUndefined);
						}
						break;
					case BindingKind::Count: EXIT("invalid descriptor binding kind");
				}
			}
			if (m_descriptor_buffers.size() != buffer_start) {
				write.pBufferInfo = m_descriptor_buffers.data() + buffer_start;
			}
			if (m_descriptor_images.size() != image_start) {
				write.pImageInfo = m_descriptor_images.data() + image_start;
			}
			m_descriptor_writes.push_back(write);
		}
		for (uint32_t i = 0; i < descriptors.images.size(); i++) {
			const auto expected =
			    descriptors.images[i].mip_views.empty()
			        ? 1u
			        : static_cast<uint32_t>(descriptors.images[i].mip_views.size());
			EXIT_IF(m_image_occurrences[i] != expected);
		}

		const auto shader_data_dwords = program.bindings.ShaderDataDwords();
		EXIT_IF(prepared->shader_data.size() != shader_data_dwords);
		if (program.bindings.UsesPushData()) {
			std::ranges::copy(prepared->shader_data,
			                  push_data.dwords.begin() + program.bindings.push_data_start_dword);
			has_push_data = true;
		}
	}

	if (has_push_data) {
		buffer.PushConstants(pipeline.pipeline_layout, push_stages, sizeof(push_data),
		                     push_data.dwords.data());
	}
	if (mesh_stage) {
		// The mesh draw path then records its draw arguments over the first push-constant dwords
		// through StateHandle(); the shadow no longer describes what is in effect.
		buffer.InvalidatePushConstants();
	}

	if (!m_descriptor_writes.empty()) {
		EXIT_IF(pipeline.descriptor_set_layout == nullptr);
		if (pipeline.uses_push_descriptors) {
			const auto result = buffer.PushDescriptors(
			    pipeline_bind_point, pipeline.pipeline_layout, 0,
			    static_cast<uint32_t>(m_descriptor_writes.size()), m_descriptor_writes.data());
			CountDescriptorPushMiss(result, m_descriptor_writes);
		} else {
			CommitDescriptorSet(buffer, pipeline_bind_point, pipeline);
		}
	}
}

void RenderExecutor::CommitDescriptorSet(CommandBuffer& buffer, vk::PipelineBindPoint point,
                                         const PipelineCache::Pipeline& pipeline) {
	const auto        layout = pipeline.descriptor_set_layout;
	const bool        reuse  = DescriptorSetReuseEnabled();
	const auto        tick   = m_context.GetCommandScheduler().CurrentTick();
	uint64_t          hash   = 0;
	vk::DescriptorSet set    = nullptr;
	if (reuse) {
		hash = DescriptorSetReuse::Hash(layout, m_descriptor_writes);
		set  = m_descriptor_set_reuse.Find(tick, layout, m_descriptor_writes, hash);
	}
	if (set != nullptr) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::DescriptorSetsReused);
	} else {
		set = m_context.GetDescriptorHeap().Commit(layout);
		for (auto& write: m_descriptor_writes) {
			write.dstSet = set;
		}
		m_context.GetGraphics().device.updateDescriptorSets(
		    static_cast<uint32_t>(m_descriptor_writes.size()), m_descriptor_writes.data(), 0,
		    nullptr);
		Profiler::CountFrameEvent(Profiler::FrameEvent::DescriptorSetsWritten);
		if (reuse) {
			m_descriptor_set_reuse.Insert(tick, layout, m_descriptor_writes, hash, set);
		}
	}
	buffer.BindDescriptorSet(point, pipeline.pipeline_layout, set);
}

} // namespace Libs::Graphics
