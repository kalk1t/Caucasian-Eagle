# Doorways GLB Asset Contract Report

Source: `Nyxara/blender/Nyxara_game.glb`

## Executive Summary

| Severity | Count |
|---|---:|
| Blocking | 0 |
| Warning | 33 |
| Info | 37 |
| Total | 70 |

## Priority Fixes for Artist

1. Fix all `Warning` rows before the next production export.
2. Keep `Info` rows as review notes unless they point to intentional source cleanup.
3. Do not change runtime assets from this report; fix Blender/source export settings first.

## Blocking Errors

No blocking errors were reported.

## Warnings

### Naming Warnings

#### W-1 — Naming / Node: `Chest`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-2 — Naming / Node: `Hand_L`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-3 — Naming / Node: `Hand_R`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-4 — Naming / Node: `Head`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-5 — Naming / Node: `Hips`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-6 — Naming / Node: `LeftFoot`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-7 — Naming / Node: `LeftLeg`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-8 — Naming / Node: `LeftShoulder`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-9 — Naming / Node: `LeftToeBase`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-10 — Naming / Node: `LowerArm_L`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-11 — Naming / Node: `LowerArm_R`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-12 — Naming / Node: `Nyxara`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-13 — Naming / Node: `NyxaraRig`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-14 — Naming / Node: `RightFoot`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-15 — Naming / Node: `RightLeg`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-16 — Naming / Node: `RightShoulder`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-17 — Naming / Node: `RightToeBase`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-18 — Naming / Node: `Root`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-19 — Naming / Node: `Spine01`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-20 — Naming / Node: `Spine02`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-21 — Naming / Node: `UpperArm_L`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-22 — Naming / Node: `UpperArm_R`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-23 — Naming / Node: `UpperLeg_L`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-24 — Naming / Node: `UpperLeg_R`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-25 — Naming / Node: `WingL_1`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-26 — Naming / Node: `WingL_2`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-27 — Naming / Node: `WingL_3`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-28 — Naming / Node: `WingR_1`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-29 — Naming / Node: `WingR_2`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-30 — Naming / Node: `WingR_3`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-31 — Naming / Node: `head_end`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-32 — Naming / Node: `headfront`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

#### W-33 — Naming / Node: `neck`

**Severity:** Warning

**Message:** Render/source node does not use the Doorways_ prefix.

**Suggested fix:** Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming.

---

## Informational Notes

### Gameplay Authoring Markers

#### I-1 — GameplayAuthoring / GLB: `GameplayMarkers`

**Severity:** Info

**Message:** INFO: No gameplay authoring markers were found in this source GLB.

**Suggested fix:** This is valid for the current main-scene baseline. Add TRN_, DOOR_, SPAWN_, or BLK_ source nodes only when authoring transition/spawn contracts.

---

### Texture Path Validation

#### I-1 — TexturePath / CompiledTexturePath: `Textures`

**Severity:** Info

**Message:** PASS: Configured compiled texture path is a safe relative path; no texture write was performed.

**Suggested fix:** Configure a relative compiled texture path that remains beneath the compiled asset root.

---

