#pragma once

// Her flight made by code, in ways of its own (2026-11-27). The owner: "nowi want you to work on flying itself and get me
// at least 9 different flying styles", "flying style i meant whole sequence of flying up and itself flying pose and all it
// includes", "just take dont follow those three base motions and get me the best different results and then i chose which
// i like most and then we refine it". Each style is her whole flight, posed here from her skeleton and not from her three
// flight clips (Fly_Rise, Fly, Fly_Land): her take-off, her body and wings as she hovers, flies, speeds (Shift), climbs,
// dives and turns, and her landing. Style 0 is her flight as made (the clips). The kit asks for a pose every frame of a
// flight (Poser::Step) and plays it in place of the three clips (NyxaraKit.cpp: Body::Sample); her swords are on her back.
//
// How she is posed. Every bone is set by the way it points and how it is turned about itself:
// - her wings: each bone's way (up from level; round from straight out to her side, forward + or back -) and its
//   feathers' turn about it (0: trailing back from it, flat to the air; -90: hanging below it, as her wings hang folded
//   and in her clips, whose wings never turn about themselves). The feathers' plane on each bone was measured on her mesh
//   (art/nyxara/blender/Nyxara_game.blend, 2026-11-27: the vertices each wing bone carries most, and their spread): Trail.
// - her arms: the upper arm's way, the way her elbow folds, the elbow's angle, the wrist's;
// - her legs: the thigh's way, the knee's and ankle's angles, the knee turned out; on the ground (landing) her feet put
//   where they stand and her knees found between (a two-bone reach);
// - her back bowed (+) or arched (-), her hips turned under her, her head lifted; her whole body leaned (her picture, as
//   the kit's FlyLean was), banked into her turns about the way she flies and spun about her length (her hips).
// Ways are given for her left side (her right is its mirror): her arms' in her chest's frame and her legs' in her hips',
// as she stands at bind (x her left, y up, z forward); her wings' in the air's frame (y the sky's up, z the way she flies,
// level; her bank and spin kept, her lean not), but for her wings folded (Fold: her chest's frame, as made) and tucked
// (Tuck: the air's frame as she lies level, so that they lie along her however she leans).

#include "DoorwaysAnimationPose.h"
#include "DoorwaysSkeleton.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

namespace Doorways::NyxaraFlight
{
    // ---- what a style is made of (degrees, metres, seconds)
    struct Aim
    {
        float Up = 0, Round = 0;   // up from level; round from straight out to her side, forward (+) or back (-)
    };
    struct ArmKey
    {
        Aim Arm{-78.f, 5.f};    // her upper arm's way
        Aim Fold{0.f, 90.f};    // the way her elbow folds her forearm
        float Elbow = 12;       // its angle (0 straight)
        float Wrist = 0;        // her hand bent (+) toward her palm
    };
    struct LegKey
    {
        Aim Thigh{-88.f, -90.f};   // her thigh's way
        float Knee = 6;            // bent (0 straight)
        float Ankle = 20;          // her toes pointed (+) from her foot as made
        float Out = 0;             // her knee turned out
    };
    struct BodyKey
    {
        float Pitch = 0;    // her whole body leaned forward (+) from upright
        float Bow = 0;      // her back bowed forward (+) or arched (-), over her three spine bones
        float Pelvis = 0;   // her hips turned forward (+, her legs back) under her body
        float Look = 0;     // her head lifted (+) from her chest's line
        float Lift = 0;     // m her body raised in her picture
        ArmKey Arm[2]{};    // left, right
        LegKey Leg[2]{};
    };
    struct WingKey
    {
        Aim Bone[3]{};              // her left wing's three bones, root to tip
        float Feather[3]{};         // the feathers' turn about each
    };
    struct BeatKey
    {
        float Rate = 1;     // beats a second
        float Amp = 1;      // how far toward the up and down keys (0 still)
        float Down = .5f;   // the share of a beat that is the down stroke
        float Lag = .1f;    // how late each bone follows the one before (a share of a beat)
        float Fold = .2f;   // how far the wing folds in on the up stroke
        float Bob = .08f;   // m her body rides up as the wings come down
        float Rock = 0;     // degrees her body rocks with the beat
    };
    constexpr ArmKey Arm(float up, float round, float elbow, float foldUp = 0, float foldRound = 90, float wrist = 0)
    {
        return {{up, round}, {foldUp, foldRound}, elbow, wrist};
    }
    constexpr LegKey Leg(float up, float round, float knee, float ankle, float out = 0) { return {{up, round}, knee, ankle, out}; }
    constexpr BodyKey Body(float pitch, float bow, float pelvis, float look, float lift, ArmKey arm, LegKey leg)
    {
        return {pitch, bow, pelvis, look, lift, {arm, arm}, {leg, leg}};
    }
    constexpr BodyKey Body(float pitch, float bow, float pelvis, float look, float lift, ArmKey left, ArmKey right, LegKey legLeft, LegKey legRight)
    {
        return {pitch, bow, pelvis, look, lift, {left, right}, {legLeft, legRight}};
    }
    constexpr WingKey Wing(float u1, float r1, float f1, float u2, float r2, float f2, float u3, float r3, float f3)
    {
        return {{{u1, r1}, {u2, r2}, {u3, r3}}, {f1, f2, f3}};
    }
    constexpr BeatKey Beat(float rate, float amp, float down, float lag, float fold, float bob, float rock = 0)
    {
        return {rate, amp, down, lag, fold, bob, rock};
    }
    // her wings folded as they were made (her bind pose; her clips' Stand): the way each bone points and its feathers
    inline constexpr WingKey Folded = Wing(35, -18, -71, -44, -19, -106, -51, -15, -96);

    struct StyleSpec
    {
        const char* Name = "";
        const char* Line = "";
        BodyKey Hover{}, Cruise{}, Fast{}, Climb{}, Dive{}, Launch{}, Reach{}, Impact{};
        WingKey Fold = Folded, Glide{}, Up{}, Down{}, HoverUp{}, HoverDown{}, Brake{}, Tuck{}, Flare{};
        BeatKey HoverBeat{}, CruiseBeat{}, FastBeat{}, RiseBeat{}, BrakeBeat{};
        float Glides = 0, GlideSeconds = 0;        // share of her cruising on still wings; how long each glide
        float FastTuck = 0, DiveTuck = 0;          // her wings drawn back (Tuck) at full speed (Shift), and diving
        float Bank = 14, BankMost = 35, Snap = 5;  // degrees of bank a radian a second of turning; the most; how quickly her body answers (1/s)
        float RiseSeconds = 1, LandSeconds = 1;    // her take-off (from the lift), her landing (from her touch)
        float Crouch = .25f;                       // m her hips go down as she lands
        float Stance[2][2] = {{.16f, .06f}, {.16f, -.06f}};   // where her ankles come down (out, forward), left and right
        float Kneel[2] = {0, 0};                   // a knee down (1) as she lands
        float Plant[2] = {1, 1};                   // a foot put down at once (0: the Impact key's leg until she stands)
        float Spin = 0;                            // turns about her length as she rises
        float Wave = 0, WaveRate = 0;              // her body's wave (degrees), waves a second
        float Loose = 0;                           // her arms and legs swinging free (0 held, 1 hanging)
        float Sway = 0, SwayRate = 0;              // her body rocked side to side (degrees, a second)
        float Rolls = 0;                           // seconds of cruising between her barrel rolls (0: none)
        float Steady = 0;                          // her head held still against her body's rock
        float Thud = 0;                            // the camera's shake as she lands
        float Alternate = 0;                       // her right wing behind her left (a share of a beat)
        float FlareHold = .25f;                    // seconds her wings stay up as she lands, before they fold
    };

