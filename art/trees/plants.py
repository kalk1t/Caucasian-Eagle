"""The ground's plants as 3D models (0.5.2, 2026-11-21; the owner: "Fallen logs, tomatoes, cucumbers, ferns and flowers
are still built into the map - i want also them to be 3D"). Until then each was baked into the map as two or three crossed
pictures (a log as a six-sided tube). Now the game draws them as it draws the trees (AscentTrees), from these:

  log       a fallen trunk 12 m long (the game scales each to its own length): mossy bark on an uneven round, broken
            limbs, moss cushions on its top, bracket fungi on its sides, grass at its foot. Variants: 0 thrown by the
            wind (its root plate torn up with it, earth held in its roots; its top snapped off), 1 snapped and rotting
            (both ends torn, their hearts gone soft, the most moss and fungi), 2 felled and sawn (both ends cut, its limbs
            cut off short).
  tomato    on its hazel stake, tied to it: a zigzag stem, compound leaves, trusses of tomatoes (ripe low down, green
            higher up), its flowers at the top.
  cucumber  on the ground: vines running out from its foot, lobed leaves held up on their stalks (a mound), tendrils
            coiled, yellow flowers, cucumbers lying in the leaves.
  fern      a shuttlecock of fronds arching out from a scaly crown, fiddleheads uncurling in its middle.
  flowers   a clump of a meadow's flowers in grass: 0 oxeye daisies, 1 poppies, 2 bellflowers, 3 buttercups.

The same units, levels and files as treegen.py's trees: metres, y up, the foot at the origin (a log's middle); four
levels of detail (0 near .. 3 far); its wood and stems (the kind's bark picture) and everything cut out of the kind's
sheet (plant_textures.py: leaves, petals, moss, end grain, swatches) as two .dmesh. None has a far picture: a plant is
its 3D model until it is a few pixels, then it is not drawn (a 12 m log 500 m away is a line two pixels thick).
Everything random is drawn before any level is chosen, so the four levels are one plant.
"""
import math
import numpy as np

import treegen as tg
from treegen import Mesh, UP, unit, perp, rot, Branch, grow_line

X = np.array((1.0, 0.0, 0.0)); Z = np.array((0.0, 0.0, 1.0))

# a fallen log's measures, by its length (build_mountain.py holds her off it with the same numbers)
LOG_LEN = 12.0       # the model's length, m
LOG_R0 = 0.055       # its radius at the root end, by its length (a piece of one of the great trunks)
LOG_TAPER = 0.30     # its top end that much thinner
LOG_SINK = 0.72      # its axis this much of its radius above the ground: the rest of it has settled into the earth
LOG_PLATE = 2.6      # a windthrow's root plate: its radius by the trunk's at the root end

SPECIES = {          # lying: trees.txt's height is its length (the game scales a log to its length, not its height)
    "log": dict(lying=True, variants=3), "tomato": dict(variants=3), "cucumber": dict(variants=3),
    "fern": dict(variants=4), "flowers": dict(variants=4),
}

# ------------------------------------------------------------------------------------------------- building blocks
def ribbon(mesh, P, W, face, rect, fold=0.25, lift=0.35, flip=False):
    """A leaf, a frond, a blade, a petal: a strip along the points P, W wide at each (or one width), its upper face
    toward 'face' (a vector, or one for each point), folded down its middle (its sides rise by 'fold' of its half
    width), its picture (rect: u0 v0 u1 v1, v up) laid along it from its base (v0) to its tip (v1). Its normals lean
    toward the sky by 'lift' (lit as a plant, not as a mirror)."""
    P = np.asarray(P, float); n = len(P)
    W = np.broadcast_to(np.asarray(W, float), (n,)); F = np.broadcast_to(np.asarray(face, float), (n, 3))
    u0, v0, u1, v1 = rect
    if flip: u0, u1 = u1, u0
    seg = np.linalg.norm(np.diff(P, axis=0), axis=1); cum = np.concatenate([[0.0], np.cumsum(seg)]); tot = max(cum[-1], 1e-9)
    PP, NN, TT, UV = [], [], [], []
    for i in range(n):
        d = unit(P[min(i + 1, n - 1)] - P[max(i - 1, 0)])
        nrm = F[i] - d * np.dot(F[i], d)
        nrm = unit(nrm) if np.linalg.norm(nrm) > 1e-6 else perp(d)
        side = unit(np.cross(d, nrm)); hw = W[i] / 2; rise = nrm * hw * fold
        N_ = unit(nrm * (1 - lift) + UP * lift)
        v = v0 + (v1 - v0) * cum[i] / tot
        for pos, u in ((P[i] - side * hw + rise, u0), (P[i], (u0 + u1) / 2), (P[i] + side * hw + rise, u1)):
            PP.append(pos); NN.append(N_); TT.append(np.append(side, 1.0)); UV.append((u, v))
    I = []
    for i in range(n - 1):
        a, b = 3 * i, 3 * (i + 1)
        I += [a, b, a + 1, a + 1, b, b + 1, a + 1, b + 1, a + 2, a + 2, b + 1, b + 2]
    mesh.add(PP, NN, TT, UV, I)

def stalk(mesh, P, R, sides, uv=None, tile=0.3, closed=False):
    """A stem, a stake, a tendril: rings of 'sides' points along P (radius R at each, or one). uv: a swatch (one colour
    from the sheet), or None: the kind's bark picture, round it once, 'tile' m of it along it."""
    P = np.asarray(P, float); n = len(P); R = np.broadcast_to(np.asarray(R, float), (n,))
    a = perp(unit(P[1] - P[0])); PP, NN, TT, UV = [], [], [], []; run = 0.0
    for i in range(n):
        d = unit(P[(i + 1) % n] - P[i - 1]) if closed else unit(P[min(i + 1, n - 1)] - P[max(i - 1, 0)])
        a = unit(a - d * np.dot(a, d)); b = np.cross(d, a)
        if i: run += float(np.linalg.norm(P[i] - P[i - 1]))
        for j in range(sides + 1):
            an = 2 * math.pi * j / sides; radial = a * math.cos(an) + b * math.sin(an)
            PP.append(P[i] + radial * R[i]); NN.append(radial); TT.append(np.append(-a * math.sin(an) + b * math.cos(an), 1.0))
            UV.append(uv if uv is not None else (j / sides, run / tile))
    I = []
    for i in range(n - 1 + (1 if closed else 0)):
        i2 = (i + 1) % n
        for j in range(sides):
            q, q2 = i * (sides + 1) + j, i2 * (sides + 1) + j
            I += [q, q2, q + 1, q + 1, q2, q2 + 1]
    mesh.add(PP, NN, TT, UV, I)

