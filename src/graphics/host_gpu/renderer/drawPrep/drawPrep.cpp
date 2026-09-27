#include "graphics/host_gpu/renderer/drawPrep/drawPrep.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "graphics/guest_gpu/graphicsRun.h"
#include "graphics/guest_gpu/pm4.h"
#include "graphics/host_gpu/coherenceLog.h"
#include "graphics/host_gpu/renderer/commandScheduler.h"
#include "graphics/host_gpu/renderer/debug.h"
#include "graphics/host_gpu/renderer/drawPrep/window.h"
#include "graphics/host_gpu/renderer/image/textureCommon.h"
#include "graphics/host_gpu/renderer/renderContext.h"
#include "graphics/shader/shader.h"
#include "graphics/shader/shaderCompiler.h"
#include "kernel/memory.h"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>

#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>
#endif

namespace Libs::Graphics::DrawPrep {

namespace {

const char* EnvValue(const char* name) {
	const auto* value = std::getenv(name);
	return value != nullptr && *value != '\0' ? value : nullptr;
}

Profiler::FrameEvent FallbackEvent(Failure failure) {
	using E = Profiler::FrameEvent;
	switch (failure) {
		case Failure::Unclean: return E::DrawPrepFallbackUnclean;
		case Failure::Backing: return E::DrawPrepFallbackBacking;
		case Failure::Overflow: return E::DrawPrepFallbackOverflow;
		case Failure::Inconsistent: return E::DrawPrepFallbackInconsistent;
		case Failure::Uncertified: return E::DrawPrepFallbackUncertified;
		case Failure::NotPublished: return E::DrawPrepFallbackNotPublished;
		case Failure::ShaderMap: return E::DrawPrepFallbackShaderMap;
		case Failure::CertUnclean: return E::DrawPrepFallbackCertUnclean;
		case Failure::CertChanged: return E::DrawPrepFallbackCertChanged;
		case Failure::CoherenceLog: return E::DrawPrepFallbackCoherenceLog;
		case Failure::Mismatch: return E::DrawPrepFallbackMismatch;
		case Failure::None:
		case Failure::Ineligible: break;
	}
	return E::DrawPrepFallbackIneligible;
}

Failure FromReadFailure(ReadFailure failure) {
	switch (failure) {
		case ReadFailure::Unclean: return Failure::Unclean;
		case ReadFailure::Backing: return Failure::Backing;
		case ReadFailure::Overflow: return Failure::Overflow;
		case ReadFailure::Inconsistent: return Failure::Inconsistent;
		case ReadFailure::Uncertified: return Failure::Uncertified;
		case ReadFailure::None: break;
	}
	return Failure::Uncertified;
}

// The same decisions RenderExecutor makes from the command buffer's registers
// (DrawHasActivePixelShader, RefreshShaders); the commit compares both.
bool HasActivePixelShader(const RegisterSnapshot& registers) {
	const auto& ctx     = registers.context;
	const auto& sh_regs = ctx.GetShaderRegisters();
	const auto& db      = sh_regs.db_shader_control;
	const bool  has_color_output = (ctx.GetRenderTargetMask() & sh_regs.m_cbShaderMask) != 0;
	const bool  side_effects = db.shader_kill_enable || db.shader_z_export_enable ||
	                          db.shader_mask_export_enable || db.shader_dual_export_enable ||
	                          db.shader_execute_on_noop;
	return registers.shaders.GetPs().ps_regs.data_addr != 0 && (has_color_output || side_effects);
}

void TargetExportMapping(const HW::Context&                              ctx,
                         std::array<Prospero::ColorComponentMapping, 8>& mapping) {
	mapping = {};
	for (uint32_t slot = 0; slot < RENDER_COLOR_ATTACHMENTS_MAX; slot++) {
		const auto& rt = ctx.GetRenderTarget(slot);
		if (rt.base.addr != 0 && render_target_mask_slot(ctx.GetRenderTargetMask(), slot) != 0) {
			mapping[slot] = TextureGetRenderTargetFormat(rt.info.format, rt.info.channel_type,
			                                             rt.info.channel_order)
			                    .export_mapping;
		}
	}
}

bool SameMapping(std::span<const Prospero::ColorComponentMapping, 8> a,
                 std::span<const Prospero::ColorComponentMapping, 8> b) {
	for (size_t i = 0; i < a.size(); i++) {
		if (a[i].packed != b[i].packed) {
			return false;
		}
	}
	return true;
}

} // namespace

Mode GetMode() {
	static const Mode mode = [] {
		const auto* value = EnvValue("KYTY_DRAW_PREP");
		if (value == nullptr || std::strcmp(value, "off") == 0 || std::strcmp(value, "0") == 0) {
			return Mode::Off;
		}
		if (std::strcmp(value, "inline") == 0) {
			return Mode::Inline;
		}
		if (std::strcmp(value, "parallel") == 0) {
			return Mode::Parallel;
		}
		EXIT("KYTY_DRAW_PREP must be off, inline or parallel (got '%s')\n", value);
	}();
	return mode;
}

int VerifyMode() {
	static const int mode = [] {
		const auto* value = EnvValue("KYTY_DRAW_PREP_VERIFY");
		if (value == nullptr || std::strcmp(value, "0") == 0) {
			return 0;
		}
		return std::strcmp(value, "exit") == 0 ? 2 : 1;
	}();
	return mode;
}

CertMode GetCertMode() {
	static const CertMode mode = [] {
		const auto* value = EnvValue("KYTY_DRAW_PREP_CERT");
		return value != nullptr && std::strcmp(value, "log") == 0 ? CertMode::Log : CertMode::Value;
	}();
	return mode;
}

// KYTY_DRAW_PREP_LOG_AUDIT=1: with value certificates, also evaluate the log check and count
// where it would have decided differently (DrawPrepLogWouldReject, DrawPrepLogMissed).
static bool LogAuditEnabled() {
	static const bool enabled = [] {
		const auto* value = EnvValue("KYTY_DRAW_PREP_LOG_AUDIT");
		return value != nullptr && std::strcmp(value, "0") != 0;
	}();
	return enabled;
}

bool PacketHookEnabled() {
	static const bool enabled = [] {
		const auto* value = EnvValue("KYTY_DRAW_PREP_HISTOGRAM");
		return GetMode() != Mode::Off || (value != nullptr && std::strcmp(value, "0") != 0);
	}();
	return enabled;
}

void Prepare(PipelineCache& pipeline_cache, const RegisterSnapshot& registers, bool eligible,
             bool exact, PreparedDraw& prepared) {
	prepared.ok         = false;
	prepared.failure    = Failure::None;
	prepared.programs   = {};
	prepared.pixel_info = {};
	prepared.reads.Reset();
	if (!eligible) {
		prepared.failure = Failure::Ineligible;
		return;
	}
	Profiler::ScopedFrameWait wait(Profiler::FrameWait::DrawPrepPrepare);
	// Loaded before any read: every coherence transition after this point is newer.
	prepared.coherence_generation  = Coherence::Generation();
	prepared.shader_map_generation = ShaderMapGeneration();

	const auto& ctx    = registers.context;
	const auto& sh_ctx = registers.shaders;
	if (sh_ctx.GetVs().es_regs.data_addr == 0) {
		prepared.failure = Failure::Ineligible;
		return;
	}
	prepared.pixel_active = HasActivePixelShader(registers);
	TargetExportMapping(ctx, prepared.target_export_mapping);

	Recorder    recorder {&prepared.reads, exact};
	RecordScope scope(recorder);
	const auto  result = pipeline_cache.PrepareGraphicsProgramsSpeculative(
	    sh_ctx.GetVs(), sh_ctx.GetPs(), ctx.GetShaderRegisters(), ctx, registers.user_config,
	    prepared.target_export_mapping, prepared.pixel_active, prepared.vertex_info,
	    prepared.pixel_info, prepared.vertex_prep, prepared.pixel_prep, prepared.programs);
	switch (result) {
		case PipelineCache::SpeculativeResult::Ok: break;
		case PipelineCache::SpeculativeResult::Ineligible:
			prepared.failure = Failure::Ineligible;
			return;
		case PipelineCache::SpeculativeResult::NotPublished:
			prepared.failure = prepared.reads.Failed() ? FromReadFailure(prepared.reads.Failure())
			                                           : Failure::NotPublished;
			return;
		case PipelineCache::SpeculativeResult::ReadFailed:
			prepared.failure = FromReadFailure(prepared.reads.Failure());
			return;
	}
	if (!prepared.reads.Finish()) {
		prepared.failure = FromReadFailure(prepared.reads.Failure());
		return;
	}
	prepared.ok = true;
}

// Certificate modes:
// - value (default): every coalesced read range must be clean for a backing read now and hold
//   the recorded bytes. Sound on its own (readSet.h); the coherence log is not consulted.
// - log: the plan's cheaper check. No coherence transition logged since the preparation began
//   may touch a read range, and every range must be clean now; bytes are not compared. It relies
//   on the log being complete for emulator-side changes and treats unsynchronized guest CPU
//   writes as races (they are: only a fence orders them against a draw). Validate it with
//   KYTY_DRAW_PREP_VERIFY before trusting it.
bool Validate(PreparedDraw& prepared, bool pixel_active,
              std::span<const Prospero::ColorComponentMapping, 8> target_export_mapping) {
	const auto fail = [&](Failure failure) {
		prepared.failure = failure;
		Profiler::CountFrameEvent(FallbackEvent(failure));
		return false;
	};
	if (!prepared.ok) {
		return fail(prepared.failure);
	}
	if (prepared.pixel_active != pixel_active ||
	    !SameMapping(prepared.target_export_mapping, target_export_mapping)) {
		return fail(Failure::Mismatch);
	}
	if (ShaderMapGeneration() != prepared.shader_map_generation) {
		return fail(Failure::ShaderMap);
	}
	Profiler::ScopedFrameWait wait(Profiler::FrameWait::DrawPrepValidate);
	const auto ranges = prepared.reads.Ranges();
	if (GetCertMode() == CertMode::Log) {
		const auto outcome =
		    Coherence::g_log.Check(prepared.coherence_generation, Coherence::Generation(), ranges);
		if (outcome.result != Coherence::CheckResult::Clean) {
			return fail(Failure::CoherenceLog);
		}
		if (!prepared.reads.AllClean(LibKernel::Memory::IsGpuCleanForRead)) {
			return fail(Failure::CertUnclean);
		}
	} else {
		static thread_local std::vector<uint8_t> scratch;
		const auto result = prepared.reads.Validate(LibKernel::Memory::TryReadGpuCleanBacking, scratch);
		if (LogAuditEnabled()) {
			// Would the log-mode certificate have decided the same? (Unclean ranges fail both.)
			const bool log_clean = Coherence::g_log
			                           .Check(prepared.coherence_generation,
			                                  Coherence::Generation(), ranges)
			                           .result == Coherence::CheckResult::Clean;
			if (log_clean && result == ValidateResult::Changed) {
				Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepLogMissed);
			} else if (!log_clean && result == ValidateResult::Ok) {
				Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepLogWouldReject);
			}
		}
		switch (result) {
			case ValidateResult::Ok: break;
			case ValidateResult::Unclean: return fail(Failure::CertUnclean);
			case ValidateResult::Changed: return fail(Failure::CertChanged);
		}
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepCommitted);
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepCertRanges, ranges.size());
	uint64_t bytes = 0;
	for (const auto& range: ranges) {
		bytes += range.end - range.begin;
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepCertBytes, bytes);
	return true;
}

