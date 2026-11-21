"""The ground plants' pictures (0.5.2, 2026-11-21; the owner: "Fallen logs, tomatoes, cucumbers, ferns and flowers are
still built into the map - i want also them to be 3D"): what plants.py's models wear, as textures.py does for the trees.

For every kind (log, tomato, cucumber, fern, flowers):
  <kind>_bark.png / _bark_n.png   its wood or its stems, tiling (a log's mossy bark, a tomato's hazel stake, green stems)
  <kind>_leaves.png               2048 x 2048, cut out (alpha): named pictures packed on it and colour swatches;
                                  manifest.json gets "rects" (name -> u0 v0 u1 v1, v up; the stem or the base at v0) and
                                  "swatches" (uv of each swatch's middle), as the trees' kinds have
From CC0 photographs (ambientCG Bark016, Bark002, Leaf001, Leaf003, LeafSet020, LeafSet025, LeafSet026, LeafSet029:
art/trees/LICENSES.md): the photographs' leaves cut out one by one, laid as a fern's pinnae, a tomato's leaflets, a
cucumber's leaves, a meadow flower's leaves. Moss, a root plate's soil, end grain, bracket fungi, petals, grass, fruit
skins and green stems are painted here.

    python -I art/trees/plant_textures.py [sources folder] [output folder]      PLANTS_ONLY_KINDS=log,fern: only those
defaults: build/tree-sources, build/trees/textures (beside the trees' pictures; build_trees.py copies them to the game).
"""
import io, json, math, os, sys, zipfile
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import textures as tx                                  # stamp, twig_line, curve, bleed, bark_from

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "tree-sources")
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "build", "trees", "textures")
SHEET = 2048
PAD = 12                                               # px between pictures (their mips do not run into each other)

# ------------------------------------------------------------------------------------------------------- sources
def zread(name, suffix, mode="RGB"):
    for folder in ("acg2k", "acg1k"):                  # the 2K set where it was fetched, else the 1K one
        p = os.path.join(SRC, folder, name + ".zip")
        if os.path.exists(p):
            with zipfile.ZipFile(p) as z:
                n = [x for x in z.namelist() if x.endswith(suffix)][0]
                return Image.open(io.BytesIO(z.read(n))).convert(mode)
    raise FileNotFoundError(name)

# where each photograph's leaves have their stems: turned to stand stem-down
STEMS = {"Leaf001": "down", "Leaf002": "down", "Leaf003": "down", "LeafSet020": "down", "LeafSet025": "down",
         "LeafSet026": "left", "LeafSet029": "left"}
_cache = {}
def sprites(name, least=1500):
    """The photograph's single leaves: RGBA float arrays, stem at the bottom, trimmed."""
    if name in _cache: return _cache[name]
    col = np.asarray(zread(name, "_Color.jpg"), np.float32) / 255.0
    op = np.asarray(zread(name, "_Opacity.jpg", "L"), np.float32) / 255.0
    lab, n = ndimage.label(op > 0.5)
    out = []
    for i, sl in enumerate(ndimage.find_objects(lab)):
        if sl is None: continue
        m = lab[sl] == i + 1
        if m.sum() < least: continue
        a = op[sl] * ndimage.binary_dilation(m, iterations=2)
        rgba = np.dstack([col[sl], a])
        if STEMS[name] == "left": rgba = np.rot90(rgba, 1)          # stem at the left: a quarter turn anticlockwise
        out.append(np.ascontiguousarray(rgba))
    _cache[name] = out
    return out

# --------------------------------------------------------------------------------------------------- small tools
def fbm(h, w, seed, scales=((48, 1.0), (16, 0.5), (5, 0.25)), wrap=False):
    r = np.random.default_rng(seed); f = np.zeros((h, w), np.float32)
    for s, a in scales:
        n = ndimage.gaussian_filter(r.standard_normal((h, w)).astype(np.float32), s, mode="wrap" if wrap else "reflect")
        f += a * n / max(n.std(), 1e-6)
    return f / max(f.std(), 1e-6)

def smooth(x):
    x = np.clip(x, 0.0, 1.0); return x * x * (3 - 2 * x)

def blend(base, colour, amount):
    """base (h, w, 3) toward colour by amount (h, w)."""
    return base * (1 - amount[:, :, None]) + np.asarray(colour, np.float32)[None, None, :] * amount[:, :, None]

def rgba(col, alpha):
    return np.dstack([np.clip(col, 0, 1), np.clip(alpha, 0, 1)]).astype(np.float32)

def draw_hi(w, h, F=4):
    """A canvas at F times the size, to draw on with PIL and bring down (smooth edges)."""
    im = Image.new("RGBA", (w * F, h * F), (0, 0, 0, 0)); return im, ImageDraw.Draw(im)

def down(im, w, h):
    return np.asarray(im.resize((w, h), Image.LANCZOS), np.float32) / 255.0

def over(dst, src):
    a = src[:, :, 3:4]
    dst[:, :, :3] = src[:, :, :3] * a + dst[:, :, :3] * (1 - a); dst[:, :, 3:4] = a + dst[:, :, 3:4] * (1 - a)

