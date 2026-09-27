#include "graphics/host_gpu/renderer/cache/bufferCache.h"

#include "common/alignment.h"
#include "common/assert.h"
#include "common/hangTrace.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "common/rendererBatch.h"
#include "graphics/guest_gpu/graphicsRun.h"
#include "graphics/host_gpu/cleanVerdictCache.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/cache/textureCache.h"
#include "graphics/host_gpu/renderer/commandScheduler.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/vulkanCommon.h"
#include "kernel/memory.h"
#include "graphics/host_gpu/renderer/gpuOpProfiler.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace Libs::Graphics {

namespace {

constexpr uint64_t MiB           = 1024 * 1024;
constexpr uint64_t GdsBufferSize = 64 * 1024;

bool IncrementalBdaSyncEnabled() {
	const auto* value = std::getenv("KYTY_BDA_INCREMENTAL_SYNC");
	return value != nullptr && value[0] == '1' && value[1] == '\0';
}

// Guest read faults copy GPU-owned bytes on a side command buffer unless this is "0".
bool SideReadbackEnabled() {
	const auto* value = std::getenv("KYTY_READBACK_SIDE_COPY");
	return value == nullptr || !(value[0] == '0' && value[1] == '\0');
}

// Aligned side-copy window in bytes: a power of two between 4 KiB and 1 MiB (default 64 KiB).
uint64_t SideReadbackWindow() {
	constexpr uint64_t Default = 64 * 1024;
	const auto*        value   = std::getenv("KYTY_READBACK_SIDE_COPY_WINDOW_KB");
	if (value == nullptr) {
		return Default;
	}
	char*      end = nullptr;
	const auto kib = std::strtoull(value, &end, 10);
	if (end == value || *end != '\0' || kib < 4 || kib > 1024 || (kib & (kib - 1)) != 0) {
		return Default;
	}
	return kib * 1024;
}

} // namespace

// One side-copy readback: the exact GPU-dirty bytes of a tracker-page-aligned window, copied
// by a command buffer outside the scheduler's recording. Completion (any thread, exactly once)
// publishes them to the backing and unprotects the window's pages no newer writer re-owned.
struct BufferCache::SideReadback {
	std::mutex              mutex;
	std::atomic<bool>       done {false};
	uint64_t                begin       = 0;
	uint64_t                end         = 0;
	uint64_t                value       = 0;
	uint32_t                slot        = 0;
	uint64_t                publication = 0;
	// Staged at (address - begin) within the slot.
	std::vector<GuestRange> ranges;
};

struct BufferCache::SideReadbackState {
	static constexpr uint32_t SlotCount = 16;
	struct Slot {
		vk::CommandBuffer command = nullptr;
		// Set by the GPU thread at issue, cleared after the slot's publication has read it.
		std::atomic<bool> busy {false};
	};

	SideReadbackState(GraphicContext& context, CommandScheduler& scheduler, uint64_t window_size)
	    : graphics(context), window(window_size) {
		vk::CommandPoolCreateInfo pool_info {};
		pool_info.queueFamilyIndex = graphics.queue_family;
		pool_info.flags            = vk::CommandPoolCreateFlagBits::eTransient |
		                  vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		RequireVulkanSuccess(graphics.device.createCommandPool(&pool_info, nullptr, &pool),
		                     "create side-readback command pool");
		std::array<vk::CommandBuffer, SlotCount> buffers {};
		vk::CommandBufferAllocateInfo            allocate {};
		allocate.commandPool        = pool;
		allocate.level              = vk::CommandBufferLevel::ePrimary;
		allocate.commandBufferCount = SlotCount;
		RequireVulkanSuccess(graphics.device.allocateCommandBuffers(&allocate, buffers.data()),
		                     "allocate side-readback command buffers");
		for (uint32_t index = 0; index < SlotCount; ++index) {
			slots[index].command = buffers[index];
		}
		vk::SemaphoreTypeCreateInfo type_info {};
		type_info.semaphoreType = vk::SemaphoreType::eTimeline;
		type_info.initialValue  = 0;
		vk::SemaphoreCreateInfo create_info {};
		create_info.pNext = &type_info;
		RequireVulkanSuccess(graphics.device.createSemaphore(&create_info, nullptr, &semaphore),
		                     "create side-readback timeline semaphore");
		staging = std::make_unique<Buffer>(graphics, scheduler, MemoryUsage::Download, 0,
		                                   vk::BufferUsageFlagBits::eTransferDst,
		                                   window * SlotCount);
		EXIT_IF(staging->Mapped().empty());
		SetVulkanObjectNameF(graphics.device, staging->Handle(), "Kyty.SideReadbackStaging");
	}

	~SideReadbackState() {
		staging.reset();
		if (semaphore != nullptr) {
			graphics.device.destroySemaphore(semaphore, nullptr);
		}
		if (pool != nullptr) {
			graphics.device.destroyCommandPool(pool, nullptr);
		}
	}

	KYTY_CLASS_NO_COPY(SideReadbackState);

	void Wait(uint64_t target) const {
		uint64_t current = 0;
		RequireVulkanSuccess(graphics.device.getSemaphoreCounterValue(semaphore, &current),
		                     "query side-readback semaphore");
		if (current >= target) {
			return;
		}
		vk::SemaphoreWaitInfo wait_info {};
		wait_info.semaphoreCount = 1;
		wait_info.pSemaphores    = &semaphore;
		wait_info.pValues        = &target;
		RequireVulkanSuccess(graphics.device.waitSemaphores(&wait_info, UINT64_MAX),
		                     "wait for side readback");
	}

	GraphicContext&                            graphics;
	const uint64_t                             window;
	vk::CommandPool                            pool      = nullptr;
	vk::Semaphore                              semaphore = nullptr;
	// GPU thread only: the last signal value handed out.
	uint64_t                                   next_value = 0;
	std::array<Slot, SlotCount>                slots;
	std::unique_ptr<Buffer>                    staging;
	mutable std::mutex                         pending_mutex;
	std::vector<std::shared_ptr<SideReadback>> pending;
	std::atomic<size_t>                        pending_count {0};
};

void BufferCache::WriteDataBuffer(Buffer& buffer, uint64_t address, const void* source,
                                  uint64_t size) {
	auto* bytes = static_cast<const uint8_t*>(source);
	while (size != 0) {
		const auto chunk  = std::min(size, m_staging_buffer.Size());
		const auto offset = m_staging_buffer.Copy(bytes, chunk, 4);
		buffer.CopyFrom(m_scheduler.Current(), m_staging_buffer, offset, buffer.Offset(address),
		                chunk, vk::AccessFlagBits::eHostWrite);
		bytes += chunk;
		address += chunk;
		size -= chunk;
	}
}

void BufferCache::Register(BufferId id) {
	ChangeRegister<true>(id);
}

void BufferCache::Unregister(BufferId id) {
	ChangeRegister<false>(id);
}

template <bool insert>
void BufferCache::ChangeRegister(BufferId id) {
	InvalidateBdaSynchronization();
	auto& buffer = m_slot_buffers[id];
	PageTable::PageRange pages {};
	EXIT_IF(!PageTable::TryGetPageRange(buffer.CpuAddress(), buffer.Size(), pages));
	for (size_t page = pages.first; page < pages.last_exclusive; ++page) {
		if constexpr (insert) {
			m_page_table[page] = id;
		} else {
			m_page_table[page] = {};
		}
	}
	const auto size_pages = pages.last_exclusive - pages.first;
	if constexpr (insert) {
		const auto [it, inserted] = m_buffers.emplace(buffer.CpuAddress(), id);
		(void)it;
		EXIT_IF(!inserted);
		m_total_used_memory += buffer.Size();
		buffer.lru_id = m_lru_cache.Insert(id, m_gc_tick);
		std::vector<vk::DeviceAddress> addresses;
		addresses.reserve(size_pages);
		for (uint64_t i = 0; i < size_pages; ++i) {
			addresses.push_back(buffer.BufferDeviceAddress() + (i << CACHING_PAGEBITS));
		}
		WriteDataBuffer(m_bda_pagetable_buffer, pages.first * sizeof(vk::DeviceAddress),
		                addresses.data(), addresses.size() * sizeof(vk::DeviceAddress));
	} else {
		const auto found = m_buffers.find(buffer.CpuAddress());
		EXIT_IF(found == m_buffers.end() || found->second != id);
		m_buffers.erase(found);
		EXIT_IF(buffer.Size() > m_total_used_memory);
		m_total_used_memory -= buffer.Size();
		m_lru_cache.Free(buffer.lru_id);
		m_bda_pagetable_buffer.Fill(pages.first * sizeof(vk::DeviceAddress),
		                            size_pages * sizeof(vk::DeviceAddress), 0);
		buffer.is_deleted = true;
	}
}

