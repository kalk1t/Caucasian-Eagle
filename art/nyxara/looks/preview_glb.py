"""A quick look at a model: its size, faces and textures, and a picture from two sides.

    blender --factory-startup -b --python preview_glb.py -- <model.glb> <out.png>
"""
import bpy, sys, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
lo = Vector((1e9, 1e9, 1e9)); hi = -lo
faces = 0
for o in meshes:
    faces += len(o.data.polygons)
    for v in o.data.vertices:
        w = o.matrix_world @ v.co
        lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
print("MODEL", src, "objects", len(meshes), "faces", faces, "size", tuple(round(x, 3) for x in (hi - lo)))
for m in bpy.data.materials:
    if m.node_tree:
        print("  MATERIAL", m.name, [(n.image.name, tuple(n.image.size)) for n in m.node_tree.nodes if n.type == "TEX_IMAGE" and n.image])
centre = (lo + hi) / 2
size = max(hi - lo)
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = 1400, 700
if max(range(3), key=lambda i: hi[i] - lo[i]) == 2:
    scene.render.resolution_x, scene.render.resolution_y = 500, 1400   # standing up: a tall picture
world = bpy.data.worlds.new("w"); scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.57, 0.6, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN")); scene.collection.objects.link(sun)
sun.data.energy = 3.5; sun.rotation_euler = (math.radians(50), 0, math.radians(30))
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); scene.collection.objects.link(cam)
cam.data.type = "ORTHO"; cam.data.ortho_scale = size * 1.08
scene.camera = cam
axis = max(range(3), key=lambda i: hi[i] - lo[i])
views = []
for k, d in enumerate(((0, -1, 0), (1, 0, 0)) if axis != 1 else ((0, 0, 1), (1, 0, 0))):
    dv = Vector(d)
    cam.location = centre - dv * size * 3
    cam.rotation_euler = dv.to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = out.replace(".png", "_%d.png" % k)
    bpy.ops.render.render(write_still=True)
    views.append(scene.render.filepath)
print("VIEWS", views)
