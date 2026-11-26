"""Nyxara's flair (2026-11-25): the two textures her light is drawn from (game/src/NyxaraFlair.cpp).

    python make_flair_textures.py <out folder> [preview.jpg]

FlairAtlas.png, 2048 x 3072, four cells of 512 across and six down (row by row; the names below; the last two rows'
spare cells are empty). In every cell:
    alpha  the shape, as a height: the shader wears it away from its low parts up (a trail's tail goes in streaks)
    red    how much of the vertex's colour it takes (1 nearly everywhere)
    green  how white-hot it is there (a slash's edge, a spark's heart): added white, as bright as the colour
FlairNoise.png, 256 x 256, tileable: three noises (r g b) the shader moves through the world to wear shapes unevenly.
Red, green and the noises are written so that they are what is meant after the sRGB decode a colour texture gets."""
import sys, os
import numpy as np
import cv2

N = 512
CELLS = ['GLOW', 'STREAK', 'SLASH_RAZOR', 'SLASH_SILK', 'SLASH_HEAVY', 'SMOKE', 'STAR', 'RING',
         'FLAME', 'SHARD', 'FEATHER', 'CRACK', 'BURST', 'DEBRIS', 'STAIN', 'WISP',
         'NEEDLE', 'SPLAT', 'SWIPE', 'SHOCK', 'SMOKE2', 'FLAME2']
COLS, ROWS = 4, 6


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def lattice(h, w, py, px, seed, tile=False):
    """value noise, py by px cells over h by w pixels (tile: it repeats)"""
    r = np.random.default_rng(seed)
    g = r.random((py + (0 if tile else 1), px + (0 if tile else 1))).astype(np.float32)
    ys = np.arange(h, dtype=np.float32) * py / h
    xs = np.arange(w, dtype=np.float32) * px / w
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    fy = ys - y0; fx = xs - x0
    fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
    y1 = y0 + 1; x1 = x0 + 1
    if tile:
        y0 %= py; y1 %= py; x0 %= px; x1 %= px
    a = g[np.ix_(y0, x0)]; b = g[np.ix_(y0, x1)]; c = g[np.ix_(y1, x0)]; d = g[np.ix_(y1, x1)]
    top = a * (1 - fx)[None, :] + b * fx[None, :]
    bot = c * (1 - fx)[None, :] + d * fx[None, :]
    return top * (1 - fy)[:, None] + bot * fy[:, None]


def fbm(h, w, py, px, seed, octaves=5, tile=False, gain=.5):
    total = np.zeros((h, w), np.float32); amp = 1.; norm = 0.
    for o in range(octaves):
        total += amp * lattice(h, w, py * 2 ** o, px * 2 ** o, seed + 101 * o, tile)
        norm += amp; amp *= gain
    return total / norm


Y, X = np.mgrid[0:N, 0:N].astype(np.float32) / (N - 1)
DX, DY = X - .5, Y - .5
R = np.hypot(DX, DY) / .5          # 0 in the middle, 1 at the middle of an edge
EDGE = smooth(0, .035, X) * smooth(1, .965, X) * smooth(0, .035, Y) * smooth(1, .965, Y)


def cell(alpha, hot=None, tint=None):
    a = np.clip(np.nan_to_num(alpha), 0, 1) * EDGE
    g = np.zeros_like(a) if hot is None else np.clip(hot, 0, 1)
    r = np.ones_like(a) if tint is None else np.clip(tint, 0, 1)
    return np.dstack([r, g, np.zeros_like(a), a]).astype(np.float32)


def glow():
    a = np.exp(-R * R * 4.5) * smooth(1., .7, R)
    return cell(a, hot=np.exp(-R * R * 26))


def streak():
    taper = np.clip(4 * X * (1 - X), 0, 1) ** 1.5
    core = np.exp(-(DY / .03) ** 2)
    halo = np.exp(-(DY / .11) ** 2) * .28
    return cell((core + halo) * taper, hot=np.exp(-(DY / .016) ** 2) * taper)