void BufferCache::TouchBuffer(const Buffer& buffer) {
	if (!buffer.is_deleted) {
		m_lru_cache.Touch(buffer.lru_id, m_gc_tick);
	}
}

void BufferCache::DeleteBuffer(BufferId id) {
	if (IsBufferInvalid(id)) {
		return;
	}
	{
		// A pending side copy reads this buffer outside the scheduler's timeline; the deferred
		// erase below is not ordered after it.
		const auto& buffer = m_slot_buffers[id];
		CompleteSideReadbacks(buffer.CpuAddress(), buffer.Size());
	}
	Unregister(id);
	if (m_scheduler.Active()) {
		m_scheduler.DeferOperation([this, id] { m_slot_buffers.erase(id); });
	} else {
		m_slot_buffers.erase(id);
	}
}

bool BufferCache::DownloadBufferMemory(Buffer& buffer, uint64_t vaddr, uint64_t size) {
	// An older side publication of these pages must reach the backing (and settle its pages)
	// before this newer download is queued; otherwise it could overwrite newer bytes.
	CompleteSideReadbacks(vaddr, size);
	KYTY_GPU_OP_SITE("buffercache.download");
	std::vector<vk::BufferCopy> copies;
	uint64_t                    total_size     = 0;
	const auto                  buffer_address = buffer.CpuAddress();
	m_memory_tracker.ForEachDownloadRange<false>(
	    vaddr, size, [&](uint64_t address, uint64_t bytes) noexcept {
		    m_memory_tracker.ValidateGpuDirtyPages(m_gpu_modified_ranges, address, bytes,
		                                           "buffer download");
		    m_gpu_modified_ranges.ForEachInRange(address, bytes, [&](uint64_t start, uint64_t end) {
			    copies.emplace_back(start - buffer_address, total_size, end - start);
			    // Keep packed ranges on separate cache lines, as in shadPS4.
			    total_size += Common::AlignUp(end - start, 64);
		    });
		    // Ownership moves to the backing publication registered below (which bumps again).
		    CleanVerdict::Invalidate(address, bytes, Coherence::Source::BufferDirtySubtract);
		    m_gpu_modified_ranges.Subtract(address, bytes);
	    });
	if (copies.empty()) {
		return false;
	}

	const auto [mapped, offset] = m_download_buffer.Map(total_size, 64);
	if (mapped == nullptr) {
		EXIT("BufferCache: download exceeds 64 MiB staging buffer capacity\n");
	}
	m_download_buffer.Commit();
	for (auto& copy: copies) {
		copy.dstOffset += offset;
	}

	auto& command = m_scheduler.Current();
	command.EndRendering();
	const auto              native = command.Handle();
	vk::BufferMemoryBarrier before {};
	before.srcAccessMask       = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	before.dstAccessMask       = vk::AccessFlagBits::eTransferRead;
	before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.buffer              = buffer.Handle();
	before.offset              = 0;
	before.size                = buffer.Size();
	native.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
	                       vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, 1, &before, 0,
	                       nullptr);
	native.copyBuffer(buffer.Handle(), m_download_buffer.Handle(),
	                  static_cast<uint32_t>(copies.size()), copies.data());

	auto after          = before;
	after.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	after.dstAccessMask = vk::AccessFlagBits::eHostRead;
	after.buffer        = m_download_buffer.Handle();
	after.offset        = offset;
	after.size          = total_size;
	native.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
	                       vk::PipelineStageFlagBits::eAllCommands |
	                           vk::PipelineStageFlagBits::eHost,
	                       {}, 0, nullptr, 1, &after, 0, nullptr);
	std::vector<GuestRange> publication_ranges;
	publication_ranges.reserve(copies.size());
	for (const auto& copy: copies) {
		publication_ranges.push_back({buffer_address + copy.srcOffset, copy.size});
	}
	const auto publication = BeginBackingPublication(publication_ranges, m_scheduler.CurrentTick());
	m_scheduler.DeferPriorityOperation([this, mapped, offset, total_size, buffer_address, publication,
	                                    copies = std::move(copies)] {
		m_download_buffer.Invalidate(offset, total_size);
		for (const auto& copy: copies) {
			Libs::LibKernel::Memory::WriteBacking(buffer_address + copy.srcOffset,
			                                      mapped + (copy.dstOffset - offset), copy.size);
		}
		EndBackingPublication(publication);
	});
	return true;
}

BufferCache::BufferCache(GraphicContext& graphics, CommandScheduler& scheduler,
                         PageManager& page_manager, TextureCache& texture_cache)
    : m_graphics(graphics), m_scheduler(scheduler), m_fault_manager(graphics, scheduler, *this),
      m_gds_buffer(graphics, scheduler, MemoryUsage::Stream, 0, AllFlags, GdsBufferSize),
      m_bda_pagetable_buffer(graphics, scheduler, MemoryUsage::DeviceLocal, 0, AllFlags,
                             BDA_PAGETABLE_SIZE),
      m_bda_incremental_sync(IncrementalBdaSyncEnabled()),
      m_memory_tracker(page_manager, m_bda_incremental_sync),
      m_staging_buffer(graphics, scheduler, MemoryUsage::Upload, 512 * MiB),
      m_stream_buffer(graphics, scheduler, MemoryUsage::Stream, 64 * MiB),
      m_download_buffer(graphics, scheduler, MemoryUsage::Download, 64 * MiB),
      m_device_buffer(graphics, scheduler, MemoryUsage::DeviceLocal, 128 * MiB),
      m_texture_cache(texture_cache) {
	std::memset(m_gds_buffer.Mapped().data(), 0, static_cast<size_t>(m_gds_buffer.Size()));
	m_gds_buffer.Flush(0, m_gds_buffer.Size());
	SetVulkanObjectNameF(m_graphics.device, m_bda_pagetable_buffer.Handle(),
	                     "BDA Page Table Buffer");
	const auto null_id =
	    m_slot_buffers.insert(m_graphics, m_scheduler, MemoryUsage::DeviceLocal, 0, AllFlags, 16);
	EXIT_IF(null_id != NULL_BUFFER_ID);
	SetVulkanObjectNameF(m_graphics.device, GetBuffer(null_id).Handle(), "Kyty.NullBuffer");
	if (SideReadbackEnabled()) {
		m_side = std::make_unique<SideReadbackState>(m_graphics, m_scheduler, SideReadbackWindow());
	}
	if (!m_graphics.CanReportMemoryUsage()) {
		return;
	}
	constexpr int64_t GiB              = 1024ll * 1024 * 1024;
	constexpr int64_t target_threshold = 8 * GiB;
	const auto        budget =
	    static_cast<int64_t>(std::min<uint64_t>(m_graphics.GetTotalMemoryBudget(), INT64_MAX));
	const auto threshold = std::min(budget, target_threshold);
	const auto expected  = std::min(budget - 6 * threshold / 10, budget - GiB);
	const auto critical  = std::min(budget - 2 * threshold / 10, budget - GiB / 2);
	m_trigger_gc_memory  = static_cast<uint64_t>(std::max<int64_t>(expected, GiB));
	m_critical_gc_memory = static_cast<uint64_t>(std::max<int64_t>(critical, 2 * GiB));
}

BufferCache::~BufferCache() {
	CompleteAllSideReadbacks();
	if (!m_gpu_modified_ranges.Empty()) {
		EXIT("BufferCache: destroyed with pending GPU-modified ranges\n");
	}
	for (const auto& [vaddr, id]: m_buffers) {
		(void)vaddr;
		const auto& buffer = m_slot_buffers[id];
		if (m_memory_tracker.IsRegionGpuModified(buffer.CpuAddress(), buffer.Size())) {
			EXIT("BufferCache: destroyed with GPU-modified buffer\n");
		}
	}
	m_buffers.clear();
}

