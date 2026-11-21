"""Real 3D trees for the Eagle's Ascent (0.5.2, 2026-11-20; owner: "improve trees, create its real 3D models, refine
them, their branches and leaves, their roots").

A tree is grown, not drawn: a skeleton of branches (the trunk, its limbs, their branches, their twigs), each a line of
points with a radius, grown step by step (bending toward the light or under its weight, wandering a little), the
children set on their parents in whorls (firs, spruces) or a spiral (the leaf trees) at the angles and lengths their kind
grows them, the crown's outline coming from the lengths (a cone, a dome, a flame). Then:
  - bark:    every branch a tube, as many sides as its width asks, its bark laid on it at true size (a 1.2 m square of
             bark picture to the metre); the trunk's foot flares and swells into its roots, and 4 to 9 roots run out
             over the ground and down into it;
  - leaves:  sprays of leaves (a twig with its leaves: textures.py) on the twigs, folded down their middle, facing up
             and out; a conifer's flat boughs of needles along its branches; a pine's tufts at its shoots' ends.
             Lit as the crown (their normals point out of the whole crown, as the old forest's did).
Three levels of detail from one skeleton: 0 everything (near), 1 without the twigs' wood and with fewer, larger sprays,
2 the trunk and its limbs and big clusters (far). Model units: metres, y up, the trunk's foot at the origin.

Writes the game's .dmesh files (DMSH v1: the engine's DoorwaysMesh.h), one for the bark and one for the leaves of every
level: build_trees.py.
"""
import math, struct
import numpy as np

UP = np.array((0.0, 1.0, 0.0))

def unit(v):
    n = np.linalg.norm(v); return v / n if n > 1e-9 else np.array((1.0, 0.0, 0.0))

def perp(d):
    a = np.cross(d, UP if abs(d[1]) < 0.95 else np.array((1.0, 0.0, 0.0))); return unit(a)

def rot(v, axis, ang):
    """v turned about axis by ang (Rodrigues)."""
    axis = unit(axis); c, s = math.cos(ang), math.sin(ang)
    return v * c + np.cross(axis, v) * s + axis * np.dot(axis, v) * (1 - c)

SHAPES = {   # length of a limb by where it stands on the crown (t: 0 its foot .. 1 the top)
    "conical": lambda t: 0.15 + 0.85 * (1 - t),
    "spherical": lambda t: 0.25 + 0.75 * math.sin(math.pi * min(max(t, 0), 1)),
    "hemispherical": lambda t: 0.25 + 0.75 * math.sin(0.5 * math.pi * (1 - min(max(t, 0), 1)) + 0.5 * math.pi * 0) * (1 - 0.6 * t),
    "flame": lambda t: (t / 0.7 if t < 0.7 else (1 - t) / 0.3) * 0.85 + 0.15,
    "tend_flame": lambda t: 0.5 + 0.5 * (t / 0.7 if t < 0.7 else (1 - t) / 0.3),
    "cylindrical": lambda t: 1.0,
    "umbrella": lambda t: 0.3 + 0.7 * min(max(t, 0), 1) ** 0.6,
}

class Branch:
    __slots__ = ("pts", "rad", "level", "length", "kids", "tparent", "dead")
    def __init__(self, pts, rad, level, tparent=0.0):
        self.pts = np.asarray(pts, float); self.rad = np.asarray(rad, float); self.level = level
        seg = np.linalg.norm(np.diff(self.pts, axis=0), axis=1); self.length = float(seg.sum())
        self.kids = []; self.tparent = tparent; self.dead = False
    def at(self, t):
        """point, direction and radius at t (0..1 of its length)"""
        seg = np.linalg.norm(np.diff(self.pts, axis=0), axis=1); cum = np.concatenate([[0], np.cumsum(seg)])
        s = t * cum[-1]; i = int(np.clip(np.searchsorted(cum, s) - 1, 0, len(seg) - 1))
        f = (s - cum[i]) / max(seg[i], 1e-9)
        p = self.pts[i] * (1 - f) + self.pts[i + 1] * f; r = self.rad[i] * (1 - f) + self.rad[i + 1] * f
        return p, unit(self.pts[i + 1] - self.pts[i]), r

def grow_line(rng, start, d, length, r0, r1, seg=0.5, up=0.0, droop=0.0, wander=0.08, tipup=0.0, taper=1.0, kink=0.0):
    """A branch's points: from start along d, bending up (up > 0) or down (droop > 0, more toward the tip, as weight),
    wandering a little at each step; its tip turning up (tipup) as a fir's and spruce's boughs do."""
    n = max(3, int(math.ceil(length / seg))); step = length / n
    pts = [np.asarray(start, float)]; rads = [r0]; d = unit(np.asarray(d, float))
    for i in range(n):
        t = (i + 1) / n
        d = d + UP * (up * step / max(length, 1e-6) * 2.0) - UP * (droop * t * step / max(length, 1e-6) * 2.0)
        if tipup > 0 and t > 0.6: d = d + UP * tipup * step / max(length, 1e-6) * 3.0
        d = d + rng.standard_normal(3) * wander * math.sqrt(step)
        if kink > 0 and rng.random() < kink: d = d + rng.standard_normal(3) * 0.35
        d = unit(d)
        pts.append(pts[-1] + d * step)
        rads.append(r1 + (r0 - r1) * (1 - t) ** taper)
    return pts, rads

