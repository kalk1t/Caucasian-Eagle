# Step 2: assemble the Nyxara game character from Nyxara_base.blend.
#   body (Meshy rig) + two rigged eagle wings + Moonfeather / Talonshade swords (and swapped copies)
#   final clip set (trimmed, in place, wing keys) -> Nyxara_game.blend + Nyxara_game.glb
import bpy, math, pathlib
import numpy as np
from mathutils import Vector, Matrix, Quaternion

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE.parent / "meshy" / "out"
FPS = 30
bpy.ops.wm.open_mainfile(filepath=str(HERE / "Nyxara_base.blend"))
sc = bpy.context.scene; sc.render.fps = FPS
arm = bpy.data.objects["NyxaraRig"]; body = bpy.data.objects["NyxaraBody"]
BODY_BONES = [b.name for b in arm.data.bones]


def rq(axis, deg):
    return Quaternion(Vector(axis), math.radians(deg))


def smooth(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a))); return t * t * (3 - 2 * t)


# ---------------------------------------------------------------- helpers: imported meshes
def import_mesh(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == 'MESH']
    for o in meshes:
        mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = mw
    for o in new:
        if o.type != 'MESH': bpy.data.objects.remove(o)
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes: o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1: bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return o


def verts(o):
    a = np.empty(len(o.data.vertices) * 3); o.data.vertices.foreach_get("co", a); return a.reshape(-1, 3)


def set_verts(o, V):
    o.data.vertices.foreach_set("co", V.ravel()); o.data.update()


def bind(o, weights):  # weights: {bone: np.array per vertex}
    for b, w in weights.items():
        g = o.vertex_groups.get(b) or o.vertex_groups.new(name=b)
        for i in np.nonzero(w > 1e-4)[0]: g.add([int(i)], float(w[i]), 'REPLACE')
    m = o.modifiers.new("Armature", 'ARMATURE'); m.object = arm
    o.parent = arm


def rename_material(o, name):
    m = o.data.materials[0].copy(); m.name = name; o.data.materials[0] = m
    for p in o.data.polygons: p.material_index = 0
    while len(o.data.materials) > 1: o.data.materials.pop(index=1)


# ---------------------------------------------------------------- wings
WING_LEN = 1.30
ROOT_L = Vector((0.085, 0.165, 1.30))
TILT, SWEEP = 12.0, 10.0          # rest: tip raised 12 deg, swept back 10 deg
wing_src = import_mesh(OUT / "wing_3d_1" / "model_url.glb")
W = verts(wing_src)
xmin, xmax = W[:, 0].min(), W[:, 0].max()
s = WING_LEN / (xmax - xmin)
root_band = W[W[:, 0] < xmin + 0.12 * (xmax - xmin)]
anchor = np.array([xmin, root_band[:, 1].mean(), root_band[:, 2].min() + 0.72 * np.ptp(root_band[:, 2])])
W = (W - anchor) * s                         # root anchor at origin, wing extends +X
rest_rot = (rq((0, 0, 1), SWEEP) @ rq((0, 1, 0), -TILT)).to_matrix()
W = W @ np.array(rest_rot).T
WDIR = rest_rot @ Vector((1, 0, 0))
print("WING scale", round(s, 3), "anchor", anchor.round(3), "dir", tuple(round(x, 3) for x in WDIR))

wings = {}
for side, sign in (("L", 1), ("R", -1)):
    o = wing_src if side == "L" else wing_src.copy()
    if side == "R":
        o.data = wing_src.data.copy(); sc.collection.objects.link(o)
    V = W.copy() if side == "L" else W * np.array([-1, 1, 1])
    root = Vector((ROOT_L.x * sign, ROOT_L.y, ROOT_L.z))
    set_verts(o, V + np.array(root))
    if side == "R":
        o.data.flip_normals()
    o.name = f"NyxaraWing{side}"; wings[side] = (o, root, Vector((WDIR.x * sign, WDIR.y, WDIR.z)))

