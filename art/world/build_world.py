# Builds the game world "RoadToAmirani": the Moonlit Sanctuary plus the long road to the mountain of Amirani.
#
# Usage (from art/world):
#   blender -b ../moonlit-sanctuary/Moonlit_Sanctuary_v01.blend -P build_world.py -- <out.glb> <Road.txt> <texture folder>
#
# All numbers below are in ENGINE coordinates: metres, X right, Y up, Z forward (toward the mountain).
# Blender position = (x, -z, y). The source .blend is not modified.
#
# Output:
#   <out.glb>    render nodes Doorways_MS_* (sanctuary), Doorways_RD_* (road world) and collision
#                COL_Walkable_MoonlitGround, COL_MoonlitBlockers, COL_Walkable_Road_### (one per road chunk)
#   <Road.txt>   what the game needs to know: path samples, arenas, chests, props, lights (see write_data)
#   <textures>   tiling ground textures referenced by the GLB (also embedded in it)
import bpy, bmesh, sys, re, math, os
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
OUT_GLB, OUT_DATA, TEX_DIR = argv[0], argv[1], argv[2]
os.makedirs(TEX_DIR, exist_ok=True)
rng = np.random.default_rng(20261110)

# ------------------------------------------------------------------ design tables
ROAD_HALF = 6.5          # m, half width of the road between arenas
ARENA_R = 17.0           # m, half width of the road inside an arena
BOSS_R = 24.0
STEP = 2.0               # m between path samples
GRID = 4.0               # m terrain cell
XMIN, XMAX, ZMIN, ZMAX = -680.0, 680.0, -180.0, 2010.0
MOUNTAIN = (0.0, 1900.0) # x, z of the peak of Amirani's mountain (GameConstants.h MountainX/Z)
LEDGE_Y, LEDGE_HALF, LEDGE_FRONT, LEDGE_BACK = 78.0, 38.0, 1872.0, 1889.0

# name, ground texture, bank height (+ valley walls / - ridge), noise amplitude, light colour
STAGES = [
    ("Moonlit Causeway",  "MossStone",  -7.0,  2.0, (0.55, 0.85, 1.00)),
    ("Whispering Wood",   "LeafLitter",  3.0,  3.0, (0.55, 1.00, 0.70)),
    ("Drowned Marsh",     "Mud",        -1.5,  0.3, (0.60, 1.00, 0.85)),
    ("Ochokochi's Wood",  "Needles",     5.0,  4.0, (0.75, 1.00, 0.60)),
    ("Stone Towers",      "Highland",    8.0,  5.0, (1.00, 0.80, 0.50)),
    ("Snow Pass",         "Snow",       18.0,  8.0, (0.70, 0.90, 1.00)),
    ("Kajeti",            "Ash",         6.0,  3.0, (1.00, 0.45, 0.15)),
    ("Giants' Valley",    "Scree",      14.0,  7.0, (1.00, 0.85, 0.60)),
    ("Dragon Lake",       "Pebbles",    -1.6,  0.4, (0.40, 0.85, 1.00)),
    ("Devi's Gorge",      "Ochre",      30.0,  6.0, (1.00, 0.78, 0.25)),
    ("Witches' Hill",     "Heath",      -8.0,  4.0, (0.75, 0.40, 1.00)),
    ("Chain Road",        "BareRock",  -26.0,  5.0, (0.60, 0.90, 1.00)),
]
WATER = {2: -13.1, 8: 17.1}  # stage -> water level
# waypoints (x, z, y); four spline segments per stage
W = [
    (0, 93, 0), (0, 150, 0), (-50, 225, -2), (-170, 280, -3), (-310, 275, -4),
    (-440, 305, -5), (-515, 400, -6), (-440, 485, -7), (-300, 470, -8),
    (-160, 440, -11), (-40, 505, -12), (95, 470, -12), (235, 440, -11),
    (380, 425, -4), (505, 485, 2), (520, 600, 6), (420, 665, 8),
    (285, 640, 12), (150, 695, 16), (20, 660, 19), (-110, 705, 22),
    (-250, 680, 32), (-400, 725, 46), (-510, 815, 52), (-465, 925, 46),
    (-340, 955, 38), (-200, 915, 35), (-70, 965, 34), (60, 930, 33),
    (200, 900, 31), (340, 945, 30), (470, 1015, 29), (500, 1135, 27),
    (400, 1215, 20), (260, 1185, 18), (130, 1235, 18), (0, 1200, 18),
    (-130, 1190, 20), (-270, 1245, 22), (-400, 1225, 24), (-500, 1325, 26),
    (-460, 1445, 31), (-330, 1495, 36), (-190, 1455, 39), (-80, 1525, 40),
    (0, 1590, 40), (0, 1680, 52), (0, 1780, 71), (0, 1846, 78), (0, 1872, 78),
]
# arenas per stage: (fraction of the stage length, kind) kind 0 = fight, 1 = boss
ARENAS = {0: [(.45, 0), (.85, 0)], 1: [(.2, 0), (.5, 0), (.85, 0)], 2: [(.2, 0), (.5, 0), (.85, 0)],
          3: [(.2, 0), (.5, 0), (.85, 0)], 4: [(.2, 0), (.5, 0), (.85, 0)], 5: [(.2, 0), (.5, 0), (.85, 0)],
          6: [(.2, 0), (.5, 0), (.85, 0)], 7: [(.2, 0), (.5, 0), (.85, 0)], 8: [(.25, 0), (.72, 1)],
          9: [(.2, 0), (.5, 0), (.85, 1)], 10: [(.2, 0), (.5, 0), (.85, 1)], 11: [(.18, 0), (.5, 0)]}

def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)

# ------------------------------------------------------------------ path
def catmull(P, per=64):
    P = np.array(P, float)
    ext = np.vstack([2 * P[0] - P[1], P, 2 * P[-1] - P[-2]])
    pts, seg = [], []
    for i in range(len(P) - 1):
        p0, p1, p2, p3 = ext[i], ext[i + 1], ext[i + 2], ext[i + 3]
        for k in range(per):
            t = k / per
            pts.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
            seg.append(i)
    pts.append(P[-1]); seg.append(len(P) - 2)
    return np.array(pts), np.array(seg)

