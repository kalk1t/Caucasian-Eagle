# 7. Components and functions

[← Index](TECHNICAL_DOCUMENTATION.md)

All game code is in `game/src/` (about 3,400 lines). Namespaces: `CaucasianEagle` for the game module and constants; `Doorways` for the kit and HUD, which were moved out of the engine and keep its namespace for now ([10-tech-debt-risks.md](10-tech-debt-risks.md), D3).

## 7.1 Files

| File | Role |
|---|---|
| `main.cpp` | `WinMain`: registers the game, runs the engine |
| `CaucasianEagleGame.h/.cpp` | `CaucasianEagle::Game`, the `IDoorwaysGame` implementation: configure, load the hero, set engine options, forward the frame callbacks |
| `GameConstants.h` | Names and tuning values shared across the game |
| `NyxaraKit.h/.cpp` | The combat kit: input, action state machine, hits and damage, shade enemies, projectiles, effects, the scripted showcase |
| `NyxaraHud.h/.cpp` | HUD snapshot struct (`NyxaraHudState`) and the function that draws it |
| `AmiraniMountain.h/.cpp` | Scenery: the night sky dome and moon, the mountain, chained Amirani, the eagle, the dog, the chains; also the moonlight and haze settings |
| `Journey.h/.cpp` | The road to Amirani: the path, creatures and their behaviour, arenas and waves, bosses, chests, story cards, the ending, the save file, the test pilot |

## 7.2 `CaucasianEagle::Game`

| Function | When the engine calls it | What it does |
|---|---|---|
| `Configure` | At registration, before the engine starts | Sets scene id, window title, user-data folder |
| `Prepare` | Once, after engine initialisation, normal launches only | Loads the `EagleV1` bundle with its 31 clips; sets movement, camera and input options; starts `NyxaraKit`; starts the showcase if requested |
| `ScaleFrameTime` | Every frame, before simulation | Kit hit-stop factor times the journey's time scale (1 except in the test pilot) |
| `Update` | Every frame while gameplay runs | Kit, then journey |
| `Draw` | Every frame, after the world is drawn | Mountain, journey, then kit |
| `AdjustEnvironment` | Every frame | Mountain's moonlight, then the journey's per-region fog and light |
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

Scenery. Its collision (the walkable ledge) is part of the `RoadToAmirani` scene. Its only gameplay state is a clock for the animation and two flags set by the journey: `SetEagleAway` (the eagle has left the wound to fight) and `SetFreed` (no eagle, no chains).

| Function | What it does |
|---|---|
| `Initialize` | Builds the sky dome, moon sphere and halo quad; loads `Mountain`, `Amirani`, `Eagle`, `Dog` (`.dmesh` plus `<Name>BaseColor.png`) from `LandmarkFolder`. Finds the flat ledge in the mountain mesh and turns it toward the play area, because the importer may mirror Z. |
| `Update` | Advances the animation clock with real time, so the scene keeps moving during hit-stop. |
| `Draw` | Sky dome and moon first (centred on the camera, so they never come closer), then the mountain, Amirani (slow breathing), the eagle (wing-beat bob and a lunge at the wound every `EaglePeckPeriod`), the dog (small head motion), and four chains made of box links. |
| `StyleEnvironment` | Called from `Game::AdjustEnvironment` every frame: switches the engine's own sky panels and moon off, and sets moonlight direction and colour, ambient colour and haze. |

**Why the game draws the sky itself.** The engine lights its sky panels like scenery (ambient, directional, image-based light), which washes a painted sky out and lights each panel differently. The dome here gives every vertex the same normal, pointing along the moonlight, so the directional light adds nothing and the ambient term is equal everywhere. The moon uses the same trick and therefore always shows as a full moon.

**Capture mode.** `CAUCASIAN_EAGLE_VISTA=1` together with `CAUCASIAN_EAGLE_DEMO=1` holds the camera on the mountain and hides the combat effects and HUD.


## 7.6 `Journey`

One object, reached through `CaucasianEagle::GetJourney()`. It owns everything between the sanctuary gate and the ending.

### How it shares work with the kit