# -------------------------------------------------------------------------------------------------------- species
# Every number is a kind's habit; heights are the model's own (the game scales a model to each tree's height).
SPECIES = {
    "fir":    dict(conifer=True, height=24.0, base_r=0.024, trunk_wander=0.015, crown_base=0.10, shape="conical",
                   l1=dict(mode="whorl", per=5, spacing=0.55, down=(88, 8), length=0.22, up=-0.05, droop=0.35, tipup=0.35, wander=0.05, rr=0.30),
                   l2=dict(mode="plane", density=2.2, start=0.10, down=(55, 12), length=0.38, droop=0.10, wander=0.06, rr=0.45),
                   cards="bough", card_len=1.05, roots=(5, 0.07), stubs=0, flare=1.35),
    "fir_great": dict(conifer=True, height=40.0, base_r=0.020, trunk_wander=0.010, crown_base=0.42, shape="conical",
                   l1=dict(mode="whorl", per=6, spacing=0.75, down=(92, 8), length=0.20, up=-0.05, droop=0.45, tipup=0.35, wander=0.05, rr=0.28),
                   l2=dict(mode="plane", density=2.0, start=0.12, down=(55, 12), length=0.36, droop=0.10, wander=0.06, rr=0.45),
                   cards="bough", card_len=1.25, roots=(7, 0.08), stubs=6, flare=1.6),
    "fir_young": dict(conifer=True, height=4.5, base_r=0.020, trunk_wander=0.02, crown_base=0.04, shape="conical",
                   l1=dict(mode="whorl", per=5, spacing=0.35, down=(80, 8), length=0.30, up=0.0, droop=0.15, tipup=0.3, wander=0.04, rr=0.35),
                   l2=dict(mode="plane", density=3.0, start=0.10, down=(55, 12), length=0.4, droop=0.05, wander=0.05, rr=0.5),
                   cards="bough", card_len=0.40, roots=(4, 0.06), stubs=0, flare=1.2),
    "spruce": dict(conifer=True, height=26.0, base_r=0.022, trunk_wander=0.015, crown_base=0.06, shape="conical",
                   l1=dict(mode="whorl", per=5, spacing=0.50, down=(100, 10), length=0.19, up=-0.1, droop=0.65, tipup=0.55, wander=0.06, rr=0.28),
                   l2=dict(mode="plane", density=2.6, start=0.08, down=(60, 12), length=0.36, droop=0.45, wander=0.07, rr=0.45),
                   cards="bough", card_len=0.95, roots=(5, 0.07), stubs=0, flare=1.35),
    "pine":   dict(conifer=True, height=20.0, base_r=0.028, trunk_wander=0.05, crown_base=0.58, shape="umbrella",
                   l1=dict(mode="spiral", density=1.2, down=(84, 10), length=0.46, up=0.10, droop=0.0, wander=0.14, rr=0.45, kink=0.08),
                   l2=dict(mode="spiral", density=2.0, start=0.20, down=(45, 15), length=0.42, up=0.30, wander=0.12, rr=0.5),
                   l3=dict(mode="spiral", density=2.5, start=0.25, down=(40, 15), length=0.40, up=0.25, wander=0.12, rr=0.5),
                   cards="tuft", card_len=1.5, roots=(6, 0.09), stubs=3, flare=1.4),
    "beech":  dict(conifer=False, height=22.0, base_r=0.026, trunk_wander=0.03, crown_base=0.30, trunk_top=0.55, shape="spherical",
                   l1=dict(mode="spiral", density=0.75, down=(58, 12), length=0.50, up=0.10, droop=0.10, wander=0.10, rr=0.55),
                   l2=dict(mode="spiral", density=1.5, start=0.15, down=(48, 12), length=0.45, up=0.08, droop=0.18, wander=0.12, rr=0.55),
                   l3=dict(mode="spiral", density=2.4, start=0.20, down=(45, 12), length=0.45, up=0.05, droop=0.12, wander=0.12, rr=0.55),
                   cards="spray", card_len=1.05, roots=(7, 0.09), stubs=0, flare=1.5, leaves="beech"),
    "oak":    dict(conifer=False, height=20.0, base_r=0.034, trunk_wander=0.05, crown_base=0.22, trunk_top=0.38, shape="hemispherical",
                   leaders=(3, 6, 0.75, 1.15, 0.85, 0.10),
                   l1=dict(mode="spiral", density=0.9, down=(70, 12), length=0.58, up=0.12, droop=0.08, wander=0.18, rr=0.6, kink=0.10),
                   l2=dict(mode="spiral", density=1.4, start=0.15, down=(52, 15), length=0.45, up=0.08, droop=0.15, wander=0.18, rr=0.55, kink=0.10),
                   l3=dict(mode="spiral", density=2.8, start=0.20, down=(48, 15), length=0.40, up=0.05, droop=0.1, wander=0.15, rr=0.55),
                   cards="spray", card_len=1.15, roots=(8, 0.10), stubs=0, flare=1.35, leaves="oak"),
    "maple":  dict(conifer=False, height=18.0, base_r=0.026, trunk_wander=0.03, crown_base=0.30, trunk_top=0.55, shape="spherical",
                   l1=dict(mode="opposite", density=0.9, down=(45, 12), length=0.45, up=0.22, droop=0.05, wander=0.10, rr=0.55),
                   l2=dict(mode="opposite", density=1.5, start=0.20, down=(45, 12), length=0.42, up=0.1, droop=0.12, wander=0.12, rr=0.55),
                   l3=dict(mode="opposite", density=2.8, start=0.25, down=(42, 12), length=0.40, droop=0.1, wander=0.12, rr=0.55),
                   cards="spray", card_len=0.95, roots=(6, 0.09), stubs=0, flare=1.45, leaves="maple"),
    "hornbeam": dict(conifer=False, height=16.0, base_r=0.028, trunk_wander=0.05, crown_base=0.28, trunk_top=0.55, shape="tend_flame",
                   l1=dict(mode="spiral", density=1.0, down=(40, 12), length=0.42, up=0.28, droop=0.05, wander=0.12, rr=0.52),
                   l2=dict(mode="spiral", density=1.7, start=0.20, down=(42, 12), length=0.42, up=0.1, droop=0.2, wander=0.12, rr=0.55),
                   l3=dict(mode="spiral", density=3.0, start=0.25, down=(42, 12), length=0.40, droop=0.15, wander=0.12, rr=0.55),
                   cards="spray", card_len=1.0, roots=(6, 0.08), stubs=0, flare=1.4, leaves="hornbeam"),
    "birch":  dict(conifer=False, height=15.0, base_r=0.018, trunk_wander=0.05, crown_base=0.30, trunk_top=0.92, shape="tend_flame",
                   l1=dict(mode="spiral", density=1.6, down=(38, 10), length=0.30, up=0.15, droop=0.35, wander=0.10, rr=0.45),
                   l2=dict(mode="spiral", density=2.4, start=0.15, down=(40, 12), length=0.45, droop=0.75, wander=0.10, rr=0.5),
                   l3=dict(mode="spiral", density=3.0, start=0.2, down=(35, 12), length=0.5, droop=0.9, wander=0.10, rr=0.5),
                   cards="spray", card_len=0.95, roots=(5, 0.07), stubs=0, flare=1.3, leaves="birch", pendulous=True),
    "fruit":  dict(conifer=False, height=5.5, base_r=0.040, trunk_wander=0.06, crown_base=0.30, trunk_top=0.36, shape="spherical",
                   l1=dict(mode="spiral", density=1.4, down=(55, 12), length=0.55, up=0.18, droop=0.12, wander=0.15, rr=0.6, kink=0.08),
                   l2=dict(mode="spiral", density=2.4, start=0.15, down=(50, 12), length=0.45, up=0.1, droop=0.15, wander=0.15, rr=0.55),
                   l3=dict(mode="spiral", density=3.5, start=0.2, down=(45, 12), length=0.45, droop=0.15, wander=0.15, rr=0.55),
                   cards="spray", card_len=0.75, roots=(5, 0.10), stubs=0, flare=1.4, leaves="apple", fruit=0.6),
    "shrub":  dict(conifer=False, height=2.0, base_r=0.025, trunk_wander=0.1, crown_base=0.0, trunk_top=0.15, shape="spherical", shrub=11,
                   l1=dict(mode="spiral", density=2.0, down=(35, 15), length=0.55, up=0.1, droop=0.25, wander=0.18, rr=0.6),
                   l2=dict(mode="spiral", density=4.5, start=0.15, down=(50, 12), length=0.55, droop=0.15, wander=0.15, rr=0.6),
                   l3=dict(mode="spiral", density=5.0, start=0.2, down=(45, 12), length=0.5, droop=0.1, wander=0.15, rr=0.6),
                   cards="spray", card_len=0.75, roots=(0, 0.0), stubs=0, flare=1.0, leaves="rhodo"),
    "juniper": dict(conifer=True, height=2.4, base_r=0.03, trunk_wander=0.12, crown_base=0.0, trunk_top=0.2, shape="spherical", shrub=6,
                   l1=dict(mode="spiral", density=2.4, down=(40, 15), length=0.6, up=0.15, droop=0.2, wander=0.2, rr=0.6, kink=0.1),
                   l2=dict(mode="spiral", density=3.0, start=0.2, down=(45, 15), length=0.5, droop=0.1, wander=0.15, rr=0.6),
                   cards="bough", card_len=0.32, roots=(0, 0.0), stubs=0, flare=1.0),
    "snag":   dict(conifer=True, height=12.0, base_r=0.035, trunk_wander=0.04, crown_base=0.35, shape="conical", snag=True,
                   l1=dict(mode="spiral", density=0.9, down=(80, 15), length=0.22, up=0.0, droop=0.25, wander=0.12, rr=0.40, kink=0.25),
                   cards=None, card_len=0.0, roots=(6, 0.08), stubs=0, flare=1.45),
}