SEG = (0.0, 0.34, 0.66, 1.0)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
for side, (o, root, d) in wings.items():
    parent = arm.data.edit_bones["Spine"]
    for i in range(3):
        eb = arm.data.edit_bones.new(f"Wing{side}_{i + 1}")
        eb.head = root + d * (SEG[i] * WING_LEN); eb.tail = root + d * (SEG[i + 1] * WING_LEN)
        eb.align_roll(Vector((0, 0, 1))); eb.parent = parent; eb.use_connect = i > 0; parent = eb
bpy.ops.object.mode_set(mode='OBJECT')
for side, (o, root, d) in wings.items():
    V = verts(o); u = ((V - np.array(root)) @ np.array(d)) / WING_LEN
    f1 = np.array([smooth(SEG[1] - .07, SEG[1] + .07, x) for x in u])
    f2 = np.array([smooth(SEG[2] - .07, SEG[2] + .07, x) for x in u])
    bind(o, {f"Wing{side}_1": 1 - f1, f"Wing{side}_2": f1 - f2, f"Wing{side}_3": f2})
    rename_material(o, "NyxaraWings")
wings["R"][0].data.materials[0] = wings["L"][0].data.materials[0]


# ---------------------------------------------------------------- swords
def hand_frame(hand):
    bn = arm.data.bones[hand]; d = (bn.tail_local - bn.head_local).normalized()
    f = Vector((0, -1, 0)); b = (f - d * f.dot(d)).normalized()       # blade forward, across the fist
    w = (d - b * d.dot(b)).normalized()                               # edge follows the knuckles
    g = body.vertex_groups[hand].index
    pts = [v.co for v in body.data.vertices if any(e.group == g and e.weight > .5 for e in v.groups)]
    grip = sum(pts, Vector()) / len(pts)
    return b, w, grip


def make_sword(path, length, hand, material):
    o = import_mesh(path); V = verts(o)
    xs = np.linspace(V[:, 0].min(), V[:, 0].max(), 80); ext = []
    for a, c in zip(xs[:-1], xs[1:]):
        sl = V[(V[:, 0] >= a) & (V[:, 0] < c)]; ext.append(np.ptp(sl[:, 2]) if len(sl) else 0)
    gi = int(np.argmax(ext)); guard = (xs[gi] + xs[gi + 1]) / 2
    pommel_pos = V[:, 0].max() if V[:, 0].max() - guard < guard - V[:, 0].min() else V[:, 0].min()
    sgn = 1 if pommel_pos > guard else -1                       # model +X is pommel side when sgn == 1
    scale = length / np.ptp(V[:, 0])
    gx = guard + 0.42 * (pommel_pos - guard)
    zc = V[np.abs(V[:, 0] - gx) < .03][:, 2].mean(); yc = V[np.abs(V[:, 0] - gx) < .03][:, 1].mean()
    V = (V - np.array([gx, yc, zc])) * scale
    b, w, grip = hand_frame(hand)
    src = Matrix(((-sgn, 0, 0), (0, -sgn, 0), (0, 0, 1))).transposed()  # columns: blade, w x blade, edge
    dst = Matrix((b, w.cross(b), w)).transposed()
    M = dst @ src.inverted()
    V = V @ np.array(M).T + np.array(grip)
    set_verts(o, V); o.name = material
    bind(o, {hand: np.ones(len(V))}); rename_material(o, material)
    print("SWORD", material, "guard", round(guard, 3), "sgn", sgn, "scale", round(scale, 3))
    return o


swords = [make_sword(OUT / "swordA_3d_1" / "model_url.glb", 1.02, "RightHand", "Slot_sword_right__Moonfeather"),
          make_sword(OUT / "swordB_3d_1" / "model_url.glb", 0.92, "LeftHand", "Slot_sword_left__Talonshade"),
          make_sword(OUT / "swordA_3d_1" / "model_url.glb", 1.02, "LeftHand", "Slot_sword_left__Moonfeather"),
          make_sword(OUT / "swordB_3d_1" / "model_url.glb", 0.92, "RightHand", "Slot_sword_right__Talonshade")]
body.data.materials[0].name = "NyxaraBody"

