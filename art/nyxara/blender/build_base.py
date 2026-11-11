# Step 1: Nyxara base rig + imported Meshy actions (metres, in-place).
import bpy, sys, math, pathlib
from mathutils import Vector, Matrix
HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE.parent / "meshy" / "out"
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps = 30
bpy.ops.import_scene.gltf(filepath=str(OUT / "rig_1" / "result_rigged_character_glb_url.glb"))
for o in list(bpy.data.objects):
    if o.type == 'MESH' and o.name.startswith("Icosphere"): bpy.data.objects.remove(o)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE'); arm.name = "NyxaraRig"; arm.data.name = "NyxaraRig"
body = next(o for o in bpy.data.objects if o.type == 'MESH'); body.name = "NyxaraBody"
for a in list(bpy.data.actions): bpy.data.actions.remove(a)
# Apply the 0.01 armature scale so bones are in metres.
bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True); body.select_set(True); bpy.context.view_layer.objects.active = arm
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
print("ARM scale", tuple(arm.scale), "BODY mw", [tuple(round(x,3) for x in r) for r in body.matrix_world])
SRC = {  # clip name: (dir, loop)
 "Idle": ("anim/89", True), "WalkWoman": ("anim/1", True), "WalkMan": ("rig_1/walking", True), "Run": ("rig_1/running", True),
 "WalkBackward": ("anim/546", True), "JumpSrc": ("anim/466", False), "JumpOpenSrc": ("anim/460", False),
 "BasicAttack": ("anim/219", False), "SlashLeft": ("anim/97", False), "HeavyAttack": ("anim/242", False),
 "Skill1": ("anim/240", False), "Skill2": ("anim/91", False), "Skill3": ("anim/102", False),
 "Combo1L": ("anim/92", False), "Combo1R": ("anim/221", False), "Combo2L": ("anim/105", False),
 "Combo2R": ("anim/238", False), "Combo3L": ("anim/241", False), "Combo3R": ("anim/86", False),
 "Guard": ("anim/149", False), "Dodge": ("anim/158", False), "HitReact": ("anim/178", False),
 "Death": ("anim/189", False), "RespawnSrc": ("anim/100", False), "ShoutSrc": ("anim/101", False)}
def glb(d):
    if d.startswith("rig_1/"):
        k = d.split("/")[1]; return OUT / "rig_1" / f"result_basic_animations_{k}_armature_glb_url.glb"
    return OUT / d / "result_animation_glb_url.glb"
hips_rest = arm.data.bones["Hips"].matrix_local.to_3x3()
R, Ri = hips_rest, hips_rest.inverted()
for name, (d, loop) in SRC.items():
    before = set(bpy.data.actions); objs = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(glb(d)))
    new = [a for a in bpy.data.actions if a not in before]
    for o in set(bpy.data.objects) - objs: bpy.data.objects.remove(o)
    act = new[0]
    for extra in new[1:]: bpy.data.actions.remove(extra)
    act.name = name; act.use_fake_user = True
    # Collect hips location channels (bone-local, centimetre units at import scale).
    fcs = {}
    for fc in act.fcurves if hasattr(act, "fcurves") else []:
        pass
    chans = []
    for layer in act.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                chans += list(bag.fcurves)
    loc = {fc.array_index: fc for fc in chans if fc.data_path == 'pose.bones["Hips"].location'}
    for fc in chans:  # every location channel is in centimetres
        if fc.data_path.endswith(".location"):
            for kp in fc.keyframe_points:
                kp.co.y *= 0.01; kp.handle_left.y *= 0.01; kp.handle_right.y *= 0.01
    if len(loc) == 3:
        n = len(loc[0].keyframe_points)
        P = [R @ Vector([loc[i].keyframe_points[k].co.y for i in range(3)]) for k in range(n)]
        T = [loc[0].keyframe_points[k].co.x for k in range(n)]
        if loop:   # remove linear drift only
            d0, d1 = P[0], P[-1]
            drift = [(d0 + (d1 - d0) * (k / max(1, n - 1))) for k in range(n)]
            Q = [P[k] - Vector((drift[k].x - d0.x, drift[k].y - d0.y, 0)) for k in range(n)]
        else:      # high-pass: keep sway, drop travel (0.5 s window)
            half = 7
            Q = []
            for k in range(n):
                win = P[max(0, k - half):k + half + 1]
                avg = sum(win, Vector()) / len(win)
                Q.append(Vector((P[k].x - avg.x + P[0].x, P[k].y - avg.y + P[0].y, P[k].z)))
        for k in range(n):
            L = Ri @ Q[k]
            for i in range(3):
                kp = loc[i].keyframe_points[k]; dy = L[i] - kp.co.y
                kp.co.y = L[i]; kp.handle_left.y += dy; kp.handle_right.y += dy
    print("CLIP", name, tuple(act.frame_range), "hips keys", len(loc.get(0).keyframe_points) if loc else 0)
arm.animation_data_create()
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / "Nyxara_base.blend"))
print("SAVED")
