"""Nyxara's looks for the Character Lab (the owner, 2026-11-21: three examples to choose between; "each upgrade of Wings
changes her wings, each upgrade of armor upgrades and changes her armor and whole set she wears and upgrading swords
replaces her swords with different swords - make sure the other swords ... will fit nyxara's animations").

Her mesh as build_from_mixamo.py left it (the .blend it saves), with every look's swords and armour pieces added as parts
of their own (materials Look_...), which the game hides until a look shows them (game/src/NyxaraLooks.cpp). Her clips,
her bones and her own parts are untouched: the export is build_from_mixamo.py's own.

    blender --factory-startup -b art/nyxara/blender/Nyxara_mixamo.blend --python art/nyxara/looks/assemble.py -- <out.glb> [<pictures folder>]

The models are Hyper3D (Rodin) ones, fetched into rodin/<name>/ by rodin_fetch.py (GENERATIONS.tsv says which).

Swords. Her fingers are closed on her own swords' handles in every clip, so another sword must be held the same way: its
grip (found by eye on each model's profile, sword_profile.py) is laid where her own sword's handle lies in each fist, along
the same line, its edge (a sabre's: the side away from where its point curves; a straight blade's: its flat's width) the
way her own edge faces, and it is made as long from the hand to the point as hers (0.775 m: the sword trails and the hits
are measured there). Its grip is then made as thick as hers where her fingers close, a long hilt squeezed towards the
guard and a short blade drawn out where a model asks for it. Each sword is carried, like hers, wholly by SwordLeft or
SwordRight: on her back, thrown and caught they go where hers go.

Pauldrons are laid over her own (her mesh has layered plates on each shoulder), outside every point of her shoulder and
upper arm, and carried as the nearest points of her own shoulder are (weights copied, then smoothed). Crowns sit round her
head at the line of her brow, as wide as her head and hair are there, carried by her Head bone.
"""
import bpy, bmesh, sys, os, math
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

HERE = os.path.dirname(os.path.abspath(__file__))
ROD = os.path.join(HERE, "rodin")
args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
PICS = args[1] if len(args) > 1 else None

# name: where its pommel is in the model (top or bottom), then as parts of its length from the pommel's end: the grip,
# where the blade begins (past the guard), how much the hilt is squeezed towards the guard, how much the blade is drawn
# out, and whether the blade has a light of its own (a part Look_<name>_Blade)
SWORDS = {
    "S11": dict(pommel="top", grip=(0.06, 0.235), blade=0.28),
    "S12": dict(pommel="top", grip=(0.09, 0.245), blade=0.30),
    "S13": dict(pommel="top", grip=(0.10, 0.235), blade=0.28),
    "S21b": dict(pommel="bottom", grip=(0.075, 0.255), blade=0.30),
    "S22b": dict(pommel="top", grip=(0.065, 0.245), blade=0.33),
    "S23": dict(pommel="bottom", grip=(0.07, 0.375), blade=0.59, squeeze=0.5, draw=1.3, lit=True),
    "S31b": dict(pommel="bottom", grip=(0.115, 0.43), blade=0.48, squeeze=0.55, draw=1.2),
    "S32": dict(pommel="top", grip=(0.07, 0.235), blade=0.37, lit=True),
    "S33": dict(pommel="bottom", grip=(0.08, 0.20), blade=0.24, lit=True),
}
SWORD_FACES = 10000
# pauldrons: the model's way out of the shoulder and down the arm
PAULDRONS = {
    "P1": dict(out=(0, -1, 0), down=(0, 0, -1)),
    "P2": dict(out=(0, -1, 0), down=(1, 0, 0)),
    "P3": dict(out=(0, -1, 0), down=(1, 0, 0), allow=0.03),   # (a strap block inside it may sink into her own plate)
}
PIECE_FACES = 12000
# crowns: a ring lying level in the model, its front at -Y; 'band' is the height of its band's middle, as a part of its height
CROWNS = {"C1": dict(band=0.5), "C2": dict(band=0.5), "C3": dict(band=0.07)}