# ------------------------------------------------------------------------------------------------------- the tree
class Tree:
    def __init__(self, species, seed, height=None):
        self.sp = dict(SPECIES[species]); self.name = species; self.r = np.random.default_rng(seed)
        self.H = (height or self.sp["height"]) * self.r.uniform(0.92, 1.08)
        self.branches = []; self.roots = []; self.grow()

    def grow(self):
        sp, r, H = self.sp, self.r, self.H
        r0 = sp["base_r"] * H + 0.04
        if sp.get("shrub"):                                             # a shrub: several stems from the ground
            stems = []
            for k in range(sp["shrub"]):
                an = 6.2832 * (k + r.uniform(-0.3, 0.3)) / sp["shrub"]; el = math.radians(r.uniform(35, 72))
                d = np.array((math.cos(an) * math.cos(el), math.sin(el), math.sin(an) * math.cos(el)))
                foot = r.uniform(0.04, 0.10) * H / 2.0      # (stems from a spread of the ground, not one pot)
                pts, rads = grow_line(r, (math.cos(an) * foot, -0.1, math.sin(an) * foot), d, H * r.uniform(0.55, 0.85), r0 * 0.38, r0 * 0.10, seg=0.25, up=0.05, droop=0.2, wander=0.15)
                b = Branch(pts, rads, 1); self.branches.append(b); stems.append(b)
            for b in stems: self.children(b, sp.get("l2"), 2, b.length, sp)
            return
        # the trunk
        top = sp.get("trunk_top", 1.0) if not sp["conifer"] else 1.0
        if sp.get("snag"): top = r.uniform(0.55, 0.85)
        lean = np.array((r.uniform(-1, 1), 0, r.uniform(-1, 1))) * 0.02
        pts, rads = grow_line(r, (0, -0.15, 0), unit(UP + lean), H * top, r0, r0 * (0.05 if sp["conifer"] else 0.45), seg=max(0.3, H / 40),
                              up=0.6 if not sp.get("snag") else 0.0, wander=sp["trunk_wander"], taper=0.9 if sp["conifer"] else 0.7)
        y1 = pts[1][1]; ins = []                                        # rings close together at the foot (its flare)
        for y in (0.0, 0.08, 0.18, 0.3, 0.45, 0.65, 0.9, 1.2):
            yy = y * r0 / 0.5
            if yy < y1 - 0.05: ins.append(pts[0] + (pts[1] - pts[0]) * ((yy + 0.15) / max(y1 + 0.15, 1e-6)))
        pts = [pts[0]] + ins + list(pts[1:]); rads = [rads[0]] * (1 + len(ins)) + list(rads[1:])
        trunk = Branch(pts, rads, 0); self.branches.append(trunk); self.trunk = trunk
        # its limbs
        self.children(trunk, sp["l1"], 1, H, sp, crown=True)
        if not sp["conifer"]:                                           # a leaf tree's trunk ends in its leader: limbs going on up
            p, d, rr = trunk.at(1.0)
            n0, n1, s0, s1, lm, lup = sp.get("leaders", (2, 4, 0.25, 0.55, 1.0, 0.2))   # (count, spread sideways, length, rise)
            az0 = r.uniform(0, 6.28); nl = int(r.integers(n0, n1))
            for k in range(nl):
                an = az0 + 6.2832 * (k + r.uniform(-0.2, 0.2)) / nl; d2 = unit(d + np.array((math.cos(an), 0, math.sin(an))) * r.uniform(s0, s1))
                L = (H * (1 - top) * r.uniform(0.75, 1.05) + H * 0.08) * lm
                pp, rr2 = grow_line(r, p, d2, L, rr * 0.75, rr * 0.08, seg=0.4, up=lup, wander=0.10)
                b = Branch(pp, rr2, 1, 1.0); trunk.kids.append(b); self.branches.append(b)
                if sp.get("l2"): self.children(b, sp["l2"], 2, L, sp)
        for k in range(sp.get("stubs", 0)):                            # dead stubs on a great tree's bare trunk
            t = r.uniform(0.15, sp["crown_base"] - 0.03); p, d, rr = trunk.at(t)
            an = r.uniform(0, 6.28); dd = unit(np.array((math.cos(an), r.uniform(-0.1, 0.3), math.sin(an))))
            pp, rr2 = grow_line(r, p, dd, r.uniform(0.6, 1.8), rr * 0.18, rr * 0.05, seg=0.3, wander=0.1)
            b = Branch(pp, rr2, 1, t); b.dead = True; self.branches.append(b)
        self.grow_roots(r0)

    def children(self, parent, lv, level, H, sp, crown=False):
        if lv is None: return
        r = self.r
        start = sp["crown_base"] if crown else lv.get("start", 0.15)
        Lp = parent.length
        span = Lp * (1 - start)
        mode = lv["mode"]
        if mode == "whorl":
            ts = []; t = start
            while t < 0.985:
                ts.append(t); t += lv["spacing"] * r.uniform(0.8, 1.2) / Lp * (1.0 if t < 0.8 else 0.7)
        else:
            n = max(1, int(round(span * lv["density"] * r.uniform(0.85, 1.15)))); ts = list(np.sort(start + (1 - start) * (np.arange(n) + r.uniform(0.2, 0.8, n)) / n))
        az = r.uniform(0, 6.28)
        sub = {1: "l2", 2: "l3"}.get(level)
        for t in ts:
            p, d, rp = parent.at(t)
            if mode == "whorl": azs = [az + 6.2832 * (k + r.uniform(-0.15, 0.15)) / lv["per"] for k in range(lv["per"])]; az += r.uniform(0.4, 1.2)
            elif mode == "opposite": azs = [az, az + math.pi]; az += math.pi / 2 + r.uniform(-0.2, 0.2)
            elif mode == "plane": azs = [None]; az = 0 if az != 0 else 1
            else: azs = [az]; az += math.radians(137.5) + r.uniform(-0.3, 0.3)
            for a in azs:
                down = math.radians(lv["down"][0] + r.uniform(-1, 1) * lv["down"][1])
                if a is None:                                           # a conifer's side shoots: to its left and right, level
                    side = unit(np.cross(d, UP)) * (1 if (len(parent.kids) % 2 == 0) else -1)
                    cd = unit(d * math.cos(down) + side * math.sin(down) + UP * 0.05)
                else:
                    ax = perp(d); ax = rot(ax, d, a); cd = unit(rot(d, ax, down))
                # how long: by its kind's crown outline (on the trunk) or its place on its parent
                if crown:
                    tc = (t - start) / max(1 - start, 1e-6)
                    L = H * lv["length"] * SHAPES[sp["shape"]](tc) * r.uniform(0.8, 1.15)
                else:
                    L = Lp * lv["length"] * (1.0 - 0.55 * t) * r.uniform(0.75, 1.2)
                if L < 0.08: continue
                rr0 = min(rp * lv["rr"] * r.uniform(0.85, 1.1), rp * 0.85); rr0 = max(rr0, 0.004)
                pts, rads = grow_line(r, p - cd * rp * 0.3, cd, L, rr0, max(rr0 * 0.08, 0.003), seg=max(0.12, L / 12), up=lv.get("up", 0.0),
                                      droop=lv.get("droop", 0.0), wander=lv.get("wander", 0.08), tipup=lv.get("tipup", 0.0), kink=lv.get("kink", 0.0))
                b = Branch(pts, rads, level, t); parent.kids.append(b); self.branches.append(b)
                if sub and sp.get(sub): self.children(b, sp[sub], level + 1, H, sp)

    def grow_roots(self, r0):
        n, frac = self.sp["roots"]; r = self.r
        if n == 0: return
        self.root_az = []
        for k in range(n):
            an = 6.2832 * (k + r.uniform(-0.25, 0.25)) / n; self.root_az.append(an)
            d = np.array((math.cos(an), 0, math.sin(an)))
            L = max(self.H * frac * r.uniform(0.45, 0.85), 0.5)   # (short: on a slope a long root hangs in the air)
            start = np.array((0.0, r0 * 0.55, 0.0)) + d * r0 * 0.75
            pts = [start]; rads = [r0 * r.uniform(0.42, 0.55)]
            m = 12; q = start.copy()
            for i in range(1, m + 1):                                   # out along the ground, half in it, then down into it
                t = i / m; d = unit(d + np.array((r.uniform(-0.25, 0.25), 0, r.uniform(-0.25, 0.25))))
                q = q + d * L / m; rr_ = rads[0] * (1 - 0.85 * t ** 0.7)
                y = rr_ * 0.45 + r0 * 0.55 * (1 - min(t / 0.3, 1.0)) ** 2 - (0.0 if t < 0.45 else (t - 0.45) * 1.6 * (rr_ + 0.08) / 0.12)
                q = np.array((q[0], y, q[2])); pts.append(q); rads.append(rr_)
            self.roots.append(Branch(pts, rads, -1))

