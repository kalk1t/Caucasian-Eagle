"""Gives the creature models a skeleton, so the game can move their limbs.

The Meshy models are single static meshes without bones. This tool reads a compiled <Name>.dmesh, works out which
vertices belong to the hips, chest, head, arms, legs (or skirt, neck chain, wings), writes those joint weights into
the vertices of the .dmesh (the format already has room for four joints per vertex) and writes <Name>.rig, a text
file with the joints and their pivots:

    TYPE biped|robed|serpent|winged
    J <joint> <parent> <x> <y> <z>        pivot in mesh units

The joint numbers are fixed and shared with the game (game/src/CreatureRig.h).
The game poses the joints in code every frame (walk, windup, strike, recoil, death); there are no animation files.

Usage:  python rig_models.py <game compiled folder> [preview.png]
Run it again after prepare_models.py recompiles a creature (that erases the weights).

How vertices are sorted: the model is cut into thin horizontal bands. In each band the vertices are grouped into
runs along x (a gap between runs means separate body parts: a leg beside a leg, an arm beside the body). The run
on the centre line is the body; runs beside it are arms; below the hip line the two runs nearest the centre are legs.
Weights are blended over a few centimetres at every joint so the skin bends instead of tearing.
"""
import struct, sys, pathlib
import numpy as np

PELVIS, CHEST, HEAD, ARM_L, FORE_L, ARM_R, FORE_R, LEG_L, SHIN_L, LEG_R, SHIN_R, SKIRT1, SKIRT2, SPINE2, SPINE3, WING_L, WING_L2, WING_R, WING_R2 = range(19)
JOINTS = 19
PARENT = [-1, PELVIS, CHEST, CHEST, ARM_L, CHEST, ARM_R, PELVIS, LEG_L, PELVIS, LEG_R, PELVIS, SKIRT1, CHEST, SPINE2, CHEST, WING_L, CHEST, WING_R]

# type, hip line and neck line as shares of the height.
# arm_x: the arms are whatever lies farther than this from the centre line (share of the height); without it the arms are
# found from the gaps beside the body. tall: -1 or 1, that side holds something that reaches above the head (a staff).
MODELS = {
    "Monsters/Chinka":     dict(type="biped", hip=.30, neck=.68, arm_x=.20),
    "Monsters/Kudiani":    dict(type="biped", hip=.38, neck=.78, arm_x=.25, tall=-1),
    "Monsters/Ochokochi":  dict(type="biped", hip=.45, neck=.80),
    "Monsters/Almasty":    dict(type="biped", hip=.40, neck=.80, arm_x=.23),
    "Monsters/Kaji":       dict(type="biped", hip=.45, neck=.82, arm_x=.22),
    "Monsters/Uaig":       dict(type="biped", hip=.24, neck=.88, arm_x=.31),
    "Monsters/Devi":       dict(type="biped", hip=.40, neck=.78, arm_x=.25, centre="extent"),
    "Monsters/Ali":        dict(type="robed", hip=.48, neck=.82),
    "Monsters/Rokapi":     dict(type="robed", hip=.45, neck=.72),
    "Monsters/Gveleshapi": dict(type="serpent"),
    "Landmarks/AmiraniMountain/Eagle": dict(type="winged"),
}
BANDS, GAP, HEADER = 50, .035, 16


def ss(a, b, x):
    """Smooth step from 0 at a to 1 at b (a may be greater than b)."""
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def read(path):
    data = bytearray(path.read_bytes())
    assert data[:4] == b"DMSH", path
    count, _, _, stride = struct.unpack_from("<4I", data, HEADER)
    assert stride == 72
    raw = np.frombuffer(data, dtype=np.uint8, count=count * 72, offset=HEADER + 56).reshape(count, 72)
    return data, count, raw[:, :12].copy().view(np.float32).reshape(count, 3)


