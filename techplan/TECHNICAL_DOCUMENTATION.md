# Caucasian Eagle: Technical Documentation

> **Scope.** This documents the game **as it is** at version **0.2.0** (2026-11-10): the first version with the whole road to Amirani.
> **Audience.** Whoever maintains, extends, packages or releases the game.
> **Repository.** `C:\Users\User\Projects\Caucasian-Eagle` (git remote `kalk1t/Caucasian-Eagle`, branch `main`). All paths are relative to the repository root unless stated otherwise.
> **Engine.** The Doorways engine lives in its own repository (`kalk1t/Doorways-D3D11`) and is included here as the git submodule `engine/`. Engine internals are documented there, in `engine/techplan/`. This plan documents the game and the line between the two.

## Evidence labels

Every claim that is not obvious from a file citation carries one of these labels:

| Label | Meaning |
|---|---|
| **[confirmed]** | Read in the code or config, or observed by running it (the cited file is the source of truth) |
| **[inferred]** | Follows from the code, comments or docs, but wasn't directly observed |
| **[unknown]** | Not answerable from the repository; listed in [Coverage and open questions](12-coverage-open-questions.md) |

## Document map

This page is the index plus sections 1 to 3. Each remaining section has its own file.

| # | Section | File |
|---|---|---|
| 1 | Overview | this page |
| 2 | Technology stack | this page |
| 3 | Repository structure | this page |
| 4 | Architecture | [01-architecture.md](01-architecture.md) |
| 5 | Startup and configuration | [02-startup-config.md](02-startup-config.md) |
| 6 | Core workflows | [03-workflows.md](03-workflows.md) |
| 7 | Components and functions | [04-components.md](04-components.md) |
| 8 | Data: assets and saved state | [05-data.md](05-data.md) |
| 9 | Engine interface | [06-engine-interface.md](06-engine-interface.md) |
| 10 | Security and privacy | [07-security.md](07-security.md) |
| 11 | Testing | [08-testing.md](08-testing.md) |
| 12 | Build, package and release | [09-build-package-release.md](09-build-package-release.md) |
| 13 | Technical debt and risks | [10-tech-debt-risks.md](10-tech-debt-risks.md) |
| — | Constants and important values (table) | [11-constants.md](11-constants.md) |
| 14 | Coverage and open questions | [12-coverage-open-questions.md](12-coverage-open-questions.md) |

Other documents in this repository:

| Document | What it is |
|---|---|
| [`README.md`](../README.md) | Project front page: what the game is, how to build and run it |
| [`story/STORY.md`](../story/STORY.md) | Story bible (legend, premise, characters, chapter structure, open questions) |
| [`milestones/README.md`](../milestones/README.md) | Road from v0.1 to the installable v1.0 |
| [`docs/manual.md`](../docs/manual.md) | Player manual: controls, skills, combos |
| [`docs/changelog.md`](../docs/changelog.md) | What changed in each version |
| [`docs/glossary.md`](../docs/glossary.md) | Names used in code, assets and story |
| [`art/README.md`](../art/README.md) | Where the source art lives and how game assets are built from it |

---

## 1. Overview

**Caucasian Eagle** is a single-player third-person action game for Windows. The player is **Nyxara**, daughter of the eagle that feeds on the chained hero Amirani in the Georgian legend. The story is her seven-year road to the mountain to fight her father and free Amirani ([story](../story/STORY.md)).

What version 0.2.0 contains:

