"""Builds the mountain of Amirani: a Caucasus peak with an open cave ledge facing the play area.

Usage:  blender -b -P build_mountain.py -- <output folder>
Writes: Mountain.glb            render mesh (planar UV, one material)
        MountainBaseColor.png   2048x2048 base colour (rock, scree, snow)
        Mountain.json           numbers the game needs (ledge centre, wall, heights), in engine axes

Engine axes: metres, Y up, the ledge faces -Z. The mesh is centred on the peak in X/Z; Y = 0 is
the height of the play area floor. Deterministic: fixed seeds.
"""
import bpy, sys, json, pathlib
import numpy as np

OUT = pathlib.Path(sys.argv[sys.argv.index("--") + 1])
OUT.mkdir(parents=True, exist_ok=True)

SIZE = 520.0        # square footprint, metres
N = 261             # grid resolution (2 m cells)
PEAK = 250.0        # peak height above the base
BASE_Y = -90.0      # base level relative to the play area floor
LEDGE_Y = 78.0      # ledge height relative to the play area floor
LEDGE_HALF_W = 40.0
LEDGE_DEPTH = 36.0


def fbm(n, beta, seed):
    rng = np.random.default_rng(seed)
    f = np.hypot(np.fft.fftfreq(n)[:, None] * n, np.fft.fftfreq(n)[None, :] * n)
    f[0, 0] = 1
    spec = (rng.normal(size=(n, n)) + 1j * rng.normal(size=(n, n))) / f ** (beta / 2)
    spec[0, 0] = 0
    a = np.fft.ifft2(spec).real
    return (a - a.min()) / (a.max() - a.min())


def smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


axis = np.linspace(-SIZE / 2, SIZE / 2, N)
X, Z = np.meshgrid(axis, axis)                 # X[i, j]: i along Z, j along X
R = np.hypot(X, Z)

ridged = 1 - np.abs(2 * fbm(N, 3.3, 1) - 1)    # crests
soft = fbm(N, 3.0, 2)
detail = fbm(N, 1.9, 3)


def cone(cx, cz, height, sigma, power=1.5):
    return height * np.exp(-(np.hypot(X - cx, Z - cz) / sigma) ** power)


H = cone(0, 40, PEAK, 122, 1.55)               # main peak, set back from the ledge
H = np.maximum(H, cone(-140, 70, 165, 84))     # left shoulder
H = np.maximum(H, cone(135, 95, 185, 88))      # right shoulder
H = np.maximum(H, cone(40, 150, 130, 80))
H *= 0.80 + 0.36 * ridged
H += 14 * soft + 2.5 * detail
H *= 1 - smooth(R, SIZE * 0.36, SIZE * 0.50)   # fall to the base at the footprint edge
Y = H + BASE_Y

