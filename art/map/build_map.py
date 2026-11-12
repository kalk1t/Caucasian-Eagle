# Paints the world map of Caucasian Eagle (the M key) from the road and the cities.
#
# Reads   game/assets/Compiled/World/Road.txt, Cities.txt
# Writes  game/assets/ui/Map/WorldMap.png            the painted land: road, regions, water, cities, sanctuary, mountain
#         game/assets/Compiled/World/Map.txt         which part of the world the picture covers (read by WorldMap.cpp)
#
# The picture has no words and no markers: names, the player, arenas and city emblems are drawn by the game
# over it, so they stay sharp and follow the journey. The road runs left to right: the sanctuary is at the
# left edge, the mountain at the right (world +z is right, world +x is down).
#
# Usage: python art/map/build_map.py      (after art/world/build_world.py and art/cities/build_cities.py)
import math
import pathlib

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

REPO = pathlib.Path(__file__).resolve().parents[2]
WORLD = REPO / "game" / "assets" / "Compiled" / "World"
OUT_PNG = REPO / "game" / "assets" / "ui" / "Map" / "WorldMap.png"
OUT_TXT = WORLD / "Map.txt"

W, H = 2048, 1152                                   # picture size (16:9)
MIN_Z, MAX_Z = -150.0, 2076.0                       # world metres across the picture
MIN_X = -(MAX_Z - MIN_Z) * H / W / 2; MAX_X = -MIN_X # and down it (same scale)
SS = 2                                              # supersampling of drawn shapes
SANCTUARY_RADIUS = 97.0
MOUNTAIN = (0.0, 1900.0)                            # x, z (GameConstants.h MountainX / MountainZ)

# one colour per region of the road (the land beside it takes this tint)
REGION = [(60, 96, 92), (62, 104, 66), (50, 96, 104), (48, 92, 56), (96, 100, 104), (150, 170, 190),
          (140, 74, 50), (118, 108, 92), (52, 104, 140), (132, 106, 58), (104, 70, 140), (70, 100, 118)]
WATER_REGIONS = (2, 8)


