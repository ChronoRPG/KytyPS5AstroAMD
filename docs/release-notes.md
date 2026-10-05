# KytyPS5 U59 int13 — Adaptive trigger fixes for Astro's Playroom

## What's new

- **Adaptive triggers in Astro's Playroom:** the gun no longer fires on its own. A vibration trigger now counts as
  firing only while you actually press it.
- Everything else is the same as int11. Your shader caches stay valid.

## Recent trigger fixes (int10 – int13)

- **Astro Bot:** L2/R2 actions such as punches work in the levels where they did nothing.
- **Astro's Playroom:** the gacha capsules break on R2 again, and the gun fires only when you press the trigger.
- Works with DualSense, other controllers and the keyboard. An L2/R2 that arrives as a plain button (input remapping,
  some controllers and tools) counts as fully pressed.

## Highlights since int7

- **RTX 50 freeze fixed.** Confirmed by a player on an RTX 5090.
- **AMD, Intel and older GPUs:** many places where the emulator stopped because a driver lacked an optional feature
  now carry on.
- **AMD/Intel CPUs:** faster "AMD CPU patch" (`--amd-cpu`), and Intel CPUs can run instructions they lack.
- **Upstream KytyPS5 changes:** rendering fixes, DualSense vibration and volume settings, cursor auto-hide and more.

## Installing

1. Download `KytyPS5-U59-Windows-x64.zip` and extract it to a new folder.
2. Open `launcher.exe`. Your existing game list and settings are picked up automatically.
3. Game patches go in a `_Patches` folder next to the launcher; saves are kept per folder in `_SaveData`.

The first launch of a game after an update to int9 or later rebuilds its shader cache, so expect slow loading and
some stutter until it fills.

## Known issues

- Microsoft Defender may flag `launcher.exe` (`Trojan:Win32/Bearfoos.A!ml`, a machine-learning verdict on the
  unsigned launcher). It is built from this repository's source by the GitHub workflow.
- Tested on one PC (RTX 3090, Ryzen 9 7950X3D). Please report problems with your GPU and CPU model.

See `U59-README.md` in the download for the full list of changes and switches.
