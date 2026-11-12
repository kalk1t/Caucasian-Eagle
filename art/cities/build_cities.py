# Builds the seven cities on the road to Amirani.
#
# Reads   game/assets/Compiled/World/Road.txt        (the road: written by art/world/build_world.py)
# Writes  game/assets/Compiled/Cities/<Model>.dmesh   building models (through the engine's asset builder)
#         game/assets/Compiled/Cities/<City>BaseColor.png   one 1024 px sheet of wall, roof and window tiles per city
#         game/assets/Compiled/World/Cities.txt       the cities, their models, every building and street light
#
# Everything is generated here: no outside models or images. Each city has its own palette, its own
# houses and its own landmarks, taken from what the real city is known for (see CITIES below).
# A city fills a stretch of road between two arenas; its houses stand on both sides of the road, which
# is its main street. Buildings follow the banks of the terrain (the same bank rule as build_world.py).
#
# Usage: python art/cities/build_cities.py [path to DoorwaysAssetBuilder.exe]
import json
import math
import os
import pathlib
import random
import shutil
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

REPO = pathlib.Path(__file__).resolve().parents[2]
ROAD = REPO / "game" / "assets" / "Compiled" / "World" / "Road.txt"
OUT_MODELS = REPO / "game" / "assets" / "Compiled" / "Cities"
OUT_DATA = REPO / "game" / "assets" / "Compiled" / "World" / "Cities.txt"
BUILDER = sys.argv[1] if len(sys.argv) > 1 else str(REPO / "engine" / "x64" / "Release" / "DoorwaysAssetBuilder.exe")

STEP = 2.0                      # m between road samples (Road.txt STEP)
BANK = [-7.0, 3.0, -1.5, 5.0, 8.0, 18.0, 6.0, 14.0, -1.6, 30.0, -8.0, -26.0]   # build_world.py STAGES: bank height per region
WATER = {2: -13.1, 8: 17.1}     # build_world.py WATER: region -> water level
TILE, SHEET = 128, 1024         # texture tile and sheet size in pixels
FOUNDATION = 4.0                # m of plinth under every house (hides uneven ground)
STREET_GAP = 2.2                # m between the road's edge and a house front

# ---- the cities, in the order Nyxara walks through them: (key, name, metres along the road, length, what it is known for)
CITIES = [
    ("Telavi", "Telavi", 443, 104, "wine"),
    ("Tbilisi", "Tbilisi", 1325, 116, "springs"),
    ("Mtskheta", "Mtskheta", 2057, 110, "cathedral"),
    ("Gori", "Gori", 2983, 116, "rock"),
    ("Kutaisi", "Kutaisi", 3738, 116, "fleece"),
    ("Batumi", "Batumi", 4561, 150, "sea"),
    ("Mestia", "Mestia", 5775, 100, "towers"),
]

# tiles of a city sheet
(WALL, WIN_LIT, WIN_DARK, DOOR, BALC, BASE, ROOF, STONE, SLIT, ARCH_LIT, ROCK, CAVE, WOOD, ROOF2, CLAY, VINE, SAIL, GOLD, BRICK,
 WHITE, RED, ARCH_DARK, CHURCH, CHURCH_DOOR) = range(24)

PALETTES = {
    #            wall plaster      roof            stone           wood            landmark roof    base
    "Telavi":   dict(wall=(196, 160, 112), roof=(150, 70, 48), stone=(150, 132, 108), wood=(92, 58, 34), roof2=(128, 62, 44), base=(110, 98, 84), trim=(236, 222, 190)),
    "Tbilisi":  dict(wall=(206, 176, 150), roof=(134, 76, 60), stone=(158, 138, 112), wood=(52, 104, 110), roof2=(120, 70, 56), base=(118, 100, 86), trim=(238, 228, 208)),
    "Mtskheta": dict(wall=(176, 170, 138), roof=(96, 112, 96), stone=(186, 176, 132), wood=(84, 62, 42), roof2=(104, 122, 108), base=(124, 118, 98), trim=(220, 212, 176)),
    "Gori":     dict(wall=(178, 152, 116), roof=(120, 86, 62), stone=(150, 128, 100), wood=(86, 60, 40), roof2=(112, 84, 62), base=(126, 106, 84), trim=(214, 196, 160)),
    "Kutaisi":  dict(wall=(222, 216, 200), roof=(150, 84, 62), stone=(214, 208, 190), wood=(96, 70, 48), roof2=(96, 138, 122), base=(150, 144, 130), trim=(246, 242, 230)),
    "Batumi":   dict(wall=(232, 234, 232), roof=(60, 104, 140), stone=(206, 210, 212), wood=(44, 92, 132), roof2=(170, 52, 44), base=(140, 146, 150), trim=(250, 250, 250)),
    "Mestia":   dict(wall=(120, 116, 110), roof=(70, 74, 82), stone=(128, 124, 116), wood=(74, 54, 38), roof2=(64, 68, 76), base=(96, 94, 90), trim=(168, 164, 156)),
}


# =============================================================== textures
def noise(n, cells, seed):
    r = np.random.default_rng(seed)
    small = (r.random((cells, cells)) * 255).astype(np.uint8)
    return np.asarray(Image.fromarray(small).resize((n, n), Image.BICUBIC), np.float32) / 255


def shade(color, amount):
    return tuple(int(max(0, min(255, c * amount))) for c in color)


def grain(img, seed, strength=.10):
    n = img.size[0]
    a = np.asarray(img, np.float32)
    f = 1 + strength * (noise(n, 28, seed) - .5) * 2 + strength * .6 * (noise(n, 6, seed + 1) - .5) * 2
    return Image.fromarray(np.clip(a * f[..., None], 0, 255).astype(np.uint8))


def blocks(color, rows, seed, mortar=.62, jitter=.16):
    n = TILE * 2
    img = Image.new("RGB", (n, n), shade(color, mortar)); d = ImageDraw.Draw(img); r = random.Random(seed)
    h = n / rows
    for j in range(rows):
        x = -r.uniform(0, n / 4)
        while x < n:
            w = r.uniform(n / 5.5, n / 3)
            d.rectangle([x + 1.5, j * h + 1.5, x + w - 1.5, (j + 1) * h - 1.5], fill=shade(color, 1 + r.uniform(-jitter, jitter)))
            x += w
    return grain(img, seed)


def plaster(color, seed):
    n = TILE * 2
    img = Image.new("RGB", (n, n), color)
    a = np.asarray(grain(img, seed, .07), np.float32)
    y = np.linspace(0, 1, n)[:, None, None]
    a *= 1 - .16 * np.clip((y - .78) / .22, 0, 1)                  # damp at the foot of the wall
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def roof_tiles(color, seed, rows=7):
    n = TILE * 2
    img = Image.new("RGB", (n, n), shade(color, .55)); d = ImageDraw.Draw(img); r = random.Random(seed)
    h = n / rows; w = n / 8
    for j in range(rows):
        for i in range(-1, 9):
            x = i * w + (w / 2 if j % 2 else 0)
            c = shade(color, 1 + r.uniform(-.18, .14))
            d.pieslice([x, j * h - h * .9, x + w, j * h + h * 1.0], 0, 180, fill=c)
            d.rectangle([x + 1, j * h, x + w - 1, j * h + h * .1], fill=c)
    return grain(img, seed, .06)


