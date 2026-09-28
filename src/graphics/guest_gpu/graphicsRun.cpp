#include "graphics/guest_gpu/graphicsRun.h"

#include "common/assert.h"
#include "common/cpuPlacement.h"
#include "common/emulatorConfig.h"
#include "common/logging/log.h"
#include "common/hangTrace.h"
#include "common/profiler.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "graphics/guest_gpu/command_processor/commandProcessor.h"
#include "graphics/guest_gpu/command_processor/pm4Dispatch.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/guest_gpu/pm4.h"
#include "graphics/host_gpu/coherenceLog.h"
#include "graphics/host_gpu/renderer/drawPrep/drawPrep.h"
#include "graphics/host_gpu/renderer/drawPrep/packetClass.h"
#include "graphics/host_gpu/renderer/drawPrep/repeatTrace.h"
#include "graphics/host_gpu/renderer/eopTimestamps.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/host_gpu/renderer/sync.h"
#include "graphics/host_gpu/syncEpoch.h"
#include "graphics/presentation/videoOut.h"
#include "graphics/presentation/window.h"
#include "graphics/shader/recompiler/CodegenOptions.h"
#include "graphics/shader/shader.h"
#include "kernel/memory.h"
#include "libs/agc.h"
#include "libs/errno.h"
#include "graphics/host_gpu/renderer/gpuOpProfiler.h"

#include <algorithm>
#include <chrono>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <semaphore>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Libs::Graphics {

static thread_local CommandProcessor* g_current_processor = nullptr;
static thread_local Pm4Execution*     g_current_execution = nullptr;
static thread_local bool              g_gpu_mutex_owned   = false;
static thread_local bool              g_gpu_thread        = false;
static thread_local GuestGpu*         g_gpu_state         = nullptr;

struct DrawIndirectArgs {
	uint32_t vertex_count_per_instance;
	uint32_t instance_count;
	uint32_t start_vertex_location;
	uint32_t start_instance_location;
};

struct DrawIndexedIndirectArgs {
	uint32_t index_count_per_instance;
	uint32_t instance_count;
	uint32_t start_index_location;
	uint32_t base_vertex_location;
	uint32_t start_instance_location;
};

// Native indirect draws hand these records to the host GPU unchanged.
static_assert(sizeof(DrawIndirectArgs) == sizeof(vk::DrawIndirectCommand) &&
              offsetof(DrawIndirectArgs, instance_count) ==
                  offsetof(vk::DrawIndirectCommand, instanceCount) &&
              offsetof(DrawIndirectArgs, start_instance_location) ==
                  offsetof(vk::DrawIndirectCommand, firstInstance));
static_assert(sizeof(DrawIndexedIndirectArgs) == sizeof(vk::DrawIndexedIndirectCommand) &&
              offsetof(DrawIndexedIndirectArgs, instance_count) ==
                  offsetof(vk::DrawIndexedIndirectCommand, instanceCount) &&
              offsetof(DrawIndexedIndirectArgs, base_vertex_location) ==
                  offsetof(vk::DrawIndexedIndirectCommand, vertexOffset) &&
              offsetof(DrawIndexedIndirectArgs, start_instance_location) ==
                  offsetof(vk::DrawIndexedIndirectCommand, firstInstance));
constexpr uint64_t IndirectInstanceCountOffset = offsetof(DrawIndirectArgs, instance_count);
static_assert(offsetof(DrawIndexedIndirectArgs, instance_count) == IndirectInstanceCountOffset);

// KYTY_NATIVE_INDIRECT=0 keeps every indirect draw on CPU-read arguments.
static bool NativeIndirectEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_NATIVE_INDIRECT");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

// KYTY_INDIRECT_VALIDATE=1 (diagnostic): also read native arguments on the CPU, which drains
// the GPU, and log where the native and CPU-read draws would differ.
static bool IndirectValidateEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_INDIRECT_VALIDATE");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return enabled;
}

static uint64_t IndexElementSize(uint32_t index_type_and_size) {
	switch (index_type_and_size) {
		case 0: return 2;
		case 1: return 4;
		case 2: return 1;
		default: EXIT("unknown index_type_and_size: %u\n", index_type_and_size);
	}
	return 0;
}

void ReadGuestForCp(uint64_t vaddr, uint64_t size, void* dst) {
	EXIT_IF(vaddr == 0 || dst == nullptr);
	if (LibKernel::Memory::TryReadGpuCleanBacking(vaddr, dst, size)) {
		return;
	}
	if (LibKernel::Memory::SynchronizeGpuBackingForRead(vaddr, size) &&
	    LibKernel::Memory::TryReadGpuCleanBacking(vaddr, dst, size)) {
		return;
	}
	std::memcpy(dst, reinterpret_cast<const void*>(vaddr), size);
}

class GpuMutexLock final {
public:
	explicit GpuMutexLock(Common::Mutex& mutex): m_mutex(mutex) {
		if (g_gpu_mutex_owned) {
			EXIT("recursive GPU mutex acquisition\n");
		}
		g_gpu_mutex_owned = true;
		m_mutex.Lock();
	}
	~GpuMutexLock() {
		if (!g_gpu_mutex_owned) {
			EXIT("invalid GPU mutex release\n");
		}
		m_mutex.Unlock();
		g_gpu_mutex_owned = false;
	}

private:
	Common::Mutex& m_mutex;
};

static bool GraphicsRunDebugDumpEnabled() {
	return Config::GraphicsDebugDumpEnabled() &&
	       Config::GetPrintfDirection() != Config::LogDirection::Silent;
}

// KYTY_CP_WAKEUPS=0 restores the old blocked-queue handling: sleep 100 us (about 1 ms with the
// Windows condition variable) and retry every blocked queue only after that timeout or when a
// queue completes. By default completed GPU work (every completion-runner operation), deferred
// label writes and flip completions wake the scheduler and unblock its queues at once, and
// before sleeping it spins for KYTY_CP_BLOCKED_SPIN_US (default 50) retrying blocked queues,
// which catches guest CPU writes (not observable otherwise) that follow shortly.
static bool CpWakeupsEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_CP_WAKEUPS");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

static uint64_t CpBlockedSpinNs() {
	static const uint64_t ns = [] {
		const auto* value  = std::getenv("KYTY_CP_BLOCKED_SPIN_US");
		const auto  parsed = value != nullptr ? std::strtoul(value, nullptr, 10) : 50ul;
		return static_cast<uint64_t>(std::min(parsed, 10000ul)) * 1000u;
	}();
	return ns;
}

static uint64_t CpNowNs() {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
	                                 std::chrono::steady_clock::now().time_since_epoch())
	                                 .count());
}

// cp.csv label rows (KYTY_HANG_TRACE_CP).
static void TraceCpLabel(const char* kind, const void* dst, uint64_t value, uint64_t size) {
	if (HangTrace::CpTraceEnabled()) {
		HangTrace::CpEvent event;
		event.event   = kind;
		event.address = reinterpret_cast<uint64_t>(dst);
		event.value   = value;
		event.size    = size;
		HangTrace::RecordCp(event);
	}
}

GuestGpu::GuestGpu(RenderContext& renderer): m_renderer(renderer) {
	EXIT_NOT_IMPLEMENTED(!Common::Thread::IsMainThread());
	GraphicsInitJmpTables();
	m_gfx_cp = std::make_unique<CommandProcessor>(renderer, 0);
	if (CpWakeupsEnabled()) {
		m_renderer.GetCommandScheduler().SetProgressHook(
		    [](void* context) { static_cast<GuestGpu*>(context)->NotifyProgress(); }, this);
	}
	m_thread = std::jthread(ThreadRun, this);
}

GuestGpu::~GuestGpu() {
	Shutdown();
}

void GuestGpu::Shutdown() {
	std::lock_guard shutdown_lock(m_shutdown_mutex);
	if (m_shutdown_complete) {
		return;
	}
	{
		Common::LockGuard lock(m_queue_mutex);
		m_accepting = false;
		m_stopping  = true;
		m_work_available.SignalAll();
	}
	if (m_thread.joinable()) {
		m_thread.join();
	}
	// No completion-runner call may reach this object once it is destroyed.
	m_renderer.GetCommandScheduler().SetProgressHook(nullptr, nullptr);
	m_renderer.GetCommandScheduler().DrainPriorityOperations();
	m_shutdown_complete = true;
}

bool GuestGpu::IsStopping() {
	Common::LockGuard lock(m_queue_mutex);
	return m_stopping;
}

void GuestGpu::SendCommand(Common::UniqueFunction<void>&& command) {
	EXIT_IF(!command);
	if (IsGpuThread()) {
		command();
		return;
	}
	Common::LockGuard lock(m_queue_mutex);
	EXIT_IF(!m_accepting);
	m_commands.push_back(std::move(command));
	m_pending_commands.fetch_add(1, std::memory_order_release);
	m_work_available.Signal();
}

void GuestGpu::ProcessCommands() {
	EXIT_IF(!IsGpuThread());
	while (m_pending_commands.load(std::memory_order_acquire) != 0) {
		Common::UniqueFunction<void> command;
		{
			Common::LockGuard lock(m_queue_mutex);
			EXIT_IF(m_commands.empty());
			command = std::move(m_commands.front());
			m_commands.pop_front();
			EXIT_IF(m_pending_commands.fetch_sub(1, std::memory_order_acq_rel) == 0);
		}
		command();
		// Service commands (mapping changes, readbacks, deferred label writes) change guest
		// memory outside the command stream (syncEpoch.h).
		SyncEpoch::Advance();
	}
}

void GuestGpu::SendCommandSync(Common::UniqueFunction<void>&& command) {
	EXIT_IF(!command);
	if (IsGpuThread()) {
		command();
		return;
	}
	std::binary_semaphore done {0};
	SendCommand([operation = std::move(command), &done]() mutable {
		operation();
		done.release();
	});
	done.acquire();
}

bool GuestGpu::TrySendCommand(Common::UniqueFunction<void>&& command) {
	EXIT_IF(!command);
	Common::LockGuard lock(m_queue_mutex);
	if (!m_accepting) {
		return false;
	}
	m_commands.push_back(std::move(command));
	m_pending_commands.fetch_add(1, std::memory_order_release);
	m_work_available.Signal();
	return true;
}

void GuestGpu::NotifyProgress() {
	// Cheap when nothing is blocked (called after every completion-runner operation).
	if (!m_has_blocked.exchange(false, std::memory_order_acq_rel)) {
		return;
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::CpProgressWakeups);
	Common::LockGuard lock(m_queue_mutex);
	for (auto& queue: m_queues) {
		if (!queue.empty()) {
			queue.front().blocked = false;
		}
	}
	m_work_available.Signal();
}

bool GuestGpu::HasRunnableComputeWork() {
	Common::LockGuard lock(m_queue_mutex);
	for (uint32_t id = 1; id < QueueCount; id++) {
		if (!m_queues[id].empty() && !m_queues[id].front().blocked &&
		    FrameFencePassed(m_queues[id].front())) {
			return true;
		}
	}
	return false;
}

// KYTY_FRAME_FENCE=0 lets a submission admitted after sceAgcSuspendPoint (bounded Done) start
// while submissions admitted before that Done are still pending on other queues. By default the
// CP scheduler holds it until they have completed, which keeps the cross-queue frame order the
// idle Done gave (only the scheduler waits, the guest thread does not). Without it, Astro Bot's
// async-compute copy into memory that the previous frame's water draw still samples ran between
// that frame's refraction copy and water draw once graphics slices could yield
// (KYTY_GFX_SLICE_DRAWS): white or opaque water.
static bool FrameFenceEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_FRAME_FENCE");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

// A front held this long by the fence runs anyway (logged once): a hang would be worse.
constexpr uint64_t FrameFenceTimeoutNs = 2'000'000'000;

bool GuestGpu::FrameFencePassed(const Submission& submission) const {
	// m_in_flight holds the submission itself (a larger sequence), so its first element exceeds
	// the fence exactly when every submission admitted before that Done has completed. A started
	// submission (blocked or yielded since) passed the fence already.
	return submission.frame_fence == 0 || submission.started || m_in_flight.empty() ||
	       *m_in_flight.begin() > submission.frame_fence ||
	       (submission.fence_hold_ns != 0 &&
	        CpNowNs() - submission.fence_hold_ns > FrameFenceTimeoutNs);
}

void GuestGpu::AddDeferredLabel(uint64_t address, uint32_t size, uint64_t tick) {
	Common::LockGuard lock(m_queue_mutex);
	m_deferred_labels.push_back({address, size, tick});
	m_deferred_label_count.store(static_cast<uint32_t>(m_deferred_labels.size()),
	                             std::memory_order_release);
}

void GuestGpu::RemoveDeferredLabel(uint64_t address, uint64_t tick) {
	Common::LockGuard lock(m_queue_mutex);
	const auto found = std::find_if(m_deferred_labels.begin(), m_deferred_labels.end(),
	                                [address, tick](const DeferredLabel& label) {
		                                return label.address == address && label.tick == tick;
	                                });
	EXIT_IF(found == m_deferred_labels.end());
	m_deferred_labels.erase(found);
	m_deferred_label_count.store(static_cast<uint32_t>(m_deferred_labels.size()),
	                             std::memory_order_release);
	// A queue suspended on this label (WAIT_REG_MEM) can make progress now.
	for (auto& queue: m_queues) {
		if (!queue.empty()) {
			queue.front().blocked = false;
		}
	}
	m_work_available.Signal();
}

uint64_t GuestGpu::DeferredLabelTick(uint64_t address, uint64_t size) {
	if (!HasDeferredLabels()) {
		return 0;
	}
	Common::LockGuard lock(m_queue_mutex);
	uint64_t          tick = 0;
	for (const auto& label: m_deferred_labels) {
		if (label.address < address + size && address < label.address + label.size) {
			tick = std::max(tick, label.tick);
		}
	}
	return tick;
}

void GuestGpu::Submit(std::span<const uint32_t> draw_commands,
                      std::span<const uint32_t> constant_commands) {
	if (draw_commands.empty()) {
		return;
	}
	GpuMutexLock lock(m_submission_mutex);
	Submission   submission;
	submission.type              = SubmissionType::Graphics;
	submission.queue_id          = 0;
	submission.commands          = draw_commands;
	submission.constant_commands = constant_commands;
	submission.reset_processor   = m_graphics_done;
	m_graphics_done              = false;
	Enqueue(std::move(submission));
}

void GuestGpu::SubmitCompute(uint32_t queue, std::span<const uint32_t> commands) {
	EXIT_IF(commands.empty());
	GpuMutexLock lock(m_submission_mutex);

	EXIT_NOT_IMPLEMENTED(queue < ComputeQueueBase || queue >= ComputeQueueBase + ComputeQueueCount);

	const auto compute_queue = queue - ComputeQueueBase;
	Submission submission;
	submission.type     = SubmissionType::Compute;
	submission.queue_id = 1 + compute_queue;
	submission.commands = commands;
	Enqueue(std::move(submission));
}

void GuestGpu::SubmitFlipPreparation(uint64_t request_id) {
	GpuMutexLock lock(m_submission_mutex);
	Submission   submission;
	submission.type            = SubmissionType::FlipPreparation;
	submission.queue_id        = 0;
	submission.reset_processor = m_graphics_done;
	submission.flip_request_id = request_id;
	m_graphics_done            = false;
	Enqueue(std::move(submission));
}

