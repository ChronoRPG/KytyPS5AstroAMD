#include "graphics/host_gpu/renderer/cache/imageCacheGcPolicy.h"
#include "common/lruCache.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

using Policy = Libs::Graphics::ImageCacheGcPolicy;
constexpr uint64_t GiB = 1024 * Policy::MiB;

void Check(bool condition, const char* description) {
	if (!condition) {
		std::fprintf(stderr, "ImageCacheGcPolicyTests: failed: %s\n", description);
		std::abort();
	}
}

void TestHeadroomAndHysteresis() {
	Policy policy(8 * GiB);
	Check(policy.Low() < policy.High() && policy.High() < policy.Critical(),
	      "all watermarks are distinct and ordered");
	for (uint64_t submission = 0; submission < 10000; ++submission) {
		Check(policy.Begin(policy.High() - 1, submission).deletions == 0,
		      "submission count alone never evicts the cache");
	}
	Check(policy.Begin(policy.High(), 10000).deletions != 0, "high watermark starts collection");
	policy.Complete(10000, policy.High() - 1, true);
	Check(policy.Begin(policy.High() - 1, 10008).deletions != 0,
	      "collection continues between watermarks");
	policy.Complete(10008, policy.Low(), true);
	Check(policy.Begin(policy.High() - 1, 10016).deletions == 0,
	      "low watermark ends collection until the high watermark is crossed again");
}

void TestRetryAndEmergency() {
	Policy policy(8 * GiB);
	const auto ordinary = policy.Begin(policy.High(), 100);
	Check(!ordinary.critical && ordinary.deletions != 0, "high watermark is normal pressure");
	policy.Complete(100, policy.High(), false);
	Check(policy.Begin(policy.High(), 101).deletions == 0,
	      "irreducible pressure does not rescan on every submission");
	const auto emergency = policy.Begin(policy.Critical(), 102);
	Check(emergency.critical && emergency.deletions > ordinary.deletions &&
	          emergency.bytes > ordinary.bytes,
	      "critical pressure bypasses the ordinary cooldown with bounded larger work");
	policy.Complete(102, policy.Critical(), false);
	Check(policy.Begin(policy.Critical(), 103).deletions == 0,
	      "unsuccessful critical collection also backs off");
	Check(policy.Begin(policy.Critical(), 110).critical, "critical collection retries after cooldown");
	policy.Complete(110, policy.Critical(), false);
	Check(policy.Begin(policy.Critical() + GiB, 111).critical,
	      "substantial new allocation bypasses cooldown");
}

void TestBudgetBoundaries() {
	for (const uint64_t budget: {uint64_t {0}, uint64_t {1}, 256 * Policy::MiB, 4 * GiB,
	                            uint64_t {UINT64_MAX}}) {
		Policy policy(budget);
		Check(policy.Low() > 0 && policy.Low() < policy.High() &&
		          policy.High() < policy.Critical(),
		      "missing, small and maximum budgets have valid nonzero thresholds");
		Check(policy.Begin(0, 0).deletions == 0, "empty usage never starts collection");
		Check(policy.Begin(policy.Critical(), UINT64_MAX - 1).critical,
		      "large submission epochs remain usable");
		policy.Complete(UINT64_MAX - 1, policy.Critical(), false);
		Check(policy.Begin(policy.Critical(), UINT64_MAX - 1).deletions == 0,
		      "cooldown epoch addition does not wrap");
	}
}

void TestBoundedScanPastPinnedPrefix() {
	Common::LeastRecentlyUsedCache<uint32_t, uint64_t> cache;
	decltype(cache)::Cursor cursor;
	std::vector<size_t> ids;
	for (uint32_t image = 0; image < 20000; ++image) {
		ids.push_back(cache.Insert(image, 0));
	}
	uint32_t eligible = 0;
	for (uint32_t pass = 0; pass < 10; ++pass) {
		uint32_t scanned = 0;
		cache.ScanItemsBelow(100, cursor, 2048, [&](uint32_t image) {
			++scanned;
			if (image >= 19000) ++eligible;
		});
		Check(scanned <= 2048, "a continuation never exceeds its scan budget");
	}
	Check(eligible == 1000, "eligible images beyond 19000 pinned entries are eventually visited");
	uint32_t original_order = 0;
	cache.ForEachItemBelow(0, [&](uint32_t image) {
		Check(image == original_order++, "scanning preserves actual LRU order and ages");
	});
	Check(original_order == 20000, "bounded scan does not alter item ages");
}

void TestScanCursorSurvivesSlotReuse() {
	Common::LeastRecentlyUsedCache<uint32_t, uint64_t> cache;
	decltype(cache)::Cursor cursor;
	const auto first = cache.Insert(1, 0);
	const auto second = cache.Insert(2, 0);
	const auto third = cache.Insert(3, 0);
	(void)first;
	(void)third;
	cache.ScanItemsBelow(100, cursor, 1, [](uint32_t) {});
	cache.Free(second);
	Check(cache.Insert(4, 1) == second, "test reuses the cursor's former slot");
	std::vector<uint32_t> seen;
	cache.ScanItemsBelow(100, cursor, 3, [&](uint32_t image) { seen.push_back(image); });
	Check(seen == std::vector<uint32_t> {4, 3, 1},
	      "slot rotation reads the current generation instead of a recycled old object");
	seen.clear();
	cache.ScanItemsBelow(0, cursor, 3, [&](uint32_t image) { seen.push_back(image); });
	Check(seen == std::vector<uint32_t> {3, 1}, "continuation honors the age cutoff");
}

void TestTouchedBoundaryCannotHideMiddle() {
	Common::LeastRecentlyUsedCache<uint32_t, uint64_t> cache;
	decltype(cache)::Cursor cursor;
	std::vector<size_t> ids;
	for (uint32_t image = 0; image < 6; ++image) ids.push_back(cache.Insert(image, 0));
	std::vector<uint32_t> seen;
	cache.ScanItemsBelow(10, cursor, 1, [&](uint32_t image) { seen.push_back(image); });
	// Move each saved cursor item to the LRU tail before resuming. Link-based
	// continuation would jump over the middle every time this happened.
	cache.Touch(ids[1], 20);
	cache.ScanItemsBelow(10, cursor, 2, [&](uint32_t image) { seen.push_back(image); });
	cache.Touch(ids[3], 21);
	cache.ScanItemsBelow(10, cursor, 2, [&](uint32_t image) { seen.push_back(image); });
	cache.Touch(ids[5], 22);
	cache.ScanItemsBelow(10, cursor, 1, [&](uint32_t image) { seen.push_back(image); });
	Check(seen == std::vector<uint32_t> {0, 2, 4},
	      "touched boundary items cannot starve older middle entries");
}

} // namespace

int main() {
	TestHeadroomAndHysteresis();
	TestRetryAndEmergency();
	TestBudgetBoundaries();
	TestBoundedScanPastPinnedPrefix();
	TestScanCursorSurvivesSlotReuse();
	TestTouchedBoundaryCannotHideMiddle();
	std::puts("ImageCacheGcPolicyTests passed");
}
