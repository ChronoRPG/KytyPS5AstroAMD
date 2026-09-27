#include "graphics/host_gpu/renderer/cache/textureCache.h"

#include "common/alignment.h"
#include "common/assert.h"
#include "common/hangTrace.h"
#include "common/emulatorConfig.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "graphics/guest_gpu/gpu_format.h"
#include "graphics/guest_gpu/graphicsRun.h"
#include "graphics/guest_gpu/tile.h"
#include "graphics/host_gpu/cleanVerdictCache.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/cache/bufferCache.h"
#include "graphics/host_gpu/renderer/commandScheduler.h"
#include "graphics/host_gpu/renderer/image/dccClear.h"
#include "graphics/host_gpu/renderer/image/imageView.h"
#include "graphics/host_gpu/renderer/image/stagingCopier.h"
#include "graphics/host_gpu/renderer/image/textureCommon.h"
#include "graphics/host_gpu/renderer/image/tiler.h"
#include "graphics/host_gpu/renderer/render.h"
#include "kernel/memory.h"
#include "graphics/host_gpu/renderer/gpuOpProfiler.h"

#include <algorithm>
#include <array>
#include <optional>
#include <atomic>
#include <bit>
#include <cinttypes>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <span>
#include <tuple>
#include <vulkan/vulkan_format_traits.hpp>
#include <xxhash.h>

namespace Libs::Graphics {

namespace {

constexpr uint64_t NumFramesBeforeRemoval = 32;
// Overlapping aliases (transient render targets sharing a heap) stay registered until unused
// for this many presented guest frames; see AliasAgeByFrames.
constexpr uint64_t AliasFramesBeforeRemoval = 4;

// KYTY_IMAGE_ALIAS_AGE=ticks restores the previous rule that frees an overlapped alias after
// NumFramesBeforeRemoval scheduler ticks, which is usually within the same guest frame.
static bool AliasAgeByFrames() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_IMAGE_ALIAS_AGE");
		return value == nullptr || std::strcmp(value, "ticks") != 0;
	}();
	return enabled;
}

[[nodiscard]] bool EnvNotZero(const char* name) {
	const auto* value = std::getenv(name);
	return value == nullptr || std::strcmp(value, "0") != 0;
}

// KYTY_DIRECT_IMAGE_COPY=0 restores the image->buffer->image copy for every depth<->color
// reinterpretation. KYTY_DIRECT_IMAGE_COPY_SHADER=0 disables only the one-pass shader
// reinterpretation; KYTY_DIRECT_IMAGE_COPY_M8=0 (read at device creation) only the
// VK_KHR_maintenance8 depth<->color vkCmdCopyImage.
static bool DirectImageCopyEnabled() {
	static const bool enabled = EnvNotZero("KYTY_DIRECT_IMAGE_COPY");
	return enabled;
}

static bool DirectImageCopyShaderEnabled() {
	static const bool enabled =
	    DirectImageCopyEnabled() && EnvNotZero("KYTY_DIRECT_IMAGE_COPY_SHADER");
	return enabled;
}

// KYTY_ALIAS_SYNC_SKIP=0 copies the owner into a kept alias on every ownership switch, even when
// the alias already holds exactly the owner's native contents (see Image::ContentSerial).
static bool AliasSyncSkipEnabled() {
	static const bool enabled = EnvNotZero("KYTY_ALIAS_SYNC_SKIP");
	return enabled;
}

// VK_KHR_maintenance8 "depth/stencil to color" copy compatibility, restricted to depth-only
// images and to the color formats listed for their depth aspect.
[[nodiscard]] bool Maintenance8CopyCompatible(vk::Format depth, vk::Format color) {
	switch (depth) {
		case vk::Format::eD32Sfloat:
			return color == vk::Format::eR32Sfloat || color == vk::Format::eR32Uint ||
			       color == vk::Format::eR32Sint;
		case vk::Format::eD16Unorm:
			return color == vk::Format::eR16Unorm || color == vk::Format::eR16Uint ||
			       color == vk::Format::eR16Sint;
		default: return false;
	}
}

[[nodiscard]] const char* UploadBindingName(TextureCache::BindingType binding) {
	switch (binding) {
		case TextureCache::BindingType::Texture: return "texture";
		case TextureCache::BindingType::Storage: return "storage";
		case TextureCache::BindingType::RenderTarget: return "render-target";
		case TextureCache::BindingType::DepthTarget: return "depth-target";
		case TextureCache::BindingType::VideoOut: return "video-out";
	}
	return "other";
}

// The stock Tracy CSV exporter supports -m. Keep messages free of commas/newlines,
// and bound diagnostics independently of how long a detailed capture stays connected.
std::atomic<uint32_t> g_dcc_diagnostic_messages {0};

template <typename... Args>
void TraceDccDiagnostic(const char* format, Args... args) {
	if (!Profiler::DetailedEnabled() || !tracy::ProfilerAvailable() || !TracyIsConnected) return;
	constexpr uint32_t MaxMessages = 8192;
	const auto ordinal = g_dcc_diagnostic_messages.fetch_add(1, std::memory_order_relaxed);
	if (ordinal >= MaxMessages) {
		if (ordinal == MaxMessages) TracyMessageLS("DCC_DIAG_LIMIT messages=8192", 0);
		return;
	}
	char text[768];
	const auto written = std::snprintf(text, sizeof(text), format, args...);
	if (written > 0 && static_cast<size_t>(written) < sizeof(text)) {
		TracyMessageS(text, static_cast<size_t>(written), 0);
	}
}

[[nodiscard]] bool DecodeDccClear(const TextureCache::ImageDesc& desc, uint8_t code,
                                  vk::ClearColorValue& clear) {
	switch (code) {
		case 0x00:
		case 0x20:
		case 0x40:
		case 0x80:
		case 0xc0: break;
		default: return false;
	}
	const auto& metadata = desc.info.metadata;
	const auto  format   = desc.view_info.format;
	if (code == 0x20) {
		// Clear-to-register is a color-buffer operation; the texture pipe cannot decode it.
		return desc.type == TextureCache::BindingType::RenderTarget &&
		       metadata.dcc_clear_register_valid &&
		       DecodePackedColorClear(format, metadata.dcc_clear_word, clear);
	}
	clear = {};
	if (code == 0x00) {
		return true;
	}
	switch (format) {
		case vk::Format::eR8Unorm:
		case vk::Format::eR8G8Unorm:
		case vk::Format::eR8G8B8A8Unorm:
		case vk::Format::eR8G8B8A8Srgb:
		case vk::Format::eB8G8R8A8Unorm:
		case vk::Format::eB8G8R8A8Srgb:
		case vk::Format::eA2B10G10R10UnormPack32:
		case vk::Format::eA2R10G10B10UnormPack32:
		case vk::Format::eR5G6B5UnormPack16:
		case vk::Format::eA1R5G5B5UnormPack16:
		case vk::Format::eR4G4B4A4UnormPack16:
		case vk::Format::eR16Unorm:
		case vk::Format::eR16G16Unorm:
		case vk::Format::eR16G16B16A16Unorm:
		case vk::Format::eR16Sfloat:
		case vk::Format::eR16G16Sfloat:
		case vk::Format::eR16G16B16A16Sfloat:
		case vk::Format::eR32Sfloat:
		case vk::Format::eR32G32Sfloat:
		case vk::Format::eR32G32B32A32Sfloat:
		case vk::Format::eB10G11R11UfloatPack32: break;
		default: return false;
	}
	const float          rgb   = (code & 0x80u) != 0 ? 1.0f : 0.0f;
	const float          alpha = (code & 0x40u) != 0 ? 1.0f : 0.0f;
	std::array<float, 4> channels {rgb, rgb, rgb, alpha};
	if (!metadata.dcc_alpha_msb) {
		std::swap(channels[0], channels[3]);
	}
	// DCC clear decoding clamps missing lanes before applying the format swizzle.
	const auto components = vk::componentCount(format);
	if (components == 1) {
		channels[0] = channels[3];
	} else if (components == 2) {
		channels[1] = channels[3];
	}
	switch (format) {
		case vk::Format::eB8G8R8A8Unorm:
		case vk::Format::eB8G8R8A8Srgb:
		case vk::Format::eA2R10G10B10UnormPack32:
		case vk::Format::eA1R5G5B5UnormPack16:
		case vk::Format::eR5G6B5UnormPack16: std::swap(channels[0], channels[2]); break;
		case vk::Format::eR4G4B4A4UnormPack16:
			std::reverse(channels.begin(), channels.end());
			break;
		default: break;
	}
	clear.float32 = channels;
	return true;
}

[[nodiscard]] std::vector<vk::BufferImageCopy> BuildDepthCopies(const ImageInfo& info,
                                                              uint64_t slice_stride,
                                                              vk::ImageAspectFlags aspect) {
	std::vector<vk::BufferImageCopy> copies(info.resources.layers);
	for (uint32_t layer = 0; layer < info.resources.layers; ++layer) {
		auto& copy             = copies[layer];
		copy.bufferOffset      = slice_stride * layer;
		copy.bufferRowLength   = info.pitch;
		copy.bufferImageHeight = info.extent.height;
		copy.imageSubresource  = {aspect, 0, layer, 1};
		copy.imageExtent       = {info.extent.width, info.extent.height, 1};
	}
	return copies;
}

[[nodiscard]] std::vector<GpuTileInfo> BuildDepthTiles(const ImageInfo& info) {
	TileBlockLayout block {};
	EXIT_NOT_IMPLEMENTED(
	    !TileGetBlockLayout(TileBlockFamily::Depth64KB, info.bytes_per_block, block));
	const auto               full_slice_size = info.data.size / info.resources.layers;
	std::vector<GpuTileInfo> tiles;
	tiles.reserve(info.resources.layers);
	for (uint32_t layer = 0; layer < info.resources.layers; ++layer) {
		const auto offset = full_slice_size * layer;
		tiles.push_back({block.family, block.bytes_per_element, offset, full_slice_size, offset,
		                 full_slice_size, 0, info.extent.width, info.extent.height, 1, info.pitch});
		tiles.back().surface_z = layer;
	}
	return tiles;
}

} // namespace

struct TextureCache::TextureTransfer {
	TextureUploadLayout              layout;
	std::vector<vk::BufferImageCopy> regions;
	std::vector<GpuTileInfo>         tiles;
	bool                             swap_bgra16 = false;
	bool                             valid       = false;

	[[nodiscard]] uint64_t LinearSize() const {
		uint64_t size = 0;
		for (const auto& tile: tiles) {
			size = std::max(size, tile.linear_offset + tile.linear_size);
		}
		return size;
	}
};

struct TextureCache::ImageDownload {
	TextureTransfer texture;
	bool                depth_target = false;
	bool                valid        = false;
};

TextureCache::TextureCache(GraphicContext& graphics, CommandScheduler& scheduler,
                           PageManager& page_manager, BufferCache& buffer_cache)
    : m_graphics(graphics), m_scheduler(scheduler), m_page_manager(page_manager),
      m_blit_helper(graphics, scheduler),
      m_tiler(graphics, scheduler, buffer_cache.GetUtilityBuffer(MemoryUsage::Stream)),
      m_buffer_cache(buffer_cache),
      m_readback_linear_images(Config::ReadbackLinearImagesEnabled()) {
	m_image_page_counts = std::make_unique<std::atomic<uint32_t>[]>(
	    size_t {1} << (ImagePageTable::kAddressSpaceBits - ImagePageTable::kPageBits));
	m_fault_fast_path = EnvNotZero("KYTY_TEXTURE_FAULT_FAST_PATH");
	m_texel_sync_skip = EnvNotZero("KYTY_TEXEL_SYNC_SKIP");
	if (m_graphics.CanReportMemoryUsage()) {
		constexpr int64_t GiB = 1024ll * 1024 * 1024;
		const auto        budget =
		    static_cast<int64_t>(std::min<uint64_t>(m_graphics.GetTotalMemoryBudget(), INT64_MAX));
		const auto threshold = std::min<int64_t>(budget, 8 * GiB);
		m_pressure_gc_memory = static_cast<uint64_t>(
		    std::max<int64_t>(std::min(budget - 6 * threshold / 10, budget - GiB), GiB + GiB / 2));
		m_critical_gc_memory = static_cast<uint64_t>(
		    std::max<int64_t>(std::min(budget - 2 * threshold / 10, budget - GiB / 2), 3 * GiB));
		m_trigger_gc_memory = static_cast<uint64_t>(std::max<int64_t>((budget - threshold) / 2, 0));
	}
	const auto* lookup = std::getenv("KYTY_IMAGE_LOOKUP");
	if (lookup != nullptr && std::strcmp(lookup, "legacy") == 0) {
		m_image_lookup_mode = ImageLookupMode::Legacy;
		LOGF("Image lookup: legacy full-region search\n");
	} else if (lookup != nullptr && std::strcmp(lookup, "verify") == 0) {
		m_image_lookup_mode = ImageLookupMode::Verify;
		LOGF("Image lookup: cross-check first-page search, use legacy results\n");
	} else {
		LOGF("Image lookup: first-page exact search\n");
	}
	const auto* dirty_query = std::getenv("KYTY_DIRTY_IMAGE_QUERY");
	m_direct_dirty_image_query = dirty_query != nullptr && std::strcmp(dirty_query, "direct") == 0;
	const auto* clean_proofs = std::getenv("KYTY_TEXTURE_CLEAN_PROOFS");
	m_clean_image_proofs = clean_proofs != nullptr && std::strcmp(clean_proofs, "1") == 0;
	if (m_direct_dirty_image_query) {
		LOGF("Dirty image query: direct page-owner search\n");
	}
	const auto* gpu_dcc = std::getenv("KYTY_DCC_GPU");
	if (gpu_dcc != nullptr && std::strcmp(gpu_dcc, "1") == 0) {
		m_dcc_clear = std::make_unique<DccClearHelper>(graphics, scheduler);
		LOGF("DCC materialization: GPU validation and conditional clear (%s)\n",
		     m_dcc_clear->Available() ? "any 8/16/32/64/128-bit color view format"
		                              : "unavailable on this device");
	}
	const auto* policy = std::getenv("KYTY_IMAGE_CACHE_POLICY");
	m_pressure_gc_enabled = policy != nullptr && std::strcmp(policy, "pressure") == 0;
	if (m_pressure_gc_enabled) {
		m_pressure_gc_policy.SetBudget(m_graphics.GetTotalMemoryBudget());
		LOGF("Image cache policy: pressure (Approach B), low=%" PRIu64
		     " MiB high=%" PRIu64 " MiB critical=%" PRIu64 " MiB\n",
		     m_pressure_gc_policy.Low() / ImageCacheGcPolicy::MiB,
		     m_pressure_gc_policy.High() / ImageCacheGcPolicy::MiB,
		     m_pressure_gc_policy.Critical() / ImageCacheGcPolicy::MiB);
	} else {
		LOGF("Image cache policy: submission age (Approach A)\n");
	}
	// Texture streaming. KYTY_TEXTURE_PARTIAL_UPLOAD=0 restores whole-image CPU write tracking
	// and whole-image refreshes. KYTY_TEXTURE_DIRTY_CHUNK_KB (power of two, 4..4096, default 64)
	// is the write-tracking granularity. KYTY_TEXTURE_PARTIAL_BANDS=0 refreshes whole mip
	// levels instead of rows of tile blocks. KYTY_TEXTURE_PARTIAL_VERIFY=1 hashes every clean
	// chunk before a partial refresh and falls back to a full one on a mismatch (diagnostic).
	m_partial_upload = EnvNotZero("KYTY_TEXTURE_PARTIAL_UPLOAD");
	m_partial_bands  = EnvNotZero("KYTY_TEXTURE_PARTIAL_BANDS");
	if (const auto* verify = std::getenv("KYTY_TEXTURE_PARTIAL_VERIFY");
	    verify != nullptr && std::strcmp(verify, "1") == 0) {
		m_partial_verify = true;
	}
	if (const auto* chunk = std::getenv("KYTY_TEXTURE_DIRTY_CHUNK_KB"); chunk != nullptr) {
		const auto kib = std::strtoull(chunk, nullptr, 10);
		if (kib >= 4 && kib <= 4096 && std::has_single_bit(kib)) {
			m_chunk_shift = static_cast<uint32_t>(std::countr_zero(kib)) + 10u;
		} else {
			LOGF("KYTY_TEXTURE_DIRTY_CHUNK_KB=%s ignored (power of two 4..4096 expected)\n", chunk);
		}
	}
	// KYTY_TEXTURE_OVERLAP_KEEP_FRAMES=N (default 0: off) keeps a clean, texture-only image that a
	// newer overlapping image would retire as stale, while it was used in the last N frames.
	if (const auto* keep = std::getenv("KYTY_TEXTURE_OVERLAP_KEEP_FRAMES"); keep != nullptr) {
		m_overlap_keep_frames = std::strtoull(keep, nullptr, 10);
	}
	LOGF("Texture streaming: partial_upload=%u chunk=%u KiB bands=%u verify=%u overlap_keep=%" PRIu64
	     " frames\n",
	     m_partial_upload ? 1u : 0u, (1u << m_chunk_shift) >> 10u, m_partial_bands ? 1u : 0u,
	     m_partial_verify ? 1u : 0u, m_overlap_keep_frames);
	// KYTY_TEXTURE_RESIDENT_MIPS=0 registers, watches and uploads every level of every texture.
	// By default a sampled texture holds only the levels its views can sample (finer than the
	// T# MIN_LOD / BASE_LEVEL clamp is never read); =poison also fills the other levels with a
	// visible marker to check that claim on screen.
	if (const auto* residency = std::getenv("KYTY_TEXTURE_RESIDENT_MIPS"); residency != nullptr) {
		if (std::strcmp(residency, "0") == 0) {
			m_residency = ResidencyMode::Off;
		} else if (std::strcmp(residency, "poison") == 0) {
			m_residency = ResidencyMode::Poison;
		}
	}
	if (const auto* idle = std::getenv("KYTY_TEXTURE_RESIDENT_IDLE_FRAMES"); idle != nullptr) {
		m_resident_idle_frames = std::strtoull(idle, nullptr, 10);
	}
	LOGF("Texture streaming: resident mip levels %s, idle partially resident images retired "
	     "after %" PRIu64 " frames\n",
	     m_residency == ResidencyMode::Off      ? "off (whole chains)"
	     : m_residency == ResidencyMode::Poison ? "on, non-resident levels poisoned"
	                                            : "on",
	     m_resident_idle_frames);
	// KYTY_TEXTURE_ASYNC_STAGING=0 keeps texture refresh staging copies on the GPU thread.
	if (EnvNotZero("KYTY_TEXTURE_ASYNC_STAGING")) {
		m_staging_copier = std::make_unique<StagingCopier>(graphics);
		m_scheduler.SetSubmitDependency(m_staging_copier.get());
		// With resizable BAR the worker writes straight into VRAM. Only when a device-local
		// host-visible heap is large, so the ring cannot starve other users of a 256 MiB BAR.
		const auto& memory = graphics.GetPhysicalDeviceMemoryProperties();
		uint64_t    bar_heap = 0;
		for (uint32_t index = 0; index < memory.memoryTypeCount; index++) {
			const auto flags = memory.memoryTypes[index].propertyFlags;
			if ((flags & vk::MemoryPropertyFlagBits::eDeviceLocal) &&
			    (flags & vk::MemoryPropertyFlagBits::eHostVisible)) {
				bar_heap = std::max<uint64_t>(bar_heap,
				                              memory.memoryHeaps[memory.memoryTypes[index].heapIndex].size);
			}
		}
		uint64_t ring_mib = 256;
		if (const auto* size = std::getenv("KYTY_TEXTURE_STAGING_MB"); size != nullptr) {
			ring_mib = std::strtoull(size, nullptr, 10);
		}
		constexpr uint64_t MiB = 1024ull * 1024;
		if (EnvNotZero("KYTY_TEXTURE_STAGING_REBAR") && ring_mib != 0 &&
		    bar_heap >= 2048 * MiB && ring_mib * MiB <= bar_heap / 8) {
			m_texture_staging = std::make_unique<StreamBuffer>(graphics, scheduler, MemoryUsage::Stream,
			                                                   ring_mib * MiB);
		}
		LOGF("Texture streaming: async staging copies on, staging ring %s (%" PRIu64
		     " MiB, largest device-local host-visible heap %" PRIu64 " MiB)\n",
		     m_texture_staging ? "device-local" : "shared upload ring",
		     m_texture_staging ? ring_mib : 0, bar_heap / MiB);
	} else {
		LOGF("Texture streaming: async staging copies off\n");
	}
}

StreamBuffer& TextureCache::StagingRing() {
	return m_texture_staging ? *m_texture_staging
	                         : m_buffer_cache.GetUtilityBuffer(MemoryUsage::Upload);
}

TextureCache::~TextureCache() {
	if (m_staging_copier) {
		// The scheduler has drained by now; no later submission may wait on the copier.
		m_scheduler.SetSubmitDependency(nullptr);
	}
	if (m_image_lookup_mode == ImageLookupMode::Verify) {
		LOGF("Image lookup verification final: checks=%" PRIu64 " mismatches=%" PRIu64 "\n",
		     m_image_lookup_checks, m_image_lookup_mismatches);
	}
	m_slot_images.ForEach([&](ImageId id, const Image& image) {
		if (image.registered) {
			UnregisterImage(id);
		}
	});
}

