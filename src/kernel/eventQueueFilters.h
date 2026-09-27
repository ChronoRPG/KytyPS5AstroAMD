#ifndef EMULATOR_SRC_KERNEL_EVENTQUEUEFILTERS_H_
#define EMULATOR_SRC_KERNEL_EVENTQUEUEFILTERS_H_

#include "kernel/eventQueue.h"

#include <cstdint>
#include <limits>

// Next-state functions of the emulated kernel event filters. Each trigger computes the event's
// state after one more occurrence from its current state; KernelEqueueApplyTrigger then makes that
// state pending, or merges it into the already pending one. A kqueue knote is updated the same
// way: an event is reported once however often it fired, and its counters say how often.
namespace Libs::LibKernel::EventQueue {

// EVFILT_GRAPHICS end-of-pipe interrupt (sceAgcDriverAddEqEvent): fflags counts the interrupts since
// the event was last delivered (the filter's reset clears it), and data is the newest interrupt's
// context id (sceAgcDriverGetEqContextId).
[[nodiscard]] inline KernelEvent GraphicsInterruptNextState(const KernelEvent& current,
                                                            uint64_t           context_id) {
	auto next = current;
	if (next.fflags != std::numeric_limits<uint32_t>::max()) {
		next.fflags++;
	}
	next.data = static_cast<intptr_t>(context_id);
	return next;
}

// EVFILT_VIDEO_OUT event data: bits 0..11 hold the low TSC bits of the newest occurrence, bits
// 12..15 count occurrences since the last delivery (saturating at 15; sceVideoOutGetEventCount),
// and bits 16..63 hold the newest occurrence's payload, the flip argument or vblank count
// (sceVideoOutGetEventData).
[[nodiscard]] inline intptr_t VideoOutEventData(intptr_t current_data, uint64_t payload,
                                                uint64_t tsc) {
	const auto old_data = static_cast<uint64_t>(current_data);
	uint64_t   counter  = (old_data >> 12u) & 0xfu;
	if (counter != 0xfu) {
		counter++;
	}
	return static_cast<intptr_t>((tsc & 0xfffu) | (counter << 12u) |
	                             ((payload & 0x0000ffffffffffffULL) << 16u));
}

// EVFILT_VIDEO_OUT (sceVideoOutAdd*Event): fflags counts like the data counter, saturating at 15.
[[nodiscard]] inline KernelEvent VideoOutNextState(const KernelEvent& current, uint64_t payload,
                                                   uint64_t tsc) {
	auto next   = current;
	next.fflags = next.fflags < 0xfu ? next.fflags + 1u : next.fflags;
	next.data   = VideoOutEventData(next.data, payload, tsc);
	return next;
}

// AMPR/APR kernel-event records (EVFILT_USER, EV_CLEAR): data is the newest record's data.
[[nodiscard]] inline KernelEvent AmprNextState(const KernelEvent& current, uint64_t data) {
	auto next = current;
	next.data = static_cast<intptr_t>(data);
	return next;
}

} // namespace Libs::LibKernel::EventQueue

#endif /* EMULATOR_SRC_KERNEL_EVENTQUEUEFILTERS_H_ */
