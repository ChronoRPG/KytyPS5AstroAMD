#ifndef KYTY_RENDERER_LODSTATS_H_
#define KYTY_RENDERER_LODSTATS_H_

#include "graphics/host_gpu/vulkanCommon.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

namespace Libs::Graphics {
class Buffer;
class RenderContext;

// GPU mip statistics for GET_LOD_STATS (KYTY_LOD_STATS_MODE=gpu, the default).
//
// Instrumented pixel shaders record, per T# mip-statistics counter (MipStatsCntId), the finest
// mip level sampled and a sample count in a device-local buffer. A GET_LOD_STATS packet copies
// the counters into a private host-visible slot in command order and resets them; a completion
// callback packs them into the newest report, which the next GET_LOD_STATS writes into guest
// memory when it is recorded (matching Kyty's record-time EOP labels), in the layout Astro
// Bot's streamer parses
// (eboot+0x479d40 / +0x7021175): a 64-byte header whose first dword marks the report valid,
// then 256 64-bit entries holding the sample count in bits 0..23 and the finest mip in bits
// 56..59 (0xF when the counter was not sampled). Host publication keeps the report pages
// CPU-owned, like OcclusionCounter.
class LodStatsCounter {
public:
	static constexpr uint32_t Counters   = 256;
	static constexpr uint32_t Entries    = Counters + 1; // last entry absorbs images without id
	static constexpr uint32_t ReportSize = 64 + Counters * 8;

	explicit LodStatsCounter(RenderContext& context);
	~LodStatsCounter();

	[[nodiscard]] static bool Enabled();
	// The device-local counter buffer bound to instrumented shaders.
	[[nodiscard]] Buffer& CounterBuffer();
	// Records a report at the current command position. Must be outside rendering.
	void Report(uint64_t destination, uint32_t size, uint32_t control);

private:
	static constexpr uint32_t PublishSlots    = 32;
	static constexpr uint64_t PublishSlotSize = 4096;

	RenderContext&                         m_context;
	std::unique_ptr<Buffer>                m_counters;
	std::unique_ptr<Buffer>                m_publish;
	std::array<uint64_t, PublishSlots>     m_slot_ticks {};
	uint64_t                               m_issued      = 0;
	bool                                   m_initialized = false;
	// Newest report packed from completed GPU counters. Kyty writes EOP labels when a command is
	// recorded, so the guest may read a report as soon as GET_LOD_STATS is recorded; the report
	// written there carries this snapshot (one or two frames old), never an unready header.
	std::mutex                             m_latest_mutex;
	std::array<uint8_t, ReportSize>        m_latest {};
	bool                                   m_has_latest = false;
	std::array<uint32_t, Counters>         m_previous_finest = [] {
		std::array<uint32_t, Counters> values {};
		values.fill(0xffffffffu);
		return values;
	}();
	std::array<uint32_t, Counters>         m_previous_count {};
	// Hang-trace diagnostics (lodreports.csv): summary of m_latest and completed copies.
	uint32_t                               m_latest_sampled     = 0;
	uint64_t                               m_latest_samples     = 0;
	double                                 m_latest_mean_finest = 0.0;
	std::atomic<uint64_t>                  m_completed {0};
};

} // namespace Libs::Graphics

#endif // KYTY_RENDERER_LODSTATS_H_
