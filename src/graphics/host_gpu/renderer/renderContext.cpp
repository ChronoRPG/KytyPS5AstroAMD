#include "graphics/host_gpu/renderer/renderContext.h"

#include "common/alignment.h"
#include "common/assert.h"
#include "common/hangTrace.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "graphics/guest_gpu/graphicsRun.h"
#include "graphics/host_gpu/cleanVerdictCache.h"
#include "graphics/host_gpu/memoryStats.h"
#include "graphics/host_gpu/syncEpoch.h"
#include "graphics/presentation/videoOut.h"
#include "libs/errno.h"

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <shared_mutex>

namespace Libs::Graphics {

RenderContext::RenderContext(GraphicContext& graphics)
    : m_graphics(graphics), m_render_executor(*this),
      m_command_scheduler(*this, graphics, CommandScheduler::Role::Guest),
      m_descriptor_heap(graphics, m_command_scheduler.GetMasterSemaphore()),
      m_pipeline_cache(graphics), m_sampler_cache(graphics),
      m_buffer_cache(graphics, m_command_scheduler, m_page_manager, m_texture_cache),
      m_texture_cache(graphics, m_command_scheduler, m_page_manager, m_buffer_cache),
      m_occlusion_counter(*this), m_lod_stats(*this) {
	EXIT_NOT_IMPLEMENTED(!Common::Thread::IsMainThread());
}

RenderContext::~RenderContext() {
	ShutdownGpu();
	m_command_scheduler.Shutdown();
}

void RenderContext::InitializeGpu(VideoOut::VideoOutDriver* video_out) {
	EXIT_IF(m_gpu != nullptr);
	m_video_out = video_out;
	m_gpu       = std::make_unique<GuestGpu>(*this);
	std::unique_lock lock(m_gpu_notify_mutex);
	m_gpu_notify = m_gpu.get();
}

void RenderContext::NotifyGpuProgress() {
	std::shared_lock lock(m_gpu_notify_mutex);
	if (m_gpu_notify != nullptr) {
		m_gpu_notify->NotifyProgress();
	}
}

void RenderContext::ShutdownGpu() {
	{
		std::unique_lock lock(m_gpu_notify_mutex);
		m_gpu_notify = nullptr;
	}
	if (m_gpu != nullptr) {
		m_gpu->Shutdown();
		m_gpu.reset();
	}
	if (m_video_out != nullptr) {
		if (m_command_scheduler.Active()) {
			m_command_scheduler.Finish();
		}
		m_command_scheduler.DrainPriorityOperations();
		m_video_out = nullptr;
	}
}

GuestGpu& RenderContext::GetGpu() const {
	EXIT_IF(m_gpu == nullptr);
	return *m_gpu;
}

VideoOut::VideoOutDriver& RenderContext::GetVideoOut() const {
	EXIT_IF(m_video_out == nullptr);
	return *m_video_out;
}

bool RenderContext::HandleFault(PageFaultAccess access, uint64_t fault_vaddr) noexcept {
	// The host reports the faulting byte, not the instruction's access width. Both caches
	// resolve its page; guessing a width can cross the end of a valid guest mapping.
	constexpr uint64_t fault_size = 1;
	if (!IsMapped(fault_vaddr, fault_size)) {
		return false;
	}
	const MemoryStats::ScopedTimer fault_timer(MemoryStats::Counter::FaultNs);
	MemoryStats::Count(access == PageFaultAccess::Write ? MemoryStats::Counter::WriteFaults
	                                                    : MemoryStats::Counter::ReadFaults);
	if (access == PageFaultAccess::Write) {
		// Both caches' releases of the page reach the host once, when the scope ends and no
		// tracking lock is held (KYTY_DEFER_UNPROTECT).
		const bool nested = PageManager::InDeferUnprotectScope();
		{
			const PageManager::DeferUnprotectScope defer_unprotect;
			m_buffer_cache.InvalidateMemory(fault_vaddr, fault_size, true);
			m_texture_cache.InvalidateMemory(fault_vaddr, fault_size);
			// The faulting page gets the protection its watchers ask even when neither cache
			// released anything here: another thread may have released it without having applied
			// the release yet. A fault inside an enclosing scope (an emulator write between that
			// scope's releases) applies at once: the enclosing scope ends only after the retry.
			m_page_manager.Reconcile(Common::AlignDown(fault_vaddr, TRACKER_PAGE_SIZE),
			                         TRACKER_PAGE_SIZE, nested);
		}
		if (nested) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::DeferredUnprotectNestedFaults);
		}
	} else {
		m_buffer_cache.ReadMemory(fault_vaddr, fault_size);
	}
	return true;
}

