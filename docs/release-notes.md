# KytyPS5 U59 int16.1 pre-release — Astro Bot with full lighting, GI and ray tracing, no patches needed

> **Pre-release.** This is the int15.1 release plus Astro Bot's full graphics. If anything works worse than in
> int15.1, please report it (with your GPU and CPU model) and use int15.1, or turn the patches back on (see below).

## What's new in int16.1

- **Level title text fixed:** in int16 (and int15) the level name shown when Astro Bot enters a level was drawn wrong
  (broken letter sizes and spacing). Two font changes taken from upstream caused this and have been undone, so the
  titles look as they did in int14. The same fix is in the int15.1 main release.

## From int16

- **Astro Bot without the "non RT patch":** the game's own tiled lighting, GI probes and ray traced shadows now run,
  so the two patches are no longer needed. Without them the robots used to be black and the lighting missing, because
  the lighting shaders contain ray-tracing instructions and were skipped. Those instructions now run in software on
  any GPU (NVIDIA, AMD and Intel).
- **How to play without the patches:** open the launcher, select Astro Bot, open the cheats window and untick
  "Select the existing non-tiled deferred-lighting renderer" and "Disable GI probes and lighting shaders". If you
  see problems, tick them again; this build also runs fine with them.
- **Speed:** on the test PC (RTX 3090, Ryzen 9 7950X3D) the full graphics cost about 3% in Sky Garden (about 33.6 fps
  against 34.8 fps with the patches) and practically nothing at the snow level start (21.1 against 21.2 fps).
- **First launch:** the first start after an update builds the large lighting shaders, which can freeze the picture
  for up to about 20 seconds once.

## Also in int15 and int15.1

- No more "unsupported sampled depth image" stops on fast GPUs, no crash when leaving extra levels in Sky Garden,
  about 20 upstream shader fixes, a Demon's Souls fix for an unknown image format, and thread priority changes.
- From int14: shader precompile after updates, faster first pipelines, a better automatic GPU choice, fewer loading
  crashes, Demon's Souls character creation, and the adaptive trigger fixes.

## Checked

- All automated tests pass (int16.1 as well).
- int16.1: Astro Bot level titles checked on screen.
- Astro Bot 1.018 on one PC (RTX 3090, Ryzen 9 7950X3D): Sky Garden and the snow level with and without the
  patches, with no errors. Not tested by us on AMD or Intel GPUs or on version 1.007; player reports from RTX 50
  and AMD cards are listed under known issues.

## Installing

1. Download `KytyPS5-U59-Windows-x64.zip` and extract it to a new folder.
2. Open `launcher.exe`. Your existing game list and settings are picked up automatically.
3. Game patches go in a `_Patches` folder next to the launcher; saves are kept per folder in `_SaveData`.

## Known issues

- **RTX 50 series:** the first launch after installing or updating can crash once while the ray tracing shaders are
  being built. Start the game again; it works from then on.
- **AMD graphics cards:** with the two patches turned off, the GPU can stop responding ("device lost"), and the water
  in Go-Go Archipelago can make the frame rate drop sharply. On AMD, keep the patches on, or use int15.1.
- One crash during startup was seen once in testing and could not be reproduced. If the console shows a crash, please
  send it; it now names the code path.
- The software ray tracing changes behaviour for other games too (for example Demon's Souls); please report any new
  problem there.
- Microsoft Defender may flag `launcher.exe` (`Trojan:Win32/Bearfoos.A!ml`, a machine-learning verdict on the
  unsigned launcher). It is built from this repository's source by the GitHub workflow.

Switches for testing: `KYTY_RT_SOFTWARE=0 KYTY_RT_STUB=1` keeps the lighting but lets every ray miss (no ray-traced
shadows); `KYTY_RT_SOFTWARE=0` alone skips those shaders again (only with the patches on). The bundled preset's
`KYTY_SRT_VARIANT_READS=1` is required. See `U59-README.md` in the download for the full list of changes and switches.
