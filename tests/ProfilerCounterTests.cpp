#include "common/hangTrace.h"
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


// HangTrace::NoteGpuWrite skips a note identical to one of its thread's recent notes in the same
// millisecond while nothing changed the page map since: the map must end exactly as if every note
// had been applied (kind and size per page; the time is the same millisecond either way).
void TestNoteGpuWriteRepeats() {
	using HangTrace::GpuWriteKind;
	constexpr uint64_t base = 0x7f0000000ull;
	const auto check = [](uint64_t vaddr, GpuWriteKind kind, uint64_t size, const char* what) {
		const auto writer = HangTrace::PageWriterForTest(vaddr);
		Check(writer.found && writer.kind == kind && writer.size == size, what);
	};
	for (int round = 0; round < 50; round++) {
		const uint64_t a = base + static_cast<uint64_t>(round) * 0x10000u;
		// A two-page write, repeated: the map keeps it.
		HangTrace::NoteGpuWrite(a, 0x2000);
		HangTrace::NoteGpuWrite(a, 0x2000);
		check(a + 0x1000, GpuWriteKind::ShaderStorage, 0x2000, "repeat");
		// An overlapping write of another kind by this thread, then the first write again: the
		// repeat must not be skipped (its second page changed).
		{
			const HangTrace::ScopedGpuWriteKind kind(GpuWriteKind::Copy);
			HangTrace::NoteGpuWrite(a + 0x1000, 0x1000);
		}
		check(a + 0x1000, GpuWriteKind::Copy, 0x1000, "overlap");
		HangTrace::NoteGpuWrite(a, 0x2000);
		check(a + 0x1000, GpuWriteKind::ShaderStorage, 0x2000, "repeat after an overlap");
		// Another thread writes the page; the repeat here must not be skipped either.
		std::thread([a] {
			const HangTrace::ScopedGpuWriteKind kind(GpuWriteKind::Fill);
			HangTrace::NoteGpuWrite(a + 0x1000, 0x1000);
		}).join();
		check(a + 0x1000, GpuWriteKind::Fill, 0x1000, "other thread");
		HangTrace::NoteGpuWrite(a, 0x2000);
		check(a, GpuWriteKind::ShaderStorage, 0x2000, "repeat after another thread (first page)");
		check(a + 0x1000, GpuWriteKind::ShaderStorage, 0x2000,
		      "repeat after another thread (second page)");
		// A large binding notes its first and last pages only; repeated, the same.
		HangTrace::NoteGpuWrite(a + 0x100000, 0x100000);
		HangTrace::NoteGpuWrite(a + 0x100000, 0x100000);
		check(a + 0x100000, GpuWriteKind::ShaderStorage, 0x100000, "large binding, first page");
		check(a + 0x1ff000, GpuWriteKind::ShaderStorage, 0x100000, "large binding, last page");
	}
	std::puts("ProfilerCounterTests: NoteGpuWrite repeats ok");
}


// Residual instrumentation on the command processor (DEEP-TRACE-U54 4.1 item 7), per call on one
// thread, in noinline functions like the renderer's call sites:
//  - a counted event through the per-thread sink (a profiler connected);
//  - a profiler block with zones off (every KYTY_PROFILER_BLOCK scope in frame-only runs);
//  - HangTrace::RecordTexture (every resolved texture, ~50k per flip at the start view; 1% carry
//    mip statistics);
//  - HangTrace::NoteGpuWrite over eight written bindings noted again and again (every draw's
//    writable bindings), and over fresh ranges.
// Printed only: timings depend on the machine and its load.
volatile uint64_t g_bench_sink = 0;