def write(path, data, count, joints, weights):
    block = np.frombuffer(data, dtype=np.uint8, count=count * 72, offset=HEADER + 56).reshape(count, 72).copy()
    block[:, 48:56] = joints.astype(np.uint16).view(np.uint8).reshape(count, 8)
    block[:, 56:72] = weights.astype(np.float32).view(np.uint8).reshape(count, 16)
    data[HEADER + 56:HEADER + 56 + count * 72] = block.tobytes()
    flags = struct.unpack_from("<I", data, HEADER + 40)[0]
    struct.pack_into("<I", data, HEADER + 40, flags | 8)   # DoorwaysDMeshFlag_HasSkinning
    path.write_bytes(data)


def runs(x):
    """Groups of vertices along x in one band: returns a run number per vertex and the list of (min, max) per run."""
    order = np.argsort(x)
    xs = x[order]
    cut = np.concatenate([[0], np.cumsum(np.diff(xs) > GAP)])
    ident = np.empty(len(x), int)
    ident[order] = cut
    spans = [(xs[cut == k].min(), xs[cut == k].max(), int((cut == k).sum())) for k in range(cut[-1] + 1)]
    # a few stray vertices are not a body part: join them to the nearest real run
    real = [k for k, s in enumerate(spans) if s[2] >= 6] or [int(np.argmax([s[2] for s in spans]))]
    for k, s in enumerate(spans):
        if k not in real:
            near = min(real, key=lambda r: min(abs(s[0] - spans[r][1]), abs(s[1] - spans[r][0])))
            ident[ident == k] = near
    return ident, {k: (x[ident == k].min(), x[ident == k].max()) for k in real}


