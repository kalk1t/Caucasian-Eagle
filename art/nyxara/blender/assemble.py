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
ROOT_L = Vector((0.085, 0.165, 1.20))   # where the left wing leaves her back (z: 1.30 was at the shoulders, 1.20 is the shoulder blades)
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


def tint_weapon(o, colour, glow):
    """The scythe's texture is near black. Each copy gets its own picture: the dark metal lifted and washed with
    `colour`, and the model's glowing veins (its emission map) painted in `glow`."""
    m = o.data.materials[0]; bsdf = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    node = bsdf.inputs["Base Color"].links[0].from_node
    img = node.image.copy(); img.name = m.name + "_BaseColor"; node.image = img
    px = np.empty(len(img.pixels), dtype=np.float32); img.pixels.foreach_get(px); px = px.reshape(-1, 4)
    lum = np.clip(px[:, :3], 0, 1) @ np.array([.2126, .7152, .0722], dtype=np.float32)
    rgb = (0.10 + 0.90 * lum[:, None] ** 0.55) * np.array(colour, dtype=np.float32)
    emit = next((n.image for n in m.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image and "Emit" in n.image.name), None)
    if emit is not None and tuple(emit.size) == tuple(img.size):
        e = np.empty(len(emit.pixels), dtype=np.float32); emit.pixels.foreach_get(e); e = e.reshape(-1, 4)[:, :3].max(1)
        rgb = rgb + np.clip(e * 3.0, 0, 1)[:, None] * np.array(glow, dtype=np.float32)
    px[:, :3] = np.clip(rgb, 0, 1)
    img.pixels.foreach_set(px.ravel()); img.update(); img.scale(1024, 1024); img.pack()
    for inp in ("Emission Color", "Emission"):   # the glow is in the picture now
        if inp in bsdf.inputs:
            for l in list(bsdf.inputs[inp].links): m.node_tree.links.remove(l)


def make_scythe(path, length, hand, material, colour, glow, hook=1, frame=None, grip_up=None):
    """Meshy scythe: shaft along model Z (head at +Z), blade hooking toward model -X. She holds the shaft below its
    middle, head forward out of the fist like a sword's blade, the hook curving the way her knuckles face."""
    o = import_mesh(path); V = verts(o)
    scale = length / np.ptp(V[:, 2])
    shaft = V[V[:, 2] < V[:, 2].min() + 0.45 * np.ptp(V[:, 2])]           # the lower half is shaft only
    cx, cy = (shaft[:, 0].min() + shaft[:, 0].max()) / 2, (shaft[:, 1].min() + shaft[:, 1].max()) / 2
    gz = V[:, 2].min() + (GRIP_UP if grip_up is None else grip_up) * np.ptp(V[:, 2])                        # the grip: a good quarter up from the butt
    V = (V - np.array([cx, cy, gz])) * scale
    # thinner (THIN): across the blade's flat everything is THIN as thick; along the hook the shaft is THIN as thick
    # and what lies beyond the shaft (the blade) only moves in by what the shaft lost, so the blade keeps its reach
    r = (shaft[:, 0].max() - shaft[:, 0].min()) / 2 * scale
    V[:, 1] *= THIN
    ax = np.abs(V[:, 0]); V[:, 0] = np.sign(V[:, 0]) * np.maximum(ax * THIN, ax - r * (1 - THIN))
    # frame: (shaft direction, hook direction, grip point) in her rest pose, for a scythe that is not in a hand
    b, w, grip = frame if frame else hand_frame(hand)
    w = w * hook   # hook: 1 = the blade curves the way this hand's knuckles face; -1 = the other way (turned half round the shaft)
    src = Matrix(((0, 0, 1), (0, 1, 0), (-1, 0, 0))).transposed()          # columns: shaft toward the head, across, hook
    dst = Matrix((b, w.cross(b), w)).transposed()
    M = dst @ src.inverted()
    V = V @ np.array(M).T + np.array(grip)
    set_verts(o, V); o.name = material
    bind(o, {hand: np.ones(len(V))}); rename_material(o, material)
    tint_weapon(o, colour, glow)
    print("SCYTHE", material, "scale", round(scale, 3), "length", length)
    return o


