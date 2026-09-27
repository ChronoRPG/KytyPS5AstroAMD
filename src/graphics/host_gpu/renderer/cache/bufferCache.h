#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_BUFFERCACHE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_BUFFERCACHE_H_

#include "common/abi.h"
#include "common/common.h"
#include "common/lruCache.h"
#include "common/slotVector.h"
#include "graphics/host_gpu/memoryTracker.h"
#include "graphics/host_gpu/rangeSet.h"
#include "graphics/host_gpu/renderer/cache/faultManager.h"
#include "graphics/host_gpu/renderer/cache/multiLevelPageTable.h"
#include "graphics/host_gpu/renderer/cache/streamBuffer.h"
#include "graphics/host_gpu/writeTickMap.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace Libs::Graphics {

struct GraphicContext;
class CommandScheduler;
class TextureCache;

using BufferId = Common::SlotId;
inline constexpr BufferId NULL_BUFFER_ID {0};

struct BufferContentRevision {
	BufferId id;
	uint64_t write_revision;
	uint64_t global_epoch;
	bool operator==(const BufferContentRevision&) const noexcept = default;
};

class BufferCache {
public:
	static constexpr uint32_t CACHING_PAGEBITS  = 14;
	static constexpr uint64_t CACHING_PAGESIZE  = uint64_t {1} << CACHING_PAGEBITS;
	static constexpr uint64_t CACHING_NUMPAGES  = uint64_t {1} << (40 - CACHING_PAGEBITS);
	static constexpr uint64_t BDA_PAGETABLE_SIZE =
	    CACHING_NUMPAGES * sizeof(vk::DeviceAddress);

	BufferCache(GraphicContext& graphics, CommandScheduler& scheduler, PageManager& page_manager,
	            TextureCache& texture_cache);
	~BufferCache();
	KYTY_CLASS_NO_COPY(BufferCache);

	// write_fault: a guest write fault on the range (fault-ahead / hot-page policy applies).
	void                   InvalidateMemory(uint64_t vaddr, uint64_t size, bool write_fault = false);
	// Once per completed guest flip (any thread): the frame clock of hot-page detection.
	void                   AdvanceFrame() noexcept;
	// While one is alive (GPU thread), CPU-dirty uploads are only queued on the current command
	// buffer (KYTY_UPLOAD_BATCH, CommandBuffer::RequestUploadCopy); the outermost scope's end
	// records them all behind one barrier. Every command recorded meanwhile through
	// CommandBuffer::Handle() records the queue first, so only commands recorded through a handle
	// obtained BEFORE the scope began must not follow uploads made in it. A scope that queued no
	// upload leaves other pending barriers to the next flush point (KYTY_UPLOAD_BATCH_SCOPED_FLUSH,
	// default on; =0 records them at the scope end as before).
	class UploadBatch {
	public:
		explicit UploadBatch(BufferCache& cache);
		~UploadBatch();
		UploadBatch(const UploadBatch&)            = delete;
		UploadBatch& operator=(const UploadBatch&) = delete;

