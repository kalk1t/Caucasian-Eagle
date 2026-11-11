"""Prepares Meshy models (monsters, chest, scenery props) for the Doorways asset builder and compiles them.

Usage:  python prepare_models.py <meshy out folder> <game compiled folder> <asset builder exe> Name=meshyname ...
For each model: copies the GLB with its mesh nodes renamed Doorways_<Name> (the importer only accepts
render nodes with that prefix), compiles it with static-pipeline and writes <Name>.dmesh and a
1024 px <Name>BaseColor.png into the game folder.
"""
import sys, json, struct, pathlib, shutil, subprocess, tempfile
from PIL import Image

src, dest, builder = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
dest.mkdir(parents=True, exist_ok=True)
work = pathlib.Path(tempfile.mkdtemp(prefix="ce_models_"))
for pair in sys.argv[4:]:
    name, meshy = pair.split("=")
    folder = src / f"{meshy}_3d"
    data = (folder / "model_url.glb").read_bytes()
    json_len = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + json_len])
    rest = data[20 + json_len:]
    count = 0
    for node in doc.get("nodes", []):
        if "mesh" in node:
            node["name"] = f"Doorways_{name}" + (f"_{count}" if count else "")
            count += 1
    body = json.dumps(doc, separators=(",", ":")).encode()
    body += b" " * (-len(body) % 4)
    glb = work / f"{name}.glb"
    glb.write_bytes(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(body) + len(rest)) + struct.pack("<I4s", len(body), b"JSON") + body + rest)
    out = work / f"{name}_out"
    result = subprocess.run([builder, "static-pipeline", str(glb), str(out)], capture_output=True, text=True)
    meshes = list(out.rglob("*.dmesh"))
    if not meshes:
        print("FAILED", name, result.stdout[-400:])
        continue
    shutil.copy(meshes[0], dest / f"{name}.dmesh")
    Image.open(folder / "texture_urls_0_base_color.png").convert("RGB").resize((1024, 1024), Image.LANCZOS).save(dest / f"{name}BaseColor.png")
    print(name, "mesh nodes:", count, "bytes:", meshes[0].stat().st_size)
shutil.rmtree(work, ignore_errors=True)
