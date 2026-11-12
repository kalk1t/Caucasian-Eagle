# Art sources

[← Project index](../README.md)

This folder holds what the game's assets are built from. The scripts are in git. The heavy binary sources are **not**: they stay on the workstation in the paths below, and git ignores them.

## What is where

| Path | In git | Content |
|---|---|---|
| `nyxara/meshy/meshy.py` | yes | Small Meshy API client. Reads the key from the `MESHY_API_KEY` environment variable; the key is never written to a file. |
| `nyxara/meshy/req/*.json` | yes | The requests sent to Meshy: concept prompts, image-to-3D, rigging, animation ids |
| `nyxara/meshy/out/` | **no** (about 980 MB) | Everything downloaded from Meshy: concept images, body, wing and sword meshes, the rig, animation clips |
| `nyxara/meshy/task_log.jsonl` | **no** | Task ids and download links |
| `nyxara/blender/build_base.py` | yes | Builds `Nyxara_base.blend` from the Meshy rig and clips |
| `nyxara/blender/assemble.py` | yes | Builds the game character: attaches and rigs the wings, fits the swords, authors the 31 clips, grades the textures, exports `Nyxara_game.glb` |
| `nyxara/blender/*.blend`, `*.glb` | **no** (about 1.1 GB) | Intermediate and final Blender files; rebuilt by the two scripts |
| `nyxara/tools/make_vfx_textures.py` | yes | Generates the six effect textures in `game/assets/Compiled/Effects/Nyxara` |
| `moonlit-sanctuary/` | yes (21 MB) | The map of version 0.1: Blender scene, build scripts, export script, reference renders. Not used by the game any more (the journey begins on the road) |
| `sky/make_sky_textures.py` | yes | Generates the night sky and moon textures in `game/assets/textures/caucasus` |
| `ui/build_hud.py` | yes | Generates the HUD artwork `game/assets/ui/Hud/HudAtlas.png` and the sprite table `game/src/HudAtlas.h` (Pillow and numpy; no outside images) |
| `cities/build_cities.py` | yes | Generates the seven cities: a 1024 px sheet of wall, roof and window tiles per city, 40 building models (compiled with the engine's asset builder into `game/assets/Compiled/Cities`), and `game/assets/Compiled/World/Cities.txt` (cities, models, buildings, lights). Pillow and numpy; no outside models or images. Run after `world/build_world.py`. |
| `map/build_map.py` | yes | Paints the world map `game/assets/ui/Map/WorldMap.png` and writes `Map.txt` from `Road.txt` and `Cities.txt`. Run after the two scripts above. |
| `mountain/build_mountain.py` | yes | Blender script: the mountain mesh, its colour map and `Mountain.json` |
| `mountain/prepare_models.py` | yes | Prepares the Meshy models of Amirani, the eagle and the dog for the asset builder |
| `mountain/meshy/req/*.json` | yes | The requests sent to Meshy for the three figures |
| `mountain/meshy/out/` | **no** (about 110 MB) | Meshy downloads for the three figures |
| `mountain/export/` | only `Mountain.json` | Intermediate `.glb` and `.png` files; rebuilt by the scripts |
| `world/build_world.py` | yes | Blender script: the whole world of the journey (the 6.3 km road to the mountain and the land round it; no source .blend is needed): terrain with rolling land, the old earth road (stone only at the first causeway and the last climb), earth banks over water, gates, ruins, collision, generated ground textures with normal maps, and `Road.txt` (path, arenas, chests, props, lights) |
| `world/build_nature.py` | yes | Plain Python: the height map of the land (`Terrain.bin`) and the grass, leaf, bark and cloud pictures the game's `Nature` module plants with. Run after `world/build_world.py` |
| `ui/build_cursor.py` | yes | Plain Python (Pillow): the mouse pointer `game/assets/ui/Cursor/Pointer.cur` (32, 48 and 64 px, hot spot at the tip) |
| `ui/build_icon.py` | yes | Plain Python (Pillow): the program icon `game/CaucasianEagle.ico` (16 to 256 px), the Eagle before the moon cut from `ui/export/CoverRender.png` |
| `world/export/Road.txt` | yes | The data file the game reads; a copy is in `game/assets/Compiled/World` |
| `world/export/*.glb`, `world/export/textures/` | **no** (about 60 MB) | Intermediate export; rebuilt by the script |
| `monsters/meshy/make_requests.py`, `make_models.py`, `req/*.json`, `*_ids.txt` | yes | The Meshy requests for the ten creatures and the old gold chest: concept image first, then image-to-3D |
| `refine/refine_assets.py` | yes | Blender, one model at a time with its own settings (table `ASSETS` at its top: triangle budget, hard-edge angle, fragment size, whether it stands on the ground). Welds the open seams Meshy leaves (every model arrives as thousands of loose faces), deletes stray fragments, makes normals consistent, smooths with hard edges kept, grounds scenery. Writes `art/refine/out/<Name>.glb` and before / after pictures. `monsters/prepare_models.py` uses the refined model when it exists. `inspect` only reports |
| `ui/render_cover.py` | yes | Blender (EEVEE), run with `blender -b -P art/ui/render_cover.py`: renders the 3D scene of the start picture from the game's own models (Nyxara in `FlightHover`, `Mountain.glb`, `Eagle.glb`, the moon texture) to `art/ui/export/CoverRender.png`. Run it before `build_start_screen.py` |
| `ui/build_start_screen.py` | yes | Uses `art/ui/export/CoverRender.png` when it exists (darkens the left for the menu, vignette, title); otherwise: Paints the picture behind the start screen (`game/assets/ui/Menu/StartScreen.png`, 1920 x 1080): night sky, moon, mountains, the Eagle, the title |
| `sounds/build_sounds.py` | yes | Synthesises the placeholder effects, loops and music into `game/assets/sounds` (63 WAV files) |
| `sounds/build_voice.py` | yes | Speaks the story lines of `game/src/Story.cpp` and Nyxara's lines with the Windows voices into `game/assets/sounds/voice` and `nyxara` (43 WAV files) |
| `monsters/rig_models.py` | yes | Gives the compiled creature models a skeleton: writes joint weights into each `.dmesh` and a `.rig` file with the joint pivots. Run after `prepare_models.py` |
| `monsters/prepare_models.py` | yes | Turns Meshy models into game models (`.dmesh` plus a 1024 px colour map) with the engine's asset builder |
| `monsters/meshy/out/` | **no** (about 460 MB) | Meshy downloads for the creatures and the chest |
| `props/meshy/` | scripts and requests yes, `out/` **no** (about 160 MB) | Meshy requests and downloads for the Svan tower, dead tree, fir and rock cluster |

## Back up the files that are not in git

The `meshy/out/` folders cannot be rebuilt without spending Meshy credits again (932 were used in all). Keep a copy of `art/nyxara/meshy/out`, `art/mountain/meshy/out`, `art/monsters/meshy/out`, `art/props/meshy/out` and `art/nyxara/blender/*.blend` on a second disk or a cloud drive. A fresh clone of this repository does not contain them, so the hero cannot be rebuilt from a clone alone; the compiled hero in `game/assets` is enough to build and run the game.

## Building assets

The commands are in [`techplan/03-workflows.md`](../techplan/03-workflows.md), sections 6.4 (hero), 6.5 (map), 6.8 (sky), 6.9 (mountain) and 6.10 (the road, creatures and props). In short:

1. Blender script produces a `.glb`.
2. The engine's asset builder compiles the `.glb` into `game/assets/Compiled/`.
3. `tools\build.ps1 -Run` stages and starts the game.

## Where the creatures come from

The creatures of the road are from Georgian and wider Caucasian folklore. Their looks were written as Meshy prompts from these descriptions; the game takes liberties with all of them.

| Creature | Folklore | Source used |
|---|---|---|
| Chinka | Small mischievous spirits of the Georgian woods | General knowledge of the folklore; no single source checked |
| Ali | Water and forest spirits that lure travellers, often as women with long hair | Articles on Georgian mythical creatures (budgetpixel.com, madloba.info) |
| Kudiani | Georgian witches, hunched, who ride to the gatherings of their queen | Same articles |
| Ochokochi | Forest goat-man with a blade-like growth on his chest | Wikipedia, "Ochokochi" |
| Almasty | The wild man of the Caucasus heights | Wikipedia, "Almas (folklore)" |
| Kaji | Spirit smiths and sorcerers; their land is Kajeti | Same articles; in this game they are the smiths who renew the chain **[proposed]** |
| Uaig | One-eyed giants of Ossetian Nart sagas | General knowledge; no single source checked |
| Devi | Many-headed ogres of Georgian tales | Same articles |
| Rokapi | Queen of the Kudiani, chained to a hill as punishment | General knowledge; no single source checked |
| Gveleshapi | The Georgian dragon-serpent of lakes and rivers | Same articles |

## Meshy budget

| Item | Value |
|---|---|
| Credits used for Nyxara | 230 |
| Credits used for Amirani, the eagle and the dog | 117 |
| Credits used for the ten creatures and the chest | 429 (11 concept images at 9, 11 models at 30) |
| Credits used for the four scenery props | 156 (4 concept images at 9, 4 models at 30) |
| Balance after the road was built (2026-11-10) | 19 |