namespace {

bool SameSnapshot(const ShaderRecompiler::IR::ResourceSnapshot& a,
                  const ShaderRecompiler::IR::ResourceSnapshot& b) {
	return a.buffers == b.buffers && a.images == b.images && a.samplers == b.samplers &&
	       a.flattened_srt == b.flattened_srt && a.user_data == b.user_data &&
	       a.uniform_fill == b.uniform_fill;
}

bool SamePrep(const PipelineCache::StagePrep& a, const PipelineCache::StagePrep& b) {
	return SameSnapshot(a.resources, b.resources) && a.specialization == b.specialization &&
	       a.permutation == b.permutation;
}

bool SameVertexInfo(const ShaderVertexInputInfo& a, const ShaderVertexInputInfo& b) {
	std::vector<uint32_t> key_a;
	std::vector<uint32_t> key_b;
	BuildStageStaticKey(a, key_a);
	BuildStageStaticKey(b, key_b);
	if (key_a != key_b || a.logical_stage != b.logical_stage || a.buffers_num != b.buffers_num ||
	    a.fetch_external != b.fetch_external || a.stage.program != b.stage.program) {
		return false;
	}
	for (int i = 0; i < a.resources_num; i++) {
		if (std::memcmp(a.resources[i].fields, b.resources[i].fields,
		                sizeof(a.resources[i].fields)) != 0) {
			return false;
		}
	}
	for (int i = 0; i < a.buffers_num; i++) {
		const auto& x = a.buffers[i];
		const auto& y = b.buffers[i];
		if (x.addr != y.addr || x.stride != y.stride || x.num_records != y.num_records ||
		    x.fetch_index != y.fetch_index || x.attr_num != y.attr_num) {
			return false;
		}
		for (int j = 0; j < x.attr_num; j++) {
			if (x.attr_indices[j] != y.attr_indices[j] || x.attr_offsets[j] != y.attr_offsets[j]) {
				return false;
			}
		}
	}
	return true;
}

bool SamePixelInfo(const ShaderPixelInputInfo& a, const ShaderPixelInputInfo& b) {
	std::vector<uint32_t> key_a;
	std::vector<uint32_t> key_b;
	BuildStageStaticKey(a, key_a);
	BuildStageStaticKey(b, key_b);
	return key_a == key_b && a.stage.program == b.stage.program;
}

} // namespace

