#ifndef KYTY_COMMON_PROFILER_H_
#define KYTY_COMMON_PROFILER_H_

#include "common/common.h"

#include <cstdint>
#include <optional>
#include <tracy/Tracy.hpp> // IWYU pragma: export

namespace profiler::colors {

inline constexpr uint32_t RedA100        = 0xff8a80;
inline constexpr uint32_t Blue300        = 0x64b5f6;
inline constexpr uint32_t CyanA700       = 0x00b8d4;
inline constexpr uint32_t Green100       = 0xc8e6c9;
inline constexpr uint32_t Green200       = 0xa5d6a7;
inline constexpr uint32_t Green300       = 0x81c784;
inline constexpr uint32_t Green400       = 0x66bb6a;
inline constexpr uint32_t Amber300       = 0xffd54f;
inline constexpr uint32_t DeepOrangeA200 = 0xff6e40;

} // namespace profiler::colors

namespace Profiler {

class ScopedBlock {
public:
	explicit ScopedBlock(const tracy::SourceLocationData* source_location, bool active = true);
	ScopedBlock(const ScopedBlock&)            = delete;
	ScopedBlock& operator=(const ScopedBlock&) = delete;
	ScopedBlock(ScopedBlock&&)                 = delete;
	ScopedBlock& operator=(ScopedBlock&&)      = delete;
	~ScopedBlock();

