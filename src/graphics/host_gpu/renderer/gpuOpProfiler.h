#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GPUOPPROFILER_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GPUOPPROFILER_H_

// Sampled per-operation GPU profiler (KYTY_GPU_OP_PROFILE=<seconds>) and cheap GPU recording
// counters. Diagnostic only; nothing here changes what the renderer records for normal play.
//
// Mechanism
//   When enabled, InstallHooks() wraps the command-recording entry points of the vulkan.hpp
//   default dispatcher (vkCmdDraw*, vkCmdDispatch*, copies, blits, clears, fills, resolves,
//   pipeline barriers, dynamic rendering begin/end, queries). Every recording site in the
//   renderer therefore gets instrumented without per-site code. Commands recorded into anything
//   other than the guest scheduler's current command buffer are passed through untouched.
//
//   Every <seconds>, the first guest command buffer begun after a guest flip starts a capture.
//   It resets a large timestamp query pool and writes an ALL_COMMANDS timestamp at the start of
//   every guest command buffer and directly after every hooked operation. The capture ends at
//   the next guest flip (command-buffer granularity: a buffer begun before the flip is recorded
//   to its end). Results are read with vkGetQueryPoolResults without WAIT once the scheduler
//   knows the last captured tick has completed, then written by a background thread.
//
// Attribution caveat (also written to gpuops-README.txt)
//   An ALL_COMMANDS timestamp is written once all previously submitted work has completed. The
//   per-op delta_ns is "completion time of this op minus completion time of the previous stamp
//   in the same command buffer". GPUs overlap consecutive draws/dispatches, so this attributes
//   the incremental completion time, not the isolated cost: a cheap op following an expensive
//   one can absorb part of its tail, and barriers usually show near-zero because the previous
//   op already waited for the drain. Totals per category/site/pipeline are still a good
//   heuristic of where the frame's GPU time goes. The per-op timestamps themselves can reduce
//   overlap, so the captured frame's total is usually larger than an unprofiled frame's
//   gpu_busy_us. Gaps between guest command buffers (presenter work, CPU starvation) are
//   reported separately on cb_begin rows and are not added to op totals.
//
// Outputs (hang-trace directory when KYTY_HANG_TRACE=1, else KYTY_GPU_OP_PROFILE_DIR, else
// ./_Profiling/gpuops-<timestamp>-pid<pid>):
//   gpuops-<flip>.csv     one row per recorded operation of one captured frame
//   gpuops-summary.csv    appended per capture: totals, time by op kind/category/site/scope,
//                         top pipelines, render targets and callers, barrier counts by site
//   gpuops-README.txt     column descriptions and the caveats above
//
// Environment
//   KYTY_GPU_OP_PROFILE=<seconds>      capture period (fractional allowed); unset/0 = off
//   KYTY_GPU_OP_PROFILE_MAX=<n>        stop after n captures (default 0 = unlimited)
//   KYTY_GPU_OP_PROFILE_QUERIES=<n>    timestamp query capacity per capture (default 262144)
//   KYTY_GPU_OP_PROFILE_DIR=<dir>      output directory when the hang trace is off
//   KYTY_GPU_OP_COUNTERS=1/0           force the cheap counters on/off. Unset, they follow
//                                      KYTY_GPU_OP_PROFILE, KYTY_HANG_TRACE=1 or Profiler
//                                      aggregate diagnostics. Counters: guest render-pass
//                                      begins, pipeline barrier calls (total and per site),
//                                      image layout transitions and guest command buffers,
//                                      published per guest flip to HangTrace summary.csv and
//                                      Profiler FrameEvents / Tracy plots.
//
// With everything unset no hook is installed and each KYTY_GPU_OP_SITE scope costs one
// predictable branch on a global flag.

#include "common/common.h"
#include "graphics/host_gpu/vulkanCommon.h"

#include <atomic>
#include <cstdint>

namespace Libs::Graphics {

struct GraphicContext;

namespace GpuOpProfiler {

// A static call-site tag. Constant-initialized; linked into a global list on first use while
// the profiler is active.
struct Site {
	const char*            name;
	const char*            plot_name = nullptr; // "GpuOps.Barriers.<name>", set when linked
	std::atomic<uint64_t>  barriers {0};        // cumulative guest pipeline barrier calls
	std::atomic<Site*>     next {nullptr};
	std::atomic<bool>      linked {false};