// KYTY_AGC_DONE_MODE: "idle" makes sceAgcSuspendPoint (GuestGpu::Done) hold the submission lock
// and wait until the CP has consumed everything and has nothing queued (the previous behaviour;
// 105-145 ms per frame in Sky Garden, serializing guest frame N+1 building with CP frame N and
// blocking other threads' submissions). "bounded" (default) records the frame boundary (the
// processor-reset flag for the next graphics submission and the frame number) under the lock,
// releases it, and waits only until the CP has completed every submission admitted before the
// previous Done: at most two guest frames in flight on the CP.
static bool DoneBounded() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_AGC_DONE_MODE");
		const bool  on    = value == nullptr || std::strcmp(value, "idle") != 0;
		std::printf("Kyty AgcSuspendPoint: %s (KYTY_AGC_DONE_MODE)\n",
		            on ? "bounded (two frames in flight)" : "wait for idle");
		return on;
	}();
	return enabled;
}

void GuestGpu::Done() {
	if (!DoneBounded()) {
		GpuMutexLock lock(m_submission_mutex);
		if (!IsGpuThread()) {
			const auto wait_start = HangTrace::Enabled() ? HangTrace::NowNs() : 0;
			Profiler::ScopedFrameWait frame_wait(Profiler::FrameWait::AgcDoneWait);
			WaitForIdle();
			if (HangTrace::Enabled()) {
				HangTrace::RecordDoneWait(HangTrace::NowNs() - wait_start);
			}
		}
		m_graphics_done = true;
		m_done_num++;
		return;
	}
	uint64_t target = 0;
	{
		GpuMutexLock lock(m_submission_mutex);
		// Admission order is what defines the frame boundary: every later graphics submission
		// resets the processor first, exactly as after an idle wait.
		m_graphics_done = true;
		m_done_num++;
		Common::LockGuard queue_lock(m_queue_mutex);
		target              = m_done_boundary;
		m_done_boundary     = m_next_submission_sequence - 1;
	}
	if (IsGpuThread() || target == 0) {
		return;
	}
	const auto wait_start = HangTrace::Enabled() ? HangTrace::NowNs() : 0;
	{
		Profiler::ScopedFrameWait frame_wait(Profiler::FrameWait::AgcDoneWait);
		Common::LockGuard         lock(m_queue_mutex);
		auto                      prefix_done = [this, target] {
			return m_stopping || m_in_flight.empty() || *m_in_flight.begin() > target;
		};
		if (!prefix_done()) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::AgcDoneBoundedWaits);
			++m_done_waiters;
			while (!prefix_done()) {
				m_done_progress.Wait(&m_queue_mutex);
			}
			--m_done_waiters;
		}
	}
	if (HangTrace::Enabled()) {
		HangTrace::RecordDoneWait(HangTrace::NowNs() - wait_start);
	}
}

int GuestGpu::GetFrameNum() const {
	return m_done_num;
}

CommandProcessor& GuestGpu::GetProcessor(uint32_t queue_id) {
	EXIT_IF(queue_id >= QueueCount);
	if (queue_id == 0) {
		return *m_gfx_cp;
	}
	auto& processor = m_compute_cp[queue_id - 1];
	if (processor == nullptr) {
		processor = std::make_unique<CommandProcessor>(m_renderer, ComputeQueueBase + queue_id - 1);
	}
	return *processor;
}

CommandProcessor::~CommandProcessor() = default;

void CommandProcessor::DrawPrepDeleter::operator()(DrawPrep::Engine* engine) const noexcept {
	delete engine;
}

DrawPrep::Engine* CommandProcessor::DrawPrepEngine() {
	// Only the graphics processor draws; compute queues would only dilute the S0 histogram.
	if (!DrawPrep::PacketHookActive() || IsAsyncComputeQueue()) {
		return nullptr;
	}
	if (m_draw_prep == nullptr) {
		m_draw_prep.reset(new DrawPrep::Engine(
		    m_renderer,
		    [] {
			    if (g_gpu_state != nullptr) {
				    g_gpu_state->ProcessCommands();
			    }
		    },
		    // After each committed (recorded) draw, with the live registers bound again: the
		    // idle-GPU early submit counts recorded draws, exactly as after a serial draw.
		    [this] { MaybeFlushIdleGpu(); }));
	}
	return m_draw_prep.get();
}

bool CommandProcessor::TrySubmitPreparedDraw(const DrawIndexArgs* index_args,
                                             const DrawAutoArgs*  auto_args) {
	if (DrawPrep::GetMode() == DrawPrep::Mode::Off) {
		return false;
	}
	auto* engine = DrawPrepEngine();
	return engine != nullptr &&
	       engine->Submit(m_submit_id, index_args, auto_args, m_ctx, m_ucfg, m_sh_ctx);
}

void CommandProcessor::DrainPreparedDraws() {
	if (m_draw_prep != nullptr) {
		m_draw_prep->Drain();
	}
}

void CommandProcessor::Reset() {
	m_sh_ctx.Reset();
	m_ucfg.Reset();
	m_ctx.Reset();
	m_saved_ctx.Reset();
	m_context_state_pushed             = false;
	m_index_type_and_size              = 0;
	m_index_buffer_size                = 0;
	m_user_data_marker                 = HW::UserSgprType::Unknown;
	m_draw_indirect_args_base_addr     = 0;
	m_dispatch_indirect_args_base_addr = 0;

	std::memset(m_const_ram, 0, sizeof(m_const_ram));
}

void CommandProcessor::ApplyContextStateOperation(ContextStateOperation operation) {
	switch (operation) {
		case ContextStateOperation::Clear: m_ctx.Reset(); break;
		case ContextStateOperation::Push:
			EXIT_IF(m_context_state_pushed);
			m_saved_ctx            = m_ctx;
			m_context_state_pushed = true;
			break;
		case ContextStateOperation::Pop:
			EXIT_IF(!m_context_state_pushed);
			m_ctx                  = m_saved_ctx;
			m_saved_ctx            = {};
			m_context_state_pushed = false;
			break;
		case ContextStateOperation::PushClear:
			EXIT_IF(m_context_state_pushed);
			m_saved_ctx            = m_ctx;
			m_context_state_pushed = true;
			m_ctx.Reset();
			break;
		default: EXIT("unknown context state operation: %u\n", static_cast<uint32_t>(operation));
	}
}

void CommandProcessor::BufferInit() {
	GetScheduler().Begin(m_ctx, m_ucfg, m_sh_ctx);
}

void CommandProcessor::BufferFlush() {
	KYTY_PROFILER_DETAIL_FUNCTION();
	m_deferred_eop_flushes     = 0;
	m_packets_since_eop_request = 0;
	// Between packets: append eager readback copies of read-hot pages to this recording
	// (KYTY_READBACK_EAGER), published when it completes.
	m_renderer.GetBufferCache().IssueEagerReadbacks();
	GetScheduler().Flush();
}

uint32_t CommandProcessor::EopFlushPacketLimit() {
	// KYTY_EOP_FLUSH_PACKETS: packets processed after the first deferred interrupt before the
	// command buffer is flushed anyway (default 256).
	static const uint32_t limit = [] {
		const char* value  = std::getenv("KYTY_EOP_FLUSH_PACKETS");
		const auto  parsed = value != nullptr ? std::strtoul(value, nullptr, 10) : 256ul;
		return static_cast<uint32_t>(std::clamp(parsed, 1ul, 1ul << 20u));
	}();
	return limit;
}

void CommandProcessor::BufferFlushForEop() {
	static const uint32_t batch = [] {
		const char* value = std::getenv("KYTY_EOP_FLUSH_BATCH");
		const auto  parsed = value != nullptr ? std::strtoul(value, nullptr, 10) : 8ul;
		return static_cast<uint32_t>(std::clamp(parsed, 1ul, 1024ul));
	}();
	if (++m_deferred_eop_flushes >= batch) {
		BufferFlush();
	}
}

// KYTY_IDLE_FLUSH_DRAWS (default 8, 0 disables): after at least that many draws/dispatches in
// the current recording, submit it early when the GPU has finished everything already submitted
// (KnownGpuTick >= CurrentTick - 1), e.g. after a drain, instead of leaving the GPU idle until
// the next natural boundary. Inside an active rendering instance it waits for 4x the draws (or
// the instance's end) to avoid splitting render passes.
static uint32_t IdleFlushMinDraws() {
	static const uint32_t draws = [] {
		const char* value  = std::getenv("KYTY_IDLE_FLUSH_DRAWS");
		const auto  parsed = value != nullptr ? std::strtoul(value, nullptr, 10) : 8ul;
		return static_cast<uint32_t>(std::min(parsed, 65536ul));
	}();
	return draws;
}

void CommandProcessor::MaybeFlushIdleGpu() {
	// A draw or dispatch just recorded writes a page the command processor reads back later (e.g.
	// indirect arguments): submit it now, outside a rendering instance, so that read waits for
	// this producer at most, not for everything recorded until then (KYTY_READBACK_EAGER).
	if (m_renderer.GetBufferCache().TakeEagerFlushRequest(CurrentBuffer().ActiveRenderingSerial() !=
	                                                     0)) {
		BufferFlush();
		return;
	}
	const auto min_draws = IdleFlushMinDraws();
	if (min_draws == 0) {
		return;
	}
	auto&      scheduler = GetScheduler();
	const auto current   = scheduler.CurrentTick();
	if (current != m_idle_flush_tick) {
		// Something else submitted since the last count: restart the bound.
		m_idle_flush_tick  = current;
		m_idle_flush_draws = 0;
	}
	if (++m_idle_flush_draws < min_draws) {
		return;
	}
	const bool in_pass = CurrentBuffer().ActiveRenderingSerial() != 0;
	if (in_pass && m_idle_flush_draws < min_draws * 4u) {
		return;
	}
	auto& master = scheduler.GetMasterSemaphore();
	if (master.KnownGpuTick() + 1u < current) {
		// Refresh the timeline value only every few draws (a driver query).
		if ((m_idle_flush_draws % 4u) != 0) {
			return;
		}
		master.Refresh();
		if (master.KnownGpuTick() + 1u < current) {
			return;
		}
	}
	Profiler::CountFrameEvent(in_pass ? Profiler::FrameEvent::IdleFlushesInPass
	                                  : Profiler::FrameEvent::IdleFlushes);
	BufferFlush();
}

// KYTY_GFX_SLICE_DRAWS (default 128, 0 disables): one GPU thread runs the graphics queue and all
// async compute queues round-robin, and a graphics slice used to run until it completed or
// blocked (up to ~100 ms of CP time per frame), so compute submissions waited that long. Every
// that many draws the graphics CP checks whether another queue has runnable (not suspended)
// work and, only then, ends its slice after the current packet (a slice end flushes).
static uint32_t GfxSliceDraws() {
	static const uint32_t draws = [] {
		const char* value  = std::getenv("KYTY_GFX_SLICE_DRAWS");
		const auto  parsed = value != nullptr ? std::strtoul(value, nullptr, 10) : 128ul;
		return static_cast<uint32_t>(std::min(parsed, 1000000ul));
	}();
	return draws;
}

void CommandProcessor::MaybeYieldSlice() {
	const auto limit = GfxSliceDraws();
	if (limit == 0 || IsAsyncComputeQueue() || g_current_execution == nullptr ||
	    g_gpu_state == nullptr || g_current_processor != this) {
		return;
	}
	if (++m_slice_draws < limit || (m_slice_draws % limit) != 0) {
		return;
	}
	if (!g_gpu_state->HasRunnableComputeWork()) {
		return;
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::GfxSliceYields);
	g_current_execution->m_yield = true;
}

void CommandProcessor::BufferFlushAndWait() {
	KYTY_PROFILER_DETAIL_FUNCTION();
	GetScheduler().FlushAndWait();
}

void CommandProcessor::BufferWait() {
	BufferInit();
	GetScheduler().Finish();
}

void CommandProcessor::ResetDeCe() {
	m_de_count    = 0;
	m_ce_count    = 0;
	m_ce_complete = false;
}

void CommandProcessor::WaitCe() {
	if (m_ce_count <= m_de_count && !m_ce_complete) {
		SuspendPm4();
	}
}

void CommandProcessor::WaitDeDiff(uint32_t diff) {
	EXIT_IF(m_de_count > m_ce_count);
	if (m_ce_count - m_de_count >= diff) {
		SuspendPm4();
	}
}

void CommandProcessor::WaitForRewind(bool valid) {
	if (!valid) {
		SuspendPm4();
	}
}

void CommandProcessor::IncrementDe() {
	m_de_count++;
}

void CommandProcessor::IncrementCe() {
	m_ce_count++;
}

void CommandProcessor::WriteConstRam(uint32_t offset, const uint32_t* src, uint32_t dw_num) {
	memcpy(m_const_ram + offset / 4, src, static_cast<size_t>(dw_num) * 4);
}

void CommandProcessor::DumpConstRam(uint32_t* dst, uint32_t offset, uint32_t dw_num) {
	memcpy(dst, m_const_ram + offset / 4, static_cast<size_t>(dw_num) * 4);
}

bool TestWaitRegMemValue(uint64_t value, uint64_t ref, uint64_t mask, uint32_t func) {
	switch (func) {
		case 0: return true;
		case 1: return (value & mask) < ref;
		case 2: return (value & mask) <= ref;
		case 3: return (value & mask) == ref;
		case 4: return (value & mask) != ref;
		case 5: return (value & mask) >= ref;
		case 6: return (value & mask) > ref;
		default: EXIT("unknown wait compare function: %" PRIu32 "\n", func);
	}

	return false;
}

template <typename T>
void CommandProcessor::WaitRegMem(uint32_t func, const T* addr, T ref, T mask, uint32_t poll,
                                  uint32_t wait_op) {
	EXIT_IF(addr == nullptr);
	if ((wait_op & ~1u) != 0) {
		EXIT("unsupported wait_reg_mem operation: 0x%08" PRIx32 "\n", wait_op);
	}

	(void)poll;
	const auto value  = ReadGuestForCp<T>(reinterpret_cast<uint64_t>(addr));
	const bool passed = TestWaitRegMemValue(value, ref, mask, func);
	if (HangTrace::CpTraceEnabled()) {
		// One row per wait: the first failed evaluation, and the pass (aux = failed retries).
		struct WaitTrace {
			uint64_t address = 0;
			uint64_t ref     = 0;
			int64_t  retries = -1; // -1: no failed evaluation pending
		};
		static std::unordered_map<const CommandProcessor*, WaitTrace> traces; // GPU thread
		auto&      trace   = traces[this];
		const auto address = reinterpret_cast<uint64_t>(addr);
		const bool same    = trace.retries >= 0 && trace.address == address &&
		                  trace.ref == static_cast<uint64_t>(ref);
		if (!passed && same) {
			trace.retries++;
		} else {
			HangTrace::CpEvent event;
			event.event   = passed ? "wait-pass" : "wait-fail";
			event.address = address;
			event.value   = static_cast<uint64_t>(value);
			event.ref     = static_cast<uint64_t>(ref);
			event.mask    = static_cast<uint64_t>(mask);
			event.aux     = passed ? (same ? trace.retries : 0) : static_cast<int64_t>(func);
			event.size    = sizeof(T);
			HangTrace::RecordCp(event);
			trace = passed ? WaitTrace {} : WaitTrace {address, static_cast<uint64_t>(ref), 0};
		}
	}
	if (!passed) {
		// Waiting on a deferred label: it is written only after its tick completes, so that
		// tick must be submitted before this queue suspends (the slice-end flush would do it
		// too; flushing here also covers a label recorded earlier in this slice).
		if (g_gpu_state != nullptr) {
			if (const auto tick =
			        g_gpu_state->DeferredLabelTick(reinterpret_cast<uint64_t>(addr), sizeof(T));
			    tick != 0) {
				Profiler::CountFrameEvent(Profiler::FrameEvent::WaitRegMemDeferredLabel);
				if (tick >= GetScheduler().CurrentTick()) {
					Profiler::CountFrameEvent(Profiler::FrameEvent::WaitRegMemDeferredLabelFlushes);
					BufferFlush();
				}
			}
		}
		SuspendPm4();
	}
}