bool TextureCache::SameBacking(const ImageInfo& cached, const ImageInfo& requested,
                               bool exact_format) {
	if (cached.data.address != requested.data.address) {
		return false;
	}
	if (cached.data.size != requested.data.size) {
		return false;
	}
	if (cached.extent != requested.extent) {
		return false;
	}
	if (cached.resources.levels < requested.resources.levels ||
	    cached.resources.layers < requested.resources.layers) {
		return false;
	}
	if (cached.samples != requested.samples) {
		return false;
	}
	if (cached.bytes_per_block != requested.bytes_per_block) {
		return false;
	}
	if (cached.tile_mode != requested.tile_mode) {
		return false;
	}
	if (!ImageViewOps::FormatsCompatible(cached.pixel_format, requested.pixel_format) ||
	    (cached.type != requested.type && requested.extent != vk::Extent3D {1, 1, 1})) {
		return false;
	}
	if (exact_format && cached.pixel_format != requested.pixel_format) {
		return false;
	}
	return true;
}

TextureCache::BindingType TextureCache::UploadBinding(const Image& image) {
	if (image.info.IsDepth()) {
		if (image.info.tile_mode == Prospero::TileMode::kDepth ||
		    image.info.tile_mode == Prospero::TileMode::kLinear) {
			return BindingType::DepthTarget;
		}
		return BindingType::Texture;
	}
	if (image.usage.render_target) {
		return BindingType::RenderTarget;
	}
	if (image.usage.video_out) {
		return BindingType::VideoOut;
	}
	return image.usage.storage ? BindingType::Storage : BindingType::Texture;
}

bool TextureCache::SafeToDownload(const Image& image) {
	// Non-resident levels hold undefined native contents and must never reach guest memory.
	// (GPU-written images are always fully resident; this only keeps downloads exact.)
	if (!image.SafeToDownload() || !image.FullyResident()) {
		return false;
	}
	const auto range = image.info.data;
	return !m_buffer_cache.HasGpuDirtyBytes(range.address, range.size);
}

ImageId TextureCache::InsertImage(const ImageInfo& info, uint32_t resident_first,
                                  uint64_t resident_prefix) {
	const auto id = m_slot_images.insert(m_graphics, m_scheduler, info);
	if (resident_first != 0) {
		if (resident_prefix == 0) {
			resident_prefix = ResidentPrefixSize(info, resident_first);
		}
		if (resident_prefix != 0) {
			auto& image          = m_slot_images[id];
			image.resident_first = resident_first;
			image.live           = {info.data.address, resident_prefix};
			if (m_resident_idle_frames != 0) {
				m_partial_images.push_back(id);
			}
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureResidentImages);
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureResidentLevelsSkipped,
			                          resident_first);
		}
	}
	if (!info.data.Empty()) {
		RegisterImage(id);
	}
	return id;
}

uint32_t TextureCache::RequestedFirstLevel(const ImageDesc& desc, uint32_t levels) const {
	if (m_residency == ResidencyMode::Off || desc.type != BindingType::Texture || levels <= 1) {
		return 0;
	}
	// The finest level a sampled view can read. Its minimum LOD (the T# MIN_LOD clamp, applied
	// through VK_EXT_image_view_min_lod) bounds every level-of-detail computation: trilinear
	// filtering at the clamp reads floor(min_lod) and the next coarser level, nearest-mip
	// selection rounds only towards coarser levels, and texel fetches of finer levels are out
	// of range for the view. Levels below base_level are outside the view altogether.
	const auto& view  = desc.view_info;
	const auto  first = view.base_level + (view.min_lod >> 8u);
	return std::min(first, levels - 1u);
}

uint64_t TextureCache::ResidentPrefixSize(const ImageInfo& info, uint32_t first_level) const {
	if (first_level == 0 || m_residency == ResidencyMode::Off || info.data.Empty() ||
	    info.type != Prospero::ImageType::kColor2D || info.resources.layers != 1 ||
	    info.samples != 1 || info.resources.levels <= first_level || info.IsDepth() ||
	    info.HasStencil() || info.HasMetadata() ||
	    info.metadata.compression != VideoOutCompression::Uncompressed ||
	    info.pixel_format == vk::Format::eUndefined) {
		return 0;
	}
	// The guest bytes of levels [first_level, levels), from the same layout the refresh detiles
	// (PS5 stores the chain smallest level first: the mip tail block, then coarser to finer).
	const auto transfer =
	    BuildTextureTransfer(info, 1, BindingType::Texture, TransferDirection::Upload);
	if (!transfer.valid) {
		return 0;
	}
	uint64_t begin = UINT64_MAX;
	uint64_t end   = 0;
	if (!transfer.tiles.empty()) {
		if (transfer.tiles.size() != transfer.regions.size()) {
			return 0;
		}
		for (size_t index = 0; index < transfer.tiles.size(); ++index) {
			if (transfer.regions[index].imageSubresource.mipLevel < first_level) {
				continue;
			}
			const auto& tile = transfer.tiles[index];
			begin            = std::min(begin, tile.tiled_offset);
			end              = std::max(end, tile.tiled_offset + tile.tiled_size);
		}
	} else {
		for (uint32_t level = first_level; level < info.resources.levels; ++level) {
			const auto& mip = transfer.layout.mips[level];
			begin           = std::min(begin, mip.offset);
			end             = std::max(end, mip.offset + mip.size);
		}
	}
	// Only a proper prefix: it keeps the first page (exact-backing lookups start there) and
	// leaves every non-resident byte after it.
	if (begin != 0 || end == 0 || end >= info.data.size) {
		return 0;
	}
	return end;
}

void TextureCache::EnsureResidency(ImageId id, uint32_t first_level, bool sampling) {
	auto& image = m_slot_images[id];
	if (first_level >= image.resident_first) {
		return;
	}
	const auto prefix    = ResidentPrefixSize(image.info, first_level);
	const auto new_first = prefix != 0 ? first_level : 0u;
	// The registered range grows. For everything keyed on the page owner index (clean-page
	// proofs, lookups, binding identity caches) this is an unregister and a register.
	const bool registered = image.registered;
	if (registered) {
		UnregisterImage(id);
	}
	image.resident_first = new_first;
	image.live = new_first != 0 ? GuestRange {image.info.data.address, prefix} : image.info.data;
	if (registered) {
		RegisterImage(id);
	}
	// Newly resident levels hold undefined contents: refresh every resident level before use
	// (from the GPU-written buffer bytes when there are any, as for a new image).
	image.MarkResidencyDirty();
	image.residency_refresh = true;
	if (m_buffer_cache.HasGpuDirtyBytes(image.live.address, image.live.size)) {
		image.MarkBufferModified();
	}
	image.residency_poisoned = image.residency_poisoned && new_first != 0;
	Profiler::CountFrameEvent(sampling ? Profiler::FrameEvent::TextureResidencyExtensions
	                                   : Profiler::FrameEvent::TextureResidencyFullFallbacks);
}

void TextureCache::RetireIdlePartialImages() {
	// A partially resident image overlaps no neighbouring streamed texture, so the overlap
	// rule that used to retire unused ones never fires: its full native mip chain would stay
	// allocated until a same-slot replacement or memory pressure. Retire one unused for
	// KYTY_TEXTURE_RESIDENT_IDLE_FRAMES presented frames instead; a later use recreates it and
	// uploads only its resident levels. Once per frame, over partially resident images only.
	const auto frame = m_frame.load(std::memory_order_relaxed);
	if (m_resident_idle_frames == 0 || m_partial_images.empty() || frame == m_partial_scan_frame) {
		return;
	}
	m_partial_scan_frame = frame;
	const auto tick      = m_scheduler.CurrentTick();
	size_t     kept      = 0;
	for (size_t index = 0; index < m_partial_images.size(); ++index) {
		const auto id    = m_partial_images[index];
		auto*      image = m_slot_images.try_get(id);
		if (image == nullptr || !image->registered || image->FullyResident()) {
			continue;
		}
		const bool idle =
		    frame - std::min(frame, image->frame_accessed_last) > m_resident_idle_frames &&
		    tick - std::min(tick, image->tick_accessed_last) > NumFramesBeforeRemoval &&
		    !image->binding.is_bound && !image->binding.is_target && !image->IsGpuModified();
		if (idle) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureResidentIdleFrees);
			FreeImage(id, HangTrace::ImageFreeReason::ResidentIdle);
			continue;
		}
		m_partial_images[kept++] = id;
	}
	m_partial_images.resize(kept);
}

void TextureCache::RequireFullResidency(ImageId id) {
	if (m_slot_images[id].FullyResident()) {
		return;
	}
	// A use other than sampling through a clamped view (render or storage target, copy,
	// clear, download, presentation): the whole chain is live from now on. Refresh it before
	// the caller records anything that reads or partially writes it.
	EnsureResidency(id, 0, false);
	RefreshImage(id);
}

void TextureCache::RestrictToResidentLevels(const Image& image, TextureTransfer& transfer) const {
	if (image.FullyResident()) {
		return;
	}
	const auto first = image.resident_first;
	const bool tiled = !transfer.tiles.empty();
	EXIT_IF(tiled && transfer.tiles.size() != transfer.regions.size());
	size_t kept = 0;
	for (size_t index = 0; index < transfer.regions.size(); ++index) {
		if (transfer.regions[index].imageSubresource.mipLevel < first) {
			continue;
		}
		transfer.regions[kept] = transfer.regions[index];
		if (tiled) {
			transfer.tiles[kept] = transfer.tiles[index];
		}
		kept++;
	}
	transfer.regions.resize(kept);
	if (tiled) {
		transfer.tiles.resize(kept);
		// The detiled scratch holds only the resident levels.
		uint64_t base = UINT64_MAX;
		for (const auto& tile: transfer.tiles) {
			base = std::min(base, tile.linear_offset);
		}
		for (size_t index = 0; index < kept; ++index) {
			transfer.tiles[index].linear_offset -= base;
			transfer.regions[index].bufferOffset -= base;
		}
	}
}

void TextureCache::PoisonNonResidentLevels(Image& image) {
	// KYTY_TEXTURE_RESIDENT_MIPS=poison: fill the levels no view may sample with a marker
	// (magenta/green texels) so any read of a non-resident level shows on screen.
	auto transfer = BuildTextureTransfer(image, BindingType::Texture, TransferDirection::Upload);
	if (!transfer.valid) {
		return;
	}
	std::vector<vk::BufferImageCopy> regions;
	uint64_t                         bytes = 0;
	const auto element = std::max(image.info.bytes_per_block, 1u);
	const auto texel   = image.info.IsBlock() ? 4u : 1u;
	for (auto region: transfer.regions) {
		if (region.imageSubresource.mipLevel >= image.resident_first) {
			continue;
		}
		region.bufferOffset      = 0;
		region.bufferRowLength   = 0;
		region.bufferImageHeight = 0;
		const uint64_t size = static_cast<uint64_t>((region.imageExtent.width + texel - 1) / texel) *
		                      ((region.imageExtent.height + texel - 1) / texel) * element;
		bytes = std::max(bytes, size);
		regions.push_back(region);
	}
	if (regions.empty() || bytes == 0) {
		return;
	}
	const auto scratch = m_tiler.GetScratchBuffer(bytes);
	m_scheduler.EndRendering();
	m_scheduler.Current().Handle().fillBuffer(scratch.buffer, 0, Common::AlignUp(bytes, 4),
	                                          0xF81FF81Fu);
	image.Upload(regions, scratch.buffer, 0, scratch.size);
	image.residency_poisoned = true;
}

void TextureCache::RegisterImage(ImageId id) {
	auto& image = m_slot_images[id];
	if (image.registered || image.info.data.Empty()) {
		EXIT("TextureCache: invalid image registration\n");
	}
	ImagePageTable::PageRange pages {};
	if (!ImagePageTable::TryGetPageRange(image.live.address, image.live.size, pages)) {
		EXIT("TextureCache: image registration is outside the guest address space\n");
	}
	InvalidateCleanImageProofs(image.live.address, image.live.size,
	                           Coherence::Source::ImageRegister);
	NoteStructureChange(image);
	ForEachPage(image.live.address, image.live.size, [this, id](uint64_t page) {
		m_image_page_table[page].push_back(id);
		// Before the image can watch pages (TrackImage follows registration).
		m_image_page_counts[page].fetch_add(1);
	});
	image.registered = true;
	image.lru_id     = m_lru_cache.Insert(id, m_gc_tick);
	m_total_used_memory += image.AccountedSize();
	m_registered_image_memory += image.AccountedSize();
	// Registration precedes the first TrackImage, so the tracking mode never changes while
	// the image watches pages. Re-registration (a residency change) re-derives it.
	if (!image.IsTracked()) {
		if (m_partial_upload && ChunkTrackingEligible(image)) {
			image.EnableChunkTracking(m_chunk_shift);
		} else {
			image.chunks = {};
		}
	}
}

bool TextureCache::ChunkTrackingEligible(const Image& image) const {
	const auto& info = image.info;
	// Page-aligned ranges only: then every write fault on one of the image's pages overlaps its
	// bytes, and no same-page neighbour write can untrack a page without dirtying it.
	return info.pixel_format != vk::Format::eUndefined && image.backing.image != nullptr &&
	       !info.IsDepth() && !info.HasStencil() && !info.HasMetadata() && info.samples == 1 &&
	       info.metadata.compression == VideoOutCompression::Uncompressed &&
	       image.live.address % TRACKER_PAGE_SIZE == 0 &&
	       image.live.size % TRACKER_PAGE_SIZE == 0 &&
	       image.live.size > (uint64_t {1} << m_chunk_shift);
}

void TextureCache::UnregisterImage(ImageId id) {
	auto& image = m_slot_images[id];
	if (!image.registered) {
		return;
	}
	InvalidateCleanImageProofs(image.live.address, image.live.size,
	                           Coherence::Source::ImageUnregister);
	NoteStructureChange(image);
	UntrackImage(id);
	ImagePageTable::PageRange pages {};
	if (!ImagePageTable::TryGetPageRange(image.live.address, image.live.size, pages)) {
		EXIT("TextureCache: registered image is outside the guest address space\n");
	}
	ForEachPage(image.live.address, image.live.size, [this, id](uint64_t page) {
		auto* owners = m_image_page_table.Find(page);
		if (owners == nullptr || !owners->Erase(id)) {
			EXIT("TextureCache: image missing from page owner index\n");
		}
		// After UntrackImage above: the image no longer watches these pages.
		m_image_page_counts[page].fetch_sub(1);
	});
	m_lru_cache.Free(image.lru_id);
	const auto accounted = image.AccountedSize();
	if (accounted > m_total_used_memory) {
		EXIT("TextureCache: image accounting underflow\n");
	}
	m_total_used_memory -= accounted;
	EXIT_IF(accounted > m_registered_image_memory);
	m_registered_image_memory -= accounted;
	image.registered = false;
}

void TextureCache::DeleteImage(ImageId id) {
	auto* image = m_slot_images.try_get(id);
	if (image == nullptr || !image->registered) {
		return;
	}
	if (HangTrace::Enabled()) {
		const auto tick = m_scheduler.CurrentTick();
		HangTrace::RecordImageFree(image->info.data.address, image->info.extent.width,
		                           image->info.extent.height, image->info.resources.levels,
		                           image->info.resources.layers,
		                           static_cast<uint32_t>(image->backing.format), image->info.data.size,
		                           tick - std::min(tick, image->tick_accessed_last));
		HangTrace::SetImageFreeReason(HangTrace::ImageFreeReason::Other);
	}
	// Stencil associations only ever point at depth images (AssociateStencil requires one, and
	// an image's pixel format never changes), so retiring a color image skips the full scan.
	if (!image->depth_id && image->info.IsDepth()) {
		std::vector<ImageId> associations;
		m_slot_images.ForEach([&](ImageId candidate, const Image& associated) {
			if (associated.depth_id == id) {
				associations.push_back(candidate);
			}
		});
		for (const auto association: associations) {
			FreeImage(association, HangTrace::ImageFreeReason::DepthAssociation);
		}
	}
	if (image->IsGpuModified()) {
		EXIT("TextureCache: deleting a GPU-modified image without resolving its contents\n");
	}
	m_download_images.erase(id);
	if (image->info.HasMetadata()) {
		const auto metadata = m_surface_metas.find(image->info.metadata.range.address);
		if (metadata != m_surface_metas.end() &&
		    image->info.metadata.kind == ImageMetadataKind::Htile &&
		    metadata->second.type == MetaDataInfo::Type::HTile) {
			// A later binding may have reused this address for another metadata type.
			m_surface_metas.erase(metadata);
		}
	}
	UnregisterImage(id);
	if (m_scheduler.Active()) {
		uint64_t retiring_bytes = 0;
		if (m_pressure_gc_enabled && image->backing.allocation != nullptr) {
			VmaAllocationInfo allocation {};
			vmaGetAllocationInfo(m_graphics.allocator, image->backing.allocation, &allocation);
			retiring_bytes = allocation.size;
			m_pressure_retirement_bytes += retiring_bytes;
		}
		m_scheduler.DeferOperation([this, id, retiring_bytes] {
			m_slot_images.erase(id);
			EXIT_IF(retiring_bytes > m_pressure_retirement_bytes);
			m_pressure_retirement_bytes -= retiring_bytes;
		});
	} else {
		m_slot_images.erase(id);
	}
}

void TextureCache::FreeImage(ImageId id, HangTrace::ImageFreeReason reason) {
	HangTrace::SetImageFreeReason(reason);
	auto& image = m_slot_images[id];
	if (image.IsGpuModified()) {
		CleanVerdict::Invalidate(image.live.address, image.live.size,
		                         Coherence::Source::ImageGpuClear);
		image.ClearGpuModified();
	}
	DeleteImage(id);
}

void TextureCache::TouchImage(Image& image) {
	image.frame_accessed_last = m_frame.load(std::memory_order_relaxed);
	if (image.registered) {
		m_lru_cache.Touch(image.lru_id, m_gc_tick);
	}
}

void TextureCache::MarkAsMaybeDirty(ImageId id, Image& image) {
	image.MarkMaybeCpuDirty();
	if (image.NeedsMaybeCpuHash()) {
		image.SetMaybeCpuHash(image.HashGuestEdges());
	}
	UntrackImage(id);
}

void TextureCache::TrackImage(ImageId id) {
	auto& image = m_slot_images[id];
	if (!image.registered) {
		return;
	}
	if (image.ChunkTracked()) {
		TrackChunkImage(image);
		return;
	}
	const auto image_begin = image.live.address;
	const auto image_end   = image.live.End();
	if (image_begin == image.track_addr && image_end == image.track_addr_end) {
		return;
	}
	if (!image.IsTracked()) {
		image.track_addr     = image_begin;
		image.track_addr_end = image_end;
		m_page_manager.UpdatePageWatchers<true>(image_begin, image.live.size);
		return;
	}
	if (image_begin < image.track_addr) {
		TrackImageHead(id);
	}
	if (image.track_addr_end < image_end) {
		TrackImageTail(id);
	}
}

void TextureCache::TrackImageHead(ImageId id) {
	auto& image = m_slot_images[id];
	if (!image.registered) {
		return;
	}
	const auto image_begin = image.live.address;
	if (image_begin == image.track_addr) {
		return;
	}
	if (!image.IsTracked() || image_begin > image.track_addr) {
		EXIT("TextureCache: invalid image head tracking range\n");
	}
	const auto size  = image.track_addr - image_begin;
	image.track_addr = image_begin;
	m_page_manager.UpdatePageWatchers<true>(image_begin, size);
}

void TextureCache::TrackImageTail(ImageId id) {
	auto& image = m_slot_images[id];
	if (!image.registered) {
		return;
	}
	const auto image_end = image.live.End();
	if (image_end == image.track_addr_end) {
		return;
	}
	if (!image.IsTracked() || image.track_addr_end > image_end) {
		EXIT("TextureCache: invalid image tail tracking range\n");
	}
	const auto address   = image.track_addr_end;
	const auto size      = image_end - address;
	image.track_addr_end = image_end;
	m_page_manager.UpdatePageWatchers<true>(address, size);
}

void TextureCache::TrackChunkImage(Image& image) {
	if (!image.IsTracked()) {
		// Nothing is watched: the untracked marks describe holes of a previous tracked period.
		std::fill(image.chunks.untracked.begin(), image.chunks.untracked.end(), 0);
		image.chunks.untracked_count = 0;
		image.track_addr     = image.live.address;
		image.track_addr_end = image.live.End();
		m_page_manager.UpdatePageWatchers<true>(image.live.address, image.live.size);
		return;
	}
	// Watch again every chunk a CPU write released. Their dirty marks stay until a refresh:
	// a write after this point faults and is recorded before it reaches memory. (Runs on every
	// bind; nothing to scan while all chunks are watched.)
	if (image.chunks.untracked_count != 0) {
		(void)UpdateChunkWatchers<true>(image, 0, image.chunks.count);
	}
}

template <bool track>
uint32_t TextureCache::UpdateChunkWatchers(Image& image, uint32_t first, uint32_t last) {
	auto&          chunks     = image.chunks;
	if (track ? chunks.untracked_count == 0 : chunks.untracked_count == chunks.count) {
		return 0;
	}
	const uint64_t chunk_size = uint64_t {1} << chunks.shift;
	const uint64_t begin      = image.live.address; // page aligned (ChunkTrackingEligible)
	const uint64_t end        = image.live.End();
	uint32_t       changed    = 0;
	uint32_t       run_first  = UINT32_MAX;
	const auto     flush      = [&](uint32_t run_last) {
		if (run_first == UINT32_MAX) {
			return;
		}
		const auto run_begin = std::max(begin, chunks.base + uint64_t {run_first} * chunk_size);
		const auto run_end   = std::min(end, chunks.base + uint64_t {run_last} * chunk_size);
		if (run_begin < run_end) {
			m_page_manager.UpdatePageWatchers<track>(run_begin, run_end - run_begin);
		}
		run_first = UINT32_MAX;
	};
	for (uint32_t index = first; index < last; index++) {
		// track: re-watch released chunks; untrack: release still-watched chunks.
		if (Image::ChunkBit(chunks.untracked, index) == track) {
			if constexpr (track) {
				Image::ClearChunkBit(chunks.untracked, index);
			} else {
				Image::SetChunkBit(chunks.untracked, index);
			}
			if (run_first == UINT32_MAX) {
				run_first = index;
			}
			changed++;
		} else {
			flush(index);
		}
	}
	flush(last);
	if constexpr (track) {
		chunks.untracked_count -= changed;
	} else {
		chunks.untracked_count += changed;
	}
	return changed;
}