	void End();

private:
	std::optional<tracy::ScopedZone> m_zone;
};

void EndBlock();
void SetThreadName(const char* name);
// Opt in before launching with KYTY_PROFILE_DETAILS=1. High-frequency nested
// zones remain disabled for measurements comparable to the normal instrumentation.
[[nodiscard]] bool DetailedEnabled();
// Keep frame markers and aggregate workload counters, without per-call zones.
// Set KYTY_PROFILE_FRAMES_ONLY=1 before launching; it overrides detailed profiling.
[[nodiscard]] bool FramesOnlyEnabled();
// Additional diagnostic counters/timers; requires KYTY_PROFILE_AGGREGATES=1 and frame-only
// mode. Collection separately requires a connected profiler. Keep off for performance runs.
[[nodiscard]] bool AggregateEnabled();

enum class FrameWork : uint32_t {
	DrawIndex,
	DrawAuto,
	DrawIndirectCommands,
	DrawIndirectMultiCommands,
	DispatchDirectCommands,
	DispatchIndirectCommands,
	Count,
};

// DrawIndex/DrawAuto include expanded indirect draws. The command categories
// describe their origins and must not be added to those renderer-call totals.
void CountFrameWork(FrameWork kind);

// The first four categories are mutually exclusive per original scheduler submission,
// not per driver call or merged VkSubmitInfo. Generic also includes forced completion
// and caller-supplied wait/signal semaphores. Remaining categories count separate work.
enum class FrameEvent : uint32_t {
	SubmitBoundaryUnprotected,
	SubmitBoundaryEopOnly,
	SubmitBoundaryEopMixed,
	SubmitBoundaryGeneric,
	MeshDraws,
	MeshRestartEnabledDraws,
	MeshInputIndices,
	MeshWorkgroups,
	ShaderProgramsCreated,
	GraphicsPipelinesCreated,
	SrtProbeHits,
	SrtProbeMisses,
	SrtProbeBytes,
	SrtProbeBatchHits,
	ResourceReuseHits,
	ResourceReuseMisses,
	ResourceReuseValidationBytes,
	ResourceReuseRejectedCaptures,
	SrtRecipeSessions,
	SrtRecipeCompiledNodes,
	SrtRecipeFallbackNodes,
	SrtRecipeMemoHits,
	SrtTapeExecutions,
	SrtTapeOperations,
	SrtTapeBoundaryCalls,
	BdaSyncPasses,
	BdaSyncSkips,
	BdaSyncScannedBuffers,
	BdaSyncUploadBytes,
	BdaSyncUploadCopies,
	ShaderHeaderProbeHits,
	ShaderHeaderProbeMisses,
	VertexMetadataProbeHits,
	VertexMetadataProbeMisses,
	NativeImagePoolHits,
	NativeImagePoolMisses,
	NativeImagePoolRetires,
	NativeImagePoolAddedBytes,
	DccKnownFillClears,
	NativeImagePoolRemovedBytes,
	TextureCleanProofHits,
	TextureCleanProofMisses,
	TextureCleanProofStores,
	ResourceCacheKeyMatches,
	ResourceCacheBackingRejects,
	ResourceCacheHistoryHits,
	ResourceCacheBudgetRejects,
	ResourceCachePermutationHits,
	SrtSharedCleanMemoHits,
	UploadReservationsOutsideLocks,
	ShaderUploadReuseHits,
	ShaderUploadReuseMisses,
	ShaderUploadBytesAvoided,
	TextureDescriptionHits,
	TextureDescriptionMisses,
	PipelineBindsAvoided,
	DescriptorPushesAvoided,
	PredicatedPackets,
	PredicatedPacketsSkipped,
	OcclusionPredicates,
	OcclusionPredicatesPending,
	OcclusionCounterDumps,
	NativeOcclusionScopes,
	NativeOcclusionDumps,
	NativeOcclusionReductions,
	MeshRestartMarkers,
	MeshRestartSegments,
	// Indirect draws whose GPU-written arguments were consumed by vkCmdDraw*Indirect*, and those
	// that fell back to reading (and so synchronizing) the arguments on the CPU.
	DrawIndirectNative,
	DrawIndirectFallback,
	// Later draws that inherited a native indirect draw's instance count and read it back.
	DrawIndirectInstanceReads,
	// KYTY_GPU_TIMING: command buffers without a usable timestamp pair (ring full, results
	// unavailable, ambiguous wrap, or pending samples over capacity). Busy/idle exclude them.
	GpuTimingDropped,
	// TryReadGpuCleanBacking calls answered from (hits) or not fully from (misses) the
	// per-page clean verdict cache; stores count 4 KiB pages newly proven clean.
	CleanVerdictHits,
	CleanVerdictMisses,
	CleanVerdictStores,
	// Direct-backing reads translated by the per-thread mapping cache without m_mutex.
	BackingMapCacheHits,
	BackingMapCacheMisses,
	// Guest-fault readbacks (BufferCache::ReadMemory, KYTY_READBACK_SIDE_COPY). SideCopies are
	// copies recorded on the side command buffer without draining the current recording; bytes
	// are the dirty bytes copied. DuplicateWaits are faults served by another thread's pending
	// side copy. Fallbacks took the drain path: the current recording wrote a dirty byte of the
	// faulting page, it holds an unbounded (address) writer, or another reason (no buffer, no
	// dirty bytes, an overlapping non-side publication, no free slot). PagesUnmarked counts
	// pages unprotected at completion; PagesRetained counts pages a newer writer re-dirtied.
	ReadbackSideCopies,
	ReadbackSideCopyBytes,
	ReadbackSideDuplicateWaits,
	ReadbackSideFallbackCurrentWriter,
	ReadbackSideFallbackUnbounded,
	ReadbackSideFallbackOther,
	ReadbackSidePagesUnmarked,
	ReadbackSidePagesRetained,
	// Guest GPU recording counters (KYTY_GPU_OP_COUNTERS, gpuOpProfiler.h), added per guest
	// flip: dynamic-rendering begins, pipeline barrier calls, image barriers that change layout
	// and guest command buffers begun. Per-site barrier plots are GpuOps.Barriers.<site>.
	GpuRenderPassBegins,
	GpuPipelineBarriers,
	GpuImageLayoutTransitions,
	GpuGuestCommandBuffers,
	// Barrier batcher (KYTY_BARRIER_BATCH, render.h), added per guest flip with the counters
	// above: barrier requests, requests merged into an already pending batch, requests elided
	// as covered by the previous barrier, pending batches kept across a same-instance draw
	// (sunk), and barrier flushes that had to end an active rendering instance.
	GpuBarrierRequests,
	GpuBarriersMerged,
	GpuBarriersElided,
	GpuBarriersSunk,
	GpuBarrierRenderSplits,
	// Texture-cache reinterpretation copies (TextureCache::CopyImage) by path: same-size color
	// vkCmdCopyImage, VK_KHR_maintenance8 depth<->color vkCmdCopyImage, one-pass shader
	// reinterpretation (depth->color compute, color->depth draw), the image->buffer->image
	// fallback and the D16 download/convert/upload path. AliasSync*: SyncAliasFromOwner copies
	// and copies skipped because both aliases already hold identical native contents.
	ImageCopyDirect,
	ImageCopyMaintenance8,
	ImageCopyShaderDepthToColor,
	ImageCopyShaderColorToDepth,
	ImageCopyViaBuffer,
	ImageCopyD16,
	AliasSyncCopies,
	AliasSyncSkips,
	// Guest memory -> native image refreshes (TextureCache::InitializeImage) and bytes, and
	// CPU-dirty bytes copied into device buffers (BufferCache::SynchronizeBuffer).
	ImageUploads,
	ImageUploadBytes,
	BufferUploadBytes,
	// Draw-prep S3 (KYTY_STAGE_PREP_PARALLEL): draws whose PS and VS were materialized in
	// parallel (PS on the DrawPrep helper), forks declined because the helper slept (it is woken
	// for the next draw), and parallel attempts that fell back to the serial path because a
	// speculative (probe-only) materialization failed, a source/permutation was missing, or the
	// KYTY_STAGE_PREP_VERIFY oracle disagreed.
	StagePrepParallelDraws,
	StagePrepForkDeclined,
	StagePrepSpeculativeFailures,
	StagePrepLookupFallbacks,
	StagePrepVerifyMismatches,
	// Per-thread last-lookup memos (KYTY_PROGRAM_LOOKUP_MEMO): program source entry per stage,
	// and the permutation last matched for that source.
	ProgramSourceMemoHits,
	ProgramSourceMemoMisses,
	PermutationMemoHits,
	PermutationMemoMisses,
	// GetGraphicsPipeline last-key memo (KYTY_PIPELINE_MEMO).
	PipelineMemoHits,
	PipelineMemoMisses,
	// RenderExecutor sampler memo keyed on the final sampler dwords (KYTY_SAMPLER_MEMO).
	SamplerMemoHits,
	SamplerMemoMisses,
	// Color/depth target descriptions memoized on the raw target registers
	// (KYTY_TARGET_DESC_MEMO).
	TargetDescMemoHits,
	TargetDescMemoMisses,
	// Graphics dynamic-state commands recorded or elided by the per-command-buffer shadow
	// (KYTY_DYNAMIC_STATE_SHADOW).
	DynamicStateCommandsEmitted,
	DynamicStateCommandsAvoided,
	// Vertex attribute/V# table reads served by one batched probe per table, or falling back
	// to per-attribute probes (KYTY_SHADER_METADATA_BATCH).
	VertexTableBatchHits,
	VertexTableBatchMisses,
	// Shader map lookups answered by the per-thread memo (KYTY_SHADER_MAP_MEMO).
	ShaderMapMemoHits,
	ShaderMapMemoMisses,
	// Draw-prep S5/S6 (KYTY_DRAW_PREP=inline|parallel). Submitted: direct draws handed to the
	// engine. Published: entered the preparation window (parallel). Ready: prepared by a worker
	// before the command processor needed it. SelfPrepared: prepared on the command processor
	// (inline mode, or the head slot was still unclaimed). Committed: the prepared programs were
	// used after a valid certificate. Unused: the draw returned before its programs were needed.
	DrawPrepSubmitted,
	DrawPrepPublished,
	DrawPrepReady,
	DrawPrepSelfPrepared,
	DrawPrepCommitted,
	DrawPrepUnused,
	// Serial fallbacks per reason. Ineligible: tessellation, O15 reuse or a draw the engine does
	// not take. Unclean/Backing/Overflow/Inconsistent/Uncertified: preparation reads (readSet.h).
	// NotPublished: a source or permutation was missing. ShaderMap: the shader map changed before
	// commit. CertUnclean/CertChanged: a read range was not clean, or held other bytes, at commit.
	// CoherenceLog: the log check failed (KYTY_DRAW_PREP_CERT=log). Mismatch: the commit's own
	// pixel-activity or export-mapping decision differed from the snapshot's.
	DrawPrepFallbackIneligible,
	DrawPrepFallbackUnclean,
	DrawPrepFallbackBacking,
	DrawPrepFallbackOverflow,
	DrawPrepFallbackInconsistent,
	DrawPrepFallbackUncertified,
	DrawPrepFallbackNotPublished,
	DrawPrepFallbackShaderMap,
	DrawPrepFallbackCertUnclean,
	DrawPrepFallbackCertChanged,
	DrawPrepFallbackCoherenceLog,
	DrawPrepFallbackMismatch,
	// Certificate sizes (coalesced ranges and bytes of committed certificates).
	DrawPrepCertRanges,
	DrawPrepCertBytes,
	// KYTY_DRAW_PREP_VERIFY: prepared draws re-prepared serially, and differences.
	DrawPrepVerifyChecks,
	DrawPrepVerifyMismatches,
	// S0 window measurement: fences (packets that end a preparation window) and the number of
	// direct draws parsed since the previous fence, bucketed. Also counted with
	// KYTY_DRAW_PREP_HISTOGRAM=1 in off mode.
	DrawPrepFences,
	DrawPrepFenceDraws0,
	DrawPrepFenceDraws1,
	DrawPrepFenceDraws2To3,
	DrawPrepFenceDraws4To7,
	DrawPrepFenceDraws8To15,
	DrawPrepFenceDraws16To31,
	DrawPrepFenceDraws32To63,
	DrawPrepFenceDraws64Plus,
	// Parallel mode: drains (window committed because of a fence or a full window), the summed
	// window occupancy observed at each publish (mean = sum / Published), and head slots the
	// command processor had to wait for while a worker was preparing them.
	DrawPrepDrains,
	DrawPrepWindowOccupancy,
	DrawPrepCommitWaits,
	Count,
};
void CountFrameEvent(FrameEvent kind, uint64_t amount = 1);

enum class FrameWait : uint32_t {
	ReadMemory,
	ShaderReadiness,
	DccFallback,
	ResourceMaterialization,
	DriverSubmit,
	ShaderProgramMiss,
	GraphicsPipelineCreate,
	ResourceReuseValidation,
	NativeImageCreate,
	NativeImageDestroy,
	// KYTY_GPU_TIMING (GpuTiming::OnGuestFlip), added once per guest flip. These are GPU
	// timeline spans, not CPU scopes. GpuBusy: calls = timed command buffers, time = union of
	// their [top-of-pipe, all-commands] spans. GpuIdle: calls = gaps between merged spans
	// (including the gap after the previous flip's last span), time = their sum. GpuStarved: the
	// part of those gaps before the next buffer's vkQueueSubmit returned. GpuRecordToStart /
	// GpuDispatchToStart / GpuEndToObserved: calls = calibrated samples, time = summed latency
	// from recording start / native submit to GPU start, and from GPU end to CPU collection.
	// Latencies and GpuStarved need VK_KHR/EXT_calibrated_timestamps.
	GpuBusy,
	GpuIdle,
	GpuStarved,
	GpuRecordToStart,
	GpuDispatchToStart,
	GpuEndToObserved,
	// Guest thread time waiting for a side-copy readback (its own or a duplicate) and publishing
	// it to the backing. Nested inside ReadMemory; the GPU thread does not wait for these.
	ReadbackSideWait,
	// Draw-prep S3: GPU-thread time spent spinning in the join after its own stage finished
	// (StagePrepJoin), and helper-thread job time (StagePrepHelper, not on the GPU thread).
	StagePrepJoin,
	StagePrepHelper,
	// Draw-prep: command-processor time waiting for a worker's head slot (CommitWaitNs),
	// preparation time on any thread, and certificate validation time at commit.
	DrawPrepCommitWait,
	DrawPrepPrepare,
	DrawPrepValidate,
	Count,
};

// Aggregate inclusive CPU work/wait durations, not GPU time or an additive frame budget.
// ReadMemory may nest within DCC fallback, shader readiness or resource materialization,
// and may itself be reentrant. DriverSubmit can overlap renderer work on another thread.
// ShaderProgramMiss includes translation and may nest resource materialization; it counts
// miss attempts/retries. ShaderProgramsCreated separately counts compiled permutations.
// GraphicsPipelineCreate excludes cache hits and includes native layout/pipeline setup.
// Completed scopes contribute only while connected with aggregate diagnostics enabled. Scopes
// spanning an on-demand connection change are omitted; no per-call zones are emitted.
class ScopedFrameWait {
public:
	explicit ScopedFrameWait(FrameWait kind);
	ScopedFrameWait(const ScopedFrameWait&) = delete;
	ScopedFrameWait& operator=(const ScopedFrameWait&) = delete;
	ScopedFrameWait(ScopedFrameWait&&) = delete;
	ScopedFrameWait& operator=(ScopedFrameWait&&) = delete;
	~ScopedFrameWait();

private:
	FrameWait m_kind;
	uint64_t m_start_ns = 0;
	uint64_t m_connection = 0;
	bool m_active = false;
};

// Adds externally measured totals (the GPU timeline entries above) with the same gating as
// ScopedFrameWait: aggregate diagnostics enabled and a connected profiler.
void AddFrameWait(FrameWait kind, uint64_t calls, uint64_t nanoseconds);

// Call immediately after the existing completed guest-flip marker. Snapshots
// include workload and wait totals, and are cumulative so an on-demand connection
// can discard its first sample. Independently atomic values can straddle a flip.
void PublishFrameWork();

// Opt-in loading diagnostics collected even before a Tracy client connects. A separate
// host thread publishes cumulative totals once a second, independently of guest flips.
[[nodiscard]] bool LoadingEnabled();
enum class LoadingEvent : uint32_t {
	AprRecordsCompleted,
	AprRequestedBytes,
	AprHostReadBytes,
	AprGuestCopiedBytes,
	AprErrors,
	AprShortReads,
	AmmErrors,
	PreadRequestedBytes,
	PreadReadBytes,
	PreadErrors,
	LodStatsPackets,
	LodStatsBufferBytes,
	LodStatsReportAndResetPackets,
	LodStatsForceResetPackets,
	LodStatsResetCountSum,
	LodStatsInterval100kSum,
	LodStatsCachePolicySum,
	DirectMemorySizeQueries,
	DirectMemoryAvailableQueries,
	DirectMemoryAvailableSuccesses,
	DirectMemoryAvailableBytesSum,
	PageTableStatsQueries,
	PageTableStatsSuccesses,
	PageTableCpuAvailableSum,
	PageTableGpuAvailableSum,
	PageTableCpuZeroAvailable,
	PageTableGpuZeroAvailable,
	FlexibleAvailableQueries,
	FlexibleAvailableSuccesses,
	FlexibleAvailableBytesSum,
	MemoryPoolStatsQueries,
	MemoryPoolAvailableBytesSum,
	AmmUsageStatsQueries,
	AmmOccupancyThresholdCalls,
	AmmOccupancyThresholdSum,
	AprReadCommandsAppended,
	AprCommandResets,
	AprCommandBufferSets,
	AprCommandBufferClears,
	AprSubmitAndGetResult,
	AprSubmitPlain,
	AprSubmitAndGetId,
	AprWaitCompleted,
	AprWaitUnknownId,
	AprUnsupportedWaitCommands,
	AprUnsupportedCounterCommands,
	AprDiagnosticRowsDropped,
	Count,
};
void CountLoadingEvent(LoadingEvent kind, uint64_t amount = 1);

// Request metadata only: no asset contents or faulting guest reads. The first 512
// submissions and every 257th thereafter are eligible, up to 8192 queued rows per
// process. The independent loading publisher drains the bounded queue to a CSV.
struct LoadingAprSubmission {
	uint64_t sequence = 0, timestamp_ns = 0, api_kind = 0;
	uint64_t command_buffer = 0, argument1 = 0, argument2 = 0;
	uint64_t result_address = 0, out_id_address = 0, submission_id = 0;
	uint64_t resolved_object = 0, generation = 0, append_serial = 0;
	uint64_t submits_in_generation = 0, unchanged_submits = 0;
	uint64_t cached_buffer = 0, cached_size = 0, cached_offset = 0;
	uint64_t header_valid = 0, header_buffer = 0, header_size = 0, header_offset = 0;
	uint64_t header_commands = 0, header_type = 0, header_mismatch = 0;
	uint64_t read_count = 0, event_count = 0, write_count = 0, map_count = 0;
	uint64_t commands_signature = 0, first_read_id = 0, first_read_destination = 0;
	uint64_t first_read_size = 0, first_read_offset = 0, read_bytes = 0;
	uint64_t execution_observed = 0, execution_result = 0, error_offset = 0;
	uint64_t submit_result = 0, api_kernel_result = 0;
};
[[nodiscard]] bool BeginLoadingAprSubmission(LoadingAprSubmission* record);
void RecordLoadingAprSubmission(const LoadingAprSubmission& record);

enum class LoadingOperation : uint32_t {
	AprSubmit,
	AprRead,
	AprHostRead,
	AprGuestCopy,
	AmmMap,
	AmmUnmap,
	Pread,
	PreadFileMutex,
	PreadCoherence,
	PreadNativeRead,
	Count,
};

// Completed means the scope returned, including errors. Durations are inclusive,
// may overlap across threads/nested scopes, and become visible only on completion.
class ScopedLoadingOperation {
public:
	explicit ScopedLoadingOperation(LoadingOperation kind);
	ScopedLoadingOperation(const ScopedLoadingOperation&) = delete;
	ScopedLoadingOperation& operator=(const ScopedLoadingOperation&) = delete;
	ScopedLoadingOperation(ScopedLoadingOperation&&) = delete;
	ScopedLoadingOperation& operator=(ScopedLoadingOperation&&) = delete;
	~ScopedLoadingOperation();
	void End();

private:
	LoadingOperation m_kind;
	uint64_t m_start_ns = 0;
	bool m_active = false;
};

void Initialize();
void Shutdown();

struct Lifecycle {
	static constexpr const char* name               = "Profiler";
	static constexpr auto        initialize         = Profiler::Initialize;
	static constexpr auto        shutdown           = Profiler::Shutdown;
	static constexpr auto        emergency_shutdown = Profiler::Shutdown;
};

} // namespace Profiler

