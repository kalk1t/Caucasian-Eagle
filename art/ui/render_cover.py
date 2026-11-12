"""Renders the cover picture of the game in 3D with Blender: art/ui/export/CoverRender.png (1920 x 1080).

Nyxara hovers on a cliff's edge with her wings spread and both swords drawn, an old gold chest glowing at
her feet. Behind her the valley falls away to snow ranges, the mountain of Amirani stands under a great
moon, and the Eagle crosses the moon. Everything in the picture is the game's own: her model and pose
(art/nyxara/blender/Nyxara_game.glb, the FlightHover clip), the mountain and the Eagle
(art/mountain/export), the chest (art/monsters/meshy/out/chest_3d), the moon and cloud textures.
Nothing is downloaded.

art/ui/build_start_screen.py then darkens the left side for the menu, letters the title and writes
game/assets/ui/Menu/StartScreen.png.

Usage:  blender -b -P art/ui/render_cover.py            (about a minute)
        python art/ui/build_start_screen.py
"""
import math, os
import bpy
import numpy as np
from mathutils import Vector

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "art", "ui", "export", "CoverRender.png")
rng = np.random.default_rng(1872)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


# ------------------------------------------------------------------ helpers
def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(REPO, path))
    return [o for o in bpy.data.objects if o not in before]


def group(objects, name):
    """Parents the top objects of an import to one empty, so the whole model moves as one."""
    root = bpy.data.objects.new(name, None)
    scene.collection.objects.link(root)
    for o in objects:
        if o.parent is None:
            o.parent = root
    return root


def fbm(n, m, beta, seed):
    r = np.random.default_rng(seed)
    kx = np.fft.fftfreq(n)[:, None]; ky = np.fft.fftfreq(m)[None, :]
    k = np.sqrt(kx * kx + ky * ky); k[0, 0] = 1
    f = (r.standard_normal((n, m)) + 1j * r.standard_normal((n, m))) / k ** beta
    f[0, 0] = 0
    out = np.real(np.fft.ifft2(f))
    out -= out.min(); out /= out.max()
    return out


def grid_mesh(name, xs, ys, z, colors):
    """A height field as a smooth mesh with a colour on every corner."""
    nx, ny = len(xs), len(ys)
    X, Y = np.meshgrid(xs, ys)
    verts = np.column_stack([X.ravel(), Y.ravel(), z.ravel()])
    i = np.arange((ny - 1) * nx).reshape(ny - 1, nx)[:, :-1].ravel()
    faces = np.column_stack([i, i + 1, i + 1 + nx, i + nx])
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts.tolist(), [], faces.tolist())
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    layer = me.color_attributes.new("Col", 'FLOAT_COLOR', 'POINT')
    rgba = np.concatenate([colors.reshape(-1, 3), np.ones((nx * ny, 1))], axis=1)
    layer.data.foreach_set("color", rgba.ravel())
    me.update()
    o = bpy.data.objects.new(name, me)
    scene.collection.objects.link(o)
    return o


