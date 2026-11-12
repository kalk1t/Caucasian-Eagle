#pragma once

// Skeletons for the creatures of the road.
//
// The creature models are generated meshes without bones. art/monsters/rig_models.py sorts the vertices of
// each model into body parts, writes the joint weights into the .dmesh and writes <Name>.rig with the pivot
// of every joint. This file reads the .rig and, each frame, turns the state of a creature (walking, winding
// up, striking, hit, dying) into the joint matrices that the engine's skinned draw takes
// (Renderer::DrawDoorwaysGpuMeshSkinned). There are no animation files: every movement is computed here.
//
// The joint numbers are fixed and must match rig_models.py.

#include "DoorwaysGpuSkinningTypes.h"

#include <DirectXMath.h>
#include <filesystem>
#include <string>

namespace CaucasianEagle
{
    struct CreatureRig
    {
        enum Joint
        {
            Pelvis, Chest, Head, ArmL, ForeL, ArmR, ForeR, LegL, ShinL, LegR, ShinR,   // two-legged creatures
            Skirt1, Skirt2,                                                             // robed spirits: the hanging cloth
            Spine2, Spine3,                                                             // the serpent's neck, between Chest and Head
            WingL, WingL2, WingR, WingR2,                                               // the eagle
            JointCount
        };
        enum class Type { None, Biped, Robed, Serpent, Winged };
        enum class Attack { Swing, Slam, Cast };   // one arm from above; both arms from above; both hands thrust forward

        Type Kind = Type::None;                  // None: the model has no .rig and is drawn rigid
        int Parent[JointCount]{};
        int Order[JointCount]{};                 // joints with every parent before its children
        DirectX::XMFLOAT3 Pivot[JointCount]{};   // mesh units

        // A missing file is not an error (the rig stays None). A file that cannot be read is.
        bool Load(const std::filesystem::path& path, std::string& error);
    };

    struct CreaturePose
    {
        float Walk = 0;      // 0 standing .. 1 at full speed
        float Stride = 0;    // radians; one step of each leg per 2 pi
        float Time = 0;      // seconds, for breathing and swaying
        float Windup = 0;    // 0 .. 1 while the blow is drawn back
        float Strike = 0;    // 1 at the moment of the blow, falling to 0
        float Recoil = 0;    // 1 when just hit, falling to 0
        float Death = 0;     // 0 .. 1 while falling
        CreatureRig::Attack Attack = CreatureRig::Attack::Swing;
    };

    void BuildCreaturePalette(const CreatureRig& rig, const CreaturePose& pose, Doorways::DoorwaysGpuSkinningPalette& out);
}
