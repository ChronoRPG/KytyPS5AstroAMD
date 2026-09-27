// Kernel event queue delivery semantics (KYTY_EQUEUE_COALESCE): kqueue-style pending state,
// exact coalescing of repeated triggers, EV_CLEAR / EV_ONESHOT / level-triggered behaviour and
// activation-order delivery. `--hitch-bench` also times the legacy unbounded queue against
// coalescing (the 135 ms std::deque growth hitch of DEEP-TRACE-U52 section 5).
#include "kernel/eventQueue.h"
#include "kernel/eventQueueFilters.h"
#include "libs/errno.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

namespace EventQueue = Libs::LibKernel::EventQueue;
using Libs::LibKernel::KERNEL_ERROR_ENOENT;
using Libs::LibKernel::KERNEL_ERROR_ETIMEDOUT;

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "EventQueueSemanticsTests: failed: %s\n", text);
		std::abort();
	}
}

void SetMode(EventQueue::EqueueCoalesceMode mode) {
	EventQueue::KernelEqueueSetCoalesceModeForTests(mode);
}

EventQueue::KernelEqueue CreateQueue(const char* name) {
	EventQueue::KernelEqueue queue = EventQueue::KERNEL_EQUEUE_INVALID;
	Check(EventQueue::KernelCreateEqueue(&queue, name) == OK, "create queue");
	return queue;
}

// Non-blocking poll (timeout 0): the number of delivered events, 0 on ETIMEDOUT.
int Poll(EventQueue::KernelEqueue queue, EventQueue::KernelEvent* events, int num) {
	int                             out     = 0;
	Libs::LibKernel::KernelUseconds timeout = 0;
	const int result = EventQueue::KernelWaitEqueue(queue, events, num, &out, &timeout);
	Check(result == OK || result == KERNEL_ERROR_ETIMEDOUT, "poll result");
	return result == OK ? out : 0;
}

// The largest legacy backlog seen by the trigger functions below (bounded coalescing keeps it 0).
std::atomic<size_t> g_max_pending {0};

void NotePending(const EventQueue::KernelEqueueEvent* event) {
	const auto size = event->pending_events.size();
	auto       seen = g_max_pending.load(std::memory_order_relaxed);
	while (seen < size && !g_max_pending.compare_exchange_weak(seen, size)) {
	}
}

// Mirrors sync.cpp's EVFILT_GRAPHICS end-of-pipe interrupt filter.
void GraphicsTrigger(EventQueue::KernelEqueueEvent* event, void* trigger_data) {
	EventQueue::KernelEqueueApplyTrigger(
	    event, EventQueue::GraphicsInterruptNextState(
	               event->event, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(trigger_data))));
	NotePending(event);
}

void GraphicsReset(EventQueue::KernelEqueueEvent* event) {
	event->triggered    = false;
	event->event.fflags = 0;
	event->event.data   = 0;
}

void AddGraphicsEvent(EventQueue::KernelEqueue queue, int id) {
	EventQueue::KernelEqueueEvent event;
	event.event.ident         = static_cast<uintptr_t>(id);
	event.event.filter        = EventQueue::KERNEL_EVFILT_GRAPHICS;
	event.event.data          = id;
	event.filter.reset_func   = GraphicsReset;
	event.filter.trigger_func = GraphicsTrigger;
	Check(EventQueue::KernelAddEvent(queue, event) == OK, "add graphics event");
}

void TriggerGraphics(EventQueue::KernelEqueue queue, int id, uint32_t context_id) {
	Check(EventQueue::KernelTriggerEvent(queue, static_cast<uintptr_t>(id),
	                                     EventQueue::KERNEL_EVFILT_GRAPHICS,
	                                     reinterpret_cast<void*>(uintptr_t {context_id})) == OK,
	      "trigger graphics event");
}

// Mirrors videoOut.cpp's EVFILT_VIDEO_OUT filter with a fixed clock.
constexpr uint64_t VIDEO_TSC = 0xabc;