def rig(p, spec):
    lo, hi = p.min(0), p.max(0)
    H = hi[1] - lo[1]
    v = (p[:, 1] - lo[1]) / H
    n = len(p)
    W = np.zeros((n, JOINTS), np.float32)
    pivot = np.zeros((JOINTS, 3), np.float32)
    kind = spec["type"]
    Y = lambda share: lo[1] + share * H
    mean = lambda mask, fallback: p[mask].mean(0) if mask.sum() > 3 else np.array(fallback, np.float32)

    if kind == "winged":
        cx, half = np.median(p[:, 0]), (hi[0] - lo[0]) / 2
        x = (p[:, 0] - cx) / half
        ax = np.abs(x)
        inner, outer = ss(.10, .22, ax) * ss(.28, .40, v), ss(.48, .62, ax)
        for side, (a, b) in ((-1, (WING_L, WING_L2)), (1, (WING_R, WING_R2))):
            m = np.sign(x) == side
            W[m, a] = (inner * (1 - outer))[m]
            W[m, b] = (inner * outer)[m]
            pivot[a] = mean(m & (ax > .10) & (ax < .22), [cx + side * .16 * half, Y(.5), 0]); pivot[a][0] = cx + side * .16 * half
            pivot[b] = mean(m & (ax > .48) & (ax < .62), [cx + side * .55 * half, Y(.6), 0]); pivot[b][0] = cx + side * .55 * half
        W[:, CHEST] = 1 - W.sum(1)
        body = ax < .12
        pivot[PELVIS] = pivot[CHEST] = mean(body, [cx, Y(.4), 0])
        return W, pivot

    core = (v > .45) & (v < .70)
    cx = np.median(p[core, 0])
    if spec.get("centre") == "extent":    # heavy detail on one side pulls the median off the centre line: use the middle of the width
        cx = (np.percentile(p[core, 0], 1) + np.percentile(p[core, 0], 99)) / 2
    x = (p[:, 0] - cx) / H
    cz = np.median(p[core, 2])

    if kind == "serpent":
        cuts = [(.24, CHEST), (.44, SPINE2), (.62, SPINE3), (.78, HEAD)]
        level = [ss(c - .06, c + .06, v) for c, _ in cuts]
        W[:, PELVIS] = 1 - level[0]
        for k, (c, j) in enumerate(cuts):
            W[:, j] = level[k] * (1 - level[k + 1] if k + 1 < len(cuts) else 1)
            pivot[j] = mean(np.abs(v - c) < .03, [cx, Y(c), cz]); pivot[j][1] = Y(c)
        pivot[PELVIS] = [cx, Y(0), cz]
        return W, pivot

    hip, neck = spec["hip"], spec["neck"]
    band = np.minimum((v * BANDS).astype(int), BANDS - 1)
    forced = spec.get("arm_x")
    part = np.zeros(n, int)          # 0 body, -1/+1 arm of that side, -2/+2 leg of that side, 3 both legs in one run
    arm_edge = {-1: {}, 1: {}}       # side -> band -> inner edge of the arm run
    for b in range(BANDS):
        idx = np.nonzero(band == b)[0]
        if len(idx) == 0: continue
        ident, spans = runs(x[idx])
        vb = (b + .5) / BANDS
        centre = [k for k, (a, c) in spans.items() if a <= 0 <= c]
        centre = centre[0] if centre else None
        left = sorted([k for k, (a, c) in spans.items() if c < 0], key=lambda k: -spans[k][1])    # nearest first
        right = sorted([k for k, (a, c) in spans.items() if a > 0], key=lambda k: spans[k][0])
        for side, group in ((-1, left), (1, right)):
            for order, k in enumerate(group):
                inner = min(abs(spans[k][0]), abs(spans[k][1]))
                sel = idx[ident == k]
                if vb < hip:
                    is_leg = (centre is None and order == 0) or inner < .18
                    if forced: is_leg = inner < forced - .03
                    part[sel] = side * 2 if is_leg else side
                    if not is_leg: arm_edge[side][b] = min(inner, arm_edge[side].get(b, 9))
                elif not forced and inner > .10 and (vb < neck or inner > .22):
                    part[sel] = side
                    arm_edge[side][b] = min(inner, arm_edge[side].get(b, 9))
        if centre is not None and vb < hip: part[idx[ident == centre]] = 3

    arm = np.zeros(n, np.float32)    # how much of each vertex follows its arm
    arm[np.abs(part) == 1] = 1
    info = {}
    for side in (-1, 1):
        mine = np.sign(x) == side
        bands = sorted(arm_edge[side])
        raised = False
        if forced:
            # everything farther than arm_x from the centre line, from the hips up to the shoulders
            top = neck - .06
            take = mine & (v >= hip - .02)
            reach = ss(forced - .03, forced + .03, np.abs(x[take]))
            if spec.get("tall") != side: reach = reach * ss(top + .04, top - .04, v[take])
            arm[take] = np.maximum(arm[take], reach)
            shoulder, edge = top - .04, forced
        elif bands:
            bands = [b for b in bands if hip - .15 < (b + .5) / BANDS < neck - .07] or bands
            end = max(bands)
            edge = max(.10, np.median([arm_edge[side][b] for b in bands if abs(b - end) <= 2]))
            v_end = (end + 1) / BANDS
            # the shoulder: body vertices beyond the armpit line, just past the end of the free arm
            near = mine & (part == 0) & ((v < v_end) & (v > v_end - .14) if raised else (v >= v_end) & (v < v_end + .14))
            fade = ss(.14, .09, np.abs(v[near] - v_end))
            arm[near] = ss(edge - .05, edge + .02, np.abs(x[near])) * fade
            shoulder = v_end + (-.05 if raised else .05)
        else:
            info[side] = "no arm"; continue
        a, f = (ARM_L, FORE_L) if side < 0 else (ARM_R, FORE_R)
        limb = mine & (arm > .5)
        if limb.sum() < 10: info[side] = "no arm"; continue
        tip = np.percentile(v[limb], 97 if raised else 3)
        along = np.clip((v - shoulder) / (tip - shoulder), 0, 1)
        fore = ss(.42, .58, along)
        W[mine, a] = (arm * (1 - fore))[mine]
        W[mine, f] = (arm * fore)[mine]
        top = limb & (np.abs(v - shoulder) < .05)
        pivot[a] = mean(top, [cx + side * edge * H, Y(shoulder), cz]); pivot[a][0] = cx + side * (edge + .02) * H; pivot[a][1] = Y(shoulder)
        pivot[f] = mean(limb & (along > .42) & (along < .58), pivot[a])
        info[side] = "%s arm, shoulder %.2f, edge %.2f, tip %.2f" % ("raised" if raised else "hanging", shoulder, edge, tip)

    rest = 1 - arm
    lower = ss(hip + .03, hip - .05, v) * (np.abs(part) != 1)     # how much follows the legs / skirt
    if kind == "robed":
        low2 = ss(hip * .55 + .05, hip * .55 - .05, v)
        W[:, SKIRT1] = rest * lower * (1 - low2)
        W[:, SKIRT2] = rest * lower * low2
        pivot[SKIRT1] = [cx, Y(hip), cz]
        pivot[SKIRT2] = mean(np.abs(v - hip * .55) < .03, [cx, Y(hip * .55), cz]); pivot[SKIRT2][1] = Y(hip * .55)
    else:
        knee = hip * .5
        shin = ss(knee + .035, knee - .035, v)
        left = np.where(part == -2, 1., np.where(part == 2, 0., ss(.03, -.03, x)))
        for j, s, share in ((LEG_L, SHIN_L, left), (LEG_R, SHIN_R, 1 - left)):
            W[:, j] = rest * lower * share * (1 - shin)
            W[:, s] = rest * lower * share * shin
            legs = (share > .6) & (np.abs(part) != 1)
            pivot[j] = mean(legs & (v < hip) & (v > hip - .07), [cx, Y(hip), cz]); pivot[j][1] = Y(hip)
            pivot[s] = mean(legs & (np.abs(v - knee) < .03), pivot[j]); pivot[s][1] = Y(knee)
    upper = rest * (1 - lower)
    head = ss(neck - .03, neck + .03, v)
    chest = ss(hip + .02, hip + .22, v)
    W[:, HEAD] = upper * head
    W[:, CHEST] = upper * (1 - head) * chest
    W[:, PELVIS] = upper * (1 - head) * (1 - chest)
    pivot[PELVIS] = [cx, Y(hip), cz]
    pivot[CHEST] = mean((np.abs(v - hip - .10) < .03) & (np.abs(x) < .12), [cx, Y(hip + .10), cz]); pivot[CHEST][1] = Y(hip + .10)
    pivot[HEAD] = mean((np.abs(v - neck) < .03) & (np.abs(x) < .12), [cx, Y(neck), cz]); pivot[HEAD][1] = Y(neck)
    print("   left:", info.get(-1), "| right:", info.get(1))
    return W, pivot