# ------------------------------------------------------------------------------------------------------- meshing
class Mesh:
    def __init__(self):
        self.P, self.N, self.T, self.UV, self.I = [], [], [], [], []; self.n = 0
    def add(self, P, N, T, UV, I):
        self.P.append(np.asarray(P, np.float32)); self.N.append(np.asarray(N, np.float32)); self.T.append(np.asarray(T, np.float32))
        self.UV.append(np.asarray(UV, np.float32)); self.I.append(np.asarray(I, np.int64) + self.n); self.n += len(P)
    def arrays(self):
        if not self.P: return None
        return (np.concatenate(self.P), np.concatenate(self.N), np.concatenate(self.T), np.concatenate(self.UV), np.concatenate(self.I).astype(np.uint32))
    @property
    def tris(self): return sum(len(i) for i in self.I) // 3

BARK_TILE = 1.2   # m of bark to one picture

def tube(mesh, b, sides, flare=None, rings_every=1, lobes=None, ustart=0.0):
    """A branch's bark: rings of 'sides' points, the picture at true size (BARK_TILE m), tangents for its relief."""
    pts = b.pts[::rings_every] if rings_every > 1 else b.pts
    rad = b.rad[::rings_every] if rings_every > 1 else b.rad
    if rings_every > 1 and not np.allclose(pts[-1], b.pts[-1]): pts = np.vstack([pts, b.pts[-1:]]); rad = np.concatenate([rad, b.rad[-1:]])
    n = len(pts); around = max(1, int(round(2 * math.pi * float(rad.max()) / BARK_TILE * 1.0)))
    P, N, T, UV = [], [], [], []; run = 0.0
    a = perp(unit(pts[1] - pts[0]))
    for i in range(n):
        d = unit(pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)])
        a = unit(a - d * np.dot(a, d)); bb = np.cross(d, a)            # a frame carried along (no twist)
        if i: run += float(np.linalg.norm(pts[i] - pts[i - 1]))
        rr = rad[i]
        if flare is not None: rr *= flare(pts[i][1])
        for j in range(sides + 1):
            an = 2 * math.pi * j / sides; c, s = math.cos(an), math.sin(an)
            radial = a * c + bb * s
            k = 1.0
            if lobes is not None and pts[i][1] < lobes[0]:             # the foot swells toward its roots
                h = 1.0 - max(pts[i][1], 0.0) / lobes[0]
                wx = sum(max(0.0, math.cos(math.atan2(radial[2], radial[0]) - az)) ** 8 for az in lobes[1])
                k = 1.0 + lobes[2] * h ** 2.5 * wx
            P.append(pts[i] + radial * rr * k); N.append(radial)
            tg = -a * s + bb * c; T.append(np.append(tg, 1.0))
            UV.append((ustart + around * j / sides, run / BARK_TILE))
    I = []
    for i in range(n - 1):
        for j in range(sides):
            q = i * (sides + 1) + j
            I += [q, q + sides + 1, q + 1, q + 1, q + sides + 1, q + sides + 2]
    # which way the picture's v runs against the normal and tangent: the sign makes the relief point out
    if len(P) > sides + 2:
        Tw = np.array(T); Nw = np.array(N); Pw = np.array(P)
        dv = Pw[sides + 1] - Pw[0]; s_ = 1.0 if np.dot(np.cross(Nw[0], Tw[0, :3]), dv) > 0 else -1.0
        Tw[:, 3] = s_; T = Tw
    mesh.add(P, N, T, UV, I)

