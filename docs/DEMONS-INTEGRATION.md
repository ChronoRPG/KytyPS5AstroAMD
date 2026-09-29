# Demon's Souls changes on experimental main

The Demon's Souls shader research branch was merged into `main`, preserving its
individual commits and authorship. Main also received four renderer fallbacks
and two draw-preparation page-hint commits from the later validation branch.
Always-on draw diagnostics and private handoff files were excluded. The `u59`
and `u59-windows-*` tags still identify the previous packaged U59 source.

The shader commits cover constant-mask folding, dead phi webs, whole-wave
EXEC/VCC branching, typed buffer store packing, and an immediate offset in
indirect-image materialization. The renderer commits avoid exiting for certain
missing shader stages or unusable sampled textures; such fallbacks can hide
content and should not be counted as speedups. The page hint can be restored to
its prior behavior with `KYTY_DRAW_PREP_GPU_DIRTY_HINT=1`.

## Validation

- Each renderer change compiled in isolation. The final cumulative candidate
  passed the `draw_prep`, `resource_tracking`, and `shader_recompiler_compute`
  tests sequentially. Focused wave-branch and typed-store shader cases passed.
- Astro Bot loaded the Sky Garden start view with normal geometry and water
  from normal and top-down angles on the cumulative build. The first three
  renderer fallback changes also passed separate visual runs. A build with
  only the page-hint changes passed both water views.
- These in-game checks were on one Windows system with an RTX 3090. They do not
  establish correctness in every scene, other games or other GPUs.
- No reliable steady-state FPS comparison exists for this merge. A prior
  single-run page-hint result was promising, but subsequent PresentMon files
  covered under two seconds of their requested 30-second windows. No 60 FPS
  claim is made.

The published indirect-image implementation retains the record immediate as
a separate selector field. The local cumulative test build folded aligned
immediates into the selector offset. These are related implementations, not
byte-identical source. The exact merged source compiled in Release mode on
Windows, including `kyty_emulator`, and passed `draw_prep`, `resource_tracking`,
and `shader_recompiler_compute` (3/3). These tests do not replace a new in-game
visual and performance check of the published source.
