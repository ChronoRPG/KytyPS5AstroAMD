# Building and launching U59

`main` uses the U59 source checkpoint plus the Demon's Souls shader, renderer
fallback and page-hint changes described in [the integration notes](DEMONS-INTEGRATION.md).
The `u59` tag and the existing Windows release tags still identify the original
sanitized U59 checkpoint. No U60 renderer changes are included. RT remains separate.

Follow the Windows requirements in [README](../README.md#build-requirements-windows):
Git, Visual Studio C++ tools, clang-cl, Ninja, CMake, Qt 6 for MSVC and glslangValidator.
Clone recursively, or initialize dependencies with `git submodule update --init --recursive`.
Configure Release in an x64 Visual Studio developer shell using the documented Qt path;
add `-DKYTY_EMULATOR_IPO=ON` to match the optimized experimental configuration.
Build `launcher` and install to `_Build/windows/install` using the README commands.

```powershell
./tools/Launch-U59.ps1
# Or use a separate installed runtime:
./tools/Launch-U59.ps1 -RuntimeDirectory 'D:/Kyty-runtime'
# Inspect the environment without launching:
./tools/Launch-U59.ps1 -PrintSettings
```

`tools/u59-preset.json` contains the portable no-diagnostics U59 renderer environment.
The five optional features are enabled: CP recorder, program cache, draw-prep binding
plans, DCC GPU refresh and CP sequencer. Shader metadata backing reads are off and
label mode is `record`, matching the selected U59 configuration. Failure diagnostics
are off for clean timing; they are distinct from the renderer settings.

Set game paths and runtime options in the launcher. For the recorded U59 baseline,
use 1920x1080, 60 Hz, Mailbox; disable Tracy, RenderDoc, Vulkan/shader validation and
shader logging. The script does not rewrite the shared INI or remove external
capture layers. It isolates its child from inherited KYTY/TRACY environment settings and
does not impose machine-specific CPU affinity. Use a separate runtime for experiments
and preserve saves/caches. The clean baseline used compatibility patches selecting
non-tiled deferred lighting and disabling GI probes/lighting shaders; these game
patch files are not distributed here.

The Windows release archive is built from the U59 main branch and includes a portable
launcher. After extracting it, open `Launch-U59.cmd`, or run `./Launch-U59.ps1`
in PowerShell. The archive contains no `Kyty.ini`, so the launcher uses its normal
shared settings file at `C:\ProgramData\Kyty\Kyty.ini`. Existing game directories and
per-game settings remain available. If the first U59 archive left a `Kyty.ini` beside
the launcher, this script backs it up before opening the launcher. It does not
modify the shared settings file. Users without shared settings should configure
resolution, validation and other options in the launcher. No game files, saves,
caches or patches are distributed. Retained source and
dependency pins were compared with U59; documentation preparation does not constitute
a fresh full test run. Historical test/results and their limitations are described
in [CHANGES-U59.md](CHANGES-U59.md). Check both normal and top-down water before
accepting a rendering change. Use a clean timing run separately from diagnostics.

The U59 Windows release is produced by a tagged GitHub Actions build. Original
licenses and credits remain intact. Personal
handoffs, local editor configuration, captures, saves and caches are excluded;
sanitizing historical files changes affected commit hashes while preserving commits
and merge relationships. U60 remains available only in the original local development
history, not on the published main lineage.