void TextureCache::InvalidateChunks(Image& image, uint64_t address, uint64_t size) {
	const auto begin = std::max(Common::AlignDown(address, TRACKER_PAGE_SIZE), image.live.address);
	const auto end   = std::min(Common::AlignUp(address + size, TRACKER_PAGE_SIZE), image.live.End());
	if (begin >= end) {
		return;
	}
	auto& chunks = image.chunks;
	// Whole rewrites (see ChunkState::full_streak) release everything on the first write.
	constexpr uint32_t WholeRewriteStreak = 2;
	const bool         whole = chunks.full_streak >= WholeRewriteStreak;
	const uint32_t     first = whole ? 0u : static_cast<uint32_t>((begin - chunks.base) >> chunks.shift);
	const uint32_t     last  = whole ? chunks.count
	                                 : static_cast<uint32_t>(((end - 1 - chunks.base) >> chunks.shift) + 1);
	chunks.whole_released |= whole;
	uint32_t newly = 0;
	for (uint32_t index = first; index < last; index++) {
		newly += image.MarkChunkDirty(index) ? 1u : 0u;
	}
	if (newly != 0) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureChunkInvalidations, newly);
	}
	// Release only the written chunks: writes elsewhere in the image keep faulting and are
	// recorded chunk by chunk, so a refresh knows every byte that may have changed.
	if (image.IsTracked()) {
		(void)UpdateChunkWatchers<false>(image, first, last);
	}
	image.NoteChunkWrite(address, size);
}

void TextureCache::UntrackImage(ImageId id) {
	auto& image = m_slot_images[id];
	if (!image.IsTracked()) {
		return;
	}
	if (image.ChunkTracked()) {
		(void)UpdateChunkWatchers<false>(image, 0, image.chunks.count);
		std::fill(image.chunks.untracked.begin(), image.chunks.untracked.end(), 0);
		image.chunks.untracked_count = 0;
		image.track_addr     = 0;
		image.track_addr_end = 0;
		// Writes are no longer observed: only a full refresh can restore partial validity.
		image.SetPartialValid(false);
		return;
	}
	const auto address   = image.track_addr;
	const auto size      = image.track_addr_end - image.track_addr;
	image.track_addr     = 0;
	image.track_addr_end = 0;
	if (size != 0) {
		m_page_manager.UpdatePageWatchers<false>(address, size);
	}
}

void TextureCache::UntrackImageHead(ImageId id) {
	auto&      image = m_slot_images[id];
	const auto begin = image.live.address;
	if (!image.IsTracked() || begin < image.track_addr) {
		return;
	}
	const auto address = Common::AlignDown(begin + TRACKER_PAGE_SIZE, TRACKER_PAGE_SIZE);
	const auto size    = address - begin;
	image.track_addr   = address;
	if (image.track_addr == image.track_addr_end) {
		MarkAsMaybeDirty(id, image);
	}
	if (size != 0) {
		m_page_manager.UpdatePageWatchers<false>(begin, size);
	}
}

void TextureCache::UntrackImageTail(ImageId id) {
	auto&      image = m_slot_images[id];
	const auto end   = image.live.End();
	if (!image.IsTracked() || image.track_addr_end < end) {
		return;
	}
	const auto address   = Common::AlignDown(end, TRACKER_PAGE_SIZE);
	const auto size      = end - address;
	image.track_addr_end = address;
	if (image.track_addr == image.track_addr_end) {
		MarkAsMaybeDirty(id, image);
	}
	if (size != 0) {
		m_page_manager.UpdatePageWatchers<false>(address, size);
	}
}

void TextureCache::TrackImageDownload(ImageId id, Image& image) {
	if (m_readback_linear_images && !image.info.IsTiled() && !image.info.data.Empty()) {
		if (!image.IsGpuModified()) {
			EXIT("TextureCache: cannot enroll a non-GPU-owned image for download\n");
		}
		m_download_images.insert(id);
	}
}

TextureCache::ImageIds TextureCache::FindImagesInRegion(uint64_t address, uint64_t size,
                                                        bool page_overlap) const {
	ImagePageTable::PageRange pages {};
	if (!ImagePageTable::TryGetPageRange(address, size, pages)) {
		return {};
	}

	uint32_t query_epoch = ++m_image_query_epoch;
	if (query_epoch == 0) {
		m_slot_images.ForEach([](ImageId, const Image& image) { image.query_epoch = 0; });
		query_epoch = ++m_image_query_epoch;
	}

	ImageIds result;
	ForEachPage(address, size, [&](uint64_t page) {
		const auto* owners = m_image_page_table.Find(page);
		if (owners == nullptr) {
			return;
		}
		owners->ForEach([&](ImageId id) {
			auto* image = m_slot_images.try_get(id);
			if (image == nullptr) {
				return;
			}
			if (image->query_epoch == query_epoch) {
				return;
			}
			image->query_epoch = query_epoch;
			if (image->Overlaps(address, size, page_overlap)) {
				result.push_back(id);
			}
		});
	});
	return result;
}

ImageId TextureCache::FindImageWithSameBacking(const ImageInfo& requested,
                                               bool exact_format) const {
	ImagePageTable::PageRange pages {};
	if (!ImagePageTable::TryGetPageRange(requested.data.address, requested.data.size, pages)) {
		return {};
	}
	const auto* owners = m_image_page_table.Find(pages.first);
	if (owners == nullptr) {
		return {};
	}
	ImageId result {};
	// SameBacking requires an identical start address, so every possible exact match is
	// already present on the first page. Preserve the last-match ordering used by the
	// full region search without visiting the remaining pages or building a candidate list.
	// Read the live index under m_lock so unregister, alias replacement and reused slot
	// generations cannot leave a cached result pointing at a retired image.
	owners->ForEach([&](ImageId id) {
		const auto* image = m_slot_images.try_get(id);
		if (image != nullptr && image->registered &&
		    SameBacking(image->info, requested, exact_format)) {
			result = id;
		}
	});
	return result;
}

ImageId TextureCache::GetNullImage(const ImageDesc& desc) {
	const auto format = desc.info.pixel_format;
	if (const auto found = m_null_images.find(format); found != m_null_images.end()) {
		return found->second;
	}
	ImageInfo info {};
	info.pixel_format    = desc.info.pixel_format;
	info.guest_format    = desc.info.guest_format;
	info.type            = Prospero::ImageType::kColor2D;
	info.extent          = {1, 1, 1};
	info.resources       = {1, 1};
	info.pitch           = 1;
	info.bytes_per_block = std::max(desc.info.bytes_per_block, 1u);
	info.samples         = 1;
	info.tile_mode       = Prospero::TileMode::kLinear;
	info.mip_layout[0]   = {0, info.bytes_per_block, 1, 1};
	const auto id        = InsertImage(info);
	m_null_images.emplace(format, id);
	return id;
}

void TextureCache::ValidateImageDesc(const ImageDesc& desc) const {
	ImageOps::Validate(desc.info);
	if (desc.view_info.format == vk::Format::eUndefined || desc.view_info.level_count == 0 ||
	    desc.view_info.layer_count == 0 ||
	    desc.view_info.base_level >= desc.info.resources.levels ||
	    desc.view_info.level_count > desc.info.resources.levels - desc.view_info.base_level ||
	    (!desc.info.IsVolume() &&
	     (desc.view_info.base_layer >= desc.info.resources.layers ||
	      desc.view_info.layer_count > desc.info.resources.layers - desc.view_info.base_layer))) {
		EXIT("TextureCache: invalid image view description\n");
	}
	if (desc.type == BindingType::DepthTarget && !IsSupportedDepthTargetFormat(desc.info)) {
		EXIT("TextureCache: unsupported depth image description\n");
	}
	if (desc.type == BindingType::VideoOut && !IsSupportedVideoOutFormat(desc.info)) {
		EXIT("TextureCache: unsupported video-out image description\n");
	}
	if (desc.type == BindingType::VideoOut &&
	    desc.info.metadata.compression == VideoOutCompression::Unsupported) {
		EXIT("TextureCache: unsupported compressed video-out description\n");
	}
}

void TextureCache::PrepareImageCopy(Image& image) {
	if (image.IsCpuDirty()) {
		image.RefreshComplete();
	}
}

void TextureCache::RefreshCopySource(ImageId id) {
	auto& image = m_slot_images[id];
	// A copy reads every level: non-resident levels must hold guest data first.
	RequireFullResidency(id);
	RefreshImage(id);
	if (image.IsDefinitelyCpuDirty()) {
		EXIT("TextureCache: image copy source remained CPU-dirty after refresh\n");
	}
}

bool TextureCache::CopyD16(Image& destination, Image& source) {
	const bool source_depth      = source.info.IsDepth();
	const bool destination_depth = destination.info.IsDepth();
	if (source_depth == destination_depth) {
		return false;
	}
	auto&      depth          = source_depth ? source : destination;
	auto&      color          = source_depth ? destination : source;
	const auto transfer_bytes = DepthAspectTransferBytes(depth.backing.format);
	if (depth.info.bytes_per_block != sizeof(uint16_t) ||
	    color.info.bytes_per_block != sizeof(uint16_t) || transfer_bytes != sizeof(uint32_t)) {
		return false;
	}
	EXIT_IF(source.backing.samples != 1 || destination.backing.samples != 1 ||
	        source.info.resources.levels != 1 || destination.info.resources.levels != 1 ||
	        source.info.extent != destination.info.extent ||
	        source.info.resources.layers != destination.info.resources.layers);

	const auto     layers = depth.info.resources.layers;
	const uint64_t depth_slice =
	    static_cast<uint64_t>(depth.info.pitch) * depth.info.extent.height * transfer_bytes;
	const uint64_t color_slice =
	    static_cast<uint64_t>(color.info.pitch) * color.info.extent.height * sizeof(uint16_t);
	EXIT_IF(layers == 0 || depth_slice > UINT64_MAX / layers || color_slice > UINT64_MAX / layers);
	const auto                       depth_size = depth_slice * layers;
	const auto                       color_size = color_slice * layers;
	std::vector<vk::BufferImageCopy> depth_copies(layers);
	std::vector<vk::BufferImageCopy> color_copies(layers);
	for (uint32_t layer = 0; layer < layers; layer++) {
		depth_copies[layer].bufferOffset      = depth_slice * layer;
		depth_copies[layer].bufferRowLength   = depth.info.pitch;
		depth_copies[layer].bufferImageHeight = depth.info.extent.height;
		depth_copies[layer].imageSubresource  = {vk::ImageAspectFlagBits::eDepth, 0, layer, 1};
		depth_copies[layer].imageExtent       = depth.info.extent;
		color_copies[layer].bufferOffset      = color_slice * layer;
		color_copies[layer].bufferRowLength   = color.info.pitch;
		color_copies[layer].bufferImageHeight = color.info.extent.height;
		color_copies[layer].imageSubresource  = {vk::ImageAspectFlagBits::eColor, 0, layer, 1};
		color_copies[layer].imageExtent       = color.info.extent;
	}

	auto                         depth_buffer = m_tiler.GetScratchBuffer(depth_size);
	auto                         color_buffer = m_tiler.GetScratchBuffer(color_size);
	const TileManager::D16Layout promote_layout {
	    .width               = depth.info.extent.width,
	    .height              = depth.info.extent.height,
	    .layers              = layers,
	    .source_row_stride   = static_cast<uint64_t>(color.info.pitch) * sizeof(uint16_t),
	    .target_row_stride   = static_cast<uint64_t>(depth.info.pitch) * transfer_bytes,
	    .source_slice_stride = color_slice,
	    .target_slice_stride = depth_slice,
	};
	const bool d32 = DepthAspectTransferFormat(depth.backing.format) == vk::Format::eD32Sfloat;
	if (source_depth) {
		source.Download(depth_copies, depth_buffer.buffer, depth_buffer.offset, depth_buffer.size);
		m_tiler.ConvertD16(depth_buffer, color_buffer, TileManager::D16Direction::Demote, d32,
		                   {.width               = promote_layout.width,
		                    .height              = promote_layout.height,
		                    .layers              = promote_layout.layers,
		                    .source_row_stride   = promote_layout.target_row_stride,
		                    .target_row_stride   = promote_layout.source_row_stride,
		                    .source_slice_stride = promote_layout.target_slice_stride,
		                    .target_slice_stride = promote_layout.source_slice_stride});
		destination.Upload(color_copies, color_buffer.buffer, color_buffer.offset,
		                   color_buffer.size);
	} else {
		source.Download(color_copies, color_buffer.buffer, color_buffer.offset, color_buffer.size);
		m_tiler.ConvertD16(color_buffer, depth_buffer, TileManager::D16Direction::Promote, d32,
		                   promote_layout);
		destination.Upload(depth_copies, depth_buffer.buffer, depth_buffer.offset,
		                   depth_buffer.size);
	}
	return true;
}

const char* TextureCache::TryDirectReinterpret(Image& destination, Image& source) {
	if (!DirectImageCopyEnabled() || source.backing.samples != destination.backing.samples) {
		return nullptr;
	}
	const bool source_depth = source.info.IsDepth();
	if (source_depth == destination.info.IsDepth()) {
		return nullptr;
	}
	const auto& depth = source_depth ? source : destination;
	const auto& color = source_depth ? destination : source;
	if (m_graphics.maintenance8_enabled && depth.backing.image_type == vk::ImageType::e2D &&
	    color.backing.image_type == vk::ImageType::e2D &&
	    Maintenance8CopyCompatible(depth.backing.format, color.backing.format)) {
		destination.CopyDepthColorImage(source);
		Profiler::CountFrameEvent(Profiler::FrameEvent::ImageCopyMaintenance8);
		return "maintenance8";
	}
	if (!DirectImageCopyShaderEnabled()) {
		return nullptr;
	}
	if (source_depth && m_blit_helper.SupportsDepthToColor32(source, destination)) {
		m_blit_helper.CopyDepthToColor32(source, destination);
		Profiler::CountFrameEvent(Profiler::FrameEvent::ImageCopyShaderDepthToColor);
		return "shader-depth-to-color";
	}
	if (!source_depth && m_blit_helper.SupportsColor32ToDepth(source, destination)) {
		m_blit_helper.CopyColor32ToDepth(source, destination);
		Profiler::CountFrameEvent(Profiler::FrameEvent::ImageCopyShaderColorToDepth);
		return "shader-color-to-depth";
	}
	return nullptr;
}

bool TextureCache::CopyImage(ImageId destination_id, ImageId source_id, const char* context) {
	RefreshCopySource(source_id);
	// Levels the copy does not write must stay valid, and the result is GPU-owned.
	RequireFullResidency(destination_id);
	auto& destination = m_slot_images[destination_id];
	auto& source      = m_slot_images[source_id];
	TrackImage(destination_id);
	if (source.backing.samples != destination.backing.samples) {
		EXIT("TextureCache: cannot issue an unequal-sample image copy\n");
	}
	PrepareImageCopy(destination);
	if (source.IsBufferModified()) {
		if (source.info.data == destination.info.data) {
			destination.MarkBufferModified();
		}
		HangTrace::RecordTransfer(HangTrace::TransferKind::ImageCopy, "skipped-buffer-owned",
		                          context, destination.info.data.address,
		                          static_cast<uint32_t>(destination.backing.format),
		                          destination.info.extent.width, destination.info.extent.height,
		                          0, 0);
		return false;
	}
	const bool source_depth = source.info.IsDepth();
	const bool dest_depth   = destination.info.IsDepth();
	const bool direct_copy =
	    source.backing.format == destination.backing.format ||
	    (!source_depth && !dest_depth &&
	     vk::blockSize(source.backing.format) == vk::blockSize(destination.backing.format));
	// Lossless paths copy raw texel bits; the D16 path converts through unorm16.
	bool        lossless = true;
	const char* path     = nullptr;
	if (direct_copy) {
		destination.CopyImage(source);
		Profiler::CountFrameEvent(Profiler::FrameEvent::ImageCopyDirect);
		path = "direct";
	} else if ((path = TryDirectReinterpret(destination, source)) != nullptr) {
	} else if (CopyD16(destination, source)) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::ImageCopyD16);
		lossless = false;
		path     = "d16-convert";
	} else {
		if (source.backing.samples != 1 || destination.backing.samples != 1) {
			EXIT("TextureCache: cross-format multisample image copy is unsupported\n");
		}
		auto& copy_buffer = m_buffer_cache.GetUtilityBuffer(MemoryUsage::DeviceLocal);
		destination.CopyImageWithBuffer(source, copy_buffer);
		Profiler::CountFrameEvent(Profiler::FrameEvent::ImageCopyViaBuffer);
		path = "via-buffer";
	}
	HangTrace::RecordTransfer(HangTrace::TransferKind::ImageCopy, path, context,
	                          destination.info.data.address,
	                          static_cast<uint32_t>(destination.backing.format),
	                          destination.info.extent.width, destination.info.extent.height,
	                          destination.info.data.size, 0);
	if (source.IsGpuModified()) {
		MarkImageGpuModified(destination);
	}
	destination.ClearBufferModified();
	// Copies never include a stencil aspect, so combined depth/stencil images never compare equal.
	const auto has_stencil = [](vk::Format format) {
		return format == vk::Format::eD16UnormS8Uint || format == vk::Format::eD24UnormS8Uint ||
		       format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eS8Uint;
	};
	return lossless && !has_stencil(source.backing.format) &&
	       !has_stencil(destination.backing.format) &&
	       source.backing.extent == destination.backing.extent &&
	       source.backing.image_type == destination.backing.image_type &&
	       source.backing.mip_levels == destination.backing.mip_levels &&
	       source.backing.layers == destination.backing.layers;
}

void TextureCache::CopyImageMip(ImageId destination_id, ImageId source_id, uint32_t mip,
                                uint32_t layer) {
	RefreshCopySource(source_id);
	RequireFullResidency(destination_id);
	auto& destination = m_slot_images[destination_id];
	auto& source      = m_slot_images[source_id];
	TrackImage(destination_id);
	if (source.IsBufferModified() || source.backing.samples != destination.backing.samples) {
		EXIT("TextureCache: invalid mip-copy ownership or sample count\n");
	}
	destination.CopyMip(source, mip, layer);
	if (source.IsGpuModified()) {
		MarkImageGpuModified(destination);
	}
}

ImageId TextureCache::ResolveDepthOverlap(const ImageInfo& requested, BindingType binding,
                                          ImageId cached_id) {
	auto& cached = m_slot_images[cached_id];
	if (!cached.info.IsDepth() && !requested.IsDepth()) {
		return {};
	}
	const bool stencil_match = requested.HasStencil() == cached.info.HasStencil();
	const bool bpp_match     = requested.bytes_per_block == cached.info.bytes_per_block;
	// PPSA04264
	const bool raw_d16_texture =
	    binding == BindingType::Texture && cached.info.IsDepth() &&
	    cached.info.guest_format == Prospero::BufferFormat::k16UNorm &&
	    requested.guest_format == Prospero::BufferFormat::k16UInt &&
	    requested.pixel_format == vk::Format::eR16Uint && cached.backing.samples == 1 &&
	    requested.samples == 1 && requested.data == cached.info.data &&
	    requested.extent == cached.info.extent && requested.resources == cached.info.resources &&
	    requested.type == cached.info.type && requested.pitch == cached.info.pitch &&
	    requested.tile_mode == cached.info.tile_mode && !requested.HasStencil() &&
	    !cached.info.HasStencil() && !requested.HasMetadata() && !cached.info.HasMetadata();
	// PPSA04264
	const bool retain_cached_layout =
	    requested.samples == 1 && cached.info.samples == 1 && cached.backing.samples == 1 &&
	    requested.bytes_per_block == cached.info.bytes_per_block &&
	    requested.data.address == cached.info.data.address &&
	    requested.data.size < cached.info.data.size && requested.extent == cached.info.extent &&
	    requested.resources.levels == 1 && cached.info.resources.levels == 1 &&
	    requested.resources.layers != 0 && cached.info.resources.layers != 0 &&
	    requested.resources.layers < cached.info.resources.layers &&
	    requested.type == cached.info.type && requested.pitch == cached.info.pitch &&
	    requested.tile_mode == cached.info.tile_mode && requested.mip_layout[0].offset == 0 &&
	    cached.info.mip_layout[0].offset == 0 &&
	    requested.mip_layout[0].size == requested.data.size &&
	    cached.info.mip_layout[0].size == cached.info.data.size &&
	    requested.data.size % requested.resources.layers == 0 &&
	    cached.info.data.size % cached.info.resources.layers == 0 &&
	    requested.data.size / requested.resources.layers ==
	        cached.info.data.size / cached.info.resources.layers &&
	    !requested.HasStencil() && !cached.info.HasStencil() && !requested.HasMetadata() &&
	    !cached.info.HasMetadata();
	bool recreate = cached.info.resources < requested.resources;
	switch (binding) {
		case BindingType::Texture:
			recreate |= requested.IsDepth() && !cached.info.IsDepth();
			recreate |= raw_d16_texture;
			break;
		case BindingType::Storage: recreate |= cached.info.IsDepth(); break;
		case BindingType::RenderTarget: recreate |= cached.info.IsDepth(); break;
		case BindingType::DepthTarget:
			recreate |= !cached.info.IsDepth();
			recreate |= cached.info.IsDepth() && !(stencil_match && bpp_match);
			break;
		case BindingType::VideoOut: recreate |= cached.info.IsDepth(); break;
	}
	if (!recreate) {
		return cached_id;
	}
	// Guests reuse one allocation as depth and as color (e.g. D32F <-> R32F) every frame. Keep one
	// image per interpretation alive and reuse it: FindImage copies the current owner's contents
	// into a non-owner alias (SyncAliasFromOwner) instead of destroying and recreating images.
	const bool keep_aliases = AliasAgeByFrames() && !(cached.info.resources < requested.resources);
	if (keep_aliases) {
		for (const auto other_id: FindImagesInRegion(requested.data.address, requested.data.size, false)) {
			const auto* other = m_slot_images.try_get(other_id);
			if (other == nullptr || other_id == cached_id || !other->registered || other->depth_id ||
			    other->backing.samples != cached.backing.samples ||
			    !SameBacking(other->info, requested, true)) {
				continue;
			}
			if (cached.binding.is_bound || cached.binding.is_target) {
				cached.binding.needs_rebind = true;
			}
			return other_id;
		}
	}
	RefreshImage(cached_id);
	auto info = requested;
	if (retain_cached_layout) {
		info.data       = cached.info.data;
		info.resources  = cached.info.resources;
		info.mip_layout = cached.info.mip_layout;
	} else {
		info.resources = std::max(requested.resources, cached.info.resources);
	}
	info.htile_clear_mask     = 0;
	const auto replacement_id = InsertImage(info);
	auto&      replacement    = m_slot_images[replacement_id];
	replacement.usage         = cached.usage;
	if (cached.binding.is_bound || cached.binding.is_target) {
		cached.binding.needs_rebind = true;
	}
	if (cached.backing.samples == replacement.backing.samples) {
		const bool copy_supported =
		    cached.backing.samples == 1 || cached.backing.format == replacement.backing.format ||
		    (!cached.info.IsDepth() && !replacement.info.IsDepth() &&
		     ImageViewOps::FormatsCompatible(cached.backing.format, replacement.backing.format));
		if (copy_supported) {
			(void)CopyImage(replacement_id, cached_id, "depth-overlap");
		} else {
			LOGF_COLOR(Log::Color::BrightYellow,
			           "TextureCache: unsupported cross-format multisample depth copy\n");
		}
	} else if (cached.backing.samples == 1 && replacement.backing.samples > 1 &&
	           replacement.info.IsDepth()) {
		RefreshCopySource(cached_id);
		if (cached.IsBufferModified() || cached.IsDefinitelyCpuDirty()) {
			EXIT("TextureCache: multisample depth conversion source is not native-current\n");
		}
		PrepareImageCopy(replacement);
		m_blit_helper.ReinterpretColorAsMsDepth(cached, replacement);
		CommitGpuWrite(replacement);
	} else {
		LOGF_COLOR(Log::Color::BrightYellow,
		           "TextureCache: unsupported unequal-sample depth overlap copy (%u -> %u)\n",
		           cached.backing.samples, replacement.backing.samples);
	}
	if (keep_aliases && cached.backing.samples == replacement.backing.samples &&
	    !cached.info.HasStencil() && !replacement.info.HasStencil()) {
		// The replacement now holds the newest contents; the cached interpretation stays alive
		// as a non-owner alias for the next switch back.
		if (replacement.IsGpuModified()) {
			CommitGpuWrite(replacement);
		}
		return replacement_id;
	}
	FreeImage(cached_id, HangTrace::ImageFreeReason::DepthRecreate);
	return replacement_id;
}