def c255(c, k=1.0, a=255):
    return tuple(int(np.clip(v * k, 0, 1) * 255) for v in c) + (a,)

# --------------------------------------------------------------------------------------------------------- pictures
def moss(seed, w=512, h=512):
    """A cushion of moss seen from above: an uneven outline, greens and yellow-greens, the tiny shoots as strokes."""
    r = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w]; u = (xx + 0.5) / w * 2 - 1; v = (yy + 0.5) / h * 2 - 1
    edge = fbm(h, w, seed, ((30, 1.0), (10, 0.5), (3, 0.3)))
    rr = np.hypot(u, v)
    alpha = smooth((0.88 - rr + 0.16 * edge) / 0.10)
    tone = fbm(h, w, seed + 1, ((40, 1.0), (12, 0.6), (3, 0.5)))
    col = np.dstack([np.full((h, w), 0.20), np.full((h, w), 0.31), np.full((h, w), 0.08)]).astype(np.float32)
    col = blend(col, (0.36, 0.46, 0.11), smooth((tone + 0.3) / 1.6))           # sunny yellow-green tops
    col = blend(col, (0.10, 0.17, 0.05), smooth((-tone - 0.4) / 1.2) * 0.8)   # deep shade between the cushions
    im, dr = draw_hi(w, h, 2)
    for _ in range(int(w * h / 40)):                                           # the shoots: short strokes every way
        x, y = r.uniform(0, w), r.uniform(0, h); a = r.uniform(0, 6.283); ln = r.uniform(3, 9)
        k = r.uniform(0.75, 1.35); g = (0.30 * k, 0.44 * k, 0.12 * k) if r.random() < 0.7 else (0.42 * k, 0.50 * k, 0.16 * k)
        dr.line([(2 * x, 2 * y), (2 * (x + math.cos(a) * ln), 2 * (y + math.sin(a) * ln))], fill=c255(g, 1.0, 150), width=2)
    over_ = down(im, w, h); col = col * (1 - over_[:, :, 3:4]) + over_[:, :, :3] * over_[:, :, 3:4]
    col *= (0.82 + 0.18 * smooth(1.0 - rr))[:, :, None]                       # darker toward its rim
    return rgba(col, alpha)

def grass_blade(seed, w=96, h=768, colour=(0.30, 0.46, 0.13)):
    """A blade of grass, base at the bottom: tapering to a point, its midrib paler, darker at its foot."""
    r = np.random.default_rng(seed); im, dr = draw_hi(w, h)
    F = 4; cx = w * F / 2; pts = []
    for i in range(41):
        t = i / 40; half = (w * F * 0.42) * (1 - t ** 1.6) * (0.75 + 0.25 * min(t / 0.15, 1))
        pts.append((t, half))
    left = [(cx - hf, h * F * (1 - t)) for t, hf in pts]; right = [(cx + hf, h * F * (1 - t)) for t, hf in pts][::-1]
    dr.polygon(left + right, fill=(255, 255, 255, 255))
    al = down(im, w, h)[:, :, 3]
    yy, xx = np.mgrid[0:h, 0:w]; t = 1 - (yy + 0.5) / h; off = np.abs((xx + 0.5) / w - 0.5) * 2
    col = np.dstack([np.full((h, w), colour[0]), np.full((h, w), colour[1]), np.full((h, w), colour[2])]).astype(np.float32)
    col *= (0.62 + 0.38 * smooth(t / 0.5))[:, :, None]                         # its foot in the grass's shade
    col = blend(col, (colour[0] * 1.35, colour[1] * 1.25, colour[2] * 1.2), (1 - smooth(off / 0.18)) * 0.5)   # the midrib
    col = blend(col, (0.62, 0.58, 0.30), smooth((t - 0.86) / 0.14) * 0.6)    # a dry tip
    col *= (0.96 + 0.04 * fbm(h, w, seed, ((14, 1.0),)))[:, :, None]
    return rgba(col, al)

