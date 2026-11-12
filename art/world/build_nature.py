"""Builds what the game needs to grow grass and trees on the land beside the road, and its cloud layer.

    python art/world/build_nature.py            (from the repository root; no Blender needed)

Writes into game/assets:
    Compiled/World/Terrain.bin      the height of the land on a 4 m grid, how far each point is from the
                                    road's edge, and which region it belongs to
    textures/nature/Grass.png       four tufts of grass side by side (RGBA, cut out by alpha)
    textures/nature/Leaves.png      four sprays of leaves side by side (RGBA)
    textures/nature/Bark.png        tiling bark
    textures/nature/Clouds.png      tiling cloud cover (RGBA)

The land is computed with the same formulas as art/world/build_world.py (its tables and path code are
read from that file, so the two cannot drift apart; the terrain formula is repeated here and checked
against the prop heights in Road.txt). Run this again whenever build_world.py is run.

Terrain.bin: "CETR", int32 nx, nz, float32 x0, z0, grid, then nz*nx float32 heights (row by row, z outer),
nz*nx uint8 metres beyond the road's edge (0 = on the road, 255 = far), nz*nx uint8 region (0..11).
"""
import math
import os
import struct
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.normpath(os.path.join(HERE, '..', '..', 'game', 'assets'))
ROAD = os.path.join(ASSETS, 'Compiled', 'World', 'Road.txt')


# ------------------------------------------------------------------ the land, as build_world.py makes it
def world_tables():
    """Runs the tables, the path and the noise function of build_world.py (everything before its textures)."""
    source = open(os.path.join(HERE, 'build_world.py'), encoding='utf-8').read()
    start = source.index('# ------------------------------------------------------------------ design tables')
    end = source.index('def save_image')
    space = {'np': np, 'math': math, 'rng': np.random.default_rng(20261110), 'print': lambda *a, **k: None}
    exec(compile(source[start:end], 'build_world.py (tables and path)', 'exec'), space)
    return space

def terrain(w):
    np_, smooth, fbm = np, w['smooth'], w['fbm']
    GRID, STAGES, MOUNTAIN = w['GRID'], w['STAGES'], w['MOUNTAIN']
    xs = np.arange(w['XMIN'], w['XMAX'] + .1, GRID); zs = np.arange(w['ZMIN'], w['ZMAX'] + .1, GRID)
    GX, GZ = np.meshgrid(xs, zs)
    px, pz, py, pw, pst = w['land_path']()
    bankA = np.array([STAGES[s][2] for s in pst]); bankN = np.array([STAGES[s][3] for s in pst])
    shape = GX.shape
    base = np.zeros(shape); A = np.zeros(shape); Nn = np.zeros(shape); dmin = np.zeros(shape); wn = np.zeros(shape); stn = np.zeros(shape, int)
    for r0 in range(0, shape[0], 24):
        r1 = min(shape[0], r0 + 24)
        d = np.hypot(GX[r0:r1, :, None] - px[None, None, :], GZ[r0:r1, :, None] - pz[None, None, :])
        wgt = 1.0 / (d + 3.0) ** 6
        ws = wgt.sum(axis=2)
        base[r0:r1] = (wgt * py).sum(axis=2) / ws
        A[r0:r1] = (wgt * bankA).sum(axis=2) / ws
        Nn[r0:r1] = (wgt * bankN).sum(axis=2) / ws
        near = d.argmin(axis=2)
        dmin[r0:r1] = d.min(axis=2); wn[r0:r1] = pw[near]; stn[r0:r1] = pst[near]
    noise = fbm(shape[0], 1.9, 101, shape[1]) * 2 - 1
    detail = fbm(shape[0], 1.2, 102, shape[1]) * 2 - 1
    ridged = 1 - np.abs(fbm(shape[0], 2.0, 103, shape[1]) * 2 - 1)
    rolling = fbm(shape[0], 2.7, 104, shape[1]) * 2 - 1
    out = dmin - wn
    h = base + A * smooth((out - 1.0) / 30.0) + Nn * (noise * 1.6 + detail * 0.5) * smooth(out / 55.0)
    h += (2.5 + 0.9 * Nn) * rolling * smooth((out - 2.0) / 45.0)
    dm = np.hypot(GX - MOUNTAIN[0], GZ - MOUNTAIN[1])
    far = smooth((out - 110.0) / 320.0) * smooth((dm - 330.0) / 220.0)
    h += far * (30 + 150 * ridged ** 2)
    h = np.where(out < 1.0, base - 0.4, h)
    h = np.where((out < 1.0) & (GZ < w['PZ'][0] - 1.0), base, h)
    near_ledge = (np.abs(GX) < 60) & (GZ > 1790)
    h = np.where(near_ledge, np.minimum(h, 30.0), h)
    return xs, zs, h, out, stn