def planks(color, seed, boards=7):
    n = TILE * 2
    img = Image.new("RGB", (n, n), shade(color, .5)); d = ImageDraw.Draw(img); r = random.Random(seed)
    w = n / boards
    for i in range(boards):
        d.rectangle([i * w + 1.5, 0, (i + 1) * w - 1.5, n], fill=shade(color, 1 + r.uniform(-.2, .16)))
    return grain(img, seed, .12)


def rock(color, seed):
    n = TILE * 2
    a = np.zeros((n, n, 3), np.float32) + np.array(color, np.float32)
    strata = np.sin(np.linspace(0, 19, n)[:, None] + noise(n, 5, seed) * 7) * .08
    f = 1 + strata + (noise(n, 14, seed + 1) - .5) * .42 + (noise(n, 40, seed + 2) - .5) * .16
    return Image.fromarray(np.clip(a * f[..., None], 0, 255).astype(np.uint8))


def flat(color, seed, strength=.08):
    return grain(Image.new("RGB", (TILE * 2, TILE * 2), color), seed, strength)


def glass(d, box, lit, arch, r):
    """Window glass in 'box' (x0, y0, x1, y1): warm lamp light or the dark of an empty room."""
    x0, y0, x1, y1 = box
    top = (255, 214, 130) if lit else (26, 36, 48)
    low = (236, 136, 52) if lit else (12, 18, 26)
    for y in range(int(y0), int(y1)):
        t = (y - y0) / max(1, y1 - y0)
        d.line([x0, y, x1, y], fill=tuple(int(top[k] * (1 - t) + low[k] * t) for k in range(3)))
    if arch:
        w = x1 - x0
        d.pieslice([x0, y0 - w / 2, x1, y0 + w / 2], 180, 360, fill=top)


def opening(base, pal, kind, seed, lit=True):
    """A wall tile with a window, door or balcony door painted into it."""
    n = TILE * 2
    img = base.copy(); d = ImageDraw.Draw(img); r = random.Random(seed)
    frame = pal["trim"]; wood = pal["wood"]
    if kind == "window":
        x0, x1, y0, y1 = n * .30, n * .70, n * .26, n * .76
        d.rectangle([x0 - 7, y0 - 7, x1 + 7, y1 + 7], fill=frame)
        glass(d, (x0, y0, x1, y1), lit, False, r)
        d.line([(x0 + x1) / 2, y0, (x0 + x1) / 2, y1], fill=shade(wood, .8), width=5)
        d.line([x0, y0 + (y1 - y0) * .42, x1, y0 + (y1 - y0) * .42], fill=shade(wood, .8), width=5)
        d.rectangle([x0 - 12, y1 + 7, x1 + 12, y1 + 15], fill=shade(frame, .8))                       # sill
    elif kind == "arch":
        x0, x1, y0, y1 = n * .36, n * .64, n * .30, n * .84
        d.pieslice([x0 - 8, y0 - (x1 - x0) / 2 - 8, x1 + 8, y0 + (x1 - x0) / 2 + 8], 180, 360, fill=frame)
        d.rectangle([x0 - 8, y0, x1 + 8, y1 + 6], fill=frame)
        glass(d, (x0, y0, x1, y1), lit, True, r)
        d.line([(x0 + x1) / 2, y0 - (x1 - x0) / 2, (x0 + x1) / 2, y1], fill=shade(frame, .6), width=4)
    elif kind == "slit":
        d.rectangle([n * .455, n * .28, n * .545, n * .70], fill=(10, 12, 16))
        d.rectangle([n * .43, n * .70, n * .57, n * .73], fill=shade(frame, .8))
    elif kind in ("door", "churchdoor"):
        wide = .26 if kind == "door" else .30
        x0, x1, y0 = n * (.5 - wide / 2), n * (.5 + wide / 2), n * (.34 if kind == "door" else .30)
        d.pieslice([x0 - 9, y0 - (x1 - x0) / 2 - 9, x1 + 9, y0 + (x1 - x0) / 2 + 9], 180, 360, fill=frame)
        d.rectangle([x0 - 9, y0, x1 + 9, n], fill=frame)
        d.pieslice([x0, y0 - (x1 - x0) / 2, x1, y0 + (x1 - x0) / 2], 180, 360, fill=shade(wood, .9))
        d.rectangle([x0, y0, x1, n], fill=shade(wood, .9))
        for k in range(1, 4):
            d.line([x0 + (x1 - x0) * k / 4, y0, x0 + (x1 - x0) * k / 4, n], fill=shade(wood, .6), width=3)
        d.ellipse([x1 - 16, n * .66, x1 - 8, n * .66 + 8], fill=(214, 176, 84))
    elif kind == "balcony":
        x0, x1, y0 = n * .30, n * .70, n * .16
        d.rectangle([x0 - 7, y0 - 7, x1 + 7, n], fill=frame)
        glass(d, (x0, y0, x1, n * .94), lit, False, r)
        d.line([(x0 + x1) / 2, y0, (x0 + x1) / 2, n], fill=shade(wood, .8), width=6)
        d.line([x0, y0 + n * .2, x1, y0 + n * .2], fill=shade(wood, .8), width=5)
    elif kind == "cave":
        x0, x1, y0 = n * .24, n * .76, n * .40
        d.pieslice([x0, y0 - (x1 - x0) / 2, x1, y0 + (x1 - x0) / 2], 180, 360, fill=(8, 8, 10))
        d.rectangle([x0, y0, x1, n * .92], fill=(8, 8, 10))
        if lit:
            d.ellipse([n * .44, n * .62, n * .56, n * .80], fill=(240, 150, 60))                       # a fire far inside
            img = img.filter(ImageFilter.GaussianBlur(1.2))
    elif kind == "arcade":
        for k in range(3):
            cx = n * (.2 + .3 * k); w = n * .09
            d.pieslice([cx - w, n * .22 - w, cx + w, n * .22 + w], 180, 360, fill=shade(pal["stone"], .72))
            d.rectangle([cx - w, n * .22, cx + w, n * .92], fill=shade(pal["stone"], .72))
            d.pieslice([cx - w + 6, n * .22 - w + 6, cx + w - 6, n * .22 + w - 6], 180, 360, fill=shade(pal["stone"], .94))
            d.rectangle([cx - w + 6, n * .22, cx + w - 6, n * .92], fill=shade(pal["stone"], .94))
    return img