if len(args) > 2:   # only some (a trial): names after the pictures folder
    keep = set(args[2:])
    SWORDS = {k: v for k, v in SWORDS.items() if k in keep}
    PAULDRONS = {k: v for k, v in PAULDRONS.items() if k in keep}
    CROWNS = {k: v for k, v in CROWNS.items() if k in keep}

arm = bpy.data.objects["Nyxara_rig"]
body = bpy.data.objects["Nyxara"]
UV = body.data.uv_layers[0].name
arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.matrix_basis = Matrix.Identity(4)
bpy.context.view_layer.update()
REST = {b.name: (np.array(b.head_local), np.array(b.tail_local)) for b in arm.data.bones}


def points(o):
    w = np.empty(len(o.data.vertices) * 3, "f4")
    o.data.vertices.foreach_get("co", w)
    return w.reshape(-1, 3).astype("f8")


def set_points(o, w):
    o.data.vertices.foreach_set("co", w.astype("f4").ravel())
    o.data.update()


def unit(v):
    v = np.asarray(v, "f8")
    return v / np.linalg.norm(v)


# ---- her body: points, which material each is on, the weights of each
B = points(body)
G = {g.name: g.index for g in body.vertex_groups}
GN = {i: n for n, i in G.items()}
nv = len(B)
vmat = np.full(nv, -1)
for p in body.data.polygons:
    for i in p.vertices:
        vmat[i] = p.material_index
MAT = {m.name: i for i, m in enumerate(body.data.materials)}
weights = [dict() for _ in range(nv)]
for v in body.data.vertices:
    weights[v.index] = {GN[g.group]: g.weight for g in v.groups if g.weight > 0}


def weight(i, names):
    return sum(weights[i].get(n, 0.0) for n in names)


# ---- her own swords: where each handle lies in each fist (build_from_mixamo.py's sword_frame, on her mesh's points)
HANDLE = (0.096, 0.197)


def own_frame(w):
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
    tip = w[t > 0.97 * length].mean(0)
    back = (tip - gc) - ga * ((tip - gc) @ ga)
    edge = unit(-back)
    return dict(gc=gc, ga=unit(ga), r=radius, edge=edge, tip=tip, reach=float((tip - gc) @ ga), length=length)


OWN = {}
for side in ("Left", "Right"):
    idx = [i for i in range(nv) if vmat[i] == MAT["Sword"] and weights[i].get("Sword" + side, 0) > 0.5]
    OWN[side] = own_frame(B[idx])
    f = OWN[side]
    print("OWN %s handle at %s radius %.4f reach %.3f length %.3f" % (side, np.round(f["gc"], 3), f["r"], f["reach"], f["length"]))


def import_model(name, faces):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROD, name, "base_basic_pbr.glb"))
    new = [o for o in bpy.data.objects if o not in before]
    o = [x for x in new if x.type == "MESH"][0]
    mw = o.matrix_world.copy()
    o.parent = None
    o.matrix_world = mw
    for x in new:
        if x is not o:
            bpy.data.objects.remove(x)
    for x in bpy.context.view_layer.objects:
        x.select_set(x is o)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(o.data.polygons) > faces:
        d = o.modifiers.new("d", "DECIMATE")
        d.ratio = faces / len(o.data.polygons)
        bpy.ops.object.modifier_apply(modifier="d")
    me = o.data
    while len(me.uv_layers) > 1:
        me.uv_layers.remove(me.uv_layers[-1])
    me.uv_layers[0].name = UV
    for a in list(me.color_attributes):
        me.color_attributes.remove(a)
    if me.has_custom_normals:
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    o.name = "Look_" + name
    mat = me.materials[0]
    mat.name = "Look_" + name
    return o


def shade(o):
    for x in bpy.context.view_layer.objects:
        x.select_set(x is o)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))


def carried_by(o, groups):
    """o's points carried by those bones: {name: weight} for all, or a list of such per point."""
    o.vertex_groups.clear()
    if isinstance(groups, dict):
        for n, wt in groups.items():
            o.vertex_groups.new(name=n).add(list(range(len(o.data.vertices))), wt, "REPLACE")
        return
    made = {}
    for i, gw in enumerate(groups):
        for n, wt in gw.items():
            if n not in made:
                made[n] = o.vertex_groups.new(name=n)
            made[n].add([i], wt, "REPLACE")


