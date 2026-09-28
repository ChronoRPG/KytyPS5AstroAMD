#ifndef GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_COMMAND_PROCESSOR_H
#define GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_COMMAND_PROCESSOR_H

#include "common/assert.h"
#include "common/profiler.h"
#include "graphics/guest_gpu/hardwareContext.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/renderer/renderContext.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Libs::Graphics {

namespace DrawPrep {
class Engine;
} // namespace DrawPrep

bool TestWaitRegMemValue(uint64_t value, uint64_t ref, uint64_t mask, uint32_t func);

// CP read of guest memory the GPU may have written: the clean backing when no GPU-owned byte
// overlaps, else after synchronizing those bytes (the drain a page fault would cause), else the
// mapped read with its page-fault readback path. GPU thread, at packet boundaries only.
void ReadGuestForCp(uint64_t vaddr, uint64_t size, void* dst);

template <typename T>
[[nodiscard]] T ReadGuestForCp(uint64_t vaddr) {
	T value {};
	ReadGuestForCp(vaddr, sizeof(T), &value);
	return value;
}

enum class Pm4ProcessResult { Complete, Blocked };

enum class ContextStateOperation : uint32_t {
	Clear     = 0,
	Push      = 1,
	Pop       = 2,
	PushClear = 3,
};

class Pm4Execution {
public:
	[[nodiscard]] bool MadeProgress() const noexcept { return m_made_progress; }
	// The last Process call stopped after a completed packet to let other queues run.
	[[nodiscard]] bool Yielded() const noexcept { return m_yielded; }

private:
	friend class CommandProcessor;

	struct BufferCursor {
		std::span<const uint32_t> commands;
		uint32_t                  offset_dw = 0;
	};

	std::vector<BufferCursor> m_buffer_stack;
	std::span<const uint32_t> m_next_buffer;
	bool                      m_chain         = false;
	bool                      m_suspended     = false;
	bool                      m_made_progress = false;
	bool                      m_yield         = false; // stop after the current packet
	bool                      m_yielded       = false;
};

class CommandProcessor {
public:
	struct FlipInfo {
		int     handle    = 0;
		int     index     = 0;
		int     flip_mode = 0;
		int64_t flip_arg  = 0;
	};

	CommandProcessor(RenderContext& renderer, int interrupt_event_id)
	    : m_renderer(renderer), m_interrupt_event_id(interrupt_event_id) {}
	~CommandProcessor();

	KYTY_CLASS_NO_COPY(CommandProcessor);

	void Reset();
	void ApplyContextStateOperation(ContextStateOperation operation);

	void            BufferInit();
	void            BufferFlush();
	void            BufferFlushAndWait();
	// Flush requested by an end-of-pipe interrupt. Interrupts fire when their tick completes, so
	// only every KYTY_EOP_FLUSH_BATCH-th request flushes (default 8; 1 = flush every time); a
	// slice always ends with a flush, so a pending interrupt is submitted before the CP blocks.
	void            BufferFlushForEop();
	static uint32_t EopFlushPacketLimit();
	void            BufferWait();
	// Early submit when the GPU ran out of submitted work (KYTY_IDLE_FLUSH_DRAWS). Call after
	// recording a draw or dispatch, at a packet boundary.
	void            MaybeFlushIdleGpu();
	// Graphics queue: after KYTY_GFX_SLICE_DRAWS draws in a slice, end the slice after the
	// current packet when another queue has runnable work. Call after recording a draw.
	void            MaybeYieldSlice();
	HW::Context&    GetCtx() { return m_ctx; }
	HW::UserConfig& GetUcfg() { return m_ucfg; }
	HW::Shader&     GetShCtx() { return m_sh_ctx; }