void BufferCache::InvalidateMemory(uint64_t vaddr, uint64_t size) {
	if (!GuestRange {vaddr, size}.Valid()) {
		EXIT("BufferCache: invalid memory-invalidation range\n");
	}
	ForgetKnownFills(vaddr, size);
	m_memory_tracker.InvalidateRegion(vaddr, size,
	                                  [this, vaddr, size] { ReadMemory(vaddr, size, true); });
}

void BufferCache::ReadMemory(uint64_t vaddr, uint64_t size, bool is_write) {
	Profiler::ScopedFrameWait frame_wait(Profiler::FrameWait::ReadMemory);
	if (!GuestGpu::IsGpuThread() && CommandScheduler::InDeferredOperation()) {
		EXIT("unsupported buffer readback from an asynchronous GPU completion, "
		     "addr=0x%016" PRIx64 " size=0x%016" PRIx64 "\n",
		     vaddr, size);
	}
	if (!GuestRange {vaddr, size}.Valid()) {
		EXIT("BufferCache: invalid readback range\n");
	}
	const auto      trace_start = HangTrace::Enabled() ? HangTrace::NowNs() : 0;
	ReadMemoryTrace trace;
	const auto      record = [&](std::optional<HangTrace::ReadbackKind> kind) {
		if (!HangTrace::Enabled()) {
			return;
		}
		const auto previous = HangTrace::GetReadbackKind();
		if (kind) {
			HangTrace::SetReadbackKind(*kind);
		}
		HangTrace::RecordReadback(vaddr, size, trace.begin, trace.size, trace.downloaded,
		                          HangTrace::NowNs() - trace_start);
		HangTrace::SetReadbackKind(previous);
	};
	auto&      gpu        = m_scheduler.Context().GetGpu();
	const auto page_begin = Common::AlignDown(vaddr, TRACKER_PAGE_SIZE);
	const auto page_end   = Common::AlignUp(vaddr + size, TRACKER_PAGE_SIZE);

	// Side copies serve guest (non-GPU-thread) reads only. Writes need the CPU-dirty transition
	// on the GPU thread, and GPU-thread callers would have to wait for the copy anyway.
	const bool side_path = m_side != nullptr && !is_write && !GuestGpu::IsGpuThread();
	if (OverlapsPendingSideReadback(page_begin, page_end)) {
		// Another fault already copies these pages: wait for (or finish) its publication instead
		// of copying again. Writes and GPU-thread reads must also be ordered after it.
		{
			Profiler::ScopedFrameWait side_wait(Profiler::FrameWait::ReadbackSideWait);
			CompleteSideReadbacks(page_begin, page_end - page_begin);
		}
		if (side_path) {
			// If a newer writer re-dirtied the page meanwhile it stays protected, and the
			// retried access faults into a fresh readback.
			Profiler::CountFrameEvent(Profiler::FrameEvent::ReadbackSideDuplicateWaits);
			record(HangTrace::ReadbackKind::FaultReadDuplicate);
			return;
		}
	}
	if (!side_path) {
		gpu.SendCommandSync([&, this, vaddr, size, is_write] {
			ReadMemoryDrain(vaddr, size, is_write, trace);
		});
		record(std::nullopt);
		return;
	}

	std::shared_ptr<SideReadback> issued;
	auto                          result = SideIssueResult::Other;
	gpu.SendCommandSync([&, this, vaddr, size] {
		result = TryIssueSideReadback(vaddr, size, issued);
		if (result != SideIssueResult::Issued && result != SideIssueResult::Pending) {
			ReadMemoryDrain(vaddr, size, false, trace);
		}
	});
	switch (result) {
		case SideIssueResult::Issued: {
			// The GPU thread returned right after submitting; this guest thread waits.
			{
				Profiler::ScopedFrameWait side_wait(Profiler::FrameWait::ReadbackSideWait);
				CompleteSideReadback(*issued);
			}
			trace.begin      = issued->begin;
			trace.size       = issued->end - issued->begin;
			trace.downloaded = true;
			record(HangTrace::ReadbackKind::FaultReadSide);
			return;
		}
		case SideIssueResult::Pending: {
			// Another thread's side copy of the faulting page was issued after the check above.
			{
				Profiler::ScopedFrameWait side_wait(Profiler::FrameWait::ReadbackSideWait);
				CompleteSideReadbacks(page_begin, page_end - page_begin);
			}
			Profiler::CountFrameEvent(Profiler::FrameEvent::ReadbackSideDuplicateWaits);
			record(HangTrace::ReadbackKind::FaultReadDuplicate);
			return;
		}
		case SideIssueResult::CurrentWriter:
			Profiler::CountFrameEvent(Profiler::FrameEvent::ReadbackSideFallbackCurrentWriter);
			break;
		case SideIssueResult::Unbounded:
			Profiler::CountFrameEvent(Profiler::FrameEvent::ReadbackSideFallbackUnbounded);
			break;
		case SideIssueResult::Other:
			Profiler::CountFrameEvent(Profiler::FrameEvent::ReadbackSideFallbackOther);
			break;
	}
	record(std::nullopt);
}

void BufferCache::ReadMemoryDrain(uint64_t vaddr, uint64_t size, bool is_write,
                                  ReadMemoryTrace& trace) {
	EXIT_IF(!GuestGpu::IsGpuThread());
	if (is_write && !IsRegionRegistered(vaddr, size)) {
		return;
	}
	auto& buffer = m_slot_buffers[FindBuffer(vaddr, size)];

	// Widen nearby CPU reads so they share one GPU drain.
	constexpr uint64_t WindowSize   = 512 * 1024;
	const auto         buffer_begin = buffer.CpuAddress();
	const auto         buffer_end   = buffer_begin + buffer.Size();
	const auto window_begin = std::max(Common::AlignDown(vaddr, WindowSize), buffer_begin);
	const auto window_end = std::min(std::max(window_begin + WindowSize, vaddr + size), buffer_end);

	trace.begin = window_begin;
	trace.size  = window_end - window_begin;
	if (DownloadBufferMemory(buffer, window_begin, window_end - window_begin)) {
		trace.downloaded = true;
		const auto tick  = m_scheduler.CurrentTick();
		m_scheduler.Wait(tick);
		m_scheduler.WaitPriorityOperations(tick);
		m_memory_tracker.UnmarkRegionAsGpuModified(window_begin, window_end - window_begin);
	}
	if (is_write) {
		m_memory_tracker.MarkRegionAsCpuModified(vaddr, size);
	}
}

