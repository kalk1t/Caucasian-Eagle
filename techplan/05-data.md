# 8. Data: assets and saved state

[← Index](TECHNICAL_DOCUMENTATION.md)

## 8.1 Game assets in git

Everything under `game/assets/` is ready-to-run output of the asset pipeline (77 files, about 183 MB in v0.1). It uses the same folder layout as the engine's `assets/Compiled`, so staging is a plain copy on top of the engine's assets.

| Path under `game/assets/Compiled/` | Content | Built from | Format |
|---|---|---|---|
| `Characters/Nyxara/EagleV1/Character.dmesh` | Hero mesh: body, two wings, four sword variants (89,230 triangles) | `art/nyxara/blender/assemble.py` | Engine skinned mesh |
| `Characters/Nyxara/EagleV1/Character.dskel` | Skeleton, 31 joints, rig signature `DoorwaysRig_v1_74bb2d2053cd36e2` | same | Engine skeleton |
| `Characters/Nyxara/EagleV1/Character.dmat` | Six materials: `NyxaraBody`, `NyxaraWings`, and four sword slots | same | Engine material table |
| `Characters/Nyxara/EagleV1/Character.meshbind` | Mesh-to-rig binding record | same | Text |
| `Characters/Nyxara/EagleV1/*.danim` | 31 animation clips (see below) | same | Engine animation |
| `Characters/Nyxara/EagleV1/Textures/*.png` | Base colour, normal, roughness, metallic and packed maps per material (163 MB) | same | PNG, 2048 px |
| `Characters/Nyxara/EagleV1/ImportReport.md/.csv` | Pipeline report | Asset builder | Text |
| `Effects/Nyxara/*.png` | Six effect textures: `SoulArc`, `Glow`, `Ring`, `Feather`, `Beam`, `MoonSigil` | `art/nyxara/tools/make_vfx_textures.py` | PNG |
| `Scenes/MoonlitSanctuary.dscene` | Scene description for the map | `art/moonlit-sanctuary/Export/export_game_glb.py` + scene pipeline | Engine scene |
| `StaticMeshes/MoonlitSanctuary/MoonlitSanctuary.dmesh` | Map geometry | same | Engine static mesh |
| `Materials/MoonlitSanctuary/MoonlitSanctuary.dmat` | Map materials (dark-green palette colours, no textures) | same | Engine material table |
| `Collision/MoonlitSanctuary/MoonlitSanctuary.dcoll` | Map collision | same | Engine collision |

### The 31 hero clips

| Group | Clips |
|---|---|
| Locomotion | `Idle`, `Walk`, `Run`, `WalkBackward`, `StrafeLeft`, `StrafeRight` |
| Jump | `Jump`, `JumpAirborne`, `JumpLanding` |
| Flight | `FlightTakeoff`, `FlightHover`, `FlightTravel`, `FlightLanding` |
| Sword attacks | `BasicAttack` (left hand), `SlashLeft` (right hand), `HeavyAttack` |
| Skills | `Skill1`, `Skill2`, `Skill3` |
| Combos | `Combo1L`, `Combo1R`, `Combo2L`, `Combo2R`, `Combo3L`, `Combo3R` |
| Defence and utility | `Guard`, `SwordSwap`, `Dodge` |
| Reactions | `HitReact`, `Death`, `Respawn` |

The clip names are a contract between three places: the pipeline command line ([03-workflows.md](03-workflows.md)), the clip list in `Game::Prepare`, and the `Acts` table in `NyxaraKit.cpp`. `Dodge` is authored and loaded but no input triggers it in v0.1 [confirmed: no `"Dodge"` in `NyxaraKit.cpp`].

### Sword materials

The mesh carries each sword twice, once per hand: `Slot_sword_right__Moonfeather`, `Slot_sword_left__Talonshade`, `Slot_sword_left__Moonfeather`, `Slot_sword_right__Talonshade`. The kit shows two of the four at a time; Q toggles which pair [confirmed: `NyxaraKit::ApplySwordVisibility`].

### Mountain of Amirani and night sky

| Path under `game/assets/` | Content | Built from |
|---|---|---|
| `Compiled/Landmarks/AmiraniMountain/Mountain.dmesh`, `MountainBaseColor.png` | The peak with the cave ledge (135,200 triangles) and its 2048 px colour map | `art/mountain/build_mountain.py` |
| `Compiled/Landmarks/AmiraniMountain/Amirani.dmesh`, `Eagle.dmesh`, `Dog.dmesh` and `<Name>BaseColor.png` | The three figures (Meshy models) and 1024 px colour maps | `art/mountain/prepare_models.py` |
| `textures/caucasus/NightSky.jpg` | 4096 x 2048 painted sky: stars, Milky Way, aurora, cloud, three ridge lines; wraps horizontally | `art/sky/make_sky_textures.py` |
| `textures/caucasus/Moon.jpg` | 2048 x 1024 moon surface | same |

These meshes have no `.dmat`: the game loads the colour map next to each mesh and builds the material in code.

## 8.2 Source art not in git

| Path | Content | Size | Why it is outside git |
|---|---|---|---|
| `art/nyxara/meshy/out/` | Meshy downloads: concept images, body, wing and sword meshes, the rig, animation clips | about 980 MB | Size; regenerating costs Meshy credits, so **back this folder up** |
| `art/nyxara/blender/Nyxara_base.blend`, `Nyxara_game.blend`, `Nyxara_game.glb` | Intermediate and final Blender files | about 1.1 GB | Size; rebuilt by the scripts from `meshy/out` |
| `art/mountain/meshy/out/` | Meshy downloads for Amirani, the eagle and the dog: concept images, models, textures | about 110 MB | Size; regenerating costs Meshy credits, so **back this folder up** |
| `art/mountain/export/*.glb`, `*.png` | Intermediate files for the asset builder | about 50 MB | Rebuilt by the two mountain scripts |

The scripts that turn those files into game assets are in git ([art/README.md](../art/README.md)). Moonlit Sanctuary's source (21 MB) is small enough to be fully in git.

## 8.3 Engine assets the game uses at run time

Copied from the engine submodule by `tools/stage.ps1`: `assets/shaders`, `assets/ui`, `assets/textures`, `assets/models` and `assets/Compiled` (about 550 MB). Most of `assets/Compiled/Textures` (399 MB) belongs to the engine's own demo scenes and is probably not needed by the game [inferred; which files the engine requires at start-up has not been mapped. See D1 in [10-tech-debt-risks.md](10-tech-debt-risks.md)].

## 8.4 Saved state

The game defines no save data of its own in v0.1. The engine stores, under `%LOCALAPPDATA%\CaucasianEagle\`:

| File | Content |
|---|---|
| `PlayerPreferences.ini` | Graphics, audio, controls and interface settings |
| `CharacterSnapshot.txt` | Engine character progression (level, experience, equipment) |
| `CharacterSnapshot.lock` | Prevents two running copies from sharing one save |

Skill cooldowns, enemy state and the player's position are not saved; every launch starts fresh in Moonlit Sanctuary.