bool VerifyCommitted(bool pixel_active, const PipelineCache::GraphicsPrograms& programs,
                     const ShaderVertexInputInfo& vertex_info, const ShaderPixelInputInfo& pixel_info,
                     const PipelineCache::GraphicsStagePreps& preps,
                     const PipelineCache::GraphicsPrograms& serial_programs,
                     const ShaderVertexInputInfo&            serial_vertex_info,
                     const ShaderPixelInputInfo&             serial_pixel_info,
                     const PipelineCache::GraphicsStagePreps& serial_preps) {
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepVerifyChecks);
	const bool programs_equal = programs.pixel.id == serial_programs.pixel.id &&
	                            programs.vertex[0].id == serial_programs.vertex[0].id &&
	                            programs.vertex[1].id == serial_programs.vertex[1].id;
	// Without an active pixel shader neither path writes the pixel prep (the draw state keeps
	// an earlier draw's, which nothing reads).
	const bool pixel_equal = SamePixelInfo(pixel_info, serial_pixel_info) &&
	                         (!pixel_active || SamePrep(preps.pixel, serial_preps.pixel));
	const bool vertex_equal = SamePrep(preps.vertex[0], serial_preps.vertex[0]) &&
	                          SameVertexInfo(vertex_info, serial_vertex_info);
	if (programs_equal && pixel_equal && vertex_equal) {
		return true;
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepVerifyMismatches);
	static std::atomic<uint32_t> logged {0};
	if (logged.fetch_add(1, std::memory_order_relaxed) < 32) {
		LOGF("DrawPrepVerify: committed preparation differs from the serial one: programs=%d "
		     "pixel=%d vertex=%d ps=%" PRIu64 "/%" PRIu64 " vs=%" PRIu64 "/%" PRIu64 "\n",
		     programs_equal, pixel_equal, vertex_equal, programs.pixel.id, serial_programs.pixel.id,
		     programs.vertex[0].id, serial_programs.vertex[0].id);
	}
	if (VerifyMode() == 2) {
		EXIT("DrawPrepVerify: committed preparation differs from the serial path\n");
	}
	return false;
}