void VideoTrigger(EventQueue::KernelEqueueEvent* event, void* trigger_data) {
	EventQueue::KernelEqueueApplyTrigger(
	    event, EventQueue::VideoOutNextState(
	               event->event, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(trigger_data)),
	               VIDEO_TSC));
	NotePending(event);
}

// 1. Unconsumed interrupts coalesce into one pending event whose fflags counts them exactly and
//    whose data is the newest context id. Nothing is queued; the next poll finds nothing.
void TestGraphicsCoalescing() {
	SetMode(EventQueue::EqueueCoalesceMode::Coalesce);
	g_max_pending    = 0;
	const auto queue = CreateQueue("graphics-coalesce");
	AddGraphicsEvent(queue, 0);
	const auto before = EventQueue::KernelEqueueGetCoalesceStats();

	constexpr uint32_t TRIGGERS = 100000;
	for (uint32_t i = 0; i < TRIGGERS; i++) {
		TriggerGraphics(queue, 0, i % 7);
	}
	EventQueue::KernelEvent events[8] {};
	Check(Poll(queue, events, 8) == 1, "coalesced interrupts are delivered once");
	Check(events[0].fflags == TRIGGERS, "fflags counts every coalesced interrupt");
	Check(events[0].data == static_cast<intptr_t>((TRIGGERS - 1) % 7),
	      "data is the newest context id");
	Check(events[0].ident == 0 && events[0].filter == EventQueue::KERNEL_EVFILT_GRAPHICS,
	      "ident and filter kept");
	Check(Poll(queue, events, 8) == 0, "a delivered edge event is not pending anymore");
	Check(g_max_pending.load() == 0, "no per-trigger queue while coalescing");

	const auto after = EventQueue::KernelEqueueGetCoalesceStats();
	Check(after.coalesced_triggers - before.coalesced_triggers == TRIGGERS - 1,
	      "coalesced trigger count");
	Check(after.merged_deliveries - before.merged_deliveries == 1, "one merged delivery");
	Check(after.max_merged >= TRIGGERS - 1, "max merged");

	// A trigger after the delivery starts a new count.
	TriggerGraphics(queue, 0, 5);
	Check(Poll(queue, events, 8) == 1 && events[0].fflags == 1 && events[0].data == 5,
	      "counting restarts after delivery");
	Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
}

// 2. KYTY_EQUEUE_COALESCE=0 keeps the old behaviour exactly: one queued copy per trigger, all
//    delivered by one wait, fflags derived from the delivered state (1, then 2 for each copy).
void TestLegacyQueue() {
	SetMode(EventQueue::EqueueCoalesceMode::Legacy);
	g_max_pending    = 0;
	const auto queue = CreateQueue("graphics-legacy");
	AddGraphicsEvent(queue, 0);
	for (uint32_t i = 0; i < 5; i++) {
		TriggerGraphics(queue, 0, 10 + i);
	}
	Check(g_max_pending.load() == 4, "legacy queues one copy per extra trigger");
	EventQueue::KernelEvent events[8] {};
	Check(Poll(queue, events, 8) == 5, "legacy delivers every copy");
	for (int i = 0; i < 5; i++) {
		Check(events[i].data == 10 + i, "legacy data order");
		Check(events[i].fflags == (i == 0 ? 1u : 2u), "legacy fflags");
	}
	Check(Poll(queue, events, 8) == 0, "legacy queue drained");
	Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
	SetMode(EventQueue::EqueueCoalesceMode::Coalesce);
}