def end_grain(seed, rotten=False, w=512):
    """A cut or broken end of a log: its rings (a little off centre, wandering), checks running out from the heart, a
    weathered grey over the wood, the bark round it; a rotten heart darker and soft."""
    r = np.random.default_rng(seed); h = w
    yy, xx = np.mgrid[0:h, 0:w]; u = (xx + 0.5) / w * 2 - 1; v = (yy + 0.5) / h * 2 - 1
    cx, cy = r.uniform(-0.12, 0.12), r.uniform(-0.12, 0.12)
    ang = np.arctan2(v - cy, u - cx); rr = np.hypot(u - cx, v - cy)
    wob = fbm(h, w, seed, ((25, 1.0), (8, 0.4))) * 0.025
    rings = smooth((np.sin((rr + wob) * 2 * math.pi * r.uniform(22, 28)) - 0.35) / 0.65)      # each year's late wood: a thin darker line
    wood = blend(np.dstack([np.full((h, w), 0.70), np.full((h, w), 0.58), np.full((h, w), 0.42)]).astype(np.float32), (0.42, 0.31, 0.20), rings * 0.75)
    wood = blend(wood, (0.55, 0.53, 0.49), 0.30 * np.ones((h, w), np.float32))              # weathered toward grey
    wood = blend(wood, (0.40, 0.30, 0.20), smooth((0.55 - rr) / 0.4) * 0.45)                 # the heartwood darker
    im = Image.new("L", (w, h), 0); dr = ImageDraw.Draw(im)
    for _ in range(int(r.integers(3, 6))):                                                     # checks: dark cracks out from the heart
        a = r.uniform(0, 6.283); r0 = r.uniform(0.02, 0.2); r1 = r.uniform(0.5, 0.95); pts = []
        for k in range(12):
            t = k / 11; rad = r0 + (r1 - r0) * t; aa = a + 0.06 * math.sin(t * 7 + a)
            pts.append(((cx + math.cos(aa) * rad + 1) * w / 2, (cy + math.sin(aa) * rad + 1) * h / 2))
        for k in range(len(pts) - 1):                                                          # (wider where it opens at the rim)
            dr.line([pts[k], pts[k + 1]], fill=230, width=max(1, int(1 + 5 * k / len(pts))))
    crack = ndimage.gaussian_filter(np.asarray(im, np.float32) / 255.0, 1.0)
    wood = blend(wood, (0.12, 0.09, 0.07), np.clip(crack * 1.2, 0, 1))
    if rotten:
        rot = smooth((0.42 + 0.12 * fbm(h, w, seed + 3, ((20, 1.0),)) - rr) / 0.2)
        wood = blend(wood, (0.22, 0.15, 0.10), rot * 0.85)
        wood = blend(wood, (0.45, 0.33, 0.20), rot * smooth(fbm(h, w, seed + 4, ((3, 1.0),)) - 0.3) * 0.5)   # crumbling fibres
    R = np.hypot(u, v); barkband = smooth((R - 0.90) / 0.04)
    wood = blend(wood, (0.22, 0.17, 0.12), barkband * (0.85 + 0.15 * fbm(h, w, seed + 5, ((2, 1.0),))))
    wood *= (0.95 + 0.05 * fbm(h, w, seed + 6, ((3, 1.0), (10, 0.7))))[:, :, None]
    return rgba(wood, smooth((1.0 - R) / 0.02))

def root_plate(seed, w=768, h=768):
    """The root plate of a fallen tree, torn up with it: earth held in its roots, stones, the roots running out from its
    middle, a little moss at its top; an uneven round outline."""
    r = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w]; u = (xx + 0.5) / w * 2 - 1; v = (yy + 0.5) / h * 2 - 1
    rr = np.hypot(u, v); edge = fbm(h, w, seed, ((40, 1.0), (14, 0.6), (4, 0.3)))
    alpha = smooth((0.86 - rr + 0.12 * edge) / 0.05)
    tone = fbm(h, w, seed + 1, ((30, 1.0), (8, 0.7), (2, 0.5)))
    col = np.dstack([np.full((h, w), 0.31), np.full((h, w), 0.24), np.full((h, w), 0.16)]).astype(np.float32)
    col = blend(col, (0.43, 0.34, 0.23), smooth((tone - 0.2) / 1.2) * 0.8)                   # drier clods
    col = blend(col, (0.17, 0.13, 0.09), smooth((-tone - 0.6) / 0.8) * 0.8)                  # hollows
    im, dr = draw_hi(w, h, 2)
    for _ in range(46):                                                                        # stones
        x, y = r.uniform(0.1, 0.9) * w, r.uniform(0.1, 0.9) * h; s = r.uniform(4, 16); g = r.uniform(0.38, 0.55)
        dr.ellipse([2 * (x - s), 2 * (y - s * 0.7), 2 * (x + s), 2 * (y + s * 0.7)], fill=c255((g, g * 0.98, g * 0.93)))
    for _ in range(34):                                                                        # roots: out from the middle, bending
        a = r.uniform(0, 6.283); rad = 0.0; x, y = w / 2 + r.uniform(-30, 30), h / 2 + r.uniform(-30, 30); wd = r.uniform(5, 14)
        pts = [(2 * x, 2 * y)]
        for k in range(16):
            a += r.uniform(-0.25, 0.25); step = r.uniform(12, 22); x += math.cos(a) * step; y += math.sin(a) * step; pts.append((2 * x, 2 * y))
        g = r.uniform(0.75, 1.15)
        for k in range(len(pts) - 1):
            dr.line([pts[k], pts[k + 1]], fill=c255((0.42, 0.31, 0.21), g), width=max(2, int(2 * wd * (1 - k / len(pts)))))
    over_ = down(im, w, h); col = col * (1 - over_[:, :, 3:4]) + over_[:, :, :3] * over_[:, :, 3:4]
    mossy = smooth((-v - 0.35) / 0.4) * smooth(fbm(h, w, seed + 2, ((12, 1.0), (4, 0.5))) - 0.1)
    col = blend(col, (0.28, 0.38, 0.10), mossy * 0.8)
    col *= (0.85 + 0.15 * smooth(1 - rr))[:, :, None]
    return rgba(col, alpha)