# ---------------------------------------------------------------- clip authoring
ALL_BONES = [b.name for b in arm.data.bones]
REST = {b.name: b.matrix_local.to_3x3() for b in arm.data.bones}


def fcurves(act):
    out = []
    for layer in act.layers:
        for strip in layer.strips:
            for bag in strip.channelbags: out += list(bag.fcurves)
    return out


SRC = {}
for act in list(bpy.data.actions):
    ch = {}
    for fc in fcurves(act):
        if fc.data_path.startswith('pose.bones["'):
            bone = fc.data_path.split('"')[1]; ch[(bone, fc.data_path.rsplit(".", 1)[1], fc.array_index)] = fc
    SRC[act.name] = (act, ch)


def sample(name, f):
    act, ch = SRC[name]; f = min(max(f, act.frame_range[0]), act.frame_range[1]); P = {}
    for b in BODY_BONES:
        loc = Vector([ch[(b, "location", i)].evaluate(f) if (b, "location", i) in ch else 0 for i in range(3)])
        q = Quaternion([ch[(b, "rotation_quaternion", i)].evaluate(f) if (b, "rotation_quaternion", i) in ch else (1 if i == 0 else 0) for i in range(4)])
        P[b] = [loc, q.normalized()]
    return P


def blend(A, B, t):
    return {b: [A[b][0].lerp(B[b][0], t), A[b][1].slerp(B[b][1], t)] for b in A}


def world_rot_basis(bone, R, basis):  # rotate a bone about its head in the armature rest frame
    rest = REST[bone]; return (rest.inverted() @ R.to_matrix() @ rest).to_quaternion() @ basis


def hips_ground(P, name, f):  # remove airborne travel: keep the lowest toe on the ground
    arm.animation_data.action = SRC[name][0]; arm.animation_data.action_slot = SRC[name][0].slots[0]
    sc.frame_set(int(f)); pb = arm.pose.bones
    toe = min((arm.matrix_world @ pb["LeftToeBase"].head).z, (arm.matrix_world @ pb["RightToeBase"].head).z)
    lift = max(0.0, toe - 0.016)
    P["Hips"][0] = P["Hips"][0] - REST["Hips"].inverted() @ Vector((0, 0, lift))
    return P


# wing poses: folded (ground) / spread with flap, per side
# folded: long axis down/back beside the spine, feather chord trailing backwards, surface facing outwards
_R0 = Matrix(((0, 1, 0), (0, 0, -1), (-1, 0, 0)))
_FL = (rq((0, 1, 0), -9) @ rq((1, 0, 0), 12)).to_matrix() @ _R0
_S = Matrix(((-1, 0, 0), (0, 1, 0), (0, 0, 1)))
FOLD = {"L": _FL.to_quaternion(), "R": (_S @ _FL @ _S).to_quaternion()}


# Open rest pose (replaces the hanging fold on the ground): wings spread at shoulder height, swept
# slightly back, tips drooping a little.
RAISE, RAISE_SWEEP, BEND2, BEND3 = 6, 14, 8, 10  # open wings held level with the shoulders
UP = {side: rq((0, 0, 1), sg * RAISE_SWEEP) @ rq((0, 1, 0), -sg * RAISE) for side, sg in (("L", 1), ("R", -1))}


def wing_keys(spread, flap=0.0, sweep=0.0, breathe=0.0):
    keys = {}
    for side, sg in (("L", 1), ("R", -1)):
        open_q = rq((0, 0, 1), sg * sweep) @ rq((0, 1, 0), -sg * flap)
        rest_q = rq((1, 0, 0), breathe) @ UP[side]
        keys[f"Wing{side}_1"] = rest_q.slerp(open_q, spread)
        keys[f"Wing{side}_2"] = Quaternion().slerp(rq((0, 1, 0), sg * BEND2), 1 - spread)
        keys[f"Wing{side}_3"] = Quaternion().slerp(rq((0, 1, 0), sg * BEND3), 1 - spread)
    return keys