// ---------------------------------------------------------------------------------------------
// Engine

struct Engine::Slot {
	DrawKind         kind      = DrawKind::Index;
	uint64_t         submit_id = 0;
	DrawIndexArgs    index_args {};
	DrawAutoArgs     auto_args {};
	RegisterSnapshot registers;
	PreparedDraw     prepared;
	bool             eligible        = false; // DrawReachesPrograms, decided at submission
	bool             worker_prepared = false;
};

namespace {

uint32_t EnvUnsigned(const char* name, uint32_t fallback, uint32_t low, uint32_t high) {
	const auto* value = EnvValue(name);
	if (value == nullptr) {
		return fallback;
	}
	const auto parsed = std::strtoul(value, nullptr, 10);
	return static_cast<uint32_t>(std::clamp<unsigned long>(parsed, low, high));
}

void CpuRelax() {
#if defined(_M_X64) || defined(__x86_64__)
	_mm_pause();
#else
	std::this_thread::yield();
#endif
}

uint64_t NowNs() {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
	                                 std::chrono::steady_clock::now().time_since_epoch())
	                                 .count());
}

} // namespace

// The parallel-mode window and its DrawPrep#k threads. Workers only run Prepare on claimed
// slots (register snapshot in, preparation out); everything else stays on the GPU thread.
struct Engine::Workers {
	Workers(PipelineCache& pipeline_cache, uint32_t window, uint32_t count, uint64_t spin_ns)
	    : pipeline_cache(pipeline_cache), window(window), spin_ns(spin_ns) {
		for (uint32_t index = 0; index < count; index++) {
			threads.emplace_back([this, index] { Run(index); });
		}
	}

