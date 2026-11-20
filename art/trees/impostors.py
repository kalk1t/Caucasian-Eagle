"""Impostors (0.5.2): every tree variant pictured once from the side, its colours only (no sun: the game lights it), on a
transparent ground, for the game's far trees (three crossed quads carrying this picture: AscentTrees).

    blender -b --factory-startup -P art/trees/impostors.py -- <textures folder> <out folder> <jobs.json>

jobs.json: [[model, species, texture kind, variant, snow 0/1, seed], ...]. Writes <out>/<model>_<variant>[_snow].png
(SLOT_W x SLOT_H) and <out>/impostors.json: for each picture the frame it shows, in the model's metres: [width, height]
(the foot at the bottom's middle).
"""
import json, math, os, sys
import bpy
import numpy as np
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import treegen as tg

SLOT_W, SLOT_H = 192, 384

def material(name, picture, cut):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree
    for n in list(nt.nodes):
        if n.type != "OUTPUT_MATERIAL": nt.nodes.remove(n)
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = picture
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion"); ao.inputs["Distance"].default_value = 1.5
    shade = nt.nodes.new("ShaderNodeMapRange"); shade.inputs["To Min"].default_value = 0.55; shade.inputs["To Max"].default_value = 1.0
    mul = nt.nodes.new("ShaderNodeMix"); mul.data_type = "RGBA"; mul.blend_type = "MULTIPLY"; mul.inputs["Factor"].default_value = 1.0
    em = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(tex.outputs["Color"], ao.inputs["Color"]); nt.links.new(ao.outputs["AO"], shade.inputs["Value"])
    nt.links.new(tex.outputs["Color"], mul.inputs["A"]); nt.links.new(shade.outputs["Result"], mul.inputs["B"])
    nt.links.new(mul.outputs["Result"], em.inputs["Color"])
    if cut:
        tr = nt.nodes.new("ShaderNodeBsdfTransparent"); mix = nt.nodes.new("ShaderNodeMixShader")
        gt = nt.nodes.new("ShaderNodeMath"); gt.operation = "GREATER_THAN"; gt.inputs[1].default_value = 0.5
        nt.links.new(tex.outputs["Alpha"], gt.inputs[0]); nt.links.new(gt.outputs[0], mix.inputs[0])
        nt.links.new(tr.outputs[0], mix.inputs[1]); nt.links.new(em.outputs[0], mix.inputs[2]); nt.links.new(mix.outputs[0], out.inputs["Surface"])
        try: m.surface_render_method = "DITHERED"
        except Exception: pass
        m.use_backface_culling = False
    else:
        nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m

def mesh_obj(name, arrays, mat):
    P, N, T, UV, I = arrays
    me = bpy.data.meshes.new(name)
    me.from_pydata([(p[0], -p[2], p[1]) for p in P], [], I.reshape(-1, 3).tolist())
    uvl = me.uv_layers.new(); uvl.data.foreach_set("uv", UV[I].ravel())
    me.materials.append(mat)
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o); return o

def main():
    a = sys.argv[sys.argv.index("--") + 1:]
    tex, out, jobs = a[0], a[1], json.load(open(a[2]))
    os.makedirs(out, exist_ok=True)
    man = json.load(open(os.path.join(tex, "manifest.json")))["kinds"]
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"; sc.render.film_transparent = True
    sc.render.resolution_x, sc.render.resolution_y = SLOT_W * 2, SLOT_H * 2      # rendered at twice the size, scaled down later
    sc.view_settings.view_transform = "Standard"
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True; w.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.0
    cam = bpy.data.cameras.new("cam"); cam.type = "ORTHO"; cam.clip_end = 2000.0; co = bpy.data.objects.new("cam", cam); sc.collection.objects.link(co); sc.camera = co
    pics = {}
    frames = {}
    if os.path.exists(os.path.join(out, "impostors.json")): frames = json.load(open(os.path.join(out, "impostors.json")))
    for model, species, kind, v, snow, seed in jobs:
        for o in list(bpy.data.objects):
            if o.type == "MESH": bpy.data.objects.remove(o)
        k = man[kind]; rects = k.get("rects", {"A": [0, .5, .5, 1], "B": [.5, .5, 1, 1], "C": [0, 0, .5, .5]})
        tree = tg.Tree(species, seed)
        sw = k.get("swatches") or None
        bark, leaves = tg.build(tree, rects, 1, fruit_uv=sw[:2] if sw else None)
        def pic(name):
            if name not in pics: pics[name] = bpy.data.images.load(os.path.join(tex, name))
            return pics[name]
        mesh_obj("bark", bark.arrays(), material("bark_" + kind, pic("%s_bark.png" % kind), False))
        la = leaves.arrays()
        if la is not None:
            mesh_obj("leaves", la, material("leaves_%s_%d" % (kind, snow), pic("%s_leaves%s.png" % (kind, "_snow" if snow else "")), True))
        P = np.concatenate([bark.arrays()[0]] + ([la[0]] if la is not None else []))
        half = float(np.abs(P[:, [0, 2]]).max()) * 1.04; top = float(P[:, 1].max()) * 1.02
        fh = max(top, half * 2 * SLOT_H / SLOT_W); fw = fh * SLOT_W / SLOT_H
        cam.ortho_scale = fh                                     # (the larger side: the height)
        co.location = Vector((0, -200, fh * 0.5)); co.rotation_euler = (math.radians(90), 0, 0)
        name = "%s_%d%s" % (model, v, "_snow" if snow else "")
        sc.render.filepath = os.path.join(out, name + ".png"); bpy.ops.render.render(write_still=True)
        frames[name] = [fw, fh]
        print("IMPOSTOR", name, "%.1f x %.1f m" % (fw, fh)); sys.stdout.flush()
    json.dump(frames, open(os.path.join(out, "impostors.json"), "w"), indent=0)

main()