#define KYTY_PROFILER_CONCAT_IMPL(a, b) a##b
#define KYTY_PROFILER_CONCAT(a, b)      KYTY_PROFILER_CONCAT_IMPL(a, b)
#define KYTY_PROFILER_COLOR_OR_DEFAULT(default_color, ...)                                         \
	KYTY_PROFILER_COLOR_OR_DEFAULT_IMPL(default_color __VA_OPT__(, ) __VA_ARGS__, default_color)
#define KYTY_PROFILER_COLOR_OR_DEFAULT_IMPL(default_color, color, ...) color

#define KYTY_PROFILER_BLOCK(name, ...)                                                             \
	KYTY_PROFILER_BLOCK_IMPL(__LINE__, true, name __VA_OPT__(, ) __VA_ARGS__)
#define KYTY_PROFILER_DETAIL_BLOCK(name, ...)                                                      \
	KYTY_PROFILER_BLOCK_IMPL(__LINE__, Profiler::DetailedEnabled(), name __VA_OPT__(, ) __VA_ARGS__)
#define KYTY_PROFILER_BLOCK_IMPL(line, active, name, ...)                                          \
	static constexpr tracy::SourceLocationData KYTY_PROFILER_CONCAT(                               \
	    kyty_profiler_source_location_, line) {name, TracyFunction, TracyFile,                     \
	                                           static_cast<uint32_t>(line),                        \
	                                           KYTY_PROFILER_COLOR_OR_DEFAULT(0, __VA_ARGS__)};    \
	Profiler::ScopedBlock KYTY_PROFILER_CONCAT(kyty_profiler_block_, line)(                        \
	    &KYTY_PROFILER_CONCAT(kyty_profiler_source_location_, line), active)

