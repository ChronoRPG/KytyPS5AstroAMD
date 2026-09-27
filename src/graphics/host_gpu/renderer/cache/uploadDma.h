#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_CACHE_UPLOADDMA_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_CACHE_UPLOADDMA_H_

#include "common/common.h"
#include "graphics/host_gpu/renderer/commandScheduler.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace Libs::Graphics {

struct GraphicContext;
class Buffer;

// Upload DMA (KYTY_UPLOAD_DMA, default on; needs a transfer-only queue family and is off under
// RenderDoc). CPU-dirty buffer uploads are staged in the host upload ring and copied into the
// device-local cache buffers by the graphics queue, which reads the staged bytes over PCIe: about
// 80 us/MB on the RTX 3090 (Gen3 x16, no resizable BAR), 20 MB and 1.3-1.6 ms of graphics-queue
// time per Sky Garden frame in the U49 capture (one per-frame 16 MB buffer alone 1.2 ms).
//
// Stage() moves the PCIe part to the copy engine: it reserves room in a device-local ring and
// queues a transfer-queue copy of the staged bytes into it. A worker submits the queued copies in
// batches, each signalling a timeline semaphore; the graphics copy then reads the ring (VRAM to
// VRAM) and the guest scheduler's submission that contains it waits for the semaphore value at
// the transfer stage (SubmitDependency). The data and the command order of the graphics copies
// are unchanged; only where the host bytes cross the bus changes.
//
// Ring space is reused only after the guest tick that recorded the reading copy has completed
// (that tick's submission waited for the transfer, so the transfer is complete too); a transfer
// batch that writes reused space also waits on the master semaphore for those ticks, so the
// ordering is explicit on the device. A full ring (or an upload below KYTY_UPLOAD_DMA_MIN_KB)
// keeps the graphics-queue copy from the host ring.
//
// KYTY_UPLOAD_DMA_MB (default 64): ring size. KYTY_UPLOAD_DMA_MIN_KB (default 64): smallest upload
// moved to the copy engine (small copies are latency bound; the semaphore wait would cost more).
[[nodiscard]] bool UploadDmaRequested();
// KYTY_UPLOAD_DMA_VERIFY=1: every staged upload also copies its ring bytes, in the recording that
// reads them, into a host-visible buffer; at completion they are compared with a snapshot of the
// staged host bytes taken when the copy was queued (UploadDmaVerifyChecks / UploadDmaVerifyMismatches
// and a log line). The copy ends the active rendering instance, so verify runs are for correctness.
[[nodiscard]] bool UploadDmaVerify();

class UploadDma final: public SubmitDependency {
public:
	UploadDma(GraphicContext& graphics, CommandScheduler& scheduler, uint64_t ring_size,
	          uint64_t min_bytes);
	~UploadDma() override;
	KYTY_CLASS_NO_COPY(UploadDma);

	// Null when disabled or the device has no transfer queue.
	[[nodiscard]] static std::unique_ptr<UploadDma> Create(GraphicContext&   graphics,
	                                                       CommandScheduler& scheduler);

	// Recording thread only. Queues the copy of [source_offset, source_offset + size) of `source`
	// (host bytes written before this call, in a buffer shared with the transfer family) into the
	// ring and returns the ring offset of the copy, or nullopt (too small, or no ring space that
	// the current recording's tick may use).
	[[nodiscard]] std::optional<uint64_t> Stage(vk::Buffer source, uint64_t source_offset,
	                                            uint64_t size);
	[[nodiscard]] vk::Buffer RingHandle() const noexcept;
	[[nodiscard]] uint64_t   RingSize() const noexcept { return m_ring_size; }
	// Copies queued so far (recording thread).
	[[nodiscard]] uint64_t   Staged() const noexcept { return m_enqueued; }
	[[nodiscard]] uint64_t   MinBytes() const noexcept { return m_min_bytes; }

	// SubmitDependency (the guest scheduler's submissions).
	[[nodiscard]] uint64_t               PendingValue() override;
	[[nodiscard]] vk::Semaphore          Semaphore() const override { return m_semaphore; }
	[[nodiscard]] vk::PipelineStageFlags WaitStages() const override {
		return vk::PipelineStageFlagBits::eTransfer;
	}
	void WaitHost(uint64_t value) override;

private:
	struct Job {
		vk::Buffer source        = nullptr;
		uint64_t   source_offset = 0;
		uint64_t   ring_offset   = 0;
		uint64_t   size          = 0;
		uint64_t   value         = 0;
		uint64_t   reuse_tick    = 0; // newest completed tick whose ring bytes were released
	};
	struct Span {
		uint64_t begin = 0;
		uint64_t end   = 0;
		uint64_t tick  = 0;
	};
	struct Batch {
		vk::CommandBuffer command = nullptr;
		uint64_t          value   = 0; // free once the semaphore reaches it
	};

	[[nodiscard]] std::optional<uint64_t> Allocate(uint64_t size);
	void                                  Worker(std::stop_token stop);
	void                                  SubmitBatch(std::vector<Job>& jobs);

	GraphicContext&         m_graphics;
	CommandScheduler&       m_scheduler;
	uint64_t                m_ring_size = 0;
	uint64_t                m_min_bytes = 0;
	std::unique_ptr<Buffer> m_ring;
	vk::Semaphore           m_semaphore = nullptr;
	vk::CommandPool         m_pool      = nullptr;
	// Recording thread.
	std::deque<Span>        m_spans; // allocated ring ranges in allocation order
	uint64_t                m_head        = 0;
	// Newest reading tick of released ranges. Released space may be written by any later job, so
	// every job carries it (the master semaphore is monotonic and that tick has completed).
	uint64_t                m_reuse_tick  = 0;
	uint64_t                m_enqueued    = 0;
	uint64_t                m_known_value = 0; // semaphore value observed by PendingValue
	// Newest value staged while recording m_stage_tick. Only the submission of that tick reads
	// its ring bytes (the graphics copies are recorded before the tick's End at the latest), so
	// only it waits; later submissions order after those copies through their own barriers.
	uint64_t                m_stage_tick  = 0;
	uint64_t                m_stage_value = 0;
	// Worker.
	std::vector<Batch>      m_batches;
	size_t                  m_next_batch = 0;
	// Shared.
	std::mutex              m_mutex;
	std::condition_variable m_available;
	std::vector<Job>        m_jobs;
	bool                    m_stopping = false;
	std::jthread            m_worker; // last: joined before the members it uses go away
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_CACHE_UPLOADDMA_H_
