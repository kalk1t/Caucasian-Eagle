# Caucasian Eagle

Caucasian Eagle is created by **Giorgi Samkharadze**. It is built in **Doorways**, a custom game engine founded by Giorgi Samkharadze in February 2026 (C++ on Direct3D 11); it is not made in a bought or borrowed engine. The same is said on the start screen, on its **Credits** page and on the last page of the guide.

A single-player action game for Windows, built on the Doorways engine.

Amirani fought against God and is chained in a cave on the Caucasus Range, where an eagle feeds on his liver. Once every seven years the cave opens. On that day **Nyxara**, the eagle's daughter, sees him, and promises to spend seven years becoming strong enough to fight her father and break the chain. The game is her road to the mountain. Full story: [`story/STORY.md`](story/STORY.md).

**Version 0.5.2** is **the Eagle's Ascent**: the climb from the sea to Amiran's court at 4,200 m, on foot and on her wings, past the creatures of the mountain and four bosses (the Gveleshapi, the Giant, the Devi Lord and the Eagle). Nyxara fights with two swords (a string of moves on each mouse button, also in a jump; Space jumps out of any of them), slides with **F**, and with **E** wraps her wings round her for 3 seconds while she goes on fighting; she finishes every creature, the bosses too, with one of four finishing kills. In the Eagle Nests she buys nine levels each of her wings, swords and armour with crystals, and every level brings a look she can wear anywhere; **I** shows her, turned round by the mouse. Her wings carry her 10 seconds at first and 30 at level 9; Space held 1 second takes her up. Crystals come from the creatures and from clay jars beside every road (Georgia's old pottery: break them to see what is inside); potions, found in the jars and drunk with **Z**, are her only healing. Every tree of the mountain, 299,664 of them, is drawn as a 3D model with its branches, leaves and roots, and every one stops her; logs, ferns and flowers lie on the ground. **M** shows the whole mountain in 3D, to go over with the mouse (**Tab**: the detailed map). **Esc** opens the menu (with the settings); **H** in any Eagle Nest saves; when she falls she wakes at the nest she saved at, and everything since that save is undone. The road to Amirani of 0.4.0 (twelve regions, seven cities, ten creatures of Caucasian folklore, the Eagle and the chain) is still in the game. It has been tested by unattended runs and only partly played by hand. Download `CaucasianEagle-0.5.2-Setup.exe` from the [releases page](https://github.com/kalk1t/Caucasian-Eagle/releases), run it (Windows may warn that it does not know the file: it is not signed yet; see [why Windows warns](docs/manual.md#why-windows-warns)), and start **Caucasian Eagle** from the Start menu: since 0.5 `CaucasianEagle.exe` starts the Eagle's Ascent (the road of 0.4 is **Caucasian Eagle - The Road (0.4)**, or `CaucasianEagle.exe --road`). What changed: [`docs/changelog.md`](docs/changelog.md). The road to v1.0 is in [`milestones/`](milestones/README.md).

## System requirements

| | Lowest (estimated, not tested) | Better |
|---|---|---|
| System | Windows 10, 64-bit | Windows 11, 64-bit |
| Processor | 64-bit, 4 cores | 8 cores or more |
| Graphics | A graphics card with DirectX 11 and 2 GB of its own memory | 4 GB |
| Memory | 8 GB | 16 GB |
| Disk | 2 GB free | the same, on an SSD |
| Screen | 1280 x 720 | 1920 x 1080 |
| Controls | Keyboard and mouse | or an Xbox-style (XInput) controller |
| Sound | Any sound device | |
| Internet | Not needed | |

The game has run on one computer only: Windows 11, Ryzen 9 9950X3D, 64 GB of memory, GeForce RTX 5080, where it holds 60 frames a second. The "lowest" column is an estimate from what version 0.5.2 was measured to use there (2026-11-21): 1.44 GB on disk once installed; up to about 2 GB of memory (1.3 to 1.4 GB in use at once); up to 1.3 GB of the graphics card's own memory (0.4 GB on the climb, 0.8 GB with the whole mountain in view, 1.3 GB on the road of 0.4); and while it loads it keeps many processor cores busy. No weaker computer has been tried.

## Guide

How to play is in three places, and they say the same:

- **In the game:** **Guide** on the start screen, and **Guide** in her menu (Esc). Seven pages: the journey, moving and keys, fighting, health, growing, what the game needs, who made it.
- **The [player manual](docs/manual.md):** everything, at length.
- **Below:** the keys.

In short: follow the old road from its first gate to the mountain. On the way are nine traps (the line under the compass says what each one asks) and 26 named places to find. In most clearings ringed with standing stones you are ambushed and walls hold you in until each wave is beaten. Creatures leave crystals; spend them in Nyxara's window (**I**) on her wings, swords and armour. Seven cities on the road heal you once each and give a lasting gift. Three guardians and, at the end, the Eagle bar the way. The game saves by itself.

## Build and run

Needs Windows 10 or 11 (x64), Visual Studio 2026 with "Desktop development with C++", and Git.

```powershell
git clone --recurse-submodules https://github.com/kalk1t/Caucasian-Eagle.git
cd Caucasian-Eagle
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Run
```

The first build compiles the whole engine and takes several minutes. The runnable game ends up in `build\stage\CaucasianEagle\`.

**The Eagle's Ascent**, the mountain climb (Map A, the 0.5 game): what `CaucasianEagle.exe` starts since 0.5 (`Play-EaglesAscent.cmd` in the repository; `--eagles-ascent` still asks for it). The road of 0.4 above: `CaucasianEagle.exe --road` (`Play-TheRoad.cmd`). See the manual.

### On the Eagle's Ascent

| Input | Action |
|---|---|
| W A S D | Run; A and D turn her |
| Space | Jump (also out of any move of her combo); held 1 s, take wing (10 s of flight, up to 30 s at wing level 9) |
| R (flying) | Land |
| Left click / right click | Her combo: each button goes on through its own string of moves (also in a jump) |
| F | The slide; flying, the twisted dash |
| E | Wing shield: 3 s, blows from every side stopped while she goes on fighting; then 7 s to rest |
| Z | Drink a potion (the only healing) |
| I | Her window: nine levels each of wings, swords and armour (bought in an Eagle Nest), her looks (anywhere); drag turns round her |
| H | Save, standing in an Eagle Nest |
| M | The whole mountain in 3D, gone over with the mouse (drag, right drag, wheel, WASD, Space to her); Tab: the detailed map |
| Backspace | Back to the last Eagle Nest she stood in |
| Esc | Pause |

## Controls

| Input | Action |
|---|---|
| W A S D | Walk; in the air W flies where the camera looks |
| Shift | Wing boost (faster) |
| Ctrl | Walk slowly |
| Space | Jump on foot; held in flight, climb (X sinks) |
| R | Take wing (3 s) / land when low (3 s) |
| Mouse | Look |
| Left click / right click | Left sword / right sword; clicks in a row go through the cuts (a click in a jump: the leaping cut) |
| F | The slide; flying, the twisted dash |
| V / C | Nothing since 0.5.2 (the thrown swords and the feather storm are kept in the Character Lab) |
| A / D, S | Turn her way (and the view); step back |
| E | Wing shield: 3 s, blows from every side stopped while she fights on; 7 s to rest |
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