def check(xs, zs, h, grid):
    """The props in Road.txt stand 0.3 m into this land. If they do not, this file and build_world.py disagree."""
    worst, count = 0.0, 0
    for line in open(ROAD, encoding='ascii'):
        part = line.split()
        if not part or part[0] != 'T':
            continue
        x, y, z = float(part[2]), float(part[3]), float(part[4])
        fx = min(max((x - xs[0]) / grid, 0), len(xs) - 1.001); fz = min(max((z - zs[0]) / grid, 0), len(zs) - 1.001)
        ix, iz = int(fx), int(fz); tx, tz = fx - ix, fz - iz
        land = (h[iz, ix] * (1 - tx) + h[iz, ix + 1] * tx) * (1 - tz) + (h[iz + 1, ix] * (1 - tx) + h[iz + 1, ix + 1] * tx) * tz
        worst = max(worst, abs(land - 0.3 - y)); count += 1
    return worst, count


# ------------------------------------------------------------------ textures
def grass_texture(path, rng):
    """Four tufts in a row, each 256 x 256: blades fanning out of the middle of the bottom edge."""
    size, tufts = 256, 4
    scale = 4                                              # drawn large and reduced, for smooth edges
    image = Image.new('RGBA', (size * tufts * scale, size * scale), (60, 110, 50, 0))
    draw = ImageDraw.Draw(image)
    for tuft in range(tufts):
        cx = (tuft + .5) * size * scale
        blades = 34 + 8 * tuft
        for _ in range(blades):
            root = cx + rng.normal(0, size * scale * .11)
            height = size * scale * rng.uniform(.45, .97) * (.8 if tuft == 3 else 1)
            lean = rng.normal(0, .32)
            width = size * scale * rng.uniform(.012, .028)
            shade = rng.uniform(.62, 1.0)
            tone = (int(70 * shade + rng.uniform(0, 28)), int(128 * shade + rng.uniform(0, 30)), int(52 * shade))
            steps = 14
            points_left, points_right = [], []
            for k in range(steps + 1):
                f = k / steps
                x = root + lean * height * f * f                # the blade bends more toward its tip
                y = size * scale - height * f
                half = width * (1 - f) ** .8
                points_left.append((x - half, y)); points_right.append((x + half, y))
            draw.polygon(points_left + points_right[::-1], fill=tone + (255,))
            # a lighter rib up the middle
            rib = [((l[0] + r[0]) / 2, l[1]) for l, r in zip(points_left, points_right)]
            draw.line(rib, fill=(min(255, tone[0] + 30), min(255, tone[1] + 34), tone[2] + 12, 255), width=max(1, int(width * .25)))
    image = image.resize((size * tufts, size), Image.LANCZOS)
    # darker at the root, where blades shade each other
    pixels = np.asarray(image).astype(np.float32)
    fall = np.linspace(1.0, .55, size)[:, None, None]
    pixels[..., :3] *= fall
    # keep the colour of the blades in the transparent pixels too, so the edges do not turn dark when filtered
    alpha = pixels[..., 3:4] / 255
    pixels[..., :3] = pixels[..., :3] * alpha + np.array([58, 104, 44], np.float32) * (1 - alpha)
    Image.fromarray(pixels.clip(0, 255).astype(np.uint8), 'RGBA').save(path)