def slash(edge_at, edge_wide, body_long, streaks_y, streaks_x, body, rag, decay, seed, second=0., wave=0.):
    """a blade's trail: x 0 at the blade, 1 its oldest part; y 0 its outer edge (the blade's point), 1 toward her hand"""
    yy = Y + wave * np.sin(X * 6.283 * 1.4 + Y * 7.) * (.25 + .75 * X)
    n = fbm(N, N, streaks_y, streaks_x, seed, 4)
    n2 = fbm(N, N, max(2, streaks_y // 3), max(1, streaks_x), seed + 7, 4)
    n = cv2.remap(n, X * (N - 1), np.clip(yy, 0, 1) * (N - 1), cv2.INTER_LINEAR)
    edge = np.exp(-((yy - edge_at) / edge_wide) ** 2)
    under = np.where(yy > edge_at, np.exp(-(yy - edge_at) / body_long), np.exp(-((yy - edge_at) / (edge_wide * 1.3)) ** 2))
    streaks = .25 + .75 * smooth(.25, .8, n)
    line2 = second * np.exp(-((yy - edge_at - .11) / (edge_wide * .7)) ** 2)
    head = smooth(0, .025, X)
    tail = np.clip(1 - X, 0, 1) ** decay
    a = (edge * (.55 + .45 * tail) + line2 + body * under * streaks) * tail * head
    a = a - rag * X * (1 - smooth(.2, .75, n2)) * (1 - .6 * edge)
    hot = (edge ** 3 * (.3 + .45 * tail) + .35 * line2) * head * smooth(.8, .3, X)   # (its very edge, near the blade)
    return cell(a, hot=hot)


def smoke():
    n = fbm(N, N, 4, 4, 31, 5)
    n2 = fbm(N, N, 9, 9, 32, 4)
    a = smooth(0, .55, (1 - R) + (n - .5) * .75) * (.5 + .5 * n2)
    return cell(a * smooth(1., .82, R))


def star():
    r = np.hypot(DX, DY)
    across = np.exp(-(DY / .011) ** 2) * np.exp(-(np.abs(DX) / .30) ** 1.5) * smooth(.5, .40, np.abs(DX))
    up = np.exp(-(DX / .011) ** 2) * np.exp(-(np.abs(DY) / .15) ** 1.5) * .85 * smooth(.5, .40, np.abs(DY))
    d1 = np.exp(-(((DX - DY) * .7071) / .010) ** 2) * np.exp(-(r / .09) ** 2) * .4
    d2 = np.exp(-(((DX + DY) * .7071) / .010) ** 2) * np.exp(-(r / .09) ** 2) * .4
    core = np.exp(-(r / .045) ** 2)
    halo = np.exp(-(r / .15) ** 2) * .22
    a = np.clip(across + up + d1 + d2 + core + halo, 0, 1)
    return cell(a, hot=np.clip(across * .8 + up * .7 + core, 0, 1))


def ring():
    a = np.exp(-((R - .80) / .032) ** 2) + np.where(R < .80, smooth(.25, .80, R) * .16, 0)
    return cell(a * smooth(.98, .92, R), hot=np.exp(-((R - .80) / .016) ** 2) * .6)


def flame():
    n = fbm(N, N, 5, 5, 41, 4)
    n2 = fbm(N, N, 10, 6, 42, 4)
    x = X + (n - .5) * .16 * (1 - Y)
    wide = .30 * (Y ** .8) * smooth(1., .82, Y) + .015
    d = np.abs(x - .5) / wide
    a = np.clip(1 - d, 0, 1) ** 1.1 * smooth(.02, .12, Y) * (.45 + .55 * n2)
    return cell(a, hot=np.clip(1 - d * 1.8, 0, 1) * smooth(.35, .9, Y) * .35)


def shard():
    d = np.abs(DX) / .15 + np.abs(DY) / .45
    inside = smooth(1., .93, d)
    edge = smooth(.72, .98, d)
    facet = np.where(DX < 0, .62, .92) * np.where((DX * 2.2 + DY) > 0, 1., .8)
    spine = np.exp(-(DX / .010) ** 2) * .7
    a = inside * np.clip(.42 * facet + .5 * edge + spine, 0, 1)
    return cell(a, hot=inside * np.clip(edge * .8 + spine, 0, 1) * .6)


def feather():
    t = np.clip((Y - .07) / .86, 0, 1)
    bend = .05 * np.sin(t * 2.6)
    dx = X - .5 - bend
    wide = .19 * np.clip(np.sin(np.pi * t), 0, 1) ** .65 * (1 - .28 * t) + .004
    d = np.abs(dx) / wide
    vane = smooth(1., .82, d) * smooth(0, .04, t) * smooth(1., .97, t)
    barbs = .62 + .38 * np.sin((Y * 150 - np.abs(dx) * 210))
    gaps = smooth(.30, .42, fbm(N, N, 26, 3, 51, 3) + (1 - d) * .35)
    shaft = np.exp(-(dx / .007) ** 2) * smooth(0, .03, t) * smooth(1., .9, t)
    return cell(np.clip(vane * barbs * gaps * .85 + shaft, 0, 1), hot=shaft * .5)


def lines(draw, blur=1.2, scale=2):
    """thin lines drawn at twice the size and brought down"""
    canvas = np.zeros((N * scale, N * scale), np.uint8)
    draw(canvas, N * scale)
    img = cv2.resize(canvas, (N, N), interpolation=cv2.INTER_AREA).astype(np.float32) / 255
    return img, cv2.GaussianBlur(img, (0, 0), blur * 4)


def crack():
    rng = np.random.default_rng(61)

    def draw(c, s):
        def walk(p, ang, length, thick, depth):
            steps = int(length / (s * .02)) + 1
            for i in range(steps):
                ang2 = ang + rng.normal(0, .32)
                q = (p[0] + np.cos(ang2) * s * .02, p[1] + np.sin(ang2) * s * .02)
                cv2.line(c, (int(p[0]), int(p[1])), (int(q[0]), int(q[1])), 255, max(1, int(round(thick * (1 - i / steps * .7)))), cv2.LINE_AA)
                p = q; ang = ang * .8 + ang2 * .2
                if depth < 2 and rng.random() < .13:
                    walk(p, ang + rng.choice([-1, 1]) * rng.uniform(.5, 1.1), length * rng.uniform(.25, .5), thick * .6, depth + 1)
        for k in range(9):
            walk((s / 2, s / 2), k * 6.283 / 9 + rng.uniform(-.25, .25), s * rng.uniform(.26, .44), 5, 0)
    img, soft = lines(draw)
    a = np.clip(img * 1.3 + soft * .9, 0, 1) * smooth(1., .8, R)
    return cell(a, hot=img * smooth(.9, .2, R) * .45)


def burst():
    rng = np.random.default_rng(71)

    def draw(c, s):
        for k in range(84):
            ang = rng.uniform(0, 6.283)
            r0 = rng.uniform(.07, .24) * s / 2
            r1 = (rng.uniform(.34, .7) if rng.random() < .75 else rng.uniform(.75, .95)) * s / 2
            v = int(rng.uniform(90, 255))
            cv2.line(c, (int(s / 2 + np.cos(ang) * r0), int(s / 2 + np.sin(ang) * r0)),
                     (int(s / 2 + np.cos(ang) * r1), int(s / 2 + np.sin(ang) * r1)), v, 2 if rng.random() < .7 else 3, cv2.LINE_AA)
    img, soft = lines(draw, .6)
    return cell(np.clip(img * 1.2 + soft * .5, 0, 1) * smooth(1., .86, R), hot=img * .3)


def debris():
    rng = np.random.default_rng(81)
    c = np.zeros((N * 2, N * 2), np.uint8)
    k = 8
    pts = []
    for i in range(k):
        ang = i * 6.283 / k + rng.uniform(-.25, .25)
        rad = rng.uniform(.26, .42) * N * 2
        pts.append([int(N + np.cos(ang) * rad), int(N + np.sin(ang) * rad * .8)])
    cv2.fillPoly(c, [np.array(pts, np.int32)], 255, cv2.LINE_AA)
    img = cv2.resize(c, (N, N), interpolation=cv2.INTER_AREA).astype(np.float32) / 255
    n = fbm(N, N, 6, 6, 82, 4)
    shade = .55 + .45 * smooth(.3, .7, n + (DX - DY) * .5)
    return cell(img, tint=shade)


def stain():
    n = fbm(N, N, 3, 3, 91, 5)
    n2 = fbm(N, N, 12, 12, 92, 4)
    a = smooth(0, .38, (1 - R * 1.12) + (n - .5) * .95) * (.6 + .4 * n2)
    speck = smooth(.66, .78, fbm(N, N, 40, 40, 93, 2)) * smooth(1., .55, R) * smooth(.3, .7, R) * .8
    return cell(np.clip(a + speck, 0, 1) * smooth(1., .86, R))


def wisp():
    mid = .5 + .2 * np.sin(6.283 * X * .9 + .5) * (1 - .25 * X)
    thick = .075 * np.clip(np.sin(np.pi * np.clip(X, 0, 1)), 0, 1) ** .8 + .002
    n = fbm(N, N, 14, 3, 97, 4)
    d = (Y - mid) / thick
    a = np.exp(-d * d) * (.45 + .55 * smooth(.25, .75, n)) * smooth(0, .06, X) * smooth(1., .9, X)
    side = np.exp(-((d - 1.5) / .5) ** 2) * .35 * smooth(.2, .6, n) * smooth(0, .1, X) * smooth(1., .8, X)
    return cell(np.clip(a + side, 0, 1), hot=np.exp(-(d / .3) ** 2) * smooth(0, .1, X) * smooth(1., .85, X) * .4)


def needle():
    """a spindle: solid through its middle, pointed at both ends, most of its cell's height at its thickest (a spark
    drawn out along its flight, the line of a cut, a ray: STREAK is a hairline in a wide glow)"""
    taper = np.clip(4 * X * (1 - X), 0, 1) ** .9
    half = .36 * taper + .004
    d = np.abs(DY) / half
    a = smooth(1., .35, d) * smooth(0, .02, X) * smooth(1., .98, X)
    return cell(a, hot=smooth(.5, .05, d) * taper * .7)   # (a heart of white; the rest its colour)


def splat():
    """what is thrown where it lands (blood): a blot with arms out of it and drops about it"""
    rng = np.random.default_rng(113)
    s = N * 2
    c = np.zeros((s, s), np.uint8)
    cv2.circle(c, (s // 2, s // 2), int(s * .12), 255, -1, cv2.LINE_AA)
    for k in range(12):
        ang = rng.uniform(0, 6.283); length = rng.uniform(.14, .36) * s; wide = rng.uniform(.012, .038) * s
        for i in range(16):
            u = i / 15
            cv2.circle(c, (int(s / 2 + np.cos(ang) * length * u), int(s / 2 + np.sin(ang) * length * u)), max(1, int(wide * (1 - u * .8))), 255, -1, cv2.LINE_AA)
        if rng.random() < .6:
            far = length * rng.uniform(1.06, 1.25)
            cv2.circle(c, (int(s / 2 + np.cos(ang) * far), int(s / 2 + np.sin(ang) * far)), max(2, int(wide * rng.uniform(.5, 1.1))), 255, -1, cv2.LINE_AA)
    for k in range(30):
        ang = rng.uniform(0, 6.283); rad = rng.uniform(.2, .45) * s
        cv2.circle(c, (int(s / 2 + np.cos(ang) * rad), int(s / 2 + np.sin(ang) * rad)), max(2, int(rng.uniform(.004, .015) * s)), 255, -1, cv2.LINE_AA)
    img = cv2.resize(c, (N, N), interpolation=cv2.INTER_AREA).astype(np.float32) / 255
    n = fbm(N, N, 9, 9, 114, 4)
    soft = cv2.GaussianBlur(img, (0, 0), 5)
    a = smooth(.28, .6, soft * .5 + cv2.GaussianBlur(img, (0, 0), 1.2) * .5 + (n - .5) * .22)
    return cell(a * (.8 + .2 * n) * smooth(1., .9, R), tint=.7 + .3 * smooth(.3, .7, n))


def swipe():
    """the mark of a cut seen face on: a crescent, one stroke of a brush bowed upward, thin where it begins (x 0) and
    pointed where it ends (x 1), torn into streaks along its length, hot along its upper (outer) edge"""
    n = fbm(N, N, 30, 3, 121, 4)
    n2 = fbm(N, N, 5, 2, 122, 3)
    mid = .58 - .22 * np.clip(np.sin(np.pi * X), 0, 1) ** 1.15
    half = .105 * np.clip(np.sin(np.pi * X), 0, 1) ** .75 * (.45 + .55 * smooth(0, .65, X)) + .002
    d = (Y - mid) / half                                    # -1 its upper edge, 1 its lower
    body = smooth(1., .72, np.abs(d))
    streaks = .3 + .7 * smooth(.3, .72, n)
    upper = np.exp(-((d + .7) / .24) ** 2)
    a = body * (streaks * (.5 + .5 * (1 - (d + 1) / 2)) + upper * .85) * smooth(0, .05, X) * smooth(1., .975, X)
    a = a - .35 * (1 - X) * (1 - smooth(.25, .7, n2)) * (1 - upper)
    return cell(a, hot=upper ** 2 * body * smooth(.08, .55, X) * .6)


def shock():
    """a ring of dust, or of force, going out over the ground: thick, broken, streaked the way it goes, sharper at its front"""
    ang = np.arctan2(DY, DX)
    polar = fbm(N, N, 3, 44, 131, 4, tile=True)
    pol = cv2.remap(polar, ((ang / 6.2832 + .5) % 1.) * (N - 1), np.clip(R, 0, 1) * (N - 1), cv2.INTER_LINEAR)
    band = np.exp(-((R - .70) / .14) ** 2)
    front = np.exp(-((R - .85) / .035) ** 2)
    a = (band * (.2 + .8 * smooth(.3, .72, pol)) + front * (.45 + .55 * pol)) * smooth(.99, .92, R)
    return cell(a, hot=front * .35)


def smoke2():
    """smoke torn open: wisps and holes"""
    n = fbm(N, N, 3, 3, 141, 5)
    n2 = fbm(N, N, 7, 7, 142, 4)
    n3 = fbm(N, N, 16, 16, 143, 3)
    a = smooth(.05, .6, (1 - R * 1.05) + (n - .5) * 1.1) * smooth(.28, .68, n2 + .12) * (.6 + .4 * n3)
    return cell(a * smooth(1., .8, R))


def flame2():
    """a tongue of flame that curls: its tip leaning over, holes torn in it toward the tip"""
    n = fbm(N, N, 5, 5, 151, 4)
    n2 = fbm(N, N, 12, 7, 152, 4)
    x = X + (n - .5) * .2 * (1 - Y) - .13 * (1 - Y) ** 1.8 * np.sin(Y * 4. + .6)
    wide = .26 * (Y ** .65) * smooth(1., .85, Y) + .012
    d = np.abs(x - .5) / wide
    a = np.clip(1 - d, 0, 1) ** .9 * smooth(0, .1, Y) * (.4 + .6 * n2) * smooth(.12, .5, n2 + Y * .7)
    return cell(a, hot=np.clip(1 - d * 1.7, 0, 1) * smooth(.4, .95, Y) * .35)


def make_atlas():
    made = [
        glow(), streak(),
        slash(edge_at=.11, edge_wide=.030, body_long=.20, streaks_y=30, streaks_x=3, body=.42, rag=.55, decay=1.25, seed=11),
        slash(edge_at=.16, edge_wide=.075, body_long=.42, streaks_y=12, streaks_x=2, body=.78, rag=.42, decay=.85, seed=21, wave=.035),
        slash(edge_at=.10, edge_wide=.045, body_long=.50, streaks_y=16, streaks_x=3, body=.95, rag=.85, decay=1.0, seed=27, second=.55),
        smoke(), star(), ring(), flame(), shard(), feather(), crack(), burst(), debris(), stain(), wisp(),
        needle(), splat(), swipe(), shock(), smoke2(), flame2()]
    assert len(made) == len(CELLS) <= COLS * ROWS
    atlas = np.zeros((ROWS * N, COLS * N, 4), np.float32)
    for i, c in enumerate(made):
        atlas[(i // COLS) * N:(i // COLS + 1) * N, (i % COLS) * N:(i % COLS + 1) * N] = c
    return atlas


def encode(linear_rgba):
    out = np.empty(linear_rgba.shape, np.float32)
    out[..., :3] = np.clip(linear_rgba[..., :3], 0, 1) ** (1 / 2.2)   # (undone by the sRGB decode)
    out[..., 3] = np.clip(linear_rgba[..., 3], 0, 1)
    return (out * 255 + .5).astype(np.uint8)


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    atlas = make_atlas()
    png = encode(atlas)
    cv2.imwrite(os.path.join(out, 'FlairAtlas.png'), cv2.cvtColor(png, cv2.COLOR_RGBA2BGRA), [cv2.IMWRITE_PNG_COMPRESSION, 9])
    noise = np.dstack([fbm(256, 256, 4, 4, 5, 5, True), fbm(256, 256, 6, 6, 6, 4, True), fbm(256, 256, 3, 3, 7, 6, True), np.ones((256, 256), np.float32)])
    for k in range(3):
        lo, hi = noise[..., k].min(), noise[..., k].max()
        noise[..., k] = (noise[..., k] - lo) / (hi - lo)
    cv2.imwrite(os.path.join(out, 'FlairNoise.png'), cv2.cvtColor(encode(noise), cv2.COLOR_RGBA2BGRA))
    if len(sys.argv) > 2:
        # a look at it: each cell as light on dark (blue for its colour, white where it is hot), names over them
        a = atlas[..., 3:4]
        look = a * (np.array([.25, .55, 1.], np.float32) * atlas[..., 0:1] + atlas[..., 1:2])
        look = (np.clip(look, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)
        look = cv2.cvtColor(look, cv2.COLOR_RGB2BGR)
        for i, name in enumerate(CELLS):
            cv2.putText(look, f'{i} {name}', ((i % COLS) * N + 10, (i // COLS) * N + 28), cv2.FONT_HERSHEY_SIMPLEX, .8, (160, 160, 160), 2)
            cv2.rectangle(look, ((i % COLS) * N, (i // COLS) * N), ((i % COLS + 1) * N - 1, (i // COLS + 1) * N - 1), (40, 40, 40), 1)
        cv2.imwrite(sys.argv[2], cv2.resize(look, (1400, 1400 * ROWS // COLS), interpolation=cv2.INTER_AREA), [cv2.IMWRITE_JPEG_QUALITY, 88])
    print('flair textures:', out)


if __name__ == '__main__':
    main()