	constexpr explicit Site(const char* site_name) noexcept: name(site_name) {}
	Site(const Site&)            = delete;
	Site& operator=(const Site&) = delete;
};

namespace Detail {
// Written once by InstallHooks() before any renderer thread records commands.
extern bool g_active;
void        EnterSite(Site& site, Site*& previous, bool& scope_owner) noexcept;
void        LeaveSite(Site* previous, bool scope_owner) noexcept;
} // namespace Detail

// True once hooks are installed (counters and/or sampled capture).
[[nodiscard]] inline bool Active() noexcept {
	return Detail::g_active;
}

// Tags the operations recorded on this thread while in scope. The innermost tag is the op's
// "site"; the outermost is its "scope" (e.g. site image.transition within scope draw).
class ScopedSite {
public:
	explicit ScopedSite(Site& site) noexcept {
		if (Detail::g_active) [[unlikely]] {
			m_set = true;
			Detail::EnterSite(site, m_previous, m_scope_owner);
		}
	}
	~ScopedSite() {
		if (m_set) [[unlikely]] {
			Detail::LeaveSite(m_previous, m_scope_owner);
		}
	}
	ScopedSite(const ScopedSite&)            = delete;
	ScopedSite& operator=(const ScopedSite&) = delete;

private:
	Site* m_previous    = nullptr;
	bool  m_set         = false;
	bool  m_scope_owner = false;
};

// Environment-derived, evaluated once.
[[nodiscard]] bool Enabled();        // counters or capture requested
[[nodiscard]] bool CaptureEnabled(); // KYTY_GPU_OP_PROFILE > 0

// Directly after VULKAN_HPP_DEFAULT_DISPATCHER.init(device). No-op unless Enabled().
void InstallHooks(GraphicContext& graphics);

// Guest command scheduler only, directly after vkBeginCommandBuffer (and the KYTY_GPU_TIMING
// start stamp), outside any rendering scope. current_tick is the tick this buffer will signal.
void OnBeginCommand(GraphicContext& graphics, vk::CommandBuffer buffer, uint64_t current_tick,
                    uint64_t known_gpu_tick);
// Guest scheduler shutdown, after every submitted tick completed: collects an open capture,
// destroys the query pool and flushes the writer thread.
void OnSchedulerShutdown(GraphicContext& graphics);

// Once per completed guest flip (any thread): frame boundary and counter publication.
void OnGuestFlip();

// Shader/pipeline identity for per-pipeline attribution. Cheap no-ops unless CaptureEnabled().
void RegisterShader(uint64_t program_id, const char* stage, uint64_t guest_hash);
void RegisterGraphicsPipeline(vk::Pipeline pipeline, const uint64_t* vertex_program_ids,
                              uint32_t vertex_program_count, uint64_t pixel_program_id);
void RegisterComputePipeline(vk::Pipeline pipeline, uint64_t compute_program_id);

} // namespace GpuOpProfiler
} // namespace Libs::Graphics

#define KYTY_GPU_OP_CONCAT_IMPL(a, b) a##b
#define KYTY_GPU_OP_CONCAT(a, b)      KYTY_GPU_OP_CONCAT_IMPL(a, b)
// Tags GPU operations recorded until the end of the enclosing scope with a static site name.
#define KYTY_GPU_OP_SITE(name) KYTY_GPU_OP_SITE_IMPL(__LINE__, name)
#define KYTY_GPU_OP_SITE_IMPL(line, name)                                                          \
	static constinit ::Libs::Graphics::GpuOpProfiler::Site KYTY_GPU_OP_CONCAT(kyty_gpu_op_site_,   \
	                                                                          line) {name};        \
	const ::Libs::Graphics::GpuOpProfiler::ScopedSite KYTY_GPU_OP_CONCAT(kyty_gpu_op_scope_,       \
	                                                                     line) {                   \
	    KYTY_GPU_OP_CONCAT(kyty_gpu_op_site_, line)}

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_RENDERER_GPUOPPROFILER_H_