// 3. Exact under concurrency: a producer triggers N interrupts while a consumer waits; the sum of
//    the delivered fflags equals N (no interrupt is lost, however they were merged).
void TestExactCountUnderConcurrency() {
	SetMode(EventQueue::EqueueCoalesceMode::Coalesce);
	const auto queue = CreateQueue("graphics-concurrent");
	AddGraphicsEvent(queue, 0);
	constexpr uint64_t TRIGGERS = 1000000;
	std::atomic<bool>  done {false};
	uint64_t           counted    = 0;
	uint64_t           deliveries = 0;
	std::thread        consumer([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (counted < TRIGGERS && std::chrono::steady_clock::now() < deadline) {
            EventQueue::KernelEvent         events[8] {};
            int                             out     = 0;
            Libs::LibKernel::KernelUseconds timeout = 1000;
            const int result = EventQueue::KernelWaitEqueue(queue, events, 8, &out, &timeout);
            if (result == OK) {
                for (int i = 0; i < out; i++) {
                    counted += events[i].fflags;
                    deliveries++;
                }
            }
        }
        done = true;
	});
	for (uint64_t i = 0; i < TRIGGERS; i++) {
		TriggerGraphics(queue, 0, static_cast<uint32_t>(i));
	}
	consumer.join();
	Check(done.load(), "consumer finished");
	Check(counted == TRIGGERS, "every interrupt is counted exactly once");
	Check(deliveries <= TRIGGERS, "no more deliveries than interrupts");
	std::printf("  concurrent: %" PRIu64 " interrupts delivered in %" PRIu64
	            " events (%.1f per event)\n",
	            TRIGGERS, deliveries,
	            static_cast<double>(TRIGGERS) / static_cast<double>(std::max<uint64_t>(deliveries, 1)));
	Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
}

// 4. EV_CLEAR user events (sceKernelAddUserEventEdge): repeated triggers are one delivery with
//    the newest data; after it the event is clear.
void TestEdgeUserEvent() {
	const auto queue = CreateQueue("user-edge");
	Check(EventQueue::KernelAddUserEventEdge(queue, 3) == OK, "add edge user event");
	for (uintptr_t i = 1; i <= 3; i++) {
		Check(EventQueue::KernelTriggerUserEvent(queue, 3, reinterpret_cast<void*>(0x100 * i)) ==
		          OK,
		      "trigger edge user event");
	}
	EventQueue::KernelEvent events[8] {};
	Check(Poll(queue, events, 8) == 1, "edge user event delivered once");
	Check(events[0].data == 0x300 && events[0].udata == reinterpret_cast<void*>(0x300),
	      "edge user event keeps the newest data");
	Check(Poll(queue, events, 8) == 0, "edge user event cleared");
	Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
}

// 5. Level-triggered user events (sceKernelAddUserEvent, no EV_CLEAR) stay pending, but each
//    wait reports them once (kqueue_scan's marker), not once per output slot.
void TestLevelUserEvent() {
	const auto queue = CreateQueue("user-level");
	Check(EventQueue::KernelAddUserEvent(queue, 4) == OK, "add level user event");
	Check(EventQueue::KernelTriggerUserEvent(queue, 4, reinterpret_cast<void*>(0x44)) == OK,
	      "trigger level user event");
	EventQueue::KernelEvent events[8] {};
	Check(Poll(queue, events, 8) == 1, "level event reported once per wait");
	Check(events[0].data == 0x44, "level event data");
	Check(Poll(queue, events, 8) == 1, "level event stays pending");
	Check(EventQueue::KernelDeleteUserEvent(queue, 4) == OK, "delete level event");
	Check(Poll(queue, events, 8) == 0, "deleted level event is gone");
	Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
}

// 6. EV_ONESHOT (sceKernelAddHRTimerEvent): delivered once, then removed.
void TestOneShotTimer() {
	const auto queue = CreateQueue("oneshot");
	Libs::LibKernel::KernelTimespec ts {};
	ts.tv_sec  = 0;
	ts.tv_nsec = 0;
	Check(EventQueue::KernelAddHRTimerEvent(queue, 9, &ts, reinterpret_cast<void*>(0x99)) == OK,
	      "add oneshot timer");
	EventQueue::KernelEvent events[8] {};
	Check(Poll(queue, events, 8) == 1 && events[0].ident == 9 &&
	          events[0].filter == EventQueue::KERNEL_EVFILT_HRTIMER &&
	          events[0].udata == reinterpret_cast<void*>(0x99),
	      "oneshot timer fires once");
	Check(Poll(queue, events, 8) == 0, "oneshot timer not pending again");
	Check(EventQueue::KernelDeleteHRTimerEvent(queue, 9) == KERNEL_ERROR_ENOENT,
	      "oneshot timer removed after delivery");
	Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
}

