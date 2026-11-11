# Constants and important values

[← Index](TECHNICAL_DOCUMENTATION.md)

**How to read this table**

- **Def** is where the value is defined, as `file:line`. Sections A to G were cited at v0.1.0 and their line numbers in `GameConstants.h` have since moved; section J was generated from the file at v0.2.0. Paths are under `game/src/` unless they start with `art/` or `engine/`.
- **Config?** says how the value changes: "code" means a constant in source (edit, rebuild); "art" means a value in an art script (rerun the script and the asset pipeline); "player" means a setting the player changes in the game.
- All values are [confirmed] by reading the cited line on 2026-11-10. Line numbers drift; the symbol name is the stable reference.

## A. Identity and content

| Name | Value | Purpose | Def | Config? | Limits / dependencies |
|---|---|---|---|---|---|
| `GameTitle` | `"Caucasian Eagle"` | Window title | `GameConstants.h:9` | code | Also written in the installer script (`tools/installer/CaucasianEagle.iss`) |
| `GameVersion` | `"0.2.0"` | Version | `GameConstants.h:10` | code | Read by `tools/package.ps1`; must match the git tag and the changelog |
| `UserDataFolder` | `"CaucasianEagle"` | Folder under `%LOCALAPPDATA%` for settings and saves | `GameConstants.h:11` | code | Changing it orphans existing players' settings and saves |
| `StartSceneId` | `"RoadToAmirani"` | Scene loaded at start | `GameConstants.h:14` | code | `assets/Compiled/Scenes/<id>.dscene` must exist in the stage |
| `HeroBundle` | `"Characters/Nyxara/EagleV1"` | Hero asset folder under `assets/Compiled` | `GameConstants.h:15` | code | Must contain the 31 clips listed in `Game::Prepare` |
| `DemoEnvironmentVariable` | `"CAUCASIAN_EAGLE_DEMO"` | `=1` starts the scripted showcase | `GameConstants.h:16` | code | |
| Rig signature | `DoorwaysRig_v1_74bb2d2053cd36e2` | Identity of the hero skeleton | printed by the character pipeline; stored in the bundle | art | Changes whenever the skeleton changes |
| Effects folder | `assets/Compiled/Effects/Nyxara` | Six effect textures | `NyxaraKit.cpp:238` | code | Start-up fails if a texture is missing |

## B. Movement and camera

| Name | Value | Units | Purpose | Def | Config? | Limits / dependencies |
|---|---|---|---|---|---|---|
| `WalkSpeed` | 1.38 | m/s | Ctrl walk speed; also the `Walk` clip's treadmill speed | `GameConstants.h:19` | code | Must match the authored `Walk` clip or feet slide |
| `RunClipSpeed` | 4.6 | m/s | Treadmill speed of `Run`, `StrafeLeft`, `StrafeRight` | `GameConstants.h:20` | code | Same |
| `WalkBackClipSpeed` | 1.33 | m/s | Treadmill speed of `WalkBackward` | `GameConstants.h:21` | code | Same |
| `WingBoostMultiplier` | 2.2 | × | Shift travel speed | `GameConstants.h:22` | code | Engine applies it to ground and flight travel |
| Glide lift | 0.45 (0.62 boosting) | m | Visual height while travelling or acting | `NyxaraKit.cpp:642` | code | Visual only; collision stays on the ground |
| Standing offset | 0.03 | m | Lifts the hero so her soles rest on the floor | `NyxaraKit.cpp:646` | code | Compensates the authored idle pose |
| Glide wing-beat rate | 1.25 (1.8 boosting) | × clip speed | Playback rate of `FlightHover` while travelling | `NyxaraKit.cpp:662` | code | |
| Lean pitch | 0.15 × clamp(forward speed / 5, −1, 1.7) | rad | Forward lean into travel | `NyxaraKit.cpp:655` | code | Spring: stiffness 70, damping 9.5 |
| Lean roll | −0.17 × clamp(side speed / 5, ±1.3) − 0.035 × turn rate | rad | Bank into strafes and turns | `NyxaraKit.cpp:656` | code | Clamped to ±0.3 |
| `CameraPitch` | −0.34 | rad | Starting view pitch (about 19° down) | `GameConstants.h:25` | code | Within `CameraMinPitch`..`CameraMaxPitch` |
| `CameraDistance` | 7.4 | m | Follow distance | `GameConstants.h:26` | code | Engine pulls the camera in when blocked |
| `CameraPivotHeight` | 1.35 | m | Orbit pivot above her feet | `GameConstants.h:27` | code | |
| `CameraMinPitch` / `CameraMaxPitch` | −1.2 / 0.45 | rad | View pitch limits | `GameConstants.h:28-29` | code | |
| Orbit floor | −0.04 | rad | Orbit never rises above this pitch, so the camera stays over the floor | `engine/src/DOORWAYS/DoorwaysGame.h` `CameraOrbitMaxPitch` | code (engine default) | |

