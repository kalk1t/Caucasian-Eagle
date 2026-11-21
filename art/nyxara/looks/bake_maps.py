"""Maps of where things are on Nyxara's textures, for her looks' new colours (make_textures.py).

    blender --factory-startup -b art/nyxara/blender/Nyxara_mixamo.blend --python bake_maps.py -- <out folder>

Writes, in the UV layout of her own textures (2048 x 2048):
  body_map.png   R: how much the Head bone carries that point (her face and hair: kept as they are)
                 G: its height (0 her feet, 1 the top of her head)   B: how far from her middle line (0..1 at 0.5 m)
  wings_map.png  R: along the wing, 0 at its root on her back, 1 at its tip
                 G: across it, 0 at the leading edge (top) to 1 at the trailing edge (bottom)
                 B: which wing (0 her left, 1 her right)
"""
import bpy, bmesh, sys, os
import numpy as np
from mathutils import Vector

OUT = sys.argv[sys.argv.index("--") + 1]
os.makedirs(OUT, exist_ok=True)
SIZE = 2048
arm = bpy.data.objects["Nyxara_rig"]
body = bpy.data.objects["Nyxara"]
bones = arm.data.bones
for pb in arm.pose.bones:   # the rest pose: the maps are of her as the mesh lies
    pb.matrix_basis.identity()
bpy.context.view_layer.update()


def part(material_name, values):
    """A copy of her mesh with only that material's faces, its points coloured by values(index, co)."""
    me = body.data.copy()
    o = bpy.data.objects.new("bake_" + material_name, me)
    bpy.context.scene.collection.objects.link(o)
    keep = [i for i, m in enumerate(me.materials) if m and m.name == material_name][0]
    bm = bmesh.new(); bm.from_mesh(me)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index != keep], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(me); bm.free()
    me.materials.clear()
    attr = me.color_attributes.new("LookMap", "FLOAT_COLOR", "POINT")
    for i, v in enumerate(me.vertices):
        attr.data[i].color = (*values(v), 1.0)
    return o


def bake_and_save(o, name):
    mat = bpy.data.materials.new("bake_" + name)
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    attr = nt.nodes.new("ShaderNodeAttribute"); attr.attribute_name = "LookMap"
    emit = nt.nodes.new("ShaderNodeEmission")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(attr.outputs["Color"], emit.inputs["Color"])
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    img = bpy.data.images.new(name, SIZE, SIZE, alpha=False, float_buffer=True)
    img.colorspace_settings.name = "Non-Color"
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img
    nt.nodes.active = tex
    o.data.materials.append(mat)
    for x in bpy.context.scene.objects:
        x.select_set(x is o)
    bpy.context.view_layer.objects.active = o
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 1
    sc.render.bake.margin = 6
    bpy.ops.object.bake(type="EMIT")
    path = os.path.join(OUT, name + ".png")
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_depth = "16"
    sc.render.image_settings.color_mode = "RGB"
    img.save_render(path)
    print("WROTE", path)


# ---- her body: the Head bone's share, the height, the distance from her middle
groups = {g.name: g.index for g in body.vertex_groups}
head = {groups["Head"]}
top = (arm.matrix_world @ bones["HeadTop_End"].head_local).z


def body_values(v):
    w = sum(g.weight for g in v.groups if g.group in head)
    return (min(1.0, w), max(0.0, min(1.0, v.co.z / top)), min(1.0, abs(v.co.x) / 0.5))


bake_and_save(part("Nyxara", body_values), "body_map")

# ---- her wings: along each from its root to its tip, across from its top edge to its bottom
side_groups = ({groups["WingL1"], groups["WingL2"], groups["WingL3"]}, {groups["WingR1"], groups["WingR2"], groups["WingR3"]})
lines = []
for s in ("L", "R"):
    a = arm.matrix_world @ bones["Wing%s1" % s].head_local
    b = arm.matrix_world @ bones["Wing%s3" % s].tail_local
    lines.append((a, (b - a).normalized(), (b - a).length))
wing_me = body.data
ts = {0: [], 1: []}
for v in wing_me.vertices:
    wl = sum(g.weight for g in v.groups if g.group in side_groups[0])
    wr = sum(g.weight for g in v.groups if g.group in side_groups[1])
    if wl + wr < 0.5:
        continue
    k = 0 if wl >= wr else 1
    a, d, n = lines[k]
    ts[k].append((v.co - a).dot(d))
reach = [max(ts[k]) if ts[k] else 1 for k in (0, 1)]


def wing_values(v):
    wl = sum(g.weight for g in v.groups if g.group in side_groups[0])
    wr = sum(g.weight for g in v.groups if g.group in side_groups[1])
    k = 0 if wl >= wr else 1
    a, d, n = lines[k]
    p = v.co - a
    t = max(0.0, min(1.0, p.dot(d) / reach[k]))
    # across: the height of the point under the line from root to tip (the top edge is near it, the trailing
    # feathers hang below), as a share of 0.9 m
    off = p - d * p.dot(d)
    across = max(0.0, min(1.0, -off.z / 0.9 + 0.1))
    return (t, across, float(k))


bake_and_save(part("Wings", wing_values), "wings_map")
print("REACH", reach)