def painted(name, rough=.9):
    """A material that shows the mesh's own colours."""
    m = bpy.data.materials.new(name); m.use_nodes = True
    bsdf = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    col = m.node_tree.nodes.new('ShaderNodeVertexColor'); col.layer_name = "Col"
    m.node_tree.links.new(col.outputs['Color'], bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value = rough
    return m


def glow(name, color, strength, image=None, alpha=False):
    """A material that only shines (the moon, clouds): not lit by anything."""
    m = bpy.data.materials.new(name); m.use_nodes = True
    nodes, links = m.node_tree.nodes, m.node_tree.links
    nodes.clear()
    out = nodes.new('ShaderNodeOutputMaterial'); em = nodes.new('ShaderNodeEmission')
    em.inputs['Color'].default_value = (*color, 1); em.inputs['Strength'].default_value = strength
    last = em.outputs['Emission']
    if image:
        tex = nodes.new('ShaderNodeTexImage'); tex.image = bpy.data.images.load(os.path.join(REPO, image))
        mul = nodes.new('ShaderNodeMixRGB'); mul.blend_type = 'MULTIPLY'; mul.inputs['Fac'].default_value = 1
        mul.inputs['Color1'].default_value = (*color, 1)
        links.new(tex.outputs['Color'], mul.inputs['Color2']); links.new(mul.outputs['Color'], em.inputs['Color'])
        if alpha:
            clear = nodes.new('ShaderNodeBsdfTransparent'); mix = nodes.new('ShaderNodeMixShader')
            # soft round edge, so the sheet of cloud has no border
            coord = nodes.new('ShaderNodeTexCoord'); grad = nodes.new('ShaderNodeTexGradient'); grad.gradient_type = 'SPHERICAL'
            mp = nodes.new('ShaderNodeMapping'); mp.inputs['Location'].default_value = (-1, -1, 0); mp.inputs['Scale'].default_value = (2, 2, 1)
            links.new(coord.outputs['UV'], mp.inputs['Vector']); links.new(mp.outputs['Vector'], grad.inputs['Vector'])
            fac = nodes.new('ShaderNodeMath'); fac.operation = 'MULTIPLY'
            links.new(tex.outputs['Alpha'], fac.inputs[0]); links.new(grad.outputs['Fac'], fac.inputs[1])
            links.new(fac.outputs['Value'], mix.inputs['Fac'])
            links.new(clear.outputs['BSDF'], mix.inputs[1]); links.new(em.outputs['Emission'], mix.inputs[2])
            last = mix.outputs['Shader']
            for attr, value in (("surface_render_method", 'BLENDED'), ("blend_method", 'BLEND')):
                try: setattr(m, attr, value)
                except Exception: pass
    links.new(last, out.inputs['Surface'])
    return m


def look_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


# ------------------------------------------------------------------ camera
cam_data = bpy.data.cameras.new("Cover"); cam_data.lens = 40; cam_data.clip_end = 20000
camera = bpy.data.objects.new("Cover", cam_data); scene.collection.objects.link(camera)
camera.location = (0, -6.5, 1.35)
look_at(camera, (0, 0, 2.0))
scene.camera = camera

# ------------------------------------------------------------------ Nyxara
hero = import_glb("art/nyxara/blender/Nyxara_game.glb")
armature = next(o for o in hero if o.type == 'ARMATURE')
action = bpy.data.actions.get("FlightHover") or bpy.data.actions.get("Idle")
if armature.animation_data is None: armature.animation_data_create()
armature.animation_data.action = action
try:
    if action.slots: armature.animation_data.action_slot = action.slots[0]
except Exception: pass
scene.frame_set(12)
for name in ("Slot_sword_left__Moonfeather", "Slot_sword_right__Talonshade"):   # each hand holds one sword, as in the game
    m = bpy.data.materials.get(name)
    if m:
        m.node_tree.nodes.clear()
        out = m.node_tree.nodes.new('ShaderNodeOutputMaterial'); clear = m.node_tree.nodes.new('ShaderNodeBsdfTransparent')
        m.node_tree.links.new(clear.outputs['BSDF'], out.inputs['Surface'])
        for attr, value in (("surface_render_method", 'BLENDED'), ("blend_method", 'BLEND')):
            try: setattr(m, attr, value)
            except Exception: pass
nyxara = group(hero, "Nyxara")
nyxara.location = (1.0, 0, .62)
nyxara.rotation_euler = (0, 0, math.radians(24))        # turned a little toward the open side of the picture

# ------------------------------------------------------------------ the cliff she stands over
xs = np.linspace(-34, 34, 240); ys = np.linspace(-14, 13, 110)
X, Y = np.meshgrid(xs, ys)
rough = fbm(len(ys), len(xs), 1.7, 3)
edge = 8.5 + 2.5 * fbm(len(ys), len(xs), 2.2, 4)[0][None, :]         # the edge wanders
drop = np.clip(Y - edge, 0, None)
z = (rough - .5) * .5 - drop * 4.5 - .012 * (X - 1.2) ** 2 * (np.abs(X - 1.2) > 5)
grass = fbm(len(ys), len(xs), 1.2, 5)
col = np.where((grass > .52)[..., None], np.array([.035, .075, .035]), np.array([.085, .07, .05]))      # grass and earth
col = np.where((drop > .1)[..., None], np.array([.07, .075, .08]), col)                                 # bare rock on the face
col = col * (.7 + .6 * fbm(len(ys), len(xs), .8, 6))[..., None]
cliff = grid_mesh("Cliff", xs, ys, z, col)
cliff.data.materials.append(painted("CliffPaint"))
for k in range(26):                                                    # stones on the ledge
    x, y = rng.uniform(-9, 9), rng.uniform(-4, 7.5)
    if abs(x - 1.25) < 1.3 and abs(y) < 1.3: continue
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=rng.uniform(.12, .5), location=(x, y, rng.uniform(-.05, .1)))
    stone = bpy.context.object
    stone.scale = (rng.uniform(.8, 1.5), rng.uniform(.8, 1.4), rng.uniform(.45, .8)); stone.rotation_euler = tuple(rng.uniform(0, 3, 3))
    for v in stone.data.vertices: v.co *= rng.uniform(.82, 1.12)
    if "Stone" not in bpy.data.materials:
        sm = bpy.data.materials.new("Stone"); sm.use_nodes = True
        b = next(n for n in sm.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        b.inputs['Base Color'].default_value = (.075, .08, .085, 1); b.inputs['Roughness'].default_value = .95
    stone.data.materials.append(bpy.data.materials["Stone"])
    stone.modifiers.new("Round", 'SUBSURF').render_levels = 2
    stone.data.polygons.foreach_set("use_smooth", [True] * len(stone.data.polygons))

chest = group(import_glb("art/monsters/meshy/out/chest_3d/model_url.glb"), "Chest")
chest.location = (2.45, .6, .42); chest.scale = (.55, .55, .55); chest.rotation_euler = (0, 0, math.radians(200))

# ------------------------------------------------------------------ the valley and the snow ranges
xs = np.linspace(-2600, 2600, 420); ys = np.linspace(30, 3400, 300)
X, Y = np.meshgrid(xs, ys)
ridged = 1 - np.abs(fbm(len(ys), len(xs), 2.0, 11) * 2 - 1)
soft = fbm(len(ys), len(xs), 2.4, 12)
away = np.clip(np.abs(X + 80) / 1500, 0, 1)                           # a valley runs from her toward the mountain
z = -210 + 60 * soft + (140 + 520 * away ** 1.3) * ridged ** 2 * (.35 + .65 * np.clip(Y / 1200, 0, 1))
snow = np.clip((z - 60 + 80 * fbm(len(ys), len(xs), 1.0, 13)) / 70, 0, 1)[..., None]
steep = np.clip(np.hypot(*np.gradient(z, ys, xs)) * 1.6, 0, 1)[..., None]
low = np.array([.018, .04, .035]); rock = np.array([.06, .065, .075]); white = np.array([.55, .62, .70])
col = low * (1 - steep) + rock * steep
col = col * (1 - snow * (1 - .6 * steep)) + white * snow * (1 - .6 * steep)
land = grid_mesh("Ranges", xs, ys, z, col)
land.data.materials.append(painted("RangePaint", .85))

mountain = group(import_glb("art/mountain/export/Mountain.glb"), "Mountain")
mountain.location = (-190, 2350, -40); mountain.scale = (3.2, 3.2, 3.6)

# ------------------------------------------------------------------ the moon, the Eagle, clouds, the sky
bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=330, location=(520, 4300, 1110))
moon = bpy.context.object
bpy.ops.object.shade_smooth()
moon.rotation_euler = (math.radians(20), 0, math.radians(200))
moon.data.materials.append(glow("Moon", (.85, .97, 1.0), 2.6, "game/assets/textures/caucasus/Moon.jpg"))

