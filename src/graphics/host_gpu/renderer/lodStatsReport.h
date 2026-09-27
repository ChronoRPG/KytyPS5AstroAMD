#ifndef KYTY_RENDERER_LODSTATSREPORT_H_
#define KYTY_RENDERER_LODSTATSREPORT_H_

// GET_LOD_STATS report layout, free of Vulkan so it can be tested on the CPU. Used by
// LodStatsCounter (lodStats.h).
//
// Report, as Astro Bot's streamer reads it (parser eboot+0x479d40, lookup eboot+0x7022e40,
// per-texture update eboot+0x7021120, debug view eboot+0x7020180):
//   64-byte header; dword 0 non-zero marks the report complete.
//   256 64-bit entries, one per mip-statistics counter:
//     bits 0..23   the counter's count. The guest's debug view labels this field "MipClamp"; its
//                  streamer promotes a texture to full resolution only while the field is
//                  non-zero, and refuses to evict it while it is non-zero.
//     bits 24..31  the counter id (the debug view draws the texture in that column).
//     bits 56..59  finest mip level sampled; 0xF when nothing was sampled ("Drawn" = not 0xF).

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace Libs::Graphics::LodStatsReport {

constexpr uint32_t Counters   = 256;
constexpr uint32_t Entries    = Counters + 1; // the shader's last entry absorbs images without id
constexpr uint32_t ReportSize = 64 + Counters * 8;
constexpr uint64_t NoData     = 0x0F00000000000000ull;
constexpr uint32_t Unsampled  = 0xffffffffu; // reset value of a finest-level word

// When a report becomes visible to the guest (KYTY_LOD_REPORT_PUBLISH).
enum class Publish : uint8_t {
	// Default: the packet's own statistics, written when its GPU copy completes.
	Completion,
	// U33..U47: at record time the newest completed statistics, rewritten with the packet's own
	// interval at completion when the guest has not touched the slot.
	Rewrite,
	// U26..U32 (also KYTY_LOD_REPORT_COMPLETION_WRITE=0): only the record-time write.
	Record,
};

[[nodiscard]] inline Publish ParsePublish(const char* publish, const char* completion_write) {
	if (publish != nullptr && publish[0] != 0) {
		if (std::strcmp(publish, "rewrite") == 0) {
			return Publish::Rewrite;
		}
		if (std::strcmp(publish, "record") == 0) {
			return Publish::Record;
		}
		return Publish::Completion;
	}
	// The U33 switch: =0 kept only the record-time write.
	if (completion_write != nullptr && completion_write[0] == '0') {
		return Publish::Record;
	}
	return Publish::Completion;
}

// One report entry from a counter's finest-level and count words.
[[nodiscard]] inline uint64_t PackEntry(uint32_t finest, uint32_t count, uint32_t counter) {
	const uint64_t level = finest == Unsampled ? 0xfu : std::min<uint32_t>(finest, 14u);
	return (level << 56u) | (uint64_t {counter & 0xffu} << 24u) |
	       std::min<uint32_t>(count, 0xffffffu);
}

struct Summary {
	uint32_t drawn       = 0; // entries with a finest level
	uint32_t counted     = 0; // entries with a non-zero count
	uint64_t count_total = 0;
	double   mean_finest = 0.0;
};

// Fills a complete report (header dword 0 = 1) from the copied counter words.
inline Summary PackReport(const uint32_t* words, uint8_t* report) {
	Summary summary;
	std::memset(report, 0, ReportSize);
	const uint32_t valid = 1;
	std::memcpy(report, &valid, sizeof(valid));
	uint64_t finest_total = 0;
	for (uint32_t counter = 0; counter < Counters; counter++) {
		const auto finest = words[counter];
		const auto count  = words[Entries + counter];
		const auto entry  = PackEntry(finest, count, counter);
		std::memcpy(report + 64 + counter * sizeof(uint64_t), &entry, sizeof(entry));
		if (finest != Unsampled) {
			summary.drawn++;
			finest_total += std::min<uint32_t>(finest, 14u);
		}
		if (count != 0) {
			summary.counted++;
			summary.count_total += count;
		}
	}
	summary.mean_finest =
	    summary.drawn != 0 ? static_cast<double>(finest_total) / summary.drawn : 0.0;
	return summary;
}

} // namespace Libs::Graphics::LodStatsReport

#endif // KYTY_RENDERER_LODSTATSREPORT_H_