## C. Actions (`Acts` table, `NyxaraKit.cpp:46-61`)

Rate is clip seconds per real second. End and hit times are in clip seconds. Real duration = End ÷ Rate.

| Action | Clip | Rate | End | Hit times | Real duration |
|---|---|---|---|---|---|
| LEFT TALON | `BasicAttack` | 1.45 | 1.00 | 0.53 | 0.69 s |
| RIGHT TALON | `SlashLeft` | 1.45 | 1.02 | 0.63 | 0.70 s |
| CHARGED SLASH | `HeavyAttack` | 1.30 | 1.75 | 1.20 | 1.35 s |
| TALON DASH | `Skill1` | 1.30 | 2.30 | 0.83, 1.43 | 1.77 s |
| FEATHER CYCLONE | `Skill2` | 1.25 | 2.25 | 0.40, 0.90, 1.27, 1.60 | 1.80 s |
| MOON LEAP | `Skill3` | 1.20 | 2.10 | 1.00 | 1.75 s |
| TWIN TALONS | `Combo1L` | 1.30 | 2.20 | 0.60, 1.17, 1.77 | 1.69 s |
| RISING EAGLE | `Combo1R` | 1.25 | 1.90 | 1.03 | 1.52 s |
| TEMPEST TRIPLE | `Combo2L` | 1.30 | 2.70 | 0.73, 1.50, 2.20 | 2.08 s |
| WHIRLWIND TALONS | `Combo2R` | 1.20 | 1.80 | 0.97 | 1.50 s |
| SOUL WEAVE | `Combo3L` | 1.35 | 3.40 | 1.17, 2.00, 3.10 | 2.52 s |
| MOONFALL DIVE | `Combo3R` | 1.15 | 2.40 | 1.32 | 2.09 s |
| SWORD BARRIER | `Guard` | 1.00 | 1.33 | none | held |
| SWORD EXCHANGE | `SwordSwap` | 1.10 | 0.86 | 0.43 (swap moment) | 0.78 s |

Hit times were measured from the hand-speed peaks of each authored clip. If a clip is re-authored, its hit times must be re-measured.

## D. Combat rules

