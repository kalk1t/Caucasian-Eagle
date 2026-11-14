"""Says where the places beside the road stand, and what stands at each.

    python art/places/build_places.py

Reads   game/assets/Compiled/World/Road.txt, Cities.txt, Terrain.bin   (the road, the cities, the land)
        art/places/sizes.txt                    each model's proportions (written by prepare_places.py)
        game/src/Story.cpp                      the names of the places (Story::Places): checked against SITES below
Writes  game/assets/Compiled/World/Places.txt   models, things, what she walks round, clearings, lights, named places

The models are made by prepare_places.py from the Meshy library. A named place (a SITE) is one stretch of roadside
with a story: she finds it by coming near. Everything else here is dressing: milestones, camps, rocks, old trees.
Nothing stands in an arena or a city. Run this again when Road.txt or Cities.txt changes.

Places.txt, engine coordinates, metres:
  M index model roofs          a model (Places/<Name>); roofs = 1: she can come down on it
  B model x y z yaw height far one thing: its base, the way it faces, how tall it stands, from how far it is drawn
  O x z r / o x z r            what she walks round (O: something with a top she may stand on)
  C x z r                      a clearing: no grass, no trees, none of the world's own scenery
  L x y z r g b size           a light
  SITE index s x y z radius    a named place: metres along the road, its middle, how near she must come
"""
import math, pathlib, random, re, struct
import numpy as np

REPO = pathlib.Path(__file__).resolve().parents[2]
WORLD = REPO / "game" / "assets" / "Compiled" / "World"
STEP = 2.0
WATER = {2: -13.1, 8: 17.1}   # build_world.py WATER: region -> water level

# what kind of thing each model is: (roofs, drawn from this far in m, what she walks round: box / legs / round / none)
KIND = {
    "Chapel": (1, 450, "box"), "Abbey": (1, 520, "box"), "Inn": (1, 450, "box"), "Farmhouse": (1, 400, "box"), "WitchHouse": (1, 400, "box"),
    "RuinedHouse": (1, 380, "box"), "RuinedFort": (1, 520, "box"), "Skullspire": (1, 520, "box"), "FaceRock": (1, 700, "box"),
    "Watchtower": (1, 600, "box"), "BrokenStair": (1, 450, "box"), "EmeraldGate": (1, 450, "legs"), "Archway": (1, 520, "legs"),
    "OldArch": (1, 520, "legs"), "Pavilion": (1, 380, "round"), "Dock": (1, 300, "none"), "Plateau": (1, 320, "box"),
    "Throne": (1, 450, "box"), "KingFountain": (1, 320, "box"), "Runestone": (0, 260, "round"), "GoldColumn": (0, 380, "round"),
    "StoneWall": (0, 300, "box"), "CobbleCircle": (0, 200, "none"), "Crag": (0, 320, "round"), "Golem": (0, 420, "round"),
    "Priestess": (0, 380, "round"), "Palisade": (0, 220, "box"), "Barrel": (0, 150, "round"), "Crate": (0, 150, "round"),
    "Stall": (0, 220, "box"), "WeaponRack": (0, 180, "box"), "Well": (0, 240, "round"), "LampPost": (0, 220, "none"),
    "Brazier": (0, 220, "round"), "Rug": (0, 110, "none"), "Quiver": (0, 110, "none"), "ObsidianHeap": (0, 220, "round"),
    "Hoard": (0, 260, "round"), "Rope": (0, 110, "none"), "Beam": (0, 130, "none"), "Key": (0, 220, "none"),
    "Shackles": (0, 200, "none"), "MapChest": (0, 180, "round"), "MossLog": (0, 180, "round"), "Stump": (0, 180, "round"),
    "Pine": (0, 320, "round"), "Shrub": (0, 130, "none"), "GlowPlant": (0, 240, "round"), "OldTree": (0, 300, "round"),
    "TreeHouse": (1, 420, "round"), "Grove": (0, 380, "none"),
}


def smooth(t):
    t = max(0.0, min(1.0, t)); return t * t * (3 - 2 * t)