BufferCache::SideIssueResult BufferCache::TryIssueSideReadback(
    uint64_t vaddr, uint64_t size, std::shared_ptr<SideReadback>& issued) {
	EXIT_IF(!GuestGpu::IsGpuThread() || m_side == nullptr);
	auto&      side    = *m_side;
	const auto current = m_scheduler.CurrentTick();
	// An address-writing shader in the unsubmitted recording may write any buffer byte.
	if (m_unbounded_write_tick == current) {
		return SideIssueResult::Unbounded;
	}
	// Never create a buffer here: GPU-dirty bytes always live in a registered buffer.
	const auto* owner = m_page_table.Find(vaddr >> PageTable::kPageBits);
	if (owner == nullptr || !*owner || IsBufferInvalid(*owner)) {
		return SideIssueResult::Other;
	}
	auto& buffer = m_slot_buffers[*owner];
	if (!buffer.IsInBounds(vaddr, size)) {
		return SideIssueResult::Other;
	}
	const auto buffer_begin = buffer.CpuAddress();
	const auto buffer_end   = buffer_begin + buffer.Size();
	const auto page_begin   = std::max(Common::AlignDown(vaddr, TRACKER_PAGE_SIZE), buffer_begin);
	const auto page_end = std::min(Common::AlignUp(vaddr + size, TRACKER_PAGE_SIZE), buffer_end);
	const auto aligned  = Common::AlignDown(vaddr, side.window);
	const auto window_begin = std::max(aligned, buffer_begin);
	const auto window_end   = std::min(aligned + side.window, buffer_end);
	if (((page_begin | page_end | window_begin | window_end) % TRACKER_PAGE_SIZE) != 0) {
		return SideIssueResult::Other;
	}

	// Prefer the aligned window (neighbouring polled values share one copy); fall back to the
	// faulting pages when a window byte is not eligible (e.g. written by the current recording).
	struct Candidate {
		uint64_t begin;
		uint64_t end;
	};
	const std::array<Candidate, 2> candidates {{{window_begin, window_end}, {page_begin, page_end}}};
	std::vector<GuestRange>        dirty;
	std::optional<Candidate>       chosen;
	uint64_t                       producer = 0;
	auto                           reason   = SideIssueResult::Other;
	for (size_t index = 0; index < candidates.size(); ++index) {
		const auto& candidate = candidates[index];
		if (index != 0 && candidate.begin == candidates[0].begin &&
		    candidate.end == candidates[0].end) {
			break;
		}
		if (candidate.begin > page_begin || candidate.end < page_end ||
		    candidate.end - candidate.begin > side.window) {
			continue;
		}
		if (OverlapsPendingSideReadback(candidate.begin, candidate.end)) {
			reason = SideIssueResult::Pending;
			continue;
		}
		// A queued drain/texture publication decides these bytes' final backing contents;
		// publishing next to it could reorder the backing writes.
		if (HasPendingBackingPublication(candidate.begin, candidate.end - candidate.begin)) {
			reason = SideIssueResult::Other;
			continue;
		}
		dirty.clear();
		uint64_t newest = m_write_tick_floor;
		m_gpu_modified_ranges.ForEachInRange(
		    candidate.begin, candidate.end - candidate.begin, [&](uint64_t start, uint64_t end) {
			    dirty.push_back({start, end - start});
			    newest = std::max(newest, m_write_ticks.MaxTick(start, end - start));
		    });
		if (dirty.empty()) {
			reason = SideIssueResult::Other;
			continue;
		}
		if (newest >= current) {
			reason = SideIssueResult::CurrentWriter;
			continue;
		}
		chosen   = candidate;
		producer = newest;
		break;
	}
	if (!chosen) {
		return reason;
	}

	uint32_t slot_index = SideReadbackState::SlotCount;
	for (uint32_t index = 0; index < SideReadbackState::SlotCount; ++index) {
		if (!side.slots[index].busy.load(std::memory_order_acquire)) {
			slot_index = index;
			break;
		}
	}
	if (slot_index == SideReadbackState::SlotCount) {
		return SideIssueResult::Other;
	}
	auto&      slot         = side.slots[slot_index];
	const auto staging_base = uint64_t {slot_index} * side.window;

	auto readback    = std::make_shared<SideReadback>();
	readback->begin  = chosen->begin;
	readback->end    = chosen->end;
	readback->slot   = slot_index;
	readback->ranges = dirty;
	std::vector<vk::BufferCopy> copies;
	copies.reserve(dirty.size());
	uint64_t bytes = 0;
	for (const auto& range: dirty) {
		copies.emplace_back(buffer.Offset(range.address),
		                    staging_base + (range.address - chosen->begin), range.size);
		bytes += range.size;
	}

	// Ownership of the exact dirty bytes moves to the publication registered below, exactly as
	// in DownloadBufferMemory. The tracker pages stay GPU-owned (protected) until completion.
	for (const auto& range: dirty) {
		CleanVerdict::Invalidate(range.address, range.size, Coherence::Source::BufferDirtySubtract);
	}
	for (const auto& range: dirty) {
		m_gpu_modified_ranges.Subtract(range.address, range.size);
	}
	m_memory_tracker.MarkReadbackPending(chosen->begin, chosen->end - chosen->begin);
	readback->publication = BeginBackingPublication(dirty, producer);

	const auto                 command = slot.command;
	vk::CommandBufferBeginInfo begin_info {};
	begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	RequireVulkanSuccess(command.begin(&begin_info), "begin side-readback command buffer");
	// The timeline wait below orders the producer. This barrier's first scope additionally
	// covers every earlier submission on this queue, so the copy observes all submitted work.
	vk::BufferMemoryBarrier before {};
	before.srcAccessMask       = vk::AccessFlagBits::eMemoryWrite;
	before.dstAccessMask       = vk::AccessFlagBits::eTransferRead;
	before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.buffer              = buffer.Handle();
	before.offset              = buffer.Offset(chosen->begin);
	before.size                = chosen->end - chosen->begin;
	command.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
	                        vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, 1, &before, 0,
	                        nullptr);
	command.copyBuffer(buffer.Handle(), side.staging->Handle(), static_cast<uint32_t>(copies.size()),
	                   copies.data());
	vk::BufferMemoryBarrier after = before;
	after.srcAccessMask           = vk::AccessFlagBits::eTransferWrite;
	after.dstAccessMask           = vk::AccessFlagBits::eHostRead;
	after.buffer                  = side.staging->Handle();
	after.offset                  = staging_base;
	after.size                    = side.window;
	command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
	                        {}, 0, nullptr, 1, &after, 0, nullptr);
	RequireVulkanSuccess(command.end(), "end side-readback command buffer");

	const auto value = ++side.next_value;
	readback->value  = value;
	slot.busy.store(true, std::memory_order_relaxed);

	const auto                      master     = m_scheduler.GetMasterSemaphore().Handle();
	const uint64_t                  wait_value = producer;
	const vk::PipelineStageFlags    wait_stage = vk::PipelineStageFlagBits::eTransfer;
	const uint32_t                  wait_count = producer != 0 ? 1u : 0u;
	vk::TimelineSemaphoreSubmitInfo timeline {};
	timeline.waitSemaphoreValueCount   = wait_count;
	timeline.pWaitSemaphoreValues      = &wait_value;
	timeline.signalSemaphoreValueCount = 1;
	timeline.pSignalSemaphoreValues    = &value;
	vk::SubmitInfo submit {};
	submit.pNext                = &timeline;
	submit.waitSemaphoreCount   = wait_count;
	submit.pWaitSemaphores      = &master;
	submit.pWaitDstStageMask    = &wait_stage;
	submit.commandBufferCount   = 1;
	submit.pCommandBuffers      = &command;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores    = &side.semaphore;
	vk::Result submit_result;
	{
		// Every tick older than the current recording was handed to the queue or to the
		// submission broker (drained here first), so the producer is submitted before this
		// copy waits on it. The current recording follows this copy in submission order.
		Common::LockGuard lock(m_graphics.queue_mutex);
		m_graphics.submission_queue.DrainPendingLocked();
		submit_result = m_graphics.queue.submit(1, &submit, nullptr);
	}
	RequireVulkanSuccess(submit_result, "submit side readback");

	{
		std::lock_guard lock(side.pending_mutex);
		side.pending.push_back(readback);
		side.pending_count.store(side.pending.size(), std::memory_order_release);
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::ReadbackSideCopies);
	Profiler::CountFrameEvent(Profiler::FrameEvent::ReadbackSideCopyBytes, bytes);
	issued = std::move(readback);
	return SideIssueResult::Issued;
}

void BufferCache::CompleteSideReadback(SideReadback& readback) {
	std::scoped_lock lock(readback.mutex);
	if (readback.done.load(std::memory_order_acquire)) {
		return;
	}
	auto& side = *m_side;
	side.Wait(readback.value);
	const auto staging_base = uint64_t {readback.slot} * side.window;
	side.staging->Invalidate(staging_base, readback.end - readback.begin);
	const auto* staged = side.staging->Mapped().data() + staging_base;
	for (const auto& range: readback.ranges) {
		Libs::LibKernel::Memory::WriteBacking(range.address,
		                                      staged + (range.address - readback.begin), range.size);
	}
	EndBackingPublication(readback.publication);
	// Only pages whose pending mark survived lose GPU ownership: a newer recorded writer (or any
	// other GPU transition) since the issue keeps its page protected for a new readback.
	const auto unmark =
	    m_memory_tracker.UnmarkReadbackPending(readback.begin, readback.end - readback.begin);
	Profiler::CountFrameEvent(Profiler::FrameEvent::ReadbackSidePagesUnmarked,
	                          unmark.unmarked_pages);
	Profiler::CountFrameEvent(Profiler::FrameEvent::ReadbackSidePagesRetained,
	                          unmark.retained_pages);
	side.slots[readback.slot].busy.store(false, std::memory_order_release);
	readback.done.store(true, std::memory_order_release);
	std::lock_guard pending_lock(side.pending_mutex);
	const auto      found =
	    std::find_if(side.pending.begin(), side.pending.end(),
	                 [&readback](const auto& entry) { return entry.get() == &readback; });
	EXIT_IF(found == side.pending.end());
	side.pending.erase(found);
	side.pending_count.store(side.pending.size(), std::memory_order_release);
}

