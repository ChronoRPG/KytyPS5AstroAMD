// Draw-prep S4-S6 unit tests: the coherence log, read-set coalescing and certificate checks,
// and (S6) the preparation window ring.
#include "graphics/host_gpu/coherenceLog.h"
#include "graphics/host_gpu/renderer/drawPrep/packetClass.h"
#include "graphics/host_gpu/renderer/drawPrep/readSet.h"
#include "graphics/host_gpu/renderer/drawPrep/window.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

namespace {

using namespace Libs::Graphics;

int g_failures = 0;

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "FAILED: %s\n", text);
		g_failures++;
	}
}

// ---------------------------------------------------------------------------------------------
// CoherenceLog

void TestLogEmptyIntervalIsClean() {
	auto  log_owner = std::make_unique<Coherence::Log>();
	auto& log       = *log_owner;
	const auto     g = log.Generation();
	Check(g == 1, "log generation starts at 1");
	const std::array<Coherence::Range, 1> ranges {{{0x1000, 0x2000}}};
	Check(log.Check(g, g, ranges).result == Coherence::CheckResult::Clean,
	      "empty interval is clean");
	Check(log.Check(g + 5, g, ranges).result == Coherence::CheckResult::Clean,
	      "reversed interval is clean");
}

void TestLogIntersection() {
	auto  log_owner = std::make_unique<Coherence::Log>();
	auto& log       = *log_owner;
	const auto     g0 = log.Generation();
	log.Append(Coherence::MakeRange(0x5000, 0x100), Coherence::Source::BufferDirtyAdd);
	log.Append(Coherence::MakeRange(0x9000, 0x10), Coherence::Source::ImageGpuModified);
	const auto g1 = log.Generation();
	Check(g1 == g0 + 2, "each append claims one generation");

	const std::array<Coherence::Range, 2> disjoint {{{0x1000, 0x5000}, {0x5100, 0x9000}}};
	auto outcome = log.Check(g0, g1, disjoint);
	Check(outcome.result == Coherence::CheckResult::Clean, "touching ranges do not intersect");
	Check(outcome.entries == 2, "every entry in the interval is examined");

	const std::array<Coherence::Range, 1> hit {{{0x900f, 0x9011}}};
	outcome = log.Check(g0, g1, hit);
	Check(outcome.result == Coherence::CheckResult::Conflict &&
	          outcome.conflict_source == Coherence::Source::ImageGpuModified,
	      "last byte overlap is a conflict with its source");

	// Only the second entry is newer than g0 + 1.
	const std::array<Coherence::Range, 1> first_only {{{0x5000, 0x5001}}};
	Check(log.Check(g0 + 1, g1, first_only).result == Coherence::CheckResult::Clean,
	      "entries at or before the certificate generation are ignored");
	Check(log.Check(g0, g1, first_only).result == Coherence::CheckResult::Conflict,
	      "an entry after the certificate generation conflicts");

	log.Append(Coherence::Universe, Coherence::Source::Universe);
	Check(log.Check(g1, log.Generation(), disjoint).result == Coherence::CheckResult::Conflict,
	      "a universe entry conflicts with everything");
	const std::array<Coherence::Range, 0> none {};
	Check(log.Check(g1, log.Generation(), none).result == Coherence::CheckResult::Clean,
	      "an empty read set never conflicts");
}

void TestLogEmptyRangeNeverIntersects() {
	auto  log_owner = std::make_unique<Coherence::Log>();
	auto& log       = *log_owner;
	const auto     g0 = log.Generation();
	log.Append(Coherence::MakeRange(0x2000, 0), Coherence::Source::Test);
	const std::array<Coherence::Range, 1> ranges {{{0x1000, 0x3000}}};
	Check(log.Check(g0, log.Generation(), ranges).result == Coherence::CheckResult::Clean,
	      "a zero-sized entry does not intersect");
	Check(Coherence::MakeRange(UINT64_MAX - 4, 100).end == UINT64_MAX,
	      "range end saturates instead of wrapping");
}

