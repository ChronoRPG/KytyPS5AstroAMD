#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_EAGERREADBACKPAGES_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_EAGERREADBACKPAGES_H_

#include "graphics/host_gpu/regionDefinitions.h"

#include <cstddef>
#include <cstdint>
#include <map>

namespace Libs::Graphics {

// Read-hot tracker pages of eager readback publication (BufferCache, KYTY_READBACK_EAGER).
//
// A page becomes hot when a CPU read of GPU-owned bytes on it needed a readback: a guest read
// fault, or a GPU-thread read such as indirect draw arguments. While a page is hot, a recording
// that writes it makes it a candidate; a later submission records a copy of its dirty bytes that
// is published to the backing when that submission completes, so a read after completion finds
// the page clean instead of faulting. Only bookkeeping lives here: the caller decides whether a
// candidate can be issued. Not thread-safe (GPU thread).
class EagerReadbackPages final {
public:
	struct Limits {
		// Hot pages kept; inserting one more evicts the least recently read.
		uint32_t capacity = 64;
		// A page no reader needed for more than this many frames stops being hot. Eager copies
		// keep a page's reads from faulting, so this is also how long a page stays hot after its
		// last readback: a page the GPU stops writing costs nothing while it waits to expire.
		uint32_t idle_frames = 600;
		// Copies issued per page and frame; later candidates wait for the next frame.
		uint32_t frame_budget = 4;
	};
	enum class IssueResult : uint8_t {
		Issued, // copy recorded: the candidate is served
		Retry,  // not possible now (e.g. the writer is in the current recording): keep it
		Drop,   // nothing to copy (no dirty bytes, no owner): forget the candidate
	};

	EagerReadbackPages() = default;
	explicit EagerReadbackPages(Limits limits): m_limits(limits) {}

	// A reader needed a readback of `page` (tracker-page aligned) in `frame`. cp: the reader was
	// the GPU (command processor) thread.
	void NoteRead(uint64_t page, uint32_t frame, bool cp) {
		auto it = m_pages.find(page);
		if (it == m_pages.end()) {
			if (m_limits.capacity == 0) {
				return;
			}
			if (m_pages.size() >= m_limits.capacity) {
				Evict();
			}
			it = m_pages.emplace(page, Page {}).first;
			UpdateBounds();
		}
		it->second.last_read = frame;
		it->second.cp        = it->second.cp || cp;
	}

	// GPU contents of [address, address + size) are written: hot pages in the range become
	// candidates. Returns true when one of them was read back by the GPU thread. Called for every
	// writable binding, so a range outside [first hot page, last hot page] costs two compares.
	bool NoteWrite(uint64_t address, uint64_t size) {
		if (m_pages.empty() || size == 0) {
			return false;
		}
		const uint64_t end = address + size;
		if (end <= m_low || address >= m_high) {
			return false;
		}
		const uint64_t first = address & ~(TRACKER_PAGE_SIZE - 1);
		bool           cp    = false;
		for (auto it = m_pages.lower_bound(first); it != m_pages.end() && it->first < end; ++it) {
			if (!it->second.candidate) {
				it->second.candidate = true;
				m_candidates++;
			}
			cp = cp || it->second.cp;
		}
		return cp;
	}

	// Offers each candidate to issue(page) -> IssueResult unless its budget for `frame` is spent.
	// Issued and Drop clear the candidate; Retry (and a spent budget) keep it.
	template <typename Issue>
	void IssueCandidates(uint32_t frame, Issue&& issue) {
		if (m_candidates == 0) {
			return;
		}
		for (auto& [page, entry]: m_pages) {
			if (!entry.candidate) {
				continue;
			}
			if (entry.budget_frame != frame) {
				entry.budget_frame = frame;
				entry.issued       = 0;
			}
			if (entry.issued >= m_limits.frame_budget) {
				continue;
			}
			switch (issue(page)) {
				case IssueResult::Issued:
					entry.issued++;
					entry.candidate = false;
					m_candidates--;
					break;
				case IssueResult::Drop:
					entry.candidate = false;
					m_candidates--;
					break;
				case IssueResult::Retry: break;
			}
		}
	}

	// Forgets pages no reader needed for more than idle_frames (checks once per frame).
	void Sweep(uint32_t frame) {
		if (frame == m_swept_frame) {
			return;
		}
		m_swept_frame = frame;
		bool erased   = false;
		for (auto it = m_pages.begin(); it != m_pages.end();) {
			if (static_cast<uint32_t>(frame - it->second.last_read) > m_limits.idle_frames) {
				it     = Erase(it);
				erased = true;
			} else {
				++it;
			}
		}
		if (erased) {
			UpdateBounds();
		}
	}

	[[nodiscard]] bool     Empty() const noexcept { return m_pages.empty(); }
	[[nodiscard]] size_t   Size() const noexcept { return m_pages.size(); }
	[[nodiscard]] uint32_t Candidates() const noexcept { return m_candidates; }
	[[nodiscard]] bool     IsHot(uint64_t page) const { return m_pages.contains(page); }
	[[nodiscard]] bool     IsCandidate(uint64_t page) const {
		const auto it = m_pages.find(page);
		return it != m_pages.end() && it->second.candidate;
	}

private:
	struct Page {
		uint32_t last_read    = 0;
		uint32_t budget_frame = 0;
		uint32_t issued       = 0;
		bool     cp           = false;
		bool     candidate    = false;
	};
	using Map = std::map<uint64_t, Page>;

	Map::iterator Erase(Map::iterator it) {
		if (it->second.candidate) {
			m_candidates--;
		}
		return m_pages.erase(it);
	}

	void UpdateBounds() {
		m_low  = m_pages.empty() ? 0 : m_pages.begin()->first;
		m_high = m_pages.empty() ? 0 : m_pages.rbegin()->first + TRACKER_PAGE_SIZE;
	}

	void Evict() {
		auto oldest = m_pages.begin();
		for (auto it = m_pages.begin(); it != m_pages.end(); ++it) {
			// Wrap-safe "read longer ago": compare ages relative to the newest possible frame.
			if (static_cast<int32_t>(it->second.last_read - oldest->second.last_read) < 0) {
				oldest = it;
			}
		}
		if (oldest != m_pages.end()) {
			Erase(oldest);
		}
	}

	Map      m_pages;
	// [first hot page, end of the last hot page); empty when no page is hot.
	uint64_t m_low         = 0;
	uint64_t m_high        = 0;
	uint32_t m_candidates  = 0;
	uint32_t m_swept_frame = 0;
	Limits   m_limits {};
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_EAGERREADBACKPAGES_H_
