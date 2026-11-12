# Generates the HUD artwork of Caucasian Eagle: one straight-alpha atlas and the C++ table of its sprites.
#
#   game/assets/ui/Hud/HudAtlas.png   frames, bars, ornaments and icons (drawn at 2x the 720p layout size)
#   game/src/HudAtlas.h               sprite rectangles (generated; do not edit by hand)
#
# Everything is drawn here from shapes and distance fields: no outside images, no fonts.
# Icons are white with their own shading and a black shadow, so the game tints them.
#
# Usage: python art/ui/build_hud.py            (needs Pillow and numpy)
#        python art/ui/build_hud.py --sheet X  also writes a labelled contact sheet to X (for review)
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_PNG = os.path.join(REPO, "game", "assets", "ui", "Hud", "HudAtlas.png")
OUT_H = os.path.join(REPO, "game", "src", "HudAtlas.h")
ATLAS = 1024      # atlas width and height
PAD = 8           # empty texels between sprites (mip safety)
SCALE = 2         # atlas texels per layout unit
SS = 4            # supersampling of drawn shapes

GOLD_LIGHT = np.array([1.00, 0.91, 0.64], np.float32)
GOLD = np.array([0.86, 0.68, 0.34], np.float32)
GOLD_DARK = np.array([0.30, 0.20, 0.08], np.float32)
WELL_TOP = np.array([0.060, 0.105, 0.110], np.float32)
WELL_BOTTOM = np.array([0.014, 0.028, 0.032], np.float32)


# ------------------------------------------------------------------ small tools
def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def grid(w, h):
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    return x + .5, y + .5


def sd_box(w, h, x0, y0, x1, y1, r):
    """Signed distance to a rounded box (negative inside)."""
    x, y = grid(w, h)
    cx, cy, hx, hy = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2 - r, (y1 - y0) / 2 - r
    qx, qy = np.abs(x - cx) - hx, np.abs(y - cy) - hy
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def sd_circle(w, h, cx, cy, r):
    x, y = grid(w, h)
    return np.hypot(x - cx, y - cy) - r


def cover(d):
    """Antialiased coverage of the inside of a distance field."""
    return np.clip(.5 - d, 0, 1)


def band(d, a, b):
    """Coverage of the band a <= d <= b."""
    return np.clip(np.minimum(d - a, b - d) + .5, 0, 1)