[[gnu::noinline]] void BenchCountEvent(uint64_t i) {
	Profiler::CountFrameEvent((i & 1u) != 0 ? FrameEvent::TextureBindingMemoHits
	                                        : FrameEvent::TextureViewMemoHits);
}
[[gnu::noinline]] void BenchBlock(uint64_t i) {
	KYTY_PROFILER_BLOCK("InstrumentationBench");
	g_bench_sink = g_bench_sink + i;
}
[[gnu::noinline]] void BenchRecordTexture(const uint32_t* fields) {
	HangTrace::RecordTexture(fields);
}
// The renderer's call sites store to memory around the record; a locked instruction then waits
// for those stores (the store buffer drains).
std::array<uint64_t, 1024> g_bench_stores {};
[[gnu::noinline]] void BenchRecordTextureAfterStores(uint64_t i, const uint32_t* fields) {
	for (uint64_t k = 0; k < 8; k++) {
		g_bench_stores[(i * 8u + k) & 1023u] = i + k;
	}
	HangTrace::RecordTexture(fields);
}
[[gnu::noinline]] void BenchNoteGpuWrite(uint64_t vaddr, uint64_t size) {
	HangTrace::NoteGpuWrite(vaddr, size);
}

void BenchmarkInstrumentation() {
	constexpr uint64_t calls  = 400000;
	constexpr uint32_t rounds = 7;
	const auto best = [&](auto&& body) {
		double best_ns = 1e30;
		for (uint32_t round = 0; round < rounds; round++) {
			const auto start = std::chrono::steady_clock::now();
			for (uint64_t i = 0; i < calls; i++) {
				body(i);
			}
			best_ns = std::min(best_ns, std::chrono::duration<double, std::nano>(
			                                std::chrono::steady_clock::now() - start)
			                                .count());
		}
		return best_ns / calls;
	};
	const auto report = [](const char* name, double ns) {
		std::printf("ProfilerCounterTests: %-40s %6.2f ns/call\n", name, ns);
	};
	SetSink(CounterSink::Thread);
	report("instrumentation: counted event", best([](uint64_t i) { BenchCountEvent(i); }));
	SetSink(CounterSink::Off);
	report("instrumentation: block, zones off", best([](uint64_t i) { BenchBlock(i); }));
	// Texture descriptors: 1 in 100 with the mip-statistics bit (fields[5] bit 25).
	std::array<uint32_t, 8> plain {0x10000u, 0x2u, 0, 0x00f0f000u, 0, 0, 0x7u, 0};
	std::array<uint32_t, 8> mips = plain;
	mips[5] |= 1u << 25u;
	report("instrumentation: RecordTexture", best([&](uint64_t i) {
		       BenchRecordTexture(i % 100u == 0 ? mips.data() : plain.data());
	       }));
	report("instrumentation: RecordTexture, 8 stores", best([&](uint64_t i) {
		       BenchRecordTextureAfterStores(i, i % 100u == 0 ? mips.data() : plain.data());
	       }));
	// Eight bindings of 4 KiB to 64 KiB written again and again.
	report("instrumentation: NoteGpuWrite, repeated", best([](uint64_t i) {
		       const uint64_t slot = i & 7u;
		       BenchNoteGpuWrite(0x200000000ull + slot * 0x100000ull, 0x1000ull << (slot & 4u));
	       }));
	report("instrumentation: NoteGpuWrite, fresh", best([](uint64_t i) {
		       BenchNoteGpuWrite(0x300000000ull + (i % 200000u) * 0x2000ull, 0x1000);
	       }));
}
} // namespace

int main(int argc, char** argv) {
	// HangTrace::Enabled (read once), for the instrumentation timing; nothing here writes files.
#if defined(_WIN32)
	(void)_putenv_s("KYTY_HANG_TRACE", "1");
#else
	(void)setenv("KYTY_HANG_TRACE", "1", 1);
#endif
	TestTotalsAcrossThreads();
	TestBlockReuse();
	TestFrameWaitWithoutProfiler();
	TestNoteGpuWriteRepeats();
	if (argc < 2 || std::strcmp(argv[1], "--no-benchmark") != 0) {
		BenchmarkCounting();
		BenchmarkInstrumentation();
	}
	std::puts("ProfilerCounterTests: all cases passed");
	return 0;
}
