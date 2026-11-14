"""Her finishing blow, as a clip of its own, made from clips she already has.

    blender -b art/nyxara/blender/Nyxara_game.blend -P art/nyxara/blender/finishers.py -- <out.glb>

Finisher: the rising strike of Combo1R flows into the leap and the slam of Combo3R. The pieces are sampled frame by
frame from the finished actions of Nyxara_game.blend (the file assemble.py saves) and cross-faded, so the skeleton and
every other clip stay what they are. The .blend is not saved. Compile the GLB with
`DoorwaysAssetBuilder character-pipeline <out.glb> <fresh dir> Idle BasicAttack Finisher` and copy Finisher.danim
into each body's bundle.
"""
import bpy, sys
import numpy as np

OUT = sys.argv[sys.argv.index("--") + 1]
FPS = 30
# (action, from s, to s) pieces; each begins FADE s before the one before it ends
PIECES = (("Combo1R", 0.40, 1.30), ("Combo3R", 0.45, 2.50))
FADE = 0.20


def fcurves(act):
    out = []
    for layer in act.layers:
        for strip in layer.strips:
            for bag in strip.channelbags: out += list(bag.fcurves)
    return out


def curves(name):
    return {(fc.data_path, fc.array_index): fc for fc in fcurves(bpy.data.actions[name])}


src = [curves(p[0]) for p in PIECES]
keys = sorted(set().union(*[set(c) for c in src]))
starts = [0.0]
for i in range(1, len(PIECES)): starts.append(starts[-1] + (PIECES[i - 1][2] - PIECES[i - 1][1]) - FADE)
total = starts[-1] + PIECES[-1][2] - PIECES[-1][1]
n = int(round(total * FPS)) + 1


def value(piece, key, t):
    """The piece's channel at finisher time t (clamped to the piece)."""
    name, a, b = PIECES[piece]
    fc = src[piece].get(key)
    if fc is None: return 1.0 if key[0].endswith("rotation_quaternion") and key[1] == 0 else (1.0 if key[0].endswith("scale") else 0.0)
    return fc.evaluate(1 + min(max(a + t - starts[piece], a), b) * FPS)


data = {k: np.zeros(n) for k in keys}
paths = sorted({k[0] for k in keys})
for f in range(n):
    t = f / FPS
    piece = max(i for i in range(len(PIECES)) if t >= starts[i])
    w = 1.0
    if piece > 0:
        x = min(max((t - starts[piece]) / FADE, 0.0), 1.0); w = x * x * (3 - 2 * x)
    for path in paths:
        idx = [k[1] for k in keys if k[0] == path]
        new = np.array([value(piece, (path, i), t) for i in idx])
        if w < 1.0:
            old = np.array([value(piece - 1, (path, i), t) for i in idx])
            if path.endswith("rotation_quaternion"):
                if float(old @ new) < 0: new = -new
                new = old * (1 - w) + new * w; new /= max(np.linalg.norm(new), 1e-9)
            else: new = old * (1 - w) + new * w
        for i, v in zip(idx, new): data[(path, i)][f] = v

arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
body = next(o for o in bpy.data.objects if o.type == 'MESH' and o.find_armature() == arm)
if "Finisher" in bpy.data.actions: bpy.data.actions.remove(bpy.data.actions["Finisher"])
act = bpy.data.actions.new("Finisher"); act.use_fake_user = True
slot = act.slots.new(id_type='OBJECT', name="NyxaraRig")
bag = act.layers.new("Layer").strips.new(type='KEYFRAME').channelbag(slot, ensure=True)
for (path, i), vals in data.items():
    fc = bag.fcurves.new(path, index=i, group_name=path.split('"')[1] if '"' in path else "")
    fc.keyframe_points.add(n)
    co = np.empty(2 * n); co[0::2] = np.arange(n) + 1; co[1::2] = vals
    fc.keyframe_points.foreach_set("co", co)
    for key in fc.keyframe_points: key.interpolation = "LINEAR"
    fc.update()
for a in list(bpy.data.actions):          # only what the compile needs: the export of 32 clips takes minutes
    if a.name not in ("Idle", "BasicAttack", "Finisher"): bpy.data.actions.remove(a)
print("FINISHER frames", n, "seconds", round((n - 1) / FPS, 3), "pieces start", [round(s, 3) for s in starts],
      "hits (clip s)", [round(starts[0] + 1.03 - PIECES[0][1], 3), round(starts[1] + 1.32 - PIECES[1][1], 3)])
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.gltf(filepath=OUT, export_format='GLB', use_selection=True,
                          export_animation_mode='ACTIONS', export_skins=True, export_influence_nb=4,
                          export_all_influences=False, export_def_bones=False, export_force_sampling=True,
                          export_frame_step=1, export_anim_single_armature=True, export_yup=True,
                          export_image_format='AUTO', export_materials='EXPORT')
print("EXPORTED", OUT)