TextureCache::OverlapResult TextureCache::ResolveOverlap(const ImageInfo& requested,
                                                         BindingType binding, ImageId cached_id,
                                                         ImageId merged_id) {
	auto owner = m_slot_images.try_get(cached_id);
	if (owner == nullptr) {
		return {merged_id};
	}
	auto&      cached       = *owner;
	const auto current_tick = m_scheduler.CurrentTick();
	const auto current_frame = m_frame.load(std::memory_order_relaxed);
	const bool safe_to_delete =
	    current_tick - std::min(current_tick, cached.tick_accessed_last) > NumFramesBeforeRemoval &&
	    (!AliasAgeByFrames() || current_frame - std::min(current_frame, cached.frame_accessed_last) >
	                                AliasFramesBeforeRemoval);

	const uint32_t requested_block = requested.bytes_per_block * requested.samples;
	const uint32_t cached_block    = cached.info.bytes_per_block * cached.info.samples;
	if (requested.data.address == cached.info.data.address &&
	    requested.BlockExtent() == cached.info.BlockExtent() && requested_block == cached_block) {
		if (const auto depth_id = ResolveDepthOverlap(requested, binding, cached_id)) {
			return {depth_id};
		}
		if (requested.IsBlock() && !cached.info.IsBlock()) {
			return {ExpandImage(requested, cached_id)};
		}
		if (requested.data.size == cached.info.data.size &&
		    (requested.IsVolume() || cached.info.IsVolume())) {
			return {ExpandImage(requested, cached_id)};
		}
		// Equal pitch does not imply equal mip placement: a changed extent can move
		// a level into or out of the mip tail. These are separate guest layouts.
		if (requested.tile_mode != cached.info.tile_mode ||
		    (requested.resources == cached.info.resources &&
		     requested.mip_layout != cached.info.mip_layout)) {
			if (safe_to_delete) {
				FreeImage(cached_id, HangTrace::ImageFreeReason::OverlapLayout);
			}
			return {merged_id};
		}
		// PPSA08394
		if (requested.data.size == cached.info.data.size &&
		    requested.resources == cached.info.resources && requested.type == cached.info.type &&
		    requested.extent.width > cached.info.extent.width &&
		    requested.extent.height >= cached.info.extent.height &&
		    requested.extent.depth >= cached.info.extent.depth &&
		    ImageViewOps::FormatsCompatible(cached.info.pixel_format, requested.pixel_format)) {
			return {ExpandImage(requested, cached_id)};
		}
		// PS5 mip tails can expose more levels without increasing the guest allocation.
		if (requested.pixel_format == cached.info.pixel_format &&
		    requested.type == cached.info.type && requested.resources > cached.info.resources &&
		    (requested.data.size > cached.info.data.size ||
		     (requested.data.size == cached.info.data.size &&
		      requested.extent == cached.info.extent &&
		      cached.info.resources.levels > 1 &&
		      requested.resources.layers == cached.info.resources.layers))) {
			return {ExpandImage(requested, cached_id)};
		}
		if (requested.pixel_format != cached.info.pixel_format ||
		    requested.data.size <= cached.info.data.size) {
			const auto result_id = merged_id ? merged_id : cached_id;
			const auto result    = m_slot_images.try_get(result_id);
			return {result != nullptr && ImageViewOps::FormatsCompatible(result->info.pixel_format,
			                                                             requested.pixel_format)
			            ? result_id
			            : ImageId {}};
		}
		EXIT("TextureCache: unresolvable equal-address image overlap, address=0x%016" PRIx64
		     " requested=%ux%u "
		     "cached=%ux%u requested_size=0x%016" PRIx64 " cached_size=0x%016" PRIx64
		     " type=%u/%u tile=%u/%u\n",
		     requested.data.address, requested.resources.levels, requested.resources.layers,
		     cached.info.resources.levels, cached.info.resources.layers, requested.data.size,
		     cached.info.data.size, static_cast<uint32_t>(requested.type),
		     static_cast<uint32_t>(cached.info.type), static_cast<uint32_t>(requested.tile_mode),
		     static_cast<uint32_t>(cached.info.tile_mode));
	}

	const int32_t requested_mip = requested.MipOf(cached.info);
	if (requested_mip >= 0) {
		const int32_t layer = requested.SliceOf(cached.info, requested_mip);
		return {cached_id, requested_mip, layer};
	}

	const int32_t mip = cached.info.MipOf(requested);
	if (mip >= 0) {
		const int32_t layer = cached.info.SliceOf(requested, mip);
		if (!merged_id) {
			return {ExpandImage(requested, cached_id)};
		}
		cached.binding.needs_rebind |= cached.binding.is_bound || cached.binding.is_target;
		m_slot_images[merged_id].binding.is_target |= cached.binding.is_target;
		CopyImageMip(merged_id, cached_id, static_cast<uint32_t>(mip),
		             static_cast<uint32_t>(layer));
		FreeImage(cached_id, HangTrace::ImageFreeReason::OverlapMipMerge);
		return {merged_id};
	}
	if (requested.data.address >= cached.info.data.address && safe_to_delete) {
		if (KeepOverlappedImage(cached, current_frame)) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureOverlapKeeps);
		} else {
			FreeImage(cached_id, HangTrace::ImageFreeReason::OverlapStale);
		}
	}
	return {merged_id};
}

bool TextureCache::KeepOverlappedImage(const Image& cached, uint64_t current_frame) const {
	// A partially overlapped sampled texture stays valid: it watches its own pages and a later
	// use refreshes (at most) its written chunks. Freeing it only bounds the number of live
	// aliases; streamed textures that return to their slot are otherwise recreated and fully
	// uploaded. Keep only clean texture-only images used recently, and never in crowded ranges
	// (each live alias adds a page watcher; the tracker allows 127 per page).
	return m_overlap_keep_frames != 0 && !m_overlap_crowded && cached.ChunkTracked() &&
	       !cached.IsGpuModified() && !cached.IsBufferModified() && !cached.depth_id &&
	       !cached.usage.render_target && !cached.usage.depth_target && !cached.usage.storage &&
	       !cached.usage.video_out && !cached.binding.is_target &&
	       current_frame - std::min(current_frame, cached.frame_accessed_last) <=
	           m_overlap_keep_frames;
}

ImageId TextureCache::ExpandImage(const ImageInfo& info, ImageId source_id) {
	RefreshCopySource(source_id);
	const auto expanded_id = InsertImage(info);
	auto&      expanded    = m_slot_images[expanded_id];
	auto&      source      = m_slot_images[source_id];
	expanded.usage         = source.usage;
	if (source.binding.is_bound || source.binding.is_target) {
		source.binding.needs_rebind = true;
	}
	InitializeImage(expanded_id);
	const int32_t mip = source.info.MipOf(info);
	const int32_t layer = source.info.SliceOf(info, mip);
	if (layer >= 0) {
		CopyImageMip(expanded_id, source_id, static_cast<uint32_t>(mip),
		             static_cast<uint32_t>(layer));
	} else {
		(void)CopyImage(expanded_id, source_id, "expand");
	}
	FreeImage(source_id, HangTrace::ImageFreeReason::Expand);
	return expanded_id;
}

TextureCache::TextureTransfer
TextureCache::BuildTextureTransfer(const Image& image, BindingType binding,
                                    TransferDirection direction) const {
	return BuildTextureTransfer(image.info, image.backing.samples, binding, direction);
}

TextureCache::TextureTransfer TextureCache::BuildTextureTransfer(const ImageInfo& info,
                                                                 uint32_t backing_samples,
                                                                 BindingType binding,
                                                                 TransferDirection direction) {
	const bool  upload           = direction == TransferDirection::Upload;
	const bool  render_target    = binding == BindingType::RenderTarget;
	const bool  video_out        = binding == BindingType::VideoOut;
	auto        format           = info.guest_format;
	uint32_t    layers           = info.TransferLayers();
	bool        volume           = info.IsVolume();
	bool        allow_depth_tile = upload;
	const char* owner            = "TextureCache readback";

	TextureTransfer transfer;
	transfer.swap_bgra16 = info.bgra16 && (!upload || render_target || video_out);
	if (render_target) {
		format = ImageOps::RenderTargetTransferFormat(info.bytes_per_block);
	}
	if (video_out) {
		allow_depth_tile = false;
	} else if (render_target || binding == BindingType::Storage) {
		allow_depth_tile = true;
	}
	if (upload) {
		if ((render_target || video_out) &&
		    (info.resources.layers == 0 || info.data.size % info.resources.layers != 0 ||
		     info.samples != 1 || backing_samples != 1)) {
			EXIT("TextureCache: invalid color-attachment upload\n");
		}
		owner = "TextureCache";
		if (render_target) {
			owner = "RenderTarget";
		} else if (binding == BindingType::Storage) {
			owner = "StorageTextureCache";
		} else if (video_out) {
			if (info.metadata.compression != VideoOutCompression::Uncompressed) {
				EXIT("TextureCache: invalid color-attachment upload\n");
			}
			layers = info.resources.layers;
			volume = false;
			owner  = "VideoOut";
		}
	}

	transfer.layout  = TextureCalcUploadLayout(format, info.extent.width, info.extent.height,
	                                       info.resources.levels, layers, info.tile_mode,
	                                       info.data.size, allow_depth_tile, volume, owner);
	transfer.regions = TextureBuildImageCopies(transfer.layout);
	if (info.IsDepth()) {
		for (auto& region: transfer.regions) {
			region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eDepth;
		}
	}
	if (transfer.layout.surface.description.tile_mode != Prospero::TileMode::kLinear) {
		if (!TextureBuildGpuTileInfos(info.data.size, transfer.regions, transfer.layout,
		                              info.resources.levels, transfer.tiles)) {
			return transfer;
		}
	}
	transfer.valid = true;
	return transfer;
}

TextureCache::ImageDownload TextureCache::BuildDownload(const Image& image) const {
	const auto&  info    = image.info;
	const auto   binding = UploadBinding(image);
	ImageDownload transfer {.depth_target = binding == BindingType::DepthTarget};
	if (info.samples != 1 || image.backing.samples != 1) {
		return transfer;
	}
	if (transfer.depth_target) {
		transfer.valid = IsSupportedDepthPlaneReadback(info) && info.resources.layers != 0 &&
		             info.data.size % info.resources.layers == 0 &&
		             Prospero::NumBytesPerElement(info.guest_format) == info.bytes_per_block;
		return transfer;
	}
	if (info.metadata.compression != VideoOutCompression::Uncompressed) {
		return transfer;
	}
	transfer.texture = BuildTextureTransfer(image, binding, TransferDirection::Download);
	transfer.valid   = transfer.texture.valid;
	return transfer;
}

void TextureCache::UploadImage(Image& image, Buffer& source, uint64_t source_offset) {
	auto& destination = image.depth_id ? m_slot_images[image.depth_id] : image;
	const auto binding = image.depth_id ? BindingType::DepthTarget : UploadBinding(image);
	const auto  upload  = [&](std::vector<vk::BufferImageCopy>& copies, TileManager::Result linear) {
		for (auto& copy: copies) {
			copy.bufferOffset += linear.offset;
		}
		destination.Upload(copies, linear.buffer, linear.offset, linear.size);
	};

	if (binding != BindingType::DepthTarget) {
		const auto& info = image.info;
		auto transfer = BuildTextureTransfer(image, binding, TransferDirection::Upload);
		if (!transfer.valid) {
			EXIT("TextureCache: invalid texture upload: binding=%u addr=0x%016" PRIx64
			     " size=0x%016" PRIx64 " format=%u tile=%u family=%u extent=%ux%ux%u "
			     "pitch=%u levels=%u layers=%u samples=%u\n",
			     static_cast<uint32_t>(binding), info.data.address, info.data.size,
			     static_cast<uint32_t>(info.guest_format), static_cast<uint32_t>(info.tile_mode),
			     static_cast<uint32_t>(transfer.layout.surface.texture.block.family), info.extent.width,
			     info.extent.height, info.extent.depth, info.pitch, info.resources.levels,
			     info.resources.layers, info.samples);
		}
		// Resident levels only: the source holds the resident prefix (image.live) of the chain.
		RestrictToResidentLevels(image, transfer);
		TileManager::Result linear {source.Handle(), source_offset, image.live.size};
		if (!transfer.tiles.empty()) {
			linear = m_tiler.Detile(source.Handle(), source_offset, image.live.size,
			                        transfer.LinearSize(), transfer.tiles);
		}
		if (transfer.swap_bgra16) {
			linear = m_tiler.SwapBgra16(linear);
		}
		upload(transfer.regions, linear);
		return;
	}

	// The stencil plane has its own row pitch and shares the native image
	// with the depth plane.
	auto info = destination.info;
	if (image.depth_id) {
		info.data            = image.info.data;
		info.guest_format    = Prospero::BufferFormat::k8UInt;
		info.bytes_per_block = 1;
		if (info.IsTiled()) info.pitch = TileGetDepthPitch(info.extent.width, 1, 0);
	}
	if (info.samples != 1 || destination.backing.samples != 1 ||
	    info.resources.layers == 0 || info.data.size % info.resources.layers != 0 ||
	    Prospero::NumBytesPerElement(info.guest_format) != info.bytes_per_block) {
		EXIT("TextureCache: invalid depth upload\n");
	}
	const auto          layers          = info.resources.layers;
	const auto          full_slice_size = info.data.size / layers;
	auto copies = BuildDepthCopies(info, full_slice_size, image.depth_id
	                                                        ? vk::ImageAspectFlagBits::eStencil
	                                                        : vk::ImageAspectFlagBits::eDepth);
	TileManager::Result linear {source.Handle(), source_offset, source.Size() - source_offset};
	if (info.IsTiled()) {
		const auto tiles = BuildDepthTiles(info);
		linear =
		    m_tiler.Detile(source.Handle(), source_offset, info.data.size, info.data.size, tiles);
	}
	const auto transfer_bytes = image.depth_id ? 1u : DepthAspectTransferBytes(info.pixel_format);
	if (transfer_bytes != info.bytes_per_block) {
		const uint64_t texels_per_slice = static_cast<uint64_t>(info.pitch) * info.extent.height;
		EXIT_NOT_IMPLEMENTED(info.bytes_per_block != sizeof(uint16_t) ||
		                     transfer_bytes != sizeof(uint32_t) || texels_per_slice > UINT32_MAX ||
		                     texels_per_slice > UINT64_MAX / transfer_bytes);
		const uint64_t transfer_slice = texels_per_slice * transfer_bytes;
		EXIT_NOT_IMPLEMENTED(transfer_slice > UINT64_MAX / layers);
		auto promoted = m_tiler.GetScratchBuffer(transfer_slice * layers);
		m_tiler.ConvertD16(
		    linear, promoted, TileManager::D16Direction::Promote,
		    info.pixel_format == vk::Format::eD32SfloatS8Uint,
		    {.width               = info.extent.width,
		     .height              = info.extent.height,
		     .layers              = layers,
		     .source_row_stride   = static_cast<uint64_t>(info.pitch) * sizeof(uint16_t),
		     .target_row_stride   = static_cast<uint64_t>(info.pitch) * sizeof(uint32_t),
		     .source_slice_stride = full_slice_size,
		     .target_slice_stride = transfer_slice});
		linear = promoted;
		for (uint32_t layer = 0; layer < layers; layer++) {
			copies[layer].bufferOffset = transfer_slice * layer;
		}
	}
	upload(copies, linear);
}

void TextureCache::InitializeImage(ImageId id) {
	auto& image = m_slot_images[id];
	if (image.info.data.Empty()) {
		return;
	}
	TrackImage(id);
	if (image.info.metadata.compression != VideoOutCompression::Uncompressed) {
		if (image.IsCpuDirty()) {
			image.RefreshComplete();
		}
		return;
	}
	if (image.info.samples > 1) {
		return;
	}
	const bool upload = image.IsBufferModified() || image.IsCpuDirty();
	bool       guest_refresh = false;
	if (upload) {
		Profiler::ScopedFrameWait upload_time(Profiler::FrameWait::TextureUpload);
		// Attribution only: why this refresh happens and how much of the image was dirtied.
		const bool  first  = !image.WasEverUploaded();
		const char* reason = image.IsBufferModified()
		                         ? (first ? "first-use-gpu-buffer" : "gpu-buffer-write")
		                     : first                     ? "first-use"
		                     : image.residency_refresh   ? "resident-extend"
		                     : image.DirtyFromEdgeHash() ? "cpu-edge-hash"
		                                                 : "cpu-write";
		const auto span = image.DirtySpanBytes();
		// For GPU buffer writes: which kind of recorded write last touched the dirty bytes.
		const char* writer =
		    HangTrace::Enabled() && image.IsBufferModified()
		        ? HangTrace::LastGpuWriteKind(span != 0 ? image.DirtySpanBegin()
		                                                : image.info.data.address)
		        : nullptr;
		const auto binding = image.depth_id ? BindingType::DepthTarget : UploadBinding(image);
		// A refresh from guest memory of a sampled-only chunk-tracked image leaves native
		// contents equal to guest bytes everywhere; later refreshes can then be partial.
		guest_refresh = image.ChunkTracked() && !image.IsBufferModified() &&
		                !image.IsGpuModified() && !image.depth_id && binding == BindingType::Texture;
		PartialUploadResult partial {};
		if (image.ChunkTracked() && image.chunks.dirty_count != 0 && !first) {
			partial = TryPartialUpload(image);
			if (!partial.done) {
				Profiler::CountFrameEvent(Profiler::FrameEvent::TexturePartialFallbacks);
			}
		}
		// Sources cover the resident prefix only (image.live; the whole range when fully
		// resident): the bytes of levels no view can sample are neither read nor uploaded.
		const auto live = image.live;
		if (!partial.done && !TryAsyncFullUpload(image)) {
			const bool cached_source = m_buffer_cache.IsRegionRegistered(live.address, live.size) ||
			                           m_buffer_cache.IsRegionGpuModified(live.address, live.size);
			Profiler::CountFrameEvent(cached_source ? Profiler::FrameEvent::TextureUploadBytesBuffer
			                                        : Profiler::FrameEvent::TextureUploadBytesStaging,
			                          live.size);
			const auto [source, source_offset] =
			    m_buffer_cache.ObtainBufferForImage(live.address, live.size);
			if (source == nullptr) {
				EXIT("TextureCache: failed to obtain image upload source\n");
			}
			UploadImage(image, *source, source_offset);
		}
		const auto uploaded = partial.done ? partial.bytes : live.size;
		image.residency_refresh = false;
		if (!image.FullyResident()) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureResidentBytesSkipped,
			                          image.info.data.size - live.size);
			if (m_residency == ResidencyMode::Poison && !image.residency_poisoned) {
				PoisonNonResidentLevels(image);
			}
		}
		image.ClearBufferModified();
		image.NoteUpload();
		image.ClearDirtySpan();
		Profiler::CountFrameEvent(Profiler::FrameEvent::ImageUploads);
		Profiler::CountFrameEvent(Profiler::FrameEvent::ImageUploadBytes, uploaded);
		if (partial.done) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::TexturePartialUploads);
			Profiler::CountFrameEvent(Profiler::FrameEvent::TexturePartialUploadBytes, uploaded);
			Profiler::CountFrameEvent(Profiler::FrameEvent::TexturePartialSkippedBytes,
			                          live.size - std::min(live.size, uploaded));
		}
		if (HangTrace::Enabled()) {
			// Partial refreshes: bytes are the tiled guest bytes refreshed, span_bytes the dirty
			// chunk bytes that caused the refresh.
			// Partially resident textures: detail "texture-resident", bytes = resident bytes.
			HangTrace::RecordTransfer(HangTrace::TransferKind::ImageUpload,
			                          partial.done ? "cpu-write-partial" : reason,
			                          writer != nullptr          ? writer
			                          : image.depth_id           ? "stencil-plane"
			                          : !image.FullyResident()   ? "texture-resident"
			                                                     : UploadBindingName(binding),
			                          image.info.data.address,
			                          static_cast<uint32_t>(image.backing.format),
			                          image.info.extent.width, image.info.extent.height, uploaded,
			                          partial.done ? partial.dirty_bytes : span);
		}
	}
	if (image.IsCpuDirty()) {
		image.RefreshComplete();
	}
	if (image.ChunkTracked()) {
		auto& chunks = image.chunks;
		if (upload && chunks.whole_released) {
			// A whole release says nothing about how much was written; probe periodically.
			if (++chunks.whole_cycles >= 8) {
				chunks.whole_cycles = 0;
				chunks.full_streak  = 0;
			}
		} else if (upload && chunks.dirty_count != 0) {
			const bool nearly_all = uint64_t {chunks.dirty_count} * 8 >= uint64_t {chunks.count} * 7;
			chunks.full_streak    = nearly_all ? std::min(chunks.full_streak + 1, 255u) : 0u;
		}
		chunks.whole_released = false;
		image.ClearChunkDirty();
		if (guest_refresh) {
			image.SetPartialValid(true);
			if (m_partial_verify) {
				RecordChunkHashes(image);
			}
		}
	}
}

