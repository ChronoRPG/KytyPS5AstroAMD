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

void TestReportLayout() {
  std::vector<uint32_t> words(Entries * 2u, 0u);
  for (uint32_t i = 0; i < Entries; i++) {
    words[i] = Unsampled;
  }
  words[0] = 3u;
  words[Entries + 0] = 0x2000000u; // saturates
  words[17] = 6u;                   // drawn, count 0
  words[255] = 20u;                 // above level 14
  std::array<uint8_t, ReportSize> report{};
  report.fill(0xcd);
  const auto summary = PackReport(words.data(), report.data());
  uint32_t header[16]{};
  std::memcpy(header, report.data(), sizeof(header));
  Check(header[0] == 1u, "report header dword 0 marks the report complete");
  for (uint32_t i = 1; i < 16; i++) {
    Check(header[i] == 0u, "the rest of the header is zero");
  }
  const auto entry = [&](uint32_t counter) {
    uint64_t value = 0;
    std::memcpy(&value, report.data() + 64 + counter * sizeof(uint64_t), sizeof(value));
    return value;
  };
  Check(entry(0) == ((3ull << 56u) | 0xffffffull), "count saturates at 24 bits");
  Check(Drawn(entry(17)) && Level(entry(17)) == 6u && !MipClamp(entry(17)) &&
            CounterId(entry(17)) == 17u,
        "a drawn counter without a count keeps its level and id");
  Check(entry(255) == ((14ull << 56u) | (255ull << 24u)), "levels are limited to 14");
  Check(!Drawn(entry(99)) && !MipClamp(entry(99)) && Level(entry(99)) == 0xfu &&
            CounterId(entry(99)) == 99u,
        "an unsampled counter reports no mip level and no count");
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
}

} // namespace

int main() {
  TestReportLayout();
  TestSwitches();
  std::printf("LodStatsReportTests: ok\n");
  return 0;
}