eagle = group(import_glb("art/mountain/export/Eagle.glb"), "Eagle")
eagle.location = (380, 3300, 800); eagle.scale = (150, 150, 150)
eagle.rotation_euler = (math.radians(-12), math.radians(14), math.radians(8))

def cloud_bank(name, seed, strength):
    """A soft bank of cloud: a sheet that shines faintly where a slow noise is high, and fades out at its rim."""
    m = bpy.data.materials.new(name); m.use_nodes = True
    nodes, links = m.node_tree.nodes, m.node_tree.links
    nodes.clear()
    out = nodes.new('ShaderNodeOutputMaterial'); em = nodes.new('ShaderNodeEmission'); clear = nodes.new('ShaderNodeBsdfTransparent'); mix = nodes.new('ShaderNodeMixShader')
    em.inputs['Color'].default_value = (.42, .58, .66, 1); em.inputs['Strength'].default_value = strength
    coord = nodes.new('ShaderNodeTexCoord')
    noise = nodes.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value = 2.2; noise.inputs['Detail'].default_value = 7; noise.inputs['Roughness'].default_value = .55
    mp = nodes.new('ShaderNodeMapping'); mp.inputs['Location'].default_value = (seed * 3.1, seed * 1.7, 0); mp.inputs['Scale'].default_value = (1, 2.6, 1)   # long along the horizon
    links.new(coord.outputs['UV'], mp.inputs['Vector']); links.new(mp.outputs['Vector'], noise.inputs['Vector'])
    ramp = nodes.new('ShaderNodeValToRGB'); ramp.color_ramp.elements[0].position = .47; ramp.color_ramp.elements[1].position = .78
    links.new(noise.outputs['Fac'], ramp.inputs['Fac'])
    grad = nodes.new('ShaderNodeTexGradient'); grad.gradient_type = 'SPHERICAL'
    mp2 = nodes.new('ShaderNodeMapping'); mp2.inputs['Location'].default_value = (-1, -1, 0); mp2.inputs['Scale'].default_value = (2, 2, 1)
    links.new(coord.outputs['UV'], mp2.inputs['Vector']); links.new(mp2.outputs['Vector'], grad.inputs['Vector'])
    fac = nodes.new('ShaderNodeMath'); fac.operation = 'MULTIPLY'
    links.new(ramp.outputs['Color'], fac.inputs[0]); links.new(grad.outputs['Fac'], fac.inputs[1])
    links.new(fac.outputs['Value'], mix.inputs['Fac']); links.new(clear.outputs['BSDF'], mix.inputs[1]); links.new(em.outputs['Emission'], mix.inputs[2])
    links.new(mix.outputs['Shader'], out.inputs['Surface'])
    for attr, value in (("surface_render_method", 'BLENDED'), ("blend_method", 'BLEND')):
        try: setattr(m, attr, value)
        except Exception: pass
    return m

