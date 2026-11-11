"""Generates the Caucasus night sky and moon textures for Caucasian Eagle.

Usage:  python make_sky_textures.py <output folder>
Writes: NightSky.jpg  (4096x2048, tiles horizontally; row 0 is the top of the sky walls, the last row the
                       horizon; the upper half is deep sky only, because the ceiling panel samples it)
        Moon.jpg      (2048x1024, equirectangular, wraps horizontally)

The engine draws the sky as four wall panels plus a ceiling, all sampling the same texture, so
the image must wrap left-to-right. Every noise field here is periodic for that reason.
Deterministic: fixed seeds.
"""
import sys, pathlib
import numpy as np
from PIL import Image

OUT = pathlib.Path(sys.argv[1])
OUT.mkdir(parents=True, exist_ok=True)


def periodic_noise(h, w, beta, seed, anis=1.0):
    """Periodic fractal noise in 0..1 (power spectrum 1/f^beta), via a filtered FFT."""
    rng = np.random.default_rng(seed)
    fy = np.fft.fftfreq(h)[:, None] * h * anis
    fx = np.fft.fftfreq(w)[None, :] * w
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1
    spec = (rng.normal(size=(h, w)) + 1j * rng.normal(size=(h, w))) / f ** (beta / 2)
    spec[0, 0] = 0
    n = np.fft.ifft2(spec).real
    n = (n - n.min()) / (n.max() - n.min())
    return n


def smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def make_sky():
    W, H = 4096, 2048
    v = np.linspace(0, 1, H)[:, None] * np.ones((1, W))      # 0 zenith .. 1 horizon
    u = np.ones((H, 1)) * np.linspace(0, 1, W, endpoint=False)[None, :]

    # Base gradient: black-blue zenith, deep teal mid sky, cold green glow at the horizon.
    zen = np.array([0.006, 0.012, 0.028])
    mid = np.array([0.012, 0.040, 0.058])
    hor = np.array([0.050, 0.150, 0.130])
    t1 = smooth(v, 0.52, 0.86)[..., None]
    t2 = smooth(v, 0.80, 0.985)[..., None] ** 1.6
    img = zen * (1 - t1) + mid * t1
    img = img * (1 - t2) + hor * t2

    # Milky Way: a band that wanders once around the horizon, so it wraps.
    centre = 0.27 + 0.11 * np.sin(2 * np.pi * (u + 0.1))
    band = np.exp(-((v - centre) / 0.075) ** 2)
    dust = periodic_noise(H, W, 2.6, 11)
    lanes = periodic_noise(H, W, 2.2, 12)
    mw = band * (0.35 + 0.9 * dust) * (1 - 0.75 * smooth(lanes, 0.50, 0.66) * band)
    img += mw[..., None] * np.array([0.050, 0.075, 0.085])

    # Aurora: faint green curtains above the horizon haze.
    cur = periodic_noise(H, W, 2.4, 21, anis=0.18)            # stretched vertically
    env = np.exp(-((v - 0.72) / 0.10) ** 2)
    aur = smooth(cur, 0.56, 0.86) * env
    img += aur[..., None] * np.array([0.012, 0.085, 0.050])

    # Stars: three populations. Fewer and dimmer toward the horizon.
    rng = np.random.default_rng(3)
    stars = np.zeros((H, W, 3))
    fade = 1 - smooth(v, 0.76, 0.94)

    def scatter(count, lo, hi, sigma):
        layer = np.zeros((H, W, 3))
        ys = rng.integers(0, int(H * 0.93), count)
        xs = rng.integers(0, W, count)
        mag = lo + (hi - lo) * rng.random(count) ** 3
        tint = np.stack([0.80 + 0.2 * rng.random(count), 0.88 + 0.12 * rng.random(count),
                         0.86 + 0.14 * rng.random(count)], 1)
        np.add.at(layer, (ys, xs), mag[:, None] * tint)
        if sigma > 0:                                         # periodic Gaussian blur, peak kept
            fy = np.fft.fftfreq(H)[:, None]
            fx = np.fft.fftfreq(W)[None, :]
            kernel = np.exp(-2 * (np.pi * sigma) ** 2 * (fx * fx + fy * fy))
            for c in range(3):
                layer[..., c] = np.fft.ifft2(np.fft.fft2(layer[..., c]) * kernel).real
            layer *= 2 * np.pi * sigma * sigma
        return layer

    dens = 0.55 + 0.9 * mw / max(mw.max(), 1e-6)
    stars += scatter(26000, 0.10, 0.55, 0.6) * dens[..., None]
    stars += scatter(2600, 0.45, 1.00, 1.0)
    stars += scatter(140, 0.9, 1.6, 2.2)
    img += stars * fade[..., None]

    # Thin moonlit cloud streaks low in the sky.
    cl = periodic_noise(H, W, 2.9, 31, anis=3.0)             # stretched horizontally
    cloud = smooth(cl, 0.52, 0.80) * np.exp(-((v - 0.84) / 0.07) ** 2)
    img = img * (1 - 0.55 * cloud[..., None]) + cloud[..., None] * np.array([0.050, 0.080, 0.085])

    # Distant Caucasus ridges along the horizon: three layers, nearer = darker.
    x = np.arange(W)

    def ridge(seed, base, amp, rough):
        n = periodic_noise(1, W, rough, seed)[0]
        n2 = periodic_noise(1, W, rough - 0.5, seed + 100)[0]
        return base - amp * (0.7 * n + 0.3 * n2 ** 2)

    layers = [(41, 0.925, 0.115, 2.3, np.array([0.030, 0.072, 0.072]), np.array([0.150, 0.200, 0.200])),
              (42, 0.955, 0.085, 2.1, np.array([0.016, 0.040, 0.040]), np.array([0.080, 0.110, 0.110])),
              (43, 0.985, 0.055, 2.0, np.array([0.006, 0.016, 0.015]), None)]
    for seed, base, amp, rough, colour, snow in layers:
        top = ridge(seed, base, amp, rough)[None, :]          # v of the ridge line per column
        mask = smooth(v, top - 0.0012, top + 0.0012)
        body = np.ones((H, W, 3)) * colour
        if snow is not None:                                  # moonlit snow on the upper slopes
            depth = (v - top)
            grain = periodic_noise(H, W, 2.8, seed + 7)
            cap = smooth(-depth, -0.012 - 0.05 * grain, -0.002) * smooth(top, base - 0.2 * amp, base - 0.75 * amp)
            body = body * (1 - cap[..., None]) + snow * cap[..., None]
        img = img * (1 - mask[..., None]) + body * mask[..., None]

    img = np.clip(img, 0, 1) ** (1 / 2.2)
    Image.fromarray((img * 255 + 0.5).astype(np.uint8)).save(OUT / "NightSky.jpg", quality=93)


