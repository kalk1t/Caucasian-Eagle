// Written from the equipment manifest of the modular weapons (art/nyxara/equipment/equipment_manifest.json) by
// art/nyxara/equipment/gen_equipment.py. Do not edit the numbers: change the manifest or that script and write this again.
// Each weapon is its own mesh with its grip at the origin and its blade or limbs along +Y. HandL and HandR place it
// in a hand: a row-major matrix in the frame of the rig's Hand_L or Hand_R joint (the engine multiplies it by the
// joint's sampled matrix and by the character's world matrix: DoorwaysCharacterSocket.h). The rig's right hand is
// the one the player sees as her left (the engine shows the model mirrored).
#pragma once

namespace CaucasianEagle
{
    struct EquipmentItem
    {
        const char* Name;            // folder and file name under Characters/Nyxara/Equipment
        float HandL[16], HandR[16];  // in the joint's frame
        float BackAt[3], BackTurn[3]; // on her back: metres in her own frame at rest, and the turn about x, y, z (rad); carried by the Chest joint
        const char* StringMaterial;  // a bow's straight string (hidden while she draws), or null
        float StringTop[3], StringBottom[3], Nock[3];   // a bow: the ends of its string and where the arrow rests, in the weapon's frame
    };
    constexpr int EquipmentCount = 3;
    constexpr EquipmentItem Equipment[EquipmentCount] = {
        {"InfernoAxe",
         {-2.4586916e-07f, 1.00000036f, 0.f, 0.f, 0.389310777f, 7.07805157e-08f, 0.921106696f, 0.f, 0.921106935f, 1.47739172e-07f, -0.389310628f, 0.f, -0.0149285123f, 0.0887192786f, 0.00179906376f, 1.f},
         {-3.31550837e-07f, 1.00000083f, 1.71363354e-07f, 0.f, -0.504374623f, -2.36555934e-07f, 0.863485456f, 0.f, 0.863485277f, 2.08152272e-07f, 0.504374743f, 0.f, 0.00733792968f, 0.0841708556f, 0.00817924738f, 1.f},
         {0.2f, 0.78f, -0.15f}, {0.f, 0.f, 0.4f},
         nullptr, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {"SerpentcoilAxe",
         {-2.4586916e-07f, 1.00000036f, 0.f, 0.f, 0.389310777f, 7.07805157e-08f, 0.921106696f, 0.f, 0.921106935f, 1.47739172e-07f, -0.389310628f, 0.f, -0.0149285123f, 0.0887192786f, 0.00179906376f, 1.f},
         {-3.31550837e-07f, 1.00000083f, 1.71363354e-07f, 0.f, -0.504374623f, -2.36555934e-07f, 0.863485456f, 0.f, 0.863485277f, 2.08152272e-07f, 0.504374743f, 0.f, 0.00733792968f, 0.0841708556f, 0.00817924738f, 1.f},
         {-0.2f, 0.78f, -0.15f}, {0.f, 0.f, -0.4f},
         nullptr, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {"EldritchLongbow",
         {-0.921106815f, -1.47739172e-07f, 0.389310569f, 0.f, 0.389310777f, 7.07805157e-08f, 0.921106696f, 0.f, -2.16066837e-07f, 1.00000036f, -1.49011612e-08f, 0.f, -0.0149285123f, 0.0887192786f, 0.00179906376f, 1.f},
         {-0.863485277f, -2.08152272e-07f, -0.504374743f, 0.f, -0.504374623f, -2.36555934e-07f, 0.863485456f, 0.f, -3.31550837e-07f, 1.00000083f, 1.71363354e-07f, 0.f, 0.00733792968f, 0.0841708556f, 0.00817924738f, 1.f},
         {0.f, 1.2f, -0.17f}, {0.f, 0.f, 0.3f},
         "BowCord", {0.f, 0.504208311f, 0.132798527f}, {0.f, -0.340873225f, 0.196179642f}, {0.f, 0.f, 0.17061415f}},
    };
    enum EquipmentIndex { EquipInferno = 0, EquipSerpentcoil = 1, EquipBow = 2 };
    constexpr const char* EquipmentBackJoint = "Chest";
    constexpr const char* EquipmentBowHand = "Hand_R";    // the hand the player sees as her left
    constexpr const char* EquipmentStringHand = "Hand_L";
}
