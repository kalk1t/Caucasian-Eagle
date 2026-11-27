"""The six horses' concept pictures from Meshy (text-to-image, 9 credits each; key from MESHY_API_KEY).

The owner, 2026-11-27: "in characterlab , i want you to put at least 6 different kind of horses that nyxara will be able
to sit on"; asked, the owner chose six Meshy models, three horses of the Caucasus and three of legend.

Usage:  python make_concepts.py [name ...]     (none: all six; one already in out/ is not asked again)
Writes req/<name>_concept.json and out/<name>_concept_1/ (task.json and the picture).
"""
import json, pathlib, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
CLIENT = HERE.parent.parent / "tools" / "meshy.py"
STYLE = ("Single game mount: a saddled horse with no rider, on a plain light gray background, stylized high-quality dark "
         "fantasy action game style, the whole horse visible from the nose to the tail, three-quarter side view from its "
         "left, standing still with all four legs visible and clearly apart, the head raised, the tail hanging free, clear "
         "readable silhouette, no rider, no text, no base, no ground, no shadow.")
HORSES = {
    "tushetian": "A Tushetian mountain horse of Georgia: a small, stocky, sure-footed highland horse with a thick neck and "
                 "short strong legs, a shaggy dark bay coat, a black mane and tail and a thick forelock, a simple old leather "
                 "saddle over a wool saddle blanket woven with red Caucasian patterns, a plain rope bridle, small saddle bags.",
    "kabardian": "A Kabardian warhorse of the Caucasus: a tall powerful dark brown, almost black, stallion with a long arched "
                 "neck and a hooked Roman-nosed head, a lean muscular body and long legs, a Circassian war saddle of black "
                 "leather with silver fittings, a breastplate and a bridle with silver Caucasian ornaments.",
    "karabakh":  "A Karabakh horse of the Caucasus: an elegant, light and swift horse with a shining golden chestnut coat "
                 "with a metallic sheen, a fine head, slender legs, a flowing pale golden mane and tail, a light ornate saddle "
                 "on a red and gold saddle cloth, a fine bridle hung with small gold coins.",
    "rashi":     "Rashi, the winged horse of Georgian legend: a majestic white horse with two great feathered eagle wings "
                 "rising from its shoulders, raised and half spread, a silver-white coat, a long flowing silver mane, an "
                 "ancient bronze saddle with sun ornaments, a bronze bridle.",
    "fire":      "A fire horse of Georgian legend, born of Amirani's fire: a black horse whose mane and tail are living flames, "
                 "glowing ember-orange cracks across its dark coat like cooling lava, burning eyes, hooves glowing hot, a "
                 "blackened iron saddle on an ember-red cloth, an iron bridle.",
    "frost":     "A frost horse of the high Caucasus: a pale ice-blue and white horse with a mane and tail of icicles and "
                 "frost, frost crystals growing on its shoulders and legs, pale glowing blue eyes, a silver saddle on a dark "
                 "blue felt cloth, a silver bridle.",
}

names = sys.argv[1:] or list(HORSES)
for name in names:
    out = HERE / "out" / f"{name}_concept_1"
    if (out / "task.json").exists():
        print("HAVE", name); continue
    req = HERE / "req" / f"{name}_concept.json"
    req.parent.mkdir(parents=True, exist_ok=True)
    req.write_text(json.dumps({"ai_model": "gpt-image-2", "prompt": HORSES[name] + " " + STYLE}, indent=1), encoding="utf-8")
    made = subprocess.run([sys.executable, "-I", str(CLIENT), "create", "v1/text-to-image", str(req), f"horse_{name}_concept"],
                          capture_output=True, text=True)
    if made.returncode != 0:
        print("FAILED", name, made.stdout[-300:], made.stderr[-300:]); continue
    task = made.stdout.strip().splitlines()[-1]
    waited = subprocess.run([sys.executable, "-I", str(CLIENT), "wait", "v1/text-to-image", task, str(out)], capture_output=True, text=True)
    print(name, [l for l in waited.stdout.splitlines() if l.startswith(("STATUS", "BALANCE"))], flush=True)
