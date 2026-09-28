# Fast-clear elimination before sampling

Color-buffer mode 2 is a fast-clear eliminate, not an ordinary shader draw. A game can clear
CMASK metadata, issue this operation, and then sample the surface through a descriptor that
has no metadata. Resolving clears only when an ordinary draw binds the render target loses
that initialization. The sampled image then exposes old contents of reused heap memory.

The renderer now resolves supported DCC/CMASK targets during eliminate/decompress operations
by default. It reuses the existing clear interpretation and consumes the clear metadata.
`KYTY_CB_METADATA_MATERIALIZE=0` restores the previous behavior for diagnosis; it is known
to break this sequence. Unsupported layouts are still outside this fix.

## Regression

`shader_recompiler_compute_tests --cmask-fast-clear-only` paints a target, fills its CMASK
with a pending clear, issues mode 2, and samples without rebinding the target or supplying
metadata. Both corner texels must contain the clear value and the metadata must be expanded.
A later paint/rebind must survive, proving the clear was consumed rather than reapplied.
The regression deliberately fails when materialization is disabled.

The same CMASK test and DCC elimination checks are included in `--compute-meta-clear-only`.
The normal, queued-recorder and inline-recorder variants pass under Vulkan validation.

## Live evidence

An affected water input contained old scene imagery and was not rewritten during gameplay.
The command trace showed a dropped eliminate immediately before its first sampled use.
Enabling materialization executed that clear; a native texture dump then contained the
requested zero RGBA value. The magnified foliage and white/rectangular corruption disappeared
at the tested pool from normal and top-down views. This is a correctness fix; performance
must be measured without native snapshots or verbose command tracing.
