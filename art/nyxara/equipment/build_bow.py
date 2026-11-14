"""Her bow as a weapon of its own, from the model the bow in her old mesh was made from (Meshy: Eldritch Longbow).

    blender -b -P art/nyxara/equipment/build_bow.py -- <art/nyxara/meshy/out/bow_3d_1/model_url.glb> <out.glb> <out.json>

The numbers are those of art/nyxara/blender/assemble.py (make_bow): the model's stave is along Z, its string toward +X,
its length 1.901. The weapon frame is the one the other modular weapons have (glTF, Y up): grip at the origin, limbs
along +Y, the string toward +Z. The string is a part of its own, material BowCord: the game hides it while she draws.
"""
import bpy, bmesh, json, sys
import numpy as np

SRC, OUT, META = sys.argv[sys.argv.index("--") + 1:][:3]
LENGTH = 1.35                      # m, tip to tip
MODEL_LENGTH = 1.901
GRIP = np.array((-0.035, 0.0, -0.07))
STRING = (0.200, -0.075)           # the string in the model: x = STRING[0] + STRING[1] * z
STRING_Z = (-0.55, 0.64)
TRIS = 9000
SCALE = LENGTH / MODEL_LENGTH

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
for o in meshes:
    mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = mw
for o in list(bpy.data.objects):
    if o.type != 'MESH': bpy.data.objects.remove(o)
bpy.ops.object.select_all(action='DESELECT')
for o in meshes: o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1: bpy.ops.object.join()
bow = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def verts(o):
    a = np.empty(len(o.data.vertices) * 3); o.data.vertices.foreach_get("co", a); return a.reshape(-1, 3)


def keep(o, mask):
    bm = bmesh.new(); bm.from_mesh(o.data); bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[v for v, k in zip(bm.verts, mask) if not k], context='VERTS')
    bm.to_mesh(o.data); bm.free(); o.data.update()


V = verts(bow)
print("BOW model bounds", V.min(0).round(3), V.max(0).round(3), "verts", len(V))
on = (np.abs(V[:, 0] - (STRING[0] + STRING[1] * V[:, 2])) < 0.016) & (V[:, 2] > STRING_Z[0]) & (V[:, 2] < STRING_Z[1])
cord = bow.copy(); cord.data = bow.data.copy(); bpy.context.scene.collection.objects.link(cord)
keep(cord, on); keep(bow, ~on)


def frame(P):
    """Model point -> Blender coordinates whose glTF export (x, z, -y) is the weapon frame (y_m, z_m, x_m)."""
    R = (np.asarray(P, dtype=float) - GRIP) * SCALE
    return np.stack([R[..., 1], -R[..., 0], R[..., 2]], axis=-1)


for part, name, tris in ((bow, "EldritchBow", TRIS), (cord, "BowCord", 200)):
    have = sum(len(p.vertices) - 2 for p in part.data.polygons)
    if have > tris:
        m = part.modifiers.new("d", 'DECIMATE'); m.ratio = tris / have
        bpy.context.view_layer.objects.active = part; bpy.ops.object.modifier_apply(modifier="d")
    W = frame(verts(part)); part.data.vertices.foreach_set("co", W.ravel()); part.data.update()
    part.name = name
    mat = part.data.materials[0].copy() if part.data.materials else bpy.data.materials.new(name)
    mat.name = name
    part.data.materials.clear(); part.data.materials.append(mat)
    for p in part.data.polygons: p.material_index = 0
    print("BOW", name, "tris", have, "->", sum(len(p.vertices) - 2 for p in part.data.polygons))


def gltf(p):
    b = frame(np.array(p)); return [float(b[0]), float(b[2]), float(-b[1])]


meta = {"id": "EldritchLongbow", "type": "bow", "height_m": LENGTH, "string_material": "BowCord",
        "string_top": gltf((STRING[0] + STRING[1] * STRING_Z[1], 0, STRING_Z[1])),
        "string_bottom": gltf((STRING[0] + STRING[1] * STRING_Z[0], 0, STRING_Z[0])),
        "nock_rest": gltf((STRING[0] + STRING[1] * GRIP[2], 0, GRIP[2])),
        "provenance": "Meshy model bow_3d_1 (the bow of her old mesh), put into the modular weapon frame"}
json.dump(meta, open(META, "w"), indent=1)
bpy.ops.object.select_all(action='DESELECT')
bow.select_set(True); cord.select_set(True)
bpy.context.view_layer.objects.active = bow
bpy.ops.object.join()                      # one object, two materials: the importer takes Doorways_<name> nodes
bow.name = "Doorways_EldritchLongbow"
bpy.ops.export_scene.gltf(filepath=OUT, export_format='GLB', use_selection=True, export_yup=True, export_apply=True)
print("BOW written", OUT, meta)