	void SetIndexType(uint32_t index_type_and_size);
	void SetIndexBaseAddress(uint64_t index_base_addr);
	void SetIndexBufferSize(uint32_t index_buffer_size);
	void SetDrawIndirectArgsBaseAddress(uint64_t draw_indirect_args_base_addr);
	void SetDispatchIndirectArgsBaseAddress(uint64_t dispatch_indirect_args_base_addr);
	[[nodiscard]] uint64_t GetDispatchIndirectArgsBaseAddress() const {
		return m_dispatch_indirect_args_base_addr;
	}
	void SetNumInstances(uint32_t num_instances);
	void DrawIndex(DrawIndexArgs args);
	void DrawIndexOffset(uint32_t index_offset, uint32_t index_count);
	void DrawIndexAuto(DrawAutoArgs args);
	void ReportLodStats(uint64_t destination, uint32_t size, uint32_t control);
	void DrawIndirect(uint32_t data_offset, uint32_t draw_initiator, bool indexed);
	void DrawIndirectMulti(uint32_t data_offset, uint32_t max_count_or_count,
	                       const volatile uint32_t* count_addr, uint32_t stride_in_bytes,
	                       uint32_t draw_initiator, bool indexed);
	void WriteAtEndOfPipe32(uint32_t cache_policy, uint32_t event_write_dest,
	                        uint32_t eop_event_type, uint32_t cache_action, uint32_t event_index,
	                        uint32_t event_write_source, void* dst_gpu_addr, uint32_t value,
	                        uint32_t interrupt_selector, uint32_t interrupt_context_id = 0);
	void WriteAtEndOfPipe64(uint32_t cache_policy, uint32_t event_write_dest,
	                        uint32_t eop_event_type, uint32_t cache_action, uint32_t event_index,
	                        uint32_t event_write_source, void* dst_gpu_addr, uint64_t value,
	                        uint32_t interrupt_selector, uint32_t interrupt_context_id = 0);
	void Flip();
	void Flip(void* dst_gpu_addr, uint32_t value);
	void FlipWithInterrupt(uint32_t eop_event_type, uint32_t cache_action, void* dst_gpu_addr,
	                       uint32_t value);
	void PrepareCpuFlip(uint64_t request_id);
	void SynchronizeGpu();
	void EmitGlobalBarrier();
	void TriggerEopEventAtEndOfPipe(uint32_t interrupt_context_id);
	void DispatchDirect(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z,
	                    uint32_t mode);
	void DispatchIndirect(uint64_t args_addr, uint32_t mode);
	void WaitFlipDone(uint32_t video_out_handle, uint32_t display_buffer_index);
	void TriggerEvent(uint32_t event_type, uint32_t event_index, uint64_t event_address = 0);

	void SetUserDataMarker(HW::UserSgprType type) { m_user_data_marker = type; }
	[[nodiscard]] HW::UserSgprType GetUserDataMarker() const { return m_user_data_marker; }

	void ResetDeCe();
	void SetCeComplete(bool complete) { m_ce_complete = complete; }
	void WaitCe();
	void WaitDeDiff(uint32_t diff);
	void WaitForRewind(bool valid);
	void IncrementDe();
	void IncrementCe();

	void WriteConstRam(uint32_t offset, const uint32_t* src, uint32_t dw_num);
	void DumpConstRam(uint32_t* dst, uint32_t offset, uint32_t dw_num);

	template <typename T>
	void WaitRegMem(uint32_t func, const T* addr, T ref, T mask, uint32_t poll, uint32_t wait_op);
	void WriteData(uint32_t* dst, const uint32_t* src, uint32_t dw_num, uint32_t write_control);
	void WriteReferenceClock(uint64_t dst_address, uint32_t num_bytes);
	void DmaData(uint8_t engine, uint8_t dst_sel, uint8_t dst_cache_policy,
	             uint64_t dst_address_or_offset, uint8_t src_sel, uint8_t src_cache_policy,
	             uint64_t src_address_or_offset_or_immediate, uint32_t num_bytes,
	             uint8_t wait_for_previous, uint8_t write_confirm, uint8_t block_engine);
	void SetPredication(uint32_t condition, uint32_t op, uint32_t wait_op,
	                    const volatile void* address, uint32_t count_in_dwords);
	[[nodiscard]] bool ShouldSkipPredicatedPackets() const { return m_predicate_skip; }

