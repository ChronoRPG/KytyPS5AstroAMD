#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_MEMORYTRACKER_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_MEMORYTRACKER_H_

#include "common/assert.h"
#include "graphics/host_gpu/cleanVerdictCache.h"
#include "graphics/host_gpu/pageManager.h"
#include "graphics/host_gpu/rangeSet.h"
#include "graphics/host_gpu/regionManager.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace Libs::Graphics {

class MemoryTracker final {
public:
	using FaultPolicy = RegionManager::FaultPolicy;

	explicit MemoryTracker(PageManager& page_manager, bool track_cpu_mutations = false,
	                       FaultPolicy fault_policy = {});
	~MemoryTracker();

	KYTY_CLASS_NO_COPY(MemoryTracker);

	[[nodiscard]] bool IsRegionCpuModified(uint64_t vaddr, uint64_t size);
	[[nodiscard]] bool IsRegionGpuModified(uint64_t vaddr, uint64_t size);
	// IsRegionCpuModified and IsRegionGpuModified of one range with each region lock taken once.
	// cpu is what IsRegionCpuModified would return (it creates missing regions, which start CPU
	// dirty) unless gpu is set: then missing regions are left alone and cpu is not meaningful,
	// exactly as `!IsRegionGpuModified(...) && IsRegionCpuModified(...)` never asks.
	struct DirtyState {
		bool cpu = false;
		bool gpu = false;
	};
	[[nodiscard]] DirtyState QueryDirty(uint64_t vaddr, uint64_t size);
	// IsRegionGpuModified without the region locks, on the regions' lock-free mirrors of their
	// GPU-dirty bits (RegionManager::IsGpuModifiedRelaxed). Any thread. A hint: a transition racing
	// it may or may not be seen, as with a locked query made a moment earlier or later.
	[[nodiscard]] bool IsRegionGpuModifiedRelaxed(uint64_t vaddr, uint64_t size) const;
	// Verify mode: under each region lock, whether the mirror equals the GPU-dirty bits.
	[[nodiscard]] bool GpuMirrorMatches(uint64_t vaddr, uint64_t size);
	void               MarkRegionAsCpuModified(uint64_t vaddr, uint64_t size);
	void               MarkRegionAsGpuModified(uint64_t vaddr, uint64_t size);
	void               UnmarkRegionAsGpuModified(uint64_t vaddr, uint64_t size);
	void               UntrackMemory(uint64_t vaddr, uint64_t size);
	// Side readbacks: mark the GPU-dirty pages of a range whose dirty bytes a side copy now
	// publishes. Any later GPU ownership change of a page cancels its mark. Unmark clears
	// the GPU bit (and read protection) only of pages whose mark survived; it may run on any
	// thread after the copy's backing publication has completed.
	struct ReadbackUnmarkResult {
		uint64_t unmarked_pages = 0;
		uint64_t retained_pages = 0;
	};
	void                               MarkReadbackPending(uint64_t vaddr, uint64_t size);
	[[nodiscard]] ReadbackUnmarkResult UnmarkReadbackPending(uint64_t vaddr, uint64_t size);
	// A dirty-state mutation token, not a backing/GPU cleanliness proof. UINT64_MAX means
	// saturated: callers must conservatively stop reusing any previously observed token. While
	// hot pages exist it is UINT64_MAX too: they are written without faults.
	[[nodiscard]] uint64_t CpuMutationEpoch() const noexcept {
		if (m_hot_count.load(std::memory_order_acquire) != 0) {
			return UINT64_MAX;
		}
		return m_cpu_mutation_epoch.load(std::memory_order_acquire);
	}
	// The same token without the hot-page override (UINT64_MAX only when saturated). It changes
	// on every transition that can make a page CPU-dirty and writable outside the hot set: write
	// faults (including the ones promoting a page to hot), explicit CPU-dirty marks, new regions,
	// untracking, and hot pages returning to normal tracking while still CPU-dirty (demotion,
	// idle sweep). Hot pages themselves are written without faults and never change it: a caller
	// relying on it must re-examine every hot page it has seen (BufferCache::SynchronizeBdaBuffers).
	[[nodiscard]] uint64_t FaultMutationEpoch() const noexcept {
		return m_cpu_mutation_epoch.load(std::memory_order_acquire);
	}
	// The sum of the mutation serials (RegionManager::Serial) of the regions [vaddr, vaddr + size)
	// spans, or 0 when one of them does not exist yet (or the range is invalid). Serials only grow,
	// so while this sum is unchanged no CPU-dirty, GPU-dirty, hot or readback-pending bit of any
	// page of the range changed. Lock-free: a transition takes the region lock and advances the
	// serial before it changes a bit, so a signature read now describes the bits as they are now
	// (a transition still waiting for the lock has not happened yet). Any thread.
	[[nodiscard]] uint64_t RangeSignature(uint64_t vaddr, uint64_t size) const noexcept {
		if (size == 0 || vaddr >= TRACKER_ADDRESS_SIZE || size > TRACKER_ADDRESS_SIZE - vaddr) {
			return 0;
		}
		uint64_t   signature = 0;
		const auto last      = (vaddr + size - 1) / TRACKER_REGION_SIZE;
		for (auto index = vaddr / TRACKER_REGION_SIZE; index <= last; index++) {
			const auto* manager = m_regions[index].load(std::memory_order_acquire);
			if (manager == nullptr) {
				return 0;
			}
			signature += manager->Serial();
		}
		return signature;
	}
	// Removes protection from a range and flushes GPU-owned data when required.
	template <typename Flush>
	void InvalidateRegion(uint64_t vaddr, uint64_t size, Flush&& on_flush) noexcept {
		InvalidateRegion(vaddr, size, on_flush, false, [](uint64_t, uint64_t) noexcept {});
	}
	// As InvalidateRegion for a guest write fault: clean pages take the fault policy (fault-ahead
	// window, hot-page detection, RegionManager::MarkWriteFault). on_ahead(address, bytes) sees
	// every run of pages fault-ahead makes CPU-dirty, under the region lock, before they become
	// writable: what the faulting range's caller does before a write can land applies to them.
	template <typename Flush, typename AheadFunc>
	void InvalidateRegionOnWriteFault(uint64_t vaddr, uint64_t size, Flush&& on_flush,
	                                  AheadFunc&& on_ahead) noexcept {
		InvalidateRegion(vaddr, size, on_flush, true, on_ahead);
	}

private:
	template <typename Flush, typename AheadFunc>
	void InvalidateRegion(uint64_t vaddr, uint64_t size, Flush& on_flush, bool write_fault,
	                      AheadFunc&& on_ahead) noexcept {
		static_assert(std::is_invocable_v<Flush&>);
		static_assert(std::is_nothrow_invocable_v<AheadFunc&, uint64_t, uint64_t>);
		CheckNotInUploadCallback();

		Iterate<false>(vaddr, size, [&](RegionManager* manager, uint64_t offset, uint64_t bytes) {
			RegionManager::FaultResult fault;
			const bool should_flush = [&] {
				// Perform both the GPU modification check and CPU state change with the lock in
				// case the GPU thread is racing to mark the page modified. If a flush is needed,
				// on_flush performs the CPU state change.
				std::scoped_lock lock(manager->lock);
				if (manager->IsModified<DirtySource::Gpu>(offset, bytes)) {
					return true;
				}
				NotifyCpuMutation();
				if (write_fault) {
					fault = manager->MarkWriteFault(manager->GetCpuAddr() + offset, bytes,
					                                m_fault_policy, Frame(), m_hot_count,
					                                on_ahead);
				} else {
					manager->ChangeState<DirtySource::Cpu, true>(manager->GetCpuAddr() + offset,
					                                             bytes);
				}
				return false;
			}();
			if (should_flush) {
				on_flush();
			}
			MemoryStats::Count(MemoryStats::Counter::FaultAheadPages, fault.ahead_pages);
			MemoryStats::Count(MemoryStats::Counter::HotPromotions, fault.promoted);
		});
	}

public:
	// Fault policy knobs (constant after construction).
	[[nodiscard]] const FaultPolicy& GetFaultPolicy() const noexcept { return m_fault_policy; }
	// Guest frame counter for hot-page detection (any thread, once per completed guest flip).
	void AdvanceFrame() noexcept { m_frame.fetch_add(1, std::memory_order_relaxed); }
	[[nodiscard]] uint32_t Frame() const noexcept {
		return m_frame.load(std::memory_order_relaxed);
	}
	[[nodiscard]] uint32_t HotPageCount() const noexcept {
		return m_hot_count.load(std::memory_order_relaxed);
	}
	[[nodiscard]] bool IsRegionHot(uint64_t vaddr, uint64_t size);
	// Returns hot pages of the range to normal tracking (they stay CPU-dirty until uploaded).
	void DemoteHotPages(uint64_t vaddr, uint64_t size);
	// Demotes hot pages no upload visited for more than idle_frames frames.
	void SweepHotPages(uint32_t idle_frames);
	// Returns the hot pages of [vaddr, vaddr + size) (every hot page when size is 0) to normal
	// tracking as clean, write-protected pages (RegionManager::SettleHot) and lists them. The
	// caller must mark each one whose contents changed since its last upload CPU-dirty again.
	[[nodiscard]] std::vector<uint64_t> SettleHotPages(uint64_t vaddr, uint64_t size);
#if KYTY_BUILD == KYTY_BUILD_DEBUG
	void ValidateGpuDirtyPages(const RangeSet& dirty, uint64_t vaddr, uint64_t size,
	                           const char* operation) const noexcept;
	void ValidateGpuDirtyOwnership(const RangeSet& dirty, uint64_t vaddr, uint64_t size,
	                               const char* operation);
#else
	void ValidateGpuDirtyPages(const RangeSet&, uint64_t, uint64_t, const char*) const noexcept {}
	void ValidateGpuDirtyOwnership(const RangeSet&, uint64_t, uint64_t, const char*) {}
#endif

