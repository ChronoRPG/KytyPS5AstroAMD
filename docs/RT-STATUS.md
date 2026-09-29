# RT research branch: status and boundaries

Branch: `codex/rt-experimental`. This is an older renderer lineage with RT work;
it is not a merge into U59. Its final WIP checkpoint preserves 21 existing modified
or new source/test files. Publication did not rebuild or retest that checkpoint.
The original RT worktree remains unchanged.

## Implemented work

| Area | Implementation | Why it matters; performance status |
|---|---|---|
| Guest instruction decode | BVH instruction recognition, software node tests and a diagnostic all-miss stub | Enables translation; all-miss output is deliberately inaccurate and not a speedup |
| Software traversal | Box/triangle arithmetic, exact arithmetic references, bounded walks and cycle detection | Executes guest semantics on shader ALUs; runaway budgets may turn excess work into misses |
| Psr BVH decoding | Shared-exponent type-6 boxes, compact layouts, leaf/instance parsing and snapshots | Required before correct host acceleration structures can be built |
| Native acceleration structures | Vulkan BLAS/TLAS allocation/build/update ownership and ray-query test kernels | Can use RT cores for hierarchy traversal; synthetic benefit only so far |
| Hybrid traversal | Hardware traversal with the guest instruction's exact triangle leaf test | Retains more guest arithmetic than hardware triangles alone; edge differences remain |
| Native scene bridge (WIP) | Bounded immutable snapshot conversion, shared BLAS ownership, instance metadata, AABB/pointer uploads and ordered AS builds | Establishes lifetime and resource plumbing; synchronous snapshot conversion is not a viable per-frame design |
| Device integration (WIP) | Optional RT feature negotiation, software fallback, early Windows RT runtime preparation before guest VA reservation | Addresses fixed-address device initialization failures; enabling a device does not replace guest traversal |
| Compiler recognition (WIP) | Structural traversal candidate finder and SSA boundary auditing | Finds replacement candidates but does not safely replace their loops yet |
| Runtime SRT reads | Loop-variant descriptor reads, unmapped-flat-read handling and unsupported-program rejection | Avoids eagerly dereferencing a null TLAS; does not prove all shader callbacks are emulated |
| BDA writes | Runtime-descriptor stores/atomics plus tracked settling and race-aware checks | Needed for guest-built structures to become visible coherently |
| Shader control flow | Loop-break merge handling, remaining 64-bit comparisons, bit reverse, skipped-callee names/dumps | Allows more builder/traversal shaders to translate; unsupported callback builders remain a limitation |
| Capture diagnostics | Targeted BVH ordinals, arm files, attempts and capture-time native preparation | Establishes whether a real scene can be converted; may stall and must be excluded from timing |

## What hardware mode currently does

`KYTY_RT_HARDWARE=verify` requests native device capabilities. It does **not**
perform per-ray verification of replacement game shaders. `KYTY_RT_PREPARE_CAPTURE=1`
builds a native scene from a successfully captured snapshot, waits, reports metadata,
then releases it. It does not change the game's dispatch shader or bindings.

Guest traversal remains software. The candidate recognizer matched 13 captured
shaders, but 17–20 live outputs (including control-flow values) still need preservation
before substituting a hardware traversal. A structural match is not sufficient.

## Existing evidence, not rerun for publication

Standalone software GPU traversal matched a CPU reference on six scenes with
40,000 rays each. Hybrid traversal differed on grazing/edge rays: 1, 0, 1, 8, 3
and 67 differences across the six 20,000-edge-ray sets; random-ray sets had none.
Approximate hardware triangle mode had 2,168/20,000 and 721/20,000 edge differences
in two scenes. These are not bit-exact replacements.

| Synthetic scene | Software GLSL | RT-core traversal + exact leaf test | Approximate hardware triangles |
|---|---:|---:|---:|
| 1M surface triangles | 28.030 ms | 3.201 ms | 0.460 ms |
| 4M surface triangles | 86.861 ms | 6.756 ms | 0.779 ms |
| 1M triangle-soup stress | 957.940 ms | 92.883 ms | 6.472 ms |

These are standalone 1080p camera-ray experiments on an RTX 3090, not translated
Astro Bot gameplay. They exclude AS construction. For the 4M hybrid case, recorded
conversion/build costs were 1.486 + 10.210 ms for BLAS and 0.540 + 0.585 ms for TLAS;
BLAS refit alone was 2.933 ms. Caching, coherence and refits are essential.

Later native-bridge tests compared seven scenes with 20,000 random and 20,000 edge
rays each against the earlier GPU conversion path. Hit/miss and distance matched;
equal-distance tie order was not certified. The associated runs used counted Vulkan
validation. This does not erase the hybrid-versus-guest arithmetic differences.

Real GI captures exposed cyclic/degenerate hierarchy interpretations and, after
further decode work, an invalid/non-invertible instance transform. The native scene
bridge correctly rejected that snapshot. Lighting captures also found no plausible
TLAS. The cause may involve capture, decode or emulated builder behavior; this is
not proof that the game created invalid data or never uses RT.

The early Windows initialization change passed fixed-address startup probes. Its
live confirmation remained pending in the retained integration report. The preceding
game run reached Sky Garden via software fallback with validation enabled; it is
not hardware RT performance evidence, and that candidate's two water checks remain
pending.

## Build and investigation entry points

Use this branch's own CMake configuration. Relevant targets are
`rt_hardware_tests`, `rt_traversal_tests`, and `shader_recompiler_compute_tests`.
Representative entry points include `--bvh-only` for instruction coverage and
`rt_hardware_tests --prepare-bvh-dump <snapshot.kbvh>` for native snapshot preparation.
Use `KYTY_RT_TEST_VALIDATION=1` where the harness supports counted validation.
Startup probe modes include `--device-after-memory`, `--device-warm-memory`,
`--device-full-startup`, and `--device-full-prepared`.

Before claiming gameplay acceleration: obtain a valid active game hierarchy;
preserve traversal outputs/EXEC semantics; bind the host structures with correct
lifetimes and invalidation; verify representative game rays; check visuals and
both water angles; then run a separate matched clean HW/SW timing comparison.
There is no measured Astro Bot HW/SW speedup yet.