for k, (x, y, zc, size, strength) in enumerate(((-1500, 4600, 620, 3600, .55), (1700, 4700, 760, 3400, .5), (200, 4500, 1750, 3800, .35), (900, 4450, 1250, 2200, .9))):
    bpy.ops.mesh.primitive_plane_add(size=size, location=(x, y, zc))
    sheet = bpy.context.object
    sheet.scale = (1, .42, 1)
    sheet.rotation_euler = (math.radians(90), 0, 0)
    sheet.data.materials.append(cloud_bank("Cloud%d" % k, k + 1, strength))

world = bpy.data.worlds.new("Night"); scene.world = world; world.use_nodes = True
nodes, links = world.node_tree.nodes, world.node_tree.links
nodes.clear()
out = nodes.new('ShaderNodeOutputWorld'); bg = nodes.new('ShaderNodeBackground')
coord = nodes.new('ShaderNodeTexCoord'); split = nodes.new('ShaderNodeSeparateXYZ')
links.new(coord.outputs['Generated'], split.inputs['Vector'])
ramp = nodes.new('ShaderNodeValToRGB')
ramp.color_ramp.elements[0].position = .0; ramp.color_ramp.elements[0].color = (.022, .060, .075, 1)    # haze at the horizon
ramp.color_ramp.elements[1].position = .55; ramp.color_ramp.elements[1].color = (.002, .005, .016, 1)   # deep night overhead
links.new(split.outputs['Z'], ramp.inputs['Fac'])
stars = nodes.new('ShaderNodeTexVoronoi'); stars.inputs['Scale'].default_value = 420
links.new(coord.outputs['Generated'], stars.inputs['Vector'])
spark = nodes.new('ShaderNodeMath'); spark.operation = 'LESS_THAN'; spark.inputs[1].default_value = .030
links.new(stars.outputs['Distance'], spark.inputs[0])
bright = nodes.new('ShaderNodeMath'); bright.operation = 'MULTIPLY'; bright.inputs[1].default_value = 14.0
links.new(spark.outputs['Value'], bright.inputs[0])
add = nodes.new('ShaderNodeMixRGB'); add.blend_type = 'ADD'; add.inputs['Fac'].default_value = 1
links.new(ramp.outputs['Color'], add.inputs['Color1']); links.new(bright.outputs['Value'], add.inputs['Color2'])
links.new(add.outputs['Color'], bg.inputs['Color']); links.new(bg.outputs['Background'], out.inputs['Surface'])

