"""The Eagle's Ascent's detailed map (the M key, 0.5.2; the owner 2026-11-21: "make Detailed Map for M and let user explore
whole map while M is open with mouse").

One picture of the whole round mountain from above, north (+z) up, made from the map's own data so it always matches
the world: its heights (the outer land, the near grid, the fine patches of carved rock), the ground's paint (the
terrain's splat: meadow, forest, scree, sand, trail, moss, snow, rock), rock by slope and snow by height as the terrain
shader adds them, every tree's crown, the sea, relief shading and contour lines. The way, the rings, the caves, the
Eagle Nests, the creatures and Nyxara are drawn over it by the game (AscentMap.cpp), so they stay sharp at any zoom.

Run:  python art/map/build_ascent_map.py
Reads game/assets/Maps/EaglesAscent.{txt,heights,trees,splat_a.dds,splat_b.dds}; writes
game/assets/ui/Map/AscentMap.jpg and AscentMap.txt (EXTENT x0 x1 z0 z1 of the picture, in metres).
"""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = Path(__file__).resolve().parents[2]
MAPS = ROOT / "game" / "assets" / "Maps"
OUT = ROOT / "game" / "assets" / "ui" / "Map"
PX = 2.0             # metres a pixel
MARGIN = 260.0       # metres of sea round the land's edge
CONTOUR, CONTOUR_MAJOR = 50.0, 250.0

# the ground's paints (terrain_splat.py's layers), as seen from high up in daylight
PAINT = {
    "meadow": (122, 146, 74), "forest": (70, 98, 52), "scree": (139, 132, 118), "sand": (201, 186, 140),
    "trail": (184, 156, 108), "moss": (96, 118, 66), "snow": (236, 241, 246), "rock": (118, 112, 106),
}
SEA_DEEP, SEA_SHALLOW, FOAM = np.array((34, 66, 92)), np.array((62, 118, 134)), np.array((196, 214, 210))
CANOPY = np.array((44, 70, 38))


def read_map():
    m = {"patches": [], "trail": [], "rings": [], "caves": []}
    for line in (MAPS / "EaglesAscent.txt").read_text(encoding="utf-8").splitlines():
        w = line.split()
        if not w:
            continue
        if w[0] == "heights":
            m["near"] = (float(w[1]), float(w[2]), float(w[3]), int(w[4]), int(w[5]))
        elif w[0] == "outer":
            m["outer"] = (float(w[1]), float(w[2]), float(w[3]), int(w[4]), int(w[5]), int(w[6]))
        elif w[0] == "patch":
            m["patches"].append((float(w[1]), float(w[2]), float(w[3]), int(w[4]), int(w[5]), int(w[6])))
        elif w[0] == "terrain":
            m["terrain"] = tuple(float(v) for v in w[1:8])
        elif w[0] == "trail":
            m["trail"].append(tuple(float(v) for v in w[1:4]))
    return m


def heights_file():
    return np.fromfile(MAPS / "EaglesAscent.heights", dtype="<f4")


def sample(grid, x0, z0, step, xs, zs, order=1):
    """grid[nz, nx] at world (xs, zs); NaN off it."""
    fx, fz = (xs - x0) / step, (zs - z0) / step
    nz, nx = grid.shape
    inside = (fx >= 0) & (fz >= 0) & (fx <= nx - 1) & (fz <= nz - 1)
    out = ndimage.map_coordinates(grid, [np.clip(fz, 0, nz - 1), np.clip(fx, 0, nx - 1)], order=order, mode="nearest")
    out[~inside] = np.nan
    return out, inside


def read_dds(path):
    raw = path.read_bytes()
    h, w = int.from_bytes(raw[12:16], "little"), int.from_bytes(raw[16:20], "little")
    return np.frombuffer(raw, dtype=np.uint8, offset=148, count=w * h * 4).reshape(h, w, 4).astype(np.float32) / 255.0


