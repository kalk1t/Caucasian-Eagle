"""Order Hyper3D (Rodin) models from text: each line of an order file is <name>\t<prompt>. 0.5 credits each
(Gen-2.5-Medium). The generation ids are added to GENERATIONS.tsv; rodin_fetch.py downloads each model when done.

    python rodin_order.py <order.tsv> [faces]
"""
import json, pathlib, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent


def main():
    order = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    faces = sys.argv[2] if len(sys.argv) > 2 else "30000"
    table = HERE / "GENERATIONS.tsv"
    done = {l.split("\t")[0] for l in table.read_text(encoding="utf-8").splitlines()[1:]} if table.exists() else set()
    for line in order:
        if not line.strip() or line.startswith("#"):
            continue
        name, prompt = line.split("\t", 1)
        if name in done:
            print("already ordered", name)
            continue
        out = subprocess.run(["hyper3d", "--output", "json", "generate", "--prompt", prompt, "--tier", "Gen-2.5-Medium",
                              "--mesh-mode", "Raw", "--quality", faces, "--format", "glb"],
                             capture_output=True, text=True, shell=True)
        gid = json.loads(out.stdout)["generation_id"]
        with open(table, "a", encoding="utf-8") as f:
            f.write("%s\t%s\t%s\n" % (name, gid, prompt))
        print("ordered", name, gid)


if __name__ == "__main__":
    main()
