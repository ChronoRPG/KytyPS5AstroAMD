#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_MEMORYSTATS_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_MEMORYSTATS_H_

// Guest memory tracking and buffer upload counters. Every counter is published twice: as a
// Profiler FrameEvent (cumulative, per guest flip; KYTY_PROFILE_FRAMES_ONLY=1 plus
// KYTY_PROFILE_AGGREGATES=1 and a connected profiler) and as a hang-trace summary.csv mem_*
// column (per second; KYTY_HANG_TRACE=1). With both off a count costs two predictable branches
// on cached flags and a timer is never read.

#include "common/hangTrace.h"
#include "common/profiler.h"

#include <array>
#include <cstdint>

namespace Libs::Graphics::MemoryStats {

using Counter = HangTrace::MemoryCounter;

namespace Detail {
// Profiler FrameEvent of each HangTrace::MemoryCounter, in enum order.
inline constexpr std::array<Profiler::FrameEvent, static_cast<size_t>(Counter::Count)> kEvents {{
    Profiler::FrameEvent::GuestWriteFaults,
    Profiler::FrameEvent::GuestReadFaults,
    Profiler::FrameEvent::GuestFaultNanoseconds,
    Profiler::FrameEvent::PageProtectCalls,
    Profiler::FrameEvent::PageProtectPages,
    Profiler::FrameEvent::PageUnprotectCalls,
    Profiler::FrameEvent::PageUnprotectPages,
    Profiler::FrameEvent::PageProtectNanoseconds,
    Profiler::FrameEvent::TrackerLockContended,
    Profiler::FrameEvent::TilerScratchAllocs,
    Profiler::FrameEvent::TilerScratchBytes,
    Profiler::FrameEvent::TilerScratchNanoseconds,
    Profiler::FrameEvent::BufferFromImageSyncs,
    Profiler::FrameEvent::BufferUploadCopies,
    Profiler::FrameEvent::BufferUploadBarriers,
    Profiler::FrameEvent::BufferUploadRenderSplits,
    Profiler::FrameEvent::ImageWritebacks,
    Profiler::FrameEvent::ImageWritebackBytes,
    Profiler::FrameEvent::ImageWritebackPartial,
    Profiler::FrameEvent::ImageWritebackSkips,
    Profiler::FrameEvent::FaultAheadPages,
    Profiler::FrameEvent::HotPagePromotions,
    Profiler::FrameEvent::HotPageDemotions,
    Profiler::FrameEvent::HotPageUploads,
    Profiler::FrameEvent::HotPageUploadsSkipped,
    Profiler::FrameEvent::WrittenUploadLatePages,
}};
// A missing initializer would leave the value-initialized first FrameEvent at the end.
static_assert(kEvents.back() != Profiler::FrameEvent::SubmitBoundaryUnprotected,
              "memory counter events must match HangTrace::MemoryCounter");
} // namespace Detail

// True when any sink collects: callers may skip reading timers otherwise.
[[nodiscard]] inline bool Enabled() {
	return HangTrace::Enabled() || Profiler::AggregateEnabled();
}

inline void Count(Counter counter, uint64_t amount = 1) {
	if (amount == 0) {
		return;
	}
	HangTrace::CountMemory(counter, amount);
	Profiler::CountFrameEvent(Detail::kEvents[static_cast<size_t>(counter)], amount);
}

// Adds the scope's duration to a nanosecond counter; reads no clock when nothing collects.
class ScopedTimer {
public:
	explicit ScopedTimer(Counter counter): m_counter(counter), m_active(Enabled()) {
		if (m_active) {
			m_start = HangTrace::NowNs();
		}
	}
	~ScopedTimer() {
		if (m_active) {
			Count(m_counter, HangTrace::NowNs() - m_start);
		}
	}
	ScopedTimer(const ScopedTimer&)            = delete;
	ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
	Counter  m_counter;
	uint64_t m_start = 0;
	bool     m_active;
};

} // namespace Libs::Graphics::MemoryStats

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_MEMORYSTATS_H_
