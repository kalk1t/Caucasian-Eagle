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
| `AmiraniMountain.h/.cpp` | Scenery: the night sky dome and moon, the mountain, chained Amirani, the eagle, the dog, the chains; also the moonlight and haze settings |

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

## 7.5 `AmiraniMountain`

Scenery only. It has no collision and no gameplay state except a clock for the animation.

| Function | What it does |
|---|---|
| `Initialize` | Builds the sky dome, moon sphere and halo quad; loads `Mountain`, `Amirani`, `Eagle`, `Dog` (`.dmesh` plus `<Name>BaseColor.png`) from `LandmarkFolder`. Finds the flat ledge in the mountain mesh and turns it toward the play area, because the importer may mirror Z. |
| `Update` | Advances the animation clock with real time, so the scene keeps moving during hit-stop. |
| `Draw` | Sky dome and moon first (centred on the camera, so they never come closer), then the mountain, Amirani (slow breathing), the eagle (wing-beat bob and a lunge at the wound every `EaglePeckPeriod`), the dog (small head motion), and four chains made of box links. |
| `StyleEnvironment` | Called from `Game::AdjustEnvironment` every frame: switches the engine's own sky panels and moon off, and sets moonlight direction and colour, ambient colour and haze. |

**Why the game draws the sky itself.** The engine lights its sky panels like scenery (ambient, directional, image-based light), which washes a painted sky out and lights each panel differently. The dome here gives every vertex the same normal, pointing along the moonlight, so the directional light adds nothing and the ambient term is equal everywhere. The moon uses the same trick and therefore always shows as a full moon.

**Capture mode.** `CAUCASIAN_EAGLE_VISTA=1` together with `CAUCASIAN_EAGLE_DEMO=1` holds the camera on the mountain and hides the combat effects and HUD.