class Road:
    def __init__(self):
        rows = [l.split() for l in open(WORLD / "Road.txt", encoding="utf-8")]
        self.P = [(float(r[1]), float(r[2]), float(r[3]), float(r[4]), int(r[5])) for r in rows if r and r[0] == "P"]
        self.arenas = [(float(r[2]), float(r[4])) for r in rows if r and r[0] == "A"]
        self.cities = [(float(r[4]), float(r[5])) for r in (l.split() for l in open(WORLD / "Cities.txt", encoding="utf-8")) if r and r[0] == "CITY"]
        data = (WORLD / "Terrain.bin").read_bytes()
        nx, nz, x0, z0, grid = struct.unpack_from("<iifff", data, 4)
        self.h = np.frombuffer(data, "<f4", nx * nz, 24).reshape(nz, nx)
        self.nx, self.nz, self.x0, self.z0, self.grid = nx, nz, x0, z0, grid
        self.length = (len(self.P) - 1) * STEP

    def frame(self, s):
        n = len(self.P); i = max(1, min(n - 2, int(round(s / STEP))))
        tx, tz = self.P[i + 1][0] - self.P[i - 1][0], self.P[i + 1][2] - self.P[i - 1][2]
        ln = math.hypot(tx, tz); tx, tz = tx / ln, tz / ln
        return self.P[i], (tx, tz), (tz, -tx), self.P[i][3], self.P[i][4]

    def land(self, x, z):
        fx = min(max((x - self.x0) / self.grid, 0), self.nx - 1.001); fz = min(max((z - self.z0) / self.grid, 0), self.nz - 1.001)
        ix, iz = int(fx), int(fz); ax, az = fx - ix, fz - iz; h = self.h
        return float((h[iz, ix] * (1 - ax) + h[iz, ix + 1] * ax) * (1 - az) + (h[iz + 1, ix] * (1 - ax) + h[iz + 1, ix + 1] * ax) * az)

    def free(self, s, margin=42.0):
        """Not in an arena, not in a city, not at the very ends of the road."""
        if s < 30 or s > self.length - 70: return False
        if any(abs(s - a) < r + margin - 17 for a, r in self.arenas): return False
        return not any(s0 - 14 < s < s1 + 14 for s0, s1 in self.cities)


road = Road()
sizes = {}
for line in open(REPO / "art" / "places" / "sizes.txt", encoding="utf-8"):
    if line.strip() and not line.startswith("#"):
        name, wide, deep, tall = line.split(); sizes[name] = (float(wide), float(deep), float(tall))
missing = [m for m in KIND if m not in sizes]
if missing: raise SystemExit("no size for %s: run prepare_places.py" % missing)

models, lines, sites = {}, [], []
things = lights = 0
glades = []


def model(name):
    if name not in models:
        models[name] = len(models)
    return models[name]


