"""Writes the Meshy concept requests for the road monsters and the gold chest (one JSON per asset in req/)."""
import json, pathlib
HERE = pathlib.Path(__file__).resolve().parent
STYLE = ("Single game enemy on a plain light gray background, stylized high-quality dark fantasy action game style, "
         "full body visible, three-quarter front view, standing in a menacing ready pose with arms slightly away from the body, "
         "clear readable silhouette, no text, no base, no ground.")
P = {
 "chinka": "A Chinka, a small mischievous forest goblin of Georgian folklore: child-sized, wiry, long pointed ears, wide grin with needle teeth, big yellow eyes, tangled moss-green hair, ragged bark-and-leaf clothing, long thin fingers holding a short bone dagger.",
 "ali": "An Ali, a malevolent water spirit woman of Georgian folklore: tall pale drowned woman with very long wet black hair covering half her face, long sharp nails, tattered white-green river-weed dress, bluish skin, empty glowing white eyes, bare feet turned backwards.",
 "kudiani": "A Kudiani, a hunchbacked witch of Georgian folklore: old crooked woman with a hooked nose, a long thin animal tail showing under her patched dark robe, iron teeth, wild gray hair, a twisted wooden staff topped with a glowing green ember, charms and bones on her belt.",
 "ochokochi": "An Ochokochi, the goat-man of Colchian forest folklore: huge hairy beast-man with goat legs and hooves, rusty reddish fur, curled horns, long thorn-like claws, and a sharp axe-shaped bone blade growing out of the middle of his chest.",
 "almasty": "An Almasty, the wild man of the Caucasus mountains: massive hunched ape-like humanoid covered in long shaggy dark brown hair, heavy brow, long arms reaching the knees, gripping a jagged boulder in one hand, snow on his shoulders.",
 "kaji": "A Kaji, a demonic smith-sorcerer of Georgian legend from the land of Kajeti: tall horned demon with ash-gray skin, glowing molten cracks on his arms, blackened iron armor plates, a leather smith apron, a huge glowing red-hot forge hammer in both hands.",
 "uaig": "A Uaig, a one-eyed giant of the Ossetian Nart sagas: towering brutish giant with a single large eye in the middle of his forehead, thick gray skin, a shaggy fur cloak, a necklace of skulls, carrying a tree trunk as a club.",
 "devi": "A Devi, a three-headed ogre giant of Georgian folklore: enormous muscular ogre with three ugly horned heads on thick necks, dark green-brown warty skin, tusks, a studded iron belt, hoarded gold chains around his wrists, a massive spiked iron mace.",
 "rokapi": "Rokapi, the old witch queen of Georgian folklore, leader of the Kudiani witches: gaunt ancient crone floating slightly, a tall crown of black antlers and bones, long tattered black and dark violet robes, broken iron chains hanging from her wrists, clawed hands crackling with violet fire.",
 "gveleshapi": "A Gveleshapi, the serpent-dragon of Georgian lakes and rivers: huge wingless serpent dragon rearing up with its body coiled beneath it, dark blue-green wet scales, a finned crest, long whiskers, rows of teeth, pale glowing belly, water dripping.",
}
for name, text in P.items():
    (HERE / "req" / f"{name}_concept.json").write_text(json.dumps({"ai_model": "gpt-image-2", "prompt": text + " " + STYLE}, indent=1), encoding="utf-8")
(HERE / "req" / "chest_concept.json").write_text(json.dumps({"ai_model": "gpt-image-2", "prompt":
    "An ancient treasure chest game prop on a plain light gray background: a heavy closed old gold chest with tarnished dented gold plates, "
    "dark iron bands and rivets, an eagle emblem on the front lock, worn engraved Caucasian knot ornaments, dust and age, slightly battered. "
    "Stylized high-quality dark fantasy game style, three-quarter front view, whole object visible, no text, no ground."}, indent=1), encoding="utf-8")
print(len(P) + 1, "requests")