| Name | Value | Units | Purpose | Def | Config? | Limits / dependencies |
|---|---|---|---|---|---|---|
| `SkillCooldown` | 4, 7, 9 | s | Cooldowns of F, V, C | `NyxaraKit.cpp:64` | code | HUD reads the same array |
| Skill press buffer | 0.45 | s | A skill press that cannot fire yet is remembered this long | `NyxaraKit.cpp:703` | code | |
| Slash chain window | 1.5 | s | Next LMB within this time continues left/right alternation | `NyxaraKit.cpp:771` | code | After it, the next slash is the left sword again |
| Combo window | 1.0 | s | Time after a skill ends in which LMB/RMB still starts its combo | `NyxaraKit.cpp:772` | code | Combos also open at 55% of the skill's clip |
| `GuardHoldClip` | 0.42 | clip s | Pose the barrier holds while E is down | `NyxaraKit.cpp:65` | code | |
| Guard maximum hold | 5 | s | Barrier drops after this | `NyxaraKit.cpp:758` | code | |
| Guard cooldown | 0.6 | s | After the barrier ends | `NyxaraKit.cpp:773` | code | |
| Swap cooldown | 0.4 | s | Between sword exchanges | `NyxaraKit.cpp:733` | code | |
| Sword slash | range 2.9 m, arc 70°, damage 16 | | LEFT / RIGHT TALON hit | `NyxaraKit.cpp:523,525` | code | |
| Charged slash | range 3.5 m, arc 85°, damage 34, launch 2 | | CHARGED SLASH hit | `NyxaraKit.cpp:527` | code | |
| Talon Dash | speed 26 m/s for 0.34 s, damage 26 to each shade passed | | Skill F dash | `NyxaraKit.cpp:464` | code | Resolved by engine collision |
| Talon Dash lance | damage 38, launch 2 | | Second hit of F | `NyxaraKit.cpp:534` | code | |
| Feather Cyclone pull radius | 8 | m | V drags shades toward her | `NyxaraKit.cpp:540` | code | |
| Rising Eagle | range 3.7 m, arc 75°, damage 48, launch 8 | | F then RMB | `NyxaraKit.cpp:565` | code | |
| Tempest Triple | range 3.3 m, arc 78°, damage 20, 20, 30 | | V then LMB | `NyxaraKit.cpp:576` | code | |
| Whirlwind Talons | speed 8.5 m/s, 2.4 s, damage 13 per tick | | V then RMB: thrown cyclone | `NyxaraKit.cpp:586-587` | code | |
| Twin Talons lunge | 11 m/s for 0.14 s per hit | | F then LMB | `NyxaraKit.cpp:559` | code | |
| Leap heights | Moon Leap 2.4, Moonfall Dive 3.3 | m | Visual height of the leap skills | `NyxaraKit.cpp:375-376` | code | Visual only |
| Hit-stop time scale | 0.06 | × | Simulation speed during a hit-stop | `NyxaraKit.cpp:319` | code | |
| Hit-stop length | 0.035 + 0.075 × heaviness | s | Freeze per hit | `NyxaraKit.cpp:350` | code | heaviness is 0..1 from damage |
| Camera shake | 0.03 + 0.13 × heaviness, decay e^(−8t) | m | Shake per hit | `NyxaraKit.cpp:351,810` | code | |

## E. Enemies (shades; switched off while the journey runs, see section J)

| Name | Value | Units | Purpose | Def | Config? |
|---|---|---|---|---|---|
| Count | 6 | | Shades placed around the arrival point | `NyxaraKit.cpp:259` | code |
| Ring radius | 8.5 and 11 (alternating) | m | Distance from the arrival point | `NyxaraKit.cpp:262` | code |
| Health | 160 | hit points | Per shade | `NyxaraKit.h:72` | code |
| Respawn | 8 | s | Time before a dead shade returns | `NyxaraKit.cpp:395` | code |
| First shot | 3 + 0.9 × index | s | Staggered first orb | `NyxaraKit.cpp:264` | code |
| Orb damage | 6 | hit points | Damage to the player per orb | `NyxaraKit.cpp:887` | code |
| Player health | 100 | hit points | Engine combat system | engine | engine |

## F. HUD

| Name | Value | Purpose | Def | Config? |
|---|---|---|---|---|
| Vitals panel | 330 × 104 (× UI scale) | Bottom-left panel size | `NyxaraHud.cpp:49` | code |
| Skill slot | 80, gap 8 (× UI scale) | Skill bar slot size | `NyxaraHud.cpp:74` | code |
| Enemy bar | 78 × 7 (× UI scale) | Health bar over each shade | `NyxaraHud.cpp:112` | code |
| Max enemy bars | 8 | Size of `NyxaraHudState::Enemies` | `NyxaraHud.h` | code |
| UI scale | player setting | Engine interface scale | engine settings | player |

Colours: soul green (0.22, 1.0, 0.52), moon silver (0.80, 0.90, 1.0), moon white (0.86, 0.97, 1.0), shade violet (0.62, 0.28, 1.0), gold (0.85, 0.73, 0.45) [`NyxaraKit.cpp:31-34`, `NyxaraHud.cpp:30-33`].

## H. Night sky

Drawn by `AmiraniMountain::DrawSky`; light and haze set in `AmiraniMountain::StyleEnvironment`.