def flap_keys(t, period, amp, sweep, spread=1.0, base=8.0):
    ph = 2 * math.pi * t / period
    beat = lambda x: math.sin(x + 0.55 * math.cos(x))   # skewed: quick power stroke, slower recovery
    k = wing_keys(spread, base + amp * beat(ph), sweep + 5 * math.cos(ph))
    for side, sg in (("L", 1), ("R", -1)):
        k[f"Wing{side}_2"] = rq((0, 1, 0), -sg * 0.55 * amp * beat(ph - 0.8) * spread)
        k[f"Wing{side}_3"] = rq((0, 1, 0), -sg * 0.50 * amp * beat(ph - 1.5) * spread)
    return k


def wing_drag(wk, e1, e2):
    """Follow-through: body/arm swing energy throws the wings back and up, tips trailing."""
    for side, sg in (("L", 1), ("R", -1)):
        wk[f"Wing{side}_1"] = rq((0, 0, 1), sg * 20 * e1) @ rq((0, 1, 0), -sg * 16 * e1) @ wk[f"Wing{side}_1"]
        wk[f"Wing{side}_2"] = rq((0, 1, 0), sg * 14 * e2) @ wk[f"Wing{side}_2"]
        wk[f"Wing{side}_3"] = rq((0, 1, 0), sg * 20 * e2) @ wk[f"Wing{side}_3"]
    return wk


ENERGY_BONES = ("Hips", "Spine02", "Spine01", "Spine", "LeftArm", "RightArm", "LeftForeArm", "RightForeArm")


def swing_energy(poses):
    """Per-frame 0..1 swing energy (lagged twice for inner / outer wing follow-through)."""
    raw = [0.0]
    for A, B in zip(poses[:-1], poses[1:]):
        raw.append(sum(math.degrees(A[b][1].rotation_difference(B[b][1]).angle) for b in ENERGY_BONES))
    raw = [min(1.0, max(0.0, (r - 6.0) / 38.0)) for r in raw]
    e1, e2, a, b = [], [], 0.0, 0.0
    for r in raw:
        a += (r - a) * (0.55 if r > a else 0.16); b += (a - b) * 0.30   # fast attack, slow release
        e1.append(a); e2.append(b)
    n = len(raw)   # settle to rest at both ends so clips blend cleanly
    for i in range(n):
        w = smooth(0, 3, i) * (1 - smooth(n - 7, n - 1, i)); e1[i] *= w; e2[i] *= w
    return e1, e2


CLIPS = []


def emit(name, frames, loop):
    """frames: list of (pose, wingkeys). Writes a fresh action named `name`."""
    if name in bpy.data.actions: bpy.data.actions[name].name = name + "_src"
    act = bpy.data.actions.new(name); act.use_fake_user = True
    slot = act.slots.new(id_type='OBJECT', name="NyxaraRig")
    layer = act.layers.new("Layer"); strip = layer.strips.new(type='KEYFRAME'); bag = strip.channelbag(slot, ensure=True)
    n = len(frames); data = {}
    for i, (P, WK) in enumerate(frames):
        for b in BODY_BONES:
            for k in range(3): data.setdefault((b, "location", k), []).append(P[b][0][k])
            for k in range(4): data.setdefault((b, "rotation_quaternion", k), []).append(P[b][1][k])
        for b, q in WK.items():  # wing keys are authored in the armature rest frame
            q = (REST[b].inverted() @ q.to_matrix() @ REST[b]).to_quaternion()
            for k in range(4): data.setdefault((b, "rotation_quaternion", k), []).append(q[k])
    for (b, prop, k), vals in data.items():
        fc = bag.fcurves.new(f'pose.bones["{b}"].{prop}', index=k, group_name=b)
        fc.keyframe_points.add(n)
        co = np.empty(2 * n); co[0::2] = np.arange(n) + 1; co[1::2] = vals
        fc.keyframe_points.foreach_set("co", co)
        fc.keyframe_points.foreach_set("interpolation", [2] * n)  # LINEAR
        fc.update()
    CLIPS.append((name, loop, n))