	private:
		BufferCache& m_cache;
	};
	// Reads use a side copy when every dirty byte they need was written by an already submitted
	// recording (KYTY_READBACK_SIDE_COPY=0 disables it). GPU-thread reads wait for their copy in
	// place (KYTY_READBACK_SIDE_GPU_THREAD=0 makes them drain instead).
	void                   ReadMemory(uint64_t vaddr, uint64_t size, bool is_write = false);
	// Publishes (waiting if necessary) every pending side readback overlapping the range. Any
	// thread; never waits for the current recording. Required before other ownership changes.
	void CompleteSideReadbacks(uint64_t vaddr, uint64_t size);
	void CompleteAllSideReadbacks();
	[[nodiscard]] Buffer&  GetBuffer(BufferId id) { return m_slot_buffers[id]; }
	[[nodiscard]] BufferId FindBuffer(uint64_t vaddr, uint64_t size);
	[[nodiscard]] std::pair<Buffer*, uint64_t> ObtainBuffer(uint64_t vaddr, uint64_t size,
	                                                        bool     is_written,
	                                                        bool     is_texel_buffer = false,
	                                                        BufferId id              = {});
	// A writable binding whose shader can only write `written` (sub-ranges of [vaddr, vaddr +
	// size), e.g. from a write-range proof): the whole range is synchronized as for any binding,
	// but only `written` becomes GPU-owned (dirty, protected, write-ticked).
	[[nodiscard]] std::pair<Buffer*, uint64_t> ObtainWrittenBuffer(uint64_t vaddr, uint64_t size,
	                                                               std::span<const GuestRange> written,
	                                                               BufferId id = {});
	[[nodiscard]] StreamBuffer&                GetUtilityBuffer(MemoryUsage usage) noexcept {
		switch (usage) {
			case MemoryUsage::Upload: return m_staging_buffer;
			case MemoryUsage::Stream: return m_stream_buffer;
			case MemoryUsage::Download: return m_download_buffer;
			case MemoryUsage::DeviceLocal: return m_device_buffer;
		}
		EXIT("BufferCache: invalid utility-buffer usage\n");
	}
	[[nodiscard]] const Buffer* GetGdsBuffer() const noexcept { return &m_gds_buffer; }
	[[nodiscard]] Buffer* GetBdaPageTableBuffer() noexcept { return &m_bda_pagetable_buffer; }
	[[nodiscard]] Buffer* GetFaultBuffer() noexcept { return m_fault_manager.GetFaultBuffer(); }
	[[nodiscard]] std::pair<Buffer*, uint64_t> ObtainBufferForImage(uint64_t vaddr, uint64_t size);
	void FillBuffer(uint64_t vaddr, uint64_t size, uint32_t value, bool is_gds);
	// CP WRITE_DATA to bytes owned by recorded GPU work: records the write (vkCmdUpdateBuffer)
	// at its position in the GPU timeline and returns true; false leaves it to the CPU write.
	[[nodiscard]] bool TryWriteDataGpu(uint64_t vaddr, const uint32_t* data, uint64_t size);
	void CopyBuffer(uint64_t dst_vaddr, uint64_t src_vaddr, uint64_t size, bool dst_gds,
	                bool src_gds);

	// Recorded GPU fills whose result has not been overwritten since: a range written by a
	// uniform-fill dispatch or FillBuffer, forgotten on any later GPU or CPU write to it. Lets
	// consumers learn a fill value (e.g. a DCC clear code) without reading GPU memory back.
	void RecordKnownFill(uint64_t vaddr, uint64_t size, uint32_t value);
	[[nodiscard]] std::optional<uint32_t> KnownFill(uint64_t vaddr, uint64_t size) const;
	// Cache-index and exact dirty-range queries require GPU-thread serialization.
	[[nodiscard]] bool IsRegionRegistered(uint64_t vaddr, uint64_t size);
	[[nodiscard]] bool HasGpuDirtyBytes(uint64_t vaddr, uint64_t size);
	// A native-buffer revision only: callers must separately rule out newer image ownership.
	// No buffer is created or synchronized. CPU-dirty and pending-publication ranges have no token.
	[[nodiscard]] std::optional<BufferContentRevision> GetContentRevision(uint64_t vaddr,
	                                                                    uint64_t size);
	// Invalidate retained results before commands whose writes cannot be bounded to one buffer.
	// Also disables side readbacks until the current recording is submitted.
	void InvalidateContentRevisions();
	// Image content serial (Image::NextContentSerial) taken when the latest unbounded writer was
	// bound: an image whose ContentSerial is larger was written after it.
	[[nodiscard]] uint64_t UnboundedWriteSerial() const noexcept { return m_unbounded_write_serial; }
	// Publications can outlive cache ownership. Registration is on the GPU thread; completion
	// is on the priority worker, and these queries never wait for GPU work or backing writes.
	[[nodiscard]] uint64_t BeginBackingPublication(std::span<const GuestRange> ranges,
	                                               uint64_t tick);
	void EndBackingPublication(uint64_t token);
	[[nodiscard]] bool HasPendingBackingPublication(uint64_t vaddr, uint64_t size) const;
	[[nodiscard]] std::optional<uint64_t> PendingBackingPublicationTick(uint64_t vaddr,
	                                                                  uint64_t size) const;
	[[nodiscard]] bool IsRegionCpuModified(uint64_t vaddr, uint64_t size);
	[[nodiscard]] bool IsRegionGpuModified(uint64_t vaddr, uint64_t size);
	void               ProcessFaultBuffer();
	// Caller holds the mapped-range lock. Cache/tracker iteration remains on the GPU thread.
	void               SynchronizeBdaBuffers(const RangeSet& mapped_ranges);
	// Map/unmap callers may run outside the GPU thread, under the mapped-range lock.
	void               InvalidateBdaSynchronization() noexcept;
	void               RunGarbageCollector();

private:
	friend struct BufferCacheTestAccess;

