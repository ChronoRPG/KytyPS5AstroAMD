#include "graphics/host_gpu/memoryTracker.h"

#include "common/alignment.h"
#include "common/assert.h"

namespace Libs::Graphics {

static_assert(std::atomic<void*>::is_always_lock_free);

MemoryTracker::MemoryTracker(PageManager& page_manager, bool track_cpu_mutations,
                             FaultPolicy fault_policy)
    : m_page_manager(page_manager), m_track_cpu_mutations(track_cpu_mutations),
      m_fault_policy(fault_policy) {
	if (m_fault_policy.ahead_pages == 0 ||
	    (m_fault_policy.ahead_pages & (m_fault_policy.ahead_pages - 1)) != 0 ||
	    m_fault_policy.ahead_pages > TRACKER_REGION_PAGES) {
		EXIT("invalid memory tracker fault-ahead window\n");
	}
	m_regions = std::make_unique<std::atomic<RegionManager*>[]>(REGION_COUNT);
}

bool MemoryTracker::IsRegionHot(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	return Iterate<false>(vaddr, size, [](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		return manager->IsHot(manager->GetCpuAddr() + offset, bytes);
	});
}

void MemoryTracker::DemoteHotPages(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	if (m_hot_count.load(std::memory_order_relaxed) == 0) {
		return;
	}
	uint32_t demoted = 0;
	Iterate<false>(vaddr, size, [&](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		demoted += manager->DemoteHot(manager->GetCpuAddr() + offset, bytes, m_hot_count);
	});
	MemoryStats::Count(MemoryStats::Counter::HotDemotions, demoted);
}

std::vector<uint64_t> MemoryTracker::SettleHotPages(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	std::vector<uint64_t> pages;
	if (m_hot_count.load(std::memory_order_relaxed) == 0) {
		return pages;
	}
	const auto collect = [&](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		(void)manager->SettleHot(manager->GetCpuAddr() + offset, bytes, m_hot_count,
		                         [&pages](uint64_t page) { pages.push_back(page); });
	};
	if (size != 0) {
		Iterate<false>(vaddr, size, collect);
	} else {
		std::vector<RegionManager*> managers;
		{
			std::lock_guard lock(m_region_mutex);
			managers.reserve(m_region_storage.size());
			for (const auto& manager: m_region_storage) {
				managers.push_back(manager.get());
			}
		}
		for (auto* manager: managers) {
			collect(manager, 0, TRACKER_REGION_SIZE);
		}
	}
	MemoryStats::Count(MemoryStats::Counter::HotDemotions, pages.size());
	return pages;
}

void MemoryTracker::SweepHotPages(uint32_t idle_frames) {
	CheckNotInUploadCallback();
	if (m_hot_count.load(std::memory_order_relaxed) == 0) {
		return;
	}
	std::vector<RegionManager*> managers;
	{
		std::lock_guard lock(m_region_mutex);
		managers.reserve(m_region_storage.size());
		for (const auto& manager: m_region_storage) {
			managers.push_back(manager.get());
		}
	}
	const auto frame   = Frame();
	uint32_t   demoted = 0;
	for (auto* manager: managers) {
		std::scoped_lock lock(manager->lock);
		demoted += manager->SweepHot(frame, idle_frames, m_hot_count);
	}
	MemoryStats::Count(MemoryStats::Counter::HotDemotions, demoted);
}

MemoryTracker::~MemoryTracker() = default;

void MemoryTracker::NotifyCpuMutation() noexcept {
	if (!m_track_cpu_mutations) {
		return;
	}
	// Publish before dirtying/unprotecting under the region lock. A scanner observing this
	// token still acquires that lock; saturation permanently disables token reuse, avoiding ABA.
	auto epoch = m_cpu_mutation_epoch.load(std::memory_order_relaxed);
	while (epoch != UINT64_MAX &&
	       !m_cpu_mutation_epoch.compare_exchange_weak(epoch, epoch + 1,
	                                                   std::memory_order_release,
	                                                   std::memory_order_relaxed)) {}
}

#if KYTY_BUILD == KYTY_BUILD_DEBUG
void MemoryTracker::ValidateGpuDirtyPages(const RangeSet& dirty, uint64_t vaddr, uint64_t size,
                                          const char* operation) const noexcept {
	if (!GuestRange {vaddr, size}.Valid() || (vaddr & (TRACKER_PAGE_SIZE - 1)) != 0 ||
	    (size & (TRACKER_PAGE_SIZE - 1)) != 0) {
		EXIT("MemoryTracker: invalid dirty-page validation range\n");
	}
	for (auto page = vaddr; page < vaddr + size; page += TRACKER_PAGE_SIZE) {
		if (!dirty.Intersects(page, TRACKER_PAGE_SIZE)) {
			EXIT("MemoryTracker: GPU-dirty tracker page has no dirty bytes, operation=%s "
			     "addr=0x%016" PRIx64 "\n",
			     operation, page);
		}
	}
}

void MemoryTracker::ValidateGpuDirtyOwnership(const RangeSet& dirty, uint64_t vaddr, uint64_t size,
                                              const char* operation) {
	ValidateRange(vaddr, size);
	const auto begin = Common::AlignDown(vaddr, TRACKER_PAGE_SIZE);
	const auto end   = Common::AlignUp(vaddr + size, TRACKER_PAGE_SIZE);
	for (auto page = begin; page < end; page += TRACKER_PAGE_SIZE) {
		const bool has_dirty_bytes = dirty.Intersects(page, TRACKER_PAGE_SIZE);
		if (IsRegionGpuModified(page, TRACKER_PAGE_SIZE) != has_dirty_bytes) {
			EXIT("MemoryTracker: tracker and byte ownership disagree, operation=%s "
			     "addr=0x%016" PRIx64 "\n",
			     operation, page);
		}
	}
}
#endif

void MemoryTracker::ValidateRange(uint64_t vaddr, uint64_t size) {
	if (!GuestRange {vaddr, size}.Valid()) {
		EXIT("invalid memory tracker range\n");
	}
}

RegionManager* MemoryTracker::GetOrCreateRegion(uint64_t index) {
	if (auto* manager = m_regions[index].load(std::memory_order_acquire); manager != nullptr) {
		return manager;
	}
	std::lock_guard lock(m_region_mutex);
	if (auto* manager = m_regions[index].load(std::memory_order_acquire); manager != nullptr) {
		return manager;
	}
	auto  manager = std::make_unique<RegionManager>(m_page_manager, index * TRACKER_REGION_SIZE);
	auto* ptr     = manager.get();
	m_region_storage.push_back(std::move(manager));
	// New regions start entirely CPU dirty. Notify before making the region visible.
	NotifyCpuMutation();
	m_regions[index].store(ptr, std::memory_order_release);
	return ptr;
}

bool MemoryTracker::IsRegionCpuModified(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	return Iterate<true>(vaddr, size, [](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		return manager->IsModified<DirtySource::Cpu>(offset, bytes);
	});
}

bool MemoryTracker::IsRegionGpuModified(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	return Iterate<false>(vaddr, size, [](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		return manager->IsModified<DirtySource::Gpu>(offset, bytes);
	});
}

void MemoryTracker::MarkRegionAsCpuModified(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	Iterate<true>(vaddr, size, [this](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		NotifyCpuMutation();
		manager->ChangeState<DirtySource::Cpu, true>(manager->GetCpuAddr() + offset, bytes);
	});
}

void MemoryTracker::MarkRegionAsGpuModified(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	CleanVerdict::Invalidate(vaddr, size, Coherence::Source::TrackerGpuMark);
	Iterate<true>(vaddr, size, [](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		manager->ChangeState<DirtySource::Gpu, true>(manager->GetCpuAddr() + offset, bytes);
	});
}

void MemoryTracker::UnmarkRegionAsGpuModified(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	CleanVerdict::Invalidate(vaddr, size, Coherence::Source::TrackerGpuUnmark);
	Iterate<false>(vaddr, size, [](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		manager->ChangeState<DirtySource::Gpu, false>(manager->GetCpuAddr() + offset, bytes);
	});
}

void MemoryTracker::MarkReadbackPending(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	// No clean-verdict bump: no dirty or protection state changes here.
	Iterate<false>(vaddr, size, [](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		manager->MarkReadbackPending(manager->GetCpuAddr() + offset, bytes);
	});
}

MemoryTracker::ReadbackUnmarkResult MemoryTracker::UnmarkReadbackPending(uint64_t vaddr,
                                                                         uint64_t size) {
	CheckNotInUploadCallback();
	CleanVerdict::Invalidate(vaddr, size, Coherence::Source::TrackerReadbackUnmark);
	ReadbackUnmarkResult result;
	Iterate<false>(vaddr, size, [&result](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		std::scoped_lock lock(manager->lock);
		const auto [unmarked, retained] =
		    manager->ClearReadbackPending(manager->GetCpuAddr() + offset, bytes);
		result.unmarked_pages += unmarked;
		result.retained_pages += retained;
	});
	return result;
}

void MemoryTracker::UntrackMemory(uint64_t vaddr, uint64_t size) {
	CheckNotInUploadCallback();
	std::vector<RegionManager*> managers;
	managers.reserve((vaddr % TRACKER_REGION_SIZE + size + TRACKER_REGION_SIZE - 1) /
	                 TRACKER_REGION_SIZE);
	Iterate<false>(vaddr, size, [&](RegionManager* manager, uint64_t, uint64_t) {
		managers.push_back(manager);
	});

	std::vector<std::unique_lock<TrackingSpinLock>> locks;
	locks.reserve(managers.size());
	for (auto* manager: managers) {
		locks.emplace_back(manager->lock);
	}
	if (Iterate<false>(vaddr, size, [](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		    return manager->IsModified<DirtySource::Gpu>(offset, bytes);
	    })) {
		EXIT("cannot untrack GPU-dirty memory\n");
	}
	uint32_t demoted = 0;
	Iterate<false>(vaddr, size, [&](RegionManager* manager, uint64_t offset, uint64_t bytes) {
		NotifyCpuMutation();
		manager->ChangeState<DirtySource::Cpu, true>(manager->GetCpuAddr() + offset, bytes);
		demoted += manager->DemoteHot(manager->GetCpuAddr() + offset, bytes, m_hot_count);
	});
	MemoryStats::Count(MemoryStats::Counter::HotDemotions, demoted);
}

} // namespace Libs::Graphics
