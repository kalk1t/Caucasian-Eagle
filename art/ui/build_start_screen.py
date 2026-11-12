"""Paints the picture behind the start screen: game/assets/ui/Menu/StartScreen.png (1920 x 1080).

A night over the Caucasus: stars, a large moon, three ranges of mountains with snow, mist between them,
the mountain of Amirani, and the Eagle with spread wings in front of the moon. The title is lettered at the
lower right. The left third is kept dark and quiet: the menu stands there.

Everything is drawn by this script (numpy and Pillow); nothing is downloaded. To use another picture,
replace the PNG: any size works, it is scaled to cover the screen and cropped at the edges.

Usage:  python art/ui/build_start_screen.py
"""
import math, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1920, 1080
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "game", "assets", "ui", "Menu", "StartScreen.png")
RENDER = os.path.join(REPO, "art", "ui", "export", "CoverRender.png")   # the 3D cover, if rendered
rng = np.random.default_rng(1872)                     # fixed: the same picture every run
MOON = (1270, 360, 215)                               # centre x, y and radius
TITLE_X = 740                                         # centre of the title (left of Nyxara in the 3D cover)
TITLE_FONTS = ["C:/Windows/Fonts/pala.ttf", "C:/Windows/Fonts/georgia.ttf", "C:/Windows/Fonts/times.ttf"]


def lerp(a, b, t): return a + (b - a) * t
def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def ridge(base, height, roughness, seed, peak=None):
    """Height of a mountain line for every x (pixels from the top): fractal noise, plus one great peak."""
    r = np.random.default_rng(seed)
    y = np.zeros(W)
    for octave in range(1, 8):
        n = 2 ** octave + 1
        y += np.interp(np.arange(W), np.linspace(0, W, n), r.uniform(-1, 1, n)) * roughness ** octave
    y = y / np.abs(y).max() * height
    if peak:
        px, ph, pw = peak
        x = np.arange(W)
        y += ph * np.maximum(0, 1 - np.abs(x - px) / pw) ** 1.35
    return base - y


def blur(a, radius):
    return np.asarray(Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius)), np.float32) / 255


def lens(p0, p1, left, right, steps=14):
    """A feather: a pointed leaf from p0 to p1."""
    (x0, y0), (x1, y1) = p0, p1
    dx, dy = x1 - x0, y1 - y0
    n = math.hypot(dx, dy); nx, ny = -dy / n, dx / n
    a, b = [], []
    for i in range(steps + 1):
        t = i / steps
        w = math.sin(math.pi * t ** .8)
        a.append((x0 + dx * t + nx * left * n * w, y0 + dy * t + ny * left * n * w))
        b.append((x0 + dx * t - nx * right * n * w, y0 + dy * t - ny * right * n * w))
    return a + b[::-1]


def eagle(scale=2):
    """The Eagle as a mask (1 = bird), wings raised and spread, head turned to the left."""
    img = Image.new("L", (W * scale, H * scale), 0)
    d = ImageDraw.Draw(img)
    cx, cy, s = MOON[0], MOON[1] + 40, 1.0
    P = lambda x, y: ((cx + x * s) * scale, (cy + y * s) * scale)
    poly = lambda pts: d.polygon([P(x, y) for x, y in pts], fill=255)
    for side in (-1, 1):
        root = (side * 34, -22)
        # long flight feathers fan out from the wrist; shorter ones fill in toward the body
        wrist = (side * 240, -130)
        poly([root, (side * 110, -112), (side * 190, -150), wrist, (side * 250, -84), (side * 130, -30), (side * 40, 18)])
        for i in range(10):
            t = i / 9
            start = (wrist[0] - side * 70 * t, wrist[1] + 34 * t)          # along the outer arm, not from one point
            ang = math.radians(lerp(-62, 44, t))
            ln = lerp(205, 215, t) * (1 - .5 * (t - .4) ** 2)
            tip = (start[0] + side * math.cos(ang) * ln, start[1] + math.sin(ang) * ln)
            poly(lens(start, tip, .075, .075))
        for i in range(10):
            t = i / 9
            start = (lerp(root[0], wrist[0], t * .95), lerp(root[1] + 6, wrist[1] + 30, t * .95))
            ang = math.radians(lerp(72, 30, t))
            ln = lerp(120, 175, t)
            poly(lens(start, (start[0] + side * math.cos(ang) * ln * .55, start[1] + math.sin(ang) * ln), .11, .11))
    # body, tail, legs with talons, head with the hooked beak
    poly(lens((0, -70), (0, 150), .22, .22, 20))
    for a in (-24, -12, 0, 12, 24):
        poly(lens((0, 110), (math.sin(math.radians(a)) * 150, 110 + math.cos(math.radians(a)) * 150), .10, .10))
    for side in (-1, 1):
        poly(lens((side * 26, 96), (side * 52, 176), .16, .16))
        for a in (-28, 0, 28):
            poly(lens((side * 52, 172), (side * 52 + math.sin(math.radians(a)) * 40, 172 + math.cos(math.radians(a)) * 40), .14, .14))
    poly(lens((0, -40), (-8, -112), .30, .30, 16))                                  # neck
    d.ellipse([P(-36, -134)[0], P(-36, -134)[1], P(14, -92)[0], P(14, -92)[1]], fill=255)   # a flat-crowned head, turned left
    poly([(-30, -126), (-66, -114), (-80, -100), (-84, -80), (-74, -92), (-58, -96), (-34, -96)])   # hooked beak
    return np.asarray(img.resize((W, H), Image.LANCZOS), np.float32) / 255