class Layer:
    """Premultiplied RGBA float canvas."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.rgb = np.zeros((h, w, 3), np.float32)
        self.a = np.zeros((h, w), np.float32)

    def over(self, color, alpha):
        color = np.asarray(color, np.float32)
        if color.ndim == 1:
            color = np.broadcast_to(color, (self.h, self.w, 3))
        alpha = np.clip(alpha, 0, 1)
        self.rgb = color * alpha[..., None] + self.rgb * (1 - alpha[..., None])
        self.a = alpha + self.a * (1 - alpha)

    def add(self, color, alpha):
        """Additive light on what is already there (keeps coverage)."""
        self.rgb = np.clip(self.rgb + np.asarray(color, np.float32) * np.clip(alpha, 0, 1)[..., None] * self.a[..., None], 0, 1)


def vgrad(w, h, top, bottom, y0=0, y1=None):
    y1 = h if y1 is None else y1
    t = np.clip((np.arange(h, dtype=np.float32) + .5 - y0) / max(1, y1 - y0), 0, 1)[:, None, None]
    return np.broadcast_to(np.asarray(top, np.float32) * (1 - t) + np.asarray(bottom, np.float32) * t, (h, w, 3))


def bevel(d, light=(-.55, -.83)):
    """Lighting 0..1 of a rim from the slope of its distance field (light from the upper left)."""
    gy, gx = np.gradient(d)
    n = np.hypot(gx, gy) + 1e-6
    return np.clip((gx / n * light[0] + gy / n * light[1]) * .5 + .5, 0, 1)


def metal(d, a, b):
    """Gold rim colours for the band a..b of d: bevelled, brighter along its middle."""
    lit = bevel(d)[..., None]
    mid = (1 - np.abs((d - (a + b) / 2) / ((b - a) / 2)) ** 2).clip(0, 1)[..., None]
    base = GOLD_DARK * (1 - lit) + GOLD_LIGHT * lit
    return base * (.62 + .38 * mid)


# ------------------------------------------------------------------ frames
def panel():
    w = h = 128; m = 16
    L = Layer(w, h)
    d = sd_box(w, h, m, m, w - m, h - m, 5)
    L.over((0, 0, 0), .60 * (1 - smooth(0, 15, sd_box(w, h, m, m + 4, w - m, h - m + 4, 5))))      # drop shadow
    L.over(vgrad(w, h, WELL_TOP * .95, WELL_BOTTOM, m, h - m), .92 * cover(d))                       # glass body
    L.over((1, 1, 1), .045 * cover(d) * (1 - smooth(m, m + 34, grid(w, h)[1])))                     # light from above
    L.over(metal(d, -2.2, 0) * .85, band(d, -2.2, 0))                                                # outer rim
    L.over(GOLD, .30 * band(d, -7.2, -6.0))                                                          # inner hairline
    x, y = grid(w, h)
    for cx in (m + 6.6, w - m - 6.6):
        for cy in (m + 6.6, h - m - 6.6):
            dd = np.abs(x - cx) + np.abs(y - cy) - 4.6
            L.over(WELL_BOTTOM, cover(dd - 1.6)); L.over(GOLD_LIGHT, cover(dd))                      # corner studs
    return L, dict(border=40, margin=m)


def slot():
    w = h = 144; m = 12
    L = Layer(w, h)
    d = sd_box(w, h, m, m, w - m, h - m, 16)
    L.over((0, 0, 0), .70 * (1 - smooth(0, 12, sd_box(w, h, m, m + 3, w - m, h - m + 3, 16))))
    L.over(vgrad(w, h, WELL_TOP, WELL_BOTTOM, m, h - m), cover(d))
    L.over((0, 0, 0), .55 * cover(d) * (1 - smooth(-22, -7, d)))                                     # shadow under the rim
    L.over((1, 1, 1), .05 * cover(d + 9) * (1 - smooth(m + 8, m + 60, grid(w, h)[1])))               # gloss
    L.over(metal(d, -7, 0), band(d, -7, 0))
    L.over((.05, .035, .015), .9 * band(d, -8.6, -7))
    L.over((0, 0, 0), .55 * band(d, 0, 1.4))
    return L, dict(border=0, margin=m)


def slot_glow():
    w = h = 176; m = 28
    L = Layer(w, h)
    d = sd_box(w, h, m, m, w - m, h - m, 16)
    glow = np.exp(-(np.maximum(d, 0) / 11.0) ** 2) * (d > -9) * .85 + .9 * band(d, -9, -6.5)
    L.over((1, 1, 1), np.clip(glow * smooth(-11, -7, d) + .9 * band(d, -9, -6.2), 0, 1))
    return L, dict(border=0, margin=m)


def medallion():
    w = h = 176; m = 8; c = w / 2; r = c - m
    L = Layer(w, h)
    d = sd_circle(w, h, c, c, r)
    L.over((0, 0, 0), .65 * (1 - smooth(0, 9, sd_circle(w, h, c, c + 3, r))))
    x, y = grid(w, h)
    rad = np.clip(np.hypot(x - c, y - c * .86) / r, 0, 1)[..., None]
    L.over(np.array([.085, .200, .190], np.float32) * (1 - rad) + WELL_BOTTOM * rad, cover(d))
    L.over((0, 0, 0), .6 * cover(d) * (1 - smooth(-26, -9, d)))
    L.over(metal(d, -10, 0), band(d, -10, 0))
    L.over((.05, .035, .015), .9 * band(d, -11.6, -10))
    L.over(GOLD, .55 * band(d, -16.5, -15.2))
    L.over((0, 0, 0), .55 * band(d, 0, 1.4))
    return L, dict(border=0, margin=m)


def bar_frame(height, rim):
    m = 8; w = 96; h = height + 2 * m
    L = Layer(w, h)
    d = sd_box(w, h, m, m, w - m, h - m, 5)
    L.over((0, 0, 0), .60 * (1 - smooth(0, 8, sd_box(w, h, m, m + 2, w - m, h - m + 2, 5))) * (1 - cover(d)))  # shadow outside only: the middle is a window
    L.over(metal(d, -rim, 0) * .92, band(d, -rim, 0))
    L.over((0, 0, 0), .80 * band(d, -rim - 1.6, -rim))                                               # dark lip inside the rim
    L.over((0, 0, 0), .35 * band(d, -rim - 5, -rim - 1.6) * (grid(w, h)[1] < h / 2))                 # shadow on the fill
    L.over((0, 0, 0), .55 * band(d, 0, 1.2))
    return L, dict(border=24, margin=m)


def bar_fill():
    w, h = 16, 32
    L = Layer(w, h)
    t = (np.arange(h, dtype=np.float32) + .5) / h
    v = .62 + .38 * np.exp(-((t - .24) / .16) ** 2) - .22 * smooth(.55, 1, t)
    L.over(np.broadcast_to(v[:, None, None], (h, w, 3)), np.ones((h, w), np.float32))
    return L, dict(border=0, margin=0)


def keycap():
    w, h = 64, 56; m = 4
    L = Layer(w, h)
    d = sd_box(w, h, m, m, w - m, h - m, 9)
    top = sd_box(w, h, m + 3, m + 2, w - m - 3, h - m - 7, 7)
    L.over((0, 0, 0), .55 * (1 - smooth(0, 4, d)))
    L.over((.030, .040, .044), cover(d))                                                             # skirt of the key
    L.over(vgrad(w, h, (.30, .34, .35), (.15, .18, .19), m, h - m), cover(top))
    L.over((1, 1, 1), .28 * band(top, -1.4, 0) * (grid(w, h)[1] < h * .45))
    return L, dict(border=22, margin=m)


def divider():
    w, h = 512, 32; c = w / 2
    L = Layer(w, h)
    x, y = grid(w, h)
    fall = np.clip(1 - np.abs(x - c) / c, 0, 1) ** 1.4
    L.over(GOLD_LIGHT, fall * np.clip(1.1 - np.abs(y - h / 2), 0, 1) * (np.abs(x - c) > 20))
    for off, size in ((0, 8.5), (-27, 3.6), (27, 3.6)):
        dd = np.abs(x - c - off) + np.abs(y - h / 2) - size
        L.over(GOLD_LIGHT * .98, cover(dd))
        if off == 0:
            L.over(WELL_BOTTOM, cover(dd + 3.2)); L.over(GOLD_LIGHT, cover(dd + 5.6))
    return L, dict(border=0, margin=0)


def glow():
    w = h = 128
    L = Layer(w, h)
    r = np.clip(sd_circle(w, h, w / 2, h / 2, 0) / (w / 2), 0, 1)
    L.over((1, 1, 1), (1 - r) ** 2.2)
    return L, dict(border=0, margin=0)


def grad_h():
    w, h = 128, 8
    L = Layer(w, h)
    L.over((1, 1, 1), np.broadcast_to((1 - (np.arange(w, dtype=np.float32) + .5) / w) ** 1.5, (h, w)).copy())
    return L, dict(border=0, margin=0)


def grad_v():
    w, h = 8, 128
    L = Layer(w, h)
    L.over((1, 1, 1), np.broadcast_to((((np.arange(h, dtype=np.float32) + .5) / h) ** 1.6)[:, None], (h, w)).copy())
    return L, dict(border=0, margin=0)


def vignette():
    w = h = 256
    L = Layer(w, h)
    x, y = grid(w, h)
    r = np.hypot((x - w / 2) / (w / 2), (y - h / 2) / (h / 2))
    L.over((1, 1, 1), smooth(.55, 1.38, r) ** 1.3)
    return L, dict(border=0, margin=0)


def diamond():
    w = h = 32
    L = Layer(w, h)
    x, y = grid(w, h)
    d = np.abs(x - 16) + np.abs(y - 16) - 11
    L.over((0, 0, 0), .6 * (1 - smooth(0, 4, d)))
    L.over(metal(d / 1.41, -8, 0) * 1.1, cover(d))
    return L, dict(border=0, margin=0)


def pip():
    w = h = 32
    L = Layer(w, h)
    d = sd_circle(w, h, 16, 16, 9)
    L.over((0, 0, 0), .6 * (1 - smooth(0, 4, d)))
    L.over(vgrad(w, h, (1, 1, 1), (.62, .62, .62), 7, 25), cover(d))
    return L, dict(border=0, margin=0)


def marker():
    w = h = 64
    L = Layer(w, h)
    x, y = grid(w, h)
    r = np.clip(np.hypot(x - 32, y - 32) / 32, 0, 1)
    L.over(GOLD_LIGHT, .75 * (1 - r) ** 2)
    d = np.abs(x - 32) + np.abs(y - 32) - 12
    L.over((.10, .06, .02), cover(d - 2)); L.over(metal(d / 1.41, -8.5, 0) * 1.25, cover(d))
    return L, dict(border=0, margin=0)


def corner():
    """Filigree corner (upper left); the game turns it for the other three."""
    n = 128; S = n * SS
    img = Image.new("L", (S, S), 0); dr = ImageDraw.Draw(img)
    def ln(pts, wd): dr.line([(x * SS, y * SS) for x, y in pts], fill=255, width=int(wd * SS), joint="curve")
    def arc(cx, cy, r, a0, a1, wd): dr.arc([(cx - r) * SS, (cy - r) * SS, (cx + r) * SS, (cy + r) * SS], a0, a1, fill=255, width=int(wd * SS))
    ln([(10, 118), (10, 22), (22, 10), (118, 10)], 2.4)            # outer line with a cut corner
    ln([(20, 96), (20, 30), (30, 20), (96, 20)], 1.3)              # inner line
    arc(42, 42, 14, 180, 450, 2.0); arc(42, 42, 7, 0, 250, 1.6)    # curl
    arc(74, 30, 9, 90, 300, 1.5); arc(30, 74, 9, 150, 360, 1.5)    # small curls along the arms
    for cx, cy, r in ((10, 124, 3.4), (124, 10, 3.4), (16, 16, 4.6)):
        dr.polygon([((cx - r) * SS, cy * SS), (cx * SS, (cy - r) * SS), ((cx + r) * SS, cy * SS), (cx * SS, (cy + r) * SS)], fill=255)
    m = np.asarray(img.resize((n, n), Image.LANCZOS), np.float32) / 255
    L = Layer(n, n)
    sh = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2.2)), np.float32) / 255
    L.over((0, 0, 0), .7 * sh)
    L.over(vgrad(n, n, GOLD_LIGHT, GOLD), m)
    return L, dict(border=0, margin=0)


# ------------------------------------------------------------------ icons
class Shape:
    """A white shape drawn in the unit square, supersampled."""

    def __init__(self, n=96):
        self.n = n; self.S = n * SS
        self.img = Image.new("L", (self.S, self.S), 0)
        self.d = ImageDraw.Draw(self.img)

    def P(self, pts): return [(x * self.S, y * self.S) for x, y in pts]
    def poly(self, pts, fill=255): self.d.polygon(self.P(pts), fill=fill); return self
    def circle(self, cx, cy, r, fill=255):
        self.d.ellipse([(cx - r) * self.S, (cy - r) * self.S, (cx + r) * self.S, (cy + r) * self.S], fill=fill); return self
    def line(self, pts, wd, fill=255):
        self.d.line(self.P(pts), fill=fill, width=max(1, int(wd * self.S)), joint="curve")
        for x, y in (pts[0], pts[-1]): self.circle(x, y, wd / 2, fill)
        return self
    def arc(self, cx, cy, r, a0, a1, wd, fill=255):
        self.d.arc([(cx - r) * self.S, (cy - r) * self.S, (cx + r) * self.S, (cy + r) * self.S], a0, a1, fill=fill, width=max(1, int(wd * self.S)))
        return self
    def arr(self): return np.asarray(self.img.resize((self.n, self.n), Image.LANCZOS), np.float32) / 255


def rot(pts, deg, c=(.5, .5)):
    a = math.radians(deg); ca, sa = math.cos(a), math.sin(a)
    return [(c[0] + (x - c[0]) * ca - (y - c[1]) * sa, c[1] + (x - c[0]) * sa + (y - c[1]) * ca) for x, y in pts]


def lens(p0, p1, left, right, power=1.0, steps=28):
    """Leaf / blade outline from p0 to p1, bulging 'left' and 'right' of the line."""
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    ln = math.hypot(dx, dy); nx, ny = -dy / ln, dx / ln
    a, b = [], []
    for i in range(steps + 1):
        t = i / steps; s = math.sin(math.pi * t) ** power
        x, y = p0[0] + dx * t, p0[1] + dy * t
        a.append((x + nx * left * s, y + ny * left * s)); b.append((x - nx * right * s, y - ny * right * s))
    return a + b[::-1]


def crescent(cx, cy, r, ox, oy, r2, deg=0):
    a = Shape().circle(cx, cy, r).arr(); b = Shape().circle(cx + ox, cy + oy, r2).arr()
    m = a * (1 - b)
    if deg:
        m = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).rotate(deg, Image.BICUBIC), np.float32) / 255
    return m


def icon_slash():
    return crescent(.50, .52, .40, -.13, -.11, .40, 12)


def icon_heavy():
    s = Shape()
    s.poly(lens((.50, .06), (.50, .70), .085, .085, .55))                 # blade, point up
    s.poly([(.30, .66), (.70, .66), (.66, .735), (.34, .735)])            # guard
    s.poly([(.455, .72), (.545, .72), (.535, .90), (.465, .90)])          # grip
    s.circle(.50, .92, .055)
    m = s.arr()
    arcs = Shape().arc(.50, .34, .40, 200, 250, .045).arc(.50, .34, .40, 290, 340, .045).arr()
    return np.maximum(m, arcs)


def icon_dash():
    s = Shape()
    for k, (dx, ln) in enumerate(((-.21, .62), (0, .78), (.21, .62))):
        s.poly(rot(lens((.5 + dx, .5 - ln / 2), (.5 + dx, .5 + ln / 2), .075, .028, .8), 28))
    return s.arr()


def icon_storm():
    m = np.zeros((96, 96), np.float32)
    for k in range(3):
        m = np.maximum(m, crescent(.50, .34, .235, .075, .035, .225, 120 * k))
    return np.maximum(m, Shape().circle(.5, .5, .055).arr())


def icon_leap():
    m = crescent(.50, .30, .215, .095, -.03, .195, 0)
    s = Shape()
    for y in (.60, .78):
        s.line([(.26, y + .11), (.50, y - .07), (.74, y + .11)], .075)
    return np.maximum(m, s.arr())


def icon_swap():
    s = Shape()
    s.arc(.5, .5, .34, 200, 330, .085).arc(.5, .5, .34, 20, 150, .085)
    s.poly(rot([(.84, .22), (.90, .52), (.63, .44)], 0))
    s.poly(rot([(.16, .78), (.10, .48), (.37, .56)], 0))
    return s.arr()


def icon_guard():
    outer = [(.50, .07), (.84, .19), (.84, .50), (.74, .72), (.50, .94), (.26, .72), (.16, .50), (.16, .19)]
    inner = [(.50, .20), (.72, .28), (.72, .50), (.65, .66), (.50, .79), (.35, .66), (.28, .50), (.28, .28)]
    m = Shape().poly(outer).arr() * (1 - .82 * Shape().poly(inner).arr())
    return np.maximum(m, Shape().poly(lens((.50, .30), (.50, .70), .07, .07, .7)).arr())


def icon_boost():
    s = Shape()
    root = (.20, .78)
    for k, (deg, ln, wd) in enumerate(((-78, .70, .075), (-58, .76, .08), (-38, .70, .08), (-18, .58, .075), (2, .44, .07))):
        a = math.radians(deg); tip = (root[0] + math.cos(a) * ln, root[1] + math.sin(a) * ln)
        s.poly(lens(root, tip, wd, wd * .55, .75))
    s.circle(root[0] + .03, root[1] - .02, .085)
    return s.arr()


def icon_coin():
    m = Shape().circle(.5, .5, .40).arr()
    m = m * (1 - .78 * Shape().circle(.5, .5, .315).arr()) + .0
    m = np.maximum(m, .55 * Shape().circle(.5, .5, .30).arr())
    star = Shape().poly([(.5, .26), (.57, .43), (.74, .5), (.57, .57), (.5, .74), (.43, .57), (.26, .5), (.43, .43)]).arr()
    return np.maximum(m, star)


def icon_feather():
    s = Shape().poly(rot(lens((.5, .06), (.5, .80), .19, .19, .62), 38))
    m = s.arr()
    cut = Shape()
    cut.line(rot([(.5, .10), (.5, .97)], 38), .028)
    for t, side in ((.36, 1), (.52, -1), (.66, 1)):
        cut.line(rot([(.5, t), (.5 + side * .24, t - .13)], 38), .03)
    quill = Shape().line(rot([(.5, .74), (.5, .97)], 38), .04).arr()
    return np.maximum(m * (1 - cut.arr()), quill)


def icon_eye():
    m = Shape().poly(lens((.06, .5), (.94, .5), .30, .30, .85)).arr()
    m = m * (1 - .85 * Shape().poly(lens((.16, .5), (.84, .5), .215, .215, .85)).arr())
    iris = Shape().circle(.5, .5, .165).arr() * (1 - Shape().circle(.5, .5, .07).arr())
    return np.maximum(m, iris)


def icon_tear():
    s = Shape().circle(.5, .62, .26)
    s.poly([(.5, .06), (.735, .50), (.265, .50)])
    m = s.arr()
    return m * (1 - .7 * crescent(.5, .62, .17, .07, -.06, .17, 0))


def icon_ember():
    s = Shape()
    s.poly(lens((.50, .95), (.56, .05), .30, .26, .72))
    m = s.arr()
    inner = Shape().poly(lens((.50, .93), (.46, .42), .15, .13, .8)).arr()
    return m * (1 - .78 * inner)


def icon_chest():
    s = Shape()
    s.poly([(.12, .46), (.88, .46), (.88, .84), (.12, .84)])
    s.poly([(.12, .42), (.16, .26), (.30, .17), (.70, .17), (.84, .26), (.88, .42)])
    m = s.arr()
    cut = Shape().poly([(.10, .415), (.90, .415), (.90, .465), (.10, .465)]).arr()
    for x in (.27, .73):
        cut = np.maximum(cut, Shape().poly([(x - .022, .17), (x + .022, .17), (x + .022, .84), (x - .022, .84)]).arr() * .8)
    lock = Shape().poly([(.44, .40), (.56, .40), (.56, .58), (.50, .63), (.44, .58)]).arr()
    return np.maximum(m * (1 - cut), lock)


def icon_book():
    s = Shape()
    s.poly([(.50, .26), (.50, .84), (.10, .76), (.10, .18), (.30, .17)])
    s.poly([(.50, .26), (.50, .84), (.90, .76), (.90, .18), (.70, .17)])
    m = s.arr()
    cut = Shape().line([(.5, .25), (.5, .86)], .03)
    for y in (.36, .48, .60):
        cut.line([(.19, y - .045), (.42, y + .01)], .026); cut.line([(.58, y + .01), (.81, y - .045)], .026)
    return m * (1 - cut.arr())


def icon_skull():
    s = Shape().circle(.5, .42, .31)
    s.poly([(.33, .60), (.67, .60), (.64, .88), (.36, .88)])
    m = s.arr()
    cut = Shape().circle(.385, .44, .085).circle(.615, .44, .085)
    cut.poly([(.5, .54), (.545, .64), (.455, .64)])
    for x in (.44, .50, .56):
        cut.line([(x, .78), (x, .90)], .02)
    return m * (1 - cut.arr())


def icon_mountain():
    s = Shape()
    s.poly([(.04, .86), (.36, .30), (.52, .56), (.66, .14), (.96, .86)])
    m = s.arr()
    cut = Shape().line([(.555, .40), (.62, .34), (.67, .42), (.73, .35), (.78, .43)], .035).arr()
    return m * (1 - cut)


def icon_wings():
    """Emblem of the eagle's daughter: two wings and a blade."""
    n = 128
    s = Shape(n)
    for side in (-1, 1):
        root = (.5 + side * .07, .60)
        for deg, ln, wd in ((-20, .44, .06), (-38, .47, .065), (-56, .45, .065), (-74, .39, .06), (4, .34, .055)):
            a = math.radians(deg); tip = (root[0] + side * math.cos(a) * ln, root[1] + math.sin(a) * ln)
            s.poly(lens(root, tip, wd if side > 0 else wd * .5, wd * .5 if side > 0 else wd, .7))
    s.poly(lens((.5, .12), (.5, .80), .05, .05, .5))
    s.poly([(.40, .76), (.60, .76), (.58, .81), (.42, .81)])
    s.poly([(.475, .80), (.525, .80), (.52, .92), (.48, .92)])
    return s.arr()