def put(name, tall, s, side, out, turn=0.0, face="road", sink=0.0, lift=None, solid=None, far=None, across=0.0):
    """Stand a model 'tall' metres high at 's' metres along the road, 'out' metres beyond its edge on 'side' (+1 right,
    -1 left; 0 = on the road, 'across' metres to the right of its middle). face: 'road' looks at the road, 'away' has
    its back to it, 'along' looks down the road (an arch over it). lift: metres over the ground (else it stands on it)."""
    global things
    wide, deep, high = sizes[name]; k = tall / high; w, d = wide * k, deep * k
    roofs, reach, shape = KIND[name]
    if solid is not None: shape = solid
    p, t, n, half, stage = road.frame(s)
    if side == 0:
        x, z = p[0] + n[0] * across, p[2] + n[1] * across; fx, fz = t
    else:
        lateral = half + out; x, z = p[0] + n[0] * side * lateral, p[2] + n[1] * side * lateral
        fx, fz = (-n[0] * side, -n[1] * side) if face != "away" else (n[0] * side, n[1] * side)
    if face == "along": fx, fz = t
    yaw = math.atan2(fx, fz) + turn
    c, sn = math.cos(yaw), math.sin(yaw)
    corner = lambda lx, lz: (x + lx * c + lz * sn, z - lx * sn + lz * c)          # model x, z -> world (as the game's Place)
    if lift is not None and side == 0: ground = p[1]
    else:
        pts = [(0, 0)] + [(sx * w * .4, sz * d * .4) for sx in (-1, 1) for sz in (-1, 1)]
        heights = [road.land(*corner(*q)) for q in pts]
        ground = p[1] if side == 0 and abs(across) < half else min(heights) if shape in ("box", "legs") else heights[0]
    if stage in WATER and name == "Dock": ground = max(ground, WATER[stage] - .25)
    y = ground - sink + (lift or 0.0)
    lines.append("B %d %.2f %.2f %.2f %.4f %.2f %d" % (model(name), x, y, z, yaw, tall, far or reach))
    things += 1
    tag = "O" if roofs else "o"
    if shape == "box":
        short, long_ = min(w, d), max(w, d); r = max(.3, short / 2 * .92)
        count = max(1, int(math.ceil(long_ / max(short, .5))))
        for i in range(count):
            off = (i - (count - 1) / 2) * ((long_ - short) / max(1, count - 1))
            q = corner(off, 0) if w >= d else corner(0, off)
            lines.append("%s %.2f %.2f %.2f" % (tag, q[0], q[1], r))
    elif shape == "legs":
        leg = w * .13
        for e in (-1, 1):
            q = corner(e * (w / 2 - leg), 0); lines.append("%s %.2f %.2f %.2f" % (tag, q[0], q[1], leg))
    elif shape == "round":
        lines.append("%s %.2f %.2f %.2f" % (tag, x, z, max(.3, min(w, d) * .42)))
    return x, y, z


def light(s, side, out, up, colour, size, across=0.0):
    global lights
    p, t, n, half, stage = road.frame(s)
    lateral = across if side == 0 else side * (half + out)
    x, z = p[0] + n[0] * lateral, p[2] + n[1] * lateral
    base = p[1] if side == 0 else road.land(x, z)
    lines.append("L %.2f %.2f %.2f %.2f %.2f %.2f %.2f" % (x, base + up, z, colour[0], colour[1], colour[2], size)); lights += 1


def flat(s, side, lo, hi, w, d):
    """The distance beyond the road's edge, between lo and hi, where a footprint w x d lies most level."""
    best, best_range = lo, 1e9
    p, t, n, half, stage = road.frame(s)
    out = lo
    while out <= hi + 1e-6:
        cx, cz = p[0] + n[0] * side * (half + out), p[2] + n[1] * side * (half + out)
        hs = [road.land(cx + n[0] * a * d * .5 + t[0] * b * w * .5, cz + n[1] * a * d * .5 + t[1] * b * w * .5) for a in (-1, 0, 1) for b in (-1, 0, 1)]
        if stage in WATER and min(hs) < WATER[stage] + .3: rng = 1e6 + out      # not in the water
        else: rng = max(hs) - min(hs) + out * .02
        if rng < best_range: best, best_range = out, rng
        out += 2.0
    return best


def side_for(s, lo, hi, w, d):
    """The flatter side of the road at s, and how far out."""
    cands = []
    for side in (1, -1):
        out = flat(s, side, lo, hi, w, d)
        p, t, n, half, stage = road.frame(s)
        cx, cz = p[0] + n[0] * side * (half + out), p[2] + n[1] * side * (half + out)
        hs = [road.land(cx + n[0] * a * d * .5 + t[0] * b * w * .5, cz + n[1] * a * d * .5 + t[1] * b * w * .5) for a in (-1, 0, 1) for b in (-1, 0, 1)]
        wet = stage in WATER and min(hs) < WATER[stage] + .3
        cands.append(((1e6 if wet else 0) + max(hs) - min(hs), side, out))
    cands.sort(); return cands[0][1], cands[0][2]


