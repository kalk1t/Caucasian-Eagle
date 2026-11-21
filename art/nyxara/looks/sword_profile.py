"""The profile of each look sword, to find its grip: along its long axis, slice by slice, how wide and how thick it is.
A grip is round (as wide as it is thick) and thin, a blade is flat (much wider than thick), a guard is wide and short.

    blender --factory-startup -b --python sword_profile.py -- <out folder> S11 S12 ...

Writes <out>/<name>_points.npy (the points along the axis, across the blade, through it; the axis's direction chosen so
the pommel end is at 0) and prints the slice table and the guess: 'GRIP <name> <t0> <t1> <pommel at 0|1>'.
"""
import bpy, sys, os
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
OUT, NAMES = args[0], args[1:]
HERE = os.path.dirname(os.path.abspath(__file__))
os.makedirs(OUT, exist_ok=True)
SLICES = 60


def points_of(name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(HERE, "rodin", name, "base_basic_pbr.glb"))
    o = [x for x in bpy.context.scene.objects if x.type == "MESH"][0]
    w = np.empty(len(o.data.vertices) * 3, "f4")
    o.data.vertices.foreach_get("co", w)
    m = np.array(o.matrix_world)
    return w.reshape(-1, 3).astype("f8") @ m[:3, :3].T + m[:3, 3]


for name in NAMES:
    w = points_of(name)
    c = w.mean(0)
    vals, vecs = np.linalg.eigh(np.cov((w - c).T))
    ax = vecs[:, -1]
    t = (w - c) @ ax
    t -= t.min()
    L = t.max()
    # the blade's flat: the widest way across, from the points of the longer half
    rest = w - c - np.outer((w - c) @ ax, ax)
    width = []
    thick = []
    centre = []
    for k in range(SLICES):
        s = (t >= k * L / SLICES) & (t < (k + 1) * L / SLICES + 1e-9)
        if s.sum() < 6:
            width.append(0); thick.append(0); centre.append(np.zeros(3)); continue
        r = rest[s]
        rc = r.mean(0)
        e, v = np.linalg.eigh(np.cov((r - rc).T))
        a, b = v[:, -1], v[:, -2]
        width.append(np.ptp((r - rc) @ a)); thick.append(np.ptp((r - rc) @ b)); centre.append(rc)
    width = np.array(width); thick = np.array(thick)
    round_ = (width < 2.2 * thick) & (width > 0)
    # the guard: the widest slice in either end's third; the grip: the round slices between it and that end
    best = None
    for end in (0, 1):
        idx = range(0, SLICES // 3) if end == 0 else range(SLICES - SLICES // 3, SLICES)
        g = max(idx, key=lambda k: width[k])
        if end == 0:
            run = [k for k in range(g - 1, -1, -1)]
        else:
            run = [k for k in range(g + 1, SLICES)]
        grip = []
        for k in run:
            if round_[k] and width[k] < 0.45 * width[g]:
                grip.append(k)
            elif grip:
                break
        score = len(grip) * (width[g] / max(1e-6, np.median(width)))
        if best is None or score > best[0]:
            best = (score, end, g, grip)
    _, end, g, grip = best
    if end == 1:   # the pommel at the far end: turn the axis round
        t = L - t
        width, thick = width[::-1], thick[::-1]
        g = SLICES - 1 - g
        grip = [SLICES - 1 - k for k in grip]
    t0 = min(grip) / SLICES if grip else 0.1
    t1 = (max(grip) + 1) / SLICES if grip else 0.2
    print("PROFILE", name, "length %.3f" % L)
    for k in range(SLICES):
        mark = "G" if k == g else ("h" if t0 * SLICES <= k < t1 * SLICES else " ")
        print("  %2d %s w %.3f t %.3f" % (k, mark, width[k], thick[k]))
    print("GRIP %s %.3f %.3f %d" % (name, t0, t1, end))
    across = rest @ np.linalg.eigh(np.cov(rest[t > 0.6 * L].T))[1][:, -1]
    through = rest @ np.linalg.eigh(np.cov(rest[t > 0.6 * L].T))[1][:, -2]
    np.save(os.path.join(OUT, name + "_points.npy"), np.stack([t / L, across / L, through / L], 1).astype("f4"))
