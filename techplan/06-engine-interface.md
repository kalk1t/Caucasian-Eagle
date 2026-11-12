# 9. Engine interface

[← Index](TECHNICAL_DOCUMENTATION.md)

The whole contract between the game and the engine is one engine header, `engine/src/DOORWAYS/DoorwaysGame.h`, plus the entry point in `DoorwaysApplication.h`. The engine side is documented in the engine repository (`engine/techplan/06-game-interface.md`). This page records how Caucasian Eagle uses it.

## 9.1 Entry point

| Call | From | Purpose |
|---|---|---|
| `Doorways::RegisterDoorwaysGame(IDoorwaysGame*)` | `main.cpp` | Registers the game; calls `Configure` at once |
| `Doorways::RunDoorwaysApplication(HINSTANCE, PSTR)` | `main.cpp` | Runs the engine; returns the exit code |

## 9.2 Callbacks the game implements (`IDoorwaysGame`)

| Callback | Implemented in | Used |
|---|---|---|
| `Configure(DoorwaysGameSession&)` | `Game::Configure` | yes |
| `Prepare(DoorwaysGameHost&, std::string& error)` | `Game::Prepare` | yes |
| `AdjustEnvironment(EnvironmentSettings&)` | `Game::AdjustEnvironment` | yes (moonlight, haze; switches the engine sky off) |
| `ScaleFrameTime(float, bool)` | `Game::ScaleFrameTime` | yes (hit-stop) |
| `Update(DoorwaysGameHost&, float, float, bool)` | `Game::Update` | yes |
| `Draw(Renderer&, const WorldState&, const XMMATRIX&)` | `Game::Draw` | yes |
| `DrawHud(const DoorwaysHudRenderContext&, HudTextRenderer&)` | `Game::DrawHud` | yes |

## 9.3 Session options the game sets (`DoorwaysGameSession`)

| Option | Value in Caucasian Eagle | Set in | Read by (engine) |
|---|---|---|---|
| `SceneId` | `"RoadToAmirani"` | `Configure` | Scene selection |
| `SceneCollision` | `true` | `Configure` | Gameplay settings: the scene's walkable meshes and blockers are used, and the player cannot step off the walkable ground |
| `WindowTitle` | `"Caucasian Eagle"` | `Configure` | Window creation and title updates |
| `UserDataFolder` | `"CaucasianEagle"` | `Configure` | Settings and save paths |
| `Active` | `true` once the kit initialises | `NyxaraKit::Initialize` | Gates every option below |
| `Unattended` | `true` only in the showcase and the test pilot | `Prepare` | Input ownership without window focus |
| `ScriptedMove`, `ScriptedFast` | Showcase and test-pilot travel keys | `NyxaraKit::ReadKeys`, `Journey::UpdatePilot` | Input map |
| `SuppressedActions` | Interact (E), CycleDebugRenderMode (Q), ToggleTextureFilter (F), ToggleImportedAnimatedDebugSampling (V), ToggleImportedSkinningSideBySideComparison (C), BasicAttack (LMB), HeavyAttack (RMB) | `Prepare` | Input map: those engine actions never fire, so the kit owns the keys |
| `SlowWalk`, `SlowWalkSpeed` | `true`, 1.38 m/s | `Prepare` | Movement |
| `FastMultiplier` | 2.2 | `Prepare` | Movement (Shift) |
| `HoldFacing` | `true` during an action | `NyxaraKit::Update` | Movement: travel does not turn her |
| `ImpulseX`, `ImpulseZ` | Dash and dive velocity | `NyxaraKit::Update` | Movement: added before collision resolution |
| `OrbitCamera` and `Camera*` | Orbit rig, see [11-constants.md](11-constants.md) | `Prepare` | Camera |
| `CameraShake` | Decaying amplitude | `NyxaraKit::Update` | Camera |

## 9.4 Engine objects the game drives (`DoorwaysGameHost`)

