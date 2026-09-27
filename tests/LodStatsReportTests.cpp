#include "graphics/host_gpu/renderer/lodStatsReport.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

using namespace Libs::Graphics::LodStatsReport;

void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "LodStatsReportTests: failed: %s\n", message);
    std::abort();
  }
}

// The streamer's reading of an entry (eboot+0x7021120): the field it calls "MipClamp" gates
// promotion to full resolution and blocks eviction; "Drawn" resets the idle count.
bool MipClamp(uint64_t entry) { return (entry & 0xffffffu) != 0; }
bool Drawn(uint64_t entry) { return (entry & NoData) != NoData; }
uint32_t Level(uint64_t entry) { return static_cast<uint32_t>((entry >> 56u) & 0xfu); }
uint32_t CounterId(uint64_t entry) { return static_cast<uint32_t>((entry >> 24u) & 0xffu); }

// A T# with the mip-statistics fields set: counter enable/id, MIN_LOD (U4.8) and BASE_LEVEL.
std::array<uint32_t, 8> TSharp(bool counter, uint32_t id, uint32_t min_lod, uint32_t base_level) {
  std::array<uint32_t, 8> t{};
  t[1] = (min_lod & 0xfffu) << 8u;
  t[3] = (base_level & 0xfu) << 12u;
  t[5] = counter ? (1u << 25u) : 0u;
  t[6] = id & 0xffu;
  return t;
}

struct Counters {
  std::vector<uint32_t> words = std::vector<uint32_t>(Entries * 2u, 0u);
  Counters() {
    for (uint32_t i = 0; i < Entries; i++) {
      words[i] = Unsampled;
    }
  }
  uint64_t Entry(uint32_t counter) const {
    std::array<uint8_t, ReportSize> report{};
    (void)PackReport(words.data(), report.data());
    uint64_t entry = 0;
    std::memcpy(&entry, report.data() + 64 + counter * sizeof(uint64_t), sizeof(entry));
    return entry;
  }
};

void TestImageField() {
  const auto disabled = TSharp(false, 7, 1024, 0);
  Check(ImageField(disabled.data(), true, true) == 0x8000u,
        "a T# without MipStatsCntEn has no counter");

  const auto head = TSharp(true, 0xab, 1024, 0);
  Check(ImageField(head.data(), true, true) == (0xabu | (1024u << 16u)),
        "clamp counting uses the T# MIN_LOD as the threshold");
  Check(ImageField(head.data(), true, false) == (0xabu | (0xfffu << 16u)),
        "sample counting uses a threshold beyond every recorded level");

  const auto based = TSharp(true, 3, 768, 2);
  Check(ImageField(based.data(), true, true) == (3u | (2u << 8u) | (768u << 16u)),
        "absolute levels carry BASE_LEVEL and the absolute MIN_LOD");
  Check(ImageField(based.data(), false, true) == (3u | (256u << 16u)),
        "relative levels compare against MIN_LOD relative to BASE_LEVEL");
  const auto below_base = TSharp(true, 3, 256, 2);
  Check(ImageField(below_base.data(), false, true) == 3u,
        "a MIN_LOD below BASE_LEVEL never clamps relative levels");
}