The kit keeps a list of up to `MaxEnemies` (30) enemy slots: position, health, radius, push scale. In "external enemies" mode (`SetExternalEnemies(true)`) the kit stops running and drawing its own shades, but still applies Nyxara's hits, pushes, pulls and launches to whatever is in the slots. The journey fills the slots (`Spawn`), moves them (`UpdateMonsters`), notices when the kit has set a slot's health to zero (`OnDeath`) and draws them. Chests and the two chain anchors are slots too, so every attack that can hit a creature can break them.

| Kit function used by the journey | Purpose |
|---|---|
| `Enemies()`, `SetExternalEnemies` | The shared slot list |
| `SpawnOrb` | Creature projectiles; the kit moves them, blocks them with the barrier and applies their damage |
| `SetDamageScale` | Eagle Feathers and Soul Ember |
| `ResetCooldowns` | Soul Ember |
| `Effect`, `RingEffect`, `Shake`, `Notice` | Feedback |
| `SetAutoKeys` | The test pilot's attack keys |
| `GlowTexture`, `QuadMesh` | Glow cards (lanterns, barriers, telegraphs) |

### Main functions

| Function | What it does |
|---|---|
| `Initialize` | Reads `Road.txt`, loads creature and prop models, switches the kit to external enemies, reads the save (normal runs) or the test variables (unattended runs), queues the prologue cards |
| `Update` | Per frame, in this order: place on the road, death and respawn handling, `Leash`, region card, `UpdateEncounter`, `UpdateFinale`, `UpdateChests`, `UpdateMonsters`, damage scale and regeneration, cards, F9 restart |
| `Leash` | Keeps the player within the road's half width, inside the arena while a fight runs, behind the gate of the next unbeaten arena, and off the ledge until the Eagle has fallen |
| `StartEncounter` / `BuildWave` / `ClearArena` | An arena fight: two or three waves bought from the region's roster with a cost budget that grows with region, arena and wave; a boss arena ends with the boss; winning heals, saves and opens the gate |
| `UpdateMonsters` | Behaviour of every creature: approach, windup and strike, ranged volleys, leaps, keeping distance, separation from each other, staying on the road |
| `UpdateBoss` | Extra moves: Rokapi blinks and summons; the Gveleshapi sends rings of orbs and summons Ali; the Devi Lord sends rings and summons Chinka; the Eagle dives and raises wing storms |
| `UpdateChests` / `OpenChest` | Puts a chest into a slot when the player is within `ChestWakeDistance`; gives its reward when broken |
| `UpdateFinale` | Words, the Eagle fight, the two chain anchors, the ending cards |
| `Save` / `Load` | `journey.sav` ([05-data.md](05-data.md), 8.4) |
| `StyleEnvironment` | Blends fog, ambient and moonlight colour toward the current region's values |
| `Draw` | Props (distance-culled), chests, creatures (procedural bob, lean, strike lunge, death sink), then translucent glow cards sorted back to front |
| `DrawHud` | Road panel, objective line, boss bar, story cards |
| `UpdatePilot` | Test pilot ([03-workflows.md](03-workflows.md), 6.11) |

### Creatures have no skeletons

Creature models are static meshes. All motion is computed in `Draw`: a walking bob and sway, a lean into the strike during the windup, a lunge on the hit, a sink and fade on death, a hover for the spirits. This is the largest visual gap against the hero, who is fully animated ([10-tech-debt-risks.md](10-tech-debt-risks.md)).

### Death and respawn

The engine owns player death. When `CombatSystem::PlayerActionsAllowed()` turns false the journey removes the current fight; when it turns true again (the player chose respawn in the engine's screen, and the engine put her at the scene's spawn point) the journey moves her to the last arena won. If the game was not updated while she was dead, a second check catches it: she was far down the road and is suddenly in the sanctuary. Verified with the mortal test pilot ([08-testing.md](08-testing.md), 11.2a).

### Staying alive

Four rules keep a crowd from killing her at once, all in `GameConstants.h`:

- **Mercy time.** Every hit goes through `NyxaraKit::Hurt`; after a hit, further hits do nothing for `MercySeconds`.
- **Attack turns.** At most `MaxAttackers` ordinary creatures wind up a blow at the same time; the others wait. Bosses are not counted.
- **Health from the fallen.** Each creature killed gives `KillHealBase + KillHealPerCost x cost` health; a beaten wave gives `WaveHeal`, a won arena `ArenaHeal`.
- **Rest.** Outside fights health returns at `RegenPerSecond`.