def pack(W):
    order = np.argsort(-W, axis=1)[:, :4]
    weights = np.take_along_axis(W, order, 1)
    weights /= np.maximum(weights.sum(1, keepdims=True), 1e-6)
    order[weights <= 0] = 0
    return order, weights


# ---------------------------------------------------------------- preview (the same maths as the game)
def rot(axis, angle):
    c, s = np.cos(angle), np.sin(angle)
    m = np.eye(4)
    if axis == 0: m[1, 1], m[1, 2], m[2, 1], m[2, 2] = c, s, -s, c        # row vectors, as DirectXMath
    if axis == 1: m[0, 0], m[0, 2], m[2, 0], m[2, 2] = c, -s, s, c
    if axis == 2: m[0, 0], m[0, 1], m[1, 0], m[1, 1] = c, s, -s, c
    return m


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
    "stride": {LEG_L: [(0, -.6)], SHIN_L: [(0, .3)], LEG_R: [(0, .5)], SHIN_R: [(0, .7)], ARM_L: [(0, .5)], ARM_R: [(0, -.5)], FORE_L: [(0, -.3)], FORE_R: [(0, -.5)],
               CHEST: [(0, .15), (1, .15)], HEAD: [(1, -.1)], SKIRT1: [(0, .25)], SKIRT2: [(0, .3)], SPINE2: [(1, .2)], SPINE3: [(1, -.2)],
               WING_L: [(2, .5)], WING_L2: [(2, .4)], WING_R: [(2, -.5)], WING_R2: [(2, -.4)]},
    "windup": {CHEST: [(0, -.3), (1, .4)], ARM_R: [(0, -2.3)], FORE_R: [(0, -.6)], ARM_L: [(0, -.6)], HEAD: [(0, .15)], LEG_L: [(0, -.3)], LEG_R: [(0, .25)],
               SPINE2: [(0, -.3)], SPINE3: [(0, -.3)], HEAD + 0: [(0, -.2)], WING_L: [(2, -.5)], WING_R: [(2, .5)]},
}
COLOURS = np.array([[120, 120, 120], [60, 110, 200], [230, 190, 40], [210, 60, 60], [240, 130, 130], [40, 160, 70], [130, 220, 150], [150, 70, 190], [210, 150, 240],
                    [230, 120, 30], [250, 190, 120], [70, 180, 190], [150, 220, 230], [100, 60, 30], [170, 120, 80], [210, 60, 60], [240, 130, 130], [40, 160, 70], [130, 220, 150]])


