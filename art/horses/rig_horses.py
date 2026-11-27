"""Gives her horses a skeleton (2026-11-27), as art/monsters/rig_models.py gives the creatures theirs.

The owner, 2026-11-27: "in characterlab , i want you to put at least 6 different kind of horses that nyxara will be able
to sit on". The six Meshy horses (art/horses/meshy) are refined in Blender (art/refine/refine_assets.py: each turned so
that its nose points along +z) and compiled by art/monsters/prepare_models.py into game/assets/Compiled/Horses/<Name>.dmesh,
their hooves on y = 0. This tool finds each horse's parts from its shape, writes the joint weights into its .dmesh (four
joints a vertex, as the creatures have them) and writes <Name>.rig:

    TYPE horse
    J <joint> <parent> <x> <y> <z>     a joint and its pivot (mesh units)
    SEAT <x> <y> <z>                    the deepest point of the saddle's seat, on its surface (where her hips go)
    STIRRUP <side> <x> <y> <z>          where the stirrup's tread hangs (side -1: x < 0, 1: x > 0)
    HOOF <leg> <x> <y> <z>              each hoof's sole as it stands (legs: 0 front x < 0, 1 front x > 0, 2 hind x < 0, 3 hind x > 0)
    WIDTH <w>                           the half-width of its barrel under the saddle (what her legs go round)
    BELLY <y>                           the height of its belly

The joint numbers are fixed and shared with the game (game/src/NyxaraHorse.h, HorseRig). The game poses the joints in
code every frame (its gaits, its head and tail, its leaps, a winged horse's wings); there are no animation files.

How the parts are found:
- its legs: each hoof is a cluster of the vertices nearest the ground; each leg is traced up from its hoof, band by band,
  as the middle of the vertices near the line so far, as far as its belly. A vertex under the belly belongs to the leg
  whose line is nearest it at its height (when near enough: the stirrups, straps and the tail are not legs; low down,
  where a hoof and the hair over it are wide, from further: 2026-11-27, the hooves' edges were left on the body and
  stretched into planks as the legs went). Each leg
  turns at its top (the shoulder, the hip), its knee (the hock behind) and its fetlock, the skin blended over a few
  centimetres at each; its shoulder or haunch above the belly goes a little with it.
- its top line (the highest vertex on its middle, every 2 cm along it): the seat is its lowest point over the barrel
  (between the cantle and the pommel); the tail's root is where it falls away behind the croup.
- its neck and head: beyond a plane through the neck's root (over its front legs), turned up along the neck; the head
  beyond a plane through the poll (under its ears: the highest point in front of its legs).
- a winged horse's wings: what stands over its back and out to its sides behind the neck.

Usage:  python rig_horses.py <game compiled folder> [preview.png]
Run it again after prepare_models.py recompiles a horse (that erases the weights).
"""
import sys, pathlib
import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "monsters"))
from rig_models import read, write, pack, rot   # (the .dmesh's layout and the pose maths are the creatures')

(PELVIS, CHEST, NECK, NECK2, HEAD, TAIL, TAIL2,
 F_UP_L, F_KNEE_L, F_FET_L, F_UP_R, F_KNEE_R, F_FET_R,
 H_UP_L, H_HOCK_L, H_FET_L, H_UP_R, H_HOCK_R, H_FET_R,
 WING_L, WING_L2, WING_R, WING_R2) = range(23)
JOINTS = 23
PARENT = [-1, PELVIS, CHEST, NECK, NECK2, PELVIS, TAIL,
          CHEST, F_UP_L, F_KNEE_L, CHEST, F_UP_R, F_KNEE_R,
          PELVIS, H_UP_L, H_HOCK_L, PELVIS, H_UP_R, H_HOCK_R,
          CHEST, WING_L, CHEST, WING_R]
LEG_JOINTS = [(F_UP_L, F_KNEE_L, F_FET_L), (F_UP_R, F_KNEE_R, F_FET_R), (H_UP_L, H_HOCK_L, H_FET_L), (H_UP_R, H_HOCK_R, H_FET_R)]

# per horse: wings, and any measure that the shape alone gets wrong
HORSES = {
    "Horses/Tushetian":  dict(),
    "Horses/Kabardian":  dict(),
    "Horses/Karabakh":   dict(),
    "Horses/Rashi":      dict(wings=True, stirrups=False),
    "Horses/FireHorse":  dict(),
    "Horses/FrostHorse": dict(),
}


