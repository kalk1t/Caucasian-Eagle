"""The trees' pictures (0.5.2, 2026-11-20; owner: "improve trees, create its real 3D models, refine them, their branches
and leaves, their roots").

From CC0 photographs (ambientCG, Poly Haven: art/trees/LICENSES.md) this paints, for every kind of tree:
  <kind>_bark.png / <kind>_bark_n.png   a square of its bark, tiling, and the bark's relief (normal map, OpenGL order)
  <kind>_leaves.png                     2048 x 2048, cut-out (alpha): four pictures in its quarters (SPRAY_RECTS):
                                        A, B   a twig with its leaves (a broadleaf) or a bough with its needles (a conifer)
                                        C      a dense cluster of leaves or needles (the crown from further off)
                                        D      single leaves (left 3/4) and small colour swatches (fruit, flowers: right)
  <kind>_leaves_snow.png                the same under snow (conifers and junipers above the snow line)
Leaves are cut out of the photographs one by one (their opacity maps) and laid along painted twigs: alternate or opposite
as the kind grows them, larger and closer toward the twig's end, each a little turned and shaded, the lower ones darker
under the upper ones. Transparent pixels take the colour of the nearest leaf (no dark fringe when mipmapped).

    python -I art/trees/textures.py [sources folder] [output folder]
defaults: build/tree-sources, build/trees/textures. Writes manifest.json beside them.
"""
import io, json, math, os, sys, zipfile
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "tree-sources")
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "build", "trees", "textures")
S = 1024                                  # one quarter of the leaves' picture
SPRAY_RECTS = {"A": (0.0, 0.5, 0.5, 0.5), "B": (0.5, 0.5, 0.5, 0.5), "C": (0.0, 0.0, 0.5, 0.5), "D": (0.5, 0.0, 0.5, 0.5)}  # u0, v0 (v up), w, h
SWATCH = 96                               # D's right column: swatches of SWATCH px, from the top

def zread(zipname, suffix, mode="RGB"):
    with zipfile.ZipFile(os.path.join(SRC, "acg2k", zipname + ".zip")) as z:
        n = [x for x in z.namelist() if x.endswith(suffix)][0]
        return Image.open(io.BytesIO(z.read(n))).convert(mode)

def phread(name, kind, mode="RGB"):
    return Image.open(os.path.join(SRC, "ph2k", "%s_%s.jpg" % (name, kind))).convert(mode)

# ------------------------------------------------------------------------------------------------------------ leaves
# how each photograph's leaves lie: 'up' stem at the bottom, 'left' stem at the left, 'right' stem at the right
LEAF_SETS = {"LeafSet001": "up", "LeafSet003": "up", "LeafSet004": "up", "LeafSet007": "left", "LeafSet009": "up",
             "LeafSet010": "up", "LeafSet013": "up", "LeafSet014": "up", "LeafSet016": "up", "LeafSet019": "right",
             "LeafSet022": "up", "LeafSet023": "left", "LeafSet027": "up"}
_cache = {}
def leaves_of(name):
    """The single leaves of a photograph: RGBA arrays (float 0..1), stem at the bottom, trimmed."""
    if name in _cache: return _cache[name]
    col = np.asarray(zread(name, "_Color.jpg"), np.float32) / 255.0
    op = np.asarray(zread(name, "_Opacity.jpg", "L"), np.float32) / 255.0
    lab, n = ndimage.label(op > 0.5)
    out = []
    for i, sl in enumerate(ndimage.find_objects(lab)):
        if sl is None: continue
        m = lab[sl] == i + 1
        if m.sum() < 1500: continue                    # specks
        a = op[sl] * ndimage.binary_dilation(m, iterations=2)
        rgba = np.dstack([col[sl], a])
        turn = {"up": 0, "left": 1, "right": 3}[LEAF_SETS[name]]       # quarter turns anticlockwise to stand it stem-down
        if turn == 1: rgba = np.rot90(rgba, 3)          # stem at the left -> bottom
        elif turn == 3: rgba = np.rot90(rgba, 1)        # stem at the right -> bottom
        out.append(np.ascontiguousarray(rgba))
    _cache[name] = out
    return out

def to_img(a):
    return Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8), "RGBA")

