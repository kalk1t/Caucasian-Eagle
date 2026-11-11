# 5. Startup and configuration

[← Index](TECHNICAL_DOCUMENTATION.md)

## 5.1 Startup sequence

[confirmed: `game/src/main.cpp`, `game/src/CaucasianEagleGame.cpp`, `engine/src/DOORWAYS/DoorwaysApplication.cpp`, `AppGame.cpp`]

1. `WinMain` (`game/src/main.cpp`) creates the one `CaucasianEagle::Game` object and calls `Doorways::RegisterDoorwaysGame(&game)`.
2. Registration immediately calls `Game::Configure`, which sets three options that must be known before the engine starts:
   - `SceneId = "MoonlitSanctuary"`: the scene the engine loads
   - `WindowTitle = "Caucasian Eagle"`
   - `UserDataFolder = "CaucasianEagle"`: settings and saves go to `%LOCALAPPDATA%\CaucasianEagle`
3. `Doorways::RunDoorwaysApplication` parses the command line, initialises COM, creates the engine `App` and initialises it (window, Direct3D device, shaders, scene, physics, audio, UI).
4. On a normal launch (no validation mode on the command line) the engine calls `Game::Prepare`. It:
   - loads Nyxara's character bundle and replaces the engine's default player visual
   - sets the session options: Ctrl walk speed, Shift boost multiplier, the orbit camera numbers, and the list of engine actions whose keys the game reads itself
   - initialises `NyxaraKit` (effect meshes and textures, shade enemies)
   - if the environment variable `CAUCASIAN_EAGLE_DEMO` is `1`, starts the scripted showcase
5. The engine shows its main menu. **Play** starts the session in Moonlit Sanctuary.

If `Prepare` fails, the engine shows the error in a message box and exits with code 1.

## 5.2 Configuration surfaces

| What | Where | How it changes | Notes |
|---|---|---|---|
| Game identity, start scene, hero bundle, camera, movement tuning | `game/src/GameConstants.h` | Edit and rebuild | Listed in [11-constants.md](11-constants.md) |
| Skill timings, damage, cooldowns, enemy stats | `game/src/NyxaraKit.cpp`, `NyxaraKit.h` | Edit and rebuild | Listed in [11-constants.md](11-constants.md) |
| Key bindings for skills (F, V, C), swap (Q), barrier (E), mouse buttons | `NyxaraKit::ReadKeys` in `game/src/NyxaraKit.cpp` | Edit and rebuild | Read directly with `GetAsyncKeyState`; not yet rebindable by the player |
| Movement keys (WASD, Space, Ctrl, Shift) | Engine input map and the engine Controls settings screen | Player setting | Engine-owned |
| Graphics, audio, interface settings | Engine settings screens | Player setting, stored in `%LOCALAPPDATA%\CaucasianEagle\PlayerPreferences.ini` | Engine-owned [confirmed: `engine/src/DOORWAYS/DoorwaysSettingsPaths.cpp`] |
| Character save | `%LOCALAPPDATA%\CaucasianEagle\CharacterSnapshot.txt` | Written by the engine | Engine progression system; the game adds nothing to it in v0.1 |

There is no game configuration file yet. Moving tuning values out of code into data is milestone M4 ([milestones](../milestones/README.md)).

## 5.3 Command line and environment

| Input | Effect | Owner |
|---|---|---|
| (none) | Normal launch: main menu, then play | Game |
| `CAUCASIAN_EAGLE_DEMO=1` (environment) | Scripted showcase: starts play without the menu and drives the kit from a fixed 30-second timeline; does not need window focus. Used for automated captures. | Game [confirmed: `NyxaraKit::ReadKeys`] |
| Engine validation modes (`--ui-preview`, `--interface-validation`, and others) | Engine self-tests. The game is not prepared in these modes. Access to some modes depends on the engine's access build. | Engine [confirmed: `DoorwaysApplication.cpp`] |

## 5.4 Files written at run time

| Path | Content | Written by |
|---|---|---|
| `%LOCALAPPDATA%\CaucasianEagle\PlayerPreferences.ini` | Player settings | Engine |
| `%LOCALAPPDATA%\CaucasianEagle\DeveloperSettings.ini` | Developer-only settings | Engine |
| `%LOCALAPPDATA%\CaucasianEagle\CharacterSnapshot.txt`, `.lock` | Character progression save and its single-instance lock | Engine |

Nothing is written into the install folder, so the game can be installed to a read-only location [inferred from the paths above; an installed copy has not been tested].
