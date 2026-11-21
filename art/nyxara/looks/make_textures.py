"""The looks' textures: her body in each armour tier's metal and cloth (her face, hair and skin as they are) and her wings
in each wings tier's plumage, from her own textures and the maps bake_maps.py bakes. Her own shading stays: each colour
comes from how light or dark her own texture is there (a gradient map), the wings' also from where on the wing it is.

    python make_textures.py <her bundle's Textures> <maps folder> <out folder>

Writes body_<example><tier>.png and wings_<example><tier>.png (2048 x 2048) and a contact sheet, looks_sheet.jpg.
"""
import os, sys
import numpy as np
from PIL import Image

SRC, MAPS, OUT = sys.argv[1:4]
os.makedirs(OUT, exist_ok=True)


def load(path):
    a = np.asarray(Image.open(path))
    return a.astype(np.float32) / (65535.0 if a.dtype == np.uint16 else 255.0)


def grad(x, stops):
    """x (0..1) through colour stops [(at, (r, g, b)), ...]."""
    at = np.array([s[0] for s in stops])
    cols = np.array([s[1] for s in stops], np.float32)
    return np.stack([np.interp(x, at, cols[:, k]) for k in range(3)], -1)


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def mix(a, b, w):
    return a * (1 - w[..., None]) + b * w[..., None]


def lum(rgb):
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def normalised(L, where):
    lo, hi = np.percentile(L[where], (2, 98))
    return np.clip((L - lo) / max(hi - lo, 1e-3), 0, 1)


# ---- her body
body = load(os.path.join(SRC, "Material_0_Nyxara_BaseColor.png"))
rgb, alpha = body[..., :3], body[..., 3:]
bm = load(os.path.join(MAPS, "body_map.png"))
covered = bm.sum(-1) > 0
mx, mn = rgb.max(-1), rgb.min(-1)
skinlike = ((mx - mn) / np.maximum(mx, 1e-4) > 0.18) & (rgb[..., 0] - rgb[..., 2] > 0.06)
keep = np.maximum(smooth(0.35, 0.75, bm[..., 0]), (skinlike & (bm[..., 1] > 0.78)).astype(np.float32))   # face, hair, neck
Lb = normalised(lum(rgb), covered & (keep < 0.5))

ARMOUR = {   # name: colour stops over her own light and dark
    "11": [(0, (0.05, 0.05, 0.055)), (0.35, (0.30, 0.31, 0.33)), (0.72, (0.62, 0.50, 0.28)), (1, (0.96, 0.85, 0.58))],
    "12": [(0, (0.035, 0.03, 0.035)), (0.32, (0.22, 0.05, 0.06)), (0.58, (0.60, 0.61, 0.64)), (1, (0.98, 0.98, 1.0))],   # black, wine, silver
    "13": [(0, (0.10, 0.03, 0.04)), (0.30, (0.42, 0.24, 0.08)), (0.62, (0.82, 0.62, 0.25)), (1, (1.0, 0.94, 0.66))],
    "21": [(0, (0.04, 0.045, 0.06)), (0.35, (0.30, 0.32, 0.37)), (0.70, (0.72, 0.74, 0.79)), (1, (0.96, 0.97, 1.0))],    # steel
    "22": [(0, (0.06, 0.08, 0.16)), (0.25, (0.40, 0.46, 0.60)), (0.55, (0.80, 0.84, 0.92)), (1, (1.0, 1.0, 1.0))],      # moon-silver plate
    "23": [(0, (0.03, 0.02, 0.10)), (0.35, (0.16, 0.13, 0.40)), (0.66, (0.72, 0.74, 0.95)), (1, (1.0, 0.96, 0.82))],    # midnight, starlight
    "31": [(0, (0.06, 0.035, 0.02)), (0.35, (0.30, 0.17, 0.08)), (0.66, (0.66, 0.44, 0.20)), (1, (0.95, 0.78, 0.50))],
    "32": [(0, (0.02, 0.015, 0.015)), (0.45, (0.11, 0.085, 0.08)), (0.75, (0.78, 0.26, 0.04)), (1, (1.0, 0.66, 0.22))],
    "33": [(0, (0.10, 0.02, 0.01)), (0.30, (0.50, 0.10, 0.04)), (0.62, (0.93, 0.48, 0.10)), (1, (1.0, 0.88, 0.46))],
}
for name, stops in ARMOUR.items():
    out = mix(grad(Lb, stops), rgb, keep)
    Image.fromarray((np.concatenate([out, alpha], -1) * 255 + 0.5).astype(np.uint8), "RGBA").save(os.path.join(OUT, "body_%s.png" % name))
    print("body", name)

