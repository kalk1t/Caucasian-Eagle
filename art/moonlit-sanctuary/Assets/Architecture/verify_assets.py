"""Reopen and inspect standalone prototype architecture files."""
import bpy, os
root=os.path.dirname(os.path.abspath(__file__))
for name,expected in [('MS_Temple.blend',137),('MS_Gothic_Arch.blend',26),('MS_Stone_Bench.blend',7),('MS_Broken_Column.blend',5)]:
    bpy.ops.wm.open_mainfile(filepath=os.path.join(root,name),load_ui=False)
    objects=list(bpy.context.scene.objects)
    meshes=[o for o in objects if o.type=='MESH']
    assert len(meshes)==expected,(name,len(meshes))
    assert all(o.type=='MESH' for o in objects),(name,[o.type for o in objects])
    assert all(o.data.materials for o in meshes),name
    print('VERIFIED',name,len(meshes),'meshes')