def frame(a, e):
    """The right-handed frame with a first and e (made square to it) second."""
    a = unit(a)
    e = unit(e - a * (e @ a))
    return np.stack([a, e, np.cross(a, e)], 1)


LOOKS = []   # every part made: (object, kind, name, side)

# ---- the swords
for name, s in SWORDS.items():
    o = import_model(name, SWORD_FACES)
    w = points(o)
    c = w.mean(0)
    ax = np.linalg.eigh(np.cov((w - c).T))[1][:, -1]
    if (ax[2] > 0) == (s["pommel"] == "top"):
        ax = -ax                                   # from the pommel to the point
    t = (w - c) @ ax
    t -= t.min()
    L = t.max()
    f = t / L
    g0, g1 = s["grip"]
    b0 = s["blade"]
    sq, dr = s.get("squeeze", 1.0), s.get("draw", 1.0)
    f2 = f.copy()
    m = f <= g1
    f2[m] = g1 - (g1 - f[m]) * sq
    m = f > b0
    f2[m] = b0 + (f[m] - b0) * dr
    w = w + np.outer((f2 - f) * L, ax)
    g0 = g1 - (g1 - g0) * sq
    grip = w[(f2 >= g0) & (f2 <= g1)]
    gc = grip.mean(0)
    ga = unit(np.linalg.eigh(np.cov((grip - gc).T))[1][:, -1])
    if ga @ ax < 0:
        ga = -ga
    off = (grip - gc) - np.outer((grip - gc) @ ga, ga)
    r = float(np.percentile(np.linalg.norm(off, axis=1), 75))
    tip = w[f2 > 0.97 * f2.max()].mean(0)
    reach = float((tip - gc) @ ga)
    back = (tip - gc) - ga * ((tip - gc) @ ga)
    if np.linalg.norm(back) > 0.04 * reach:
        edge, kind = unit(-back), "curved"
    else:
        bl = w[f2 > b0 + 0.1] - gc
        bl = bl - np.outer(bl @ ga, ga)
        edge, kind = unit(np.linalg.eigh(np.cov(bl.T))[1][:, -1]), "straight"
    set_points(o, w)
    me = o.data
    if s.get("lit"):   # its blade a part of its own, to be lit
        mb = me.materials[0].copy()
        mb.name = "Look_%s_Blade" % name
        me.materials.append(mb)
        cut = b0 + 0.01
        for p in me.polygons:
            if min(f2[i] for i in p.vertices) > cut:
                p.material_index = 1
    shade(o)
    pair = []
    for side in ("Left", "Right"):
        x = o if side == "Left" else o.copy()
        if side == "Right":
            x.data = o.data.copy()
            bpy.context.scene.collection.objects.link(x)
            x.name = "Look_%s_R" % name
        F = OWN[side]
        k = F["reach"] / reach
        R = frame(F["ga"], F["edge"]) @ frame(ga, edge).T
        p = ((w - gc) * k) @ R.T + F["gc"]
        # the grip as thick as hers where her fingers close; the guard and the pommel as they are
        q = p - F["gc"]
        along = q @ F["ga"]
        radial = q - np.outer(along, F["ga"])
        dist = np.linalg.norm(radial, axis=1)
        ratio = float(np.clip(F["r"] / (r * k), 0.45, 1.5))
        fade = 0.015 / (L * k)
        wa = np.clip(1 - np.maximum(g0 - f2, f2 - g1) / fade, 0, 1)
        wa[(f2 >= g0) & (f2 <= g1)] = 1
        wr = np.clip((2.0 * r * k - dist) / (0.7 * r * k), 0, 1)
        sc = 1 + (ratio - 1) * wa * wr
        p = F["gc"] + np.outer(along, F["ga"]) + radial * sc[:, None]
        set_points(x, p)
        carried_by(x, {"Sword" + side: 1.0})
        pair.append(x)
        LOOKS.append((x, "sword", name, side))
    print("SWORD %s %s: %.3f m long, grip %.0f%% of its length, %.3f m long, %.4f m thick made %.4f (hers %.4f)" %
          (name, kind, L * k * f2.max() / f.max(), 100 * (g1 - g0), (g1 - g0) * L * k, r * k, r * k * ratio, F["r"]))