def bracket(seed, w=384, h=256):
    """A bracket fungus seen from above (a tinder or an artist's conk on a beech log): a half disc, its growth bands
    dark at the wood and paler to a cream rim, fine streaks out from its foot. Its foot (against the wood) at the bottom."""
    r = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w]; u = (xx + 0.5) / w * 2 - 1; v = 1 - (yy + 0.5) / h
    rr = np.hypot(u, v * w / h * 0.5 * 2); edge = fbm(h, w, seed, ((12, 1.0), (4, 0.4)))
    alpha = smooth((0.97 - rr + 0.03 * edge) / 0.03) * smooth(v / 0.04)
    bands = 0.5 + 0.5 * np.sin(rr * 2 * math.pi * r.uniform(5, 8) + edge * 0.6)
    col = np.dstack([np.full((h, w), 0.30), np.full((h, w), 0.20), np.full((h, w), 0.12)]).astype(np.float32)
    col = blend(col, (0.55, 0.40, 0.24), smooth(rr / 0.9) * 0.9)
    col = blend(col, (0.24, 0.16, 0.10), bands * 0.35)
    col = blend(col, (0.86, 0.80, 0.66), smooth((rr - 0.84) / 0.1))                             # the growing rim
    ang = np.arctan2(v, u); streak = 0.5 + 0.5 * np.sin(ang * 90 + fbm(h, w, seed + 1, ((3, 1.0),)) * 2)
    col *= (0.9 + 0.1 * streak)[:, :, None]
    return rgba(col, alpha)

def petal(kind, w=192, h=384):
    """One petal, its foot at the bottom: 'ray' (a daisy's white ray), 'poppy' (broad, red, a dark blotch at its foot),
    'butter' (a buttercup's glossy yellow), 'scab' (a scabious' lilac)."""
    F = 4; im, dr = draw_hi(w, h, F); W, H = w * F, h * F
    if kind == "ray":
        pts = []
        for i in range(31):
            t = i / 30; half = W * 0.40 * (0.35 + 0.65 * smooth(t / 0.25)) * (1 if t < 0.9 else 1 - (t - 0.9) / 0.1 * 0.3)
            pts.append((W / 2 - half, H * (1 - t * 0.97)))
        tip = [(W / 2 - W * 0.28, H * 0.03), (W / 2 - W * 0.14, H * 0.0), (W / 2, H * 0.04), (W / 2 + W * 0.14, H * 0.0), (W / 2 + W * 0.28, H * 0.03)]
        poly = pts + tip + [(W - x, y) for x, y in pts[::-1]]
    elif kind == "poppy":                                                                       # (drawn in a square: w = h)
        poly = [(W / 2 - W * 0.06, H)]
        for i in range(81):
            a = math.pi * (0.10 + 0.80 * i / 80); rad = 1.0 + 0.025 * math.sin(i * 0.9)          # a broad fan, a little crumpled at its rim
            poly.append((W / 2 - math.cos(a) * W * 0.49 * rad, H * 0.97 - math.sin(a) * H * 0.80 * rad - H * 0.12 * math.sin(a) ** 4))
        poly += [(W / 2 + W * 0.06, H)]
    else:                                                                                       # obovate: butter, scab
        poly = []
        for i in range(61):
            t = i / 60; half = W * 0.46 * math.sin(math.pi * min(t * 0.62 + 0.08, 1.0)) ** 0.7 * (0.45 + 0.55 * smooth(t / 0.35))
            poly.append((W / 2 - half, H * (1 - t)))
        poly = poly + [(W - x, y) for x, y in poly[::-1]]
    dr.polygon(poly, fill=(255, 255, 255, 255))
    al = down(im, w, h)[:, :, 3]
    yy, xx = np.mgrid[0:h, 0:w]; t = 1 - (yy + 0.5) / h; s = (xx + 0.5) / w - 0.5
    base = {"ray": (0.93, 0.93, 0.89), "poppy": (0.84, 0.09, 0.05), "butter": (0.99, 0.82, 0.10), "scab": (0.66, 0.56, 0.86)}[kind]
    col = np.dstack([np.full((h, w), base[0]), np.full((h, w), base[1]), np.full((h, w), base[2])]).astype(np.float32)
    veins = 0.5 + 0.5 * np.cos(np.arctan2(s, t + 0.05) * (26 if kind == "poppy" else 14))
    col *= (0.93 + 0.07 * veins)[:, :, None]
    if kind == "ray": col = blend(col, (0.80, 0.82, 0.45), smooth((0.16 - t) / 0.16) * 0.8)
    if kind == "poppy":
        blot = smooth((0.13 - t - 0.25 * np.abs(s)) / 0.04)
        col = blend(col, (0.06, 0.05, 0.05), blot)
        col = blend(col, (0.55, 0.04, 0.03), smooth((t - 0.6) / 0.4) * 0.35)
    if kind == "butter": col = blend(col, (1.0, 0.95, 0.55), np.exp(-((s / 0.12) ** 2)) * smooth((t - 0.25) / 0.3) * 0.45)   # its gloss
    if kind == "scab": col = blend(col, (0.80, 0.74, 0.95), smooth((t - 0.6) / 0.4) * 0.4)
    return rgba(col, al)