def ellipsoid(mesh, c, ax, ay, az, seg, uv=None, rect=None):
    """A fruit, a bud: an ellipsoid round c (its half axes the vectors ax, ay, az; ay its long one, the poles on it).
    uv: one swatch; rect: a picture round it (u round, v from pole to pole)."""
    M = np.column_stack([ax, ay, az]); Minv = np.linalg.inv(M).T
    PP, NN, TT, UV = [], [], [], []
    rings = max(2, seg // 2 + 1)
    for i in range(rings + 1):
        th = math.pi * i / rings
        for j in range(seg + 1):
            ph = 2 * math.pi * j / seg; s = np.array((math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)))
            PP.append(c + M @ s); NN.append(unit(Minv @ s)); TT.append(np.append(unit(M @ np.array((-math.sin(ph), 0, math.cos(ph)))), 1.0))
            if rect is not None: UV.append((rect[0] + (rect[2] - rect[0]) * j / seg, rect[3] - (rect[3] - rect[1]) * i / rings))
            else: UV.append(uv)
    I = []
    for i in range(rings):
        for j in range(seg):
            q = i * (seg + 1) + j; I += [q, q + seg + 1, q + 1, q + 1, q + seg + 1, q + seg + 2]
    mesh.add(PP, NN, TT, UV, I)

def star(mesh, c, normal, rad, points, uv, inner=0.42, cup=0.25):
    """A small flower or a calyx: a star of 'points' points facing 'normal', its points rising by 'cup'."""
    n = unit(normal); a = perp(n); b = np.cross(n, a)
    PP, NN, TT, UV = [c + n * rad * 0.1], [n], [np.append(a, 1.0)], [uv]
    for k in range(2 * points):
        an = math.pi * k / points; rr = rad if k % 2 == 0 else rad * inner
        PP.append(c + (a * math.cos(an) + b * math.sin(an)) * rr + n * rr * cup); NN.append(unit(n + (a * math.cos(an) + b * math.sin(an)) * 0.2))
        TT.append(np.append(a, 1.0)); UV.append(uv)
    I = []
    for k in range(2 * points): I += [0, 1 + k, 1 + (k + 1) % (2 * points)]
    mesh.add(PP, NN, TT, UV, I)

def disc(mesh, c, normal, radius, rect, seg, dome=0.0, along=None):
    """A flat round (a flower's middle, an end's grain): a fan facing 'normal', the round middle of its picture on it,
    its middle raised by 'dome' (of its radius)."""
    n = unit(normal); a = unit(along - n * np.dot(along, n)) if along is not None else perp(n); b = np.cross(n, a)
    u0, v0, u1, v1 = rect
    PP, NN, TT, UV = [c + n * radius * dome], [n], [np.append(a, 1.0)], [((u0 + u1) / 2, (v0 + v1) / 2)]
    for k in range(seg + 1):
        an = 2 * math.pi * k / seg; ca, sa = math.cos(an), math.sin(an)
        PP.append(c + (a * ca + b * sa) * radius); NN.append(unit(n + (a * ca + b * sa) * dome))
        TT.append(np.append(a, 1.0)); UV.append((u0 + (0.5 + 0.5 * ca) * (u1 - u0), v0 + (0.5 + 0.5 * sa) * (v1 - v0)))
    I = []
    for k in range(seg): I += [0, 1 + k, 2 + k]
    mesh.add(PP, NN, TT, UV, I)

def arc(start, d, length, steps, droop, rng=None, wander=0.0, up=0.0):
    """Points from start along d, bending down toward its tip (droop) or up (up), wandering a little."""
    P = [np.asarray(start, float)]; d = unit(np.asarray(d, float)); step = length / steps
    for i in range(steps):
        t = (i + 1) / steps
        d = unit(d - UP * droop * t * 1.6 / steps + UP * up / steps + (rng.standard_normal(3) * wander / math.sqrt(steps) if rng is not None and wander else 0))
        P.append(P[-1] + d * step)
    return np.array(P)

def asp(R):
    return (R[2] - R[0]) / max(R[3] - R[1], 1e-6)

