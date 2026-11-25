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
| `GameVersion` | `"0.5.1"` | Version | `GameConstants.h:40` | code | Read by `tools/package.ps1`; must match the git tag, the changelog and the numbers in `game/CaucasianEagle.rc` |
| `UserDataFolder` | `"CaucasianEagle"` | Folder under `%LOCALAPPDATA%` for settings and saves | `GameConstants.h:11` | code | Changing it orphans existing players' settings and saves |
| `StartSceneId` | `"RoadToAmirani"` | Scene loaded at start | `GameConstants.h:14` | code | `assets/Compiled/Scenes/<id>.dscene` must exist in the stage |
| `HeroBundle` | `"Characters/Nyxara/EagleV1"` | Hero asset folder under `assets/Compiled` | `GameConstants.h:15` | code | Must contain the 31 clips listed in `Game::Prepare` |
| `DemoEnvironmentVariable` | `"CAUCASIAN_EAGLE_DEMO"` | `=1` starts the scripted showcase | `GameConstants.h:16` | code | |
| Rig signature | `DoorwaysRig_v1_116d1b0e1935a8eb` | Identity of the hero skeleton | printed by the character pipeline; stored in the bundle | art | Changes whenever the skeleton changes |
| Effects folder | `assets/Compiled/Effects/Nyxara` | Six effect textures | `NyxaraKit.cpp:238` | code | Start-up fails if a texture is missing |

## B. Movement and camera