def bell(w=384, h=384):
    """A bellflower's bell unrolled (u round it, v from its foot to its mouth): violet-blue, five darker veins, its five
    pointed lobes cut out at the top."""
    yy, xx = np.mgrid[0:h, 0:w]; u = (xx + 0.5) / w; t = 1 - (yy + 0.5) / h
    lobe = np.abs(((u * 5) % 1.0) - 0.5) * 2                                                    # 0 at a lobe's middle, 1 between
    alpha = smooth((0.99 - 0.17 * lobe ** 1.4 - t) / 0.02)
    col = np.dstack([np.full((h, w), 0.38), np.full((h, w), 0.36), np.full((h, w), 0.78)]).astype(np.float32)
    col = blend(col, (0.55, 0.52, 0.90), smooth((t - 0.5) / 0.5) * 0.6)
    col = blend(col, (0.20, 0.18, 0.50), np.exp(-(((u * 5 + 0.5) % 1.0 - 0.5) / 0.04) ** 2) * 0.55)   # veins under the lobes' middles
    col = blend(col, (0.30, 0.40, 0.20), smooth((0.10 - t) / 0.1) * 0.7)                        # green at its foot
    return rgba(col, alpha)

def disc(kind, w=256):
    """A flower's middle from above: 'daisy' (yellow florets, dot by dot), 'poppy' (the seed head's dark crown and the
    black stamens round it), 'butter' (a green heart in a ring of yellow stamens)."""
    r = np.random.default_rng({"daisy": 1, "poppy": 2, "butter": 3}[kind]); h = w
    yy, xx = np.mgrid[0:h, 0:w]; u = (xx + 0.5) / w * 2 - 1; v = (yy + 0.5) / h * 2 - 1; rr = np.hypot(u, v)
    alpha = smooth((0.98 - rr) / 0.03)
    if kind == "daisy":
        col = blend(np.dstack([np.full((h, w), 0.95), np.full((h, w), 0.72), np.full((h, w), 0.08)]).astype(np.float32), (0.75, 0.48, 0.05), smooth((0.45 - rr) / 0.4) * 0.5)
        im, dr = draw_hi(w, h, 2)
        for k in range(900):                                                                   # the florets in a sunflower's spiral
            a = k * 2.39996; rad = math.sqrt(k / 900) * 0.95
            x, y = (1 + math.cos(a) * rad) * w / 2, (1 + math.sin(a) * rad) * h / 2
            dr.ellipse([2 * (x - 2.2), 2 * (y - 2.2), 2 * (x + 2.2), 2 * (y + 2.2)], fill=c255((1.0, 0.85, 0.25), r.uniform(0.9, 1.1), 160))
        o = down(im, w, h); col = col * (1 - o[:, :, 3:4]) + o[:, :, :3] * o[:, :, 3:4]
    elif kind == "poppy":
        col = np.dstack([np.full((h, w), 0.07), np.full((h, w), 0.06), np.full((h, w), 0.06)]).astype(np.float32)
        ang = np.arctan2(v, u); rays = np.exp(-((((ang / (2 * math.pi) * 9) % 1.0) - 0.5) / 0.08) ** 2)
        col = blend(col, (0.30, 0.36, 0.20), smooth((0.42 - rr) / 0.06) * 0.9)                  # the seed head
        col = blend(col, (0.10, 0.09, 0.12), smooth((0.42 - rr) / 0.06) * rays * 0.9)            # its stigma rays
    else:
        col = blend(np.dstack([np.full((h, w), 0.98), np.full((h, w), 0.78), np.full((h, w), 0.10)]).astype(np.float32), (0.45, 0.60, 0.15), smooth((0.35 - rr) / 0.1))
    return rgba(col, alpha)

def fruit_skin(kind, w=256, h=512):
    """Skins wrapped round a fruit (u round it, v along it): 'cucumber' (dark green, pale stripes from its flower end,
    pale warts)."""
    r = np.random.default_rng(7)
    yy, xx = np.mgrid[0:h, 0:w]; u = (xx + 0.5) / w; t = 1 - (yy + 0.5) / h
    col = np.dstack([np.full((h, w), 0.13), np.full((h, w), 0.27), np.full((h, w), 0.08)]).astype(np.float32)
    stripe = np.exp(-((((u * 8) % 1.0) - 0.5) / 0.12) ** 2) * smooth((t - 0.35) / 0.5)
    col = blend(col, (0.42, 0.55, 0.22), stripe * 0.75)
    col *= (0.88 + 0.12 * fbm(h, w, 8, ((6, 1.0), (2, 0.5)), wrap=True))[:, :, None]
    im, dr = draw_hi(w, h, 2)
    for _ in range(260):
        x, y = r.uniform(0, w), r.uniform(0, h)
        for dx in (-w, 0, w): dr.ellipse([2 * (x + dx - 2.5), 2 * (y - 2.5), 2 * (x + dx + 2.5), 2 * (y + 2.5)], fill=c255((0.62, 0.70, 0.45), 1.0, 200))
    o = down(im, w, h); col = col * (1 - o[:, :, 3:4]) + o[:, :, :3] * o[:, :, 3:4]
    return rgba(col, np.ones((h, w), np.float32))