def mirror_pose(P):
    """Left/right mirror of a body pose (exact in the armature frame), e.g. a right-hand slash becomes a left-hand one."""
    Q = {}
    for b, (loc, q) in P.items():
        m = "Right" + b[4:] if b.startswith("Left") else "Left" + b[5:] if b.startswith("Right") else b
        if m not in P: m = b
        d = (REST[b] @ q.to_matrix() @ REST[b].inverted()).to_quaternion(); d = Quaternion((d.w, d.x, -d.y, -d.z))
        v = REST[b] @ loc; v.x = -v.x
        Q[m] = [REST[m].inverted() @ v, (REST[m].inverted() @ d.to_matrix() @ REST[m]).to_quaternion()]
    return Q


CHIN_NECK, CHIN_HEAD = -12.0, -23.0   # proud, level gaze: the source clips stare ~30 deg at the floor


def chin_up(P, k=1.0):
    P["neck"][1] = world_rot_basis("neck", rq((1, 0, 0), CHIN_NECK * k), P["neck"][1])
    P["Head"][1] = world_rot_basis("Head", rq((1, 0, 0), CHIN_HEAD * k), P["Head"][1])
    return P


def ground(name, f0, f1, loop=False, flare=0.0, flare_span=(0.15, 0.85), rate=1.0, toe=False, droop=False, mirror=False, chin=1.0):
    act = SRC[name][0]; f1 = min(f1, act.frame_range[1]); count = int(round((f1 - f0) / rate)) + 1
    dur = (count - 1) / FPS; out = []
    for i in range(count):
        f = f0 + i * rate; P = sample(name, f)
        if toe: P = hips_ground(P, name, f)
        if mirror: P = mirror_pose(P)
        if chin: P = chin_up(P, chin)
        t = i / max(1, count - 1)
        br = 2.5 * math.sin(2 * math.pi * t * max(1, round(dur / 2.5))) if loop else 0.0
        sp = flare * smooth(flare_span[0] - .12, flare_span[0], t) * (1 - smooth(flare_span[1], flare_span[1] + .12, t))
        wk = wing_keys(sp, flap=10 * sp, sweep=10 * sp, breathe=br + (12 if droop else 0) * t)
        if loop:   # resting wings flex slowly, tips trailing the shoulder
            ph = 2 * math.pi * t * max(1, round(dur / 2.5))
            for side, sg in (("L", 1), ("R", -1)):
                wk[f"Wing{side}_1"] = rq((0, 1, 0), -sg * 4.5 * math.sin(ph)) @ wk[f"Wing{side}_1"]
                wk[f"Wing{side}_2"] = rq((0, 1, 0), -sg * 5 * math.sin(ph - .9)) @ wk[f"Wing{side}_2"]
                wk[f"Wing{side}_3"] = rq((0, 1, 0), -sg * 7 * math.sin(ph - 1.7)) @ wk[f"Wing{side}_3"]
        out.append((P, wk))
    if not loop and not droop and count > 12:
        e1, e2 = swing_energy([P for P, _ in out])
        out = [(P, wing_drag(wk, e1[i], e2[i])) for i, (P, wk) in enumerate(out)]
    return center(out)


def center(frames):  # keep the hips over the root on average (collision/visual alignment)
    R = REST["Hips"]; m = sum((R @ P["Hips"][0] for P, _ in frames), Vector()) / len(frames)
    d = R.inverted() @ Vector((m.x, m.y, 0))
    for P, _ in frames: P["Hips"][0] = P["Hips"][0] - d
    return frames


arm.animation_data_create()
emit("Idle", ground("Idle", 1, 51, loop=True), True)
emit("Walk", ground("WalkWoman", 1, 30, loop=True), True)
emit("Run", ground("Run", 1, 20, loop=True), True)
emit("WalkBackward", ground("WalkBackward", 1, 41, loop=True), True)
for nm, sg in (("StrafeLeft", 1), ("StrafeRight", -1)):
    fr = []
    for P, wk in ground("Run", 1, 20, loop=True):
        P["Hips"][1] = rq((0, 1, 0), sg * 62) @ P["Hips"][1]
        P["Spine02"][1] = rq((0, 1, 0), -sg * 20) @ P["Spine02"][1]
        P["Spine01"][1] = rq((0, 1, 0), -sg * 22) @ P["Spine01"][1]
        P["Spine"][1] = rq((0, 1, 0), -sg * 20) @ P["Spine"][1]
        fr.append((P, wk))
    emit(nm, fr, True)
