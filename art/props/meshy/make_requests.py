"""Writes the Meshy concept requests for the road scenery props (one JSON per asset in req/)."""
import json, pathlib
HERE = pathlib.Path(__file__).resolve().parent
STYLE = (" Single game environment prop on a plain light gray background, stylized high-quality dark fantasy game style, "
         "whole object visible from a three-quarter view, clear silhouette, no text, no ground plane, no people.")
P = {
 "tower": "An ancient Svan defensive stone watchtower of the Caucasus mountains: tall square tapering tower of rough gray stone blocks, small arrow-slit windows, a crenellated projecting top with arched machicolations, a weathered slate roof, moss and cracks, partly ruined.",
 "deadtree": "A large twisted dead tree: thick gnarled black trunk, bare crooked branches reaching upward like claws, exposed roots, patches of pale moss, no leaves.",
 "fir": "A tall dark Caucasian fir tree: straight trunk, dense layered drooping dark green needle branches dusted with snow, wide at the base and pointed at the top.",
 "rocks": "A cluster of jagged mountain boulders and one tall leaning standing stone carved with worn spiral and eagle petroglyphs, dark gray granite with lichen and moss.",
}
for name, text in P.items():
    (HERE / "req" / f"{name}_concept.json").write_text(json.dumps({"ai_model": "gpt-image-2", "prompt": text + STYLE}, indent=1), encoding="utf-8")
print(len(P), "requests")
