"""Nyxara from Mixamo: one model with the skin of Idle.fbx and the clips Idle, Walk, Run, Death, Jump.

Reads the four Mixamo downloads in art/nyxara and writes Nyxara_mixamo.glb (for the engine's character
importer) and Nyxara_mixamo.blend (to look at, and to add hand-made clips to) beside this script.

Run:  blender --factory-startup -b --python build_from_mixamo.py

What it changes on the way:
- bone names lose Mixamo's "mixamorig:" prefix (Hips, Spine1, LeftArm ...; the engine knows these names);
- the size Mixamo wrote (centimetres) is applied, so the model is in metres again, as in the Blender source;
- Walk, Run and Jump are made to stay in place (the game moves her, the clip only moves her legs);
  the speed each clip was walking at is printed, the lab uses it so that her feet do not slide;
- every clip starts at frame 0;
- BasicAttack is a placeholder (one second of Idle): the engine's importer wants a clip of that name. It goes
  when the first real attack exists.
"""
import bpy, math, sys
from pathlib import Path
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
SRC = HERE.parent
FPS = 30
# clip, file, what happens to her travel over the ground:
#   False  nothing (the clip ends where it starts, or she stays where she falls)
#   True   a loop: the steady travel is taken out, the clip stays in place
#   "root" an attack that steps or leaps forward: her path over the ground is taken out of the clip and written
#          to NyxaraLabClips.h, and the lab moves her along it (so the ground and walls still count)
CLIPS = (
    ("Idle", "Idle.fbx", False),
    ("Walk", "Strut Walking.fbx", True),
    ("Run", "Running.fbx", True),          # the user's choice for W and for Shift, after trying Run Forward.fbx (a bigger, bouncier run made for 3.58 m/s)
    ("Death", "Standing React Death Forward.fbx", False),
    ("Jump", "Running Jump.fbx", True),
    # left sword
    ("Attack_L1", "attacks/Standing Torch Melee Attack 01.fbx", False),
    ("Attack_L2", "attacks/Standing Torch Melee Attack Stab.fbx", False),
    ("Attack_L3", "attacks/Standing Torch Melee Attack 03.fbx", False),
    ("Attack_L_Step", "attacks/Standing Torch Melee Attack 04.fbx", "root"),
    ("Attack_L_Run", "attacks/Standing Torch Melee Attack 05.fbx", "root"),
    # right sword
    ("Attack_R1", "attacks/right hand attack 03.fbx", False),
    ("Attack_R2", "attacks/right hand attack 01.fbx", False),
    ("Attack_R3", "attacks/right hand attack 02.fbx", False),
    ("Attack_R4", "attacks/Standing Melee Attack 360 High.fbx", False),
    ("Attack_R_Step", "attacks/Sword And Shield Attack.fbx", "root"),
    ("Attack_R_Run", "attacks/right hand attack 04.fbx", "root"),
    ("Attack_Jump", "attacks/jump attack.fbx", "root"),
    # skills (the lab: C and F). Not "Attack": the passes made for sword cuts (aim, guard, head) leave them alone
    ("Skill_Kick", "Hurricane Kick.fbx", "root"),
    ("Skill_Throw", "Flying Shoulder Throw.fbx", "root"),
    ("Skill_Slide", "Running Slide.fbx", "root"),
    ("Skill_Cast", "Standing 2H Magic Attack 05.fbx", "root"),
)
PREFIX = "mixamorig:"
for c in CLIPS:
    if not (SRC / c[1]).exists():
        print("MISSING", c[1], "- the clip", c[0], "is left out")
CLIPS = tuple(c for c in CLIPS if (SRC / c[1]).exists())


def curves(action):
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                yield from bag.fcurves


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps = FPS
arm = body = None
actions = {}
for name, file, _ in CLIPS:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(SRC / file))
    new = [o for o in bpy.data.objects if o not in before]
    rig = [o for o in new if o.type == "ARMATURE"][0]
    act = rig.animation_data.action
    act.name = name
    act.use_fake_user = True
    actions[name] = act
    if arm is None:  # the first file gives the skin and the skeleton
        arm = rig
        body = [o for o in new if o.type == "MESH"][0]
    else:
        for o in new:
            bpy.data.objects.remove(o)

# ---- names
for b in arm.data.bones:
    if b.name.startswith(PREFIX):
        b.name = b.name[len(PREFIX):]
for act in actions.values():
    for fc in curves(act):
        fc.data_path = fc.data_path.replace(PREFIX, "")
arm.name = "Nyxara_rig"
arm.data.name = "Nyxara_rig"
body.name = "Nyxara"
body.data.name = "Nyxara"

# ---- size and direction: metres, standing on Z, transforms applied
k = 1.0 / arm.scale.x
world = body.matrix_world.copy()
body.parent = None
arm.scale = (1, 1, 1)
body.matrix_world = Matrix.Scale(k, 4) @ world
bpy.context.view_layer.update()
for o in bpy.data.objects:
    o.select_set(o in (arm, body))
bpy.context.view_layer.objects.active = arm
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
body.parent = arm
body.matrix_parent_inverse = Matrix.Identity(4)
for m in body.modifiers:
    if m.type == "ARMATURE":
        m.object = arm

# ---- clips: start at 0, Walk and Run in place
speeds = {}
roots = {}      # clip -> metres forward at every frame (taken out of the clip)
HIPS = 'pose.bones["Hips"].location'
for name, _, in_place in CLIPS:
    act = actions[name]
    first = min(fc.keyframe_points[0].co[0] for fc in curves(act))
    last = max(fc.keyframe_points[-1].co[0] for fc in curves(act))
    if in_place == "root":
        # her hips' path over the ground, smoothed over 0.3 s so that the sway of her body stays in the clip
        to_world = arm.data.bones["Hips"].matrix_local.to_3x3()
        ch = {fc.array_index: fc for fc in curves(act) if fc.data_path == HIPS}
        n = int(last - first) + 1
        local = [Vector([ch[i].evaluate(first + f) for i in range(3)]) for f in range(n)]
        world = [to_world @ (v - local[0]) for v in local]
        smooth = []
        for f in range(n):
            w = [world[min(max(g, 0), n - 1)] for g in range(f - 4, f + 5)]
            smooth.append(Vector((sum(v.x for v in w) / 9, sum(v.y for v in w) / 9, 0)))
        smooth = [v - smooth[0] for v in smooth]
        back = to_world.inverted()
        for i in range(3):
            for p in ch[i].keyframe_points:
                f = min(max(int(round(p.co[0] - first)), 0), n - 1)
                dy = -(back @ smooth[f])[i]
                p.co = (p.co[0], p.co[1] + dy)
                p.handle_left = (p.handle_left[0], p.handle_left[1] + dy)
                p.handle_right = (p.handle_right[0], p.handle_right[1] + dy)
        roots[name] = [-v.y for v in smooth]          # she looks along -Y
        in_place = False
    for fc in curves(act):
        hips = fc.data_path == HIPS
        a, b = fc.keyframe_points[0].co[1], fc.keyframe_points[-1].co[1]
        drift = b - a if (hips and in_place and abs(b - a) > 0.2) else 0.0
        if drift:
            speeds[name] = abs(drift) / ((last - first) / FPS)
        for p in fc.keyframe_points:
            t = (p.co[0] - first) / (last - first)
            dx, dy = -first, -drift * t
            p.co = (p.co[0] + dx, p.co[1] + dy)
            p.handle_left = (p.handle_left[0] + dx, p.handle_left[1] + dy)
            p.handle_right = (p.handle_right[0] + dx, p.handle_right[1] + dy)
        fc.update()
    print("CLIP %-6s %3d frames  %.3f s%s" % (name, last - first, (last - first) / FPS,
                                               "  in place, was moving at %.3f m/s" % speeds[name] if name in speeds else ""))

# ---- Walk centred: Mixamo's strut carries her to one side of the line she stands on (left foot far out, right foot
# on the line), so she hopped sideways when she began to walk. Her hips are moved so her feet are either side of it.
for name in ("Walk",):
    act = actions[name]
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and act.slots:
        arm.animation_data.action_slot = act.slots[0]
    last = int(max(fc.keyframe_points[-1].co[0] for fc in curves(act)))
    off = 0.0
    for f in range(last):
        bpy.context.scene.frame_set(f)
        off += (arm.pose.bones["LeftFoot"].head.x + arm.pose.bones["RightFoot"].head.x) / 2 / last
    move = arm.data.bones["Hips"].matrix_local.to_3x3().inverted() @ Vector((-off, 0, 0))
    for fc in curves(act):
        if fc.data_path == HIPS:
            for p in fc.keyframe_points:
                d = move[fc.array_index]
                p.co = (p.co[0], p.co[1] + d)
                p.handle_left = (p.handle_left[0], p.handle_left[1] + d)
                p.handle_right = (p.handle_right[0], p.handle_right[1] + d)
            fc.update()
    print("CENTRED %s: her feet were %.1f cm to one side, now on her line" % (name, off * 100))

# ---- room for her arms: Mixamo's run swings the fists across her chest, into the armour. Each upper arm is turned
# outwards by the same angle on every frame (what Mixamo's "Character Arm-Space" slider does), so the swing stays.
ARM_SPACE = {"Death": 40.0, "Run": 14.0}      # clip: degrees each upper arm is turned outwards (Mixamo folds her arms into her chest)
ELBOW_OPEN = {}     # clip: degrees the forearm is let down from the elbow


def hands_from_middle(act, last):
    """How far her elbows and her hands are from her middle line, on average over the clip (m)."""
    e = h = 0.0
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        for side in ("Left", "Right"):
            e += abs((arm.matrix_world @ arm.pose.bones[side + "ForeArm"].head).x) / (2 * (last + 1))
            h += abs((arm.matrix_world @ arm.pose.bones[side + "Hand"].tail).x) / (2 * (last + 1))
    return "elbows %.3f hands %.3f" % (e, h)