    // The styles. 0: her clips. The others are original ways of flying, each a whole flight.
    inline const StyleSpec Styles[] = {
        {.Name = "AS MADE", .Line = "her three flight clips: Fly_Rise, Fly and Fly_Land, her swords in her hands"},

        // 1. EAGLE: a raptor's flight. Deep slow beats and long soaring glides; her body long and level, her chest lifted, her
        // legs trailing; two great beats to rise; she lands as a bird of prey does, wings cupped forward to brake, feet reaching.
        {.Name = "EAGLE",
         .Line = "deep slow beats and long glides, body level, legs trailing; two great beats to rise; lands braking, wings cupped",
         .Hover = Body(24, 8, 0, 18, 0, Arm(-62, 30, 40), Leg(-80, 90, 30, 25)),
         .Cruise = Body(72, -8, 4, 52, 0, Arm(-80, -40, 14), Leg(-87, -90, 10, 50)),
         .Fast = Body(82, -10, 4, 62, 0, Arm(-85, -60, 8), Leg(-89, -90, 4, 60)),
         .Climb = Body(42, 4, 0, 30, 0, Arm(-70, -20, 25), Leg(-84, -90, 18, 40)),
         .Dive = Body(100, -6, 0, 75, 0, Arm(-82, -55, 10), Leg(-88, -90, 6, 60)),
         .Launch = Body(14, 18, 0, 10, 0, Arm(-55, -30, 35), Leg(-35, 90, 95, 30)),
         .Reach = Body(-6, 6, -8, 12, 0, Arm(-40, 35, 30), Leg(-62, 90, 22, 5)),
         .Impact = Body(10, 26, 0, 6, 0, Arm(-55, 50, 40), Leg(-60, 90, 80, 10)),
         .Glide = Wing(14, -6, -45, 8, -12, -30, 12, -20, -25),
         .Up = Wing(62, -22, -80, 52, -32, -95, 40, -40, -100),
         .Down = Wing(-22, 12, -25, -38, 18, -20, -48, 22, -15),
         .HoverUp = Wing(50, -55, -95, 40, -70, -110, 30, -80, -120),
         .HoverDown = Wing(10, 35, -30, 0, 50, -20, -10, 60, -15),
         .Brake = Wing(40, 30, -95, 35, 45, -100, 25, 60, -105),
         .Tuck = Wing(20, -45, -40, 0, -65, -30, -5, -80, -25),
         .Flare = Wing(70, -10, -85, 65, -5, -95, 55, 0, -100),
         .HoverBeat = Beat(1.7f, 1, .5f, .12f, .25f, .1f, 3),
         .CruiseBeat = Beat(1.1f, .9f, .55f, .12f, .35f, .12f, 3),
         .FastBeat = Beat(1.5f, .7f, .5f, .1f, .3f, .08f, 2),
         .RiseBeat = Beat(2.2f, 1, .45f, .1f, .3f, .2f, 6),
         .BrakeBeat = Beat(2.6f, .35f, .5f, .08f, .1f, .04f, 2),
         .Glides = .5f, .GlideSeconds = 2.4f, .FastTuck = .35f, .DiveTuck = .7f,
         .Bank = 16, .BankMost = 45, .Snap = 4,
         .RiseSeconds = 1.f, .LandSeconds = 1.f, .Crouch = .28f,
         .Stance = {{.17f, .12f}, {.17f, -.1f}},
         .Steady = .7f, .Thud = .15f, .FlareHold = .3f},

        // 2. ARROW: a living arrow. Straight up out of the ground with one beat, her fists ahead of her in flight, her body
        // one straight line, her wings swept back to a narrow point and folded away at speed; she lands on one knee and a fist.
        {.Name = "ARROW",
         .Line = "fists ahead, body one straight line, wings swept to a point, folded away at speed; up in one beat; lands on a knee and a fist",
         .Hover = Body(6, 0, 0, 4, 0, Arm(-84, 2, 6), Leg(-89, -90, 3, 55)),
         .Cruise = Body(86, -4, 0, 70, 0, Arm(82, 18, 6, 0, 90), Leg(-89, -90, 2, 62)),
         .Fast = Body(89, -2, 0, 76, 0, Arm(86, 6, 3, 0, 90), Leg(-90, -90, 0, 65)),
         .Climb = Body(8, 0, 0, 10, 0, Arm(86, 10, 4, 0, 90), Leg(-90, -90, 0, 60)),
         .Dive = Body(125, -4, 0, 80, 0, Arm(86, 10, 4, 0, 90), Leg(-90, -90, 0, 65)),
         .Launch = Body(0, -4, 0, 18, 0, Arm(84, 12, 10, 0, 90), Leg(-90, -90, 4, 55)),
         .Reach = Body(6, 4, 0, 4, 0, Arm(-30, 20, 25), Leg(-80, 90, 12, 25)),
         .Impact = Body(18, 32, 0, 22, 0, Arm(-25, -70, 20), Arm(-72, 60, 8), Leg(-20, 90, 100, 10), Leg(-84, -90, 95, 40)),
         .Glide = Wing(18, -40, -20, 10, -60, -10, 4, -75, -5),
         .Up = Wing(55, -45, -70, 40, -60, -80, 30, -70, -85),
         .Down = Wing(-5, -15, -20, -15, -25, -15, -20, -35, -10),
         .HoverUp = Wing(50, -40, -90, 45, -55, -100, 40, -65, -105),
         .HoverDown = Wing(10, 20, -40, 0, 25, -30, -5, 30, -25),
         .Brake = Wing(35, 20, -90, 30, 35, -95, 20, 45, -100),
         .Tuck = Wing(12, -70, -25, -5, -100, -20, -8, -110, -15),
         .Flare = Wing(75, -20, -85, 70, -25, -95, 65, -30, -100),
         .HoverBeat = Beat(2.6f, .55f, .45f, .08f, .2f, .04f, 1),
         .CruiseBeat = Beat(2.2f, .45f, .45f, .08f, .2f, .03f, 1),
         .FastBeat = Beat(2.4f, .1f, .45f, .06f, 0, .01f, 0),
         .RiseBeat = Beat(1.6f, 1, .3f, .06f, .5f, .1f, 2),
         .BrakeBeat = Beat(3.f, .3f, .5f, .06f, .1f, .03f, 1),
         .Glides = .55f, .GlideSeconds = 1.6f, .FastTuck = 1, .DiveTuck = 1,
         .Bank = 10, .BankMost = 30, .Snap = 7,
         .RiseSeconds = .8f, .LandSeconds = 1.25f, .Crouch = .5f,
         .Stance = {{.2f, .34f}, {.13f, -.42f}}, .Kneel = {0, 1},
         .Thud = .4f, .FlareHold = .35f},

        // 3. SERAPH: lifted, not flying. Upright, her arms open at her sides, her legs together and her toes pointed;
        // grand slow beats whose tips meet over her head, and she rides each one; she comes down on her toes.
        {.Name = "SERAPH",
         .Line = "upright, arms open, toes pointed; grand slow beats meeting over her head, riding each one; down on her toes",
         .Hover = Body(4, -6, 0, 8, 0, Arm(-45, 20, 22), Arm(-45, 20, 22), Leg(-88, 90, 14, 62), Leg(-86, -90, 26, 62)),
         .Cruise = Body(28, -8, 0, 22, 0, Arm(-30, 0, 18), Arm(-30, 0, 18), Leg(-84, -90, 12, 62), Leg(-80, -90, 24, 62)),
         .Fast = Body(48, -10, 0, 36, 0, Arm(-38, -38, 14), Leg(-82, -90, 10, 65)),
         .Climb = Body(0, -10, 0, 22, 0, Arm(-20, 10, 20), Leg(-88, -90, 10, 65)),
         .Dive = Body(55, 4, 0, 30, 0, Arm(-40, -30, 18), Leg(-82, -90, 14, 65)),
         .Launch = Body(0, -12, 0, 24, 0, Arm(-15, 10, 25), Leg(-89, -90, 8, 60)),
         .Reach = Body(0, -4, 0, 4, 0, Arm(-40, 15, 22), Leg(-89, -90, 3, 68)),
         .Impact = Body(0, 2, 0, 0, 0, Arm(-55, 15, 22), Leg(-88, 90, 10, 20)),
         .Glide = Wing(30, -8, -80, 25, 0, -85, 18, 8, -90),
         .Up = Wing(78, -15, -95, 72, 0, -100, 68, 15, -105),
         .Down = Wing(-8, 0, -60, -28, 8, -55, -42, 12, -50),
         .HoverUp = Wing(75, -25, -95, 70, -15, -100, 65, -5, -105),
         .HoverDown = Wing(0, 10, -65, -20, 18, -60, -35, 22, -55),
         .Brake = Wing(45, 20, -95, 40, 35, -100, 30, 45, -105),
         .Tuck = Wing(25, -35, -80, 5, -50, -85, -5, -60, -90),
         .Flare = Wing(60, 0, -90, 55, 10, -95, 48, 20, -100),
         .HoverBeat = Beat(.75f, 1, .5f, .18f, .15f, .2f, 2),
         .CruiseBeat = Beat(.7f, 1, .5f, .18f, .15f, .18f, 2),
         .FastBeat = Beat(.95f, .9f, .5f, .15f, .15f, .14f, 2),
         .RiseBeat = Beat(.8f, 1, .5f, .2f, .1f, .3f, 2),
         .BrakeBeat = Beat(1.1f, .5f, .5f, .15f, .1f, .08f, 1),
         .Bank = 8, .BankMost = 20, .Snap = 2.5f,
         .RiseSeconds = 1.4f, .LandSeconds = 1.2f, .Crouch = .06f,
         .Stance = {{.11f, .02f}, {.11f, -.02f}},
         .Steady = .4f, .FlareHold = .45f},

        // 4. DART: a hummingbird's. Her wings a shimmer high in a V, her legs drawn up, her hands at her chest; she darts and
        // stops at once, leaned into it; she is up in a blink and down light.
        {.Name = "DART",
         .Line = "a shimmer of quick small beats high in a V, legs drawn up, hands at her chest; darts and stops at once",
         .Hover = Body(34, 10, 0, 32, 0, Arm(-62, 40, 118, 30, 90), Leg(-30, 90, 112, 40)),
         .Cruise = Body(56, 6, 0, 46, 0, Arm(-64, 38, 120, 30, 90), Leg(-38, 90, 110, 45)),
         .Fast = Body(74, 2, 0, 60, 0, Arm(-70, 30, 115, 30, 90), Leg(-50, 90, 100, 50)),
         .Climb = Body(18, 6, 0, 30, 0, Arm(-62, 40, 118, 30, 90), Leg(-28, 90, 115, 40)),
         .Dive = Body(80, 4, 0, 62, 0, Arm(-70, 30, 115, 30, 90), Leg(-45, 90, 105, 50)),
         .Launch = Body(24, 8, 0, 26, 0, Arm(-62, 40, 118, 30, 90), Leg(-30, 90, 112, 40)),
         .Reach = Body(10, 6, 0, 14, 0, Arm(-62, 35, 100, 30, 90), Leg(-86, 90, 18, 30)),
         .Impact = Body(8, 12, 0, 8, 0, Arm(-65, 30, 80, 30, 90), Leg(-80, 90, 30, 15)),
         .Glide = Wing(45, -15, -50, 48, -20, -55, 50, -25, -60),
         .Up = Wing(60, -35, -90, 62, -45, -100, 64, -50, -110),
         .Down = Wing(30, 15, -20, 28, 25, -15, 25, 35, -10),
         .HoverUp = Wing(50, -45, -100, 50, -55, -115, 50, -60, -125),
         .HoverDown = Wing(35, 30, -15, 35, 40, -10, 35, 48, -5),
         .Brake = Wing(40, 10, -80, 40, 20, -85, 40, 30, -90),
         .Tuck = Wing(30, -50, -50, 20, -70, -40, 15, -80, -35),
         .Flare = Wing(60, -10, -80, 62, -15, -90, 65, -20, -95),
         .HoverBeat = Beat(5.2f, .7f, .5f, .04f, 0, .025f, 1),
         .CruiseBeat = Beat(4.8f, .65f, .5f, .04f, 0, .025f, 1),
         .FastBeat = Beat(5.6f, .55f, .5f, .04f, 0, .02f, 1),
         .RiseBeat = Beat(5.6f, .8f, .5f, .04f, 0, .035f, 1),
         .BrakeBeat = Beat(5.2f, .55f, .5f, .04f, 0, .025f, 1),
         .FastTuck = .2f, .DiveTuck = .3f,
         .Bank = 12, .BankMost = 40, .Snap = 11,
         .RiseSeconds = .5f, .LandSeconds = .7f, .Crouch = .12f,
         .Stance = {{.15f, .03f}, {.15f, -.02f}},
         .Steady = .9f, .FlareHold = .1f},

        // 5. SERPENT: she swims the sky. A wave runs down her whole body to her feet, kicking together; her arms along her
        // sides, her wings rowing with the wave; she spirals up and rolls into her turns; she lands low on a knee and a hand.
        {.Name = "SERPENT",
         .Line = "she swims: a wave down her body to her feet, arms along her sides, wings rowing with it; spirals up; lands low",
         .Hover = Body(22, 0, 0, 20, 0, Arm(-55, 20, 30), Leg(-84, -90, 20, 50)),
         .Cruise = Body(88, -4, 0, 70, 0, Arm(-88, -20, 4), Leg(-89, -90, 6, 62)),
         .Fast = Body(90, -4, 0, 74, 0, Arm(-89, -12, 2), Leg(-90, -90, 3, 66)),
         .Climb = Body(30, -4, 0, 28, 0, Arm(-85, -20, 6), Leg(-88, -90, 8, 60)),
         .Dive = Body(110, -4, 0, 76, 0, Arm(-88, -15, 4), Leg(-90, -90, 4, 66)),
         .Launch = Body(20, -10, 0, 30, 0, Arm(-86, -15, 4), Leg(-90, -90, 2, 60)),
         .Reach = Body(-10, 8, -10, 14, 0, Arm(-30, 0, 25), Leg(-55, 90, 30, 20)),
         .Impact = Body(28, 30, 0, 26, 0, Arm(-82, 15, 10), Arm(-35, -40, 30), Leg(-35, 90, 95, 10), Leg(-86, -90, 95, 40)),
         .Glide = Wing(10, -30, -30, 0, -45, -20, -5, -55, -15),
         .Up = Wing(40, -50, -60, 30, -65, -75, 20, -75, -85),
         .Down = Wing(-15, -5, -25, -25, -15, -20, -30, -25, -15),
         .HoverUp = Wing(35, -55, -80, 25, -70, -95, 15, -80, -105),
         .HoverDown = Wing(0, 15, -30, -10, 20, -25, -15, 25, -20),
         .Brake = Wing(35, 25, -90, 25, 40, -95, 15, 50, -100),
         .Tuck = Wing(15, -65, -35, 0, -90, -25, -5, -100, -20),
         .Flare = Wing(55, 10, -85, 45, 25, -90, 35, 35, -95),
         .HoverBeat = Beat(.9f, .8f, .5f, .2f, .3f, .06f, 2),
         .CruiseBeat = Beat(.8f, .8f, .5f, .2f, .3f, .05f, 2),
         .FastBeat = Beat(1.2f, .6f, .5f, .2f, .25f, .03f, 1),
         .RiseBeat = Beat(1.2f, .9f, .5f, .2f, .3f, .08f, 2),
         .BrakeBeat = Beat(1.8f, .4f, .5f, .15f, .1f, .03f, 1),
         .FastTuck = .5f, .DiveTuck = .7f,
         .Bank = 28, .BankMost = 70, .Snap = 4,
         .RiseSeconds = 1.2f, .LandSeconds = 1.2f, .Crouch = .42f,
         .Stance = {{.2f, .28f}, {.12f, -.42f}}, .Kneel = {0, 1},
         .Spin = 1, .Wave = 16, .WaveRate = .8f, .Thud = .2f, .FlareHold = .3f},

        // 6. VALKYRIE: a warrior's flight. Her chest out, one fist leading and the other drawn back, her legs in a stride;
        // strong even beats; she bursts up with a knee driven high and comes down kneeling, her wings raised over her.
        {.Name = "VALKYRIE",
         .Line = "chest out, one fist leading, legs in a stride; strong even beats; a knee driven up to rise; lands kneeling, wings raised",
         .Hover = Body(14, -6, 0, 12, 0, Arm(-50, 30, 90, 40, 90), Arm(-50, 30, 90, 40, 90), Leg(-40, 90, 85, 35), Leg(-85, -90, 15, 45)),
         .Cruise = Body(55, -12, 0, 38, 0, Arm(-55, -70, 30), Arm(30, 70, 25, 0, 90), Leg(-55, 90, 70, 40), Leg(-70, -90, 35, 55)),
         .Fast = Body(74, -12, 0, 55, 0, Arm(-70, -70, 20), Arm(60, 40, 10, 0, 90), Leg(-70, 90, 45, 50), Leg(-80, -90, 20, 60)),
         .Climb = Body(20, -10, 0, 25, 0, Arm(-55, -60, 30), Arm(70, 30, 15, 0, 90), Leg(-45, 90, 80, 40), Leg(-80, -90, 25, 50)),
         .Dive = Body(95, -8, 0, 70, 0, Arm(-75, -60, 20), Arm(70, 40, 10, 0, 90), Leg(-75, 90, 35, 55), Leg(-85, -90, 15, 60)),
         .Launch = Body(10, 22, 0, 8, 0, Arm(-60, -40, 40), Arm(70, 30, 20, 0, 90), Leg(-25, 90, 100, 35), Leg(-86, -90, 20, 50)),
         .Reach = Body(-4, 0, 0, 10, 0, Arm(-35, 10, 35), Leg(-70, 90, 25, 15)),
         .Impact = Body(12, 18, 0, 22, 0, Arm(-20, 0, 25), Arm(-20, 0, 25), Leg(-20, 90, 100, 10), Leg(-84, -90, 95, 40)),
         .Glide = Wing(20, -12, -50, 14, -16, -40, 10, -22, -35),
         .Up = Wing(72, -30, -85, 60, -40, -95, 48, -48, -100),
         .Down = Wing(-28, 22, -25, -42, 28, -20, -52, 30, -15),
         .HoverUp = Wing(55, -55, -95, 45, -65, -105, 35, -75, -110),
         .HoverDown = Wing(5, 35, -30, -5, 50, -20, -15, 58, -15),
         .Brake = Wing(45, 25, -95, 40, 40, -100, 30, 55, -105),
         .Tuck = Wing(20, -50, -45, 5, -70, -35, 0, -85, -30),
         .Flare = Wing(78, -5, -85, 75, 5, -95, 70, 15, -100),
         .HoverBeat = Beat(1.6f, 1, .45f, .1f, .25f, .1f, 3),
         .CruiseBeat = Beat(1.45f, .9f, .45f, .1f, .25f, .1f, 3),
         .FastBeat = Beat(1.8f, .75f, .45f, .08f, .2f, .07f, 2),
         .RiseBeat = Beat(1.8f, 1, .4f, .08f, .3f, .22f, 5),
         .BrakeBeat = Beat(2.4f, .4f, .5f, .08f, .1f, .04f, 2),
         .FastTuck = .2f, .DiveTuck = .5f,
         .Bank = 14, .BankMost = 40, .Snap = 5,
         .RiseSeconds = .9f, .LandSeconds = 1.3f, .Crouch = .48f,
         .Stance = {{.2f, .3f}, {.13f, -.42f}}, .Kneel = {0, 1},
         .Steady = .5f, .Thud = .35f, .FlareHold = .45f},

        // 7. FALCON: a hunter's. Quick shallow beats and short glides, tucked small; at speed or diving her wings fold back
        // to a teardrop (the stoop); still in the air she hangs into the wind, wings wide and trembling, her head quite
        // still; she lands feet first, her wings mantled round her.
        {.Name = "FALCON",
         .Line = "quick shallow beats, tucked small; at speed her wings fold back to a teardrop; hangs in the wind; lands feet first",
         .Hover = Body(48, 6, 0, 42, 0, Arm(-62, 40, 95, 30, 90), Leg(-72, 90, 40, 20)),
         .Cruise = Body(78, 4, 0, 58, 0, Arm(-70, 50, 105, 30, 90), Leg(-80, -90, 30, 50)),
         .Fast = Body(90, 2, 0, 70, 0, Arm(-80, 30, 100, 30, 90), Leg(-89, -90, 6, 65)),
         .Climb = Body(45, 4, 0, 38, 0, Arm(-65, 45, 100, 30, 90), Leg(-70, -90, 40, 45)),
         .Dive = Body(120, 2, 0, 80, 0, Arm(-80, 30, 100, 30, 90), Leg(-89, -90, 6, 65)),
         .Launch = Body(40, 10, 0, 34, 0, Arm(-65, 45, 100, 30, 90), Leg(-55, 90, 70, 25)),
         .Reach = Body(-18, 4, -12, 18, 0, Arm(-45, 40, 60, 30, 90), Leg(-25, 90, 12, -10)),
         .Impact = Body(14, 22, 0, 24, 0, Arm(-55, 45, 70, 30, 90), Leg(-55, 90, 80, 10)),
         .Glide = Wing(6, -25, -20, 0, -35, -12, -4, -48, -8),
         .Up = Wing(40, -35, -60, 30, -45, -70, 20, -55, -80),
         .Down = Wing(-18, 0, -15, -25, -5, -10, -30, -15, -5),
         .HoverUp = Wing(32, -10, -40, 28, -12, -35, 24, -15, -30),
         .HoverDown = Wing(22, 0, -30, 18, 0, -25, 14, -2, -20),
         .Brake = Wing(40, 35, -95, 30, 55, -100, 20, 70, -105),
         .Tuck = Wing(15, -80, -30, -8, -112, -30, -10, -122, -25),
         .Flare = Wing(30, 40, -90, 5, 70, -95, -15, 85, -100),
         .HoverBeat = Beat(4.2f, .35f, .5f, .05f, 0, .015f, 0),
         .CruiseBeat = Beat(2.6f, .6f, .45f, .08f, .25f, .05f, 1),
         .FastBeat = Beat(2.8f, .2f, .45f, .06f, .1f, .01f, 0),
         .RiseBeat = Beat(3.4f, .8f, .45f, .07f, .25f, .08f, 2),
         .BrakeBeat = Beat(3.4f, .45f, .5f, .06f, .1f, .03f, 1),
         .Glides = .3f, .GlideSeconds = 1.1f, .FastTuck = 1, .DiveTuck = 1,
         .Bank = 18, .BankMost = 55, .Snap = 7,
         .RiseSeconds = .8f, .LandSeconds = 1.1f, .Crouch = .3f,
         .Stance = {{.18f, .16f}, {.18f, .04f}},
         .Steady = 1, .Thud = .2f, .FlareHold = .5f},

        // 8. WRAITH: she drifts as a ghost. Upright and bowed, her head low, her arms and legs hanging and swinging free;
        // her wings ripple slowly, not beat; she sways; she is drawn up by her chest and sinks down onto her toes.
        {.Name = "WRAITH",
         .Line = "upright and bowed, head low, arms and legs hanging free; wings ripple slowly; drawn up by her chest; sinks down",
         .Hover = Body(12, 20, 0, -24, 0, Arm(-80, -8, 16), Leg(-88, -90, 14, 60)),
         .Cruise = Body(34, 18, 0, -12, 0, Arm(-78, -30, 14), Leg(-84, -90, 18, 62)),
         .Fast = Body(55, 14, 0, 4, 0, Arm(-80, -50, 10), Leg(-82, -90, 14, 65)),
         .Climb = Body(4, 12, 0, -10, 0, Arm(-82, -15, 14), Leg(-89, -90, 10, 65)),
         .Dive = Body(45, 22, 0, -18, 0, Arm(-70, 10, 20), Leg(-80, -90, 20, 60)),
         .Launch = Body(-6, -20, 0, -42, 0, Arm(-70, -60, 20), Leg(-89, -90, 6, 66)),
         .Reach = Body(2, 14, 0, -20, 0, Arm(-82, -10, 14), Leg(-89, -90, 4, 70)),
         .Impact = Body(4, 16, 0, -14, 0, Arm(-82, -5, 16), Leg(-88, 90, 12, 25)),
         .Glide = Wing(25, -25, -95, 5, -30, -100, -10, -35, -105),
         .Up = Wing(45, -30, -100, 30, -40, -110, 15, -45, -115),
         .Down = Wing(5, -15, -80, -20, -20, -85, -40, -22, -90),
         .HoverUp = Wing(45, -30, -100, 30, -40, -110, 15, -45, -115),
         .HoverDown = Wing(5, -15, -80, -20, -20, -85, -40, -22, -90),
         .Brake = Wing(35, 0, -95, 20, 10, -100, 5, 15, -105),
         .Tuck = Wing(25, -45, -95, 0, -55, -100, -20, -60, -105),
         .Flare = Wing(50, -10, -100, 35, -5, -105, 20, 0, -110),
         .HoverBeat = Beat(.5f, .8f, .5f, .3f, 0, .1f, 0),
         .CruiseBeat = Beat(.5f, .8f, .5f, .3f, 0, .08f, 0),
         .FastBeat = Beat(.6f, .7f, .5f, .3f, 0, .06f, 0),
         .RiseBeat = Beat(.5f, .6f, .5f, .3f, 0, .05f, 0),
         .BrakeBeat = Beat(.6f, .5f, .5f, .3f, 0, .05f, 0),
         .Bank = 6, .BankMost = 18, .Snap = 2,
         .RiseSeconds = 1.5f, .LandSeconds = 1.4f, .Crouch = .05f,
         .Stance = {{.1f, .02f}, {.1f, -.02f}},
         .Loose = 1, .Sway = 6, .SwayRate = .25f, .FlareHold = .5f},

        // 9. ZEPHYR: an acrobat's. Her arms spread wide, one leg bent up behind her, her wings beating out of step with
        // each other; a barrel roll now and then as she cruises; a whole turn as she rises; she lands on one foot, the other
        // leg reaching back, and settles.
        {.Name = "ZEPHYR",
         .Line = "arms spread wide, one leg bent up behind, wings out of step; barrel rolls as she cruises; turns as she rises; lands on one foot",
         .Hover = Body(6, -6, 0, 10, 0, Arm(70, 30, 40), Arm(0, 30, 30), Leg(-86, -90, 8, 65), Leg(-84, 90, 14, 65)),
         .Cruise = Body(80, -15, 0, 48, 0, Arm(5, 20, 20), Arm(5, 20, 20), Leg(-75, -90, 80, 65), Leg(-88, -90, 4, 70)),
         .Fast = Body(86, -10, 0, 62, 0, Arm(-20, -60, 10), Leg(-89, -90, 4, 70)),
         .Climb = Body(15, -10, 0, 24, 0, Arm(60, 25, 30), Leg(-88, -90, 6, 68)),
         .Dive = Body(110, -10, 0, 70, 0, Arm(-20, -60, 10), Leg(-89, -90, 4, 70)),
         .Launch = Body(0, -6, 0, 16, 0, Arm(85, 10, 30, 0, 90), Leg(-89, -90, 4, 70)),
         .Reach = Body(0, -4, 0, 10, 0, Arm(10, 25, 20), Leg(-88, -90, 6, 60)),
         .Impact = Body(18, 8, 0, 18, 0, Arm(15, 30, 15), Arm(15, 30, 15), Leg(-88, 90, 30, 20), Leg(-30, -90, 10, 60)),
         .Glide = Wing(15, -15, -40, 10, -20, -30, 8, -28, -25),
         .Up = Wing(58, -30, -80, 48, -40, -90, 38, -48, -95),
         .Down = Wing(-15, 15, -25, -28, 20, -20, -38, 25, -15),
         .HoverUp = Wing(50, -45, -90, 40, -55, -100, 30, -62, -105),
         .HoverDown = Wing(8, 30, -30, 0, 40, -25, -8, 48, -20),
         .Brake = Wing(40, 25, -95, 35, 40, -100, 25, 52, -105),
         .Tuck = Wing(18, -55, -40, 5, -75, -30, 0, -88, -25),
         .Flare = Wing(65, -5, -85, 60, 5, -95, 55, 12, -100),
         .HoverBeat = Beat(1.3f, .8f, .5f, .12f, .25f, .08f, 2),
         .CruiseBeat = Beat(1.2f, .75f, .5f, .12f, .25f, .08f, 2),
         .FastBeat = Beat(1.6f, .5f, .5f, .1f, .2f, .05f, 1),
         .RiseBeat = Beat(1.6f, .8f, .5f, .1f, .3f, .1f, 2),
         .BrakeBeat = Beat(2.2f, .4f, .5f, .1f, .1f, .04f, 1),
         .FastTuck = .6f, .DiveTuck = .7f,
         .Bank = 18, .BankMost = 55, .Snap = 5,
         .RiseSeconds = 1.1f, .LandSeconds = 1.3f, .Crouch = .14f,
         .Stance = {{.08f, .02f}, {.16f, -.1f}}, .Plant = {1, 0},
         .Spin = 1, .Rolls = 4.5f, .Alternate = .5f, .FlareHold = .4f},

        // 10. TITAN: a heavy power. Huge slow beats that heave her whole body up and down, her legs swinging under her,
        // hunched over her fists; she rises in one great heave; she lands like a falling stone, wide and low, her wings
        // smashed down beside her.
        {.Name = "TITAN",
         .Line = "huge slow beats that heave her whole body, hunched over her fists, legs swinging; rises in a heave; lands like a stone",
         .Hover = Body(20, 16, 0, 22, 0, Arm(-45, 45, 105, 40, 90), Leg(-78, 90, 40, 30)),
         .Cruise = Body(60, 18, 0, 42, 0, Arm(-45, 45, 100, 40, 90), Leg(-50, 90, 90, 35)),
         .Fast = Body(72, 14, 0, 52, 0, Arm(-55, 30, 95, 40, 90), Leg(-62, 90, 75, 40)),
         .Climb = Body(30, 18, 0, 30, 0, Arm(-45, 45, 105, 40, 90), Leg(-55, 90, 85, 35)),
         .Dive = Body(90, 16, 0, 60, 0, Arm(-55, 30, 95, 40, 90), Leg(-65, 90, 70, 40)),
         .Launch = Body(16, 32, 0, 20, 0, Arm(-55, 45, 110, 40, 90), Leg(-30, 90, 115, 30)),
         .Reach = Body(4, 14, 0, 16, 0, Arm(-45, 40, 90, 40, 90), Leg(-82, 90, 30, 15, 15)),
         .Impact = Body(24, 30, 0, 30, 0, Arm(-70, 35, 30), Leg(-60, 90, 90, 10, 20)),
         .Glide = Wing(12, -5, -50, 5, -8, -45, 0, -12, -40),
         .Up = Wing(78, -20, -90, 68, -28, -100, 58, -35, -105),
         .Down = Wing(-35, 20, -30, -52, 25, -25, -62, 28, -20),
         .HoverUp = Wing(60, -45, -95, 50, -55, -105, 40, -62, -110),
         .HoverDown = Wing(-10, 30, -35, -25, 40, -30, -35, 45, -25),
         .Brake = Wing(45, 25, -95, 40, 40, -100, 30, 50, -105),
         .Tuck = Wing(20, -40, -50, 5, -55, -45, -5, -65, -40),
         .Flare = Wing(-30, 0, -60, -50, 10, -55, -60, 15, -50),
         .HoverBeat = Beat(1.f, 1, .4f, .1f, .3f, .3f, 8),
         .CruiseBeat = Beat(.9f, 1, .4f, .1f, .3f, .35f, 8),
         .FastBeat = Beat(1.2f, .9f, .4f, .1f, .3f, .25f, 6),
         .RiseBeat = Beat(1.1f, 1, .35f, .1f, .3f, .45f, 10),
         .BrakeBeat = Beat(1.6f, .5f, .45f, .08f, .15f, .08f, 3),
         .FastTuck = .1f, .DiveTuck = .3f,
         .Bank = 10, .BankMost = 30, .Snap = 3,
         .RiseSeconds = 1.f, .LandSeconds = 1.3f, .Crouch = .45f,
         .Stance = {{.3f, .1f}, {.3f, -.1f}},
         .Loose = .5f, .Thud = .6f, .FlareHold = .35f},
    };

