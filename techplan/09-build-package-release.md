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

Sizes at v0.3.0: folder 916 MB (about 490 MB of it engine assets the game may not need, debt D1), zip 781 MB, Setup file 735 MB. At v0.5.0 (with Map A): folder 3,086 MB, Setup file 1,814 MB, zip 2,031 MiB, close to GitHub's 2 GiB limit for one release file (risk R10).

Since 0.5 `CaucasianEagle.exe` starts the Eagle's Ascent. The package folder has `Play-TheRoad.cmd` (`--road`) beside the executable; the installer's **Caucasian Eagle** shortcut (and the desktop one, ticked by default) starts the Ascent, **Caucasian Eagle - The Road (0.4)** the road.

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

First compiled and run for v0.3.0; what was checked is in [08-testing.md](08-testing.md), section 11.2b. Since 0.5.1 its file properties name the company, product and copyright (`VersionInfo*`, `AppCopyright`).

### Signing (since 0.5.1; no identity yet)

The owner's decision (2026-11-20): releases stay unsigned until version 1.0.0; the certificate is to be got then. Until then `tools\release\Test-Release.ps1` is run with `-AllowUnsigned`.

Up to 0.5.1 nothing of the game's own is signed: the installer, `CaucasianEagle.exe` and the uninstaller `unins000.exe`. That is why Windows warns (SmartScreen: unknown publisher, no reputation; Smart App Control, where it is on, may block). The audit: [`docs/release-audits/v0.5-to-v0.5.1/REPORT.md`](../docs/release-audits/v0.5-to-v0.5.1/REPORT.md). The Visual C++ runtime DLLs keep Microsoft's signatures.

- `tools\package.ps1 -Sign` signs `CaucasianEagle.exe` in the package folder before the zip and the installer take it, and has Inno Setup sign the installer and its uninstaller (`SignTool=cesign`, `SignedUninstaller=yes`, only when `-Sign` defines `SignInstaller`). Any failure stops the packaging.
- `tools\release\Sign-Files.ps1`: signtool, `/fd SHA256`, an RFC 3161 timestamp (`/tr`, `/td SHA256`), then `signtool verify /pa` and `Get-AuthenticodeSignature` must say Valid with a timestamp. A file signed by another publisher (Microsoft's DLLs) is left as it is. Two ways: a certificate in the Windows store by thumbprint (a hardware token shows there too), or Microsoft Artifact Signing (its dlib and metadata json, signing in through Azure). Settings in `tools\release\signing.local.json` (git-ignored, from `signing.template.json`) or `CE_SIGN_*` variables; no password or key in the repository, on the command line or in a log. Exit 2 when nothing is set up.
- Signing is to be done once per version, on the final files: re-signing changes the bytes and so the SHA-256.
- Reputation: a signed new file can still be warned about until the publisher's certificate has a reputation (Microsoft: no fixed threshold); signing makes it carry from version to version instead of starting again with every unsigned file.

### The release gate (`tools\release\Test-Release.ps1`)

Run after packaging: `GameVersion`, the executable's and the installer's version information, the installer's publisher and copyright, each `.sha256` against its file, each file under 2 GiB, that only the game, Microsoft's three runtime DLLs and `Play-TheRoad.cmd` are programs in the package, the DLLs' Microsoft signatures, our signatures with timestamps (`-AllowUnsigned` reports them instead of failing), the installed uninstaller's signature if installed, `-Scan` a Defender scan of copies, and `-Published` every download link of the GitHub release (HTTP 200, the listed size, and the bytes its `.sha256` names). A checksum shows a file is the one the release lists; only a signature says who made it.

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
8. `tools\release\Test-Release.ps1 -Version <version> -Published`: every link answers and matches. A version, once published, is never changed: no new files under its tag, no re-signed bytes; a fix is a new version.

Before step 3, with a signing identity: `tools\package.ps1 -Sign`, then `tools\release\Test-Release.ps1 -Version <version> -Scan` (without `-AllowUnsigned`).

## 12.7 Not in place yet

| Item | Milestone |
|---|---|
| Trimmed engine assets in the package | M2 |
| Installer tried on a PC that never had the project (the runtime DLLs are packed for it, the test is not done) | M2 |
| Code signing: the tools are in place (0.5.1); the owner's signing identity is not | M6 |