for name, degrees in ARM_SPACE.items():
    act = actions[name]
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and act.slots:
        arm.animation_data.action_slot = act.slots[0]
    last = int(max(fc.keyframe_points[-1].co[0] for fc in curves(act)))
    before = hands_from_middle(act, last)
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        for bone, sign in (("LeftArm", -1), ("RightArm", 1)):
            pb = arm.pose.bones[bone]
            m = pb.matrix.copy()
            pivot = Matrix.Translation(m.translation)
            chest = arm.pose.bones["Spine2"]      # outwards as her chest stands at this moment, also when she lies
            ahead = (chest.matrix.to_3x3() @ chest.bone.matrix_local.to_3x3().inverted() @ Vector((0, 1, 0))).normalized()
            pb.matrix = pivot @ Matrix.Rotation(math.radians(sign * degrees), 4, ahead) @ pivot.inverted() @ m
            bpy.context.view_layer.update()
            pb.keyframe_insert("rotation_quaternion", frame=f)
            down = ELBOW_OPEN.get(name, 0.0)
            if down:
                fore = arm.pose.bones[bone.replace("Arm", "ForeArm")]
                u = (pb.tail - pb.head).normalized()
                v = (fore.tail - fore.head).normalized()
                hinge = u.cross(v)
                bend = math.degrees(u.angle(v))
                if hinge.length > 1e-4 and bend > 12:   # never past a straight arm
                    m = fore.matrix.copy()
                    pivot = Matrix.Translation(m.translation)
                    fore.matrix = pivot @ Matrix.Rotation(-math.radians(min(down, bend - 10)), 4, hinge.normalized()) @ pivot.inverted() @ m
                    bpy.context.view_layer.update()
                    fore.keyframe_insert("rotation_quaternion", frame=f)
    print("ARMS %s turned out %.0f degrees: from her middle line: %s -> %s (shoulders at %.3f)"
          % (name, degrees, before, hands_from_middle(act, last), abs(arm.data.bones["LeftArm"].head_local.x)))

# ---- arms borrowed from another clip: Run takes the arm movement of Walk (the user's choice: the run's own arms
# pull her fists into her chest). Shoulders, arms, hands and fingers are copied; legs and body stay the clip's own.
# One stride of the giving clip is spread over one stride of the taking clip, shifted so that the same foot leads.
ARMS_FROM = {"Run": "Walk"}
# How much of the giving clip's arms is taken. 1 was used first (Walk's arms alone): her arms hung still while her
# legs ran. 0 is the run's own arms: a sprinter's pumping, fists at her chest and the swords flailing. Half of each:
# the arms swing with the run, lower and wider.
ARMS_SHARE = {"Run": 0.5}


def use(act):
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and act.slots:
        arm.animation_data.action_slot = act.slots[0]
    return int(max(fc.keyframe_points[-1].co[0] for fc in curves(act)))


def at(frame):
    bpy.context.scene.frame_set(int(math.floor(frame)), subframe=frame - math.floor(frame))


def left_foot_ahead(act, n=40):
    last = use(act)
    out = []
    for i in range(n):
        at(last * i / n)
        out.append(arm.pose.bones["RightFoot"].head.y - arm.pose.bones["LeftFoot"].head.y)   # she faces -Y
    return out


ARM_BONES = [b.name for b in arm.data.bones if b.name.startswith(("Left", "Right")) and
             any(k in b.name for k in ("Shoulder", "Arm", "Hand"))]
for taker, giver in ARMS_FROM.items():
    a_, b_ = left_foot_ahead(actions[taker]), left_foot_ahead(actions[giver])
    n = len(a_)
    shift = max(range(n), key=lambda k: sum(a_[i] * b_[(i + k) % n] for i in range(n))) / n
    last_g = use(actions[giver])
    last_t = int(max(fc.keyframe_points[-1].co[0] for fc in curves(actions[taker])))
    poses, holds = [], []
    chest = arm.pose.bones["Spine2"]
    for f in range(last_t + 1):
        at(((f / last_t + shift) % 1.0) * last_g)
        poses.append({b: arm.pose.bones[b].rotation_quaternion.copy() for b in ARM_BONES})
        # how each hand is turned against her chest: the way she holds the swords, kept whatever the arm does
        holds.append({h: chest.matrix.to_3x3().inverted() @ arm.pose.bones[h].matrix.to_3x3() for h in ("LeftHand", "RightHand")})
    use(actions[taker])
    share = ARMS_SHARE.get(taker, 1.0)
    for f in range(last_t + 1):
        bpy.context.scene.frame_set(f)
        g = f if f < last_t else 0                     # the last frame is the first again: the loop closes
        for b in ARM_BONES:
            pb = arm.pose.bones[b]
            swings = b.endswith(("Shoulder", "Arm"))   # shoulder, upper arm, forearm: shared; hands and fingers: the giver's
            pb.rotation_quaternion = pb.rotation_quaternion.slerp(poses[g][b], share) if swings else poses[g][b]
            pb.keyframe_insert("rotation_quaternion", frame=f)
        if share < 1.0:
            bpy.context.view_layer.update()
            for h in ("LeftHand", "RightHand"):
                pb = arm.pose.bones[h]
                pb.matrix = Matrix.Translation(pb.matrix.translation) @ (chest.matrix.to_3x3() @ holds[g][h]).to_4x4()
                bpy.context.view_layer.update()
                pb.keyframe_insert("rotation_quaternion", frame=f)
    print("ARMS of %s now partly from %s (%d bones), %s shifted by %.3f of a stride" % (taker, giver, len(ARM_BONES), giver, shift))

# ---- the placeholder the importer asks for
attack = actions["Attack_R1" if "Attack_R1" in actions else "Idle"].copy()
attack.name = "BasicAttack"
attack.use_fake_user = True
actions["BasicAttack"] = attack

# ---- one plain material: the colour picture, not shiny, not see-through
image = [i for i in bpy.data.images if i.size[0] > 0][0]
image.name = "Nyxara_color"
mat = bpy.data.materials.new("Nyxara")
mat.use_nodes = True
bsdf = mat.node_tree.nodes["Principled BSDF"]
tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
tex.image = image
mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 0.75
bsdf.inputs["Metallic"].default_value = 0.0
body.data.materials.clear()
body.data.materials.append(mat)

# ---- a sword in each fist. The sword model (art/nyxara/Sword.glb, a curved one-edged blade) is made 0.908 m long
# (30 % smaller than the 1.297 m it had in the user's Blender file), its handle is laid across each palm under the
# knuckles, the fingers and the thumb are closed on it, and the wrist is turned so that in Idle the blade stands up,
# leaning SLANT degrees forward. The closed fingers and the wrist turn are then written into every frame of every
# clip, and the swords become part of her mesh, carried by the hand bones.
SWORD_LENGTH = 0.908
SLANT = float(sys.argv[sys.argv.index("--") + 1]) if "--" in sys.argv else 40.0
HANDLE = (0.096, 0.197)      # the handle, as parts of the sword's length from the pommel's end
import numpy as np
from mathutils import Quaternion

arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.matrix_basis = Matrix.Identity(4)
bpy.context.view_layer.update()
PB = arm.pose.bones


def world_points(o):
    w = np.empty(len(o.data.vertices) * 3, "f4")
    o.data.vertices.foreach_get("co", w)
    m = np.array(o.matrix_world)
    return w.reshape(-1, 3).astype("f8") @ m[:3, :3].T + m[:3, 3]


def sword_frame(o):
    """Handle centre, handle direction (pommel to blade), handle radius, the way the cutting edge faces, length."""
    w = world_points(o)
    c = w.mean(0)
    ax = np.linalg.eigh(np.cov((w[::5] - c).T))[1][:, -1]
    t = (w - c) @ ax
    if abs(t.min()) > abs(t.max()):
        ax, t = -ax, -t
    t = t - t.min()
    length = t.max()
    g = w[(t >= HANDLE[0] * length) & (t <= HANDLE[1] * length)]
    gc = g.mean(0)
    ga = np.linalg.eigh(np.cov((g - gc).T))[1][:, -1]
    if ga @ ax < 0:
        ga = -ga
    radius = float(np.percentile(np.linalg.norm((g - gc) - np.outer((g - gc) @ ga, ga), axis=1), 75))
    tip = w[t > 0.97 * length].mean(0) - gc
    back = tip - ga * (tip @ ga)          # a sabre's point lies back from the handle's line; the edge faces the other way
    return Vector(gc), Vector(ga).normalized(), radius, (-Vector(back)).normalized(), length, Vector(gc + tip)


def turn(pb, q, pivot):
    m = Matrix.Translation(pivot) @ q.to_matrix().to_4x4() @ Matrix.Translation(-pivot)
    pb.matrix = m @ pb.matrix
    bpy.context.view_layer.update()


def off_line(p, c, d):
    v = p - c
    return (v - d * v.dot(d)).length


before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=str(SRC / "Sword.glb"))
sword0 = [o for o in bpy.data.objects if o not in before and o.type == "MESH"][0]
for o in [o for o in bpy.data.objects if o not in before and o is not sword0]:
    bpy.data.objects.remove(o)
sword0.parent = None
for o in bpy.data.objects:
    o.select_set(o is sword0)
bpy.context.view_layer.objects.active = sword0
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
dec = sword0.modifiers.new("d", "DECIMATE")
dec.ratio = 14000 / len(sword0.data.polygons)
bpy.ops.object.modifier_apply(modifier="d")
sword0.material_slots[0].material.name = "Sword"
sword0.data.uv_layers[0].name = body.data.uv_layers[0].name
k = SWORD_LENGTH / sword_frame(sword0)[4]
sword0.data.transform(Matrix.Scale(k, 4))

