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

## Back up the files that are not in git

`nyxara/meshy/out/` cannot be rebuilt without spending Meshy credits again (230 were used). Keep a copy of `art/nyxara/meshy/out` and `art/nyxara/blender/*.blend` on a second disk or a cloud drive. A fresh clone of this repository does not contain them, so the hero cannot be rebuilt from a clone alone; the compiled hero in `game/assets` is enough to build and run the game.

## Building assets

The commands are in [`techplan/03-workflows.md`](../techplan/03-workflows.md), sections 6.4 (hero) and 6.5 (map). In short:

1. Blender script produces a `.glb`.
2. The engine's asset builder compiles the `.glb` into `game/assets/Compiled/`.
3. `tools\build.ps1 -Run` stages and starts the game.

## Meshy budget

| Item | Value |
|---|---|
| Credits used for Nyxara | 230 |
| Limit set by the author | 900 |