def site(name, s, side, out, radius, glade=None):
    """A named place: its middle is 'out' metres beyond the road's edge on 'side' (0: on the road)."""
    if not road.free(s, 40): raise SystemExit("%s at %.0f m stands in an arena or a city" % (name, s))
    p, t, n, half, stage = road.frame(s)
    lateral = 0 if side == 0 else side * (half + out)
    x, z = p[0] + n[0] * lateral, p[2] + n[1] * lateral
    sites.append((s, name, x, p[1] if side == 0 else road.land(x, z), z, radius))
    if glade: glades.append((x, z, glade))


FIRE, GREEN, COLD, GOLD, RED, VIOLET = (1.0, .62, .28), (.45, 1.0, .40), (.62, .85, 1.0), (1.0, .82, .42), (1.0, .30, .22), (.72, .42, 1.0)


def camp(s, side, out, seed, stall=False):
    """A few things travellers left: barrels, crates, a beam, sometimes a stall."""
    r = random.Random(seed)
    put("Barrel", 1.1, s, side, out, turn=r.uniform(0, 6)); put("Barrel", 1.05, s + 1.3, side, out + .6, turn=r.uniform(0, 6))
    put("Crate", .75, s - 1.6, side, out + .3, turn=r.uniform(-.5, .5)); put("Crate", .7, s - 1.4, side, out + 1.7, turn=r.uniform(-.5, .5))
    put("Beam", .42, s + 3.2, side, out + 1.2, turn=r.uniform(-.6, .6))
    if stall: put("Stall", 3.0, s - 5.5, side, out + 1.8)


# ================================================================ the named places, in road order
# 1 Moonlit Causeway
side, out = side_for(72, 9, 20, 16, 11)
site("The Last Inn", 72, side, out, 30, glade=17)
put("Inn", 11, 72, side, out); put("Well", 3.2, 60, side, 4.5); put("LampPost", 4.4, 66, side, 1.2, turn=1.57); light(66, side, 1.4, 3.6, FIRE, 2.0)
put("LampPost", 4.4, 80, side, 1.2, turn=-1.57); light(80, side, 1.4, 3.6, FIRE, 2.0); put("Stall", 3.0, 86, side, 4.0)
camp(84, side, 2.2, 1); put("Palisade", 2.4, 58, side, out + 8, turn=1.57); put("Palisade", 2.4, 88, side, out + 8, turn=1.57)

site("The Travellers' Arch", 128, 0, 0, 22)
put("Archway", 22, 128, 0, 0, face="along", sink=.7); light(128, 0, 0, 20, COLD, 3.5)
put("Runestone", 3.4, 121, 1, 1.5); put("Runestone", 3.4, 121, -1, 1.5)

side, out = side_for(282, 10, 22, 12, 11)
site("The Shepherd's Farm", 282, side, out, 28, glade=15)
put("Farmhouse", 8, 282, side, out)
for k in range(5): put("Palisade", 2.2, 268 + k * 5.2, side, 3.0, turn=1.57)
put("Stump", 1.1, 296, side, 6); put("MossLog", 1.3, 299, side, 10, turn=.6); put("Crate", .75, 276, side, 5.5); put("Barrel", 1.1, 278, side, 5.0)
put("Well", 3.0, 292, side, out - 4)

# 2 Whispering Wood
side, out = side_for(604, 10, 24, 11, 10)
site("The House of Green Fire", 604, side, out, 28, glade=14)
put("WitchHouse", 9.5, 604, side, out); put("Brazier", 1.3, 598, side, out - 7); light(598, side, out - 7, 1.9, GREEN, 3.0)
put("Brazier", 1.3, 610, side, out - 7); light(610, side, out - 7, 1.9, GREEN, 3.0)
put("GlowPlant", 3.4, 594, side, out + 3); put("GlowPlant", 2.6, 615, side, out + 1); put("Shrub", 1.7, 600, side, 4); put("Shrub", 1.5, 609, side, 5)
put("OldTree", 9, 618, side, out + 9, turn=1.0)