#### I-2 — TexturePath / Material: `NyxaraBody`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraBody. Slot: MetallicRoughness. Texture index: 2. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-3 — TexturePath / Material: `NyxaraBody`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraBody. Slot: Normal. Texture index: 0. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-4 — TexturePath / Material: `NyxaraBody`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraBody. Slot: BaseColor. Texture index: 1. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-5 — TexturePath / Material: `NyxaraWings`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraWings. Slot: MetallicRoughness. Texture index: 5. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-6 — TexturePath / Material: `NyxaraWings`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraWings. Slot: Normal. Texture index: 3. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-7 — TexturePath / Material: `NyxaraWings`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraWings. Slot: BaseColor. Texture index: 4. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-8 — TexturePath / Material: `Slot_sword_left__Moonfeather`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Moonfeather. Slot: MetallicRoughness. Texture index: 14. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-9 — TexturePath / Material: `Slot_sword_left__Moonfeather`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Moonfeather. Slot: Normal. Texture index: 12. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-10 — TexturePath / Material: `Slot_sword_left__Moonfeather`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Moonfeather. Slot: BaseColor. Texture index: 13. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-11 — TexturePath / Material: `Slot_sword_left__Talonshade`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Talonshade. Slot: MetallicRoughness. Texture index: 11. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-12 — TexturePath / Material: `Slot_sword_left__Talonshade`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Talonshade. Slot: Normal. Texture index: 9. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-13 — TexturePath / Material: `Slot_sword_left__Talonshade`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Talonshade. Slot: BaseColor. Texture index: 10. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-14 — TexturePath / Material: `Slot_sword_right__Moonfeather`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Moonfeather. Slot: MetallicRoughness. Texture index: 8. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-15 — TexturePath / Material: `Slot_sword_right__Moonfeather`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Moonfeather. Slot: Normal. Texture index: 6. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-16 — TexturePath / Material: `Slot_sword_right__Moonfeather`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Moonfeather. Slot: BaseColor. Texture index: 7. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-17 — TexturePath / Material: `Slot_sword_right__Talonshade`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Talonshade. Slot: MetallicRoughness. Texture index: 17. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-18 — TexturePath / Material: `Slot_sword_right__Talonshade`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Talonshade. Slot: BaseColor. Texture index: 16. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

#### I-19 — TexturePath / Material: `Slot_sword_right__Talonshade`

**Severity:** Info

**Message:** PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Talonshade. Slot: Normal. Texture index: 15. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid.

**Suggested fix:** Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder.

---

### Packed Map Validation

#### I-1 — PackedMap / Material: `NyxaraBody`

**Severity:** Info

**Message:** PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: NyxaraBody. Texture index: 2. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured.

**Suggested fix:** Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R.

---

#### I-2 — PackedMap / Material: `NyxaraWings`

**Severity:** Info

**Message:** PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: NyxaraWings. Texture index: 5. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured.

**Suggested fix:** Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R.

---

#### I-3 — PackedMap / Material: `Slot_sword_left__Moonfeather`

**Severity:** Info

**Message:** PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: Slot_sword_left__Moonfeather. Texture index: 14. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured.

**Suggested fix:** Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R.

---

#### I-4 — PackedMap / Material: `Slot_sword_left__Talonshade`

**Severity:** Info

**Message:** PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: Slot_sword_left__Talonshade. Texture index: 11. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured.

**Suggested fix:** Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R.

---

#### I-5 — PackedMap / Material: `Slot_sword_right__Moonfeather`

**Severity:** Info

**Message:** PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: Slot_sword_right__Moonfeather. Texture index: 8. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured.

**Suggested fix:** Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R.

---

#### I-6 — PackedMap / Material: `Slot_sword_right__Talonshade`

**Severity:** Info

**Message:** PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: Slot_sword_right__Talonshade. Texture index: 17. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured.

**Suggested fix:** Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R.

---

### Source Scene Bounds

#### I-1 — SceneBounds / Scene: `Nyxara_game.glb`

**Severity:** Info

**Message:** PASS: Source-scene bounds Min=(-1.32019, 1.49297e-07, -0.424764), Max=(1.32019, 1.85786, 0.847538), Size=(2.64037, 1.85786, 1.2723); contributors=1, invalid/missing=0.

**Suggested fix:** Use the aggregate as source-validation evidence only; no arbitrary world-size limits are imposed.

---

## Full Raw Issue Table