TextureCache::PartialUploadResult TextureCache::TryPartialUpload(Image& image) {
	PartialUploadResult result;
	const auto&         info = image.info;
	auto&               chunks = image.chunks;
	if (!image.PartialValid() || image.IsGpuModified() || image.IsBufferModified() ||
	    image.depth_id || info.samples != 1 || image.backing.samples != 1 || info.IsVolume() ||
	    !info.IsTiled() || info.IsDepth() ||
	    info.metadata.compression != VideoOutCompression::Uncompressed ||
	    chunks.dirty_count == 0 || chunks.dirty_count >= chunks.count ||
	    UploadBinding(image) != BindingType::Texture) {
		return result;
	}
	const auto transfer = BuildTextureTransfer(image, BindingType::Texture, TransferDirection::Upload);
	if (!transfer.valid || transfer.swap_bgra16 || transfer.tiles.empty() ||
	    transfer.tiles.size() != transfer.regions.size()) {
		return result;
	}
	if (m_partial_verify && !VerifyCleanChunks(image)) {
		return result;
	}

	// Select the parts of the chain whose tiled guest bytes overlap a dirty chunk. A detile
	// dispatch reads only its tile's [tiled_offset, tiled_offset + tiled_size) range, so every
	// changed byte that affects a texel lies in a selected part. Outside the mip tail, 2D block
	// families address a texel as (block row * blocks per row + block column) * block size plus
	// an offset that depends only on the texel's position inside its block, so a run of block
	// rows is an independent sub-surface: its own tiled range, texel rows and copy region.
	const uint64_t                   base = info.data.address;
	std::vector<GpuTileInfo>         tiles;
	std::vector<vk::BufferImageCopy> regions;
	uint64_t                         linear_total = 0;
	uint64_t                         tiled_bytes  = 0;
	const auto add = [&](GpuTileInfo tile, vk::BufferImageCopy region, uint64_t linear_size) {
		linear_total        = Common::AlignUp(linear_total, 256);
		tile.linear_offset  = linear_total;
		tile.linear_size    = linear_size;
		region.bufferOffset = linear_total;
		linear_total += linear_size;
		tiled_bytes += tile.tiled_size;
		tiles.push_back(tile);
		regions.push_back(region);
	};
	const auto texel_height = std::max(transfer.layout.surface.texture.texel_height, 1u);
	for (size_t index = 0; index < transfer.tiles.size(); index++) {
		const auto& tile   = transfer.tiles[index];
		const auto& region = transfer.regions[index];
		if (region.imageSubresource.mipLevel < image.resident_first || tile.tiled_size == 0 ||
		    !image.ChunkRangeDirty(base + tile.tiled_offset, tile.tiled_size)) {
			continue;
		}
		TileBlockLayout block {};
		const bool      family_bands = tile.family == TileBlockFamily::Standard256B ||
		                          tile.family == TileBlockFamily::Standard4KB ||
		                          tile.family == TileBlockFamily::Standard64KB ||
		                          tile.family == TileBlockFamily::Prt64KB;
		const bool bands = m_partial_bands && family_bands && !tile.tail && tile.depth == 1 &&
		                   region.imageOffset.y == 0 &&
		                   TileGetBlockLayout(tile.family, tile.bytes_per_element, block) &&
		                   block.block_depth == 1 && block.block_width != 0 &&
		                   block.block_height != 0 && block.block_size != 0;
		const uint32_t tiled_width = tile.tiled_width != 0 ? tile.tiled_width : tile.pitch;
		const uint64_t row_bytes =
		    bands ? static_cast<uint64_t>((tiled_width + block.block_width - 1) / block.block_width) *
		                block.block_size
		          : 0;
		const uint32_t rows =
		    bands ? (tile.height + block.block_height - 1) / block.block_height : 0;
		if (!bands || row_bytes == 0 || rows <= 1 || rows * row_bytes > tile.tiled_size) {
			add(tile, region, tile.linear_size);
			continue;
		}
		const uint64_t pitch_bytes = static_cast<uint64_t>(tile.pitch) * tile.bytes_per_element;
		for (uint32_t row = 0; row < rows;) {
			const auto row_dirty = [&](uint32_t r) {
				return image.ChunkRangeDirty(base + tile.tiled_offset + r * row_bytes, row_bytes);
			};
			if (!row_dirty(row)) {
				row++;
				continue;
			}
			uint32_t end_row = row + 1;
			while (end_row < rows && row_dirty(end_row)) {
				end_row++;
			}
			const uint32_t y0   = row * block.block_height;
			const uint32_t y1   = std::min(end_row * block.block_height, tile.height);
			GpuTileInfo    band = tile;
			band.tiled_offset   = tile.tiled_offset + row * row_bytes;
			band.tiled_size     = (end_row - row) * row_bytes;
			band.height         = y1 - y0;
			band.tiled_height   = (end_row - row) * block.block_height;
			auto band_region    = region;
			const uint32_t texel_y0 = y0 * texel_height;
			const uint32_t texel_y1 = std::min(y1 * texel_height, region.imageExtent.height);
			band_region.imageOffset.y      = static_cast<int32_t>(texel_y0);
			band_region.imageExtent.height = texel_y1 - texel_y0;
			add(band, band_region, static_cast<uint64_t>(band.height) * pitch_bytes);
			row = end_row;
		}
	}
	if (tiles.empty()) {
		// The writes touched only bytes no texel is decoded from (gaps between levels).
		result.done        = true;
		result.dirty_bytes = static_cast<uint64_t>(chunks.dirty_count) << chunks.shift;
		return result;
	}
	// Nearly everything is dirty: the plain refresh needs fewer dispatches for the same bytes.
	const auto live = image.live;
	if (tiled_bytes >= live.size - live.size / 8) {
		return result;
	}

	// Source: an existing cache buffer for this range (synchronized page by page) or the
	// canonical GPU-written bytes; otherwise only the selected tiled ranges of guest memory.
	vk::Buffer tiled_buffer   = nullptr;
	uint64_t   tiled_offset   = 0;
	uint64_t   tiled_capacity = 0;
	if (m_buffer_cache.IsRegionRegistered(base, live.size) ||
	    m_buffer_cache.IsRegionGpuModified(base, live.size)) {
		const auto [source, source_offset] = m_buffer_cache.ObtainBufferForImage(base, live.size);
		if (source == nullptr) {
			EXIT("TextureCache: failed to obtain image upload source\n");
		}
		tiled_buffer   = source->Handle();
		tiled_offset   = source_offset;
		tiled_capacity = live.size;
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureUploadBytesBuffer, tiled_bytes);
	} else {
		struct SourceRange {
			uint64_t offset;
			uint64_t size;
			uint64_t packed;
		};
		std::vector<SourceRange> ranges;
		uint64_t                 packed_total = 0;
		for (auto& tile: tiles) {
			const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const SourceRange& r) {
				return r.offset == tile.tiled_offset && r.size == tile.tiled_size;
			});
			if (found != ranges.end()) {
				tile.tiled_offset = found->packed;
				continue;
			}
			packed_total = Common::AlignUp(packed_total, 256);
			ranges.push_back({tile.tiled_offset, tile.tiled_size, packed_total});
			tile.tiled_offset = packed_total;
			packed_total += tile.tiled_size;
		}
		auto& staging = m_staging_copier ? StagingRing()
		                                 : m_buffer_cache.GetUtilityBuffer(MemoryUsage::Upload);
		auto [mapped, offset] = staging.Map(packed_total, 256);
		if (mapped == nullptr) {
			return result;
		}
		Profiler::CountFrameEvent(m_staging_copier ? Profiler::FrameEvent::TextureUploadBytesAsync
		                                           : Profiler::FrameEvent::TextureUploadBytesStaging,
		                          packed_total);
		if (m_staging_copier) {
			std::vector<StagingCopier::Range> copies;
			copies.reserve(ranges.size());
			for (const auto& range: ranges) {
				copies.push_back({base + range.offset, mapped + range.packed, range.size});
			}
			staging.Commit();
			m_staging_copier->Enqueue(std::move(copies), &staging, offset, packed_total);
		} else {
			for (const auto& range: ranges) {
				if (!LibKernel::Memory::TryReadBacking(base + range.offset, mapped + range.packed,
				                                       range.size) &&
				    !LibKernel::Memory::TryReadPrtBacking(base + range.offset,
				                                          mapped + range.packed, range.size)) {
					EXIT("TextureCache: failed to read mapped guest image backing\n");
				}
			}
			staging.Commit();
		}
		tiled_buffer   = staging.Handle();
		tiled_offset   = offset;
		tiled_capacity = packed_total;
	}
	const auto linear = m_tiler.Detile(tiled_buffer, tiled_offset, tiled_capacity, linear_total, tiles);
	for (auto& region: regions) {
		region.bufferOffset += linear.offset;
	}
	image.Upload(regions, linear.buffer, linear.offset, linear.size);
	result.done        = true;
	result.bytes       = tiled_bytes;
	result.dirty_bytes = static_cast<uint64_t>(chunks.dirty_count) << chunks.shift;
	return result;
}

bool TextureCache::TryAsyncFullUpload(Image& image) {
	const auto& info = image.info;
	if (!m_staging_copier || image.depth_id || image.IsBufferModified() || info.samples != 1 ||
	    image.backing.samples != 1 || !info.IsTiled() || info.IsDepth() ||
	    info.metadata.compression != VideoOutCompression::Uncompressed ||
	    UploadBinding(image) != BindingType::Texture) {
		return false;
	}
	// Same source choice as ObtainBufferForImage: cache buffers and GPU-written bytes stay on
	// their synchronous paths; only plain guest memory is copied by the worker.
	const auto base = image.live.address;
	const auto size = image.live.size;
	if (m_buffer_cache.IsRegionRegistered(base, size) ||
	    m_buffer_cache.IsRegionGpuModified(base, size)) {
		return false;
	}
	auto transfer = BuildTextureTransfer(image, BindingType::Texture, TransferDirection::Upload);
	if (!transfer.valid || transfer.swap_bgra16 || transfer.tiles.empty()) {
		return false;
	}
	RestrictToResidentLevels(image, transfer);
	auto& staging         = StagingRing();
	auto [mapped, offset] = staging.Map(size, 256);
	if (mapped == nullptr) {
		return false;
	}
	staging.Commit();
	Profiler::CountFrameEvent(Profiler::FrameEvent::TextureUploadBytesAsync, size);
	m_staging_copier->Enqueue({{base, mapped, size}}, &staging, offset, size);
	// The detile barrier includes host writes; the submission waits for the copy on the GPU.
	const auto linear =
	    m_tiler.Detile(staging.Handle(), offset, size, transfer.LinearSize(), transfer.tiles);
	for (auto& region: transfer.regions) {
		region.bufferOffset += linear.offset;
	}
	image.Upload(transfer.regions, linear.buffer, linear.offset, linear.size);
	return true;
}

void TextureCache::RecordChunkHashes(Image& image) {
	auto&          chunks     = image.chunks;
	const uint64_t chunk_size = uint64_t {1} << chunks.shift;
	chunks.hashes.assign(chunks.count, 0);
	std::vector<uint8_t> bytes(chunk_size);
	for (uint32_t index = 0; index < chunks.count; index++) {
		const auto begin = std::max(image.live.address, chunks.base + uint64_t {index} * chunk_size);
		const auto end   = std::min(image.live.End(), chunks.base + uint64_t {index + 1} * chunk_size);
		if (begin >= end ||
		    (!LibKernel::Memory::TryReadBacking(begin, bytes.data(), end - begin) &&
		     !LibKernel::Memory::TryReadPrtBacking(begin, bytes.data(), end - begin))) {
			chunks.hashes.clear();
			return;
		}
		chunks.hashes[index] = XXH3_64bits(bytes.data(), static_cast<size_t>(end - begin));
	}
}

bool TextureCache::VerifyCleanChunks(Image& image) {
	auto&          chunks     = image.chunks;
	const uint64_t chunk_size = uint64_t {1} << chunks.shift;
	if (chunks.hashes.size() != chunks.count) {
		return false;
	}
	std::vector<uint8_t> bytes(chunk_size);
	for (uint32_t index = 0; index < chunks.count; index++) {
		if (Image::ChunkBit(chunks.dirty, index)) {
			continue;
		}
		const auto begin = std::max(image.live.address, chunks.base + uint64_t {index} * chunk_size);
		const auto end   = std::min(image.live.End(), chunks.base + uint64_t {index + 1} * chunk_size);
		if (begin >= end ||
		    (!LibKernel::Memory::TryReadBacking(begin, bytes.data(), end - begin) &&
		     !LibKernel::Memory::TryReadPrtBacking(begin, bytes.data(), end - begin))) {
			return false;
		}
		if (XXH3_64bits(bytes.data(), static_cast<size_t>(end - begin)) != chunks.hashes[index]) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::TexturePartialVerifyMismatches);
			if (++m_partial_verify_mismatches <= 64) {
				LOGF("Texture partial verify: clean chunk changed without a recorded write: "
				     "image=0x%016" PRIx64 " size=0x%" PRIx64 " chunk=0x%016" PRIx64 "\n",
				     image.info.data.address, image.info.data.size, begin);
			}
			return false;
		}
	}
	return true;
}

Profiler::FrameEvent TextureCache::TryMaterializeGpuDccClear(ImageId id, const ImageDesc& desc,
                                                              uint32_t metadata_base_layer) {
	using Event = Profiler::FrameEvent;
	using Support = DccClearHelper::Support;
	const auto  range = desc.info.metadata.range;
	const auto& view  = desc.view_info;
	if (!m_dcc_clear || !m_dcc_clear->Available()) {
		return Event::DccFallbackDisabled;
	}
	// The video-out path does not consume its clear key; keep it on the CPU inspection.
	if (desc.type == BindingType::VideoOut) {
		return Event::DccFallbackBinding;
	}
	const auto layers = desc.info.TransferLayers();
	const auto first  = metadata_base_layer;
	const auto count  = view.layer_count;
	if (desc.info.IsVolume() || desc.info.resources.levels != 1 || desc.info.samples != 1 ||
	    view.base_level != 0 || view.level_count != 1 ||
	    view.aspect != vk::ImageAspectFlagBits::eColor ||
	    (view.type != vk::ImageViewType::e2D && view.type != vk::ImageViewType::e2DArray) ||
	    desc.info.metadata.compression != VideoOutCompression::Uncompressed || layers == 0 ||
	    count == 0 || first >= layers || count > layers - first) {
		return Event::DccFallbackShape;
	}
	// The CPU decoder's result for every clear code under this binding (ClearCodes order).
	DccClearHelper::ClearValues values {};
	uint32_t                    decodable = 0;
	for (uint32_t index = 0; index < DccClearHelper::ClearCodes.size(); ++index) {
		vk::ClearColorValue color {};
		if (DecodeDccClear(desc, DccClearHelper::ClearCodes[index], color)) {
			values[index] = color;
			decodable |= 1u << index;
		}
	}
	switch (m_dcc_clear->SupportsFormat(view.format)) {
		case Support::Ok: break;
		case Support::Disabled: return Event::DccFallbackDisabled;
		case Support::Format: return Event::DccFallbackFormat;
		case Support::Unsupported: return Event::DccFallbackUnsupported;
	}
	const auto slice_size = range.size / layers;
	const auto slice_range = [&](uint32_t slice) {
		return GuestRange {range.address + slice_size * (first + slice), slice_size};
	};
	{
		std::scoped_lock lock {m_lock};
		if (!FindImagesInRegion(range.address, range.size, false).empty()) {
			return Event::DccFallbackMetadataAliased;
		}
		// A retained inspection of an unchanged slice leaves consumed keys (0xFF), nonuniform
		// bytes or a code its interpretation rejected. An interpretation accepting no more codes
		// than that one reads the same bytes to the same no-op, for any image or layer; only the
		// metadata processing is skipped, the caller still refreshes this image normally.
		bool reusable = true;
		for (uint32_t slice = 0; reusable && slice < count; ++slice) {
			const auto metadata = slice_range(slice);
			const auto revision = m_buffer_cache.GetContentRevision(metadata.address, metadata.size);
			const auto found    = m_gpu_dcc_inspections.find(metadata.address);
			reusable = revision && found != m_gpu_dcc_inspections.end() &&
			           found->second.metadata == metadata && found->second.revision == *revision &&
			           (decodable & ~found->second.decodable_mask) == 0;
		}
		if (reusable) {
			m_gpu_dcc_reuses += count;
			Profiler::CountFrameEvent(Event::DccGpuReuses, count);
			return Event::DccGpuReuses;
		}
	}
	if (ImageRangeOverlaps(range, desc.info.data)) {
		return Event::DccFallbackMetadataAliased;
	}
	const auto eligible = [&]() -> std::optional<Event> {
		const auto* image = m_slot_images.try_get(id);
		if (image == nullptr || !image->registered || image->depth_id ||
		    image->info.data != desc.info.data || image->info.extent != desc.info.extent ||
		    !SafeToDownload(*image)) {
			return Event::DccFallbackImageState;
		}
		if (image->backing.extent.width != desc.info.extent.width ||
		    image->backing.extent.height != desc.info.extent.height ||
		    view.base_layer >= image->backing.layers ||
		    count > image->backing.layers - view.base_layer) {
			return Event::DccFallbackShape;
		}
		switch (m_dcc_clear->SupportsImage(*image, view.format, slice_size)) {
			case Support::Ok: break;
			case Support::Disabled: return Event::DccFallbackDisabled;
			case Support::Format: return Event::DccFallbackFormat;
			case Support::Unsupported: return Event::DccFallbackUnsupported;
		}
		if (!FindImagesInRegion(range.address, range.size, false).empty()) {
			return Event::DccFallbackMetadataAliased;
		}
		return std::nullopt;
	};
	{
		std::scoped_lock lock {m_lock};
		if (const auto reason = eligible()) return *reason;
	}
	// Use the canonical buffer and its normal GPU-write tracking. No CPU shadow of the
	// clear value is trusted. Acquiring it may upload/submit, so hold no texture lock.
	auto [buffer, offset] = m_buffer_cache.ObtainBuffer(range.address, range.size, true, false);
	if (buffer == nullptr) {
		return Event::DccFallbackUnsupported;
	}
	if (offset % sizeof(uint32_t) != 0) {
		return Event::DccFallbackAlignment;
	}
	std::scoped_lock lock {m_lock};
	if (const auto reason = eligible()) return *reason;
	auto& image = m_slot_images[id];
	TrackImage(id);
	{
		KYTY_PROFILER_DETAIL_BLOCK("DCC::GpuMaterialize");
		for (uint32_t slice = 0; slice < count; ++slice) {
			m_dcc_clear->RecordSlice(image, view.format, view.base_layer + slice, buffer->Handle(),
			                         offset + slice_size * (first + slice), slice_size, values);
		}
	}
	CommitGpuWrite(image);
	m_gpu_dcc_records += count;
	Profiler::CountFrameEvent(Event::DccGpuRecords, count);
	for (uint32_t slice = 0; slice < count; ++slice) {
		const auto metadata = slice_range(slice);
		if (const auto revision =
		        m_buffer_cache.GetContentRevision(metadata.address, metadata.size)) {
			// Retained metadata is independent of image lifetime; cap address reuse history.
			if (m_gpu_dcc_inspections.size() >= 256 &&
			    !m_gpu_dcc_inspections.contains(metadata.address)) {
				m_gpu_dcc_inspections.clear();
			}
			m_gpu_dcc_inspections.insert_or_assign(
			    metadata.address, GpuDccInspection {metadata, *revision, decodable});
		} else {
			m_gpu_dcc_inspections.erase(metadata.address);
		}
	}
	if (m_gpu_dcc_records <= 8) {
		LOGF("GPU DCC inspection: metadata=0x%016" PRIx64 " bytes=%" PRIu64
		     " image=0x%016" PRIx64 " format=%u type=%u slices=%u codes=0x%02x\n",
		     range.address, range.size, desc.info.data.address,
		     static_cast<uint32_t>(view.format), static_cast<uint32_t>(desc.type), count,
		     decodable);
	}
	TraceDccDiagnostic("DCC_GPU_RECORD meta=0x%" PRIx64 " bytes=%" PRIu64
	                   " data=0x%" PRIx64 " format=%u type=%u first=%u count=%u tick=%" PRIu64,
	                   range.address, range.size, desc.info.data.address,
	                   static_cast<uint32_t>(view.format), static_cast<uint32_t>(desc.type), first,
	                   count, m_scheduler.CurrentTick());
	return Event::DccGpuRecords;
}