side, out = side_for(772, 9, 20, 13, 13)
site("The Watching Tree", 772, side, out, 28, glade=12)
put("TreeHouse", 17, 772, side, out); put("Rope", .4, 768, side, out - 6.5); put("Stump", 1.0, 779, side, out - 5); put("MossLog", 1.4, 764, side, out - 3, turn=.8)
light(772, side, out, 9, GREEN, 2.2)

# 3 Drowned Marsh
site("The Ferryman's Dock", 992, 1, 12, 26)
put("Dock", 3.6, 992, 1, 9, face="away"); put("LampPost", 4.2, 992, 1, 1.5, turn=1.57); light(992, 1, 1.6, 3.5, COLD, 2.2)
put("Barrel", 1.1, 988, 1, 2.5); put("Crate", .75, 989.5, 1, 3.4); put("Rope", .4, 995, 1, 3.0)

side, out = side_for(1140, 12, 26, 10, 17)
site("The Drowned Chapel", 1140, side, out, 30, glade=15)
put("Chapel", 15, 1140, side, out, sink=1.2); put("Runestone", 3.2, 1131, side, 3); put("Runestone", 3.2, 1149, side, 3)
light(1140, side, out, 11, COLD, 3.0)

# 4 Ochokochi's Wood
side, out = side_for(1560, 4, 12, 9, 8)
site("The Hunter's Camp", 1560, side, out, 24, glade=10)
put("Rug", .12, 1560, side, out, turn=.3); put("WeaponRack", 2.4, 1560, side, out + 3.2); put("Quiver", 1.0, 1557.4, side, out + 2.8)
put("Brazier", 1.2, 1560, side, out - 2.8); light(1560, side, out - 2.8, 1.7, FIRE, 2.6)
put("MossLog", 1.3, 1564.5, side, out - 1, turn=1.3); put("Stump", 1.0, 1555.5, side, out - 1.2); put("Crate", .75, 1563.6, side, out + 2.6); put("Crate", .7, 1564.8, side, out + 3.7, turn=.5)

site("The Forest Queen's Arch", 1696, 0, 0, 22)
put("OldArch", 27, 1696, 0, 0, face="along"); light(1696, 0, 0, 24, GREEN, 3.5)
put("OldTree", 10, 1690, 1, 9, turn=2.0); put("OldTree", 8.5, 1702, -1, 8)

side, out = side_for(1872, 6, 14, 7, 6)
site("The King in the Moss", 1872, side, out, 24, glade=9)
put("CobbleCircle", .45, 1872, side, out - 1); put("KingFountain", 6.5, 1872, side, out + 3)
put("Shrub", 1.6, 1866, side, out + 2); put("Shrub", 1.8, 1878, side, out + 3); put("MossLog", 1.3, 1880, side, out - 3, turn=.9)
light(1872, side, out, 3.5, COLD, 2.2)

# 5 Stone Towers
side, out = side_for(2236, 10, 22, 17, 17)
site("The Empty Watchtower", 2236, side, out, 32, glade=15)
put("Watchtower", 26, 2236, side, out); light(2236, side, out, 25, FIRE, 4.0)
for k in range(3): put("StoneWall", 5.5, 2222 + k * 3.6, side, out - 6, turn=1.57)
put("Crag", 4.5, 2250, side, out + 2, turn=1.2)

side, out = side_for(2416, 10, 22, 11, 16)
site("The Broken Stair", 2416, side, out, 30, glade=14)
put("BrokenStair", 17, 2416, side, out); put("Crag", 6, 2405, side, out + 5, turn=.7); put("Crag", 3.5, 2427, side, out - 3, turn=2.4)
put("GoldColumn", 6, 2424, side, 3.5)

side, out = side_for(2578, 9, 20, 13, 12)
site("The Silent House", 2578, side, out, 28, glade=14)
put("RuinedHouse", 6, 2578, side, out); put("Well", 3.0, 2568, side, 4); put("Palisade", 2.2, 2588, side, 3.5, turn=1.57); put("Palisade", 2.2, 2593, side, 3.5, turn=1.57)
put("Stump", 1.0, 2586, side, out - 7)

