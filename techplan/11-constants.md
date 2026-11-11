# Constants and important values

[← Index](TECHNICAL_DOCUMENTATION.md)

**How to read this table**

- **Def** is where the value is defined, as `file:line` at v0.1.0. Paths are under `game/src/` unless they start with `art/` or `engine/`.
- **Config?** says how the value changes: "code" means a constant in source (edit, rebuild); "art" means a value in an art script (rerun the script and the asset pipeline); "player" means a setting the player changes in the game.
- All values are [confirmed] by reading the cited line on 2026-11-10. Line numbers drift; the symbol name is the stable reference.

## A. Identity and content

| Name | Value | Purpose | Def | Config? | Limits / dependencies |
|---|---|---|---|---|---|
| `GameTitle` | `"Caucasian Eagle"` | Window title | `GameConstants.h:9` | code | Also written in the installer script (`tools/installer/CaucasianEagle.iss`) |
| `GameVersion` | `"0.1.0"` | Version | `GameConstants.h:10` | code | Read by `tools/package.ps1`; must match the git tag and the changelog |
| `UserDataFolder` | `"CaucasianEagle"` | Folder under `%LOCALAPPDATA%` for settings and saves | `GameConstants.h:11` | code | Changing it orphans existing players' settings and saves |
| `StartSceneId` | `"MoonlitSanctuary"` | Scene loaded at start | `GameConstants.h:14` | code | `assets/Compiled/Scenes/<id>.dscene` must exist in the stage |
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
| `CameraPitch` | −0.60 | rad | Starting view pitch (about 34° down) | `GameConstants.h:25` | code | Within `CameraMinPitch`..`CameraMaxPitch` |
| `CameraDistance` | 6.5 | m | Follow distance | `GameConstants.h:26` | code | Engine pulls the camera in when blocked |
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

## E. Enemies (shades)

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

## G. Art build values (`art/nyxara/blender/assemble.py`)

| Name | Value | Purpose | Def | Config? |
|---|---|---|---|---|
| `FPS` | 30 | Clip frame rate | `assemble.py:10` | art |
| `WING_LEN` | 1.30 m | Length of one wing | `assemble.py:67` | art |
| `RAISE`, `RAISE_SWEEP`, `BEND2`, `BEND3` | 6°, 14°, 8°, 10° | Resting wing pose: open at shoulder height, slight sweep, drooping tips | `assemble.py:211` | art |
| `CHIN_NECK`, `CHIN_HEAD` | −12°, −23° | Raises the head in standing and attack clips | `assemble.py:303` | art |
| `GRADE` | body (gamma 0.74, gain 1.12, saturation 1.15); wings (0.70, 1.18, 1.25) | Brightens the textures so she reads on the dark map | `assemble.py:501` | art |
| Sword lengths | Moonfeather 1.02 m, Talonshade 0.92 m | | `assemble.py` `make_sword` calls | art |
