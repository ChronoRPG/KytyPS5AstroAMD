#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_WORKERGATE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_WORKERGATE_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>
#endif

// Draw-prep worker parking (KYTY_DRAW_PREP_HOT). At the Sky Garden rate the window needs about
// 1.2 busy workers, yet all six spun 200 us after every slot: about 3.7 cores of spin per flip.
//
// - Hot workers (the first `hot`) keep that behaviour: spin for a while after their last slot,
//   then park; the producer wakes parked hot workers on its next publish. While they keep up, a
//   publish costs the producer one relaxed-ordering load (no syscall).
// - Cold workers park as soon as nothing is claimable, and wake only when the unclaimed backlog
//   reaches `wake_backlog`. The wake is requested by whichever thread sees that backlog: a worker
//   right after its claim, or the producer on every `wake_backlog`-th publish. At most one cold
//   wake is in flight, so a burst costs one WakeByAddress call, not one per draw.
//
// No wakeup is lost: a sleeper announces itself (sleepers++, seq_cst) and then re-checks its
// predicate, and a waker changes the signal word after its own seq_cst update of what the
// predicate reads. Either the sleeper sees the new state, or the waker sees the sleeper.
namespace Libs::Graphics::DrawPrep {

class WorkerGate {
public:
	WorkerGate(uint32_t workers, uint32_t hot, uint32_t wake_backlog) noexcept
	    : m_hot(hot == 0 || hot > workers ? workers : hot), m_has_cold(m_hot < workers),
	      m_wake_backlog(wake_backlog == 0 ? 1u : wake_backlog) {}

	[[nodiscard]] bool     Hot(uint32_t index) const noexcept { return index < m_hot; }
	[[nodiscard]] uint32_t HotCount() const noexcept { return m_hot; }
	[[nodiscard]] bool     HasCold() const noexcept { return m_has_cold; }
	[[nodiscard]] uint32_t WakeBacklog() const noexcept { return m_wake_backlog; }

	// Producer, after each publish (the publish itself is a seq_cst store).
	// `backlog` is evaluated only on every WakeBacklog()-th publish, so the producer rarely reads
	// the workers' contended claim counter. Returns true when it requested a cold wake.
	template <typename Backlog>
	bool OnPublish(Backlog&& backlog) noexcept {
		if (m_hot_sleepers.load(std::memory_order_seq_cst) != 0) {
			m_hot_signal.fetch_add(1, std::memory_order_seq_cst);
			m_hot_signal.notify_all();
		}
		if (!m_has_cold || ++m_publishes < m_wake_backlog) {
			return false;
		}
		m_publishes = 0;
		return MaybeWakeCold(backlog());
	}

	// Any thread that has just measured the unclaimed backlog. Returns true when it woke one.
	bool MaybeWakeCold(uint64_t backlog) noexcept {
		if (backlog < m_wake_backlog || m_cold_sleepers.load(std::memory_order_seq_cst) == 0 ||
		    m_cold_wake_pending.load(std::memory_order_relaxed) ||
		    m_cold_wake_pending.exchange(true, std::memory_order_acq_rel)) {
			return false;
		}
		m_cold_signal.fetch_add(1, std::memory_order_seq_cst);
		m_cold_signal.notify_one();
		m_cold_wakes.fetch_add(1, std::memory_order_relaxed);
		return true;
	}

	// Hot worker with nothing claimable after its spin: sleeps until the next publish.
	template <typename Claimable>
	void ParkHot(Claimable&& claimable, const std::atomic<bool>& stop) noexcept {
		const auto observed = m_hot_signal.load(std::memory_order_seq_cst);
		m_hot_sleepers.fetch_add(1, std::memory_order_seq_cst);
		if (!claimable() && !stop.load(std::memory_order_seq_cst)) {
			m_hot_signal.wait(observed, std::memory_order_seq_cst);
		}
		m_hot_sleepers.fetch_sub(1, std::memory_order_seq_cst);
	}