def ss(a, b, v):
    """Smooth step from 0 at a to 1 at b (a may be greater than b)."""
    t = np.clip((v - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def top_line(p, half=.08, step=.02, near=3, deep=.15):
    """The highest vertex on its middle (|x| < half) every 'step' along z: (z, y) rows. Where the mesh has a gap on its
    middle the highest vertex there is one far below the top (its belly's): a row more than 'deep' under the highest within
    'near' rows either side is such a gap, and is put on the line between the rows beside it that are not (a seat's dip
    is a few centimetres deep, a gap tens)."""
    x, y, z = p.T
    rows = []
    for zz in np.arange(z.min() + step / 2, z.max(), step):
        m = (np.abs(x) < half) & (np.abs(z - zz) < step / 2)
        if m.sum():
            rows.append((zz, y[m].max()))
    rows = np.array(rows)
    n = len(rows)
    high = np.array([rows[max(0, i - near):i + near + 1, 1].max() for i in range(n)])
    gap = rows[:, 1] < high - deep
    if gap.any() and (~gap).sum() >= 2:
        rows[gap, 1] = np.interp(rows[gap, 0], rows[~gap, 0], rows[~gap, 1])
    return rows


def trace(p, start, top, r=.09, step=.03):
    """A leg's middle line from its hoof up to 'top': rows of (y, x, z)."""
    x, y, z = p.T
    cx, cz = start
    line = []
    for y0 in np.arange(0, top, step):
        m = (y >= y0) & (y < y0 + step) & ((x - cx) ** 2 + (z - cz) ** 2 < r * r)
        if m.sum() >= 6:
            cx += float(np.clip(x[m].mean() - cx, -.03, .03))
            cz += float(np.clip(z[m].mean() - cz, -.03, .03))
        line.append((y0 + step / 2, cx, cz))
    return np.array(line)


def at(line, h):
    """A traced line's (x, z) at height h."""
    return np.interp(h, line[:, 0], line[:, 1]), np.interp(h, line[:, 0], line[:, 2])


def measure(p, spec):
    x, y, z = p.T
    tall = y.max()
    # ---- the hooves: the vertices nearest the ground, in four clusters (front and hind by the widest gap along z,
    #      then left and right by the widest gap along x)
    low = y < .05 * tall
    zs = np.sort(z[low]); k = int(np.argmax(np.diff(zs))); zsplit = (zs[k] + zs[k + 1]) / 2
    hooves = []
    for front in (True, False):
        m = low & ((z > zsplit) == front)
        xs = np.sort(x[m]); k = int(np.argmax(np.diff(xs))); xsplit = (xs[k] + xs[k + 1]) / 2
        for right in (False, True):
            mm = m & ((x > xsplit) == right)
            hooves.append((float(x[mm].mean()), float(z[mm].mean())))
    # ---- the belly: the lowest of its middle between the legs (rough legs first: the hooves straight up)
    zf0 = (hooves[0][1] + hooves[1][1]) / 2; zh0 = (hooves[2][1] + hooves[3][1]) / 2; D0 = zf0 - zh0
    mid = (z > zh0 + .3 * D0) & (z < zf0 - .3 * D0) & (np.abs(x) < .05) & (y > .25 * tall)
    belly = float(np.percentile(y[mid], 1))
    # ---- the legs, traced up from the hooves to the belly
    lines = [trace(p, h, belly - .01) for h in hooves]
    tops = [at(l, belly - .03) for l in lines]
    zf = (tops[0][1] + tops[1][1]) / 2; zh = (tops[2][1] + tops[3][1]) / 2; D = zf - zh
    # ---- the top line: the seat (its lowest over the barrel), the tail's root (where it falls behind the croup)
    line = top_line(p)
    # (the seat's dip is a hand wide, a dip between two things on its back (a cantle and a rolled blanket) a few
    # centimetres: the line is averaged over 4 cm either side before its lowest point is taken)
    soft = np.array([line[max(0, i - 2):i + 3, 1].mean() for i in range(len(line))])
    # (and it is a dip between a cantle behind and a pommel before: the back behind the saddle may be as low)
    best, seat_z, seat_y = -1.0, None, None
    zs_, ys_ = line[:, 0], soft
    for i in range(1, len(line) - 1):
        if not (zh + .2 * D < zs_[i] < zf - .25 * D) or ys_[i] > ys_[i - 1] or ys_[i] > ys_[i + 1]:   # (over the barrel, not the withers)
            continue
        behind = ys_[(zs_ < zs_[i]) & (zs_ > zs_[i] - .16)]
        before = ys_[(zs_ > zs_[i]) & (zs_ < zs_[i] + .22)]
        if len(behind) == 0 or len(before) == 0:
            continue
        rise = min(behind.max(), before.max()) - ys_[i]
        if rise > .025 and rise > best:
            best, seat_z, seat_y = rise, float(zs_[i]), float(ys_[i])
    if seat_z is None:   # (no saddle to be seen: the lowest of its back over the barrel)
        over = (zs_ > zh + .25 * D) & (zs_ < zf)
        k = int(np.argmin(np.where(over, ys_, 9))); seat_z, seat_y = float(zs_[k]), float(ys_[k])
    if spec.get("seat_z") is not None:
        seat_z = spec["seat_z"]; seat_y = float(np.interp(seat_z, zs_, ys_))
    behind = line[line[:, 0] < zh]
    croup = float(behind[:, 1].max()); croup_z = float(behind[np.argmax(behind[:, 1]), 0])
    fall = behind[(behind[:, 0] < croup_z) & (behind[:, 1] < croup - .06)]
    tail_z = float(fall[:, 0].max()) if len(fall) else float(z.min()) + .05
    tail_y = float(np.interp(tail_z, line[:, 0], line[:, 1])) - .05
    # ---- the poll: under its ears, the highest vertices in front of its legs
    front = z > zf + .1
    ears = front & (y > np.percentile(y[front], 98))
    poll = np.array([0, y[ears].mean() - .10 * tall / 1.8, z[ears].mean() - .02])
    neck_root = np.array([0, seat_y - .03, zf - .04])      # (the joint: in the neck where it leaves the withers)
    withers = np.array([0, seat_y + .05, zf - .12])        # (its boundary runs from here down and forward to the breast)
    # ---- the barrel under the saddle: its half-width (her legs go round it), and the stirrups
    slab = (np.abs(z - seat_z) < .06) & (y > belly + .15 * (seat_y - belly)) & (y < seat_y - .1 * (seat_y - belly)) & (np.abs(x) < .4)
    width = float(np.percentile(np.abs(x[slab]), 90)) if slab.sum() > 20 else .2
    stirrups = []
    for side in (-1, 1):
        # (hanging at its side, not the girth under its belly)
        # (under the saddle's front half, before its seat's deepest point: a saddle cloth's pendants hang behind it too)
        m = (x * side > .6 * width) & (z > seat_z - .02) & (z < seat_z + .3) & (y < belly + .25 * (seat_y - belly)) & (y > .4 * belly)
        if not spec.get("stirrups", True):
            m &= False
        # (not a leg: far from every leg's line at its height)
        for l in lines:
            lx, lz = at(l, y)
            m &= (x - lx) ** 2 + (z - lz) ** 2 > .11 ** 2
        if m.sum() > 10:
            bottom = m & (y < y[m].min() + .03)
            stirrups.append((float(x[bottom].mean()), float(y[bottom].mean()), float(z[bottom].mean())))
        else:
            stirrups.append((side * (width + .03), belly + .02, seat_z + .1))   # (none: her feet hang free at its sides)
    return dict(tall=tall, hooves=hooves, belly=belly, lines=lines, tops=tops, zf=zf, zh=zh, D=D, seat=(0.0, seat_y, seat_z),
                croup=croup, tail=(0.0, tail_y, tail_z), poll=poll, neck_root=neck_root, withers=withers, width=width, stirrups=stirrups, line=line)


def rig(p, spec):
    x, y, z = p.T
    n = len(p)
    M = measure(p, spec)
    belly, seat_y, seat_z = M["belly"], M["seat"][1], M["seat"][2]
    deep = seat_y - belly                       # the barrel's depth, belly to seat
    W = np.zeros((n, JOINTS))
    pivot = np.zeros((JOINTS, 3))
    taken = np.zeros(n)                         # weight given to legs, neck, head, tail, wings (the body has the rest)

    # ---- the legs (front 0 1, hind 2 3): under the belly a vertex is the nearest leg's, near enough
    dist = np.full((n, 4), 9.0)
    for i, l in enumerate(M["lines"]):
        lx, lz = at(l, np.minimum(y, belly - .03))
        dist[:, i] = np.hypot(x - lx, z - lz)
    nearest = np.argmin(dist, 1)
    # (near enough: 12 cm from its line over its knee; from its fetlock down, where a hoof and its hair are wide, 30 cm,
    # 18 cm between; never behind the tail's root, where the tail hangs)
    reach_leg = np.where(y < .25 * belly, .30, np.where(y < .45 * belly, .18, .12))
    reach_leg = np.where(z < M["tail"][2] + .06, .12, reach_leg)
    near = dist[np.arange(n), nearest] < reach_leg
    for i, (up, mid_j, fet) in enumerate(LEG_JOINTS):
        hind = i >= 2
        side = 1 if i % 2 else -1
        tx, tz = M["tops"][i]
        # under the belly: wholly the leg's; over it its shoulder (haunch) goes a little with it
        below = near & (nearest == i) & (y < belly - .01)
        reach = (.35 if hind else .2) * deep
        along = ss(.22, .12, np.abs(z - tz)) if not hind else ss(.26, .14, np.abs(z - tz + .04))
        aside = ss(.0, .06, x * side)
        over = ss(belly + reach, belly - .02, y) * along * aside * (y >= belly - .02)
        share = np.where(below, 1.0, over)
        share = np.minimum(share, 1 - taken)
        # its three joints by height: the knee (front) or hock (hind), the fetlock
        k_y = (.62 if hind else .50) * belly
        f_y = (.22 if hind else .20) * belly
        lower = ss(k_y + .03, k_y - .03, y)
        foot = ss(f_y + .025, f_y - .025, y)
        W[:, up] += share * (1 - lower)
        W[:, mid_j] += share * lower * (1 - foot)
        W[:, fet] += share * lower * foot
        taken += share
        # pivots: the top (shoulder: a third up the barrel; hip: near half), the knee or hock, the fetlock
        pivot[up] = (tx * .85, belly + (.45 if hind else .32) * deep, tz + (.03 if hind else 0))
        kx, kz = at(M["lines"][i], k_y); pivot[mid_j] = (kx, k_y, kz)
        fx, fz = at(M["lines"][i], f_y); pivot[fet] = (fx, f_y, fz)

    # ---- a winged horse's wings: over its back and out to its sides, behind its neck
    if spec.get("wings"):
        for side, (a, b) in ((-1, (WING_L, WING_L2)), (1, (WING_R, WING_R2))):
            over_back = (y > seat_y + .12) | ((np.abs(x) > M["width"] + .05) & (y > belly + .6 * deep))
            # (behind the line from its withers to its breast: before it is its neck, but for the wing's far feathers)
            behind_neck = ((p - M["withers"]) @ np.array([0, .6, .8]) < 0) | (np.abs(x) > .22)
            m = over_back & behind_neck & (x * side > .04) & (z < M["neck_root"][2] + .1) & (y > M["croup"] - .02)   # (its wings sweep back over its croup; the tail hangs below)
            wing = m * (1 - taken)
            rootp = np.array([side * .1, seat_y + .1, float(z[m & (np.abs(x) < .25)].mean()) if (m & (np.abs(x) < .25)).sum() else seat_z + .2])
            d = np.linalg.norm(p - rootp, axis=1)
            tip = d[m].max() if m.sum() else 1
            outer = ss(.40 * tip, .55 * tip, d)
            W[:, b] += wing * outer
            W[:, a] += wing * (1 - outer)
            taken += wing
            pivot[a] = rootp
            mid2 = m & (np.abs(d - .47 * tip) < .04)
            pivot[b] = p[mid2].mean(0) if mid2.sum() else rootp + (np.array([side, 1, 0]) * .3)
    else:
        for side, (a, b) in ((-1, (WING_L, WING_L2)), (1, (WING_R, WING_R2))):
            pivot[a] = (side * .1, seat_y, seat_z + .2); pivot[b] = (side * .4, seat_y + .2, seat_z + .2)

    # ---- the neck and head: beyond a plane through the neck's root, turned up along the neck; the head beyond the poll
    root, poll = M["neck_root"], M["poll"]
    way = poll - root; way /= np.linalg.norm(way)
    base_way = np.array([0, .6, .8])                       # (square to the line from the withers to the breast)
    s_root = (p - M["withers"]) @ base_way
    s_poll = (p - poll) @ way
    s_mid = (p - (root + poll) / 2) @ way
    neck = ss(-.03, .04, s_root) * ss(.3, .22, np.abs(x)) * (1 - taken)
    head = ss(-.05, .03, s_poll)
    neck2 = ss(-.05, .05, s_mid)
    W[:, HEAD] += neck * head
    W[:, NECK2] += neck * (1 - head) * neck2
    W[:, NECK] += neck * (1 - head) * (1 - neck2)
    taken += neck
    pivot[NECK] = root; pivot[NECK2] = (root + poll) / 2; pivot[HEAD] = poll

    # ---- the tail: behind the plane at its root, falling from the croup; its lower half swings the more
    tail_y, tail_z = M["tail"][1], M["tail"][2]
    tail = ss(tail_z + .03, tail_z - .03, z) * (1 - taken)   # (all of it, to its tip on the ground: what the legs did not take)
    hang = tail_y - y[tail > .5].min() if (tail > .5).sum() > 10 else .5
    low = ss(tail_y - .25 * hang, tail_y - .45 * hang, y)
    W[:, TAIL2] += tail * low
    W[:, TAIL] += tail * (1 - low)
    taken += tail
    pivot[TAIL] = (0, tail_y, tail_z)
    l2 = (tail > .5) & (np.abs(y - (tail_y - .35 * hang)) < .04)
    pivot[TAIL2] = (0, tail_y - .35 * hang, z[l2].mean() if l2.sum() else tail_z - .05)

    # ---- what hangs low and went to none of them (a tail's front edge, before its root's plane; hair among the hind
    #      legs): low down nothing is the body's, so each goes as the nearest vertex that went wholly to a leg or the tail
    #      (2026-11-27: left on the body they stretched as the legs and the tail went)
    stray = np.flatnonzero((y < .6 * belly) & (taken < .99))
    given = np.flatnonzero(taken >= .99)
    for chunk in np.array_split(stray, max(1, len(stray) // 64)) if len(stray) and len(given) else []:
        nearest_given = given[np.argmin(((p[chunk, None, :] - p[None, given, :]) ** 2).sum(2), 1)]
        W[chunk] = W[nearest_given]
        taken[chunk] = taken[nearest_given]

    # ---- the body: the hindquarters and the forehand, bending at the middle of the back
    body = 1 - taken
    front = ss(M["zh"] + .3 * M["D"], M["zf"] - .25 * M["D"], z)
    W[:, CHEST] += body * front
    W[:, PELVIS] += body * (1 - front)
    middle = (0, (belly + seat_y) / 2, seat_z)
    pivot[PELVIS] = middle
    pivot[CHEST] = middle
    return W, pivot, M


# ---------------------------------------------------------------- preview (the same maths as the game)
def pose(p, joints, weights, pivot, angles):
    final = [None] * JOINTS
    def solve(j):
        if final[j] is None:
            local = np.eye(4)
            for axis, angle in angles.get(j, []): local = local @ rot(axis, angle)
            t0, t1 = np.eye(4), np.eye(4)
            t0[3, :3], t1[3, :3] = -pivot[j], pivot[j]
            final[j] = t0 @ local @ t1 @ (solve(PARENT[j]) if PARENT[j] >= 0 else np.eye(4))
        return final[j]
    mats = np.stack([solve(j) for j in range(JOINTS)])
    ph = np.concatenate([p, np.ones((len(p), 1))], 1)
    out = np.zeros((len(p), 3))
    for k in range(4):
        out += np.einsum("ni,nij->nj", ph, mats[joints[:, k]])[:, :3] * weights[:, k:k + 1]
    return out


POSES = {
    "rest": {},
    "stride": {F_UP_L: [(0, -.55)], F_KNEE_L: [(0, 1.2)], F_FET_L: [(0, .6)], F_UP_R: [(0, .35)],
               H_UP_L: [(0, .45)], H_UP_R: [(0, -.4)], H_HOCK_R: [(0, -1.0)], H_FET_R: [(0, .7)],
               NECK: [(0, .25)], HEAD: [(0, .3)], TAIL: [(0, -.5)], TAIL2: [(1, .4)], CHEST: [(0, .08)],
               WING_L: [(2, .6)], WING_L2: [(2, .5)], WING_R: [(2, -.6)], WING_R2: [(2, -.5)]},
}
COLOURS = np.array([[120, 120, 120], [60, 110, 200], [230, 190, 40], [240, 220, 120], [250, 120, 20], [100, 60, 30], [170, 120, 80],
                    [210, 60, 60], [240, 130, 130], [255, 200, 200], [40, 160, 70], [130, 220, 150], [200, 250, 210],
                    [150, 70, 190], [210, 150, 240], [240, 210, 255], [70, 180, 190], [150, 220, 230], [210, 245, 250],
                    [200, 40, 120], [250, 140, 200], [40, 80, 160], [120, 160, 240]])


def preview(results, path):
    from PIL import Image, ImageDraw
    S = 300
    sheet = Image.new("RGB", (S * 6, S * len(results)), "white")
    for row, (name, p, joints, weights, pivot, M) in enumerate(results):
        colour = COLOURS[joints[:, 0]]
        lo, hi = p.min(0), p.max(0)
        scale = (S - 30) / max(hi[1] - lo[1], hi[2] - lo[2], hi[0] - lo[0])
        for col, (label, angles) in enumerate(POSES.items()):
            q = pose(p, joints, weights, pivot, angles)
            for view, (ax, depth) in enumerate(((2, 0), (0, 2), (2, 1))):
                img = Image.new("RGB", (S, S), (250, 250, 250))
                px = img.load()
                u = ((q[:, ax] - (lo[ax] + hi[ax]) / 2) * scale + S / 2).astype(int).clip(0, S - 1)
                if view < 2:
                    w = (S - 12 - (q[:, 1] - lo[1]) * scale).astype(int).clip(0, S - 1)
                else:   # from above: z right, x down
                    w = ((q[:, 0] - (lo[0] + hi[0]) / 2) * scale + S / 2).astype(int).clip(0, S - 1)
                order = np.argsort(q[:, depth] * (1 if view == 0 else -1))
                for k in order: px[int(u[k]), int(w[k])] = tuple(int(c) for c in colour[k])
                d = ImageDraw.Draw(img)
                if view == 0 and col == 0:
                    for j in range(JOINTS):
                        pu = (pivot[j][2] - (lo[2] + hi[2]) / 2) * scale + S / 2; pw = S - 12 - (pivot[j][1] - lo[1]) * scale
                        d.ellipse([pu - 3, pw - 3, pu + 3, pw + 3], outline=(0, 0, 0))
                    sx, sy, sz = M["seat"]
                    pu = (sz - (lo[2] + hi[2]) / 2) * scale + S / 2; pw = S - 12 - (sy - lo[1]) * scale
                    d.rectangle([pu - 4, pw - 4, pu + 4, pw + 4], outline=(255, 0, 0))
                    for (sx, sy, sz) in M["stirrups"]:
                        pu = (sz - (lo[2] + hi[2]) / 2) * scale + S / 2; pw = S - 12 - (sy - lo[1]) * scale
                        d.rectangle([pu - 3, pw - 3, pu + 3, pw + 3], outline=(0, 120, 255))
                d.text((4, 2), "%s %s %s" % (name.split("/")[-1], label, ("side", "front", "above")[view]), fill=(0, 0, 0))
                sheet.paste(img, ((col * 3 + view) * S, row * S))
    sheet.save(path)


if __name__ == "__main__":
    root = pathlib.Path(sys.argv[1])
    results = []
    for name, spec in HORSES.items():
        path = root / (name + ".dmesh")
        data, count, p = read(path)
        p = p.astype(np.float64)
        W, pivot, M = rig(p, spec)
        joints, weights = pack(W)
        write(path, data, count, joints, weights)
        used = sorted(set(joints[weights > 0].tolist()))
        with open(root / (name + ".rig"), "w", newline="\n") as out:
            out.write("# Written by art/horses/rig_horses.py. Joint, parent, pivot; her seat, the stirrups, the hooves.\nTYPE horse\n")
            for j in range(JOINTS):
                out.write("J %d %d %.5f %.5f %.5f\n" % (j, PARENT[j], *pivot[j]))
            out.write("SEAT %.5f %.5f %.5f\n" % M["seat"])
            for side, s in zip((-1, 1), M["stirrups"]):
                out.write("STIRRUP %d %.5f %.5f %.5f\n" % (side, *s))
            for leg, l in enumerate(M["lines"]):
                hx, hz = at(l, .02)
                out.write("HOOF %d %.5f %.5f %.5f\n" % (leg, hx, 0.0, hz))
            out.write("WIDTH %.5f\nBELLY %.5f\n" % (M["width"], M["belly"]))
        print("%-18s %6d vertices  seat (%.3f, %.3f)  belly %.3f  width %.3f  tail (%.3f, %.3f)  poll (%.3f, %.3f)  stirrups %s  joints used %d" % (
            name, count, M["seat"][1], M["seat"][2], M["belly"], M["width"], M["tail"][1], M["tail"][2], M["poll"][1], M["poll"][2],
            " ".join("(%.2f %.2f %.2f)" % s for s in M["stirrups"]), len(used)))
        results.append((name, p, joints, weights, pivot, M))
    if len(sys.argv) > 2:
        preview(results, sys.argv[2])