# Two axes, one in each hand (before them: the pair of swords, make_sword with swordA_3d_1 / swordB_3d_1; then a
# scythe, make_scythe with scythe_3d_1). The clips were made for a blade in each hand, and the axes are held the same way.
# The material names are what the game shows and hides (NyxaraKit::ApplySwordVisibility): each axe has a copy for
# each hand, because Q exchanges them. The names after the two underscores are the old swords' and are kept.
AXE_INFERNO = OUT / "axe_inferno_3d_1" / "model_url.glb"   # Molten Inferno Axe: double-bitted, black stone with lava
AXE_WAR = OUT / "axe_war_3d_1" / "model_url.glb"           # Serpentcoil War Axe: dark iron, one great blade
AXE_LENGTH = (0.95, 1.00)   # m, butt to top: Inferno, Serpentcoil
AXE_GRIP_UP = 0.20          # where her hand holds the shaft: this part of its length up from the butt
AXE_TRIS = 7000             # triangles each copy is reduced to


def make_axe(path, length, hand, material, tris=AXE_TRIS):
    """A Meshy axe: shaft along model Z, head at +Z, blades toward model +-X. It is held like the swords were: the
    shaft out of the fist forward, her hand low on it, the blades in the plane her knuckles face. The model is
    reduced to `tris` triangles first (they come with 150 000 to 250 000). Its own texture is kept."""
    o = import_mesh(path)
    n = sum(len(f.vertices) - 2 for f in o.data.polygons)
    if n > tris:
        mod = o.modifiers.new("Reduce", 'DECIMATE'); mod.ratio = tris / n
        bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier=mod.name)
    V = verts(o)
    scale = length / np.ptp(V[:, 2])
    shaft = V[V[:, 2] < V[:, 2].min() + 0.45 * np.ptp(V[:, 2])]           # the lower half is shaft only
    cx, cy = (shaft[:, 0].min() + shaft[:, 0].max()) / 2, (shaft[:, 1].min() + shaft[:, 1].max()) / 2
    gz = V[:, 2].min() + AXE_GRIP_UP * np.ptp(V[:, 2])
    V = (V - np.array([cx, cy, gz])) * scale
    b, w, grip = hand_frame(hand)
    src = Matrix(((0, 0, 1), (0, 1, 0), (-1, 0, 0))).transposed()          # columns: shaft toward the head, across, blade
    dst = Matrix((b, w.cross(b), w)).transposed()
    V = V @ np.array(dst @ src.inverted()).T + np.array(grip)
    set_verts(o, V); o.name = material
    bind(o, {hand: np.ones(len(V))}); rename_material(o, material)
    print("AXE", material, "tris", n, "->", sum(len(f.vertices) - 2 for f in o.data.polygons), "scale", round(scale, 3))
    return o


# The bow (Meshy: Eldritch Longbow): stave along model Z, the string toward +X, the belly (away from the archer)
# toward -X, the leather grip a little below the middle. She has it in her left hand or across her back; while it is
# out, the axes are on her back instead (NyxaraKit::ApplySwordVisibility shows and hides the materials).
BOW = OUT / "bow_3d_1" / "model_url.glb"
BOW_LENGTH = 1.35                      # m, tip to tip
BOW_GRIP = (-0.035, 0.0, -0.07)        # the middle of the grip, in the model's own units
BOW_TRIS = 6000
BACK_Y = 0.27                          # m behind her spine: just clear of her back, her hair and the wing roots
BACK_BOW = dict(at=(0.02, BACK_Y, 1.16), roll=-24.0)                       # across her back, the top over her right shoulder
BACK_AXES = (dict(at=(-0.03, BACK_Y, 1.05), roll=26.0), dict(at=(0.03, BACK_Y + 0.07, 1.05), roll=-26.0))   # crossed, heads up
BACK_TRIS = 3500


def reduce_to(o, tris):
    n = sum(len(f.vertices) - 2 for f in o.data.polygons)
    if n > tris:
        mod = o.modifiers.new("Reduce", 'DECIMATE'); mod.ratio = tris / n
        bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier=mod.name)
    return n


