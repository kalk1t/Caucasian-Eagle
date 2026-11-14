"""Writes game/src/NyxaraEquipment.h from the equipment manifest.

    python art/nyxara/equipment/gen_equipment.py art/nyxara/equipment/equipment_manifest.json game/src/NyxaraEquipment.h

The manifest came with the modular weapons (grip frames measured from the model the weapons were cut out of).
The places on her back are set here by hand: they are not in the manifest.
"""
import json, sys

USE = ["InfernoAxe", "SerpentcoilAxe", "EldritchLongbow"]
# On her back, in her own frame at rest (metres from her feet: x across, y up, z ahead), then the turn
# about x, y and z in radians (applied in the order z, x, y, as XMMatrixRotationRollPitchYaw does).
BACK = {
    "InfernoAxe": ((0.20, 0.78, -0.15), (0.0, 0.0, 0.4)),
    "SerpentcoilAxe": ((-0.20, 0.78, -0.15), (0.0, 0.0, -0.4)),
    "EldritchLongbow": ((0.0, 1.20, -0.17), (0.0, 0.0, 0.3)),
}
CR, LF = chr(13), chr(10)


def num(v):
    s = "%.9g" % v
    return (s if "." in s or "e" in s else s + ".") + "f"


def mat(rows): return "{" + ", ".join(num(v) for r in rows for v in r) + "}"
def vec(v): return "{" + ", ".join(num(x) for x in v) + "}"


m = json.load(open(sys.argv[1]))
out = ["// Written from the equipment manifest of the modular weapons (art/nyxara/equipment/equipment_manifest.json) by",
       "// art/nyxara/equipment/gen_equipment.py. Do not edit the numbers: change the manifest or that script and write this again.",
       "// Each weapon is its own mesh with its grip at the origin and its blade or limbs along +Y. HandL and HandR place it",
       "// in a hand: a row-major matrix in the frame of the rig's Hand_L or Hand_R joint (the engine multiplies it by the",
       "// joint's sampled matrix and by the character's world matrix: DoorwaysCharacterSocket.h). The rig's right hand is",
       "// the one the player sees as her left (the engine shows the model mirrored).",
       "#pragma once", "", "namespace CaucasianEagle", "{",
       "    struct EquipmentItem",
       "    {",
       "        const char* Name;            // folder and file name under Characters/Nyxara/Equipment",
       "        float HandL[16], HandR[16];  // in the joint's frame",
       "        float BackAt[3], BackTurn[3]; // on her back: metres in her own frame at rest, and the turn about x, y, z (rad); carried by the Chest joint",
       "        const char* StringMaterial;  // a bow's straight string (hidden while she draws), or null",
       "        float StringTop[3], StringBottom[3], Nock[3];   // a bow: the ends of its string and where the arrow rests, in the weapon's frame",
       "    };",
       "    constexpr int EquipmentCount = %d;" % len(USE),
       "    constexpr EquipmentItem Equipment[EquipmentCount] = {"]
for name in USE:
    w = next(x for x in m["weapons"] if x["id"] == name)
    s = w["socket_local_row_major"]
    bow = w["type"] == "bow"
    zero = "{0, 0, 0}"
    out.append("        {\"%s\"," % name)
    out.append("         " + mat(s["Hand_L"]) + ",")
    out.append("         " + mat(s["Hand_R"]) + ",")
    out.append("         %s, %s," % (vec(BACK[name][0]), vec(BACK[name][1])))
    out.append("         %s, %s, %s, %s}," % ('"%s"' % w["string_material"] if bow else "nullptr",
               vec(w["string_top"]) if bow else zero, vec(w["string_bottom"]) if bow else zero, vec(w["nock_rest"]) if bow else zero))
out += ["    };", "    enum EquipmentIndex { EquipInferno = 0, EquipSerpentcoil = 1, EquipBow = 2 };",
        "    constexpr const char* EquipmentBackJoint = \"Chest\";",
        "    constexpr const char* EquipmentBowHand = \"Hand_R\";    // the hand the player sees as her left",
        "    constexpr const char* EquipmentStringHand = \"Hand_L\";", "}", ""]
open(sys.argv[2], "w", newline="").write((CR + LF).join(out))
print("written", sys.argv[2])
