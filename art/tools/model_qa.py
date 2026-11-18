"""The QA gate for game models: qa_check.py (blender_skills, game profile) on each model, one line a model in a report.

As a module (inside Blender):   import model_qa; model_qa.check(obj, name, report)   -> "pass zf 0" / "FAIL zf 3 +2 other"
On its own:   blender -b --factory-startup -P art/tools/model_qa.py -- <report.txt> <model.glb> ...

A report keeps one line per model (name, verdict, triangles, z-fighting pairs, errors, warnings); checking a model again
replaces its line, the others stay. The "duplicate verts" warning is left out: the asset builder welds them.
Used by art/refine/refine_assets.py (creatures, props, the chest; art/refine/qa_report.txt). art/places/prepare_places.py
has its own copy of the same check, with the source's triangles too (art/places/qa_report.txt).
"""
import bpy, sys, os, pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent / "blender_skills"))
import qa_check

HEADER = "# model\tverdict\ttriangles\tz-fighting\terrors\twarnings  (art/tools/model_qa.py: blender_skills/qa_check.py, game profile)\n"


def check(obj, name, report):
    r = qa_check.run(objects=[obj.name], profile="game")
    zf = sum(int(e.split("(")[1].split()[0]) for e in r["errors"] if e.startswith("z-fighting") and "(" in e)
    errors = [e for e in r["errors"] if not e.startswith("z-fighting")]
    warnings = [w for w in r["warnings"] if "duplicate verts" not in w]
    verdict = "pass" if r["pass"] else "FAIL"
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    line = "%s\t%s\t%d tris\tz-fight pairs %d\t%s\t%s\n" % (name, verdict, tris, zf, "; ".join(errors) or "-", "; ".join(warnings) or "-")
    report = pathlib.Path(report)
    lines = {}
    if report.exists():
        for old in report.read_text(encoding="utf-8").splitlines(keepends=True):
            if old.strip() and not old.startswith("#"): lines[old.split("\t")[0]] = old
    lines[name] = line
    report.write_text(HEADER + "".join(lines[k] for k in sorted(lines)), encoding="utf-8")
    return "%s zf %d%s" % (verdict, zf, (" +%d other" % len(errors)) if errors else "")


if __name__ == "__main__" and "--" in sys.argv:
    args = sys.argv[sys.argv.index("--") + 1:]
    for glb in args[1:]:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=glb)
        meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
        bpy.ops.object.select_all(action='DESELECT')
        for o in meshes: o.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        if len(meshes) > 1: bpy.ops.object.join()
        name = os.path.splitext(os.path.basename(glb))[0]
        print("QA %-12s %s" % (name, check(bpy.context.view_layer.objects.active, name, args[0])))