# The engine draws glTF's right-handed model in its left-handed world without turning it over, so on the screen she is
# the mirror of what Blender shows: this rig's RightHand is the hand the player sees as her left. The bow is in it.
BOW_GAME_HAND = "RightHand"
BOW_STRING = (0.200, -0.075)     # the string in the model: x = BOW_STRING[0] + BOW_STRING[1] * z
BOW_STRING_Z = (-0.55, 0.64)     # between these heights it is taken off the bow (short ends stay at the tips)


def bow_frame(hand):
    """Model point -> her rest pose, for the bow in this hand: the stave across her fist (as an axe's shaft is), the
    belly the way her hand points, so that with her arm out toward the target the bow stands upright and the string
    is on her side of it."""
    bn = arm.data.bones[hand]; d = (bn.tail_local - bn.head_local).normalized()
    b, w, grip = hand_frame(hand)
    src = Matrix(((0, 0, 1), (-1, 0, 0), (0, -1, 0))).transposed()         # columns: stave, belly, stave x belly
    dst = Matrix((b, d, b.cross(d))).transposed()
    M = np.array(dst @ src.inverted()); scale = BOW_LENGTH / 1.901        # 1.901: the model's length
    return lambda V: ((np.asarray(V, dtype=float) - np.array(BOW_GRIP)) * scale) @ M.T + np.array(grip)


def only(o, mask):   # keep the vertices where mask is true
    import bmesh
    bm = bmesh.new(); bm.from_mesh(o.data); bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[v for v, k in zip(bm.verts, mask) if not k], context='VERTS')
    bm.to_mesh(o.data); bm.free(); o.data.update()


def make_bow(hand, material, string_material):
    """The bow in her hand, and its string as a part of its own: the game hides the string while she draws and
    looses, and draws it bent to her hand instead."""
    o = import_mesh(BOW); V = verts(o)
    on = (np.abs(V[:, 0] - (BOW_STRING[0] + BOW_STRING[1] * V[:, 2])) < 0.016) & (V[:, 2] > BOW_STRING_Z[0]) & (V[:, 2] < BOW_STRING_Z[1])
    cord = o.copy(); cord.data = o.data.copy(); sc.collection.objects.link(cord)
    only(cord, on); only(o, ~on)
    xf = bow_frame(hand); out = []
    for part, name, tris in ((o, material, BOW_TRIS), (cord, string_material, 160)):
        n = reduce_to(part, tris); set_verts(part, xf(verts(part))); part.name = name
        bind(part, {hand: np.ones(len(part.data.vertices))}); rename_material(part, name)
        print("BOW", name, "tris", n, "->", sum(len(f.vertices) - 2 for f in part.data.polygons))
        out.append(part)
    return out


def make_back(path, length, material, at, roll, tris=BACK_TRIS):
    """A weapon carried on her back: flat against it (the model's thin axis, Y, is the way her back faces), its long
    axis upright and then turned `roll` degrees, its middle at `at`. It moves with her chest."""
    o = import_mesh(path); n = reduce_to(o, tris); V = verts(o)
    V = (V - (V.min(0) + V.max(0)) / 2) * (length / np.ptp(V[:, 2]))
    V = V @ np.array(rq((0, 1, 0), roll).to_matrix()).T + np.array(at)
    set_verts(o, V); o.name = material
    bind(o, {"Spine": np.ones(len(V))}); rename_material(o, material)
    print("BACK", material, "tris", n, "->", sum(len(f.vertices) - 2 for f in o.data.polygons))
    return o


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
        for key in fc.keyframe_points:
            key.interpolation = "LINEAR"  # Named RNA value: numeric 2 selects BEZIER.
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