	template <bool clear, typename Func>
	void ForEachDownloadRange(uint64_t vaddr, uint64_t size, Func&& func) {
		static_assert(std::is_nothrow_invocable_v<Func&, uint64_t, uint64_t>);
		CheckNotInUploadCallback();
		if constexpr (clear) {
			CleanVerdict::Invalidate(vaddr, size, Coherence::Source::TrackerDownload);
		}
		Iterate<false>(vaddr, size, [&](RegionManager* manager, uint64_t offset, uint64_t bytes) {
			std::scoped_lock lock(manager->lock);
			const auto       address = manager->GetCpuAddr() + offset;
			manager->template ForEachModifiedRange<DirtySource::Gpu, false>(address, bytes, func);
			if constexpr (clear) {
				manager->template ChangeState<DirtySource::Gpu, false>(address, bytes);
			}
		});
	}

	// range_func(address, bytes) or range_func(address, bytes, hot). Only a caller taking the hot
	// flag keeps hot pages CPU-dirty and writable on a read-only upload; it must then treat every
	// hot range as possibly changed since its previous upload. Written uploads and callers
	// without the flag return hot pages to normal tracking first.
	template <typename RangeFunc, typename UploadFunc>
	void ForEachUploadRange(uint64_t vaddr, uint64_t size, bool is_written, RangeFunc&& range_func,
	                        UploadFunc&& upload_func) {
		constexpr bool hot_aware = std::is_invocable_v<RangeFunc&, uint64_t, uint64_t, bool>;
		if constexpr (hot_aware) {
			static_assert(std::is_nothrow_invocable_v<RangeFunc&, uint64_t, uint64_t, bool>);
		} else {
			static_assert(std::is_nothrow_invocable_v<RangeFunc&, uint64_t, uint64_t>);
		}
		static_assert(std::is_nothrow_invocable_v<UploadFunc&>);
		CheckNotInUploadCallback();
		Iterate<true>(vaddr, size, [](RegionManager*, uint64_t, uint64_t) {});
		const auto* previous_upload_owner = std::exchange(s_upload_owner, this);
		const bool  keep_hot              = hot_aware && !is_written;
		const auto  frame                 = Frame();
		uint32_t    demoted               = 0;
		Iterate<false>(vaddr, size, [&](RegionManager* manager, uint64_t offset, uint64_t bytes) {
			manager->lock.lock();
			demoted += manager->CollectUpload(
			    manager->GetCpuAddr() + offset, bytes, keep_hot, frame, m_hot_count,
			    [&](uint64_t address, uint64_t range_bytes, bool hot) noexcept {
				    if constexpr (hot_aware) {
					    range_func(address, range_bytes, hot);
				    } else {
					    range_func(address, range_bytes);
				    }
			    });
			if (!is_written) {
				manager->lock.unlock();
			}
		});
		MemoryStats::Count(MemoryStats::Counter::HotDemotions, demoted);
		upload_func();
		// No clean-verdict bump: these GPU bits are not read by clean-read verdicts, and the
		// only writer (BufferCache::ObtainBuffer) bumps when it adds the exact dirty range.
		if (is_written) {
			Iterate<false>(vaddr, size,
			               [](RegionManager* manager, uint64_t offset, uint64_t bytes) {
				               manager->template ChangeState<DirtySource::Gpu, true>(
				                   manager->GetCpuAddr() + offset, bytes);
				               manager->lock.unlock();
			               });
		}
		s_upload_owner = previous_upload_owner;
	}