def preview(results, path):
    from PIL import Image, ImageDraw
    S = 260
    sheet = Image.new("RGB", (S * 6, S * len(results)), "white")
    for row, (name, p, joints, weights, pivot) in enumerate(results):
        colour = COLOURS[joints[:, 0]]
        lo, hi = p.min(0), p.max(0)
        scale = (S - 40) / max(hi[1] - lo[1], hi[0] - lo[0])
        for col, (label, angles) in enumerate(POSES.items()):
            q = pose(p, joints, weights, pivot, angles)
            for view, (ax, depth) in enumerate(((0, 2), (2, 0))):
                img = Image.new("RGB", (S, S), (250, 250, 250))
                px = img.load()
                u = ((q[:, ax] - (lo[ax] + hi[ax]) / 2) * scale + S / 2).astype(int).clip(0, S - 1)
                w = (S - 20 - (q[:, 1] - lo[1]) * scale).astype(int).clip(0, S - 1)
                for k in np.argsort(q[:, depth] * (-1 if view == 0 else 1))[::2]: px[int(u[k]), int(w[k])] = tuple(int(c) for c in colour[k])
                ImageDraw.Draw(img).text((4, 2), "%s %s %s" % (name.split("/")[-1], label, "front" if view == 0 else "side"), fill=(0, 0, 0))
                sheet.paste(img, ((col * 2 + view) * S, row * S))
    sheet.save(path)


if __name__ == "__main__":
    root = pathlib.Path(sys.argv[1])
    results = []
    for name, spec in MODELS.items():
        path = root / (name + ".dmesh")
        data, count, p = read(path)
        print(name, spec["type"], count, "vertices")
        W, pivot = rig(p.astype(np.float64), spec)
        joints, weights = pack(W)
        write(path, data, count, joints, weights)
        used = sorted(set(joints[weights > 0].tolist()))
        with open(root / (name + ".rig"), "w") as out:
            out.write("# Written by art/monsters/rig_models.py. Joint, parent, pivot.\nTYPE %s\n" % spec["type"])
            for j in range(JOINTS):
                parent = PARENT[j]
                if spec["type"] == "serpent" and j == HEAD: parent = SPINE3
                out.write("J %d %d %.5f %.5f %.5f\n" % (j, parent, *pivot[j]))
        print("   joints used:", used)
        results.append((name, p.astype(np.float64), joints, weights, pivot))
    if len(sys.argv) > 2: preview(results, sys.argv[2])
