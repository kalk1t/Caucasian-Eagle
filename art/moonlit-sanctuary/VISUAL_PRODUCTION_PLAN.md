# Moonlit Sanctuary: visual production plan

This document is the path from the existing Blender prototype to the selected **expanded Moonlit Sanctuary** image. The image is an atmospheric design target, not an exact construction drawing: paths, buildings, character proportions, effects and background details in it require interpretation. The current map and assets are **early placeholders**, not a finished or playable level.

## 1. The visual target

The chosen image shows a large circular stone sanctuary suspended in mist. A moonlit central combat court is surrounded by bridges, Gothic temples, cloisters, cemetery gardens and broken arches. It should feel ancient, cold, mystical and dangerous, with room for Nyxara to fight and explore. The dominant colors are deep blue-gray and silver; small warm orange lights and pale blue souls add contrast. Look for these visual layers:

1. **Silhouette:** tall temple towers, pointed arches, distant ruined bridges, dark trees and a full moon.
2. **Layout:** a circular arena, radial paths, an outer loop and distinctive places off that loop.
3. **Materials:** worn stone blocks, cracked paving, aged metal, weathered statues and roots.
4. **Atmosphere:** moonlight, fog below the bridges, restrained soul glows and warm torch accents.
5. **Story:** a sacred place overtaken by monsters; visual storytelling through damaged structures, graves and abandoned spaces.

The reference includes Nyxara and monsters as illustrations. Neither is an existing part of the Blender level or a finished animation/character asset in this package. Source any approved character model separately before trying to match their appearance.

## 2. What exists now, and what is missing

The current `Moonlit_Sanctuary_v01.blend` contains a central round floor, four bridges, a surrounding ring, four main landmark pads and four smaller outlooks. It has a simple north temple facade, west crypt, entrance arches, benches, leafless trees, graves, blue soul markers and basic cold/warm preview lighting. It also contains a human-sized scale placeholder. Existing standalone assets are prototype components; consult the folder inventory for their exact files.

Compared with the image, the scene is still sparse, flat and orderly. Major missing elements include the tall cathedral silhouette, detailed arches and colonnades, masonry and damaged paving, graveyard and garden dressing, heroic statues, misty depths, distant architecture and a visible moon. There are no monsters or animated souls. A Blender still does not establish that the map is connected, navigable, balanced, optimized or working in Doorways.

## 3. Set the layout before polishing

The existing layout is a starting proposal: central arena radius **25 m**, outer route at approximately **59–77 m** radius, and four radial bridges approximately **9 m** wide. The main destination platforms extend to approximately **93 m** from the center. These values are provisional, not approved final dimensions.

1. Keep the center legible and open for movement, attacks, jump and flight; place columns or broken cover near its perimeter rather than packing the middle.
2. Walk the intended route in a test version: south entrance → outer loop → north temple → west crypt → east graves → one bridge → central court. Give each location a recognizable silhouette and sightline so a player can tell where they are.
3. Create an actual player-sized movement test. The current small placeholder is only a reference, not Nyxara. Check clearances for wings, camera distance, sword reach and camera clipping using the real approved character when available.
4. Test gaps, bridges, edges, stairs and entrances using actual collision and navigation. Visual mesh overlap is not proof of traversability. Confirm whether falling, flying across gaps, or entering the temple is intended with the gameplay owner.
5. Decide the purpose and size of exploration zones and the proposed monster encounter locations before adding dense props. Do not assume these are the same rules as the separate three-lane PvP map.

**Checkpoint:** a reviewer can identify the arena, the full outer loop and four destinations from an overhead view and can follow the intended route at player height. Document all inaccessible or placeholder areas.

## 4. Build a modular environment kit

Model these as individually editable assets with consistent scale, pivot and naming; keep their originals as separate Blender source files. Use reusable pieces and connected/linked instances for repeating elements where appropriate. The checklist is a production target: **unchecked items are not completed**.

### Priority A — structural kit

- [ ] Circular arena flooring with worn radial and concentric stone patterns; modular floor tiles and trim bands.
- [ ] Straight and curved bridge sections, bridge under-arches, railings, pillars and broken variants.
- [ ] Curved perimeter walls: straight/curved sections, openings, corner posts, caps and a few damaged pieces.
- [ ] Full main temple: large pointed entry, upper nave and towers, believable side walls, roofline, open entrance, interior floor, columns and broken variants. The existing facade is a simple blockout.
- [ ] Smaller shrine/crypt kit: doorways, altar, wall, corner, roof and steps.
- [ ] Wide stair and ramp pieces, with transitions checked at character scale. Keep central routes clear.

### Priority B — identity and exploration

- [ ] Statue set: one large temple focal statue and smaller robed stone figures on plinths; build separate silhouette variants.
- [ ] Grave markers, tomb slabs and mausoleum pieces with varied but readable shapes.
- [ ] Bare-tree silhouettes: several different branching shapes, broken limbs and roots; keep limbs clear of important camera views.
- [ ] Stone bench, lantern pedestal, torch sconce, iron fence, pillar, rubble and broken masonry set.
- [ ] Distinct garden and courtyard pieces, including carved fountain or shrine bases, and a small set of optional muted plants.
- [ ] Path and altar markings, moon symbol, worn banners and other selective storytelling details.

### Priority C — distant scenery and effects

- [ ] Simplified distant Gothic towers, ruined aqueducts, cliffs and a moon/backdrop, clearly separated from walkable spaces.
- [ ] Mist in the lower void, restrained blue soul wisps and torch flames, after confirming what effects the target runtime can support.
- [ ] Nyxara, monster models and their animations as separate content pipelines; placeholders must not be presented as approved character assets.