	// A written upload (the range becomes GPU-owned) that copies its CPU-dirty pages WITHOUT the
	// region locks held, for FaultPolicy::copy_outside_lock:
	//  1. per region, under its lock: clear and write-protect the CPU-dirty pages, reporting them
	//     to range_func, then release the lock;
	//  2. upload_func() copies them with no tracker lock held. A guest write to one of them now
	//     faults, and the fault marks it CPU-dirty again under the region lock;
	//  3. all region locks are taken and held; pages that became CPU-dirty since step 1 are
	//     cleared and write-protected again (only then can their contents no longer change) and
	//     reported to late_range_func; late_upload_func() copies them under the locks;
	//  4. the whole range becomes GPU-dirty and the locks are released.
	// Pages not reported in step 3 stayed write-protected and clean from step 1 to step 4, so the
	// step-2 copy is exact; step 3 and 4 are the single locked upload of ForEachUploadRange.
	template <typename RangeFunc, typename UploadFunc, typename LateRangeFunc,
	          typename LateUploadFunc>
	void ForEachWrittenUploadRange(uint64_t vaddr, uint64_t size, RangeFunc&& range_func,
	                               UploadFunc&& upload_func, LateRangeFunc&& late_range_func,
	                               LateUploadFunc&& late_upload_func) {
		static_assert(std::is_nothrow_invocable_v<RangeFunc&, uint64_t, uint64_t>);
		static_assert(std::is_nothrow_invocable_v<UploadFunc&>);
		static_assert(std::is_nothrow_invocable_v<LateRangeFunc&, uint64_t, uint64_t>);
		static_assert(std::is_nothrow_invocable_v<LateUploadFunc&>);
		CheckNotInUploadCallback();
		Iterate<true>(vaddr, size, [](RegionManager*, uint64_t, uint64_t) {});
		const auto* previous_upload_owner = std::exchange(s_upload_owner, this);
		const auto  frame                 = Frame();
		uint32_t    demoted               = 0;
		Iterate<false>(vaddr, size, [&](RegionManager* manager, uint64_t offset, uint64_t bytes) {
			std::scoped_lock lock(manager->lock);
			demoted += manager->CollectUpload(
			    manager->GetCpuAddr() + offset, bytes, false, frame, m_hot_count,
			    [&](uint64_t address, uint64_t range_bytes, bool) noexcept {
				    range_func(address, range_bytes);
			    });
		});
		upload_func();
		uint64_t late_pages = 0;
		Iterate<false>(vaddr, size, [&](RegionManager* manager, uint64_t offset, uint64_t bytes) {
			manager->lock.lock();
			demoted += manager->CollectUpload(
			    manager->GetCpuAddr() + offset, bytes, false, frame, m_hot_count,
			    [&](uint64_t address, uint64_t range_bytes, bool) noexcept {
				    late_pages += range_bytes / TRACKER_PAGE_SIZE;
				    late_range_func(address, range_bytes);
			    });
		});
		late_upload_func();
		Iterate<false>(vaddr, size, [](RegionManager* manager, uint64_t offset, uint64_t bytes) {
			manager->template ChangeState<DirtySource::Gpu, true>(manager->GetCpuAddr() + offset,
			                                                      bytes);
			manager->lock.unlock();
		});
		s_upload_owner = previous_upload_owner;
		MemoryStats::Count(MemoryStats::Counter::HotDemotions, demoted);
		MemoryStats::Count(MemoryStats::Counter::WrittenUploadLatePages, late_pages);
	}

private:
	static constexpr size_t REGION_COUNT = TRACKER_ADDRESS_SIZE / TRACKER_REGION_SIZE;
	inline static thread_local const MemoryTracker* s_upload_owner = nullptr;

