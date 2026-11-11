# Moonlit Sanctuary — Blender environment, version 01

This is a first editable 3D environment based on the selected Moonlit Sanctuary concept. It is a layout and visual prototype, not a finished game level. Open `Moonlit_Sanctuary_v01.blend` in Blender 5.2.3 LTS or later. `sanctuary-overview.png`, `sanctuary-top.png` and `sanctuary-temple-view.png` are renders of the model, not generated concept art.

## Exploring the file

1. Open the `.blend` in Blender. Orbit with the middle mouse button, pan with Shift + middle mouse, and zoom with the mouse wheel.
2. Press the Home key to frame the scene, or select an object in the Outliner and press numpad period to frame it. Use numpad 7 for top view and numpad 5 to switch orthographic/perspective.
3. The central disc is the combat space. Four broad stone bridges lead to a connected outer circular path. The main temple lies to the north; the west has a smaller crypt, the east a grave garden, and the south an entrance. Four smaller outlooks sit between them.
4. Dark trees, benches, soul lamps, broken columns, walls and arches are editable geometry. One simplified human-sized figure marks scale; it is not the Nyxara character asset.

## Scale and editing

Units are metres with Z up. The arena radius is about 25 m; the outer ring runs from radius 59 m to 77 m; its four main plazas reach roughly 93 m from the center. These measurements are provisional and not balanced or validated for Doorways. Select and move any object in Blender; the multiple stones and decorative objects remain separate and can be adjusted. Save a new version before substantial edits.

The three Python files are the editable source for the generated model. `architecture.py` creates modular Gothic architectural pieces, `nature.py` creates trees and graves, and `build_sanctuary.py` assembles the scene and renders previews. Rebuilding from the scripts recreates the generated `.blend` and renders; manual changes made only in the `.blend` are not included in the script and would be overwritten if it is run in this folder. Save a separate copy of any hand-edited version.

## What still needs work

- The model is an early prototype, much simpler than the detailed painted concept. It needs richer temple interiors, stone textures and damage, varied locations, atmosphere and further artistic work.
- Paths and floors overlap visually; real player movement, actual reachability, collision, navigation, flight and fall handling have not been tested. Some decorative models are placeholders.
- Nyxara, monsters, combat, encounters, doors, lighting effects and interactions are not implemented. Blue lights are Blender preview lights and materials, not game effects.
- This file has not been imported into the Doorways game. A production version needs properly named collision meshes and markers, UVs and suitable materials, GLB export, compilation through Doorways' asset pipeline, then runtime testing and performance work. Blender uses Z up; the native game uses Y up, so an export/import check is required.
- There is no streaming or open-world system implied by this model. Any gameplay mode, placement of enemies, size and balancing are still decisions to make.

No Meshy generation or paid jobs were used to make this prototype.
