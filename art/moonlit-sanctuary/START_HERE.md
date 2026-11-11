# Moonlit Sanctuary project folder

This folder brings together the chosen Moonlit Sanctuary visual reference, the first editable 3D map, individual prototype assets and the plan for reaching the reference's level of detail. The art target is a large moonlit sanctuary with a circular fighting area and room to explore.

## 1. Main visual reference

- [Chosen expanded Moonlit Sanctuary image](References/Moonlit_Sanctuary_Main_Reference.png)
- [Original smaller concept](References/Moonlit_Sanctuary_Original_Concept.png)

The expanded image is concept art: exact dimensions, building details and traversable paths still need to be designed. Nyxara in it is an interpretation of her written design, not the verified finished character model.

## 2. Current map

- [Open the editable Blender map](Moonlit_Sanctuary_v01.blend)
- [Overview render](sanctuary-overview.png)
- [Top-down render](sanctuary-top.png)
- [Temple approach render](sanctuary-temple-view.png)
- [How to open, navigate and edit](README.md)

The current map is an early 3D prototype. It is deliberately much simpler than the reference, and is not a playable Doorways level. The small human-shaped figure is a scale placeholder, not Nyxara.

## 3. Separate editable assets

Each item below is its own `.blend` file. A file may contain several separate mesh pieces forming that one asset. These are prototype assets based on the current model, not final reference-quality models. Standalone materials may differ from the assembled scene's preview materials. Editing a standalone file does not automatically update the assembled map.

### Architecture

- [Main temple facade](Assets/Architecture/MS_Temple.blend)
- [Gothic arch](Assets/Architecture/MS_Gothic_Arch.blend)
- [Stone bench](Assets/Architecture/MS_Stone_Bench.blend)
- [Broken column](Assets/Architecture/MS_Broken_Column.blend)

### Nature and graves

- [Dead tree 1](Assets/Nature/Dead_Tree_01_Prototype.blend)
- [Dead tree 2](Assets/Nature/Dead_Tree_02_Prototype.blend)
- [Grave marker](Assets/Nature/Grave_Marker_Prototype.blend)

### Map components

- [Central arena floor](Assets/Map%20Components/arena_floor.blend)
- [Outer ring floor](Assets/Map%20Components/outer_ring_floor.blend)
- [Radial bridge assembly](Assets/Map%20Components/radial_bridge.blend)
- [Bridge parapet](Assets/Map%20Components/bridge_parapet.blend)
- [Bridge merlon](Assets/Map%20Components/bridge_merlon.blend)
- [Main district platform](Assets/Map%20Components/district_platform.blend)
- [Smaller outlook platform](Assets/Map%20Components/outlook_platform.blend)
- [Wall section](Assets/Map%20Components/wall_section.blend)
- [Paving slab](Assets/Map%20Components/paving_slab.blend)
- [Soul lamp](Assets/Map%20Components/soul_lamp.blend)
- [Soul flame shape](Assets/Map%20Components/soul_flame.blend)
- [Torch and wall bracket](Assets/Map%20Components/torch_sconce.blend)
- [Arena seal](Assets/Map%20Components/arena_seal.blend)
- [Floating foundation](Assets/Map%20Components/floating_foundation.blend)
- [Human scale figure](Assets/Map%20Components/scale_figure.blend)

There are 22 separate Blender asset files. The lamp and torch files contain visual meshes and materials; they do not provide actual game lighting or effects.

## 4. Plan to achieve the reference

[Open the visual production plan](VISUAL_PRODUCTION_PLAN.md). It identifies the gaps between the current prototype and the image, a prioritized list of missing assets, and review stages for the layout, detailed temple sector, full environment, and game integration.

The biggest outstanding visual work is the large Gothic temple silhouette and interior, varied broken masonry and bridges, statues, tombs and gardens, distant structures, moon, fog, and finished materials. Nyxara, monsters, animation, interactions, collision and game integration remain separate work. None is implied to be complete by this package.

## 5. Rebuilding and preserving edits

The Python scripts in this folder generate the current prototype map and component files. Re-running them overwrites the generated `.blend` files and previews in their target folders, so save hand-edited versions under new names before rebuilding. The existing map is not automatically updated when standalone assets are edited; updating the assembly requires a deliberate change in Blender or in the build scripts.
