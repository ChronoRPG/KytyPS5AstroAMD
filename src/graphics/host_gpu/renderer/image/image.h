#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGE_H_

#include "common/alignment.h"
#include "common/assert.h"
#include "common/slotVector.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/image/imageInfo.h"

#include <algorithm>
#include <compare>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace Libs::Graphics {

class Buffer;
class CommandScheduler;
struct ImageTestAccess;

using ImageId = Common::SlotId;

struct CachedImageView {
	ImageViewInfo info;
	vk::ImageView view = nullptr;
};

struct ImageUsage {
	bool texture       = false;
	bool storage       = false;
	bool render_target = false;
	bool depth_target  = false;
	bool video_out     = false;
};

struct ImageBinding {
	vk::ImageLayout  attachment_layout = vk::ImageLayout::eUndefined;
	vk::AccessFlags2 attachment_access;
	bool             is_bound      = false;
	bool             is_target     = false;
	bool             needs_rebind  = false;
	bool             force_general = false;
	bool             shader_write  = false;
};

class Image final {
public:
	Image(GraphicContext& graphics, CommandScheduler& scheduler, const ImageInfo& info);
	~Image();
	KYTY_CLASS_NO_COPY(Image);

	[[nodiscard]] vk::ImageView FindView(const ImageViewInfo& view_info);
	using Barriers = std::vector<vk::ImageMemoryBarrier2>;
	[[nodiscard]] Barriers GetBarriers(vk::ImageLayout                      destination_layout,
	                                   vk::AccessFlags2                     destination_access,
	                                   vk::PipelineStageFlags2              destination_stage,
	                                   std::optional<ImageSubresourceRange> range);
	// deferrable: see CommandBuffer::BatchImageBarriers (render.h); only for callers that record
	// no memory-accessing command through command_buffer before the next barrier flush point.
	void Transit(vk::ImageLayout destination_layout, vk::AccessFlags2 destination_access,
	             std::optional<ImageSubresourceRange> range, vk::CommandBuffer command_buffer,
	             bool deferrable = false);
	void Upload(std::span<const vk::BufferImageCopy> copies, vk::Buffer buffer, uint64_t offset,
	            uint64_t size);
	void Download(std::span<const vk::BufferImageCopy> copies, vk::Buffer buffer, uint64_t offset,
	              uint64_t size);
	void CopyImage(Image& source);
	// vkCmdCopyImage between a depth aspect and a compatible color format (VK_KHR_maintenance8).
	void CopyDepthColorImage(Image& source);
	void Resolve(Image& source, const ImageSubresourceRange& source_range,
	             const ImageSubresourceRange& destination_range);
	void CopyImageWithBuffer(Image& source, Buffer& buffer);
	void CopyMip(Image& source, uint32_t mip, uint32_t layer);

	// Native contents identity. Every recorded write to this image gives it a fresh serial
	// (Image copy/upload/resolve methods here, TextureCache::MarkImageGpuModified for draws,
	// dispatches, clears and helper passes). A bit-exact copy from another image may adopt
	// the source's serial afterwards: equal nonzero serials then prove equal native bits.
	// NoteContentWrite: a write that is recorded (also counted in DefiniteWrites).
	// NotePossibleWrite: a binding that may write (render/depth target, storage image); the
	// caller can restore the previous serial once it knows the binding wrote nothing.
	[[nodiscard]] uint64_t ContentSerial() const noexcept { return m_content_serial; }
	[[nodiscard]] uint64_t DefiniteWrites() const noexcept { return m_definite_writes; }
	void                   NoteContentWrite() noexcept;
	void                   NotePossibleWrite() noexcept;
	void AdoptContentSerial(uint64_t serial) noexcept { m_content_serial = serial; }

	void InvalidateCpuWrite(uint64_t vaddr, uint64_t size) {
		if (ImageRangeOverlaps(info.data.address, info.data.size, vaddr, size)) {
			m_cpu_dirty        = true;
			m_maybe_cpu_dirty  = false;
			m_maybe_hash_valid = false;
			// Whole-image invalidation carries no chunk information.
			m_partial_valid = false;
			NoteDirtySpan(vaddr, size);
		} else if (ImagePageRangesOverlap(info.data.address, info.data.size, vaddr, size)) {
			m_maybe_cpu_dirty = true;
			NoteDirtySpan(vaddr, size);
		}
	}