| # | Severity | Category | Object Type | Object Name | Message | Suggested Fix |
|---:|---|---|---|---|---|---|
| 1 | Warning | Naming | Node | Chest | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 2 | Warning | Naming | Node | Hand_L | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 3 | Warning | Naming | Node | Hand_R | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 4 | Warning | Naming | Node | Head | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 5 | Warning | Naming | Node | Hips | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 6 | Warning | Naming | Node | LeftFoot | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 7 | Warning | Naming | Node | LeftLeg | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 8 | Warning | Naming | Node | LeftShoulder | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 9 | Warning | Naming | Node | LeftToeBase | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 10 | Warning | Naming | Node | LowerArm_L | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 11 | Warning | Naming | Node | LowerArm_R | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 12 | Warning | Naming | Node | Nyxara | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 13 | Warning | Naming | Node | NyxaraRig | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 14 | Warning | Naming | Node | RightFoot | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 15 | Warning | Naming | Node | RightLeg | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 16 | Warning | Naming | Node | RightShoulder | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 17 | Warning | Naming | Node | RightToeBase | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 18 | Warning | Naming | Node | Root | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 19 | Warning | Naming | Node | Spine01 | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 20 | Warning | Naming | Node | Spine02 | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 21 | Warning | Naming | Node | UpperArm_L | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 22 | Warning | Naming | Node | UpperArm_R | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 23 | Warning | Naming | Node | UpperLeg_L | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 24 | Warning | Naming | Node | UpperLeg_R | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 25 | Warning | Naming | Node | WingL_1 | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 26 | Warning | Naming | Node | WingL_2 | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 27 | Warning | Naming | Node | WingL_3 | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 28 | Warning | Naming | Node | WingR_1 | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 29 | Warning | Naming | Node | WingR_2 | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 30 | Warning | Naming | Node | WingR_3 | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 31 | Warning | Naming | Node | head_end | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 32 | Warning | Naming | Node | headfront | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 33 | Warning | Naming | Node | neck | Render/source node does not use the Doorways_ prefix. | Rename render nodes to Doorways_<DescriptiveName>. Collision nodes should use COL_ or UCX_ naming. |
| 34 | Info | Summary | GLB | Nyxara_game.glb | Asset contract report generated successfully. | Review warnings and blocking errors before sending the source asset to runtime import. |
| 35 | Info | Summary | GLB | Nyxara_game.glb | GLB contains 33 nodes, 1 meshes, and 6 materials. | Use this count to compare source exports between revisions. |
| 36 | Info | PolicyHit | ImportPolicy | GenericGltf | glTF import policy selected explicitly for this import. | Generic assets should use GenericGltf. Doorways main-scene assets may use DoorwaysMainScene for documented compatibility rules. |
| 37 | Info | GameplayAuthoring | GLB | GameplayMarkers | INFO: No gameplay authoring markers were found in this source GLB. | This is valid for the current main-scene baseline. Add TRN_, DOOR_, SPAWN_, or BLK_ source nodes only when authoring transition/spawn contracts. |
| 38 | Info | PBRValidation | Material | NyxaraBody | PBR material validation: BaseColor=Valid/SRGB/2048x2048x4/8bit/Sampler=Explicit, Normal=Valid/Linear/2048x2048x4/8bit/Sampler=Explicit, MetallicRoughness=Valid/Linear/2048x2048x4/8bit/G=Roughness,B=Metallic/Sampler=Explicit, AO=MissingOptional/Fallback, Emissive=MissingOptional/Fallback. | Use this row to confirm color-space expectations, packed metallic-roughness channel usage, sampler source, and fallback behavior. |
| 39 | Info | PBRValidation | Material | NyxaraWings | PBR material validation: BaseColor=Valid/SRGB/2048x2048x4/8bit/Sampler=Explicit, Normal=Valid/Linear/2048x2048x4/8bit/Sampler=Explicit, MetallicRoughness=Valid/Linear/2048x2048x4/8bit/G=Roughness,B=Metallic/Sampler=Explicit, AO=MissingOptional/Fallback, Emissive=MissingOptional/Fallback. | Use this row to confirm color-space expectations, packed metallic-roughness channel usage, sampler source, and fallback behavior. |
| 40 | Info | PBRValidation | Material | Slot_sword_left__Moonfeather | PBR material validation: BaseColor=Valid/SRGB/2048x2048x4/8bit/Sampler=Explicit, Normal=Valid/Linear/2048x2048x4/8bit/Sampler=Explicit, MetallicRoughness=Valid/Linear/2048x2048x4/8bit/G=Roughness,B=Metallic/Sampler=Explicit, AO=MissingOptional/Fallback, Emissive=MissingOptional/Fallback. | Use this row to confirm color-space expectations, packed metallic-roughness channel usage, sampler source, and fallback behavior. |
| 41 | Info | PBRValidation | Material | Slot_sword_left__Talonshade | PBR material validation: BaseColor=Valid/SRGB/2048x2048x4/8bit/Sampler=Explicit, Normal=Valid/Linear/2048x2048x4/8bit/Sampler=Explicit, MetallicRoughness=Valid/Linear/2048x2048x4/8bit/G=Roughness,B=Metallic/Sampler=Explicit, AO=MissingOptional/Fallback, Emissive=MissingOptional/Fallback. | Use this row to confirm color-space expectations, packed metallic-roughness channel usage, sampler source, and fallback behavior. |
| 42 | Info | PBRValidation | Material | Slot_sword_right__Moonfeather | PBR material validation: BaseColor=Valid/SRGB/2048x2048x4/8bit/Sampler=Explicit, Normal=Valid/Linear/2048x2048x4/8bit/Sampler=Explicit, MetallicRoughness=Valid/Linear/2048x2048x4/8bit/G=Roughness,B=Metallic/Sampler=Explicit, AO=MissingOptional/Fallback, Emissive=MissingOptional/Fallback. | Use this row to confirm color-space expectations, packed metallic-roughness channel usage, sampler source, and fallback behavior. |
| 43 | Info | PBRValidation | Material | Slot_sword_right__Talonshade | PBR material validation: BaseColor=Valid/SRGB/2048x2048x4/8bit/Sampler=Explicit, Normal=Valid/Linear/2048x2048x4/8bit/Sampler=Explicit, MetallicRoughness=Valid/Linear/2048x2048x4/8bit/G=Roughness,B=Metallic/Sampler=Explicit, AO=MissingOptional/Fallback, Emissive=MissingOptional/Fallback. | Use this row to confirm color-space expectations, packed metallic-roughness channel usage, sampler source, and fallback behavior. |
| 44 | Info | PBRValidation | MaterialSet | Nyxara_game.glb | PBR validation summary: valid texture references=18, non-fatal fallback references=12, fatal corrupt references=0, unsupported appearance extensions=0. | Fatal corrupt references must be zero before committing compiled assets. |
| 45 | Info | TexturePath | CompiledTexturePath | Textures | PASS: Configured compiled texture path is a safe relative path; no texture write was performed. | Configure a relative compiled texture path that remains beneath the compiled asset root. |
| 46 | Info | TexturePath | Material | NyxaraBody | PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraBody. Slot: MetallicRoughness. Texture index: 2. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 47 | Info | TexturePath | Material | NyxaraBody | PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraBody. Slot: Normal. Texture index: 0. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 48 | Info | TexturePath | Material | NyxaraBody | PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraBody. Slot: BaseColor. Texture index: 1. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 49 | Info | TexturePath | Material | NyxaraWings | PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraWings. Slot: MetallicRoughness. Texture index: 5. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 50 | Info | TexturePath | Material | NyxaraWings | PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraWings. Slot: Normal. Texture index: 3. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 51 | Info | TexturePath | Material | NyxaraWings | PASS: Texture path validation. Source nodes: Nyxara. Material: NyxaraWings. Slot: BaseColor. Texture index: 4. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 52 | Info | TexturePath | Material | Slot_sword_left__Moonfeather | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Moonfeather. Slot: MetallicRoughness. Texture index: 14. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 53 | Info | TexturePath | Material | Slot_sword_left__Moonfeather | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Moonfeather. Slot: Normal. Texture index: 12. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 54 | Info | TexturePath | Material | Slot_sword_left__Moonfeather | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Moonfeather. Slot: BaseColor. Texture index: 13. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 55 | Info | TexturePath | Material | Slot_sword_left__Talonshade | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Talonshade. Slot: MetallicRoughness. Texture index: 11. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 56 | Info | TexturePath | Material | Slot_sword_left__Talonshade | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Talonshade. Slot: Normal. Texture index: 9. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 57 | Info | TexturePath | Material | Slot_sword_left__Talonshade | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_left__Talonshade. Slot: BaseColor. Texture index: 10. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 58 | Info | TexturePath | Material | Slot_sword_right__Moonfeather | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Moonfeather. Slot: MetallicRoughness. Texture index: 8. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 59 | Info | TexturePath | Material | Slot_sword_right__Moonfeather | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Moonfeather. Slot: Normal. Texture index: 6. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 60 | Info | TexturePath | Material | Slot_sword_right__Moonfeather | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Moonfeather. Slot: BaseColor. Texture index: 7. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 61 | Info | TexturePath | Material | Slot_sword_right__Talonshade | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Talonshade. Slot: MetallicRoughness. Texture index: 17. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 62 | Info | TexturePath | Material | Slot_sword_right__Talonshade | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Talonshade. Slot: BaseColor. Texture index: 16. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 63 | Info | TexturePath | Material | Slot_sword_right__Talonshade | PASS: Texture path validation. Source nodes: Nyxara. Material: Slot_sword_right__Talonshade. Slot: Normal. Texture index: 15. Source URI: <embedded/no URI>. Problem/status: embedded GLB image is valid. | Use an embedded GLB image or an existing source-relative image path that stays inside the source asset folder. |
| 64 | Info | PackedMap | Material | NyxaraBody | PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: NyxaraBody. Texture index: 2. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured. | Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R. |
| 65 | Info | PackedMap | Material | NyxaraWings | PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: NyxaraWings. Texture index: 5. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured. | Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R. |
| 66 | Info | PackedMap | Material | Slot_sword_left__Moonfeather | PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: Slot_sword_left__Moonfeather. Texture index: 14. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured. | Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R. |
| 67 | Info | PackedMap | Material | Slot_sword_left__Talonshade | PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: Slot_sword_left__Talonshade. Texture index: 11. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured. | Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R. |
| 68 | Info | PackedMap | Material | Slot_sword_right__Moonfeather | PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: Slot_sword_right__Moonfeather. Texture index: 8. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured. | Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R. |
| 69 | Info | PackedMap | Material | Slot_sword_right__Talonshade | PASS: Valid packed metallic-roughness source. Source nodes: Nyxara. Material: Slot_sword_right__Talonshade. Texture index: 17. Image source: Image_1 (embedded). Required channels: at least 3; G=roughness, B=metallic. No glTF occlusion texture is configured. | Provide a decodable metallic-roughness image with at least RGB channels; preserve G=roughness and B=metallic, and verify AO uses R. |
| 70 | Info | SceneBounds | Scene | Nyxara_game.glb | PASS: Source-scene bounds Min=(-1.32019, 1.49297e-07, -0.424764), Max=(1.32019, 1.85786, 0.847538), Size=(2.64037, 1.85786, 1.2723); contributors=1, invalid/missing=0. | Use the aggregate as source-validation evidence only; no arbitrary world-size limits are imposed. |

## Artist Fix Notes

- `Doorways_` names are expected for render/source nodes.
- `COL_` or `UCX_` names are expected for collision proxy nodes.
- Walkable proxies should use explicit names such as `COL_Walk_Field`, `COL_Walk_Temple`, or `COL_Walkable_Stairs_Main`.
- Gameplay authoring markers use `TRN_`, `DOOR_`, `SPAWN_`, or `BLK_`; these are validation-only and do not implement runtime story transitions.
- Foliage/card materials should usually use `MASK`, a useful alpha cutoff, and `doubleSided=true` until sorted transparency exists.
- AO textures should be connected to glTF `occlusionTexture`, not only placed in the source texture folder.