template void CommandProcessor::WaitRegMem<uint32_t>(uint32_t, const uint32_t*, uint32_t, uint32_t,
                                                     uint32_t, uint32_t);
template void CommandProcessor::WaitRegMem<uint64_t>(uint32_t, const uint64_t*, uint64_t, uint64_t,
                                                     uint32_t, uint32_t);

void CommandProcessor::WriteData(uint32_t* dst, const uint32_t* src, uint32_t dw_num,
                                 uint32_t write_control) {
	const uint32_t dst_sel = ((write_control >> 30u) & 0x1u) | ((write_control >> 7u) & 0x1eu);
	const bool     write_one_address = ((write_control >> 16u) & 0x1u) != 0;

	switch (dst_sel) {
		case 0:
		case 2:
		case 4:
		case 5:
		case 6: break;
		default: EXIT("unsupported writeData destination selector 0x%02" PRIx32 "\n", dst_sel);
	}
	if (dw_num == 0) {
		return;
	}

	// KYTY_WRITE_DATA_GPU (default on): a destination owned by recorded-but-unexecuted GPU work is
	// written on the GPU timeline, after that work, instead of by the CPU now (which drained the
	// GPU through a fault, or was later overwritten by the GPU data's readback).
	static const bool gpu_writes = [] {
		const auto* value = std::getenv("KYTY_WRITE_DATA_GPU");
		return value == nullptr || std::strcmp(value, "0") != 0;
	}();
	if (gpu_writes) {
		const auto address = reinterpret_cast<uint64_t>(dst);
		auto&      cache   = m_renderer.GetBufferCache();
		const bool written =
		    write_one_address
		        ? cache.TryWriteDataGpu(address, src + (dw_num - 1u), sizeof(uint32_t))
		        : cache.TryWriteDataGpu(address, src, uint64_t {dw_num} * sizeof(uint32_t));
		if (written) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::WriteDataGpu);
			TraceCpLabel("wd-gpu", dst, src[write_one_address ? dw_num - 1u : 0u],
			             uint64_t {dw_num} * 4u);
			return;
		}
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::WriteDataCpu);
	TraceCpLabel("wd-cpu", dst, src[write_one_address ? dw_num - 1u : 0u], uint64_t {dw_num} * 4u);

	if (write_one_address) {
		for (uint32_t i = 0; i < dw_num; i++) {
			dst[0] = src[i];
		}
	} else {
		memcpy(dst, src, static_cast<size_t>(dw_num) * sizeof(uint32_t));
	}
}

void CommandProcessor::WriteReferenceClock(uint64_t dst_address, uint32_t num_bytes) {
	if (dst_address == 0 || (num_bytes != sizeof(uint32_t) && num_bytes != sizeof(uint64_t)) ||
	    (dst_address & (num_bytes - 1u)) != 0) {
		EXIT("invalid reference-clock copy, dst=0x%016" PRIx64 " size=%u\n", dst_address,
		     num_bytes);
	}
	const auto value = Sync::ReadReferenceClock();
	std::memcpy(reinterpret_cast<void*>(dst_address), &value, num_bytes);
	static std::atomic<uint32_t> clock_log_count {0};
	if (clock_log_count.fetch_add(1) < 64) {
		LOGF("\t copy_data reference clock: dst=0x%016" PRIx64 " value=0x%016" PRIx64
		     " size=%u\n",
		     dst_address, value, num_bytes);
	}
}

void CommandProcessor::DmaData(uint8_t engine, uint8_t dst_sel, uint8_t dst_cache_policy,
                               uint64_t dst_address_or_offset, uint8_t src_sel,
                               uint8_t  src_cache_policy,
                               uint64_t src_address_or_offset_or_immediate, uint32_t num_bytes,
                               uint8_t wait_for_previous, uint8_t write_confirm,
                               uint8_t block_engine) {
	EXIT_NOT_IMPLEMENTED(engine > 1);
	if (num_bytes == 0) {
		return;
	}
	EXIT_NOT_IMPLEMENTED(dst_cache_policy > 3);
	EXIT_NOT_IMPLEMENTED(src_cache_policy > 3);
	EXIT_NOT_IMPLEMENTED(wait_for_previous > 1);
	EXIT_NOT_IMPLEMENTED(write_confirm > 1);
	EXIT_NOT_IMPLEMENTED(block_engine > 1);
	if (static_cast<uint32_t>(dst_address_or_offset) == 0x3022cu) {
		return;
	}
	auto decode_gds = [](uint8_t selector, bool& is_gds) {
		switch (selector) {
			case 0:
			case 3: is_gds = false; return true;
			case 1: is_gds = true; return true;
			default: return false;
		}
	};
	if (dst_sel == 2) {
		// kNowhere discards the GL2 prefetch destination without a guest-visible write.
		if (src_sel != 3) {
			EXIT("unsupported dmaData nowhere source selector 0x%02" PRIx8 "\n", src_sel);
		}
		return;
	}
	bool dst_gds = false;
	if (!decode_gds(dst_sel, dst_gds)) {
		EXIT("unsupported dmaData destination selector 0x%02" PRIx8 "\n", dst_sel);
	}
	auto& buffer_cache = m_renderer.GetBufferCache();
	// Hang-trace CP rows for GDS transfers (the append/consume counter resets and readbacks of
	// passes such as Astro Bot's GI ray-bundle linked lists): address = GDS byte offset, value =
	// fill value or memory address, ref = 0 fill, 1 memory to GDS, 2 GDS to memory.
	const auto trace_gds = [&](uint64_t gds_offset, uint64_t value, uint64_t direction) {
		if (HangTrace::CpTraceEnabled()) {
			HangTrace::CpEvent event;
			event.event   = "gds-dma";
			event.address = gds_offset;
			event.value   = value;
			event.ref     = direction;
			event.size    = num_bytes;
			HangTrace::RecordCp(event);
		}
	};
	if (src_sel == 2) {
		if (dst_gds) {
			trace_gds(dst_address_or_offset, src_address_or_offset_or_immediate & 0xffffffffu, 0);
		}
		buffer_cache.FillBuffer(
		    dst_address_or_offset, num_bytes,
		    static_cast<uint32_t>(src_address_or_offset_or_immediate & 0xffffffffu), dst_gds);
		return;
	}
	bool src_gds = false;
	if (!decode_gds(src_sel, src_gds)) {
		EXIT("unsupported dmaData source selector 0x%02" PRIx8 "\n", src_sel);
	}
	if (src_gds && dst_gds) {
		EXIT("unsupported dmaData GDS-to-GDS copy\n");
	}
	if (dst_gds) {
		trace_gds(dst_address_or_offset, src_address_or_offset_or_immediate, 1);
	} else if (src_gds) {
		trace_gds(src_address_or_offset_or_immediate, dst_address_or_offset, 2);
	}
	buffer_cache.CopyBuffer(dst_address_or_offset, src_address_or_offset_or_immediate, num_bytes,
	                        dst_gds, src_gds);
}

void GuestGpu::Enqueue(Submission submission) {
	EXIT_IF(submission.queue_id >= QueueCount);
	if (HangTrace::Enabled()) {
		submission.enqueue_ns = HangTrace::NowNs();
	}
	Common::LockGuard lock(m_queue_mutex);
	EXIT_IF(!m_accepting);
	submission.sequence    = m_next_submission_sequence++;
	submission.frame_fence = FrameFenceEnabled() ? m_done_boundary : 0;
	if (HangTrace::CpTraceEnabled()) {
		HangTrace::CpEvent event;
		event.event   = submission.type == SubmissionType::Compute   ? "admit-compute"
		                : submission.type == SubmissionType::Graphics ? "admit-graphics"
		                                                               : "admit-flip";
		event.address = reinterpret_cast<uint64_t>(submission.commands.data());
		event.value   = submission.frame_fence;
		event.ref     = static_cast<uint64_t>(m_done_num.load());
		event.size    = submission.commands.size() * sizeof(uint32_t);
		event.queue   = submission.queue_id;
		event.seq     = submission.sequence;
		HangTrace::RecordCp(event);
	}
	m_in_flight.insert(submission.sequence);
	m_queues[submission.queue_id].push_back(std::move(submission));
	m_submission_count++;
	m_work_available.Signal();
}

void GuestGpu::WaitForIdle() {
	Common::LockGuard lock(m_queue_mutex);
	while (m_processing || !m_commands.empty() || m_submission_count != 0) {
		m_idle.Wait(&m_queue_mutex);
	}
}

void GuestGpu::ThreadRun(void* data) {
	auto* gpu = static_cast<GuestGpu*>(data);
	EXIT_IF(gpu == nullptr);
	KYTY_PROFILER_THREAD("Thread_Gpu");
	// The command processor is the frame-rate limit; keep it ahead of guest spin loops.
	Common::RaiseCurrentThreadPriority();
	// KYTY_CPU_RESERVE: its own physical core.
	Common::PlaceCurrentThread(Common::ThreadRole::Cp);
	g_gpu_thread = true;
	g_gpu_state  = gpu;

	const bool wakeups       = CpWakeupsEnabled();
	uint64_t   spin_deadline = 0; // 0: not spinning on blocked queues
	for (;;) {
		Submission                   submission;
		Common::UniqueFunction<void> command;
		bool                         has_submission = false;
		bool                         should_stop    = false;
		{
			Common::LockGuard lock(gpu->m_queue_mutex);
			while (gpu->m_commands.empty() && gpu->m_submission_count == 0 && !gpu->m_stopping) {
				gpu->m_processing = false;
				gpu->m_idle.Signal();
				gpu->m_work_available.Wait(&gpu->m_queue_mutex);
			}
			if (gpu->m_stopping && gpu->m_commands.empty() && gpu->m_submission_count == 0) {
				gpu->m_processing = false;
				gpu->m_idle.SignalAll();
				gpu->m_done_progress.SignalAll();
				should_stop = true;
			} else if (!gpu->m_commands.empty()) {
				command = std::move(gpu->m_commands.front());
				gpu->m_commands.pop_front();
				EXIT_IF(gpu->m_pending_commands.fetch_sub(1, std::memory_order_acq_rel) == 0);
				gpu->m_processing = true;
			} else {
				int  selected_queue = -1;
				bool fence_held     = false;
				for (uint32_t offset = 0; offset < QueueCount; offset++) {
					const auto id    = (gpu->m_next_queue + offset) % QueueCount;
					auto&      queue = gpu->m_queues[id];
					if (queue.empty() || queue.front().blocked) {
						continue;
					}
					if (!gpu->FrameFencePassed(queue.front())) {
						if (queue.front().fence_hold_ns == 0) {
							queue.front().fence_hold_ns = CpNowNs();
						}
						fence_held = true;
						continue;
					}
					if (queue.front().fence_hold_ns != 0 &&
					    CpNowNs() - queue.front().fence_hold_ns > FrameFenceTimeoutNs) {
						static std::atomic_bool logged {false};
						if (!logged.exchange(true)) {
							LOGF("CP frame fence: queue %u waited over 2 s for the previous frame; "
							     "running it anyway (KYTY_FRAME_FENCE)\n",
							     id);
						}
					}
					selected_queue = static_cast<int>(id);
					break;
				}
				if (fence_held && selected_queue >= 0) {
					// The fence changed which queue runs next.
					Profiler::CountFrameEvent(Profiler::FrameEvent::FrameFenceHolds);
				}
				if (selected_queue < 0) {
					gpu->m_processing = false;
					if (!wakeups) {
						gpu->m_work_available.WaitFor(&gpu->m_queue_mutex, 100);
					} else {
						const auto now = CpNowNs();
						if (spin_deadline == 0) {
							spin_deadline = now + CpBlockedSpinNs();
						}
						if (now < spin_deadline) {
							// Every queue is suspended: retry shortly without sleeping.
							Profiler::CountFrameEvent(Profiler::FrameEvent::CpBlockedSpins);
							gpu->m_queue_mutex.Unlock();
							std::this_thread::yield();
							gpu->m_queue_mutex.Lock();
						} else {
							// Completions, deferred labels and flips signal this condition.
							Profiler::CountFrameEvent(Profiler::FrameEvent::CpBlockedSleeps);
							gpu->m_work_available.WaitFor(&gpu->m_queue_mutex, 1000);
						}
					}
					gpu->m_has_blocked.store(false, std::memory_order_release);
					for (auto& queue: gpu->m_queues) {
						if (!queue.empty()) {
							queue.front().blocked = false;
						}
					}
					continue;
				}
				auto& queue = gpu->m_queues[static_cast<uint32_t>(selected_queue)];
				submission  = std::move(queue.front());
				queue.pop_front();
				gpu->m_submission_count--;
				gpu->m_next_queue = (static_cast<uint32_t>(selected_queue) + 1) % QueueCount;
				gpu->m_processing = true;
				has_submission    = true;
			}
		}
		if (should_stop) {
			gpu->m_gfx_cp->BufferWait();
			// Deferred label writes still queued on the completion runner write the backing
			// directly once commands are refused; finish them while this GuestGpu exists.
			gpu->m_renderer.GetCommandScheduler().DrainPriorityOperations();
			g_gpu_state  = nullptr;
			g_gpu_thread = false;
			return;
		}

		if (command) {
			EXIT_IF(g_current_processor != nullptr);
			command();
			SyncEpoch::Advance();
			spin_deadline = 0;

			Common::LockGuard lock(gpu->m_queue_mutex);
			gpu->m_processing = false;
			if (gpu->m_commands.empty() && gpu->m_submission_count == 0) {
				gpu->m_idle.SignalAll();
			}
			continue;
		}

		EXIT_IF(!has_submission);
		uint64_t slice_start = 0;
		if (HangTrace::Enabled()) {
			slice_start = HangTrace::NowNs();
			if (!submission.started && submission.enqueue_ns != 0) {
				HangTrace::RecordQueueWait(submission.queue_id, slice_start - submission.enqueue_ns);
			}
		}
		HangTrace::SetCpContext(submission.queue_id, submission.sequence);
		const bool complete = gpu->Process(submission);
		if (HangTrace::Enabled()) {
			HangTrace::RecordQueueBusy(submission.queue_id, HangTrace::NowNs() - slice_start,
			                           complete);
		}
		if (HangTrace::CpTraceEnabled()) {
			HangTrace::CpEvent event;
			event.event = "slice";
			// 0 complete, 1 suspended (blocked), 2 yielded
			event.aux = complete ? 0 : (submission.command_execution.Yielded() ? 2 : 1);
			event.value = submission.slice_progress ? 1 : 0;
			HangTrace::RecordCp(event);
		}
		HangTrace::SetCpContext(UINT32_MAX, 0);

		if (complete || submission.slice_progress) {
			spin_deadline = 0;
		}
		Common::LockGuard lock(gpu->m_queue_mutex);
		if (!complete && submission.command_execution.Yielded()) {
			// Yielded to other queues: runnable again in round-robin order.
			gpu->m_queues[submission.queue_id].push_front(std::move(submission));
			gpu->m_submission_count++;
		} else if (!complete) {
			submission.blocked = true;
			gpu->m_has_blocked.store(true, std::memory_order_release);
			gpu->m_queues[submission.queue_id].push_front(std::move(submission));
			gpu->m_submission_count++;
		} else {
			for (auto& queue: gpu->m_queues) {
				if (!queue.empty()) {
					queue.front().blocked = false;
				}
			}
			gpu->m_in_flight.erase(submission.sequence);
			if (gpu->m_done_waiters != 0) {
				gpu->m_done_progress.SignalAll();
			}
		}
		gpu->m_processing = false;
		if (gpu->m_commands.empty() && gpu->m_submission_count == 0) {
			gpu->m_idle.SignalAll();
		}
	}
}