bool BufferCache::OverlapsPendingSideReadback(uint64_t begin, uint64_t end) const {
	if (m_side == nullptr || m_side->pending_count.load(std::memory_order_acquire) == 0) {
		return false;
	}
	std::lock_guard lock(m_side->pending_mutex);
	return std::any_of(m_side->pending.begin(), m_side->pending.end(),
	                   [begin, end](const auto& entry) {
		                   return entry->begin < end && begin < entry->end;
	                   });
}

void BufferCache::CompleteSideReadbacks(uint64_t vaddr, uint64_t size) {
	if (m_side == nullptr || m_side->pending_count.load(std::memory_order_acquire) == 0 ||
	    !GuestRange {vaddr, size}.Valid()) {
		return;
	}
	std::vector<std::shared_ptr<SideReadback>> overlapping;
	{
		std::lock_guard lock(m_side->pending_mutex);
		for (const auto& entry: m_side->pending) {
			if (entry->begin < vaddr + size && vaddr < entry->end) {
				overlapping.push_back(entry);
			}
		}
	}
	// Pending entries never overlap each other (issue skips overlapping windows), and are in
	// issue order, so completing them in this order keeps publications in submission order.
	for (const auto& entry: overlapping) {
		CompleteSideReadback(*entry);
	}
}

void BufferCache::CompleteAllSideReadbacks() {
	if (m_side == nullptr || m_side->pending_count.load(std::memory_order_acquire) == 0) {
		return;
	}
	std::vector<std::shared_ptr<SideReadback>> all;
	{
		std::lock_guard lock(m_side->pending_mutex);
		all = m_side->pending;
	}
	for (const auto& entry: all) {
		CompleteSideReadback(*entry);
	}
}

void BufferCache::NoteBufferContentWrite(uint64_t vaddr, uint64_t size) {
	if (m_side == nullptr) {
		return;
	}
	m_write_ticks.Assign(vaddr, size, m_scheduler.CurrentTick());
	if (m_write_ticks.Size() >= m_write_tick_prune_size) {
		// Completed writers need no entry: their ranges read back as the prune floor.
		const auto completed = m_scheduler.GetMasterSemaphore().KnownGpuTick();
		m_write_ticks.Prune(completed);
		m_write_tick_floor      = std::max(m_write_tick_floor, completed);
		m_write_tick_prune_size = std::max<size_t>(1024, m_write_ticks.Size() * 2);
	}
}

BufferId BufferCache::FindBuffer(uint64_t vaddr, uint64_t size) {
	if (vaddr == 0) {
		return NULL_BUFFER_ID;
	}
	if (!GuestRange {vaddr, size}.Valid()) {
		EXIT("BufferCache: invalid buffer discovery request\n");
	}
	const auto* owner = m_page_table.Find(vaddr >> PageTable::kPageBits);
	if (owner != nullptr && *owner) {
		auto& buffer = m_slot_buffers[*owner];
		if (buffer.IsInBounds(vaddr, size)) {
			return *owner;
		}
	}
	return CreateBuffer(vaddr, size);
}

BufferCache::OverlapResult BufferCache::ResolveOverlaps(uint64_t vaddr, uint64_t size) {
	static constexpr int      StreamLeapThreshold = 16;
	static constexpr uint64_t StreamLeapSize      = CACHING_PAGESIZE * 128;

	auto       begin      = vaddr;
	auto       end        = vaddr + size;
	const auto find_first = [&](uint64_t address) {
		auto first = m_buffers.lower_bound(address);
		if (first != m_buffers.begin()) {
			const auto  previous = std::prev(first);
			const auto& buffer   = m_slot_buffers[previous->second];
			if (buffer.CpuAddress() + buffer.Size() > address) {
				first = previous;
			}
		}
		return first;
	};
	auto first           = find_first(begin);
	auto last            = first;
	int  stream_score    = 0;
	bool has_stream_leap = false;
	for (; last != m_buffers.end() && last->first < end; ++last) {
		const auto& buffer        = m_slot_buffers[last->second];
		const auto  buffer_begin  = buffer.CpuAddress();
		const auto  buffer_end    = buffer_begin + buffer.Size();
		const bool  expands_left  = buffer_begin < begin;
		const bool  expands_right = buffer_end > end;
		begin                     = std::min(begin, buffer_begin);
		end                       = std::max(end, buffer_end);
		if (!has_stream_leap && (stream_score += buffer.StreamScore()) > StreamLeapThreshold) {
			has_stream_leap = true;
			// Reserve space in the incoming stream's direction of growth.
			// The old buffer extending left of the request predicts growth to the right, and vice versa.
			if (expands_left) {
				end += std::min(StreamLeapSize, PageTable::kAddressSpaceSize - end);
			}
			if (expands_right) {
				const auto minimum = CACHING_PAGESIZE * 2;
				if (begin > minimum) {
					begin -= std::min(StreamLeapSize, begin - minimum);
				}
				first = find_first(begin);
				begin = std::min(begin, first->first);
			}
		}
	}
	return {first, last, begin, end, has_stream_leap};
}

void BufferCache::JoinOverlap(BufferId new_id, BufferId overlap_id, bool accumulate_stream_score) {
	auto& new_buffer = m_slot_buffers[new_id];
	auto& overlap    = m_slot_buffers[overlap_id];
	if (accumulate_stream_score) {
		new_buffer.IncreaseStreamScore(overlap.StreamScore() + 1);
	}
	new_buffer.CopyFrom(m_scheduler.Current(), overlap, 0,
	                    overlap.CpuAddress() - new_buffer.CpuAddress(), overlap.Size());
	DeleteBuffer(overlap_id);
}

BufferId BufferCache::CreateBuffer(uint64_t vaddr, uint64_t size) {
	EXIT_IF(m_scheduler.Current().IsInvalid());
	const auto end = Common::AlignUp(vaddr + size, CACHING_PAGESIZE);
	vaddr = Common::AlignDown(vaddr, CACHING_PAGESIZE);
	size               = end - vaddr;
	const auto overlap = ResolveOverlaps(vaddr, size);

	const auto id = m_slot_buffers.insert(
	    m_graphics, m_scheduler, MemoryUsage::DeviceLocal, overlap.begin,
	    AllFlags | vk::BufferUsageFlagBits::eShaderDeviceAddress, overlap.end - overlap.begin);
	const auto& buffer = m_slot_buffers[id];
	SetVulkanObjectNameF(m_graphics.device, buffer.Handle(),
	                     "Kyty.GameBuffer[guest=0x{:016x} size=0x{:x}]", overlap.begin,
	                     overlap.end - overlap.begin);
	const bool joined = overlap.first != overlap.last;
	for (auto it = overlap.first; it != overlap.last;) {
		const auto old_id = (it++)->second;
		JoinOverlap(id, old_id, !overlap.has_stream_leap);
	}
	if (joined) {
		// The joined contents (including GPU-dirty bytes) are copied by this recording.
		NoteBufferContentWrite(overlap.begin, overlap.end - overlap.begin);
	}
	Register(id);
	return id;
}