# ------------------------------------------------------------------ map
def pixel():
    """Solid white; the game draws lines with its middle (Ui::Line)."""
    L = Layer(16, 16)
    L.over((1, 1, 1), np.ones((16, 16), np.float32))
    return L, dict(border=0, margin=0)


def map_disc():
    """The dark glass of the minimap."""
    w = h = 192; m = 8; c = w / 2; r = c - m
    L = Layer(w, h)
    d = sd_circle(w, h, c, c, r)
    x, y = grid(w, h)
    rad = np.clip(np.hypot(x - c, y - c) / r, 0, 1)[..., None]
    L.over((0, 0, 0), .55 * (1 - smooth(0, 8, sd_circle(w, h, c, c + 3, r))))
    L.over(np.array([.050, .105, .105], np.float32) * (1 - rad) + WELL_BOTTOM * rad, .90 * cover(d))
    return L, dict(border=0, margin=m)


def map_rim():
    """The gold ring over the minimap, with a notch at the top (the way she looks)."""
    w = h = 352; m = 16; c = w / 2; r = c - m
    L = Layer(w, h)
    d = sd_circle(w, h, c, c, r)
    L.over((0, 0, 0), .55 * band(d, -16, -7) * smooth(-16, -7, d))
    L.over(metal(d, -7, 0), band(d, -7, 0))
    L.over((.05, .035, .015), .9 * band(d, -8.6, -7))
    L.over(GOLD, .45 * band(d, -12.5, -11.5))
    L.over((0, 0, 0), .55 * band(d, 0, 1.6))
    x, y = grid(w, h)
    dd = np.abs(x - c) + np.abs(y - (m + 3)) - 9
    L.over((.10, .06, .02), cover(dd - 2)); L.over(metal(dd / 1.41, -7, 0) * 1.25, cover(dd))
    return L, dict(border=0, margin=m)


