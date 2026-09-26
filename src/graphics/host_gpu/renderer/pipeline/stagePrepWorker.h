#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_STAGEPREPWORKER_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_STAGEPREPWORKER_H_

#include <atomic>
#include <cstdint>

namespace Libs::Graphics {

// One helper thread ("DrawPrep#0") that runs a single job at a time for the GPU thread, which
// forks it and then joins (fork/join within one draw; draw-prep S3). The job runs inside a
// GpuReadDelegate scope, so it may use the silent clean-backing probe; see gpuReadDelegate.h for
// what the GPU thread must (not) do until it joins.
//
// The helper spins for KYTY_STAGE_PREP_SPIN_US microseconds (default 250) after its last job and
// then sleeps. TryFork never waits for a sleeping helper: it wakes it and returns false, so the
// caller runs the job itself and the next draw finds the helper awake.
class StagePrepWorker {
public:
	using Function = void (*)(void* context);

	// Null when KYTY_STAGE_PREP_PARALLEL=0. Created on first use; never destroyed.
	[[nodiscard]] static StagePrepWorker* Get();

	// Starts function(context) on the helper and returns true, or returns false without
	// running it (helper asleep or busy). A started job must be joined with Join before the
	// caller leaves the fork window.
	[[nodiscard]] bool TryFork(Function function, void* context);
	// Spins until the forked job has returned. Its writes are visible afterwards.
	void Join();

	StagePrepWorker(const StagePrepWorker&)            = delete;
	StagePrepWorker& operator=(const StagePrepWorker&) = delete;

private:
	StagePrepWorker();
	void Run();

	std::atomic<uint64_t> m_posted {0};
	std::atomic<uint64_t> m_completed {0};
	// Incremented to wake a sleeping helper (std::atomic::wait needs a changed value).
	std::atomic<uint64_t> m_signal {0};
	std::atomic<bool>     m_sleeping {false};
	Function              m_function = nullptr;
	void*                 m_context  = nullptr;
	uint64_t              m_spin_ns  = 0;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_PIPELINE_STAGEPREPWORKER_H_