	// Chunk-granular CPU write tracking (TextureCache, KYTY_TEXTURE_PARTIAL_UPLOAD). Only for
	// page-aligned images, so every write to one of their pages overlaps their bytes. While
	// tracked, the image watches its whole page range except the chunks marked untracked; a
	// chunk is untracked only by a CPU write, which also marks it dirty. Dirty chunks are the
	// guest bytes that may differ from the native contents; they are cleared by a refresh.
	struct ChunkState {
		uint64_t              base        = 0; // AlignDown(data.address, chunk size)
		uint32_t              count       = 0; // 0: whole-image tracking
		uint32_t              shift       = 0;
		uint32_t              dirty_count = 0;
		// Consecutive refreshes that found (nearly) every chunk dirty. Such an image is
		// rewritten whole by the CPU: its next write releases every chunk at once, so it takes
		// one fault per refresh instead of one per chunk.
		uint32_t              full_streak = 0;
		// Refresh cycles since the last chunk-by-chunk cycle; every 8th cycle probes again.
		uint32_t              whole_cycles   = 0;
		bool                  whole_released = false;
		std::vector<uint64_t> dirty;
		std::vector<uint64_t> untracked;
		// KYTY_TEXTURE_PARTIAL_VERIFY=1 only: guest-byte hash of each chunk at the last upload.
		std::vector<uint64_t> hashes;
	};
	[[nodiscard]] bool ChunkTracked() const noexcept { return chunks.count != 0; }
	void               EnableChunkTracking(uint32_t shift) {
		chunks.shift = shift;
		chunks.base  = info.data.address & ~((uint64_t {1} << shift) - 1);
		const auto end   = info.data.End();
		const auto count = ((end - chunks.base) + (uint64_t {1} << shift) - 1) >> shift;
		chunks.count     = static_cast<uint32_t>(count);
		chunks.dirty.assign((count + 63) / 64, 0);
		chunks.untracked.assign((count + 63) / 64, 0);
		chunks.dirty_count = 0;
	}
	[[nodiscard]] static bool ChunkBit(const std::vector<uint64_t>& bits, uint32_t index) noexcept {
		return (bits[index >> 6] >> (index & 63u)) & 1u;
	}
	static void SetChunkBit(std::vector<uint64_t>& bits, uint32_t index) noexcept {
		bits[index >> 6] |= uint64_t {1} << (index & 63u);
	}
	static void ClearChunkBit(std::vector<uint64_t>& bits, uint32_t index) noexcept {
		bits[index >> 6] &= ~(uint64_t {1} << (index & 63u));
	}
	// Records a CPU write to chunk `index` (the caller removes the watch). Returns true when
	// the chunk was clean before.
	bool MarkChunkDirty(uint32_t index) noexcept {
		if (ChunkBit(chunks.dirty, index)) {
			return false;
		}
		SetChunkBit(chunks.dirty, index);
		chunks.dirty_count++;
		return true;
	}
	// A CPU write to guest bytes of this chunk-tracked image: the image needs a refresh.
	void NoteChunkWrite(uint64_t vaddr, uint64_t size) noexcept {
		m_cpu_dirty        = true;
		m_maybe_cpu_dirty  = false;
		m_maybe_hash_valid = false;
		NoteDirtySpan(vaddr, size);
	}
	void ClearChunkDirty() noexcept {
		if (chunks.dirty_count != 0) {
			std::fill(chunks.dirty.begin(), chunks.dirty.end(), 0);
			chunks.dirty_count = 0;
		}
	}
	// Whether any chunk overlapping guest range [vaddr, vaddr + size) is dirty.
	[[nodiscard]] bool ChunkRangeDirty(uint64_t vaddr, uint64_t size) const noexcept {
		if (size == 0 || vaddr < chunks.base) {
			return true;
		}
		const auto first = (vaddr - chunks.base) >> chunks.shift;
		const auto last  = (vaddr + size - 1 - chunks.base) >> chunks.shift;
		if (last >= chunks.count) {
			return true;
		}
		for (auto index = first; index <= last; index++) {
			if (ChunkBit(chunks.dirty, static_cast<uint32_t>(index))) {
				return true;
			}
		}
		return false;
	}
	// True while every native byte outside the dirty chunks equals the detiled guest bytes:
	// set only after an upload from guest memory, cleared by any other write to the image.
	[[nodiscard]] bool PartialValid() const noexcept { return m_partial_valid; }
	void               SetPartialValid(bool valid) noexcept { m_partial_valid = valid; }