void TestLogOverflow() {
	auto       log = std::make_unique<Coherence::Log>();
	const auto g0  = log->Generation();
	for (uint64_t i = 0; i < Coherence::Log::Capacity; i++) {
		log->Append(Coherence::MakeRange(0x100000 + i * 16, 16), Coherence::Source::Test);
	}
	const std::array<Coherence::Range, 1> ranges {{{0x10, 0x20}}};
	Check(log->Check(g0, log->Generation(), ranges).result == Coherence::CheckResult::Overflow,
	      "an interval as long as the ring overflows");
	// A recent interval is still exact after the ring wrapped.
	const auto g1 = log->Generation();
	log->Append(Coherence::MakeRange(0x10, 1), Coherence::Source::Test);
	Check(log->Check(g1, log->Generation(), ranges).result == Coherence::CheckResult::Conflict,
	      "entries after a wrap are exact");
	Coherence::Range  range;
	Coherence::Source source {};
	Check(!log->Read(g0 + 1, range, source), "a recycled slot is not readable as an old generation");
	Check(log->Read(log->Generation(), range, source) && range.begin == 0x10,
	      "the newest entry is readable");
}

// Every entry thread t appends is [((t + 1) << 20) + 8 i, +8) with Source::Test: a torn read
// (payload words from two appends) breaks this shape.
bool WellFormedEntry(Coherence::Range range, Coherence::Source source, uint32_t threads) {
	const auto window = range.begin >> 20u;
	return source == Coherence::Source::Test && range.end == range.begin + 8u &&
	       (range.begin & 7u) == 0u && window >= 1u && window <= threads &&
	       ((range.begin - (window << 20u)) >> 3u) < (1u << 17u);
}

// Appenders on several threads while readers read recent generations concurrently. Readers must
// only ever accept complete entries; with `laps` > 1 the ring wraps several times, so slots are
// rewritten while being read.
void RunConcurrentLog(Coherence::Log& log, uint32_t threads_count, uint32_t per_thread,
                      std::atomic<uint32_t>& torn, std::atomic<uint64_t>& reads_ok) {
	std::atomic<bool>        done {false};
	std::vector<std::thread> readers;
	for (uint32_t r = 0; r < 2; r++) {
		readers.emplace_back([&, r] {
			uint64_t salt = 0x9e3779b97f4a7c15ull * (r + 1u);
			while (!done.load(std::memory_order_acquire)) {
				const auto newest = log.Generation();
				salt ^= salt << 13u;
				salt ^= salt >> 7u;
				salt ^= salt << 17u;
				const auto back = salt % 64u;
				if (newest < Coherence::Log::FirstEntryGeneration + back) {
					continue;
				}
				Coherence::Range  range;
				Coherence::Source source {};
				if (log.Read(newest - back, range, source)) {
					reads_ok.fetch_add(1, std::memory_order_relaxed);
					if (!WellFormedEntry(range, source, threads_count)) {
						torn.fetch_add(1, std::memory_order_relaxed);
					}
				}
			}
		});
	}
	std::vector<std::thread> writers;
	for (uint32_t t = 0; t < threads_count; t++) {
		writers.emplace_back([&, t] {
			const uint64_t base = (uint64_t {t} + 1u) << 20u;
			for (uint32_t i = 0; i < per_thread; i++) {
				log.Append(Coherence::MakeRange(base + uint64_t {i} * 8u, 8),
				           Coherence::Source::Test);
			}
		});
	}
	for (auto& thread: writers) {
		thread.join();
	}
	done.store(true, std::memory_order_release);
	for (auto& thread: readers) {
		thread.join();
	}
}