bool RenderContext::InvalidateMemory(uint64_t vaddr, uint64_t size) {
	if (!IsMapped(vaddr, size)) {
		return false;
	}
	// One host update per page for both caches, after their locks (KYTY_DEFER_UNPROTECT).
	const PageManager::DeferUnprotectScope defer_unprotect;
	m_buffer_cache.InvalidateMemory(vaddr, size);
	m_texture_cache.InvalidateMemory(vaddr, size);
	return true;
}

bool RenderContext::IsMapped(uint64_t vaddr, uint64_t size) const noexcept {
	if (!GuestRange {vaddr, size}.Valid()) {
		return false;
	}
	std::shared_lock lock(m_mapped_ranges_mutex);
	return m_mapped_ranges.Contains(vaddr, size);
}

bool RenderContext::SynchronizeGpuBackingForRead(uint64_t vaddr, uint64_t size) {
	if (!GuestGpu::IsGpuThread() || CommandScheduler::InDeferredOperation() ||
	    !m_command_scheduler.Active() || m_command_scheduler.Current().IsInvalid() ||
	    !IsMapped(vaddr, size) || m_texture_cache.IsRegionGpuModified(vaddr, size)) {
		return false;
	}

	// Exact dirty ranges are removed when a download is queued, before its backing publication.
	// Retired images can likewise have an outstanding publication without a live image owner.
	// Never hold the registry, mapping, texture-cache, or tracker locks over these waits.
	// Side readbacks are published by their faulting guest thread; finish any overlapping one
	// here (already submitted, so this never waits for the current recording).
	m_buffer_cache.CompleteSideReadbacks(vaddr, size);
	if (const auto tick = m_buffer_cache.PendingBackingPublicationTick(vaddr, size)) {
		m_command_scheduler.Wait(*tick);
		m_command_scheduler.WaitPriorityOperations(*tick);
	}
	if (m_buffer_cache.HasGpuDirtyBytes(vaddr, size)) {
		// Retain the CPU fault path's canonical-buffer discovery, clipped readback window,
		// actual backing copy, and dirty-page ownership transition.
		HangTrace::SetReadbackKind(HangTrace::ReadbackKind::GpuSync);
		m_buffer_cache.ReadMemory(vaddr, size);
		HangTrace::SetReadbackKind(HangTrace::ReadbackKind::Invalidate);
	}
	// A publication may already have finished since the failed strict read. Such a range is
	// ready too; callers still retry the actual read and bound their preparation retries.
	return IsMapped(vaddr, size) && !m_buffer_cache.HasGpuDirtyBytes(vaddr, size) &&
	       !m_texture_cache.IsRegionGpuModified(vaddr, size) &&
	       !m_buffer_cache.HasPendingBackingPublication(vaddr, size);
}

void RenderContext::MapMemory(uint64_t vaddr, uint64_t size) {
	std::lock_guard lock(m_mapped_ranges_mutex);
	// GPU mapping changes retire clean-read verdicts (the backing translation has its own
	// generation in the guest address space).
	CleanVerdict::Invalidate(vaddr, size, Coherence::Source::MapMemory);
	m_mapped_ranges.Add(vaddr, size);
	m_buffer_cache.InvalidateBdaSynchronization();
	SyncEpoch::Advance();
}

