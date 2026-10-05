# KytyPS5 U59 int14 pre-release — Less shader stutter, loading and shader crash fixes

> **Pre-release.** This is the int13 pre-release plus the changes below. If anything works worse than in int13, please
> report it (with your GPU and CPU model) and use the int13 release instead.

## What's new

- **Astro Bot without the "non RT patch" (int15 RT candidate):** the game's own tiled lighting, GI probes and ray
  traced shadows now run, so the patch is no longer needed. Without it the title screen robots used to be black and
  the lighting was missing, because the lighting shaders contain ray-tracing instructions and were skipped. Those
  instructions now run in software on any GPU. To play without the patch, remove or disable both mods of
  `_Patches\PPSA21567.json` (or `PPSA21564.json`). On the test PC (RTX 3090, Ryzen 9 7950X3D) the unpatched game ran
  about 3-4% slower than the patched one in Sky Garden and at the Creamy Canyon (snow) start. The first launch after
  an update builds the large lighting shaders, which can freeze the picture for up to about 20 seconds once.
  Switches: `KYTY_RT_SOFTWARE=0 KYTY_RT_STUB=1` keeps the lighting but lets every ray miss (no ray-traced shadows);
  `KYTY_RT_SOFTWARE=0` alone skips those shaders again (only useful with the patch on). The bundled preset's
  `KYTY_SRT_VARIANT_READS=1` is required; without it the lighting shaders are skipped.
- **Shader precompile:** the emulator remembers every shader it has built and rebuilds them in the background at the
  next launch, before the game needs them. Places you have already visited stutter much less on later visits: in
  Astro Bot's Sky Garden, 437 shaders were built during play on the first run and 8 on the second. The first launch
  works as before.
- **Faster first pipelines:** when the game needs a new pipeline, a quick version is built first and the fully
  optimized one replaces it in the background. In Sky Garden with an empty cache, the game waited 0.3 s in total for
  pipelines instead of 1.4 s.
- **Shader fix:** a kind of pixel shader that picks its texture at run time was translated incorrectly and stopped
  the emulator (for example at Demon's Souls' character creation).
- **Automatic GPU choice:** with the launcher's GPU setting on auto, the emulator could pick the wrong device on PCs
  with more than one, and the game then crashed in levels. It now prefers the dedicated graphics card with the most
  video memory, and the console shows the chosen GPU ("Kyty GPU: ..."). If a game crashes for you on auto, select your graphics card in
  the launcher.
- **Vulkan validation no longer stops the game:** with the launcher's Vulkan validation option on, Astro Bot stopped
  at boot. Validation messages now go to `_kyty_vulkan_validation.log`. Leave the option off for playing; it makes
  games much slower.

## Also in the int13 pre-release

- **Fewer crashes when loading:** game file reads into memory the emulator protects no longer look like empty files
  to the game. If int13 crashes for you while a level loads, try this build.
- **Faster AMD CPU patch (`--amd-cpu`):** in Sky Garden it is now as fast as without the option.
- **Low-VRAM cards, Intel GPUs and one compute shader:** fixes for cases that stopped the emulator.
- **Adaptive triggers** (also in int13): Astro Bot's L2/R2 actions, Astro's Playroom capsules and the gun work.

## Checked

- All automated tests pass.
- Astro Bot Sky Garden runs as fast as int13 on the test PC (RTX 3090, Ryzen 9 7950X3D): 33.0 fps against 33.4 fps,
  within run-to-run noise.

## Installing

1. Download `KytyPS5-U59-Windows-x64.zip` and extract it to a new folder.
2. Open `launcher.exe`. Your existing game list and settings are picked up automatically.
3. Game patches go in a `_Patches` folder next to the launcher; saves are kept per folder in `_SaveData`.

Both new features are switched on in `u59-preset.json` next to the launcher. If something looks wrong, set
`KYTY_SHADER_PRECOMPILE` or `KYTY_PIPELINE_FAST_FIRST` to `"0"` there and tell us which one it was.

## Known issues

- Microsoft Defender may flag `launcher.exe` (`Trojan:Win32/Bearfoos.A!ml`, a machine-learning verdict on the
  unsigned launcher). It is built from this repository's source by the GitHub workflow.

See `U59-README.md` in the download for the full list of changes and switches.