# 6 Snow Pass
side, out = side_for(2644, 6, 16, 9, 8)
site("The Stone Sleeper", 2644, side, out, 26, glade=10)
put("Golem", 10, 2644, side, out, turn=.25); put("Crag", 5, 2637, side, out + 3, turn=1.5); put("Crag", 3.2, 2651, side, out + 1)
light(2644, side, out, 6.5, COLD, 2.6)

side, out = side_for(2800, 14, 30, 24, 28)
site("The Abbey in the Snow", 2800, side, out, 40, glade=22)
put("Abbey", 21, 2800, side, out); light(2800, side, out - 8, 6, RED, 4.5); light(2792, side, out, 6, RED, 3.5); light(2808, side, out, 6, RED, 3.5)
put("LampPost", 4.4, 2790, side, 1.4, turn=1.57); light(2790, side, 1.6, 3.6, FIRE, 2.0); put("Runestone", 3.6, 2812, side, 2)

# 7 Kajeti
side, out = side_for(3362, 10, 22, 15, 15)
site("The Smiths' Stores", 3362, side, out, 32, glade=15)
put("Skullspire", 23, 3362, side, out); light(3362, side, out, 16, FIRE, 4.0)
put("ObsidianHeap", 1.7, 3352, side, 4, turn=.4); put("ObsidianHeap", 1.4, 3371, side, 5, turn=2.0); put("WeaponRack", 2.4, 3358, side, 2.5)
put("Beam", .45, 3366, side, 3, turn=.4); put("Beam", .45, 3366.6, side, 3.7, turn=.7); put("Brazier", 1.3, 3374, side, 2.2); light(3374, side, 2.2, 1.9, FIRE, 2.6)

side, out = side_for(3546, 9, 20, 18, 13)
site("The Emerald Gate", 3546, side, out, 30, glade=14)
put("EmeraldGate", 13, 3546, side, out, solid="box"); light(3546, side, out, 4, GREEN, 5.0); light(3546, side, out - 5, 2.5, GREEN, 3.0)

# 8 Giants' Valley
side, out = side_for(3922, 16, 34, 28, 29)
site("The Rock of Faces", 3922, side, out, 44, glade=22)
put("FaceRock", 40, 3922, side, out); light(3922, side, out - 12, 8, GOLD, 4.0)
put("Brazier", 1.6, 3914, side, 3); light(3914, side, 3, 2.2, FIRE, 2.6); put("Brazier", 1.6, 3930, side, 3); light(3930, side, 3, 2.2, FIRE, 2.6)

side, out = side_for(4106, 8, 18, 11, 11)
site("The Empty Throne", 4106, side, out, 30, glade=13)
put("Throne", 14, 4106, side, out); put("Crag", 7, 4094, side, out + 4, turn=.9); put("Crag", 5, 4118, side, out + 3, turn=2.2)
put("CobbleCircle", .5, 4106, side, out - 9)

# 9 Dragon Lake
side, out = side_for(4332, 6, 16, 20, 12)
site("The Golden Columns", 4332, side, out, 30, glade=14)
for k in range(4):
    put("GoldColumn", 9, 4323 + k * 6, side, out - 3); put("GoldColumn", 9, 4323 + k * 6, side, out + 4)
put("Brazier", 1.6, 4332, side, out + .5); light(4332, side, out + .5, 2.4, GOLD, 3.5); put("CobbleCircle", .5, 4332, side, out + .5)

# 10 Devi's Gorge
side, out = side_for(5042, 5, 14, 9, 9)
site("The Devi's Hoard", 5042, side, out, 26, glade=10)
put("Hoard", 2.4, 5042, side, out); put("MapChest", 1.4, 5046.5, side, out - 2.5, turn=-.5); put("Key", 2.4, 5038.5, side, out - 1.5, turn=.4)
put("Brazier", 1.3, 5036, side, out + 2); light(5036, side, out + 2, 1.9, GOLD, 3.0); light(5042, side, out, 2.5, GOLD, 3.0)

