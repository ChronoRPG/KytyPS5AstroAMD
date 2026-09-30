# Complete fork change catalog

This catalog lists every commit beyond the shared upstream base for U59, then the
RT and Demon's Souls branch additions. The description identifies the change; the
mechanism explains the relevant cost/correctness boundary. It is not a set of
independent benchmark claims. See [CHANGES-U59.md](CHANGES-U59.md) for measured
results, defaults, caveats and the decomposition of the early combined checkpoint.

Published U59 source: [`919e712a`](https://github.com/Jetsku/KytyPS5-experimental/commit/919e712a0c109d3de167ebbbbeb58c3a8e4986c1).
Published shared base: [`de0b7fee`](https://github.com/Jetsku/KytyPS5-experimental/commit/de0b7fee4f63254f5c606f472fb581316ae52e5a).

## U59 complete history

315 commits (including integration merges).

### 1. Preserve Astro Bot renderer experiments through S19

Commit: [`6a7f10ee`](https://github.com/Jetsku/KytyPS5-experimental/commit/6a7f10eea46c666ec1f49b93a43a5f8f9ff69476) · **Early architecture checkpoint**

Bundles A–S19: indexed lookup, queued/coalesced submissions, dirty queries, SRT runs/recipes/tapes, incremental BDA sync, native image pooling, clean proofs, renderer batching, GPU occlusion/reduction, mesh restart and capture lifecycle fixes. Sections 1–6 of CHANGES-U59.md explain each constituent and its evidence; the checkpoint is not one isolated optimization.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Checkpoint accumulated profiling and renderer work: queued submission,
> resource evaluation and ownership optimizations, DCC handling, native image
> reuse, RenderDoc lifecycle fixes, GPU occlusion counters, mesh restart,
> and batched query reduction.
>
> Includes existing test changes from earlier iterations. S19 emulator-only
> build succeeded; gameplay correctness and performance remain experimental.
> Sky Garden loading stalls are unresolved. No 60 FPS result is claimed.

</details>

Changed files: `CMakeLists.txt`, `src/common/lruCache.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/common/rendererBatch.h`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/guest_gpu/hardwareContext.h`, `src/graphics/host_gpu/graphicContext.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/queueSubmission.cpp`, `src/graphics/host_gpu/queueSubmission.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/faultManager.cpp`, `src/graphics/host_gpu/renderer/cache/imageCacheGcPolicy.h`, `src/graphics/host_gpu/renderer/cache/streamBuffer.cpp`, `src/graphics/host_gpu/renderer/cache/streamBuffer.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.h`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/image/blitHelper.cpp`, `src/graphics/host_gpu/renderer/image/dccClear.cpp`, `src/graphics/host_gpu/renderer/image/dccClear.h`, `src/graphics/host_gpu/renderer/image/tiler.cpp`, `src/graphics/host_gpu/renderer/masterSemaphore.cpp`, `src/graphics/host_gpu/renderer/masterSemaphore.h`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/occlusion.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/host_gpu/renderer/renderContext.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/host_gpu/renderer/sync.cpp`, `src/graphics/host_gpu/shaders/gpu_dcc_clear_rgba16f.comp`, `src/graphics/host_gpu/shaders/gpu_dcc_occlusion.comp`, `src/graphics/host_gpu/shaders/gpu_dcc_validate.comp`, `src/graphics/host_gpu/vma.cpp`, `src/graphics/presentation/renderDoc.cpp`, `src/graphics/presentation/systemOverlay.cpp`, `src/graphics/presentation/videoOut.cpp`, `src/graphics/presentation/window/swapchain.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`, `src/graphics/shader/recompiler/ir/Program.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.cpp`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.cpp`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.h`, `src/graphics/shader/shader.cpp`, `src/kernel/fileSystem.cpp`, `src/kernel/memory.cpp`, `src/kernel/memory.h`, `src/kernel/memoryAddressSpace.inc`, `src/launcher/src/mainDialog.cpp`, `src/libs/libAmpr.cpp`, `tests/ImageCacheGcPolicyTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 2. Document Claude Code handoff, measurements, build and tracing workflow

Commit: [`82c86ff4`](https://github.com/Jetsku/KytyPS5-experimental/commit/82c86ff44d127a70960f692918f088bc2178d915) · **Workflow and compatibility controls**

Supports controlled launches, navigation or documentation. This does not optimize rendering; private operational documents are excluded from the publication.

No retained file delta (for example, a private-document-only change removed during history sanitation).

### 3. Add T20 hang flight recorder (KYTY_HANG_TRACE) and LOD-stats A/B switch

Commit: [`381526cd`](https://github.com/Jetsku/KytyPS5-experimental/commit/381526cddd7b5bbf1f672162ab2953550edc0500) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Always-on per-second CSV recorder for the intermittent Sky Garden loading
> hang: flips, APR reads with host path/size/repeat and guest callers, per-
> second HLE import call counts, GET_LOD_STATS buffer contents before Kyty
> overwrites them, T# mip-stat counter usage, and guest GPU queue latency.
>
> KYTY_LOD_STATS_MODE=legacy|zero|untouched|ones selects how GET_LOD_STATS
> reports are filled (legacy = previous behaviour). Diagnostic only.
>
> Built on Windows as t20-hang-trace-20260926-211603 (EXE EC77C99F...).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/guest_gpu/graphicsRun.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/presentation/videoOut.cpp`, `src/libs/libAmpr.cpp`, `src/loader/runtimeLinker.cpp`.

### 4. Add U21 readback, image-churn and LOD-report-consumer attribution to hang trace

Commit: [`66e26c71`](https://github.com/Jetsku/KytyPS5-experimental/commit/66e26c71164757ba0662693c29c9487ccab16e67) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> readbacks.csv: every BufferCache::ReadMemory with its cause (guest fault
> read/write, GPU-thread sync for resource readiness, invalidate), range,
> download window, duration, guest thread and faulting pc.
> images.csv: texture-cache image deletions tagged with the FreeImage call
> site reason, plus per-second native image create/destroy by format/extent/
> usage.
> lodwatch.csv: briefly guards plain read/write GET_LOD_STATS report pages
> (sampled, capped) to log the first guest access with registers and
> surrounding code bytes, restoring the original protection immediately.
>
> Diagnostic only (KYTY_HANG_TRACE=1); rendering behaviour unchanged.
> Built as u21-readback-attribution-20260926-213835 (EXE 3439A455...).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/host_gpu/vma.cpp`, `src/loader/runtimeLinker.cpp`.

### 5. Report GET_LOD_STATS as not-ready by default; sample hang-trace stack scans

Commit: [`e58dd966`](https://github.com/Jetsku/KytyPS5-experimental/commit/e58dd966a5d611b36b681a213aeeb8e18792f974) · **Historical LOD diagnostic**

Reports LOD feedback not-ready and samples stack scans. This was an investigation step superseded by GPU feedback/publication work; it is not evidence of a valid permanent performance shortcut.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot's texture streamer (eboot+0x7022f24) only parses a mip-statistics
> report whose first dword is non-zero. The old placeholder wrote zero counters
> with 1 in that dword, which declared every texture unused and drove a
> continuous evict/re-stream loop (loaded Sky Garden: 17 reads/s re-streaming
> ~30 textures; zero mode: none). Default to a zeroed report; the previous
> behaviour stays available via KYTY_LOD_STATS_MODE=legacy.
>
> Readback rows now scan guest stacks only for the first faults per pc.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`.

### 6. shader: remove per-draw vertex diagnostic atomics

Commit: [`138341ec`](https://github.com/Jetsku/KytyPS5-experimental/commit/138341ec25bf52ef1b565041a60b866dd7689d40) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> (cherry picked from commit cf423df62435c733211a235d6280f83399f1175f)

</details>

Changed files: `src/graphics/shader/shader.cpp`.

### 7. graphics: name Vulkan images and views only on creation

Commit: [`8faa199e`](https://github.com/Jetsku/KytyPS5-experimental/commit/8faa199e682268c455db2fb93df857e34fe6eafc) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> (cherry picked from commit 0ec43a8d3dfc523f66ebcf433c4e402eccf7bf3e)

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/imageView.cpp`, `src/graphics/host_gpu/renderer/renderDraw.cpp`.

### 8. graphics: share EOP clock and immediate write handling

Commit: [`6f26dbfa`](https://github.com/Jetsku/KytyPS5-experimental/commit/6f26dbfa944f243ad18002900e1edcca4549da55) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> (cherry picked from commit bfdbba3aa43f9851bb19fab0eee775672b086fa7)

</details>

Changed files: `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/sync.cpp`, `src/graphics/host_gpu/renderer/sync.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 9. shader: stop repeated CFG merge gateway splitting

Commit: [`c0bb47c5`](https://github.com/Jetsku/KytyPS5-experimental/commit/c0bb47c5f205086f9f5e1033ddf7ed701def0ccc) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> (cherry picked from commit 6bb68a3fd97c0ac913d9c22d6c1279549597dd56)

</details>

Changed files: `src/graphics/shader/recompiler/frontend/cfg/ShaderCFG.cpp`, `tests/shaderCfgTests.cpp`.

### 10. U22: skip drains for never-GPU-mapped unmaps; age image aliases by frames

Commit: [`e1b75ca9`](https://github.com/Jetsku/KytyPS5-experimental/commit/e1b75ca976b3f3b1f1d1dcd9bf62be024f2e57e3) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RenderContext::UnmapMemory returns early when the range does not intersect
> any GPU-mapped range. The kernel unmaps every free range before mapping it,
> and each call forced a GPU-thread service command with a full drain.
>
> Texture-cache overlap staleness now also requires AliasFramesBeforeRemoval
> presented guest frames since last access (TextureCache::AdvanceFrame is
> called per completed flip). The tick-only rule expired within one frame,
> so ~15 aliased transient render targets were freed and re-initialised from
> guest memory every frame (~150 MiB memcpy/detile per frame in Sky Garden).
> Live aliases follow a single-owner rule: a GPU write clears GPU-modified on
> every other overlapping alias so none is downloaded over newer bytes.
> KYTY_IMAGE_ALIAS_AGE=ticks restores the previous behaviour.
>
> Hang trace: readback rows report which kind of recorded GPU write last
> marked the page GPU-owned (shader storage, occlusion dump, fill, copy).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/image/image.h`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/presentation/videoOut.cpp`.

### 11. U23: publish occlusion dumps from the host at completion; keep pipeline cache across builds

Commit: [`f1d673f3`](https://github.com/Jetsku/KytyPS5-experimental/commit/f1d673f39d40e5b3f7315cf9993568aceff68062) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Occlusion dumps are reduced into a private host-visible slot and copied into
> guest memory by a completion callback, so dump pages are never GPU-owned.
> Astro Bot keeps EOP labels on the same pages; GPU-owned dump pages made each
> CPU label write fault, download 512 KiB and drain the GPU (~5 per frame,
> ~19 ms/frame in Sky Garden). CPU predication reads wait for pending dumps.
>
> The Vulkan pipeline cache no longer depends on the exact git revision and is
> no longer disabled for uncommitted builds: the driver validates its header
> and keys entries by exact shader code and state, so another revision can only
> miss. Every experimental build previously recompiled all pipelines at boot.

</details>

Changed files: `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/occlusion.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`.

### 12. Hang trace: sample APR call-chain scans per call site

Commit: [`9247af76`](https://github.com/Jetsku/KytyPS5-experimental/commit/9247af7693a80d86f2dd7905120de4881fd14c9b) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Scanning the guest stack for every APR read costs hundreds of VirtualQuery
> calls. Boot issues ~6,000 reads from a few call sites; with the recorder on,
> the guest streamer stalled and the first level loaded after ~25 s instead of
> ~4 s (loading-progress logs: L12/P16/R18/S19 vs T20/U23). Scan the first four
> reads of each call site and every 1024th afterwards.

</details>

Changed files: `src/common/hangTrace.cpp`.

### 13. U25: GPU mip statistics for GET_LOD_STATS

Commit: [`21450ca3`](https://github.com/Jetsku/KytyPS5-experimental/commit/21450ca392d815a0db4dbbe6283fb23b347ead61) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Pixel shaders that sample images record, per T# mip-statistics counter
> (MipStatsCntEn/MipStatsCntId, delivered per draw in the shader data, 16 bits
> per image), the finest mip level sampled (floor of the unclamped query LOD,
> or the explicit LOD) and a sample count: one subgroup-reduced atomic pair per
> sample into a 257-entry device buffer (new MipStats binding).
>
> GET_LOD_STATS copies and resets the counters in command order and publishes
> the report from the host at GPU completion in the layout Astro Bot's streamer
> parses (64-byte header with dword0 = valid; entries: count in bits 0..23,
> finest mip in bits 56..59, 0xF when unsampled). Visible textures now request
> the detail they are sampled at instead of decaying (zero mode) or all
> demanding mip 0 (legacy mode). KYTY_LOD_STATS_MODE=gpu is the default.

</details>

Changed files: `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/lodStats.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/host_gpu/renderer/renderContext.h`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterImage.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterModule.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/passes/BindingLayout.cpp`, `src/graphics/shader/recompiler/ir/passes/BindingLayout.h`.

### 14. U26: write LOD reports at record time from the newest completed statistics

Commit: [`91eab891`](https://github.com/Jetsku/KytyPS5-experimental/commit/91eab891657bf60753a04d849e9c199336743236) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Kyty writes EOP labels when a command is recorded, so Astro Bot may read a
> GET_LOD_STATS report before the GPU executes it. Its parser treats an unready
> report as 'no data' for every texture (decay to low detail) and does not fall
> back to older reports. U25 published reports at GPU completion, which arrived
> late whenever the command processor lagged (Sky Garden at ~7 fps): textures
> cycled between the 128 KiB prefix and full resolution (~650 files).
>
> Each report is now written when recorded, carrying the newest statistics packed
> at GPU completion, combining the two newest intervals because the game resets
> the counters twice per frame.

</details>

Changed files: `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/lodStats.h`.

### 15. Key the texture-description cache on the fields it depends on

Commit: [`2e9471b9`](https://github.com/Jetsku/KytyPS5-experimental/commit/2e9471b91507fb37e370dab29da3073da6441b75) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The cache compared the whole ImageResource, including the shader-specific
> source slot, first-use pc and indirect-image tables (a std::vector), so one
> texture bound from different shaders always missed (~25k misses vs ~22k hits
> per frame in Sky Garden). Key on the 13 fields BuildTextureDescription and its
> helpers read, mix them into the hash, and grow the table from 256 to 4096
> entries (~47k texture resolutions per frame).

</details>

Changed files: `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/render.h`.

### 16. Skip DCC metadata readbacks for known uniform fills

Commit: [`8f9296e0`](https://github.com/Jetsku/KytyPS5-experimental/commit/8f9296e003824565a9c0967d0624c1d53ad49921) · **Compressed metadata and clear semantics**

Handles the named metadata/clear operation without stale contents; eligible GPU work avoids CPU readbacks. Accuracy and lower transfer cost are separate claims; see the water discussion.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot fast-clears DCC render targets with uniform-fill compute dispatches.
> The dispatch runs natively, so the metadata becomes GPU-owned and the next
> render-target bind read it back (full drain) just to learn the clear code
> (~1 per frame in Sky Garden, ~17 ms/frame DccFallback in total).
>
> BufferCache now remembers ranges written by proven uniform-fill dispatches and
> by FillBuffer, forgets them (splitting partial overlaps) on any later GPU
> writable binding or CPU invalidation, and MaterializeDccClear uses a known
> byte-uniform fill covering the whole metadata range instead of reading it back.
> Counter: FrameEvent.DccKnownFillClears.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/renderCompute.cpp`.

### 17. Keep depth/color aliases alive; refine mip-statistics LOD

Commit: [`9983732d`](https://github.com/Jetsku/KytyPS5-experimental/commit/9983732d23b320262c557e413ea778aa9a7130ff) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot reuses a 960x540 allocation as D32F depth, R32F and B10G11R11
> color, switching ~90 times per second while sliding in the desert. Each
> switch destroyed the image, created a new one and copied the contents
> (4,080 recreations in 45 s). ResolveDepthOverlap now reuses a registered
> alias of the requested interpretation and keeps the previous one alive;
> FindImage copies the current owner's contents into a non-owner alias
> (SyncAliasFromOwner) before use, so no destroy/create/upload remains.
> KYTY_IMAGE_ALIAS_AGE=ticks restores the old behaviour.
>
> Mip statistics: add the IMAGE_SAMPLE_B bias to the query LOD and report
> absolute mip levels by adding T# BASE_LEVEL (per-draw field: id bits 0..7,
> base level bits 8..11, bit 15 = no counter), per the RDNA2 ISA review.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterImage.cpp`.

### 18. Add opt-in GPU execution timing per guest flip (KYTY_GPU_TIMING)

Commit: [`d5d58173`](https://github.com/Jetsku/KytyPS5-experimental/commit/d5d58173dee83e5425fa3db04986ab3a3dbff1c9) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Each guest-scheduler command buffer gets an in-buffer query reset plus a
> top-of-pipe start and all-commands end timestamp from a fixed 8192-pair
> ring. Pairs are read with vkGetQueryPoolResults (64-bit, with availability,
> never WAIT/PARTIAL) only once KnownGpuTick() covers the owning tick, at the
> existing BeginCommand retirement point in batches of 16. CPU steady_clock
> stamps record recording start, the Submit call, the native vkQueueSubmit
> return (written by the broker before it publishes dispatched_tick) and the
> observed completion. No submissions, waits, barriers or render-pass breaks
> are added; coalescing and callback boundaries are unchanged. The presenter
> scheduler is not timed.
>
> At each completed guest flip the collected spans are merged into GPU busy
> time (union), idle gaps, the part of those gaps before the next buffer was
> dispatched (CPU starvation), and record/dispatch -> GPU start latency via
> VK_KHR/EXT_calibrated_timestamps, enabled only when timing is on.
> Results are appended to hang-trace summary.csv (gpu_busy_us, gpu_cmdbufs,
> gpu_latency_avg_us, gpu_idle_us, gpu_max_gap_us, gpu_starved_us,
> gpu_dispatch_latency_avg_us, gpu_dropped) and to Profiler FrameWait.Gpu*
> and FrameEvent.GpuTimingDropped aggregates.
>
> KYTY_GPU_TIMING=1/0 forces it; unset, it follows KYTY_HANG_TRACE=1 or
> Profiler aggregate diagnostics, so normal play records nothing.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/queueSubmission.cpp`, `src/graphics/host_gpu/queueSubmission.h`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.h`, `src/graphics/host_gpu/renderer/gpuTiming.cpp`, `src/graphics/host_gpu/renderer/gpuTiming.h`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/presentation/videoOut.cpp`, `src/graphics/presentation/window/swapchain.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`.

### 19. Merge GPU execution timing (claude/gpu-timing)

Commit: [`4323ea43`](https://github.com/Jetsku/KytyPS5-experimental/commit/4323ea4312e61609e95f9e60dbf6d905981e560e) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 20. ampr: implement counter bank, counter/time write-address and waits

Commit: [`d22d6114`](https://github.com/Jetsku/KytyPS5-experimental/commit/d22d6114249879f03d94d0bd02339b2e8d0a0a29) · **Guest services and synchronization**

Implements guest-visible state/waits or reduces redundant host wakeups/polling. These changes can improve progress and contention but have no isolated per-commit FPS result.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> WaitOnAddress, WaitOnCounter, WriteCounter(OnCompletion) were recorded as
> no-ops and WriteAddressFromCounter/CounterPair/TimeCounter wrote 0.
>
> - New header-only model (src/libs/amprCounterBank.h): 256 little-endian
>   32-bit counters, access lanes (64-bit pair N/N+1, 32, 16x2, 8x4), write
>   ops (store/or/and-complement/xor/add, truncated to the lane) and wait
>   compares (eq, gt/lt unsigned, ne, wrapped ge, gt/lt signed).
> - libAmpr records these as ordered sync commands executed with the other
>   record kinds. The executor is synchronous, so OnCompletion variants are
>   exact: each record runs after all earlier records have completed.
> - WriteAddressFromCounter writes the counter zero-extended, the Pair form
>   counter N | counter N+1 << 32, the time form KernelGetProcessTimeCounter()
>   sampled when the record executes (was: 0 at record time).
> - Waits re-evaluate on any AMPR counter/address write (cv + epoch) and poll
>   for CPU stores with 50us..2ms backoff. A wait whose producer is the
>   submitting thread itself can never resolve inside a synchronous submit, so
>   each wait is bounded at 1s, logged (rate-limited) and then treated as
>   satisfied instead of hanging or failing the submission.
> - AprUnsupportedWaitCommands / AprUnsupportedCounterCommands now count only
>   encodings outside the decoded tables (kept as 0x20 no-ops as before). The
>   first recording of each implemented kind is logged.
> - ampr_counter_bank_tests covers lane decoding, ops and all compares.

</details>

Changed files: `CMakeLists.txt`, `src/libs/amprCounterBank.h`, `src/libs/libAmpr.cpp`, `tests/AmprCounterBankTests.cpp`.

### 21. Merge AMPR counter bank, counter writes and waits (claude/ampr-counters)

Commit: [`83899697`](https://github.com/Jetsku/KytyPS5-experimental/commit/838996970e0aa0d84713b1c94e363160affb8c81) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 22. shader: seal resource plans and move evaluation scratch per thread (draw-prep S1)

Commit: [`46a6a238`](https://github.com/Jetsku/KytyPS5-experimental/commit/46a6a238addcb2a18e1ac4d66ee93eae9f278ea5) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Make ResourcePlan immutable while it is evaluated, so later steps can
> evaluate one plan from several threads. No behaviour change.
>
> - ExtractResourcePlan now ends with SealEvaluationIndices, which assigns a
>   memo slot to every instruction in value_storage and sets
>   evaluation_sealed. SrtWalker reads sealed slots through the const
>   Inst::SealedEvaluationIndex (EXIT_IF when unassigned); unsealed programs
>   (translation-time passes and tests) keep the lazy assignment.
> - The mutable evaluation scratch (memo contexts and depth, flow visit tags
>   and epoch, active sources, visited/pending blocks, material keys,
>   specialization reads) moves out of ResourcePlan into IR::EvaluationScratch.
>   Memo contexts are generation-stamped and flow visit tags epoch-stamped, so
>   one scratch can serve different plans in turn.
> - New SrtWalker and MaterializeResources overloads take an
>   EvaluationScratch&. The old signatures use a thread_local scratch
>   (ThreadEvaluationScratch), so existing callers compile unchanged.
> - resource_materialization_tests gains a case that evaluates two sealed
>   plans from six threads, alternating plans and user data on each scratch,
>   and compares every result with a serial reference.
> - The standalone scalar_provenance, resource_materialization and
>   resource_tracking test targets now link common. SrtWalker.cpp and
>   ResourceMaterialization.cpp include common/profiler.h (Tracy), and without
>   common these targets no longer built.

</details>

Changed files: `CMakeLists.txt`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/Value.h`, `src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.cpp`, `src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.h`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.cpp`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.h`, `tests/ResourceMaterializationTests.cpp`.

### 23. Merge draw-prep S1: sealed resource plans and per-thread evaluation scratch

Commit: [`bbefc78b`](https://github.com/Jetsku/KytyPS5-experimental/commit/bbefc78b4b77493926ea5169a8b51064bdc86ce5) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 24. Cache clean-page verdicts and backing mappings for guest reads

Commit: [`3e643606`](https://github.com/Jetsku/KytyPS5-experimental/commit/3e6436069662da8fb95df20aeb6ff2f2addd1bba) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> TryReadGpuCleanBacking runs about 36 times per draw and each call took the
> texture-cache spin lock, walked the buffer dirty ranges and pending backing
> publications, and then took the guest mapping mutex for the copy.
>
> - Add a per-thread, generation-tagged table of 4 KiB "clean for a GPU-backing
>   read" verdicts (graphics/host_gpu/cleanVerdictCache.h). Only the ownership
>   verdict is cached; bytes are still copied fresh from backing. A global
>   generation is bumped before every transition that can dirty a page or move
>   backing authority: buffer dirty-range Add (when it grows) and Subtract,
>   Begin/EndBackingPublication, InvalidateContentRevisions, texture register,
>   unregister and GPU-modified transitions (InvalidateCleanImageProofs) and
>   every ClearGpuModified site, tracker GPU mark/unmark/clearing downloads, and
>   RenderContext Map/UnmapMemory.
> - Give GuestBackingStore a map generation, bumped under m_mutex wherever the
>   map hints are invalidated, and let small TryReadBacking calls translate
>   through a thread_local cache of recent mapping records with a seqlock-style
>   recheck. Writes and larger reads keep the locked path.
> - KYTY_CLEAN_VERDICT_CACHE=0 disables both. Add CleanVerdict and
>   BackingMapCache hit/miss frame events.
> - Tests: verdict query/invalidation, tracker bumps and cross-thread
>   publication in memory_tracker_tests; remap/unmap staleness of cached
>   backing translations in virtual_memory_allocation_tests.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/cleanVerdictCache.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/kernel/memory.cpp`, `src/kernel/memoryAddressSpace.inc`, `tests/MemoryTrackerTests.cpp`, `tests/VirtualMemoryAllocationTests.cpp`.

### 25. Merge clean-page verdict cache and lock-free backing map cache (claude/clean-verdict-cache)

Commit: [`059dc140`](https://github.com/Jetsku/KytyPS5-experimental/commit/059dc140a42d7fa6e3111aea4146900cc62cecf7) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> # Conflicts:
> #	src/common/profiler.cpp
> #	src/common/profiler.h

</details>

### 26. renderer: per-draw program preparation output (draw-prep S2)

Commit: [`b9ff86f7`](https://github.com/Jetsku/KytyPS5-experimental/commit/b9ff86f7b548861b0a94d7f26a157c640ab7449f) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Materialization output no longer lives in the shared ProgramCache entry.
> Each stage writes into a caller-owned PipelineCache::StagePrep (resources,
> specialization, permutation) and its stage runtime points there:
> DrawRenderState owns the graphics preps, RenderExecutor::m_compute_prep the
> compute one.
>
> ProgramCache::Get is split into FindSource (shared lookup), a pure
> MaterializeStage that takes explicit EvaluationScratch, a lock-free
> FindPermutation and CompileAndPublish (exclusive). The programs map is
> guarded by its own shared_mutex, so GetGraphicsPrograms/GetComputeProgram no
> longer hold PipelineCache::m_mutex across materialization. Permutations live
> in an append-only PermutationList (16 inline slots published with
> release/acquire, stable addresses, locked overflow). The lookup key and
> validation buffer moved to per-thread ProgramScratch. The O15
> KYTY_RESOURCE_REUSE path keeps its in-entry certificate/history state behind
> a dedicated mutex and copies its result into the StagePrep.
>
> DrawRenderState (36880 bytes) is now a reused RenderExecutor member.
> Reset() clears depth, the color slots and tessellation stages the previous
> draw wrote, instead of value-initialising the whole state per draw.
>
> No behaviour change is intended; KYTY_* defaults are unchanged.

</details>

Changed files: `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 27. Merge draw-prep S2: per-stage preparation results and lock-free program lookup (claude/draw-prep-s2)

Commit: [`f95621a5`](https://github.com/Jetsku/KytyPS5-experimental/commit/f95621a574402c63df3afc506a97186388ddade1) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 28. graphics: native GPU indirect draws with CPU fallbacks

Commit: [`146edeaf`](https://github.com/Jetsku/KytyPS5-experimental/commit/146edeaf02bf2b7cb7b9dad540b3b6c0deaa3753) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DRAW_INDIRECT / DRAW_INDIRECT_MULTI copied the GPU-written argument
> records on the CPU, which page-faulted on GPU-owned pages and drained
> the GPU (BufferCache::ReadMemory) for every such draw.
>
> When the argument (or count) bytes are GPU-owned (exact dirty bytes or
> a pending backing publication), the CP now hands a DrawIndirectSource to
> RenderExecutor::DrawIndirectNative, which records vkCmdDrawIndexedIndirect
> / vkCmdDrawIndirect (or the *Count forms) over the cached guest records.
> The records are byte-identical to the Vulkan commands (static_asserts).
> Argument, count and whole index ranges are acquired before the shader
> bindings so cache merges cannot retire resolved bindings; an indirect
> barrier is recorded outside rendering once per rendering instance.
>
> Fallbacks (the previous CPU-read path): custom primitive-restart values,
> 8-bit indices without VK_KHR/EXT_index_type_uint8, mesh-shader draws,
> kQuadListLegacy, metadata-color / depth-stencil-copy / resolve modes,
> INDEX_BUFFER_SIZE 0, and missing host features. The instance count an
> indirect draw leaves for later draws stays GPU data and is read back only
> if such a draw consumes it.
>
> CP reads of GPU-writable memory (SET_*_REG_INDIRECT pairs, WAIT_REG_MEM,
> COND_EXEC-style branch, SET_PREDICATION op 3, indirect counts, dispatch
> thread dimensions) go through ReadGuestForCp: clean backing, else
> synchronize and retry, else the mapped read.
>
> KYTY_NATIVE_INDIRECT=0 disables the native path; KYTY_INDIRECT_VALIDATE=1
> also reads the arguments on the CPU and logs divergences. New frame
> events: DrawIndirectNative, DrawIndirectFallback, DrawIndirectInstanceReads.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/graphicContext.h`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`.

### 29. Merge native GPU indirect draws with CPU fallbacks (claude/native-indirect)

Commit: [`34deb660`](https://github.com/Jetsku/KytyPS5-experimental/commit/34deb660519a5d3760f51026de45256907ed45f1) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 30. Add KYTY_MIP_STATS_BASE_LEVEL=0 to report mip statistics relative to BASE_LEVEL

Commit: [`6364a83c`](https://github.com/Jetsku/KytyPS5-experimental/commit/6364a83c5b5228288243a8b177121b60b7d386f3) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

Changed files: `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`.

### 31. readback: writer-tick side copy for guest read faults, waited on by the guest

Commit: [`982b3200`](https://github.com/Jetsku/KytyPS5-experimental/commit/982b3200123c8410cf4ff35bce937e61350ba1b9) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Guest read faults on GPU-owned pages used to run BufferCache::ReadMemory on the
> GPU/command-processor thread: download a 512 KiB window into the current
> recording, submit it and wait for the whole recording plus priority callbacks.
> This blocked the CP for every polled value on the edge of a large writable
> binding (READBACK-DESIGN-20260926 item 3).
>
> Design
> - Writer ticks: every GPU write of cached buffer contents records the tick of
>   the recording that wrote it in a WriteTickMap (ObtainBuffer(written), which
>   covers shader storage, FillBuffer and CopyBuffer destinations and metadata;
>   buffer joins in CreateBuffer; image->buffer copies in
>   SynchronizeBufferFromImage). Entries of completed ticks are pruned. Shaders
>   with unbounded address writes (InvalidateContentRevisions) mark the whole
>   current recording as an unbounded writer.
> - Guest (non-GPU-thread) read fault: the GPU thread (still via SendCommandSync,
>   so all cache/tracker state stays GPU-thread serialized) picks an aligned
>   window (default 64 KiB, KYTY_READBACK_SIDE_COPY_WINDOW_KB), or just the
>   faulting pages if the window is not eligible. If every dirty byte in it has
>   a writer tick older than the current recording, it records a copy of only
>   those exact dirty bytes on a dedicated command pool/buffer (16 slots, one
>   shared 1 MiB host-visible staging buffer), and submits it under queue_mutex
>   (after draining the submission broker) with a timeline wait on the
>   producer's master tick and a signal on a dedicated readback timeline
>   semaphore. SendCommandSync returns right after the submit; the current
>   recording is not submitted or waited for.
> - The faulting guest thread waits on the readback semaphore itself, writes
>   the bytes with WriteBacking, ends the backing publication, and unprotects
>   the pages that are still clean.
> - Duplicate faults (any thread) on a page with a pending side copy wait for or
>   finish that same publication instead of copying again (the retried access
>   faults again if the page was re-dirtied meanwhile).
> - Fallback to the old submit+drain path when a dirty byte was written by the
>   current recording, when the current recording has an unbounded writer, or
>   when nothing is eligible (no registered buffer, no dirty bytes, an
>   overlapping queued drain/texture publication, no free slot). GPU-thread
>   readers and write faults always use the drain path.
> - KYTY_READBACK_SIDE_COPY=0 restores the old behaviour exactly (no side state
>   is created; the only remaining difference is bookkeeping calls that return
>   immediately).
>
> Correctness
> - Copy contents: the side submission carries a pipeline barrier with
>   srcStage=ALL_COMMANDS/srcAccess=MEMORY_WRITE. Its first synchronization
>   scope covers every command submitted earlier to this queue, i.e. every
>   recording older than the current one, so the copy observes all submitted
>   writes (the timeline wait on the producer is belt and braces). Only the
>   unsubmitted current recording can hold newer writes, and any tracked
>   writer there makes its bytes' tick == current -> fallback. Untracked
>   (address) writers make the whole recording ineligible.
> - Producer is always submitted: all ticks < CurrentTick were natively
>   submitted or enqueued in the broker; the broker is drained under
>   queue_mutex before our vkQueueSubmit, so no wait-before-signal occurs.
> - Ownership/visibility follow the existing DownloadBufferMemory protocol: at
>   issue, the exact dirty bytes are subtracted from m_gpu_modified_ranges and
>   a backing publication (BeginBackingPublication) covers them, so
>   TryReadGpuCleanBacking/clean verdicts treat them as not clean until
>   EndBackingPublication, which runs only after WriteBacking. The tracker
>   pages stay GPU-dirty (read+write protected) until after the backing write,
>   so no guest thread (VirtualProtect is process-wide) can observe the page
>   before the data is there, and no CPU write can land before our backing
>   write: a write fault goes through InvalidateMemory -> ReadMemory(write),
>   which first completes the pending side readback, then performs the normal
>   CPU-dirty transition. Host-side InvalidateMemory (file reads etc.) takes
>   the same route.
> - Newer writers during the copy: pages get a "readback pending" bit in the
>   RegionManager at issue. Any GPU ownership transition of a page (a new
>   writer's ChangeState<Gpu,true>, an unmark, a download clear) clears the
>   bit, and completion unprotects only pages whose bit survived. A page that
>   a newer recording re-dirtied stays protected with its new exact dirty
>   bytes; its next readback is ordered after ours (every readback, drain
>   download, GC and buffer deletion first completes overlapping pending side
>   readbacks), so backing publications stay in write order.
> - Other ordering: DownloadBufferMemory, DeleteBuffer (the side copy reads the
>   buffer outside the scheduler timeline), RunGarbageCollector, the
>   destructor, and RenderContext::SynchronizeGpuBackingForRead complete
>   overlapping side readbacks first. Side copies are never issued next to a
>   queued drain/texture publication (HasPendingBackingPublication), so two
>   publications of one byte cannot race.
> - Texture cache: the side path only moves buffer-owned dirty bytes, exactly
>   like the drain path; texture-cache fault handling is unchanged, and
>   image->buffer copies record a writer tick.
> - Known fills: a readback does not change GPU contents, so fill records stay
>   valid; CPU writes still forget them in InvalidateMemory.
> - Clean-verdict cache: the subtract, BeginBackingPublication,
>   EndBackingPublication and the unmark all bump CleanVerdict.
>
> Instrumentation
> - FrameEvent.ReadbackSideCopies / ReadbackSideCopyBytes /
>   ReadbackSideDuplicateWaits / ReadbackSideFallbackCurrentWriter /
>   ReadbackSideFallbackUnbounded / ReadbackSideFallbackOther /
>   ReadbackSidePagesUnmarked / ReadbackSidePagesRetained, and
>   FrameWait.ReadbackSideWait (guest-thread wait + publish time).
> - HangTrace ReadbackKind gains fault-read-side and fault-read-dup rows; the
>   existing RecordReadback call is kept for every path.
>
> Tests: MemoryTrackerTests covers WriteTickMap and the pending-mark unmark
> semantics (including a re-dirty and an explicit unmark during the copy).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/host_gpu/writeTickMap.h`, `tests/MemoryTrackerTests.cpp`.

### 32. Merge side-copy readbacks with guest-thread waits (claude/readback-side-copy)

Commit: [`2351838f`](https://github.com/Jetsku/KytyPS5-experimental/commit/2351838fa3f6e13415b1e305cd147aa1a008db78) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 33. Add sampled per-operation GPU profiler and GPU recording counters

Commit: [`8252bc13`](https://github.com/Jetsku/KytyPS5-experimental/commit/8252bc138a636d07708f4415a0675a2564ecdea9) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_GPU_OP_PROFILE=<seconds> wraps the vulkan.hpp dispatcher's command
> recording entries. Every period, the guest command buffers begun between two
> guest flips get an ALL_COMMANDS timestamp at their start and after every draw,
> dispatch, copy/blit/clear/fill/resolve, barrier, dynamic rendering begin/end
> and query op. Results are read without waiting once the last captured tick
> completes and a background thread writes gpuops-<flip>.csv (per op) and
> appends gpuops-summary.csv (totals and time by kind/site/scope/pipeline/
> render target/caller, barrier and render pass counts by site) into the hang
> trace directory or _Profiling/gpuops-*. Static KYTY_GPU_OP_SITE tags give
> site/scope attribution; vkCmd return addresses are recorded for symbolization.
>
> Cheap counters (render pass begins, pipeline barriers total and per site,
> layout transitions, guest command buffers) are published per flip to the hang
> trace summary and Profiler FrameEvents when KYTY_GPU_OP_COUNTERS, the hang
> trace or aggregate profiling is on. Nothing is hooked when all are unset.
>
> tools/hangtrace/analyze_gpuops.py prints the breakdown and symbolizes
> callers with llvm-symbolizer.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/faultManager.cpp`, `src/graphics/host_gpu/renderer/cache/streamBuffer.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.h`, `src/graphics/host_gpu/renderer/depthRenderTarget.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.h`, `src/graphics/host_gpu/renderer/image/blitHelper.cpp`, `src/graphics/host_gpu/renderer/image/dccClear.cpp`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/tiler.cpp`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/shaderResourceBarrier.cpp`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/presentation/videoOut.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`, `tools/hangtrace/analyze_gpuops.py`.

### 34. Merge sampled per-operation GPU profiler (claude/gpu-op-profiler)

Commit: [`21f5dced`](https://github.com/Jetsku/KytyPS5-experimental/commit/21f5dced87ba1902d23fde51caf9d1b38f1abe20) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> # Conflicts:
> #	src/common/profiler.cpp
> #	src/common/profiler.h
> #	src/graphics/host_gpu/renderer/cache/bufferCache.cpp

</details>

### 35. shader: RDNA2 ISA accuracy fixes (TFE/LWE, readfirstlane, denorms, LDS order, opcodes)

Commit: [`6a25c043`](https://github.com/Jetsku/KytyPS5-experimental/commit/6a25c0430e6711d5bf18cbff86dd42444d99de72) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - MIMG TFE/LWE (word0 bits 16/17) and MUBUF/MTBUF TFE (word1 bit 23) are
>   decoded; loads, samples and gathers write the extra status dword after
>   the data as 0 (resident, LOD not clamped) instead of leaving a stale
>   VGPR. One log line per shader when TFE/LWE is seen.
> - V_READFIRSTLANE_B32 with EXEC == 0 reads lane 0 (ISA p.137) instead of
>   shuffling from FindLSB(0) = -1.
> - Declare SPIR-V float controls matching FLOAT_MODE 0xC0 (DenormFlushToZero
>   32, DenormPreserve 16/64) when the device reports support under its
>   denormBehaviorIndependence; KYTY_SHADER_FLOAT_CONTROLS=0 disables.
> - S_WAITCNT / S_WAITCNT_LGKMCNT with lgkmcnt(0) after LDS writes emits a
>   workgroup OpMemoryBarrier (AcquireRelease|WorkgroupMemory) so intra-wave
>   LDS exchange is ordered, including split wave64. Only emitted when an
>   LDS write is pending since the last barrier; KYTY_LDS_WAITCNT_BARRIER=0
>   disables.
> - New opcodes: V_PERM_B32, S_CMOV_B32, S_CMOVK_I32, S_SEXT_I32_I8/I16,
>   S_{OR,XOR,ANDN2,ORN1,NAND,NOR,XNOR}_SAVEEXEC_B32/B64, image atomics
>   CMPSWAP/SUB/SMIN/SMAX/INC/DEC, buffer atomics INC/DEC and
>   CMPSWAP/ADD/SUB/SMIN/UMIN/SMAX/UMAX/AND/XOR_X2, no-op S_DCACHE_INV,
>   S_GL1_INV, BUFFER_GL0_INV, BUFFER_GL1_INV, S_CLAUSE. FLAT/GLOBAL
>   GLC/SLC/DLC are accepted as cache hints.
> - Tests: shader_cfg_tests --isa-accuracy-only (decode, barrier placement,
>   float-control declarations) and shader_recompiler_compute_tests
>   --isa-accuracy-only (10 GPU cases).

</details>

Changed files: `src/graphics/presentation/window/vulkanWindow.cpp`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterFlow.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterImage.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInstructions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterModule.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterProgram.cpp`, `src/graphics/shader/recompiler/frontend/decode/ImageOps.cpp`, `src/graphics/shader/recompiler/frontend/decode/MemoryOps.cpp`, `src/graphics/shader/recompiler/frontend/decode/ScalarAluOps.cpp`, `src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.cpp`, `src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.h`, `src/graphics/shader/recompiler/frontend/decode/VectorAluOps.cpp`, `src/graphics/shader/recompiler/frontend/translate/Control.cpp`, `src/graphics/shader/recompiler/frontend/translate/Integer.cpp`, `src/graphics/shader/recompiler/frontend/translate/Memory.cpp`, `src/graphics/shader/recompiler/frontend/translate/Scalar.cpp`, `src/graphics/shader/recompiler/frontend/translate/Translate.cpp`, `src/graphics/shader/recompiler/frontend/translate/Translator.h`, `src/graphics/shader/recompiler/frontend/translate/Vector.cpp`, `src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.cpp`, `src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.inc`, `tests/ShaderRecompilerComputeTests.cpp`, `tests/shaderCfgTests.cpp`.

### 36. Merge RDNA2 ISA accuracy fixes: TFE/LWE, readfirstlane EXEC=0, denormals, LDS waitcnt ordering, missing opcodes (claude/isa-accuracy)

Commit: [`e1a00e9b`](https://github.com/Jetsku/KytyPS5-experimental/commit/e1a00e9bbab5a38c7c6777d505c6dee27b510cb7) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 37. Hang trace: record occlusion scopes, dumps and predicates (occlusion.csv)

Commit: [`7c0dfff0`](https://github.com/Jetsku/KytyPS5-experimental/commit/7c0dfff0780794694e301d49a276c9a7f725d1b3) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Diagnoses predicated draws that disappear with KYTY_GPU_OCCLUSION=1 (Sky Garden water): a zero-sample predicate whose dumps bracket counted scopes means the samples were rejected (e.g. wrong depth contents); one bracketing no scopes means the draws never reached a counted scope.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/occlusion.h`.

### 38. Hang trace: occlusion publish values and dump depth targets; LOD report contents

Commit: [`ecf9f0d0`](https://github.com/Jetsku/KytyPS5-experimental/commit/ecf9f0d0efaa32e97036eee9bb3314956d4ee05f) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> occlusion.csv now records the cumulative sample count published per dump and the depth target of the latest counted scope on each dump row (per-scope rows removed: they filled the row limit). lodreports.csv records what each GET_LOD_STATS report tells the guest: sampled counters, samples, mean finest mip and pending GPU copies.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/lodStats.h`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/occlusion.h`.

### 39. LOD reports: rewrite each slot with its own interval when its GPU copy completes

Commit: [`3470c223`](https://github.com/Jetsku/KytyPS5-experimental/commit/3470c22338cccfb1e0c3563d8c314e77200d2762) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Kyty writes EOP labels at record time, so GET_LOD_STATS must write a report when recorded, and that report can only hold older statistics. Astro Bot enables mip-statistics counting in a rotating 16-frame schedule (U32 lodreports.csv: 4-5 reports with samples, 11-12 empty, repeating); the one-interval lag handed the first frame of each counting window an empty report, which its streamer treats as unsampled (decay toward low detail). After the copy completes, the slot is now rewritten with exactly that packet's interval (hardware semantics) unless the guest has already consumed or modified it. KYTY_LOD_REPORT_COMPLETION_WRITE=0 restores record-time-only writes. Also: occlusion draw-state rows (KYTY_HANG_TRACE_OCCLUSION_DRAWS=1).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/lodStats.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`.

### 40. Texture cache: check Tracy availability before IsConnected in the DCC fallback sampler

Commit: [`70b4a368`](https://github.com/Jetsku/KytyPS5-experimental/commit/70b4a3684d7f6d350654954bf70674f3bb149924) · **Compressed metadata and clear semantics**

Handles the named metadata/clear operation without stale contents; eligible GPU work avoids CPU readbacks. Accuracy and lower transfer cost are separate claims; see the water discussion.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> With the profiler disabled (profiler_enabled=false) the Tracy profiler object does not exist; TracyIsConnected dereferenced it and crashed on level load (textureCache.cpp MaterializeDccClear). Every other call site already checks tracy::ProfilerAvailable() first.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`.

### 41. Guard remaining Tracy calls with ProfilerAvailable for runs with the profiler disabled

Commit: [`c399c078`](https://github.com/Jetsku/KytyPS5-experimental/commit/c399c07879ae5a485c4958a04f99cb0d778bf22f) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> MaterializeDccClear plotted DCC totals every 256 attempts without checking that the Tracy profiler exists (second crash on level load with profiler_enabled=false). The detailed-diagnostic message sites relied on detailed profiling being off; they now check availability too.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/renderCompute.cpp`.

### 42. Renderer: batch, merge, elide and sink pipeline barriers (KYTY_BARRIER_BATCH)

Commit: [`447b6aaa`](https://github.com/Jetsku/KytyPS5-experimental/commit/447b6aaa8120d5605c165b43d99bb931a27d07d1) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Per gameplay frame the guest command buffer carried ~3,350 pipeline barriers
> (guest.global_barrier alone 2.6 ms stamped) and ~494 dynamic-rendering begins for
> ~4,400 draws. Every guest ACQUIRE/RELEASE/cache-flush event recorded a full
> ALL_COMMANDS barrier immediately and ended the active rendering instance, even
> when it directly followed another barrier.
>
> Design (CommandBuffer, render.h / context.cpp)
> - Barrier requests are queued on the CommandBuffer instead of recorded:
>   EmitGlobalBarrier (guest), ShaderAccess/ShaderWrite/ShaderWriteHazard barriers,
>   indirect-argument barriers (draw and dispatch), the GDS buffer barrier, and
>   Image::Transit calls from AcquireRenderTargets / CommitBindings (deferrable).
>   Other Image::Transit callers go through the batch but record immediately.
> - The pending batch is recorded as ONE vkCmdPipelineBarrier2 at the next flush
>   point: CommandBuffer::Handle() (every native recording site obtains its handle
>   there), BeginRendering() (the draw is recorded right after it), End() and the
>   GPU-timing end stamp. A flush ends an active rendering instance first.
> - StateHandle() returns the handle without flushing, for state-only commands
>   (pipeline/descriptor/vertex/index binds, dynamic state, push constants,
>   begin/end rendering bookkeeping). The draw path, CommitBindings, BindPipeline and
>   PushDescriptors use it; dispatches re-obtain Handle() for the dispatch itself.
> - Sync's end-of-pipe label/flip/interrupt helpers only validated the buffer via
>   Handle(); they now use IsInvalid() (the writes happen on the CPU at tick
>   completion, so they are not flush points).
> - Merge: a request made while a batch is pending joins it (memory masks OR-ed,
>   buffer/image barriers appended). Two layout transitions of one image never share
>   a call; the second flushes the first.
> - Elide: a memory request is dropped when nothing was recorded since the last
>   recorded batch of this command buffer and that batch's memory dependency covers
>   the request (ALL_COMMANDS covers queue stages, MEMORY_READ/WRITE cover the listed
>   device read/write bits; HOST and unknown bits must match literally).
> - Sink (KYTY_BARRIER_SINK, default on): a pending memory-only batch may stay
>   pending across a draw that continues the same rendering instance, so guest
>   flushes between draws to the same targets no longer split the render pass.
>
> Switches: KYTY_BARRIER_BATCH=0 restores the previous per-site recording paths
> exactly (Handle() is then a plain accessor). KYTY_BARRIER_SINK=0 keeps
> batching/merging/elision but always flushes before the next draw.
>
> Counters (KYTY_GPU_OP_COUNTERS / hang trace): summary.csv appends
> gpu_barrier_requests, gpu_barriers_merged, gpu_barriers_elided,
> gpu_barriers_sunk, gpu_barrier_rp_splits (a draw continuing its instance had to
> restart it to record a pending barrier); matching Profiler FrameEvents. The
> existing gpu_barriers / gpu_render_passes columns measure the result. Recorded
> batches are attributed to gpuOpProfiler sites batch.guest_global,
> batch.shader_access, batch.shader_write, batch.shader_hazard,
> batch.indirect_args, batch.gds, batch.image or batch.mixed (the old per-site tags
> remain for KYTY_BARRIER_BATCH=0).
>
> Correctness argument
> 1. Deferral. A barrier orders commands before it against commands after it.
>    Moving it later is only unsafe if a command that must follow it is recorded
>    first. Every memory-accessing command is recorded through Handle() (which
>    records the batch first), after BeginRendering() (which records it first unless
>    sinking, 4.), or is the begin/end-rendering/query bookkeeping itself, which
>    the batch also precedes or which the original code recorded before the barrier
>    too (EndRendering always preceded these barriers). Commands recorded through
>    StateHandle() (binds, dynamic state, push constants/descriptors) are not
>    memory accesses and are unordered by barriers. Handles cached before a request
>    are only used for such state commands and the draw after BeginRendering();
>    deferrable Transit is limited to AcquireRenderTargets/CommitBindings, whose
>    callers satisfy this. Command buffer end flushes, so no batch crosses a
>    submission (other queue submitters may interleave).
> 2. Merge. Barriers B1..Bn recorded back to back with no command between them are
>    replaced by one call whose memory barrier has the union of their stage and
>    access scopes: every earlier command is in the union's first scope and every
>    later one in its second, so each Bi's dependency and any chain Bi->Bj is
>    implied. Buffer/image barriers in the same call are widened by the union
>    memory scopes, which reproduces the chains they formed with the memory
>    barriers; widening only adds synchronization. Image barriers for the same image
>    are kept in separate, ordered calls.
> 3. Elision. If nothing was recorded since barrier F and F's memory dependency
>    covers request R (stages and accesses), R's first scope and second scope are
>    those of F, so R adds nothing. "Recorded" is tracked conservatively: every
>    Handle() call, every rendering begin/end and every draw counts; the state is
>    reset at command buffer begin (other submissions may precede it).
> 4. Sinking pending batch B past draw D2 in the active instance R removes only the
>    orderings P -> D2 for commands P recorded before B. Allowed only when:
>    (a) a full ALL_COMMANDS/MEMORY_WRITE -> ALL_COMMANDS/MEMORY_READ|WRITE batch F
>        was recorded in this command buffer; commands before F are ordered against
>        D2 by F (F's buffer/image barriers were widened to the same scopes);
>    (b) since F only state commands, the begin of R (first instance after F), and
>        safe draws in R were recorded (anything through Handle(), any end of
>        rendering, a second instance or an unsafe draw clears the epoch);
>    (c) D2 is safe: no storage buffer/image writes or atomics, no address (BDA)
>        writes, no GDS/fault-buffer/LOD-counter bindings, no indirect arguments, no
>        depth feedback loop and no bound image equal to one of its color/depth
>        targets;
>    (d) B holds no buffer/image barriers (layout transitions must precede D2).
>    Then every P writes only R's attachments and reads anything; D2 writes only R's
>    attachments and reads them only as attachments. Attachment accesses of draws in
>    one rendering instance are ordered by rasterization order, so no RAW/WAR/WAW
>    hazard between P and D2 needs B. B itself is still recorded before the next
>    non-safe command and at the latest at command buffer end, covering P and D2
>    for everything later. Pipeline barriers are never recorded inside rendering.
>
> What to watch at runtime: flicker or stale content in passes that read results
> of compute/copies (missing dependency), wrong post-processing or shadows on
> render targets drawn after in-pass guest flushes (sinking; retry with
> KYTY_BARRIER_SINK=0), Vulkan validation errors about barriers inside rendering or
> layout mismatches (retry with KYTY_BARRIER_BATCH=0).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.h`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/image.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/shaderResourceBarrier.cpp`, `src/graphics/host_gpu/renderer/pipeline/shaderResourceBarrier.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/host_gpu/renderer/sync.cpp`.

### 43. Merge barrier batching, merging, elision and sinking (claude/barrier-coalesce)

Commit: [`d15346c6`](https://github.com/Jetsku/KytyPS5-experimental/commit/d15346c68bd8ed78e0be997c8e3b5df300013307) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 44. Texture cache: direct depth<->color reinterpretation, alias sync skip, transfer attribution

Commit: [`d36d8b32`](https://github.com/Jetsku/KytyPS5-experimental/commit/d36d8b321759681829f27a9b4a8de8efce9e4008) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Reinterpretation copies (TextureCache::CopyImage between a depth and a color
> interpretation of one allocation) no longer go image -> staging buffer -> image
> when a direct path exists:
> - VK_KHR_maintenance8 (enabled when available): vkCmdCopyImage between the depth
>   aspect of D32/D16 depth-only images and R32/R16 SFLOAT/UINT/SINT color images
>   (new site image.copy_depth_color).
> - Otherwise a one-pass shader: D32 depth -> any 32-bit color image via compute
>   (texelFetch + floatBitsToUint into an R32_UINT storage view), 32-bit color ->
>   D32 depth-only image via a fullscreen draw writing gl_FragDepth from an R32_UINT
>   storage view (sites image.reinterpret_depth_to_color / _color_to_depth).
> - The buffer path stays as fallback (3D, MSAA, stencil destinations, D24, other
>   sizes). KYTY_DIRECT_IMAGE_COPY=0 disables all of it; _M8=0 / _SHADER=0 each path.
>
> Images now carry a content serial: every recorded write gives a fresh one (Image
> copy/upload/resolve methods, TextureCache::MarkImageGpuModified). After a lossless
> full-shape copy the destination adopts the source serial. SyncAliasFromOwner skips
> the copy when the alias already holds exactly the owner's bits (e.g. D32 synced to
> R32F, R32F only sampled, then D32 bound again), with identical ownership changes.
> KYTY_ALIAS_SYNC_SKIP=0 restores the unconditional copy.
>
> Diagnostics: HangTrace transfers.csv (per second, by reason/address/shape) and
> summary.csv columns for image uploads (first-use, cpu-write, cpu-edge-hash,
> gpu-buffer-write with the last GPU writer kind, dirty span), buffer uploads (by
> caller), reinterpretation copies (by path and caller) and alias sync copy/skip.
> Matching Profiler FrameEvents for Tracy.

</details>

Changed files: `CMakeLists.txt`, `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/graphicContext.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/image/blitHelper.cpp`, `src/graphics/host_gpu/renderer/image/blitHelper.h`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/image.h`, `src/graphics/host_gpu/shaders/gpu_blit_color32_to_depth.frag`, `src/graphics/host_gpu/shaders/gpu_blit_depth_to_color32.comp`, `src/graphics/presentation/window/vulkanWindow.cpp`.

### 45. Texture cache: keep content identity across read-only depth target binds

Commit: [`9a82ff81`](https://github.com/Jetsku/KytyPS5-experimental/commit/9a82ff815d8a0bd60f7d1c67818122a56b5795ba) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The Sky Garden gpuops capture shows ~32 D32 -> 4-byte color alias copies per
> frame, one before each 960x540 mesh draw (RGBA16F + D32 targets): every draw
> samples the depth memory through a non-depth-compatible color alias while the
> D32 image is bound as depth target. FindDepthTarget counts every bind as a write,
> so the alias serial never matched and each draw re-copied unchanged depth.
>
> Split content serial updates into definite writes (Image copy/upload/resolve,
> texture-cache clears, DCC and blit helper passes, depth/stencil copies) and
> possible writes (MarkImageGpuModified at bind time). AcquireRenderTargets marks
> the depth image before FindDepthTarget and, when the draw writes neither aspect
> (no depth write, no load clear, no stencil write or clear: LOAD/STORE only),
> restores the serial if no definite write happened in between. Later alias syncs
> from that depth image are then skipped until a draw really writes it.
> Covered by KYTY_ALIAS_SYNC_SKIP=0.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/depthRenderTarget.cpp`, `src/graphics/host_gpu/renderer/image/blitHelper.cpp`, `src/graphics/host_gpu/renderer/image/dccClear.cpp`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/image.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`.

### 46. Merge direct depth/colour reinterpretation copies, alias-sync skip and transfer attribution (claude/image-copy-direct)

Commit: [`4277e059`](https://github.com/Jetsku/KytyPS5-experimental/commit/4277e059992e27dd749ccc3c3027c29baa20a301) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> # Conflicts:
> #	src/common/hangTrace.cpp
> #	src/common/profiler.cpp
> #	src/common/profiler.h

</details>

### 47. renderer: draw-prep S3 parallel stage materialization and per-draw memos

Commit: [`c7c4da45`](https://github.com/Jetsku/KytyPS5-experimental/commit/c7c4da45526b11c50744f4063568fdf97a99cc18) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Parallel stage preparation (draw-prep S3, KYTY_STAGE_PREP_PARALLEL=0 disables):
> - A single helper thread (DrawPrep#0, stagePrepWorker.{h,cpp}) materializes the pixel
>   stage while the GPU thread materializes the vertex stage of the same draw
>   (fork/join inside GetGraphicsPrograms). Both use probe-only readers: every guest
>   read goes through the silent clean-backing probe and a read that is not provably
>   clean fails the stage without faulting, reading back or recording missing ranges.
>   Permutation lookup and binding stay serial in the original order (push data).
>   Any failure (probe miss, source or permutation not yet published, helper asleep)
>   falls back to the unchanged serial path.
> - TryReadGpuCleanBacking accepts the helper inside a GpuReadDelegate scope
>   (gpuReadDelegate.h). During the fork window the GPU thread only performs the same
>   read-only probes, so its unlocked dirty state is stable.
> - The helper spins KYTY_STAGE_PREP_SPIN_US (default 250) after its last job, then
>   sleeps; a fork never waits for a sleeping helper (it is woken for the next draw).
> - KYTY_STAGE_PREP_VERIFY=1|exit reruns the serial preparation after each parallel
>   one and counts/logs (or stops on) differences.
> - Tessellation, O15 reuse mode and PS-less draws stay serial.
>
> Per-draw memos keyed on exact inputs (each can be disabled):
> - KYTY_PROGRAM_LOOKUP_MEMO: per-thread last source entry per stage (skips the shared
>   lock and hash; entries are never erased) and last matched permutation per source
>   (permutations are unique per specialization and push-data start).
> - KYTY_PIPELINE_MEMO: last GraphicsPipelineKey -> pipeline, compared with the map's
>   own key equality before hashing and locking; the key is now built unlocked.
> - KYTY_SAMPLER_MEMO: final sampler dwords -> VkSampler (the cache never evicts).
> - KYTY_TARGET_DESC_MEMO: color/depth target descriptions keyed on the raw target
>   register bytes (plus mask and slice offset); FindImage still runs every draw.
>   The color decision-log counter no longer takes an atomic RMW after 128 logs.
> - KYTY_DYNAMIC_STATE_SHADOW: dynamic state is recorded only when it differs from the
>   values the previous draw left in the same command buffer while its pipeline is
>   still bound (bitwise compare; color-write enable and the other optional groups
>   are tracked separately; blits or Begin change the bound pipeline).
> - KYTY_SHADER_MAP_MEMO: per-thread shader map lookups tagged with a map generation.
> - KYTY_SHADER_METADATA_BATCH: the two shader-header marker words and the used spans
>   of the vertex attribute and V# tables are read with one silent probe each
>   (about 11 probes per draw before); a failed probe falls back to per-word reads.
>
> Other:
> - DrawIndirectNative reuses the member DrawRenderState instead of value-initialising
>   ~37 KB and allocating fresh preparation vectors per draw; mesh restart segments use
>   a reused vector.
> - ResolveTexture resolves into the destination binding in place (two ~0.5 KB
>   description copies fewer per image); a returning overload remains for callers.
> - Detail zones: Programs::PrepareStatic, ProgramCache::GetParallel,
>   Draw::RefreshShaders, Draw::ResolveTargets, Draw::PrepareBindings,
>   Draw::PrepareGraphicsBindings, Draw::CommitBindings, Draw::DynamicState.
> - Frame events for every memo (hits/misses), the parallel path (draws, declined
>   forks, speculative failures, lookup fallbacks, verify mismatches) and frame waits
>   StagePrepJoin (GPU-thread join spin) and StagePrepHelper (helper job time).
> - resource_materialization_tests: the probe-only runtime equals the serial runtime
>   on clean memory and refuses a stage with an unclean word.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/gpuReadDelegate.h`, `src/graphics/host_gpu/renderer/colorRenderTarget.cpp`, `src/graphics/host_gpu/renderer/colorRenderTarget.h`, `src/graphics/host_gpu/renderer/depthRenderTarget.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/stagePrepWorker.cpp`, `src/graphics/host_gpu/renderer/pipeline/stagePrepWorker.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/shader/shader.cpp`, `src/kernel/memory.cpp`, `tests/ResourceMaterializationTests.cpp`.

### 48. Merge draw-prep S3: parallel stage materialization and exact per-draw memos (claude/draw-prep-s3)

Commit: [`f3426ea8`](https://github.com/Jetsku/KytyPS5-experimental/commit/f3426ea8b15878a385f318a318fcc86fdbc7cea2) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> # Conflicts:
> #	src/common/profiler.cpp
> #	src/common/profiler.h

</details>

### 49. Occlusion: optional synchronous publication of depth-only proxy results (KYTY_OCCLUSION_SYNC_PROXY=1)

Commit: [`21c30b9e`](https://github.com/Jetsku/KytyPS5-experimental/commit/21c30b9ec355616d0c715b235d920e30fcb16624) · **Occlusion and result publication**

Makes visibility results usable with the required publication order, or reduces query/reduction waits. Only guest decisions using the result can reduce draws; an isolated culling gain is unproven.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Test for the missing Sky Garden water: RenderDoc replay shows its proxy boxes pass millions of samples, yet the water only renders in the synthetic mode that publishes at record time. With the switch, an end dump that closes a depth-only scope waits for its publication (BufferWait) before the CP processes the following label, as hardware guarantees. About four proxy pairs per frame.

</details>

Changed files: `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/occlusion.h`.

### 50. Occlusion: publish visibility-proxy results synchronously by default

Commit: [`2209c91e`](https://github.com/Jetsku/KytyPS5-experimental/commit/2209c91ea51122e000d08d02a52aaeccde48048a) · **Occlusion and result publication**

Makes visibility results usable with the required publication order, or reduces query/reduction waits. Only guest decisions using the result can reduce draws; an isolated culling gain is unproven.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Verified in U37 (2026-09-27): with KYTY_OCCLUSION_SYNC_PROXY=1 the Sky Garden water renders with GPU occlusion enabled and textures stay sharp. The game reads a proxy's result right after the label that follows its end dump; Kyty writes labels at record time, so asynchronous publication handed it stale results. KYTY_OCCLUSION_SYNC_PROXY=0 restores asynchronous publication.

</details>

Changed files: `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/occlusion.h`.

### 51. Recompiler: record BDA faults with OpAtomicOr

Commit: [`89601426`](https://github.com/Jetsku/KytyPS5-experimental/commit/896014262571e8bb89f0dfbaeeb191b9a9cf1a39) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RecordBdaFault set the page bit with a load/or/store sequence, so lanes and waves faulting on pages that share a bitmap word could drop each other's bits and a fault could go unhandled. Found by the 2026-09-27 gap analysis.

</details>

Changed files: `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp`.

### 52. renderer: precise write ranges for writable storage bindings

Commit: [`e69f6e06`](https://github.com/Jetsku/KytyPS5-experimental/commit/e69f6e06cda5ca2db35194c06f61d88b44079b2b) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A recompiler range proof (ir/passes/WriteRangeAnalysis) records, per written
> storage buffer, how every store/atomic address is formed from values bounded
> at compile time (constants, local thread/lane ids) or at record time (user
> data, flattened SRT dwords, direct dispatch size). RebindBuffers evaluates it
> with the snapshot the shader actually receives; a provable binding marks only
> the written spans GPU-owned (BufferCache::ObtainWrittenBuffer) and invalidates
> only images overlapping them. Any unknown source (memory loads, lane
> shuffles, loop-carried values, possible 32-bit wrap) keeps the whole binding.
>
> KYTY_PRECISE_WRITE_RANGES=0 restores whole-binding marking.
> KYTY_WRITE_RANGE_LOG=N logs the first N writable bindings with shader hash,
> range, proven spans and unknown sources. FrameEvent.WriteRange{Narrowed,
> Whole,BytesAvoided,ImagesSpared} count the effect (KYTY_WRITE_RANGE_STATS=0
> skips the image lookup behind ImagesSpared).
>
> Also updates the stale descriptor-binding ABI assertion (MipStats) in
> resource_tracking_tests.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.h`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/shader/recompiler/ShaderRecompiler.cpp`, `src/graphics/shader/recompiler/ir/Program.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/passes/WriteRangeAnalysis.cpp`, `src/graphics/shader/recompiler/ir/passes/WriteRangeAnalysis.h`, `tests/ResourceTrackingTests.cpp`.

### 53. Merge precise storage-write ranges from recompiler range proofs (claude/precise-write-ranges)

Commit: [`17a09f82`](https://github.com/Jetsku/KytyPS5-experimental/commit/17a09f8271b3fee6b0beef332f03a188133bcd43) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 54. DCC: format-independent GPU clear materialization; draw-write sink; occlusion dump-pair gate

Commit: [`96248b8d`](https://github.com/Jetsku/KytyPS5-experimental/commit/96248b8d6f43aaa61f2184d2204a1a9313055b16) · **Occlusion and result publication**

Makes visibility results usable with the required publication order, or reduces query/reduction waits. Only guest decisions using the result can reduce draws; an isolated culling gain is unproven.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DCC (KYTY_DCC_GPU=1): the native path accepted only single-layer RGBA16F render
> targets, so U36 Sky Garden recorded zero native inspections and every GPU-written
> metadata lookup drained the GPU (~2/flip, ~21 ms/flip). The validation shader now
> takes the CPU decoder's per-code acceptance for the binding (00/20/40/80/C0), the
> clear value's texel bits come from vkCmdClearColorImage on a 1x1 palette image in
> the view format, and the conditional clear stores them through an R8/R16/R32/
> R32G32/R32G32B32A32 UINT alias view, per metadata slice/layer. Any binding but
> video-out, any 1/2/4/8/16-byte color view format, multi-layer views. Reuse of a
> retained inspection now also requires the current accepted codes to be a subset of
> the inspected ones. Every CPU fallback is counted under its first failing reason
> (FrameEvent DccFallback*) and the first four of each reason are logged with image
> state.
>
> Render passes: draws with guest storage-buffer writes ended their rendering instance
> to record the post-draw shader-write barrier (1,546 of 1,889 begins in a heavy U31
> frame). KYTY_DRAW_WRITE_SINK (default on) keeps that barrier pending across later
> draws of the same instance; any other barrier origin or non-draw command still
> records it first. DB_COUNT_CONTROL changes no longer split an instance when neither
> side is counted. Per-site EndRendering counters (GpuOps.EndRendering.<site>,
> summary.csv gpu_rendering_ends, gpu_draw_write_sinks).
>
> Occlusion (KYTY_OCCLUSION_GATE, default on): queries are recorded only while a
> begin/end dump pair is open; unexpected dump patterns disable the gate.

</details>

Changed files: `CMakeLists.txt`, `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.h`, `src/graphics/host_gpu/renderer/image/dccClear.cpp`, `src/graphics/host_gpu/renderer/image/dccClear.h`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/occlusion.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/host_gpu/shaders/gpu_dcc_clear.comp`, `src/graphics/host_gpu/shaders/gpu_dcc_clear_rgba16f.comp`, `src/graphics/host_gpu/shaders/gpu_dcc_validate.comp`.

### 55. Merge GPU DCC clears for more formats, draw write-barrier sinking and occlusion query gating (claude/dcc-renderpass)

Commit: [`a24c308d`](https://github.com/Jetsku/KytyPS5-experimental/commit/a24c308dafaaf60c5b4fdaeaea86ca9128cf21bd) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> # Conflicts:
> #	src/common/profiler.cpp
> #	src/common/profiler.h

</details>

### 56. CP: batch command-buffer flushes requested by end-of-pipe interrupts

Commit: [`99e9721c`](https://github.com/Jetsku/KytyPS5-experimental/commit/99e9721cdcc2bbf71a6d3c8ec86b46ca42c898dd) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every RELEASE_MEM with an interrupt (and every data_sel==1 label) flushed the command buffer: ~387 of ~531 host command buffers per frame in the U28/U37 captures, each ending dynamic rendering and resetting bound state. Interrupts are delivered when their tick completes, and every slice that made progress ends with a flush (including slices that suspend on WAIT_REG_MEM), so a pending interrupt is always submitted before the CP can block on the guest's response. Now only every KYTY_EOP_FLUSH_BATCH-th request flushes (default 8; 1 restores the old behaviour). Interrupt timing shifts later by at most the batched work.

</details>

Changed files: `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/guest_gpu/graphicsRun.cpp`.

### 57. memory: instrument guest faults, page protection and buffer uploads

Commit: [`faeed450`](https://github.com/Jetsku/KytyPS5-experimental/commit/faeed4506a71ae8161daec7679f9dd51174b54c6) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Adds cheap counters for the memory-tracking / upload audit, published both as
> Profiler FrameEvents (cumulative per guest flip, KYTY_PROFILE_FRAMES_ONLY=1 +
> KYTY_PROFILE_AGGREGATES=1) and as appended hang-trace summary.csv mem_* columns
> (per second, KYTY_HANG_TRACE=1), through the new graphics/host_gpu/memoryStats.h:
>
> - guest write / read faults handled by RenderContext::HandleFault and the
>   steady-clock (QPC) time spent inside it;
> - host protection calls and pages, split into write-protect and restore
>   read-write, plus the time inside ProtectGuestHostMemory (PageManager);
> - contended region-tracking spinlock acquisitions;
> - tiler scratch vmaCreateBuffer count / bytes / time;
> - SynchronizeBufferFromImage downloads;
> - copies, barriers and rendering-instance ends recorded by CPU-dirty buffer
>   uploads (SynchronizeBuffer, site buffercache.upload).
>
> Nothing changes what is recorded. With both sinks off every count is two
> predictable branches on cached flags and no clock is read. Enum/column lists
> are append-only (FrameEvent, HangTrace::MemoryCounter).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryStats.h`, `src/graphics/host_gpu/pageManager.cpp`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/image/tiler.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`.

### 58. memory: guest faults first in the VEH chain, cheaper fill bookkeeping

Commit: [`3240f48b`](https://github.com/Jetsku/KytyPS5-experimental/commit/3240f48bf50ef246ed1702af1f0018b3dfd0cc34) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Three small items from the memory/upload audit.
>
> 1. Guest tracking faults are resolved by a vectored handler registered FIRST
>    (AddVectoredExceptionHandler(1, ...)) instead of only by the handler
>    registered last. The first-position handler (HostException::
>    InstallFirstAccessHandler) only looks at access violations and runs the
>    non-terminating part of KytyExceptionHandler (hang-trace LOD watch, then
>    Memory::HandleGpuFault); anything it does not resolve returns
>    EXCEPTION_CONTINUE_SEARCH and reaches the unchanged last-position
>    KytyExceptionHandler (x64 emulation, diagnostics, EXIT), so exceptions
>    owned by other components see the same handler order as before.
>    Correctness: the only in-tree vectored handler is ours (Tracy uses
>    SetUnhandledExceptionFilter, which runs after all VEHs either way);
>    HandleGpuFault claims a fault only inside GPU-mapped guest ranges, which no
>    other component owns, and the same code already ran for those faults.
>    Illegal instructions stay on the last handler. KYTY_VEH_FIRST=0 restores the
>    previous registration.
>
> 2. ForgetKnownFills, called on every guest write fault and writable binding,
>    returns before taking its mutex when no fill is known (atomic flag kept
>    under the mutex) and no longer allocates a vector when no known fill
>    overlaps the range. Same result: nothing to forget in either case. A fill
>    recorded concurrently with the check is ordered as if the fault took the
>    mutex first, which the mutex allowed before.
>
> 3. KYTY_WRITE_FAULT_WINDOW_KB (power of two, 4..512, default 512 = unchanged)
>    sets the readback window of guest write faults on GPU-owned pages
>    (ReadMemoryDrain). 4 clips the download to the faulting page. Correct for
>    any value: the window is only the range whose GPU-dirty bytes are
>    published and whose pages lose GPU ownership; pages outside it stay GPU
>    owned and protected. The default is unchanged because a smaller window
>    turns one drain per 512 KiB into one per window for a CPU writer walking a
>    GPU-written range (compare readbacks.csv FaultWrite rows before changing).

</details>

Changed files: `src/common/hostException.cpp`, `src/common/hostException.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/loader/runtimeLinker.cpp`.

### 59. buffers: keep a GPU-modified image's contents when a GPU write takes it over

Commit: [`842e7080`](https://github.com/Jetsku/KytyPS5-experimental/commit/842e708058b0d1e86cc44d7845b2e49bda785910) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Accuracy fix. A writable storage binding (and a GPU fill/copy destination)
> over a GPU-modified image made TextureCache::InvalidateMemoryFromGPU clear the
> image's GPU ownership and mark it buffer-modified, so it was later rebuilt from
> the buffer. For written bindings SynchronizeBuffer never pulled the image into
> the buffer (only read-only texel bindings do, SynchronizeBufferFromImage), so
> the rebuilt image and every CPU readback got stale guest bytes everywhere the
> shader did not store (read-write bindings also read stale data). Precise write
> ranges shrink the overlap but cannot remove it.
>
> Now BufferCache::PreserveImagesForGpuWrite runs for every GPU-written range
> (ObtainBuffer written, each ObtainWrittenBuffer range) after its upload and
> before the writer is recorded and before InvalidateMemoryFromGPU:
>
> - it finds exactly the images InvalidateMemoryFromGPU will take over (byte
>   overlap, GPU-modified);
> - an image is moved only when Image::SafeToDownload() holds (GPU-modified, not
>   buffer-modified, not CPU-dirty: its native contents supersede every byte of
>   its range), it has no depth association, it starts inside the binding's
>   buffer, and no other GPU-modified image overlapping it lies outside the
>   written range (an alias whose contents would otherwise be replaced);
> - the moved range R = image range clipped to the buffer is owned exactly like
>   a writable binding: SynchronizeBuffer(R, written) uploads CPU-dirty pages and
>   takes the tracker pages under their locks, the image is downloaded over it
>   with the existing DownloadImage / tiler path (RecordImageDownload, factored
>   out of SynchronizeBufferFromImage; every mip that fits), R is added to the
>   GPU-dirty byte ranges (clean-verdict bump, write ticks, known fills, hang
>   trace as for any GPU write), and InvalidateMemoryFromGPU(R) then hands every
>   overlapping image to the buffer;
> - the shader / fill / copy is recorded afterwards, so it overwrites only what
>   it writes.
>
> Correctness: the buffer ends up holding the image's newest bytes for R plus
> the writer's stores, and owns R as GPU-dirty, so the image rebuild (from the
> buffer) and CPU readbacks (from the buffer) both see them. Bytes of R the
> download does not cover hold their guest values and read back unchanged.
> Ordering: upload, then download (both before the writer) matches
> SynchronizeBufferFromImage. A guest thread that dirties the image between scan
> and download makes the re-check fail; its fault waits for this GPU-thread
> command and reads back guest-valued bytes, as for any GPU-owned page. No new
> buffer is created (R is clipped to the binding's buffer), so bindings resolved
> earlier keep their buffers. FillBuffer / CopyBuffer now invalidate images after
> obtaining the destination (and CopyBuffer after synchronizing its source, which
> also lets a source image the destination overlaps be pulled in first).
>
> Unchanged / limits: images starting before the buffer, unsupported downloads,
> unsafe or aliased images keep the previous (lossy) behaviour and are counted;
> bytes of an image beyond the buffer end are still lost (counted as partial).
>
> Switch: KYTY_IMAGE_WRITEBACK_ON_GPU_WRITE=0 restores the previous behaviour and
> call order. Counters (FrameEvent / hang-trace mem_*): ImageWritebacks,
> ImageWritebackBytes, ImageWritebackPartial, ImageWritebackSkips.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryStats.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`.

### 60. memory: fault-ahead window and hot pages for guest write faults

Commit: [`6183827a`](https://github.com/Jetsku/KytyPS5-experimental/commit/6183827acb82460d737c6860eed9bf0ffb3847a7) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every guest write to a buffer page the GPU does not own used to cost a full
> 4 KiB fault/unprotect/reprotect cycle: the fault marks one page CPU-dirty and
> unprotects it (RegionManager::ChangeState), and the next upload clears the bit
> and write-protects the page again (UpdateProtection), so a page the CPU writes
> every frame faults every frame, and a CPU writer walking a range faults once
> per page.
>
> Fault-ahead (KYTY_FAULT_AHEAD_KB, power of two 8..256, default 32; 0 or 4 =
> off). RenderContext::HandleFault now passes write faults to
> BufferCache::InvalidateMemory(..., write_fault = true). When the faulting page
> is not GPU-dirty (the normal, non-flush path), RegionManager::MarkWriteFault
> makes it CPU-dirty as before and also every page of the aligned window that is
> neither GPU-dirty nor already CPU-dirty, in one protection update. Correctness:
> CPU-dirty only means "may have changed; upload before GPU use", so extra dirty
> pages cost upload bytes and never correctness; GPU-dirty pages (and pages with
> a pending side readback, a subset) are excluded, so no GPU ownership changes;
> page watchers are reference counted, so pages an image still watches stay
> protected and keep faulting for the texture cache. Known fills are forgotten
> for the whole window, since writes there no longer fault. The GPU-dirty
> (flush) path is unchanged.
>
> Hot pages (KYTY_HOT_PAGES=0 disables; KYTY_HOT_PAGE_FRAMES default 3,
> KYTY_HOT_PAGE_QUIET_FRAMES default 8, KYTY_HOT_PAGE_MAX default 1024). A page
> this tracker protected that write-faults in N consecutive guest frames (frame
> clock: BufferCache::AdvanceFrame at each completed guest flip) becomes hot: it
> stays CPU-dirty and writable across read-only uploads (CollectUpload keeps it
> dirty and reports it separately), so its writes stop faulting. Each upload of
> a hot page snapshots it once and copies the snapshot only if it differs from
> an exact 4 KiB shadow of the last snapshot uploaded (memcmp, no hash); the
> snapshot is also what becomes the new shadow, so the shadow always equals what
> the buffer received even while the guest keeps writing. A shadow is valid only
> for the buffer registered at the page and is erased by any other upload of the
> page, any GPU-side content write (NoteBufferContentWrite: written bindings,
> joins, image downloads, fills, copies), an unbounded address writer
> (InvalidateContentRevisions clears all) and the buffer's unregistration; a page
> without a shadow is always copied. A hot page returns to normal tracking (stays
> dirty until its next upload protects it) after K frames without a change, after
> K frames without an upload (sweep every 8 frames from RunGarbageCollector), on
> any written upload (GPU ownership needs a clean page, and ForEachUploadRange
> demotes before clearing), on untracking, or when the shadow budget is full.
> Hot-unaware ForEachUploadRange callers demote too. While hot pages exist
> CpuMutationEpoch() is saturated, so the incremental BDA scan
> (KYTY_BDA_INCREMENTAL_SYNC) never skips a pass. Faults on pages that were
> already CPU-dirty (another watcher) never count toward promotion.
>
> Instrumentation (FrameEvent / hang-trace mem_*): FaultAheadPages,
> HotPagePromotions / Demotions, HotPageUploads, HotPageUploadsSkipped, next to
> the fault / protection counters. BitArray gains &, | and Count().
>
> Tests (memory_tracker_tests): fault-ahead window around a GPU-owned page and
> its protection calls, the unchanged flush path, promotion only after
> consecutive frames, hot pages surviving hot-aware uploads writable, demotion
> by hot-unaware uploads, written uploads, explicit demotion, the idle sweep,
> untracking, the hot budget, and faults on already-dirty pages not promoting.

</details>

Changed files: `src/common/bitArray.h`, `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryStats.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/presentation/videoOut.cpp`, `tests/MemoryTrackerTests.cpp`.

### 61. memory: copy written uploads without holding the region tracking locks

Commit: [`66a5600d`](https://github.com/Jetsku/KytyPS5-experimental/commit/66a5600d68bdfb7a55a4d8d29f15cd5eaa98f1a4) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> For a written binding (the range becomes GPU-owned), ForEachUploadRange held
> every region spinlock of the range from collecting the CPU-dirty pages through
> the memcpy into staging until the GPU bits were set, so a guest write fault
> anywhere in those 4 MiB regions spun for the whole copy (and the copy of a
> large binding held several region locks at once).
>
> MemoryTracker::ForEachWrittenUploadRange (used by SynchronizeBuffer for
> written uploads unless KYTY_UPLOAD_COPY_OUTSIDE_LOCK=0):
>  1. per region, under its lock: clear and write-protect the CPU-dirty pages and
>     report them; release the lock;
>  2. copy them with no tracker lock held;
>  3. take all region locks (ascending, as before) and collect again: pages that
>     became CPU-dirty since step 1 (a racing write fault, or fault-ahead from a
>     neighbouring fault) are cleared and write-protected again and only then
>     copied, under the locks, into a separate staging range;
>  4. set the GPU bits for the whole range and release the locks (unchanged).
>
> Correctness: a page not re-reported in step 3 stayed CPU-clean from step 1 to
> step 4, i.e. write-protected the whole time (any write would have faulted and
> set its CPU bit under the region lock), so its step-2 copy is exact. A page
> re-reported in step 3 is protected before its late copy, so every write that
> completed before the protection is copied and any later write faults, waits
> for the lock and then sees a GPU-owned page (the flush path, exactly as
> before). Steps 3-4 are the previous single locked upload, so the GPU/CPU-dirty
> exclusivity invariant holds when the GPU bits are set. Pages that were
> GPU-dirty are never copied; their faults go through the GPU thread, which is
> busy in this upload, as before. The late copy is recorded after the main copy
> with a transfer->transfer barrier between them (it rewrites the same pages).
> Hot pages in a written range are demoted in step 1 and copied normally.
>
> Counter: WrittenUploadLatePages (FrameEvent / mem_written_upload_late_pages);
> TrackerLockContended from the instrumentation commit shows the contention this
> removes. Test: memory_tracker_tests TestWrittenUploadCopiesOutsideLock (a
> racing write fault from another thread during the unlocked copy completes
> without deadlock, is re-copied protected in the late pass, and the range ends
> GPU-owned; no late pages without a race).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryStats.h`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `tests/MemoryTrackerTests.cpp`.

### 62. buffers: batch CPU-dirty uploads behind one barrier pair

Commit: [`fbda279f`](https://github.com/Jetsku/KytyPS5-experimental/commit/fbda279fb0cc83e06bd7e0ea8a4016d4efe37ac8) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> SynchronizeBuffer ended rendering and recorded its own full-buffer barrier
> pair around every upload (AllCommands -> Transfer, copy, Transfer ->
> AllCommands) directly, bypassing the barrier batcher; a written binding
> (ObtainWrittenBuffer) could do that 1 + N times, and every storage binding of a
> draw repeated it.
>
> With KYTY_UPLOAD_BATCH (default on, needs KYTY_BARRIER_BATCH) an upload is
> queued on the command buffer (CommandBuffer::RequestUploadCopy) and recorded at
> the batcher's next flush point by RecordPendingUploads: one pipeline barrier
> with a buffer barrier per distinct destination (the same scopes as the old
> pre-copy barrier), all queued copies, and the post-copy buffer barriers (the
> old post-copy scopes) added to the batch FlushBarriers records immediately
> after the copies, so they merge with whatever else was pending (image layout
> transitions from RebindImages, etc.). BufferCache::UploadBatch scopes keep the
> queue open across all storage bindings of a dispatch (RebindBuffers) and of all
> stages of a draw (PrepareGraphicsBindings) and flush at their end; outside a
> scope an upload is flushed right away (pre barrier, copy, post barrier merged
> with the pending batch).
>
> Correctness:
> - Every command recorded through CommandBuffer::Handle(), BeginRendering() and
>   End() flushes the queue first, exactly like pending barriers, so the copies
>   precede any command that can read or write their destination or staging
>   source (joins, image downloads into the buffer, tiler passes inside the
>   scope all obtain fresh handles). The scope ends with a flush, so no copy is
>   deferred past the code that was audited.
> - Copies in one batch are unordered: RequestUploadCopy records the queue first
>   when a new request overlaps a queued destination range of the same buffer
>   (e.g. a page re-dirtied between the read and the written upload of
>   ObtainWrittenBuffer). Written uploads whose late pass (unlocked copy) found
>   re-dirtied pages keep the direct recording with their transfer->transfer
>   barrier.
> - Moving the other pending barriers after the copies is safe: the copies only
>   write their destinations (ordered after all earlier accesses by the pre
>   barrier) and read host-written staging memory.
> - A batch with queued uploads is never sunk past a draw (CanSinkPending /
>   CanSinkDrawWrites require an empty upload queue); End() flushes before
>   submission, so staging reservations stay in their recording's tick.
> - New BarrierOrigin::Upload (site batch.upload) for attribution.
>
> Result per draw/dispatch with uploads: 2 barrier commands (pre, and post merged
> with the pending batch) instead of 2 per upload plus the pending batch.
> Counters: BufferUploadCopies / Barriers / RenderSplits now count batched
> flushes; the batcher's own request/merge counters include upload requests.
> KYTY_UPLOAD_BATCH=0 restores the per-upload recording. Not done: hoisting
> uploads into a prologue command buffer (item 3b) needs exact per-recording read
> tracking of destination ranges and is left out.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/render.h`.

### 63. images: batch copy_via_buffer regions, widen direct depth<->color copies

Commit: [`fe7f7ede`](https://github.com/Jetsku/KytyPS5-experimental/commit/fe7f7edeb3dc72049ec0b0732640d38ec470e5d9) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> 1. Image::CopyImageWithBuffer (the image->buffer->image reinterpretation
>    fallback) recorded, per (mip level, slice, row chunk), a barrier, a
>    one-region copyImageToBuffer, a barrier and a one-region copyBufferToImage,
>    all at buffer offset 0 of the 128 MiB device-local scratch buffer. Regions
>    are now packed at distinct offsets (multiples of lcm(texel block bytes, 16),
>    so block, depth-aspect (4) and 3/6/12-byte texel rules hold) and each round
>    records one barrier, ONE copyImageToBuffer with every region, one barrier
>    and ONE copyBufferToImage; a new round starts only when the buffer is full
>    (any single region fits an empty buffer, as before). Bit-exact: every
>    region copies the same texels with the same aspects, extents and packed
>    row layout as before; regions never overlap in the buffer
>    (VUID ...pRegions-00184) nor in the destination image; the round barriers
>    are the old ones widened to the used range. KYTY_COPY_VIA_BUFFER_BATCH=0
>    restores one region per round. Counter: FrameEvent ImageCopyViaBufferRounds.
>
> 2. VK_KHR_maintenance8 depth<->color vkCmdCopyImage (TryDirectReinterpret):
>    the spec's "compatible depth-stencil and color formats" list
>    (formats-compatible-zs-color) makes the DEPTH aspect of D32_SFLOAT_S8_UINT
>    size-compatible with R32_SFLOAT/SINT/UINT and that of D16_UNORM_S8_UINT
>    (and D16_UNORM) with R16_SFLOAT/UNORM/SNORM/UINT/SINT. Maintenance8CopyCompatible
>    now accepts those pairs. The copy moves raw depth-aspect bits and never
>    touches the stencil aspect, which is exactly what the via-buffer path moved
>    (it strips the stencil aspect too, and copies raw bits); CopyD16 only
>    handles 32-bit-transfer depth backings, so no converting path is replaced.
>    24-bit depth stays excluded (padding bits are undefined in copies).
>
> 3. The color32 -> depth shader copy (BlitHelper::CopyColor32ToDepth) supports a
>    D32_SFLOAT_S8_UINT destination: one view with both aspects is bound as the
>    depth attachment (DONT_CARE / STORE, every texel written by the fullscreen
>    triangle, as for D32) and as the stencil attachment (LOAD / STORE, stencil
>    test off, so stencil bits are preserved as the via-buffer path preserves
>    them); the attachment transition includes attachment reads, and the
>    pipeline declares the stencil attachment format. Depth bits follow the same
>    gl_FragDepth path as the existing D32 case, so it is exactly as bit-exact as
>    that path. It only matters where maintenance8 does not apply (no
>    maintenance8, or non-R32 32-bit color formats such as RGBA8 views).
>
> KYTY_DIRECT_IMAGE_COPY_WIDE=0 restores the previous format lists for 2 and 3
> (the existing KYTY_DIRECT_IMAGE_COPY / _M8 / _SHADER switches still apply).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/image/blitHelper.cpp`, `src/graphics/host_gpu/renderer/image/blitHelper.h`, `src/graphics/host_gpu/renderer/image/image.cpp`.

### 64. memory: forget known fills only for the pages fault-ahead opens

Commit: [`e30c2716`](https://github.com/Jetsku/KytyPS5-experimental/commit/e30c27163e0211b5c4dd99a4369fc63a947315fd) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Review follow-up to the fault-ahead commit. InvalidateMemory forgot known
> fills over the whole aligned fault-ahead window, which also dropped fills of
> GPU-owned pages in the window (fault-ahead never touches those, and a CPU
> write to them still faults), costing DCC clear knowledge for nothing.
>
> MemoryTracker::InvalidateRegionOnWriteFault (the write-fault entry, replacing
> the write_fault flag) now hands every run of pages fault-ahead makes CPU-dirty
> to a callback, under the region lock and before the protection update makes
> them writable; BufferCache forgets known fills over exactly those runs (plus
> the faulting range, before the tracker call, as before). Ordering matches the
> faulting page: a fill is forgotten before any write to its page can land
> without a fault. Lock order region spinlock -> known-fill mutex is new but has
> no inverse (no known-fill path touches the tracker).
>
> Also: BufferCache::UploadBatch only flushes on an active scheduler.
>
> Test: TestFaultAheadWindow checks the reported runs (exactly the opened pages,
> reported while still write-protected) and that the GPU-owned flush path
> reports none.

</details>

Changed files: `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `tests/MemoryTrackerTests.cpp`.

### 65. memory: make hot pages exactly equivalent to fault tracking

Commit: [`9efc78de`](https://github.com/Jetsku/KytyPS5-experimental/commit/9efc78de0fdd5ab87b901b4fc537023a4d87e153) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Review follow-up to the hot-page commit. The hot-page shadow was erased on
> every GPU-side content write (NoteBufferContentWrite), on unbounded address
> writers and on buffer unregistration, forcing the next hot upload to copy the
> CPU bytes. That diverged from ordinary fault tracking, under which a page the
> CPU did not write since its last upload is NOT uploaded again, so GPU-side
> bytes (an image downloaded into the buffer by a texel binding, an address
> writer's stores, a join's copy) stay in the buffer. Keeping the shadow instead
> would diverge the other way when the CPU rewrites identical bytes after such a
> write.
>
> Now the invariant is: while a page is hot, its buffer bytes equal its shadow
> (the last snapshot uploaded). Every write that could break it returns the page
> to ordinary tracking first or keeps the bytes:
> - written uploads demote (unchanged), untracking demotes (unchanged);
> - joins copy the bytes (no action; unregistration no longer erases shadows);
> - image downloads into the buffer (SynchronizeBufferFromImage) and unbounded
>   address writers (InvalidateContentRevisions, all hot pages) SETTLE the hot
>   pages: MemoryTracker::SettleHotPages clears their CPU bits and
>   write-protects them under the region lock (RegionManager::SettleHot), then
>   BufferCache compares each page with its shadow and marks it CPU-dirty again
>   if it differs (or had no shadow). A write before the protection is caught by
>   the compare, a write after it faults, so the page ends in exactly the state
>   fault tracking would have: clean if the CPU did not write since its last
>   upload, dirty otherwise.
> Under the invariant, skipping an unchanged hot page is identical to uploading
> it (same bytes), and a CPU rewrite of identical bytes is a no-op either way.
> This also makes the small-read stream path (guest bytes for CPU-dirty ranges)
> exact for hot pages: a hot page's buffer bytes always equal its CPU bytes as of
> its last upload.
>
> Test: TestHotPageSettle (ranged and global settle, clean and protected
> results, re-dirtying, empty settle).

</details>

Changed files: `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `tests/MemoryTrackerTests.cpp`.

### 66. buffers: batch the uploads of a BDA synchronization pass

Commit: [`94c7990e`](https://github.com/Jetsku/KytyPS5-experimental/commit/94c7990ef2ee12faf9f5f956dfcf87d6135485aa) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> SynchronizeBdaBuffers (draws/dispatches using DMA/BDA) uploads the CPU-dirty
> pages of every cached buffer in the mapped ranges; each upload flushed its own
> barrier pair. The pass now runs inside a BufferCache::UploadBatch scope, so all
> of its uploads are recorded behind one barrier pair (KYTY_UPLOAD_BATCH).
> Correct for the same reasons as the binding scopes: nothing but uploads is
> recorded during the scan (read-only, non-texel synchronization), overlapping
> requests into one buffer start a new batch, and the scope ends with a flush.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`.

### 67. buffers: one global dependency for upload batches with many destinations

Commit: [`02857e81`](https://github.com/Jetsku/KytyPS5-experimental/commit/02857e81fccead03704f6bca950a2a259b5e0fa6) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A batch of queued uploads into more than 8 distinct buffers (typically a BDA
> synchronization pass) records its pre-copy dependency as one global memory
> barrier and adds its post-copy dependency to the pending batch's memory
> barrier, instead of one buffer barrier per destination. Same stage and access
> scopes, applied to all memory: at least as strong as the buffer barriers, one
> barrier structure for the driver. Up to 8 destinations keep buffer barriers.

</details>

Changed files: `src/graphics/host_gpu/renderer/context.cpp`.

### 68. CP: detail-only packet zones, cheaper HLE register calls, raised command-processor priority

Commit: [`df41efe7`](https://github.com/Jetsku/KytyPS5-experimental/commit/df41efe711dfeb1aa107b501bfbbed431768952e) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - The 50 PM4 handler zones were always active (profiler_enabled=true in comparison runs) and ran per packet (~25-40k per frame); they are now KYTY_PROFILER_DETAIL_FUNCTION.
>
> - Agc HLE log counters did a locked fetch_add on a shared line on every call (AgcGetDataPacketPayloadAddress ~76k/s, AgcCbSetShRegisterRangeDirect ~75k/s); they now check with a relaxed load first (same log limits). AgcCbSetShRegistersDirect copies short register lists on the stack instead of allocating a vector per call.
>
> - Thread_Gpu raises its priority to above normal and opts out of power throttling (KYTY_CP_PRIORITY: 0 off, 1 above normal (default), 2 highest). Guest threads spin heavily (~67k yields and ~67k usleeps per frame).

</details>

Changed files: `src/common/threads.cpp`, `src/common/threads.h`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/libs/agc.cpp`.

### 69. Textures: binding identity memo for ResolveTexture and sampled views

Commit: [`4777f7c6`](https://github.com/Jetsku/KytyPS5-experimental/commit/4777f7c62416c99a3d1b085ff6b7694ab8357205) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every bound texture of every draw (~46k per flip) re-ran the full
> resolution: description cache probe and 0.5 KB copy, ValidateImageDesc,
> FindImage (first-page owner scan with SameBacking, SyncAliasFromOwner's
> region walk, LRU touch), post-lookup validation, and in RebindImages
> FindTexture (lock, RefreshImage checks, linear FindView scan).
>
> New module pipeline/textureBindingMemo.{h,cpp} (4096-slot direct-mapped
> table owned by RenderExecutor) memoizes, per exact key (8 T# dwords plus
> every ImageResource field the resolution reads: the texture description
> key), the final description, the image FindImage returned and the view
> FindTexture returned.
>
> Correctness (exact results):
> - FindImage answer. Only first-page answers are recorded: after the full
>   resolution, under m_lock, FindImageWithSameBacking(final description,
>   exact_format) must return the same image, FindImage must not have
>   rebased the view (mip/slice answers of ResolveOverlap are never
>   recorded) and the lookup mode must be the default first-page one. That
>   lookup reads only the first page's owner list, the registered flag and
>   SameBacking fields (address, size, extent, resources, samples, block
>   size, tile mode, format, type), which are fixed for an image's
>   lifetime (info.metadata/stencil, the only fields mutated in place, are
>   not read). Owner lists and registered flags change only in
>   RegisterImage/UnregisterImage, which now bump
>   TextureCache::m_binding_generation; every creation, free, expansion,
>   overlap/depth recreation, alias replacement, GC and pressure
>   retirement, unmap and stencil-association insert goes through them.
>   AssociateStencil also bumps when it changes an image's depth_id. An
>   entry is used only while the generation equals the recorded one, so
>   FindImage would return the recorded image, with the recorded
>   description, again.
> - FindImage side effects. DCC descriptions are never recorded
>   (MaterializeDccClear inspects guest metadata on every lookup).
>   SyncAliasFromOwner must be a no-op: the image is its alias owner
>   (checked live) or has a stencil plane (live), or no other registered
>   image has the same backing range, extent and sample count (checked
>   when recording over the first-page owners, a superset of its copy
>   sources that only registration can change). tick_accessed_last and the
>   LRU/frame touch are done exactly as FindImage does; later touches of
>   the draw (BindImage, RebindImages, CommitBindings) are unchanged, so
>   the LRU order is identical. A stencil association (depth_id) or a
>   pending rebind (both live) take the full path. The post-lookup
>   validations are pure functions of the key and the image's fixed
>   format, so they passed when recording.
> - FindTexture (sampled bindings only; storage bindings keep the full
>   path because binding marks the image GPU-written). The fast path runs
>   under m_lock and requires what makes RefreshImage a no-op: not
>   CPU-dirty, maybe-dirty or buffer-modified, whole range write-watched
>   (TrackImage no-op); plus no stencil plane to refresh and FindTexture's
>   rediscovery conditions (registered, no depth_id, no pending rebind).
>   Views are never destroyed or replaced while their image lives and
>   FindView returns the first matching view, so the recorded view of the
>   same image id (slot generation included) is FindView's answer. It
>   performs FindTexture's TouchImage.
> - Null descriptors resolve to the permanent null images (never
>   registered or freed); their entries skip the generation.
> - A binding remembers the entry tag its description came from; a hit on
>   the same entry skips the 0.5 KB description copy (tags are unique per
>   recording, and the full path clears the tag before changing desc).
>
> The description cache now shares the memo's hash (same formula as before).
>
> Env: KYTY_TEXTURE_BINDING_MEMO=0 restores the full resolution for every
> binding (no entries are recorded; the generation counter still runs).
>
> Counters (FrameEvent.*): TextureBindingMemoHits/Misses/Stale/Fills/
> Rejects, TextureViewMemoHits/Misses, TextureBindingDescCopiesAvoided,
> TextureCacheStructureChanges.
>
> Merge note for texture-cache work: a new per-image refresh trigger must
> be reflected in TextureBindingMemo::RefreshIsNoOp, and changing a
> registered image's SameBacking fields in place must call
> NoteStructureChange().

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.h`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.cpp`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.h`, `src/graphics/host_gpu/renderer/render.h`.

### 70. Descriptors: intern renderer pipeline layouts by binding signature

Commit: [`5aa47e88`](https://github.com/Jetsku/KytyPS5-experimental/commit/5aa47e886c8f50f9e8a49a486d6c161843ecf234) · **Native state and descriptor reuse**

Avoids repeated native state construction, hashing or binding when identity and contents permit reuse. Diagnostic audits measure opportunity, not achieved frame-time savings.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every graphics and compute pipeline created its own descriptor-set
> layout and pipeline layout (shaders.cpp CreateDescriptorLayout and the
> two vkCreatePipelineLayout blocks). The push-descriptor shadow
> (CommandBuffer::PushDescriptors) compares layout handles, so it could
> never match across the ~1,000 pipeline switches per frame, and the
> descriptor heap keeps one 32-set batch per set layout in a 1024-set
> pool, which ~32 distinct layouts exhaust.
>
> New module pipeline/pipelineLayoutCache.{h,cpp}: AcquirePipelineLayout
> returns one VkDescriptorSetLayout/VkPipelineLayout pair per signature =
> set-0 bindings (binding number, descriptor type, count, stage flags;
> sorted by binding number) + push-descriptor flag (same rule as before:
> total descriptor count <= maxPushDescriptors) + the single push-constant
> range {stages, 0, NativePushConstantSize}. The registry is keyed by
> VkDevice and guarded by a mutex (pipelines may be created on several
> threads). CreatePipelineInternal (graphics and compute) now calls it;
> ~PipelineCache releases per-pipeline layouts only when not interned and
> then destroys the interned set, after all pipelines are destroyed.
>
> Correctness: pipeline layouts created from equal parameters are
> identically defined; binding order in pBindings carries no meaning
> (bindings are identified by number), so the shared set layout is the
> same interface every shader was compiled against. Pipeline layouts with
> identical set layouts and push-constant ranges are compatible for set 0
> and for push constants, so descriptors and constants recorded with the
> shared handle are valid for every pipeline that uses it. Only handles
> change; no descriptor content, binding number or stage changes.
>
> Also declares the descriptor-commit FrameEvent counters used by this and
> the following descriptor commits (layouts, push constants, push misses
> by kind, descriptor set reuse, upload fast hits).
>
> Env: KYTY_LAYOUT_INTERN=0 gives every pipeline its own layouts again.
> Counters: FrameEvent.PipelineLayoutsCreated / PipelineLayoutsShared.
>
> Tests: new binding_path_tests target (tests/BindingPathTests.cpp) checks the
> signature rules (order independence, every field significant, the
> maxPushDescriptors threshold) without a Vulkan device.
>
> Merge note for pipeline-creation work: layouts from
> AcquirePipelineLayout are shared; release them only through
> ReleasePipelineLayout (never vkDestroyPipelineLayout directly), e.g.
> when discarding a duplicate pipeline compiled concurrently.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineLayoutCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineLayoutCache.h`, `src/graphics/host_gpu/renderer/pipeline/shaders.cpp`, `tests/BindingPathTests.cpp`.

### 71. Descriptors: dedup uploads, shadow push constants, reuse descriptor sets

Commit: [`d9867c89`](https://github.com/Jetsku/KytyPS5-experimental/commit/d9867c8963263d517c22166df370cc4d7cac445a) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Makes identical descriptor state produce no work at commit time, and
> counts why push-descriptor updates still happen.
>
> 1. Per-draw shader-data / flattened-SRT uploads (UploadShaderData,
>    KYTY_UPLOAD_DEDUP, default on). Identical bytes uploaded earlier in
>    the same scheduler tick reuse that stream allocation, so equal
>    contents give equal descriptors. A 256-slot hashed table replaces the
>    64-slot one, and each upload site (stage type x table kind) first
>    checks the entry it filled or matched last, without hashing.
>    Correct: the stream ring never overwrites a range during the tick
>    that allocated it (a wrap reaching it waits for that tick, which
>    submits it and advances CurrentTick()), the tables are read-only, and
>    the entry's tick is taken after the upload (an upload that wraps
>    belongs to the new tick). A tick is one command buffer, the unit of
>    push-descriptor state. KYTY_UPLOAD_DEDUP=0 restores the previous memo
>    (64 slots, only with KYTY_RENDERER_BATCH=1).
>
> 2. Push constants (CommandBuffer::PushConstants,
>    KYTY_PUSH_CONSTANT_SHADOW, default on). CommitBindings skips
>    vkCmdPushConstants when the last update recorded in this command
>    buffer had the same layout, stages, size and bytes. Correct: push
>    constant values persist until updated and are not disturbed by
>    pipeline binds; values set with the very layout of the new pipeline
>    are valid for it. The shadow is forgotten at Begin() (values are
>    undefined in a new command buffer), in Handle() (every other recorder
>    of vkCmdPushConstants - tiler, DCC, blit, occlusion reductions - gets
>    its native handle there, before recording), and after committing a
>    mesh pipeline (the mesh draw then pushes its draw arguments over the
>    first dwords through StateHandle()).
>
> 3. Descriptor sets for layouts beyond maxPushDescriptors (CommitDescriptorSet,
>    CommandBuffer::BindDescriptorSet, pipeline/descriptorSetReuse.{h,cpp},
>    KYTY_DESCRIPTOR_SET_REUSE, default on). Instead of a fresh heap set,
>    vkUpdateDescriptorSets and vkCmdBindDescriptorSets per commit: a set
>    written earlier in the same command buffer (tick) with the same set
>    layout and exactly the same writes (binding, element, type, count and
>    every buffer/image/sampler info including image layouts) is reused,
>    and the bind is skipped while it is still bound at that bind point.
>    Correct: every object a descriptor references (image views, cache
>    buffers, stream buffer, samplers) is destroyed only by deferred
>    operations after the tick that could use it completes, so during a
>    tick equal handles denote the same live objects; the heap resets a
>    pool only after the tick that retired it completes, and a set handed
>    out during tick T is from a pool retired at T or later; a reused set
>    is never updated again; stream ranges are not overwritten within
>    their tick. Bound sets are disturbed only by binds or push-descriptor
>    updates of set 0, which all go through CommandBuffer (push updates
>    clear the bound-set shadow, Begin() and InvalidateDescriptors reset
>    it), and pipeline binds never disturb sets (layouts are interned, so
>    the bind layout equals the pipeline's).
>
> 4. Diagnostics: CommandBuffer::PushDescriptors now returns why an update
>    was recorded (no comparable state, different binding list, or the
>    index of the first differing write) and CommitBindings classifies
>    the renderer's recorded pushes by the kind of that descriptor. The
>    comparison itself is unchanged (still gated by KYTY_RENDERER_BATCH=1)
>    except that a push after a set bind never matches.
>
> Counters: FrameEvent.ShaderUploadReuseHits/Misses/BytesAvoided,
> ShaderUploadLastHits; PushConstantUpdates/PushConstantUpdatesAvoided;
> DescriptorPushes, DescriptorPushesAvoided, DescriptorPushMissLayout/
> Shape/Image/Sampler/Buffer/Upload/Other; DescriptorSetsWritten/Reused,
> DescriptorSetBindsAvoided.
>
> Tests: binding_path_tests covers DescriptorSetReuse (tick, layout and
> every descriptor field significant; entries own copies; unsupported
> writes never cached).
>
> Not done: passing the per-draw tables by buffer device address in push
> constants (optional item) needs recompiler changes to the memory and
> image emitters and the push-data layout, which another branch is
> editing.

</details>

Changed files: `CMakeLists.txt`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptorSetReuse.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptorSetReuse.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/render.h`, `tests/BindingPathTests.cpp`.

### 72. Texture streaming: chunk-granular write tracking and partial refreshes

Commit: [`7e688d30`](https://github.com/Jetsku/KytyPS5-experimental/commit/7e688d306e239eeb08adfb3476ec56199215e0aa) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot's texture streamer rewrites slots of a pooled texture region every
> few seconds. Streamed textures overlap each other (reversed mip chains whose
> unresident levels extend over neighbouring slots), so every slot write dirtied
> dozens of 2.8-22 MB images and each was refreshed whole: first fault untracked
> the whole image and the refresh re-read, detiled and copied the full chain.
>
> - Image write tracking at chunk granularity (64 KiB, KYTY_TEXTURE_DIRTY_CHUNK_KB)
>   for page-aligned color images: a write fault releases and dirties only its
>   chunk; other chunks stay protected, so a refresh knows every byte that can
>   have changed. Images rewritten whole on consecutive cycles release all
>   chunks on the first write (one fault per cycle), re-probing every 8 cycles.
> - Partial refresh: while an image's native contents are known to equal guest
>   memory outside its dirty chunks (set only by a guest-sourced refresh,
>   cleared by any native write, GPU/buffer ownership or full untrack), refresh
>   only the mip levels whose tiled ranges overlap dirty chunks, and within
>   non-tail levels of 2D block families only the dirty rows of tile blocks.
>   Only those tiled ranges are read from guest memory (or the existing cache
>   buffer / GPU-written bytes are used as before).
>   KYTY_TEXTURE_PARTIAL_UPLOAD=0 restores whole-image tracking and refreshes;
>   KYTY_TEXTURE_PARTIAL_BANDS=0 refreshes whole levels;
>   KYTY_TEXTURE_PARTIAL_VERIFY=1 hashes clean chunks before each partial
>   refresh and falls back to a full one on any unrecorded change.
> - TileManager reuses completed scratch buffers by size class instead of a
>   vmaCreateBuffer/destroy per detile (KYTY_TILER_SCRATCH_POOL=0, _MB=256).
> - Native image pool on by default and sized for streaming churn: 1 GiB
>   (at most budget/8) and 1024 images (KYTY_NATIVE_IMAGE_POOL=0/_MB/_COUNT).
> - Retiring a color image no longer scans every image for stencil
>   associations (only depth images can have them).
> - Opt-in KYTY_TEXTURE_OVERLAP_KEEP_FRAMES=N keeps clean texture-only images
>   that a newer overlapping image would retire as stale.
> - Counters: FrameEvent.TexturePartial*, TextureChunkInvalidations,
>   TextureOverlapKeeps, TilerScratchPool*, FrameWait.TextureUpload.
>   transfers.csv reports partial refreshes as reason cpu-write-partial with
>   bytes = refreshed tiled bytes and span_bytes = dirty chunk bytes.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/image.h`, `src/graphics/host_gpu/renderer/image/tiler.cpp`, `src/graphics/host_gpu/renderer/image/tiler.h`, `src/graphics/host_gpu/vma.cpp`.

### 73. Texture streaming: async staging copies, fault fast path, texel sync skip

Commit: [`78935a08`](https://github.com/Jetsku/KytyPS5-experimental/commit/78935a08fb0a1fbf565dab2506234dbd8a10db2e) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Take the guest->staging memcpy of texture refreshes off the GPU thread and
> remove other per-refresh CPU costs found by the upload audit.
>
> - StagingCopier (KYTY_TEXTURE_ASYNC_STAGING=0 disables): sampled tiled
>   texture refreshes from plain guest memory (full and partial) reserve
>   staging, record the detile and copy immediately, and hand the memcpy to a
>   worker thread. Each job signals a host timeline semaphore; the guest
>   scheduler's next Submit waits for the latest pending value on the GPU
>   (new CommandScheduler::SubmitDependency hook, compute/transfer stages), so
>   neither the recording thread nor the submission path waits. The detile
>   barrier already includes host writes. Source pages were re-protected
>   before the job was queued: a racing guest write faults and dirties its
>   chunk, so the next refresh replaces whatever the copy saw.
> - With resizable BAR (a device-local host-visible heap >= 2 GiB) the worker
>   writes into a dedicated 256 MiB device-local ring so detile reads VRAM
>   (KYTY_TEXTURE_STAGING_REBAR=0, KYTY_TEXTURE_STAGING_MB).
> - Guest write faults skip the texture-cache lock when no registered image
>   covers the faulting 1 MiB page (per-page atomic counts maintained at
>   register/unregister; KYTY_TEXTURE_FAULT_FAST_PATH=0).
> - SynchronizeBufferFromImage skips the image download for texel-buffer reads
>   when the image's content serial and the buffer's content revision are
>   unchanged since the previous download (KYTY_TEXEL_SYNC_SKIP=0).
> - Detile no longer clears its linear scratch: copies and conversions read
>   only the elements the dispatches write (KYTY_TILER_CLEAR_SCRATCH=1).
> - Counters: FrameEvent.TextureAsyncCopies/Bytes, TextureUploadBytes{Buffer,
>   Staging,Async} (refresh bytes by source route), TextureInvalidateSkips,
>   TexelImageSync{Downloads,Skips}; FrameWait.TextureStagingCopy (worker).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.h`, `src/graphics/host_gpu/renderer/image/image.h`, `src/graphics/host_gpu/renderer/image/stagingCopier.cpp`, `src/graphics/host_gpu/renderer/image/stagingCopier.h`, `src/graphics/host_gpu/renderer/image/tiler.cpp`, `src/graphics/host_gpu/renderer/image/tiler.h`.

### 74. Texture streaming: O(1) re-track check on binds of chunk-tracked images

Commit: [`4d191252`](https://github.com/Jetsku/KytyPS5-experimental/commit/4d191252ca769388ae70172440025b3c164e554d) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> TrackImage runs on every texture bind (RefreshImage). Keep a count of
> released chunks so fully watched images skip the per-chunk scan.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/image/image.h`.

### 75. Merge texture streaming: chunk tracking, partial refreshes, async staging, larger image pool (claude/texture-streaming)

Commit: [`dbbd5055`](https://github.com/Jetsku/KytyPS5-experimental/commit/dbbd505575d5a79703e83680002563785a0758ff) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> # Conflicts:
> #	src/common/profiler.cpp
> #	src/common/profiler.h

</details>

### 76. CP: bound the delay of batched end-of-pipe interrupts

Commit: [`e38a39dc`](https://github.com/Jetsku/KytyPS5-experimental/commit/e38a39dc695eace2ebab6f448da38ba210c9069e) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Count-based batching alone could hold an interrupt raised early in a long slice until the slice ends (~100 ms of CP time per frame's DCB). After the first deferred interrupt, the command buffer is now flushed at the latest after KYTY_EOP_FLUSH_PACKETS further packets (default 256).

</details>

Changed files: `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`.

### 77. Vulkan validation: log mode and optional synchronization validation

Commit: [`ab19fab2`](https://github.com/Jetsku/KytyPS5-experimental/commit/ab19fab2af04f30e30854e84b3860574cdd0ffc9) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The built-in validation mode exits on the first validation error, which ends a diagnostic run at the first known issue. KYTY_VULKAN_VALIDATION_MODE=log records errors and warnings per message id in a file (KYTY_VULKAN_VALIDATION_LOG; first five occurrences in full, then counts at powers of two) and keeps running. KYTY_VULKAN_SYNC_VALIDATION=1 adds synchronization validation, which checks the barrier batching/sinking and the new copy paths.

</details>

Changed files: `src/graphics/presentation/window/vulkanWindow.cpp`.

### 78. LOD stats: order the counter reset after the report copy

Commit: [`dd2c02f1`](https://github.com/Jetsku/KytyPS5-experimental/commit/dd2c02f1885624bb871943dd38c5e9da9bbb4b96) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Synchronization validation (U38 correctness run) reported SYNC-HAZARD-WRITE-AFTER-READ: vkCmdFillBuffer reset the 257 mip-statistics counters (1,028 bytes) that the preceding vkCmdCopyBuffer reads for the report, with no dependency between them, so a report could occasionally capture partly reset counters. A transfer-to-transfer barrier now orders the fills after the copy. The command-buffer handle is also re-taken after the publish-slot wait, which could in principle finish the recording.

</details>

Changed files: `src/graphics/host_gpu/renderer/lodStats.cpp`.

### 79. Vulkan validation log: deduplicate by message id and call site

Commit: [`7d3b5eec`](https://github.com/Jetsku/KytyPS5-experimental/commit/7d3b5eecf404a0259ae0f55bab910e8c1232748a) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The validation layer suppresses a message id after 10 repeats by default, so one hazard could hide a second one with the same id (U38: the LOD-stats write-after-read filled the SYNC-HAZARD-WRITE-AFTER-READ quota). Validation runs now disable the layer's message limit (Start-Comparison sets VK_KHRONOS_VALIDATION_ENABLE_MESSAGE_LIMIT=false) and the log keys each message on its id plus its first line with handles and numbers blanked.

</details>

Changed files: `src/graphics/presentation/window/vulkanWindow.cpp`.

### 80. Texture residency: register, track and upload only sampleable mip levels

Commit: [`aa8eac1e`](https://github.com/Jetsku/KytyPS5-experimental/commit/aa8eac1eccce1a2d08e59985d09bfade2c5f6fd2) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot streams textures into a pool of 128 KiB slots and clamps each T# with
> MIN_LOD so only the levels present in the slot are sampled. PS5 stores a mip
> chain smallest level first, so a 2048^2-4096^2 texture in a slot still claims
> 2.8-22 MB: its non-resident fine levels lie over neighbouring slots. The cache
> registered, watched, overlap-tested and uploaded that whole claim, so every
> slot write dirtied dozens of neighbours and every new slot texture retired
> them as overlap-stale and uploaded a full chain.
>
> A sampled view reads no level finer than floor(view MIN_LOD) (and none below
> BASE_LEVEL); Kyty already applies that clamp through
> VK_EXT_image_view_min_lod. The host image keeps its full level count (keys,
> view level indexing and LOD computation stay as they are), but the cache now
> treats only the prefix of the guest range that holds the sampleable levels
> (Image::live, levels >= resident_first) as the image:
>
> - FindImage derives the finest requested level from the view (base_level +
>   floor(min_lod)); a new image registers, watches and overlap-tests only the
>   resident prefix, computed from the same tiled layout the refresh detiles.
>   Images that overlap only non-resident levels are neither merge candidates
>   nor retired by it.
> - Refreshes (full, chunk-partial, async-staged) read and detile only resident
>   levels; non-resident levels stay undefined.
> - A finer MIN_LOD later extends the image in place (unregister/register of the
>   larger prefix, then a refresh of all resident levels, from GPU-written
>   buffer bytes when present). FindTexture re-checks, so binding caches that
>   skip FindImage stay exact. Residency only grows.
> - Any non-sampling use makes the image fully resident and refreshes it before
>   the use is recorded: storage/render/depth/video-out lookups, copy sources
>   and destinations, alias sync, clears, external writers (MarkGpuWritten) and
>   readers (UpdateImage). Downloads refuse partially resident images; DCC,
>   depth, stencil, arrays, volumes and MSAA are never partially resident.
>   MarkImageGpuModified reports any GPU write that would still reach one.
> - Unmapping any byte of a partially resident image retires it.
> - Partially resident images no longer get retired by overlap, so ones unused
>   for 30 presented frames are retired explicitly (their native chains are
>   full size); a later use recreates them and uploads only resident levels.
>
> Switches: KYTY_TEXTURE_RESIDENT_MIPS=0 restores whole-chain behaviour;
> =poison fills non-resident levels with a magenta/green marker to verify on
> screen that they are never sampled; KYTY_TEXTURE_RESIDENT_IDLE_FRAMES (30,
> 0 = never).
> Counters: FrameEvent.TextureResidentImages, TextureResidentLevelsSkipped,
> TextureResidentBytesSkipped, TextureResidencyExtensions,
> TextureResidencyFullFallbacks, TextureResidencyUnmapFrees,
> TextureResidencyViolations (expected 0), TextureResidentIdleFrees.
> transfers.csv: detail texture-resident (bytes = resident bytes), reason
> resident-extend; images.csv: free reason resident-idle.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/image.h`.

### 81. Merge resident-mip texture registration (claude/texture-residency)

Commit: [`2216c16e`](https://github.com/Jetsku/KytyPS5-experimental/commit/2216c16ebb8fbfc3af9ccf272f79c9fb3ffb733d) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 82. Merge codex/astro-profile (texture streaming, resident mip levels) into claude/binding-path

Commit: [`6a65beff`](https://github.com/Jetsku/KytyPS5-experimental/commit/6a65beffa6804740b060eb046b101695f253f2fd) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Conflicts: profiler FrameEvent lists (both sides' counters kept, upstream
> first), textureCache.h members/forward declarations (both kept), and
> RegisterImage (upstream registers image.live; the binding-memo
> generation bump stays in RegisterImage/UnregisterImage).
>
> Semantic resolution for the texture binding memo (textureBindingMemo.*):
> - Resident mip levels: FindImage and FindTexture now call
>   EnsureResidency(RequestedFirstLevel(desc, levels)). The memo records
>   that first level per entry and answers only while the image's
>   resident_first is at most it (live check in both the ResolveTexture
>   and the view fast path), so a skipped EnsureResidency is a no-op; any
>   extension goes through UnregisterImage/RegisterImage and bumps the
>   generation as well. Storage descriptions request level 0, so they are
>   answered only for fully resident images.
> - Chunk tracking: RefreshIsNoOp now mirrors the new TrackImage: the
>   image must be watched (IsTracked) and either chunk-tracked with no
>   released chunk or watching exactly its resident range (live), besides
>   not CPU-dirty/maybe-dirty/buffer-modified (chunk writes and residency
>   extensions mark the image CPU-dirty).
> - The guest write-fault fast path skips the texture-cache lock only on
>   pages with no registered image, so it never changes an image the memo
>   answers for; the memo reads image state under the lock.

</details>

### 83. Textures: per-page structure versions for the binding memo

Commit: [`3e5789f9`](https://github.com/Jetsku/KytyPS5-experimental/commit/3e5789f9f127f992b24429155380ae0a3a44a6d1) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The binding memo was validated by one texture-cache-wide generation, so
> every registration anywhere (image creation/free, residency extension,
> idle retirement, alias replacement) made every entry stale. FindImage's
> first-page answer, the only answer the memo records, depends on the
> owner list of the description's first 1 MiB ImagePageTable page alone:
> its owners' registered flags and SameBacking fields, and (for the alias
> partner superset) images with the same start address, i.e. owners of
> that page.
>
> TextureCache::NoteStructureChange(image) now bumps a version for every
> page the image's registered range (live) covers; RegisterImage,
> UnregisterImage and AssociateStencil (depth_id change) call it, as
> before. The memo records the first page and its version and uses an
> entry only while that version is unchanged. Exact: any change of that
> page's owner list or of an owner's registered flag goes through
> Register/UnregisterImage of an image covering the page, which bumps it;
> the found image's own re-registration (residency) covers its first page;
> stencil associations, residency levels, pending rebinds and alias
> ownership stay live checks. Versions are 64-bit (no wraparound) and
> allocated on the first structure change (8 MiB for the 40-bit space).
>
> Env and counters unchanged (KYTY_TEXTURE_BINDING_MEMO,
> FrameEvent.TextureCacheStructureChanges counts version bumps).

</details>

Changed files: `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.cpp`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.h`.

### 84. Profiler: attribute CP-thread GPU waits to their caller (FrameWait.GpuWait*)

Commit: [`0c453856`](https://github.com/Jetsku/KytyPS5-experimental/commit/0c453856bc8c1aa91440f23ceee4ff4acaecfcc9) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> MasterSemaphore::Wait now records a FrameWait scope when it actually blocks on the guest GPU
> (CP) thread. The category comes from a per-thread Profiler::ScopedGpuWaitReason set by the
> caller: ReadMemory drains (GpuWaitDrain), occlusion publication waits (sync proxy, predication
> on unpublished dumps, publish-slot reuse: GpuWaitOcclusion), stream-buffer wraps, LOD-stats
> slot reuse, unmaps, boolean predication waits, GDS end-of-pipe reads, and GpuWaitOther for
> everything untagged. GpuWaitSideCopy is reserved for side-copy waits of GPU-thread reads.
>
> Waits that find the tick already complete, and waits on other threads (completion runner,
> guest threads), are not counted. Aggregate diagnostics only (KYTY_PROFILE_AGGREGATES=1); no
> behaviour change: the early IsFree return is the same test the old code reached after the
> dispatch wait (KnownGpuTick already includes the dispatched tick).
>
> Enum values are appended to FrameWait; name arrays are updated in the same order.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/streamBuffer.cpp`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/masterSemaphore.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`.

### 85. Shader: hash headerless shader code from the clean backing (CP fault at XXH3_64bits)

Commit: [`c64b5ae0`](https://github.com/Jetsku/KytyPS5-experimental/commit/c64b5ae06da086727d628ef2b77a0b1ee14be8cc) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> u37 Sky Garden readbacks.csv attributes ~7.8k CP-thread "fault-read" drains (about 33 ms per
> flip, host pc 0x1400f0a29) to XXH3_hashLong_64b_default, called only through XXH3_64bits. Of
> the XXH3_64bits call sites in the u37 binary, the one that reads guest memory on the GPU thread
> is GetShaderParams (shader.cpp): shaders without the 0xBEEB03FF header have no declared hash, so
> their code is hashed in place on every draw, per stage. The code shares 4 KiB tracker pages with
> GPU-written storage data (last writer: shader-storage, 16-byte writes), so each hash faults and
> drains the whole GPU backlog although the code bytes themselves are CPU-owned.
>
> HashShaderCode first copies the code with TryReadGpuCleanBacking, which proves the exact bytes
> are not GPU-dirty, not pending a backing publication and not a GPU-modified image, and reads the
> backing without touching the protected mapping. Only if that proof fails (or on a non-GPU thread
> outside a draw-prep fork) is the code hashed in place, keeping the old fault/readback path.
>
> Correctness: the hash input is byte-identical to the in-place read whenever the proof holds,
> because the backing is the guest memory those bytes live in and no GPU write to them is
> outstanding. The span handed to the recompiler is unchanged.
>
> Switch: KYTY_SHADER_HASH_BACKING=0 restores in-place hashing.
> Counters: FrameEvent.ShaderCodeHashBacking / ShaderCodeHashDirect.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/shader/shader.cpp`.

### 86. BufferCache: serve GPU-thread reads with side copies instead of full drains (F1a)

Commit: [`a8581856`](https://github.com/Jetsku/KytyPS5-experimental/commit/a858185601fa093c9545c8578a08f6f6fe01a4e1) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> GPU-thread reads of GPU-owned bytes (CP reads through SynchronizeGpuBackingForRead such as
> inherited indirect instance counts and indirect arguments, and GPU-thread page faults) always
> took ReadMemoryDrain: record a download into the current command buffer, submit it, and wait
> for CurrentTick, i.e. the whole queued backlog plus the current recording, then begin a new
> command buffer. u37 Sky Garden: ~19.6 ms/flip in gpu-sync with producer ages of ~24 ms.
>
> With KYTY_READBACK_SIDE_GPU_THREAD (default on) a GPU-thread read first tries the existing
> side-copy path (TryIssueSideReadback): when every dirty byte of the window was written by an
> already submitted recording (m_write_ticks, no unbounded writer in the current recording, no
> pending backing publication), the copy goes to a separate command buffer that waits for the
> producer's timeline value, and the GPU thread waits for that copy and publishes it itself.
> The current recording is not submitted or split. Otherwise (current-recording writer,
> unbounded writer, pending publication, no free slot) it drains exactly as before.
>
> Correctness: this is the same issue/complete sequence guest-thread side readbacks already use.
> The GPU thread records nothing between issue and completion, so no newer writer can re-own
> the pages meanwhile; all copied bytes are published and the pages unprotected before the
> caller retries its read. On a still-shared queue the copy additionally follows all earlier
> submissions, so it observes everything the drain would have except the current recording,
> which by construction did not write the copied bytes. A GPU-thread read that overlaps a pending
> guest side copy completes that copy and then continues (instead of returning) so bytes
> re-dirtied after it was issued are still read back.
>
> Switch: KYTY_READBACK_SIDE_GPU_THREAD=0 (or KYTY_READBACK_SIDE_COPY=0) restores the drain.
> Counters: FrameEvent.ReadbackGpuThreadSideCopies / ReadbackGpuThreadDrains (the fallback
> reasons are also counted in ReadbackSideFallback*), FrameWait.GpuWaitSideCopy.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`.

### 87. Vulkan: run side-copy readbacks on a second queue of the universal family (F1b)

Commit: [`1f2017fb`](https://github.com/Jetsku/KytyPS5-experimental/commit/1f2017fb35c541c3aee419221c8697abf07b39b1) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Everything (guest graphics/compute, readback copies, presentation) used one VkQueue. A side
> copy waited on its producer's timeline value, but its barrier (and the queue's in-order
> execution) put it behind every earlier submission, i.e. behind the ~18 ms backlog measured in
> Sky Garden, even when the producer finished long before.
>
> The device now creates two queues of the selected family when it has them (NVIDIA family 0 has
> 16) and side copies (guest-thread faults and, since F1a, GPU-thread reads) are submitted to
> queue 1 under their own mutex. The copy then starts as soon as its producer completes.
>
> Correctness / sharing rules:
> - Buffers are EXCLUSIVE to the queue family, not to a queue; both queues are in the same family,
>   so no ownership transfer is needed. The side command pool is created for that family.
> - The only ordering is the timeline wait: producer = newest m_write_ticks writer of the copied
>   dirty bytes (unchanged), now also max'ed with the newest unbounded (address-writing) recording,
>   whose writes carry no writer tick. Both are older than the current recording (the side path
>   already refuses current-recording and current-unbounded writers), hence submitted. The
>   semaphore signal/wait pair is a full memory dependency, so producer writes are visible.
> - The broker is drained (under queue_mutex) only when the producer has not been dispatched yet,
>   so a wait is never submitted ahead of its signal.
> - Write-after-read with later queue-0 work is not ordered. A later writer of copied bytes adds
>   them to the GPU-dirty set again and keeps their pages protected (UnmarkReadbackPending retains
>   re-marked pages), so a possibly torn published value is never read before a newer readback
>   replaces it; clean-backing reads reject dirty bytes. Buffer deletion, downloads and backing
>   reads of the pages already complete overlapping side copies first (host wait).
> - Queue 0 submission/presentation is unchanged; vkDeviceWaitIdle at teardown covers both queues.
>
> Switch: KYTY_SIDE_QUEUE=0 keeps the single queue (logged at device creation).
> Counter: FrameEvent.ReadbackSideQueueCopies.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/graphicContext.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`.

### 88. Occlusion/CP: replace the visibility-proxy GPU wait with label deferral (F2)

Commit: [`3da72010`](https://github.com/Jetsku/KytyPS5-experimental/commit/3da7201010a84b9db38c4c084fd4254d6f750d0f) · **Occlusion and result publication**

Makes visibility results usable with the required publication order, or reduces query/reduction waits. Only guest decisions using the result can reduce draws; an isolated culling gain is unproven.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The occlusion "sync proxy" (default since 2209c91e) made the CP BufferWait() -> Finish() after
> every depth-only visibility-proxy end dump (~5 per flip), draining the whole ~18 ms backlog,
> because Astro Bot reads the proxy result right after the end-of-pipe label that follows the
> dump, and Kyty writes labels when the packet is recorded.
>
> KYTY_OCCLUSION_PROXY_MODE=defer-label (new default) instead arms the CP after a proxy end dump;
> the next end-of-pipe label write (RELEASE_MEM / EVENT_WRITE_EOP data write, 32 or 64 bit) is
> not written at record time but deferred to its tick's completion, and the CP continues.
>
> Why ordering is preserved (the water must keep rendering):
> - In this mode every occlusion publication is a DeferPriorityOperation (it only reads the
>   host-visible slot and writes guest memory with TryWriteBacking, which is safe off the GPU
>   thread; LodStats and download publications already do the same).
> - The deferred label is a DeferPriorityOperation registered after the dump's publication, for a
>   tick >= the dump's tick. The priority runner is a single FIFO thread that waits for each
>   operation's tick before running it, so the result is in guest memory before the label
>   operation even starts.
> - The label operation posts the write to the GPU thread (TrySendCommand), because a label page
>   can be write-protected by resource tracking and only the GPU thread may take that fault and
>   readback (the record-time path wrote from the GPU thread too). The write is the same memcpy.
> - A label with an interrupt raises it after the write (it is not queued separately), so an
>   interrupt handler never sees the old label value.
> - A later label to an address whose deferred label is still pending is deferred as well
>   (FIFO), so it cannot be overwritten by the older value.
> Hence the guest can observe the label only after the published result, exactly the guarantee
> the sync wait gave for this consumer, without blocking the CP.
>
> Consumers that wait inside the GPU:
> - WAIT_REG_MEM on a pending deferred label flushes the recording if the label's tick has not
>   been submitted, then suspends (no deadlock: the tick is submitted, the completion runner and
>   the GPU-thread command make progress without the suspended queue). Removing a deferred label
>   clears the blocked marks and signals the CP scheduler, so any suspended queue (this or
>   another) retries immediately instead of after its timeout.
> - SET_PREDICATION on occlusion results now also waits for priority publications after its
>   BufferWait; boolean predication with wait_op runs pending label-write commands first.
> - At shutdown the GPU thread drains the completion runner before it exits; label operations
>   that can no longer post a command write the backing directly.
> Done()/WaitForIdle do not wait for deferred labels (hardware does not either).
>
> Stretch: KYTY_LABEL_MODE=completion (default "record") defers ALL such label writes to their
> tick's completion, in order, with the same mechanism (the yuzu/RPCS3 model).
>
> Switches: KYTY_OCCLUSION_PROXY_MODE=sync restores the previous CP wait; KYTY_OCCLUSION_SYNC_PROXY=0
> still disables proxy handling; KYTY_LABEL_MODE=record|completion.
> Counters: FrameEvent.OcclusionProxyDumps, LabelWritesDeferred(/Proxy/Ordered),
> WaitRegMemDeferredLabel(/Flushes); FrameWait.GpuWaitOcclusion drops for defer-label.
> Verify: Sky Garden water must still render with the default (defer-label).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/guest_gpu/graphicsRun.h`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/occlusion.h`.

### 89. CommandScheduler: batch completion-runner wakeups (T4)

Commit: [`34bd486e`](https://github.com/Jetsku/KytyPS5-experimental/commit/34bd486e29921389fbcf83a9c92e8c52a837a49a) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DeferPriorityOperation did notify_one on the shared m_operation_available for every queued
> operation (EOP interrupts, publications, flips), and the runner did notify_all after every
> operation (~500-1,000 wakes per frame). The shared condition also meant a push's notify_one
> could wake a WaitPriorityOperations waiter instead of the runner, which then slept until the
> next notification.
>
> With KYTY_PRIORITY_WAKE_BATCH (default on):
> - The runner sleeps on its own condition (m_priority_available). A push wakes it only when the
>   queue was empty; otherwise the runner pops the operation itself after the current one.
> - After an operation the runner clears its active mark and pops the next operation in the same
>   lock section, and broadcasts m_operation_available only if a thread is in
>   WaitPriorityOperations/DrainPriorityOperations (counted under the mutex) and the finished
>   operation was the last of its tick (queue empty or next operation from a later tick).
> - SetProgressHook lets the owner observe each completed operation (used by the CP scheduler to
>   wake suspended queues); the hook runs while the operation is still marked active, so
>   clear-then-drain is race free.
>
> Correctness: operation ticks are non-decreasing in FIFO order (registered at CurrentTick by the
> single recording producer). A waiter for tick X is satisfied exactly when no active or queued
> operation has tick <= X; that state first occurs right after the last operation with tick <= X,
> when the next front has a later tick (or none) - the point where the broadcast now happens.
> Shutdown takes the mutex between request_stop and the runner notify so the stop cannot be lost.
> The priority thread is now the last member, so it never runs before the members it uses exist.
>
> Switch: KYTY_PRIORITY_WAKE_BATCH=0 restores per-operation notifications.
> Counters: FrameEvent.PriorityOperationsRun / PriorityWaiterWakeups.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.h`.

### 90. CP scheduler: wake suspended queues on progress, spin briefly before sleeping (F6)

Commit: [`d6470af9`](https://github.com/Jetsku/KytyPS5-experimental/commit/d6470af9831bf25c4ed9aa4e950a5f7a17f09dd3) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> When every guest queue was suspended (WAIT_REG_MEM, WaitCe, predication pending, ...), the
> GPU thread slept in WaitFor(100 us), which the Windows condition variable rounds to >= 1 ms,
> and blocked marks were cleared only on that timeout or when some queue completed. Completions
> that a suspended queue waits for (labels, published results, interrupts) therefore cost up to a
> timer tick each.
>
> With KYTY_CP_WAKEUPS (default on):
> - GuestGpu registers a CommandScheduler progress hook; after every completion-runner operation
>   (occlusion/LOD publications, downloads, EOP interrupts, flip completions, deferred-label
>   posts) NotifyProgress clears the blocked marks and signals the scheduler. An atomic
>   "some queue is blocked" flag makes the call a single exchange when nothing is blocked.
> - Deferred label writes (F2) already clear the marks and signal when they land.
> - When all queues are blocked the scheduler first retries them for KYTY_CP_BLOCKED_SPIN_US
>   (default 50 us, yielding between rounds) - guest CPU writes are not observable otherwise -
>   and then sleeps up to 1 ms, now normally ended by a signal. The spin window restarts only
>   after real progress (a slice that advanced or completed, or a GPU-thread command), so a
>   queue that stays blocked cannot keep the thread spinning.
> - Slices now report whether they advanced (Submission::slice_progress).
>
> Correctness: only retry timing changes. A blocked queue is re-run exactly as after the old
> timeout (its suspended packet re-evaluates its condition); clearing blocked marks early can
> only cause an extra re-evaluation. Shutdown clears the hook and drains the runner before the
> GuestGpu is destroyed.
>
> Switches: KYTY_CP_WAKEUPS=0 (old 100 us wait, no hook, no spin); KYTY_CP_BLOCKED_SPIN_US.
> Counters: FrameEvent.CpBlockedSpins / CpBlockedSleeps / CpProgressWakeups.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/guest_gpu/graphicsRun.h`.

### 91. CP: suspend the queue in WAIT_FLIP_DONE instead of blocking the GPU thread (F7)

Commit: [`7868ec80`](https://github.com/Jetsku/KytyPS5-experimental/commit/7868ec80dd65d5e0f872a2d76ad04bb2a83a4a0c) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> WAIT_FLIP_DONE flushed and then blocked the GPU thread in FlipQueue::Wait until the presenter
> had displayed the buffer. That stalled every other guest queue (async compute) and every
> GPU-thread command, including guest-thread readbacks and unmaps that are served there.
>
> With KYTY_FLIP_WAIT_MODE=suspend (default) the handler flushes once on its first evaluation
> (as before), then, while a flip of that buffer is still queued (new non-blocking
> VideoOutDriver::IsFlipPending, the same predicate FlipQueue::Wait loops on), suspends its queue
> with SuspendPm4 like WAIT_REG_MEM. Retries skip the flush. When the presenter pops a flip (or a
> cancel removes requests) it calls RenderContext::NotifyGpuProgress, which unblocks suspended
> queues and wakes the CP scheduler (F6), so the retry happens right away rather than after the
> blocked-queue timeout.
>
> Correctness: the packet completes only when the same condition the blocking wait waited for
> holds, and nothing after it in that queue runs earlier, so the queue's ordering is unchanged.
> Other queues and GPU-thread commands may now run while the flip is pending; they were never
> ordered behind the flip on hardware either. Flip completion does not depend on the GPU thread
> (the EOP completion runner marks it ready, the presenter thread displays it). A CPU flip
> queued behind this very submission on queue 0 could not complete in either mode.
> NotifyGpuProgress is guarded by a shared mutex and becomes a no-op once GPU shutdown starts.
>
> Switch: KYTY_FLIP_WAIT_MODE=block restores the blocking wait.
> Counters: FrameEvent.FlipWaitSuspends / FlipWaitBlocking.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/host_gpu/renderer/renderContext.h`, `src/graphics/presentation/videoOut.cpp`, `src/graphics/presentation/videoOut.h`.

### 92. CP: count GDS end-of-pipe reads; optional deferred GDS snapshot (F8); in-order deferred labels

Commit: [`8303769a`](https://github.com/Jetsku/KytyPS5-experimental/commit/8303769a5b4612322b1c752d6a1fffe82ab60947) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The GDS end-of-pipe read (RELEASE_MEM data_sel=5 -> WriteAtEndOfPipe event source 1, type 0x2f,
> index 6) calls SynchronizeGpu() -> Finish(): a full submit and wait for the whole backlog,
> then copies GDS to guest memory at record time. It is now counted (FrameEvent.GdsEopReads;
> its wait time is FrameWait.GpuWaitGds from the attribution commit).
>
> KYTY_GDS_EOP_MODE=defer (default "sync" until the counters show the path is frequent) instead
> records a copy of the GDS range into the download ring at the packet's position, i.e. a
> snapshot at exactly this point of the GPU timeline, and publishes it like a deferred label:
> a completion-runner operation for this tick reads the snapshot and posts the guest write to the
> GPU thread, followed by the interrupt. The download ring slot cannot be reused before that
> operation ran (Download stream buffers wait for their ticks' priority operations on reuse).
>
> Ordering rule for deferred end-of-pipe writes, tightened for both GDS and F2 labels: while any
> deferred write is pending, later labels are deferred as well (previously only labels to the
> same address). End-of-pipe writes therefore become visible in submission order, as on
> hardware, even when a later label goes to a different address; the only cost is that labels
> following a deferred one are written at completion too.
>
> Correctness (defer): the value is the GDS contents after all earlier work in the recording
> (barrier: all commands, memory writes -> transfer read), independent of later GPU work; the
> guest write happens after the tick completed, which is no earlier than hardware.
>
> Switch: KYTY_GDS_EOP_MODE=sync|defer. Counters: FrameEvent.GdsEopReads / GdsEopReadsDeferred.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`.

### 93. CP: submit early when the GPU has run out of submitted work (F4)

Commit: [`5052fbbb`](https://github.com/Jetsku/KytyPS5-experimental/commit/5052fbbb55b5201fb30cc73b6f38b0da17fb3080) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> After a drain (or any wait that emptied the queue) the GPU sat idle until the CP reached the
> next natural submission boundary (EOP interrupt batch, slice end, flip), while the CP kept
> recording into the open command buffer (measured GPU starved ~37-46 ms per Sky Garden frame).
>
> After each recorded draw/dispatch (DrawIndex, DrawAuto, native indirect draws, DispatchDirect,
> DispatchIndirect) MaybeFlushIdleGpu counts the work recorded in the current tick; once there
> are KYTY_IDLE_FLUSH_DRAWS (default 8) and the GPU has completed everything submitted
> (KnownGpuTick >= CurrentTick - 1, refreshed from the driver every 4th check), it flushes the
> recording so the GPU has work again. Inside an active rendering instance it requires 4x the
> draws, so passes are split only when an idle GPU has waited for a long pass. The count restarts
> whenever something else submits (the tick changes).
>
> Correctness: this only adds a submission boundary at a packet boundary, the same operation the
> EOP-interrupt batch flush and the slice-end flush already perform at arbitrary points; command
> buffer state (pipelines, dynamic state, descriptors, rendering instances, occlusion scopes) is
> re-established per command buffer as for those flushes.
>
> Bound: at most one extra submit per KYTY_IDLE_FLUSH_DRAWS draws, and only while the GPU is idle,
> so it cannot add submits while the GPU has a backlog.
>
> Switch: KYTY_IDLE_FLUSH_DRAWS=0 disables. Counters: FrameEvent.IdleFlushes / IdleFlushesInPass.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`.

### 94. CP scheduler: let runnable async compute preempt long graphics slices (T3)

Commit: [`7c9768a8`](https://github.com/Jetsku/KytyPS5-experimental/commit/7c9768a86f193b7be091638c825fb4f1a5c30850) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> One GPU thread runs the graphics queue and all 56 async compute queues round-robin, and a
> graphics slice ran until its submission completed or blocked (~100 ms of CP time for Sky
> Garden's DCB), so compute submissions waited up to that long (comp_incomplete ~34/s).
>
> With KYTY_GFX_SLICE_DRAWS (default 128) the graphics CP checks, every that many draws recorded
> in a slice (DrawIndex, DrawAuto, native indirect draws), whether some compute queue has a
> submission that is not suspended; only then it requests a yield. The PM4 loop honours the
> request after the current packet has been fully executed and its cursor advanced (unlike
> SuspendPm4, which re-executes the packet), the slice ends with its usual flush, and the
> submission goes back to the front of its queue unblocked; the round-robin pointer already
> points past the graphics queue, so the waiting compute queues run next, then graphics resumes
> exactly where it stopped.
>
> Correctness: a yield is a slice boundary at a packet boundary, the same state a suspension
> (e.g. WAIT_REG_MEM) produces, minus the retry of the packet; the submission's execution cursor,
> CE/DE counters and processor state are kept, and no packet is skipped or repeated. Queues were
> already interleaved at slice boundaries, so no new inter-queue ordering is introduced. The
> check (a locked scan of the compute queue fronts) runs once per KYTY_GFX_SLICE_DRAWS draws.
>
> Switch: KYTY_GFX_SLICE_DRAWS=0 disables. Counter: FrameEvent.GfxSliceYields.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/guest_gpu/graphicsRun.h`.

### 95. CP: write end-of-pipe data that INT_SEL used to suppress (A1, accuracy)

Commit: [`90a073d0`](https://github.com/Jetsku/KytyPS5-experimental/commit/90a073d06df93a4589c1fd5c2ca444a07518196c) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Two paths dropped the data write of an end-of-pipe event because of its interrupt selector:
> - WriteAtEndOfPipe on the graphics queue with INT_SEL=1 queued the interrupt and returned
>   without writing the label (compute queues wrote it).
> - CpOpReleaseMem with INT_SEL=4 and DATA_SEL != 0 took the "no data" branch (barrier plus
>   interrupt only).
> On RDNA INT_SEL does not gate DATA_SEL, so a guest waiting on such a label never saw it
> change (or relied on a later write).
>
> Both are now counted (FrameEvent.EopLabelsIntSel1 / ReleaseMemLabelsIntSel4) and, by default,
> the data is written as well: 32-bit data, 64-bit data or the reference clock, exactly the
> selections the regular paths support (GDS and unknown selections keep the old behaviour).
> The write goes through the F2 label logic: written at record time, or deferred to its tick's
> completion when a proxy dump armed it, KYTY_LABEL_MODE=completion is set, or an older deferred
> write is pending. When deferred, the interrupt is raised by the deferred write after the label
> is visible (the separate interrupt operation is skipped; the EOP flush batching is kept);
> otherwise the label is written before the interrupt operation is queued, so the interrupt
> still follows the label.
>
> No new EXIT paths: the event-type switch that validates regular writes is not consulted for
> these; only the value/size decoding is.
>
> Switch: KYTY_EOP_DROPPED_LABELS=count restores the old behaviour (counters only).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/guest_gpu/graphicsRun.cpp`.

### 96. CP: record WRITE_DATA on the GPU timeline when its destination is GPU-owned (A2, accuracy)

Commit: [`3988680a`](https://github.com/Jetsku/KytyPS5-experimental/commit/3988680a43e9bc050bf80f2efb22db77ab00eb60) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> WRITE_DATA (~175 per frame) was a CPU memcpy at parse time. If recorded-but-unexecuted GPU work
> owns the destination bytes, the CPU write either faulted (write fault -> invalidation readback
> -> full GPU drain) or, where it did not fault, was later overwritten by the GPU data's readback,
> leaving the wrong final value.
>
> With KYTY_WRITE_DATA_GPU (default on), when the destination has GPU-dirty bytes or a pending
> backing publication, BufferCache::TryWriteDataGpu records vkCmdUpdateBuffer into the cached
> buffer at the CP's position (after EndRendering, with memory->transfer->memory barriers),
> obtained as a written binding: CPU-dirty bytes are uploaded first, the range becomes GPU-owned
> with this recording's writer tick, and overlapping images are invalidated as for a GPU copy.
> WRITE_ONE_ADDRESS writes only the last dword (the final value the CPU loop left). Otherwise the
> CPU write is kept unchanged.
>
> Correctness: the GPU applies the write after all earlier recorded work and before later work,
> which is the CP order; the result is GPU-owned like any GPU write and reaches the CPU through
> the normal readback path. A pending publication of older GPU data may still land in the
> backing, but the bytes stay GPU-dirty, so the newer value is read back before any CPU read.
>
> Switch: KYTY_WRITE_DATA_GPU=0. Counters: FrameEvent.WriteDataGpu / WriteDataCpu.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`.

### 97. AGC: bound sceAgcSuspendPoint to two frames in flight instead of waiting for CP idle (S2)

Commit: [`11a941b3`](https://github.com/Jetsku/KytyPS5-experimental/commit/11a941b3e1b1a572b96476100ee55890593d4562) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> AgcSuspendPoint -> GuestGpu::Done held m_submission_mutex and waited until the CP had consumed
> everything and had nothing queued (WaitForIdle). Measured: one wait per frame of 105-145 ms,
> which serialized building guest frame N+1 with CP processing of frame N and blocked every other
> thread's Submit/SubmitCompute/SubmitFlipPreparation on the admission lock. Hardware does not
> wait for GPU idle there (the GPU consumes command buffers asynchronously; games gate command
> memory reuse with labels/flips).
>
> KYTY_AGC_DONE_MODE=bounded (default):
> - Under the admission lock, Done records the frame boundary exactly as before: the next
>   graphics / CPU-flip submission carries reset_processor, and the frame number advances. The
>   processor reset therefore still happens at this point of the graphics queue's admission
>   order, i.e. after every earlier graphics submission - the same place as after an idle wait.
> - Every submission gets an admission sequence number; the set of not-yet-completed sequences
>   is kept under m_queue_mutex (requeued/suspended submissions stay in it until they complete).
> - Done then releases the admission lock and waits only until every submission admitted before
>   the PREVIOUS Done has completed on the CP (at most two frames in flight), signalled per
>   completed submission while a Done waiter exists; shutdown releases it.
>
> DCB/ring memory safety: submitted spans are borrowed until the CP has parsed them. Labels are
> written when the CP parses them (record mode) or later (deferred), so a guest that reuses
> command memory after observing a label from the end of that memory still reuses it only after
> the CP consumed it; a guest that relied on Done-idle alone for reuse would be relying on a
> guarantee hardware does not give. The bound keeps the CP at most one frame behind the frame
> being built, which also bounds memory/borrowed-span lifetime growth.
> Deadlock: a submission that waits for something the guest does after Done could not complete
> in idle mode either; bounded mode waits for strictly less (an older prefix, no service
> commands) and does not hold the admission lock while waiting.
>
> Switch: KYTY_AGC_DONE_MODE=idle restores the old wait (logged at first use).
> Counters: FrameWait.AgcDoneWait (both modes), FrameEvent.AgcDoneBoundedWaits.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/guest_gpu/graphicsRun.h`.

### 98. CP: submit the tick of a deferred visibility-proxy label right away (F2 follow-up)

Commit: [`a2cf68e7`](https://github.com/Jetsku/KytyPS5-experimental/commit/a2cf68e71838b142f34432cad4c2d9055626e3da) · **Occlusion and result publication**

Makes visibility results usable with the required publication order, or reduces query/reduction waits. Only guest decisions using the result can reduce draws; an isolated culling gain is unproven.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A deferred label becomes visible only after its tick completes, and its tick is submitted at
> the next boundary (EOP flush batch, slice end, idle flush), which after a proxy dump can be
> most of a frame away while a guest thread already waits for that label. The old sync mode
> made the label visible right after the wait; to keep that latency bounded, the CP now flushes
> the recording immediately after deferring a proxy-armed label (about 5 extra submits per
> Sky Garden flip, replacing the 5 full drains of the sync mode). Labels deferred only for
> ordering or by KYTY_LABEL_MODE=completion keep the existing boundaries.
>
> Correctness: an extra submission boundary at a packet boundary (see F4); ordering of the
> publication and the label is unchanged.

</details>

Changed files: `src/graphics/guest_gpu/graphicsRun.cpp`.

### 99. Merge submission/sync: shader-hash fault fix, side-queue readbacks, label deferral, bounded AgcDone, CP wakeups (claude/submission-sync)

Commit: [`4ff6ed69`](https://github.com/Jetsku/KytyPS5-experimental/commit/4ff6ed69ad9d5b6de6d46caf1abf5deda6a52b3c) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> # Conflicts:
> #	src/common/profiler.cpp
> #	src/common/profiler.h
> #	src/graphics/guest_gpu/command_processor/commandProcessor.h
> #	src/graphics/host_gpu/renderer/commandScheduler.h

</details>

### 100. Merge codex/astro-profile (submission/sync) into claude/binding-path

Commit: [`1ce0ded6`](https://github.com/Jetsku/KytyPS5-experimental/commit/1ce0ded67017c9a76e409a5be17b4a0b0f4038bd) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Conflicts only in the profiler FrameEvent lists: both sides' counters kept,
> upstream first, the binding-path counters after them.

</details>

### 101. Merge texture binding memo, upload dedup, push-constant shadow, shared layouts, descriptor-set reuse (claude/binding-path)

Commit: [`0ef041f6`](https://github.com/Jetsku/KytyPS5-experimental/commit/0ef041f6fb018806c0a2ab62c44ed6fa9952bb72) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 102. tests: fix CommandScheduler construction and add --cases-only selector

Commit: [`703ca10d`](https://github.com/Jetsku/KytyPS5-experimental/commit/703ca10d519fcf8b1055c750d2fe1ec3f638d4ba) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> CommandScheduler gained a Role parameter (guest vs presenter timing); the
> six test constructions still used the two-argument form, so
> shader_recompiler_compute_tests did not compile. They now pass Role::Guest.
>
> The default run aborts early on this machine (stack overflow inside
> CheckComparisonDepthTexture, unrelated to the recompiler), so add
> --cases-only, which runs just the recompiler semantic cases (MakeCases and
> MakeGraphicsCases). All 383 cases pass on the RTX 3090 in about 7 s.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 103. shader: fold V_MOVRELS/V_MOVRELD select chains with M0 value sets

Commit: [`e421aeef`](https://github.com/Jetsku/KytyPS5-experimental/commit/e421aeefbdc8ae4e9fe37806bdd7cc12100c3fec) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> V_MOVRELS/V_MOVRELD expand to one IEqual(M0 & 0xff, k) + Select per VGPR
> above the base (up to the highest VGPR the program names). In Astro Bot's
> material pixel shaders M0 is BFE(s, 24, 3) * 5 or similar, so almost all
> of those compares are always false, yet they made up about half of the
> SPIR-V of shaders like 0082_ps.
>
> Constant propagation now computes finite value sets for U32 SSA values
> (bit-field extracts of <= 6 bits, AND with a mask of <= 6 bits, and
> add/sub/mul/shift/or/xor/min/max/select/phi over known sets, capped at 64
> values) and folds any U32 compare that has the same outcome for every pair
> of possible operands. The chain selects then fold to their false operand
> and dead-code elimination removes them.
>
> Correctness: the sets are exact over-approximations. Unknown values stay
> unknown, phi cycles give up, and a compare folds only when every possible
> operand pair agrees, so no reachable result changes.
>
> ISA fix (RDNA2 3.6.1 / 6.2.6): a V_MOVRELS source index at or past the
> VGPR range reads VGPR0; it used to read VGPR[base]. The chain now ends in
> Select(M0 < limit - base, chain, v0). VGPRs between the highest named one
> and the allocation size are never written, so VGPR0 is a valid value for
> them too. When the value set proves the index in range, the select folds.
> V_MOVRELD already ignored out-of-range destinations.
>
> Switch: KYTY_MOVREL_RANGE=0 restores the previous code (no value-set
> folding, VGPR[base] for out-of-range sources). Switches live in the new
> ShaderRecompiler::CodegenOptions (read from the environment once; tests
> can override them).
>
> Test: shader_recompiler_compute_tests --codegen-only runs
> CodegenMoveRelValueSet on the GPU with M0 loaded from memory (8 values),
> checking every result in both modes, including in-range, never-written
> and out-of-range indices. OpSelect count 1262 -> 344, SPIR-V 16638 ->
> 6586 words. --cases-only (383 cases) still passes.

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/frontend/translate/Control.cpp`, `src/graphics/shader/recompiler/ir/passes/ConstantPropagation.cpp`, `tests/ShaderCodegenTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 104. shader: cheaper exact f32 min/max/min3/max3/med3

Commit: [`c2b047ac`](https://github.com/Jetsku/KytyPS5-experimental/commit/c2b047ac7ce8b04ded77a68883290e46df39c519) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> EmitMinMaxF32Value classified both operands bit by bit (NaN, zero),
> then combined an ordered compare with four selects: about 20 SPIR-V ops
> per min/max, about 110 for med3 (five min/max plus three NaN
> classifications).
>
> Each min/max now keeps the legacy decision but computes it directly:
>   pick lhs = (FOrd{LessThan|GreaterThanEqual}(lhs, rhs) || isnan(rhs))
>              && !isnan(lhs)
>   result   = both zero by bits ? lhs|rhs (min) / lhs&rhs (max)
>                                : (pick ? lhs : rhs), selected as raw bits
> Constants drop the parts that cannot apply (a non-NaN constant is never
> the NaN operand; a non-zero constant never forms two zeros). med3 keeps
> the legacy composition (any NaN -> min3, else max(min(a,b),
> min(max(a,b),c))) on top of these, and skips the NaN test for non-NaN
> constants.
>
> Why not GLSL NMin/NMax plus fix-ups (as planned): on the RTX 3090 the
> driver flushes f32 denormals in register-register compares (the module
> declares no f32 denorm mode because shaderDenormFlushToZeroFloat32 is
> false), but lowers compares against some constants to integer tests that
> do not flush. NMin/NMax, unordered compares or a FOrdEqual-based signed
> zero fix pick the other operand when a zero meets a denormal, so they
> were not bit-identical to the current code (the GPU test caught
> min(-0, 0x00000001) and med3(0, 0x00000001, 1.0)). Using the very same
> ordered compare instruction and selecting raw bits keeps the result
> identical.
>
> Correctness: for non-NaN, not-both-zero operands the pick is exactly the
> legacy compare; a NaN lhs yields rhs, a NaN rhs with a non-NaN lhs yields
> lhs, two NaNs yield rhs; two zeros take the same AND/OR of the bits.
>
> Switch: KYTY_FAST_FMINMAX=0 restores the classification code.
>
> Test (shader_recompiler_compute_tests --codegen-only, RTX 3090):
> - CodegenFloatMinMaxMed3: min, max, min3, max3, med3 (two operand
>   orders) and min(b,a) over all 32768 triples of 32 special f32 values
>   (zeros, denormals, normals, inf, quiet/signalling NaNs with payloads)
>   plus 32768 random triples: new == legacy bit for bit, and both equal a
>   CPU model of the legacy semantics with flushing compares.
> - CodegenFloatClampConstants: 17 min/max/min3/max3/med3 forms with
>   inline and literal constants (0, +-1, +-0.5, 4, -0.0, NaN) over 4096
>   values: new == legacy == CPU model, except x denormal (skipped): there
>   the legacy code itself is inconsistent (med3(0, 0x00000001, 1.0) gives
>   0x00000001 with registers but 0 with constants), so no single current
>   result exists.
> SPIR-V of the min/max test shader 7362 -> 5032 words; OpSelect 180 -> 112.

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterAlu.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInstructions.h`, `tests/ShaderCodegenTests.inc`.

### 105. shader: shorter exact V_CVT_PKRTZ_F16_F32 conversion

Commit: [`d1b0380b`](https://github.com/Jetsku/KytyPS5-experimental/commit/d1b0380ba812830349690ff4fb71a132f6d9b8da) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> EmitF32ToF16RtzBits built each half from separate normal, subnormal,
> NaN, infinity and saturation candidates joined by six selects, about 35
> SPIR-V ops per half. The new formulation computes the same bits with
> about 20 integer ops and three selects:
>   normal    = UMin((|bits| >> 13) - (112 << 10), 0x7bff)  (exp >= 113)
>   subnormal = (mant | 0x800000) >> UMin(126 - exp, 31)    (exp <= 112)
>   special   = ((|bits| >> 13) & 0x7fff) | (NaN ? 0x200 : 0) (exp == 255)
>   result    = select(...) | sign
>
> Correctness: for exponents 113..142 the first form equals the legacy
> (exp - 112) << 10 | mant >> 13; for finite exponents >= 143 it is at
> least 0x7c00, so UMin gives the legacy 0x7bff saturation; for exponents
> 103..112 the shift equals the legacy 126 - exp; for exponents <= 102
> (including f32 zeros and denormals) every bit is shifted out, leaving the
> signed zero as before; exponent 255 yields 0x7c00 plus the top payload
> bits, with the quiet bit for NaNs. Pure integer arithmetic, so it does
> not depend on host rounding, denormal flushing or NaN canonicalization.
>
> PackHalf2x16 plus a |unpack(h)| > |x| correction (the planned approach)
> was tried first and is not exact on NVIDIA: the driver folds
> UnpackHalf2x16(PackHalf2x16(x)) back to x, so overflowing inputs kept
> 0x7c00 instead of 0x7bff (caught by the GPU test).
>
> Switch: KYTY_FAST_PKRTZ=0 restores the select chain.
>
> Tests (shader_recompiler_compute_tests --codegen-only):
> - CodegenCvtPkrtzExhaustive: a CPU model of the new ops equals a CPU
>   port of the legacy ops for all 2^32 inputs.
> - CodegenCvtPkrtzF16F32 (RTX 3090): 524288 pairs (every exponent/sign
>   with mantissas around the rounding points, special values, random
>   bits), both operand orders: new == legacy == CPU reference.

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterAlu.cpp`, `tests/ShaderCodegenTests.inc`.

### 106. shader: saturate float-to-int conversions once

Commit: [`bd4979e7`](https://github.com/Jetsku/KytyPS5-experimental/commit/bd4979e748199f01152c2e7acc1c6b380a63d07f) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> V_CVT_U32/I32/RPI/FLR_F32, V_CVT_U16/I16_F16 and V_CVT_PK_U8_F32 go
> through Translator::ConvertF32To{U,I}32Saturated, which replaces NaN,
> truncates, clamps the operand into the destination range and selects the
> saturated results itself. The resulting ConvertU32F32/ConvertS32F32 were
> then lowered by EmitF32ToU32, which truncated, classified NaN and
> range-checked the (already safe) operand a second time: about 12 extra
> SPIR-V ops per conversion.
>
> The emitter now lowers them to a bare OpConvertFToU / OpConvertFToS.
>
> Correctness: those translator helpers are the only producers of the two
> opcodes (SrtWalker only evaluates them on the CPU). Their operand is
> never NaN, is integral, and lies in [0, 4294967040] or [-2^31,
> 2147483520] (255/65535/32767 variants are narrower), so trunc is the
> identity, the NaN select never fires, the upper checks never fire and
> the lower check (x <= 0 / x <= -2^31) returns what the conversion
> returns anyway.
>
> Switch: KYTY_SINGLE_F2I_SATURATION=0 keeps the second layer.
>
> Test: CodegenFloatToIntSaturation (RTX 3090) runs all seven conversions
> over 65536 inputs (special values, +-2 ulp around every range limit and
> rounding point, random bits): identical with and without the switch.
>
> Not done (item 8 of the audit): skipping the manual denormal flush before
> rcp/rsq/sqrt/exp/log when DenormFlushToZero is declared. Vulkan only
> requires denormal results to be flushed under that mode; denormal
> operands may be flushed, so rcp/rsq/sqrt/log of a denormal could
> differ (e.g. rcp(2^-127) = 2^127 instead of inf). It is also moot on the
> RTX 3090, whose driver reports shaderDenormFlushToZeroFloat32 = false, so
> the mode is never declared there.

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterAlu.cpp`, `tests/ShaderCodegenTests.inc`.

### 107. shader: skip GET_LOD_STATS feedback for images without a counter

Commit: [`a4bc914d`](https://github.com/Jetsku/KytyPS5-experimental/commit/a4bc914d0635e9c7b041f24dc634615c283282e6) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every pixel shader that samples gets GET_LOD_STATS instrumentation: per
> sample instruction an OpImageQueryLod plus LOD math, a subgroup UMin, an
> elect and two device-scope atomics. When the T# has MipStatsCntEn clear
> (id field 0x8000, the usual case: Astro Bot enables counting on only a
> few frames), all of that still ran and hammered entry 256, a dump slot
> the host never reads.
>
> Now (a) the counter id is loaded before the query; it is uniform per
> draw (push constant or shader-data word), so a branch on bit 15 skips
> the query, the subgroup ops and both atomics when there is no counter.
> Inside the branch the image and sampler are combined again because
> OpSampledImage must be in the consumer's block.
> (c) The elected lane issues AtomicUMin for the finest level only when a
> relaxed OpAtomicLoad shows a larger value.
>
> Correctness:
> - (a) The host (lodStats.cpp) reads entries 0..255 and their counts
>   only; entry 256 only absorbed images without a counter, so skipping
>   those records leaves every published value unchanged. The branch
>   condition is dynamically uniform, so the implicit-LOD query inside it
>   keeps valid derivatives.
> - (c) The finest words only decrease between resets: AtomicUMin is the
>   only shader writer, and GET_LOD_STATS resets with transfer fills that
>   pipeline barriers order against all shader access. A loaded value at
>   or below `finest` therefore stays at or below it, so the skipped UMin
>   could not have changed the word; a stale larger value just issues the
>   UMin as before.
> - (b), one record per image per invocation, is not done: the counts are
>   per sample instruction and subgroup today, and merging samples of the
>   same image would change the published counts.
>
> Switch: KYTY_LOD_STATS_GATE=0 restores the unconditional recording.
>
> Test: CodegenLodStatsGate compiles an IMAGE_SAMPLE pixel shader in both
> modes, validates the SPIR-V, and checks the instrumentation (one query,
> one UMin, one IAdd, one AtomicLoad and two extra selection constructs
> when gated). The graphics harness has no mip-statistics buffer, so the
> recorded values are not checked at runtime; measure with
> KYTY_LOD_STATS_GATE=0/1 in game (GPU time on non-counting frames, and
> lodreports.csv contents on counting frames should match).

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterImage.cpp`, `tests/ShaderCodegenTests.inc`.

### 108. shader: leave plain dword buffer-load bounds checks to robustBufferAccess2

Commit: [`199fc678`](https://github.com/Jetsku/KytyPS5-experimental/commit/199fc678b6dc50d6949ee40b42d647e03e01714c) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every buffer/scalar-buffer dword load was wrapped in
> "index < OpArrayLength ? load : 0" as a SelectionMerge + branch + phi
> (about 55 % of all conditional branches in the Astro Bot corpus), even
> though the device enables robustBufferAccess2, which returns zero for
> out-of-range storage-buffer loads.
>
> When the device guarantees it (new Spirv::HostBufferRobustness, set in
> VulkanCreateDevice: VK_EXT_robustness2 enabled, robustBufferAccess2
> supported and robustStorageBufferAccessSizeAlignment == 1), plain dword
> loads (LoadWordPrepared, including raw dwordxN components and the
> components of the single formatted path, sub-dword loads, which load
> their containing dword, and s_buffer_load/ReadConstBuffer) now load
> unconditionally from Buffer/ScalarBuffer descriptors.
>
> Kept explicit: LDS, GDS, scratch, the all-or-nothing formatted plans,
> stores, atomics (the robustness2 result of an out-of-range atomic is
> not defined as 0) and the indirect/BDA paths.
>
> Correctness: the shader check compares the dword index with
> OpArrayLength, i.e. the descriptor range in whole dwords, and returns 0
> otherwise; robustBufferAccess2 returns 0 for accesses outside the
> descriptor range. Every OOB_SELECT mode is modelled on this path by the
> same descriptor-range check today, so no mode changes.
> The GPU test found that NVIDIA returns data for a dword that is only
> partly inside the range, despite reporting a 1-byte alignment. So
> NativeStorageBuffer (descriptors.cpp) now binds storage buffers in whole
> dwords (range rounded down to a multiple of 4 when it is at least 4).
> That is a no-op for every shader-side check (OpArrayLength already
> floors, and floor(range/8) for 64-bit atomics is unchanged). Remaining
> difference: a guest buffer smaller than 4 bytes, where a dword load of
> dword 0 may now return its bytes instead of 0.
>
> Switch: KYTY_ROBUST_BUFFER_LOADS=0 keeps the shader checks. Not done:
> binding scalar-only buffers as UBOs (audit 4a).
>
> Tests:
> - The test harness now enables robustBufferAccess + robustBufferAccess2
>   like the emulator and sets HostBufferRobustness, so all 383
>   --cases-only cases (many OOB tests among them) run with device-checked
>   loads and pass.
> - CodegenRobustBufferLoads: dword, ushort, sshort, ubyte, sbyte,
>   dwordx4 and s_buffer_load x1/x4 across the end of a range of
>   4*264+2 bytes, shader-checked with that range and device-checked with
>   the whole-dword range: both equal the CPU expectation.

</details>

Changed files: `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`, `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp`, `tests/ShaderCodegenTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 109. shader: unfused V_MAD/V_MAC with KYTY_MAD_MODE (default: position)

Commit: [`3cf3ed8b`](https://github.com/Jetsku/KytyPS5-experimental/commit/3cf3ed8bdcdb0291078019ab2020da1ac59d4c22) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> PS5 shaders use the legacy V_MAD_F32 (VOP3 0x141, 14175 uses in the
> Astro Bot corpus), V_MAC_F32 (VOP2 0x1f / VOP3 0x11f), V_MADMK_F32 and
> V_MADAK_F32. The RDNA2 ISA document lists them as removed; on PS5 they
> keep their GCN/RDNA1 meaning: the product is rounded to f32 before the
> add, so v_mad_f32 equals v_mul_f32 + v_add_f32 bit for bit. They were
> all emitted as a fused GLSL Fma, and no float arithmetic carried
> NoContraction, so the host could also fuse separate multiplies and adds.
> Two shaders computing the same position (depth pre-pass and main pass,
> shadow caster and receiver) could then round differently and z-fight.
>
> New IR opcode FPMad32 for the unfused family. The decoder maps the
> fused RDNA2 V_FMAC/V_FMAMK/V_FMAAK_F32 (VOP2 0x2b-0x2d, VOP3 0x12b) to
> the same Decoder opcodes, so the translator checks the encoding and
> keeps those (and V_FMA_F32) on the fused FPFma32.
>
> KYTY_MAD_MODE selects the emission:
> - exact: every FPMad32 is OpFMul + OpFAdd, both NoContraction, and every
>   guest FAdd/FSub/FMul is NoContraction. Bit exact everywhere; one extra
>   instruction per MAD.
> - position (default): exact only for instructions in the backward data
>   flow of position exports (SetAttribute with a Position target), and the
>   gl_PerVertex position member is decorated Invariant; FMA and free
>   contraction elsewhere. Positions become bit exact and identical across
>   shaders while pixel-shader math keeps the cheaper FFMA. Chosen as the
>   default because MAD/MAC are about 36k instructions in the corpus,
>   mostly in pixel shaders, and the game is GPU bound; the pixel-side
>   differences are last-ulp shading noise.
> - fused: the previous behaviour.
>
> Correctness: FMul then FAdd, each rounded to f32 and forbidden from
> contracting, is the definition of the unfused MAD. The position slice
> follows every SSA operand (including phis) back from each position
> export, so no instruction that influences a position is left fused.
>
> Tests (shader_recompiler_compute_tests --codegen-only, RTX 3090):
> - CodegenMadModes: V_MAD, V_MAC, V_MADMK, V_MADAK, V_FMA and V_FMAC over
>   16384 random normal triples in all three modes against a CPU model
>   (round(round(a*b)+c) for exact, fmaf otherwise; compute shaders have
>   no position, so position mode is fused there). 1967 elements differ
>   between fused and unfused, and each mode matches its model exactly.
>   FMAC/FMA stay fused in every mode.
> - CodegenMadPositionSlice: a vertex shader with one MAD into pos0 and
>   one into param0 gives 0/1/2 Fma, 4/2/0 NoContraction and 1/1/0
>   Invariant in exact/position/fused, and validates.
> - --cases-only passes with the default and with KYTY_MAD_MODE=exact.

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterAlu.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInstructions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterModule.cpp`, `src/graphics/shader/recompiler/frontend/translate/Vector.cpp`, `src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.inc`, `tests/ShaderCodegenTests.inc`.

### 110. shader: interpolate each pixel input like its V_INTERP I/J pair

Commit: [`18e9abfd`](https://github.com/Jetsku/KytyPS5-experimental/commit/18e9abfd04e64700eed4407de89be837a43aec67) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> V_INTERP_P1 was a no-op and V_INTERP_P2 read the host-interpolated input
> while ignoring its I/J operand. Every input was interpolated at the pixel
> center, and all of them were NoPerspective as soon as SPI_PS_INPUT
> enabled LINEAR_CENTER, even inputs the shader interpolated with the
> perspective pair. In the Astro Bot corpus 226 of 1037 V_INTERP_P2 use
> the second or third I/J pair (v3 or v5), not v1.
>
> Now:
> - ShaderPixelInputInfo also records the first VGPR of the perspective
>   sample and the linear sample/center/centroid pairs (SPI_PS_INPUT_ADDR
>   order; part of the stage key), and the pixel entry block sets those
>   VGPRs to the matching barycentrics (new StageInputKinds
>   BaryCoordSmoothSample, BaryCoordNoPerspectiveCentroid,
>   BaryCoordNoPerspectiveSample; emitted with InterpolateAtCentroid /
>   InterpolateAtSample on gl_BaryCoord(NoPersp)KHR), so shaders that read
>   them directly get correct values instead of 0.
> - V_INTERP_P2 emits GetAttributeWithBary(attr, chan, J). Constant
>   propagation turns it into GetAttribute with an InterpolationMode flag
>   when J is the second component of a hardware pair, else Unknown.
> - DefineInputs decorates each input variable (all aliases of its
>   location together) from its reads: NoPerspective when all reads use
>   linear pairs, Centroid or Sample when all reads use that location.
>   Centroid reads of an input that is also read at the center use
>   InterpolateAtCentroid. Unknown reads, perspective/linear mixes and
>   sample reads mixed with other locations keep the old shader-wide
>   interpolation.
>
> Correctness: the hardware interpolates P2 with the I/J values in its
> operands; for the hardware-provided pairs those are exactly the
> perspective/linear barycentrics at the center, centroid or sample, which
> is what the decorations (or InterpolateAtCentroid) request from the
> host. Custom I/J values are left as before. Zero runtime cost:
> decorations only, plus InterpolateAtCentroid for mixed inputs.
>
> Switch: KYTY_INTERP_MODES=0 restores the previous behaviour.
>
> Tests: CodegenInterpolationModes compiles a pixel shader reading one
> input through the perspective center pair, one through the linear
> center pair, one at centroid and center and one at centroid only, with
> LINEAR_CENTER enabled: 1 NoPerspective, 1 Centroid, 1
> InterpolateAtCentroid (legacy: 4 NoPerspective); SPIR-V validates.
> --cases-only (including the graphics interpolation cases) passes with
> the switch on and off. No runtime image comparison: the graphics harness
> draws with a constant w and one sample, where all modes coincide.

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterFlow.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInstructions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterModule.cpp`, `src/graphics/shader/recompiler/frontend/translate/Attribute.cpp`, `src/graphics/shader/recompiler/frontend/translate/Translate.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.inc`, `src/graphics/shader/recompiler/ir/passes/ConstantPropagation.cpp`, `src/graphics/shader/recompiler/ir/passes/ShaderInfoCollection.cpp`, `src/graphics/shader/shader.cpp`, `src/graphics/shader/shader.h`, `tests/ShaderCodegenTests.inc`.

### 111. tests: shader_cfg_tests budget for the Invariant position

Commit: [`6d6e4d80`](https://github.com/Jetsku/KytyPS5-experimental/commit/6d6e4d806c88cfb40597884fe98569f8032142c7) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> MadMode::Position (default) decorates the vertex position Invariant,
> one OpMemberDecorate: the "wqm" vertex-shader size budget grows by 4
> words and 1 instruction.
>
> KYTY_CFG_TESTS_CONTINUE=1 makes Check() report and continue instead of
> aborting, so the checks after the known pre-existing failure ("plain 2D
> sample emitted unrelated image declarations") can run. With it, the
> suite reports only that failure, both with every new codegen switch on
> (defaults) and with all of them off.

</details>

Changed files: `tests/shaderCfgTests.cpp`.

### 112. build: link CodegenOptions into scalar_provenance_tests

Commit: [`73ae992d`](https://github.com/Jetsku/KytyPS5-experimental/commit/73ae992d91cbb52706da8a59b3614af8d52dbbb5) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The test compiles ConstantPropagation.cpp, which reads the codegen
> switches since the V_MOVRELS value-set change. resource_tracking,
> resource_materialization, shader_vertex_metadata and scalar_provenance
> tests all pass.

</details>

Changed files: `CMakeLists.txt`.

### 113. Merge exact shader codegen improvements: MOVREL ranges, min/max/med3, PKRTZ, single F2I saturation, LOD-stats gating, robust loads, MAD modes, interpolation modes (claude/shader-codegen)

Commit: [`373f6a27`](https://github.com/Jetsku/KytyPS5-experimental/commit/373f6a277415b8067a673000ceca058e364e7ea1) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 114. Merge codex/astro-profile into claude/memory-uploads

Commit: [`fea22fb7`](https://github.com/Jetsku/KytyPS5-experimental/commit/fea22fb798ebb01fa81633bf13dac6bd0d891c32) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Brings texture streaming, resident mips, submission/sync, binding path and
> shader codegen (373f6a27) under the memory/upload work.
>
> Conflicts resolved:
> - common/profiler.h, profiler.cpp: both sides appended FrameEvents. Upstream's
>   entries first, then this branch's (append-only, names unchanged).
> - renderer/render.h: both switch declarations kept (PushConstantShadowEnabled,
>   DescriptorSetReuseEnabled, UploadBatchEnabled).
> - cache/bufferCache.h: this branch's InvalidateMemory(write_fault),
>   AdvanceFrame and UploadBatch, with upstream's updated side-copy comment
>   (GPU-thread side reads) on ReadMemory.
> - image/tiler.cpp: upstream's scratch pool kept. The ScratchAllocs /
>   ScratchAllocBytes / ScratchAllocNs counters now count native vmaCreateBuffer
>   calls only (pool misses, or every use with KYTY_TILER_SCRATCH_POOL=0), bytes
>   = the created capacity; pool hits are upstream's TilerScratchPoolHits.
> - cache/textureCache.cpp (SynchronizeBufferFromImage): upstream added the
>   texel-sync skip into the function this branch split into
>   SynchronizeBufferFromImage + RecordImageDownload (the auto-merge left the
>   skip returning `true` from a byte-count function). RecordImageDownload now
>   takes skip_unchanged/*skipped and holds upstream's skip check and the
>   texel_sync invalidation verbatim; the texel wrapper counts
>   TexelImageSyncSkips / TexelImageSyncDownloads and records texel_sync exactly
>   as upstream, then settles hot pages (downloads only). A skip needs a content
>   revision for the whole range, which no CPU-dirty (so no hot) page has, so
>   skipping the settle there is exact. PreserveImagesForGpuWrite does not use
>   the skip; its download bumps the buffer revision, which invalidates any
>   texel_sync mark referring to it.
>
> Semantic adjustments (no conflict markers):
> - PreserveImagesForGpuWrite also requires Image::FullyResident() (resident
>   mips): non-resident levels hold undefined native contents and must never
>   reach guest memory, as TextureCache::SafeToDownload now requires. GPU-modified
>   images are fully resident anyway (MarkImageGpuModified counts violations), so
>   this only keeps the exactness argument explicit; candidates are still found
>   with Image::Overlaps, which now tests the resident (live) range, exactly like
>   InvalidateMemoryFromGPU.
> - BufferCache::TryWriteDataGpu (CP WRITE_DATA recorded as vkCmdUpdateBuffer)
>   invalidated overlapping images before obtaining the destination, like fills
>   and copies did before item 4. With KYTY_IMAGE_WRITEBACK_ON_GPU_WRITE (default
>   on) it now obtains the destination first (which moves overlapping GPU-modified
>   images into it) and invalidates the images afterwards; =0 keeps upstream's
>   order. Its ObtainBuffer(written) also demotes hot pages in the range, like
>   every GPU write.
>
> Checked without changes: the flip-path AdvanceFrame call sits after upstream's
> NotifyGpuProgress wakeup, once per completed flip. The UploadBatch scopes in
> RebindBuffers / PrepareGraphicsBindings (descriptors.cpp) now also cover
> upstream's UploadShaderData, which records nothing (a stream-ring wrap submits,
> and End() flushes queued uploads first). Upstream's CommandBuffer changes (push
> constant shadow, BindDescriptorSet) record state commands only, and queued
> uploads are still never sunk past a draw (CanSinkPending / CanSinkDrawWrites).
> Neither fault-ahead nor the first-position VEH changes image page watchers, so
> the texture fault fast path and chunk tracking are unaffected.
>
> Tests: memory_tracker, page_manager, bit_array, binding_path, resource_tracking,
> resource_materialization pass. GPU selectors give the same results as before the
> merge, except --gpu-tiler-only, which fails on upstream's default of not
> clearing detile scratch (KYTY_TILER_CLEAR_SCRATCH=1 passes; the test compares
> padding bytes).

</details>

### 115. Merge memory tracking and uploads: fault-ahead, hot pages, unlocked written uploads, upload barrier batching, image writeback on GPU writes, copy_via_buffer batching (claude/memory-uploads)

Commit: [`68931f65`](https://github.com/Jetsku/KytyPS5-experimental/commit/68931f65364c1fcef93dfdcfb0c28505a2c67c9b) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 116. renderer: draw-prep S4 coherence log, clean-read query and read-set certificates

Commit: [`0adb8669`](https://github.com/Jetsku/KytyPS5-experimental/commit/0adb86693eb8c4fb0ce2da9c0a3bef14ac2a6476) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Coherence log (graphics/host_gpu/coherenceLog.h):
> - The clean-verdict generation now lives in a range log: every transition that
>   retires clean verdicts appends {generation, [begin, end), source} before its
>   state change becomes observable. CleanVerdict::Invalidate(address, size,
>   source) replaces the rangeless bump at the buffer dirty Add/Subtract sites,
>   backing publication Begin/End (per published range, End before the erase),
>   tracker GPU mark/unmark/download/readback-unmark, image register/unregister,
>   GPU-modified and clear transitions (image guest range), and GPU map/unmap.
>   InvalidateContentRevisions logs a universe entry. Occlusion and LOD-statistics
>   results written straight into the backing log their range after the write.
> - The log is a 16K-entry seqlock ring. Check(g0, g1, ranges) answers Clean,
>   Conflict (with the source), Overflow (interval older than the ring) or
>   Unknown (entry being written or recycled); the last two are conservative.
>
> Read sets (graphics/host_gpu/renderer/drawPrep/readSet.h):
> - While a DrawPrep::Recorder is active on a thread, TryReadGpuCleanBacking
>   records every read (address, size, bytes) instead of only probing. The GPU
>   thread keeps the exact clean predicate; other preparing threads gate reads
>   with a thread-safe hint (tracker GPU-dirty pages and pending publications)
>   and read the backing view. Any unserved read fails the preparation.
> - Finish() coalesces reads into sorted non-overlapping ranges and rejects
>   overlapping reads that disagree. Validate() re-reads every range with the
>   coherent clean read and compares bytes; AllClean() checks verdicts only.
> - LibKernel::Memory::IsGpuCleanForRead: the clean verdict without a copy.
>
> No behaviour change: nothing activates a recorder yet.
>
> Tests: draw_prep_tests (log intersection, ordering, empty and universe
> entries, ring overflow and recycling, concurrent appends from eight threads;
> read-set coalescing, inconsistency, limits, validation, recorder scopes).

</details>

Changed files: `CMakeLists.txt`, `src/graphics/host_gpu/cleanVerdictCache.h`, `src/graphics/host_gpu/coherenceLog.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/drawPrep/readSet.h`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/kernel/memory.cpp`, `src/kernel/memory.h`, `tests/DrawPrepTests.cpp`.

### 117. renderer: draw-prep S5 inline prepare-then-commit with a serial oracle

Commit: [`3b30c863`](https://github.com/Jetsku/KytyPS5-experimental/commit/3b30c863ec5d86d0d12bdae24f4365daceb621e6) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_DRAW_PREP=inline (default off; off is unchanged):
> - CommandProcessor::DrawIndex/DrawIndexAuto hand each direct draw, with its
>   fully resolved arguments, to a DrawPrep::Engine. The engine copies the
>   context, user-config and shader register files into a snapshot, prepares
>   the draw from it on the GPU thread and commits it at once.
> - Prepare (drawPrep.cpp) is the speculative program preparation:
>   PipelineCache::PrepareGraphicsProgramsSpeculative runs the same static stage
>   preparation as GetGraphicsPrograms (shared FinishMeshStage,
>   ApplyDualSourceBlending and ApplyClipSpace helpers), materializes both
>   stages with probe-only readers and binds already published permutations.
>   Every guest read goes through the thread's DrawPrep recorder (read set).
>   It never compiles, synchronizes, reads back or faults: tessellation, O15
>   reuse, missing sources/permutations, header-less shaders (hashing the code
>   would bypass the certificate), unregistered shaders and any refused read
>   fail the preparation instead. shader.cpp stops at a failed probe rather
>   than reading the guest mapping and turns data-dependent EXITs on possibly
>   stale tables into failures while preparing.
> - Commit binds the command buffer to the snapshot
>   (CommandScheduler::BindRegisters/RestoreRegisters) and runs the unchanged
>   draw path. Where the serial path calls GetGraphicsPrograms, RefreshShaders
>   takes the preparation and validates it: the commit's own pixel-activity and
>   export-mapping decisions must match, the shader map generation must be
>   unchanged, and the certificate must hold. Only then are the programs,
>   stage infos and stage preps installed; otherwise the serial preparation
>   runs as before.
> - KYTY_DRAW_PREP_CERT=value (default): every coalesced read range must be
>   clean for a backing read now and hold the recorded bytes, which makes the
>   serial preparation now read and compute the same (readSet.h). =log: the
>   plan's cheaper check (no intersecting coherence-log entry since the
>   preparation began, and every range clean now); to be validated with the
>   oracle before use.
> - KYTY_DRAW_PREP_VERIFY=1|exit: after every committed preparation the serial
>   preparation runs on copies and programs, stage preps, static keys, vertex
>   descriptors and buffers are compared; differences are counted and logged
>   (exit stops).
> - CommandBuffer register accessors are now const: the draw path provably
>   never writes the registers a snapshot stands in for.
>
> S0 measurement: every PM4 packet of the graphics processor is classified
> (window-safe register/state packets, direct draws, fences); fences record the
> number of draws since the previous one in a bucketed histogram. Enabled with
> any draw-prep mode, or with KYTY_DRAW_PREP_HISTOGRAM=1 in off mode.
>
> Counters (FrameEvent.DrawPrep*): Submitted, SelfPrepared, Committed, Unused,
> Fallback{Ineligible,Unclean,Backing,Overflow,Inconsistent,Uncertified,
> NotPublished,ShaderMap,CertUnclean,CertChanged,CoherenceLog,Mismatch},
> CertRanges/CertBytes, VerifyChecks/VerifyMismatches, Fences and
> FenceDraws{0,1,2To3,...,64Plus}; FrameWait.DrawPrepPrepare and
> DrawPrepValidate.
>
> Tests: resource_materialization_tests gains a recorded-preparation case: the
> certificate covers the descriptor table, validates on unchanged memory,
> fails when a read byte changes (which changes the serial result), and eight
> threads preparing concurrently all certify and equal the serial result.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/guest_gpu/graphicsRun.h`, `src/graphics/host_gpu/renderer/commandScheduler.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `src/graphics/host_gpu/renderer/drawPrep/readSet.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/shader/shader.cpp`, `src/graphics/shader/shader.h`, `tests/ResourceMaterializationTests.cpp`.

### 118. renderer: draw-prep S6 parallel preparation window with DrawPrep workers

Commit: [`41c566f5`](https://github.com/Jetsku/KytyPS5-experimental/commit/41c566f529d47c51be215352a072c0c5a55cabb2) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_DRAW_PREP=parallel (default off):
> - Direct draws are published into a window (drawPrep/window.h): a bounded
>   single-producer ring whose published slots any number of preparing threads
>   claim (SPMC, per-slot {seq, state} words, so a slow claimer can never take
>   a recycled slot) and that the command processor retires in order. Each slot
>   holds the draw's register snapshot, its resolved arguments and its
>   PreparedDraw.
> - KYTY_DRAW_PREP_WORKERS (default 6) DrawPrep#k threads claim slots and run
>   the pure Prepare from the snapshot. They read guest memory through the
>   recorder with the thread-safe hint (tracker GPU-dirty pages, pending
>   backing publications) and the backing view; they read the program cache and
>   shader map (lookups only) and never touch the texture/buffer caches, the
>   scheduler or Vulkan. Idle workers spin KYTY_DRAW_PREP_SPIN_US (default 200)
>   and then park; publishing wakes parked workers.
> - The command processor keeps parsing. It commits the whole window in guest
>   order before every fence packet (ClassifyPacket: everything except
>   register/state writes, index/instance state, plain NOPs and markers, context
>   push/pop and plain indirect-buffer calls), before servicing commands from
>   other threads, before a draw whose instance count may be GPU data, when the
>   window (KYTY_DRAW_PREP_WINDOW, default 32) is full, and at the end of every
>   command-stream slice. Guest-visible label writes, waits, flips and
>   dispatches therefore still observe every earlier draw recorded.
> - Committing the head: if no worker claimed it, the command processor
>   prepares it itself (exact clean predicate); if a worker is on it, it spins,
>   servicing cross-thread commands every 1024 spins so a worker that page
>   faults cannot deadlock. The commit then runs exactly as in inline mode: the
>   command buffer reads the snapshot, the certificate is validated where
>   GetGraphicsPrograms would run, and a failed preparation or certificate runs
>   the serial preparation.
>
> Workers are flagged (DrawPrep::IsWorkerThread) and CommandScheduler::CheckActive
> stops the emulator if one ever reaches the scheduler.
>
> Counters: FrameEvent.DrawPrepPublished, Ready (worker finished before the
> commit needed it), SelfPrepared, CommitWaits, Drains, WindowOccupancy (sum of
> occupancy at publish; mean = WindowOccupancy / Published); FrameWait
> DrawPrepCommitWait (command-processor wait for workers) and DrawPrepPrepare
> (preparation time on all threads).
>
> Tests: draw_prep_tests covers the window protocol single-threaded (claim,
> producer self-claim, skipping taken positions, wrap-around) and with 1, 6 and
> 8 claiming threads over 4- and 32-slot windows (200k items, periodic drains):
> every item is prepared exactly once, correctly, and retired in order.

</details>

Changed files: `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `src/graphics/host_gpu/renderer/drawPrep/readSet.h`, `src/graphics/host_gpu/renderer/drawPrep/window.h`, `tests/DrawPrepTests.cpp`.

### 119. renderer: draw-prep S4-S6 review fixes

Commit: [`59e74d86`](https://github.com/Jetsku/KytyPS5-experimental/commit/59e74d8678cfbae64b142e4233b7d22f2e33bdfa) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Fixes from an independent review of the three stage commits (kept separate so
> the post-review changes stay auditable):
>
> - Certificate gap / worker faults: the AGC shader metadata a stage preparation
>   parses (user-data header, direct-resource offsets, input semantics) was
>   dereferenced directly. Preparations now copy it through the recorder and
>   the parsers consume the copies, so the certificate covers every parsed byte
>   (a small superset: the whole header and all semantics are copied) and workers
>   never dereference guest memory.
> - Draws the serial path never prepares are no longer prepared:
>   DrawPrep::DrawReachesPrograms (renderDraw.cpp) mirrors the early returns of
>   DrawIndex/DrawAuto on the snapshot (zero counts, target operations via the
>   existing pure superset, no vertex shader, unsupported topology). A
>   preparation therefore never evaluates registers the serial path would not
>   have evaluated for that draw (and cannot hit an EXIT the serial path would
>   not hit). Speculative guards also cover static vertex-buffer semantics.
> - Draws without an active pixel shader no longer swap the stage's pixel prep
>   (GetGraphicsPrograms leaves it untouched too), and the verify oracle only
>   compares the pixel prep when the pixel stage is active; before, depth-only
>   draws reported false mismatches.
> - Service commands run only with an empty window (the ProcessPm4 check and the
>   servicing are no longer separable by a racing enqueue). While waiting for a
>   worker, the command processor services commands only after 2 ms, as a
>   deadlock guard.
> - Coherence log slots are written in turn (a writer waits for the previous
>   lap's writer of its slot), so a reader can never accept a payload torn
>   between two laps.
> - Occlusion and LOD-statistics backing writes are logged only when the log
>   certificate (or its audit) is enabled, so in off mode they no longer move
>   the generation the clean-verdict cache shares.
> - Read sets no longer merge touching reads across a 4 KiB boundary (two guest
>   mappings can meet there; one read across them could fail spuriously).
> - KYTY_DRAW_PREP_LOG_AUDIT=1: with value certificates, counts where the log
>   check would have refused an unchanged certificate (DrawPrepLogWouldReject)
>   or accepted changed bytes (DrawPrepLogMissed).
> - ClassifyPacket moved to drawPrep/packetClass.h with its rationale.
>
> Tests: draw_prep_tests checks concurrent readers never accept a torn log entry
> (also across five ring laps), the 4 KiB merge rule, and the classification of
> every PM4 opcode, NOP marker and custom code; the recorded-preparation case
> also compares uniform_fill; the window test no longer requires workers to win
> a race.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/coherenceLog.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `src/graphics/host_gpu/renderer/drawPrep/packetClass.h`, `src/graphics/host_gpu/renderer/drawPrep/readSet.h`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/shader/shader.cpp`, `tests/DrawPrepTests.cpp`, `tests/ResourceMaterializationTests.cpp`.

### 120. Merge codex/astro-profile (0ef041f6) into claude/draw-prep-s4-s6

Commit: [`332e1e39`](https://github.com/Jetsku/KytyPS5-experimental/commit/332e1e39e1436d18355557f629b37b48f8719875) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Conflicts resolved:
> - CMakeLists.txt: both new test registrations kept (draw_prep, binding_path).
> - profiler.{h,cpp}: both sets of new FrameEvent/FrameWait entries kept, upstream
>   first, draw-prep after; name arrays in the same order (checked by script).
> - commandProcessor.h: upstream CP members (EOP batching, deferred labels, idle
>   flush, slice yield) kept, draw-prep engine member after them.
> - textureCache.cpp: Register/UnregisterImage keep NoteStructureChange and the
>   resident range `live`; the coherence entry is ranged over `live` (the range
>   the ownership predicate and the page owner index now use).
>   CountImagesOutsideGpuWrite kept next to the ranged InvalidateCleanImageProofs.
> - lodStats.cpp: both includes.
> - shader.cpp: GetShaderParams uses upstream HashShaderCode (clean-backing code
>   hash) serially; a draw-prep preparation still never hashes code (it fails as
>   Uncertified before, and HashShaderCode itself refuses under a recorder), so
>   workers never read code and the certificate covers every byte a committed
>   preparation used.
>
> Semantic adjustments:
> - Idle-GPU early submit (F4) and graphics slice yield (T3): prepared draws
>   returned before MaybeFlushIdleGpu/MaybeYieldSlice. The engine now runs an
>   after-commit hook (MaybeFlushIdleGpu) after every committed, i.e. recorded,
>   draw, with the live registers bound again; MaybeYieldSlice counts the draw
>   when it is submitted (a yield ends the slice after the packet and the slice
>   end commits the window first). Inline mode keeps the serial order.
> - ObtainWrittenBuffer (precise write ranges) logs its dirty Add per range.
> - Image coherence entries use the registered range `live`.
> - Deferred label / GDS writes (F2, F8), which run as GPU-thread commands at
>   completion, are noted in the log for the log certificate.
> - The log records entries only when a reader exists (KYTY_DRAW_PREP_CERT=log
>   or KYTY_DRAW_PREP_LOG_AUDIT=1); otherwise transitions only bump the shared
>   generation, as before the log existed. Content writes are noted likewise.
>
> Unchanged by design: EOP flush batching and its per-packet bound only submit
> recorded work (every draw before a RELEASE_MEM was committed by that fence's
> drain); WAIT_FLIP_DONE, WRITE_DATA, RELEASE_MEM, EVENT_WRITE(_EOP) remain
> fences; service commands (including deferred label writes) run with an empty
> window; the binding-path memos, push-constant shadow and descriptor-set reuse
> run at commit through the unchanged draw path.

</details>

### 121. Merge codex/astro-profile (373f6a27, shader codegen) into claude/draw-prep-s4-s6

Commit: [`231894b3`](https://github.com/Jetsku/KytyPS5-experimental/commit/231894b3732c2f9a908105ecdffd978dfc6660a9) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> No conflicts. Checked interactions:
> - The new ShaderPixelInputInfo I/J-pair fields (perspective sample, linear
>   sample/center/centroid VGPRs) are derived from the PS registers in
>   ShaderGetStaticInputInfoPS, which the speculative preparation runs on the
>   register snapshot, and they are part of BuildStageStaticKey, which the
>   verify oracle compares, so both paths cover them.
> - CodegenOptions is read once per process and only affects compilation,
>   which stays on the serial path.
> - descriptors.cpp storage-range rounding runs at commit, unchanged.

</details>

### 122. renderer: share the metadata color-mode test between the draw path and draw-prep

Commit: [`d98af116`](https://github.com/Jetsku/KytyPS5-experimental/commit/d98af11679a1f09aa25a2b97632b4f7e686adc8c) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> ConsumeMetadataColorOperation and MayRunTargetOperation (the pure superset
> DrawIndirectNative and DrawPrep::DrawReachesPrograms use) now test the same
> IsMetadataColorMode list, so the draw-prep eligibility check cannot drift
> from the draw path's early return. No behaviour change.

</details>

Changed files: `src/graphics/host_gpu/renderer/renderDraw.cpp`.

### 123. Merge draw-prep S4-S6: coherence log, read-set certificates, inline and parallel draw preparation (claude/draw-prep-s4-s6)

Commit: [`6a32195c`](https://github.com/Jetsku/KytyPS5-experimental/commit/6a32195c8bd0cf74f152fdf81c24f0a60db33b0b) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> # Conflicts:
> #	src/common/profiler.cpp
> #	src/common/profiler.h

</details>

### 124. memory: ranged coherence entry for image writeback; audit test of tracker transitions

Commit: [`16ec7af3`](https://github.com/Jetsku/KytyPS5-experimental/commit/16ec7af3ba7f6e6da85fbb20f51353c7c55280ff) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Audit of the memory-tracking/upload changes (claude/memory-uploads) against the
> draw-prep S4 invariant that every transition making guest bytes not clean for a
> backing read, or beginning/ending a backing publication, logs its range before
> the state changes:
>
> - PreserveImagesForGpuWrite (image writeback on GPU writes) bumped the verdict
>   generation before its dirty-range Add, but logged all memory. It now logs
>   the moved range with source BufferDirtyAdd, like the other Add sites. (The
>   range was never clean in between: the moved image is GPU-modified until
>   InvalidateMemoryFromGPU clears it, which logs its own entry.)
> - Everything else keeps the invariant unchanged: the other dirty-range Adds
>   (ObtainBuffer, ObtainWrittenBuffer, reached by TryWriteDataGpu and fills and
>   copies in their new order), the Subtracts, publications and image
>   transitions are hooked. Fault-ahead, hot pages (promotion, keep-hot uploads,
>   demotion, settle, sweep), unlocked written uploads and batched upload
>   recording change only CPU-dirty state, protection, tracker GPU bits (which
>   clean verdicts do not read) or command recording.
>
> memory_tracker_tests: TestCoherenceLogTrackerTransitions checks that the
> tracker's GPU-ownership transitions (mark, unmark, clearing download, readback
> unmark) log exactly their range and source, and that fault-ahead, hot-page
> promotion/upload/demotion/settle/sweep and the unlocked written upload log
> nothing and leave the other pages' GPU ownership unchanged. main() enables a
> coherence log reader so that entries are recorded.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `tests/MemoryTrackerTests.cpp`.

### 125. Texture downloads: decline unsupported layouts instead of exiting; writeback skips them up front

Commit: [`66e37f7a`](https://github.com/Jetsku/KytyPS5-experimental/commit/66e37f7a866f0095631d4b8769c0c80556d0e6ef) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The U39 correctness run stopped with 'TextureCache readback: unsupported typed tiled upload: fmt=7 tile=24 (kDepth) 960x540': the new image writeback on GPU writes (KYTY_IMAGE_WRITEBACK_ON_GPU_WRITE) tried to download a colour view of depth-tiled memory, a layout the download path does not support and for which TextureCalcUploadLayout exits. Downloads now report such layouts as invalid (TextureUploadLayoutSupported mirrors the exit conditions; uploads keep exiting), and PreserveImagesForGpuWrite requires a valid download when selecting candidates, so these images keep the previous behaviour instead of having their whole range taken into GPU ownership without their contents.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/image/textureCommon.cpp`, `src/graphics/host_gpu/renderer/image/textureCommon.h`.

### 126. CP scheduler: fence each guest frame's submissions behind the previous frame's

Commit: [`1fbc0396`](https://github.com/Jetsku/KytyPS5-experimental/commit/1fbc0396017817db860b182fe3ee1887dbdb2228) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Bounded sceAgcSuspendPoint (KYTY_AGC_DONE_MODE=bounded) lets the guest admit frame N+1 work
> while the CP still runs frame N, and graphics slices now yield to runnable async compute
> (KYTY_GFX_SLICE_DRAWS). Together they let an async-compute submission admitted after a Done
> run in the middle of the previous frame's graphics DCB. In Astro Bot (Sky Garden) a guest
> memcpy CS then overwrote the refraction scene copy between its draw and the water draw, which
> sampled the copied bytes (RenderDoc: water shader output equal to the stale texel; buffer
> hashes show the dispatch copying the other range over the written-back image): white stream,
> opaque pool with green specks.
>
> Submissions now record the Done boundary at admission (Submission::frame_fence) and the
> scheduler starts one only after every submission admitted before that Done has completed, the
> GPU-side order the idle Done gave. The guest thread still does not wait. HasRunnableComputeWork
> applies the same rule, so graphics does not yield to fenced work. A front held for over 2 s
> runs anyway (logged once). KYTY_FRAME_FENCE=0 disables. Counter: FrameEvent.FrameFenceHolds.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/guest_gpu/graphicsRun.h`.

### 127. Pipeline stutter 1/7: time every shader and pipeline compile

Commit: [`2e615800`](https://github.com/Jetsku/KytyPS5-experimental/commit/2e61580034a1af67ff4e3bb6dd7168d7664de0a9) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Compile stutter could not be attributed: only ShaderProgramMiss and
> GraphicsPipelineCreate existed, as Tracy aggregates that are recorded only
> with a connected profiler. This adds per-phase timing on the miss paths only
> (the clock is read after a cache miss, never on hits):
>
> - Program permutations: TranslateProgram, CompileProgram (specialization and
>   SPIR-V emission), spirv-val and vkCreateShaderModule, timed separately.
> - Pipelines: whole graphics/compute pipeline creation, plus
>   vkCreateGraphicsPipelines alone.
> - Per-draw stall: the compile time a draw or dispatch waited, lock waits
>   included, summed across its program and pipeline compiles. The sum is
>   flushed by the pipeline lookup that completes the draw.
> - Why each graphics pipeline was new:
>   - new: no pipeline existed for these guest shaders;
>   - permutation: the same guest shaders, but other program permutations;
>   - variant: the same program ids. For variants, the key groups that differ
>     from the closest sibling are named (rt, vi, topo, raster, cull, dbounds,
>     mask, blend, ms).
>   This shows which state is worth making dynamic or normalizing.
>
> Outputs:
> - Hang trace (KYTY_HANG_TRACE=1): compiles.csv gets one row per compile.
>   summary.csv appends these per-second columns: compile_programs,
>   compile_translate_us, compile_emit_us, compile_validate_us,
>   compile_module_us, compile_gfx_pipelines, compile_gfx_pipeline_us,
>   compile_cs_pipelines, compile_cs_pipeline_us, compile_stall_us,
>   compile_stall_max_us, compile_gfx_new, compile_gfx_perm and
>   compile_gfx_variant. Existing column indices are unchanged.
> - Tracy aggregates: FrameWait ShaderTranslate, ShaderEmit, ShaderValidate,
>   ShaderModuleCreate, GraphicsPipelineDriver and ComputePipelineCreate;
>   FrameEvent ComputePipelinesCreated. Also zones Shader::Translate,
>   Shader::Emit, Shader::Validate and Shader::CreateModule.
> - One "Compile totals" line printed when the pipeline cache is destroyed, so
>   every run reports its compile cost without extra switches.
>
> This is instrumentation only; no behaviour changes. The compile paths gain a
> few steady_clock reads, and the draw path gains one thread-local read.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`, `src/graphics/host_gpu/renderer/pipeline/shaders.cpp`.

### 128. Pipeline stutter 2/7: gate the per-compile "Shaders:" console line

Commit: [`0d2a4025`](https://github.com/Jetsku/KytyPS5-experimental/commit/0d2a40258b9190efa8750a88955bba55d4a6041e) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every new program permutation printed "Shaders: VS n | PS n | ..." with
> std::printf. Two costs came with it:
>
> - The counts were recomputed over every program source (thousands of them)
>   while the exclusive programs lock was held.
> - A line was then written to the Windows console, which costs milliseconds
>   per line when the console is visible.
>
> Both costs landed on the compiling draw, and FindSource callers had to wait
> behind the lock.
>
> Now:
> - Per-stage totals are atomic counters, bumped when a permutation is
>   published. They equal the old sums, since permutations are never removed.
> - The line is printed only when KYTY_SHADER_COUNT_LOG=1, or when
>   shader_log_direction is not Silent (shader debugging sessions keep the old
>   behaviour). KYTY_SHADER_COUNT_LOG=0 forces it off. The output format is
>   unchanged.
> - The same information is available without the console: the "Compile
>   totals" line at shutdown, and compiles.csv in the hang trace (1/7).
>
> Revert: KYTY_SHADER_COUNT_LOG=1.

</details>

Changed files: `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`.

### 129. Pipeline stutter 3/7: crash-safe periodic driver pipeline cache saves

Commit: [`3358a167`](https://github.com/Jetsku/KytyPS5-experimental/commit/3358a1674a5f4f843d1eee8edb72e44348401305) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The VkPipelineCache was written only by the clean exit
> (WindowRun -> PipelineCache::Save). A killed, crashed or stalled run
> (several last night) lost every pipeline it had compiled, so the next boot
> compiled them all again in the driver. That is the cold-compile stutter the
> user saw repeat. Common::File::RenameFile also deleted the old file before
> moving the new one into place, so a kill in between lost the whole cache.
>
> Change:
> - A PipelineCacheSaver thread wakes once a second. The command processor
>   only bumps two atomics per new pipeline.
> - A save needs at least 32 new pipelines since the last save, and no new
>   pipeline for 2 s, so it never competes with a compile burst. It then runs
>   in either of these cases:
>   - 60 s have passed since the last save;
>   - a burst has ended: 10 s without new pipelines, at least 15 s after the
>     last save. This is how level loads and area transitions are detected.
> - Fallbacks:
>   - a steady stream is saved after 3 intervals regardless of calm;
>   - fewer than 32 new pipelines are saved after 5 intervals;
>   - a failed save retries one interval later.
> - Environment switches:
>   - KYTY_PIPELINE_CACHE_SAVE_MIN_NEW, _INTERVAL_S and _QUIET_S tune the
>     rules;
>   - KYTY_PIPELINE_CACHE_SAVE=0 reverts to saving at exit only.
> - Writes are atomic: the data goes to "<file>.tmp", is flushed
>   (FlushFileBuffers), then std::filesystem::rename replaces the file in one
>   step (MoveFileExW with MOVEFILE_REPLACE_EXISTING). A kill at any point
>   leaves the old or the new complete file. Nothing deletes cache files.
> - The format, the "KytyPC2" signature (no git revision) and the 512 MiB cap
>   are unchanged. A periodic save over the cap is skipped, and periodic saves
>   stop for the rest of the session, so the last accepted file is kept. The
>   exit save still writes the full cache, which is what makes the next boot
>   start over, as before.
> - The exit save stops the saver first, and skips writing when no pipeline
>   was created since the last successful save. It then destroys the driver
>   cache as before.
> - Loading: if the cache file is missing or invalid, a complete "<file>.tmp"
>   (payload hash verified) is used instead. This recovers caches that older
>   builds lost between the delete and the rename.
>
> Vulkan synchronization:
> - vkGetPipelineCacheData has no externally synchronized parameter (vk.xml
>   declares no externsync on pipelineCache).
> - The cache is created without
>   VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT, so
>   vkCreate*Pipelines may use it concurrently: the driver synchronizes
>   internally.
> - The saver takes no emulator lock, so a draw never waits for the
>   serialization or the file I/O. At worst, a pipeline created during the
>   serialization waits inside the driver. Such pipelines are counted
>   (pcache_save_overlaps).
> - vkDestroyPipelineCache is the only externally synchronized use, and it
>   runs only after the saver thread is joined.
> - vkGetPipelineCacheData is called with 12.5% + 1 MiB of headroom, so
>   pipelines created between the size query and the copy do not force a
>   retry.
>
> Counters:
> - Each save logs one line with its size and its serialize/write times.
> - Hang trace summary.csv appends pcache_saves, pcache_save_kb,
>   pcache_save_serialize_us, pcache_save_write_us, pcache_save_overlaps and
>   pcache_save_overlap_us.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`.

### 130. Pipeline stutter 4/7: normalize pipeline keys, make cull and depth bounds dynamic

Commit: [`ef5e8fd9`](https://github.com/Jetsku/KytyPS5-experimental/commit/ef5e8fd904a8c0b1778ff8d7e893789e8adcb659) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Pipeline keys held state that cannot change the created pipeline, so draws
> differing only in such state compiled identical pipelines.
>
> Normalization (KYTY_PIPELINE_KEY_NORMALIZE, default on; =0 reverts). Each
> zeroed field, and why it is unused (Vulkan spec plus CreatePipelineInternal):
> - Blend factors and ops of an attachment with blending disabled:
>   VkPipelineColorBlendAttachmentState ignores src/dst factors and ops when
>   blendEnable is VK_FALSE. 0 maps to BlendFactor::kZero / BlendOp::kAdd,
>   which GetBlendFactor/GetBlendOp accept. (Before, a garbage factor register
>   with blending off could even EXIT in GetBlendFactor.)
> - Alpha factors and op with separate alpha blending off:
>   CreatePipelineInternal copies the color factors and op into the alpha ones
>   and never reads these registers.
> - The depth-bounds test enable without a depth/stencil attachment:
>   pDepthStencilState is null for such pipelines.
> - Depth-bounds min/max while the test is disabled: they are inputs of that
>   test only. Before, every distinct DB_DEPTH_BOUNDS value created a new
>   pipeline even with the test off.
>
> Dynamic state (KYTY_PIPELINE_DYNAMIC_STATE, default on; =0 reverts):
> - Cull mode, front face (core Vulkan 1.3 extended dynamic state, no feature
>   bit), depth-bounds test enable (core 1.3) and depth bounds (core 1.0;
>   depthBounds is already enabled) become dynamic state of every renderer
>   pipeline, and are zeroed in the key.
> - SetGraphicsDynamicParams records them per draw from the same registers,
>   with the same rule: no culling for rect lists. The per-command-buffer
>   dynamic-state shadow elides unchanged values.
> - The polygon mode stays static. It is still derived from the real cull bits
>   before they leave the key.
> - macOS keeps depth bounds static (MoltenVK has no depthBounds).
>
> Exactness:
> - Dynamic and static state with equal values define the same rendering
>   (Vulkan spec).
> - Every renderer pipeline declares the same dynamic states, so the shadow's
>   rule still holds: its values stay valid while the same renderer pipeline
>   is bound in the same command buffer. Any other graphics pipeline bind
>   (blits) still forces a full re-record.
>
> Not normalized: the clip-space viewport transform in the vertex-program key
> (shader.cpp BuildStageStaticKey). ConvertPositionToClipSpace bakes scale,
> offset and half extent into the SPIR-V as constants, so different values are
> different shaders. Moving them to push constants would also turn the
> power-of-two division into a runtime division, whose result is not
> guaranteed bit-identical.
>
> Not done here, but candidates if the new compiles.csv "variant" details show
> them:
> - dynamic primitive topology: needs mesh pipelines' dynamic-state set to be
>   handled separately;
> - dynamic vertex binding stride: needs vkCmdBindVertexBuffers2 in the draw
>   path;
> - VK_EXT_extended_dynamic_state3 blend and color mask.

</details>

Changed files: `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`, `src/graphics/host_gpu/renderer/pipeline/shaders.cpp`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`.

### 131. Pipeline stutter 7/7: compile programs outside the exclusive programs lock

Commit: [`984d013a`](https://github.com/Jetsku/KytyPS5-experimental/commit/984d013a5b9f168db55158d70a80d754a393dc90) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> CompileAndPublish held m_programs_mutex exclusively from its lookup through
> translation, SPIR-V emission, spirv-val and vkCreateShaderModule. That is
> milliseconds to tens of milliseconds per new permutation. Every FindSource
> (shared lock) on any other thread blocked for the whole compile. That thread
> might be the draw-prep workers that draw-prep S4-S6 adds, or the
> overflow-permutation lookups. All compiles were also serialized.
>
> Now the exclusive lock covers only four short steps:
> - the lookup;
> - registering the compile in `in_flight`;
> - inserting a new source;
> - publishing the permutation.
>
> Translation, resource-plan extraction, materialization, emission, validation
> and module creation all run unlocked.
>
> - In-flight records live on the compiling thread's stack and are guarded by
>   m_programs_mutex.
>   - While a new source is being translated, its record covers every request
>     for that ProgramKey.
>   - Once the source is inserted, the record covers every permutation of that
>     source until its specialization is known. From then on it covers only
>     the (specialization, push cursor) being compiled.
>   - Another thread that needs a covered source or permutation waits on
>     compile_done and repeats its lookup. It does not compile a second copy.
>     Independent compiles proceed in parallel.
>   - An RAII scope unregisters the record on every exit, so no waiter can see
>     a dead record.
> - Publication re-runs FindPermutation under the lock. An equal permutation
>   can appear meanwhile only when two cursors map to the same push-data start
>   (both NoStart). In that case the new copy's module is destroyed and the
>   published one is returned. Permutations therefore stay unique per
>   (specialization, push start), which the per-thread LookupMemo exactness
>   argument relies on.
> - next_shader_id is now atomic, because ids are assigned by unlocked
>   compiles.
>
> Why this is exact:
> - The translated source and every compiled permutation are the same
>   functions of the same inputs as before. Only the lock scope and the dedup
>   mechanism changed.
> - Materialization was already documented as thread-safe on a sealed plan.
>   Get already ran it unlocked. Reuse-mode state stays under m_reuse_mutex,
>   which its callers hold.
>
> Lock order:
> - Unchanged, and no mutex was added: m_reuse_mutex (reuse mode only, held
>   across the whole Get) comes before m_programs_mutex.
> - A thread never waits while it owns an in-flight record, so waits cannot
>   form a cycle. Waiting releases m_programs_mutex.
> - In reuse mode, m_reuse_mutex serializes all preparation, so nothing ever
>   waits there.
>
> Today only the command-processor thread compiles, so a single-threaded run
> behaves exactly as before. The gain is for other threads' FindSource calls
> during a compile.
>
> Counters: FrameEvent ProgramCompileWaits (requests that waited for another
> thread's compile) and ProgramCompileDuplicates (compiles dropped at
> publication). The waits count in the per-draw compile stall (1/7).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`.

### 132. Pipeline caching: deterministic SPIR-V ids for split-wave64 dispatcher spills

Commit: [`4ea52850`](https://github.com/Jetsku/KytyPS5-experimental/commit/4ea52850fedec281a788e6a9a397d710aaa5ed3f) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> In dispatcher-fallback programs compiled with lane_count 2 (split wave64:
> compute and mesh shaders on 32-wide NVIDIA subgroups), the high half's
> spill variable ids came from iterating an unordered_map keyed by IR::Inst
> pointers. That order depends on instruction addresses, so the same guest
> shader emitted differently numbered SPIR-V on every run.
>
> VkPipelineCache and the NVIDIA driver's shader cache key on the SPIR-V
> bytes. Such programs therefore missed both caches on every boot, and on
> every re-translation.
>
> The high half now allocates its ids in the insertion order of spills[0],
> which is program order (blocks, then instructions, as before). Which
> instructions spill, and the low half's ids, are unchanged. The output
> differs from before only in the numbering of those ids, so the semantics
> are identical.
>
> Found while checking that translation reuse (5/7) emits bit-identical
> SPIR-V. This is shader-codegen territory: flagged for the codegen owner.

</details>

Changed files: `src/graphics/shader/recompiler/backend/spirv/spirvEmitterProgram.cpp`.

### 133. Pipeline stutter 5/7: translate each program source once, specialize copies

Commit: [`7a830c91`](https://github.com/Jetsku/KytyPS5-experimental/commit/7a830c91c945ca8d4e17fcff7314d09bf9cb28d7) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every new permutation of an existing program source ran TranslateProgram on
> the guest code again: decode, CFG build and structurization, IR translation,
> SSA, constant propagation, DCE, SRT planning and resource tracking. Only then
> did CompileProgram specialize it. Permutations come from
> (ResourceSpecialization, push-data start). The VS push start depends on the
> paired PS, so one VS is compiled once per PS shader-data size it meets.
>
> Now each source keeps its first translation, unmodified. Each further
> permutation specializes a deep copy of it instead of translating again.
> Emission, validation and module creation still run per permutation.
>
> Why this is exact:
> - TranslateProgram is a function of the guest code and the ProgramKey fields.
>   The static key covers every input-info field translation reads: pixel
>   system-input registers, scratch size, mesh/tess/compute parameters, and the
>   V# format and swizzle for embedded fetch. That same property is what
>   already makes reusing a source's permutations across draws exact.
> - The kept copy is taken right after translation, before plan extraction or
>   specialization touch the program.
> - The copy is a new function, IR::CloneProgram (ir/ProgramClone.cpp), a
>   member-by-member deep copy. It keeps:
>   - every block and instruction in storage and list order;
>   - opcode, flags, arguments and phi blocks;
>   - use lists in their exact order (copied, not rebuilt);
>   - evaluation indices and all 28 ResourcePlan and 14 Program members.
>   Every Value, Inst* and Block* is remapped into the copy.
> - Only the per-block SSA scratch arrays are left out. RewriteToSsa is their
>   only reader, and it runs inside TranslateProgram. After that they can name
>   erased instructions, so they could not be remapped anyway.
> - A program that references an instruction or block it does not own is
>   refused, and the cache translates as before.
> - Static layout checks of Inst, Block, ResourcePlan, Program and the copied
>   member types fail the build when a member is added, so the copy cannot
>   silently fall behind the IR.
> - The shader tests now compile every compute (356) and graphics (17) case a
>   second time from a copied translation. They require bit-identical SPIR-V
>   and equal shader metadata (info, bindings, write ranges, scratch). All
>   cases pass. Also verified with the isa-accuracy, tessellation, wave64,
>   dpp, indirect-image, centroid, shader-data-storage and position-w modes.
> - The runtime check KYTY_TRANSLATION_CACHE_VERIFY=1 compiles every reused
>   permutation both ways and compares the SPIR-V and metadata. On a
>   difference it logs, uses the fresh translation, and stops reusing that
>   source; "exit" stops the emulator instead.
>
> Memory:
> - Kept translations are bounded by KYTY_TRANSLATION_CACHE_MB (default 256)
>   of estimated IR, with least-recently-used eviction.
> - Readers hold a shared_ptr, so eviction never frees a program that is
>   being copied.
> - Reads take m_programs_mutex only to fetch the pointer. The copy runs
>   unlocked, inside the in-flight scope from 7/7.
>
> Switches:
> - KYTY_TRANSLATION_CACHE=0 translates every permutation from the guest code
>   as before.
> - KYTY_TRANSLATION_CACHE_VERIFY=1 or =exit, described above.
>
> Counters:
> - FrameEvent TranslationReuses and TranslationVerifyMismatches.
> - Hang trace summary.csv appends compile_translation_reuses and
>   compile_clone_us. compiles.csv appends clone_us.
> - The compiles.csv detail column for programs names why the permutation was
>   needed: first, push (same specialization, another push-data start) or
>   spec, plus "+reused".
> - The "Compile totals" line reports reused translations and copy time.
>
> Tests: the CommandScheduler calls in ShaderRecompilerComputeTests.cpp now
> pass CommandScheduler::Role::Guest. The constructor gained that parameter
> earlier, and the test binary no longer compiled without it. Separately, four
> modes still overflow the 1 MiB test stack: descriptor-heap, mapped-range,
> packed-texture and push-constant-bank. Each constructs a RenderContext on
> the stack, whose frame is about 2.5 MiB. That is independent of this change,
> which adds 16 bytes to PipelineCache.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/shader/recompiler/ir/Block.h`, `src/graphics/shader/recompiler/ir/ProgramClone.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/Value.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 134. Pipeline stutter: validate SPIR-V on a background thread

Commit: [`0f4b7cab`](https://github.com/Jetsku/KytyPS5-experimental/commit/0f4b7cab165168cdb4ccc79a5cb3c77990d58517) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> With shader_validation_enabled (on in the user's Kyty.ini, and forced on by
> the comparison launcher unless -NoShaderValidation is given), every new
> program permutation ran spirv-val on the compiling draw's thread before
> vkCreateShaderModule.
>
> Measured offline with SPIRV-Tools, run the same way as ValidateShaderSpirv,
> over the 248 SPIR-V modules dumped from Astro Bot in
> Profiling\analysis\guest-shader-diagnostic:
> - total 2.9 s, mean 11.7 ms, median 5.2 ms per module;
> - up to 50-235 ms for the 0.5-1.45 MB compute modules.
>
> Validation time grows with module size. The mip-statistics instrumentation
> added code to every sampling pixel shader, which raised it. That fits the
> reported doubling of compile hitches.
>
> Now the compile path creates the module and hands the words to a
> SpirvValidator thread (moved, not copied). The validator:
> - checks every module, in submission order;
> - logs the same messages and dumps the module as before;
> - stops the emulator with the same EXIT message on the first invalid
>   module.
> Only the timing of the report changes. An invalid module can now reach the
> driver before the report. Use KYTY_SHADER_VALIDATION_ASYNC=0 to restore
> synchronous validation when chasing a codegen bug. Rendering is not
> affected. Modules still queued at shutdown are not validated.
>
> Counters: Tracy FrameWait ShaderValidate (validator-thread time), hang trace
> summary.csv appends validate_async_count and validate_async_us, and the
> "Compile totals" line reports background validations. compile_validate_us
> stays the synchronous time (0 in async mode).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`.

### 135. Pipeline stutter 4/7 follow-up: record dynamic raster state in the rasterization test

Commit: [`7fb56d0b`](https://github.com/Jetsku/KytyPS5-experimental/commit/7fb56d0b20abd825dceefb119d7d705a3921dce6) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> CheckRasterization draws with pipelines from PipelineCache::GetGraphicsPipeline,
> but it records the dynamic state itself. Since 4/7, cull mode, front face and
> the depth-bounds state are dynamic in every renderer pipeline
> (KYTY_PIPELINE_DYNAMIC_STATE). The test's draws therefore left them unset, and
> its front/back stencil checks depend on the front face.
>
> The draw now records them the way SetGraphicsDynamicParams does, from the
> same ModeControl and RenderDepthInfo, whenever the switch is on.
>
> --polygon-mode-only passes with this change. It had to be run with the stack
> reserve raised by editbin: the unmodified test binary overflows its 1 MiB
> stack before reaching the draws. That overflow predates this branch.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 136. Pipeline stutter 6/7: graphics pipeline libraries behind KYTY_PIPELINE_LIBRARY (off)

Commit: [`b9b15f50`](https://github.com/Jetsku/KytyPS5-experimental/commit/b9b15f505dfe8dbadd0df7978e9aa336232a43e7) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A new graphics pipeline is one synchronous vkCreateGraphicsPipelines on the
> command processor that compiles every stage, even when only blend, format or
> depth state changed. With VK_EXT_graphics_pipeline_library the four parts of a
> pipeline are created separately, kept and reused:
> - vertex input interface;
> - pre-rasterization shaders;
> - fragment shader;
> - fragment output interface.
> A new pipeline is then a fast link of the four, with no link-time
> optimization. The first pipeline of a new shader still compiles its shader
> libraries. A later pipeline that differs only in output or vertex-input state
> needs only the cheap interface libraries and the link.
>
> Path (pipelineLibrary.h/.cpp, hooked into CreatePipelineInternal):
> - CreatePipelineInternal builds the same create info as before and hands it to
>   the hook, with a signature of the pipeline layout's definition.
> - GraphicsPipelineSnapshot::Capture deep-copies the create info. It accepts
>   only the structures the renderer uses:
>   - dynamic rendering info;
>   - depth clip control, depth clip enable and provoking vertex;
>   - color write enables;
>   - one vertex shader and optionally one fragment shader.
>   Anything else returns null and the pipeline is created monolithically,
>   exactly as before. That covers mesh, tessellation and rect-list (TCS/TES)
>   pipelines, flags, specialization and unknown pNext structures.
> - Probe: with pipelineCreationCacheControl, the monolithic create info is
>   first tried with FAIL_ON_PIPELINE_COMPILE_REQUIRED. A pipeline the driver
>   cache already holds is used directly, so warm runs keep the same monolithic
>   pipelines as without libraries. A standalone check on the RTX 3090 driver
>   confirmed the semantics: VK_PIPELINE_COMPILE_REQUIRED for a never-seen
>   pipeline, success after a compile, and success in a new process from the
>   disk cache.
> - Otherwise the four libraries are looked up or created, then fast-linked.
>   - Each library is keyed by the exact field values of the state it consumes.
>     The shader libraries' keys also include the module handle and the layout
>     signature, because linking requires identically defined layouts.
>     Program modules are never destroyed before the cache is, so handles are
>     not reused. Rect-list modules are destroyed, but those pipelines never
>     take this path.
>   - The fragment shader library always gets a depth/stencil state. The
>     default one tests nothing, which matches a pipeline without a depth
>     attachment.
>   - The pre-rasterization and fragment shader libraries carry only the view
>     mask in their rendering info.
>   - Every library gets the full dynamic-state list. Implementations ignore
>     states outside a library's subset.
> - Background optimize (KYTY_PIPELINE_LIBRARY_OPTIMIZE, default 1): the
>   snapshot is compiled monolithically on 1-8 worker threads
>   (KYTY_PIPELINE_LIBRARY_THREADS, default 2). The snapshot is the create info
>   the renderer would have used, so the result is the pipeline the default path
>   creates, and the driver cache then holds it for the next run's probe.
>   ReplaceLinkedPipeline swaps it in under m_mutex:
>   - It installs a new Pipeline object instead of mutating the handle.
>   - The old object moves to a retired list that lives until the cache is
>     destroyed. A thread that still holds the old reference, or a recorded
>     command buffer, keeps a valid pipeline.
>   - m_pipeline_generation is bumped, so every per-thread memo misses and
>     looks up again.
>
> Lifetime and locking:
> - The library cache is used only under PipelineCache::m_mutex.
> - Lock order is m_mutex -> LibraryState::mutex (Enqueue). Workers take them
>   one at a time.
> - Save() stops and joins the workers before it takes m_mutex and destroys the
>   driver cache. Jobs queued after that are dropped, and their linked pipelines
>   stay.
> - The destructor destroys, in order: the live pipelines and layouts, then the
>   retired pipeline handles (their layouts are shared with the live entries),
>   then the libraries, then the driver cache.
>
> Memo hardening: m_pipeline_generation now starts in a range of its own for
> each PipelineCache instance, (instance + 1) << 32. A thread_local
> last-pipeline memo left by a destroyed cache can therefore never match a new
> cache allocated at the same address. Tests create several render contexts
> per process. This fixes a latent use-after-free in KYTY_PIPELINE_MEMO that
> predates this commit.
>
> Device: VK_KHR_pipeline_library, VK_EXT_graphics_pipeline_library and the
> graphicsPipelineLibrary feature are enabled only when KYTY_PIPELINE_LIBRARY is
> set, and the path is used only with graphicsPipelineLibraryFastLinking.
> pipelineCreationCacheControl is also enabled only then. The default device and
> default pipeline creation are unchanged.
>
> Why it is off by default (KYTY_PIPELINE_LIBRARY=1 enables it):
> - The recompiler does not decorate the position output Invariant. A
>   fast-linked pipeline is compiled without whole-pipeline optimization, so it
>   is not guaranteed to produce bit-identical positions to the optimized build
>   that replaces it, or to other monolithic pipelines that share the vertex
>   shader. A pass that depth-tests EQUAL against an earlier pass could then
>   lose fragments while a linked pipeline is in use. This cannot be verified
>   without running the game. DXVK runs GPL with invariant positions.
> - On NVIDIA, enabling the extension makes vkCreateShaderModule compile
>   eagerly. In the harness, module time rose from 0.0 to about 7 ms for 12
>   programs. That would shift the compile cost profile under other agents'
>   measurements.
> Turning it on by default would be a one-line change in
> PipelineLibraryRequested() once runtime testing shows no flicker. An
> Invariant position decoration in codegen would remove the first risk.
>
> Switches:
> - KYTY_PIPELINE_LIBRARY=1: enable.
> - KYTY_PIPELINE_LIBRARY_OPTIMIZE=0: keep the linked pipelines and never swap.
> - KYTY_PIPELINE_LIBRARY_THREADS=N: background compile threads (1-8,
>   default 2).
> - KYTY_PIPELINE_LIBRARY_PROBE=0: skip the driver-cache probe and link even
>   cached pipelines. This measures the cold path on a warm machine.
>
> Counters:
> - Log line "Pipeline libraries: N linked (ms), N driver-cache hits,
>   N monolithic, N libraries (ms), N optimized in background (ms)".
> - summary.csv (hang trace, appended columns): gpl_cache_hits, gpl_links,
>   gpl_link_us, gpl_libraries, gpl_library_us, gpl_optimized,
>   gpl_optimize_us.
> - compiles.csv detail per pipeline: gpl-linked / gpl-cache-hit /
>   gpl-monolithic, plus libs=N and the fallback reason (ineligible,
>   probe-failed, library-failed, link-failed).
> - Tracy aggregate: FrameEvent PipelineLibraryLinks, PipelineLibraryCacheHits
>   and PipelineLibrariesCreated; FrameWait PipelineOptimize.
>
> Tests: the harness enables the extension, feature and cache control when
> KYTY_PIPELINE_LIBRARY is set. Runs of --polygon-mode-only (polygon modes,
> provoking vertex, blending, stencil operations, packed vertex fetch) under
> VK_LAYER_KHRONOS_validation:
> - default: pass.
> - GPL with probe: 14 of 15 pipelines were driver-cache hits. Pass.
> - GPL with PROBE=0 and OPTIMIZE=0: 14 linked from 26 libraries, 1
>   monolithic (the ineligible rect-list pipeline). Pass, with exact pixel
>   comparisons.
> - GPL with PROBE=0 and OPTIMIZE=1: 14 optimized in background and swapped.
>   Every rendering check passes. Only the harness's pipeline-handle identity
>   asserts fail, as expected after a swap; that was checked with a local,
>   uncommitted relaxation.
> No new VUIDs appeared. The extra independentBlend report comes from the
> fragment output library's copy of the same blend state, a pre-existing
> harness feature gap. Twelve other test modes give identical output with and
> without GPL. Of those, only --image-overlap-only creates graphics pipelines
> (14 linked). It stops at the same pre-existing bufferCache.cpp:1313
> assertion in both runs.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/graphicContext.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineLibrary.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineLibrary.h`, `src/graphics/host_gpu/renderer/pipeline/shaders.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 137. Merge codex/astro-profile (66e37f7a) into claude/pipeline-stutter

Commit: [`5f63826c`](https://github.com/Jetsku/KytyPS5-experimental/commit/5f63826c45307c262cbc29dab4acba9b1eb0b61f) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Brings in, since the branch point dbbd5055:
> - binding path: interned pipeline layouts, descriptor-set reuse,
>   push-constant shadow, texture binding memo;
> - draw-prep S4-S6: speculative program preparation;
> - shader codegen: MOVREL ranges, min/max, PKRTZ, MAD modes with an
>   Invariant position, interpolation modes;
> - submission/sync;
> - memory tracking and uploads;
> - texture streaming/residency;
> - occlusion/label deferral;
> - Vulkan validation log mode.
>
> Conflicts resolved:
> - common/hangTrace.h, hangTrace.cpp (summary.csv columns):
>   - Both sides appended columns after gpu_draw_write_sinks. mainline's
>     mem_* columns keep their positions and indices. The compile, pcache,
>     translation-reuse, async-validation and gpl columns follow them.
>   - Memory counters added to MemoryCounter from now on are emitted at the
>     end of the row (kMemoryColumnsBeforeCompile), so no existing column
>     index ever moves.
>   - compiles.csv is unchanged.
> - common/profiler.h, profiler.cpp: mainline's FrameEvent and FrameWait
>   entries first, then this branch's. The name tables are checked entry by
>   entry against the enums (292 events, 41 waits).
> - pipeline/pipelineCache.cpp:
>   - Includes both headers.
>   - Keeps draw-prep's TryPrepareSpeculative, together with this branch's
>     ProgramCache constructor (the background SPIR-V validator).
>   - Destructor order:
>     1. live pipelines (ReleasePipelineLayout: interned layouts are not
>        destroyed there);
>     2. the handles of GPL-retired linked pipelines, which never release
>        layouts (they are copies of the live entries);
>     3. the pipeline libraries;
>     4. DestroyInternedPipelineLayouts;
>     5. the driver cache.
>     Libraries therefore go after every pipeline linked from them and
>     before the layouts they were created with.
> - tests/ShaderRecompilerComputeTests.cpp: the harness keeps mainline's
>   optional VK_EXT_robustness2 (robustBufferAccess2), now as a
>   vector-built extension list. The GPL feature struct (KYTY_PIPELINE_LIBRARY
>   only) is chained in front of whatever the chain already holds, so it no
>   longer drops the robustness2 struct.
>
> Semantic adjustments:
> - GPL library keys (pipeline/shaders.cpp) now use the binding path's
>   interned layout identity.
>   - With KYTY_LAYOUT_INTERN (default), pipelines with equal
>     MakePipelineLayoutSignature results get the same layout handles.
>     Libraries are keyed by that canonical signature, so they are shared
>     between pipelines whose bindings differ only in order.
>   - With interning off, the ordered binding list is used, as before.
>   - A leading word tells the two apart.
> - Draw-prep. Checked, and nothing needed changing:
>   - Compiles outside the exclusive lock (7/7) only register in-flight
>     records.
>   - A source is inserted into `programs` with its sealed resource plan
>     after translation. A permutation is appended (release-store publish)
>     only after its module exists, and the publish re-checks uniqueness.
>   - TryPrepareSpeculative (FindSourceMemo/FindPermutationMemo) never
>     consults in_flight and never waits on compile_done. A source or
>     permutation still being compiled reads as NotPublished.
>   - The serial and speculative lookups search the same unique
>     (specialization, push start) entries.
> - Codegen. Checked, and nothing needed changing:
>   - IR::CloneProgram's layout static_asserts still hold. The new
>     GetAttributeWithBary/FPMad32 opcodes and the InterpolationMode flag
>     are ordinary opcode and flag values, which the clone already copies.
>   - The new pixel-input I/J VGPRs are in BuildStageStaticKey, which this
>     branch does not touch. Pipeline-key normalization (4/7) only
>     zeroes blend and depth-bounds fields.
>   - The spill-id determinism fix (spirvEmitterProgram.cpp) is untouched
>     by the emitter changes. Their new pointer-keyed containers
>     (position_slice, input_interpolation, the value-set memo) are only
>     used for lookups, never iterated.
> - Shutdown: PipelineCache::Save() still stops the cache saver and the GPL
>   optimizer threads before the driver cache is destroyed. ~PipelineCache
>   runs from render_context.reset(), before device destruction. Neither
>   thread touches the command scheduler or its runner.

</details>

### 138. Pipeline stutter 5/7 follow-up: free evicted translations outside the programs lock

Commit: [`980a9e52`](https://github.com/Jetsku/KytyPS5-experimental/commit/980a9e52445dd44857c7ef5d3db3a57673547e52) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A kept translation (5/7) is a full IR program. Freeing one takes long
> enough to matter: thousands of instructions, blocks and use lists. Two
> paths did it while holding m_programs_mutex exclusively:
> - the LRU eviction in KeepTranslation;
> - dropping a copy that lost the race to be kept.
>
> Draw-prep S5/S6 workers look sources up with shared locks
> (FindSource/FindPermutation overflow). They could therefore wait behind a
> free that has nothing to do with them. Draw-prep workers must never wait
> on another thread's compile work.
>
> Now KeepTranslation and ForgetTranslation move whatever they drop into a
> DroppedTranslations list owned by CompileAndPublish:
> - The list is declared before the unique_lock, so it is destroyed only
>   after the lock is released, on every return path.
> - The new-source path clears it right after unlocking.
> - The local reference used for copying a kept translation is released
>   before the lock is taken again.
>
> The exclusive sections now hold only map, LRU and in-flight bookkeeping.
>
> No behaviour change: the same translations are kept, evicted and reused,
> and CompileAndPublish's stall accounting still covers the frees.

</details>

Changed files: `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`.

### 139. Pipeline stutter 6/7 follow-up: GPL rationale after the codegen merge

Commit: [`02b77a6e`](https://github.com/Jetsku/KytyPS5-experimental/commit/02b77a6e678285147b3445de38453baadc0ca37e) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The merged shader codegen (KYTY_MAD_MODE=position, the default) computes
> the position without contraction and decorates it Invariant. That removes
> the main reason graphics pipeline libraries were off by default. A
> fast-linked pipeline and the optimized build that replaces it now produce
> bit-identical positions, so a pass that depth-tests EQUAL against an
> earlier pass keeps its fragments.
>
> pipelineLibrary.h now says so, together with what still holds:
> - Other outputs, whose pixel-shader math keeps FMA contraction, may differ
>   in the last bits while a linked pipeline renders.
> - Which frames those are depends on background compile timing, so runs
>   with libraries are not pixel-reproducible.
> - NVIDIA compiles shader modules at creation once the extension is
>   enabled.
> - Libraries are keyed by the interned layout signature, or by the ordered
>   binding list without interning.
>
> PipelineCache logs a warning when libraries are enabled together with
> KYTY_MAD_MODE=fused. Fused mode drops the Invariant decoration, so a
> linked pipeline and its optimized build may rasterize different depths.
>
> The default stays off: none of this has run in the game yet, and
> reproducible frames matter for the A/B comparisons. Turning it on is a
> one-line change in PipelineLibraryRequested() once a KYTY_PIPELINE_LIBRARY=1
> run shows no visual regressions and fewer compile hitches (gpl_* and
> compile_stall_* columns). Also wraps one line of the GPL layout signature
> to the column limit.

</details>

Changed files: `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineLibrary.h`, `src/graphics/host_gpu/renderer/pipeline/shaders.cpp`.

### 140. tests: keep the pixel shader's user data alive in CompilePixelShader

Commit: [`6bc00d6f`](https://github.com/Jetsku/KytyPS5-experimental/commit/6bc00d6f7e2b67733a2ef933a6718037ffcab8a2) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> CompilePixelShader (ShaderCodegenTests.inc) assigned
> MakeNativeUserData(nullptr), a temporary std::array, to
> CompileOptions::user_data, which is a std::span. The array was destroyed at
> the end of that statement, so TranslateProgram, the materialization runtime
> and CompileProgram all read freed stack memory. clang flagged it with
> -Wdangling-assignment-gsl. The array now lives in a local for the whole
> compile.
>
> Test-only; no emulator code changes.

</details>

Changed files: `tests/ShaderCodegenTests.inc`.

### 141. Merge pipeline compile-stutter fixes: shader count log, pipeline cache save, pipeline key normalization, dynamic state, translation cache, async SPIR-V validation, optional GPL (claude/pipeline-stutter)

Commit: [`19f6264e`](https://github.com/Jetsku/KytyPS5-experimental/commit/19f6264ed7c6070ae22973e55adc0d57e40e795c) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> GPU suite on the merged branch: --polygon-mode-only (default, GPL linked, GPL driver cache),
> --cases-only (374, copied translations bit-identical), codegen modes and 12 draw modes pass;
> only the known --image-overlap-only assert remains. GPL stays off by default.

</details>

### 142. tests: compare only defined detile bytes in GpuTilerCpuParity

Commit: [`06b11822`](https://github.com/Jetsku/KytyPS5-experimental/commit/06b11822716175b27d0af04977fe0e7fac9ce697) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> GpuTilerCpuParity failed with "detile bytes family=0 bpe=2: first mismatch
> at 102 of 4096". Byte 102 is element x=51 of row 0 in a 51-element-wide
> Standard256B 16bpp fixture with a 64-element pitch: row padding, not a
> texel.
>
> Classification: stale expectation. 78935a08 (texture streaming) stopped
> clearing the detile scratch buffer by default (KYTY_TILER_CLEAR_SCRATCH=1
> restores the fill). The test still compared the whole linear buffer
> against a zero-initialised CPU reference, so it depended on the old fill
> of row, slice and level padding.
>
> The padding is never read. Every consumer of TileManager::Detile reads
> exactly the elements the dispatches write:
> - UploadImage, TryAsyncFullUpload and the partial-band refresh issue
>   buffer->image copies whose extent and row length are the tile infos'
>   width/height/pitch. Both come from TextureBuildImageCopies and
>   TextureBuildGpuTileInfos; partial bands shrink both.
> - Depth uploads copy extent.width x extent.height at info.pitch, and
>   ConvertD16 converts the same rows.
> - SwapBgra16 transforms padding too, but its output feeds the same
>   copies.
>
> The detile checks now record which linear bytes the CPU reference defines
> and compare only those. Tile-direction checks still compare every byte
> of the prefilled tiled buffer, so a stray write into guest memory is
> still caught. The scheduler-owned reuse check now detiles different
> input the second time. Otherwise a dispatch that wrote nothing would
> pass, because the recycled scratch still held the first result.
>
> With this change the GPU detile and tile paths match the CPU reference
> on every defined byte:
> - all 9 block families x all admitted bytes-per-element (330 cases),
> - 236 format/tile-mode pairs, including every BCn format,
> - 2D and 3D mip tails, arrays with surface Z, odd multi-mip strides,
>   and RT/depth volume slices.
> The tiler shaders have no product bug.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 143. tests: heap-allocate per-check RenderContexts

Commit: [`348b703b`](https://github.com/Jetsku/KytyPS5-experimental/commit/348b703bc33acbe103e5ffa0e901fb043a4f18c7) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The default shader_recompiler_compute_tests run died with
> STATUS_STACK_OVERFLOW (0xc00000fd), as did 18 selectors, among them
> --descriptor-heap-only, --image-overlap-only, --buffer-cache-gc-only and
> --sampled-depth-resource-only.
>
> Classification: harness bug. 23 checks built a private RenderContext on
> the main-thread stack, which is 1 MiB on Windows. RenderContext holds its
> RenderExecutor inline, and RenderExecutor::m_texture_descriptions has
> 4096 entries of about 0.6 KB each, so one RenderContext is several MiB.
> The product allocates its RenderContext on the heap.
>
> VulkanHarness::MakeRenderContext() now returns a std::unique_ptr, and
> each check binds `auto &context = *context_owner;`. The rest of each
> check's code, and its construction and destruction order, are unchanged.
> The linker stack size is unchanged and so is product code.
>
> With this change, 10 selectors that only overflowed now pass:
> packed-texture, descriptor-heap, push-constant-bank, mapped-range,
> stream-buffer, image-exact-lookup, polygon-mode, dcc-clear,
> layered-image and standard-tile-rt.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 144. tests: check that tile dispatches and image copies cover the same bytes

Commit: [`45fe3fc4`](https://github.com/Jetsku/KytyPS5-experimental/commit/45fe3fc4b9ff079436b44b44b2f10ff67233d317) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> TileManager::Detile has not cleared its linear scratch since 78935a08
> (KYTY_TILER_CLEAR_SCRATCH=1 brings the clear back). TileImage never
> cleared the scratch it downloads the image into. Both are correct only
> if a tiled transfer's buffer<->image copies touch exactly the linear
> bytes its tile dispatches touch:
> - If an upload copy read a byte that no detile dispatch wrote, the
>   image would get stale scratch contents.
> - If a tile dispatch read a byte that no download copy wrote, stale
>   scratch would land in guest memory.
>
> The new host-only check, CheckDetileCopyCoverage, runs in the default
> suite and under --gpu-tiler-only. It builds transfers with the same
> functions TextureCache::BuildTextureTransfer uses:
> TextureCalcUploadLayout, TextureBuildImageCopies and
> TextureBuildGpuTileInfos. It covers:
> - every non-FMASK format,
> - Standard256B/4KB/64KB, PRT, RenderTarget and Depth tile modes,
> - single levels, full mip chains, mip tails, arrays and volumes,
> - odd extents and 1x1 surfaces.
> For each transfer it compares the bytes the dispatches write
> (width x height x depth elements at pitch and slice stride) against the
> bytes the copies read, using Vulkan's buffer addressing in texel blocks
> (bufferRowLength and bufferImageHeight, rounded up for BC). It also
> checks every resident-level subset that RestrictToResidentLevels
> uploads for partially resident 2D chains, re-based as in the product.
>
> Result: 2418 transfers and 4438 resident subsets, and dispatches and
> copies cover identical bytes in every case.
>
> Paths the check does not build, reviewed by hand:
> - Depth upload and download (BuildDepthTiles and BuildDepthCopies) and
>   the D16<->D32 conversions: tiles, copies and ConvertD16 all use
>   extent.width x extent.height at info.pitch per layer.
> - Partial-refresh bands: each band keeps its source region's width and
>   row length. Its element rows [y0, y1) map to texel rows
>   [y0 * th, min(y1 * th, H)), which is exactly the band tile's
>   y1 - y0 rows.
> - SwapBgra16 on detile output and inside TileImage: it transforms the
>   whole scratch, padding included, but its output feeds the same
>   copies and dispatches.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 145. tests: run GC and image downloads on the guest GPU thread

Commit: [`7cc02637`](https://github.com/Jetsku/KytyPS5-experimental/commit/7cc02637ceeb7191849a856ec2c2afef5dae2adb) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Six selectors (image-overlap, image-pressure-retirement,
> compute-meta-clear, buffer-cache-gc, sampled-depth-resource and
> storage-mip-host) died on
>
>   EXIT_IF(!GuestGpu::IsGpuThread() || ranges.empty())
>   in BufferCache::BeginBackingPublication.
>
> Stacks from a /DEBUG relink under cdb show the entry points:
> - BufferCache::RunGarbageCollector -> DownloadBufferMemory
>   (CheckBufferCacheDirtyGarbageCollection);
> - TextureCache::RunPressureGarbageCollector -> DownloadImageMemory
>   (CheckImagePressureReadback);
> - TextureCacheTestAccess::TryDownload -> DownloadImageMemory (color
>   volume, BGRA16, comparison depth, depth-tile and stencil-binding
>   checks);
> - TextureCache::ProcessDownloadImages -> DownloadImageMemory
>   (stencil-binding and unified-cache checks).
> Every one was called directly from the test's main thread.
>
> Classification: harness bug. The assertion is intentional. It arrived
> with the backing-publication ordering in 6a7f10ee: publications must
> begin on the thread that records the guest command stream. In the
> product, every caller runs there. RenderContext::RunGarbageCollector
> (ProcessDownloadImages, texture GC with its pressure collector, buffer
> GC) is called only from GuestGpu::Process. Guest-thread faults reach
> the caches through GuestGpu::SendCommandSync or side readbacks. There is
> no product path from a non-GPU thread, so this is not a product bug.
>
> VulkanHarness::OnGpuThread(context, work) runs a callable through the
> context's GuestGpu::SendCommandSync and returns its result.
> DownloadOnGpuThread wraps TryDownload with it. The GC, pressure
> collection, ProcessDownloadImages and TryDownload call sites now use
> them. Four checks never started a guest GPU and now call
> RenderContext::InitializeGpu: image pressure readback, BGRA16 readback,
> comparison depth texture and color depth-tile discovery. The caller
> blocks for the duration, so each check's direct scheduler use still
> never overlaps the GPU thread.
>
> Result: image-pressure-retirement, compute-meta-clear, buffer-cache-gc,
> sampled-depth-resource and storage-mip-host pass. image-overlap now
> stops at the separate UnifiedTextureCacheFlow FindImage expectation.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 146. tests: count SyncAliasFromOwner's region scan in the FindImage epoch check

Commit: [`746e7f75`](https://github.com/Jetsku/KytyPS5-experimental/commit/746e7f75ebf387b541895e32699a6253090b8cf7) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> UnifiedTextureCacheFlow (--htile-clear-only, --buffer-cache-range-only,
> --image-overlap-only) failed at "normalized FindImage: registered
> compatible backing did not reuse one ImageId". With instrumentation the
> reuse itself was correct: first, repeated and compatible were all the
> same ImageId, first-page lookup was active, verification was healthy and
> the backing format was right. Only the query-epoch clause failed: the
> epoch went 2 -> 4 across the two hits, where the check expected no
> change.
>
> Classification: stale expectation. The clause dates from 6a7f10ee and
> asserts that a first-page hit (FindImageWithSameBacking) resolves
> without FindImagesInRegion. 9983732d, "Keep depth/color aliases alive",
> added SyncAliasFromOwner to every FindImage. While aliases age by frames
> (the default, KYTY_IMAGE_ALIAS_AGE != ticks) it scans the image's region
> once for a GPU-written alias owner, so each hit now costs exactly one
> region query. That scan is intentional: aliases stay alive, so a lookup
> has to find an owner that another interpretation wrote.
>
> The check now expects exactly one scan per hit when alias aging by
> frames is on and none otherwise. It still fails if the lookup itself
> starts scanning regions again.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 147. tests: model batched end-of-pipe flushes in GpuCommandLane

Commit: [`b30e1dab`](https://github.com/Jetsku/KytyPS5-experimental/commit/b30e1daba663d7866255646ed58da60371a45e27) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> --gpu-command-lane-only failed at "nonblocking GPU packets: a cache
> event, GL2 writeback, DE counter, or interrupt boundary used the wrong
> submission behavior". With KYTY_EOP_FLUSH_BATCH=1 the check and the
> following "RELEASE_MEM submission counts" pass.
>
> Classification: stale expectation. Both checks required every
> end-of-pipe flush request to submit the recording immediately (tick
> +1): a RELEASE_MEM interrupt boundary (INT_SEL 1 and 4) or a DATA_SEL 1
> label. 99e9721c ("CP: batch command-buffer flushes requested by
> end-of-pipe interrupts") made CommandProcessor::BufferFlushForEop submit
> only every KYTY_EOP_FLUSH_BATCH-th request (default 8). Interrupts fire
> when their tick completes, and every slice ends with a flush, so batched
> interrupts are still delivered. e38a39dc additionally bounds the delay
> at 256 packets. This was a deliberate performance change: 387 of 531
> host command buffers per frame came from these flushes.
>
> The check now mirrors the batching rule for the processor it drives:
> the same KYTY_EOP_FLUSH_BATCH parsing, and a count reset by
> BufferFlush. It expects each request's exact tick advance, so a lost
> request or an extra submit still fails. GDS reads still have to wait
> exactly once. A new loop issues interrupt-only releases up to the batch
> boundary and checks that only the boundary request submits.
>
> Verified with the default batch, with KYTY_EOP_FLUSH_BATCH=1 and with
> KYTY_EOP_FLUSH_BATCH=3.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 148. tests: drain GpuCommandLane submissions under bounded AgcSuspendPoint

Commit: [`261ded69`](https://github.com/Jetsku/KytyPS5-experimental/commit/261ded694f0a61143349c202d7ac372422a9e840) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Once the end-of-pipe expectations were fixed, GpuCommandLane failed at
> "borrowed compute commands: compute submission executed a copied PM4
> stream". The live value was still 0 when it was checked.
>
> Classification: stale harness assumption. The check used a single
> GuestGpu::Done() as a barrier for "the submitted PM4 work has executed".
> The submission/sync merge (4ff6ed69) made Done bounded by default
> (KYTY_AGC_DONE_MODE): it records the frame boundary and waits only for
> submissions admitted before the *previous* Done, keeping two guest
> frames in flight on the CP. The very first Done does not wait at all.
> The earlier "borrowed graphics commands" check therefore passed only by
> racing the GPU thread. "ordered completion" and "unmap queue progress"
> would also fail, because a single Done returns before the queued
> submission.
>
> The completion barriers are now drain_submissions(): two consecutive
> Dones. Under bounded mode the second Done drains everything admitted
> before the first; under idle mode each Done drains. The ordered-
> completion thread uses the same barrier, so "submit done barrier" still
> proves the drain blocks until the queued, WAIT_REG_MEM-suspended
> submission runs. The Done inside the shutdown test runs on the GPU
> thread, where Done never waits, and is unchanged.
>
> Verified with the default mode and with KYTY_AGC_DONE_MODE=idle.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 149. tests: expect the graphics kOnly RELEASE_MEM to write its label data

Commit: [`d0263438`](https://github.com/Jetsku/KytyPS5-experimental/commit/d026343848caf2de5089abbc525f037f9dd1d009) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Once the drain fix was in, GpuCommandLane failed at "graphics interrupt
> routing: graphics kOnly interrupt was misrouted or wrote its label". The
> interrupt was delivered correctly. The label held the packet's data
> (0x11223344) instead of the untouched 0xa5a5a5a5.
>
> Classification: stale expectation. 90a073d0 ("CP: write end-of-pipe data
> that INT_SEL used to suppress (A1, accuracy)") changed this on purpose.
> On RDNA, INT_SEL does not gate DATA_SEL. The graphics queue had dropped
> the DATA_SEL 1/2/clock write of an INT_SEL=1 event, while compute queues
> wrote it. By default (KYTY_EOP_DROPPED_LABELS=write) the label is now
> written before the interrupt operation is queued, or by the deferred
> write that then raises the interrupt, so the interrupt still follows the
> data. The compute kOnly check already expected its label to be written.
>
> The graphics check now expects the packet data, or the old untouched
> value under KYTY_EOP_DROPPED_LABELS=count. It still checks the
> interrupt's queue, context, udata and count.
>
> With the earlier fixes, --gpu-command-lane-only passes:
> GpuCommandLane, Pm4IndirectControlFlow, Pm4WaitResume, Pm4RewindResume
> and Pm4CeCompletion. It also passes under
> KYTY_EOP_FLUSH_BATCH=1 + KYTY_AGC_DONE_MODE=idle +
> KYTY_EOP_DROPPED_LABELS=count, and under KYTY_EOP_FLUSH_BATCH=3.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 150. tests: check the tile block equations are bijective; host-only tile selector

Commit: [`bbb6f057`](https://github.com/Jetsku/KytyPS5-experimental/commit/bbb6f05785d6f432b08f3ad28059c8f16ba7984d) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> GpuTilerCpuParity compares the GPU tiler shaders with TileGetBlockOffset
> at every in-block position. A mistake in the per-family bit equations,
> which the CPU reference and the shaders transcribe from the same source,
> would therefore agree on both sides and go unnoticed. The new
> CheckTileBlockBijection tests the equations themselves. For every block
> family and admitted bytes-per-element (44 layouts: Standard 256B/4KB/
> 64KB, their 3D and PRT variants, RenderTarget64KB and Depth64KB), the
> block's width x height x depth elements must map onto distinct,
> element-aligned offsets inside the block. By pigeonhole that means they
> fill it exactly. All 44 layouts pass.
>
> The equations were also checked by hand against the addrlib/D3D12
> standard swizzle:
> - Block dimensions for thin 256B/4KB/64KB and thick 4KB/64KB blocks, at
>   every element size, match addrlib's Block256_2d/Block1K_3d scaling.
> - The 2D Standard 4KB/64KB bit interleaves match the D3D12 standard
>   swizzle at every element size.
> The RenderTarget/Depth (PS5 _X) patterns cannot be derived from public
> addrlib data without the console's pipe configuration. Parity plus this
> bijection check is the strongest check possible without captures.
>
> --tile-layout-only runs this check and DetileCopyCoverage without a
> Vulkan device and is registered as the tile_layout CTest. --gpu-tiler-only
> and the default run now include it as well.

</details>

Changed files: `CMakeLists.txt`, `tests/ShaderRecompilerComputeTests.cpp`.

### 151. tests: keep CompilePixelShader's user data alive through compilation

Commit: [`f3213b27`](https://github.com/Jetsku/KytyPS5-experimental/commit/f3213b2703b6098a2c60551abaf4d17e85d0bef0) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> clang warned, in tests/ShaderCodegenTests.inc:
>
>   options.user_data = MakeNativeUserData(nullptr);
>   warning: object backing the pointer 'options.user_data' will be
>   destroyed at the end of the full-expression [-Wdangling-assignment-gsl]
>
> CompileOptions::user_data is a std::span. The array it pointed to was a
> temporary, so TranslateProgram, ExtractResourcePlan,
> MaterializeResources (through SrtRuntime::user_data) and CompileProgram
> all read a dead stack array.
>
> Classification: harness bug, undefined behaviour. The codegen checks
> passed only because the stale stack slot still held the expected words.
> The array now lives in a local for the whole function, the same pattern
> CompileCase and CompileFragmentCase already use.

</details>

Changed files: `tests/ShaderCodegenTests.inc`.

### 152. tests: expect retained depth/color aliases in UnifiedTextureCacheFlow

Commit: [`db6c3bdd`](https://github.com/Jetsku/KytyPS5-experimental/commit/db6c3bdd17ce61e2265687a349ac4e14b0454865) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> After the epoch fix, UnifiedTextureCacheFlow still failed four checks:
> "raw D16 uint texture backing", "layered raw D16 owner layout",
> "layered raw D16 round trip" and "Buffer-superseded exact coexistence".
> All four pass with KYTY_IMAGE_ALIAS_AGE=ticks.
>
> Classification: stale expectations. 9983732d ("Keep depth/color aliases
> alive") stopped freeing the previous interpretation when a guest
> allocation switches between depth and color. Astro Bot does this with
> one 960x540 allocation about 90 times a second, and the old behavior
> recreated 4,080 images in 45 s. ResolveDepthOverlap now leaves the
> previous image registered as a non-owner alias. FindImage runs
> SyncAliasFromOwner, which copies the current GPU owner's contents into
> a non-owner alias before use. CommitGpuWrite keeps one GPU owner per
> range and clears GPU ownership on superseded aliases without
> downloading them.
>
> Instrumented values, default mode vs KYTY_IMAGE_ALIAS_AGE=ticks:
> - raw D16 -> R16 UINT: a new R16Uint backing in both modes. The D16
>   image stays registered and is not GPU-owned; in ticks mode it is
>   freed.
> - layered R16 UNORM color -> 5-layer D16 view: the depth owner keeps
>   all 6 layers and the full guest range in both modes. The color image
>   stays registered as a non-owner.
> - R16 UINT lookup over that depth owner: FindImage is not exact-format,
>   so it reuses the retained R16 UNORM color image. That backing is
>   format-compatible; color images are created MUTABLE_FORMAT |
>   EXTENDED_USAGE and are sampled through an R16 UINT view.
>   SyncAliasFromOwner copies the depth contents in and the image becomes
>   the owner. The readback holds all 12 words at 0xffff, identical to
>   ticks mode, which builds a new R16Uint backing.
> - exact R32 FLOAT lookup over a GPU-owned R32 UINT image: both images
>   stay registered and neither is Buffer-modified. The FLOAT alias takes
>   over the owner's contents and becomes the single GPU owner. In ticks
>   mode the UINT image stays the owner. The later FLOAT readback matches
>   the guest value either way.
>
> Each check now asserts the ownership model of the active mode:
> - which image is registered,
> - which image holds GPU ownership, and that superseded aliases are not
>   GPU-owned,
> - no Buffer ownership transfer,
> - the layer and guest-range layout,
> - the round-tripped data.
> It is not weakened to "anything goes": the data checks are unchanged,
> and a lost owner, a double owner or a Buffer transfer still fails.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 153. Merge GPU test triage: harness fixes and expectations for intentional product changes (claude/test-health)

Commit: [`166ff5d8`](https://github.com/Jetsku/KytyPS5-experimental/commit/166ff5d8869c129f24538e8872b1d8bff7ea308f) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Test-only (plus one new CTest). Full GPU suite and all 84 selectors pass on the branch:
> tiler parity compares only reference-defined bytes (scratch clear skipped by design), new
> tile-layout coverage checks, heap-allocated RenderContexts, GPU-thread routing for
> GC/downloads, stale expectations for alias retention, EOP flush batching, bounded Done and
> INT_SEL=1 label writes. Conflict in tests/ShaderCodegenTests.inc: both branches fixed the
> same dangling user-data span; kept one declaration.

</details>

### 154. tests: cover the CP frame fence in GpuCommandLane

Commit: [`cf17b3a3`](https://github.com/Jetsku/KytyPS5-experimental/commit/cf17b3a32d75f7910a7d40c2d049161dd3cd758c) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> 1fbc0396 fenced each guest frame's submissions behind the previous
> frame's: a submission admitted after a bounded Done starts only once
> every submission admitted before that Done has completed, on any queue
> (KYTY_FRAME_FENCE). No existing check exercised this. GpuCommandLane
> drains every submission before admitting the next, so it passes with the
> fence on and off alike.
>
> The new check reproduces the Sky Garden water ordering in miniature:
> - Frame N is a graphics submission suspended on WAIT_REG_MEM.
> - One Done follows.
> - Frame N+1 is an async-compute WRITE_DATA on queue 0x20.
>
> Host commands, which run between packets, sample the state for up to
> 500 ms, well inside the fence's 2 s safety timeout. The check then
> releases frame N, drains, and asserts:
> - default: frame N+1 did not run until frame N completed;
> - KYTY_FRAME_FENCE=0: frame N+1 ran while frame N was suspended, which
>   is the old order that sampled overwritten water inputs.
> It is skipped under KYTY_AGC_DONE_MODE=idle, where Done itself would
> wait for the suspended frame.
>
> A regression of the fence makes the default run fail, as the
> KYTY_FRAME_FENCE=0 behavior shows.
> --gpu-command-lane-only passed 5/5 in both modes and under
> KYTY_AGC_DONE_MODE=idle. The default run passes.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 155. Hang trace: command-processor ordering log (cp.csv, KYTY_HANG_TRACE_CP=1)

Commit: [`890818a6`](https://github.com/Jetsku/KytyPS5-experimental/commit/890818a66d81f814e8ec6cd801307dce76c75963) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Rows in record order: submissions admitted (queue, admission sequence, frame fence, Done
> count), slices (complete / suspended / yielded), WAIT_REG_MEM (first failed evaluation and the
> pass with its retry count), label writes (record-time EOP, dropped-INT_SEL, deferred and their
> completion writes, GDS, WRITE_DATA CPU/GPU) and, for KYTY_HANG_TRACE_CP_WATCH=0xADDR:0xSIZE,
> GPU buffer writes, image GPU writes and image uses in that window. The GPU thread tags rows
> with the queue and submission it is processing. Off by default.
>
> For the Sky Garden water: which queue wait lets the guest's 0x514080000 -> 0x53ad00000 copy
> run between the refraction scene copy and the water draw.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`.

### 156. Hang trace: raise the cp.csv row cap to 60M (a diagnostic run spans boot, hub and level load)

Commit: [`31b942e7`](https://github.com/Jetsku/KytyPS5-experimental/commit/31b942e74e1de0967a43a49617b28e805a2258c2) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

Changed files: `src/common/hangTrace.cpp`.

### 157. Textures: a GPU-modified image supersedes stale GPU-dirty buffer bytes for texel-read syncs

Commit: [`8179c714`](https://github.com/Jetsku/KytyPS5-experimental/commit/8179c7141d32c78211624cc4b2a48477f6386c2d) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Sky Garden water (white stream, opaque pool with green specks since U39/U40): the game
> copies its lit scene colour target (1920x1080 RGBA16F at 0x514080000) into the water's
> refraction input (0x53ad00000) with a compute memcpy that reads the target's memory as a
> texel buffer. U37 synced the render target into the buffer first (RenderDoc: eid 15668
> CopyImageToBuffer 651518 + tiler into the source range, then the memcpy at 15679). U40+
> never did (TexelImageSyncDownloads/Skips 0 per flip): TextureCache::SafeToDownload refuses
> any image whose range holds GPU-dirty buffer bytes, and FindImageFromRange applies it, so
> the image was not even found. Those bytes are left by image writebacks (842e7080 marks the
> whole moved range GPU-dirty) and persist until a readback, which the newer readback paths
> rarely do. The memcpy then copied G-buffer bytes from an older use of that memory.
>
> Every bounded GPU buffer write clears the GPU ownership of the images it overlaps
> (InvalidateMemoryFromGPU: storage bindings, fills, copies, GPU WRITE_DATA, writebacks), so an
> image that is GPU-modified now was written after all of them. Unbounded writers are ordered
> by an image content serial taken when they are bound (BufferCache::UnboundedWriteSerial,
> Image::NextContentSerial). SupersedesGpuDirtyBytes combines both; SafeToSyncIntoBuffer lets
> texel-read syncs (FindImageFromRange buffer_sync, SynchronizeBufferFromImage) copy such an
> image into the buffer over the stale bytes. Downloads to guest memory and GC keep the old
> check. KYTY_IMAGE_SUPERSEDES_GPU_DIRTY=0 restores it everywhere. Counter:
> FrameEvent.TexelImageSyncOverGpuDirty.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/image.h`.

### 158. tests: texel-read sync of a GPU-modified image over stale GPU-dirty bytes

Commit: [`eb14c5b7`](https://github.com/Jetsku/KytyPS5-experimental/commit/eb14c5b74bc7d8e8bc823110f1e3d04404d86ac2) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Regression test for the Sky Garden water fix (KYTY_IMAGE_SUPERSEDES_GPU_DIRTY): an image
> written after every GPU-dirty buffer byte of its range is selected for the texel-read sync
> (FindImageFromRange buffer_sync, SafeToSyncIntoBuffer) while guest-memory downloads keep the
> veto; an unbounded writer bound after the image write, a content mark restored across one, and a
> bounded GPU buffer write over the image all withdraw it again. Fails with
> KYTY_IMAGE_SUPERSEDES_GPU_DIRTY=0 at the U40 failure (the texel-read sync finds no image).
> Selector: shader_recompiler_compute_tests --texel-sync-gpu-dirty-only.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 159. Textures/buffers: resident mips, partial chunk refresh and image writeback default off

Commit: [`38b104e6`](https://github.com/Jetsku/KytyPS5-experimental/commit/38b104e6d4723dfa4a683bab072dfa38c5ff3ce0) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Bisected in Sky Garden (U44, Start-Comparison -Revert, user-verified against U37 and PS5
> reference footage):
> - KYTY_TEXTURE_RESIDENT_MIPS: the water's second normal map had mip 0 undefined (zero,
>   RenderDoc GetMinMax) although the water samples it; -Revert Residency alone made the water
>   less bright but still opaque.
> - KYTY_IMAGE_WRITEBACK_ON_GPU_WRITE: whole-range writebacks leave GPU-dirty bytes that later
>   images are created from; the water's quarter-res input (never written in any captured frame)
>   held stale HDR 1-85 instead of zeros, and the water rendered white. Fixed with this off.
> - KYTY_TEXTURE_PARTIAL_UPLOAD: stale coarse mips in reused streaming slots: distant trees
>   washed out or missing leaves (fine up close), another texture's data on the water. Only this
>   switch of the texture-streaming group was needed; async staging, the fault fast path and
>   the texel sync skip stay on.
> With all three off (and the U44 texel-read sync), the stream and pool are see-through and the
> trees match the reference. Each stays available (=1; residency also =poison) until fixed.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`.

### 160. BDA sync: keep incremental passes with hot pages (KYTY_BDA_HOT_SYNC)

Commit: [`0e9e85b1`](https://github.com/Jetsku/KytyPS5-experimental/commit/0e9e85b1f936ca6cb0c3e44b6cb046c3ec801ae6) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Hot pages made MemoryTracker::CpuMutationEpoch() return UINT64_MAX, so no BDA
> pass could skip: every draw using DMA rescanned every buffer in the mapped
> ranges, and compared every hot page with its shadow through a 4 KiB snapshot.
>
> Per flip in the Sky Garden fly-in (U44 151558 against 150758, hot pages and the
> other memory groups reverted):
> - BdaSyncSkips fell from 133 to 0.
> - BdaSyncScannedBuffers rose from 10.1k to 108.8k.
> - HotPageUploads was 8.47k, and 99.7% of them found the page unchanged.
> The T20 ETW puts a scanned buffer at about 106 ns, so this is about 10 ms of
> extra CP time per flip, plus 3-7 ms of hot-page compares.
>
> With KYTY_BDA_HOT_SYNC (default on, needs KYTY_BDA_INCREMENTAL_SYNC=1):
> - MemoryTracker::FaultMutationEpoch() is the same token without the hot-page
>   override. Hot-page demotion and the idle sweep now bump it, because they
>   leave CPU-dirty, writable pages outside the hot set.
> - A full pass records the hot page runs it finds in the scanned buffers.
> - While the fault and structure epochs hold, a pass re-synchronizes only those
>   runs (BdaSyncHotPasses, BdaSyncHotRanges). Every other page of the scanned
>   buffers is clean and protected and can only change through a transition that
>   moves an epoch: the fault that promotes a page, or a demotion.
> - A pass with no recorded runs is skipped as before.
> - Hot pages are compared with their shadow in place. Only a page that differs
>   is snapshotted.
>
> KYTY_BDA_HOT_SYNC=0 restores the previous behaviour.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `tests/MemoryTrackerTests.cpp`.

### 161. Upload batch: flush at scope end only when uploads were queued (KYTY_UPLOAD_BATCH_SCOPED_FLUSH)

Commit: [`35463ca4`](https://github.com/Jetsku/KytyPS5-experimental/commit/35463ca4104e337c4752ec8d718ec56e0abde535) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> BufferCache::UploadBatch recorded every pending barrier when its outermost scope
> ended, even if the scope had queued no upload. The draw path opens such scopes
> in two places:
> - PrepareGraphicsBindings, which wraps RebindBuffers;
> - PrepareBda, which wraps SynchronizeBdaBuffers.
>
> So a draw that follows a draw with storage-buffer writes recorded the pending
> shader-write barrier before it reached BeginRendering. That ended the rendering
> instance and defeated KYTY_DRAW_WRITE_SINK. The scope is used whether
> KYTY_UPLOAD_BATCH is on or off. Per-flip evidence:
> - GpuDrawWriteSinks was 0 and GpuBarriersSunk was 0.
> - u42 Sky Garden: GpuOps.EndRendering.draw.execute 745, batch.shader_write 728,
>   out of 1,186 render-pass begins.
> - u44 fly-in: GpuOps.EndRendering.draw.execute 772-800.
>
> The scope end now records the batch only when uploads are queued. Otherwise
> the pending barriers wait for the next flush point, as the batcher specifies
> (render.h): BeginRendering, where a shader-write-only batch may be sunk past a
> draw that continues the same instance, or Handle() or End(). Every action
> command already reaches one of those first, so no command is recorded ahead of
> a barrier it needs. UploadBatchFlushesDeferred counts the scopes that left
> barriers pending.
>
> KYTY_UPLOAD_BATCH_SCOPED_FLUSH=0 restores the flush at every scope end.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/render.h`.

### 162. BDA hot sync: verify mode (KYTY_BDA_HOT_SYNC_VERIFY)

Commit: [`ef9d5cfc`](https://github.com/Jetsku/KytyPS5-experimental/commit/ef9d5cfcc7402dca316980bb84d3cddfc29174ad) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_BDA_HOT_SYNC_VERIFY=1 follows every hot pass with the full scan it
> replaced. A normal (non-hot) CPU-dirty page is counted when that scan finds it
> while the pass's fault and structure epochs still hold, and it is still
> uploaded, so the mode stays correct. Such a page is one the hot pass would
> have missed. The count is taken under the region lock, where any transition
> that dirtied the page has already published its epoch change.
>
> Counters:
> - BdaSyncHotVerifyChecks: verified hot passes.
> - BdaSyncHotVerifyMismatches: missed pages, expected 0. The first mismatches
>   are also logged to stderr.
>
> KYTY_BDA_HOT_SYNC_VERIFY=exit stops on the first mismatch. The mode is
> diagnostic: it pays the full scans again.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`.

### 163. tests: retire the image's GPU ownership after the raw fill in UnifiedTextureCacheFlow

Commit: [`5c14d519`](https://github.com/Jetsku/KytyPS5-experimental/commit/5c14d519f3d0843683af4196596a5bacd39ddf8c) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The overlapping-buffer check wrote a GPU buffer over a GPU-modified image
> through ObtainBuffer plus Buffer::Fill, but skipped the
> TextureCache::InvalidateMemoryFromGPU call that every product writer makes
> (storage bindings, fills, copies, GPU-timeline WRITE_DATA). With image
> writeback off by default (38b104e6) nothing else cleared the image's
> ownership, so SupersedesGpuDirtyBytes (8179c714) let the stale image
> supersede the newer bytes and the check failed. Follow the product contract
> and also require that the image lost GPU ownership.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 164. Draw prep: passive fence-kind histogram (KYTY_DRAW_PREP_FENCE_HISTOGRAM)

Commit: [`797ebae9`](https://github.com/Jetsku/KytyPS5-experimental/commit/797ebae9f041413124a610dce34b4ceb7c79c0cd) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Parallel draw preparation (S6) cannot open windows in Astro Bot. The U40
> parallel run counted 5,276 fences per flip against about 5,260 draws, and
> 4,125 of those fences followed the previous fence with no draw in between.
> Before S6 can be judged, we need to know which packets these fences are. The
> AGC register-indirect packets (SET_*_REG_INDIRECT) are the main suspects.
>
> - DrawPrep::ClassifyFence (packetClass.h) buckets every fence packet into one
>   of 12 kinds: register-indirect, event write, end of pipe, acquire,
>   wait/predication, data write, constant engine, marker/flip, dispatch,
>   indirect draw, context control, and other.
> - Each fence is counted as FrameEvent.DrawPrepFence<kind>.
> - The per-packet hook now also runs passively in off mode while aggregate
>   diagnostics are collected and a profiler is connected. It then also fills
>   the S0 draws-per-fence histogram (DrawPrepFences, DrawPrepFenceDraws*).
> - In off mode the hook only classifies and counts. Submit still declines and
>   Drain has nothing to commit, so rendering is unchanged.
>
> KYTY_DRAW_PREP_FENCE_HISTOGRAM=0 disables the passive collection.
> KYTY_DRAW_PREP_HISTOGRAM=1 keeps working as before.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `src/graphics/host_gpu/renderer/drawPrep/packetClass.h`, `tests/DrawPrepTests.cpp`.

### 165. Merge claude/cpu-opt: draw-prep fence-kind histogram

Commit: [`dd59ec70`](https://github.com/Jetsku/KytyPS5-experimental/commit/dd59ec70aa9dc7c4592733ff29742db6fe3afcf2) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 166. Colour metadata draws: trace their targets; optionally materialize DCC fast clears on eliminate

Commit: [`7c1f3dc8`](https://github.com/Jetsku/KytyPS5-experimental/commit/7c1f3dc803c5841ee09e9acc7712d1e502db0aab) · **Compressed metadata and clear semantics**

Handles the named metadata/clear operation without stale contents; eligible GPU work avoids CPU readbacks. Accuracy and lower transfer cost are separate claims; see the water discussion.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Fast-clear-eliminate and FMASK/DCC decompress draws (CB_COLOR_CONTROL.MODE 2/5/6) are consumed
> without drawing, and until now left no trace at all. Sky Garden's water composites a quarter-res
> RGBA16F overlay (T# 0x538bc0000, no DCC metadata) that no Kyty operation ever writes; one
> candidate producer is a DCC fast clear of that target followed by an eliminate, which Kyty drops
> unless a later draw binds the target.
>
> - cp.csv (KYTY_HANG_TRACE_CP=1) rows "cb-meta-op" for every such draw and target slot:
>   address = colour base, value = mode, ref = DCC base, mask = CMASK base,
>   aux = slot | DCC << 8 | tile mode << 16 | resolved << 24, size = resolved image size.
> - FrameEvent.MetadataColorOps / MetadataColorOpMaterializations.
> - KYTY_CB_METADATA_MATERIALIZE=1 (default off): on eliminate / DCC decompress, resolve each
>   render-target-tiled, single-sample DCC target (FindImage), which materializes a pending
>   uniform DCC clear into the image and consumes the key, exactly as a later attachment bind
>   would, so readers without DCC metadata see the cleared values.
> - RenderExecutorDccFixedClearFloat: paint, fill the metadata with a clear code, run an eliminate
>   draw, read through a description without metadata: the clear with the switch on, the painted
>   texel with it off (shader_recompiler_compute_tests --dcc-clear-only).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 167. tests: consume the eliminate case's clear before the HTile-reuse checks

Commit: [`69dc8bff`](https://github.com/Jetsku/KytyPS5-experimental/commit/69dc8bff0cdaccd1ad17135ec61f5a1d18f21371) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> With KYTY_CB_METADATA_MATERIALIZE unset the eliminate draw is dropped and left an unconsumed
> uniform clear code in the metadata, which the following HTile-reuse case then materialized over
> its painted texel. Bind the target after the eliminate case: the clear shows either way and the
> key is consumed. --dcc-clear-only passes with the switch on and off.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 168. Texture residency: count extensions over bytes another image already covers

Commit: [`7af5acb5`](https://github.com/Jetsku/KytyPS5-experimental/commit/7af5acb5a0260478e1db6866d241fbb3d00fd324) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A partially resident image registers only its resident prefix, so an image created later in its
> non-resident bytes never saw it during overlap resolution. A residency extension then registers
> those bytes without resolving the overlap, and its refresh reads guest memory even where a
> GPU-modified image holds newer contents. This is the one way the resident-mip scheme can leave a
> sampled level stale; nothing shows whether Astro Bot ever reaches it.
>
> Diagnostics only, no behaviour change: FrameEvent.TextureResidencyExtensionOverlaps (any other
> registered image there) and TextureResidencyExtensionGpuOverlaps (a GPU-modified one; expected 0),
> and a log line for the first 16 of the latter with both images.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`.

### 169. Merge claude/water-fix: colour-metadata draw trace, opt-in DCC eliminate materialization, residency overlap counters

Commit: [`2712da3a`](https://github.com/Jetsku/KytyPS5-experimental/commit/2712da3ac1f8da819bcf9cc2ebe16a40c4188336) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 170. Tiler: detile straight into images and tile straight from them (KYTY_TILER_IMAGE_DIRECT)

Commit: [`a59fa6aa`](https://github.com/Jetsku/KytyPS5-experimental/commit/a59fa6aa6720609c2cea73f2e4477815d7526117) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Uploads of tiled guest images ran a detile pass into a linear scratch buffer and then
> vkCmdCopyBufferToImage; downloads ran vkCmdCopyImageToBuffer into scratch and then a tile
> pass. On the RTX 3090 the copies cost several times the compute passes for the same
> bytes (U31 GPU-op profile, 1920x1080 render targets: buffer->image copy 90-181 us and
> image->buffer copy ~355 us against 21-41 us for the detile/tile dispatches; 2432x1368:
> ~570 us).
>
> The 2D tile families now also build image variants of their shaders (same sources with
> TILER_IMAGE_STORE / TILER_IMAGE_LOAD): each element moves between the tiled buffer and
> element (x, y) of one mip level through an unsigned-integer storage view whose texel size
> equals the element size (R8/R16/R32/R32G32/R32G32B32A32_UINT), so the bits that reach the
> image or the tiled buffer are exactly those of the buffer path. Eligible: single-sample 2D
> colour images with storage usage (not block-compressed, not depth), no BGRA16 swap, regions
> that match their tile infos; everything else keeps the buffer path. Downloads need
> shaderStorageImageReadWithoutFormat, now enabled when the device supports it.
>
> Used by TextureCache::UploadImage, TryPartialUpload, TryAsyncFullUpload and DownloadImage.
>
> KYTY_TILER_IMAGE_DIRECT=0 restores the buffer paths. KYTY_TILER_IMAGE_DIRECT_VERIFY=1 also
> runs the buffer path into scratch after every direct transfer and compares both results on
> completion (FrameEvent TilerImageVerifyChecks/Mismatches; first mismatches logged).
> Counters: FrameEvent.TilerImageUploads/UploadBytes/Downloads/DownloadBytes.
>
> Tests: TilerImageDirect (--gpu-tiler-only) compares direct and buffer paths byte for byte
> for render-target, standard 256B/4KB/64KB, PRT and depth-tiled layouts with 1-16 byte
> elements, mip chains, array layers and partial bands, and checks the fallbacks;
> --tiler-image-bench times both paths on 1920x1080 render targets.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/graphicContext.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/image/tiler.cpp`, `src/graphics/host_gpu/renderer/image/tiler.h`, `src/graphics/host_gpu/shaders/gpu_tiler_common.inc`, `src/graphics/presentation/window/vulkanWindow.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 171. Draws: keep a sampled read-only depth attachment readable within its rendering instance (KYTY_DEPTH_FEEDBACK_KEEP)

Commit: [`bc7c3883`](https://github.com/Jetsku/KytyPS5-experimental/commit/bc7c38831d41c665f705a93cb424d7152981d6d7) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A draw that samples its own depth attachment (decals, soft particles: depth test on, depth
> writes off) transitioned the image to the attachment scope in AcquireRenderTargets and back
> to attachment+shader-read in CommitBindings. The toggle is an access-only image barrier
> (same layout) before every such draw, and a pending image barrier cannot be sunk, so each of
> these draws also ended the rendering instance and began a new one with the same targets.
> In the U31 GPU-op profile about 128 of the ~494 rendering instances per frame were restarts
> with identical targets and nothing but these toggles in between; U44 still ends ~229
> instances per flip at image.transition.
>
> Inside one rendering instance the toggles order nothing when the image is only read: the
> depth/stencil tests and the sampling are all reads. The keep is taken only with an exact
> proof: the image is attached to the active instance, no draw of that instance wrote it (the
> first draw attaching it in the instance wrote nothing and recorded the image's content
> serial; every draw writing depth or stencil, or clearing on load, resets the proof), the
> serial is unchanged (FindDepthTarget's bind-time bump is restored for unwritten draws, and
> any upload, copy or clear changes it), the draw writes nothing, the view covers the whole
> image and the tracked layout is the read-only attachment layout. The image then keeps the
> union of both scopes and no barrier is requested.
>
> Across instances the store of the ended instance still has to precede the next one's
> accesses: when the instance a kept access relied on ends before its draw (other pending
> barriers, a target or occlusion-control change, any EndRendering), CommandBuffer queues the
> left-out access-only barrier with the batch recorded before the next instance.
>
> KYTY_DEPTH_FEEDBACK_KEEP=0 restores the toggles. Counter:
> FrameEvent.DepthFeedbackBarriersAvoided (barriers the toggles would have requested).
> Sync validation (Start-Comparison -SyncValidation) checks the ordering in game.
>
> Test: DepthFeedbackKeep (--depth-feedback-keep-only) - consecutive sampling draws share one
> instance, a depth write splits and orders later sampling, and ending the instance queues
> the ordering barrier.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/image/image.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 172. Profiler: aggregate counters for why rendering instances end between draws

Commit: [`dc733bf1`](https://github.com/Jetsku/KytyPS5-experimental/commit/dc733bf19e0cf7cb909e7d5c04ecb0f822d35bdf) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The desert slide (U46) ends one rendering instance per mesh auto draw: about 1,200-1,340
> GpuOps.EndRendering.image.transition and 2,500-2,800 image barrier batches per flip against
> ~1,260 mesh draws. That site is a second Image::Transit of an image already pending in the
> same draw (the attachment scope from AcquireRenderTargets, then attachment+shader-read from
> CommitBindings when the draw samples its own attachment); BatchImageBarriers records the
> pending batch first and so ends the instance. The aggregate profile could not say which
> attachment, which barrier origins, or why KYTY_DEPTH_FEEDBACK_KEEP was not taken.
>
> New FrameEvent counters (aggregate mode):
> - BarrierRequests{Guest,ShaderAccess,ShaderWrite,ShaderWriteHazard,IndirectArgs,Gds,Image,
>   Upload}: requests queued in the batcher, by origin;
> - ImageBarrierSameImageFlushes / ImageBarrierSameImageRenderEnds: second barriers of an image
>   already pending, and those that ended an active instance;
> - SampledColorAttachmentBindings / SampledDepthAttachmentBindings: draw bindings that sample
>   an image that is also an attachment of the draw;
> - DepthFeedbackKeepMiss{Write,Instance,Serial,State}: sampled depth attachments that kept the
>   toggles, by reason (the draw writes it; no read-only proof for the active instance, which
>   includes the first draw of every instance; contents changed; tracked state differs).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/renderDraw.cpp`.

### 173. tests: time the buffer->image copy of block-compressed uploads in --tiler-image-bench

Commit: [`84d78678`](https://github.com/Jetsku/KytyPS5-experimental/commit/84d78678cdc6dfddf4103f180907c69cafbe0aa3) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Block-compressed textures have no direct path (no storage usage). Detile alone against
> detile plus the copy for 2048x2048 BC7 and BC1 textures shows what the copy costs for them.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 174. Tiler: create the image pipelines before the first descriptor push

Commit: [`3e0aa1a3`](https://github.com/Jetsku/KytyPS5-experimental/commit/3e0aa1a3785bd72b3d0079e83fed97f6252d2cdc) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> TileManager::RecordImage pushed the descriptors of the first direct transfer before
> GetImagePipeline had created the image pipeline layout they name, so the first
> KYTY_TILER_IMAGE_DIRECT upload or download of a process pushed with a null layout and the
> driver faulted (TilerImageDirect and --tiler-image-bench crashed). All pipelines of a
> transfer are now resolved before any command is recorded.

</details>

Changed files: `src/graphics/host_gpu/renderer/image/tiler.cpp`.

### 175. tests: DepthFeedbackKeep covers load clears and content changes

Commit: [`ae217628`](https://github.com/Jetsku/KytyPS5-experimental/commit/ae21762821291e9a0692308396d978fe07118b88) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A draw clearing the depth attachment on load, and a content change of the image inside the
> rendering instance (what an upload, copy or clear records), both make the next sampling
> draw order after them in a new instance; read-only sampling after that keeps again.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 176. Merge claude/gpu-opt: direct tiler transfers, depth feedback keep, pass-end counters

Commit: [`7568349b`](https://github.com/Jetsku/KytyPS5-experimental/commit/7568349bced4666cde581ab382d9b2a136813999) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 177. Textures: resident mip levels default on again

Commit: [`119fcce9`](https://github.com/Jetsku/KytyPS5-experimental/commit/119fcce93ffbfb0b955837bd444da68a5e63b445) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The U44 bisect turned them off with the water fix, but they were not a cause:
> the water normal map's mip 0 stays non-resident because its T# MIN_LOD is 1.0,
> so no view samples it, and the white water came from the stale quarter-res
> input B (image writeback). Every sampled view still ensures residency of
> RequestedFirstLevel first. KYTY_TEXTURE_RESIDENT_MIPS=0 turns them off,
> =poison marks non-resident levels on screen.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`.

### 178. Draw prep: certify the code of headerless shaders (KYTY_DRAW_PREP_CODE_CERT)

Commit: [`7b1d998a`](https://github.com/Jetsku/KytyPS5-experimental/commit/7b1d998abf49ea02771888cac73b07f8d4268e0b) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every Astro Bot shader lacks an AGC header hash, so GetShaderParams hashed the
> whole code and failed every draw-prep preparation as Uncertified. In the U40
> parallel run, 5,438 of 5,450 preparations per flip fell back this way.
>
> A preparation now reads the code through its recorder (TryReadGpuCleanBacking)
> and hashes the recorded copy, so every code byte becomes part of the
> certificate (readSet.h). At commit, Validate re-reads each certified range
> with the exact clean predicate and compares it byte for byte. If the check
> holds, the serial path would read the same clean bytes (HashShaderCode's
> clean-backing branch) and compute the same hash. A code range that is not
> clean, or a changed code byte, rejects the certificate, and the serial
> preparation runs as before. ReadSet::MaxBytes grows from 64 to 256 KiB to hold
> the code of both stages.
>
> - Counter: ShaderCodeHashCertified.
> - KYTY_DRAW_PREP_CODE_CERT=0 restores the Uncertified failure.
> - KYTY_DRAW_PREP_VERIFY=1 still compares committed programs with a serial
>   preparation.
> - New GPU-harness test (--draw-prep-code-cert-only, ctest draw_prep_code_cert):
>   - the speculative hash equals the serial one;
>   - the certificate covers the code;
>   - one changed code byte makes it reject, and restoring the byte makes it
>     accept again;
>   - GPU-owned code fails the preparation as Unclean.
>   The test fails with the switch off.
>
> Draw prep stays off by default (KYTY_DRAW_PREP=off), so default behaviour is
> unchanged.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/drawPrep/readSet.h`, `src/graphics/shader/shader.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 179. Draw prep: keep windows open over clean register loads (KYTY_DRAW_PREP_REG_INDIRECT_WINDOW)

Commit: [`1cb62a43`](https://github.com/Jetsku/KytyPS5-experimental/commit/1cb62a4311542299812b45e5082f88264a581a56) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Parallel draw preparation drains its window at every fence packet. Most of the
> ~5,300 fences per flip in Sky Garden are AGC register loads (U43 hang-trace
> imports, per flip):
> - SetUcRegistersIndirect 938, SetCxRegistersIndirect 873 and
>   SetShRegistersIndirect 805, about 2,600 in all.
> - ReleaseMem 1,000, EventWrite 491, AcquireMem 338, WaitRegMem 242.
> - DrawIndexOffset 5,095.
>
> A SET_*_REG_INDIRECT packet now keeps the window open when its register pairs
> are clean for a backing read, checked with IsGpuCleanForRead, the predicate of
> the handler's own read. Its handler then reads those clean bytes with no
> synchronization and records nothing. It writes command-processor registers,
> which pending draws do not read because they use their snapshots.
>
> A pending draw's recording cannot change the bytes before the serial path
> would read them. Readback publications only write bytes that are GPU-dirty or
> being published, and the pairs are neither. The pending draws' own shader
> writes have not executed; on the hardware, the command processor fetches the
> pairs when it reaches the packet without waiting for earlier draws, so there
> is no ordering to preserve. A guest wait, acquire or end-of-pipe packet in
> between is still a fence.
>
> - Counter: DrawPrepRegIndirectKept. Packets whose pairs are not clean stay
>   fences (DrawPrepFenceRegIndirect).
> - KYTY_DRAW_PREP_REG_INDIRECT_WINDOW=0 keeps every such packet a fence.
> - The passive fence histogram in off mode is now opt-in
>   (KYTY_DRAW_PREP_FENCE_HISTOGRAM=1). Classifying every packet costs the
>   command processor 20-50 ns each; inline and parallel modes count fence kinds
>   anyway.
> - Tests: draw_prep_tests decode the register-pair range. The PM4 and draw GPU
>   tests pass with KYTY_DRAW_PREP=inline and =parallel (VERIFY=exit).
>
> Draw prep stays off by default.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `src/graphics/host_gpu/renderer/drawPrep/packetClass.h`, `tests/DrawPrepTests.cpp`.

### 180. tests: draw-prep end to end, serial draws vs the engine's (DrawPrepEngineDraw)

Commit: [`62c59d59`](https://github.com/Jetsku/KytyPS5-experimental/commit/62c59d598979e72b560fa2bdfb8a77f9384b0e43) · **Regression coverage and build integration**

Checks the named behavior or repairs its fixture/build linkage. This is supporting evidence for correctness, not a runtime speedup.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The harness had no test that pushes a whole draw through DrawPrep::Engine. The
> new GPU test records two native draws (a fullscreen triangle, then a
> SET_SH_REG_INDIRECT load of the second pixel shader's address from clean guest
> memory, then the same triangle) twice: serially through RenderExecutor, and as
> a PM4 stream through the command processor. Additive blending of the two pixel
> shaders' constants shows which shader each draw ran, so a draw committed with
> live registers instead of its snapshot fails. Both runs must match the expected
> pixels and each other.
>
> With KYTY_DRAW_PREP=inline or parallel it also checks the engine's decisions:
> both headerless draws commit (certified code hashes), the register load stays
> in the window, and parallel mode drains once for both draws (twice with
> KYTY_DRAW_PREP_REG_INDIRECT_WINDOW=0). KYTY_DRAW_PREP_CODE_CERT=0 expects two
> fallbacks instead. Off mode checks the pixels only.
>
> The Tracy frame events only count with a connected profiler, so the engine now
> keeps process-wide totals (DrawPrep::GetTotals: committed, fallbacks, drains,
> register loads kept, last failure). They are relaxed atomics written on the GPU
> thread, once per validated draw, drain or kept register load; nothing changes
> in off mode.
>
> CTest: draw_prep_engine (off), draw_prep_engine_inline and
> draw_prep_engine_parallel (both KYTY_DRAW_PREP_VERIFY=exit), and
> draw_prep_engine_parallel_fence (register-load window off).

</details>

Changed files: `CMakeLists.txt`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 181. Merge claude/cpu-opt: draw-prep code certificates, register-load windows, end-to-end test

Commit: [`9c28af90`](https://github.com/Jetsku/KytyPS5-experimental/commit/9c28af90a50045024eec3707490a8e28c7d1c14d) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 182. Tiler: detile block-compressed uploads straight into the image (KYTY_TILER_IMAGE_DIRECT_BC)

Commit: [`2bd2f5c6`](https://github.com/Jetsku/KytyPS5-experimental/commit/2bd2f5c6cb1124394c6838dbfee91d8eafa91514) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Block-compressed textures (the streamed BC4/BC5/BC7 textures) still took the buffer path: a
> detile pass into a linear scratch buffer, then vkCmdCopyBufferToImage. On the RTX 3090 the
> copy costs about 26 us/MB (2048x2048 BC7, 4.2 MB: detile 14.7 us, detile + copy 121 us,
> direct 10.3 us with random data; BC1 2.1 MB: 79.6 us -> 6.4 us). At the U47 Sky Garden start
> view about 46 MB/flip of texture streaming goes through it: about -1.2 ms GPU per flip.
>
> Block-compressed images now get storage usage where the device accepts it for the format
> (VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT with extended usage; checked with
> vkGetPhysicalDeviceImageFormatProperties, otherwise the image keeps its old usage and the
> buffer path). DetileToImage writes their blocks through uncompressed views (R32G32_UINT for
> 8-byte blocks, R32G32B32A32_UINT for 16-byte blocks) whose texels are the image's 4x4 blocks,
> one view per (level, layer) as block-texel views require. The bits reaching the image are those
> the copy wrote. Only uploads: block-compressed downloads keep the buffer path.
>
> KYTY_TILER_IMAGE_DIRECT_BC=0 (or KYTY_TILER_IMAGE_DIRECT=0) restores the buffer path and the
> old image usage. KYTY_TILER_IMAGE_DIRECT_VERIFY=1 covers these uploads too (row ranges in
> blocks). Counters: FrameEvent.TilerImageBlockUploads/BlockUploadBytes (also counted in
> TilerImageUploads/UploadBytes).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/image.h`, `src/graphics/host_gpu/renderer/image/tiler.cpp`, `src/graphics/host_gpu/renderer/image/tiler.h`.

### 183. tests: block-compressed direct uploads and host-staging timings in the tiler checks

Commit: [`9045edc2`](https://github.com/Jetsku/KytyPS5-experimental/commit/9045edc2adaab033b5b157c8e088f1a5d0a5775a) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> TilerImageDirect compares direct and buffer-path uploads byte for byte for BC1, BC3, BC4, BC5
> and BC7 textures (standard 256B/4KB/64KB and PRT tiling, odd sizes, full mip chains, array
> layers) and checks that block-compressed downloads keep the buffer path.
>
> --tiler-image-bench now fills its tiled buffers with random data (uniform data let compressed
> memory paths flatter the direct timings), times block-compressed direct uploads, and compares
> detiling from a host-memory staging buffer with copying it to device memory first (both are
> bound by the host link: about 12-13 GB/s on this machine).

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 184. Merge claude/gpu-opt: direct uploads for block-compressed textures

Commit: [`b6cf278d`](https://github.com/Jetsku/KytyPS5-experimental/commit/b6cf278da6934cf13ccf2765e4aa39718771b4e0) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 185. readback: publish read-hot pages eagerly at completion; early submit for CP-read writers

Commit: [`a66ef633`](https://github.com/Jetsku/KytyPS5-experimental/commit/a66ef63353cdadc2829e0e36fc55b5b1e66a8fa6) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U47 evidence (hang trace hangtrace-u47-gpu-20260927-162014; windows aligned to the Tracy
> captures, whose clock and the hang trace's both start at the game process start, 16:20:34):
> - The CPU reads that fault are unsynchronized reads of small GPU results. eboot+0x736fa9e
>   (Draw Shadow/Decal/DrawThread jobs, called from 0x2b0c63/0x2b0d73/0x2aa74b) loads one float
>   from a V#-described buffer (0x555f41dd0, writer: an 8-byte shader-storage write);
>   eboot+0x7465159 loads a vec4 the same way (0x50740de, 128-byte writer); libc memcpy reads
>   24..7056-byte results (0x5798121, 0x57984ff, 0x579e0fb, 0x57980a4). No label wait or poll
>   precedes them. The page's writer was recorded 9 ms (desert running) to 27-47 ms (Sky Garden,
>   slide) before the read, so most reads find a finished producer and pay only the fault ->
>   SendCommandSync -> side copy -> wake -> publish round trip (p50 0.3-0.9 ms, two to three
>   threads per value per frame).
> - The largest desert cost is on the command processor: one indirect draw per frame falls back
>   to the CPU path and reads its 20-byte arguments (0x56ddaffa0), written by the current
>   recording about 6 ms earlier, so the read drains the whole GPU: 4.6 ms/flip running and
>   5.6 ms/flip sliding (GpuWaitDrain). Flips without that drain finish in one 120 Hz vblank
>   (9.0 ms), flips with one in two (15.9 ms).
>
> Change (KYTY_READBACK_EAGER, default on; needs KYTY_READBACK_SIDE_COPY):
> - A page becomes read-hot when a CPU read of GPU-owned bytes on it needs a readback (guest
>   read fault, or a GPU-thread read such as ReadGuestForCp). EagerReadbackPages keeps up to 64
>   (KYTY_READBACK_EAGER_PAGES), forgets a page after 600 frames without a readback
>   (KYTY_READBACK_EAGER_IDLE_FRAMES) and issues at most 4 copies per page and frame
>   (KYTY_READBACK_EAGER_FRAME_BUDGET). Writes of hot pages (NoteBufferContentWrite) make them
>   candidates.
> - CommandProcessor::BufferFlush, between packets, calls BufferCache::IssueEagerReadbacks: for a
>   candidate whose dirty bytes were all written by already submitted recordings (the
>   side-readback rule; a writer registered by the current recording retries at the next flush),
>   it appends barrier + copy of those bytes to the end of the recording being submitted, into
>   one of 64 page slots, moves them from the dirty set to a backing publication, marks the page
>   readback-pending and queues a completion-runner operation at that tick. The record is a
>   SideReadback (eager=true, value=master tick) in the side registry, so every existing ordering
>   rule applies: readers, downloads, buffer deletion, GC and SynchronizeGpuBackingForRead complete
>   overlapping entries first; entries never overlap; nothing is issued next to a pending
>   publication. Not issuing inside Submit itself is deliberate: a Submit can happen inside a
>   written upload (staging-ring wrap) with tracker locks held.
> - Completion (the runner, or a reader that gets there first) writes the bytes to the backing,
>   ends the publication and unprotects the page unless a newer writer re-owned it. A read after
>   that finds the page clean; a read before it waits for that copy (fault-read-eager), exactly as
>   for a side copy of the same bytes.
> - A recorded writer of a page the GPU thread reads back requests an early submission; the CP
>   honours it after the draw/dispatch outside a rendering instance (MaybeFlushIdleGpu), at most
>   8 per frame (KYTY_READBACK_EAGER_FLUSHES, 0 disables). The producer then runs before the CP's
>   read, which waits for that recording (side copy or eager copy) instead of draining the
>   current one. Scheduling only.
>
> Accuracy: nothing is published speculatively. The published bytes, their order against newer
> writers and the protection rules are those of the side readback the read would otherwise have
> issued; only the time of the copy moves earlier (end of a submitted recording instead of the
> read). Values the guest can observe are unchanged, so there is no verify mode;
> KYTY_READBACK_EAGER=0 reverts completely.
>
> Diagnostics: FrameEvent.ReadbackEager{HotPages,Copies,CopyBytes,Retries,Waits,PagesUnmarked,
> PagesRetained,Flushes}; hang-trace readback kinds fault-read-eager and eager-publish
> (duration_us = issue to publication). FrameEvent.DrawIndirectFallback{Host,IndexBuffer,
> TargetOp,QuadList,Restart,Mesh} name why the per-frame indirect draw takes the CPU path.
>
> Tests: MemoryTrackerTests covers EagerReadbackPages (candidates, retry/drop, per-frame budget,
> eviction, idle expiry, frame wrap, capacity 0). ShaderRecompilerComputeTests
> --readback-eager-only (CTest buffer_cache_eager_readback) covers issue timing, publication at
> completion, a newer writer during the copy, a guest read and a CP read during the copy, the
> early-submission request, and KYTY_READBACK_EAGER=0. Built; the GPU test was not run in this
> session.

</details>

Changed files: `CMakeLists.txt`, `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/eagerReadbackPages.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `tests/MemoryTrackerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 186. Merge claude/readback-waits: eager publication of read-hot pages, early submit for CP-read writers

Commit: [`a67df633`](https://github.com/Jetsku/KytyPS5-experimental/commit/a67df633697dc3db5346e54c525f1f9e93b15191) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 187. LOD reports: publish each report at GPU completion only (KYTY_LOD_REPORT_PUBLISH)

Commit: [`741d2f0d`](https://github.com/Jetsku/KytyPS5-experimental/commit/741d2f0df876802c756da6a05f8f24f3b49cd43c) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot keeps 16 buckets of 256 mip-statistics counters, each with a ring of 16 report
> slots, and reports one bucket per frame (eboot+0x7022cf0 -> +0x4798a0). The sequence is:
> DMA_DATA clears the slot header, then an empty GET_LOD_STATS probe, then GET_LOD_STATS
> report+reset, then RELEASE_MEM. A final DMA_DATA publishes the slot index. On hardware the
> index therefore always names a complete report.
>
> Kyty performs the index DMA when the packet is recorded. Since U26 it also writes a
> record-time placeholder: the newest completed statistics, which belong to the previous
> bucket's counters. The guest could consume them as this bucket's until the U33 completion
> rewrite landed (2-3 ms in U47, up to 250 ms in U44).
>
> The guest's lookup (eboot+0x7022e40) skips a current slot whose header dword is zero. It
> keeps the statistics it parsed from the previous slot, which is what hardware would still
> show it. Its parser (eboot+0x479d40) falls back through all 16 ring slots for an unsampled
> entry. So the U26 premise was wrong: a report published late does not read as "no data".
>
> The default now writes only the packet's own interval, at GPU completion. The empty probe
> packet (no buffer, no reset) records nothing and no longer ends rendering. Report entries
> carry the counter id in bits 24..31; the guest's debug view uses it as the column.
>
> Switches:
> - KYTY_LOD_REPORT_PUBLISH=rewrite: the U33..U47 behaviour.
> - KYTY_LOD_REPORT_PUBLISH=record, or KYTY_LOD_REPORT_COMPLETION_WRITE=0: the U26..U32
>   behaviour.
>
> lodreports.csv gains drawn_counters and counted_counters: entries with a finest mip, and
> entries with a non-zero bits 0..23 field, which the streamer calls "MipClamp". The
> Vulkan-free report layout moves to lodStatsReport.h. lod_stats_report_tests covers the
> layout and the switch parsing.

</details>

Changed files: `CMakeLists.txt`, `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/lodStats.h`, `src/graphics/host_gpu/renderer/lodStatsReport.h`, `tests/LodStatsReportTests.cpp`.

### 188. LOD stats: count samples the T# MIN_LOD clamp raised (KYTY_LOD_STATS_COUNT)

Commit: [`bd77aef1`](https://github.com/Jetsku/KytyPS5-experimental/commit/bd77aef10279899191d8ce1e9fbebe515b554958) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot's streamer reads bits 0..23 of a GET_LOD_STATS entry as "MipClamp". Its debug
> view prints '[%s] %08x : Drawn = %d, MipClamp = %d, MipLevel = %2.2f, Idle=%d' at
> eboot+0x7020180, where Drawn is "mip field != 0xF" and MipClamp is "bits 0..23 != 0". The
> streamer thread (eboot+0x740b350, pool vtable at eboot+0x8d33ca0) uses the field in three
> ways:
> - It promotes a texture from its 128 KiB head to the full file only while MipClamp is set
>   (eboot+0x7375440).
> - It refuses to demote a texture while MipClamp is set (eboot+0x7375640).
> - It demotes, 32 per pass, textures without MipClamp whose idle count is at least 61, but
>   only while its fixed 4.5 GiB pool (0x120000000, eboot+0x7375c40) is over 85% full. It
>   promotes while the pool is under 90%.
> The update at eboot+0x7021120 keeps an unsampled texture's LOD when the field is non-zero.
> The T# bit that enables the counter is the RDNA "partially resident texture hardware
> counter enable". The field therefore counts samples whose LOD was finer than the texture's
> resident clamp (T# MIN_LOD): a head (MIN_LOD 4.0 in Astro Bot) sampled at LOD < 4 is
> clamped, and a fully resident texture never is.
>
> Kyty wrote every sample into the field, so every drawn texture looked clamped. The streamer
> promoted each drawn streamable texture to full size, whether or not its head already held
> the sampled mips. In Sky Garden that filled the pool to ~82-87% (U47, estimated from the
> streamer's reads). That is inside the 85-90% band, where each pass evicts idle textures and
> loads more: 32 demotions per frame, ~19 promotions and ~19 demotions per flip even with the
> camera stationary, i.e. the 128 KiB <-> full cycling seen since U33.
>
> The per-image shader field grows to 32 bits (LodStatsReport::ImageField). It adds a U4.8
> threshold, the T# MIN_LOD, so a subgroup adds to the count only when a lane's level, limited
> to 0..14, is below it. The finest-level word is unchanged. Magnification of a MIN_LOD 0
> texture is not a clamp. Counting only when needed also skips the count atomic for most
> subgroups.
>
> KYTY_LOD_STATS_COUNT=samples restores the U25..U47 count by passing a threshold beyond level
> 14. The U47 reports are KYTY_LOD_STATS_COUNT=samples with KYTY_LOD_REPORT_PUBLISH=rewrite.
>
> Tests:
> - lod_stats_report_tests: the field, a CPU model of the recording (head below/above
>   MIN_LOD, resident and magnified textures, samples mode) and the switches.
> - lod_stats_codegen: CPU-only, validates and checks the instrumentation SPIR-V
>   (shader_recompiler_compute_tests --lod-stats-codegen-only).

</details>

Changed files: `CMakeLists.txt`, `src/common/hangTrace.h`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/lodStats.h`, `src/graphics/host_gpu/renderer/lodStatsReport.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterImage.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `tests/LodStatsReportTests.cpp`, `tests/ShaderCodegenTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 189. Hang trace: count streamed-texture promote and demote reads

Commit: [`2e8cf7e6`](https://github.com/Jetsku/KytyPS5-experimental/commit/2e8cf7e6473c3a100b4dc8db7b6ee471a4e0c3fd) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> summary.csv gains four columns at the end:
> - apr_grow_reads / apr_shrink_reads: reads from file offset 0 larger / smaller than the same
>   file's previous offset-0 read. Astro Bot's texture streamer promotes a texture by reading the
>   full file and demotes it by re-reading its 128 KiB head.
> - apr_shrink_max_per_flip: the most shrink reads between two flips. The guest's eviction pass
>   stops at 32 textures, so 32 marks a pass that hit the cap.
> - apr_stream_mib: the sum of the latest offset-0 read of every file a TextureStreamer thread read,
>   an estimate of the streamed-texture footprint against the guest's 4.5 GiB pool, which evicts
>   above 85%. Level unloads issue no reads, so compare it within one level visit.
>
> Replayed on the U48 speed-run apr.csv, the Sky Garden shows 140-260 grow and shrink reads per
> second (11-18 per flip), bursts of 32 or more per frame and a 3.6-4.0 GiB footprint (79-89%);
> the desert shows none.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`.

### 190. Merge claude/texture-cycling: GET_LOD_STATS counts only samples below MIN_LOD, reports published at completion

Commit: [`54e83a43`](https://github.com/Jetsku/KytyPS5-experimental/commit/54e83a432c2946539e69d4091aa0d7433282d2ac) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 191. Buffer cache: skip unchanged read synchronizations, settle over-checked hot pages

Commit: [`5136b520`](https://github.com/Jetsku/KytyPS5-experimental/commit/5136b520b7c7aea384886126ac2e854880d64a9c) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Two exact fast paths for the per-draw buffer work that still runs on the command
> processor (U47 Sky Garden WPR: buffer synchronization 9.1 ms/flip, ObtainBuffer
> bookkeeping 6.9 ms/flip; U48 desert: 68.6k hot-page compares per flip).
>
> Region mutation serials. Every RegionManager now has a serial that advances,
> under the region lock and before the bits change, on every change of its
> CPU-dirty, GPU-dirty, hot or readback-pending bits. MemoryTracker::RangeSignature
> sums the serials of the regions a range spans (0 if one does not exist yet).
> Serials only grow, so an unchanged sum means no tracker bit of the range
> changed in between.
>
> KYTY_BUFFER_RANGE_MEMO (default on; =0 off). A direct-mapped memo of facts
> about a range, valid while its signature is unchanged:
> - Clean: no page is CPU-dirty (normal or hot). A read-only, non-texel
>   SynchronizeBuffer of such a range collects nothing and does nothing else,
>   whatever buffer it is for, so it returns at once. The fact is recorded when a
>   synchronization found nothing and the signature was the same before and
>   after it. Covers read bindings, image sources and BDA full passes.
> - Stream: a small read binding (<= 16 KiB) is CPU-dirty and not GPU-dirty, so
>   ObtainBuffer copies it into the stream buffer. The decision is reused
>   without its two locked tracker queries; the bytes are copied every time.
> A transition racing the lookup is a guest write racing the draw, which the
> normal path misses in the same way. KYTY_BUFFER_RANGE_MEMO_VERIFY=1|exit
> re-evaluates every hit the normal way (uploading whatever it finds) and counts
> disagreements (BufferRangeMemoVerifyMismatches). Counters: BufferRangeMemo
> CleanHits, StreamHits, Records, VerifyChecks, VerifyMismatches.
>
> KYTY_HOT_PAGE_CHECK_LIMIT (default 64; 0 off). A hot page that this many
> uploads in a row found unchanged returns to normal tracking through
> SettleHotPages: it becomes clean and write-protected, its shadow is compared
> once more after the protection (a write that raced the last compare marks it
> CPU-dirty), and its next write faults as for any tracked page. Hot pages suit
> pages written between most uploads; a page every BDA draw re-examines but the
> CPU rewrites once a frame costs hundreds of 4 KiB compares per frame instead of
> one fault. A page that keeps changing never reaches the limit and stays hot.
> Counter: HotPageCheckSettles. Hot shadows of a run are now looked up once per
> run instead of once per page (same shadows, ordered walk).
>
> Tests: memory_tracker_tests covers which transitions move a signature and which
> do not (queries, clean re-synchronizations, hot pages kept hot) over one and
> two regions. The GPU test --buffer-range-memo-only (CTest buffer_range_memo,
> _verify with VERIFY=exit, _off with both switches off) checks that a clean
> range is recorded once and then skipped, that a CPU write fault makes it upload
> again, that a small dirty read reuses its stream decision but copies the new
> bytes, and that a hot page settles after exactly the limit, a write while hot
> restarts the count and a write after settling faults and is uploaded.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `tests/MemoryTrackerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 192. Draw prep: certify headerless shader code by its digest (KYTY_DRAW_PREP_CODE_DIGEST)

Commit: [`22f72de0`](https://github.com/Jetsku/KytyPS5-experimental/commit/22f72de0d5eb50e6cf6fe45eb141ef11bc1c8169) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U48 Sky Garden start with KYTY_DRAW_PREP=parallel: committed certificates cover
> 49.8 MB per flip and DrawPrepValidate costs the command processor 5.6 ms per
> flip. Nearly all of it is shader code: since 7b1d998a every preparation records
> the whole code of both stages so that the commit can compare it byte for byte.
>
> A preparation only hashes that code (XXH3-64, the program key the serial path
> looks the program up by); nothing else of it depends on the bytes. Such a read
> is now certified by that digest: the recorder keeps no copy, and Validate()
> re-reads the range with the same clean read and compares digests. Equal digests
> make the serial path compute the same key from the bytes it would read at
> commit, which is all the certificate has to prove; a digest-only read fails the
> same way a byte read does (Unclean when not clean now, Changed when the digest
> differs). The commit hashes each range once instead of comparing it with a
> recorded copy, and workers no longer copy code into certificates.
>
> ReadSet::RecordDigest keeps (address, size, digest) apart from the byte reads;
> AllClean and the log-mode check (sorted and merged with the byte ranges) cover
> digest ranges too. LibKernel::Memory::TryReadGpuCleanBackingDigest is the
> recorder-aware read (same gating as TryReadGpuCleanBacking) that records the
> digest. KYTY_DRAW_PREP_CODE_DIGEST=0 records the code bytes as before.
> KYTY_DRAW_PREP_VERIFY still compares every committed preparation with the
> serial one. Counter: DrawPrepCertDigestBytes (DrawPrepCertBytes now counts
> only byte-compared ranges).
>
> Tests: draw_prep_tests covers digest ranges beside byte reads (changed,
> restored, unclean, limits); the GPU certificate test accepts either form and
> checks the code is certified by digest by default (draw_prep_code_cert_bytes
> runs it with the switch off).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/readSet.h`, `src/graphics/shader/shader.cpp`, `src/kernel/memory.cpp`, `src/kernel/memory.h`, `tests/DrawPrepTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 193. Diagnostics: count draws that bind what the previous draw bound

Commit: [`5b5fa1a4`](https://github.com/Jetsku/KytyPS5-experimental/commit/5b5fa1a47c5b70d12937320c6239767d76b144ce) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_DRAW_BINDING_REPEAT_STATS=1 (opt-in, counted with aggregates only) compares
> every draw's stage programs and resource snapshots with the previous draw's:
> FrameEvent.DrawBindingRepeatPrograms (same programs), ...Textures (also the
> same image and sampler descriptors), ...Resources (also the same buffer
> descriptors) and ...All (also the same user data and flattened SRT words).
>
> This sizes a draw-level binding reuse for the desert "stamps" (identical
> small draws that differ only in constants) before building one: Resources
> minus All is the constant-only share. No behaviour changes; off by default
> because it copies each draw's descriptor words.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`.

### 194. Texture binding memo: revalidate entries whose page changed (KYTY_TEXTURE_MEMO_REVALIDATE)

Commit: [`4915c420`](https://github.com/Jetsku/KytyPS5-experimental/commit/4915c420f58b48e7d3080b0e5f0b492007a13ca7) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U48 Sky Garden start: 3.8k texture bindings per flip found their memo entry
> stale and took the full resolution (FindImage, description, validation, a new
> entry, then FindTexture/RefreshImage because the new entry has no view yet).
> Most of them only because another image registered or unregistered on the
> entry's first 1 MiB page (transient render targets: about 28 texture-cache
> structure changes and 14 native pool retires per flip), which moves the page's
> structure version.
>
> A moved version now only means the owner list changed. The entry is checked
> again the way Record() checks it, under the same lock: FindImage's first-page
> lookup (FindImageWithSameBacking on the recorded description, with the recorded
> exact_format) must still return the recorded image, and the alias-partner scan
> is redone for the new owner list. If both hold, FindImage would return the same
> image with the same side effects (the live checks for a stencil association, a
> pending rebind, resident levels and the alias owner follow as before), so the
> entry takes the new version and stays in place with its tag and view. Otherwise
> it is stale as before.
>
> KYTY_TEXTURE_MEMO_REVALIDATE=0 drops such entries as before.
> KYTY_TEXTURE_MEMO_REVALIDATE_VERIFY=1|exit follows every revalidated hit with
> the full resolution (RenderExecutor::ResolveTextureFull, the memo-free part of
> ResolveTexture) and counts a different image or description
> (TextureBindingMemoRevalidateMismatches). Counter: TextureBindingMemoRevalidated.
> The memo also keeps always-on totals (hits, stale, revalidated) for tests.
>
> Tests: --texture-memo-revalidate-only (CTest texture_memo_revalidate, _verify
> with VERIFY=exit, _off with the switch off) registers a disjoint image on the
> same page and checks the entry is revalidated in place and equals the full
> resolution, or re-recorded when the switch is off.
>
> Also links xxhash into resource_materialization_tests, which validates
> draw-prep certificates (digest reads since the previous commit).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.cpp`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.h`, `src/graphics/host_gpu/renderer/render.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 195. Draw prep and bindings: in-place certificate checks, lock-free worker hint, one-lock dirty query

Commit: [`5ed9e89e`](https://github.com/Jetsku/KytyPS5-experimental/commit/5ed9e89e1b83200c6c1b3eab5e013fb549d99fbc) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U49 (parallel draw prep, Sky Garden start): DrawPrep::Validate costs the GPU thread 5.1 ms per
> flip, mostly copying 52 MB of certified bytes out of the backing under the global backing mutex
> before comparing them; the preparing workers take every region spinlock the GPU thread takes for
> bindings (10.8k contended tracker locks per flip).
>
> - KYTY_BACKING_INPLACE (default on): Validate compares the certified byte ranges and hashes the
>   digest ranges where they are in the guest backing, through the per-thread mapping record
>   (lock-free and generation-checked as TryReadCachedMapping is; redone under the mapping lock when
>   a map change races it; ranges spanning mappings are gathered under the lock). The gate is the
>   exact one of TryReadGpuCleanBacking, so the bytes seen are those a copy would have returned.
>   Shader-code digests (worker preparations and the serial backing hash) are hashed in place too.
>   KYTY_BACKING_INPLACE_VERIFY=1|exit repeats every validation with copies
>   (FrameEvent.DrawPrepValidateVerify{Checks,Mismatches,Races}); counters
>   DrawPrepValidateInPlace{Ranges,Locked} and BackingInPlaceHashes{,Locked}.
> - KYTY_DRAW_PREP_LOCKFREE_HINT (default on): the workers' GPU-dirty read hint uses a lock-free
>   mirror of each region's GPU-dirty bits, republished under the region lock on every GPU-bit
>   change, instead of the region locks. The hint only gates speculative reads; the commit's exact
>   check decides as before. KYTY_DRAW_PREP_LOCKFREE_HINT_VERIFY=1|exit compares the mirror with
>   the locked bits (FrameEvent.DrawPrepHintVerify{Checks,Mismatches}).
> - KYTY_BUFFER_DIRTY_QUERY_COMBINED (default on): ObtainBuffer's small-read stream decision takes
>   each region lock once (MemoryTracker::QueryDirty) instead of twice, creating missing regions
>   exactly when `!IsRegionGpuModified && IsRegionCpuModified` would
>   (FrameEvent.BufferDirtyQueriesCombined).
> - RegionManager::IsModified, IsHot and the dirty-conflict checks test the page run's words
>   directly (BitArray::AnyInRange) instead of building a masked 1024-bit copy.
>
> Tests: BitArray AnyInRange differential; tracker QueryDirty and the mirror across two regions and
> bit words after every GPU-bit transition; ReadSet::ValidateInPlace against Validate; backing
> inspection inside one mapping (lock-free after the first), across two non-contiguous mappings and
> after an unmap; the code-certificate GPU test checks both validations; ctest variants
> draw_prep_engine_parallel_{inplace_verify,copied}, draw_prep_code_cert_copied and
> buffer_range_memo_separate_queries.

</details>

Changed files: `CMakeLists.txt`, `src/common/bitArray.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/readSet.h`, `src/graphics/shader/shader.cpp`, `src/kernel/memory.cpp`, `src/kernel/memory.h`, `src/kernel/memoryAddressSpace.inc`, `tests/BitArrayTests.cpp`, `tests/DrawPrepTests.cpp`, `tests/MemoryTrackerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`, `tests/VirtualMemoryAllocationTests.cpp`.

### 196. Merge claude/cpu-opt: buffer read-sync range memo, code digests, texture memo revalidation, in-place certificate checks, lock-free worker hint, one-lock dirty query

Commit: [`4ff2377e`](https://github.com/Jetsku/KytyPS5-experimental/commit/4ff2377ed4c67e79cb4820337deb3af1b7853a50) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 197. Diagnostics: command-stream repetition trace (KYTY_CP_REPEAT_TRACE)

Commit: [`e4cae300`](https://github.com/Jetsku/KytyPS5-experimental/commit/e4cae300546a4b33ee97bcf048f09ef72e80d727) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Measures how much of the guest command stream repeats from one guest frame
> to the next, to size cross-frame reuse of prepared draws, bindings and
> recorded command buffers. Off by default; with the switch unset every hook
> is one branch and nothing else runs.
>
> - Guest command buffers: a content hash of every submission (DCB, CE) and
>   of every indirect buffer called or chained to, every frame.
> - Draws (draw-prep inline/parallel): hashes of the draw arguments, render
>   targets, viewports, fixed-function state, shader registers, user SGPRs,
>   prepared program ids, V#/T#/S# words, flattened SRT and user data, the
>   preparation's read set, and the constant/vertex bytes behind the V#s.
>   The preparing thread hashes; the GPU thread appends one record per draw
>   in commit order.
> - Dispatches: CS registers, user SGPRs and group counts.
> - repeat-frames.csv: per frame, for every component and composite key, the
>   multiset match rate against frames -1/-2/-3, any of them, and the
>   positional match against frame -1. Per-draw rows and raw command-buffer
>   dumps for periodic bursts of frames.

</details>

Changed files: `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/repeatTrace.cpp`, `src/graphics/host_gpu/renderer/drawPrep/repeatTrace.h`.

### 198. tests: repeat-trace hashes ignore padding and move with their own inputs

Commit: [`b1a51f10`](https://github.com/Jetsku/KytyPS5-experimental/commit/b1a51f10da0da7add4bc046a3deaebc2bfe669d0) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> repeat_trace_tests (CPU only) checks the KYTY_CP_REPEAT_TRACE draw hashes:
> register snapshots built over different padding bytes hash equally, and a
> user SGPR, render target, viewport, blend state, shader address, index
> address/count, V# base/record count, T# or flattened SRT change only the
> component(s) that cover it.

</details>

Changed files: `CMakeLists.txt`, `tests/RepeatTraceTests.cpp`.

### 199. Repeat trace: hash only the leading 4 KiB of each V# payload by default

Commit: [`f1c088d9`](https://github.com/Jetsku/KytyPS5-experimental/commit/f1c088d907a763fa7d78f6cc856b5eccbb2c6b00) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Vertex-fetch V#s can span megabytes; hashing up to 64 KiB each on the draw-prep
> workers would slow the diagnostic run itself. The payload hash now covers the
> first KYTY_CP_REPEAT_PAYLOAD_PER_VSHARP bytes (default 4096) of every V#, at
> most KYTY_CP_REPEAT_PAYLOAD_LIMIT (default 16384) per draw.

</details>

Changed files: `src/graphics/host_gpu/renderer/drawPrep/repeatTrace.cpp`, `src/graphics/host_gpu/renderer/drawPrep/repeatTrace.h`.

### 200. Merge claude/cp-architecture: per-frame and per-draw repetition trace (KYTY_CP_REPEAT_TRACE, off by default)

Commit: [`248c00c0`](https://github.com/Jetsku/KytyPS5-experimental/commit/248c00c043ca5b197f508cb4c0ec17c9e328443f) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 201. Draws: keep an unsampled depth target's attachment layout while it allows the draw's writes (KYTY_DEPTH_LAYOUT_STABLE)

Commit: [`9b3f2b00`](https://github.com/Jetsku/KytyPS5-experimental/commit/9b3f2b00330f88662d5edf9d49e413ee431edef7) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> depth_attachment_layout() picked the narrowest layout for each draw's write aspects. Draws that
> alternate stencil writes (a stencil mark, then draws that only test it) therefore toggled the
> depth target between DEPTH_STENCIL_ATTACHMENT and DEPTH_ATTACHMENT_STENCIL_READ_ONLY: a layout
> transition, a barrier and a new rendering instance before every such draw. In the U49 Sky Garden
> start-view RenderDoc capture 92 of the frame's 177 rendering instances began this way (121 of
> them restarted on the same attachments).
>
> The layout of an attachment that the draw does not sample is not observable: every standard
> depth/stencil attachment layout allows the tests, and a writable one the writes. A draw that
> samples no aspect of its depth target now keeps the image's current layout when that is a
> standard attachment layout for its aspects allowing the draw's writes, and otherwise takes the
> fully writable layout (depth_stable_attachment_layout). Sampling draws keep the readable or
> feedback-loop layouts exactly as before.
>
> KYTY_DEPTH_LAYOUT_STABLE=0 restores the per-draw layouts. FrameEvent
> DepthLayoutTransitionsAvoided counts the draws that kept a layout the old rule would have
> changed. Tests: --depth-layout-stable-only (layout rule table; draw sequence through
> AcquireRenderTargets in both modes), in the full run.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/depthRenderTarget.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 202. LOD stats: draws without a mip-statistics counter use the pixel program compiled without feedback (KYTY_LOD_STATS_PLAIN_VARIANT)

Commit: [`be9a7bdb`](https://github.com/Jetsku/KytyPS5-experimental/commit/be9a7bdbb83b80e6445c72abb817fdbf40637bc0) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> An instrumented pixel shader records GET_LOD_STATS feedback only for images whose T# has a
> counter (the per-draw field's no-counter bit). Its storage-buffer atomics nevertheless make the
> driver run the depth/stencil tests after the shader whenever the shader can discard, so occluded
> fragments of alpha-tested draws were shaded (and sampled) in full. Replay of the U49 Sky Garden
> start-view capture (EventGPUDuration, shader replacement): removing the feedback from all 87
> instrumented shaders saved 1.0-2.3 ms per frame; removing it from the 49 late-Z (discard) shaders
> alone saved the same (0.8-1.4 ms per frame), from the 38 early-Z shaders nothing. Removing only
> the LOD query, or the gating load, or making that load L1-cacheable changed nothing. In the U49
> hang trace only about 1.4% of texture descriptor resolutions carry MipStatsCntEn.
>
> Every instrumented pixel permutation is now also emitted without the feedback (same program,
> bindings and pipeline layout; the mip_stats binding stays declared: Spirv::EmitProgram
> mip_stats_records=false, CompileOptions::plain_mip_stats_variant, Permutation::plain). A draw
> whose pixel-stage fields all have the no-counter bit uses that plain program; the instrumented
> one would have recorded nothing, so the rendering and the reported statistics are identical. The
> plain draw also counts as barrier-safe for the mip-statistics binding (DrawIsBarrierSafe).
>
> KYTY_LOD_STATS_PLAIN_VARIANT: 1 (default) as above; 0 always the instrumented program; verify
> keeps the instrumented program for such draws but binds a canary counter buffer that every
> GET_LOD_STATS copies and checks for its reset state (LodStatsCanaryChecks /
> LodStatsCanaryMismatches, and a log line naming the first changed counter). FrameEvents
> LodStatsPlainDraws / LodStatsInstrumentedDraws count the choice. Test: CodegenLodStatsPlainVariant
> (--lod-stats-codegen-only, --codegen-only).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/lodStats.h`, `src/graphics/host_gpu/renderer/lodStatsReport.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/shader/recompiler/ShaderRecompiler.cpp`, `src/graphics/shader/recompiler/ShaderRecompiler.h`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterImage.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `tests/ShaderCodegenTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 203. Buffer uploads: copy the staged host bytes to VRAM on the copy engine (KYTY_UPLOAD_DMA)

Commit: [`0f17ad8e`](https://github.com/Jetsku/KytyPS5-experimental/commit/0f17ad8eb0bbfe84a9e61b86c28802dc9b543355) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> CPU-dirty buffer uploads are staged in the 512 MiB host upload ring and copied into the
> device-local cache buffers by the graphics queue, which then reads the staged bytes over PCIe:
> about 80 us/MB on the RTX 3090 (Gen3 x16, resizable BAR off). In the U49 Sky Garden start-view
> capture that is 20 MB and 1.3-1.6 ms of graphics-queue time per frame (79 us/MB measured), 1.2 ms
> of it one 16 MB BDA-synchronized buffer uploaded every frame before the mesh-shader draws (hang
> trace: bda-sync uploads, about 20 MB per flip).
>
> UploadDma moves the host -> VRAM part to a queue of the transfer-only family (the copy engines):
> BufferCache::StageUploadDma reserves room in a 64 MiB device-local ring and queues a copy of the
> upload's packed staging range into it; a worker submits the queued copies in batches that signal
> a timeline semaphore. The graphics copies then read the ring (VRAM to VRAM) and the guest
> submission of the recording that staged them waits for that value at the transfer stage (a second
> SubmitDependency slot of the CommandScheduler, with per-dependency wait stages). Data and the
> order of the graphics copies are unchanged. Ring space is reused only after the guest tick that
> read it completed; a transfer batch writing reused space also waits on the master semaphore for
> that tick. Uploads below 64 KiB, a full ring, the non-batched upload path and temporary staging
> buffers keep the direct copy. The device gets the extra queue at creation (not under RenderDoc);
> the staging ring and the DMA ring are shared by both families (concurrent sharing).
>
> Switches: KYTY_UPLOAD_DMA=0 reverts (no transfer queue); KYTY_UPLOAD_DMA_MB (64) ring size;
> KYTY_UPLOAD_DMA_MIN_KB (64) smallest upload moved. KYTY_UPLOAD_DMA_VERIFY=1 copies every staged
> range back from the ring in the recording that reads it and compares it at completion with a
> snapshot of the staged host bytes (UploadDmaVerifyChecks / UploadDmaVerifyMismatches). FrameEvents
> UploadDmaCopies, UploadDmaBytes, UploadDmaRingFull, UploadDmaSubmits, UploadDmaSubmitWaits.
> Test: --upload-dma-only (a 2 MiB CPU-dirty upload through the BufferCache arrives intact; ring
> allocation refuses space the recording still reads, reuses it after completion, and leaves small
> uploads direct); the test harness now creates the transfer queue like the emulator.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/graphicContext.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/streamBuffer.cpp`, `src/graphics/host_gpu/renderer/cache/streamBuffer.h`, `src/graphics/host_gpu/renderer/cache/uploadDma.cpp`, `src/graphics/host_gpu/renderer/cache/uploadDma.h`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.h`, `src/graphics/presentation/window/vulkanWindow.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 204. tests: release the UploadDma check's guest mapping before the context goes away

Commit: [`b63df479`](https://github.com/Jetsku/KytyPS5-experimental/commit/b63df479a676482c7b9e3b140b92060e31aca74e) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The PageManager fail-fast (live page state at destruction) ended the process after the check
> passed. Unmap, finish, shut the GPU thread down and release the direct memory like the other
> BufferCache checks.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 205. Merge claude/gpu-efficiency: LOD-stats plain shader variants, upload DMA ring on a transfer queue, stable depth layouts

Commit: [`4acfc385`](https://github.com/Jetsku/KytyPS5-experimental/commit/4acfc385d4711a5e646947c7af9f01ecfcc806b2) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 206. Draw prep: log-mode certificates by default (KYTY_DRAW_PREP_CERT=value reverts)

Commit: [`8fb76567`](https://github.com/Jetsku/KytyPS5-experimental/commit/8fb765678be73893231edb5f38aff57c7dbee202) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Run A (U50, repeat trace plus KYTY_DRAW_PREP_LOG_AUDIT=1, 1,946 audited
> flips over the fly-in, start view, pool, hub and desert) found no draw that
> the log-mode certificate would have accepted with changed bytes
> (DrawPrepLogMissed 0 in every scene) and at most 0.24 extra rejections per
> flip (DrawPrepLogWouldReject). At U50 the value certificate costs 4.1-4.7
> ms/flip on the command processor, mostly re-hashing about 50 MB/flip of
> certified shader code (DrawPrepCertDigestBytes); log mode checks the
> coherence log and the clean verdicts instead.
>
> Coherence::LogReadersEnabled() now records log entries whenever draw prep
> runs (KYTY_DRAW_PREP=inline|parallel) unless KYTY_DRAW_PREP_CERT=value, or
> with the audit. Without that, the flipped default would find an empty log
> and every certificate would fail as Unknown (serial preparation for every
> draw).
>
> The value path keeps GPU-test coverage: draw_prep_engine_parallel_value and
> draw_prep_engine_parallel_value_audit. The audit needs
> KYTY_DRAW_PREP_CERT=value KYTY_DRAW_PREP_LOG_AUDIT=1.
>
> Merge only after the performance A/B (CP-ARCHITECTURE-20260927.md, Run B)
> and after `ctest -R draw_prep_engine` with the GPU free.

</details>

Changed files: `CMakeLists.txt`, `src/graphics/host_gpu/coherenceLog.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`.

### 207. Merge claude/cp-architecture: log-mode draw-prep certificates by default

Commit: [`51541266`](https://github.com/Jetsku/KytyPS5-experimental/commit/51541266f174828a9f2343b6d3a921cc281dfc60) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 208. Draw prep: parallel by default

Commit: [`3a423964`](https://github.com/Jetsku/KytyPS5-experimental/commit/3a4239644bf158f727bdd328d414fb914714ace4) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every draw-prep verify counter stayed at 0 in Astro Bot's Sky Garden (fly-in,
> start view, pool), hub and desert (run and slide) across U48-U50, with a 97-99%
> commit rate; the start view gained 26% in U48. KYTY_DRAW_PREP=off reverts.

</details>

Changed files: `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`.

### 209. Colour targets: apply CMASK fast clears (KYTY_CMASK_FAST_CLEAR)

Commit: [`8aeaf98f`](https://github.com/Jetsku/KytyPS5-experimental/commit/8aeaf98f0468d4640aa533f3fbe745f8ceb7a2d8) · **Compressed metadata and clear semantics**

Handles the named metadata/clear operation without stale contents; eligible GPU work avoids CPU readbacks. Accuracy and lower transfer cost are separate claims; see the water discussion.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Sky Garden leaves invisible in a middle-distance band, and the hub planet's dark speckles
> (U49 RenderDoc captures 1-4):
> - The missing leaves are not discarded or depth-failed: their vertex positions are NaN
>   (RenderDoc post-VS: e.g. eid 49394 instances 1-3 all NaN, 49359 instance 0 3966 of 6048
>   triangles), in the depth prepass and the G-buffer pass alike.
> - The first NaN in the vertex shader (RenderDoc debugger on a DenormPreserve-free copy) is a
>   sample of the foliage-interaction field (1024x1024 RGBA16F ping-pong pair at 0x5627f0000 /
>   0x562ff0000): texels holding 0xfff0/0xffff/0x7fff halves.
> - That field is simulated each frame from an input render target at 0x516830000, which the
>   game stamps with 42 additive quads. The game fast-clears that target every frame through
>   CMASK: its generic metadata fill writes 0 over the 8 KiB CMASK at 0x56c05e000 before the
>   stamp pass, and a fast-clear eliminate names the target afterwards (U47 cp.csv:
>   cb-meta-op 0x516830000 mode 2 cmask 0x56c05e000, 4810 times). Kyty ignored CMASK fast clears,
>   so the target was never cleared: stamps accumulated (R/G saturated at 2048) and 256 KiB of
>   depth HTILE clear values (0xfffffff0) that aliased its memory during a level load stayed
>   in it forever. The simulation turned them into NaN, which spread (hub: 880299 NaN texels;
>   Sky Garden 40960 at 18:09, 44290 at 18:11, 125928 at 18:13), and leaves whose tree maps
>   into the NaN region (a distance band around the camera) got NaN positions.
>
> Now a render-target binding with CB_COLORn_INFO.FAST_CLEAR and a CMASK (single sample, one
> mip, 2D, no DCC) carries the CMASK range (4 KiB blocks of 1024x512 pixels, the Gen5 block
> of the DCC formula with 4-bit 8x8 elements; 8 KiB for 1024x1024) and CLEAR_WORD0/1. When all
> CMASK bytes of a bound slice are 0 (every tile fast-cleared), TextureCache::
> MaterializeCmaskClear clears the slice to the CLEAR_WORD colour (64-bit colours decoded for
> 8-byte formats) and leaves the CMASK 0xFF (expanded), as the colour block's reads and the
> eliminate would; a later binding keeps what the draws wrote. The proof is exact: guest
> memory for CPU-owned bytes, a recorded uniform fill for GPU-owned ones, otherwise the DCC
> helper's native inspection with only code 0x00 accepted (validation dispatch plus indirect
> clear, no readback; a CMASK-only helper instance when KYTY_DCC_GPU=1 did not create one),
> or a readback if no native inspection is possible. Bytes an image covers, partial or other
> values are left alone. With KYTY_CB_METADATA_MATERIALIZE=1 the eliminate itself also
> materializes, as for DCC. TryMaterializeGpuDccClear's native part is now the shared
> TryMaterializeGpuMetadataClear (DCC behaviour unchanged).
>
> Switches: KYTY_CMASK_FAST_CLEAR=0 ignores CMASK fast clears as before; KYTY_CMASK_NATIVE=0
> decides GPU-owned CMASK bytes by readback instead of the native inspection.
> Counters: FrameEvent CmaskFastClears, CmaskFastClearInspections, CmaskFastClearInspection-
> Reuses, CmaskFastClearReadbacks, CmaskFastClearExpanded, CmaskFastClearUnproven,
> CmaskFastClearAliased, CmaskFastClearFormat, CmaskFastClearShape.
> Test: shader_recompiler_compute_tests --cmask-fast-clear-only (RenderExecutorCmaskFastClear:
> the game's fill kernel, CPU-written CMASK, expanded and partial CMASK, rebinding, FAST_CLEAR
> off; also run with KYTY_CMASK_NATIVE=0 and KYTY_DCC_GPU=1), and in the image-overlap,
> compute-meta-clear and full runs.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/tile.cpp`, `src/graphics/guest_gpu/tile.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/colorRenderTarget.cpp`, `src/graphics/host_gpu/renderer/colorRenderTarget.h`, `src/graphics/host_gpu/renderer/image/imageInfo.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 210. Merge claude/gpu-opt: materialize CMASK fast clears of colour targets (Sky Garden leaf band)

Commit: [`9d4331dc`](https://github.com/Jetsku/KytyPS5-experimental/commit/9d4331dcded1aa6c021e9f5a63a15b24216011de) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 211. Draw prep: always record the coherence log; log-mode refusals decided by value

Commit: [`28d345dd`](https://github.com/Jetsku/KytyPS5-experimental/commit/28d345ddcf3fd4c69befe2af8fac85578f7cca4b) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U51 (log certificates by default, KYTY_DRAW_PREP left at its new parallel
> default) refused 170-210 draws per flip as DrawPrepFallbackCoherenceLog where
> the U50 audit predicted 0.07-0.24.
>
> Cause: Coherence::LogReadersEnabled() only recorded log entries when
> KYTY_DRAW_PREP=inline|parallel was spelled out in the environment. After
> 3a423964 made parallel the default, the U51 runs no longer set it, so every
> transition only bumped the generation. Log::Check then found an unreadable
> entry for any interval containing a transition and answered Unknown, and
> every such draw fell back to the serial preparation (hence also the higher
> StagePrepParallelDraws, StagePrepHelper and StagePrepJoin). The audit run
> set KYTY_DRAW_PREP_LOG_AUDIT=1, which enabled recording, so it could not see
> this.
>
> - The log now always records (LogReadersEnabled() is constexpr true):
>   nothing depends on the environment, so it cannot disagree with
>   DrawPrep::GetMode() or GetCertMode() again. An append costs a few stores.
> - A log-mode certificate whose log check does not pass (conflict, unreadable
>   entry, overflow) is decided by the value check instead of being refused.
>   Log mode therefore refuses exactly what the value certificate refuses; it
>   only skips the byte and digest comparisons where no transition touched a
>   certified range (DrawPrepLogMissed, the race case, was 0 in all five U50
>   scenes).
> - New counters: DrawPrepLogConflicts, DrawPrepLogUnknown,
>   DrawPrepLogOverflows, DrawPrepLogValueRescues (accepted by value after the
>   log said no). DrawPrepFallbackCoherenceLog no longer occurs.
> - DrawPrepTests: the global log records transitions and content writes with
>   no environment variable set.
>
> KYTY_DRAW_PREP_CERT=value still reverts to value certificates.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/coherenceLog.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `tests/DrawPrepTests.cpp`.

### 212. Merge claude/cp-architecture: always record the coherence log; log-mode refusals decided by value

Commit: [`8194c54c`](https://github.com/Jetsku/KytyPS5-experimental/commit/8194c54caecf7f0284a0786f6469016d291748a6) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 213. Game patches: KYTY_GAME_PATCH_DISABLE skips mods by name at load

Commit: [`23ba1e24`](https://github.com/Jetsku/KytyPS5-experimental/commit/23ba1e249d0371e53245f6dd4b12d500fdfc77a7) · **Workflow and compatibility controls**

Supports controlled launches, navigation or documentation. This does not optimize rendering; private operational documents are excluded from the publication.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_GAME_PATCH_DISABLE=<name substring>[;<name substring>...] removes the
> matching mods of the --game-patch file when it is loaded, so a run can drop
> one mod without editing the JSON. Matching is a case-insensitive substring of
> the mod "name"; ';' and '|' both separate patterns ('|' because
> Start-Comparison -ExtraEnv values cannot contain ';'). Unset or empty changes
> nothing.
>
> The loader now logs, to the console and the log, each mod as applying or
> skipping (disabled in the file, or which pattern matched), every pattern that
> matched no mod, and when no mod writes are left. Astro Bot examples:
>   non-tiled   -> skips mod 1 (deferred-lighting renderer selection)
>   GI probes   -> skips mod 2 (GI probes and lighting shaders)
>   non-tiled|GI probes -> skips both
>
> The selection logic is a pure header (loader/gamePatchFilter.h) with a CPU
> test, game_patch_filter_tests.

</details>

Changed files: `CMakeLists.txt`, `src/loader/gamePatch.cpp`, `src/loader/gamePatchFilter.h`, `tests/GamePatchFilterTests.cpp`.

### 214. Merge claude/rt-tiled-lighting: KYTY_GAME_PATCH_DISABLE switch for game-patch mods

Commit: [`06a6a146`](https://github.com/Jetsku/KytyPS5-experimental/commit/06a6a146f3b8125c90584714928f2d338b719e2b) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 215. tests: streamed head textures keep sampling their MIN_LOD tail level

Commit: [`955964da`](https://github.com/Jetsku/KytyPS5-experimental/commit/955964da76f93c4fde41643924f394339b854225) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot streams textures as "heads": only the GFX10 mip tail block is loaded
> and the T# MIN_LOD is the first tail level (4.0 for a 2048^2 BC7 head). The
> new case (--storage-mip-host-only, StreamedHeadMinLodResidency) drives the real
> TextureCache residency, view and sampler path with a 512^2 R32F chain whose
> first tail level is 3:
> - only the 64 KiB tail is resident; IMAGE_SAMPLE_L at LOD 0..6 returns the
>   tail's level 3 with the non-resident levels poisoned;
> - MIN_LOD 2.5 extends residency to level 2, MIN_LOD 0 promotes the chain in
>   place and every level reads its guest data;
> - IMAGE_GATHER4_LZ and IMAGE_LOAD_MIP of level 0 are printed: Vulkan returns
>   zero below the view's minimum LOD (0,0,0,0 and 0 on the RTX 3090).

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 216. Samplers: honour the S# FILTER_MODE min/max reduction (KYTY_SAMPLER_REDUCTION)

Commit: [`ec1d46f5`](https://github.com/Jetsku/KytyPS5-experimental/commit/ec1d46f541705403cb2eba6e8c764cc11f606090) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RDNA2 ISA 8.2.7: S# bits 29..30 select how the filter footprint is combined,
> 0 = blend, 1 = min, 2 = max. The host sampler always blended, so a min or max
> sampler (depth pyramids, conservative downsampling) returned the weighted
> average. SamplerCache now chains a VkSamplerReductionModeCreateInfo when the
> device has samplerFilterMinmax (Vulkan 1.2, enabled when supported; logged at
> startup). Depth-compare samplers keep the average, as Vulkan requires.
>
> Other S# fields the host does not model (force/skip degamma, trunc_coord,
> mc_coord_trunc, aniso threshold/bias, perf_mip/perf_z, lod_bias_sec,
> mip_point_preclamp, blend_zero_prt, or a reduction that could not be applied)
> are logged for the first 32 samplers that use them, so a normal run shows
> whether a title depends on them.
>
> KYTY_SAMPLER_REDUCTION=0 restores the blend-only behaviour.
> Test: --storage-mip-host-only SamplerFilterMode (4x4 R32F, blend/min/max at a
> texel corner = 8.5/6/11; fails with the switch off).

</details>

Changed files: `src/graphics/host_gpu/graphicContext.h`, `src/graphics/host_gpu/renderer/cache/samplerCache.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 217. Shaders: apply IMAGE_SAMPLE*_O texel offsets and the IMAGE_SAMPLE*_CL LOD clamp

Commit: [`e442f4d7`](https://github.com/Jetsku/KytyPS5-experimental/commit/e442f4d7bd7f42f2c8d4a3f3ed94630711cd8089) · **LOD feedback and texture streaming**

Keeps feedback/residency aligned with what sampling needs, avoiding unnecessary streaming or upload work where possible. Report ordering and view clamps are correctness requirements; no per-commit FPS attribution is available.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Offsets (KYTY_SAMPLE_OFFSETS, default on). RDNA2 ISA 8.2.5: the first address
> dword of a *_O sample holds six-bit signed x/y/z texel offsets. Only gathers
> used them; every IMAGE_SAMPLE*_O read the unshifted texel. Astro Bot's corpus
> has ten IMAGE_SAMPLE_LZ_O in two pixel shaders, e.g. ps_2936e5eca99d75e7 reads
> the eight neighbours of the current pixel (offsets (+-1, +-1)) next to a
> reprojected history sample and min/max-clamps it: with the offsets dropped all
> eight neighbours were the centre texel. Constant offsets in -8..7 (the range
> every Vulkan device supports) become ConstOffset; a literal written under the
> same EXEC that masks the sample's result counts as constant. Other offsets move
> the normalized coordinate by offset / size of the level read (exact for single
> level reads up to rounding; logged once).
>
> LOD clamp (KYTY_SAMPLE_LOD_CLAMP, default on). The _CL clamp is the last
> address component (Table 43) and was neither counted nor applied. Implicit and
> gradient samples now take it as the MinLod image operand (shaderResourceMinLod,
> enabled when supported and logged at startup); explicit-LOD forms (every sample
> outside pixel shaders) raise their LOD to it, and GET_LOD_STATS records the
> clamped LOD. No _CL sample occurs in the Astro Bot corpus.
>
> Tests (--storage-mip-host-only): SampleLzOffsets (4x4 texture, offsets
> (+1,0) (0,+1) (-1,-1) (+9,0) (-8,0)), SampleLodClamp (_CL 2.0, _D_CL 2.5, _CL
> 0 on a 1/2/4/8 chain). ImageSampleA16OffsetKeepsTexelOffset32BitOnGpu expected
> the unshifted texel; it now expects the offset texel and a ConstOffset.

</details>

Changed files: `src/graphics/presentation/window/vulkanWindow.cpp`, `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterImage.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/frontend/decode/ImageOps.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 218. Shaders: DPP row_share lanes and DPP8 on VOP2/VOPC

Commit: [`0d11b4e0`](https://github.com/Jetsku/KytyPS5-experimental/commit/0d11b4e033273f2be87fa1ddcf041bc6aeea0c84) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - DPP_ROW_SHARE (GFX10 dpp_ctrl 0x150-0x15F) fell through to the identity
>   lane, a silent wrong result. Every lane now reads lane (ctrl & 15) of its row.
> - DPP8 (src0 = 233) and DPP8 with FI (234) were decoded only for VOP1, and 234
>   was parsed as a DPP16 control word. VOP2 and VOPC now accept both, FI fetches
>   inactive lanes, and the lane-select bits are no longer read as src1 neg/abs.
>
> Neither occurs in the Astro Bot corpus (quad_perm and row_shr only).
> Tests (--dpp-only): VectorDppRowShare, VectorDpp8Vop2.

</details>

Changed files: `src/graphics/shader/recompiler/backend/spirv/spirvEmitterHelpers.cpp`, `src/graphics/shader/recompiler/frontend/decode/VectorAluOps.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 219. Shaders: unpack compressed SNORM16_ABGR colour exports as snorm

Commit: [`c1474f17`](https://github.com/Jetsku/KytyPS5-experimental/commit/c1474f17c8fe2cd059152158a1a75ce70a20050f) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> SPI_SHADER_COL_FORMAT 6 (SNORM16_ABGR) exports carry V_CVT_PKNORM_I16_F32
> pairs, but compressed exports were unpacked as FP16 unless the format was
> UNORM16. MRT0 rejects the mode, MRT1-7 were silently wrong. SINT16_ABGR (8)
> would need an integer output and stays FP16, now with a one-time warning.
>
> Test: --codegen-only CodegenCompressedSnormExport.

</details>

Changed files: `src/graphics/shader/recompiler/backend/spirv/spirvEmitterFlow.cpp`, `tests/ShaderCodegenTests.inc`.

### 220. Diagnostics: log unmodelled compute float modes and buffer OOB_SELECT modes

Commit: [`06b44f6c`](https://github.com/Jetsku/KytyPS5-experimental/commit/06b44f6c208bed024ac894f55769ad834092a876) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Passive data for two open ISA review items; no behaviour change.
> - COMPUTE_PGM_RSRC1: compute shaders are recompiled with the graphics float
>   state (FLOAT_MODE 0xC0, DX10_CLAMP, IEEE off). The first eight dispatches
>   that ask for anything else are logged (RDNA2 ISA 6.4).
> - Storage-buffer V#s: the binding range is stride * NUM_RECORDS whatever the
>   OOB_SELECT (Table 35: modes 0 and 3 check differently). Each combination of
>   OOB_SELECT, stride, swizzle and ADD_TID is logged the first time it is bound.

</details>

Changed files: `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`.

### 221. Samplers: also log a non-zero S# LOD bias

Commit: [`5f3441e0`](https://github.com/Jetsku/KytyPS5-experimental/commit/5f3441e08f6a293057c930be599c9da26c5a6388) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Vulkan adds VkSamplerCreateInfo::mipLodBias to explicit-LOD samples too, so
> IMAGE_SAMPLE_L/_LZ with a biased S# read a different level than the texture
> unit most likely does (LLVM folds image.sample.l with LOD <= 0 into
> image.sample.lz, which is only valid if the S# bias does not apply to _L).
> The sampler log now lists the bias so a run shows whether this matters.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/samplerCache.cpp`.

### 222. Shaders: shorter denormal flush for transcendental inputs; opt-in host FTZ

Commit: [`dfc91e00`](https://github.com/Jetsku/KytyPS5-experimental/commit/dfc91e001500e8bae31b4497111ebe340b96501d) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> V_RCP/RSQ/SQRT/EXP/LOG_F32 flush denormal inputs to zeros of the same sign.
> The shader-side flush tested "non-zero" before "below the smallest normal";
> a zero is its own sign bits, so the magnitude test alone suffices (two
> fewer instructions per transcendental, identical results).
>
> KYTY_HOST_FTZ_INPUTS=1 skips the flush when the module declares
> DenormFlushToZero 32. Vulkan only says such operands "may" be flushed, so
> it is opt-in; the RTX 3090 (driver 610.74) has no f32 flush-to-zero.
> CodegenTranscendentalDenormInputs checks 8,192 inputs (half denormals of
> both signs) against the signed-zero results in both variants.

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterAlu.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterAluHelpers.cpp`, `tests/ShaderCodegenTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 223. Shaders: drop EXEC-masked selects that no lane observes (KYTY_EXEC_SELECTS)

Commit: [`c828d688`](https://github.com/Jetsku/KytyPS5-experimental/commit/c828d688c88cf5d5a37139e8a128778f489c48ed) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every EXEC-masked VGPR write is Select(exec, new, old). IR::EliminateExecSelects
> (run in CompileProgram after resource specialization) replaces a select by
> the arm every observer sees:
> - one lane per host invocation: the emitter branches per invocation, so a
>   block entered only through a branch edge knows that branch's condition
>   (s_cbranch_execz regions know EXEC is set);
> - Select(c, Select(e, t, f), z) with c => e reads t, and
>   Select(c, x, Select(e, t, f)) with e => c reads f;
> - when every use of Select(c, t, f) is masked by c (true arm of a select
>   or an AND on c, an access guarded by c) or feeds lane-local arithmetic,
>   a join phi or a loop phi that only feeds false arms of such selects
>   (greatest fixpoint, at most 8 passes), t replaces it. Cross-lane ops,
>   implicit-derivative samples, references and unguarded side effects
>   count as observations.
> Resource-plan values and their operands are left alone; branch conditions
> keep their instruction. ShaderLanesPerInvocation now gives the emitter and
> the pass the same lane_count.
>
> Corpus (141 CS/PS dumps, null resources): SPIR-V function instructions
> 912,789 -> 852,907 (-6.6%), OpSelect 106,921 -> 50,245. vkCreateComputePipelines
> on the RTX 3090 for the 52 corpus compute shaders: 10.75 s -> 8.43 s (-22%),
> e.g. cs_900aba8df9448d3d 2245 -> 1665 ms, cs_5bb5e709d757909d 643 -> 285 ms.
> GPU time does not change on NVIDIA (a 4096-op EXEC region with 8,213 selects:
> 181 vs 183 us wave64, 150.5 vs 150.5 us wave32); the driver predicates them.
>
> Tests: CodegenExecSelects (wave64 and wave32, branch and branchless regions,
> a V_READLANE of an inactive lane, CPU reference, on vs off). Tools:
> --exec-selects-bench (GPU time), --corpus-pipeline-times <dir> (driver
> compile time), --corpus-spirv-stats <file> [out] (SPIR-V mix, disassembly
> and IR dump); Dispatch can repeat and time a dispatch.

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/ShaderRecompiler.cpp`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/ir/passes/ExecSelectElimination.cpp`, `src/graphics/shader/recompiler/ir/passes/ExecSelectElimination.h`, `src/graphics/shader/shader.h`, `tests/ShaderCodegenTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 224. Merge claude/isa-accuracy-3: sample offsets, LOD clamp, sampler reduction, DPP fixes, EXEC select elimination, shorter denormal flush

Commit: [`b24ad498`](https://github.com/Jetsku/KytyPS5-experimental/commit/b24ad4981134e7f143d8c33f69393f0551b34ea6) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 225. Range memo verify: count a write fault racing the re-evaluation as a race, not a mismatch

Commit: [`5b798560`](https://github.com/Jetsku/KytyPS5-experimental/commit/5b7985607637c34ff2a2c77db3c676b64f2157cd) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U50 check run: KYTY_BUFFER_RANGE_MEMO_VERIFY reported about one mismatch per 30 s at the Sky
> Garden start ("a skipped synchronization found pages to upload", "a small read binding changed
> its stream decision"), always for ranges whose pages the guest writes.
>
> The skips themselves are exact. A hit reads the range signature (the sum of the regions'
> mutation serials) lock-free and acts at that moment; every transition that turns a page
> CPU-dirty advances its region's serial under the region lock BEFORE the bit changes (write
> faults with hot promotion and fault-ahead, explicit CPU-dirty marks, new regions, untracking),
> so a page is either dirtied after the read (the skip precedes it, like a sync done a moment
> earlier) or the signature differs and there is no hit.
>
> The verify mode, however, re-evaluates the hit later, under the region locks: a guest write
> fault landing in between (it often holds the very region lock the re-evaluation waits for, while
> it changes page protection) dirties a page the hit legitimately did not see, and was reported as
> a mismatch. Host writers of backing bytes (LOD reports, occlusion dumps) never touch tracker bits
> at all, so they cannot produce either message.
>
> - Every region now also keeps a dirtying serial (RegionManager::Dirtied), advanced under the lock
>   before pages turn CPU-dirty and never by clears; MemoryTracker::RangeDirtiedSignature sums it.
> - The verify mode reads it before the lookup. A difference that pages turned dirty since explain
>   counts as BufferRangeMemoVerifyRaces; only an unexplained one (or a Stream fact whose range is
>   no longer CPU-dirty, or GPU-dirty now: transitions only this thread makes) is a mismatch.
> - A test hook (BufferCache::s_range_memo_verify_hook) lands a write fault exactly between a hit
>   and its re-evaluation; before this change buffer_range_memo_verify (exit mode) stopped on it.
>
> Tests: tracker dirtying serial moves on faults and changing marks only; the range-memo GPU test
> checks a write from another host thread invalidating a fact, and in verify mode a racing write
> for a skipped synchronization and for a small read's decision (a race, the bytes copied).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `tests/MemoryTrackerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 226. Buffer bindings: lock-free dirty snapshots on the GPU thread (KYTY_TRACKER_RELAXED_QUERIES)

Commit: [`8247814b`](https://github.com/Jetsku/KytyPS5-experimental/commit/8247814b0e52a7b98928c16fbc3351d403eb9297) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U49 WPR, Sky Garden start: ObtainBuffer's small-read decision took 4.6 ms/flip in two locked
> tracker queries, read synchronizations of clean ranges another 3 ms, and the region spinlocks
> were contended ~11k times per flip against the guest threads' write faults.
>
> Every region now also publishes lock-free copies of its CPU-dirty bits (as it already did for
> its GPU-dirty bits), under its lock after each change and before any page the change makes
> CPU-dirty becomes writable. MemoryTracker::QueryDirtyRelaxed reads both mirrors without a lock.
> On the GPU thread it is exact: only that thread clears CPU-dirty bits (uploads, hot-page settles)
> and sets GPU-dirty bits (written uploads), while other threads only set CPU-dirty bits (write
> faults) and clear GPU-dirty ones (readback completion). A page seen CPU-dirty therefore stays so,
> and one seen not GPU-dirty stays so, during the call: `!gpu && cpu` (the stream decision) and
> `!cpu` (nothing to upload) equal what a locked query gives at some moment of the call.
>
> - ObtainBuffer's small-read stream decision uses it (before the range memo).
> - A read-only, non-texel SynchronizeBuffer returns at once when no page of the range is
>   CPU-dirty (hot pages are CPU-dirty too), before the range memo.
> - A range with a region that does not exist yet keeps the locked path (which creates it).
> - KYTY_TRACKER_RELAXED_QUERIES=0 restores the locked queries. KYTY_TRACKER_RELAXED_VERIFY=1|exit
>   follows every relaxed answer with the locked query: only the transitions other threads make,
>   and only while the range's mutation serials moved, may separate them (counted as races).
> - Counters TrackerRelaxed{Queries,SyncSkips,VerifyChecks,VerifyMismatches,VerifyRaces}; test
>   totals BufferCache::m_relaxed_totals.
>
> Tests: the mirrors equal the locked bits after every tracker transition (faults, uploads, hot
> promotion/settle/demotion/sweep, readbacks, downloads, written uploads) and a range with a
> missing region is left to the locked path; the range-memo GPU test binds on the GPU thread,
> where the relaxed queries apply, and checks their skips; ctest buffer_range_memo_relaxed_verify
> and buffer_range_memo_locked_queries.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `tests/MemoryTrackerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 227. Sync epoch; BDA synchronization once per epoch (KYTY_SYNC_EPOCH, KYTY_BDA_SYNC_EPOCH)

Commit: [`85aef5a3`](https://github.com/Jetsku/KytyPS5-experimental/commit/85aef5a34f1558aa8a36fae77c9cc13bd1be6940) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A guest CPU write only has to become visible to GPU work at a point that orders the two: the
> start of a submission, a wait on memory (WAIT_REG_MEM: how the GPU waits for a CPU-written
> label), a cache invalidation (ACQUIRE_MEM, SURFACE_SYNC), or a command-processor packet that
> writes memory itself. Between two such points a CPU write races the GPU on the real hardware
> too (survey round 2, section 3.1; Ryujinx sequence numbers, shadPS4 #5100 sessions).
>
> SyncEpoch (syncEpoch.h) is a global counter that advances conservatively:
> - before every packet the draw-prep classifier calls a fence, except register loads from memory
>   (SET_*_REG_INDIRECT only read memory) - DrawPrep::AdvancesSyncEpoch, for every queue;
> - at the start of every command-processor slice (a new submission, or other queues ran);
> - after every service command the command processor runs (mapping changes, readbacks, deferred
>   label writes) and on every GPU map/unmap.
>
> BDA synchronization (SynchronizeBdaBuffers, once per draw that uses DMA) now runs once per epoch:
> after a completed pass, memory the pass left clean can only need an upload again within the same
> epoch through a guest CPU write, which races the draws that follow (the next epoch's first pass
> uploads it), or through a change of the registered buffers or GPU mappings, which moves the BDA
> structure epoch. That epoch now advances on every buffer registration and mapping change also
> without incremental synchronization (it used to stay constant there).
>
> - KYTY_SYNC_EPOCH=0 or KYTY_BDA_SYNC_EPOCH=0 restores a pass per draw.
> - KYTY_BDA_SYNC_EPOCH_VERIFY=1|exit (with KYTY_BDA_INCREMENTAL_SYNC=1, as the product runs): each
>   skipped pass scans anyway; a normal page found CPU-dirty while the fault epoch is still the one
>   taken before the last pass is a page that pass should have uploaded (BdaSyncEpochVerify-
>   Mismatches); hot pages, written without faults, are races the skip assumes.
> - Counters SyncEpochAdvances, BdaSyncEpoch{Skips,VerifyChecks,VerifyMismatches}; test totals
>   BufferCache::m_bda_epoch_totals.
>
> Visual checks: grass near Astro and the pool (per-frame data read through BDA), the desert slide.
>
> Tests: which packets advance the epoch (every synchronization and memory-writing packet and custom
> operation; not register writes, register loads, direct draws or indirect calls); a BDA pass per
> epoch and per structure change, a CPU write uploaded by the pass after the epoch advanced, and in
> verify mode counted as a race. ctest bda_sync_epoch{,_incremental,_verify,_off}.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/drawPrep/packetClass.h`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/host_gpu/syncEpoch.h`, `tests/DrawPrepTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 228. Buffer bindings: reuse a read binding within a sync epoch (KYTY_BINDING_EPOCH_MEMO)

Commit: [`abcd50f2`](https://github.com/Jetsku/KytyPS5-experimental/commit/abcd50f222a8267f98a867763aa2b145209b439c) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The draw loop binds the same guest ranges many times per sync epoch: vertex and pixel programs
> share constant blocks, draws share per-view constants and index and vertex buffers. In the U50
> start-view check run the range memo reused 13.7k stream decisions and 30k clean
> synchronizations per flip, yet every binding still paid the dirty queries, the page-table lookup
> and the synchronization call, and a CPU-dirty small range was copied into the stream ring again,
> at a new offset, so its push descriptor could not match the previous draw's either.
>
> A read binding (ObtainBuffer: not written, not a texel read; GPU thread) of a range reuses the
> previous result for exactly that range while
> - the sync epoch is the same: a guest CPU write since then races the draws;
> - the range's MemoryTracker::RangeSignature is unchanged: no tracker transition in its regions
>   (uploads, GPU-dirty marks, readbacks, hot-page changes, write faults);
> - for a cache-buffer result, the buffer structure is unchanged (every Register/Unregister moves
>   it). It is recorded with the signature taken after its synchronization, and for a small read
>   only when the tracker bits then do not make the next one a stream copy;
> - for a stream copy, the signature did not move during the decision and the copy, and the
>   stream tick is the same (the ring never overwrites an allocation during its tick).
> Everything ordered within an epoch is a GPU-thread tracker transition or buffer registration; the
> emulator's own writes of guest bytes happen in fence packets, which start a new epoch first, or
> complete asynchronously like GPU writes.
>
> - KYTY_BINDING_EPOCH_MEMO=0 (or KYTY_SYNC_EPOCH=0) reverts.
> - KYTY_BINDING_EPOCH_MEMO_VERIFY=1|exit: every hit also runs the normal path and returns its
>   result. A different decision, buffer or offset with no tracker transition racing the check is
>   a mismatch; changed stream bytes, or pages the normal path uploads for a cache-buffer hit, are
>   races.
> - Counters BindingEpochMemo{StreamHits,CachedHits,Records,VerifyChecks,VerifyMismatches,
>   VerifyRaces}.
>
> Visual checks: Sky Garden stream see-through, the pool, distant trees in the fly-in, grass near
> Astro.
>
> Tests: ctest binding_epoch_memo{,_verify,_off,_no_sync_epoch}: stream-copy reuse; a guest write
> within the epoch (the copy is kept; verify mode copies anew and counts a race); the next epoch
> and the next tick copy again; a GPU write in between binds the written buffer; cache-buffer
> reuse; a write fault and a buffer join end the reuse; the joined bytes. The verify mode also runs
> in draw_prep_engine_parallel_inplace_verify.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 229. Merge claude/cpu-opt: range-memo race classification, relaxed tracker queries, sync epoch with one BDA pass per epoch, binding epoch memo

Commit: [`fccce957`](https://github.com/Jetsku/KytyPS5-experimental/commit/fccce957da6971141dc2c9753ee7942b09a2b210) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

### 230. Split mesh draws whose instance count exceeds one host dispatch

Commit: [`b1647304`](https://github.com/Jetsku/KytyPS5-experimental/commit/b1647304dacc8a69a658b36bc02865e952422adb) · **Geometry and indirect execution**

Preserves the named geometry semantics or keeps eligible indirect arguments on the GPU. Correctness is distinct from avoiding a CPU readback; it does not automatically reduce scene complexity.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A Sky Garden draw with one mesh workgroup and 75,198 instances hit the
> fatal "mesh draw exceeds host workgroup limits" check: instances go in
> dispatch Y, and the RTX 3090 allows 65,535 there. The shader's instance
> index is the pushed first instance plus WorkgroupId.y, so the draw is now
> recorded as instance ranges, each pushing its own first instance. Only a
> segment whose single instance exceeds the X or total limit stays fatal.

</details>

Changed files: `src/graphics/host_gpu/renderer/renderDraw.cpp`.

### 231. GI probes: pixel DS_APPEND without helper lanes; GI hang diagnostics

Commit: [`b74acf1b`](https://github.com/Jetsku/KytyPS5-experimental/commit/b74acf1be61ef4b7ac6d39a5248db214e533594f) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot's GI ray-bundle pass (the pass the game patch "Disable GI probes"
> turns off) builds per-pixel fragment linked lists with a pixel-shader
> DS_APPEND on a GDS counter. Kyty's pixel EXEC includes helper invocations, so
> the append could elect a helper lane. Vulkan discards a helper's atomic, the
> subgroup got an undefined base, duplicate node indices made the lists cyclic,
> and GfxGiProbeTraceLinkedList (0x657ad04626bf9d55) walked them forever (the
> device loss of RT data run 1). Every switch below is off by default.
>
> - KYTY_PS_APPEND_LIVE_ELECTION=1: a pixel shader's DS_APPEND/DS_CONSUME elects
>   a non-helper lane; the count stays popcount(EXEC).
> - KYTY_PS_LIVE_EXEC=1|all: pixel shaders with DS_APPEND/DS_CONSUME (1) or all
>   pixel shaders (all) start with EXEC = the non-helper invocations, like the
>   PS5's pixel valid mask; S_WQM_B64 adds the helpers back. An append then
>   counts and indexes exactly the covered pixels (new IR op IsHelperInvocation).
> - KYTY_LOOP_GUARD=<n> with KYTY_LOOP_GUARD_SHADERS=<hash>[,...]: diagnostic
>   loop budget for the listed shaders. Exhausted invocations leave their loops
>   and are counted in the last GDS dword, reported at flips ("Loop guard: N").
> - KYTY_GDS_TRACE=1 logs the user SGPRs of programs that bind GDS; hang-trace
>   CP rows "gds-dma" record GDS fills and copies.
> - Tests: --gi-probe-codegen-only and --gi-probe-only (ctest gi_probe_codegen
>   and gi_probe) cover the helper election, exact append counts and S_WQM with
>   a helper-free EXEC on the GPU, and the loop guard. --gi-decode-inventory
>   decodes a whole guest shader past BVH instructions.

</details>

Changed files: `CMakeLists.txt`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterHelpers.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterModule.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterProgram.cpp`, `src/graphics/shader/recompiler/frontend/translate/Translate.cpp`, `src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.inc`, `src/graphics/shader/recompiler/ir/passes/BindingLayout.cpp`, `tests/ShaderGiProbeTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 232. Merge claude/rt-gi-probes: pixel DS_APPEND without helper lanes, GI hang diagnostics

Commit: [`eebf6a1a`](https://github.com/Jetsku/KytyPS5-experimental/commit/eebf6a1ac6c89db9bfc6483099628a666f97bb77) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> All switches default off: KYTY_PS_LIVE_EXEC (pixel shaders with DS_APPEND or
> DS_CONSUME start with EXEC holding only non-helper invocations; S_WQM adds
> the helpers back), KYTY_PS_APPEND_LIVE_ELECTION, KYTY_LOOP_GUARD(_SHADERS),
> KYTY_GDS_TRACE, and hang-trace gds-dma rows.

</details>

### 233. Shaders: KYTY_SRT_VARIANT_READS plans loop-variant scalar reads as runtime reads

Commit: [`938c6cae`](https://github.com/Jetsku/KytyPS5-experimental/commit/938c6cae8b7533e5e9d12b4cdae78990de99a889) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot's default (tiled) deferred-lighting kernels walk the BVH: they
> S_LOAD an instance record through a loop-carried pointer (pc 0x2030) and
> S_BUFFER_LOAD through the V# read from that record (pc 0x2150).
> BuildSrtPlan gives such a read a flat SRT slot. A flat slot is evaluated
> once before the dispatch, when that address does not exist yet, so every
> dispatch of the shader is dropped (the SRT-plan limitation noted in
> PR #558). Offline, all 26 RT permutations of the tiled kernels failed to
> materialize.
>
> With KYTY_SRT_VARIANT_READS=1:
> - BuildSrtPlan plans a read whose address (its GetAddressResource or V#
>   handle) is not a valid runtime value as a runtime read, by the same
>   ValidateRuntimeValue rules as every other runtime value. Reads whose
>   address can be evaluated before the dispatch keep their flat slots.
> - TrackResources turns an S_BUFFER_LOAD through such a V# into a BDA read
>   (ResourceKind::IndirectBuffer) with RDNA2 ISA 7.2.1 semantics:
>   addr = (base + OFFSET + SOFFSET) & ~3; the bound is that of a bound
>   scalar buffer (stride 0: num_records bytes, else stride * num_records),
>   and a dword with any byte past it reads 0.
> - Any other descriptor the shader computes at runtime (image, sampler,
>   single-dword vector load) drops the program with one log line per
>   shader instead of exiting. Without the switch its dispatches are
>   dropped as well.
> - ExtractResourcePlan skips those BDA reads (no descriptor source).
>
> Off by default; nothing changes without the switch.
>
> Offline (engine shaders extracted from the eboot, shader_cfg_tests plan
> harness): with the switch 26/26 RT permutations materialize; across all
> 197 engine compute shaders only the 27 RT kernels change.
>
> Tests: resource_tracking (loop-variant record reads, loop-variant image
> dropped, loop-invariant reads stay flat) and the srt_variant_reads GPU
> test (ScalarBufferLoadsGpuSelectedDescriptors,
> ScalarLoadsThroughLoopCarriedRecords).

</details>

Changed files: `CMakeLists.txt`, `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/ShaderRecompiler.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp`, `src/graphics/shader/recompiler/ir/Program.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.cpp`, `src/graphics/shader/recompiler/ir/passes/ResourceTracking.cpp`, `src/graphics/shader/recompiler/ir/passes/ResourceTracking.h`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.cpp`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.h`, `tests/ResourceTrackingTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`, `tests/ShaderSrtVariantTests.inc`.

### 234. Merge claude/rt-tiled-lighting: KYTY_SRT_VARIANT_READS

Commit: [`e01ad1c1`](https://github.com/Jetsku/KytyPS5-experimental/commit/e01ad1c1a28b38b06888139d89d020b21228e426) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Default off. With it, scalar reads whose address depends on a loop-variant
> value (the RT kernels' TLAS instance records) become runtime reads, and
> S_BUFFER_LOADs through the V# in such a record go through BDA with RDNA2
> bounds. Other runtime descriptors drop their program with a log line
> instead of exiting.

</details>

### 235. Detectors for guest-memory changes resource tracking does not see (tracker gaps A and B)

Commit: [`419e6cb3`](https://github.com/Jetsku/KytyPS5-experimental/commit/419e6cb3a1c23960d0781ae9570aeac0bb2273f9) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Two paths change guest memory, or its host protection, without telling resource tracking. These
> detectors only count and log (first 16 of each on stderr); nothing changes behaviour. They decide
> which of the two exact fixes is needed.
>
> A. Emulator writes of guest backing bytes outside a publication: LOD-statistics reports (at
>    record time, at completion and the rewrite) and occlusion results write through the backing
>    alias (TryWriteBacking), which no page protection sees. RenderContext::NoteHostBackingWrite,
>    called right before each such write, classifies its tracker pages from the lock-free mirrors
>    (BufferCache::CountPageStates):
>    - GPU-dirty: a later readback of the page copies the GPU's older bytes over the write;
>    - clean (tracked, neither CPU- nor GPU-dirty): a GPU copy of the page keeps the old bytes
>      until the page is dirtied again.
>    Counters HostBackingWrites, HostBackingWriteGpuDirtyPages, HostBackingWriteCleanPages.
>
> B. Guest protection changes applied with a direct host protect instead of through the
>    PageManager (KernelMprotect, ProtectGuestMemory, SetProgramMemoryProtection): for GPU-mapped
>    memory, RenderContext::NoteGuestProtection counts the pages resource tracking watches in the
>    range (PageManager::CountWatchedPages), those whose tracking protection the new mode replaces
>    (a write-watched page made writable, an access-watched page made accessible: their faults
>    stop), and changes that restrict access (the next tracking transition of those pages sets the
>    tracking protection, not the guest's). Counters GuestProtectCalls, GuestProtectWatchedPages,
>    GuestProtectOverriddenPages, GuestProtectRestrictsGpuMemory.
>
> Cost: a few relaxed loads per LOD report or occlusion dump (about 20 per flip), and one mapped-
> range lookup plus a PageManager scan per protection change (rare).
>
> Tests: page_manager_tests TestCountWatchedPages (write- and access-watched pages, sub-page
> ranges, an untracked region, unwatching); ctest tracker_gap_detectors (the page states a host
> write is classified by: before tracking, clean after an upload, GPU-dirty after a GPU write,
> neither after a CPU write fault; both detectors run).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/pageManager.cpp`, `src/graphics/host_gpu/pageManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/host_gpu/renderer/renderContext.h`, `src/kernel/memory.cpp`, `tests/PageManagerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 236. tests: EagerReadback holds the completion runner while it checks two pending eager copies

Commit: [`bf0e7996`](https://github.com/Jetsku/KytyPS5-experimental/commit/bf0e79961fd82e0585593e046a5ab8311aa6dc03) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Step 5 checked that the eager copies of both pages were pending right after the submission that
> issued them, but the completion runner can publish one first (one failure in six runs on U52).
> A priority operation queued before that submission holds the runner, which runs them in order,
> until the check has been made.

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 237. WIP (unbuilt): publish only the dirty-mirror words of the pages a transition changes

Commit: [`b9b6654a`](https://github.com/Jetsku/KytyPS5-experimental/commit/b9b6654aedfa51991c6365aa0b323c448d4850e3) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

Changed files: `src/graphics/host_gpu/regionManager.h`, `tests/MemoryTrackerTests.cpp`.

### 238. WIP (unbuilt): ClampRangeSize answers from a per-thread cache of committed runs (KYTY_CLAMP_RANGE_MEMO)

Commit: [`71d252bf`](https://github.com/Jetsku/KytyPS5-experimental/commit/71d252bfff6965579ec6ba70b1407d5702a50183) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/kernel/memory.cpp`, `src/kernel/memory.h`, `tests/VirtualMemoryAllocationTests.cpp`.

### 239. tests: repair the dirty-mirror and clamp-memo tests

Commit: [`03e29365`](https://github.com/Jetsku/KytyPS5-experimental/commit/03e29365918a683d07abb7a6e8a6f91d9684558a) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> MemoryTrackerTests: the mirror check's fprintf format string held a raw
> line break instead of \n, so memory_tracker_tests did not compile.
>
> VirtualMemoryAllocationTests: ClampRangeSize exits on an address outside
> every committed range, so the clamp-memo test no longer looks up after
> unmapping both pages. It maps the left page again and expects the run to
> be that page alone (0x100), not the two pages from before the unmap.
>
> memory_tracker, virtual_memory_allocation, _clamp_memo_verify and
> _clamp_memo_off pass.

</details>

Changed files: `tests/MemoryTrackerTests.cpp`, `tests/VirtualMemoryAllocationTests.cpp`.

### 240. Draw sequence: repeat unchanged target lookups and stage texture sets (KYTY_DRAW_SEQUENCE_FAST)

Commit: [`c490b6fd`](https://github.com/Jetsku/KytyPS5-experimental/commit/c490b6fd423059547e7b918e83ee90eff0be009a) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A draw that consumes the same state as the draws before it no longer redoes two lookups whose
> answer is proven unchanged. Both repeats are exact: every input the full path reads is either
> unchanged by construction or checked, and only the full path's effects are performed.
>
> Targets (TextureCache::RepeatLookup). A target slot whose registers did not change
> (KYTY_TARGET_DESC_MEMO hit) looks up the same description. FindImage now records, on request,
> why that lookup would repeat: a first-page answer (the page's structure version), no residency
> extension (resident_first), no possible alias owner (or the image returns at once), and metadata
> decisions that were provable no-ops. MaterializeDccClear and MaterializeCmaskClear report such a
> no-op: decided by the description alone, by images over the metadata bytes (page versions), or
> by a recorded fill of GPU-owned bytes that is not a clear (the new BufferCache known-fill
> generation, and the GPU-dirty state). Decisions that read guest bytes, inspect on the GPU or
> clear are never repeated. TryRepeatLookup checks all of it (one texture-cache lock) and performs
> FindImage's bookkeeping (tick, touch; metadata assignment for DCC).
>
> Textures. A stage whose program and T# words are those its bindings were resolved from repeats
> TryResolve for every binding under one lock (TextureBindingMemo::TryRepeatResolve: the same
> entry tags, TryResolve's hit conditions without revalidation), and RebindImages repeats
> TryAcquireView for every binding (TryRepeatViews). Touches, BindImage and usage flags end in the
> full path's order. Programs live in the program cache for its lifetime (append-only
> permutations), so a program pointer names one program.
>
> Switches: KYTY_DRAW_SEQUENCE_FAST (default on; 0 off; or "targets", "textures").
> KYTY_DRAW_SEQUENCE_VERIFY=1|exit runs the full path after every repeat and compares (image,
> side effects, record validity, entry tags, views). A target difference counts as a race when
> another thread forgot a recorded fill (BufferCache::ForeignKnownFillChanges) or ended GPU
> ownership of the metadata (TextureCache::RepeatGpuRangesLost) meanwhile. Counters:
> DrawSequenceTarget{Repeats,Misses,Records}, DrawSequenceTexture{Repeats,Misses},
> DrawSequenceViewRepeats, DrawSequenceVerify{Checks,Mismatches,Races}. Repeated lookups no longer
> count DccKnownFillClears, CmaskFastClearExpanded/Aliased/Shape/Format.
>
> Tests: CheckDrawSequence (--draw-sequence-only): CMASK decisions from guest bytes are not
> repeatable; a recorded fill is; a new fill, an image over the CMASK bytes and its removal refuse
> the next repeat; a stage repeats its bindings and views, not after other T# words nor while a
> binding's page changed owners since it was resolved, and a guest write refreshes the texture
> instead of repeating its view. ctest draw_sequence, _off and _verify (the whole compute suite with
> KYTY_DRAW_SEQUENCE_VERIFY=exit); draw_prep_engine_parallel_inplace_verify also runs the verify
> mode. Full suite 83/83 (shader_cfg and kernel_file_system excluded, known failures).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/colorRenderTarget.cpp`, `src/graphics/host_gpu/renderer/colorRenderTarget.h`, `src/graphics/host_gpu/renderer/depthRenderTarget.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.h`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.cpp`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.h`, `src/graphics/host_gpu/renderer/render.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 241. WIP (unbuilt): profiler instrumentation off the hot path (per-thread counters, inlined gates)

Commit: [`19c04dfd`](https://github.com/Jetsku/KytyPS5-experimental/commit/19c04dfd83f81e688eb7c94fd597d96e764298ba) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DEEP-TRACE-U52 item 2: instrumentation cost the command processor 4.6 ms/flip with Tracy
> disconnected and 6.1-7.2 ms/flip connected (CountFrameEvent 1.6/4.2, ScopedBlock 1.35,
> GpuOpProfiler site scopes and hooks 1.1, HangTrace 1.0).
>
> - Profiler switches (FramesOnlyEnabled, AggregateEnabled, DetailedEnabled) are inline loads of
>   lazily initialized flags instead of out-of-line calls through function-local statics.
> - CountFrameEvent, ScopedFrameWait and AddFrameWait check one inlined sink flag. PublishFrameWork
>   refreshes it once per guest flip: Off (aggregates off or no profiler connected), Thread (the
>   calling thread's counter block: a relaxed load and store, no locked instruction and no cache
>   line shared with other writers), or Shared. The flip publisher sums every thread's block. Blocks
>   are never freed: a released block (thread exit) is reused by the next new thread and continues
>   its totals, so published values stay cumulative. CountFrameWork counts per thread too.
> - KYTY_PROFILE_COUNTERS=shared restores the previous counting (every call checks the switches and
>   the connection and adds to shared atomics), for comparisons.
> - ScopedBlock's constructor and destructor are inline; zones are decided once (Initialize).
> - GpuOpProfiler::ScopedSite enters and leaves its site inline; Detail::EnterSite/LeaveSite stay
>   for callers that set a thread's site themselves (the P2 recorder).
> - HangTrace::Enabled and CpWatch are inline (the watch window is set once by Initialize).
>
> Tests: profiler_counter_tests (ctest profiler_counters): totals across threads, block reuse
> after thread exit, and a printed timing of the counting paths.

</details>

Changed files: `CMakeLists.txt`, `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/gpuOpProfiler.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.h`, `tests/ProfilerCounterTests.cpp`.

### 242. Draw prep: name the reads refused as unclean (DrawPrepFallbackUnclean diagnostics)

Commit: [`ad57b3e1`](https://github.com/Jetsku/KytyPS5-experimental/commit/ad57b3e10a2b94486733cd283538b5d69652e947) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DrawPrepFallbackUnclean persists in clean U52 captures (49/flip fly-in, 18/flip start view, 3.7 in
> U51) and equals VertexMetadataProbeMisses. This records why and where each refused read was:
>
> - FrameEvent.DrawPrepUnclean{HintGpuDirty,HintPublication,Exact,Backing}: the refusing gate (a
>   worker's lock-free GPU-dirty hint or pending backing publication, the GPU thread's exact clean
>   predicate, an exact read without a backing).
> - With the hang trace, unclean.csv aggregates the refusals per second by reason, read purpose,
>   calling host code (exe+RVA, resolve with the build's PDB) and 4 KiB page, with the page's last
>   recorded GPU writer (kind, age, size). Exact refusals are classified by predicate
>   (buffer-gpu-dirty, publication, image-gpu-modified, thread, verdict).
> - DrawPrep::ScopedReadPurpose names reads; the vertex metadata probes are tagged
>   "vertex-attributes" and "vertex-buffers".
>
> Nothing changes what is refused; the classification runs only on refusals.
>
> Test: CheckDrawPrepCertifiedShaderHash counts its GPU-owned code read once as an exact refusal.

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/drawPrep/readSet.h`, `src/graphics/shader/shader.cpp`, `src/kernel/memory.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 243. tests: a failed check always fails its test; EagerReadback holds the runner at every pending check

Commit: [`cfefb9c5`](https://github.com/Jetsku/KytyPS5-experimental/commit/cfefb9c54fcfbefcb9f47a99acf4f7cb4c106a2d) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - ctest: every test fails when its output holds a failure message ("<Suite>: failed: ...",
>   "<Suite>: <case> failed ...", "FAILED: ...", "... test failed: ...", "SaveDataMemoryTests:
>   <line>: ..."), not only on a failure exit status. A harness can lose the status, and a process
>   exit can overtake a check failing on another thread. failure_message_fails_test checks the
>   expression itself (a program printing a failure and exiting 0 fails; inverted by WILL_FAIL).
> - ShaderRecompilerComputeTests Fail(): flushes both streams and restores the default SIGABRT
>   action before abort(), so no handler a loaded library installed can end the process with 0.
>   (A forced check failure exits 0xC0000409.)
> - ImeDialogTests CHECK prints what failed before aborting.
> - EagerReadback: steps 2-4 checked "the eager copy is still pending" while the completion runner
>   could already have published it (the "second eager issue" flake). They now hold the runner as
>   step 5 does (bf0e7996), through shared hold_runner/release_runner helpers. 20 sequential and
>   15 concurrent (3 at a time) runs passed.

</details>

Changed files: `CMakeLists.txt`, `tests/ImeDialogTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 244. Draw sequence: repeat a stage's earlier texture sets too (per-stage history)

Commit: [`aeb7fa7b`](https://github.com/Jetsku/KytyPS5-experimental/commit/aeb7fa7bd1b296822fd41e49a5b1d15fa0125c40) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The desert mesh stamps alternate their pixel stage between about three T# sets per frame
> (U53 repeat trace: the set changes on 47-65% of consecutive stamps), so the stage texture repeat
> of c490b6fd, which compares with the last set only, hit about 45% of them.
>
> PreparedBindings keeps the stage's three most recent other sets (program, T# words, resolved
> bindings). A stage whose words match one of them swaps it in as the current set (the current set
> takes its place; vectors are swapped, never copied) and validates it exactly as the current set
> (TextureBindingMemo::TryRepeatResolve memo tags, TryRepeatViews); a stale set misses and is
> resolved again in place. New words rotate the current set into the history and resolve into the
> oldest set's vectors. KYTY_DRAW_SEQUENCE_FAST=0 resolves as before; KYTY_DRAW_SEQUENCE_VERIFY
> checks history repeats like the others.
>
> Counter: DrawSequenceTextureHistoryHits. Test: CheckDrawSequence alternates two T# sets and
> repeats each from the history. Full suite 85/85 (shader_cfg and kernel_file_system excluded).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.h`, `src/graphics/host_gpu/renderer/render.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 245. Merge claude/cpu-opt: draw-sequence reuse, per-thread profiler counters, dirty-mirror and clamp memos

Commit: [`569bacf5`](https://github.com/Jetsku/KytyPS5-experimental/commit/569bacf53d6d0c768c61c4fee3fab5b57455e10f) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - Draw sequence (KYTY_DRAW_SEQUENCE_FAST, _VERIFY): repeat proven-unchanged
>   target lookups and stage texture sets; per-stage texture-set history.
> - Profiler: per-thread counter blocks summed at each flip, inlined gates
>   (KYTY_PROFILE_COUNTERS=shared restores the shared atomics for A/B).
> - Dirty mirrors republish only the words a transition changes;
>   ClampRangeSize answers from a per-thread cache (KYTY_CLAMP_RANGE_MEMO).
> - Tracker-gap detectors A and B; DrawPrepFallbackUnclean diagnostics.
> - Tests: a printed failure always fails its test; EagerReadback holds the
>   completion runner at every pending check. The WIP (unbuilt) titles of
>   b9b6654a, 71d252bf and 19c04dfd are stale: all were built and pass the
>   full suite (85/85 at aeb7fa7b).

</details>

### 246. Kernel event queues: coalesce repeated triggers like kqueue (KYTY_EQUEUE_COALESCE)

Commit: [`f88b5e64`](https://github.com/Jetsku/KytyPS5-experimental/commit/f88b5e645d97ae611ba22bfc6eb45f0452e5dcf5) · **Guest services and synchronization**

Implements guest-visible state/waits or reduces redundant host wakeups/polling. These changes can improve progress and contention but have no isolated per-commit FPS result.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every end-of-pipe interrupt the guest had not consumed was appended to an
> unbounded std::deque on its event (sync.cpp InterruptEventTriggerFunc; the
> same pattern in video-out and AMPR events). Growth of that deque's map caused
> the 135 ms hitch in the U52 Sky Garden trace: the priority thread spent 77 ms
> in deque::_Growmap while the CP waited 72.5 ms (DEEP-TRACE-U52 section 5).
> The leak grows with play time, and so does each hitch.
>
> What the guest relies on (Astro Bot eboot):
> - It registers 8 graphics equeues: 5 with interrupt id 0, and ids 0x40,
>   0x46 and 0x48 (0x4a3c22..0x4a3f20).
> - It waits on them only in 0x4a5eb0 and 0x4a65f0. Each first drains up to
>   8 events with a zero timeout, then waits with a 1 us timeout until its
>   label reaches the target.
> - The event contents are never read. sceAgcDriverGetEqEventType/ContextId,
>   sceKernelGetEvent* and sceVideoOutGetEvent* are not imported.
> So only the pending/woken state matters, and a stale backlog only turned
> those waits into spins.
>
> New default semantics, those of a FreeBSD kqueue knote:
> - One pending state per registered event. A repeated trigger updates it in
>   place: fflags counts graphics interrupts since the last delivery, the
>   video-out counter counts occurrences (saturating at 15), and data holds
>   the newest value.
> - Each pending event is reported at most once per wait, in activation order.
> - EV_ONESHOT events are removed after delivery. EV_CLEAR events (or those
>   with a filter reset) are cleared. A level-triggered event stays pending
>   and moves behind the others (kqueue_scan's marker).
> The filters' next-state rules are pure functions in eventQueueFilters.h,
> shared by the filters and the tests.
>
> Switch: KYTY_EQUEUE_COALESCE=0 restores the old per-trigger queue and delivery
> loop exactly. KYTY_EQUEUE_COALESCE=verify also prints every event whose
> merged-trigger count reaches a new power of two: at delivery with its queue,
> and while unconsumed from 1024 merged triggers on, which names the ids that
> used to accumulate. New counters: FrameEvent.EqueueCoalescedTriggers and
> EqueueCoalescedDataChanges.
>
> Tests:
> - event_queue_semantics_tests (new) covers coalesced counting and newest
>   data, the exact legacy mode, and an exact count under concurrency (1M
>   interrupts, sum of delivered fflags == 1M, in ~750-1400 deliveries).
> - It also covers EV_CLEAR and level user events, EV_ONESHOT timers,
>   activation order and level re-queue, the video-out counter and verify
>   stats.
> - `--hitch-bench N` with 16M unconsumed triggers: legacy worst trigger
>   12.5 ms (map growth), 5 triggers >= 1 ms, 0.42 s to free the queue.
>   Coalescing: worst 0.07 ms, none >= 1 ms, nothing to free.
> - event_queue_lifetime_tests' duplicate-add case now runs in both modes.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/sync.cpp`, `src/graphics/presentation/videoOut.cpp`, `src/kernel/eventQueue.cpp`, `src/kernel/eventQueue.h`, `src/kernel/eventQueueFilters.h`, `tests/EventQueueLifetimeTests.cpp`, `tests/EventQueueSemanticsTests.cpp`.

### 247. Raise service-thread priority; cheaper guest sched_yield, usleep and signal polls

Commit: [`55da0837`](https://github.com/Jetsku/KytyPS5-experimental/commit/55da0837979835a34abb8c424e405a33e140a04f) · **Guest services and synchronization**

Implements guest-visible state/waits or reduces redundant host wakeups/polling. These changes can improve progress and contention but have no isolated per-commit FPS result.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Service threads (DEEP-TRACE-U52 item 7):
> - Evidence: all three 87 ms Sky Garden frames came from threads waiting for
>   a CPU. The present thread was ready 28 ms and the submission thread 31 ms
>   while 16 equal-priority threads held the 16 allowed CPUs. The untraced
>   capture shows the same 73-83 ms frames every 3-7 s.
> - New Common::RaiseServiceThreadPriority(), switch KYTY_SERVICE_PRIORITY:
>   0 = unchanged, 1 = above normal, 2 = highest (default).
> - Applied at thread start of the VideoOut present thread, the
>   queue-submission worker and every CommandScheduler priority (completion)
>   thread. The CP keeps KYTY_CP_PRIORITY (above normal).
> - None of them spins. The present thread blocks between vblanks except for
>   its last <= 50 us SleepMicro spin. The submission worker waits on a
>   condition variable. The priority thread waits on its queue, the
>   dispatched-tick WaitOnAddress and vkWaitSemaphores; its callbacks only
>   queue work.
> - A raised present thread would still have waited synchronously for the
>   window thread (Presenter::Present -> UpdateTitle -> SDL_RunOnMainThread
>   wait=true), which stays at guest priority. The title is now published to
>   one slot with at most one queued main-thread callback (bounded, looked up
>   by window id). KYTY_TITLE_UPDATE=sync restores the synchronous call.
>
> Guest yield/usleep poll (DEEP-TRACE-U52 item 4):
> - The "usleep(1) loop" at eboot 0x114b2f0 is `while (fence.done !=
>   fence.target) { scePthreadYield(); sceKernelUsleep(0); }` (0x13b0 ->
>   T72hz6ffq08, 0x1370 passes *&0). The U52 hang trace counts 221.6M yields
>   and 218.1M usleeps.
> - On the PS5 (FreeBSD) this loop is a spin by design: sched_yield
>   (sched_relinquish) returns at once when nothing else is runnable on the
>   CPU, and kern_nanosleep returns 0 at once for a zero time. So a faithful
>   emulation must not sleep there; each iteration is only made cheaper.
> - sched_yield: SwitchToThread only. The legacy path added Sleep(0) after an
>   empty SwitchToThread, a second system call that was ~15 of the thread's
>   48 ms/flip.
> - KernelDispatchPendingSignalForCurrentThread: one load when no signal is
>   pending, instead of 64 locked fetch_and (13.3 ms/flip in the trace).
>   Pending signals are taken lowest-first as before (kernel/pendingSignals.h).
> - KernelUsleep: the unused Common::Timer (QPC) is gone, and usleep(0)
>   returns at once as before.
> - usleep(1): FreeBSD blocks for at least the requested time. It now yields
>   once, then pauses until 1 us has passed. Before, it only yielded and could
>   return early.
> - KYTY_GUEST_SCHED=legacy restores all of the old calls.
>
> Tests (thread_service_tests, new):
> - Service level 2 gives THREAD_PRIORITY_HIGHEST; the CP stays above normal.
> - sched_yield returns in ~170 ns when alone and hands the CPU to a ready
>   thread pinned to the same CPU.
> - The 1 us sleep never returns early (mean 1.00 us).
> - Pending signals are taken lowest-first, bits at or above the limit are
>   kept, and concurrent posts are neither lost nor duplicated.
> - An empty poll costs ~0.3 ns against 105-113 ns for the legacy scan.

</details>

Changed files: `CMakeLists.txt`, `src/common/threads.cpp`, `src/common/threads.h`, `src/graphics/host_gpu/queueSubmission.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/presentation/videoOut.cpp`, `src/graphics/presentation/window/window.cpp`, `src/kernel/pendingSignals.h`, `src/kernel/pthread.cpp`, `src/kernel/pthread.h`, `src/libs/libKernel.cpp`, `tests/ThreadServiceTests.cpp`.

### 248. DrawPrep: keep two workers hot, park the rest until the backlog needs them

Commit: [`3019e3c6`](https://github.com/Jetsku/KytyPS5-experimental/commit/3019e3c68f5a1a70ed966c25afd080274ac47849) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The six DrawPrep workers spun 200 us after every slot, and at the Sky Garden
> rate a slot arrives about every 70 us. So they never parked while the CP
> streamed draws: 210 ms/flip of spin (3.7 cores) for about 1.15 cores of
> Prepare (DEEP-TRACE-U52 sections 3.4 and 6.4).
>
> WorkerGate (drawPrep/workerGate.h):
> - Hot workers, the first KYTY_DRAW_PREP_HOT (default 2), keep the old
>   behaviour. They spin KYTY_DRAW_PREP_SPIN_US after their last slot, then
>   park, and the next publish wakes them.
> - Cold workers park as soon as nothing is claimable. They are woken one at a
>   time when KYTY_DRAW_PREP_WAKE_BACKLOG (default 8) published slots wait
>   unclaimed, and spin KYTY_DRAW_PREP_COLD_SPIN_US (default 50) before
>   parking again.
> - The cold wake is requested by whichever thread sees the backlog: a worker
>   right after its claim, or the producer on every 8th publish. At most one
>   cold wake is in flight, so the CP never pays a syscall per draw.
> - The park protocol is the old one: a seq_cst sleepers count, then a
>   predicate reload before waiting.
> - KYTY_DRAW_PREP_HOT >= KYTY_DRAW_PREP_WORKERS keeps every worker hot. That
>   is the old behaviour; the producer then never reads the claim counter.
> - The worker loop itself moved into workerGate.h (RunPreparationWorker), so
>   the unit tests run the production loop. Window gained Unclaimed().
> - New counter: FrameEvent.DrawPrepColdWakes.
>
> Measured (draw_prep_tests --measure-worker-gate, Sky Garden shape: ~11 us of
> Prepare per draw, a draw every ~10 us, a drain every 60 draws, 20k draws,
> 4 runs):
> - All six workers hot: 1.19-1.22 s of worker CPU per ~0.20 s wall, for
>   0.22 s of work (~5.9 cores busy).
> - hot=2 (default): 0.39-0.41 s (~1.9 cores). Same wall time, no extra
>   self-prepares, 0-16 commit waits per 20k draws.
> - hot=1: 0.22-0.41 s, but the producer sometimes waited on 600-1600 heads.
>   So 2 is the default.
>
> Tests (draw_prep_tests):
> - The gate's shape and clamping.
> - The producer reads the backlog only every WakeBacklog-th publish, and
>   never when every worker is hot.
> - A bursty stress with 1 and 2 hot workers (60k slots, idle gaps long
>   enough to park): every slot is prepared exactly once and in order,
>   nothing hangs, and cold workers are woken.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `src/graphics/host_gpu/renderer/drawPrep/window.h`, `src/graphics/host_gpu/renderer/drawPrep/workerGate.h`, `tests/DrawPrepTests.cpp`.

### 249. Merge claude/sched-contention: event-queue coalescing, service-thread priority, draw-prep worker parking

Commit: [`2b81c30f`](https://github.com/Jetsku/KytyPS5-experimental/commit/2b81c30fc1b1bcfb3f51ae8152b3c66f0b75b17b) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - Kernel event queues keep one pending state per registered event, as a
>   kqueue does: repeated triggers coalesce (KYTY_EQUEUE_COALESCE; =0 restores
>   the old queue, =verify names the ids that used to pile up). Fixes the
>   unbounded interrupt backlog behind the growing hitches.
> - The present thread, the submission worker and the command-scheduler
>   priority threads run at the highest priority (KYTY_SERVICE_PRIORITY); the
>   title update no longer waits on the window thread (KYTY_TITLE_UPDATE).
> - Guest yield, usleep and pending-signal polls are cheaper; usleep(1) lasts
>   at least 1 us (KYTY_GUEST_SCHED=legacy reverts).
> - Draw-prep workers: two stay hot, the rest park until the backlog needs
>   them (KYTY_DRAW_PREP_HOT, KYTY_DRAW_PREP_WAKE_BACKLOG,
>   KYTY_DRAW_PREP_COLD_SPIN_US).

</details>

### 250. tests: link CodegenOptions into resource_tracking_tests

Commit: [`af95961e`](https://github.com/Jetsku/KytyPS5-experimental/commit/af95961e90562da18d9394e6e6958fb11a37fdf2) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> BindingLayout.cpp calls LoopGuardApplies (KYTY_LOOP_GUARD, from the
> rt-gi-probes merge eebf6a1a), which CodegenOptions.cpp defines, so
> resource_tracking_tests no longer linked.

</details>

Changed files: `CMakeLists.txt`.

### 251. Draw state: copy only what a prepared draw uses; drop redundant per-draw resets (KYTY_RENDER_STATE_FAST)

Commit: [`60ba86a1`](https://github.com/Jetsku/KytyPS5-experimental/commit/60ba86a1e7ae562f28b85e2c57d299488dce4afd) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U52 WPR, CP thread: memcpy/memset under PrepareDrawRenderState 0.74 ms/flip (the prepared vertex
> input, about 10 KB per draw), DrawRenderState::Reset 0.29, std::swap of the stage preparations
> 0.27, the colour/depth target resolution's own zeroing and copies 0.40.
>
> Three parts, each exact by construction and each behind KYTY_RENDER_STATE_FAST (unset/1: all;
> 0: none; or a list of vertex, swap, reset):
> - vertex: ApplyPreparedDraw copies a prepared vertex input as its used array prefixes (and each
>   buffer's attribute prefix) plus the fields after the arrays, and resets what the draw state
>   used past the new counts. Entries past the counts are default in every vertex input involved
>   (preparations value-initialise and fill prefixes; the draw state starts value-initialised and
>   the copy keeps it so), so the draw state gets the bytes of a full copy. A count out of range
>   falls back to the full copy (counted).
> - swap: the stage preparations are swapped member by member (vector swaps) instead of through
>   three whole-structure moves. Structured bindings name every member, so a new member stops the
>   build until it is swapped.
> - reset: DrawRenderState::Reset no longer value-initialises the colour/depth target entries (and
>   RefreshShaders the programs/pixel interface on the prepared path). ResolveRenderColorTarget and
>   ResolveRenderDepthTarget assign every field of the entry they resolve, and value-initialise it
>   only on their paths without a target (no colour output, inactive or unbound depth; stencil
>   states without stencil tests). Debug dumps, which log entries of draws without targets, keep
>   the full reset.
>
> KYTY_RENDER_STATE_VERIFY=1|exit: the vertex copy is compared byte for byte with the source, the
> swap member by member with copies of both preparations, and each target entry resolved over the
> last draw's field by field (SameRenderColorInfo/SameRenderDepthInfo, which name every field) with
> a resolution into a value-initialised entry; the full result is used on a difference.
>
> Counters: RenderStateVertexPartialCopies, RenderStateVertexFullCopies, RenderStateMinimalResets,
> RenderStateMemberSwaps, RenderStateVerifyChecks, RenderStateVerifyMismatches.
>
> Tests: render_state and render_state_off (CheckRenderStateCopies: 4000 random prepared inputs
> through the partial copy, byte-compared; invalid counts; colour and depth entries resolved over
> garbage on every path equal value-initialised ones), draw_prep_engine_render_state_off, and
> KYTY_RENDER_STATE_VERIFY=exit added to draw_sequence_verify (the whole compute suite) and
> draw_prep_engine_parallel_inplace_verify. Full suite 88/88 (shader_cfg and kernel_file_system
> excluded as before). A typical input (6 attributes in 2 buffers), both copies cache-hot: full
> copy 92 ns, partial 27 ns; in the emulator the full copy also pulls the ~10 KB a preparing worker
> wrote from another core.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/colorRenderTarget.cpp`, `src/graphics/host_gpu/renderer/colorRenderTarget.h`, `src/graphics/host_gpu/renderer/depthRenderTarget.cpp`, `src/graphics/host_gpu/renderer/depthRenderTarget.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/host_gpu/renderer/renderDraw.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 252. Descriptor-set reuse: audit the potential reuse rate (KYTY_DESCRIPTOR_SET_REUSE_AUDIT)

Commit: [`09c5831a`](https://github.com/Jetsku/KytyPS5-experimental/commit/09c5831ad9e7abdf6a0cb690ed5273eee7593045) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DescriptorSetsReused is 0 in every capture while about 1,035 sets are written per flip at the
> Sky Garden start view (U54). The desert agent pointed at DescriptorSetReuse::Hash: its last two
> mixes (a buffer's offset, then its range) leave the slot bits (hash % 64) independent of the
> offset's bits 8 and up, and stream-buffer ranges (flattened SRT, shader data) are 256-aligned and
> usually written last. Sets that differ only in such an offset share a slot and evict each other.
> The unit test now shows it: 16 offsets 256 apart all land in one slot, and two alternating sets
> never hit.
>
> Before changing the cache, measure what it could reuse. KYTY_DESCRIPTOR_SET_REUSE_AUDIT=1 (default
> off) counts, per descriptor-set commit:
> - DescriptorSetAuditCommits: every commit;
> - DescriptorSetAuditRepeats: an earlier commit of the same command buffer had exactly these
>   contents (an XXH3 digest of everything Matches compares) - what an unbounded cache would reuse;
> - DescriptorSetAuditDigestSlotHits: a 64-slot direct-mapped cache indexed by that digest would
>   still hold the set - what fixing only the hash would reuse.
> Compare with DescriptorSetsReused (the current cache). No behaviour changes.
>
> Test: binding_path (TestDescriptorSetReuseAudit: the slot collision, the digest separating the
> sets, repeats and digest-slot hits over A, B, A, B, and the per-command-buffer reset). Full suite
> 88/88 (shader_cfg and kernel_file_system excluded).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptorSetReuse.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptorSetReuse.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/render.h`, `tests/BindingPathTests.cpp`.

### 253. Upload DMA: the DMA worker copies staged guest bytes, not the command processor (KYTY_UPLOAD_DMA_HOST_COPY)

Commit: [`5e3b3709`](https://github.com/Jetsku/KytyPS5-experimental/commit/5e3b3709efda2420c097efe2b8fc0c1943d76859) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U54 start view: the BDA synchronisation passes upload about 20 MB per flip, and the command
> processor spends 1.14 ms/flip (traced) in UploadCopies' memcpy of CPU-dirty guest pages into the
> host staging ring. 19.5 MB of it (95%) then goes through UploadDma (uploads of 64 KiB and more):
> a worker submits the staging -> VRAM transfer that the graphics copies wait for.
>
> For those uploads the worker now also performs the guest -> staging copy, just before it submits
> the transfer that reads the bytes. The command processor only reserves the staging space and
> queues the copies with the job:
> - The worker reads the guest bytes through the direct-memory backing alias (new
>   LibKernel::Memory::GuestBackingAlias): that view stays mapped and writable for the process
>   lifetime, so the copy never faults, whatever protection the tracker (a later writable binding
>   making the pages GPU-owned) or the guest gives the guest view meanwhile, and a later unmap
>   cannot make it fault either. Ranges without an alias (not direct memory, or spanning
>   mappings) are copied by the command processor as before, as are hot-page copies (their bytes
>   are a scratch snapshot) and every upload the DMA does not take (below 64 KiB, ring full).
> - Order and data are those of the command-processor copy: ForEachUploadRange has made the pages
>   clean and write-protected before the copies are queued, so a guest write after that point
>   faults and makes them CPU-dirty again for the next upload whether the worker reads the bytes
>   before or after it (a write racing the draws, as with the copy made right after the
>   protection). The submission containing the graphics copies waits for the transfer
>   (PendingValue), which the worker submits after the host copies; the staging range is reused
>   only after that submission's tick completed.
> - Only read uploads (the BDA passes and read bindings) with UploadBatch; off with
>   KYTY_UPLOAD_DMA_VERIFY (it snapshots the staged bytes at queue time) and for non-coherent
>   staging memory.
>
> KYTY_UPLOAD_DMA_HOST_COPY=0 restores the command-processor copy. FrameEvents UploadDmaHostCopies and
> UploadDmaHostCopyBytes. Tests: upload_dma and upload_dma_host_copy_off (--upload-dma-only): the
> 2 MiB upload is left to the worker (or not, when off) and arrives intact; a re-dirtied range whose
> pages a writable binding makes GPU-owned (no access) while the worker is held is copied through
> the alias and arrives intact. Full suite 90/90 (shader_cfg and kernel_file_system excluded).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/uploadDma.cpp`, `src/graphics/host_gpu/renderer/cache/uploadDma.h`, `src/kernel/memory.cpp`, `src/kernel/memory.h`, `src/kernel/memoryAddressSpace.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 254. Merge claude/cpu-opt: render-state copies, upload DMA host copy, descriptor-set reuse audit

Commit: [`13243cce`](https://github.com/Jetsku/KytyPS5-experimental/commit/13243cce73ee44a8c0361dfef87962879ac96bd4) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_RENDER_STATE_FAST (default on; 0, or a list of vertex/swap/reset):
>   ApplyPreparedDraw copies only the used prefixes of the vertex input,
>   stage preps swap member by member, and Reset no longer zeroes target
>   entries the resolvers assign; KYTY_RENDER_STATE_VERIFY compares every
>   skipped copy or reset with the full one. About 1 ms/flip less CP time.
> - KYTY_UPLOAD_DMA_HOST_COPY (default on; 0 restores the CP copy): the DMA
>   worker copies staged guest bytes through the direct-memory backing alias
>   just before submitting the transfer, instead of the command processor.
>   About 1.1 ms/flip less CP time.
> - KYTY_DESCRIPTOR_SET_REUSE_AUDIT=1 (off by default) counts commits that an
>   unbounded or full-digest reuse cache would hit.

</details>

### 255. Tracking locks: write-unprotects leave the lock (KYTY_DEFER_UNPROTECT), waiters park

Commit: [`727ac9e6`](https://github.com/Jetsku/KytyPS5-experimental/commit/727ac9e6b6cf374835081cb92f59c7575c7a1377) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DEEP-TRACE-U52 item 5. Every guest write fault ran VirtualProtect while holding the
> 4 MiB region's TrackingSpinLock, and nested inside it the PageManager region spin lock:
> MemoryTracker::InvalidateRegion -> MarkWriteFault -> UpdateProtection ->
> PageManager -> VirtualProtect. The texture cache did the same under its global m_lock
> (InvalidateMemory -> InvalidateCpuAliases -> UntrackImage). Measured traced: guest threads
> spun 12.4 ms/flip on these locks, the CP 1.8 ms/flip, plus 1.8 ms/flip of protect calls.
>
> Deferred write-unprotect (PageManager::DeferUnprotectScope, default on):
> - Inside a scope, releasing a write watcher still updates the watcher counts, under the
>   region's count lock, in order with every other change. The host call for the pages it
>   makes looser moves to the end of the thread's outermost scope, from the counts as they
>   are then.
> - Scopes are opened before the callers' own locks: RenderContext::HandleFault (write
>   faults, both caches, so one host call per page), RenderContext::InvalidateMemory,
>   MemoryTracker::InvalidateRegion and TextureCache::InvalidateMemory. So the host call
>   happens after the tracker region lock and m_lock are released.
> - Everything that tightens protection stays synchronous: uploads, GPU-dirty read
>   protection, hot-page settles, image tracking. So do read-watcher changes and calls
>   outside a scope. Bits, serials, mirrors (b9b6654a) and on_ahead still change under
>   the tracker lock before the page becomes writable; only "writable" comes later.
>
> Two locks per PageManager region (always taken host_lock, then lock):
> - `lock` guards the counts and a per-page record of the last protection requested from
>   the host.
> - `host_lock` serializes the region's host calls and the decisions they rest on. Every
>   change that is not a deferred release (watches, read-watcher changes, synchronous
>   releases) and every deferred update holds it from reading the counts until the host
>   call is recorded.
> - With deferral on, `lock` is never held across a host call. So a release inside a scope
>   (a fault, under the tracker's region lock) never waits for another thread's
>   VirtualProtect.
>
> Why it is exact:
> - The only change that can overtake a host call is a deferred release, and it only
>   loosens the counts. So at every host_lock release no page's host protection is looser
>   than its watchers ask.
> - A pending release only leaves a page stricter (one extra fault). The last update
>   always writes the latest state, whatever order threads run in.
> - A stale "writable" cannot land after a newer protect: both hold host_lock from reading
>   the counts to recording the call.
> - A synchronous watch returns only after its protect, so an upload still copies after
>   the page is protected. Any write before that is in the copy; any write after it
>   faults.
> - HandleFault also reconciles the faulting page, so a fault whose page another thread
>   released but has not applied yet does not wait for that thread.
> - A write fault taken inside an enclosing scope applies its page at once (the enclosing
>   scope would only end after the retry); it is counted.
>
> Locks (KYTY_TRACKER_LOCK_PARK, default on): TrackingSpinLock (tracker regions, texture
> cache) and both PageManager region locks spin reading only, with pause, for
> KYTY_TRACKER_LOCK_SPIN_US (default 20), then park on the lock word (WaitOnAddress).
> Unlock wakes one waiter, only if one parked. Owner and recursion checks are unchanged.
>
> Switches:
> - KYTY_DEFER_UNPROTECT=0 releases under the caller's lock as before: host calls under
>   both region locks.
> - KYTY_DEFER_UNPROTECT=verify checks, with the region's host calls quiesced, after every
>   change, and counts and logs mismatches:
>   - no page's recorded or host (VirtualQuery) protection is looser than its watchers;
>   - the host holds what was just set.
> - KYTY_TRACKER_LOCK_PARK=0 restores the old spin loops exactly.
>
> Counters (FrameEvents appended): DeferredUnprotectSpans, DeferredUnprotectCalls,
> DeferredUnprotectSettled, DeferredUnprotectOverflows, DeferredUnprotectNestedFaults,
> TrackerLockParks, ProtectVerifyChecks, ProtectVerifyMismatches.
>
> Tests:
> - page_manager_tests:
>   - a release waits for its outermost scope, and watches stay synchronous;
>   - a pending release racing a re-watch, or a watch+release, on another thread;
>   - Reconcile of another thread's pending release;
>   - batch overflow;
>   - a 6-thread watch/release stress (a watched page is never writable), in all three
>     modes;
>   - the synchronous cases again under verify;
>   - the parking lock.
> - memory_tracker_tests:
>   - every existing case now runs with deferral on; the fault and upload cases also run
>     with it off and in verify mode;
>   - a deferred fault inside and outside an enclosing scope;
>   - an upload racing a pending fault release;
>   - a real-fault stress (Windows): 4 writer threads storing to tracked pages, with the
>     faults resolved as HandleFault does, while an uploader keeps protecting them. The
>     final upload must equal memory, in all three modes.
> - The GPU cache/draw-prep ctest cases pass, and pass again with KYTY_DEFER_UNPROTECT=verify
>   with 0 mismatches.
>
> Measured (memory_tracker_tests --fault-bench, CPUs 0-15, 3 runs each):
> - Setup: 8 writers each take one fault per page on their own pages of one region, per
>   round, while a third thread (the CP's role) queries the tracker.
> - Tracker query latency:
>
>   | Mode                       | p99 (us) | p99.9 (us) | max (us) |
>   |----------------------------|---------:|-----------:|---------:|
>   | Deferral off, park on      | 6.4-7.3  | 18-20      | 280-470  |
>   | Deferred, park on          | 0.4      | 0.7        | 34-45    |
>   | Deferral off, park off     | 20-21    | 38-41      | 183-276  |
>   | Deferred, park off         | 0.4-0.5  | 0.9-1.0    | 27-72    |
>
>   "Park off" here is the non-batch spin, as KYTY_RENDERER_BATCH is unset.
> - Fault throughput: 1.2-1.4 us of wall per fault before, 1.0-1.1 us after.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/pageManager.cpp`, `src/graphics/host_gpu/pageManager.h`, `src/graphics/host_gpu/parkingLock.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `tests/MemoryTrackerTests.cpp`, `tests/PageManagerTests.cpp`.

### 256. Merge claude/sched-contention: tracker locks leave VirtualProtect, parking lock waiters

Commit: [`3d1fd041`](https://github.com/Jetsku/KytyPS5-experimental/commit/3d1fd04117b9322d4f867ee5291b0eb01de9763a) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_DEFER_UNPROTECT (default on; 0 reverts; verify checks every change
>   against the watchers and VirtualQuery): write-unprotects on faults and
>   invalidations run after the tracker and texture-cache locks are released;
>   protects stay synchronous, so a page is never looser than its watchers.
> - KYTY_TRACKER_LOCK_PARK (default on; 0 restores the spin loops): tracker,
>   texture-cache and page-manager lock waiters spin for
>   KYTY_TRACKER_LOCK_SPIN_US (20) and then park.

</details>

### 257. Tiler parity: 2- and 1-byte depth-tiled colour views, depth-route check

Commit: [`2e4b2551`](https://github.com/Jetsku/KytyPS5-experimental/commit/2e4b255156506cd7e1b6269f77fdfab203efa9fc) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> CheckTilerImageDirect covered depth tiling only as R32F. At Astro Bot's
> 1368p layouts the half-resolution depth's R16 alias is rebuilt straight
> from buffer bytes through the colour path and DetileToImage; at 1080p it
> is copied from the D16 image instead. Adds R16 cases at every size the
> U52 hang trace shows (1216x684, 960x540, 1664x936, 1920x1080), a layered
> R16 case and a 1-byte case, plus check_depth_route: for every depth-tiled
> case the colour path's texels must equal the depth path's (the
> BuildDepthTiles layout), with the same pitch and plane size. All pass on
> the RTX 3090 (23 parity checks, 7 depth-route checks).

</details>

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 258. Texture cache: GPU ownership follows the bytes a write can reach (KYTY_ALIAS_BYTES)

Commit: [`04d76a82`](https://github.com/Jetsku/KytyPS5-experimental/commit/04d76a82f9468ebb77bb2f3dee0ada9c461e4bfb) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot's transient render-target heap aliases targets of different layouts. At the
> 1080p <-> 1368p resolution switches, U52 lost bytes that only an image held:
> - a bounded buffer write over part of an image took the whole image's ownership;
> - an image write took every overlapping image's ownership, including bytes it cannot write;
> - an overlap-stale free dropped the image's contents;
> - texel reads synchronized only a whole-image owner starting at the read address.
> Rebuilds and copies then read never-written or stale buffer bytes (the suspected cause of the
> Sky Garden water's block grid).
>
> With KYTY_ALIAS_BYTES (default on; =0 restores U52 exactly):
> - An image owns all of its bytes or an explicit set (Image::OwnedSet). A bounded buffer write
>   takes only its range. An image write takes only its claim from other images. A draw claims
>   only the 64 KiB blocks under the union of its scissors (DrawScissorUnion -> AcquireRenderTargets
>   -> FindRenderTarget; single-level, single-layer render-target-tiled colour targets; a
>   claimed-rectangle cache keeps repeated draws cheap). Copies move the source's owned bytes to the
>   destination.
> - TextureCache::MaterializeOwnedBytes tiles an owner into a scratch copy of the buffer bytes and
>   copies its owned ranges into the cache buffer, which then owns them (GPU-dirty). It runs before
>   a read rebuild (bytes other images own), before any rebuild (the image's own remaining bytes),
>   in texel-buffer reads and CopyBuffer sources (every owned byte the whole-image fast path did not
>   cover; a lock-free page check first), before a free (not unmaps; not images the GC already
>   downloaded), and for a partial alias owner in SyncAliasFromOwner.
> - New images over bytes other images own start buffer-modified.
> - Render, depth and storage bindings still rebuild without taking other images' bytes, and an
>   image another image partly overwrote is still sampled as it is (U52 behaviour; in the U43
>   steady state the first would cost tens of MB of tiling per frame).
> - Frame events AliasBytesKept, AliasBytesBoundedClaims, AliasBytesMaterializations,
>   AliasBytesMaterializedBytes, AliasBytesUnmaterialized, AliasBytesTexelReads,
>   AliasBytesTexelScans; hang-trace transfers "alias-bytes" (detail: free, rebuild, rebuild-own,
>   texel-read, alias-sync).
>
> Tests: CheckAliasBytesAcrossPartialWrites (--alias-bytes-only; ctest texture_cache_alias_bytes,
> _writeback, _off): a copy and a rebuild after a partial buffer write, a copy from inside a
> GPU-written image, an overlap-stale free, and a 15-frame two-layout replay with dynamic
> resolution. U52 fails all seven checks (writeback fixes three); the fix passes all. It also times
> the render-target claim path (FindRenderTarget about 60 ns on and off; the scissor union 10-14 ns
> per draw). CheckTexelSyncOverStaleGpuDirtyBytes now expects the image to keep its other bytes
> after a 4-byte bounded write (ctest texture_cache_texel_sync_gpu_dirty, _whole_images).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/image/image.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/host_gpu/renderer/renderDraw.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 259. Merge codex/astro-profile (U54, af95961e) into claude/water-tiler

Commit: [`e4782eb8`](https://github.com/Jetsku/KytyPS5-experimental/commit/e4782eb8565605af4afe7cf714d9dfafcf09753d) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Conflicts, both additive:
> - CMakeLists.txt: the KYTY_ALIAS_BYTES and texel-sync ctest entries next to srt_variant_reads.
> - textureCache.h: RefreshImage's RefreshIntent parameter next to MaterializeDccClear's new
>   MetadataNoop comment.
>
> The draw-sequence repeat (TryRepeatLookup) skips only FindImage; FindRenderTarget, where the
> scissor-bounded claim is made, still runs for every draw. Verified: shader_recompiler_compute
> full suite at defaults, with KYTY_DRAW_SEQUENCE_VERIFY=exit and with KYTY_ALIAS_BYTES=0; ctest
> 96/96 (without shader_cfg, kernel_file_system).

</details>

### 260. Merge claude/water-tiler: texture-cache GPU ownership follows the bytes a write can reach

Commit: [`779e5ba5`](https://github.com/Jetsku/KytyPS5-experimental/commit/779e5ba5267e49f9fac1c295cad3ca21cb1155d4) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_ALIAS_BYTES (default on; 0 restores U52 exactly): an image owns all of
> its bytes or an explicit set; bounded buffer writes, image writes and draws
> (the 64 KiB blocks under their scissor union) take only what they reach, and
> owned bytes are copied into the buffer before any read goes through buffer
> bytes (rebuilds, texel reads, copy sources, frees). Fixes the Sky Garden pool
> water's block grid above 60 fps, where the game's dynamic resolution switches
> between aliased 1080p and 1368p render-target layouts. Also tiler parity
> cases for 2- and 1-byte depth-tiled colour views.

</details>

### 261. Shaders: pixel DS_APPEND/DS_CONSUME without helper lanes by default

Commit: [`a5195147`](https://github.com/Jetsku/KytyPS5-experimental/commit/a519514787f1a6100ef67137526d71052be2493d) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_PS_LIVE_EXEC=1 (pixel shaders that contain DS_APPEND/DS_CONSUME start
> with EXEC = the non-helper invocations, as the PS5's pixel valid mask) and
> KYTY_PS_APPEND_LIVE_ELECTION=1 (the append's elected lane is never a helper)
> are now the defaults; =0 restores the old behaviour. Only pixel shaders that
> append change: on the default path that is Astro Bot's mesh-particle emitter
> 0x4dd1f85484fc31f2, which could get an undefined append base (duplicate or
> lost particle slots) or leave unwritten slots; with the GI patch disabled also
> the 223 ray-bundle shaders. U53 ran both switches in Sky Garden (the emitter
> was compiled with gl_HelperInvocation and drawn; no device loss, no particle
> problem seen).

</details>

Changed files: `src/graphics/shader/recompiler/CodegenOptions.h`, `tests/ShaderGiProbeTests.inc`.

### 262. Merge claude/rt-gi-probes: pixel DS_APPEND/DS_CONSUME without helper lanes by default

Commit: [`f3b410a0`](https://github.com/Jetsku/KytyPS5-experimental/commit/f3b410a00e7f0a85508770e1f0c6ae80fe82f19c) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_PS_LIVE_EXEC=1 and KYTY_PS_APPEND_LIVE_ELECTION=1 become the defaults
> (=0 on either restores the old behaviour): pixel shaders with DS_APPEND or
> DS_CONSUME start with EXEC holding only the non-helper invocations, as the
> PS5's initial EXEC does. Fixes Astro Bot's mesh-particle emitter
> 0x4dd1f85484fc31f2, which could overwrite or lose particle slots when a
> subgroup's first lane was a helper; confirmed in game in the U53 GI run.

</details>

### 263. Desert: native indirect mesh draws without the per-flip args drain; stamp set-reuse and sampler memo savings

Commit: [`0cd821d0`](https://github.com/Jetsku/KytyPS5-experimental/commit/0cd821d034ec8c33acdb1f0f57c41e834b07fa7a) · **Native state and descriptor reuse**

Avoids repeated native state construction, hashing or binding when identity and contents permit reuse. Diagnostic audits measure opportunity, not achieved frame-time savings.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> 1) Indirect mesh draws: no CPU read of provably empty records; GPU conversion
>    (KYTY_NATIVE_INDIRECT_MESH)
>
> The per-flip readback drain in every scene (U52: GpuWaitSideCopy 2.27 ms/flip in the desert
> run, 1.65 on the slide; all 16,146 gpu-sync rows of hangtrace-u52-gpu-20260927-222918) is one
> DRAW_INDEX_INDIRECT whose vertex stage is NGG, so it ran as a mesh draw, which
> DrawIndirectNative did not support (DrawIndirectFallbackMesh 1/flip). The command processor
> read its 20-byte record at 0x56ddaffa0, written by a shader of the recording the eager flush had
> just submitted: eager publication cannot help (the copy can only follow the producer, which the
> GPU has not run yet when the CP reads), so the CP waited for the GPU backlog.
>
> The draw binds INDEX_BASE = record + 0x20 and INDEX_BUFFER_SIZE = 1 (U50 Sky Garden and U53
> desert DCB dumps). The CPU path clamps its count to 1 index, below one triangle, so it draws
> nothing whatever the record holds (MeshInputIndices 0 in both desert captures).
>
> MeshIndirect::AlwaysEmpty recognises that from the registers (indexed, INDEX_BUFFER_SIZE below
> the input primitive size): DrawIndirectNative then consumes the draw without reading the record,
> as the CPU path would draw it: nothing. Target operations and restart values that need an index
> scan still take the CPU path first; the record stays the pending NUM_INSTANCES source as for
> every native indirect draw.
>
> Other indirect mesh draws can be converted on the GPU: a helper dispatch (gpu_mesh_indirect.comp)
> turns the record into up to 4 mesh dispatches and their six draw dwords exactly as the CPU path
> derives them (MeshIndirect::Convert is the C++ twin), and the mesh shaders read those dwords
> from the parameter block at the address in push dwords 0-1 when dword 3 is the sentinel
> (CodegenOptions::mesh_indirect_params). Its status word reports the documented differences
> (more than 4 dispatches, draws beyond the host limits, indices past INDEX_BUFFER_SIZE).
>
> KYTY_NATIVE_INDIRECT_MESH: unset/empty (default) provably empty draws only, no codegen change;
> 1/on also the GPU conversion; verify/exit on plus a completion check of every conversion against
> Convert() of the bytes the GPU read; 0 off (the CPU path for every indirect mesh draw).
> Counters MeshIndirect{Draws,AlwaysEmpty,Declined*,VerifyChecks,VerifyMismatches,Status}.
>
> 2) Desert stamps: no set-reuse lookup for fresh uploads (KYTY_SET_REUSE_FRESH), 4-way sampler
>    memo
>
> The desert slide's stamps (about 900-1,500 mesh DRAW_INDEX_AUTO per flip, one program pair,
> NGG VS 0x500035600 / PS 0x50003b900, mesh program 0xb6dbdd5904905160) each carry a new SRT
> pointer, per-stamp V# and user data, so every stamp uploads a new flattened SRT and shader-data
> table and writes a new descriptor set (933 sets/flip, DescriptorSetsReused 0 in every U52
> capture).
>
> KYTY_SET_REUSE_FRESH (default on; 0 off; verify, exit): CommitDescriptorSet skips the
> DescriptorSetReuse hash, lookup and insert when the set's writes refer to a shader-data or
> flattened-SRT range UploadShaderData allocated for this draw (RebindBuffers records it in
> PreparedBindings::fresh_upload). The reuse cache holds sets written during the current tick
> only, and the stream ring never hands out a range twice within one tick (a wrap submits and
> completes the tick first), so no remembered set refers to that range and the lookup cannot hit;
> not remembering the set only loses hits by later draws whose dedup returns the same range with
> byte-identical tables, which the stamps never have. verify/exit still look up and insert, and
> report (or stop on) a hit. About 0.32 ms/flip of DescriptorSetReuse Hash/Insert on the U52 slide.
> Counter DescriptorSetReuseSkippedFresh.
>
> KYTY_SAMPLER_MEMO becomes 64 sets of 4 ways, most recently used first: the stamps bind eight
> samplers per draw and two of them shared a direct-mapped slot (SamplerMemoMisses 1,815 per flip
> on the slide against 159 in the run). The key is unchanged (the four final S# dwords, exactly
> the SamplerCache key) and the cache never evicts, so a remembered handle is always the one
> GetSampler returns; the associativity only decides which entries stay remembered.
>
> Tests: mesh_indirect (default), mesh_indirect_on, mesh_indirect_verify (exit), mesh_indirect_off:
> the GPU conversion and Convert() against a step-by-step simulation of the CPU path (indexed and
> not, clamps, zero counts, lines/points/strips/fans, instance splits, overflow, host limits,
> wrapping first instance), AlwaysEmpty against InputPrimitiveCount, and an NGG draw end to end
> (always-empty record left GPU-owned and undrawn; converted draws of 3 and 70,000 instances equal
> to the CPU path's pixels). mesh_indirect_codegen: SPIR-V validates with the option on and off,
> with and without guest address loads, and the option adds exactly one sentinel compare and one
> parameter-block load per draw dword read. The GPU test harness enables mesh shaders and the
> indirect draw features when the device has them; only checks inside a MeshShaderScope see them
> in the runtime context. sampler_memo: colliding S#s all stay remembered, a fifth evicts the least
> recently used, every answer equals SamplerCache::GetSampler. set_reuse_fresh / _off / _verify:
> a set without a fresh upload is remembered and found; a fresh one is not remembered unless off
> or verify; verify reports a fresh set the lookup finds.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/streamBuffer.cpp`, `src/graphics/host_gpu/renderer/cache/streamBuffer.h`, `src/graphics/host_gpu/renderer/meshIndirect.cpp`, `src/graphics/host_gpu/renderer/meshIndirect.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/host_gpu/shaders/gpu_mesh_indirect.comp`, `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterFlow.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterModule.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `tests/ShaderRecompilerComputeTests.cpp`, `tests/shaderCfgTests.cpp`.

### 264. Merge claude/desert-stamps: no per-flip args drain for empty indirect mesh draws; set-reuse skip, 4-way sampler memo

Commit: [`c97f7b96`](https://github.com/Jetsku/KytyPS5-experimental/commit/c97f7b964468a87db5da56301f3638d4efd173e3) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_NATIVE_INDIRECT_MESH (default "empty"): the one indirect mesh draw per
>   flip whose INDEX_BUFFER_SIZE is below one primitive draws nothing on the
>   CPU path, so it is consumed without reading its GPU-written record. That
>   removes the per-flip readback wait (U54 desert run 3.9 ms/flip). =1/on
>   adds a GPU conversion of other indirect mesh draws (verify/exit check it).
> - KYTY_SET_REUSE_FRESH (default on): descriptor sets that refer to a fresh
>   upload skip the reuse lookup that cannot hit (about 0.3 ms/flip on the
>   desert slide).
> - KYTY_SAMPLER_MEMO becomes 4-way set-associative (the slide's stamps hit a
>   direct-mapped collision, 1,815 misses per flip).

</details>

### 265. CP recorder (P2a): Vulkan emission off the CP thread (KYTY_CP_RECORDER, default off)

Commit: [`f60e7ae5`](https://github.com/Jetsku/KytyPS5-experimental/commit/f60e7ae5fd377be435f900cb9b4b562060689dd1) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Design: Profiling/analysis/CP-RECORDER-P2.md. The command processor keeps every decision it
> makes today (barrier batching, render-pass continuation, pipeline/push-constant/push-descriptor
> dedup, dynamic-state shadow, all resolution and guest-visible work); only the native vkCmd*
> calls, vkBegin/EndCommandBuffer and the queue hand-off become fully resolved packets in a
> single-producer ring, which one recorder thread replays in order into the same command buffer
> and submits exactly as the CP did (broker in queued mode, queue_mutex submit otherwise).
>
> - renderer/commandStream.h/.cpp/.inl: SPSC byte ring (wrap packets, batched publication,
>   spin-then-park doorbells on both sides), Encoder with vk::CommandBuffer's argument lists
>   (every argument copied, no pointer into CP memory), templated Replay, VerifyHash functions
>   shared by both sides.
> - renderer/commandRecorder.h/.cpp: recorder thread (CP priority, bounded idle spin), native
>   executor (begin/end, KYTY_GPU_TIMING and GpuOpProfiler stamps, broker or direct submit with
>   its own SubmissionProgress), drains, WaitRecorded, drain histogram
>   (KYTY_CP_RECORDER_DRAIN_LOG=1, every 10 s and at shutdown), placement sampling and the
>   opt-in ideal-CPU hint (KYTY_CP_RECORDER_IDEAL_CPU).
> - CommandBuffer: CommandSink (vk::CommandBuffer method names; native with the recorder off,
>   so the default path is unchanged), Sink() with Handle()'s semantics minus the drain,
>   StateSink(), EmissionSink() safe points, Identity() for transition targets. In recorder mode
>   Handle()/StateHandle() drain the recorder and open a direct window: every site not converted
>   yet records natively at its exact position.
> - Converted: every CommandBuffer native call, the draw emission tail (vertex/index binds,
>   dynamic state, draws, mesh, indirect), dispatch/dispatchIndirect, occlusion queries.
> - Scheduler: logical ticks stay on the CP; Begin/Submit are packets; the pool grows only after
>   a drain; Shutdown stops the recorder before the timing ring and pool are touched.
> - MasterSemaphore tracks host dispatch whenever the recorder runs, so KnownGpuTick/IsFree/Wait
>   cover the recorder's lag and all tick-based retirement stays sound.
> - Side-copy readbacks and RenderDoc capture boundaries wait until the recorder handed older
>   ticks to the queue (before taking queue_mutex).
> - Verify (KYTY_CP_RECORDER_VERIFY=1|exit): per-packet hash of the original call arguments
>   against the arguments passed to the driver plus sequence numbers, per-command-buffer digest
>   and count at Submit, and dispatcher hooks rejecting native recording into the guest command
>   buffer by the replayer inside a window or by anyone else outside one.
> - KYTY_CP_RECORDER=inline replays each packet on the CP (no thread) for A/B isolation.
>
> Tests: cp_recorder (CPU: every packet type round-trips, wraps, back-pressure, threaded SPSC,
> corrupted/dropped/duplicated packets detected); cp_recorder_gpu(_queued/_inline) (program order
> across a direct window, a CP waiting on a tick still in the ring, a drain waking a parked
> recorder, a full 1 MiB ring, ownership-fault detection); the whole compute suite and the
> renderer/cache focused modes with the recorder thread (synchronous and queued submission) and
> inline replay, all under verify=exit. The test harness starts the submission broker when
> KYTY_SUBMISSION_MODE=queued. --cp-recorder-bench measures CP ns per draw in each mode.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/commandRecorder.cpp`, `src/graphics/host_gpu/renderer/commandRecorder.h`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.h`, `src/graphics/host_gpu/renderer/commandStream.cpp`, `src/graphics/host_gpu/renderer/commandStream.h`, `src/graphics/host_gpu/renderer/commandStreamReplay.inl`, `src/graphics/host_gpu/renderer/context.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.h`, `src/graphics/host_gpu/renderer/gpuTiming.cpp`, `src/graphics/host_gpu/renderer/gpuTiming.h`, `src/graphics/host_gpu/renderer/masterSemaphore.cpp`, `src/graphics/host_gpu/renderer/masterSemaphore.h`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/presentation/renderDoc.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`, `tests/CpRecorderTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 266. Merge claude/cp-recorder: P2a, Vulkan emission off the CP thread (KYTY_CP_RECORDER, default off)

Commit: [`8cd57940`](https://github.com/Jetsku/KytyPS5-experimental/commit/8cd57940b54583f62f850db25afd1eeda264cf70) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The CP keeps every decision and encodes only the native vkCmd* calls as
> typed packets into an SPSC ring; a recorder thread replays them in order
> and submits (KYTY_CP_RECORDER=1; inline replays on the CP for A/B).
> Unconverted sites drain the recorder and record natively in a direct window.
> KYTY_CP_RECORDER_VERIFY hashes every packet against the driver arguments,
> checks per-command-buffer digests and rejects native recording outside the
> recorder or a direct window. Default off until checked in game.

</details>

### 267. Mesh indirect conversion: record through Sink() instead of Handle()

Commit: [`8a5f2d9d`](https://github.com/Jetsku/KytyPS5-experimental/commit/8a5f2d9d2329b0c1dad7e0d9d9c7500c6e443b12) · **Geometry and indirect execution**

Preserves the named geometry semantics or keeps eligible indirect arguments on the GPU. Correctness is distinct from avoiding a CPU readback; it does not automatically reduce scene complexity.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The GPU conversion's push constants and dispatch (KYTY_NATIVE_INDIRECT_MESH=1)
> go through the command sink, so with KYTY_CP_RECORDER they are encoded
> instead of draining the recorder into a direct window. Sink() records the
> pending barrier batch first and resets the push-constant shadow, as
> Handle() did. The default mode never runs this path.

</details>

Changed files: `src/graphics/host_gpu/renderer/meshIndirect.cpp`.

### 268. CP recorder: idle drains, tests under the Vulkan validation layer

Commit: [`eb798fab`](https://github.com/Jetsku/KytyPS5-experimental/commit/eb798fab45e5bfde0e206f2dc9f21467db672679) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - CommandRecorder::Drain: when the ring's consumed index equals the write
>   position, the recorder has executed and released every encoded packet. The
>   direct window then opens without a DrainMarker and without waking a parked
>   recorder (about 20 us per drain in the tests). Idle drains are counted
>   separately: FrameEvent.CpRecorderIdleDrains, the shutdown summary, and the
>   drain histogram (count, idle, us per drain, us per waited drain).
> - CheckCpRecorder 1b: a drain with nothing encoded since the previous one is
>   idle and encodes nothing. The bench line reports idle drains and drain time.
> - Test harness: KYTY_TEST_VULKAN_VALIDATION=1|sync|full runs the instance
>   under VK_LAYER_KHRONOS_validation. 1 enables core and thread-safety checks,
>   sync adds synchronization validation, and full keeps shader validation. The
>   harness fails at teardown on any validation error whose id is not listed in
>   KYTY_TEST_VULKAN_VALIDATION_IGNORE.
> - Test harness device and teardown now match the emulator. This fixes the
>   errors the layer found with the recorder off:
>   - depthClamp is enabled when supported (the renderer always sets
>     depthClampEnable);
>   - VK_EXT_depth_range_unrestricted is enabled when available (GL clip-space
>     viewports have minDepth = zoffset - zscale);
>   - teardown calls GraphicContext::DestroyAllocator, which frees the native
>     image pool's retired images before the allocator. A bare
>     vmaDestroyAllocator left them alive: 1 and 12 leaked VkImages.
> - ctest (label validation): cp_recorder_gpu, draw_prep_engine,
>   gpu_command_lane, stream_buffer_ring, texture_cache_layered_image and
>   buffer_cache_eager_readback under the layer, with the recorder off (the
>   baseline), in thread mode (queued submission) and inline.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/commandRecorder.cpp`, `src/graphics/host_gpu/renderer/commandRecorder.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 269. Merge claude/cp-recorder: idle drains, recorder tests under the Vulkan validation layer

Commit: [`5164ba27`](https://github.com/Jetsku/KytyPS5-experimental/commit/5164ba278338802e7504abf42f0f3bb5c7883033) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - CommandRecorder::Drain skips the marker and the wait when the ring is
>   already consumed (no wake of a parked recorder); CpRecorderIdleDrains.
> - KYTY_TEST_VULKAN_VALIDATION=1|sync|full in the test harness, with 17
>   ctest entries under the validation label (recorder off, thread, inline).
>   The harness device now enables depthClamp and
>   VK_EXT_depth_range_unrestricted like the emulator, and tears down through
>   DestroyAllocator: 17/17 pass with 0 validation errors.

</details>

### 270. DrawPrep: wake parked workers at a backlog of 2; optional CP work stealing (KYTY_DRAW_PREP_STEAL)

Commit: [`b0beea75`](https://github.com/Jetsku/KytyPS5-experimental/commit/b0beea7595bca3bec2efb7e8ea16fdb17f2bcfb6) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DEEP-TRACE-U54 3.1, a regression from 3019e3c6 (two hot workers, the rest parked until 8 slots
> wait unclaimed). The CP now spins in Engine::CommitHead, or prepares heads itself, much more:
> 0.76 -> 1.90 ms/flip at the Sky Garden start, 0.39 -> 1.77 on the slide, and 0.54 -> 3.13
> ms/frame on the slide's heaviest frames. DrawPrepCommitWaits rose from 53 to 235 per flip. The
> cause: after a fence, bursts of expensive draws arrive, the two hot workers prepare them two at a
> time, and bursts shorter than 8 never wake a cold worker.
>
> Default change: KYTY_DRAW_PREP_WAKE_BACKLOG 8 -> 2. KYTY_DRAW_PREP_HOT stays 2.
>
> Work stealing (new; off by default after the measurements below):
> - KYTY_DRAW_PREP_STEAL=N (N >= 1) enables it. While a worker holds the head the CP must commit,
>   the CP claims the oldest unclaimed published slot, as long as at least N wait unclaimed.
>   KYTY_DRAW_PREP_STEAL_AFTER_US first spins that long on the held head.
> - The claim uses the workers' own Window::TryClaim. The CP prepares the slot exactly as a worker
>   does: one shared Workers::PrepareClaimed, i.e. Prepare(exact=false), the repeat-trace hash,
>   worker_prepared=true, and Complete(seq) with a release store. t_worker_thread is set for the
>   duration, so CommandScheduler::CheckActive traps any scheduler access as on DrawPrep#k.
> - A stolen slot is committed later, in order, when it is the head, through the same Validate
>   (log or value certificate) and KYTY_DRAW_PREP_VERIFY as any worker-prepared slot. Only a head
>   the CP claims itself keeps the exact predicate.
> - CommitHead still commits only the head. The producer publishes nothing inside it, so there is
>   one steal phase and then at most one spin phase. The 2 ms service-commands guard is kept, and
>   now counts only spinning.
> - The steal/spin loop is workerGate.h AwaitHead (StealPolicy, AwaitStats), shared by
>   Engine::CommitHead and the unit tests, as RunPreparationWorker already is.
> - CommitHead exits if a preparation is active on the CP (recorder and per-thread scratch must be
>   idle).
>
> Counters:
> - FrameEvent DrawPrepSteals (appended) counts stolen slots; FrameWait DrawPrepSteal (appended) is
>   their CP time.
> - FrameWait DrawPrepCommitWait keeps one call per held head, but its time is now only the idle
>   spin (AddFrameWait).
> - FrameEvent DrawPrepCommitWaits keeps its meaning: heads a worker still held when needed.
>
> Measured with draw_prep_tests --measure-worker-gate [reps] [hot/backlog/steal/after_us,...].
> Setup: the production gate, worker loop and AwaitHead; 15 interleaved runs per configuration,
> medians; pinned to CPUs 0-15, 7950X3D, machine idle. The frame-shaped workloads follow the U54
> Tracy draws-per-fence counters. The CP commits each draw at 7-8 us.
> - Heavy burst (the slide's segments; each segment of 4+ draws starts with 8 draws of 30-45 us
>   Prepare, 8-16 us otherwise; 18,097 draws in 16 frames):
>     hot 2, backlog 8, no steal (U54)   270.4 ms, CP spin 10.19 ms, worker CPU  562 ms
>     hot 2, backlog 2, no steal (new)   265.2 ms (-1.9%), spin 5.18,  worker CPU  531 ms
>     hot 2, backlog 2, steal 1          266.8 ms (-1.3%), spin 0.60,  worker CPU  516 ms
>     hot 2, backlog 4, steal 1          268.5 ms
>     hot 3, backlog 2, no steal         266.0 ms, worker CPU 766 ms
>     hot 4, backlog 8, no steal         265.6 ms, worker CPU 969 ms
>     all six hot (U52)                  266.0 ms, spin 3.87,  worker CPU 1312 ms
>   The model reproduces the regression: the CP spins 2.6x as long at U54's settings as with all
>   workers hot. The game went from 0.76 to 1.90 ms/flip, 2.5x.
> - The same with workers stalling 300 us on 0.3% of their slots (a worker preempted or late to
>   wake while it holds a slot): U54 282.6 ms; new 279.2 (-1.2%); steal 280.3; hot 4 277.7.
> - Sky Garden start (4,955 draws per frame, long runs): every configuration within +-0.8%
>   (177-179 ms for 4 frames).
> - Stealing: never better than no stealing at backlog 2, in any policy tried. Against no stealing
>   (heavy burst / with stalls): immediate +0.4% / +0.3%; min backlog 2 after 20 us +0.3% / +0.1%;
>   after 20 us +0.5% / +1.2%; after 50 us it almost never triggered. It removes most of the CP's
>   idle spin, but a stolen preparation that outlasts the head delays every commit behind it.
>   Hence off by default; the switch stays for game checks.
>
> Tests:
> - draw_prep_tests:
>   - TestAwaitHead, deterministic: steals in claim order until the head is done; no claims without
>     stealing; nothing claimable; a lagging claim counter; the minimum backlog; the delay.
>   - TestWorkerGateStress: now with and without stealing.
>   - TestStealStress: windows 4-32, 1-6 workers, five steal policies. Every slot is prepared once
>     and retired in order, and steals happen.
> - New ctest variants draw_prep_engine_parallel_steal and draw_prep_engine_parallel_steal_one_worker
>   (KYTY_DRAW_PREP_STEAL=1, KYTY_DRAW_PREP_VERIFY=exit).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `src/graphics/host_gpu/renderer/drawPrep/workerGate.h`, `tests/DrawPrepTests.cpp`.

### 271. EOP timestamps: optional GPU times for guest clock writes (KYTY_EOP_TIMESTAMPS), DRS trace columns

Commit: [`e2b935a6`](https://github.com/Jetsku/KytyPS5-experimental/commit/e2b935a6c4bbe8d498eb1eb9cea8c596db62125d) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Astro Bot's dynamic resolution works from GPU time it measures with end-of-pipe clock writes.
> Kyty writes those, like every label, when the command processor records the packet. So the game
> measured CP recording time, and it raises the resolution whenever the CP is fast, whatever the
> GPU costs (WATER-TOPDOWN-GLITCH.md: 1080p at 20 fps, 1368p/4K above 60).
>
> What the game measures (eboot 01.018 static analysis, U51 CP trace):
> - Clock writes: RELEASE_MEM / EVENT_WRITE_EOP with the 64-bit clock data select, about 880 per
>   frame at 4,100 addresses. None is a WAIT_REG_MEM target, a WRITE_DATA target or a label.
> - The controller is 0x73e5060: Update(a, b, c, fps), called from the frame-statistics function
>   0x2a57e0. Its debug overlay prints "RESOLUTION %s | BUDGET %5.2f/%5.2fms | PMODE %d".
> - Inputs come from GPU timer rings of 8 slots (begin at B+0x10+32i, end at B+0x110+32i).
>   - The reader takes the slot 3 back and uses (end-begin)/100 us, accepting only begin != 0 and
>     end >= begin.
>   - a is the scalable pass; the total is the graphics timers plus two compute timers.
> - Decision:
>   - Downgrade when the total exceeds 1000/fps ms. fps is a constant 60 (0x29fef0), so 16.67 ms.
>   - Upgrade after 31 updates with room: budget - (total - a) > 0.85 * a * pixels(next)/pixels(cur).
>   - Levels: 1920x1080, 2432x1368 (x2), 3328x1872, 3840x2160.
>
> KYTY_EOP_TIMESTAMPS=record (default, unchanged) | gpu | gpu-verify:
> - gpu: every clock write still stores its record-time value at once. Labels, fences and their
>   order are untouched, and a later record-time label never becomes visible before the slot has a
>   value.
> - A vkCmdWriteTimestamp2(ALL_COMMANDS) is also recorded at the packet's position, through
>   StateHandle(): it touches no guest resource, so no barrier flush and no rendering split.
> - At its next clock write (a fence, draw-prep window empty), the CP reads the queries of completed
>   ticks without waiting, one read per run of slots. It converts them to the guest clock:
>   - device -> QPC via VK_KHR/EXT_calibrated_timestamps, recalibrated every 100 ms;
>   - QPC -> TSC via a paired read;
>   - TSC -> 100 MHz, ReadReferenceClock's domain.
> - It then rewrites each slot that still holds its record value. A slot the guest wrote again keeps
>   the guest's value (counted).
> - A clock write deferred with its label (ordered/proxy/completion) gets the GPU value from the
>   deferred write on the completion runner.
> - Unavailable slots, results or calibration keep the record value. The CP never waits, and no
>   service command or extra draw-prep drain is added.
> - Query slots are reset in bulk at command-buffer begin, outside rendering, once consumed
>   (EopTimestamps::QueryRing).
> - The calibrated-timestamp extension is now also enabled in gpu mode.
> - COPY_DATA from the clock stays CP time, as on hardware.
> - gpu-verify additionally checks each published value against its record time and the previously
>   published one, with a 20 us tolerance.
> - Why this is consistent for the game: its reader looks at least 3 slots back, far behind the 1-2
>   ms GPU lag, so begin and end are both rewritten before it reads them. A guest running ahead of
>   the CP reads the previous cycle's (already rewritten) slot, as stale as today.
>
> Counters:
> - FrameEvent (appended): EopTimestampsRewritten, EopTimestampsSkipped, EopTimestampsUnavailable,
>   EopTimestampsDeferred, EopTimestampsVerifyChecks, EopTimestampsVerifyMismatches.
> - FrameWait EopTimestampPublish (appended): the CP's publishing time.
>
> Hang trace, timestamps.csv (every mode), one row per flip:
> - rewrite statistics: counts, mean and max GPU - record shift, publishing time;
> - the latest end - begin of the eight DRS/timer rings (KYTY_HANG_TRACE_TIMESTAMP_RINGS, default
>   the U51 addresses);
> - the game's DRS controller state through its pointer at eboot+0xee29f68
>   (KYTY_HANG_TRACE_DRS_PROBE, 0 = off): resolution index, level, updates with room, fps, and the
>   scalable and total GPU ms. The values are validated.
> - These reads run on the present thread and never fault: the backing view, else
>   ReadProcessMemory (HangTrace::TryReadReadable, ModuleBase).
> - The label-ts-gpu CP trace event records each rewrite (value = GPU, ref = record).
>
> Also: Sync::ScaleReferenceClock is now inline in referenceClock.h, so the conversion tests need no
> renderer.
>
> Tests (full suite 87/89; the failures are the known shader_cfg and kernel_file_system, not built):
> - eop_timestamp_tests (CPU): device deltas with 36-bit wrap; conversion (calibration and pair
>   offsets, device period, invalid maps); query-ring order, blocking, wrap-split resets; a
>   concurrent stress of 200,000 allocations through 64 slots with an out-of-order consumer
>   (20 of 20 runs pass).
> - The compute-test harness now enables the calibrated-timestamps extension like the emulator's
>   device does.
> - shader_recompiler_compute_tests --eop-timestamps-only:
>   - record mode keeps the values;
>   - gpu mode rewrites both slots with record <= begin <= end <= now, and keeps a slot the guest
>     wrote again;
>   - with KYTY_LABEL_MODE=completion the deferred writes carry GPU times;
>   - gpu-verify finds no mismatch.
>   ctest: eop_timestamp, eop_timestamps, eop_timestamps_gpu, eop_timestamps_gpu_verify,
>   eop_timestamps_gpu_deferred.

</details>

Changed files: `CMakeLists.txt`, `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.h`, `src/graphics/host_gpu/renderer/eopTimestampClock.h`, `src/graphics/host_gpu/renderer/eopTimestamps.cpp`, `src/graphics/host_gpu/renderer/eopTimestamps.h`, `src/graphics/host_gpu/renderer/gpuTiming.cpp`, `src/graphics/host_gpu/renderer/gpuTiming.h`, `src/graphics/host_gpu/renderer/referenceClock.h`, `src/graphics/host_gpu/renderer/sync.cpp`, `src/graphics/host_gpu/renderer/sync.h`, `src/graphics/presentation/videoOut.cpp`, `src/graphics/presentation/window/vulkanWindow.cpp`, `tests/EopTimestampTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 272. Merge claude/sched-contention: draw-prep wakes at a backlog of 2, optional CP stealing, GPU times for guest clock writes

Commit: [`9042db36`](https://github.com/Jetsku/KytyPS5-experimental/commit/9042db36a980a9c23bbe4676b2a56c7bc1838e4c) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_DRAW_PREP_WAKE_BACKLOG default 8 -> 2: parked draw-prep workers wake
>   when two slots wait unclaimed (DEEP-TRACE-U54 3.1, CP commit waits
>   0.76 -> 1.90 ms/flip at U54). KYTY_DRAW_PREP_STEAL=N (off by default):
>   the CP prepares the oldest unclaimed slot while a worker holds the head,
>   committed later through the same validation (DrawPrepSteals).
> - KYTY_EOP_TIMESTAMPS=record (default, unchanged) | gpu | gpu-verify: with
>   gpu, end-of-pipe clock writes still store their record-time value at once
>   and are rewritten with the query's GPU time, converted to the guest
>   clock, once the tick completed (deferred labels carry it themselves).
>   Astro Bot's dynamic resolution measures these. hang-trace timestamps.csv
>   (every mode): rewrite statistics, the guest GPU timer rings
>   (KYTY_HANG_TRACE_TIMESTAMP_RINGS) and the DRS controller state
>   (KYTY_HANG_TRACE_DRS_PROBE).
>
> Conflicts: profiler.h/.cpp keep U55's FrameEvent/FrameWait entries first,
> then DrawPrepSteals, EopTimestamps* and DrawPrepSteal, EopTimestampPublish;
> hangTrace keeps U55's unclean.csv and the new timestamps.csv; the scheduler
> keeps both the CP recorder and the EOP query ring (the ring is destroyed
> after the recorder, and after the priority runner that reads it).
>
> With KYTY_CP_RECORDER the recorder begins each command buffer, so
> BeginCommand now resets the EOP query slots through a direct window
> (StateHandle) in recorder mode too; otherwise no slot would ever become free
> and gpu mode would keep every record-time value. New ctest variants
> eop_timestamps_gpu_cp_recorder and _inline.

</details>

### 273. BDA sync: synchronize the ranges dirtied since the last pass, not every buffer (KYTY_BDA_DIRTY_LOG)

Commit: [`25f1e2f8`](https://github.com/Jetsku/KytyPS5-experimental/commit/25f1e2f81b5e852c0f17e3c9e5a056b8ad3a238d) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U54 start view: 5.7 BDA passes per flip scan 4,176 buffers (about 730 per pass), and the scan
> itself (SynchronizeBuffersInRange, SynchronizeBuffer entry, the relaxed dirty queries) costs about
> 0.56 ms/flip traced on the command processor. With KYTY_BDA_INCREMENTAL_SYNC and KYTY_BDA_HOT_SYNC
> a pass already skips when the fault and structure epochs are unchanged, but any write fault since
> the last pass (1,843 per flip) made it scan every mapped buffer again.
>
> The tracker now logs, for every transition FaultMutationEpoch() covers, the range it can make
> CPU-dirty: a write fault's whole fault-ahead window (MemoryTracker::FaultWindow, the window
> RegionManager::MarkWriteFault uses), explicit CPU-dirty marks, a new region, untracking, hot-page
> demotions and sweeps (their whole region). The range is added under the same log lock that
> advances the epoch, while the caller holds the region lock and before it changes the pages' bits,
> so MemoryTracker::TakeDirtiedRanges returns the ranges together with the epoch they account for.
> A BDA pass whose structure epoch is unchanged but whose fault epoch moved then synchronizes only
> the logged ranges (inside the mapped ranges) and the recorded hot runs, adding the hot runs the
> logged ranges turn up (a page becomes hot only through a logged write fault):
> - after the last pass every page of a mapped buffer was clean (uploaded and write-protected) or
>   hot; one can only turn CPU-dirty again through a logged transition, and whoever handles a taken
>   range under the region lock sees the bits the transition set;
> - a full scan runs when the log overflowed (over 4096 disjoint ranges), the buffers or mappings
>   changed (structure epoch), no full scan with the log has run yet, a recorded hot run's buffer is
>   gone, or more than 4096 hot runs accumulated.
>
> KYTY_BDA_DIRTY_LOG=0 restores the full scans. KYTY_BDA_DIRTY_LOG_VERIFY=1|exit follows every logged
> pass with the full scan it replaced and counts the normal CPU-dirty pages that scan finds while the
> pass's epochs still hold (expected 0). FrameEvents BdaSyncLogPasses, BdaSyncLogRanges,
> BdaSyncLogOverflows, BdaSyncLogVerifyChecks, BdaSyncLogVerifyMismatches.
>
> Tests: memory_tracker TestDirtiedLog (new region, a take without transitions, a write fault's
> window exactly, an explicit mark, each with its epoch); CheckBdaSyncEpoch now has a second write
> in a later epoch and checks that both passes with unchanged buffers synchronize from the log and
> upload the writes (ctest bda_sync_dirty_log_verify with the verify modes at exit, and
> bda_sync_dirty_log_off). Full suite 92/92 (shader_cfg and kernel_file_system excluded).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/rangeSet.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `tests/MemoryTrackerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 274. Host writes: give emulator writes of guest bytes the guest write-fault transition (KYTY_HOST_WRITE_TRACKING)

Commit: [`b41a2eee`](https://github.com/Jetsku/KytyPS5-experimental/commit/b41a2eeed73724c7ec4a8f34c20dc90d94d3aed1) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> LOD-statistics reports and occlusion results are written into guest memory with TryWriteBacking.
> That write goes through the backing view, so it never faults and the memory tracker never sees
> it. When the destination page is tracked as clean, a buffer or image that already holds a copy
> of that page keeps the old bytes. The next GPU read of the range then gets the stale report
> instead of the new one.
>
> Tracker-gap detector A (NoteHostBackingWrite) shows that this happens in game:
> - "lands on 0 GPU-dirty and 1-2 clean tracked page(s)", 16 lines per launch;
> - U54 HostBackingWriteCleanPages: 0.1/flip in Sky Garden, 1.7-2.3/flip in the desert.
>
> RenderContext::PrepareHostBackingWrite now runs right before each such write (three sites in
> lodStats.cpp, one in occlusion.cpp). It acts only when all of these hold:
> - the calling thread is the GPU thread;
> - the range is mapped;
> - the range has clean tracked pages;
> - it has no GPU-dirty buffer pages and no GPU-modified image bytes.
>
> In that case it calls InvalidateMemory, the same transition a guest write fault makes: the pages
> become CPU-dirty and writable, pending fills are forgotten, and images over the range are
> invalidated. The next GPU use of the range uploads the new bytes, as after a guest store.
>
> Every other case is only reported, as before:
> - GPU-owned bytes: a guest write would first download them, which a write made from a
>   completion cannot do.
> - Nothing tracked as clean.
> - Another thread, for example an occlusion publish on the priority thread. Only the GPU thread
>   marks pages GPU-dirty, so the ownership check cannot go stale before the invalidation.
>
> Env switch and counter:
> - KYTY_HOST_WRITE_TRACKING, default on; =0/false/off restores report-only.
> - FrameEvent HostBackingWritesTracked, appended. Tracked writes still count HostBackingWrites
>   and HostBackingWriteCleanPages.
>
> Cost: 1-2 page invalidations and re-uploads per flip in the desert, none in Sky Garden.
>
> Tests:
> - CheckTrackerGapDetectors, on the GPU thread: re-uploads a clean range, then prepares and
>   performs a host write.
>   - With tracking on, the clean page becomes CPU-dirty and the next binding uploads it.
>   - A GPU-dirty page is left unchanged (report only).
> - ctest tracker_gap_host_write_tracking_off (KYTY_HOST_WRITE_TRACKING=0).
> - Full suite: 93/93, excluding shader_cfg and kernel_file_system; the latter has a
>   pre-existing SDL_main link failure.
>
> Visual checks:
> - Desert: LOD transitions and texture streaming (mip pop-in) as before or better, with no
>   flicker of objects that the occlusion results cull.
> - Sky Garden: unchanged.
> - In Tracy, HostBackingWritesTracked should follow HostBackingWriteCleanPages, and the
>   detector-A log lines for LOD statistics should stop.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/lodStats.cpp`, `src/graphics/host_gpu/renderer/occlusion.cpp`, `src/graphics/host_gpu/renderer/renderContext.cpp`, `src/graphics/host_gpu/renderer/renderContext.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 275. Buffer bindings: reuse clean read bindings across sync epochs, skip written syncs of GPU-owned ranges

Commit: [`5b4a73c8`](https://github.com/Jetsku/KytyPS5-experimental/commit/5b4a73c861b57583c52da0e5617a9762826e1081) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Item 3 of the U54 ranking: buffer and BDA synchronization. Traced at the start view in U54:
> RebindBuffers 3.97 ms/flip, including ObtainBuffer 2.38. The start view also had these counts
> per flip: 21.0K binding-memo records (misses), 19.8K cached hits, 14.3K stream hits,
> 3,055 sync-epoch advances (about one every 1.6 draws) and about 950 writable bindings.
>
> KYTY_BINDING_MEMO_CROSS_EPOCH (default on; =0 off)
> - Change: a cache-buffer read-binding memo from an earlier sync epoch is reused when:
>   - its range signature and buffer structure still hold;
>   - under the region locks, no page of the range is CPU-dirty (normal or hot);
>   - the signature is unchanged after that check.
>   The memo then moves to the current epoch.
> - Why this is exact:
>   - With no CPU-dirty page, every guest write to the range faults first. A fault is a tracker
>     transition, which changes the signature.
>   - Given the same tracker bits and buffers, the normal path would decide no stream copy, find
>     the same buffer and offset, and upload nothing.
>   - Within an epoch, the epoch only covers writes that do not fault. Those land on CPU-dirty
>     pages, and such pages are excluded here.
>   - Emulator writes that bypass the tracker are invisible to the normal path as well
>     (b41a2eee tracks the LOD-statistics and occlusion ones).
> - Stream memos stay within their epoch, because their bytes can change without a fault.
> - The relaxed mirror rejects CPU-dirty ranges without a lock. The locked query also waits for a
>   transition that has advanced a serial but not yet changed its bits.
> - Verify: KYTY_BINDING_EPOCH_MEMO_VERIFY also checks cross-epoch hits. A cross-epoch hit that
>   finds CPU-dirty pages while the signature is still unchanged counts as a mismatch, not a race.
> - Counters: BindingEpochMemoCrossHits and CrossRejects, and why lookups missed
>   (BindingEpochMemoMissSlot, Signature, Guard, Epoch). U55+ data on these tells how much of the
>   21K misses per flip were epoch-only.
>
> KYTY_WRITTEN_SYNC_SKIP (default on; =0 off; KYTY_WRITTEN_SYNC_SKIP_VERIFY=1|exit)
> - Change: a written SynchronizeBuffer (GPU thread, not a BDA pass) returns at once when every
>   page of the range is GPU-dirty and none is readback-pending. The check is
>   MemoryTracker::IsRangeGpuOwned, under the region locks.
> - Why this is exact:
>   - The written upload would collect nothing (GPU-dirty pages are neither CPU-dirty nor hot).
>   - It would set bits that are already set, with no pending mark to cancel, so no serial or
>     protection changes.
>   - Only the GPU thread sets GPU-dirty bits and readback marks. Other threads clear GPU-dirty
>     bits only on marked pages (readback completion). A CPU access to a GPU-dirty page waits for
>     the GPU thread. So the answer holds until this thread acts.
> - This is the common case of a writable binding that is written again before any CPU access
>   (about 950 per flip at the start view).
> - Verify mode runs the normal path anyway, and a range from which it collects anything is a
>   mismatch.
> - Counters: WrittenSyncSkips, WrittenSyncSkipVerifyChecks, WrittenSyncSkipVerifyMismatches.
>
> Exact container fast paths
> - RangeSet::Add returns early when one range already covers the new one. The merge would erase
>   that range and insert it unchanged, because ranges never touch.
> - WriteTickMap::Assign returns early when one entry with the same tick covers the range. The
>   split pieces would coalesce back into that entry, because equal-tick neighbours are always
>   merged.
> - ObtainBufferNow and ObtainWrittenBuffer call m_gpu_modified_ranges.Add only when the range is
>   not already contained. That Add would change nothing and costs a std::map erase and emplace.
>
> Tests
> - MemoryTrackerTests:
>   - randomized RangeSet and WriteTickMap models, which check exact coverage and the canonical
>     form the early returns rely on;
>   - covered and newer-tick assignment cases;
>   - TestRangeGpuOwned: every page required, readback marks, re-owning, downloads, and a written
>     upload of an owned range collecting nothing with an unchanged signature.
> - CheckBindingEpochMemo gains a cross-epoch section:
>   - a clean range is reused across an epoch;
>   - a write fault ends the reuse;
>   - a hot page written without a fault is rejected and re-uploaded, with the buffer bytes
>     checked.
>   New ctest: binding_epoch_memo_cross_off.
> - CheckWrittenSyncSkip:
>   - owned ranges and sub-ranges skip, including an ObtainWrittenBuffer written range;
>   - a CPU write takes a page back, and the next binding uploads it (bytes checked).
>   New ctests: written_sync_skip, written_sync_skip_verify (exit) and written_sync_skip_off.
> - CheckBufferRangeMemo builds its cache with KYTY_BINDING_MEMO_CROSS_EPOCH=0. Each of its
>   bindings is a separate GPU-thread command, hence a separate epoch, so the binding memo now
>   answers the repeats before the range memo and the relaxed queries it checks.
> - Full suite: 97/97, excluding shader_cfg and kernel_file_system (the pre-existing SDL_main link
>   failure).
>
> Visual checks: none expected to change. Check the start view, fly-in, desert run and slide for
> flicker or stale geometry and constants, especially objects whose constant buffers the CPU
> updates each frame and GPU-written (compute) buffers read by later draws.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/memoryTracker.cpp`, `src/graphics/host_gpu/memoryTracker.h`, `src/graphics/host_gpu/rangeSet.h`, `src/graphics/host_gpu/regionManager.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/writeTickMap.h`, `tests/MemoryTrackerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 276. Merge claude/cpu-opt: BDA dirty log, host-write tracking, cross-epoch binding memos, written-sync skip

Commit: [`ca3dbdea`](https://github.com/Jetsku/KytyPS5-experimental/commit/ca3dbdeaad5c28a07493565b709920e099857bea) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_BDA_DIRTY_LOG (default on, with KYTY_BDA_INCREMENTAL_SYNC=1; 0
>   restores the full scans; KYTY_BDA_DIRTY_LOG_VERIFY=1|exit): a BDA pass
>   whose buffers are unchanged synchronizes only the ranges the tracker
>   logged as possibly CPU-dirtied since the last pass, not every buffer.
> - KYTY_HOST_WRITE_TRACKING (default on; 0/false/off reports only): LOD
>   statistics and occlusion results written through the backing view first
>   give their clean tracked pages the guest write-fault transition, so
>   buffers and images holding those pages re-upload the new bytes.
> - KYTY_BINDING_MEMO_CROSS_EPOCH (default on; 0 off): a cache-buffer read
>   binding memo is reused across sync epochs when no page of the range is
>   CPU-dirty and its signature holds (KYTY_BINDING_EPOCH_MEMO_VERIFY checks).
> - KYTY_WRITTEN_SYNC_SKIP (default on; 0 off; KYTY_WRITTEN_SYNC_SKIP_VERIFY=
>   1|exit): a written synchronization of an entirely GPU-owned range
>   returns at once (about 950 per flip at the start view).
> - Exact early returns in RangeSet::Add, WriteTickMap::Assign and the
>   GPU-modified range bookkeeping.
>
> Conflicts: profiler.h/.cpp keep U55's and sched-contention's FrameEvents
> first, then BdaSyncLog*, HostBackingWritesTracked, BindingEpochMemo* and
> WrittenSyncSkip* in their order; CMakeLists keeps U55's mesh-indirect,
> sampler-memo and set-reuse tests and adds tracker_gap_host_write_tracking_off
> next to tracker_gap_detectors.

</details>

### 277. Draw prep: build the log check's certificate ranges on the preparing thread

Commit: [`d52bc9db`](https://github.com/Jetsku/KytyPS5-experimental/commit/d52bc9db692905c30054375acd9360f09c41050b) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Validate sorted and merged the read set's byte and digest ranges on the
> command processor for every committed draw (CertificateRanges, 0.41 ms per
> flip inclusive at the Sky Garden start, U55). The list is a pure function of
> the finished read set, so the preparing thread now builds it once after
> ReadSet::Finish (ReadSet::BuildCertificate) and Validate hands it to
> Coherence::Log::Check. AllClean keeps the separate lists: merging touching
> ranges across 4 KiB boundaries is only valid for the log check.
>
> KYTY_DRAW_PREP_CERT_RANGES=worker (default) | commit (the previous path).
> KYTY_DRAW_PREP_CERT_RANGES_VERIFY=1|exit rebuilds the list at commit with
> the commit-time function and compares (DrawPrepCertRangesVerifyChecks and
> DrawPrepCertRangesVerifyMismatches events; on a difference the commit-time
> list is used).
>
> Tests: ReadSet certificate unit test (fixed cases, 2000 random read sets
> against the reference merge, sortedness and byte coverage); the draw-prep
> engine test counts prebuilt lists and verify checks; new ctest variants for
> parallel, inline, value-certificate log audit and =commit, and the whole
> compute/renderer suite under the verify mode with exit.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `src/graphics/host_gpu/renderer/drawPrep/readSet.h`, `tests/DrawPrepTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 278. CP: split packet parsing and execution into ops (P3a), inline op ring and reference-front verify

Commit: [`0c1ac549`](https://github.com/Jetsku/KytyPS5-experimental/commit/0c1ac549b97b7d4a54e1a8d67acada0f35f8999c) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The command processor is split into a front and a back. The front is the PM4 parse and the
> register state the packet handlers write. The back executes every effect: draws, dispatches,
> end-of-pipe and RELEASE_MEM writes, events, data writes, DMA, LOD statistics, flips, waits,
> predication, COND_EXEC and branches. Front methods build an op (cpOps.h: kind, payload with every
> front value the effect needs, inline data) and hand it to Submit(). ExecuteOp()/Exec*() are the
> direct path's method bodies, reading the payload instead of front state. Handlers that touched
> guest memory themselves now go through the processor: COND_EXEC, conditional INDIRECT_BUFFER,
> register-indirect pairs, GET_LOD_STATS, and RELEASE_MEM as a whole.
>
> KYTY_CP_SEQ=0 (default): ops are executed directly, where the handler asks for them. This is the
> same code path and order as before.
> KYTY_CP_SEQ=inline: every op is encoded into an SPSC op ring (OpStream over the CP recorder's
> CommandStream::Ring) and decoded and executed at once on the same thread. SET_NUM_INSTANCES stays
> front state. Draws without an instance count take it from the front while it is known and
> inherit the back's count after an indirect draw, which applies a newer SET_NUM_INSTANCES first.
> This is the op format the sequencer thread (P3b) will use.
> KYTY_CP_SEQ_VERIFY=1|exit (with inline): a reference front, a second CommandProcessor in
> reference mode, parses the same streams (cpVerify.h). Before each op it runs to its own next op.
> The two are compared by kind, packet position, a running hash of every parsed packet dword,
> payload and inline data. Register-pair reads are compared as ReadCheck ops against the
> reference's serial read. Lockstep results (waits, predication, COND_EXEC, branches) are handed to
> the reference after execution. Constant-engine submissions are not compared. Events:
> CpSeqOps, CpSeqVerifyChecks, CpSeqVerifyMismatches, CpSeqVerifyReadDivergences.
> KYTY_CP_SEQ_RING_KB (default 512): the op ring.
>
> Tests: cp_sequencer unit tests (every op kind through the ring, wrap-around with records up to
> the largest WRITE_DATA, a producer/consumer thread pair). CheckCpSeqOps (WRITE_DATA, register
> pairs, COND_EXEC both ways, constant RAM dump, predication, a branch in a called buffer, a wait
> that suspends and resumes; in verify count mode a front-state difference must be detected).
> New ctest variants: the PM4/lane checks direct, inline, inline+verify and verify count; the
> whole compute/renderer suite inline, inline+verify with draw-prep verify, and inline+verify with
> the CP recorder and its verify; draw prep parallel and inline; EOP timestamps GPU and deferred.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/command_processor/cpOps.cpp`, `src/graphics/guest_gpu/command_processor/cpOps.h`, `src/graphics/guest_gpu/command_processor/cpVerify.cpp`, `src/graphics/guest_gpu/command_processor/cpVerify.h`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/guest_gpu/graphicsRun.cpp`, `tests/CpSequencerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 279. CP: graphics front on a sequencer thread (P3b, KYTY_CP_SEQ=1)

Commit: [`fad24ee2`](https://github.com/Jetsku/KytyPS5-experimental/commit/fad24ee291933a813d024fdeaba2507947ee3882) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The graphics queue's PM4 parse moves to its own thread, CpSequencer (cpSequencer.h). It parses
> every graphics submission in admission order with the packet handlers and emits its ops
> between StreamBegin and StreamEnd. The GPU thread (the resolver) executes each submission's ops
> in order with the P3a Exec bodies, under today's scheduling: slices, yields, suspended waits
> retried at the ring's head, the frame fence, and service commands between ops. Compute queues
> keep the direct path. Default off.
>
> Ordering points: the sequencer waits for the resolver at
> - lockstep ops (waits, predication, COND_EXEC, branches);
> - guest reads it cannot prove clean (LockstepRead);
> - the start of a submission with a frame fence (NoteStarted);
> - constant-engine submissions, which are handed off (the resolver parses and executes them in
>   Direct mode);
> - command bytes under a pending CP write.
> After each one it re-checks the command bytes of every buffer on its stack.
>
> Sequencer reads: command bytes and register-indirect pairs are read directly only when no
> pending CP-write op covers them and no page was ever GPU-touched (gpuTouchedPages.h: a sticky
> page bitmap set in Coherence::Log::Append for buffer dirty adds, tracker GPU marks, GPU-modified
> images and publications, maintained from process start when KYTY_CP_SEQ=1). Otherwise the
> resolver reads them in order (ReadGuestForCp), and a command buffer read that way is parsed from
> the copy.
>
> Registers:
> - Direct draws with parallel draw prep are published to the draw-prep window by the sequencer
>   (Engine::Publish, atomic window head). The resolver commits each one at its op
>   (CommitPublished, with the submission id and an inherited instance count patched in). The
>   window is no longer drained at fences or slice ends.
> - Other draws, dispatches and indirect draws carry a register snapshot (SnapshotRing,
>   KYTY_CP_SEQ_SNAPSHOTS, default 64) that the resolver binds around the op.
> - Outside those ops the resolver binds its own register files, which nothing reads.
>
> CP writes: with KYTY_CP_SEQ=1 every CPU write of guest memory by a command processor (labels,
> WRITE_DATA, flips, the reference clock, synthetic occlusion, DUMP_CONST_RAM, GDS reads, the LOD
> fallback) appends a CpWrite coherence-log entry, so a preparation spanning it fails its
> certificate.
>
> Waiting: the sequencer spins KYTY_CP_SEQ_SPIN_US (default 200), then parks. The resolver wakes
> a parked sequencer only when the wake is useful: at the executed-op threshold of its wait, 8 ops
> before a lockstep op it waits for (pre-wake), or at an explicit notification. The resolver spins
> KYTY_CP_SEQ_RESOLVER_SPIN_US (default 50) on an empty ring before letting other queues run.
> KYTY_CP_SEQ_IDEAL_CPU places the sequencer. The sequencer traps scheduler use like a draw-prep
> worker. KYTY_CP_REPEAT_TRACE keeps the direct CP.
>
> Verify (KYTY_CP_SEQ_VERIFY with KYTY_CP_SEQ=1): StreamBegin carries the front state at the stream's start;
> the reference front re-parses each stream on the resolver. Different parsed command bytes
> (packet count or hash) now count as read divergences, the guest-race class, like ReadCheck
> differences; "exit" stops at mismatches only.
>
> Also: the draw-prep packet hook (PacketHookActive, 0.13 ms/flip on the U55 CP) is decided once
> per slice instead of per packet, and not run by the sequencer.
>
> Counters for the risks: FrameEvent CpSeqBarriers, CpSeqLockstepReads, CpSeqLockstepBuffers,
> CpSeqDirectReads, CpSeqPendingWriteStops, CpSeqSnapshots, CpSeqWindowFull, CpSeqFrontWaits,
> CpSeqResolverStarved, DrawPrepLogChecks, DrawPrepLogEntries (log walk per check); FrameWait
> CpSeqSequencerWait, CpSeqResolverStarved, DrawPrepCommit (commit time per draw, for the
> cross-core slot reads).
>
> Tests: cp_sequencer (bitmap, snapshot ring); CheckCpSeqGuestGpu (every lockstep kind through
> GuestGpu::Submit, a suspending wait, a constant-engine handoff, 400 back-to-back barriers);
> CheckDrawPrepEngineDraw's stream through a sequencer with parallel, inline and off draw prep
> (published per op, no drain, pixels equal to the serial draws). New ctest variants:
> cp_seq_thread, cp_seq_thread_verify, gpu_command_lane_cp_seq_thread_verify,
> draw_prep_engine_{parallel,parallel_steal,inline,off} under thread mode, the whole compute suite
> under thread mode, with verify and draw-prep verify, and with the CP recorder and its verify, EOP
> timestamps GPU under thread mode, and three park variants (no spin on either side, 4 snapshots,
> and a 2-slot window for the draw test), so the waits park at once instead of spinning.
>
> CheckGpuCommandLane under the sequencer:
> - "borrowed graphics commands" patches a submitted stream after Submit, ordered only by a gate
>   on the GPU thread. With the sequencer that is a race (P3-SEQUENCER.md 2.5, 3(d)), so that form
>   runs in the other modes. A new form, ordered by a CP wait (the patch before the label), runs
>   in every mode.
> - The command-polling case retries its host command for up to 5 s instead of 8 times (with the
>   sequencer the first op can follow many host commands), then ends the loop anyway, so a
>   failure cannot hang the test.
>
> Full suite 190/190 and the validation label 17/17 (shader_cfg and kernel_file_system excluded as
> known); the lane and cp_seq_thread variants 10 times each without a failure. No game run was
> made: the gain is not measured.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/command_processor/cpOps.cpp`, `src/graphics/guest_gpu/command_processor/cpOps.h`, `src/graphics/guest_gpu/command_processor/cpSequencer.cpp`, `src/graphics/guest_gpu/command_processor/cpSequencer.h`, `src/graphics/guest_gpu/command_processor/cpVerify.cpp`, `src/graphics/guest_gpu/command_processor/pm4Handlers.cpp`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/guest_gpu/graphicsRun.h`, `src/graphics/host_gpu/coherenceLog.h`, `src/graphics/host_gpu/gpuTouchedPages.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.h`, `src/graphics/host_gpu/renderer/drawPrep/window.h`, `tests/CpSequencerTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 280. Scheduling: draw-path pops without priority waits, optional CP core reservation (KYTY_CPU_RESERVE), placement samples

Commit: [`291b140c`](https://github.com/Jetsku/KytyPS5-experimental/commit/291b140c66b389e58c2eddd7faa2f59c9ffad136) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DEEP-TRACE-U54 4.1 item 4 and the WaitPriorityOperations finding: the CP waited for the priority
> runner inside every draw/dispatch housekeeping pop, and its SMT sibling was busy 21.7% of the
> time, mostly with guest job threads.
>
> Draw-path pops (KYTY_PENDING_OPS_NOWAIT, default on; =0 waits as before):
> - The five opportunistic pops in renderDraw/renderCompute use CommandScheduler::PopReadyOperations.
>   At the first completed normal operation whose tick still has a priority operation queued or
>   running, it stops and leaves that operation and every later one queued, in order. Nothing runs
>   early or out of order; housekeeping is only postponed to a later pop. (Normal operations must run
>   after their tick's priority operations, 54547c86; new priority operations carry the recording
>   tick, later than any completed one, so the check cannot go stale.)
> - Every blocking pop still waits and drains fully: Finish (BufferWait, FlushAndWait), the fault
>   manager and explicit WaitPriorityOperations callers use PopPendingOperations, unchanged.
> - Guest-visible normal operations can land one or more pops later than before (usually the next
>   draw): a non-proxy occlusion publish, the LOD report completion rewrite. Occlusion proxies still
>   reach their publication through BufferWait, a blocking pop. Before, such an operation already
>   waited for the next pop whenever its tick had not completed at the last one.
> - FrameEvents PendingOpsDeferred and PendingOpsDeferredDepth (queue depth summed over deferrals);
>   hang-trace summary.csv pending_ops_max (the deepest queue left per second), so unbounded growth
>   would show.
>
> KYTY_PRIORITY_WAIT_SPIN_US (default 0): WaitPriorityOperations spins that long on the runner's
> progress counter (bumped under the lock after each operation) before blocking. FrameEvents
> PriorityWaitSpins, PriorityWaitSpinHits. Off: no measurement on this machine shows a gain yet.
>
> CPU placement (common/cpuPlacement.h; KYTY_CPU_RESERVE=off (default) | cp | cp+recorder):
> - Windows CPU sets. cp: the CP thread gets one physical core, both logical processors
>   (SetThreadSelectedCpuSets); the process default CPU sets keep every other thread off it: guest
>   threads, draw-prep workers, service and completion threads, the recorder, driver threads.
>   cp+recorder: the CP recorder (KYTY_CP_RECORDER=1) gets a second core the same way
>   (CommandRecorder::Run calls PlaceCurrentThread(Recorder); in cp mode it joins the general pool).
> - The allowed processors are read at runtime: the process affinity mask (Process Lasso's 0-15
>   rule for the game) and any external process default CPU sets. Only whole cores are reserved, in
>   the last-level cache holding most allowed processors, by CPPC SchedulingClass, the core of
>   logical processor 0 last. On this machine: CP on logical 2-3 (preference 7), the recorder on
>   10-11; KYTY_CPU_RESERVE_CORE=<logical processor> picks the CP's core. Nothing is reserved
>   (logged) when fewer than 6 processors would remain.
> - A monitor thread re-applies the layout when the process affinity mask changes (Lasso applies it
>   after start), checking once a second.
> - Hard affinities: Kyty sets none (guest pthread affinities are only stored; no
>   SetThreadAffinityMask anywhere). CPU sets never override a hard affinity, so the monitor scans
>   the process's threads every 5 s (drivers, overlays): one that meets the general processors is
>   narrowed to them by the CPU sets (tested), one confined to reserved cores still runs there.
>   KYTY_CPU_RESERVE_REPIN=1 gives those the general processors (GeneralPoolAffinity); by default
>   they are counted and logged.
> - The monitor exists only with a reservation or placement samples; KYTY_CPU_RESERVE=off with
>   samples off changes nothing.
>
> Placement samples (with the profiler's aggregates, the hang trace, or
> KYTY_CPU_PLACEMENT_SAMPLES=1): the CP every 256th PM4 packet, the recorder every 256th batch, guest
> threads every 64th mutex lock and 4096th yield, draw-prep workers every 64th slot (their worker
> lambda only; the CPU agent was told), the priority runner and the submission worker every 16th
> operation, the present thread every 8th vblank.
> - FrameEvents (appended): CpuPlacementCpSamples, CpuPlacementCpOffCore,
>   CpuPlacementGuestSamples, CpuPlacementGuestOnCpCore, CpuPlacementHostSamples,
>   CpuPlacementHostOnCpCore (on the physical core of the CP's latest sample: SMT sharing),
>   CpuPlacementRecorderOffCore, CpuPlacementHardAffinity, CpuPlacementHardAffinityReserved,
>   CpuPlacementRepinned (each thread counted once).
> - Hang trace placement.csv: per second and role (cp, recorder, guest, host), samples, those on the
>   CP's core, the CP's core, and samples per logical processor. timestamps.csv and placement.csv are
>   now flushed every second like the other files.
>
> Measurements (this machine, Ryzen 9 7950X3D, the game's Process Lasso rule 0-15 at HIGH):
> - SMT probe (quiet machine, CPU 2%): a CP-like proxy (hash-map lookups, 10 KB copies, hashing,
>   branchy dispatch) on logical 2 runs 15-20% slower with an integer load on its sibling (logical
>   3) and 11-13% slower with a float load.
> - cpu_placement_tests --bench (children pinned to 0-15, a CP-like thread doing ~300 us chunks after
>   ~100 us blocking waits, bursty guest-like threads and a yield poller; a first 1 s run on a busy
>   machine, 50% CPU from other agents' builds on CCD1): 10 guest threads: no CP gain (noise);
>   24 guest threads: CP chunks/s +26%, chunk time p50 -11%, guest throughput -2%. The pinned
>   4 x 4 s run is in the U57 report.
> - Default off: the reservation costs the other threads two of 16 processors and helps the CP only
>   when its sibling is busy; the morning A/B (KYTY_CPU_RESERVE=cp) decides.
>
> Tests:
> - cpu_placement (CPU): layouts on this machine's CPU set table (Lasso mask, full mask, cp and
>   cp+recorder, forced and partial cores, too few processors, two caches, core 0 last, group 0
>   only), GeneralPoolAffinity, ParseCpuReserveMode. Live: the CP thread only on its core, the
>   recorder in the general pool, a thread created before the layout and a full load (one busy
>   thread per processor) never on the reserved core; placement histogram and counters agree; a hard
>   affinity on a CP processor runs there, is found and moved; one that also allows a general
>   processor is narrowed to it; a Lasso-like mask change moves the layout.
> - cpu_placement_recorder: the same in cp+recorder mode (the recorder only on its core).
> - command_scheduler_timeline (and its cp_recorder variants) runs SchedulerReadyOperations: with a
>   priority operation holding the runner and two completed ticks' normal operations queued, a ready
>   pop runs nothing and returns at once (counted, with the depth); the runner wait returns only
>   after the priority operation (spinning first, counted, only with KYTY_PRIORITY_WAIT_SPIN_US);
>   Finish then runs both normal operations in order; with the runner idle a ready pop runs at once.
>   command_scheduler_ready_ops_wait (KYTY_PENDING_OPS_NOWAIT=0) and command_scheduler_priority_spin
>   (KYTY_PRIORITY_WAIT_SPIN_US=500) cover the switches. The test turns the per-thread counter sink
>   on only around calls that submit nothing: a ScopedFrameWait under that sink calls
>   tracy::GetProfiler() (TRACY_ON_DEMAND), which crashes in a process without a running profiler
>   (not reachable in the emulator, where the sink is on only while a profiler is connected).
> Full suite 163/163 and the validation label 17/17 (shader_cfg and kernel_file_system excluded as
> known). claude/u56 (ca3dbdea) was fast-forwarded into this branch first.

</details>

Changed files: `CMakeLists.txt`, `src/common/cpuPlacement.cpp`, `src/common/cpuPlacement.h`, `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/common/threads.cpp`, `src/graphics/guest_gpu/command_processor/commandProcessor.h`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/queueSubmission.cpp`, `src/graphics/host_gpu/renderer/commandRecorder.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/presentation/videoOut.cpp`, `src/kernel/pthread.cpp`, `tests/CpuPlacementTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 281. EOP timestamps: query writes and slot resets as CP recorder packets (no recorder drains)

Commit: [`8a9097db`](https://github.com/Jetsku/KytyPS5-experimental/commit/8a9097dba1e6071e34b8f5881af00930431a563b) · **Command processing, scheduling and completion**

Changes the named producer/consumer or completion boundary to reduce serial work, wakeups or starvation while retaining guest ordering. Accuracy fixes may add required work; no isolated gain is assigned.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> With KYTY_CP_RECORDER and KYTY_EOP_TIMESTAMPS=gpu both on, every end-of-pipe clock write (about
> 880 per frame) recorded its timestamp query through StateHandle(), and every command-buffer begin
> reset the consumed slots through it (9042db36). Each such call drained the recorder and opened a
> direct window.
>
> - New recorder packet WriteTimestamp2 (vkCmdWriteTimestamp2: stage, pool, query): encoder,
>   replay, verify hash (KYTY_CP_RECORDER_VERIFY), the recorder's executor, and
>   CommandSink::writeTimestamp2.
> - EopTimestampRing::BeginCommand and RecordQuery take a CommandSink; the CP passes StateSink().
>   With the recorder, the slot resets are ResetQueryPool packets after the recorder's Begin packet
>   and the query writes are WriteTimestamp2 packets, both in stream order; without it they are
>   recorded natively, as before. StateSink keeps StateHandle's semantics otherwise: batched
>   barriers stay pending and no rendering instance is split.
>
> Tests:
> - cp_recorder (CPU): WriteTimestamp2 round-trips through encode, replay and verify with every
>   other packet.
> - CheckEopTimestamps: the phase-1 clock writes must not drain the recorder. Negative check: with
>   the old StateHandle() call restored, eop_timestamps_gpu_cp_recorder and _inline fail with "clock
>   writes drained the CP recorder 1 times".
> - New ctest variants eop_timestamps_gpu_verify_cp_recorder and _inline (gpu-verify: published
>   values against record time and order) next to eop_timestamps_gpu_cp_recorder and _inline.
> Full suite 165/165 and the validation label 17/17 (shader_cfg and kernel_file_system excluded as
> known).

</details>

Changed files: `CMakeLists.txt`, `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/host_gpu/renderer/commandRecorder.cpp`, `src/graphics/host_gpu/renderer/commandScheduler.cpp`, `src/graphics/host_gpu/renderer/commandStream.cpp`, `src/graphics/host_gpu/renderer/commandStream.h`, `src/graphics/host_gpu/renderer/commandStreamReplay.inl`, `src/graphics/host_gpu/renderer/eopTimestamps.cpp`, `src/graphics/host_gpu/renderer/eopTimestamps.h`, `src/graphics/host_gpu/renderer/render.h`, `tests/CpRecorderTests.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 282. Merge claude/sched-contention: draw-path pops without priority waits, CPU placement, EOP timestamps as recorder packets

Commit: [`b741e7ef`](https://github.com/Jetsku/KytyPS5-experimental/commit/b741e7ef37197c27716b14d6666243c26f8ba6d1) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_PENDING_OPS_NOWAIT (default on; 0 waits as before): the draw and
>   dispatch housekeeping pops stop at the first completed operation whose
>   tick still has a priority operation queued or running, instead of waiting
>   for the priority runner; blocking pops still wait and drain fully.
> - KYTY_PRIORITY_WAIT_SPIN_US (default 0): WaitPriorityOperations spins on
>   the runner's progress before blocking.
> - KYTY_CPU_RESERVE=off (default) | cp | cp+recorder: Windows CPU sets give
>   the CP thread (and the recorder) a whole physical core
>   (KYTY_CPU_RESERVE_CORE, KYTY_CPU_RESERVE_REPIN). Placement samples
>   (profiler aggregates, hang trace or KYTY_CPU_PLACEMENT_SAMPLES=1) and the
>   hang-trace placement.csv.
> - KYTY_EOP_TIMESTAMPS=gpu with KYTY_CP_RECORDER: timestamp writes and slot
>   resets are WriteTimestamp2/ResetQueryPool recorder packets through
>   StateSink(), in both BeginCommand paths; this replaces U56's StateHandle()
>   direct window, so neither drains the recorder any more.
>
> No conflicts: the branch already contained U56 (ca3dbdea).

</details>

### 283. Write faults: release a GPU-owned page shared with CPU bytes without draining (KYTY_FALSE_SHARING_WRITES, default off)

Commit: [`9e37bfff`](https://github.com/Jetsku/KytyPS5-experimental/commit/9e37bfff6acf2a1425e5b113b05270899ecf537c) · **False-sharing diagnostic**

Adds an off-by-default shortcut for pages sharing CPU and GPU bytes. Historical notes explicitly do not establish exactness; it is not an endorsed optimization.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RT-GI's analysis of R1 against U54 (GI on) found about 1.1 ms/flip of render-thread stall from
> page-level false sharing:
> - The ray-bundle G-buffer is a 12 MiB writable binding at [0x55b408b20, 0x55c008b20).
> - The per-frame block that the GI code writes (eboot 0x70d5193) starts right after it, on the
>   shared tracker page 0x55c008000.
> - Every bundle draw makes that page GPU-owned. The next CPU write to the block faults and drains
>   the GPU: submit, wait, a 48 KiB download, then the page is released. That happens 1.24 times
>   per flip at the start view, median 0.87 ms.
> - The GPU never writes those CPU bytes. m_gpu_modified_ranges is byte-exact; protection and dirty
>   tracking are per page.
>
> KYTY_FALSE_SHARING_WRITES=1 (default off): BufferCache::TryFalseSharingWrite, on the GPU thread
> in the write-fault drain path.
> - Conditions: the written bytes are not GPU-owned, no image lies over the page, the page is
>   inside one registered buffer, and no address-writing (unbounded) shader is still running.
> - The page's GPU-owned bytes get the usual download (DownloadBufferMemory, recorded behind every
>   writer, with a backing publication). The page is released at once: GPU ownership cleared, CPU
>   bytes dirty and writable. There is no submit and no wait.
> - An early flush is requested (within the eager-flush budget), so the publication lands soon.
> - Until it lands:
>   - uploads of the page skip those bytes, and the buffer keeps the GPU's copy. This covers
>     SynchronizeBuffer runs, the late copies of written uploads, and hot pages, which are demoted
>     and upload only the other bytes of their snapshot;
>   - HasGpuDirtyBytes reports them, so new images over them refresh from the buffer and CP reads
>     wait for the publication;
>   - the pending publication still keeps side readbacks there on the draining path.
> - Everything else takes the drain as before: true sharing, unpublished bytes, images,
>   unbounded writers.
>
> Not exact, hence default off until a GI-on run checks it. Before the publication lands:
> - a guest CPU read of the page's GPU-owned bytes sees their old guest contents;
> - a guest CPU write to those bytes is overwritten by the publication.
> The drain made both see the GPU's bytes.
> KYTY_FALSE_SHARING_WRITES_VERIFY=1|exit compares those bytes at publication with their contents
> at the release. A difference is a CPU write that the publication overwrites
> (FalseSharingVerifyConflicts; exit stops on the first). Reads cannot be observed.
>
> Counters: FalseSharingWrites, FalseSharingBytes, FalseSharingUploadSplits,
> FalseSharingVerifyChecks, FalseSharingVerifyConflicts.
>
> Tests:
> - CheckFalseSharingWrites: one page holds a GPU-filled binding tail and CPU bytes.
>   - Round 1 checks the release without a drain (no submit), the pending publication (old guest
>     bytes, reported GPU-dirty), the upload that leaves them out, the publication, and the buffer
>     bytes.
>   - Round 2 writes a GPU-owned byte before the publication. With the switch on, the publication
>     overwrites it and the verify mode counts one conflict. With the switch off, the CPU write
>     stands.
>   - ctests false_sharing_writes_off, false_sharing_writes and false_sharing_writes_verify.
> - Full suite: 100/100, excluding shader_cfg and kernel_file_system. With
>   KYTY_FALSE_SHARING_WRITES=1 and _VERIFY=1 set for every test: 99/99 (the off test excluded).
>
> GI-on run (A1 on R3):
> - FalseSharingWrites should be about 1.2 per flip.
> - FalseSharingVerifyConflicts should be 0; any conflict means the switch must stay off.
> - The render-thread ReadMemory and GpuWaitDrain waits should drop by about 1 ms per flip.
> Visual checks: GI lighting and the GI geometry passes unchanged, no flicker or stale matrices.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 284. Shader-data uploads: check only the site's last upload, drop the hashed content table (KYTY_UPLOAD_DEDUP_TABLE)

Commit: [`77e60f95`](https://github.com/Jetsku/KytyPS5-experimental/commit/77e60f95e681a9dbd346b706a651186c40e8304e) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> UploadShaderData (per draw stage: the flattened SRT and the shader-data table) cost 0.85 ms/flip
> at the U54 Sky Garden start view, traced: about 10.4K calls per flip.
>
> Each call that did not match its site's last upload (about 9,200 per flip) hashed its words with
> XXH3 and looked them up in a 256-entry content table. The U54 captures show the table almost
> never matches:
> - Sky Garden start: ShaderUploadReuseHits 1,226 per flip, of which ShaderUploadLastHits 1,212,
>   so the table found 14 of about 9,200 lookups.
> - Desert run: 32.7 reuses per flip, 32.1 of them last-upload hits, so the table found 0.6 of
>   about 900 lookups.
>
> Now each upload site keeps its own last upload (m_upload_site_last). An upload equal to it in the
> same tick reuses its allocation, and any other upload goes straight to the stream ring: no hash
> and no table. Sites no longer evict each other's last entry through shared table slots.
> - KYTY_UPLOAD_DEDUP_TABLE=1 restores the hashed table (the U54 behaviour).
> - KYTY_UPLOAD_DEDUP=0 still selects the older per-draw memo.
> - Counters are unchanged: ShaderUploadLastHits, ReuseHits, ReuseMisses and BytesAvoided.
>
> Exactness: a reused allocation holds equal bytes from the same tick (the ring never overwrites
> an allocation during its tick), so only which equal copy a descriptor points at can change. The
> 14 per flip that the table would still have found now get their own allocation.
>
> Expected: about 0.15-0.2 ms/flip at the start view (the hash and table bookkeeping of about 9K
> uploads), less where there are fewer draws.
>
> Tests:
> - CheckShaderUploadDedup: every allocation holds its bytes, a site's repeat is reused, and
>   different bytes never share an allocation. Without the table, an equal upload of another site
>   or an older position is not reused; with it, it is. No allocation is reused across ticks.
> - ctests shader_upload_dedup and shader_upload_dedup_table.
> - Full suite: 102/102, excluding shader_cfg and kernel_file_system.
>
> Visual checks: none expected (the same bytes reach every draw).

</details>

Changed files: `CMakeLists.txt`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/render.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 285. GPU buffer writes: skip the texture-cache lock and image walk on pages without images (KYTY_GPU_WRITE_IMAGE_SKIP)

Commit: [`f7cd2492`](https://github.com/Jetsku/KytyPS5-experimental/commit/f7cd24926a66a9bb513a8fe42b97cbb8757381f1) · **Transfer and readback work**

Targets the named copy, upload or synchronization cost. Reusing current bytes, batching or overlapping transfers can avoid waits; fallbacks and ownership checks remain necessary.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every writable binding runs two image checks: BufferCache::PreserveImagesForGpuWrite and
> TextureCache::InvalidateMemoryFromGPU. That is about 950 per flip at the U54 Sky Garden start
> view. Each check takes the texture-cache lock and walks the image owner lists of the write's
> 1 MiB pages, and usually finds nothing.
>
> Now both first ask TextureCache::NoImagesOnPages, which reads the per-page registered-image counts
> without the lock. When no image is registered on any of the write's pages, they return at once.
> The walk would have found no image to act on.
>
> Exact under concurrency:
> - RegisterImage now counts a page (seq_cst fetch_add) before the image enters that page's owner
>   list. UnregisterImage already uncounted it after removal. So an image that FindImagesInRegion
>   can find always has its pages counted.
> - A zero count at a page's load means no image was findable on that page then. An image
>   registered after that load belongs after this check in any serialization of the two, as it
>   would if it had taken the lock after the walk.
> - The fault fast path's requirement is unchanged: pages are counted before the image watches
>   them.
>
> KYTY_GPU_WRITE_IMAGE_SKIP=0 restores the lock and walk.
> KYTY_GPU_WRITE_IMAGE_SKIP_VERIFY=1|exit takes the lock after each skip decision and runs the walk
> anyway:
> - an image found over pages still uncounted under the lock is a mismatch (exit stops on the
>   first);
> - an image over pages counted by then was registered after the decision, which is a race.
> FrameEvents: GpuWriteImageSkips and GpuWriteImageSkipVerify{Checks,Races,Mismatches}.
>
> Expected: about 0.1-0.2 ms/flip at the start view (two lock round trips and walks per writable
> binding).
>
> Tests:
> - CheckGpuWriteImageSkip:
>   - pages without images skip;
>   - an ownership-only image is counted on its pages only, and a write over it does not skip;
>   - a thread toggles the image's registration 20,000 times while another checks and writes.
>     Under the lock, counts must equal the owner lists at every sample. In a verify-mode run:
>     165 racing skips, 18 races, 0 mismatches;
>   - a deleted image's pages are uncounted.
> - ctests gpu_write_image_skip, gpu_write_image_skip_verify (exit) and gpu_write_image_skip_off.
> - Full suite: 105/105, excluding shader_cfg and kernel_file_system. With
>   KYTY_GPU_WRITE_IMAGE_SKIP_VERIFY=exit set for every test: 104/104 (the off test excluded).
>
> Visual checks: none expected. Watch render targets and storage images that alias buffers
> written by compute: they must still refresh from the buffer after such writes.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 286. Readback drains: count why DCC fallbacks and GPU-thread side copies are refused (instrumentation only)

Commit: [`0e09a6ab`](https://github.com/Jetsku/KytyPS5-experimental/commit/0e09a6abfaaee9b417a0dbc00c360294feab8d11) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Item 5 of the U54 ranking is readback drains on the CP. At the Sky Garden start they are almost
> entirely the DCC CPU fallback: DccCpuFallbacks 2 per flip, all with reason DccFallbackImageState,
> 0.36 ms/flip.
>
> In the U55 hang trace:
> - Each fallback is a GPU-thread ReadMemory of 80 KiB of DCC metadata at 0x570800000 (8,008 in
>   the run, all downloaded, 168 us on average).
> - The metadata's last writer is a compute shader (shader-storage, age 0-1 ms), so it is in the
>   recording being built and nothing but a drain can read it on the CPU.
> - The drain happens because TryIssueSideReadback returns Other (ReadbackSideFallbackOther 2 per
>   flip): the 80 KiB read exceeds the 64 KiB side-copy window.
>
> Nothing recorded which image-state condition refused the GPU inspection. New FrameEvents
> (appended) record it:
> - DccImageState{Unregistered, Stencil, Mismatch, NotGpuModified, BufferModified, CpuDirty,
>   Partial, GpuDirtyBytes}: the cause of each DccFallbackImageState, in the order of the
>   eligibility check. The first four fallbacks of each cause also go to stderr, with the metadata
>   and image ranges, since LOGF may be compiled out.
> - ReadbackSideOther{Owner, Alignment, Window, Publication, NoDirty, Slot}: the cause of each Other
>   result of TryIssueSideReadback.
>
> No behaviour change. These counters decide step 2 of the item: which image state the GPU DCC
> inspection must accept, so the metadata decision stays on the GPU right after its compute writer
> and the CPU readback disappears.
>
> Full suite: 105/105, excluding shader_cfg and kernel_file_system.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`.

### 287. DCC fallback causes: report pending refreshes before "not GPU-modified"

Commit: [`5703a8fa`](https://github.com/Jetsku/KytyPS5-experimental/commit/5703a8fab3bffd8a63597f4203e92c481d2afd72) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A buffer-modified or CPU-dirty image is usually also not GPU-modified. With the cause checked in
> the previous order, DccImageStateNotGpuModified would have hidden the more specific cause that
> step 2 of the readback-drain item needs: an image waiting for a refresh, which the GPU inspection
> could accept by refreshing it first. The order is now registered, stencil, description,
> buffer-modified, CPU-dirty, not GPU-modified, partially resident, GPU-dirty bytes.
>
> Instrumentation only. Full suite: 105/105, excluding shader_cfg and kernel_file_system.

</details>

Changed files: `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`.

### 288. DCC: inspect refresh-pending images on the GPU (KYTY_DCC_GPU_REFRESH, default off)

Commit: [`05696543`](https://github.com/Jetsku/KytyPS5-experimental/commit/056965430528006283c110340cc6d0e2c3a43a6b) · **Compressed metadata and clear semantics**

Handles the named metadata/clear operation without stale contents; eligible GPU work avoids CPU readbacks. Accuracy and lower transfer cost are separate claims; see the water discussion.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Item 5 step 2 (DEEP-TRACE-U54 section 4.1). The Sky Garden readback drains are
> DCC CPU fallbacks whose image waits for a refresh: its memory was written
> through a buffer by the GPU (buffer-modified) or by the CPU, so the native
> inspection (TryMaterializeGpuMetadataClear) refused it and the CPU fallback
> drained the GPU for the metadata.
>
> With KYTY_DCC_GPU_REFRESH=1 (and KYTY_DCC_GPU=1) the native inspection also
> accepts a registered, matching, fully resident DCC image in the
> buffer-modified, CPU-dirty, not-GPU-modified and GPU-dirty-bytes states. It
> refreshes the image first (InitializeImage, as the CPU fallback's ClearImage
> does before a layer clear), then records the inspection and commits it as a
> GPU write. The decision stays on the GPU, behind the metadata's writer.
> Unregistered, stencil, mismatched and partially resident images, and CMASK,
> are still refused. The refresh runs under the texture lock before the
> metadata buffer is acquired, and again only if something dirtied the image
> in between.
>
> KYTY_DCC_GPU_REFRESH_VERIFY=1|exit: for such an image, also take the CPU
> fallback's decision from the drained metadata, copy the inspected slices out
> behind the inspection, and compare when it completes. A slice the fallback
> clears must have its key consumed (all 0xFF); any other slice must be
> unchanged. KYTY_DCC_GPU had no verify mode to extend.
>
> FrameEvents: DccGpuRefreshes (slices), DccGpuRefreshVerify{Checks,Mismatches}.
> The DCC CPU fallback's stderr line now names its image-state cause.
>
> Test dcc_gpu_refresh: a two-layer DCC render target whose metadata a shader
> wrote (layer 0 a clear code, layer 1 not), for a buffer-modified image and a
> CPU-dirty image. With the switch on (verify=exit) the native inspection runs
> after the refresh. With it off the CPU fallback runs. Both leave the same
> texels and metadata bytes. The check gives its own RenderContext the
> device's push-descriptor limit, which DccClearHelper needs and the harness
> otherwise leaves at 0. Full suite 106/106.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 289. Profiler: ScopedFrameWait without a started Tracy profiler

Commit: [`7338e8e4`](https://github.com/Jetsku/KytyPS5-experimental/commit/7338e8e4d407fac616553c679c6d77dea2b880ea) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> With the per-thread counter sink on and no Tracy profiler,
> ScopedFrameWait::Begin and Finish read tracy::GetProfiler().ConnectionId()
> (TRACY_ON_DEMAND). Under TRACY_MANUAL_LIFETIME that dereferences the
> profiler that was never started, and the process faults. Tests set the sink
> directly, and a profiler shutdown can follow the flip that chose the sink.
>
> The connection id is now read only while tracy::ProfilerAvailable(), with 0
> standing for "no profiler" (as for a started profiler that has never been
> connected). Scopes that span an on-demand connection change are still
> omitted. With a profiler present nothing changes.
>
> Test (profiler_counters): with the per-thread sink on and no profiler, a
> ScopedFrameWait on the main thread and on another thread is counted with its
> duration, AddFrameWait is counted, and the shared and off sinks count
> nothing. Without the fix the test process faults (0xC0000005). Full suite
> 106/106.

</details>

Changed files: `src/common/profiler.cpp`, `tests/ProfilerCounterTests.cpp`.

### 290. Merge claude/cpu-opt: false-sharing writes, per-site upload dedup, image-walk skip, DCC refresh on the GPU

Commit: [`a1eb98c1`](https://github.com/Jetsku/KytyPS5-experimental/commit/a1eb98c1adae73843d4d8aa4c25a1cfaffecf65e) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_FALSE_SHARING_WRITES=1 (default off, not exact before the
>   publication lands; _VERIFY=1|exit counts CPU writes the publication
>   overwrites): a write fault on a GPU-owned page shared with CPU bytes
>   releases it without draining the GPU; the GPU-owned bytes reach guest
>   memory through a download publication.
> - Shader-data uploads check only their site's last upload
>   (KYTY_UPLOAD_DEDUP_TABLE=1 restores the hashed content table).
> - KYTY_GPU_WRITE_IMAGE_SKIP (default on; 0 restores the lock and walk;
>   _VERIFY=1|exit): GPU buffer writes skip the texture-cache lock and image
>   walk when no image is registered on their pages.
> - DCC fallback and side-readback refusal causes (DccImageState*,
>   ReadbackSideOther*), instrumentation only.
> - KYTY_DCC_GPU_REFRESH=1 (default off, with KYTY_DCC_GPU=1; _VERIFY=1|exit):
>   the native DCC inspection refreshes buffer-modified or CPU-dirty images
>   first instead of the CPU fallback's drain.
> - ScopedFrameWait reads the Tracy connection id only while a profiler is
>   available.
>
> Conflicts: profiler.h/.cpp keep the earlier FrameEvents first (up to
> sched-contention's PendingOps*, PriorityWait* and CpuPlacement*), then
> FalseSharing*, GpuWriteImageSkip*, DccImageState*, ReadbackSideOther* and
> DccGpuRefresh* in their order; the texture-cache constructor keeps the
> alias-bytes log line and reads the new switches; the compute-test harness
> keeps DenormFlushF32Supported next to MaxPushDescriptors and
> PushDescriptorsScope.

</details>

### 291. Merge claude/p3-sequencer: certificate ranges built on the preparing thread (KYTY_DRAW_PREP_CERT_RANGES)

Commit: [`26f4d9ac`](https://github.com/Jetsku/KytyPS5-experimental/commit/26f4d9acfbd32ca194fd13424ec2c871f5085fb3) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_DRAW_PREP_CERT_RANGES=worker (default) | commit: the coherence-log
>   check's merged certificate ranges are built once by the preparing thread
>   after ReadSet::Finish (ReadSet::BuildCertificate), a pure function of the
>   finished read set, instead of by the CP in Validate for every committed
>   draw (0.41 ms/flip inclusive at the U55 Sky Garden start). Stolen slots
>   (KYTY_DRAW_PREP_STEAL) go through the same Prepare and get the list too.
> - KYTY_DRAW_PREP_CERT_RANGES_VERIFY=1|exit rebuilds the list at commit and
>   compares (DrawPrepCertRangesVerify*); on a difference the commit-time list
>   decides.
>
> Only d52bc9db of claude/p3-sequencer. Conflicts: profiler.h/.cpp keep every
> earlier FrameEvent first, then DrawPrepCertRangesVerifyChecks and
> DrawPrepCertRangesVerifyMismatches.

</details>

### 292. GPU op profiler: pass stamps (KYTY_GPU_OP_PROFILE_STAMPS) for captures that keep the GPU's overlap

Commit: [`de5e5251`](https://github.com/Jetsku/KytyPS5-experimental/commit/de5e5251a3db5efefe960a7ef69affe67610d71e) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The sampled GPU op capture writes an ALL_COMMANDS timestamp after every recorded op. On a Sky
> Garden frame that serializes the GPU: U53's capture of flip 2667 (5,845 draws, 14,870 ops) stamped
> 154 ms of GPU time against about 33 ms of unprofiled GPU busy per flip around it (4.7x). Light
> frames stay near 1x. So the capture could not say how the ~16.7 ms/flip of GPU busy at the start
> view split between the game's rendering and the emulator's own work
> (Profiling/analysis/GPU-CEILING-PLAN.md).
>
> KYTY_GPU_OP_PROFILE_STAMPS=ops (default, unchanged) | passes | alternate:
> - passes: timestamps only before a render pass begins and after it ends (the pass is timed whole:
>   load/clear, draws, store), at each change between guest dispatches and other work outside passes,
>   and before the command buffer ends (new vkEndCommandBuffer hook, capture mode only). Draws and
>   consecutive guest dispatches keep their overlap; the stamps sit where the GPU drains anyway.
> - The boundary stamps are "segment" rows (kind/category segment): site segment.compute closes a
>   run of guest dispatches (the renderer's "dispatch"/"dispatch.indirect" sites), segment.emulator a
>   stretch of other work (copies, fills, clears, barriers, internal dispatches, queries). Every
>   other op has an empty delta_ns: the next stamped row of its command buffer times it.
> - alternate: captures 0, 2, 4, ... per-op, 1, 3, 5, ... pass stamps, so one run gives both.
> - gpuops-summary.csv totals gain pass_stamps and unstamped_ops (unstamped ops of a pass capture are
>   not "unavailable"); gpuops-README.txt and gpuOpProfiler.h describe the mode.
> - Nothing changes without the switch; it only acts during capture frames.
>
> Test: gpu_op_profiler_stamps (shader_recompiler_compute_tests --gpu-op-profile-only; hooks
> installed in the harness, alternate mode, two captured frames of the same scheduler work: fills,
> barriers, a run of three dispatches of an empty compute pipeline, a clear-only render pass). The
> per-op capture stamps all 18 ops; the pass capture stamps 2 pass ends and 8 segment rows (2
> compute, 6 emulator) and leaves the 16 other ops without a delta.
>
> Analysis: Profiling/tools/hangtrace/gpu_ceiling.py (per-flip counters, and per capture the
> game/pass/emulator split, time by render target with begin counts, and the inflation against the
> unprofiled GPU busy of the surrounding seconds).
>
> Full suite 166/166 and the validation label 17/17 (shader_cfg and kernel_file_system excluded as
> known).

</details>

Changed files: `CMakeLists.txt`, `src/graphics/host_gpu/renderer/gpuOpProfiler.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 293. Merge claude/sched-contention: GPU op profiler pass stamps (KYTY_GPU_OP_PROFILE_STAMPS)

Commit: [`b65ceab0`](https://github.com/Jetsku/KytyPS5-experimental/commit/b65ceab04e1b525a7a9357cb069efd08460900ca) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_GPU_OP_PROFILE_STAMPS=ops (default, unchanged) | passes | alternate:
>   with passes, a sampled GPU op capture stamps only render-pass begins and
>   ends, changes between guest dispatches and other work, and the end of the
>   command buffer (segment rows), so draws and consecutive dispatches keep
>   their GPU overlap; alternate interleaves per-op and pass captures. It acts
>   only during capture frames. New ctest gpu_op_profiler_stamps.
>
> No conflicts.

</details>

### 294. Texture cache: skip LRU touches of images already touched in this GC tick (KYTY_IMAGE_LRU_SKIP)

Commit: [`5549047a`](https://github.com/Jetsku/KytyPS5-experimental/commit/5549047ac78e651f4b096f17c38324aebfbfe53c) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> TouchImage runs for every texture binding, view, target and GetImage of a
> draw, about 65k times per flip at the Sky Garden start. It cost 1.12 ms/flip
> traced at U55 (P4B-WORKER-LOOKUPS section 6). Of that, 0.45 was the load of
> the LRU item's tick (lruCache.h:42), 0.11 the deque indexing and 0.07
> Attach/Detach. The GC tick advances once per completed submission, so most
> touches find the item already touched and LeastRecentlyUsedCache::Touch
> returns at once.
>
> Image::lru_tick now mirrors the tick of the image's LRU item. It is copied
> from the item after every Insert (RegisterImage) and Touch (TouchImage), the
> only writers of an item's tick. With KYTY_IMAGE_LRU_SKIP (default on; =0
> off), TouchImage skips the call when the mirror is not older than
> m_gc_tick: Touch's own early return, without reading the item. The LRU order
> and every GC decision stay the same.
>
> KYTY_IMAGE_LRU_SKIP_VERIFY=1|exit checks the mirror against the item (linked,
> equal tick) on every decision for a registered image. A mismatch takes
> today's path (the touch, which repairs the mirror) and is counted and logged;
> exit stops. FrameEvents ImageLruTouchSkips and
> ImageLruVerify{Checks,Mismatches}.
>
> Test image_lru_skip (_verify with =exit, _off with =0): one script of 600
> steps (touches through GetImage, GC ticks, re-registrations, image
> recreations that reuse LRU items) leaves the same LRU order and item ticks
> after every step with the skip on and off. The verify checks every decision.
> Planted mirror corruptions are caught and still touched, with the same order.
> The GC fixture (ConfigureGarbageCollection) sets the mirror with each Insert.
> Full suite 109/109, also with KYTY_IMAGE_LRU_SKIP_VERIFY=exit for every test.

</details>

Changed files: `CMakeLists.txt`, `src/common/lruCache.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `src/graphics/host_gpu/renderer/image/image.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 295. Images: return from no-op layout transitions before GetBarriers (KYTY_IMAGE_TRANSIT_SKIP)

Commit: [`e990b807`](https://github.com/Jetsku/KytyPS5-experimental/commit/e990b807487463177801eb6f49705a29d5604fca) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every draw transitions each bound image in CommitBindings, and its
> attachments in ExecutePreparedDraw. At the Sky Garden start that cost 0.92
> and 0.26 ms/flip traced at U55 (P4B-WORKER-LOOKUPS section 6). Most calls
> change nothing. Even so, each paid for the GPU-op site, the range copy, the
> destination stage computation and the out-of-line GetBarriers call before
> its early return.
>
> Image::TransitIsNoOp is that early return's condition, in GetBarriers' own
> order:
> - no per-subresource states;
> - a range that covers every level and layer (a volume's range counts as one
>   layer, as in GetBarriers);
> - a state that already has the layout and the access;
> - no write access (a repeated write needs a barrier).
> With KYTY_IMAGE_TRANSIT_SKIP (default on; =0 off), Transit returns at once
> when it holds. GetBarriers would have returned no barrier and left the state
> unchanged, so nothing recorded or tracked changes.
>
> KYTY_IMAGE_TRANSIT_SKIP_VERIFY=1|exit runs GetBarriers after such a decision
> and records whatever it returns, as without the skip. A barrier there is a
> mismatch (counted and logged; exit stops). FrameEvents ImageTransitSkips and
> ImageTransitVerify{Checks,Mismatches}.
>
> Test image_transit_skip (_verify with =exit, _off with =0): 13,824
> combinations of initial state, per-subresource states, destination layout
> and access, and full, partial and volume ranges, on a 2-level 2-layer array
> and a 3D image. Wherever the pre-check says no-op (96 cases), GetBarriers
> returns no barrier and changes no state. The common case (whole image, same
> read state) is always taken. Transit skips exactly the repeated read
> transition, never the repeated write, and in verify mode checks it with no
> mismatch. Full suite 112/112, also with KYTY_IMAGE_TRANSIT_SKIP_VERIFY=exit
> for every test.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/image/image.cpp`, `src/graphics/host_gpu/renderer/image/image.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 296. Draw sequence: count why target records end invalid (instrumentation only)

Commit: [`1395d36b`](https://github.com/Jetsku/KytyPS5-experimental/commit/1395d36b1889402ad050bf5b7ade82987842511a) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> About 5,500 target lookups per flip at the Sky Garden start have no provable
> repeat record, so each one is a full FindImage (0.90 ms/flip traced at U55,
> plus 0.22 in the metadata decisions; P4B-WORKER-LOOKUPS section 6, item 4).
> Step 1 counts why, per lookup whose record ends invalid, by its first reason:
> - TargetRecordNotFirstPage: the image did not come from the first-page lookup
>   (overlap resolution or a new image).
> - TargetRecordChanged: the first-page answer changed during the lookup
>   (residency extension, alias sync), or the image is unregistered or a
>   video-out surface.
> - TargetRecordDcc{Clear,Native,Fallback,Guest,Pages}: the DCC decision was
>   not a provable no-op. Clear: a recorded-fill clear was applied. Native and
>   Fallback: the native inspection or the readback decided GPU-owned bytes.
>   Guest: guest bytes decided. Pages: the metadata pages could not be captured.
> - TargetRecordCmask{Native,Other}: the CMASK decision found GPU-owned bytes
>   without a recorded fill, or was otherwise not provable (guest bytes, a
>   clear, fills).
> MaterializeDccClear and MaterializeCmaskClear record their reason in a
> member; FindImage counts it only for records (draw-sequence target lookups).
> Nothing else changes.
>
> Test draw_sequence (and _off, _verify): the guest-CMASK lookup and the lookup
> that applies a new fast clear each count one TargetRecordCmaskOther (none
> with KYTY_DRAW_SEQUENCE_FAST=0). Full suite 112/112.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/textureCache.cpp`, `src/graphics/host_gpu/renderer/cache/textureCache.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 297. Merge claude/cpu-opt: skip LRU touches and no-op layout transitions of images, target-record refusal counters

Commit: [`3f7f4348`](https://github.com/Jetsku/KytyPS5-experimental/commit/3f7f4348dd6f4a4821638003ca14828ff6f5f4e2) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_IMAGE_LRU_SKIP (default on; 0 off; _VERIFY=1|exit): Image::lru_tick
>   mirrors its LRU item's tick (copied after RegisterImage's Insert and
>   TouchImage's Touch, still the only writers of a texture-cache item's tick
>   in this tree), and TouchImage skips the call when the mirror is not older
>   than m_gc_tick: exactly Touch's own early return, without reading the
>   item (about 65k touches per flip at the Sky Garden start, 1.12 ms/flip
>   traced at U55). The LRU order and every GC decision are unchanged.
> - KYTY_IMAGE_TRANSIT_SKIP (default on; 0 off; _VERIFY=1|exit runs
>   GetBarriers after each decision and records what it returns): Transit
>   returns before the GPU-op site and GetBarriers when GetBarriers would
>   return no barrier and change nothing (no per-subresource states, a whole
>   range, the same layout and access, no write access). CommitBindings and
>   ExecutePreparedDraw paid 0.92 and 0.26 ms/flip for these at U55.
>   image.cpp is the same here as on the branch, so GetBarriers is the one
>   the predicate was written against.
> - TargetRecord* FrameEvents (instrumentation only): why a draw-sequence
>   target record ends invalid, by first reason (not a first-page answer, a
>   changed answer, the DCC or CMASK decision not a provable no-op).
>
> Conflicts: profiler.h/.cpp keep every earlier FrameEvent first (through
> DrawPrepCertRangesVerify*), then ImageLru*, ImageTransit* and TargetRecord*
> in their order.

</details>

### 298. Program cache: keep translated programs across runs (KYTY_PROGRAM_CACHE, default off)

Commit: [`02799e44`](https://github.com/Jetsku/KytyPS5-experimental/commit/02799e44f449b21ee23451f7fc92ccb1c4e6f209) · **Compilation and cache reuse**

Avoids repeated compilation, key variants, locking or lost warm-cache work in the named path. Benefits primarily concern compilation/loading; no isolated per-commit FPS gain is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Every run translated and emitted every program again although the Vulkan pipeline cache is
> persistent: about 500 programs per Sky Garden run, 10.7-10.9 s of command-processor time
> (U54/U55 hang traces: translate 4.6 s, emit 5.5 s, translation copies 0.4 s; module creation
> 0.02 s; spirv-val runs off the CP), p50 4 ms, p90 55 ms, max 0.4 s per program, and 26-27
> seconds of each run with a compile stall over 33 ms. The driver's pipeline creation is cheap on
> a cache hit (U54: 665 graphics pipelines, p50 0.15 ms, p99 2 ms, one over 8 ms).
>
> A program seen in an earlier run is now reloaded instead of translated:
> - Stored per source: the sealed resource plan, or the skip-dispatch verdict. Stored per
>   permutation: the SPIR-V, the plain LOD-stats variant and the compiled metadata. The file is
>   _PipelineCache/<title>.programs.bin next to the driver cache (KYTY_PROGRAM_CACHE_PATH names
>   another file).
> - IR::ProgramCodec encodes plans, specializations and metadata member for member, with layout
>   checks. Pointers become indices. Decoding rebuilds the plan exactly, use lists included, and
>   rejects inconsistent input.
> - Keys are exact. A source matches only its full key: stage, guest hash, user-data count, code
>   size, stage static key, wave size, user-data base, plain-variant flag, and every guest code
>   word including a merged back half. A permutation matches only its source digest, push-data
>   cursor and specialization bytes.
> - The file header (identity) holds everything else translation reads:
>   - the codegen version, a build-time SHA-256 of the recompiler sources and the code they call
>     (src/codegen_version.cmake);
>   - every CodegenOptions field and the emitter's host state;
>   - KYTY_RENDERER_BATCH and the buffer-cache page constants;
>   - the raw value of every KYTY_* switch named in the codegen sources, collected by the script;
>   - the device and driver.
>   The build fails on an include outside the codegen set, or on a SetHost* state missing from
>   the fingerprint. A static_assert covers CompileOptions. A file with another identity is
>   ignored, and replaced once this run has records of its own.
> - BuildStageStaticKey moves to shaderStaticKey.cpp, which is in the codegen set. It now also
>   keys the mesh workgroup's y and z sizes: the translator reads them, and PrepareProgram sets
>   both to 1.
> - Loading: a thread started with the pipeline cache loads and indexes the file, and lookups wait
>   for it.
> - Saving: a saver thread writes "<file>.tmp", flushes it and renames it over the file.
>   - Saves are coalesced like the driver cache saves, plus one at exit.
>   - A briefly held file (scanner, sync client) is retried for about a second.
>   - Each record has an XXH3-64 checksum. A damaged record and everything after it are dropped,
>     and the next save rewrites the file. A header mismatch rejects the whole file.
> - KYTY_PROGRAM_CACHE_VERIFY:
>   - 1: every reloaded program is translated and emitted anyway and compared (plan encoding,
>     SPIR-V, plain variant, every metadata member). On a difference, the fresh result is used and
>     the record dropped.
>   - exit: stops on the first difference.
>   - background: the reloaded program is used at once and checked on a background thread; a
>     difference drops the record for later runs.
>   Without verify, a reloaded source that needs a new permutation is compared at no extra cost.
> - Counters: FrameEvent ProgramDiskHits, Misses and VerifyMismatches; FrameWait ShaderDiskLoad;
>   compiles.csv load_us and "+disk"; summary.csv compile_disk_loads and compile_disk_load_us; and
>   a summary line at shutdown.
>
> Default off: the key is complete by construction under the stage static-key invariant that the
> in-memory program cache already relies on (audited here), and every verify run in the tests is
> clean, but no game run has used it yet.
>
> Tests:
> - Key and fingerprint sensitivity: every input changes the key, and every CodegenOptions field,
>   host state and codegen switch changes the fingerprint.
> - Cache file (--program-cache-only): round trip, identity mismatch, truncation at 155 offsets,
>   byte corruption, garbage and empty files, repair, invalidation and the size cap.
> - Codec round trips and translate-twice determinism for every compute and fragment case, and for
>   239 Astro Bot CS/PS dumps (--corpus-program-cache, not in ctest).
> - Renderer (--program-cache-gpu-only): compute dispatches and guest VS/PS draws run cold, then
>   reloaded, with identical results, also under verify and background verify.
> - A tampered stored SPIR-V is caught, replaced and then reloaded cleanly
>   (--program-cache-tamper-only).
> - The compute suite and the draw-prep engine run with the cache on and every reload verified.

</details>

Changed files: `CMakeLists.txt`, `src/codegen_version.cmake`, `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`, `src/graphics/host_gpu/renderer/pipeline/programDiskCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/programDiskCache.h`, `src/graphics/shader/recompiler/CodegenFingerprint.cpp`, `src/graphics/shader/recompiler/CodegenFingerprint.h`, `src/graphics/shader/recompiler/ir/ProgramCodec.cpp`, `src/graphics/shader/recompiler/ir/ProgramCodec.h`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/Value.h`, `src/graphics/shader/shader.cpp`, `src/graphics/shader/shaderStaticKey.cpp`, `tests/ShaderProgramCacheTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 299. Merge claude/program-cache: persistent translated-program cache (KYTY_PROGRAM_CACHE, default off), mesh workgroup y/z in the stage static key

Commit: [`0d5b7e7b`](https://github.com/Jetsku/KytyPS5-experimental/commit/0d5b7e7b9da6374e5ef6a5d5a5fda124040cb3c9) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_PROGRAM_CACHE=1 (default off): translated programs survive the run
>   in _PipelineCache/<title>.programs.bin (KYTY_PROGRAM_CACHE_PATH): per
>   source the sealed resource plan or skip verdict, per permutation the
>   SPIR-V, the plain LOD-stats variant and the compiled metadata
>   (IR::ProgramCodec). Keys are exact (the full guest code, stage static
>   key, specialization bytes); the file header holds the codegen version (a
>   build-time SHA-256 of the recompiler sources, src/codegen_version.cmake,
>   which also fails the build on an include outside the codegen set), every
>   CodegenOptions field, host state, codegen switch, and the device and
>   driver. Loading on a thread; saves coalesced (KYTY_PROGRAM_CACHE_SAVE=0:
>   only at exit), checksummed records.
> - KYTY_PROGRAM_CACHE_VERIFY=1|exit|background: every reloaded program is
>   translated and emitted anyway and compared (background: checked off the
>   CP, the stored record dropped on a difference).
> - Accuracy fix, also for the in-memory program cache: BuildStageStaticKey
>   moves to shaderStaticKey.cpp and now keys the mesh workgroup's y and z
>   sizes, which the translator reads.
> - FrameEvents ProgramDiskHits/Misses/VerifyMismatches (next to the
>   translation-cache counters), FrameWait ShaderDiskLoad; compiles.csv
>   load_us, summary.csv compile_disk_loads and compile_disk_load_us.
>
> The codegen source set is untouched on the U57 side (sched, cpu-opt and P3
> changed no recompiler, shader or gpu_format file, nor pipelineCache.cpp),
> so the restructured CompileAndPublish and Emit/FinishPermutation apply as
> on the branch. Conflicts: hang-trace summary.csv keeps U57's
> pending_ops_max and then compile_disk_loads, compile_disk_load_us, in the
> header and the rows alike; the compute-test dispatch keeps the image LRU
> and transit checks (closing the last one) before the program-cache modes.

</details>

### 300. Draw prep P4b-1: binding plans on the preparing threads (KYTY_DRAW_PREP_BINDINGS, off)

Commit: [`d4cef64f`](https://github.com/Jetsku/KytyPS5-experimental/commit/d4cef64f353d9cce87a7c9511efd30d3947aeb4c) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The thread that prepares a draw (a DrawPrep worker, or the command processor for a stolen slot)
> now also computes the draw's binding plan: the pure parts of binding and pipeline resolution
> (drawPrep/bindingPlan.h, Profiling/analysis/P4B-WORKER-LOOKUPS.md). The command processor uses
> a plan item only when DrawPrep::Validate accepted the same slot's preparation and the item's
> certificate holds; otherwise it runs today's code for that item.
>
> Items and certificates:
> - V# ranges (decode, ClampRangeSize) and vertex-buffer ranges (sizes, sort, merge, clamp): the
>   guest virtual-range generation read before the first clamp is unchanged at commit (new
>   VirtualRangesGeneration, ClampRangeSizeQuiet). A clamp that would change a size is left to
>   the command processor (it logs or stops). FindBuffer and ObtainBuffer still run there, in
>   the same order.
> - Pipeline: the key is built by the shared BuildGraphicsPipelineKey from predicted targets
>   (colour formats and sample counts from the registers, PredictRenderDepthTarget), topology,
>   the restart decision (DecidePrimitiveRestart; a custom index that needs the scan is left to
>   the command processor) and the plain-pixel choice; looked up with a per-thread memo and a
>   bounded TryLock, never created. At commit the resolved targets, topology, restart flag and
>   plain-pixel choice must equal the prediction and the pipeline generation must be unchanged.
> - Shader-data user dwords and mip-statistics fields, texture memo hashes, the program half of
>   DrawIsBarrierSafe, shader write stages, the KYTY_ALIAS_BYTES scissor union, the target export
>   mapping in RefreshShaders: pure functions of the preparation and the snapshot.
> - Sampler handles: lookup only (SamplerCache::FindSampler behind a per-thread memo keyed by the
>   cache instance); the cache never evicts. A sampler not created yet is resolved at commit.
> The serial paths now call the same helpers (key builder, vertex-range collection, restart
> decision, program barrier conditions), so the two cannot differ.
>
> Switches: KYTY_DRAW_PREP_BINDINGS=0 (default) | 1 | a list of pipeline, buffers, userdata,
> samplers, textures, statics. KYTY_DRAW_PREP_BINDINGS_VERIFY=1|exit computes every used item
> the serial way, compares, and uses the serial value. Inline mode computes plans on the command
> processor (tests).
>
> Counters: FrameEvent DrawPrepBinding{Plans,PlansUsed,PlansDropped,AbstainRanges,
> AbstainSamplers,AbstainPipeline,AbstainPipelineBusy,FallbackVm,FallbackPipeline,PipelinesUsed,
> VerifyChecks,VerifyMismatches}; FrameWait DrawPrepBindingPlan (plan time) and
> DrawPrepWorker1..8 (each DrawPrep#k's busy time, independent of the switch).
>
> Tests: draw_prep_engine gains a depth-target phase and checks the plans (inline: every
> committed draw has a plan and takes its pipeline from it); new ctest variants inline, parallel,
> parallel with a stealing single worker, the parts list, the CP recorder (thread and inline),
> and the whole compute suite under the verify mode.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/samplerCache.cpp`, `src/graphics/host_gpu/renderer/cache/samplerCache.h`, `src/graphics/host_gpu/renderer/depthRenderTarget.cpp`, `src/graphics/host_gpu/renderer/depthRenderTarget.h`, `src/graphics/host_gpu/renderer/drawPrep/bindingPlan.cpp`, `src/graphics/host_gpu/renderer/drawPrep/bindingPlan.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.h`, `src/graphics/host_gpu/renderer/render.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/kernel/memory.cpp`, `src/kernel/memory.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 301. Descriptors P4b-D0: offset-masked reuse audit (KYTY_DESCRIPTOR_OFFSET_AUDIT, off)

Commit: [`e8fd3f5f`](https://github.com/Jetsku/KytyPS5-experimental/commit/e8fd3f5f4cf31bf78a04f82e1af3d8a3eba99f2a) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A measurement for the dynamic-offset binding option (P4B-WORKER-LOOKUPS.md 2.8). With
> KYTY_DESCRIPTOR_OFFSET_AUDIT=1, CommitBindings digests every descriptor set write and push
> update with the buffer offsets of the would-be dynamic descriptors left out: the flattened SRT
> and shader-data tables of every stage first (new stream-ring ranges each draw), then the V#
> storage buffers in write order, up to maxDescriptorSetStorageBuffersDynamic. Buffers, ranges,
> images, samplers and every other descriptor stay in the digest. It counts, per command buffer,
> the sets and pushes whose digest repeats the previous one of their kind or any earlier one:
> FrameEvent DescriptorOffsetAudit{Sets,SetRepeatsPrevious,SetRepeatsAny,Pushes,
> PushRepeatsPrevious,PushRepeatsAny,OverLimit}. With KYTY_DESCRIPTOR_SET_REUSE_AUDIT=1 as well,
> DescriptorSetAuditRepeats gives the exact repeats, so the difference is what dynamic offsets
> would add. Push layouts cannot hold dynamic descriptors; their counts are the potential of
> splitting them into a pushed set and a dynamic one. Nothing else changes; off by default.
>
> Test: the whole compute suite with both audits on (shader_recompiler_compute_offset_audit).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`.

### 302. Merge claude/p4b-lookups: binding plans on the preparing threads (KYTY_DRAW_PREP_BINDINGS, off), offset-masked reuse audit

Commit: [`91540e49`](https://github.com/Jetsku/KytyPS5-experimental/commit/91540e493c829fa7385bd265bf85f09f19012b0a) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - KYTY_DRAW_PREP_BINDINGS=0 (default) | 1 | a list of pipeline, buffers,
>   userdata, samplers, textures, statics: the thread that prepares a draw
>   (a DrawPrep worker, or the CP for a stolen slot) also computes the pure
>   parts of binding and pipeline resolution (drawPrep/bindingPlan.h). The CP
>   uses a plan item only when Validate accepted the same slot's preparation
>   and the item's certificate holds, else today's code runs for that item.
>   The pipeline key comes from the shared PipelineCache::BuildGraphicsPipelineKey,
>   which the serial GetGraphicsPipeline now calls too.
>   KYTY_DRAW_PREP_BINDINGS_VERIFY=1|exit computes every used item the serial
>   way, compares and uses the serial value.
> - FrameEvent DrawPrepBinding*, FrameWait DrawPrepBindingPlan and
>   DrawPrepWorker1..8 (each DrawPrep#k's busy time, independent of the
>   switch).
> - KYTY_DESCRIPTOR_OFFSET_AUDIT=1 (default off, measurement only): descriptor
>   set and push repeats with the would-be dynamic offsets left out.
>
> Coexistence: a slot carries P3's certificate ranges inside `prepared`
> (built by Prepare) and P4b's `plan` (built right after it by PlanBindings);
> Validate uses the certificate at commit and the plan is activated only
> after it accepts. The program cache's restructuring of pipelineCache.cpp
> is confined to the program cache (CompileAndPublish, Emit/Finish
> permutation, disk cache); GetGraphicsPipeline and the key are only changed
> here, so the worker-side and the resolver's keys come from one function.
>
> Conflict: DrawPrep worker Run() keeps sched-contention's placement-sample
> counter and sets P4b's worker number.

</details>

### 303. Instrumentation: cheaper collection of the same profiler and hang-trace data on the CP

Commit: [`98714ef1`](https://github.com/Jetsku/KytyPS5-experimental/commit/98714ef1fb40869867c8ea6efe3dfd97a7e46cac) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> DEEP-TRACE-U54 4.1 item 7. The U55 start-view WPR trace (Tracy disconnected, hang trace on) still
> shows on the command processor, per flip: ~ScopedBlock 0.22 ms, HangTrace::RecordTexture 0.23,
> HangTrace::NoteGpuWrite 0.19 (0.13 of it in std::unordered_map), and with Tracy connected (U54
> fly-in) Profiler::Detail::CurrentThreadCounters 0.51. The GpuOpProfiler hooks around BeginRendering
> and barriers are 93-99% driver time inside the wrapped call; their own cost is ~0.03 ms/flip, so
> they are left as they are. No output changes: same counters, zones, CSV rows and values.
>
> - constinit thread-locals (Profiler::Detail::t_counters, GpuOpProfiler::Detail::t_site and
>   t_scope). As plain extern thread_locals, every access from another translation unit first tested
>   the thread-local initialization guard of profiler.cpp's TLS block and could call its
>   initializer; that also kept CurrentThreadCounters out of line (510 call sites in the U56 exe).
>   With constinit the compiler reads the variable directly.
> - Profiler::ScopedBlock keeps its Tracy zone in raw storage with a flag instead of
>   std::optional<tracy::ScopedZone>. The optional's destructor pulled Tracy's zone-end code into
>   every scope, so ~ScopedBlock stayed out of line (327 call sites). The destructor is now one flag
>   test, always inlined (End is noexcept); Begin and End construct and destroy the same zone.
> - In the new kyty_emulator.exe no out-of-line ~ScopedBlock, CurrentThreadCounters or
>   RecordTexture remains (PDB publics; the U56-based build before had 327, 510 and all call
>   sites).
> - HangTrace::RecordTexture (every resolved texture, ~50k per flip) is inline and counts
>   tex_resolved in a per-thread block with a relaxed load and store (no locked instruction, no
>   call); the publisher sums the blocks. The 1% of descriptors with the mip-statistics bit take the
>   out-of-line path as before (tex_mipstats, texstats.csv).
> - HangTrace::NoteGpuWrite skips a note identical to one of its thread's last eight notes (range,
>   kind, size and millisecond) when nothing has changed the page map since that thread's last
>   update (a version bumped by every change) and the map is below its clearing size. The map then
>   already holds exactly these values for these pages. A thread's recent notes are invalidated by
>   its own overlapping updates, by an update that clears the map and by any other thread's update.
>   Readback rows (readbacks.csv) see the same map.
>
> Bench (profiler_counter_tests timing, the same bench code built against the old and the new
> library, the two binaries alternating 8 times each, logical processors 8-15 at HIGH priority, quiet
> machine: CPU 2.9%, no builds; median ns per call; the "fresh" case is noise-sensitive):
>   counted event, per-thread sink (profiler connected)   2.50 -> 1.40  (-44%)
>   counting loop, per-thread sink                         1.75 -> 1.10  (-37%)
>   profiler block, zones off                              2.20 -> 1.23  (-44%)
>   HangTrace::RecordTexture                               1.56 -> 1.51  (no change in isolation)
>   HangTrace::NoteGpuWrite, 8 bindings noted repeatedly   54.9 -> 18.7  (-66%)
>   HangTrace::NoteGpuWrite, fresh ranges                  45.6 -> 50.7  (+5 ns: the recent-note
>                                                                          bookkeeping)
> RecordTexture's in-situ cost (0.23 ms/flip, ~4.6 ns a call) is the locked add among the CP's
> stores plus the call; a tight loop hides both, so its effect shows only in a trace.
> Estimate (E) from the U55/U54 attributions: -0.3 to -0.5 ms/flip on the CP with Tracy disconnected
> and the hang trace on, -0.5 to -0.8 with Tracy connected; the next WPR trace measures it.
>
> Tests: profiler_counters adds TestNoteGpuWriteRepeats (repeats, an overlapping write of another
> kind, another thread's write, large bindings: kind and size per page as if every note were
> applied; HangTrace::PageWriterForTest reads the map) and prints the instrumentation timings.
> Full suite 166/166 and the validation label 17/17 (shader_cfg and kernel_file_system excluded as
> known).

</details>

Changed files: `src/common/hangTrace.cpp`, `src/common/hangTrace.h`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/gpuOpProfiler.cpp`, `src/graphics/host_gpu/renderer/gpuOpProfiler.h`, `tests/ProfilerCounterTests.cpp`.

### 304. Merge claude/sched-contention: cheaper collection of the same profiler and hang-trace data on the CP

Commit: [`7f00339c`](https://github.com/Jetsku/KytyPS5-experimental/commit/7f00339cfe512bedc489a8609fc5d7fc62be6469) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - constinit thread-locals (Profiler::Detail::t_counters,
>   GpuOpProfiler::Detail::t_site and t_scope): other translation units read
>   them without the thread-local initialization guard, so
>   CurrentThreadCounters inlines.
> - Profiler::ScopedBlock keeps its Tracy zone in raw storage with a flag; the
>   destructor is one inlined flag test.
> - HangTrace::RecordTexture is inline with per-thread counts summed by the
>   publisher; NoteGpuWrite skips a note identical to one of its thread's
>   last eight while the page map is unchanged.
> - Same counters, zones, CSV rows and values; new test
>   TestNoteGpuWriteRepeats in profiler_counters.
>
> Checked with U57's other profiler additions (program cache, P4b's
> DrawPrepBinding* events and DrawPrepWorker1..8 waits, which P4b's workers
> record through ScopedFrameWait): check_profiler_names aligned, and every
> target builds. Conflict: profiler_counters' main runs cpu-opt's
> TestFrameWaitWithoutProfiler, then TestNoteGpuWriteRepeats.

</details>

### 305. Draw prep P4b: keep profiler and test changes clear of P3b's

Commit: [`7b121c8a`](https://github.com/Jetsku/KytyPS5-experimental/commit/7b121c8a0ad8e6368cd221bcfac60bb2869ddb4e) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> P3b (claude/p3-sequencer, not yet committed) appends FrameWaits at the end of the enum and
> changes the final line of CheckDrawPrepEngineDraw. Moves the DrawPrepBindingPlan and
> DrawPrepWorker1..8 FrameWaits (and their names) next to DrawPrepSteal, and prints the binding-plan
> summary of the engine test on its own line, so both branches merge without conflicts (checked
> with a three-way merge against P3's working copy; the merged name tables align). No behaviour
> change.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 306. Merge claude/p4b-lookups: P4b's FrameWaits next to DrawPrepSteal, engine-test summary on its own line

Commit: [`4f9ab78d`](https://github.com/Jetsku/KytyPS5-experimental/commit/4f9ab78d0f12e9fd7eb14d49cf89ed7c96ceb9c2) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - DrawPrepBindingPlan and DrawPrepWorker1..8 (enum and both name tables)
>   move next to DrawPrepSteal, and the engine test prints the binding-plan
>   summary on its own line, so P3b can append FrameWaits and change the
>   test's last line without conflicts. No behaviour change.
>
> No conflicts; check_profiler_names aligned after the move.

</details>

### 307. Merge claude/u57 into claude/p3-sequencer: P3a and P3b on U57

Commit: [`d5a281cb`](https://github.com/Jetsku/KytyPS5-experimental/commit/d5a281cb2bf8cbfa2e29aa82a42fa15a46d1026e) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> U57 (4f9ab78d): P4b-1 with 7b121c8a, the program cache, the sched-contention and cpu-opt work,
> and this branch's d52bc9db.
>
> Conflicts, each two additions at the same place:
> - profiler.h/.cpp: U57's FrameEvents (image LRU and transition skips, target records), then
>   P3's, so U57's values stay as they are.
> - commandProcessor.h: U57's m_placement_packets, then the P3 members.
> - graphicsRun.cpp, ProcessPacket: U57's CP placement sample (every 256th packet) runs only where
>   packets execute. It does not run on the sequencer thread, which it would count as the CP, or in
>   a reference front. With KYTY_CP_SEQ=1 the resolver samples every 256th op
>   (ResolveSubmission).
>
> With KYTY_CPU_RESERVE=cp the sequencer keeps off the CP's core like every other thread (the
> process default CPU sets).
>
> Full suite 230/230 and the validation label 17/17 (shader_cfg and kernel_file_system excluded as
> known). No game run.

</details>

### 308. Merge claude/p3-sequencer: CP ops (P3a) and the graphics front on a sequencer thread (P3b, KYTY_CP_SEQ, default 0)

Commit: [`02091730`](https://github.com/Jetsku/KytyPS5-experimental/commit/020917303e8dd9f5a06a740297de974e78349844) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - P3a: the command processor is split into a front (PM4 parse, the
>   register state the handlers write) and a back that executes every effect
>   from an op (cpOps.h). KYTY_CP_SEQ=0 (default) executes each op directly
>   where the handler asks for it (the same path and order as before);
>   =inline encodes every op into an SPSC ring (KYTY_CP_SEQ_RING_KB, 512) and
>   executes it at once on the same thread.
> - P3b: KYTY_CP_SEQ=1 moves the graphics queue's parse to its own thread
>   (CpSequencer); the GPU thread resolves the ops in order under today's
>   scheduling. Ordering points (lockstep ops, unprovable guest reads, frame
>   fences, constant-engine handoffs, pending CP writes) wait for the
>   resolver; direct draws with parallel draw prep are published to the
>   draw-prep window by the sequencer, other draws carry register snapshots
>   (KYTY_CP_SEQ_SNAPSHOTS, 64). KYTY_CP_SEQ_SPIN_US (200),
>   KYTY_CP_SEQ_RESOLVER_SPIN_US (50), KYTY_CP_SEQ_IDEAL_CPU.
> - KYTY_CP_SEQ_VERIFY=1|exit: a reference front re-parses each stream and
>   compares every op (kind, position, parsed-dword hash, payload).
> - The draw-prep packet hook is decided once per slice instead of per
>   packet.
>
> The branch already contains U57 (d5a281cb merged 4f9ab78d), so this merge
> has no conflicts and its tree is the branch's.

</details>

### 309. Draw prep P4b-1b: check verdict and dynamic viewports in binding plans (off)

Commit: [`6bcd9508`](https://github.com/Jetsku/KytyPS5-experimental/commit/6bcd9508e159b9b157be610b846ed6027e19e30d) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Two new KYTY_DRAW_PREP_BINDINGS parts (both in =1):
> - hwcheck: the preparing thread decides whether uc_check and hw_check would neither stop the
>   emulator nor log for the draw's registers (hw_checks_quiet); DrawIndex/DrawAuto then skip
>   them. Each check now runs from one implementation with two policies (debug.cpp): CheckRun,
>   today's exits and logs, and CheckPredict, which only notes whether CheckRun would exit or log.
>   The log-once flags and counted logs moved into one state (atomics): a flag only goes from
>   clear to set and a counter only grows, so a quiet verdict stays true until the commit, and the
>   log output is the same. The verdict needs only the registers, which are the draw's snapshot at
>   commit, so it applies without a validated preparation.
> - dynamic: SetGraphicsDynamicParams' viewports and scissors before the framebuffer clamp, from
>   the registers and the vertex program; the command processor clamps them to the framebuffer
>   it acquired. calc_final_scissor is split into calc_scissor_unclamped (no log, false for an
>   unsupported clip-rect rule, which the plan leaves to the command processor) and the clamp.
>
> Fix to P4b-1: the statics part's scissor union called calc_final_scissor on the preparing
> thread, which logs an unsupported clip-rect rule; it now uses the quiet variant and leaves such
> draws to the command processor (no log from a worker, the same log count).
>
> Verify mode (KYTY_DRAW_PREP_BINDINGS_VERIFY) predicts the checks again at commit, runs them and
> compares; it computes the viewports and scissors serially and compares them bitwise.
> Counters: DrawPrepBindingHwChecksSkipped, DrawPrepBindingViewportsUsed.
>
> Tests: draw_prep_engine gains a phase where the test's check logs are spent and, in inline mode,
> both committed draws skip the checks and take their viewports from the plan (pixels equal to the
> serial draws); new ctest variants draw_prep_engine_inline_bindings_checks(_verify).

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/debug.cpp`, `src/graphics/host_gpu/renderer/debug.h`, `src/graphics/host_gpu/renderer/drawPrep/bindingPlan.cpp`, `src/graphics/host_gpu/renderer/drawPrep/bindingPlan.h`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `src/graphics/host_gpu/renderer/renderDraw.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 310. Draw prep P4b-2: texture memo hints and runs of memo hits under one lock (off)

Commit: [`1c48a4e7`](https://github.com/Jetsku/KytyPS5-experimental/commit/1c48a4e73a0483b3cbb3eb40850db930d87fb065) · **Parallel draw preparation and state reuse**

Moves or avoids repeated preparation/validation work while checking dependency freshness before ordered commit. The named check or fallback preserves correctness; bundle measurements do not isolate this commit.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> New KYTY_DRAW_PREP_BINDINGS part texturememo (in =1):
> - Worker: for every texture binding of a stage, besides the key's hash (P4b-1), the tag of the
>   TextureBindingMemo entry that holds the key now (TextureBindingMemo::FindHint). The memo's
>   entries publish their key (packed losslessly into atomic words) and tag under a sequence
>   word that Record makes odd while it rewrites them, so a worker reads a consistent pair without
>   a lock and never touches the entries' other fields. Tags are unique per recording and an
>   entry's key never changes under its tag: while the entry keeps the tag it holds the key.
> - Command processor, PrepareBindings of a stage that is not a draw-sequence repeat:
>   TryResolveRun resolves the bindings whose entries still hold their hinted tags and pass
>   TryResolve's hit conditions (no revalidation), in binding order, under one texture-cache lock,
>   with TryResolve's touches, description copies and counts; the first binding that is not such
>   a hit is resolved as before (TryResolve with the tag hint in place of the key comparison) and
>   the next run starts after it. Touches and BindImage do not change what the next binding's
>   check reads, so the order of effects is TryResolve's; other threads see a run as one critical
>   section.
> - RebindImages: TryAcquireViewRun does the same for TryAcquireView (when the stage-wide
>   TryRepeatViews did not take the stage), with the same per-binding fallback.
>
> Verify (KYTY_DRAW_PREP_BINDINGS_VERIFY): runs are only predicted; every binding is resolved and
> every view acquired as before, and each predicted hit must be TryResolve's plain hit on the
> hinted entry and acquire the entry's view; a tag hint's key is compared with the binding's.
> Counters: DrawPrepBindingTextureHints (workers), DrawPrepBindingTextureRunHits,
> DrawPrepBindingViewRunHits.
>
> Tests: CheckDrawPrepEngineTextures (--draw-prep-engine-only and the whole suite): five
> textured draws (T# and S# in the pixel shader's user SGPRs, each texture on its own page),
> serially, then one draw per stream in two rounds, then all five in one stream (one window,
> blended); pixels equal the serial draws'. In inline mode every binding of the second round is
> resolved in a run (and every view with KYTY_DRAW_SEQUENCE_FAST=0); in parallel mode the window
> phase gets plans from the workers. New ctest variants: texturememo inline, verified, without
> draw-sequence textures, and parallel with every part verified.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/drawPrep/bindingPlan.cpp`, `src/graphics/host_gpu/renderer/drawPrep/bindingPlan.h`, `src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.cpp`, `src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.h`, `src/graphics/host_gpu/renderer/render.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 311. Merge claude/p4b-lookups: check verdicts, dynamic viewports and texture-memo runs in binding plans (P4b-1b, P4b-2; off)

Commit: [`390842c9`](https://github.com/Jetsku/KytyPS5-experimental/commit/390842c94e446382913c7df694b997af167bd7dc) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> New KYTY_DRAW_PREP_BINDINGS parts (all in =1; the default stays 0):
> - hwcheck: the preparing thread predicts whether uc_check and hw_check
>   would neither stop nor log for the draw's registers, and the commit then
>   skips them (one check implementation, CheckRun and CheckPredict; log-once
>   flags and counters as atomics, so a quiet verdict stays true).
> - dynamic: viewports and scissors before the framebuffer clamp, which the
>   CP then applies.
> - texturememo: the worker records each texture binding's memo tag
>   (TextureBindingMemo::FindHint, a seqlock-published key and tag), and the
>   CP resolves runs of hinted memo hits (TryResolveRun) and view acquisitions
>   (TryAcquireViewRun) under one texture-cache lock, in binding order, with
>   TryResolve's effects; the first non-hit falls back per binding.
> - Fix to P4b-1: the statics part's scissor union no longer logs from a
>   worker (quiet clip-rect variant; unsupported rules go to the CP).
> KYTY_DRAW_PREP_BINDINGS_VERIFY also predicts and runs the checks,
> recomputes viewports and scissors, and resolves every predicted run
> binding the serial way.
>
> No conflicts. The hunks do not overlap P3b's: in drawPrep.cpp P4b changes
> only PlanBindings; the slot keeps the agreed layout (`plan` next to
> `prepared`), CommitPublished commits through CommitHead and Commit, which
> hands the worker's plan to the executor, and the fields the resolver
> patches there (submit id, instance count) are not read by a plan. The
> merged engine test prints P3b's "sequenced" summary line and then runs
> P4b's CheckDrawPrepEngineTextures; P3b's thread-mode draw-prep variants
> therefore also run the texture checks under the sequencer. Profiler: P4b's
> events after DrawPrepBindingVerifyMismatches, P3's appended; aligned.

</details>

### 312. Materialize fast-clear eliminates before metadata-free sampling

Commit: [`8c4cf634`](https://github.com/Jetsku/KytyPS5-experimental/commit/8c4cf634a5e2b9a7d845361dce50f9a7875da93b) · **Water fast-clear elimination**

Materializes the fast-clear eliminate before metadata-free sampling so aliased water inputs contain the requested clear. Correctness fix; matched isolated timing was not established.

Changed files: `docs/metadata-clear-eliminate.md`, `src/graphics/host_gpu/renderer/renderDraw.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 313. Add opt-in host input controls for unattended testing

Commit: [`440a02d3`](https://github.com/Jetsku/KytyPS5-experimental/commit/440a02d30b7b11abb14851eecd29083f274b512c) · **Workflow and compatibility controls**

Supports controlled launches, navigation or documentation. This does not optimize rendering; private operational documents are excluded from the publication.

Changed files: `CMakeLists.txt`, `docs/host-input-testing.md`, `src/common/hostInputTrace.h`, `src/graphics/presentation/window/hostInput.cpp`, `src/graphics/presentation/window/hostInputPulse.h`, `src/libs/controller.cpp`, `tests/HostInputPulseTests.cpp`.

### 314. Handle constant-only vertex inputs and diagnose invalid descriptors

Commit: [`e786dd6a`](https://github.com/Jetsku/KytyPS5-experimental/commit/e786dd6a2b7f3bed6a658f6a1f0ca49adde1b49a) · **Vertex constants and failure diagnostics**

Avoids native formats for embedded constant-only inputs and validates actual formatted reads; retains strict checks and captures invalid descriptors. This correction alone did not resolve the intermittent crash.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Omit native vertex attributes when embedded fetches emit only constants, retaining
> original shader locations for the remaining attributes. Validate only consumed
> components and reject invalid speculative descriptors before buffer detection.
> Add bounded descriptor logs and opt-in shader/table dumps for startup failures.
>
> This correction alone did not resolve the observed fast-start crash. Builds
> succeeded; no new automated tests were run for these changes.

</details>

Changed files: `src/graphics/host_gpu/renderer/pipeline/shaders.cpp`, `src/graphics/shader/recompiler/frontend/translate/Attribute.cpp`, `src/graphics/shader/shader.cpp`.

### 315. U59: reject CPU-dirty images during alias materialization

Commit: [`919e712a`](https://github.com/Jetsku/KytyPS5-experimental/commit/919e712a0c109d3de167ebbbbeb58c3a8e4986c1) · **U59 stale-image fix**

Rejects CPU-dirty images as alias materialization sources so stale GPU contents cannot overwrite newer CPU vertex data. This fixes a correctness failure; no FPS gain is claimed.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Prevent stale native image contents from being copied over CPU-written guest data
> on image retirement or alias rebuild. Exclude CPU-dirty aliases from ownership
> preservation candidates. Add opt-in rejection diagnostics limited to eight events.
>
> The failing trace copied a retired 3840x2160 image over the vertex-table region.
> The candidate's next live run rejected that same stale image and the user reported
> no crash. Repeated stability, normal/top-down water and performance remain unverified.
>
> Archived EXE SHA256:
> 08CF24AE046C401FAA1EE3B2522059C8C19DC64E074A9DD1961C50B04907AE5B
> Build completed successfully. No automated tests added or run this iteration.

</details>

Changed files: `src/graphics/host_gpu/renderer/cache/textureCache.cpp`.

## RT additions

34 commits (including integration merges).

### 1. Shaders: the remaining 64-bit V_CMP/V_CMPX integer compares

Commit: [`f266ddf6`](https://github.com/Jetsku/KytyPS5-experimental/commit/f266ddf60a9fea82f21a9afd4ecc078499405627) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Adds the RDNA2 64-bit integer compares that were missing:
> - V_CMP_{F,LT,LE,GT,NE,GE,T}_I64 and V_CMP_{F,LE,GE,T}_U64;
> - every V_CMPX_*_I64/U64 form except the NE ones Kyty already had.
> They cover VOPC 0xa0-0xa7, 0xb0-0xb7, 0xe0-0xe7 and 0xf0-0xf7; 64-bit
> operands cannot use DPP.
>
> Each compare is built from the existing SLessThan64 / ULessThan64 /
> UGreaterThan64 / IEqual64 / INotEqual64 IR, with swapped sources or a
> negated result. F and T are constants.
>
> Psr's sanity-checker BVH kernels use V_CMP_LE_U64. A shader with any of
> these opcodes used to exit at CFG build, so no shader that runs today
> changes. That is why, like the other opcode additions (6a25c043), these
> are not behind a switch.
>
> Test: VectorCompare64BitOps, in the default shader_recompiler_compute
> suite and also run by --cmp-64-only. It covers all 25 opcodes with 6
> operand pairs that separate signed from unsigned order and the high
> dword from the low. A V_CMPX result shows as whether the following store
> ran.

</details>

Changed files: `src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.h`, `src/graphics/shader/recompiler/frontend/decode/VectorAluOps.cpp`, `src/graphics/shader/recompiler/frontend/translate/Compare.cpp`, `src/graphics/shader/recompiler/frontend/translate/Translator.h`, `src/graphics/shader/recompiler/frontend/translate/Vector.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 2. Shaders: KYTY_SRT_VARIANT_READS covers every raw load through a runtime V# and skips S_SWAPPC_B64

Commit: [`ebb3f71e`](https://github.com/Jetsku/KytyPS5-experimental/commit/ebb3f71e3a77dea0cff0f7d232f2dfe4d1229bb2) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> These are two more gaps that Psr's BVH builders hit. Offline, current Kyty
> exits on 135 of the 460 Psr kernels in the eboot.
>
> - Raw vector loads through a V# the shader computes (BUFFER_LOAD_DWORD,
>   UBYTE, SBYTE, USHORT and SSHORT) now read through BDA like the
>   DWORDX2-X4 loads (ResourceKind::IndirectBuffer). Before, only DWORDX2-X4
>   could.
>   - Addressing and the OOB_SELECT range checks follow LoadIndirectBuffer
>     (RDNA2 ISA 8.1.5), with the element's own size as the OOB_SELECT 3
>     payload. An out-of-range element reads 0.
>   - This fixes the 3 load-only Psr kernels among the 32 that compute V#s at
>     runtime (estimate_traversal_cost, qbvh_refit_bottom_level{,_with_copy}).
>   - The other 29 store or atomically update through such V#s. They need
>     KYTY_BDA_WRITES (design: Profiling/analysis/BDA-WRITES-DESIGN.md) and are
>     still skipped with a log line.
> - S_SWAPPC_B64 with a destination is a call through a function pointer.
>   Psr's 96 shader_mesh builders use it to fetch vertices through a callback.
>   A program that uses it is now skipped with one log line instead of
>   exiting when the CFG is built.
> - IndirectBuffer accesses that the switch creates carry
>   NoIndirectBufferResource, and resource-control-flow planning skips them,
>   because they have no descriptor source. This replaces the
>   ReadConstBuffer-only special case.
>
> Default behaviour is unchanged: the new tracking paths, the skip and the
> resource marker exist only under the switch.
>
> Tests (srt_variant_reads):
> - RawLoadsThroughGpuSelectedDescriptors: 18 cases covering OOB_SELECT
>   0-3, SOFFSET, the payload size of byte and short loads, sign extension
>   and an invalid FORMAT. The V#s are selected through GPU data.
> - CallIsSkipped.

</details>

Changed files: `src/graphics/shader/recompiler/ShaderRecompiler.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp`, `src/graphics/shader/recompiler/ir/Program.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.cpp`, `src/graphics/shader/recompiler/ir/passes/ResourceTracking.cpp`, `tests/ShaderSrtVariantTests.inc`.

### 3. KYTY_BDA_WRITES: stores and atomics through runtime V#s write through BDA, settled synchronously

Commit: [`868ec21e`](https://github.com/Jetsku/KytyPS5-experimental/commit/868ec21e9d695ab8d082da3e37d8d5bafcfa2744) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Psr's BVH builders (the decompress_* kernels, radix sort/scan/reduce
> *_indirect, qbvh_partial_copy, compacted headers) store and do atomics
> through V#s they compute: a loop-carried per-geometry base, or an indirect
> count as num_records. Kyty refused writes through BDA ("writable
> FLAT/GLOBAL addresses require GPU ownership tracking"), so 29 of the 32
> kernels that compute V#s at runtime could not run.
>
> The design was agreed with the CPU, scheduling, Water and P2 recorder
> agents: Profiling/analysis/BDA-WRITES-DESIGN.md, v2.
>
> Shader side, with KYTY_BDA_WRITES=1:
> - Resource tracking turns a raw store (byte, short, dword x1-x4) or any
>   buffer atomic through such a V# in a compute program into an
>   IndirectBuffer write. ShaderInfo::bda_writes and has_address_writes are
>   set, so CommitBindings' unbounded-writer handling applies.
> - The SPIR-V side uses the RDNA2 ISA 8.1.5 range checks of the BDA loads,
>   for all OOB_SELECT modes:
>   - stores are checked per DWORD;
>   - atomics are all-or-nothing, and an out-of-range atomic returns 0;
>   - a dword address ignores its low bits, and a byte or short replaces its
>     bits of the containing dword atomically, as the bound path does.
> - Each write goes through the BDA page table to a PhysicalStorageBuffer
>   pointer (u32 or u64 atomics) and sets its page in a written-page bitmap.
>   The bitmap is the fault buffer's second half, which exists only with the
>   switch on.
> - A write to a page without a cache buffer is dropped and counted. Its
>   fault creates the buffer for later dispatches.
>
> Renderer side, in the compute dispatch paths (direct and indirect):
> - Before the dispatch:
>   - clear the bitmap on first use;
>   - forget every known fill;
>   - count GPU-modified images over cache buffers, and move them into their
>     buffers when KYTY_IMAGE_WRITEBACK_ON_GPU_WRITE is on, as for writable
>     bindings.
> - Right after the dispatch (synchronous settle, before the CP continues):
>   - record a compaction of the bitmap. It is fault_buffer_process.comp
>     built with room for 64Ki pages; on overflow every cache buffer is
>     settled.
>   - submit, wait, and treat every written range exactly like a writable
>     binding over it: SynchronizeBuffer(written), MarkContentWritten,
>     CleanVerdict, m_gpu_modified_ranges, NoteBufferContentWrite,
>     ForgetKnownFills, hang trace, and TextureCache::InvalidateMemoryFromGPU.
>   - A written page under a GPU-modified image is counted.
>   - No later reader, label or GC can see a written page as clean.
> - KYTY_BDA_WRITES=verify makes dropped writes and written pages under a
>   GPU-modified image fatal.
> - Counters: FrameEvent BdaSettles, BdaSettlePages, BdaDroppedWrites,
>   BdaAliasHits and BdaAliasedImages; FrameWait BdaSettle.
>
> With the switch off there is no new work. The fault buffer keeps its size
> and no pipeline or buffer is created. The only additions on the dispatch
> path are `program.info.bda_writes` tests next to the uses_dma ones.
>
> Offline, on all 460 Psr kernels in the eboot (wave64, with
> KYTY_SRT_VARIANT_READS=1 and KYTY_BDA_WRITES=1):
> - 363 translate and materialize. That includes all 32 that compute V#s,
>   and the 9 whose descriptors only failed with the harness's fake index
>   user data.
> - The 96 shader_mesh builders are skipped (S_SWAPPC_B64 callbacks).
> - One Pro-only kernel exits on S_BREV_B64: decompress_refit_hierarchy_trinity.
>   Kyty reports base PS5 mode.
> Without the switches, 135 of the 460 exit.
>
> Tests (ctest bda_writes, which sets KYTY_BDA_WRITES=1 and
> KYTY_SRT_VARIANT_READS=1):
> - WritesThroughGpuSelectedDescriptors: stores and atomics through
>   GPU-selected V#s, checked against expected memory. It covers
>   per-component ranges, byte and short RMW, SOFFSET, structured records,
>   add/or/cmpswap/inc, 64-bit swap/or, out-of-range writes and an invalid
>   format.
> - BdaWritesSettle, through the real RenderExecutor and buffer cache:
>   - the written page is GPU-owned right after the dispatch, and its
>     neighbours stay clean;
>   - a guest-thread read afterwards (the fault route) sees the BDA store
>     and atomic;
>   - a write to a page without a cache buffer is dropped.

</details>

Changed files: `CMakeLists.txt`, `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/bufferCache.h`, `src/graphics/host_gpu/renderer/cache/faultManager.cpp`, `src/graphics/host_gpu/renderer/cache/faultManager.h`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/ShaderRecompiler.cpp`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterModule.cpp`, `src/graphics/shader/recompiler/ir/Program.cpp`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/passes/ResourceTracking.cpp`, `src/graphics/shader/recompiler/ir/passes/ResourceTracking.h`, `tests/ShaderBdaWriteTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 4. KYTY_BDA_WRITES: place the profiler entries and the settle test away from astro-profile's additions

Commit: [`a2cc8b76`](https://github.com/Jetsku/KytyPS5-experimental/commit/a2cc8b76a235585091af3dc6c1f373ef76f8f029) · **Diagnostics and attribution**

Makes the named event, cost or failure observable. It does not itself establish lower frame time; collection can add overhead. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> No functional change: the BDA FrameEvents follow BdaSyncHotVerifyMismatches, FrameWait
> BdaSettle follows AgcDoneWait, and CheckBdaWritesSettle precedes the CMASK fast-clear test,
> so a merge with codex/astro-profile (af95961e) has no conflicts.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 5. Merge claude/rt-tiled-lighting: 64-bit compares, KYTY_SRT_VARIANT_READS, KYTY_BDA_WRITES

Commit: [`f725ef79`](https://github.com/Jetsku/KytyPS5-experimental/commit/f725ef79c3d3259c8b3c0529bbf00880b94ce632) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RT-TILED's Psr builder support for the joint RT run (all off by default).

</details>

### 6. Hardware RT prototype: Psr BVH conversion and ray-query kernels (tests only)

Commit: [`4002d7be`](https://github.com/Jetsku/KytyPS5-experimental/commit/4002d7be26e2baa825e430c7c6d18679f51de557) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Optional hardware ray-tracing groundwork. Nothing is wired into the
> emulator; default behaviour is unchanged.
>
> - graphics/host_gpu/rt/psrBvh: the Sony Psr BVH layout as Astro Bot's
>   inlined traversal routine reads it (qbvh header +24/+48/+88/+96, fake
>   instance at +0x80, root pointer 37, box16/box32, Psr leaf lists, fan
>   triangle nodes t0..t3, 128-byte instance records); CPU walkers for BLAS
>   leaves, triangles and TLAS instances; a CPU reference traversal with the
>   guest's closest-hit rules and pluggable node tests. Type-6 (Psr shared
>   exponent) boxes report "unsupported" until the exact decoder exists.
> - graphics/host_gpu/rt/rtAccel: RT device support query, required
>   extensions, build sizes, build/update recording, barriers.
> - shaders rt_psr_*: conversion on the GPU without readback (batched
>   breadth-first BLAS walks emitting one AABB per leaf or float3 triangles,
>   TLAS walk plus per-instance conversion, NaN-inactive capacity slots,
>   guest addresses through a 16 KiB page table like the BDA path); a GPU
>   software traversal; ray-query traces in hybrid mode (RT cores for boxes,
>   the guest's leaf tests and acceptance rule per candidate AABB) and in
>   triangle mode (opaque triangles, approximate).
> - rt_hardware_tests (ctest rt_hardware_cpu / rt_hardware_gpu, --bench):
>   hand-built Psr BVHs (box32, box16 interiors, leaf roots, fan nodes,
>   culling and masked instances, mesh-like surfaces). Reference traversal
>   equals brute force; GPU software trace equals the CPU reference; hybrid
>   equals both with bit-identical t; triangle mode agrees on all tested rays.

</details>

Changed files: `CMakeLists.txt`, `src/graphics/host_gpu/rt/psrBvh.cpp`, `src/graphics/host_gpu/rt/psrBvh.h`, `src/graphics/host_gpu/rt/rtAccel.cpp`, `src/graphics/host_gpu/rt/rtAccel.h`, `src/graphics/host_gpu/shaders/rt_psr_common.inc`, `src/graphics/host_gpu/shaders/rt_psr_convert.comp`, `src/graphics/host_gpu/shaders/rt_psr_trace_hw.comp`, `src/graphics/host_gpu/shaders/rt_psr_trace_sw.comp`, `tests/RtHardwareTests.cpp`, `tests/RtPsrBvhBuilder.h`.

### 7. Hardware RT: decode Psr shared-exponent (type 6) box nodes

Commit: [`a04b2556`](https://github.com/Jetsku/KytyPS5-experimental/commit/a04b255660f17aaa0eaca92b7271c9a4818dc929) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The PS5 type-6 box, the compressed node Psr's builders write
> (psr_scatter_shared_exp_node_*), read from Psr's own kernels in the eboot:
> decoder 0x8ffdb60 (pc 0x1a84..0x1f70), child pointers 0x8ff3730
> (pc 0x2bc..0x4c0), encoder 0x8fad010 (pc 0xd9c..0x1258).
>
> Layout of the 64-byte node, in bits:
> - 0..28: child base, in 64-byte units.
> - 29..40: four 3-bit child types.
> - 41..48: four 2-bit child fields; 3 means an empty child.
> - 56/64/72: shared exponents x, y, z.
> - 80..511: per child 6 x 18-bit sign-magnitude bounds.
>
> Child pointers are implicit: ((base + units of earlier children) << 3) |
> type, where type 2 spans 4 units, types 3, 5 and 6 span 2, others 1.
>
> Bounds renormalise against the shared exponent and round outwards: minima
> towards -inf, maxima towards +inf, by filling the truncated bits.
>
> - psrBvh: DecodeBox(type 6), DecodeSharedExpBound, the Psr encoder
>   (EncodeSharedExpBound/EncodeSharedExpBox) for fixtures, type-6 node
>   counts in BLAS walks. The GLSL twin sits in rt_psr_common.inc, so the
>   GPU walker and the software/hybrid traces handle type 6 too.
> - Fixtures: shared_exp_interior builds type-6 nodes (siblings laid out
>   contiguously), for scenes whose interior children are all boxes.
> - Tests:
>   - 16 hand-worked bound vectors.
>   - Encode->decode over 20k random nodes: pointers and fields exact,
>     bounds conservative and within the quanta.
>   - A type-6 scene: CPU reference equals brute force; GPU software and
>     hybrid traces equal the CPU reference.
>   - --emit-type6-vectors writes bit-exact vectors for the instruction
>     lowering's tests.

</details>

Changed files: `src/graphics/host_gpu/rt/psrBvh.cpp`, `src/graphics/host_gpu/rt/psrBvh.h`, `src/graphics/host_gpu/shaders/rt_psr_common.inc`, `tests/RtHardwareTests.cpp`, `tests/RtPsrBvhBuilder.h`.

### 8. Shader: decode BVH intersections; KYTY_RT_STUB runs them as misses

Commit: [`79f39727`](https://github.com/Jetsku/KytyPS5-experimental/commit/79f397271683891d0d2061fbd68320f424c9d96a) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> IMAGE_BVH_INTERSECT_RAY and IMAGE_BVH64_INTERSECT_RAY (MIMG 0xe6/0xe7) now
> decode as real instructions: address dword counts from RDNA2 ISA Table 48
> (11/12, or 8/9 with A16), contiguous or NSA addresses, and the ISA 8.2.10
> restrictions (DMASK=0xf, R128=1, no D16/TFE/LWE, NSA long enough).
>
> Default behaviour is unchanged: the decoder still stops at the first BVH
> instruction, compute dispatches of such shaders are skipped and other
> stages fail in the CFG. New: the skipped shaders are now written to the
> shader dump (they were never compiled, so the dump missed them).
>
> KYTY_RT_STUB=1 (off by default) decodes the whole shader and translates
> each BVH instruction as "no intersection" for every lane, in RDNA2 return
> layout: a box node (type 4-7) gives four invalid child pointers
> (0xffffffff); a triangle node (type 0-3) gives t_num=+inf, t_denom=1.0 and
> zero dwords 2-3 (hit_status=0 in return mode 0). Astro Bot's traversal
> treats both as a miss. Each BVH shader is logged once, and the
> FrameEvent counters RtStubDispatches/RtStubDraws count the dispatches and
> draws that run a stubbed program (ShaderInfo::uses_bvh).
>
> Tests: shader_cfg_tests --ray-tracing-only (decode of the captured
> contiguous/NSA forms, BVH64, A16, rejected encodings, stub translation in
> CS and PS); shader_recompiler_compute_tests --bvh-only (GPU: both wave
> sizes, BVH/BVH64, A16, NSA, all node types).

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/descriptors.cpp`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/ShaderRecompiler.cpp`, `src/graphics/shader/recompiler/frontend/decode/ImageOps.cpp`, `src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.cpp`, `src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.h`, `src/graphics/shader/recompiler/frontend/translate/Memory.cpp`, `src/graphics/shader/recompiler/frontend/translate/RayTracing.cpp`, `src/graphics/shader/recompiler/frontend/translate/Translate.cpp`, `src/graphics/shader/recompiler/frontend/translate/Translator.h`, `src/graphics/shader/recompiler/ir/ShaderIR.h`, `tests/ShaderBvhTests.inc`, `tests/ShaderRayTracingTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`, `tests/shaderCfgTests.cpp`.

### 9. Tests: shader_cfg_tests --bvh-files translates raw guest BVH shaders with KYTY_RT_STUB

Commit: [`3352f37a`](https://github.com/Jetsku/KytyPS5-experimental/commit/3352f37adeb71d3a8f06a9391c0da97575edb8e6) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Development check, not part of the default run: decodes each raw shader
> binary with the BVH decoder, counts BVH/BVH64/unsupported instructions and
> translates it as a compute shader with the stub on.

</details>

Changed files: `tests/ShaderRayTracingTests.inc`, `tests/shaderCfgTests.cpp`.

### 10. Merge claude/rt-software: BVH instruction decode and KYTY_RT_STUB

Commit: [`05516cd8`](https://github.com/Jetsku/KytyPS5-experimental/commit/05516cd86c4237d919a3885d42e8275e850c08ad) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The BVH dump (KYTY_RT_DUMP_BVH) hooks the dispatches of programs that use BVH
> instructions (ShaderInfo::uses_bvh), which the stub introduces.

</details>

### 11. Hardware RT: KYTY_RT_DUMP_BVH dumps the game's Psr BVHs for offline analysis

Commit: [`18f4fbb6`](https://github.com/Jetsku/KytyPS5-experimental/commit/18f4fbb612206f791431274373a116fa3e81af7f) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> KYTY_RT_DUMP_BVH=<n>[,<n>...] (off by default, diagnostic): at the n-th dispatch
> of a program with BVH instructions (translated by KYTY_RT_STUB, or skipped
> without it), read the SRT from user SGPRs s[0:1], take the TLAS from SRT +0
> (lighting shaders) or +40 (GI shaders' TraceRayDescriptor::m_bvh), other
> offsets as a fallback, and dump that TLAS plus every BLAS its instances
> reference:
>
> - reads are coherent: GPU-written bytes are synchronized first
>   (SynchronizeGpuBackingForRead, TryReadGpuCleanBacking), and each page
>   records whether it was GPU-dirty;
> - the dump holds every page the psrBvh walkers read plus each BVH's declared
>   extent (header +88 nodes, +96 leaf records) and the SRT bytes;
> - output: <KYTY_RT_DUMP_BVH_DIR or shader log folder/bvh>/bvh_*.kbvh (sparse
>   pages plus a key=value manifest) and a .txt summary; one log line per dump;
> - the hook runs right after the compute program lookup, before bindings are
>   prepared; at most 1 GiB per dump; a dispatch without a plausible TLAS is
>   retried at the next BVH dispatch (at most 64 times per ordinal).
>
> rt_hardware_tests --bvh-dump <file> [--rays N] analyses a dump offline: the
> walks on the loaded pages only, leaf-list addressing and box-child
> containment per parent type (box16, box32, type 6), and reference traces with
> the guest's 60-entry stack, without it, and against brute force.
> --write-test-dump writes a synthetic dump for it. CPU tests cover the capture
> and the file round trip on two scenes.

</details>

Changed files: `CMakeLists.txt`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/host_gpu/renderer/rtBvhDump.cpp`, `src/graphics/host_gpu/renderer/rtBvhDump.h`, `src/graphics/host_gpu/rt/bvhDump.cpp`, `src/graphics/host_gpu/rt/bvhDump.h`, `tests/RtHardwareTests.cpp`.

### 12. Shader: KYTY_RT_SOFTWARE runs BVH intersections exactly in software

Commit: [`89492447`](https://github.com/Jetsku/KytyPS5-experimental/commit/894924477f454fabe6b88af8f225f6e4439a1f2c) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> IMAGE_BVH_INTERSECT_RAY / IMAGE_BVH64_INTERSECT_RAY (MIMG 0xe6/0xe7)
> become one IR value op, BvhIntersectRay. The SPIR-V backend lowers it
> (spirvEmitterRayTracing.cpp). Off by default; KYTY_RT_SOFTWARE=1 turns it
> on and takes precedence over KYTY_RT_STUB. Spec:
> Profiling/analysis/RT-SOFTWARE-DESIGN.md.
>
> - Operands, A16 and T# fields follow the RDNA2 ISA (8.2.10). The T# is
>   read as plain SGPR values, so a T# built inside the traversal loop
>   works. Nodes are fetched through the BDA page table: one lookup per
>   node, 16-byte loads when the device address is aligned.
> - Node tests follow AMD GPURT's emulation of the instruction in IEEE f32:
>   - Box16/box32: slab test, T# box growing, the sorting network.
>   - Triangle: the fan of four triangles, both return modes, barycentric
>     swizzle, and an exact integer division.
>   - Out-of-range nodes and inactive lanes.
> - Type 6 is the PS5 shared-exponent box (implicit child pointers,
>   outward-rounded 18-bit bounds). The layout and decode are RT-HW's
>   reading of Psr's eboot kernels. KYTY_RT_TYPE6=0 makes it miss like
>   RDNA2.
> - KYTY_RT_STUB now emits an opaque op (BvhIntersectRayStub). Constant
>   misses let the optimizer delete the game's hit handling.
> - BvhReference.h: a header-only CPU reference of one node test, with the
>   same operations in the same order.
>
> Tests (shader_recompiler_compute_tests):
> - --bvh-only checks the reference against hand results, and against
>   RT-HW's type-6 vectors.
> - It runs the GPU against the reference for box32/box16/triangles/
>   type 6, including a box32 split over discontiguous pages. Each runs in
>   wave32/64, BVH/BVH64, NSA and A16, with partial and zero EXEC.
> - --bvh-fuzz N [first]: differential fuzzing.
> - --bvh-bench: GPU cost per million node tests.
> - The reference follows the float mode the emulator declares on the
>   device.
> - shader_cfg_tests --bvh-files translates raw guest shaders as wave32
>   with both modes.

</details>

Changed files: `src/graphics/shader/recompiler/BvhReference.h`, `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/ShaderRecompiler.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInstructions.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterRayTracing.cpp`, `src/graphics/shader/recompiler/frontend/translate/RayTracing.cpp`, `src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.cpp`, `src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.inc`, `tests/ShaderBvhTests.inc`, `tests/ShaderBvhType6Vectors.inc`, `tests/ShaderRayTracingTests.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 13. Shader: expose the BVH node test as EmitBvhNodeTest

Commit: [`4d7eb58b`](https://github.com/Jetsku/KytyPS5-experimental/commit/4d7eb58b319a25de041a5ee61f83a505c4264507) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> - EmitBvhNodeTest(ctx, BvhOperands) (spirvEmitterRayTracing.h) emits
>   the instruction's node test for one lane from SPIR-V ids. It covers
>   every node type, the T# size check and the BDA node fetch, but does no
>   EXEC handling.
> - EmitBvhIntersectRay now reads its IR operands into BvhOperands and
>   calls EmitBvhNodeTest under EXEC.
> - The hardware ray-query path can call it per leaf entry and so run
>   exactly the software semantics.
> - Type-6 bounds: a zero magnitude now takes its exponent from the
>   nonzero path. FindUMsb(0) = -1 gives max(e - 17, 0), which saves two
>   selects per bound.
>
> No behaviour change: --bvh-only, --ray-tracing-only, 1,000 fuzz
> iterations and the compute suite pass.

</details>

Changed files: `src/graphics/shader/recompiler/backend/spirv/spirvEmitterRayTracing.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterRayTracing.h`.

### 14. Merge claude/rt-software: KYTY_RT_SOFTWARE and EmitBvhNodeTest

Commit: [`bf91d428`](https://github.com/Jetsku/KytyPS5-experimental/commit/bf91d42831eb4db32163efd79a7955a4ae90341f) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RT-SW's exact BVH instruction (BvhReference.h, the BvhIntersectRay IR op and
> its SPIR-V lowering, type-6 nodes) and the callable node-test emitter that
> the hardware path's leaf test will use.

</details>

### 15. Hardware RT: the instruction's exact triangle test as reference and leaf test

Commit: [`661fbf19`](https://github.com/Jetsku/KytyPS5-experimental/commit/661fbf1906229913ba97cd2309b0d4731b93e4e8) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Stage B of the hardware path. The CPU reference and the GPU test kernels now
> use RT-SW's exact IMAGE_BVH_INTERSECT_RAY triangle test instead of the
> prototype's own Moller-Trumbore:
>
> - psrBvh: ExactTriangleTest (BvhReference::Triangle, return mode 1, IEEE
>   denormals as on the RTX 3090, then the guest's use: hit unless t_num is
>   +inf, t = t_num * V_RCP_F32(t_den), facing from the sign bit of t_num) is
>   DefaultNodeTests().triangle; GuestTsharp builds the traversal's T#s;
>   TraceGuestExact runs every node through BvhReference::Intersect.
> - rt_psr_common.inc: bit-exact GLSL twins: PsrDivBits (correctly rounded f32
>   division with integer arithmetic), PsrTriangleExact, and PsrTriangleMiss,
>   which decides from an approximate quotient when it is clear of every
>   threshold (GLSL division is within 2.5 ULP) and divides exactly only near
>   one; results are identical and the hybrid keeps most of its speed.
> - rt_psr_exact_test.comp probes the twins: 229,700 division cases and
>   131,072 triangle cases (random, vertex, edge, just outside, degenerate,
>   parallel, zero direction components), 0 differ.
> - Tests add rays aimed exactly at triangle vertices and edges. The GPU
>   software trace equals the CPU reference; the hybrid equals it except where
>   the reference's box tests reject a grazing hit, and there it equals brute
>   force (0 real mismatches). A traversal with the instruction's own box
>   tests differs from the hybrid on 0 random rays and on up to 67 of 20,000
>   edge-aimed rays. Triangle mode is checked on random rays only.

</details>

Changed files: `CMakeLists.txt`, `src/graphics/host_gpu/rt/psrBvh.cpp`, `src/graphics/host_gpu/rt/psrBvh.h`, `src/graphics/host_gpu/shaders/rt_psr_common.inc`, `src/graphics/host_gpu/shaders/rt_psr_exact_test.comp`, `tests/RtHardwareTests.cpp`.

### 16. Merge claude/rt-hardware: BVH dump, exact triangle test, RT-SW's KYTY_RT_STUB/KYTY_RT_SOFTWARE

Commit: [`82892236`](https://github.com/Jetsku/KytyPS5-experimental/commit/828922362cb30066ac5c04da18bc3dfb0e7408c5) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RT-HW's branch (661fbf19) with RT-SW's commits (79f39727, 3352f37a,
> 89492447, 4d7eb58b) for the joint RT run: KYTY_RT_DUMP_BVH, the exact BVH
> instruction in software, EmitBvhNodeTest and the rt_hardware_tests target.
>
> Conflicts were additions on both sides, resolved to keep both:
> - CodegenOptions.{h,cpp}: the rt_* switches after srt_variant_reads/bda_writes;
> - ShaderIR.h: uses_bvh after bda_writes;
> - ShaderRecompiler.cpp: <string_view>;
> - renderCompute.cpp (direct and indirect dispatch): the PrepareBdaWrites block
>   and the uses_bvh profiler block, each with its own closing brace;
> - ShaderRecompilerComputeTests.cpp: both includes (<filesystem>, <functional>),
>   both device queries (DenormFlushF32Supported, DeviceFloatControls), both test
>   includes, and one timing path for the two benchmark interfaces:
>   repeats/elapsed_us (per dispatch) and bench_repeats/bench_last_ns (total),
>   with bench_repeats deciding the count when set.

</details>

### 17. Merge claude/rt-gi-probes: KYTY_PS_LIVE_EXEC and the append election by default

Commit: [`db316211`](https://github.com/Jetsku/KytyPS5-experimental/commit/db31621119ca29f7173f737e05afe01af8bf21ea) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RT-GI's a5195147: pixel DS_APPEND/DS_CONSUME without helper lanes by default,
> which the GI path needs with both mods off.

</details>

### 18. Merge codex/astro-profile (U53) into claude/rt-software

Commit: [`3803886c`](https://github.com/Jetsku/KytyPS5-experimental/commit/3803886c8d9639055866a84f1afa46f8fa19ee2c) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Brings in RT-GI's loop guard (KYTY_LOOP_GUARD) and RT-TILED's
> KYTY_SRT_VARIANT_READS so the BVH traversal budget can build on the loop
> guard. Conflicts were additive:
> - CodegenOptions: both sides' switches.
> - Test harness: both float-control helpers. The integration branch's
>   dispatch timing (repeats/elapsed_us) replaces RT-SW's bench_repeats,
>   and BvhTests::TimeCase uses it.
> - Test includes: all three test files.
>
> shader_cfg_tests --bvh-files prints why a shader failed (skipped, block
> and BVH-op counts).
>
> Tests on the merged tree: the full compute suite, --bvh-only and
> shader_cfg_tests --ray-tracing-only pass. The full shader_cfg_tests run
> stops only at the known "plain 2D sample" failure.

</details>

### 19. Shader: KYTY_RT_NODE_BUDGET ends runaway BVH traversals

Commit: [`7f7f300f`](https://github.com/Jetsku/KytyPS5-experimental/commit/7f7f300fbfe6862229d9ad21c82d973c2dc2fe0f) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> A garbage or cyclic BVH can keep a guest traversal looping forever and
> lose the device. The first in-game BVHs may be partly wrong. With
> KYTY_RT_SOFTWARE, every invocation now counts its BVH instruction
> executions. Every invocation of a wave executes the instruction for each
> node the wave's packet traversal visits, whatever EXEC holds, so the
> count is the wave's traversal length.
>
> - KYTY_RT_NODE_BUDGET=<n>: on by default at 8192 node tests per guest
>   lane; 0 turns it off. Past the budget:
>   - every node test misses without reading memory (a box has no hit
>     children, a triangle is not hit);
>   - the invocation takes each loop's exit edge, through RT-GI's
>     KYTY_LOOP_GUARD mechanism. This also ends loops the instruction does
>     not drive, such as a leaf-entry loop over a zeroed list;
>   - at return it adds one to GDS dword end - 2. The command processor
>     reports that count at flips ("RT node budget").
> - KYTY_RT_NODE_STATS=1: at return each invocation that ran node tests adds
>   one to a log2 histogram bin of its per-lane count (GDS dwords
>   end - 3 - k). It is reported every 300 flips, to check the default on
>   real frames.
> - A program with BVH ops binds GDS whenever either switch is on. The count
>   is a Function variable, and both halves of a two-lane invocation add to
>   it.
>
> Tests (--bvh-only):
> - A cyclic box ends at the budget: the (budget + 1)-th test misses and a
>   following scalar loop is left at once. Each invocation reports once.
> - With the statistics, a 5-node walk lands in bin [4, 8).
> - Both run in wave32 and wave64.
> - The count and its GDS binding exist only with a switch on.
> - Every ordinary case checks that the default budget never fires.
>
> Also: shader_cfg_tests --bvh-files translates at IR wave size 32. Setting
> only the compute input info left CompileOptions::wave_size at 64, so
> kernels that use vcc_lo/vcc_hi as scalar temporaries were mistranslated.

</details>

Changed files: `src/graphics/guest_gpu/graphicsRun.cpp`, `src/graphics/shader/recompiler/CodegenOptions.cpp`, `src/graphics/shader/recompiler/CodegenOptions.h`, `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterProgram.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterRayTracing.cpp`, `src/graphics/shader/recompiler/ir/passes/BindingLayout.cpp`, `src/graphics/shader/recompiler/ir/passes/BindingLayout.h`, `tests/ShaderBvhTests.inc`, `tests/ShaderRayTracingTests.inc`.

### 20. Merge claude/rt-software: KYTY_RT_NODE_BUDGET and KYTY_RT_NODE_STATS

Commit: [`84a40766`](https://github.com/Jetsku/KytyPS5-experimental/commit/84a40766b93f0b185b2af4d00be3ad23b04a24e3) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RT-SW's 7f7f300f (with its U53 merge 3803886c): a per-lane budget of BVH node
> tests that ends runaway traversals (on by default with KYTY_RT_SOFTWARE, 8192)
> and its node-count histogram.
>
> Conflicts come from RT-SW's own resolution of U53:
> - CodegenOptions.{h,cpp}: bda_writes kept, the budget and stats options added
>   after the rt_* switches.
> - ShaderRecompilerComputeTests.cpp: RT-SW dropped bench_repeats/bench_last_ns
>   and its BVH benchmark now uses mainline's Dispatch(..., repeats, &elapsed_us),
>   so the Dispatch helper's timing is mainline's again; the two device queries
>   follow RT-SW's order; the test includes keep all four.

</details>

### 21. KYTY_RT_DUMP_BVH: '/' separates dispatch ordinals too

Commit: [`31f805c0`](https://github.com/Jetsku/KytyPS5-experimental/commit/31f805c0a7f49367f320381f65db5b4d6aa18bf3) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Start-Comparison's -ExtraEnv rejects ',' and ';' inside a value, so
> KYTY_RT_DUMP_BVH=1,500,5000 could not be passed; 1/500/5000 now means the
> same (',', ';' and spaces still work). The parser moves to
> Rt::ParseDumpOrdinals, which also no longer takes a signed token (-3
> wrapped around to a huge ordinal). rt_hardware_tests covers it.

</details>

Changed files: `src/graphics/host_gpu/renderer/rtBvhDump.cpp`, `src/graphics/host_gpu/renderer/rtBvhDump.h`, `src/graphics/host_gpu/rt/bvhDump.cpp`, `src/graphics/host_gpu/rt/bvhDump.h`, `tests/RtHardwareTests.cpp`.

### 22. Shader: triangle miss decisions skip the exact division away from the thresholds

Commit: [`36cb325b`](https://github.com/Jetsku/KytyPS5-experimental/commit/36cb325b99a6f9aa2ce7928967b7b85d384ee2b6) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The triangle test needs its quotients t, u and v only for the miss
> decision: the signs of t, u and v, and where u and u + v lie against 1.
> Every quotient went through the exact integer division, a 64-bit integer
> divide that is slow on NVIDIA. RT-HW measured its leaf test falling from
> ~1,000 to ~260 Mrays/s because of it.
>
> This adopts RT-HW's PsrTriangleMiss. Vulkan bounds OpFDiv by 2.5 ulp for a
> divisor in [2^-126, 2^126], so num * (1 / t_den) is within about 3.5 ulp
> (2^-21.2 relative) of the correctly rounded quotient. Those
> approximations decide exactly as the correctly rounded quotients when:
> - |t_den| is a normal in [2^-126, 2^126], and every approximation is
>   finite;
> - |t|, |u| and |v| are above 2^-100, so no quotient rounds to a zero;
> - |u - 1| > 2^-18 and |u + v - 1| > 2^-18 (1 + |u| + |v|).
> Otherwise the exact divisions decide, as before.
>
> The fuzz now aims 40% of its triangle rays within 2^-26..2^-12 of an edge
> (u = 0, v = 0, u + v = 1) or of u = 1, on either side. Results stay
> bit-identical to BvhReference:
> - --bvh-only passes;
> - shader_cfg_tests --ray-tracing-only passes, including spirv-val of the
>   lowering;
> - 6,000 fuzz iterations pass, 3,000 of them with the near-edge rays.

</details>

Changed files: `src/graphics/shader/recompiler/backend/spirv/spirvEmitterRayTracing.cpp`, `tests/ShaderBvhTests.inc`.

### 23. Merge claude/rt-software: exact triangle miss decisions without divisions away from thresholds

Commit: [`59a87993`](https://github.com/Jetsku/KytyPS5-experimental/commit/59a8799384cbffa45150660cadfc77ff29485ff8) · **Integration merge**

Integrates the named parent work and any conflict resolution. Read its parent commits and merge diff; no independent speedup is assigned to the act of merging.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> RT-SW's 36cb325b: the SPIR-V triangle test decides a miss from an
> approximate quotient when it is clear of every threshold and divides
> exactly only near one (the PsrTriangleMiss rule). Results are unchanged.

</details>

### 24. CFG: give a loop's break-region conditional a selection merge

Commit: [`1f4cec88`](https://github.com/Jetsku/KytyPS5-experimental/commit/1f4cec881a4d4e6b5b1c6650715e58a795442864) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The RT data run R1 (rt-joint db316211, KYTY_RT_STUB=1,
> KYTY_SRT_VARIANT_READS=1) exited on CS 0x380bb9d636390bae (TraceRadiance):
> "SPIR-V validation failed". RT-TILED reproduced the validator's message,
> "Selection must be structured: OpBranchConditional", and found the
> shape.
>
> Every Astro Bot BVH traversal has it: 27 of 197 cs_all kernels, in both
> wave sizes. The shape is:
> - The first body block of the traversal loop either stays in the loop or
>   enters a region outside the natural loop.
> - That region is not the loop's merge or continue target. It holds its
>   own loop and leaves through the loop's merge.
> - The body has another exit.
>
> IsInnermostLoopControlConditional counts such a conditional as loop
> control, so it got no merge. In SPIR-V the region lies inside the loop
> construct, so the branch is a selection.
>
> StructurizeImpl now ends with StructureBreakRegions:
> - For each such conditional it follows the region's spine from its entry,
>   through the merges its selections and loops already have, to the block
>   that breaks to the loop merge.
> - A synthetic block inserted on that edge, before the loop merge, becomes
>   the conditional's merge.
> - Other exits of the region stay breaks.
> - A region without such a spine fails structurization, as any other
>   unstructured CFG, and so falls back to the dispatcher instead of
>   emitting invalid SPIR-V.
>
> This applies only to CFGs that emitted invalid SPIR-V before. RT-TILED's
> equivalent prototype left all 170 valid cs_all and 332/363 Psr modules
> bit-identical.
>
> Tests:
> - shader_cfg_tests --ray-tracing-only: the traversal-loop shape gets the
>   synthetic merge, and its compute and pixel SPIR-V validates. Without
>   the fix the test fails.
> - --bvh-only: the same program runs on the GPU in wave32 and wave64 and
>   gives the loop counts.
> - shader_cfg_tests --bvh-validate <file|@list>: a new harness that
>   compiles raw guest shaders the way the emulator does. It materializes
>   against synthetic memory and runs spirv-val with Vulkan 1.3. The codegen
>   switches come from the environment; KYTY_HARNESS_WAVE and
>   KYTY_HARNESS_RT_MODES select waves and BVH modes.
>   - With the fix, under A1 (stub + variant reads) and B (software +
>     variant reads + BDA writes + node stats), at wave32 and wave64, it
>     finds no invalid module in the 36 RT shaders (stub and software
>     modes), the 26 tiled_rt and 7 gi_trace kernels, or the 27 formerly
>     invalid cs_all kernels.
>   - The only skips are SRT-variant-read skips: the 4 wave32 tiled kernels
>     at wave64, and 092754e7 (pc 0x16c0).

</details>

Changed files: `src/graphics/shader/recompiler/frontend/cfg/ShaderCFG.cpp`, `tests/ShaderBvhTests.inc`, `tests/ShaderRayTracingTests.inc`, `tests/shaderCfgTests.cpp`.

### 25. SRT: a flat read of an unmapped address reads 0 instead of faulting

Commit: [`9bf0cc2d`](https://github.com/Jetsku/KytyPS5-experimental/commit/9bf0cc2d930caacd59cfc6d78b1fd982d032d881) · **Support/correctness change**

The subject and linked diff identify the exact change. No isolated performance measurement is available; do not count it as an additional FPS gain. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> R2 (launch B) crashed at the first dispatch of Astro Bot's tiled lighting, 78af8e269b528b5c. The
> fault was an access violation reading 0x30 in SrtWalker::ReadRawWord's in-place fallback.
>
> The shader loads a TLAS pointer from its SRT (s_load_dwordx2 s[8:9], s[0:1], 0), then header
> fields through it: dword +0x18 (a trap bit), qwords +0x30 and +0x58. These loads sit behind
> S_CBRANCH_EXECZ, on the path that only lanes tracing a shadow ray take. While no TLAS exists the
> pointer is null, and on hardware the loads are never issued. Kyty evaluates every flat SRT read of
> the plan before the dispatch, whatever path the shader will take. The probe correctly refused
> null + 0x30, and the in-place memcpy then touched the address. No guest mapping covers it, so the
> fault handler has nothing to resolve and the emulator stops. Before KYTY_SRT_VARIANT_READS, this
> program never got as far as materializing its plan.
>
> Such reads now return 0, as the GPU reads an unmapped page, and the dispatch runs:
> - The in-place fallback never touches an address that no mapping can contain: the first 64 KiB,
>   or a non-canonical address. Ungated: this changes only reads that fault with nothing to resolve
>   them.
> - With KYTY_SRT_VARIANT_READS, the pipeline cache also passes SrtRuntime::is_guest_mapped
>   (LibKernel::Memory::IsGpuMapped, the renderer's mapped ranges, which its fault handler
>   resolves). Any in-place read outside the guest mappings then reads 0, for example through a
>   garbage pointer.
> - The strict (specialization) reader, ReadShaderGuestMemory, does the same after a failed backing
>   read. That failure could not be synchronized away and ended in an EXIT.
> - A zero read is not certifiable, so a prepared-read capture that includes one is rejected.
> - Each walker logs one console line per shader with the address, and the strict reader logs its
>   first eight. FrameEvent SrtUnmappedReads counts both.
>
> Dropping the dispatch instead would lose the whole tiled-lighting pass for a read that its
> shader does not perform. A zero descriptor or pointer is what an unmapped read yields on the GPU.
>
> Test (ctest srt_variant_reads, CPU only), NullBasedFlatReadReadsZero, with and without the switch:
> - A plan follows a null pointer to null + 0x30 with the game's in-place reader: it materializes
>   and the slots read 0. Without the fix the process dies.
> - A mapped pointer still reads its data, with and without is_guest_mapped.
> - A pointer outside the guest mappings is refused through is_guest_mapped and reads 0.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.cpp`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.h`, `src/kernel/memory.cpp`, `src/kernel/memory.h`, `tests/ShaderSrtVariantTests.inc`.

### 26. Shaders: S_BREV_B64

Commit: [`1cc0411e`](https://github.com/Jetsku/KytyPS5-experimental/commit/1cc0411e341325b589df47f267aaeb80684ee860) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> SOP1 0x0c, RDNA2 ISA: D.u64[63 - i] = S0.u64[i]; SCC is not written. Both source halves are read
> before either destination half is written, so a destination that overlaps the source is exact.
> The source is read like every other 64-bit scalar source: a 32-bit literal is zero-extended and an
> integer inline constant is sign-extended.
>
> Psr's decompress_refit_hierarchy_trinity uses s_brev_b64 s[10:11], 0x20468000 to build a 64-bit
> constant whose low half is zero. It was the one Psr kernel outside the 96 shader-mesh builders
> that stopped at an unimplemented instruction. A decode-only scan of 715 guest shaders (197 compute,
> 460 Psr, 58 ray-tracing) finds no other unimplemented instruction apart from the shader-mesh
> builders' S_SWAPPC_B64.
>
> Tests: ScalarBrevB64 runs on the GPU against a bit-by-bit reference. It covers an SGPR pair, the
> pair as its own destination, the literal form above, and the inline constants 1, -1 and -2, and
> checks that SCC stays set and stays clear across the instruction. Under launch B's switches the
> trinity kernel now translates and validates at wave32 and wave64. Every other shader in the
> corpora compiles to bit-identical SPIR-V under the default, A1 and B switches.

</details>

Changed files: `src/graphics/shader/recompiler/frontend/decode/ScalarAluOps.cpp`, `src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.cpp`, `src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.h`, `src/graphics/shader/recompiler/frontend/translate/Integer.cpp`, `src/graphics/shader/recompiler/frontend/translate/Scalar.cpp`, `src/graphics/shader/recompiler/frontend/translate/Translator.h`, `tests/ShaderRecompilerComputeTests.cpp`.

### 27. Shaders: name and dump the callees of a program skipped for S_SWAPPC_B64

Commit: [`e5dd1a4b`](https://github.com/Jetsku/KytyPS5-experimental/commit/e5dd1a4b782ee363d14504f1c74eea77bfbc1ddc) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> With KYTY_SRT_VARIANT_READS, a program that calls a function through S_SWAPPC_B64 is skipped with
> one console line. Psr's 96 shader-mesh BLAS builders are such programs: they call game callbacks
> whose code is not in the eboot. Launch B has to show whether the game uses them and, if so, give us
> the callees. So the skip path now reports where the calls go and keeps the callee code.
>
> - **Finding the targets.** TranslateProgram traces each call's target pair back through the code
>   before it. It accepts copies of user SGPRs (S_MOV_B32 per half, S_MOV_B64) that nothing
>   overwrote before the copy. Across control flow it accepts only an entry-block copy that every
>   other write of the half repeats. A call with no such copy counts as unresolved.
> - **What is recorded.** The addresses come from the dispatch's user data and go into
>   TranslateResult::call_targets. The skip line lists them, for example "callees from user data:
>   s[8:9]=0x…, s[12:13]=0x…".
> - **Corpus check.** With synthetic user data, every call in the 94 distinct shader-mesh builders
>   resolves this way to one or two user-data callback pairs.
> - **The dump.** With the shader dump on, the pipeline cache's skip branch saves each callee as
>   original/callee_<stage>_<program hash>_<address>.bin, next to the program's own dump. The file
>   holds the code from the first instruction up to and including the return through the link pair
>   (S_SETPC_B64, or S_SWAPPC_B64 with a null destination), at most 4 KiB.
> - **Reading guest memory safely.** The code is read through the clean backing, or in place
>   inside the GPU-mapped ranges. An address outside them, or null, logs a line and is not
>   dumped. Each dumped callee logs its size and whether its return was found.
>
> Nothing changes on a path that is not skipped. SPIR-V is bit-identical across the corpora under
> the default, A1 and B switches.
>
> Tests (ctest srt_variant_reads):
> - CallTargetsFromUserData (CPU) checks the resolved callees for:
>   - copies before each call, and a call directly through user SGPRs;
>   - a target computed from a copy, which is not reported;
>   - an entry-block copy that reaches a call behind a branch target;
>   - a copy on one path only, which is not reported.
> - CallTargetDump (GPU, the real pipeline cache with the dump on) checks that:
>   - a callee in guest memory is dumped exactly up to its return, and the words after it are not;
>   - an unmapped callee and a null one are skipped without a dump.

</details>

Changed files: `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/shader/recompiler/ShaderRecompiler.cpp`, `src/graphics/shader/recompiler/ShaderRecompiler.h`, `tests/ShaderRecompilerComputeTests.cpp`, `tests/ShaderSrtVariantTests.inc`.

### 28. Detect cyclic BVH walks and reject incomplete offline scenes

Commit: [`a6efeceb`](https://github.com/Jetsku/KytyPS5-experimental/commit/a6efecebdc75a6ab765e80ccfcbe3f1b2da4c5a3) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

Changed files: `src/graphics/host_gpu/rt/psrBvh.cpp`, `src/graphics/host_gpu/rt/psrBvh.h`, `tests/RtHardwareTests.cpp`.

### 29. KYTY_BDA_WRITES: count guest writes that race a settle, and assert uses_dma

Commit: [`45065df8`](https://github.com/Jetsku/KytyPS5-experimental/commit/45065df85c5bc68d5317363791a609e4218c3f8a) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> The CPU agent's review of 868ec21e:
>
> - Both dispatch paths assert that a BDA-writing program also uses DMA. The settle relies on
>   PrepareBda having uploaded every CPU-dirty page before the dispatch.
> - The settle counts written pages that are CPU-dirty when it runs, as FrameEvent
>   BdaSettleCpuDirtyPages (16 KiB pages). It logs one line per shader, and KYTY_BDA_WRITES=verify
>   makes them fatal. Such a page was written by the guest while the dispatch was recorded or ran.
>   The written upload then puts the guest's page over the dispatch's bytes, where hardware keeps
>   both writers' bytes.
> - CollectBdaWrites clears and flushes only the count before the compaction. Afterwards it
>   invalidates only the count, the entries the count covers and the dropped-write counter, instead
>   of the whole 512 KiB download area.
>
> Hot pages (KYTY_HOT_PAGES, on by default) are CPU-dirty by construction, and the settle's written
> upload would put their guest copy over the dispatch's bytes. They were already safe: these
> programs set has_address_writes, so CommitBindings calls InvalidateContentRevisions, which returns
> every unchanged hot page to clean tracking before the dispatch. A build without that settle shows
> it. The BdaWritesSettle test gains a second dispatch that writes a page made hot by write faults
> in consecutive frames. A guest read afterwards must see both the dispatch's bytes and the guest's
> earlier write.
>
> Nothing changes with KYTY_BDA_WRITES unset.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/cache/bufferCache.cpp`, `src/graphics/host_gpu/renderer/cache/faultManager.cpp`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 30. Shaders: skip a program whose SRT reads follow a loop-carried pointer instead of exiting

Commit: [`b0bc3f85`](https://github.com/Jetsku/KytyPS5-experimental/commit/b0bc3f8542e97fe9ec590b7ec9709af0bc5c9aad) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Without KYTY_SRT_VARIANT_READS, a flat SRT read through a loop-carried pointer can never be
> evaluated before the dispatch: a BVH traversal's instance record, for example GI's TraceRadiance
> (380bb9d6) under KYTY_RT_STUB. Every materialization of such a plan fails with no guest range to
> synchronize, and ProgramCache::Materialize exited the emulator. That exit ended the first A1 run.
>
> Materialize now asks FindVariantFlatRead whether one of the plan's flat reads has an address that
> depends on a phi ResolveInvariantPhi cannot reduce. If so, and the switch is off, it marks the
> source skip_dispatch, which is atomic and set without the programs lock. The pipeline cache skips
> the program's dispatches and draws as it does for programs skipped at translation, and prints one
> console line per shader with the read's pc. FrameEvent VariantPlanSkips counts the sources.
> Every other failed materialization without missing reads still exits, and a plan that
> materializes never reaches the check.
>
> Tests (ctest srt_variant_reads):
> - VariantPlanIsFound: without the switch, FindVariantFlatRead names one of the loop's SMEM reads
>   in the record-walk shader, and its plan does not materialize. A pointer loaded once and followed
>   outside any loop is not reported.
> - VariantPlanSkipped runs the record walk through the real pipeline cache and RenderExecutor.
>   Without the switch, two dispatches are skipped and store nothing, where before the first one
>   exited. With the switch, a fresh context runs the walk through BDA and stores both records'
>   values.

</details>

Changed files: `src/common/profiler.cpp`, `src/common/profiler.h`, `src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.cpp`, `src/graphics/shader/recompiler/ir/passes/SrtWalker.h`, `tests/ShaderRecompilerComputeTests.cpp`, `tests/ShaderSrtVariantTests.inc`.

### 31. Exercise BDA settling across synchronization epochs and strict verification

Commit: [`bf29c2e7`](https://github.com/Jetsku/KytyPS5-experimental/commit/bf29c2e78164a82853bfb6b6f77191992508b76c) · **Memory coherence and cached proofs**

Reduces repeated range/page work or corrects which copy owns the bytes. Cached answers are valid only until the relevant writes/epochs change; coherence fixes prevent stale-data reuse. RT-specific limits apply; no game hardware-RT speedup is established.

Changed files: `CMakeLists.txt`, `docs/rt-offline-validation.md`, `tests/ShaderRecompilerComputeTests.cpp`.

### 32. Add opt-in host input controls for unattended testing

Commit: [`bc64dba0`](https://github.com/Jetsku/KytyPS5-experimental/commit/bc64dba09885eae62a7350d9dce54902b5205e57) · **Workflow and compatibility controls**

Supports controlled launches, navigation or documentation. This does not optimize rendering; private operational documents are excluded from the publication. RT-specific limits apply; no game hardware-RT speedup is established.

Changed files: `CMakeLists.txt`, `docs/host-input-testing.md`, `src/common/hostInputTrace.h`, `src/graphics/presentation/window/hostInput.cpp`, `src/graphics/presentation/window/hostInputPulse.h`, `src/libs/controller.cpp`, `tests/HostInputPulseTests.cpp`.

### 33. Fix compact Psr BVH decoding and target level RT captures

Commit: [`69745bd6`](https://github.com/Jetsku/KytyPS5-experimental/commit/69745bd687756875fcb845fd50aa725d77ec87e6) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

Changed files: `src/graphics/host_gpu/renderer/rtBvhDump.cpp`, `src/graphics/host_gpu/renderer/rtBvhDump.h`, `src/graphics/host_gpu/rt/bvhDump.cpp`, `src/graphics/host_gpu/rt/bvhDump.h`, `src/graphics/host_gpu/rt/psrBvh.cpp`, `src/graphics/host_gpu/rt/psrBvh.h`, `tests/RtHardwareTests.cpp`.

### 34. WIP: snapshot native RT scene preparation and device startup integration

Commit: [`8652c44a`](https://github.com/Jetsku/KytyPS5-experimental/commit/8652c44ada55574fa0eae938cd3c2f3e2a4c02d3) · **RT decode, traversal and integration**

Implements the named hierarchy, traversal or native-device step. Native traversal can reduce ALU work, but snapshot validity, exact ray behavior and game integration remain prerequisites; this is research code, not an established game acceleration. RT-specific limits apply; no game hardware-RT speedup is established.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Publication checkpoint of existing working changes. Guest traversal remains software; no Astro Bot hardware RT speedup is established. Not newly built or tested for publication.

</details>

Changed files: `CMakeLists.txt`, `src/emulator.cpp`, `src/graphics/host_gpu/graphicContext.h`, `src/graphics/host_gpu/renderer/renderCompute.cpp`, `src/graphics/host_gpu/renderer/rtBvhDump.cpp`, `src/graphics/host_gpu/renderer/rtBvhDump.h`, `src/graphics/host_gpu/rt/rtAccel.cpp`, `src/graphics/host_gpu/rt/rtAccel.h`, `src/graphics/host_gpu/rt/rtAllocation.cpp`, `src/graphics/host_gpu/rt/rtAllocation.h`, `src/graphics/host_gpu/rt/rtScene.cpp`, `src/graphics/host_gpu/rt/rtScene.h`, `src/graphics/host_gpu/shaders/rt_psr_common.inc`, `src/graphics/presentation/window/vulkanWindow.cpp`, `src/graphics/shader/recompiler/PsrTraversal.cpp`, `src/graphics/shader/recompiler/PsrTraversal.h`, `src/loader/runtimeLinker.cpp`, `tests/RtDeviceStartupProbe.h`, `tests/RtHardwareTests.cpp`, `tests/RtPsrBvhBuilder.h`, `tests/RtTraversalTests.cpp`.

## Demon's Souls additions

7 commits (including integration merges).

### 1. shader: carry the material key immediate through invariant indirect image tables

Commit: [`098990bf`](https://github.com/Jetsku/KytyPS5-experimental/commit/098990bfd8b558322f5ba811472fc9615b6b78ab) · **Image ownership, allocation and GPU dependencies**

Targets the named image lookup, lifetime, copy or dependency. Reuse is conditional on matching content/ownership; required barriers and clears cannot be skipped for speed. This remains on the separate game branch; no new timing result was produced for publication.

<details><summary>Original commit rationale (historical claims; not a fresh benchmark)</summary>

> Ports upstream PR #500 (92219c71): a scalar-buffer material key read at a
> nonzero immediate offset was rejected, leaving the image handle unplanned and
> ending in 'GetImageResource dword 0 is not a valid runtime value' (Demon's
> Souls compute 0x9fb5cc27). Carry the immediate into the probe and accept raw
> (stride 0) material buffers.

</details>

Changed files: `src/graphics/shader/recompiler/ir/ShaderIR.h`, `src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.cpp`, `src/graphics/shader/recompiler/ir/passes/ResourceTracking.cpp`.

### 2. shader: fold thread bits of constant all-zero and all-one masks

Commit: [`a597947d`](https://github.com/Jetsku/KytyPS5-experimental/commit/a597947df079cd66436f12e01f2f97109377ff9f) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. This remains on the separate game branch; no new timing result was produced for publication.

Changed files: `src/graphics/shader/recompiler/frontend/translate/Translate.cpp`.

### 3. shader: remove phi webs with no non-phi consumer

Commit: [`91748d9f`](https://github.com/Jetsku/KytyPS5-experimental/commit/91748d9f79b887866d26420e4d998e4bf66fa011) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. This remains on the separate game branch; no new timing result was produced for publication.

Changed files: `src/graphics/shader/recompiler/ir/passes/DeadCodeElimination.cpp`.

### 4. tests: add a selector that runs only the shader cases

Commit: [`2ed6b36a`](https://github.com/Jetsku/KytyPS5-experimental/commit/2ed6b36abd8fc4652dda3bde197c7abd3f099f6e) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain. This remains on the separate game branch; no new timing result was produced for publication.

Changed files: `tests/ShaderRecompilerComputeTests.cpp`.

### 5. shader: branch on the whole wave for execz, execnz, vccz and vccnz

Commit: [`d9b88645`](https://github.com/Jetsku/KytyPS5-experimental/commit/d9b88645d9eb5f3d87f914fa0625ac8176a89581) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. This remains on the separate game branch; no new timing result was produced for publication.

Changed files: `src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterFlow.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInstructions.h`, `src/graphics/shader/recompiler/frontend/translate/Translate.cpp`, `src/graphics/shader/recompiler/ir/IREmitter.cpp`, `src/graphics/shader/recompiler/ir/IREmitter.h`, `src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.inc`, `tests/ShaderRecompilerComputeTests.cpp`.

### 6. shader: convert and pack typed buffer stores

Commit: [`259bb8b8`](https://github.com/Jetsku/KytyPS5-experimental/commit/259bb8b83ba35c1158d7fcdcf4f49c1bd6ab2b31) · **Shader translation and ISA behavior**

Implements the named instruction/semantic correction or simplifies equivalent generated work. Accuracy changes need not be faster, and no per-instruction gameplay gain is claimed. This remains on the separate game branch; no new timing result was produced for publication.

Changed files: `src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp`, `src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemoryHelpers.cpp`, `tests/ShaderRecompilerComputeTests.cpp`.

### 7. tests: expect the material key immediate to be carried through invariant indirect images

Commit: [`62cec08a`](https://github.com/Jetsku/KytyPS5-experimental/commit/62cec08a5780b59834047e55d730e1badc214f23) · **Tests/build**

Covers the named behavior or wires its regression target. It provides validation infrastructure, not a direct runtime performance gain. This remains on the separate game branch; no new timing result was produced for publication.

Changed files: `tests/ResourceTrackingTests.cpp`.

## Complete U59 file delta from shared base

Added/modified/deleted paths are listed below. This includes implementation, tests,
build integration and documentation; submodule entries preserve their pinned IDs.

```text
M	CMakeLists.txt
A	docs/host-input-testing.md
A	docs/metadata-clear-eliminate.md
A	src/codegen_version.cmake
M	src/common/bitArray.h
A	src/common/cpuPlacement.cpp
A	src/common/cpuPlacement.h
A	src/common/hangTrace.cpp
A	src/common/hangTrace.h
M	src/common/hostException.cpp
M	src/common/hostException.h
A	src/common/hostInputTrace.h
M	src/common/lruCache.h
M	src/common/profiler.cpp
M	src/common/profiler.h
A	src/common/rendererBatch.h
M	src/common/threads.cpp
M	src/common/threads.h
M	src/graphics/guest_gpu/command_processor/commandProcessor.h
A	src/graphics/guest_gpu/command_processor/cpOps.cpp
A	src/graphics/guest_gpu/command_processor/cpOps.h
A	src/graphics/guest_gpu/command_processor/cpSequencer.cpp
A	src/graphics/guest_gpu/command_processor/cpSequencer.h
A	src/graphics/guest_gpu/command_processor/cpVerify.cpp
A	src/graphics/guest_gpu/command_processor/cpVerify.h
M	src/graphics/guest_gpu/command_processor/pm4Handlers.cpp
M	src/graphics/guest_gpu/graphicsRun.cpp
M	src/graphics/guest_gpu/graphicsRun.h
M	src/graphics/guest_gpu/hardwareContext.h
M	src/graphics/guest_gpu/tile.cpp
M	src/graphics/guest_gpu/tile.h
A	src/graphics/host_gpu/cleanVerdictCache.h
A	src/graphics/host_gpu/coherenceLog.h
A	src/graphics/host_gpu/eagerReadbackPages.h
A	src/graphics/host_gpu/gpuReadDelegate.h
A	src/graphics/host_gpu/gpuTouchedPages.h
M	src/graphics/host_gpu/graphicContext.h
A	src/graphics/host_gpu/memoryStats.h
M	src/graphics/host_gpu/memoryTracker.cpp
M	src/graphics/host_gpu/memoryTracker.h
M	src/graphics/host_gpu/pageManager.cpp
M	src/graphics/host_gpu/pageManager.h
A	src/graphics/host_gpu/parkingLock.h
A	src/graphics/host_gpu/queueSubmission.cpp
A	src/graphics/host_gpu/queueSubmission.h
M	src/graphics/host_gpu/rangeSet.h
M	src/graphics/host_gpu/regionManager.h
M	src/graphics/host_gpu/renderer/cache/bufferCache.cpp
M	src/graphics/host_gpu/renderer/cache/bufferCache.h
M	src/graphics/host_gpu/renderer/cache/faultManager.cpp
A	src/graphics/host_gpu/renderer/cache/imageCacheGcPolicy.h
M	src/graphics/host_gpu/renderer/cache/samplerCache.cpp
M	src/graphics/host_gpu/renderer/cache/samplerCache.h
M	src/graphics/host_gpu/renderer/cache/streamBuffer.cpp
M	src/graphics/host_gpu/renderer/cache/streamBuffer.h
M	src/graphics/host_gpu/renderer/cache/textureCache.cpp
M	src/graphics/host_gpu/renderer/cache/textureCache.h
A	src/graphics/host_gpu/renderer/cache/uploadDma.cpp
A	src/graphics/host_gpu/renderer/cache/uploadDma.h
M	src/graphics/host_gpu/renderer/colorRenderTarget.cpp
M	src/graphics/host_gpu/renderer/colorRenderTarget.h
A	src/graphics/host_gpu/renderer/commandRecorder.cpp
A	src/graphics/host_gpu/renderer/commandRecorder.h
M	src/graphics/host_gpu/renderer/commandScheduler.cpp
M	src/graphics/host_gpu/renderer/commandScheduler.h
A	src/graphics/host_gpu/renderer/commandStream.cpp
A	src/graphics/host_gpu/renderer/commandStream.h
A	src/graphics/host_gpu/renderer/commandStreamReplay.inl
M	src/graphics/host_gpu/renderer/context.cpp
M	src/graphics/host_gpu/renderer/debug.cpp
M	src/graphics/host_gpu/renderer/debug.h
M	src/graphics/host_gpu/renderer/depthRenderTarget.cpp
M	src/graphics/host_gpu/renderer/depthRenderTarget.h
A	src/graphics/host_gpu/renderer/drawPrep/bindingPlan.cpp
A	src/graphics/host_gpu/renderer/drawPrep/bindingPlan.h
A	src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp
A	src/graphics/host_gpu/renderer/drawPrep/drawPrep.h
A	src/graphics/host_gpu/renderer/drawPrep/packetClass.h
A	src/graphics/host_gpu/renderer/drawPrep/readSet.h
A	src/graphics/host_gpu/renderer/drawPrep/repeatTrace.cpp
A	src/graphics/host_gpu/renderer/drawPrep/repeatTrace.h
A	src/graphics/host_gpu/renderer/drawPrep/window.h
A	src/graphics/host_gpu/renderer/drawPrep/workerGate.h
A	src/graphics/host_gpu/renderer/eopTimestampClock.h
A	src/graphics/host_gpu/renderer/eopTimestamps.cpp
A	src/graphics/host_gpu/renderer/eopTimestamps.h
A	src/graphics/host_gpu/renderer/gpuOpProfiler.cpp
A	src/graphics/host_gpu/renderer/gpuOpProfiler.h
A	src/graphics/host_gpu/renderer/gpuTiming.cpp
A	src/graphics/host_gpu/renderer/gpuTiming.h
M	src/graphics/host_gpu/renderer/image/blitHelper.cpp
M	src/graphics/host_gpu/renderer/image/blitHelper.h
A	src/graphics/host_gpu/renderer/image/dccClear.cpp
A	src/graphics/host_gpu/renderer/image/dccClear.h
M	src/graphics/host_gpu/renderer/image/image.cpp
M	src/graphics/host_gpu/renderer/image/image.h
M	src/graphics/host_gpu/renderer/image/imageInfo.h
M	src/graphics/host_gpu/renderer/image/imageView.cpp
A	src/graphics/host_gpu/renderer/image/stagingCopier.cpp
A	src/graphics/host_gpu/renderer/image/stagingCopier.h
M	src/graphics/host_gpu/renderer/image/textureCommon.cpp
M	src/graphics/host_gpu/renderer/image/textureCommon.h
M	src/graphics/host_gpu/renderer/image/tiler.cpp
M	src/graphics/host_gpu/renderer/image/tiler.h
A	src/graphics/host_gpu/renderer/lodStats.cpp
A	src/graphics/host_gpu/renderer/lodStats.h
A	src/graphics/host_gpu/renderer/lodStatsReport.h
M	src/graphics/host_gpu/renderer/masterSemaphore.cpp
M	src/graphics/host_gpu/renderer/masterSemaphore.h
A	src/graphics/host_gpu/renderer/meshIndirect.cpp
A	src/graphics/host_gpu/renderer/meshIndirect.h
A	src/graphics/host_gpu/renderer/occlusion.cpp
A	src/graphics/host_gpu/renderer/occlusion.h
A	src/graphics/host_gpu/renderer/pipeline/descriptorSetReuse.cpp
A	src/graphics/host_gpu/renderer/pipeline/descriptorSetReuse.h
M	src/graphics/host_gpu/renderer/pipeline/descriptors.cpp
M	src/graphics/host_gpu/renderer/pipeline/descriptors.h
M	src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp
M	src/graphics/host_gpu/renderer/pipeline/pipelineCache.h
A	src/graphics/host_gpu/renderer/pipeline/pipelineLayoutCache.cpp
A	src/graphics/host_gpu/renderer/pipeline/pipelineLayoutCache.h
A	src/graphics/host_gpu/renderer/pipeline/pipelineLibrary.cpp
A	src/graphics/host_gpu/renderer/pipeline/pipelineLibrary.h
A	src/graphics/host_gpu/renderer/pipeline/programDiskCache.cpp
A	src/graphics/host_gpu/renderer/pipeline/programDiskCache.h
M	src/graphics/host_gpu/renderer/pipeline/shaderResourceBarrier.cpp
M	src/graphics/host_gpu/renderer/pipeline/shaderResourceBarrier.h
M	src/graphics/host_gpu/renderer/pipeline/shaders.cpp
A	src/graphics/host_gpu/renderer/pipeline/stagePrepWorker.cpp
A	src/graphics/host_gpu/renderer/pipeline/stagePrepWorker.h
A	src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.cpp
A	src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.h
A	src/graphics/host_gpu/renderer/referenceClock.h
M	src/graphics/host_gpu/renderer/render.h
M	src/graphics/host_gpu/renderer/renderCompute.cpp
M	src/graphics/host_gpu/renderer/renderContext.cpp
M	src/graphics/host_gpu/renderer/renderContext.h
M	src/graphics/host_gpu/renderer/renderDraw.cpp
M	src/graphics/host_gpu/renderer/renderDraw.h
M	src/graphics/host_gpu/renderer/sync.cpp
M	src/graphics/host_gpu/renderer/sync.h
A	src/graphics/host_gpu/shaders/gpu_blit_color32_to_depth.frag
A	src/graphics/host_gpu/shaders/gpu_blit_depth_to_color32.comp
A	src/graphics/host_gpu/shaders/gpu_dcc_clear.comp
A	src/graphics/host_gpu/shaders/gpu_dcc_occlusion.comp
A	src/graphics/host_gpu/shaders/gpu_dcc_validate.comp
A	src/graphics/host_gpu/shaders/gpu_mesh_indirect.comp
M	src/graphics/host_gpu/shaders/gpu_tiler_common.inc
A	src/graphics/host_gpu/syncEpoch.h
M	src/graphics/host_gpu/vma.cpp
A	src/graphics/host_gpu/writeTickMap.h
M	src/graphics/presentation/renderDoc.cpp
M	src/graphics/presentation/systemOverlay.cpp
M	src/graphics/presentation/videoOut.cpp
M	src/graphics/presentation/videoOut.h
M	src/graphics/presentation/window/hostInput.cpp
A	src/graphics/presentation/window/hostInputPulse.h
M	src/graphics/presentation/window/swapchain.cpp
M	src/graphics/presentation/window/vulkanWindow.cpp
M	src/graphics/presentation/window/window.cpp
A	src/graphics/shader/recompiler/CodegenFingerprint.cpp
A	src/graphics/shader/recompiler/CodegenFingerprint.h
A	src/graphics/shader/recompiler/CodegenOptions.cpp
A	src/graphics/shader/recompiler/CodegenOptions.h
M	src/graphics/shader/recompiler/ShaderRecompiler.cpp
M	src/graphics/shader/recompiler/ShaderRecompiler.h
M	src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp
M	src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.h
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterAlu.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterAluHelpers.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterFlow.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterHelpers.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterImage.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterInstructions.h
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterModule.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterProgram.cpp
M	src/graphics/shader/recompiler/frontend/cfg/ShaderCFG.cpp
M	src/graphics/shader/recompiler/frontend/decode/ImageOps.cpp
M	src/graphics/shader/recompiler/frontend/decode/MemoryOps.cpp
M	src/graphics/shader/recompiler/frontend/decode/ScalarAluOps.cpp
M	src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.cpp
M	src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.h
M	src/graphics/shader/recompiler/frontend/decode/VectorAluOps.cpp
M	src/graphics/shader/recompiler/frontend/translate/Attribute.cpp
M	src/graphics/shader/recompiler/frontend/translate/Control.cpp
M	src/graphics/shader/recompiler/frontend/translate/Integer.cpp
M	src/graphics/shader/recompiler/frontend/translate/Memory.cpp
M	src/graphics/shader/recompiler/frontend/translate/Scalar.cpp
M	src/graphics/shader/recompiler/frontend/translate/Translate.cpp
M	src/graphics/shader/recompiler/frontend/translate/Translator.h
M	src/graphics/shader/recompiler/frontend/translate/Vector.cpp
M	src/graphics/shader/recompiler/ir/Block.h
M	src/graphics/shader/recompiler/ir/Program.cpp
A	src/graphics/shader/recompiler/ir/ProgramClone.cpp
A	src/graphics/shader/recompiler/ir/ProgramCodec.cpp
A	src/graphics/shader/recompiler/ir/ProgramCodec.h
M	src/graphics/shader/recompiler/ir/ShaderIR.h
M	src/graphics/shader/recompiler/ir/Value.h
M	src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.cpp
M	src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.inc
M	src/graphics/shader/recompiler/ir/passes/BindingLayout.cpp
M	src/graphics/shader/recompiler/ir/passes/BindingLayout.h
M	src/graphics/shader/recompiler/ir/passes/ConstantPropagation.cpp
A	src/graphics/shader/recompiler/ir/passes/ExecSelectElimination.cpp
A	src/graphics/shader/recompiler/ir/passes/ExecSelectElimination.h
M	src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.cpp
M	src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.h
M	src/graphics/shader/recompiler/ir/passes/ResourceTracking.cpp
M	src/graphics/shader/recompiler/ir/passes/ResourceTracking.h
M	src/graphics/shader/recompiler/ir/passes/ShaderInfoCollection.cpp
M	src/graphics/shader/recompiler/ir/passes/SrtWalker.cpp
M	src/graphics/shader/recompiler/ir/passes/SrtWalker.h
A	src/graphics/shader/recompiler/ir/passes/WriteRangeAnalysis.cpp
A	src/graphics/shader/recompiler/ir/passes/WriteRangeAnalysis.h
M	src/graphics/shader/shader.cpp
M	src/graphics/shader/shader.h
A	src/graphics/shader/shaderStaticKey.cpp
M	src/kernel/eventQueue.cpp
M	src/kernel/eventQueue.h
A	src/kernel/eventQueueFilters.h
M	src/kernel/fileSystem.cpp
M	src/kernel/memory.cpp
M	src/kernel/memory.h
M	src/kernel/memoryAddressSpace.inc
A	src/kernel/pendingSignals.h
M	src/kernel/pthread.cpp
M	src/kernel/pthread.h
M	src/launcher/src/mainDialog.cpp
M	src/libs/agc.cpp
A	src/libs/amprCounterBank.h
M	src/libs/controller.cpp
M	src/libs/libAmpr.cpp
M	src/libs/libKernel.cpp
M	src/loader/gamePatch.cpp
A	src/loader/gamePatchFilter.h
M	src/loader/runtimeLinker.cpp
A	tests/AmprCounterBankTests.cpp
A	tests/BindingPathTests.cpp
M	tests/BitArrayTests.cpp
A	tests/CpRecorderTests.cpp
A	tests/CpSequencerTests.cpp
A	tests/CpuPlacementTests.cpp
A	tests/DrawPrepTests.cpp
A	tests/EopTimestampTests.cpp
M	tests/EventQueueLifetimeTests.cpp
A	tests/EventQueueSemanticsTests.cpp
A	tests/GamePatchFilterTests.cpp
A	tests/HostInputPulseTests.cpp
A	tests/ImageCacheGcPolicyTests.cpp
M	tests/ImeDialogTests.cpp
A	tests/LodStatsReportTests.cpp
M	tests/MemoryTrackerTests.cpp
M	tests/PageManagerTests.cpp
A	tests/ProfilerCounterTests.cpp
A	tests/RepeatTraceTests.cpp
M	tests/ResourceMaterializationTests.cpp
M	tests/ResourceTrackingTests.cpp
A	tests/ShaderCodegenTests.inc
A	tests/ShaderGiProbeTests.inc
A	tests/ShaderProgramCacheTests.inc
M	tests/ShaderRecompilerComputeTests.cpp
A	tests/ShaderSrtVariantTests.inc
A	tests/ThreadServiceTests.cpp
M	tests/VirtualMemoryAllocationTests.cpp
M	tests/shaderCfgTests.cpp
A	tools/hangtrace/analyze_gpuops.py
```

## Upstream-only changes at the comparison revision

These 76 commits are absent from U59. Their presence matters when comparing against
current upstream: similar feature names do not establish equivalent implementations.

- [`93185953`](https://github.com/KytyPS5/KytyPS5/commit/93185953f113026c9bd98bf2a34861ce9e0f8f25) shader: fuse image specialization setup with materialization
- [`6bb68a3f`](https://github.com/KytyPS5/KytyPS5/commit/6bb68a3fd97c0ac913d9c22d6c1279549597dd56) shader: stop repeated CFG merge gateway splitting
- [`ff546085`](https://github.com/KytyPS5/KytyPS5/commit/ff54608569d1bcfb6974b7a0e84e22f25d7c4d9a) shader: add V_CMPX_O_F32 opcode
- [`3585cd8f`](https://github.com/KytyPS5/KytyPS5/commit/3585cd8f037f3122906ad0681e9d74d6f34fcdbe) shader: simplify translator dispatch to void handlers
- [`f7ae6e0a`](https://github.com/KytyPS5/KytyPS5/commit/f7ae6e0a83a9b2abdba67882ba791d585bf0cadf) audio: support NGS2 PCM16 samplers and rear low-pass filters (#825)
- [`bfdbba3a`](https://github.com/KytyPS5/KytyPS5/commit/bfdbba3aa43f9851bb19fab0eee775672b086fa7) graphics: share EOP clock and immediate write handling
- [`0ec43a8d`](https://github.com/KytyPS5/KytyPS5/commit/0ec43a8d3dfc523f66ebcf433c4e402eccf7bf3e) graphics: name Vulkan images and views only on creation
- [`7e1c2d15`](https://github.com/KytyPS5/KytyPS5/commit/7e1c2d1532cd0421ccd7b9fdc61bd2eb7b535d46) avplayer: auto-start playback without an event callback
- [`5ce4f083`](https://github.com/KytyPS5/KytyPS5/commit/5ce4f08364f8875f6554dcbfffd8619c7afc6433) audio: improve NGS2 stream and ATRAC9 playback
- [`cf423df6`](https://github.com/KytyPS5/KytyPS5/commit/cf423df62435c733211a235d6280f83399f1175f) shader: remove per-draw vertex diagnostic atomics
- [`1371219f`](https://github.com/KytyPS5/KytyPS5/commit/1371219f5cd49eecf4440528d7db5593d1aa9120) loader: preserve resolved relocations across dynamic loads
- [`e056d55d`](https://github.com/KytyPS5/KytyPS5/commit/e056d55dd5b633000cf234e1fb8c744f0e710a5e) loader: consolidate eager symbol resolution and remove dead PLT machinery
- [`0fb3ac56`](https://github.com/KytyPS5/KytyPS5/commit/0fb3ac5605cfe675ed8bbefad7fb3033c64dc7ae) kernel: retain modules until their final explicit unload
- [`49716600`](https://github.com/KytyPS5/KytyPS5/commit/497166009c8c0dd663fa1ffce3805a31bdc40d0e) loader: unlink exact module imports before releasing their mapping
- [`1216fa36`](https://github.com/KytyPS5/KytyPS5/commit/1216fa36cf1f61fd49fe984e721e8d9d2e98c2e7) docs: fix stale README notes, document building without Qt (#812)
- [`cda236db`](https://github.com/KytyPS5/KytyPS5/commit/cda236dbb34bd37b473c60d700884d75bd100471) Shader recompiler: accept V_NOT_B32 SDWA partial dst with any source; allow 64 buffers
- [`313b60e9`](https://github.com/KytyPS5/KytyPS5/commit/313b60e922ff53f901a78de19979a60366cdfb83) Capture PCM at set time when multiple AudioOut2 ports share a buffer (#826)
- [`1e438136`](https://github.com/KytyPS5/KytyPS5/commit/1e438136faa37b120d45fd398eef78b906ca7ca2) Build save_data_memory_tests from the kyty_tests aggregate (#798)
- [`4e8d8c84`](https://github.com/KytyPS5/KytyPS5/commit/4e8d8c849dd45023bc5c1f8ffae324efe15cfaca) tests: add LeastRecentlyUsedCache regression tests (#671)
- [`fd2e15ee`](https://github.com/KytyPS5/KytyPS5/commit/fd2e15ee737ea9a671619246129bc2aae7e0bfda) ci: run the recompiler unit tests (#447)
- [`cf268af8`](https://github.com/KytyPS5/KytyPS5/commit/cf268af83922d28568c53ef960fe3f3558117048) Shader: implement RDNA2 image atomic float maximum
- [`16b83a03`](https://github.com/KytyPS5/KytyPS5/commit/16b83a034cdc7abed2482605c77921b10056d83c) Shader: execute RDNA2 FP64 shadow arithmetic and log denorm limits
- [`e0b682e4`](https://github.com/KytyPS5/KytyPS5/commit/e0b682e4b2cc08b127765333123aeeaa3d092fdb) AvPlayer: fix Crash Bandicoot 4 intro playback and green video padding (#848)
- [`f1f930e0`](https://github.com/KytyPS5/KytyPS5/commit/f1f930e06c35229e7a3a4cec2287c99c19d61815) fix(launcher): use native macOS game folder picker
- [`81f31263`](https://github.com/KytyPS5/KytyPS5/commit/81f312638fc4534b6046df066bb36460f56ead0a) graphics: encode packed UNorm buffer stores before writing skinning normals
- [`a22a9d93`](https://github.com/KytyPS5/KytyPS5/commit/a22a9d9348bb079c6b97c7f14ffb47d9b754fa44) audio: play DualSense haptics from vibration audio ports (#843)
- [`bec67b78`](https://github.com/KytyPS5/KytyPS5/commit/bec67b78639dfaf212141479d0028921845de3c8) Fix HITMAN 3 boot, layered presentation, and socket imports (#851)
- [`496432bb`](https://github.com/KytyPS5/KytyPS5/commit/496432bbb42acaf3f4b4c638536456496674d1b2) Shader: skip identity image remapping on materialization hits
- [`041874f6`](https://github.com/KytyPS5/KytyPS5/commit/041874f663cb409fe9fe7ed15fc0ab4c897a265f) Graphics: resolve equivalent terminal mip views without expanding backing
- [`a88181b8`](https://github.com/KytyPS5/KytyPS5/commit/a88181b89751d42ceccb9a58011df75242c4a788) Graphics: share texture layout for linear render-target mip chains
- [`7b6bfdec`](https://github.com/KytyPS5/KytyPS5/commit/7b6bfdec643dcf8ce71f1fac18243541452a1f02) Graphics: share canonical padded layouts for single-sample color targets
- [`f53f0ca4`](https://github.com/KytyPS5/KytyPS5/commit/f53f0ca4833b5e99ffe53973e1cc21a34346d98f) Graphics: preserve rendered contents across 1D and 2D texture aliases
- [`da33a1c4`](https://github.com/KytyPS5/KytyPS5/commit/da33a1c4a371102c116ec81e35d971bf9bc2a787) Graphics: respect physical tile layout when resolving depth aliases
- [`0f1ff37f`](https://github.com/KytyPS5/KytyPS5/commit/0f1ff37f2328b1ea7c8268157fcf7be62fdb2276) Graphics: materialize CMASK clears before publishing expanded metadata
- [`e51f252b`](https://github.com/KytyPS5/KytyPS5/commit/e51f252b75a0dfb1de9fa1015e1e0c090600fbc4) AvPlayer: preserve clip timestamps across playback loops
- [`1b6af301`](https://github.com/KytyPS5/KytyPS5/commit/1b6af301b5d7d0ee79e1e13147d0f6a276dcfdef) tests: wire dormant VOP3 lane-read check and guard new dormant tests (#840)
- [`9229c04e`](https://github.com/KytyPS5/KytyPS5/commit/9229c04ebf344f59246cc6f599dde5460a69a51a) samplerCache: handle reserved sampler anisotropy ratios
- [`7c8583d2`](https://github.com/KytyPS5/KytyPS5/commit/7c8583d204d96e056359182735e07500e3a0e4e6) audio: implement the AJM Opus decoder (#858)
- [`025b39c0`](https://github.com/KytyPS5/KytyPS5/commit/025b39c0184581bc45ad3bc0bc67d4addbb095f4) Fix empty query handling in HttpUriParse (#764)
- [`421684e7`](https://github.com/KytyPS5/KytyPS5/commit/421684e75f75e6d1a2c4b0817ca573a2e800f748) docs: add weekly updates and Discord links to README (#866)
- [`0791f921`](https://github.com/KytyPS5/KytyPS5/commit/0791f921f6cd82456da988107d8d19e8922526d2) UFC 5 in-game (#883)
- [`0a5774be`](https://github.com/KytyPS5/KytyPS5/commit/0a5774be7b725759576b90fdb4a5075214fd3100) shader: support FP64 conversions used by FighterZ (#884)
- [`ab2c1c4c`](https://github.com/KytyPS5/KytyPS5/commit/ab2c1c4c03590b8eace83bafd5b9f1010d36afdb) Update README screenshot to UFC 5
- [`c91fe80d`](https://github.com/KytyPS5/KytyPS5/commit/c91fe80d6416c171224361d14d380bc9d63b1e2c) memory: separate extended allocations from ordinary guest mappings
- [`2016f8e2`](https://github.com/KytyPS5/KytyPS5/commit/2016f8e2fe38789d9ae6117f3bd8379a9392bf6b) gpu: address extended guest memory without widening dense cache tables
- [`a20b64d3`](https://github.com/KytyPS5/KytyPS5/commit/a20b64d3e5dd6cbdb07c7895219152ddf8959ba6) audio: rewind compressed sampler blocks at loop boundaries
- [`34a3eb6b`](https://github.com/KytyPS5/KytyPS5/commit/34a3eb6b58a60a13a56ca44f4a794d9593a17d68) memory: recycle automatic backing through physical ownership
- [`c8baa7b9`](https://github.com/KytyPS5/KytyPS5/commit/c8baa7b9090d552d78ddb5ed587c91dbf8b28919) shader: derive immutable resource policy once per plan
- [`a892097f`](https://github.com/KytyPS5/KytyPS5/commit/a892097ff0ed56d5c79c2c2a48cb18533a38c32a) tests: fix macOS extended-memory alias regression check (#889)
- [`05f92b64`](https://github.com/KytyPS5/KytyPS5/commit/05f92b64229f933b5ed9af9bd3b7800e64883843) libCes CP932 fix for Windows/macOS, fix SaveDataDialog lockups (#878)
- [`a107d1e3`](https://github.com/KytyPS5/KytyPS5/commit/a107d1e3330d62912438e580dd2c42ab55c8f89e) filesystem: keep unmapped guest paths out of the host namespace
- [`22ff4693`](https://github.com/KytyPS5/KytyPS5/commit/22ff4693c568a772164e3e2a232f14b87cfc4bbd) audio: play pad speaker ports on the DualSense speaker (#868)
- [`4cf456fb`](https://github.com/KytyPS5/KytyPS5/commit/4cf456fb7715ca0e95448bdf0d9968628251cd6c) graphics: retain buffer names across draw bindings
- [`b10bd53d`](https://github.com/KytyPS5/KytyPS5/commit/b10bd53dd3c225131405407e60d4313eba532dc3) audio: prevent ATRAC9 vibration configuration heap corruption
- [`0471c999`](https://github.com/KytyPS5/KytyPS5/commit/0471c9991cdf7045f9d855ba55b2788cd8a40855) audio: implement AudioOut2 user capability queries
- [`6ee1bf9d`](https://github.com/KytyPS5/KytyPS5/commit/6ee1bf9db0812d08876d12ed5361c468e27e1225) graphics: trust image-local readback validity
- [`8e61798b`](https://github.com/KytyPS5/KytyPS5/commit/8e61798bfd3b22ef0896f522e5f284ab9971158b) graphics: honor CPU ownership in image dirty-region queries
- [`4b0de1b6`](https://github.com/KytyPS5/KytyPS5/commit/4b0de1b67265069110492e5e1005d5c7635043a9) Zarchive Loading (#724)
- [`49db3ea5`](https://github.com/KytyPS5/KytyPS5/commit/49db3ea54c27d80d67b99bd472f79b9937bf9b76) tests: skip rasterization cases instead of aborting the suite on limited GPUs (#875)
- [`21d7e8e1`](https://github.com/KytyPS5/KytyPS5/commit/21d7e8e1f0b546ca5424a1fffc5466802c2a78aa) tests: run the saveexec mask cases in a populated subgroup (#876)
- [`6f241bef`](https://github.com/KytyPS5/KytyPS5/commit/6f241bef4983e67338d533a552668395a59f9120) Fix macOS aggregate test build (#536)
- [`62a0222e`](https://github.com/KytyPS5/KytyPS5/commit/62a0222e24e2cc727c66742b1dbaaa06188510df) tests: cover 1D-array color render targets (#377)
- [`539c0f7b`](https://github.com/KytyPS5/KytyPS5/commit/539c0f7bf82704c5d3e52026d3f8c516631a093c) graphics: restore block image storage usage where the device supports it (#894)
- [`8e42e997`](https://github.com/KytyPS5/KytyPS5/commit/8e42e99788faf3b8443e479b3ab7e5f6201358d6) graphics: batch release labels without redundant submissions
- [`b43bf9f7`](https://github.com/KytyPS5/KytyPS5/commit/b43bf9f7fb93ac3c49fa908432fdcbf08e048747) graphics: store vertex attribute bindings without duplicated arrays
- [`37501b5a`](https://github.com/KytyPS5/KytyPS5/commit/37501b5a787e64d48312cd864a2aa6f98529d832) graphics: stream clean guest buffers without backing-store lookups
- [`5e4f6afc`](https://github.com/KytyPS5/KytyPS5/commit/5e4f6afc3000c327720ed56ac270694d8aa3f668) graphics: build color target descriptors in place
- [`0fbeac6d`](https://github.com/KytyPS5/KytyPS5/commit/0fbeac6d3f4ea05aa84785ca09b083aa81e203a6) graphics: compact shader keys and hash packed pipeline state
- [`6d666681`](https://github.com/KytyPS5/KytyPS5/commit/6d6666819a449ffe4bcbecebcdab0b70e635ed37) graphics: serialize pipeline cache through its renderer owner
- [`e57c37fa`](https://github.com/KytyPS5/KytyPS5/commit/e57c37fa9ce69617b446e80c719608897be53d59) graphics: consume vertex semantics directly from shader metadata
- [`0c844e4b`](https://github.com/KytyPS5/KytyPS5/commit/0c844e4bc2c47ae781a6c490972e86b44ca496f9) profiler: use native Tracy scopes and remove unused manual lifetime tracking
- [`6f24b031`](https://github.com/KytyPS5/KytyPS5/commit/6f24b031f563a9807916cc850f287ffd5dcc27ca) kernel: block short sleep requests instead of busy waiting
- [`35c4dca2`](https://github.com/KytyPS5/KytyPS5/commit/35c4dca2ce7a90ec1a82f4e3f4d2a6070ac8f2f4) shader: add V_CMPX_NLE_F16 opcode
- [`6799ecbc`](https://github.com/KytyPS5/KytyPS5/commit/6799ecbc5be06680444c0fb34f305c73f332e55d) shader: add native 64-bit LDS OR with shared typed storage views
- [`73615c31`](https://github.com/KytyPS5/KytyPS5/commit/73615c31ff36aec9edb8fece167f8c2a0a6ec294) graphics: preserve logical alpha when blending reordered targets (PPSA02721)
- [`59a17604`](https://github.com/KytyPS5/KytyPS5/commit/59a17604274e55c0380bee823c61d3fd0a4717ff) sampler: clamp mipLodBias to not be greater than maxSamplerLodBias

## Complete two-tip file comparison

Direction: checked current upstream → development U59. Deletions can represent
newer upstream additions missing from the older U59 branch, not intentional
removal for performance. Local private handoff/editor files are omitted.

```text
M	.github/workflows/build.yml
M	3rdparty/CMakeLists.txt
D	3rdparty/patches/zarchive-reader.patch
M	CMakeLists.txt
M	README.md
A	docs/host-input-testing.md
A	docs/metadata-clear-eliminate.md
M	docs/screenshots/ps5-06.png
A	src/codegen_version.cmake
M	src/common/CMakeLists.txt
D	src/common/archive.cpp
D	src/common/archive.h
M	src/common/bitArray.h
A	src/common/cpuPlacement.cpp
A	src/common/cpuPlacement.h
M	src/common/file.cpp
M	src/common/file.h
A	src/common/hangTrace.cpp
A	src/common/hangTrace.h
M	src/common/hostException.cpp
M	src/common/hostException.h
A	src/common/hostInputTrace.h
M	src/common/lruCache.h
M	src/common/platform/sysFileIO.h
M	src/common/platform/sysLinuxFileIO.cpp
M	src/common/platform/sysWindowsFileIO.cpp
M	src/common/profiler.cpp
M	src/common/profiler.h
A	src/common/rendererBatch.h
M	src/common/threads.cpp
M	src/common/threads.h
M	src/graphics/guest_gpu/command_processor/commandProcessor.h
A	src/graphics/guest_gpu/command_processor/cpOps.cpp
A	src/graphics/guest_gpu/command_processor/cpOps.h
A	src/graphics/guest_gpu/command_processor/cpSequencer.cpp
A	src/graphics/guest_gpu/command_processor/cpSequencer.h
A	src/graphics/guest_gpu/command_processor/cpVerify.cpp
A	src/graphics/guest_gpu/command_processor/cpVerify.h
M	src/graphics/guest_gpu/command_processor/pm4Handlers.cpp
M	src/graphics/guest_gpu/graphicsRun.cpp
M	src/graphics/guest_gpu/graphicsRun.h
M	src/graphics/guest_gpu/hardwareContext.h
M	src/graphics/guest_gpu/tile.cpp
M	src/graphics/guest_gpu/tile.h
A	src/graphics/host_gpu/cleanVerdictCache.h
A	src/graphics/host_gpu/coherenceLog.h
A	src/graphics/host_gpu/eagerReadbackPages.h
A	src/graphics/host_gpu/gpuReadDelegate.h
A	src/graphics/host_gpu/gpuTouchedPages.h
M	src/graphics/host_gpu/graphicContext.h
A	src/graphics/host_gpu/memoryStats.h
M	src/graphics/host_gpu/memoryTracker.cpp
M	src/graphics/host_gpu/memoryTracker.h
M	src/graphics/host_gpu/pageManager.cpp
M	src/graphics/host_gpu/pageManager.h
A	src/graphics/host_gpu/parkingLock.h
A	src/graphics/host_gpu/queueSubmission.cpp
A	src/graphics/host_gpu/queueSubmission.h
M	src/graphics/host_gpu/rangeSet.h
M	src/graphics/host_gpu/regionDefinitions.h
M	src/graphics/host_gpu/regionManager.h
M	src/graphics/host_gpu/renderer/cache/bufferCache.cpp
M	src/graphics/host_gpu/renderer/cache/bufferCache.h
M	src/graphics/host_gpu/renderer/cache/faultManager.cpp
A	src/graphics/host_gpu/renderer/cache/imageCacheGcPolicy.h
M	src/graphics/host_gpu/renderer/cache/samplerCache.cpp
M	src/graphics/host_gpu/renderer/cache/samplerCache.h
M	src/graphics/host_gpu/renderer/cache/streamBuffer.cpp
M	src/graphics/host_gpu/renderer/cache/streamBuffer.h
M	src/graphics/host_gpu/renderer/cache/textureCache.cpp
M	src/graphics/host_gpu/renderer/cache/textureCache.h
A	src/graphics/host_gpu/renderer/cache/uploadDma.cpp
A	src/graphics/host_gpu/renderer/cache/uploadDma.h
M	src/graphics/host_gpu/renderer/colorRenderTarget.cpp
M	src/graphics/host_gpu/renderer/colorRenderTarget.h
A	src/graphics/host_gpu/renderer/commandRecorder.cpp
A	src/graphics/host_gpu/renderer/commandRecorder.h
M	src/graphics/host_gpu/renderer/commandScheduler.cpp
M	src/graphics/host_gpu/renderer/commandScheduler.h
A	src/graphics/host_gpu/renderer/commandStream.cpp
A	src/graphics/host_gpu/renderer/commandStream.h
A	src/graphics/host_gpu/renderer/commandStreamReplay.inl
M	src/graphics/host_gpu/renderer/context.cpp
M	src/graphics/host_gpu/renderer/debug.cpp
M	src/graphics/host_gpu/renderer/debug.h
M	src/graphics/host_gpu/renderer/depthRenderTarget.cpp
M	src/graphics/host_gpu/renderer/depthRenderTarget.h
A	src/graphics/host_gpu/renderer/drawPrep/bindingPlan.cpp
A	src/graphics/host_gpu/renderer/drawPrep/bindingPlan.h
A	src/graphics/host_gpu/renderer/drawPrep/drawPrep.cpp
A	src/graphics/host_gpu/renderer/drawPrep/drawPrep.h
A	src/graphics/host_gpu/renderer/drawPrep/packetClass.h
A	src/graphics/host_gpu/renderer/drawPrep/readSet.h
A	src/graphics/host_gpu/renderer/drawPrep/repeatTrace.cpp
A	src/graphics/host_gpu/renderer/drawPrep/repeatTrace.h
A	src/graphics/host_gpu/renderer/drawPrep/window.h
A	src/graphics/host_gpu/renderer/drawPrep/workerGate.h
A	src/graphics/host_gpu/renderer/eopTimestampClock.h
A	src/graphics/host_gpu/renderer/eopTimestamps.cpp
A	src/graphics/host_gpu/renderer/eopTimestamps.h
A	src/graphics/host_gpu/renderer/gpuOpProfiler.cpp
A	src/graphics/host_gpu/renderer/gpuOpProfiler.h
A	src/graphics/host_gpu/renderer/gpuTiming.cpp
A	src/graphics/host_gpu/renderer/gpuTiming.h
M	src/graphics/host_gpu/renderer/image/blitHelper.cpp
M	src/graphics/host_gpu/renderer/image/blitHelper.h
A	src/graphics/host_gpu/renderer/image/dccClear.cpp
A	src/graphics/host_gpu/renderer/image/dccClear.h
M	src/graphics/host_gpu/renderer/image/image.cpp
M	src/graphics/host_gpu/renderer/image/image.h
M	src/graphics/host_gpu/renderer/image/imageInfo.h
A	src/graphics/host_gpu/renderer/image/stagingCopier.cpp
A	src/graphics/host_gpu/renderer/image/stagingCopier.h
M	src/graphics/host_gpu/renderer/image/textureCommon.cpp
M	src/graphics/host_gpu/renderer/image/textureCommon.h
M	src/graphics/host_gpu/renderer/image/tiler.cpp
M	src/graphics/host_gpu/renderer/image/tiler.h
A	src/graphics/host_gpu/renderer/lodStats.cpp
A	src/graphics/host_gpu/renderer/lodStats.h
A	src/graphics/host_gpu/renderer/lodStatsReport.h
M	src/graphics/host_gpu/renderer/masterSemaphore.cpp
M	src/graphics/host_gpu/renderer/masterSemaphore.h
A	src/graphics/host_gpu/renderer/meshIndirect.cpp
A	src/graphics/host_gpu/renderer/meshIndirect.h
A	src/graphics/host_gpu/renderer/occlusion.cpp
A	src/graphics/host_gpu/renderer/occlusion.h
D	src/graphics/host_gpu/renderer/pipeline/blendMapping.cpp
D	src/graphics/host_gpu/renderer/pipeline/blendMapping.h
A	src/graphics/host_gpu/renderer/pipeline/descriptorSetReuse.cpp
A	src/graphics/host_gpu/renderer/pipeline/descriptorSetReuse.h
M	src/graphics/host_gpu/renderer/pipeline/descriptors.cpp
M	src/graphics/host_gpu/renderer/pipeline/descriptors.h
M	src/graphics/host_gpu/renderer/pipeline/pipelineCache.cpp
M	src/graphics/host_gpu/renderer/pipeline/pipelineCache.h
A	src/graphics/host_gpu/renderer/pipeline/pipelineLayoutCache.cpp
A	src/graphics/host_gpu/renderer/pipeline/pipelineLayoutCache.h
A	src/graphics/host_gpu/renderer/pipeline/pipelineLibrary.cpp
A	src/graphics/host_gpu/renderer/pipeline/pipelineLibrary.h
A	src/graphics/host_gpu/renderer/pipeline/programDiskCache.cpp
A	src/graphics/host_gpu/renderer/pipeline/programDiskCache.h
M	src/graphics/host_gpu/renderer/pipeline/shaderResourceBarrier.cpp
M	src/graphics/host_gpu/renderer/pipeline/shaderResourceBarrier.h
M	src/graphics/host_gpu/renderer/pipeline/shaders.cpp
A	src/graphics/host_gpu/renderer/pipeline/stagePrepWorker.cpp
A	src/graphics/host_gpu/renderer/pipeline/stagePrepWorker.h
A	src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.cpp
A	src/graphics/host_gpu/renderer/pipeline/textureBindingMemo.h
A	src/graphics/host_gpu/renderer/referenceClock.h
M	src/graphics/host_gpu/renderer/render.h
M	src/graphics/host_gpu/renderer/renderCompute.cpp
M	src/graphics/host_gpu/renderer/renderContext.cpp
M	src/graphics/host_gpu/renderer/renderContext.h
M	src/graphics/host_gpu/renderer/renderDraw.cpp
M	src/graphics/host_gpu/renderer/renderDraw.h
M	src/graphics/host_gpu/renderer/sync.cpp
M	src/graphics/host_gpu/renderer/sync.h
A	src/graphics/host_gpu/shaders/gpu_blit_color32_to_depth.frag
A	src/graphics/host_gpu/shaders/gpu_blit_depth_to_color32.comp
A	src/graphics/host_gpu/shaders/gpu_dcc_clear.comp
A	src/graphics/host_gpu/shaders/gpu_dcc_occlusion.comp
A	src/graphics/host_gpu/shaders/gpu_dcc_validate.comp
A	src/graphics/host_gpu/shaders/gpu_mesh_indirect.comp
M	src/graphics/host_gpu/shaders/gpu_tiler_common.inc
D	src/graphics/host_gpu/shaders/gpu_video_out_overlay.frag
A	src/graphics/host_gpu/syncEpoch.h
M	src/graphics/host_gpu/vma.cpp
A	src/graphics/host_gpu/writeTickMap.h
M	src/graphics/presentation/presenter.h
M	src/graphics/presentation/renderDoc.cpp
M	src/graphics/presentation/systemOverlay.cpp
M	src/graphics/presentation/videoOut.cpp
M	src/graphics/presentation/videoOut.h
M	src/graphics/presentation/window/hostInput.cpp
A	src/graphics/presentation/window/hostInputPulse.h
M	src/graphics/presentation/window/swapchain.cpp
M	src/graphics/presentation/window/vulkanWindow.cpp
M	src/graphics/presentation/window/window.cpp
A	src/graphics/shader/recompiler/CodegenFingerprint.cpp
A	src/graphics/shader/recompiler/CodegenFingerprint.h
A	src/graphics/shader/recompiler/CodegenOptions.cpp
A	src/graphics/shader/recompiler/CodegenOptions.h
M	src/graphics/shader/recompiler/ShaderRecompiler.cpp
M	src/graphics/shader/recompiler/ShaderRecompiler.h
M	src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.cpp
M	src/graphics/shader/recompiler/backend/spirv/SpirvEmitter.h
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterAlu.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterAluHelpers.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterFlow.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterHelpers.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterImage.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterInstructions.h
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterInternal.h
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemory.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterMemoryHelpers.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterModule.cpp
M	src/graphics/shader/recompiler/backend/spirv/spirvEmitterProgram.cpp
M	src/graphics/shader/recompiler/frontend/cfg/ShaderCFG.cpp
M	src/graphics/shader/recompiler/frontend/cfg/ShaderCFG.h
M	src/graphics/shader/recompiler/frontend/decode/ImageOps.cpp
M	src/graphics/shader/recompiler/frontend/decode/MemoryOps.cpp
M	src/graphics/shader/recompiler/frontend/decode/ScalarAluOps.cpp
M	src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.cpp
M	src/graphics/shader/recompiler/frontend/decode/ShaderDecoder.h
M	src/graphics/shader/recompiler/frontend/decode/VectorAluOps.cpp
M	src/graphics/shader/recompiler/frontend/translate/Attribute.cpp
M	src/graphics/shader/recompiler/frontend/translate/Compare.cpp
M	src/graphics/shader/recompiler/frontend/translate/Control.cpp
M	src/graphics/shader/recompiler/frontend/translate/Dispatch.cpp
M	src/graphics/shader/recompiler/frontend/translate/Float.cpp
M	src/graphics/shader/recompiler/frontend/translate/Integer.cpp
M	src/graphics/shader/recompiler/frontend/translate/Memory.cpp
M	src/graphics/shader/recompiler/frontend/translate/Scalar.cpp
M	src/graphics/shader/recompiler/frontend/translate/Translate.cpp
M	src/graphics/shader/recompiler/frontend/translate/Translator.h
M	src/graphics/shader/recompiler/frontend/translate/Vector.cpp
M	src/graphics/shader/recompiler/ir/Block.h
M	src/graphics/shader/recompiler/ir/Program.cpp
A	src/graphics/shader/recompiler/ir/ProgramClone.cpp
A	src/graphics/shader/recompiler/ir/ProgramCodec.cpp
A	src/graphics/shader/recompiler/ir/ProgramCodec.h
M	src/graphics/shader/recompiler/ir/ShaderIR.h
M	src/graphics/shader/recompiler/ir/Type.cpp
M	src/graphics/shader/recompiler/ir/Type.h
M	src/graphics/shader/recompiler/ir/Value.h
M	src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.cpp
M	src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.h
M	src/graphics/shader/recompiler/ir/opcodes/ValueOpcodes.inc
M	src/graphics/shader/recompiler/ir/passes/BindingLayout.cpp
M	src/graphics/shader/recompiler/ir/passes/BindingLayout.h
M	src/graphics/shader/recompiler/ir/passes/ConstantPropagation.cpp
A	src/graphics/shader/recompiler/ir/passes/ExecSelectElimination.cpp
A	src/graphics/shader/recompiler/ir/passes/ExecSelectElimination.h
M	src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.cpp
M	src/graphics/shader/recompiler/ir/passes/ResourceMaterialization.h
M	src/graphics/shader/recompiler/ir/passes/ResourceTracking.cpp
M	src/graphics/shader/recompiler/ir/passes/ResourceTracking.h
M	src/graphics/shader/recompiler/ir/passes/ShaderInfoCollection.cpp
M	src/graphics/shader/recompiler/ir/passes/SrtWalker.cpp
M	src/graphics/shader/recompiler/ir/passes/SrtWalker.h
A	src/graphics/shader/recompiler/ir/passes/WriteRangeAnalysis.cpp
A	src/graphics/shader/recompiler/ir/passes/WriteRangeAnalysis.h
M	src/graphics/shader/shader.cpp
M	src/graphics/shader/shader.h
M	src/graphics/shader/shaderBindings.h
A	src/graphics/shader/shaderStaticKey.cpp
M	src/graphics/shader/shaderVertexMetadata.cpp
M	src/graphics/shader/shaderVertexMetadata.h
M	src/kernel/eventQueue.cpp
M	src/kernel/eventQueue.h
A	src/kernel/eventQueueFilters.h
M	src/kernel/fileSystem.cpp
M	src/kernel/fileSystem.h
M	src/kernel/memory.cpp
M	src/kernel/memory.h
M	src/kernel/memoryAddressSpace.inc
A	src/kernel/pendingSignals.h
M	src/kernel/pthread.cpp
M	src/kernel/pthread.h
M	src/launcher/CMakeLists.txt
M	src/launcher/include/configurationItem.h
D	src/launcher/include/gameContent.h
M	src/launcher/include/gameListTreeWidget.h
M	src/launcher/include/updateChecker.h
M	src/launcher/src/configurationEditDialog.cpp
M	src/launcher/src/configurationItem.cpp
M	src/launcher/src/configurationListWidget.cpp
D	src/launcher/src/gameContent.cpp
M	src/launcher/src/mainDialog.cpp
M	src/launcher/src/trophyViewerDialog.cpp
M	src/launcher/src/updateChecker.cpp
M	src/libs/agc.cpp
M	src/libs/ajm.cpp
M	src/libs/ajm/atrac9_decoder.h
D	src/libs/ajm/opus_decoder.h
A	src/libs/amprCounterBank.h
M	src/libs/audio.cpp
M	src/libs/audio.h
M	src/libs/avPlayer.cpp
M	src/libs/controller.cpp
M	src/libs/controller.h
M	src/libs/dialog.cpp
D	src/libs/dualSenseHaptics.cpp
D	src/libs/dualSenseHaptics.h
M	src/libs/libAmpr.cpp
M	src/libs/libAudio.cpp
M	src/libs/libAudio2.cpp
M	src/libs/libCes.cpp
M	src/libs/libKernel.cpp
M	src/libs/libNet.cpp
M	src/libs/libVideoOut.cpp
M	src/libs/network.cpp
M	src/libs/ngs2.cpp
M	src/loader/gamePatch.cpp
A	src/loader/gamePatchFilter.h
M	src/loader/jit.h
M	src/loader/runtimeLinker.cpp
M	src/loader/runtimeLinker.h
M	src/loader/symbolDatabase.cpp
M	src/loader/symbolDatabase.h
M	src/main.cpp
A	tests/AmprCounterBankTests.cpp
D	tests/ArchiveFileTests.cpp
D	tests/ArchiveTestFixture.h
M	tests/AudioOut2PortTests.cpp
D	tests/AvPlayerFileTests.cpp
A	tests/BindingPathTests.cpp
M	tests/BitArrayTests.cpp
D	tests/CesTests.cpp
A	tests/CpRecorderTests.cpp
A	tests/CpSequencerTests.cpp
A	tests/CpuPlacementTests.cpp
A	tests/DrawPrepTests.cpp
A	tests/EopTimestampTests.cpp
M	tests/EventQueueLifetimeTests.cpp
A	tests/EventQueueSemanticsTests.cpp
A	tests/GamePatchFilterTests.cpp
A	tests/HostInputPulseTests.cpp
D	tests/HttpUriParseTests.cpp
A	tests/ImageCacheGcPolicyTests.cpp
M	tests/ImagePageTableTests.cpp
M	tests/ImeDialogTests.cpp
M	tests/KernelFileSystemTests.cpp
A	tests/LodStatsReportTests.cpp
D	tests/LruCacheTests.cpp
M	tests/MemoryTrackerTests.cpp
D	tests/Ngs2SamplerTests.cpp
D	tests/PadHapticsTests.cpp
M	tests/PageManagerTests.cpp
A	tests/ProfilerCounterTests.cpp
A	tests/RepeatTraceTests.cpp
M	tests/ResourceMaterializationTests.cpp
M	tests/ResourceTrackingTests.cpp
D	tests/SaveDataDialogTests.cpp
M	tests/ScalarProvenanceTests.cpp
A	tests/ShaderCodegenTests.inc
A	tests/ShaderGiProbeTests.inc
A	tests/ShaderProgramCacheTests.inc
M	tests/ShaderRecompilerComputeTests.cpp
A	tests/ShaderSrtVariantTests.inc
M	tests/ShaderVertexMetadataTests.cpp
A	tests/ThreadServiceTests.cpp
M	tests/VirtualMemoryAllocationTests.cpp
M	tests/shaderCfgTests.cpp
A	tools/hangtrace/analyze_gpuops.py
```
