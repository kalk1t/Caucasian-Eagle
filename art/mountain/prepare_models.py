"""Prepares the Meshy models of the mountain scene for the Doorways asset builder.

Usage:  python prepare_models.py <meshy out folder> <export folder> <game texture folder>
For Amirani, Eagle and Dog: copies the GLB with its mesh nodes renamed Doorways_<Name> (the
importer only accepts render nodes with that prefix) and writes a 1024 px base-colour PNG.
"""
import sys, json, struct, pathlib
from PIL import Image

src, export, textures = (pathlib.Path(a) for a in sys.argv[1:4])
export.mkdir(parents=True, exist_ok=True)
textures.mkdir(parents=True, exist_ok=True)

for name in ("Amirani", "Eagle", "Dog"):
    folder = src / f"{name.lower()}_3d_1"
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
    out = struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(body) + len(rest)) + struct.pack("<I4s", len(body), b"JSON") + body + rest
    (export / f"{name}.glb").write_bytes(out)
    Image.open(folder / "texture_urls_0_base_color.png").convert("RGB").resize((1024, 1024), Image.LANCZOS).save(textures / f"{name}BaseColor.png")
    print(name, "mesh nodes:", count)