// 7. Delivery follows activation order, not registration order. A coalesced trigger keeps the
//    event's place; a limited wait leaves the rest for the next one.
void TestActivationOrder() {
	const auto queue = CreateQueue("order");
	for (int id = 1; id <= 3; id++) {
		Check(EventQueue::KernelAddUserEventEdge(queue, id) == OK, "add ordered event");
	}
	auto trigger = [&](int id) {
		Check(EventQueue::KernelTriggerUserEvent(queue, id,
		                                         reinterpret_cast<void*>(uintptr_t {0x10u + id})) ==
		          OK,
		      "trigger ordered event");
	};
	trigger(2);
	trigger(3);
	trigger(1);
	trigger(2); // coalesces; 2 stays first
	EventQueue::KernelEvent events[8] {};
	Check(Poll(queue, events, 2) == 2 && events[0].ident == 2 && events[1].ident == 3,
	      "first two in activation order");
	Check(Poll(queue, events, 8) == 1 && events[0].ident == 1, "the rest on the next wait");

	// A level event re-queues behind the others after each delivery.
	Check(EventQueue::KernelAddUserEvent(queue, 7) == OK, "add level event");
	Check(EventQueue::KernelTriggerUserEvent(queue, 7, nullptr) == OK, "trigger level event");
	trigger(1);
	trigger(2);
	Check(Poll(queue, events, 1) == 1 && events[0].ident == 7, "level first");
	Check(Poll(queue, events, 1) == 1 && events[0].ident == 1, "then edge 1");
	Check(Poll(queue, events, 1) == 1 && events[0].ident == 2, "then edge 2");
	Check(Poll(queue, events, 8) == 1 && events[0].ident == 7, "level again, once per wait");
	Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
}

// 8. Video-out events: the 4-bit counter (sceVideoOutGetEventCount) and fflags count the
//    coalesced occurrences and saturate at 15; the payload is the newest.
void TestVideoOutCounter() {
	SetMode(EventQueue::EqueueCoalesceMode::Coalesce);
	const auto                    queue = CreateQueue("video-out");
	EventQueue::KernelEqueueEvent event;
	event.event.ident         = 0; // flip
	event.event.filter        = EventQueue::KERNEL_EVFILT_VIDEO_OUT;
	event.filter.reset_func   = GraphicsReset;
	event.filter.trigger_func = VideoTrigger;
	Check(EventQueue::KernelAddEvent(queue, event) == OK, "add video-out event");
	auto trigger = [&](uint64_t payload) {
		Check(EventQueue::KernelTriggerEvent(queue, 0, EventQueue::KERNEL_EVFILT_VIDEO_OUT,
		                                     reinterpret_cast<void*>(uintptr_t {payload})) == OK,
		      "trigger video-out event");
	};
	trigger(10);
	trigger(11);
	trigger(12);
	EventQueue::KernelEvent events[8] {};
	Check(Poll(queue, events, 8) == 1, "video-out occurrences coalesce");
	auto data = static_cast<uint64_t>(events[0].data);
	Check(((data >> 12u) & 0xfu) == 3 && events[0].fflags == 3, "counter counts occurrences");
	Check((data >> 16u) == 12 && (data & 0xfffu) == VIDEO_TSC, "newest payload and time");
	for (uint64_t i = 0; i < 40; i++) {
		trigger(100 + i);
	}
	Check(Poll(queue, events, 8) == 1, "saturating burst delivered once");
	data = static_cast<uint64_t>(events[0].data);
	Check(((data >> 12u) & 0xfu) == 15 && events[0].fflags == 15, "counter saturates at 15");
	Check((data >> 16u) == 139, "payload of the last occurrence");
	Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
}