grip = {}       # bone -> its closed rotation
wrist = {}      # hand bone -> the blade's direction in the hand's own axes
blade = {}      # hand bone -> the blade's point, in the hand's own axes
held = {}       # hand bone -> the middle of the handle, in the hand's own axes
edge_of = {}    # hand bone -> the way the cutting edge faces, in the hand's own axes
# Each sword has a bone of its own (SwordLeft, SwordRight), lying exactly on its hand's bone and a child of it. In
# every clip it rests there, so the sword is in her fist as before; the lab moves these two bones when the swords
# leave her hands (the thrown swords of Skill_Cast).
for o in bpy.data.objects:
    o.select_set(o is arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
for side in ("Left", "Right"):
    h = arm.data.edit_bones[side + "Hand"]
    b = arm.data.edit_bones.new("Sword" + side)
    b.head, b.tail, b.roll = h.head.copy(), h.tail.copy(), h.roll
    b.parent = h
    b.use_connect = False
    b.use_deform = True
bpy.ops.object.mode_set(mode="OBJECT")
swords = []
for side in ("Left", "Right"):
    o = sword0 if side == "Left" else sword0.copy()
    if side == "Right":
        o.data = sword0.data.copy()
        o.vertex_groups.clear()          # the copy must not keep the other hand
        bpy.context.scene.collection.objects.link(o)
    hand = PB[side + "Hand"]
    ki, kr = PB[side + "HandIndex1"].head.copy(), PB[side + "HandRing1"].head.copy()
    a = (ki - kr).normalized()                                   # across the palm, towards the index finger
    f = (PB[side + "HandIndex1"].vector.normalized() + PB[side + "HandRing1"].vector.normalized())
    f = (f - a * f.dot(a)).normalized()                          # along the fingers
    n = f.cross(a) if side == "Left" else a.cross(f)             # out of the palm
    gc, ga, r, edge, _, tip = sword_frame(o)
    # the handle across the palm, the blade leaving at the thumb's side, the edge towards the knuckles
    q = ga.rotation_difference(a)
    edge2 = q @ edge
    q = Quaternion(a, edge2.angle(f) * (1 if edge2.cross(f).dot(a) > 0 else -1)) @ q
    centre = (ki + kr) / 2 - a * 0.012 + f * 0.006 + n * (r + 0.012)
    o.matrix_world = Matrix.Translation(centre) @ q.to_matrix().to_4x4() @ Matrix.Translation(-gc) @ o.matrix_world
    bpy.context.view_layer.update()
    gc, ga, r, edge, _, tip = sword_frame(o)
    reach = r + 0.009
    hinge = a if side == "Right" else -a                         # turning about it closes a finger
    for finger in ("Index", "Ring"):
        closed = []
        for j, most in ((1, 95), (2, 110), (3, 80)):
            pb = PB["%sHand%s%d" % (side, finger, j)]
            base = pb.matrix.copy()
            best = (1e9, 0)
            for deg in range(0, most + 1, 2):
                pb.matrix = base
                bpy.context.view_layer.update()
                turn(pb, Quaternion(hinge, math.radians(deg)), pb.head.copy())
                d = off_line(pb.tail, gc, ga)
                if d < best[0]:
                    best = (d, deg)
                if d <= reach or d > best[0] + 0.004:
                    break
            if d > reach:
                deg = best[1]
            if j == 3:
                deg = max(deg, min(int(closed[1] * 0.5), 45))    # a fingertip never stays straight on a closed finger
            pb.matrix = base
            bpy.context.view_layer.update()
            turn(pb, Quaternion(hinge, math.radians(deg)), pb.head.copy())
            closed.append(deg)
        print("GRIP %s %s closes by %s degrees" % (side, finger, closed))
    # the thumb: swung across the handle, then closed round it from the other side
    pb = PB[side + "HandThumb1"]
    base = pb.matrix.copy()
    swing = 0
    for deg in range(0, 75, 3):
        for sg in (1, -1):
            pb.matrix = base
            bpy.context.view_layer.update()
            turn(pb, Quaternion(n, math.radians(sg * deg)), pb.head.copy())
            d = pb.vector.normalized()
            if abs(d.dot(ga)) < 0.30 and d.dot(f) > 0:
                swing = sg * deg
                break
        if swing:
            break
    if not swing:
        pb.matrix = base
        bpy.context.view_layer.update()
    closed = [swing]
    for j, most in ((1, 60), (2, 70), (3, 60)):
        pb = PB["%sHandThumb%d" % (side, j)]
        base = pb.matrix.copy()

        def at_angle(deg):
            pb.matrix = base
            bpy.context.view_layer.update()
            turn(pb, Quaternion(ga, math.radians(deg)), pb.head.copy())
            return off_line(pb.tail, gc, ga)
        d0, d1, d2 = at_angle(0), at_angle(6), at_angle(-6)
        closer = 1 if d1 < d2 else -1
        sgn = -closer if d0 < reach else closer                  # inside the handle it opens, away from it it closes
        best = (1e9, 0)
        deg = 0
        for deg in range(0, most + 1, 2):
            d = at_angle(sgn * deg)
            if d0 < reach:
                if d >= reach:
                    break
            else:
                if d < best[0]:
                    best = (d, deg)
                if d <= reach + 0.002:
                    break
                if d > best[0] + 0.004:
                    deg = best[1]
                    break
        at_angle(sgn * deg)
        closed.append(sgn * deg)
    print("GRIP %s thumb swings and closes by %s degrees; handle radius %.3f m" % (side, closed, r))
    for pb in PB:
        if pb.name.startswith(side + "Hand") and pb.name != side + "Hand":
            grip[pb.name] = pb.rotation_quaternion.copy()
    wrist[hand.name] = hand.bone.matrix_local.to_3x3().inverted() @ ga
    blade[hand.name] = hand.bone.matrix_local.inverted() @ tip
    held[hand.name] = hand.bone.matrix_local.inverted() @ gc
    edge_of[hand.name] = hand.bone.matrix_local.to_3x3().inverted() @ edge
    o.vertex_groups.new(name="Sword" + side).add(list(range(len(o.data.vertices))), 1.0, "REPLACE")
    swords.append(o)

# the swords into her mesh (the clips are still untouched: she is in her rest pose)
for o in bpy.data.objects:
    o.select_set(o is body or o in swords)
bpy.context.view_layer.objects.active = body
bpy.ops.object.join()

# the wrist turn that puts the blade up and SLANT degrees forward in Idle (she looks along -Y)
want = Vector((0, -math.sin(math.radians(SLANT)), math.cos(math.radians(SLANT))))
use(actions["Idle"])
bpy.context.scene.frame_set(0)
turn_of = {}
for name, local in wrist.items():
    m = PB[name].matrix.to_3x3().normalized()
    now = (m @ local).normalized()
    r_ = now.rotation_difference(want).to_matrix()
    turn_of[name] = (m.inverted() @ r_ @ m).to_4x4()
    print("WRIST %s turned %.0f degrees to bring the blade from %s to up and %.0f degrees forward"
          % (name, math.degrees(now.angle(want)), [round(v, 2) for v in now], SLANT))
# Each attack was made for one hand; Mixamo's other hand holds nothing (or a torch, a shield) and waves about. With a
# sword in it that hand is brought to her guard instead: the arm she stands with in Idle, carried by her chest, so
# it follows every turn and lean of her body, with OFF_LIFE of the clip's own movement left in for life.
OFF_LIFE = 0.25
OFF_ARM = ("Shoulder", "Arm", "ForeArm")
use(actions["Idle"])
bpy.context.scene.frame_set(0)
guard = {s_ + b: PB[s_ + b].rotation_quaternion.copy() for s_ in ("Left", "Right") for b in OFF_ARM + ("Hand",)}
for name, act in actions.items():
    last = use(act)
    natural = name.startswith("Attack") or name == "BasicAttack"   # attacks keep Mixamo's own wrists: the cuts were made for them
    off = None
    if natural:
        off = "Right" if "_L" in name else "Left" if ("_R" in name or name == "BasicAttack") else None
    if off:
        print("OFFHAND %-14s the %s arm keeps her guard" % (name, off.lower()))
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        if off:
            for b in OFF_ARM:
                pb = PB[off + b]
                q = guard[off + b].copy()
                if q.dot(pb.rotation_quaternion) < 0:
                    q.negate()
                pb.rotation_quaternion = q.slerp(pb.rotation_quaternion, OFF_LIFE)
                pb.keyframe_insert("rotation_quaternion", frame=f)
            PB[off + "Hand"].rotation_quaternion = guard[off + "Hand"]
            bpy.context.view_layer.update()
        for bone, m in turn_of.items():
            if natural and bone != (off or "") + "Hand":
                continue
            PB[bone].matrix = PB[bone].matrix @ m
            bpy.context.view_layer.update()
            PB[bone].keyframe_insert("rotation_quaternion", frame=f)
        for bone, q in grip.items():
            PB[bone].rotation_quaternion = q
            PB[bone].keyframe_insert("rotation_quaternion", frame=f)

# ---- the spinning kick (Skill_Kick, Mixamo's Hurricane Kick): one fixed pose turning round, with her arms folded
# close (43 degrees apart). The user asked for them opened to 160-180 degrees and the swords carried the same way:
# each arm is stretched straight out to its side of her chest, ARMS_OPEN apart, and each blade lies on the line of
# its arm, so the two swords are one line through her shoulders that sweeps round as she spins.
ARMS_OPEN = math.radians(170)
if "Skill_Kick" in actions:
    # Later the user asked for the kick itself to go: no leap and no leg thrown out. Only the turning is kept from
    # Mixamo's clip (how far her hips have turned about the upright, frame by frame); her hips stay upright at their
    # standing height, and her legs, spine, neck and head are Idle's. The line of the swords also rocks: one blade
    # up while the other is down, SWORDS_ROCK degrees either way, once every SWORDS_ROCK_FRAMES frames.
    SWORDS_ROCK, SWORDS_ROCK_FRAMES = math.radians(22), 20
    use(actions["Idle"])
    bpy.context.scene.frame_set(0)
    STILL = ("Spine", "Spine1", "Spine2", "Neck", "Head") + tuple(
        s + b for s in ("Left", "Right") for b in ("UpLeg", "Leg", "Foot", "ToeBase"))
    idle_pose = {b: PB[b].rotation_quaternion.copy() for b in STILL if b in PB}
    idle_hips = PB["Hips"].location.copy()
    last = use(actions["Skill_Kick"])
    hips = PB["Hips"]
    rest = hips.bone.matrix_local.copy()
    turned = []
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        v = (hips.matrix.to_3x3() @ rest.to_3x3().inverted()) @ Vector((0, -1, 0))
        a = math.atan2(v.x, -v.y)
        if turned:
            while a - turned[-1] > math.pi:
                a -= 2 * math.pi
            while a - turned[-1] < -math.pi:
                a += 2 * math.pi
        turned.append(a)
    smooth = [sum(turned[max(0, f - 2):f + 3]) / len(turned[max(0, f - 2):f + 3]) for f in range(last + 1)]
    print("SPIN Skill_Kick turns %.0f degrees in %d frames; legs, spine and head are Idle's" % (math.degrees(smooth[-1] - smooth[0]), last))
    drop = (math.pi - ARMS_OPEN) / 2
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        hips.matrix = Matrix.Translation(rest.translation) @ Matrix.Rotation(-smooth[f], 4, "Z") @ rest.to_3x3().to_4x4()
        bpy.context.view_layer.update()
        hips.location = idle_hips
        for b, q in idle_pose.items():
            PB[b].rotation_quaternion = q
        bpy.context.view_layer.update()
        hips.keyframe_insert("rotation_quaternion", frame=f)
        hips.keyframe_insert("location", frame=f)
        for b in idle_pose:
            PB[b].keyframe_insert("rotation_quaternion", frame=f)
        chest = PB["Spine2"]
        stands = chest.matrix.to_3x3() @ chest.bone.matrix_local.to_3x3().inverted()
        rock = SWORDS_ROCK * math.sin(2 * math.pi * f / SWORDS_ROCK_FRAMES)
        for side_name, sx in (("Left", 1), ("Right", -1)):
            rise = -drop + sx * rock
            want = (stands @ Vector((sx * math.cos(rise), 0, math.sin(rise)))).normalized()
            upper, fore, hand = PB[side_name + "Arm"], PB[side_name + "ForeArm"], PB[side_name + "Hand"]
            turn(upper, (fore.head - upper.head).rotation_difference(want), upper.head.copy())
            turn(fore, (hand.head - fore.head).rotation_difference(want), fore.head.copy())
            turn(hand, ((hand.matrix @ blade[hand.name]) - hand.head).rotation_difference(want), hand.head.copy())
            for pb in (upper, fore, hand):
                pb.keyframe_insert("rotation_quaternion", frame=f)
    bpy.context.scene.frame_set(last // 2)
    arms = (PB["LeftHand"].head - PB["LeftArm"].head).angle(PB["RightHand"].head - PB["RightArm"].head)
    blades = ((PB["LeftHand"].matrix @ blade["LeftHand"]) - PB["LeftHand"].head).angle((PB["RightHand"].matrix @ blade["RightHand"]) - PB["RightHand"].head)
    print("KICK arms %.0f degrees apart (Mixamo had 43), swords %.0f degrees apart" % (math.degrees(arms), math.degrees(blades)))

# ---- her head in the cuts. Every Mixamo cut throws her head its own way, so in a combo each next cut jerked her head
# (and with it the look of her whole body) to a new place. In all cuts her head now keeps looking ahead, at the enemy,
# as it does in Idle; HEAD_LIFE of the clip's own head movement is left in, and it never turns further from her chest
# than HEAD_TURN (in a spin her body turns away and her head goes with it).
HEAD_LIFE = 0.2
HEAD_TURN = math.radians(55)
use(actions["Idle"])
bpy.context.scene.frame_set(0)
head_idle = PB["Head"].matrix.to_quaternion()
head_on_chest = PB["Spine2"].matrix.to_quaternion().inverted() @ head_idle
# The same is done while she walks, runs and jumps: Mixamo swings her head left and right with every step. There
# her head keeps the way it looks on average in that clip (so the clip's own carriage of the head stays), and only
# HEAD_STEADY_LIFE of the swinging is left.
HEAD_STEADY = ("Walk", "Run", "Jump")
HEAD_STEADY_LIFE = 0.15
for name, act in actions.items():
    if not (name.startswith("Attack") or name in HEAD_STEADY):
        continue
    last = use(act)
    most = 0.0
    keep, life = head_idle, HEAD_LIFE
    if name in HEAD_STEADY:
        total = [0.0, 0.0, 0.0, 0.0]
        for f in range(last + 1):
            bpy.context.scene.frame_set(f)
            q = PB["Head"].matrix.to_quaternion()
            if q.dot(head_idle) < 0:
                q.negate()
            for i in range(4):
                total[i] += q[i]
        keep, life = Quaternion(total).normalized(), HEAD_STEADY_LIFE
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        head, neck = PB["Head"], PB["Neck"]
        now = head.matrix.to_quaternion()
        want = keep.copy()
        if want.dot(now) < 0:
            want.negate()
        want = want.slerp(now, life)
        chest = PB["Spine2"].matrix.to_quaternion()
        off = (chest @ head_on_chest).inverted() @ want          # how far that is from straight on her chest
        if off.w < 0:
            off.negate()
        if off.angle > HEAD_TURN:
            off = Quaternion().slerp(off, HEAD_TURN / off.angle)
        want = chest @ head_on_chest @ off
        delta = want @ now.inverted()
        if delta.w < 0:
            delta.negate()
        most = max(most, math.degrees(delta.angle))
        turn(neck, Quaternion().slerp(delta, 0.5), neck.head.copy())      # half by the neck, the rest by the head
        turn(head, want @ head.matrix.to_quaternion().inverted(), head.head.copy())
        neck.keyframe_insert("rotation_quaternion", frame=f)
        head.keyframe_insert("rotation_quaternion", frame=f)
    print("HEAD %-14s kept looking ahead; Mixamo had it up to %.0f degrees away" % (name, most))

# ---- the blade is aimed in the cuts Mixamo made without a sword. In these clips the wrist was animated for an empty
# hand, so the blade pointed behind her in the strike and spun round between wind-up and blow. Here the wrist is set
# from the swing itself in every frame: the blade carries on the line of the forearm, lifted a little up and forward,
# and its edge leads the way the hand is moving. AIM_FRONT clips are a cut to the front: around the hit the blade
# points at a spot REACH in front of the middle of her chest, at chest height.
# Every cut is made at an enemy standing right in front of her: all of them are aimed, by the hand that strikes
# (both hands in the leaping cut).
AIM = [(n, side) for n in actions if n.startswith("Attack")
       for side in (("Left",) if "_L" in n else ("Right",) if "_R" in n else ("Left", "Right"))]
AIM_FRONT = tuple(n for n, _ in AIM)
AHEAD, UP = Vector((0, -1, 0)), Vector((0, 0, 1))
aimed = {}      # (clip, side) -> where the blade should point, frame by frame
front = {}      # (clip, side, frame) -> how much the blow is aimed at the enemy there, and where the enemy is
REACH = 1.3     # metres in front of her chest: where a cut to the front is aimed


def eased(vs, i):
    acc = Vector((0, 0, 0))
    for k in range(-3, 4):
        acc += vs[min(max(i + k, 0), len(vs) - 1)] * (4 - abs(k))
    return acc


for name, side_name in AIM:
    last = use(actions[name])
    hand, fore = PB[side_name + "Hand"], PB[side_name + "ForeArm"]
    pos, along, mark_at = [], [], []
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        pos.append(hand.head.copy())
        along.append((hand.head - fore.head).normalized())
        chest = PB["Spine2"]
        # the point she strikes at: in front of the middle of her chest, the way she faces (not the way her chest
        # is twisted at that moment: in a swing it turns far to the side)
        mark_at.append(Vector((0.0, 0.0, PB["Spine1"].head.z)) + AHEAD * REACH)   # on her middle line, below the chest
    speed = [(pos[min(f + 1, last)] - pos[max(f - 1, 0)]).length * FPS / 2 for f in range(last + 1)]
    hit = max(range(last + 1), key=lambda f: speed[f])
    aim, lead = [], []
    going = AHEAD.copy()
    for f in range(last + 1):
        d = (along[f] * 0.8 + UP * 0.35 + AHEAD * 0.25).normalized()
        if name in AIM_FRONT:
            t = (f - hit) / FPS
            w = max(0.0, min(1.0, (t + 0.33) / 0.15)) * max(0.0, min(1.0, (0.30 - t) / 0.15))
            # only while her hand is in front of her chest: from behind or beside it the line to the enemy
            # would run through her own body
            w *= max(0.0, min(1.0, ((pos[f] - mark_at[f]).dot(AHEAD) + REACH - 0.05) / 0.2))
            d = d.lerp((mark_at[f] - pos[f]).normalized(), w).normalized()
            front[(name, side_name, f)] = (w, mark_at[f].copy())
        aim.append(d)
        v = pos[min(f + 2, last)] - pos[max(f - 2, 0)]
        if v.length > 0.06:
            going = v.normalized()
        lead.append(going.copy())
    aim = [eased(aim, f).normalized() for f in range(last + 1)]
    aimed[(name, side_name)] = aim
    lead = [eased(lead, f) for f in range(last + 1)]
    worst = 0.0
    before = None
    kept = None
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        now = (hand.matrix.to_3x3() @ wrist[hand.name]).normalized()
        turn(hand, now.rotation_difference(aim[f]), hand.head.copy())
        e = lead[f] - aim[f] * lead[f].dot(aim[f])
        if kept is not None:                 # the edge turns round the blade by at most 14 degrees a frame
            k = kept - aim[f] * kept.dot(aim[f])
            if k.length > 0.05:
                k.normalize()
                if e.length > 0.05:
                    want = k.angle(e.normalized()) * (1 if k.cross(e).dot(aim[f]) > 0 else -1)
                    step = max(-math.radians(14), min(math.radians(14), want))
                    e = Quaternion(aim[f], step) @ k
                else:
                    e = k
        if e.length > 0.05:
            e.normalize()
            kept = e.copy()
            ec = hand.matrix.to_3x3() @ edge_of[hand.name]
            ec = (ec - aim[f] * ec.dot(aim[f])).normalized()
            angle = ec.angle(e) * (1 if ec.cross(e).dot(aim[f]) > 0 else -1)
            turn(hand, Quaternion(aim[f], angle), hand.head.copy())
        hand.keyframe_insert("rotation_quaternion", frame=f)
        q = hand.matrix.to_quaternion()
        if before is not None:
            turned = math.degrees(before.rotation_difference(q).angle)
            worst = max(worst, min(turned, 360 - turned))
        before = q
    print("AIM %-10s %s blade set from the swing on %d frames, hit at %.2f s, fastest turn of the hand %.0f degrees a frame"
          % (name, side_name.lower(), last + 1, hit / FPS, worst))

# ---- her wrist in the cuts. Aiming the blade by the wrist alone bent it further than a wrist goes (past a right
# angle) and wrung it round. Here the striking hand is set the way an arm does it. Held straight, the blade stands
# square to the forearm. First the forearm rolls about its own length (up to FOREARM_TURN, a little more in the
# wrist, and never faster than TURN_STEP a frame) until the blade leans the way it should point; then the wrist bends
# towards that way, at most WRIST_BEND. What is still missing around the hit is made by the whole arm, below.
WRIST_BEND = math.radians(48)
FOREARM_TURN = math.radians(100)
WRIST_TURN = math.radians(20)
TURN_STEP = math.radians(22)
Y_AXIS = Vector((0, 1, 0))
for name, side_name in AIM:
    last = use(actions[name])
    hand, fore = PB[side_name + "Hand"], PB[side_name + "ForeArm"]
    want = aimed[(name, side_name)]
    roll = None
    off = 0.0
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        hand.rotation_quaternion = Quaternion()                       # a straight wrist
        bpy.context.view_layer.update()
        along = (hand.head - fore.head).normalized()
        blade0 = (hand.matrix.to_3x3() @ wrist[hand.name]).normalized()
        flat0, flat = blade0 - along * blade0.dot(along), want[f] - along * want[f].dot(along)
        if roll is None:
            roll = 0.0
        if flat.length > 0.15 and flat0.length > 0.15:
            raw = flat0.angle(flat) * (1 if flat0.cross(flat).dot(along) > 0 else -1)
            raw = max(-(FOREARM_TURN + WRIST_TURN), min(FOREARM_TURN + WRIST_TURN, raw))
            roll = raw if f == 0 else roll + max(-TURN_STEP, min(TURN_STEP, raw - roll))
        by_forearm = max(-FOREARM_TURN, min(FOREARM_TURN, roll))
        fore.rotation_quaternion = fore.rotation_quaternion @ Quaternion(Y_AXIS, by_forearm)
        bpy.context.view_layer.update()
        if abs(roll - by_forearm) > 1e-4:
            turn(hand, Quaternion(along, roll - by_forearm), hand.head.copy())
        now = (hand.matrix.to_3x3() @ wrist[hand.name]).normalized()
        q = now.rotation_difference(want[f])
        if q.angle > 1e-4:
            turn(hand, Quaternion(q.axis, min(q.angle, WRIST_BEND)), hand.head.copy())
        now = (hand.matrix.to_3x3() @ wrist[hand.name]).normalized()
        off = max(off, math.degrees(now.angle(want[f])))
        fore.keyframe_insert("rotation_quaternion", frame=f)
        hand.keyframe_insert("rotation_quaternion", frame=f)
    print("WRIST %-14s %-5s forearm rolls, wrist bends at most %.0f degrees; the blade is at most %2.0f degrees off its aim"
          % (name, side_name.lower(), math.degrees(WRIST_BEND), off))

# ---- the arm finishes the aim. With her wrist held to a hand's range the blade stopped short of the enemy. Around
# the hit the rest of the way is made by the whole arm, turned at the shoulder (at most ARM_AIM), so the blade
# points at the spot on her middle line in front of her.
ARM_AIM = math.radians(40)
for name, side_name in AIM:
    last = use(actions[name])
    hand, upper = PB[side_name + "Hand"], PB[side_name + "Arm"]
    most = left_over = 0.0
    for f in range(last + 1):
        w, spot = front.get((name, side_name, f), (0.0, None))
        if w <= 0.01:
            continue
        bpy.context.scene.frame_set(f)
        now = (hand.matrix.to_3x3() @ wrist[hand.name]).normalized()
        q = now.rotation_difference((spot - hand.head).normalized())
        if q.angle > 1e-4:
            angle = min(q.angle * w, ARM_AIM)
            most = max(most, math.degrees(angle))
            turn(upper, Quaternion(q.axis, angle), upper.head.copy())
            upper.keyframe_insert("rotation_quaternion", frame=f)
        if w > 0.99:
            now = (hand.matrix.to_3x3() @ wrist[hand.name]).normalized()
            left_over = max(left_over, math.degrees(now.angle((spot - hand.head).normalized())))
    print("ARM %-14s %-5s arm turned up to %2.0f degrees at the shoulder; at the hit the blade is at most %2.0f degrees off the enemy"
          % (name, side_name.lower(), most, left_over))

# ---- hands and blades stay clear of her body. Her trunk (hips to neck, and the skirt below) is taken as a rounded
# column; its width and depth are measured on her own mesh. A fist is kept CLEAR away from it by turning the upper
# arm at the shoulder, a blade BLADE_CLEAR away by turning the wrist. The turn needed is worked out for every frame
# and then spread smoothly over the frames around it: a correction that switched on and off from one frame to the
# next made the sword jerk in her hand instead of following the swing.
CLEAR = 0.07
BLADE_CLEAR = 0.02
TRUNK = ("Hips", "Spine", "Spine1", "Spine2")
gi = {body.vertex_groups[b].index for b in TRUNK if b in body.vertex_groups}
xs, ys = [], []
for v in body.data.vertices:
    if sum(g.weight for g in v.groups if g.group in gi) > 0.6:
        xs.append(abs(v.co.x))
        ys.append(abs(v.co.y - arm.data.bones["Spine1"].head_local.y))
xs.sort()
ys.sort()
HALF_WIDTH = xs[int(len(xs) * 0.97)]
HALF_DEPTH = ys[int(len(ys) * 0.97)]
print("CLEAR her trunk is taken as %.2f m wide and %.2f m deep; fists keep %.2f m from it, blades %.2f m"
      % (2 * HALF_WIDTH, 2 * HALF_DEPTH, CLEAR, BLADE_CLEAR))


def out_of_trunk(pt, room):
    """None if the point is clear of her trunk, else the way out of it."""
    hips, neck = PB["Hips"].head, PB["Neck"].head
    up = neck - hips
    span = up.length
    up = up / span
    side = PB["LeftArm"].head - PB["RightArm"].head
    side = (side - up * side.dot(up)).normalized()
    front = up.cross(side)
    d = pt - hips
    t = d.dot(up) / span
    if t < -0.55 or t > 1.0:
        return None
    hw, hd = HALF_WIDTH + room, HALF_DEPTH + room
    x, y = d.dot(side), d.dot(front)
    if (x / hw) ** 2 + (y / hd) ** 2 >= 1:
        return None
    way = side * (x / hw ** 2) + front * (y / hd ** 2)
    return way.normalized() if way.length > 1e-6 else side


def pose_point(pb, local):
    return pb.matrix @ pb.bone.matrix_local.inverted() @ (pb.bone.matrix_local @ local)


def spread(deltas, loop):
    """Per-frame turns -> the same turns, widened by 2 frames and then eased in and out over 4."""
    n = len(deltas)
    pick = (lambda i: deltas[i % n]) if loop else (lambda i: deltas[min(max(i, 0), n - 1)])
    wide = [max((pick(g) for g in range(f - 2, f + 3)), key=lambda q: q.angle if abs(q.w) < 1 else 0.0) for f in range(n)]
    pick = (lambda i: wide[i % n]) if loop else (lambda i: wide[min(max(i, 0), n - 1)])
    out = []
    for f in range(n):
        acc = [0.0, 0.0, 0.0, 0.0]
        for k in range(-4, 5):
            q = pick(f + k)
            w = (5 - abs(k)) * (1 if q.w >= 0 else -1)
            for i in range(4):
                acc[i] += q[i] * w
        out.append(Quaternion(acc).normalized())
    return out


def jerk(deltas):
    return max([math.degrees((deltas[f].inverted() @ deltas[f + 1]).angle) for f in range(len(deltas) - 1)] or [0.0])


for name, act in actions.items():
    last = use(act)
    loop = name in ("Idle", "Walk", "Run")
    n = last if loop else last + 1          # a loop's last frame is its first again
    bones = [PB[s_ + b] for s_ in ("Left", "Right") for b in ("Arm", "Hand")]
    base = {pb.name: [] for pb in bones}
    need = {pb.name: [] for pb in bones}
    for f in range(n):
        bpy.context.scene.frame_set(f)
        for pb in bones:
            base[pb.name].append(pb.rotation_quaternion.copy())
        for side_name in ("Left", "Right"):
            hand, upper = PB[side_name + "Hand"], PB[side_name + "Arm"]
            for _ in range(30):                                   # the fist, by the shoulder
                p = pose_point(hand, held[hand.name])
                way = out_of_trunk(p, CLEAR)
                if way is None:
                    break
                axis = (p - upper.head).cross(way)
                if axis.length < 1e-6:
                    break
                turn(upper, Quaternion(axis.normalized(), math.radians(2)), upper.head.copy())
            for part in (0.25, 0.5, 0.75, 1.0):                   # along the blade to its point, by the wrist
                for _ in range(25):
                    c, t = pose_point(hand, held[hand.name]), pose_point(hand, blade[hand.name])
                    p = c + (t - c) * part
                    way = out_of_trunk(p, BLADE_CLEAR)
                    if way is None:
                        break
                    axis = (p - hand.head).cross(way)
                    if axis.length < 1e-6:
                        break
                    turn(hand, Quaternion(axis.normalized(), math.radians(3)), hand.head.copy())
        bpy.context.view_layer.update()
        for pb in bones:
            need[pb.name].append(base[pb.name][f].inverted() @ pb.rotation_quaternion)
    report = []
    for pb in bones:
        raw = need[pb.name]
        if max(q.angle if abs(q.w) < 1 else 0.0 for q in raw) < math.radians(0.5):
            continue
        soft = spread(raw, loop)
        report.append("%s %.0f->%.0f" % (pb.name, jerk(raw), jerk(soft)))
        for f in range(last + 1):
            pb.rotation_quaternion = base[pb.name][f % n] @ soft[f % n]
            pb.keyframe_insert("rotation_quaternion", frame=f)
    if report:
        print("CLEAR %-14s largest jump of a correction between two frames, degrees, before->after: %s"
              % (name, ", ".join(report)))

# ---- the check: in how many frames does a blade still pass through her trunk (no room added)?
def crossing(name):
    last = use(actions[name])
    count = {"Left": 0, "Right": 0}
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        for side_name in count:
            hand = PB[side_name + "Hand"]
            c, t = pose_point(hand, held[hand.name]), pose_point(hand, blade[hand.name])
            if any(out_of_trunk(c + (t - c) * (k / 12), 0.0) is not None for k in range(2, 13)):
                count[side_name] += 1
    return count, last + 1


for name in actions:
    count, frames = crossing(name)
    print("CROSS %-14s blade through her trunk: left %d, right %d of %d frames" % (name, count["Left"], count["Right"], frames))

# A blade never goes into the ground: where a clip lays her down (Death), each wrist is turned just enough to keep
# the blade's point on the floor.
ON_GROUND = ("Death",)
FLOOR = 0.03
for name in ON_GROUND:
    if name not in actions:
        continue
    last = use(actions[name])
    lifted = 0
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        for bone, local in blade.items():
            pb = PB[bone]
            moved = False
            for _ in range(4):
                rest_to_pose = pb.matrix @ pb.bone.matrix_local.inverted()
                p, t = pb.head.copy(), rest_to_pose @ (pb.bone.matrix_local @ local)
                if t.z >= FLOOR - 0.002:
                    break
                d = t - p
                length = d.length
                now, want_z = math.asin(max(-1, min(1, d.z / length))), min(1, (FLOOR - p.z) / length)
                axis = d.cross(Vector((0, 0, 1)))
                if axis.length < 1e-5:
                    break
                turn(pb, Quaternion(axis.normalized(), math.asin(want_z) - now), p)
                moved = True
            if moved:
                pb.keyframe_insert("rotation_quaternion", frame=f)
                lifted += 1
    print("FLOOR %s: a blade's point was lifted back onto the ground on %d hand-frames" % (name, lifted))

# ---- what the lab needs to know about the clips: the pace of the gaits, and for every attack the moment the blade
# is fastest (the hit) and her path forward. Written as a header the lab includes.
def hit_time(act, hands):
    last = use(act)
    pts = []
    for f in range(last + 1):
        bpy.context.scene.frame_set(f)
        pts.append([PB[h + "Hand"].tail - PB["Hips"].head for h in hands])
    speed = [max((pts[f + 1][i] - pts[f][i]).length for i in range(len(hands))) for f in range(last)]
    return (max(range(last), key=lambda f: speed[f]) + 0.5) / FPS


lines = ["// Written by art/nyxara/blender/build_from_mixamo.py. Do not edit: build the model again instead.",
         "#pragma once", "#include <map>", "#include <string>", "#include <vector>", "namespace NyxaraLab", "{",
         "inline constexpr float WalkPace = %.6ff, RunPace = %.6ff;" % (speeds.get("Walk", 1), speeds.get("Run", 4)),
         "struct AttackClip", "{", "    float Hit = 0;                // seconds into the clip: the blade is fastest",
         "    std::vector<float> Forward;   // metres forward at each 1/30 s; empty: she stays on the spot", "};",
         "inline const std::map<std::string, AttackClip> Attacks{"]
for name in actions:
    if not name.startswith(("Attack", "Skill")):
        continue
    hands = ("Left",) if "_L" in name else ("Right",) if "_R" in name else ("Left", "Right")
    hit = hit_time(actions[name], hands)
    path = roots.get(name, [])
    lines.append("    {\"%s\", {%.4ff, {%s}}}," % (name, hit, ", ".join("%.4ff" % v for v in path)))
    print("ATTACK %-14s hit at %.2f s%s" % (name, hit, ", goes %.2f m forward" % path[-1] if path else ""))
lines += ["};",
          "// The point of each blade and the middle of its handle, in the axes of its sword bone (Blender's), left then right.",
          "inline constexpr float SwordTip[2][3] = {%s};" % ", ".join("{%.5ff, %.5ff, %.5ff}" % tuple(blade[s + "Hand"]) for s in ("Left", "Right")),
          "inline constexpr float SwordGrip[2][3] = {%s};" % ", ".join("{%.5ff, %.5ff, %.5ff}" % tuple(held[s + "Hand"]) for s in ("Left", "Right")),
          "} // namespace NyxaraLab", ""]
(SRC.parents[1] / "testing" / "CharacterLab" / "NyxaraLabClips.h").write_text("\n".join(lines), encoding="utf-8", newline="\n")
# the game's copy (game/src/NyxaraClips.h): the same, in a namespace of its own
game_lines = [l.replace("namespace NyxaraLab", "namespace NyxaraClips") for l in lines]
game_lines[0] = game_lines[0].replace("Do not edit", "(a copy of testing/CharacterLab/NyxaraLabClips.h). Do not edit")
(SRC.parents[1] / "game" / "src" / "NyxaraClips.h").write_text(chr(10).join(game_lines), encoding="utf-8", newline=chr(10))

# ---- wings on her back. The wings model (art/nyxara/Wings.glb, a spread pair 1.9 m wide, a copy of the user's
# Desktop/blndr/nyxara-wings/base_basic_pbr.glb) is turned so its tips lie behind her, set with its roots in her back
# between the shoulder blades, and given three bones a wing (root to the top of the arch, to the middle of the long
# feathers, to the tip), carried by her chest (Spine2). The wings become part of her mesh. They stay spread: she does
# not fly yet. Their movement is not drawn by hand but worked out from each clip: a slow breath of the open wing,
# and on top of it the wing swinging after her body like a thing with weight (it lags when her chest rises, falls,
# turns or starts forward, then settles), the outer parts a little later than the root.
WING_SIZE = 1.222                # of the model's size: 0.94 fits her height (the model was made for 1.9 m, she is 1.79 m), and the user asked for 30 % more
WING_BACK, WING_UP = 0.28, 0.50  # where the model's origin goes: metres behind her middle, metres over the ground (roots kept in her back at the same height)
WING_TRIANGLES = 36000
WING_LIFT = math.radians(8)      # the root is held this much higher than the model has it: wings opened
WING_BREATH = math.radians(3.5)  # the slow rise and fall of the open wing
WING_MOST = math.radians(28)     # no swing goes further than this
WING_BEAT, WING_DAMP = 2 * math.pi * 1.6, 0.5   # how fast a wing swings back (1.6 times a second), how soon it settles
WING_RISE, WING_TURN, WING_START = 1.2, 0.6, 0.5  # how much it answers her chest going up, turning, going forward
WING_SWEEP = {"Run": math.radians(12), "Walk": math.radians(3), "Jump": math.radians(10)}   # held back in the wind
# While she walks and runs her chest twists left and right with every step, and wings fixed to it swept back and
# forth as fast. There the wings keep to the way her chest stands on average (WING_STEADY of the twist is taken out)
# and answer her steps only weakly (WING_GAIN).
WING_STEADY = {"Walk": 0.85, "Run": 0.9}
WING_GAIN = {"Walk": 0.4, "Run": 0.2}
WING_POINTS = ((0.04, 0.13, 0.73), (0.28, 0.05, 0.91), (0.60, -0.06, 0.58), (0.90, -0.14, 0.20))   # in the model
WING_SRC = SRC / "Wings.glb"
if WING_SRC.exists():
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    seen = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(WING_SRC))
    wing = [o for o in bpy.data.objects if o not in seen and o.type == "MESH"][0]
    for o in [o for o in bpy.data.objects if o not in seen and o is not wing]:
        bpy.data.objects.remove(o)
    wing.parent = None
    for o in bpy.data.objects:
        o.select_set(o is wing)
    bpy.context.view_layer.objects.active = wing
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    dec = wing.modifiers.new("d", "DECIMATE")
    dec.ratio = WING_TRIANGLES / sum(len(p.vertices) - 2 for p in wing.data.polygons)
    bpy.ops.object.modifier_apply(modifier="d")
    place = Matrix.Translation((0, WING_BACK, WING_UP)) @ Matrix.Rotation(math.pi, 4, "Z") @ Matrix.Scale(WING_SIZE, 4)
    wing.data.transform(place)
    wing.data.uv_layers[0].name = body.data.uv_layers[0].name
    # one plain material, as for her body: the colour picture, not shiny
    wimage = wing.material_slots[0].material.node_tree.nodes["Principled BSDF"].inputs["Base Color"].links[0].from_node.image
    wimage.name = "Wings_color"
    wmat = bpy.data.materials.new("Wings")
    wmat.use_nodes = True
    wb = wmat.node_tree.nodes["Principled BSDF"]
    wt = wmat.node_tree.nodes.new("ShaderNodeTexImage")
    wt.image = wimage
    wmat.node_tree.links.new(wt.outputs["Color"], wb.inputs["Base Color"])
    wb.inputs["Roughness"].default_value = 0.8
    wb.inputs["Metallic"].default_value = 0.0
    wing.data.materials.clear()
    wing.data.materials.append(wmat)

    pts = [place @ Vector(p) for p in WING_POINTS]          # the right-hand line of the model; mirrored below
    pts = [Vector((abs(p.x), p.y, p.z)) for p in pts]
    for o in bpy.data.objects:
        o.select_set(o is arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for side_name, sx in (("L", 1), ("R", -1)):
        parent = arm.data.edit_bones["Spine2"]
        for k in range(3):
            b = arm.data.edit_bones.new("Wing%s%d" % (side_name, k + 1))
            b.head = Vector((sx * pts[k].x, pts[k].y, pts[k].z))
            b.tail = Vector((sx * pts[k + 1].x, pts[k + 1].y, pts[k + 1].z))
            b.roll = 0
            b.parent = parent
            b.use_connect = False
            b.use_deform = True
            parent = b
    bpy.ops.object.mode_set(mode="OBJECT")

    # weights: by the distance from her middle, blended 10 cm either side of a joint; the whole wing turns at its root
    co = np.empty(len(wing.data.vertices) * 3, "f4")
    wing.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    ease = lambda x: np.clip(x, 0, 1) ** 2 * (3 - 2 * np.clip(x, 0, 1))
    for side_name, sx in (("L", 1), ("R", -1)):
        idx = np.nonzero(co[:, 0] * sx >= 0)[0]
        out = np.abs(co[idx, 0])
        b1, b2 = ease((out - pts[1].x + 0.10) / 0.20), ease((out - pts[2].x + 0.10) / 0.20)
        for k, wgt in enumerate((1 - b1, b1 * (1 - b2), b1 * b2)):
            g = wing.vertex_groups.new(name="Wing%s%d" % (side_name, k + 1))
            steps = np.round(wgt * 50).astype(int)
            for val in range(1, 51):
                sel = idx[steps == val]
                if len(sel):
                    g.add(sel.tolist(), val / 50.0, "REPLACE")
    for o in bpy.data.objects:
        o.select_set(o in (body, wing))
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    PB = arm.pose.bones
    print("WINGS %d triangles, roots at %s, tips at x %.2f y %.2f z %.2f; she has %d bones now"
          % (WING_TRIANGLES, [round(v, 2) for v in pts[0]], pts[3].x, pts[3].y, pts[3].z, len(arm.data.bones)))

    # ---- Fly: her in the air, made here (there is no Mixamo clip of it). A loop of one wing beat a second. Her body
    # is set from the rest pose: leaning a little forward, back arched, one knee drawn up and the other leg trailing,
    # feet pointed; her arms and hands are Idle's (they hold the swords). She rides up and down with the beat.
    # The wings beat wide: the root most, the outer bones after it.
    FLY_FRAMES = 30
    FLY_FLAP = ((math.radians(30), 0.0), (math.radians(17), 0.5), (math.radians(13), 1.0))   # how far, how late (radians of the beat)
    FLY_RAISED, FLY_BACK = math.radians(10), math.radians(6)
    fly = actions["Idle"].copy()
    fly.name = "Fly"
    fly.use_fake_user = True
    use(actions["Idle"])
    bpy.context.scene.frame_set(0)
    held = {pb.name: pb.rotation_quaternion.copy() for pb in PB
            if pb.name.startswith(("Left", "Right")) and any(k in pb.name for k in ("Shoulder", "Arm", "Hand"))}
    use(fly)
    for fc in curves(fly):
        for i in range(len(fc.keyframe_points) - 1, -1, -1):
            if fc.keyframe_points[i].co[0] > FLY_FRAMES:
                fc.keyframe_points.remove(fc.keyframe_points[i])
        fc.update()
    across = Vector((1, 0, 0))     # turning about it by a positive angle leans a thing forward

    def lean(bone, degrees):
        pb = PB[bone]
        turn(pb, Quaternion(across, math.radians(degrees)), pb.head.copy())

    for f in range(FLY_FRAMES + 1):
        beat = 2 * math.pi * f / FLY_FRAMES
        bpy.context.scene.frame_set(f)
        for pb in PB:
            if not pb.name.startswith("Wing"):
                pb.rotation_quaternion = held.get(pb.name, Quaternion())
        rides = math.sin(beat - 1.0)
        PB["Hips"].location = PB["Hips"].bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, 0.05 * rides))
        bpy.context.view_layer.update()
        lean("Hips", 10 + 2 * rides)
        for b in ("Spine", "Spine1", "Spine2"):
            lean(b, -3)
        lean("Neck", -3)
        lean("Head", -5)
        lean("RightUpLeg", -22 + 3 * math.sin(beat))
        lean("RightLeg", 48)
        lean("RightFoot", 30)
        lean("LeftUpLeg", 6 - 3 * math.sin(beat))
        lean("LeftLeg", 16)
        lean("LeftFoot", 38)
        for pb in PB:
            if not pb.name.startswith("Wing"):
                pb.keyframe_insert("rotation_quaternion", frame=f)
        PB["Hips"].keyframe_insert("location", frame=f)
    actions["Fly"] = fly

    # ---- Fly_Rise and Fly_Land: leaving the ground and coming back to it, made here like Fly. Her body goes through
    # a few poses (degrees, as in Fly; dz: her hips up or down), eased from one to the next.
    #   Fly_Rise: stands, crouches with her wings lifted high (0.3 s), springs up on one hard beat (0.5 s; the lab
    #             lifts her from 0.4 s), and is in Fly's pose at 1.2 s.
    #   Fly_Land: from Fly's pose her legs reach down (0.4 s: the lab holds this until her feet touch), her knees
    #             take the landing with her wings flared (0.6 s), and she stands at 1.0 s.
    legs = PB["RightUpLeg"].bone.length + PB["RightLeg"].bone.length
    FLYING = dict(hl=10, sp=-3, nk=-3, hd=-5, rt=-22, rk=48, rf=30, lt=6, lk=16, lf=38, dz=0.0)
    STANDS = dict(hl=0, sp=0, nk=0, hd=0, rt=0, rk=0, rf=0, lt=0, lk=0, lf=0, dz=0.0)
    STRETCH = dict(hl=4, sp=-3, nk=-2, hd=-4, rt=-4, rk=0, rf=35, lt=-4, lk=0, lf=35, dz=0.0)
    REACH = dict(hl=5, sp=-2, nk=-2, hd=-4, rt=-17, rk=24, rf=18, lt=-9, lk=12, lf=22, dz=0.0)

    def crouch(deg, bow):
        return dict(hl=bow, sp=2, nk=-4, hd=-6, rt=-deg - bow, rk=2 * deg, rf=-deg, lt=-deg - bow, lk=2 * deg, lf=-deg,
                    dz=-legs * (1 - math.cos(math.radians(deg))))

    def eased(keys, f):
        for (f0, a), (f1, b) in zip(keys, keys[1:]):
            if f <= f1:
                w = (f - f0) / (f1 - f0)
                w = w * w * (3 - 2 * w)
                return a + (b - a) * w if not isinstance(a, dict) else {k: a.get(k, 0) + (b.get(k, 0) - a.get(k, 0)) * w for k in set(a) | set(b)}
        return keys[-1][1]

    # V, as the user asked for it later: she flies to the creature feet first and flat: lying back LEAN_BACK degrees, her
    # body straight (no bend at the hips or knees), her arms by her sides, her legs out before her, straight and together (the user: "close to each other", not bent; op: degrees each leg is opened), and her
    # arms out to her swords. That is Cast_Fly, a loop. Cast_Pull then: both feet land on the creature, one each side
    # of the swords, knees bent, hands on the handles (0.4 s); her arms pull the swords up and out as both legs push
    # the creature away (0.4-0.57 s); she goes back through the air, lands in a crouch and stands.
    # Her legs and hips are made here; from her spine up she is Skill_Cast played backwards from the moment her arms
    # are out (in Cast_Fly and until she pushes: that moment), and in the last frames her arms come to Idle's.
    WRAP = 80
    WRAP_SIGN = -1      # which way about the upright brings a wing round before her
    LEAN_BACK = 75     # degrees from upright: the user's "160-170" is that far opened from standing, nearly flat on her back
    FEET_FIRST = dict(hl=-LEAN_BACK, sp=0, nk=12, hd=16, rt=0, rk=2, rf=-20, lt=0, lk=2, lf=-20, op=2, dz=0.0)
    FEET_FIRST2 = dict(hl=-LEAN_BACK - 2, sp=0, nk=12, hd=16, rt=-1, rk=3, rf=-20, lt=1, lk=2, lf=-20, op=2, dz=0.0)
    BRACE = dict(hl=-12, sp=4, nk=4, hd=5, rt=-84, rk=30, rf=-14, lt=-84, lk=30, lf=-14, op=4, dz=0.0)
    THRUST = dict(hl=-27, sp=-2, nk=4, hd=6, rt=-70, rk=2, rf=-25, lt=-70, lk=2, lf=-25, op=3, dz=0.0)
    DRAWN = dict(hl=-6, sp=1, nk=1, hd=1, rt=-42, rk=66, rf=10, lt=-30, lk=52, lf=14, op=8, dz=0.0)
    PULL_UPPER = ("Spine", "Spine1", "Spine2", "Neck", "Head")
    pull_bones = [pb.name for pb in PB if pb.name in PULL_UPPER or
                  (pb.name.startswith(("Left", "Right")) and any(k in pb.name for k in ("Shoulder", "Arm", "Hand")))]
    pull_pose = {}
    if "Skill_Cast" in actions:
        cast_last = use(actions["Skill_Cast"])
        thrown = min(cast_last, int(round(hit_time(actions["Skill_Cast"], ("Left", "Right")) * FPS)))
        for back in range(0, 31):
            bpy.context.scene.frame_set(max(0, thrown - back))
            pull_pose[back] = {b: PB[b].rotation_quaternion.copy() for b in pull_bones}
        print("PULL from frame %d of Skill_Cast backwards" % thrown)
    MOVES = {   # name: frames, her body, her wing roots' lift in degrees
        "Cast_Fly": (20, ((0, FEET_FIRST), (20, FEET_FIRST)), ((0, -12), (20, -12))),
        "Cast_Pull": (39, ((0, BRACE), (12, BRACE), (17, THRUST), (23, DRAWN), (27, REACH), (32, crouch(22, 8)), (39, STANDS)),
                      ((0, 25), (6, -10), (12, 30), (17, -30), (23, 35), (27, -10), (32, 30), (39, 8))),
        "Fly_Rise": (36, ((0, STANDS), (9, crouch(38, 16)), (15, STRETCH), (36, FLYING)),
                     ((0, 8), (9, 52), (15, -30), (24, 28), (36, 10))),
        "Fly_Land": (30, ((0, FLYING), (12, REACH), (18, crouch(30, 12)), (30, STANDS)),
                     ((0, 10), (6, 45), (12, -15), (16, 42), (20, 30), (30, 8))),
    }
    # Later still the user asked for her to go to her swords twisting: her wings folded over her body (WRAPS: degrees
    # each wing is brought round before her; in Cast_Pull they open again as she arrives), and her whole body turning
    # about its own length, once round in Cast_Fly's 20 frames (the lab plays it faster: several turns on the way).
    WRAPS = {"Cast_Fly": ((0, WRAP), (20, WRAP)), "Cast_Pull": ((0, WRAP), (7, 0), (39, 0))}
    for move, (frames, body_keys, wing_keys) in MOVES.items():
        act = actions["Idle"].copy()
        act.name = move
        act.use_fake_user = True
        use(act)
        for fc in curves(act):
            for i in range(len(fc.keyframe_points) - 1, -1, -1):
                if fc.keyframe_points[i].co[0] > frames:
                    fc.keyframe_points.remove(fc.keyframe_points[i])
            fc.update()
        for f in range(frames + 1):
            P = eased(body_keys, f)
            bpy.context.scene.frame_set(f)
            for pb in PB:
                if not pb.name.startswith("Wing"):
                    pb.rotation_quaternion = held.get(pb.name, Quaternion())
            PB["Hips"].location = PB["Hips"].bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, P["dz"]))
            bpy.context.view_layer.update()
            lean("Hips", P["hl"])
            for b in ("Spine", "Spine1", "Spine2"):
                lean(b, P["sp"])
            lean("Neck", P["nk"])
            lean("Head", P["hd"])
            for side_name, t in (("Right", "r"), ("Left", "l")):
                thigh = PB[side_name + "UpLeg"]     # opened first, as the leg hangs; then raised
                turn(thigh, Quaternion(Vector((0, 1, 0)), math.radians((1 if t == "r" else -1) * P.get("op", 0))), thigh.head.copy())
                lean(side_name + "UpLeg", P[t + "t"])
                lean(side_name + "Leg", P[t + "k"])
                lean(side_name + "Foot", P[t + "f"])
            if move == "Cast_Fly":      # the twist: about the line from her hips to her head
                along = (PB["Head"].head - PB["Hips"].head).normalized()
                turn(PB["Hips"], Quaternion(along, 2 * math.pi * f / frames), PB["Hips"].head.copy())
                q = PB["Hips"].rotation_quaternion
                if f and q.dot(spun) < 0:
                    PB["Hips"].rotation_quaternion = -q
                spun = PB["Hips"].rotation_quaternion.copy()
            if move == "Cast_Pull" and pull_pose:
                back = 0 if f <= 12 else min(30, (f - 12) * 2)
                idle = max(0.0, (f - 27) / 12.0)
                for b in pull_bones:
                    q = pull_pose[back][b]
                    PB[b].rotation_quaternion = q.slerp(held.get(b, Quaternion()), idle) if idle > 0 else q
            for pb in PB:
                if not pb.name.startswith("Wing"):
                    pb.keyframe_insert("rotation_quaternion", frame=f)
            PB["Hips"].keyframe_insert("location", frame=f)
        actions[move] = act
        print("MOVE %s %d frames, legs %.3f" % (move, frames, legs))

    # ---- their movement in every clip
    def wing_local(bone, axis, angle):
        return Quaternion(arm.data.bones[bone].matrix_local.to_3x3().inverted() @ Vector(axis), angle)

    for name, act in actions.items():
        last = use(act)
        if name in MOVES:
            for f in range(last + 1):
                for side_name, sx in (("L", 1), ("R", -1)):
                    for k, part in enumerate((1.0, 0.55, 0.45)):      # the outer bones follow, two frames late each
                        up = math.radians(eased(MOVES[name][2], max(0, f - 2 * k))) * part
                        round_her = math.radians(eased(WRAPS.get(name, ((0, 0), (1, 0))), f)) * (1.0, 0.45, 0.35)[k]
                        bone = "Wing%s%d" % (side_name, k + 1)
                        PB[bone].rotation_quaternion = wing_local(bone, (0, 0, 1), WRAP_SIGN * sx * round_her) @ wing_local(bone, (0, 1, 0), -sx * up)
                        PB[bone].keyframe_insert("rotation_quaternion", frame=f)
            print("WINGS %s made" % name)
            continue
        if name == "Fly":
            for f in range(last + 1):
                beat = 2 * math.pi * f / last
                for side_name, sx in (("L", 1), ("R", -1)):
                    for k, (far, late) in enumerate(FLY_FLAP):
                        up = far * math.sin(beat - late) + (FLY_RAISED if k == 0 else 0)
                        behind = FLY_BACK * math.cos(beat - late) if k == 0 else 0
                        bone = "Wing%s%d" % (side_name, k + 1)
                        PB[bone].rotation_quaternion = wing_local(bone, (0, 0, 1), sx * behind) @ wing_local(bone, (0, 1, 0), -sx * up)
                        PB[bone].keyframe_insert("rotation_quaternion", frame=f)
            print("WINGS Fly            beat once a second, the root %.0f degrees either way" % math.degrees(FLY_FLAP[0][0]))
            continue
        loop = name in ("Idle", "Walk", "Run")
        n = last if loop else last + 1
        chest = PB["Spine2"]
        where, turned, stands = [], [], []
        for f in range(n):
            bpy.context.scene.frame_set(f)
            where.append(chest.head.copy())
            stands.append((chest.matrix.to_3x3() @ chest.bone.matrix_local.to_3x3().inverted()).to_quaternion())
            if stands[-1].dot(stands[0]) < 0:
                stands[-1].negate()
            ahead = chest.matrix.to_3x3() @ chest.bone.matrix_local.to_3x3().inverted() @ Vector((0, -1, 0))
            a = math.atan2(ahead.x, -ahead.y)
            if turned:
                while a - turned[-1] > math.pi:
                    a -= 2 * math.pi
                while a - turned[-1] < -math.pi:
                    a += 2 * math.pi
            turned.append(a)
        at = (lambda seq, i: seq[i % n]) if loop else (lambda seq, i: seq[min(max(i, 0), n - 1)])
        rise = [(at(where, f + 1).z - 2 * at(where, f).z + at(where, f - 1).z) * FPS * FPS for f in range(n)]
        start = [-(at(where, f + 1).y - 2 * at(where, f).y + at(where, f - 1).y) * FPS * FPS for f in range(n)]
        spin = [(at(turned, f + 1) - 2 * at(turned, f) + at(turned, f - 1)) * FPS * FPS for f in range(n)]
        gain, steady = WING_GAIN.get(name, 1.0), WING_STEADY.get(name, 0.0)
        rise, start, spin = [v * gain for v in rise], [v * gain for v in start], [v * gain for v in spin]
        mean = Quaternion([sum(q[i] for q in stands) for i in range(4)]).normalized()
        breaths = max(1, round(n / FPS * 0.3))          # whole breaths in a loop, so it closes
        sweep = WING_SWEEP.get(name, 0.0)
        most = 0.0
        for side_name, sx in (("L", 1), ("R", -1)):
            flap = vflap = 0.0                           # away from where the wing is held: up, and back
            back = vback = 0.0
            track = []
            for turn_no in range(3 if loop else 1):      # a loop is gone through until the swing repeats itself
                track = []
                for f in range(n):
                    for _ in range(4):
                        h = 1.0 / FPS / 4
                        vflap += (-WING_BEAT ** 2 * flap - 2 * WING_DAMP * WING_BEAT * vflap - WING_RISE * rise[f]) * h
                        flap = max(-WING_MOST, min(WING_MOST, flap + vflap * h))
                        vback += (-WING_BEAT ** 2 * back - 2 * WING_DAMP * WING_BEAT * vback
                                  + WING_START * start[f] - sx * WING_TURN * spin[f]) * h
                        back = max(-WING_MOST, min(WING_MOST, back + vback * h))
                    track.append((flap, back))
            for f in range(last + 1):
                g = f % n if loop else f
                breath = WING_BREATH * math.sin(2 * math.pi * (breaths * g / n if loop else 0.35 * g / FPS))
                for k, (late, share) in enumerate(((0, 1.0), (2, 0.55), (4, 0.5))):
                    fl, bk = at(track, g - late)
                    up = fl * share + (WING_LIFT + breath if k == 0 else breath * 0.5)
                    behind = bk * share + (sweep if k == 0 else sweep * 0.35)
                    most = max(most, abs(math.degrees(fl)), abs(math.degrees(bk)))
                    bone = "Wing%s%d" % (side_name, k + 1)
                    turn = wing_local(bone, (0, 0, 1), sx * behind) @ wing_local(bone, (0, 1, 0), -sx * up)
                    if k == 0 and steady:
                        rest = arm.data.bones[bone].matrix_local.to_quaternion()
                        undo = stands[g].inverted() @ stands[g].slerp(mean, steady)
                        turn = (rest.inverted() @ undo @ rest) @ turn
                    PB[bone].rotation_quaternion = turn
                    PB[bone].keyframe_insert("rotation_quaternion", frame=f)
        print("WINGS %-14s move with her body, swinging up to %2.0f degrees" % (name, most))