    // ---- the poser: her flight, a frame at a time
    class Poser
    {
      public:
        static int Count() { return static_cast<int>(std::size(Styles)); }
        static const StyleSpec& Style(int style) { return Styles[static_cast<size_t>(std::clamp(style, 0, Count() - 1))]; }

        struct Input
        {
            float Dt = 0;
            bool Flying = false;            // in flight (the engine's): false once she is down
            bool Landing = false;           // R: coming down
            float Rise = -1;                // seconds since she left the ground (her take-off); -1 none
            float Touch = -1;               // the kit's landing clock (-1 none): its reach in the air, then on the ground
            float Reach = .4f;              // the kit's LandReachTime
            DirectX::XMFLOAT3 Velocity{};   // how she moves (m/s; the engine's measure: Player::ActualVelocity)
            float Yaw = 0;                  // her facing
            float Turn = 0;                 // radians a second (the kit's TurnRate)
            const DoorwaysSkeletonPose* After = nullptr;   // on the ground: the pose she goes on in after her landing, as it
                                                           // will be played (null: her rest, the pose she was bound with)
        };
        struct Output
        {
            float Pitch = 0;                // her picture leaned (radians, about its x)
            DirectX::XMFLOAT3 Shift{};      // her picture moved so that the lean turns her about her middle, not her feet
            float Thud = 0;                 // the camera's shake, the frame she lands
        };

