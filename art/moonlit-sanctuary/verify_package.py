"""Read-only validation of each separate Blender prototype asset."""
import bpy, os, glob, math

root=os.path.dirname(os.path.abspath(__file__))
files=sorted(glob.glob(os.path.join(root,'Assets','*','*.blend')))
assert len(files)==22, 'Expected 22 individually saved assets, found '+str(len(files))
for path in files:
    bpy.ops.wm.open_mainfile(filepath=path,load_ui=False)
    meshes=[o for o in bpy.data.objects if o.type=='MESH']
    assert meshes and all(len(o.data.vertices)>0 for o in meshes),path
    assert all(math.isfinite(float(v)) for o in meshes for v in o.location),path
    assert all(not o.library for o in meshes),path
    print('OK:',os.path.relpath(path,root),'meshes:',len(meshes))
print('PACKAGE_VALIDATION_COMPLETE',len(files))
