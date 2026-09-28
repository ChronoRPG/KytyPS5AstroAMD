#include "common/profiler.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

// Per-thread profiler counters (common/profiler.h): totals summed over every thread, blocks reused
// after a thread exits with their totals kept, and a timing comparison of the counting paths
// (KYTY_PROFILE_COUNTERS=shared restores the previous one).

namespace {

using Profiler::FrameEvent;
using Profiler::Detail::CounterSink;

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "ProfilerCounterTests: failed: %s\n", text);
		std::abort();
	}
}

void SetSink(CounterSink sink) {
	Profiler::Detail::g_event_sink.store(sink, std::memory_order_relaxed);
}

// Counts from several threads at once reach the total exactly once each.
void TestTotalsAcrossThreads() {
	SetSink(CounterSink::Thread);
	constexpr uint32_t threads = 8;
	constexpr uint64_t calls   = 20000;
	const auto         before  = Profiler::FrameEventTotal(FrameEvent::MeshDraws);
	std::vector<std::thread> workers;
	for (uint32_t t = 0; t < threads; t++) {
		workers.emplace_back([] {
			for (uint64_t i = 0; i < calls; i++) {
				Profiler::CountFrameEvent(FrameEvent::MeshDraws);
			}
			Profiler::CountFrameEvent(FrameEvent::MeshDraws, 5);
		});
	}
	for (auto& worker: workers) {
		worker.join();
	}
	Check(Profiler::FrameEventTotal(FrameEvent::MeshDraws) == before + threads * (calls + 5),
	      "counts from several threads were lost or duplicated");
	// Off: nothing is counted.
	SetSink(CounterSink::Off);
	const auto off = Profiler::FrameEventTotal(FrameEvent::MeshDraws);
	Profiler::CountFrameEvent(FrameEvent::MeshDraws, 7);
	Check(Profiler::FrameEventTotal(FrameEvent::MeshDraws) == off, "an Off sink counted");
	std::puts("ProfilerCounterTests: totals across threads ok");
}

// A thread's block is released when it exits and reused by the next new thread, which continues
// its totals: the published cumulative values never go down, and threads that come and go do not
// grow the registry.
void TestBlockReuse() {
	SetSink(CounterSink::Thread);
	const auto run = [](uint64_t amount) {
		std::thread([amount] { Profiler::CountFrameEvent(FrameEvent::MeshWorkgroups, amount); })
		    .join();
	};
	run(1); // registers (or reuses) a block
	const auto blocks = Profiler::Detail::CounterBlockCount();
	const auto before = Profiler::FrameEventTotal(FrameEvent::MeshWorkgroups);
	for (uint64_t i = 0; i < 100; i++) {
		run(3);
	}
	Check(Profiler::Detail::CounterBlockCount() == blocks,
	      "threads created one after another did not reuse released blocks");
	Check(Profiler::FrameEventTotal(FrameEvent::MeshWorkgroups) == before + 300,
	      "a reused block lost the totals of the threads before it");
	SetSink(CounterSink::Off);
	std::puts("ProfilerCounterTests: block reuse ok");
}