def ground(name, f0, f1, loop=False, flare=0.0, flare_span=(0.15, 0.85), rate=1.0, toe=False, droop=False, mirror=False, chin=1.0, wild=0.0, beats=2):
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
        if wild:   # a hunting gait: the back low, the head up and forward, the wings half open, beating with her stride
            lean = WILD_LEAN * wild; st = 2 * math.pi * t * beats
            P["Spine"][1] = world_rot_basis("Spine", rq((1, 0, 0), 0.6 * lean), P["Spine"][1])
            P["Spine01"][1] = world_rot_basis("Spine01", rq((1, 0, 0), 0.4 * lean), P["Spine01"][1])
            P["neck"][1] = world_rot_basis("neck", rq((1, 0, 0), -0.45 * lean + 2.5 * wild * math.sin(st)), P["neck"][1])
            P["Head"][1] = world_rot_basis("Head", rq((1, 0, 0), -0.35 * lean), P["Head"][1])
            wk = wing_keys(WILD_SPREAD * wild, flap=wild * (5 + WILD_BEAT * math.sin(st - .6)), sweep=WILD_SWEEP * wild, breathe=br)
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


WILD_LEAN = 30.0     # degrees her back comes forward at a full run (wild = 1)
WILD_SPREAD = 0.42   # how far her wings open at a full run (0 folded, 1 spread wide)
WILD_BEAT = 13.0     # degrees her half-open wings beat with each stride
WILD_SWEEP = 22.0    # degrees they are swept back


def restless(frames, times=3):
    """Standing, she is never still: the idle loop is played `times` over, and across it she looks left, then right,
    in the quick turns of a bird's head (a snap, a hold, a snap back), cocks it once, and shakes her wings out."""
    out = []
    for k in range(times):
        for P, wk in frames[:-1] if k < times - 1 else frames:
            out.append(({b: [v[0].copy(), v[1].copy()] for b, v in P.items()}, dict(wk)))
    n = len(out)
    hold = lambda t, a, b, e=.022: smooth(a - e, a + e, t) * (1 - smooth(b - e, b + e, t))   # 0 -> 1 fast, held, 1 -> 0 fast
    for i, (P, wk) in enumerate(out):
        t = i / (n - 1)
        yaw = IDLE_LOOK[0] * hold(t, .10, .27) - IDLE_LOOK[1] * hold(t, .42, .60)
        cock = IDLE_COCK * hold(t, .48, .60, .03)
        P["neck"][1] = world_rot_basis("neck", rq((0, 0, 1), .35 * yaw), P["neck"][1])
        P["Head"][1] = world_rot_basis("Head", rq((0, 1, 0), cock) @ rq((0, 0, 1), .65 * yaw), P["Head"][1])
        bell = hold(t, .76, .90, .05)
        if bell > 0:
            shake = wing_keys(IDLE_SHAKE * bell, flap=10 * bell * math.sin(2 * math.pi * 5 * (t - .76) / .14), sweep=8 * bell)
            for key in wk: wk[key] = wk[key].slerp(shake[key], bell)
    return out


IDLE_LOOK = (24.0, 30.0)   # degrees her head snaps to her left, then to her right
IDLE_COCK = 9.0            # degrees she cocks it while she looks right
IDLE_SHAKE = 0.32          # how far her wings open when she shakes them out


def center(frames):  # keep the hips over the root on average (collision/visual alignment)
    R = REST["Hips"]; m = sum((R @ P["Hips"][0] for P, _ in frames), Vector()) / len(frames)
    d = R.inverted() @ Vector((m.x, m.y, 0))
    for P, _ in frames: P["Hips"][0] = P["Hips"][0] - d
    return frames


arm.animation_data_create()
emit("Idle", restless(ground("Idle", 1, 51, loop=True)), True)
emit("Walk", ground("WalkWoman", 1, 30, loop=True, wild=0.45), True)   # stalking
emit("Run", ground("Run", 1, 20, loop=True, wild=1.0), True)           # hunting
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
        for b in ("LeftArm", "RightArm"):   # arms and swords swept back along her sides, as an eagle tucks its talons
            P[b][1] = world_rot_basis(b, rq((1, 0, 0), 20 + 0.9 * lean), P[b][1])
    return center([(P, None)])[0][0]


TRAVEL_LEAN = 42   # degrees the body tips forward in travel. 64 lies flat (her hips-to-neck line at 88 from upright); 42 is about 66
HOVER, TRAVEL = chin_up(flight_body(0), .3), flight_body(TRAVEL_LEAN)   # travel: the body leans along the flight, as an eagle soars


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


