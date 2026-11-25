#pragma once

// Skeletons for the creatures of the road.
//
// The creature models are generated meshes without bones. art/monsters/rig_models.py sorts the vertices of
// each model into body parts, writes the joint weights into the .dmesh and writes <Name>.rig with the pivot
// of every joint. This file reads the .rig and, each frame, turns the state of a creature (walking, winding
// up, striking, dodging, hit, dying) into the joint matrices that the engine's skinned draw takes
// (Renderer::DrawDoorwaysGpuMeshSkinned). There are no animation files: every movement is computed here.
//
// 2026-11-20 (the owner: "update each monster combat skills and movements animations, attacking skills and dodging
// skills for those twelve monsters"): every kind walks its own way (CreatureGait), its legs step as far as it goes
// (its owner turns the distance into Stride), it stands in a fighting stance near her (Ready), steps round on the
// spot as it turns (Turn), has blows of its own (Attack, either side: Mirror) and a dodge of its own (Dodge), and a
// two-legged creature keeps its feet on the ground however its legs bend (BuildCreaturePalette's Drop).
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
        // Its blows. Two-legged and robed: Swing one arm from above; Slam both arms from above; Cast both hands thrust
        // forward; Sweep one arm out to its side and across in a wide flat arc; Charge head and shoulders down in a
        // rush; Pounce a crouch and a leap, claws first; Pound both fists into the ground before it; Stamp one foot
        // raised high and stamped down; Throw one arm back over its shoulder and through; Curse both hands raised over
        // its head and struck down; Shriek arms flung wide, head thrown back; Breath chest and head thrust forward;
        // Roar arms crossed before it, then flung up and wide. The serpent: Bite it draws back and whips forward;
        // Spray its neck sweeps from side to side; Rear it rears up high and crashes down. The eagle: Gust its wings
        // thrown forward; Swoop wings swept back, as it comes down on her.
        enum class Attack { Swing, Slam, Cast, Sweep, Charge, Pounce, Pound, Stamp, Throw, Curse, Shriek, Breath, Roar, Bite, Spray, Rear, Gust, Swoop };
        // Its dodges (to its right; Mirror for its left; EvadeSide 0 is back): SideHop a crouched leap aside, knees
        // tucked; BackHop a hop back, arms forward; Glide (robed) a fast drift aside, the cloth streaming; FlameStep
        // crouched into fire and up out of it (its owner hides it between); SideStep a long lunge aside, ducking;
        // HeavyStep (giants) a heavy step back or aside, an arm up before its face; Sway (the serpent) its neck pulled
        // back and aside; AirStrafe (the eagle) a hard beat of its wings, banking.
        enum class Dodge { None, SideHop, BackHop, Glide, FlameStep, SideStep, HeavyStep, Sway, AirStrafe };

        Type Kind = Type::None;                  // None: the model has no .rig and is drawn rigid
        int Parent[JointCount]{};
        int Order[JointCount]{};                 // joints with every parent before its children
        DirectX::XMFLOAT3 Pivot[JointCount]{};   // mesh units
        float Sole = 0;                          // mesh units: the height of its soles (its owner sets it: the model's lowest point)

        // A missing file is not an error (the rig stays None). A file that cannot be read is.
        bool Load(const std::filesystem::path& path, std::string& error);
    };

    // How a kind carries itself and goes (all as given: as every creature went before 2026-11-20).
    struct CreatureGait
    {
        float Hunch = 0;     // rad its chest leans forward (its head still looks ahead, its arms still hang)
        float Swing = 1;     // its arms' swing as it goes, times the usual
        float Low = 0;       // 0..1 its arms hang long and loose and swing wide (an ape's)
        float Out = 0;       // rad its arms are held out from its sides
        float Lift = 1;      // its knees' lift as it goes, times the usual
        float Sway = 0;      // rad its hips and shoulders roll from side to side with its steps (the heavy)
        float Limp = 0;      // 0..1 its left leg is weak: a short, stiff step and a dip of the head onto it (a hobble)
        float Prowl = 0;     // 0..1 crouched, its knees bent (hunters)
        float Slither = 0;   // the serpent: how far its neck winds as it goes
    };

    struct CreaturePose
    {
        float Walk = 0;      // 0 standing .. 1 at full speed
        float Stride = 0;    // radians; one step of each leg per 2 pi
        float Heading = 0;   // which way it goes, from where it faces (radians: 0 ahead, pi/2 to its right, pi back): its
                             // legs swing that way
        float Time = 0;      // seconds, for breathing and swaying
        float Windup = 0;    // 0 .. 1 while the blow is drawn back (held at 1 through its Action)
        float Strike = 0;    // 1 at the moment of the blow, falling to 0
        float Action = 0;    // 0 .. 1 through what comes between the wind-up and the blow (a charge's rush, a pounce's flight)
        float Recoil = 0;    // 1 when just hit, falling to 0
        float Death = 0;     // 0 .. 1 while falling
        float Daze = 0;      // 0 .. 1 dazed (doomed: its head hangs, it sways)
        float Kneel = 0;     // 0 .. 1 down on its knees (its owner lowers it: NyxaraKit::Wraith::KneelDrop)
        float Ready = 0;     // 0 at ease .. 1 in its fighting stance (awake and near her)
        float Turn = 0;      // -1 .. 1 it steps round on the spot, to its left (-) or right (+)
        float Evade = 0;     // 0 .. 1 through a dodge
        float EvadeSide = 1; // which way: 1 its right, -1 its left, 0 back
        float Air = 0;       // 0 .. 1 off the ground (a hop, a leap): its feet are not kept on the ground
        float Run = 0;       // 0 .. 1 running, faster than it walks: it leans into it, its knees come up, its arms pump
        float Perch = 0;     // 0 .. 1 (the eagle) down on the ground, its wings folded and still
        bool Mirror = false; // the blow with its other side (its left arm, its left foot)
        CreatureRig::Attack Attack = CreatureRig::Attack::Swing;   // the blow wound up (Windup, Action)
        // the blow landing (Strike), and with which side: as a rule the one wound up, but a quick creature winds up its
        // second blow with its other arm while its first is still coming down
        CreatureRig::Attack Struck = CreatureRig::Attack::Swing;
        bool StruckMirror = false;
        CreatureRig::Dodge Dodge = CreatureRig::Dodge::None;
        CreatureGait Gait;
    };

    // 'drop', when given: how far (mesh units) the model was let down so that its lower foot stands on the ground
    // (two-legged creatures; none while dazed, kneeling or dying: KneelDrop and the dying's sinking have those).
    void BuildCreaturePalette(const CreatureRig& rig, const CreaturePose& pose, Doorways::DoorwaysGpuSkinningPalette& out, float* drop = nullptr);
}