| Name | Value | Units | Purpose | Def | Config? | Limits / dependencies |
|---|---|---|---|---|---|---|
| `SkyTexture` | "assets/textures/caucasus/NightSky.jpg" |  | Painted night sky, wraps horizontally | `GameConstants.h:34` | code | Built by `art/sky/make_sky_textures.py` |
| `MoonTexture` | "assets/textures/caucasus/Moon.jpg" |  | Moon surface, equirectangular | `GameConstants.h:35` | code | Same script |
| `MoonGlowTexture` | "assets/Compiled/Effects/Nyxara/Glow.png" |  | Soft disc used for the moon halo | `GameConstants.h:36` | code | Shared with the combat effects |
| `SkyRadius` | 3000 | m | Radius of the sky dome around the camera | `GameConstants.h:37` | code | Must stay below the engine far plane (5400 m) |
| `SkyTextureWraps` | 2 |  | Times the sky texture repeats around the horizon | `GameConstants.h:38` | code | Whole numbers only, or a seam shows |
| `SkyBrightness` | 0.46 |  | Dome tint | `GameConstants.h:39` | code | The engine's ambient light multiplies it |
| `MoonAzimuth` | -0.46 | rad | Moon direction, from +Z toward +X | `GameConstants.h:41` | code | Negative = left of the mountain |
| `MoonElevation` | 0.47 | rad | Moon height above the horizon | `GameConstants.h:42` | code |  |
| `MoonAngularSize` | 0.125 | rad | Apparent moon diameter | `GameConstants.h:43` | code | The real moon is 0.009 rad |
| `MoonBrightness` | 1.05 |  | Moon tint | `GameConstants.h:45` | code |  |
| `MoonHaloSize` | 4.2 |  | Halo width in moon diameters | `GameConstants.h:47` | code |  |
| `MoonlightX` | 0.72 |  | Direction the moonlight travels (X, Y, Z) | `GameConstants.h:49` | code | Not the true direction from the moon: chosen so the mountain face is lit |
| `HazeStart` | 40 | m | Distance where haze begins | `GameConstants.h:50` | code |  |
| `HazeEnd` | 900 | m | Distance where haze is complete | `GameConstants.h:51` | code | Must exceed the mountain distance or it disappears |
| `HazeStrength` | 0.62 |  | Haze strength | `GameConstants.h:52` | code |  |

## I. Mountain of Amirani

Scenery drawn by `AmiraniMountain::Draw`. Values not listed here (yaw angles, wrist and ankle fractions, halo colour) are in the same block of `GameConstants.h`.

| Name | Value | Units | Purpose | Def | Config? | Limits / dependencies |
|---|---|---|---|---|---|---|
| `LandmarkFolder` | "assets/Compiled/Landmarks/AmiraniMountain" |  | Folder of the four compiled meshes and their textures | `GameConstants.h:55` | code | Start-up fails if a file is missing |
| `MountainX`, `MountainZ` | 0, 1900 | m | Peak centre in the world | `GameConstants.h` | code | Must equal `MOUNTAIN` in `art/world/build_world.py`; the road ends on its ledge |
| `LedgeHeight` | 78 | m | Cave ledge above the play floor | `GameConstants.h:57` | code | Must equal `ledgeY` in `art/mountain/export/Mountain.json` |
| `LedgeHalfWidth` | 40 | m | Half the ledge width | `GameConstants.h:58` | code | Must equal `ledgeHalfWidth` in `Mountain.json` |
| `LedgeBackZ` | 8 | m | Cave back wall relative to the peak centre | `GameConstants.h:59` | code | Must equal `ledgeBackZ` in `Mountain.json` |
| `MountainTint` | 0.42 |  | Darkens the rock | `GameConstants.h` | code |  |
| `MountainFarDim` | 0.45 |  | How much the mountain and the figures darken when far away | `GameConstants.h` | code | The mountain takes no region fog (`ReceiveFog` off), so it is in view from the whole road; this dimming stands in for the fog |
| `MountainDimStart`, `MountainDimEnd` | 150, 1700 | m | Distance range over which the dimming grows | `GameConstants.h` | code |  |
| `FigureEmission` | 0.22 |  | Self-light of Amirani | `GameConstants.h:62` | code | Without it he is black at night |
| `EagleEmission` | 0.40 |  | Self-light of the eagle and the dog | `GameConstants.h:63` | code |  |
| `AmiraniHeight` | 50 | m | Height of Amirani | `GameConstants.h:64` | code | Sized to be read from far down the road |
| `AmiraniFromWall` | 10 | m | His distance in front of the cave wall | `GameConstants.h:65` | code |  |
| `EagleHeight` | 30 | m | Height of the eagle | `GameConstants.h:69` | code |  |
| `EagleSide` | -21 | m | Eagle offset to his right side | `GameConstants.h:72` | code | Negative = viewer's left |
| `EaglePeckPeriod` | 3.6 | s | Time between lunges at the wound | `GameConstants.h:75` | code |  |
| `DogHeight` | 13 | m | Height of the dog | `GameConstants.h:76` | code |  |
| `ChainLinkLength` | 2.6 | m | Length of one chain link | `GameConstants.h:80` | code |  |
| `ChainLinkWidth` | 1.5 | m | Width of one chain link | `GameConstants.h:81` | code |  |
| `VistaPitch` | 0.24 | rad | Camera pitch in the mountain capture mode | `GameConstants.h:31` | code | Only with `CAUCASIAN_EAGLE_VISTA=1` |

