"""Reusable low-poly nature and grave props for the Moonlit Sanctuary.

Call create_tree(location, rotation, scale, materials, seed) or
create_grave(location, rotation, scale, materials). Rotation is a Z-axis
angle in radians, scale is a scalar or a three-component tuple, and materials
is a dictionary containing Blender materials under 'bark' and 'stone'.
Both return one mesh object and use metres in a Z-up scene.
"""
import bpy
import math
import random
from mathutils import Vector


class _MeshParts:
    def __init__(self):
        self.vertices = []
        self.faces = []

    def branch(self, start, end, radius_start, radius_end, sides=7):
        start, end = Vector(start), Vector(end)
        axis = end - start
        if axis.length < 0.0001:
            return
        axis.normalize()
        tangent = axis.cross(Vector((0, 0, 1)))
        if tangent.length < 0.001:
            tangent = axis.cross(Vector((0, 1, 0)))
        tangent.normalize()
        bitangent = axis.cross(tangent).normalized()
        base = len(self.vertices)
        for center, radius in ((start, radius_start), (end, radius_end)):
            for i in range(sides):
                angle = 2 * math.pi * i / sides
                p = center + radius * (math.cos(angle) * tangent + math.sin(angle) * bitangent)
                self.vertices.append(tuple(p))
        self.faces.append(tuple(base + i for i in reversed(range(sides))))
        self.faces.append(tuple(base + sides + i for i in range(sides)))
        for i in range(sides):
            j = (i + 1) % sides
            self.faces.append((base+i, base+j, base+sides+j, base+sides+i))

    def box(self, lo, hi):
        x0,y0,z0 = lo
        x1,y1,z1 = hi
        base = len(self.vertices)
        self.vertices.extend([(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),
                              (x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)])
        self.faces.extend([tuple(base+i for i in f) for f in
            ((0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7))])

    def extruded_outline(self, outline, depth):
        base = len(self.vertices)
        n = len(outline)
        self.vertices.extend([(x,-depth/2,z) for x,z in outline])
        self.vertices.extend([(x, depth/2,z) for x,z in outline])
        self.faces.append(tuple(base+i for i in range(n-1,-1,-1)))
        self.faces.append(tuple(base+n+i for i in range(n)))
        for i in range(n):
            j = (i+1) % n
            self.faces.append((base+i,base+j,base+n+j,base+n+i))

    def finish(self, name, location, rotation, scale, material):
        mesh = bpy.data.meshes.new(name+'Mesh')
        mesh.from_pydata(self.vertices, [], self.faces)
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
        obj.location = location
        obj.rotation_euler[2] = rotation
        obj.scale = (scale, scale, scale) if isinstance(scale, (int,float)) else scale
        if material is not None:
            mesh.materials.append(material)
        return obj


def create_tree(location, rotation, scale, materials, seed):
    """A leafless, twisted tree approximately 7 m tall at scale 1."""
    rng = random.Random(seed)
    parts = _MeshParts()
    trunk = [(0,0,0),(.05,-.03,1.25),(-.12,.05,2.55),(.08,.10,3.7),(-.08,.10,4.8),(.10,.17,5.9)]
    widths = [.39,.31,.24,.17,.10,.025]
    for i in range(len(trunk)-1):
        parts.branch(trunk[i], trunk[i+1], widths[i], widths[i+1])
    # Knotted surface roots extend only a short distance onto the ground.
    for i in range(5):
        angle = i*math.tau/5 + rng.uniform(-.2,.2)
        parts.branch((0,0,.24),(math.cos(angle)*.85,math.sin(angle)*.85,.04),.20,.025,6)
    for level in range(7):
        angle = level*2.39996 + rng.uniform(-.35,.35)
        z = 1.7 + level*.48 + rng.uniform(-.12,.12)
        origin = Vector((rng.uniform(-.10,.10),rng.uniform(-.10,.10),z))
        span = rng.uniform(1.25,2.0) * (1.0-level*.035)
        direction = Vector((math.cos(angle),math.sin(angle),0))
        bend = origin + direction*span*.55 + Vector((0,0,.65))
        tip = origin + direction*span + Vector((0,0,rng.uniform(1.15,1.7)))
        thickness = .15-level*.012
        parts.branch(origin,bend,thickness,thickness*.56,6)
        parts.branch(bend,tip,thickness*.56,.012,6)
        for s in (-1,1):
            fork_start = bend.lerp(tip,.4)
            sideways = Vector((-direction.y,direction.x,0))*s
            fork_tip = fork_start + direction*rng.uniform(.35,.6) + sideways*rng.uniform(.3,.6) + Vector((0,0,rng.uniform(.55,.95)))
            parts.branch(fork_start,fork_tip,thickness*.28,.008,5)
    return parts.finish('Nature_DeadTree',location,rotation,scale,materials.get('bark'))


def create_grave(location, rotation, scale, materials):
    """A rounded headstone on a plinth, about 1.5 m high at scale 1."""
    parts = _MeshParts()
    parts.box((-.58,-.26,0),(.58,.26,.14))
    parts.box((-.49,-.19,.14),(.49,.19,.24))
    outline = [(-.42,.24),(.42,.24),(.42,1.03),(.38,1.20),(.27,1.35),
               (0,1.43),(-.27,1.35),(-.38,1.20),(-.42,1.03)]
    parts.extruded_outline(outline,.22)
    # Shallow relief; visually simple enough to reuse throughout the cemetery.
    parts.box((-.035,-.126,.57),(.035,-.108,1.12))
    parts.box((-.18,-.126,.86),(.18,-.108,.93))
    return parts.finish('Nature_Grave',location,rotation,scale,materials.get('stone'))