| Name | Value | Units | Purpose | Def | Config? | Limits / dependencies |
|---|---|---|---|---|---|---|
| `WalkSpeed` | 1.38 | m/s | Ctrl walk speed; also the `Walk` clip's treadmill speed | `GameConstants.h:19` | code | Must match the authored `Walk` clip or feet slide |
| `RunClipSpeed` | 4.6 | m/s | Treadmill speed of `Run`, `StrafeLeft`, `StrafeRight` | `GameConstants.h:20` | code | Same |
| `WalkBackClipSpeed` | 1.33 | m/s | Treadmill speed of `WalkBackward` | `GameConstants.h:21` | code | Same |
| `WingBoostMultiplier` | 2.2 | × | Shift travel speed | `GameConstants.h:22` | code | Engine applies it to ground and flight travel |
| `GlideLift`, `GlideLiftBoost` | 0.75, 1.0 | m | Height of her feet over the ground while she soars; with the wing boost | `GameConstants.h` | code | Presentation only |
| `GlideLiftRate` | 2.6 | 1/s | How fast she rises to that height and settles from it | `GameConstants.h` | code | Lower = slower |
| `SoarSway`, `SoarSwayRate` | 0.06, 1.3 | m, rad/s | Slow rise and fall on the air while soaring | `GameConstants.h` | code | |
| `SoarBank` | 0.06 | rad per rad/s | Bank into a turn | `GameConstants.h` | code | Held under 0.42 rad |
| `SoarClimbPitch` | 0.30 | rad | Nose up in a full climb, down in a full dive (free flight) | `GameConstants.h` | code | |
| `SoarSpring`, `SoarDamping` | 30, 10 | | Spring of the lean and bank | `GameConstants.h` | code | Lower spring = slower, smoother |
| `SoarBoostRate` | 1.35 | × | Wing clips play faster with the wing boost | `GameConstants.h` | code | |
| `FlightSway`, `FlightSwayRate` | 0.11, 1.9 | m, rad/s | She rides up and down in flight (two rhythms, 0.7 and 0.3 of it) | `GameConstants.h` | code | Presentation only |
| `FlightRock` | 0.035 | rad | Her body rocks in flight (pitch; roll 0.8 of it, slower) | `GameConstants.h` | code | Not during sword work |
| `FlightGroundLift` | 0.8 | m | She hovers this high when she is on the ground in flight | `GameConstants.h` | code | Presentation only: collision is at the ground |
| `FlightTakeoffHeight`, `FlightTakeoffSeconds` | 6, 3 | m, s | R on the ground: she rises this far in this time | `GameConstants.h` | code | Engine `FreeFlightTakeoffHeight`, `FreeFlightTakeoffSeconds` |
| `FlightLandReach` | 6 | m | Height over the ground or a roof within which R lands her | `GameConstants.h` | code | |
| `FlightLandSeconds`, `FlightLandMinSpeed` | 3, 0.05 | s, m/s | The change from flying to walking on R: its time; the slowest she sinks | `GameConstants.h` | code | She sinks at height / time left |
| `FootSpeed`, `FootRunSpeed` | 3.2, 5.4 | m/s | On foot: an easy jog; running with Shift | `GameConstants.h` | code | Both over 3 m/s, so the engine plays Run (authored 4.6 m/s) at 0.70 and 1.17 of its pace |
| `RoofStep` | 0.5 | m | She steps up onto a building's surface this far above her feet; lower than this under it, its wall stops her | `GameConstants.h` | code | |
| `RoofGrid` | 96 | cells | Side of a building's top-height map (`Journey::Model::Top`) | `GameConstants.h` | code | Built at load from the mesh of every model under `Cities/` |
| `FlightSteerGain` | 1 | | Climb or dive angle per radian of camera pitch away from `CameraPitch` | `GameConstants.h` | code | |
| `FlightCeiling` | 45 | m | How far above the ground (or roof) under her free flight can climb; `Journey` gives the engine the difference to the take-off height every frame | `GameConstants.h` | code | Engine `FreeFlightCeiling` |
| `DeathSeconds` | 3.5 | s she lies where she fell before the game asks the engine for the respawn (the engine's own delay is 3) | `GameConstants.h` | code | |
| `RageCrySeconds` | 2.5 | s | Least time between two of her cries of fury on kills | `GameConstants.h` | code | |
| `FlyEnvironmentVariable` | `"CAUCASIAN_EAGLE_FLY"` | | Testing, with the pilot: R at 4 s, Space held 8 to 9 s (she climbs), pushed onto the ground 10 to 14 s, R at 16 s; logs `[FLY]` lines; 2 also holds the pilot still | `GameConstants.h` | environment | |
| `CAUCASIAN_EAGLE_BOW` (read in `NyxaraKit::Update`) | 1 or 2 | | Testing, unattended only: the bow is out and the left button held; 1 also aims. Logs `[BOW] shots hits` each second | `NyxaraKit.cpp` | environment | |
| `RoofEnvironmentVariable` | `"CAUCASIAN_EAGLE_ROOF"` | | Testing, with the pilot: value N drops her from 4 m over building N-1 ten seconds into play and holds the pilot still; logs `[ROOF]` lines | `GameConstants.h` | env | |
| Glide lift | 0.45 (0.62 boosting) | m | Visual height while travelling or acting | `NyxaraKit.cpp:642` | code | Visual only; collision stays on the ground |
| Standing offset | 0.03 | m | Lifts the hero so her soles rest on the floor | `NyxaraKit.cpp:646` | code | Compensates the authored idle pose |
| Glide wing-beat rate | 1.25 (1.8 boosting) | × clip speed | Playback rate of `FlightHover` while travelling | `NyxaraKit.cpp:662` | code | |
| Lean pitch | 0.15 × clamp(forward speed / 5, −1, 1.7) | rad | Forward lean into travel | `NyxaraKit.cpp:655` | code | Spring: stiffness 70, damping 9.5 |
| Lean roll | −0.17 × clamp(side speed / 5, ±1.3) − 0.035 × turn rate | rad | Bank into strafes and turns | `NyxaraKit.cpp:656` | code | Clamped to ±0.3 |
| `CameraPitch` | −0.50 | rad | Starting view pitch (about 29° down; more negative puts the camera higher) | `GameConstants.h:201` | code | Within `CameraMinPitch`..`CameraMaxPitch` |
| `CameraDistance` | 15.0 | m | Follow distance | `GameConstants.h:202` | code | Engine pulls the camera in when blocked |
| `CameraMaxDistance` | 18.0 | m | How far the mouse wheel can pull the camera back; set on the engine camera (`MaxFollowDistance`, engine default 8) before the distance | `GameConstants.h:203` | code | Must be at least `CameraDistance`, or the engine cuts the distance down to it |
| `CameraPivotHeight` | 1.35 | m | Orbit pivot above her feet | `GameConstants.h:27` | code | |
| `BowReleaseSeconds` / `BowDrawSeconds` / `BowInterval` | 0.08 / 0.253 / 0.333 | s | Her hand back to the string / the draw / between arrows while the button is held (the sum) | `GameConstants.h` | code | |
| `BowDamage` / `BowAimDamage` | 25 / 25 | | An arrow loosed without aiming / aimed (before her damage scale) | `GameConstants.h` | code | |
| `BowArrowSpeed` / `BowAimArrowSpeed` / `BowRange` | 75 / 75 / 60 | m/s, m/s, m | An arrow's speed (not aimed / aimed) and reach | `GameConstants.h` | code | |
| `BowHipRange` | 32 | m | Within this an arrow loosed without aiming finds the nearest creature ahead of the camera | `GameConstants.h` | code | |
| `BowAimAssist` | 0.5 | m | Beside a creature's body the mark still counts as on it | `GameConstants.h` | code | |
| `BowAimDistance` / `BowAimSide` / `BowAimRaise` | 4.5 / 0.85 / 0.35 | m | The camera while she aims: distance, to her right, pivot raised | `GameConstants.h` | code | Engine: `CameraSideOffset`, `CameraPivotHeight` |
| `BowAimLook` | 0.6 | | The mouse turns the view this much as fast while she aims | `GameConstants.h` | code | |
| `BowAimAssistFar` | 0.012 | m per m | Added to `BowAimAssist` for each metre the creature is away | `GameConstants.h` | code | |
| `BowHoming` | 10 | 1/s | How hard an arrow bends to the creature it was loosed at (only while that is within 60 degrees ahead of it) | `GameConstants.h` | code | |
| `BowSnapDegrees` / `BowSnapRate` | 22 / 12 | deg, 1/s | As she raises the bow the view turns to the creature nearest its middle within this angle, at this rate, until the camera is in (`mAim` > 0.92) | `GameConstants.h` | code | |
| `BowAimLevelPitch` | −0.10 | rad | With no creature to turn to, the view comes up to this | `GameConstants.h` | code | The pitch she had comes back when she lets go |
| `BowAimZoom` | 0.20 | | The camera's field of view narrows by this part while she aims (engine: `MainCamera.FieldOfViewY`) | `GameConstants.h` | code | |
| `BowAimFriction` | 0.55 | | With the mark on a creature the look scale is multiplied by this again | `GameConstants.h` | code | |
| `BowPoseSeconds` / `BowClipEnd` | 0.45 / 0.2 | s | She keeps the bow up after an arrow / the length of `BowShot` | `GameConstants.h` | code | |
| `CameraMinPitch` / `CameraMaxPitch` | −1.2 / 0.45 | rad | View pitch limits | `GameConstants.h:28-29` | code | |
| Orbit floor | −0.04 | rad | Orbit never rises above this pitch, so the camera stays over the floor | `engine/src/DOORWAYS/DoorwaysGame.h` `CameraOrbitMaxPitch` | code (engine default) | |

## C. Actions (`Acts` table, `NyxaraKit.cpp:46-61`)

Rate is clip seconds per real second. End and hit times are in clip seconds. Real duration = End ÷ Rate.

| Action | Clip | Rate | End | Hit times | Real duration |
|---|---|---|---|---|---|
| RIGHT TALON | `BasicAttack` | 1.45 | 1.00 | 0.53 | 0.69 s |
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
| AXE BARRIER | `Guard` | 1.00 | 1.33 | none | held |
| AXE EXCHANGE | `SwordSwap` | 1.10 | 0.86 | 0.43 (swap moment) | 0.78 s |
| arrows (no `Act`) | `BowShot` | 1.00 | 0.20 | 0 (loosed) | 0.20 s |

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
| Layout sizes | in layout units, 1 unit = 1 px at 1280 × 720 | Emblem 92, health bar 214 × 17, energy bar 168 × 10, skill slot 62 with gaps 7 and 17 between groups, icon 38, key badge 19 | `NyxaraHud.cpp`, the `constexpr` block at the top of `DrawNyxaraHud` | code |
| Journey HUD sizes | layout units | Emblem 66, road bar 236 × 11, guardian bar up to 520 × 17, notice 300 wide, at most 14 enemy dots | `Journey.cpp`, the `constexpr` block at the top of `Journey::DrawHud` | code |
| Journal sizes | layout units | Panel up to 1060 × 664, text sizes, column gap | `Story.cpp`, the `constexpr` block above `DrawJournal` | code |
| Low health | 0.3 | Below this share the bar and screen edge turn red | `NyxaraHud.cpp` | code |
| Health trail | 0.45 s delay, 0.35 per second | How the pale "just lost" part of the health bar drains | `NyxaraHud.cpp` | code |
| Max enemy bars | 8 | Size of `NyxaraHudState::Enemies` | `NyxaraHud.h` | code |
| `MaxToasts` | 4 | Chest notices shown at once | `GameConstants.h` | code |
| `RestartHoldSeconds` | 1.5 s | How long F9 is held to begin again | `GameConstants.h` | code |
| `JournalTimeScale` | 0.001 | Simulation speed while the journal, the map, her window, her menu or the guide is open | `GameConstants.h` | code |
| `WallReach` | 12 | m beyond the road's edge within which the wall of an unwon arena stops her | `GameConstants.h` | code |
| `CityReach` | 25 | m beyond the road's edge within which she is in a city | `GameConstants.h` | code |
| `WayOffRoad` | 2 | m beyond the road's edge from which the way back is shown | `GameConstants.h` | code |
| `WayStep`, `WayTrail` | 30, 30 | m along the road to the way's target; length of the trail of lights | `GameConstants.h` | code |
| `WayFadeSeconds` | 0.4 | Fade of the compass and the trail | `GameConstants.h` | code |
| `TroubleFirst`, `TroubleLastBefore` | 150, 190 | m from the start and before the end of the road without trouble | `GameConstants.h` | code |
| `TroubleEvery` | 85 | m between two troubles, give or take a quarter | `GameConstants.h` | code |
| `TroubleClear` | 20 | m kept free round every arena and city | `GameConstants.h` | code |
| `TroubleWarn` | 14 | m before a trouble at which the objective line names it | `GameConstants.h` | code |
| `MireDamage` | 7 | Cursed ground, each bite (before `DamageTaken()`) | `GameConstants.h` | code |
| `RockfallReach`, `RockfallEvery`, `RockfallWarn` | 16, 1.5, 1.1 | m of the stretch; s between stones; s the ring shows first | `GameConstants.h` | code |
| `RockfallSpread`, `RockfallRadius`, `RockfallDamage` | 2.2, 2.3, 12 | m round her a stone is aimed; m it hurts; harm | `GameConstants.h` | code |
| `WispCount`, `WispGap`, `WispSpeed` | 3, 4.5, 1.15 | Lights; m between them along the road; radians a second of swing | `GameConstants.h` | code |
| `WispTouch`, `WispDamage` | 1.15, 10 | m; harm | `GameConstants.h` | code |
| `TrapHeight`, `TrapSide`, `TrapHalf`, `TrapWarn`, `TrapCrystals` | 70, 26, 7, 30, 30 | m a trap reaches up; m its scenery stands to the side; m half width of its course; m before it the HUD names it; crystals for beating it | `GameConstants.h` | code |
| `GateRunLength`, `GateRunPace`, `GateRunGrace`, `GateRetry` | 195, 1.45, 3, 3 | m from light to door; the clock is the length over (her cruising speed times the pace) plus the grace, about 37 s; s the light sleeps after a loss | `GameConstants.h` | code |
| `GateSwitchHeight`, `GateSwitchTouch`, `GateArchTall`, `GateHalf`, `GateTall`, `GateDoor` | 4.5, 3.6, 22, 8, 14, 1.2 | m: the light's height and touch; the arch's height; the door's half width, height and thickness | `GameConstants.h` | code |
| `GateBars`, `GateBarLow`, `GateBarHigh`, `GateBarOut`, `GateBarDamage`, `GateGuards` | 5, 2, 15, 9, 12, 4 | bars; m they slide between; m half length; harm; guards woken | `GameConstants.h` | code |
| `WindLength`, `WindEdge`, `WindCalm`, `WindBlow`, `WindPush`, `WindWall`, `WindDamage` | 56, 3, 2.4, 1.8, 1.1, 1, 11 | m; m from the course's edge to a wall; s calm; s gust; push as a multiple of her speed; m wall; harm | `GameConstants.h` | code |
| `LauncherLength`, `LauncherTall`, `LauncherEvery`, `LauncherSpeed`, `LauncherDamage` | 60, 5.5, 2.6, 13, 9 | Rune stones: m; m; s between fans; m/s; harm of an orb | `GameConstants.h` | code |
| `WatchLength`, `WatchTall`, `WatchTurn`, `WatchReach`, `WatchWidth`, `WatchDamage`, `WatchGuardEvery` | 50, 9, 0.75, 36, 1.3, 14, 9 | m; m; rad/s; m a sheet reaches; m its thickness; harm; s between guards | `GameConstants.h` | code |
| `SealLength`, `SealRadius`, `SealCracks`, `SealEvery`, `SealWarn` | 48, 4.5, 3, 1.3, 1.25 | m; m; cracks that break it; s between stones; s of warning | `GameConstants.h` | code |
| `PressLength`, `PressTall`, `PressEvery`, `PressWarn`, `PressFall`, `PressOpen`, `PressDamage`, `PressOrbDamage` | 44, 11, 3.6, 1, 0.25, 1.4, 28, 8 | m; m; s cycle; s warning; s fall; s the link can be hurt; harm of the blow; of an orb | `GameConstants.h` | code |
| `LAND_CHUNK` | 16 | Terrain cells per side of one piece of land collision | `art/world/build_world.py` | code |
| `RoamBody`, `TrunkRadius` | 0.45 m, 0.045 of a tree's width | Her radius against obstacles; a trunk's radius | `GameConstants.h` | code |
| `ComboWindowSeconds` | 1.2 | s after a skill ends in which its combo can start | `GameConstants.h` | code |
| `ComboBufferSeconds`, `SkillBufferSeconds` | 0.6, 0.45 | s an early mouse press / skill key is remembered | `GameConstants.h` | code |
| `RecoveryCancel` | 0.22 | Clip s after the last hit of a skill or combo, from which its flourish can be cut | `GameConstants.h` | code |
| `SlashChainAfterHit`, `HeavyChainAfterHit` | 0.12, 0.30 | Clip s after a slash / the heavy blow lands, at which a kept click starts the next attack | `GameConstants.h` | code |
| `SlashChainSeconds`, `GuardCooldownSeconds` | 1.5, 0.6 | s the next click still swings the other sword; s before the barrier can rise again | `GameConstants.h` | code |
| `PauseRows` | 8 | Rows of her menu (Esc) | `GameConstants.h` | code |
| `Guide::PageCount` | 7 | Pages of the guide (the last says who made the game and the engine) | `Guide.h` | code |
| `JournalFadeSeconds` | 0.18 s | Journal fade | `GameConstants.h` | code |
| `JournalScrollSpeed` | 520 units/s | Journal scrolling while W or S is held | `GameConstants.h` | code |
| Atlas | 1024 × 1024, 2 texels per layout unit | `assets/ui/Hud/HudAtlas.png` | `art/ui/build_hud.py` (`ATLAS`, `SCALE`) | script |
| UI scale | player setting | Engine interface scale | engine settings | player |
| `CAUCASIAN_EAGLE_JOURNAL` | 1..4, or 11..14 with everything unlocked | Unattended runs: hold the journal open on that page | `GameConstants.h` | environment |
| `CAUCASIAN_EAGLE_RESTART` | seconds | Unattended runs: hold F9 from that second | `GameConstants.h` | environment |

Colours are in `GameUi.h` (`Palette`): ink, dim, faint, gold, light gold, silver, soul green (0.30, 1.0, 0.58), moon blue (0.52, 0.80, 1.0), red, violet. The kit's effect colours are in `NyxaraKit.cpp:31-34`.

## H. Night sky

Drawn by `AmiraniMountain::DrawSky`; light and haze set in `AmiraniMountain::StyleEnvironment`.

| Name | Value | Units | Purpose | Def | Config? | Limits / dependencies |
|---|---|---|---|---|---|---|
| `SkyTexture` | "assets/textures/caucasus/NightSky.jpg" |  | Painted night sky, wraps horizontally | `GameConstants.h:34` | code | Built by `art/sky/make_sky_textures.py` |
| `SkyRadius` | 3000 | m | Radius of the sky dome around the camera | `GameConstants.h:37` | code | Must stay below the engine far plane (5400 m) |
| `SkyTextureWraps` | 2 |  | Times the sky texture repeats around the horizon | `GameConstants.h:38` | code | Whole numbers only, or a seam shows |
| `SkyBrightness` | 0.46 |  | Dome tint | `GameConstants.h:39` | code | The engine's ambient light multiplies it |
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
| Axes | Inferno 0.95 m, Serpentcoil 1.00 m, held a fifth of the way up, 7 000 triangles each | | `assemble.py`: `AXE_LENGTH`, `AXE_GRIP_UP`, `AXE_TRIS` | art |

## J. The journey (`Journey.cpp`, `GameConstants.h`)

| Name | Value | Purpose | Def | Config? |
|---|---|---|---|---|
| `RoadFile` | `"assets/Compiled/World/Road.txt"` | Path, arenas, chests, props, lights | `GameConstants.h:20` | code |
| `SaveFileName` | `"journey.sav"` | In the user data folder | `GameConstants.h:21` | code |
| `PilotEnvironmentVariable` | `"CAUCASIAN_EAGLE_PILOT"` | =1 with the showcase: a test pilot plays the whole road | `GameConstants.h:22` | code |
| `SpeedEnvironmentVariable` | `"CAUCASIAN_EAGLE_SPEED"` | Test pilot only: simulation speed 1..12 | `GameConstants.h:23` | code |
| `MortalEnvironmentVariable` | `"CAUCASIAN_EAGLE_MORTAL"` | =1: the test pilot can die and asks for a respawn (tests death) | `GameConstants.h:24` | code |
| `StartEnvironmentVariable` | `"CAUCASIAN_EAGLE_START"` | Start after this many arenas (testing; ignores the save) | `GameConstants.h:25` | code |
| `StartS` | 2 | m along the road where a new journey begins | `GameConstants.h` | code |
| `LedgeWalkFrontZ` | 1872 | m, front edge of the walkable ledge under Amirani (MountainZ - 28) | `GameConstants.h:27` | code |
| `LedgeWalkBackZ` | 1889 | m, back edge of the walkable ledge | `GameConstants.h:28` | code |
| `LedgeWalkHalfWidth` | 38 | m | `GameConstants.h:29` | code |
| `MonsterHealthScale` | 1.8 | Health of every ordinary creature | `GameConstants.h:30` | code |
| `BossHealthScale` | 1.0 | Health of the three region bosses and the Eagle | `GameConstants.h:31` | code |
| `MonsterSpeedScale`, `MonsterSprint`, `MonsterSprintDistance` | 1.2, 1.6, 14 m | Creature speed; sprint multiple and the distance beyond which they sprint | `GameConstants.h` | code |
| `MonsterTurnRate`, `MonsterLeadSeconds` | 9 rad/s, 0.45 s | Turning; how far ahead of her they aim | `GameConstants.h` | code |
| `MonsterFlank`, `MonsterFlankDistance`, `MonsterCircleSpeed` | 0.65, 7 m, 0.55 | Swing to the sides while closing in; circling while waiting to strike | `GameConstants.h` | code |
| `MonsterAttackPace` | 0.8 | Ordinary creatures' time between blows, times this | `GameConstants.h` | code |
| `MonsterDamageScale` | 0.85 | Every monster's damage | `GameConstants.h:32` | code |
| `LateWaveStage` | 3 | From this region on an arena has 4 waves instead of 3 | `GameConstants.h:33` | code |
| `PatrolBudget` | 0.8 | A roaming pack costs this share of an arena's first wave | `GameConstants.h:34` | code |
| `PatrolAhead` | 26 | m, a roaming pack appears this far ahead of her | `GameConstants.h:35` | code |
| `PatrolGiveUp` | 70 | m, a pack left this far behind gives up | `GameConstants.h:36` | code |
| `PatrolEvery` | 55 | m of road per roaming pack between two fights (at least one, at most six) | `GameConstants.h` | code |
| `PackLost`, `PackRoam`, `PackDrop`, `PackClimb`, `PackFlank` | 130, 40, 3, 7, 11 | m: a pack gives up this far from her; follows this far off the road; only over land this far below and above the road; comes in this far to the side | `GameConstants.h` | code |
| `MeleeReachUp`, `FlyerCeiling`, `FlyerClimb` | 1.2, 22, 5.5 | m a blow reaches over a creature's head; m a flyer rises; m/s it climbs | `GameConstants.h` | code |
| `LeapReach`, `LeapRange`, `LeapSeconds`, `LeapEvery`, `LeapTouch`, `LeapDamage` | 15, 11, 0.95, 4.5, 1.5, 1.2 | m up and m away a small creature leaps; s in the air; s between leaps; m touch; multiple of its blow | `GameConstants.h` | code |
| `ThrowReach`, `ThrowRange`, `ThrowSpeed`, `ThrowEvery`, `ThrowDamage` | 34, 28, 15, 3.2, 0.8 | m up and m away a giant throws; m/s; s between throws; multiple of its blow | `GameConstants.h` | code |
| `UnderLand`, `CorridorRim` | 0.8, 8 | m under the land at which she is lifted back (when not on ground); m beyond the road's edge where she is lifted to the road's level if it is the higher | `GameConstants.h` | code |
| `PlaceCrystals` | 8 | crystals for finding a place | `GameConstants.h` | code |
| `StageHealthGrowth` | 0.20 | + per stage (12 stages) | `GameConstants.h:37` | code |
| `StageDamageGrowth` | 0.07 | + per stage | `GameConstants.h:38` | code |
| `FeatherDamageBonus` | 0.03 | Nyxara's damage per Eagle Feather | `GameConstants.h:39` | code |
| `EmberSeconds` | 20 | Soul Ember duration | `GameConstants.h:40` | code |
| `EmberDamage` | 1.5 | Nyxara's damage multiplier while it burns | `GameConstants.h:41` | code |
| `MercySeconds` | 1.0 | After a hit she cannot be hurt again for this long (a crowd cannot kill her in one moment) | `GameConstants.h:42` | code |
| `MaxAttackers` | 2 | Ordinary creatures that may wind up a blow at the same time | `GameConstants.h:43` | code |
| `PotionHeal` | 35 | Health from a potion (chests give potions, not health) | `GameConstants.h` | code |
| `ArenaHeal` | 30 | Health for winning an arena | `GameConstants.h:47` | code |
| `WaveHeal` | 20 | Health when a wave is beaten and another comes | `GameConstants.h:48` | code |
| `MoonShardCrystals` | 15 | Crystals from a Moon Shard (chests do not heal) | `GameConstants.h` | code |
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
| `Story::Chapters[12]`, `Story::Memories[12]` | Region titles and texts, and the twelve memories (`Story.cpp`) | Story text | code |
| Kit slot limit `NyxaraKit::MaxEnemies` | 30 | Creatures, chests and anchors alive at once | code (`NyxaraKit.h`) |

Cities and maps (`GameConstants.h`, section "the seven cities"; all code-only):

| Name | Value | Purpose |
|---|---|---|
| `CitiesFile`, `MapFile`, `MapTexture` | `assets/Compiled/World/Cities.txt`, `.../Map.txt`, `assets/ui/Map/WorldMap.png` | City data, map extent, map picture |
| `MapEnvironmentVariable` | `"CAUCASIAN_EAGLE_MAP"` | Testing: =1 holds the world map open |
| `TroubleEnvironmentVariable` | `"CAUCASIAN_EAGLE_TROUBLE"` | Testing, with the pilot: =N stands her before trouble N and holds the pilot still; =-N stands her 30 m off the road beside it |
| `RoamEnvironmentVariable` | `"CAUCASIAN_EAGLE_ROAM"` | Testing: =N makes the test pilot weave up to N m off the road |
| `PauseEnvironmentVariable` | `"CAUCASIAN_EAGLE_PAUSE"` | Testing: =1 holds her menu open |
| `GuideEnvironmentVariable` | `"CAUCASIAN_EAGLE_GUIDE"` | Testing: =N holds the guide open on page N (1 to 7); without DEMO, over the start screen |
| `GrowthEnvironmentVariable` | `"CAUCASIAN_EAGLE_GROWTH"` | Testing: =1 holds Nyxara's window open with sample crystals and levels; =2 its second page with sample items |
| `CityDrawDistance` | 450 | m, buildings are drawn this far |
| `CityTint`, `CityEmission` | 0.78, 0.16 | Building colour scale and self-light |
| `MinimapRange` | 110 | m from Nyxara to the rim of the minimap |
| `WinePotionHeal` | 10 | Telavi: extra health from every potion |
| `SpringsHealScale` | 2 | Tbilisi: wave and arena healing, times this |
| `BlessingPotionHeal` | 15 | Mtskheta: extra health per potion |
| `StoneDamageScale` | 0.8 | Gori: damage taken, times this |
| `FleeceGoldScale` | 2 | Kutaisi: chest gold, times this |
| `SeaWindFeathers` | 3 | Batumi: Eagle Feathers given |
| `SvanSteelDamage` | 1.25 | Mestia: damage dealt, times this |
| `JournalKeyCount` | 12 | Keys `UpdateJournal` watches (F1, paging, scrolling, M) |

Creature skeletons: hip line, neck line and arm distance per model are in the `MODELS` table of `art/monsters/rig_models.py`; band count 50 and run gap 0.035 of the height in the same file. Joint angles of every movement are in `BuildCreaturePalette` (`CreatureRig.cpp`), next to the code that uses them.

Growth (`GameConstants.h`; rules in `Growth.cpp`):

| Name | Value | Meaning |
|---|---|---|
| `BossCrystals`, `ChestCrystals` | 60, 5 | Crystals from a guardian; in a chest (x 3 feather chest, x 8 king's hoard). An ordinary creature leaves its cost |
| `SaveCrystalsPerArena` | 20 | Given per arena won to a save from before crystals |
| `MaxItemTier`, `ItemTierCost` | 3; 40, 140, 320 | Evolutions of wings, axes, armour and their prices |
| `MaxTrainingLevel`, `TrainingCostBase`, `TrainingCostStep` | 8, 10, 10 | Power and defence levels; level n costs 10 + 10 n |
| `PowerStep`, `DefenceStep` | 0.07, 0.05 | Per level: more damage dealt, less taken |
| `SwordTierDamage`, `ArmourTierTaken` | 0.20, 0.12 | Per evolution |
| `WingTierCooldown`, `WingTierBoost` | 0.12, 0.08 | Per evolution: skills sooner, boost faster |
| `FormDamage`, `FormTaken`, `FormScale` | 0.10, 0.08, 0.03 | Per form of Nyxara |
| `MaxPotions`, `PotionHeal`, `PotionKey` | 5, 35, Z | Potions |
| `ArenaCageMargin`, `WallStandOff` | 20 m, 1.4 m | The cage beyond the arena radius; wall beyond the cage end |
| `WallSightNear`, `WallSightFar` | 1.3 m, 3.2 m | Wall blocks sink within the first, stand full beyond the second, of the camera's line to her |
| `WallCameraNear` / `WallCameraFar` | 3.2 / 6.5 | m | A wall block nearer to the camera than the first is sunk; farther than the second it stands at full height | `GameConstants.h` | code | Added after a capture showed a block filling half the screen |

Arena walls (`GameConstants.h`): `WallHeight` 5.2 m, `WallBlockWidth` 1.7 m, `WallBlockDepth` 1.5 m, `WallRiseSeconds` 0.9, `WallSinkSeconds` 1.6. The walls are drawn only; what stops Nyxara is `Journey::Leash`.

City build values (`art/cities/build_cities.py`, tables at the top of the file): the `CITIES` table gives each city its centre along the road and its length (Telavi 443 m / 104 m, Tbilisi 1325 / 116, Mtskheta 2057 / 110, Gori 2983 / 116, Kutaisi 3738 / 116, Batumi 4561 / 150, Mestia 5775 / 100); a city may not overlap an arena. 40 models, 218 buildings. Map build values (`art/map/build_map.py`): picture 2048 x 1152, world Z -150..2076 across and X -626..626 down.

Art build values for the world (`art/world/build_world.py`, tables at the top of the file): road half width 6.5 m, arena radius 17 m, boss arena radius 24 m, path sample spacing 2 m, terrain grid 4 m, world bounds X -680..680 and Z -180..2010, mountain at (0, 1900), ledge 78 m high. Path length 6,346 m, 34 arenas, 81 chests, 664 props.

## Sound

| Name | Value | Unit | Purpose | Where |
|---|---|---|---|---|
| `SoundFolder` | `assets/sounds` | | Folder listed by `Sound::Initialize` | `GameConstants.h` |
| `MusicVolume` | 0.5 | × Music setting | Level of music | `GameConstants.h` |
| `MusicFadeSeconds` | 2.5 | s | One piece into the next | `GameConstants.h` |
| `WindVolume` | 0.45 | × Effects setting | Wind loop at full strength | `GameConstants.h` |
| `PlaceVolume` | 0.6 | × Effects setting | City voices, or fire of the walls | `GameConstants.h` |
| `FlightVolume` | 0.5 | × Effects setting | Air loop while she flies | `GameConstants.h` |
| `NarrationVolume` | 1 | × Effects setting | Storyteller | `GameConstants.h` |
| `NarrationDuck` | 0.4 | × | Music and world loops while the storyteller speaks | `GameConstants.h` |
| `StepLength` | 1.7 | m | Ground walked between two footsteps | `GameConstants.h` |
| `StepVolume` | 0.4 | × | Footsteps | `GameConstants.h` |
| `SoundRepeatSeconds` | 0.07 | s | The same sound name is not started again sooner | `GameConstants.h` |

One-shot volumes (0.5 to 1) are written at each `Sound::Play` call. None of these values has been tuned by ear.

### What she carries (`Collection`)

| Name | Value | Unit | Meaning | File |
|---|---|---|---|---|
| `CharmSet` | 3 | charms | Charms of one kind that make a set | `GameConstants.h` |
| `CharmChance` | 0.3 | chance | A creature drops its charm | `GameConstants.h` |
| `CharmSetDamage` | 0.15 | share | More damage to that kind with the set | `GameConstants.h` |
| `CharmSetTaken` | 0.15 | share | Less damage from that kind with the set | `GameConstants.h` |
| `KeepsakeDamage`, `KeepsakeTaken` | 0.02 | share per keepsake | More damage dealt, less taken | `GameConstants.h` |
| `FatherFeatherBoost` | 0.025 | share per feather | Stronger boost | `GameConstants.h` |
| `FatherFeatherReach` | 2.2 | m | She takes a feather this near | `GameConstants.h` |
| `FatherFeatherSight` | 110 | m | A feather's glow is drawn this far | `GameConstants.h` |
| `SongCrystals` | 1.25 | x | Crystals with all seven verses | `GameConstants.h` |
| `MaxRunes` | 3 | runes | Of one kind | `GameConstants.h` |
| `RuneChance` | 0.35 | chance | A tier 1 gold chest holds a rune (tier 2 always) | `GameConstants.h` |
| `FireRuneDamage` | 0.06 | share per rune | Her blows | `GameConstants.h` |
| `FrostRunePace` | 0.08 | share per rune | Longer between a creature's blows | `GameConstants.h` |
| `MoonRuneHeal` | 4 | health per rune | Per wave and per arena won | `GameConstants.h` |

### Nature

| Name | Value | Unit | Meaning | File |
|---|---|---|---|---|
| `NatureTint` | 0.5 | x | Darkens grass and trees under the engine's moonlight | `GameConstants.h` |
| `NatureMaxSlope` | 0.85 | rise / run | Steeper land is bare | `GameConstants.h` |
| `CameraLookScale` | 0.6 | multiplier | How fast the mouse and the right stick turn the camera, against the engine (1). The player's mouse speed multiplies it | `GameConstants.h` |
| `MountainClearZ` | 1790 | m | Nothing planted beyond, near the middle line | `GameConstants.h` |
| `GrassCell` | 20 | m | Side of one square of grass | `GameConstants.h` |
| `GrassDistance` | 80 | m | Grass is drawn this far | `GameConstants.h` |
| `GrassFromRoad` | 0.3 | m | Grass begins this far beyond the road's edge | `GameConstants.h` |
| `GrassCellsPerFrame` | 2 | squares | Built in one frame | `GameConstants.h` |
| `GrassCellLimit` | 160 | squares | Kept before old ones are dropped | `GameConstants.h` |
| `TreeCell` | 15 | m | At most one tree per square | `GameConstants.h` |
| `TreeDistance` | 240 | m | Trees are drawn this far | `GameConstants.h` |
| `TreeFromRoad`, `TreeToRoad` | 6, 150 | m | Band beyond the road's edge where trees stand | `GameConstants.h` |
| `BroadleafHeight`, `ConiferHeight` | 11, 15 | m | A middling tree | `GameConstants.h` |
| `CloudRadius` | 2600 | m | Cloud dome (inside the sky dome at 3000) | `GameConstants.h` |
| `CloudTextureScale`, `CloudFlatness` | 0.16, 0.14 | - | How the cloud picture is laid over the dome | `GameConstants.h` |
| `CloudCover`, `CloudBrightness`, `CloudEmission` | 0.62, 0.5, 1 | - | Opacity and light of the cloud | `GameConstants.h` |
| `CloudDriftU`, `CloudDriftV` | 0.0035, 0.0012 | picture widths / s | The wind | `GameConstants.h` |

The region table (grass density, blade height, colours, share of trees, fir or broadleaf) is at the top of `Nature.cpp`. None of these values has been tuned in play.

### Rest and creature tactics

| Name | Value | Unit | Meaning | File |
|---|---|---|---|---|
| `HeavyBlowChance` | 0.28 | share | Blows that are heavy | `GameConstants.h` |
| `HeavyBlowWindup`, `HeavyBlowDamage` | 1.7, 1.6 | x | Against a light blow | `GameConstants.h` |
| `HeavyBlowReach` | 1.6 | m | Beyond the creature's reach (light: 0.9) | `GameConstants.h` |
| `FollowUpChance`, `FollowUpWindup` | 0.35, 0.55 | chance, x | Second blow of quick creatures | `GameConstants.h` |
| `QuickWindup` | 0.56 | s | Shorter wind-up than this = quick | `GameConstants.h` |
| `SlamRange`, `SlamRadius` | 12, 3.2 | m | Giants' ground slam | `GameConstants.h` |
| `SlamSeconds`, `SlamEvery` | 1.1, 7 | s | Warning; pause between slams | `GameConstants.h` |
| `SlamDamage` | 1.3 | x | Of the giant's blow | `GameConstants.h` |
| `DodgeDistance`, `DodgeChance`, `DodgeEvery` | 4.5, 0.5, 2.5 | m, chance, s | Small creatures jumping aside | `GameConstants.h` |
| `HopSeconds`, `HopSpeed` | 0.28, 3 | s, x speed | A jump aside or back | `GameConstants.h` |
| `RoundGuardSeconds` | 2.5 | s | Running round her guard | `GameConstants.h` |
| `WoundedShare`, `WoundedKeep` | 0.3, 6.5 | share, m | Hit-and-run when badly hurt | `GameConstants.h` |
| `RangedLead` | 0.7 | 0..1 | How far ahead of her shots are aimed | `GameConstants.h` |
| `FanEvery`, `KajiNovaOrbs` | 3, 8 | volleys, orbs | Shot patterns | `GameConstants.h` |

None of these has been tuned by play.

## The Eagle's Ascent 0.5 (2026-11-20)

| Name | Value | Unit | What it does | Def | Kind | Note |
|---|---|---|---|---|---|---|
| `AscentFlightSeconds` | 10, 15, 20, 30 | s | How long her wings carried her at wing tier 0 to 3 (0.5 to 0.5.1; since 0.5.2 `AscentLevelFlightSeconds`) | `GameConstants.h` | code | The owner's numbers; `NyxaraKit::SetFlightSeconds` |
| Jar groups | every 30 to 50 | m | Along every road (way, caves, rings), 3 to 5 jars a group | `Wayside.cpp` (`JarStepLeast`, `JarStepMore`) | code | |
| Jar contents | 8 % a potion; crystals: each stretch's share (the map's `level` lines), one or two a jar | | What a jar holds, from a hash of its id | `Wayside.cpp` (`Lay`) | code | 0.5 to 0.5.1: 30 % a crystal or two, about 1,450; since 0.5.2 307 crystals (175 beside the way, 132 off it) and about 300 potions |
| `PlaceCrystals` | 8 | crystals | A place beside the road found (the road) | `GameConstants.h` | code | The Ascent's hidden places, which paid it too, were taken away on 2026-11-21 |
| `AscentBossCrystals` / a creature's `Cost` | 25 / 1 to 10 | crystals | What a beaten creature leaves on the mountain | `GameConstants.h`, `Journey.cpp` (`KindCrystals`) | code | 60 (`BossCrystals`, still the road's) until 0.5.2 |
| A potion over five | 1 | crystal | A potion found with five carried | `EaglesAscentGame::Collect` | code | |