void TextureCache::MaterializeDccClear(ImageId id, const ImageDesc& desc,
                                       uint32_t metadata_base_layer) {
	if (desc.info.metadata.kind != ImageMetadataKind::Dcc) {
		return;
	}
	KYTY_PROFILER_DETAIL_FUNCTION();
	const auto range = desc.info.metadata.range;
	{
		std::scoped_lock lock {m_lock};
		auto& image         = m_slot_images[id];
		image.info.metadata = desc.info.metadata;
		// A native DCC allocation must not retain a reused HTile/CMask/FMask interpretation.
		m_surface_metas.erase(range.address);
		if (range.size == 0 || desc.info.resources.levels != 1 || image.info.resources.levels != 1) {
			return;
		}
	}
	const auto layers = desc.info.TransferLayers();
	// These one-mip surfaces use complete 4 KiB DCC metadata blocks.
	constexpr uint64_t MetadataBlockSize = 0x1000;
	if (!range.Valid() || range.address % MetadataBlockSize != 0 || layers == 0 ||
	    range.size % layers != 0 || (range.size / layers) % MetadataBlockSize != 0) {
		EXIT("TextureCache: DCC slices must contain aligned 4 KiB blocks\n");
	}
	const auto& view           = desc.view_info;
	const bool  volume_texture = desc.info.IsVolume() && view.type == vk::ImageViewType::e3D;
	const auto  first          = volume_texture ? 0u : metadata_base_layer;
	const auto  image_first    = volume_texture ? 0u : view.base_layer;
	const auto  count          = volume_texture ? desc.info.extent.depth : view.layer_count;
	if (first >= layers || count > layers - first) {
		EXIT("TextureCache: DCC view exceeds its native metadata slices\n");
	}
	// Finish native metadata writes before reading backing bytes. This can submit the scheduler,
	// so discovery runs before final draw uploads and never holds the texture lock across it.
	uint64_t diagnostic_readback = 0;
	if (m_buffer_cache.IsRegionGpuModified(range.address, range.size)) {
		// The guest's DCC fast clear is a uniform fill of the metadata. When the whole range still
		// holds a recorded fill (nothing wrote it since), every slice's code is that byte: no
		// GPU readback is needed to decide the clear.
		const auto known = m_buffer_cache.KnownFill(range.address, range.size);
		bool       unaliased = false;
		if (known) {
			std::scoped_lock lock {m_lock};
			unaliased = FindImagesInRegion(range.address, range.size, false).empty();
		}
		const uint32_t known_byte = known ? (*known & 0xffu) : 0u;
		if (known && unaliased && *known == known_byte * 0x01010101u) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::DccKnownFillClears);
			vk::ClearValue clear {};
			if (!DecodeDccClear(desc, static_cast<uint8_t>(known_byte), clear.color)) {
				return; // e.g. 0xFF: not a clear code, nothing to materialize
			}
			const auto slice_size = range.size / layers;
			for (uint32_t slice = 0; slice < count; slice++) {
				{
					std::scoped_lock lock {m_lock};
					ClearImage(m_scheduler.Current(), id, view.format,
					           {vk::ImageAspectFlagBits::eColor, view.base_level, view.level_count,
					            image_first + slice, 1}, clear);
				}
				if (desc.type != BindingType::VideoOut) {
					m_buffer_cache.FillBuffer(range.address + slice_size * (first + slice),
					                          slice_size, UINT32_MAX, false);
				}
			}
			return;
		}
		{
			++m_gpu_dcc_attempts;
			const auto outcome = TryMaterializeGpuDccClear(id, desc, metadata_base_layer);
			const bool native  = outcome == Profiler::FrameEvent::DccGpuRecords ||
			                    outcome == Profiler::FrameEvent::DccGpuReuses;
			if (!native) {
				++m_gpu_dcc_fallbacks;
				Profiler::CountFrameEvent(Profiler::FrameEvent::DccCpuFallbacks);
				Profiler::CountFrameEvent(outcome);
				// The first few fallbacks of every reason, with what decided it.
				static std::array<std::atomic<uint32_t>, static_cast<size_t>(
				                                             Profiler::FrameEvent::Count)>
				    logged {};
				if (logged[static_cast<size_t>(outcome)].fetch_add(1, std::memory_order_relaxed) <
				    4) {
					uint32_t state = 0;
					uint32_t native_format = 0, native_layers = 0, native_usage = 0,
					         native_flags = 0;
					{
						std::scoped_lock lock {m_lock};
						if (const auto* image = m_slot_images.try_get(id)) {
							state = (image->IsCpuDirty() ? 1u : 0u) |
							        (image->IsBufferModified() ? 2u : 0u) |
							        (image->IsGpuModified() ? 4u : 0u) |
							        (image->registered ? 8u : 0u) |
							        (image->depth_id ? 16u : 0u) |
							        (m_buffer_cache.HasGpuDirtyBytes(image->info.data.address,
							                                          image->info.data.size)
							             ? 32u
							             : 0u) |
							        (!FindImagesInRegion(range.address, range.size, false).empty()
							             ? 64u
							             : 0u);
							native_format = static_cast<uint32_t>(image->backing.format);
							native_layers = image->backing.layers;
							native_usage =
							    static_cast<uint32_t>(static_cast<VkImageUsageFlags>(image->backing.usage));
							native_flags = static_cast<uint32_t>(
							    static_cast<VkImageCreateFlags>(image->backing.flags));
						}
					}
					LOGF("DCC CPU fallback (reason event %u): metadata=0x%016" PRIx64
					     " bytes=%" PRIu64 " data=0x%016" PRIx64 " type=%u view_format=%u"
					     " view_type=%u base_layer=%u layers=%u meta_first=%u meta_layers=%u"
					     " extent=%ux%ux%u native_format=%u native_layers=%u usage=0x%x"
					     " flags=0x%x state=0x%x (1 cpu-dirty 2 buffer-modified 4 gpu-modified"
					     " 8 registered 16 stencil 32 gpu-dirty-bytes 64 metadata-image)\n",
					     static_cast<uint32_t>(outcome), range.address, range.size,
					     desc.info.data.address, static_cast<uint32_t>(desc.type),
					     static_cast<uint32_t>(view.format), static_cast<uint32_t>(view.type),
					     view.base_layer, view.layer_count, first, layers, desc.info.extent.width,
					     desc.info.extent.height, desc.info.extent.depth, native_format,
					     native_layers, native_usage, native_flags, state);
				}
			}
			if ((m_gpu_dcc_attempts & 255u) == 0) {
				if (tracy::ProfilerAvailable()) {
					TracyPlot("DCC.NativeInspections", static_cast<double>(m_gpu_dcc_records));
					TracyPlot("DCC.ReusedInspections", static_cast<double>(m_gpu_dcc_reuses));
					TracyPlot("DCC.CpuFallbacks", static_cast<double>(m_gpu_dcc_fallbacks));
				}
				LOGF("GPU DCC totals: requests=%" PRIu64 " native=%" PRIu64
				     " reused=%" PRIu64 " CPU_fallback=%" PRIu64 "\n",
				     m_gpu_dcc_attempts, m_gpu_dcc_records, m_gpu_dcc_reuses, m_gpu_dcc_fallbacks);
			}
			if (native) return;
		}
		KYTY_PROFILER_DETAIL_BLOCK("DCC::Readback");
		if (Profiler::DetailedEnabled() && tracy::ProfilerAvailable() && TracyIsConnected) {
			static std::atomic<uint64_t> readback_count {0};
			diagnostic_readback = readback_count.fetch_add(1, std::memory_order_relaxed) + 1u;
			TraceDccDiagnostic(
			    "DCC_READBACK_BEGIN id=%" PRIu64 " meta=0x%" PRIx64 " bytes=%" PRIu64
			    " data=0x%" PRIx64 " data_bytes=%" PRIu64 " format=%u type=%u first=%u count=%u"
			    " layers=%u mip=%u tick=%" PRIu64,
			    diagnostic_readback, range.address, range.size, desc.info.data.address,
			    desc.info.data.size, static_cast<uint32_t>(view.format),
			    static_cast<uint32_t>(desc.type), first, count, layers, view.base_level,
			    m_scheduler.CurrentTick());
		}
		// An odd period avoids repeatedly sampling the same member of an alternating pair.
		const bool sample_fallback = m_dcc_clear && tracy::ProfilerAvailable() && TracyIsConnected &&
		                             (m_gpu_dcc_fallbacks % 257u) == 0;
		uint32_t ownership = 0;
		if (sample_fallback) {
			std::scoped_lock lock {m_lock};
			const auto& image = m_slot_images[id];
			ownership = (image.IsCpuDirty() ? 1u : 0u) |
			            (image.IsBufferModified() ? 2u : 0u) |
			            (image.IsGpuModified() ? 4u : 0u) |
			            (m_buffer_cache.HasGpuDirtyBytes(image.info.data.address,
			                                              image.info.data.size) ? 8u : 0u);
		}
		const auto readback_start = sample_fallback ? std::chrono::steady_clock::now()
		                                            : std::chrono::steady_clock::time_point {};
		{
			Profiler::ScopedFrameWait frame_wait(Profiler::FrameWait::DccFallback);
			m_buffer_cache.ReadMemory(range.address, range.size, false);
		}
		if (sample_fallback) {
			const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
			    std::chrono::steady_clock::now() - readback_start).count();
			char message[400];
			const auto length = std::snprintf(message, sizeof(message),
			    "DCC_CPU_FALLBACK meta=0x%" PRIx64 " bytes=%" PRIu64 " data=0x%" PRIx64
			    " format=%u type=%u ownership=%u layers=%u first=%u count=%u ns=%" PRIu64,
			    range.address, range.size, desc.info.data.address,
			    static_cast<uint32_t>(view.format), static_cast<uint32_t>(desc.type),
			    ownership, layers, first, count,
			    static_cast<uint64_t>(elapsed));
			if (length > 0 && static_cast<size_t>(length) < sizeof(message)) {
				TracyMessageS(message, static_cast<size_t>(length), 0);
			}
		}
		if (diagnostic_readback != 0) {
			TraceDccDiagnostic("DCC_READBACK_END id=%" PRIu64 " tick=%" PRIu64,
			                   diagnostic_readback, m_scheduler.CurrentTick());
		}
	}
	const auto slice_size = range.size / layers;
	for (uint32_t slice = 0; slice < count; slice++) {
		KYTY_PROFILER_DETAIL_BLOCK("DCC::InspectSlice");
		const auto address = range.address + slice_size * (first + slice);
		uint8_t code = 0;
		if (!LibKernel::Memory::TryReadBacking(address, &code, sizeof(code))) {
			EXIT("TextureCache: failed to read DCC metadata backing\n");
		}
		vk::ClearValue clear {};
		if (!DecodeDccClear(desc, code, clear.color)) {
			if (diagnostic_readback != 0) {
				TraceDccDiagnostic(
				    "DCC_SLICE id=%" PRIu64 " slice=%u address=0x%" PRIx64 " bytes=%" PRIu64
				    " code=0x%02x decoded=0 uniform=-1 consumed=0",
				    diagnostic_readback, first + slice, address, slice_size, unsigned(code));
			}
			continue;
		}
		std::vector<uint8_t> bytes(slice_size);
		if (!LibKernel::Memory::TryReadBacking(address, bytes.data(), bytes.size())) {
			EXIT("TextureCache: failed to read DCC metadata slice\n");
		}
		if (!std::all_of(bytes.begin(), bytes.end(), [code](uint8_t byte) { return byte == code; })) {
			if (diagnostic_readback != 0) {
				TraceDccDiagnostic(
				    "DCC_SLICE id=%" PRIu64 " slice=%u address=0x%" PRIx64 " bytes=%" PRIu64
				    " code=0x%02x decoded=1 uniform=0 consumed=0",
				    diagnostic_readback, first + slice, address, slice_size, unsigned(code));
			}
			continue;
		}
		{
			KYTY_PROFILER_DETAIL_BLOCK("DCC::ClearImage");
			std::scoped_lock lock {m_lock};
			ClearImage(m_scheduler.Current(), id, view.format,
			           {vk::ImageAspectFlagBits::eColor, view.base_level, view.level_count,
			            image_first + slice, 1}, clear);
		}
		// Native expanded keys own consumption. Existing buffer tracking publishes this CPU
		// write to future GPU readers; FillBuffer can fault and must run outside the texture lock.
		if (desc.type != BindingType::VideoOut) {
			KYTY_PROFILER_DETAIL_BLOCK("DCC::ConsumeClearKey");
			m_buffer_cache.FillBuffer(address, slice_size, UINT32_MAX, false);
		}
		if (diagnostic_readback != 0) {
			TraceDccDiagnostic(
			    "DCC_SLICE id=%" PRIu64 " slice=%u address=0x%" PRIx64 " bytes=%" PRIu64
			    " code=0x%02x decoded=1 uniform=1 consumed=%u",
			    diagnostic_readback, first + slice, address, slice_size, unsigned(code),
			    desc.type != BindingType::VideoOut ? 1u : 0u);
		}
	}
}

// TextureBindingMemo::RefreshIsNoOp mirrors when this is a no-op; keep them in sync.
void TextureCache::RefreshImage(ImageId id) {
	auto& image = m_slot_images[id];
	if (image.depth_id &&
	    (m_slot_images[image.depth_id].info.metadata.stencil_compressed ||
	     m_slot_images[image.depth_id].info.samples != 1)) {
		return;
	}
	TrackImage(id);
	if (image.IsMaybeCpuDirty()) {
		const auto hash = image.HashGuestEdges();
		if (image.NeedsMaybeCpuHash()) {
			image.SetMaybeCpuHash(hash);
			return;
		}
		(void)image.ResolveMaybeCpuHash(hash);
	}
	bool cpu_dirty = image.IsBufferModified() || image.IsDefinitelyCpuDirty();
	if (image.info.metadata.compression != VideoOutCompression::Uncompressed) {
		if (cpu_dirty) {
			EXIT("TextureCache: compressed guest image refresh is unsupported\n");
		}
		return;
	}
	if (!cpu_dirty) {
		return;
	}
	InitializeImage(id);
}

ImageId TextureCache::AssociateStencil(ImageId depth_id, GuestRange stencil) {
	if (!stencil.Valid()) {
		EXIT("TextureCache: invalid stencil association range\n");
	}
	auto& depth = m_slot_images[depth_id];
	if (!depth.info.IsDepth() || !depth.info.HasStencil()) {
		EXIT("TextureCache: stencil association requires a depth/stencil image\n");
	}

	ImageId association {};
	for (const auto id: FindImagesInRegion(stencil.address, stencil.size, false)) {
		const auto owner = m_slot_images.try_get(id);
		if (owner != nullptr && owner->info.data == stencil &&
		    owner->info.extent == depth.info.extent) {
			association = id;
		}
	}
	if (!association) {
		ImageInfo info {};
		info.data   = stencil;
		info.extent = depth.info.extent;
		association = InsertImage(info);
	}
	auto& record = m_slot_images[association];
	TouchImage(record);
	if (record.depth_id != depth_id) {
		// A binding of this image now resolves to the depth image (TextureBindingMemo).
		NoteStructureChange(record);
	}
	record.depth_id = depth_id;
	return association;
}

ImageId TextureCache::FindImage(ImageDesc& desc, bool exact_format) {
	KYTY_PROFILER_DETAIL_FUNCTION();
	auto& command = m_scheduler.Current();
	if (command.IsInvalid()) {
		EXIT("TextureCache: image lookup requires a valid command buffer\n");
	}
	ValidateImageDesc(desc);
	if (desc.info.data.Empty()) {
		std::scoped_lock lock {m_lock};
		return GetNullImage(desc);
	}
	const auto metadata_base_layer = desc.view_info.base_layer;

	ImageId result {};
	{
		std::scoped_lock lock {m_lock};
		ImageIds candidates;
		const bool use_legacy = m_image_lookup_mode != ImageLookupMode::FirstPage;
		if (use_legacy) {
			candidates = FindImagesInRegion(desc.info.data.address, desc.info.data.size, false);
			for (const auto id: candidates) {
				if (SameBacking(m_slot_images[id].info, desc.info, exact_format)) {
					result = id;
				}
			}
			if (m_image_lookup_mode == ImageLookupMode::Verify) {
				const auto fast = FindImageWithSameBacking(desc.info, exact_format);
				++m_image_lookup_checks;
				if (fast != result) {
					++m_image_lookup_mismatches;
					if (m_image_lookup_mismatches <= 32) {
						LOGF("Image lookup mismatch: address=0x%016" PRIx64
						     " size=0x%016" PRIx64 " exact=%u fast=%u:%u legacy=%u:%u\n",
						     desc.info.data.address, desc.info.data.size, exact_format ? 1u : 0u,
						     fast.index, fast.generation, result.index, result.generation);
					}
				}
				if ((m_image_lookup_checks & 0xffffu) == 0) {
					LOGF("Image lookup verification: checks=%" PRIu64 " mismatches=%" PRIu64 "\n",
					     m_image_lookup_checks, m_image_lookup_mismatches);
				}
			}
		} else {
			result = FindImageWithSameBacking(desc.info, exact_format);
		}

		int32_t view_mip   = -1;
		int32_t view_layer = -1;
		// Levels this lookup can sample. A new image registers only their prefix, and images that
		// overlap only levels it cannot sample are neither merge candidates nor retired by it.
		const auto wanted_first    = RequestedFirstLevel(desc, desc.info.resources.levels);
		uint64_t   wanted_prefix   = 0;
		bool       prefix_computed = false;
		if (!result) {
			if (!use_legacy) {
				wanted_prefix   = ResidentPrefixSize(desc.info, wanted_first);
				prefix_computed = true;
				candidates    = FindImagesInRegion(
				    desc.info.data.address,
				    wanted_prefix != 0 ? wanted_prefix : desc.info.data.size, false);
			}
			constexpr size_t CrowdedOverlap = 24;
			m_overlap_crowded = candidates.size() > CrowdedOverlap;
			for (const auto candidate: candidates) {
				view_mip                = -1;
				view_layer              = -1;
				const auto& merged_info = result ? m_slot_images[result].info : desc.info;
				const auto  overlap     = ResolveOverlap(merged_info, desc.type, candidate, result);
				if (overlap.image) {
					result     = overlap.image;
					view_mip   = overlap.mip;
					view_layer = overlap.layer;
				}
			}
		}

		if (result) {
			auto& resolved = m_slot_images[result];
			if (exact_format && resolved.info.pixel_format != desc.info.pixel_format) {
				result = {};
			} else if (resolved.info.resources < desc.info.resources) {
				FreeImage(result, HangTrace::ImageFreeReason::SmallerResources);
				result = {};
			}
		}
		if (!result) {
			// A prefix computed as not applicable (0) keeps the image fully resident; InsertImage
			// computes it when this lookup did not.
			result = InsertImage(desc.info,
			                     prefix_computed && wanted_prefix == 0 ? 0u : wanted_first,
			                     wanted_prefix);
			auto& inserted = m_slot_images[result];
			if (m_buffer_cache.HasGpuDirtyBytes(inserted.live.address, inserted.live.size)) {
				inserted.MarkBufferModified();
			}
		}
		auto& image = m_slot_images[result];
		if (view_mip >= 0) {
			desc.view_info.base_level = static_cast<uint32_t>(view_mip);
		}
		if (view_layer >= 0) {
			desc.view_info.base_layer = static_cast<uint32_t>(view_layer);
		}
		// Every level the (adjusted) view can sample must be resident: a finer MIN_LOD than
		// before extends the image, any non-sampling binding makes the whole chain resident.
		EnsureResidency(result, RequestedFirstLevel(desc, image.info.resources.levels),
		                desc.type == BindingType::Texture);
		SyncAliasFromOwner(result);
		image.tick_accessed_last = m_scheduler.CurrentTick();
		TouchImage(image);
	}
	MaterializeDccClear(result, desc, metadata_base_layer);
	if (desc.type == BindingType::VideoOut &&
	    desc.info.metadata.compression != VideoOutCompression::Uncompressed) {
		std::scoped_lock lock {m_lock};
		const auto& image = m_slot_images[result];
		const bool guest_dirty = image.IsBufferModified() || image.IsCpuDirty();
		const bool native_current =
		    (image.usage.render_target || image.IsGpuModified()) && !guest_dirty;
		if (!native_current) {
			EXIT("TextureCache: compressed video-out read requires clean native GPU "
			     "contents\n");
		}
	}
	return result;
}

void TextureCache::UpdateImage(ImageId id) {
	std::scoped_lock lock {m_lock};
	auto&            image = m_slot_images[id];
	TouchImage(image);
	// External readers (presentation, depth copies) read every level.
	RequireFullResidency(id);
	RefreshImage(id);
}