# hover: slow full beats, the body riding each one. travel: soaring (irao), wings held wide with a deep slow stroke
n = 66; emit("FlightHover", [(bob(HOVER, i / FPS, 1.1, .10, 1.7), flap_keys(i / FPS, 1.1, 36, 6)) for i in range(n + 1)], True)
def soar_keys(t, period, amp):
    """Wings for the level body. Lying forward turns the wing axes: what raises a tip when she is upright (flap)
    now sweeps it forward, and what sweeps it back when upright (sweep) now raises it. So the wings are held
    out with the tips a little behind the shoulders (negative flap) and the slow stroke goes on the sweep axis."""
    ph = 2 * math.pi * t / period
    beat = lambda x: math.sin(x + 0.55 * math.cos(x))
    k = wing_keys(1.0, -30 + 3 * math.cos(ph), 4 + amp * beat(ph))
    for side, sg in (("L", 1), ("R", -1)):   # the outer wing trails the stroke
        k[f"Wing{side}_2"] = rq((0, 0, 1), sg * 0.45 * amp * beat(ph - 0.8))
        k[f"Wing{side}_3"] = rq((0, 0, 1), sg * 0.45 * amp * beat(ph - 1.5))
    level = rq((1, 0, 0), 64 - TRAVEL_LEAN)   # the keys above are for the flat body (lean 64): tip the wings the rest of the way,
    for side in ("L", "R"):                   # so they stay level with the ground whatever the body's lean
        k[f"Wing{side}_1"] = level @ k[f"Wing{side}_1"]
    return k


n = 144; emit("FlightTravel", [(bob(TRAVEL, i / FPS, 2.4, .08, 1.5), soar_keys(i / FPS, 2.4, 24)) for i in range(n + 1)], True)   # two strokes: the slow sway spans both
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


# ---------------------------------------------------------------- the bow: drawn, loosed, drawn again
# The clips she came with have no archery. This one is posed here: her chest turned so that her left shoulder leads,
# her head still on the target, her left arm out with the bow upright, her right hand at her chin. Both arms are
# solved (two bones each) to where the hands must be.
BOW_TWIST = 35.0                         # degrees her chest turns to her right
BOW_SIDE = 0.035                         # m to her right of her chin the arrow lies
BOW_DROP = 0.02                          # m below her chin
BOW_ARM = 0.97                           # how straight her bow arm is (1 = fully)
BOW_WINGS = (-52.0, 34.0)                # degrees her wings are lowered (negative) and swept back while she shoots
BOW_WRIST = 0.07                         # m from a wrist to the middle of the fist
# frame by frame (30 a second): how far the string is drawn (0 = at rest on the bow, 1 = at her chin); m her string
# hand flies back past her jaw as it lets go; whether her hand is on the string; degrees the bow tips forward in her
# hand after the arrow leaves; degrees her chest gives back. The arrow leaves at frame 0; at the last she is drawn again.
BOW_KEYS = ((1.00, 0.00, 1, 0, 0), (1.00, 0.09, 0, 12, 4), (0.55, 0.04, 0, 8, 3), (0.00, 0.0, 1, 3, 1),
            (0.32, 0.0, 1, 0, 0), (0.74, 0.0, 1, 0, 0), (1.00, 0.0, 1, 0, 0))
BOW_POINTS = HERE.parents[2] / "game" / "src" / "BowPoints.h"   # written here for the game: where her hand and the bow are


def rig_pose(P):
    arm.animation_data.action = None
    for b in BODY_BONES:
        pb = arm.pose.bones[b]; pb.rotation_mode = 'QUATERNION'; pb.location = P[b][0]; pb.rotation_quaternion = P[b][1]
    bpy.context.view_layer.update()


def set_rot(bone, R):   # the bone's rotation in the armature's frame (3x3); its head stays where its parent puts it
    pb = arm.pose.bones[bone]; M = R.to_4x4(); M.translation = pb.matrix.translation
    pb.matrix = M; bpy.context.view_layer.update()


def turn(bone, q):
    set_rot(bone, q.to_matrix() @ arm.pose.bones[bone].matrix.to_3x3())


def aim(bone, to):
    cur = (arm.pose.bones[bone].matrix.to_3x3() @ Vector((0, 1, 0))).normalized()
    turn(bone, cur.rotation_difference(to.normalized()))