def frond(seed, tint, pinnae=17, w=512, h=1024):
    """A fern's frond (a male fern's, a lady fern's): its rachis up the middle, the pinnae (the photograph's finely cut
    leaves) to either side, longest a third of the way up, shorter to its tip and toward its foot; its foot at the bottom
    of the picture (the stipe below it is wood: plants.py draws it)."""
    r = np.random.default_rng(seed); c = np.zeros((h, w, 4), np.float32)
    sp = sprites("Leaf003")
    pts = tx.curve(r, w * 0.5, h * 0.995, w * 0.5 + r.uniform(-0.02, 0.02) * w, h * 0.02, r.uniform(-0.015, 0.015) * w, n=48)
    tx.twig_line(c, pts, w * 0.012, w * 0.004, (0.30, 0.40, 0.14))
    jobs = []
    for i in range(pinnae):
        t = 0.03 + 0.93 * (i + 0.5) / pinnae; k = int(t * (len(pts) - 1)); px, py = pts[k]
        shape = (0.42 + 0.58 * smooth(t / 0.32)) * (1 - smooth((t - 0.32) / 0.68) * 0.86)   # longest a third of the way up
        ln = w * 0.50 * shape * r.uniform(0.92, 1.06)
        for side in (-1, 1):
            q = min(k + (1 if side > 0 else 0), len(pts) - 1)                               # (nearly opposite: one a little above)
            ang = side * r.uniform(1.10, 1.30) * (1 - 0.25 * t)
            jobs.append((py, sp[int(r.integers(len(sp)))], pts[q][0], pts[q][1], ln, ang))
    for _, s_, x, y, ln, ang in sorted(jobs, key=lambda j: -j[0]):
        tx.stamp(c, s_, x, y, ln, ang, shade=r.uniform(0.86, 1.06), tint=tuple(t_ * r.uniform(0.97, 1.03) for t_ in tint), shadow=0.22)
    tx.stamp(c, sp[int(r.integers(len(sp)))], pts[-1][0], pts[-1][1] + w * 0.04, w * 0.12, 0.0, tint=tint)
    return c

def compound(seed, name, tint, pairs, leaflet, rachis_col, w=512, h=1024, little=True, terminal=1.2):
    """A compound leaf (a tomato's): its rachis up the middle, pairs of leaflets (the photograph's), the lower ones
    smaller, little leaflets between the pairs, one at its tip; its petiole's foot at the bottom."""
    r = np.random.default_rng(seed); c = np.zeros((h, w, 4), np.float32)
    sp = sprites(name)
    pts = tx.curve(r, w * 0.5, h * 0.995, w * 0.5 + r.uniform(-0.03, 0.03) * w, h * 0.16, r.uniform(-0.03, 0.03) * w, n=40)
    tx.twig_line(c, pts, w * 0.022, w * 0.008, rachis_col)
    jobs = []
    for i in range(pairs):
        t = 0.20 + 0.72 * (i + 0.5) / pairs; k = int(t * (len(pts) - 1)); px, py = pts[k]
        ln = h * leaflet * (0.62 + 0.38 * t) * r.uniform(0.9, 1.1)
        for side in (-1, 1):
            jobs.append((py, sp[int(r.integers(len(sp)))], px, py, ln, side * r.uniform(0.95, 1.35)))
        if little and i < pairs - 1:
            t2 = t + 0.31 / pairs; k2 = int(t2 * (len(pts) - 1))
            for side in (-1, 1):
                jobs.append((pts[k2][1], sp[int(r.integers(len(sp)))], pts[k2][0], pts[k2][1], ln * 0.32, side * r.uniform(0.9, 1.4)))
    for _, s_, x, y, ln, ang in sorted(jobs, key=lambda j: -j[0]):
        tx.stamp(c, s_, x, y, ln, ang, shade=r.uniform(0.84, 1.04), tint=tuple(t_ * r.uniform(0.97, 1.03) for t_ in tint), shadow=0.3)
    tx.stamp(c, sp[int(r.integers(len(sp)))], pts[-1][0], pts[-1][1] + h * 0.02, h * leaflet * terminal, r.uniform(-0.1, 0.1), tint=tint)
    return c

def single(name, idx, tint, w=512, h=512, along=0.92):
    """One of the photograph's leaves, its stem at the bottom middle."""
    c = np.zeros((h, w, 4), np.float32); sp = sprites(name); s_ = sp[idx % len(sp)]
    tx.stamp(c, s_, w * 0.5, h * 0.995, h * along, 0.0, tint=tint, shadow=0.0)
    return c

def tone(c, sat=1.0, gain=1.0, cool=0.0):
    """A picture's colours toward their grey (sat), darker (gain), cooler (toward blue-green): leaves that lie flat in the
    full sun (a cucumber's) read neon in the game at a photograph's saturation (2026-11-21, Map Lab)."""
    rgb = c[:, :, :3]; lum = (rgb @ np.array([0.2126, 0.7152, 0.0722], np.float32))[:, :, None]
    rgb = (lum + (rgb - lum) * sat) * gain
    if cool: rgb = rgb * (1.0 - cool) + rgb * np.array([0.85, 1.0, 1.6], np.float32) * cool
    c[:, :, :3] = np.clip(rgb, 0.0, 1.0); return c