ImageId TextureCache::FindImageFromRange(uint64_t address, uint64_t size, bool ensure_valid) {
	if (!GuestRange {address, size}.Valid()) {
		return {};
	}
	std::scoped_lock lock {m_lock};
	ImageIds         matches;
	for (const auto id: FindImagesInRegion(address, size, false)) {
		auto owner = m_slot_images.try_get(id);
		if (owner == nullptr || owner->info.data.address != address) {
			continue;
		}
		if (ensure_valid && owner->depth_id) {
			owner = m_slot_images.try_get(owner->depth_id);
		}
		if (owner == nullptr || (ensure_valid && !SafeToDownload(*owner))) {
			continue;
		}
		matches.push_back(id);
	}
	ImageId selected {};
	if (matches.size() == 1) {
		selected = matches.front();
	} else {
		for (const auto id: matches) {
			const auto& image = m_slot_images[id];
			if (image.info.data.size == size) {
				selected = id;
				break;
			}
		}
	}
	if (selected && ensure_valid) {
		const auto owner = m_slot_images.try_get(selected);
		if (owner != nullptr && owner->depth_id) {
			selected = owner->depth_id;
		}
	}
	return selected;
}

vk::ImageView TextureCache::FindTexture(ImageId id, const ImageDesc& desc) {
	std::scoped_lock lock {m_lock};
	auto&            image = m_slot_images[id];
	TouchImage(image);
	if (!image.info.data.Empty()) {
		if (!image.registered || image.depth_id || image.binding.needs_rebind) {
			EXIT("TextureCache: texture requires rediscovery before final acquisition\n");
		}
		// The view's levels must be resident (a storage binding: the whole chain). FindImage
		// already did this for the same description; binding caches may skip FindImage.
		EnsureResidency(id, RequestedFirstLevel(desc, image.info.resources.levels),
		                desc.type == BindingType::Texture);
	}
	if (desc.type == BindingType::Storage) {
		MarkImageGpuModified(image);
	}
	if (!image.info.data.Empty()) {
		RefreshImage(id);
		if (image.info.HasStencil() &&
		    desc.info.data.address >= image.info.stencil.address &&
		    desc.info.data.End() <= image.info.stencil.End()) {
			for (const auto stencil_id:
			     FindImagesInRegion(image.info.stencil.address, image.info.stencil.size, false)) {
				const auto* stencil = m_slot_images.try_get(stencil_id);
				if (stencil != nullptr && stencil->depth_id == id &&
				    stencil->info.data == image.info.stencil) {
					RefreshImage(stencil_id);
					break;
				}
			}
		}
	}
	switch (desc.type) {
		case BindingType::Texture: break;
		case BindingType::Storage:
			if (!image.info.data.Empty()) {
				CommitGpuWrite(image);
			}
			TrackImageDownload(id, image);
			break;
		default: EXIT("TextureCache: invalid texture binding\n");
	}
	return image.FindView(desc.view_info);
}

vk::ImageView TextureCache::FindRenderTarget(ImageId id, const ImageDesc& desc) {
	if (desc.type != BindingType::RenderTarget) {
		EXIT("TextureCache: invalid color-target binding\n");
	}
	std::scoped_lock lock {m_lock};
	auto&            image = m_slot_images[id];
	if (!image.registered || image.depth_id || image.binding.needs_rebind) {
		EXIT("TextureCache: color target requires rediscovery before final acquisition\n");
	}
	RequireFullResidency(id);
	TouchImage(image);
	MarkImageGpuModified(image);
	image.usage.render_target = true;
	RefreshImage(id);
	CommitGpuWrite(image);
	TrackImageDownload(id, image);
	return image.FindView(desc.view_info);
}

vk::ImageView TextureCache::FindDepthTarget(ImageId id, const ImageDesc& desc) {
	if (desc.type != BindingType::DepthTarget) {
		EXIT("TextureCache: invalid depth-target binding\n");
	}
	std::scoped_lock lock {m_lock};
	auto&            image = m_slot_images[id];
	if (!image.registered || image.depth_id || image.binding.needs_rebind) {
		EXIT("TextureCache: depth target requires rediscovery before final acquisition\n");
	}
	RequireFullResidency(id);
	TouchImage(image);
	MarkImageGpuModified(image);
	image.usage.depth_target = true;
	image.info.stencil = desc.info.stencil;
	image.info.metadata = desc.info.metadata;
	if (desc.info.HasMetadata()) {
		m_surface_metas.emplace(desc.info.metadata.range.address,
		                        MetaDataInfo {.type       = MetaDataInfo::Type::HTile,
		                                      .clear_mask = image.info.htile_clear_mask});
	}
	RefreshImage(id);
	CommitGpuWrite(image);
	if (desc.info.HasStencil()) {
		RefreshImage(AssociateStencil(id, desc.info.stencil));
	}
	return image.FindView(desc.view_info);
}

void TextureCache::MarkGpuWritten(ImageId id) {
	std::scoped_lock lock {m_lock};
	auto&            image = m_slot_images[id];
	if (!image.registered || image.depth_id) {
		EXIT("TextureCache: cannot mark an unavailable image GPU-written\n");
	}
	// Before the caller records its write: levels it does not write must hold guest data.
	RequireFullResidency(id);
	TrackImage(id);
	CommitGpuWrite(image);
	if (image.info.HasStencil()) {
		const auto stencil_id = AssociateStencil(id, image.info.stencil);
		TrackImage(stencil_id);
		CommitGpuWrite(m_slot_images[stencil_id]);
	}
}

void TextureCache::SyncAliasFromOwner(ImageId id) {
	auto& image = m_slot_images[id];
	if (!AliasAgeByFrames() || image.alias_owner || image.depth_id || image.info.data.Empty() ||
	    image.info.HasStencil() || image.backing.image == nullptr) {
		return;
	}
	// A kept alias (same memory, another format) is stale when another interpretation wrote the
	// memory since. Reinterpret the owner's contents, as the recreate path used to, and take over.
	for (const auto other_id: FindImagesInRegion(image.info.data.address, image.info.data.size, false)) {
		const auto* other = m_slot_images.try_get(other_id);
		if (other_id == id || other == nullptr || !other->registered || other->depth_id ||
		    !other->alias_owner || !other->IsGpuModified() || other->IsBufferModified() ||
		    other->info.HasStencil() || other->info.data != image.info.data ||
		    other->info.extent != image.info.extent ||
		    other->backing.samples != image.backing.samples) {
			continue;
		}
		// Taking over the owner's contents makes this image GPU-owned: whole chain.
		RequireFullResidency(id);
		const auto trace_sync = [&](const char* reason, uint64_t bytes) {
			HangTrace::RecordTransfer(HangTrace::TransferKind::AliasSync, reason, "",
			                          image.info.data.address,
			                          static_cast<uint32_t>(image.backing.format),
			                          image.info.extent.width, image.info.extent.height, bytes, 0);
		};
		// Skip the copy when this alias already holds exactly the owner's native bits: its last
		// contents came from a lossless full copy of the owner (or the other way round) and
		// neither image has been written since. Ownership moves exactly as after a copy.
		if (AliasSyncSkipEnabled() && image.ContentSerial() != 0 &&
		    image.ContentSerial() == other->ContentSerial() && !image.IsCpuDirty() &&
		    !image.IsBufferModified()) {
			// A CPU-dirty owner is refreshed first, exactly as CopyImage would; that upload is
			// a write and gives it a new serial.
			RefreshCopySource(other_id);
			const auto& owner = m_slot_images[other_id];
			if (image.ContentSerial() == owner.ContentSerial() && owner.IsGpuModified() &&
			    !owner.IsBufferModified() && !image.IsCpuDirty() && !image.IsBufferModified()) {
				const auto serial = owner.ContentSerial();
				TrackImage(id);
				CommitGpuWrite(image);
				image.AdoptContentSerial(serial);
				Profiler::CountFrameEvent(Profiler::FrameEvent::AliasSyncSkips);
				trace_sync("skip-same-contents", 0);
				return;
			}
		}
		const bool lossless = CopyImage(id, other_id, "alias-sync");
		CommitGpuWrite(image);
		if (lossless && AliasSyncSkipEnabled()) {
			image.AdoptContentSerial(m_slot_images[other_id].ContentSerial());
		}
		Profiler::CountFrameEvent(Profiler::FrameEvent::AliasSyncCopies);
		trace_sync("copy", image.info.data.size);
		return;
	}
}

void TextureCache::CommitGpuWrite(Image& image) {
	if (!image.depth_id && image.backing.image == nullptr) {
		EXIT("TextureCache: GPU writes require a native image or stencil association\n");
	}
	image.ClearBufferModified();
	if (image.IsCpuDirty()) {
		image.RefreshComplete();
	}
	// Single owner among live aliases: this write supersedes the bytes of every other image
	// overlapping it, so none of them may later be downloaded over it. Their native contents
	// stay as they are (the previous rule freed them without a download). Scanned only when
	// ownership changes, not on every bind.
	if (AliasAgeByFrames() && !image.alias_owner && !image.depth_id && !image.info.data.Empty()) {
		for (const auto id: FindImagesInRegion(image.info.data.address, image.info.data.size, false)) {
			auto* other = m_slot_images.try_get(id);
			if (other == nullptr || other == &image || other->depth_id || !other->registered ||
			    !other->Overlaps(image.info.data.address, image.info.data.size, false)) {
				continue;
			}
			other->alias_owner = false;
			if (other->IsGpuModified()) {
				other->ClearGpuModified();
				InvalidateCleanImageProofs(other->live.address, other->live.size,
				                           Coherence::Source::ImageGpuClear);
			}
		}
		image.alias_owner = true;
	}
	MarkImageGpuModified(image);
}

bool TextureCache::ClearImageFromBuffer(CommandBuffer& command, uint64_t address, uint64_t size,
                                        uint32_t packed_clear) {
	KYTY_GPU_OP_SITE("texcache.clear_from_buffer");
	if (command.IsInvalid() || !GuestRange {address, size}.Valid()) {
		EXIT("TextureCache: invalid image clear\n");
	}
	std::scoped_lock     lock {m_lock};
	ImageId              selected {};
	vk::ImageAspectFlags aspect {};
	for (const auto id: FindImagesInRegion(address, size, false)) {
		auto owner = m_slot_images.try_get(id);
		if (owner == nullptr) {
			continue;
		}
		vk::ImageAspectFlags candidate {};
		ImageId              candidate_id = id;
		if (owner->depth_id && owner->info.data.address == address &&
		    owner->info.data.size == size) {
			candidate    = vk::ImageAspectFlagBits::eStencil;
			candidate_id = owner->depth_id;
			owner        = m_slot_images.try_get(candidate_id);
			if (owner == nullptr || owner->backing.image == nullptr || !owner->info.HasStencil()) {
				continue;
			}
		} else if (!owner->depth_id && owner->info.data.address == address &&
		           owner->info.data.size == size) {
			candidate = owner->info.IsDepth() ? vk::ImageAspectFlagBits::eDepth
			                                  : vk::ImageAspectFlagBits::eColor;
		}
		if (!candidate) {
			continue;
		}
		if (selected && selected != candidate_id) {
			return false;
		}
		selected = candidate_id;
		aspect   = candidate;
	}
	if (!selected) {
		return false;
	}
	auto&          image = m_slot_images[selected];
	vk::ClearValue clear {};
	if (aspect == vk::ImageAspectFlagBits::eColor) {
		if (!DecodePackedColorClear(image.info.pixel_format, packed_clear, clear.color)) {
			return false;
		}
	} else {
		uint8_t stencil_clear = 0;
		if ((aspect == vk::ImageAspectFlagBits::eDepth &&
		     !DecodePackedDepthClear(image.info.pixel_format, packed_clear, clear.depthStencil.depth)) ||
		    (aspect == vk::ImageAspectFlagBits::eStencil &&
		     !DecodePackedStencilClear(packed_clear, stencil_clear))) {
			return false;
		}
		clear.depthStencil.stencil = stencil_clear;
	}
	ClearImage(command, selected, image.backing.format,
	           {aspect, 0, image.info.resources.levels, 0, image.info.TransferLayers()}, clear);
	return true;
}

void TextureCache::ClearImage(CommandBuffer& command, ImageId id, vk::Format format,
                              const vk::ImageSubresourceRange& range, const vk::ClearValue& clear) {
	KYTY_GPU_OP_SITE("texcache.clear");
	// Levels outside the cleared range must hold guest data, and the result is GPU-owned.
	RequireFullResidency(id);
	auto& image = m_slot_images[id];
	const auto aspects = image.info.IsDepth() ? ImageViewOps::DepthAspectMask(image.backing.format)
	                                          : vk::ImageAspectFlagBits::eColor;
	EXIT_IF(range.baseMipLevel >= image.info.resources.levels);
	const auto layers = image.info.IsVolume()
	                        ? std::max(image.info.extent.depth >> range.baseMipLevel, 1u)
	                        : image.backing.layers;
	EXIT_IF(command.IsInvalid() || image.depth_id || !range.aspectMask || range.levelCount == 0 ||
	        range.levelCount > image.info.resources.levels - range.baseMipLevel ||
	        range.layerCount == 0 || range.baseArrayLayer >= layers ||
	        range.layerCount > layers - range.baseArrayLayer ||
	        (range.aspectMask & aspects) != range.aspectMask);
	const bool full_subresources = range.baseMipLevel == 0 &&
	                               range.levelCount == image.info.resources.levels &&
	                               range.baseArrayLayer == 0 && range.layerCount == layers;
	const bool full_image = range.aspectMask == aspects && full_subresources;
	TrackImage(id);
	if (!full_image && (image.IsBufferModified() || image.IsCpuDirty())) {
		InitializeImage(id);
		if (image.info.samples == 1 && (image.IsBufferModified() || image.IsCpuDirty())) {
			EXIT("TextureCache: image clear retained guest ownership\n");
		}
	}
	if (image.info.HasStencil() && (range.aspectMask & vk::ImageAspectFlagBits::eStencil)) {
		const auto stencil_id = AssociateStencil(id, image.info.stencil);
		if (!full_subresources) {
			RefreshImage(stencil_id);
		} else {
			TrackImage(stencil_id);
			CommitGpuWrite(m_slot_images[stencil_id]);
		}
	}
	command.EndRendering();
	// Transfer clears use the backing format; aliased clears must encode through their view.
	if (format != image.backing.format || (image.info.IsVolume() && !full_image)) {
		EXIT_NOT_IMPLEMENTED(range.aspectMask != vk::ImageAspectFlagBits::eColor ||
		                     range.levelCount != 1);
		ImageViewInfo view {};
		view.format = format;
		view.type   = range.layerCount == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
		view.base_level  = range.baseMipLevel;
		view.base_layer  = range.baseArrayLayer;
		view.layer_count = range.layerCount;
		view.usage       = vk::ImageUsageFlagBits::eColorAttachment;
		image.Transit(vk::ImageLayout::eColorAttachmentOptimal,
		              vk::AccessFlagBits2::eColorAttachmentWrite, {}, command.Handle());
		vk::RenderingAttachmentInfo attachment {};
		attachment.imageView   = image.FindView(view);
		attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachment.loadOp      = vk::AttachmentLoadOp::eClear;
		attachment.storeOp     = vk::AttachmentStoreOp::eStore;
		attachment.clearValue  = clear;
		vk::RenderingInfo rendering {};
		rendering.renderArea.extent = {
		    std::max(image.info.extent.width >> range.baseMipLevel, 1u),
		    std::max(image.info.extent.height >> range.baseMipLevel, 1u)};
		rendering.layerCount           = range.layerCount;
		rendering.colorAttachmentCount = 1;
		rendering.pColorAttachments    = &attachment;
		command.Handle().beginRendering(&rendering);
		command.Handle().endRendering();
		image.NoteContentWrite();
		CommitGpuWrite(image);
		return;
	}
	image.Transit(vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits2::eTransferWrite, {},
	              command.Handle());
	auto native_range = range;
	if (image.info.IsVolume()) {
		native_range.baseArrayLayer = 0;
		native_range.layerCount     = 1;
	}
	if (range.aspectMask == vk::ImageAspectFlagBits::eColor) {
		command.Handle().clearColorImage(image.backing.image, vk::ImageLayout::eTransferDstOptimal,
		                                 &clear.color, 1, &native_range);
	} else {
		command.Handle().clearDepthStencilImage(image.backing.image,
		                                        vk::ImageLayout::eTransferDstOptimal,
		                                        &clear.depthStencil, 1, &native_range);
	}
	image.NoteContentWrite();
	CommitGpuWrite(image);
}

void TextureCache::InvalidateMemory(uint64_t address, uint64_t size) {
	if (!GuestRange {address, size}.Valid()) {
		EXIT("TextureCache: invalid memory-invalidation range\n");
	}
	if (m_fault_fast_path) {
		// Guest write faults on pages only buffers watch must not wait for the texture-cache
		// lock, which the GPU thread holds across refreshes. An image watches a page only while
		// registered on its 1 MiB page (counted before tracking starts, and after it ends).
		ImagePageTable::PageRange pages {};
		bool                      covered = !ImagePageTable::TryGetPageRange(address, size, pages);
		for (auto page = pages.first; !covered && page < pages.last_exclusive; ++page) {
			covered = m_image_page_counts[page].load() != 0;
		}
		if (!covered) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureInvalidateSkips);
			return;
		}
	}
	std::scoped_lock lock {m_lock};
	InvalidateCpuAliases(address, size);
}

void TextureCache::DownloadDepth(Image& image, Buffer& destination, uint64_t destination_offset) {
	const auto&    info             = image.info;
	const auto     layers           = info.resources.layers;
	const auto     full_slice_size  = info.data.size / layers;
	const auto     transfer_bytes   = DepthAspectTransferBytes(info.pixel_format);
	const uint64_t texels_per_slice = static_cast<uint64_t>(info.pitch) * info.extent.height;
	EXIT_NOT_IMPLEMENTED(transfer_bytes == 0 || texels_per_slice > UINT32_MAX ||
	                     texels_per_slice > UINT64_MAX / transfer_bytes ||
	                     texels_per_slice > UINT64_MAX / info.bytes_per_block);
	const uint64_t transfer_slice = texels_per_slice * transfer_bytes;
	const uint64_t guest_slice    = texels_per_slice * info.bytes_per_block;
	EXIT_NOT_IMPLEMENTED(transfer_slice > UINT64_MAX / layers);
	const uint64_t transfer_size = transfer_slice * layers;
	EXIT_NOT_IMPLEMENTED(guest_slice > full_slice_size);
	auto copies = BuildDepthCopies(info, full_slice_size, vk::ImageAspectFlagBits::eDepth);
	if (transfer_bytes == info.bytes_per_block) {
		if (!info.IsTiled()) {
			for (auto& copy: copies) {
				copy.bufferOffset += destination_offset;
			}
			image.Download(copies, destination.Handle(), destination_offset, info.data.size);
			return;
		}
		const auto tiles = BuildDepthTiles(info);
		m_tiler.TileImage(image, copies, destination.Handle(), destination_offset, info.data.size,
		                  info.data.size, tiles);
		return;
	}
	EXIT_NOT_IMPLEMENTED(info.bytes_per_block != sizeof(uint16_t) ||
	                     transfer_bytes != sizeof(uint32_t));
	for (uint32_t layer = 0; layer < layers; layer++) {
		copies[layer].bufferOffset = transfer_slice * layer;
	}
	auto host_linear = m_tiler.GetScratchBuffer(transfer_size);
	image.Download(copies, host_linear.buffer, 0, host_linear.size);
	const bool tiled        = info.IsTiled();
	auto       guest_linear = tiled ? m_tiler.GetScratchBuffer(info.data.size)
	                                : TileManager::Result {destination.Handle(), destination_offset,
	                                                       destination.Size() - destination_offset};
	m_tiler.ConvertD16(host_linear, guest_linear, TileManager::D16Direction::Demote,
	                   DepthAspectTransferFormat(info.pixel_format) == vk::Format::eD32Sfloat,
	                   {.width               = info.extent.width,
	                    .height              = info.extent.height,
	                    .layers              = layers,
	                    .source_row_stride   = static_cast<uint64_t>(info.pitch) * sizeof(uint32_t),
	                    .target_row_stride   = static_cast<uint64_t>(info.pitch) * sizeof(uint16_t),
	                    .source_slice_stride = transfer_slice,
	                    .target_slice_stride = full_slice_size});
	if (!tiled) {
		return;
	}
	const auto tiles = BuildDepthTiles(info);
	m_tiler.Tile(guest_linear.buffer, guest_linear.offset, info.data.size, destination.Handle(),
	             destination_offset, info.data.size, tiles);
}

void TextureCache::DownloadImage(Image& image, Buffer& destination, uint64_t destination_offset,
                                     uint64_t destination_size, ImageDownload transfer) {
	if (!transfer.valid) {
		EXIT("TextureCache: invalid image download transfer\n");
	}
	if (transfer.depth_target) {
		if (destination_size != image.info.data.size) {
			EXIT("TextureCache: partial depth image download is unsupported\n");
		}
		DownloadDepth(image, destination, destination_offset);
		return;
	}

	auto&      texture   = transfer.texture;
	const auto transform = texture.swap_bgra16 ? TileManager::ColorTransform::SwapBgra16
	                                           : TileManager::ColorTransform::None;
	if (texture.tiles.empty()) {
		if (transform == TileManager::ColorTransform::SwapBgra16) {
			auto linear = m_tiler.GetScratchBuffer(destination_size);
			image.Download(texture.regions, linear.buffer, 0, linear.size);
			m_tiler.SwapBgra16(linear,
			                   {destination.Handle(), destination_offset, destination_size});
			return;
		}
		for (auto& copy: texture.regions) {
			copy.bufferOffset += destination_offset;
		}
		image.Download(texture.regions, destination.Handle(), destination_offset, destination_size);
		return;
	}

	m_tiler.TileImage(image, texture.regions, destination.Handle(), destination_offset,
	                  destination_size, texture.LinearSize(), texture.tiles, transform);
}

