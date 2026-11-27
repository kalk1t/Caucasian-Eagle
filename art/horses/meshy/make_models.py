"""The six horses as 3D models from their concept pictures (Meshy image-to-3d, 30 credits each; key from MESHY_API_KEY).

Usage:  python make_models.py [name ...]     (none: every horse with a concept in out/; one already made is not asked again)
Writes req/<name>_3d.json, model_ids.txt and out/<name>_3d/ (task.json, the model and its maps), as the creatures'
make_models.py does (art/monsters/meshy).
"""
import json, pathlib, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
CLIENT = HERE.parent.parent / "tools" / "meshy.py"
POLYS = {"rashi": 24000}   # (her wings: more surface)

ids = {}
done = HERE / "model_ids.txt"
if done.exists():
    ids = dict(line.split() for line in done.read_text().splitlines() if line.strip())
wanted = sys.argv[1:]

for folder in sorted((HERE / "out").glob("*_concept_1")):
    name = folder.name.split("_")[0]
    if name in ids or (wanted and name not in wanted):
        continue
    task = json.loads((folder / "task.json").read_text(encoding="utf-8"))
    if task.get("status") != "SUCCEEDED":
        print("NO CONCEPT", name); continue
    request = {
        "image_url": task["image_urls"][0],
        "ai_model": "latest",
        "should_texture": True,
        "enable_pbr": True,
        "texture_resolution": "2k",
        "should_remesh": True,
        "topology": "triangle",
        "target_polycount": POLYS.get(name, 20000),
        "target_formats": ["glb"],
    }
    path = HERE / "req" / f"{name}_3d.json"
    path.write_text(json.dumps(request, indent=1), encoding="utf-8")
    result = subprocess.run([sys.executable, "-I", str(CLIENT), "create", "v1/image-to-3d", str(path), f"horse_{name}_3d"],
                            capture_output=True, text=True)
    if result.returncode != 0:
        print("FAILED", name, result.stdout[-300:], result.stderr[-300:]); continue
    ids[name] = result.stdout.strip().splitlines()[-1]
    done.write_text("".join(f"{k} {v}\n" for k, v in sorted(ids.items())))
    print("CREATED", name, flush=True)

for name, task_id in sorted(ids.items()):
    out = HERE / "out" / f"{name}_3d"
    if (out / "task.json").exists() or (wanted and name not in wanted):
        continue
    result = subprocess.run([sys.executable, "-I", str(CLIENT), "wait", "v1/image-to-3d", task_id, str(out)], capture_output=True, text=True)
    print(name, [l for l in result.stdout.splitlines() if l.startswith(("STATUS", "BALANCE"))], flush=True)
