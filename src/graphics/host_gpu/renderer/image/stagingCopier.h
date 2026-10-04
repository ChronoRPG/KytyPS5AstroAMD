#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGE_STAGINGCOPIER_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGE_STAGINGCOPIER_H_

#include "common/common.h"
#include "graphics/host_gpu/renderer/commandScheduler.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace Libs::Graphics {

struct GraphicContext;
class Buffer;

// Guest memory -> staging buffer copies for texture refreshes, made on a host worker after the
// commands reading the staging bytes were recorded (KYTY_TEXTURE_ASYNC_STAGING, default on).
// Every job publishes its value once its bytes are written and flushed. The thread that submits
// the guest scheduler's next batch waits for the latest value on the host before vkQueueSubmit
// (SubmitDependency), so the recording thread never waits for the copy, and the GPU never waits
// for a signal that only the host can make later. With KYTY_SUBMIT_WAIT_BEFORE_SIGNAL=1 the job
// signals a timeline semaphore instead and the batch waits for it on the GPU (up to int7).
//
// The source pages are write-protected by the refreshed image before the job is queued. A
// guest write racing with the copy faults and dirties its chunk (TextureCache), so the next
// refresh replaces whatever the copy saw of that chunk.
class StagingCopier final: public SubmitDependency {
public:
	struct Range {
		uint64_t guest_address = 0;
		uint8_t* destination   = nullptr;
		uint64_t size          = 0;
	};

	explicit StagingCopier(GraphicContext& graphics);
	~StagingCopier() override;
	KYTY_CLASS_NO_COPY(StagingCopier);

	// Recording thread only. `flush_buffer` (the mapped staging buffer) is flushed over
	// [flush_offset, flush_offset + flush_size) after the copies, for non-coherent memory.
	void Enqueue(std::vector<Range> ranges, Buffer* flush_buffer, uint64_t flush_offset,
	             uint64_t flush_size);

	[[nodiscard]] uint64_t      PendingValue() override;
	// Null unless KYTY_SUBMIT_WAIT_BEFORE_SIGNAL=1.
	[[nodiscard]] vk::Semaphore Semaphore() const override { return m_semaphore; }
	[[nodiscard]] bool          Submittable(uint64_t value) override {
		return m_completed.load(std::memory_order_acquire) >= value;
	}
	void WaitSubmittable(uint64_t value) override { WaitHost(value); }
	void WaitHost(uint64_t value) override;

private:
	struct Job {
		std::vector<Range> ranges;
		Buffer*            flush_buffer = nullptr;
		uint64_t           flush_offset = 0;
		uint64_t           flush_size   = 0;
		uint64_t           value        = 0;
	};

	void Worker(std::stop_token stop);
	void Run(Job& job);

	GraphicContext&         m_graphics;
	vk::Semaphore           m_semaphore = nullptr;
	std::mutex              m_mutex;
	std::condition_variable m_available;
	std::deque<Job>         m_jobs;
	uint64_t                m_enqueued = 0; // recording thread
	std::atomic<uint64_t>   m_completed {0};
	bool                    m_stopping = false;
	uint64_t                m_read_failures = 0; // worker thread
	std::jthread            m_worker;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_IMAGE_STAGINGCOPIER_H_