def icon_arrow():
    return Shape().poly([(.5, .06), (.84, .90), (.5, .70), (.16, .90)]).arr()


def icon_chevron():
    return Shape().line([(.16, .66), (.5, .30), (.84, .66)], .13).arr()


def icon_swords():
    s = Shape()
    for deg in (-42, 42):
        s.poly(rot(lens((.5, .04), (.5, .74), .06, .06, .5), deg))
        s.poly(rot([(.38, .72), (.62, .72), (.60, .78), (.40, .78)], deg))
        s.poly(rot([(.47, .77), (.53, .77), (.53, .93), (.47, .93)], deg))
    return s.arr()


def icon_grapes():                      # Telavi: the wine of Kakheti
    s = Shape()
    for row, count in enumerate((4, 3, 2, 1)):
        for k in range(count):
            s.circle(.5 + (k - (count - 1) / 2) * .19, .40 + row * .16, .10)
    s.line([(.5, .30), (.5, .12), (.66, .08)], .045)
    s.poly(lens((.50, .22), (.22, .10), .09, .05, .8))
    return s.arr()


def icon_baths():                       # Tbilisi: the domes of the warm baths
    s = Shape()
    for cx, r in ((.27, .17), (.5, .25), (.73, .17)):
        s.circle(cx, .62, r)
    m = s.arr() * Shape().poly([(0, 0), (1, 0), (1, .64), (0, .64)]).arr()
    base = Shape().poly([(.06, .66), (.94, .66), (.94, .80), (.06, .80)])
    for cx in (.34, .5, .66):           # steam
        base.line([(cx, .30 - (.12 if cx == .5 else 0)), (cx + .04, .22 - (.12 if cx == .5 else 0)), (cx - .03, .14 - (.12 if cx == .5 else 0))], .035)
    return np.maximum(m, base.arr())


