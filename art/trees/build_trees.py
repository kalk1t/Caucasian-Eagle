"""The game's tree library (0.5.2): every kind of tree the Eagle's Ascent grows, a few variants of each, four levels of
detail, written for the game (AscentTrees) into game/assets/Trees:

    Models/<model>_<variant>_<lod>_bark.dmesh, ..._leaves.dmesh     (lod 0 near .. 3 far; a bare tree has no leaves)
    Textures/<kind>_bark.png, _bark_n.png, _leaves.png, _leaves_snow.png   (copied from textures.py's output)
    trees.txt   one line a model: "<model> <texture kind> <variants> <conifer 0/1> <snow 0/1> <height of each variant>"

    python -I art/trees/build_trees.py [only=fir,beech] [textures=build/trees/textures] [out=game/assets/Trees]

textures.py must have run first. The meshes are generated, not kept in Git (.gitignore): run this after a fresh clone.
The models are in metres with the trunk's foot at the origin; the game scales each to its tree's height.
"""
import os, shutil, sys, time, json
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import treegen as tg

# (model, species in treegen, texture kind, variants)
MODELS = [
    ("fir", "fir", "fir", 4), ("fir_great", "fir_great", "fir", 3), ("fir_young", "fir_young", "fir", 3),
    ("spruce", "spruce", "spruce", 4), ("pine", "pine", "pine", 4),
    ("beech", "beech", "beech", 4), ("oak", "oak", "oak", 4), ("maple", "maple", "maple", 4), ("hornbeam", "hornbeam", "hornbeam", 4),
    ("gold", "beech", "gold", 3), ("birch", "birch", "birch", 4),
    ("apple", "fruit", "apple", 2), ("cherry", "fruit", "cherry", 2), ("orange", "fruit", "orange", 2), ("pomegranate", "fruit", "pomegranate", 2),
    ("shrub", "shrub", "rhodo", 3), ("juniper", "juniper", "juniper", 3), ("snag", "snag", "snag", 3),
]

def one(job):
    model, species, kind, v, tex, out = job
    man = json.load(open(os.path.join(tex, "manifest.json")))["kinds"][kind]
    rects = man.get("rects", {"A": [0, .5, .5, 1], "B": [.5, .5, 1, 1], "C": [0, 0, .5, .5]})
    sw = man.get("swatches") or None
    tree = tg.Tree(species, seed_of(model, v))
    top, tris = 0.0, []
    for lod in range(4):
        bark, leaves = tg.build(tree, rects, lod, fruit_uv=sw[:2] if sw else None)
        n = 0
        for part, mesh in (("bark", bark), ("leaves", leaves)):
            arr = mesh.arrays()
            path = os.path.join(out, "Models", "%s_%d_%d_%s.dmesh" % (model, v, lod, part))
            if arr is None:
                if os.path.exists(path): os.remove(path)
                continue
            tg.write_dmesh(path, arr); n += mesh.tris
            if lod == 0: top = max(top, float(arr[0][:, 1].max()))
        tris.append(n)
    return model, v, top, tris

COLS, ROWS = 10, 8          # the impostor atlas: 80 slots of 192 x 384 (impostors.py's SLOT_W, SLOT_H)

def seed_of(model, v):
    return 1000 * (MODELS.index(next(m for m in MODELS if m[0] == model)) + 1) + v

def bleed(a, steps=24):
    """Colour carried out under the transparent pixels (no dark fringes when the far pictures shrink)."""
    import numpy as np
    rgb = a[:, :, :3].astype(np.float32); have = a[:, :, 3] > 8
    for _ in range(steps):
        if have.all(): break
        acc = np.zeros_like(rgb); cnt = np.zeros(have.shape, np.float32)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sh = np.roll(have, (dy, dx), (0, 1)); acc += np.roll(rgb, (dy, dx), (0, 1)) * sh[:, :, None]; cnt += sh
        new = (~have) & (cnt > 0); rgb[new] = acc[new] / cnt[new][:, None]; have = have | new
    out = a.copy(); out[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8); return out