| Object | What the game does with it |
|---|---|
| `host.World` (`WorldState`) | Reads the player and camera; writes the hero's visual offset and rotation; replaces the hero character asset; the journey moves the player back onto the road, to checkpoints, and (test pilot only) turns the camera |
| `host.WorldRender` (`WorldRenderer`) | Loads the hero's materials; shows and hides sword materials |
| `host.Render` (`Renderer`) | Creates effect meshes and textures; draws effects |
| `Renderer::LoadUiTexture`, `Renderer::DrawUiSprite` | Load the HUD atlas and draw parts of it in `DrawHud` (used only by `GameUi.cpp`) |
| `host.Combat` (`CombatSystem`) | Checks that the player may act; applies enemy damage to the player (`ApplyIncomingDamage`); heals the player (`RestorePlayerHealth`, added in engine `aec7057`) |
| `host.StartGame()` | Starts play from the menu in the showcase and the test pilot |

The game also includes engine headers directly (`WorldState.h`, `Renderer.h`, `WorldRenderer.h`, `CombatSystem.h`, `DoorwaysAssetPaths.h`, `DoorwaysRenderContexts.h`, `HudTextRenderer.h`, the asset loaders). These are not a stable interface: an engine update can break the game build. The plan to narrow this is D2 in [10-tech-debt-risks.md](10-tech-debt-risks.md).

## 9.5 Engine version in use

The submodule pointer is the single source of truth: `git submodule status` prints the engine commit. Each engine bump is its own commit in this repository and is noted in [`docs/changelog.md`](../docs/changelog.md).

## 9.5a Game controller

The start screen is the game's own: `DoorwaysGameSession::OwnStartScreen` with `IDoorwaysGame::DrawStartScreen` and `StartRequest` (engine `techplan/06-game-interface.md`). `MenuTitle`, `MenuSubtitle`, `MenuPlayLabel`, `MenuCredits`, `MenuHideLinks`, `MenuBackdrop` and `MenuPanelSide` are still set for the engine's menu, which shows only if `OwnStartScreen` is turned off. The engine's settings screens are used as they are. The engine's `ToggleInventory` action is suppressed; I opens the game's own window.
The settings window is the game's own too: `OwnSettings` with `GameSession().Settings` (`Active`, `Resolutions`, `BorderlessSize`, `Confirming`, `SecondsRemaining`; `Ask`, `DisplayAnswer`). Also used: `Cursor` (the pointer file), `FreeCursor` (pointer free while her windows are open), `LookScale` (camera turning speed, `CameraLookScale`), `OwnSpawn` / `SpawnPosition` / `SpawnYaw` (she begins on the road).

The engine reads XInput controllers (`DoorwaysGamepad.h`). A button is bound to a virtual key and then counts as that key. The game keeps the engine's defaults (left stick = movement keys, right stick = camera, A jump, B dodge (suppressed by the game, so B does nothing in play), X / Y mouse buttons, LT fast, left stick click slow, Back inventory, Start Escape) and binds its own keys in `Game::Prepare` with `BindDoorwaysGamepadButton`: RB `F`, RT `V`, D-pad up `C`, LB `E`, D-pad left `Q`, D-pad right `M`, D-pad down `F1`. The game reads its own keys with `IsDoorwaysKeyDown` (kit, journal, map) so they answer to both devices. `Ui::KeyLabel` shows the button name instead of the key name while `IsDoorwaysGamepadInUse()` is true. Hold-F9 restart reads the keyboard only.

## 9.5b Sound files

`Doorways::GameAudio()` (`DoorwaysGameAudio.h`) plays the game's WAV files: `Play(file, volume, pitch, channel)` for one-shots, `Loop(slot, file, volume, channel)` and `Say(slot, file, volume, channel)` for six slots that hold one file each, `Stop(slot)`, `Playing(slot)`. Paths are relative to the content root (`assets/sounds/...`). Only `Sound.cpp` calls it.

## 9.6 Rules

1. No file under `engine/` is modified from this repository.
2. A need the engine does not cover becomes a general option or callback in `DoorwaysGame.h`, made in the engine repository.
3. The engine must keep building and running without any game (its sandbox executable is the check).
4. Game names (Nyxara, Caucasian Eagle, Moonlit Sanctuary) do not appear in engine source.