	void CheckNotInUploadCallback() const noexcept {
		if (s_upload_owner == this) {
			EXIT("memory tracker re-entered from upload callback\n");
		}
	}

	template <bool create, typename Func>
	bool Iterate(uint64_t vaddr, uint64_t size, Func&& func) {
		ValidateRange(vaddr, size);
		using Result = std::invoke_result_t<Func, RegionManager*, uint64_t, uint64_t>;
		constexpr bool returns_bool = std::is_same_v<Result, bool>;
		uint64_t       remaining    = size;
		uint64_t       index        = vaddr / TRACKER_REGION_SIZE;
		uint64_t       offset       = vaddr % TRACKER_REGION_SIZE;
		while (remaining != 0) {
			const auto bytes   = std::min(TRACKER_REGION_SIZE - offset, remaining);
			auto*      manager = m_regions[index].load(std::memory_order_acquire);
			if (manager == nullptr && create) {
				manager = GetOrCreateRegion(index);
			}
			if (manager != nullptr) {
				if constexpr (returns_bool) {
					if (func(manager, offset, bytes)) {
						return true;
					}
				} else {
					func(manager, offset, bytes);
				}
			}
			remaining -= bytes;
			offset = 0;
			index++;
		}
		return false;
	}

	static void    ValidateRange(uint64_t vaddr, uint64_t size);
	RegionManager* GetOrCreateRegion(uint64_t index);
	void           NotifyCpuMutation() noexcept;

	std::unique_ptr<std::atomic<RegionManager*>[]> m_regions;
	std::vector<std::unique_ptr<RegionManager>>    m_region_storage;
	std::mutex                                     m_region_mutex;
	PageManager&                                   m_page_manager;
	const bool                                     m_track_cpu_mutations;
	std::atomic_uint64_t                            m_cpu_mutation_epoch {1};
	const FaultPolicy                              m_fault_policy;
	std::atomic_uint32_t                           m_frame {1};
	std::atomic_uint32_t                           m_hot_count {0};
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_MEMORYTRACKER_H_