def reach(side, target, pole):
    """Two-bone solve: the wrist of this arm goes to `target`; the elbow bends toward `pole`."""
    up, fore, hand = side + "Arm", side + "ForeArm", side + "Hand"; rest = arm.data.bones
    a = (rest[fore].head_local - rest[up].head_local).length; b = (rest[hand].head_local - rest[fore].head_local).length
    S = arm.pose.bones[up].head.copy(); v = target - S; d = min(v.length, (a + b) * 0.985); n = v.normalized()
    x = (a * a - b * b + d * d) / (2 * d); h = math.sqrt(max(0.0, a * a - x * x))
    side_way = (pole - n * pole.dot(n)).normalized()
    aim(up, n * x + side_way * h)
    aim(fore, S + n * d - arm.pose.bones[fore].head)


def grip_rot(hand, fwd, up):
    """The hand's rotation that points it along `fwd` with what it grips (a stave across the fist) along `up`."""
    bn = arm.data.bones[hand]; d = (bn.tail_local - bn.head_local).normalized()
    f = Vector((0, -1, 0)); b = (f - d * f.dot(d)).normalized()
    up = (up - fwd * up.dot(fwd)).normalized()
    src = Matrix((d, b, d.cross(b))).transposed(); dst = Matrix((fwd, up, fwd.cross(up))).transposed()
    return dst @ src.inverted() @ REST[hand]


def posed(bone, point):   # a point given in her rest pose, where this bone has carried it
    return arm.pose.bones[bone].matrix @ arm.data.bones[bone].matrix_local.inverted() @ Vector(point)


def bow_frames():
    base = ground("Idle", 1, 51, loop=True)[0][0]; rig_pose(base)   # as she stands in Idle (her hips over the root, as there)
    head = arm.pose.bones["Head"].matrix.to_3x3()
    fwd, up, Z = Vector((0, -1, 0)), Vector((0, 0, 1)), (0, 0, 1)
    rest = arm.data.bones
    reach_l = BOW_ARM * ((rest["LeftForeArm"].head_local - rest["LeftArm"].head_local).length + (rest["LeftHand"].head_local - rest["LeftForeArm"].head_local).length)
    xf = bow_frame("LeftHand")
    ends = [xf((BOW_STRING[0] + BOW_STRING[1] * z, 0, z)) for z in (BOW_STRING_Z[1], BOW_STRING_Z[0])]   # the string's ends, top and bottom
    rest_string = BOW_LENGTH / 1.901 * (BOW_STRING[0] + BOW_STRING[1] * BOW_GRIP[2] - BOW_GRIP[0])       # m from the grip back to the string
    fist_l, fist_r = hand_frame("LeftHand")[2], hand_frame("RightHand")[2]
    out, rows = [], []
    for i, (draw, back, nocked, tip, give) in enumerate(BOW_KEYS):
        rig_pose(base)
        for b, k in (("Spine02", .30), ("Spine01", .35), ("Spine", .35)): turn(b, rq(Z, -(BOW_TWIST - give) * k))
        turn("neck", rq(Z, 0.4 * BOW_TWIST)); set_rot("Head", head)
        # the arrow's line: straight ahead, from just beside and below her chin
        anchor = arm.pose.bones["headfront"].head + Vector((-BOW_SIDE, 0, -BOW_DROP))
        # the bow hand: on that line, as far out as her arm goes
        S = arm.pose.bones["LeftArm"].head; v = anchor - S; along = v.dot(fwd)
        far = -along + math.sqrt(max(0.0, along * along - v.length_squared + reach_l * reach_l))   # |anchor + fwd * far - S| = reach
        grip = anchor + fwd * (far + BOW_WRIST)
        reach("Left", grip - fwd * BOW_WRIST, Vector((0.6, 0.2, -0.8)))
        lean = rq((1, 0, 0), tip).to_matrix() @ up                                # the top of the bow tips away from her
        set_rot("LeftHand", grip_rot("LeftHand", fwd, lean))
        # the string hand: at her chin when drawn, at the string when it takes it, flying back as it lets go
        travel = (grip - anchor).length - rest_string
        fingers = anchor + fwd * (travel * (1 - draw)) - fwd * back + Vector((-0.5 * back, 0, 0.25 * back))
        if not nocked: fingers = fingers + Vector((-0.03, 0, -0.035)) * math.sin(math.pi * min(1.0, draw + .2))   # it comes round, not straight back along the arrow
        reach("Right", fingers - fwd * BOW_WRIST, Vector((-0.5, 0.8, 0.25)))
        set_rot("RightHand", grip_rot("RightHand", fwd, up))
        P = {b: [arm.pose.bones[b].location.copy(), arm.pose.bones[b].rotation_quaternion.copy()] for b in BODY_BONES}
        P = mirror_pose(P)   # the game shows her mirrored (see BOW_GAME_HAND): posed here with the bow in this rig's left hand, then turned over
        out.append((P, wing_keys(1.0, flap=BOW_WINGS[0] + 3 * give, sweep=BOW_WINGS[1])))   # wings down and back: her arms must be seen from behind
        rows.append((posed("RightHand", fist_r), posed("LeftHand", fist_l), posed("LeftHand", ends[0]), posed("LeftHand", ends[1]), nocked))
    rig_pose(base)
    game = lambda v: "{%.4ff, %.4ff, %.4ff}" % (-v.x, v.z, -v.y)   # to the game's frame (x to her right, y up, z ahead) of the pose turned over: x changes sign twice, once for the mirror_pose above and once for the engine's mirror
    text = ["// Written by art/nyxara/blender/assemble.py (bow_frames). Do not edit: change the pose there and build her again.",
            "// Where her string hand, the bow's grip and the two ends of its string are in each frame of the BowShot clip",
            "// (30 frames a second), in her own frame: x to her right, y up, z ahead, metres from her feet.", "#pragma once", "",
            "namespace CaucasianEagle", "{",
            "    struct BowFrame { float Hand[3], Grip[3], Top[3], Bottom[3]; bool Nocked; };",
            "    constexpr int BowFrameCount = %d;" % len(rows), "    constexpr float BowFrameRate = %d.f;" % FPS,
            "    constexpr BowFrame BowFrames[BowFrameCount] = {"]
    text += ["        {%s, %s, %s, %s, %s}," % (game(h), game(g), game(t), game(b), "true" if n else "false") for h, g, t, b, n in rows]
    text += ["    };", "}", ""]
    BOW_POINTS.write_text("\n".join(text), encoding="utf-8")
    print("BOWPOINTS", BOW_POINTS, "travel", round(travel, 3), "grip", [round(c, 3) for c in grip], "anchor", [round(c, 3) for c in anchor])
    return out


