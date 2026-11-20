# 8. Data: assets and saved state

[← Index](TECHNICAL_DOCUMENTATION.md)

## 8.1 Game assets in git

Everything under `game/assets/` is ready-to-run output of the asset pipeline (77 files, about 183 MB in v0.1). It uses the same folder layout as the engine's `assets/Compiled`, so staging is a plain copy on top of the engine's assets.

| Path under `game/assets/Compiled/` | Content | Built from | Format |
|---|---|---|---|
| `Characters/Nyxara/EagleV1/Character.dmesh` | Hero mesh: body, two wings, four sword variants (89,230 triangles) | `art/nyxara/blender/assemble.py` | Engine skinned mesh |
| `Characters/Nyxara/EagleV1/Character.dskel` | Skeleton, 31 joints, rig signature `DoorwaysRig_v1_116d1b0e1935a8eb` | same | Engine skeleton |
| `Characters/Nyxara/EagleV1/Character.dmat` | Six materials: `NyxaraBody`, `NyxaraWings`, and four sword slots | same | Engine material table |
| `Characters/Nyxara/EagleV1/Character.meshbind` | Mesh-to-rig binding record | same | Text |
| `Characters/Nyxara/EagleV1/*.danim` | 31 animation clips (see below) | same | Engine animation |
| `Characters/Nyxara/EagleV1/Textures/*.png` | Base colour, normal, roughness, metallic and packed maps per material (163 MB) | same | PNG, 2048 px |
| `Characters/Nyxara/EagleV1/ImportReport.md/.csv` | Pipeline report | Asset builder | Text |
| `Effects/Nyxara/*.png` | Six effect textures: `SoulArc`, `Glow`, `Ring`, `Feather`, `Beam`, `MoonSigil` | `art/nyxara/tools/make_vfx_textures.py` | PNG |
| `Scenes/RoadToAmirani.dscene` | Scene description for the world (the road and the land) | `art/world/build_world.py` + scene pipeline | Engine scene |
| `StaticMeshes/RoadToAmirani/` | World geometry (about 291,000 triangles, 39 MB) | same | Engine static mesh |
| `Materials/RoadToAmirani/` | World materials: 25 textured materials | same | Engine material table |
| `../sounds/` (`game/assets/sounds`) | 106 WAV files in `nyxara`, `creatures`, `combat`, `world`, `ui`, `music`, `voice` (39 MB). Listed in its `README.md` | `art/sounds/build_sounds.py`, `art/sounds/build_voice.py` | WAV, PCM 16-bit |
| `Textures/RoadToAmirani/` | Generated 512 px ground, earth-road, cobble and plank textures with normal maps (31 MB) | same | PNG |
| `Collision/RoadToAmirani/` | World collision: 133 walkable road chunks, the ledge, and 748 pieces of land | same | Engine collision |
| `World/Road.txt` | Path samples, arenas, chests, props and lights (see 8.1a) | `art/world/build_world.py` | Text |
| `Monsters/<Name>/<Name>.dmesh`, `<Name>BaseColor.png` | Ten creatures and the old gold chest (Meshy models, 1024 px colour maps, 42 MB) | `art/monsters/prepare_models.py`; joint weights added by `art/monsters/rig_models.py` | Engine static mesh with joint weights, PNG |
| `Monsters/<Name>.rig`, `Landmarks/AmiraniMountain/Eagle.rig` | Creature skeleton: `TYPE biped|robed|serpent|winged`, then `J joint parent x y z` (pivot in mesh units) for 19 joints | `art/monsters/rig_models.py` | Text |
| `Props/<Name>/...` | Svan tower, dead tree, fir, rock cluster (12 MB) | same script | same |

### 8.1a `Road.txt`

One record per line, fields separated by spaces. Coordinates are engine world metres.