void TestLogConcurrentAppends() {
	auto                    log       = std::make_unique<Coherence::Log>();
	constexpr uint32_t      Threads   = 8;
	constexpr uint32_t      PerThread = 1000;
	const auto              g0        = log->Generation();
	std::atomic<uint32_t>   torn {0};
	std::atomic<uint64_t>   reads_ok {0};
	RunConcurrentLog(*log, Threads, PerThread, torn, reads_ok);
	Check(torn.load() == 0, "concurrent readers never accept a torn entry");
	const auto g1 = log->Generation();
	Check(g1 - g0 == Threads * PerThread, "concurrent appends claim distinct generations");
	// Afterwards every entry is readable and falls in exactly one window.
	std::array<uint32_t, Threads> per_window {};
	bool                          all_read = true;
	for (auto g = g0 + 1; g <= g1; g++) {
		Coherence::Range  range;
		Coherence::Source source {};
		if (!log->Read(g, range, source) || range.end - range.begin != 8) {
			all_read = false;
			continue;
		}
		const auto window = (range.begin >> 20u) - 1u;
		if (window < Threads) {
			per_window[window]++;
		}
	}
	Check(all_read, "every concurrently appended entry is readable and intact");
	bool balanced = true;
	for (const auto count: per_window) {
		balanced &= count == PerThread;
	}
	Check(balanced, "every thread's entries are present exactly once");
	for (uint32_t t = 0; t < Threads; t++) {
		const uint64_t base = (uint64_t {t} + 1u) << 20u;
		const std::array<Coherence::Range, 1> probe {{{base + 8u * 500u, base + 8u * 500u + 1u}}};
		Check(log->Check(g0, g1, probe).result == Coherence::CheckResult::Conflict,
		      "a concurrently appended entry is found by Check");
	}
}

void TestLogConcurrentWrap() {
	// About five laps of the ring: slots are rewritten in turn while readers read them.
	auto                  log        = std::make_unique<Coherence::Log>();
	constexpr uint32_t    Threads    = 8;
	constexpr uint32_t    PerThread  = static_cast<uint32_t>(Coherence::Log::Capacity * 5u / 8u);
	const auto            g0         = log->Generation();
	std::atomic<uint32_t> torn {0};
	std::atomic<uint64_t> reads_ok {0};
	RunConcurrentLog(*log, Threads, PerThread, torn, reads_ok);
	Check(torn.load() == 0, "readers never accept a torn entry while the ring wraps");
	const auto g1 = log->Generation();
	Check(g1 - g0 == uint64_t {Threads} * PerThread, "wrapping appends claim distinct generations");
	bool last_lap = true;
	for (auto g = g1 - Coherence::Log::Capacity + 1u; g <= g1; g++) {
		Coherence::Range  range;
		Coherence::Source source {};
		last_lap &= log->Read(g, range, source) && WellFormedEntry(range, source, Threads);
	}
	Check(last_lap, "the last lap of a wrapped ring is readable and intact");
	Check(log->Check(g0, g1, std::array<Coherence::Range, 1> {{{0, 1}}}).result ==
	          Coherence::CheckResult::Overflow,
	      "an interval longer than the ring overflows");
}

void TestIntersectsAny() {
	const std::array<Coherence::Range, 3> ranges {{{10, 20}, {30, 40}, {50, 60}}};
	Check(!Coherence::Log::IntersectsAny({20, 30}, ranges), "gap between ranges");
	Check(Coherence::Log::IntersectsAny({19, 21}, ranges), "overlaps the first range end");
	Check(Coherence::Log::IntersectsAny({0, 100}, ranges), "covers all");
	Check(!Coherence::Log::IntersectsAny({60, 70}, ranges), "after the last range");
	Check(!Coherence::Log::IntersectsAny({0, 10}, ranges), "before the first range");
	Check(Coherence::Log::IntersectsAny({59, 60}, ranges), "last byte");
	Check(!Coherence::Log::IntersectsAny({35, 35}, ranges), "empty query");
}

// ---------------------------------------------------------------------------------------------
// ReadSet

struct FakeMemory {
	std::array<uint8_t, 4096> bytes {};
	uint64_t                  base          = 0x10000;
	uint64_t                  dirty_begin   = 0;
	uint64_t                  dirty_end     = 0;