Art build values for the mountain (`art/mountain/build_mountain.py`): footprint 520 m, grid 261 x 261 (2 m cells), peak 250 m above a base 90 m below the play floor, ledge 80 m wide and 36 m deep.

## G. Art build values (`art/nyxara/blender/assemble.py`)

| Name | Value | Purpose | Def | Config? |
|---|---|---|---|---|
| `FPS` | 30 | Clip frame rate | `assemble.py:10` | art |
| `WING_LEN` | 1.30 m | Length of one wing | `assemble.py:67` | art |
| `RAISE`, `RAISE_SWEEP`, `BEND2`, `BEND3` | 6°, 14°, 8°, 10° | Resting wing pose: open at shoulder height, slight sweep, drooping tips | `assemble.py:211` | art |
| `CHIN_NECK`, `CHIN_HEAD` | −12°, −23° | Raises the head in standing and attack clips | `assemble.py:303` | art |
| `GRADE` | body (gamma 0.74, gain 1.12, saturation 1.15); wings (0.70, 1.18, 1.25) | Brightens the textures so she reads on the dark map | `assemble.py:501` | art |
| Sword lengths | Moonfeather 1.02 m, Talonshade 0.92 m | | `assemble.py` `make_sword` calls | art |

## J. The journey (`Journey.cpp`, `GameConstants.h`)