	bool IsBufferInvalid(BufferId id) const {
		const auto* buffer = m_slot_buffers.try_get(id);
		return buffer == nullptr || buffer->is_deleted;
	}

	using BufferMap = std::map<uint64_t, BufferId>;
	struct OverlapResult {
		BufferMap::iterator first;
		BufferMap::iterator last;
		uint64_t            begin;
		uint64_t            end;
		bool                has_stream_leap;
	};

	using PageTable = MultiLevelPageTable<BufferId, CACHING_PAGEBITS, 40, 16>;
	static_assert(CACHING_PAGESIZE == (uint64_t {1} << PageTable::kPageBits));
	void WriteDataBuffer(Buffer& buffer, uint64_t address, const void* source, uint64_t size);
	void TouchBuffer(const Buffer& buffer);
	[[nodiscard]] OverlapResult ResolveOverlaps(uint64_t vaddr, uint64_t size);
	void JoinOverlap(BufferId new_id, BufferId overlap_id, bool accumulate_stream_score);
	[[nodiscard]] BufferId CreateBuffer(uint64_t vaddr, uint64_t size);
	void                   Register(BufferId id);
	void Unregister(BufferId id);
	template <bool insert>
	void ChangeRegister(BufferId id);
	void DeleteBuffer(BufferId id);
	// A run of hot pages a full BDA pass found inside one scanned buffer (KYTY_BDA_HOT_SYNC).
	struct BdaHotRange {
		BufferId id;
		uint64_t address = 0;
		uint64_t size    = 0;
	};
	struct BdaSyncStats {
		uint64_t scanned_buffers = 0;
		uint64_t upload_bytes    = 0;
		uint64_t upload_copies   = 0;
		// Full passes with KYTY_BDA_HOT_SYNC: the buffer being synchronized and where to record
		// the hot page runs it reports.
		BufferId                  buffer_id;
		std::vector<BdaHotRange>* hot_ranges = nullptr;
	};
	void SynchronizeBuffersInRange(uint64_t vaddr, uint64_t size, BdaSyncStats* stats);
	// KYTY_BDA_HOT_SYNC pass: re-synchronizes only the hot page runs the last full pass recorded.
	// False (nothing done) when a recorded buffer is gone; the caller then scans fully.
	[[nodiscard]] bool SynchronizeBdaHotRanges(BdaSyncStats& stats);
	[[nodiscard]] bool SynchronizeBuffer(Buffer& buffer, uint64_t vaddr, uint64_t size,
	                                     bool is_written, bool is_texel_buffer,
	                                     BdaSyncStats* stats = nullptr,
	                                     const char* upload_reason = nullptr);
	[[nodiscard]] vk::Buffer UploadCopies(Buffer& buffer, std::span<vk::BufferCopy> copies,
	                                      uint64_t total_size, size_t guest_copies = SIZE_MAX,
	                                      const uint8_t* host_data = nullptr,
	                                      uint64_t       host_base = 0);
	// Hot pages (GPU thread). Snapshots each hot page of hot_ranges, appends a copy (reading
	// m_hot_scratch from the first appended srcOffset on) for those that differ from their
	// shadow, and lists pages to return to normal tracking.
	void CollectHotPages(Buffer& buffer, std::span<const GuestRange> hot_ranges,
	                     std::vector<vk::BufferCopy>& copies, uint64_t& total_size,
	                     std::vector<uint64_t>& demote);
	void EraseHotShadows(uint64_t vaddr, uint64_t size);
	// Before (or right after recording) a GPU-side write of the range that tracked GPU ownership
	// does not cover (image downloads into the buffer, unbounded address writers; size 0 = all):
	// returns its hot pages to normal tracking, clean unless their contents changed since their
	// last upload. From then on the ordinary fault tracking decides their next upload.
	void SettleHotPages(uint64_t vaddr, uint64_t size);
	void MaintainHotPages();
	[[nodiscard]] bool SynchronizeBufferFromImage(Buffer& buffer, uint64_t vaddr, uint64_t size);
	// Records a download of a GPU-modified image into `buffer` at the image's own guest address
	// (every mip level that fits). Caller holds the texture-cache lock and has checked that the
	// image may be downloaded. Returns the bytes covered from the image start, 0 when nothing
	// was recorded. skip_unchanged (texel reads, KYTY_TEXEL_SYNC_SKIP): record nothing when
	// neither the image nor the buffer changed since the last download (sets *skipped).
	[[nodiscard]] uint64_t RecordImageDownload(Buffer& buffer, Common::SlotId image_id,
	                                           bool skip_unchanged = false,
	                                           bool* skipped       = nullptr);
	// A GPU write about to own [vaddr, vaddr + size) of buffer `id` takes GPU ownership away from
	// every overlapping GPU-modified image (TextureCache::InvalidateMemoryFromGPU), after which
	// the image is rebuilt from the buffer. Moves each such image's contents into the buffer
	// first and makes those bytes GPU-owned, so neither the rebuild nor a CPU readback sees the
	// stale guest bytes (KYTY_IMAGE_WRITEBACK_ON_GPU_WRITE=0 disables). GPU thread, before the
	// writer is recorded and before InvalidateMemoryFromGPU.
	void PreserveImagesForGpuWrite(BufferId id, uint64_t vaddr, uint64_t size);
	[[nodiscard]] static bool ImageWritebackOnGpuWriteEnabled();
	// Queues backing publication; callers wait before clearing dirty pages or reusing their data.
	[[nodiscard]] bool DownloadBufferMemory(Buffer& buffer, uint64_t vaddr, uint64_t size);
	struct ReadMemoryTrace {
		uint64_t begin      = 0;
		uint64_t size       = 0;
		bool     downloaded = false;
	};
	// The drain path: GPU thread only.
	void ReadMemoryDrain(uint64_t vaddr, uint64_t size, bool is_write, ReadMemoryTrace& trace);