bool BufferCache::SynchronizeBufferFromImage(Buffer& buffer, uint64_t vaddr, uint64_t size) {
	const auto selected = m_texture_cache.FindImageFromRange(vaddr, size);
	if (!selected) {
		return false;
	}

	std::scoped_lock lock {m_texture_cache.m_lock};
	auto& image = m_texture_cache.m_slot_images[selected];
	// The GPU thread owns image retirement; CPU invalidation can dirty this image after lookup.
	if (!m_texture_cache.SafeToDownload(image)) {
		return false;
	}
	if (!buffer.IsInBounds(image.info.data.address, 1)) {
		return false;
	}
	const auto buf_offset = buffer.Offset(image.info.data.address);
	const auto available  = buffer.Size() - buf_offset;
	uint32_t   levels     = 0;
	uint64_t   copy_size  = 0;
	if (image.info.IsVolume()) {
		// Volume mips contain strided block slices, so a mip's linear span cannot prove that
		// every retained slice fits. Keep volume synchronization whole-image only.
		if (!buffer.IsInBounds(image.info.data.address, image.info.data.size)) {
			return false;
		}
		levels    = image.info.resources.levels;
		copy_size = image.info.data.size;
	} else {
		for (; levels < image.info.resources.levels; ++levels) {
			const auto& mip = image.info.mip_layout[levels];
			if (mip.size == 0 || mip.offset > available || mip.size > available - mip.offset) {
				break;
			}
			copy_size = std::max(copy_size, mip.offset + mip.size);
		}
	}
	if (copy_size == 0) {
		return false;
	}
	// The buffer already holds this image's bytes when neither changed since the last download:
	// the image's content serial is unchanged and the buffer's write revision (which every GPU
	// or CPU-upload write to it advances, and which CPU-dirty pages invalidate) is the one that
	// download produced.
	const bool track_texel_sync = m_texture_cache.m_texel_sync_skip && GuestGpu::IsGpuThread();
	if (track_texel_sync && image.texel_sync.valid && image.ContentSerial() != 0 &&
	    image.texel_sync.serial == image.ContentSerial() && image.texel_sync.size == copy_size) {
		const auto revision = GetContentRevision(image.info.data.address, copy_size);
		if (revision && revision->id == image.texel_sync.buffer &&
		    revision->write_revision == image.texel_sync.revision &&
		    revision->global_epoch == image.texel_sync.epoch &&
		    &m_slot_buffers[revision->id] == &buffer) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::TexelImageSyncSkips);
			return true;
		}
	}
	image.texel_sync.valid = false;
	auto transfer = m_texture_cache.BuildDownload(image);
	if (!transfer.valid) {
		return false;
	}
	if (transfer.depth_target && copy_size != image.info.data.size) {
		return false;
	}
	if (!transfer.depth_target && levels < image.info.resources.levels) {
		auto& texture = transfer.texture;
		std::erase_if(texture.regions, [levels](const vk::BufferImageCopy& region) {
			return region.imageSubresource.mipLevel >= levels;
		});
		if (texture.regions.empty()) {
			return false;
		}
		if (!texture.tiles.empty()) {
			texture.tiles.clear();
			if (!TextureBuildGpuTileInfos(copy_size, texture.regions, texture.layout, levels,
			                              texture.tiles)) {
				return false;
			}
		}
	}
	m_texture_cache.DownloadImage(image, buffer, buf_offset, copy_size, std::move(transfer));
	buffer.MarkContentWritten();
	NoteBufferContentWrite(image.info.data.address, copy_size);
	Profiler::CountFrameEvent(Profiler::FrameEvent::TexelImageSyncDownloads);
	if (track_texel_sync) {
		if (const auto revision = GetContentRevision(image.info.data.address, copy_size);
		    revision && &m_slot_buffers[revision->id] == &buffer) {
			image.texel_sync = {revision->id, revision->write_revision, revision->global_epoch,
			                    image.ContentSerial(), copy_size, true};
		}
	}
	return true;
}

bool TextureCache::DownloadImageMemory(ImageId id) {
	KYTY_GPU_OP_SITE("texcache.download");
	auto& image = m_slot_images[id];
	if (image.depth_id) {
		return false;
	}
	auto transfer = BuildDownload(image);
	if (!transfer.valid || !SafeToDownload(image)) {
		return false;
	}
	const auto range    = image.info.data;
	auto&      download = m_buffer_cache.GetUtilityBuffer(MemoryUsage::Download);
	auto [mapped, offset] =
	    download.Map(range.size, std::max<uint64_t>(image.info.bytes_per_block, 4));
	if (mapped == nullptr) {
		EXIT("TextureCache: failed to map reusable download buffer\n");
	}
	download.Commit();
	if (!LibKernel::Memory::TryReadBacking(range.address, mapped, range.size)) {
		return false;
	}
	download.Flush(offset, range.size);

	DownloadImage(image, download, offset, range.size, std::move(transfer));
	vk::BufferMemoryBarrier barrier {};
	barrier.srcAccessMask = vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eTransferWrite |
	                        vk::AccessFlagBits::eShaderWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer              = download.Handle();
	barrier.offset              = offset;
	barrier.size                = range.size;
	m_scheduler.EndRendering();
	m_scheduler.Current().Handle().pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
	                                               vk::PipelineStageFlagBits::eHost, {}, 0, nullptr,
	                                               1, &barrier, 0, nullptr);
	const auto publication = m_buffer_cache.BeginBackingPublication(
	    std::span<const GuestRange>(&range, 1), m_scheduler.CurrentTick());
	m_scheduler.DeferPriorityOperation([this, &download, range, mapped, offset, publication] {
		download.Invalidate(offset, range.size);
		LibKernel::Memory::WriteBacking(range.address, mapped, range.size);
		m_buffer_cache.EndBackingPublication(publication);
	});
	return true;
}

void TextureCache::InvalidateMemoryFromGPU(uint64_t address, uint64_t size) {
	if (!GuestRange {address, size}.Valid()) {
		return;
	}
	std::scoped_lock lock {m_lock};
	for (const auto id: FindImagesInRegion(address, size, true)) {
		auto& image = m_slot_images[id];
		if (!image.Overlaps(address, size)) {
			continue;
		}
		if (image.IsGpuModified()) {
			// The buffer cache takes ownership of these bytes; its Add bumps as well.
			CleanVerdict::Invalidate(image.live.address, image.live.size,
			                         Coherence::Source::ImageGpuClear);
			image.ClearGpuModified();
		}
		image.MarkBufferModified();
		image.NoteDirtySpan(address, size);
	}
}

uint32_t TextureCache::CountImagesOutsideGpuWrite(uint64_t address, uint64_t size,
                                                  std::span<const GuestRange> written) {
	if (!GuestRange {address, size}.Valid()) {
		return 0;
	}
	std::scoped_lock lock {m_lock};
	uint32_t         count = 0;
	for (const auto id: FindImagesInRegion(address, size, true)) {
		const auto& image = m_slot_images[id];
		if (!image.Overlaps(address, size)) {
			continue;
		}
		const bool hit = std::any_of(written.begin(), written.end(), [&](const GuestRange& range) {
			return image.Overlaps(range.address, range.size);
		});
		count += hit ? 0u : 1u;
	}
	return count;
}

void TextureCache::InvalidateCleanImageProofs(uint64_t address, uint64_t size,
                                              Coherence::Source source) {
	// Every image register/unregister and GPU-modified transition passes here, before the
	// change is made under m_lock. Global clean-read verdicts depend on the same state.
	CleanVerdict::Invalidate(address, size, source);
	if (!m_clean_image_proofs) {
		return;
	}
	// Saturation disables reuse permanently instead of allowing an old epoch to match.
	if (m_clean_image_epoch != UINT64_MAX) {
		++m_clean_image_epoch;
	}
}

void TextureCache::MarkImageGpuModified(Image& image) {
	if (!image.FullyResident()) {
		// Every GPU-write path makes the image fully resident before recording. Reaching this
		// point partially resident would expose undefined levels: count and report it.
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureResidencyViolations);
		if (++m_residency_violations <= 16) {
			LOGF("TextureCache: GPU write to a partially resident image 0x%016" PRIx64
			     " (resident from level %u)\n",
			     image.info.data.address, image.resident_first);
		}
	}
	if (!image.IsGpuModified()) {
		// The ownership predicate (IsRegionGpuModified) covers the registered range `live`.
		InvalidateCleanImageProofs(image.live.address, image.live.size,
		                           Coherence::Source::ImageGpuModified);
	}
	image.MarkGpuModified();
	// Every GPU writer (draw targets, storage bindings, clears, helper passes, copies) passes
	// here before or right after recording its write: the native contents get a new identity.
	image.NotePossibleWrite();
}

TextureCache::ContentMark TextureCache::MarkContent(ImageId id) {
	std::scoped_lock lock {m_lock};
	const auto*      image = m_slot_images.try_get(id);
	return image == nullptr ? ContentMark {}
	                        : ContentMark {image->ContentSerial(), image->DefiniteWrites()};
}

void TextureCache::RestoreContentIfUnwritten(ImageId id, const ContentMark& mark) {
	if (!AliasSyncSkipEnabled() || mark.serial == 0) {
		return;
	}
	std::scoped_lock lock {m_lock};
	auto*            image = m_slot_images.try_get(id);
	// Only bind-time possible writes happened since the mark: no upload, copy or clear.
	if (image != nullptr && image->DefiniteWrites() == mark.definite_writes) {
		image->AdoptContentSerial(mark.serial);
	}
}

bool TextureCache::IsRegionGpuModified(uint64_t address, uint64_t size) {
	if (!GuestRange {address, size}.Valid()) {
		return false;
	}
	std::scoped_lock lock {m_lock};
	// A negative proof covers one complete 4 KiB page, while the owner index uses
	// 1 MiB pages. Dirty neighboring bytes only prevent caching: the exact query
	// below must still succeed for clean subranges. No guest bytes are cached/read.
	constexpr uint64_t proof_size = 4096;
	const auto proof_begin = Common::AlignDown(address, proof_size);
	const bool use_proof = m_clean_image_proofs && m_clean_image_epoch != UINT64_MAX &&
	                       size <= proof_size - (address - proof_begin);
	if (use_proof) {
		const auto page = proof_begin / proof_size;
		auto& proof = m_clean_image_pages[(page ^ (page >> 8)) % m_clean_image_pages.size()];
		if (proof.epoch == m_clean_image_epoch && proof.page == page) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureCleanProofHits);
			return false;
		}
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureCleanProofMisses);
		const auto* owners = m_image_page_table.Find(address >> ImagePageTable::kPageBits);
		bool page_modified = false;
		if (owners != nullptr) {
			for (const auto id: *owners) {
				const auto* image = m_slot_images.try_get(id);
				if (image == nullptr || image->depth_id || !image->IsGpuModified()) {
					continue;
				}
				if (image->Overlaps(address, size, false)) {
					return true;
				}
				page_modified |= image->Overlaps(proof_begin, proof_size, false);
			}
		}
		if (!page_modified) {
			proof = {page, m_clean_image_epoch};
			Profiler::CountFrameEvent(Profiler::FrameEvent::TextureCleanProofStores);
		}
		return false;
	}
	if (m_direct_dirty_image_query) {
		ImagePageTable::PageRange pages {};
		if (!ImagePageTable::TryGetPageRange(address, size, pages)) {
			return false;
		}
		// Scalar SRT reads stay within one page. Keep multi-page queries on the
		// deduplicated path so large clean ranges cannot revisit every owner.
		if (pages.last_exclusive - pages.first == 1) {
			const auto* owners = m_image_page_table.Find(pages.first);
			if (owners == nullptr) {
				return false;
			}
			for (const auto id: *owners) {
				const auto* image = m_slot_images.try_get(id);
				if (image != nullptr && image->Overlaps(address, size, false) &&
				    !image->depth_id && image->IsGpuModified()) {
					return true;
				}
			}
			return false;
		}
	}
	for (const auto id: FindImagesInRegion(address, size, false)) {
		const auto& image = m_slot_images[id];
		if (!image.depth_id && image.IsGpuModified()) {
			return true;
		}
	}
	return false;
}

void TextureCache::InvalidateCpuAliases(uint64_t address, uint64_t size) {
	const auto page_begin = Common::AlignDown(address, TRACKER_PAGE_SIZE);
	const auto page_end   = Common::AlignUp(address + size, TRACKER_PAGE_SIZE);
	for (const auto id: FindImagesInRegion(address, size, true)) {
		auto owner = m_slot_images.try_get(id);
		if (owner == nullptr) {
			continue;
		}
		if (owner->ChunkTracked()) {
			InvalidateChunks(*owner, address, size);
			continue;
		}
		if (owner->Overlaps(address, size)) {
			owner->InvalidateCpuWrite(address, size);
			UntrackImage(id);
			continue;
		}
		const auto image_begin = owner->live.address;
		const auto image_end   = owner->live.End();
		if (page_end < image_end) {
			UntrackImageHead(id);
		} else if (image_begin < page_begin) {
			UntrackImageTail(id);
		} else {
			MarkAsMaybeDirty(id, *owner);
		}
	}
}

bool TextureCache::IsMeta(uint64_t address) {
	std::scoped_lock lock {m_lock};
	const auto       found = m_surface_metas.find(address);
	return found != m_surface_metas.end();
}

bool TextureCache::IsMetaCleared(uint64_t address, uint32_t slice) {
	std::scoped_lock lock {m_lock};
	const auto       found = m_surface_metas.find(address);
	if (found == m_surface_metas.end() || slice >= 32) {
		return false;
	}
	return (found->second.clear_mask & (1u << slice)) != 0;
}

bool TextureCache::ClearMeta(uint64_t address) {
	std::scoped_lock lock {m_lock};
	const auto       found = m_surface_metas.find(address);
	if (found == m_surface_metas.end()) {
		return false;
	}
	found->second.clear_mask = UINT32_MAX;
	return true;
}

bool TextureCache::TouchMeta(uint64_t address, uint32_t slice, bool is_clear) {
	std::scoped_lock lock {m_lock};
	const auto       found = m_surface_metas.find(address);
	if (found == m_surface_metas.end() || slice >= 32) {
		return false;
	}
	if (is_clear) {
		found->second.clear_mask |= 1u << slice;
	} else {
		found->second.clear_mask &= ~(1u << slice);
	}
	return true;
}

void TextureCache::UnmapMemory(uint64_t address, uint64_t size) {
	if (!GuestRange {address, size}.Valid()) {
		EXIT("TextureCache: invalid unmap range\n");
	}
	std::scoped_lock lock {m_lock};
	m_gpu_dcc_inspections.clear();
	for (auto metadata = m_surface_metas.begin(); metadata != m_surface_metas.end();) {
		const auto base = metadata->first;
		if (base >= address && base < address + size) {
			metadata = m_surface_metas.erase(metadata);
		} else {
			++metadata;
		}
	}
	auto images = FindImagesInRegion(address, size, false);
	for (const auto id: images) {
		auto owner = m_slot_images.try_get(id);
		if (owner == nullptr) {
			continue;
		}
		FreeImage(id, HangTrace::ImageFreeReason::Unmap);
	}
	// A partially resident image is registered on its resident prefix only. Its non-resident
	// bytes must stay mapped for a later residency extension: retire it with the mapping.
	// (Unmaps drain the GPU first; a scan of the live images is negligible next to that.)
	std::vector<ImageId> partial;
	m_slot_images.ForEach([&](ImageId id, const Image& image) {
		if (image.registered && !image.FullyResident() &&
		    ImageRangeOverlaps(image.info.data, GuestRange {address, size})) {
			partial.push_back(id);
		}
	});
	for (const auto id: partial) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureResidencyUnmapFrees);
		FreeImage(id, HangTrace::ImageFreeReason::Unmap);
	}
}

void TextureCache::RunGarbageCollector() {
	std::scoped_lock lock {m_lock};
	RetireIdlePartialImages();
	const uint64_t   tick = m_gc_tick++;
	if (m_graphics.CanReportMemoryUsage()) {
		m_total_used_memory = m_graphics.GetDeviceMemoryUsage();
	}
	if ((tick & 63u) == 0 && Profiler::DetailedEnabled() && tracy::ProfilerAvailable()) {
		if (m_graphics.CanReportMemoryUsage()) {
			TracyPlot("ImageCache.DeviceMemoryBytes", static_cast<double>(m_total_used_memory));
		}
		TracyPlot("ImageCache.RegisteredGuestBytes", static_cast<double>(m_registered_image_memory));
		TracyPlot("ImageCache.PendingRetirementBytes", static_cast<double>(m_pressure_retirement_bytes));
	}
	if (m_pressure_gc_enabled) {
		RunPressureGarbageCollector(tick);
		return;
	}
	if (m_total_used_memory < m_trigger_gc_memory) {
		return;
	}
	const auto collect = [&](bool allow_aggressive) {
		bool           pressured  = m_total_used_memory >= m_pressure_gc_memory;
		bool           aggressive = allow_aggressive && m_total_used_memory >= m_critical_gc_memory;
		const uint64_t age       = std::min<uint64_t>(aggressive ? 160 : pressured ? 80 : 16, tick);
		size_t         deletions = aggressive ? 40 : pressured ? 20 : 10;
		std::vector<ImageId> candidates;
		candidates.reserve(deletions);
		// Deleting depth recursively deletes its stencil association, so finish LRU traversal
		// first.
		m_lru_cache.ForEachItemBelow(tick - age, [&](ImageId id) {
			candidates.push_back(id);
			return candidates.size() == deletions;
		});
		for (const auto id: candidates) {
			if (deletions == 0) {
				break;
			}
			--deletions;
			auto owner = m_slot_images.try_get(id);
			if (owner == nullptr || !owner->registered || owner->depth_id) {
				continue;
			}
			if (owner->IsGpuModified()) {
				const bool safe = SafeToDownload(*owner);
				if (safe && owner->info.IsTiled()) {
					continue;
				}
				if (safe && !pressured) {
					continue;
				}
				if (safe && !DownloadImageMemory(id)) {
					continue;
				}
			}
			FreeImage(id, HangTrace::ImageFreeReason::GarbageCollect);
			if (m_total_used_memory < m_critical_gc_memory && aggressive) {
				deletions >>= 2;
				aggressive = false;
			}
			if (m_total_used_memory < m_pressure_gc_memory && pressured) {
				deletions >>= 1;
				pressured = false;
			}
		}
	};
	collect(false);
	if (m_total_used_memory >= m_critical_gc_memory) {
		collect(true);
	}
}

void TextureCache::RunPressureGarbageCollector(uint64_t tick) {
	// Driver heap usage includes images awaiting deferred destruction. Do not evict
	// a second cache's worth of resources while those bytes are already retiring.
	const auto pending_bytes = m_graphics.CanReportMemoryUsage() ? m_pressure_retirement_bytes : 0;
	const auto effective_usage = m_total_used_memory - std::min(m_total_used_memory, pending_bytes);
	const auto plan = m_pressure_gc_policy.Begin(effective_usage, tick);
	if (plan.deletions == 0) {
		return;
	}
	KYTY_PROFILER_BLOCK("TextureCache::PressureGarbageCollection");
	m_scheduler.GetMasterSemaphore().Refresh();
	const auto completed_tick = m_scheduler.GetMasterSemaphore().KnownGpuTick();
	std::vector<ImageId> candidates;
	candidates.reserve(std::min<size_t>(plan.candidates, 256));
	// Complete traversal before deleting depth images and their stencil associations.
	m_lru_cache.ScanItemsBelow(tick, m_pressure_gc_cursor, plan.candidates,
	                          [&](ImageId id) { candidates.push_back(id); });

	uint64_t reclaimed_bytes = 0;
	size_t deletions = 0;
	for (const auto id: candidates) {
		if (deletions >= plan.deletions || reclaimed_bytes >= plan.bytes ||
		    effective_usage - std::min(effective_usage, reclaimed_bytes) <= m_pressure_gc_policy.Low()) {
			break;
		}
		auto* owner = m_slot_images.try_get(id);
		if (owner == nullptr || !owner->registered || owner->depth_id) {
			continue;
		}
		// Retain resources still in use whenever pressure leaves room to do so.
		// The critical path retains the existing deferred timeline retirement.
		if (!plan.critical && owner->tick_accessed_last > completed_tick) {
			continue;
		}
		if (owner->IsGpuModified()) {
			const bool safe = SafeToDownload(*owner);
			if (safe && !plan.critical) {
				continue;
			}
			if (safe && owner->info.IsTiled()) {
				continue;
			}
			if (safe && !DownloadImageMemory(id)) {
				continue;
			}
		}
		const auto before = m_total_used_memory;
		FreeImage(id, HangTrace::ImageFreeReason::PressureCollect);
		const auto accounted = before - m_total_used_memory;
		reclaimed_bytes += accounted;
		++deletions;
	}
	// A download can drain the scheduler mid-collection, running destruction
	// callbacks. Refresh actual heap usage instead of inferring release by tick.
	if (m_graphics.CanReportMemoryUsage()) {
		m_total_used_memory = m_graphics.GetDeviceMemoryUsage();
	}
	const auto pending_after = m_graphics.CanReportMemoryUsage() ? m_pressure_retirement_bytes : 0;
	m_pressure_gc_policy.Complete(tick,
	    m_total_used_memory - std::min(m_total_used_memory, pending_after), deletions != 0);
}

void TextureCache::ProcessDownloadImages() {
	std::scoped_lock lock {m_lock};
	for (const auto id: m_download_images) {
		const auto owner = m_slot_images.try_get(id);
		if (owner != nullptr && owner->registered && owner->IsGpuModified()) {
			(void)DownloadImageMemory(id);
		}
	}
	m_download_images.clear();
}

} // namespace Libs::Graphics