# ---- the pauldrons
def region(side, low=-0.03, high=0.16):
    """Her shoulder and upper arm on that side: the points carried mostly by its shoulder and arm bones."""
    a, b = REST[side + "Arm"]
    d = unit(b - a)
    out = []
    for i in range(nv):
        if vmat[i] != MAT["Nyxara"]:
            continue
        if weight(i, (side + "Arm", side + "Shoulder")) < 0.5:
            continue
        s_ = (B[i] - a) @ d
        if low <= s_ <= high:
            out.append(i)
    return np.array(out), a, d


def hits_inward(bvh, p, a, d):
    """Points of hers that have the piece between them and the line of her arm: the piece goes through them there."""
    n = 0
    for v in p:
        s_ = (v - a) @ d
        foot = a + d * s_
        to = foot - v
        dist = np.linalg.norm(to)
        if dist < 1e-4:
            continue
        hit = bvh.ray_cast(Vector(v), Vector(to / dist), dist)
        if hit[0] is not None:
            n += 1
    return n


for name, s in PAULDRONS.items():
    o = import_model(name, PIECE_FACES)
    w0 = points(o)
    c0 = (w0.min(0) + w0.max(0)) / 2
    side = "Left"
    idx, a, d_arm = region(side)
    down_t = unit(0.75 * d_arm + 0.25 * np.array([0, 0, -1.0]))
    out_t = unit(np.array([1.0, 0, 0]) - down_t * down_t[0])
    side_t = np.cross(down_t, out_t)
    om, dm = unit(s["out"]), unit(s["down"])
    sm = np.cross(dm, om)
    R = np.stack([out_t, down_t, side_t], 1) @ np.stack([om, dm, sm], 1).T
    reg = B[idx]
    across = np.ptp(reg @ side_t)
    w = (w0 - c0) @ R.T
    k = min(1.2 * across / np.ptp(w @ side_t), 0.22 / np.ptp(w @ down_t))   # (no longer than 22 cm down her arm)
    w = w * k
    centre = a + d_arm * 0.05
    bvh_faces = [list(p.vertices) for p in o.data.polygons]
    best = None
    for step in range(60):
        push = 0.004 * step
        p = w + centre + out_t * push
        bvh = BVHTree.FromPolygons([Vector(v) for v in p], bvh_faces)
        n = hits_inward(bvh, reg, a, d_arm)
        if n <= max(3, int(len(reg) * s.get("allow", 0.0025))):
            best = (push, p, n)
            break
    if best is None:
        best = (push, p, n)
    push, p, n = best
    p = p + out_t * 0.006
    set_points(o, p)
    shade(o)
    # carried as the nearest points of her own shoulder are, smoothed over the piece
    near = [i for i in range(nv) if vmat[i] == MAT["Nyxara"] and weight(i, (side + "Arm", side + "Shoulder", "Spine2", "Neck")) > 0.3
            and np.linalg.norm(B[i] - centre) < 0.35]
    kd = KDTree(len(near))
    for j, i in enumerate(near):
        kd.insert(Vector(B[i]), j)
    kd.balance()
    names = sorted({n_ for i in near for n_ in weights[i]})
    col = {n_: j for j, n_ in enumerate(names)}
    W = np.zeros((len(p), len(names)))
    for vi, v in enumerate(p):
        for (_, j, dist) in kd.find_n(Vector(v), 6):
            wt = 1.0 / (dist + 0.005)
            for n_, x in weights[near[j]].items():
                W[vi, col[n_]] += wt * x
    W /= W.sum(1, keepdims=True)
    edges = np.array([e.vertices[:] for e in o.data.edges])
    for _ in range(25):
        acc = np.zeros_like(W)
        cnt = np.zeros(len(W))
        np.add.at(acc, edges[:, 0], W[edges[:, 1]]); np.add.at(acc, edges[:, 1], W[edges[:, 0]])
        np.add.at(cnt, edges[:, 0], 1); np.add.at(cnt, edges[:, 1], 1)
        W = 0.5 * W + 0.5 * acc / np.maximum(cnt, 1)[:, None]
    per = []
    for row in W:
        top = np.argsort(row)[::-1][:4]
        tw = row[top]
        tw = tw / tw.sum()
        per.append({names[j]: float(x) for j, x in zip(top, tw) if x > 0.01})
    carried_by(o, per)
    LOOKS.append((o, "pauldron", name, side))
    print("PAULDRON %s: %.3f m across, pushed out %.3f m, %d of %d of her points still through it" % (name, np.ptp(p @ side_t), push, n, len(reg)))
    # the right one: the left one mirrored, then pushed out of her right shoulder if it goes in
    x = o.copy()
    x.data = o.data.copy()
    bpy.context.scene.collection.objects.link(x)
    x.name = "Look_%s_R" % name
    pr = p * np.array([-1.0, 1, 1])
    bm = bmesh.new(); bm.from_mesh(x.data)
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    bm.to_mesh(x.data); bm.free()
    idx_r, a_r, d_r = region("Right")
    out_r = out_t * np.array([-1.0, 1, 1])
    faces_r = [list(q.vertices) for q in x.data.polygons]
    for step in range(30):
        q = pr + out_r * 0.004 * step
        n = hits_inward(BVHTree.FromPolygons([Vector(v) for v in q], faces_r), B[idx_r], a_r, d_r)
        if n <= max(3, int(len(idx_r) * s.get("allow", 0.0025))):
            break
    set_points(x, q)
    flip = {}
    for vg in o.vertex_groups:
        flip[vg.name] = vg.name.replace("Left", "Right")
    per_r = [{flip.get(n_, n_): wt for n_, wt in gw.items()} for gw in per]
    carried_by(x, per_r)
    shade(x)
    LOOKS.append((x, "pauldron", name, "Right"))
    print("PAULDRON %s right: pushed out a further %.3f m, %d of %d through" % (name, 0.004 * step, n, len(idx_r)))