**Checkpoint:** finish one representative bridge-to-temple section at player scale before propagating its kit across the whole map. Review a close camera, overhead camera and wireframe/object count; revise proportions and materials based on feedback.

## 5. Model the sanctuary's key details

1. **Temple:** replace the simple facade with layered towers, pointed-arch portals and supporting piers. Open enough of the building to make the entry and intended interior believable. Break the silhouette with missing stonework and different roof heights, without cluttering the entrance.
2. **Paving:** build stone tile variations and use a few larger broken pieces near the edge. Keep level collision independent from small decorative cracks. Establish a circular moon motif in the arena as geometry, a decal or a texture according to runtime support.
3. **Walls and bridges:** form recognizable stone courses and arch supports beneath the decks. Create solid and damaged wall pieces; ensure every intended path remains open. Broken rails should read as broken even in silhouette.
4. **Trees and benches:** vary repeated shapes and rotations, cluster trees at the perimeter and keep the center clear. Put benches along contemplative alcoves, not in the principal route.
5. **Grave garden and crypt:** create clear landmarks using statues, tombs and a local focal light. Connect each to the outer route with a visible, testable path.
6. **Distant depth:** add cheaper background towers and cliffs behind the playable map. They should frame the sanctuary without being confused with places the player can reach.

## 6. Material and lighting direction

The `.blend` currently uses procedural preview materials. They are not automatically game-ready. Develop a small coherent material library: dark blue-gray masonry, lighter worn edging, cracked paving, dark bark, tarnished iron, old cloth and restrained emissive soul/torch materials.

- UV unwrap the actual game meshes and use a consistent texel-density strategy, with a small set of reusable tileable surfaces and trim sheets where useful.
- Prepare base color, normal and roughness textures; use packed maps only if the Doorways pipeline expects that packing. Base-color image data should be treated as sRGB; normal and other data maps should be linear/non-color. Avoid relying on Blender-only procedural nodes in the export: bake them or use supported texture/material inputs and verify after import.
- Preview a cool broad moon key with faint ambient fill; reserve warmer pools of light for torches at major landmarks. Soul blue should be an accent so enemies and paths remain readable.
- Add fog, bloom, moving flame and a moon as separate visual layers and validate their game support and performance. Blender Cycles lights, compositor effects and volume settings are not automatically runtime features.
- Compare both a wide composition and player-height views. A beautiful aerial image is insufficient if the ground-level map is too dark or the third-person camera cannot see enemies.

**Checkpoint:** the approved representative section has readable silhouettes and dark-soul mood in a Blender render **and** remains readable in an actual engine scene after export. Neither result guarantees the other.

## 7. Optional Meshy use

Blender is suitable for the repeatable architectural kit and layout. Meshy may help with a few isolated organic or sculptural props, such as a statue or a gnarled tree, if a suitable reference and available budget exist. Before submitting any Meshy generation, check the tool's current requirements, estimated credit cost and the user's spending authorization. Generated pieces must be reviewed for anatomy, silhouette, topology, scale, textures, licensing and game performance; retopology or manual correction may be necessary. A single generated model should not be assumed to produce the entire coherent map.

## 8. Doorways production handoff

1. Preserve the editable `.blend` sources and original reference. Keep separate source files for modular assets and distinguish prototype files from approved final work.
2. Prepare actual static meshes, UVs and materials for the existing Blender → GLB → Doorways asset-builder pipeline. Give visible meshes clear ASCII names. Create separate, simple, properly aligned collision proxies using `COL_` or `UCX_` names; walkable and stair proxies should follow the project's `COL_Walkable_` and `COL_Stairs_` conventions.
3. Treat `DOOR_`, `SPAWN_` and other authoring markers as report-only until the game explicitly wires them to logic. Their names do not create teleportation, encounters or spawning. Exporting a GLB alone does not create a playable scene.
4. Blender uses Z up; the native game uses Y up. Verify the actual exported/imported axes, scale, normals, materials and collision in the existing toolchain rather than assuming a coordinate conversion worked.
5. Compile required runtime assets and assemble the scene through the project pipeline. Check imported dimensions, visible-versus-collision alignment and names. Confirm lighting/effects separately in the engine.
6. Optimize against measured budgets for the target hardware: mesh/triangle counts, materials, texture memory, object/draw counts and lighting. Modular files do not prove streaming exists.
7. Test movement, camera, combat, jump/flight, falling, AI navigation, encounter placement, performance and reload behavior in the actual game. Record what was tested and the build/assets used.

Relevant project references: `docs/AssetAuthoringContract.md` and `docs/Assets/R301PipelineContract.md` in the Doorways-D3D11 repository. Check their current versions before integration.

## 9. Suggested milestone order

| Stage | Deliverable | Review needed before continuing |
| --- | --- | --- |
| 1. Layout | Updated circular blockout and route/scale diagram | Approve landmarks, scale assumptions and exploration direction |
| 2. Vertical slice | One detailed temple-to-bridge sector with materials and light | Compare to reference at player height and overhead |
| 3. Asset kit | Modular walls, arches, bridge, paving, temple pieces and props | Confirm reuse, scale, silhouette and collision approach |
| 4. Full art pass | Kit assembled across the map; gardens, ruins and background | Review variety, route readability and visual balance |
| 5. Runtime proof | Imported scene with tested collision and supported materials | Measure performance and validate movement/camera |
| 6. Encounters | Approved Nyxara/monster assets and implemented gameplay | Test actual combat, navigation and encounter behavior |

Each stage should retain source files and screenshots, list unresolved issues, and avoid calling the map finished until the relevant runtime tests and visual review pass.