def icon_cross():                       # Mtskheta: the cross of the first church
    s = Shape()
    for deg in (0, 90, 180, 270):
        s.poly(rot([(.44, .5), (.56, .5), (.66, .10), (.34, .10)], deg))
    s.circle(.5, .5, .10)
    return s.arr()


def icon_fort():                        # Gori: the fortress on the rock
    s = Shape()
    s.poly([(.14, .88), (.24, .46), (.76, .46), (.86, .88)])
    for k in range(4):
        x = .24 + k * .145
        s.poly([(x, .46), (x + .085, .46), (x + .085, .32), (x, .32)])
    m = s.arr()
    cut = Shape().circle(.5, .74, .09).poly([(.41, .74), (.59, .74), (.59, .88), (.41, .88)]).arr()
    return m * (1 - cut)


def icon_fleece():                      # Kutaisi: the Golden Fleece of Colchis
    s = Shape()
    for cx, cy, r in ((.5, .44, .22), (.34, .40, .15), (.66, .40, .15), (.40, .58, .15), (.60, .58, .15), (.5, .64, .15)):
        s.circle(cx, cy, r)
    for x0, x1 in ((.30, .20), (.70, .80)):
        s.line([(x0, .34), (x1, .16)], .08); s.line([(x0 + (.04 if x0 < .5 else -.04), .66), (x1, .88)], .08)
    s.line([(.5, .22), (.5, .08)], .10)
    return s.arr()


