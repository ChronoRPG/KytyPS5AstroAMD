# U59: changes, mechanisms and performance evidence

## What this comparison means

`main` contains the U59 renderer plus publication documentation and a portable
launch preset. The `u59` tag identifies the sanitized source checkpoint before
publication-only files. Later U60 renderer experiments are excluded.

The development U59 commit was `3220a90b`; privacy filtering changes its hash.
The shared development base was upstream `7855c980cb85cb916d47ad2e74ea14a311252e23`.
Upstream was also checked at
[`59a17604274e55c0380bee823c61d3fd0a4717ff`](https://github.com/KytyPS5/KytyPS5/commit/59a17604274e55c0380bee823c61d3fd0a4717ff).
U59 has 315 commits beyond that common base; the checked upstream has 76 commits
not in U59. This is a fork of an older baseline, not current upstream plus a patch.
Some upstream work addresses overlapping problems. The complete commit and file
inventories in [CHANGE-CATALOG.md](CHANGE-CATALOG.md) distinguish the two directions.

The catalog covers every fork commit through U59, including merges, tests,
diagnostics and the early A–S19 checkpoint. This guide explains the mechanisms
by subsystem.

### Reading performance claims

- **Measured:** a saved comparison observed a change under its stated conditions.
- **Expected:** the implementation removes work or overlap constraints, but its
  individual end-to-end contribution was not isolated.
- **Correctness:** fixes emulation; it can cost time or prevent misleadingly fast output.
- **Diagnostic/test:** helps investigate or validate changes; not an FPS optimization.

Most features were measured in bundles. Their individual savings cannot be added
together, and a cache hit does not prove a shorter frame. CPU work, GPU work and
wait intervals overlap. Skipping rendering, forcing ray misses or using stale
resources is not a valid performance improvement.

## 1. Submission and command processing

### Queued Vulkan submission and coalescing

The shared submission worker (`queueSubmission.*`) moves eligible driver calls
off the rendering thread. Ordered submission records retain completion gates,
callbacks, external semaphores and synchronous boundaries. Coalescing combines
compatible submissions rather than making one native submit structure per small
piece of work. This reduces driver entry and scheduling overhead and lets the
producer prepare later work while submission progresses.

**Measured, early desert comparisons:** Control/B2 pooled throughput was
29.601 → 33.249 Hz (+12.3%); a later B2/C3 comparison was 33.643 → 35.471 Hz
(+5.43%), with 16.96% fewer native submit structures. These used older
instrumentation and are not U59-versus-current-upstream results. Relevant controls:
`KYTY_SUBMISSION_MODE=queued`, `KYTY_SUBMISSION_COALESCE=1`.

### Recorder and sequencer: two different pipeline stages

`commandStream.*` and `commandRecorder.*` encode Vulkan emission into owned packets
for a recorder thread. The CP can prepare subsequent work without issuing every
Vulkan recording call itself. The ring has explicit ownership, ordering, idle-drain,
readback and completion rules. Indirect mesh conversion also records through the
same command sink. `KYTY_CP_RECORDER=1` enables it in the U59 preset.

`cpOps.*`, `cpSequencer.*` and `cpVerify.*` separately split PM4 parsing from execution.
The sequencer parses graphics queue 0 into an operation ring and publishes direct
draws to preparation workers. Unclean reads, lockstep operations, frame fences,
constant-engine handoffs and pending CP writes retain synchronization points.
`KYTY_CP_SEQ=1` enables this additional stage. Earlier command-byte reads resemble
prefetching but can expose lifetime mistakes, which is why reference-front
verification exists.

**Expected:** overlap parsing, resource preparation and driver recording. Historical
estimates of 1.5–2.5 ms/flip for sequencing were estimates, not an isolated measured
gain. The U58 bundle result below includes both architectures and other changes.

### Flushes, queue progress and frame pacing

- Batch end-of-pipe flush requests with a bounded delay instead of submitting for
  every interrupt. Submit early when the GPU is starved, and immediately submit
  the tick needed by a deferred visibility label.
- Wake suspended queues on actual progress, batch completion-runner wakeups, and
  use bounded spinning before parking. Opportunistic pending-operation pops avoid
  waiting on work that still has priority completion tasks.
- Suspend `WAIT_FLIP_DONE` queues instead of blocking the graphics processor thread.
  Slice long graphics runs so runnable asynchronous compute can make progress.
- Bound `sceAgcSuspendPoint` to two frames in flight rather than waiting for complete
  CP idleness. Preserve a per-frame submission fence so overlap does not reorder
  guest-visible frame dependencies.
- Service-thread priority, cheaper yield/sleep/signal polling, parking locks and
  optional CPU placement reduce wakeup overhead and contention. CPU-core reservation
  did not establish an end-to-end gain and is not forced by the portable preset.

These reduce starvation and avoid unnecessarily serial execution. Incorrectly
removing a wait would change guest behavior; accuracy fixes also restore EOP data
writes suppressed by interrupt selection and put GPU-owned `WRITE_DATA` destinations
on the GPU timeline. Record-time labels remain the tested U59 setting. Optional
GPU timestamps are a separate diagnostic/accuracy mode, not a promised faster mode.

## 2. Draw preparation and resource evaluation

### Resource lookup, SRT evaluation and reuse

The early checkpoint adds first-page image lookup before general overlap discovery,
an exact single-page dirty-image query, cheaper resource/binding preparation, clean
backing lookups, and batching of lookup/descriptor/upload bookkeeping. The dirty
query comparison observed about +4.1% in one desert session (two candidate captures,
no reverse control). Other lookup microbenchmarks do not isolate gameplay savings.

Shader resource table (SRT) read runs, compiled recipes and arithmetic tapes avoid
walking the same symbolic expression graph for each draw. Materialization shares
exact clean values, while dependency checks retain the memory ordering needed by
the guest. Clean-page verdicts and backing mappings are cached with invalidation.
Prepared-state caches and direct shader-variant reuse were also explored.

**Limitations:** large inter-draw caches had weak hit rates; one prepared-resource
capture had only 7.05% hits. Resource reuse and dependency-cache experiments remain
off in the preset. Root predecoding correlated with failed loading runs and remains
off (`KYTY_SRT_DECODED_ROOTS=0`). Shader metadata backing reads remain off after
the vertex-data investigation (`KYTY_SHADER_METADATA_BACKING=0`). This is not a
blanket statement that bypassing protected guest reads is safe.

### Parallel draw preparation and binding plans

The S1–S6 preparation series seals immutable shader plans, gives workers separate
scratch/output, and splits preparation from ordered commit. Workers resolve
programs/resources while the committing thread checks coherence certificates.
Coherence logs, read-set certificates, shader-code digests and value fallbacks reject
stale speculative work. Clean indirect register loads need not close the window.

P4b adds binding plans, hardware-check verdicts, dynamic viewport work, texture hints
and grouped texture lookups on workers (`KYTY_DRAW_PREP_BINDINGS=1`). Smaller state
copies, fewer resets, precomputed certificate ranges and in-place checks reduce
serial commit overhead. Worker wakeup thresholds were reduced after a measured
commit-wait problem; two workers stay hot while others park until needed.

**Expected:** move CPU work off the limiting thread and avoid duplicate evaluation.
A binding-plan microbenchmark showed about 10% less CP time per draw; projected
1.8–2.6 ms/flip was not an isolated game measurement. More workers can also increase
contention. Serial-oracle and verification modes are correctness checks, not timing
configurations.

### State and descriptor reuse

Pipeline-layout interning, sampler/program/pipeline/target-description memos,
dynamic-state shadows, unchanged render-target and stage-texture sequence reuse,
push-constant shadows and descriptor-set reuse avoid rebuilding identical native
state. Shader-data upload deduplication eventually favors a site's last upload over
a larger hashed content table. Texture binding memos validate page structure and
content identity before reusing images/views; changed pages can be revalidated.

**Expected:** reduce locks, hashing, allocation and driver calls per draw. Repetition
and descriptor-offset audits measure opportunity, not gains. The later U60 dynamic
descriptor and native-input-reuse candidates are not in U59.

## 3. Memory coherence, transfers and images

### Dirty tracking, epochs and buffer bindings

Incremental BDA synchronization, a dirty-range log, synchronization epochs,
cross-epoch clean-binding memos and written-sync skips replace repeated full-buffer
scans with work on changed ranges. Dirty-bit mirrors, per-thread committed-run
clamp caches, lock-free read snapshots and combined dirty queries reduce bookkeeping.
Precise write ranges avoid invalidating an entire resource for a narrow store.

Host writes now take the same tracking transition as guest write faults. This is
essential for CPU-written occlusion/LOD reports and other emulated stores to invalidate
old GPU copies. Verification accounts for genuinely concurrent writes separately
from deterministic mismatches. Fault-ahead opens only the affected pages and retires
known-fill information for those pages; hot-page behavior was corrected to remain
equivalent to fault tracking. A false-sharing write shortcut is experimental and off.

**Expected:** fewer scans, protections, unnecessary copies and locks. These caches
must not turn old GPU data into the authority after a CPU write.

### Readbacks and upload batching

Writer-tick side copies service protected guest reads without draining all GPU work;
a second universal-family queue can execute these transfers. Eager publication of
read-hot pages at completion and earlier submission of needed writers reduce late
readback stalls. The guest still waits for the bytes it actually depends on.

Uploads copy outside region-tracking locks, batch dirty ranges behind shared barriers,
and flush only when a batch contains work. A DMA queue and worker move staged bytes
without doing every host copy on the CP thread. Upload dedup removes repeated data.

**Measured diagnostic evidence:** desert `GpuWaitDrain` fell from approximately
4.6–5.6 ms/flip in U47 to 0.07 in U49; Sky Garden remained about 0.6–0.7 ms.
The U48→U49 start-view bundle changed 13.32→16.13 FPS with parallel preparation,
4K output and 120 Hz pacing. It is not a per-feature or current-upstream comparison.

### Texture residency, partial refresh and native allocation reuse

Only sampleable mip levels need registration, watching and upload. Resident-level
tracking follows view clamps and promotion; chunk tracking supports partial refresh
and asynchronous staging. Texture fault fast paths and texel synchronization skips
avoid whole-resource work when ownership already proves the needed bytes current.
Defaults changed during debugging: the resident-mip feature was re-enabled after
checks, while not every partial-refresh/writeback experiment is enabled.

Native image pooling recycles retired compatible Vulkan allocations. This reduces
allocation churn without reusing stale contents. One early L12 tail recorded 2,963
pool hits and 11 misses. Texture ownership clean proofs reached 98.66% hits in an
M13 capture, but differing workloads and worse p99 prevented a causal FPS claim.
The early pressure-based image-retirement policy is separate and remains `legacy`
in the tested preset.

### Tiling, direct copies and alias ownership

Direct depth/color reinterpretation copies, wider compatible-copy coverage and
batched buffer-mediated regions avoid needless detile/readback/re-upload paths.
Direct tiler image kernels also cover block-compressed uploads. Parity tests check
defined bytes, dispatch coverage and tile equations, including small depth-tiled
color views. Unsupported downloads are declined rather than crashing.

GPU ownership follows the byte ranges a write can reach, not just a nominal image
identity. Depth/color aliases remain available where needed; read-only target binds
retain content identity, and fresh GPU images supersede stale dirty-buffer bytes.
Range-aware writeback and alias synchronization prevent losing writes while avoiding
copies already satisfied by the current owner.

**Correctness/performance tradeoff:** broad preservation experiments outside U59
caused much more transfer work. Returning to U58 plus only the targeted water fix
observed 23.91→27.08 FPS, GPU busy 31.8→16.4 ms/flip and image uploads
390.2→134.0 MB/flip. This was a diagnostic bundle comparison, not a clean upstream
benchmark, and does not isolate each rejected preservation change.

### DCC, CMASK and the water fix

GPU DCC materialization handles eligible compressed-color clear/refresh work without
CPU readback. Known uniform metadata fills can avoid unnecessary downloads; pending
refresh inspection can stay on the GPU (`KYTY_DCC_GPU_REFRESH=1` in the preset).
CMASK clear support, metadata-only draw handling and draw-write sinking distinguish
logical clears from ordinary draws.

The targeted water fix materializes fast-clear elimination before sampling without
metadata. Previously a dropped eliminate left stale aliased contents visible in the
pool, especially from above. This is an accuracy fix; it is not a shortcut that
disables water. Its isolated overhead was not established under matched pacing.

### U59 vertex-input and stale-image corrections

Constant-selected embedded vertex inputs no longer require an unused native vertex
format. Attribute descriptions are compacted while shader locations and binding
indices remain intact; real memory inputs still require valid formats.

The final U59 change refuses CPU-dirty images as alias-materialization sources. It
prevents tiling an old GPU image back over newer CPU-written vertex descriptors.
This addresses the fast-start invalid-input failure whose frequency changed with
tracing and menu wait time. Failure dumps aid diagnosis; they do not make malformed
memory inputs valid. These are correctness changes, not demonstrated FPS gains or
a claim that every loading failure is fixed.

## 4. Occlusion, LOD feedback and geometry

### Occlusion queries: what they do and what they do not do

U59 implements GPU sample counters, begin/end scopes, cumulative guest dumps,
predicates and batched reductions. The dump-pair gate avoids unnecessary scopes
outside the intervals the game consumes. Visibility-proxy results use deferred
label publication: the corresponding completion label is delayed until the query
result is published, avoiding the older synchronous GPU wait.

This lets guest software use actual visibility results instead of an always-visible
answer. It does not add an independent scene-graph occlusion culler to the emulator.
Only work that the game subsequently suppresses can disappear. Query scope, result
format, host-write tracking and publication order matter for correctness.

**Performance evidence:** early R18 was slower than the saved P16 view after adding
queries and restart support; later batching and label deferral reduce query overhead.
There is no controlled result establishing a particular reduction in Sky Garden
draw calls attributable to occlusion. Do not market it as a proven draw-call saving.
Main controls include `KYTY_GPU_OCCLUSION`, `KYTY_OCCLUSION_GATE` and
`KYTY_OCCLUSION_PROXY_MODE` (deferred labels in the normal path).

### LOD and GET_LOD_STATS

The shader path produces GPU mip-feedback statistics and publishes the reports the
guest texture streamer consumes. Work includes report-slot intervals, reset-after-copy
ordering, completion publication, MIN_LOD/clamp counting and base-level interpretation.
The eventual clamp-counting semantics avoid treating every sample as a request for
the finest mip. Draws with no counter can use a plain pixel-shader variant without
feedback instrumentation (`KYTY_LOD_STATS_PLAIN_VARIANT`).

Texture residency then avoids uploading mip levels the view cannot sample. Sampler
and view fixes preserve the MIN_LOD tail of streamed head textures; this matters for
the formerly missing middle-distance leaf band. Offset sampling, explicit LOD clamps
and sampler reduction also affect correct rendering.

**Expected:** less streaming churn, smaller uploads and fewer shader atomics when no
feedback is needed. This is texture mip feedback, not a universal geometry-LOD
reduction or a forced low-detail option. The game chooses its LOD policy. Early
always-not-ready and record-time report experiments remain visible in history;
they are not interchangeable with later completion-correct behavior. No isolated
end-to-end LOD FPS percentage is established.

### Mesh restart and native indirect drawing

Mesh-shader primitive restart honors index-stream restart boundaries instead of
continuing strips across restart indices. Large instance counts are split to fit
host dispatch limits. Native GPU indirect draws and mesh argument conversion keep
supported argument processing on the GPU, with CPU fallback for unsupported cases.
Later work avoids the per-flip argument drain in supported mesh draws.

Restart and instance splitting primarily fix geometry correctness. Native indirect
execution can save CPU readback and synchronization; it does not reduce the number
of objects the guest requests. No restart-only FPS gain is claimed.

## 5. Render passes, barriers and shaders

### Dependencies and rendering-instance retention

Barrier batching merges compatible ranges/stages and elides redundant transitions
while retaining required dependencies. Depth feedback keeps a sampled read-only
attachment usable within its rendering instance. Unsampled depth targets retain
attachment layout when compatible with writes. No-op layout changes return before
barrier construction, and images already touched in a GC tick avoid redundant LRU
updates. Buffer writes on pages without images skip texture-cache scans.

**Expected:** fewer barriers, pass endings and CPU lookups, plus more GPU overlap.
These are dependency-sensitive optimizations, not permission to drop synchronization.
The later U60 stage-aware barriers and compute-batch experiments are excluded.

### Program and pipeline compilation

- Normalize pipeline keys, make eligible raster state dynamic and intern layouts,
  reducing unnecessary variants. Make generated SPIR-V IDs deterministic where
  dispatcher spills previously disturbed keys.
- Translate a program source once, specialize copies, compile outside the exclusive
  program-cache lock, and free evicted translations outside that lock.
- Persist translated sources/permutations with a build-time codegen hash. Include
  mesh workgroup Y/Z in the static key. Reload only compatible entries.
- Persist driver pipeline caches periodically with crash-safe saves. Move shader
  validation off the critical compilation path when that diagnostic is enabled.
- Graphics pipeline libraries remain an opt-in experiment, default off.

**Evidence:** profiling attributed about 10.7 seconds of CP translation time to
roughly 500 programs in a run; historical notes report warm reloads around
0.2–0.5 ms each. This principally targets loading and first-use stutter. It does
not establish a steady-state FPS increase. Cache serialization itself can stall.

### Shader code generation and ISA accuracy

The fork shortens exact f32 min/max/median and packed RTZ conversion sequences,
avoids duplicate float-to-int saturation, folds relative-register select chains
using known M0 sets, and removes EXEC selects that no lane can observe. Eligible
plain buffer loads use robustBufferAccess2 bounds behavior. Transcendental input
denormal handling has a shorter equivalent path; host FTZ is separately optional.

Accuracy work covers TFE/LWE behavior, readfirstlane, denorm/LDS ordering, image
offsets and LOD clamps, sampler min/max reduction, DPP row-share/DPP8, SNORM16 exports,
pixel interpolation modes and position-sensitive MAD/MAC contraction. CFG fixes
prevent repeated merge-gateway splitting. Pixel DS_APPEND/DS_CONSUME excludes helper
lanes so they cannot corrupt counters used by particles/GI work.

**Expected:** fewer shader instructions and less host translation work where exact
transformations apply. Accuracy corrections can add work. There is no measured
per-opcode Sky Garden FPS attribution.

## 6. Diagnostics, compatibility helpers and regression coverage

Tracy aggregates, named GPU timing, sampled operation timing, hang flight recorders,
CP ordering/repetition logs, memory/upload/fault counters, draw-prep fallback causes,
validation logging/deduplication and CPU-placement samples make costs attributable.
Instrumentation overhead itself was reduced using per-thread counters and cheaper
gates; disabled-profiler paths no longer assume Tracy is initialized. These tools
measure work and can alter scheduling; their FPS is not clean benchmark FPS.

The RenderDoc lifecycle correction coordinates capture with recorder/submission
completion. Capture tools still need their own stability checks. Game-patch filtering
allows controlled compatibility experiments without rewriting patch files.
Opt-in host input and minimum key-press duration support repeatable navigation;
they do not modify rendering. AMPR counter-bank/address/wait emulation improves
guest compatibility and makes hangs easier to diagnose.

Regression work includes memory/page/alias tests, tiler parity, shader corpus and
GPU execution tests, draw-prep serial comparison, recorder/sequencer lanes,
occlusion/LOD reports, event queues and cache serialization. Fixture lifetime,
defined-byte comparisons, pending-completion control, target linkage and test-failure
propagation were also corrected. Tests and merge commits are listed individually
in the catalog; none should be represented as a direct runtime speedup.

## 7. What was actually measured

| Comparison | Observed result | Interpretation |
|---|---|---|
| Early queued submission, desert | 29.601 → 33.249 Hz | Same-binary comparison with older instrumentation; not current upstream |
| Submission coalescing, desert | 33.643 → 35.471 Hz | Separate early comparison; not additive with every later gain |
| U54 → U55 Sky Garden start | 23.45 → 24.15 FPS | About 3% for a bundle; GPU busy roughly unchanged |
| U54 → U58 Sky Garden start | 23.45 → 29.18 FPS | Historical bundled result, about 24%; does not isolate sequencer/binding plans |
| U54 → U58 desert run | 65.51 → 61.11 FPS | Regression; reported p95 17.1 → 24.9 ms |
| Broad preservation candidate → water-only | 23.91 → 27.08 FPS | Diagnostic 1080p/60 Hz comparison; extra transfers removed |
| Clean U59 Sky Garden start | 27.53 FPS; p50 33.536, p95 49.720, p99 50.118 ms | A baseline, not a comparison against upstream |

The historical U54/U58 comparisons used 4K output and 120 Hz pacing with diagnostics.
The clean U59 baseline used 1080p, 60 Hz, Mailbox and a 30-second PresentMon window
with 823 analyzed intervals; all were over 16.7 ms. Test hardware was an RTX 3090
and Ryzen 9 7950X3D. Baseline compatibility patches select non-tiled deferred lighting
and disable GI probes/lighting shaders. Resolution, patches, pacing, view and capture
overhead must match before deriving an improvement. Raw private traces are not
distributed; these numbers summarize the retained measurement notes.

U59 is the selected last known good build, not a certification of all games or a
60 FPS result. Check both normal-angle and top-down water; menu/loading timing and
validation layers have hidden failures before. No new game benchmark or rebuild
was performed just to publish this source.

## 8. Reproducing the review

Use the published commit links in the catalog to inspect each change and its tests.
The source base and comparison upstream hashes above identify the original public
history; sanitized hashes are different because local files were removed throughout
history. Do not compute a new merge base between sanitized and unsanitized repositories
and mistake the rewritten ancestry for unrelated source development.

The catalog also lists all changed paths relative to the common base and all 76
upstream-only commits. Current upstream includes newer vertex semantics, pipeline
ownership/keys, buffer streaming, timing, shader and platform work that U59 does not
incorporate. Review those independently before proposing upstream contributions.