bool GuestGpu::Process(Submission& submission) {
	const bool first_slice = !submission.started;
	auto& cp = GetProcessor(submission.queue_id);
	// A new submission, or a slice after other queues ran (syncEpoch.h).
	SyncEpoch::Advance();

	if (first_slice && submission.reset_processor) {
		cp.Reset();
	}
	if (first_slice && RepeatTrace::Enabled()) {
		// KYTY_CP_REPEAT_TRACE: a guest frame starts with the processor reset after
		// sceAgcSuspendPoint; every submission's content is hashed.
		if (submission.reset_processor && submission.type != SubmissionType::Compute) {
			RepeatTrace::OnFrameBoundary();
		}
		RepeatTrace::OnSubmission(submission.queue_id, submission.sequence, submission.commands,
		                          submission.constant_commands);
	}

	if (first_slice) {
		submission.started = true;
		cp.SetSubmitId(++m_submit_id);
		cp.ResetDeCe();
		cp.SetFlip({});
	}

	cp.BufferInit();
	bool complete = true;

	switch (submission.type) {
		case SubmissionType::Graphics: {
			bool progressed = false;
			submission.constant_complete |= submission.constant_commands.empty();
			for (;;) {
				bool round_progress = false;
				if (!submission.constant_complete) {
					submission.constant_complete =
					    cp.Process(submission.constant_execution, submission.constant_commands) ==
					    Pm4ProcessResult::Complete;
					round_progress |= submission.constant_execution.MadeProgress();
				}
				cp.SetCeComplete(submission.constant_complete);
				if (!submission.command_complete) {
					submission.command_complete =
					    cp.Process(submission.command_execution, submission.commands) ==
					    Pm4ProcessResult::Complete;
					round_progress |= submission.command_execution.MadeProgress();
				}
				progressed |= round_progress;
				complete = submission.command_complete && submission.constant_complete;
				if (complete || !round_progress || submission.command_execution.Yielded()) {
					break;
				}
			}
			submission.slice_progress = progressed;
			if (progressed) {
				if (complete) {
					m_renderer.RunGarbageCollector();
				}
				cp.BufferFlush();
			} else if (complete) {
				m_renderer.RunGarbageCollector();
			}
			break;
		}
		case SubmissionType::Compute: {
			const auto      num_dw = static_cast<uint32_t>(submission.commands.size());
			const auto*     buffer = submission.commands.data();
			static uint32_t compute_batch_log_count = 0;
			if (first_slice && num_dw <= 128 && compute_batch_log_count++ < 32) {
				LOGF("compute direct batch: data=0x%016" PRIx64 ", num_dw=%" PRIu32 "\n",
				     reinterpret_cast<uint64_t>(buffer), num_dw);
				for (uint32_t i = 0; i < std::min<uint32_t>(num_dw, 16); i++) {
					LOGF("\t compute[%02" PRIu32 "] = 0x%08" PRIx32 "\n", i, buffer[i]);
				}
			}
			if (first_slice) {
				GraphicsDbgDumpDcb("cc", num_dw, buffer);
			}
			complete = cp.Process(submission.command_execution, submission.commands) ==
			           Pm4ProcessResult::Complete;
			submission.slice_progress = submission.command_execution.MadeProgress();
			if (submission.command_execution.MadeProgress()) {
				if (complete) {
					m_renderer.RunGarbageCollector();
				}
				cp.BufferFlush();
			} else if (complete) {
				m_renderer.RunGarbageCollector();
			}
			break;
		}
		case SubmissionType::FlipPreparation:
			m_renderer.RunGarbageCollector();
			cp.PrepareCpuFlip(submission.flip_request_id);
			break;
	}

	return complete;
}

Pm4ProcessResult CommandProcessor::Process(Pm4Execution&             execution,
                                           std::span<const uint32_t> commands) {
	KYTY_PROFILER_BLOCK("CommandProcessor::Process");
	EXIT_IF(g_current_execution != nullptr);
	EXIT_IF(commands.size() > UINT32_MAX);
	if (execution.m_buffer_stack.empty() && !commands.empty()) {
		execution.m_buffer_stack.push_back({commands});
	}
	execution.m_suspended     = false;
	execution.m_made_progress = false;
	execution.m_yield         = false;
	execution.m_yielded       = false;
	m_slice_draws             = 0;

	struct ExecutionScope {
		ExecutionScope(CommandProcessor& processor, Pm4Execution& execution)
		    : previous_processor(g_current_processor), previous_execution(g_current_execution) {
			g_current_processor = &processor;
			g_current_execution = &execution;
		}
		~ExecutionScope() {
			g_current_processor = previous_processor;
			g_current_execution = previous_execution;
		}

		CommandProcessor* previous_processor;
		Pm4Execution*     previous_execution;
	} execution_scope(*this, execution);

	ProcessPm4(execution);
	// Draw-prep: every draw parsed in this slice is committed before the slice ends.
	DrainPreparedDraws();
	return execution.m_buffer_stack.empty() ? Pm4ProcessResult::Complete
	                                        : Pm4ProcessResult::Blocked;
}

// KYTY_CP_REPEAT_TRACE: the guest address of the packet whose handler is running (a draw packet
// when called from DrawIndex/DrawIndexAuto).
void CommandProcessor::NoteRepeatTraceDrawPacket() {
	const uint32_t* packet = nullptr;
	if (g_current_execution != nullptr && !g_current_execution->m_buffer_stack.empty()) {
		const auto& cursor = g_current_execution->m_buffer_stack.back();
		packet             = cursor.commands.data() + cursor.offset_dw;
	}
	RepeatTrace::NoteDrawPacket(packet);
}

void CommandProcessor::ProcessIndirectBuffer(std::span<const uint32_t> commands, bool chain) {
	EXIT_IF(g_current_execution == nullptr);
	if (RepeatTrace::Enabled()) {
		RepeatTrace::OnIndirectBuffer(commands, chain);
	}
	EXIT_IF(!g_current_execution->m_next_buffer.empty());
	g_current_execution->m_next_buffer = commands;
	g_current_execution->m_chain       = chain;
}

void CommandProcessor::SuspendPm4() {
	EXIT_IF(g_current_execution == nullptr);
	g_current_execution->m_suspended = true;
}

void CommandProcessor::ProcessPm4(Pm4Execution& execution) {
	while (!execution.m_buffer_stack.empty()) {
		if (g_gpu_state != nullptr) {
			if (m_draw_prep != nullptr && m_draw_prep->Pending()) {
				// Draw-prep: service commands (readbacks, unmaps) observe every parsed draw, so
				// they only run with an empty window.
				if (g_gpu_state->HasPendingCommands()) {
					m_draw_prep->Drain();
					g_gpu_state->ProcessCommands();
				}
			} else {
				g_gpu_state->ProcessCommands();
			}
		}
		auto& cursor = execution.m_buffer_stack.back();
		EXIT_IF(cursor.offset_dw > cursor.commands.size());
		if (cursor.offset_dw == cursor.commands.size()) {
			execution.m_buffer_stack.pop_back();
			continue;
		}

		// Placement samples (common/cpuPlacement.h), every 256th packet.
		if ((++m_placement_packets & 255u) == 0u) {
			Common::SamplePlacement(Common::ThreadRole::Cp);
		}
		const auto* const packet        = cursor.commands.data() + cursor.offset_dw;
		const auto        total_dw      = static_cast<uint32_t>(cursor.commands.size());
		const auto        remaining_dw  = total_dw - cursor.offset_dw;
		const auto        packet_header = packet[0];
		const auto        opcode        = (packet_header >> 8u) & 0xffu;
		EXIT_NOT_IMPLEMENTED(remaining_dw > total_dw);

		if (packet_header == 0x80000000u) {
			cursor.offset_dw++;
			execution.m_made_progress = true;
			continue;
		}

		EXIT_NOT_IMPLEMENTED(remaining_dw < 2);

		if (GraphicsRunDebugDumpEnabled()) {
			LOGF("CP packet: offset=0x%05" PRIx32 " cmd_id=0x%08" PRIx32 " op=0x%02" PRIx32
			     " len=%" PRIu32 "\n",
			     total_dw - remaining_dw, packet_header, opcode, KYTY_PM4_LEN(packet_header));
		}

		if ((packet_header & 1u) != 0) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::PredicatedPackets);
		}
		if ((packet_header & 1u) != 0 && ShouldSkipPredicatedPackets()) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::PredicatedPacketsSkipped);
			auto packet_dw = KYTY_PM4_LEN(packet_header);
			EXIT_NOT_IMPLEMENTED(packet_dw == 0 || packet_dw > remaining_dw);
			static std::atomic<uint32_t> skip_log_count {0};
			if (skip_log_count.fetch_add(1) < 2048) {
				LOGF("\t predicated skip: op=0x%02" PRIx32 ", r=0x%02" PRIx32 ", len=%" PRIu32
				     ", packet=0x%016" PRIx64 ", cmd_id=0x%08" PRIx32 "\n",
				     opcode, KYTY_PM4_R(packet_header), packet_dw,
				     reinterpret_cast<uint64_t>(packet), packet_header);
			}
			if (opcode == Pm4::IT_NOP && KYTY_PM4_R(packet_header) == Pm4::R_RELEASE_MEM &&
			    packet_dw >= 7) {
				static std::atomic<uint32_t> log_count {0};
				if (log_count.fetch_add(1) < 128) {
					const auto dst = packet[3] | (static_cast<uint64_t>(packet[4]) << 32u);
					const auto val = packet[5] | (static_cast<uint64_t>(packet[6]) << 32u);
					LOGF("\t predicated skip: R_RELEASE_MEM dst=0x%016" PRIx64
					     ", value=0x%016" PRIx64 ", action=0x%08" PRIx32
					     ", gcr/data/int=0x%08" PRIx32 "\n",
					     dst, val, packet[1], packet[2]);
				}
			}
			cursor.offset_dw += packet_dw;
			execution.m_made_progress = true;
			continue;
		}

		auto handler = g_cp_op_func[opcode];

		if (handler == nullptr) {
			const auto offset = total_dw - remaining_dw;
			LOGF("unknown PM4 packet: data=0x%016" PRIx64 ", num_dw=%" PRIu32
			     ", offset=0x%05" PRIx32 ", current=0x%016" PRIx64 "\n",
			     reinterpret_cast<uint64_t>(packet - offset), total_dw, offset,
			     reinterpret_cast<uint64_t>(packet));
			const auto  dump_begin = (offset > 8 ? offset - 8 : 0);
			const auto  dump_end   = std::min<uint32_t>(total_dw, offset + 16);
			auto* const base       = packet - offset;
			for (uint32_t i = dump_begin; i < dump_end; i++) {
				LOGF("\t%05" PRIx32 "%s %08" PRIx32 "\n", i, (i == offset ? ":" : " "), base[i]);
			}
			EXIT("unknown op\n\t%05" PRIx32 ":\n\tcmd_id = %08" PRIx32 "\n",
			     total_dw - remaining_dw, packet_header);
		}

		// KYTY_SYNC_EPOCH (syncEpoch.h): every fence but a register load from memory is a point
		// where guest CPU writes must become visible to the GPU work that follows it.
		if (SyncEpoch::Enabled() &&
		    DrawPrep::AdvancesSyncEpoch(packet_header & ~1u, packet + 1, remaining_dw)) {
			SyncEpoch::Advance();
		}
		if (DrawPrep::PacketHookActive()) [[unlikely]] {
			// Window fences commit every pending draw before their handler runs.
			if (auto* engine = DrawPrepEngine(); engine != nullptr) {
				const auto header       = packet_header & ~1u;
				auto       packet_class = DrawPrep::ClassifyPacket(header, packet + 1, remaining_dw);
				auto       fence_kind   = DrawPrep::FenceKind::Other;
				if (packet_class == DrawPrep::PacketClass::Fence) {
					fence_kind = DrawPrep::ClassifyFence(header);
					// Register loads from clean guest memory keep the window open (drawPrep.h).
					DrawPrep::RegisterIndirectRange pairs;
					if (fence_kind == DrawPrep::FenceKind::RegIndirect &&
					    DrawPrep::RegisterIndirectWindowEnabled() &&
					    DrawPrep::RegisterIndirectPairs(header, packet + 1, remaining_dw, pairs) &&
					    (pairs.size == 0 ||
					     LibKernel::Memory::IsGpuCleanForRead(pairs.address, pairs.size))) {
						packet_class = DrawPrep::PacketClass::WindowSafe;
						Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepRegIndirectKept);
						DrawPrep::GetTotals().register_indirect_kept.fetch_add(
						    1, std::memory_order_relaxed);
					}
				}
				engine->OnPacket(packet_class, fence_kind);
			}
		}
		const auto packet_dw =
		    handler(*this, packet_header & ~1u, packet + 1, remaining_dw, total_dw) + 1;
		EXIT_IF(packet_dw > remaining_dw);
		if (execution.m_suspended) {
			return;
		}
		cursor.offset_dw += packet_dw;
		execution.m_made_progress = true;
		if (m_deferred_eop_flushes != 0 && ++m_packets_since_eop_request >= EopFlushPacketLimit()) {
			// Bound how long a batched end-of-pipe interrupt can wait inside a long slice.
			BufferFlush();
		}
		if (!execution.m_next_buffer.empty()) {
			// Chains and taken branches reuse the fetcher; only calls retain a return cursor.
			if (execution.m_chain) {
				cursor = {execution.m_next_buffer};
			} else {
				execution.m_buffer_stack.push_back({execution.m_next_buffer});
			}
			execution.m_next_buffer = {};
		}
		if (execution.m_yield) {
			execution.m_yield   = false;
			execution.m_yielded = true;
			return;
		}
	}
}

void CommandProcessor::SetIndexType(uint32_t index_type_and_size) {
	m_index_type_and_size = index_type_and_size & 0x3u;
}

