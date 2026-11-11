"""Reusable Gothic architecture for the Moonlit Sanctuary concept scene.

Coordinates are metres with Z up. All rotations are radians about Z. The front
of each object faces local -Y. ``materials`` accepts Blender material objects
under the keys ``stone``, ``trim``, ``dark``, ``metal`` and ``glow``. Missing
keys receive small neutral fallback materials. These are visual meshes only;
none is a gameplay collision proxy or an installed game asset.
"""
import bpy
import math
from mathutils import Matrix, Vector


def _material(materials, key):
    if materials and materials.get(key):
        return materials[key]
    name = 'MS_Architecture_' + key
    old = bpy.data.materials.get(name)
    if old:
        return old
    color = {
        'stone': (.16, .18, .22, 1),
        'trim': (.31, .32, .34, 1),
        'dark': (.035, .045, .06, 1),
        'metal': (.09, .11, .14, 1),
        'glow': (.55, .73, .95, 1),
    }.get(key, (.25, .25, .25, 1))
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    return mat


def _root(location, rotation, scale):
    if scale <= 0:
        raise ValueError('scale must be positive')
    return (Matrix.Translation(Vector(location)) @
            Matrix.Rotation(rotation, 4, 'Z') @ Matrix.Scale(scale, 4))


def _mesh(name, verts, faces, root, material, collection=None):
    mesh = bpy.data.meshes.new(name + '_mesh')
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    (collection or bpy.context.collection).objects.link(obj)
    obj.matrix_world = root
    if material:
        mesh.materials.append(material)
    return obj


_CUBE_V = [(-.5,-.5,-.5),(.5,-.5,-.5),(.5,.5,-.5),(-.5,.5,-.5),
           (-.5,-.5,.5),(.5,-.5,.5),(.5,.5,.5),(-.5,.5,.5)]
_CUBE_F = [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),
           (2,3,7,6),(3,0,4,7)]


def _box(name, center, dimensions, root, material, z_rotation=0):
    # Keep the module usable when parent scripts transform whole structures.
    matrix = (root @ Matrix.Translation(Vector(center)) @
              Matrix.Rotation(z_rotation, 4, 'Z') @
              Matrix.Diagonal((*dimensions, 1.0)))
    return _mesh('MS_' + name, _CUBE_V, _CUBE_F, matrix, material)


def _cylinder(name, center, radius, depth, root, material, count=12,
              top_radius=None):
    if top_radius is None:
        top_radius = radius
    verts = []
    for z, r in ((-.5, 1.0),(.5, top_radius / radius)):
        for i in range(count):
            a = 2.0 * math.pi * i / count
            verts.append((math.cos(a)*r, math.sin(a)*r, z))
    faces = [tuple(reversed(range(count))), tuple(range(count, count*2))]
    for i in range(count):
        j = (i+1) % count
        faces.append((i,j,j+count,i+count))
    matrix = root @ Matrix.Translation(Vector(center)) @ Matrix.Diagonal((radius,radius,depth,1))
    return _mesh('MS_' + name, verts, faces, matrix, material)


def _beam(name, a, b, thickness, depth, root, material):
    a, b = Vector(a), Vector(b)
    direction = b - a
    matrix = (root @ Matrix.Translation((a+b)*.5) @
              direction.to_track_quat('Z','Y').to_matrix().to_4x4() @
              Matrix.Diagonal((thickness,depth,direction.length,1)))
    return _mesh('MS_' + name, _CUBE_V, _CUBE_F, matrix, material)


def _pointed_arch(root, center_x, y, base_z, width, spring_height,
                  arch_height, thickness, depth, stone, trim):
    """An open pointed arch in local X/Z; the negative Y side is its front."""
    left, right = center_x-width*.5, center_x+width*.5
    _box('Arch_left_pier', (left,y,base_z+spring_height*.5),
         (thickness,depth,spring_height),root,stone)
    _box('Arch_right_pier', (right,y,base_z+spring_height*.5),
         (thickness,depth,spring_height),root,stone)
    for side in (-1,1):
        x = center_x + side*width*.5
        for z in (base_z+.15,base_z+spring_height-.1):
            _box('Arch_pier_trim',(x,y,z),(thickness*1.55,depth*1.2,.25),root,trim)
        pts=[]
        # Parabolic curves meet at a sharp Gothic apex.
        for i in range(9):
            t = i/8
            pts.append((center_x+side*width*.5*(1-t),y,
                        base_z+spring_height+arch_height*(1-(1-t)**1.55)))
        for a,b in zip(pts,pts[1:]):
            _beam('Arch_rib',a,b,thickness,depth,root,trim)


def create_arch(location, rotation=0.0, scale=1.0, materials=None):
    """Build a freestanding open 5 m Gothic arch; front is local -Y.

    Returns the 4x4 local-to-world placement matrix.
    """
    root=_root(location, rotation, scale)
    stone=_material(materials,'stone'); trim=_material(materials,'trim')
    _pointed_arch(root,0,0,0,4.8,4.2,2.4,.55,.8,stone,trim)
    for x in (-2.4,2.4):
        _box('Arch_plinth',(x,0,.18),(1.25,1.25,.36),root,stone)
        _cylinder('Arch_finial',(x,0,5.0),.33,1.6,root,trim,8,0)
    return root


