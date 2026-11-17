# Blender skills scripts (vendored)

Three scripts copied unchanged from the **blender-agent-skills** plugin by EL4CTEO
(https://github.com/EL4CTEO/blender-skills, version 1.3.2, MIT licence: `LICENSE` here). They are copied so the art
pipeline does not depend on a plugin folder inside one person's Claude Code setup.

| File | From | Used by | What it does |
|---|---|---|---|
| `qa_check.py` | `skills/blender-asset-qa/scripts` | `art/places/prepare_places.py` (the QA gate) | Checks a mesh for z-fighting, floating parts, hidden back-to-back faces, flipped or inconsistent normals, open or degenerate geometry, UV and material problems. `run(objects=[...], profile="game")` returns `errors` and `warnings`. |
| `bake_maps.py` | `skills/blender-texture-baking/scripts` | `art/places/prepare_places.py` | Cycles bakes, high to low (selected to active): colour, tangent-space normals (OpenGL), AO and more, on the GPU when there is one. |
| `_bpyx.py` | `shared` | both | Their shared helpers. |

To update: copy the newer files over these, then run `prepare_places.py` on a few models and compare
(`art/places/qa_report.txt` and the pictures in `art/places/out`).