void CommandProcessor::SetIndexBaseAddress(uint64_t index_base_addr) {
	m_index_base_addr = index_base_addr;
}

void CommandProcessor::SetIndexBufferSize(uint32_t index_buffer_size) {
	m_index_buffer_size = index_buffer_size;
}

void CommandProcessor::SetDrawIndirectArgsBaseAddress(uint64_t draw_indirect_args_base_addr) {
	m_draw_indirect_args_base_addr = draw_indirect_args_base_addr;
}

void CommandProcessor::SetDispatchIndirectArgsBaseAddress(
    uint64_t dispatch_indirect_args_base_addr) {
	m_dispatch_indirect_args_base_addr = dispatch_indirect_args_base_addr;
}

void CommandProcessor::SetNumInstances(uint32_t num_instances) {
	if (num_instances == 0) {
		num_instances = 1;
	}

	m_num_instances = num_instances;
	m_pending_num_instances.clear();
}

void CommandProcessor::SetPredication(uint32_t condition, uint32_t op, uint32_t wait_op,
                                      const volatile void* address, uint32_t count_in_dwords) {
	(void)count_in_dwords;
	uint64_t value = 0;

	switch (op) {
		case 0x00:
			m_predicate_skip = false;
			return;
		case 0x01: {
			EXIT_NOT_IMPLEMENTED(address == nullptr);
			Profiler::CountFrameEvent(Profiler::FrameEvent::OcclusionPredicates);
			// Native dumps are published to guest memory when their GPU work completes. Make every
			// recorded dump visible before this CPU-side read.
			if (OcclusionCounter::Enabled() &&
			    m_renderer.GetOcclusionCounter().HasUnpublishedDumps()) {
				Profiler::ScopedGpuWaitReason wait_reason(Profiler::FrameWait::GpuWaitOcclusion);
				BufferWait();
				if (OcclusionCounter::PriorityPublication()) {
					// Publications run on the completion runner: wait for them too.
					GetScheduler().WaitPriorityOperations(GetScheduler().CurrentTick());
				}
			}
			// One begin/end pair per DB; bit 63 marks each counter ready.
			constexpr uint64_t ready_bit = 1ull << 63u;
			const auto* results = reinterpret_cast<const volatile uint64_t*>(address);
			for (uint32_t db = 0; db < 16u; db++) {
				const auto begin = results[db * 2u];
				const auto end   = results[db * 2u + 1u];
				if ((begin & end & ready_bit) == 0) {
					Profiler::CountFrameEvent(Profiler::FrameEvent::OcclusionPredicatesPending);
					if (wait_op == 0) {
						SuspendPm4();
					} else {
						m_predicate_skip = false;
					}
					return;
				}
				value += end - begin;
			}
		} break;
		case 0x03:
			if (wait_op != 0) {
				Profiler::ScopedGpuWaitReason wait_reason(Profiler::FrameWait::GpuWaitPredication);
				BufferFlushAndWait();
				// Labels deferred to completion (defer-label / KYTY_LABEL_MODE=completion) are
				// written by commands the completion runner posts: run them before reading.
				if (g_gpu_state != nullptr &&
				    g_gpu_state->DeferredLabelTick(reinterpret_cast<uint64_t>(address),
				                                   sizeof(uint64_t)) != 0) {
					GetScheduler().WaitPriorityOperations(GetScheduler().CurrentTick());
					g_gpu_state->ProcessCommands();
				}
			}
			EXIT_NOT_IMPLEMENTED(address == nullptr);
			value = ReadGuestForCp<uint64_t>(reinterpret_cast<uint64_t>(address));
			break;
		default: EXIT("unknown predication op: 0x%08" PRIx32 "\n", op);
	}
	switch (condition) {
		case 0x00: m_predicate_skip = (value != 0); break;
		case 0x01: m_predicate_skip = (value == 0); break;
		default: EXIT("unknown predication condition: 0x%08" PRIx32 "\n", condition);
	}
	if (op == 0x01 && HangTrace::Enabled()) {
		HangTrace::OcclusionEvent event;
		event.event     = "predicate";
		event.address   = reinterpret_cast<uint64_t>(address);
		event.value     = value;
		event.condition = condition;
		event.skip      = m_predicate_skip;
		HangTrace::RecordOcclusion(event);
	}
	if (op == 0x03) {
		static std::atomic<uint32_t> log_count {0};
		if (log_count.fetch_add(1) < 128) {
			LOGF("\t bool predication: addr=0x%016" PRIx64 ", value=0x%016" PRIx64
			     ", condition=%" PRIu32 ", skip=%u, wait_op=%" PRIu32 "\n",
			     reinterpret_cast<uint64_t>(address), value, condition,
			     m_predicate_skip ? 1u : 0u, wait_op);
		}
	}
}

void CommandProcessor::DrawIndex(DrawIndexArgs args) {
	args.index_type_and_size = m_index_type_and_size;
	if (args.instance_count == 0) {
		if (!m_pending_num_instances.empty()) {
			// The inherited count may be GPU data an earlier (pending) draw writes.
			DrainPreparedDraws();
		}
		args.instance_count = NumInstances();
	}
	if (GraphicsRunDebugDumpEnabled() && (args.base_vertex != 0 || args.first_instance != 0)) {
		LOGF("\t draw indexed offsets: base_vertex = %" PRId32 ", first_instance = %" PRIu32 "\n",
		     args.base_vertex, args.first_instance);
	}
	if (RepeatTrace::Enabled()) {
		NoteRepeatTraceDrawPacket();
	}
	if (TrySubmitPreparedDraw(&args, nullptr)) {
		// Draw-prep: the engine runs MaybeFlushIdleGpu after it records (commits) each draw. The
		// slice may count the draw now: a yield ends the slice after this packet, and the slice
		// end commits the whole window before anything else runs.
		MaybeYieldSlice();
		return;
	}
	if (RepeatTrace::Enabled()) {
		(void)RepeatTrace::TakeDrawPacket();
		RepeatTrace::OnUnpreparedDraw();
	}
	m_renderer.GetRenderExecutor().DrawIndex(m_submit_id, CurrentBuffer(), args);
	MaybeFlushIdleGpu();
	MaybeYieldSlice();
}

void CommandProcessor::DrawIndexOffset(uint32_t index_offset, uint32_t index_count) {
	uint64_t index_size = 0;
	switch (m_index_type_and_size) {
		case 0: index_size = 2; break;
		case 1: index_size = 4; break;
		case 2: index_size = 1; break;
		default: EXIT("unknown index_type_and_size: %u\n", m_index_type_and_size);
	}

	auto* index_addr = reinterpret_cast<const void*>(
	    m_index_base_addr + static_cast<uint64_t>(index_offset) * index_size);

	DrawIndex({.index_count = index_count, .index_addr = index_addr});
}

uint32_t CommandProcessor::NumInstances() {
	if (m_pending_num_instances.empty()) {
		return m_num_instances;
	}
	// A draw without its own count inherits instance_count of the last record a native indirect
	// draw drew. Reading it synchronizes (drains) as the CPU-read path would have at the
	// indirect draw, but reads the bytes now: a record rewritten in between yields its newer
	// value (KYTY_INDIRECT_VALIDATE=1 reports that case).
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawIndirectInstanceReads);
	for (auto it = m_pending_num_instances.rbegin(); it != m_pending_num_instances.rend(); ++it) {
		const auto& pending = *it;
		uint32_t    count   = pending.max_count;
		if (pending.count_addr != 0) {
			if (!m_renderer.IsMapped(pending.count_addr, sizeof(uint32_t))) {
				break; // Unmapped since: keep the last known count.
			}
			count = std::min(ReadGuestForCp<uint32_t>(pending.count_addr), pending.max_count);
		}
		if (count == 0) {
			// That draw drew nothing and left the count unchanged: an older source decides.
			continue;
		}
		const auto address = pending.args_addr +
		                     static_cast<uint64_t>(count - 1u) * pending.stride +
		                     IndirectInstanceCountOffset;
		if (!m_renderer.IsMapped(address, sizeof(uint32_t))) {
			break;
		}
		const auto instances = ReadGuestForCp<uint32_t>(address);
		if (pending.has_expected && pending.expected != instances) {
			LOGF("IndirectValidate: inherited instance count changed after the native draw: "
			     "args=0x%016" PRIx64 " at_draw=%u now=%u\n",
			     pending.args_addr, pending.expected, instances);
		}
		m_num_instances = instances;
		break;
	}
	m_pending_num_instances.clear();
	return m_num_instances;
}

void CommandProcessor::ValidateIndirectSource(const DrawIndirectSource& source) {
	static std::atomic<uint32_t> log_count {0};
	const auto log_enabled = [] { return log_count.fetch_add(1, std::memory_order_relaxed) < 256; };
	uint32_t   count       = source.max_count;
	if (source.count_addr != 0) {
		count = std::min(ReadGuestForCp<uint32_t>(source.count_addr), source.max_count);
	}
	if (log_enabled()) {
		LOGF("IndirectValidate: native %s draw args=0x%016" PRIx64 " stride=%u count=%u/%u\n",
		     source.indexed ? "indexed" : "auto", source.args_addr, source.stride, count,
		     source.max_count);
	}
	if (!source.indexed) {
		return;
	}
	for (uint32_t i = 0; i < count; i++) {
		const auto record = source.args_addr + static_cast<uint64_t>(i) * source.stride;
		const auto args   = ReadGuestForCp<DrawIndexedIndirectArgs>(record);
		// The CPU path clamps the index count to INDEX_BUFFER_SIZE (ignoring firstIndex); the
		// native draw reads past the bound range instead.
		const auto end =
		    static_cast<uint64_t>(args.start_index_location) + args.index_count_per_instance;
		if (args.instance_count != 0 && args.index_count_per_instance != 0 &&
		    end > source.index_buffer_size && log_enabled()) {
			LOGF("IndirectValidate: record %u/%u args=0x%016" PRIx64 " indices [%u, +%u) exceed "
			     "INDEX_BUFFER_SIZE=%u (the CPU-read draw uses %u indices)\n",
			     i, count, record, args.start_index_location, args.index_count_per_instance,
			     source.index_buffer_size,
			     std::min(args.index_count_per_instance, source.index_buffer_size));
		}
	}
}

// Takes the host indirect path when the argument (or count) bytes are GPU-owned, i.e. when a CPU
// read would page-fault and drain the GPU. CPU-clean arguments stay on the CPU path, which then
// reads them without synchronization.
bool CommandProcessor::TryDrawIndirectNative(DrawIndirectSource source) {
	if (!NativeIndirectEnabled() || !GuestRange {source.args_addr, source.ArgsSize()}.Valid() ||
	    (source.count_addr != 0 && !GuestRange {source.count_addr, sizeof(uint32_t)}.Valid())) {
		return false;
	}
	auto&      cache     = m_renderer.GetBufferCache();
	const auto gpu_owned = [&cache](uint64_t address, uint64_t size) {
		return cache.HasGpuDirtyBytes(address, size) ||
		       cache.HasPendingBackingPublication(address, size);
	};
	if (!gpu_owned(source.args_addr, source.ArgsSize()) &&
	    (source.count_addr == 0 || !gpu_owned(source.count_addr, sizeof(uint32_t)))) {
		return false;
	}
	if (source.indexed) {
		source.index_base_addr     = m_index_base_addr;
		source.index_buffer_size   = m_index_buffer_size;
		source.index_type_and_size = m_index_type_and_size;
	}

	PendingNumInstances pending {.args_addr  = source.args_addr,
	                             .stride     = source.stride,
	                             .max_count  = source.max_count,
	                             .count_addr = source.count_addr};
	if (IndirectValidateEnabled()) {
		ValidateIndirectSource(source);
		uint32_t count = source.max_count;
		if (source.count_addr != 0) {
			count = std::min(ReadGuestForCp<uint32_t>(source.count_addr), source.max_count);
		}
		if (count != 0) {
			pending.has_expected = true;
			pending.expected     = ReadGuestForCp<uint32_t>(
			    source.args_addr + static_cast<uint64_t>(count - 1u) * source.stride +
			    IndirectInstanceCountOffset);
		}
	}

	if (!m_renderer.GetRenderExecutor().DrawIndirectNative(m_submit_id, CurrentBuffer(), source)) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::DrawIndirectFallback);
		return false;
	}
	if (source.count_addr == 0) {
		// At least one record was drawn: older sources can no longer decide.
		m_pending_num_instances.clear();
	} else if (m_pending_num_instances.size() >= 64) {
		(void)NumInstances();
	}
	m_pending_num_instances.push_back(pending);
	if (RepeatTrace::Enabled()) {
		RepeatTrace::OnUnpreparedDraw();
	}
	MaybeFlushIdleGpu();
	MaybeYieldSlice();
	return true;
}

void CommandProcessor::DrawIndirect(uint32_t data_offset, uint32_t draw_initiator, bool indexed) {
	EXIT_NOT_IMPLEMENTED((draw_initiator & ~0x20u) != 2u);
	EXIT_NOT_IMPLEMENTED(m_draw_indirect_args_base_addr == 0);

	const auto args_addr   = m_draw_indirect_args_base_addr + data_offset;
	const auto record_size = static_cast<uint32_t>(indexed ? sizeof(DrawIndexedIndirectArgs)
	                                                       : sizeof(DrawIndirectArgs));
	if (TryDrawIndirectNative({.args_addr  = args_addr,
	                           .stride     = record_size,
	                           .max_count  = 1,
	                           .count_addr = 0,
	                           .indexed    = indexed})) {
		return;
	}

	m_pending_num_instances.clear();
	if (!indexed) {
		const auto args = ReadGuestForCp<DrawIndirectArgs>(args_addr);
		m_num_instances = args.instance_count;
		DrawIndexAuto({.vertex_count   = args.vertex_count_per_instance,
		               .instance_count = args.instance_count,
		               .first_vertex   = args.start_vertex_location,
		               .first_instance = args.start_instance_location,
		               .offset_source  = DrawOffsetSource::IndirectArgs});
		return;
	}

	const auto args       = ReadGuestForCp<DrawIndexedIndirectArgs>(args_addr);
	const auto index_size = IndexElementSize(m_index_type_and_size);

	auto* index_addr = reinterpret_cast<const void*>(
	    m_index_base_addr + static_cast<uint64_t>(args.start_index_location) * index_size);

	const uint32_t index_count =
	    (m_index_buffer_size != 0 ? std::min(args.index_count_per_instance, m_index_buffer_size)
	                              : args.index_count_per_instance);
	if (GraphicsRunDebugDumpEnabled() && index_count != args.index_count_per_instance) {
		static std::atomic<uint32_t> log_count {0};
		if (log_count.fetch_add(1, std::memory_order_relaxed) < 64) {
			LOGF("\t DrawIndexIndirect: clamped index_count from %" PRIu32 " to %" PRIu32
			     " using INDEX_BUFFER_SIZE\n",
			     args.index_count_per_instance, index_count);
		}
	}

	m_num_instances = args.instance_count;
	DrawIndex({.index_count    = index_count,
	           .index_addr     = index_addr,
	           .instance_count = args.instance_count,
	           .base_vertex    = static_cast<int32_t>(args.base_vertex_location),
	           .first_instance = args.start_instance_location,
	           .offset_source  = DrawOffsetSource::IndirectArgs});
}