def sides_for(radius, lod):
    base = 20 if radius > 0.35 else 14 if radius > 0.18 else 9 if radius > 0.08 else 6 if radius > 0.035 else 4 if radius > 0.015 else 3
    return max(3, int(base * (1.0, 0.55, 0.35)[lod]))

def card(mesh, base, out, side, length, width, rect, crown_c, fold=0.22, flip=False):
    """A spray (or a bough) of leaves: its stem at base, growing along out, 'width' across along side; folded down its
    middle (its halves hang a little: from the side it is not a line). rect: (u0, v0, u1, v1) of its picture."""
    u0, v0, u1, v1 = rect
    if flip: u0, u1 = u1, u0
    down = np.cross(out, side); down = down if down[1] < 0 else -down
    hang = down * width * 0.5 * fold
    P = []; UV = []
    for f in (0.0, 1.0):
        s = base + out * length * f
        P += [s - side * width / 2 + hang, s, s + side * width / 2 + hang]
        v = v0 + (v1 - v0) * f
        UV += [(u0, v), ((u0 + u1) / 2, v), (u1, v)]
    P = np.array(P)
    nrm = P - crown_c[0]; nrm = nrm / np.array((crown_c[1] ** 2, crown_c[2] ** 2, crown_c[1] ** 2)); nrm /= np.linalg.norm(nrm, axis=1)[:, None]
    face = unit(np.cross(side, out)); face = face if face[1] > 0 else -face
    N = unit_rows(nrm * 0.75 + face[None, :] * 0.25)
    T = np.tile(np.append(side, 1.0), (len(P), 1))
    I = [0, 3, 1, 1, 3, 4, 1, 4, 2, 2, 4, 5]
    mesh.add(P, N, T, UV, I)