| Record | Fields | Meaning |
|---|---|---|
| `STEP d` | spacing | Distance between path samples (2 m) |
| `S i name` | index, name with `_` for spaces | A region |
| `P x y z w stage` | position, half width, region | One path sample; the road is the chain of these |
| `A stage s kind r x y z` | region, distance along the road, kind (0 ambush, 1 boss, 2 the Eagle's terrace, 3 an open clearing: no fight, the stones and chest only), radius, centre | An arena |
| `C x y z yaw tier` | position, facing, tier (0 roadside, 1 arena, 2 boss hoard) | A chest |
| `T type x y z yaw height` | `deadtree`, `fir`, `tower` or `rocks` | A scenery prop drawn by the game |
| `L x y z r g b size` | position, colour, size | A glow (lantern, crystal, wisp, lava) |

The game refuses to start if the file is missing or has no path [confirmed: `Journey::Initialize`].

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

The mesh carries each sword twice, once per hand: `Slot_sword_right__Moonfeather`, `Slot_sword_left__Talonshade`, `Slot_sword_left__Moonfeather`, `Slot_sword_right__Talonshade`. The kit shows two of the four at a time; G toggles which pair [confirmed: `NyxaraKit::ApplySwordVisibility`].

The bow and the carried weapons are four more materials on the same mesh: `Slot_bow_hand` (bound to her left hand), `Slot_bow_back`, `Slot_axe_back__Moonfeather` and `Slot_axe_back__Talonshade` (bound to her chest). With the bow out (`mBow`) the kit shows `Slot_bow_hand` and the two `Slot_axe_back__*` and hides all four hand axes; otherwise it shows `Slot_bow_back` and one pair of hand axes. The clip `BowShot` (0.2 s, 7 frames) is posed in `assemble.py` (`bow_frames`: a two-bone solve for each arm), not taken from a source clip.

**The screen shows her mirrored.** glTF is right-handed and the engine is left-handed, and neither the asset builder nor the engine turns the model over, so the rig's `RightHand` is the hand the player sees as her left (found 2026-11-12: the bow, bound to `LeftHand`, showed in her right). The game's words follow the screen: the bow is bound to `RightHand` (`BOW_GAME_HAND`) and its pose is turned over with `mirror_pose`; Inferno "in her right hand" is `Slot_sword_left__Moonfeather`; RIGHT TALON is the clip made for the rig's left hand.

`Slot_bow_string` is the bow's string (taken off the hand bow in `make_bow`): shown with the bow out, hidden while she has it up (`mBowPosing`), when `NyxaraKit::Draw` draws the string bent to her hand and an arrow of light on it. The places come from `game/src/BowPoints.h`, which `assemble.py` (`bow_frames`) writes each time she is built: for each of the clip's 7 frames her string hand, the bow's grip and the string's two ends, in her own frame (x right, y up, z ahead). The pose is built on the Idle stance (hips over the root) from her posed chin (`headfront`) and bow shoulder; the wings are keyed lowered and swept back (`BOW_WINGS`). Build her before the game when the pose changes: the header and the clip must be from the same run.

### Mountain of Amirani and night sky

| Path under `game/assets/` | Content | Built from |
|---|---|---|
| `Compiled/Landmarks/AmiraniMountain/Mountain.dmesh`, `MountainBaseColor.png` | The peak with the cave ledge (135,200 triangles) and its 2048 px colour map | `art/mountain/build_mountain.py` |
| `Compiled/Landmarks/AmiraniMountain/Amirani.dmesh`, `Eagle.dmesh`, `Dog.dmesh` and `<Name>BaseColor.png` | The three figures (Meshy models) and 1024 px colour maps | `art/mountain/prepare_models.py` |
| `textures/caucasus/NightSky.jpg` | 4096 x 2048 painted sky: stars, Milky Way, aurora, cloud, three ridge lines; wraps horizontally | `art/sky/make_sky_textures.py` |
| `textures/caucasus/Moon.jpg` | 2048 x 1024 moon surface. Not loaded by the game (no moon is drawn); read by `art/ui/render_cover.py` for the start picture | same |

These meshes have no `.dmat`: the game loads the colour map next to each mesh and builds the material in code.

### Land and plants (`Nature`)

| File | Content |
|---|---|
| `game/assets/Compiled/World/Terrain.bin` | `"CETR"`, int32 columns (341) and rows (548), float32 x and z of the first sample, float32 grid (4 m), then for every sample row by row: float32 height, then one byte each metres beyond the road's edge (0..255), then one byte each region (0..11) |
| `game/assets/textures/nature/Grass.png`, `Leaves.png` | 1024 x 256, four cut-out pictures side by side, with alpha |
| `game/assets/textures/nature/Bark.png` | Tiling bark |
| `game/assets/textures/nature/Clouds.png` | 1024 x 1024 tiling cloud with alpha |

All four pictures and `Terrain.bin` are written by `art/world/build_nature.py`. `Terrain.bin` repeats the terrain formula of `build_world.py`, so it must be rebuilt whenever the world is.

#### The Eagle's Ascent's map files (`game/assets/Maps/`)

`tools/map-to-game.ps1` copies them from the Map Lab's last build: `EaglesAscent.txt` (the map's records), `.heights` (the land's grids), `.splat_a.dds` and `.splat_b.dds` (the ground's look; `AscentGrass` reads them too), `.trees` (0.5: every tree's trunk, `x y z radius height`, 261,650 lines, 8.9 MB; `build_mountain.py` writes it from the forest's plants, `TREES_ONLY=1` for the trunks alone) and `EaglesAscent.creatures.txt` (written by `art/map/ascent_creatures.py`).

## 8.1b `Places.txt`

`game/assets/Compiled/World/Places.txt`, written by `art/places/build_places.py`; the models it names are in `game/assets/Compiled/Places` (50 `.dmesh` files with a colour map each, about 149 MB). Engine coordinates, metres.