void CommandProcessor::DrawIndirectMulti(uint32_t data_offset, uint32_t max_count_or_count,
                                         const volatile uint32_t* count_addr,
                                         uint32_t stride_in_bytes, uint32_t draw_initiator,
                                         bool indexed) {
	EXIT_NOT_IMPLEMENTED((draw_initiator & ~0x20u) != 2u);
	EXIT_NOT_IMPLEMENTED(m_draw_indirect_args_base_addr == 0);

	// Zero records draw nothing and leave the instance count unchanged on either path.
	if (max_count_or_count == 0) {
		return;
	}

	// The renderer rejects strides shorter than a record; the CPU path below reports them.
	const auto args_base = m_draw_indirect_args_base_addr + data_offset;
	if (TryDrawIndirectNative({.args_addr  = args_base,
	                           .stride     = stride_in_bytes,
	                           .max_count  = max_count_or_count,
	                           .count_addr = reinterpret_cast<uint64_t>(count_addr),
	                           .indexed    = indexed})) {
		return;
	}

	uint32_t draw_count = max_count_or_count;
	if (count_addr != nullptr) {
		draw_count = ReadGuestForCp<uint32_t>(reinterpret_cast<uint64_t>(count_addr));
		if (draw_count > max_count_or_count) {
			draw_count = max_count_or_count;
		}
	}

	if (draw_count == 0) {
		return;
	}
	const auto args_size = indexed ? sizeof(DrawIndexedIndirectArgs) : sizeof(DrawIndirectArgs);
	EXIT_NOT_IMPLEMENTED(stride_in_bytes < args_size);
	// Every drawn record sets the count, so pending native sources cannot decide any more.
	m_pending_num_instances.clear();

	const uint64_t index_size = indexed ? IndexElementSize(m_index_type_and_size) : 0;

	for (uint32_t i = 0; i < draw_count; i++) {
		const auto args_addr = args_base + static_cast<uint64_t>(i) * stride_in_bytes;

		if (!indexed) {
			const auto args = ReadGuestForCp<DrawIndirectArgs>(args_addr);
			m_num_instances = args.instance_count;
			DrawIndexAuto({.vertex_count   = args.vertex_count_per_instance,
			               .instance_count = args.instance_count,
			               .first_vertex   = args.start_vertex_location,
			               .first_instance = args.start_instance_location,
			               .offset_source  = DrawOffsetSource::IndirectArgs});
			continue;
		}

		const auto args = ReadGuestForCp<DrawIndexedIndirectArgs>(args_addr);

		auto* index_addr = reinterpret_cast<const void*>(
		    m_index_base_addr + static_cast<uint64_t>(args.start_index_location) * index_size);

		const uint32_t index_count =
		    (m_index_buffer_size != 0
		         ? std::min(args.index_count_per_instance, m_index_buffer_size)
		         : args.index_count_per_instance);
		if (GraphicsRunDebugDumpEnabled() && index_count != args.index_count_per_instance) {
			static std::atomic<uint32_t> log_count {0};
			if (log_count.fetch_add(1, std::memory_order_relaxed) < 64) {
				LOGF("\t DrawIndexIndirectMulti: clamped index_count from %" PRIu32 " to %" PRIu32
				     " using INDEX_BUFFER_SIZE\n",
				     args.index_count_per_instance, index_count);
			}
		}

		m_num_instances = args.instance_count;
		DrawIndex({.index_count    = index_count,
		           .index_addr     = index_addr,
		           .instance_count = args.instance_count,
		           .base_vertex    = static_cast<int32_t>(args.base_vertex_location),
		           .first_instance = args.start_instance_location,
		           .offset_source  = DrawOffsetSource::IndirectArgs});
	}
}

void CommandProcessor::DispatchDirect(uint32_t thread_group_x, uint32_t thread_group_y,
                                      uint32_t thread_group_z, uint32_t mode) {
	m_sh_ctx.SetCsWaveSize(Pm4::ComputeWaveSize(mode));

	uint32_t frame_num = 0;
	// uint32_t local_x   = 1;
	// uint32_t local_y   = 1;
	// uint32_t local_z   = 1;

	{
		frame_num = m_renderer.GetGpu().GetFrameNum();
		if (GraphicsRunDebugDumpEnabled()) {
			static std::atomic<uint32_t> log_count {0};
			if (log_count.fetch_add(1, std::memory_order_relaxed) < 1024) {
				const auto& cs = m_sh_ctx.GetCs().cs_regs;
				const auto& oa = m_ucfg.GetGdsOaCounter(m_ucfg.GetGdsOaState().GetIndex());
				LOGF("QueuePoint DispatchDirect: frame=%u submit=%" PRIu64
				     " groups=%ux%ux%u local=%ux%ux%u mode=0x%08" PRIx32 " wave=%u cs=0x%016" PRIx64
				     " oa_index=%u oa_enabled=%s oa_addr=0x%04" PRIx32 " oa_space=0x%08" PRIx32
				     "\n",
				     frame_num, m_submit_id, thread_group_x, thread_group_y, thread_group_z,
				     std::max(cs.num_thread_x, 1u), std::max(cs.num_thread_y, 1u),
				     std::max(cs.num_thread_z, 1u), mode, static_cast<uint32_t>(cs.wave_size),
				     cs.data_addr, m_ucfg.GetGdsOaState().GetIndex(),
				     oa.IsCounterEnabled() ? "true" : "false", oa.GetAddressBytes(),
				     oa.GetSpaceAvailable());
			}
		}

		const auto& cs = m_sh_ctx.GetCs().cs_regs;
		if (RepeatTrace::Enabled()) {
			RepeatTrace::OnDispatch(static_cast<uint32_t>(m_interrupt_event_id), m_sh_ctx.GetCs(),
			                        thread_group_x, thread_group_y, thread_group_z, mode);
		}
		// local_x        = std::max(cs.num_thread_x, 1u);
		// local_y        = std::max(cs.num_thread_y, 1u);
		// local_z        = std::max(cs.num_thread_z, 1u);
		m_renderer.GetRenderExecutor().DispatchDirect(m_submit_id, CurrentBuffer(), thread_group_x,
		                                              thread_group_y, thread_group_z, mode);
		MaybeFlushIdleGpu();
	}

	/*constexpr uint32_t DispatchInitiatorUseThreadDimensions = 1u << 5u;
	auto               group_count = [](uint32_t threads, uint32_t group_size) {
	    return (threads == 0
	                ? 0u
	                : (threads + std::max(group_size, 1u) - 1u) / std::max(group_size, 1u));
	};

	auto groups_x = thread_group_x;
	auto groups_y = thread_group_y;
	auto groups_z = thread_group_z;
	if ((mode & DispatchInitiatorUseThreadDimensions) != 0) {
	    groups_x = group_count(thread_group_x, local_x);
	    groups_y = group_count(thread_group_y, local_y);
	    groups_z = group_count(thread_group_z, local_z);
	}

	const uint64_t invocations =
	    static_cast<uint64_t>(groups_x) * groups_y * groups_z * local_x * local_y * local_z;
	if (invocations != 0) {
	    BufferFlushAndWait();
	}*/
}

void CommandProcessor::DispatchIndirect(uint64_t args_addr, uint32_t mode) {
	EXIT_NOT_IMPLEMENTED(args_addr == 0 || (args_addr & 3u) != 0);
	if ((mode & Pm4::COMPUTE_DISPATCH_INITIATOR_USE_THREAD_DIMENSIONS) != 0) {
		const auto args = ReadGuestForCp<vk::DispatchIndirectCommand>(args_addr);
		DispatchDirect(args.x, args.y, args.z, mode);
		return;
	}
	m_sh_ctx.SetCsWaveSize(Pm4::ComputeWaveSize(mode));
	m_renderer.GetRenderExecutor().DispatchIndirect(m_submit_id, CurrentBuffer(), args_addr, mode);
	MaybeFlushIdleGpu();
}

void CommandProcessor::ReportLodStats(uint64_t destination, uint32_t size, uint32_t control) {
	(void)CurrentBuffer(); // the report is recorded at this command position
	Common::LockGuard lock(m_renderer.GetMutex());
	m_renderer.GetLodStats().Report(destination, size, control);
}

void CommandProcessor::DrawIndexAuto(DrawAutoArgs args) {
	if (args.instance_count == 0) {
		if (!m_pending_num_instances.empty()) {
			DrainPreparedDraws();
		}
		args.instance_count = NumInstances();
	}
	if (RepeatTrace::Enabled()) {
		NoteRepeatTraceDrawPacket();
	}
	if (TrySubmitPreparedDraw(nullptr, &args)) {
		// See DrawIndex: idle flushes follow each commit, the slice counts the draw now.
		MaybeYieldSlice();
		return;
	}
	if (RepeatTrace::Enabled()) {
		(void)RepeatTrace::TakeDrawPacket();
		RepeatTrace::OnUnpreparedDraw();
	}
	m_renderer.GetRenderExecutor().DrawAuto(m_submit_id, CurrentBuffer(), args);
	MaybeFlushIdleGpu();
	MaybeYieldSlice();
}

// KYTY_FLIP_WAIT_MODE=block restores the blocking WAIT_FLIP_DONE, which stalled every guest
// queue and GPU-thread command (e.g. guest readbacks) until the presenter finished the flip. By
// default ("suspend") only the waiting queue is suspended, like WAIT_REG_MEM, and retried when
// a flip completes (NotifyGpuProgress) or the scheduler's blocked-queue timeout passes.
static bool FlipWaitSuspends() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_FLIP_WAIT_MODE");
		return value == nullptr || std::strcmp(value, "block") != 0;
	}();
	return enabled;
}

void CommandProcessor::WaitFlipDone(uint32_t video_out_handle, uint32_t display_buffer_index) {
	const auto handle = static_cast<int>(video_out_handle);
	const auto index  = static_cast<int>(display_buffer_index);
	if (!FlipWaitSuspends()) {
		BufferFlush();
		Profiler::CountFrameEvent(Profiler::FrameEvent::FlipWaitBlocking);
		m_renderer.GetVideoOut().WaitFlipDone(handle, index);
		return;
	}
	if (!m_flip_wait_suspended) {
		// First evaluation of this packet: submit everything recorded before it (as the
		// blocking form did); retries have nothing new to submit.
		BufferFlush();
	}
	if (m_renderer.GetVideoOut().IsFlipPending(handle, index)) {
		if (!m_flip_wait_suspended) {
			Profiler::CountFrameEvent(Profiler::FrameEvent::FlipWaitSuspends);
		}
		m_flip_wait_suspended = true;
		SuspendPm4();
		return;
	}
	m_flip_wait_suspended = false;
}

// KYTY_LABEL_MODE=completion writes every end-of-pipe label (RELEASE_MEM / EVENT_WRITE_EOP data
// writes) only after its tick has completed, in order, like hardware; the default ("record")
// writes them when the packet is recorded and defers only visibility-proxy labels.
static bool LabelCompletionMode() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_LABEL_MODE");
		const bool  on    = value != nullptr && std::strcmp(value, "completion") == 0;
		std::printf("Kyty end-of-pipe labels: written at %s (KYTY_LABEL_MODE)\n",
		            on ? "completion" : "record time");
		return on;
	}();
	return enabled;
}

bool CommandProcessor::TryDeferLabel(void* dst, uint64_t value, uint32_t size, bool interrupt,
                                     uint32_t interrupt_context_id, uint32_t timestamp_slot) {
	const auto address = reinterpret_cast<uint64_t>(dst);
	auto&      gpu     = m_renderer.GetGpu();
	const bool proxy   = m_defer_next_label;
	const bool all     = LabelCompletionMode();
	// End-of-pipe writes become visible in order on hardware: while any older deferred write
	// (label or GDS snapshot) is pending, a later label must not overtake it.
	(void)address;
	const bool ordered = !proxy && !all && gpu.HasDeferredLabels();
	if (!proxy && !all && !ordered) {
		return false;
	}
	m_defer_next_label = false;
	auto&      scheduler = GetScheduler();
	const auto tick      = scheduler.CurrentTick();
	gpu.AddDeferredLabel(address, size, tick);
	TraceCpLabel(proxy ? "label-defer-proxy" : (ordered ? "label-defer-ordered" : "label-defer"),
	             dst, value, size);
	const int64_t trace_queue =
	    m_interrupt_event_id == 0 ? 0 : static_cast<int64_t>(m_interrupt_event_id) - 0x20 + 1;
	Profiler::CountFrameEvent(Profiler::FrameEvent::LabelWritesDeferred);
	if (proxy) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::LabelWritesDeferredProxy);
	} else if (ordered) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::LabelWritesDeferredOrdered);
	}
	auto*     renderer = &m_renderer;
	const int event_id = m_interrupt_event_id;
	// KYTY_EOP_TIMESTAMPS=gpu: a clock value is written with its query's GPU time.
	auto* timestamps =
	    timestamp_slot != EopTimestampRing::NoSlot ? scheduler.GuestTimestamps() : nullptr;
	// Runs on the completion runner once `tick` has completed, after every priority operation
	// registered before it (FIFO), including this tick's occlusion publications. The write itself
	// is handed to the GPU thread: a label page may be protected by resource tracking, and only
	// the GPU thread may take the resulting fault/readback. The interrupt follows the write.
	scheduler.DeferPriorityOperation(
	    [renderer, &gpu, address, value, size, tick, interrupt, event_id, interrupt_context_id,
	     trace_queue, timestamps, timestamp_slot] {
		    // The tick has completed, so a timestamp query's result is final.
		    const uint64_t written =
		        timestamps != nullptr ? timestamps->TakeDeferred(timestamp_slot, value) : value;
		    const bool sent = gpu.TrySendCommand([renderer, &gpu, address, written, size, tick,
		                                          interrupt, event_id, interrupt_context_id,
		                                          trace_queue] {
			    {
				    KYTY_PROFILER_DETAIL_BLOCK("EndOfPipe::WriteDeferredLabel");
				    std::memcpy(reinterpret_cast<void*>(address), &written, size);
			    }
			    if (HangTrace::CpTraceEnabled()) {
				    HangTrace::CpEvent event;
				    event.event   = "label-deferred-write";
				    event.address = address;
				    event.value   = written;
				    event.size    = size;
				    event.aux     = static_cast<int64_t>(tick);
				    event.queue   = trace_queue;
				    HangTrace::RecordCp(event);
			    }
			    // Emulator write outside a fence position (draw-prep log certificate only).
			    Coherence::NoteContentWrite(address, size, Coherence::Source::CpWrite);
			    gpu.RemoveDeferredLabel(address, tick);
			    if (interrupt) {
				    renderer->TriggerInterrupt(event_id, interrupt_context_id);
			    }
		    });
		    if (!sent) {
			    // Shutdown: the GPU thread no longer runs commands. Best-effort direct write.
			    (void)LibKernel::Memory::TryWriteBacking(address, &written, size);
			    gpu.RemoveDeferredLabel(address, tick);
			    if (interrupt) {
				    renderer->TriggerInterrupt(event_id, interrupt_context_id);
			    }
		    }
	    },
	    interrupt ? CommandScheduler::PriorityOperationKind::EopInterrupt
	              : CommandScheduler::PriorityOperationKind::Generic);
	if (proxy) {
		// A guest thread is about to wait for this label: submit its tick now rather than at the
		// next EOP batch or slice boundary, which can be most of a frame away.
		BufferFlush();
	}
	return true;
}