#define KYTY_PROFILER_FUNCTION(...)                                                               \
	KYTY_PROFILER_FUNCTION_IMPL(__LINE__, true __VA_OPT__(, ) __VA_ARGS__)
#define KYTY_PROFILER_DETAIL_FUNCTION(...)                                                        \
	KYTY_PROFILER_FUNCTION_IMPL(__LINE__, Profiler::DetailedEnabled() __VA_OPT__(, ) __VA_ARGS__)
#define KYTY_PROFILER_FUNCTION_IMPL(line, active, ...)                                             \
	static constexpr tracy::SourceLocationData KYTY_PROFILER_CONCAT(                               \
	    kyty_profiler_source_location_, line) {nullptr, TracyFunction, TracyFile,                  \
	                                           static_cast<uint32_t>(line),                        \
	                                           KYTY_PROFILER_COLOR_OR_DEFAULT(0, __VA_ARGS__)};    \
	Profiler::ScopedBlock KYTY_PROFILER_CONCAT(kyty_profiler_block_, line)(                        \
	    &KYTY_PROFILER_CONCAT(kyty_profiler_source_location_, line), active)

#define KYTY_PROFILER_END_BLOCK Profiler::EndBlock()

#define KYTY_PROFILER_THREAD(name) Profiler::SetThreadName(name)

#endif /* KYTY_COMMON_PROFILER_H_ */