## The Eagle's Ascent 0.5.3: her gears and quests (2026-11-22)

| Name | Value | Unit | What it sets | Where | Kind | Why |
|---|---|---|---|---|---|---|
| `GearFlightSeconds` | 10, 15, 22, 30 | s | Her flight in her own gear and in gears 1 to 3 | `GameConstants.h` | code | The owner's 10 s at first and 30 s at the end kept |
| `GearDamage` | 1, 1.6, 2.3, 3.2 | x | Her blows | `GameConstants.h` | code | The last as strong as 0.5.2's level 9 with her form |
| `GearTaken` | 1, 0.8, 0.6, 0.3 | x | Blows she takes | `GameConstants.h` | code | The same |
| `GearAttackPace` | 0.5 | x | Her attacks and skills in the game (the Character Lab's trial pace) | `GameConstants.h` | code | The owner: "effects and half speed" |
| `CrystalNests` | 3, 6 | nest | Where wind crystals end and fire end (ice beyond) | `GameConstants.h` | code | Ledger: wind 130, fire 297, ice 545 |
| Quests | 3, 6, 9; prices 38, 131, 182 crystals a set | | Her quests, a set a time | `AscentQuests.cpp` | code | A first set, to be chosen again with the owner |
| `CameraMaxPitch` | 1.05 | rad | Highest look-up (was 0.45) | `GameConstants.h` | code | The owner: "look up all up" |
| `CameraLookUpNear`, `CameraLookUpKeep`, `CameraLookUpClear`, `CameraLookUpSide` | 1.7, 0.2, 0.35, 0.6 | m, rad, m, m | The camera coming in close and low as she looks up | `GameConstants.h` | code | The owner: "it should bring camera to nyxara closer" |
| `CameraPivotSmoothing`, `CameraLiftUnder` | 0.12, 3.5 | s, m | Stairs glide; the camera rises over ground behind her | `GameConstants.h` | code | |

## The Eagle's Ascent 0.5.2: nine levels bought in the nests (2026-11-21; replaced by the gears in 0.5.3)

| Name | Value | Unit | What it does | Def | Kind | Note |
|---|---|---|---|---|---|---|
| `AscentMaxLevel` | 9 | levels | Of her wings, swords and armour each | `GameConstants.h` | code | The owner: "make the upgrade 9 times instead of 3" |
| `AscentDefaultPrices`, the map's `level` lines | 8, 14, 21, 27, 33, 39, 46, 52, 58 | crystals | A level 1 to 9 of any one thing | `GameConstants.h`, `EaglesAscent.creatures.txt` | data | The map's prices are read; these only stand in when it has none (`PRICES` in `ascent_creatures.py`) |
| `AscentLevelFlightSeconds` | 10, 12, 14, 17, 19, 21, 23, 26, 28, 30 | s | Her flight at wing level 0 to 9 | `GameConstants.h` | code | Ends at the owner's 30 s |
| `AscentSwordLevelDamage` | 0.165 | per level | Her blows: x (1 + 0.165 a level), x 2.49 at 9 | `GameConstants.h` | code | With her forms (`FormDamage` 0.10 each) |
| `AscentArmourLevelTaken` | 0.068 | per level | Blows taken: x (1 - 0.068 a level), x 0.39 at 9 | `GameConstants.h` | code | With her forms (`FormTaken` 0.08 each) |
| `WAY_SHARE` | 0.65 | share | Of a stretch's three levels' price, on the way itself | `ascent_creatures.py` | data | The owner: 60 to 70 %; the ledger finds 64 to 65 % |
| `AROUND_EVERY` | 150, 400 | m | Between two groups around the way | `ascent_creatures.py`, `EaglesAscent::PlaceAround` (`AroundGap`) | data, code | The owner's; the game allows 140 m where no ground is farther |
| `AroundReach`, `Out`, `Edge` | 120; 9 to 30; 4.5 | m | How far along the way a group's place may move; how far out from the way's middle it stands; where the walk out begins | `EaglesAscent.cpp` (`PlaceAround`) | code | |
| `AROUND_CLEAR`, `AROUND_BOSS` | 60, 100 | m | No group around the way that near a nest or a boss | `ascent_creatures.py`, `EaglesAscent.cpp` | data, code | |
| `OtherClear`, `WayMargin` | 26, 8 | m | A group around the way: no other group's place nor a peopled cave's mouth within 26 m of its place across the ground; from where its path leaves the way it is nearer than any other group by 8 m, and no group further up the way (or in a cave) is within 26 m of there | `EaglesAscent.cpp` (`PlaceAround`); checked by the crystals' ledger | code | So it is a fight of its own and wakes first (the creatures' tours, 2026-11-21); 26 m: more than the tallest creature sees, `MountainSight` and a Devi's 5.2 m. 40 m left 10 of 20 placed, 30 m 17; 26 m where its path leaves the way from every group, below it too, 17 of 19 |
| `CaveRoom` | 100 | m | A group around the way and a cave's group that are more than the three alive at once (`MountainMostAlive`) together: their places this far apart | `EaglesAscent.cpp` (`PlaceAround`); checked by the crystals' ledger | code | `MountainGiveWay` (70 m), more than the tallest creature sees and a group's spread: wherever she fights the one, the other is calm farther than 70 m from her and gives way. Nearer, the one awake first holds the three places and the other never wakes (the creatures' tour, 2026-11-21: around-4374, three, beside cave-14, two) |
| Grass squares | 12; thick to 34, light to 78 | m | Side of a square of grass; how far the thick field and its light layer reach | `AscentGrass.h` | code | |
| Grass density | 13 | places per m² | Tried where the ground is all meadow; the patches keep about half to nine tenths | `AscentGrass.h` (`Density`) | code | |
| Grass slope | 0.78 | rise per m | No grass steeper (the terrain shader draws rock there) | `AscentGrass.cpp` | code | |
| Her half width at a trunk | 0.42 / 0.6 | m | On foot / on wings; a trunk holds her from its foot to three quarters of its height | `EaglesAscent::HoldOffTrees` | code | |
| Cave wall margin on foot | 0.75 | m | How far from a cave's wall line she is held (her half width and the rough rock) | `EaglesAscent::HoldInCave` | code | In flight 0.9 (`KeepOutOfRock`) |