# mist lying in the valley
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 1700, -60))
mist = bpy.context.object; mist.scale = (5200, 3300, 420)
mm = bpy.data.materials.new("Mist"); mm.use_nodes = True
mm.node_tree.nodes.clear()
mo = mm.node_tree.nodes.new('ShaderNodeOutputMaterial'); vol = mm.node_tree.nodes.new('ShaderNodeVolumePrincipled')
vol.inputs['Color'].default_value = (.55, .80, .85, 1); vol.inputs['Density'].default_value = .0008
mm.node_tree.links.new(vol.outputs['Volume'], mo.inputs['Volume'])
mist.data.materials.append(mm)

# ------------------------------------------------------------------ light
def light(kind, name, location, energy, color, target=None, size=None):
    data = bpy.data.lights.new(name, kind); data.energy = energy; data.color = color
    if size and kind == 'AREA': data.size = size
    if size and kind == 'POINT': data.shadow_soft_size = size
    o = bpy.data.objects.new(name, data); scene.collection.objects.link(o); o.location = location
    if target: look_at(o, target)
    return o

light('SUN', "Moonlight", (520, 4300, 1110), 3.2, (.62, .82, 1.0), target=(0, 0, 1.5))          # from the moon: the rim on her wings
light('SUN', "Skylight", (-900, -1500, 1600), .9, (.42, .58, .80), target=(0, 1500, 0))                 # what the night sky gives the land: the ranges and the mountain keep their shape
light('AREA', "Front", (-4.0, -5.0, 4.0), 700, (.70, .88, 1.0), target=(1.25, 0, 1.7), size=5)   # so her face and armour read
light('AREA', "Side", (6.5, -2.0, 2.5), 260, (.35, .95, .80), target=(1.25, 0, 1.6), size=3)     # the green of her feathers
light('POINT', "ChestGlow", (2.6, -.25, 1.0), 220, (1.0, .62, .22), size=.4)                     # gold from the chest

# ------------------------------------------------------------------ render
scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = OUT
for engine in ('BLENDER_EEVEE_NEXT', 'BLENDER_EEVEE'):
    try:
        scene.render.engine = engine
        break
    except Exception: pass
try:
    scene.eevee.taa_render_samples = 96
    scene.eevee.use_raytracing = True
    scene.eevee.volumetric_end = 6000; scene.eevee.volumetric_samples = 128; scene.eevee.volumetric_tile_size = '4'
    scene.eevee.use_volumetric_shadows = True
except Exception as error: print("eevee option:", error)
try:
    scene.view_settings.view_transform = 'AgX'; scene.view_settings.look = 'AgX - Medium High Contrast'
except Exception as error: print("view option:", error)
scene.view_settings.exposure = .6
os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.render.render(write_still=True)
print("wrote", OUT, "with", scene.render.engine)