dense, dseg = catmull(W)
dl = np.hypot(np.diff(dense[:, 0]), np.diff(dense[:, 1]))
ds = np.concatenate([[0], np.cumsum(dl)])
LENGTH = ds[-1]
N = int(LENGTH // STEP) + 1
S = np.arange(N) * STEP
PX = np.interp(S, ds, dense[:, 0]); PZ = np.interp(S, ds, dense[:, 1]); PY = np.interp(S, ds, dense[:, 2])
PSEG = np.interp(S, ds, dseg).astype(int)
PSTAGE = np.minimum(PSEG // 4, len(STAGES) - 1)
for _ in range(6):  # smooth heights
    PY[1:-1] = (PY[:-2] + 2 * PY[1:-1] + PY[2:]) / 4
# the Eagle's terrace (the last 54 m) is level with the ledge; the climb eases into it
FLAT = 27
PY[-FLAT:] = LEDGE_Y
PY[:-FLAT] = np.minimum(PY[:-FLAT], LEDGE_Y)
for _ in range(40):
    PY[-FLAT - 30:-FLAT] = (PY[-FLAT - 31:-FLAT - 1] + 2 * PY[-FLAT - 30:-FLAT] + PY[-FLAT - 29:-FLAT + 1]) / 4
PY[:10] = 0
tx = np.gradient(PX); tz = np.gradient(PZ); tl = np.hypot(tx, tz); tx /= tl; tz /= tl
NXv, NZv = tz, -tx     # right-hand side of travel (engine left-handed: forward z, right x)

stage_s = {}
for st in range(len(STAGES)):
    idx = np.where(PSTAGE == st)[0]
    stage_s[st] = (S[idx[0]], S[idx[-1]])
arenas = []  # (stage, s, kind, radius)
for st, lst in ARENAS.items():
    a, b = stage_s[st]
    for f, kind in lst:
        arenas.append((st, a + (b - a) * f, kind, BOSS_R if kind else ARENA_R))
arenas.append((11, LENGTH - 26.0, 2, 21.0))  # the terrace below Amirani: the Eagle
PW = np.full(N, ROAD_HALF)
for st, s0, kind, r in arenas:
    PW = np.maximum(PW, ROAD_HALF + (r - ROAD_HALF) * smooth(1.6 - np.abs(S - s0) / (r * 0.95)))
print("PATH length %.0f m, %d samples, %d arenas" % (LENGTH, N, len(arenas)))
for st in range(len(STAGES)):
    print("  stage %2d %-18s %5.0f .. %5.0f m" % (st, STAGES[st][0], *stage_s[st]))

# ------------------------------------------------------------------ noise and textures
def fbm(n, beta, seed, m=None):
    m = m or n
    r = np.random.default_rng(seed)
    kx = np.fft.fftfreq(n)[:, None]; ky = np.fft.fftfreq(m)[None, :]
    k = np.sqrt(kx * kx + ky * ky); k[0, 0] = 1
    f = (r.standard_normal((n, m)) + 1j * r.standard_normal((n, m))) / k ** beta
    f[0, 0] = 0
    out = np.real(np.fft.ifft2(f))
    out -= out.min(); out /= out.max()
    return out

def save_image(name, rgb, colorspace='sRGB'):
    n = rgb.shape[0]
    img = bpy.data.images.new("T_" + name, n, n, alpha=False)
    px = np.ones((n, n, 4), np.float32); px[:, :, :3] = np.clip(rgb, 0, 1)
    img.pixels.foreach_set(px.ravel())
    img.file_format = 'PNG'
    path = os.path.join(TEX_DIR, name + ".png")
    img.save(filepath=path)
    img = bpy.data.images.load(path)
    img.colorspace_settings.name = colorspace
    img.pack()
    return img

def normal_from_height(height, strength):
    # tangent-space normal map of a tiling height field (0..1)
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    nz = np.ones_like(height)
    l = np.sqrt(dx * dx + dy * dy + nz * nz)
    return np.stack([-dx / l * .5 + .5, -dy / l * .5 + .5, nz / l * .5 + .5], axis=2)

def ground_texture(name, a, b, seed, grain=0.10, speck=None, n=512, bump=5.0):
    big = fbm(n, 1.7, seed); fine = fbm(n, 0.7, seed + 1); mid = fbm(n, 1.2, seed + 3)
    rgb = np.array(a)[None, None, :] * (1 - big[:, :, None]) + np.array(b)[None, None, :] * big[:, :, None]
    rgb = rgb * (1 - grain + 2 * grain * fine[:, :, None])
    if speck:
        mask = (fbm(n, 0.35, seed + 2) > 0.80)[:, :, None]
        rgb = np.where(mask, np.array(speck)[None, None, :], rgb)
    height = .55 * mid + .30 * fine + .15 * big
    return save_image(name, rgb), save_image(name + "_N", normal_from_height(height, bump), 'Non-Color')

def cobble_texture(name, base, seed, n=512, stones=190, moss=None, snow=None, bump=6.0):
    # irregular cobbles: wrapped Voronoi cells with dark joints and rounded tops
    r = np.random.default_rng(seed)
    pts = r.uniform(0, 1, (stones, 2)); tone = r.uniform(0.70, 1.10, stones)
    v, u = np.mgrid[0:n, 0:n] / n
    d1 = np.full((n, n), 9.0); d2 = np.full((n, n), 9.0); idx = np.zeros((n, n), int)
    for k in range(stones):
        du = np.abs(u - pts[k, 0]); du = np.minimum(du, 1 - du)
        dv = np.abs(v - pts[k, 1]); dv = np.minimum(dv, 1 - dv)
        d = np.sqrt(du * du + dv * dv)
        closer = d < d1
        d2 = np.where(closer, d1, np.minimum(d2, d))
        idx = np.where(closer, k, idx); d1 = np.where(closer, d, d1)
    gap = d2 - d1
    stone = smooth(gap / 0.016)                     # 0 in the joints, 1 on the stone
    fine = fbm(n, 0.9, seed + 5)
    val = tone[idx] * (0.86 + 0.28 * fine) * (0.30 + 0.70 * stone)
    rgb = np.array(base)[None, None, :] * val[:, :, None]
    cover = fbm(n, 1.8, seed + 7)
    if moss:
        m = smooth((cover - 0.52) / 0.12)[:, :, None] * (1 - 0.5 * stone[:, :, None])
        rgb = rgb * (1 - m) + np.array(moss)[None, None, :] * (0.8 + 0.4 * fine[:, :, None]) * m
    if snow:
        drift = fbm(n, 1.1, seed + 8)
        m = np.maximum(smooth((drift - 0.60) / 0.10), 0.85 * (1 - stone))[:, :, None]
        rgb = rgb * (1 - m) + np.array(snow)[None, None, :] * (0.9 + 0.2 * fine[:, :, None]) * m
    height = stone * (0.75 + 0.25 * smooth(gap / 0.06)) + 0.08 * fine
    return save_image(name, rgb), save_image(name + "_N", normal_from_height(height, bump), 'Non-Color')

def plank_texture(name, base, seed, n=512, boards=7):
    r = np.random.default_rng(seed)
    v, u = np.mgrid[0:n, 0:n] / n
    board = np.floor(u * boards).astype(int)
    fu = u * boards - board
    offs = r.uniform(0, 1, boards)[board]; tone = r.uniform(0.7, 1.1, boards)[board]
    fv = (v * 2 + offs) % 1.0
    gap = (np.minimum(fu, 1 - fu) < 0.05) | (np.minimum(fv, 1 - fv) < 0.012)
    streak = fbm(n, 1.4, seed + 1)
    streak = (streak + np.roll(streak, 7, axis=0) + np.roll(streak, 15, axis=0) + np.roll(streak, 23, axis=0)) / 4  # stretch along the board
    val = np.where(gap, 0.22, tone * (0.75 + 0.5 * streak))
    height = np.where(gap, 0.0, 0.8 + 0.2 * streak)
    return save_image(name, np.array(base)[None, None, :] * val[:, :, None]), save_image(name + "_N", normal_from_height(height, 7.0), 'Non-Color')

MATS = {}
def material(name, images=None, color=(0.5, 0.5, 0.5), rough=0.9, metal=0.0):
    m = bpy.data.materials.new("RD_" + name)
    m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    bsdf.inputs['Roughness'].default_value = rough
    bsdf.inputs['Metallic'].default_value = metal
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    if images:
        tex = m.node_tree.nodes.new('ShaderNodeTexImage'); tex.image = images[0]
        m.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
        ntex = m.node_tree.nodes.new('ShaderNodeTexImage'); ntex.image = images[1]
        nmap = m.node_tree.nodes.new('ShaderNodeNormalMap')
        m.node_tree.links.new(ntex.outputs['Color'], nmap.inputs['Color'])
        m.node_tree.links.new(nmap.outputs['Normal'], bsdf.inputs['Normal'])
    MATS[name] = m
    return m

def make_materials():
    g = ground_texture
    material("MossStone", g("MossStone", (0.13, 0.20, 0.17), (0.21, 0.27, 0.22), 1))
    material("LeafLitter", g("LeafLitter", (0.11, 0.095, 0.07), (0.20, 0.16, 0.095), 2, speck=(0.29, 0.19, 0.09)))
    material("Mud", g("Mud", (0.085, 0.10, 0.075), (0.16, 0.17, 0.11), 3), rough=0.55)
    material("Needles", g("Needles", (0.085, 0.14, 0.08), (0.19, 0.17, 0.095), 4, speck=(0.12, 0.22, 0.10)))
    material("Highland", g("Highland", (0.14, 0.19, 0.10), (0.25, 0.24, 0.14), 5, speck=(0.30, 0.30, 0.27)))
    material("Snow", g("Snow", (0.36, 0.41, 0.49), (0.50, 0.55, 0.63), 6, grain=0.04, bump=2.5), rough=0.7)
    material("Ash", g("Ash", (0.075, 0.07, 0.07), (0.16, 0.145, 0.135), 7, speck=(0.50, 0.17, 0.04)))
    material("Scree", g("Scree", (0.17, 0.16, 0.145), (0.29, 0.265, 0.23), 8, grain=0.16, bump=8.0))
    material("Pebbles", g("Pebbles", (0.125, 0.16, 0.18), (0.25, 0.28, 0.28), 9, grain=0.18, bump=8.0))
    material("Ochre", g("Ochre", (0.20, 0.145, 0.075), (0.34, 0.245, 0.12), 10, speck=(0.62, 0.48, 0.15)))
    material("Heath", g("Heath", (0.125, 0.10, 0.16), (0.21, 0.17, 0.23), 11, speck=(0.31, 0.19, 0.36)))
    material("BareRock", g("BareRock", (0.145, 0.155, 0.17), (0.25, 0.26, 0.275), 12, grain=0.14, bump=8.0))
    material("Rock", g("Rock", (0.095, 0.11, 0.12), (0.19, 0.205, 0.215), 13, grain=0.15, bump=9.0))
    material("SnowRock", g("SnowRock", (0.22, 0.25, 0.30), (0.44, 0.49, 0.56), 14, grain=0.10, bump=6.0))
    material("Road", cobble_texture("Road", (0.27, 0.29, 0.29), 3, moss=(0.11, 0.17, 0.10)))
    material("RoadDark", cobble_texture("RoadDark", (0.17, 0.155, 0.15), 9))
    material("RoadSnow", cobble_texture("RoadSnow", (0.25, 0.27, 0.30), 5, snow=(0.46, 0.51, 0.59)))
    material("Wood", plank_texture("Wood", (0.21, 0.155, 0.10), 20), rough=0.8)
    material("Stone", g("Stone", (0.17, 0.195, 0.195), (0.28, 0.305, 0.30), 15, grain=0.12, bump=8.0))
    material("DarkIron", g("DarkIron", (0.06, 0.06, 0.07), (0.13, 0.115, 0.115), 16), rough=0.5, metal=0.5)
    material("Bone", g("Bone", (0.40, 0.37, 0.29), (0.56, 0.53, 0.43), 17, grain=0.08))
    material("Ice", g("Ice", (0.30, 0.48, 0.62), (0.52, 0.68, 0.78), 18, grain=0.05, bump=3.0), rough=0.2)
    material("Gold", g("Gold", (0.42, 0.30, 0.07), (0.72, 0.55, 0.16), 19, grain=0.10), rough=0.35, metal=0.9)
    material("Water", None, (0.012, 0.045, 0.060), rough=0.08)
    material("Lava", None, (0.95, 0.32, 0.05), rough=0.5)

ROAD_MAT = ["Road", "Road", "Wood", "Road", "Road", "RoadSnow", "RoadDark", "Road", "Road", "RoadDark", "RoadDark", "Road"]

# ------------------------------------------------------------------ mesh bags (engine coordinates)
class Bag:
    def __init__(self, sheet=False):
        self.v, self.f, self.uv, self.sheet = [], [], [], sheet
    def add(self, verts, faces, uvs):
        base = len(self.v)
        self.v.extend(verts)
        for face, fuv in zip(faces, uvs):
            self.f.append(tuple(base + i for i in face)); self.uv.append(fuv)
    def quad(self, a, b, c, d, t=6.0):
        # planar-ish quad with world-size UVs
        ax = np.array(b) - np.array(a); ay = np.array(d) - np.array(a)
        lu, lv = np.linalg.norm(ax) / t, np.linalg.norm(ay) / t
        self.add([a, b, c, d], [(0, 1, 2, 3)], [((0, 0), (lu, 0), (lu, lv), (0, lv))])
    def box(self, c, size, yaw=0.0, pitch=0.0, roll=0.0, t=4.0):
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        corners = np.array([(-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz), (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)])
        cy, sy, cp, sp, cr, sr = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch), math.cos(roll), math.sin(roll)
        Rr = np.array([(cr, -sr, 0), (sr, cr, 0), (0, 0, 1)]); Rp = np.array([(1, 0, 0), (0, cp, -sp), (0, sp, cp)]); Ry = np.array([(cy, 0, sy), (0, 1, 0), (-sy, 0, cy)])
        pts = corners @ (Ry @ Rp @ Rr).T + np.array(c)
        faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        dims = [(size[0], size[1]), (size[0], size[1]), (size[0], size[2]), (size[1], size[2]), (size[0], size[2]), (size[1], size[2])]
        uvs = [((0, 0), (0, d[1] / t), (d[0] / t, d[1] / t), (d[0] / t, 0)) for d in dims]
        self.add([tuple(p) for p in pts], faces, uvs)

BAGS = {}
def bag(name, sheet=False):
    if name not in BAGS:
        BAGS[name] = Bag(sheet)
    return BAGS[name]

def to_blender(v):
    v = np.asarray(v, float)
    return np.column_stack([v[:, 0], -v[:, 2], v[:, 1]])

def build_object(name, b, mat=None, smooth_shade=False):
    if not b.f:
        return None
    me = bpy.data.meshes.new(name)
    me.from_pydata(to_blender(b.v).tolist(), [], b.f)
    me.update()
    uv = me.uv_layers.new(name="UVMap")
    flat = [c for face in b.uv for pair in face for c in pair]
    uv.data.foreach_set("uv", flat)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    bm = bmesh.new(); bm.from_mesh(me)
    if b.sheet:
        for f in bm.faces:
            if f.normal.z < 0:
                f.normal_flip()
    else:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    if smooth_shade:
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    if mat:
        me.materials.append(mat)
    return o

# ------------------------------------------------------------------ terrain
def build_terrain():
    xs = np.arange(XMIN, XMAX + .1, GRID); zs = np.arange(ZMIN, ZMAX + .1, GRID)
    GX, GZ = np.meshgrid(xs, zs)            # [iz, ix]
    sub = slice(None, None, 3)
    px, pz, py, pw, pst = PX[sub], PZ[sub], PY[sub], PW[sub], PSTAGE[sub]
    bankA = np.array([STAGES[s][2] for s in pst]); bankN = np.array([STAGES[s][3] for s in pst])
    shape = GX.shape
    base = np.zeros(shape); A = np.zeros(shape); Nn = np.zeros(shape); dmin = np.zeros(shape); wn = np.zeros(shape); stn = np.zeros(shape, int)
    for r0 in range(0, shape[0], 24):
        r1 = min(shape[0], r0 + 24)
        d = np.hypot(GX[r0:r1, :, None] - px[None, None, :], GZ[r0:r1, :, None] - pz[None, None, :])
        wgt = 1.0 / (d + 3.0) ** 6
        ws = wgt.sum(axis=2)
        base[r0:r1] = (wgt * py).sum(axis=2) / ws
        A[r0:r1] = (wgt * bankA).sum(axis=2) / ws
        Nn[r0:r1] = (wgt * bankN).sum(axis=2) / ws
        near = d.argmin(axis=2)
        dmin[r0:r1] = d.min(axis=2); wn[r0:r1] = pw[near]; stn[r0:r1] = pst[near]
    noise = fbm(shape[0], 1.9, 101, shape[1]) * 2 - 1
    detail = fbm(shape[0], 1.2, 102, shape[1]) * 2 - 1
    ridged = 1 - np.abs(fbm(shape[0], 2.0, 103, shape[1]) * 2 - 1)
    out = dmin - wn
    h = base + A * smooth((out - 1.0) / 30.0) + Nn * (noise * 1.6 + detail * 0.5) * smooth(out / 55.0)
    # distant ridgelines ring the world, but leave Amirani's mountain alone
    dm = np.hypot(GX - MOUNTAIN[0], GZ - MOUNTAIN[1])
    far = smooth((out - 110.0) / 320.0) * smooth((dm - 330.0) / 220.0)
    h += far * (30 + 150 * ridged ** 2)
    # the corridor itself sits just under the road ribbon
    h = np.where(out < 1.0, base - 0.4, h)
    # the sanctuary floats over a chasm
    r = np.hypot(GX, GZ)
    h = h * smooth((r - 100.0) / 70.0) + (-70.0) * (1 - smooth((r - 100.0) / 70.0))
    # nothing pokes through the ledge and the terrace of the mountain
    near_ledge = (np.abs(GX) < 60) & (GZ > 1790)
    h = np.where(near_ledge, np.minimum(h, 30.0), h)
    # materials
    names = [s[1] for s in STAGES]
    matid = stn.copy()
    rock = len(names); snowrock = rock + 1
    far_cell = out > 120.0
    matid = np.where(far_cell, np.where((stn == 5) | (h > 95), snowrock, rock), matid)
    names += ["Rock", "SnowRock"]
    nz, nx = shape
    vid = np.arange(nz * nx).reshape(nz, nx)
    cell_mat = matid[:-1, :-1]
    for m, name in enumerate(names):
        cz, cx = np.where(cell_mat == m)
        if len(cz) == 0:
            continue
        quads = np.stack([vid[cz, cx], vid[cz, cx + 1], vid[cz + 1, cx + 1], vid[cz + 1, cx]], axis=1)
        used, inv = np.unique(quads, return_inverse=True)
        iz, ix = np.divmod(used, nx)
        verts = np.column_stack([GX[iz, ix], h[iz, ix], GZ[iz, ix]])
        b = Bag(sheet=True)
        b.v = verts.tolist(); faces = inv.reshape(-1, 4)
        b.f = [tuple(q) for q in faces.tolist()]
        uvx, uvz = verts[:, 0] / 7.0, verts[:, 2] / 7.0
        b.uv = [tuple((uvx[i], uvz[i]) for i in q) for q in faces.tolist()]
        build_object("Doorways_RD_Ground_" + name, b, MATS[name], smooth_shade=True)
        print("TERRAIN", name, len(b.f))
    # water sheets
    for st, level in WATER.items():
        cz, cx = np.where((stn[:-1, :-1] == st) & (h[:-1, :-1] < level + 1.5) & (dmin[:-1, :-1] < 260))
        b = bag("Water", sheet=True)
        for z, x in zip(cz, cx):
            x0, z0 = xs[x], zs[z]
            b.add([(x0, level, z0), (x0 + GRID, level, z0), (x0 + GRID, level, z0 + GRID), (x0, level, z0 + GRID)], [(0, 1, 2, 3)], [((0, 0), (1, 0), (1, 1), (0, 1))])
    return xs, zs, h, dmin - wn

def sampler(xs, zs, field):
    def at(x, z):
        fx = np.clip((x - xs[0]) / GRID, 0, len(xs) - 1.001); fz = np.clip((z - zs[0]) / GRID, 0, len(zs) - 1.001)
        ix, iz = int(fx), int(fz); tx_, tz_ = fx - ix, fz - iz
        return (field[iz, ix] * (1 - tx_) + field[iz, ix + 1] * tx_) * (1 - tz_) + (field[iz + 1, ix] * (1 - tx_) + field[iz + 1, ix + 1] * tx_) * tz_
    return at

# ------------------------------------------------------------------ road ribbon and its collision
def edge(i, side, extra=0.0):
    w = PW[i] + extra
    return (PX[i] + NXv[i] * w * side, PY[i], PZ[i] + NZv[i] * w * side)

def is_bridge(i):
    return S[i] < 95.0 or PSTAGE[i] == 11 or PSTAGE[i] in WATER

def build_road():
    for i in range(N - 1):
        st = PSTAGE[i]
        b = bag("Road_" + ROAD_MAT[st], sheet=True)
        cols = [-1.0, -0.5, 0.0, 0.5, 1.0]
        rows = []
        for j in (i, i + 1):
            rows.append([(PX[j] + NXv[j] * PW[j] * c, PY[j], PZ[j] + NZv[j] * PW[j] * c) for c in cols])
        for k in range(4):
            u0, u1 = (cols[k] * PW[i]) / 4.0, (cols[k + 1] * PW[i]) / 4.0
            b.add([rows[0][k], rows[0][k + 1], rows[1][k + 1], rows[1][k]], [(0, 1, 2, 3)],
                  [((u0, S[i] / 4.0), (u1, S[i] / 4.0), (u1, S[i + 1] / 4.0), (u0, S[i + 1] / 4.0))])
        # skirts hide the seam with the terrain; bridges get a deep stone side
        deep = 5.0 if is_bridge(i) else 1.6
        sk = bag("Stone" if is_bridge(i) else "Road_" + ROAD_MAT[st])
        for side in (-1, 1):
            a0, a1 = edge(i, side), edge(i + 1, side)
            out0, out1 = edge(i, side, 0.0 if is_bridge(i) else 1.8), edge(i + 1, side, 0.0 if is_bridge(i) else 1.8)
            b0 = (out0[0], a0[1] - deep, out0[2]); b1 = (out1[0], a1[1] - deep, out1[2])
            sk.add([a0, a1, b1, b0], [(0, 1, 2, 3)], [((S[i] / 4, 0), (S[i + 1] / 4, 0), (S[i + 1] / 4, deep / 4), (S[i] / 4, deep / 4))])
        if is_bridge(i):  # underside
            l0, r0, l1, r1 = edge(i, -1), edge(i, 1), edge(i + 1, -1), edge(i + 1, 1)
            sk.add([(l0[0], l0[1] - deep, l0[2]), (r0[0], r0[1] - deep, r0[2]), (r1[0], r1[1] - deep, r1[2]), (l1[0], l1[1] - deep, l1[2])],
                   [(0, 1, 2, 3)], [((0, 0), (1, 0), (1, 1), (0, 1))])
    # the ledge under Amirani keeps the mountain's own rock floor (a second floor here flickered against it)

def build_collision():
    chunk = 24
    count = 0
    for c0 in range(0, N - 1, chunk):
        c1 = min(N - 1, c0 + chunk)
        b = Bag(sheet=True)
        for i in range(c0, c1):
            # 3 m wider than the road: the game keeps her on the road and lets her slide along its edge
            b.add([edge(i, -1, 3.0), edge(i, 1, 3.0), edge(i + 1, 1, 3.0), edge(i + 1, -1, 3.0)], [(0, 1, 2, 3)], [((0, 0),) * 4])
        build_object("COL_Walkable_Road_%03d" % count, b)
        count += 1
    b = Bag(sheet=True)
    # starts 5 m before the ledge so it overlaps the end of the terrace (no gap to step over)
    b.add([(-LEDGE_HALF, LEDGE_Y, LEDGE_FRONT - 5), (LEDGE_HALF, LEDGE_Y, LEDGE_FRONT - 5), (LEDGE_HALF, LEDGE_Y, LEDGE_BACK), (-LEDGE_HALF, LEDGE_Y, LEDGE_BACK)], [(0, 1, 2, 3)], [((0, 0),) * 4])
    build_object("COL_Walkable_Road_Ledge", b)
    # the sanctuary south district to the first road sample
    b = Bag(sheet=True)
    b.add([(-ROAD_HALF, 0, 90.0), (ROAD_HALF, 0, 90.0), (ROAD_HALF, 0, 94.0), (-ROAD_HALF, 0, 94.0)], [(0, 1, 2, 3)], [((0, 0),) * 4])
    build_object("COL_Walkable_Road_Join", b)
    print("COLLISION chunks", count + 2)

# ------------------------------------------------------------------ props baked into the scene
LIGHTS, PROPS, CHESTS = [], [], []

def yaw_of(i):
    return math.atan2(tx[i], tz[i])

def gate(i, mat="Stone", height=9.0):
    w = PW[i] + 1.6; yaw = yaw_of(i); b = bag(mat)
    for side in (-1, 1):
        c = (PX[i] + NXv[i] * w * side, PY[i] + height / 2 - 0.5, PZ[i] + NZv[i] * w * side)
        b.box(c, (2.4, height + 1, 2.4), yaw)
        b.box((c[0], PY[i] + height + 0.6, c[2]), (3.2, 0.9, 3.2), yaw)
        b.box((c[0], PY[i] + 0.4, c[2]), (3.4, 1.4, 3.4), yaw)
    # lintel of three stones
    b.box((PX[i], PY[i] + height + 1.6, PZ[i]), (2 * w + 3.4, 1.5, 2.6), yaw)
    b.box((PX[i], PY[i] + height + 2.9, PZ[i]), (2 * w * 0.6, 1.2, 2.2), yaw)
    b.box((PX[i], PY[i] + height + 4.0, PZ[i]), (2.2, 1.4, 1.8), yaw)

def lantern(i, side, color):
    w = PW[i] + 0.9
    x, y, z = PX[i] + NXv[i] * w * side, PY[i], PZ[i] + NZv[i] * w * side
    b = bag("Stone")
    b.box((x, y + 0.9, z), (0.5, 2.6, 0.5), yaw_of(i))
    b.box((x, y + 2.35, z), (0.9, 0.25, 0.9), yaw_of(i))
    LIGHTS.append((x, y + 2.9, z, *color, 1.5))

def boulders(xs, zs, height_at, clear_at):
    b = bag("Rock"); bs = bag("SnowRock"); count = 0
    ico = bmesh.new(); bmesh.ops.create_icosphere(ico, subdivisions=1, radius=1.0)
    iv = np.array([v.co[:] for v in ico.verts]); ifc = [tuple(v.index for v in f.verts) for f in ico.faces]; ico.free()
    density = [0.5, 0.7, 0.15, 0.6, 1.0, 0.8, 0.6, 1.8, 0.4, 1.4, 0.8, 0.6]
    for i in range(0, N, 2):
        st = PSTAGE[i]
        for _ in range(rng.poisson(density[st])):
            side = rng.choice((-1, 1)); off = PW[i] + 1.5 + rng.gamma(2.0, 9.0)
            x, z = PX[i] + NXv[i] * off * side, PZ[i] + NZv[i] * off * side
            if clear_at(x, z) < 1.2 or not (XMIN < x < XMAX and ZMIN < z < ZMAX) or math.hypot(x, z) < 150:
                continue
            size = float(np.clip(rng.gamma(2.0, 0.8), 0.5, 7.0)) * (1.6 if st in (7, 9) else 1.0)
            sc = np.array([size * rng.uniform(0.7, 1.3), size * rng.uniform(0.5, 1.0), size * rng.uniform(0.7, 1.3)])
            verts = iv * (1 + rng.uniform(-0.22, 0.22, (len(iv), 1))) * sc
            a = rng.uniform(0, 6.283); ca, sa = math.cos(a), math.sin(a)
            verts = np.column_stack([verts[:, 0] * ca - verts[:, 2] * sa, verts[:, 1], verts[:, 0] * sa + verts[:, 2] * ca])
            verts += np.array([x, height_at(x, z) + sc[1] * 0.25, z])
            target = bs if st == 5 else b
            target.add([tuple(p) for p in verts], ifc, [tuple((float(verts[k][0] / 3), float(verts[k][2] / 3 + verts[k][1] / 3)) for k in f) for f in ifc])
            count += 1
    print("BOULDERS", count)

def crystal(b, x, y, z, size, lean):
    for k in range(int(rng.integers(3, 6))):
        a = rng.uniform(0, 6.283); r = rng.uniform(0, size * 0.5); hgt = size * rng.uniform(0.8, 2.4)
        b.box((x + math.cos(a) * r, y + hgt * 0.4, z + math.sin(a) * r), (size * 0.35, hgt, size * 0.35), rng.uniform(0, 3.1), rng.uniform(-lean, lean), rng.uniform(-lean, lean))

def scatter(xs, zs, height_at, clear_at):
    # per stage: (Meshy prop type, spacing along the road, min/max scale, spread)
    table = {0: [("deadtree", 26, 6, 9, 14), ("rocks", 60, 4, 7, 20)],
             1: [("deadtree", 5, 6, 12, 26), ("rocks", 45, 4, 7, 22)],
             2: [("deadtree", 22, 5, 9, 30), ("rocks", 70, 3, 5, 25)],
             3: [("fir", 4, 9, 18, 30), ("rocks", 50, 4, 7, 22)],
             4: [("tower", 70, 22, 30, 26), ("fir", 12, 9, 15, 34), ("rocks", 40, 4, 8, 22)],
             5: [("fir", 7, 9, 17, 32), ("rocks", 45, 5, 9, 24)],
             6: [("tower", 90, 24, 32, 22), ("deadtree", 30, 6, 10, 26), ("rocks", 50, 4, 8, 22)],
             7: [("rocks", 14, 6, 16, 30), ("fir", 30, 9, 14, 34)],
             8: [("rocks", 30, 4, 8, 22), ("fir", 18, 9, 15, 34)],
             9: [("rocks", 16, 6, 14, 26), ("deadtree", 40, 6, 10, 24)],
             10: [("deadtree", 9, 7, 13, 26), ("rocks", 35, 5, 9, 22)],
             11: [("rocks", 40, 5, 9, 18)]}
    for st, rows in table.items():
        a, b_ = stage_s[st]
        for kind, spacing, smin, smax, spread in rows:
            s = a + rng.uniform(0, spacing)
            while s < b_:
                i = int(s / STEP); side = rng.choice((-1, 1))
                off = PW[i] + (9 if kind == "tower" else 3.0) + rng.gamma(2.0, spread / 2.0)
                x, z = PX[i] + NXv[i] * off * side, PZ[i] + NZv[i] * off * side
                if clear_at(x, z) > (7 if kind == "tower" else 2.2) and XMIN + 20 < x < XMAX - 20 and ZMIN + 20 < z < ZMAX - 60 and math.hypot(x, z) > 160 and not is_bridge(i):
                    PROPS.append((kind, x, height_at(x, z) - 0.3, z, rng.uniform(0, 6.283), rng.uniform(smin, smax)))
                s += spacing * rng.uniform(0.6, 1.4)

def decorate(xs, zs, height_at, clear_at):
    stone = bag("Stone")
    # gates at the sanctuary and at every stage border; lanterns along the whole road
    gate(2)
    for st in range(1, len(STAGES)):
        gate(int(stage_s[st][0] / STEP), "DarkIron" if st in (6, 7) else "Stone", 9.0 + (3 if st in (6, 11) else 0))
    for k, i in enumerate(range(8, N - 20, 22)):
        lantern(i, 1 if k % 2 else -1, STAGES[PSTAGE[i]][4])
    # arena markers: a ring of standing stones just outside each arena, braziers at boss arenas
    for st, s0, kind, r in arenas:
        i = int(s0 / STEP); col = STAGES[st][4]
        n = 10 if kind == 0 else 14
        for k in range(n):
            a = 6.283 * (k + 0.5) / n
            x, z = PX[i] + math.cos(a) * (r + 2.2), PZ[i] + math.sin(a) * (r + 2.2)
            along = abs((x - PX[i]) * tx[i] + (z - PZ[i]) * tz[i])
            if along > r * 0.72:      # leave the road open
                continue
            if abs(x) < 46 and z > 1868:   # never inside the ledge
                continue
            hgt = rng.uniform(3.0, 6.5) * (1.5 if kind else 1.0)
            bag("Bone" if st == 7 else "Ice" if st == 5 else "DarkIron" if st == 6 else "Stone").box(
                (x, PY[i] + hgt / 2 - 0.6, z), (1.5, hgt, 1.1), a + 1.57, rng.uniform(-.12, .12), rng.uniform(-.12, .12))
            if k % 2 == 0:
                LIGHTS.append((x, PY[i] + hgt + 0.3, z, *col, 2.2 if kind else 1.6))
    # bridges: parapets and pillars
    for i in range(0, N - 1):
        if not is_bridge(i) or PW[i] > ROAD_HALF + 0.5:
            continue
        seg = math.hypot(PX[i + 1] - PX[i], PZ[i + 1] - PZ[i])
        wood = PSTAGE[i] == 2
        for side in (-1, 1):
            e = edge(i, side, -0.3)
            if wood:
                if i % 3 == 0:
                    bag("Wood").box((e[0], e[1] + 0.2, e[2]), (0.35, 2.6, 0.35), yaw_of(i))
            else:
                stone.box((e[0], e[1] + 0.45, e[2]), (0.6, 1.1, seg + 0.05), yaw_of(i), -math.atan2(PY[i + 1] - PY[i], seg))
                if i % 6 == 0:
                    stone.box((e[0], e[1] + 1.2, e[2]), (0.9, 0.7, 0.9), yaw_of(i))
        if i % 14 == 0 and PSTAGE[i] not in WATER:
            stone.box((PX[i], PY[i] - 45, PZ[i]), (5.5, 82, 5.5), yaw_of(i))
            stone.box((PX[i], PY[i] - 5.2, PZ[i]), (2 * PW[i] + 1, 1.6, 7.5), yaw_of(i))
    # Kajeti: fortress walls with merlons, forge towers, lava channels
    a, b_ = stage_s[6]
    iron = bag("DarkIron")
    for i in range(int(a / STEP) + 6, int(b_ / STEP) - 6):
        if PW[i] > ROAD_HALF + 0.5:
            continue
        seg = math.hypot(PX[i + 1] - PX[i], PZ[i + 1] - PZ[i])
        for side in (-1, 1):
            e = edge(i, side, 5.5)
            iron.box((e[0], PY[i] + 3.4, e[2]), (1.6, 8.5, seg + 0.1), yaw_of(i))
            if i % 2 == 0:
                iron.box((e[0], PY[i] + 8.3, e[2]), (1.6, 1.3, 1.1), yaw_of(i))
            if i % 26 == 0:
                iron.box((e[0], PY[i] + 7.0, e[2]), (5.5, 16, 5.5), yaw_of(i))
                iron.box((e[0], PY[i] + 15.6, e[2]), (6.6, 1.4, 6.6), yaw_of(i))
                LIGHTS.append((e[0], PY[i] + 17.4, e[2], 1.0, 0.45, 0.12, 4.0))
            a0, a1, b0, b1 = edge(i, side, 2.2), edge(i + 1, side, 2.2), edge(i, side, 4.0), edge(i + 1, side, 4.0)
            bag("Lava", sheet=True).add([(a0[0], PY[i] - 0.25, a0[2]), (a1[0], PY[i + 1] - 0.25, a1[2]), (b1[0], PY[i + 1] - 0.25, b1[2]), (b0[0], PY[i] - 0.25, b0[2])],
                                        [(0, 1, 2, 3)], [((0, 0), (1, 0), (1, 1), (0, 1))])
            if i % 9 == 0:
                c = edge(i, side, 3.1)
                LIGHTS.append((c[0], PY[i] + 0.5, c[2], 1.0, 0.40, 0.08, 2.6))
    # Giants' Valley: ribs of a dead giant arch over the road
    a, b_ = stage_s[7]
    bone = bag("Bone")
    for i in range(int(a / STEP) + 20, int(b_ / STEP) - 10, 9):
        if PW[i] > ROAD_HALF + 0.5 or (i // 9) % 5 > 2:
            continue
        span = PW[i] + 4.0
        for side in (-1, 1):
            prev = None
            for k in range(9):
                t = k / 8; ang = t * 1.35
                off = span * math.cos(ang) + 0.0; up = (span + 3.0) * math.sin(ang)
                p = (PX[i] + NXv[i] * off * side, PY[i] + up - 0.5, PZ[i] + NZv[i] * off * side)
                if prev:
                    mid = tuple((p[q] + prev[q]) / 2 for q in range(3))
                    ln = math.dist(p, prev)
                    bone.box(mid, (1.1 - 0.6 * t, ln + 0.3, 1.3 - 0.7 * t), yaw_of(i) , 0.0, side * (ang - 0.08) )
                prev = p
    # Snow Pass and Devi's Gorge and Witches' Hill: crystals with light
    for st, mat, col, every in ((5, "Ice", (0.6, 0.9, 1.0), 16), (9, "Gold", (1.0, 0.75, 0.2), 13), (10, "Ice", (0.7, 0.35, 1.0), 19)):
        a, b_ = stage_s[st]
        for i in range(int(a / STEP) + 5, int(b_ / STEP) - 5, every):
            side = rng.choice((-1, 1)); off = PW[i] + rng.uniform(2.0, 9.0)
            x, z = PX[i] + NXv[i] * off * side, PZ[i] + NZv[i] * off * side
            if clear_at(x, z) < 1.5:
                continue
            y = height_at(x, z)
            crystal(bag(mat), x, y, z, rng.uniform(0.7, 1.8), 0.35)
            LIGHTS.append((x, y + 1.6, z, *col, 2.0))
    # Stone Towers: ruined walls beside the road
    a, b_ = stage_s[4]
    for i in range(int(a / STEP) + 10, int(b_ / STEP) - 10, 17):
        side = rng.choice((-1, 1)); off = PW[i] + rng.uniform(3.0, 8.0)
        x, z = PX[i] + NXv[i] * off * side, PZ[i] + NZv[i] * off * side
        if clear_at(x, z) < 2.5:
            continue
        y = height_at(x, z); yaw = yaw_of(i) + rng.uniform(-.4, .4)
        for k in range(int(rng.integers(2, 5))):
            hgt = rng.uniform(1.2, 4.5)
            stone.box((x + math.sin(yaw) * k * 2.6, y + hgt / 2 - 0.4, z + math.cos(yaw) * k * 2.6), (1.2, hgt, 2.7), yaw)
    # Drowned Marsh and Dragon Lake: will-o'-the-wisps over the water, drowned posts
    for st, col in ((2, (0.55, 1.0, 0.75)), (8, (0.35, 0.80, 1.0))):
        a, b_ = stage_s[st]
        for i in range(int(a / STEP) + 4, int(b_ / STEP) - 4, 7):
            side = rng.choice((-1, 1)); off = PW[i] + rng.uniform(4.0, 38.0)
            x, z = PX[i] + NXv[i] * off * side, PZ[i] + NZv[i] * off * side
            if clear_at(x, z) < 3.0:
                continue
            LIGHTS.append((x, WATER[st] + rng.uniform(0.8, 2.2), z, *col, rng.uniform(0.7, 1.3)))
            if rng.uniform() < 0.6:
                hgt = rng.uniform(1.5, 4.5)
                bag("Wood").box((x + rng.uniform(-3, 3), WATER[st] + hgt / 2 - 0.8, z + rng.uniform(-3, 3)), (0.45, hgt, 0.45), rng.uniform(0, 3), rng.uniform(-.25, .25), rng.uniform(-.25, .25))
    # chests: old gold boxes beside the road and in every arena
    k = 0
    for st in range(len(STAGES)):
        a, b_ = stage_s[st]
        for f in (0.08, 0.34, 0.64, 0.93):
            i = int((a + (b_ - a) * f) / STEP); side = 1 if k % 2 else -1; k += 1
            w = PW[i] - 1.6
            CHESTS.append((PX[i] + NXv[i] * w * side, PY[i], PZ[i] + NZv[i] * w * side, yaw_of(i) + (1.57 if side < 0 else -1.57), 0))
    for st, s0, kind, r in arenas:
        if kind == 2:
            continue
        i = int(s0 / STEP)
        CHESTS.append((PX[i] + NXv[i] * (r - 2.5), PY[i], PZ[i] + NZv[i] * (r - 2.5), yaw_of(i) - 1.57, 1 + kind))

# ------------------------------------------------------------------ sanctuary (same preparation as Export/export_game_glb.py)
def prepare_sanctuary():
    WALKABLE = re.compile(r"^(Arena surface|Arena foundation|Radial bridge|Outer circular foundations|District |Outlook foundation|Outlook paving|MS_Temple_step|MS_Temple_foundation|MS_Temple_inner_floor)")
    BLOCKER = re.compile(r"^(Bridge parapet|Ring outer battlement|MS_Arch_left_pier|MS_Arch_right_pier|MS_Arch_plinth|MS_Bench_|MS_Broken_column|MS_Column_base|MS_Column_foot|MS_Temple_column|MS_Temple_rear_wall|MS_Temple_side_wall|MS_Temple_side_bay|MS_Temple_tower|MS_Temple_tower_base|Nature_DeadTree|Nature_Grave|Soul lantern plinth)")
    DROP = re.compile(r"^(PLAYER SCALE|Arena central seal)")   # the seal was the white disc in the middle of the arena
    scene = bpy.context.scene
    for o in list(scene.objects):
        if o.type != 'MESH' or DROP.match(o.name):
            bpy.data.objects.remove(o, do_unlink=True)
    PALETTE = {
        "Paving 00": (0.035, 0.080, 0.052, 1), "Paving 01": (0.045, 0.095, 0.062, 1), "Paving 02": (0.052, 0.105, 0.072, 1),
        "Paving 03": (0.062, 0.120, 0.085, 1), "Paving 04": (0.030, 0.068, 0.046, 1), "Stone - pale trim": (0.30, 0.36, 0.36, 1),
        "Stone - shadow": (0.016, 0.040, 0.030, 1), "Stone - slate": (0.040, 0.095, 0.072, 1), "Stone - weathered": (0.075, 0.150, 0.095, 1),
        "Trees - black bark": (0.020, 0.060, 0.034, 1), "Soul - icy blue": (0.62, 0.90, 1.00, 1), "Lantern glow": (0.66, 1.00, 0.78, 1),
        "Iron": (0.24, 0.27, 0.29, 1)}
    for m in bpy.data.materials:
        if m.name in PALETTE:
            m.diffuse_color = PALETTE[m.name]
        if not m.node_tree:
            continue
        for n in m.node_tree.nodes:
            if n.type == 'BSDF_PRINCIPLED':
                for link in list(n.inputs['Base Color'].links):
                    m.node_tree.links.remove(link)
                n.inputs['Base Color'].default_value = m.diffuse_color
                n.inputs['Metallic'].default_value = 0.6 if m.name == 'Iron' else 0.0
                n.inputs['Roughness'].default_value = 0.85
                if 'Emission Strength' in n.inputs:
                    n.inputs['Emission Strength'].default_value = 0.0
    dg = bpy.context.evaluated_depsgraph_get()
    def evaluated_copy(o, name):
        me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
        me.transform(o.matrix_world)
        c = bpy.data.objects.new(name, me)
        scene.collection.objects.link(c)
        return c
    walk, block, render = [], [], {}
    for o in list(scene.objects):
        base = re.sub(r"\.\d+$", "", o.name)
        if WALKABLE.match(base):
            walk.append(evaluated_copy(o, "w"))
        elif BLOCKER.match(base):
            block.append(evaluated_copy(o, "b"))
        c = evaluated_copy(o, "r")
        c.data.materials.clear()
        mat = o.active_material
        c.data.materials.append(mat)
        render.setdefault(mat.name if mat else "Default", []).append(c)
        bpy.data.objects.remove(o, do_unlink=True)
    def join(objs, name):
        bpy.ops.object.select_all(action='DESELECT')
        for x in objs:
            x.select_set(True)
        bpy.context.view_layer.objects.active = objs[0]
        if len(objs) > 1:
            bpy.ops.object.join()
        j = bpy.context.view_layer.objects.active
        j.name = j.data.name = name
        return j
    for mat, objs in render.items():
        join(objs, "Doorways_MS_" + re.sub(r"[^A-Za-z0-9]+", "_", mat).strip("_"))
    for objs, name in ((walk, "COL_Walkable_MoonlitGround"), (block, "COL_MoonlitBlockers")):
        j = join(objs, name)
        j.data.materials.clear()

# ------------------------------------------------------------------ data for the game
def write_data():
    with open(OUT_DATA, "w", encoding="ascii") as f:
        f.write("# Road to Amirani: written by art/world/build_world.py. Engine coordinates, metres.\n")
        f.write("# S index name | P x y z halfwidth stage | A stage s kind radius x y z | C x y z yaw tier | T type x y z yaw height | L x y z r g b size\n")
        f.write("STEP %.3f\n" % STEP)
        for st, s in enumerate(STAGES):
            f.write("S %d %s\n" % (st, s[0].replace(" ", "_")))
        for i in range(N):
            f.write("P %.3f %.3f %.3f %.3f %d\n" % (PX[i], PY[i], PZ[i], PW[i], PSTAGE[i]))
        for st, s0, kind, r in sorted(arenas, key=lambda a: a[1]):
            i = int(s0 / STEP)
            f.write("A %d %.2f %d %.2f %.3f %.3f %.3f\n" % (st, s0, kind, r, PX[i], PY[i], PZ[i]))
        for c in CHESTS:
            f.write("C %.3f %.3f %.3f %.3f %d\n" % c)
        for p in PROPS:
            f.write("T %s %.3f %.3f %.3f %.3f %.3f\n" % p)
        for l in LIGHTS:
            f.write("L %.3f %.3f %.3f %.2f %.2f %.2f %.2f\n" % l)
    print("DATA chests %d props %d lights %d" % (len(CHESTS), len(PROPS), len(LIGHTS)))

# ------------------------------------------------------------------ main
prepare_sanctuary()
make_materials()
xs, zs, H, CLEAR = build_terrain()
height_at, clear_at = sampler(xs, zs, H), sampler(xs, zs, CLEAR)
build_road()
boulders(xs, zs, height_at, clear_at)
scatter(xs, zs, height_at, clear_at)
decorate(xs, zs, height_at, clear_at)
for name, b in BAGS.items():
    mat = name[5:] if name.startswith("Road_") else name
    build_object("Doorways_RD_" + name, b, MATS[mat], smooth_shade=name in ("Rock", "SnowRock"))
    print("BAG", name, len(b.f))
build_collision()
write_data()
tris = sum(len(o.data.polygons) for o in bpy.context.scene.objects)
print("OBJECTS", len(bpy.context.scene.objects), "FACES", tris)
bpy.ops.export_scene.gltf(filepath=OUT_GLB, export_format='GLB', use_selection=False, export_apply=True,
                          export_yup=True, export_lights=False, export_cameras=False)
print("EXPORTED", OUT_GLB)
