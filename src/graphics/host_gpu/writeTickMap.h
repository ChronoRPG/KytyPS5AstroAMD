#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_WRITETICKMAP_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_WRITETICKMAP_H_

#include "common/assert.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>

namespace Libs::Graphics {

// Non-overlapping guest intervals, each tagged with the scheduler tick of the last recording
// that wrote GPU buffer contents there. A later Assign overwrites the overlapped part only.
// Missing intervals mean "no recorded writer newer than the prune floor": Prune may drop
// entries whose ticks are complete, so readers combine MaxTick with the last prune floor.
class WriteTickMap final {
public:
	void Assign(uint64_t address, uint64_t size, uint64_t tick) {
		const auto end = End(address, size);
		auto       it  = m_entries.lower_bound(address);
		if (it != m_entries.begin() && std::prev(it)->second.end > address) {
			it = std::prev(it);
		}
		// One entry with this tick already covers the range (a binding written again in the same
		// recording): splitting it and coalescing the pieces back would restore exactly it.
		if (it != m_entries.end() && it->first <= address && it->second.end >= end &&
		    it->second.tick == tick) {
			return;
		}
		while (it != m_entries.end() && it->first < end) {
			const auto begin      = it->first;
			const auto entry      = it->second;
			it                    = m_entries.erase(it);
			if (begin < address) {
				m_entries.emplace(begin, Entry {address, entry.tick});
			}
			if (entry.end > end) {
				m_entries.emplace(end, Entry {entry.end, entry.tick});
				break;
			}
		}
		auto inserted = m_entries.emplace(address, Entry {end, tick}).first;
		// Coalesce with equal-tick neighbours so repeated writes of one binding stay one entry.
		if (inserted != m_entries.begin()) {
			auto previous = std::prev(inserted);
			if (previous->second.end == address && previous->second.tick == tick) {
				previous->second.end = end;
				m_entries.erase(inserted);
				inserted = previous;
			}
		}
		if (auto next = std::next(inserted);
		    next != m_entries.end() && next->first == inserted->second.end &&
		    next->second.tick == tick) {
			inserted->second.end = next->second.end;
			m_entries.erase(next);
		}
	}

	// The newest recorded tick overlapping the range; 0 when no entry overlaps it.
	[[nodiscard]] uint64_t MaxTick(uint64_t address, uint64_t size) const {
		const auto end    = End(address, size);
		auto       it     = m_entries.upper_bound(address);
		uint64_t   result = 0;
		if (it != m_entries.begin()) {
			--it;
		}
		for (; it != m_entries.end() && it->first < end; ++it) {
			if (it->second.end > address) {
				result = std::max(result, it->second.tick);
			}
		}
		return result;
	}

	// Drops entries whose writers are complete (tick <= completed_tick).
	void Prune(uint64_t completed_tick) {
		for (auto it = m_entries.begin(); it != m_entries.end();) {
			if (it->second.tick <= completed_tick) {
				it = m_entries.erase(it);
			} else {
				++it;
			}
		}
	}

	void Clear() { m_entries.clear(); }

	[[nodiscard]] size_t Size() const noexcept { return m_entries.size(); }

private:
	struct Entry {
		uint64_t end  = 0;
		uint64_t tick = 0;
	};

	static uint64_t End(uint64_t address, uint64_t size) {
		if (size == 0 || size > UINT64_MAX - address) {
			EXIT("invalid write-tick range\n");
		}
		return address + size;
	}

	std::map<uint64_t, Entry> m_entries;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_WRITETICKMAP_H_