        // Her skeleton, once: her joints by name, how they stand at bind; rest: a standing pose of hers (her fingers and
        // toes and all that is not posed here; and a landing ends in it)
        bool Bind(const DoorwaysSkeleton& skeleton, const DoorwaysSkeletonPose& rest)
        {
            using namespace DirectX;
            mBound = false;
            const auto& joints = skeleton.Joints;
            const auto find = [&](const char* name) {
                for (size_t i = 0; i < joints.size(); ++i)
                {
                    const std::string& n = joints[i].Name;
                    const std::string want(name);
                    if (n.size() >= want.size() && n.compare(n.size() - want.size(), want.size(), want) == 0 &&
                        (n.size() == want.size() || n[n.size() - want.size() - 1] == ':' || n[n.size() - want.size() - 1] == '_'))
                        return static_cast<int>(i);
                }
                return -1;
            };
            mHips = find("Hips"); mSpine[0] = find("Spine"); mSpine[1] = find("Spine1"); mSpine[2] = find("Spine2");
            mNeck = find("Neck"); mHead = find("Head");
            const char* arms[2][4] = {{"LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand"}, {"RightShoulder", "RightArm", "RightForeArm", "RightHand"}};
            const char* legs[2][4] = {{"LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase"}, {"RightUpLeg", "RightLeg", "RightFoot", "RightToeBase"}};
            const char* wings[2][3] = {{"WingL1", "WingL2", "WingL3"}, {"WingR1", "WingR2", "WingR3"}};
            const char* dress[2][2] = {{"DressLeft", "DressLeftKnee"}, {"DressRight", "DressRightKnee"}};
            bool all = mHips >= 0 && mSpine[0] >= 0 && mSpine[1] >= 0 && mSpine[2] >= 0 && mNeck >= 0 && mHead >= 0;
            for (int s = 0; s < 2; ++s)
            {
                for (int k = 0; k < 4; ++k) { mArm[s][k] = find(arms[s][k]); mLeg[s][k] = find(legs[s][k]); all = all && mArm[s][k] >= 0 && mLeg[s][k] >= 0; }
                for (int k = 0; k < 3; ++k) { mWing[s][k] = find(wings[s][k]); all = all && mWing[s][k] >= 0; }
                for (int k = 0; k < 2; ++k) mDress[s][k] = find(dress[s][k]);
            }
            if (!all || rest.LocalJoints.size() != joints.size()) return false;
            const size_t n = joints.size();
            mLocal.assign(n, {}); mModel.assign(n, {}); mAt.assign(n, {});
            for (size_t i = 0; i < n; ++i)
            {
                const auto& j = joints[i];
                if (j.ParentIndex >= static_cast<int>(i)) return false;   // (parents come first)
                const XMVECTOR local = XMQuaternionNormalize(XMLoadFloat4(&j.LocalBindRotation));
                XMStoreFloat4(&mLocal[i], local);
                const XMVECTOR model = j.ParentIndex >= 0 ? XMQuaternionMultiply(local, XMLoadFloat4(&mModel[static_cast<size_t>(j.ParentIndex)])) : local;
                XMStoreFloat4(&mModel[i], XMQuaternionNormalize(model));
                const XMVECTOR at = j.ParentIndex >= 0 ? XMLoadFloat3(&mAt[static_cast<size_t>(j.ParentIndex)]) +
                                                            XMVector3Rotate(XMLoadFloat3(&j.LocalBindTranslation), XMLoadFloat4(&mModel[static_cast<size_t>(j.ParentIndex)]))
                                                      : XMLoadFloat3(&j.LocalBindTranslation);
                XMStoreFloat3(&mAt[i], at);
            }
            mRest = rest;
            const XMVECTOR along = XMVectorSet(0, 1, 0, 0);
            for (int s = 0; s < 2; ++s)
            {
                // her elbow: the way her forearm leans off her upper arm at bind is the way it folds; its angle there
                {
                    const XMVECTOR upper = XMQuaternionInverse(Model(mArm[s][1]));
                    const XMVECTOR fore = XMVector3Normalize(XMVector3Rotate(XMVector3Normalize(At(mArm[s][3]) - At(mArm[s][2])), upper));
                    const float c = std::clamp(XMVectorGetY(fore), -1.f, 1.f);
                    mElbowBind[s] = std::acos(c);
                    XMVECTOR fold = fore - along * c;
                    fold = XMVectorGetX(XMVector3LengthSq(fold)) < 1e-6f ? XMVectorSet(-1, 0, 0, 0) : XMVector3Normalize(fold);
                    XMStoreFloat3(&mArmFold[s], fold);
                    XMStoreFloat3(&mElbowAxis[s], XMVector3Normalize(XMVector3Cross(along, fold)));
                    // (her wrist bends about the same line, as her forearm has it)
                    XMStoreFloat3(&mWristAxis[s], XMVector3Normalize(XMVector3Rotate(XMLoadFloat3(&mElbowAxis[s]), XMQuaternionInverse(XMLoadFloat4(&mLocal[static_cast<size_t>(mArm[s][2])])))));
                }
                // her knee folds her shin back: the thigh's own way to her back, at bind
                {
                    const XMVECTOR back = XMVector3Rotate(XMVectorSet(0, 0, -1, 0), XMQuaternionInverse(Model(mLeg[s][0])));
                    XMVECTOR fold = back - along * XMVectorGetY(back);
                    fold = XMVector3Normalize(fold);
                    XMStoreFloat3(&mLegFold[s], fold);
                    XMStoreFloat3(&mKneeAxis[s], XMVector3Normalize(XMVector3Cross(along, fold)));
                    XMStoreFloat3(&mAnkleAxis[s], XMVector3Normalize(XMVector3Rotate(XMLoadFloat3(&mKneeAxis[s]), XMQuaternionInverse(XMLoadFloat4(&mLocal[static_cast<size_t>(mLeg[s][1])])))));
                }
            }
            mThigh = XMVectorGetX(XMVector3Length(At(mLeg[0][1]) - At(mLeg[0][0])));
            mShin = XMVectorGetX(XMVector3Length(At(mLeg[0][2]) - At(mLeg[0][1])));
            mAnkleHigh = XMVectorGetY(At(mLeg[0][2]));
            mBound = true;
            mStyle = -1;
            Reset();
            return true;
        }
        bool Bound() const { return mBound; }