# ---- every clip on its own track, so the exporter writes each under its name
arm.animation_data.action = None
for name, act in actions.items():
    track = arm.animation_data.nla_tracks.new()
    track.name = name
    strip = track.strips.new(name, 0, act)
    strip.name = name
    if hasattr(strip, "action_slot") and strip.action_slot is None and act.slots:
        strip.action_slot = act.slots[0]
    track.mute = True

bpy.context.view_layer.update()
pts = [body.matrix_world @ Vector(c) for c in body.bound_box]
height = max(p.z for p in pts) - min(p.z for p in pts)
bones = arm.data.bones
toes = arm.matrix_world @ bones["LeftToeBase"].head_local - arm.matrix_world @ bones["LeftFoot"].head_local
print("SIZE height %.3f m, lowest point %.3f, %d bones, %d triangles" % (height, min(p.z for p in pts), len(bones),
                                                                        sum(len(p.vertices) - 2 for p in body.data.polygons)))
print("FACES toes point along", [round(v, 2) for v in toes.normalized()], "left arm at x =",
      round((arm.matrix_world @ bones["LeftArm"].head_local).x, 3))
for name, v in speeds.items():
    print("SPEED", name, "%.6f" % v)

# The file opens showing her standing as in Idle. (It used to be saved in whatever pose the last clip worked on
# had left behind: one hand in front of her trunk, at frame 68, which looked like a broken rig in Object mode.)
use(actions["Idle"])
bpy.context.scene.frame_set(0)
bpy.context.view_layer.update()
arm.animation_data.action = None
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / "Nyxara_mixamo.blend"), check_existing=False)
for o in bpy.data.objects:
    o.select_set(o in (arm, body))
bpy.ops.export_scene.gltf(filepath=str(HERE / "Nyxara_mixamo.glb"), export_format="GLB", use_selection=True,
                          export_animation_mode="NLA_TRACKS", export_skins=True, export_influence_nb=4,
                          export_all_influences=False, export_def_bones=False, export_force_sampling=True,
                          export_frame_step=1, export_yup=True, export_image_format="AUTO",
                          export_materials="EXPORT")
print("EXPORTED")
