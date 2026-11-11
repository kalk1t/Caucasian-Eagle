"""Build an editable exploration-map concept in Blender 5.2+.

Run: blender --background --factory-startup --disable-autoexec --python build_sanctuary.py
All dimensions and content are early proposals; no game logic is implemented.
"""
import bpy, math, random, os, sys
from mathutils import Vector

HERE=os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path: sys.path.insert(0,HERE)
import architecture as arch
import nature
random.seed(118)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
bpy.context.preferences.filepaths.save_version=0
scene=bpy.context.scene; scene.unit_settings.system='METRIC'

def material(name,color,roughness=.8,emission=0,metal=0):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF'); p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Roughness'].default_value=roughness; p.inputs['Metallic'].default_value=metal
    if emission:
        p.inputs['Emission Color'].default_value=(*color,1); p.inputs['Emission Strength'].default_value=emission
    return m
materials={
    'stone':material('Stone - slate',(.18,.205,.24)),
    'stone_light':material('Stone - weathered',(.29,.32,.35)),
    'stone_dark':material('Stone - shadow',(.08,.10,.13)),
    'trim':material('Stone - pale trim',(.37,.39,.40)),
    'wood':material('Trees - black bark',(.065,.07,.085)),
    'metal':material('Iron',(.09,.11,.14),.45,metal=.5),
    'gold':material('Aged bronze',(.30,.22,.12),.45,metal=.5),
    'soul':material('Soul - icy blue',(.08,.57,.93),.35,3.0),
    'warm':material('Lantern glow',(.98,.44,.11),.5,3.0),
    'marker':material('Scale reference',(.73,.53,.32)),
}
tiles=[material('Paving %02d'%i,c) for i,c in enumerate([(.20,.215,.24),(.24,.245,.25),(.26,.265,.27),(.29,.30,.30),(.18,.20,.22)])]
materials['bark']=materials['wood']
materials['dark']=materials['stone_dark']

# Preview-only procedural stone: adds weathering while keeping the file self-contained.
for m in [materials['stone'],materials['stone_light'],materials['stone_dark'],materials['trim']]+tiles:
    nt=m.node_tree; bs=nt.nodes.get('Principled BSDF'); base=tuple(bs.inputs['Base Color'].default_value)
    noise=nt.nodes.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value=9; noise.inputs['Detail'].default_value=4
    ramp=nt.nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.elements[0].color=tuple(min(1,c*.55) for c in base[:3])+(1,)
    ramp.color_ramp.elements[1].color=tuple(min(1,c*1.25) for c in base[:3])+(1,)
    bump=nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value=.20; bump.inputs['Distance'].default_value=.04
    nt.links.new(noise.outputs['Fac'],ramp.inputs['Fac']); nt.links.new(ramp.outputs['Color'],bs.inputs['Base Color'])
    nt.links.new(noise.outputs['Fac'],bump.inputs['Height']); nt.links.new(bump.outputs['Normal'],bs.inputs['Normal'])

def box(name,loc,dimensions,mat,angle=0,bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc); ob=bpy.context.object; ob.name=name; ob.dimensions=dimensions
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); ob.rotation_euler[2]=angle; ob.data.materials.append(mat)
    if bevel:
        md=ob.modifiers.new('Edge bevel','BEVEL'); md.width=bevel; md.segments=2
    return ob
def cylinder(name,loc,radius,depth,mat,vertices=64):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc)
    ob=bpy.context.object; ob.name=name; ob.data.materials.append(mat); return ob
def cone(name,loc,radius,depth,mat,upper=0,vertices=8):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=radius,radius2=upper,depth=depth,location=loc)
    ob=bpy.context.object; ob.name=name; ob.data.materials.append(mat); return ob
