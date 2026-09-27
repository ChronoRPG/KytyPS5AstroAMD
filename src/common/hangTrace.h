#ifndef EMULATOR_INCLUDE_EMULATOR_COMMON_HANGTRACE_H_
#define EMULATOR_INCLUDE_EMULATOR_COMMON_HANGTRACE_H_

// Loading/hang flight recorder.
//
// Always-cheap, opt-in (KYTY_HANG_TRACE=1) recorder that runs from process start without a
// profiler connection. A background thread writes plain CSV files once per second, so an
// intermittent failure is captured whether or not anyone armed a trace beforehand. Two runs
// (one that loads, one that does not) can then be diffed directly.
//
// Output directory: KYTY_HANG_TRACE_DIR, or ./_HangTrace/<timestamp>-pid<pid>.
//   summary.csv        per-second totals (flips, APR reads/repeats, LOD packets, textures with
//                      mip-stat counters, per-queue GPU wait/busy, AgcSuspendPoint/Done waits)
//   apr.csv            every APR file read: path, range, destination, repeat count, guest callers
//   imports.csv        per-second call counts of every guest->HLE import (KYTY_HANG_TRACE_IMPORTS)
//   imports-index.csv  import id -> NID / host function / importing module
//   lodstats.csv       sampled GET_LOD_STATS packets with the buffer contents *before* the
//                      emulator overwrites them (shows what the guest left there)
//   texstats.csv       per-second use of T# mip-statistics counters (id, base, min LOD)
//   modules.csv        guest module load addresses, to map caller addresses to module offsets
//   queues.csv         per-second, per active guest GPU queue: submission wait and busy time
//   readbacks.csv      every GPU->CPU readback: cause, range, duration, guest thread and pc
//   images.csv         texture-cache image deletions with reason, and per-second native image
//                      create/destroy totals grouped by format, extent and usage
//   transfers.csv      per-second image uploads, buffer uploads, reinterpretation copies and
//                      alias synchronizations grouped by reason, address and shape (RecordTransfer)
//
// summary.csv ends with GPU timeline columns (KYTY_GPU_TIMING, default on with this trace), summed
// over the guest flips published in that second: gpu_busy_us (union of command-buffer spans),
// gpu_cmdbufs, gpu_latency_avg_us (recording start -> GPU start), gpu_idle_us, gpu_max_gap_us,
// gpu_starved_us (idle before the next buffer reached vkQueueSubmit), gpu_dispatch_latency_avg_us
// (vkQueueSubmit return -> GPU start) and gpu_dropped. Latency/starved columns are 0 without
// calibrated timestamps. After them come GPU recording counters (KYTY_GPU_OP_COUNTERS, default on
// with this trace; see graphics/host_gpu/renderer/gpuOpProfiler.h): gpu_render_passes (guest
// dynamic-rendering begins), gpu_barriers (guest pipeline barrier calls), gpu_layout_transitions
// (image barriers changing layout) and gpu_guest_cmdbufs (guest command buffers begun). Then the
// barrier batcher (KYTY_BARRIER_BATCH, graphics/host_gpu/renderer/render.h): gpu_barrier_requests,
// gpu_barriers_merged (joined an already pending batch), gpu_barriers_elided (covered by the
// previous barrier with nothing recorded since), gpu_barriers_sunk (a pending batch kept across a
// draw in the same rendering instance) and gpu_barrier_rp_splits (barrier flushes that ended an
// active rendering instance). After the transfer columns: gpu_rendering_ends (guest rendering
// instances ended, every cause; per-site Tracy plots GpuOps.EndRendering.<site>) and
// gpu_draw_write_sinks (post-draw shader-write barriers kept pending across a draw continuing the
// same instance, KYTY_DRAW_WRITE_SINK). Then shader/pipeline compilation (see RecordCompile):
// compile_programs, compile_translate_us, compile_emit_us, compile_validate_us, compile_module_us,
// compile_gfx_pipelines, compile_gfx_pipeline_us, compile_cs_pipelines, compile_cs_pipeline_us,
// compile_stall_us, compile_stall_max_us, compile_gfx_new, compile_gfx_perm, compile_gfx_variant.
// Then driver pipeline cache saves (RecordPipelineCacheSave): pcache_saves, pcache_save_kb,
// pcache_save_serialize_us, pcache_save_write_us, pcache_save_overlaps, pcache_save_overlap_us.
// Then compile_translation_reuses (programs specialized from a kept translation instead of
// translating again) and compile_clone_us (copying translations, RecordCompile clone_ns).
// Then validate_async_count and validate_async_us: spirv-val runs on the background validator
// (KYTY_SHADER_VALIDATION_ASYNC), off the compiling thread (RecordShaderValidation).
// New columns are only ever appended.
//
//   compiles.csv       one row per new shader program permutation or pipeline: phase times, the
//                      requesting thread, and for graphics pipelines which key fields differ from
//                      the closest existing pipeline of the same programs (RecordCompile)