# jump: takeoff / airborne (feet grounded to root, wings half open) / landing
emit("Jump", ground("JumpSrc", 11, 19, toe=True), False)
air = ground("JumpSrc", 19, 33, toe=True)
for i, (P, wk) in enumerate(air):
    air[i] = (P, wing_keys(0.55, flap=15, sweep=15))
emit("JumpAirborne", air, False)
land = ground("JumpSrc", 33, 53)
for i, (P, wk) in enumerate(land):
    land[i] = (P, wing_keys(0.55 * (1 - smooth(0, 0.6, i / (len(land) - 1))), flap=15, sweep=15))
emit("JumpLanding", land, False)
emit("BasicAttack", ground("BasicAttack", 1, 46, mirror=True), False)   # left-hand slash (LMB opener)
emit("SlashLeft", ground("SlashLeft", 1, 60), False)
emit("HeavyAttack", ground("HeavyAttack", 1, 67, flare=0.3), False)
emit("Dodge", ground("Dodge", 1, 56), False)
emit("HitReact", ground("HitReact", 1, 50, chin=0), False)
emit("Death", ground("Death", 1, 67, droop=True, chin=0), False)
emit("Respawn", ground("RespawnSrc", 54, 200, chin=0), False)
emit("Skill1", ground("Skill1", 1, 91, flare=0.45), False)
emit("Skill2", ground("Skill2", 1, 171, flare=0.7, flare_span=(0.12, 0.8)), False)
emit("Skill3", ground("Skill3", 1, 95, flare=1.0, flare_span=(0.15, 0.55)), False)
emit("Combo1L", ground("Combo1L", 1, 86, flare=0.4), False)
emit("Combo1R", ground("Combo1R", 1, 96, flare=0.85, flare_span=(0.1, 0.6)), False)
emit("Combo2L", ground("Combo2L", 1, 112, flare=0.5), False)
emit("Combo2R", ground("Combo2R", 1, 76, flare=0.9, flare_span=(0.2, 0.75)), False)
emit("Combo3L", ground("Combo3L", 1, 118, flare=0.5), False)
emit("Combo3R", ground("Combo3R", 35, 140, flare=1.0, flare_span=(0.2, 0.55)), False)
g = ground("Guard", 1, 41, chin=0)
emit("Guard", [(P, wing_keys(0.25 * smooth(0, .3, i / (len(g) - 1)), sweep=-20)) for i, (P, wk) in enumerate(g)], False)
sw = ground("Guard", 1, 14, chin=0); emit("SwordSwap", sw + sw[-2::-1], False)


# flight
def flight_body(lean):
    P = sample("Idle", 1); L = sample("JumpOpenSrc", 49)
    for b in ("Hips", "Spine02", "Spine01", "Spine", "neck", "Head",
              "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase", "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase"):
        P[b] = [L[b][0].copy(), L[b][1].copy()]   # square, forward-facing torso; arms keep the sword guard
    P["Hips"][0] = sample("Idle", 1)["Hips"][0].copy()
    if lean:
        P["Hips"][1] = world_rot_basis("Hips", rq((1, 0, 0), lean), P["Hips"][1])
        P["neck"][1] = rq((1, 0, 0), -0.35 * lean) @ P["neck"][1]
        P["Head"][1] = rq((1, 0, 0), -0.25 * lean) @ P["Head"][1]
        for b in ("LeftUpLeg", "RightUpLeg", "LeftLeg", "RightLeg"):
            P[b][1] = P[b][1].slerp(Quaternion(), 0.7)
    return center([(P, None)])[0][0]


HOVER, TRAVEL = chin_up(flight_body(0), .3), flight_body(52)