def leaves_texture(path, rng):
    """Four sprays of leaves in a row, each 256 x 256: a twig with many small leaves, thinning at the edge."""
    size, sprays, scale = 256, 4, 3
    image = Image.new('RGBA', (size * sprays * scale, size * scale), (48, 86, 40, 0))
    draw = ImageDraw.Draw(image)
    for spray in range(sprays):
        cx, cy = (spray + .5) * size * scale, size * scale * .5
        for _ in range(300 + 60 * spray):
            angle = rng.uniform(0, 2 * math.pi)
            reach = size * scale * .47 * math.sqrt(rng.uniform(0, 1)) * rng.uniform(.75, 1)
            x, y = cx + math.cos(angle) * reach, cy + math.sin(angle) * reach * .9
            length = size * scale * rng.uniform(.035, .07); wide = length * rng.uniform(.38, .55)
            turn = rng.uniform(0, math.pi)
            shade = rng.uniform(.5, 1.0) * (1.1 - .35 * reach / (size * scale * .47))
            tone = (int(58 * shade + rng.uniform(0, 26)), int(112 * shade + rng.uniform(0, 32)), int(46 * shade))
            leaf = []
            for k in range(12):
                a = k / 12 * 2 * math.pi
                lx, ly = math.cos(a) * length, math.sin(a) * wide * (1 - .35 * math.cos(a))    # pointed at one end
                leaf.append((x + lx * math.cos(turn) - ly * math.sin(turn), y + lx * math.sin(turn) + ly * math.cos(turn)))
            draw.polygon(leaf, fill=tone + (255,))
    image = image.resize((size * sprays, size), Image.LANCZOS)
    pixels = np.asarray(image).astype(np.float32)
    alpha = pixels[..., 3:4] / 255
    pixels[..., :3] = pixels[..., :3] * alpha + np.array([50, 92, 42], np.float32) * (1 - alpha)
    Image.fromarray(pixels.clip(0, 255).astype(np.uint8), 'RGBA').save(path)

def bark_texture(path, fbm):
    n = 256
    grain = fbm(n, 1.4, 311)
    ridges = np.abs(np.sin((np.arange(n)[None, :] / n * 22 + fbm(n, 2.0, 312) * 5) * math.pi))     # furrows running up the trunk
    value = (.35 + .4 * ridges ** .6 + .25 * grain)
    rgb = np.stack([value * 92, value * 74, value * 58], axis=-1)
    Image.fromarray(rgb.clip(0, 255).astype(np.uint8), 'RGB').save(path)

def cloud_texture(path, fbm):
    """Tiling cloud cover: billows with clear sky between them. Lit from above, darker underneath."""
    n = 1024
    cover = fbm(n, 1.75, 401) * .65 + fbm(n, 1.2, 402) * .35           # fbm is periodic, so the picture tiles
    cover = (cover - cover.mean()) / cover.std()
    density = np.clip((cover - .15) / 1.5, 0, 1) ** .8                   # about four tenths of the sky is cloud
    light = np.clip((cover - np.roll(cover, 9, axis=0)) * 2.2 + .62, .3, 1)   # the side toward the moon is brighter
    rgb = np.stack([light * 200, light * 214, light * 235], axis=-1)
    rgba = np.concatenate([rgb, (density * 235)[..., None]], axis=-1)
    Image.fromarray(rgba.clip(0, 255).astype(np.uint8), 'RGBA').filter(ImageFilter.GaussianBlur(1.2)).save(path)


if __name__ == '__main__':
    w = world_tables()
    xs, zs, h, out, stage = terrain(w)
    worst, count = check(xs, zs, h, w['GRID'])
    print(f'land {len(xs)} x {len(zs)} points; {count} props checked, worst height difference {worst:.3f} m')
    assert worst < .05, 'the land computed here is not the land of build_world.py'
    path = os.path.join(ASSETS, 'Compiled', 'World', 'Terrain.bin')
    with open(path, 'wb') as f:
        f.write(b'CETR' + struct.pack('<iifff', len(xs), len(zs), xs[0], zs[0], w['GRID']))
        f.write(h.astype('<f4').tobytes())
        f.write(np.clip(np.round(out), 0, 255).astype(np.uint8).tobytes())
        f.write(stage.astype(np.uint8).tobytes())
    print('wrote', path, os.path.getsize(path) // 1024, 'KB')

    folder = os.path.join(ASSETS, 'textures', 'nature')
    os.makedirs(folder, exist_ok=True)
    rng = np.random.default_rng(77)
    grass_texture(os.path.join(folder, 'Grass.png'), rng)
    leaves_texture(os.path.join(folder, 'Leaves.png'), rng)
    bark_texture(os.path.join(folder, 'Bark.png'), w['fbm'])
    cloud_texture(os.path.join(folder, 'Clouds.png'), w['fbm'])
    print('wrote textures in', folder)