#include <cstdint>
#include <string>
#include <string_view>

namespace HangTrace {

[[nodiscard]] bool Enabled();
[[nodiscard]] bool ImportsEnabled();

void Initialize();
void Shutdown();

// Output directory of this trace, or empty when the trace is disabled or failed to start.
[[nodiscard]] std::string OutputDirectory();

[[nodiscard]] uint64_t NowNs();

// Guest code map, used to validate stack-scanned return addresses.
void RegisterGuestCode(uint64_t base, uint64_t size, std::string_view name);
[[nodiscard]] bool IsGuestAddress(uint64_t address);

// Import call counters. Returns the address of a 64-bit counter the import thunk increments.
// One counter per distinct target, shared by all call sites.
[[nodiscard]] uint64_t* AllocateImportCounter(uint64_t target, std::string_view nid,
                                              std::string_view dbg_name, std::string_view program);
[[nodiscard]] uint64_t FindImportThunk(uint64_t target);
void                   SetImportThunk(uint64_t target, uint64_t thunk);

// APR: the guest return address of the current APR submit call on this thread.
void SetGuestCaller(uint64_t return_address);
void RecordAprRead(uint32_t file_id, std::string_view host_path, uint64_t file_offset,
                   uint64_t size, uint64_t destination, uint64_t bytes_read, int result,
                   std::string_view thread_name);

// GET_LOD_STATS: call before the emulator writes the destination.
void RecordLodStats(const void* destination, uint32_t size, uint32_t control);

// LOD report consumer discovery (KYTY_HANG_TRACE_LOD_WATCH, default on with the trace): after
// the emulator writes a report, its plain read/write pages are briefly made inaccessible; the
// first guest access is logged (pc, registers, surrounding code bytes) and the original
// protection restored. Disarm before the emulator itself reads or writes the report.
void DisarmLodReportWatch();
void ArmLodReportWatch(const void* destination, uint32_t size);
// Called from the host access-violation handler. Returns true when the fault was a watch hit.
[[nodiscard]] bool HandleLodWatchFault(uint64_t fault_vaddr, bool write, uint64_t pc,
                                       const uint64_t* gpr16, std::string_view thread_name);

// Texture descriptor (8 dwords) resolved for a draw/dispatch.
void RecordTexture(const uint32_t* fields);

// GPU->CPU readbacks (BufferCache::ReadMemory), attributed to the path that requested them.
// FaultReadSide / FaultReadDuplicate: a guest read fault served by a side copy (no drain of the
// current recording), or by waiting on another thread's pending side copy.
enum class ReadbackKind : uint8_t {
	Invalidate,
	FaultRead,
	FaultWrite,
	GpuSync,
	FaultReadSide,
	FaultReadDuplicate
};
// Guest access fault context for readbacks on this thread (instruction address, guest thread).
void SetFaultContext(uint64_t pc, std::string_view thread_name);
void ClearFaultContext();
void SetReadbackKind(ReadbackKind kind);
[[nodiscard]] ReadbackKind GetReadbackKind();
void RecordReadback(uint64_t vaddr, uint64_t size, uint64_t window_begin, uint64_t window_size,
                    bool downloaded, uint64_t duration_ns);

// occlusion.csv (KYTY_GPU_OCCLUSION=1): "dump" rows for each ZPASS_DONE dump (scopes counted since
// the previous dump, plus the latest scope's target size, colour count, depth format and depth
// address), "publish" rows with the cumulative DB 0 sample count written to the guest for that
// dump, and "predicate" rows for each SET_PREDICATION op 1 (sample delta, condition, resulting
// skip). A begin/end pair whose published counts are equal although the pair brackets counted
// scopes means the samples were rejected; one bracketing no scopes never reached a counted scope.
struct OcclusionEvent {
	const char* event        = "";
	uint64_t    address      = 0;
	uint64_t    value        = 0;
	uint32_t    scopes       = 0;
	uint32_t    width        = 0;
	uint32_t    height       = 0;
	uint32_t    colors       = 0;
	bool        has_depth    = false;
	uint32_t    depth_format = 0;
	uint64_t    depth_address = 0;
	uint32_t    condition    = 0;
	bool        skip         = false;
	std::string detail; // "draw" rows: state of a draw recorded inside a counted scope
};
void RecordOcclusion(const OcclusionEvent& event);

// lodreports.csv (KYTY_LOD_STATS_MODE=gpu): one row per GET_LOD_STATS report written to the
// guest: counters reported as sampled, their summed sample counts and mean finest mip, whether
// any completed statistics existed yet, and GPU copies issued but not yet completed (report age).
struct LodReportEvent {
	uint64_t destination      = 0;
	uint32_t control          = 0;
	bool     has_latest       = false;
	uint32_t sampled_counters = 0;
	uint64_t total_samples    = 0;
	double   mean_finest_mip  = 0.0;
	uint64_t pending_copies   = 0;
};
void RecordLodReport(const LodReportEvent& event);

// Which kind of recorded GPU write last marked a guest page GPU-owned (reported per readback).
enum class GpuWriteKind : uint8_t { ShaderStorage, OcclusionDump, Fill, Copy };
class ScopedGpuWriteKind {
public:
	explicit ScopedGpuWriteKind(GpuWriteKind kind);
	~ScopedGpuWriteKind();
	ScopedGpuWriteKind(const ScopedGpuWriteKind&)            = delete;
	ScopedGpuWriteKind& operator=(const ScopedGpuWriteKind&) = delete;

private:
	GpuWriteKind m_previous;
};
void NoteGpuWrite(uint64_t vaddr, uint64_t size);
// Kind name of the last recorded GPU buffer write to the page holding vaddr ("unknown" if none).
[[nodiscard]] const char* LastGpuWriteKind(uint64_t vaddr);

// Texture-cache image deletion reasons (set around FreeImage calls) and native image churn.
enum class ImageFreeReason : uint8_t {
	Other,
	DepthAssociation,
	DepthRecreate,
	OverlapLayout,
	OverlapMipMerge,
	OverlapStale,
	Expand,
	SmallerResources,
	Unmap,
	GarbageCollect,
	PressureCollect,
	Count
};
void SetImageFreeReason(ImageFreeReason reason);
void RecordImageFree(uint64_t address, uint32_t width, uint32_t height, uint32_t levels,
                     uint32_t layers, uint32_t format, uint64_t bytes, uint64_t age_ticks);
void RecordNativeImage(bool create, bool pool_hit, uint32_t format, uint32_t width, uint32_t height,
                       uint32_t levels, uint32_t usage, uint64_t bytes);

// Texture-cache / buffer-cache transfer attribution (transfers.csv), aggregated per second by
// kind, reason, detail, guest address and image shape. reason/detail must be string literals.
//   ImageUpload  guest memory -> native image refresh (reason: first-use, first-use-gpu-buffer,
//                cpu-write, cpu-edge-hash, gpu-buffer-write; detail: upload binding, or for
//                GPU buffer writes the kind of the last recorded write, see NoteGpuWrite)
//   BufferUpload CPU-dirty pages -> device buffer (reason: caller; detail: empty; address: first
//                uploaded byte; width: number of dirty page runs copied; format/height: 0)
//   ImageCopy    texture-cache reinterpretation copy (reason: path; detail: caller)
//   AliasSync    alias synchronization decision (reason: copy or skip)
// bytes: bytes moved. span_bytes: dirty span inside the image that forced the refresh (image
// uploads) or the requested range (buffer uploads).
enum class TransferKind : uint8_t { ImageUpload, BufferUpload, ImageCopy, AliasSync, Count };
void RecordTransfer(TransferKind kind, const char* reason, const char* detail, uint64_t address,
                    uint32_t format, uint32_t width, uint32_t height, uint64_t bytes,
                    uint64_t span_bytes);

// Guest GPU scheduler.
void RecordQueueWait(uint32_t queue, uint64_t wait_ns);
void RecordQueueBusy(uint32_t queue, uint64_t busy_ns, bool complete);
void RecordDoneWait(uint64_t wait_ns);
void RecordFlip();

// GPU execution timing for one guest flip (see graphics/host_gpu/renderer/gpuTiming.h).
struct GpuFrame {
	uint64_t busy_ns             = 0;
	uint64_t idle_ns             = 0;
	uint64_t max_gap_ns          = 0;
	uint64_t starved_ns          = 0;
	uint64_t command_buffers     = 0;
	uint64_t latency_samples     = 0;
	uint64_t record_latency_ns   = 0; // summed over latency_samples
	uint64_t dispatch_latency_ns = 0; // summed over latency_samples
	uint64_t dropped             = 0;
};
void RecordGpuFrame(const GpuFrame& frame);

// Guest GPU recording counters for one guest flip (graphics/host_gpu/renderer/gpuOpProfiler.h).
struct GpuOpCounts {
	uint64_t render_passes      = 0;
	uint64_t barriers           = 0;
	uint64_t layout_transitions = 0;
	uint64_t command_buffers    = 0;
	// Barrier batcher (KYTY_BARRIER_BATCH); zero when it is disabled.
	uint64_t barrier_requests      = 0;
	uint64_t barriers_merged       = 0;
	uint64_t barriers_elided       = 0;
	uint64_t barriers_sunk         = 0;
	uint64_t barrier_render_splits = 0;
	// Appended: rendering instances ended (all causes) and post-draw write barriers sunk.
	uint64_t rendering_ends   = 0;
	uint64_t draw_write_sinks = 0;
};
void RecordGpuOpCounts(const GpuOpCounts& counts);

// Shader and pipeline compilation (graphics/host_gpu/renderer/pipeline/pipelineCache.cpp). One
// event per new program permutation (translate = ShaderRecompiler::TranslateProgram, emit =
// CompileProgram: specialization and SPIR-V emission, validate = spirv-val, module =
// vkCreateShaderModule) or new pipeline (pipeline = pipeline and layout creation, including
// vkCreateGraphicsPipelines/vkCreateComputePipelines). total_ns is the wall time of the whole
// compile on the thread that needed it. summary.csv adds each column per second.
enum class CompileKind : uint8_t { Program, GraphicsPipeline, ComputePipeline, Count };
// Graphics pipelines only: whether another pipeline already existed for the same program ids
// (Variant, detail names the differing key fields), only for the same guest shaders with other
// program permutations (Permutation), or for neither (New).
enum class PipelineOrigin : uint8_t { None, New, Permutation, Variant };
struct CompileEvent {
	CompileKind      kind         = CompileKind::Program;
	PipelineOrigin   origin       = PipelineOrigin::None;
	const char*      stage        = "";
	uint64_t         guest_hash   = 0;
	uint64_t         id           = 0; // program id (for pipelines: the first vertex program)
	uint64_t         id2          = 0; // pipelines: pixel program id
	uint64_t         translate_ns = 0;
	uint64_t         emit_ns      = 0;
	uint64_t         validate_ns  = 0;
	uint64_t         module_ns    = 0;
	uint64_t         pipeline_ns  = 0;
	uint64_t         total_ns     = 0;
	uint64_t         spirv_words  = 0;
	std::string_view detail;
	// Programs: copy of a kept translation (reused, translate_ns 0) or of a new one being kept.
	uint64_t         clone_ns = 0;
	bool             reused   = false;
};
void RecordCompile(const CompileEvent& event);
// A draw or dispatch spent stall_ns in the compile paths (new programs and pipelines, including
// lock waits). compile_stall_max_us is the longest single stall of the second.
void RecordCompileStall(uint64_t stall_ns);
// A driver pipeline cache save (periodic or at exit): payload bytes, vkGetPipelineCacheData time
// and file write time. Overlaps are pipelines created while a save was serializing the same
// cache (the driver may serialize them internally); create_ns is that creation's duration.
void RecordPipelineCacheSave(uint64_t bytes, uint64_t serialize_ns, uint64_t write_ns);
void RecordPipelineCacheSaveOverlap(uint64_t create_ns);
// One background spirv-val run (not on a compiling draw's thread).
void RecordShaderValidation(uint64_t validate_ns);

} // namespace HangTrace

#endif /* EMULATOR_INCLUDE_EMULATOR_COMMON_HANGTRACE_H_ */