// ScopedFrameWait with the per-thread sink on and no Tracy profiler (this process never starts
// one; TRACY_MANUAL_LIFETIME): the scope is counted in the thread's block without touching the
// profiler, whose GetProfiler() would dereference none. The shared sink without a profiler counts
// nothing and must not touch it either.
void TestFrameWaitWithoutProfiler() {
	Check(!tracy::ProfilerAvailable(), "the test process has a Tracy profiler");
	using Profiler::FrameWait;
	constexpr auto kind  = FrameWait::ReadMemory;
	constexpr auto index = static_cast<size_t>(kind);
	const auto     wait_calls = [] {
		return Profiler::Detail::CurrentThreadCounters().wait_calls[index].load();
	};
	const auto wait_ns = [] {
		return Profiler::Detail::CurrentThreadCounters().wait_ns[index].load();
	};
	SetSink(CounterSink::Thread);
	const auto calls = wait_calls();
	const auto ns    = wait_ns();
	{
		Profiler::ScopedFrameWait wait(kind);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	Check(wait_calls() == calls + 1, "a frame wait with the per-thread sink and no profiler was lost");
	Check(wait_ns() - ns >= 1000000, "a frame wait with the per-thread sink lost its duration");
	// Another thread, with its own block.
	uint64_t other_calls = 0;
	std::thread([&] {
		const auto before = Profiler::Detail::CurrentThreadCounters().wait_calls[index].load();
		{ Profiler::ScopedFrameWait wait(kind); }
		other_calls = Profiler::Detail::CurrentThreadCounters().wait_calls[index].load() - before;
	}).join();
	Check(other_calls == 1, "a frame wait on another thread was lost");
	// Externally measured totals take the same sink.
	Profiler::AddFrameWait(kind, 2, 1000);
	Check(wait_calls() == calls + 3, "AddFrameWait with the per-thread sink was lost");
	// Shared sink, no profiler: not counted (and no profiler access).
	SetSink(CounterSink::Shared);
	{
		Profiler::ScopedFrameWait wait(kind);
	}
	Check(wait_calls() == calls + 3, "a shared-sink frame wait without a profiler was counted");
	SetSink(CounterSink::Off);
	{
		Profiler::ScopedFrameWait wait(kind);
	}
	Check(wait_calls() == calls + 3, "an Off sink counted a frame wait");
	std::puts("ProfilerCounterTests: frame waits without a profiler ok");
}

// Timing: one "command processor" thread makes a flip's worth of counts (about 200k calls in the
// Sky Garden start view, DEEP-TRACE-U52) while six "workers" count too, through
//  - the per-thread path (a profiler connected),
//  - the Off path (not connected, or aggregates off: one inlined flag check),
//  - the previous path without a connection (an out-of-line call, the aggregate switch and the
//    profiler check; the test process has no Tracy profiler, so the connection check is not
//    reached), and
//  - the shared atomics alone, as the previous path with a connection adds them (every thread
//    adding to one array; that path also made its checks first).
// Printed only: timings depend on the machine and its load.
void BenchmarkCounting() {
	// Aggregate diagnostics on, as in the profiled runs (the previous path checks them).
	Profiler::Detail::g_frames_only.store(1, std::memory_order_relaxed);
	Profiler::Detail::g_aggregate.store(1, std::memory_order_relaxed);
	constexpr uint64_t calls   = 200000;
	constexpr uint32_t workers = 6;
	constexpr uint32_t rounds  = 5;
	static std::array<std::atomic<uint64_t>, Profiler::Detail::kFrameEventCount> shared {};
	const auto shared_add = [](size_t index) {
		shared[index].fetch_add(1, std::memory_order_relaxed);
	};
	// Round-robin over a few neighbouring events, as the draw path does.
	constexpr std::array<FrameEvent, 4> kinds {FrameEvent::TextureBindingMemoHits,
	                                           FrameEvent::TextureViewMemoHits,
	                                           FrameEvent::TargetDescMemoHits,
	                                           FrameEvent::BindingEpochMemoCachedHits};
	enum class Path { Thread, Off, SharedDisconnected, SharedAtomics };
	const auto measure = [&](Path path) {
		switch (path) {
			case Path::Thread: SetSink(CounterSink::Thread); break;
			case Path::Off: SetSink(CounterSink::Off); break;
			case Path::SharedDisconnected: SetSink(CounterSink::Shared); break;
			case Path::SharedAtomics: SetSink(CounterSink::Off); break;
		}
		const auto count = [&](uint64_t i) {
			const auto kind = kinds[i & 3u];
			if (path == Path::SharedAtomics) {
				shared_add(static_cast<size_t>(kind));
			} else {
				Profiler::CountFrameEvent(kind);
			}
		};
		double best_ns = 1e30;
		for (uint32_t round = 0; round < rounds; round++) {
			std::atomic<bool> stop {false};
			std::vector<std::thread> threads;
			for (uint32_t w = 0; w < workers; w++) {
				threads.emplace_back([&] {
					uint64_t i = 0;
					while (!stop.load(std::memory_order_relaxed)) {
						count(i++);
					}
				});
			}
			const auto start = std::chrono::steady_clock::now();
			for (uint64_t i = 0; i < calls; i++) {
				count(i);
			}
			const auto elapsed = std::chrono::duration<double, std::nano>(
			                         std::chrono::steady_clock::now() - start)
			                         .count();
			stop.store(true, std::memory_order_relaxed);
			for (auto& thread: threads) {
				thread.join();
			}
			best_ns = std::min(best_ns, elapsed);
		}
		SetSink(CounterSink::Off);
		return best_ns;
	};
	const auto report = [&](const char* name, double ns) {
		std::printf("ProfilerCounterTests: %-40s %6.2f ns/call, %6.3f ms per %llu calls\n", name,
		            ns / calls, ns / 1e6, static_cast<unsigned long long>(calls));
	};
	report("per-thread (connected)", measure(Path::Thread));
	report("off (inlined flag)", measure(Path::Off));
	report("previous, not connected", measure(Path::SharedDisconnected));
	report("previous, connected (shared atomics)", measure(Path::SharedAtomics));
}

} // namespace

int main(int argc, char** argv) {
	TestTotalsAcrossThreads();
	TestBlockReuse();
	TestFrameWaitWithoutProfiler();
	if (argc < 2 || std::strcmp(argv[1], "--no-benchmark") != 0) {
		BenchmarkCounting();
	}
	std::puts("ProfilerCounterTests: all cases passed");
	return 0;
}