	// Side readbacks. Issue runs on the GPU thread; completion on any thread.
	struct SideReadback;
	struct SideReadbackState;
	enum class SideIssueResult : uint8_t {
		Issued,
		Pending,
		CurrentWriter,
		Unbounded,
		Other,
	};
	[[nodiscard]] SideIssueResult TryIssueSideReadback(uint64_t vaddr, uint64_t size,
	                                                  std::shared_ptr<SideReadback>& issued);
	void CompleteSideReadback(SideReadback& readback);
	[[nodiscard]] bool OverlapsPendingSideReadback(uint64_t begin, uint64_t end) const;
	// Every GPU-side write of cached buffer contents for a guest range (GPU thread).
	void NoteBufferContentWrite(uint64_t vaddr, uint64_t size);

	struct BackingPublication {
		uint64_t                token;
		uint64_t                tick;
		std::vector<GuestRange> ranges;
	};
	mutable std::mutex               m_backing_publication_mutex;
	std::vector<BackingPublication> m_backing_publications;
	std::atomic<size_t>              m_backing_publication_count {0};
	uint64_t                        m_next_backing_publication_token = 0;

	GraphicContext&                                   m_graphics;
	CommandScheduler&                                 m_scheduler;
	FaultManager                                      m_fault_manager;
	Buffer                                            m_gds_buffer;
	Buffer                                            m_bda_pagetable_buffer;
	Common::SlotVector<Buffer>                        m_slot_buffers;
	Common::LeastRecentlyUsedCache<BufferId, uint64_t> m_lru_cache;
	BufferMap                                         m_buffers;
	PageTable                                         m_page_table;
	RangeSet                                          m_gpu_modified_ranges;
	struct KnownFillRange {
		uint64_t address = 0;
		uint64_t size    = 0;
		uint32_t value   = 0;
	};
	void                                              ForgetKnownFills(uint64_t vaddr, uint64_t size);
	void                                              ForgetKnownFillsLocked(uint64_t vaddr, uint64_t size);
	mutable std::mutex                                m_known_fill_mutex;
	std::vector<KnownFillRange>                       m_known_fills;
	// m_known_fills is nonempty; written under m_known_fill_mutex, read without it.
	std::atomic<bool>                                 m_has_known_fills {false};
	const bool                                        m_bda_incremental_sync;
	// KYTY_BDA_HOT_SYNC (default on; needs KYTY_BDA_INCREMENTAL_SYNC=1): with hot pages present,
	// a BDA pass whose fault and structure epochs are unchanged re-synchronizes only the hot page
	// runs the last full pass recorded (m_bda_hot_ranges) instead of every mapped buffer, and hot
	// pages are compared with their shadow in place before any snapshot is taken.
	const bool                                        m_bda_hot_sync;
	MemoryTracker                                     m_memory_tracker;
	// Hot pages: exact copy of the last contents uploaded for each hot page (GPU thread only).
	// While a page is hot its buffer bytes equal this copy: every other write of them either
	// returns the page to normal tracking first (written uploads, SettleHotPages for image
	// downloads and address writers, untracking) or keeps the bytes (joins copy them). A normal
	// upload of the page erases the copy.
	struct HotShadow {
		std::unique_ptr<uint8_t[]> data;
		uint32_t                   last_change = 0;
		uint32_t                   last_use    = 0;
	};
	std::map<uint64_t, HotShadow>                     m_hot_shadows;
	std::vector<uint8_t>                              m_hot_scratch;
	uint32_t                                          m_hot_quiet_frames = 8;
	uint32_t                                          m_upload_batch_depth = 0;
	uint32_t                                          m_hot_sweep_frame  = 0;
	std::atomic_uint64_t                               m_bda_structure_epoch {1};
	// GPU-thread-only snapshots taken BEFORE the last full scan, never after it.
	uint64_t                                          m_bda_scanned_cpu_epoch = 0;
	uint64_t                                          m_bda_scanned_structure_epoch = 0;
	// Hot page runs inside the buffers the last full BDA pass scanned (KYTY_BDA_HOT_SYNC; GPU
	// thread). Valid while the epochs of that pass hold: a page can only become hot through a
	// write fault, which changes the fault epoch, and buffers only change with the structure one.
	std::vector<BdaHotRange>                          m_bda_hot_ranges;
	StreamBuffer                                      m_staging_buffer;
	StreamBuffer                                      m_stream_buffer;
	StreamBuffer                                      m_download_buffer;
	StreamBuffer                                      m_device_buffer;
	TextureCache&                                     m_texture_cache;
	uint64_t                                          m_total_used_memory  = 0;
	uint64_t m_trigger_gc_memory  = 1ull * 1024 * 1024 * 1024;
	uint64_t m_critical_gc_memory = 2ull * 1024 * 1024 * 1024;
	uint64_t m_gc_tick            = 0;
	uint64_t m_content_revision_epoch = 1;
	// Writer ticks of buffer contents (GPU thread). Missing ranges are no newer than the floor.
	WriteTickMap m_write_ticks;
	uint64_t     m_write_tick_floor      = 0;
	size_t       m_write_tick_prune_size = 1024;
	// Recording tick that holds an unbounded (address) GPU writer; 0 when none.
	uint64_t     m_unbounded_write_tick  = 0;
	uint64_t     m_unbounded_write_serial = 0;
	std::unique_ptr<SideReadbackState> m_side;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_BUFFERCACHE_H_