side, out = side_for(5222, 14, 28, 24, 25)
site("The Fort of Three Kings", 5222, side, out, 40, glade=20)
put("RuinedFort", 19, 5222, side, out); put("StoneWall", 5.5, 5208, side, 5, turn=1.57); put("StoneWall", 5.5, 5236, side, 5, turn=1.57)
light(5222, side, out, 10, FIRE, 3.5)

# 11 Witches' Hill
side, out = side_for(5452, 5, 12, 12, 4)
site("The First Chain", 5452, side, out, 24, glade=9)
for k in range(3): put("StoneWall", 6, 5448.2 + k * 3.8, side, out, turn=0)
put("Shackles", 3.0, 5452, side, out - .7, lift=2.2); put("Runestone", 3.2, 5444, side, out - 3); light(5452, side, out - 2, 3, VIOLET, 3.0)

side, out = side_for(5602, 8, 18, 12, 12)
site("The Dancing Floor", 5602, side, out, 30, glade=14)
put("Pavilion", 9.5, 5602, side, out); light(5602, side, out, 5, VIOLET, 4.0)
for k in range(6):
    a = k * math.pi / 3; put("Runestone", 3.4, 5602 + math.cos(a) * 9.5, side, out + math.sin(a) * 6.5)
put("GlowPlant", 3.4, 5590, side, out + 6); put("GlowPlant", 2.8, 5614, side, out + 5)

# 12 Chain Road
side, out = side_for(6092, 5, 12, 6, 6)
site("The Fire Bearer", 6092, side, out, 26, glade=9)
put("Priestess", 7.5, 6092, side, out); put("Brazier", 1.5, 6087, side, out - 2.5); light(6087, side, out - 2.5, 2.1, FIRE, 3.0)
put("Brazier", 1.5, 6097, side, out - 2.5); light(6097, side, out - 2.5, 2.1, FIRE, 3.0); put("CobbleCircle", .45, 6092, side, out - 1)

site("The Last Arch", 6236, 0, 0, 22)
put("Archway", 24, 6236, 0, 0, face="along", sink=.8); light(6236, 0, 0, 21, COLD, 4.0)

# ================================================================ dressing: what stands between the named places
rng = random.Random(20261112)
taken = [s for s, *_ in sites]
clear = lambda s, gap=34: road.free(s, 44) and all(abs(s - t) > gap for t in taken)

# milestones: a rune stone at the road's edge about every 400 m
s = 210.0
while s < road.length - 120:
    t = s
    while not clear(t) and t < s + 120: t += 12
    if clear(t): put("Runestone", 3.3, t, 1 if int(s / 400) % 2 else -1, 1.3, turn=rng.uniform(-.3, .3)); taken.append(t)
    s += 400

# camps: what travellers left when they ran
for k, s in enumerate((330, 905, 1480, 2130, 2700, 3210, 3600, 4270, 4800, 5360, 5990)):
    t = s
    while not clear(t) and t < s + 140: t += 10
    if clear(t): camp(t, 1 if k % 2 else -1, 2.4, 100 + k, stall=k % 3 == 0); taken.append(t)