def make_moon():
    W, H = 2048, 1024
    lat = (np.linspace(0, 1, H)[:, None] - 0.5) * np.pi
    base = 0.50 + 0.30 * periodic_noise(H, W, 2.0, 51)
    maria = smooth(periodic_noise(H, W, 3.2, 52), 0.44, 0.58)
    alb = base * (1 - 0.52 * maria)
    rng = np.random.default_rng(53)
    yy, xx = np.mgrid[0:H, 0:W]
    for _ in range(900):                                       # craters: dark floor, bright rim
        cy, cx = rng.integers(60, H - 60), rng.integers(0, W)
        r = float(rng.choice([2, 3, 5, 8, 12, 18], p=[.34, .26, .18, .12, .07, .03]))
        sx = r / max(np.cos(lat[cy, 0]), 0.25)
        y0, y1 = max(cy - int(2 * r), 0), min(cy + int(2 * r), H)
        cols = (np.arange(cx - int(2 * sx), cx + int(2 * sx)) % W)
        d = np.sqrt(((yy[y0:y1, :1] - cy) / r) ** 2 + ((np.arange(-int(2 * sx), int(2 * sx))[None, :]) / sx) ** 2)
        prof = -0.16 * np.exp(-(d / 0.75) ** 4) + 0.11 * np.exp(-((d - 1.0) / 0.16) ** 2)
        alb[y0:y1, cols] += prof * (0.6 + 0.4 * rng.random())
    for _ in range(3):                                         # a few ray systems
        cy, cx = rng.integers(200, H - 200), rng.integers(0, W)
        ang = np.arctan2(yy - cy, ((xx - cx + W // 2) % W) - W // 2)
        dist = np.hypot(yy - cy, ((xx - cx + W // 2) % W) - W // 2)
        rays = (np.sin(ang * rng.integers(9, 17) + rng.random() * 6) > 0.86) * np.exp(-dist / 150.0)
        alb += 0.06 * rays
    alb = np.clip(alb, 0, 1)
    tint = np.array([0.90, 1.00, 0.95])                         # cold silver with a breath of green
    img = np.clip(alb[..., None] * tint * 1.15, 0, 1) ** (1 / 2.2)
    Image.fromarray((img * 255 + 0.5).astype(np.uint8)).save(OUT / "Moon.jpg", quality=93)


make_sky()
make_moon()
print("wrote", OUT / "NightSky.jpg", OUT / "Moon.jpg")