def stamp(canvas, sprite, cx, cy, length, angle, shade=1.0, tint=(1, 1, 1), shadow=0.35, pivot="stem"):
    """Lays a leaf (stem at its bottom) with its stem at (cx, cy) (pixels, y down), 'length' px long, pointing at
    'angle' (radians, 0 = up, positive = clockwise). A soft shade falls on what is under it."""
    h, w = sprite.shape[:2]; k = length / h
    im = to_img(sprite).resize((max(2, int(w * k)), max(2, int(h * k))), Image.LANCZOS)
    rgba = np.asarray(im, np.float32) / 255.0
    rgba[:, :, :3] *= np.asarray(tint, np.float32)[None, None, :] * shade
    im = to_img(rgba)
    side = 2 * max(im.width, im.height) + 4               # square: turned any way, nothing of the leaf is cut off
    big = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    big.paste(im, (big.width // 2 - im.width // 2, big.height // 2 - im.height))   # its stem at the centre
    rot = big.rotate(-math.degrees(angle), resample=Image.BICUBIC)
    x0, y0 = int(cx - rot.width / 2), int(cy - rot.height / 2)
    if shadow > 0:                                     # the shade it casts on the leaves under it
        al = np.asarray(rot, np.float32)[:, :, 3] / 255.0
        sh = ndimage.gaussian_filter(al, length * 0.035)
        region = canvas[max(y0 + 6, 0):y0 + 6 + al.shape[0], max(x0 + 4, 0):x0 + 4 + al.shape[1]]
        sh = sh[max(0, -(y0 + 6)):max(0, -(y0 + 6)) + region.shape[0], max(0, -(x0 + 4)):max(0, -(x0 + 4)) + region.shape[1]]
        region[:, :, :3] *= (1.0 - shadow * sh[:, :, None])
    src = np.asarray(rot, np.float32) / 255.0
    ys, xs = max(y0, 0), max(x0, 0)
    ye, xe = min(y0 + src.shape[0], canvas.shape[0]), min(x0 + src.shape[1], canvas.shape[1])
    if ye <= ys or xe <= xs: return
    s = src[ys - y0:ye - y0, xs - x0:xe - x0]; d = canvas[ys:ye, xs:xe]
    a = s[:, :, 3:4]
    d[:, :, :3] = s[:, :, :3] * a + d[:, :, :3] * (1 - a)
    d[:, :, 3:4] = a + d[:, :, 3:4] * (1 - a)

def twig_line(canvas, pts, w0, w1, colour):
    """A twig: a tapering line through pts (pixels), painted at 4x and brought down (smooth edges)."""
    H, W = canvas.shape[:2]; F = 4
    im = Image.new("RGBA", (W * F, H * F), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
    n = len(pts)
    for i in range(n - 1):
        t = i / max(n - 2, 1); w = (w0 + (w1 - w0) * t) * F
        dr.line([(pts[i][0] * F, pts[i][1] * F), (pts[i + 1][0] * F, pts[i + 1][1] * F)], fill=tuple(int(c * 255) for c in colour) + (255,), width=max(1, int(w)))
        dr.ellipse([pts[i + 1][0] * F - w / 2, pts[i + 1][1] * F - w / 2, pts[i + 1][0] * F + w / 2, pts[i + 1][1] * F + w / 2], fill=tuple(int(c * 255) for c in colour) + (255,))
    src = np.asarray(im.resize((W, H), Image.LANCZOS), np.float32) / 255.0
    a = src[:, :, 3:4]
    canvas[:, :, :3] = src[:, :, :3] * a + canvas[:, :, :3] * (1 - a); canvas[:, :, 3:4] = a + canvas[:, :, 3:4] * (1 - a)

def curve(r, x0, y0, x1, y1, bend, n=24):
    t = np.linspace(0, 1, n)
    px = x0 + (x1 - x0) * t + bend * np.sin(np.pi * t); py = y0 + (y1 - y0) * t
    return list(zip(px, py))

def spray(seed, sets, twig_col, leaf_len, pairs=7, opposite=False, spread=(0.55, 1.05), tint=(1, 1, 1), droop=0.0, terminal=True, dense=1.0):
    """A twig with its leaves, stem at the bottom middle, its tip near the top."""
    r = np.random.default_rng(seed); c = np.zeros((S, S, 4), np.float32)
    sprites = [s for name in sets for s in leaves_of(name)]
    bend = r.uniform(-0.06, 0.06) * S
    pts = curve(r, S * 0.5, S * 0.99, S * 0.5 + bend * 0.3, S * 0.10, bend)
    L = leaf_len * S
    jobs = []
    for i in range(pairs):
        t = 0.05 + 0.85 * (i + 0.5) / pairs; k = int(t * (len(pts) - 1)); px, py = pts[k]
        size = L * (0.70 + 0.30 * t) * r.uniform(0.85, 1.12)
        for side in ((-1, 1) if opposite else ((-1,) if i % 2 == 0 else (1,))):
            ang = side * r.uniform(*spread) * (1.0 - 0.35 * t) + droop * side
            jobs.append((py, sprites[int(r.integers(len(sprites)))], px, py, size, ang))
        if dense > 1.0 and r.random() < dense - 1.0:
            side = -1 if r.random() < 0.5 else 1
            jobs.append((py, sprites[int(r.integers(len(sprites)))], px, py, size * 0.8, side * r.uniform(0.2, 0.6)))
    twig_line(c, pts, S * 0.016, S * 0.005, twig_col)
    for _, sp, px, py, size, ang in sorted(jobs, key=lambda j: -j[0]):         # the lower ones first: the upper lie over them
        stamp(c, sp, px, py, size, ang, shade=r.uniform(0.82, 1.08), tint=tuple(t_ * r.uniform(0.96, 1.04) for t_ in tint))
    if terminal:
        tx, ty = pts[-1]; stamp(c, sprites[int(r.integers(len(sprites)))], tx, ty + L * 0.05, L * 1.02, r.uniform(-0.15, 0.15), tint=tint)
    return c

def cluster(seed, sets, twig_col, leaf_len, count=60, tint=(1, 1, 1)):
    """A dense round cluster of leaves: several short twigs from one point, leaves all round (the crown from off)."""
    r = np.random.default_rng(seed); c = np.zeros((S, S, 4), np.float32)
    sprites = [s for name in sets for s in leaves_of(name)]
    cx, cy = S * 0.5, S * 0.62; L = leaf_len * S
    for k in range(7):
        a = -1.2 + 2.4 * k / 6 + r.uniform(-0.15, 0.15)
        twig_line(c, [(cx, S * 0.98), (cx + math.sin(a) * S * 0.30, cy - math.cos(a) * S * 0.30)], S * 0.012, S * 0.004, twig_col)
    jobs = []
    for i in range(count):
        rr = math.sqrt(r.random()) * S * 0.33; th = r.uniform(0, 2 * math.pi)
        x, y = cx + math.cos(th) * rr, cy + math.sin(th) * rr * 0.9
        jobs.append((y + r.uniform(-30, 30), x, y, L * r.uniform(0.75, 1.1), math.atan2(x - cx, cy + S * 0.25 - y) + r.uniform(-0.5, 0.5)))
    for _, x, y, size, ang in sorted(jobs, key=lambda j: -j[0]):
        stamp(c, sprites[int(r.integers(len(sprites)))], x, y, size, ang, shade=r.uniform(0.78, 1.08), tint=tint)
    return c

def needle_tuft(seed, cols, count=260, long_=0.42):
    """A pine's tuft: needles in pairs radiating from the end of a shoot, painted line by line (no photograph needed)."""
    r = np.random.default_rng(seed); F = 3; W = S * F
    im = Image.new("RGBA", (W, W), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
    base = (W * 0.5, W * 0.98); tip = (W * 0.5 + r.uniform(-0.04, 0.04) * W, W * 0.40)
    dr.line([base, tip], fill=(92, 62, 40, 255), width=int(W * 0.018))
    for i in range(count):
        t = r.uniform(0.15, 1.0) ** 0.6
        p = (base[0] + (tip[0] - base[0]) * t, base[1] + (tip[1] - base[1]) * t)
        a = r.uniform(-1.25, 1.25) * (1.15 - 0.5 * t)
        ln = W * long_ * r.uniform(0.75, 1.0) * (0.75 + 0.25 * t)
        q = (p[0] + math.sin(a) * ln, p[1] - math.cos(a) * ln)
        cc = cols[int(r.integers(len(cols)))]; sh = r.uniform(0.8, 1.15)
        dr.line([p, q], fill=tuple(int(min(1, x * sh) * 255) for x in cc) + (255,), width=max(2, int(W * 0.0045)))
    a = np.asarray(im.resize((S, S), Image.LANCZOS), np.float32) / 255.0
    return a

def bough(seed, cols_tint, twig_col, sprig_len=0.30, pairs=6, dense=1.0):
    """A fir's or a spruce's bough: a stem with flat sprigs of needles (the photograph's) to both sides, a sprig at its
    end; flat, as the boughs of a fir lie."""
    r = np.random.default_rng(seed); c = np.zeros((S, S, 4), np.float32)
    sprigs = leaves_of("LeafSet019")
    pts = curve(r, S * 0.5, S * 0.99, S * 0.5, S * 0.06, r.uniform(-0.04, 0.04) * S)
    twig_line(c, pts, S * 0.020, S * 0.006, twig_col)
    jobs = []
    for i in range(pairs):
        t = 0.10 + 0.82 * (i + 0.5) / pairs; k = int(t * (len(pts) - 1)); px, py = pts[k]
        for side in (-1, 1):
            ln = S * sprig_len * (1.15 - 0.55 * t) * r.uniform(0.85, 1.1)
            jobs.append((py, px, py, ln, side * r.uniform(0.75, 1.05)))
            if dense > 1.0: jobs.append((py, px, py, ln * 0.7, side * r.uniform(0.35, 0.6)))
    for _, px, py, ln, ang in sorted(jobs, key=lambda j: -j[0]):
        stamp(c, sprigs[int(r.integers(len(sprigs)))], px, py, ln, ang, shade=r.uniform(0.80, 1.05), tint=cols_tint, shadow=0.25)
    tx, ty = pts[-1]; stamp(c, sprigs[int(r.integers(len(sprigs)))], tx, ty + S * 0.06, S * sprig_len * 0.9, 0.0, tint=cols_tint)
    return c

def snowed(img, seed, amount=0.55):
    """Snow on the top of what lies flat: drifts in patches (noise), on the leaves only."""
    r = np.random.default_rng(seed); H, W = img.shape[:2]
    n = ndimage.gaussian_filter(r.standard_normal((H, W)), 14) ; n2 = ndimage.gaussian_filter(r.standard_normal((H, W)), 4)
    f = (n - n.mean()) / n.std() * 0.8 + (n2 - n2.mean()) / n2.std() * 0.35
    lie = np.clip((f - np.percentile(f, 100 * (1 - amount))) * 1.6, 0, 1) * 0.92
    out = img.copy(); white = np.array((0.88, 0.91, 0.96), np.float32)
    out[:, :, :3] = img[:, :, :3] * (1 - lie[:, :, None]) + white[None, None, :] * lie[:, :, None]
    return out

def bleed(a, passes=12):
    """Colour into the transparent pixels from the nearest leaf (mipmaps then do not darken the leaves' edges)."""
    rgb = a[:, :, :3].copy(); m = a[:, :, 3] > 0.02
    if not m.any(): return a
    idx = ndimage.distance_transform_edt(~m, return_distances=False, return_indices=True)
    rgb = rgb[idx[0], idx[1]]
    out = a.copy(); out[:, :, :3] = np.where(m[:, :, None], a[:, :, :3], rgb); return out

# ------------------------------------------------------------------------------------------------------------- bark
def bark_from(img_col, img_nor, size=1024, tint=(1, 1, 1), sat=1.0):
    c = np.asarray(img_col.resize((size, size), Image.LANCZOS), np.float32) / 255.0
    g = c.mean(axis=2, keepdims=True); c = g + (c - g) * sat; c = np.clip(c * np.asarray(tint)[None, None, :], 0, 1)
    nn = np.asarray(img_nor.resize((size, size), Image.LANCZOS), np.float32) / 255.0
    return c, nn

def birch_bark(seed, size=1024):
    """A birch's bark (no CC0 photograph of one was found): chalk white, rows of dark lenticels across it, black rough
    patches and cracks, and its relief."""
    r = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:size, 0:size] / size
    base = 0.66 + 0.06 * ndimage.gaussian_filter(r.standard_normal((size, size)), 18, mode="wrap")   # (a real birch reflects ~0.6: at 0.86 it shone blank white in the game)
    col = np.dstack([base * 0.97, base * 0.96, base * 0.90]); h = np.zeros((size, size), np.float32)
    im = Image.new("L", (size, size), 0); dr = ImageDraw.Draw(im)
    for _ in range(620):                                # lenticels: short dark dashes across the trunk (u across)
        x, y = r.uniform(0, size), r.uniform(0, size); ln = r.uniform(8, 70); w = r.uniform(1.5, 4.5)
        for dx in (-size, 0, size):
            dr.line([(x + dx - ln / 2, y), (x + dx + ln / 2, y + r.uniform(-2, 2))], fill=int(r.uniform(150, 255)), width=int(w))
    len_ = ndimage.gaussian_filter(np.asarray(im, np.float32) / 255.0, 0.8, mode="wrap")
    blot = ndimage.gaussian_filter(r.standard_normal((size, size)), 22, mode="wrap"); blot = np.clip((blot - np.percentile(blot, 88)) * 6, 0, 1)
    rough = ndimage.gaussian_filter(r.standard_normal((size, size)), 2, mode="wrap") * blot
    dark = np.clip(len_ * 0.85 + blot * (0.65 + 0.3 * rough), 0, 1)
    col = col * (1 - dark[:, :, None]) + np.array((0.10, 0.09, 0.08))[None, None, :] * dark[:, :, None]
    h = -dark * 0.8 + 0.15 * ndimage.gaussian_filter(r.standard_normal((size, size)), 1.5, mode="wrap")
    gy, gx = np.gradient(ndimage.gaussian_filter(h, 1.2, mode="wrap"))
    n = np.dstack([-gx * 6, gy * 6, np.ones_like(h)]); n /= np.linalg.norm(n, axis=2, keepdims=True)
    return np.clip(col, 0, 1), n * 0.5 + 0.5

# ------------------------------------------------------------------------------------------------------------ kinds
BARK_BROWN = (0.30, 0.22, 0.15)
KINDS = {
    # kind: bark, leaves (A, B, C), swatches. (Bark tints bring each bark near a real bark's brightness, a mean of
    # about 0.3; at 0.5-0.6 the firs' and beeches' trunks shone white in the game's sun, 2026-11-20.)
    "fir":      dict(bark=("acg", "Bark004", (0.60, 0.58, 0.60), 0.8), conifer=True,
                     A=lambda: bough(11, (0.62, 0.78, 0.66), (0.33, 0.24, 0.17), sprig_len=0.40, pairs=7, dense=2), B=lambda: bough(12, (0.58, 0.74, 0.64), (0.33, 0.24, 0.17), sprig_len=0.42, pairs=8, dense=2),
                     C=lambda: bough(13, (0.52, 0.68, 0.60), (0.30, 0.22, 0.16), sprig_len=0.36, pairs=5, dense=2)),
    "spruce":   dict(bark=("acg", "Bark004", (0.60, 0.53, 0.49), 0.9), conifer=True,
                     A=lambda: bough(21, (0.50, 0.66, 0.62), (0.36, 0.22, 0.14), sprig_len=0.40, pairs=8, dense=2), B=lambda: bough(22, (0.48, 0.64, 0.62), (0.36, 0.22, 0.14), sprig_len=0.42, pairs=9, dense=2),
                     C=lambda: bough(23, (0.44, 0.60, 0.58), (0.33, 0.21, 0.14), sprig_len=0.36, pairs=5, dense=2)),
    "pine":     dict(bark=("ph", "pine_bark", (1.0, 1.0, 1.0), 1.0), conifer=True,
                     A=lambda: needle_tuft(31, [(0.20, 0.36, 0.17), (0.28, 0.45, 0.22), (0.16, 0.30, 0.15)]),
                     B=lambda: needle_tuft(32, [(0.22, 0.38, 0.18), (0.30, 0.47, 0.24), (0.18, 0.31, 0.16)], count=340, long_=0.38),
                     C=lambda: needle_tuft(33, [(0.18, 0.32, 0.16), (0.25, 0.41, 0.20)], count=420, long_=0.46)),
    "juniper":  dict(bark=("acg", "Bark002", (0.85, 0.75, 0.70), 0.8), conifer=True,
                     A=lambda: bough(41, (0.50, 0.62, 0.64), (0.35, 0.25, 0.18), sprig_len=0.40, pairs=4, dense=2),
                     B=lambda: bough(42, (0.46, 0.58, 0.62), (0.35, 0.25, 0.18), sprig_len=0.44, pairs=5, dense=2),
                     C=lambda: bough(43, (0.44, 0.56, 0.60), (0.33, 0.24, 0.17), sprig_len=0.46, pairs=4, dense=2)),
    "beech":    dict(bark=("acg", "Bark011", (0.76, 0.73, 0.68), 0.55),
                     A=lambda: spray(51, ["LeafSet014"], BARK_BROWN, 0.27, pairs=9, dense=1.4), B=lambda: spray(52, ["LeafSet014"], BARK_BROWN, 0.25, pairs=10, dense=1.8),
                     C=lambda: cluster(53, ["LeafSet014"], BARK_BROWN, 0.17, count=70)),
    "oak":      dict(bark=("ph", "jolcham_oak_bark_01", (1.0, 1.0, 1.0), 0.9),
                     A=lambda: spray(61, ["LeafSet016"], BARK_BROWN, 0.23, pairs=6, spread=(0.5, 1.0)), B=lambda: spray(62, ["LeafSet016"], BARK_BROWN, 0.21, pairs=7, dense=1.5),
                     C=lambda: cluster(63, ["LeafSet016"], BARK_BROWN, 0.19, count=60)),
    "maple":    dict(bark=("acg", "Bark002", (0.80, 0.77, 0.73), 0.8),
                     A=lambda: spray(71, ["LeafSet010"], (0.42, 0.28, 0.18), 0.26, pairs=4, opposite=True), B=lambda: spray(72, ["LeafSet010"], (0.42, 0.28, 0.18), 0.24, pairs=5, opposite=True),
                     C=lambda: cluster(73, ["LeafSet010"], (0.42, 0.28, 0.18), 0.21, count=45)),
    "hornbeam": dict(bark=("acg", "Bark010", (0.74, 0.74, 0.76), 0.7),
                     A=lambda: spray(81, ["LeafSet001"], BARK_BROWN, 0.19, pairs=9), B=lambda: spray(82, ["LeafSet001"], BARK_BROWN, 0.18, pairs=10, dense=1.5),
                     C=lambda: cluster(83, ["LeafSet001"], BARK_BROWN, 0.16, count=75)),
    "gold":     dict(bark=("acg", "Bark011", (0.72, 0.70, 0.68), 0.7),
                     A=lambda: spray(91, ["LeafSet007"], BARK_BROWN, 0.21, pairs=8), B=lambda: spray(92, ["LeafSet027"], (0.42, 0.28, 0.18), 0.25, pairs=4, opposite=True),
                     C=lambda: cluster(93, ["LeafSet007", "LeafSet027"], BARK_BROWN, 0.18, count=65)),
    "birch":    dict(bark=("birch",),
                     A=lambda: spray(101, ["LeafSet004"], (0.25, 0.17, 0.12), 0.15, pairs=11, spread=(0.4, 0.9), droop=0.35),
                     B=lambda: spray(102, ["LeafSet004"], (0.25, 0.17, 0.12), 0.14, pairs=12, spread=(0.4, 0.9), droop=0.5),
                     C=lambda: cluster(103, ["LeafSet004"], (0.25, 0.17, 0.12), 0.13, count=85)),
    "apple":    dict(bark=("acg", "Bark002", (0.80, 0.74, 0.70), 0.8), swatches=[(0.78, 0.12, 0.08), (0.90, 0.62, 0.14), (0.45, 0.32, 0.18)],
                     A=lambda: spray(111, ["LeafSet009"], BARK_BROWN, 0.19, pairs=7), B=lambda: spray(112, ["LeafSet009"], BARK_BROWN, 0.18, pairs=8, dense=1.5),
                     C=lambda: cluster(113, ["LeafSet009"], BARK_BROWN, 0.16, count=70)),
    "cherry":   dict(bark=("acg", "Bark010", (0.75, 0.60, 0.58), 0.9), swatches=[(0.52, 0.03, 0.07), (0.70, 0.07, 0.10), (0.30, 0.42, 0.12)],
                     A=lambda: spray(121, ["LeafSet022"], (0.40, 0.22, 0.18), 0.20, pairs=8), B=lambda: spray(122, ["LeafSet022"], (0.40, 0.22, 0.18), 0.19, pairs=9, dense=1.5),
                     C=lambda: cluster(123, ["LeafSet022"], (0.40, 0.22, 0.18), 0.17, count=70)),
    "orange":   dict(bark=("acg", "Bark002", (0.72, 0.70, 0.66), 0.7), swatches=[(0.98, 0.50, 0.05), (0.95, 0.95, 0.88), (0.30, 0.42, 0.12)],
                     A=lambda: spray(131, ["LeafSet023"], (0.24, 0.30, 0.14), 0.19, pairs=8, tint=(0.78, 0.90, 0.80)),
                     B=lambda: spray(132, ["LeafSet023"], (0.24, 0.30, 0.14), 0.18, pairs=9, dense=1.5, tint=(0.75, 0.88, 0.78)),
                     C=lambda: cluster(133, ["LeafSet023"], (0.24, 0.30, 0.14), 0.16, count=75, tint=(0.72, 0.86, 0.76))),
    "pomegranate": dict(bark=("acg", "Bark002", (0.78, 0.70, 0.62), 0.8), swatches=[(0.72, 0.13, 0.10), (0.85, 0.30, 0.12), (0.30, 0.42, 0.12)],
                     A=lambda: spray(141, ["LeafSet013"], (0.40, 0.25, 0.16), 0.15, pairs=10, tint=(0.85, 0.95, 0.80)),
                     B=lambda: spray(142, ["LeafSet013"], (0.40, 0.25, 0.16), 0.14, pairs=11, dense=1.5, tint=(0.85, 0.95, 0.80)),
                     C=lambda: cluster(143, ["LeafSet013"], (0.40, 0.25, 0.16), 0.13, count=85, tint=(0.82, 0.93, 0.78))),
    "rhodo":    dict(bark=("acg", "Bark002", (0.70, 0.62, 0.56), 0.8), swatches=[(0.78, 0.45, 0.70), (0.93, 0.89, 0.76), (0.40, 0.55, 0.20)],
                     A=lambda: spray(151, ["LeafSet003"], (0.30, 0.24, 0.16), 0.27, pairs=4, spread=(0.9, 1.5), tint=(0.62, 0.78, 0.62)),
                     B=lambda: spray(152, ["LeafSet003"], (0.30, 0.24, 0.16), 0.25, pairs=5, spread=(0.9, 1.6), dense=1.6, tint=(0.60, 0.76, 0.60)),
                     C=lambda: cluster(153, ["LeafSet003"], (0.30, 0.24, 0.16), 0.22, count=40, tint=(0.58, 0.74, 0.58))),
    "grape":    dict(bark=("acg", "Bark002", (0.62, 0.52, 0.44), 0.7), swatches=[(0.22, 0.07, 0.20), (0.62, 0.66, 0.26), (0.30, 0.42, 0.12)],
                     A=lambda: spray(161, ["LeafSet010"], (0.40, 0.30, 0.20), 0.24, pairs=4, spread=(0.8, 1.3), tint=(0.80, 0.95, 0.70)),
                     B=lambda: spray(162, ["LeafSet010"], (0.40, 0.30, 0.20), 0.22, pairs=5, spread=(0.8, 1.4), dense=1.5, tint=(0.78, 0.93, 0.68)),
                     C=lambda: cluster(163, ["LeafSet010"], (0.40, 0.30, 0.20), 0.20, count=45, tint=(0.76, 0.92, 0.66))),
    "snag":     dict(bark=("acg", "Bark004", (0.72, 0.72, 0.72), 0.25), bare=True),
}

def make_kind(name, spec):
    os.makedirs(OUT, exist_ok=True)
    b = spec["bark"]
    if b[0] == "acg": col, nor = bark_from(zread(b[1], "_Color.jpg"), zread(b[1], "_NormalGL.jpg"), tint=b[2], sat=b[3])
    elif b[0] == "ph": col, nor = bark_from(phread(b[1], "Diffuse"), phread(b[1], "nor_gl"), tint=b[2], sat=b[3])
    else: col, nor = birch_bark(7)
    Image.fromarray((col * 255).astype(np.uint8)).save(os.path.join(OUT, name + "_bark.png"))
    Image.fromarray((np.clip(nor, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT, name + "_bark_n.png"))
    if spec.get("bare"): return {}
    sheet = np.zeros((2 * S, 2 * S, 4), np.float32)
    def put(q, img):
        u0, v0, w, h = SPRAY_RECTS[q]; x0, y0 = int(u0 * 2 * S), int((1 - v0 - h) * 2 * S)
        sheet[y0:y0 + S, x0:x0 + S] = img
    put("A", spec["A"]()); put("B", spec["B"]()); put("C", spec["C"]())
    d = np.zeros((S, S, 4), np.float32)                 # D: single leaves in its left three quarters, swatches at the right
    r = np.random.default_rng(hash(name) & 0xffff)
    if not spec.get("conifer"):
        sets = {"beech": ["LeafSet014"], "oak": ["LeafSet016"], "maple": ["LeafSet010"], "hornbeam": ["LeafSet001"], "gold": ["LeafSet007"],
                "birch": ["LeafSet004"], "apple": ["LeafSet009"], "cherry": ["LeafSet022"], "orange": ["LeafSet023"], "pomegranate": ["LeafSet013"],
                "rhodo": ["LeafSet003"], "grape": ["LeafSet010"]}[name]
        sp = [s for n_ in sets for s in leaves_of(n_)]
        for k in range(4):                              # four single leaves, 2 x 2 in the left part (each its own cell)
            cx = (0.375 * S) * (0.5 + (k % 2)) ; cy = S * 0.5 * (k // 2) + S * 0.47
            stamp(d, sp[k % len(sp)], cx, cy, S * 0.42, 0.0, shadow=0.0)
    sw = spec.get("swatches", [])
    for i, c_ in enumerate(sw):
        d[i * SWATCH:(i + 1) * SWATCH, S - SWATCH:S, :3] = c_; d[i * SWATCH:(i + 1) * SWATCH, S - SWATCH:S, 3] = 1.0
    put("D", d)
    tight = {}
    for q in "ABC":                                     # each picture's own outline: the stem stays in the middle
        u0, v0, w, h = SPRAY_RECTS[q]; x0, y0 = int(u0 * 2 * S), int((1 - v0 - h) * 2 * S)
        al = sheet[y0:y0 + S, x0:x0 + S, 3] > 0.1
        ys, xs = np.nonzero(al)
        half = max(abs(xs.max() - S / 2), abs(S / 2 - xs.min())) + 6 if len(xs) else S / 2
        top = max(ys.min() - 6, 0) if len(ys) else 0
        tight[q] = [(x0 + S / 2 - half) / (2 * S), 1 - (y0 + S) / (2 * S), (x0 + S / 2 + half) / (2 * S), 1 - (y0 + top) / (2 * S)]
    sheet = bleed(sheet)
    Image.fromarray((np.clip(sheet, 0, 1) * 255).astype(np.uint8), "RGBA").save(os.path.join(OUT, name + "_leaves.png"))
    if spec.get("conifer"):
        sn = bleed(snowed(sheet, 5)); sn[:, :, 3] = sheet[:, :, 3]
        Image.fromarray((np.clip(sn, 0, 1) * 255).astype(np.uint8), "RGBA").save(os.path.join(OUT, name + "_leaves_snow.png"))
    return {"rects": tight, "swatches": [((S + S - SWATCH / 2) / (2 * S), 1.0 - ((S + i * SWATCH + SWATCH / 2) / (2 * S))) for i in range(len(sw))]}  # uv of each swatch's middle

if __name__ == "__main__":
    only = os.environ.get("TREES_ONLY_KINDS", "")
    man = {"rects": SPRAY_RECTS, "kinds": {}}
    for name, spec in KINDS.items():
        if only and name not in only.split(","): continue
        man["kinds"][name] = make_kind(name, spec); print("made", name, flush=True)
    path = os.path.join(OUT, "manifest.json")
    if only and os.path.exists(path):
        old = json.load(open(path)); old["kinds"].update(man["kinds"]); man = old
    json.dump(man, open(path, "w"), indent=1)