def impostors(tex, out, only, blender):
    """Renders every variant's picture (Blender: impostors.py) and packs them into Textures/impostors.png; returns
    trees.txt's lines "impostor <model> <variant> <snow> <u0> <v0> <u1> <v1> <width m> <height m>" (v from the top)."""
    import subprocess, numpy as np
    from PIL import Image
    work = os.path.join(os.path.dirname(tex), "impostors"); os.makedirs(work, exist_ok=True)
    jobs = []
    for (m, s, k, n) in MODELS:
        for v in range(n):
            for snow in ((0, 1) if os.path.exists(os.path.join(tex, "%s_leaves_snow.png" % k)) else (0,)):
                jobs.append([m, s, k, v, snow, seed_of(m, v)])
    assert len(jobs) <= COLS * ROWS, "the impostor atlas is full"
    todo = [j for j in jobs if not only or j[0] in only]
    jf = os.path.join(work, "jobs.json"); json.dump(todo, open(jf, "w"))
    r = subprocess.run([blender, "-b", "--factory-startup", "-P", os.path.join(HERE, "impostors.py"), "--", tex, work, jf], capture_output=True, text=True)
    if r.returncode != 0 or "Traceback" in r.stdout + r.stderr:
        print((r.stdout + r.stderr)[-3000:]); raise SystemExit("impostors.py failed")
    frames = json.load(open(os.path.join(work, "impostors.json")))
    SW, SH = 192, 384
    atlas = np.zeros((ROWS * SH, COLS * SW, 4), np.uint8); lines = []
    for i, (m, s, k, v, snow, _) in enumerate(jobs):
        name = "%s_%d%s" % (m, v, "_snow" if snow else "")
        pic = os.path.join(work, name + ".png")
        if not os.path.exists(pic) or name not in frames: continue
        im = np.asarray(Image.open(pic).convert("RGBA").resize((SW, SH), Image.LANCZOS))
        cx, cy = i % COLS, i // COLS; atlas[cy * SH:(cy + 1) * SH, cx * SW:(cx + 1) * SW] = bleed(im)
        fw, fh = frames[name]
        lines.append("impostor %s %d %d %.6f %.6f %.6f %.6f %.3f %.3f" % (m, v, snow, (cx * SW + 0.5) / (COLS * SW), (cy * SH + 0.5) / (ROWS * SH),
                                                                     ((cx + 1) * SW - 0.5) / (COLS * SW), ((cy + 1) * SH - 0.5) / (ROWS * SH), fw, fh))
    Image.fromarray(atlas).save(os.path.join(out, "Textures", "impostors.png"))
    print("IMPOSTORS %d pictures (%d drawn now)" % (len(lines), len(todo)))
    return lines

def main():
    args = dict(a.split("=", 1) for a in sys.argv[1:])
    repo = os.path.dirname(os.path.dirname(HERE))
    tex = os.path.abspath(args.get("textures", os.path.join(repo, "build", "trees", "textures")))
    out = os.path.abspath(args.get("out", os.path.join(repo, "game", "assets", "Trees")))
    only = set(args["only"].split(",")) if "only" in args else None
    os.makedirs(os.path.join(out, "Models"), exist_ok=True); os.makedirs(os.path.join(out, "Textures"), exist_ok=True)
    jobs = [(m, s, k, v, tex, out) for (m, s, k, n) in MODELS if not only or m in only for v in range(n)]
    t0 = time.time()
    with Pool(max(1, min(8, (os.cpu_count() or 2) - 1))) as pool: done = pool.map(one, jobs)
    heights = {}
    for model, v, top, tris in done:
        heights.setdefault(model, {})[v] = top
        print("%-12s %d  height %5.1f  triangles by detail %s" % (model, v, top, " / ".join(str(t) for t in tris)))
    imp = impostors(tex, out, only, args.get("blender", r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"))
    kinds = set()
    old = {}
    lst = os.path.join(out, "trees.txt")
    if only and os.path.exists(lst):                       # a partial run keeps the other models' lines
        for line in open(lst):
            if line.strip() and not line.startswith("#"): old[line.split()[0]] = line.rstrip("\n")
    with open(lst, "w") as f:
        f.write("# model texture-kind variants conifer snow heights...: the tree library (art/trees/build_trees.py)\n")
        for (m, s, k, n) in MODELS:
            if m in heights:
                snow = int(os.path.exists(os.path.join(tex, "%s_leaves_snow.png" % k)))
                f.write("%s %s %d %d %d %s\n" % (m, k, n, int(tg.SPECIES[s]["conifer"]), snow, " ".join("%.2f" % heights[m][v] for v in range(n))))
                kinds.add(k)
            elif m in old: f.write(old[m] + "\n")
        for line in imp: f.write(line + "\n")
    for k in kinds:
        for part in ("bark", "bark_n", "leaves", "leaves_snow"):
            src = os.path.join(tex, "%s_%s.png" % (k, part))
            if os.path.exists(src):     # (as RGBA: the game's loader drew 24-bit bark pictures white, 2026-11-20)
                from PIL import Image
                Image.open(src).convert("RGBA").save(os.path.join(out, "Textures", "%s_%s.png" % (k, part)))
    size = sum(os.path.getsize(os.path.join(dp, f)) for dp, _, fs in os.walk(out) for f in fs)
    print("TREES %d models, %d variants in %.0f s; %s: %.0f MB" % (len(heights), len(done), time.time() - t0, out, size / 1e6))

if __name__ == "__main__":
    main()