def cluster_of(seed, pics, balls=(), w=512, h=512, count=9):
    """A plant's whole crown from further off: its pictures fanned out from the bottom middle, a few fruit on them."""
    r = np.random.default_rng(seed); c = np.zeros((h, w, 4), np.float32)
    for k in range(count):
        a = -1.15 + 2.3 * (k + r.uniform(0.2, 0.8)) / count; p = pics[k % len(pics)]
        tx.stamp(c, p, w * 0.5, h * 0.98, h * r.uniform(0.62, 0.9), a, shade=r.uniform(0.85, 1.05), shadow=0.25)
    im, dr = draw_hi(w, h, 2)
    for col, n in balls:
        for _ in range(n):
            x, y, s = r.uniform(0.25, 0.75) * w, r.uniform(0.35, 0.8) * h, r.uniform(9, 15)
            dr.ellipse([2 * (x - s), 2 * (y - s), 2 * (x + s), 2 * (y + s)], fill=c255(col))
            dr.ellipse([2 * (x - s * 0.5), 2 * (y - s * 0.7), 2 * (x - s * 0.1), 2 * (y - s * 0.3)], fill=c255((1, 1, 1), 1.0, 60))
    o = down(im, w, h); over(c, o)
    return c

def stem_bark(seed, colour, streaks=0.10, flecks=None, size=512):
    """Green stems' 'bark' (tiling): fine streaks along them (v), a little hair; a fern's stipe with its brown scales."""
    r = np.random.default_rng(seed)
    n = ndimage.gaussian_filter(r.standard_normal((size, size)), (24, 1.2), mode="wrap")
    col = np.dstack([np.full((size, size), colour[0]), np.full((size, size), colour[1]), np.full((size, size), colour[2])]).astype(np.float32)
    col *= (1 + streaks * n / n.std())[:, :, None]
    if flecks:
        f = ndimage.gaussian_filter(r.standard_normal((size, size)), (3, 1.5), mode="wrap")
        col = blend(col, flecks, smooth((f / f.std() - 1.4) / 0.6) * 0.8)
    h = ndimage.gaussian_filter(n, 1.0, mode="wrap"); gy, gx = np.gradient(h)
    nn = np.dstack([-gx * 4, gy * 4, np.ones_like(h)]); nn /= np.linalg.norm(nn, axis=2, keepdims=True)
    return np.clip(col, 0, 1), nn * 0.5 + 0.5

# ------------------------------------------------------------------------------------------------------------ kinds
GREEN_TOMATO = (0.62, 0.80, 0.56)
def kinds():
    return {
        # a fallen trunk: mossy bark (Bark016); moss cushions on its top, its ends' grain, a windthrow's root plate,
        # bracket fungi, grass at its sides
        "log": dict(bark=lambda: tx.bark_from(zread("Bark016", "_Color.jpg"), zread("Bark016", "_NormalGL.jpg"), tint=(0.72, 0.69, 0.68), sat=0.75),
                    pics=lambda: {"moss": moss(3), "moss2": moss(4), "grain": end_grain(5), "rot": end_grain(6, rotten=True),
                                  "plate": root_plate(7), "fungus": bracket(8), "blade": grass_blade(9)},
                    swatches=[(0.80, 0.74, 0.60), (0.16, 0.12, 0.08), (0.60, 0.50, 0.36)]),
        # a tomato on its hazel stake: compound leaves of toothed leaflets (Leaf001), tomatoes from swatches
        "tomato": dict(bark=lambda: tx.bark_from(zread("Bark002", "_Color.jpg"), zread("Bark002", "_NormalGL.jpg"), tint=(0.78, 0.72, 0.66), sat=0.7),
                       pics=lambda: {"leafA": compound(21, "Leaf001", GREEN_TOMATO, 4, 0.25, (0.30, 0.42, 0.16)),
                                     "leafB": compound(22, "Leaf001", (0.58, 0.76, 0.52), 4, 0.23, (0.30, 0.42, 0.16)),
                                     "leafC": compound(23, "Leaf001", (0.70, 0.78, 0.48), 3, 0.27, (0.34, 0.42, 0.18))},
                       far=lambda p: {"crown": cluster_of(24, [p["leafA"], p["leafB"]], balls=[((0.80, 0.12, 0.06), 7), ((0.45, 0.62, 0.18), 3)])},
                       swatches=[(0.80, 0.11, 0.06), (0.93, 0.46, 0.09), (0.44, 0.62, 0.18), (0.25, 0.40, 0.12), (0.97, 0.82, 0.14), (0.74, 0.66, 0.48), (0.18, 0.34, 0.10)]),
        # a cucumber on the ground: its lobed leaves (LeafSet029), its skin painted
        "cucumber": dict(bark=lambda: stem_bark(31, (0.32, 0.50, 0.17), 0.12),
                         pics=lambda: {"leafA": tone(single("LeafSet029", 0, (0.80, 0.88, 0.70)), 0.65, 0.75, 0.2),
                                       "leafB": tone(single("LeafSet029", 3, (0.76, 0.86, 0.68)), 0.65, 0.75, 0.2),
                                       "leafC": tone(single("LeafSet029", 5, (0.78, 0.86, 0.66)), 0.65, 0.75, 0.2), "skin": fruit_skin("cucumber")},
                         far=lambda p: {"crown": cluster_of(34, [p["leafA"], p["leafB"], p["leafC"]], balls=[((0.98, 0.82, 0.14), 3)], count=11)},
                         swatches=[(0.98, 0.82, 0.14), (0.30, 0.48, 0.16)]),
        # a fern: its fronds of finely cut pinnae (Leaf003), its stipes green going brown, scaly at the crown
        "fern": dict(bark=lambda: stem_bark(41, (0.30, 0.34, 0.15), 0.14, flecks=(0.22, 0.14, 0.08)),
                     pics=lambda: {"frondA": frond(42, (0.64, 0.86, 0.50)), "frondB": frond(43, (0.60, 0.82, 0.46), pinnae=21),
                                   "frondC": frond(44, (0.70, 0.88, 0.48), pinnae=15)},
                     swatches=[(0.42, 0.56, 0.20), (0.36, 0.25, 0.14)]),
        # meadow flowers: grass and leaves (LeafSet020, LeafSet026, LeafSet025), petals, bells and flowers' middles painted
        "flowers": dict(bark=lambda: stem_bark(51, (0.30, 0.46, 0.15), 0.10),
                        pics=lambda: {"blade": grass_blade(52), "blade2": grass_blade(53, colour=(0.36, 0.50, 0.16)),
                                      "leaf_daisy": single("LeafSet020", 1, (0.70, 0.82, 0.60)), "leaf_poppy": single("LeafSet026", 1, (0.80, 0.90, 0.72)),
                                      "leaf_butter": single("LeafSet025", 2, (0.86, 0.94, 0.80)), "leaf_bell": single("LeafSet020", 3, (0.66, 0.80, 0.58)),
                                      "ray": petal("ray"), "poppy": petal("poppy", 320, 320), "butter": petal("butter"), "scab": petal("scab"),
                                      "bell": bell(), "disc_daisy": disc("daisy"), "disc_poppy": disc("poppy"), "disc_butter": disc("butter")},
                        swatches=[(0.28, 0.44, 0.14), (0.22, 0.38, 0.12), (0.33, 0.48, 0.16), (0.06, 0.05, 0.05), (0.42, 0.34, 0.62)]),
    }

