#ifndef KYTY_RENDERER_LODSTATSREPORT_H_
#define KYTY_RENDERER_LODSTATSREPORT_H_

// GET_LOD_STATS report layout and the per-image counting field, free of Vulkan so they can be
// tested on the CPU. Used by LodStatsCounter (lodStats.h) and the per-draw descriptor data.
//
// Report, as Astro Bot's streamer reads it (parser eboot+0x479d40, lookup eboot+0x7022e40,
// per-texture update eboot+0x7021120, debug view eboot+0x7020180):
//   64-byte header; dword 0 non-zero marks the report complete.
//   256 64-bit entries, one per mip-statistics counter:
//     bits 0..23   samples whose LOD the T# MIN_LOD clamp raised. The guest's debug view labels
//                  this field "MipClamp". Its streamer promotes a texture to full resolution only
//                  while the field is non-zero, and refuses to evict it while it is non-zero.
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
// ImageField bit set when the image's T# has no mip-statistics counter: the instrumented shader
// records nothing for it.
constexpr uint32_t NoCounterFlag = 0x8000u;

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

// KYTY_LOD_STATS_COUNT: "clamp" (default) counts samples below the T# MIN_LOD, "samples" counts
// every sample (U25..U47).
[[nodiscard]] inline bool ParseCountClamped(const char* value) {
	return value == nullptr || std::strcmp(value, "samples") != 0;
}

// The 32-bit per-image field of an instrumented shader's data, from the image's T# dwords:
//   bits 0..7   MipStatsCntId (dword 6 bits 0..7)
//   bits 8..11  BASE_LEVEL (dword 3 bits 12..15) when absolute_levels: recorded levels are
//               absolute (KYTY_MIP_STATS_BASE_LEVEL=0 keeps them relative, the U27 behaviour)
//   bit 15      set when MipStatsCntEn (dword 5 bit 25) is clear
//   bits 16..27 U4.8 LOD threshold in the recorded level space: a sample finer than it counts.
//               count_clamped: MIN_LOD (dword 1 bits 8..19); otherwise 0xfff, beyond the finest
//               level the shader records (14), so every sample counts.
[[nodiscard]] inline uint32_t ImageField(const uint32_t* tsharp, bool absolute_levels,
                                         bool count_clamped) {
	if (((tsharp[5] >> 25u) & 1u) == 0u) {
		return 0x8000u;
	}
	const uint32_t id         = tsharp[6] & 0xffu;
	const uint32_t base_level = (tsharp[3] >> 12u) & 0xfu;
	uint32_t       threshold  = 0xfffu;
	if (count_clamped) {
		// MIN_LOD and BASE_LEVEL are absolute levels.
		const uint32_t min_lod = (tsharp[1] >> 8u) & 0xfffu;
		threshold = absolute_levels ? min_lod
		                            : (min_lod > base_level * 256u ? min_lod - base_level * 256u
		                                                           : 0u);
	}
	return id | ((absolute_levels ? base_level : 0u) << 8u) | (threshold << 16u);
}

// CPU model of one sample recorded by the instrumented shader (EmitMipStatsSample and
// EmitGatedMipStatsRecord): `lod` is the unclamped LOD relative to the view's base level.
// `words` is the counter buffer (Entries finest-level words, then Entries counts).
inline void RecordSample(uint32_t* words, uint32_t field, float lod) {
	if ((field & 0x8000u) != 0u) {
		return;
	}
	const uint32_t counter   = field & 0xffu;
	const float    absolute  = lod + static_cast<float>((field >> 8u) & 0xfu);
	const float    clamped   = std::clamp(absolute, 0.0f, 14.0f);
	const auto     level     = static_cast<uint32_t>(clamped); // floor: clamped is not negative
	const float    threshold = static_cast<float>((field >> 16u) & 0xfffu) / 256.0f;
	words[counter]           = std::min(words[counter], level);
	if (clamped < threshold) {
		words[Entries + counter]++;
	}
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
