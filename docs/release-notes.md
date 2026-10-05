# KytyPS5 U59 int15 — Crash fixes, upstream shader fixes and less shader stutter

This is the new main release. It includes everything from the int13 and int14 pre-releases. Keep using the Astro Bot
"non RT patch" game patches with this build; the pre-release int16 runs the game's full graphics without them.

## What's new since int14

- **Fewer crashes on fast GPUs:** on fast graphics cards Astro Bot raises its resolution, reuses memory between
  images, and the emulator stopped with "unsupported sampled depth image". These images are now read correctly.
- **No crash when leaving extra levels:** leaving an extra level in Sky Garden could stop the emulator on an image
  clear it rejected. Such a clear is now skipped, and the console prints one `TextureCache: ClearImage skipped ...`
  line with the details. Please send that line if you see it.
- **Upstream fixes:** about 20 shader translation fixes, a fix for Demon's Souls stopping on an unknown image format,
  thread priorities that keep busy game threads from starving the graphics thread, and trophy notifications.

## From int14

- **Shader precompile:** the emulator records the shaders it has built, and after an emulator update it rebuilds the
  ones you have already seen in the background, so areas you have visited do not stutter again. The first time you
  play an area works as before.
- **Faster first pipelines:** a quick version of each new pipeline is used until the optimized one is ready. In Sky
  Garden with an empty cache, the game waited 0.3 s in total for pipelines instead of 1.4 s.
- **Automatic GPU choice:** on auto, the emulator now prefers your dedicated graphics card, and the console shows the
  chosen GPU ("Kyty GPU: ..."). If a game crashes for you on auto, select your graphics card in the launcher.
- **Fewer loading crashes, Vulkan validation no longer stops the game, Demon's Souls character creation works, and
  the adaptive trigger fixes.**

## Checked

- All automated tests pass.
- Astro Bot on the test PC (RTX 3090, Ryzen 9 7950X3D): Sky Garden 34.8 fps (int14: 34.1) and the snow level
  21.2 fps (int14: 21.7), within run-to-run noise.

## Installing

1. Download `KytyPS5-U59-Windows-x64.zip` and extract it to a new folder.
2. Open `launcher.exe`. Your existing game list and settings are picked up automatically.
3. Game patches go in a `_Patches` folder next to the launcher; saves are kept per folder in `_SaveData`.

If a game crashes, please send the console text with your GPU and CPU model.

## Known issues

- Microsoft Defender may flag `launcher.exe` (`Trojan:Win32/Bearfoos.A!ml`, a machine-learning verdict on the
  unsigned launcher). It is built from this repository's source by the GitHub workflow.
- A game patch made for another game version can crash the game. Use patch files that match your version exactly.

See `U59-README.md` in the download for the full list of changes and switches.
