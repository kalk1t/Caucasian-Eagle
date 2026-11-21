"""Six views of a model (from -Y, +X, +Z, +Y, -X, a three-quarter), axes marked, for deciding how a piece is turned.

    blender --factory-startup -b --python preview_views.py -- <model.glb> <out.png>
"""
import bpy, sys, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
lo = Vector((1e9, 1e9, 1e9)); hi = -lo
for o in meshes:
    for v in o.data.vertices:
        w = o.matrix_world @ v.co
        lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
centre = (lo + hi) / 2
size = max(hi - lo)
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = 420
world = bpy.data.worlds.new("w"); scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.5, 0.52, 0.56, 1)
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN")); scene.collection.objects.link(sun)
sun.data.energy = 3.0; sun.rotation_euler = (math.radians(40), math.radians(10), math.radians(25))
# axis marks: red +X, green +Y, blue +Z from the centre
for i, col in enumerate(((1, 0, 0, 1), (0, 1, 0, 1), (0, 0, 1, 1))):
    m = bpy.data.materials.new("ax%d" % i); m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]; bsdf.inputs["Base Color"].default_value = col
    bsdf.inputs["Emission Color"].default_value = col; bsdf.inputs["Emission Strength"].default_value = 2
    bpy.ops.mesh.primitive_cylinder_add(radius=size * 0.01, depth=size * 0.6)
    c = bpy.context.object; c.data.materials.append(m)
    d = Vector([1 if k == i else 0 for k in range(3)])
    c.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
    c.location = centre + d * size * 0.3
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); scene.collection.objects.link(cam)
cam.data.type = "ORTHO"; cam.data.ortho_scale = size * 1.25
scene.camera = cam
files = []
for k, d in enumerate(((0, 1, 0), (-1, 0, 0), (0, 0, -1), (0, -1, 0), (1, 0, 0), (-0.6, 0.6, -0.5))):
    dv = Vector(d).normalized()
    cam.location = centre - dv * size * 3
    up = "Y" if abs(dv.z) > 0.9 else "Y"
    cam.rotation_euler = dv.to_track_quat("-Z", "Y" if abs(dv.z) > 0.9 else "Y").to_euler() if abs(dv.z) > 0.9 else dv.to_track_quat("-Z", "Z").to_euler()
    f = out.replace(".png", "_v%d.png" % k)
    scene.render.filepath = f
    bpy.ops.render.render(write_still=True)
    files.append(f)
print("VIEWS", ";".join(files))
