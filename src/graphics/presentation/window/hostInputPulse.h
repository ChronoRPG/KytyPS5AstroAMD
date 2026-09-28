#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Libs::Graphics {

// Opt-in test aid: retain short host presses long enough for a guest state poll.
// This deliberately changes input timing and must not be used for latency measurements.
class HostInputPulse {
public:
	void SetMinimum(uint64_t milliseconds) { minimum = milliseconds; }

	template<class Emit> void Key(int key, bool down, uint64_t now, Emit emit) {
		if (minimum == 0) {
			emit(key, down);
			return;
		}
		auto it = std::find_if(keys.begin(), keys.end(), [key](const Held& h) { return h.key == key; });
		if (down) {
			if (it == keys.end()) {
				keys.push_back({key, now + minimum, false});
				emit(key, true);
			} else {
				it->until = now + minimum;
				it->released = false;
			}
		} else if (it == keys.end()) {
			emit(key, false);
		} else if (now >= it->until) {
			emit(key, false);
			keys.erase(it);
		} else {
			it->released = true;
		}
	}

	template<class Emit> int Poll(uint64_t now, Emit emit) {
		int wait = -1;
		for (auto it = keys.begin(); it != keys.end();) {
			if (!it->released) { ++it; continue; }
			if (now >= it->until) {
				emit(it->key, false);
				it = keys.erase(it);
			} else {
				const int remaining = static_cast<int>(it->until - now);
				wait = wait < 0 ? remaining : std::min(wait, remaining);
				++it;
			}
		}
		return wait;
	}

	template<class Emit> void ReleaseAll(Emit emit) {
		for (const auto& h: keys) { emit(h.key, false); }
		keys.clear();
	}

private:
	struct Held { int key; uint64_t until; bool released; };
	uint64_t minimum = 0;
	std::vector<Held> keys;
};

} // namespace Libs::Graphics
