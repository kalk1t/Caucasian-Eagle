"""Create the image-to-3d tasks for every prop concept, wait, and download the models.

Usage: python make_models.py          (30 credits per model; key from MESHY_API_KEY)
Writes req/<name>_3d.json, model_ids.txt and out/<name>_3d/.
"""
import json, pathlib, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
CLIENT = HERE.parent.parent / "nyxara" / "meshy" / "meshy.py"
POLYS = {"tower": 12000, "deadtree": 7000, "fir": 7000, "rocks": 6000}

ids = {}
done = HERE / "model_ids.txt"
if done.exists():
    ids = dict(line.split() for line in done.read_text().splitlines() if line.strip())

for folder in sorted((HERE / "out").glob("*_concept_1")):
    name = folder.name.split("_")[0]
    if name in ids:
        continue
    task = json.loads((folder / "task.json").read_text(encoding="utf-8"))
    request = {
        "image_url": task["image_urls"][0],
        "ai_model": "latest",
        "should_texture": True,
        "enable_pbr": True,
        "texture_resolution": "2k",
        "should_remesh": True,
        "topology": "triangle",
        "target_polycount": POLYS.get(name, 8000),
        "target_formats": ["glb"],
    }
    path = HERE / "req" / f"{name}_3d.json"
    path.write_text(json.dumps(request, indent=1), encoding="utf-8")
    result = subprocess.run([sys.executable, str(CLIENT), "create", "v1/image-to-3d", str(path), f"prop_{name}_3d"],
                            capture_output=True, text=True)
    if result.returncode != 0:
        print("FAILED", name, result.stdout[-300:], result.stderr[-300:])
        continue
    ids[name] = result.stdout.strip().splitlines()[-1]
    done.write_text("".join(f"{k} {v}\n" for k, v in sorted(ids.items())))
    print("CREATED", name, flush=True)

for name, task_id in sorted(ids.items()):
    out = HERE / "out" / f"{name}_3d"
    if (out / "task.json").exists():
        continue
    result = subprocess.run([sys.executable, str(CLIENT), "wait", "v1/image-to-3d", task_id, str(out)],
                            capture_output=True, text=True)
    print(name, [l for l in result.stdout.splitlines() if l.startswith(("STATUS", "BALANCE"))], flush=True)