	Pm4ProcessResult Process(Pm4Execution& execution, std::span<const uint32_t> commands);
	void             ProcessIndirectBuffer(std::span<const uint32_t> commands, bool chain);

	void SetFlip(const FlipInfo& flip) { m_flip = flip; }

	[[nodiscard]] uint64_t GetSubmitId() const { return m_submit_id; }
	void                   SetSubmitId(uint64_t submit_id) { m_submit_id = submit_id; }
	[[nodiscard]] bool     IsAsyncComputeQueue() const { return m_interrupt_event_id >= 0x20; }

private:
	template <typename T>
	void WriteAtEndOfPipe(uint32_t cache_policy, uint32_t event_write_dest, uint32_t eop_event_type,
	                      uint32_t cache_action, uint32_t event_index, uint32_t event_write_source,
	                      void* dst_gpu_addr, T value, uint32_t interrupt_selector,
	                      uint32_t interrupt_context_id);
	void ProcessPm4(Pm4Execution& execution);
	void SuspendPm4();
	// Defers an end-of-pipe label (and its interrupt) to the completion of the current tick when
	// a visibility-proxy dump armed it (KYTY_OCCLUSION_PROXY_MODE=defer-label), every label is
	// deferred (KYTY_LABEL_MODE=completion), or an older deferred label to the same address is
	// still pending (write order). Returns false when the label must be written now.
	// timestamp_slot: the label is a clock value with a KYTY_EOP_TIMESTAMPS=gpu query
	// (RecordEopTimestamp); a deferred write then writes the query's GPU time instead.
	[[nodiscard]] bool TryDeferLabel(void* dst, uint64_t value, uint32_t size, bool interrupt,
	                                 uint32_t interrupt_context_id,
	                                 uint32_t timestamp_slot = UINT32_MAX);
	// KYTY_EOP_TIMESTAMPS=gpu, at an end-of-pipe clock write (a fence: the draw-prep window is
	// empty): publishes the completed timestamps, then records a query at this point. Returns its
	// slot, or UINT32_MAX in record mode or without a free slot.
	[[nodiscard]] uint32_t RecordEopTimestamp();
	// The clock value `value` was just written to `dst` at record time: rewrite it with the
	// query's GPU time once the current tick completes (no-op without a slot).
	void QueueEopTimestamp(uint32_t slot, const void* dst, uint64_t value);
	// KYTY_GDS_EOP_MODE=defer: snapshot a GDS range at this packet and write it to `dst` at the
	// tick's completion instead of draining the GPU. Returns false for the synchronous path.
	[[nodiscard]] bool TryDeferGdsRead(uint32_t* dst, uint32_t dw_offset, uint32_t dw_size,
	                                   bool interrupt, uint32_t interrupt_context_id);
	// Writes (or defers) the data of an end-of-pipe event Kyty used to drop (counted under
	// `counter`). Returns true when a deferred write took over raising the interrupt.
	// timestamp: `value` is the reference clock (KYTY_EOP_TIMESTAMPS).
	[[nodiscard]] bool WriteDroppedLabel(void* dst, uint64_t value, uint32_t size, bool interrupt,
	                                     uint32_t interrupt_context_id,
	                                     Profiler::FrameEvent counter, bool timestamp = false);

public:
	// RELEASE_MEM with INT_SEL=4 and DATA_SEL != 0 (previously no data was written): writes the
	// data (1: 32-bit, 2: 64-bit, 3: reference clock). Returns true when a deferred write took
	// over raising the interrupt (the caller then only flushes).
	[[nodiscard]] bool WriteReleaseMemDroppedData(void* dst, uint64_t value, uint32_t data_sel,
	                                              bool interrupt, uint32_t interrupt_context_id);

private:
	[[nodiscard]] bool  TryDrawIndirectNative(DrawIndirectSource source);
	void                ValidateIndirectSource(const DrawIndirectSource& source);
	[[nodiscard]] uint32_t NumInstances();
	// Draw-prep (drawPrep.h): the engine of the graphics processor, created on first use when
	// KYTY_DRAW_PREP (or the S0 histogram) is enabled; null otherwise.
	[[nodiscard]] DrawPrep::Engine* DrawPrepEngine();
	[[nodiscard]] bool TrySubmitPreparedDraw(const DrawIndexArgs* index_args,
	                                         const DrawAutoArgs*  auto_args);
	void               DrainPreparedDraws();
	void               NoteRepeatTraceDrawPacket();
	CommandScheduler&   GetScheduler() const { return m_renderer.GetCommandScheduler(); }
	CommandBuffer&      CurrentBuffer() { return GetScheduler().Current(); }

