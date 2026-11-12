# 12. Build, package and release

[← Index](TECHNICAL_DOCUMENTATION.md)

## 12.1 Requirements

| Tool | Version used | Needed for |
|---|---|---|
| Visual Studio 2026 (v145 toolset), "Desktop development with C++" | 18 Community | Build |
| Windows SDK | 10.0.26100.0 (named in `engine/Direct3D.props`) | Build |
| Git | any recent | Clone, submodule |
| Inno Setup 6 | 6.7.3, installed for the user (`winget install JRSoftware.InnoSetup`) | Installer |
| Python 3 with Pillow | any recent | Only to redraw the icon, pointer and HUD (`art/ui`) |
| Blender 5.2 | 5.2 | Only to rebuild art |

## 12.2 Build

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1            # Release
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Run       # and start the game
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Configuration Debug
```

What it does [confirmed: `tools/build.ps1`]:

1. Initialises the `engine` submodule if missing.
2. Restores the DirectX Tool Kit NuGet package into `engine/packages/` (first run only).
3. Builds `CaucasianEagle.slnx`: `DoorwaysEngine.lib`, then `CaucasianEagle.exe`.
4. Runs `tools/stage.ps1`.

Outputs (all ignored by git):

| Path | Content |
|---|---|
| `x64/Release/DoorwaysEngine.lib` | Engine static library |
| `build/obj/` | Game object files |
| `build/stage/CaucasianEagle/` | **The runnable game**: exe plus `assets/` |
| `build/stage-debug/CaucasianEagle/` | Debug build of the same |

A first build compiles the whole engine (several minutes). Later builds recompile only what changed.

## 12.3 Stage

`tools/stage.ps1` copies into the stage folder [confirmed: `tools/stage.ps1`]:

1. From `engine/assets`: `shaders`, `ui`, `textures`, `models`, `Compiled`.
2. From `game/assets`: everything, on top.

It uses `robocopy`, so repeated runs only copy changes and never delete: files of older builds stay in the stage. That is why the package is not made from it (12.4).

## 12.4 Package

```powershell
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

[confirmed: `tools/package.ps1`]

1. Builds and stages Release (`-SkipBuild` uses the executable already staged; `-Exe <path>` packs another one).
2. Assembles `dist/CaucasianEagle-<version>-win64/` fresh: the engine's runtime assets, the game's assets on top (the same folders as the stage, but nothing left over from older builds), `CaucasianEagle.exe`, and the Visual C++ runtime beside it (`msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` from the Visual Studio redistributable folder), so a PC without Visual Studio can start it.
3. Writes `dist/CaucasianEagle-<version>-win64.zip` and its SHA-256 (`-NoZip` skips this).
4. If Inno Setup 6 is installed, compiles `tools/installer/CaucasianEagle.iss` into `dist/CaucasianEagle-<version>-Setup.exe` and writes its SHA-256. If it is not installed, the script says so and stops after the zip.

`<version>` is read from `GameVersion` in `game/src/GameConstants.h`.

Sizes at v0.3.0: folder 916 MB (about 490 MB of it engine assets the game may not need, debt D1), zip 781 MB, Setup file 735 MB.

### Icon and version information

`game/CaucasianEagle.rc` is compiled into the executable: icon resource `1` (`game/CaucasianEagle.ico`, drawn by `art/ui/build_icon.py` from the rendered start picture) and a version block. The engine's window takes icon `1` of the executable (`App::InitWindow`). The version numbers in the `.rc` file are written by hand and must be changed with `GameVersion`.

### The installer (`tools/installer/CaucasianEagle.iss`)

| Setting | Value | Why |
|---|---|---|
| Install location | `%LOCALAPPDATA%\Programs\Caucasian Eagle` by default | Per-user install, no administrator prompt |
| Privileges | `lowest` | Same |
| Shortcuts | Start menu; desktop optional | |
| Uninstaller | Yes; leaves `%LOCALAPPDATA%\CaucasianEagle` (settings and saves) in place | Player data survives a reinstall |
| Architecture | x64 only | The game is x64 only |
| Icon, publisher | `game/CaucasianEagle.ico`; Giorgi Samkharadze | |

First compiled and run for v0.3.0; what was checked is in [08-testing.md](08-testing.md), section 11.2b. It is not signed.

## 12.5 Versioning

- Version format: `MAJOR.MINOR.PATCH`. The single source is `GameVersion` in `game/src/GameConstants.h`.
- Each released version gets a git tag `vMAJOR.MINOR.PATCH` and an entry in [`docs/changelog.md`](../docs/changelog.md).
- The engine version is the submodule commit recorded by the tagged game commit.

## 12.6 Release checklist

1. `git status` clean in both repositories; engine submodule on a pushed commit.
2. Bump `GameVersion`, the four version numbers in `game/CaucasianEagle.rc` and the default in `tools/installer/CaucasianEagle.iss`; update `docs/changelog.md`, the README, the manual and the version lines in this tech plan.
3. `tools\package.ps1`.
4. Run the manual checklist in [08-testing.md](08-testing.md) against `dist\CaucasianEagle-<version>-win64\`.
5. Install with the Setup exe on a machine (or user account) that has never had the project; run the checklist's first three steps; uninstall.
6. Commit, tag `v<version>`, push with tags.
7. Attach the Setup exe, the zip and the two SHA-256 files to a GitHub release (`gh release create v<version> ...`).

## 12.7 Not in place yet

| Item | Milestone |
|---|---|
| Trimmed engine assets in the package | M2 |
| Installer tried on a PC that never had the project (the runtime DLLs are packed for it, the test is not done) | M2 |
| Code signing | M6 |
