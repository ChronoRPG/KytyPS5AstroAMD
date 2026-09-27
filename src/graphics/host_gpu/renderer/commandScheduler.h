#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_COMMANDSCHEDULER_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_COMMANDSCHEDULER_H_

#include "common/common.h"
#include "common/uniqueFunction.h"
#include "graphics/host_gpu/renderer/masterSemaphore.h"
#include "graphics/host_gpu/renderer/render.h"

#include <condition_variable>
#include <memory>
#include <mutex>

#include <queue>

#include <thread>
#include <vector>

namespace Libs::Graphics {

class GpuTimestampRing;

class CommandScheduler {
public:
	// Diagnostic attribution only; both kinds retain the same completion boundary.
	enum class PriorityOperationKind { Generic, EopInterrupt };
	// Only the guest scheduler carries KYTY_GPU_TIMING timestamps. Presenter submissions wait on
	// image acquisition at the transfer stage, so their top-of-pipe spans would include that wait.
	enum class Role { Guest, Presenter };

	CommandScheduler(RenderContext& context, GraphicContext& graphics, Role role);
	~CommandScheduler();
	KYTY_CLASS_NO_COPY(CommandScheduler);

	void           Begin(HW::Context& registers, HW::UserConfig& user_config, HW::Shader& shaders);
	void           BeginRendering(const RenderState& state);
	void           EndRendering();
	void           Flush();
	void           Flush(SubmitInfo& submit);
	void           FlushAndWait();
	void           Finish();
	CommandBuffer& BeginCommand();
	uint64_t       Submit(SubmitInfo submit = {}, bool force_completion = false);
	// Deferred callbacks can observe an externally owned drain, but cannot initiate shutdown:
	// the priority runner cannot join itself.
	void                      Shutdown();
	void                      Wait(uint64_t tick);
	void                      PopPendingOperations();
	void                      DrainPriorityOperations();
	void                      WaitPriorityOperations(uint64_t tick);
	void                      DeferOperation(Common::UniqueFunction<void>&& operation);
	void                      DeferPriorityOperation(
	    Common::UniqueFunction<void>&& operation,
	    PriorityOperationKind kind = PriorityOperationKind::Generic);
	[[nodiscard]] static bool InDeferredOperation() noexcept;

	// Draw-prep: the register set the current command buffer reads. A committed draw's buffer is
	// pointed at that draw's register snapshot (the live registers may already belong to later
	// packets) and restored afterwards; it survives command-buffer restarts in between because
	// the scheduler reuses one CommandBuffer wrapper.
	struct RegisterBinding {
		HW::Context*    registers   = nullptr;
		HW::UserConfig* user_config = nullptr;
		HW::Shader*     shaders     = nullptr;
	};
	[[nodiscard]] RegisterBinding BindRegisters(HW::Context& registers, HW::UserConfig& user_config,
	                                            HW::Shader& shaders) noexcept {
		const RegisterBinding previous {m_command.m_registers, m_command.m_user_config,
		                                m_command.m_shaders};
		m_command.Bind(registers, user_config, shaders);
		return previous;
	}
	void RestoreRegisters(const RegisterBinding& binding) noexcept {
		m_command.m_registers   = binding.registers;
		m_command.m_user_config = binding.user_config;
		m_command.m_shaders     = binding.shaders;
	}

	[[nodiscard]] bool Active() const noexcept { return m_command.m_registers != nullptr; }
	void                           CheckActive() const;
	CommandBuffer&                 Current();
	[[nodiscard]] uint64_t         CurrentTick() const noexcept { return m_master.CurrentTick(); }
	[[nodiscard]] bool             IsFree(uint64_t tick);
	[[nodiscard]] MasterSemaphore& GetMasterSemaphore() noexcept { return m_master; }
	[[nodiscard]] RenderContext&   Context() const noexcept { return m_context; }
	[[nodiscard]] GraphicContext&  Graphics() const noexcept { return m_graphics; }

private:
	class CommandPool {
	public:
		CommandPool(GraphicContext& graphics, MasterSemaphore& master);
		~CommandPool();
		KYTY_CLASS_NO_COPY(CommandPool);

		vk::CommandBuffer Commit();

	private:
		static constexpr size_t GrowStep = 4;

		size_t Grow();

		GraphicContext&                m_graphics;
		MasterSemaphore&               m_master;
		vk::CommandPool                m_pool = nullptr;
		std::vector<vk::CommandBuffer> m_buffers;
		std::vector<uint64_t>          m_ticks;
		size_t                         m_hint = 0;
	};

	enum class OperationState { Open, Draining, Closed };

	struct PendingOperation {
		Common::UniqueFunction<void> callback;
		uint64_t                     tick = 0;
	};

	void BeginNext();
	void PriorityOperationsThread(std::stop_token stop);
	void RunOperation(Common::UniqueFunction<void>&& operation);

	MasterSemaphore              m_master;
	RenderContext&               m_context;
	GraphicContext&              m_graphics;
	CommandPool                  m_command_pool;
	CommandBuffer                m_command;
	std::queue<PendingOperation> m_pending_operations;
	std::queue<PendingOperation> m_priority_operations;
	std::mutex                   m_operation_mutex;
	std::condition_variable      m_operation_available;
	std::jthread                 m_priority_thread;
	bool                         m_priority_active      = false;
	uint64_t                     m_priority_active_tick = 0;
	OperationState               m_operation_state      = OperationState::Open;
	// Guarded by m_operation_mutex, alongside callback registration and the
	// queued-submit tick transition. Captured into each owned submission record.
	bool                         m_preserve_current_completion = false;
	// Aggregate tracing reasons, guarded by m_operation_mutex. They never affect submission.
	bool                         m_diagnostic_eop_completion     = false;
	bool                         m_diagnostic_generic_completion = false;
	// KYTY_GPU_TIMING ring; null when disabled. Owned by the recording producer, like m_command.
	std::unique_ptr<GpuTimestampRing> m_gpu_timing;
	// Guest scheduler with KYTY_GPU_OP_PROFILE / counters enabled (gpuOpProfiler.h).
	bool m_gpu_ops = false;
};

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_COMMANDSCHEDULER_H_