def main():
    rows = [l.split() for l in open(WORLD / "Road.txt", encoding="utf-8")]
    P = [(float(r[1]), float(r[3]), float(r[4]), int(r[5])) for r in rows if r and r[0] == "P"]       # x, z, half width, region
    arenas = [(float(r[5]), float(r[7]), float(r[4]), int(r[3])) for r in rows if r and r[0] == "A"]   # x, z, radius, kind
    city_rows = [l.split() for l in open(WORLD / "Cities.txt", encoding="utf-8")]
    buildings = [(float(r[2]), float(r[4])) for r in city_rows if r and r[0] == "B"]
    cities = [(float(r[6]), float(r[8])) for r in city_rows if r and r[0] == "CITY"]

    k = W / (MAX_Z - MIN_Z)
    def px(x, z, s=SS): return ((z - MIN_Z) * k * s, (x - MIN_X) * k * s)

    rng = np.random.default_rng(7)
    def noise(cells):
        small = (rng.random((cells * H // W + 2, cells + 2)) * 255).astype(np.uint8)
        return np.asarray(Image.fromarray(small).resize((W, H), Image.BICUBIC), np.float32) / 255

    def mask(draw, blur=0.0):
        img = Image.new("L", (W * SS, H * SS), 0); draw(ImageDraw.Draw(img))
        img = img.resize((W, H), Image.LANCZOS)
        if blur: img = img.filter(ImageFilter.GaussianBlur(blur))
        return np.asarray(img, np.float32) / 255

    def road_line(d, width, region=None):
        pts = [px(x, z) for x, z, _, st in P if region is None or st == region]
        if len(pts) > 1: d.line(pts, fill=255, width=int(width * SS), joint="curve")

    # ---- the land: dark where nobody walks, lit and tinted by its region near the road
    land = np.zeros((H, W, 3), np.float32) + np.array([14, 26, 30], np.float32)
    land *= (.75 + .5 * noise(40))[..., None]
    near = np.clip(mask(lambda d: road_line(d, 150), 60) * 1.6, 0, 1)
    wide = np.clip(mask(lambda d: road_line(d, 420), 150) * 1.4, 0, 1)
    tint = np.zeros((H, W, 3), np.float32); weight = np.zeros((H, W), np.float32)
    for region, color in enumerate(REGION):
        m = mask(lambda d, region=region: road_line(d, 240, region), 70)
        tint += m[..., None] * np.array(color, np.float32); weight += m
    tint /= np.maximum(weight, 1e-3)[..., None]
    relief = .62 + .76 * noise(90) * .5 + .38 * noise(18)
    land = land * (1 - wide[..., None] * .9) + tint * (.30 + .40 * near[..., None]) * relief[..., None] * wide[..., None] * .9 + land * 0
    ridge = np.clip((noise(26) - .5) * 5, -1, 1)                                                     # hill shading
    gy, gx = np.gradient(ridge)
    land *= (1 + np.clip((gx + gy) * 14, -.35, .35) * wide)[..., None]

    # ---- water: the lake regions
    for region in WATER_REGIONS:
        m = np.clip(mask(lambda d, region=region: road_line(d, 210, region), 26) * 1.8 * (.55 + .9 * noise(14)), 0, 1)
        m = np.clip((m - .42) * 6, 0, 1)
        water = np.array([24, 62, 84], np.float32) * (.8 + .4 * noise(120))[..., None]
        land = land * (1 - m[..., None]) + water * m[..., None]
        shore = np.clip(1 - np.abs(m - .5) * 2.4, 0, 1) * (m > .02)
        land += shore[..., None] * np.array([40, 60, 60], np.float32) * .5

    img = Image.fromarray(np.clip(land, 0, 255).astype(np.uint8)).resize((W * SS, H * SS), Image.BILINEAR)
    d = ImageDraw.Draw(img, "RGBA")

    # ---- the sanctuary and the mountain
    cx, cy = px(0, 0); r = SANCTUARY_RADIUS * k * SS
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(40, 96, 96, 255), outline=(150, 220, 210, 255), width=3 * SS)
    d.ellipse([cx - r * .55, cy - r * .55, cx + r * .55, cy + r * .55], outline=(150, 220, 210, 140), width=2 * SS)
    mx, my = px(*MOUNTAIN)
    for dz, dx, size, shade in ((-70, -150, 150, .62), (-60, 150, 140, .58), (-20, -70, 200, .78), (-10, 80, 190, .74), (40, 0, 270, 1.0)):
        ax, ay = mx + dz * k * SS, my + dx * k * SS; s = size * k * SS
        body = tuple(int(c * shade) for c in (74, 88, 104))
        d.polygon([(ax - s * .50, ay + s * .55), (ax + s * .16, ay - s * .62), (ax + s * .62, ay + s * .55)], fill=body + (255,))
        d.polygon([(ax + s * .16, ay - s * .62), (ax + s * .62, ay + s * .55), (ax + s * .22, ay + s * .55)], fill=tuple(int(c * .62) for c in body) + (255,))
        d.polygon([(ax + s * .16, ay - s * .62), (ax + s * .02, ay - s * .34), (ax + s * .12, ay - s * .40), (ax + s * .18, ay - s * .28),
                   (ax + s * .25, ay - s * .40), (ax + s * .30, ay - s * .30)], fill=(232, 240, 248, 255))

    # ---- the road: dark edge, then gold
    pts = [px(x, z) for x, z, _, _ in P]
    d.line(pts, fill=(8, 10, 12, 255), width=11 * SS, joint="curve")
    d.line(pts, fill=(170, 128, 62, 255), width=6 * SS, joint="curve")
    d.line(pts, fill=(246, 214, 140, 255), width=2 * SS, joint="curve")

    # ---- arenas: rings on the road (the game marks the next one)
    for x, z, radius, kind in arenas:
        ax, ay = px(x, z); r = max(radius * k, 9 if kind else 7) * SS
        d.ellipse([ax - r, ay - r, ax + r, ay + r], fill=(18, 20, 22, 255), outline=(236, 200, 120, 255) if kind else (150, 150, 140, 255), width=2 * SS)

    # ---- cities: a glow, and every building as a roof
    glow = Image.new("L", img.size, 0); g = ImageDraw.Draw(glow)
    for x, z in cities:
        ax, ay = px(x, z); r = 86 * k * SS
        g.ellipse([ax - r, ay - r, ax + r, ay + r], fill=150)
    glow = glow.filter(ImageFilter.GaussianBlur(28 * SS))
    img = Image.composite(Image.new("RGB", img.size, (255, 206, 120)), img, glow.point(lambda v: int(v * .42)))
    d = ImageDraw.Draw(img, "RGBA")
    for x, z in buildings:
        ax, ay = px(x, z); r = 3.4 * SS
        d.rectangle([ax - r, ay - r, ax + r, ay + r], fill=(250, 226, 170, 255), outline=(60, 34, 20, 255))

    img = img.resize((W, H), Image.LANCZOS)
    a = np.asarray(img, np.float32)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    edge = np.minimum(np.minimum(xx, W - 1 - xx) / 110, np.minimum(yy, H - 1 - yy) / 110).clip(0, 1)
    a *= (.35 + .65 * edge * edge * (3 - 2 * edge))[..., None]                                       # the edges fall into the dark
    OUT_PNG.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(OUT_PNG, optimize=True)
    OUT_TXT.write_text("# The part of the world that assets/ui/Map/WorldMap.png shows: written by art/map/build_map.py.\n"
                       "# EXTENT minX maxX minZ maxZ (world metres; +z is right, +x is down) | SIZE width height (pixels)\n"
                       f"EXTENT {MIN_X:.2f} {MAX_X:.2f} {MIN_Z:.2f} {MAX_Z:.2f}\nSIZE {W} {H}\n", encoding="utf-8")
    print(f"WorldMap.png {W}x{H}, {k:.3f} px per metre, {len(cities)} cities, {len(buildings)} buildings")


if __name__ == "__main__":
    main()