        void Reset()
        {
            mKnown = false; mVel = mAccel = {};
            mHoverW = 1; mFastW = mClimbW = mDiveW = mApproach = mGlide = 0;
            mPhase = 0; mBeatsLeft = 2; mGliding = false; mGlideLeft = 0;
            mPitch = mPitchGo = mBank = mBankGo = 0; mBeatKnown = false;
            mDown = -1; mRiseWas = -1; mRollClock = 0; mRolling = -1;
            mLegSwing = mLegSwingGo = mArmSwing = mArmSwingGo = 0;
            mCrouch = mCrouchGo = 0; mClock = 0; mBobWas = mBobGo = 0;
        }

        // A frame of her flight in a style (1 or more): her pose (all her joints, local) and her picture's lean and shift
        void Step(int style, const Input& in, DoorwaysSkeletonPose& pose, Output& out)
        {
            using namespace DirectX;
            out = {};
            if (!mBound) return;
            const StyleSpec& st = Style(style);
            if (style != mStyle) { mStyle = style; Cache(st); }
            const float dt = std::clamp(in.Dt, 0.f, .1f);
            mClock += dt;

            // ---- how she moves (as the engine measured it: her place's change over its frame)
            XMFLOAT3 v = in.Velocity;
            if (std::abs(v.x) + std::abs(v.y) + std::abs(v.z) > 80) v = mVel;   // (put somewhere: not a move)
            if (!mKnown) { mVel = v; mKnown = true; }
            const XMFLOAT3 velWas = mVel;
            const float k8 = Toward(dt, 8);
            mVel = {mVel.x + (v.x - mVel.x) * k8, mVel.y + (v.y - mVel.y) * k8, mVel.z + (v.z - mVel.z) * k8};
            if (dt > 1e-5f)
            {
                const float k5 = Toward(dt, 5);
                mAccel = {mAccel.x + ((mVel.x - velWas.x) / dt - mAccel.x) * k5, mAccel.y + ((mVel.y - velWas.y) / dt - mAccel.y) * k5,
                          mAccel.z + ((mVel.z - velWas.z) / dt - mAccel.z) * k5};
            }
            const float speed = std::hypot(mVel.x, mVel.z), climb = mVel.y;
            const float fwdX = std::sin(in.Yaw), fwdZ = std::cos(in.Yaw);
            const float aForward = mAccel.x * fwdX + mAccel.z * fwdZ, aUp = mAccel.y;

            // ---- where she is in her flight
            const bool rising = in.Rise >= 0;
            const float riseT = rising ? in.Rise : -1.f;
            if (rising && mRiseWas < 0) { mPhase = 0; mBeatKnown = false; }   // (a take-off: her first beat from the top)
            mRiseWas = riseT;
            const bool landed = !in.Flying && in.Touch >= 0;
            if (landed) { if (mDown < 0) { mDown = 0; out.Thud = st.Thud; mCrouchGo += st.Crouch * 4.f; } else mDown += dt; }   // (down fast: her fall's own speed)
            else mDown = -1;
            const float reach = landed ? 1.f : in.Touch >= 0 ? Ease(in.Touch / (std::max)(.05f, in.Reach)) : 0.f;
            mApproach += (((in.Landing && in.Flying) || in.Touch >= 0 ? 1.f : 0.f) - mApproach) * Toward(dt, 2.2f);
            const float k3 = Toward(dt, 3);
            mHoverW += ((1 - Smooth(1.f, 6.f, speed)) - mHoverW) * k3;
            mFastW += (Smooth(10.f, 16.5f, speed) - mFastW) * k3;
            mClimbW += (Smooth(1.f, 6.f, climb) - mClimbW) * k3;
            mDiveW += (Smooth(2.f, 9.f, -climb) * (1 - mApproach) - mDiveW) * k3;
            const float launch = rising ? 1 - Smooth(st.RiseSeconds * .3f, st.RiseSeconds, riseT) : 0.f;
            const float riseBeat = rising ? 1 - Smooth(st.RiseSeconds * .5f, st.RiseSeconds * 1.2f, riseT) : 0.f;
            const float down = landed ? mDown : -1.f;
            const float impact = landed ? Ease(down / .1f) : 0.f;
            const float recover = landed ? Smooth(st.FlareHold + .05f, st.LandSeconds * .8f, down) : 0.f;
            const float rest = landed ? Smooth(st.LandSeconds * .55f, st.LandSeconds, down) : 0.f;

            // ---- her body
            Shape body = Mix(mC.Hover, mC.Cruise, 1 - mHoverW);
            body = Mix(body, mC.Fast, mFastW);
            body = Mix(body, mC.Climb, mClimbW * (1 - mDiveW));
            body = Mix(body, mC.Dive, mDiveW);
            body = Mix(body, mC.Launch, launch);
            body = Mix(body, Mix(mC.Hover, mC.Reach, .4f), mApproach * (1 - reach));
            body = Mix(body, mC.Reach, reach * (1 - impact));
            body = Mix(body, mC.Impact, impact * (1 - recover));
            body = Mix(body, mC.Stand, recover);

            // ---- her wings' beat
            BeatKey want = MixBeat(st.HoverBeat, st.CruiseBeat, 1 - mHoverW);
            want = MixBeat(want, st.FastBeat, mFastW);
            want = MixBeat(want, st.RiseBeat, riseBeat);
            want = MixBeat(want, st.BrakeBeat, (std::max)(mApproach, reach));
            if (landed) want.Amp = 0;
            if (!mBeatKnown) { mBeat = want; mBeatKnown = true; }
            else mBeat = MixBeat(mBeat, want, Toward(dt, rising ? 10.f : 4.f));
            // (glides: so many beats, then still wings a while; only cruising)
            const float glideOk = st.Glides > 0 ? (1 - mHoverW) * (1 - mClimbW) * (1 - mApproach) * (1 - riseBeat) * (1 - mFastW * .6f) : 0.f;
            if (glideOk > .5f)
            {
                if (mGliding) { if ((mGlideLeft -= dt) <= 0) { mGliding = false; mBeatsLeft = (std::max)(1, static_cast<int>(std::lround(st.GlideSeconds * mBeat.Rate * (1 - st.Glides) / st.Glides))); } }
                else if (mBeatsLeft <= 0) { mGliding = true; mGlideLeft = st.GlideSeconds; }
            }
            else { mGliding = false; mBeatsLeft = (std::max)(mBeatsLeft, 1); }
            mGlide += ((mGliding ? 1.f : 0.f) - mGlide) * Toward(dt, mGliding ? 2.2f : 3.5f);
            const float amp = mBeat.Amp * (1 - mGlide);
            mPhase += mBeat.Rate * dt * (1 - .85f * mGlide);
            if (mPhase >= 1) { mPhase -= std::floor(mPhase); if (!mGliding) --mBeatsLeft; }

            // ---- her body with the beat: up as the wings come down, rocked; her wave; her sway
            const float strength = mBeat.Amp > 1e-3f ? amp / mBeat.Amp : 0.f;
            const float stroke = StrokeValue(Frac(mPhase - .12f), mBeat.Down);
            const float bob = -mBeat.Bob * strength * stroke;
            const float rock = -mBeat.Rock * strength * StrokeValue(Frac(mPhase - .08f), mBeat.Down);
            if (dt > 1e-5f) { const float go = (bob - mBobWas) / dt; mBobAccel = (go - mBobGo) / dt; mBobGo = go; }
            mBobWas = bob;
            body.Pitch += rock;
            body.Look += st.Steady * rock;
            if (st.Wave > 0)
            {
                const float wave = st.Wave * (1 - .5f * mHoverW) * (1 - mApproach) * (1 - launch), a = XM_2PI * st.WaveRate * mClock;
                body.Bow += wave * std::sin(a);
                body.Pelvis += wave * .9f * std::sin(a - 1.3f);
                body.Look -= wave * .6f * std::sin(a + .4f);
                for (int s = 0; s < 2; ++s)
                {
                    TurnAbout(body.Thigh[s], XMVectorSet(1, 0, 0, 0), XMConvertToRadians(wave * 1.1f * std::sin(a - 2.4f)));
                    body.Knee[s] += wave * .9f * (1 + std::sin(a - 2.9f));
                    body.Ankle[s] += wave * .6f * std::sin(a - 3.3f);
                }
                for (int s = 0; s < 2; ++s) TurnAbout(body.Arm[s], XMVectorSet(1, 0, 0, 0), XMConvertToRadians(wave * .5f * std::sin(a + .6f)));
            }
            // her arms and legs swinging free: springs driven by how she is pushed (along her own body)
            {
                const float th = XMConvertToRadians(mPitch);
                const float aFront = aForward * std::cos(th) - (aUp + mBobAccel) * std::sin(th);   // (her front: model +z leaned)
                const float loose = st.Loose;
                const float w = 5 + 9 * (1 - loose), damp = .35f + .55f * (1 - loose);
                const float gain = (.004f + .02f * loose);
                const float target = std::clamp(-aFront * gain, -.6f, .6f);
                for (float left = dt; left > 1e-6f;)
                {
                    const float h = (std::min)(left, 1 / 120.f); left -= h;
                    mLegSwingGo += (w * w * (target - mLegSwing) - 2 * damp * w * mLegSwingGo) * h;
                    mLegSwing += mLegSwingGo * h;
                    mArmSwingGo += (w * w * (target * .7f - mArmSwing) - 2 * damp * w * mArmSwingGo) * h;
                    mArmSwing += mArmSwingGo * h;
                }
                const float legs = landed ? 1 - impact : 1.f;
                for (int s = 0; s < 2; ++s)
                {
                    TurnAbout(body.Thigh[s], XMVectorSet(1, 0, 0, 0), -mLegSwing * legs);
                    TurnAbout(body.Arm[s], XMVectorSet(1, 0, 0, 0), -mArmSwing * legs);
                }
            }

            // ---- her whole body: leaned, banked, spun
            {
                const float target = body.Pitch;
                const float w = st.Snap;
                for (float left = dt; left > 1e-6f;)
                {
                    const float h = (std::min)(left, 1 / 120.f); left -= h;
                    mPitchGo += (w * w * (target - mPitch) - 2 * w * mPitchGo) * h;
                    mPitch += mPitchGo * h;
                }
                const float sway = st.Sway * std::sin(XM_2PI * st.SwayRate * mClock) * (1 - mApproach);
                const float bankTo = (rising || landed ? 0.f : std::clamp(-in.Turn * st.Bank, -st.BankMost, st.BankMost) * (1 - reach)) + sway;
                for (float left = dt; left > 1e-6f;)
                {
                    const float h = (std::min)(left, 1 / 120.f); left -= h;
                    mBankGo += (w * w * (bankTo - mBank) - 2 * w * mBankGo) * h;
                    mBank += mBankGo * h;
                }
            }
            float spin = 0;
            if (rising && st.Spin != 0) spin += XM_2PI * st.Spin * Ease(riseT / (st.RiseSeconds * .9f));
            if (st.Rolls > 0)
            {   // her barrel rolls, cruising straight
                const bool cruising = !rising && !in.Landing && in.Touch < 0 && mHoverW < .3f && std::abs(in.Turn) < .35f;
                if (mRolling >= 0)
                {
                    mRolling += dt / 1.1f;
                    if (mRolling >= 1) { mRolling = -1; mRollClock = 0; }
                }
                else if (cruising && (mRollClock += dt) >= st.Rolls) mRolling = 0;
                else if (!cruising) mRollClock = (std::min)(mRollClock, st.Rolls * .5f);
                if (mRolling >= 0) spin += XM_2PI * Ease(mRolling);
            }

            // ---- her wings (in the air's frame: the sky's up, the way she flies; her folded and tucked wings are her
            // body's, turned into it as far as her chest leans)
            const float chestLean = XMConvertToRadians(mPitch + body.Bow);
            const WingShape fold = Turned(mC.Fold, chestLean), tucked = Turned(mC.Tuck, chestLean - XM_PIDIV2);   // (Tuck: as she lies level)
            WingShape up = Mix(mC.Up, mC.HoverUp, mHoverW), downW = Mix(mC.Down, mC.HoverDown, mHoverW);
            WingShape mid = Mix(mC.Glide, Mix(mC.HoverUp, mC.HoverDown, .5f), mHoverW);
            const float brake = (std::max)(mApproach, reach);
            up = Mix(up, Mix(mC.Brake, mC.Up, .35f), brake);
            downW = Mix(downW, Mix(mC.Brake, mC.Down, .35f), brake);
            mid = Mix(mid, mC.Brake, brake);
            const float tuck = (std::max)(mFastW * st.FastTuck, mDiveW * st.DiveTuck) * (1 - brake) * (1 - riseBeat);
            WingShape wings[2];
            for (int s = 0; s < 2; ++s)
            {
                WingShape w = Stroke(up, mid, downW, fold, Frac(mPhase + (s ? st.Alternate : 0.f)), mBeat, amp);
                w = Mix(w, tucked, tuck);
                if (landed)
                {
                    const float folding = Smooth(st.FlareHold, st.LandSeconds * .85f, down);
                    w = Mix(w, mC.Flare, impact * (1 - folding));
                    w = Mix(w, fold, folding);
                }
                wings[s] = w;
            }

            // ---- on the ground: her feet put down, her hips lowered and rising again
            float crouchTo = landed ? st.Crouch * (1 - recover) : 0.f;
            for (float left = dt; left > 1e-6f;)
            {
                const float h = (std::min)(left, 1 / 120.f); left -= h;
                constexpr float W = 16, D = .55f;
                mCrouchGo += (W * W * (crouchTo - mCrouch) - 2 * D * W * mCrouchGo) * h;
                mCrouch += mCrouchGo * h;
            }
            if (!landed) mCrouch = mCrouchGo = 0;
            Ground ground{};
            ground.Weight = impact;
            ground.Crouch = (std::max)(0.f, mCrouch);
            for (int s = 0; s < 2; ++s)
            {
                const float plant = std::clamp(st.Plant[s] + recover, 0.f, 1.f);
                ground.Plant[s] = plant;
                const float kneel = st.Kneel[s] * (1 - recover);
                const float outW = st.Stance[s][0] + (.12f - st.Stance[s][0]) * recover, fwd = st.Stance[s][1] * (1 - recover);
                ground.Ankle[s] = {s ? -outW : outW, mAnkleHigh + .04f * kneel, fwd};
                ground.Kneel[s] = kneel;
            }

            // ---- her pose
            const float th = XMConvertToRadians(mPitch);
            const XMVECTOR lift = XMVectorSet(0, std::cos(th), -std::sin(th), 0) * bob + XMVectorSet(0, body.Lift, 0, 0);
            Solve(body, wings, th, XMConvertToRadians(mBank), spin, lift, ground, pose);
            if (rest > 0)
            {   // (into the pose she goes on in: what follows her landing, so that it follows without a jump)
                const DoorwaysSkeletonPose& goal = in.After && in.After->LocalJoints.size() == pose.LocalJoints.size() ? *in.After : mRest;
                for (size_t i = 0; i < pose.LocalJoints.size(); ++i)
                {
                    auto& to = pose.LocalJoints[i];
                    const auto& r = goal.LocalJoints[i];
                    XMStoreFloat4(&to.Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&to.Rotation), XMLoadFloat4(&r.Rotation), rest)));
                    XMStoreFloat3(&to.Translation, XMVectorLerp(XMLoadFloat3(&to.Translation), XMLoadFloat3(&r.Translation), rest));
                }
            }
            const float lean = th * (1 - rest);   // (none left as her landing ends)
            out.Pitch = lean;
            // (her picture leaned about her middle: what the lean does to a point there, undone)
            constexpr float Middle = 1.05f;
            out.Shift = {0, Middle - Middle * std::cos(lean), -Middle * std::sin(lean)};
        }

        static float RiseSeconds(int style) { return Style(style).RiseSeconds; }
        // how she is flying, for a log: her speed and climb (m/s), the shares of hovering, speed, climbing, diving, coming
        // down and gliding, her beat (its phase, beats a second, how far), her lean and bank (degrees)
        std::string Note() const
        {
            char s[240];
            std::snprintf(s, sizeof s, "speed %.1f climb %.1f | hover %.2f fast %.2f climbing %.2f dive %.2f approach %.2f glide %.2f | phase %.2f rate %.2f amp %.2f | pitch %.0f bank %.0f",
                          static_cast<double>(std::hypot(mVel.x, mVel.z)), static_cast<double>(mVel.y), static_cast<double>(mHoverW), static_cast<double>(mFastW),
                          static_cast<double>(mClimbW), static_cast<double>(mDiveW), static_cast<double>(mApproach), static_cast<double>(mGlide),
                          static_cast<double>(mPhase), static_cast<double>(mBeat.Rate), static_cast<double>(mBeat.Amp), static_cast<double>(mPitch),
                          static_cast<double>(mBank));
            return s;
        }
        static float LandSeconds(int style) { return Style(style).LandSeconds; }

      private:
        static constexpr float Pi = 3.14159265f;
        struct Shape   // a pose as numbers that mix: ways as vectors (her left side's, in her chest's and hips' frames)
        {
            float Pitch = 0, Bow = 0, Pelvis = 0, Look = 0, Lift = 0;
            DirectX::XMFLOAT3 Arm[2]{}, Fold[2]{};
            float Elbow[2]{}, Wrist[2]{};
            DirectX::XMFLOAT3 Thigh[2]{};
            float Knee[2]{}, Ankle[2]{}, Out[2]{};
        };
        struct WingShape   // each bone's way and the way its feathers trail from it
        {
            DirectX::XMFLOAT3 Bone[3]{}, Trail[3]{};
        };
        struct Ground   // her feet on the ground as she lands
        {
            float Weight = 0, Crouch = 0;
            float Plant[2]{}, Kneel[2]{};
            DirectX::XMFLOAT3 Ankle[2]{};
        };
        struct Cached
        {
            Shape Hover, Cruise, Fast, Climb, Dive, Launch, Reach, Impact, Stand;
            WingShape Fold, Glide, Up, Down, HoverUp, HoverDown, Brake, Tuck, Flare;
        };

        static float Toward(float dt, float rate) { return 1 - std::exp(-rate * dt); }
        static float Smooth(float from, float to, float v)
        {
            if (to <= from) return v >= to ? 1.f : 0.f;
            const float t = std::clamp((v - from) / (to - from), 0.f, 1.f);
            return t * t * (3 - 2 * t);
        }
        static float Ease(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3 - 2 * t); }
        static float Frac(float v) { return v - std::floor(v); }
        static float Lerp(float a, float b, float t) { return a + (b - a) * t; }
        static DirectX::XMFLOAT3 Lerp3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t)
        {
            return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
        }
        static DirectX::XMFLOAT3 Way(const Aim& a)
        {
            const float u = DirectX::XMConvertToRadians(a.Up), r = DirectX::XMConvertToRadians(a.Round);
            return {std::cos(u) * std::cos(r), std::sin(u), std::cos(u) * std::sin(r)};
        }
        static void TurnAbout(DirectX::XMFLOAT3& v, DirectX::FXMVECTOR axis, float radians)
        {
            if (std::abs(radians) < 1e-6f) return;
            DirectX::XMStoreFloat3(&v, DirectX::XMVector3Rotate(DirectX::XMLoadFloat3(&v), DirectX::XMQuaternionRotationAxis(axis, radians)));
        }
        static Shape ToShape(const BodyKey& k)
        {
            Shape o;
            o.Pitch = k.Pitch; o.Bow = k.Bow; o.Pelvis = k.Pelvis; o.Look = k.Look; o.Lift = k.Lift;
            for (int s = 0; s < 2; ++s)
            {
                o.Arm[s] = Way(k.Arm[s].Arm); o.Fold[s] = Way(k.Arm[s].Fold); o.Elbow[s] = k.Arm[s].Elbow; o.Wrist[s] = k.Arm[s].Wrist;
                o.Thigh[s] = Way(k.Leg[s].Thigh); o.Knee[s] = k.Leg[s].Knee; o.Ankle[s] = k.Leg[s].Ankle; o.Out[s] = k.Leg[s].Out;
            }
            return o;
        }
        static WingShape ToWing(const WingKey& k)
        {   // (the feathers' turn about each bone as the way they trail: 0 level and back from it, -90 below it, +90 above)
            using namespace DirectX;
            WingShape o;
            for (int i = 0; i < 3; ++i)
            {
                o.Bone[i] = Way(k.Bone[i]);
                const XMVECTOR d = XMVector3Normalize(XMLoadFloat3(&o.Bone[i]));
                XMVECTOR t0 = XMVector3Cross(XMVectorSet(0, 1, 0, 0), d);
                t0 = XMVectorGetX(XMVector3LengthSq(t0)) < 1e-4f ? XMVectorSet(0, 0, -1, 0) : XMVector3Normalize(t0);
                const float f = XMConvertToRadians(k.Feather[i]);
                XMStoreFloat3(&o.Trail[i], t0 * std::cos(f) + XMVector3Cross(d, t0) * std::sin(f));
            }
            return o;
        }
        // a wing given in her chest's frame, in the air's: her chest leaned forward so far (radians)
        static WingShape Turned(const WingShape& w, float lean)
        {
            using namespace DirectX;
            const XMVECTOR q = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), lean);
            WingShape o;
            for (int i = 0; i < 3; ++i)
            {
                XMStoreFloat3(&o.Bone[i], XMVector3Rotate(XMLoadFloat3(&w.Bone[i]), q));
                XMStoreFloat3(&o.Trail[i], XMVector3Rotate(XMLoadFloat3(&w.Trail[i]), q));
            }
            return o;
        }
        static Shape Mix(const Shape& a, const Shape& b, float t)
        {
            if (t <= 0) return a;
            if (t >= 1) return b;
            Shape o;
            o.Pitch = Lerp(a.Pitch, b.Pitch, t); o.Bow = Lerp(a.Bow, b.Bow, t); o.Pelvis = Lerp(a.Pelvis, b.Pelvis, t);
            o.Look = Lerp(a.Look, b.Look, t); o.Lift = Lerp(a.Lift, b.Lift, t);
            for (int s = 0; s < 2; ++s)
            {
                o.Arm[s] = Lerp3(a.Arm[s], b.Arm[s], t); o.Fold[s] = Lerp3(a.Fold[s], b.Fold[s], t);
                o.Elbow[s] = Lerp(a.Elbow[s], b.Elbow[s], t); o.Wrist[s] = Lerp(a.Wrist[s], b.Wrist[s], t);
                o.Thigh[s] = Lerp3(a.Thigh[s], b.Thigh[s], t); o.Knee[s] = Lerp(a.Knee[s], b.Knee[s], t);
                o.Ankle[s] = Lerp(a.Ankle[s], b.Ankle[s], t); o.Out[s] = Lerp(a.Out[s], b.Out[s], t);
            }
            return o;
        }
        static WingShape Mix(const WingShape& a, const WingShape& b, float t)
        {
            if (t <= 0) return a;
            if (t >= 1) return b;
            WingShape o;
            for (int i = 0; i < 3; ++i) { o.Bone[i] = Lerp3(a.Bone[i], b.Bone[i], t); o.Trail[i] = Lerp3(a.Trail[i], b.Trail[i], t); }
            return o;
        }
        static BeatKey MixBeat(const BeatKey& a, const BeatKey& b, float t)
        {
            t = std::clamp(t, 0.f, 1.f);
            return {Lerp(a.Rate, b.Rate, t), Lerp(a.Amp, b.Amp, t), Lerp(a.Down, b.Down, t), Lerp(a.Lag, b.Lag, t),
                    Lerp(a.Fold, b.Fold, t), Lerp(a.Bob, b.Bob, t), Lerp(a.Rock, b.Rock, t)};
        }
        // the stroke: +1 at the top of the up stroke (phase 0), -1 at the bottom of the down stroke (phase Down)
        static float StrokeValue(float p, float downShare)
        {
            const float d = std::clamp(downShare, .1f, .9f);
            return p < d ? std::cos(Pi * p / d) : -std::cos(Pi * (p - d) / (1 - d));
        }
        static WingShape Stroke(const WingShape& up, const WingShape& mid, const WingShape& down, const WingShape& fold, float phase, const BeatKey& b, float amp)
        {
            WingShape o;
            const float d = std::clamp(b.Down, .1f, .9f);
            for (int k = 0; k < 3; ++k)
            {
                const float p = Frac(phase - b.Lag * static_cast<float>(k));
                const float s = StrokeValue(p, d) * amp;
                const float u = p >= d ? std::sin(Pi * (p - d) / (1 - d)) : 0.f;   // (the up stroke: folded in at its middle)
                const WingShape& to = s >= 0 ? up : down;
                o.Bone[k] = Lerp3(mid.Bone[k], to.Bone[k], std::abs(s));
                o.Trail[k] = Lerp3(mid.Trail[k], to.Trail[k], std::abs(s));
                const float f = std::clamp(b.Fold * u * amp, 0.f, 1.f);
                o.Bone[k] = Lerp3(o.Bone[k], fold.Bone[k], f);
                o.Trail[k] = Lerp3(o.Trail[k], fold.Trail[k], f);
            }
            return o;
        }
        void Cache(const StyleSpec& st)
        {
            mC.Hover = ToShape(st.Hover); mC.Cruise = ToShape(st.Cruise); mC.Fast = ToShape(st.Fast); mC.Climb = ToShape(st.Climb);
            mC.Dive = ToShape(st.Dive); mC.Launch = ToShape(st.Launch); mC.Reach = ToShape(st.Reach); mC.Impact = ToShape(st.Impact);
            mC.Stand = ToShape(Body(0, 0, 0, 0, 0, Arm(-80, 8, 14), Leg(-88, 90, 4, 18)));
            mC.Fold = ToWing(st.Fold); mC.Glide = ToWing(st.Glide); mC.Up = ToWing(st.Up); mC.Down = ToWing(st.Down);
            mC.HoverUp = ToWing(st.HoverUp); mC.HoverDown = ToWing(st.HoverDown); mC.Brake = ToWing(st.Brake); mC.Tuck = ToWing(st.Tuck);
            mC.Flare = ToWing(st.Flare);
        }

        // ---- turns
        static DirectX::XMVECTOR Then(DirectX::FXMVECTOR first, DirectX::FXMVECTOR second) { return DirectX::XMQuaternionMultiply(first, second); }
        static DirectX::XMVECTOR About(DirectX::FXMVECTOR axis, float radians) { return DirectX::XMQuaternionRotationAxis(axis, radians); }
        // the turn that takes her local Y to y and her local X to x (made square to y)
        static DirectX::XMVECTOR Frame(DirectX::FXMVECTOR yWay, DirectX::FXMVECTOR xWay)
        {
            using namespace DirectX;
            const XMVECTOR y = XMVector3Normalize(XMVectorSetW(yWay, 0));
            XMVECTOR x = XMVectorSetW(xWay, 0);
            x = x - y * XMVectorGetX(XMVector3Dot(x, y));
            if (XMVectorGetX(XMVector3LengthSq(x)) < 1e-8f)
            {
                x = XMVector3Cross(y, XMVectorSet(0, 0, 1, 0));
                if (XMVectorGetX(XMVector3LengthSq(x)) < 1e-6f) x = XMVector3Cross(y, XMVectorSet(1, 0, 0, 0));
            }
            x = XMVector3Normalize(x);
            const XMVECTOR z = XMVector3Cross(x, y);
            const XMMATRIX m(x, y, z, XMVectorSet(0, 0, 0, 1));
            return XMQuaternionNormalize(XMQuaternionRotationMatrix(m));
        }
        // a bone turned so that its length points along way and its own side (local) along side
        static DirectX::XMVECTOR Aimed(DirectX::FXMVECTOR way, DirectX::FXMVECTOR side, DirectX::FXMVECTOR localSide)
        {
            using namespace DirectX;
            return XMQuaternionNormalize(Then(XMQuaternionInverse(Frame(XMVectorSet(0, 1, 0, 0), localSide)), Frame(way, side)));
        }
        DirectX::XMVECTOR Model(int j) const { return DirectX::XMLoadFloat4(&mModel[static_cast<size_t>(j)]); }
        DirectX::XMVECTOR LocalBind(int j) const { return DirectX::XMLoadFloat4(&mLocal[static_cast<size_t>(j)]); }
        DirectX::XMVECTOR At(int j) const { return DirectX::XMLoadFloat3(&mAt[static_cast<size_t>(j)]); }
        static void SetLocal(DoorwaysSkeletonPose& pose, int j, DirectX::FXMVECTOR model, DirectX::FXMVECTOR parentModel)
        {
            using namespace DirectX;
            XMStoreFloat4(&pose.LocalJoints[static_cast<size_t>(j)].Rotation, XMQuaternionNormalize(Then(model, XMQuaternionInverse(parentModel))));
        }
        // her feathers' own plane on each wing bone (measured on her mesh): the way they trail from it, in the bone's frame
        static DirectX::XMVECTOR Trail(int side, int bone)
        {
            static constexpr float t[2][3][3] = {{{.30f, 0, -.95f}, {-.50f, 0, -.86f}, {-.71f, 0, -.70f}},
                                                 {{-.32f, 0, -.94f}, {.53f, 0, -.84f}, {.67f, 0, -.74f}}};
            return DirectX::XMVector3Normalize(DirectX::XMVectorSet(t[side][bone][0], t[side][bone][1], t[side][bone][2], 0));
        }

        void Solve(const Shape& b, const WingShape wings[2], float pitch, float bank, float spin, DirectX::FXMVECTOR lift, const Ground& ground, DoorwaysSkeletonPose& pose) const
        {
            using namespace DirectX;
            pose.LocalJoints = mRest.LocalJoints;
            const XMVECTOR X = XMVectorSet(1, 0, 0, 0), Y = XMVectorSet(0, 1, 0, 0);
            const XMVECTOR mirror = XMVectorSet(-1, 1, 1, 0);
            const auto side = [&](int s, XMVECTOR v) { return s ? XMVectorMultiply(v, mirror) : v; };
            // her body's own turn: spun about her length, banked about the way she flies (her length leaned by her picture)
            const XMVECTOR flies = XMVectorSet(0, std::sin(pitch), std::cos(pitch), 0);
            const XMVECTOR whole = Then(About(Y, spin), About(flies, bank));
            const float r = Pi / 180;
            // hips and spine (each a turn of her bind, in her own frame, then her body's)
            const XMVECTOR hipsTurn = Then(About(X, b.Pelvis * r), whole);
            const XMVECTOR hips = Then(Model(mHips), hipsTurn);
            XMStoreFloat4(&pose.LocalJoints[static_cast<size_t>(mHips)].Rotation, XMQuaternionNormalize(hips));
            XMVECTOR rootAt = XMLoadFloat3(&mRest.LocalJoints[static_cast<size_t>(mHips)].Translation) + lift - XMVectorSet(0, ground.Crouch, 0, 0) * ground.Weight;
            XMStoreFloat3(&pose.LocalJoints[static_cast<size_t>(mHips)].Translation, rootAt);
            constexpr float share[3] = {.3f, .65f, 1.f};
            XMVECTOR parent = hips, chestTurn = whole;
            for (int i = 0; i < 3; ++i)
            {
                const XMVECTOR turn = Then(About(X, b.Bow * share[i] * r), whole);
                const XMVECTOR m = Then(Model(mSpine[i]), turn);
                SetLocal(pose, mSpine[i], m, parent);
                parent = m;
                chestTurn = turn;
            }
            const XMVECTOR chest = parent;
            {
                const XMVECTOR neck = Then(Model(mNeck), Then(About(X, (b.Bow - .4f * b.Look) * r), whole));
                SetLocal(pose, mNeck, neck, chest);
                const XMVECTOR head = Then(Model(mHead), Then(About(X, (b.Bow - b.Look) * r), whole));
                SetLocal(pose, mHead, head, neck);
            }
            // her arms
            for (int s = 0; s < 2; ++s)
            {
                const int* a = mArm[s];
                const XMVECTOR shoulder = Then(XMLoadFloat4(&mRest.LocalJoints[static_cast<size_t>(a[0])].Rotation), chest);
                const XMVECTOR way = XMVector3Rotate(side(s, XMLoadFloat3(&b.Arm[s])), chestTurn);
                const XMVECTOR fold = XMVector3Rotate(side(s, XMLoadFloat3(&b.Fold[s])), chestTurn);
                const XMVECTOR upper = Aimed(way, fold, XMLoadFloat3(&mArmFold[s]));
                SetLocal(pose, a[1], upper, shoulder);
                const XMVECTOR foreLocal = Then(LocalBind(a[2]), About(XMLoadFloat3(&mElbowAxis[s]), b.Elbow[s] * r - mElbowBind[s]));
                XMStoreFloat4(&pose.LocalJoints[static_cast<size_t>(a[2])].Rotation, XMQuaternionNormalize(foreLocal));
                if (std::abs(b.Wrist[s]) > 1e-3f)
                {
                    const XMVECTOR handLocal = Then(XMLoadFloat4(&mRest.LocalJoints[static_cast<size_t>(a[3])].Rotation), About(XMLoadFloat3(&mWristAxis[s]), b.Wrist[s] * r));
                    XMStoreFloat4(&pose.LocalJoints[static_cast<size_t>(a[3])].Rotation, XMQuaternionNormalize(handLocal));
                }
            }
            // her wings: each bone along its way, its feathers trailing as they are to (in the air's frame: her picture's
            // lean undone, her bank and spin kept)
            const XMVECTOR air = Then(About(X, -pitch), whole);
            for (int s = 0; s < 2; ++s)
            {
                XMVECTOR above = chest;
                for (int k = 0; k < 3; ++k)
                {
                    const XMVECTOR way = XMVector3Rotate(side(s, XMVector3Normalize(XMLoadFloat3(&wings[s].Bone[k]))), air);
                    const XMVECTOR trail = XMVector3Rotate(side(s, XMLoadFloat3(&wings[s].Trail[k])), air);
                    const XMVECTOR m = Aimed(way, trail, Trail(s, k));
                    SetLocal(pose, mWing[s][k], m, above);
                    above = m;
                }
            }
            // her legs: in the air as the shape has them; on the ground (landing) her feet where they come down
            const XMVECTOR hipsAt = rootAt;
            for (int s = 0; s < 2; ++s)
            {
                const int* l = mLeg[s];
                // in the air
                XMVECTOR way = XMVector3Normalize(XMLoadFloat3(&b.Thigh[s]));
                XMVECTOR fold = XMVector3Cross(X, way);
                if (XMVectorGetX(XMVector3LengthSq(fold)) < 1e-4f) fold = XMVectorSet(0, 0, -1, 0);
                fold = XMVector3Normalize(fold);
                if (std::abs(b.Out[s]) > 1e-3f) fold = XMVector3Rotate(fold, About(way, -b.Out[s] * r));
                const XMVECTOR wayM = XMVector3Rotate(side(s, way), hipsTurn), foldM = XMVector3Rotate(side(s, fold), hipsTurn);
                XMVECTOR thigh = Aimed(wayM, foldM, XMLoadFloat3(&mLegFold[s]));
                XMVECTOR shinLocal = Then(LocalBind(l[1]), About(XMLoadFloat3(&mKneeAxis[s]), b.Knee[s] * r));
                XMVECTOR shin = Then(shinLocal, thigh);
                XMVECTOR foot = Then(Then(LocalBind(l[2]), About(XMLoadFloat3(&mAnkleAxis[s]), b.Ankle[s] * r)), shin);
                const float plant = ground.Weight * ground.Plant[s];
                if (plant > 1e-3f)
                {   // on the ground: her ankle where it comes down, her knee forward between
                    const XMVECTOR hip = hipsAt + XMVector3Rotate(XMLoadFloat3(&mRest.LocalJoints[static_cast<size_t>(l[0])].Translation), hips);
                    const XMVECTOR ankle = XMLoadFloat3(&ground.Ankle[s]);
                    XMVECTOR d = ankle - hip;
                    const float len = (std::max)(1e-3f, XMVectorGetX(XMVector3Length(d)));
                    d = d / len;
                    const float reachTo = (std::min)(len, (mThigh + mShin) * .999f);
                    const float c = std::clamp((mThigh * mThigh + reachTo * reachTo - mShin * mShin) / (2 * mThigh * reachTo), -1.f, 1.f);
                    XMVECTOR pole = XMVectorSet(s ? -.15f : .15f, 0, 1, 0);
                    pole = pole - d * XMVectorGetX(XMVector3Dot(pole, d));
                    pole = XMVectorGetX(XMVector3LengthSq(pole)) < 1e-6f ? XMVectorSet(0, 0, 1, 0) : XMVector3Normalize(pole);
                    const float sn = std::sqrt((std::max)(0.f, 1 - c * c));
                    const XMVECTOR knee = hip + (d * c + pole * sn) * mThigh;
                    const XMVECTOR upper = XMVector3Normalize(knee - hip);
                    XMVECTOR lower = ankle - knee;
                    lower = XMVectorGetX(XMVector3LengthSq(lower)) < 1e-8f ? upper : XMVector3Normalize(lower);
                    XMVECTOR bend = lower - upper * XMVectorGetX(XMVector3Dot(lower, upper));
                    bend = XMVectorGetX(XMVector3LengthSq(bend)) < 1e-6f ? foldM : XMVector3Normalize(bend);
                    const XMVECTOR thighG = Aimed(upper, bend, XMLoadFloat3(&mLegFold[s]));
                    const float kneeAngle = std::acos(std::clamp(XMVectorGetX(XMVector3Dot(upper, lower)), -1.f, 1.f));
                    const XMVECTOR shinG = Then(Then(LocalBind(l[1]), About(XMLoadFloat3(&mKneeAxis[s]), kneeAngle)), thighG);
                    // (her foot flat, as made; a kneeling one on its toes)
                    const XMVECTOR footG = Then(Model(l[2]), About(X, ground.Kneel[s] * 50 * r));
                    thigh = XMQuaternionSlerp(thigh, thighG, plant);
                    shin = XMQuaternionSlerp(shin, shinG, plant);
                    foot = XMQuaternionSlerp(foot, footG, plant);
                }
                SetLocal(pose, l[0], thigh, hips);
                SetLocal(pose, l[1], shin, thigh);
                SetLocal(pose, l[2], foot, shin);
                // (her dress over her legs: its bones are her legs' own)
                if (mDress[s][0] >= 0) pose.LocalJoints[static_cast<size_t>(mDress[s][0])].Rotation = pose.LocalJoints[static_cast<size_t>(l[0])].Rotation;
                if (mDress[s][1] >= 0) pose.LocalJoints[static_cast<size_t>(mDress[s][1])].Rotation = pose.LocalJoints[static_cast<size_t>(l[1])].Rotation;
            }
        }

        // her skeleton
        bool mBound = false;
        int mHips = -1, mSpine[3]{-1, -1, -1}, mNeck = -1, mHead = -1;
        int mArm[2][4]{}, mLeg[2][4]{}, mWing[2][3]{}, mDress[2][2]{};
        std::vector<DirectX::XMFLOAT4> mLocal, mModel;   // bind: local and model turns
        std::vector<DirectX::XMFLOAT3> mAt;              // bind: places (model)
        DoorwaysSkeletonPose mRest;
        DirectX::XMFLOAT3 mArmFold[2]{}, mElbowAxis[2]{}, mWristAxis[2]{}, mLegFold[2]{}, mKneeAxis[2]{}, mAnkleAxis[2]{};
        float mElbowBind[2]{}, mThigh = .44f, mShin = .43f, mAnkleHigh = .09f;
        // the style
        int mStyle = -1;
        Cached mC;
        // her flight
        bool mKnown = false;
        DirectX::XMFLOAT3 mVel{}, mAccel{};
        float mClock = 0;
        float mHoverW = 1, mFastW = 0, mClimbW = 0, mDiveW = 0, mApproach = 0;
        BeatKey mBeat{};
        bool mBeatKnown = false, mGliding = false;
        float mPhase = 0, mGlide = 0, mGlideLeft = 0;
        int mBeatsLeft = 2;
        float mPitch = 0, mPitchGo = 0, mBank = 0, mBankGo = 0;
        float mDown = -1, mRiseWas = -1, mRollClock = 0, mRolling = -1;
        float mLegSwing = 0, mLegSwingGo = 0, mArmSwing = 0, mArmSwingGo = 0;
        float mCrouch = 0, mCrouchGo = 0, mBobWas = 0, mBobGo = 0, mBobAccel = 0;
    };
}