def icon_anchor():                      # Batumi: the port on the Black Sea
    s = Shape()
    s.line([(.5, .26), (.5, .84)], .075); s.line([(.34, .40), (.66, .40)], .065)
    s.arc(.5, .56, .30, 20, 160, .075)
    s.poly([(.14, .56), (.30, .58), (.20, .72)]); s.poly([(.86, .56), (.70, .58), (.80, .72)])
    m = s.arr()
    ring = Shape().circle(.5, .17, .10).arr() * (1 - Shape().circle(.5, .17, .045).arr())
    return np.maximum(m, ring)


def icon_tower():                       # Mestia: the towers of Svaneti
    s = Shape()
    s.poly([(.36, .90), (.40, .30), (.60, .30), (.64, .90)])
    s.poly([(.35, .30), (.65, .30), (.65, .18), (.35, .18)])
    s.poly([(.31, .18), (.5, .07), (.69, .18)])
    m = s.arr()
    cut = Shape()
    for y in (.44, .62): cut.poly([(.48, y), (.52, y), (.52, y + .09), (.48, y + .09)])
    cut.poly([(.40, .22), (.44, .22), (.44, .27), (.40, .27)]); cut.poly([(.56, .22), (.60, .22), (.60, .27), (.56, .27)])
    return m * (1 - cut.arr())