// KYTY_EOP_DROPPED_LABELS: "write" (default) also writes the data of end-of-pipe events whose
// interrupt selector made Kyty skip it (graphics INT_SEL=1, RELEASE_MEM INT_SEL=4 with a data
// selection); "count" only counts them, as before.
static bool DroppedLabelsWritten() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_EOP_DROPPED_LABELS");
		return value == nullptr || std::strcmp(value, "count") != 0;
	}();
	return enabled;
}

bool CommandProcessor::WriteDroppedLabel(void* dst, uint64_t value, uint32_t size,
                                         bool interrupt, uint32_t interrupt_context_id,
                                         Profiler::FrameEvent counter, bool timestamp) {
	Profiler::CountFrameEvent(counter);
	if (!DroppedLabelsWritten() || dst == nullptr || (size != 4 && size != 8)) {
		return false;
	}
	const auto timestamp_slot =
	    timestamp && size == 8 ? RecordEopTimestamp() : EopTimestampRing::NoSlot;
	if (TryDeferLabel(dst, value, size, interrupt, interrupt_context_id, timestamp_slot)) {
		return interrupt;
	}
	KYTY_PROFILER_DETAIL_BLOCK("EndOfPipe::WriteLabel");
	std::memcpy(dst, &value, size);
	QueueEopTimestamp(timestamp_slot, dst, value);
	TraceCpLabel("label-dropped", dst, value, size);
	return false;
}

uint32_t CommandProcessor::RecordEopTimestamp() {
	auto& scheduler  = GetScheduler();
	auto* timestamps = scheduler.GuestTimestamps();
	if (timestamps == nullptr) {
		return EopTimestampRing::NoSlot;
	}
	// A fence position: publish what completed ticks measured first (never waits).
	timestamps->Publish(scheduler.GetMasterSemaphore().KnownGpuTick());
	// A query write touches no guest resource, so batched barriers may stay pending (and no
	// rendering instance is split for it).
	return timestamps->RecordQuery(CurrentBuffer().StateHandle());
}

void CommandProcessor::QueueEopTimestamp(uint32_t slot, const void* dst, uint64_t value) {
	if (slot == EopTimestampRing::NoSlot) {
		return;
	}
	auto& scheduler = GetScheduler();
	scheduler.GuestTimestamps()->Queue(slot, scheduler.CurrentTick(),
	                                   reinterpret_cast<uint64_t>(dst), value);
}

bool CommandProcessor::WriteReleaseMemDroppedData(void* dst, uint64_t value, uint32_t data_sel,
                                                  bool interrupt, uint32_t interrupt_context_id) {
	uint32_t size = 0;
	switch (data_sel) {
		case 1: size = 4; break;
		case 2: size = 8; break;
		case 3:
			size  = 8;
			value = Sync::ReadReferenceClock();
			break;
		default: return false; // 5 (GDS) and others keep the old behaviour
	}
	return WriteDroppedLabel(dst, value, size, interrupt, interrupt_context_id,
	                         Profiler::FrameEvent::ReleaseMemLabelsIntSel4, data_sel == 3);
}

// KYTY_GDS_EOP_MODE=defer snapshots the GDS range with a copy recorded at the packet's position
// and writes it to guest memory once that tick has completed (as a deferred label), instead of
// draining the GPU (SynchronizeGpu) and reading GDS at record time. Default "sync" until the
// counters show this path is frequent enough to matter (FrameEvent.GdsEopReads).
static bool GdsEopDeferEnabled() {
	static const bool enabled = [] {
		const auto* value = std::getenv("KYTY_GDS_EOP_MODE");
		return value != nullptr && std::strcmp(value, "defer") == 0;
	}();
	return enabled;
}

bool CommandProcessor::TryDeferGdsRead(uint32_t* dst, uint32_t dw_offset, uint32_t dw_size,
                                       bool interrupt, uint32_t interrupt_context_id) {
	if (!GdsEopDeferEnabled() || dst == nullptr || dw_size == 0) {
		return false;
	}
	const auto* gds    = m_renderer.GetBufferCache().GetGdsBuffer();
	const auto  offset = uint64_t {dw_offset} * sizeof(uint32_t);
	const auto  size   = uint64_t {dw_size} * sizeof(uint32_t);
	if (offset > gds->Size() || size > gds->Size() - offset) {
		return false;
	}
	auto& download         = m_renderer.GetBufferCache().GetUtilityBuffer(MemoryUsage::Download);
	const auto [mapped, staged] = download.Map(size, 16);
	if (mapped == nullptr) {
		return false;
	}
	download.Commit();

	// Snapshot at this point of the GPU timeline: later work may change GDS before completion.
	auto& scheduler = GetScheduler();
	auto& buffer    = CurrentBuffer();
	buffer.EndRendering();
	const auto              native = buffer.Handle();
	vk::BufferMemoryBarrier before {};
	before.srcAccessMask       = vk::AccessFlagBits::eMemoryWrite;
	before.dstAccessMask       = vk::AccessFlagBits::eTransferRead;
	before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	before.buffer              = gds->Handle();
	before.offset              = offset;
	before.size                = size;
	native.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
	                       vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, 1, &before, 0,
	                       nullptr);
	const vk::BufferCopy copy {offset, staged, size};
	native.copyBuffer(gds->Handle(), download.Handle(), 1, &copy);
	auto after          = before;
	after.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	after.dstAccessMask = vk::AccessFlagBits::eHostRead;
	after.buffer        = download.Handle();
	after.offset        = staged;
	native.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
	                       vk::PipelineStageFlagBits::eHost, {}, 0, nullptr, 1, &after, 0,
	                       nullptr);
	// The GDS buffer's own shader accesses after this copy are ordered by their own barriers
	// (it is written only by recorded GPU work), exactly as for any other transfer read.

	const auto address = reinterpret_cast<uint64_t>(dst);
	const auto tick    = scheduler.CurrentTick();
	auto&      gpu     = m_renderer.GetGpu();
	gpu.AddDeferredLabel(address, static_cast<uint32_t>(size), tick);
	Profiler::CountFrameEvent(Profiler::FrameEvent::GdsEopReadsDeferred);
	auto*     renderer = &m_renderer;
	const int event_id = m_interrupt_event_id;
	scheduler.DeferPriorityOperation(
	    [renderer, &gpu, &download, mapped, staged, address, size, tick, interrupt, event_id,
	     interrupt_context_id] {
		    // The download ring slot stays reserved until this tick's priority operations ran.
		    download.Invalidate(staged, size);
		    std::vector<uint8_t> bytes(mapped, mapped + size);
		    auto write = [renderer, &gpu, bytes = std::move(bytes), address, tick, interrupt,
		                  event_id, interrupt_context_id](bool on_gpu_thread) {
			    if (on_gpu_thread) {
				    std::memcpy(reinterpret_cast<void*>(address), bytes.data(), bytes.size());
			    } else {
				    (void)LibKernel::Memory::TryWriteBacking(address, bytes.data(), bytes.size());
			    }
			    Coherence::NoteContentWrite(address, bytes.size(), Coherence::Source::CpWrite);
			    gpu.RemoveDeferredLabel(address, tick);
			    if (interrupt) {
				    renderer->TriggerInterrupt(event_id, interrupt_context_id);
			    }
		    };
		    auto shared = std::make_shared<decltype(write)>(std::move(write));
		    if (!gpu.TrySendCommand([shared] { (*shared)(true); })) {
			    (*shared)(false);
		    }
	    },
	    interrupt ? CommandScheduler::PriorityOperationKind::EopInterrupt
	              : CommandScheduler::PriorityOperationKind::Generic);
	return true;
}

template <typename T>
void CommandProcessor::WriteAtEndOfPipe(uint32_t cache_policy, uint32_t event_write_dest,
                                        uint32_t eop_event_type, uint32_t cache_action,
                                        uint32_t event_index, uint32_t event_write_source,
                                        void* dst_gpu_addr, T value, uint32_t interrupt_selector,
                                        uint32_t interrupt_context_id) {
	static_assert(sizeof(T) == sizeof(uint32_t) || sizeof(T) == sizeof(uint64_t));

	auto& command = CurrentBuffer();

	if (GraphicsRunDebugDumpEnabled()) {
		const auto bits      = static_cast<unsigned>(sizeof(T) * 8u);
		const auto log_width = static_cast<int>(sizeof(T) * 2u);

		LOGF("CommandProcessor::WriteAtEndOfPipe%u()\n"
		     "\t cache_policy        = 0x%08" PRIx32 "\n"
		     "\t event_write_dest    = 0x%08" PRIx32 "\n"
		     "\t eop_event_type      = 0x%08" PRIx32 "\n"
		     "\t cache_action        = 0x%08" PRIx32 "\n"
		     "\t event_index         = 0x%08" PRIx32 "\n"
		     "\t event_write_source  = 0x%08" PRIx32 "\n"
		     "\t interrupt_selector  = 0x%08" PRIx32 "\n"
		     "\t interrupt_context   = 0x%08" PRIx32 "\n"
		     "\t dst_gpu_addr        = 0x%016" PRIx64 "\n"
		     "\t value               = 0x%0*" PRIx64 "\n",
		     bits, cache_policy, event_write_dest, eop_event_type, cache_action, event_index,
		     event_write_source, interrupt_selector, interrupt_context_id,
		     reinterpret_cast<uint64_t>(dst_gpu_addr), log_width, static_cast<uint64_t>(value));
	}

	EXIT_NOT_IMPLEMENTED(cache_policy != 0x00000000);
	EXIT_NOT_IMPLEMENTED(event_write_dest != 0x00000000);

	bool with_interrupt = false;
	switch (interrupt_selector) {
		case 0x00:
		case 0x03: with_interrupt = false; break;
		case 0x01:
			if (!IsAsyncComputeQueue()) {
				// INT_SEL=1 on the graphics queue used to raise only the interrupt. On RDNA
				// INT_SEL does not gate DATA_SEL: write the data as well (KYTY_EOP_DROPPED_LABELS).
				// Plain data selections only (32-bit data, 64-bit data, reference clock).
				uint32_t label_size  = 0;
				uint64_t label_value = static_cast<uint64_t>(value);
				bool     label_clock = false;
				if constexpr (sizeof(T) == sizeof(uint32_t)) {
					label_size = event_write_source == 0x02 ? 4u : 0u;
				} else {
					switch (event_write_source) {
						case 0x01: label_size = 4; break;
						case 0x02: label_size = 8; break;
						case 0x04:
							label_size  = 8;
							label_value = Sync::ReadReferenceClock();
							label_clock = true;
							break;
						default: break;
					}
				}
				if (label_size != 0 && dst_gpu_addr != nullptr &&
				    WriteDroppedLabel(dst_gpu_addr, label_value, label_size, true,
				                      interrupt_context_id, Profiler::FrameEvent::EopLabelsIntSel1,
				                      label_clock)) {
					return; // the deferred label raises the interrupt after its write
				}
				Sync::TriggerEopEventAtEndOfPipe(command, m_interrupt_event_id,
				                                 interrupt_context_id);
				return;
			}
			with_interrupt = true;
			break;
		case 0x02: with_interrupt = true; break;
		default: EXIT("unknown interrupt_selector\n");
	}

	auto write32 = [&](bool with_writeback) {
		auto* dst  = static_cast<uint32_t*>(dst_gpu_addr);
		auto  data = static_cast<uint32_t>(value);
		if (TryDeferLabel(dst, data, sizeof(data), with_interrupt, interrupt_context_id)) {
			// The deferred write raises the interrupt itself, after the label is visible.
			if (with_writeback) {
				Sync::WriteAtEndOfPipeWithWriteBack32(m_submit_id, command, dst, data);
			} else {
				Sync::WriteAtEndOfPipe32(m_submit_id, command, dst, data);
			}
			return;
		}
		{
			// Guest label pages can be protected by resource tracking. Attribute any
			// resulting fault separately from the end-of-pipe submission/interrupt work.
			KYTY_PROFILER_DETAIL_BLOCK("EndOfPipe::WriteLabel32");
			std::memcpy(dst, &data, sizeof(data));
		}
		TraceCpLabel("label-eop", dst, data, sizeof(data));

		if (with_interrupt) {
			if (with_writeback) {
				Sync::WriteAtEndOfPipeWithInterruptWriteBack32(m_submit_id, command, dst,
				                                               data, m_interrupt_event_id,
				                                               interrupt_context_id);
			} else {
				Sync::WriteAtEndOfPipeWithInterrupt32(m_submit_id, command, dst, data,
				                                      m_interrupt_event_id, interrupt_context_id);
			}
		} else if (with_writeback) {
			Sync::WriteAtEndOfPipeWithWriteBack32(m_submit_id, command, dst, data);
		} else {
			Sync::WriteAtEndOfPipe32(m_submit_id, command, dst, data);
		}
	};

	switch (event_write_source) {
		case 0x01:
			if constexpr (sizeof(T) == sizeof(uint32_t)) {
				if (eop_event_type == 0x2f && cache_action == 0x00 && event_index == 0x06) {
					auto* dst = static_cast<uint32_t*>(dst_gpu_addr);
					Profiler::CountFrameEvent(Profiler::FrameEvent::GdsEopReads);
					if (TryDeferGdsRead(dst, value & 0xffffu, value >> 16u, with_interrupt,
					                    interrupt_context_id)) {
						Sync::WriteAtEndOfPipeGds32(m_submit_id, command, dst, value & 0xffffu,
						                            value >> 16u);
						return;
					}
					Profiler::ScopedGpuWaitReason wait_reason(Profiler::FrameWait::GpuWaitGds);
					SynchronizeGpu();
					Sync::ReadGds(*m_renderer.GetBufferCache().GetGdsBuffer(), dst, value & 0xffffu,
					              value >> 16u);
					TraceCpLabel("label-gds", dst, *dst, value >> 16u);
					Sync::WriteAtEndOfPipeGds32(m_submit_id, command, dst, value & 0xffffu,
					                            value >> 16u);
					if (with_interrupt) {
						m_renderer.TriggerInterrupt(m_interrupt_event_id, interrupt_context_id);
					}
					return;
				}
			} else if (eop_event_type == 0x04 && cache_action == 0x00 && event_index == 0x05) {
				write32(false);
				return;
			}
			break;
		case 0x02:
		case 0x04:
			if constexpr (sizeof(T) == sizeof(uint32_t)) {
				if (event_write_source == 0x02 && eop_event_type == 0x2f && event_index == 0x06) {
					switch (cache_action) {
						case 0x00: write32(false); return;
						case 0x38: write32(true); return;
						default: break;
					}
				}
			} else {
				const bool clock_write = event_write_source == 0x04;
				if (clock_write) {
					value = Sync::ReadReferenceClock();
				}
				auto write64 = [&](bool with_writeback) {
					auto* dst = static_cast<uint64_t*>(dst_gpu_addr);
					// KYTY_EOP_TIMESTAMPS=gpu: a query at this point; the slot gets its GPU time
					// once the tick completes (from the deferred write when deferred).
					const auto timestamp_slot =
					    clock_write ? RecordEopTimestamp() : EopTimestampRing::NoSlot;
					if (TryDeferLabel(dst, value, sizeof(value), with_interrupt,
					                  interrupt_context_id, timestamp_slot)) {
						// The deferred write raises the interrupt itself, after the label.
						if (with_writeback) {
							Sync::WriteAtEndOfPipeWithWriteBack64(m_submit_id, command, dst,
							                                      value);
						} else {
							Sync::WriteAtEndOfPipe64(m_submit_id, command, dst, value);
						}
						return;
					}
					{
						KYTY_PROFILER_DETAIL_BLOCK("EndOfPipe::WriteLabel64");
						std::memcpy(dst, &value, sizeof(value));
					}
					QueueEopTimestamp(timestamp_slot, dst, value);
					TraceCpLabel("label-eop", dst, value, sizeof(value));

					if (with_interrupt) {
						if (with_writeback) {
							Sync::WriteAtEndOfPipeWithInterruptWriteBack64(
							    m_submit_id, command, dst, value, m_interrupt_event_id,
							    interrupt_context_id);
						} else {
							Sync::WriteAtEndOfPipeWithInterrupt64(m_submit_id, command, dst,
							                                      value, m_interrupt_event_id,
							                                      interrupt_context_id);
						}
					} else if (with_writeback) {
						Sync::WriteAtEndOfPipeWithWriteBack64(m_submit_id, command, dst,
						                                      value);
					} else {
						Sync::WriteAtEndOfPipe64(m_submit_id, command, dst, value);
					}
				};

				switch (cache_action) {
					case 0x00:
						switch (eop_event_type) {
							case 0x04:
								if (event_index == 0x05) {
									write64(false);
									return;
								}
								break;
							case 0x14:
							case 0x28:
							case 0x2f:
								if (event_index == 0x00) {
									write64(false);
									return;
								}
								break;
							case 0x2b:
							case 0x2d:
							case 0x30:
								if (event_index == 0x00 && !with_interrupt) {
									write64(false);
									return;
								}
								break;
							default: break;
						}
						break;
					case 0x38:
						switch (eop_event_type) {
							case 0x04:
							case 0x14:
							case 0x28:
								if (((eop_event_type == 0x04 || eop_event_type == 0x28) &&
								     event_index == 0x05) ||
								    (event_index == 0x00)) {
									write64(true);
									return;
								}
								break;
							case 0x2b:
							case 0x2d:
								if (event_index == 0x00 && !with_interrupt) {
									write64(true);
									return;
								}
								break;
							case 0x2f:
								if (event_index == 0x06 && !with_interrupt) {
									write64(true);
									return;
								}
								break;
							default: break;
						}
						break;
					case 0x3b:
						if (eop_event_type == 0x04 && event_index == 0x05 && with_interrupt) {
							write64(true);
							return;
						}
						break;
					default: break;
				}
			}
			break;
		default: break;
	}

	EXIT("unknown event type\n");
}