# ---- the crowns
head_idx = np.array([i for i in range(nv) if vmat[i] == MAT["Nyxara"] and weights[i].get("Head", 0) > 0.6])
H = B[head_idx]
top = REST["HeadTop_End"][0][2]
face = H[(H[:, 2] > top - 0.2) & (H[:, 2] < top - 0.06) & (np.abs(H[:, 0]) < 0.012)]   # (her middle line: not the white lock
                                                                                       # that falls in front of her face)
nose = face[np.argmin(face[:, 1])]
brow = nose[2] + 0.11           # the band's middle on her forehead, 11 cm over the tip of her nose
TILT = math.radians(10)         # the band lower in front than behind
print("HEAD top %.3f nose %s brow %.3f" % (top, np.round(nose, 3), brow))
for name, s in CROWNS.items():
    o = import_model(name, PIECE_FACES)
    w0 = points(o)
    lo, hi = w0.min(0), w0.max(0)
    height = hi[2] - lo[2]
    zb = lo[2] + s["band"] * height
    band = w0[np.abs(w0[:, 2] - zb) < 0.04 * height + 0.01]
    half = (band[:, 0].max() - band[:, 0].min()) / 2
    band = band[band[:, 1] > band[:, 1].max() - 1.6 * half]          # (not the jewel in front)
    A_ = np.c_[band[:, 0], band[:, 1], np.ones(len(band))]
    sol = np.linalg.lstsq(A_, -(band[:, 0] ** 2 + band[:, 1] ** 2), rcond=None)[0]
    ctr = np.array([-sol[0] / 2, -sol[1] / 2, zb])
    rel = w0 - ctr
    rb = np.hypot(band[:, 0] - ctr[0], band[:, 1] - ctr[1])
    r_in = float(np.percentile(rb, 4))
    # her head where the band goes round it: the plane through her brow, tilted
    n_ = np.array([0, -math.sin(TILT), math.cos(TILT)])
    y0 = H[:, 1].mean()
    p0 = np.array([0.0, y0, brow + (y0 - nose[1]) * math.tan(TILT)])
    near = H[np.abs((H - p0) @ n_) < 0.008]
    u_ = np.array([1.0, 0, 0]); v_ = np.cross(n_, u_)
    hx, hy = (near - p0) @ u_, (near - p0) @ v_
    # the front of her forehead on her middle line; what lies in front of it (the white lock falling over her face)
    # the band passes behind
    front = hy[np.abs(hx) < 0.015].min()
    keep = hy >= front - 0.004
    hx, hy = hx[keep], hy[keep]
    cx, cy = (hx.max() + hx.min()) / 2, (hy.max() + hy.min()) / 2
    # her head's outline round the band, angle by angle from its middle: the band follows it, 5 mm off
    NB = 72
    ang = np.arctan2(hy - cy, hx - cx)
    rad = np.hypot(hx - cx, hy - cy)
    bins = ((ang + np.pi) / (2 * np.pi) * NB).astype(int) % NB
    R = np.array([np.percentile(rad[bins == b], 98) if (bins == b).sum() > 2 else np.nan for b in range(NB)])
    good = ~np.isnan(R)
    R = np.interp(np.arange(NB), np.arange(NB)[good], R[good], period=NB)
    R = np.maximum.reduce([np.roll(R, -1), R, np.roll(R, 1)]) + 0.005
    phi = np.arctan2(rel[:, 1], rel[:, 0])
    out = np.interp((phi + np.pi) / (2 * np.pi) * NB - 0.5, np.arange(NB), R, period=NB)
    grow = out / r_in
    local = np.c_[rel[:, 0] * grow, rel[:, 1] * grow, rel[:, 2] * grow.mean()]
    a_, b_ = R.max(), R.min()
    centre = p0 + u_ * cx + v_ * cy
    Rt = np.stack([u_, v_, n_], 1)                 # the crown's x, y, z to her across, back, up
    p = local @ Rt.T + centre
    set_points(o, p)
    shade(o)
    carried_by(o, {"Head": 1.0})
    LOOKS.append((o, "crown", name, "Head"))
    print("CROWN %s: round her head (%.3f x %.3f m there) 5 mm off, %.3f to %.3f m from its middle; band at %.3f m" %
          (name, np.ptp(hx), np.ptp(hy), b_, a_, centre[2]))