# ---- her wings
wings = load(os.path.join(SRC, "Material_2_Wings_BaseColor.png"))
wrgb, walpha = wings[..., :3], wings[..., 3:]
wm = load(os.path.join(MAPS, "wings_map.png"))
wcov = wm.sum(-1) > 0
Lw = normalised(lum(wrgb), wcov)
t, a = wm[..., 0], wm[..., 1]                  # along the wing (root 0, tip 1), across it (leading edge 0, trailing 1)
rng = np.random.default_rng(7)
stars = np.zeros(t.shape, np.float32)          # (night-sky wings: small points of light)
ys, xs = rng.integers(0, t.shape[0], 9000), rng.integers(0, t.shape[1], 9000)
stars[ys, xs] = rng.uniform(0.6, 1.0, 9000)
stars = np.maximum.reduce([stars, np.roll(stars, 1, 0) * 0.6, np.roll(stars, 1, 1) * 0.6])


def plumage(name):
    if name == "11":   # golden eagle: dark brown, its coverts and nape golden
        c = grad(Lw, [(0, (0.09, 0.05, 0.025)), (0.5, (0.36, 0.22, 0.11)), (1, (0.76, 0.56, 0.28))])
        return mix(c, c * np.array([1.35, 1.25, 0.95]) + 0.06, smooth(0.45, 0.1, t) * smooth(0.6, 0.2, a))
    if name == "12":   # lammergeier: slate wings, rust at the root, pale shafts
        c = grad(Lw, [(0, (0.05, 0.05, 0.06)), (0.5, (0.24, 0.24, 0.27)), (1, (0.66, 0.64, 0.60))])
        return mix(c, grad(Lw, [(0, (0.30, 0.12, 0.04)), (1, (0.85, 0.50, 0.22))]), smooth(0.32, 0.08, t))
    if name == "13":   # royal eagle: near black and gold, white shoulders, gold along the trailing edge
        c = grad(Lw, [(0, (0.05, 0.035, 0.025)), (0.55, (0.30, 0.18, 0.08)), (1, (0.95, 0.82, 0.46))])
        white = grad(Lw, [(0, (0.50, 0.50, 0.52)), (0.45, (0.84, 0.84, 0.82)), (1, (1.0, 1.0, 0.97))])   # (its feathers' shading kept)
        c = mix(c, white, smooth(0.28, 0.12, t) * smooth(0.45, 0.2, a))
        return mix(c, grad(Lw, [(0, (0.55, 0.38, 0.10)), (1, (1.0, 0.88, 0.50))]), smooth(0.72, 0.92, a) * 0.8)
    if name == "21":   # silver
        return grad(Lw, [(0, (0.24, 0.26, 0.30)), (0.5, (0.70, 0.72, 0.77)), (1, (0.97, 0.98, 1.0))])
    if name == "22":   # moonlit: blue silver, the tips light
        c = grad(Lw, [(0, (0.08, 0.10, 0.20)), (0.5, (0.42, 0.50, 0.68)), (1, (0.84, 0.91, 1.0))])
        return mix(c * 0.8, np.clip(c * 1.25 + np.array([0.05, 0.12, 0.2]), 0, 1), smooth(0.62, 0.95, t))
    if name == "23":   # night sky: indigo, with stars
        c = grad(Lw, [(0, (0.02, 0.02, 0.08)), (0.5, (0.08, 0.07, 0.26)), (1, (0.30, 0.30, 0.62))])
        return np.clip(c + stars[..., None] * np.array([0.9, 0.95, 1.0]), 0, 1)
    if name == "31":   # russet
        return grad(Lw, [(0, (0.09, 0.035, 0.02)), (0.5, (0.42, 0.16, 0.07)), (1, (0.82, 0.48, 0.24))])
    if name == "32":   # ember: charcoal, burning at the edges and the tips
        c = grad(Lw, [(0, (0.025, 0.018, 0.015)), (0.5, (0.11, 0.065, 0.045)), (1, (0.40, 0.18, 0.08))])
        burn = np.maximum(smooth(0.6, 0.9, a), smooth(0.72, 0.97, t))
        return mix(c, grad(Lw, [(0, (0.55, 0.12, 0.02)), (1, (1.0, 0.62, 0.15))]), burn)
    if name == "33":   # phoenix: crimson at the root, orange, gold at the tips
        base = grad(t, [(0, (0.55, 0.05, 0.03)), (0.5, (0.95, 0.42, 0.07)), (1, (1.0, 0.86, 0.34))])
        return base * (0.45 + 0.75 * Lw[..., None])
    raise KeyError(name)


for name in ("11", "12", "13", "21", "22", "23", "31", "32", "33"):
    c = np.clip(plumage(name), 0, 1)
    c = np.where(wcov[..., None], c, wrgb)
    Image.fromarray((np.concatenate([c, walpha], -1) * 255 + 0.5).astype(np.uint8), "RGBA").save(os.path.join(OUT, "wings_%s.png" % name))
    print("wings", name)

# ---- a sheet of them all, to look at
tiles = []
for kind in ("body", "wings"):
    for name in ("11", "12", "13", "21", "22", "23", "31", "32", "33"):
        tiles.append(Image.open(os.path.join(OUT, "%s_%s.png" % (kind, name))).convert("RGB").resize((256, 256)))
sheet = Image.new("RGB", (256 * 9, 512))
for i, im in enumerate(tiles):
    sheet.paste(im, ((i % 9) * 256, (i // 9) * 256))
sheet.save(os.path.join(OUT, "looks_sheet.jpg"), quality=85)