def bob(P, t, period, amp, life=1.0):
    Q = {b: [v[0].copy(), v[1].copy()] for b, v in P.items()}
    ph = 2 * math.pi * t / period; slow = ph * .5   # slow sway spans two wing beats
    Q["Hips"][0] = Q["Hips"][0] + REST["Hips"].inverted() @ Vector((0, 0, amp * math.sin(ph - 1.2)))
    def add(bone, axis, deg): Q[bone][1] = world_rot_basis(bone, rq(axis, deg * life), Q[bone][1])
    X, Y = (1, 0, 0), (0, 1, 0)
    add("Hips", X, 2.5 * math.sin(ph - 1.6)); add("Hips", Y, 2.2 * math.sin(slow))          # pumps with each beat, slow roll
    add("Spine01", X, -2.0 * math.sin(ph - 1.9)); add("Spine", X, -2.5 * math.sin(ph - 2.2))  # chest opens on the down stroke
    add("neck", X, 1.5 * math.sin(ph - 2.0)); add("Head", X, 1.5 * math.sin(ph - 2.4))       # head stays level
    for side, sg in (("Left", 1), ("Right", -1)):   # legs trail the body, alternating a little
        add(side + "UpLeg", X, 5 * math.sin(ph - 2.3) + sg * 3.5 * math.sin(slow - .6))
        add(side + "Leg", X, -7 * math.sin(ph - 3.0) - sg * 3 * math.sin(slow - 1.2))
        add(side + "Foot", X, 6 * math.sin(ph - 3.6))
        add(side + "Arm", X, 2.5 * math.sin(ph - 2.2)); add(side + "Arm", Y, sg * 2.5 * math.sin(ph - 1.4))
        add(side + "ForeArm", X, 3.5 * math.sin(ph - 2.9))
    return Q


n = 48; emit("FlightHover", [(bob(HOVER, i / FPS, .8, .06), flap_keys(i / FPS, .8, 40, 6)) for i in range(n + 1)], True)
n = 36; emit("FlightTravel", [(bob(TRAVEL, i / FPS, .6, .03), flap_keys(i / FPS, .6, 32, 22)) for i in range(n + 1)], True)
fr = []
for i in range(16):
    t = i / 15; crouch = sample("JumpSrc", 9 + 7 * t)
    P = blend(crouch, HOVER, smooth(.35, 1, t))
    k = flap_keys(.2 + .6 * t, .8, 45, 6, spread=smooth(0, .5, t))
    fr.append((P, k))
emit("FlightTakeoff", fr, False)
fr = []
for i in range(19):
    t = i / 18; ld = sample("JumpSrc", 33 + 14 * t)
    P = blend(HOVER, ld, smooth(0, .45, t))
    fr.append((P, flap_keys(i / FPS, .8, 30 * (1 - t), 6, spread=1 - smooth(.3, 1, t))))
emit("FlightLanding", fr, False)

# drop source actions; keep only emitted clips
keep = {c[0] for c in CLIPS}
for a in list(bpy.data.actions):
    if a.name not in keep: bpy.data.actions.remove(a)
arm.animation_data.action = bpy.data.actions["Idle"]; arm.animation_data.action_slot = bpy.data.actions["Idle"].slots[0]

# Doorways joint contract (DoorwaysCharacterPresentation): Root/Hips/Chest/UpperArm/LowerArm/Hand/UpperLeg names.
RENAME = {"LeftHand": "Hand_L", "RightHand": "Hand_R", "Spine": "Chest",
          "LeftArm": "UpperArm_L", "RightArm": "UpperArm_R", "LeftForeArm": "LowerArm_L", "RightForeArm": "LowerArm_R",
          "LeftUpLeg": "UpperLeg_L", "RightUpLeg": "UpperLeg_R"}
for old, new in RENAME.items():
    arm.data.bones[old].name = new
for act in bpy.data.actions:
    for fc in fcurves(act):
        for old, new in RENAME.items():
            key = f'pose.bones["{old}"]'
            if fc.data_path.startswith(key): fc.data_path = f'pose.bones["{new}"]' + fc.data_path[len(key):]
