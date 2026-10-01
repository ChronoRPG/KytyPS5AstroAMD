#include "graphics/host_gpu/renderer/drawPrep/drawRun.h"

#include "common/assert.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/host_gpu/renderer/drawPrep/drawPrep.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <xxhash.h>

namespace Libs::Graphics::DrawRun {

namespace Detail {

Mode ReadMode() {
	const auto* value = std::getenv("KYTY_DRAW_RUN");
	if (value == nullptr || value[0] == '\0' || std::strcmp(value, "0") == 0 ||
	    std::strcmp(value, "off") == 0) {
		return Mode::Off;
	}
	if (std::strcmp(value, "verify") == 0 || std::strcmp(value, "exit") == 0) {
		return Mode::Verify;
	}
	return Mode::On;
}

bool ReadExit() {
	const auto* value = std::getenv("KYTY_DRAW_RUN");
	return value != nullptr && std::strcmp(value, "exit") == 0;
}

bool ReadAcquire() {
	const auto* value = std::getenv("KYTY_DRAW_RUN_ACQUIRE");
	return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

} // namespace Detail

namespace {

std::atomic<uint64_t> g_activity {1};
Totals                g_totals;

uint64_t NowNs() {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
	                                 std::chrono::steady_clock::now().time_since_epoch())
	                                 .count());
}

// Seed-chained one-shot hashes (the streaming state type is opaque here).
uint64_t HashBytes(uint64_t hash, const void* data, size_t size) {
	return XXH3_64bits_withSeed(data, size, hash);
}

uint64_t HashValues(uint64_t hash, const std::vector<ShaderRecompiler::IR::DescriptorValue>& values) {
	const uint64_t count = values.size();
	hash                 = HashBytes(hash, &count, sizeof(count));
	for (const auto& value: values) {
		const auto dwords =
		    std::min<uint32_t>(value.dword_count, static_cast<uint32_t>(value.dwords.size()));
		hash = HashBytes(hash ^ value.dword_count, value.dwords.data(), dwords * sizeof(uint32_t));
	}
	return hash;
}

} // namespace

void NoteForeignActivity() noexcept {
	g_activity.fetch_add(1, std::memory_order_relaxed);
}

uint64_t ActivityEpoch() noexcept {
	return g_activity.load(std::memory_order_relaxed);
}

uint64_t StructureKey(const HW::Context& context, const HW::UserConfig& user_config,
                      const DrawPrep::PreparedDraw& prepared) {
	if (!prepared.ok || prepared.failure != DrawPrep::Failure::None ||
	    !prepared.vertex_info.stage.program) {
		return 0;
	}
	// Every context register (targets, depth/stencil, blend, raster, viewports, shader registers).
	uint64_t hash = HashBytes(0x9e3779b97f4a7c15ull, &context, sizeof(context));
	// The user-config registers but the per-draw index offset and object id.
	auto config = user_config;
	config.SetIndexOffset(0);
	config.SetObjectId(0);
	hash = HashBytes(hash, &config, sizeof(config));
	// The programs and the T#/S# words of both stages (V# words, user data and the flattened SRT are
	// per-draw data).
	const std::array<const void*, 2> programs {prepared.vertex_info.stage.program,
	                                          prepared.pixel_active ? prepared.pixel_info.stage.program
	                                                                : nullptr};
	hash = HashBytes(hash, programs.data(), sizeof(programs));
	const uint32_t flags = (prepared.pixel_active ? 1u : 0u) |
	                       (prepared.programs.VertexStageCount() == 1 ? 2u : 0u);
	hash = HashBytes(hash, &flags, sizeof(flags));
	hash = HashValues(hash, prepared.vertex_prep.resources.images);
	hash = HashValues(hash, prepared.vertex_prep.resources.samplers);
	if (prepared.pixel_active) {
		hash = HashValues(hash, prepared.pixel_prep.resources.images);
		hash = HashValues(hash, prepared.pixel_prep.resources.samplers);
	}
	return hash != 0 ? hash : 1;
}

