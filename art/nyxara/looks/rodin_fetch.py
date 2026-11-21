"""Fetch a finished Hyper3D (Rodin) generation's PBR model into art/nyxara/looks/rodin/<name>/.

    python rodin_fetch.py <name> <generation-id>

The model is the generation's base_basic_pbr.glb (PBR textures, no baked light). Each model goes into a folder of its
own (downloads are kept apart from the scripts). The generations are listed in GENERATIONS.tsv beside this script, so a
model can be fetched again (`hyper3d result <id>` gives fresh links).
"""
import json, pathlib, subprocess, sys, urllib.request

HERE = pathlib.Path(__file__).resolve().parent


def main():
    name, gid = sys.argv[1], sys.argv[2]
    out = HERE / "rodin" / name
    out.mkdir(parents=True, exist_ok=True)
    raw = subprocess.run("hyper3d --output json result " + gid, shell=True, capture_output=True, text=True, check=True).stdout
    files = json.loads(raw)["files"]
    url = next(f["url"] for f in files if f["name"] == "base_basic_pbr.glb")
    target = out / "base_basic_pbr.glb"
    with urllib.request.urlopen(url, timeout=120) as r, open(target, "wb") as f:
        f.write(r.read())
    print(name, gid, target.stat().st_size, "bytes")


if __name__ == "__main__":
    main()