# ---------------------------------------------------------------------------------------------------------- the log
def log(seed, v, lod, R, sw):
    r = np.random.default_rng(seed); deco = np.random.default_rng(seed + 77)
    bark, leaves = Mesh(), Mesh()
    kind = v % 3; L = LOG_LEN; r0 = LOG_R0 * L
    # --- everything random first (the same log at every level)
    bow_z, bow_y = r.uniform(-0.10, 0.10), r.uniform(0.0, 0.05)        # (a little: the map's pieces that hold her run straight)
    lobes = [(m, a_, r.uniform(0, 6.283), r.uniform(0.15, 0.35)) for m, a_ in ((2, 0.055), (3, 0.035), (5, 0.022), (7, 0.012))]
    stubs = []
    for k in range(int(r.integers(4, 8)) if kind != 2 else int(r.integers(4, 7))):
        t = r.uniform(0.32, 0.94); phi = (1 if r.random() < 0.5 else -1) * r.uniform(0.35, 1.8); lean = math.radians(r.uniform(42, 70))
        length = (r.uniform(0.25, 0.9) if r.random() < 0.7 else r.uniform(0.9, 1.8)) if kind != 2 else r.uniform(0.05, 0.12)
        frac = r.uniform(0.14, 0.30); fork = r.random() < 0.35 and kind != 2 and length > 0.9
        stubs.append((t, phi, lean, length, frac, fork, int(r.integers(1 << 30))))
    stubs.sort(key=lambda s_: -s_[3] * s_[4])
    moss_ = [(r.uniform(0.05, 0.95), r.uniform(0.4, 1.4), r.uniform(-0.5, 0.5), r.uniform(0.5, 1.15), r.random() < 0.5) for _ in range({0: 9, 1: 15, 2: 4}[kind])]
    fungi = [(r.uniform(0.15, 0.85), (1 if r.random() < 0.5 else -1) * r.uniform(1.15, 1.75), int(r.integers(2, 5)), r.uniform(0.07, 0.15)) for _ in range({0: 2, 1: 4, 2: 0}[kind])]
    roots = [(2 * math.pi * (k + r.uniform(-0.3, 0.3)) / 9, r.uniform(0.9, 1.45), r.uniform(0.12, 0.2), int(r.integers(1 << 30))) for k in range(9)]
    plate_edge = r.uniform(0, 6.283, 4)
    spikes_top = (0.06 + 1.1 * r.random(64) ** 3, r.uniform(0.62, 0.95, 64)); spikes_root = (0.05 + 0.7 * r.random(64) ** 3, r.uniform(0.62, 0.95, 64))
    # --- the trunk: rings along x, each an uneven round, the bottom a little flattened where it lies on the earth
    step = {0: 0.30, 1: 0.6, 2: 1.4, 3: 4.0}[lod]; n = max(4, int(round(L / step)) + 1)
    sides = {0: 20, 1: 12, 2: 7, 3: 5}[lod]
    ts = np.linspace(0.0, 1.0, n); xs = -L / 2 + ts * L
    rad = r0 * (1 - LOG_TAPER * ts)
    if kind == 0: rad = rad * (1 + 0.45 * np.exp(-ts * L / (0.6 * r0)))        # a windthrown foot swells into its roots
    ys = LOG_SINK * rad + bow_y * np.sin(np.pi * ts); zs = bow_z * np.sin(np.pi * ts)
    rough = 1.0 if lod < 2 else 0.5
    def shape(phi, t):
        k = 1.0; dk = 0.0
        for m, a_, p_, w_ in lobes:
            ph_ = p_ + w_ * math.sin(t * 9.0 + m); k += rough * a_ * math.cos(m * phi + ph_); dk -= rough * a_ * m * math.sin(m * phi + ph_)
        cb = -math.cos(phi)                                                      # (phi from the top: cos -1 under it)
        if cb > 0: k *= 1 - 0.08 * cb * cb
        return k, dk
    def axis_at(t):
        """The log's middle and radius at t (0 its root end .. 1 its top end)."""
        rl = r0 * (1 - LOG_TAPER * t) * ((1 + 0.45 * math.exp(-t * L / (0.6 * r0))) if kind == 0 else 1.0)
        return np.array((-L / 2 + t * L, LOG_SINK * rl + bow_y * math.sin(math.pi * t), bow_z * math.sin(math.pi * t))), rl
    around = max(1, int(round(2 * math.pi * r0 / tg.BARK_TILE)))
    rings = []
    for i in range(n):
        c = np.array((xs[i], ys[i], zs[i])); ring = []
        for j in range(sides + 1):
            phi = 2 * math.pi * j / sides; k, dk = shape(phi, ts[i])
            radial = np.array((0.0, math.cos(phi), math.sin(phi))); tang = np.array((0.0, -math.sin(phi), math.cos(phi)))
            ring.append((c + radial * rad[i] * k, unit(radial - tang * dk / max(k, 1e-3) * 0.6), (around * j / sides, (xs[i] + L / 2) / tg.BARK_TILE)))
        rings.append(ring)
    def tube_rings(rs):
        PP, NN, TT, UV = [], [], [], []
        for ring in rs:
            for j, (p_, n_, uv_) in enumerate(ring):
                PP.append(p_); NN.append(n_); UV.append(uv_)
                an = 2 * math.pi * j / sides; TT.append((0.0, -math.sin(an), math.cos(an), 1.0))
        I = []
        for i in range(len(rs) - 1):
            for j in range(sides):
                q = i * (sides + 1) + j; I += [q, q + sides + 1, q + 1, q + 1, q + sides + 1, q + sides + 2]
        bark.add(PP, NN, TT, UV, I)
    # --- its ends: torn into splinters, or sawn flat; the grain or the rotten heart on them
    grain = R["rot"] if kind == 1 else R["grain"]
    def cap(ring, c, out, pic):
        """The end's face: a fan from its rim to its middle, the round middle of the grain picture on it."""
        u0, v0, u1, v1 = pic; rr = max(float(np.linalg.norm(ring[0][0] - c)), 1e-3) * 1.12
        PP, NN, TT, UV = [c + out * 0.02 * r0], [out], [(0.0, 0.0, 1.0, 1.0)], [((u0 + u1) / 2, (v0 + v1) / 2)]
        for p_, _, _ in ring:
            q = p_ - c; PP.append(p_); NN.append(out); TT.append((0.0, 0.0, 1.0, 1.0))
            UV.append((u0 + (0.5 + 0.5 * q[2] / rr) * (u1 - u0), v0 + (0.5 + 0.5 * q[1] / rr) * (v1 - v0)))
        I = []
        for j in range(len(ring) - 1): I += [0, 1 + j, 2 + j]
        leaves.add(PP, NN, TT, UV, I)
    def torn(i_end, sgn, spikes):
        """Splinters: a ring beyond the end, each point of it out along the log and in toward the heart."""
        c = np.array((xs[i_end], ys[i_end], zs[i_end])); last = rings[i_end]; s_, k_ = spikes; tip = []
        for j, (p_, n_, uv_) in enumerate(last):
            jj = j % sides
            s = s_[jj % 64] * rad[i_end]
            kk = k_[jj % 64] * (1.0 - 0.25 * min(s_[jj % 64], 1.0))          # (a long splinter narrows toward its point)
            tip.append((c + (p_ - c) * kk + X * sgn * s, n_, (uv_[0], uv_[1] + sgn * s / tg.BARK_TILE)))
        tube_rings([last, tip] if sgn > 0 else [tip, last])
        cap(tip, c - X * sgn * 0.35 * rad[i_end], X * sgn, R["rot"])
    tube_rings(rings)
    if kind == 2 or lod == 3:
        cap(rings[-1], np.array((xs[-1], ys[-1], zs[-1])), X, grain)
        if kind != 0: cap(rings[0], np.array((xs[0], ys[0], zs[0])), -X, grain)
    else:
        torn(n - 1, 1, spikes_top)
        if kind == 1: torn(0, -1, spikes_root)
    # --- a windthrow's root plate: earth held in the roots, standing up at its foot; the roots torn out of it
    if kind == 0:
        c = np.array((xs[0] - 0.06, ys[0], zs[0])); Rp = LOG_PLATE * r0; thick = 0.4 * r0; K = {0: 28, 1: 18, 2: 11, 3: 8}[lod]
        rim = []
        for j in range(K):
            a_ = 2 * math.pi * j / K
            rr = Rp * (1 + 0.10 * math.sin(2 * a_ + plate_edge[0]) + 0.06 * math.sin(3 * a_ + plate_edge[1]) + 0.04 * math.sin(5 * a_ + plate_edge[2]))
            rim.append(np.array((0.0, math.cos(a_) * rr, math.sin(a_) * rr)))
        u0, v0, u1, v1 = R["plate"]; sc_ = Rp * 1.12
        def plate_uv(q): return (u0 + (0.5 + 0.5 * q[2] / sc_) * (u1 - u0), v0 + (0.5 + 0.5 * q[1] / sc_) * (v1 - v0))
        for face_x, bulge, out in ((c[0], -0.35 * r0, -X), (c[0] + thick, 0.12 * r0, X)):
            mid = np.array((face_x + bulge, c[1], c[2]))
            PP, NN, TT, UV = [mid], [out], [(0.0, 0.0, 1.0, 1.0)], [plate_uv(np.zeros(3))]
            for q in rim + [rim[0]]:
                PP.append(np.array((face_x, c[1] + q[1], c[2] + q[2]))); NN.append(unit(out + q / Rp * 0.35)); TT.append((0.0, 0.0, 1.0, 1.0)); UV.append(plate_uv(q))
            I = []
            for j in range(K): I += [0, 1 + j, 2 + j]
            leaves.add(PP, NN, TT, UV, I)
        if lod < 2:                                                               # its torn edge, between the two faces
            PP, NN, TT, UV = [], [], [], []
            for q in rim + [rim[0]]:
                for dx in (0.0, thick):
                    PP.append(np.array((c[0] + dx, c[1] + q[1] * 0.97, c[2] + q[2] * 0.97))); NN.append(unit(q)); TT.append((1.0, 0.0, 0.0, 1.0))
                    UV.append(plate_uv(q * 0.93))
            I = []
            for j in range(K): q_ = 2 * j; I += [q_, q_ + 2, q_ + 1, q_ + 1, q_ + 2, q_ + 3]
            leaves.add(PP, NN, TT, UV, I)
        if lod < 2:
            for a_, ln, fr, sd in roots[: (9 if lod == 0 else 5)]:
                rr_ = np.random.default_rng(sd)
                d0 = unit(np.array((-0.25, math.cos(a_), math.sin(a_))))
                pts, rads = grow_line(rr_, c + d0 * rad[0] * 0.6 + X * 0.05, d0, Rp * ln, r0 * fr, 0.012, seg=0.25, droop=0.0, wander=0.25)
                pts = [p_ - X * 0.25 * (k / len(pts)) ** 2 * Rp for k, p_ in enumerate(pts)]   # torn back toward the pit
                tg.tube(bark, Branch(pts, rads, 1), max(3, tg.sides_for(r0 * fr, lod) - 1))
    # --- broken limbs (a felled log's: cut short)
    keep = {0: len(stubs), 1: 3, 2: 2, 3: 0}[lod]
    for t, phi, lean, length, frac, fork, sd in stubs[:keep]:
        rr_ = np.random.default_rng(sd); c, rl = axis_at(t)
        radial = np.array((0.0, math.cos(phi), math.sin(phi)))
        d = unit(radial * math.cos(lean) + X * math.sin(lean)); rb = rl * frac
        pts, rads = grow_line(rr_, c + radial * rl * 0.75, d, length + rl * 0.25, rb, rb * (0.5 if kind != 2 else 0.95), seg=0.18, droop=0.35 if length > 0.9 else 0.05, wander=0.1)
        tg.tube(bark, Branch(pts, rads, 1), max(3, tg.sides_for(rb, lod)))
        if kind == 2 and lod == 0:
            disc(leaves, pts[-1], unit(pts[-1] - pts[-2]), rads[-1] * 1.02, R["grain"], 8)
        if fork and lod == 0:
            p_, d_, r_ = Branch(pts, rads, 1).at(0.6)
            d2 = unit(rot(d_, perp(d_), 0.7))
            pp, rr2 = grow_line(rr_, p_, d2, length * 0.45, r_ * 0.6, r_ * 0.2, seg=0.15, wander=0.12)
            tg.tube(bark, Branch(pp, rr2, 2), 3)
    # --- moss cushions over its top, following its round
    keepm = {0: len(moss_), 1: max(3, len(moss_) // 2), 2: 3, 3: 0}[lod]
    for t0, ln, mid, half, which in moss_[:keepm]:
        t1 = min(t0 + ln / L, 1.0); pic = R["moss"] if which else R["moss2"]; u0, v0, u1, v1 = pic
        na, nl = (5, 3) if lod == 0 else (3, 2)
        PP, NN, TT, UV = [], [], [], []
        for a in range(nl):
            t = t0 + (t1 - t0) * a / (nl - 1); c, rl = axis_at(t)
            for b in range(na):
                phi = mid + half * (2 * b / (na - 1) - 1); k, _ = shape(phi, t)
                radial = np.array((0.0, math.cos(phi), math.sin(phi)))
                PP.append(c + radial * (rl * k + 0.012 * r0 / 0.54)); NN.append(unit(radial + UP * 0.3)); TT.append((1.0, 0.0, 0.0, 1.0))
                UV.append((u0 + (u1 - u0) * b / (na - 1), v0 + (v1 - v0) * a / (nl - 1)))
        I = []
        for a in range(nl - 1):
            for b in range(na - 1):
                q = a * na + b; I += [q, q + na, q + 1, q + 1, q + na, q + na + 1]
        leaves.add(PP, NN, TT, UV, I)
    # --- bracket fungi in tiers on its sides
    if lod < 3:
        for t, phi, count, size in fungi:
            c, rl = axis_at(t)
            radial = np.array((0.0, math.cos(phi), math.sin(phi)))
            for k in range(count if lod == 0 else 1):
                s = size * (1 - 0.18 * k) * (r0 / 0.54); base = c + radial * rl * 0.98 - UP * k * s * 0.55 + X * k * s * 0.3
                out = unit(np.array((0.0, 0.0, radial[2])) + UP * 0.05) if abs(radial[2]) > 0.2 else unit(radial)
                u0, v0, u1, v1 = R["fungus"]; seg = 9 if lod == 0 else 5
                PP, NN, TT, UV = [base], [unit(UP * 0.9 + out * 0.2)], [(1.0, 0.0, 0.0, 1.0)], [((u0 + u1) / 2, v0)]
                for j in range(seg + 1):
                    a_ = math.pi * j / seg
                    PP.append(base + X * math.cos(a_) * s * 0.8 + out * math.sin(a_) * s - UP * 0.12 * s * math.sin(a_))
                    NN.append(unit(UP * 0.9 + out * 0.2)); TT.append((1.0, 0.0, 0.0, 1.0))
                    UV.append((u0 + (0.5 + 0.5 * math.cos(a_)) * (u1 - u0), v0 + math.sin(a_) * (v1 - v0) * 0.97))
                I = []
                for j in range(seg): I += [0, 1 + j, 2 + j]
                leaves.add(PP, NN, TT, UV, I)
                if lod == 0:                                                      # its pale rim, a little thick
                    rimp = [base + X * math.cos(math.pi * j / seg) * s * 0.8 + out * math.sin(math.pi * j / seg) * s - UP * 0.12 * s * math.sin(math.pi * j / seg) for j in range(seg + 1)]
                    ribbon(leaves, rimp, s * 0.07, out, (sw[0][0], sw[0][1], sw[0][0], sw[0][1]), fold=0.0, lift=0.0)
    # --- grass at its foot, where it meets the earth (hides the line it cuts in the ground)
    if lod < 2:
        every = 0.55 if lod == 0 else 1.3; u0, v0, u1, v1 = R["blade"]
        for side in (-1, 1):
            x = -L / 2 + deco.uniform(0.1, 0.5)
            while x < L / 2 - 0.2:
                t = (x + L / 2) / L; c, rl = axis_at(t)
                base = np.array((x, 0.0, c[2] + side * rl * 0.72))
                for b in range(3 if lod == 0 else 2):
                    a_ = deco.uniform(-0.6, 0.6); ln = deco.uniform(0.16, 0.36) * (r0 / 0.54)
                    d = unit(np.array((math.sin(a_) * 0.5, 1.0, side * deco.uniform(0.2, 0.7))))
                    P = arc(base + np.array((deco.uniform(-0.12, 0.12), -0.03, side * deco.uniform(0.0, 0.08))), d, ln, 3, deco.uniform(0.2, 0.6))
                    ribbon(leaves, P, (0.016 if lod == 0 else 0.026) * (r0 / 0.54), np.cross(d, X) * side + UP * 0.2, R["blade"], fold=0.15, lift=0.5, flip=deco.random() < 0.5)
                x += every * deco.uniform(0.6, 1.4)
    return bark, leaves, {"size": L}

# ------------------------------------------------------------------------------------------------------- the tomato
def tomato(seed, v, lod, R, sw):
    """sw: 0 ripe red, 1 orange, 2 green fruit, 3 stem green, 4 flower yellow, 5 twine, 6 calyx green"""
    r = np.random.default_rng(seed); bark, leaves = Mesh(), Mesh()
    H = 1.55; lx, lz = r.uniform(-0.03, 0.03), r.uniform(-0.03, 0.03)
    Hp = r.uniform(1.25, 1.42); phs = r.uniform(0, 6.283)
    nodes = []; y = 0.08
    while y < Hp - 0.05:
        nodes.append(y); y += r.uniform(0.10, 0.14)
    zig = [r.uniform(0.008, 0.016) * (1 if k % 2 else -1) for k in range(len(nodes))]
    leafs = [(phs + k * 2.52 + r.uniform(-0.3, 0.3), r.uniform(0.9, 1.1), r.uniform(20, 42) if k > 1 else r.uniform(5, 20), r.uniform(0.7, 1.3), r.random() < 0.5, int(r.integers(1 << 30)))
             for k in range(len(nodes))]
    shoots = [(k, r.uniform(0, 6.283), r.uniform(0.25, 0.45), int(r.integers(1 << 30))) for k in (2, 5) if k < len(nodes)]
    trusses = []
    for k in range(3, len(nodes), 3):
        m = int(r.integers(3, 6)); tr = len(trusses)
        fr = [(r.uniform(0.85, 1.15), r.random()) for _ in range(m)]
        trusses.append((k, m, fr, r.uniform(-0.4, 0.4), r.uniform(0.09, 0.14)))
    # the stake: a hazel pole, a little out of true, driven 0.35 m into the earth
    sides = {0: 7, 1: 5, 2: 4, 3: 3}[lod]
    stake = [np.array((0.0, -0.35, 0.0)), np.array((lx * 0.5, 0.6, lz * 0.5)), np.array((lx, H, lz))]
    stalk(bark, stake, [0.022, 0.020, 0.018], sides, tile=1.2)
    disc(bark, stake[-1], UP, 0.018, (0.45, 0.45, 0.55, 0.55), sides)               # its cut top (the bark picture's middle)
    off = np.array((math.cos(phs + 1.6), 0.0, math.sin(phs + 1.6))) * 0.035
    def stake_at(y_): return np.array((lx * y_ / H, y_, lz * y_ / H))
    def stem_at(k): return stake_at(nodes[k]) + off + np.array((math.cos(phs) * zig[k], 0.0, math.sin(phs) * zig[k]))
    stem = [stake_at(-0.02) + off] + [stem_at(k) for k in range(len(nodes))] + [stake_at(Hp) + off * 0.8]
    if lod < 3: stalk(leaves, stem, np.linspace(0.011, 0.004, len(stem)), {0: 5, 1: 4, 2: 3}[lod], uv=sw[3])
    # its leaves: compound, out and arching down, the lowest yellowing
    seg = {0: 6, 1: 4, 2: 3, 3: 2}[lod]
    for k, (az, sz, el, droop, flip, sd) in enumerate(leafs):
        if lod >= 2 and k % 2 == 1: continue
        y_ = nodes[k]; Ll = 0.46 * (1.10 - 0.45 * y_ / Hp) * sz * (1.3 if lod >= 2 else 1.0)
        d = np.array((math.cos(az) * math.cos(math.radians(el)), math.sin(math.radians(el)), math.sin(az) * math.cos(math.radians(el))))
        P = arc(stem_at(k), d, Ll, seg, 1.1 * droop)
        pic = R["leafC"] if k == 0 else (R["leafA"] if k % 2 == 0 else R["leafB"])
        ribbon(leaves, P, Ll * asp(pic) * 1.25, UP, pic, fold=0.28, lift=0.4, flip=flip)
    # side shoots left on it: a short stem out and up, two or three leaves of their own
    if lod < 2:
        for k, az, ln, sd in shoots:
            rr_ = np.random.default_rng(sd); d = unit(np.array((math.cos(az), 1.4, math.sin(az))))
            S = arc(stem_at(k), d, ln, 3, -0.1); stalk(leaves, S, np.linspace(0.007, 0.004, len(S)), 3, uv=sw[3])
            for j in range(1, len(S)):
                a2 = az + (1.3 if j % 2 else -1.3) + rr_.uniform(-0.3, 0.3); el = math.radians(rr_.uniform(15, 35)); L2 = 0.30 * rr_.uniform(0.85, 1.1)
                d2 = np.array((math.cos(a2) * math.cos(el), math.sin(el), math.sin(a2) * math.cos(el)))
                pic = R["leafA"] if j % 2 else R["leafB"]
                ribbon(leaves, arc(S[j], d2, L2, 3 if lod == 0 else 2, 0.9), L2 * asp(pic) * 1.25, UP, pic, fold=0.28, lift=0.4, flip=rr_.random() < 0.5)
    # trusses: ripe low down, green higher up; flowers on the top one
    for ti, (k, m, fr, tw, ln) in enumerate(trusses):
        if lod == 3 and ti > 1: break
        az = leafs[k][0] + math.pi + tw; out = np.array((math.cos(az), 0.0, math.sin(az)))
        p0 = stem_at(k); p1 = p0 + out * 0.05 - UP * 0.01; p2 = p1 + out * ln * 0.6 - UP * ln * 0.5
        if lod < 2: stalk(leaves, [p0, p1, p2], [0.004, 0.003, 0.0025], 3, uv=sw[3])
        top = ti == len(trusses) - 1 and len(trusses) > 2
        for j, (s_, c_) in enumerate(fr):
            f = (j + 0.5) / m; q = p1 + (p2 - p1) * f + np.array((math.cos(az + 1.6), 0, math.sin(az + 1.6))) * 0.04 * (1 if j % 2 else -1)
            if top:
                if lod < 2: star(leaves, q + UP * 0.005, unit(-UP * 0.3 + out), 0.012, 5, sw[4], inner=0.35, cup=-0.35)
                continue
            rf = (0.038 - 0.004 * ti) * s_ * (1.3 if lod == 3 else 1.0); cq = q - UP * (rf + 0.012)
            ripe = 0 if ti == 0 else (0 if c_ < 0.6 else 1) if ti == 1 else (1 if c_ < 0.3 else 2) if ti == 2 else 2
            if lod == 3 and ripe == 2: continue
            ellipsoid(leaves, cq, X * rf, UP * rf * 0.84, Z * rf, {0: 10, 1: 7, 2: 5, 3: 4}[lod], uv=sw[ripe])
            if lod == 0: star(leaves, cq + UP * rf * 0.86, UP, rf * 0.55, 5, sw[6], inner=0.3, cup=-0.15)
    # the twine that ties it to its stake
    if lod == 0:
        for y_ in (0.30, 0.62, 0.95, 1.25):
            if y_ > Hp: break
            c = stake_at(y_) + off * 0.5; a_ = unit(off); b_ = np.cross(UP, a_)
            loop = [c + a_ * math.cos(2 * math.pi * q / 10) * 0.032 + b_ * math.sin(2 * math.pi * q / 10) * 0.024 + UP * 0.004 * math.sin(4 * math.pi * q / 10) for q in range(10)]
            stalk(leaves, loop, 0.0028, 3, uv=sw[5], closed=True)
    return bark, leaves, {}

# ----------------------------------------------------------------------------------------------------- the cucumber
def cucumber(seed, v, lod, R, sw):
    """sw: 0 flower yellow, 1 stem green"""
    r = np.random.default_rng(seed); bark, leaves = Mesh(), Mesh()
    nv = 4 + v % 2 + int(r.integers(0, 2)); vines = []
    for i in range(nv):
        az = 2 * math.pi * (i + r.uniform(-0.3, 0.3)) / nv; Lv = r.uniform(0.7, 1.15); P = [np.array((0.0, 0.03, 0.0))]; d = az
        for k in range(int(Lv / 0.05)):
            d += r.uniform(-0.22, 0.22); q = P[-1] + np.array((math.cos(d), 0.0, math.sin(d))) * 0.05
            q[1] = 0.025 + 0.012 * math.sin(k * 0.9 + i); P.append(q)
        nodes = []
        for k in range(2, len(P) - 1, 2):
            nodes.append((k, 1 if (k // 2) % 2 else -1, r.uniform(0.7, 1.15), r.uniform(0.1, 0.5), r.uniform(-0.3, 0.3), int(r.integers(0, 3)), r.random(), r.random(), r.random() < 0.5))
        fruit = [(int(r.integers(3, max(4, len(P) - 2))), (1 if r.random() < 0.5 else -1) * r.uniform(0.3, 0.7), r.uniform(0.14, 0.22), r.uniform(0.018, 0.024), r.uniform(-0.012, 0.012)) for _ in range(int(r.integers(0, 2)) + (1 if i % 2 == 0 else 0))]
        vines.append((np.array(P), nodes, fruit, int(r.integers(1 << 30))))
    pics = [R["leafA"], R["leafB"], R["leafC"]]
    for P, nodes, fruit, sd in vines:
        rr_ = np.random.default_rng(sd)
        Pv = P if lod < 2 else np.vstack([P[:: (2 if lod == 2 else 5)], P[-1:]])  # (far off its vine is a few straight runs)
        stalk(bark, Pv, np.linspace(0.0042, 0.0024, len(Pv)), {0: 5, 1: 4, 2: 3, 3: 3}[lod], tile=0.25)
        for j, (k, side, sz, tilt, twist, which, tend, flw, flip) in enumerate(nodes):
            if lod == 3 and j % 3: continue
            if lod == 2 and j % 2: continue
            p = P[k]; d = unit(P[k + 1] - P[k - 1]); out = unit(np.array((p[0], 0.0, p[2]))) if np.hypot(p[0], p[2]) > 0.05 else d
            sidev = np.cross(UP, d) * side
            frac = k / len(P); lp = (0.07 + 0.16 * (1 - frac)) * sz
            pd = unit(UP * 0.75 + out * 0.55 + sidev * 0.3); top = p + pd * lp
            if lod < 3: stalk(bark, [p, p + pd * lp * 0.5 + UP * 0.01, top], [0.0026, 0.0022, 0.0018], 3, tile=0.25)
            s = 0.20 * sz * (1.2 if lod >= 2 else 1.0) * (1.25 if lod == 3 else 1.0); pic = pics[which]
            bd = unit(out * math.cos(twist) + sidev * math.sin(twist) + UP * (0.15 + tilt * 0.6))
            P2 = arc(top, bd, s, 3 if lod < 2 else 2, -0.15)
            ribbon(leaves, P2, s * asp(pic), unit(UP + out * 0.25), pic, fold=0.32, lift=0.35, flip=flip)
            if lod == 0 and tend < 0.35:                                         # a tendril, coiled
                ax_ = unit(out * 0.5 + sidev * 0.5 + UP * 0.6); a_ = perp(ax_); b_ = np.cross(ax_, a_)
                coil = [p + ax_ * 0.09 * q / 24 + (a_ * math.cos(q * 0.65) + b_ * math.sin(q * 0.65)) * 0.009 * (0.4 + 0.6 * q / 24) for q in range(25)]
                stalk(bark, coil, 0.0013, 3, tile=0.25)
            if lod < 2 and flw < 0.3:                                            # a flower on a short stalk
                f = p + UP * 0.05 + sidev * 0.03
                if lod == 0: stalk(bark, [p, f], 0.0018, 3, tile=0.25)
                star(leaves, f, unit(UP + sidev * 0.4), 0.022, 5, sw[0], inner=0.45, cup=0.2)
        for k, ang, ln, rf, bend in fruit:                                       # cucumbers lying among the leaves
            if lod == 3: continue
            p = P[min(k, len(P) - 2)]; d = unit(P[min(k + 1, len(P) - 1)] - P[k - 1]); dd = unit(rot(d, UP, ang))
            m = {0: 9, 1: 6, 2: 4}[lod]; ns = {0: 10, 1: 6, 2: 4}[lod]
            g = np.array((p[0], 0.0, p[2]))
            spine = [g + dd * (0.03 + ln * q / (m - 1)) + UP * rf * 0.9 + np.cross(UP, dd) * bend * math.sin(math.pi * q / (m - 1)) for q in range(m)]
            prof = [rf * max(math.sin(math.pi * (0.04 + 0.92 * q / (m - 1))) ** 0.45, 0.25) for q in range(m)]
            u0, v0, u1, v1 = R["skin"]
            PP, NN, TT, UV = [], [], [], []
            a_ = perp(dd)
            for q in range(m):
                t = unit(spine[min(q + 1, m - 1)] - spine[max(q - 1, 0)]); a_ = unit(a_ - t * np.dot(a_, t)); b_ = np.cross(t, a_)
                for j in range(ns + 1):
                    an = 2 * math.pi * j / ns; radial = a_ * math.cos(an) + b_ * math.sin(an)
                    PP.append(spine[q] + radial * prof[q]); NN.append(radial); TT.append(np.append(t, 1.0))
                    UV.append((u0 + (u1 - u0) * j / ns, v0 + (v1 - v0) * q / (m - 1)))
            I = []
            for q in range(m - 1):
                for j in range(ns):
                    a0 = q * (ns + 1) + j; I += [a0, a0 + ns + 1, a0 + 1, a0 + 1, a0 + ns + 1, a0 + ns + 2]
            leaves.add(PP, NN, TT, UV, I)
            if lod == 0: stalk(bark, [p, p + UP * 0.02, spine[0]], 0.0022, 3, tile=0.25)
    return bark, leaves, {}

# --------------------------------------------------------------------------------------------------------- the fern
def fern(seed, v, lod, R, sw):
    """sw: 0 a fiddlehead's green, 1 scales' brown"""
    r = np.random.default_rng(seed); bark, leaves = Mesh(), Mesh()
    Lf = 1.4 * r.uniform(0.92, 1.08); nf = (9, 12, 7, 10)[v % 4]
    fronds = []
    for i in range(nf):
        az = 2 * math.pi * (i + r.uniform(-0.25, 0.25)) / nf
        young = i % 4 == 0
        el = math.radians(r.uniform(58, 74) if young else r.uniform(30, 52))
        fronds.append((az, el, Lf * (0.75 if young else 1.0) * r.uniform(0.85, 1.12), r.uniform(0.55, 0.95) * (0.6 if young else 1.0),
                       ("frondA", "frondB", "frondC")[int(r.integers(0, 3))], r.random() < 0.5, r.uniform(-0.25, 0.25)))
    heads = [(r.uniform(0, 6.283), r.uniform(0.08, 0.20), r.uniform(0.9, 1.5)) for _ in range(int(r.integers(2, 4)))]
    # its crown: a stump of old stipes' bases, scaly
    if lod < 3: stalk(bark, [np.array((0.0, -0.06, 0.0)), np.array((0.0, 0.03, 0.0)), np.array((0.0, 0.07, 0.0))], [0.05, 0.045, 0.03], {0: 9, 1: 6, 2: 4}[lod], tile=0.15)
    steps = {0: 14, 1: 8, 2: 5, 3: 3}[lod]
    order = sorted(range(nf), key=lambda q: fronds[q][2], reverse=True)
    keep = set(order[: {0: nf, 1: nf, 2: max(5, nf * 2 // 3), 3: 5}[lod]])
    for i, (az, el, ln, droop, pic, flip, twist) in enumerate(fronds):
        if i not in keep: continue
        d = np.array((math.cos(az) * math.cos(el), math.sin(el), math.sin(az) * math.cos(el)))
        P = arc(np.array((math.cos(az) * 0.025, 0.06, math.sin(az) * 0.025)), d, ln, steps, droop)
        cut = max(1, int(round(steps * 0.2)))                                     # the stipe: bare, from the crown
        if lod < 3: stalk(bark, P[: cut + 1], np.linspace(0.0065, 0.0045, cut + 1), {0: 4, 1: 3, 2: 3}[lod], tile=0.2)
        lam = P[cut:] if lod < 3 else P
        side = np.cross(unit(lam[-1] - lam[0]), UP); face = unit(UP + side * twist)
        w = (len(lam) and float(np.linalg.norm(np.diff(lam, axis=0), axis=1).sum())) * 0.40
        ribbon(leaves, lam, w, face, R[pic], fold=0.38, lift=0.45, flip=flip)
    # fiddleheads: new fronds uncurling in its middle
    if lod == 0:
        for az, h, turns in heads:
            out = np.array((math.cos(az), 0.0, math.sin(az))); base = out * 0.02 + UP * 0.05; top = base + UP * h + out * 0.02
            pts = [base, base + UP * h * 0.5 + out * 0.01, top]; R0 = 0.026; C = top + out * R0
            for q in range(1, 17):
                a_ = q / 16 * turns * 2 * math.pi; rr = R0 * (1 - 0.8 * q / 16)
                pts.append(C + (-out * math.cos(a_) + UP * math.sin(a_)) * rr)
            stalk(bark, pts, np.linspace(0.006, 0.002, len(pts)), 4, tile=0.2)
    return bark, leaves, {}

# ------------------------------------------------------------------------------------------------------ the flowers
def flowers(seed, v, lod, R, sw):
    """v: 0 oxeye daisies, 1 poppies, 2 bellflowers, 3 buttercups. sw: 0 stem green, 1 sepal, 2 bud, 3 black, 4 violet"""
    r = np.random.default_rng(seed); bark, leaves = Mesh(), Mesh()
    sp = ("daisy", "poppy", "bell", "butter")[v % 4]
    Hf = {"daisy": 0.72, "poppy": 0.62, "bell": 0.70, "butter": 0.58}[sp]
    leafpic = R[{"daisy": "leaf_daisy", "poppy": "leaf_poppy", "bell": "leaf_bell", "butter": "leaf_butter"}[sp]]
    blades = [(r.uniform(0, 6.283), r.uniform(0.0, 0.07), r.uniform(0.18, 0.46), math.radians(r.uniform(50, 84)), r.uniform(0.4, 1.0), r.random() < 0.5, r.random() < 0.5) for _ in range(34)]
    basal = [(r.uniform(0, 6.283), r.uniform(0.10, 0.20), math.radians(r.uniform(12, 35)), r.random() < 0.5) for _ in range(6)]
    nst = {"daisy": int(r.integers(7, 11)), "poppy": int(r.integers(4, 7)), "bell": int(r.integers(4, 7)), "butter": int(r.integers(6, 10))}[sp]
    stems = []
    for k in range(nst):
        az = r.uniform(0, 6.283); h = Hf * r.uniform(0.55, 1.0); lean = math.radians(r.uniform(3, 18))
        stems.append((az, h, lean, r.uniform(0.0, 0.05), r.uniform(-0.6, 0.6), int(r.integers(1 << 30))))
    stems.sort(key=lambda s_: -s_[1])
    # the grass it grows in, and its own leaves
    nb = {0: 34, 1: 18, 2: 9, 3: 5}[lod]; wide = {0: 0.014, 1: 0.020, 2: 0.030, 3: 0.040}[lod]
    for i, (az, o, ln, el, droop, which, flip) in enumerate(blades[:nb]):
        d = np.array((math.cos(az) * math.cos(el), math.sin(el), math.sin(az) * math.cos(el)))
        pic = R["blade"] if which else R["blade2"]
        P = arc(np.array((math.cos(az) * o, -0.02, math.sin(az) * o)), d, ln * (1.15 if lod >= 2 else 1.0), {0: 4, 1: 3, 2: 2, 3: 2}[lod], droop)
        ribbon(leaves, P, wide * (0.8 + 0.4 * ln / 0.46), unit(np.cross(d, np.cross(UP, d)) + UP * 0.1), pic, fold=0.15, lift=0.5, flip=flip)
    for az, ln, el, flip in basal[: {0: 6, 1: 4, 2: 2, 3: 0}[lod]]:
        d = np.array((math.cos(az) * math.cos(el), math.sin(el), math.sin(az) * math.cos(el)))
        P = arc(np.array((0.0, 0.0, 0.0)), d, ln, 3 if lod == 0 else 2, 0.25)
        ribbon(leaves, P, ln * asp(leafpic) * 1.05, UP, leafpic, fold=0.2, lift=0.4, flip=flip)
    # the stems and their heads
    keep = {0: nst, 1: nst, 2: max(3, nst // 2), 3: 3}[lod]
    for az, h, lean, o, bend, sd in stems[:keep]:
        rr_ = np.random.default_rng(sd)
        d = np.array((math.cos(az) * math.sin(lean), math.cos(lean), math.sin(az) * math.sin(lean)))
        ns = {0: 6, 1: 4, 2: 3, 3: 2}[lod]
        P = arc(np.array((math.cos(az) * o, -0.01, math.sin(az) * o)), d, h, ns, -0.15 + 0.3 * bend)
        stalk(bark, P, np.linspace(0.0032, 0.0022, len(P)), {0: 4, 1: 3, 2: 3, 3: 3}[lod], tile=0.25)
        tip = P[-1]; tdir = unit(P[-1] - P[-2])
        if lod == 0 and sp != "poppy":                                           # a leaf on the stem
            q = P[len(P) // 3]; ld = unit(np.array((math.cos(az + 2.0), 0.6, math.sin(az + 2.0))))
            ribbon(leaves, arc(q, ld, 0.07, 2, 0.3), 0.07 * asp(leafpic) * 0.9, UP, leafpic, fold=0.2, lift=0.4)
        if sp == "daisy":
            face = unit(tdir + UP * 0.8 + np.array((math.cos(az), 0, math.sin(az))) * 0.4)
            disc(leaves, tip + face * 0.003, face, 0.0105, R["disc_daisy"], {0: 10, 1: 6, 2: 5, 3: 4}[lod], dome=0.35)
            npt = {0: 21, 1: 13, 2: 9, 3: 7}[lod]; a_ = perp(face); b_ = np.cross(face, a_)
            for k in range(npt):
                an = 2 * math.pi * k / npt + rr_.uniform(-0.08, 0.08); rd = a_ * math.cos(an) + b_ * math.sin(an)
                pd = unit(rd - face * rr_.uniform(0.0, 0.25)); ln = rr_.uniform(0.020, 0.026)
                ribbon(leaves, [tip + rd * 0.009, tip + rd * 0.009 + pd * ln * 0.5, tip + rd * 0.009 + pd * ln], ln * asp(R["ray"]) * 1.0, face, R["ray"], fold=0.1, lift=0.25)
        elif sp == "poppy":
            face = unit(tdir * 0.6 + UP); a_ = perp(face); b_ = np.cross(face, a_); rot0 = rr_.uniform(0, 6.283)
            for k in range(4):
                an = rot0 + math.pi / 2 * k + rr_.uniform(-0.15, 0.15); rd = a_ * math.cos(an) + b_ * math.sin(an)
                cup = math.radians(rr_.uniform(38, 58)) * (0.75 if k % 2 else 1.0); ln = rr_.uniform(0.036, 0.046)
                pd = unit(rd * math.cos(cup) + face * math.sin(cup))
                P2 = [tip + face * 0.004 * (k % 2), tip + face * 0.004 * (k % 2) + pd * ln * 0.5 + face * ln * 0.05, tip + face * 0.004 * (k % 2) + pd * ln]
                ribbon(leaves, P2, ln * asp(R["poppy"]), unit(face - rd * 0.6), R["poppy"], fold=-0.25 if lod == 0 else 0.0, lift=0.2)
            disc(leaves, tip + face * 0.008, face, 0.009, R["disc_poppy"], {0: 8, 1: 5, 2: 4, 3: 4}[lod], dome=0.5)
            if lod == 0:
                ellipsoid(leaves, tip + face * 0.013, a_ * 0.0055, face * 0.006, b_ * 0.0055, 6, uv=sw[1])
        elif sp == "bell":
            nbell = 3 + int(rr_.integers(0, 3))
            for k in range(nbell if lod < 2 else 1):
                q = P[-1] if k == 0 else P[max(1, len(P) - 1 - k)] + (P[-1] - P[-2]) * 0.0
                hang = unit(np.array((math.cos(az + k * 2.1), 0.0, math.sin(az + k * 2.1))) * 0.7 - UP * 0.7)
                base = q + hang * 0.01; ln = rr_.uniform(0.026, 0.034); ns_b = {0: 10, 1: 6, 2: 5, 3: 4}[lod]
                a_ = perp(hang); b_ = np.cross(hang, a_); u0, v0, u1, v1 = R["bell"]
                PP, NN, TT, UV = [], [], [], []
                for i in range(4):
                    t = i / 3; rad = 0.004 + 0.011 * t ** 0.8 + (0.003 * (t - 0.66) / 0.34 if t > 0.66 else 0.0)
                    for j in range(ns_b + 1):
                        an = 2 * math.pi * j / ns_b; rd = a_ * math.cos(an) + b_ * math.sin(an)
                        PP.append(base + hang * ln * t + rd * rad); NN.append(unit(rd - hang * 0.3)); TT.append(np.append(hang, 1.0))
                        UV.append((u0 + (u1 - u0) * j / ns_b, v0 + (v1 - v0) * t))
                I = []
                for i in range(3):
                    for j in range(ns_b):
                        a0 = i * (ns_b + 1) + j; I += [a0, a0 + ns_b + 1, a0 + 1, a0 + 1, a0 + ns_b + 1, a0 + ns_b + 2]
                leaves.add(PP, NN, TT, UV, I)
                if lod == 0: star(leaves, base - hang * 0.002, -hang, 0.007, 5, sw[1], inner=0.3, cup=0.4)
        else:                                                                    # buttercups: forked stems, glossy cups
            heads = [(tip, tdir)]
            if lod < 2:
                for k in range(2):
                    q = P[max(1, len(P) - 2)]; bd = unit(tdir + np.array((math.cos(az + 2.4 * k + 1), 0.2, math.sin(az + 2.4 * k + 1))) * 0.8)
                    br = arc(q, bd, h * 0.22, 2, -0.2); stalk(bark, br, 0.0018, 3, tile=0.25); heads.append((br[-1], bd))
            for hp, hd in heads:
                face = unit(hd + UP * 0.9); a_ = perp(face); b_ = np.cross(face, a_)
                for k in range(5):
                    an = 2 * math.pi * k / 5 + rr_.uniform(-0.1, 0.1); rd = a_ * math.cos(an) + b_ * math.sin(an)
                    pd = unit(rd * math.cos(0.55) + face * math.sin(0.55)); ln = rr_.uniform(0.011, 0.014)
                    ribbon(leaves, [hp, hp + pd * ln * 0.5, hp + pd * ln], ln * asp(R["butter"]) * 1.1, unit(face - rd * 0.5), R["butter"], fold=-0.2 if lod == 0 else 0.0, lift=0.25)
                disc(leaves, hp + face * 0.003, face, 0.0045, R["disc_butter"], 6, dome=0.6)
    return bark, leaves, {}

BUILDERS = {"log": log, "tomato": tomato, "cucumber": cucumber, "fern": fern, "flowers": flowers}

def build(species, seed, v, lod, rects, swatches):
    """(bark mesh, leaves mesh, info) of one level of detail; info["size"]: what trees.txt lists as its height when
    that is not its top (a log's length)."""
    return BUILDERS[species](seed, v, lod, rects, [tuple(s) for s in swatches])
