"""The Eagle Nest's colour map (2026-11-20): EagleNestBaseColor.png, 1024 x 1024, made here from noise (no downloaded
picture: CC0 by construction). build_eagle_nest.py maps the nest's parts into it:

    u 0 .. 1,    v 0 .. .75     bark of the sticks: 16 strips, each a different wood (dark brown, grey, weathered pale,
                                reddish), grain running along v (along a stick)
    u 0 .. .75,  v .75 .. 1     the matted floor: dry grass, moss and down
    u .75 .. 1,  v .75 .. 1     a feather: white, a dark tip, its shaft

v is measured from the top of the picture as glTF has it (row = v * 1024).

    python art/map/eagle_nest/make_nest_texture.py art/map/eagle_nest/EagleNestBaseColor.png
"""
import sys

import numpy as np
from PIL import Image, ImageFilter

SIZE = 1024
STRIPS = 16
rng = np.random.default_rng(20261120)


def noise(h, w, cells_y, cells_x, seed):
    """Smooth value noise, h x w, from a cells_y x cells_x grid (bicubic up)."""
    g = np.random.default_rng(seed).random((cells_y + 3, cells_x + 3)).astype(np.float32)
    im = Image.fromarray((g * 255).astype(np.uint8)).resize((w + w // max(cells_x, 1) * 3, h + h // max(cells_y, 1) * 3), Image.BICUBIC)
    a = np.asarray(im, dtype=np.float32)[:h, :w] / 255.0
    return a


def bark(h, w, base, seed):
    """One strip of bark, grain along the rows' direction (v: down the picture)."""
    out = np.zeros((h, w, 3), np.float32)
    streak = noise(h, w, 6, 40, seed) * .55 + noise(h, w, 24, 90, seed + 1) * .45          # long grain
    cracks = noise(h, w, 50, 130, seed + 2)
    knots = noise(h, w, 9, 6, seed + 3)
    shade = .62 + .55 * (streak - .5)
    shade -= np.clip((cracks - .62) * 3.2, 0, .45)                                          # dark splits in the bark
    shade += np.clip((knots - .78) * 2.0, 0, .25) * (noise(h, w, 30, 30, seed + 4) - .2)   # paler worn patches
    for c in range(3):
        out[..., c] = base[c] * shade
    # a little green-grey lichen here and there
    lichen = np.clip((noise(h, w, 14, 18, seed + 5) - .7) * 4, 0, 1) * (noise(h, w, 60, 60, seed + 6) > .45)
    out = out * (1 - lichen[..., None] * .55) + np.array([.42, .46, .36]) * lichen[..., None] * .55
    return np.clip(out, 0, 1)


def floor(h, w, seed):
    """The matted floor: crossing blades of dry grass, darker moss, flecks of down."""
    out = np.zeros((h, w, 3), np.float32)
    base = np.array([.40, .33, .22])
    shade = .55 + .5 * (noise(h, w, 10, 10, seed) - .5)
    img = Image.new("L", (w, h), 0)
    px = np.zeros((h, w), np.float32)
    r = np.random.default_rng(seed)
    for _ in range(2600):   # blades: short strokes in every direction
        x, y = r.random() * w, r.random() * h
        a = r.random() * np.pi
        length = 8 + r.random() * 30
        for t in np.linspace(0, 1, int(length)):
            xi, yi = int(x + np.cos(a) * length * t) % w, int(y + np.sin(a) * length * t) % h
            px[yi, xi] = max(px[yi, xi], .5 + .5 * r.random())
    blades = np.asarray(Image.fromarray((px * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3)), np.float32) / 255.0
    shade = shade * (.55 + .6 * blades)
    for c in range(3):
        out[..., c] = base[c] * shade
    moss = np.clip((noise(h, w, 8, 8, seed + 7) - .62) * 3, 0, 1)
    out = out * (1 - moss[..., None] * .6) + np.array([.20, .24, .13]) * moss[..., None] * .6
    down = (r.random((h, w)) > .9975).astype(np.float32)
    down = np.asarray(Image.fromarray((down * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2.2)), np.float32) / 255.0
    down = np.clip(down * 6, 0, 1)
    out = out * (1 - down[..., None]) + np.array([.86, .84, .80]) * down[..., None]
    del img
    return np.clip(out, 0, 1)


def feather(h, w, seed):
    """A feather along v (its quill at the bottom of the region, its tip at the top): white vane, a dark tip, a shaft."""
    out = np.zeros((h, w, 3), np.float32)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    v = 1 - yy / (h - 1)                 # 0 at the quill, 1 at the tip
    u = (xx / (w - 1)) * 2 - 1           # -1 .. 1 across
    barbs = .9 + .1 * np.sin((v * 90 + np.abs(u) * 14) * np.pi)   # the barbs' fine lines, slanting from the shaft
    white = np.array([.90, .89, .86])
    dark = np.array([.13, .12, .12])
    tip = np.clip((v - .72) * 5.5, 0, 1)[..., None]
    col = white * (1 - tip) + dark * tip
    col = col * barbs[..., None]
    shaft = np.clip(1 - np.abs(u) * 40, 0, 1)[..., None]
    col = col * (1 - shaft) + np.array([.80, .76, .66]) * shaft
    out[:] = col
    return np.clip(out, 0, 1)


def main(path):
    tex = np.zeros((SIZE, SIZE, 3), np.float32)
    bark_h = int(SIZE * .75)
    woods = [(.36, .27, .19), (.30, .23, .17), (.44, .40, .35), (.52, .48, .42), (.40, .27, .18), (.27, .21, .16),
             (.47, .38, .28), (.34, .31, .28)]
    sw = SIZE // STRIPS
    for i in range(STRIPS):
        tex[:bark_h, i * sw:(i + 1) * sw] = bark(bark_h, sw, woods[i % len(woods)], 100 + i)
    fh = SIZE - bark_h
    fw = int(SIZE * .75)
    tex[bark_h:, :fw] = floor(fh, fw, 300)
    tex[bark_h:, fw:] = feather(fh, SIZE - fw, 400)
    Image.fromarray((tex * 255).astype(np.uint8), "RGB").save(path)
    print("wrote", path)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "EagleNestBaseColor.png")