// 9. Verify mode coalesces the same way and records data changes.
void TestVerifyModeStats() {
	SetMode(EventQueue::EqueueCoalesceMode::Verify);
	const auto queue = CreateQueue("verify");
	AddGraphicsEvent(queue, 0x40);
	const auto before = EventQueue::KernelEqueueGetCoalesceStats();
	TriggerGraphics(queue, 0x40, 1);
	TriggerGraphics(queue, 0x40, 1);
	TriggerGraphics(queue, 0x40, 2);
	EventQueue::KernelEvent events[8] {};
	Check(Poll(queue, events, 8) == 1 && events[0].fflags == 3 && events[0].data == 2,
	      "verify mode coalesces");
	const auto after = EventQueue::KernelEqueueGetCoalesceStats();
	Check(after.coalesced_triggers - before.coalesced_triggers == 2, "verify coalesced count");
	Check(after.data_changes - before.data_changes == 1, "verify counts data changes");
	// An unconsumed event is reported while it accumulates (at 1024 and 2048 merged triggers).
	for (uint32_t i = 0; i < 2100; i++) {
		TriggerGraphics(queue, 0x40, 7);
	}
	Check(Poll(queue, events, 8) == 1 && events[0].fflags == 2100, "verify accumulation counted");
	Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
	SetMode(EventQueue::EqueueCoalesceMode::Coalesce);
}

// --hitch-bench: the cost of N unconsumed interrupts, legacy queue against coalescing. The
// legacy deque doubles its map as it grows; each doubling is one long trigger (the U52 hitch).
void HitchBench(uint64_t triggers) {
	for (const auto mode :
	     {EventQueue::EqueueCoalesceMode::Legacy, EventQueue::EqueueCoalesceMode::Coalesce}) {
		SetMode(mode);
		const auto queue = CreateQueue("hitch-bench");
		AddGraphicsEvent(queue, 0);
		uint64_t   worst_ns = 0;
		uint64_t   slow     = 0;
		const auto start    = std::chrono::steady_clock::now();
		for (uint64_t i = 0; i < triggers; i++) {
			const auto t0 = std::chrono::steady_clock::now();
			TriggerGraphics(queue, 0, static_cast<uint32_t>(i));
			const auto ns = static_cast<uint64_t>(
			    std::chrono::duration_cast<std::chrono::nanoseconds>(
			        std::chrono::steady_clock::now() - t0)
			        .count());
			worst_ns = std::max(worst_ns, ns);
			slow += ns >= 1000000 ? 1 : 0;
		}
		const auto total_ms = std::chrono::duration<double, std::milli>(
		                          std::chrono::steady_clock::now() - start)
		                          .count();
		std::printf("  hitch-bench %-8s: %" PRIu64 " triggers, %.1f ms total, worst trigger "
		            "%.3f ms, %" PRIu64 " triggers >= 1 ms\n",
		            mode == EventQueue::EqueueCoalesceMode::Legacy ? "legacy" : "coalesce",
		            triggers, total_ms, static_cast<double>(worst_ns) / 1e6, slow);
		const auto drain_start = std::chrono::steady_clock::now();
		Check(EventQueue::KernelDeleteEqueue(queue) == OK, "delete queue");
		std::printf("  hitch-bench %-8s: queue deleted in %.1f ms\n",
		            mode == EventQueue::EqueueCoalesceMode::Legacy ? "legacy" : "coalesce",
		            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
		                                                      drain_start)
		                .count());
	}
	SetMode(EventQueue::EqueueCoalesceMode::Coalesce);
}

} // namespace

int main(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--hitch-bench") == 0) {
			const uint64_t triggers =
			    i + 1 < argc ? std::strtoull(argv[i + 1], nullptr, 10) : 16000000ull;
			HitchBench(triggers);
			return 0;
		}
	}
	TestGraphicsCoalescing();
	TestLegacyQueue();
	TestExactCountUnderConcurrency();
	TestEdgeUserEvent();
	TestLevelUserEvent();
	TestOneShotTimer();
	TestActivationOrder();
	TestVideoOutCounter();
	TestVerifyModeStats();
	std::printf("EventQueueSemanticsTests: all cases passed\n");
	return 0;
}
