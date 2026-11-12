# Caucasian Eagle

Caucasian Eagle is created by **Giorgi Samkharadze**. It is built in **Doorways**, a custom game engine founded by Giorgi Samkharadze in February 2026 (C++ on Direct3D 11); it is not made in a bought or borrowed engine. The same is said on the start screen, on its **Credits** page and on the last page of the guide.

A single-player action game for Windows, built on the Doorways engine.

Amirani fought against God and is chained in a cave on the Caucasus Range, where an eagle feeds on his liver. Once every seven years the cave opens. On that day **Nyxara**, the eagle's daughter, sees him, and promises to spend seven years becoming strong enough to fight her father and break the chain. The game is her road to the mountain. Full story: [`story/STORY.md`](story/STORY.md).

**Version 0.3.0** is the living road: the whole journey through twelve regions and seven cities of the Caucasus, against ten creatures of its folklore and three bosses, to the fight with the Eagle and the breaking of the chain, with the game's own start screen, menu, settings, maps, story journal, sound and mouse pointer. It has been tested by an unattended test pilot and only partly played by hand. It is the first version with an installer: download `CaucasianEagle-0.3.0-Setup.exe` from the [releases page](https://github.com/kalk1t/Caucasian-Eagle/releases), run it (Windows will warn that the file is not signed: choose "More info", then "Run anyway"), and start Caucasian Eagle from the Start menu. A zip that runs without installing is on the same page. What changed: [`docs/changelog.md`](docs/changelog.md). The road to v1.0 is in [`milestones/`](milestones/README.md).

## System requirements

| | Lowest (estimated, not tested) | Better |
|---|---|---|
| System | Windows 10, 64-bit | Windows 11, 64-bit |
| Graphics | A graphics card with DirectX 11 and 2 GB of its own memory | 4 GB |
| Memory | 4 GB | 8 GB |
| Disk | 1.5 GB free | the same |
| Screen | 1280 x 720 | 1920 x 1080 |
| Controls | Keyboard and mouse | or an Xbox-style (XInput) controller |
| Sound | Any sound device | |
| Internet | Not needed | |

The game has run on one computer only: Windows 11, Ryzen 9 9950X3D, 64 GB of memory, GeForce RTX 5080, where it holds 60 frames a second. The "lowest" column is an estimate from what the game uses there (about 1 GB on disk, under 1 GB of memory); no weaker computer has been tried.

## Guide

How to play is in three places, and they say the same:

- **In the game:** **Guide** on the start screen, and **Guide** in her menu (Esc). Seven pages: the journey, moving and keys, fighting, health, growing, what the game needs, who made it.
- **The [player manual](docs/manual.md):** everything, at length.
- **Below:** the keys.

In short: follow the old road from its first gate to the mountain. In every clearing ringed with standing stones you are ambushed and walls hold you in until each wave is beaten. Creatures leave crystals; spend them in Nyxara's window (**I**) on her wings, swords and armour. Seven cities on the road heal you once each and give a lasting gift. Three guardians and, at the end, the Eagle bar the way. The game saves by itself.

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
| Z | Drink a potion |
| I | Nyxara's window: grow, and (D) what she carries |
| F1 / M | Story journal / world map |
| Esc | Her menu: continue, settings, guide, start screen, leave |
| F9 (hold 2 s) | Begin the journey again |

More in the [player manual](docs/manual.md).

## Repository map

| Folder | What is in it |
|---|---|
| `engine/` | The Doorways engine (git submodule, [kalk1t/Doorways-D3D11](https://github.com/kalk1t/Doorways-D3D11)). Never edited from here. |
| `game/` | The game: source code and ready-to-run assets |
| `art/` | Scripts that build the assets; heavy source art stays off git ([`art/README.md`](art/README.md)) |
| `tools/` | Build, stage and package scripts; installer script (`tools\package.ps1` builds the zip and the Setup file into `dist\`) |
| `story/` | Story bible |
| `techplan/` | Technical documentation: start at [`TECHNICAL_DOCUMENTATION.md`](techplan/TECHNICAL_DOCUMENTATION.md) |
| `milestones/` | Road from v0.1 to the installable v1.0 |
| `docs/` | Player manual, changelog, glossary |

## Engine and game

The engine and the game are separate projects. The engine knows nothing about this game; the game plugs in through one engine header, `DoorwaysGame.h`. How that works and the rules that keep it clean are in [`techplan/06-engine-interface.md`](techplan/06-engine-interface.md).