# Cave ledge: flat shelf cut into the -Z face. Walls stay vertical (no blend) at the sides and back.
centre_col = N // 2
profile = Y[:, centre_col]
front_i = next(i for i in range(N // 2) if profile[i] >= LEDGE_Y - 8)   # first row reaching the ledge
z_front = axis[front_i]
z_back = z_front + LEDGE_DEPTH
inside = (np.abs(X) <= LEDGE_HALF_W) & (Z >= z_front) & (Z <= z_back)
natural = Y.copy()
Y[inside] = LEDGE_Y + 0.8 * (detail[inside] - 0.5)
# clear the slope in front of the ledge so the shelf and whoever stands on it are seen from below
front = (np.abs(X) <= LEDGE_HALF_W + 14) & (Z >= z_front - 70) & (Z < z_front)
Y[front] = np.minimum(Y[front], LEDGE_Y - 2 - 0.85 * (z_front - Z[front]))
# raise the rock behind and beside the ledge so the back wall is tall
rim = (np.abs(X) <= LEDGE_HALF_W + 26) & (Z > z_back) & (Z <= z_back + 30)
Y[rim] = np.maximum(Y[rim], LEDGE_Y + 50 + 34 * soft[rim] + 22 * ridged[rim])
side = (np.abs(X) > LEDGE_HALF_W) & (np.abs(X) <= LEDGE_HALF_W + 22) & (Z >= z_front + 6) & (Z <= z_back)
Y[side] = np.maximum(Y[side], LEDGE_Y + 34 + 26 * smooth(Z[side], z_front + 6, z_back) + 24 * ridged[side] - 8)
wall_top = float(Y[(np.abs(X) <= LEDGE_HALF_W) & (Z > z_back) & (Z <= z_back + 4)].min())

# ---- base colour: rock by height, scree on gentle slopes, snow high up and on flatter ground
gy, gx = np.gradient(Y, axis, axis)
slope = np.hypot(gx, gy)
rock_low = np.array([0.022, 0.042, 0.036])
rock_high = np.array([0.070, 0.086, 0.088])
scree = np.array([0.045, 0.058, 0.054])
snow = np.array([0.330, 0.400, 0.420])
t = smooth(Y, BASE_Y + 20, 150)[..., None]
col = rock_low * (1 - t) + rock_high * t
col *= (0.72 + 0.56 * detail)[..., None]
col = col * (1 - 0.5 * smooth(slope, 0.2, 0.9)[..., None]) + scree * 0.5 * smooth(slope, 0.2, 0.9)[..., None]
snow_line = 108 + 46 * soft
s = smooth(Y, snow_line, snow_line + 30) * (1 - 0.8 * smooth(slope, 1.5, 3.2))
col = col * (1 - s[..., None]) + snow * s[..., None]
dark = inside | ((np.abs(X) <= LEDGE_HALF_W + 4) & (Z > z_back) & (Z <= z_back + 6))
col[dark] *= 0.55                                  # the cave is darker than the open rock
TEX = 2048
img = np.clip(col, 0, 1) ** (1 / 2.2)
tex = bpy.data.images.new("MountainBaseColor", N, N, alpha=False)
rgba = np.concatenate([img, np.ones((N, N, 1))], 2)[::-1]   # Blender images are bottom-up
tex.pixels = rgba.astype(np.float32).ravel()
tex.scale(TEX, TEX)
tex.file_format = 'PNG'
tex.save(filepath=str(OUT / "MountainBaseColor.png"))
tex.filepath = str(OUT / "MountainBaseColor.png")

# ---- mesh. Blender is Z up; engine (x, y, z) = Blender (x, -y, z) with y<->z, i.e. blender = (x, -z_engine, y_engine).
verts = np.stack([X.ravel(), -Z.ravel(), Y.ravel()], 1)
idx = np.arange(N * N).reshape(N, N)
a, b, c, d = idx[:-1, :-1].ravel(), idx[:-1, 1:].ravel(), idx[1:, 1:].ravel(), idx[1:, :-1].ravel()
faces = np.stack([a, b, c, d], 1)
mesh = bpy.data.meshes.new("Mountain")
mesh.from_pydata(verts.tolist(), [], faces.tolist())
uv = mesh.uv_layers.new(name="UVMap")
u = (X.ravel() / SIZE + 0.5)
v = 1 - (Z.ravel() / SIZE + 0.5)                   # image row 0 is +... see the flip above
loops = np.array([l.vertex_index for l in mesh.loops])
uv.data.foreach_set("uv", np.stack([u[loops], v[loops]], 1).ravel())
mesh.update()
# face the outside up (Blender +Z)
import bmesh
bm = bmesh.new(); bm.from_mesh(mesh)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
if sum(f.normal.z for f in bm.faces) < 0:
    bmesh.ops.reverse_faces(bm, faces=bm.faces)
bm.to_mesh(mesh); bm.free()
for p in mesh.polygons:
    p.use_smooth = True

mat = bpy.data.materials.new("MountainRock")
mat.use_nodes = True
bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
node = mat.node_tree.nodes.new("ShaderNodeTexImage")
node.image = tex
mat.node_tree.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 0.95
mesh.materials.append(mat)

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)
obj = bpy.data.objects.new("Doorways_Mountain", mesh)
bpy.context.scene.collection.objects.link(obj)
bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.gltf(filepath=str(OUT / "Mountain.glb"), export_format='GLB', use_selection=True,
                          export_apply=True, export_yup=True, export_image_format='JPEG')

info = {"axes": "engine: metres, Y up, ledge faces -Z, origin = peak centre at play-floor height",
        "size": SIZE, "peakY": float(Y.max()), "baseY": BASE_Y,
        "ledgeY": LEDGE_Y, "ledgeHalfWidth": LEDGE_HALF_W,
        "ledgeFrontZ": float(z_front), "ledgeBackZ": float(z_back), "wallTopY": wall_top,
        "vertices": int(N * N), "triangles": int(2 * (N - 1) * (N - 1))}
(OUT / "Mountain.json").write_text(json.dumps(info, indent=2))
print("MOUNTAIN", json.dumps(info))
