# Exports Moonlit_Sanctuary_v01.blend as a Doorways game GLB (does not modify the source .blend).
# Usage: blender -b "Moonlit_Sanctuary_v01.blend" -P export_game_glb.py -- <output.glb>
# Render meshes are joined per material (Doorways_MS_<material>). Collision copies use the
# Doorways naming policy: COL_Walkable_* = walkable ground, COL_* = blockers.
import bpy, sys, re

out = sys.argv[sys.argv.index("--") + 1]

WALKABLE = re.compile(r"^(Arena surface|Arena foundation|Radial bridge|Outer circular foundations|District |Outlook foundation|Outlook paving|MS_Temple_step|MS_Temple_foundation|MS_Temple_inner_floor)")
BLOCKER = re.compile(r"^(Bridge parapet|Ring outer battlement|MS_Arch_left_pier|MS_Arch_right_pier|MS_Arch_plinth|MS_Bench_|MS_Broken_column|MS_Column_base|MS_Column_foot|MS_Temple_column|MS_Temple_rear_wall|MS_Temple_side_wall|MS_Temple_side_bay|MS_Temple_tower|MS_Temple_tower_base|Nature_DeadTree|Nature_Grave|Soul lantern plinth)")
DROP = re.compile(r"^PLAYER SCALE")

scene = bpy.context.scene
for o in list(scene.objects):
    if o.type != 'MESH' or DROP.match(o.name):
        bpy.data.objects.remove(o, do_unlink=True)

# Nyxara palette: dark-green moss stone under moonlight (linear base colors).
PALETTE = {
    "Paving 00": (0.035, 0.080, 0.052, 1), "Paving 01": (0.045, 0.095, 0.062, 1),
    "Paving 02": (0.052, 0.105, 0.072, 1), "Paving 03": (0.062, 0.120, 0.085, 1),
    "Paving 04": (0.030, 0.068, 0.046, 1),
    "Stone - pale trim": (0.30, 0.36, 0.36, 1),      # moonlit silver-green trim
    "Stone - shadow": (0.016, 0.040, 0.030, 1),
    "Stone - slate": (0.040, 0.095, 0.072, 1),
    "Stone - weathered": (0.075, 0.150, 0.095, 1),   # moss
    "Trees - black bark": (0.020, 0.060, 0.034, 1),  # deep forest green
    "Soul - icy blue": (0.62, 0.90, 1.00, 1),        # moonlight
    "Lantern glow": (0.66, 1.00, 0.78, 1),           # pale soul-green
    "Iron": (0.24, 0.27, 0.29, 1),                   # dark silver
}
for m in bpy.data.materials:
    if m.name in PALETTE:
        m.diffuse_color = PALETTE[m.name]

# Bake Blender viewport colors into the exported Principled base color; no emission.
for m in bpy.data.materials:
    if not m.node_tree:
        continue
    for n in m.node_tree.nodes:
        if n.type == 'BSDF_PRINCIPLED':
            for link in list(n.inputs['Base Color'].links):  # procedural colour nodes would override the palette
                m.node_tree.links.remove(link)
            n.inputs['Base Color'].default_value = m.diffuse_color
            n.inputs['Metallic'].default_value = 0.6 if m.name == 'Iron' else 0.0
            n.inputs['Roughness'].default_value = 0.85
            if 'Emission Strength' in n.inputs:
                n.inputs['Emission Strength'].default_value = 0.0

dg = bpy.context.evaluated_depsgraph_get()
def evaluated_copy(o, name):
    me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
    me.transform(o.matrix_world)
    c = bpy.data.objects.new(name, me)
    scene.collection.objects.link(c)
    return c

walk, block, render = [], [], {}
for o in list(scene.objects):
    base = re.sub(r"\.\d+$", "", o.name)
    if WALKABLE.match(base):
        walk.append(evaluated_copy(o, "w"))
    elif BLOCKER.match(base):
        block.append(evaluated_copy(o, "b"))
    c = evaluated_copy(o, "r")
    c.data.materials.clear()
    mat = o.active_material
    c.data.materials.append(mat)
    render.setdefault(mat.name if mat else "Default", []).append(c)
    bpy.data.objects.remove(o, do_unlink=True)

def join(objs, name):
    bpy.ops.object.select_all(action='DESELECT')
    for x in objs:
        x.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    j = bpy.context.view_layer.objects.active
    j.name = j.data.name = name
    return j

for mat, objs in render.items():
    join(objs, "Doorways_MS_" + re.sub(r"[^A-Za-z0-9]+", "_", mat).strip("_"))
for objs, name in ((walk, "COL_Walkable_MoonlitGround"), (block, "COL_MoonlitBlockers")):
    j = join(objs, name)
    j.data.materials.clear()

for o in scene.objects:
    print("EXPORT", o.name, len(o.data.polygons))
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', use_selection=False, export_apply=True,
                          export_yup=True, export_lights=False, export_cameras=False)
print("EXPORTED", out)