void CommandProcessor::WriteAtEndOfPipe32(uint32_t cache_policy, uint32_t event_write_dest,
                                          uint32_t eop_event_type, uint32_t cache_action,
                                          uint32_t event_index, uint32_t event_write_source,
                                          void* dst_gpu_addr, uint32_t value,
                                          uint32_t interrupt_selector,
                                          uint32_t interrupt_context_id) {
	KYTY_PROFILER_DETAIL_FUNCTION();
	WriteAtEndOfPipe(cache_policy, event_write_dest, eop_event_type, cache_action, event_index,
	                 event_write_source, dst_gpu_addr, value, interrupt_selector,
	                 interrupt_context_id);
}

void CommandProcessor::WriteAtEndOfPipe64(uint32_t cache_policy, uint32_t event_write_dest,
                                          uint32_t eop_event_type, uint32_t cache_action,
                                          uint32_t event_index, uint32_t event_write_source,
                                          void* dst_gpu_addr, uint64_t value,
                                          uint32_t interrupt_selector,
                                          uint32_t interrupt_context_id) {
	KYTY_PROFILER_DETAIL_FUNCTION();
	WriteAtEndOfPipe(cache_policy, event_write_dest, eop_event_type, cache_action, event_index,
	                 event_write_source, dst_gpu_addr, value, interrupt_selector,
	                 interrupt_context_id);
}

void CommandProcessor::EmitGlobalBarrier() {
	KYTY_GPU_OP_SITE("guest.global_barrier");
	KYTY_PROFILER_DETAIL_FUNCTION();
	Common::LockGuard lock(m_renderer.GetMutex());
	// Keep renderer-lock contention in the parent zone's self time.
	KYTY_PROFILER_DETAIL_BLOCK("CommandProcessor::RecordGlobalBarrier");

	if (BarrierBatchEnabled()) {
		// Queued: merged with adjacent requests, elided when the previous barrier already covers
		// it with nothing recorded since, recorded before the next memory-accessing command.
		// The rendering instance ends only if the barrier is recorded (see render.h).
		CurrentBuffer().RequestMemoryBarrier(
		    vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eMemoryWrite,
		    vk::PipelineStageFlagBits2::eAllCommands,
		    vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
		    BarrierOrigin::Guest);
		return;
	}

	vk::MemoryBarrier2 barrier {};
	barrier.srcStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
	barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
	barrier.dstStageMask  = vk::PipelineStageFlagBits2::eAllCommands;
	barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;

	vk::DependencyInfo dependency {};
	dependency.memoryBarrierCount = 1;
	dependency.pMemoryBarriers    = &barrier;
	GetScheduler().EndRendering();
	CurrentBuffer().Handle().pipelineBarrier2(dependency);
}

void CommandProcessor::TriggerEopEventAtEndOfPipe(uint32_t interrupt_context_id) {
	Sync::TriggerEopEventAtEndOfPipe(CurrentBuffer(), m_interrupt_event_id, interrupt_context_id);
}

void CommandProcessor::TriggerEvent(uint32_t event_type, uint32_t event_index,
                                    uint64_t event_address) {
	if (GraphicsRunDebugDumpEnabled()) {
		LOGF("CommandProcessor::TriggerEvent()\n"
		     "\t event_type  = 0x%08" PRIx32 "\n"
		     "\t event_index = 0x%08" PRIx32 "\n"
		     "\t address     = 0x%016" PRIx64 "\n",
		     event_type, event_index, event_address);
	}

	const auto valid_cache_event_index = event_index == 0x00000000 || event_index == 0x00000007;
	switch (event_type) {
		// CsPartialFlush, GsPartialFlush, PsPartialFlush.
		case 0x00000007:
		case 0x0000000f:
		case 0x00000010: EmitGlobalBarrier(); break;
		// CbDbDataWritebackInvalidate, CbDataWritebackInvalidate.
		case 0x00000016:
		case 0x00000031:
			if (!valid_cache_event_index) {
				EXIT("unknown event type: 0x%08" PRIx32 ", 0x%08" PRIx32 "\n", event_type,
				     event_index);
			}
			EmitGlobalBarrier();
			break;
		// DbDataWritebackInvalidate, DbMetadataWritebackInvalidate, CbMetadataWritebackInvalidate.
		case 0x0000002a:
		case 0x0000002c:
		case 0x0000002e:
			if (!valid_cache_event_index) {
				EXIT("unknown event type: 0x%08" PRIx32 ", 0x%08" PRIx32 "\n", event_type,
				     event_index);
			}
			EmitGlobalBarrier();
			break;
		case 0x0000000d:
		case 0x0000000e:
		case 0x00000012:
		case 0x00000017:
		case 0x00000018:
		case 0x00000019:
		case 0x0000001a:
		case 0x0000001b:
		case 0x00000038:
		case 0x0000003a:
			LOGF("\t temporary: ignoring unsupported event_write type 0x%08" PRIx32
			     ", index 0x%08" PRIx32 "\n",
			     event_type, event_index);
			break;
		case 0x00000039: {
			Profiler::CountFrameEvent(Profiler::FrameEvent::OcclusionCounterDumps);
			if (event_index != 0x00000001 || event_address == 0 || (event_address & 0x7u) != 0) {
				EXIT("invalid occlusion-counter dump: index=0x%08" PRIx32 ", address=0x%016" PRIx64
				     "\n",
				     event_index, event_address);
			}
			if (OcclusionCounter::Enabled()) {
				bool sync = false;
				{
					Common::LockGuard lock(m_renderer.GetMutex());
					sync = m_renderer.GetOcclusionCounter().Dump(event_address);
				}
				if (sync) {
					Profiler::CountFrameEvent(Profiler::FrameEvent::OcclusionProxyDumps);
					if (OcclusionCounter::GetProxyMode() == OcclusionCounter::ProxyMode::Sync) {
						// Publish this visibility-proxy result before the CP processes the label
						// that follows it (labels are written at record time).
						Profiler::ScopedGpuWaitReason wait_reason(
						    Profiler::FrameWait::GpuWaitOcclusion);
						BufferWait();
					} else {
						// defer-label: the next end-of-pipe label is written only after this
						// dump's tick completed and its publication (queued above on the same
						// completion runner) has run. See TryDeferLabel.
						m_defer_next_label = true;
					}
				}
				break;
			}
			static std::once_flag warning_once;
			std::call_once(warning_once, [] {
				std::printf("Warning: game uses occlusion queries, which are currently treated as "
				            "always visible; GPU usage may be higher and FPS may be lower.\n");
			});

			// Until host occlusion queries are implemented, publish an always-visible result. The
			// PS5 layout contains one interleaved begin/end pair per DB, and bit 63 marks a result
			// ready.
			constexpr uint64_t ready_bit    = 1ull << 63u;
			constexpr uint64_t counter_mask = ready_bit - 1u;
			auto*              results      = reinterpret_cast<volatile uint64_t*>(event_address);
			const auto         value        = ready_bit | m_synthetic_occlusion_counter;
			for (uint32_t db = 0; db < 16u; db++) {
				results[db * 2u] = value;
			}
			m_synthetic_occlusion_counter = (m_synthetic_occlusion_counter + 1u) & counter_mask;
			break;
		}
		default:
			EXIT("unknown event type: 0x%08" PRIx32 ", 0x%08" PRIx32 "\n", event_type, event_index);
	}
}

// KYTY_LOOP_GUARD: guarded shaders count exhausted invocations in the last GDS dword. The mapped
// word is read without waiting for the GPU (a diagnostic) and reported when it changes.
static void NoteLoopGuardHits(RenderContext& renderer) {
	const auto& options = Libs::Graphics::ShaderRecompiler::GetCodegenOptions();
	if (options.loop_guard_budget == 0 || options.loop_guard_shaders.empty()) {
		return;
	}
	const auto mapped = renderer.GetBufferCache().GetGdsBuffer()->Mapped();
	if (mapped.size() < sizeof(uint32_t)) {
		return;
	}
	static uint32_t reported = 0;
	uint32_t        hits     = 0;
	std::memcpy(&hits, mapped.data() + mapped.size() - sizeof(uint32_t), sizeof(hits));
	if (hits != reported) {
		reported = hits;
		Log::WriteToConsoleAndLog(fmt::format(
		    "Loop guard: {} invocations of the guarded shaders exhausted the {}-iteration loop "
		    "budget (KYTY_LOOP_GUARD)\n",
		    hits, options.loop_guard_budget));
	}
}

void CommandProcessor::Flip() {
	NoteLoopGuardHits(m_renderer);
	if (GraphicsRunDebugDumpEnabled()) {
		LOGF("CommandProcessor::Flip()\n");
	}

	auto& command = CurrentBuffer();
	auto request = Sync::PrepareVideoOutFlip(command, m_flip.handle, m_flip.index, m_flip.flip_mode,
	                                         m_flip.flip_arg);
	Sync::WriteAtEndOfPipeOnlyFlip(m_submit_id, command, m_flip.handle, m_flip.index,
	                               m_flip.flip_mode, m_flip.flip_arg, request);
	GetScheduler().Flush();
}

void CommandProcessor::Flip(void* dst_gpu_addr, uint32_t value) {
	NoteLoopGuardHits(m_renderer);
	auto& command = CurrentBuffer();

	if (GraphicsRunDebugDumpEnabled()) {
		LOGF("CommandProcessor::Flip()\n"
		     "\t dst_gpu_addr = 0x%016" PRIx64 "\n"
		     "\t value        = 0x%08" PRIx32 "\n",
		     reinterpret_cast<uint64_t>(dst_gpu_addr), value);
	}

	std::memcpy(dst_gpu_addr, &value, sizeof(value));
	auto request = Sync::PrepareVideoOutFlip(command, m_flip.handle, m_flip.index, m_flip.flip_mode,
	                                         m_flip.flip_arg);
	Sync::WriteAtEndOfPipeWithFlip32(m_submit_id, command, static_cast<uint32_t*>(dst_gpu_addr),
	                                 value, m_flip.handle, m_flip.index, m_flip.flip_mode,
	                                 m_flip.flip_arg, request);
	GetScheduler().Flush();
}

void CommandProcessor::FlipWithInterrupt(uint32_t eop_event_type, uint32_t cache_action,
                                         void* dst_gpu_addr, uint32_t value) {
	NoteLoopGuardHits(m_renderer);
	auto& command = CurrentBuffer();

	if (GraphicsRunDebugDumpEnabled()) {
		LOGF("CommandProcessor::FlipWithInterrupt()\n"
		     "\t eop_event_type      = 0x%08" PRIx32 "\n"
		     "\t cache_action        = 0x%08" PRIx32 "\n"
		     "\t dst_gpu_addr        = 0x%016" PRIx64 "\n"
		     "\t value               = 0x%08" PRIx32 "\n",
		     eop_event_type, cache_action, reinterpret_cast<uint64_t>(dst_gpu_addr), value);
	}

	if (eop_event_type != 0x00000004 || cache_action != 0x00000038) {
		EXIT("unknown event type\n");
	}
	std::memcpy(dst_gpu_addr, &value, sizeof(value));
	auto request = Sync::PrepareVideoOutFlip(command, m_flip.handle, m_flip.index, m_flip.flip_mode,
	                                         m_flip.flip_arg);
	Sync::WriteAtEndOfPipeWithInterruptWriteBackFlip32(
	    m_submit_id, command, static_cast<uint32_t*>(dst_gpu_addr), value, m_flip.handle,
	    m_flip.index, m_flip.flip_mode, m_flip.flip_arg, request, m_interrupt_event_id);
	GetScheduler().Flush();
}

void CommandProcessor::PrepareCpuFlip(uint64_t request_id) {
	auto& command = CurrentBuffer();
	if (g_current_processor != nullptr) {
		EXIT("invalid graphics-thread CPU flip preparation\n");
	}
	struct ProcessorScope {
		explicit ProcessorScope(CommandProcessor& processor) { g_current_processor = &processor; }
		~ProcessorScope() { g_current_processor = nullptr; }
	};
	ProcessorScope processor_scope(*this);

	m_renderer.GetVideoOut().PrepareFlip(request_id, command);
	GetScheduler().Flush();
	m_renderer.GetVideoOut().CompleteFlip(request_id);
}

void CommandProcessor::SynchronizeGpu() {
	KYTY_PROFILER_DETAIL_FUNCTION();
	GetScheduler().Finish();
}

bool GuestGpu::IsGpuThread() noexcept {
	return g_gpu_thread;
}

} // namespace Libs::Graphics