	// Transfer attribution (diagnostics only): union of the guest ranges that dirtied this image
	// since its last refresh, clipped to the image, and why it was last refreshed.
	void NoteDirtySpan(uint64_t vaddr, uint64_t size) noexcept {
		const auto begin = std::max(vaddr, info.data.address);
		const auto end   = std::min(vaddr + size, info.data.End());
		if (begin >= end) {
			return;
		}
		if (m_dirty_begin == m_dirty_end) {
			m_dirty_begin = begin;
			m_dirty_end   = end;
		} else {
			m_dirty_begin = std::min(m_dirty_begin, begin);
			m_dirty_end   = std::max(m_dirty_end, end);
		}
	}
	[[nodiscard]] uint64_t DirtySpanBytes() const noexcept { return m_dirty_end - m_dirty_begin; }
	[[nodiscard]] uint64_t DirtySpanBegin() const noexcept { return m_dirty_begin; }
	void                   ClearDirtySpan() noexcept { m_dirty_begin = m_dirty_end = 0; }
	// False until the first refresh (upload, or overwrite of the initial guest contents).
	[[nodiscard]] bool     WasEverUploaded() const noexcept { return m_uploads != 0 || m_refreshed; }
	void                   NoteUpload() noexcept { m_uploads++; }
	[[nodiscard]] bool     DirtyFromEdgeHash() const noexcept { return m_dirty_from_hash; }

	[[nodiscard]] bool IsCpuDirty() const { return m_cpu_dirty || m_maybe_cpu_dirty; }
	[[nodiscard]] bool IsDefinitelyCpuDirty() const { return m_cpu_dirty; }
	[[nodiscard]] bool IsMaybeCpuDirty() const { return m_maybe_cpu_dirty; }
	void               MarkMaybeCpuDirty() {
		if (!m_cpu_dirty) {
			m_maybe_cpu_dirty = true;
		}
	}
	[[nodiscard]] bool NeedsMaybeCpuHash() const {
		return m_maybe_cpu_dirty && !m_maybe_hash_valid;
	}
	void SetMaybeCpuHash(uint64_t hash) {
		if (!NeedsMaybeCpuHash()) {
			EXIT("image cannot initialize maybe-dirty hash\n");
		}
		m_maybe_cpu_hash   = hash;
		m_maybe_hash_valid = true;
	}
	[[nodiscard]] bool ResolveMaybeCpuHash(uint64_t hash) {
		if (!m_maybe_cpu_dirty || !m_maybe_hash_valid || m_cpu_dirty) {
			EXIT("image cannot resolve maybe-dirty hash\n");
		}
		m_maybe_cpu_dirty  = false;
		m_maybe_hash_valid = false;
		m_dirty_from_hash  = hash != m_maybe_cpu_hash;
		m_cpu_dirty |= m_dirty_from_hash;
		if (!m_cpu_dirty) {
			ClearDirtySpan();
		}
		return m_cpu_dirty;
	}

	void RefreshComplete() {
		if (!IsCpuDirty()) {
			EXIT("clean image cannot complete a refresh\n");
		}
		m_cpu_dirty        = false;
		m_maybe_cpu_dirty  = false;
		m_maybe_hash_valid = false;
		m_dirty_from_hash  = false;
		m_refreshed        = true;
		// Also reached without an upload (the image is being overwritten on the GPU); a
		// guest-sourced refresh sets partial validity again afterwards.
		m_partial_valid = false;
		ClearChunkDirty();
		ClearDirtySpan();
	}