def main():
    m = read_map()
    allh = heights_file()
    ox, oz, ostep, onx, onz, ooff = m["outer"]
    outer = allh[ooff:ooff + onx * onz].reshape(onz, onx)
    nx0, nz0, nstep, nnx, nnz = m["near"]
    near = allh[:nnx * nnz].reshape(nnz, nnx)
    sea = m["terrain"][5]
    snowline = m["terrain"][4]

    # the picture's extent: the land (above the sea) and MARGIN round it, square cells of PX
    land = outer > sea
    zi, xi = np.nonzero(land)
    lx0, lx1 = ox + xi.min() * ostep - MARGIN, ox + xi.max() * ostep + MARGIN
    lz0, lz1 = oz + zi.min() * ostep - MARGIN, oz + zi.max() * ostep + MARGIN
    W, H = int(math.ceil((lx1 - lx0) / PX)), int(math.ceil((lz1 - lz0) / PX))
    x1, z1 = lx0 + W * PX, lz0 + H * PX
    print(f"extent x {lx0:.0f}..{x1:.0f} z {lz0:.0f}..{z1:.0f}: {W} x {H} px at {PX} m")
    cols = lx0 + (np.arange(W) + .5) * PX
    rows = z1 - (np.arange(H) + .5) * PX          # row 0 at the north (+z) edge
    XS, ZS = np.meshgrid(cols, rows)

    # heights: the outer land, the near grid over it (feathered at its edge), the fine patches over that
    hgt, _ = sample(outer, ox, oz, ostep, XS, ZS, order=3)
    hgt = np.nan_to_num(hgt, nan=sea - 40)
    hn, inn = sample(near, nx0, nz0, nstep, XS, ZS, order=3)
    edge = np.minimum.reduce([(XS - nx0), (nx0 + (nnx - 1) * nstep - XS), (ZS - nz0), (nz0 + (nnz - 1) * nstep - ZS)])
    feather = np.clip(edge / 220.0, 0, 1) * inn
    hgt = np.where(inn, hgt * (1 - feather) + np.nan_to_num(hn) * feather, hgt)
    for (px0, pz0, pstep, pnx, pnz, poff) in m["patches"]:
        g = allh[poff:poff + pnx * pnz].reshape(pnz, pnx)
        g = ndimage.uniform_filter(g, size=max(1, int(PX / pstep)))
        hp, inp = sample(g, px0, pz0, pstep, XS, ZS, order=1)
        hgt = np.where(inp, np.maximum(hgt, np.nan_to_num(hp, nan=-1e9)), hgt)

    # slope and relief light (from the north-west, as maps are lit)
    gz, gx = np.gradient(hgt, PX)          # gz along rows (south-going), gx along columns (east)
    gnorth = -gz
    slope = np.degrees(np.arctan(np.hypot(gx, gnorth)))
    nxv, nyv, nzv = -gx, np.ones_like(hgt), -gnorth
    length = np.sqrt(nxv ** 2 + nyv ** 2 + nzv ** 2)
    nxv, nyv, nzv = nxv / length, nyv / length, nzv / length
    light = np.array((-.55, .62, .56)); light /= np.linalg.norm(light)
    shade = np.clip(nxv * light[0] + nyv * light[1] + nzv * light[2], 0, 1)
    soft = np.clip(nxv * -.2 + nyv * .95 + nzv * .2, 0, 1)
    relief = .42 + .72 * shade * .78 + .22 * soft
    cavity = hgt - ndimage.gaussian_filter(hgt, 6)
    relief *= np.clip(1 + cavity * .012, .78, 1.12)

    # the ground's paint: the splat where it lies, else by height and slope
    rng = np.random.default_rng(7)
    noise = ndimage.gaussian_filter(rng.standard_normal(hgt.shape), 3) * 3.5
    # by height first (a map's tint: low green, olive, the brown and grey of the high rock, snow), the ground's own
    # paint over it where it lies, rock where it is steep (a part of it: the whole cone is steep, and must still read)
    stops = np.array((0, 400, 1100, 2000, 2900, 3500, 4300), np.float32)
    tints = np.array(((112, 138, 76), (98, 128, 68), (110, 122, 74), (128, 114, 88), (132, 124, 116), (170, 170, 172), (236, 240, 246)), np.float32)
    base = np.stack([np.interp(hgt + noise * 8, stops, tints[:, k]) for k in range(3)], -1).astype(np.float32)
    sand = np.clip(1 - (hgt - sea) / 6.0, 0, 1)[..., None]
    base = base * (1 - sand) + np.array(PAINT["sand"], np.float32) * sand
    tx0, tz0, tw, td = m["terrain"][0:4]
    a, b = read_dds(MAPS / "EaglesAscent.splat_a.dds"), read_dds(MAPS / "EaglesAscent.splat_b.dds")
    sh, sw = a.shape[:2]
    fx, fz = (XS - tx0) / tw * sw - .5, (ZS - tz0) / td * sh - .5
    ins = (fx >= 0) & (fz >= 0) & (fx <= sw - 1) & (fz <= sh - 1)
    names = ("meadow", "forest", "scree", "sand", "trail", "moss", "snow", "rock")
    weights = []
    for pic in (a, b):
        for ch in range(4):
            weights.append(ndimage.map_coordinates(pic[..., ch], [np.clip(fz, 0, sh - 1), np.clip(fx, 0, sw - 1)], order=1))
    weights[6] = weights[6] * np.clip((hgt - 3000) / 300, 0, 1)     # (snow only high up: low down the splat's snow is rock)
    total = np.maximum(sum(weights), 1e-4)
    splat = sum(wt[..., None] * np.array(PAINT[n], np.float32) for wt, n in zip(weights, names)) / total[..., None]
    sfeather = .85 * (np.clip(np.minimum.reduce([fx, sw - 1 - fx, fz, sh - 1 - fz]) / 110.0, 0, 1) * ins * np.clip(total * 1.5, 0, 1))[..., None]
    base = base * (1 - sfeather) + splat * sfeather
    rock = (np.clip((slope - 42 + noise) / 16, 0, 1) * .55)[..., None]
    rocktint = np.array(PAINT["rock"], np.float32) * (1 + np.clip((hgt - 2500) / 3000, -.15, .25))[..., None]
    base = base * (1 - rock) + rocktint * rock
    snow = np.clip((hgt - snowline + noise * 25) / 160, 0, 1) * np.clip((52 - slope) / 14, 0, 1)
    snow = snow[..., None]
    base = base * (1 - snow) + np.array(PAINT["snow"], np.float32) * snow

    # every tree's crown: a canopy over the ground, darker where it is thick
    trees = np.loadtxt(MAPS / "EaglesAscent.trees", comments="#", usecols=(0, 1, 2, 3, 4), dtype=np.float32)
    trees = trees[(trees[:, 3] > 0) & (trees[:, 4] >= 2.5)]
    col = ((trees[:, 0] - lx0) / PX).astype(int); row = ((z1 - trees[:, 2]) / PX).astype(int)
    ok = (col >= 0) & (col < W) & (row >= 0) & (row < H)
    dens = np.zeros(hgt.shape, np.float32)
    np.add.at(dens, (row[ok], col[ok]), np.clip(trees[ok, 4] / 12.0, .4, 1.6))
    crowns = ndimage.gaussian_filter(dens, 1.1)
    canopy = (1 - np.exp(-crowns * 5.5))[..., None]
    lit = np.clip(ndimage.gaussian_filter(dens, .6) - ndimage.shift(ndimage.gaussian_filter(dens, .6), (1, 1), order=1), -1, 1)[..., None]
    base = base * (1 - canopy * .82) + (CANOPY * (1 + lit * .35)) * canopy * .82

    colour = base * relief[..., None]

    # contour lines: every CONTOUR m, thicker every CONTOUR_MAJOR m (on the land)
    grad = np.maximum(np.hypot(gx, gnorth) * PX, 1e-3)    # metres of height a pixel
    for step, alpha, width in ((CONTOUR, .16, .7), (CONTOUR_MAJOR, .34, 1.0)):
        d = np.abs(((hgt / step) + .5) % 1.0 - .5) * step / grad
        line = np.clip(1 - d / width, 0, 1) * alpha * (hgt > sea + 2) * np.clip((46 - slope) / 10, 0, 1)
        colour = colour * (1 - line[..., None]) + np.array((60, 46, 30), np.float32) * line[..., None]

    # the sea: deeper further out, shallows and a line of foam at the shore
    landmask = hgt > sea + .05
    near_land = ndimage.distance_transform_edt(~landmask) * PX
    depth = np.clip(np.maximum((sea - hgt) / 40.0, near_land / 220.0), 0, 1)[..., None]
    water = SEA_SHALLOW * (1 - depth) + SEA_DEEP * depth
    water = water * (.92 + .08 * ndimage.gaussian_filter(rng.standard_normal(hgt.shape), 6)[..., None] * 3)
    wet = (~landmask).astype(np.float32)[..., None]
    shore = (np.exp(-near_land / 5.0) * (~landmask) * .5)[..., None]
    colour = colour * (1 - wet) + water * wet
    colour = colour * (1 - shore) + FOAM * shore

    # the outer edge fades into the deep sea (the picture's edge never shows as a cut)
    edge = np.minimum.reduce([XS - lx0, x1 - XS, ZS - lz0, z1 - ZS])
    fade = np.clip(edge / (MARGIN * .8), 0, 1)[..., None]
    colour = colour * fade + SEA_DEEP * .7 * (1 - fade)

    OUT.mkdir(parents=True, exist_ok=True)
    img = Image.fromarray(np.clip(colour, 0, 255).astype(np.uint8), "RGB")
    img.save(OUT / "AscentMap.jpg", quality=90, optimize=True, subsampling=0)
    (OUT / "AscentMap.txt").write_text(
        "# The Eagle's Ascent's detailed map (art/map/build_ascent_map.py): the picture's extent in metres, north (+z) up\n"
        f"EXTENT {lx0:.2f} {x1:.2f} {lz0:.2f} {z1:.2f}\nPIXEL {PX:.2f}\n", encoding="utf-8")
    print("wrote", OUT / "AscentMap.jpg", (OUT / "AscentMap.jpg").stat().st_size // 1024, "KB")


if __name__ == "__main__":
    sys.exit(main())
