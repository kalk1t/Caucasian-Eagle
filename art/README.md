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
| `moonlit-sanctuary/` | yes (21 MB) | The map: Blender scene, build scripts, export script, reference renders |
| `sky/make_sky_textures.py` | yes | Generates the night sky and moon textures in `game/assets/textures/caucasus` |
| `mountain/build_mountain.py` | yes | Blender script: the mountain mesh, its colour map and `Mountain.json` |
| `mountain/prepare_models.py` | yes | Prepares the Meshy models of Amirani, the eagle and the dog for the asset builder |
| `mountain/meshy/req/*.json` | yes | The requests sent to Meshy for the three figures |
| `mountain/meshy/out/` | **no** (about 110 MB) | Meshy downloads for the three figures |
| `mountain/export/` | only `Mountain.json` | Intermediate `.glb` and `.png` files; rebuilt by the scripts |
| `world/build_world.py` | yes | Blender script: the whole world of the journey (Moonlit Sanctuary plus the 6.3 km road to the mountain): terrain, road, bridges, gates, ruins, collision, generated ground textures with normal maps, and `Road.txt` (path, arenas, chests, props, lights) |
| `world/export/Road.txt` | yes | The data file the game reads; a copy is in `game/assets/Compiled/World` |
| `world/export/*.glb`, `world/export/textures/` | **no** (about 60 MB) | Intermediate export; rebuilt by the script |
| `monsters/meshy/make_requests.py`, `make_models.py`, `req/*.json`, `*_ids.txt` | yes | The Meshy requests for the ten creatures and the old gold chest: concept image first, then image-to-3D |
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
