#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_EOPTIMESTAMPCLOCK_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_EOPTIMESTAMPCLOCK_H_

#include "graphics/host_gpu/renderer/referenceClock.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>

// The parts of KYTY_EOP_TIMESTAMPS (eopTimestamps.h) that need no device: the conversion of a
// device timestamp to the guest reference clock, and the life cycle of the query slots.
namespace Libs::Graphics::EopTimestamps {

// Maps device timestamps to the guest reference clock, the domain of Sync::ReadReferenceClock
// (the host TSC scaled to 100 MHz). It is built from two samples:
// - a calibrated (device, host QPC) pair (VK_KHR/EXT_calibrated_timestamps);
// - a (QPC, TSC) pair read back to back on the CPU.
// Each conversion adds deltas to those samples, so drift stays small while the map is refreshed
// often (EopTimestampRing recalibrates every 100 ms).
struct ClockMap {
	uint64_t device_raw    = 0; // calibrated device timestamp
	uint64_t device_qpc    = 0; // host QPC at device_raw
	uint64_t qpc_frequency = 0;
	double   period_ns     = 0.0; // device nanoseconds per tick
	uint32_t valid_bits    = 64;
	uint64_t pair_qpc      = 0; // QPC read next to pair_tsc
	uint64_t pair_tsc      = 0;
	uint64_t tsc_frequency = 0;

	[[nodiscard]] bool Valid() const noexcept {
		return qpc_frequency != 0 && tsc_frequency != 0 && period_ns > 0.0 &&
		       std::isfinite(period_ns) && valid_bits != 0 && valid_bits <= 64;
	}
};

// a - b of two device timestamps modulo their valid bits, sign-extended: counters narrower than
// 64 bits unwrap correctly for spans below half their range.
[[nodiscard]] inline int64_t DeviceDelta(uint64_t a, uint64_t b, uint32_t valid_bits) noexcept {
	const uint64_t mask =
	    valid_bits >= 64 ? ~uint64_t {0} : (uint64_t {1} << valid_bits) - 1u;
	uint64_t delta = (a - b) & mask;
	if (valid_bits < 64 && ((delta >> (valid_bits - 1u)) & 1u) != 0) {
		delta |= ~mask;
	}
	return static_cast<int64_t>(delta);
}

// The guest reference-clock value of device timestamp `device_raw`. False when the map is invalid
// or the result falls outside the clock's range.
[[nodiscard]] inline bool ToReference(const ClockMap& map, uint64_t device_raw,
                                      uint64_t& reference) noexcept {
	if (!map.Valid()) {
		return false;
	}
	// Host QPC ticks of device_raw relative to pair_qpc; only deltas go through doubles.
	const double device_ns =
	    static_cast<double>(DeviceDelta(device_raw, map.device_raw, map.valid_bits)) *
	    map.period_ns;
	const double qpc_offset =
	    static_cast<double>(static_cast<int64_t>(map.device_qpc - map.pair_qpc)) +
	    device_ns * static_cast<double>(map.qpc_frequency) / 1e9;
	const double tsc_offset = qpc_offset * static_cast<double>(map.tsc_frequency) /
	                          static_cast<double>(map.qpc_frequency);
	if (!std::isfinite(tsc_offset) || std::fabs(tsc_offset) > 9.0e18) {
		return false;
	}
	const auto offset = static_cast<int64_t>(std::llround(tsc_offset));
	if (offset < 0 && static_cast<uint64_t>(-offset) > map.pair_tsc) {
		return false;
	}
	const uint64_t tsc = map.pair_tsc + static_cast<uint64_t>(offset);
	return Sync::ScaleReferenceClock(tsc, map.tsc_frequency, reference);
}

// The timestamp query slots, in ring order.
// - Life cycle: Free --Allocate--> Pending --Consume--> Done --Reset--> Free.
// - Allocate and Reset run on the recording thread only. Reset is called at a command-buffer begin,
//   outside rendering (vkCmdResetQueryPool is not allowed inside it), with the runs to reset.
// - Consume may run on any thread, once the slot's result has been read.
// - Allocation and resets both advance in ring order. A slot is reset only after its previous use
//   was consumed, so a reset never races a pending write or read. A slow consumer only makes
//   later allocations fail (the caller then keeps the record-time value); nothing waits.
class QueryRing {
public:
	static constexpr uint32_t NoSlot = UINT32_MAX;

	explicit QueryRing(uint32_t capacity)
	    : m_capacity(capacity == 0 ? 1u : capacity),
	      m_state(std::make_unique<std::atomic<uint8_t>[]>(m_capacity)) {
		// Every slot starts unused and must be reset once before its first write.
		for (uint32_t i = 0; i < m_capacity; i++) {
			m_state[i].store(Done, std::memory_order_relaxed);
		}
	}

	[[nodiscard]] uint32_t Capacity() const noexcept { return m_capacity; }
	// Slots reset and not yet allocated (recording thread).
	[[nodiscard]] uint64_t Available() const noexcept { return m_reset_pos - m_alloc_pos; }

	// Recording thread: the next free slot, or NoSlot.
	[[nodiscard]] uint32_t Allocate() noexcept {
		if (m_alloc_pos == m_reset_pos) {
			return NoSlot;
		}
		const auto slot = static_cast<uint32_t>(m_alloc_pos % m_capacity);
		m_state[slot].store(Pending, std::memory_order_relaxed);
		m_alloc_pos++;
		return slot;
	}

	// Any thread, after the slot's result was read (or abandoned).
	void Consume(uint32_t slot) noexcept { m_state[slot].store(Done, std::memory_order_release); }

	// Recording thread, outside rendering: resets every consumed slot next in ring order, calling
	// reset(first, count) once per contiguous run. Returns the number of slots reset.
	template <typename Reset>
	uint32_t ResetConsumed(Reset&& reset) {
		uint32_t total = 0;
		uint32_t first = 0;
		uint32_t count = 0;
		// The slot of position p was last used by p - capacity: never reset one lap ahead of the
		// allocation, and only after that use was consumed.
		while (m_reset_pos < m_alloc_pos + m_capacity) {
			const auto slot = static_cast<uint32_t>(m_reset_pos % m_capacity);
			if (m_state[slot].load(std::memory_order_acquire) != Done) {
				break;
			}
			if (count != 0 && slot != first + count) {
				reset(first, count);
				count = 0;
			}
			if (count == 0) {
				first = slot;
			}
			m_state[slot].store(Free, std::memory_order_relaxed);
			count++;
			total++;
			m_reset_pos++;
		}
		if (count != 0) {
			reset(first, count);
		}
		return total;
	}

private:
	enum : uint8_t { Free = 0, Pending = 1, Done = 2 };

	uint32_t                                 m_capacity;
	std::unique_ptr<std::atomic<uint8_t>[]> m_state;
	uint64_t                                 m_alloc_pos = 0; // positions allocated
	uint64_t                                 m_reset_pos = 0; // positions reset (usable)
};

} // namespace Libs::Graphics::EopTimestamps

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_EOPTIMESTAMPCLOCK_H_