for o in [body] + [w[0] for w in wings.values()] + swords:
    for old, new in RENAME.items():
        g = o.vertex_groups.get(old)
        if g is not None and o.vertex_groups.get(new) is None: g.name = new
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
root = arm.data.edit_bones.new("Root"); root.head = (0, 0, 0); root.tail = (0, 0, 0.12); root.roll = 0
arm.data.edit_bones["Hips"].parent = root
bpy.ops.object.mode_set(mode='OBJECT')
print("JOINTS", [b.name for b in arm.data.bones])

# single skinned mesh: body + wings + swords
bpy.ops.object.select_all(action='DESELECT')
parts = [body] + [w[0] for w in wings.values()] + swords
for o in parts: o.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.join(); body.name = "Nyxara"
# glTF-safe materials: opaque, no emission/alpha links
for m in body.data.materials:
    if m.node_tree:
        for nd in m.node_tree.nodes:
            if nd.type == 'BSDF_PRINCIPLED':
                for inp in ("Alpha", "Emission Color", "Emission Strength"):
                    for l in list(nd.inputs[inp].links): m.node_tree.links.remove(l)
                nd.inputs["Alpha"].default_value = 1.0; nd.inputs["Emission Strength"].default_value = 0.0
    m.surface_render_method = 'DITHERED'
def grade(img, gamma, gain, sat):
    px = np.empty(len(img.pixels), dtype=np.float32); img.pixels.foreach_get(px); px = px.reshape(-1, 4)
    rgb = np.clip(px[:, :3], 0, 1) ** gamma * gain
    lum = rgb @ np.array([.2126, .7152, .0722], dtype=np.float32)
    px[:, :3] = np.clip(lum[:, None] + (rgb - lum[:, None]) * sat, 0, 1)
    if img.name == BODY_IMAGE:   # pitch-black eyes: the teal irises and a few pixels around them
        w, h = img.size; c = px[:, :3].reshape(h, w, 3); mx, mn = c.max(-1), c.min(-1)
        m = ((mx - mn) / (mx + 1e-6) > .45) & (mx > .45) & (c[..., 1] > c[..., 0] * 1.3) & (c[..., 2] > c[..., 0] * 1.3)
        grow = m.copy()
        for _ in range(5):
            g = grow.copy(); g[1:] |= grow[:-1]; g[:-1] |= grow[1:]; g[:, 1:] |= grow[:, :-1]; g[:, :-1] |= grow[:, 1:]; grow = g
        c[grow] = 0.004; print("EYES", int(m.sum()), "->", int(grow.sum()), "px")
    img.pixels.foreach_set(px.ravel()); img.update(); img.pack()


BODY_IMAGE = body.data.materials["NyxaraBody"].node_tree.nodes["Principled BSDF"].inputs["Base Color"].links[0].from_node.image.name
GRADE = {"NyxaraBody": (0.74, 1.12, 1.15), "NyxaraWings": (0.70, 1.18, 1.25)}
for m in body.data.materials:
    if m.name in GRADE and m.node_tree:
        for nd in m.node_tree.nodes:
            if nd.type == 'BSDF_PRINCIPLED' and nd.inputs["Base Color"].links:
                src = nd.inputs["Base Color"].links[0].from_node
                if src.type == 'TEX_IMAGE' and src.image:
                    grade(src.image, *GRADE[m.name]); print("GRADED", m.name, src.image.name, tuple(src.image.size))
print("TRIS", sum(len(p.vertices) - 2 for p in body.data.polygons), "BONES", len(arm.data.bones),
      "MATS", [m.name for m in body.data.materials])
for c in CLIPS: print("CLIP", c[0], "loop" if c[1] else "once", round((c[2] - 1) / FPS, 3), "s")
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / "Nyxara_game.blend"))
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); arm.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(HERE / "Nyxara_game.glb"), export_format='GLB', use_selection=True,
                          export_animation_mode='ACTIONS', export_skins=True, export_influence_nb=4,
                          export_all_influences=False, export_def_bones=False, export_force_sampling=True,
                          export_frame_step=1, export_anim_single_armature=True, export_yup=True,
                          export_image_format='AUTO', export_materials='EXPORT')
print("EXPORTED")
