# 12. Build, package and release

[← Index](TECHNICAL_DOCUMENTATION.md)

## 12.1 Requirements

| Tool | Version used | Needed for |
|---|---|---|
| Visual Studio 2026 (v145 toolset), "Desktop development with C++" | 18 Community | Build |
| Windows SDK | 10.0.26100.0 (named in `engine/Direct3D.props`) | Build |
| Git | any recent | Clone, submodule |
| Inno Setup 6 | not installed on the author's PC yet | Installer (optional until M2 is closed) |
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

It uses `robocopy`, so repeated runs only copy changes. The v0.1.0 stage is about 740 MB.

## 12.4 Package

```powershell
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

[confirmed: `tools/package.ps1`]

1. Builds and stages Release.
2. Copies the stage to `dist/CaucasianEagle-<version>-win64/` without debug files (`.pdb`, `.ilk`).
3. Writes `dist/CaucasianEagle-<version>-win64.zip` and its SHA-256.
4. If Inno Setup 6 is installed, compiles `tools/installer/CaucasianEagle.iss` into `dist/CaucasianEagle-<version>-Setup.exe`. If it is not installed, the script says so and stops after the zip.

`<version>` is read from `GameVersion` in `game/src/GameConstants.h`.

### The installer (`tools/installer/CaucasianEagle.iss`)

| Setting | Value | Why |
|---|---|---|
| Install location | `%LOCALAPPDATA%\Programs\Caucasian Eagle` by default | Per-user install, no administrator prompt |
| Privileges | `lowest` | Same |
| Shortcuts | Start menu; desktop optional | |
| Uninstaller | Yes; leaves `%LOCALAPPDATA%\CaucasianEagle` (settings and saves) in place | Player data survives a reinstall |
| Architecture | x64 only | The game is x64 only |

The installer script is written but **has not been compiled or run** in v0.1.0, because Inno Setup is not installed on the build PC.

## 12.5 Versioning

- Version format: `MAJOR.MINOR.PATCH`. The single source is `GameVersion` in `game/src/GameConstants.h`.
- Each released version gets a git tag `vMAJOR.MINOR.PATCH` and an entry in [`docs/changelog.md`](../docs/changelog.md).
- The engine version is the submodule commit recorded by the tagged game commit.

## 12.6 Release checklist

1. `git status` clean in both repositories; engine submodule on a pushed commit.
2. Bump `GameVersion`; update `docs/changelog.md` and the version lines in this tech plan.
3. `tools\package.ps1`.
4. Run the manual checklist in [08-testing.md](08-testing.md) against `dist\CaucasianEagle-<version>-win64\`.
5. Install with the Setup exe on a machine (or user account) that has never had the project; run the checklist's first three steps; uninstall.
6. Commit, tag `v<version>`, push with tags.
7. Attach the Setup exe, the zip and the SHA-256 file to a GitHub release.

## 12.7 Not in place yet

| Item | Milestone |
|---|---|
| Compiled and tested installer | M2 |
| Trimmed engine assets in the package | M2 |
| Game icon and version resource in the exe | M2 |
| Code signing | M6 |
| Visual C++ runtime check (the build links the runtime dynamically by default; a clean PC may need the redistributable) | M2 [unknown until tested on a clean machine] |
