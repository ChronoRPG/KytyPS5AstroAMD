#include "graphics/presentation/window/hostInputPulse.h"
#include <cstdio>
#include <utility>
#include <vector>

int main() {
	using Event = std::pair<int, bool>;
	Libs::Graphics::HostInputPulse pulse;
	std::vector<Event> events;
	const auto emit = [&](int key, bool down) { events.emplace_back(key, down); };
	int failed = 0;
	const auto check = [&](bool ok, const char* label) {
		if (!ok) { std::printf("FAIL: %s\n", label); ++failed; }
	};
	pulse.Key(1, true, 0, emit); pulse.Key(1, false, 0, emit);
	check(events == std::vector<Event>{{1, true}, {1, false}}, "default timing unchanged");
	events.clear(); pulse.SetMinimum(250);
	pulse.Key(1, true, 100, emit); pulse.Key(1, false, 101, emit);
	check(pulse.Poll(349, emit) == 1 && events.size() == 1, "short press retained");
	check(pulse.Poll(350, emit) == -1 && events.back() == Event{1, false}, "release at deadline");
	events.clear(); pulse.Key(1, true, 400, emit);
	check(pulse.Poll(900, emit) == -1 && events.size() == 1, "physical hold not shortened");
	pulse.Key(1, false, 901, emit);
	check(events.size() == 2 && events.back() == Event{1, false}, "long press releases immediately");
	events.clear(); pulse.Key(1, true, 1000, emit); pulse.Key(1, false, 1001, emit);
	pulse.Key(1, true, 1100, emit); pulse.Key(1, false, 1101, emit);
	check(pulse.Poll(1250, emit) == 100 && events.size() == 1, "repress cancels stale release");
	pulse.Key(2, true, 1200, emit); pulse.Key(2, false, 1201, emit);
	check(pulse.Poll(1350, emit) == 100 && events.back() == Event{1, false}, "independent keys");
	pulse.ReleaseAll(emit);
	check(events.back() == Event{2, false} && pulse.Poll(1500, emit) == -1, "focus loss releases keys");
	events.clear(); pulse.Key(3, false, 1600, emit);
	check(events == std::vector<Event>{{3, false}}, "unmatched release passes through");
	std::printf("Host input pulse tests: %s\n", failed == 0 ? "PASS" : "FAIL");
	return failed ? 1 : 0;
}
