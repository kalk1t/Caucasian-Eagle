"""The far version of each place model: the game model with a quarter of its triangles, drawn by the game from 12 times
its height away (Journey, FarModelHeights). It keeps the UVs, so it uses the same colour and normal maps.

    blender -b --factory-startup -P art/places/make_far.py -- <asset builder exe> [Name ...]

Reads   art/places/out/<Name>.glb                 the game model (prepare_places.py; it also calls this for each model)
Writes  art/places/out/<Name>Far.glb
        game/assets/Compiled/Places/<Name>Far.dmesh
Models under FAR_MIN triangles get none: there is too little to gain.
"""
import bpy, sys, pathlib, shutil, subprocess, tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
OUT = REPO / "art" / "places" / "out"
GAME = REPO / "game" / "assets" / "Compiled" / "Places"
FAR_RATIO = 0.25
FAR_MIN = 4000


def make_far(name, builder):
    """Returns the far version's triangle count, or 0 when the model gets none."""
    glb = OUT / (name + ".glb")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(glb))
    obj = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    far_glb = OUT / (name + "Far.glb")
    if tris < FAR_MIN:
        far_glb.unlink(missing_ok=True); (GAME / (name + "Far.dmesh")).unlink(missing_ok=True)
        return 0
    mod = obj.modifiers.new("Far", 'DECIMATE'); mod.ratio = FAR_RATIO; mod.use_collapse_triangulate = True
    with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    far = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(far_glb), export_format='GLB', use_selection=True, export_apply=True, export_yup=True,
                              export_lights=False, export_cameras=False, export_tangents=True)
    work = pathlib.Path(tempfile.mkdtemp(prefix="ce_far_"))
    result = subprocess.run([builder, "static-pipeline", str(far_glb), str(work / "out")], capture_output=True, text=True)   # it wants a fresh folder
    made = list(work.rglob("*.dmesh"))
    if not made:
        print("FAR BUILDER FAILED", name, result.stdout[-300:]); shutil.rmtree(work, ignore_errors=True); return -1
    shutil.copy(made[0], GAME / (name + "Far.dmesh")); shutil.rmtree(work, ignore_errors=True)
    return far


if __name__ == "__main__" and "--" in sys.argv:
    args = sys.argv[sys.argv.index("--") + 1:]
    builder, names = args[0], args[1:]
    if not names: names = sorted(p.stem for p in OUT.glob("*.glb") if not p.stem.endswith("Far"))
    for n in names:
        print("FAR %-13s %d" % (n, make_far(n, builder)))
