"""The Eagle Nest (2026-11-20): the Eagle's Ascent's save places (owner, 2026-11-19: "place something like Eagle Nest
that will be save spots for nyxara, put 9 save places on the path"). A great eagle's eyrie of sticks, 3.4 m across and
0.9 m high, built here from nothing (CC0 by construction):

  - the rim: some 300 sticks laid round in a thick ring, each bent along it, weaving up and down, thick at one end;
  - long sticks thrust out of its outside and lying over its top, as an eyrie is never tidy;
  - the bowl's floor: a matted bed of dry grass and down, with short sticks lying across it;
  - six white feathers with dark tips, lying in it and caught in the rim.

Every stick is a closed five-sided tube, pointed at both ends, shaded smooth, its bark from one of the colour map's 16
strips (make_nest_texture.py). One mesh, one material, the node named Doorways_EagleNest (the engine's importer takes
only nodes so named). Blender's Z up is the glTF's Y up; the nest stands on its lowest point at the origin.

    blender -b --factory-startup -P art/map/eagle_nest/build_eagle_nest.py -- art/map/eagle_nest/export
writes export/EagleNest.glb and export/EagleNest.blend (export/EagleNestBaseColor.png must be there: the texture script).
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = os.path.abspath(argv[0] if argv else "export")
rng = random.Random(20261120)

STRIPS = 16          # bark strips across the colour map (u)
BARK_V = .75         # the bark is the top three quarters of the map (glTF v)
TEXEL = .47          # map units per metre along a stick (v) and round it (u)
SIDES = 5

RIM_R, RIM_H = 1.30, .40      # the middle of the rim's cross-section: m from the nest's middle, m up
RIM_AR, RIM_AH = .38, .40     # its half width and half height
FLOOR_R = 1.16                # the matted floor's radius

for ob in list(bpy.data.objects):   # (a fresh factory scene: its cube, camera and light)
    bpy.data.objects.remove(ob, do_unlink=True)

bm = bmesh.new()
uv_layer = bm.loops.layers.uv.new("UVMap")
stats = {"sticks": 0, "feathers": 0}


def set_uv(face, uvs):
    for loop, (u, v) in zip(face.loops, uvs):
        loop[uv_layer].uv = (u, 1.0 - v)   # Blender's v runs up, glTF's down: the exporter turns it


def stick(points, r0, r1=None, strip=None):
    """A stick along points: radius r0 at its first point, r1 at its last; a closed, pointed tube."""
    if r1 is None:
        r1 = r0 * .5
    if strip is None:
        strip = rng.randrange(STRIPS)
    n = len(points)
    lengths = [0.0]
    for i in range(1, n):
        lengths.append(lengths[-1] + (points[i] - points[i - 1]).length)
    total = max(lengths[-1], 1e-4)
    # frames along it, carried from one point to the next without twisting
    t_prev = (points[1] - points[0]).normalized()
    up = Vector((0, 0, 1)) if abs(t_prev.z) < .9 else Vector((1, 0, 0))
    a = t_prev.cross(up).normalized()
    frames = []
    for i in range(n):
        if i == 0:
            t = t_prev
        elif i == n - 1:
            t = (points[i] - points[i - 1]).normalized()
        else:
            t = (points[i + 1] - points[i - 1]).normalized()
        axis = t_prev.cross(t)
        if axis.length > 1e-6:
            a = Matrix.Rotation(t_prev.angle(t), 3, axis.normalized()) @ a
        a = (a - t * a.dot(t)).normalized()
        frames.append((t, a, t.cross(a)))
        t_prev = t
    # its bark: a piece of one strip, as long as it is (v) and as round (u)
    strip_w = 1.0 / STRIPS
    du = min(2 * math.pi * max(r0, r1) * TEXEL * 1.6, strip_w * .9)
    u0 = strip * strip_w + rng.random() * (strip_w - du)
    span = min(total * TEXEL, BARK_V * .98)
    v0 = rng.random() * (BARK_V - span)
    rings = []
    for i in range(n):
        t, fa, fb = frames[i]
        r = r0 + (r1 - r0) * lengths[i] / total
        turn = rng.random() * 2 * math.pi if i == 0 else 0
        ring = []
        for k in range(SIDES):
            ang = 2 * math.pi * k / SIDES + turn
            ring.append(bm.verts.new(points[i] + (fa * math.cos(ang) + fb * math.sin(ang)) * r * (1 + (rng.random() - .5) * .12)))
        rings.append(ring)
    for i in range(n - 1):
        va, vb = v0 + span * lengths[i] / total, v0 + span * lengths[i + 1] / total
        for k in range(SIDES):
            k2 = (k + 1) % SIDES
            f = bm.faces.new((rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]))
            f.smooth = True
            ua, ub = u0 + du * k / SIDES, u0 + du * (k + 1) / SIDES
            set_uv(f, ((ua, va), (ub, va), (ub, vb), (ua, vb)))
    for i, sign in ((0, -1.0), (n - 1, 1.0)):   # broken ends: a short point
        t = frames[i][0]
        r = r0 if i == 0 else r1
        tip = bm.verts.new(points[i] + t * sign * r * (1.2 + rng.random()))
        v = v0 + (0 if i == 0 else span)
        for k in range(SIDES):
            k2 = (k + 1) % SIDES
            f = bm.faces.new((rings[i][k], rings[i][k2], tip))
            f.smooth = True
            set_uv(f, ((u0 + du * k / SIDES, v), (u0 + du * (k + 1) / SIDES, v), (u0 + du * (k + .5) / SIDES, v)))
    stats["sticks"] += 1


def ring_point(r, h, theta):
    return Vector((r * math.cos(theta), r * math.sin(theta), h))


def rim_stick(r, h, length, thick, bulge=.07, tilt=.12):
    """A stick laid round the ring at (r, h), bending along it, weaving up and down."""
    theta0 = rng.random() * 2 * math.pi
    sweep = length / r * (1 if rng.random() < .5 else -1)
    count = max(4, int(length / .22) + 1)
    phase = rng.random() * math.pi
    lean = (rng.random() - .5) * 2 * tilt
    pts = []
    for j in range(count):
        s = j / (count - 1)
        rr = r + bulge * math.sin(math.pi * s + phase) * (rng.random() * .4 + .8)
        hh = h + lean * (s - .5) + .035 * math.sin(2 * math.pi * s * 1.5 + phase) + (rng.random() - .5) * .02
        hh = max(hh, .03)
        pts.append(ring_point(rr, hh, theta0 + sweep * s))
    stick(pts, thick)


# ---- the rim: a thick ring, wider at its foot, its sticks thicker low down
for _ in range(300):
    while True:
        x, y = rng.random() * 2 - 1, rng.random() * 2 - 1
        if x * x + y * y <= 1:
            break
    h = RIM_H + RIM_AH * y
    r = RIM_R + RIM_AR * x + .14 * (1 - h / (RIM_H + RIM_AH))      # its foot spreads out
    low = 1 - h / (RIM_H + RIM_AH)
    rim_stick(r, h, .6 + rng.random() * .9, .02 + .022 * rng.random() + .012 * low)

# its top: a closer round of sticks, so the rim reads as a rim
for _ in range(70):
    rim_stick(RIM_R + (rng.random() - .5) * .5, RIM_H + RIM_AH * (.8 + .25 * rng.random()), .7 + rng.random() * .8, .018 + .016 * rng.random(), bulge=.05, tilt=.06)

# ---- long sticks out of its outside, and over its top
for _ in range(34):
    theta = rng.random() * 2 * math.pi
    h0 = .2 + rng.random() * .5
    r0 = RIM_R + RIM_AR * .7 + .1 * (1 - h0)
    out = Vector((math.cos(theta), math.sin(theta), 0))
    along = Vector((-math.sin(theta), math.cos(theta), 0)) * (1 if rng.random() < .5 else -1)
    angle = math.radians(8 + rng.random() * 16)
    d = (along * math.cos(angle) + out * math.sin(angle)).normalized()
    length = .8 + rng.random() * .7
    start = ring_point(r0, h0, theta) - d * length * (.4 + .2 * rng.random())
    pts = []
    for j in range(5):
        s = j / 4
        p = start + d * length * s
        p.z = max(.04, h0 - .45 * max(0.0, (p.xy.length - r0)) + .04 * math.sin(math.pi * s) + (rng.random() - .5) * .03)
        pts.append(p)
    stick(pts, .034 + .022 * rng.random(), .02 + .01 * rng.random())

for _ in range(16):   # over the top: from the bowl's inside, across the rim, down the outside
    theta = rng.random() * 2 * math.pi
    wob = (rng.random() - .5) * .5
    pts = []
    for j in range(6):
        s = j / 5
        r = .85 + 1.05 * s
        h = .25 + .62 * math.sin(math.pi * min(1.0, s * 1.15)) - .12 * s
        pts.append(ring_point(r, max(.04, h), theta + wob * s))
    stick(pts, .02 + .015 * rng.random())

# ---- the floor: a matted bed, rising to the rim, with short sticks across it
floor_faces = []
rings = [(0.0, .10)] + [(FLOOR_R * f, .10 + .09 * f * f) for f in (.25, .5, .75, 1.0)]
segments = 28
center = bm.verts.new((0, 0, rings[0][1]))
verts = [[center]]
for r, h in rings[1:]:
    row = []
    for k in range(segments):
        a = 2 * math.pi * k / segments
        row.append(bm.verts.new((r * math.cos(a), r * math.sin(a), h + (rng.random() - .5) * .025)))
    verts.append(row)


def floor_uv(v):
    return (.375 + .25 * v.co.x / (2 * FLOOR_R), .875 + .25 * v.co.y / (2 * FLOOR_R))   # a 256 px square of the floor picture


for k in range(segments):
    k2 = (k + 1) % segments
    f = bm.faces.new((center, verts[1][k], verts[1][k2]))
    f.smooth = True
    set_uv(f, [floor_uv(v) for v in (center, verts[1][k], verts[1][k2])])
    floor_faces.append(f)
for i in range(1, len(verts) - 1):
    for k in range(segments):
        k2 = (k + 1) % segments
        f = bm.faces.new((verts[i][k], verts[i + 1][k], verts[i + 1][k2], verts[i][k2]))
        f.smooth = True
        set_uv(f, [floor_uv(v) for v in (verts[i][k], verts[i + 1][k], verts[i + 1][k2], verts[i][k2])])
        floor_faces.append(f)
# its underside, so the floor is closed (it is never seen)
under = [bm.verts.new((v.co.x, v.co.y, .02)) for v in verts[-1]]
under_center = bm.verts.new((0, 0, .02))
for k in range(segments):
    k2 = (k + 1) % segments
    f = bm.faces.new((under_center, under[k2], under[k]))
    set_uv(f, [floor_uv(v) for v in (under_center, under[k2], under[k])])
    f = bm.faces.new((verts[-1][k], under[k], under[k2], verts[-1][k2]))
    set_uv(f, [floor_uv(v) for v in (verts[-1][k], under[k], under[k2], verts[-1][k2])])

for _ in range(64):
    a = rng.random() * 2 * math.pi
    rr = FLOOR_R * .85 * math.sqrt(rng.random())
    mid = Vector((rr * math.cos(a), rr * math.sin(a), 0))
    d = Vector((math.cos(rng.random() * math.pi), math.sin(rng.random() * math.pi), 0)).normalized()
    length = .3 + rng.random() * .55
    base = .13 + .09 * (mid.length / FLOOR_R) ** 2
    pts = [mid + d * length * (s - .5) + Vector((0, 0, base + .025 * math.sin(math.pi * s) + (rng.random() - .5) * .015)) for s in (0, .33, .66, 1)]
    stick(pts, .011 + .012 * rng.random())


# ---- feathers: white, dark tipped, a little curved; three lying in the bowl, three caught in the rim
def feather(base, direction, length, width, lift):
    d = direction.normalized()
    side = d.cross(Vector((0, 0, 1)))
    if side.length < 1e-3:
        side = Vector((1, 0, 0))
    side.normalize()
    rows = 6
    cols = []
    for j in range(rows):
        s = j / (rows - 1)
        shape = math.sin(math.pi * min(1.0, s * 1.1)) ** .6 * (1 - .65 * s) + .08   # narrow at the quill, a round tip
        w = width * shape
        centre = base + d * length * s + Vector((0, 0, lift * math.sin(math.pi * s * .8)))
        cols.append((bm.verts.new(centre - side * w), bm.verts.new(centre + Vector((0, 0, .004))), bm.verts.new(centre + side * w)))
    for j in range(rows - 1):
        va, vb = 1.0 - .25 * j / (rows - 1), 1.0 - .25 * (j + 1) / (rows - 1)
        for c in range(2):
            f = bm.faces.new((cols[j][c], cols[j][c + 1], cols[j + 1][c + 1], cols[j + 1][c]))
            f.smooth = True
            ua, ub = .75 + .125 * c, .75 + .125 * (c + 1)
            set_uv(f, ((ua, va), (ub, va), (ub, vb), (ua, vb)))
    stats["feathers"] += 1


for k in range(3):
    a = rng.random() * 2 * math.pi
    rr = .3 + .6 * rng.random()
    p = Vector((rr * math.cos(a), rr * math.sin(a), .16 + .1 * (rr / FLOOR_R) ** 2))
    feather(p, Vector((math.cos(a + 2.0 * k), math.sin(a + 2.0 * k), 0)), .42 + .14 * rng.random(), .055, .03)
for k in range(3):
    a = 2 * math.pi * (k + rng.random() * .5) / 3
    p = ring_point(RIM_R - .1, RIM_H + RIM_AH * .9, a)
    d = Vector((math.cos(a) * .6, math.sin(a) * .6, .55)) + Vector((-math.sin(a), math.cos(a), 0)) * (rng.random() - .5)
    feather(p, d, .38 + .16 * rng.random(), .05, .02)

bmesh.ops.recalc_face_normals(bm, faces=[f for f in bm.faces])
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)

mesh = bpy.data.meshes.new("EagleNest")
bm.to_mesh(mesh)
bm.free()
ob = bpy.data.objects.new("Doorways_EagleNest", mesh)
bpy.context.scene.collection.objects.link(ob)
# it stands on its lowest point
low = min(v.co.z for v in mesh.vertices)
mesh.transform(Matrix.Translation((0, 0, -low)))

material = bpy.data.materials.new("EagleNest")
nodes = material.node_tree.nodes
bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
image_path = os.path.join(OUT, "EagleNestBaseColor.png")
if os.path.exists(image_path):
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(image_path)
    material.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = .9
mesh.materials.append(material)

os.makedirs(OUT, exist_ok=True)
glb = os.path.join(OUT, "EagleNest.glb")
result = bpy.ops.export_scene.gltf(filepath=glb, export_format="GLB", use_selection=False, export_apply=True, export_yup=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "EagleNest.blend"))
dims = ob.dimensions
print("@@NEST@@", {"result": list(result), "sticks": stats["sticks"], "feathers": stats["feathers"], "triangles": sum(len(p.vertices) - 2 for p in mesh.polygons),
                   "vertices": len(mesh.vertices), "size_m": [round(dims.x, 3), round(dims.y, 3), round(dims.z, 3)], "glb": glb, "glb_bytes": os.path.getsize(glb)})