def annulus(name,r_in,r_out,z,thickness,mat,segments=96):
    verts=[]
    for zz in (z-thickness,z):
        for r in (r_in,r_out):
            verts.extend([(r*math.cos(2*math.pi*i/segments),r*math.sin(2*math.pi*i/segments),zz) for i in range(segments)])
    n=segments; faces=[]
    for i in range(n):
        j=(i+1)%n
        faces.extend([(2*n+i,2*n+j,3*n+j,3*n+i), (i,n+i,n+j,j), (i,j,2*n+j,2*n+i), (n+i,3*n+i,3*n+j,n+j)])
    mesh=bpy.data.meshes.new(name); mesh.from_pydata(verts,[],faces); mesh.update()
    o=bpy.data.objects.new(name,mesh); bpy.context.collection.objects.link(o); o.data.materials.append(mat); return o
def sector(name,r0,r1,a0,a1,z,mat):
    coords=[(r0*math.cos(a0),r0*math.sin(a0),z),(r1*math.cos(a0),r1*math.sin(a0),z),(r1*math.cos(a1),r1*math.sin(a1),z),(r0*math.cos(a1),r0*math.sin(a1),z)]
    mesh=bpy.data.meshes.new(name); mesh.from_pydata(coords,[],[(0,1,2,3)]); mesh.update()
    o=bpy.data.objects.new(name,mesh); bpy.context.collection.objects.link(o); o.data.materials.append(mat); return o
def polar(r,a): return (r*math.cos(a),r*math.sin(a))
def empty(name,pos,description):
    o=bpy.data.objects.new(name,None); bpy.context.collection.objects.link(o); o.location=pos; o.empty_display_type='PLAIN_AXES'; o.empty_display_size=1.1; o['purpose']=description; return o
def point(name,loc,color,energy,radius):
    d=bpy.data.lights.new(name,'POINT'); d.energy=energy; d.color=color; d.shadow_soft_size=radius
    o=bpy.data.objects.new(name,d); bpy.context.collection.objects.link(o); o.location=loc

# Connected navigable form: central disk, four broad radial walks, continuous outer ring.
cylinder('Arena foundation',(0,0,-1.0),25,2,materials['stone_dark'],96)
cylinder('Arena surface',(0,0,.02),24.6,.10,materials['stone'],96)
for r0,r1 in ((2,7),(7.1,12),(12.1,18),(18.1,24.3)):
    count=24 if r1<13 else 48
    for i in range(count):
        a=2*math.pi*i/count; b=2*math.pi*(i+1)/count-.003
        sector('Arena flagstone',r0,r1,a,b,.09,random.choice(tiles))
for r in (7,12,18,24.4): annulus('Arena circular inlay',r,r+.12,.12,.08,materials['trim'],96)
cylinder('Arena central seal',(0,0,.13),2,.09,materials['soul'],32)
annulus('Outer circular foundations',59,77,0,2,materials['stone_dark'],96)
for r0,r1 in ((59.3,64.8),(64.9,70.6),(70.7,76.7)):
    for i in range(96):
        a=2*math.pi*i/96; b=2*math.pi*(i+1)/96-.002
        sector('Outer ring stone',r0,r1,a,b,.07,random.choice(tiles))
for r in (59.2,76.8): annulus('Edge band',r,r+.25,.16,.20,materials['trim'],96)

for deg in (0,90,180,270):
    angle=math.radians(deg); x,y=polar(41.7,angle)
    box('Radial bridge %d'%deg,(x,y,-.85),(36,9,1.8),materials['stone_dark'],angle)
    for step in range(9):
        x,y=polar(25.6+4*step,angle); box('Bridge flagstone',(x,y,.05),(3.94,8.9,.10),random.choice(tiles),angle)
    for side in (-1,1):
        ox=-math.sin(angle)*side*4.8; oy=math.cos(angle)*side*4.8
        x,y=polar(41.7,angle); box('Bridge parapet',(x+ox,y+oy,.75),(36,.55,1.5),materials['stone'],angle,.08)
        for i in range(6):
            x,y=polar(27.5+i*5.7,angle); box('Bridge merlon',(x+ox,y+oy,1.7),(1,.8,.65),materials['trim'],angle,.04)