	bool Read(uint64_t address, void* data, uint64_t size) const {
		if (address < base || address + size > base + bytes.size()) {
			return false;
		}
		if (address < dirty_end && dirty_begin < address + size) {
			return false;
		}
		std::memcpy(data, bytes.data() + (address - base), size);
		return true;
	}
};

void RecordFrom(DrawPrep::ReadSet& set, const FakeMemory& memory, uint64_t address,
                uint64_t size) {
	std::vector<uint8_t> data(size);
	Check(memory.Read(address, data.data(), size), "fake memory read");
	Check(set.Record(address, data.data(), size), "record");
}

void TestReadSetCoalesces() {
	FakeMemory memory;
	for (size_t i = 0; i < memory.bytes.size(); i++) {
		memory.bytes[i] = static_cast<uint8_t>(i * 7u + 3u);
	}
	DrawPrep::ReadSet set;
	RecordFrom(set, memory, 0x10100, 16);
	RecordFrom(set, memory, 0x10000, 8);
	RecordFrom(set, memory, 0x10108, 16); // overlaps the first read
	RecordFrom(set, memory, 0x10008, 4);  // touches the second read
	RecordFrom(set, memory, 0x10104, 4);  // contained in the first read
	RecordFrom(set, memory, 0x10800, 4);
	Check(set.Finish(), "consistent reads finish");
	const auto ranges = set.Ranges();
	Check(ranges.size() == 3, "reads coalesce into three ranges");
	Check(ranges.size() == 3 && ranges[0] == Coherence::Range {0x10000, 0x1000c} &&
	          ranges[1] == Coherence::Range {0x10100, 0x10118} &&
	          ranges[2] == Coherence::Range {0x10800, 0x10804},
	      "coalesced range bounds");
	bool bytes_match = true;
	for (size_t i = 0; i < ranges.size(); i++) {
		const auto bytes = set.RangeBytes(i);
		bytes_match &= std::memcmp(bytes.data(), memory.bytes.data() + (ranges[i].begin - memory.base),
		                           bytes.size()) == 0;
	}
	Check(bytes_match, "coalesced bytes equal memory");

	std::vector<uint8_t> scratch;
	const auto read = [&](uint64_t address, void* data, uint64_t size) {
		return memory.Read(address, data, size);
	};
	Check(set.Validate(read, scratch) == DrawPrep::ValidateResult::Ok,
	      "unchanged clean memory validates");
	memory.bytes[0x110] ^= 0xffu;
	Check(set.Validate(read, scratch) == DrawPrep::ValidateResult::Changed,
	      "a changed byte fails validation");
	memory.bytes[0x110] ^= 0xffu;
	memory.bytes[0x200] ^= 0xffu; // outside every range
	Check(set.Validate(read, scratch) == DrawPrep::ValidateResult::Ok,
	      "a change outside the read set is irrelevant");
	memory.dirty_begin = 0x10802;
	memory.dirty_end   = 0x10803;
	Check(set.Validate(read, scratch) == DrawPrep::ValidateResult::Unclean,
	      "an unclean byte in a range fails validation");
	memory.dirty_begin = memory.dirty_end = 0;
	Check(set.AllClean([&](uint64_t address, uint64_t size) {
		      std::vector<uint8_t> tmp(size);
		      return memory.Read(address, tmp.data(), size);
	      }),
	      "AllClean on clean memory");
}

