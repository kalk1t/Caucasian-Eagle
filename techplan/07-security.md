# 10. Security and privacy

[← Index](TECHNICAL_DOCUMENTATION.md)

Caucasian Eagle v0.1 is an offline single-player game. Its security surface is small; this page records it so it stays small on purpose.

## 10.1 What the game does and does not do

| Area | v0.1 | Evidence |
|---|---|---|
| Network | The game code opens no connections | [confirmed: no socket, HTTP or WinINet use in `game/src`] |
| Accounts, telemetry, analytics | None | [confirmed: `game/src`] |
| Engine menu items "Website" and "Forum" | Present in the engine's main menu; what they open has not been checked | [unknown] |
| Files written | Only under `%LOCALAPPDATA%\CaucasianEagle\` (engine settings and saves; the game's `journey.sav`) | [confirmed: `engine/src/DOORWAYS/DoorwaysSettingsPaths.cpp`, `Journey::Save`] |
| Files read as data | `assets/Compiled/World/Road.txt` and `journey.sav`, both parsed with stream extraction into fixed fields; values are clamped (`Journey::Load`), a bad save is ignored | [confirmed: `Journey::LoadRoad`, `Journey::Load`] |
| Environment variables | Five development switches; the test pilot cannot write the save | [confirmed: `Journey::Initialize`, `Journey::Save`] |
| Administrator rights | Not needed to run | [inferred: user-profile writes only] |
| Input | Keyboard and mouse state read with `GetAsyncKeyState` while the game owns input | [confirmed: `NyxaraKit::ReadKeys`] |

## 10.2 Secrets

- The Meshy API key used to generate art is **never** stored in this repository. `art/nyxara/meshy/meshy.py` reads it from the environment [confirmed by scanning the tracked files for key patterns before the first commit].
- `art/nyxara/meshy/task_log.jsonl` (task ids and download links) is ignored by git.
- No signing certificate exists yet. When one is obtained it must stay outside the repository ([09-build-package-release.md](09-build-package-release.md)).

## 10.3 Release integrity (planned)

| Item | Status |
|---|---|
| Code-signed executable and installer | Not done. Unsigned builds trigger the Windows SmartScreen warning. Planned for M6. |
| Checksums published with each release | Done since v0.3.0: `tools/package.ps1` writes a `.sha256` file for the zip and for the Setup file; both are attached to the release. |
| Engine diagnostics and validation modes disabled for players | The engine has an access model (player / developer audiences). How a release build is locked to the player audience has not been verified for the game. [unknown] |

## 10.4 Third-party content and licences

| Item | Source | Note |
|---|---|---|
| Hero meshes, rig and base animations | Generated with Meshy under the author's account | Licence terms depend on the Meshy plan in force when generated. [unknown: not recorded in the repository] |
| DirectX Tool Kit | Microsoft, MIT licence | Restored by NuGet, linked statically |
| Fonts and UI artwork | Engine assets (`engine/assets/ui`) | Licence recorded in the engine repository, not re-checked here [unknown] |

A licence and credits file for the shipped game is part of M6.