| Name | Value | Purpose | Def | Config? |
|---|---|---|---|---|
| `RoadFile` | `"assets/Compiled/World/Road.txt"` | Path, arenas, chests, props, lights | `GameConstants.h:20` | code |
| `SaveFileName` | `"journey.sav"` | In the user data folder | `GameConstants.h:21` | code |
| `PilotEnvironmentVariable` | `"CAUCASIAN_EAGLE_PILOT"` | =1 with the showcase: a test pilot plays the whole road | `GameConstants.h:22` | code |
| `SpeedEnvironmentVariable` | `"CAUCASIAN_EAGLE_SPEED"` | Test pilot only: simulation speed 1..12 | `GameConstants.h:23` | code |
| `MortalEnvironmentVariable` | `"CAUCASIAN_EAGLE_MORTAL"` | =1: the test pilot can die and asks for a respawn (tests death) | `GameConstants.h:24` | code |
| `StartEnvironmentVariable` | `"CAUCASIAN_EAGLE_START"` | Start after this many arenas (testing; ignores the save) | `GameConstants.h:25` | code |
| `SanctuaryRadius` | 97 | m, inside it the road rules do not apply | `GameConstants.h:26` | code |
| `LedgeWalkFrontZ` | 1872 | m, front edge of the walkable ledge under Amirani (MountainZ - 28) | `GameConstants.h:27` | code |
| `LedgeWalkBackZ` | 1889 | m, back edge of the walkable ledge | `GameConstants.h:28` | code |
| `LedgeWalkHalfWidth` | 38 | m | `GameConstants.h:29` | code |
| `MonsterHealthScale` | 1.8 | Health of every ordinary creature | `GameConstants.h:30` | code |
| `BossHealthScale` | 1.0 | Health of the three region bosses and the Eagle | `GameConstants.h:31` | code |
| `MonsterDamageScale` | 0.6 | Every monster's damage | `GameConstants.h:32` | code |
| `LateWaveStage` | 3 | From this region on an arena has 4 waves instead of 3 | `GameConstants.h:33` | code |
| `PatrolBudget` | 0.55 | A roaming pack costs this share of an arena's first wave | `GameConstants.h:34` | code |
| `PatrolAhead` | 26 | m, a roaming pack appears this far ahead of her | `GameConstants.h:35` | code |
| `PatrolGiveUp` | 70 | m, a pack left this far behind gives up | `GameConstants.h:36` | code |
| `StageHealthGrowth` | 0.12 | + per stage (12 stages) | `GameConstants.h:37` | code |
| `StageDamageGrowth` | 0.03 | + per stage | `GameConstants.h:38` | code |
| `FeatherDamageBonus` | 0.03 | Nyxara's damage per Eagle Feather | `GameConstants.h:39` | code |
| `EmberSeconds` | 20 | Soul Ember duration | `GameConstants.h:40` | code |
| `EmberDamage` | 1.5 | Nyxara's damage multiplier while it burns | `GameConstants.h:41` | code |
| `MercySeconds` | 1.0 | After a hit she cannot be hurt again for this long (a crowd cannot kill her in one moment) | `GameConstants.h:42` | code |
| `MaxAttackers` | 2 | Ordinary creatures that may wind up a blow at the same time | `GameConstants.h:43` | code |
| `RegenPerSecond` | 3 | Health per second outside fights | `GameConstants.h:44` | code |
| `KillHealBase` | 4 | Health she takes back from every creature she fells... | `GameConstants.h:45` | code |
| `KillHealPerCost` | 3 | ...plus this for each point of its cost (a Devi gives 34) | `GameConstants.h:46` | code |
| `ArenaHeal` | 30 | Health for winning an arena | `GameConstants.h:47` | code |
| `WaveHeal` | 20 | Health when a wave is beaten and another comes | `GameConstants.h:48` | code |
| `MoonTearHeal` | 40 | Health from a Moon Tear | `GameConstants.h:49` | code |
| `WaveDelay` | 1.4 | s between waves | `GameConstants.h:50` | code |
| `SpawnInterval` | 0.3 | s between monsters of one wave | `GameConstants.h:51` | code |
| `ChestWakeDistance` | 30 | m, a chest becomes breakable inside this distance | `GameConstants.h:52` | code |
| `PropDrawDistance` | 260 | m, trees and rocks | `GameConstants.h:53` | code |
| `TowerDrawDistance` | 600 | m, towers | `GameConstants.h:54` | code |
| `LightDrawDistance` | 140 | m, lantern and crystal glows | `GameConstants.h:55` | code |
| `PropTint` | 0.55 | Night tint of scenery props | `GameConstants.h:56` | code |
| `MonsterTint` | 0.85 | Monsters read against the night | `GameConstants.h:57` | code |
| `MonsterEmission` | 0.10 |  | `GameConstants.h:58` | code |

Values inside `Journey.cpp` (symbol names are the reference):

| Name | Value | Purpose | Config? |
|---|---|---|---|
| `Kinds[]` | 12 creatures, the chest and the chain anchor | Per creature: model, height, health, speed, melee reach / damage / cooldown / windup, orb range / damage / interval / volley, keep-distance, radius, push scale, cost, colour, boss flag, hover | code |
| `Rosters[12]` | Up to three creature kinds per region | Who can appear in a region's arenas and roaming packs | code |
| `StageBoss[12]` | Gveleshapi (region 9), Devi Lord (10), Rokapi (11) | Boss of a region's boss arena | code |
| Wave budget | (4 + 1.4 x region) x (1 + 0.2 x arena number within the region) x (0.8 + 0.2 x wave) | Cost points a wave may spend on creatures; at most 11 creatures | code (`BuildWave`) |
| Waves per arena | 2 in region 1 and in boss arenas, 3 otherwise, 4 from region `LateWaveStage` + 1 on | | code (`StartEncounter`) |
| `Air[12]` | Fog colour, fog end, fog strength, ambient and light colour per region | Atmosphere blend | code |
| `StageCards[12]`, `Lore[12]` | Region title cards and the twelve memories | Story text | code |
| Kit slot limit `NyxaraKit::MaxEnemies` | 30 | Creatures, chests and anchors alive at once | code (`NyxaraKit.h`) |

Art build values for the world (`art/world/build_world.py`, tables at the top of the file): road half width 6.5 m, arena radius 17 m, boss arena radius 24 m, path sample spacing 2 m, terrain grid 4 m, world bounds X -680..680 and Z -180..2010, mountain at (0, 1900), ledge 78 m high. Path length 6,346 m, 34 arenas, 81 chests, 664 props.
