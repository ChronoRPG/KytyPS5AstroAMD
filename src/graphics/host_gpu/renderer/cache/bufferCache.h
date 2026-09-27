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

	void                   InvalidateMemory(uint64_t vaddr, uint64_t size);
	// Guest read faults outside the GPU thread use a side copy when every dirty byte they need
	// was written by an already submitted recording (KYTY_READBACK_SIDE_COPY=0 disables it).
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
	struct BdaSyncStats {
		uint64_t scanned_buffers = 0;
		uint64_t upload_bytes    = 0;
		uint64_t upload_copies   = 0;
	};
	void SynchronizeBuffersInRange(uint64_t vaddr, uint64_t size, BdaSyncStats* stats);
	[[nodiscard]] bool SynchronizeBuffer(Buffer& buffer, uint64_t vaddr, uint64_t size,
	                                     bool is_written, bool is_texel_buffer,
	                                     BdaSyncStats* stats = nullptr,
	                                     const char* upload_reason = nullptr);
	[[nodiscard]] vk::Buffer UploadCopies(Buffer& buffer, std::span<vk::BufferCopy> copies,
	                                      uint64_t total_size);
	[[nodiscard]] bool SynchronizeBufferFromImage(Buffer& buffer, uint64_t vaddr, uint64_t size);
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
	const bool                                        m_bda_incremental_sync;
	MemoryTracker                                     m_memory_tracker;
	std::atomic_uint64_t                               m_bda_structure_epoch {1};
	// GPU-thread-only snapshots taken BEFORE the last full scan, never after it.
	uint64_t                                          m_bda_scanned_cpu_epoch = 0;
	uint64_t                                          m_bda_scanned_structure_epoch = 0;
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
	std::unique_ptr<SideReadbackState> m_side;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_BUFFERCACHE_H_