# Four distinct broad stone destinations overlap the ring; each has direct walking access.
for name,ang in [('EAST_GARDEN',0),('NORTH_TEMPLE',90),('WEST_CRYPT',180),('SOUTH_GATE',270)]:
    x,y=polar(78,math.radians(ang)); cylinder('District '+name,(x,y,-.55),15,1.1,materials['stone_dark'],48)
    cylinder('District paving '+name,(x,y,.04),14.7,.12,materials['stone_light'],48)
    empty('DISTRICT_'+name,(x,y,.15),'Location marker for exploration; no runtime behavior')

# Four smaller connected outlooks create distinct places to discover along the loop.
for name,deg in [('NORTHEAST_CLOISTER',45),('NORTHWEST_RUINS',135),('SOUTHWEST_MEMORIAL',225),('SOUTHEAST_GARDEN',315)]:
    a=math.radians(deg); x,y=polar(75,a)
    cylinder('Outlook foundation '+name,(x,y,-.65),11,1.3,materials['stone_dark'],32)
    cylinder('Outlook paving '+name,(x,y,.025),10.8,.15,materials['stone'],32)
    empty('DISTRICT_'+name,(x,y,.15),'Exploration landmark; marker only')

# Wall fragments stop near cardinal entrances. All decorative, not validated collisions.
for i in range(64):
    angle=2*math.pi*i/64
    if min(abs(((angle-j*math.pi/4+math.pi)%(2*math.pi))-math.pi) for j in range(8))<.15: continue
    x,y=polar(76.9,angle); box('Ring outer battlement',(x,y,1.05),(6.7,.65,2.1),materials['stone'],angle+math.pi/2,.07)
    if i%2==0: box('Battlement cap',(x,y,2.25),(2.8,.9,.55),materials['trim'],angle+math.pi/2,.04)

# Authentic model kit provided by architecture.py; front of each temple faces inward.
arch.create_temple((0,84,.12),rotation=0,scale=1,materials=materials)
arch.create_temple((-82,0,.12),rotation=math.pi/2,scale=.65,materials=materials)
arch.create_arch((0,-85,.12),rotation=math.pi,scale=1.0,materials=materials)
arch.create_arch((83,0,.12),rotation=math.pi/2,scale=.75,materials=materials)
for x in (-6,6):
    box('Temple torch bracket',(x,78.35,2.4),(.45,.45,.8),materials['metal'])
    cone('Temple torch',(x,78.15,3.05),.24,.65,materials['warm'],0,8)
    point('Temple torchlight',(x,77.5,3.4),(1,.38,.10),600,1.0)
for i in range(16):
    angle=2*math.pi*(i+.5)/16; x,y=polar(73.3,angle)
    arch.create_bench((x,y,.15),rotation=angle+math.pi/2,scale=.85,materials=materials)
for deg in (45,135,225,315):
    a=math.radians(deg); x,y=polar(80,a)
    arch.create_arch((x,y,.12),rotation=a-math.pi/2,scale=.7,materials=materials)
    for delta in (-.075,.075):
        xx,yy=polar(68,a+delta); arch.create_broken_column((xx,yy,.10),rotation=a,scale=.65,materials=materials)
for i in range(12):
    angle=2*math.pi*(i+.5)/12; x,y=polar(22.5,angle)
    arch.create_broken_column((x,y,.10),rotation=angle,scale=.7,materials=materials)

# Grave garden on the east district, displaced from its approach.
for x,y in [(78,-8),(82,-6),(86,-4),(88,0),(86,4),(82,7),(77,9)]:
    nature.create_grave((x,y,.16),rotation=.2,scale=.9,materials=materials)
# Dead trees punctuate the outer boundary, without filling main walking routes.
for i in range(16):
    angle=2*math.pi*(i+.5)/16; x,y=polar(72,angle)
    nature.create_tree((x,y,.12),rotation=angle,scale=1.2+(i%3)*.18,materials=materials,seed=100+i)