bool BufferCache::SynchronizeBuffer(Buffer& buffer, uint64_t vaddr, uint64_t size, bool is_written,
                                    bool is_texel_buffer, BdaSyncStats* stats,
                                    const char* upload_reason) {
	KYTY_GPU_OP_SITE("buffercache.upload");
	std::vector<vk::BufferCopy> copies;
	uint64_t                    total_size = 0;
	vk::Buffer                  source;
	uint8_t* reserved = nullptr;
	uint64_t reserved_offset = 0;
	uint64_t reserved_size = 0;
	if (Common::RendererBatchEnabled() && is_written && size <= MiB &&
	    m_memory_tracker.IsRegionCpuModified(vaddr, size)) {
		// Reserve before entering writable tracker locks. The dirty set is collected
		// again under those locks, so a concurrent CPU write cannot be missed.
		const auto begin = Common::AlignDown(vaddr, CACHING_PAGESIZE);
		reserved_size = Common::AlignUp(vaddr + size, CACHING_PAGESIZE) - begin;
		copies.reserve(static_cast<size_t>(reserved_size / CACHING_PAGESIZE));
		std::tie(reserved, reserved_offset) = m_staging_buffer.Map(reserved_size, 4);
	}
	m_memory_tracker.ForEachUploadRange(
	    vaddr, size, is_written,
	    [&](uint64_t address, uint64_t bytes) noexcept {
		    copies.emplace_back(total_size, buffer.Offset(address), bytes);
		    total_size += bytes;
	    },
	    [&]() noexcept {
		    if (reserved != nullptr && total_size <= reserved_size) {
			    for (auto& copy: copies) {
				    std::memcpy(reserved + copy.srcOffset,
				                reinterpret_cast<const void*>(buffer.CpuAddress() + copy.dstOffset),
				                copy.size);
				    copy.srcOffset += reserved_offset;
			    }
			    if (!copies.empty()) source = m_staging_buffer.Handle();
		    } else {
			    reserved = nullptr;
			    source = UploadCopies(buffer, copies, total_size);
		    }
	    });
	if (reserved != nullptr && source) {
		// Source copying and GPU ownership publication stayed atomic. Flush and ring
		// bookkeeping need no tracker lock and finish before native copy recording.
		m_staging_buffer.Commit();
		Profiler::CountFrameEvent(Profiler::FrameEvent::UploadReservationsOutsideLocks);
	}
	if (source) {
		if (stats != nullptr) {
			stats->upload_bytes += total_size;
			stats->upload_copies += copies.size();
		}
		Profiler::CountFrameEvent(Profiler::FrameEvent::BufferUploadBytes, total_size);
		if (HangTrace::Enabled()) {
			const char* reason = upload_reason;
			if (reason == nullptr) {
				reason = stats != nullptr    ? "bda-sync"
				         : is_written        ? "written-binding"
				         : is_texel_buffer   ? "texel-read"
				                             : "read-binding";
			}
			// Address is the first uploaded page run; span_bytes the requested range.
			HangTrace::RecordTransfer(HangTrace::TransferKind::BufferUpload, reason, "",
			                          copies.empty() ? vaddr
			                                         : buffer.CpuAddress() + copies.front().dstOffset,
			                          0, static_cast<uint32_t>(copies.size()), 0, total_size, size);
		}
		auto& command = m_scheduler.Current();
		command.EndRendering();
		const auto native = command.Handle();
		vk::BufferMemoryBarrier before {};
		before.srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite |
		                       vk::AccessFlagBits::eTransferRead |
		                       vk::AccessFlagBits::eTransferWrite;
		before.dstAccessMask       = vk::AccessFlagBits::eTransferWrite;
		before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		before.buffer              = buffer.Handle();
		before.offset              = 0;
		before.size                = buffer.Size();
		native.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
		                       vk::PipelineStageFlagBits::eTransfer,
		                       vk::DependencyFlagBits::eByRegion, 0, nullptr, 1, &before, 0, nullptr);
		native.copyBuffer(source, buffer.Handle(), static_cast<uint32_t>(copies.size()),
		                  copies.data());
		buffer.MarkContentWritten();
		auto after          = before;
		after.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		after.dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
		native.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
		                       vk::PipelineStageFlagBits::eAllCommands,
		                       vk::DependencyFlagBits::eByRegion, 0, nullptr, 1, &after, 0, nullptr);
	}
	if (is_texel_buffer && !is_written) {
		return SynchronizeBufferFromImage(buffer, vaddr, size);
	}
	return false;
}

vk::Buffer BufferCache::UploadCopies(Buffer& buffer, std::span<vk::BufferCopy> copies,
                                     uint64_t total_size) {
	if (copies.empty()) {
		return nullptr;
	}

	auto [mapped, base_offset] = m_staging_buffer.Map(total_size, 4);
	if (mapped != nullptr) {
		for (auto& copy: copies) {
			const auto address = buffer.CpuAddress() + copy.dstOffset;
			std::memcpy(mapped + copy.srcOffset, reinterpret_cast<const void*>(address), copy.size);
			copy.srcOffset += base_offset;
		}
		m_staging_buffer.Commit();
		return m_staging_buffer.Handle();
	}

	auto temporary = std::make_unique<Buffer>(m_graphics, m_scheduler, MemoryUsage::Upload, 0,
	                                         vk::BufferUsageFlagBits::eTransferSrc, total_size);
	for (const auto& copy: copies) {
		const auto address = buffer.CpuAddress() + copy.dstOffset;
		std::memcpy(temporary->Mapped().data() + copy.srcOffset,
		            reinterpret_cast<const void*>(address), copy.size);
	}
	temporary->Flush(0, total_size);
	const auto handle = temporary->Handle();
	m_scheduler.DeferOperation([owner = std::move(temporary)]() mutable { owner.reset(); });
	return handle;
}

std::pair<Buffer*, uint64_t> BufferCache::ObtainBuffer(uint64_t vaddr, uint64_t size,
                                                       bool is_written, bool is_texel_buffer,
                                                       BufferId id) {
	auto& command = m_scheduler.Current();
	if (command.IsInvalid() || !GuestRange {vaddr, size}.Valid()) {
		EXIT("BufferCache: buffer request requires a recording command buffer\n");
	}

	if (!is_written && size <= CACHING_PAGESIZE &&
	    !m_memory_tracker.IsRegionGpuModified(vaddr, size) &&
	    m_memory_tracker.IsRegionCpuModified(vaddr, size)) {
		const auto alignment = std::max<uint64_t>(
		    m_graphics.physical_device_properties.limits.minUniformBufferOffsetAlignment, 1);
		auto [mapped, offset] = m_stream_buffer.Map(size, alignment, false);
		if (mapped != nullptr && Libs::LibKernel::Memory::TryReadBacking(vaddr, mapped, size)) {
			m_stream_buffer.Commit();
			return {&m_stream_buffer, offset};
		}
	}

	if (IsBufferInvalid(id) || !m_slot_buffers[id].IsInBounds(vaddr, size)) {
		id = FindBuffer(vaddr, size);
	}
	auto& buffer = m_slot_buffers[id];
	TouchBuffer(buffer);
	(void)SynchronizeBuffer(buffer, vaddr, size, is_written, is_texel_buffer);
	if (is_written) {
		// Writable descriptors reserve a new version before recording their shader commands.
		buffer.MarkContentWritten();
		// An Add that changes nothing cannot stale a cached clean page, because pages with
		// dirty bytes are never cached clean. The tracker GPU bits that SynchronizeBuffer just
		// set are only ever set together with this Add; clean-read verdicts never read them.
		if (!m_gpu_modified_ranges.Contains(vaddr, size)) {
			CleanVerdict::Invalidate(vaddr, size, Coherence::Source::BufferDirtyAdd);
		}
		m_gpu_modified_ranges.Add(vaddr, size);
		NoteBufferContentWrite(vaddr, size);
		ForgetKnownFills(vaddr, size);
		HangTrace::NoteGpuWrite(vaddr, size);
	}
	return {&buffer, buffer.Offset(vaddr)};
}

std::pair<Buffer*, uint64_t> BufferCache::ObtainBufferForImage(uint64_t vaddr, uint64_t size) {
	if (!GuestRange {vaddr, size}.Valid()) {
		EXIT("BufferCache: invalid image source\n");
	}
	const auto* owner = m_page_table.Find(vaddr >> PageTable::kPageBits);
	if (owner != nullptr && *owner) {
		auto& buffer = m_slot_buffers[*owner];
		if (buffer.IsInBounds(vaddr, size)) {
			TouchBuffer(buffer);
			(void)SynchronizeBuffer(buffer, vaddr, size, false, false, nullptr, "image-source");
			return {&buffer, buffer.Offset(vaddr)};
		}
	}
	if (IsRegionGpuModified(vaddr, size)) {
		return ObtainBuffer(vaddr, size, false, false);
	}

	auto [staging, stage_offset] = m_staging_buffer.Map(size, 16);
	if (staging == nullptr || (!Libs::LibKernel::Memory::TryReadBacking(vaddr, staging, size) &&
	                           !Libs::LibKernel::Memory::TryReadPrtBacking(vaddr, staging, size))) {
		EXIT("BufferCache: failed to read mapped guest image backing\n");
	}
	m_staging_buffer.Commit();
	return {&m_staging_buffer, stage_offset};
}