def icon_crystal():                     # what creatures leave: a cut crystal
    m = Shape().poly([(.50, .04), (.74, .30), (.66, .78), (.50, .96), (.34, .78), (.26, .30)]).arr()
    cut = Shape().line([(.26, .30), (.50, .42), (.74, .30)], .028).line([(.50, .42), (.50, .96)], .028).line([(.34, .78), (.50, .42), (.66, .78)], .022).arr()
    return m * (1 - .7 * cut)


def icon_potion():                      # a round flask with a stopper
    s = Shape()
    s.circle(.50, .62, .30)
    s.poly([(.41, .14), (.59, .14), (.59, .40), (.41, .40)])
    s.poly([(.35, .08), (.65, .08), (.65, .17), (.35, .17)])
    m = s.arr()
    return m * (1 - .6 * Shape().poly(lens((.34, .56), (.50, .44), .05, .03, .8)).arr())


def icon_armour():                      # a breastplate
    m = Shape().poly([(.22, .12), (.38, .08), (.50, .17), (.62, .08), (.78, .12), (.90, .30), (.76, .42), (.74, .82), (.50, .94), (.26, .82), (.24, .42), (.10, .30)]).arr()
    cut = Shape().line([(.50, .26), (.50, .88)], .03).line([(.30, .52), (.50, .60), (.70, .52)], .026).line([(.30, .70), (.50, .78), (.70, .70)], .026).arr()
    return m * (1 - .7 * cut)


