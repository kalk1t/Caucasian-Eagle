# Caucasian Eagle

A single-player action game for Windows, built on the Doorways engine.

Amirani fought against God and is chained in a cave on the Caucasus Range, where an eagle feeds on his liver. Once every seven years the cave opens. On that day **Nyxara**, the eagle's daughter, sees him, and promises to spend seven years becoming strong enough to fight her father and break the chain. The game is her road to the mountain. Full story: [`story/STORY.md`](story/STORY.md).

**Version 0.2.0** is the whole road in a first form: from Moonlit Sanctuary through twelve regions of the Caucasus, against ten creatures of its folklore and three bosses, to the fight with the Eagle and the breaking of the chain. It has been tested by an unattended test pilot, not yet played by hand. There is no sound and no installer yet. What changed: [`docs/changelog.md`](docs/changelog.md). The road to v1.0 is in [`milestones/`](milestones/README.md).

## Build and run

Needs Windows 10 or 11 (x64), Visual Studio 2026 with "Desktop development with C++", and Git.

```powershell
git clone --recurse-submodules https://github.com/kalk1t/Caucasian-Eagle.git
cd Caucasian-Eagle
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Run
```

The first build compiles the whole engine and takes several minutes. The runnable game ends up in `build\stage\CaucasianEagle\`.

## Controls

| Input | Action |
|---|---|
| W A S D | Fly (upright, low over the ground) |
| Shift | Wing boost (faster) |
| Ctrl | Walk |
| Space / hold Space | Jump / rise |
| Mouse | Look |
| Left click | Sword slash: left sword, then right sword, alternating |
| Right click | Charged slash |
| F / V / C | Talon Dash / Feather Cyclone / Moon Leap |
| F, V or C, then left or right click | That skill's two combos |
| Q | Swap the swords between hands |
| E (hold) | Sword barrier |
| F9 (hold 2 s) | Begin the journey again |

More in the [player manual](docs/manual.md).

## Repository map

| Folder | What is in it |
|---|---|
| `engine/` | The Doorways engine (git submodule, [kalk1t/Doorways-D3D11](https://github.com/kalk1t/Doorways-D3D11)). Never edited from here. |
| `game/` | The game: source code and ready-to-run assets |
| `art/` | Scripts that build the assets; heavy source art stays off git ([`art/README.md`](art/README.md)) |
| `tools/` | Build, stage and package scripts; installer script |
| `story/` | Story bible |
| `techplan/` | Technical documentation: start at [`TECHNICAL_DOCUMENTATION.md`](techplan/TECHNICAL_DOCUMENTATION.md) |
| `milestones/` | Road from v0.1 to the installable v1.0 |
| `docs/` | Player manual, changelog, glossary |

## Engine and game

The engine and the game are separate projects. The engine knows nothing about this game; the game plugs in through one engine header, `DoorwaysGame.h`. How that works and the rules that keep it clean are in [`techplan/06-engine-interface.md`](techplan/06-engine-interface.md).