# ---- pictures: each sword in her hands and on her back, each example at its last tier, before it all becomes one mesh
if PICS:
    os.makedirs(PICS, exist_ok=True)
    for o, kind, name, side in LOOKS:
        mod = o.modifiers.new("arm", "ARMATURE")
        mod.object = arm
        o.parent = arm
        o.matrix_parent_inverse = arm.matrix_world.inverted()
    own_sword = body.data.materials[MAT["Sword"]]
    hide = bpy.data.materials.new("hidden")
    hide.use_nodes = True
    nt = hide.node_tree
    for n_ in list(nt.nodes):
        nt.nodes.remove(n_)
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    ou = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(tr.outputs[0], ou.inputs[0])
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    for o in list(sc.objects):
        if o.type in ("LIGHT", "CAMERA"):
            bpy.data.objects.remove(o)
    wo = bpy.data.worlds.new("w"); sc.world = wo; wo.use_nodes = True
    wo.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.62, 0.66, 1)
    wo.node_tree.nodes["Background"].inputs[1].default_value = 1.2
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN")); sc.collection.objects.link(sun)
    sun.data.energy = 3; sun.rotation_euler = (math.radians(45), 0, math.radians(-30))
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam)
    cam.data.type = "ORTHO"; sc.camera = cam
    tracks = {t_.name: t_ for t_ in arm.animation_data.nla_tracks}
    muted = {n_: t_.mute for n_, t_ in tracks.items()}

    def show(names):
        for o, kind, name, side in LOOKS:
            o.hide_render = name not in names
        body.data.materials[MAT["Sword"]] = hide if any(n_ in SWORDS for n_ in names) else own_sword

    def pose(clip, frame_):
        act = tracks[clip].strips[0].action
        for t_ in tracks.values():
            t_.mute = True
        arm.animation_data.action = act
        try:
            arm.animation_data.action_slot = act.slots[0]
        except Exception:
            pass
        sc.frame_set(int(act.frame_range[0]) + frame_)
        bpy.context.view_layer.update()

    def shoot(path, d, c, size, res=(700, 900)):
        sc.render.resolution_x, sc.render.resolution_y = res
        d = Vector(d).normalized()
        cam.data.ortho_scale = size
        cam.location = Vector(c) - d * 5
        cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)

    def at(bone):
        return tuple(arm.matrix_world @ arm.pose.bones[bone].head)

    for name in SWORDS:
        show({name})
        pose("Idle", 0)
        shoot(os.path.join(PICS, "sword_%s_idle.png" % name), (0, 1, 0), (0, 0, 1.25), 1.7, (700, 700))
        shoot(os.path.join(PICS, "sword_%s_grip.png" % name), (-0.5, 1, 0.1), at("LeftHand"), 0.32, (500, 500))
        pose("Walk", 0)
        shoot(os.path.join(PICS, "sword_%s_back.png" % name), (0, -1, 0), (0, 0, 1.25), 1.7, (700, 700))
        if "Attack_L2" in tracks:
            pose("Attack_L2", 14)
            shoot(os.path.join(PICS, "sword_%s_attack.png" % name), (0.4, 1, 0), (0, 0, 1.2), 2.2, (700, 700))
    for e in "123":
        names = {"S%s3" % e, "P" + e, "C" + e} & {n_ for _, _, n_, _ in LOOKS}
        if not names:
            continue
        show(names)
        pose("Idle", 0)
        shoot(os.path.join(PICS, "ex%s_front.png" % e), (0, 1, 0), (0, 0, 1.0), 2.6)
        shoot(os.path.join(PICS, "ex%s_side.png" % e), (-1, 0, 0), (0, 0, 1.0), 2.6)
        shoot(os.path.join(PICS, "ex%s_back.png" % e), (0, -1, 0), (0, 0, 1.0), 2.6)
        shoot(os.path.join(PICS, "ex%s_head.png" % e), (0.5, 1, -0.15), at("Head"), 0.55, (600, 600))
        shoot(os.path.join(PICS, "ex%s_shoulder.png" % e), (-0.8, 1, -0.3), at("LeftArm"), 0.6, (600, 600))
        pose("Walk", 0)
        shoot(os.path.join(PICS, "ex%s_walk_back.png" % e), (0, -1, 0), (0, 0, 1.0), 2.6)
        if "Attack_L2" in tracks:
            pose("Attack_L2", 14)
            shoot(os.path.join(PICS, "ex%s_attack.png" % e), (0.4, 1, 0), (0, 0, 1.0), 2.6)
    body.data.materials[MAT["Sword"]] = own_sword
    arm.animation_data.action = None
    for n_, t_ in tracks.items():
        t_.mute = muted[n_]
    for pb in arm.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    for o, kind, name, side in LOOKS:
        o.modifiers.remove(o.modifiers["arm"])
        o.parent = None
        o.hide_render = False
    bpy.context.view_layer.update()
    print("PICTURES", PICS)

# ---- one mesh, exported as build_from_mixamo.py exports her
for x in bpy.context.view_layer.objects:
    x.select_set(x is body or any(x is o for o, *_ in LOOKS))
bpy.context.view_layer.objects.active = body
bpy.ops.object.join()
print("MATERIALS", [m.name for m in body.data.materials])
print("FACES", len(body.data.polygons), "POINTS", len(body.data.vertices))
for x in bpy.context.view_layer.objects:
    x.select_set(x in (arm, body))
bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLB", use_selection=True,
                          export_animation_mode="NLA_TRACKS", export_skins=True, export_influence_nb=4,
                          export_all_influences=False, export_def_bones=False, export_force_sampling=True,
                          export_frame_step=1, export_yup=True, export_image_format="AUTO",
                          export_materials="EXPORT")
print("EXPORTED", OUT)
