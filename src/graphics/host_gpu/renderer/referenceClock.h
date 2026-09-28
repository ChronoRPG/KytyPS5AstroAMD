#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_REFERENCECLOCK_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_REFERENCECLOCK_H_

#include <cstdint>
#include <limits>

// The guest GPU reference clock (end-of-pipe timestamps, COPY_DATA clock sources): 100 MHz. Kyty
// derives it from the host TSC (Sync::ReadReferenceClock). Header-only so that the timestamp
// conversion tests need no renderer.
namespace Libs::Graphics::Sync {

constexpr uint64_t GRAPHICS_REFERENCE_CLOCK_FREQUENCY = 100000000;

// `host_ticks` of a `host_frequency` Hz counter as reference-clock ticks. False for a zero
// frequency or when the result does not fit in 64 bits.
[[nodiscard]] inline bool ScaleReferenceClock(uint64_t host_ticks, uint64_t host_frequency,
                                              uint64_t& value) {
	if (host_frequency == 0) {
		return false;
	}

	const auto     whole_seconds = host_ticks / host_frequency;
	const auto     remainder     = host_ticks % host_frequency;
	constexpr auto MAX_VALUE     = std::numeric_limits<uint64_t>::max();
	if (whole_seconds > MAX_VALUE / GRAPHICS_REFERENCE_CLOCK_FREQUENCY ||
	    remainder > MAX_VALUE / GRAPHICS_REFERENCE_CLOCK_FREQUENCY) {
		return false;
	}

	const auto whole_value      = whole_seconds * GRAPHICS_REFERENCE_CLOCK_FREQUENCY;
	const auto fractional_value = (remainder * GRAPHICS_REFERENCE_CLOCK_FREQUENCY) / host_frequency;
	if (whole_value > MAX_VALUE - fractional_value) {
		return false;
	}
	value = whole_value + fractional_value;
	return true;
}

} // namespace Libs::Graphics::Sync

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_REFERENCECLOCK_H_
