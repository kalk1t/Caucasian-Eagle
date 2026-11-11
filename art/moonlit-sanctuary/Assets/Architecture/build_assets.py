"""Build four standalone, editable architecture source assets.

Run in Blender 5.2: blender -b --factory-startup --disable-autoexec --python build_assets.py
Each file uses metre units and Z up, at the world origin; front faces local -Y.
Geometry and procedural materials are preview-source content, not a game export.
"""
import bpy, os, sys

HERE=os.path.dirname(os.path.abspath(__file__))
ROOT=os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, ROOT)
import architecture

bpy.context.preferences.filepaths.save_version=0

def make_material(name,color,roughness=.8,metal=0):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
    bs=m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value=(*color,1)
    bs.inputs['Roughness'].default_value=roughness; bs.inputs['Metallic'].default_value=metal
    return m

def mats():
    d={
      'stone':make_material('Stone - slate',(.18,.205,.24)),
      'trim':make_material('Stone - pale trim',(.37,.39,.40)),
      'dark':make_material('Stone - shadow',(.08,.10,.13)),
      'metal':make_material('Iron',(.09,.11,.14),.45,.5),
    }
    for key in ('stone','trim','dark'):
        m=d[key]; nt=m.node_tree; bs=nt.nodes.get('Principled BSDF')
        base=tuple(bs.inputs['Base Color'].default_value)
        noise=nt.nodes.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value=9; noise.inputs['Detail'].default_value=4
        ramp=nt.nodes.new('ShaderNodeValToRGB')
        ramp.color_ramp.elements[0].color=tuple(min(1,c*.55) for c in base[:3])+(1,)
        ramp.color_ramp.elements[1].color=tuple(min(1,c*1.25) for c in base[:3])+(1,)
        bump=nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value=.20; bump.inputs['Distance'].default_value=.04
        nt.links.new(noise.outputs['Fac'],ramp.inputs['Fac']); nt.links.new(ramp.outputs['Color'],bs.inputs['Base Color'])
        nt.links.new(noise.outputs['Fac'],bump.inputs['Height']); nt.links.new(bump.outputs['Normal'],bs.inputs['Normal'])
    return d

for label,builder in [('Temple',architecture.create_temple),('Gothic_Arch',architecture.create_arch),
                      ('Stone_Bench',architecture.create_bench),('Broken_Column',architecture.create_broken_column)]:
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    bpy.context.scene.unit_settings.system='METRIC'
    builder((0,0,0),rotation=0,scale=1,materials=mats())
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    assert meshes, label
    assert not any(o.type in {'CAMERA','LIGHT'} for o in bpy.context.scene.objects)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE,'MS_'+label+'.blend'))
    print('ASSET',label,'MESHES',len(meshes),'VERTICES',sum(len(o.data.vertices) for o in meshes))
