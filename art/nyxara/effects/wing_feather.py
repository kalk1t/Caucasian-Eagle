"""Nyxara's wing feather, for the feather storm (C): game/assets/Compiled/Effects/Nyxara/WingFeather.png.

    python art/nyxara/effects/wing_feather.py [<out.png>]

A flight feather as her wings have them, drawn from nothing (no source image; CC0 as this repo's own art): long and
narrow, the vane before the shaft narrow and the one behind it wide, a rounded tip, a bare quill at the base, its barbs
slanting to the tip, a few splits in its edge, a white bar across it and a thin one below. The tip is at the top of the
picture (the kit draws a feather with the top of its picture forward, the way it flies). The body is a middle grey and the
bar white, so that the colour the kit gives a feather (dark slate, silver, white) is its body's and the bar stays lighter.
128 x 512, drawn four times as large and made smaller (soft edges).
"""
import math
import os
import random
import sys

from PIL import Image

W, H, SS = 128, 512, 4
BODY, BAR, SHAFT = 150, 248, 225          # grey levels (sRGB)
TIP, VANE_END, QUILL_END = .03, .86, .995  # along the feather, from its tip (0) to the end of its quill (1)


def smooth(a, b, x):
    x = min(1.0, max(0.0, (x - a) / (b - a)))
    return x * x * (3 - 2 * x)


def shaft_x(v):
    return .47 + .05 * (v - .45) ** 2        # (curved a little, as a flight feather's shaft is)


def shaft_half(v):
    return (.004 + .012 * v) if v < VANE_END else .016


def vane(v):
    """How wide each vane is at v (as parts of the picture's width): before the shaft, behind it."""
    if v < TIP or v > VANE_END:
        return 0.0, 0.0
    tip = math.sqrt(smooth(TIP, TIP + .16, v))                       # the rounded tip
    base = 1 - .8 * smooth(.58, VANE_END, v)                         # narrowing into the quill
    return .13 * tip * base, .40 * tip * base * (1 - .12 * smooth(.05, .5, v) * (1 - smooth(.5, .8, v)))


def main(out):
    random.seed(19)
    w, h = W * SS, H * SS
    img = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    px = img.load()
    splits = [(random.uniform(.2, .72), random.choice((-1, 1)), random.uniform(.008, .014)) for _ in range(3)]
    for y in range(h):
        v = (y + .5) / h
        sx, half = shaft_x(v), shaft_half(v)
        before, behind = vane(v)
        for x in range(w):
            u = (x + .5) / w
            d = u - sx
            if abs(d) <= half and TIP * .6 <= v <= QUILL_END:
                quill = v > VANE_END
                px[x, y] = (SHAFT, SHAFT, SHAFT + 6, 215 if quill else 255)
                continue
            reach = before if d < 0 else behind
            if reach <= 0:
                continue
            across = (abs(d) - half) / reach                              # 0 at the shaft, 1 at the vane's edge
            if across > 1:
                continue
            # a split: barbs come apart in a narrow wedge from the edge in
            edge = 1.0
            for at, side, width in splits:
                if (d < 0) == (side < 0):
                    slant = at - .10 * across                             # (barbs slant toward the tip)
                    if abs(v - slant) < width * across and across > .35:
                        edge = 0.0
            if edge == 0:
                continue
            # barbs: lines slanting from the shaft toward the tip
            barb = .5 + .5 * math.sin(2 * math.pi * (v + .22 * abs(d)) * 150)
            level = BODY * (.86 + .14 * barb)
            # the white bars across it (wavy, as on her wings), and its edge a shade lighter
            bar = smooth(.0, .02, v - .27 - .015 * math.sin(9 * u)) * (1 - smooth(.0, .02, v - .36 - .02 * math.sin(7 * u)))
            bar = max(bar, .7 * smooth(.0, .012, v - .54) * (1 - smooth(.0, .012, v - .565)))
            level = level + (BAR - level) * bar
            level = level + 18 * smooth(.8, 1, across)
            alpha = 255 * (1 - smooth(.9, 1, across)) * (1 - .35 * smooth(.72, VANE_END, v) * smooth(.4, 1, across))
            g = int(min(255, level))
            px[x, y] = (g, g, min(255, g + 6), int(alpha))
    small = img.resize((W, H), Image.LANCZOS)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    small.save(out)
    print("wrote", out)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    default = os.path.join(here, "..", "..", "..", "game", "assets", "Compiled", "Effects", "Nyxara", "WingFeather.png")
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(default))