def asp(R):
    return (R[2] - R[0]) / max(R[3] - R[1], 1e-6)

def unit_rows(a):
    return a / np.maximum(np.linalg.norm(a, axis=1), 1e-9)[:, None]

def crossed(mesh, c, size, rect, crown_c, rng, n=3):
    """A cluster: n cards crossing at c (a pine's tuft, a far crown's lump)."""
    tw = rng.uniform(0, math.pi)
    for k in range(n):
        an = tw + math.pi * k / n
        if k == n - 1 and n >= 3: side = np.array((math.cos(an), 0, math.sin(an))); out = unit(np.array((-math.sin(an), 0.3, math.cos(an))))
        else: side = np.array((math.cos(an), 0, math.sin(an))); out = UP
        card(mesh, c - out * size * 0.5, out, side, size, size * asp(rect), rect, crown_c, fold=0.05)

def crown_shape(tree):
    """The crown's middle and its half widths, from where its leaves are."""
    tips = np.array([b.pts[-1] for b in tree.branches if b.level >= 1 and not b.dead]) if any(b.level >= 1 for b in tree.branches) else np.array([[0, tree.H * 0.7, 0]])
    c = tips.mean(axis=0); rx = max(float(np.percentile(np.hypot(tips[:, 0] - c[0], tips[:, 2] - c[2]), 90)), 0.3)
    ry = max(float(np.percentile(np.abs(tips[:, 1] - c[1]), 90)), 0.3)
    return (c, rx, ry)

