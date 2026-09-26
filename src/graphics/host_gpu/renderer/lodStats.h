#ifndef KYTY_RENDERER_LODSTATS_H_
#define KYTY_RENDERER_LODSTATS_H_

#include "graphics/host_gpu/vulkanCommon.h"

#include <array>
#include <cstdint>
#include <memory>

namespace Libs::Graphics {
class Buffer;
class RenderContext;

// GPU mip statistics for GET_LOD_STATS (KYTY_LOD_STATS_MODE=gpu, the default).
//
// Instrumented pixel shaders record, per T# mip-statistics counter (MipStatsCntId), the finest
// mip level sampled and a sample count in a device-local buffer. A GET_LOD_STATS packet copies
// the counters into a private host-visible slot in command order, resets them, and a completion
// callback publishes the report into guest memory in the layout Astro Bot's streamer parses
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
};

} // namespace Libs::Graphics

#endif // KYTY_RENDERER_LODSTATS_H_
