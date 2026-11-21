# Nyxara's looks

The owner, 2026-11-21: "i like 3rd option but i want it to be tested at least 3 different variants in characterlab and
let me test them with 1,2,3 buttons ... let me also upgrade for each example how she may improve and then i will decide
which way i like her to move, lets try each upgrade of Wings changes her wings, each upgrade of armor upgrades and
changes her armor and whole set she wears and upgrading swords replaces her swords with different swords - make sure
the other swords you choose for each example will fit nyxara's animations as well as wings and set".

This folder makes a copy of her lab bundle, `Lab/Nyxara_Looks_v01`, in which every look's part is in her mesh (hidden
until a look shows it), with the looks' textures and `Looks.txt`. The Character Lab loads it (`testing/CharacterLab/heroes.tsv`);
`game/src/NyxaraLooks.cpp` reads `Looks.txt` and shows a look. Not in the game: the owner chooses first.

## The three examples

| | Wings (span) | Swords | Armour |
|---|---|---|---|
| **1 THE HIGHLANDS** | golden eagle (1.04), lammergeier (1.10), royal eagle (1.20) | shashkas of Svaneti (S11), khmali broadswords (S12), sabres of Queen Tamar (S13) | Svan mail; Khevsur silver (+ pauldrons P1); harness of Queen Tamar (+ P1 in gold, crown C1) |
| **2 MOONLIT** | silver, moonlit (glow), night-sky with stars (glow) | crescent sabres (S21b), moonsteel longswords (S22b), star-crystal blades (S23, the blade lit) | moon-silver mail; moonplate (+ P2); celestial harness (+ P2, crown C2, both lit) |
| **3 AMIRANI'S FIRE** | russet, ember (burning edges), phoenix | Colchian bronze swords (S31b, tinted bronze), ember blades (S32, lava cracks lit), phoenix flamberges (S33, lit) | bronze and leather; obsidian and ember (+ P3, lit); phoenix harness (+ P3, crown C3) |

A wings tier is a new plumage (`wings_<example><tier>.png`) and more span (the kit's `SetWingScale`, about the wings'
roots). An armour tier recolours her whole armour (`body_<example><tier>.png`; her face, hair and skin kept) and adds
pauldrons, then a crown. A swords tier shows another pair of swords and hides her own.

## How it is made

1. **The models: Hyper3D Rodin** (Gen-2.5 Medium, raw mesh, GLB with PBR textures; 0.5 credits each). An order file is
   `<name>\t<prompt>` a line: `order_swords.tsv`, `order_pieces.tsv`, `order_s31b.tsv`.
   `python rodin_order.py <order.tsv> [faces]` orders each name not yet in `GENERATIONS.tsv` and adds its generation id
   there; `python rodin_fetch.py <name> <generation-id>` downloads it to `rodin/<name>/base_basic_pbr.glb`. 18
   generations, 9.0 credits (S21, S22 and S31 came out wrong and were made again as S21b, S22b, S31b). `rodin/` is not
   in git (154 MB): keep a copy, the models were paid for.
2. **Fitted to her in Blender** (5.2): `blender --factory-startup -b art/nyxara/blender/Nyxara_mixamo.blend --python
   art/nyxara/looks/assemble.py -- build/character-lab/looks/Nyxara_looks.glb [<pictures folder>] [names...]`.
   - Swords (`SWORDS`): each is turned and scaled so that its handle lies where her own sword's does in her fist (the
     grip's middle at her handle's centre, 0.754 m from it to the point), its edge as hers; the grip made her handle's
     thickness; a glowing blade split into its own material (`Look_S23_Blade` ...); bound wholly to her sword bones
     (SwordLeft, SwordRight), so it is drawn, swung, carried crossed on her back and thrown as hers are.
   - Pauldrons (`PAULDRONS`): set over her shoulder along her upper arm, pushed out until her own shoulder is under it,
     bound to her nearest points' bones (smoothed); the right one is the left mirrored.
   - Crowns (`CROWNS`): fitted to her head's own outline at the brow (10 degrees tilted), bound to her Head bone.
   - Exported with `build_from_mixamo.py`'s glTF settings: her 31 clips and skeleton come out the same as the game's.
   With a pictures folder it renders each sword in her hand, on her back and in a cut, and each example from the
   front, side and back.
3. **Imported**: `DoorwaysAssetBuilder.exe character-pipeline build/character-lab/looks/Nyxara_looks.glb
   build/character-lab/runtime/assets/Compiled/Lab/Nyxara_Looks_v01 Idle Walk Stairs_Up Run Death Jump Attack_L1
   Attack_L2 Attack_L3 Attack_L_Step Attack_L_Run Attack_R1 Attack_R2 Attack_R3 Attack_R4 Attack_R_Step Attack_R_Run
   Attack_Jump Skill_Kick Skill_Throw Skill_Slide Skill_Cast BasicAttack Fly Cast_Fly Cast_Pull Fly_Rise Fly_Land Fall
   Stand Sword_Reach` (the asset builder is in `build/character-lab/tools`).
4. **Made small**: `python slim_textures.py <bundle>` (the parts' textures 512 x 512).
5. **Her looks' textures**: `blender --factory-startup -b art/nyxara/blender/Nyxara_mixamo.blend --python
   art/nyxara/looks/bake_maps.py -- build/character-lab/looks/maps` (once: where on her body and on her wings each
   texel is), then `python make_textures.py game/assets/Compiled/Characters/Nyxara/MixamoV1/Textures
   build/character-lab/looks/maps build/character-lab/looks/textures` (the colours are its `ARMOUR` table and
   `plumage`). Copy `textures/*.png` into the bundle's `Looks/` and `Looks.txt` into the bundle.

`tools/character-lab.ps1` does not make or delete the bundle: it stays in `build/character-lab/runtime` until made again.

## Looks.txt

`example <n> <title> | <about>`, then for each item and tier `<wings|swords|armour> <n> <tier> <name> | key=value ...`.
Keys: `show=` parts shown, `lit=<part>:<0..1>` a part's own light, `tint=<part>:<r>,<g>,<b>`, `body=` (and `bodynormal=`,
`bodyrough=`, `bodymetal=`, `bodyglow=`) her armour, `wings=`, `glow=`, `span=` her wings. Tier 0 of each item is her own.

## Other tools here

`preview_glb.py` and `preview_views.py` render a downloaded model; `sword_profile.py` measures a sword along its length
(where the guard, grip and pommel are) for the `SWORDS` table.