| Row | Fields | Meaning |
|---|---|---|
| `M index model roofs` | number, path under `Compiled`, 1 when she may stand on it | A model (50) |
| `B model x y z yaw height far` | model number, position, turn, height in metres, distance it is drawn from | One standing piece (313) |
| `O x z r` / `o x z r` | centre and radius | A blocker with a top she may stand on (36) / without (333) |
| `C x z r` | centre and radius | A circle kept clear of trees and grass (22) |
| `L x y z r g b size` | position, colour, size | A light (33) |
| `SITE index s x y z radius` | number, road distance, centre, radius | A named place (26); the name is `Story::Places[index]` |

## 8.2 Source art not in git

| Path | Content | Size | Why it is outside git |
|---|---|---|---|
| `art/nyxara/meshy/out/` | Meshy downloads: concept images, body, wing and sword meshes, the rig, animation clips | about 980 MB | Size; regenerating costs Meshy credits, so **back this folder up** |
| `art/nyxara/blender/Nyxara_base.blend`, `Nyxara_game.blend`, `Nyxara_game.glb` | Intermediate and final Blender files | about 1.1 GB | Size; rebuilt by the scripts from `meshy/out` |
| `art/mountain/meshy/out/` | Meshy downloads for Amirani, the eagle and the dog: concept images, models, textures | about 110 MB | Size; regenerating costs Meshy credits, so **back this folder up** |
| `art/mountain/export/*.glb`, `*.png` | Intermediate files for the asset builder | about 50 MB | Rebuilt by the two mountain scripts |
| `art/monsters/meshy/out/`, `art/props/meshy/out/` | Meshy downloads for the creatures, the chest and the scenery props | about 620 MB | Size; regenerating costs Meshy credits, so **back these folders up** |
| `art/world/export/RoadToAmirani.glb`, `textures/` | Intermediate world export | about 60 MB | Rebuilt by `build_world.py` |

The scripts that turn those files into game assets are in git ([art/README.md](../art/README.md)). Moonlit Sanctuary's source (21 MB) is small enough to be fully in git.

## 8.3 Engine assets the game uses at run time

Copied from the engine submodule by `tools/stage.ps1`: `assets/shaders`, `assets/ui`, `assets/textures`, `assets/models` and `assets/Compiled` (about 550 MB). Most of `assets/Compiled/Textures` (399 MB) belongs to the engine's own demo scenes and is probably not needed by the game [inferred; which files the engine requires at start-up has not been mapped. See D1 in [10-tech-debt-risks.md](10-tech-debt-risks.md)].

## 8.4 Saved state

**`eagles-ascent.sav`** (the Eagle's Ascent, 0.5), written only when she saves with H in a nest: `#` comment lines, then `nest <station>`, `crystals <n>`, `potions <n>`, `level <row> <n>` for the five rows (wings, swords, armour, power, defence), and one line per id: `beaten <creature>`, `jar <jar>`, `place <place>` (ids: `Wayside`, the placing model). Unknown words are skipped; a save from before 0.5 (nest and beaten only) loads with no growth. Unattended runs never touch a player's save (the tours write `tour.sav`, the jars' test `ways.sav`, beside their evidence).

Under `%LOCALAPPDATA%\CaucasianEagle\` the engine stores its files and the game stores one:

| File | Content |
|---|---|
| `PlayerPreferences.ini` | Graphics, audio, controls and interface settings |
| `CharacterSnapshot.txt` | Engine character progression (level, experience, equipment) |
| `CharacterSnapshot.lock` | Prevents two running copies from sharing one save |
| `journey.sav` | The game's own save: one text line with a format number, arenas won, gold, Eagle Feathers, memories found, deaths, whether the journey is complete, seconds played, one character per chest (opened or not), and on a last line a number with one bit per city entered (absent in saves made before the cities; such a save counts every city behind the last arena won as entered) |

`journey.sav` is written after every arena won, every chest opened, every purchase in Nyxara's window and every potion drunk, and read at start. Its last line is her growth: crystals, potions, then the five levels (wings, swords, armour, power, defence). A launch continues at the last arena won. It is not written in unattended runs. Skill cooldowns, health and the exact position are not saved. A file that does not parse is ignored and the journey starts from the beginning [inferred from `Journey::Load`; not tested with a damaged file].

The line after her growth in `journey.sav` is what she carries: eight charm counts, links, the feather bits, three rune counts and the rune set in her blades (-1 none). Saves from before it load with nothing carried.

After that come two lines: the places she has found (one number, a bit per place) and the traps she has beaten (one number, a bit per trap in `TrapTable` order). Both are absent in older saves and then read as none. The format number is 2 since nine arenas became open clearings: a save of format 1 has its arena count converted on reading (the old count walks the `A` rows, clearings included, and the new count is the fights among them).

The last line is the save point: `x y z`, where she was when the game was last saved outside a fight (`Journey::CanMark`). It is absent in older saves, and then she wakes at the last arena won. `Journey::WakeAtSavePoint` uses x and z only: within the road's width she stands at the road's height, beyond it on the land or roof there.

What calls `Journey::Save`: a fight won, a chest opened, a place found, a trap beaten, a charm, rune or feather found, a city's gift, a purchase, a rune set or a potion drunk, the road completed, a new journey (F9), leaving through her menu, and F8.
