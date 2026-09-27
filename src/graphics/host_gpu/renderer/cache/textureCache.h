#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_TEXTURECACHE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_TEXTURECACHE_H_

#include "common/hangTrace.h"
#include "common/abi.h"
#include "common/common.h"
#include "common/lruCache.h"
#include "common/profiler.h"
#include "common/slotVector.h"
#include "graphics/host_gpu/pageManager.h"
#include "graphics/host_gpu/regionManager.h"
#include "graphics/host_gpu/renderer/cache/bufferCache.h"
#include "graphics/host_gpu/renderer/cache/imageCacheGcPolicy.h"
#include "graphics/host_gpu/renderer/cache/multiLevelPageTable.h"
#include "graphics/host_gpu/renderer/image/blitHelper.h"
#include "graphics/host_gpu/renderer/image/image.h"
#include "graphics/host_gpu/renderer/image/tiler.h"

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Libs::Graphics {

struct GraphicContext;
class Buffer;
class BufferCache;
class CommandBuffer;
class CommandScheduler;
class DccClearHelper;
class RenderExecutor;
class StagingCopier;
class TextureBindingMemo;
struct TextureCacheTestAccess;

class TextureCache {
public:
	enum class BindingType : uint8_t { Texture, Storage, RenderTarget, DepthTarget, VideoOut };

	struct ImageDesc {
		ImageInfo     info;
		ImageViewInfo view_info;
		BindingType   type = BindingType::Texture;
	};

	TextureCache(GraphicContext& graphics, CommandScheduler& scheduler, PageManager& page_manager,
	             BufferCache& buffer_cache);
	~TextureCache();
	KYTY_CLASS_NO_COPY(TextureCache);

	[[nodiscard]] ImageId       FindImage(ImageDesc& desc, bool exact_format = false);
	void                        UpdateImage(ImageId id);
	[[nodiscard]] ImageId       FindImageFromRange(uint64_t address, uint64_t size,
	                                               bool ensure_valid = true);
	[[nodiscard]] vk::ImageView FindTexture(ImageId id, const ImageDesc& desc);
	[[nodiscard]] vk::ImageView FindRenderTarget(ImageId id, const ImageDesc& desc);
	[[nodiscard]] vk::ImageView FindDepthTarget(ImageId id, const ImageDesc& desc);
	[[nodiscard]] Image&        GetImage(ImageId id) {
		auto& image = m_slot_images[id];
		TouchImage(image);
		return image;
	}
	void MarkGpuWritten(ImageId id);

	// Content identity around an attachment binding (see Image::ContentSerial). Take a mark
	// before FindDepthTarget; once the draw is known not to write the attachment, restoring it
	// keeps the image's serial when nothing else (upload, copy, clear) wrote the image meanwhile.
	struct ContentMark {
		uint64_t serial          = 0;
		uint64_t definite_writes = 0;
	};
	[[nodiscard]] ContentMark MarkContent(ImageId id);
	void                      RestoreContentIfUnwritten(ImageId id, const ContentMark& mark);

	[[nodiscard]] bool ClearImageFromBuffer(CommandBuffer& command, uint64_t address, uint64_t size,
	                                        uint32_t packed_clear);
	void               InvalidateMemory(uint64_t address, uint64_t size);
	void               InvalidateMemoryFromGPU(uint64_t address, uint64_t size);
	// Diagnostics only (changes nothing): images that InvalidateMemoryFromGPU(address, size) would
	// invalidate but that no range in `written` overlaps.
	[[nodiscard]] uint32_t CountImagesOutsideGpuWrite(uint64_t address, uint64_t size,
	                                                  std::span<const GuestRange> written);
	[[nodiscard]] bool IsRegionGpuModified(uint64_t address, uint64_t size);

	[[nodiscard]] bool IsMeta(uint64_t address);
	[[nodiscard]] bool IsMetaCleared(uint64_t address, uint32_t slice);
	[[nodiscard]] bool ClearMeta(uint64_t address);
	[[nodiscard]] bool TouchMeta(uint64_t address, uint32_t slice, bool is_clear);

	void UnmapMemory(uint64_t address, uint64_t size);
	void ProcessDownloadImages();
	// Called once per completed guest flip. Alias lifetimes are measured in presented frames:
	// a frame spans hundreds of scheduler ticks, so tick age alone treats every alias used
	// earlier in the same frame as stale.
	void AdvanceFrame() noexcept { m_frame.fetch_add(1, std::memory_order_relaxed); }
	void RunGarbageCollector();

private:
	enum class TransferDirection { Upload, Download };
	enum class ImageLookupMode { FirstPage, Legacy, Verify };
	struct TextureTransfer;
	struct ImageDownload;

	struct MetaDataInfo {
		enum class Type : uint8_t { CMask, FMask, HTile };

		Type     type;
		uint32_t clear_mask = UINT32_MAX;
	};

	struct OverlapResult {
		ImageId image;
		int32_t mip   = -1;
		int32_t layer = -1;
	};

	struct GpuDccInspection {
		GuestRange metadata;
		BufferContentRevision revision;
		// Clear codes (DccClearHelper::ClearCodes bits) the inspected interpretation accepted.
		uint32_t decodable_mask = 0;
	};

	using ImageIds       = InlinePageOwnerList<ImageId, 16>;
	using ImagePageTable = MultiLevelPageTable<ImageIds, 20, 40, 10>;

	// Callers have validated the nonempty 40-bit range with TryGetPageRange.
	template <typename Func>
	static void ForEachPage(uint64_t address, size_t size, Func&& func) {
		using FuncReturn = typename std::invoke_result<Func, uint64_t>::type;
		static constexpr bool RETURNS_BOOL = std::is_same_v<FuncReturn, bool>;
		const uint64_t page_end = (address + size - 1) >> ImagePageTable::kPageBits;
		for (uint64_t page = address >> ImagePageTable::kPageBits; page <= page_end; ++page) {
			if constexpr (RETURNS_BOOL) {
				if (func(page)) {
					break;
				}
			} else {
				func(page);
			}
		}
	}

	// resident_first > 0: register only the prefix holding levels [resident_first, levels)
	// (resident_prefix bytes, computed when 0); stays fully resident when not applicable.
	[[nodiscard]] ImageId     InsertImage(const ImageInfo& info, uint32_t resident_first = 0,
	                                      uint64_t resident_prefix = 0);
	// Resident mip levels (Image::live, KYTY_TEXTURE_RESIDENT_MIPS). Caller holds m_lock.
	// The finest level a view of `desc` can read, for an image with `levels` levels.
	[[nodiscard]] uint32_t    RequestedFirstLevel(const ImageDesc& desc, uint32_t levels) const;
	[[nodiscard]] uint64_t    ResidentPrefixSize(const ImageInfo& info, uint32_t first_level) const;
	// Makes levels >= first_level resident (re-registration; the next refresh uploads them).
	// `sampling`: requested by a sampled view (counted as an extension, else a fallback).
	void                      EnsureResidency(ImageId id, uint32_t first_level, bool sampling);
	// Whole chain resident and refreshed now, before a non-sampling use records anything.
	void                      RequireFullResidency(ImageId id);
	void                      PoisonNonResidentLevels(Image& image);
	void                      RetireIdlePartialImages();
	[[nodiscard]] ImageId     GetNullImage(const ImageDesc& desc);
	void                      RegisterImage(ImageId id);
	void                      UnregisterImage(ImageId id);
	void                      DeleteImage(ImageId id);
	void                      FreeImage(ImageId id, HangTrace::ImageFreeReason reason =
	                                                    HangTrace::ImageFreeReason::Other);
	void                      TouchImage(Image& image);
	void                      SyncAliasFromOwner(ImageId id);
	void                      TrackImage(ImageId id);
	// Chunk-granular tracking (Image::ChunkState). Caller holds m_lock.
	[[nodiscard]] bool        ChunkTrackingEligible(const Image& image) const;
	void                      TrackChunkImage(Image& image);
	template <bool track>
	uint32_t                  UpdateChunkWatchers(Image& image, uint32_t first, uint32_t last);
	void                      InvalidateChunks(Image& image, uint64_t address, uint64_t size);
	struct PartialUploadResult {
		bool     done        = false;
		uint64_t bytes       = 0; // tiled guest bytes detiled and uploaded
		uint64_t dirty_bytes = 0; // dirty chunk bytes that caused the refresh
	};
	[[nodiscard]] PartialUploadResult TryPartialUpload(Image& image);
	// Whole-image refresh of a tiled sampled texture whose guest bytes are copied to staging
	// by m_staging_copier. False when the ordinary refresh path must be used.
	[[nodiscard]] bool        TryAsyncFullUpload(Image& image);
	void                      RecordChunkHashes(Image& image);
	[[nodiscard]] bool        VerifyCleanChunks(Image& image);
	[[nodiscard]] bool        KeepOverlappedImage(const Image& cached, uint64_t current_frame) const;
	void                      TrackImageHead(ImageId id);
	void                      TrackImageTail(ImageId id);
	void                      UntrackImage(ImageId id);
	void                      UntrackImageHead(ImageId id);
	void                      UntrackImageTail(ImageId id);
	void                      MarkAsMaybeDirty(ImageId id, Image& image);
	// All callers hold m_lock; negative ownership proofs contain no guest values.
	void                      MarkImageGpuModified(Image& image);
	void                      InvalidateCleanImageProofs();
	void                      TrackImageDownload(ImageId id, Image& image);
	[[nodiscard]] static bool SameBacking(const ImageInfo& cached, const ImageInfo& requested,
	                                      bool exact_format);
	[[nodiscard]] static BindingType UploadBinding(const Image& image);
	[[nodiscard]] bool               SafeToDownload(const Image& image);

	// Caller holds m_lock; it also serializes the per-image query epoch.
	[[nodiscard]] ImageIds      FindImagesInRegion(uint64_t address, uint64_t size,
	                                               bool page_overlap) const;
	// Caller holds m_lock. Equal backing ranges must begin in the same indexed page.
	[[nodiscard]] ImageId       FindImageWithSameBacking(const ImageInfo& requested,
	                                                     bool exact_format) const;
	[[nodiscard]] OverlapResult ResolveOverlap(const ImageInfo& requested, BindingType binding,
	                                           ImageId cached, ImageId merged);
	[[nodiscard]] ImageId       ResolveDepthOverlap(const ImageInfo& requested, BindingType binding,
	                                                ImageId cached);
	[[nodiscard]] ImageId       ExpandImage(const ImageInfo& info, ImageId source);
	void                        RefreshImage(ImageId id);
	void                        MaterializeDccClear(ImageId id, const ImageDesc& desc,
	                                                uint32_t metadata_base_layer);
	// Returns DccGpuRecords or DccGpuReuses when the native path handled every slice, otherwise
	// the first failing reason (Profiler::FrameEvent::DccFallback*) for the CPU fallback.
	[[nodiscard]] Profiler::FrameEvent TryMaterializeGpuDccClear(ImageId id, const ImageDesc& desc,
	                                                             uint32_t metadata_base_layer);
	void                        InitializeImage(ImageId id);
	[[nodiscard]] TextureTransfer
	BuildTextureTransfer(const Image& image, BindingType binding, TransferDirection direction) const;
	[[nodiscard]] static TextureTransfer BuildTextureTransfer(const ImageInfo& info,
	                                                          uint32_t backing_samples,
	                                                          BindingType binding,
	                                                          TransferDirection direction);
	// Drops the regions (and tiles) of non-resident levels and packs the detiled scratch.
	void RestrictToResidentLevels(const Image& image, TextureTransfer& transfer) const;
	[[nodiscard]] ImageDownload BuildDownload(const Image& image) const;
	void UploadImage(Image& image, Buffer& source, uint64_t source_offset);
	void DownloadImage(Image& image, Buffer& destination, uint64_t destination_offset,
	                       uint64_t destination_size, ImageDownload transfer);
	void DownloadDepth(Image& image, Buffer& destination, uint64_t destination_offset);
	void CommitGpuWrite(Image& image);
	// Caller holds m_lock. Volume layer ranges select depth slices.
	void ClearImage(CommandBuffer& command, ImageId id, vk::Format format,
	                const vk::ImageSubresourceRange& range, const vk::ClearValue& clear);
	void PrepareImageCopy(Image& image);
	void RefreshCopySource(ImageId id);
	[[nodiscard]] bool CopyD16(Image& destination, Image& source);
	// Depth <-> color reinterpretation without a staging buffer (VK_KHR_maintenance8 copy or a
	// one-pass shader). Returns the path name, or nullptr when nothing was recorded.
	[[nodiscard]] const char* TryDirectReinterpret(Image& destination, Image& source);
	// Returns true when the destination's copied subresources now hold exactly the source's
	// native bits for every subresource of both images (same shape, lossless path).
	bool CopyImage(ImageId destination, ImageId source, const char* context = "other");
	[[nodiscard]] ImageId AssociateStencil(ImageId depth, GuestRange stencil);
	void CopyImageMip(ImageId destination, ImageId source, uint32_t mip, uint32_t layer);
	void ValidateImageDesc(const ImageDesc& desc) const;

	void               InvalidateCpuAliases(uint64_t address, uint64_t size);
	[[nodiscard]] bool DownloadImageMemory(ImageId id);
	void RunPressureGarbageCollector(uint64_t tick);

	GraphicContext&                                   m_graphics;
	CommandScheduler&                                 m_scheduler;
	TrackingSpinLock                                  m_lock;
	PageManager&                                      m_page_manager;
	BlitHelper                                        m_blit_helper;
	TileManager                                       m_tiler;
	BufferCache&                                      m_buffer_cache;
	std::unique_ptr<DccClearHelper>                    m_dcc_clear;
	std::unordered_map<uint64_t, GpuDccInspection>      m_gpu_dcc_inspections;
	uint64_t m_gpu_dcc_attempts = 0;
	uint64_t m_gpu_dcc_records = 0;
	uint64_t m_gpu_dcc_reuses = 0;
	uint64_t m_gpu_dcc_fallbacks = 0;
	Common::SlotVector<Image>                         m_slot_images;
	ImagePageTable                                    m_image_page_table;
	std::unordered_map<vk::Format, ImageId>           m_null_images;
	Common::LeastRecentlyUsedCache<ImageId, uint64_t> m_lru_cache;
	std::unordered_set<ImageId>                       m_download_images;
	std::atomic<uint64_t>                             m_frame {0};
	std::map<uint64_t, MetaDataInfo>                  m_surface_metas;
	uint64_t                                          m_total_used_memory  = 0;
	uint64_t                                          m_registered_image_memory = 0;
	uint64_t                                          m_trigger_gc_memory  = 0;
	uint64_t                                          m_pressure_gc_memory = 1536ull * 1024 * 1024;
	uint64_t         m_critical_gc_memory     = 3ull * 1024 * 1024 * 1024;
	uint64_t         m_gc_tick                = 0;
	ImageCacheGcPolicy m_pressure_gc_policy;
	bool             m_pressure_gc_enabled   = false;
	Common::LeastRecentlyUsedCache<ImageId, uint64_t>::Cursor m_pressure_gc_cursor;
	uint64_t m_pressure_retirement_bytes = 0;
	mutable uint32_t m_image_query_epoch      = 0;
	bool             m_direct_dirty_image_query = false;
	struct CleanImagePageProof {
		uint64_t page = 0;
		uint64_t epoch = 0;
	};
	std::array<CleanImagePageProof, 256> m_clean_image_pages {};
	uint64_t m_clean_image_epoch = 1;
	bool m_clean_image_proofs = false;
	ImageLookupMode  m_image_lookup_mode = ImageLookupMode::FirstPage;
	uint64_t         m_image_lookup_checks = 0;
	uint64_t         m_image_lookup_mismatches = 0;
	bool             m_readback_linear_images = false;
	// Texture streaming (see TextureCache constructor for the environment switches).
	bool             m_partial_upload      = true;
	bool             m_partial_bands       = true;
	bool             m_partial_verify      = false;
	uint32_t         m_chunk_shift         = 16;
	uint64_t         m_overlap_keep_frames = 0;
	// Set by FindImage for the ResolveOverlap calls of one lookup: many live overlapping
	// images already share the requested range, so overlapped images are not kept.
	bool             m_overlap_crowded     = false;
	uint64_t         m_partial_verify_mismatches = 0;
	// KYTY_TEXTURE_ASYNC_STAGING=0: null, staging copies stay on the GPU thread.
	std::unique_ptr<StagingCopier> m_staging_copier;
	// Device-local host-visible (resizable BAR) staging ring for StagingCopier jobs, so detile
	// reads VRAM instead of system memory over PCIe (KYTY_TEXTURE_STAGING_REBAR=0: none).
	std::unique_ptr<StreamBuffer>  m_texture_staging;
	// Registered images per ImagePageTable page, readable without m_lock
	// (KYTY_TEXTURE_FAULT_FAST_PATH=0 always takes the lock).
	std::unique_ptr<std::atomic<uint32_t>[]> m_image_page_counts;
	bool                                     m_fault_fast_path = true;
	enum class ResidencyMode : uint8_t { Off, On, Poison };
	ResidencyMode                            m_residency            = ResidencyMode::On;
	uint64_t                                 m_residency_violations = 0;
	// Partially resident images (stale ids are dropped by the once-per-frame scan).
	std::vector<ImageId>                     m_partial_images;
	uint64_t                                 m_partial_scan_frame   = 0;
	uint64_t                                 m_resident_idle_frames = 30;
	// KYTY_TEXEL_SYNC_SKIP=0 downloads image contents for every texel-buffer read.
	bool                                     m_texel_sync_skip = true;
	[[nodiscard]] StreamBuffer& StagingRing();
	// Structural generation for TextureBindingMemo (pipeline/textureBindingMemo.h): bumped by
	// every change of the page-owner index or of an image's registered flag, i.e. whenever the
	// first-page lookup of FindImage may answer differently. Changing a registered image's
	// SameBacking fields (address, size, extent, resources, samples, block size, tile mode,
	// format, type) in place must bump it too.
	std::atomic<uint64_t> m_binding_generation {1};
	void NoteStructureChange() noexcept {
		m_binding_generation.fetch_add(1, std::memory_order_relaxed);
		Profiler::CountFrameEvent(Profiler::FrameEvent::TextureCacheStructureChanges);
	}

	friend struct TextureCacheTestAccess;
	friend class BufferCache;
	friend class RenderExecutor;
	friend class TextureBindingMemo;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_TEXTURECACHE_H_