# what grows and what lies about, by region: (model, tall from, tall to, how many, metres beyond the road's edge from, to)
REGIONS = {
    0: [("OldTree", 7, 10, 3, 6, 22), ("Stump", .9, 1.2, 4, 3, 14), ("Shrub", 1.3, 1.9, 6, 2, 12)],
    1: [("OldTree", 8, 12, 5, 6, 24), ("MossLog", 1.1, 1.5, 5, 3, 14), ("Stump", .9, 1.3, 5, 3, 14), ("Shrub", 1.4, 2.0, 8, 2, 12), ("GlowPlant", 2.2, 3.4, 3, 6, 18)],
    2: [("Grove", 6, 9, 3, 10, 22), ("GlowPlant", 2.4, 3.8, 6, 4, 14), ("MossLog", 1.1, 1.4, 4, 2, 8), ("Stump", .9, 1.2, 3, 2, 8)],
    3: [("Pine", 20, 27, 6, 8, 26), ("MossLog", 1.2, 1.6, 5, 3, 14), ("Stump", 1.0, 1.4, 6, 3, 14), ("Shrub", 1.4, 2.0, 8, 2, 12)],
    4: [("Crag", 3, 8, 8, 5, 24), ("OldTree", 7, 10, 3, 6, 20), ("Shrub", 1.3, 1.8, 4, 2, 10)],
    5: [("Crag", 4, 10, 9, 5, 26), ("Pine", 18, 24, 4, 8, 24)],
    6: [("Crag", 3, 7, 6, 5, 22), ("ObsidianHeap", 1.2, 1.9, 6, 3, 12)],
    7: [("Crag", 5, 12, 9, 6, 28), ("Stump", 1.0, 1.4, 3, 3, 12)],
    8: [("GlowPlant", 2.4, 3.8, 6, 4, 14), ("Crag", 3, 6, 4, 5, 18)],
    9: [("Crag", 4, 9, 8, 5, 22), ("ObsidianHeap", 1.2, 1.8, 3, 3, 12)],
    10: [("GlowPlant", 2.6, 4.0, 6, 4, 16), ("OldTree", 7, 11, 5, 6, 22), ("Crag", 3, 6, 3, 5, 18)],
    11: [("Crag", 4, 9, 7, 5, 20)],
}
stage_range = {}
for i, p in enumerate(road.P):
    stage_range.setdefault(p[4], [i * STEP, i * STEP])[1] = i * STEP
for stage, rows in REGIONS.items():
    lo, hi = stage_range[stage]
    for name, t0, t1, count, o0, o1 in rows:
        for _ in range(count):
            for attempt in range(30):
                s = rng.uniform(lo + 20, hi - 20)
                if not clear(s, 30): continue
                side = rng.choice((-1, 1)); out = rng.uniform(o0, o1)
                p, t, n, half, st = road.frame(s)
                x, z = p[0] + n[0] * side * (half + out), p[2] + n[1] * side * (half + out)
                ground = road.land(x, z)
                if st in WATER and ground < WATER[st] + .2: continue
                if abs(ground - p[1]) > 9: continue                       # not up a cliff or down a pit
                put(name, rng.uniform(t0, t1), s, side, out, turn=rng.uniform(0, 6.28), sink=.15)
                break

# ================================================================ write
sites.sort()
story = (REPO / "game" / "src" / "Story.cpp").read_text(encoding="utf-8")
block = story[story.index("const Place Places[PlaceCount]"):]
block = block[:block.index("};")]
told = re.findall(r'\{L"((?:[^"\\]|\\.)*)",', block)
ours = [name for s, name, *_ in sites]
if told != ours:
    raise SystemExit("Story::Places and the places here differ:\n  story: %s\n  here:  %s" % (told, ours))
head = ["# The places beside the road to Amirani: written by art/places/build_places.py. Engine coordinates, metres.",
        "# M index model roofs | B model x y z yaw height far | O x z r (she may stand on it) | o x z r | C x z r | L x y z r g b size | SITE index s x y z radius"]
head += ["M %d Places/%s %d" % (i, name, KIND[name][0]) for name, i in sorted(models.items(), key=lambda kv: kv[1])]
tail = ["C %.2f %.2f %.2f" % g for g in glades]
tail += ["SITE %d %.1f %.2f %.2f %.2f %.1f" % (i, s, x, y, z, r) for i, (s, name, x, y, z, r) in enumerate(sites)]
(WORLD / "Places.txt").write_text("\n".join(head + lines + tail) + "\n", encoding="utf-8")
print("places: %d models, %d things, %d lights, %d named places" % (len(models), things, lights, len(sites)))
for i, (s, name, x, y, z, r) in enumerate(sites): print("  %2d  %6.0f m  %s" % (i + 1, s, name))
unused = [m for m in KIND if m not in models]
if unused: print("not used yet:", ", ".join(unused))