void BufferCache::FillBuffer(uint64_t vaddr, uint64_t size, uint32_t value, bool is_gds) {
	if ((vaddr & 3u) != 0 || size == 0 || (size & 3u) != 0 || size > UINT64_MAX - vaddr) {
		EXIT("BufferCache: fill range must be dword aligned\n");
	}
	if (is_gds) {
		if (vaddr > m_gds_buffer.Size() || size > m_gds_buffer.Size() - vaddr) {
			EXIT("BufferCache: GDS fill range is out of bounds\n");
		}
		m_gds_buffer.Fill(vaddr, size, value);
		return;
	}
	if (vaddr == 0) {
		EXIT("BufferCache: invalid fill memory address\n");
	}
	(void)m_texture_cache.ClearMeta(vaddr);
	if (!IsRegionGpuModified(vaddr, size)) {
		// Access the guest mapping so write faults invalidate cached buffers and images.
		auto* destination = reinterpret_cast<uint32_t*>(vaddr);
		std::fill(destination, destination + size / sizeof(uint32_t), value);
		return;
	}

	m_texture_cache.InvalidateMemoryFromGPU(vaddr, size);
	HangTrace::ScopedGpuWriteKind trace_kind(HangTrace::GpuWriteKind::Fill);
	auto [dst, dst_offset] = ObtainBuffer(vaddr, size, true, true);
	dst->Fill(dst_offset, size, value);
	RecordKnownFill(vaddr, size, value);
}

void BufferCache::RecordKnownFill(uint64_t vaddr, uint64_t size, uint32_t value) {
	if (size == 0) {
		return;
	}
	std::scoped_lock lock(m_known_fill_mutex);
	ForgetKnownFillsLocked(vaddr, size);
	if (m_known_fills.size() >= 64) {
		m_known_fills.erase(m_known_fills.begin());
	}
	m_known_fills.push_back({vaddr, size, value});
}

std::optional<uint32_t> BufferCache::KnownFill(uint64_t vaddr, uint64_t size) const {
	// The range may be covered by several adjacent fills (e.g. per-slice consumption); all of
	// them must carry the same value.
	std::scoped_lock        lock(m_known_fill_mutex);
	const uint64_t          end    = vaddr + size;
	uint64_t                cursor = vaddr;
	std::optional<uint32_t> value;
	while (cursor < end) {
		const auto covering =
		    std::find_if(m_known_fills.begin(), m_known_fills.end(), [cursor](const KnownFillRange& fill) {
			    return cursor >= fill.address && cursor < fill.address + fill.size;
		    });
		if (covering == m_known_fills.end() || (value && *value != covering->value)) {
			return std::nullopt;
		}
		value  = covering->value;
		cursor = covering->address + covering->size;
	}
	return value;
}

void BufferCache::ForgetKnownFills(uint64_t vaddr, uint64_t size) {
	std::scoped_lock lock(m_known_fill_mutex);
	ForgetKnownFillsLocked(vaddr, size);
}

void BufferCache::ForgetKnownFillsLocked(uint64_t vaddr, uint64_t size) {
	// Keep the parts of each fill outside the written range.
	const uint64_t              end = vaddr + size;
	std::vector<KnownFillRange> kept;
	kept.reserve(m_known_fills.size() + 1);
	for (const auto& fill: m_known_fills) {
		const uint64_t fill_end = fill.address + fill.size;
		if (end <= fill.address || fill_end <= vaddr) {
			kept.push_back(fill);
			continue;
		}
		if (fill.address < vaddr) {
			kept.push_back({fill.address, vaddr - fill.address, fill.value});
		}
		if (end < fill_end) {
			kept.push_back({end, fill_end - end, fill.value});
		}
	}
	m_known_fills.swap(kept);
}

void BufferCache::CopyBuffer(uint64_t dst_vaddr, uint64_t src_vaddr, uint64_t size, bool dst_gds,
                             bool src_gds) {
	const bool dst_memory = !dst_gds;
	const bool src_memory = !src_gds;
	if ((dst_memory && dst_vaddr == 0) || (src_memory && src_vaddr == 0) || size == 0 ||
	    ((dst_gds || src_gds) && ((dst_vaddr | src_vaddr | size) & 3u) != 0) ||
	    size > UINT64_MAX - dst_vaddr || size > UINT64_MAX - src_vaddr || (dst_gds && src_gds) ||
	    (dst_gds && (dst_vaddr > m_gds_buffer.Size() || size > m_gds_buffer.Size() - dst_vaddr)) ||
	    (src_gds && (src_vaddr > m_gds_buffer.Size() || size > m_gds_buffer.Size() - src_vaddr))) {
		EXIT("BufferCache: invalid copy range, src=0x%016" PRIx64 " dst=0x%016" PRIx64
		     " size=0x%016" PRIx64 " src_gds=%d dst_gds=%d\n",
		     src_vaddr, dst_vaddr, size, static_cast<int>(src_gds), static_cast<int>(dst_gds));
	}
	if (src_memory && dst_memory && !IsRegionGpuModified(dst_vaddr, size) &&
	    !IsRegionGpuModified(src_vaddr, size) && !m_texture_cache.FindImageFromRange(src_vaddr, size)) {
		std::memcpy(reinterpret_cast<void*>(dst_vaddr), reinterpret_cast<const void*>(src_vaddr),
		            size);
		return;
	}

	auto& command = m_scheduler.Current();
	if (dst_memory) {
		m_texture_cache.InvalidateMemoryFromGPU(dst_vaddr, size);
	}
	const auto src_id      = src_memory ? FindBuffer(src_vaddr, size) : BufferId {};
	const auto dst_id      = dst_memory ? FindBuffer(dst_vaddr, size) : BufferId {};
	auto [src, src_offset] = src_memory ? ObtainBuffer(src_vaddr, size, false, true, src_id)
	                                    : std::pair {&m_gds_buffer, src_vaddr};
	HangTrace::ScopedGpuWriteKind trace_kind(HangTrace::GpuWriteKind::Copy);
	auto [dst, dst_offset] = dst_memory ? ObtainBuffer(dst_vaddr, size, true, true, dst_id)
	                                    : std::pair {&m_gds_buffer, dst_vaddr};
	dst->CopyFrom(command, *src, src_offset, dst_offset, size);
}

bool BufferCache::IsRegionRegistered(uint64_t vaddr, uint64_t size) {
	if (!GuestRange {vaddr, size}.Valid()) {
		EXIT("BufferCache: invalid registered-region query\n");
	}
	// Cached buffers are ordered and non-overlapping. The last buffer beginning before the query
	// end is therefore the only possible intersection.
	const auto candidate = m_buffers.lower_bound(vaddr + size);
	if (candidate == m_buffers.begin()) {
		return false;
	}
	const auto& [address, id] = *std::prev(candidate);
	return address + m_slot_buffers[id].Size() > vaddr;
}

bool BufferCache::IsRegionGpuModified(uint64_t vaddr, uint64_t size) {
	return m_memory_tracker.IsRegionGpuModified(vaddr, size);
}

bool BufferCache::HasGpuDirtyBytes(uint64_t vaddr, uint64_t size) {
	return m_gpu_modified_ranges.Intersects(vaddr, size);
}

std::optional<BufferContentRevision> BufferCache::GetContentRevision(uint64_t vaddr,
	                                                                uint64_t size) {
	EXIT_IF(!GuestGpu::IsGpuThread());
	if (!GuestRange {vaddr, size}.Valid() ||
	    m_memory_tracker.IsRegionCpuModified(vaddr, size) ||
	    HasPendingBackingPublication(vaddr, size)) {
		return std::nullopt;
	}
	const auto* owner = m_page_table.Find(vaddr >> PageTable::kPageBits);
	if (owner == nullptr || IsBufferInvalid(*owner)) {
		return std::nullopt;
	}
	const auto& buffer = m_slot_buffers[*owner];
	if (!buffer.IsInBounds(vaddr, size)) {
		return std::nullopt;
	}
	return BufferContentRevision {*owner, buffer.ContentRevision(), m_content_revision_epoch};
}

void BufferCache::InvalidateContentRevisions() {
	EXIT_IF(!GuestGpu::IsGpuThread() || m_content_revision_epoch == UINT64_MAX);
	// Unbounded GPU writes follow; retire clean-read verdicts along with the revisions.
	CleanVerdict::Invalidate(0, UINT64_MAX, Coherence::Source::ContentRevisions);
	++m_content_revision_epoch;
	// Their bytes carry no writer tick, so no readback may skip this recording.
	m_unbounded_write_tick = m_scheduler.CurrentTick();
}

