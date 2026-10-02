# U59 integration release (Windows x64)

This release builds on the U59 renderer and the Demon's Souls changes of the previous U59 release.

## What is new

- Command-processor work, behind flags that the bundled `u59-preset.json` turns on:
  - a fix for draw-preparation workers that stopped waking (`KYTY_DRAW_PREP_COLD_TOKEN`);
  - cheaper per-draw commits (`KYTY_CP_COMMIT=all`);
  - descriptor sets written on the recorder thread, push-descriptor and metadata-clear memos, and fewer GPU progress
    queries (`KYTY_RECORDER_DESCRIPTOR_SETS`, `KYTY_PUSH_SHADOW_FRESH_SKIP`, `KYTY_META_CLEAR_MEMO`,
    `KYTY_PENDING_REFRESH_US`).
- Lower VRAM use, also behind preset flags:
  - sparse residency for partially resident textures and for the BDA page table;
  - idle limits for the native image pool and the tiler scratch pool;
  - images unused for 600 frames are freed.
- `kyty_emulator.exe` is built with profile-guided optimization (PGO). The profile was recorded in Astro Bot
  (`tools/pgo/`).
- Upstream KytyPS5 changes up to the merge: controller, audio and compatibility fixes, and the layered VideoOut
  presenter.

## Measured

Astro Bot (PPSA21567), Sky Garden start view; RTX 3090, Ryzen 9 7950X3D; 1920x1080 output at a 120 Hz vblank.
These are separate timed runs on one PC, with the U59 renderer settings plus the bundle flags that the preset adds.

| | Before (earlier U59 test build) | This build |
|---|---:|---:|
| Frame rate | 28.2 fps | about 36 fps |
| Dedicated VRAM | 12.7 GB | 9.7 GB |

- The VRAM figure is the mean over the timed minute. A spike to about 13 GB can occur for a moment while the galaxy
  map streams.
- Demon's Souls boots to its menu with the preset. Its frame rate with this build was not measured.

## Launching

Extract the archive and open `launcher.exe` directly. When `u59-preset.json` is beside the executable, the launcher
applies its environment and clears inherited KYTY/TRACY variables. The archive contains no `Kyty.ini`: the launcher uses
the shared settings file `C:\ProgramData\Kyty\Kyty.ini`, so existing game directories and per-game settings stay
available. If an older archive left a `Kyty.ini` beside the launcher, move it aside. No game files, saves, caches or
patches are distributed.

Optional: `"KYTY_PRESENT_BOX_DOWNSCALE": "1"` in `u59-preset.json` presents the 4K frame with a two-texel box filter
when the window is between half and full size (for example 2560x1440), which removes a fine one-pixel stipple
the default blit leaves. It is off by default; other window sizes are unaffected.

## Caveats

- The first launch of each game compiles its shaders again. Program caches from older builds are not reused, so the
  first load is slow and the game stutters until the cache fills.
- The PGO profile comes from Astro Bot only. Other games run with code laid out for Astro Bot.
- The upstream controller and audio changes were not tested by hand.
- The preset also sets `KYTY_SRT_VARIANT_READS=1` (needed by Demon's Souls) and `KYTY_CPU_RESERVE=cp`. The Astro Bot
  measurements above were taken without these two.

## Building from source

Follow the Windows requirements in [README](../README.md#build-requirements-windows) and clone recursively. Configure
Release with Ninja in an x64 Visual Studio developer shell, as in `.github/workflows/u59-windows-release.yml`:

- Use clang-cl, lld-link and llvm-lib from LLVM 22.1.3: the profile needs the compiler version that recorded it.
- Add `-DKYTY_EMULATOR_IPO=ON` and `-DKYTY_PGO_USE=<checkout>/tools/pgo/u59-int-up-sg-1.profdata`. Without
  `KYTY_PGO_USE` the build works, but without the profile's speedup.
- Build `launcher` and `kyty_emulator`, install to `_Build/windows/install`, and copy `tools/u59-preset.json` beside
  `launcher.exe`.

The Windows release is produced by a tagged GitHub Actions build. Original licenses and credits remain intact.
Personal handoffs, local editor configuration, captures, saves and caches are excluded. Historical results and their
limitations are described in [CHANGES-U59.md](CHANGES-U59.md).
