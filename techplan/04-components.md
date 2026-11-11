# 7. Components and functions

[← Index](TECHNICAL_DOCUMENTATION.md)

All game code is in `game/src/` (about 1,800 lines in v0.1). Namespaces: `CaucasianEagle` for the game module and constants; `Doorways` for the kit and HUD, which were moved out of the engine and keep its namespace for now ([10-tech-debt-risks.md](10-tech-debt-risks.md), D3).

## 7.1 Files

| File | Role |
|---|---|
| `main.cpp` | `WinMain`: registers the game, runs the engine |
| `CaucasianEagleGame.h/.cpp` | `CaucasianEagle::Game`, the `IDoorwaysGame` implementation: configure, load the hero, set engine options, forward the frame callbacks |
| `GameConstants.h` | Names and tuning values shared across the game |
| `NyxaraKit.h/.cpp` | The combat kit: input, action state machine, hits and damage, shade enemies, projectiles, effects, the scripted showcase |
| `NyxaraHud.h/.cpp` | HUD snapshot struct (`NyxaraHudState`) and the function that draws it |

## 7.2 `CaucasianEagle::Game`

| Function | When the engine calls it | What it does |
|---|---|---|
| `Configure` | At registration, before the engine starts | Sets scene id, window title, user-data folder |
| `Prepare` | Once, after engine initialisation, normal launches only | Loads the `EagleV1` bundle with its 31 clips; sets movement, camera and input options; starts `NyxaraKit`; starts the showcase if requested |
| `ScaleFrameTime` | Every frame, before simulation | Forwards to the kit (hit-stop) |
| `Update` | Every frame while gameplay runs | Forwards to the kit |
| `Draw` | Every frame, after the world is drawn | Forwards to the kit |
| `DrawHud` | Every frame, in the HUD pass | Draws the game HUD when a kit snapshot is valid and the engine's melee session is active; otherwise lets the engine draw its own |

## 7.3 `NyxaraKit`

One object, reached through `Doorways::GetNyxaraKit()`.

### Actions

An action is one row of the `Acts` table: display name, animation clip, playback rate, end time, and up to four hit times (in clip seconds). The enum `NyxaraKit::Act` indexes it.

| Act | Input | Name shown | Clip |
|---|---|---|---|
| `Slash1` | LMB (first, then every other click) | LEFT TALON | `BasicAttack` (mirrored to the left hand) |
| `Slash2` | LMB (second, then every other click) | RIGHT TALON | `SlashLeft` |
| `Heavy` | RMB | CHARGED SLASH | `HeavyAttack` |
| `Skill1` | F | TALON DASH | `Skill1` |
| `Skill2` | V | FEATHER CYCLONE | `Skill2` |
| `Skill3` | C | MOON LEAP | `Skill3` |
| `Combo1L` / `Combo1R` | after F: LMB / RMB | TWIN TALONS / RISING EAGLE | `Combo1L` / `Combo1R` |
| `Combo2L` / `Combo2R` | after V: LMB / RMB | TEMPEST TRIPLE / WHIRLWIND TALONS | `Combo2L` / `Combo2R` |
| `Combo3L` / `Combo3R` | after C: LMB / RMB | SOUL WEAVE / MOONFALL DIVE | `Combo3L` / `Combo3R` |
| `Guard` | E (hold) | SWORD BARRIER | `Guard` |
| `Swap` | Q | SWORD EXCHANGE | `SwordSwap` |

### Main functions

| Function | Purpose |
|---|---|
| `Initialize` | Builds effect meshes in code, loads six effect textures from `assets/Compiled/Effects/Nyxara`, places the shades, sets `GameSession().Active` |
| `ReadKeys` | Reads the mouse buttons and Q, E, F, V, C, Shift; in showcase mode returns scripted keys instead |
| `ScaleFrameTime` | Returns 6% of real time while a hit-stop is running |
| `Update` | Per frame: glide lift and lean, cooldowns, input to action (including skill cancel and the 0.45 s press buffer), hit events, dash and dive movement, enemies, projectiles, effects, HUD snapshot |
| `StartAction` | Begins an action: aims at the camera direction or a nearby shade, starts step-in dashes |
| `OnHit` | Runs when an action's hit time is reached: damage shape, effects, pulls, launches |
| `Impact` | Hit feedback: hit-stop, camera shake, sparks, cross-slash, ring |
| `Dash` | Sets a timed planar impulse that the engine's movement resolves against collision |
| `AirProfile` | Visual height curve for the leap skills |
| `PublishHud` | Fills `NyxaraHud()` for the HUD |
| `Draw` | Draws shades, projectiles, barrier swords and effects, sorted back to front; projects enemy positions for the HUD bars |

### How the kit drives the engine

| Engine hook | Used for |
|---|---|
| `ImportedAnimatedCharacter.SetPresentationClipOverride` | Plays the action clip on the hero |
| `ImportedAnimatedCharacter.SetGroundTravelOverride("FlightHover", rate)` | Presents ground travel as upright wing-beat flight |
| `AnimatedPlayerVisual.Translation / Rotation` | Visual lift, leap height, lean and bank |
| `GameSession().ImpulseX/Z` | Dashes and dives, resolved by engine collision |
| `GameSession().HoldFacing` | Keeps her facing the target while moving during an action |
| `GameSession().CameraShake` | Impact shake |
| `WorldRenderer::SetCharacterMaterialVisible` | Shows the right sword model in each hand after a swap |
| `CombatSystem::ApplyIncomingDamage` | Shade orbs damaging the player |

## 7.4 `NyxaraHud`

`NyxaraHudState` is a plain snapshot: skill cooldowns, combo availability and names, current action flags, the banner text and timer, and up to eight projected enemy bars. `DrawNyxaraHud` reads it plus the engine's vitals text (`"HP %d EN %d"`) and draws everything with the engine's rectangle and text primitives. It holds no gameplay state.