- **One hero.** Nyxara: a winged fighter with two swords, 31 authored animation clips, built from Meshy-generated parts in Blender [confirmed: `art/nyxara/blender/assemble.py`, `game/src/CaucasianEagleGame.cpp`].
- **One world.** Moonlit Sanctuary and a 6.3 km road through twelve regions to the mountain of Amirani, as one scene [confirmed: `game/src/GameConstants.h` `StartSceneId`, `game/assets/Compiled/Scenes/RoadToAmirani.dscene`, `art/world/build_world.py`].
- **Combat kit.** Alternating left/right sword slashes, a charged slash, three skills, six skill-plus-click combos, a sword swap, a two-sword barrier and a wing boost [confirmed: `game/src/NyxaraKit.cpp` `Acts` table].
- **Enemies.** Ten creatures of Caucasian folklore, three region bosses and the Eagle, in 34 arenas and 65 roaming packs; 81 breakable chests [confirmed: `game/src/Journey.cpp` `Kinds`, `Road.txt`]. The kit's original shade enemies remain in the code but are switched off.
- **The journey.** Gates, waves, rewards, story cards, the ending, a save file and an unattended test pilot [confirmed: `game/src/Journey.cpp`].
- **HUD.** Health and soul bars, an eight-slot skill bar with cooldowns, a combo prompt, enemy health bars and a skill-name banner [confirmed: `game/src/NyxaraHud.cpp`].
- **Story as text cards only.** No voice, cutscenes or dialogue choices. The engine's main menu, settings, character progression and inventory screens are still the engine's defaults [confirmed by running the game].
- **No sound.**

**Status.** v0.2.0 builds and runs from the stage folder; its journey was run end to end by the test pilot ([08-testing.md](08-testing.md)). It is not yet packaged as an installer; that is milestone M2 ([milestones](../milestones/README.md)).

## 2. Technology stack

| Layer | Technology | Where |
|---|---|---|
| Language | C++20, MSVC toolset v145 (Visual Studio 2026) | `game/CaucasianEagle.vcxproj` |
| Engine | Doorways (Direct3D 11), linked as the static library `DoorwaysEngine.lib` | `engine/` submodule, `engine/src/DOORWAYS/DoorwaysEngine.vcxproj` |
| Graphics API | Direct3D 11, HLSL shaders compiled at run time by the engine | `engine/assets/shaders/` |
| Third-party code | DirectX Tool Kit (NuGet `directxtk_desktop_win10` 2026.5.8.1), restored into `engine/packages/` | `engine/src/DOORWAYS/packages.config` |
| Build | MSBuild solution `CaucasianEagle.slnx`; PowerShell scripts | `tools/build.ps1`, `tools/stage.ps1` |
| Art tools | Blender 5.2 (Python scripts), Meshy (generated meshes, rig and animation clips) | `art/` |
| Asset compiler | `DoorwaysAssetBuilder` (engine tool): glTF to the engine's compiled formats | `engine/Tools/DoorwaysAssetBuilder/` |
| Platform | Windows 10/11, x64 only | `CaucasianEagle.slnx` |

## 3. Repository structure

```
Caucasian-Eagle/
├── CaucasianEagle.slnx        solution: engine library + game executable
├── README.md
├── engine/                    git submodule: the Doorways engine (kalk1t/Doorways-D3D11)
├── game/
│   ├── CaucasianEagle.vcxproj game executable project
│   ├── src/                   game code (see 04-components.md)
│   └── assets/Compiled/       ready-to-run game assets (see 05-data.md)
├── art/                       scripts that build the assets; heavy source files are not in git
│   ├── nyxara/                hero: Meshy requests, Blender assembly, effect textures
│   └── moonlit-sanctuary/     map: Blender scene, build and export scripts
├── tools/
│   ├── build.ps1              restore, build, stage (one command)
│   └── stage.ps1              assemble the runtime folder
├── story/STORY.md             story bible
├── techplan/                  this documentation
├── milestones/                road to v1.0
├── docs/                      manual, changelog, glossary
└── build/                     generated, not in git: obj/, stage/CaucasianEagle/ (the runnable game)
```

The rule that keeps the split clean: **nothing under `engine/` is edited from this repository.** Engine changes are made in the engine repository, committed there, and brought in by moving the submodule pointer ([06-engine-interface.md](06-engine-interface.md), [03-workflows.md](03-workflows.md)).
