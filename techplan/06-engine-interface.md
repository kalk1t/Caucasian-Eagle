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
| `SceneId` | `"MoonlitSanctuary"` | `Configure` | Scene selection |
| `WindowTitle` | `"Caucasian Eagle"` | `Configure` | Window creation and title updates |
| `UserDataFolder` | `"CaucasianEagle"` | `Configure` | Settings and save paths |
| `Active` | `true` once the kit initialises | `NyxaraKit::Initialize` | Gates every option below |
| `Unattended` | `true` only in the showcase | `Prepare` | Input ownership without window focus |
| `ScriptedMove`, `ScriptedFast` | Showcase travel keys | `NyxaraKit::ReadKeys` | Input map |
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
| `host.World` (`WorldState`) | Reads the player and camera; writes the hero's visual offset and rotation; replaces the hero character asset |
| `host.WorldRender` (`WorldRenderer`) | Loads the hero's materials; shows and hides sword materials |
| `host.Render` (`Renderer`) | Creates effect meshes and textures; draws effects |
| `host.Combat` (`CombatSystem`) | Checks that the player may act; applies enemy damage to the player |
| `host.StartGame()` | Starts play from the menu in the showcase |

The game also includes engine headers directly (`WorldState.h`, `Renderer.h`, `WorldRenderer.h`, `CombatSystem.h`, `DoorwaysAssetPaths.h`, `DoorwaysRenderContexts.h`, `HudTextRenderer.h`, the asset loaders). These are not a stable interface: an engine update can break the game build. The plan to narrow this is D2 in [10-tech-debt-risks.md](10-tech-debt-risks.md).

## 9.5 Engine version in use

The submodule pointer is the single source of truth: `git submodule status` prints the engine commit. Each engine bump is its own commit in this repository and is noted in [`docs/changelog.md`](../docs/changelog.md).

## 9.6 Rules

1. No file under `engine/` is modified from this repository.
2. A need the engine does not cover becomes a general option or callback in `DoorwaysGame.h`, made in the engine repository.
3. The engine must keep building and running without any game (its sandbox executable is the check).
4. Game names (Nyxara, Caucasian Eagle, Moonlit Sanctuary) do not appear in engine source.