def shade_icon(m):
    n = m.shape[0]
    L = Layer(n, n)
    sh = Image.fromarray((np.clip(m, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(n / 40))
    sh = np.roll(np.asarray(sh, np.float32) / 255, max(1, n // 48), axis=0)
    L.over((0, 0, 0), .85 * np.clip(sh * 1.5, 0, 1))
    L.over(vgrad(n, n, (1, 1, 1), (.74, .74, .74), n * .12, n * .92), np.clip(m, 0, 1))
    return L, dict(border=0, margin=0)


SPRITES = [
    ("Panel", panel), ("Slot", slot), ("SlotGlow", slot_glow), ("Medallion", medallion),
    ("BarFrame", lambda: bar_frame(32, 4.0)), ("BarFrameThin", lambda: bar_frame(18, 2.6)), ("BarFill", bar_fill),
    ("Keycap", keycap), ("Divider", divider), ("Glow", glow), ("GradH", grad_h), ("GradV", grad_v),
    ("Vignette", vignette), ("Diamond", diamond), ("Pip", pip), ("Marker", marker), ("Corner", corner),
    ("Pixel", pixel), ("MapDisc", map_disc), ("MapRim", map_rim),
] + [(name, (lambda f=f: shade_icon(f()))) for name, f in [
    ("IconSlash", icon_slash), ("IconHeavy", icon_heavy), ("IconDash", icon_dash), ("IconStorm", icon_storm),
    ("IconLeap", icon_leap), ("IconSwap", icon_swap), ("IconGuard", icon_guard), ("IconBoost", icon_boost),
    ("IconCoin", icon_coin), ("IconFeather", icon_feather), ("IconEye", icon_eye), ("IconTear", icon_tear),
    ("IconEmber", icon_ember), ("IconChest", icon_chest), ("IconBook", icon_book), ("IconSkull", icon_skull),
    ("IconMountain", icon_mountain), ("IconWings", icon_wings),
    ("IconArrow", icon_arrow), ("IconChevron", icon_chevron), ("IconSwords", icon_swords),
    ("IconGrapes", icon_grapes), ("IconBaths", icon_baths), ("IconCross", icon_cross), ("IconFort", icon_fort),
    ("IconFleece", icon_fleece), ("IconAnchor", icon_anchor), ("IconTower", icon_tower),
    ("IconCrystal", icon_crystal), ("IconPotion", icon_potion), ("IconArmour", icon_armour)]]


def main():
    made = [(name, *make()) for name, make in SPRITES]
    order = sorted(range(len(made)), key=lambda i: -made[i][1].h)
    rgb = np.zeros((ATLAS, ATLAS, 3), np.float32); alpha = np.zeros((ATLAS, ATLAS), np.float32)
    place = {}
    x = y = PAD; row = 0
    for i in order:
        name, L, info = made[i]
        if x + L.w + PAD > ATLAS: x = PAD; y += row + PAD; row = 0
        if y + L.h + PAD > ATLAS: raise SystemExit("atlas is full: enlarge ATLAS")
        rgb[y:y + L.h, x:x + L.w] = L.rgb; alpha[y:y + L.h, x:x + L.w] = L.a
        place[name] = (x, y, L.w, L.h, info["border"], info["margin"])
        x += L.w + PAD; row = max(row, L.h)

    # straight alpha; colour is carried into the empty texels around every shape so mips and
    # bilinear sampling never pull black into an edge
    straight = np.where(alpha[..., None] > 1e-4, rgb / np.maximum(alpha[..., None], 1e-4), 0)
    known = alpha > 1e-4
    for _ in range(12):
        img = Image.fromarray((np.clip(straight, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.BoxBlur(1))
        k = Image.fromarray((known * 255).astype(np.uint8)).filter(ImageFilter.BoxBlur(1))
        blurred = np.asarray(img, np.float32) / 255; weight = np.asarray(k, np.float32) / 255
        grow = (~known) & (weight > .01)
        straight[grow] = blurred[grow] / weight[grow][:, None]
        known = known | grow
    out = np.dstack([np.clip(straight, 0, 1), np.clip(alpha, 0, 1)])
    os.makedirs(os.path.dirname(OUT_PNG), exist_ok=True)
    Image.fromarray((out * 255 + .5).astype(np.uint8), "RGBA").save(OUT_PNG, optimize=True)

    lines = [
        "#pragma once", "",
        "// Generated by art/ui/build_hud.py. Do not edit: change the script and run it again.",
        "// Sprite rectangles of assets/ui/Hud/HudAtlas.png, in texels.",
        "//   Border  texels of a nine-slice (or three-slice) edge, counted from the sprite's edge; 0 = not sliced",
        "//   Margin  texels of soft shadow around the solid shape (drawn outside the rectangle the game asks for)", "",
        "namespace CaucasianEagle::HudArt", "{",
        "    struct Sprite { float X, Y, W, H, Border, Margin; };",
        f"    inline constexpr float TexelsPerUnit = {SCALE}.f; // atlas texels per layout unit (1 unit = 1 px at 1280x720)",
        '    inline constexpr const char* AtlasPath = "assets/ui/Hud/HudAtlas.png";', ""]
    for name, _ in SPRITES:
        p = place[name]
        lines.append(f"    inline constexpr Sprite {name}{{{p[0]}, {p[1]}, {p[2]}, {p[3]}, {p[4]}, {p[5]}}};")
    lines += ["}", ""]
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print(f"HudAtlas.png {ATLAS}x{ATLAS}, {len(SPRITES)} sprites, lowest row ends at {y + row}")

    if "--sheet" in sys.argv:
        path = sys.argv[sys.argv.index("--sheet") + 1]
        im = Image.fromarray((out * 255 + .5).astype(np.uint8), "RGBA")
        bg = Image.new("RGBA", im.size, (38, 62, 66, 255)); bg.alpha_composite(im)
        bg.convert("RGB").save(path)


if __name__ == "__main__":
    main()