	~Workers() {
		stop.store(true, std::memory_order_seq_cst);
		signal.fetch_add(1, std::memory_order_seq_cst);
		signal.notify_all();
		for (auto& thread: threads) {
			thread.join();
		}
	}

	Workers(const Workers&)            = delete;
	Workers& operator=(const Workers&) = delete;

	// Producer side, after Publish: wake parked workers. Paired with Run's sleepers increment
	// and HasClaimable() reload (both seq_cst): either the worker sees the new slot before
	// parking, or this thread sees it parking and changes the signal it waits on.
	void Wake() {
		if (sleepers.load(std::memory_order_seq_cst) != 0) {
			signal.fetch_add(1, std::memory_order_seq_cst);
			signal.notify_all();
		}
	}

	void Run(uint32_t index) {
		char name[32];
		std::snprintf(name, sizeof(name), "DrawPrep#%u", index + 1u);
		Profiler::SetThreadName(name);
		t_worker_thread = true;
		auto idle_start = NowNs();
		uint32_t spins  = 0;
		while (!stop.load(std::memory_order_acquire)) {
			uint64_t seq  = 0;
			if (auto* slot = window.TryClaim(seq); slot != nullptr) {
				Prepare(pipeline_cache, slot->registers, slot->eligible, false, slot->prepared);
				slot->worker_prepared = true;
				window.Complete(seq);
				idle_start = NowNs();
				spins      = 0;
				continue;
			}
			CpuRelax();
			if ((++spins & 255u) != 0u || NowNs() - idle_start < spin_ns) {
				continue;
			}
			const auto observed = signal.load(std::memory_order_seq_cst);
			sleepers.fetch_add(1, std::memory_order_seq_cst);
			if (!window.HasClaimable() && !stop.load(std::memory_order_seq_cst)) {
				signal.wait(observed, std::memory_order_seq_cst);
			}
			sleepers.fetch_sub(1, std::memory_order_seq_cst);
			idle_start = NowNs();
		}
	}

