"""Review pictures of the trees (Blender, EEVEE): every model from three sides and its foot close up, on a neutral
ground under an afternoon sun and a sky, one sheet per kind. Run by build_trees.py, or alone:

    blender -b --factory-startup -P art/trees/review.py -- <species> <seed> <lod> <textures folder> <out.png> [kind of textures]
"""
import math, os, sys
import bpy
import numpy as np
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import treegen as tg
import plants

def args():
    a = sys.argv[sys.argv.index("--") + 1:]
    return a[0], int(a[1]), int(a[2]), a[3], a[4], (a[5] if len(a) > 5 else a[0])

def mat(name, tex_dir, kind, part):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree
    bs = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"); bs.inputs["Roughness"].default_value = 0.85
    tn = nt.nodes.new("ShaderNodeTexImage"); tn.image = bpy.data.images.load(os.path.join(tex_dir, "%s_%s.png" % (kind, part)))
    nt.links.new(tn.outputs["Color"], bs.inputs["Base Color"])
    if part.startswith("leaves"):
        rn = nt.nodes.new("ShaderNodeMath"); rn.operation = "GREATER_THAN"; rn.inputs[1].default_value = 0.5
        nt.links.new(tn.outputs["Alpha"], rn.inputs[0]); nt.links.new(rn.outputs[0], bs.inputs["Alpha"])
        bs.inputs["Subsurface Weight"].default_value = 0.0
        m.use_backface_culling = False
        try: m.surface_render_method = "DITHERED"
        except Exception: pass
    else:
        nf = os.path.join(tex_dir, "%s_bark_n.png" % kind)
        if os.path.exists(nf):
            ni = bpy.data.images.load(nf); ni.colorspace_settings.name = "Non-Color"
            tnn = nt.nodes.new("ShaderNodeTexImage"); tnn.image = ni
            nm = nt.nodes.new("ShaderNodeNormalMap"); nm.inputs["Strength"].default_value = 1.0
            nt.links.new(tnn.outputs["Color"], nm.inputs["Color"]); nt.links.new(nm.outputs["Normal"], bs.inputs["Normal"])
    return m

def obj(name, arrays, material):
    P, N, T, UV, I = arrays
    me = bpy.data.meshes.new(name)
    V = [(p[0], -p[2], p[1]) for p in P]                                  # y up -> Blender's z up
    F = I.reshape(-1, 3).tolist()
    me.from_pydata(V, [], F)
    uvl = me.uv_layers.new(); lu = UV[I]
    uvl.data.foreach_set("uv", lu.ravel())
    me.normals_split_custom_set_from_vertices([(n[0], -n[2], n[1]) for n in N])
    me.materials.append(material)
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o); return o

def setup(H):
    sc = bpy.context.scene
    for o in list(bpy.data.objects): bpy.data.objects.remove(o)
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 900, 1100
    sc.view_settings.view_transform = "AgX"
    w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]; sky = w.node_tree.nodes.new("ShaderNodeTexSky")
    try: sky.sky_type = "NISHITA"
    except Exception: pass
    w.node_tree.links.new(sky.outputs["Color"], bg.inputs["Color"]); bg.inputs["Strength"].default_value = 0.12
    sun = bpy.data.lights.new("sun", "SUN"); sun.energy = 2.6; so = bpy.data.objects.new("sun", sun); sc.collection.objects.link(so)
    so.rotation_euler = (math.radians(50), 0, math.radians(35)); sun.angle = math.radians(2)
    gm = bpy.data.materials.new("ground"); gm.use_nodes = True
    gb = next(n for n in gm.node_tree.nodes if n.type == "BSDF_PRINCIPLED"); gb.inputs["Base Color"].default_value = (0.07, 0.06, 0.045, 1)
    bpy.ops.mesh.primitive_plane_add(size=H * 6); bpy.context.active_object.data.materials.append(gm)
    cam = bpy.data.cameras.new("cam"); co = bpy.data.objects.new("cam", cam); sc.collection.objects.link(co); sc.camera = co
    return co

def shoot(co, target, dist, az, el, lens, path):
    t = Vector(target); d = Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el)))
    co.location = t + d * dist; co.data.lens = lens
    co.rotation_euler = (t - co.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.render.filepath = path; bpy.ops.render.render(write_still=True)

if __name__ == "__main__":
    import json
    species, seed, lod, tex, out, kind = args()
    man = json.load(open(os.path.join(tex, "manifest.json")))
    k = man["kinds"].get(kind, {}); rects = k.get("rects", {"A": [0, .5, .5, 1], "B": [.5, .5, 1, 1], "C": [0, 0, .5, .5]})
    sw = k.get("swatches", [])
    if species in plants.SPECIES:                       # (seed: the variant; plants.py's own seed as build_trees.py gives it)
        import build_trees as bt
        bark, leaves, info = plants.build(species, bt.seed_of(species, seed), seed, lod, rects, sw)
        P = np.concatenate([m.arrays()[0] for m in (bark, leaves) if m.arrays() is not None])
        lying = info.get("size", 0) > 0
        class T: pass
        tree = T(); tree.H = float(P[:, 1].max()) if not lying else float(np.ptp(P[:, 0])) * 0.45
        tree.reach = float(np.hypot(P[:, 0], P[:, 2]).max())
    else:
        tree = tg.Tree(species, seed)
        bark, leaves = tg.build(tree, rects, lod, fruit_uv=sw[:2] if sw else None)
    co = setup(tree.H)
    obj("bark", bark.arrays(), mat("bark", tex, kind, "bark"))
    la = leaves.arrays()
    if la is not None: obj("leaves", la, mat("leaves", tex, kind, "leaves"))
    H = tree.H; stem = os.path.splitext(out)[0]
    shots = [(0.0, 0.08, 50), (2.1, 0.30, 50), (4.0, 0.62, 50)]
    files = []
    lying = species in plants.SPECIES and plants.SPECIES[species].get("lying")
    for i, (az, el, lens) in enumerate(shots):
        f = "%s_%d.png" % (stem, i)
        if lying: shoot(co, (0, 0, 0.6), H * 1.55, (0.75, 2.6, 4.3)[i], (0.22, 0.30, 0.75)[i], lens, f)
        elif species in plants.SPECIES: shoot(co, (0, 0, H * 0.4), max(H * 2.1, tree.reach * 2.5), az, el + 0.1, lens, f)
        else: shoot(co, (0, 0, H * 0.5), H * 2.1, az, el, lens, f)
        files.append(f)
    f = "%s_foot.png" % stem
    if lying: shoot(co, (H * 1.1, 0, 0.6), 4.2, 0.45, 0.30, 35, f)
    else: shoot(co, (0, 0, H * 0.04), max(H * 0.28, 2.5) if H > 3 else H * 0.9, 0.7, 0.25, 35, f)
    files.append(f)
    print("REVIEW", species, seed, lod, "bark", bark.tris, "leaves", leaves.tris, " ".join(files))