def pack(pics, swatches):
    """Shelves: the pictures by height, each with PAD round it, from the top left; swatches (48 px) after them.
    Returns the sheet and the rects (u0, v0, u1, v1; v up)."""
    sheet = np.zeros((SHEET, SHEET, 4), np.float32); rects = {}
    x = y = 0; row = 0
    for name, img in sorted(pics.items(), key=lambda kv: -kv[1].shape[0]):
        h, w = img.shape[:2]
        if x + w + 2 * PAD > SHEET: x = 0; y += row; row = 0
        if y + h + 2 * PAD > SHEET: raise SystemExit("the sheet is full at " + name)
        x0, y0 = x + PAD, y + PAD
        sheet[y0:y0 + h, x0:x0 + w] = img
        rects[name] = [x0 / SHEET, 1 - (y0 + h) / SHEET, (x0 + w) / SHEET, 1 - y0 / SHEET]
        x += w + 2 * PAD; row = max(row, h + 2 * PAD)
    S_ = 48; uv = []
    if x + S_ * len(swatches) + PAD > SHEET: x = 0; y += row
    for i, col in enumerate(swatches):
        x0, y0 = x + PAD + i * S_, y + PAD
        sheet[y0:y0 + S_, x0:x0 + S_, :3] = col; sheet[y0:y0 + S_, x0:x0 + S_, 3] = 1.0
        uv.append(((x0 + S_ / 2) / SHEET, 1 - (y0 + S_ / 2) / SHEET))
    return sheet, rects, uv

def make(name, spec):
    os.makedirs(OUT, exist_ok=True)
    col, nor = spec["bark"]()
    Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT, name + "_bark.png"))
    Image.fromarray((np.clip(nor, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT, name + "_bark_n.png"))
    pics = spec["pics"]()
    if "far" in spec: pics.update(spec["far"](pics))
    sheet, rects, uv = pack(pics, spec["swatches"])
    sheet = tx.bleed(sheet)
    Image.fromarray((np.clip(sheet, 0, 1) * 255).astype(np.uint8), "RGBA").save(os.path.join(OUT, name + "_leaves.png"))
    return {"rects": rects, "swatches": uv}

if __name__ == "__main__":
    only = os.environ.get("PLANTS_ONLY_KINDS", "")
    path = os.path.join(OUT, "manifest.json")
    man = json.load(open(path)) if os.path.exists(path) else {"kinds": {}}
    for name, spec in kinds().items():
        if only and name not in only.split(","): continue
        man["kinds"][name] = make(name, spec); print("made", name, flush=True)
    json.dump(man, open(path, "w"), indent=1)