uint64_t BufferCache::BeginBackingPublication(std::span<const GuestRange> ranges, uint64_t tick) {
	EXIT_IF(!GuestGpu::IsGpuThread() || ranges.empty());
	for (const auto& range: ranges) {
		EXIT_IF(!range.Valid());
	}
	std::lock_guard lock(m_backing_publication_mutex);
	const auto token = ++m_next_backing_publication_token;
	EXIT_IF(token == 0);
	// Pending ranges are not clean for backing reads; retire verdicts before publishing them.
	for (const auto& range: ranges) {
		CleanVerdict::Invalidate(range.address, range.size, Coherence::Source::PublicationBegin);
	}
	m_backing_publications.push_back({token, tick, {ranges.begin(), ranges.end()}});
	m_backing_publication_count.store(m_backing_publications.size(), std::memory_order_release);
	return token;
}

void BufferCache::EndBackingPublication(uint64_t token) {
	std::lock_guard lock(m_backing_publication_mutex);
	const auto found = std::find_if(m_backing_publications.begin(), m_backing_publications.end(),
	                                [token](const auto& entry) { return entry.token == token; });
	EXIT_IF(found == m_backing_publications.end());
	// Backing authority changes here (runs on the priority worker). Bump before the entry is
	// erased so a verdict evaluated after the erase is tagged with the new generation.
	for (const auto& range: found->ranges) {
		CleanVerdict::Invalidate(range.address, range.size, Coherence::Source::PublicationEnd);
	}
	m_backing_publications.erase(found);
	// Publish completion only after the callback has written every registered backing range.
	m_backing_publication_count.store(m_backing_publications.size(), std::memory_order_release);
}

bool BufferCache::HasPendingBackingPublication(uint64_t vaddr, uint64_t size) const {
	return PendingBackingPublicationTick(vaddr, size).has_value();
}

std::optional<uint64_t> BufferCache::PendingBackingPublicationTick(uint64_t vaddr,
	                                                               uint64_t size) const {
	const GuestRange query {vaddr, size};
	EXIT_IF(!query.Valid());
	if (m_backing_publication_count.load(std::memory_order_acquire) == 0) {
		return std::nullopt;
	}
	std::lock_guard lock(m_backing_publication_mutex);
	std::optional<uint64_t> latest;
	for (const auto& entry: m_backing_publications) {
		for (const auto& range: entry.ranges) {
			if (range.address < query.End() && query.address < range.End()) {
				latest = latest ? std::max(*latest, entry.tick) : entry.tick;
				break;
			}
		}
	}
	return latest;
}

bool BufferCache::IsRegionCpuModified(uint64_t vaddr, uint64_t size) {
	return m_memory_tracker.IsRegionCpuModified(vaddr, size);
}

void BufferCache::RunGarbageCollector() {
	const auto tick = m_gc_tick++;
	if (m_graphics.CanReportMemoryUsage()) {
		m_total_used_memory = m_graphics.GetDeviceMemoryUsage();
	}
	if (m_total_used_memory < m_trigger_gc_memory) {
		return;
	}
	// Pending side readbacks keep tracker pages GPU-owned without exact dirty bytes; settle
	// them so the ownership checks and downloads below see a consistent state.
	CompleteAllSideReadbacks();

	const bool     aggressive = m_total_used_memory >= m_critical_gc_memory;
	const uint64_t age        = std::min<uint64_t>(aggressive ? 80 : 160, tick);
	const size_t   limit      = aggressive ? 64 : 32;

	std::vector<BufferId> dirty_buffers;
	size_t                retire_count = 0;
	m_lru_cache.ForEachItemBelow(tick - age, [&](BufferId id) {
		auto& buffer = m_slot_buffers[id];
		EXIT_IF(buffer.is_deleted);
		m_memory_tracker.ValidateGpuDirtyOwnership(m_gpu_modified_ranges, buffer.CpuAddress(),
		                                           buffer.Size(), "garbage collection");
		const bool dirty = m_memory_tracker.IsRegionGpuModified(buffer.CpuAddress(), buffer.Size());
		if (dirty && !aggressive) {
			return false;
		}
		if (dirty) {
			EXIT_IF(!DownloadBufferMemory(buffer, buffer.CpuAddress(), buffer.Size()));
			dirty_buffers.push_back(id);
		} else {
			m_memory_tracker.UntrackMemory(buffer.CpuAddress(), buffer.Size());
			DeleteBuffer(id);
		}
		return ++retire_count == limit;
	});
	if (dirty_buffers.empty()) {
		return;
	}

	// Publish all queued downloads before releasing their tracked pages and owners.
	const auto completion_tick = m_scheduler.CurrentTick();
	m_scheduler.Wait(completion_tick);
	m_scheduler.WaitPriorityOperations(completion_tick);
	for (const auto id: dirty_buffers) {
		auto& buffer = m_slot_buffers[id];
		m_memory_tracker.UnmarkRegionAsGpuModified(buffer.CpuAddress(), buffer.Size());
		if (m_memory_tracker.IsRegionGpuModified(buffer.CpuAddress(), buffer.Size()) ||
		    m_gpu_modified_ranges.Intersects(buffer.CpuAddress(), buffer.Size())) {
			EXIT("BufferCache: garbage collection retained GPU ownership\n");
		}
		m_memory_tracker.UntrackMemory(buffer.CpuAddress(), buffer.Size());
		Unregister(id);
		m_slot_buffers.erase(id);
	}
}

void BufferCache::ProcessFaultBuffer() {
	m_fault_manager.ProcessFaultBuffer();
}

void BufferCache::InvalidateBdaSynchronization() noexcept {
	if (!m_bda_incremental_sync) {
		return;
	}
	auto epoch = m_bda_structure_epoch.load(std::memory_order_relaxed);
	while (epoch != UINT64_MAX &&
	       !m_bda_structure_epoch.compare_exchange_weak(epoch, epoch + 1,
	                                                     std::memory_order_release,
	                                                     std::memory_order_relaxed)) {}
}

void BufferCache::SynchronizeBdaBuffers(const RangeSet& mapped_ranges) {
	const bool collect = Profiler::AggregateEnabled();
	// Read these before scanning: a fault to an already scanned page must force the NEXT
	// pass, even if its dirty transition completed before this pass finished uploading.
	const auto cpu_epoch = m_bda_incremental_sync ? m_memory_tracker.CpuMutationEpoch() : 0;
	const auto structure_epoch =
	    m_bda_incremental_sync ? m_bda_structure_epoch.load(std::memory_order_acquire) : 0;
	if (m_bda_incremental_sync && cpu_epoch != UINT64_MAX && structure_epoch != UINT64_MAX &&
	    cpu_epoch == m_bda_scanned_cpu_epoch && structure_epoch == m_bda_scanned_structure_epoch) {
		if (collect) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::BdaSyncSkips);
		}
		return;
	}

	BdaSyncStats stats;
	mapped_ranges.ForEach([this, collect, &stats](uint64_t start, uint64_t end) {
		SynchronizeBuffersInRange(start, end - start, collect ? &stats : nullptr);
	});
	if (m_bda_incremental_sync) {
		m_bda_scanned_cpu_epoch       = cpu_epoch;
		m_bda_scanned_structure_epoch = structure_epoch;
	}
	if (collect) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::BdaSyncPasses);
		Profiler::CountFrameEvent(Profiler::FrameEvent::BdaSyncScannedBuffers, stats.scanned_buffers);
		Profiler::CountFrameEvent(Profiler::FrameEvent::BdaSyncUploadBytes, stats.upload_bytes);
		Profiler::CountFrameEvent(Profiler::FrameEvent::BdaSyncUploadCopies, stats.upload_copies);
	}
}

void BufferCache::SynchronizeBuffersInRange(uint64_t vaddr, uint64_t size, BdaSyncStats* stats) {
	const auto end = vaddr + size;
	auto       it  = m_buffers.upper_bound(vaddr);
	if (it != m_buffers.begin()) {
		--it;
	}
	for (; it != m_buffers.end() && it->first < end; ++it) {
		auto&      buffer = m_slot_buffers[it->second];
		const auto start  = std::max(buffer.CpuAddress(), vaddr);
		const auto finish = std::min(buffer.CpuAddress() + buffer.Size(), end);
		if (start < finish) {
			if (stats != nullptr) {
				++stats->scanned_buffers;
			}
			(void)SynchronizeBuffer(buffer, start, finish - start, false, false, stats);
		}
	}
}

} // namespace Libs::Graphics
