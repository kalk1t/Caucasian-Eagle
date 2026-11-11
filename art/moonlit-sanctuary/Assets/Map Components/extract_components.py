"""Extract individual reusable layout components from the v01 scene.

Outputs are prototypes, not engine-ready assets. Re-run only after preserving
manual edits to previously extracted files: generated files are overwritten.
"""
import bpy, os, math, json
from mathutils import Matrix, Vector

ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),'..','..'))
OUT=os.path.dirname(os.path.abspath(__file__))
SOURCE=os.path.join(ROOT,'Moonlit_Sanctuary_v01.blend')

def starts(o, names): return any(o.name.startswith(n) for n in names)
def bridge(o):
    return (o.type=='MESH' and o.name.startswith('Radial bridge 0')) or (
        o.type=='MESH' and starts(o,['Bridge flagstone','Bridge parapet','Bridge merlon'])
        and 23 < o.location.x < 61 and abs(o.location.y) < 6)
def lamp(o):
    angle=math.pi/12
    x,y=61*math.cos(angle),61*math.sin(angle)
    return o.type=='MESH' and starts(o,['Soul lantern plinth','Soul flame']) and (o.location.xy-Vector((x,y))).length<.2
def first(o, name): return o.type=='MESH' and o.name==name

jobs=[
 ('arena_floor.blend',lambda o:o.type=='MESH' and starts(o,['Arena foundation','Arena surface','Arena flagstone','Arena circular inlay','Arena central seal']),(0,0,0),0),
 ('outer_ring_floor.blend',lambda o:o.type=='MESH' and starts(o,['Outer circular foundations','Outer ring stone','Edge band']),(0,0,0),0),
 ('radial_bridge.blend',bridge,(41.7,0,0),0),
 ('district_platform.blend',lambda o:o.type=='MESH' and o.name in ['District EAST_GARDEN','District paving EAST_GARDEN'],(78,0,0),0),
 ('soul_lamp.blend',lamp,(61*math.cos(math.pi/12),61*math.sin(math.pi/12),0),0),
 ('wall_section.blend',lambda o:first(o,'Ring outer battlement') or first(o,'Battlement cap'),None,None),
 ('paving_slab.blend',lambda o:first(o,'Bridge flagstone'),None,None),
 ('bridge_parapet.blend',lambda o:first(o,'Bridge parapet'),(41.7,-4.8,0),0),
 ('bridge_merlon.blend',lambda o:first(o,'Bridge merlon'),(27.5,-4.8,0),0),
 ('outlook_platform.blend',lambda o:o.type=='MESH' and o.name in ['Outlook foundation NORTHEAST_CLOISTER','Outlook paving NORTHEAST_CLOISTER'],(75/math.sqrt(2),75/math.sqrt(2),0),0),
 ('arena_seal.blend',lambda o:first(o,'Arena central seal'),(0,0,0),0),
 ('soul_flame.blend',lambda o:first(o,'Soul flame'),(61*math.cos(math.pi/12),61*math.sin(math.pi/12),0),0),
 ('torch_sconce.blend',lambda o:first(o,'Temple torch bracket') or first(o,'Temple torch'),(-6,78.35,0),0),
 ('floating_foundation.blend',lambda o:first(o,'Floating foundation'),None,0),
 ('scale_figure.blend',lambda o:o.type=='MESH' and o.name.startswith('PLAYER SCALE -'),(1,-11,0),0),
]
manifest=[]
for filename,predicate,anchor,angle in jobs:
    bpy.ops.wm.open_mainfile(filepath=SOURCE,load_ui=False)
    bpy.context.preferences.filepaths.save_version=0
    selected=[o for o in bpy.data.objects if predicate(o)]
    assert selected, 'No geometry matched '+filename
    if anchor is None:
        ref=next((o for o in selected if o.name=='Ring outer battlement'),selected[0])
        anchor=(ref.location.x,ref.location.y,0 if filename != 'floating_foundation.blend' else ref.location.z)
    if angle is None:
        angle=next((o for o in selected if o.name=='Ring outer battlement'),selected[0]).rotation_euler.z
    transform=Matrix.Rotation(-angle,4,'Z') @ Matrix.Translation(-Vector(anchor))
    for o in list(bpy.data.objects):
        if o not in selected: bpy.data.objects.remove(o,do_unlink=True)
    for o in selected: o.matrix_world=transform @ o.matrix_world
    # Remove scene-specific render setup and unused scene data without touching live material users.
    bpy.context.scene.camera=None
    bpy.ops.outliner.orphans_purge(do_recursive=True)
    path=os.path.join(OUT,filename)
    bpy.ops.wm.save_as_mainfile(filepath=path)
    manifest.append({'file':filename,'mesh_objects':len(selected)})
    print('EXTRACTED',filename,len(selected))
with open(os.path.join(OUT,'component_manifest.json'),'w',encoding='utf8') as f:
    json.dump(manifest,f,indent=2)
