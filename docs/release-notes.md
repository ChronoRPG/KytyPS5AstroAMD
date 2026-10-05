# KytyPS5 U59 int13 pre-release — Faster AMD CPU patch, stability fixes

> **Pre-release.** This is int13 plus the changes below. If anything works worse than in int13, please report it
> (with your GPU and CPU model) and use the int13 release instead.

## What's new compared to int13

- **Faster AMD CPU patch (`--amd-cpu`):** the launcher's AMD CPU patch now runs almost all of the game's affected
  instructions as native code. In Astro Bot's Sky Garden it is as fast as without the option (34.5 fps instead of
  31.6 fps on the test PC).
- **Fewer crashes when loading:** game file reads into memory the emulator protects no longer look like empty files
  to the game.
- **Low-VRAM cards:** when video memory runs out while a texture is converted, the emulator frees idle buffers and
  tries again instead of stopping.
- **Shader fix:** a kind of compute shader that stopped the shader translator on any GPU now works.
- **Intel GPUs:** some shaders now run with the subgroup width they need.

## Also in int13

- **Adaptive triggers:** Astro Bot's L2/R2 actions work, Astro's Playroom's gacha capsules break on R2, and the
  gun fires only when you press the trigger.

## Checked

- All 342 automated tests pass.
- Astro Bot Sky Garden and Creamy Canyon run as fast and look the same as int13 on the test PC (RTX 3090,
  Ryzen 9 7950X3D). Shader caches stay valid.

## Installing

1. Download `KytyPS5-U59-Windows-x64.zip` and extract it to a new folder.
2. Open `launcher.exe`. Your existing game list and settings are picked up automatically.
3. Game patches go in a `_Patches` folder next to the launcher; saves are kept per folder in `_SaveData`.

## Known issues

- Microsoft Defender may flag `launcher.exe` (`Trojan:Win32/Bearfoos.A!ml`, a machine-learning verdict on the
  unsigned launcher). It is built from this repository's source by the GitHub workflow.

See `U59-README.md` in the download for the full list of changes and switches.
