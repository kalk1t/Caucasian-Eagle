"""Check the asset file passed to Blender is a single editable, nonempty mesh."""
import bpy
objs=list(bpy.context.scene.objects)
assert len(objs)==1, [(o.name,o.type) for o in objs]
obj=objs[0]
assert obj.type=='MESH' and len(obj.data.vertices)>0 and len(obj.data.polygons)>0
assert all(abs(v)<1e-6 for v in obj.location)
assert obj.data.materials and obj.data.materials[0] is not None
print('VERIFIED_NATURE_ASSET',bpy.data.filepath,obj.name,len(obj.data.vertices),len(obj.data.polygons))