def create_bench(location, rotation=0.0, scale=1.0, materials=None):
    """Build a simple three metre stone bench, with local front toward -Y."""
    root=_root(location,rotation,scale)
    stone=_material(materials,'stone'); trim=_material(materials,'trim')
    for x in (-1.0,1.0):
        _box('Bench_leg',(x,0,.28),(.42,.72,.56),root,stone)
        _box('Bench_foot',(x,0,.07),(.66,1.0,.14),root,trim)
    _box('Bench_seat',(0,0,.62),(3.0,.9,.20),root,trim)
    _box('Bench_back',(0,.36,1.03),(3.0,.18,.75),root,stone)
    _box('Bench_back_cap',(0,.36,1.45),(3.08,.27,.12),root,trim)
    return root


def create_broken_column(location, rotation=0.0, scale=1.0, materials=None):
    """Build a decorative broken-column silhouette, not a collision proxy."""
    root=_root(location,rotation,scale)
    stone=_material(materials,'stone'); trim=_material(materials,'trim')
    _box('Column_foot',(0,0,.15),(1.65,1.65,.3),root,stone)
    _cylinder('Column_base',(0,0,.37),.68,.22,root,trim)
    _cylinder('Broken_column',(0,0,1.48),.47,2.0,root,stone,10,.42)
    _cylinder('Fracture_cap',(0,0,2.51),.43,.08,root,trim,10,.34)
    _box('Column_rubble',(.83,-.3,.18),(.66,.42,.36),root,stone,z_rotation=.45)
    return root


def create_temple(location, rotation=0.0, scale=1.0, materials=None):
    """Build a 20 m-wide Gothic temple facade with open front arch and towers.

    Front is local -Y. It is a decorative façade with shallow interior; placement
    should leave room for players to walk around it. Returns placement matrix.
    """
    root=_root(location,rotation,scale)
    stone=_material(materials,'stone'); trim=_material(materials,'trim')
    dark=_material(materials,'dark'); metal=_material(materials,'metal')
    # Foundation, three broad shallow stairs and elevated floor.
    _box('Temple_foundation',(0,0,.38),(20,12,.76),root,stone)
    for i in range(5):
        _box('Temple_step',(0,-7.5+i*.5,.09+i*.09),
             (10.2,.8,.18+i*.18),root,trim)
    _box('Temple_inner_floor',(0,0,.84),(17.6,10,.16),root,trim)
    # Rear wall stays solid, front entrance remains visually open.
    _box('Temple_rear_wall',(0,5.35,4.9),(17.8,.8,8.2),root,stone)
    for x in (-8.55,8.55):
        _box('Temple_side_wall',(x,.4,4.9),(.7,10,8.2),root,stone)
    # Three arch openings on the front. Solid side bays frame the main door.
    _pointed_arch(root,0,-5.35,.8,5.2,4.0,2.7,.48,.65,stone,trim)
    for x in (-6.05,6.05):
        _box('Temple_side_bay',(x,-5.0,4.45),(3.35,.55,7.3),root,stone)
        _box('Temple_window_recess',(x,-5.31,4.2),(1.8,.10,4.2),root,dark)
        _pointed_arch(root,x,-5.42,1.6,2.0,3.2,1.25,.19,.26,stone,trim)
        _box('Temple_window_mullion',(x,-5.6,3.3),(.1,.12,3.3),root,metal)
    _box('Temple_lower_cornice',(0,-5.28,7.95),(18.2,.95,.55),root,trim)
    _box('Temple_upper_cornice',(0,-5.28,8.5),(19,1.1,.3),root,stone)
    # Large front gable, modelled as an extruded triangle.
    vs=[(-6.8,-5.2,8.62),(6.8,-5.2,8.62),(0,-5.2,12.15),
        (-6.8,-4.25,8.62),(6.8,-4.25,8.62),(0,-4.25,12.15)]
    fs=[(0,1,2),(5,4,3),(0,3,4,1),(1,4,5,2),(2,5,3,0)]
    _mesh('MS_Temple_gable',vs,fs,root,stone)
    _beam('Gable_rib',(-6.8,-5.35,8.62),(0,-5.35,12.15),.22,.25,root,trim)
    _beam('Gable_rib',(0,-5.35,12.15),(6.8,-5.35,8.62),.22,.25,root,trim)
    _pointed_arch(root,0,-5.42,8.8,1.65,1.0,1.0,.17,.20,stone,trim)
    # Paired corner towers with layered ledges, slit windows and pointed spires.
    for side in (-1,1):
        x=side*9.0
        _box('Temple_tower_base',(x,-4.65,1.15),(3.2,3.2,2.3),root,stone)
        _box('Temple_tower',(x,-4.65,6.15),(2.5,2.6,8.0),root,stone)
        for z in (2.35,6.1,10.1):
            _box('Temple_tower_band',(x,-4.65,z),(3.0,3.0,.27),root,trim)
        _box('Tower_window',(x,-6.02,7.7),(.5,.10,2.0),root,dark)
        _cylinder('Tower_spire',(x,-4.65,11.95),1.8,3.4,root,dark,4,0)
        _cylinder('Tower_tip',(x,-4.65,13.8),.18,.8,root,trim,8,0)
    # Colonnade on inner floor, offering a readable temple silhouette.
    for x in (-4.2,4.2):
        for y in (-3.2,2.7):
            _box('Temple_column_plinth',(x,y,1.0),(1.3,1.3,.35),root,trim)
            _cylinder('Temple_column',(x,y,4.0),.45,5.8,root,stone,10)
            _cylinder('Temple_capital',(x,y,6.95),.7,.3,root,trim,10)
    return root
