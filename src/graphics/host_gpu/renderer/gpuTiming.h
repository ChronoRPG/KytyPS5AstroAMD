#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GPUTIMING_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GPUTIMING_H_

// Diagnostic GPU execution timing (KYTY_GPU_TIMING).
//
// Each guest-scheduler command buffer gets a top-of-pipe and an all-commands timestamp written
// inside the existing buffer, plus CPU timestamps for recording start, the scheduler's Submit call,
// the native vkQueueSubmit return and the observed completion. Results are read without waiting
// once the owning tick is known complete, and aggregated per guest flip into GPU busy time (the
// union of the command-buffer spans), idle gaps and CPU record -> GPU start latency.
//
// Nothing here adds a submission, a wait, a barrier or a render-pass break. Timestamp spans are
// execution-span estimates, not a hardware busy counter; the presenter scheduler is not timed.

#include "common/common.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <cstdint>
#include <vector>

namespace Libs::Graphics {

struct GraphicContext;

namespace GpuTiming {

// KYTY_GPU_TIMING=1/0 forces the diagnostic on/off. Unset, it follows KYTY_HANG_TRACE=1 or
// Profiler aggregate diagnostics (KYTY_PROFILE_FRAMES_ONLY=1 + KYTY_PROFILE_AGGREGATES=1), the two
// consumers of its results. Evaluated once; normal play records no timestamp commands.
[[nodiscard]] bool Enabled();

// steady_clock nanoseconds, the CPU domain of every sample and of the calibration mapping.
[[nodiscard]] uint64_t NowNs() noexcept;

// Device creation: returns the calibrated-timestamp extension to enable (KHR preferred, then EXT),
// or nullptr when timing is disabled or neither is available. Without one, busy/idle are still
// reported but CPU->GPU latencies are not (no clock mapping), and cross-submission ordering of
// timestamps relies on the implementation rather than on the Vulkan specification.
[[nodiscard]] const char* SelectCalibrationExtension(
    const std::vector<vk::ExtensionProperties>& available);
// Records that the extension returned above was enabled on the created device.
void NoteCalibrationExtensionEnabled(const char* name);
// Whether a calibrated-timestamp extension was enabled (also for KYTY_EOP_TIMESTAMPS=gpu).
[[nodiscard]] bool CalibrationEnabled();

// Called once per completed guest flip, before Profiler::PublishFrameWork(). Aggregates the
// samples collected since the previous flip and publishes them to HangTrace and Profiler.
void OnGuestFlip();

} // namespace GpuTiming

// One collected command buffer: raw device ticks and steady_clock nanoseconds (GpuTiming::NowNs).
struct GpuTimingSample {
	uint64_t gpu_start   = 0;
	uint64_t gpu_end     = 0;
	uint64_t record_ns   = 0; // BeginCommand: recording of this buffer started
	uint64_t submit_ns   = 0; // CommandScheduler::Submit was entered
	uint64_t dispatch_ns = 0; // native vkQueueSubmit containing it returned
	uint64_t observed_ns = 0; // producer observed the tick complete and read the queries
};

// Fixed ring of timestamp-query pairs owned by one command scheduler.
//
// Threading: every member except the broker's dispatch-time store is called by the scheduler's
// single recording producer (the same discipline that already owns CommandScheduler::m_command).
// The submission broker writes a slot's dispatch time only through the pointer returned by
// Submitted(), before it publishes that tick's dispatched_tick with release semantics.
//
// Slot life cycle, in ring (and therefore tick) order: Free -> Recording (BeginCommand: reset +
// start timestamp) -> Recorded (EndCommand: end timestamp) -> Submitted (tick known) -> Collected
// -> Free. A slot is reused only after collection, and collection requires KnownGpuTick() >= tick,
// which also covers the queued-mode host dispatch gate. Until the GPU executes the recorded reset,
// a previous generation can still read as available, so availability alone is never sufficient.
class GpuTimestampRing {
public:
	// About 14 guest flips of headroom at ~570 command buffers per flip.
	static constexpr uint32_t GuestPairs = 8192;

	GpuTimestampRing(GraphicContext& graphics, uint32_t pair_capacity);
	~GpuTimestampRing();
	KYTY_CLASS_NO_COPY(GpuTimestampRing);

	// False when the queue family has no timestamp support; every call is then a no-op.
	[[nodiscard]] bool Valid() const noexcept { return m_pool != nullptr; }

	// Directly after vkBeginCommandBuffer, outside any rendering scope. record_ns: when the
	// producer started recording this buffer (0: now; the CP recorder passes the CP's time).
	void BeginCommand(vk::CommandBuffer buffer, uint64_t record_ns = 0);
	// After the final EndRendering and before vkEndCommandBuffer.
	void EndCommand(vk::CommandBuffer buffer);
	// Once the submitted tick is allocated; submit_ns is the Submit call time. Returns the
	// dispatch-time field the native submitter must store before publishing the tick, or nullptr
	// when this command buffer is not timed.
	[[nodiscard]] uint64_t* Submitted(uint64_t tick, uint64_t submit_ns);
	// Reads completed slots without waiting. With force, collects every complete slot regardless
	// of the batching threshold (shutdown); otherwise at most a bounded batch per call.
	void Collect(uint64_t known_gpu_tick, bool force = false);

private:
	enum class SlotState : uint8_t { Free, Recording, Recorded, Submitted };
	struct Slot {
		uint64_t  tick        = 0;
		uint64_t  record_ns   = 0;
		uint64_t  submit_ns   = 0;
		uint64_t  dispatch_ns = 0;
		SlotState state       = SlotState::Free;
	};

	void CollectRun(uint32_t first, uint32_t count, uint64_t observed_ns);

	GraphicContext&   m_graphics;
	vk::QueryPool     m_pool = nullptr;
	std::vector<Slot> m_slots;
	// Collection scratch, kept off the producer's stack: {timestamp, availability} x 2 per pair.
	std::vector<uint64_t> m_results;
	std::vector<GpuTimingSample> m_samples;
	uint32_t          m_head      = 0; // oldest occupied slot
	uint32_t          m_count     = 0; // occupied slots, including the one being recorded
	uint32_t          m_recording = UINT32_MAX;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GPUTIMING_H_