// Astro Bot's streaming textures: a 128 KiB head keeps only the coarse mips (T# MIN_LOD 4.0),
// the full file every mip (MIN_LOD 0).
void TestClampCounting() {
  Counters head;
  const auto head_field = ImageField(TSharp(true, 12, 1024, 0).data(), true, true);
  RecordSample(head.words.data(), head_field, 5.25f); // far away: the head's mips suffice
  RecordSample(head.words.data(), head_field, 4.0f);
  auto entry = head.Entry(12);
  Check(Drawn(entry) && Level(entry) == 4u && !MipClamp(entry) && CounterId(entry) == 12u,
        "a head sampled at or above its MIN_LOD is drawn but not clamped");

  RecordSample(head.words.data(), head_field, 2.5f); // close: wants mip 2, clamped to 4
  entry = head.Entry(12);
  Check(Drawn(entry) && Level(entry) == 2u && MipClamp(entry) && (entry & 0xffffffu) == 1u,
        "a head sampled below its MIN_LOD reports the clamp and the finest wanted level");

  Counters full;
  const auto full_field = ImageField(TSharp(true, 200, 0, 0).data(), true, true);
  RecordSample(full.words.data(), full_field, 3.0f);
  RecordSample(full.words.data(), full_field, -2.0f); // magnified
  entry = full.Entry(200);
  Check(Drawn(entry) && Level(entry) == 0u && !MipClamp(entry) && CounterId(entry) == 200u,
        "a fully resident texture is never clamped, even when magnified");

  Counters idle;
  entry = idle.Entry(99);
  Check(!Drawn(entry) && !MipClamp(entry) && Level(entry) == 0xfu && CounterId(entry) == 99u,
        "an unsampled counter reports no mip level and no count");

  Counters disabled;
  RecordSample(disabled.words.data(), 0x8000u, 0.0f);
  Check(disabled.words[0] == Unsampled && disabled.words[Entries] == 0u,
        "an image without a counter records nothing");
}

void TestSampleCounting() {
  Counters counters;
  const auto field = ImageField(TSharp(true, 5, 1024, 0).data(), true, false);
  RecordSample(counters.words.data(), field, 9.0f);
  RecordSample(counters.words.data(), field, 14.9f);
  const auto entry = counters.Entry(5);
  Check(Drawn(entry) && Level(entry) == 9u && (entry & 0xffffffu) == 2u,
        "KYTY_LOD_STATS_COUNT=samples counts every sample (U25..U47)");
}

void TestReportLayout() {
  Counters counters;
  counters.words[0] = 3u;
  counters.words[Entries + 0] = 0x2000000u; // saturates
  counters.words[17] = 6u;                  // drawn, count 0
  counters.words[255] = 20u;                // above level 14
  std::array<uint8_t, ReportSize> report{};
  report.fill(0xcd);
  const auto summary = PackReport(counters.words.data(), report.data());
  uint32_t header[16]{};
  std::memcpy(header, report.data(), sizeof(header));
  Check(header[0] == 1u, "report header dword 0 marks the report complete");
  for (uint32_t i = 1; i < 16; i++) {
    Check(header[i] == 0u, "the rest of the header is zero");
  }
  Check(counters.Entry(0) == ((3ull << 56u) | 0xffffffull), "count saturates at 24 bits");
  const auto drawn = counters.Entry(17);
  Check(Drawn(drawn) && Level(drawn) == 6u && !MipClamp(drawn) && CounterId(drawn) == 17u,
        "a drawn counter without a count keeps its level and id");
  Check(counters.Entry(255) == ((14ull << 56u) | (255ull << 24u)), "levels are limited to 14");
  Check(summary.drawn == 3u && summary.counted == 1u && summary.count_total == 0x2000000u,
        "report summary counts drawn and counted entries");
  Check(PackEntry(Unsampled, 0, 7) == (NoData | (7ull << 24u)), "unsampled entry layout");
}

void TestSwitches() {
  Check(ParsePublish(nullptr, nullptr) == Publish::Completion, "completion is the default");
  Check(ParsePublish("", nullptr) == Publish::Completion, "empty selects the default");
  Check(ParsePublish("rewrite", nullptr) == Publish::Rewrite, "rewrite selects U33..U47");
  Check(ParsePublish("record", nullptr) == Publish::Record, "record selects U26..U32");
  Check(ParsePublish(nullptr, "0") == Publish::Record,
        "KYTY_LOD_REPORT_COMPLETION_WRITE=0 keeps its U33 meaning");
  Check(ParsePublish("completion", "0") == Publish::Completion,
        "KYTY_LOD_REPORT_PUBLISH wins over the U33 switch");
  Check(ParseCountClamped(nullptr) && ParseCountClamped("") && ParseCountClamped("clamp"),
        "clamp counting is the default");
  Check(!ParseCountClamped("samples"), "samples selects the U25..U47 count");
}

} // namespace

int main() {
  TestImageField();
  TestClampCounting();
  TestSampleCounting();
  TestReportLayout();
  TestSwitches();
  std::printf("LodStatsReportTests: ok\n");
  return 0;
}
