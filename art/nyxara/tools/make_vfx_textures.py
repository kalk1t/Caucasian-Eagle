"""Nyxara soul-VFX alpha masks (white RGB; tint comes from the runtime material)."""
import math, pathlib, sys
import numpy as np
from PIL import Image
OUT = pathlib.Path(sys.argv[1])
OUT.mkdir(parents=True, exist_ok=True)

def save(name, a):
    a = np.clip(a, 0, 1); h, w = a.shape
    rgba = np.ones((h, w, 4), np.float32); rgba[..., 3] = a
    Image.fromarray((rgba * 255).astype(np.uint8), "RGBA").save(OUT / name)

def grid(w, h):
    u = (np.arange(w) + .5) / w; v = (np.arange(h) + .5) / h
    return np.meshgrid(u, v)

# Arc: u along the swing (1 = leading edge), v across (1 = outer rim, bright core near the rim)
u, v = grid(512, 128)
core = np.exp(-((v - .78) / .10) ** 2) + .45 * np.exp(-((v - .55) / .22) ** 2)
streak = .75 + .25 * np.sin(v * 90 + u * 7) ** 2
save("SoulArc.png", core * streak * (u ** 1.6) * np.clip((1 - u) * 18, 0, 1) * np.clip(v * 4, 0, 1))
# Soft round glow
u, v = grid(128, 128); r = np.hypot(u - .5, v - .5) * 2
save("Glow.png", np.exp(-(r / .42) ** 2) + .25 * np.exp(-(r / .9) ** 2) * (r < 1))
# Ring: u around, v radial (1 = outer edge)
u, v = grid(512, 64)
save("Ring.png", (np.exp(-((v - .86) / .07) ** 2) + .35 * np.exp(-((v - .6) / .25) ** 2)) * (.8 + .2 * np.sin(u * 2 * math.pi * 24) ** 2))
# Feather: u across, v along (0 = quill, 1 = tip)
u, v = grid(128, 256)
half = .46 * np.sin(np.clip(v, 0, 1) * math.pi) ** .55 * (1 - .25 * v)
x = np.abs(u - .5)
vane = (x < half) * (.55 + .45 * np.sin((v * 34 - x * 22)) ** 2) * np.clip((half - x) * 25, 0, 1)
quill = np.exp(-(x / .018) ** 2) * (v < .97)
save("Feather.png", np.maximum(vane * .85, quill))
# Beam: u around (seamless), v up (0 bottom)
u, v = grid(64, 256)
save("Beam.png", (.55 + .45 * np.sin(u * 2 * math.pi * 3 + v * 6) ** 2) * (1 - v) ** .7 * np.clip(v * 12, 0, 1))
# Moon sigil (ground decal, planar UV)
u, v = grid(512, 512); X, Y = (u - .5) * 2, (v - .5) * 2; r = np.hypot(X, Y); th = np.arctan2(Y, X)
sig = (np.exp(-((r - .95) / .018) ** 2) + np.exp(-((r - .80) / .012) ** 2) + .7 * np.exp(-((r - .55) / .01) ** 2))
ticks = (np.abs(np.sin(th * 18)) > .97) * ((r > .82) & (r < .93))
runes = ((np.sin(th * 36) > .55) & (np.sin(r * 120) > .2)) * ((r > .58) & (r < .77)) * .7
moon = (np.hypot(X, Y) < .32) & (np.hypot(X - .13, Y - .06) > .27)
save("MoonSigil.png", (sig + ticks + runes + moon * .9) * (r < 1))
print("written", sorted(p.name for p in OUT.iterdir()))