void TestReadSetPageBoundary() {
	DrawPrep::ReadSet set;
	const std::array<uint8_t, 16> bytes {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	// Touching at a 4 KiB boundary: two ranges (two mappings may meet there).
	Check(set.Record(0x20ff8, bytes.data(), 8), "record below the boundary");
	Check(set.Record(0x21000, bytes.data() + 8, 8), "record above the boundary");
	// Touching inside a page: one range.
	Check(set.Record(0x21008, bytes.data(), 4), "record touching inside the page");
	// One read across a boundary stays one range.
	Check(set.Record(0x22ffc, bytes.data(), 8), "record across the next boundary");
	Check(set.Finish(), "page-boundary reads finish");
	const auto ranges = set.Ranges();
	Check(ranges.size() == 3 && ranges[0] == Coherence::Range {0x20ff8, 0x21000} &&
	          ranges[1] == Coherence::Range {0x21000, 0x2100c} &&
	          ranges[2] == Coherence::Range {0x22ffc, 0x23004},
	      "ranges split only at touching page boundaries");
	Check(set.RangeBytes(1).size() == 12 && set.RangeBytes(1)[8] == 1,
	      "bytes of a range merged inside a page");
}

void TestReadSetInconsistent() {
	DrawPrep::ReadSet set;
	const uint32_t    a = 0x11111111u;
	const uint32_t    b = 0x22222222u;
	Check(set.Record(0x2000, &a, 4), "record a");
	Check(set.Record(0x2000, &b, 4), "record b");
	Check(!set.Finish(), "two different values at one address do not finish");
	Check(set.Failure() == DrawPrep::ReadFailure::Inconsistent, "inconsistent failure reason");

	DrawPrep::ReadSet partial;
	const std::array<uint8_t, 8> wide {1, 2, 3, 4, 5, 6, 7, 8};
	const std::array<uint8_t, 2> narrow {4, 9};
	Check(partial.Record(0x3000, wide.data(), wide.size()), "record wide");
	Check(partial.Record(0x3003, narrow.data(), narrow.size()), "record narrow");
	Check(!partial.Finish(), "a partially overlapping read that disagrees fails");
}

void TestReadSetLimits() {
	DrawPrep::ReadSet set;
	std::vector<uint8_t> big(DrawPrep::ReadSet::MaxBytes);
	Check(set.Record(0x100000, big.data(), big.size()), "a read up to the byte limit fits");
	const uint8_t one = 1;
	Check(!set.Record(0x200000, &one, 1), "one byte over the limit is refused");
	Check(set.Failure() == DrawPrep::ReadFailure::Overflow, "overflow reason");
	Check(!set.Finish(), "a failed set does not finish");
	set.Reset();
	Check(!set.Failed() && set.ReadCount() == 0, "reset clears the failure");
	for (uint32_t i = 0; i < DrawPrep::ReadSet::MaxReads; i++) {
		Check(set.Record(0x1000 + uint64_t {i} * 16u, &one, 1), "read count within limit");
	}
	Check(!set.Record(0x900000, &one, 1), "one read over the count limit is refused");
	set.Reset();
	Check(set.Record(0x1000, &one, 0), "a zero-sized read is ignored");
	Check(set.Finish() && set.Ranges().empty(), "an empty set finishes with no ranges");
	std::vector<uint8_t> scratch;
	Check(set.Validate([](uint64_t, void*, uint64_t) { return false; }, scratch) ==
	          DrawPrep::ValidateResult::Ok,
	      "an empty set validates without reading");
}

void TestRecordScopeNests() {
	DrawPrep::ReadSet  outer_set;
	DrawPrep::ReadSet  inner_set;
	DrawPrep::Recorder outer {&outer_set, true};
	DrawPrep::Recorder inner {&inner_set, false};
	Check(!DrawPrep::Speculative(), "no recorder by default");
	{
		DrawPrep::RecordScope a(outer);
		Check(DrawPrep::ActiveRecorder() == &outer, "outer recorder active");
		{
			DrawPrep::RecordScope b(inner);
			Check(DrawPrep::ActiveRecorder() == &inner, "inner recorder active");
			DrawPrep::FailActive(DrawPrep::ReadFailure::Uncertified);
		}
		Check(DrawPrep::ActiveRecorder() == &outer, "outer recorder restored");
	}
	Check(!DrawPrep::Speculative(), "recorder cleared");
	Check(inner_set.Failure() == DrawPrep::ReadFailure::Uncertified && !outer_set.Failed(),
	      "FailActive marks only the active set");
	std::thread([] { Check(!DrawPrep::Speculative(), "recorders are per thread"); }).join();
}

// ---------------------------------------------------------------------------------------------
// Window (S6)

struct Item {
	uint64_t input  = 0;
	uint64_t output = 0;
	uint32_t prepared_by = 0; // 0 = producer, k = worker k
	uint32_t preparations = 0;
};

uint64_t Work(uint64_t value) {
	// Deterministic, a little expensive, so workers and the producer race for slots.
	uint64_t x = value * 0x9e3779b97f4a7c15ull + 1u;
	for (int i = 0; i < 64; i++) {
		x ^= x >> 29u;
		x *= 0xbf58476d1ce4e5b9ull;
	}
	return x;
}

void TestWindowSingleThread() {
	DrawPrep::Window<Item> window(3);
	Check(window.Capacity() == 4, "capacity rounds up to a power of two");
	Check(window.Empty() && !window.Full(), "new window is empty");
	for (uint64_t i = 0; i < 4; i++) {
		window.Reserve().input = i;
		window.Publish();
	}
	Check(window.Full() && window.Occupancy() == 4, "window fills to capacity");
	uint64_t seq  = 0;
	auto*    item = window.TryClaim(seq);
	Check(item != nullptr && seq == 0 && item->input == 0, "a worker claims the oldest slot");
	Check(!window.TryClaimHead(), "the producer cannot claim a worker's slot");
	Check(!window.HeadDone(), "a claimed slot is not done");
	window.Complete(seq);
	Check(window.HeadDone(), "a completed head is done");
	window.Retire();
	Check(window.TryClaimHead(), "the producer claims an unclaimed head");
	window.Retire();
	auto* third = window.TryClaim(seq);
	Check(third != nullptr && seq == 2, "workers skip positions the producer took");
	window.Complete(seq);
	Check(window.HeadDone(), "the third slot is done");
	window.Retire();
	Check(window.TryClaimHead(), "the last slot is claimable by the producer");
	window.Retire();
	Check(window.Empty(), "window drained");
	Check(window.TryClaim(seq) == nullptr, "nothing to claim in an empty window");
	// Reuse after wrap: the slot of position 4 is the slot of position 0.
	window.Reserve().input = 42;
	window.Publish();
	auto* wrapped = window.TryClaim(seq);
	Check(wrapped != nullptr && seq == 4 && wrapped->input == 42, "wrapped slot is claimable");
	window.Complete(seq);
	window.Retire();
}

void TestWindowConcurrent(uint32_t capacity, uint32_t workers, uint64_t items) {
	DrawPrep::Window<Item> window(capacity);
	std::atomic<bool>      stop {false};
	std::vector<std::thread> threads;
	for (uint32_t w = 0; w < workers; w++) {
		threads.emplace_back([&, w] {
			while (!stop.load(std::memory_order_acquire)) {
				uint64_t seq  = 0;
				auto*    item = window.TryClaim(seq);
				if (item == nullptr) {
					std::this_thread::yield();
					continue;
				}
				item->output      = Work(item->input);
				item->prepared_by = w + 1u;
				item->preparations++;
				window.Complete(seq);
			}
		});
	}
	uint64_t next_input   = 0;
	uint64_t next_retire  = 0;
	uint64_t self         = 0;
	uint64_t by_workers   = 0;
	bool     order_ok     = true;
	bool     result_ok    = true;
	const auto retire_head = [&] {
		auto& item = window.HeadPayload();
		if (window.TryClaimHead()) {
			item.output      = Work(item.input);
			item.prepared_by = 0;
			item.preparations++;
			self++;
		} else {
			while (!window.HeadDone()) {
				std::this_thread::yield();
			}
			by_workers++;
		}
		order_ok &= item.input == next_retire;
		result_ok &= item.output == Work(item.input) && item.preparations == 1;
		next_retire++;
		window.Retire();
	};
	while (next_input < items) {
		if (window.Full()) {
			retire_head();
		}
		auto& item        = window.Reserve();
		item.input        = next_input++;
		item.output       = 0;
		item.preparations = 0;
		window.Publish();
		// Occasionally drain like a fence does.
		if (next_input % 97u == 0u) {
			while (!window.Empty()) {
				retire_head();
			}
		}
	}
	while (!window.Empty()) {
		retire_head();
	}
	stop.store(true, std::memory_order_release);
	for (auto& thread: threads) {
		thread.join();
	}
	Check(order_ok, "items retire in publication order");
	Check(result_ok, "every item is prepared exactly once with the right result");
	Check(next_retire == items && self + by_workers == items, "every item retires once");
	// How many items the workers (rather than the producer) prepared depends on scheduling.
	std::printf("  window %u x %u workers: %llu of %llu prepared by workers\n", capacity, workers,
	            static_cast<unsigned long long>(by_workers), static_cast<unsigned long long>(items));
}

// ---------------------------------------------------------------------------------------------
// Packet classification (S0/S6)

void TestPacketClassification() {
	using DrawPrep::ClassifyPacket;
	using DrawPrep::PacketClass;
	namespace Pm4 = Libs::Graphics::Pm4;
	const std::array<uint32_t, 4> no_body {};
	const auto classify = [&](uint32_t len, uint32_t op, uint32_t r = 0,
	                          const uint32_t* body = nullptr) {
		return ClassifyPacket(KYTY_PM4(len, op, r), body != nullptr ? body : no_body.data(), len);
	};
	const std::array<uint32_t, 11> safe_ops {
	    Pm4::IT_SET_CONTEXT_REG, Pm4::IT_SET_SH_REG,        Pm4::IT_SET_UCONFIG_REG,
	    Pm4::IT_SET_UCONFIG_REG_INDEX, Pm4::IT_INDEX_TYPE,  Pm4::IT_INDEX_BASE,
	    Pm4::IT_INDEX_BUFFER_SIZE, Pm4::IT_NUM_INSTANCES,   Pm4::IT_SET_BASE,
	    Pm4::IT_CLEAR_STATE,       Pm4::IT_PFP_SYNC_ME};
	for (const auto op: safe_ops) {
		Check(classify(3, op) == PacketClass::WindowSafe, "register/state packet is window-safe");
	}
	const std::array<uint32_t, 4> draw_ops {Pm4::IT_DRAW_INDEX_2, Pm4::IT_DRAW_INDEX_OFFSET_2,
	                                        Pm4::IT_DRAW_INDEX_AUTO,
	                                        Pm4::IT_DISPATCH_DRAW_PREAMBLE};
	for (const auto op: draw_ops) {
		Check(classify(5, op) == PacketClass::Draw, "direct draw packet is a draw");
	}
	const std::array<uint32_t, 27> fence_ops {
	    Pm4::IT_SET_CONTEXT_REG_INDIRECT, Pm4::IT_SET_SH_REG_INDIRECT,
	    Pm4::IT_SET_UCONFIG_REG_INDIRECT, Pm4::IT_DRAW_INDIRECT,
	    Pm4::IT_DRAW_INDEX_INDIRECT,      Pm4::IT_DRAW_INDIRECT_MULTI,
	    Pm4::IT_DRAW_INDEX_INDIRECT_MULTI, Pm4::IT_DISPATCH_DIRECT,
	    Pm4::IT_DISPATCH_INDIRECT,        Pm4::IT_WRITE_DATA,
	    Pm4::IT_WAIT_REG_MEM,             Pm4::IT_WAIT_REG_MEM_64,
	    Pm4::IT_EVENT_WRITE,              Pm4::IT_EVENT_WRITE_EOP,
	    Pm4::IT_EVENT_WRITE_EOS,          Pm4::IT_DMA_DATA,
	    Pm4::IT_ACQUIRE_MEM,              Pm4::IT_COPY_DATA,
	    Pm4::IT_COND_EXEC,                Pm4::IT_SET_PREDICATION,
	    Pm4::IT_WRITE_CONST_RAM,          Pm4::IT_DUMP_CONST_RAM,
	    Pm4::IT_INCREMENT_CE_COUNTER,     Pm4::IT_INCREMENT_DE_COUNTER,
	    Pm4::IT_WAIT_ON_CE_COUNTER,       Pm4::IT_WAIT_ON_DE_COUNTER_DIFF,
	    Pm4::IT_GET_LOD_STATS};
	for (const auto op: fence_ops) {
		Check(classify(5, op) == PacketClass::Fence, "packet with side effects is a fence");
	}
	Check(classify(5, Pm4::IT_REWIND) == PacketClass::Fence, "rewind is a fence");
	// Every opcode outside the two short lists is a fence (unknown opcodes included).
	uint32_t non_fences = 0;
	for (uint32_t op = 0; op < 256; op++) {
		if (op == Pm4::IT_NOP || op == Pm4::IT_INDIRECT_BUFFER) {
			continue;
		}
		non_fences += classify(3, op) != PacketClass::Fence ? 1u : 0u;
	}
	Check(non_fences == safe_ops.size() + draw_ops.size(),
	      "only the listed opcodes avoid draining the window");
	// Indirect buffers: plain call/chain vs conditional branch.
	Check(classify(4, Pm4::IT_INDIRECT_BUFFER) == PacketClass::WindowSafe,
	      "indirect buffer call is window-safe");
	Check(classify(14, Pm4::IT_INDIRECT_BUFFER) == PacketClass::Fence,
	      "conditional indirect branch is a fence");
	// NOPs, markers and custom R codes.
	Check(classify(4, Pm4::IT_NOP) == PacketClass::WindowSafe, "plain NOP is window-safe");
	const std::array<uint32_t, 3> marker_safe {0x68750000u, 0x68750004u, 0x6875000du};
	for (const auto marker: marker_safe) {
		const std::array<uint32_t, 4> body {marker, 0, 0, 0};
		Check(classify(4, Pm4::IT_NOP, Pm4::R_ZERO, body.data()) == PacketClass::WindowSafe,
		      "user-data marker is window-safe");
	}
	const std::array<uint32_t, 3> marker_flip {0x68750777u, 0x68750778u, 0x68750781u};
	for (const auto marker: marker_flip) {
		const std::array<uint32_t, 4> body {marker, 0, 0, 0};
		Check(classify(4, Pm4::IT_NOP, Pm4::R_ZERO, body.data()) == PacketClass::Fence,
		      "flip marker is a fence");
	}
	for (const auto r: {Pm4::R_CONTEXT_STATE, Pm4::R_PUSH_MARKER, Pm4::R_POP_MARKER}) {
		Check(classify(3, Pm4::IT_NOP, r) == PacketClass::WindowSafe,
		      "context state and markers are window-safe");
	}
	for (const auto r: {Pm4::R_RELEASE_MEM, Pm4::R_FLIP, Pm4::R_ACQUIRE_MEM,
	                    Pm4::R_WAIT_FLIP_DONE, Pm4::R_DISPATCH_RESET}) {
		Check(classify(8, Pm4::IT_NOP, r) == PacketClass::Fence, "custom operation is a fence");
	}
}

} // namespace

int main() {
	TestLogEmptyIntervalIsClean();
	TestLogIntersection();
	TestLogEmptyRangeNeverIntersects();
	TestLogOverflow();
	TestLogConcurrentAppends();
	TestLogConcurrentWrap();
	TestIntersectsAny();
	TestReadSetCoalesces();
	TestReadSetPageBoundary();
	TestReadSetInconsistent();
	TestReadSetLimits();
	TestRecordScopeNests();
	TestPacketClassification();
	TestWindowSingleThread();
	TestWindowConcurrent(4, 8, 200000);  // tiny window: constant wrap-around and races
	TestWindowConcurrent(32, 6, 200000); // the default shape
	TestWindowConcurrent(32, 1, 50000);
	if (g_failures != 0) {
		std::fprintf(stderr, "DrawPrepTests: %d failure(s)\n", g_failures);
		return 1;
	}
	std::puts("DrawPrepTests: all cases passed");
	return 0;
}