	[[nodiscard]] bool IsGpuModified() const noexcept { return m_gpu_modified; }
	void               MarkGpuModified() noexcept {
		m_gpu_modified  = true;
		m_partial_valid = false;
	}
	void               ClearGpuModified() noexcept { m_gpu_modified = false; }

	[[nodiscard]] bool IsBufferModified() const noexcept { return m_buffer_modified; }
	void               MarkBufferModified() noexcept {
		m_buffer_modified = true;
		m_partial_valid   = false;
	}
	void               ClearBufferModified() noexcept { m_buffer_modified = false; }

	[[nodiscard]] bool Overlaps(uint64_t address, uint64_t size,
	                            bool pages = false) const noexcept {
		return pages ? ImagePageRangesOverlap(info.data.address, info.data.size, address, size)
		             : ImageRangeOverlaps(info.data.address, info.data.size, address, size);
	}
	[[nodiscard]] bool SafeToDownload() const noexcept {
		return IsGpuModified() && !IsBufferModified() && !IsCpuDirty();
	}
	[[nodiscard]] bool IsTracked() const noexcept { return track_addr != 0 && track_addr_end != 0; }
	[[nodiscard]] uint64_t AccountedSize() const noexcept {
		return backing.image == nullptr ? 0 : Common::AlignUp(info.data.size, 1024);
	}
	[[nodiscard]] uint64_t HashGuestEdges() const;

	ImageInfo        info;
	VulkanImage      backing;
	std::vector<CachedImageView> views;
	ImageUsage       usage;
	ImageBinding     binding;
	bool             registered     = false;
	mutable uint32_t query_epoch    = 0;
	uint64_t         track_addr     = 0;
	uint64_t         track_addr_end = 0;
	ImageId          depth_id {};
	uint64_t         tick_accessed_last  = 0;
	uint64_t         frame_accessed_last = 0; // presented guest frames, see TextureCache::AdvanceFrame
	size_t           lru_id              = 0;
	// Last GPU writer among overlapping aliases; cleared when another alias takes the bytes.
	bool             alias_owner         = false;
	ChunkState       chunks;
	// Last download into a cache buffer for texel-buffer reads (SynchronizeBufferFromImage):
	// the buffer revision it produced and this image's content serial at the time.
	struct TexelSyncMark {
		Common::SlotId buffer {};
		uint64_t       revision = 0;
		uint64_t       epoch    = 0;
		uint64_t       serial   = 0;
		uint64_t       size     = 0;
		bool           valid    = false;
	};
	TexelSyncMark    texel_sync;

private:
	friend struct ImageTestAccess;

	[[nodiscard]] static vk::ImageAspectFlags FullAspectMask(vk::Format format) noexcept;
	void                                      CopyImageRegions(Image& source);
	[[nodiscard]] static uint32_t             CopyRows(uint64_t row_size, uint32_t rows,
	                                                   uint64_t capacity) noexcept;
	[[nodiscard]] static std::pair<uint32_t, uint32_t>
	SanitizeCopyLayers(const Image& source, const Image& destination, uint32_t depth);

	GraphicContext&   m_graphics;
	CommandScheduler& m_scheduler;
	uint64_t          m_maybe_cpu_hash   = 0;
	bool              m_cpu_dirty        = false;
	bool              m_maybe_cpu_dirty  = false;
	bool              m_maybe_hash_valid = false;
	bool              m_gpu_modified     = false;
	bool              m_buffer_modified  = false;
	bool              m_dirty_from_hash  = false;
	bool              m_refreshed        = false;
	bool              m_partial_valid    = false;
	uint64_t          m_content_serial   = 0;
	uint64_t          m_definite_writes  = 0;
	uint64_t          m_dirty_begin      = 0;
	uint64_t          m_dirty_end        = 0;
	uint32_t          m_uploads          = 0;
};

namespace ImageOps {

void                                 Validate(const ImageInfo& info);
[[nodiscard]] Prospero::BufferFormat RenderTargetTransferFormat(uint32_t bytes_per_element);

} // namespace ImageOps

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGE_H_