Totals& GetTotals() {
	return g_totals;
}

void CountMiss(Miss miss) noexcept {
	g_totals.misses[static_cast<uint32_t>(miss)].fetch_add(1, std::memory_order_relaxed);
}

void CountVerifyCheck() noexcept {
	g_totals.verify_checks.fetch_add(1, std::memory_order_relaxed);
}

void ReportMismatch(const char* what, uint64_t detail) {
	g_totals.verify_mismatches.fetch_add(1, std::memory_order_relaxed);
	static std::atomic<uint32_t> logged {0};
	if (logged.fetch_add(1, std::memory_order_relaxed) < 32) {
		std::printf("DrawRunVerify: a continuation would have reused a different %s (detail %" PRIu64
		            " = 0x%" PRIx64 ")\n",
		            what, detail, detail);
		std::fflush(stdout);
	}
	if (VerifyExit()) {
		EXIT("DrawRunVerify: a continuation would have reused a different %s\n", what);
	}
}

void PrintSummary() {
	if (!Enabled()) {
		return;
	}
	static uint64_t                                          last_ns = 0;
	static std::array<uint64_t, 11>                          last {};
	static std::array<uint64_t, static_cast<size_t>(Miss::Count)> last_misses {};
	const auto                                               now = NowNs();
	if (last_ns == 0) {
		last_ns = now;
		return;
	}
	if (now - last_ns < 10'000'000'000ull) {
		return;
	}
	const std::array<uint64_t, 11> values {
	    g_totals.draws.load(std::memory_order_relaxed),
	    g_totals.eligible.load(std::memory_order_relaxed),
	    g_totals.key_matches.load(std::memory_order_relaxed),
	    g_totals.continued.load(std::memory_order_relaxed),
	    g_totals.late_fallbacks.load(std::memory_order_relaxed),
	    g_totals.pipeline_lookups.load(std::memory_order_relaxed),
	    g_totals.verify_checks.load(std::memory_order_relaxed),
	    g_totals.verify_mismatches.load(std::memory_order_relaxed),
	    g_totals.alias_excluded.load(std::memory_order_relaxed),
	    g_totals.acquire_reused.load(std::memory_order_relaxed),
	    g_totals.dynamic_emitted.load(std::memory_order_relaxed)};
	std::array<uint64_t, 11> delta {};
	for (size_t i = 0; i < values.size(); i++) {
		delta[i] = values[i] - last[i];
	}
	static const char* const miss_names[] = {"activity", "command",  "instance", "programs",
	                                         "validation", "images", "pipeline"};
	std::string misses;
	for (size_t i = 0; i < last_misses.size(); i++) {
		const auto value = g_totals.misses[i].load(std::memory_order_relaxed);
		if (value != last_misses[i]) {
			misses += " " + std::string(miss_names[i]) + "=" + std::to_string(value - last_misses[i]);
			last_misses[i] = value;
		}
	}
	std::printf("DrawRun %.0fs (%s): %" PRIu64 " draws, %" PRIu64 " eligible, %" PRIu64
	            " key matches, %" PRIu64 " continued (%.1f%%), %" PRIu64
	            " late fallbacks; misses:%s; verify %" PRIu64 " checks, %" PRIu64
	            " mismatches; %" PRIu64 " alias-excluded, %" PRIu64 " acquisitions reused, %" PRIu64
	            " dynamic re-emitted\n",
	            static_cast<double>(now - last_ns) * 1e-9,
	            GetMode() == Mode::Verify ? "verify" : "on", delta[0], delta[1], delta[2], delta[3],
	            delta[0] != 0 ? 100.0 * static_cast<double>(delta[3]) / static_cast<double>(delta[0])
	                          : 0.0,
	            delta[4], misses.empty() ? " none" : misses.c_str(), delta[6], delta[7], delta[8],
	            delta[9], delta[10]);
	std::fflush(stdout);
	last    = values;
	last_ns = now;
}

} // namespace Libs::Graphics::DrawRun