void RenderContext::UnmapMemory(uint64_t vaddr, uint64_t size) {
	if (CommandScheduler::InDeferredOperation()) {
		EXIT("unsupported memory unmap from an asynchronous GPU completion, "
		     "addr=0x%016" PRIx64 " size=0x%016" PRIx64 "\n",
		     vaddr, size);
	}
	// The kernel unmaps every free range it is about to (re)map, most of which were never GPU
	// mapped. Buffers, images and GPU-dirty pages only exist inside GPU-mapped ranges, so such an
	// unmap has nothing to invalidate; skip the GPU round trip and full drain it would force.
	if (GuestRange {vaddr, size}.Valid()) {
		std::shared_lock lock(m_mapped_ranges_mutex);
		if (!m_mapped_ranges.Intersects(vaddr, size)) {
			return;
		}
	}
	const auto unmap = [this, vaddr, size] {
		if (m_command_scheduler.Active()) {
			Profiler::ScopedGpuWaitReason wait_reason(Profiler::FrameWait::GpuWaitUnmap);
			const auto                    tick = m_command_scheduler.CurrentTick();
			m_command_scheduler.Finish();
			m_command_scheduler.WaitPriorityOperations(tick);
		}
		m_buffer_cache.InvalidateMemory(vaddr, size);
		m_texture_cache.UnmapMemory(vaddr, size);
		std::lock_guard lock(m_mapped_ranges_mutex);
		CleanVerdict::Invalidate(vaddr, size, Coherence::Source::UnmapMemory);
		m_mapped_ranges.Subtract(vaddr, size);
		m_buffer_cache.InvalidateBdaSynchronization();
		SyncEpoch::Advance();
	};
	// Shutdown still owns the GPU while queued rendering drains, but its command lane no
	// longer accepts external work. Use the guest GPU's state for the teardown route.
	if (m_gpu == nullptr || m_gpu->IsStopping()) {
		unmap();
		return;
	}
	m_gpu->SendCommandSync(unmap);
}

void RenderContext::PrepareBda() {
	std::shared_lock lock(m_mapped_ranges_mutex);
	m_buffer_cache.SynchronizeBdaBuffers(m_mapped_ranges);
	m_fault_process_pending = true;
}

void RenderContext::NoteHostBackingWrite(uint64_t vaddr, uint64_t size,
                                         HostWriter writer) noexcept {
	const auto states = m_buffer_cache.CountPageStates(vaddr, size);
	Profiler::CountFrameEvent(Profiler::FrameEvent::HostBackingWrites);
	if (states.gpu_dirty == 0 && states.clean == 0) {
		return;
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::HostBackingWriteGpuDirtyPages, states.gpu_dirty);
	Profiler::CountFrameEvent(Profiler::FrameEvent::HostBackingWriteCleanPages, states.clean);
	static std::atomic<uint32_t> logged {0};
	if (logged.fetch_add(1, std::memory_order_relaxed) < 16) {
		std::fprintf(stderr,
		             "HostBackingWrite: %s addr=0x%016" PRIx64 " size=0x%" PRIx64
		             " lands on %" PRIu64 " GPU-dirty and %" PRIu64
		             " clean tracked page(s) the tracker is not told about\n",
		             writer == HostWriter::LodStats ? "LOD-statistics report" : "occlusion result",
		             vaddr, size, states.gpu_dirty, states.clean);
	}
}