def build_sheet(key):
    pal = PALETTES[key]; seed = sum(map(ord, key)) * 7
    wall = plaster(pal["wall"], seed)
    stone = blocks(pal["stone"], 6, seed + 3)
    church = blocks(pal["stone"], 8, seed + 5, mortar=.78, jitter=.08)
    cliff = rock(shade(pal["stone"], .82), seed + 9)
    tiles = {
        WALL: wall,
        WIN_LIT: opening(wall, pal, "window", seed + 11, True), WIN_DARK: opening(wall, pal, "window", seed + 12, False),
        DOOR: opening(wall, pal, "door", seed + 13), BALC: opening(wall, pal, "balcony", seed + 14, True),
        BASE: blocks(pal["base"], 5, seed + 15), ROOF: roof_tiles(pal["roof"], seed + 16),
        STONE: stone, SLIT: opening(stone, pal, "slit", seed + 17),
        ARCH_LIT: opening(church, pal, "arch", seed + 18, True), ARCH_DARK: opening(church, pal, "arch", seed + 19, False),
        ROCK: cliff, CAVE: opening(cliff, pal, "cave", seed + 20, True),
        WOOD: planks(pal["wood"], seed + 21), ROOF2: roof_tiles(pal["roof2"], seed + 22, 9),
        CLAY: flat((168, 96, 58), seed + 23, .14), VINE: rock((58, 110, 50), seed + 24), SAIL: planks((226, 214, 186), seed + 25, 5),
        GOLD: flat((236, 190, 70), seed + 26, .16), BRICK: blocks((150, 84, 60), 10, seed + 27, mortar=.7, jitter=.12),
        WHITE: flat((236, 236, 232), seed + 28, .05), RED: flat((176, 48, 40), seed + 29, .07),
        CHURCH: opening(church, pal, "arcade", seed + 30), CHURCH_DOOR: opening(church, pal, "churchdoor", seed + 31),
    }
    sheet = Image.new("RGB", (SHEET, SHEET), pal["wall"])
    for index, img in tiles.items():
        sheet.paste(img.resize((TILE, TILE), Image.LANCZOS), ((index % 8) * TILE, (index // 8) * TILE))
    OUT_MODELS.mkdir(parents=True, exist_ok=True)
    sheet.save(OUT_MODELS / f"{key}BaseColor.png")


# =============================================================== geometry
def v3(*a): return np.array(a, np.float64)


class Mesh:
    """Flat-shaded triangles with one tile of the sheet on every face."""

    def __init__(self):
        self.pos, self.nrm, self.uv, self.idx = [], [], [], []

    def _uv(self, tile, u, v):
        c, r = tile % 8, tile // 8
        inset = 2.5
        return ((c * TILE + inset + u * (TILE - 2 * inset)) / SHEET, (r * TILE + inset + v * (TILE - 2 * inset)) / SHEET)

    def face(self, pts, uvs, tile, out=None):
        pts = [np.asarray(p, np.float64) for p in pts]
        n = np.cross(pts[1] - pts[0], pts[-1] - pts[0]); ln = np.linalg.norm(n)
        if ln < 1e-9: return
        n /= ln
        if out is not None and np.dot(n, out) < 0:
            pts = pts[::-1]; uvs = uvs[::-1]; n = -n
        base = len(self.pos)
        for p, (u, v) in zip(pts, uvs):
            self.pos.append(p); self.nrm.append(n); self.uv.append(self._uv(tile, u, v))
        for k in range(1, len(pts) - 1):
            self.idx += [base, base + k, base + k + 1]

    def quad(self, a, b, c, d, tile, out=None, u=(0, 1), v=(0, 1)):
        """a bottom left, b bottom right, c top right, d top left."""
        self.face([a, b, c, d], [(u[0], v[1]), (u[1], v[1]), (u[1], v[0]), (u[0], v[0])], tile, out)

    def tri(self, a, b, c, tile, out=None):
        self.face([a, b, c], [(0, 1), (1, 1), (.5, 0)], tile, out)

    def add(self, other, dx=0, dy=0, dz=0, yaw=0, scale=1):
        base = len(self.pos); ca, sa = math.cos(yaw), math.sin(yaw)
        for p, n in zip(other.pos, other.nrm):
            x, y, z = p * scale
            self.pos.append(v3(x * ca + z * sa + dx, y + dy, -x * sa + z * ca + dz))
            self.nrm.append(v3(n[0] * ca + n[2] * sa, n[1], -n[0] * sa + n[2] * ca))
        self.uv += other.uv; self.idx += [i + base for i in other.idx]
        return self

    # ---- parts
    def wall(self, p0, p1, y0, y1, cols, rows, pick, out):
        """Vertical wall from p0 to p1 (x, z) split into bays; pick(column, row, columns, rows) gives each bay's tile."""
        for i in range(cols):
            a = np.array(p0) + (np.array(p1) - np.array(p0)) * i / cols
            b = np.array(p0) + (np.array(p1) - np.array(p0)) * (i + 1) / cols
            for j in range(rows):
                ya, yb = y0 + (y1 - y0) * j / rows, y0 + (y1 - y0) * (j + 1) / rows
                self.quad(v3(a[0], ya, a[1]), v3(b[0], ya, b[1]), v3(b[0], yb, b[1]), v3(a[0], yb, a[1]), pick(i, j, cols, rows), out)

    def box(self, cx, cz, w, d, y0, y1, tile, top=None, cols=None, rows=1, front=None, side=None, bay=3.4):
        """Four walls and a lid. front / side are pick functions (default: every bay 'tile')."""
        x0, x1, z0, z1 = cx - w / 2, cx + w / 2, cz - d / 2, cz + d / 2
        plain = lambda i, j, c, r: tile
        cw = cols or max(1, round(w / bay)); cd = max(1, round(d / bay))
        self.wall((x0, z1), (x1, z1), y0, y1, cw, rows, front or plain, v3(0, 0, 1))
        self.wall((x1, z0), (x0, z0), y0, y1, cw, rows, side or plain, v3(0, 0, -1))
        self.wall((x1, z1), (x1, z0), y0, y1, cd, rows, side or plain, v3(1, 0, 0))
        self.wall((x0, z0), (x0, z1), y0, y1, cd, rows, side or plain, v3(-1, 0, 0))
        self.quad(v3(x0, y1, z1), v3(x1, y1, z1), v3(x1, y1, z0), v3(x0, y1, z0), tile if top is None else top, v3(0, 1, 0))

    def gable(self, cx, cz, w, d, y, h, tile, end_tile, along="x", over=.5):
        """Pitched roof with its ridge along x or z; the two end triangles take 'end_tile'."""
        if along == "z":
            m = Mesh(); m.gable(0, 0, d, w, 0, h, tile, end_tile, "x", over); self.add(m, cx, y, cz, math.pi / 2); return
        x0, x1, z0, z1 = cx - w / 2 - over, cx + w / 2 + over, cz - d / 2 - over, cz + d / 2 + over
        cols = max(1, round(w / 3.2))
        for i in range(cols):
            a, b = x0 + (x1 - x0) * i / cols, x0 + (x1 - x0) * (i + 1) / cols
            self.quad(v3(a, y, z1), v3(b, y, z1), v3(b, y + h, cz), v3(a, y + h, cz), tile, v3(0, 1, 1))
            self.quad(v3(b, y, z0), v3(a, y, z0), v3(a, y + h, cz), v3(b, y + h, cz), tile, v3(0, 1, -1))
        self.tri(v3(cx + w / 2, y, cz + d / 2), v3(cx + w / 2, y, cz - d / 2), v3(cx + w / 2, y + h, cz), end_tile, v3(1, 0, 0))
        self.tri(v3(cx - w / 2, y, cz - d / 2), v3(cx - w / 2, y, cz + d / 2), v3(cx - w / 2, y + h, cz), end_tile, v3(-1, 0, 0))

    def hip(self, cx, cz, w, d, y, h, tile, over=.5):
        x0, x1, z0, z1 = cx - w / 2 - over, cx + w / 2 + over, cz - d / 2 - over, cz + d / 2 + over
        r = max(0.0, (w - d) / 2)                                  # half length of the ridge
        a, b = v3(cx - r, y + h, cz), v3(cx + r, y + h, cz)
        self.quad(v3(x0, y, z1), v3(x1, y, z1), b, a, tile, v3(0, 1, 1))
        self.quad(v3(x1, y, z0), v3(x0, y, z0), a, b, tile, v3(0, 1, -1))
        self.tri(v3(x1, y, z1), v3(x1, y, z0), b, tile, v3(1, 1, 0))
        self.tri(v3(x0, y, z0), v3(x0, y, z1), a, tile, v3(-1, 1, 0))

    def round(self, cx, cz, r0, r1, y0, y1, seg, rows, pick):
        """Round wall, r0 at the foot and r1 at the top."""
        for i in range(seg):
            a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            for j in range(rows):
                t0, t1 = j / rows, (j + 1) / rows
                ra, rb = r0 + (r1 - r0) * t0, r0 + (r1 - r0) * t1
                ya, yb = y0 + (y1 - y0) * t0, y0 + (y1 - y0) * t1
                out = v3(math.sin((a0 + a1) / 2), 0, math.cos((a0 + a1) / 2))
                self.quad(v3(cx + math.sin(a0) * ra, ya, cz + math.cos(a0) * ra), v3(cx + math.sin(a1) * ra, ya, cz + math.cos(a1) * ra),
                          v3(cx + math.sin(a1) * rb, yb, cz + math.cos(a1) * rb), v3(cx + math.sin(a0) * rb, yb, cz + math.cos(a0) * rb),
                          pick(i, j, seg, rows), out)

    def cone(self, cx, cz, r, y, h, seg, tile):
        for i in range(seg):
            a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            out = v3(math.sin((a0 + a1) / 2), .6, math.cos((a0 + a1) / 2))
            self.tri(v3(cx + math.sin(a0) * r, y, cz + math.cos(a0) * r), v3(cx + math.sin(a1) * r, y, cz + math.cos(a1) * r), v3(cx, y + h, cz), tile, out)

    def dome(self, cx, cz, r, y, seg, rings, tile, squash=1.0):
        for j in range(rings):
            p0, p1 = math.pi / 2 * j / rings, math.pi / 2 * (j + 1) / rings
            for i in range(seg):
                a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
                def pt(a, p): return v3(cx + math.sin(a) * math.cos(p) * r, y + math.sin(p) * r * squash, cz + math.cos(a) * math.cos(p) * r)
                out = pt((a0 + a1) / 2, (p0 + p1) / 2) - v3(cx, y - r * .2, cz)
                if j == rings - 1: self.tri(pt(a0, p0), pt(a1, p0), pt(a0, p1), tile, out)
                else: self.quad(pt(a0, p0), pt(a1, p0), pt(a1, p1), pt(a0, p1), tile, out)

    def merlons(self, p0, p1, y, tile, size=1.0, height=1.1, thick=.6):
        """Battlements along the top of a wall from p0 to p1 (x, z)."""
        p0, p1 = np.array(p0, np.float64), np.array(p1, np.float64)
        length = np.linalg.norm(p1 - p0); count = max(1, int(length / (size * 2)))
        yaw = math.atan2(p1[0] - p0[0], p1[1] - p0[1])
        for k in range(count):
            c = p0 + (p1 - p0) * (k + .5) / count
            m = Mesh(); m.box(0, 0, thick, size, 0, height, tile)
            self.add(m, c[0], y, c[1], yaw)

    def bounds(self):
        p = np.array(self.pos); return p.min(0), p.max(0)


# ---------------------------------------------------------------- buildings (origin: centre of the footprint, y = 0 at the street)
def house(rng, w, d, floors, roof="gable", balcony=True, storey=3.2, wall=WALL, roof_tile=ROOF, lit=.55, ridge="x"):
    m = Mesh()
    m.box(0, 0, w + .5, d + .5, -FOUNDATION, .35, BASE)
    cols = max(2, round(w / 3.2)); door = rng.randrange(cols)
    def front(i, j, c, r):
        if j == 0: return DOOR if i == door else (WIN_LIT if rng.random() < lit else WIN_DARK)
        if balcony: return BALC
        return WIN_LIT if rng.random() < lit else WIN_DARK
    def side(i, j, c, r):
        return wall if rng.random() < .45 else (WIN_LIT if rng.random() < lit * .7 else WIN_DARK)
    top = .35 + floors * storey
    m.box(0, 0, w, d, .35, top, wall, cols=cols, rows=floors, front=front, side=side)
    if balcony:
        for f in range(1, floors):                                     # a wooden gallery on every upper floor
            y = .35 + f * storey
            m.box(0, d / 2 + .55, w + .3, 1.1, y - .14, y, WOOD)
            m.wall((-w / 2 - .15, d / 2 + 1.1), (w / 2 + .15, d / 2 + 1.1), y, y + .95, cols, 1, lambda *a: WOOD, v3(0, 0, 1))
            for i in range(cols + 1):
                x = -w / 2 + w * i / cols
                m.box(x, d / 2 + 1.05, .16, .16, y, min(top, y + storey), WOOD)
    if roof == "gable": m.gable(0, 0, w, d, top, min(w, d) * .42, roof_tile, wall, ridge, over=1.3 if balcony else .5)
    elif roof == "hip": m.hip(0, 0, w, d, top, min(w, d) * .36, roof_tile, over=1.3 if balcony else .6)
    else:
        m.box(0, 0, w + .4, d + .4, top, top + .5, wall, top=BASE)
    return m


def svan_tower(rng, height=24.0, width=5.2):
    m = Mesh()
    m.box(0, 0, width + .6, width + .6, -FOUNDATION, .2, BASE)
    top = width * .8; rows = 6
    for sx, sz in ((0, 1), (1, 0), (0, -1), (-1, 0)):                  # four leaning walls
        for j in range(rows):
            t0, t1 = j / rows, (j + 1) / rows
            h0, h1 = (width + (top - width) * t0) / 2, (width + (top - width) * t1) / 2
            y0, y1 = .2 + (height - .2) * t0, .2 + (height - .2) * t1
            if sz: pts = [v3(-h0 * sz, y0, h0 * sz), v3(h0 * sz, y0, h0 * sz), v3(h1 * sz, y1, h1 * sz), v3(-h1 * sz, y1, h1 * sz)]
            else: pts = [v3(h0 * sx, y0, h0 * sx), v3(h0 * sx, y0, -h0 * sx), v3(h1 * sx, y1, -h1 * sx), v3(h1 * sx, y1, h1 * sx)]
            m.quad(*pts, SLIT if j in (2, 4) and rng.random() < .7 else STONE, v3(sx, 0, sz))
    crown = top + 1.0                                                   # the wider fighting floor under the roof
    m.box(0, 0, crown, crown, height, height + 2.4, STONE, rows=1, front=lambda *a: SLIT, side=lambda *a: SLIT, cols=2)
    m.gable(0, 0, crown, crown, height + 2.4, 1.5, ROOF, STONE, over=.3)
    return m


def church(rng, scale=1.0, enclosure=False):
    """Georgian cross-dome church: a long nave, a transept, a drum and a pointed dome. The west door faces +z."""
    m = Mesh(); s = scale
    nave_w, nave_d, nave_h = 11 * s, 26 * s, 13 * s
    arm_w, arm_d = 22 * s, 9 * s
    m.box(0, 0, arm_w + 1, nave_d + 1, -FOUNDATION * 2, .5, BASE)
    def facade(i, j, c, r): return CHURCH_DOOR if j == 0 and i == c // 2 else (ARCH_LIT if j == r - 1 and i == c // 2 else CHURCH)
    def flank(i, j, c, r): return ARCH_LIT if j == 1 and i % 2 == 0 else CHURCH
    m.box(0, 0, nave_w, nave_d, .5, nave_h, CHURCH, cols=3, rows=3, front=facade, side=flank, bay=4.4 * s)
    m.gable(0, 0, nave_w, nave_d, nave_h, 4.6 * s, ROOF2, CHURCH, "z", over=.4)
    m.box(0, 0, arm_w, arm_d, .5, nave_h, CHURCH, rows=3, front=flank, side=flank, bay=4.4 * s)
    m.gable(0, 0, arm_w, arm_d, nave_h, 4.2 * s, ROOF2, ARCH_DARK, "x", over=.4)
    drum_r, drum_y = 4.3 * s, nave_h + 2.2 * s
    m.round(0, 0, drum_r, drum_r, drum_y - 2.2 * s, drum_y + 7.5 * s, 12, 1, lambda i, j, c, r: ARCH_LIT if i % 2 == 0 else ARCH_DARK)
    m.cone(0, 0, drum_r + .5 * s, drum_y + 7.5 * s, 8.5 * s, 12, ROOF2)
    m.box(0, 0, .35 * s, .35 * s, drum_y + 16 * s, drum_y + 18.6 * s, GOLD)                         # the cross
    m.box(0, 0, 1.5 * s, .3 * s, drum_y + 17.3 * s, drum_y + 17.7 * s, GOLD)
    if enclosure:                                                                                    # the cathedral's own wall
        half = 24 * s
        for (a, b) in (((-half, half), (half, half)), ((half, half), (half, -half)), ((half, -half), (-half, -half)), ((-half, -half), (-half, half))):
            wall = Mesh(); length = 2 * half
            wall.box(0, 0, length, 1.2, -FOUNDATION * 2, 5.5, STONE, bay=4)
            yaw = math.atan2(b[0] - a[0], b[1] - a[1]) - math.pi / 2
            if a == (-half, half):                                                                    # the gate is in the west wall
                wall = Mesh()
                for cx in (-half / 2 - 2.5, half / 2 + 2.5): wall.box(cx, 0, half - 5, 1.2, -FOUNDATION * 2, 5.5, STONE, bay=4)
            m.add(wall, (a[0] + b[0]) / 2, 0, (a[1] + b[1]) / 2, yaw)
            m.merlons(a, b, 5.5, STONE)
            t = Mesh(); t.round(0, 0, 2.2, 2.0, -FOUNDATION * 2, 8, 8, 2, lambda *q: STONE); t.cone(0, 0, 2.5, 8, 3, 8, ROOF2)
            m.add(t, a[0], 0, a[1])
    return m


def rock_mound(rng, w, d, h, layers=4, cave=0.0):
    """A rock hill: stacked, shrinking, slightly turned slabs. 'cave' is the share of bays that are cave mouths."""
    m = Mesh()
    for k in range(layers):
        t0, t1 = k / layers, (k + 1) / layers
        seg = 10
        def ring(t, y):
            pts = []
            for i in range(seg):
                a = 2 * math.pi * i / seg + k * .31
                rr = 1 - .62 * t + .10 * math.sin(i * 2.3 + k)
                pts.append(v3(math.sin(a) * w / 2 * rr, y, math.cos(a) * d / 2 * rr))
            return pts
        y0 = -FOUNDATION * 3 if k == 0 else h * t0
        a, b = ring(t0, y0), ring(t1 * .96, h * t1)
        for i in range(seg):
            j = (i + 1) % seg
            out = (a[i] + a[j]) / 2; out[1] = 0
            m.quad(a[i], a[j], b[j], b[i], CAVE if rng.random() < cave and k < layers - 1 else ROCK, out)
        for i in range(1, seg - 1): m.face([b[0], b[i], b[i + 1]], [(0, 0), (1, 0), (1, 1)], ROCK, v3(0, 1, 0))
    return m


def fortress(rng, length=46.0, towers=3, wall_h=9.0, tower_h=14.0, round_towers=True, on_rock=9.0):
    """A stretch of fortress wall with towers on a rock ridge. The wall runs along x and faces +z."""
    m = Mesh()
    if on_rock > 0:
        m.add(rock_mound(rng, length * 1.15, 22, on_rock, 3), 0, -on_rock, 0)
    m.box(0, 0, length, 2.2, -1, wall_h, STONE, bay=4.2, rows=2, front=lambda i, j, c, r: SLIT if j == 1 and i % 2 else STONE)
    m.merlons((-length / 2, 1.0), (length / 2, 1.0), wall_h, STONE)
    for k in range(towers):
        x = -length / 2 + length * k / (towers - 1)
        t = Mesh()
        if round_towers:
            t.round(0, 0, 3.6, 3.2, -1, tower_h, 10, 3, lambda i, j, c, r: SLIT if j == 2 and i % 3 == 0 else STONE)
            t.cone(0, 0, 3.9, tower_h, 4.2, 10, ROOF2)
        else:
            t.box(0, 0, 6, 6, -1, tower_h, STONE, rows=3, front=lambda i, j, c, r: SLIT if j == 2 else STONE, side=lambda i, j, c, r: SLIT if j == 2 else STONE, cols=2)
            for (a, b) in (((-3, 3), (3, 3)), ((3, 3), (3, -3)), ((3, -3), (-3, -3)), ((-3, -3), (-3, 3))): t.merlons(a, b, tower_h, STONE, .8)
        m.add(t, x, 0, 0)
    return m


def gate(rng, style):
    """City gate across the road: two towers and an arch over a 15 m opening."""
    m = Mesh(); half = 7.6; h = 11.0
    for side in (-1, 1):
        t = Mesh()
        if style == "round":
            t.round(0, 0, 2.6, 2.3, -FOUNDATION, h + 2, 10, 3, lambda i, j, c, r: SLIT if j == 2 and i % 3 == 0 else STONE); t.cone(0, 0, 2.9, h + 2, 3.6, 10, ROOF2)
        elif style == "svan":
            t.add(svan_tower(rng, 17, 4.2))
        else:
            t.box(0, 0, 4.4, 4.4, -FOUNDATION, h + 2, STONE, rows=3, cols=1, front=lambda i, j, c, r: SLIT if j == 2 else STONE, side=lambda *a: STONE)
            if style == "roofed": t.hip(0, 0, 4.4, 4.4, h + 2, 2.6, ROOF, over=.6)
            else:
                for (a, b) in (((-2.2, 2.2), (2.2, 2.2)), ((2.2, 2.2), (2.2, -2.2)), ((2.2, -2.2), (-2.2, -2.2)), ((-2.2, -2.2), (-2.2, 2.2))): t.merlons(a, b, h + 2, STONE, .7)
        m.add(t, side * (half + 2.2), 0, 0)
    m.box(0, 0, 2 * half, 2.0, h - 3.2, h, STONE, bay=3.8, front=lambda i, j, c, r: ARCH_LIT if i == c // 2 else STONE)
    m.merlons((-half, 0), (half, 0), h, STONE, .8)
    return m


def jar(rng, height=2.6):
    """Qvevri: the clay jar wine is made in."""
    m = Mesh(); profile = [(0.0, .12), (.18, .55), (.45, .98), (.70, .86), (.88, .46), (1.0, .50)]
    for (t0, r0), (t1, r1) in zip(profile, profile[1:]):
        m.round(0, 0, r0 * height * .42, r1 * height * .42, t0 * height, t1 * height, 10, 1, lambda *a: CLAY)
    return m


def pergola(rng, w=7.0, d=5.0, h=3.0):
    m = Mesh()
    for x in (-w / 2, w / 2):
        for z in (-d / 2, d / 2): m.box(x, z, .28, .28, -1.5, h, WOOD)
    m.box(0, 0, w + 1, d + 1, h, h + .45, VINE)
    for k in range(3): m.add(jar(rng, 1.7), -w / 3 + k * w / 3, 0, 0)
    return m


def baths(rng):
    """The brick domes of the warm sulphur baths."""
    m = Mesh()
    m.box(0, 0, 22, 12, -FOUNDATION, 1.6, BRICK, bay=3.6, front=lambda i, j, c, r: DOOR if i == c // 2 else BRICK)
    for k, (x, z, r) in enumerate(((-7, 0, 4.2), (0, 0, 5.0), (7, 0, 4.2), (-3.5, -3.5, 2.4), (3.5, -3.5, 2.4))):
        m.round(x, z, r, r, 1.6, 2.6, 12, 1, lambda *a: BRICK)
        m.dome(x, z, r, 2.6, 12, 4, BRICK, .82)
        m.round(x, z, .7, .7, 2.6 + r * .82 - .2, 2.6 + r * .82 + 1.0, 8, 1, lambda *a: ARCH_LIT)   # the light well on top
        m.cone(x, z, .9, 2.6 + r * .82 + 1.0, .8, 8, BRICK)
    return m


def cave_town(rng):
    """Uplistsikhe: halls cut into a rock ridge, with fires far inside."""
    m = rock_mound(rng, 64, 30, 26, 5, cave=.5)
    m.box(-6, 6, 9, 7, 26 * .55, 26 * .55 + 5, STONE, rows=1, front=lambda i, j, c, r: ARCH_LIT)   # the small church on the rock
    m.gable(-6, 6, 9, 7, 26 * .55 + 5, 2.6, ROOF2, STONE)
    return m


def temple(rng):
    """Colchis: a columned hall with the Golden Fleece hung in it."""
    m = Mesh(); w, d, h = 20.0, 12.0, 9.0
    m.box(0, 0, w + 3, d + 3, -FOUNDATION * 2, .6, BASE)
    m.box(0, 0, w + 1.6, d + 1.6, .6, 1.2, STONE)
    for i in range(6):
        for z in (-d / 2, d / 2):
            x = -w / 2 + w * i / 5
            m.round(x, z, .78, .66, 1.2, h, 10, 1, lambda *a: WHITE); m.box(x, z, 1.8, 1.8, h, h + .5, STONE)
    for z in (-d / 2, d / 2):
        m.round(-w / 2, 0, .78, .66, 1.2, h, 10, 1, lambda *a: WHITE) if z < 0 else m.round(w / 2, 0, .78, .66, 1.2, h, 10, 1, lambda *a: WHITE)
    m.box(0, 0, w + 2.2, d + 2.2, h + .5, h + 1.8, STONE, bay=3.6)
    m.gable(0, 0, w + 2.2, d + 2.2, h + 1.8, 3.4, ROOF2, CHURCH, over=.3)
    m.box(0, 0, 3.4, .5, 3.4, 7.2, GOLD)                                                              # the fleece
    m.box(0, 0, 4.6, .45, 6.6, 7.4, GOLD)
    m.box(0, 0, .5, .5, 1.2, 8.4, WOOD)
    return m


def lighthouse(rng, height=30.0):
    m = Mesh()
    m.round(0, 0, 5.2, 4.6, -FOUNDATION * 2, 1.5, 12, 1, lambda *a: BASE)
    bands = 6
    for k in range(bands):
        t0, t1 = k / bands, (k + 1) / bands
        r0, r1 = 3.6 - 1.3 * t0, 3.6 - 1.3 * t1
        m.round(0, 0, r0, r1, 1.5 + (height - 1.5) * t0, 1.5 + (height - 1.5) * t1, 12, 1, lambda i, j, c, r, k=k: RED if k % 2 else WHITE)
    m.round(0, 0, 3.2, 3.2, height, height + .5, 12, 1, lambda *a: STONE)                             # gallery
    m.round(0, 0, 1.9, 1.9, height + .5, height + 3.4, 10, 1, lambda *a: WIN_LIT)                      # the lamp
    m.cone(0, 0, 2.4, height + 3.4, 2.4, 10, ROOF2)
    return m


def boat(rng, length=11.0):
    m = Mesh(); w = length * .26; h = 1.5
    bow, stern = v3(0, h, length / 2), v3(0, h, -length / 2)
    keel_f, keel_b = v3(0, -.5, length * .34), v3(0, -.5, -length * .34)
    for side in (-1, 1):
        a, b = v3(side * w, h, length * .14), v3(side * w, h, -length * .22)
        out = v3(side, -.3, 0)
        m.face([keel_f, bow, a], [(0, 1), (0, 0), (1, 0)], WOOD, out); m.face([keel_f, a, b, keel_b], [(0, 1), (0, 0), (1, 0), (1, 1)], WOOD, out)
        m.face([keel_b, b, stern], [(0, 1), (0, 0), (1, 0)], WOOD, out)
    m.face([bow, v3(w, h, length * .14), v3(w, h, -length * .22), stern, v3(-w, h, -length * .22), v3(-w, h, length * .14)],
           [(0, 0), (1, 0), (1, 1), (0, 1), (0, 1), (0, 0)], WOOD, v3(0, 1, 0))
    m.box(0, length * .06, .26, .26, h, h + length * .9, WOOD)
    m.face([v3(0, h + length * .86, length * .08), v3(0, h + 1.2, length * .08), v3(0, h + 1.6, -length * .40)], [(0, 0), (0, 1), (1, 1)], SAIL, v3(1, 0, 0))
    m.face([v3(0.02, h + length * .80, length * .12), v3(0.02, h + 1.4, length * .12), v3(0.02, h + 1.5, length * .46)], [(0, 0), (0, 1), (1, 1)], SAIL, v3(1, 0, 0))
    return m


def pier(rng, length=26.0):
    m = Mesh()
    m.box(0, 0, 4.2, length, 0, .4, WOOD, bay=3)
    for z in np.arange(-length / 2 + 1, length / 2, 4.0):
        for x in (-1.8, 1.8): m.box(x, z, .4, .4, -6, 1.2, WOOD)
    return m


def machubi(rng):
    """Svan house: squat, stone, slate roof, with its own tower at the back."""
    m = house(rng, 9.5, 8.5, 2, "gable", balcony=False, storey=2.9, wall=STONE, lit=.4)
    m.add(svan_tower(rng, 19, 4.4), 3.2, 0, -6.0)
    return m


def jvari(rng):
    """A small church on a high rock, seen from the whole valley."""
    m = rock_mound(rng, 30, 26, 22, 4)
    m.add(church(rng, .5), 0, 22 * .96, 0)
    return m


def hill_fort(rng):
    """Gori's fortress: an oval wall around the top of a rock."""
    m = rock_mound(rng, 46, 34, 18, 4)
    ring = 14
    for i in range(ring):
        a0, a1 = 2 * math.pi * i / ring, 2 * math.pi * (i + 1) / ring
        p0 = (math.sin(a0) * 13, math.cos(a0) * 9.5); p1 = (math.sin(a1) * 13, math.cos(a1) * 9.5)
        mid = v3((p0[0] + p1[0]) / 2, 0, (p0[1] + p1[1]) / 2)
        seg = Mesh(); seg.wall(p0, p1, 16.5, 23, 1, 1, lambda *q: SLIT if i % 3 == 0 else STONE, mid)
        m.add(seg); m.merlons(p0, p1, 23, STONE, .8, .9, .5)
    t = Mesh(); t.box(0, 0, 5, 5, 16, 27, STONE, rows=2, cols=1, front=lambda i, j, c, r: SLIT, side=lambda i, j, c, r: SLIT); t.hip(0, 0, 5, 5, 27, 2.4, ROOF2)
    m.add(t, 0, 0, 0)
    return m


# model name -> (builder, seed). Every city has two houses, a gate and its own landmarks.
def models_of(key):
    r = lambda s: random.Random(s + sum(map(ord, key)))
    common = {
        "Telavi":   dict(HouseA=lambda: house(r(1), 11, 8, 2, "hip", True), HouseB=lambda: house(r(2), 14, 7.5, 1, "gable", False, storey=3.6),
                         Gate=lambda: gate(r(3), "roofed"), Cathedral=lambda: church(r(4), 1.15), Fortress=lambda: fortress(r(5), 40, 3, 8, 12, True, 0),
                         Pergola=lambda: pergola(r(6)), Jar=lambda: jar(r(7), 1.7)),
        "Tbilisi":  dict(HouseA=lambda: house(r(1), 10, 8, 3, "hip", True), HouseB=lambda: house(r(2), 13, 8, 2, "gable", True),
                         Gate=lambda: gate(r(3), "square"), Fortress=lambda: fortress(r(4), 58, 4, 10, 16, False, 14), Baths=lambda: baths(r(5)),
                         Church=lambda: church(r(6), .7)),
        "Mtskheta": dict(HouseA=lambda: house(r(1), 10, 8, 2, "gable", False, wall=STONE), HouseB=lambda: house(r(2), 12, 7.5, 1, "hip", False, storey=3.5),
                         Gate=lambda: gate(r(3), "round"), Cathedral=lambda: church(r(4), 1.45, enclosure=True), Jvari=lambda: jvari(r(5))),
        "Gori":     dict(HouseA=lambda: house(r(1), 10, 8, 2, "flat", False), HouseB=lambda: house(r(2), 12, 8, 1, "flat", False, storey=3.6),
                         Gate=lambda: gate(r(3), "square"), Fortress=lambda: hill_fort(r(4)), Caves=lambda: cave_town(r(5))),
        "Kutaisi":  dict(HouseA=lambda: house(r(1), 11, 8, 2, "hip", True), HouseB=lambda: house(r(2), 13, 8, 2, "gable", False),
                         Gate=lambda: gate(r(3), "roofed"), Cathedral=lambda: church(r(4), 1.3), Temple=lambda: temple(r(5))),
        "Batumi":   dict(HouseA=lambda: house(r(1), 10, 8, 3, "hip", True), HouseB=lambda: house(r(2), 12, 8, 2, "flat", True),
                         Gate=lambda: gate(r(3), "round"), Lighthouse=lambda: lighthouse(r(4)), Boat=lambda: boat(r(5)), Pier=lambda: pier(r(6))),
        "Mestia":   dict(HouseA=lambda: machubi(r(1)), HouseB=lambda: house(r(2), 9, 8, 1, "gable", False, storey=3.2, wall=STONE, lit=.4),
                         Gate=lambda: gate(r(3), "svan"), Tower=lambda: svan_tower(r(4), 26, 5.4), TowerB=lambda: svan_tower(r(5), 21, 4.8),
                         Church=lambda: church(r(6), .55)),
    }
    return common[key]


# =============================================================== files
def write_glb(path, name, mesh):
    pos = np.asarray(mesh.pos, np.float32); nrm = np.asarray(mesh.nrm, np.float32); uv = np.asarray(mesh.uv, np.float32); idx = np.asarray(mesh.idx, np.uint32)
    blobs = [pos.tobytes(), nrm.tobytes(), uv.tobytes(), idx.tobytes()]
    views, off = [], 0
    for b in blobs:
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(b)}); off += len(b) + (-len(b) % 4)
    binary = b"".join(b + b"\0" * (-len(b) % 4) for b in blobs)
    doc = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": "Doorways_" + name, "mesh": 0}],
           "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}],
           "materials": [{"name": name, "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0, "roughnessFactor": .9}}],
           "buffers": [{"byteLength": len(binary)}], "bufferViews": views,
           "accessors": [{"bufferView": 0, "componentType": 5126, "count": len(pos), "type": "VEC3", "min": pos.min(0).tolist(), "max": pos.max(0).tolist()},
                         {"bufferView": 1, "componentType": 5126, "count": len(nrm), "type": "VEC3"},
                         {"bufferView": 2, "componentType": 5126, "count": len(uv), "type": "VEC2"},
                         {"bufferView": 3, "componentType": 5125, "count": len(idx), "type": "SCALAR"}]}
    body = json.dumps(doc, separators=(",", ":")).encode(); body += b" " * (-len(body) % 4)
    path.write_bytes(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(body) + 8 + len(binary)) + struct.pack("<I4s", len(body), b"JSON") + body +
                     struct.pack("<I4s", len(binary), b"BIN\0") + binary)


def smooth(t):
    t = max(0.0, min(1.0, t)); return t * t * (3 - 2 * t)


def main():
    rows = [l.split() for l in open(ROAD, encoding="utf-8")]
    P = [(float(r[1]), float(r[2]), float(r[3]), float(r[4]), int(r[5])) for r in rows if r and r[0] == "P"]
    arenas = [(float(r[2]), float(r[4])) for r in rows if r and r[0] == "A"]
    n = len(P)

    def frame(s):
        """Road centre, tangent, normal, half width and region at 's' metres along the road."""
        i = max(1, min(n - 2, int(round(s / STEP))))
        tx, tz = P[i + 1][0] - P[i - 1][0], P[i + 1][2] - P[i - 1][2]
        ln = math.hypot(tx, tz); tx, tz = tx / ln, tz / ln
        return P[i], (tx, tz), (tz, -tx), P[i][3], P[i][4]

    def ground(p, stage, out):
        """Height of the terrain 'out' metres beyond the road's edge (build_world.py bank rule, without its noise)."""
        y = p[1] + BANK[stage] * smooth((out - 1.0) / 30.0)
        return max(y, WATER[stage] - .4) if stage in WATER else y

    work = pathlib.Path(tempfile.mkdtemp(prefix="ce_cities_"))
    shutil.rmtree(OUT_MODELS, ignore_errors=True); OUT_MODELS.mkdir(parents=True)
    lines = ["# The seven cities on the road to Amirani: written by art/cities/build_cities.py. Engine coordinates, metres.",
             "# CITY index key name s0 s1 x y z | M index model texture | B model x y z yaw height | L x y z r g b size"]
    model_index, sizes, total_buildings = {}, {}, 0
    for ci, (key, name, centre, length, known) in enumerate(CITIES):
        s0, s1 = centre - length / 2, centre + length / 2
        for a, radius in arenas:
            if a + radius + 6 > s0 and a - radius - 6 < s1: raise SystemExit(f"{name} overlaps the arena at {a} m")
        build_sheet(key)
        for model, make in models_of(key).items():
            mesh = make(); full = f"{key}{model}"
            glb = work / f"{full}.glb"; write_glb(glb, full, mesh)
            out = work / f"{full}_out"
            result = subprocess.run([BUILDER, "static-pipeline", str(glb), str(out)], capture_output=True, text=True)
            made = list(out.rglob("*.dmesh"))
            if not made: raise SystemExit(f"asset builder failed for {full}: {result.stdout[-400:]}")
            shutil.copy(made[0], OUT_MODELS / f"{full}.dmesh")
            lo, hi = mesh.bounds()
            model_index[full] = len(model_index); sizes[full] = (hi - lo, lo, hi)
            lines.append(f"M {model_index[full]} Cities/{full} Cities/{key}")
        mid, _, _, _, _ = frame(centre)
        lines.append(f"CITY {ci} {key} {name} {s0:.1f} {s1:.1f} {mid[0]:.2f} {mid[1]:.2f} {mid[2]:.2f}")
        rng = random.Random(1000 + ci)

        def put(model, s, side, out, face=None, lift=0.0, turn=0.0):
            """Place a model 'out' metres beyond the road's edge on 'side' (+1 / -1; 0 = on the road), facing the road."""
            nonlocal total_buildings
            full = f"{key}{model}"; size, lo, hi = sizes[full]
            p, t, nrm, half, stage = frame(s)
            if side == 0:
                x, z, y = p[0], p[2], p[1]; fx, fz = t
            else:
                lateral = half + out
                x, z = p[0] + nrm[0] * side * lateral, p[2] + nrm[1] * side * lateral
                y = ground(p, stage, out); fx, fz = -nrm[0] * side, -nrm[1] * side
            if face == "along": fx, fz = t
            yaw = math.atan2(fx, fz) + turn
            # the game stands a model on the middle of its bounding box: move that middle to where the model's origin belongs
            mx, mz = (lo[0] + hi[0]) / 2, (lo[2] + hi[2]) / 2
            x += mx * math.cos(yaw) + mz * math.sin(yaw); z += -mx * math.sin(yaw) + mz * math.cos(yaw)
            lines.append(f"B {model_index[full]} {x:.2f} {y + lo[1] + lift:.2f} {z:.2f} {yaw:.4f} {size[1]:.2f}")
            total_buildings += 1
            if side != 0 and size[1] > 0.8:
                # What she walks round: the footprint as a row of circles along its longer side ("O x z radius").
                # They never reach onto the street: a radius is at most the distance to the road's edge.
                wide, deep = float(size[0]), float(size[2])
                short, long_ = min(wide, deep), max(wide, deep)
                radius = max(0.3, min(short / 2, out + 0.4))
                count = max(1, int(math.ceil(long_ / max(short, 0.5))))
                ax = (math.cos(yaw), -math.sin(yaw)) if wide >= deep else (math.sin(yaw), math.cos(yaw))
                for k in range(count):
                    off = (k - (count - 1) / 2) * ((long_ - short) / max(1, count - 1))
                    lines.append(f"O {x + ax[0] * off:.2f} {z + ax[1] * off:.2f} {radius:.2f}")

        def light(s, side, out, up, color=(1.0, .72, .38), size=1.5):
            p, t, nrm, half, stage = frame(s)
            lateral = half + out
            lines.append(f"L {p[0] + nrm[0] * side * lateral:.2f} {ground(p, stage, out) + up:.2f} {p[2] + nrm[1] * side * lateral:.2f} {color[0]:.2f} {color[1]:.2f} {color[2]:.2f} {size:.2f}")

        put("Gate", s0 + 1, 0, 0); put("Gate", s1 - 1, 0, 0)
        # the main street: a row of houses on each side, and a looser second row behind them
        for side in (-1, 1):
            s = s0 + 9
            while s < s1 - 14:
                model = "HouseA" if rng.random() < .55 else "HouseB"
                size = sizes[f"{key}{model}"][0]
                put(model, s + size[0] / 2, side, STREET_GAP + size[2] / 2)
                light(s + size[0] / 2, side, .9, 3.4)
                if rng.random() < .62:
                    back = "HouseB" if model == "HouseA" else "HouseA"
                    bs = sizes[f"{key}{back}"][0]
                    put(back, s + size[0] / 2 + rng.uniform(-2, 2), side, STREET_GAP + size[2] + 5.5 + bs[2] / 2, turn=rng.uniform(-.25, .25))
                s += size[0] + rng.uniform(1.2, 3.4)
        # what the city is known for
        if known == "wine":
            put("Cathedral", centre + 6, 1, 54); put("Fortress", centre - 10, -1, 46)
            for k in range(5): put("Pergola", s0 + 16 + k * 18, -1 if k % 2 else 1, 30 + (k % 3) * 5, turn=k * .4)
            for k in range(8): put("Jar", s0 + 12 + k * 11, 1 if k % 2 else -1, .9)
        elif known == "springs":
            put("Fortress", centre, 1, 62); put("Baths", centre - 14, -1, 34); put("Church", centre + 30, -1, 44); put("Church", centre - 36, 1, 40)
            for k in range(6): light(centre - 24 + k * 4, -1, 34, 6, (1.0, .80, .50), 1.2)
        elif known == "cathedral":
            put("Cathedral", centre, 1, 64); put("Jvari", centre + 8, -1, 84)
        elif known == "rock":
            put("Caves", centre - 6, 1, 52); put("Fortress", centre + 10, -1, 60)
            for k in range(7): light(centre - 30 + k * 9, 1, 30, 9 + (k % 3) * 4, (1.0, .55, .22), 1.6)
        elif known == "fleece":
            put("Cathedral", centre - 12, -1, 58); put("Temple", centre + 8, 1, 40)
            for k in range(4): light(centre + 2 + k * 4, 1, 34, 5, (1.0, .85, .40), 1.8)
        elif known == "sea":
            put("Lighthouse", centre + 30, 1, 46); light(centre + 30, 1, 46, 33, (1.0, .92, .62), 5.0)
            for k in range(3): put("Pier", centre - 40 + k * 34, 1, 34, lift=.9)
            for k in range(7): put("Boat", centre - 52 + k * 17 + rng.uniform(-3, 3), 1 if k % 3 else -1, 44 + rng.uniform(0, 26), lift=-.3, turn=rng.uniform(0, 6.28))
        elif known == "towers":
            for k in range(9):
                put("Tower" if k % 2 else "TowerB", s0 + 6 + k * 11 + rng.uniform(-2, 2), -1 if k % 2 else 1, 26 + rng.uniform(0, 24), turn=rng.uniform(-.2, .2))
            put("Church", centre, -1, 58)
        print(f"{name:9s} {s0:6.0f}..{s1:6.0f} m  region {frame(centre)[4] + 1}")
    OUT_DATA.write_text("\n".join(lines) + "\n", encoding="utf-8")
    shutil.rmtree(work, ignore_errors=True)
    print(f"{len(model_index)} models, {total_buildings} buildings -> {OUT_DATA}")


if __name__ == "__main__":
    main()