def build(tree, rects, lod, fruit_uv=None):
    """(bark mesh, leaves mesh) of one level of detail."""
    sp, rng = tree.sp, np.random.default_rng(12345 + lod)
    bark, leaves = Mesh(), Mesh()
    crown_c = crown_shape(tree)
    if lod == 3: return far(tree, rects, crown_c, rng)
    maxlevel = {0: 9, 1: 2, 2: 1}[lod]
    if sp.get("shrub"): maxlevel = {0: 9, 1: 2, 2: 1}[lod]
    r0 = tree.branches[0].rad[0] if tree.branches else 0.1
    lobes = None
    if tree.roots:
        lobes = (max(r0 * 2.8, 0.4), tree.root_az, 0.75 * sp["flare"])
    flare = (lambda y: 1.0 + 0.35 * (sp["flare"] - 1.0) * math.exp(-max(y, 0.0) / max(r0 * 1.4, 0.15))) if not sp.get("shrub") else None
    for b in tree.branches:
        if b.level > maxlevel or b.length < 0.05: continue
        if lod >= 1 and b.level >= 2 and b.rad[0] < 0.02: continue
        sd = sides_for(float(b.rad[0]), lod)
        if lod == 0 and b.rad[0] < 0.010 and b.level >= 2: continue      # twigs this thin: inside their sprays
        if sp["conifer"] and b.level >= 2 and sp.get("cards") == "bough": continue   # a fir's side shoots: inside its boughs
        if sp["conifer"] and b.level == 1 and lod >= 1 and not sp.get("shrub"):            # its limbs, from afar: only the stoutest, low ones
            if lod == 2 or b.rad[0] < 0.05: continue
        if b.level == 0 and lod == 2 and sp["conifer"] and sp.get("cards") == "bough" and not sp.get("snag"):
            # from afar a fir's few bough cards leave gaps: its trunk up through the crown showed as a pale stripe. Here it
            # stops halfway up the crown, where the boughs hide it (2026-11-20).
            stop = tree.H * (sp["crown_base"] + 0.5 * (1 - sp["crown_base"]))
            keep = int((b.pts[:, 1] <= stop).sum())       # (the trunk rises: its points below 'stop' come first)
            if keep >= 2: b = Branch(b.pts[:keep], b.rad[:keep], 0)
        if b.level == 0: tube(bark, b, max(sd, 22 if lod == 0 else 10 if lod == 1 else 6), flare=flare, lobes=lobes, rings_every=1 if lod == 0 else 2)
        else: tube(bark, b, sd, rings_every=(1 if b.level == 1 else 3) if lod == 0 else 3)
    if lod < 2:
        for b in tree.roots: tube(bark, b, max(sides_for(float(b.rad[0]), lod) - 2, 5 if lod == 0 else 4))
    kind = sp.get("cards")
    if kind is None: return bark, leaves
    A, B, C = rects["A"], rects["B"], rects["C"]
    L0 = sp["card_len"] * (tree.H / sp["height"]) ** 0.35
    if kind == "spray":
        ends = [b for b in tree.branches if not b.dead and not b.kids and b.level >= 1]
        deep = max(b.level for b in tree.branches)
        want = max(deep - 1, 1) if lod == 0 else max(deep - (1 if lod == 1 else 2), 1)
        hosts = [b for b in tree.branches if not b.dead and b.level >= want] if lod == 0 else [b for b in tree.branches if not b.dead and b.level == want]
        if lod == 1 and not hosts: hosts = ends
        per_m = {0: 3.6, 1: 1.6, 2: 0.0}[lod] * sp.get("leafy", 1.0); size = L0 * (1.0, 1.45, 2.6)[lod]
        for b in hosts:
            if lod == 2:
                c = b.pts[-1]; crossed(leaves, c, size * rng.uniform(0.9, 1.2), C, crown_c, rng); continue
            n = max(1, int(round(b.length * per_m)))
            t0_ = 0.15 if b.kids else 0.05
            for k in range(n):
                t = t0_ + (1 - t0_) * (k + rng.uniform(0.2, 0.9)) / n
                p, d, _ = b.at(min(t, 1.0))
                outw = p - crown_c[0]; outw[1] *= 0.4; outw = unit(outw)
                if sp.get("pendulous"): dd = unit(d * 0.4 + outw * 0.3 - UP * 0.9 + rng.standard_normal(3) * 0.15)
                else: dd = unit(d * 0.6 + outw * 0.5 + UP * 0.25 + rng.standard_normal(3) * 0.2)
                side = unit(np.cross(dd, UP + rng.standard_normal(3) * 0.4)) if not sp.get("pendulous") else unit(np.cross(dd, outw))
                R = A if rng.random() < 0.5 else B; ln = size * rng.uniform(0.85, 1.15)
                card(leaves, p, dd, side, ln, ln * asp(R) * rng.uniform(0.9, 1.1), R, crown_c, flip=rng.random() < 0.5)
            if lod == 0:                                                 # a spray at its very end
                p, d, _ = b.at(1.0); card(leaves, p - d * size * 0.15, unit(d + UP * 0.2), unit(np.cross(d, UP)), size * 0.9, size * 0.9 * asp(B), B, crown_c)
        if sp.get("fruit") and fruit_uv and lod < 2:                     # fruit: small balls hanging under the sprays
            fr = sp["fruit"]
            for b in ends:
                if rng.random() > fr: continue
                p, d, _ = b.at(rng.uniform(0.5, 1.0)); fruit_ball(leaves, p - UP * 0.08, 0.045 * rng.uniform(0.85, 1.15), fruit_uv[int(rng.integers(0, 2))], 6 if lod == 0 else 4)
        if lod >= 1: inner(leaves, tree, crown_c, C, rng, lod)
    elif kind == "bough":
        hosts = [b for b in tree.branches if not b.dead and (b.level == 1 if not sp.get("shrub") else b.level >= 1)]
        for b in hosts:
            seg = L0 * (1.0, 1.3, 1.9)[lod]
            n = max(1, int(math.ceil(b.length / (seg * (0.55, 0.75, 1.0)[lod]))))
            for k in range(n):
                t = min(1.0, (k + 0.3) / n); p, d, _ = b.at(t)
                side = unit(np.cross(d, UP)); out = unit(d + UP * 0.05)
                R = A if rng.random() < 0.5 else B; ln = seg * rng.uniform(0.9, 1.1); w = ln * asp(R) * (1.0 - 0.2 * t) * 1.15
                card(leaves, p - out * seg * 0.1, out, side, ln, w, R, crown_c, fold=0.30, flip=rng.random() < 0.5)
                if lod < 2:                                              # a second, turned about the bough: a V, full from every side
                    tw = rng.uniform(0.45, 0.75) * (1 if rng.random() < 0.5 else -1)
                    s2 = unit(rot(side, out, tw)); R2 = B if R is A else A
                    card(leaves, p - out * seg * 0.05, unit(out - UP * 0.08), s2, ln * 0.9, w * 0.9, R2, crown_c, fold=0.2, flip=rng.random() < 0.5)
                if lod == 0 and b.kids:                                  # and its side shoots' own sprigs
                    pass
            if lod == 0:
                for kd in b.kids:
                    p, d, _ = kd.at(0.15); side = unit(np.cross(d, UP))
                    ln = max(kd.length, seg * 0.5); card(leaves, p, unit(d + UP * 0.03), side, ln, ln * asp(B), B, crown_c, fold=0.3)
        top = tree.trunk.pts[-1] if hasattr(tree, "trunk") else None; tl = L0 * 1.1   # the leader
        for an in ((0.0, math.pi / 2) if top is not None else ()):
            card(leaves, top - UP * tl * 0.9, UP, np.array((math.cos(an), 0, math.sin(an))), tl, tl * asp(A), A, crown_c, fold=0.05)
        inner(leaves, tree, crown_c, C, rng, lod, conifer=True)
    elif kind == "tuft":
        ends = [b for b in tree.branches if not b.kids and b.level >= 1 and not b.dead]
        if lod == 2: ends = [b for b in tree.branches if b.level == 1 and not b.dead]
        size = L0 * (1.0, 1.35, 2.4)[lod]
        for b in ends:
            p, d, _ = b.at(1.0)
            crossed(leaves, p + d * size * 0.15, size * rng.uniform(0.85, 1.15), (A, B, C)[int(rng.integers(0, 3))] if lod < 2 else C, crown_c, rng, n=3 if lod < 2 else 2)
            if lod == 0 and b.length > 0.8:
                p2, d2, _ = b.at(0.6); crossed(leaves, p2, size * 0.75, A, crown_c, rng, n=2)
    return bark, leaves