def main():
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    # ---- sky: indigo above, a cold green glow toward the horizon
    t = (yy / H)[..., None]
    sky = lerp(np.array([.016, .024, .070]), np.array([.060, .150, .190]), smooth(0, .85, t))
    sky = sky + np.array([.02, .07, .06]) * smooth(.45, .9, t) ** 2
    img = sky.astype(np.float32)
    # ---- stars: many small, a few bright
    stars = np.zeros((H, W), np.float32)
    for count, bright in ((2600, .35), (420, .7), (60, 1.0)):
        sx, sy = rng.integers(0, W, count), (rng.random(count) ** 1.6 * H * .72).astype(int)
        stars[sy, sx] = np.maximum(stars[sy, sx], bright * rng.uniform(.5, 1, count))
    stars = np.maximum(stars, blur(stars, 1.6) * 2.2)
    img += stars[..., None] * np.array([.85, .92, 1.0])
    # ---- moon: a disc with seas, and a wide glow
    mx, my, mr = MOON
    dist = np.hypot(xx - mx, yy - my)
    disc = smooth(mr + 1.5, mr - 1.5, dist)
    seas = np.zeros((H, W), np.float32)
    for _ in range(26):
        ax, ay, ar = mx + rng.uniform(-.7, .7) * mr, my + rng.uniform(-.7, .7) * mr, rng.uniform(.08, .3) * mr
        seas += np.exp(-((xx - ax) ** 2 + (yy - ay) ** 2) / (2 * ar * ar)) * rng.uniform(.3, 1)
    seas = np.clip(seas / seas.max(), 0, 1)
    face = (np.array([.93, .96, .92]) * (1 - .30 * seas[..., None])) * (1 - .22 * smooth(.35 * mr, mr, dist)[..., None])
    glow = np.exp(-np.maximum(dist - mr, 0) / (mr * .55)) * .55 + np.exp(-np.maximum(dist - mr, 0) / (mr * 2.6)) * .28
    img += glow[..., None] * np.array([.42, .78, .80]) * (1 - disc[..., None])
    img = img * (1 - disc[..., None]) + face * disc[..., None]
    # ---- the Eagle against the moon, with the moonlight caught on its edges
    bird = eagle()
    rim = np.clip(blur(bird, 5) - bird * .0, 0, 1) * (1 - bird)
    img += rim[..., None] * np.array([.55, .80, .78]) * .55
    img = img * (1 - bird[..., None]) + np.array([.012, .020, .034]) * bird[..., None]
    lit = np.clip(bird - np.roll(blur(bird, 3), (5, 3), (0, 1)), 0, 1)             # feathers lit from above
    img += lit[..., None] * np.array([.50, .62, .66]) * .5
    # ---- mountains, far to near: lighter and bluer far away, snow on the heights, mist between the ranges
    ranges = [
        dict(base=700, height=120, rough=.62, seed=11, peak=None, rock=(.110, .200, .255), snow=(.52, .66, .72), line=.42),
        dict(base=790, height=150, rough=.60, seed=23, peak=(1290, 330, 520), rock=(.060, .115, .160), snow=(.66, .80, .84), line=.50),
        dict(base=900, height=120, rough=.58, seed=37, peak=None, rock=(.028, .058, .082), snow=(.30, .42, .47), line=.20),
        dict(base=1010, height=90, rough=.55, seed=51, peak=None, rock=(.008, .018, .028), snow=None, line=0),
    ]
    for k, r in enumerate(ranges):
        top = ridge(r["base"], r["height"], r["rough"], r["seed"], r["peak"])
        mask = smooth(-1.2, 1.2, yy - top[None, :])
        depth = yy - top[None, :]
        colour = np.array(r["rock"])[None, None, :] * (1 - .35 * smooth(0, 320, depth))[..., None]
        if r["snow"]:
            slope = np.convolve(np.gradient(top), np.ones(31) / 31, mode="same")     # the lie of the land, not every notch
            light = np.clip(.78 - slope * .30, .50, 1.0)[None, :]                    # slopes that face the moon are brighter
            ragged = np.interp(np.arange(W), np.linspace(0, W, 90), np.random.default_rng(r["seed"] + 1).uniform(-1, 1, 90))[None, :] * 26
            snowline = r["base"] - r["height"] * r["line"] + ragged                     # pixels from the top: snow lies above this
            streak = .72 + .28 * np.sin((xx + depth * (np.sign(slope)[None, :] * .9)) * .09 + np.sin(xx * .013) * 3)
            cover = smooth(snowline + 26, snowline - 34, yy) * smooth(190, 20, depth) * streak
            colour = colour * (1 - cover[..., None]) + np.array(r["snow"])[None, None, :] * light[..., None] * cover[..., None]
        img = img * (1 - mask[..., None]) + colour * mask[..., None]
        if k < len(ranges) - 1:
            foot = ranges[k + 1]["base"] - ranges[k + 1]["height"] * .2
            mist = np.exp(-((yy - foot) / 46) ** 2) * (.5 + .5 * np.sin(xx * .004 + k * 2.1) * np.sin(xx * .0013 + k)) * .34
            img += mist[..., None] * np.array([.38, .60, .62])
    # ---- the chains of Amirani: two faint lines of light down the great peak
    chain = Image.new("L", (W, H), 0)
    cd = ImageDraw.Draw(chain)
    for side in (-1, 1):
        pts = [(1290 + side * (14 + 150 * (i / 40) ** 1.3), 492 + 250 * (i / 40)) for i in range(41)]
        cd.line(pts, fill=255, width=2)
    chain = blur(np.asarray(chain, np.float32) / 255, 1.2)
    img += chain[..., None] * np.array([.80, .62, .30]) * .30
    # ---- quiet and dark on the left for the menu; darker corners everywhere
    img *= (1 - .62 * smooth(900, 80, xx))[..., None]
    img *= (1 - .45 * smooth(.55, 1.15, np.hypot((xx - W * .56) / W, (yy - H * .46) / H) * 1.55))[..., None]
    out = Image.fromarray((np.clip(img, 0, 1) ** (1 / 1.06) * 255).astype(np.uint8), "RGB")

    # ---- the 3D cover (art/ui/render_cover.py), when it has been rendered, replaces the painted picture
    if os.path.exists(RENDER):
        shot = np.asarray(Image.open(RENDER).convert("RGB").resize((W, H), Image.LANCZOS), np.float32) / 255
        shot *= (1 - .72 * smooth(760, 60, xx))[..., None]                       # dark at the left: the menu stands there
        shot *= (1 - .40 * smooth(.62, 1.20, np.hypot((xx - W * .60) / W, (yy - H * .45) / H) * 1.55))[..., None]
        out = Image.fromarray((np.clip(shot, 0, 1) * 255).astype(np.uint8), "RGB")

    # ---- title
    font = next((p for p in TITLE_FONTS if os.path.exists(p)), None)
    def spaced(draw, text, centre, y, size, tracking, fill):
        f = ImageFont.truetype(font, size) if font else ImageFont.load_default()
        widths = [draw.textlength(ch, font=f) for ch in text]
        x = centre - (sum(widths) + tracking * (len(text) - 1)) / 2
        for ch, w in zip(text, widths):
            draw.text((x, y), ch, font=f, fill=fill); x += w + tracking
    shadow = Image.new("L", (W, H), 0)
    sd = ImageDraw.Draw(shadow)
    # The menu panel covers the left third of the screen, so over the 3D cover the title stands small in the sky
    # at the right of the moon; over the painted picture it stands large at the bottom.
    cover = os.path.exists(RENDER)
    tx, ty, size, track = (1648, 76, 42, 5) if cover else (TITLE_X, 862, 84, 10)
    sub_y, sub_size, sub_track, rule = (ty + 62, 17, 8, (196, 250)) if cover else (976, 30, 16, (350, 455))
    spaced(sd, "CAUCASIAN EAGLE", tx, ty + 4, size, track, 255)
    spaced(sd, "THE ROAD TO AMIRANI", tx, sub_y + 2, sub_size, sub_track, 255)
    out = Image.composite(Image.new("RGB", (W, H), (0, 0, 0)), out, shadow.filter(ImageFilter.GaussianBlur(9)).point(lambda v: min(255, int(v * 1.5))))
    letters = Image.new("L", (W, H), 0)
    ld = ImageDraw.Draw(letters)
    spaced(ld, "CAUCASIAN EAGLE", tx, ty, size, track, 255)
    gold = np.zeros((H, W, 3), np.float32)
    gold[...] = lerp(np.array([1.0, .93, .70]), np.array([.80, .58, .24]), smooth(ty - 12, ty + size * 1.2, yy)[..., None])
    out = Image.composite(Image.fromarray((gold * 255).astype(np.uint8), "RGB"), out, letters)
    small = Image.new("L", (W, H), 0)
    spaced(ImageDraw.Draw(small), "THE ROAD TO AMIRANI", tx, sub_y, sub_size, sub_track, 255)
    out = Image.composite(Image.new("RGB", (W, H), (176, 214, 214)), out, small)
    line = ImageDraw.Draw(out)
    for x0, x1 in ((tx - rule[1], tx - rule[0]), (tx + rule[0], tx + rule[1])):
        line.line([(x0, sub_y + sub_size * .6), (x1, sub_y + sub_size * .6)], fill=(150, 118, 60), width=2)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    out.save(OUT, optimize=True)
    print("wrote", OUT, out.size)


if __name__ == "__main__":
    main()
