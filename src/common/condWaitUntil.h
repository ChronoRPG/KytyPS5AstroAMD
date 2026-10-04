#ifndef EMULATOR_SRC_COMMON_CONDWAITUNTIL_H_
#define EMULATOR_SRC_COMMON_CONDWAITUNTIL_H_

#include "common/threads.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <thread>

namespace Common {

// Waits on `cv` (with `lock` held) until ready() or the deadline. Every poll_us at the latest, and
// after each wake-up that is not ready, dispatch() runs with the lock released (pending guest
// signals). precise (KYTY_PRECISE_COND_WAITS, Senaxx d069e1e99): Windows times condition-variable
// waits in whole milliseconds on the system tick, so a ~5 ms wait woke up to ~1 ms late; precise
// waits time the last 2.5 ms with the high-resolution timer in slices of at most 0.5 ms (a wake-up
// in that stretch is seen within a slice) and a short yield loop. Not precise: one
// condition-variable wait per poll, as before.
template <class Lock, class Ready, class Dispatch>
void CondWaitUntil(Lock& lock, std::condition_variable& cv, const Ready& ready,
                   std::chrono::steady_clock::time_point deadline, uint32_t poll_us, bool precise,
                   Dispatch&& dispatch) {
	constexpr auto coarse_margin = std::chrono::microseconds(2500);
	constexpr auto spin_margin   = std::chrono::microseconds(300);
	constexpr auto fine_slice    = std::chrono::microseconds(500);
	const auto     poll          = std::chrono::steady_clock::duration(std::chrono::microseconds(poll_us));
	while (!ready()) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline) {
			break;
		}
		const auto remaining = deadline - now;
		if (!precise) {
			cv.wait_for(lock, std::min(remaining, poll));
		} else if (remaining > coarse_margin) {
			cv.wait_for(lock, std::min<std::chrono::steady_clock::duration>(remaining - coarse_margin, poll));
		} else {
			lock.unlock();
			if (remaining > spin_margin) {
				const auto slice =
				    std::min<std::chrono::steady_clock::duration>(remaining - spin_margin, fine_slice);
				Thread::SleepMicro(static_cast<uint32_t>(
				    std::chrono::duration_cast<std::chrono::microseconds>(slice).count()));
			} else {
				std::this_thread::yield();
			}
			lock.lock();
			continue;
		}
		if (!ready()) {
			lock.unlock();
			dispatch();
			lock.lock();
		}
	}
}

} // namespace Common

#endif // EMULATOR_SRC_COMMON_CONDWAITUNTIL_H_