	PipelineCache&           pipeline_cache;
	Window<Slot>             window;
	uint64_t                 spin_ns = 0;
	std::atomic<bool>        stop {false};
	std::atomic<uint64_t>    signal {0};
	std::atomic<uint32_t>    sleepers {0};
	std::vector<std::thread> threads;
};

Engine::Engine(RenderContext& renderer, std::function<void()> service_commands)
    : m_renderer(renderer), m_mode(GetMode()), m_service_commands(std::move(service_commands)),
      m_inline_slot(std::make_unique<Slot>()) {
	if (m_mode == Mode::Parallel) {
		const auto window  = EnvUnsigned("KYTY_DRAW_PREP_WINDOW", 32, 2, 1024);
		const auto workers = EnvUnsigned("KYTY_DRAW_PREP_WORKERS", 6, 1, 32);
		const auto spin_us = EnvUnsigned("KYTY_DRAW_PREP_SPIN_US", 200, 0, 1000000);
		m_workers = std::make_unique<Workers>(m_renderer.GetPipelineCache(), window, workers,
		                                      uint64_t {spin_us} * 1000u);
		LOGF("DrawPrep: parallel mode, window=%u workers=%u spin=%uus cert=%s verify=%d\n",
		     m_workers->window.Capacity(), workers, spin_us,
		     GetCertMode() == CertMode::Log ? "log" : "value", VerifyMode());
	}
}

Engine::~Engine() {
	// Pending draws at teardown are dropped with the stream (shutdown only).
	m_workers.reset();
}

bool Engine::Pending() const noexcept {
	return m_workers != nullptr && !m_workers->window.Empty();
}

void Engine::FillSlot(Slot& slot, uint64_t submit_id, const DrawIndexArgs* index_args,
                      const DrawAutoArgs* auto_args, const HW::Context& context,
                      const HW::UserConfig& user_config, const HW::Shader& shaders) {
	slot.submit_id = submit_id;
	uint32_t count = 0;
	uint32_t instances = 0;
	if (index_args != nullptr) {
		slot.kind       = DrawKind::Index;
		slot.index_args = *index_args;
		count           = index_args->index_count;
		instances       = index_args->instance_count;
	} else {
		slot.kind      = DrawKind::Auto;
		slot.auto_args = *auto_args;
		count          = auto_args->vertex_count;
		instances      = auto_args->instance_count;
	}
	slot.registers.context     = context;
	slot.registers.user_config = user_config;
	slot.registers.shaders     = shaders;
	slot.eligible        = DrawReachesPrograms(context, user_config, shaders, count, instances);
	slot.worker_prepared = false;
}

bool Engine::Submit(uint64_t submit_id, const DrawIndexArgs* index_args,
                    const DrawAutoArgs* auto_args, const HW::Context& context,
                    const HW::UserConfig& user_config, const HW::Shader& shaders) {
	EXIT_IF(!GuestGpu::IsGpuThread());
	EXIT_IF((index_args == nullptr) == (auto_args == nullptr));
	if (m_mode == Mode::Off) {
		return false;
	}
	m_draws_since_fence++;
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepSubmitted);
	if (m_workers == nullptr) {
		// Inline: prepare on this thread from the snapshot, then commit at once.
		auto& slot = *m_inline_slot;
		FillSlot(slot, submit_id, index_args, auto_args, context, user_config, shaders);
		Prepare(m_renderer.GetPipelineCache(), slot.registers, slot.eligible, true, slot.prepared);
		Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepSelfPrepared);
		Commit(slot);
		return true;
	}
	auto& window = m_workers->window;
	if (window.Full()) {
		CommitHead();
	}
	FillSlot(window.Reserve(), submit_id, index_args, auto_args, context, user_config, shaders);
	window.Publish();
	m_workers->Wake();
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepPublished);
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepWindowOccupancy, window.Occupancy());
	return true;
}

