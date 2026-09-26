#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_CACHE_IMAGECACHEGCPOLICY_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_CACHE_IMAGECACHEGCPOLICY_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace Libs::Graphics {

// Pressure, rather than the number of guest submissions, determines image lifetime.
// Keep hysteresis and retry decisions independent of Vulkan for deterministic tests.
class ImageCacheGcPolicy {
public:
	static constexpr uint64_t MiB = 1024 * 1024;

	struct Plan {
		size_t   candidates = 0;
		size_t   deletions  = 0;
		uint64_t bytes      = 0;
		bool     critical   = false;
	};

	explicit ImageCacheGcPolicy(uint64_t budget = 0) { SetBudget(budget); }

	void SetBudget(uint64_t budget) {
		// A missing heap budget must still result in finite, nonzero watermarks.
		m_budget = std::max<uint64_t>(budget == 0 ? 1024 * MiB : budget, 1024);
		m_low = Fraction(m_budget, 7, 10);
		m_high = Fraction(m_budget, 8, 10);
		m_critical = Fraction(m_budget, 9, 10);
	}

	[[nodiscard]] Plan Begin(uint64_t usage, uint64_t epoch) {
		if (usage <= m_low) {
			m_collecting = false;
			m_next_epoch = 0;
		}
		if (!m_collecting && usage >= m_high) {
			m_collecting = true;
		}
		if (!m_collecting) {
			return {};
		}
		const bool critical = usage >= m_critical;
		const bool grew = usage > m_last_usage &&
		                  usage - m_last_usage >= std::max<uint64_t>(64 * MiB, m_budget / 32);
		if (epoch < m_next_epoch && !grew && !(critical && !m_last_critical)) {
			return {};
		}
		m_last_usage = usage;
		m_last_critical = critical;
		return critical ? Plan {8192, 128, 1024 * MiB, true}
		                : Plan {2048, 32, 256 * MiB, false};
	}

	void Complete(uint64_t epoch, uint64_t remaining, bool reclaimed) {
		if (remaining <= m_low) {
			m_collecting = false;
		}
		// An irreducible buffer/driver allocation or nondownloadable image must not
		// provoke another cache scan and GPU readback after every tiny submission.
		const uint64_t delay = reclaimed ? (m_last_critical ? 1 : 8)
		                                 : (m_last_critical ? 8 : 64);
		m_next_epoch = epoch > UINT64_MAX - delay ? UINT64_MAX : epoch + delay;
	}

	[[nodiscard]] uint64_t Low() const noexcept { return m_low; }
	[[nodiscard]] uint64_t High() const noexcept { return m_high; }
	[[nodiscard]] uint64_t Critical() const noexcept { return m_critical; }

private:
	static constexpr uint64_t Fraction(uint64_t value, uint64_t numerator, uint64_t denominator) {
		return value / denominator * numerator + value % denominator * numerator / denominator;
	}

	uint64_t m_budget        = 0;
	uint64_t m_low           = 0;
	uint64_t m_high          = 0;
	uint64_t m_critical      = 0;
	uint64_t m_next_epoch    = 0;
	uint64_t m_last_usage    = 0;
	bool     m_collecting    = false;
	bool     m_last_critical = false;
};

} // namespace Libs::Graphics

#endif