void RenderContext::NoteGuestProtection(uint64_t vaddr, uint64_t size, bool allows_read,
                                        bool allows_write) noexcept {
	Profiler::CountFrameEvent(Profiler::FrameEvent::GuestProtectCalls);
	{
		std::shared_lock lock(m_mapped_ranges_mutex);
		if (!m_mapped_ranges.Intersects(vaddr, size)) {
			return;
		}
	}
	const auto watched = m_page_manager.CountWatchedPages(vaddr, size);
	// Pages whose tracking protection the new guest mode replaces: a write-watched page that
	// becomes writable, an access-watched page that becomes readable or writable.
	const auto overridden = (allows_write ? watched.write : 0u) +
	                        (allows_read || allows_write ? watched.access : 0u);
	const bool restricts = !allows_read || !allows_write;
	Profiler::CountFrameEvent(Profiler::FrameEvent::GuestProtectWatchedPages,
	                          watched.write + watched.access);
	Profiler::CountFrameEvent(Profiler::FrameEvent::GuestProtectOverriddenPages, overridden);
	if (restricts) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::GuestProtectRestrictsGpuMemory);
	}
	if (overridden == 0 && !restricts) {
		return;
	}
	static std::atomic<uint32_t> logged {0};
	if (logged.fetch_add(1, std::memory_order_relaxed) < 16) {
		std::fprintf(stderr,
		             "GuestProtect: addr=0x%016" PRIx64 " size=0x%" PRIx64
		             " read=%d write=%d on GPU memory: %" PRIu64 " write-watched and %" PRIu64
		             " access-watched page(s), %" PRIu64 " whose tracking protection it replaces\n",
		             vaddr, size, allows_read ? 1 : 0, allows_write ? 1 : 0, watched.write,
		             watched.access, overridden);
	}
}

void RenderContext::RunGarbageCollector() {
	if (m_fault_process_pending) {
		m_fault_process_pending = false;
		m_buffer_cache.ProcessFaultBuffer();
	}
	m_texture_cache.ProcessDownloadImages();
	m_texture_cache.RunGarbageCollector();
	m_buffer_cache.RunGarbageCollector();
}

void RenderContext::AddInterruptEq(LibKernel::EventQueue::KernelEqueue eq, int event_id) {
	Common::LockGuard lock(m_interrupt_mutex);

	auto it = std::find_if(
	    m_interrupt_eqs.begin(), m_interrupt_eqs.end(),
	    [eq, event_id](const auto& entry) { return entry.eq == eq && entry.event_id == event_id; });
	if (it != m_interrupt_eqs.end()) {
		return;
	}

	m_interrupt_eqs.push_back({eq, event_id});
}

void RenderContext::DeleteInterruptEq(LibKernel::EventQueue::KernelEqueue eq, int event_id) {
	Common::LockGuard lock(m_interrupt_mutex);

	auto it = std::find_if(
	    m_interrupt_eqs.begin(), m_interrupt_eqs.end(),
	    [eq, event_id](const auto& entry) { return entry.eq == eq && entry.event_id == event_id; });
	if (it == m_interrupt_eqs.end()) {
		return;
	}

	m_interrupt_eqs.erase(it);
}

void RenderContext::TriggerInterrupt(int event_id, uint32_t context_id) {
	std::vector<InterruptEqRegistration> registrations;
	{
		Common::LockGuard lock(m_interrupt_mutex);
		for (const auto& registration: m_interrupt_eqs) {
			if (registration.event_id == event_id) {
				registrations.push_back(registration);
			}
		}
	}

	for (const auto& registration: registrations) {
		const auto result = LibKernel::EventQueue::KernelTriggerEvent(
		    registration.eq, static_cast<uintptr_t>(registration.event_id),
		    LibKernel::EventQueue::KERNEL_EVFILT_GRAPHICS,
		    reinterpret_cast<void*>(static_cast<uintptr_t>(context_id)));
		if (result == LibKernel::KERNEL_ERROR_EBADF || result == LibKernel::KERNEL_ERROR_ENOENT) {
			DeleteInterruptEq(registration.eq, registration.event_id);
			continue;
		}
		EXIT_NOT_IMPLEMENTED(result != OK);
	}
}

} // namespace Libs::Graphics
