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
// and the counting rule are in lodStatsReport.h.
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
	// KYTY_LOD_STATS_COUNT (read once): clamp (default) or samples.
	[[nodiscard]] static bool CountClamped();
	// KYTY_LOD_STATS_PLAIN_VARIANT (read once). An instrumented pixel shader records feedback only
	// for images whose T# has a counter (the per-draw field, LodStatsReport::ImageField); a draw
	// in which no image has one records nothing. Its storage-buffer atomics still make the driver
	// run the depth/stencil tests after the shader whenever the shader can discard (no early
	// rejection of occluded fragments), which a U49 Sky Garden replay measured at 0.8-1.4 ms per
	// frame, all in the late-Z shaders.
	//   1 (default) such draws use the same program compiled without the feedback (plain);
	//   verify      they keep the instrumented program, bound to a canary counter buffer that
	//               GET_LOD_STATS checks is still in its reset state (any change: a draw classified
	//               as counter-free did record, LodStatsCanary* frame events and a log line);
	//   0           every draw uses the instrumented program (U26..U49).
	enum class Plain : uint8_t { Off, On, Verify };
	[[nodiscard]] static Plain PlainVariant();
	// Verify mode: the counter buffer bound to draws classified as counter-free. Valid only after
	// the first report (CanaryReady()); before that such draws keep the real counters.
	[[nodiscard]] bool    CanaryReady() const noexcept { return m_canary_ready; }
	[[nodiscard]] Buffer& CanaryBuffer();
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
	// KYTY_LOD_STATS_PLAIN_VARIANT=verify.
	void                                   CheckCanary(uint64_t slot_offset);
	std::unique_ptr<Buffer>                m_canary;
	std::unique_ptr<Buffer>                m_canary_publish;
	bool                                   m_canary_ready = false;
	std::atomic<uint64_t>                  m_canary_mismatches {0};
	std::atomic<uint32_t>                  m_canary_logged {0};
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