	// Cold worker with nothing claimable: sleeps until the backlog reaches WakeBacklog().
	template <typename BacklogHigh>
	void ParkCold(BacklogHigh&& backlog_high, const std::atomic<bool>& stop) noexcept {
		const auto observed = m_cold_signal.load(std::memory_order_seq_cst);
		m_cold_sleepers.fetch_add(1, std::memory_order_seq_cst);
		if (!backlog_high() && !stop.load(std::memory_order_seq_cst)) {
			m_cold_signal.wait(observed, std::memory_order_seq_cst);
		}
		m_cold_sleepers.fetch_sub(1, std::memory_order_seq_cst);
		// Whoever leaves the park (woken or not) re-arms the next wake. A pending flag is only set
		// while a sleeper is announced, and that sleeper always gets here.
		m_cold_wake_pending.store(false, std::memory_order_release);
	}

	// Shutdown: after `stop` is set.
	void WakeAll() noexcept {
		m_hot_signal.fetch_add(1, std::memory_order_seq_cst);
		m_hot_signal.notify_all();
		m_cold_signal.fetch_add(1, std::memory_order_seq_cst);
		m_cold_signal.notify_all();
	}

	[[nodiscard]] uint64_t ColdWakes() const noexcept {
		return m_cold_wakes.load(std::memory_order_relaxed);
	}
	[[nodiscard]] uint32_t ColdSleepers() const noexcept {
		return m_cold_sleepers.load(std::memory_order_relaxed);
	}
	[[nodiscard]] uint32_t HotSleepers() const noexcept {
		return m_hot_sleepers.load(std::memory_order_relaxed);
	}

private:
	const uint32_t m_hot;
	const bool     m_has_cold;
	const uint32_t m_wake_backlog;
	uint32_t       m_publishes = 0; // producer only

	alignas(64) std::atomic<uint64_t> m_hot_signal {0};
	std::atomic<uint32_t>             m_hot_sleepers {0};
	alignas(64) std::atomic<uint64_t> m_cold_signal {0};
	std::atomic<uint32_t>             m_cold_sleepers {0};
	std::atomic<bool>                 m_cold_wake_pending {false};
	std::atomic<uint64_t>             m_cold_wakes {0};
};

inline uint64_t WorkerNowNs() noexcept {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
	                                 std::chrono::steady_clock::now().time_since_epoch())
	                                 .count());
}

inline void WorkerRelax() noexcept {
#if defined(_M_X64) || defined(__x86_64__)
	_mm_pause();
#else
	std::this_thread::yield();
#endif
}

// One preparation worker (Engine::Workers::Run; the unit tests run the same loop). `prepare` gets
// each claimed slot and its sequence number and must complete it; `on_cold_wake` is called after
// this worker woke a cold one. Workers look at the clock only every 256 idle spins.
template <typename WindowT, typename Prepare, typename OnColdWake>
void RunPreparationWorker(WorkerGate& gate, WindowT& window, uint32_t index, uint64_t hot_spin_ns,
                          uint64_t cold_spin_ns, const std::atomic<bool>& stop, Prepare&& prepare,
                          OnColdWake&& on_cold_wake) {
	const bool hot        = gate.Hot(index);
	const auto spin_limit = hot ? hot_spin_ns : cold_spin_ns;
	auto       idle_start = WorkerNowNs();
	uint32_t   spins      = 0;
	while (!stop.load(std::memory_order_acquire)) {
		uint64_t seq = 0;
		if (auto* slot = window.TryClaim(seq); slot != nullptr) {
			if (gate.HasCold() && gate.MaybeWakeCold(window.Unclaimed())) {
				on_cold_wake();
			}
			prepare(*slot, seq);
			idle_start = WorkerNowNs();
			spins      = 0;
			continue;
		}
		WorkerRelax();
		if ((++spins & 255u) != 0u || WorkerNowNs() - idle_start < spin_limit) {
			continue;
		}
		if (hot) {
			gate.ParkHot([&window] { return window.HasClaimable(); }, stop);
		} else {
			gate.ParkCold([&window, &gate] { return window.Unclaimed() >= gate.WakeBacklog(); },
			              stop);
		}
		idle_start = WorkerNowNs();
	}
}

} // namespace Libs::Graphics::DrawPrep

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_DRAWPREP_WORKERGATE_H_
