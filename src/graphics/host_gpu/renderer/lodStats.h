#ifndef KYTY_RENDERER_LODSTATS_H_
#define KYTY_RENDERER_LODSTATS_H_

#include "graphics/host_gpu/renderer/lodStatsReport.h"
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
// mip level sampled and a count in a device-local buffer. A GET_LOD_STATS packet copies the
// counters into a private host-visible slot in command order and resets them; the report layout
// is in lodStatsReport.h.
//
// Astro Bot keeps 16 buckets of 256 counters, each with a ring of 16 reports, and reports one
// bucket per frame: DMA_DATA clears the slot header, GET_LOD_STATS writes the report,
// RELEASE_MEM flushes, then DMA_DATA publishes the slot index. On hardware the index therefore
// names a complete report. Kyty performs that index write when the command is recorded, before
// the GPU has produced the report. The guest's lookup (eboot+0x7022e40) skips a current slot whose
// header is still zero and keeps the statistics it parsed from the previous slot, which is what
// hardware would still show it, so writing the report at GPU completion is exact
// (KYTY_LOD_REPORT_PUBLISH=completion, the default). The record-time modes instead write other
// buckets' counters into the slot until the copy completes.
//
// Host publication keeps the report pages CPU-owned, like OcclusionCounter.
class LodStatsCounter {
public:
	static constexpr uint32_t Counters   = LodStatsReport::Counters;
	static constexpr uint32_t Entries    = LodStatsReport::Entries;
	static constexpr uint32_t ReportSize = LodStatsReport::ReportSize;
	using Publish                        = LodStatsReport::Publish;

	explicit LodStatsCounter(RenderContext& context);
	~LodStatsCounter();

	[[nodiscard]] static bool Enabled();
	// KYTY_LOD_REPORT_PUBLISH (read once): completion (default), rewrite or record.
	[[nodiscard]] static Publish PublishMode();
	// True unless KYTY_LOD_REPORT_PUBLISH=record (or the U33 switch
	// KYTY_LOD_REPORT_COMPLETION_WRITE=0): a completed copy writes the guest slot.
	[[nodiscard]] static bool CompletionWriteEnabled();
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
	// Publish::Rewrite / Publish::Record: the newest report packed from completed GPU counters,
	// written when the next GET_LOD_STATS is recorded (Kyty writes EOP labels at record time).
	std::mutex                             m_latest_mutex;
	std::array<uint8_t, ReportSize>        m_latest {};
	bool                                   m_has_latest = false;
	std::array<uint32_t, Counters>         m_previous_finest = [] {
		std::array<uint32_t, Counters> values {};
		values.fill(LodStatsReport::Unsampled);
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
