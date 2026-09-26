#ifndef KYTY_GRAPHICS_HOST_GPU_QUEUESUBMISSION_H_
#define KYTY_GRAPHICS_HOST_GPU_QUEUESUBMISSION_H_

#include "common/assert.h"
#include "common/common.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace Libs::Graphics {

struct GraphicContext;

struct SubmitInfo {
	static constexpr uint32_t MaxSemaphores = 3;

	std::array<vk::Semaphore, MaxSemaphores>          wait_semaphores {};
	std::array<uint64_t, MaxSemaphores>               wait_ticks {};
	std::array<vk::PipelineStageFlags, MaxSemaphores> wait_stages {};
	std::array<vk::Semaphore, MaxSemaphores>          signal_semaphores {};
	std::array<uint64_t, MaxSemaphores>               signal_ticks {};
	uint32_t                                        num_wait_semaphores   = 0;
	uint32_t                                        num_signal_semaphores = 0;

	void AddWait(vk::Semaphore semaphore, uint64_t tick = 1,
	             vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eAllCommands) {
		EXIT_IF(semaphore == nullptr || num_wait_semaphores >= MaxSemaphores);
		wait_semaphores[num_wait_semaphores] = semaphore;
		wait_ticks[num_wait_semaphores]      = tick;
		wait_stages[num_wait_semaphores++]   = stage;
	}

	void AddSignal(vk::Semaphore semaphore, uint64_t tick = 1) {
		EXIT_IF(semaphore == nullptr || num_signal_semaphores >= MaxSemaphores);
		signal_semaphores[num_signal_semaphores] = semaphore;
		signal_ticks[num_signal_semaphores++]   = tick;
	}
};

// A completed GPU signal does not prove vkQueueSubmit has returned on its host
// thread. Reuse and teardown need both facts. Records retain this token so its
// notification stays alive even when the owning scheduler finishes shutting down.
struct SubmissionProgress {
	std::atomic<uint64_t> dispatched_tick {0};
};

// Own every argument used by vkQueueSubmit. No pointer into the scheduler's mutable
// CommandBuffer wrapper or the producer's stack may survive enqueueing.
struct QueuedSubmission {
	SubmitInfo        submit;
	std::shared_ptr<SubmissionProgress> progress;
	vk::Semaphore     master_semaphore = nullptr;
	vk::CommandBuffer command = nullptr;
	uint64_t          tick = 0;
	// This tick has a callback or an explicit CPU wait. It may terminate a
	// coalesced group, but its signal must not move past a later command buffer.
	bool              preserve_completion = false;
	// Optional KYTY_GPU_TIMING slot field: steady_clock nanoseconds when the native vkQueueSubmit
	// containing this record returned. Written before dispatched_tick is published (release), and
	// read only after KnownGpuTick() covers this tick, so the slot outlives the write.
	uint64_t*         dispatch_ns = nullptr;
	uint32_t          debug_op = 0;
	uint64_t          debug_submit = 0;
	uint32_t          debug_arg0 = 0;
	uint32_t          debug_arg1 = 0;
	uint32_t          debug_arg2 = 0;
	uint32_t          debug_arg3 = 0;
	uint64_t          debug_arg4 = 0;
};

// One broker per native queue, shared by the guest and presentation schedulers.
// Queued mode is opt-in through KYTY_SUBMISSION_MODE=queued.
class QueueSubmissionBroker {
public:
	QueueSubmissionBroker() = default;
	~QueueSubmissionBroker();
	KYTY_CLASS_NO_COPY(QueueSubmissionBroker);

	void Initialize(GraphicContext& graphics);
	[[nodiscard]] bool Enabled() const noexcept { return m_enabled; }
	// The producer must not hold queue_mutex: capacity backpressure waits for a
	// consumer that takes that mutex. A scheduler retains its existing single-
	// producer ownership, so its allocated ticks enter this FIFO in order.
	void Enqueue(QueuedSubmission submission);

	// The caller MUST own GraphicContext::queue_mutex. Both the worker and direct
	// queue users acquire that mutex before taking records, so older records can
	// never be detached but still waiting to enter the native queue.
	void DrainPendingLocked();
	// Called by the window owner after both schedulers stop, before device destruction. Joins
	// without holding queue_mutex and submits all accepted records before returning.
	void Shutdown();

private:
	static constexpr size_t MaxQueued = 256;
	static constexpr size_t MaxBatch  = 64;
	void Worker();
	void SubmitBatch(const QueuedSubmission* records, size_t count);

	GraphicContext*              m_graphics = nullptr;
	bool                         m_enabled = false;
	bool                         m_coalesce = false;
	bool                         m_initialized = false;
	bool                         m_stopping = false;
	bool                         m_stopped = false;
	std::mutex                   m_mutex;
	std::condition_variable      m_available;
	std::condition_variable      m_space_available;
	std::deque<QueuedSubmission> m_pending;
	std::jthread                 m_worker;
	uint64_t                     m_queued_count = 0;
	uint64_t                     m_driver_calls = 0;
	uint64_t                     m_native_submits = 0;
	uint64_t                     m_coalesced_boundaries = 0;
	uint64_t                     m_protected_boundaries = 0;
	size_t                       m_peak_queued = 0;
};

} // namespace Libs::Graphics

#endif // KYTY_GRAPHICS_HOST_GPU_QUEUESUBMISSION_H_