emit("BowShot", bow_frames(), False)

# drop source actions; keep only emitted clips
keep = {c[0] for c in CLIPS}
for a in list(bpy.data.actions):
    if a.name not in keep: bpy.data.actions.remove(a)
arm.animation_data.action = bpy.data.actions["Idle"]; arm.animation_data.action_slot = bpy.data.actions["Idle"].slots[0]

# Doorways joint contract (DoorwaysCharacterPresentation): Root/Hips/Chest/UpperArm/LowerArm/Hand/UpperLeg names.
swords = [make_axe(AXE_INFERNO, AXE_LENGTH[0], "RightHand", "Slot_sword_right__Moonfeather"),
          make_axe(AXE_WAR, AXE_LENGTH[1], "LeftHand", "Slot_sword_left__Talonshade"),
          make_axe(AXE_INFERNO, AXE_LENGTH[0], "LeftHand", "Slot_sword_left__Moonfeather"),
          make_axe(AXE_WAR, AXE_LENGTH[1], "RightHand", "Slot_sword_right__Talonshade"),
          *make_bow(BOW_GAME_HAND, "Slot_bow_hand", "Slot_bow_string"),
          make_back(BOW, BOW_LENGTH, "Slot_bow_back", **BACK_BOW),
          make_back(AXE_INFERNO, AXE_LENGTH[0], "Slot_axe_back__Moonfeather", **BACK_AXES[0]),
          make_back(AXE_WAR, AXE_LENGTH[1], "Slot_axe_back__Talonshade", **BACK_AXES[1])]
body.data.materials[0].name = "NyxaraBody"

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