	RenderContext&   m_renderer;
	HW::Context      m_ctx;
	HW::Context      m_saved_ctx;
	bool             m_context_state_pushed = false;
	HW::UserConfig   m_ucfg;
	HW::Shader       m_sh_ctx;
	HW::UserSgprType m_user_data_marker                 = HW::UserSgprType::Unknown;
	uint32_t         m_index_type_and_size              = 0;
	uint32_t         m_index_buffer_size                = 0;
	uint64_t         m_index_base_addr                  = 0;
	uint64_t         m_draw_indirect_args_base_addr     = 0;
	uint64_t         m_dispatch_indirect_args_base_addr = 0;
	// Persistent draw state: indirect draws update it for subsequent draws.
	uint32_t m_num_instances = 1;
	// After native indirect draws the instance count is still GPU data: instance_count of the
	// last record drawn, which a draw without its own count reads back (NumInstances). With a
	// GPU count that may be zero, older sources remain candidates, newest last. Empty whenever
	// m_num_instances is current; SetNumInstances and CPU-read indirect draws clear it.
	struct PendingNumInstances {
		uint64_t args_addr  = 0;
		uint32_t stride     = 0;
		uint32_t max_count  = 0;
		uint64_t count_addr = 0;
		bool     has_expected = false; // KYTY_INDIRECT_VALIDATE: value read at the draw
		uint32_t expected     = 0;
	};
	std::vector<PendingNumInstances> m_pending_num_instances;

	uint32_t m_de_count    = 0;
	uint32_t m_ce_count    = 0;
	bool     m_ce_complete = false;

	uint32_t m_const_ram[0x3000] = {0};

	FlipInfo  m_flip;
	const int m_interrupt_event_id;
	uint64_t  m_submit_id                   = 0;
	uint64_t  m_synthetic_occlusion_counter = 0;
	bool      m_predicate_skip              = false;
	uint32_t  m_deferred_eop_flushes        = 0;
	uint32_t  m_packets_since_eop_request   = 0;
	// A visibility-proxy end dump was recorded: defer the next end-of-pipe label (defer-label).
	bool      m_defer_next_label            = false;
	// The current WAIT_FLIP_DONE packet already flushed and suspended (retries skip the flush).
	bool      m_flip_wait_suspended         = false;
	// MaybeFlushIdleGpu: recording tick being counted and its draws/dispatches so far.
	uint64_t  m_idle_flush_tick             = 0;
	uint32_t  m_idle_flush_draws            = 0;
	// Draws recorded in the current graphics slice (MaybeYieldSlice).
	uint32_t  m_slice_draws                 = 0;

	struct DrawPrepDeleter {
		void operator()(DrawPrep::Engine* engine) const noexcept;
	};
	std::unique_ptr<DrawPrep::Engine, DrawPrepDeleter> m_draw_prep;
	// Packets processed, for the CP's placement samples (common/cpuPlacement.h).
	uint32_t m_placement_packets = 0;
};

} // namespace Libs::Graphics

#endif // GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_COMMAND_PROCESSOR_H