# Restrained soul lamps and moonlit silhouette accents.
for i in range(12):
    a=2*math.pi*i/12+math.pi/12; x,y=polar(61,a)
    cylinder('Soul lantern plinth',(x,y,.7),.65,1.4,materials['stone_dark'],8)
    cone('Soul flame',(x,y,2.0),.34,1.2,materials['soul'],0,8)
    if i%3==0: point('Soul light',(x,y,3),(.30,.65,1),800,4)
for i in range(8):
    a=2*math.pi*i/8+math.pi/8; x,y=polar(23,a)
    cone('Arena soul flame',(x,y,1.6),.28,1.0,materials['soul'],0,8)

# Human scale reference only, not Nyxara's character asset or monster AI.
cylinder('PLAYER SCALE - body',(1,-11,1.08),.28,1.2,materials['marker'],12)
cone('PLAYER SCALE - head',(1,-11,1.75),.20,.35,materials['marker'],.1,12)
for dx in (-.13,.13): cylinder('PLAYER SCALE - leg',(1+dx,-11,.3),.10,.6,materials['marker'],8)
empty('SPAWN_PLAYER_SOUTH',(0,-72,.1),'Example only - no spawning logic')
empty('ENCOUNTER_ARENA',(0,0,.1),'Example encounter marker - no monster AI')

# Floating rock silhouettes below walkable paths, atmospheric backdrop only.
for i in range(20):
    a=2*math.pi*i/20; x,y=polar(70,a)
    cone('Floating foundation',(x,y,-8),.4,14,materials['stone_dark'],upper=random.uniform(5,8),vertices=7)
cylinder('Arena floating foundation',(0,0,-9),13,16,materials['stone_dark'],9)

world=scene.world; world.use_nodes=True; bg=world.node_tree.nodes['Background']; bg.inputs['Color'].default_value=(.025,.04,.085,1); bg.inputs['Strength'].default_value=.35
def area(name,pos,power,size,color,target=(0,0,0)):
    d=bpy.data.lights.new(name,'AREA'); d.energy=power; d.shape='DISK'; d.size=size; d.color=color
    o=bpy.data.objects.new(name,d); bpy.context.collection.objects.link(o); o.location=pos; o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
area('Moonlight',(20,45,110),100000,90,(.48,.62,1))
area('Cool fill',(-60,-45,75),55000,80,(.36,.46,.72))
scene.render.engine='CYCLES'; scene.cycles.samples=20; scene.cycles.use_denoising=True
scene.render.resolution_x=1800; scene.render.resolution_y=1200; scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'; scene.render.image_settings.file_format='PNG'
data=bpy.data.cameras.new('Review camera'); camera=bpy.data.objects.new('Review camera',data); bpy.context.collection.objects.link(camera); scene.camera=camera
def camera_at(pos,target,scale):
    camera.location=pos; camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler(); data.type='ORTHO'; data.ortho_scale=scale
camera_at((155,-210,235),(0,0,0),230)
scene.render.filepath=os.path.join(HERE,'sanctuary-overview.png'); bpy.ops.render.render(write_still=True)
scene.render.resolution_x=1400; scene.render.resolution_y=1400
camera_at((0,-.01,260),(0,0,0),210)
scene.render.filepath=os.path.join(HERE,'sanctuary-top.png'); bpy.ops.render.render(write_still=True)
scene.render.resolution_x=1800; scene.render.resolution_y=1200
camera.location=(8,44,5); camera.rotation_euler=(Vector((0,84,5))-camera.location).to_track_quat('-Z','Y').to_euler(); data.type='PERSP'; data.lens=35
scene.render.filepath=os.path.join(HERE,'sanctuary-temple-view.png'); bpy.ops.render.render(write_still=True)
camera_at((155,-210,235),(0,0,0),230)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE,'Moonlit_Sanctuary_v01.blend'))
print('BUILD_COMPLETE',len(bpy.data.objects))