def far(tree, rects, crown_c, rng):
    """Level 3 (seen from hundreds of metres): the trunk as a 4-sided post, the crown as a few large crossed cards
    of its cluster picture (C) where its leaves are: a leaf tree's in one ball, a conifer's stacked and narrowing."""
    sp = tree.sp; bark, leaves = Mesh(), Mesh(); C = rects["C"]
    stems = [b for b in tree.branches if b.level == 0] or [b for b in tree.branches if b.level == 1][:3]
    for b in stems:
        tube(bark, b, 4, rings_every=max(1, len(b.pts) // 4))
    if sp.get("cards") is None:
        for b in [b for b in tree.branches if b.level == 1][:6]: tube(bark, b, 3, rings_every=max(1, len(b.pts) // 2))
        return bark, leaves
    c, rx, ry = crown_c
    A, B = rects["A"], rects["B"]
    if sp["conifer"] and hasattr(tree, "trunk"):
        tr = tree.trunk; tiers = 9; cb = sp["crown_base"]
        lim = [b for b in tree.branches if b.level == 1 and not b.dead]
        reach = max(np.percentile([b.length for b in lim], 75), 0.5) if lim else rx
        for k in range(tiers):                                   # tiers of boughs, radiating, shorter toward the top
            t = cb + (1 - cb) * (k + 0.2) / tiers; p, _, _ = tr.at(t)
            L = max(rx, reach) * max(1.0 - (t - cb) / (1 - cb) * 0.9, 0.12) * 1.35
            n = 7 if k < tiers - 2 else 5; a0 = rng.uniform(0, 6.28)
            for j in range(n):
                an = a0 + 6.2832 * j / n; d = np.array((math.cos(an), -0.25, math.sin(an))); d = unit(d)
                side = unit(np.cross(d, UP)); R = A if j % 2 else B
                W = max(L * asp(R) * 1.6, (1 - cb) * tree.H / tiers * 1.4)
                card(leaves, p, d, side, L, W, R, crown_c, fold=0.35)
                card(leaves, p - UP * W * 0.15, d, unit(np.cross(side, d)), L, W * 1.1, B if R is A else A, crown_c, fold=0.1)   # and upright: its silhouette from the side
        top = tr.pts[-1]; tl = reach * 0.35
        for an in (0.0, math.pi / 2): card(leaves, top - UP * tl * 1.2, UP, np.array((math.cos(an), 0, math.sin(an))), tl * 1.3, tl * 1.3 * asp(A), A, crown_c, fold=0.05)
    else:
        crossed(leaves, c, (rx + ry) * 0.8, C, crown_c, rng, n=3)
        for k in range(8):                                       # clusters over the crown's shell
            an = 6.2832 * (k + rng.uniform(-0.2, 0.2)) / 8; el = rng.uniform(-0.3, 0.8)
            d = np.array((math.cos(an) * math.cos(el), math.sin(el), math.sin(an) * math.cos(el)))
            crossed(leaves, c + d * np.array((rx, ry, rx)) * 0.62, (rx + ry) * 0.55, C, crown_c, rng, n=2)
    return bark, leaves

def inner(leaves, tree, crown_c, rect, rng, lod, conifer=False):
    """Clusters inside the crown, so that no sky shows through its middle (a real crown is thick)."""
    c, rx, ry = crown_c; n = {0: 14, 1: 10, 2: 6}[lod] if not conifer else {0: 10, 1: 8, 2: 6}[lod]
    if conifer and not hasattr(tree, "trunk"): conifer = False
    if conifer:
        tr = tree.trunk
        for k in range(n):
            t = tree.sp["crown_base"] + (1 - tree.sp["crown_base"]) * (k + 0.5) / n; p, _, _ = tr.at(t)
            w = rx * (1.25 - t) * 0.9
            crossed(leaves, p, max(w, 0.4), rect, crown_c, rng, n=2)
        return
    for k in range(n):
        d = unit(rng.standard_normal(3)); q = c + d * np.array((rx, ry, rx)) * 0.45
        crossed(leaves, q, (rx + ry) * 0.5 * rng.uniform(0.55, 0.75), rect, crown_c, rng, n=3)

def fruit_ball(mesh, c, rad, uv, seg=6):
    P, N, T, UV, I = [], [], [], [], []
    for i in range(seg + 1):
        th = math.pi * i / seg
        for j in range(seg + 1):
            ph = 2 * math.pi * j / seg; d = np.array((math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)))
            P.append(c + d * rad); N.append(d); T.append((-math.sin(ph), 0, math.cos(ph), 1.0)); UV.append(uv)
    for i in range(seg):
        for j in range(seg):
            q = i * (seg + 1) + j; I += [q, q + seg + 1, q + 1, q + 1, q + seg + 1, q + seg + 2]
    mesh.add(P, N, T, UV, I)

# ------------------------------------------------------------------------------------------------------- .dmesh
def write_dmesh(path, arrays):
    """DMSH v1 (engine/Source/DoorwaysAsset/DoorwaysMesh.h): binary header, mesh header, 72-byte vertices, u32 indices,
    one submesh. UV: the engine's v runs down the picture (glTF's way): v is turned here (1 - v)."""
    P, N, T, UV, I = arrays
    # every triangle wound so that its face turns the way its normals do (Direct3D's front faces: on screen clockwise,
    # cross(p1 - p0, p2 - p0) toward the eye). The tubes were wound the other way: the game culled every trunk's near
    # wall and showed the inside of its far one, lit from behind (blue trunks, 2026-11-20).
    F = I.reshape(-1, 3).copy()
    g = np.cross(P[F[:, 1]] - P[F[:, 0]], P[F[:, 2]] - P[F[:, 0]])
    wrong = np.einsum("ij,ij->i", g, N[F].sum(axis=1)) < 0
    F[wrong] = F[wrong][:, [0, 2, 1]]; I = F.ravel()
    V = len(P); n = len(I)
    vert = np.zeros(V, dtype=[("p", "<f4", 3), ("n", "<f4", 3), ("t", "<f4", 4), ("uv", "<f4", 2), ("j", "<u2", 4), ("w", "<f4", 4)])
    vert["p"] = P; vert["n"] = unit_rows(N); vert["t"] = T; uv = UV.copy(); uv[:, 1] = 1.0 - uv[:, 1]; vert["uv"] = uv
    lo, hi = P.min(axis=0), P.max(axis=0)
    mh = struct.pack("<4I3f3f4I", V, n, 1, 72, *lo, *hi, 1 | 2 | 4, 0, 0, 0)
    sub = struct.pack("<4I", 0, n, 0, 0)
    data = mh + vert.tobytes() + I.astype("<u4").tobytes() + sub
    with open(path, "wb") as f:
        f.write(struct.pack("<4I", 0x48534D44, 1, 16, len(data))); f.write(data)
    return V, n // 3