void Engine::CommitHead() {
	auto& window = m_workers->window;
	EXIT_IF(window.Empty());
	auto& slot = window.HeadPayload();
	if (window.TryClaimHead()) {
		// No worker has started it: prepare it here, with the exact clean predicate.
		Prepare(m_renderer.GetPipelineCache(), slot.registers, slot.eligible, true, slot.prepared);
		Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepSelfPrepared);
	} else if (window.HeadDone()) {
		Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepReady);
	} else {
		Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepCommitWaits);
		Profiler::ScopedFrameWait wait(Profiler::FrameWait::DrawPrepCommitWait);
		const auto wait_start = NowNs();
		for (uint32_t spins = 1; !window.HeadDone(); spins++) {
			CpuRelax();
			// Workers never dereference guest memory, so they cannot page fault into a request
			// this thread must service. Only as a last-resort deadlock guard, after 2 ms, pending
			// cross-thread commands are serviced here; such a command then observes the window
			// partially committed, as if it had arrived just before these draws were parsed.
			if ((spins & 1023u) == 0u && m_service_commands &&
			    NowNs() - wait_start > 2'000'000u) {
				m_service_commands();
			}
		}
	}
	Commit(slot);
	window.Retire();
}

void Engine::Drain() {
	if (!Pending()) {
		return;
	}
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepDrains);
	while (Pending()) {
		CommitHead();
	}
}

void Engine::Commit(Slot& slot) {
	auto&      scheduler = m_renderer.GetCommandScheduler();
	auto&      executor  = m_renderer.GetRenderExecutor();
	const auto previous  = scheduler.BindRegisters(slot.registers.context,
	                                               slot.registers.user_config,
	                                               slot.registers.shaders);
	executor.m_prepared_draw = &slot.prepared;
	if (slot.kind == DrawKind::Index) {
		executor.DrawIndex(slot.submit_id, scheduler.Current(), slot.index_args);
	} else {
		executor.DrawAuto(slot.submit_id, scheduler.Current(), slot.auto_args);
	}
	if (executor.m_prepared_draw != nullptr) {
		// The draw returned before preparing its programs (nothing to draw, a metadata
		// operation, no targets): the preparation is simply dropped.
		executor.m_prepared_draw = nullptr;
		Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepUnused);
	}
	scheduler.RestoreRegisters(previous);
}

void Engine::NoteFence() {
	Profiler::CountFrameEvent(Profiler::FrameEvent::DrawPrepFences);
	using E          = Profiler::FrameEvent;
	const auto draws = m_draws_since_fence;
	const auto event = draws == 0    ? E::DrawPrepFenceDraws0
	                   : draws == 1  ? E::DrawPrepFenceDraws1
	                   : draws < 4   ? E::DrawPrepFenceDraws2To3
	                   : draws < 8   ? E::DrawPrepFenceDraws4To7
	                   : draws < 16  ? E::DrawPrepFenceDraws8To15
	                   : draws < 32  ? E::DrawPrepFenceDraws16To31
	                   : draws < 64  ? E::DrawPrepFenceDraws32To63
	                                 : E::DrawPrepFenceDraws64Plus;
	Profiler::CountFrameEvent(event);
	m_draws_since_fence = 0;
}

void Engine::OnPacket(PacketClass packet_class) {
	switch (packet_class) {
		case PacketClass::WindowSafe: break;
		case PacketClass::Draw:
			// Off mode (histogram only) counts here; inline/parallel count in Submit, which
			// also sees draws issued through other paths.
			if (m_mode == Mode::Off) {
				m_draws_since_fence++;
			}
			break;
		case PacketClass::Fence:
			NoteFence();
			Drain();
			break;
	}
}

} // namespace Libs::Graphics::DrawPrep
