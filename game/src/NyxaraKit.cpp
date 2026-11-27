#include "NyxaraKit.h"

#include "CombatSystem.h"
#include "DoorwaysAnimatedCharacterAsset.h"
#include "DoorwaysAnimationSampler.h"
#include "DoorwaysCharacterPresentation.h"
#include "DoorwaysGame.h"
#include "DoorwaysGamepad.h"
#include "DoorwaysRuntimePathResolver.h"
#include "DoorwaysPhysicsTypes.h"
#include "DoorwaysRuntimeScene.h"
#include "DoorwaysWalkableSurfaceQuery.h"
#include "GameConstants.h"
#include "NyxaraClips.h"
#include "NyxaraFlair.h"
#include "NyxaraHud.h"
#include "NyxaraSoul.h"
#include "Renderer.h"
#include "Sound.h"
#include "WorldRenderer.h"
#include "WorldState.h"
#include "DoorwaysAsset/DoorwaysDMeshLoader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>

using namespace DirectX;

// Her moves are the Character Lab's (testing/CharacterLab, where the user tried and shaped them; its README tells how
// each came to be), with the lab's station creatures replaced by the enemies of the kit: the journey's monsters, or
// the kit's own shades. Numbers and names are kept as the lab has them, so the two can be read side by side.

namespace Doorways
{
    NyxaraKit& GetNyxaraKit()
    {
        static NyxaraKit kit;
        return kit;
    }

    namespace
    {
        constexpr float Pi = 3.14159265f;
        const XMFLOAT4 MoonWhite{.86f, .97f, 1.f, 1.f};
        const XMFLOAT4 SoulGreen{.22f, 1.f, .52f, 1.f};
        const XMFLOAT4 ShadeViolet{.62f, .28f, 1.f, 1.f};
        // her wings' feathers (on WingFeather.png, whose body is a middle grey and its bar white): dark slate, and silver
        const XMFLOAT4 FeatherDark{.32f, .33f, .38f, 1.f}, FeatherSilver{.78f, .8f, .86f, 1.f};

        // ---- the lab's numbers (testing/CharacterLab/CharacterLab.cpp)
        using NyxaraClips::RunPace;
        using NyxaraClips::WalkPace;
        constexpr float AttackRate = 0.98f;  // how much faster than made
        constexpr float AttackLead = 0.34f;  // clip seconds before the hit at which a cut is entered
        constexpr float ChainAfter = 0.14f;  // clip seconds after the hit at which the next cut starts
        constexpr float AttackTail = 0.80f;  // clip seconds after the hit at which the last cut is over
        constexpr float WhipMost = 6.5f;     // m/s: her hand goes no faster than this, at its fastest, to where a cut's swing begins
        constexpr float TurnMost = 26.f;     // radians a second (1500 degrees): nor turns faster
        constexpr float CutEnd = .04f;       // what is carried into a cut is gone this long before its blow (clip s)
        constexpr float LeadCost = 12.f;     // entering a cut earlier is worth it if her hand is this much slower (m/s per second earlier)
        constexpr int CutTrackEarly = 36, CutTrack = CutTrackEarly + 4;   // a cut's hands are known from 0.6 s before its blow (60 a second)
        constexpr float SettleSeconds = .12f;                           // a sword taken or let go is in its new place after this long
        // A draw into a cut or a skill: up to the handles, and on to the clip (s). (0.19 and 0.10 until 2026-11-19: out of
        // standing at ease her hands went up at 16 m/s and came down onto the cut turning at 3000 degrees a second.)
        constexpr float DrawReachPressed = .22f, DrawOnPressed = .18f;
        constexpr float DrawOnCut = .22f;    // (into a cut: from the handles into the swing, there just before the blow)
        // A wrist bends so far (2026-11-19), degrees: toward its little finger (ulnar), toward its thumb (radial), up or
        // down. Within the first of each pair her hand is as the clip has it; beyond, it gives ever less up to the second
        // (HoldWrists). On a handle on her back the first is as far as it goes (GripOnBack).
        constexpr float WristUlnar[2] = {32.f, 45.f}, WristRadial[2] = {20.f, 32.f}, WristFlex[2] = {60.f, 80.f};
        constexpr float WristTakes = .25f;        // seconds over which her wrists come to be so held as her swords come into her hands (and go)
        constexpr float SkillOut = .42f;          // seconds from the end of a skill back to her guard, or on her way
        constexpr float CutOut = .34f;            // and from the end of a cut (one not followed by another)
        constexpr float SkillIn = .30f;           // seconds into the spin, the cast or the throw (slow away and slow in)
        // Her arms' way to the handles and from them (ReachArms):
        constexpr float ElbowFollows = 16.f;     // her elbow goes after where it is wanted on its circle: this much of what is left, a second
        constexpr float RollFollows = 40.f;      // her forearm and hand roll after the sword: so too
        constexpr float RollMost = 1800.f;       // and no faster than this (degrees a second)
        constexpr float BladeLeads = .75f;       // the blade is where the pose has it when her hand has gone this much of its way from the handle
        // Each hand's cuts in a row, in the order in which they go into one another (2026-11-19). From the end of one cut
        // to where the next one's swing begins her hand had 0.8 and 1.7 m to go in the left row (slash, thrust, slash: the
        // thrust ends with her arm stretched out before her, far from every wind-up) and 0.4, 1.0 and 1.0 m in the right;
        // now 0.5 and 0.5 m, and 0.3, 0.3 and 1.0 m. The last of each row is the heavy blow.
        constexpr const char* LeftChain[] = {"Attack_L1", "Attack_L3", "Attack_L2"};
        constexpr const char* RightChain[] = {"Attack_R1", "Attack_R4", "Attack_R2", "Attack_R3"};
        constexpr float HipsShare = .35f;    // of the cut's own turn of her hips that her hips keep while she walks or runs
        constexpr float FrontArc = 30.f;     // degrees to either side of her line: what is before her
        constexpr float CastAirRange = 3, CastAirHeight = .35f, CastAirSink = 9;
        constexpr float SwordReach = 1.7f, JumpReach = 2.2f, SwordArc = FrontArc;
        constexpr int CutDamage = 45, LastDamage = 80, JumpDamage = 110;
        constexpr float SteerRate = 180.f;   // degrees a second A and D turn her (owner, 2026-11-18: A/D turn her slowly round, the view stays where the mouse puts it, 90; 2026-11-20: "make it one seconds 180dagree")
        constexpr float BackSpeed = 1.25f;   // metres a second stepping backwards
        // The Jump clip (Mixamo's Running Jump as build_from_mixamo.py leaves it, 0.9 s), measured from the clip: she
        // leaves the ground at its start, is at the top at JumpApex, her first foot is down again at JumpLand; falling
        // faster than she jumped she holds the pose at JumpHold (her leg reaching down). After JumpLand: her step out.
        constexpr float JumpApex = .333f, JumpHold = .575f, JumpLand = .617f;
        constexpr float DropSeconds = .45f;  // held that long in the reaching pose, a drop becomes a fall (the Fall clip)
        // F, her slide (Skill_Slide: 1.53 s and 6.2 m as it was made), as a dash (2026-11-25; the owner: "i want f to be 3x
        // faster and 3x longer distance to dash").
        // Three times as fast: the game played it, as her other skills, at half the speed it was made at (3.1 s); it is
        // played at SlidePace now, one and a half times that speed, wherever she is (1.04 s).
        // Three times as far (18.7 m), and then seven tenths of that (2026-11-26; the owner: "i want F do have 0.7x of the
        // current distance"): SlideFar times the clip's own way (13.1 m), the way added while she is down and sliding
        // (gathered from SlideGoIn of the clip over SlideGoGather, held to SlideGoOut, lost over SlideGoLose), so that she
        // drops into it and gets up out of it at the clip's own speed and her feet hold. (Laid evenly over the clip she
        // would leave it faster than a run and be stopped short: out of a move she is carried on by a run's speed at
        // the most.)
        // What she passes is cut, to SlideTo of the clip: every creature her way since a frame ago goes within SlideWide of
        // (and its own width), and again for every SlideCutEvery metres she goes on within that reach of where it stood
        // when her way first met it, SlideCutsMost times at the most: three cuts through a creature of a man's width,
        // four through a giant. (Her cuts knock it on along her way and it moves by itself: measured to where it is, the
        // first ali of the Ascent, passed squarely, was cut three times or four as the frames fell.) (It was a cut every
        // 0.22 s of the clip at what stood within 1.7 m before her: one every 1.1 m of her way, four in all. By the clock,
        // at this speed, she would go five metres between two cuts and pass a creature without one. And looked at only
        // where she stands each frame, half a metre on from the last, a creature a metre off her line is stepped over:
        // in the Ascent she slid past the first ali so, and did not cut it.) The clay jars beside her way break as near
        // it: the game is told of a blow all round her every SlideTell m of it. (SlidePut m or more between two frames is
        // no way of hers: she was put there, her window's stage or a fall's way back, and nothing between is cut.)
        // Its look is her gear's (2026-11-26; the owner, asked whether it should have her gear's trail: "yes based on
        // what gear she wears"): with a flair on, its wake along her way (NyxaraFlair::Dash), her blades' trails, and
        // each cut its bite, as a cut in passing (Blow::Passing: no moment is held, or her slide would stop in it). As
        // she set out, with no gear, there is only blood. Without a flair: her old light.
        constexpr float SlidePace = 1.5f, SlideFar = 2.1f;
        constexpr float SlideGoIn = .03f, SlideGoGather = .15f, SlideGoOut = .8f, SlideGoLose = .35f;
        constexpr float SlideTo = 1.1f, SlideWide = 1.f, SlideCutEvery = 1.1f, SlideTell = .5f, SlidePut = 12.f;
        // Her soul (Q: THREAD STEP, tried in the Character Lab and chosen by the owner, 2026-11-26; NyxaraSoul.h). Its
        // leaving (Skill_Soul, made at load: MakeSoul, played as made) throws her SoulFar m on, from SoulOutFrom s into it
        // to SoulOutTo s, fastest in the middle. Her way back takes SoulBackLeast + SoulBackPer s a metre apart,
        // SoulBackShortest to SoulBackMost s (the owner: "she flies back fast along the strings in about a second"),
        // pulled at her chest (SoulFlinch: her flinch's push, a quarter more when it is behind her). Its throw over
        // (SoulOutTo), the rest of its leaving is hers to give up at once (the owner, 2026-11-26: "let it be able to fight
        // right after Q right away"). SoulCooldownSeconds s after it is over (she is in it again, it is let go, she dies) Q
        // may leave it again (the owner, 2026-11-26: "give it a 15second cooldown for a skill"; 0.6 s, mine, until then).
        // Put more than SoulPutAway m from where the kit last left her (a nest, the start: no step of hers), she lets it go.
        constexpr float SoulFar = 6.f, SoulOutFrom = .04f, SoulOutTo = .26f, SoulFlinch = 4.f;
        constexpr float SoulBackLeast = .55f, SoulBackPer = .05f, SoulBackShortest = .7f, SoulBackMost = 1.f, SoulCooldownSeconds = 15.f;
        constexpr float SoulPutAway = 25.f;
        // On a climb: the engine makes its moves in pieces of SlideStep m, each on the ground it reaches (GameSession's
        // LongMovePiece). Made in one piece, half a metre a step of the engine, it went 5 m up the way before the first
        // creature of the Ascent: her height follows the ground with a lag, and the ground ahead was a step too high.
        // In pieces it goes up what her easy pace goes up (every slope that is no rock face to the engine); her run is
        // stopped sooner on a steep one, its height further behind. Measured over the whole way (the off-the-way test,
        // -Dash against -Jog): where her slide went further than her run for that, her easy pace ended where it did.
        constexpr float SlideStep = .15f;
        // Her going on foot at a faster pace than as made (SetFootPace; the game's three times since 2026-11-26) is made
        // in pieces of FootStep m too. In one piece her run of 16.2 m/s went 0.27 m a step of the engine, and her height,
        // following the ground a fifth of the way a step (HeightFollowSpeed 12), fell behind it by more than the engine's
        // step (0.45 m) on a slope steeper than about 18 degrees, by the engine's sums (her run as made: 45), and there she
        // stopped. In pieces she goes up what her easy pace as made went up, at all three of her paces (Ctrl's 5 m/s goes
        // 0.083 m a step: two pieces).
        constexpr float FootStep = .075f;
        constexpr int SlideDamage = 35, SlideCutsMost = 4;
        // The feather storm (C; Skill_Storm, made by MakeStorm): her times in it (s of the clip). She crouches (to StormCrouch),
        // leaves the ground (StormLeave), is at the top (StormTop), floats there and comes down from StormFall, her feet on
        // the ground again at StormDown; her body hovers as in Fly_Rise up to StormHang and lands as in Fly_Land after it.
        constexpr float StormCrouch = .20f, StormLeave = .25f, StormTop = .58f, StormFall = .95f, StormDown = 1.36f, StormHang = .91f;
        constexpr float StormSeconds = 1.91f;
        constexpr float StormHeight = 1.7f, StormFloat = .1f;   // m her picture goes up, and floats on up at the top
        // StormTurns whole turns to her left from StormSpinFrom to StormSpinTo, gathering speed over the first StormSpinIn of
        // that time and losing it over the last StormSpinOut (at the fastest 1300 degrees a second)
        constexpr float StormSpinFrom = .25f, StormSpinTo = 1.25f, StormSpinIn = .12f, StormSpinOut = .22f, StormTurns = 3;
        // her wings: up as she crouches (StormWingUp), beaten down as she leaves the ground (StormWingBeat), spread until
        // StormWingClose, as on guard again at StormWingFolded; each of their three bones trails her turn by so much at its fastest
        constexpr float StormWingUp = .18f, StormWingBeat = .30f, StormWingClose = 1.22f, StormWingFolded = 1.58f;
        constexpr float StormTrail[3] = {.14f, .10f, .10f};
        // Her feathers are thrown as daggers (2026-11-19; the owner: "its throwing feathers like a firework, but i want more
        // like dagger effect and increase range 3 times"): from StormThrowFrom to StormThrowTo, StormRate a second, each
        // wing in turn, each straight out from it (turned StormSwirl on round, the way she turns) at StormDaggerSpeed and
        // down to the ground somewhere from StormDaggerNear to StormReach off, falling StormDrop on its way; there it sticks,
        // half in, for StormStuckSeconds (fading over the last StormStuckFade).
        constexpr float StormThrowFrom = .30f, StormThrowTo = 1.18f, StormRate = 50.f;
        constexpr float StormDaggerSpeed = 30.f, StormDaggerNear = 6.f, StormSwirl = .45f, StormDrop = 3.f;
        constexpr float StormStuckSeconds = 2.2f, StormStuckFade = .6f;
        // stuck, one stands at least this steep (radians from level; it came down 6 to 25 degrees steep, and lying so,
        // half in the ground, could not be seen), this much of its length in the ground
        constexpr float StormStuckSlant = .75f, StormStuckIn = .3f;
        constexpr float StormReach = 27.f;   // m: every creature this near (its width counted) is struck by her feathers at each turn (9 until 2026-11-19)
        constexpr int StormDamage = 40;
        constexpr float StormPush = 1.2f;
        constexpr float FlyLeanHover = .07f, FlyLeanFast = 1.05f, FlyLeanSpeed = 7.f;
        constexpr float RiseLifts = .4f, LandReachTime = .4f;
        constexpr float CastRange = 7, CastRate = 1.7f, CastSwordSpeed = 26, CastWait = .22f, CastDash = 12;
        constexpr float CastSpin = 2.6f, CastAirEvery = .012f, CastAirRound = .8f, CastAirLife = .3f;
        constexpr float CastFallSeconds = .35f, CastDownSeconds = .9f, CastRiseSeconds = .6f, CastFallen = 1.45f;
        constexpr int CastKick = 40;
        // her swords stick in the middle of its body (CastMiddle of its height), at least CastLowest up (0.35 until
        // 2026-11-19: she crouches on its side by them now, her back to the ground, and her back would be on it)
        constexpr float CastMiddle = .55f, CastLowest = .8f;
        constexpr int CastDamage = 90, CastPull = 60;
        // V's way to her swords and her backflip off the creature (2026-11-19; MakeCast, TurnBody, ReachHandles). The
        // owner: "she twists, sticks with two feet on [it], grab[s the] swords and make[s] then a backflip and stand[s]
        // still". Her way takes at least DiveLeast s (at CastDash, or as fast as CastAirSink lets her down in flight): over
        // DiveLie s she lies back into her twist as Cast_Fly lay (feet first: DrillLean) and twists as she used to, whole
        // turns; her middle rises DiveArc on its way, and her feet come to the creature's side FeetUnder under the handles.
        // In Cast_Flip her feet stick on it, her body turns square to it and her legs give, crouching her on its side, her
        // wings opening and her hands on the handles by FlipStick
        // and her fists closed by FlipGrab; at FlipPush she kicks off it (her swords coming out over FlipRip) and her feet
        // have left it by FlipOff, her body turning back over; she tucks at FlipTuck, has turned upright and her legs reach
        // down at FlipReach, she lands at FlipLand, FlipBack short of the handles, facing it, and stands on guard, still,
        // at FlipSeconds. Off it her hips fly as a thrown thing does, pulled down at FlipFall m/s2 (more than the world's
        // 9.8, to make it quick: about a metre up); her feet's middles are FlipSole off its side.
        constexpr float DiveLeast = .42f, DiveLie = .12f, DiveArc = .18f, DiveSeconds = 1.f, FeetUnder = .05f;
        constexpr float FlipStick = .16f, FlipGrab = .22f, FlipPush = .34f, FlipOff = .46f, FlipRip = .1f;
        constexpr float FlipTuck = .66f, FlipReach = .92f, FlipLand = 1.06f, FlipSeconds = 1.44f;
        constexpr float FlipBack = 1.4f, FlipFall = 12.f, FlipSole = .07f;
        constexpr float ReachBendMost = .9f;   // radians her back may bend forward more for her hands to reach the handles (ReachHandles)
        // (the poses it is made of: Jump's tuck, on it with her back bent FlipSquatLean (radians) forward over her spine,
        // and in the backflip; Fly_Land's reach for the ground and its crouch; seconds into those clips)
        constexpr float FlipTuckPose = .45f, FlipReachPose = .40f, FlipCrouchPose = .62f, FlipSquatLean = .6f;
        // F in flight, the twisted dash (2026-11-19; the owner: "i want F while flying to do same twist as it does while V,
        // it will be called twisted-dash and it also makes damage through"): DashSeconds at DashSpeed, laid back into V's
        // twist (DrillLean, feet first, over DashTip, and back to her flight over DashTipBack), DashTurns whole times round
        // about her length. Every creature her way goes through (DashWide either side of her line, its width counted) is
        // struck once. Off unless asked for (SetFlightDash; 2026-11-26, the owner: "F should not work while flying"): in
        // the game and the Character Lab F in flight does nothing but say that her slide is from the ground.
        constexpr float DashSpeed = 15, DashSeconds = .6f, DashTip = .14f, DashTipBack = .18f, DashTurns = 2;
        constexpr float DashWide = .7f, DashPush = 2.5f;
        constexpr int DashDamage = 60;
        // Her finishing kills (2026-11-19; the owner: "i want 3 different finishing kill animations for nyxara to happen
        // when she is killing monsters everytime and each one happens danromly for each kill. its needs hard work"). A
        // blow that would kill a creature she may finish (Wraith::Finishable) dooms it instead: it stays alive, dazed,
        // DoomSeconds. As soon as she is free on foot and it is within FinishReach of her (its width counted) and
        // FinishHigh above or below her, she finishes it: one of the three (Finishes), at random and never the same twice
        // running; nothing hurts her meanwhile. Not finished in time, it falls on its own: on its knees over DoomKneel s,
        // then over in DoomFall s. A dead body lies FallenLies s after it is down, then fades; down, it lies FallenTilt
        // radians from upright (the rest of the way it is raised onto the ground by its owner).
        constexpr float DoomSeconds = 3.f, FinishReach = 5.5f, FinishHigh = 1.6f;
        constexpr float DoomKneel = .4f, DoomFall = .55f, FallenLies = NyxaraKit::Wraith::LiesAfter, FallenTilt = 1.45f;
        // Each finisher: its name (shown as it begins), its clip (MakeFinishes), how long it lasts; when its blows land
        // (s into it; < 0 none), when the creature dies and begins to fall, and how long its fall takes. Talon Cross:
        // a dash in, two cuts crossing on it (Attack_R1's and Attack_L1's, where their blades go fastest), and past it
        // round its side; she stands with her back to it, and it falls on its face. Eagle's Plunge: Attack_Jump's leap
        // (her wings beat), her left blade down into it from the front under its neck (the moment and her lift are
        // worked out from its height: StartFinish); it goes over backwards and she rides it down, then springs off it.
        // Gale Slide: Skill_Slide past its right, her blade through its legs (it drops to its knees); up, round, and
        // Attack_R3's blade down behind her through its neck (the moment is its height's; the table's are a fallback).
        // (2026-11-20, the owner: "create 10 finishing animations that i will choose 3 from. you can also keep those 3 we
        // have now and addition 7 . then let me test them same way with 0-9 numbers"; and "all those 10 animation should be
        // chosen base a criteria that should be possible to make to all of our 12characters": the seven more ask nothing of
        // a creature but its height, width, neck and knees, which every one has: FinishMore.)
        constexpr int Finishers = 10;
        struct FinishSpec { const wchar_t* Name; const char* Clip; float Seconds; float Blow[2]; float Death, Falls; };
        constexpr FinishSpec Finishes[Finishers + 1] = {
            {L"", "", 0, {-1, -1}, 0, 0},
            {L"TALON CROSS", "Finish_Cross", 2.3f, {.44f, .57f}, 1.40f, .48f},
            {L"EAGLE'S PLUNGE", "Finish_Plunge", 2.75f, {1.10f, -1}, 1.10f, .65f},
            {L"GALE SLIDE", "Finish_Slide", 2.6f, {.39f, -1}, 1.55f, .55f},
            {L"HEARTPIERCE", "Finish_Pierce", 2.2f, {-1, -1}, 1.45f, .6f},
            {L"REAPER'S WHEEL", "Finish_Wheel", 2.3f, {-1, -1}, 1.30f, .55f},
            {L"MOUNTAIN BREAKER", "Finish_Breaker", 2.3f, {-1, -1}, 1.05f, .5f},
            {L"MOONLIT FLASH", "Finish_Flash", 2.4f, {-1, -1}, 1.55f, .5f},
            {L"TEMPEST FLURRY", "Finish_Flurry", 2.4f, {-1, -1}, 1.52f, .55f},
            {L"HEADSMAN", "Finish_Headsman", 2.5f, {-1, -1}, 1.42f, .5f},
            {L"RISING TALON", "Finish_Rising", 2.3f, {-1, -1}, .50f, .7f},
        };
        // The seven more (4 to 10), each the same way: she goes in to where her blade's point, as the blow Place lands,
        // is on the front of it at Aim times its neck's height (on its knees, as low as they bring it), lifted up to it
        // where it is tall (no higher than 2.6 m); so the blow lands on a small one and on a giant alike.
        //   Hit, Hand: its blows (s into it; -1 none), the blade of each (0 left, 1 right, 2 both); Place: the blow her place
        //   is for; Aim < 0: as the blade comes (no lift). InFrom..InTo: in to that place. Past: on past it, round its side,
        //   to Past m beyond it (0 none), over PastFrom..PastTo. Leap: a leap of her own (m), up from LeapFrom to LeapTop,
        //   down by LeapDown. LiftDown: her lift to its height let down by then. Kneels: the blow after which it goes down
        //   on its knees (KneelTo of the way; -1 none). Falls: 0 away from her, 1 back the way she came (she went past it),
        //   2 to her right. Show, its kill: 0 a cut, 1 the thrust held in it, 2 both blades into the ground (it shakes),
        //   3 her cuts opening after she has gone past it, 4 a rising cut that takes her up on her wings.
        //   Heartpierce: her left blade driven through its chest, held there as it sags to its knees, drawn out.
        //   Reaper's Wheel: one whole turn with both blades out (cutting it), and the right brought down from high.
        //   Mountain Breaker: a cut across, a hop, and both blades down into the ground before it.
        //   Moonlit Flash: crouched, a blink past its side with one cut; she stands with her back to it, and it opens.
        //   Tempest Flurry: four cuts: high from her left, back across, low at its legs (it sinks), and across its neck.
        //   Headsman: a cut at its legs drops it to its knees; her right blade raised high and down on its neck.
        //   Rising Talon: a rising cut that carries her up on her wings, and she comes down as it falls back.
        struct FinishMore
        {
            float Hit[4]; int Hand[4]; int Place; float Aim;
            float InFrom, InTo, Past, PastFrom, PastTo;
            float Leap, LeapFrom, LeapTop, LeapDown, LiftDown;
            int Kneels; float KneelTo; int Falls, Show;
        };
        // The owner chose four of the ten (2026-11-20: "i like finishing on number 4,5,7,8 ... let them be finishing randomly
        // for each kill"; the lab's keys 4, 5, 7 and 8): she plays one of these at random, never the same twice running. The
        // others are kept, made at load, for the Character Lab's cue and CAUCASIAN_EAGLE_NYXARA_FINISHER.
        constexpr int Kept[] = {5, 6, 8, 9};   // Reaper's Wheel, Mountain Breaker, Tempest Flurry, Headsman
        constexpr FinishMore More[Finishers + 1] = {
            {}, {}, {}, {},
            {{.70f, -1, -1, -1}, {0, 0, 0, 0}, 0, .8f, 0, .62f, 0, 0, 0, 0, 0, 0, 0, 1.7f, 0, .45f, 0, 1},
            {{.60f, 1.30f, -1, -1}, {1, 1, 0, 0}, 1, .75f, 0, .5f, 0, 0, 0, 0, 0, 0, 0, 1.6f, -1, 0, 2, 0},
            {{.50f, 1.05f, -1, -1}, {1, 2, 0, 0}, 1, -1, 0, .45f, 0, 0, 0, .5f, .70f, .88f, 1.05f, 1.3f, -1, 0, 0, 2},
            {{.55f, -1, -1, -1}, {1, 0, 0, 0}, 0, .7f, .15f, .50f, 1.5f, .50f, .75f, 0, 0, 0, 0, .9f, -1, 0, 1, 3},
            {{.42f, .84f, 1.12f, 1.52f}, {0, 0, 0, 0}, 3, .7f, 0, .38f, 0, 0, 0, 0, 0, 0, 0, 1.8f, 2, .4f, 0, 0},
            {{.42f, 1.42f, -1, -1}, {0, 1, 0, 0}, 1, 1.f, 0, .38f, 0, 0, 0, 0, 0, 0, 0, 1.75f, 0, 1.f, 2, 0},
            {{.50f, -1, -1, -1}, {1, 0, 0, 0}, 0, .55f, 0, .42f, 0, 0, 0, 1.3f, .40f, 1.0f, 1.6f, 1.5f, -1, 0, 0, 4},
        };
        // Eagle's Plunge: when she springs off it ('contact': when her blades went in), and how long her spring takes
        constexpr float PlungeHop = .45f;
        float PlungeHopFrom(float contact) { return (std::max)(2.05f, contact + Finishes[2].Falls + .1f); }
        constexpr float FlyHover = .5f;      // m she is shown over the ground when she rests on it in flight
        constexpr float RunFrom = 2.0f;      // m/s: slower than this she walks
        // Stairs_Up (Mixamo's climb fitted to Map A's stairs) is off: the owner (2026-11-17) prefers her ordinary walk and run
        // on stairs, mountain ground and hills alike, her feet set on the ground and treads under them. True plays it again.
        constexpr bool StairClip = false;
        constexpr float StairPace = 1.0f, StairRunPace = 1.6f;    // m/s along the ground up a stair (W, Shift): the stair clip (0.62 m/s on 0.37 m treads) 1.6 and 2.6 times over
        constexpr float ClimbEnter = .3f, ClimbLeave = .15f;    // m up a metre forward on a stair's treads: from this she climbs it (Stairs_Up), below the second she walks again
        constexpr float CameraFollowLift = 25.f;
        // ---- the game's
        constexpr float SkillCooldown[3] = {4.f, 7.f, 9.f};   // F the slide, V the thrown swords, C the feather storm
        // Her wing shield (E; owner, 2026-11-21: "i want E should protect her for 3seconds and she also should be able to keep
        // fighting at the same time"): one press raises it for ShieldSeconds, from every side, and she goes on cutting, using
        // her skills, walking and flying under it; then it rests ShieldRestSeconds. (Until then E was held, and held her still.)
        constexpr float ShieldSeconds = 3.f, ShieldRestSeconds = 7.f;
        constexpr float GoneSeconds = .5f;                    // a creature that died is held this long (the journey frees its slot after .55 s)
        constexpr float CutPush = 1.5f;                       // m/s a cut knocks a creature back
        constexpr int HeavyBlow = 80;                         // from this much damage a blow freezes the moment

        XMFLOAT3 Add(XMFLOAT3 a, XMFLOAT3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
        XMFLOAT3 Lerp(XMFLOAT3 a, XMFLOAT3 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; }
        XMFLOAT3 Sub(XMFLOAT3 a, XMFLOAT3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
        XMFLOAT3 Mul(XMFLOAT3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
        float Len(XMFLOAT3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
        float LenXZ(XMFLOAT3 a) { return std::sqrt(a.x * a.x + a.z * a.z); }
        XMFLOAT3 Norm(XMFLOAT3 a) { const float l = Len(a); return l > 1e-5f ? Mul(a, 1 / l) : XMFLOAT3{0, 0, 1}; }
        XMFLOAT3 Cross(XMFLOAT3 a, XMFLOAT3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
        float Dot(XMFLOAT3 a, XMFLOAT3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        XMFLOAT3 Forward(float yaw) { return {std::sin(yaw), 0, std::cos(yaw)}; }
        XMFLOAT4 Alpha(XMFLOAT4 c, float a) { c.w = a; return c; }
        float Smooth(float a, float b, float x) { const float t = std::clamp((x - a) / (b - a), 0.f, 1.f); return t * t * (3 - 2 * t); }
        // a creature's feet (its Pos is its aim point, 1.3 m over them), and its neck from them in the world's axes
        XMFLOAT3 FeetOf(const NyxaraKit::Wraith& w) { return {w.Pos.x, w.Pos.y - 1.3f + w.Lift, w.Pos.z}; }
        XMFLOAT3 NeckFrom(const NyxaraKit::Wraith& w)
        {
            const float c = std::cos(w.Yaw), sn = std::sin(w.Yaw);
            return {w.Neck.x * c + w.Neck.z * sn, w.Neck.y, -w.Neck.x * sn + w.Neck.z * c};
        }
        // a quadratic curve from a over c to e, at s (0..1), and its way there
        XMFLOAT3 Curve(XMFLOAT3 a, XMFLOAT3 c, XMFLOAT3 e, float s) { const float q = 1 - s; return Add(Add(Mul(a, q * q), Mul(c, 2 * s * q)), Mul(e, s * s)); }
        XMFLOAT3 CurveWay(XMFLOAT3 a, XMFLOAT3 c, XMFLOAT3 e, float s) { return Add(Mul(Sub(c, a), 2 * (1 - s)), Mul(Sub(e, c), 2 * s)); }
        // Gale Slide: how far along her slide she is (0..1), t s into it, at Skill_Slide's own pace (its first 0.87 s
        // over the finisher's first 0.78)
        float SlideShare(float t)
        {
            const auto found = NyxaraClips::Attacks.find("Skill_Slide");
            if (found == NyxaraClips::Attacks.end() || found->second.Forward.size() < 2) return Smooth(0, .78f, t);
            const auto& path = found->second.Forward;
            const auto along = [&](float clip)
            {
                const float f = std::clamp(clip * 30, 0.f, static_cast<float>(path.size() - 1));
                const size_t i = (std::min)(static_cast<size_t>(f), path.size() - 2);
                return path[i] + (path[i + 1] - path[i]) * (f - static_cast<float>(i));
            };
            return std::clamp(along(std::clamp(t, 0.f, .78f) * (.87f / .78f)) / (std::max)(along(.87f), 1e-3f), 0.f, 1.f);
        }
        // F, her slide: the metres a second of the clip that are added to its own way, t s into it (SlideFar; whole: the
        // clip's own way, m). Over the clip they come to SlideFar - 1 times that way.
        float SlideMore(float t, float whole)
        {
            const float held = SlideGoGather * .5f + (SlideGoOut - SlideGoIn - SlideGoGather) + SlideGoLose * .5f;   // (s: what the window comes to)
            return (SlideFar - 1) * whole / held * Smooth(SlideGoIn, SlideGoIn + SlideGoGather, t) * (1 - Smooth(SlideGoOut, SlideGoOut + SlideGoLose, t));
        }

        // ------------------------------------------------------------------ procedural meshes
        struct MeshBuilder
        {
            DoorwaysLoadedDMesh Mesh;
            u32 Vertex(XMFLOAT3 p, XMFLOAT3 n, float u, float v)
            {
                DoorwaysDMeshVertex vertex{};
                vertex.Position = {p.x, p.y, p.z};
                vertex.Normal = {n.x, n.y, n.z};
                vertex.Tangent = {1, 0, 0, 1};
                vertex.TexCoord0 = {u, v};
                vertex.JointWeights[0] = 1;
                Mesh.Vertices.push_back(vertex);
                return static_cast<u32>(Mesh.Vertices.size() - 1);
            }
            void Tri(u32 a, u32 b, u32 c) { Mesh.Indices.insert(Mesh.Indices.end(), {a, b, c}); }
            void Quad(u32 a, u32 b, u32 c, u32 d) { Tri(a, b, c); Tri(a, c, d); }
            bool Upload(Renderer& renderer, DoorwaysGpuMesh& out, std::string& error)
            {
                Mesh.Submeshes = {{0, static_cast<u32>(Mesh.Indices.size()), 0, 0}};
                Mesh.MeshHeader.VertexCount = static_cast<u32>(Mesh.Vertices.size());
                Mesh.MeshHeader.IndexCount = static_cast<u32>(Mesh.Indices.size());
                Mesh.MeshHeader.SubmeshCount = 1;
                Mesh.MeshHeader.VertexStride = sizeof(DoorwaysDMeshVertex);
                return renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(Mesh, out, &error);
            }
        };

        bool BuildRing(Renderer& r, DoorwaysGpuMesh& out, std::string& e)
        {
            MeshBuilder b; const int n = 72;
            for (int i = 0; i <= n; ++i)
            {
                const float u = static_cast<float>(i) / n, a = u * 2 * Pi;
                b.Vertex({std::sin(a) * .55f, 0, std::cos(a) * .55f}, {0, 1, 0}, u, 0);
                b.Vertex({std::sin(a), 0, std::cos(a)}, {0, 1, 0}, u, 1);
                if (i) b.Quad(2 * i - 2, 2 * i - 1, 2 * i + 1, 2 * i);
            }
            return b.Upload(r, out, e);
        }
        bool BuildQuad(Renderer& r, DoorwaysGpuMesh& out, std::string& e, float y0)
        {   // XY plane, x -0.5..0.5, y y0..y0+1 (normal +Z)
            MeshBuilder b;
            b.Quad(b.Vertex({-.5f, y0, 0}, {0, 0, 1}, 0, 1), b.Vertex({.5f, y0, 0}, {0, 0, 1}, 1, 1),
                b.Vertex({.5f, y0 + 1, 0}, {0, 0, 1}, 1, 0), b.Vertex({-.5f, y0 + 1, 0}, {0, 0, 1}, 0, 0));
            return b.Upload(r, out, e);
        }
        bool BuildTri(Renderer& r, DoorwaysGpuMesh& out, std::string& e)
        {   // one triangle, (0,0,0) (1,0,0) (0,1,0), normal +Z: any other is it stretched and turned (Basis)
            MeshBuilder b;
            b.Tri(b.Vertex({0, 0, 0}, {0, 0, 1}, 0, 0), b.Vertex({1, 0, 0}, {0, 0, 1}, 1, 0), b.Vertex({0, 1, 0}, {0, 0, 1}, 0, 1));
            return b.Upload(r, out, e);
        }
        bool BuildSphere(Renderer& r, DoorwaysGpuMesh& out, std::string& e)
        {
            MeshBuilder b; const int rings = 12, segs = 20;
            for (int y = 0; y <= rings; ++y)
                for (int x = 0; x <= segs; ++x)
                {
                    const float v = static_cast<float>(y) / rings, u = static_cast<float>(x) / segs;
                    const float th = v * Pi, ph = u * 2 * Pi;
                    const XMFLOAT3 p{std::sin(th) * std::sin(ph), std::cos(th), std::sin(th) * std::cos(ph)};
                    b.Vertex(p, p, u, v);
                    if (y && x) { const u32 w = segs + 1, i = y * w + x; b.Quad(i - w - 1, i - w, i, i - 1); }
                }
            return b.Upload(r, out, e);
        }
        bool BuildCone(Renderer& r, DoorwaysGpuMesh& out, std::string& e)
        {   // base radius 1 at y=0, apex at y=1
            MeshBuilder b; const int n = 20;
            for (int i = 0; i <= n; ++i)
            {
                const float u = static_cast<float>(i) / n, a = u * 2 * Pi;
                const XMFLOAT3 nrm = Norm({std::sin(a), .7f, std::cos(a)});
                b.Vertex({std::sin(a), 0, std::cos(a)}, nrm, u, 0); b.Vertex({0, 1, 0}, nrm, u, 1);
                if (i) b.Tri(2 * i - 2, 2 * i, 2 * i - 1);
            }
            return b.Upload(r, out, e);
        }
        bool BuildBlade(Renderer& r, DoorwaysGpuMesh& out, std::string& e)
        {   // spectral sword, height 1 (grip -0.18..0, guard at 0, tip at 1); flat-shaded
            MeshBuilder b;
            const auto face = [&](std::initializer_list<XMFLOAT3> pts)
            {
                std::vector<XMFLOAT3> p(pts);
                const XMFLOAT3 n = Norm(Cross(Sub(p[1], p[0]), Sub(p[2], p[0])));
                std::vector<u32> id;
                for (std::size_t i = 0; i < p.size(); ++i) id.push_back(b.Vertex(p[i], n, static_cast<float>(i & 1), p[i].y));
                for (std::size_t i = 1; i + 1 < id.size(); ++i) b.Tri(id[0], id[i], id[i + 1]);
            };
            const float w = .085f, t = .018f, shoulder = .86f;
            const XMFLOAT3 L0{-w, 0, 0}, R0{w, 0, 0}, F0{0, 0, t}, B0{0, 0, -t};
            const XMFLOAT3 L1{-w * .8f, shoulder, 0}, R1{w * .8f, shoulder, 0}, F1{0, shoulder, t}, B1{0, shoulder, -t}, Tip{0, 1, 0};
            face({L0, F0, F1, L1}); face({F0, R0, R1, F1}); face({R0, B0, B1, R1}); face({B0, L0, L1, B1});
            face({L1, F1, Tip}); face({F1, R1, Tip}); face({R1, B1, Tip}); face({B1, L1, Tip});
            const auto box = [&](XMFLOAT3 lo, XMFLOAT3 hi)
            {
                const XMFLOAT3 c[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                                       {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
                face({c[0], c[1], c[2], c[3]}); face({c[5], c[4], c[7], c[6]}); face({c[4], c[0], c[3], c[7]});
                face({c[1], c[5], c[6], c[2]}); face({c[3], c[2], c[6], c[7]}); face({c[4], c[5], c[1], c[0]});
            };
            box({-.24f, -.025f, -.035f}, {.24f, .025f, .035f});    // eagle-wing guard
            box({-.022f, -.19f, -.022f}, {.022f, -.025f, .022f});  // grip
            box({-.04f, -.23f, -.04f}, {.04f, -.19f, .04f});       // pommel
            return b.Upload(r, out, e);
        }

        // Row-vector matrix whose local axes are the given world vectors.
        XMMATRIX Basis(XMFLOAT3 x, XMFLOAT3 y, XMFLOAT3 z, XMFLOAT3 p)
        {
            return XMMATRIX(x.x, x.y, x.z, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, p.x, p.y, p.z, 1);
        }

        // ------------------------------------------------------------------ her attack styles (owner, 2026-11-20)
        // "for now this attacks are so slow and hard to control, also curent right/left clicks attacks contain several
        // different atacks which concates a combo itself, but we could do better, i want you to examine soullike games and
        // get me at least 5 different variants for each click", and then "try at least each attack to be 3x faster then
        // current one". Ten styles of one move for each button: a click is that move, the same every time, never the next of
        // a row. As in the games they are taken from, a move turns her onto the creature it is aimed at as it begins (the
        // soft lock: StyleTrackRate until TrackTo), carries her in to it and stops short of it (Lunge, Gap), gives up its end
        // to whatever is pressed next (Free) and keeps a click made during it for a moment (StyleBuffer). Each is made at
        // load from pieces of her clips (MakeSpliced). Her cuts: the blow 0.27 to 0.47 s after the click, the next cut 0.14 s
        // after it, the cut over 0.82 s after it. Here every first blow by 0.116 s, the next press taken by 0.163 s, the move
        // over by 0.388 s (a move of n blows against n of her cuts in a row). The Character Lab gives them keys 0 to 9.
        struct StyleKnot { float Out, In; };                      // t s into the move: s into the clip
        struct StyleTrack { const char* Clip; std::vector<StyleKnot> Knots; };
        struct StyleHit
        {
            float At, Reach, Arc;      // s into the move; m; degrees either side of her facing (Shape 1: Reach long, Arc m either side)
            int Damage; float Push, Launch;
            int Shape;                 // 0 before her, 1 a line along the ground before her
            float Stop, Shake, Ring;   // the moment held (s) and the view shaken when it strikes; a ring on the ground (m across)
            int Hand;                  // the blade that strikes (0 left, 1 right, 2 both)
        };
        struct StyleMove
        {
            const wchar_t* Name; const wchar_t* What; const char* Clip;
            float Seconds, Free, BlendIn, TrackTo;     // all of it; free for what comes next from; into it from what was; turned onto its creature until
            float Lunge, LungeFrom, LungeTo, Gap;      // at most so far in (m), over that time, stopping so far short of the creature's edge
            float Back, BackFrom, BackTo;              // a step or hop back (m), over that time
            float Hop, HopFrom, HopTo;                 // her feet off the ground (m at the highest), over that time
            float Spin, SpinFrom, SpinTo;              // turned round (radians, to her right), over that time
            std::vector<StyleHit> Hits;
            std::vector<StyleTrack> Tracks;
            float LastHit() const { return Hits.empty() ? 0.f : Hits.back().At; }
        };
        struct StyleSpec { const wchar_t* Name; const wchar_t* Like; StyleMove Move[2]; };
        // The owner, 2026-11-20: "make those 10 attacks in character lab 1.5x slower". Every time of every move (its length,
        // when it is free, its blend, its turn, lunge, step back, hop and spin, its blows, and where it is in her clips) is
        // the table's times StyleSlow; the clips' own times, the distances and a blow's held moment are as they were.
        constexpr float StyleSlow = 1.5f;
        std::vector<StyleSpec> Slowed(std::vector<StyleSpec> all)
        {
            for (auto& st : all)
                for (auto& m : st.Move)
                {
                    for (float* f : {&m.Seconds, &m.Free, &m.BlendIn, &m.TrackTo, &m.LungeFrom, &m.LungeTo, &m.BackFrom, &m.BackTo,
                                     &m.HopFrom, &m.HopTo, &m.SpinFrom, &m.SpinTo}) *f *= StyleSlow;
                    for (auto& h : m.Hits) h.At *= StyleSlow;
                    for (auto& tr : m.Tracks) for (auto& k : tr.Knots) k.Out *= StyleSlow;
                }
            return all;
        }
        const std::vector<StyleSpec>& Styles()
        {   // (written out by a scratch tool, styles.py, from its table: it checks every blow against her clips, the blade fast
            // and before her as it lands, and every move against her cuts, three times as fast)
            static const std::vector<StyleSpec> all = Slowed({
            {L"FALCON", L"twin daggers: Bloodborne's Blade of Mercy, Elden Ring's daggers", {
                {.Name = L"TALON FLICK", .What = L"the right blade whipped down across her", .Clip = "Attack_S0_L",
                 .Seconds = 0.3f, .Free = 0.12f, .BlendIn = 0.035f, .TrackTo = 0.05f,
                 .Lunge = 0.5f, .LungeFrom = 0, .LungeTo = 0.06f, .Gap = 0.9f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.075f, 1.8f, 45.f, 30, 1.f, 0, 0, 0, 0, 0, 1}},
                 .Tracks = {{"Attack_R1", {{0, 0.5f}, {0.035f, 0.56f}, {0.07f, 0.645f}, {0.09f, 0.672f}, {0.12f, 0.72f}, {0.16f, 0.8f}, {0.24f, 0.95f}}}, {"Idle", {{0.12f, 0}, {0.3f, 0.18f}}}}},
                {.Name = L"TWIN FANG", .What = L"both blades in an X: the right down across her, the left back through it", .Clip = "Attack_S0_R",
                 .Seconds = 0.36f, .Free = 0.22f, .BlendIn = 0.035f, .TrackTo = 0.1f,
                 .Lunge = 1.f, .LungeFrom = 0, .LungeTo = 0.12f, .Gap = 0.9f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.075f, 2.f, 45.f, 35, 1.f, 0, 0, 0, 0, 0, 1}, {0.15f, 2.f, 60.f, 45, 2.f, 0, 0, 0, 0, 0, 0}},
                 .Tracks = {{"Attack_R1", {{0, 0.5f}, {0.035f, 0.56f}, {0.07f, 0.645f}, {0.09f, 0.672f}, {0.13f, 0.73f}}}, {"Attack_L1", {{0.09f, 0.79f}, {0.13f, 0.845f}, {0.15f, 0.875f}, {0.175f, 0.905f}, {0.22f, 0.97f}, {0.3f, 1.08f}}}, {"Idle", {{0.2f, 0}, {0.36f, 0.16f}}}}}}},
            {L"MOONBLADE", L"a straight sword: Dark Souls' and Elden Ring's straight swords", {
                {.Name = L"CRESCENT", .What = L"a wide level sweep of the left blade, from her left round to her right", .Clip = "Attack_S1_L",
                 .Seconds = 0.34f, .Free = 0.14f, .BlendIn = 0.04f, .TrackTo = 0.06f,
                 .Lunge = 0.6f, .LungeFrom = 0, .LungeTo = 0.08f, .Gap = 0.9f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.09f, 2.f, 75.f, 45, 1.5f, 0, 0, 0, 0, 0, 0}},
                 .Tracks = {{"Attack_L1", {{0, 0.74f}, {0.04f, 0.8f}, {0.075f, 0.85f}, {0.09f, 0.875f}, {0.11f, 0.905f}, {0.15f, 0.96f}, {0.22f, 1.03f}, {0.32f, 1.15f}}}, {"Idle", {{0.14f, 0}, {0.34f, 0.2f}}}}},
                {.Name = L"MOONFALL", .What = L"the right blade down from high, and on down into the ground", .Clip = "Attack_S1_R",
                 .Seconds = 0.5f, .Free = 0.28f, .BlendIn = 0.035f, .TrackTo = 0.16f,
                 .Lunge = 1.4f, .LungeFrom = 0, .LungeTo = 0.17f, .Gap = 0.9f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.075f, 2.2f, 50.f, 30, 1.f, 0, 0, 0, 0, 0, 1}, {0.2f, 2.3f, 45.f, 85, 3.f, 0, 0, 0.06f, 0.2f, 0, 1}},
                 .Tracks = {{"Attack_R1", {{0, 0.5f}, {0.035f, 0.56f}, {0.07f, 0.645f}, {0.09f, 0.672f}, {0.12f, 0.72f}, {0.16f, 0.78f}}}, {"Attack_R_Step", {{0.1f, 0.74f}, {0.15f, 0.82f}, {0.18f, 0.87f}, {0.2f, 0.895f}, {0.23f, 0.94f}, {0.3f, 1.05f}, {0.4f, 1.25f}}}, {"Idle", {{0.3f, 0}, {0.5f, 0.2f}}}}}}},
            {L"IAIDO", L"a katana: Sekiro; Elden Ring's katanas and Unsheathe", {
                {.Name = L"FLASH STEP", .What = L"a blink in, turning, and the cut as she arrives", .Clip = "Attack_S2_R",
                 .Seconds = 0.36f, .Free = 0.14f, .BlendIn = 0.04f, .TrackTo = 0.05f,
                 .Lunge = 2.8f, .LungeFrom = 0, .LungeTo = 0.09f, .Gap = 0.8f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.1f, 2.2f, 55.f, 80, 2.5f, 0, 0, 0.05f, 0.15f, 0, 1}},
                 .Tracks = {{"Attack_R_Run", {{0, 0.47f}, {0.04f, 0.53f}, {0.08f, 0.6f}, {0.1f, 0.65f}, {0.12f, 0.69f}, {0.16f, 0.75f}, {0.24f, 0.85f}}}, {"Idle", {{0.18f, 0}, {0.36f, 0.18f}}}}},
                {.Name = L"DRAW CUT", .What = L"from high over her left shoulder, down and level across at the waist", .Clip = "Attack_S2_L",
                 .Seconds = 0.32f, .Free = 0.13f, .BlendIn = 0.035f, .TrackTo = 0.05f,
                 .Lunge = 0.7f, .LungeFrom = 0, .LungeTo = 0.07f, .Gap = 1.f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.09f, 2.1f, 50.f, 45, 1.5f, 0, 0, 0, 0, 0, 0}},
                 .Tracks = {{"Attack_L_Step", {{0, 0.96f}, {0.025f, 1.f}, {0.045f, 1.04f}, {0.06f, 1.075f}, {0.075f, 1.15f}, {0.09f, 1.185f}, {0.11f, 1.22f}, {0.15f, 1.27f}, {0.22f, 1.33f}, {0.3f, 1.4f}}}, {"Idle", {{0.13f, 0}, {0.32f, 0.19f}}}}}}},
            {L"STING", L"a rapier: Lies of P's rapiers; Elden Ring's thrusting swords and estocs", {
                {.Name = L"NEEDLE", .What = L"two jabs of the left blade, far before her", .Clip = "Attack_S3_L",
                 .Seconds = 0.34f, .Free = 0.22f, .BlendIn = 0.04f, .TrackTo = 0.14f,
                 .Lunge = 0.4f, .LungeFrom = 0, .LungeTo = 0.07f, .Gap = 1.3f, .Back = -0.3f, .BackFrom = 0.1f, .BackTo = 0.15f,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.07f, 2.6f, 20.f, 25, 0.5f, 0, 0, 0, 0, 0, 0}, {0.155f, 2.6f, 20.f, 25, 0.5f, 0, 0, 0, 0, 0, 0}},
                 .Tracks = {{"Attack_L2", {{0, 0.96f}, {0.04f, 1.03f}, {0.07f, 1.09f}, {0.085f, 1.095f}, {0.11f, 1.02f}, {0.135f, 1.04f}, {0.155f, 1.09f}, {0.17f, 1.095f}, {0.21f, 1.1f}, {0.3f, 1.105f}}}, {"Idle", {{0.18f, 0}, {0.34f, 0.16f}}}}},
                {.Name = L"FLECHE", .What = L"she runs in low and drives the blade through it", .Clip = "Attack_S3_R",
                 .Seconds = 0.38f, .Free = 0.14f, .BlendIn = 0.04f, .TrackTo = 0.06f,
                 .Lunge = 3.f, .LungeFrom = 0, .LungeTo = 0.1f, .Gap = 1.2f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.1f, 2.8f, 20.f, 85, 3.f, 0, 0, 0.06f, 0.15f, 0, 0}},
                 .Tracks = {{"Attack_L2", {{0, 0.85f}, {0.04f, 0.95f}, {0.08f, 1.04f}, {0.1f, 1.08f}, {0.115f, 1.095f}, {0.15f, 1.1f}, {0.3f, 1.105f}}}, {"Idle", {{0.16f, 0}, {0.38f, 0.22f}}}}}}},
            {L"WHIRL", L"a twinblade: Elden Ring's twinblades, the Godskin Peeler", {
                {.Name = L"PINWHEEL", .What = L"one turn with both blades out, cutting all round her", .Clip = "Attack_S4_L",
                 .Seconds = 0.36f, .Free = 0.14f, .BlendIn = 0.03f, .TrackTo = 0.04f,
                 .Lunge = 0.6f, .LungeFrom = 0, .LungeTo = 0.08f, .Gap = 0.7f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.105f, 2.1f, 180.f, 40, 1.5f, 0, 0, 0, 0, 0, 1}},
                 .Tracks = {{"Attack_R4", {{0, 0.3f}, {0.035f, 0.42f}, {0.065f, 0.6f}, {0.09f, 0.78f}, {0.105f, 0.88f}, {0.12f, 0.95f}, {0.16f, 1.05f}, {0.23f, 1.2f}, {0.32f, 1.32f}}}, {"Idle", {{0.16f, 0}, {0.36f, 0.2f}}}}},
                {.Name = L"CYCLONE", .What = L"turn after turn, three times round", .Clip = "Attack_S4_R",
                 .Seconds = 0.62f, .Free = 0.46f, .BlendIn = 0.04f, .TrackTo = 0.04f,
                 .Lunge = 1.f, .LungeFrom = 0.03f, .LungeTo = 0.42f, .Gap = 0.7f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = XM_2PI, .SpinFrom = 0.03f, .SpinTo = 0.45f,
                 .Hits = {{0.1f, 2.2f, 180.f, 35, 1.5f, 0, 0, 0, 0, 0, 1}, {0.25f, 2.2f, 180.f, 35, 1.5f, 0, 0, 0, 0, 0, 1}, {0.4f, 2.2f, 180.f, 35, 1.5f, 0, 0, 0, 0, 0, 1}},
                 .Tracks = {{"Attack_R4", {{0, 0.3f}, {0.45f, 1.2f}, {0.55f, 1.42f}}}, {"Idle", {{0.45f, 0}, {0.62f, 0.17f}}}}}}},
            {L"KESTREL", L"aerial: Devil May Cry's launcher; Elden Ring's jumping attacks", {
                {.Name = L"UPDRAFT", .What = L"a rising cut that throws it up, her feet leaving the ground", .Clip = "Attack_S5_L",
                 .Seconds = 0.32f, .Free = 0.14f, .BlendIn = 0.04f, .TrackTo = 0.05f,
                 .Lunge = 0.5f, .LungeFrom = 0, .LungeTo = 0.07f, .Gap = 0.9f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0.35f, .HopFrom = 0.07f, .HopTo = 0.3f, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.09f, 2.f, 60.f, 40, 0.5f, 6.f, 0, 0, 0, 0, 1}},
                 .Tracks = {{"Attack_R2", {{0, 0.7f}, {0.04f, 0.76f}, {0.075f, 0.83f}, {0.09f, 0.865f}, {0.11f, 0.9f}, {0.14f, 0.93f}, {0.24f, 0.96f}}}, {"Idle", {{0.12f, 0}, {0.32f, 0.2f}}}}},
                {.Name = L"EAGLE DIVE", .What = L"a rising cut that takes her up, and both blades down into it as she comes down", .Clip = "Attack_S5_R",
                 .Seconds = 0.52f, .Free = 0.3f, .BlendIn = 0.04f, .TrackTo = 0.2f,
                 .Lunge = 1.8f, .LungeFrom = 0.1f, .LungeTo = 0.23f, .Gap = 0.7f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0.25f, .HopFrom = 0.03f, .HopTo = 0.2f, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.09f, 2.f, 60.f, 40, 0.5f, 4.f, 0, 0, 0, 0, 1}, {0.235f, 2.5f, 180.f, 95, 3.f, 0, 0, 0.07f, 0.25f, 2.6f, 2}},
                 .Tracks = {{"Attack_R2", {{0, 0.7f}, {0.04f, 0.76f}, {0.075f, 0.83f}, {0.09f, 0.865f}, {0.11f, 0.89f}, {0.14f, 0.92f}, {0.18f, 0.95f}}}, {"Attack_Jump", {{0.1f, 1.f}, {0.16f, 1.1f}, {0.2f, 1.18f}, {0.235f, 1.255f}, {0.26f, 1.32f}, {0.32f, 1.45f}, {0.44f, 1.66f}}}, {"Idle", {{0.3f, 0}, {0.52f, 0.22f}}}}}}},
            {L"TEMPEST", L"dual curved swords: Elden Ring's dual curved swords; Sekiro's flurries", {
                {.Name = L"TWIN SLASH", .What = L"a cut down from high on her left, and straight back across her", .Clip = "Attack_S6_L",
                 .Seconds = 0.4f, .Free = 0.26f, .BlendIn = 0.035f, .TrackTo = 0.14f,
                 .Lunge = 0.6f, .LungeFrom = 0, .LungeTo = 0.07f, .Gap = 0.9f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.08f, 2.f, 60.f, 30, 1.f, 0, 0, 0, 0, 0, 0}, {0.195f, 2.f, 70.f, 35, 1.5f, 0, 0, 0, 0, 0, 0}},
                 .Tracks = {{"Attack_L3", {{0, 0.7f}, {0.035f, 0.75f}, {0.065f, 0.8f}, {0.08f, 0.83f}, {0.1f, 0.88f}, {0.12f, 0.93f}, {0.16f, 1.4f}, {0.18f, 1.46f}, {0.195f, 1.51f}, {0.21f, 1.55f}, {0.24f, 1.6f}, {0.34f, 1.645f}}}, {"Idle", {{0.22f, 0}, {0.4f, 0.18f}}}}},
                {.Name = L"GALE RUSH", .What = L"running in with two cuts, one low and one across", .Clip = "Attack_S6_R",
                 .Seconds = 0.42f, .Free = 0.28f, .BlendIn = 0.035f, .TrackTo = 0.16f,
                 .Lunge = 2.4f, .LungeFrom = 0, .LungeTo = 0.19f, .Gap = 0.9f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.085f, 2.1f, 60.f, 45, 1.5f, 0, 0, 0, 0, 0, 0}, {0.21f, 2.1f, 70.f, 55, 2.f, 0, 0, 0, 0, 0, 0}},
                 .Tracks = {{"Attack_L_Run", {{0, 0.62f}, {0.035f, 0.67f}, {0.07f, 0.735f}, {0.085f, 0.76f}, {0.1f, 0.8f}, {0.12f, 0.85f}, {0.165f, 1.4f}, {0.19f, 1.48f}, {0.21f, 1.54f}, {0.225f, 1.57f}, {0.26f, 1.62f}, {0.36f, 1.655f}}}, {"Idle", {{0.24f, 0}, {0.42f, 0.18f}}}}}}},
            {L"COLOSSUS", L"weight: Elden Ring's greatswords and colossal swords", {
                {.Name = L"HEAVY CLEAVE", .What = L"the right blade brought down from high with all her weight", .Clip = "Attack_S7_L",
                 .Seconds = 0.38f, .Free = 0.14f, .BlendIn = 0.045f, .TrackTo = 0.06f,
                 .Lunge = 0.8f, .LungeFrom = 0, .LungeTo = 0.09f, .Gap = 0.9f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.1f, 2.2f, 55.f, 70, 3.5f, 0, 0, 0.06f, 0.15f, 0, 1}},
                 .Tracks = {{"Attack_R1", {{0, 0.46f}, {0.045f, 0.55f}, {0.08f, 0.62f}, {0.1f, 0.66f}, {0.12f, 0.69f}, {0.16f, 0.75f}, {0.24f, 0.85f}, {0.34f, 0.98f}}}, {"Idle", {{0.16f, 0}, {0.38f, 0.22f}}}}},
                {.Name = L"MOUNTAIN BREAKER", .What = L"a cut across, then a hop and both blades into the ground: it shakes round her", .Clip = "Attack_S7_R",
                 .Seconds = 0.54f, .Free = 0.3f, .BlendIn = 0.04f, .TrackTo = 0.14f,
                 .Lunge = 1.2f, .LungeFrom = 0.1f, .LungeTo = 0.23f, .Gap = 0.7f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.08f, 2.2f, 55.f, 50, 2.f, 0, 0, 0, 0, 0, 1}, {0.235f, 3.f, 180.f, 120, 4.f, 3.f, 0, 0.09f, 0.35f, 3.2f, 2}},
                 .Tracks = {{"Attack_R1", {{0, 0.46f}, {0.04f, 0.55f}, {0.075f, 0.645f}, {0.09f, 0.672f}, {0.12f, 0.72f}, {0.16f, 0.78f}}}, {"Attack_Jump", {{0.1f, 1.f}, {0.16f, 1.08f}, {0.2f, 1.16f}, {0.235f, 1.255f}, {0.26f, 1.31f}, {0.32f, 1.42f}, {0.46f, 1.66f}}}, {"Idle", {{0.34f, 0}, {0.54f, 0.2f}}}}}}},
            {L"VIPER", L"low and long: Elden Ring's Ashes of War that send a wave along the ground", {
                {.Name = L"LOW FANG", .What = L"a cut down to its legs", .Clip = "Attack_S8_L",
                 .Seconds = 0.32f, .Free = 0.13f, .BlendIn = 0.035f, .TrackTo = 0.05f,
                 .Lunge = 0.5f, .LungeFrom = 0, .LungeTo = 0.07f, .Gap = 0.9f, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.085f, 2.f, 55.f, 40, 1.f, 0, 0, 0, 0, 0, 0}},
                 .Tracks = {{"Attack_L_Run", {{0, 0.6f}, {0.035f, 0.66f}, {0.07f, 0.73f}, {0.085f, 0.76f}, {0.1f, 0.79f}, {0.13f, 0.83f}, {0.18f, 0.86f}, {0.26f, 0.92f}}}, {"Idle", {{0.13f, 0}, {0.32f, 0.19f}}}}},
                {.Name = L"GROUND WAVE", .What = L"the blade struck into the ground sends a wave along it, 7 m before her", .Clip = "Attack_S8_R",
                 .Seconds = 0.38f, .Free = 0.14f, .BlendIn = 0.04f, .TrackTo = 0.05f,
                 .Lunge = 0, .LungeFrom = 0, .LungeTo = 0, .Gap = 0, .Back = 0, .BackFrom = 0, .BackTo = 0,
                 .Hop = 0, .HopFrom = 0, .HopTo = 0, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.095f, 7.f, 1.1f, 70, 2.f, 0, 1, 0, 0.15f, 0, 1}},
                 .Tracks = {{"Attack_R_Step", {{0, 0.74f}, {0.04f, 0.8f}, {0.08f, 0.87f}, {0.095f, 0.895f}, {0.11f, 0.92f}, {0.15f, 0.98f}, {0.22f, 1.06f}, {0.32f, 1.2f}}}, {"Idle", {{0.16f, 0}, {0.38f, 0.22f}}}}}}},
            {L"RAPTOR", L"hit and run: Bloodborne's quickstep; Sekiro's step-ins", {
                {.Name = L"HIT AND FADE", .What = L"a cut down from her left, and a hop back out of reach", .Clip = "Attack_S9_L",
                 .Seconds = 0.34f, .Free = 0.16f, .BlendIn = 0.035f, .TrackTo = 0.05f,
                 .Lunge = 0, .LungeFrom = 0, .LungeTo = 0, .Gap = 0, .Back = 1.2f, .BackFrom = 0.08f, .BackTo = 0.17f,
                 .Hop = 0.2f, .HopFrom = 0.08f, .HopTo = 0.17f, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.08f, 2.f, 60.f, 40, 1.5f, 0, 0, 0, 0, 0, 0}},
                 .Tracks = {{"Attack_L3", {{0, 0.7f}, {0.035f, 0.75f}, {0.065f, 0.8f}, {0.08f, 0.83f}, {0.1f, 0.88f}, {0.12f, 0.92f}, {0.16f, 0.97f}, {0.24f, 1.05f}}}, {"Idle", {{0.12f, 0}, {0.34f, 0.22f}}}}},
                {.Name = L"SWALLOW DART", .What = L"a dash in under its guard with a rising cut, and a hop back out", .Clip = "Attack_S9_R",
                 .Seconds = 0.38f, .Free = 0.16f, .BlendIn = 0.04f, .TrackTo = 0.06f,
                 .Lunge = 2.6f, .LungeFrom = 0, .LungeTo = 0.09f, .Gap = 0.9f, .Back = 1.2f, .BackFrom = 0.11f, .BackTo = 0.2f,
                 .Hop = 0.15f, .HopFrom = 0.11f, .HopTo = 0.2f, .Spin = 0, .SpinFrom = 0, .SpinTo = 0,
                 .Hits = {{0.1f, 2.1f, 60.f, 75, 2.5f, 0, 0, 0.05f, 0.12f, 0, 1}},
                 .Tracks = {{"Attack_R2", {{0, 0.7f}, {0.04f, 0.76f}, {0.075f, 0.83f}, {0.1f, 0.87f}, {0.12f, 0.9f}, {0.15f, 0.93f}, {0.24f, 0.96f}}}, {"Idle", {{0.14f, 0}, {0.38f, 0.24f}}}}}}}
            });
            return all;
        }
        constexpr float StyleTrackRate = 2000.f;   // degrees a second she turns onto a move's creature while the move lets her
        constexpr float StyleSeek = 6.f;           // m from her (its edge) a creature may be to be aimed at
        constexpr float StyleCone = 75.f;          // degrees either side of her facing it may be (and within StyleNear m of her, StyleConeNear)
        constexpr float StyleConeNear = 150.f, StyleNear = 2.5f;
        constexpr float StyleBuffer = .3f;         // s a click made during a move is kept for when the move is free
        constexpr float StyleDrawReach = .06f, StyleDrawOn = .05f;   // out of standing at ease: her hands to the handles, and on into the move (s)
        constexpr float StyleFinishAfter = .05f;   // s after a move's last blow a doomed creature may be finished
        constexpr float StyleTrailSeconds = .12f;  // how long a blade's trail of light lasts
        constexpr float StyleWaveSeconds = .3f;    // GROUND WAVE's sparks run out along its line in this time

        // Her combos (owner, 2026-11-20): "number 2's LMB and RMB ... set 2's RMB on LMB and 2's LMB on RMB! and then with
        // 7's RMB and 9's RMB and 2's RMB and 4's LMB and 8's LMB create 5 different combinations - then let me use them in
        // characterlab with 1,2,3,4,5 numbers"; and "we dont need 2's skills additionally in combos because itself clicks
        // has those attacks and those are which they both should be starting". So a click begins with style 2's move for
        // its button (LMB FLASH STEP, RMB DRAW CUT), and the same button clicked again during the move or within
        // ComboWindow s after it goes on down that button's string: PINWHEEL, MOUNTAIN BREAKER, LOW FANG and SWALLOW DART
        // in each combo's own order. The other button begins its own string; after a string's last move a click begins it
        // again. A move given up for a jump, a skill or the barrier ends the string; one given up for her way (W, A, S, D)
        // does not (the owner: "while moving pressing W/S/A/D combo is not available ... fix it"), so that she goes on
        // down it on the move. Of the five first made, the owner kept the first's LMB string and the fourth's RMB string:
        // "i like 1's LMB combo and 4's RMB , now set those".
        struct MoveRef { int Style, Button; };
        constexpr MoveRef FlashStep{2, 0}, DrawCut{2, 1}, Pinwheel{4, 0}, MountainBreaker{7, 1}, LowFang{8, 0}, SwallowDart{9, 1};
        // (and the rest of the twenty, for the nine combos of her flair)
        constexpr MoveRef TalonFlick{0, 0}, TwinFang{0, 1}, Crescent{1, 0}, Moonfall{1, 1}, Needle{3, 0}, Fleche{3, 1}, Cyclone{4, 1}, Updraft{5, 0},
                          EagleDive{5, 1}, TwinSlash{6, 0}, GaleRush{6, 1}, HeavyCleave{7, 0}, GroundWave{8, 1}, HitAndFade{9, 0};
        constexpr float ComboWindow = .4f;
        struct ComboSpec { const wchar_t* Name; const wchar_t* What; std::vector<MoveRef> String[2]; };
        const std::vector<ComboSpec>& Combos()
        {
            static const std::vector<ComboSpec> all{
                {L"HER COMBO", L"in and round, the ground broken; in and out, and round", {
                    {FlashStep, Pinwheel, MountainBreaker},
                    {DrawCut, SwallowDart, Pinwheel}}},
                // The nine of her flair (NyxaraFlair; owner, 2026-11-25), 1 + gear * 3 + style. In each gear the first is
                // fast and sharp (many quick blows, a rush through at the end), the second heavy (few blows, the ground
                // broken), the third flowing (turns, one move running into the next): the third is the game's, as her
                // gear is put on ("i like 3'd most - so add those to current nyxara's ingame version"); the Character Lab
                // plays all nine.
                {L"GALE EDGE", L"quick cuts and a rush through; in, the draw, and out again", {
                    {TalonFlick, TwinFang, TwinSlash, GaleRush},
                    {FlashStep, DrawCut, SwallowDart}}},
                {L"STONEBREAKER", L"a cleave, a sweep, the ground broken; down from high, up, and down on it", {
                    {HeavyCleave, Crescent, MountainBreaker},
                    {Moonfall, Updraft, EagleDive}}},
                {L"SKY WHEEL", L"round, a sweep, and round three times; in and out, two cuts, and round", {
                    {Pinwheel, Crescent, Cyclone},
                    {SwallowDart, TwinSlash, Pinwheel}}},
                {L"CINDER RAZOR", L"a thrust, a flick, the X, a thrust through; in, low, and out again", {
                    {Needle, TalonFlick, TwinFang, Fleche},
                    {FlashStep, LowFang, SwallowDart}}},
                {L"FORGE HAMMER", L"a cleave, down from high, the ground broken; fire along the ground, up, and down on it", {
                    {HeavyCleave, Moonfall, MountainBreaker},
                    {GroundWave, Updraft, EagleDive}}},
                {L"FIRE DANCE", L"two cuts, round, a rush, and round three times; a sweep, back, in, and round", {
                    {TwinSlash, Pinwheel, GaleRush, Cyclone},
                    {Crescent, HitAndFade, SwallowDart, Pinwheel}}},
                {L"SILVER THREAD", L"the draw, a thrust, the X, and through it; a thrust through, back, and through again", {
                    {DrawCut, Needle, TwinFang, FlashStep},
                    {Fleche, HitAndFade, FlashStep}}},
                {L"GLACIER", L"a sweep, a cleave, down from high; low, ice along the ground, the ground broken", {
                    {Crescent, HeavyCleave, Moonfall},
                    {LowFang, GroundWave, MountainBreaker}}},
                {L"MOON WRAITH", L"a sweep, two cuts, and round three times; in, round, out, and down from the air", {
                    {Crescent, TwinSlash, Cyclone},
                    {FlashStep, Pinwheel, SwallowDart, EagleDive}}},
            };
            return all;
        }
        const StyleMove& RefMove(MoveRef r) { return Styles()[static_cast<size_t>(r.Style)].Move[r.Button]; }
        // A trial FLASH STEP, for the Character Lab only (SetHiggsfieldTrial; owner, 2026-11-22: "use this animation now as
        // a reference and improve our nyxara version's combos ... only in character lab yet"), after Higgsfield's test video
        // (art/nyxara/higgsfield_nyxara/flash_step_kling_test1.mp4): the sword raised high behind her before she goes, the
        // cut as she arrives, then down into a low crouch, swords out wide and wings open, before she rises into her guard;
        // a streak of blue light where she went and a burst where the blade bites (Update). Its blow, reach, lunge and the
        // moment it may be followed are FLASH STEP's; only its end is longer (0.9 s, the crouch held), and its blow stops the
        // moment longer and shakes the view more (owner, 2026-11-22: "i didnt see any different").
        constexpr XMFLOAT4 TrialBlue{.3f, .62f, 1.f, 1};
        // (and every attack, move and skill of hers there plays at half pace, SetHiggsfieldTrial's: owner, 2026-11-22, "slow
        // down each skill at least twice"; the game's own pace is untouched)
        const StyleMove& FlashStepTrial()
        {
            static const StyleMove m = []
            {
                StyleMove t = RefMove(FlashStep);
                t.Clip = "Attack_S2_R_HF"; t.Seconds = .9f;
                t.Tracks = {{"Attack_R_Run", {{0, .38f}, {.03f, .44f}, {.07f, .55f}, {.1f, .62f}, {.13f, .68f}, {.17f, .74f}}},
                            {"Attack_Jump", {{.13f, 1.44f}, {.18f, 1.5f}, {.5f, 1.62f}, {.62f, 1.8f}, {.74f, 1.95f}}},
                            {"Idle", {{.66f, 0}, {.9f, .24f}}}};
                return t;
            }();
            return m;
        }

        // Her wing shield for E (owner, 2026-11-20: of ten made to try, "0 is good - keep it only": COCOON. "lets make different skill for her E - lets make E close her wings around
        // her and wings protect her, remove the effect now E has and it's effect should be blue bright lights around her
        // wings which helps her when her wings are closed around her with E skill - make 10 different version of that idea
        // and then i will test them in characterlab"). While E is held her wings close round her (SwingWings: each of a
        // wing's three bones turned Back radians forward (-) or back and Flap radians down (-) or up, on top of what they
        // do, closing at Close a second), and blue lights play about them (Draw). A blow from within Cover of her facing
        // (the cosine of the angle off it: -1.1 all round) is stopped, and knocks her back Jolt radians. Some hold her body
        // in a moment of one of her clips (At s into Clip, swaying Sway s of it either way over Breath s, raised into in
        // Raise s; made at load as the clip Guard_<n>); the others keep her guard stance.
        // The lights (Kind): 0 running along her wings; 1 going round her; 2 at her wing tips; 3 rising from her wings as
        // flames; 4 waves from her shoulders to the tips. Lights of them, Size m across, Spread m about where they are,
        // Speed (Kind's own: along, round, up), Pulse a second, Twinkle (0 steady .. 1 flickering), Motes sparks a second.
        struct GuardSpec
        {
            const wchar_t* Name; const wchar_t* What;
            const char* Clip; float At, Sway, Breath, Raise, Jolt;
            float Back[3], Flap[3], Close, Cover;
            int Kind, Lights; float Size, Spread, Speed, Pulse, Twinkle, Motes;
            XMFLOAT4 Color;
        };
        const std::vector<GuardSpec>& Guards()
        {
            constexpr XMFLOAT4 Sky{.2f, .55f, 1.f, 1}, Azure{.1f, .38f, 1.f, 1}, Ice{.4f, .78f, 1.f, 1}, Deep{.08f, .22f, 1.f, 1};
            static const std::vector<GuardSpec> all{
                {L"COCOON", L"both wings wrapped right round her, their tips crossing before her; lights run along them",
                 nullptr, 0, 0, 0, 0, .06f, {-1.25f, -.85f, -.6f}, {-.25f, -.1f, 0}, 9, -1.1f, 0, 18, .32f, .12f, .25f, 1.2f, .2f, 6, Sky},
            };
            return all;
        }
        std::string GuardClip(int g) { return "Guard_" + std::to_string(g); }
        // Where in her clip a move is, t s into it: a monotone cubic through its knots (Fritsch and Carlson; the ends as
        // SciPy's PchipInterpolator has them), so that the clip goes on between knots without overshoot, and back where they go back
        float StyleWarp(const std::vector<StyleKnot>& k, float t)
        {
            const size_t n = k.size();
            if (n == 0) return 0;
            if (n == 1 || t <= k.front().Out) return k.front().In;
            if (t >= k.back().Out) return k.back().In;
            const auto h = [&](size_t j) { return (std::max)(k[j + 1].Out - k[j].Out, 1e-5f); };
            const auto d = [&](size_t j) { return (k[j + 1].In - k[j].In) / h(j); };
            const auto edge = [](float h0, float h1, float m0, float m1)
            {
                const float e = ((2 * h0 + h1) * m0 - h0 * m1) / (h0 + h1);
                if (e * m0 <= 0) return 0.f;
                if (m0 * m1 < 0 && std::abs(e) > 3 * std::abs(m0)) return 3 * m0;
                return e;
            };
            const auto slope = [&](size_t j) -> float
            {
                if (n == 2) return d(0);
                if (j == 0) return edge(h(0), h(1), d(0), d(1));
                if (j == n - 1) return edge(h(n - 2), h(n - 3), d(n - 2), d(n - 3));
                const float d0 = d(j - 1), d1 = d(j);
                if (d0 * d1 <= 0) return 0.f;
                const float w1 = 2 * h(j) + h(j - 1), w2 = h(j) + 2 * h(j - 1);
                return (w1 + w2) / (w1 / d0 + w2 / d1);
            };
            size_t i = 0;
            while (i + 2 < n && t > k[i + 1].Out) ++i;
            const float hi = h(i), u = (t - k[i].Out) / hi, u2 = u * u, u3 = u2 * u;
            return (2 * u3 - 3 * u2 + 1) * k[i].In + (u3 - 2 * u2 + u) * hi * slope(i) + (3 * u2 - 2 * u3) * k[i + 1].In + (u3 - u2) * hi * slope(i + 1);
        }
        // how much of a move's travel between a and c is done t s into it (slow away, slow in); a hop's height then (of its highest)
        float Travel(float t, float a, float c)
        {
            if (c <= a) return t >= c ? 1.f : 0.f;
            const float u = std::clamp((t - a) / (c - a), 0.f, 1.f);
            return u * u * (3 - 2 * u);
        }
        float HopShare(float t, float a, float c)
        {
            if (c <= a) return 0;
            const float u = (t - a) / (c - a);
            return u <= 0 || u >= 1 ? 0.f : 4 * u * (1 - u);
        }
        // Her ways of going on foot (2026-11-26; NyxaraKit::SetGaitStyle). The owner, her pace on foot three times as fast
        // in the Character Lab: "because we fasting up walking then we need different styles of walking so i can choose one.
        // bring me at least 6-9 walking style for nyxara so i choose one". As made (0) her walk and run are stepped as often
        // as the ground asks: at three times the pace her legs go three times as often, as a film run fast. Each style is a
        // way of going made of the same two clips:
        // - how often she strides at most (Cadence + CadenceRise x her speed, strides a second): fewer, longer strides. A foot
        //   on the ground still goes back under her as fast as the ground does (none slides); what is drawn out is the time
        //   with both feet off the ground, and her body goes up and comes down in it as a body thrown does (Gravity, m/s2),
        //   her legs holding their split in the middle of it as long as Hang says (0 evenly, to .7);
        // - which of her walk and run she goes in: by her speed over Divide (0: by her pace, as made; 1: by her speed);
        // - how far her feet reach before and behind her (Reach: her stride drawn out, PlantFeet);
        // - her body: leaning forward (Lean, radians), her chest bowed (Bow, radians), her hips let down (Crouch, m), her
        //   head kept level against her lean and bow (Steady, 0..1), her knees taking each landing (Land);
        // - her arms (unless they hold her swords): their swing (times as made), bent at the elbows, carried back and held
        //   out from her sides (degrees); her wings: folded (0..1; -1 as made), opened toward their spread in flight (0..1),
        //   swept back (radians), beaten with her steps (radians).
        // All of it shows as she goes at a run (from 1.2 m/s, all of it from 3.5 m/s); standing, turning on the spot or
        // going back she is as made.
        struct GaitStyleSpec
        {
            const char* Name;
            const char* Line;
            float Divide, Cadence, CadenceRise, Reach, Gravity, Hang;
            float Lean, Bow, Crouch, Steady, Land;
            float ArmSwing, ArmBend, ArmBack, ArmOut;
            float WingFold, WingOpen, WingBack, WingBeat;
        };
        const GaitStyleSpec GaitStyles[] = {
            // divide, cadence, rise, reach, gravity, hang;  lean, bow, crouch, steady, land;  arm swing, bend, back, out;  wing fold, open, back, beat
            {"AS MADE", "her walk and run stepped as often as the ground asks: at a fast pace, a film run fast",
             0, 0, 0, 1, 9.8f, 0,  0, 0, 0, 0, 0,  1, 0, 0, 0,  -1, 0, 0, 0},
            {"SWIFT", "a sprinter's stride: long and quick, leaning in, her arms driving bent at the elbows, her wings held tight",
             1, 1.55f, .065f, 1.15f, 9.8f, 0,  .14f, .06f, .02f, .6f, .3f,  1.9f, 75, 0, 4,  1, 0, .15f, 0},
            {"BOUNDS", "long low leaps: few strides, her legs split in the air, her arms wide, her wings flicking open at each",
             1, 1.1f, .025f, 1.2f, 9.8f, .45f,  .08f, .04f, .03f, .4f, .5f,  1.6f, 25, 0, 10,  .5f, 0, 0, .5f},
            {"WIND GLIDE", "light steps under half-open wings swept back, her arms back and out, as if the air carried half of her",
             1, 1.25f, .03f, 1.1f, 4.5f, .3f,  .12f, .05f, 0, .7f, .2f,  .35f, 10, 25, 18,  0, .55f, .25f, .1f},
            {"SHADOW DASH", "low and fast: crouched, bowed far forward, her arms swept straight back, her wings flat to her back",
             1, 1.9f, .06f, 1.25f, 9.8f, 0,  .3f, .2f, .05f, .9f, .2f,  .15f, 5, 60, 10,  1, 0, .35f, 0},
            {"FEATHER", "slow floating strides: long hangs in the air, her wings lifting at each, her arms soft and out",
             1, .8f, .015f, 1.15f, 5.5f, .6f,  .04f, 0, 0, .5f, .4f,  .8f, 20, -5, 22,  .3f, .25f, 0, .45f},
            {"HUNTER", "a hunter's lope: level, long reaching steps, her knees soft, her head steady, her arms low",
             1, 1.35f, .04f, 1.35f, 9.8f, .15f,  .1f, .1f, .1f, 1, .6f,  .7f, 30, 10, 6,  .8f, 0, .2f, 0},
            {"WINGBEAT", "a bird's run to take off: a wing beat with every leap, long flights, her arms back",
             1, 1.f, .02f, 1.2f, 7.f, .4f,  .1f, .05f, .02f, .5f, .4f,  .6f, 15, 20, 12,  0, .35f, 0, .9f},
            {"PROUD", "upright and proud: her chest up, long even strides, her arms easy, her wings folded at her back",
             1.6f, 1.3f, .035f, 1.3f, 9.8f, .2f,  -.02f, -.06f, 0, .8f, .3f,  .55f, 15, 0, 5,  1, 0, 0, 0},
        };
    }

    // ================================================================ her body and her moves
    struct NyxaraKit::Body
    {
        // ---- the bundle
        DoorwaysSkeleton Skeleton;
        std::map<std::string, DoorwaysAnimationClip> Clips;
        std::vector<char> Upper;   // per joint: above the hips (spine, head, arms)
        std::vector<char> Wing;    // per joint: a wing bone
        std::vector<int> BodyJoints;                       // all but her wings and swords
        int Hips = -1, Spine = -1, Spine1 = -1, Spine2 = -1;
        int LegJoint[4] = {-1, -1, -1, -1};                // left and right hip joints, left and right knees
        int SwordJoint[2] = {-1, -1};
        // The swords are on her back in these clips and in her hands in all others (owner, 2026-11-18). Changing from one
        // to the other her arms go through the bundle's Sword_Reach pose (hands on the handles over her shoulders), and
        // the swords change place as her hands are there.
        static bool SwordsAway(const std::string& clip) { return clip == "Stand" || clip == "Move" || clip == "Walk" || clip == "Run" || clip == "Stairs_Up" || clip == "Jump" || clip == "Fall"; }
        // (in a fight they stay in her hands as she goes, jumps and falls: Armed)
        bool Away(const std::string& clip) const { return SwordsAway(clip) && !(Armed && (clip == "Move" || clip == "Jump" || clip == "Fall")); }
        std::vector<char> ArmJoint;                        // per joint: a shoulder, arm, hand or finger
        DoorwaysSkeletonPose ReachPose; bool HasReach = false, Drawing = false;
        bool InHand = true;                                // where her swords are: in her hands, or on her back
        float DrawSeconds = 0, DrawReach = 0;              // a draw or a putting away: how long her arms take in all, and up to the handles
        XMFLOAT4X4 SwordOnBack[2]{}; bool HasBack = false;   // each sword on her back, as her chest (Spine2) carries it
        int FootJoint[2] = {-1, -1}, ToeJoint[2] = {-1, -1};  // left and right ankle, left and right toes
        bool FootHeld[2] = {false, false};                   // a foot that is down is held where it came down:
        float FootAt[2][2]{}, FootOff[2][2]{};                // where (the world's x and z), and how far that is from where her clip has it
        float FootFree[2] = {9, 9}, FootGo[2] = {0, 0};      // seconds since each foot was let go; how fast its ball goes over the ground (m/s)
        float FootStood[2] = {0, 0};                         // seconds since each foot came down
        float RaiseWant[2] = {0, 0}, BodyWas = 0;            // a frame ago: what each foot was to be raised by; how high her body was in the world
        float SwingAt[2] = {1e9f, 1e9f};                     // on a stair: how high each foot's way through the air is, in the world
        float LandsAt[2] = {1e9f, 1e9f}, StoodAt[2] = {1e9f, 1e9f};   // and how high the tread it is coming down to, and the one it last stood on
        float SwingOf[2] = {0, 0}, LandSpeed[2] = {0, 0};    // each foot's own last swing: how long it took, and how fast she went as it ended (0: none yet)
        float FootWas[2][2]{}; bool FootWasKnown[2] = {false, false};   // where its ball was a frame ago (the world's x and z)
        float SwingSeconds = 0, LandAhead[2] = {0, 0};                  // her last swing (of either foot): how long it took; and of each foot, how far before her hip its ball came down (m, as her clip has it)
        float HerSpeed = 0, SpeedBefore = 0;                          // her speed, without the single frames in which it reads nothing (m/s)
        float FootAir[2] = {0, 0};                           // on a stair: in the air (1) or standing (0), by what the foot does
        bool FootOnStair[2] = {false, false};                // each foot over the map's stair treads
        float FootToGo[2] = {0, 0}, FootLifted[2] = {0, 0};  // (the log)
        int WingJoint[2][3] = {{-1, -1, -1}, {-1, -1, -1}};   // left and right wing: its three bones, root to tip
        float WingRightIs = 1;                                // +1: the wing named right is on her +x side as she was made; -1: the other (the importer mirrors)
        int NeckJoint = -1, HeadJoint = -1;
        float Look = 0;                    // her head turned into her turn (radians, + to her right)
        float TurnSteps = 0;               // turning on the spot: how much her legs are the stride's (0..1)
        float Bank = 0;                    // in flight: rolled into her turn (radians)
        struct WingSwing { float Flap = 0, FlapGo = 0, Back = 0, BackGo = 0, Flap2 = 0, Flap3 = 0, Back2 = 0, Back3 = 0; } WingSpring[2];
        float WingClimb = 0, WingShare = 0; bool WingKnown = false;   // her rise a frame ago (m/s); how much her wings answer
        float WingFold = 0, WingFoldGo = 0; // her wings lowered and drawn in (0 as the clip holds them .. 1 at ease), and how fast
        float AtEase = 0, NextStretch = 11, Stretch = -1;   // seconds she has stood at ease; when she next stretches her wings; seconds into a stretch
        static constexpr float FoldFlap[3] = {-.50f, -.25f, -.15f}, FoldBack[3] = {.35f, .22f, .12f};   // lowered: each bone down, and back (radians; more back and the tips cross behind her)
        XMFLOAT3 SwordGrip[2]{}, SwordTip[2]{};            // in the sword bone's axes: the handle's middle, the blade's point
        float FeetOn = 0, FeetDrop = 0, FeetRaise[2]{};    // feet on the drawn ground: how much; her body lowered; each foot raised (m)
        XMFLOAT3 KneeAxis[2][2]{};                         // each knee's axis as her clips have it, in its thigh's own frame and in its shin's (0: not seen bent yet)
        // her arms at ease while she walks and runs (EaseArms)
        int ArmChain[2][4] = {{-1, -1, -1, -1}, {-1, -1, -1, -1}};      // each side: shoulder, arm, forearm, hand
        DoorwaysSkeletonPose EasePose;                     // the arms she stands with, now
        std::vector<XMFLOAT4> ArmClip;                     // her arms as the clip has them (for the way in and out)
        XMFLOAT3 ArmRest[2][2]{}; bool ArmRestKnown = false;            // standing: which way each upper arm and forearm hangs, in her space
        float ArmsEasy = 0, LeadMost = 0, ArmLag = 0;      // how much of it; her widest stride of late; her arms left behind as she gathers speed (rad)
        float ForeAt[2] = {0, 0}, ForeGo[2] = {0, 0};      // each forearm: swung forward from where it hangs (rad), and how fast
        float ArmPace = 0;                                 // her speed as her arms take it (m/s, eased)
        bool Armed = false, WasArmed = false;              // a fight is on: her swords stay in her hands as she goes (the kit says so); as Compose last took it
        struct CutSwing { float Lead = .12f; XMFLOAT3 Hands[2]{}; XMFLOAT4 Turns[2]{}; bool Cuts[2] = {false, false}; XMFLOAT3 Track[2][CutTrack]{}; };
        std::map<std::string, CutSwing> Swings;            // each cut: how long before its blow its swing begins (clip s), her hands there, which of them cut
        float CutBlend = 0;                                // the cut she enters: the time her body takes to where its swing begins (s; 0: no cut, or a draw first)
        bool CarrySmooth = false;                          // what is carried on dies away evenly (into a cut), not as a spring does
        // her hands to the handles on her back (ReachArms)
        int Knuckle[2][2] = {{-1, -1}, {-1, -1}};          // each hand: its first and third fingers' first joints
        XMFLOAT3 HandLine[2]{};                            // which way each hand points, from its wrist to its knuckles (in its own axes)
        XMFLOAT3 HandAcross[2]{}, HandPalm[2]{};           // and along its knuckles (from the third finger's to the first's), and across both
        XMFLOAT4 SwordTurnInHand[2]{};                     // each sword in its fist: its turn against the hand,
        XMFLOAT3 GripInHand[2]{};                          // where the handle's middle is from the wrist (in the hand's axes, m),
        XMFLOAT3 GripInSword[2]{}, BladeInSword[2]{};      // and from the sword bone (in its axes, m); which way its blade points (so too)
        XMFLOAT3 ForeAxis[2]{};                            // which way each forearm's bone points (in its own axes)
        struct BentArm { XMFLOAT4 Upper{0, 0, 0, 1}, Fore{0, 0, 0, 1}; XMFLOAT3 UpperLine{}, ForeLine{}, Hinge{}; } ArmBent[2];   // an arm bent, as the reach pose has it: its bones' turns, their lines, the elbow's axis (her space)
        bool GripKnown = false;
        struct ArmState { bool Known = false; XMFLOAT3 Side{}, Axis{}, ForeLine{}; XMFLOAT4 Frame{0, 0, 0, 1}; } Reaching[2];   // each arm leaving its handle, a frame ago: its elbow's side, the line from shoulder to wrist, its forearm's line, and the frame its forearm and hand turn in
        bool SwordKept = false;                            // through this change her swords stay in her fists
        float WristHeld = 0;                               // how far her wrists are held to what a wrist does (HoldWrists)
        bool SwordWasInHand = true, SettleKnown = false;   // a sword taken or let go settles into its new place:
        float SwordSettle = 9;                             // seconds since
        XMFLOAT4X4 SwordBefore[2]{};                       // where each was (her space)
        XMFLOAT4 SettleTurn[2]{}; XMFLOAT3 SettleSlip[2]{};   // and that against where it now belongs: a turn about the handle's middle, and what is left
        float AnkleRest[2] = {1e9f, 1e9f};                 // the lowest each ankle stands in her clips (m over her feet): the foot is down
        float BallRest[2] = {1e9f, 1e9f};                  // the lowest the ball of each foot (its toe joint) stands in them
        float SoleGap[2] = {0, 0};                          // (the log) each sole over the drawn ground under the middle of its foot (m; NaN: none found)
        float FootPitch[2] = {0, 0};                        // each foot tipped to the slope under it (radians, toes up a hill)
        float FootSlide[2][2] = {{0, 0}, {0, 0}};           // each foot slid along a stair onto its tread (m, world x and z)
        float OnStair = 0;                                  // 0 off a stair, 1 climbing one (eased): her pace
        float Guard = 0;                   // seconds she still stands on guard (Idle: swords out); 0: at ease (Stand: swords on her back)
        float Climb = 0, ClimbRate = 0, LastY = -1e9f;      // how steeply and how fast she climbs (m/m, m/s); her height a frame ago

        // ---- the pose
        DoorwaysSkeletonPose Previous, Before, BlendFrom, LegsPose;
        std::string Gait = "Move", LastClip, Current = "Idle";
        float GaitRun = 0;                 // her stride ("Move"): the share of Run in it, 0 a walk .. 1 a run
        float StrideScale = 1;             // < 1: her steps shortened (slower than a walk steps she keeps stepping, shorter)
        // her way of going (GaitStyle): what she goes in (StyleNow; none as made), how much of it shows (StyleShare, 0..1: her
        // speed), and what it does to her this frame: her body over her stride's own in the air between steps (StyleArc, m),
        // her lean (radians), her hips let down (m), her wings' aims (radians); and how much the clip shown is her stride
        // (StyleBow, 0..1: her chest bowed, her head and wings, as it is)
        const GaitStyleSpec* StyleNow = nullptr;
        float StyleShare = 0, StyleArc = 0, StyleLean = 0, StyleLow = 0, StyleWingFlap = 0, StyleWingBack = 0, StyleBow = 0;
        float FlightClock = 0, FlightTotal = 0, FlightNatural = 0;   // her flight between steps: how long so far, in all, and as made (s)
        bool InFlight = false;
        // her soles in her walk and her run (LearnSoles): how high each is over its lowest, along the phase, as her body is made
        static constexpr int SoleSteps = 64;
        float SoleUp[2][2][SoleSteps] = {};   // [walk, run][her left foot, her right][phase]
        bool SolesKnown = false;
        std::string SoleNote;
        // her wings held steady in her ways of going (LearnWings; StylePose): what her walk and her run have on average over a
        // stride, for each wing (its root in her model's space, its two outer bones each in its own), her chest's turn, and her
        // chest's right and up as SwingWings reads them. (2026-11-26: the clips' wings rise and fall once a stride with her
        // chest's twist, and the at-ease fold lowered them about her twisting chest; at a style's pace the time warp held the
        // high and the low of it, one flight her wings up, the next down.)
        XMFLOAT4 WingHeldRoot[2][2] = {}, WingHeldOuter[2][2][2] = {}, ChestHeld[2] = {};   // [walk, run][wing][its 2nd, 3rd bone]
        XMFLOAT3 WingRightHeld[2] = {}, WingUpHeld[2] = {};
        bool WingsHeldKnown = false;
        float StyleHold = 0;                      // how much her wings are so held this frame (StylePose; SwingWings' axes with it)
        XMFLOAT4 StyleChestTurn = {0, 0, 0, 1};   // what her way of going turned her chest by (its bow), in her model's space
        // (out of a move that carried her, a skill such as the slide: how she went in its last frame, and whether that was
        // this frame's last one; owner 2026-11-21: "when nyxara dashes and stands than between the animation and the
        // continueing walk is a delay and scene cut happens")
        XMFLOAT2 CarriedGo{0, 0}; bool WasCarried = false, Freed = false;
        float Throttle = 0, FullSpeed = 3.2f, GlideLose = 0; XMFLOAT2 Glide{0, 0}, Went{0, 0};   // gathering speed; carried on after the key (m/s, x and z); how she went a frame ago
        std::vector<XMFLOAT4> ArmTo;       // her arms' turns in the clip she changes to, a frame ago (kept the same way round)
        float LeanYaw = 0, LeanSpeed = 0, TurnRate = 0, Gain = 0, LeanRoll = 0, LeanPitch = 0; bool LeanKnown = false;
        // A change of clip: what she was doing is carried on and dies away (per joint: how far the old pose was from
        // the new one, and how fast it was moving), instead of a mix with the old pose held still.
        std::vector<XMFLOAT3> CarryAt, CarryGo, CarryTurn, CarrySpin;
        float CarrySeconds = 0, LastDt = 0; bool CarryArms = true;
        float UpperOut = 0;                                // (>0) her body above her hips is carried on so long (s), not the change's own time
        float Legs = 0, Phase = 0, BlendTime = 0, BlendSeconds = 0, Speed = 0, Clock = 0;
        bool FlyBody = false;
        float Lowest = 0, LiftNow = 0;     // her lowest joint in her own space; metres her picture is raised now

        // ---- what she is doing
        std::string Attack;                // the cut or skill being played, empty: none
        int Begun = 0;                     // cuts and skills begun (NyxaraKit::AttacksBegun)
        float AttackTime = 0, SkillStrike = 0, SkillReach = 1;
        // (F, her slide) where she was a frame ago, how far it has carried her (m), how far on the game is next told of a
        // blow, and the creatures it has cut: which, how far she has gone within reach of it, how many times
        struct SlideCut { int Who; float In; int Times; float X, Z; };   // (X, Z: where it stood when her way first met it)
        XMFLOAT3 SlideWas{}; float SlideGone = 0, SlideTold = 0; std::vector<SlideCut> SlideCuts;
        bool SlideWake = false;   // (her flair's wake of it has begun)
        float Pace = 1;   // how fast her attacks and skills play (the Character Lab's trial: half; NyxaraKit::Update sets it)
        // a style's move (StartMove): which, by which button (0 left, 1 right), the creature it was aimed at, its next blow,
        // how far it carries her in, how far it has turned her; where it means her to be now and how fast it carries her
        // there (the world's x and z; m/s: the travel)
        const StyleMove* MoveOf = nullptr;
        int MoveButton = 0, MoveTarget = -1, MoveHitNext = 0;
        float GuardRaise = .12f;                           // into her guard (Compose), its Raise
        float WingClose = 0, WingCloseGo = 0, WingCloseTo = 0, WingCloseRate = 9;   // her wings closed round her (0 .. 1), how fast, toward, its rate
        float CloseBack[3]{}, CloseFlap[3]{};              // and closed so: each bone forward (-) and down (-) (radians)
        XMFLOAT3 WingLine[2][4]{}; bool WingLineKnown = false;   // each wing in the world: its three bones and its feathers' tip (Draw's lights)
        int ComboButton = -1, ComboStep = 0; float ComboEndedAt = -9;   // a combo's string: its button (-1 none), the move of it begun last, when a move of it was over
        float MovePlan = 0, MoveSpun = 0;
        XMFLOAT2 MoveAt{}, MoveVel{};
        XMFLOAT2 MoveOwe{};                               // what is left of its way, asked of the next frame (ScaleFrameTime)
        int Slices = 0;                                    // slices of 1/60 s the engine was to move her by at this frame's start (ScaleFrameTime)
        XMFLOAT2 WasAt{};                                  // her place when the kit's last Update was over (the world's x and z)
        float QueuedAt = -1;                               // when the click in Queued was made (her clock)
        bool Restarted = false;                            // the clip she plays begun again from its start: Compose blends into it anew
        float WaveTime = -1, WaveReach = 0; int WaveSparks = 0;   // GROUND WAVE's sparks: how long they have run (-1 none), how far they go, how many
        XMFLOAT3 WaveFrom{}, WaveWay{};                    // and from where, which way
        int Step = 0, Queued = 0;          // place in the combo; the button pressed during a cut (1 left, 2 right)
        int RowAt[2] = {0, 0}, LastHand = 0; bool RowUsed[2] = {false, false};   // each hand's place in its row of cuts; the hand that cut last; whether each has cut in this combo
        bool Jumping = false, Back = false, Dead = false;
        float JumpTime = 0, DeathTime = 0;
        bool JumpReal = false, JumpAir = false, WasGrounded = true;   // a jump that the world moves her in; she has left the ground in it
        float JumpClock = 0, AirSeconds = 0, Dropped = 0, FellAt = 0; // seconds of this jump; off the ground on foot; held reaching down; her fall a frame ago (m/s)
        float Squat = 0, SquatGo = 0;      // her knees taking a landing: her hips let down (m), and how fast
        bool Struck = false;               // something hurt her since her last frame
        float Flinch = 0, FlinchGo = 0;    // knocked back by it: her picture tipped forward (radians; back < 0), and how fast
        float FlinchRoll = 0, FlinchRollGo = 0;   // and to a side (to her left > 0), and how fast
        float StruckWeight = 1, StruckX = 0, StruckZ = -1;   // how hard it was (1: a blow of 9), and the way it knocked her (her axes: x her right, z the way she faces)
        bool StruckBelow = false;          // it burst under her feet
        float FlyTime = 0, FlyLean = 0, Hover = 0, Rise = -1, Touch = -1;
        bool WasFlying = false;
        int Air = 0; float AirTime = 0;    // F in flight: 1 the twisted dash
        XMFLOAT3 DashFrom{}; std::vector<int> DashStruck;   // (in it) where she was a frame ago; the creatures it has struck

        // ---- the feather storm (C)
        std::vector<float> StormTurned;                    // how far she has turned in it (radians), every 1/120 s of the clip
        float StormRateMost = 1;                           // her fastest turn in it (radians a second of the clip)
        float StormVolleyAt[3] = {1e9f, 1e9f, 1e9f};       // when she is half-way round each time (s of the clip): feathers at every creature near
        int StormVolley = 0; float StormFeathers = 0;      // the next of those; feathers owed (thrown as whole ones)
        int StormWing = 0;                                 // the wing that threw the last

        // ---- the thrown swords (V)
        XMFLOAT3 SwordFrom[2]{}, SwordFromWay[2]{}, SwordTo[2]{}, SwordToWay[2]{};
        XMFLOAT4 SwordFromTurn[2]{};       // each sword's turn in the world as it left her hand (PlaceSwords)
        bool SwordsOut = false, SwordCapture = false;
        float SwordFlight = 0, SwordBack = 0;
        int Cast = 0;                      // 0 no; 1 the throw's wind-up, 2 the swords fly, 3 they stick, 4 her twist to them, 5 on it, the grab and the backflip
        float CastTime = 0, CastGround = 0, CastRadius = .4f;
        int CastTarget = -1;
        XMFLOAT3 CastAt{};
        int Pushed = -1;                   // the creature her legs pushed over
        float PushTime = 0, PushStick = 1, PushGone = -1;
        XMFLOAT3 PushWay{0, 0, 1}, PushFeet{};
        float CastTurns = 0, CastTwist = 0;   // her whole turns on her way (she comes to it belly up); how far she has twisted (0 to 1)
        std::string WindUpsEvened, WindUpsLeft;   // the cuts whose wind-up EvenWindUps made one move, and left (for the log)
        bool CastStruck = false, CastKicked = false;
        // (her way, Cast 4) the handles' middle (world) and how far it is from the creature's middle; where she lands,
        // from it; her way's time; her place: from where, how far, to what height, and how fast this frame; her middle
        // (her hips) from where and to where her twist ends (world); where her feet stick on it (world)
        XMFLOAT3 CastHandles{};
        float CastGripAt = 1, CastLand = 2.4f, CastDiveTime = 1, CastRootWay = 0, CastRootY = 0, CastRootGo = 0;
        XMFLOAT3 CastRootFrom{}, CastFrom{}, CastDrillTo{}, CastFeetAt{};
        // (the backflip, Cast 5) where her hips are as she kicks off and how fast they go (her place's axes); how far the
        // handles in her fists were from her swords' at the most while she held them (m, for the log)
        XMFLOAT3 CastOffAt{}, CastOffGo{};
        float HandleGap = 0;

        // ---- her soul (Q)
        int Soul = 0;                        // 0 none, 1 she leaves it (thrown on), 2 away from it, 3 on her way back to it
        float SoulTime = 0, SoulBack = 1;    // s into her way back; how long it takes
        XMFLOAT3 SoulAt{}, SoulFrom{};       // where it stands (where she stood); her way back: from where
        float SoulYaw = 0, SoulFromYaw = 0;  // her facing as she left it; as she set off back
        float SoulClock = 0;                                      // s since she left it (Q's rest: the kit's, mSoulCooldown)
        float SoulGhostWay = 0; XMFLOAT3 SoulGhostWas{};          // m gone since her last afterimage; where she was a frame ago
        XMFLOAT3 SoulSeen{}; bool SoulSeenKnown = false;          // (where the kit last left her: put further than SoulPutAway from it, she lets it go)
        bool SoulLeaving = false, SoulMissDue = false;          // (this frame she leaves it: UpdateSoul takes her pose; she is in it again: how far off)
        float SoulLastWay = 0, SoulLastBack = 0, SoulLastMiss = 0;   // (the check's: her last leaving's way, her last way back's time, how far from it she ended)
        float SoulLastHeld = 0; std::string SoulOutOf;               // (and how long her last leaving held her; what she was doing as she left it)

        // ---- her finishing kills (Finishes; UpdateFinish)
        int Finish = 0, FinishLast = 0, FinishTarget = -1;    // the one she plays (0 none), the last she played, the creature
        float FinishTime = 0;
        float FinishRetry = 0;                                // s before she tries again where no finisher had room (StartFinish)
        float FinishSide = 1; bool FinishBehind = true;       // (the cross) past it on her right (1) or left (-1); round to behind it or beside it
        bool FinishBlown[2] = {false, false}, FinishKilled = false, FinishLanded = false, FinishRipped = false;
        float FinishBlowAt[2] = {-1, -1}, FinishDeathAt = 1;  // (this time: when her blows land and it dies)
        // as it began: her place and facing, its feet, the way from her to it (level) and her right as she faces it; its
        // height, width, neck (from its feet, the world's axes) and knees; its facing then and the one it turns to (her)
        XMFLOAT3 FinishFrom{}, FinishFeet{}, FinishWay{0, 0, 1}, FinishRight{1, 0, 0}, FinishNeck{0, 1.5f, 0};
        float FinishYawFrom = 0, FinishYawTo = 0, FinishHeight = 2, FinishRadius = .5f, FinishKnee = .45f;
        float FinishTheirYawFrom = 0, FinishTheirYawTo = 0;
        // the way she goes (each its own): points of it (the world), how she faces at the end, how high she is lifted
        XMFLOAT3 FinishA{}, FinishB{}, FinishC{};
        float FinishYawEnd = 0, FinishLiftMost = 0, FinishLiftNow = 0, FinishKneel = 0;
        XMFLOAT3 FinishAnchor{};                              // (the plunge) where her swords go into it, as it stands
        XMFLOAT3 FinishOff{}; float FinishOffLift = 0;        // (the plunge) where she is as it comes to lie, and how high
        XMFLOAT2 FinishGo{0, 0};                              // how fast the engine is to move her (m/s, x and z: the session's impulse)
        XMFLOAT3 FinishLand{};                                // (the plunge) where she comes down off it
        float FinishKneelTo = 0, FinishTall = 1;              // (the slide) how far it goes down on its knees; her height (her picture's scale)
        float FinishDepth = 1;                                // its depth, front to back (Wraith::Depth)
        XMFLOAT3 FinishStrike{};                              // (the slide) her blade's point as it kills: on its neck, or short of it in its back
        bool FinishHit[4]{};                                  // (4 to 10) which of its blows have landed
        XMFLOAT3 FinishMark[4]{}, FinishMarkWay[4]{}; int FinishMarks = 0;   // (4 to 10) where they cut it, and which way
        XMFLOAT3 FinishLit{}, FinishLitWay{}; bool FinishLitAny = false;     // (her flair) its last blow's light: where on it, and the way her blade went
        float WingAimFlap = 0, WingAimBack = 0;               // her wings held up (+) and swept back (+) by a move (radians, on what else they do)
        // the made clips: every 1/60 s her feet's middle and the points of her swords, left and right (her own space)
        std::vector<XMFLOAT3> FinishFeetAt[Finishers + 1], FinishTipAt[Finishers + 1][2];

        // ---- her whole picture turned (V's way and backflip, the twisted dash; TurnBody): BodyPitch forward about her
        // right and BodyRoll about her line (her picture's Rotation), BodyTwist about her own length (her hips turned
        // about her place's up, in Compose), BodyAt where that leaves her picture (its Translation); BodyMove: so this
        // frame. TwistNow: how far she has turned about her length in all. RingDue: the turned air thrown out in a ring.
        bool BodyMove = false, RingDue = false;
        float BodyPitch = 0, BodyRoll = 0, BodyTwist = 0, TwistNow = 0;
        XMFLOAT3 BodyAt{};
        // (made with V's clips, MakeCast) her hips in her twist, stood up; each sword's handle in her fist on guard (how
        // far apart her swords stick); every 1/60 s of Cast_Flip her hips and her feet (in her own space, before her
        // picture is turned); her hips' height on guard; how far Cast_Fly lies back (radians); her place's up as her hips'
        // parent has it; Fly's wings spread
        XMFLOAT3 DiveHips{}, GrabGrip[2]{}, Upright{0, 1, 0};
        std::vector<XMFLOAT3> FlipHipsAt, FlipFeetAt;
        float GuardHips = 1, DrillLean = 1.3f;
        DoorwaysSkeletonPose WingsLevel; bool HasWingsLevel = false;

        // ---- travel and the camera
        float PivotBase = 1.35f, CameraLift = 0;
        bool FlightHeld = false; float FlightSaved = 0;

        // ---- what a blow leaves: a bright streak along the cut, sparks, and blood thrown the way the blade went, which
        // falls and lies on the ground for a few seconds; and the air she turns in V
        struct Mark
        {
            XMFLOAT3 At{}, Go{};
            float Life = 0, Full = 1, Size = .05f, Floor = 0;
            int Kind = 0;   // 0 blood in the air, 1 blood on the ground, 2 the streak, 3 a spark, 4 turned air
        };
        std::vector<Mark> Marks;
        unsigned Seed = 2463534242u;
        std::string LogPath;
        float LastLog = -1;
        std::ofstream PoseFile; std::vector<int> PoseJoints;   // CAUCASIAN_EAGLE_NYXARA_POSELOG=<file>: her joints in the world, every frame
        XMFLOAT3 ClipHand[2]{}; DoorwaysSkeletonPose ClipOnly;  // (for that log) where the clip alone has her hands

        float Rand(float a = 0, float b = 1)
        {
            Seed = Seed * 1664525u + 1013904223u;
            return a + (b - a) * static_cast<float>(Seed >> 8) * (1.f / 16777216.f);
        }
        static float Hit(const std::string& name)
        {
            const auto found = NyxaraClips::Attacks.find(name);
            return found == NyxaraClips::Attacks.end() ? 0.f : found->second.Hit;
        }
        bool Skill() const { return Attack.rfind("Skill_", 0) == 0; }
        bool InMove() const { return MoveOf && Attack == MoveOf->Clip; }               // a style's move is played
        bool MoveFree() const { return InMove() && AttackTime >= MoveOf->Free; }       // and may be given up for what comes next
        bool Carried() const { return Attack == "Attack_Jump" || Skill() || InMove() || Soul == 3; }
        bool Moving() const { return !Attack.empty() || Cast != 0 || Air != 0 || Rise >= 0 || Touch >= 0 || Finish != 0 || Soul == 3; }
        // her soul (Q): its leaving and its way back (MakeSoul); how fast its leaving carries her on t s into it (m/s)
        static constexpr const char* SoulClip = "Skill_Soul";
        static constexpr const char* SoulBackClip = "Soul_Back";
        // her soul's leaving over (its clip played out, or given up, its throw over): away from it
        void SoulGone(const WorldState& world)
        {
            const XMFLOAT3& at = world.MainPlayer.Position;
            Soul = 2; SoulLastWay = std::hypot(at.x - SoulAt.x, at.z - SoulAt.z); SoulLastHeld = SoulClock;
            Log(world, ("soul away, " + std::to_string(SoulLastWay) + " m, held " + std::to_string(SoulLastHeld) + " s").c_str());
        }
        static float SoulGo(float t)
        {
            if (t <= SoulOutFrom || t >= SoulOutTo) return 0;
            const float u = (t - SoulOutFrom) / (SoulOutTo - SoulOutFrom);
            return SoulFar * 6 * u * (1 - u) / (SoulOutTo - SoulOutFrom);
        }
        static bool Loops(const std::string& name)
        {
            return name == "Idle" || name == "Stand" || name == "Walk" || name == "Run" || name == "Fly" || name == "Cast_Fly" || name == "Fall";
        }
        float Duration(const std::string& name) const
        {
            const auto found = Clips.find(name);
            return found == Clips.end() ? 0.f : found->second.DurationSeconds;
        }
        // Her stride (2026-11-19): Walk and Run are one stride, played by one phase (in both clips her right foot is
        // under her at 0 and her left at .5) and mixed by her speed. The share of Run is the one at which the mixed
        // stride, stepped at its own pace, covers the ground as fast as she goes: so at any speed between a walk and a
        // run she steps as often as a body does (her jog was the run played at 0.74 of its speed: slow motion), and a
        // change of pace is a change of stride, not of clip.
        void Stride(float speed, bool back, float dt)
        {
            const float dw = Duration("Walk"), dr = Duration("Run"), lw = WalkPace * dw, lr = RunPace * dr;
            const float share = back ? 0.f : std::clamp((speed * dw - lw) / ((lr - lw) - speed * (dr - dw)), 0.f, 1.f);
            GaitRun += (share - GaitRun) * (std::min)(1.f, dt * 8);
            const float length = lw + GaitRun * (lr - lw);      // metres a pair of steps
            // slower than a slow walk she does not step in slow motion: she keeps stepping (three quarters as often as
            // the walk does) and her steps are shortened to the ground she covers (PlantFeet draws her feet in)
            const float pace = speed / length, stepped = (std::max)(pace, .75f / dw);
            StrideScale += (pace / stepped - StrideScale) * (std::min)(1.f, dt * 20);
            Phase = std::fmod(Phase + (back ? -1.f : 1.f) * dt * stepped + 1.f, 1.f);
        }
        float GaitTime() const { return Gait == "Move" ? Phase : Phase * Duration(Gait); }
        // Her soles in her walk and her run, along the phase (2026-11-26, for her ways of going): how high each is over the
        // lowest it goes in that clip, its ankle or its ball, whichever is nearer. Within 2 cm of it a sole is down, past
        // 5 cm off the ground.
        void LearnSoles()
        {
            SolesKnown = false;
            if (FootJoint[0] < 0 || FootJoint[1] < 0) return;
            const char* const names[2] = {"Walk", "Run"};
            std::string note;
            for (int c = 0; c < 2; ++c)
            {
                const float d = Duration(names[c]);
                if (d <= 0) return;
                float y[2][2][SoleSteps] = {}, low[2][2] = {{1e9f, 1e9f}, {1e9f, 1e9f}};   // [foot][ankle, ball][phase]
                for (int i = 0; i < SoleSteps; ++i)
                {
                    DoorwaysSkeletonPose pose;
                    if (!Sample(names[c], d * static_cast<float>(i) / SoleSteps, pose, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) return;
                    for (int k = 0; k < 2; ++k)
                    {
                        y[k][0][i] = pose.ModelSpaceMatrices[FootJoint[k]]._42;
                        y[k][1][i] = ToeJoint[k] >= 0 ? pose.ModelSpaceMatrices[ToeJoint[k]]._42 : y[k][0][i];
                        for (int e = 0; e < 2; ++e) low[k][e] = (std::min)(low[k][e], y[k][e][i]);
                    }
                }
                float air = 0;
                for (int i = 0; i < SoleSteps; ++i)
                {
                    float down = 0;
                    for (int k = 0; k < 2; ++k)
                    {
                        SoleUp[c][k][i] = (std::min)(y[k][0][i] - low[k][0], y[k][1][i] - low[k][1]);
                        down = (std::max)(down, 1 - Ramp(SoleUp[c][k][i], .02f, .05f));
                    }
                    air += 1 - down;
                }
                note += std::string(c ? "; " : "") + names[c] + ":";
                for (int k = 0; k < 2; ++k)
                {   // (where this foot comes down and lifts, as phases)
                    note += k ? " right" : " left";
                    for (int i = 0; i < SoleSteps; ++i)
                    {
                        const bool now = SoleUp[c][k][i] < .035f, before = SoleUp[c][k][(i + SoleSteps - 1) % SoleSteps] < .035f;
                        if (now == before) continue;
                        char at[24];
                        std::snprintf(at, sizeof at, " %s %.2f", now ? "down" : "up", static_cast<double>(i) / SoleSteps);
                        note += at;
                    }
                }
                char share[64];
                std::snprintf(share, sizeof share, " (both off the ground %.0f%%; ankles lowest %.3f %.3f)", 100.0 * air / SoleSteps,
                              static_cast<double>(low[0][0]), static_cast<double>(low[1][0]));
                note += share;
            }
            SoleNote = note;
            SolesKnown = true;
        }
        void LearnWings()
        {
            WingsHeldKnown = false;
            for (const auto& wing : WingJoint)
                for (const int j : wing)
                    if (j < 0) return;
            const int chest = Skeleton.Joints[WingJoint[0][0]].ParentIndex, under = chest >= 0 ? Skeleton.Joints[chest].ParentIndex : -1;
            if (chest < 0 || Skeleton.Joints[WingJoint[1][0]].ParentIndex != chest) return;
            const char* const names[2] = {"Walk", "Run"};
            constexpr int Samples = 64;
            const auto turnOf = [](const DoorwaysSkeletonPose& pose, int j) { XMVECTOR sc, q, t; XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&pose.ModelSpaceMatrices[j])); return XMQuaternionNormalize(q); };
            const auto at = [](const DoorwaysSkeletonPose& pose, int j) { const auto& m = pose.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            const auto add = [](XMVECTOR& sum, XMVECTOR q, XMVECTOR first) { sum += XMVectorGetX(XMQuaternionDot(q, first)) < 0 ? -q : q; };   // (one sign for all)
            for (int c = 0; c < 2; ++c)
            {
                const float d = Duration(names[c]);
                if (d <= 0) return;
                XMVECTOR rootSum[2] = {XMVectorZero(), XMVectorZero()}, outerSum[2][2] = {{XMVectorZero(), XMVectorZero()}, {XMVectorZero(), XMVectorZero()}};
                XMVECTOR rootFirst[2] = {XMVectorZero(), XMVectorZero()}, outerFirst[2][2] = {{XMVectorZero(), XMVectorZero()}, {XMVectorZero(), XMVectorZero()}};
                XMVECTOR chestSum = XMVectorZero(), chestFirst = XMQuaternionIdentity(), rightSum = XMVectorZero(), upSum = XMVectorZero();
                for (int i = 0; i < Samples; ++i)
                {
                    DoorwaysSkeletonPose pose;
                    if (!Sample(names[c], d * static_cast<float>(i) / Samples, pose, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) return;
                    const XMVECTOR chestTurn = turnOf(pose, chest);
                    if (i == 0) chestFirst = chestTurn;
                    add(chestSum, chestTurn, chestFirst);
                    for (int k = 0; k < 2; ++k)
                    {
                        const XMVECTOR rootTurn = turnOf(pose, WingJoint[k][0]);
                        if (i == 0) rootFirst[k] = rootTurn;
                        add(rootSum[k], rootTurn, rootFirst[k]);
                        for (int e = 0; e < 2; ++e)
                        {
                            const XMVECTOR own = XMQuaternionNormalize(XMLoadFloat4(&pose.LocalJoints[WingJoint[k][e + 1]].Rotation));
                            if (i == 0) outerFirst[k][e] = own;
                            add(outerSum[k][e], own, outerFirst[k][e]);
                        }
                    }
                    const XMVECTOR sideways = (at(pose, WingJoint[1][0]) - at(pose, WingJoint[0][0])) * WingRightIs;   // (as SwingWings reads them)
                    if (XMVectorGetX(XMVector3LengthSq(sideways)) > 1e-8f) rightSum += XMVector3Normalize(sideways);
                    const XMVECTOR upward = under >= 0 ? at(pose, chest) - at(pose, under) : XMVectorSet(0, 1, 0, 0);
                    if (XMVectorGetX(XMVector3LengthSq(upward)) > 1e-8f) upSum += XMVector3Normalize(upward);
                }
                if (XMVectorGetX(XMVector3LengthSq(rightSum)) < 1e-8f || XMVectorGetX(XMVector3LengthSq(upSum)) < 1e-8f) return;
                XMStoreFloat4(&ChestHeld[c], XMQuaternionNormalize(chestSum));
                for (int k = 0; k < 2; ++k)
                {
                    XMStoreFloat4(&WingHeldRoot[c][k], XMQuaternionNormalize(rootSum[k]));
                    for (int e = 0; e < 2; ++e) XMStoreFloat4(&WingHeldOuter[c][k][e], XMQuaternionNormalize(outerSum[k][e]));
                }
                XMStoreFloat3(&WingRightHeld[c], XMVector3Normalize(rightSum));
                XMStoreFloat3(&WingUpHeld[c], XMVector3Normalize(upSum));
            }
            WingsHeldKnown = true;
        }
        // Her stride in a way of going (GaitStyle; Stride is hers as made): the share of her run, the phase stepped on, her
        // feet's reach; and her flight between steps (StyleArc). speed: hers (m/s); pace: her pace on foot (SetFootPace).
        void StyleStride(const GaitStyleSpec& g, float speed, float pace, bool back, float dt)
        {
            const float dw = Duration("Walk"), dr = Duration("Run"), lw = WalkPace * dw, lr = RunPace * dr;
            const float by = speed / (g.Divide > 0 ? g.Divide : pace);
            const float share = back ? 0.f : std::clamp((by * dw - lw) / ((lr - lw) - by * (dr - dw)), 0.f, 1.f);
            GaitRun += (share - GaitRun) * (std::min)(1.f, dt * 8);
            const float length = lw + GaitRun * (lr - lw), w = StyleShare;     // (metres a stride, as made)
            const float reach = 1 + (g.Reach - 1) * w;
            const float ground = speed / (length * reach);   // strides a second at which a foot down goes back as fast as the ground
            const float most = g.Cadence > 0 ? g.Cadence + g.CadenceRise * speed : ground;
            const float want = ground - w * (std::max)(0.f, ground - most);   // strides a second, no more than the ground asks
            // where her feet are down along the phase, in her walk and run as mixed now; her flights between; and the pace of
            // each part of a flight against the rest of it (slower in its middle: Hang)
            float down[SoleSteps] = {}, hang[SoleSteps] = {}, air = 0;
            bool warped = SolesKnown && !back && want < ground * .98f;
            if (warped)
            {
                for (int i = 0; i < SoleSteps; ++i)
                {
                    float d = 0;
                    for (int k = 0; k < 2; ++k) d = (std::max)(d, 1 - Ramp(SoleUp[0][k][i] + (SoleUp[1][k][i] - SoleUp[0][k][i]) * GaitRun, .02f, .05f));
                    down[i] = d; hang[i] = 1; air += 1 - d;
                }
                warped = air > .05f * SoleSteps;   // (a walk has a foot down all the time: nothing to draw out)
            }
            float aloftRate = ground;   // strides a second through the air
            if (warped)
            {
                int start = 0;
                while (start < SoleSteps && down[start] < .5f) ++start;
                if (start < SoleSteps && g.Hang > 0)
                    for (int n = 0; n < SoleSteps;)
                    {
                        if (down[(start + n) % SoleSteps] >= .5f) { ++n; continue; }
                        int m = 0;
                        while (n + m < SoleSteps && down[(start + n + m) % SoleSteps] < .5f) ++m;
                        for (int j = 0; j < m; ++j)
                            hang[(start + n + j) % SoleSteps] = 1 + g.Hang * w * std::cos(XM_2PI * (static_cast<float>(j) + .5f) / static_cast<float>(m));
                        n += m;
                    }
                const auto cycle = [&](float r)   // seconds a stride, at r strides a second through the air
                {
                    float t = 0;
                    for (int i = 0; i < SoleSteps; ++i) t += 1.f / (down[i] * ground + (1 - down[i]) * r * hang[i]);
                    return t / SoleSteps;
                };
                float lo = ground / 8, hi = ground;
                if (cycle(hi) < 1 / want)
                {
                    for (int it = 0; it < 24; ++it)
                    {
                        const float mid = (lo + hi) * .5f;
                        (cycle(mid) > 1 / want ? lo : hi) = mid;
                    }
                    aloftRate = (lo + hi) * .5f;
                }
            }
            if (!warped)
            {   // (as made: slower than a slow walk she keeps stepping, her steps drawn in)
                const float stepped = (std::max)(ground, .75f / dw);
                StrideScale += (reach * ground / stepped - StrideScale) * (std::min)(1.f, dt * 20);
                Phase = std::fmod(Phase + (back ? -1.f : 1.f) * dt * stepped + 1.f, 1.f);
            }
            else
            {
                StrideScale += (reach - StrideScale) * (std::min)(1.f, dt * 20);
                for (float left = dt; left > 1e-6f;)
                {
                    const float h = (std::min)(left, 1 / 240.f); left -= h;
                    const float at = Phase * SoleSteps;
                    const int i0 = static_cast<int>(at) % SoleSteps, i1 = (i0 + 1) % SoleSteps;
                    const float f = at - std::floor(at), d = down[i0] + (down[i1] - down[i0]) * f, hg = hang[i0] + (hang[i1] - hang[i0]) * f;
                    Phase = std::fmod(Phase + h * (d * ground + (1 - d) * aloftRate * hg), 1.f);
                }
            }
            // Her flight: from both her feet leaving the ground until one is down her body goes up and comes down as thrown,
            // over what her stride as made has of it (her clips' own, shorter flight)
            const int now = static_cast<int>(Phase * SoleSteps) % SoleSteps;
            const bool aloft = warped && down[now] < .5f;
            if (aloft && !InFlight)
            {   // a flight begins: how long it will be, and how long it would have been as made
                FlightClock = 0; FlightTotal = FlightNatural = 0;
                for (int n = 0; n < SoleSteps && down[(now + n) % SoleSteps] < .5f; ++n)
                {
                    const int i = (now + n) % SoleSteps;
                    FlightTotal += 1.f / (SoleSteps * (down[i] * ground + (1 - down[i]) * aloftRate * hang[i]));
                    FlightNatural += 1.f / (SoleSteps * ground);
                }
            }
            if (aloft) FlightClock += dt;
            else if (InFlight && g.Land > 0)   // (down: her knees take it, as hard as she came down over her stride as made)
                SquatGo += g.Land * w * g.Gravity * (std::max)(0.f, FlightTotal - FlightNatural) * .75f;
            InFlight = aloft;
            const float p = FlightTotal > 0 ? std::clamp(FlightClock / FlightTotal, 0.f, 1.f) : 0.f;
            StyleArc = aloft ? .5f * g.Gravity * p * (1 - p) * (std::max)(0.f, FlightTotal * FlightTotal - FlightNatural * FlightNatural) : 0.f;
        }
        // her way of going, every frame on foot: how much of it shows, and what it does to her body (StyleStride steps it)
        void StyleFrame(const GaitStyleSpec* g, bool going, float speed, float dt)
        {
            if (g) StyleNow = g;
            const bool on = g && going && !Back && Gait == "Move";
            StyleShare += ((on ? Ramp(speed, 1.2f, 3.5f) : 0.f) - StyleShare) * (std::min)(1.f, dt * 5);
            if (!g && StyleShare < 1e-3f) { StyleNow = nullptr; StyleShare = 0; }
            if (!on) { StyleArc *= std::exp(-dt * 20); InFlight = false; }
            const GaitStyleSpec* s = StyleNow;
            const float w = s ? StyleShare : 0.f;
            StyleLean = s ? s->Lean * w : 0.f;
            StyleLow = s ? s->Crouch * w : 0.f;
            StyleWingBack = s ? s->WingBack * w : 0.f;
            StyleWingFlap = s ? s->WingBeat * w * std::sin(XM_2PI * 2 * Phase) : 0.f;
        }
        float StyleFold(float asMade) const { return StyleNow && StyleNow->WingFold >= 0 ? asMade + (StyleNow->WingFold - asMade) * StyleShare : asMade; }
        // Her way of going on her pose (after Compose, before PlantFeet): her chest bowed, her head kept level, her wings
        // opened, her body up in her flight between steps (her feet with it: PlantFeet sees them off the ground).
        // stride: the clip shown is her stride; scale: her picture's (metres a unit of her body).
        void StylePose(DoorwaysSkeletonPose& out, float dt, bool stride, float scale)
        {
            StyleBow += ((stride ? 1.f : 0.f) - StyleBow) * (std::min)(1.f, dt * 10);
            const GaitStyleSpec* s = StyleNow;
            if (!s || StyleShare < 1e-3f || out.LocalJoints.empty() || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr)) return;
            const auto turnOf = [&](int j) { XMVECTOR sc, q, t; XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[j])); return XMQuaternionNormalize(q); };
            const int wingChest = WingsHeldKnown ? Skeleton.Joints[WingJoint[0][0]].ParentIndex : -1;
            const XMVECTOR chestAsMade = wingChest >= 0 ? turnOf(wingChest) : XMQuaternionIdentity();   // (before her bow)
            const float w = StyleShare * StyleBow, bow = s->Bow * w, upright = -(s->Bow * w + StyleLean * StyleBow) * s->Steady;
            bool changed = false;
            if (std::abs(bow) > 1e-4f)
            {   // her chest bowed: a share at each joint of her spine, each a turn about her right as her body has it
                const int bend[3] = {Spine, Spine1, Spine2};
                int count = 0;
                for (const int j : bend) count += j >= 0 && Skeleton.Joints[j].ParentIndex >= 0;
                if (count > 0)
                {
                    const XMVECTOR forward = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), bow / static_cast<float>(count));
                    XMVECTOR ups[3];
                    for (int i = 0; i < 3; ++i) ups[i] = bend[i] >= 0 && Skeleton.Joints[bend[i]].ParentIndex >= 0 ? turnOf(Skeleton.Joints[bend[i]].ParentIndex) : XMQuaternionIdentity();
                    for (int i = 0; i < 3; ++i)
                    {   // (all about the same axis: no turn of one changes the others')
                        const int j = bend[i];
                        if (j < 0 || Skeleton.Joints[j].ParentIndex < 0) continue;
                        const XMVECTOR there = XMQuaternionMultiply(XMQuaternionMultiply(ups[i], forward), XMQuaternionConjugate(ups[i]));
                        XMStoreFloat4(&out.LocalJoints[j].Rotation, XMQuaternionNormalize(After(there, XMLoadFloat4(&out.LocalJoints[j].Rotation))));
                    }
                    changed = true;
                }
            }
            if (std::abs(upright) > 1e-4f && NeckJoint >= 0 && HeadJoint >= 0 && Skeleton.Joints[HeadJoint].ParentIndex == NeckJoint && Skeleton.Joints[NeckJoint].ParentIndex >= 0)
            {   // her head turned back up by what she leans and bows, as far as Steady says (her neck half of it)
                if (changed) EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr);
                const int above = Skeleton.Joints[NeckJoint].ParentIndex;
                const XMVECTOR side = XMVectorSet(1, 0, 0, 0);
                const XMVECTOR neck = XMQuaternionMultiply(turnOf(NeckJoint), XMQuaternionRotationAxis(side, upright * .5f));
                const XMVECTOR head = XMQuaternionMultiply(turnOf(HeadJoint), XMQuaternionRotationAxis(side, upright));
                XMStoreFloat4(&out.LocalJoints[NeckJoint].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(neck, XMQuaternionInverse(turnOf(above)))));
                XMStoreFloat4(&out.LocalJoints[HeadJoint].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(head, XMQuaternionInverse(neck))));
                changed = true;
            }
            if (wingChest >= 0 && w > 1e-3f)
            {   // her wings held as her stride has them on average (not rising and falling with her chest's twist, whose high and
                // low the time warp holds), her chest's bow carried with them; and opened toward their spread in flight (where
                // Fly's beat down has them level) as her chest stands on average. SwingWings lowers and sweeps them about it too.
                if (changed) EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr);
                const XMVECTOR chestNow = turnOf(wingChest), bowed = XMQuaternionNormalize(XMQuaternionMultiply(XMQuaternionInverse(chestAsMade), chestNow));
                const float run = GaitRun;
                const XMVECTOR chestHeld = XMQuaternionMultiply(XMQuaternionSlerp(XMLoadFloat4(&ChestHeld[0]), XMLoadFloat4(&ChestHeld[1]), run), bowed);
                const bool spread = s->WingOpen > 0 && HasWingsLevel && WingsLevel.LocalJoints.size() == out.LocalJoints.size();
                for (int k = 0; k < 2; ++k)
                {
                    const int* j = WingJoint[k];
                    XMVECTOR to = XMQuaternionMultiply(XMQuaternionSlerp(XMLoadFloat4(&WingHeldRoot[0][k]), XMLoadFloat4(&WingHeldRoot[1][k]), run), bowed);
                    if (spread) to = XMQuaternionSlerp(to, XMQuaternionMultiply(XMLoadFloat4(&WingsLevel.LocalJoints[j[0]].Rotation), chestHeld), s->WingOpen);
                    const XMVECTOR now = XMQuaternionSlerp(turnOf(j[0]), to, w);
                    XMStoreFloat4(&out.LocalJoints[j[0]].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(now, XMQuaternionInverse(chestNow))));
                    for (int e = 0; e < 2; ++e)
                    {
                        XMVECTOR held = XMQuaternionSlerp(XMLoadFloat4(&WingHeldOuter[0][k][e]), XMLoadFloat4(&WingHeldOuter[1][k][e]), run);
                        if (spread) held = XMQuaternionSlerp(held, XMLoadFloat4(&WingsLevel.LocalJoints[j[e + 1]].Rotation), s->WingOpen);
                        XMStoreFloat4(&out.LocalJoints[j[e + 1]].Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&out.LocalJoints[j[e + 1]].Rotation), held, w)));
                    }
                }
                XMStoreFloat4(&StyleChestTurn, bowed);
                StyleHold = w;
                changed = true;
            }
            else if (s->WingOpen > 0 && HasWingsLevel && WingsLevel.LocalJoints.size() == out.LocalJoints.size())
            {   // her wings opened toward their spread in flight (where Fly's beat down has them level)
                const float open = s->WingOpen * w;
                for (const auto& wing : WingJoint)
                    for (const int j : wing)
                        if (j >= 0)
                            XMStoreFloat4(&out.LocalJoints[j].Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&out.LocalJoints[j].Rotation),
                                                                                                                  XMLoadFloat4(&WingsLevel.LocalJoints[j].Rotation), open)));
                changed = true;
            }
            if (std::abs(StyleArc) > 1e-4f && Hips >= 0)
            {   // her body up in her flight, her feet with it
                XMVECTOR by = XMVectorSet(0, StyleArc / (std::max)(.01f, scale), 0, 0);
                if (const int above = Skeleton.Joints[Hips].ParentIndex; above >= 0)
                    by = XMVector3TransformNormal(by, XMMatrixInverse(nullptr, XMLoadFloat4x4(&out.ModelSpaceMatrices[above])));
                XMStoreFloat3(&out.LocalJoints[Hips].Translation, XMLoadFloat3(&out.LocalJoints[Hips].Translation) + by);
                changed = true;
            }
            if (changed) EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr);
        }
        bool SampleMove(float phase, DoorwaysSkeletonPose& out, std::string* error) const
        {
            const auto walk = Clips.find("Walk"), run = Clips.find("Run");
            if (walk == Clips.end() || run == Clips.end()) return false;
            phase -= std::floor(phase);
            if (GaitRun > .999f) return SampleDoorwaysAnimationClipLocalPose(Skeleton, run->second, phase * run->second.DurationSeconds, out, error);
            if (!SampleDoorwaysAnimationClipLocalPose(Skeleton, walk->second, phase * walk->second.DurationSeconds, out, error)) return false;
            if (GaitRun < .001f) return true;
            DoorwaysSkeletonPose fast;
            if (!SampleDoorwaysAnimationClipLocalPose(Skeleton, run->second, phase * run->second.DurationSeconds, fast, error) || fast.LocalJoints.size() != out.LocalJoints.size()) return false;
            for (size_t i = 0; i < out.LocalJoints.size(); ++i)
            {
                auto& to = out.LocalJoints[i]; const auto& f = fast.LocalJoints[i];
                XMStoreFloat3(&to.Translation, XMVectorLerp(XMLoadFloat3(&to.Translation), XMLoadFloat3(&f.Translation), GaitRun));
                XMStoreFloat4(&to.Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&to.Rotation), XMLoadFloat4(&f.Rotation), GaitRun)));
                XMStoreFloat3(&to.Scale, XMVectorLerp(XMLoadFloat3(&to.Scale), XMLoadFloat3(&f.Scale), GaitRun));
            }
            return true;
        }
        static XMVECTOR SlerpAsGiven(XMVECTOR a, XMVECTOR b, float t)   // from a to b the way the two are given (not the shorter way)
        {
            const float d = std::clamp(XMVectorGetX(XMVector4Dot(a, b)), -1.f, 1.f);
            if (d > .9995f) return XMQuaternionNormalize(XMVectorLerp(a, b, t));
            const float angle = std::acos(d), s = std::sin(angle);
            return XMQuaternionNormalize(a * (std::sin((1 - t) * angle) / s) + b * (std::sin(t * angle) / s));
        }
        static XMVECTOR TurnVector(XMVECTOR q)     // a turn as its axis times its angle
        {
            if (XMVectorGetW(q) < 0) q = XMVectorNegate(q);
            const float half = std::sqrt(XMVectorGetX(XMVector3LengthSq(q)));
            if (half < 1e-6f) return XMVectorZero();
            return XMVectorScale(XMVectorSetW(q, 0), 2 * std::atan2(half, XMVectorGetW(q)) / half);
        }
        static XMVECTOR TurnOf(XMVECTOR v)         // and back
        {
            const float angle = XMVectorGetX(XMVector3Length(v));
            return angle < 1e-6f ? XMQuaternionIdentity() : XMQuaternionRotationAxis(v, angle);
        }
        bool Sample(const std::string& name, float time, DoorwaysSkeletonPose& out, std::string* error) const
        {
            if (name == "Move") return SampleMove(time, out, error);
            const auto found = Clips.find(name);
            if (found == Clips.end()) { if (error) *error = "Missing NYXARA clip: " + name; return false; }
            const float duration = found->second.DurationSeconds;
            if (!std::isfinite(time) || duration <= 0) return false;
            time = Loops(name) ? std::fmod((std::max)(0.f, time), duration) : std::clamp(time, 0.f, duration);
            return SampleDoorwaysAnimationClipLocalPose(Skeleton, found->second, time, out, error);
        }
        void Log(const WorldState& world, const char* event)
        {   // CAUCASIAN_EAGLE_NYXARA_LOG=<file>: what she does, for unattended tests
            if (LogPath.empty()) return;
            const auto& p = world.MainPlayer;
            std::ofstream f(LogPath, std::ios::app);
            f << Clock << '\t' << event << '\t' << Current << '\t' << Speed << '\t' << p.Position.x << '\t' << p.Position.y << '\t' << p.Position.z
              << '\t' << LiftNow << '\t' << FlyLean << '\t' << FeetDrop << '\t' << FeetRaise[0] << '\t' << FeetRaise[1] << '\t' << SoleGap[0] << '\t' << SoleGap[1]
              << '\t' << world.MainCamera.Position.x << '\t' << world.MainCamera.Position.y << '\t' << world.MainCamera.Position.z << '\t' << Throttle << '\t' << p.MoveSpeed << '\n';
        }

        // Every frame, as she is drawn: what she plays, where she is, and her joints in the world (for the measurements
        // of testing/MapLab/anim_measure.py: feet that slide, jerks, feet over or in the ground).
        void PoseLog(const WorldState& world, const DoorwaysSkeletonPose& pose, float dt, bool planted)
        {
            if (!PoseFile.is_open() || pose.ModelSpaceMatrices.size() != Skeleton.Joints.size()) return;
            const auto& p = world.MainPlayer;
            const XMMATRIX place = BuildDoorwaysCharacterWorldMatrix(p.Position, p.Yaw, world.AnimatedPlayerVisual);
            PoseFile << Clock << '\t' << dt << '\t' << Current << '\t' << Phase << '\t' << Speed << '\t' << p.Position.x << '\t' << p.Position.y << '\t' << p.Position.z
                     << '\t' << p.Yaw << '\t' << p.IsGrounded << '\t' << p.VerticalVelocity << '\t' << (p.TraversalMode == DoorwaysPlayerTraversalMode::Flight)
                     << '\t' << planted << '\t' << SoleGap[0] << '\t' << SoleGap[1];
            for (const int j : PoseJoints)
            {
                XMFLOAT3 at; XMStoreFloat3(&at, XMVector3TransformCoord(XMLoadFloat4x4(&pose.ModelSpaceMatrices[j]).r[3], place));
                PoseFile << '\t' << at.x << '\t' << at.y << '\t' << at.z;
            }
            for (int k = 0; k < 4; ++k)
            {   // her swords' points, and where the clip alone had her hands (before anything was carried over, drawn or eased)
                XMFLOAT3 at{};
                if (k < 2 && SwordJoint[k] >= 0)
                    XMStoreFloat3(&at, XMVector3TransformCoord(XMLoadFloat3(&SwordTip[k]), XMLoadFloat4x4(&pose.ModelSpaceMatrices[SwordJoint[k]]) * place));
                else if (k >= 2) XMStoreFloat3(&at, XMVector3TransformCoord(XMLoadFloat3(&ClipHand[k - 2]), place));
                PoseFile << '\t' << at.x << '\t' << at.y << '\t' << at.z;
            }
            for (int k = 0; k < 2; ++k)
                PoseFile << '\t' << FootHeld[k] << '\t' << FootAir[k] << '\t' << FootGo[k] << '\t' << FootToGo[k] << '\t' << FootLifted[k] << '\t' << FootOnStair[k]
                         << '\t' << FootFree[k] << '\t' << SwingSeconds << '\t' << LandAhead[k];
            PoseFile << '\n';
        }

        // Where each cut's swing begins (2026-11-19): the hand that cuts is at its fastest about the blow; back from
        // there to where it had a third of that speed. And the way of both hands through the cut's last 0.6 s (60 times
        // a second), for choosing how to enter it.
        void LearnSwings()
        {
            if (ArmChain[0][3] < 0 || ArmChain[1][3] < 0) return;
            constexpr float Tick = 1 / 60.f; constexpr int Early = CutTrackEarly, Count = CutTrack;      // from 0.6 s before the blow
            DoorwaysSkeletonPose pose;
            for (const auto& entry : NyxaraClips::Attacks)
            {
                const std::string& name = entry.first;
                if (name.rfind("Attack", 0) != 0 || Duration(name) <= 0) continue;
                const bool left = name.rfind("Attack_L", 0) == 0, right = name.rfind("Attack_R", 0) == 0;
                CutSwing sw;
                sw.Cuts[0] = left || !right; sw.Cuts[1] = right || !left;
                float speed[Count] = {};
                bool whole = true;
                for (int f = 0; f < Count && whole; ++f)
                {
                    whole = Sample(name, std::clamp(entry.second.Hit + (f - Early) * Tick, 0.f, Duration(name)), pose, nullptr) &&
                            EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr);
                    for (int s = 0; s < 2 && whole; ++s) { const auto& m = pose.ModelSpaceMatrices[ArmChain[s][3]]; sw.Track[s][f] = {m._41, m._42, m._43}; }
                }
                if (!whole) continue;
                for (int f = 1; f < Count - 1; ++f)
                    for (int s = 0; s < 2; ++s)
                        if (sw.Cuts[s])
                            speed[f] = (std::max)(speed[f], XMVectorGetX(XMVector3Length(XMLoadFloat3(&sw.Track[s][f + 1]) - XMLoadFloat3(&sw.Track[s][f - 1]))) / (2 * Tick));
                int most = Early;
                for (int f = Early - 12; f <= Early + 2; ++f) if (speed[f] > speed[most]) most = f;
                int from = most;
                while (from > 1 && speed[from] > speed[most] / 3) --from;
                sw.Lead = std::clamp((Early - from) * Tick, .08f, .25f);
                if (!Sample(name, (std::max)(0.f, entry.second.Hit - sw.Lead), pose, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) continue;
                for (int s = 0; s < 2; ++s)
                {
                    XMVECTOR sc, q, t;
                    XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&pose.ModelSpaceMatrices[ArmChain[s][3]]));
                    XMStoreFloat3(&sw.Hands[s], t); XMStoreFloat4(&sw.Turns[s], XMQuaternionNormalize(q));
                }
                Swings[name] = sw;
            }
        }
        // The time her hands need to where that cut's swing begins, from where they are and as they are turned now (s):
        // no faster than WhipMost and TurnMost at their fastest, going evenly. The hand that cuts; the other counts 0.6.
        float Needs(const CutSwing& sw) const
        {
            float needs = 0;
            if (Previous.ModelSpaceMatrices.size() != Skeleton.Joints.size() || ArmChain[0][3] < 0 || ArmChain[1][3] < 0) return needs;
            for (int s = 0; s < 2; ++s)
            {
                XMVECTOR sc, q, t;
                XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&Previous.ModelSpaceMatrices[ArmChain[s][3]]));
                const float apart = XMVectorGetX(XMVector3Length(t - XMLoadFloat3(&sw.Hands[s])));
                const float turned = 2 * std::acos((std::min)(1.f, std::abs(XMVectorGetX(XMVector4Dot(XMQuaternionNormalize(q), XMLoadFloat4(&sw.Turns[s]))))));
                needs = (std::max)(needs, 1.875f * (std::max)(apart / WhipMost, turned / TurnMost) * (sw.Cuts[s] ? 1.f : .6f));
            }
            return needs;
        }
        // How long before its blow a cut is entered (clip seconds; 2026-11-19). What her body was doing is carried into
        // the cut and dies away evenly until just before the blow (CutEnd): so her hand does not go all the way back to
        // where the clip winds up and wait there, it goes round by a shorter way and meets the swing as that comes
        // forward. The longer before the blow, the more of the clip's wind-up she makes and the further her hand has to
        // go. Each of a few lengths is played here as Compose will play it: the clip from that point, what is carried
        // over dying away as there (CarryAt, CarryTurn, CarryGo, CarrySpin), her joints evaluated and her hands where
        // they put them, until the swing begins. The one taken is that in which her hand is slowest at its fastest, a
        // longer one costing a little (LeadCost). (A first form guessed her hand's way as the clip's and a straight line
        // to it; her joints take it on arcs, and out of her guard into L1 her hand went in at 9 m/s where much less was
        // guessed, stopped, and struck at 11: two moves.)
        float BestLead(const std::string& name, const CutSwing& sw) const
        {
            constexpr float Leads[] = {.26f, .30f, .34f, .38f, .42f, .46f};
            constexpr float Tick = 1 / 60.f;
            const size_t n = Skeleton.Joints.size();
            const int hand = ArmChain[sw.Cuts[0] ? 0 : 1][3], other = ArmChain[sw.Cuts[0] ? 1 : 0][3];
            if (Previous.LocalJoints.size() != n || Hips < 0 || hand < 0 || other < 0) return Leads[1];
            const bool moving = Before.LocalJoints.size() == n && LastDt > 1e-4f;
            const float hit = Hit(name);
            DoorwaysSkeletonPose entry, pose;
            std::vector<XMFLOAT3> at(n), go(n), turned(n), spin(n);
            float best = Leads[1], least = 1e9f;
            for (const float lead : Leads)
            {
                const float start = (std::max)(0.f, hit - lead), seconds = (lead - CutEnd) / AttackRate;
                if (!Sample(name, start, entry, nullptr) || entry.LocalJoints.size() != n) continue;
                for (size_t i = 0; i < n; ++i)
                {
                    const auto& was = Previous.LocalJoints[i]; const auto& now = entry.LocalJoints[i];
                    const XMVECTOR apart = XMLoadFloat3(&was.Translation) - XMLoadFloat3(&now.Translation);
                    const XMVECTOR turn = TurnVector(XMQuaternionMultiply(XMQuaternionInverse(XMLoadFloat4(&now.Rotation)), XMLoadFloat4(&was.Rotation)));
                    XMVECTOR going = XMVectorZero(), spinning = XMVectorZero();
                    if (moving)
                    {
                        const auto& before = Before.LocalJoints[i];
                        going = (XMLoadFloat3(&was.Translation) - XMLoadFloat3(&before.Translation)) / LastDt;
                        spinning = TurnVector(XMQuaternionMultiply(XMQuaternionInverse(XMLoadFloat4(&before.Rotation)), XMLoadFloat4(&was.Rotation))) / LastDt;
                        going = XMVector3ClampLength(going, 0, 3 * XMVectorGetX(XMVector3Length(apart)) / seconds);
                        spinning = XMVector3ClampLength(spinning, 0, 3 * XMVectorGetX(XMVector3Length(turn)) / seconds);
                    }
                    XMStoreFloat3(&at[i], apart); XMStoreFloat3(&turned[i], turn); XMStoreFloat3(&go[i], going); XMStoreFloat3(&spin[i], spinning);
                }
                float top = 0;
                XMVECTOR last[2] = {XMVectorZero(), XMVectorZero()};
                for (int f = 0; ; ++f)
                {
                    const float t = f * Tick, played = start + t * AttackRate;
                    if (played > hit - sw.Lead || !Sample(name, played, pose, nullptr) || pose.LocalJoints.size() != n) break;
                    const float g = (std::min)(1.f, t / seconds), rest = 1 - g;
                    const float wAt = 1 - g * g * g * (10 - 15 * g + 6 * g * g), wGo = t * rest * rest * rest;
                    for (size_t i = 0; i < n; ++i)
                    {
                        auto& to = pose.LocalJoints[i];
                        XMStoreFloat3(&to.Translation, XMLoadFloat3(&to.Translation) + XMLoadFloat3(&at[i]) * wAt + XMLoadFloat3(&go[i]) * wGo);
                        const XMVECTOR turn = XMLoadFloat3(&turned[i]) * wAt + XMLoadFloat3(&spin[i]) * wGo;
                        XMStoreFloat4(&to.Rotation, XMQuaternionNormalize(XMQuaternionMultiply(XMLoadFloat4(&to.Rotation), TurnOf(turn))));
                    }
                    if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) break;
                    const auto& h = pose.ModelSpaceMatrices[Hips];
                    for (int k = 0; k < 2; ++k)
                    {
                        const auto& m = pose.ModelSpaceMatrices[k == 0 ? hand : other];
                        const XMVECTOR p = XMVectorSet(m._41 - h._41, m._42 - h._42, m._43 - h._43, 0);
                        if (f > 0) top = (std::max)(top, XMVectorGetX(XMVector3Length(p - last[k])) / Tick * (k == 0 ? 1.f : .6f));
                        last[k] = p;
                    }
                }
                const float cost = top + LeadCost * (lead - Leads[0]);
                if (cost < least) { least = cost; best = lead; }
            }
            return best;
        }
        // The cut she makes next (2026-11-19). With the same hand again: the next of its row. With the other hand: the
        // one of that hand's cuts that her hands can come to soonest from where they are, and not the one that hand made
        // last: so left and right in turn go into one another however they are mixed. (They were taken by their number
        // in the row, whatever the other hand had just done: up to 1.4 m for her hand to go between two blows.)
        const char* NextCut(int hand)      // 1: left, 2: right
        {
            const char* const* row = hand == 1 ? LeftChain : RightChain;
            const int count = static_cast<int>(hand == 1 ? std::size(LeftChain) : std::size(RightChain)), h = hand == 1 ? 0 : 1;
            if (Attack.empty() || Attack == "Attack_Jump" || Skill()) { RowAt[0] = RowAt[1] = 0; RowUsed[0] = RowUsed[1] = false; }
            else if (hand == LastHand) RowAt[h] = (RowAt[h] + 1) % count;
            else
            {
                int best = -1; float least = 1e9f;
                for (int i = 0; i < count; ++i)
                {
                    const auto swing = Swings.find(row[i]);
                    if (swing == Swings.end() || (RowUsed[h] && i == RowAt[h])) continue;
                    const float d = Needs(swing->second);
                    if (d < least) { least = d; best = i; }
                }
                RowAt[h] = best >= 0 ? best : (RowAt[h] + 1) % count;
            }
            LastHand = hand; RowUsed[h] = true;
            return row[RowAt[h]];
        }
        // Her wrists in the cuts (2026-11-19; owner: "during attacks also, wrist rotates not right"). The clips' wrists
        // were made when the clips were built, frame by frame, and held at the most a wrist bends: where that set in and
        // let go the hand flicked (15 to 20 degrees from one frame to the next, before the swing and after it), and with
        // it the blade. The keys of the hand that cuts are smoothed here, each with its neighbours over a tenth of a
        // second. (The blade's roll about its own length through a swing, 120 to 230 degrees, is her forearm's own, as
        // it was played: holding the hand's roll to the forearm changed it by nothing to speak of.)
        void SmoothWrists()
        {
            for (auto& entry : Clips)
            {
                const std::string& name = entry.first;
                if (name.rfind("Attack", 0) != 0 || Hit(name) <= 0) continue;
                const bool left = name.rfind("Attack_L", 0) == 0, right = name.rfind("Attack_R", 0) == 0;
                for (int s = 0; s < 2; ++s)
                {
                    if ((s == 0 && right) || (s == 1 && left) || ArmChain[s][3] < 0) continue;
                    for (auto& track : entry.second.JointTracks)
                    {
                        if (static_cast<int>(track.JointIndex) != ArmChain[s][3] || track.RotationKeys.size() < 5) continue;
                        auto& keys = track.RotationKeys;
                        const int count = static_cast<int>(keys.size());
                        std::vector<XMFLOAT4> was(keys.size());
                        for (int k = 0; k < count; ++k) was[k] = keys[k].Value;
                        for (int k = 0; k < count; ++k)
                        {
                            const XMVECTOR here = XMLoadFloat4(&was[k]);
                            XMVECTOR sum = XMVectorZero();
                            for (int d = -3; d <= 3; ++d)
                            {
                                XMVECTOR q = XMLoadFloat4(&was[std::clamp(k + d, 0, count - 1)]);
                                if (XMVectorGetX(XMVector4Dot(q, here)) < 0) q = XMVectorNegate(q);
                                sum += q * std::exp(-d * d / 3.4f);
                            }
                            XMStoreFloat4(&keys[k].Value, XMQuaternionNormalize(sum));
                        }
                    }
                }
            }
        }

        // Each cut's wind-up as one move into its swing (2026-11-19; the owner: "animations seems like concated wrong
        // way"). In four cuts (R2, R3, R4, L3) Mixamo's arm cocks the blade in a tenth of a second, at 8 to 12 m/s (nearly
        // the blow's own speed), stops, and then strikes: two moves. From WindUpFrom before the blow to where her hand
        // turns into the swing she now goes through the clip's own poses, all of her, but evenly: the distance her hand
        // goes along its way (against her hips) goes as a cubic from the clip's pace at one end to its pace at the other,
        // in the same time. Every frame is still one of the clip's own poses; the blow is where it was. A cut whose
        // wind-up is calm already (at most WindUpCalm of its blow's speed) is left as it is. (2026-11-20: R3's cock, 44 %
        // of its blow, was left just under the line at 45 %, and still read as a twitch before the blow; at 40 % it is
        // made one move too, and L1's, 35 %, is still left.)
        void EvenWindUps()
        {
            constexpr float WindUpFrom = .45f, WindUpCalm = .40f, Tick = 1 / 240.f, KeysEvery = 1 / 120.f;
            if (Hips < 0) return;
            for (auto& entry : Clips)
            {
                const std::string& name = entry.first;
                const float hit = Hit(name);
                if (name.rfind("Attack_", 0) != 0 || name == "Attack_Jump" || hit <= WindUpFrom + .02f || hit + .06f > entry.second.DurationSeconds) continue;
                const int s = name.rfind("Attack_L", 0) == 0 ? 0 : 1;
                if (ArmChain[s][0] < 0 || ArmChain[s][3] < 0) continue;
                // her hand's way against her hips, 240 times a second, from WindUpFrom before the blow to just after it
                const int count = static_cast<int>((WindUpFrom + .05f) / Tick) + 1, blow = static_cast<int>(std::lround(WindUpFrom / Tick));
                std::vector<XMFLOAT3> way(static_cast<size_t>(count));
                DoorwaysSkeletonPose pose;
                bool whole = true;
                for (int i = 0; i < count && whole; ++i)
                {
                    whole = Sample(name, hit - WindUpFrom + i * Tick, pose, nullptr) && EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr);
                    if (!whole) break;
                    const auto& m = pose.ModelSpaceMatrices[ArmChain[s][3]];
                    const auto& h = pose.ModelSpaceMatrices[Hips];
                    way[i] = {m._41 - h._41, m._42 - h._42, m._43 - h._43};
                }
                if (!whole) continue;
                const auto gap = [&](int i, int k) { return XMVectorGetX(XMVector3Length(XMLoadFloat3(&way[k]) - XMLoadFloat3(&way[i]))); };
                std::vector<float> pace(static_cast<size_t>(count), 0.f), even(static_cast<size_t>(count), 0.f);
                for (int i = 1; i + 1 < count; ++i) pace[i] = gap(i - 1, i + 1) / (2 * Tick);
                pace[0] = pace[1]; pace[count - 1] = pace[count - 2];
                for (int i = 0; i < count; ++i)   // (over a thirtieth of a second: a wobble is not a turn)
                {
                    float sum = 0; int n = 0;
                    for (int d = -4; d <= 4; ++d) if (i + d >= 0 && i + d < count) { sum += pace[i + d]; ++n; }
                    even[i] = sum / static_cast<float>(n);
                }
                int top = blow - static_cast<int>(.12f / Tick);
                for (int i = top; i < count; ++i) if (even[i] > even[top]) top = i;
                // back from the swing's top to where her hand turns into it: the least speed before the swing, once the
                // speed plainly climbs again behind it
                int turn = top;
                float least = even[top];
                for (int i = top - 1; i > 0; --i)
                {
                    if (even[i] < least) { least = even[i]; turn = i; }
                    else if (even[i] > least * 1.3f + .3f) break;
                }
                float burst = 0;
                for (int i = 0; i < turn; ++i) burst = (std::max)(burst, even[i]);
                const auto leave = [&](const char* why)
                {
                    char said[160];
                    std::snprintf(said, sizeof(said), "%s (%s: its wind-up %.1f m/s, its blow %.1f, its turn %.2f s before the blow); ",
                                  name.c_str(), why, burst, even[top], (blow - turn) * Tick);
                    WindUpsLeft += said;
                };
                if ((blow - turn) * Tick < .03f || (blow - turn) * Tick > .25f) { leave("no turn into its swing"); continue; }
                if (burst <= WindUpCalm * even[top]) { leave("calm"); continue; }
                std::vector<float> along(static_cast<size_t>(turn) + 1, 0.f);
                for (int i = 1; i <= turn; ++i) along[i] = along[i - 1] + gap(i - 1, i);
                const float length = along[turn], span = turn * Tick, from = hit - WindUpFrom, to = from + span;
                if (length < .05f) continue;
                float v0 = pace[0] * span / length, v1 = pace[turn] * span / length;    // (the cubic's slopes at its ends)
                if (const float over = std::hypot(v0, v1) / 3; over > 1) { v0 /= over; v1 /= over; }   // (so it never goes back)
                const auto shown = [&](float u)   // a share u of the window: the clip's own time whose pose is shown then
                {
                    const float u2 = u * u, u3 = u2 * u;
                    const float want = std::clamp((u3 - 2 * u2 + u) * v0 + (3 * u2 - 2 * u3) + (u3 - u2) * v1, 0.f, 1.f) * length;
                    const int i = std::clamp(static_cast<int>(std::upper_bound(along.begin(), along.end(), want) - along.begin()) - 1, 0, turn - 1);
                    const float piece = along[i + 1] - along[i];
                    return from + (static_cast<float>(i) + (piece > 1e-6f ? std::clamp((want - along[i]) / piece, 0.f, 1.f) : 0.f)) * Tick;
                };
                const int steps = (std::max)(2, static_cast<int>(std::ceil(span / KeysEvery)));
                for (auto& track : entry.second.JointTracks)
                {
                    const auto redo = [&](auto& keys, auto mix)
                    {
                        if (keys.size() < 2) return;
                        const auto at = [&](float t)
                        {
                            size_t k = 0;
                            while (k + 1 < keys.size() && keys[k + 1].TimeSeconds <= t) ++k;
                            if (k + 1 >= keys.size() || t <= keys[k].TimeSeconds) return keys[k].Value;
                            const float w = (t - keys[k].TimeSeconds) / (std::max)(1e-6f, keys[k + 1].TimeSeconds - keys[k].TimeSeconds);
                            return mix(keys[k].Value, keys[k + 1].Value, w);
                        };
                        auto made = keys;
                        made.clear();
                        for (const auto& key : keys) if (key.TimeSeconds < from - 1e-5f) made.push_back(key);
                        for (int k = 0; k <= steps; ++k)
                        {
                            auto key = keys.front();
                            key.TimeSeconds = from + span * static_cast<float>(k) / static_cast<float>(steps);
                            key.Value = at(shown(static_cast<float>(k) / static_cast<float>(steps)));
                            made.push_back(key);
                        }
                        for (const auto& key : keys) if (key.TimeSeconds > to + 1e-5f) made.push_back(key);
                        keys = std::move(made);
                    };
                    redo(track.RotationKeys, [](const XMFLOAT4& a, const XMFLOAT4& b, float w)
                    {
                        XMFLOAT4 r;
                        XMStoreFloat4(&r, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&a), XMLoadFloat4(&b), w)));
                        return r;
                    });
                    const auto line = [](const XMFLOAT3& a, const XMFLOAT3& b, float w) { return XMFLOAT3{a.x + (b.x - a.x) * w, a.y + (b.y - a.y) * w, a.z + (b.z - a.z) * w}; };
                    redo(track.TranslationKeys, line);
                    redo(track.ScaleKeys, line);
                }
                char said[160];
                std::snprintf(said, sizeof(said), "%s (its cock %.1f m/s, its blow %.1f, over %.2f s to %.2f s before the blow); ",
                              name.c_str(), burst, even[top], span, (blow - turn) * Tick);
                WindUpsEvened += said;
            }
        }

        // ---- her hands to the handles on her back (2026-11-19; owner: "hands are not following the skills and combos
        // properly"). To take her swords or put them away her arms went, joint by joint, the shortest way to one drawn
        // pose (the bundle's Sword_Reach) and on. From arms hanging that way is out to the sides: both arms were flung
        // wide (a T for a frame or two, her hands at 13 to 16 m/s) and then folded behind her head, elbows out; and the
        // pose's hands are 16 and 24 cm short of where the handles lie, so each sword jumped that far into its fist.
        // A first remedy took each hand to the handle itself, its turn going evenly between the pose's and the grip's:
        // her blades then passed through her head and neck (6 to 9 cm from the middle of her head), her elbows jumped
        // for a frame where a wrist passed 5 cm from its shoulder (17 to 26 m/s), and her wrists were bent 100 to 128
        // degrees on the way. Now (worked out in a model of her arm before it was built):
        // - the grip: the sword lies on her back as her chest carries it; her hand closes round the handle's middle with
        //   its knuckles along the blade, turned about the handle to where her forearm comes from, and no further bent
        //   than a wrist bends (the first of WristUlnar, WristRadial, WristFlex): by what is over, the handle lies askew in her fist;
        // - her wrist's way: from where the pose has it, round before and above her shoulder (never nearer the joint
        //   than a hand's breadth), to the grip; slow away and slow in;
        // - her elbow: two bones of fixed length leave it a circle; where on it is given by a curve of elbow places (the
        //   pose's, out at her side, up by her ear) and followed at a limited pace, carried along with the line from
        //   her shoulder to her wrist: it cannot flip;
        // - her upper arm is turned as an elbow allows (a hinge: the plane her arm bends in), keeping the roll the pose
        //   has against that plane; her forearm and hand are carried from frame to frame with her forearm's line and
        //   rolled about it toward where the blade her fist would hold is nearest the sword's own, at a limited rate;
        //   her wrist's bend goes between the pose's and the grip's; near the pose her arm is the pose's own, carried
        //   to where her elbow and wrist are;
        // - the sword: its blade goes a way of its own between her back and the pose (out past her shoulder, never
        //   through her head), evenly along it and ahead of her hand; its handle's middle stays in her fist's middle.
        //   So it turns in her fist by what is left, and is on her back or in the pose's grip exactly at either end.
        // Leaving the handles this is gone through as time passes (the pose she goes to moves on); going to them it is
        // the same way back: gone through from the handle to where her hand is, anew every frame.
        static XMVECTOR ElbowAt(XMVECTOR shoulder, XMVECTOR& hand, float upper, float fore, XMVECTOR pole, XMVECTOR* side)
        {
            XMVECTOR to = hand - shoulder;
            float apart = XMVectorGetX(XMVector3Length(to));
            if (apart < 1e-5f) { to = XMVectorSet(0, -1, 0, 0); apart = 1; }
            const XMVECTOR axis = to / apart;
            const float reach = std::clamp(apart, std::abs(upper - fore) + .01f, upper + fore - .001f);
            hand = shoulder + axis * reach;
            const float along = (upper * upper - fore * fore + reach * reach) / (2 * reach), off = std::sqrt((std::max)(0.f, upper * upper - along * along));
            XMVECTOR out = pole - axis * XMVector3Dot(pole, axis);
            if (XMVectorGetX(XMVector3LengthSq(out)) < 1e-8f) out = XMVector3Cross(axis, XMVectorSet(1, 0, 0, 0));
            if (XMVectorGetX(XMVector3LengthSq(out)) < 1e-8f) out = XMVector3Cross(axis, XMVectorSet(0, 0, 1, 0));
            out = XMVector3Normalize(out);
            if (side) *side = out;
            return shoulder + axis * along + out * off;
        }
        // the turn that takes one line and a second across it to another such pair
        static XMVECTOR FrameTurn(XMVECTOR line0, XMVECTOR across0, XMVECTOR line1, XMVECTOR across1)
        {
            const auto frame = [](XMVECTOR line, XMVECTOR across)
            {
                line = XMVector3Normalize(line);
                across = across - line * XMVector3Dot(across, line);
                if (XMVectorGetX(XMVector3LengthSq(across)) < 1e-8f) across = XMVector3Cross(line, XMVectorSet(1, 0, 0, 0));
                if (XMVectorGetX(XMVector3LengthSq(across)) < 1e-8f) across = XMVector3Cross(line, XMVectorSet(0, 1, 0, 0));
                across = XMVector3Normalize(across);
                XMMATRIX m = XMMatrixIdentity();
                m.r[0] = XMVectorSetW(line, 0); m.r[1] = XMVectorSetW(across, 0); m.r[2] = XMVectorSetW(XMVector3Cross(line, across), 0);
                return m;
            };
            return XMQuaternionNormalize(XMQuaternionRotationMatrix(XMMatrixTranspose(frame(line0, across0)) * frame(line1, across1)));
        }
        void LearnGrip()
        {
            GripKnown = false;
            DoorwaysSkeletonPose pose;
            for (int s = 0; s < 2; ++s)
                for (int w = 0; w < 4; ++w) if (ArmChain[s][w] < 0) return;
            if (SwordJoint[0] < 0 || SwordJoint[1] < 0 || Knuckle[0][0] < 0 || Knuckle[0][1] < 0 || Knuckle[1][0] < 0 || Knuckle[1][1] < 0) return;
            for (int s = 0; s < 2; ++s)
                if (Skeleton.Joints[SwordJoint[s]].ParentIndex != ArmChain[s][3] || Skeleton.Joints[ArmChain[s][3]].ParentIndex != ArmChain[s][2] ||
                    Skeleton.Joints[ArmChain[s][2]].ParentIndex != ArmChain[s][1] || Skeleton.Joints[ArmChain[s][1]].ParentIndex < 0) return;
            // a sword in its fist, and a hand's own lines: from its wrist to the middle of its knuckles, along its
            // knuckles, out of its palm
            if (!Sample("Idle", 0, pose, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) return;
            const auto place = [&](int j) { const auto& m = pose.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            const auto turnOf = [&](int j) { XMVECTOR sc, q, t; XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&pose.ModelSpaceMatrices[j])); return XMQuaternionNormalize(q); };
            for (int s = 0; s < 2; ++s)
            {
                const XMVECTOR qHand = turnOf(ArmChain[s][3]), qSword = turnOf(SwordJoint[s]), wrist = place(ArmChain[s][3]);
                const XMVECTOR intoHand = XMQuaternionConjugate(qHand), intoSword = XMQuaternionConjugate(qSword);
                const XMMATRIX sword = XMLoadFloat4x4(&pose.ModelSpaceMatrices[SwordJoint[s]]);
                const XMVECTOR middle = XMVector3TransformCoord(XMLoadFloat3(&SwordGrip[s]), sword), tip = XMVector3TransformCoord(XMLoadFloat3(&SwordTip[s]), sword);
                const XMVECTOR line = XMVector3Normalize((place(Knuckle[s][0]) + place(Knuckle[s][1])) * .5f - wrist);
                XMVECTOR along = place(Knuckle[s][0]) - place(Knuckle[s][1]);
                along -= line * XMVector3Dot(along, line);
                if (XMVectorGetX(XMVector3LengthSq(along)) < 1e-8f || XMVectorGetX(XMVector3LengthSq(tip - middle)) < 1e-6f) return;
                along = XMVector3Normalize(along);
                XMStoreFloat3(&HandLine[s], XMVector3Rotate(line, intoHand));
                XMStoreFloat3(&HandAcross[s], XMVector3Rotate(along, intoHand));
                XMStoreFloat3(&HandPalm[s], XMVector3Rotate(XMVector3Cross(line, along), intoHand));
                XMStoreFloat3(&GripInHand[s], XMVector3Rotate(middle - wrist, intoHand));
                XMStoreFloat3(&GripInSword[s], XMVector3Rotate(middle - place(SwordJoint[s]), intoSword));
                XMStoreFloat3(&BladeInSword[s], XMVector3Rotate(XMVector3Normalize(tip - middle), intoSword));
                XMStoreFloat4(&SwordTurnInHand[s], XMQuaternionNormalize(After(intoHand, qSword)));
            }
            // her arms bent, as the bundle's reach pose has them: how an upper arm and a forearm are turned about themselves
            if (!Sample("Sword_Reach", 0, pose, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) return;
            for (int s = 0; s < 2; ++s)
            {
                const XMVECTOR upper = XMVector3Normalize(place(ArmChain[s][2]) - place(ArmChain[s][1])), fore = XMVector3Normalize(place(ArmChain[s][3]) - place(ArmChain[s][2]));
                const XMVECTOR hinge = XMVector3Cross(upper, fore);
                if (XMVectorGetX(XMVector3LengthSq(hinge)) < .04f) return;      // (not bent: no use)
                XMStoreFloat4(&ArmBent[s].Upper, turnOf(ArmChain[s][1])); XMStoreFloat4(&ArmBent[s].Fore, turnOf(ArmChain[s][2]));
                XMStoreFloat3(&ArmBent[s].UpperLine, upper); XMStoreFloat3(&ArmBent[s].ForeLine, fore); XMStoreFloat3(&ArmBent[s].Hinge, XMVector3Normalize(hinge));
                XMStoreFloat3(&ForeAxis[s], XMVector3Rotate(fore, XMQuaternionConjugate(turnOf(ArmChain[s][2]))));
            }
            GripKnown = true;
        }
        static float Ramp(float x, float from, float to) { x = std::clamp((x - from) / (to - from), 0.f, 1.f); return x * x * (3 - 2 * x); }
        static XMVECTOR After(XMVECTOR second, XMVECTOR first) { return XMQuaternionMultiply(first, second); }   // one turn after another
        static XMVECTOR Unit(XMVECTOR v, XMVECTOR otherwise) { const float l = XMVectorGetX(XMVector3Length(v)); return l > 1e-6f ? v / l : otherwise; }
        static XMVECTOR Across(XMVECTOR v, XMVECTOR axis, bool& some)   // what of v lies across axis, a length of one (some: there is any)
        {
            const XMVECTOR o = v - axis * XMVector3Dot(v, axis);
            const float l = XMVectorGetX(XMVector3Length(o));
            some = l > 1e-5f && l > 1e-3f * XMVectorGetX(XMVector3Length(v));
            return some ? o / l : XMVectorZero();
        }
        static XMVECTOR TwistLast(XMVECTOR q, XMVECTOR axis)   // q as a swing and then a turn about axis: that turn
        {
            const XMVECTOR back = XMQuaternionConjugate(q);
            const XMVECTOR t = XMVectorSetW(axis * XMVector3Dot(back, axis), XMVectorGetW(back));
            if (XMVectorGetX(XMVector4LengthSq(t)) < 1e-12f) return XMQuaternionIdentity();
            return XMQuaternionConjugate(XMQuaternionNormalize(t));
        }
        // how far a wrist is bent: sideways (toward its knuckles' line) and up or down, degrees; her forearm's line as her hand sees it
        void WristBend(int s, XMVECTOR qHand, XMVECTOR foreLine, float& sideways, float& updown) const
        {
            const XMVECTOR f = XMVector3Rotate(foreLine, XMQuaternionConjugate(qHand));
            const float on = XMVectorGetX(XMVector3Dot(f, XMLoadFloat3(&HandLine[s]))), over = XMVectorGetX(XMVector3Dot(f, XMLoadFloat3(&HandAcross[s])));
            const float out = XMVectorGetX(XMVector3Dot(f, XMLoadFloat3(&HandPalm[s])));
            sideways = XMConvertToDegrees(std::asin(std::clamp(over, -1.f, 1.f)));
            updown = XMConvertToDegrees(std::atan2(out, on));
        }
        // Her wrist no further than a wrist bends (WristUlnar, WristRadial, WristFlex): within the first of each pair as it
        // is; beyond, giving ever less up to the second (soft), or held at the first (!soft). weight: how much of that.
        // A wrist is bent so far and some way (toward its little finger or thumb, up or down, or between): how far it may
        // go each way is an ellipse through the sideways and the up-down limits, and only how far is held, not the way.
        // (Held as a sideways and an up-down angle each alone, a hand bent near 90 degrees sideways - the spin's - has an
        // up-down angle that means nothing: it read -158, was held to -80, and the hand stayed bent 83 degrees.)
        XMVECTOR HoldWrist(int s, XMVECTOR qHand, XMVECTOR foreLine, bool soft, float weight = 1) const
        {
            const XMVECTOR f = XMVector3Rotate(foreLine, XMQuaternionConjugate(qHand));         // her forearm's line as her hand sees it
            const XMVECTOR line = XMLoadFloat3(&HandLine[s]), across = XMLoadFloat3(&HandAcross[s]), palm = XMLoadFloat3(&HandPalm[s]);
            const float on = XMVectorGetX(XMVector3Dot(f, line)), over = XMVectorGetX(XMVector3Dot(f, across)), outward = XMVectorGetX(XMVector3Dot(f, palm));
            const float aside = std::sqrt(over * over + outward * outward);
            if (aside < 1e-5f) return qHand;
            const float bent = std::atan2(aside, on), c = over / aside, sn = outward / aside;
            const auto reach = [c, sn](int k)
            {
                const float side = XMConvertToRadians(c >= 0 ? WristUlnar[k] : WristRadial[k]), flex = XMConvertToRadians(WristFlex[k]);
                return 1 / std::sqrt((c / side) * (c / side) + (sn / flex) * (sn / flex));
            };
            const float knee = reach(0);
            if (bent <= knee) return qHand;
            const float most = reach(1), held = soft ? knee + (most - knee) * std::tanh((bent - knee) / (most - knee)) : knee;
            const float b1 = bent + (held - bent) * weight;
            const XMVECTOR allowed = line * std::cos(b1) + (across * c + palm * sn) * std::sin(b1);
            return XMQuaternionNormalize(After(ArcTurn(XMVector3Normalize(XMVector3Rotate(allowed, qHand)), foreLine), qHand));
        }
        // Her wrists (2026-11-19; owner: "hands are not following the skills and combos properly"). The clips bend them
        // further than a wrist goes through most of every cut and skill: the cutting hand 51 to 53 degrees toward its
        // little finger, the guard's right hand 59 toward its thumb, the spinning swords 87 and 78 (each blade laid along
        // its arm), the slide, the cast and the throw 55 to 82 toward the thumb and 92 to 106 up or down. With her swords
        // in her hands each wrist is now held to what a wrist does (HoldWrist), and her sword keeps its own way: the hand
        // gives, the handle turns in her fist about its middle (a sabre's grip, the handle aslant across the palm), and the
        // blade is where the clip has it. weight: how much of this (it comes as her swords come into her hands, and goes).
        void HoldWrists(DoorwaysSkeletonPose& out, float weight)
        {
            if (!GripKnown || weight <= 0 || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr)) return;
            const auto place = [&](int j) { const auto& m = out.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            const auto turnOf = [&](int j) { XMVECTOR sc, q, t; XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[j])); return XMQuaternionNormalize(q); };
            for (int s = 0; s < 2; ++s)
            {
                const int fa = ArmChain[s][2], ha = ArmChain[s][3], sj = SwordJoint[s];
                const XMVECTOR wrist = place(ha), qFore = turnOf(fa), qHand = turnOf(ha);
                const XMVECTOR foreLine = Unit(wrist - place(fa), XMVectorZero());
                if (XMVectorGetX(XMVector3LengthSq(foreLine)) < .5f) continue;
                const XMVECTOR held = HoldWrist(s, qHand, foreLine, true, weight);
                if (XMVector4Equal(held, qHand)) continue;
                XMStoreFloat4(&out.LocalJoints[ha].Rotation, XMQuaternionNormalize(After(XMQuaternionConjugate(qFore), held)));
                if (!InHand) continue;
                // her sword: its own turn kept, the middle of its handle where the middle of her fist now is (her swords out
                // too, 2026-11-19: PlaceSwords puts them where they are from this, and hands them back as this holds them)
                XMVECTOR sc, q, t;
                if (!XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[ha]))) sc = XMVectorSplatOne();
                const XMVECTOR qSword = turnOf(sj), swordAt = place(sj);
                const XMVECTOR middle = XMVector3TransformCoord(XMLoadFloat3(&SwordGrip[s]), XMLoadFloat4x4(&out.ModelSpaceMatrices[sj]));
                const XMVECTOR middleNow = wrist + XMVector3Rotate(XMVector3Rotate(middle - wrist, XMQuaternionConjugate(qHand)), held);
                XMStoreFloat4(&out.LocalJoints[sj].Rotation, XMQuaternionNormalize(After(XMQuaternionConjugate(held), qSword)));
                XMStoreFloat3(&out.LocalJoints[sj].Translation, XMVector3Rotate(swordAt + middleNow - middle - wrist, XMQuaternionConjugate(held)) / XMVectorSetW(sc, 1));
            }
        }
        // The feather storm (C; 2026-11-19). The owner: "instead of this skill - i want nyxara to jump up make 360 degree
        // turn 3 times and she should throw wings feathers around at the same time while rotating". It is made here, as a
        // clip of its own (Skill_Storm), out of her clips: her body crouches and springs as it does taking off into flight
        // (Fly_Rise), hangs as it does there, and comes down and lands as it does out of flight (Fly_Land); her arms hold
        // her swords as on guard (Idle); her wings go up as she crouches, are beaten down as she leaves the ground and are
        // spread wide (the top of Fly's beat, and where its beat down has them level); and her hips turn her three whole
        // times about herself, gathering speed and losing it, each wing bone trailing the turn as it goes. How high she
        // goes is her picture's (StormLift); her feathers are thrown in the frame code, from where her wings are.
        void MakeStorm()
        {
            constexpr float KeyRate = 60.f;
            if (Hips < 0 || Duration("Fly_Rise") < 1.15f || Duration("Fly_Land") < .95f || Duration("Fly") <= 0 || Duration("Idle") <= 0) return;
            for (const auto& wing : WingJoint) for (const int j : wing) if (j < 0) return;
            const size_t n = Skeleton.Joints.size();
            const int chest = Skeleton.Joints[WingJoint[0][0]].ParentIndex;
            if (chest < 0 || Wing.size() != n || ArmJoint.size() != n) return;
            const auto turnOf = [](const DoorwaysSkeletonPose& q, int j)
            {
                XMVECTOR sc, r, t;
                XMMatrixDecompose(&sc, &r, &t, XMLoadFloat4x4(&q.ModelSpaceMatrices[j]));
                return XMQuaternionNormalize(r);
            };
            const auto at = [](const DoorwaysSkeletonPose& q, int j) { const auto& m = q.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            // her wings in Fly's beat as her chest on guard would hold them: at its top, and level on the way down
            DoorwaysSkeletonPose guard, pose, up, level;
            if (!Sample("Idle", 0, guard, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, guard, nullptr)) return;
            const XMVECTOR chestOnGuard = turnOf(guard, chest);
            const float beat = Duration("Fly");
            float rise[60] = {};
            int top = 0;
            for (int f = 0; f < 60; ++f)
            {
                if (!Sample("Fly", beat * f / 60, pose, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) return;
                const XMVECTOR way = XMVector3Rotate(XMVector3Rotate(at(pose, WingJoint[0][2]) - at(pose, WingJoint[0][0]), XMQuaternionConjugate(turnOf(pose, chest))), chestOnGuard);
                rise[f] = XMVectorGetY(XMVector3Normalize(way));
                if (rise[f] > rise[top]) top = f;
            }
            int flat = (top + 1) % 60;
            for (int k = 1; k < 45; ++k)
            {
                const int f = (top + k) % 60;
                if (std::abs(rise[f] - .05f) < std::abs(rise[flat] - .05f)) flat = f;
                if (rise[f] < .05f) break;
            }
            if (!Sample("Fly", beat * static_cast<float>(top) / 60, up, nullptr) || !Sample("Fly", beat * static_cast<float>(flat) / 60, level, nullptr)) return;
            WingsLevel = level; HasWingsLevel = true;   // (V's backflip spreads them so: MakeCast)
            // how far she has turned, every 1/120 s: her pace gathered and lost smoothly, StormTurns whole turns in all
            const int count = static_cast<int>(StormSeconds * 120) + 2;
            StormTurned.assign(static_cast<size_t>(count), 0.f);
            const auto pace = [](float t)
            {
                const float u = (t - StormSpinFrom) / (StormSpinTo - StormSpinFrom);
                if (u <= 0 || u >= 1) return 0.f;
                const auto s = [](float x) { x = std::clamp(x, 0.f, 1.f); return x * x * (3 - 2 * x); };
                return s(u / StormSpinIn) * s((1 - u) / StormSpinOut);
            };
            for (int i = 1; i < count; ++i) StormTurned[i] = StormTurned[i - 1] + pace((static_cast<float>(i) - .5f) / 120.f) / 120.f;
            const float whole = StormTurned[count - 1];
            if (whole <= 0) return;
            for (auto& a : StormTurned) a *= StormTurns * XM_2PI / whole;
            StormRateMost = 1e-3f;
            for (int i = 1; i < count; ++i) StormRateMost = (std::max)(StormRateMost, (StormTurned[i] - StormTurned[i - 1]) * 120.f);
            for (int k = 0, i = 0; k < 3; ++k)
            {   // half-way round each time
                while (i < count - 1 && StormTurned[i] < (2 * k + 1) * XM_PI) ++i;
                StormVolleyAt[k] = static_cast<float>(i) / 120.f;
            }
            // up, in the axes of her hips' parent (her place's up)
            XMVECTOR upright = XMVectorSet(0, 1, 0, 0);
            if (const int above = Skeleton.Joints[Hips].ParentIndex; above >= 0) upright = XMVector3Rotate(upright, XMQuaternionConjugate(turnOf(guard, above)));
            const auto mix = [](float t, float a, float c) { const float x = std::clamp((t - a) / (c - a), 0.f, 1.f); return x * x * (3 - 2 * x); };
            DoorwaysAnimationClip storm;
            storm.Name = "Skill_Storm"; storm.DurationSeconds = StormSeconds; storm.PlaybackSpeed = 1;
            storm.PlaybackMode = Clips.find("Fly_Rise")->second.PlaybackMode;
            storm.JointTracks.resize(n);
            for (size_t j = 0; j < n; ++j) storm.JointTracks[j].JointIndex = static_cast<std::uint32_t>(j);
            DoorwaysSkeletonPose body, arms;
            // her body's time in Fly_Rise, through these knots: crouched (.10 to .32), springing (to .47), hovering (to its
            // end); its pace changing smoothly between them (a monotone cubic: pieced straight, her legs' push stopped short
            // where the pace changed, her foot from 3.3 to 1.2 m/s in a frame)
            const float rose = Duration("Fly_Rise");
            const float knotT[4] = {0, StormCrouch, StormWingBeat, StormHang}, knotAt[4] = {.10f, .32f, .47f, rose};
            float slope[4] = {};
            {
                float secant[3];
                for (int i = 0; i < 3; ++i) secant[i] = (knotAt[i + 1] - knotAt[i]) / (knotT[i + 1] - knotT[i]);
                slope[0] = secant[0]; slope[3] = secant[2];
                for (int i = 1; i < 3; ++i) slope[i] = secant[i - 1] * secant[i] > 0 ? 2 * secant[i - 1] * secant[i] / (secant[i - 1] + secant[i]) : 0.f;
            }
            const auto riseAt = [&](float t)
            {
                int i = 0;
                while (i < 2 && t > knotT[i + 1]) ++i;
                const float h = knotT[i + 1] - knotT[i], s = std::clamp((t - knotT[i]) / h, 0.f, 1.f), s2 = s * s, s3 = s2 * s;
                return (2 * s3 - 3 * s2 + 1) * knotAt[i] + (s3 - 2 * s2 + s) * h * slope[i] + (3 * s2 - 2 * s3) * knotAt[i + 1] + (s3 - s2) * h * slope[i + 1];
            };
            const int frames = static_cast<int>(std::ceil(StormSeconds * KeyRate));
            for (int f = 0; f <= frames; ++f)
            {
                const float t = (std::min)(StormSeconds, static_cast<float>(f) / KeyRate);
                // her body: down and up and hovering (Fly_Rise), landing (Fly_Land)
                const bool rising = t < StormHang;
                if (!Sample(rising ? "Fly_Rise" : "Fly_Land", rising ? riseAt(t) : t - StormHang, body, nullptr) || !Sample("Idle", t, arms, nullptr)) return;
                pose = body;
                for (size_t j = 0; j < n; ++j)
                {
                    auto& to = pose.LocalJoints[j];
                    const bool sword = static_cast<int>(j) == SwordJoint[0] || static_cast<int>(j) == SwordJoint[1];
                    if (ArmJoint[j] || sword) to = arms.LocalJoints[j];
                    else if (Wing[j])
                    {   // from her guard's wings up, beaten down to level, held, and back
                        const XMVECTOR g = XMLoadFloat4(&arms.LocalJoints[j].Rotation), u = XMLoadFloat4(&up.LocalJoints[j].Rotation), l = XMLoadFloat4(&level.LocalJoints[j].Rotation);
                        const XMVECTOR q = t < StormWingUp ? XMQuaternionSlerp(g, u, mix(t, 0, StormWingUp))
                                         : t < StormWingBeat ? XMQuaternionSlerp(u, l, mix(t, StormWingUp, StormWingBeat))
                                         : t < StormWingClose ? l : XMQuaternionSlerp(l, g, mix(t, StormWingClose, StormWingFolded));
                        to = arms.LocalJoints[j];
                        XMStoreFloat4(&to.Rotation, XMQuaternionNormalize(q));
                    }
                }
                // her turn
                XMStoreFloat4(&pose.LocalJoints[Hips].Rotation, XMQuaternionNormalize(After(XMQuaternionRotationAxis(upright, StormTurn(t)), XMLoadFloat4(&pose.LocalJoints[Hips].Rotation))));
                if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) return;
                // her wings trail it, the outer bones more
                float rate = 0;
                StormTurn(t, &rate);
                const float trail = rate / StormRateMost;
                const int under = Skeleton.Joints[chest].ParentIndex;
                XMVECTOR axis = under >= 0 ? at(pose, chest) - at(pose, under) : XMVectorSet(0, 1, 0, 0);
                axis = XMVectorGetX(XMVector3LengthSq(axis)) < 1e-8f ? XMVectorSet(0, 1, 0, 0) : XMVector3Normalize(axis);
                for (int k = 0; k < 2 && trail > 1e-3f; ++k)
                {
                    const int* j = WingJoint[k];
                    const int root = Skeleton.Joints[j[0]].ParentIndex;
                    if (root < 0 || Skeleton.Joints[j[1]].ParentIndex != j[0] || Skeleton.Joints[j[2]].ParentIndex != j[1]) continue;
                    XMVECTOR parent = turnOf(pose, root);
                    float behind = 0;
                    for (int i = 0; i < 3; ++i)
                    {
                        behind += StormTrail[i] * trail;
                        const XMVECTOR now = XMQuaternionNormalize(After(XMQuaternionRotationAxis(axis, -behind), turnOf(pose, j[i])));
                        XMStoreFloat4(&pose.LocalJoints[j[i]].Rotation, XMQuaternionNormalize(After(XMQuaternionConjugate(parent), now)));
                        parent = now;
                    }
                }
                for (size_t j = 0; j < n; ++j)
                {
                    auto& tr = storm.JointTracks[j];
                    const auto& l = pose.LocalJoints[j];
                    XMFLOAT4 q = l.Rotation;   // (each key on the same side as the last: no key between them is turned the long way round)
                    if (!tr.RotationKeys.empty())
                    {
                        const XMFLOAT4& was = tr.RotationKeys.back().Value;
                        if (q.x * was.x + q.y * was.y + q.z * was.z + q.w * was.w < 0) q = {-q.x, -q.y, -q.z, -q.w};
                    }
                    tr.TranslationKeys.push_back({t, l.Translation});
                    tr.RotationKeys.push_back({t, q});
                    tr.ScaleKeys.push_back({t, l.Scale});
                }
            }
            Clips.insert_or_assign(std::string("Skill_Storm"), std::move(storm));
        }
        // V's clips and the twisted dash's pose (2026-11-19), made here from her others. The owner: "nyxara should be 180
        // degree position, now she has 90 degree position, and grab swords with her both hands and make a flip kick" ("i
        // meant backflip kick"); then "backflip should happen while grabbing swords back, before she should twist as she
        // used to do", and "she twists, sticks with two feet on [it], grab[s the] swords and make[s] then a backflip and
        // stand[s] still". Her picture is turned by TurnBody: these are her body within it, her hips' line upright.
        // - Cast_Drill: her twist as she used to twist it, Cast_Fly's body (straight, her legs together and pointed, her
        //   wings folded round her, her arms and head as there) stood up and held (her turns are BodyTwist; her picture
        //   lays her back, feet first).
        // - Cast_Flip: from there her feet stick on the creature and her legs give, crouching her on its side (Jump's
        //   tuck, her back bent forward; her picture turns her square to it), her wings rising over her as a bird's do as
        //   it lands on a rock (the top of Fly_Rise's beat: her chest is nearly upright there, and folded, or half open,
        //   they hung down to the ground), her arms going to her guard's and her hands opening (as Skill_Cast's let her
        //   swords go) and closing on the handles (ReachHandles takes them there); she kicks off it, her legs straight
        //   again and her wings beating down, level, tucks as she goes over, her legs reach down (Fly_Land) and she lands
        //   in its crouch, and stands up on guard (Idle), her wings folding.
        // - Dash_Twist: Cast_Drill's pose.
        // What the frame code needs of them is kept: her hips and her feet every 1/60 s, and her fists on guard.
        void MakeCast()
        {
            constexpr float KeyRate = 60.f;
            const size_t n = Skeleton.Joints.size();
            if (Hips < 0 || Upper.size() != n || ArmJoint.size() != n || Wing.size() != n || SwordJoint[0] < 0 || SwordJoint[1] < 0 ||
                FootJoint[0] < 0 || FootJoint[1] < 0 || ArmChain[0][0] < 0 || ArmChain[1][0] < 0 || (NeckJoint < 0 && HeadJoint < 0)) return;
            DoorwaysSkeletonPose fly, cast, guard, tuck, reach, crouch;
            if (!Sample("Cast_Fly", 0, fly, nullptr) || !Sample("Skill_Cast", Hit("Skill_Cast"), cast, nullptr) || !Sample("Idle", 0, guard, nullptr) ||
                !Sample("Jump", FlipTuckPose, tuck, nullptr) || !Sample("Fly_Land", FlipReachPose, reach, nullptr) ||
                !Sample("Fly_Land", FlipCrouchPose, crouch, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, guard, nullptr)) return;
            const auto turnOf = [](const DoorwaysSkeletonPose& q, int j)
            {
                XMVECTOR sc, r, t;
                XMMatrixDecompose(&sc, &r, &t, XMLoadFloat4x4(&q.ModelSpaceMatrices[j]));
                return XMQuaternionNormalize(r);
            };
            const auto at = [](const DoorwaysSkeletonPose& q, int j) { const auto& m = q.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            // a turn of her whole body (in her own space) given to her hips, as their parent has it
            const int above = Skeleton.Joints[Hips].ParentIndex;
            const XMVECTOR parent = above >= 0 ? turnOf(guard, above) : XMQuaternionIdentity();
            const auto turnHips = [&](DoorwaysSkeletonPose& q, XMVECTOR turn)
            {
                const XMVECTOR there = XMQuaternionMultiply(XMQuaternionMultiply(parent, turn), XMQuaternionConjugate(parent));
                XMStoreFloat4(&q.LocalJoints[Hips].Rotation, XMQuaternionNormalize(After(there, XMLoadFloat4(&q.LocalJoints[Hips].Rotation))));
            };
            XMStoreFloat3(&Upright, XMVector3Rotate(XMVectorSet(0, 1, 0, 0), XMQuaternionConjugate(parent)));
            GuardHips = XMVectorGetY(at(guard, Hips));
            // her joints: her legs (and all that hangs from her hips), her spine and head, her fingers, her swords
            std::vector<char> legs(n, 0), spine(n, 0), fingers(n, 0), sword(n, 0);
            for (size_t j = 0; j < n; ++j)
            {
                const auto& name = Skeleton.Joints[j].Name;
                sword[j] = static_cast<int>(j) == SwordJoint[0] || static_cast<int>(j) == SwordJoint[1];
                legs[j] = !Upper[j] && static_cast<int>(j) != Hips;
                spine[j] = Upper[j] && !ArmJoint[j] && !Wing[j] && !sword[j];
                fingers[j] = ArmJoint[j] && name.find("Hand") != std::string::npos && !name.ends_with("Hand");
            }
            // her twist: Cast_Fly's body, stood up straight, her hips' line upright, and facing as she was made (her
            // shoulders' line across her as there); how far it lay back is kept (her picture lays her back so on her way)
            if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, fly, nullptr)) return;
            DoorwaysSkeletonPose drill = fly;
            for (size_t j = 0; j < n; ++j) if (sword[j]) drill.LocalJoints[j] = guard.LocalJoints[j];   // (in her fists as on guard, once she has them)
            const XMVECTOR line = at(fly, NeckJoint >= 0 ? NeckJoint : HeadJoint) - at(fly, Hips);
            if (XMVectorGetX(XMVector3LengthSq(line)) < 1e-6f) return;
            DrillLean = std::atan2(-XMVectorGetZ(line), XMVectorGetY(line));
            turnHips(drill, ArcTurn(XMVector3Normalize(line), XMVectorSet(0, 1, 0, 0)));
            if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, drill, nullptr)) return;
            {
                const auto bound = [&](int j) { return XMMatrixInverse(nullptr, XMLoadFloat4x4(&Skeleton.Joints[j].InverseBindPose)).r[3]; };
                const auto level = [](XMVECTOR v)
                {
                    v = XMVectorSetW(XMVectorSetY(v, 0), 0);
                    return XMVectorGetX(XMVector3LengthSq(v)) < 1e-8f ? XMVectorSet(1, 0, 0, 0) : XMVector3Normalize(v);
                };
                turnHips(drill, ArcTurn(level(at(drill, ArmChain[1][0]) - at(drill, ArmChain[0][0])), level(bound(ArmChain[1][0]) - bound(ArmChain[0][0]))));
                if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, drill, nullptr)) return;
            }
            XMStoreFloat3(&DiveHips, at(drill, Hips));
            // on it: Jump's tuck, her back bent FlipSquatLean forward, a share at each joint of her spine (each a turn about
            // her right, as her body has it)
            DoorwaysSkeletonPose squat = tuck;
            if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, squat, nullptr)) return;
            {
                const int bend[] = {Spine, Spine1, Spine2};
                int count = 0;
                for (int j : bend) count += j >= 0 && Skeleton.Joints[j].ParentIndex >= 0;
                const XMVECTOR forward = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), count ? FlipSquatLean / static_cast<float>(count) : 0.f);
                for (int j : bend)
                {   // (all about the same axis: no turn of one changes the others')
                    if (j < 0 || Skeleton.Joints[j].ParentIndex < 0) continue;
                    const XMVECTOR up = turnOf(squat, Skeleton.Joints[j].ParentIndex);
                    const XMVECTOR there = XMQuaternionMultiply(XMQuaternionMultiply(up, forward), XMQuaternionConjugate(up));
                    XMStoreFloat4(&squat.LocalJoints[j].Rotation, XMQuaternionNormalize(After(there, XMLoadFloat4(&squat.LocalJoints[j].Rotation))));
                }
            }
            const DoorwaysSkeletonPose& spread = HasWingsLevel && WingsLevel.LocalJoints.size() == n ? WingsLevel : guard;
            // her wings as she clings to it: Fly_Rise where they are highest over her chest (as her chest on guard would
            // have them)
            DoorwaysSkeletonPose raised = spread;
            if (Duration("Fly_Rise") > 0 && WingJoint[0][0] >= 0 && WingJoint[0][2] >= 0 && Skeleton.Joints[WingJoint[0][0]].ParentIndex >= 0)
            {
                const int chest = Skeleton.Joints[WingJoint[0][0]].ParentIndex;
                const XMVECTOR chestOnGuard = turnOf(guard, chest);
                float highest = -2;
                DoorwaysSkeletonPose q;
                for (int f = 0; f < 60; ++f)
                {
                    if (!Sample("Fly_Rise", Duration("Fly_Rise") * static_cast<float>(f) / 60, q, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, q, nullptr)) break;
                    const XMVECTOR way = XMVector3Rotate(XMVector3Rotate(at(q, WingJoint[0][2]) - at(q, WingJoint[0][0]), XMQuaternionConjugate(turnOf(q, chest))), chestOnGuard);
                    const float rise = XMVectorGetY(XMVector3Normalize(way));
                    if (rise > highest) { highest = rise; raised = q; }
                }
            }
            // keys, every 1/60 s
            const auto clipOf = [&](const char* name, float seconds)
            {
                DoorwaysAnimationClip c;
                c.Name = name; c.DurationSeconds = seconds; c.PlaybackSpeed = 1;
                c.PlaybackMode = Clips.find("Fly_Land")->second.PlaybackMode;
                c.JointTracks.resize(n);
                for (size_t j = 0; j < n; ++j) c.JointTracks[j].JointIndex = static_cast<std::uint32_t>(j);
                return c;
            };
            const auto key = [&](DoorwaysAnimationClip& clip, float t, const DoorwaysSkeletonPose& pose)
            {
                for (size_t j = 0; j < n; ++j)
                {
                    auto& tr = clip.JointTracks[j];
                    const auto& l = pose.LocalJoints[j];
                    XMFLOAT4 q = l.Rotation;   // (each key on the same side as the last: no key between them is turned the long way round)
                    if (!tr.RotationKeys.empty())
                    {
                        const XMFLOAT4& was = tr.RotationKeys.back().Value;
                        if (q.x * was.x + q.y * was.y + q.z * was.z + q.w * was.w < 0) q = {-q.x, -q.y, -q.z, -q.w};
                    }
                    tr.TranslationKeys.push_back({t, l.Translation});
                    tr.RotationKeys.push_back({t, q});
                    tr.ScaleKeys.push_back({t, l.Scale});
                }
            };
            DoorwaysAnimationClip drilling = clipOf("Cast_Drill", DiveSeconds), dashing = clipOf("Dash_Twist", 1.f), flipping = clipOf("Cast_Flip", FlipSeconds);
            key(drilling, 0, drill);
            key(drilling, DiveSeconds, drill);
            key(dashing, 0, drill);
            key(dashing, 1, drill);
            // the backflip: each part of her from pose to pose
            const auto mix = [](float t, float a, float c) { const float x = std::clamp((t - a) / (c - a), 0.f, 1.f); return x * x * (3 - 2 * x); };
            const auto blend = [&](DoorwaysSkeletonPose& to, const DoorwaysSkeletonPose& a, const DoorwaysSkeletonPose& c, size_t j, float w)
            {
                auto& o = to.LocalJoints[j];
                XMStoreFloat4(&o.Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&a.LocalJoints[j].Rotation), XMLoadFloat4(&c.LocalJoints[j].Rotation), w)));
                XMStoreFloat3(&o.Translation, XMVectorLerp(XMLoadFloat3(&a.LocalJoints[j].Translation), XMLoadFloat3(&c.LocalJoints[j].Translation), w));
            };
            // (her legs, hips, spine and head: straight in her twist, crouched on it, straight as she has kicked off, tucked,
            // reaching down, crouched as she lands, on guard)
            const float bodyAt[] = {0, FlipStick, FlipPush, FlipOff, FlipTuck, FlipReach, FlipLand, FlipSeconds};
            const DoorwaysSkeletonPose* bodyPose[] = {&drill, &squat, &squat, &drill, &tuck, &reach, &crouch, &guard};
            const int grabKey = static_cast<int>(std::lround(FlipGrab * KeyRate));
            FlipHipsAt.clear(); FlipFeetAt.clear();
            DoorwaysSkeletonPose pose;
            for (int f = 0, frames = static_cast<int>(std::ceil(FlipSeconds * KeyRate)); f <= frames; ++f)
            {
                const float t = (std::min)(FlipSeconds, static_cast<float>(f) / KeyRate);
                int i = 0;
                while (i < 6 && t > bodyAt[i + 1]) ++i;
                const float bodyW = mix(t, bodyAt[i], bodyAt[i + 1]), armsW = mix(t, 0, FlipStick);
                const bool opening = t < FlipStick;   // (her hands open as they go to the handles, and close on them)
                const float handW = opening ? mix(t, 0, FlipStick * .8f) : mix(t, FlipStick, FlipGrab);
                // (her wings: rising over her as she lands on it, beaten down level as she kicks off, folding as she lands)
                const int wingsStage = t < FlipPush ? 0 : t < FlipReach - .04f ? 1 : 2;
                const float wingsW = wingsStage == 0 ? mix(t, 0, FlipStick) : wingsStage == 1 ? mix(t, FlipPush, FlipOff) : mix(t, FlipReach - .04f, FlipLand + .24f);
                const DoorwaysSkeletonPose* wingsFrom[] = {&drill, &raised, &spread};
                const DoorwaysSkeletonPose* wingsTo[] = {&raised, &spread, &guard};
                pose = drill;
                for (size_t j = 0; j < n; ++j)
                {
                    if (legs[j] || spine[j] || static_cast<int>(j) == Hips) blend(pose, *bodyPose[i], *bodyPose[i + 1], j, bodyW);
                    else if (fingers[j]) blend(pose, opening ? drill : cast, opening ? cast : guard, j, handW);
                    else if (ArmJoint[j]) blend(pose, drill, guard, j, armsW);
                    else if (Wing[j]) blend(pose, *wingsFrom[wingsStage], *wingsTo[wingsStage], j, wingsW);
                    else if (sword[j]) pose.LocalJoints[j] = guard.LocalJoints[j];
                }
                key(flipping, t, pose);
                if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) return;
                const auto handle = [&](int k) { return XMVector3TransformCoord(XMLoadFloat3(&SwordGrip[k]), XMLoadFloat4x4(&pose.ModelSpaceMatrices[SwordJoint[k]])); };
                const auto foot = [&](int k) { return ToeJoint[k] >= 0 ? (at(pose, FootJoint[k]) + at(pose, ToeJoint[k])) * .5f : at(pose, FootJoint[k]); };
                XMFLOAT3 h, fe;
                XMStoreFloat3(&h, at(pose, Hips));
                XMStoreFloat3(&fe, (foot(0) + foot(1)) * .5f);
                FlipHipsAt.push_back(h); FlipFeetAt.push_back(fe);
                if (f == grabKey) for (int k = 0; k < 2; ++k) XMStoreFloat3(&GrabGrip[k], handle(k));
            }
            Clips.insert_or_assign(std::string("Cast_Drill"), std::move(drilling));
            Clips.insert_or_assign(std::string("Cast_Flip"), std::move(flipping));
            Clips.insert_or_assign(std::string("Dash_Twist"), std::move(dashing));
        }

        // Her finishing kills' clips (2026-11-19; Finishes), made of her own: each a run of pieces of them, every piece
        // played at its own pace (a clip's from..to over the piece's time), and where one clip goes over into another
        // the two are mixed across the piece, slow away and slow in. Every 1/60 s her feet's middle and the points of
        // her swords are kept (her own space): the kit sets her place by them, to bring a blade to where it cuts.
        //   Talon Cross: Attack_R1's cut from her upper left, crossing over into Attack_L1's from her upper right; the
        //     follow-through as she flashes past, held long; her guard.
        //   Eagle's Plunge: Attack_Jump's leaping plunge: into the crouch, up (the kit lifts her higher for a tall
        //     creature), a moment at the top, both swords down into it, her crouch as she rides it down, held there,
        //     and up again; her guard.
        //   Gale Slide: Skill_Slide's drop and slide (her blades low along the ground at her left), rising into
        //     Attack_R3's wind-up; its spin, whose blade comes round her right to behind her; her guard.
        void MakeFinishes()
        {
            constexpr float KeyRate = 60.f;
            const size_t n = Skeleton.Joints.size();
            if (SwordJoint[0] < 0 || SwordJoint[1] < 0 || FootJoint[0] < 0 || FootJoint[1] < 0) return;
            struct Piece { float Until; const char* A; float A0, A1; const char* B = nullptr; float B0 = 0, B1 = 0; };
            const std::vector<Piece> made[Finishers + 1] = {
                {},
                {{.43f, "Attack_R1", .20f, .65f},
                 {.60f, "Attack_R1", .65f, .80f, "Attack_L1", .78f, .8833f},
                 {.95f, "Attack_L1", .8833f, 1.25f},
                 {1.75f, "Attack_L1", 1.25f, 1.55f},
                 {2.30f, "Attack_L1", 1.55f, 1.80f, "Idle", 0, .55f}},
                {{.25f, "Idle", 0, .2f, "Attack_Jump", .33f, .36f},
                 {.36f, "Attack_Jump", .36f, .42f},
                 {.80f, "Attack_Jump", .42f, .87f},
                 {.98f, "Attack_Jump", .87f, 1.00f},
                 {1.10f, "Attack_Jump", 1.00f, 1.2167f},
                 {1.75f, "Attack_Jump", 1.2167f, 1.50f},
                 {2.05f, "Attack_Jump", 1.50f, 1.70f},
                 {2.55f, "Attack_Jump", 1.70f, 2.20f},
                 {2.75f, "Attack_Jump", 2.20f, 2.333f, "Idle", 0, .2f}},
                {{.78f, "Skill_Slide", 0, .87f},
                 {1.12f, "Skill_Slide", .87f, 1.05f, "Attack_R3", .55f, .85f},
                 {1.50f, "Attack_R3", .85f, 1.30f},
                 {2.10f, "Attack_R3", 1.30f, 1.90f},
                 {2.60f, "Attack_R3", 1.90f, 2.20f, "Idle", 0, .5f}},
                // Heartpierce: Attack_L2's thrust, slowly in, held, drawn back out
                {{.30f, "Idle", 0, .2f, "Attack_L2", .80f, .85f},
                 {.55f, "Attack_L2", .85f, .95f},
                 {.70f, "Attack_L2", .95f, 1.09f},
                 {1.30f, "Attack_L2", 1.09f, 1.105f},
                 {1.65f, "Attack_L2", 1.105f, .95f},
                 {2.20f, "Attack_L2", .95f, .90f, "Idle", 0, .4f}},
                // Reaper's Wheel: Attack_R4's turn; into Attack_R1's cut from high
                {{.25f, "Idle", 0, .15f, "Attack_R4", .30f, .42f},
                 {.60f, "Attack_R4", .42f, .88f},
                 {.85f, "Attack_R4", .88f, 1.20f},
                 {1.10f, "Attack_R4", 1.20f, 1.32f, "Attack_R1", .46f, .55f},
                 {1.30f, "Attack_R1", .55f, .672f},
                 {1.75f, "Attack_R1", .672f, .85f},
                 {2.30f, "Attack_R1", .85f, .98f, "Idle", 0, .4f}},
                // Mountain Breaker: Attack_R1's cut, into Attack_Jump's blades down into the ground
                {{.30f, "Idle", 0, .15f, "Attack_R1", .46f, .55f},
                 {.50f, "Attack_R1", .55f, .672f},
                 {.70f, "Attack_R1", .672f, .72f, "Attack_Jump", 1.f, 1.08f},
                 {1.05f, "Attack_Jump", 1.08f, 1.255f},
                 {1.60f, "Attack_Jump", 1.255f, 1.45f},
                 {2.30f, "Attack_Jump", 1.45f, 1.66f, "Idle", 0, .4f}},
                // Moonlit Flash: Attack_R_Run's cut as she runs, its end held
                {{.35f, "Idle", 0, .2f, "Attack_R_Run", .40f, .47f},
                 {.55f, "Attack_R_Run", .47f, .65f},
                 {.80f, "Attack_R_Run", .65f, .75f},
                 {1.60f, "Attack_R_Run", .75f, .85f},
                 {2.40f, "Attack_R_Run", .85f, .85f, "Idle", 0, .5f}},
                // Tempest Flurry: Attack_L3's two cuts and Attack_L_Run's two
                {{.25f, "Idle", 0, .15f, "Attack_L3", .70f, .75f},
                 {.42f, "Attack_L3", .75f, .83f},
                 {.62f, "Attack_L3", .83f, .93f},
                 {.70f, "Attack_L3", .93f, 1.40f},
                 {.84f, "Attack_L3", 1.40f, 1.51f},
                 {.98f, "Attack_L3", 1.51f, 1.55f, "Attack_L_Run", .62f, .67f},
                 {1.12f, "Attack_L_Run", .67f, .76f},
                 {1.26f, "Attack_L_Run", .76f, .85f},
                 {1.36f, "Attack_L_Run", .85f, 1.40f},
                 {1.52f, "Attack_L_Run", 1.40f, 1.54f},
                 {1.90f, "Attack_L_Run", 1.54f, 1.62f},
                 {2.40f, "Attack_L_Run", 1.62f, 1.655f, "Idle", 0, .4f}},
                // Headsman: Attack_L_Run's low cut; Attack_R1's blade raised, slowly, and Attack_R_Step's down
                {{.25f, "Idle", 0, .15f, "Attack_L_Run", .60f, .66f},
                 {.42f, "Attack_L_Run", .66f, .76f},
                 {.70f, "Attack_L_Run", .76f, .86f, "Attack_R1", .46f, .50f},
                 {1.05f, "Attack_R1", .50f, .64f},
                 {1.30f, "Attack_R1", .64f, .72f, "Attack_R_Step", .80f, .87f},
                 {1.42f, "Attack_R_Step", .87f, .895f},
                 {1.90f, "Attack_R_Step", .895f, 1.05f},
                 {2.50f, "Attack_R_Step", 1.05f, 1.25f, "Idle", 0, .4f}},
                // Rising Talon: Attack_R2's rising cut, held as she rises; Attack_Jump's landing
                {{.30f, "Idle", 0, .2f, "Attack_R2", .65f, .72f},
                 {.50f, "Attack_R2", .72f, .865f},
                 {.95f, "Attack_R2", .865f, .96f},
                 {1.40f, "Attack_R2", .96f, .96f, "Attack_Jump", 1.40f, 1.45f},
                 {1.75f, "Attack_Jump", 1.45f, 1.66f},
                 {2.30f, "Attack_Jump", 1.66f, 1.90f, "Idle", 0, .4f}},
            };
            const auto at = [](const DoorwaysSkeletonPose& q, int j) { const auto& m = q.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            const auto make = [&](int which)
            {   // (one that cannot be made is left out; the others are made)
                const auto& pieces = made[which];
                const float seconds = pieces.back().Until;
                if (std::abs(seconds - Finishes[which].Seconds) > 1e-3f) return;   // (the table and the clip must agree)
                DoorwaysAnimationClip clip;
                clip.Name = Finishes[which].Clip; clip.DurationSeconds = seconds; clip.PlaybackSpeed = 1;
                clip.PlaybackMode = Clips.find("Idle")->second.PlaybackMode;
                clip.JointTracks.resize(n);
                for (size_t j = 0; j < n; ++j) clip.JointTracks[j].JointIndex = static_cast<std::uint32_t>(j);
                auto& feetAt = FinishFeetAt[which];
                auto& tipAt = FinishTipAt[which];
                feetAt.clear(); tipAt[0].clear(); tipAt[1].clear();
                DoorwaysSkeletonPose pose, other;
                for (int f = 0, frames = static_cast<int>(std::ceil(seconds * KeyRate)); f <= frames; ++f)
                {
                    const float t = (std::min)(seconds, static_cast<float>(f) / KeyRate);
                    size_t i = 0;
                    while (i + 1 < pieces.size() && t > pieces[i].Until) ++i;
                    const Piece& piece = pieces[i];
                    const float from = i ? pieces[i - 1].Until : 0.f;
                    const float u = std::clamp((t - from) / (std::max)(piece.Until - from, 1e-3f), 0.f, 1.f);
                    if (!Sample(piece.A, piece.A0 + (piece.A1 - piece.A0) * u, pose, nullptr)) return;
                    if (piece.B)
                    {
                        if (!Sample(piece.B, piece.B0 + (piece.B1 - piece.B0) * u, other, nullptr) || other.LocalJoints.size() != n) return;
                        const float w = u * u * (3 - 2 * u);
                        for (size_t j = 0; j < n; ++j)
                        {
                            auto& o = pose.LocalJoints[j];
                            const auto& c = other.LocalJoints[j];
                            XMStoreFloat4(&o.Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&o.Rotation), XMLoadFloat4(&c.Rotation), w)));
                            XMStoreFloat3(&o.Translation, XMVectorLerp(XMLoadFloat3(&o.Translation), XMLoadFloat3(&c.Translation), w));
                            XMStoreFloat3(&o.Scale, XMVectorLerp(XMLoadFloat3(&o.Scale), XMLoadFloat3(&c.Scale), w));
                        }
                    }
                    for (size_t j = 0; j < n; ++j)
                    {   // (each key on the same side as the last: no key between them is turned the long way round)
                        auto& tr = clip.JointTracks[j];
                        const auto& l = pose.LocalJoints[j];
                        XMFLOAT4 q = l.Rotation;
                        if (!tr.RotationKeys.empty())
                        {
                            const XMFLOAT4& was = tr.RotationKeys.back().Value;
                            if (q.x * was.x + q.y * was.y + q.z * was.z + q.w * was.w < 0) q = {-q.x, -q.y, -q.z, -q.w};
                        }
                        tr.TranslationKeys.push_back({t, l.Translation});
                        tr.RotationKeys.push_back({t, q});
                        tr.ScaleKeys.push_back({t, l.Scale});
                    }
                    if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, pose, nullptr)) return;
                    const auto foot = [&](int k) { return ToeJoint[k] >= 0 ? (at(pose, FootJoint[k]) + at(pose, ToeJoint[k])) * .5f : at(pose, FootJoint[k]); };
                    XMFLOAT3 v;
                    XMStoreFloat3(&v, (foot(0) + foot(1)) * .5f); feetAt.push_back(v);
                    for (int k = 0; k < 2; ++k)
                    {
                        XMStoreFloat3(&v, XMVector3TransformCoord(XMLoadFloat3(&SwordTip[k]), XMLoadFloat4x4(&pose.ModelSpaceMatrices[SwordJoint[k]])));
                        tipAt[k].push_back(v);
                    }
                }
                Clips.insert_or_assign(std::string(Finishes[which].Clip), std::move(clip));
            };
            for (int which = 1; which <= Finishers; ++which) make(which);
        }
        // A style's move begun: out of whatever she was doing (Compose blends into it over BlendIn), at the creature it is
        // aimed at (target; away: how far its edge is, m), or with none half its lunge straight on. Out of standing at ease her
        // swords are drawn first, quickly (Compose: StyleDrawReach, StyleDrawOn): the move waits for her hands at the handles.
        void StartMove(const WorldState& world, const StyleMove& m, int button, int target, float away)
        {
            Restarted = Attack == m.Clip;
            ComboEndedAt = -9;
            Attack = m.Clip; ++Begun;
            MoveOf = &m; MoveButton = button; MoveTarget = target; MoveHitNext = 0; MoveSpun = 0;
            MoveAt = {world.MainPlayer.Position.x, world.MainPlayer.Position.z}; MoveVel = MoveOwe = {0, 0};
            const float back = m.BackTo <= m.LungeFrom ? m.Back : 0.f;   // (a step back before the lunge: so much farther to go in)
            MovePlan = away >= 0 ? std::clamp(away + back - m.Gap, 0.f, m.Lunge) : m.Lunge * .5f;
            CutBlend = m.BlendIn;
            AttackTime = InHand ? 0.f : -StyleDrawReach;
            Queued = 0; QueuedAt = -1; Jumping = false;
            Log(world, ("move " + std::string(m.Clip) + (target >= 0 ? " at " + std::to_string(target) : std::string(" at none"))).c_str());
        }
        void EndMove(const WorldState& world, const char* why)
        {
            const float off = std::hypot(MoveAt.x - world.MainPlayer.Position.x, MoveAt.y - world.MainPlayer.Position.z);
            ComboEndedAt = Clock + (MoveOf ? (std::max)(0.f, MoveOf->Seconds - AttackTime) : 0.f);   // (given up early: as if it had played out)
            Attack.clear(); MoveOf = nullptr; MoveVel = MoveOwe = {0, 0}; MoveTarget = -1;
            Log(world, (std::string(why) + " (" + std::to_string(off) + " m off its way)").c_str());
        }
        // A clip made of pieces of hers (a style's move): each track one of her clips played through its knots (StyleWarp),
        // each next one blended in where the two overlap (slow away, slow in), keyed 60 times a second
        bool MakeSpliced(const char* name, float seconds, const std::vector<StyleTrack>& tracks)
        {
            constexpr float KeyRate = 60.f;
            const size_t n = Skeleton.Joints.size();
            if (tracks.empty() || seconds <= 0 || Clips.find("Idle") == Clips.end()) return false;
            for (const auto& tr : tracks)
                if (tr.Knots.size() < 2 || Clips.find(tr.Clip) == Clips.end()) return false;
            DoorwaysAnimationClip clip;
            clip.Name = name; clip.DurationSeconds = seconds; clip.PlaybackSpeed = 1;
            clip.PlaybackMode = Clips.find("Idle")->second.PlaybackMode;
            clip.JointTracks.resize(n);
            for (size_t j = 0; j < n; ++j) clip.JointTracks[j].JointIndex = static_cast<std::uint32_t>(j);
            DoorwaysSkeletonPose pose, other;
            for (int f = 0, frames = static_cast<int>(std::ceil(seconds * KeyRate)); f <= frames; ++f)
            {
                const float t = (std::min)(seconds, static_cast<float>(f) / KeyRate);
                bool have = false;
                float before = 0;   // (where the track before ends)
                for (const auto& tr : tracks)
                {
                    const float a = tr.Knots.front().Out, e = tr.Knots.back().Out;
                    if (t >= a - 1e-4f && t <= e + 1e-4f)
                    {
                        if (!Sample(tr.Clip, StyleWarp(tr.Knots, t), have ? other : pose, nullptr)) return false;
                        if (have)
                        {
                            if (other.LocalJoints.size() != n) return false;
                            const float u = std::clamp((t - a) / (std::max)(before - a, 1e-3f), 0.f, 1.f), w = u * u * (3 - 2 * u);
                            for (size_t j = 0; j < n; ++j)
                            {
                                auto& o = pose.LocalJoints[j];
                                const auto& c = other.LocalJoints[j];
                                XMStoreFloat4(&o.Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&o.Rotation), XMLoadFloat4(&c.Rotation), w)));
                                XMStoreFloat3(&o.Translation, XMVectorLerp(XMLoadFloat3(&o.Translation), XMLoadFloat3(&c.Translation), w));
                                XMStoreFloat3(&o.Scale, XMVectorLerp(XMLoadFloat3(&o.Scale), XMLoadFloat3(&c.Scale), w));
                            }
                        }
                        have = true;
                    }
                    before = e;
                }
                if (!have)
                {   // (between two tracks: the nearer one's end)
                    const StyleTrack* best = &tracks.front(); float gap = 1e9f;
                    for (const auto& tr : tracks)
                        for (const float edge : {tr.Knots.front().Out, tr.Knots.back().Out})
                            if (std::abs(t - edge) < gap) { gap = std::abs(t - edge); best = &tr; }
                    if (!Sample(best->Clip, StyleWarp(best->Knots, t), pose, nullptr)) return false;
                }
                if (pose.LocalJoints.size() != n) return false;
                for (size_t j = 0; j < n; ++j)
                {   // (each key on the same side as the last: no key between them is turned the long way round)
                    auto& tr = clip.JointTracks[j];
                    const auto& l = pose.LocalJoints[j];
                    XMFLOAT4 q = l.Rotation;
                    if (!tr.RotationKeys.empty())
                    {
                        const XMFLOAT4& was = tr.RotationKeys.back().Value;
                        if (q.x * was.x + q.y * was.y + q.z * was.z + q.w * was.w < 0) q = {-q.x, -q.y, -q.z, -q.w};
                    }
                    tr.TranslationKeys.push_back({t, l.Translation});
                    tr.RotationKeys.push_back({t, q});
                    tr.ScaleKeys.push_back({t, l.Scale});
                }
            }
            Clips.insert_or_assign(std::string(name), std::move(clip));
            return true;
        }
        // The styles' moves, made at load from her clips as they are then (after all their fixes). One that cannot be made is
        // left out (NyxaraKit::StyleMade says so): the game, which has no style, is never stopped by it.
        void MakeStyles()
        {
            for (const auto& st : Styles())
                for (const auto& m : st.Move)
                    if (!MakeSpliced(m.Clip, m.Seconds, m.Tracks))
                        OutputDebugStringA((std::string("[WARN] Nyxara: the move ") + m.Clip + " could not be made\n").c_str());
            if (const auto& m = FlashStepTrial(); !MakeSpliced(m.Clip, m.Seconds, m.Tracks))
                OutputDebugStringA("[WARN] Nyxara: the trial FLASH STEP could not be made\n");
            // her guards: each a loop of Breath s, out from its moment and back (slow away, slow in at each end), so that it
            // goes round without a seam
            for (int g = 0; g < static_cast<int>(Guards().size()); ++g)
            {
                const GuardSpec& gs = Guards()[static_cast<size_t>(g)];
                if (!gs.Clip) continue;   // (her guard stance)
                const float b = gs.Breath;
                const std::vector<StyleTrack> held{{gs.Clip, {{0, gs.At - gs.Sway}, {b * .5f, gs.At + gs.Sway}, {b, gs.At - gs.Sway}}}};
                if (!MakeSpliced(GuardClip(g).c_str(), b, held))
                    OutputDebugStringA(("[WARN] Nyxara: the guard " + GuardClip(g) + " could not be made\n").c_str());
                else GuardTurn[g] = SquareGuard(GuardClip(g), gs.Clip, gs.At);
            }
        }
        // Her soul's clips (Q), made at load of her own (each track: s into the clip made, s into hers; a next one blended
        // in where they overlap): its leaving, Skill_Soul (THREAD STEP, as the owner chose it: one long stride in a blink,
        // down on her feet 6 m on, on guard: SoulFar, SoulOutFrom, SoulOutTo), and its way back, Soul_Back (pulled
        // through the air, played over however long her way back takes). Each of her clips is taken by shares of its own
        // length.
        void MakeSoul()
        {
            const auto of = [&](const char* clip, float share) { return share * (std::max)(Duration(clip), .01f); };
            const auto make = [&](const char* name, float seconds, const std::vector<StyleTrack>& tracks)
            {
                if (!MakeSpliced(name, seconds, tracks)) OutputDebugStringA((std::string("[WARN] Nyxara: her soul's ") + name + " could not be made\n").c_str());
            };
            make(SoulClip, .62f, {{"Run", {{0, of("Run", .05f)}, {.24f, of("Run", .42f)}}},
                                  {"Fly_Land", {{.18f, of("Fly_Land", .55f)}, {.44f, of("Fly_Land", 1.f)}}},
                                  {"Idle", {{.38f, 0}, {.62f, .24f}}}});
            make(SoulBackClip, 1.f, {{"Jump", {{0, of("Jump", .42f)}, {.7f, of("Jump", .6f)}}},
                                     {"Fly_Land", {{.6f, of("Fly_Land", .6f)}, {1.f, of("Fly_Land", 1.f)}}}});
        }
        // Where her soul's strings are tied on a body of hers (NyxaraSoul::Tie), in the world: a pose whose model matrices
        // are made, its picture placed so; feet: the ground under it
        NyxaraSoul::Body SoulTies(const DoorwaysSkeletonPose& pose, const XMMATRIX& place, const XMFLOAT3& feet) const
        {
            NyxaraSoul::Body out;
            const auto at = [&](int j)
            {
                if (j < 0 || j >= static_cast<int>(pose.ModelSpaceMatrices.size())) return XMVectorSet(0, 1, 0, 1);
                const auto& m = pose.ModelSpaceMatrices[static_cast<size_t>(j)];
                return XMVectorSet(m._41, m._42, m._43, 1);
            };
            const auto put = [&](NyxaraSoul::Tie tie, XMVECTOR v) { XMStoreFloat3(&out.At[tie], XMVector3TransformCoord(v, place)); };
            put(NyxaraSoul::Chest, at(Spine2 >= 0 ? Spine2 : Spine));
            put(NyxaraSoul::Head, at(HeadJoint));
            put(NyxaraSoul::Hips, at(Hips));
            put(NyxaraSoul::HandL, at(ArmChain[0][3]));
            put(NyxaraSoul::HandR, at(ArmChain[1][3]));
            for (int side = 0; side < 2; ++side)
            {   // (her left wing first: WingRightIs says which of the two is on her right; its feathers reach past its last bone)
                const int k = (WingRightIs > 0) == (side == 1) ? 1 : 0;
                const XMVECTOR c = at(WingJoint[k][1]), e = at(WingJoint[k][2]);
                put(side ? NyxaraSoul::WingR : NyxaraSoul::WingL, at(WingJoint[k][0]));
                put(side ? NyxaraSoul::TipR : NyxaraSoul::TipL, XMVectorSetW(e + (e - c) * 1.1f, 1));
            }
            put(NyxaraSoul::FootL, at(FootJoint[0]));
            put(NyxaraSoul::FootR, at(FootJoint[1]));
            out.Feet = feet;
            return out;
        }
        // A guard turned about her up so that her shoulders face as they do on guard (Idle): some of its moments are taken
        // from a spin, and held as they were she stood with her back or her side to what she guards against. Its hips are
        // turned (and where they are, about her feet's middle); all the rest of her goes with them. The turn (radians).
        float GuardTurn[16]{};
        float SquareGuard(const std::string& name, const char* source, float at)
        {
            const auto it = Clips.find(name);
            if (it == Clips.end() || Hips < 0 || ArmChain[0][1] < 0 || ArmChain[1][1] < 0) return 0;
            DoorwaysSkeletonPose idle, held;
            if (!Sample("Idle", 0, idle, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, idle, nullptr) ||
                !Sample(source, at, held, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, held, nullptr)) return 0;
            const auto yawOf = [&](const DoorwaysSkeletonPose& q)
            {   // which way the line from one shoulder to the other lies, about her up
                const auto& a = q.ModelSpaceMatrices[ArmChain[0][1]];
                const auto& c = q.ModelSpaceMatrices[ArmChain[1][1]];
                return std::atan2(c._41 - a._41, c._43 - a._43);
            };
            const float turn = std::remainder(yawOf(idle) - yawOf(held), XM_2PI);
            const int parent = Skeleton.Joints[Hips].ParentIndex;
            XMMATRIX up = parent >= 0 ? XMLoadFloat4x4(&held.ModelSpaceMatrices[parent]) : XMMatrixIdentity();
            XMVECTOR scale, qUp, place;
            if (!XMMatrixDecompose(&scale, &qUp, &place, up)) return 0;
            const XMMATRIX down = XMMatrixInverse(nullptr, up);
            const XMVECTOR r = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), turn);
            auto& track = it->second.JointTracks[static_cast<size_t>(Hips)];
            for (auto& k : track.RotationKeys)
            {
                const XMVECTOR q = XMQuaternionMultiply(XMQuaternionMultiply(XMQuaternionMultiply(XMLoadFloat4(&k.Value), qUp), r), XMQuaternionInverse(qUp));
                XMStoreFloat4(&k.Value, XMQuaternionNormalize(q));
            }
            for (auto& k : track.TranslationKeys)
            {
                const XMVECTOR m = XMVector3TransformCoord(XMLoadFloat3(&k.Value), up);
                XMStoreFloat3(&k.Value, XMVector3TransformCoord(XMVector3Rotate(m, r), down));
            }
            return turn;
        }
        // Eagle's Plunge, riding it down: her place (the world's x and z) and how high her picture is lifted, t s into it,
        // as it goes over backwards about its feet from her blow (FinishBlowAt[0]; the kit's Fallen, as fast) and her
        // left blade's point (as Finish_Plunge has it) stays where it went into it (FinishAnchor, turned over with it)
        void PlungeRide(float t, XMFLOAT3& place, float& lift, XMFLOAT3* anchor = nullptr) const
        {
            const float contact = FinishBlowAt[0], fall = Finishes[2].Falls;
            const float over = std::clamp((t - contact) / fall, 0.f, 1.f), tilt = FallenTilt * over * over;
            XMVECTOR axis = XMVector3Cross(XMVectorSet(0, 1, 0, 0), XMVectorSet(FinishWay.x, 0, FinishWay.z, 0));
            axis = XMVectorGetX(XMVector3LengthSq(axis)) < 1e-8f ? XMVectorSet(1, 0, 0, 0) : XMVector3Normalize(axis);
            const XMVECTOR feet = Load3(FinishFeet);
            XMFLOAT3 a;
            XMStoreFloat3(&a, feet + XMVector3TransformNormal(Load3(FinishAnchor) - feet, XMMatrixRotationAxis(axis, tilt)));
            a.y += .3f * FinishDepth * (1 - std::cos(tilt));     // (lying, it is raised by its depth: on the ground, not in it)
            const XMFLOAT3 tip = Mul(FlipAt(FinishTipAt[2][0], (std::min)(t, contact + fall)), FinishTall);
            place = {a.x - FinishRight.x * tip.x - FinishWay.x * tip.z, a.y, a.z - FinishRight.z * tip.x - FinishWay.z * tip.z};
            lift = a.y - FinishFeet.y - tip.y;
            if (anchor) *anchor = a;
        }
        static XMVECTOR Load3(const XMFLOAT3& v) { return XMLoadFloat3(&v); }
        // v turned as her picture's Rotation.x turns it (forward about her right)
        static XMVECTOR Pitched(XMVECTOR v, float pitch) { return XMVector3TransformNormal(v, XMMatrixRotationX(pitch)); }
        // a point of the world in her place's axes (x her right, y up, z the way she faces), and back (or a way, not a point)
        static XMVECTOR Here(const WorldState& world, const XMFLOAT3& w)
        {
            const auto& p = world.MainPlayer;
            const float c = std::cos(p.Yaw), sn = std::sin(p.Yaw), dx = w.x - p.Position.x, dz = w.z - p.Position.z;
            return XMVectorSet(dx * c - dz * sn, w.y - p.Position.y, dx * sn + dz * c, 0);
        }
        static XMFLOAT3 There(const WorldState& world, XMVECTOR v, bool point = true)
        {
            const auto& p = world.MainPlayer;
            const float c = std::cos(p.Yaw), sn = std::sin(p.Yaw), x = XMVectorGetX(v), y = XMVectorGetY(v), z = XMVectorGetZ(v);
            const XMFLOAT3 way{x * c + z * sn, y, -x * sn + z * c};
            return point ? XMFLOAT3{p.Position.x + way.x, p.Position.y + way.y, p.Position.z + way.z} : way;
        }
        // V's way: how far along it, c of its time gone (she leaves and arrives at a third of her pace)
        static float Along(float c)
        {
            c = std::clamp(c, 0.f, 1.f);
            return .65f * c * c * (3 - 2 * c) + .35f * c;
        }
        // a Cast_Flip table (MakeCast) at t s
        static XMFLOAT3 FlipAt(const std::vector<XMFLOAT3>& v, float t)
        {
            if (v.empty()) return {};
            const size_t last = v.size() - 1;
            const float f = std::clamp(t * 60.f, 0.f, static_cast<float>(last));
            const size_t i = last ? (std::min)(static_cast<size_t>(f), last - 1) : 0;
            const float w = last ? f - static_cast<float>(i) : 0.f;
            const XMFLOAT3 &a = v[i], &c = v[(std::min)(i + 1, last)];
            return {a.x + (c.x - a.x) * w, a.y + (c.y - a.y) * w, a.z + (c.z - a.z) * w};
        }
        // V's backflip: how her body lies (BodyPitch), t s into Cast_Flip: from as her twist left it (lying back) to square
        // to the creature, lying level with her feet on its side, as she crouches on it; then back over, three quarters of
        // a turn, upright as her legs reach down for the ground
        float FlipPitch(float t) const
        {
            const auto ease = [](float u) { u = std::clamp(u, 0.f, 1.f); return u * u * (3 - 2 * u); };
            if (t < FlipPush) return -DrillLean + (DrillLean - XM_PIDIV2) * ease(t / FlipStick);
            if (t >= FlipReach) return 0.f;
            return -XM_PIDIV2 - (XM_2PI - XM_PIDIV2) * ease((t - FlipPush) / (FlipReach - FlipPush));
        }
        // V, as her feet stick on the creature: where her hips are as she kicks off it and how fast they go (her place's
        // axes), to fly as a thrown thing does (FlipFall) to over where she lands, in the time there is
        void StartFlip(const WorldState& world)
        {
            const float tall = (std::max)(.01f, world.AnimatedPlayerVisual.Scale.y), pitch = FlipPitch(FlipPush), span = FlipLand - FlipPush;
            const XMVECTOR from = Here(world, CastFeetAt) + tall * (Pitched(Load3(FlipAt(FlipHipsAt, FlipPush)), pitch) - Pitched(Load3(FlipAt(FlipFeetAt, FlipPush)), pitch));
            const XMVECTOR to = tall * Load3(FlipAt(FlipHipsAt, FlipLand));
            XMStoreFloat3(&CastOffAt, from);
            XMStoreFloat3(&CastOffGo, (to - from) / span + XMVectorSet(0, .5f * FlipFall * span, 0, 0));
        }
        // V: her hands on the handles where her swords stick (2026-11-19; the owner: "sticks with two feet on [it],
        // grab[s the] swords"). First her back bends forward at its foot as far as her hands need to reach them (at most
        // ReachBendMost; tried every hundredth of a radian), weight of it. Then each arm is turned at the shoulder and the
        // elbow, the elbow bending on the side it bends to now, so that the handle as her fist holds it comes weight of the
        // way to that sword's handle; three times over, as the handle in her fist moves with her forearm. HandleGap keeps
        // how far they were apart at the most while she held them.
        void ReachHandles(const WorldState& world, DoorwaysSkeletonPose& out, float weight)
        {
            if (weight <= 0 || SwordJoint[0] < 0 || SwordJoint[1] < 0) return;
            const auto& p = world.MainPlayer;
            const XMMATRIX toModel = XMMatrixInverse(nullptr, BuildDoorwaysCharacterWorldMatrix(p.Position, p.Yaw, world.AnimatedPlayerVisual));
            const auto place = [&](int j) { const auto& m = out.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            const auto turnOf = [&](int j)
            {
                XMVECTOR sc, q, t;
                XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[j]));
                return XMQuaternionNormalize(q);
            };
            const auto grip = [&](int k) { return XMVector3TransformCoord(XMLoadFloat3(&SwordGrip[k]), XMLoadFloat4x4(&out.ModelSpaceMatrices[SwordJoint[k]])); };
            const auto length = [](XMVECTOR v) { return XMVectorGetX(XMVector3Length(v)); };
            if (Spine >= 0 && Skeleton.Joints[Spine].ParentIndex >= 0 && EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr))
            {   // her back: the least bend (about her right, at its foot) that brings each handle within her arm and most of
                // her fist of her shoulder
                const XMVECTOR pivot = place(Spine);
                float need = 0;
                for (int step = 0; step <= static_cast<int>(ReachBendMost * 100); ++step)
                {
                    need = static_cast<float>(step) / 100;
                    const XMMATRIX bend = XMMatrixTranslationFromVector(-pivot) * XMMatrixRotationX(need) * XMMatrixTranslationFromVector(pivot);
                    bool within = true;
                    for (int k = 0; k < 2; ++k)
                    {
                        const int ua = ArmChain[k][1], fa = ArmChain[k][2], ha = ArmChain[k][3];
                        if (ua < 0 || fa < 0 || ha < 0) continue;
                        const float arm = length(place(fa) - place(ua)) + length(place(ha) - place(fa)), fist = length(grip(k) - place(ha));
                        const XMVECTOR handle = XMVector3TransformCoord(XMLoadFloat3(&SwordTo[k]), toModel);
                        if (length(handle - XMVector3TransformCoord(place(ua), bend)) > arm + .8f * fist) within = false;
                    }
                    if (within) break;
                }
                if (need > 0)
                {
                    const XMVECTOR up = turnOf(Skeleton.Joints[Spine].ParentIndex);
                    const XMVECTOR there = XMQuaternionMultiply(XMQuaternionMultiply(up, XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), need * weight)), XMQuaternionConjugate(up));
                    XMStoreFloat4(&out.LocalJoints[Spine].Rotation, XMQuaternionNormalize(After(there, XMLoadFloat4(&out.LocalJoints[Spine].Rotation))));
                }
            }
            for (int pass = 0; pass <= 3; ++pass)
            {
                if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr)) return;
                for (int k = 0; k < 2; ++k)
                {
                    const int ua = ArmChain[k][1], fa = ArmChain[k][2], ha = ArmChain[k][3];
                    if (ua < 0 || fa < 0 || ha < 0 || Skeleton.Joints[ua].ParentIndex < 0) continue;
                    const XMVECTOR handle = XMVector3TransformCoord(XMLoadFloat3(&SwordTo[k]), toModel);
                    if (pass == 3)
                    {   // (how near it came)
                        if (weight > .99f) HandleGap = (std::max)(HandleGap, length(handle - grip(k)));
                        continue;
                    }
                    const XMVECTOR shoulder = place(ua), elbow = place(fa), wrist = place(ha);
                    const float upper = length(elbow - shoulder), fore = length(wrist - elbow);
                    const XMVECTOR want = wrist + (handle - grip(k)) * weight - shoulder;   // where her wrist goes, from her shoulder
                    const float away = length(want);
                    if (upper < .01f || fore < .01f || away < 1e-4f) continue;
                    const XMVECTOR dir = want / away;
                    const float reach = std::clamp(away, std::abs(upper - fore) + .001f, upper + fore - .001f);
                    XMVECTOR side = (elbow - shoulder) - dir * XMVectorGetX(XMVector3Dot(elbow - shoulder, dir));
                    if (XMVectorGetX(XMVector3LengthSq(side)) < 1e-8f) side = XMVector3Cross(dir, XMVectorSet(1, 0, 0, 0));
                    if (XMVectorGetX(XMVector3LengthSq(side)) < 1e-8f) side = XMVector3Cross(dir, XMVectorSet(0, 0, 1, 0));
                    side = XMVector3Normalize(side);
                    const float cosA = std::clamp((upper * upper + reach * reach - fore * fore) / (2 * upper * reach), -1.f, 1.f);
                    const XMVECTOR elbowTo = shoulder + (dir * cosA + side * std::sqrt(1 - cosA * cosA)) * upper, wristTo = shoulder + dir * reach;
                    const XMVECTOR upperTurn = ArcTurn(XMVector3Normalize(elbow - shoulder), XMVector3Normalize(elbowTo - shoulder));
                    const XMVECTOR upperNow = XMQuaternionNormalize(XMQuaternionMultiply(turnOf(ua), upperTurn));
                    const XMVECTOR foreTurn = ArcTurn(XMVector3Normalize(XMVector3Rotate(wrist - elbow, upperTurn)), XMVector3Normalize(wristTo - elbowTo));
                    const XMVECTOR foreNow = XMQuaternionNormalize(XMQuaternionMultiply(XMQuaternionMultiply(turnOf(fa), upperTurn), foreTurn));
                    const XMVECTOR above = turnOf(Skeleton.Joints[ua].ParentIndex);
                    XMStoreFloat4(&out.LocalJoints[ua].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(upperNow, XMQuaternionConjugate(above))));
                    XMStoreFloat4(&out.LocalJoints[fa].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(foreNow, XMQuaternionConjugate(upperNow))));
                }
            }
        }
        // V's way and backflip, the twisted dash (2026-11-19): her whole picture turned, BodyPitch forward about her right
        // and BodyTwist about her own length, and put (BodyAt) where what must be somewhere is: her middle on its way to
        // the creature, her feet on it, her middle on its way back and her feet where she lands. lifted: her picture's
        // lift as it would be (in flight).
        void TurnBody(const WorldState& world, float dt, float lifted)
        {
            const float tall = (std::max)(.01f, world.AnimatedPlayerVisual.Scale.y);
            const XMVECTOR dive = Load3(DiveHips);
            const float was = TwistNow;
            XMVECTOR at = XMVectorZero(), middle = XMVectorZero();   // BodyAt; where her hips are shown
            float pitch = 0, roll = 0;
            if (Air == 1)
            {   // the twisted dash: laid back into V's twist (from how she leans in flight, and back to it), her hips where they were
                const float u = std::clamp(AirTime / DashSeconds, 0.f, 1.f);
                TwistNow = BodyTwist = XM_2PI * DashTurns * u * u * u * (10 - 15 * u + 6 * u * u);
                const auto ease = [](float v) { v = std::clamp(v, 0.f, 1.f); return v * v * (3 - 2 * v); };
                const float flat = (std::min)(ease(AirTime / DashTip), ease((DashSeconds - AirTime) / DashTipBack));
                pitch = FlyLean + (-DrillLean - FlyLean) * flat;
                roll = (LeanRoll + Bank) * (1 - flat);
                constexpr float Chest = 1.15f;   // (her bank about her chest, as in flight)
                at = XMVectorSet(Chest * std::sin(roll), lifted + Chest * (1 - std::cos(roll)), 0, 0) + tall * (Pitched(dive, FlyLean) - Pitched(dive, pitch));
                middle = tall * Pitched(dive, pitch) + at;
            }
            else if (Cast == 4)
            {   // V's way: her twist as she used to twist, laid back (feet first) over DiveLie s; her middle from where it was
                // to where her twist ends, her feet on the creature
                const float come = std::clamp(CastTime / CastDiveTime, 0.f, 1.f), go = Along(come);
                const float lie = std::clamp(CastTime / (std::min)(DiveLie, .3f * CastDiveTime), 0.f, 1.f);
                TwistNow = BodyTwist = XM_2PI * CastTurns * CastTwist;
                pitch = -DrillLean * lie * lie * (3 - 2 * lie);
                XMFLOAT3 w;
                XMStoreFloat3(&w, XMVectorLerp(Load3(CastFrom), Load3(CastDrillTo), go) + XMVectorSet(0, DiveArc * std::sin(XM_PI * go), 0, 0));
                middle = Here(world, w);
                at = middle - tall * Pitched(dive, pitch);
            }
            else if (Cast == 5)
            {   // her feet stuck on it as her legs give and push; then her hips through the air from where she kicked off to
                // over where she lands, as a thrown thing flies (StartFlip); her picture her place again as she lands
                const float t = CastTime;
                pitch = FlipPitch(t);
                const XMVECTOR hips = Load3(FlipAt(FlipHipsAt, t));
                const XMVECTOR onIt = Here(world, CastFeetAt) - tall * Pitched(Load3(FlipAt(FlipFeetAt, t)), pitch);
                at = onIt;
                if (t > FlipPush)
                {
                    const float flown = (std::min)(t, FlipLand) - FlipPush;
                    const XMVECTOR way = Load3(CastOffAt) + Load3(CastOffGo) * flown - XMVectorSet(0, .5f * FlipFall * flown * flown, 0, 0);
                    const float off = std::clamp((t - FlipPush) / (FlipOff - FlipPush), 0.f, 1.f);
                    at = XMVectorLerp(onIt, way - tall * Pitched(hips, pitch), off * off * (3 - 2 * off));
                }
                if (t >= FlipLand) { at = XMVectorZero(); pitch = 0; }   // on her feet: her picture is her place again
                middle = tall * Pitched(hips, pitch) + at;
            }
            BodyPitch = pitch; BodyRoll = roll; XMStoreFloat3(&BodyAt, at); BodyMove = true;
            if (Cast >= 4) LiftNow = (std::max)(0.f, XMVectorGetY(middle) - tall * GuardHips);   // (the camera goes up with her)
            if ((Air == 1 || Cast == 4) && dt > 1e-4f)
            {   // the air she turns: streaks on a circle round her length, going round as she does, one each side
                const XMVECTOR length = Pitched(XMVectorSet(0, 1, 0, 0), pitch), side = XMVectorSet(1, 0, 0, 0), across = XMVector3Cross(length, side);
                const XMVECTOR chest = middle + length * (.3f * tall);
                const int count = (std::max)(1, static_cast<int>(dt / CastAirEvery));
                for (int k = 0; k < count; ++k)
                    for (int arm = 0; arm < 2; ++arm)
                    {
                        const float a = was + (TwistNow - was) * static_cast<float>(k + 1) / static_cast<float>(count) + static_cast<float>(arm) * XM_PI;
                        const XMVECTOR out = side * std::cos(a) + across * std::sin(a), go = across * std::cos(a) - side * std::sin(a);
                        const float round = CastAirRound * Rand(.8f, 1.1f), along = Rand(-.5f, .5f);
                        Marks.push_back({There(world, chest + out * round + length * along), There(world, go * 3.f, false), CastAirLife, CastAirLife, .016f, 0, 4});
                    }
            }
        }
        // her twist stops on the creature, or the dash is over: the turned air thrown out in a ring round her length
        void TurnedAirRing(const WorldState& world)
        {
            RingDue = false;
            const float tall = (std::max)(.01f, world.AnimatedPlayerVisual.Scale.y), pitch = BodyMove ? BodyPitch : FlyLean;
            const XMVECTOR at = BodyMove ? Load3(BodyAt) : XMVectorSet(0, LiftNow, 0, 0);
            const XMVECTOR length = Pitched(XMVectorSet(0, 1, 0, 0), pitch), side = XMVectorSet(1, 0, 0, 0), across = XMVector3Cross(length, side);
            const XMVECTOR chest = tall * Pitched(Load3(BodyMove && Cast == 5 ? FlipAt(FlipHipsAt, CastTime) : DiveHips), pitch) + at + length * (.3f * tall);
            for (int k = 0; k < 28; ++k)
            {
                const float a = XM_2PI * static_cast<float>(k) / 28;
                const XMVECTOR out = side * std::cos(a) + across * std::sin(a);
                Marks.push_back({There(world, chest + out * .5f), There(world, out * 5.f, false), .35f, .35f, .02f, 0, 4});
            }
        }
        // How far she has turned in the storm at t (radians, to her left), and how fast (radians a second of the clip).
        float StormTurn(float t, float* rate = nullptr) const
        {
            if (rate) *rate = 0;
            if (StormTurned.size() < 2) return 0;
            const float f = std::clamp(t * 120.f, 0.f, static_cast<float>(StormTurned.size() - 1) - 1e-3f);
            const size_t i = static_cast<size_t>(f);
            if (rate) *rate = (StormTurned[i + 1] - StormTurned[i]) * 120.f;
            return StormTurned[i] + (StormTurned[i + 1] - StormTurned[i]) * (f - static_cast<float>(i));
        }
        // And her picture's height in it (m): up from the ground fast and slowing, floating at the top, down slowing again.
        static float StormLift(float t)
        {
            if (t <= StormLeave || t >= StormDown) return 0;
            if (t < StormTop) { const float u = (t - StormLeave) / (StormTop - StormLeave); return StormHeight * (1 - (1 - u) * (1 - u)); }
            if (t < StormFall) return StormHeight + StormFloat * std::sin(XM_PI * (t - StormTop) / (StormFall - StormTop));
            const float u = (t - StormFall) / (StormDown - StormFall);
            return (StormHeight) * (1 - u * u * (3 - 2 * u));
        }
#pragma warning(push)
#pragma warning(disable : 4324)     // (padded for the vectors' sake)
        struct HandleGrip { XMVECTOR Wrist, Elbow, Side, Hand, Blade, Middle, Lies; };   // her hand on a handle on her back: where her wrist and elbow are, her elbow's side, her hand's turn; the blade, the handle's middle, the sword's turn as it lies
        struct ArmWay       // one arm's way between a pose and its handle: what a frame gives
        {
            XMVECTOR Shoulder, ElbowThen, WristThen;        // the pose's arm, on her body as it is
            XMVECTOR UpperThen, ForeThen, HandThen;         // its bones' turns
            XMVECTOR SwordThen, GripThen;                   // the sword's turn as that hand holds it, and its handle's middle in that hand
            XMVECTOR Chest;                                 // her chest's turn
            HandleGrip Grip;
            XMVECTOR Blades[17];                            // the blade's way from her back to the pose
            float Gone[17];                                 // how far along each of them is (radians)
            float Sign = 1, Upper = 0, Fore = 0, Low = 0, Front = 0;
            int Arm = 0;
        };
        struct ArmNow { XMVECTOR Upper, Fore, Hand, Wrist, Sword, SwordAt; };       // an arm on its way: its bones' turns, her wrist; the sword's turn and place
#pragma warning(pop)
        // Her hand on the handle of the sword on her back.
        HandleGrip GripOnBack(int s, const XMMATRIX& chest, XMVECTOR chestTurn, XMVECTOR shoulder, float upper, float fore) const
        {
            const float sign = s == 0 ? 1.f : -1.f;       // her left is +x
            HandleGrip g{};
            const XMMATRIX lies = XMLoadFloat4x4(&SwordOnBack[s]) * chest;
            XMVECTOR sc, turned, at;
            if (!XMMatrixDecompose(&sc, &turned, &at, lies)) turned = XMQuaternionIdentity();
            g.Lies = XMQuaternionNormalize(turned);
            g.Middle = XMVector3TransformCoord(XMLoadFloat3(&SwordGrip[s]), lies);
            g.Blade = Unit(XMVector3TransformCoord(XMLoadFloat3(&SwordTip[s]), lies) - g.Middle, XMVectorSet(0, -1, 0, 0));
            const XMVECTOR whole = After(g.Lies, XMQuaternionConjugate(XMLoadFloat4(&SwordTurnInHand[s])));       // the hand that holds it as it lies
            const XMVECTOR inHand = XMLoadFloat3(&GripInHand[s]), line0 = XMVector3Rotate(XMLoadFloat3(&HandLine[s]), whole);
            const XMVECTOR elbowUp = XMVector3Rotate(XMVectorSet(sign * .09f, .22f, .04f, 0), chestTurn);           // her elbow up by her ear
            g.Hand = whole;
            for (int pass = 0; ; ++pass)
            {
                g.Wrist = g.Middle - XMVector3Rotate(inHand, g.Hand);
                g.Elbow = ElbowAt(shoulder, g.Wrist, upper, fore, elbowUp, &g.Side);
                if (pass == 8) break;
                // turned about the handle to where her forearm comes from, and no further bent than a wrist bends
                const XMVECTOR comes = XMVector3Normalize(g.Wrist - g.Elbow);
                const XMVECTOR line = comes - g.Blade * XMVector3Dot(comes, g.Blade);
                float roll = 0;
                if (XMVectorGetX(XMVector3LengthSq(line)) > 1e-4f)
                {
                    const XMVECTOR to = XMVector3Normalize(line);
                    roll = std::atan2(XMVectorGetX(XMVector3Dot(XMVector3Cross(line0, to), g.Blade)), XMVectorGetX(XMVector3Dot(line0, to)));
                }
                g.Hand = HoldWrist(s, After(XMQuaternionRotationAxis(g.Blade, roll), whole), comes, false);
            }
            return g;
        }
        // The blade's way from her back to where the pose has it: out past her shoulder (a hand that is low and before
        // her: back and up over it), a curve on the ball of directions, known by how far along it is.
        void BladeWay(ArmWay& w) const
        {
            const XMVECTOR from = w.Grip.Blade, to = Unit(XMVector3Rotate(XMLoadFloat3(&BladeInSword[w.Arm]), w.SwordThen), w.Grip.Blade);
            const XMVECTOR backward = XMVector3Rotate(XMVector3Normalize(XMVectorSet(w.Sign * .60f, .15f, -1.f, 0)), w.Chest);
            const XMVECTOR upward = XMVector3Rotate(XMVector3Normalize(XMVectorSet(w.Sign * .30f, .95f, -.10f, 0)), w.Chest);
            const XMVECTOR outward = XMVector3Rotate(XMVector3Normalize(XMVectorSet(w.Sign * .70f, -.70f, -.10f, 0)), w.Chest);
            const XMVECTOR first = Unit(from + Unit(XMVectorLerp(outward, backward, w.Front), outward) * 1.6f, from);
            const XMVECTOR second = Unit(XMVectorLerp(to, upward, w.Front), to);
            for (int i = 0; i <= 16; ++i)
            {
                const float a = i / 16.f, b = 1 - a;
                w.Blades[i] = Unit(from * (b * b * b) + first * (4.5f * a * b * b) + second * (4.5f * a * a * b) + to * (a * a * a), i ? w.Blades[i - 1] : from);
                w.Gone[i] = i ? w.Gone[i - 1] + std::acos(std::clamp(XMVectorGetX(XMVector3Dot(w.Blades[i - 1], w.Blades[i])), -1.f, 1.f)) : 0.f;
            }
        }
        static XMVECTOR BladeAt(const ArmWay& w, float a)
        {
            const float want = a * w.Gone[16];
            int i = 0;
            while (i < 15 && w.Gone[i + 1] < want) ++i;
            const float span = w.Gone[i + 1] - w.Gone[i];
            return Unit(XMVectorLerp(w.Blades[i], w.Blades[i + 1], span > 1e-6f ? std::clamp((want - w.Gone[i]) / span, 0.f, 1.f) : 0.f), w.Blades[i]);
        }
        // One arm, v of its way from the handle (0) to the pose (1), dt after the step before (st: what that step left).
        void ArmGoes(const ArmWay& w, float v, float dt, ArmState& st, ArmNow& now) const
        {
            const int s = w.Arm;
            const HandleGrip& g = w.Grip;
            const XMVECTOR at = w.Shoulder;
            v = std::clamp(v, 0.f, 1.f);
            const float rest = 1 - v, e = rest * rest * rest * (10 - 15 * rest + 6 * rest * rest), f = 1 - e;       // (e: 1 at the handle)
            // her wrist's way
            const XMVECTOR by = XMVectorLerp((w.WristThen + g.Wrist) * .5f, at + XMVector3Rotate(XMVectorSet(w.Sign * .16f, .30f, .26f, 0), w.Chest), w.Low);
            XMVECTOR wrist = w.WristThen * (f * f) + by * (2 * e * f) + g.Wrist * (e * e);
            // her elbow's side
            const XMVECTOR mid = XMVector3Rotate(XMVectorLerp(XMVectorSet(w.Sign * .26f, -.05f, -.10f, 0), XMVectorSet(w.Sign * .26f, .04f, .14f, 0), w.Low), w.Chest);
            const XMVECTOR pole = (w.ElbowThen - at) * (f * f) + mid * (2 * e * f) + (g.Elbow - at) * (e * e);
            const XMVECTOR axis = Unit(wrist - at, XMVectorSet(0, -1, 0, 0));
            bool wanted = false;
            const XMVECTOR want = Across(pole, axis, wanted);
            XMVECTOR side = wanted ? want : g.Side;
            if (st.Known)
            {
                bool kept = false;
                const XMVECTOR was = XMLoadFloat3(&st.Side);
                XMVECTOR carried = Across(XMVector3Rotate(was, ArcTurn(XMLoadFloat3(&st.Axis), axis)), axis, kept);
                if (!kept) carried = was;
                side = carried;
                if (wanted)
                {
                    const float turn = std::atan2(XMVectorGetX(XMVector3Dot(XMVector3Cross(carried, want), axis)), XMVectorGetX(XMVector3Dot(carried, want)));
                    const float follow = (std::max)(1 - std::exp(-ElbowFollows * dt), Ramp(std::abs(2 * e - 1), .94f, 1.f));    // (at either end: where it belongs)
                    side = XMVector3Rotate(carried, XMQuaternionRotationAxis(axis, turn * follow));
                }
            }
            const XMVECTOR elbow = ElbowAt(at, wrist, w.Upper, w.Fore, side, &side);
            const XMVECTOR upperLine = XMVector3Normalize(elbow - at), foreLine = XMVector3Normalize(wrist - elbow), hinge = XMVector3Cross(side, axis);
            const XMVECTOR bentHinge = XMLoadFloat3(&ArmBent[s].Hinge), bentUpperLine = XMLoadFloat3(&ArmBent[s].UpperLine), bentForeLine = XMLoadFloat3(&ArmBent[s].ForeLine);
            const XMVECTOR bentUpper = XMLoadFloat4(&ArmBent[s].Upper), bentFore = XMLoadFloat4(&ArmBent[s].Fore), foreAxis = XMLoadFloat3(&ForeAxis[s]);
            const XMVECTOR hinged = After(FrameTurn(bentUpperLine, bentHinge, upperLine, hinge), bentUpper);
            // the pose's own: the plane its arm bends in (a straight arm has none: the one her arm has now), its wrist's
            // roll and bend
            const XMVECTOR toElbow = w.ElbowThen - at, axisThen = Unit(w.WristThen - at, axis);
            bool own = false, lends = false;
            const XMVECTOR sideOwn = Across(toElbow, axisThen, own);
            XMVECTOR sideLent = Across(side, axisThen, lends);
            if (!lends) sideLent = side;
            const float bent = XMVectorGetX(XMVector3Length(toElbow - axisThen * XMVector3Dot(toElbow, axisThen)));
            const XMVECTOR sideThen = own ? Unit(XMVectorLerp(sideLent, sideOwn, Ramp(bent, .01f, .03f)), sideOwn) : sideLent;
            const XMVECTOR upperLineThen = Unit(toElbow, upperLine), foreLineThen = Unit(w.WristThen - w.ElbowThen, foreLine);
            const XMVECTOR hingedThen = After(FrameTurn(bentUpperLine, bentHinge, upperLineThen, XMVector3Cross(sideThen, axisThen)), bentUpper);
            const XMVECTOR rollThen = TwistLast(After(XMQuaternionConjugate(w.ForeThen), w.HandThen), foreAxis);
            const XMVECTOR bendThen = After(After(XMQuaternionConjugate(rollThen), XMQuaternionConjugate(w.ForeThen)), w.HandThen);
            // the grip's: its wrist's bend, with her forearm rolled as her hand is turned
            const XMVECTOR foreLineGrip = XMVector3Normalize(g.Wrist - g.Elbow);
            const XMVECTOR hingedGrip = After(FrameTurn(bentForeLine, bentHinge, foreLineGrip, XMVector3Cross(g.Side, Unit(g.Wrist - at, axis))), bentFore);
            const XMVECTOR frameGrip = After(hingedGrip, TwistLast(After(XMQuaternionConjugate(hingedGrip), g.Hand), foreAxis));
            const XMVECTOR bend = XMQuaternionSlerp(bendThen, After(XMQuaternionConjugate(frameGrip), g.Hand), e);
            // her upper arm as the pose rolled it, carried with the plane her arm bends in
            const XMVECTOR upperTurn = After(After(hinged, XMQuaternionConjugate(hingedThen)), w.UpperThen);
            // the sword's way: ahead of her hand
            const float a = Ramp((std::min)(1.f, v / BladeLeads), 0.f, 1.f);
            const XMVECTOR bladeIn = XMLoadFloat3(&BladeInSword[s]), bladeWanted = BladeAt(w, a);
            const XMVECTOR base = XMQuaternionSlerp(g.Lies, w.SwordThen, a);
            const XMVECTOR swordTurn = XMQuaternionNormalize(After(ArcTurn(Unit(XMVector3Rotate(bladeIn, base), bladeWanted), bladeWanted), base));
            // her forearm and hand: as they were, carried to where her forearm is now, and rolled about it toward where
            // the blade her fist would hold is nearest the sword's: at a limited rate, as far as that is well defined
            const XMVECTOR frameAtGrip = After(ArcTurn(foreLineGrip, foreLine), frameGrip);
            XMVECTOR frame = frameAtGrip;
            if (st.Known)
            {
                frame = After(ArcTurn(XMLoadFloat3(&st.ForeLine), foreLine), XMLoadFloat4(&st.Frame));
                const XMVECTOR held = XMVector3Rotate(bladeIn, After(After(frame, bend), XMLoadFloat4(&SwordTurnInHand[s])));
                const XMVECTOR b0 = held - foreLine * XMVector3Dot(held, foreLine), w0 = bladeWanted - foreLine * XMVector3Dot(bladeWanted, foreLine);
                const float sure = Ramp((std::min)(XMVectorGetX(XMVector3Length(b0)), XMVectorGetX(XMVector3Length(w0))), .12f, .40f);
                if (sure > 0)
                {
                    const float goal = std::atan2(XMVectorGetX(XMVector3Dot(XMVector3Cross(b0, w0), foreLine)), XMVectorGetX(XMVector3Dot(b0, w0)));
                    const float most = XMConvertToRadians(RollMost) * dt;
                    frame = After(XMQuaternionRotationAxis(foreLine, std::clamp(goal * (1 - std::exp(-RollFollows * dt)) * sure, -most, most)), frame);
                }
                frame = XMQuaternionSlerp(frame, frameAtGrip, Ramp(e, .94f, 1.f));       // (at the grip: the grip's own)
            }
            frame = XMQuaternionNormalize(frame);
            st.Known = true;
            XMStoreFloat3(&st.Side, side); XMStoreFloat3(&st.Axis, axis); XMStoreFloat3(&st.ForeLine, foreLine); XMStoreFloat4(&st.Frame, frame);
            // toward the pose her arm is the pose's own, carried to where her elbow and wrist are
            const float mine = Ramp(e, 0.f, .5f);
            const XMVECTOR carry = ArcTurn(foreLineThen, foreLine);
            now.Upper = XMQuaternionNormalize(XMQuaternionSlerp(After(ArcTurn(upperLineThen, upperLine), w.UpperThen), upperTurn, mine));
            now.Fore = XMQuaternionNormalize(XMQuaternionSlerp(After(carry, w.ForeThen), After(frame, XMQuaternionConjugate(rollThen)), mine));
            now.Hand = XMQuaternionNormalize(XMQuaternionSlerp(After(carry, w.HandThen), After(frame, bend), mine));
            now.Wrist = wrist;
            now.Sword = swordTurn;
            now.SwordAt = wrist + XMVector3Rotate(XMVectorLerp(XMLoadFloat3(&GripInHand[s]), w.GripThen, a), now.Hand) - XMVector3Rotate(XMLoadFloat3(&GripInSword[s]), swordTurn);
        }
        // Her arms on the way between the pose they come from, or go to (there: 0), and the handles (there: 1); with the
        // swords, while they are in her hands. False: not done (her arms are left as they were).
        bool ReachArms(DoorwaysSkeletonPose& out, bool leaving, float there, float dt)
        {
            const size_t n = out.LocalJoints.size();
            if (!GripKnown || !HasBack || Spine2 < 0 || BlendFrom.LocalJoints.size() != n || ArmTo.size() != n) return false;
            XMFLOAT4 kept[2][3];
            for (int s = 0; s < 2; ++s)
                for (int b = 1; b < 4; ++b)
                {   // her arms as the other pose has them, on her body as it is now
                    const int j = ArmChain[s][b];
                    kept[s][b - 1] = out.LocalJoints[j].Rotation;
                    out.LocalJoints[j].Rotation = leaving ? ArmTo[j] : BlendFrom.LocalJoints[j].Rotation;
                }
            const auto putBack = [&](int arm) { for (int bone = 1; bone < 4; ++bone) out.LocalJoints[ArmChain[arm][bone]].Rotation = kept[arm][bone - 1]; };
            if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr)) { putBack(0); putBack(1); return false; }
            const auto place = [&](int j) { const auto& m = out.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            const auto turnOf = [&](int j) { XMVECTOR sc, q, t; XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[j])); return XMQuaternionNormalize(q); };
            const XMMATRIX chest = XMLoadFloat4x4(&out.ModelSpaceMatrices[Spine2]);
            const XMVECTOR chestTurn = turnOf(Spine2);
            there = std::clamp(there, 0.f, 1.f);
            for (int s = 0; s < 2; ++s)
            {
                const int ua = ArmChain[s][1], fa = ArmChain[s][2], ha = ArmChain[s][3], sj = SwordJoint[s];
                ArmWay way;
                way.Arm = s; way.Sign = s == 0 ? 1.f : -1.f;       // her left is +x
                way.Shoulder = place(ua); way.ElbowThen = place(fa); way.WristThen = place(ha);
                way.UpperThen = turnOf(ua); way.ForeThen = turnOf(fa); way.HandThen = turnOf(ha); way.Chest = chestTurn;
                way.Upper = XMVectorGetX(XMVector3Length(way.ElbowThen - way.Shoulder)); way.Fore = XMVectorGetX(XMVector3Length(way.WristThen - way.ElbowThen));
                if (way.Upper < .01f || way.Fore < .01f) { putBack(s); continue; }
                // the sword as the pose's hand has it: its own while it is in her hand, else as a fist would hold it
                way.SwordThen = InHand ? turnOf(sj) : After(way.HandThen, XMLoadFloat4(&SwordTurnInHand[s]));
                way.GripThen = InHand ? XMVector3Rotate(XMVector3TransformCoord(XMLoadFloat3(&SwordGrip[s]), XMLoadFloat4x4(&out.ModelSpaceMatrices[sj])) - way.WristThen, XMQuaternionConjugate(way.HandThen))
                                      : XMLoadFloat3(&GripInHand[s]);
                way.Grip = GripOnBack(s, chest, chestTurn, way.Shoulder, way.Upper, way.Fore);
                way.Low = Ramp(XMVectorGetY(way.Shoulder - way.WristThen), .05f, .30f);       // a hand below her shoulder goes round it, before her
                way.Front = Ramp(XMVectorGetX(XMVector3Dot(way.WristThen - way.Shoulder, XMVector3Rotate(XMVectorSet(0, 0, 1, 0), chestTurn))), -.05f, .15f) * way.Low;
                BladeWay(way);
                ArmNow now{};
                if (leaving) ArmGoes(way, 1 - there, dt, Reaching[s], now);
                else
                {   // to the handle: the way from it, gone through as far as where her hand is
                    const int steps = (std::max)(4, static_cast<int>(std::lround(DrawReach * 60)));
                    const float along = (1 - there) * steps;
                    const int last = (std::min)(steps, static_cast<int>(along) + 1);
                    ArmState st;
                    ArmNow before{};
                    for (int i = 0; i <= last; ++i) { before = now; ArmGoes(way, static_cast<float>(i) / steps, DrawReach / steps, st, now); }
                    const float part = std::clamp(along - (last - 1), 0.f, 1.f);
                    now.Upper = XMQuaternionSlerp(before.Upper, now.Upper, part); now.Fore = XMQuaternionSlerp(before.Fore, now.Fore, part);
                    now.Hand = XMQuaternionSlerp(before.Hand, now.Hand, part); now.Sword = XMQuaternionSlerp(before.Sword, now.Sword, part);
                    now.Wrist = XMVectorLerp(before.Wrist, now.Wrist, part); now.SwordAt = XMVectorLerp(before.SwordAt, now.SwordAt, part);
                }
                const XMVECTOR above = turnOf(Skeleton.Joints[ua].ParentIndex);
                XMStoreFloat4(&out.LocalJoints[ua].Rotation, XMQuaternionNormalize(After(XMQuaternionConjugate(above), now.Upper)));
                XMStoreFloat4(&out.LocalJoints[fa].Rotation, XMQuaternionNormalize(After(XMQuaternionConjugate(now.Upper), now.Fore)));
                XMStoreFloat4(&out.LocalJoints[ha].Rotation, XMQuaternionNormalize(After(XMQuaternionConjugate(now.Fore), now.Hand)));
                if (InHand)
                {   // the sword on its own way, its handle in her fist
                    XMVECTOR sc, q, t;
                    if (!XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[ha]))) sc = XMVectorSplatOne();     // (her hand's size, if it has one)
                    XMStoreFloat4(&out.LocalJoints[sj].Rotation, XMQuaternionNormalize(After(XMQuaternionConjugate(now.Hand), now.Sword)));
                    XMStoreFloat3(&out.LocalJoints[sj].Translation, XMVector3Rotate(now.SwordAt - now.Wrist, XMQuaternionConjugate(now.Hand)) / XMVectorSetW(sc, 1));
                }
            }
            return true;
        }

        void StartAttack(const WorldState& world, const std::string& name, float into = 0)
        {
            Attack = name; ++Begun;
            // A cut is entered so that her body can come to it (2026-11-19; owner: "odd gaps between attacks",
            // "animations seems like concated wrong way", "hands are not following the skills and combos properly").
            // Every cut was entered 0.34 s before its blow and her body brought to it in a tenth of a second, wherever
            // it was: her hand was thrown to the clip's wind-up at 14 to 27 m/s, faster than any cut (8 to 15), turning
            // at 1400 to 3700 degrees a second, and then waited there for its swing. Now what she was doing dies away
            // evenly until just before the blow, and how long before the blow she enters is chosen for her hands
            // (BestLead). Out of standing at ease the time is the draw's: to the handles, and from them into the swing.
            float lead = AttackLead;
            CutBlend = 0;
            const auto swing = Swings.find(name);
            if (swing != Swings.end() && name != "Attack_Jump")
            {
                if (!InHand) lead = (DrawReachPressed + DrawOnCut) * AttackRate + CutEnd;
                else
                {
                    lead = BestLead(name, swing->second);
                    CutBlend = (lead - CutEnd) / AttackRate;
                }
            }
            AttackTime = name.rfind("Skill_", 0) == 0 ? 0.f : name == "Attack_Jump" ? (std::min)(.25f + into, (std::max)(.25f, Hit(name) - .15f))
                                                                                     : (std::max)(0.f, Hit(name) - lead);
            Queued = 0;
            Jumping = false;
            Log(world, ("attack " + name).c_str());
            if (swing != Swings.end()) Log(world, ("cut lead " + std::to_string(lead) + " blend " + std::to_string(CutBlend) + " swing " + std::to_string(swing->second.Lead)).c_str());
        }

        // Metres per second forward that the move being played carries her (its own path, taken out of the clip).
        float ForwardSpeed(const WorldState& world) const
        {
            if (InMove()) return 0;   // (a style's move carries her itself: MoveVel, the travel in Update)
            if (Air) return DashSpeed;
            if (Cast == 4) return world.MainPlayer.TraversalMode == DoorwaysPlayerTraversalMode::Flight ? 0.f : CastRootGo;
            if (Cast == 5) return 0.f;
            if (Soul == 1 && Attack == SoulClip) return SoulGo(AttackTime) * AttackRate * Pace;   // (her soul left: thrown on)
            const auto found = NyxaraClips::Attacks.find(Attack);
            if (found == NyxaraClips::Attacks.end() || found->second.Forward.size() < 2) return 0;
            const auto& path = found->second.Forward;
            const size_t i = (std::min)(static_cast<size_t>((std::max)(0.f, AttackTime) * 30), path.size() - 2);
            float go = (path[i + 1] - path[i]) * 30;   // (m a second of the clip)
            if (Attack == "Skill_Slide") go += SlideMore(AttackTime, path.back());   // (F: three times as far)
            return go * AttackRate * Pace * (Skill() ? SkillReach : 1.f);
        }

        // She looks where she turns (2026-11-19): her head and neck lead a turn, as far as she turns fast, and come
        // back after it. turn: radians a second, to her right. And she keeps an eye on the nearest creature before
        // her: toward, where it is from her facing (radians, to her right; NaN: none in sight).
        void LookAbout(DoorwaysSkeletonPose& out, float dt, float turn, float toward, float share)
        {
            const float to = std::isfinite(toward) ? std::clamp(toward * .85f, -.95f, .95f) : std::clamp(turn * .28f, -.5f, .5f);
            Look += (to * share - Look) * (std::min)(1.f, dt * 5);
            if (NeckJoint < 0 || HeadJoint < 0 || std::abs(Look) < 1e-3f || out.ModelSpaceMatrices.size() != out.LocalJoints.size()) return;
            const int above = Skeleton.Joints[NeckJoint].ParentIndex;
            if (above < 0 || Skeleton.Joints[HeadJoint].ParentIndex != NeckJoint) return;
            const auto turnOf = [&](int j) { XMVECTOR s, q, t; XMMatrixDecompose(&s, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[j])); return XMQuaternionNormalize(q); };
            const XMVECTOR upright = XMVectorSet(0, 1, 0, 0);
            const XMVECTOR neck = XMQuaternionMultiply(turnOf(NeckJoint), XMQuaternionRotationAxis(upright, Look * .4f));
            const XMVECTOR head = XMQuaternionMultiply(turnOf(HeadJoint), XMQuaternionRotationAxis(upright, Look));
            XMStoreFloat4(&out.LocalJoints[NeckJoint].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(neck, XMQuaternionInverse(turnOf(above)))));
            XMStoreFloat4(&out.LocalJoints[HeadJoint].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(head, XMQuaternionInverse(neck))));
            EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr);
        }

        // Her wings answer what the world does to her (2026-11-19). In her clips they swing as the clip's own chest
        // moves, worked out in Blender: so they knew nothing of a jump, a fall, a landing, a turn, a start or a stop, and
        // stood through all of them as painted on. Here each wing is a damped spring about where the clip holds it, up
        // and down (Flap) and back and forth (Back): falling, the air lifts it, and rising presses it down; a jump off
        // and a landing knock it down and it swings back; in a turn the outer wing trails and the inner one swings
        // forward; gathering speed both are left behind, losing it they swing forward. The two outer bones follow the
        // root late, so the wing bends along its length. climb: m/s up, off the ground on foot; gain: m/s2 along her
        // facing; turn: radians a second, to her right; share: how much of it all (less in flight: Fly beats them).
        // fold: how far they are lowered and drawn in (at ease: she does not stand with her wings spread as for flight).
        void SwingWings(DoorwaysSkeletonPose& out, float dt, float climb, float gain, float turn, float share, float fold)
        {
            constexpr float Beat = XM_2PI * 1.3f, Damp = .42f, Most = .6f;
            for (float left = dt; left > 1e-6f;)
            {   // lowered slowly (over a second), opened quickly (under half a second); neither with a start
                const float h = (std::min)(left, 1 / 120.f), quick = fold > WingFold ? 3.6f : 10.f; left -= h;
                WingFoldGo += (quick * quick * (fold - WingFold) - 2 * quick * WingFoldGo) * h;
                WingFold = std::clamp(WingFold + WingFoldGo * h, 0.f, 1.f);
            }
            for (float left = dt; left > 1e-6f;)
            {   // closed round her (her wing shield, E): quickly, without a start; opened as quickly
                const float h = (std::min)(left, 1 / 120.f), r = WingCloseRate; left -= h;
                WingCloseGo += (r * r * (WingCloseTo - WingClose) - 2 * r * WingCloseGo) * h;
                WingClose = std::clamp(WingClose + WingCloseGo * h, 0.f, 1.1f);
            }
            for (const auto& wing : WingJoint) for (const int j : wing) if (j < 0) return;
            if (dt <= 1e-5f || out.ModelSpaceMatrices.size() != out.LocalJoints.size()) return;
            WingShare += (share - WingShare) * (std::min)(1.f, dt * 6);
            float blow = WingKnown ? climb - WingClimb : 0.f;      // her rise changed at once: she jumped off, or landed
            if (std::abs(blow) < 3) blow = 0;
            WingClimb = climb; WingKnown = true;
            const float flapTo = WingShare * (climb < 0 ? (std::min)(.45f, -climb * .03f) : -(std::min)(.14f, climb * .015f)) + WingAimFlap + StyleWingFlap;
            const auto at = [&](int j) { const auto& m = out.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            const auto turnOf = [&](int j) { XMVECTOR s, q, t; XMMatrixDecompose(&s, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[j])); return XMQuaternionNormalize(q); };
            // Her chest's own right, up and forward, from the two wing roots and her spine: a wing is lifted and swept
            // about these. (It was lifted toward the world's up and swept toward the world's back "as the wing sees
            // them", across its own length: with a wing pointing nearly straight back, as when her chest turns in a
            // cut, "back" had no direction across it, changed sides between two frames, and the wing tip jumped 30 cm;
            // and which wing was the left one was read from where it pointed. 2026-11-19.)
            XMVECTOR right = (at(WingJoint[1][0]) - at(WingJoint[0][0])) * WingRightIs;
            right = XMVectorGetX(XMVector3LengthSq(right)) < 1e-6f ? XMVectorSet(1, 0, 0, 0) : XMVector3Normalize(right);
            const int chest = Skeleton.Joints[WingJoint[0][0]].ParentIndex, under = chest >= 0 ? Skeleton.Joints[chest].ParentIndex : -1;
            XMVECTOR up = under >= 0 ? at(chest) - at(under) : XMVectorSet(0, 1, 0, 0);
            if (StyleHold > 1e-3f && WingsHeldKnown)
            {   // (her wings held in her way of going: lifted and swept about her chest as it stands on average, its bow with it,
                // not as it twists in her stride: StylePose)
                const XMVECTOR chestTurn = XMLoadFloat4(&StyleChestTurn);   // (not turn: SwingWings' own turn is her turning)
                const XMVECTOR heldRight = XMVector3Rotate(XMVectorLerp(XMLoadFloat3(&WingRightHeld[0]), XMLoadFloat3(&WingRightHeld[1]), GaitRun), chestTurn);
                const XMVECTOR heldUp = XMVector3Rotate(XMVectorLerp(XMLoadFloat3(&WingUpHeld[0]), XMLoadFloat3(&WingUpHeld[1]), GaitRun), chestTurn);
                right = XMVector3Normalize(XMVectorLerp(right, XMVector3Normalize(heldRight), StyleHold));
                if (XMVectorGetX(XMVector3LengthSq(up)) > 1e-8f) up = XMVector3Normalize(up);
                up = XMVectorLerp(up, XMVector3Normalize(heldUp), StyleHold);
            }
            up = up - right * XMVectorGetX(XMVector3Dot(up, right));
            up = XMVectorGetX(XMVector3LengthSq(up)) < 1e-6f ? XMVectorSet(0, 1, 0, 0) : XMVector3Normalize(up);
            const XMVECTOR forward = XMVector3Normalize(XMVector3Cross(right, up));
            bool moved = false;
            for (int k = 0; k < 2; ++k)
            {
                auto& w = WingSpring[k];
                const int* j = WingJoint[k];
                const int above = Skeleton.Joints[j[0]].ParentIndex;
                if (above < 0 || Skeleton.Joints[j[1]].ParentIndex != j[0] || Skeleton.Joints[j[2]].ParentIndex != j[1]) continue;
                const float side = (k ? 1.f : -1.f) * WingRightIs;       // the wing on her +x side, or the other
                const XMVECTOR outward = right * side;
                const XMVECTOR liftAxis = XMVector3Cross(outward, up), sweepAxis = XMVector3Cross(forward, outward);   // (turning it up, and back)
                const float backTo = WingShare * std::clamp(-side * turn * .13f, -.3f, .3f) + WingAimBack + StyleWingBack;
                w.FlapGo -= WingShare * blow * .14f;
                for (float left = dt; left > 1e-6f;)
                {
                    const float h = (std::min)(left, 1 / 120.f); left -= h;
                    w.FlapGo += (-Beat * Beat * (w.Flap - flapTo) - 2 * Damp * Beat * w.FlapGo) * h;
                    w.Flap = std::clamp(w.Flap + w.FlapGo * h, -Most, Most);
                    w.BackGo += (-Beat * Beat * (w.Back - backTo) - 2 * Damp * Beat * w.BackGo + WingShare * gain * 1.1f) * h;
                    w.Back = std::clamp(w.Back + w.BackGo * h, -Most, Most);
                }
                const float late = (std::min)(1.f, dt * 9);
                w.Flap2 += (w.Flap - w.Flap2) * late; w.Flap3 += (w.Flap2 - w.Flap3) * late;
                w.Back2 += (w.Back - w.Back2) * late; w.Back3 += (w.Back2 - w.Back3) * late;
                const float open = 1 - (std::min)(1.f, WingClose);   // (closed, her wings are not lowered at ease)
                const float flaps[3] = {w.Flap + open * WingFold * FoldFlap[0] + WingClose * CloseFlap[0], w.Flap2 * .5f + open * WingFold * FoldFlap[1] + WingClose * CloseFlap[1],
                                        w.Flap3 * .4f + open * WingFold * FoldFlap[2] + WingClose * CloseFlap[2]};
                const float backs[3] = {w.Back + open * WingFold * FoldBack[0] + WingClose * CloseBack[0], w.Back2 * .45f + open * WingFold * FoldBack[1] + WingClose * CloseBack[1],
                                        w.Back3 * .35f + open * WingFold * FoldBack[2] + WingClose * CloseBack[2]};
                if (std::abs(flaps[0]) + std::abs(flaps[1]) + std::abs(flaps[2]) + std::abs(backs[0]) + std::abs(backs[1]) + std::abs(backs[2]) < 1e-4f) continue;
                XMVECTOR all = XMQuaternionIdentity(), parent = turnOf(above);
                for (int i = 0; i < 3; ++i)
                {
                    const XMVECTOR turned = XMQuaternionMultiply(XMQuaternionRotationAxis(liftAxis, flaps[i]), XMQuaternionRotationAxis(sweepAxis, backs[i]));
                    all = XMQuaternionNormalize(XMQuaternionMultiply(all, turned));                // this bone's turn on top of those before it
                    const XMVECTOR now = XMQuaternionNormalize(XMQuaternionMultiply(turnOf(j[i]), all));
                    XMStoreFloat4(&out.LocalJoints[j[i]].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(now, XMQuaternionInverse(parent))));
                    parent = now;
                }
                moved = true;
            }
            if (moved) EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr);
        }

        static XMVECTOR ArcTurn(XMVECTOR from, XMVECTOR to)   // the shortest turn that takes one direction to another
        {
            const float d = XMVectorGetX(XMVector3Dot(from, to));
            if (d < -.9999f)
            {
                XMVECTOR axis = XMVector3Cross(from, XMVectorSet(1, 0, 0, 0));
                if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-4f) axis = XMVector3Cross(from, XMVectorSet(0, 1, 0, 0));
                return XMQuaternionRotationAxis(XMVector3Normalize(axis), XM_PI);
            }
            // (the turn itself, down to no turn at all: under 0.8 degrees it was none, and a thigh so turned jumped 6 mm
            // at the knee as it came over that)
            return XMQuaternionNormalize(XMVectorSetW(XMVector3Cross(from, to), 1 + d));
        }
        // While the swords are out of her hands their two bones are put where the swords are in the world.
        void PlaceSwords(const WorldState& world, DoorwaysSkeletonPose& out)
        {
            if (!SwordsOut || SwordJoint[0] < 0 || SwordJoint[1] < 0) return;
            std::string error;
            if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, &error)) return;
            const auto& p = world.MainPlayer;
            // (where her picture is, all of it: turned, lifted, grown; until 2026-11-19 her place alone, and her swords in
            // the creature were drawn as far off as her picture was lifted)
            const XMMATRIX place = BuildDoorwaysCharacterWorldMatrix(p.Position, p.Yaw, world.AnimatedPlayerVisual);
            const XMMATRIX model = XMMatrixInverse(nullptr, place);
            XMVECTOR placeScale, placeTurn, placeAt;
            if (!XMMatrixDecompose(&placeScale, &placeTurn, &placeAt, place)) return;
            for (int k = 0; k < 2; ++k)
            {
                const int j = SwordJoint[k], parent = Skeleton.Joints[j].ParentIndex;
                if (parent < 0) continue;
                const XMVECTOR grip = XMLoadFloat3(&SwordGrip[k]), tip = XMLoadFloat3(&SwordTip[k]);
                const XMMATRIX held = XMLoadFloat4x4(&out.ModelSpaceMatrices[j]) * place;
                const XMVECTOR handAt = XMVector3TransformCoord(grip, held);
                const XMVECTOR handWay = XMVector3Normalize(XMVector3TransformCoord(tip, held) - handAt);
                XMVECTOR heldScale, heldTurn, heldAt;
                if (!XMMatrixDecompose(&heldScale, &heldTurn, &heldAt, held)) continue;
                heldTurn = XMQuaternionNormalize(heldTurn);
                if (SwordCapture)
                {
                    XMStoreFloat3(&SwordFrom[k], handAt);
                    XMStoreFloat3(&SwordFromWay[k], handWay);
                    XMStoreFloat4(&SwordFromTurn[k], heldTurn);
                }
                XMVECTOR at = XMVectorLerp(XMLoadFloat3(&SwordFrom[k]), XMLoadFloat3(&SwordTo[k]), SwordFlight);
                const XMVECTOR way = XMVector3Normalize(XMVectorLerp(XMLoadFloat3(&SwordFromWay[k]), XMLoadFloat3(&SwordToWay[k]), (std::min)(1.f, SwordFlight * 4)));
                // its turn in the world: as it left her hand, turned the shortest way to where it points (until 2026-11-19
                // the shortest way from its own bone, in her space: the swords stuck in a creature rolled about their blades
                // as she twisted, and jumped as they came back to her hands); coming out of it, to her hand's own
                XMVECTOR turn = XMQuaternionNormalize(XMQuaternionMultiply(XMLoadFloat4(&SwordFromTurn[k]), ArcTurn(XMLoadFloat3(&SwordFromWay[k]), way)));
                turn = XMQuaternionNormalize(XMQuaternionSlerp(turn, heldTurn, SwordBack));
                at = XMVectorLerp(at, handAt, SwordBack);
                const XMMATRIX above = XMLoadFloat4x4(&out.ModelSpaceMatrices[parent]);
                XMVECTOR aboveScale, aboveTurn, aboveAt;
                if (!XMMatrixDecompose(&aboveScale, &aboveTurn, &aboveAt, above)) continue;
                const XMVECTOR wantAt = XMVector3TransformCoord(at, model);
                XMMATRIX m = XMMatrixScalingFromVector(aboveScale) * XMMatrixRotationQuaternion(XMQuaternionMultiply(turn, XMQuaternionConjugate(XMQuaternionNormalize(placeTurn))));
                m = m * XMMatrixTranslationFromVector(wantAt - XMVector3TransformCoord(grip, m));
                XMVECTOR s, q, t;
                if (!XMMatrixDecompose(&s, &q, &t, m * XMMatrixInverse(nullptr, above))) continue;
                XMStoreFloat3(&out.LocalJoints[j].Translation, t);
                XMStoreFloat4(&out.LocalJoints[j].Rotation, XMQuaternionNormalize(q));
                XMStoreFloat3(&out.LocalJoints[j].Scale, XMVectorAbs(s));
            }
            SwordCapture = false;
        }
        // Her feet on the ground as it is drawn (owner, 2026-11-17). Two ways:
        // - Ground and slopes (any clip but Stairs_Up): each foot keeps the height over the ground under it that the clip
        //   gives it over flat ground, standing or stepping: no foot is told when it is down, so nothing jumps when her
        //   feet change. Her body comes down to the lower foot and the other leg bends; no leg is ever stretched past the
        //   clip. A foot standing (its ankle low) is set on the ground under its heel and turned to the slope, toes up the
        //   hill; a foot in the air is kept clear of the ground under heel and toes.
        // - Stairs_Up: the clip itself is fitted to the map's stairs (art/nyxara/blender/build_from_mixamo.py) and played
        //   by her height on the stair, so its feet stand on the treads' heights; each foot standing (StairsStance) is
        //   only slid along the stair onto the middle of its tread, the map's treads being 0.30 to 0.45 m deep.
        // A foot is moved by bending its leg: a two-bone turn of thigh and shin, the knee toward her toes.
        float PlantFeet(const WorldState& world, const NyxaraKit::FootGround& ground, const NyxaraKit::StairTread& stair, DoorwaysSkeletonPose& out, float dt, bool on)
        {
            constexpr float Ease = 10, FootEase = 16, FootRise = 40, BodyEase = 8, BodyFall = 16, SlideEase = 14, Down = .35f, Up = .45f, MostPitch = .45f;
            FeetOn += ((on ? 1.f : 0.f) - FeetOn) * (std::min)(1.f, dt * Ease);
            for (int k = 0; k < 4; ++k) if (LegJoint[k] < 0) return 0;
            if (FootJoint[0] < 0 || FootJoint[1] < 0 || out.ModelSpaceMatrices.size() != out.LocalJoints.size()) return 0;
            const auto& p = world.MainPlayer;
            const float scale = (std::max)(.01f, world.AnimatedPlayerVisual.Scale.y), c = std::cos(p.Yaw), sn = std::sin(p.Yaw);
            const auto at = [&](int j) { const auto& m = out.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            const auto worldX = [&](XMVECTOR v) { return p.Position.x + (XMVectorGetX(v) * c + XMVectorGetZ(v) * sn) * scale; };
            const auto worldZ = [&](XMVECTOR v) { return p.Position.z + (-XMVectorGetX(v) * sn + XMVectorGetZ(v) * c) * scale; };
            const auto surface = [&](XMVECTOR v, float nearTo = 0.f) -> float
            {   // the drawn ground under this point of her body, as metres over the place she stands (nearTo: about how high
                // there, for a stair's treads)
                const float x = worldX(v), z = worldZ(v);
                float y = ground ? ground(x, z, p.Position.y + nearTo) : std::numeric_limits<float>::quiet_NaN();
                if (!std::isfinite(y) && world.RuntimeScene)
                {
                    const auto hit = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*world.RuntimeScene, x, z, p.Position.y - Down - .1f, p.Position.y + Up + .1f);
                    if (hit.Hit) y = hit.GroundY;
                }
                return std::isfinite(y) ? y - p.Position.y : std::numeric_limits<float>::quiet_NaN();
            };
            const bool stairClip = Current == "Stairs_Up";
            // her picture leans as a whole about her feet (into a turn, gathering speed): what that lifts or lowers a foot
            // is taken back here, so her feet stay on the ground
            const XMMATRIX leaning = XMMatrixRotationRollPitchYaw(world.AnimatedPlayerVisual.Rotation.x, 0, world.AnimatedPlayerVisual.Rotation.z);
            const auto leanLift = [&](XMVECTOR v) { return (XMVectorGetY(XMVector3TransformNormal(v, leaning)) - XMVectorGetY(v)) * scale; };
            float lifted[2] = {0, 0}, raiseTo[2] = {0, 0}, pitchTo[2] = {0, 0}, slideTo[2][2] = {{0, 0}, {0, 0}};
            bool ballDown[2] = {false, false};
            float fitTo[2][2] = {{0, 0}, {0, 0}};      // (on a stair) how far each foot has still to go to stand clear of the risers (m, world x and z)
            float body = 0;
            for (int k = 0; k < 2; ++k)
            {   // in the air (1) or down (0): its ankle against the lowest the ankle stands in her clips (8 cm up: in the air)
                const float y = XMVectorGetY(at(FootJoint[k])) * scale;
                if (on && !stairClip) AnkleRest[k] = (std::min)(AnkleRest[k], y);
                const float h = AnkleRest[k] > 1e8f ? 0.f : std::clamp((y - (std::min)(AnkleRest[k], y) - .03f) / .05f, 0.f, 1.f);
                lifted[k] = h * h * (3 - 2 * h);
                if (ToeJoint[k] >= 0)
                {   // the ball of the foot down: a runner stands on it with her heel up, a walker pushes off from it
                    const float ball = XMVectorGetY(at(ToeJoint[k])) * scale;
                    if (on && !stairClip) BallRest[k] = (std::min)(BallRest[k], ball);
                    ballDown[k] = BallRest[k] < 1e8f && ball - (std::min)(BallRest[k], ball) < .035f;
                }
            }
            if (!stairClip && FeetOn > .01f)
            {
                float h[2] = {0, 0};
                // (her speed reads nothing for a frame as a key is let go: a foot in the air over a stair dropped 9 cm for it)
                HerSpeed += ((std::max)(Speed, SpeedBefore) - HerSpeed) * (std::min)(1.f, dt * 20);
                SpeedBefore = Speed;
                // the map's stair treads alone, as metres over the place she stands (NaN: none there)
                const auto tread = [&](XMVECTOR v, float nearTo) -> float
                {
                    return ground ? ground(worldX(v), worldZ(v), p.Position.y + nearTo) - p.Position.y : std::numeric_limits<float>::quiet_NaN();
                };
                for (int k = 0; k < 2; ++k)
                {
                    // Where the foot is: the clip's place and what it was moved by a frame ago (held on its spot, slid onto
                    // a tread, drawn in for a short step). The ground was read under the clip's place: on a stair that is
                    // often another tread than the one the foot is held on.
                    const XMVECTOR A0 = at(FootJoint[k]), T0 = ToeJoint[k] >= 0 ? at(ToeJoint[k]) : A0 + XMVectorSet(0, 0, .12f / scale, 0);
                    const float mx = FootSlide[k][0] + FootOff[k][0], mz = FootSlide[k][1] + FootOff[k][1];
                    const float drawn = (StrideScale - 1) * FeetOn * XMVectorGetZ(A0 - at(LegJoint[k]));     // (short steps: drawn in under her hip)
                    const XMVECTOR moved = XMVectorSet((mx * c - mz * sn) / scale, 0, (mx * sn + mz * c) / scale + drawn, 0);
                    const XMVECTOR A = A0 + moved, T = T0 + moved;
                    const float nearTo = std::clamp(FeetRaise[k] + FeetDrop, -Down, Up);     // about where its ground was a frame ago
                    float heel = surface(A, nearTo), mid = surface((A + T) * .5f, nearTo), toe = surface(T, nearTo);
                    if (!std::isfinite(heel)) heel = std::isfinite(mid) ? mid : toe;
                    if (!std::isfinite(toe)) toe = std::isfinite(mid) ? mid : heel;
                    if (!std::isfinite(mid)) mid = (std::max)(heel, toe);
                    if (!std::isfinite(heel)) continue;   // no ground found: as the clip has it
                    // On a step (heel and toes more than 12 cm apart: a stair's edge, a ledge) the whole sole goes onto the
                    // highest ground under it and stays level (owner, 2026-11-17: with her walk on a stair the toes were in the
                    // next step); on a slope she stands on her heel's ground, the foot tipped to the slope.
                    const float top = (std::max)({heel, mid, toe});
                    const bool step = std::abs(toe - heel) > .12f || std::abs(mid - heel) > .12f;
                    float standing = step ? top : heel, swinging = top, air = lifted[k];
                    bool onStair = false;
                    // On a stair, with her ordinary stride (2026-11-19). A foot came down wherever the stride put it: often
                    // with its toes over the next tread, and then the whole sole was lifted level to that tread and stood in
                    // the air over the lower one; and a foot in the air was thrown up 16-19 cm in one frame each time its
                    // toes came over a riser, three times in a stride. Now a foot's way over a stair is laid out:
                    // - Standing, it is on the tread under the middle of its sole, and no part of the sole is in a riser: a
                    //   sole across one is set on onto the upper tread (its heel may hang over the edge) or drawn back clear
                    //   of it, whichever is nearer.
                    // - Pushing off (its heel coming up) it is lifted where it stands, toward the line through the edges of
                    //   the treads before it; let go, it goes up along that line, over each tread before it comes to it.
                    // - Where it will come down is forecast (this foot's swing before took SwingOf and ended with the ball
                    //   LandAhead before her hip, at LandSpeed): the treads beyond that place do not lift it, so it comes
                    //   down onto its tread as it arrives, not after; and it is steered clear of a riser there while still
                    //   in the air. What it was coming down to counts: at an edge it is not chosen anew each frame.
                    // - Going down, its way is kept as a height in the world (SwingAt) and let down evenly over the ground it
                    //   has still to go to the next edge or to where it comes down: no hump, no drop at the edge.
                    // - Down, it stays where it is in the world while her body rises and falls (FeetRaise, below); let go,
                    //   it leaves the tread it stood on (StoodAt), not the one its middle happens to be over.
                    // - In the air or not is by what the foot does, not by the height of its ankle in the clip alone: held
                    //   on its spot it stands; dragged over the ground (her first step out of standing) it is lifted over
                    //   what it is dragged across; low but fast (a jog's reach) it is still on its way.
                    // Its sole along her way: from the foremost to the rearmost of its toes' end (8 cm past the ball) and its
                    // heel's (5 cm behind the ankle). (Along her way, not along the foot: a foot let go hangs toes down
                    // behind its ankle for the first of its swing, and its own length then points behind her.)
                    const float wayZ = Back ? -1.f : 1.f;
                    const XMVECTOR fwd = XMVectorSet(0, 0, wayZ, 0);
                    XMVECTOR along = XMVectorSetY(T - A, 0);
                    along = XMVectorGetX(XMVector3LengthSq(along)) < 1e-8f ? XMVectorSet(0, 0, 1, 0) : XMVector3Normalize(along);
                    const float turned = XMVectorGetZ(along) * wayZ, ankleAt = XMVectorGetZ(A) * wayZ * scale, ballAt = XMVectorGetZ(T) * wayZ * scale;
                    const float front = (std::max)(ballAt + .08f * turned, ankleAt - .05f * turned), rear = (std::min)(ballAt + .08f * turned, ankleAt - .05f * turned);
                    const float half = (std::max)(.02f, (front - rear) * .5f);
                    const XMVECTOR sole = (A + T) * .5f + fwd * (((front + rear) * .5f - (ankleAt + ballAt) * .5f) / scale);
                    const float underNear = tread(sole, nearTo);
                    // (going down a stair a foot's way lies higher over the tread it comes over than a tread is looked for
                    // about its own height: looked for once more, lower. Without, the foot was a frame without its stair.)
                    if (const float under = std::isfinite(underNear) ? underNear : tread(sole, nearTo - .45f); std::isfinite(under))
                    {
                        constexpr float Riser = .12f, LookAhead = 1.2f, Each = .1f, Unsure = .04f, Clear = .05f;
                        const auto ramp = [](float v, float from, float to) { v = std::clamp((v - from) / (to - from), 0.f, 1.f); return v * v * (3 - 2 * v); };
                        const bool held = FootHeld[k];
                        const float reachTo = half + .02f, soon = (reachTo - .01f) * .5f;
                        // how far it has still to go over the ground before it comes down (held: it stays)
                        const bool told = !held && SwingSeconds > 0 && LandSpeed[k] > 0;
                        const float ballAhead = (XMVectorGetZ(T0 - at(LegJoint[k])) + drawn) * scale * wayZ;
                        // Her swing: this foot's own last one (her left and right differ by a thirtieth of a second, which
                        // at a jog is 10 cm of ground), unless her pace has changed since: then the last of either foot.
                        // Its reach before her hip: as learned (at a walk and at a jog it is the same), less under a walk's
                        // speed: stopping, the foot comes down under her, and forecast to go on, it was let down into the
                        // tread before it, 20 cm into the stone.
                        const float swing = std::abs(SwingOf[k] - SwingSeconds) < .15f * SwingSeconds ? SwingOf[k] : SwingSeconds;
                        const float reach = LandAhead[k] * (std::min)(1.f, HerSpeed / (std::min)(LandSpeed[k], 1.5f));
                        // (and no less than it takes a foot to stop from the speed it has: 70 m/s2 is the hardest her clips
                        // bring a foot down; a swing longer than her last one, as she gathers speed, was forecast to have landed)
                        const float toGo = told ? (std::max)({0.f, HerSpeed * (std::max)(0.f, swing - FootFree[k]) + reach - ballAhead, FootGo[k] * FootGo[k] / 140}) : 0.f;
                        // in the air (1) or standing (0)
                        const float low = std::clamp((1 - lifted[k]) * 1.6f, 0.f, 1.f), dragged = ramp(FootGo[k], .25f, .9f);
                        const float settle = low * (std::max)(1 - ramp(FootGo[k], 1.f, 3.f), told ? 1 - ramp(toGo, .05f, .3f) : 0.f);
                        const float want = held ? (std::max)(lifted[k], dragged) : 1 - settle;
                        const float rise = dt / (held && dragged < lifted[k] ? .16f : .07f);
                        FootAir[k] = want > FootAir[k] ? (std::min)(want, FootAir[k] + rise) : (std::max)(want, FootAir[k] - dt / .08f);
                        air = FootAir[k];
                        onStair = true;
                        FootToGo[k] = told ? toGo : -1.f;
                        const float going = ramp(HerSpeed, .15f, .5f);
                        // the stair before the middle of its sole, as she goes: each edge, how far off, the tread before it and the one after
                        struct Edge { float At, From, To; };
                        Edge edges[6]; int count = 0;
                        {
                            const float lookTo = reachTo + (going * air > .01f ? LookAhead : toGo);
                            float before = under;
                            for (float s = Each; s < lookTo + Each && count < 6; s += Each)
                            {
                                const float now = tread(sole + fwd * (s / scale), before);
                                if (!std::isfinite(now)) break;
                                if (std::abs(now - before) > Riser)
                                {
                                    float lo = s - Each, hi = s;
                                    for (int i = 0; i < 5; ++i)
                                    {
                                        const float m = (lo + hi) * .5f;
                                        if (std::abs(tread(sole + fwd * (m / scale), before) - before) > Riser) hi = m; else lo = m;
                                    }
                                    edges[count++] = {hi, before, now};
                                }
                                before = now;
                            }
                        }
                        // and the edge under the rear of its sole, where the tread behind is the higher (she comes down a stair)
                        float backAt = -1, backTop = under;
                        if (const float behind = tread(sole - fwd * (reachTo / scale), under); std::isfinite(behind) && behind > under + Riser)
                        {
                            float lo = 0, hi = reachTo;
                            for (int i = 0; i < 5; ++i)
                            {
                                const float m = (lo + hi) * .5f;
                                if (tread(sole - fwd * (m / scale), under) > under + Riser) hi = m; else lo = m;
                            }
                            backAt = hi; backTop = behind;
                        }
                        // Where its middle comes down (where it is, held), on which tread, and what the sole has still to go to
                        // stand well there. Across a riser up it is set on onto the upper tread if its middle is nearly there
                        // (its heel may hang over the edge), else drawn back clear of the riser. Across an edge down it is
                        // drawn back until the ball of the foot is on the upper tread, or set on until its heel is clear of
                        // that tread, whichever is nearer. And it stands on that tread from the first (on the lower one until
                        // its middle was over the edge, its toes were in the riser).
                        // What it was coming down to counts. In the air, a place just between two treads is not chosen anew
                        // each frame. And come down, it goes on to the tread it was coming down to, though it came down
                        // short or far of where it was forecast: a foot let down to the lower tread, its heel still over
                        // the upper one, was thrown up 11 cm onto the upper one in the frame it landed.
                        const float ballBack = (std::max)(.01f, half - .07f), nearer = (reachTo - ballBack) * .5f;
                        const float meant = LandsAt[k] < 1e8f ? LandsAt[k] - p.Position.y : 1e9f, sway = held ? reachTo : .03f;
                        const auto toward = [&](float upper, float lower) { return std::abs(meant - upper) < Riser ? sway : std::abs(meant - lower) < Riser ? -sway : 0.f; };
                        float fit = 0, lands = under;
                        for (int i = 0; i < count; ++i)
                        {
                            const float g = edges[i].At - toGo;     // the edge before (+) or behind (-) the place its middle comes down
                            if (g >= reachTo) break;
                            if (edges[i].To > edges[i].From)
                            {
                                const float onto = soon + toward(edges[i].To, edges[i].From);
                                if (g > 0) fit = g < onto ? g + .01f : g - reachTo;
                                if (g < onto) lands = edges[i].To;
                            }
                            else if (g <= 0)
                            {
                                const float back = nearer + toward(edges[i].From, edges[i].To);
                                lands = edges[i].To;
                                if (-g < reachTo) { fit = -g < back ? g - ballBack : reachTo + g; if (-g < back) lands = edges[i].From; }
                            }
                            if (g > 0) break;
                        }
                        if (const float past = backAt + toGo; backAt >= 0 && fit == 0 && past < reachTo)
                        {
                            const float back = nearer + toward(backTop, under);
                            fit = past < back ? -(past + ballBack) : reachTo - past;
                            if (past < back) lands = backTop;
                        }
                        LandsAt[k] = held && FootStood[k] > .15f ? 1e9f : p.Position.y + lands;
                        const float ux = XMVectorGetX(fwd), uz = XMVectorGetZ(fwd), steer = held ? 1.f : told ? 1 - ramp(toGo, .1f, .5f) : low;
                        fitTo[k][0] = (ux * c + uz * sn) * fit; fitTo[k][1] = (-ux * sn + uz * c) * fit;
                        for (int i = 0; i < 2; ++i) slideTo[k][i] = std::clamp((FootSlide[k][i] + fitTo[k][i]) * steer, -.2f, .2f);
                        // In the air it is over the highest tread under its sole, from end to end,
                        float over = under;
                        for (const float s : {-half, half}) if (const float t = tread(sole + fwd * (s / scale), under); std::isfinite(t)) over = (std::max)(over, t);
                        swinging = over;
                        if (going * air > .01f)
                        {
                            // and going up, on the line through the edges of the risers before it: as steep as the steepest
                            // pair of them (one riser alone: 45 degrees).
                            float found[4][2] = {}; int rises = 0;
                            for (int i = 0; i < count && rises < 4; ++i)
                                if (edges[i].To > edges[i].From) { found[rises][0] = edges[i].At; found[rises][1] = edges[i].To; ++rises; }
                            float steep = 1.f;
                            for (int i = 0; i + 1 < rises; ++i)
                            {
                                const float between = (found[i + 1][1] - found[i][1]) / (std::max)(.05f, found[i + 1][0] - found[i][0]);
                                steep = i ? (std::max)(steep, between) : between;
                            }
                            steep = std::clamp(steep, .3f, 1.5f);
                            // A tread counts as far as it is sure the middle of the sole will come onto it (or within 'soon'
                            // of it: then it is set on onto it); one the foot stops short of is not climbed. Held, it has
                            // come down where it is; and pushing off, a little later, they all count. (Come down, it was
                            // taken for pushing off at once, and for the frames it came to rest in it was lifted toward
                            // the next tread: 7 to 10 cm up and down again after every landing.)
                            const float pushing = held ? ramp(FootStood[k], .05f, .12f) : 1.f;
                            float line = under;
                            for (int i = 0; i < rises; ++i)
                            {
                                const float here = ramp(toGo - found[i][0] + soon, -Unsure, Unsure), sure = told ? here : here + (1 - here) * pushing;
                                line = (std::max)(line, under + (found[i][1] - under) * sure - steep * (std::max)(0.f, found[i][0] - half) / (std::max)(sure, .08f));
                            }
                            // Its way comes down (a stair going down; a tread it will not reach after all): it is not dropped
                            // by a tread's height as its sole comes clear of an edge, nor left up until it has stopped: it is
                            // let down evenly over the ground it has still to go to the next thing it must be down for: the
                            // far edge of the tread it is over, or the place it comes down on, whichever is first. (Kept as
                            // a height in the world: she goes down herself meanwhile.)
                            const float wayAt = p.Position.y + (std::max)(over, under + (line - under) * going);
                            if (!(std::abs(SwingAt[k] - wayAt) < 1.f) || wayAt >= SwingAt[k]) SwingAt[k] = wayAt;
                            else
                            {
                                float room = told ? (std::max)(0.f, toGo + fit * steer) : LookAhead;
                                for (int i = 0; i < count; ++i) if (edges[i].To < edges[i].From) { room = (std::min)(room, edges[i].At + half); break; }
                                const float gone = FootGo[k] * dt, drop = SwingAt[k] - wayAt;
                                SwingAt[k] -= (std::min)(drop, (std::max)(1.5f * dt, drop * gone / (room + gone + 1e-4f)));
                            }
                            // (and Clear over all of it: the clip's toes pass a few centimetres over a floor, and so over an edge)
                            swinging = SwingAt[k] - p.Position.y + Clear * going;
                        }
                        else SwingAt[k] = p.Position.y + over;
                        // Let go, it leaves the tread it stood on (that one, kept as a height in the world: not the one its
                        // middle is over, which for a foot that stood across an edge is another, and changes as it goes:
                        // 13 cm down and up again in the two frames after it was let go). A tenth of a second later it is
                        // in the air, and what it stands on is where it comes down: the tread forecast, and never lower
                        // than the tread under it now.
                        const float onto = (std::max)(lands, under);
                        if (held) StoodAt[k] = p.Position.y + onto;
                        const float from = std::abs(StoodAt[k] - p.Position.y - under) < 1.f ? StoodAt[k] - p.Position.y : under;
                        standing = held || FootFree[k] > .1f ? onto : from;
                    }
                    if (!onStair) { FootAir[k] = lifted[k]; SwingAt[k] = LandsAt[k] = StoodAt[k] = 1e9f; }
                    FootLifted[k] = lifted[k]; FootOnStair[k] = onStair;
                    h[k] = std::clamp(standing * (1 - air) + swinging * air - leanLift(A), -Down, Up) * FeetOn;
                    const float reach = std::hypot(XMVectorGetX(T - A), XMVectorGetZ(T - A)) * scale;
                    if (reach > .03f && !step) pitchTo[k] = std::clamp(std::atan2(toe - heel, reach), -MostPitch, MostPitch) * (1 - lifted[k]) * FeetOn;
                }
                // her body goes down to her lower foot's ground, and up to it where the drawn ground lies over the ground she
                // is held on (2026-11-19: there she stood with her knees bent, which at ease, her legs straight, looked a crouch)
                body = std::clamp((std::min)(h[0], h[1]), -Down, .3f);
                for (int k = 0; k < 2; ++k) raiseTo[k] = h[k];
            }
            else if (stairClip && FeetOn > .01f)
                for (int k = 0; k < 2; ++k)
                {   // the stair clip stands on the treads by itself; a foot is only lifted out of a tread it would be in (a deep
                    // tread, or the moment she comes onto the stair from a run, the clips blended)
                    if (AnkleRest[k] > 1e8f) continue;
                    const XMVECTOR A = at(FootJoint[k]), T = ToeJoint[k] >= 0 ? at(ToeJoint[k]) : A;
                    float g = -1e9f;
                    for (const XMVECTOR v : {A, (A + T) * .5f}) if (const float s = surface(v); std::isfinite(s)) g = (std::max)(g, s);
                    if (g > -1e8f) raiseTo[k] = std::clamp(g + AnkleRest[k] - XMVectorGetY(A) * scale, 0.f, Up) * FeetOn;
                }
            // eased, but quickly where the ground comes up under a foot or her body must come down (else a foot is in the hill a moment)
            FeetDrop += (body - FeetDrop) * (std::min)(1.f, dt * (body < FeetDrop ? BodyFall : BodyEase));
            if (stairClip && stair && FeetOn > .01f)
                for (int k = 0; k < 2; ++k)
                {   // standing on a tread (by the clip's phase): slid along the stair onto its middle
                    const float from = NyxaraClips::StairsStance[k][0], length = NyxaraClips::StairsStance[k][1] - from;
                    const float x = Phase - from - std::floor(Phase - from);
                    if (x > length || AnkleRest[k] > 1e8f) continue;
                    const auto edge = [](float v) { v = std::clamp(v / .06f, 0.f, 1.f); return v * v * (3 - 2 * v); };
                    const float standing = edge(x) * edge(length - x);
                    const XMVECTOR A = at(FootJoint[k]), T = ToeJoint[k] >= 0 ? at(ToeJoint[k]) : A, mid = (A + T) * .5f;
                    const float ankleY = p.Position.y + XMVectorGetY(A) * scale;
                    const float top = .15f + NyxaraClips::StairsRise * std::round((ankleY - AnkleRest[k] - .15f) / NyxaraClips::StairsRise);
                    float cx, cz, ax, az;
                    if (!stair(worldX(mid), worldZ(mid), top, cx, cz, ax, az)) continue;
                    const float d = std::clamp((cx - worldX(mid)) * ax + (cz - worldZ(mid)) * az, -.2f, .2f) * standing * FeetOn;
                    slideTo[k][0] = ax * d; slideTo[k][1] = az * d;
                }
            bool moved = false;
            const float ease = (std::min)(1.f, dt * FootEase), slideEase = (std::min)(1.f, dt * SlideEase);
            // how much her body went up in the world since the frame before: she herself, and what she is let down by
            const float bodyAt = p.Position.y + FeetDrop, bodyRose = std::abs(bodyAt - BodyWas) < .3f ? bodyAt - BodyWas : 0.f;
            BodyWas = bodyAt;
            for (int k = 0; k < 2; ++k)
            {
                const float raiseWant = raiseTo[k] - FeetDrop;
                if (FootHeld[k])
                {   // Held, it stays where it is in the world while her body rises and falls, and is eased only to what its
                    // ground is found to be otherwise (2026-11-19). It was eased after its ground in her own frame: going
                    // up a stair, the tread sinking under her at 1 to 2 m/s, it hung 5 to 20 cm over the tread for the
                    // first of every stance, and uphill and downhill the same in little.
                    FeetRaise[k] -= bodyRose;
                    FeetRaise[k] += (raiseWant - FeetRaise[k]) * (raiseWant > FeetRaise[k] ? (std::min)(1.f, dt * FootRise) : ease);
                }
                else if (FootOnStair[k])
                {   // In the air over a stair its way is laid out, and smooth: it goes as its way goes, and only what it
                    // is off it (coming off its tread) is eased away. (Eased after its way, it went 8 cm under it going
                    // up at a walk, and 19 cm over it going down.)
                    FeetRaise[k] += std::clamp(raiseWant - RaiseWant[k], -8 * dt, 8 * dt);
                    FeetRaise[k] += (raiseWant - FeetRaise[k]) * (std::min)(1.f, dt * FootRise);
                }
                else FeetRaise[k] += (raiseWant - FeetRaise[k]) * (raiseWant > FeetRaise[k] ? (std::min)(1.f, dt * FootRise) : ease);
                RaiseWant[k] = raiseWant;
                FootPitch[k] += (pitchTo[k] - FootPitch[k]) * ease;
                for (int i = 0; i < 2; ++i) FootSlide[k][i] += (slideTo[k][i] - FootSlide[k][i]) * slideEase;
                // in her own space: up, and the slide turned from the world into her axes
                const float raise = (FeetRaise[k] + Squat + StyleLow * FeetOn) / scale;      // (her knees taking a landing, or bent in her way of going: her body is let down, her feet stay)
                const float sx = (FootSlide[k][0] * c - FootSlide[k][1] * sn) / scale, sz = (FootSlide[k][0] * sn + FootSlide[k][1] * c) / scale;
                const float drawnIn = (StrideScale - 1) * FeetOn;     // short steps: each foot drawn in under its hip, along her way
                // A foot that is down is held where it came down (2026-11-19). Her clips carry a standing foot back under
                // her at their own pace, the world carries her forward at hers, and between two clips a foot is somewhere
                // between two places: so a foot on the ground slid, a little in a steady stride (4-9 % of her speed), a
                // lot through a start, a stop and a turn on the spot. Now, from its ankle coming down until the ball of the
                // foot lifts (she pushes off from the ball with her heel up: letting go as the heel rose let the ball slide
                // for the rest of the step), the ball stays on the spot of the world it came down on, and her leg is bent
                // to it; lifted, it is let go and is back in the clip's place within a tenth of a second. Held no further
                // than HoldMost from the clip's place: past that it is dragged along (something else than her legs moves her).
                float lx = 0, lz = 0;
                {
                    constexpr float HoldMost = .14f;
                    const XMVECTOR A0 = at(FootJoint[k]), H0 = at(LegJoint[k]);
                    const XMVECTOR placed = (ToeJoint[k] >= 0 ? at(ToeJoint[k]) : A0) + XMVectorSet(sx, 0, sz + drawnIn * XMVectorGetZ(A0 - H0), 0);
                    const float wx = worldX(placed), wz = worldZ(placed);
                    // (Latched arriving, not passing low: in her walk the left ankle dips to 4 cm over the ground late in
                    // its swing, the foot going at three times her speed, and latched there it hung back for four frames of
                    // every stride. A foot coming down goes over the ground no faster than she does.)
                    const bool heldBefore = FootHeld[k], arriving = heldBefore || FootGo[k] < 1.f + HerSpeed;
                    if (on && !stairClip && ((lifted[k] < .3f && arriving) || (heldBefore && ballDown[k])) && FeetOn > .9f)
                    {
                        if (!FootHeld[k])
                        {
                            FootHeld[k] = true; FootAt[k][0] = wx + FootOff[k][0]; FootAt[k][1] = wz + FootOff[k][1];
                            if (FootFree[k] > .15f && FootFree[k] < 1.f && Speed > .5f)
                            {   // a swing ended: how long it took, where before her hip its ball came down, and how fast she went
                                SwingSeconds = SwingOf[k] = FootFree[k];
                                LandAhead[k] = (XMVectorGetZ((ToeJoint[k] >= 0 ? at(ToeJoint[k]) : A0) - H0) + drawnIn * XMVectorGetZ(A0 - H0)) * scale * (Back ? -1.f : 1.f);
                                LandSpeed[k] = (std::max)(.5f, HerSpeed);
                                // (her other foot, if it has not landed from a stride yet: as this one)
                                if (LandSpeed[1 - k] <= 0) { SwingOf[1 - k] = SwingOf[k]; LandAhead[1 - k] = LandAhead[k]; LandSpeed[1 - k] = LandSpeed[k]; }
                            }
                        }
                        else for (int i = 0; i < 2; ++i) FootAt[k][i] += fitTo[k][i] * (std::min)(1.f, dt * 14);   // (on a stair: the rest of the way clear of the risers)
                        float ox = FootAt[k][0] - wx, oz = FootAt[k][1] - wz;
                        const float apart = std::hypot(ox, oz);
                        if (apart > .6f) ox = oz = 0;      // (she was put somewhere else)
                        else if (apart > HoldMost) { ox *= HoldMost / apart; oz *= HoldMost / apart; }
                        FootAt[k][0] = wx + ox; FootAt[k][1] = wz + oz;
                        FootOff[k][0] = ox; FootOff[k][1] = oz;
                    }
                    else
                    {
                        FootHeld[k] = false;
                        const float keep = std::exp(-dt * 16);
                        FootOff[k][0] *= keep; FootOff[k][1] *= keep;
                    }
                    lx = (FootOff[k][0] * c - FootOff[k][1] * sn) / scale; lz = (FootOff[k][0] * sn + FootOff[k][1] * c) / scale;
                    const float nowX = wx + FootOff[k][0], nowZ = wz + FootOff[k][1];
                    // (come down in this frame it is at rest: the speed it came with was taken, a frame later, for a held foot dragged)
                    FootGo[k] = FootHeld[k] && !heldBefore ? 0.f : FootWasKnown[k] && dt > 1e-5f ? std::hypot(nowX - FootWas[k][0], nowZ - FootWas[k][1]) / dt : 0.f;
                    FootWas[k][0] = nowX; FootWas[k][1] = nowZ; FootWasKnown[k] = true;
                    FootFree[k] = FootHeld[k] ? 0.f : (std::min)(9.f, FootFree[k] + dt);
                    FootStood[k] = FootHeld[k] ? (std::min)(9.f, FootStood[k] + dt) : 0.f;
                }
                if (std::abs(raise) < 1e-3f && std::abs(sx) + std::abs(sz) < 1e-3f && std::abs(FootPitch[k]) < 1e-3f && std::abs(drawnIn) < 1e-3f &&
                    std::abs(lx) + std::abs(lz) < 1e-3f) continue;
                const int hip = LegJoint[k], knee = LegJoint[k + 2], foot = FootJoint[k], above = Skeleton.Joints[hip].ParentIndex;
                if (above < 0 || Skeleton.Joints[knee].ParentIndex != hip || Skeleton.Joints[foot].ParentIndex != knee) continue;
                const XMVECTOR H = at(hip), K = at(knee), A = at(foot);
                const float a = XMVectorGetX(XMVector3Length(K - H)), b = XMVectorGetX(XMVector3Length(A - K));
                if (a < 1e-3f || b < 1e-3f) continue;
                const XMVECTOR goal = A + XMVectorSet(sx + lx, raise, sz + lz + drawnIn * XMVectorGetZ(A - H), 0);
                const XMVECTOR way = XMVector3Normalize(goal - H);
                const float d = std::clamp(XMVectorGetX(XMVector3Length(goal - H)), std::abs(a - b) + 1e-3f, (a + b) * .999f);
                const auto turnOf = [&](int j) { XMVECTOR s, q, t; XMMatrixDecompose(&s, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[j])); return XMQuaternionNormalize(q); };
                const XMVECTOR qAbove = turnOf(above), qHip = turnOf(hip), qKnee = turnOf(knee);
                // Which way the knee bends (owner, 2026-11-17: a nearly straight leg's knee pointed anywhere, often sideways;
                // 2026-11-18: "bending his knees backward while walking", on stairs too; 2026-11-19: "it adds some odd
                // animation for each step", "each leg").
                // The knee is a hinge, and the clip says where its axis lies: across its thigh and its shin where the knee
                // is bent; and where the leg is straight, and they say nothing, where the thigh and the shin carry the axis
                // they had while it was bent (kept in each bone's own frame). That axis is carried over to the leg as it
                // is laid now, by the shortest turn from the clip's hip-to-ankle line to the new one, and the knee bends
                // about it: so a foot moved does to the knee what moving it would, and nothing else.
                // (Before: first the knee was turned toward the toes laid flat, and a foot lifting off, toes down, sent
                // it behind her. Then it was the clip's own knee where that pointed forward and her facing where it did
                // not: but the clip's knee as seen from the NEW hip-to-ankle line. With the foot far from the clip's place
                // - 20 cm on a slope, 45 on a stair - that is no direction at all, and it changed side as the lines
                // crossed: in every step of each leg the knee swung 15 cm across her body as the leg came up and snapped
                // back in one frame, and was thrown 20 cm outward in the frame after the foot came down. 1700 m/s2 at the
                // knee, twice a step, on every slope and stair; five times the worst of anything else in her.)
                const auto across = [](XMVECTOR v, XMVECTOR line) { v = v - line * XMVectorGetX(XMVector3Dot(v, line)); return XMVectorGetX(XMVector3LengthSq(v)) < 1e-8f ? XMVectorZero() : XMVector3Normalize(v); };
                const XMVECTOR way0 = XMVector3Normalize(A - H), crossed = XMVector3Cross(K - H, A - K);
                const float bent = XMVectorGetX(XMVector3Length(crossed)) / (a * b);      // the sine of the knee's angle in the clip
                const XMVECTOR seen = bent > 1e-4f ? crossed / (a * b * bent) : XMVectorZero();
                if (bent > .4f)
                {   // well bent: where the axis lies in the thigh's own frame, and in the shin's
                    const float w = (std::min)(1.f, dt * 4);
                    XMStoreFloat3(&KneeAxis[k][0], XMVectorLerp(XMLoadFloat3(&KneeAxis[k][0]), XMVector3Rotate(seen, XMQuaternionInverse(qHip)), w));
                    XMStoreFloat3(&KneeAxis[k][1], XMVectorLerp(XMLoadFloat3(&KneeAxis[k][1]), XMVector3Rotate(seen, XMQuaternionInverse(qKnee)), w));
                }
                XMVECTOR carried = XMVector3Rotate(XMLoadFloat3(&KneeAxis[k][0]), qHip) + XMVector3Rotate(XMLoadFloat3(&KneeAxis[k][1]), qKnee);
                const bool known = XMVectorGetX(XMVector3LengthSq(carried)) > 1.f;        // (both learned and agreeing: 4)
                carried = known ? XMVector3Normalize(carried) : XMVectorZero();
                const float sure = known ? std::clamp((bent - .1f) / .2f, 0.f, 1.f) : bent > .03f ? 1.f : 0.f, seenFor = sure * sure * (3 - 2 * sure);
                XMVECTOR axis = seen * seenFor + carried * (1 - seenFor);
                // (no axis at all - a straight leg before she has taken a step - : that of a knee that bends ahead of her)
                // The fore side of a leg's line: ahead of her for a leg that hangs straight down, and turned with the line
                // as it leans: under a leg that reaches back, over one that reaches forward. (It was her facing laid across
                // the line. For a foot behind her and higher than her hip - the foot she leaves a stair's tread with, going
                // down at a jog, her body let down - that points up: the knee went over her hip, and flipped under it as
                // the foot came down past the hip's height. 3000 to 4000 m/s2 at the knee, in every step down.)
                const auto fore = [](XMVECTOR line) { return XMVector3Rotate(XMVectorSet(0, 0, 1, 0), ArcTurn(XMVectorSet(0, -1, 0, 0), line)); };
                if (XMVectorGetX(XMVector3LengthSq(axis)) < .25f) axis = XMVector3Cross(fore(way0), way0);
                if (XMVectorGetX(XMVector3LengthSq(axis)) < .25f) continue;
                XMVECTOR bend = across(XMVector3Cross(way, XMVector3Rotate(XMVector3Normalize(axis), ArcTurn(way0, way))), way);
                // (and never on the back side of its leg, whatever a clip and a far reach make of it)
                {
                    const XMVECTOR aheadKnee = fore(way);
                    const float trust = XMVectorGetX(XMVector3LengthSq(bend)) < .5f ? 0.f : std::clamp((XMVectorGetX(XMVector3Dot(bend, aheadKnee)) + .15f) / .3f, 0.f, 1.f);
                    bend = bend * trust + aheadKnee * (1 - trust);
                }
                if (XMVectorGetX(XMVector3LengthSq(bend)) < 1e-6f) continue;
                bend = XMVector3Normalize(bend);
                const float cosA = std::clamp((a * a + d * d - b * b) / (2 * a * d), -1.f, 1.f);
                const XMVECTOR K2 = H + way * (a * cosA) + bend * (a * std::sqrt(1 - cosA * cosA)), A2 = H + way * d;
                const XMVECTOR turnThigh = ArcTurn(XMVector3Normalize(K - H), XMVector3Normalize(K2 - H));
                const XMVECTOR shin = XMVector3Rotate(A - K, turnThigh);
                const XMVECTOR turnShin = ArcTurn(XMVector3Normalize(shin), XMVector3Normalize(A2 - K2));
                // the foot as the clip turns it, then tipped about her side axis to the slope (toes up a hill)
                const XMVECTOR qFoot = XMQuaternionMultiply(turnOf(foot), XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), -FootPitch[k]));
                const XMVECTOR newHip = XMQuaternionMultiply(qHip, turnThigh), newKnee = XMQuaternionMultiply(XMQuaternionMultiply(qKnee, turnThigh), turnShin);
                XMStoreFloat4(&out.LocalJoints[hip].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(newHip, XMQuaternionInverse(qAbove))));
                XMStoreFloat4(&out.LocalJoints[knee].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(newKnee, XMQuaternionInverse(newHip))));
                XMStoreFloat4(&out.LocalJoints[foot].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(qFoot, XMQuaternionInverse(newKnee))));
                moved = true;
            }
            if (moved && !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr)) return 0;
            for (int k = 0; k < 2; ++k)
            {   // for the log: how far each sole is over the ground under the middle of its foot, as she is drawn
                const XMVECTOR A = at(FootJoint[k]), mid = ToeJoint[k] >= 0 ? (A + at(ToeJoint[k])) * .5f : A;
                const float g = surface(mid);
                SoleGap[k] = AnkleRest[k] > 1e8f || !std::isfinite(g) ? std::numeric_limits<float>::quiet_NaN() : XMVectorGetY(A) * scale + FeetDrop - Squat - StyleLow * FeetOn - AnkleRest[k] - g;
            }
            return FeetDrop - Squat - StyleLow * FeetOn;
        }
        // Which way the sword bones' axes came through the importer is found: of the eight mirrorings, the one that in
        // Idle puts the handle in her fist and the blade's point up.
        void FindSwords()
        {
            int knuckle[2] = {-1, -1};
            BodyJoints.clear();
            std::fill(std::begin(LegJoint), std::end(LegJoint), -1);
            for (size_t i = 0; i < Skeleton.Joints.size(); ++i)
            {
                const auto& n = Skeleton.Joints[i].Name;
                const char* const legs[4] = {"LeftUpLeg", "RightUpLeg", "LeftLeg", "RightLeg"};
                for (int k = 0; k < 4; ++k)
                    if (n.ends_with(legs[k]) && LegJoint[k] < 0) LegJoint[k] = static_cast<int>(i);
                for (int k = 0; k < 2; ++k)
                {
                    if (n.ends_with(k ? "RightFoot" : "LeftFoot") && FootJoint[k] < 0) FootJoint[k] = static_cast<int>(i);
                    if (n.ends_with(k ? "RightToeBase" : "LeftToeBase") && ToeJoint[k] < 0) ToeJoint[k] = static_cast<int>(i);
                    const char* const arms[4] = {"Shoulder", "Arm", "ForeArm", "Hand"};
                    if (n.ends_with(k ? "RightHandIndex1" : "LeftHandIndex1") && Knuckle[k][0] < 0) Knuckle[k][0] = static_cast<int>(i);
                    if (n.ends_with(k ? "RightHandRing1" : "LeftHandRing1") && Knuckle[k][1] < 0) Knuckle[k][1] = static_cast<int>(i);
                    for (int w = 0; w < 4; ++w)
                        if (n.ends_with(std::string(k ? "Right" : "Left") + arms[w]) && ArmChain[k][w] < 0) ArmChain[k][w] = static_cast<int>(i);
                    for (int w = 0; w < 3; ++w)
                        if (n.ends_with(std::string(k ? "WingR" : "WingL") + static_cast<char>('1' + w)) && WingJoint[k][w] < 0) WingJoint[k][w] = static_cast<int>(i);
                }
                if (n.find("Wing") == std::string::npos && n.find("Sword") == std::string::npos) BodyJoints.push_back(static_cast<int>(i));
                ArmJoint.resize(Skeleton.Joints.size(), 0);
                ArmJoint[i] = n.find("Wing") == std::string::npos && n.find("Sword") == std::string::npos &&
                              (n.find("Left") != std::string::npos || n.find("Right") != std::string::npos) &&
                              (n.find("Shoulder") != std::string::npos || n.find("Arm") != std::string::npos || n.find("Hand") != std::string::npos);
                if (n.ends_with("Neck") && NeckJoint < 0) NeckJoint = static_cast<int>(i);
                if (n.ends_with("Head") && HeadJoint < 0) HeadJoint = static_cast<int>(i);
                if (n.ends_with("SwordLeft")) SwordJoint[0] = static_cast<int>(i);
                if (n.ends_with("SwordRight")) SwordJoint[1] = static_cast<int>(i);
            }
            if (WingJoint[0][0] >= 0 && WingJoint[1][0] >= 0)
            {   // on which side of her each wing is, as she was made
                const auto made = [&](int j) { return XMMatrixInverse(nullptr, XMLoadFloat4x4(&Skeleton.Joints[j].InverseBindPose)).r[3]; };
                WingRightIs = XMVectorGetX(made(WingJoint[1][0]) - made(WingJoint[0][0])) >= 0 ? 1.f : -1.f;
            }
            for (int k = 0; k < 2 && SwordJoint[k] >= 0; ++k)   // a finger of that hand: the first other child of the hand
                for (size_t i = 0; i < Skeleton.Joints.size(); ++i)
                    if (static_cast<int>(i) != SwordJoint[k] && Skeleton.Joints[i].ParentIndex == Skeleton.Joints[SwordJoint[k]].ParentIndex)
                    {
                        knuckle[k] = static_cast<int>(i);
                        break;
                    }
            DoorwaysSkeletonPose idle;
            std::string error;
            if (SwordJoint[0] < 0 || SwordJoint[1] < 0 || knuckle[0] < 0 || knuckle[1] < 0 || !Sample("Idle", 0, idle, &error) ||
                !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, idle, &error))
            {
                SwordJoint[0] = SwordJoint[1] = -1;
                return;
            }
            float best = 1e9f;
            for (int f = 0; f < 8; ++f)
            {
                const XMVECTOR sign = XMVectorSet(f & 1 ? -1.f : 1.f, f & 2 ? -1.f : 1.f, f & 4 ? -1.f : 1.f, 0);
                float score = 0;
                XMFLOAT3 g[2], t[2];
                for (int k = 0; k < 2; ++k)
                {
                    XMStoreFloat3(&g[k], XMVectorSet(NyxaraClips::SwordGrip[k][0], NyxaraClips::SwordGrip[k][1], NyxaraClips::SwordGrip[k][2], 0) * sign);
                    XMStoreFloat3(&t[k], XMVectorSet(NyxaraClips::SwordTip[k][0], NyxaraClips::SwordTip[k][1], NyxaraClips::SwordTip[k][2], 0) * sign);
                    const XMMATRIX m = XMLoadFloat4x4(&idle.ModelSpaceMatrices[SwordJoint[k]]);
                    const XMVECTOR grip = XMVector3TransformCoord(XMLoadFloat3(&g[k]), m), tip = XMVector3TransformCoord(XMLoadFloat3(&t[k]), m);
                    const XMVECTOR fist = XMLoadFloat4x4(&idle.ModelSpaceMatrices[knuckle[k]]).r[3];
                    score += XMVectorGetX(XMVector3Length(grip - fist)) - .25f * (XMVectorGetY(tip) - XMVectorGetY(grip));
                }
                if (score < best)
                {
                    best = score;
                    for (int k = 0; k < 2; ++k) { SwordGrip[k] = g[k]; SwordTip[k] = t[k]; }
                }
            }
        }
        // Her arms as she stands and goes (2026-11-19; owner: "i rather if she has iddle hands while moving and follow the
        // movement little bit"). The clips' arms were set frame by frame when they were built: the upper arm swung as a
        // rod on her chest and the elbow was held at one angle, 22 degrees in the walk and 78 in the run, so that in a
        // run her forearms stuck out before her like a tray's bearer's.
        // At ease (Stand, and Move with her swords on her back) her arms are the ones she stands with, and move as arms
        // left alone do:
        // - they hang by their own weight, down in the world and not down her chest: her chest leans into a run and
        //   turns with every step, her arms do not;
        // - each swings a little against the leg of its side, by how far her feet are apart (so it is in step at any
        //   pace, backwards, and turning on the spot), wider and a little behind her as she goes faster;
        // - the forearm is not held: it follows the upper arm late, as a thing hung from it does, so the elbow opens
        //   as the arm goes back and bends as it comes forward, more the faster she goes; an elbow does not bend back;
        //   the hand is loose after the forearm;
        // - gathering speed her arms are left behind a little, and stopping they swing on, and settle.
        // In a fight (Armed: Move, Jump and Fall with her swords in her hands) her arms are her guard's (Idle), carried by
        // her chest, and give with her stride only a little: the swords are held, not swung.
        // In and out at once: this is part of the pose she changes to, and what she was doing is carried on by Compose
        // as at any change. Her speed is taken eased: it reads nothing for a frame as a key is let go, and her arms
        // twitched at every stop (400 to 800 m/s2 at her hands).
        void EaseArms(const std::string& name, float time, float dt, DoorwaysSkeletonPose& out)
        {
            const bool armed = Armed && (name == "Move" || name == "Jump" || name == "Fall") && Duration("Idle") > 0;
            const bool easy = armed || ((name == "Move" || name == "Stand") && Duration("Stand") > 0);
            const size_t n = out.LocalJoints.size();
            bool whole = ArmJoint.size() == n && FootJoint[0] >= 0 && FootJoint[1] >= 0 && LegJoint[0] >= 0 && LegJoint[2] >= 0;
            for (int s = 0; s < 2 && whole; ++s)
            {
                for (int w = 0; w < 4; ++w) whole = whole && ArmChain[s][w] >= 0;
                whole = whole && Skeleton.Joints[ArmChain[s][2]].ParentIndex == ArmChain[s][1] && Skeleton.Joints[ArmChain[s][3]].ParentIndex == ArmChain[s][2] &&
                        Skeleton.Joints[ArmChain[s][1]].ParentIndex >= 0;
            }
            const bool was = ArmsEasy > 0;
            ArmsEasy = 0;
            const float pace = name == "Move" ? (std::max)(Speed, std::hypot(Glide.x, Glide.y)) : 0.f;
            ArmPace = was ? ArmPace + (pace - ArmPace) * (std::min)(1.f, dt * 12) : pace;
            if (!easy || !whole) return;
            const auto place = [](const DoorwaysSkeletonPose& p, int j) { const auto& m = p.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 0); };
            if (!ArmRestKnown)
            {   // which way her arms hang as she stands
                DoorwaysSkeletonPose rest;
                if (!Sample("Stand", 0, rest, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, rest, nullptr)) return;
                for (int s = 0; s < 2; ++s)
                {
                    XMStoreFloat3(&ArmRest[s][0], XMVector3Normalize(place(rest, ArmChain[s][2]) - place(rest, ArmChain[s][1])));
                    XMStoreFloat3(&ArmRest[s][1], XMVector3Normalize(place(rest, ArmChain[s][3]) - place(rest, ArmChain[s][2])));
                }
                ArmRestKnown = true;
            }
            const std::string source = armed ? "Idle" : "Stand";
            if (!Sample(source, name == source ? time : std::fmod(Clock, Duration(source)), EasePose, nullptr) || EasePose.LocalJoints.size() != n) return;
            ArmClip.resize(n);
            for (size_t i = 0; i < n; ++i)
                if (ArmJoint[i]) { ArmClip[i] = out.LocalJoints[i].Rotation; out.LocalJoints[i].Rotation = EasePose.LocalJoints[i].Rotation; }
            if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, nullptr))
            {
                for (size_t i = 0; i < n; ++i) if (ArmJoint[i]) out.LocalJoints[i].Rotation = ArmClip[i];
                return;
            }
            if (armed)      // her swords as her fists hold them (the stride's clips have them on her back)
                for (int k = 0; k < 2; ++k)
                    if (SwordJoint[k] >= 0) out.LocalJoints[SwordJoint[k]] = EasePose.LocalJoints[SwordJoint[k]];
            ArmsEasy = 1;
            const auto at = [&](int j) { return place(out, j); };
            const auto turnOf = [&](int j) { XMVECTOR sc, q, t; XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&out.ModelSpaceMatrices[j])); return XMQuaternionNormalize(q); };
            const auto ramp = [](float v, float from, float to) { v = std::clamp((v - from) / (to - from), 0.f, 1.f); return v * v * (3 - 2 * v); };
            // her stride: how far her left foot is before her right, against her widest of late (no less than a short step)
            const float leg = XMVectorGetX(XMVector3Length(at(LegJoint[2]) - at(LegJoint[0]))) + XMVectorGetX(XMVector3Length(at(FootJoint[0]) - at(LegJoint[2])));
            const float lead = XMVectorGetZ(at(FootJoint[0]) - at(FootJoint[1]));
            LeadMost = (std::max)(std::abs(lead), LeadMost * std::exp(-dt * 1.5f));
            const float moving = ramp(ArmPace, .15f, .9f);
            const float stride = std::clamp(lead / (std::max)(LeadMost, .35f * leg), -1.f, 1.f) * moving;
            // (and as her way of going has them, GaitStyle, unless they hold her swords: swung more or less, bent, carried
            // back, held out)
            const float styleArms = armed || !StyleNow ? 0.f : StyleShare;
            const float swing = XMConvertToRadians(armed ? 4 + 4 * GaitRun : 13 + 6 * GaitRun) * (1 + (styleArms > 0 ? StyleNow->ArmSwing - 1 : 0.f) * styleArms);
            const float bent = armed ? 0.f : XMConvertToRadians(3 + 8 * GaitRun) * moving + (styleArms > 0 ? XMConvertToRadians(StyleNow->ArmBend) * styleArms : 0.f);
            const float armOut = styleArms > 0 ? XMConvertToRadians(StyleNow->ArmOut) * styleArms : 0.f;
            if (!was) ArmLag = 0;
            ArmLag += (std::clamp(-Gain * .012f, -.14f, .14f) - ArmLag) * (std::min)(1.f, dt * 6);
            const float behind = (armed ? ArmLag * .5f : ArmLag - XMConvertToRadians(2 + 7 * GaitRun) * moving) -
                                 (styleArms > 0 ? XMConvertToRadians(StyleNow->ArmBack) * styleArms : 0.f);
            for (int s = 0; s < 2; ++s)
            {
                const int ua = ArmChain[s][1], fa = ArmChain[s][2], ha = ArmChain[s][3], above = Skeleton.Joints[ua].ParentIndex;
                const XMVECTOR d1 = XMVector3Normalize(at(fa) - at(ua)), d2 = XMVector3Normalize(at(ha) - at(fa));
                XMFLOAT3 r1 = ArmRest[s][0], r2 = ArmRest[s][1];
                if (armed) { XMStoreFloat3(&r1, d1); XMStoreFloat3(&r2, d2); }      // (her guard as her chest carries it now)
                const float tilt1 = std::atan2(r1.z, -r1.y), tilt2 = std::atan2(r2.z, -r2.y), across1 = std::hypot(r1.y, r1.z), across2 = std::hypot(r2.y, r2.z);
                // the upper arm: forward as the foot of its side is back
                const float arm = behind + (s == 0 ? -1.f : 1.f) * swing * stride;
                // the forearm: after it, late (a weight on a spring, a little short of damped: it swings on once)
                const float to = arm + bent;
                if (!was) { ForeAt[s] = to; ForeGo[s] = 0; }
                constexpr float Quick = 14.f, Damp = .6f;
                for (float left = dt; left > 1e-6f;)
                {
                    const float h = (std::min)(left, 1 / 120.f); left -= h;
                    ForeGo[s] += (Quick * Quick * (to - ForeAt[s]) - 2 * Damp * Quick * ForeGo[s]) * h;
                    ForeAt[s] += ForeGo[s] * h;
                }
                // (an elbow bends one way, and no further than it does; a guard is held: it gives a little, no more)
                const float least = armed ? arm - XMConvertToRadians(12) : arm + XMConvertToRadians(3) - (tilt2 - tilt1);
                const float most = armed ? arm + XMConvertToRadians(12) : arm + XMConvertToRadians(75) + (std::max)(0.f, bent - XMConvertToRadians(11)) - (tilt2 - tilt1);
                if (ForeAt[s] < least) { ForeAt[s] = least; ForeGo[s] = (std::max)(ForeGo[s], 0.f); }
                if (ForeAt[s] > most) { ForeAt[s] = most; ForeGo[s] = (std::min)(ForeGo[s], 0.f); }
                XMVECTOR want1 = XMVectorSet(r1.x, -across1 * std::cos(tilt1 + arm), across1 * std::sin(tilt1 + arm), 0);
                XMVECTOR want2 = XMVectorSet(r2.x, -across2 * std::cos(tilt2 + ForeAt[s]), across2 * std::sin(tilt2 + ForeAt[s]), 0);
                if (armOut != 0)
                {   // (held out from her side: turned about her forward line, away from her other arm)
                    const float outSide = XMVectorGetX(at(ua)) < XMVectorGetX(at(ArmChain[1 - s][1])) ? -1.f : 1.f;
                    const XMMATRIX outTurn = XMMatrixRotationZ(armOut * outSide);
                    want1 = XMVector3TransformNormal(want1, outTurn);
                    want2 = XMVector3TransformNormal(want2, outTurn);
                }
                const XMVECTOR qAbove = turnOf(above), qArm = turnOf(ua), qFore = turnOf(fa);
                const XMVECTOR turn1 = ArcTurn(d1, XMVector3Normalize(want1)), newArm = XMQuaternionMultiply(qArm, turn1);
                const XMVECTOR turn2 = ArcTurn(XMVector3Rotate(d2, turn1), XMVector3Normalize(want2)), newFore = XMQuaternionMultiply(XMQuaternionMultiply(qFore, turn1), turn2);
                XMStoreFloat4(&out.LocalJoints[ua].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(newArm, XMQuaternionInverse(qAbove))));
                XMStoreFloat4(&out.LocalJoints[fa].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(newFore, XMQuaternionInverse(newArm))));
                // the hand: a loose wrist, left behind a little by the forearm as that swings (8 degrees at a run); a fist on a handle is not
                const float loose = armed ? 0.f : std::clamp(-.035f * ForeGo[s], -.2f, .2f);
                const XMVECTOR flop = ArcTurn(XMVectorSet(0, -1, 0, 0), XMVectorSet(0, -std::cos(loose), std::sin(loose), 0));
                const XMVECTOR newHand = XMQuaternionMultiply(XMQuaternionMultiply(XMQuaternionMultiply(turnOf(ha), turn1), turn2), flop);
                XMStoreFloat4(&out.LocalJoints[ha].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(newHand, XMQuaternionInverse(newFore))));
            }
        }
        // One clip at one time, eased in from whatever she was doing when the clip changed; during a cut on the move
        // her legs are the gait's (Legs) and her spine takes up the difference of the hips.
        bool Compose(const WorldState& world, const std::string& name, float time, float dt, DoorwaysSkeletonPose& out, std::string* error)
        {
            if (!Sample(name, time, out, error)) return false;
            const bool mixed = Legs > 0 && LegsPose.LocalJoints.size() == out.LocalJoints.size() && Upper.size() == out.LocalJoints.size();
            const XMVECTOR cutHips = Hips >= 0 ? XMLoadFloat4(&out.LocalJoints[Hips].Rotation) : XMQuaternionIdentity();
            if (mixed)
                for (size_t i = 0; i < out.LocalJoints.size(); ++i)
                    if (!Upper[i] || (FlyBody && i < Wing.size() && Wing[i]))
                    {
                        auto& to = out.LocalJoints[i];
                        const auto& legs = LegsPose.LocalJoints[i];
                        XMStoreFloat3(&to.Translation, XMVectorLerp(XMLoadFloat3(&to.Translation), XMLoadFloat3(&legs.Translation), Legs));
                        if (!(FlyBody && static_cast<int>(i) == Hips))   // a cut in the air keeps the cut's own hips
                            XMStoreFloat4(&to.Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&to.Rotation), XMLoadFloat4(&legs.Rotation), Legs)));
                    }
            if (mixed && !FlyBody && Hips >= 0 && Spine >= 0)
            {
                const XMVECTOR walkHips = XMLoadFloat4(&out.LocalJoints[Hips].Rotation);
                const XMVECTOR gaitHips = XMQuaternionNormalize(XMQuaternionSlerp(walkHips, cutHips, HipsShare * Legs));
                XMStoreFloat4(&out.LocalJoints[Hips].Rotation, gaitHips);
                const XMVECTOR spine = XMLoadFloat4(&out.LocalJoints[Spine].Rotation);
                XMVECTOR rest = XMQuaternionMultiply(cutHips, XMQuaternionInverse(gaitHips));
                if (XMVectorGetW(rest) < 0) rest = XMVectorNegate(rest);
                if (Spine1 >= 0 && Spine2 >= 0)
                {
                    const XMVECTOR third = XMQuaternionNormalize(XMQuaternionSlerp(XMQuaternionIdentity(), rest, 1.f / 3));
                    const XMVECTOR s1 = XMLoadFloat4(&out.LocalJoints[Spine1].Rotation), s2 = XMLoadFloat4(&out.LocalJoints[Spine2].Rotation);
                    const XMVECTOR made = XMQuaternionMultiply(XMQuaternionMultiply(XMQuaternionMultiply(s2, s1), spine), cutHips);
                    const XMVECTOR n0 = XMQuaternionNormalize(XMQuaternionMultiply(spine, third)), n1 = XMQuaternionNormalize(XMQuaternionMultiply(s1, third));
                    const XMVECTOR below = XMQuaternionMultiply(XMQuaternionMultiply(n1, n0), gaitHips);
                    XMStoreFloat4(&out.LocalJoints[Spine].Rotation, n0);
                    XMStoreFloat4(&out.LocalJoints[Spine1].Rotation, n1);
                    XMStoreFloat4(&out.LocalJoints[Spine2].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(made, XMQuaternionInverse(below))));
                }
                else XMStoreFloat4(&out.LocalJoints[Spine].Rotation, XMQuaternionNormalize(XMQuaternionMultiply(spine, rest)));
            }
            EaseArms(name, time, dt, out);      // her arms at ease, or on guard as she goes: part of the pose she changes to
            if (PoseFile.is_open() && ArmChain[0][3] >= 0 && ArmChain[1][3] >= 0)
            {   // (for the pose log: her hands as the clip alone has them)
                ClipOnly = out;
                if (EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, ClipOnly, nullptr))
                    for (int s = 0; s < 2; ++s) { const auto& m = ClipOnly.ModelSpaceMatrices[ArmChain[s][3]]; ClipHand[s] = {m._41, m._42, m._43}; }
            }
            // (her swords taken or put away as she goes on: a change as any other, but of her arms and swords alone)
            const bool same = name == LastClip && !Restarted;   // (a style's move begun again is a change as any other)
            Restarted = false;
            if (!same || (Armed != WasArmed && (name == "Move" || name == "Jump" || name == "Fall")))
            {
                WasArmed = Armed;
                if (!HasReach && Clips.find("Sword_Reach") != Clips.end()) HasReach = Sample("Sword_Reach", 0, ReachPose, nullptr);
                if (!HasBack && Spine2 >= 0 && SwordJoint[0] >= 0 && SwordJoint[1] >= 0)
                {
                    DoorwaysSkeletonPose away;
                    std::string why;
                    if (Sample("Walk", 0, away, &why) && EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, away, &why))
                    {
                        const XMMATRIX chest = XMMatrixInverse(nullptr, XMLoadFloat4x4(&away.ModelSpaceMatrices[Spine2]));
                        for (int k = 0; k < 2; ++k)
                            XMStoreFloat4x4(&SwordOnBack[k], XMLoadFloat4x4(&away.ModelSpaceMatrices[SwordJoint[k]]) * chest);
                        HasBack = true;
                    }
                }
                // Where her swords are is kept (2026-11-19), not read from the clip she comes from: a draw broken off
                // before her hands were at the handles left them on her back, and the next clip had them in her fists
                // without her ever taking them. Dying, she does not draw: they stay where they are.
                if (LastClip.empty()) InHand = !Away(name);
                const bool hadThem = InHand;
                Drawing = HasReach && name != "Death" && InHand == Away(name) && ReachPose.LocalJoints.size() == out.LocalJoints.size();
                if (!Drawing && name != "Death") InHand = !Away(name);
                SwordKept = hadThem && InHand && !Drawing;
                BlendFrom = Previous;
                BlendTime = 0;
                BlendSeconds = name == "Death" || name == "Jump" ? .12f : name.rfind("Attack", 0) == 0 ? (CutBlend > 0 ? CutBlend : .17f) : .2f;
                // Out of a cut or a skill back to her guard, or on her way (2026-11-19). Each cut ends with her chest 24 to 71
                // degrees from her guard and her cutting hand 25 to 67 cm from it; the spin and the throw ended turned
                // and still turning; and in a fifth of a second she was thrown back, her hands at 7 to 16 m/s. Now she is
                // given time: standing, all of her; going on her way, her body above her hips, her legs taking up her
                // stride at once.
                UpperOut = 0;
                const bool fromSkill = LastClip.rfind("Skill", 0) == 0 || LastClip == "Cast_Flip" || LastClip == "Attack_Jump" || LastClip.rfind("Finish", 0) == 0;
                if (!Drawing && (fromSkill || LastClip.rfind("Attack", 0) == 0) && (name == "Idle" || name == "Stand" || name == "Move"))
                {
                    if (name == "Move") UpperOut = fromSkill ? SkillOut : CutOut;
                    else BlendSeconds = fromSkill ? SkillOut : CutOut;
                }
                // (into the feather storm or the cast her body goes a little longer, slow away and slow in: from her guard
                // her arms were flung out to the old spin's in a fifth of a second, her hands at 7 to 8 m/s in its first frames)
                const bool skillIn = !Drawing && (name == "Skill_Storm" || name == "Skill_Cast");
                if (skillIn) BlendSeconds = SkillIn;
                // (into a finisher quickly, as into a cut: its first move is a dash or a spring)
                const bool finishIn = !Drawing && name.rfind("Finish", 0) == 0;
                if (finishIn) BlendSeconds = .16f;
                if (!Drawing && name.rfind("Guard_", 0) == 0) BlendSeconds = GuardRaise;   // (into a guard: its Raise)
                if (Drawing)
                {   // Her arms have their own time: up to the handles, and on to the new clip. A cut out of standing at
                    // ease took her hand from her hip to the handle over her shoulder in an eighth of a second (20 m/s at
                    // its fastest: a whip, not an arm). Now the way up takes the longer part and the cut comes down from
                    // the handle; unhurried when nothing presses, and slowest putting them away to stand at ease.
                    const bool pressed = name.rfind("Attack", 0) == 0 || name.rfind("Skill", 0) == 0 || name.rfind("Cast", 0) == 0 || name.rfind("Finish", 0) == 0;
                    const bool cut = name.rfind("Attack", 0) == 0 && name != "Attack_Jump";
                    // (Putting them away the blade is taken back over her shoulder: that is given its time.)
                    const bool putting = InHand, atEase = name == "Stand";
                    BlendSeconds = cut ? DrawReachPressed + DrawOnCut : pressed ? .26f : .36f;   // (into a cut her body goes with her arms: there just before the blow)
                    DrawReach = pressed ? DrawReachPressed : putting ? (atEase ? .42f : .32f) : .26f;
                    DrawSeconds = DrawReach + (cut ? DrawOnCut : pressed ? DrawOnPressed : putting ? (atEase ? .34f : .28f) : .30f);
                    if (InMove() && name == MoveOf->Clip)
                    {   // (a style's move: drawn as fast as it goes; owner, 2026-11-20: "at least each attack to be 3x faster")
                        DrawReach = StyleDrawReach; DrawSeconds = BlendSeconds = StyleDrawReach + StyleDrawOn;
                    }
                    Reaching[0] = Reaching[1] = ArmState{};
                }
                LastClip = name;
                // what is carried on: for every joint, the old pose against the new one, and how the old one was moving
                // (no faster than would cross that difference three times in the time of the change: no overshoot of note)
                const size_t n = out.LocalJoints.size();
                CarrySmooth = (name.rfind("Attack", 0) == 0 && name != "Attack_Jump" && (CutBlend > 0 || Drawing)) || skillIn || finishIn;
                CarrySeconds = Previous.LocalJoints.size() != n || same ? 0.f : CarrySmooth ? BlendSeconds : BlendSeconds * 1.6f;
                if (CarrySeconds > 0 && !CarrySmooth) CarrySeconds = (std::max)(CarrySeconds, UpperOut * 1.6f);
                CarryArms = !Drawing;
                ArmTo.assign(out.LocalJoints.size(), XMFLOAT4{0, 0, 0, 0});
                CarryAt.assign(n, {}); CarryGo.assign(n, {}); CarryTurn.assign(n, {}); CarrySpin.assign(n, {});
                const bool moving = Before.LocalJoints.size() == n && LastDt > 1e-4f;
                for (size_t i = 0; i < n && CarrySeconds > 0; ++i)
                {
                    const auto& was = Previous.LocalJoints[i]; const auto& now = out.LocalJoints[i];
                    const XMVECTOR at = XMLoadFloat3(&was.Translation) - XMLoadFloat3(&now.Translation);
                    const XMVECTOR turned = TurnVector(XMQuaternionMultiply(XMQuaternionInverse(XMLoadFloat4(&now.Rotation)), XMLoadFloat4(&was.Rotation)));
                    XMVECTOR go = XMVectorZero(), spin = XMVectorZero();
                    if (moving)
                    {
                        const auto& before = Before.LocalJoints[i];
                        go = (XMLoadFloat3(&was.Translation) - XMLoadFloat3(&before.Translation)) / LastDt;
                        spin = TurnVector(XMQuaternionMultiply(XMQuaternionInverse(XMLoadFloat4(&before.Rotation)), XMLoadFloat4(&was.Rotation))) / LastDt;
                        go = XMVector3ClampLength(go, 0, 3 * XMVectorGetX(XMVector3Length(at)) / BlendSeconds);
                        spin = XMVector3ClampLength(spin, 0, 3 * XMVectorGetX(XMVector3Length(turned)) / BlendSeconds);
                    }
                    XMStoreFloat3(&CarryAt[i], at); XMStoreFloat3(&CarryGo[i], go); XMStoreFloat3(&CarryTurn[i], turned); XMStoreFloat3(&CarrySpin[i], spin);
                }
            }
            BlendTime += (std::max)(0.f, dt);
            if (CarrySeconds > 0 && BlendTime < CarrySeconds && CarryAt.size() == out.LocalJoints.size())
            {   // it dies away as a spring without swing does: all but gone when the old mix would have ended
                const float y = 5.5f / BlendSeconds, e = std::exp(-y * BlendTime);
                // (into a cut it dies away evenly instead, slow away and slow in, and is gone as the swing begins: as a
                // spring it was fastest in its first frames, and her hand was thrown at the wind-up)
                const float gone = BlendTime / CarrySeconds, rest = 1 - gone;
                const float wAt = CarrySmooth ? 1 - gone * gone * gone * (10 - 15 * gone + 6 * gone * gone) : (1 + y * BlendTime) * e;
                const float wGo = CarrySmooth ? BlendTime * rest * rest * rest : BlendTime * e;
                // (her body above her hips more slowly, out of a cut or a skill as she goes on her way: UpperOut)
                const float yUp = UpperOut > 0 ? 5.5f / UpperOut : y, eUp = std::exp(-yUp * BlendTime);
                const float wAtUp = UpperOut > 0 ? (1 + yUp * BlendTime) * eUp : wAt, wGoUp = UpperOut > 0 ? BlendTime * eUp : wGo;
                for (size_t i = 0; i < out.LocalJoints.size(); ++i)
                {
                    if (static_cast<int>(i) == SwordJoint[0] || static_cast<int>(i) == SwordJoint[1]) continue;   // (below)
                    if (!CarryArms && i < ArmJoint.size() && ArmJoint[i]) continue;                                // (to the handles, below)
                    auto& to = out.LocalJoints[i];
                    const bool slow = UpperOut > 0 && i < Upper.size() && Upper[i];
                    const XMVECTOR at = XMLoadFloat3(&CarryAt[i]), turned = XMLoadFloat3(&CarryTurn[i]);
                    XMStoreFloat3(&to.Translation, XMLoadFloat3(&to.Translation) + at * (slow ? wAtUp : wAt) + XMLoadFloat3(&CarryGo[i]) * (slow ? wGoUp : wGo));
                    const XMVECTOR turn = turned * (slow ? wAtUp : wAt) + XMLoadFloat3(&CarrySpin[i]) * (slow ? wGoUp : wGo);
                    XMStoreFloat4(&to.Rotation, XMQuaternionNormalize(XMQuaternionMultiply(XMLoadFloat4(&to.Rotation), TurnOf(turn))));
                }
            }
            const bool drawing = Drawing && BlendTime < DrawSeconds;      // her arms on their way to the handles and on
            const bool atHandles = BlendTime >= DrawReach;                // (in a draw) her hands have been there
            bool led = false;                                             // her arms and swords go the handles' way (ReachArms)
            if (Drawing && atHandles) InHand = !Away(name);               // taken, or put away
            if (((BlendSeconds > 0 && BlendTime < BlendSeconds) || drawing) && BlendFrom.LocalJoints.size() == out.LocalJoints.size())
            {
                float w = BlendSeconds > 0 ? std::clamp(BlendTime / BlendSeconds, 0.f, 1.f) : 1.f;
                w = w * w * (3 - 2 * w);
                for (size_t i = 0; i < out.LocalJoints.size(); ++i)
                {
                    auto& to = out.LocalJoints[i];
                    const auto& from = BlendFrom.LocalJoints[i];
                    if (static_cast<int>(i) == SwordJoint[0] || static_cast<int>(i) == SwordJoint[1])
                    {   // a sword goes between her fist and her back (walk, run, stairs, jump: put away; 2026-11-18) at once,
                        // as her hands are at the handles, not through her body: until then it is where it was. One that
                        // stays in her fist goes evenly to how the new clip holds it: a draw broken off (a cut asked for
                        // as she takes them) leaves it turned in her fist, and it would jump half-way through the change.
                        if (SwordKept)
                        {
                            XMStoreFloat3(&to.Translation, XMVectorLerp(XMLoadFloat3(&from.Translation), XMLoadFloat3(&to.Translation), w));
                            XMStoreFloat4(&to.Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&from.Rotation), XMLoadFloat4(&to.Rotation), w)));
                        }
                        else if (Drawing ? !atHandles : w < .5f) to = from;
                        continue;
                    }
                    if (drawing && i < ArmJoint.size() && ArmJoint[i])
                    {   // to the handles, then on to the new clip
                        const auto ease = [](float v) { v = std::clamp(v, 0.f, 1.f); return v * v * (3 - 2 * v); };
                        const XMVECTOR reach = XMLoadFloat4(&ReachPose.LocalJoints[i].Rotation);
                        // The clip she changes to goes on moving (and her stride changes its mix as she gathers speed): the
                        // way round from the handles to it is chosen once, the shorter one, and kept. Chosen anew every
                        // frame it could change sides when the two were near half a turn apart, and her arm jumped.
                        XMVECTOR goes = XMLoadFloat4(&to.Rotation);
                        if (i < ArmTo.size())
                        {
                            const XMVECTOR was = XMLoadFloat4(&ArmTo[i]);
                            if (XMVectorGetX(XMVector4Dot(XMVectorGetX(XMVector4LengthSq(was)) > .5f ? was : reach, goes)) < 0) goes = XMVectorNegate(goes);
                            XMStoreFloat4(&ArmTo[i], goes);
                        }
                        const XMVECTOR q = !atHandles ? XMQuaternionSlerp(XMLoadFloat4(&from.Rotation), reach, ease(BlendTime / DrawReach))
                                                      : SlerpAsGiven(reach, goes, ease((BlendTime - DrawReach) / (DrawSeconds - DrawReach)));
                        XMStoreFloat4(&to.Rotation, XMQuaternionNormalize(q));
                        continue;
                    }
                }
                // (her shoulders and fingers go so; her arms and hands by the way an arm takes)
                if (drawing) led = ReachArms(out, atHandles, atHandles ? 1 - (BlendTime - DrawReach) / (std::max)(.01f, DrawSeconds - DrawReach) : BlendTime / (std::max)(.01f, DrawReach), dt);
            }
            // her wrists no further than a wrist bends, while her swords are in her hands (HoldWrists)
            WristHeld = std::clamp(WristHeld + (InHand ? 1.f : -1.f) * (std::max)(0.f, dt) / WristTakes, 0.f, 1.f);
            HoldWrists(out, WristHeld * WristHeld * (3 - 2 * WristHeld));
            // On her back her swords are carried by her chest, whatever her arms do (2026-11-19). The clips key them
            // against her hands so that they lie on her back as the clip moves her arms; but through every change of
            // clip her arms are not where the clip has them, and her swords slid 10-18 cm over her back at every start
            // and stop. So too while her hands go to her back or come away empty.
            std::string whyNot;
            if (!InHand && HasBack && EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, &whyNot))
                for (int k = 0; k < 2; ++k)
                {
                    const int j = SwordJoint[k], parent = Skeleton.Joints[j].ParentIndex;
                    if (parent < 0) continue;
                    const XMMATRIX want = XMLoadFloat4x4(&SwordOnBack[k]) * XMLoadFloat4x4(&out.ModelSpaceMatrices[Spine2]);
                    XMVECTOR sc, q, t;
                    if (!XMMatrixDecompose(&sc, &q, &t, want * XMMatrixInverse(nullptr, XMLoadFloat4x4(&out.ModelSpaceMatrices[parent])))) continue;
                    XMStoreFloat3(&out.LocalJoints[j].Translation, t);
                    XMStoreFloat4(&out.LocalJoints[j].Rotation, XMQuaternionNormalize(q));
                }
            // A sword taken or let go is not at once where its new holder has it (2026-11-19): it was, and jumped there
            // (16 to 24 cm, and turned). Where her hands go the handles' way (ReachArms) the sword is taken where it
            // lies and goes on from there: nothing jumps. Otherwise (no way known: a rig without the reach pose) the
            // sword turns about its handle into its new place over a short time.
            if (InHand != SwordWasInHand)
            {
                SwordWasInHand = InHand; SettleKnown = false;
                SwordSettle = Previous.ModelSpaceMatrices.size() == Skeleton.Joints.size() && SwordJoint[0] >= 0 && SwordJoint[1] >= 0 && !led ? 0.f : SettleSeconds;
                if (SwordSettle < SettleSeconds)
                    for (int k = 0; k < 2; ++k) SwordBefore[k] = Previous.ModelSpaceMatrices[SwordJoint[k]];
            }
            if (SwordSettle < SettleSeconds && !SwordsOut && EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, &whyNot))
            {
                for (int k = 0; k < 2 && !SettleKnown; ++k)
                {
                    XMVECTOR sc, turned, slipped;
                    if (!XMMatrixDecompose(&sc, &turned, &slipped, XMLoadFloat4x4(&SwordBefore[k]) * XMMatrixInverse(nullptr, XMLoadFloat4x4(&out.ModelSpaceMatrices[SwordJoint[k]]))))
                    { turned = XMQuaternionIdentity(); slipped = XMVectorZero(); }
                    turned = XMQuaternionNormalize(turned);
                    if (XMVectorGetW(turned) < 0) turned = XMVectorNegate(turned);
                    const XMVECTOR middle = XMLoadFloat3(&SwordGrip[k]);
                    XMStoreFloat4(&SettleTurn[k], turned);
                    XMStoreFloat3(&SettleSlip[k], slipped - middle + XMVector3Rotate(middle, turned));
                }
                SettleKnown = true;
                const float gone = SwordSettle / SettleSeconds, left = 1 - gone * gone * gone * (10 - 15 * gone + 6 * gone * gone);
                for (int k = 0; k < 2; ++k)
                {
                    auto& sword = out.LocalJoints[SwordJoint[k]];
                    const XMVECTOR turned = XMQuaternionSlerp(XMQuaternionIdentity(), XMLoadFloat4(&SettleTurn[k]), left), middle = XMLoadFloat3(&SwordGrip[k]);
                    const XMVECTOR slipped = (middle - XMVector3Rotate(middle, turned) + XMLoadFloat3(&SettleSlip[k]) * left) * XMLoadFloat3(&sword.Scale);
                    XMStoreFloat3(&sword.Translation, XMLoadFloat3(&sword.Translation) + XMVector3Rotate(slipped, XMLoadFloat4(&sword.Rotation)));
                    XMStoreFloat4(&sword.Rotation, XMQuaternionNormalize(XMQuaternionMultiply(turned, XMLoadFloat4(&sword.Rotation))));
                }
                SwordSettle += (std::max)(0.f, dt);
            }
            if (BodyTwist != 0.f && Hips >= 0)
            {   // V's and the twisted dash's turns about her own length (TurnBody): her hips about her place's up
                XMStoreFloat4(&out.LocalJoints[Hips].Rotation, XMQuaternionNormalize(After(XMQuaternionRotationAxis(XMLoadFloat3(&Upright), BodyTwist), XMLoadFloat4(&out.LocalJoints[Hips].Rotation))));
            }
            if (Cast == 5 && SwordsOut && CastTime < FlipPush + FlipRip)
            {   // her hands on the handles (ReachHandles): to them as her feet stick, on them until she kicks off, off them as
                // her swords come out with them
                const float u = std::clamp(CastTime / FlipStick, 0.f, 1.f);
                ReachHandles(world, out, u * u * (3 - 2 * u) * (1 - SwordBack));
            }
            PlaceSwords(world, out);
            if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, error)) return false;
            if (LegJoint[0] >= 0)
            {   // her lowest joint
                Lowest = 1e9f;
                for (const int i : BodyJoints) Lowest = (std::min)(Lowest, out.ModelSpaceMatrices[i]._42);
            }
            Before = std::move(Previous); LastDt = dt;
            Previous = out;
            return IsFiniteDoorwaysPose(out, Skeleton.Joints.size());
        }

        void Wound(const XMFLOAT3& feet, float height, float radius, const XMFLOAT3& from, float side, bool blood, int damage)
        {   // the streak across the cut, sparks, and (from a creature) blood
            float dx = from.x - feet.x, dz = from.z - feet.z;
            const float d = (std::max)(.01f, std::hypot(dx, dz));
            dx /= d; dz /= d;
            const XMFLOAT3 at{feet.x + dx * radius * .6f, feet.y + std::clamp(from.y - feet.y + 1.25f, .3f, (std::max)(.4f, height * .9f)), feet.z + dz * radius * .6f};
            const float rise = Rand(-.25f, .45f);
            const XMFLOAT3 cut{-dz * side, rise, dx * side};   // across her, the way the blade goes
            if (Marks.size() > 500) Marks.erase(Marks.begin(), Marks.begin() + 150);
            Marks.push_back({at, cut, .2f, .2f, .05f, feet.y, 2});
            for (int i = 0; i < 7; ++i)
                Marks.push_back({at, {cut.x * Rand(2, 6) + dx * Rand(0, 2) + Rand(-1, 1), Rand(-.5f, 2.5f), cut.z * Rand(2, 6) + dz * Rand(0, 2) + Rand(-1, 1)},
                                 Rand(.15f, .3f), .3f, Rand(.02f, .04f), feet.y, 3});
            if (!blood) return;
            const int drops = std::clamp(damage / 5, 8, 20);
            for (int i = 0; i < drops; ++i)
                Marks.push_back({at, {cut.x * Rand(.8f, 3.6f) + dx * Rand(.2f, 1.6f) + Rand(-.5f, .5f), Rand(.6f, 3.2f) + cut.y,
                                      cut.z * Rand(.8f, 3.6f) + dz * Rand(.2f, 1.6f) + Rand(-.5f, .5f)},
                                 3.f, 3.f, Rand(.018f, .042f), feet.y, 0});
        }
        // blood thrown from a wound at 'at' the way given (about speed m/s), falling and lying on the ground (floor)
        void Spray(const XMFLOAT3& at, const XMFLOAT3& way, int count, float speed, float floor)
        {
            if (Marks.size() > 500) Marks.erase(Marks.begin(), Marks.begin() + 150);
            for (int i = 0; i < count; ++i)
            {
                const float s = speed * Rand(.4f, 1.f);
                Marks.push_back({at, {way.x * s + Rand(-1, 1), way.y * s + Rand(.3f, 2.2f), way.z * s + Rand(-1, 1)}, 3.f, 3.f, Rand(.02f, .045f), floor, 0});
            }
        }
        void UpdateMarks(float dt)
        {
            for (auto& f : Marks)
            {
                f.Life -= dt;
                if (f.Kind == 4) { f.At.x += f.Go.x * dt; f.At.y += f.Go.y * dt; f.At.z += f.Go.z * dt; }
                if (f.Kind == 0 || f.Kind == 3)
                {
                    f.Go.y -= (f.Kind == 0 ? 9.8f : 4.f) * dt;
                    f.At.x += f.Go.x * dt; f.At.y += f.Go.y * dt; f.At.z += f.Go.z * dt;
                    if (f.Kind == 0 && f.At.y <= f.Floor + .012f)
                    {   // it lands and lies
                        f.Kind = 1;
                        f.At.y = f.Floor + .012f;
                        f.Life = f.Full = Rand(4, 7);
                        f.Size *= Rand(1.8f, 3.f);
                        f.Go = {Rand(0, 6.28f), 0, 0};
                    }
                }
            }
            std::erase_if(Marks, [](const Mark& f) { return f.Life <= 0; });
        }
    };

    NyxaraKit::NyxaraKit() : mBody(std::make_unique<Body>()) {}
    NyxaraKit::~NyxaraKit() = default;

    float NyxaraKit::Random()
    {
        mSeed = mSeed * 1664525u + 1013904223u;
        return static_cast<float>((mSeed >> 8) & 0xFFFFFF) / 16777216.f;
    }

    bool NyxaraKit::LoadBody(Renderer& renderer, WorldRenderer& worldRenderer, WorldState& world, const std::filesystem::path& bundle, std::string& error)
    {
        if (!std::filesystem::is_directory(bundle)) { error = "her body is missing: " + bundle.string(); return false; }
        DoorwaysAnimatedCharacterAssetLoadDesc desc;
        desc.Name = "Nyxara (daughter of the eagle)";
        desc.MeshPath = bundle / "Character.dmesh";
        desc.SkeletonPath = bundle / "Character.dskel";
        desc.MeshRigBindingPath = bundle / "Character.meshbind";
        desc.IdleAnimationPath = bundle / "Idle.danim";
        desc.WalkAnimationPath = bundle / "Walk.danim";
        desc.BasicAttackAnimationPath = bundle / "BasicAttack.danim";
        desc.RequireRigSignature = true;
        desc.AllowMissingFiles = false;
        desc.LoadPolicy = DoorwaysAnimatedCharacterAssetLoadPolicy::RequiredProduction;
        for (const auto& entry : std::filesystem::directory_iterator(bundle))
            if (entry.path().extension() == ".danim")
            {
                const auto name = entry.path().stem().string();
                desc.AuthoredClips.emplace(name, DoorwaysAuthoredClipSource{entry.path(), Body::Loops(name), 0});
            }
        DoorwaysAnimatedCharacterAsset asset;
        if (!LoadDoorwaysAnimatedCharacterAsset(desc, asset, &error)) return false;
        if (!worldRenderer.InitializeSkinnedMaterials(renderer, bundle, bundle / "Character.dmat", asset.Skeleton.Name, error)) return false;
        // a bundle with her looks (the Character Lab's: NyxaraLooks) carries their parts as materials named Look_...:
        // hidden until a look shows them, so she is as she set out
        for (const auto& name : worldRenderer.CharacterMaterialNames())
            if (name.rfind("Look_", 0) == 0) worldRenderer.SetCharacterMaterialVisible(name, false);
        mWingScale = 1;
        auto& character = world.ImportedAnimatedCharacter;
        if (!character.InitializeFromAsset(asset, &error)) return false;
        worldRenderer.InvalidateCharacterMeshForAssetChange();
        character.ResetPlaybackForSession();
        character.SetRootTransform(world.MainPlayer.Position, world.MainPlayer.Yaw);
        auto& visual = world.AnimatedPlayerVisual;
        visual.Enabled = true; visual.Scale = {1, 1, 1}; visual.Rotation = {0, 0, 0}; visual.Translation = {0, 0, 0}; visual.YawOffset = 0;

        auto& b = *mBody;
        b = Body{};
        b.Skeleton = asset.Skeleton;
        b.Clips = asset.AuthoredClips;
        b.Upper.assign(b.Skeleton.Joints.size(), 0);
        b.Wing.assign(b.Skeleton.Joints.size(), 0);
        for (size_t i = 0; i < b.Skeleton.Joints.size(); ++i)
        {
            const auto& j = b.Skeleton.Joints[i];
            const bool spine = j.Name == "Spine" || j.Name.ends_with(":Spine");
            if (spine) b.Spine = static_cast<int>(i);
            if (j.Name.ends_with("Spine1")) b.Spine1 = static_cast<int>(i);
            if (j.Name.ends_with("Spine2")) b.Spine2 = static_cast<int>(i);
            b.Upper[i] = spine || (j.ParentIndex >= 0 && j.ParentIndex < static_cast<int>(i) && b.Upper[j.ParentIndex]);
            b.Wing[i] = j.Name.find("Wing") != std::string::npos;
            if (j.Name.ends_with("Hips")) b.Hips = static_cast<int>(i);
        }
        for (const char* need : {"Idle", "Walk", "Run", "Death", "Jump", "Fly", "Attack_L1", "Attack_R1", "Skill_Slide", "Skill_Cast", "Cast_Fly", "Cast_Pull", "Fly_Rise", "Fly_Land"})
            if (b.Duration(need) <= 0) { error = std::string("her body has no clip ") + need + " (" + bundle.string() + ")"; return false; }
        b.FindSwords();
        if (b.SwordJoint[0] < 0) { error = "her body has no SwordLeft / SwordRight bones (" + bundle.string() + ")"; return false; }
        if (!b.Sample("Idle", 0, b.Previous, &error)) return false;
        b.SmoothWrists();
        b.EvenWindUps();
        b.LearnSwings();
        b.LearnGrip();
        b.MakeStorm();
        if (b.Duration("Skill_Storm") <= 0) { error = "her feather storm could not be made from her clips (" + bundle.string() + ")"; return false; }
        b.MakeCast();
        if (b.Duration("Cast_Drill") <= 0 || b.Duration("Cast_Flip") <= 0 || b.Duration("Dash_Twist") <= 0)
        { error = "her thrown swords' way and backflip could not be made from her clips (" + bundle.string() + ")"; return false; }
        b.MakeFinishes();
        b.MakeStyles();   // (after every fix of her clips: the styles' moves are made of them as they are then)
        b.MakeSoul();     // (her soul's leaving and way back: Q)
        b.LearnSoles();   // (where her feet are down in her walk and her run: her ways of going, GaitStyle)
        b.LearnWings();   // (her wings as her walk and her run have them on average: held so in her ways of going)
        if (!mSoul) mSoul = std::make_unique<NyxaraSoul>();   // (its picture: its textures are loaded as it is first drawn)
        for (int which = 1; which <= 3; ++which)
            if (b.Duration(Finishes[which].Clip) <= 0) { error = std::string("her finishing kill ") + Finishes[which].Clip + " could not be made from her clips (" + bundle.string() + ")"; return false; }
        char log[MAX_PATH] = {};
        if (GetEnvironmentVariableA("CAUCASIAN_EAGLE_NYXARA_LOG", log, sizeof(log)) > 0 && log[0])
        {
            b.LogPath = log;
            std::ofstream(b.LogPath) << "seconds\tevent\tclip\tspeed\tx\ty\tz\tlift\tlean\tfeet_drop\tleft_raise\tright_raise\tleft_gap\tright_gap\tcam_x\tcam_y\tcam_z\tthrottle\tmove_speed\n";
        }
        if (!b.WindUpsEvened.empty()) b.Log(world, ("cut wind-ups made one move: " + b.WindUpsEvened).c_str());
        if (!b.WindUpsLeft.empty()) b.Log(world, ("cut wind-ups left as they were: " + b.WindUpsLeft).c_str());
        if (GetEnvironmentVariableA("CAUCASIAN_EAGLE_NYXARA_POSELOG", log, sizeof(log)) > 0 && log[0])
        {
            b.PoseFile.open(log);
            b.PoseFile << "seconds\tdt\tclip\tphase\tspeed\tx\ty\tz\tyaw\tgrounded\tvy\tflying\tplanted\tleft_gap\tright_gap";
            for (const char* name : {"Hips", "Spine2", "Head", "LeftHand", "RightHand", "LeftLeg", "RightLeg", "LeftFoot", "RightFoot", "LeftToeBase", "RightToeBase",
                                     "WingL3", "WingR3", "SwordLeft", "SwordRight", "DressLeftKnee", "DressRightKnee",
                                     "LeftArm", "RightArm", "LeftForeArm", "RightForeArm", "LeftHandIndex1", "RightHandIndex1", "LeftHandRing1", "RightHandRing1"})
                for (size_t i = 0; i < b.Skeleton.Joints.size(); ++i)
                    if (b.Skeleton.Joints[i].Name.ends_with(name))
                    {
                        b.PoseJoints.push_back(static_cast<int>(i));
                        for (const char* axis : {"_x", "_y", "_z"}) b.PoseFile << '\t' << name << axis;
                        break;
                    }
            for (const char* name : {"SwordLeftTip", "SwordRightTip", "ClipLeftHand", "ClipRightHand"})
                for (const char* axis : {"_x", "_y", "_z"}) b.PoseFile << '\t' << name << axis;
            for (const char* foot : {"left", "right"})
                for (const char* what : {"_held", "_air", "_go", "_togo", "_lifted", "_stair", "_free", "_swing", "_land"}) b.PoseFile << '\t' << foot << what;
            b.PoseFile << '\n' << std::setprecision(6);
        }
        if (GetEnvironmentVariableA("CAUCASIAN_EAGLE_NYXARA_BAKE", log, sizeof(log)) > 0 && log[0])
        {   // Every clip as the game has it after load (its cuts' wind-ups evened, her storm, cast, finishers and combo moves
            // made of her clips): each joint's local translation, rotation (x y z w) and scale, 30 times a second, into
            // <folder>/<clip>.tsv; her skeleton (names, parents, bind) into skeleton.tsv, and the list into clips.tsv. For
            // art/nyxara/bake_game_clips.py, which makes them Actions in her Blender file (owner, 2026-11-21: "update
            // nyxara's assets/blender project with latest animations which should only include what we use ingame now")
            const std::filesystem::path folder(log);
            std::error_code ec; std::filesystem::create_directories(folder, ec);
            {
                std::ofstream sk(folder / "skeleton.tsv");
                sk << std::setprecision(9) << "index\tname\tparent\ttx\tty\ttz\trx\try\trz\trw\tsx\tsy\tsz\n";
                for (size_t i = 0; i < b.Skeleton.Joints.size(); ++i)
                {
                    const auto& j = b.Skeleton.Joints[i];
                    sk << i << '\t' << j.Name << '\t' << j.ParentIndex << '\t' << j.LocalBindTranslation.x << '\t' << j.LocalBindTranslation.y << '\t' << j.LocalBindTranslation.z
                       << '\t' << j.LocalBindRotation.x << '\t' << j.LocalBindRotation.y << '\t' << j.LocalBindRotation.z << '\t' << j.LocalBindRotation.w
                       << '\t' << j.LocalBindScale.x << '\t' << j.LocalBindScale.y << '\t' << j.LocalBindScale.z << '\n';
                }
            }
            std::ofstream list(folder / "clips.tsv");
            list << "clip\tseconds\tframes\tloops\n";
            for (const auto& [name, clip] : b.Clips)
            {
                const float seconds = clip.DurationSeconds;
                if (seconds <= 0) continue;
                const int frames = (std::max)(1, static_cast<int>(std::lround(seconds * 30)));
                std::ofstream out(folder / (name + ".tsv"));
                out << std::setprecision(7);
                DoorwaysSkeletonPose pose;
                for (int f = 0; f <= frames; ++f)
                {
                    const float t = (std::min)(seconds, f / 30.f);
                    if (!SampleDoorwaysAnimationClipLocalPose(b.Skeleton, clip, t, pose, nullptr)) break;
                    out << f;
                    for (const auto& l : pose.LocalJoints)
                        out << '\t' << l.Translation.x << '\t' << l.Translation.y << '\t' << l.Translation.z << '\t' << l.Rotation.x << '\t' << l.Rotation.y
                            << '\t' << l.Rotation.z << '\t' << l.Rotation.w << '\t' << l.Scale.x << '\t' << l.Scale.y << '\t' << l.Scale.z;
                    out << '\n';
                }
                list << name << '\t' << seconds << '\t' << frames << '\t' << (Body::Loops(name) ? 1 : 0) << '\n';
            }
        }
        if (GetEnvironmentVariableA("CAUCASIAN_EAGLE_NYXARA_CLIPDUMP", log, sizeof(log)) > 0 && log[0])
        {   // every clip as the bundle has it, 60 times a second: her joints in her own space (for looking at the clips themselves)
            std::ofstream dump(log);
            std::vector<int> joints;
            dump << "clip\tseconds";
            for (const char* name : {"Hips", "Spine2", "Head", "LeftArm", "RightArm", "LeftForeArm", "RightForeArm", "LeftHand", "RightHand",
                                     "LeftHandIndex1", "RightHandIndex1", "LeftHandRing1", "RightHandRing1", "SwordLeft", "SwordRight", "LeftFoot", "RightFoot"})
                for (size_t i = 0; i < b.Skeleton.Joints.size(); ++i)
                    if (b.Skeleton.Joints[i].Name.ends_with(name))
                    {
                        joints.push_back(static_cast<int>(i));
                        for (const char* axis : {"_x", "_y", "_z"}) dump << '\t' << name << axis;
                        break;
                    }
            for (const char* name : {"SwordLeftTip", "SwordRightTip"})
                for (const char* axis : {"_x", "_y", "_z"}) dump << '\t' << name << axis;
            // (and how these are turned, in her own space: for working out her grip)
            std::vector<int> turned;
            for (const char* name : {"Spine2", "LeftShoulder", "RightShoulder", "LeftArm", "RightArm", "LeftForeArm", "RightForeArm", "LeftHand", "RightHand", "SwordLeft", "SwordRight"})
                for (size_t i = 0; i < b.Skeleton.Joints.size(); ++i)
                    if (b.Skeleton.Joints[i].Name.ends_with(name))
                    {
                        turned.push_back(static_cast<int>(i));
                        for (const char* axis : {"_qx", "_qy", "_qz", "_qw"}) dump << '\t' << name << axis;
                        break;
                    }
            dump << '\n' << std::setprecision(6);
            DoorwaysSkeletonPose pose;
            for (const auto& entry : b.Clips)
            {
                const float last = b.Duration(entry.first);
                for (int f = 0; f / 60.f <= last + 1e-4f; ++f)
                {
                    if (!b.Sample(entry.first, (std::min)(f / 60.f, last), pose, nullptr) || !EvaluateDoorwaysSkeletonPoseMatrices(b.Skeleton, pose, nullptr)) break;
                    dump << entry.first << '\t' << f / 60.f;
                    for (const int j : joints) { const auto& m = pose.ModelSpaceMatrices[j]; dump << '\t' << m._41 << '\t' << m._42 << '\t' << m._43; }
                    for (int k = 0; k < 2; ++k)
                    {
                        XMFLOAT3 at; XMStoreFloat3(&at, XMVector3TransformCoord(XMLoadFloat3(&b.SwordTip[k]), XMLoadFloat4x4(&pose.ModelSpaceMatrices[b.SwordJoint[k]])));
                        dump << '\t' << at.x << '\t' << at.y << '\t' << at.z;
                    }
                    for (const int j : turned)
                    {
                        XMVECTOR sc, q, t; XMFLOAT4 r{0, 0, 0, 1};
                        if (XMMatrixDecompose(&sc, &q, &t, XMLoadFloat4x4(&pose.ModelSpaceMatrices[j]))) XMStoreFloat4(&r, XMQuaternionNormalize(q));
                        dump << '\t' << r.x << '\t' << r.y << '\t' << r.z << '\t' << r.w;
                    }
                    dump << '\n';
                }
            }
        }
        OutputDebugStringA(("[PASS] Nyxara: her body " + bundle.filename().string() + " with " + std::to_string(b.Clips.size()) + " clips.\n").c_str());
        return true;
    }

    bool NyxaraKit::Initialize(Renderer& renderer, WorldRenderer& worldRenderer, WorldState& world, std::string& error)
    {
        (void)worldRenderer;
        mReady = false;
        mTrainingMode = false; mTrainingTargetHits = mTrainingPlayerHits = 0;
        if (mBody->Skeleton.Joints.empty()) { error = "her body is not loaded (NyxaraKit::LoadBody)"; return false; }
        if (!BuildRing(renderer, mRingMesh, error) || !BuildQuad(renderer, mQuadMesh, error, -.5f) || !BuildTri(renderer, mTriMesh, error) || !BuildQuad(renderer, mFeatherMesh, error, 0) ||
            !BuildSphere(renderer, mSphereMesh, error) || !BuildCone(renderer, mConeMesh, error) || !BuildBlade(renderer, mBladeMesh, error))
            return false;
        const auto root = ResolveDoorwaysRuntimePath("assets/Compiled/Effects/Nyxara");
        const auto load = [&](const char* name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& out)
        {
            DoorwaysTextureLoadDesc desc;
            desc.Path = root / name;
            desc.Semantic = DoorwaysMaterialTextureSemantic::BaseColor;
            desc.GenerateMips = true;
            DoorwaysTextureLoadResult result;
            if (!renderer.LoadDoorwaysTexture(desc, result, &error)) return false;
            out = result.SRV;
            return true;
        };
        if (!load("Glow.png", mGlowTex) || !load("Ring.png", mRingTex) || !load("Feather.png", mFeatherTex) || !load("WingFeather.png", mWingFeatherTex)) return false;
        // Six shade wraiths hover in a ring around the arrival point (until an owner brings its own enemies).
        mWraiths.clear();
        const XMFLOAT3 origin = world.MainPlayer.Position;
        for (int i = 0; i < 6; ++i)
        {
            Wraith w;
            const float a = (static_cast<float>(i) + .5f) * Pi / 3, radius = 8.5f + 2.5f * static_cast<float>(i % 2);
            w.Anchor = {origin.x + std::sin(a) * radius, origin.y + 1.3f, origin.z + std::cos(a) * radius};
            w.Pos = w.Anchor; w.Phase = static_cast<float>(i) * 1.7f; w.Shoot = 3.f + static_cast<float>(i) * .9f;
            w.Height = 2.f; w.Neck = {0, 1.6f, 0};
            mWraiths.push_back(w);
        }
        mFx.clear(); mOrbs.clear(); mBody->Marks.clear();
        auto& session = GameSession();
        session.Active = true;
        // A and D turn her instead of stepping sideways (the engine's are taken away); W and S go along her facing
        for (const auto action : {DoorwaysInputAction::MoveLeft, DoorwaysInputAction::MoveRight})
            if (std::find(session.SuppressedActions.begin(), session.SuppressedActions.end(), action) == session.SuppressedActions.end())
                session.SuppressedActions.push_back(action);
        mBody->PivotBase = session.CameraPivotHeight;
        mReady = true;
        return true;
    }

    void NyxaraKit::SetExternalEnemies(bool external)
    {
        mExternalEnemies = external;
        if (external) { mWraiths.clear(); mOrbs.clear(); }
    }

    void NyxaraKit::SpawnOrb(XMFLOAT3 from, XMFLOAT3 velocity, int damage, XMFLOAT4 color, float life)
    {
        if (mOrbs.size() >= 40) return;
        Orb o; o.Pos = from; o.Vel = velocity; o.Life = life; o.Damage = damage; o.Color = color;
        mOrbs.push_back(o);
    }

    bool NyxaraKit::Blocks(const WorldState& world, const XMFLOAT3& from)
    {
        if (mBarrier <= .5f) return false;
        const XMFLOAT3 facing = Forward(world.MainPlayer.Yaw);
        XMFLOAT3 rel = Sub(from, world.MainPlayer.Position); rel.y = 0; rel = Norm(rel);
        const bool styled = mGuardStyle >= 0 && mGuardStyle < static_cast<int>(Guards().size());
        if (rel.x * facing.x + rel.z * facing.z < (styled ? Guards()[static_cast<size_t>(mGuardStyle)].Cover : -.3f)) return false;
        mBlockFlash = .25f;
        if (mGuardStyle >= 0 && mGuardStyle < static_cast<int>(Guards().size()))
            mBody->FlinchGo -= Guards()[static_cast<size_t>(mGuardStyle)].Jolt * 30.f;   // (her guard knocked back: her picture tips back and comes up)
        const XMFLOAT3 at = Add(world.MainPlayer.Position, Add(Mul(facing, 1.2f), {0, 1.2f, 0}));
        const XMFLOAT4 shine = styled ? Guards()[static_cast<size_t>(mGuardStyle)].Color : XMFLOAT4{.45f, .75f, 1.f, 1};
        if (!mHiggsfieldTrial) { Burst(at, shine, 16, 4.f, .22f); Burst(at, MoonWhite, 6, 3.f, .16f); }
        else
        {   // (the trial: lines of her shield's light shot out where it was struck, and sparks)
            for (int i = 0; i < 12; ++i)
                Ray(at, {Random() - .5f, Random() - .3f, Random() - .5f}, .6f + .7f * Random(), .05f, i % 3 ? shine : MoonWhite, .2f + .1f * Random(), true);
            Embers(at, facing, 10, shine, 4.f);
        }
        mNotice = L"BLOCKED"; mNoticeTimer = .5f;
        mBody->Log(world, "blocked");
        return true;
    }

    void NyxaraKit::RingEffect(const XMFLOAT3& at, float size, XMFLOAT4 color, float life)
    {
        Fx ring; ring.Kind = FxKind::Ring; ring.Pos = at; ring.Size = size; ring.Life = life; ring.Color = color;
        mFx.push_back(ring);
    }

    bool NyxaraKit::Airborne() const
    {
        const auto& b = *mBody;
        return b.WasFlying || b.LiftNow > .3f || (b.Jumping && b.JumpTime > .25f && b.JumpTime < b.Duration("Jump") - .2f) || b.Attack == "Attack_Jump";
    }

    bool NyxaraKit::Busy() const { return mBody->Moving(); }
    int NyxaraKit::AttacksBegun() const { return mBody->Begun; }

    NyxaraKit::StyleInfo NyxaraKit::Style(int style)
    {
        StyleInfo info;
        if (style < 0 || style >= static_cast<int>(Styles().size())) return info;
        const StyleSpec& spec = Styles()[static_cast<size_t>(style)];
        info.Name = spec.Name; info.Like = spec.Like;
        for (int k = 0; k < 2; ++k)
        {
            const StyleMove& m = spec.Move[k];
            info.Move[k] = m.Name; info.What[k] = m.What; info.Seconds[k] = m.Seconds; info.Free[k] = m.Free;
            info.Blow[k] = m.Hits.empty() ? 0.f : m.Hits.front().At; info.Blows[k] = static_cast<int>(m.Hits.size());
            info.Reach[k] = m.Lunge + m.Gap;
        }
        return info;
    }
    void NyxaraKit::SetAttackStyle(int style) { mCombo = -1; mStyle = style >= 0 && style < StyleCount && style < static_cast<int>(Styles().size()) ? style : -1; }
    bool NyxaraKit::StyleMade(int style) const
    {
        if (!mBody || style < 0 || style >= static_cast<int>(Styles().size())) return false;
        for (const auto& m : Styles()[static_cast<size_t>(style)].Move)
            if (mBody->Clips.find(m.Clip) == mBody->Clips.end()) return false;
        return true;
    }
    const DoorwaysSkeleton* NyxaraKit::BodySkeleton() const { return mBody ? &mBody->Skeleton : nullptr; }


    NyxaraKit::ComboInfo NyxaraKit::Combo(int combo)
    {
        ComboInfo info;
        if (combo < 0 || combo >= static_cast<int>(Combos().size())) return info;
        const ComboSpec& spec = Combos()[static_cast<size_t>(combo)];
        info.Name = spec.Name; info.What = spec.What;
        for (int k = 0; k < 2; ++k)
            for (const MoveRef r : spec.String[k])
            {
                info.String[k].push_back(RefMove(r).Name);
            }
        return info;
    }
    NyxaraKit::GuardInfo NyxaraKit::Guard(int guard)
    {
        GuardInfo info;
        if (guard < 0 || guard >= static_cast<int>(Guards().size())) return info;
        info.Name = Guards()[static_cast<size_t>(guard)].Name; info.What = Guards()[static_cast<size_t>(guard)].What;
        return info;
    }
    void NyxaraKit::SetGuardStyle(int guard) { mGuardStyle = guard >= 0 && guard < GuardCount && guard < static_cast<int>(Guards().size()) ? guard : -1; }
    bool NyxaraKit::GuardMade(int guard) const
    {   // (one that keeps her guard stance has no clip of its own)
        return mBody && guard >= 0 && guard < static_cast<int>(Guards().size()) && (!Guards()[static_cast<size_t>(guard)].Clip || mBody->Duration(GuardClip(guard)) > 0);
    }
    void NyxaraKit::SetAttackCombo(int combo)
    {
        mStyle = -1;
        mCombo = combo >= 0 && combo < ComboCount && combo < static_cast<int>(Combos().size()) ? combo : -1;
        if (mBody) mBody->ComboButton = -1;
    }
    void NyxaraKit::SetFlair(int gear, int style, bool own, bool plain)
    {
        if (gear < 0 || style < 0) { if (mFlair) mFlair->Choose(-1, -1); return; }
        if (!mFlair) { mFlair = std::make_unique<NyxaraFlair>(); mFlair->SetGround(&mFootGround); }
        mFlair->Choose(gear, style, own, plain);
    }
    int NyxaraKit::FlairGear() const { return mFlair ? mFlair->Gear() : -1; }
    int NyxaraKit::FlairStyle() const { return mFlair ? mFlair->Style() : -1; }
    bool NyxaraKit::FlairOwn() const { return mFlair && mFlair->Own(); }
    bool NyxaraKit::FlairOn() const { return mFlair && mFlair->On(); }
    bool NyxaraKit::FlairPlain() const { return mFlair && mFlair->Chosen() && mFlair->Plain(); }
    bool NyxaraKit::FlairLit() const { return mFlair && mFlair->Lit(); }
    void NyxaraKit::LetSoulGo()
    {
        if (mBody && mBody->Soul != 0) { mBody->Soul = 0; mSoulCooldown = SoulCooldownSeconds * mCooldownScale; }   // (its skill over: Q rests)
        if (mSoul) mSoul->Drop();
    }
    int NyxaraKit::SoulState() const { return mBody ? mBody->Soul : 0; }
    float NyxaraKit::SoulApart() const
    {
        return mBody && mBody->Soul != 0 ? std::hypot(mHerPlace.x - mBody->SoulAt.x, mHerPlace.z - mBody->SoulAt.z) : 0.f;
    }
    float NyxaraKit::SoulLastWay() const { return mBody ? mBody->SoulLastWay : 0.f; }
    float NyxaraKit::SoulLastBack() const { return mBody ? mBody->SoulLastBack : 0.f; }
    float NyxaraKit::SoulLastMiss() const { return mBody ? mBody->SoulLastMiss : 0.f; }
    float NyxaraKit::SoulLastHeld() const { return mBody ? mBody->SoulLastHeld : 0.f; }
    const std::string& NyxaraKit::SoulLastOutOf() const
    {
        static const std::string none;
        return mBody ? mBody->SoulOutOf : none;
    }
    int NyxaraKit::SoulQuads() const { return mSoul ? mSoul->QuadsDrawn() : 0; }
    // Her soul each frame (Update, her pose made; not while her window shows her): its own body (she as she left it,
    // held), her body now for its threads, her afterimages on her ways out and back (one every so many metres she goes:
    // none where she stands).
    // ---- her ways of going on foot (GaitStyles, before Body)
    int NyxaraKit::GaitStyleCount() { return static_cast<int>(std::size(GaitStyles)); }
    const char* NyxaraKit::GaitStyleName(int style) { return GaitStyles[std::clamp(style, 0, GaitStyleCount() - 1)].Name; }
    const char* NyxaraKit::GaitStyleLine(int style) { return GaitStyles[std::clamp(style, 0, GaitStyleCount() - 1)].Line; }
    void NyxaraKit::SetGaitStyle(int style) { mGaitStyle = std::clamp(style, 0, GaitStyleCount() - 1); }
    std::string NyxaraKit::GaitNote() const { return mBody ? mBody->SoleNote + (mBody->WingsHeldKnown ? "; wings held steady in a style" : "; wings NOT held (not learned)") : std::string(); }

    void NyxaraKit::UpdateSoul(WorldState& world, const DoorwaysSkeletonPose& pose, float dt)
    {
        auto& b = *mBody;
        const auto& p = world.MainPlayer;
        DoorwaysSkeletonPose now = pose;
        if (!EvaluateDoorwaysSkeletonPoseMatrices(b.Skeleton, now, nullptr)) return;
        const XMMATRIX place = BuildDoorwaysCharacterWorldMatrix(p.Position, p.Yaw, world.AnimatedPlayerVisual);
        XMFLOAT4X4 placed;
        XMStoreFloat4x4(&placed, place);
        const NyxaraSoul::Body her = b.SoulTies(now, place, p.Position);
        if (b.Soul != 0)
        {
            if (b.SoulLeaving) { mSoul->Leave(now, placed, her); b.SoulGhostWay = 0; b.SoulGhostWas = p.Position; }
            b.SoulLeaving = false;
            const bool going = b.Soul == 3 || (b.Soul == 1 && b.AttackTime > SoulOutFrom - .02f && b.AttackTime < SoulOutTo + .03f);
            b.SoulGhostWay += std::hypot(p.Position.x - b.SoulGhostWas.x, p.Position.y - b.SoulGhostWas.y, p.Position.z - b.SoulGhostWas.z);
            b.SoulGhostWas = p.Position;
            const float every = NyxaraSoul::GhostEvery(b.Soul == 3);
            if (!going) b.SoulGhostWay = 0;
            else if (b.SoulGhostWay >= every)
            {   // her afterimages on her way out and back
                b.SoulGhostWay = std::fmod(b.SoulGhostWay, every);
                mSoul->Ghost(now, placed, NyxaraSoul::GhostSeconds(b.Soul == 3));
            }
        }
        const float pull = b.Soul == 3 ? std::clamp(b.SoulTime / (std::max)(b.SoulBack, .01f), 0.f, 1.f) : -1.f;
        mSoul->Update(dt, her, pull);
    }
    int NyxaraKit::FlairMotes() const { return mFlair ? mFlair->Motes() : 0; }
    int NyxaraKit::FlairQuads() const { return mFlair ? mFlair->Quads() : 0; }
    bool NyxaraKit::ComboMade(int combo) const
    {
        if (!mBody || combo < 0 || combo >= static_cast<int>(Combos().size())) return false;
        for (const auto& string : Combos()[static_cast<size_t>(combo)].String)
            for (const MoveRef r : string)
                if (mBody->Clips.find(RefMove(r).Clip) == mBody->Clips.end()) return false;
        return true;
    }

    std::vector<std::string> NyxaraKit::ClipNames() const
    {
        std::vector<std::string> names;
        if (mBody) for (const auto& clip : mBody->Clips) names.push_back(clip.first);
        return names;
    }

    const std::string& NyxaraKit::ShownClip() const
    {
        static const std::string none;
        return mBody ? mBody->Current : none;
    }

    NyxaraKit::Keys NyxaraKit::ReadKeys(bool inputAllowed) const
    {
        Keys k;
        if (mInputBlocked) return k;
        if (GameSession().Unattended && mAutoKeys) return *mAutoKeys; // the journey's test pilot
        if (GameSession().Unattended)
        {   // CAUCASIAN_EAGLE_NYXARA_KEYS="<seconds>:<key>,...": each key is down for a tenth of a second from that moment
            // of her clock (L, R the mouse buttons, F, V, C, E, J for Space, Q her soul). Never reads the desktop.
            static const std::vector<std::pair<float, char>> script = []
            {
                std::vector<std::pair<float, char>> out;
                char text[1024] = {};
                if (GetEnvironmentVariableA("CAUCASIAN_EAGLE_NYXARA_KEYS", text, sizeof(text)) == 0) return out;
                char* rest = nullptr;
                for (char* item = strtok_s(text, ",", &rest); item; item = strtok_s(nullptr, ",", &rest))
                    if (const char* colon = std::strchr(item, ':')) out.push_back({static_cast<float>(std::atof(item)), colon[1]});
                return out;
            }();
            for (const auto& [at, key] : script)
                if (mBody->Clock >= at && mBody->Clock < at + .1f)
                    switch (key)
                    {
                    case 'L': k.Lmb = true; break; case 'R': k.Rmb = true; break; case 'F': k.K1 = true; break;
                    case 'V': k.K2 = true; break; case 'C': k.K3 = true; break; case 'E': k.E = true; break; case 'J': k.Space = true; break;
                    case 'Q': k.Q = true; break;
                    default: break;
                    }
            return k;
        }
        if (!inputAllowed) return k;
        const auto down = [](int vk) { return IsDoorwaysKeyDown(vk); };   // the key, or the controller button bound to it
        k.Lmb = down(VK_LBUTTON); k.Rmb = down(VK_RBUTTON); k.E = down('E'); k.K1 = down('F'); k.K2 = down('V'); k.K3 = down('C');
        k.Shift = down(VK_SHIFT); k.Space = down(VK_SPACE); k.Q = down('Q');
        return k;
    }

    float NyxaraKit::ScaleFrameTime(float realDeltaTime, bool inputAllowed, float timeScale)
    {
        (void)inputAllowed;
        if (!mReady) return realDeltaTime * timeScale;
        float scaled = realDeltaTime;
        const bool held = mHitStop > 0;   // (her flair's light goes on through it: mFlairDt)
        if (mHitStop > 0) { mHitStop -= realDeltaTime; scaled = realDeltaTime * .06f; }   // the moment a heavy blow lands
        else if (mSlow > 0)
        {   // a finishing kill's moment: slow, and back to speed evenly over its time
            mSlow -= realDeltaTime;
            const float back = 1 - std::clamp(mSlow / (std::max)(mSlowFull, 1e-3f), 0.f, 1.f);
            scaled = realDeltaTime * (mSlowRate + (1 - mSlowRate) * back * back * (3 - 2 * back));
        }
        mFlairDt = (held ? realDeltaTime : scaled) * timeScale;
        scaled *= timeScale;
        // The engine moves her on a 60 Hz grid: this frame's time is added to what was left over, and she is moved in
        // slices of 1/60 s while there is a whole one (PlayerController::UpdateWithInput, its SliceSeconds; at most 1.25 s
        // kept, 75 slices a frame). The same here, from the same frame times, so that the slices about to be taken are known.
        constexpr double Slice = static_cast<double>(1.f / 60.f);
        mGrid += (std::min)(static_cast<double>(scaled), (std::max)(0., 1.25 - mGrid));
        int slices = 0;
        while (mGrid + 1e-10 >= Slice && slices < 75) { mGrid -= Slice; ++slices; }
        if (mGrid < 1e-10) mGrid = 0;
        mBody->Slices = slices;
        const auto& b = *mBody;
        if (b.InMove() && (b.MoveOwe.x != 0 || b.MoveOwe.y != 0))
        {   // a style's move: what is left of its way, over the slices she is about to be moved by (Update: the move's way;
            // none counted, it is asked of one, in case the engine takes one the grid here did not see coming)
            auto& s = GameSession();
            const float over = static_cast<float>((std::max)(1, slices)) * (1.f / 60.f);
            s.ImpulseX = b.MoveOwe.x / over;
            s.ImpulseZ = b.MoveOwe.y / over;
        }
        return scaled;
    }

    void NyxaraKit::Burst(const XMFLOAT3& at, XMFLOAT4 color, int count, float speed, float size)
    {
        for (int i = 0; i < count; ++i)
        {
            Fx f; f.Kind = FxKind::Spark; f.Pos = at; f.Color = color;
            const float a = Random() * 2 * Pi, up = Random() * 1.4f - .3f, s = speed * (.4f + .6f * Random());
            f.Vel = Mul(Norm({std::sin(a), up, std::cos(a)}), s);
            f.Life = .35f + .45f * Random(); f.Size = size * (.6f + .7f * Random());
            mFx.push_back(f);
        }
    }

    void NyxaraKit::Ray(const XMFLOAT3& at, const XMFLOAT3& way, float length, float width, XMFLOAT4 color, float life, bool outward)
    {
        Fx f; f.Kind = FxKind::Ray; f.Pos = at; f.Vel = Norm(way); f.Size = length; f.Roll = width; f.Color = color;
        f.Life = life; f.Mirror = outward;
        mFx.push_back(f);
    }
    void NyxaraKit::Embers(const XMFLOAT3& at, const XMFLOAT3& forward, int count, XMFLOAT4 color, float speed)
    {   // hot sparks thrown on (forward) and out, falling
        for (int i = 0; i < count; ++i)
        {
            Fx f; f.Kind = FxKind::Ember; f.Pos = at; f.Color = color;
            const float a = Random() * 2 * Pi, out = .35f + .9f * Random();
            const XMFLOAT3 side = Norm(Cross(forward, {0, 1, 0}));
            const XMFLOAT3 way = Norm(Add(Add(forward, Mul(side, std::cos(a) * out)), {0, std::sin(a) * out + .25f, 0}));
            f.Vel = Mul(way, speed * (.5f + .7f * Random()));
            f.Life = .22f + .3f * Random(); f.Size = .025f + .03f * Random();
            mFx.push_back(f);
        }
    }
    // The trial FLASH STEP's light (Character Lab), after Higgsfield's video: not balls of light but lines. Along her way in,
    // speed lines of blue light laid back along it at the height of her body, a white core in some, fading in a third of a
    // second; where she set off, a flash.
    void NyxaraKit::TrialDashLight(const XMFLOAT3& from, const XMFLOAT3& to)
    {
        const XMFLOAT3 d = Sub(to, from);
        const float len = Len(d);
        if (len < .05f) return;
        const XMFLOAT3 way = Mul(d, 1 / len), back = Mul(way, -1), side = Norm(Cross(way, {0, 1, 0}));
        const int pieces = (std::max)(1, static_cast<int>(len / .16f));
        for (int i = 1; i <= pieces; ++i)
        {
            const XMFLOAT3 q = Add(from, Mul(d, static_cast<float>(i) / pieces));
            for (int k = 0; k < 2; ++k)
            {
                const XMFLOAT3 at = Add(Add(q, Mul(side, (Random() - .5f) * .5f)), {0, (Random() - .5f) * 1.3f, 0});
                Ray(at, back, .7f + .8f * Random(), .03f + .05f * Random(), Alpha(ThemeMain(), .75f), .2f + .14f * Random(), false);
            }
            if (i % 3 == 0)
                Ray(Add(q, {0, (Random() - .5f) * .6f, 0}), back, 1.4f + .8f * Random(), .025f, ThemeCore(), .18f + .1f * Random(), false);
            if (mTrialTheme == 0 && mTrialLevel > 0 && i % 2 == 0) Gusts(q, Mul(back, -1), mTrialLevel, 7.f, .6f);   // (wind drawn on with her)
            if (mTrialTheme == 1 && i % 2 == 0) Shards(Add(q, {0, (Random() - .5f) * .8f, 0}), back, 1, 1.5f);   // (frost shed as she goes)
            if (mTrialTheme == 2 && i % 2 == 0) Flames({q.x, q.y - .9f, q.z}, 1, .25f, .5f + .4f * Random());       // (fire left in her wake)
        }
    }
    // ... and where its blade bites: a white flash in a blue one, rays of light shot out round it across her way, and hot
    // sparks thrown on; on a creature twice as much, and the moment held and the view shaken (Update)
    void NyxaraKit::TrialBiteLight(const XMFLOAT3& at, const XMFLOAT3& forward, bool struck)
    {
        const float k = struck ? 1.f : .5f;
        Fx flash; flash.Kind = FxKind::Spark; flash.Pos = at; flash.Color = Alpha(ThemeMain(), .9f); flash.Life = .14f; flash.Size = 1.5f * k;
        mFx.push_back(flash);
        flash.Color = ThemeCore(); flash.Life = .1f; flash.Size = .7f * k;
        mFx.push_back(flash);
        const XMFLOAT3 side = Norm(Cross(forward, {0, 1, 0}));
        const int rays = struck ? 18 : 9;
        for (int i = 0; i < rays; ++i)
        {   // (across her way, as a wheel facing her, tipped a little on)
            const float a = XM_2PI * (static_cast<float>(i) + Random() * .7f) / rays;
            const XMFLOAT3 way = Add(Add(Mul(side, std::cos(a)), {0, std::sin(a), 0}), Mul(forward, .25f + .3f * Random()));
            Ray(at, way, (1.f + 1.4f * Random()) * (.6f + .4f * k), .05f + .05f * Random(), i % 3 ? ThemeMain() : ThemeCore(), .2f + .12f * Random(), true);
        }
        Embers(at, forward, struck ? 28 : 12, ThemeCore(), 7.f);
        Embers(at, forward, struck ? 14 : 6, ThemeMain(), 5.f);
        if (mTrialTheme == 0 && mTrialLevel > 0) Gusts(Sub(at, Mul(forward, .6f)), forward, (struck ? 5 : 3) * mTrialLevel, 9.f, .35f + .1f * mTrialLevel);
        if (mTrialTheme == 1) Shards(at, forward, struck ? 14 : 7, 6.f);
        if (mTrialTheme == 2) { Flames(Add(at, {0, -.3f, 0}), struck ? 14 : 7, .45f, .55f); Embers(at, {0, 1, 0}, struck ? 16 : 8, ThemeDeep(), 3.f); }
    }

    // ... the ground struck: lines of light shot out along it all round, spikes of it up, and hot sparks
    void NyxaraKit::TrialGroundLight(const XMFLOAT3& at, float ring, bool struck)
    {
        Fx flash; flash.Kind = FxKind::Spark; flash.Pos = Add(at, {0, .3f, 0}); flash.Color = Alpha(ThemeMain(), .85f); flash.Life = .16f; flash.Size = 1.6f;
        mFx.push_back(flash);
        const int rays = struck ? 22 : 16;
        for (int i = 0; i < rays; ++i)
        {
            const float a = XM_2PI * (static_cast<float>(i) + Random() * .6f) / rays;
            const XMFLOAT3 way{std::sin(a), .06f + .1f * Random(), std::cos(a)};
            Ray(at, way, ring * (.45f + .35f * Random()), .06f + .05f * Random(), i % 4 ? ThemeMain() : ThemeCore(), .3f + .15f * Random(), true);
        }
        for (int i = 0; i < 7; ++i)
        {
            const float a = XM_2PI * Random(), d = .3f + .9f * Random();
            Ray({at.x + std::sin(a) * d, at.y, at.z + std::cos(a) * d}, {(Random() - .5f) * .4f, 1, (Random() - .5f) * .4f},
                .9f + 1.2f * Random(), .06f, i % 2 ? ThemeMain() : ThemeCore(), .3f + .12f * Random(), true);
        }
        Embers(at, {0, 1, 0}, struck ? 30 : 18, ThemeCore(), 6.f);
        Embers(at, {0, 1, 0}, 12, ThemeMain(), 4.f);
        if (mTrialTheme == 0 && mTrialLevel > 0)
            for (int i = 0; i < 6 * mTrialLevel; ++i)
            {   // (wind blown out over the ground all round)
                const float a = XM_2PI * Random();
                Gusts(Add(at, {std::sin(a) * .5f, .25f + .3f * Random(), std::cos(a) * .5f}), {std::sin(a), .05f, std::cos(a)}, 1, 8.f + 2.f * mTrialLevel, .1f);
            }
        if (mTrialTheme == 1) { IceSpikes(at, ring * .55f, struck ? 16 : 12); Shards(Add(at, {0, .2f, 0}), {0, 1, 0}, 14, 5.f); }
        if (mTrialTheme == 2) { for (int i = 0; i < 18; ++i) { const float a = XM_2PI * i / 18.f, d = ring * (.2f + .3f * Random()); Flames({at.x + std::sin(a) * d, at.y, at.z + std::cos(a) * d}, 1, .1f, .8f + .6f * Random()); } Flames(at, 8, .4f, 1.3f); }
    }
    // ... a turn with her blades out: short lines of light along the way her blades go, either side of her, each a moment
    void NyxaraKit::TrialWhirlLight(const XMFLOAT3& middle, float yaw, bool right)
    {
        const XMFLOAT3 fwd = Forward(yaw), side = Norm(Cross(fwd, {0, 1, 0}));
        for (int k = -1; k <= 1; k += 2)
        {
            const XMFLOAT3 at = Add(middle, Add(Mul(side, 1.1f * static_cast<float>(k)), {0, (Random() - .5f) * .25f, 0}));
            const XMFLOAT3 way = Mul(fwd, static_cast<float>(k) * (right ? 1.f : -1.f));
            Ray(Sub(at, Mul(way, .35f)), way, .7f, .05f, k > 0 ? ThemeCore() : ThemeMain(), .2f, false);
            if (mTrialTheme == 0 && mTrialLevel > 0) Gusts(at, way, mTrialLevel, 7.f, .2f);
            if (mTrialTheme == 1 && Random() < .35f) Shards(at, way, 1, 2.f);
            if (mTrialTheme == 2 && Random() < .5f) Flames(at, 1, .05f, .35f);
        }
    }
    // ... and its blow: lines of light shot out level all round her, and hot sparks
    void NyxaraKit::TrialWhirlBite(const XMFLOAT3& middle, bool struck)
    {
        const int rays = struck ? 20 : 14;
        for (int i = 0; i < rays; ++i)
        {
            const float a = XM_2PI * (static_cast<float>(i) + Random() * .6f) / rays;
            const XMFLOAT3 way{std::sin(a), (Random() - .5f) * .15f, std::cos(a)};
            Ray(Add(middle, Mul(way, .5f)), way, 1.f + 1.f * Random(), .05f + .04f * Random(), i % 3 ? ThemeMain() : ThemeCore(), .22f + .1f * Random(), true);
        }
        for (int i = 0; i < 4; ++i)
        {
            const float a = XM_2PI * Random();
            Embers(Add(middle, {std::sin(a) * 1.1f, 0, std::cos(a) * 1.1f}), {std::sin(a), 0, std::cos(a)}, struck ? 7 : 4, ThemeCore(), 6.f);
            if (mTrialTheme == 0 && mTrialLevel > 0) Gusts(Add(middle, {std::sin(a) * 1.1f, 0, std::cos(a) * 1.1f}), {std::cos(a), 0, -std::sin(a)}, 2 * mTrialLevel, 8.f, .25f);
            if (mTrialTheme == 1) Shards(Add(middle, {std::sin(a) * 1.1f, 0, std::cos(a) * 1.1f}), {std::sin(a), .2f, std::cos(a)}, 4, 6.f);
            if (mTrialTheme == 2) Flames(Add(middle, {std::sin(a) * 1.1f, -.2f, std::cos(a) * 1.1f}), 3, .2f, .5f);
        }
    }

    int NyxaraKit::ThemeMore(int count) const { return static_cast<int>(static_cast<float>(count) * (1.f + .5f * mTrialLevel) + .5f); }
    float NyxaraKit::Bigger() const { return 1.f + .15f * mTrialLevel; }
    void NyxaraKit::Gusts(const XMFLOAT3& at, const XMFLOAT3& way, int count, float speed, float spread)
    {   // lines of wind blown along way, spread about at, pale; more and longer as her gear grows
        const XMFLOAT3 w = Len(way) < 1e-3f ? XMFLOAT3{0, 0, 1} : Norm(way);
        for (int i = 0; i < count; ++i)
        {
            Fx f; f.Kind = FxKind::Gust; f.Color = Random() < .5f ? XMFLOAT4{.62f, .85f, 1.f, 1} : XMFLOAT4{.4f, .7f, 1.f, 1};
            f.Pos = Add(at, {(Random() - .5f) * 2 * spread, (Random() - .5f) * 2 * spread, (Random() - .5f) * 2 * spread});
            const XMFLOAT3 bent = Norm(Add(w, {(Random() - .5f) * .35f, (Random() - .5f) * .25f, (Random() - .5f) * .35f}));
            f.Vel = Mul(bent, speed * (.7f + .6f * Random()));
            f.Size = (.8f + .9f * Random()) * (.7f + .2f * mTrialLevel); f.Roll = .02f + .025f * Random(); f.Life = .3f + .25f * Random();
            mFx.push_back(f);
        }
    }
    XMFLOAT4 NyxaraKit::ThemeMain() const { return mTrialTheme == 1 ? XMFLOAT4{.55f, .9f, 1.f, 1} : mTrialTheme == 2 ? XMFLOAT4{1.f, .45f, .08f, 1} : TrialBlue; }
    XMFLOAT4 NyxaraKit::ThemeCore() const { return mTrialTheme == 1 ? XMFLOAT4{.93f, .99f, 1.f, 1} : mTrialTheme == 2 ? XMFLOAT4{1.f, .78f, .35f, 1} : MoonWhite; }
    XMFLOAT4 NyxaraKit::ThemeDeep() const { return mTrialTheme == 1 ? XMFLOAT4{.3f, .6f, 1.f, 1} : mTrialTheme == 2 ? XMFLOAT4{.95f, .18f, .04f, 1} : TrialBlue; }
    void NyxaraKit::Shards(const XMFLOAT3& at, const XMFLOAT3& forward, int count, float speed)
    {   // crystals of ice thrown on and out, turning as they fall
        const XMFLOAT3 side = Norm(Cross(Len(Cross(forward, {0, 1, 0})) < 1e-3f ? XMFLOAT3{1, 0, 0} : forward, {0, 1, 0}));
        count = ThemeMore(count);
        for (int i = 0; i < count; ++i)
        {
            Fx f; f.Kind = FxKind::Shard; f.Pos = at; f.Color = Random() < .5f ? ThemeMain() : ThemeDeep();
            const float a = Random() * 2 * Pi, out = .4f + .8f * Random();
            const XMFLOAT3 way = Norm(Add(Add(forward, Mul(side, std::cos(a) * out)), {0, std::sin(a) * out + .3f, 0}));
            f.Vel = Mul(way, speed * (.4f + .7f * Random()));
            f.Life = .45f + .4f * Random(); f.Size = (.12f + .16f * Random()) * Bigger(); f.Roll = Random() * XM_2PI; f.Yaw = (Random() - .5f) * 14.f;
            mFx.push_back(f);
        }
    }
    void NyxaraKit::IceSpikes(const XMFLOAT3& at, float reach, int count)
    {   // crystals of ice up out of the ground round where it was struck, leaning out, standing a moment and melting away
        count = ThemeMore(count); reach *= Bigger();
        for (int i = 0; i < count; ++i)
        {
            const float a = XM_2PI * (static_cast<float>(i) + Random() * .7f) / count, d = reach * (.35f + .65f * Random());
            Fx f; f.Kind = FxKind::Shard; f.Stuck = true; f.Color = Random() < .5f ? ThemeMain() : ThemeDeep();
            f.Pos = {at.x + std::sin(a) * d, at.y, at.z + std::cos(a) * d};
            f.Vel = Norm(XMFLOAT3{std::sin(a) * .45f, 1, std::cos(a) * .45f});
            f.Size = (.35f + .55f * Random()) * (1.2f - .5f * d / (std::max)(.1f, reach)) * Bigger(); f.Life = .9f + .5f * Random();
            mFx.push_back(f);
        }
    }
    void NyxaraKit::Flames(const XMFLOAT3& at, int count, float spread, float size)
    {   // tongues of fire rising, flickering, yellow at their birth and red as they go
        count = ThemeMore(count); size *= Bigger(); spread *= Bigger();
        for (int i = 0; i < count; ++i)
        {
            Fx f; f.Kind = FxKind::Flame;
            f.Pos = Add(at, {(Random() - .5f) * 2 * spread, Random() * .2f, (Random() - .5f) * 2 * spread});
            f.Vel = {(Random() - .5f) * .8f, .8f + 1.4f * Random(), (Random() - .5f) * .8f};
            f.Life = .35f + .35f * Random(); f.Size = size * (.7f + .6f * Random()); f.Roll = Random() * 100.f;
            mFx.push_back(f);
        }
    }

    void NyxaraKit::DamageWraith(int index, int damage, const XMFLOAT3& from, float push, float launch)
    {
        auto& w = mWraiths[index];
        if (!w.Alive) return;
        if (w.Doom > 0)
        {   // doomed: it is hers to finish, and a blow only shows on it
            w.Flash = .16f;
            mShake = (std::max)(mShake, .03f);
            CaucasianEagle::Sound::Play("nyxara/cut", .7f, .08f);
            return;
        }
        damage = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(damage) * mDamageScale * w.Weak)));
        // A blow that would kill a creature she may finish, near her and with her on foot, dooms it instead: one at a
        // time (another killed meanwhile dies as before), and not in a training session (there the lab dooms one: Doom)
        bool dooms = w.Finishable && !mTrainingMode && w.Health - damage <= 0 && mHerAfoot;
        if (dooms)
        {
            const float feet = w.Pos.y - 1.3f + w.Lift;
            dooms = std::hypot(w.Pos.x - mHerPlace.x, w.Pos.z - mHerPlace.z) - w.Radius <= FinishReach && std::abs(feet - mHerPlace.y) <= FinishHigh;
            for (const auto& o : mWraiths) dooms = dooms && !(o.Alive && o.Doom > 0);
            if (dooms)
            {   // ... and by chance (SetFinishChance: half of them; the owner, 2026-11-26: "make finishing kill of each
                // monster randomly with 50% chance of happening"): the others die of this blow itself
                static const float asked = [] { char v[32]{}; size_t n = 0; return getenv_s(&n, v, "CAUCASIAN_EAGLE_NYXARA_FINISH") == 0 && n > 1 ? static_cast<float>(std::atof(v)) : -1.f; }();
                dooms = Random() < (asked >= 0 ? asked : mFinishChance);
            }
        }
        if (!mTrainingMode) w.Health -= damage;
        else ++mTrainingTargetHits;
        w.Flash = .16f;
        XMFLOAT3 away = Sub(w.Pos, from); away.y = 0; away = Norm(away);
        if (!dooms && !w.Pinned && w.Hold <= 0)
        {
            w.Push = Add(w.Push, Mul(away, push * w.PushScale));
            if (launch > 0 && w.PushScale > .5f) w.LiftVel = (std::max)(w.LiftVel, launch);
        }
        const float heavy = std::clamp(static_cast<float>(damage) / 120.f, 0.f, 1.f);
        mShake = (std::max)(mShake, .02f + .08f * heavy);
        if (damage >= HeavyBlow) mHitStop = (std::max)(mHitStop, .04f);
        CaucasianEagle::Sound::Play("nyxara/cut", .85f + .15f * heavy, .08f);   // the 'meat heavy' the user chose in the lab
        if (dooms)
        {   // it stops where it stands, dazed, its head hanging, and waits for her (UpdateDoomed, UpdateFinish)
            w.Health = 1; w.Doom = DoomSeconds; w.RoomLooked = false;
            mHitStop = (std::max)(mHitStop, .07f);
            mShake = (std::max)(mShake, .14f);
            CaucasianEagle::Sound::Play("combat/blow", .8f, .1f);
            CaucasianEagle::Sound::Play("creatures/hurt", .8f, .12f);
            return;
        }
        if (w.Health > 0 && Random() < .4f) CaucasianEagle::Sound::Play("creatures/hurt", .6f, .15f);
        if (w.Health <= 0) StrikeDown(index);
    }

    void NyxaraKit::StrikeDown(int index)
    {
        if (index < 0 || index >= static_cast<int>(mWraiths.size())) return;
        auto& w = mWraiths[index];
        if (!w.Alive) return;
        KillWraith(index, 0);
        if (mFlair && mFlair->On())
        {   // (her flair: a kill's light is its own, here as in a finisher)
            NyxaraFlair::Kill kill;
            const XMFLOAT3 feet = FeetOf(w);
            XMFLOAT3 way{feet.x - mHerPlace.x, 0, feet.z - mHerPlace.z};
            way = Len(way) > 1e-3f ? Norm(way) : XMFLOAT3{0, 0, 1};
            kill.It = {feet, w.Height, w.Radius}; kill.Way = way;
            kill.At = {feet.x - way.x * .6f * w.Radius, feet.y + .55f * w.Height, feet.z - way.z * .6f * w.Radius};
            mFlair->Finish(kill);
            return;
        }
        Burst(w.Pos, ShadeViolet, 26, 6.f, .35f);
        Fx ring; ring.Kind = FxKind::Ring; ring.Pos = {w.Pos.x, w.Pos.y - w.Lift - 1.2f, w.Pos.z}; ring.Size = 2.2f;
        ring.Life = .6f; ring.Color = ShadeViolet; mFx.push_back(ring);
    }

    void NyxaraKit::KillWraith(int index, float linger)
    {
        if (index < 0 || index >= static_cast<int>(mWraiths.size())) return;
        auto& w = mWraiths[index];
        if (!w.Alive) return;
        w.Alive = false; w.Health = 0; w.Doom = 0;
        w.Linger = (std::max)(0.f, linger); w.Respawn = 8.f + w.Linger;
    }

    void NyxaraKit::Doom(int index)
    {
        if (index < 0 || index >= static_cast<int>(mWraiths.size())) return;
        auto& w = mWraiths[index];
        if (!w.Alive || !w.Finishable || w.Doom > 0) return;
        w.Health = 1; w.Doom = DoomSeconds; w.Flash = .16f; w.RoomLooked = false;
        mShake = (std::max)(mShake, .1f);
        CaucasianEagle::Sound::Play("combat/blow", .8f, .1f);
    }

    int NyxaraKit::Finishing() const { return mBody ? mBody->Finish : 0; }
    const wchar_t* NyxaraKit::FinishName(int which) { return which >= 1 && which <= Finishers ? Finishes[which].Name : L""; }
    void NyxaraKit::SetFinisher(int which) { mFinisher = which >= 1 && which <= Finishers ? which : 0; }
    bool NyxaraKit::FinishMade(int which) const { return mBody && which >= 1 && which <= Finishers && mBody->Duration(Finishes[which].Clip) > 0; }

    void NyxaraKit::LetFall(const Fallen& fall)
    {
        if (fall.Index < 0 || fall.Index >= static_cast<int>(mWraiths.size())) return;
        std::erase_if(mFallen, [&](const Fallen& f) { return f.Index == fall.Index; });
        mFallen.push_back(fall);
    }

    void NyxaraKit::UpdateDoomed(float dt)
    {
        const auto& b = *mBody;
        for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
        {
            auto& w = mWraiths[i];
            if (w.Doom <= 0) continue;
            if (!w.Alive) { w.Doom = 0; continue; }          // (killed another way: as a pack falls with its master)
            w.Pinned = true;
            if (b.Finish && b.FinishTarget == i) continue;   // hers now (UpdateFinish)
            w.Doom -= dt;
            const float dazed = std::clamp((DoomSeconds - w.Doom) / .25f, 0.f, 1.f);
            w.Daze = dazed * dazed * (3 - 2 * dazed);
            if (w.Doom > 0) continue;
            // not finished in time: it falls on its own, on its knees and over on its face
            Fallen f;
            f.Index = i; f.Feet = FeetOf(w); f.Neck = NeckFrom(w); f.Way = Forward(w.Yaw);
            f.Wait = DoomKneel; f.Takes = DoomFall; f.KneelFrom = 0; f.Kneel = 1; f.Daze = 1; f.Knee = w.Knee;
            KillWraith(i, f.Wait + f.Takes + FallenLies);
            f.Until = w.Linger + .65f;
            LetFall(f);
            CaucasianEagle::Sound::Play("creatures/hurt", .7f, .15f);
        }
        // the fallen: down on their knees, over, the thud, and they lie
        for (auto& f : mFallen)
        {
            f.Time += dt;
            if (f.Index < 0 || f.Index >= static_cast<int>(mWraiths.size()) || mWraiths[f.Index].Alive) { f.Until = -1; continue; }   // (its place has gone to another)
            auto& w = mWraiths[f.Index];
            const float give = f.Wait > 1e-3f ? std::clamp(f.Time / f.Wait, 0.f, 1.f) : 1.f;
            const float knees = f.KneelFrom + (f.Kneel - f.KneelFrom) * give * give * (3 - 2 * give);
            const float over = std::clamp((f.Time - f.Wait) / (std::max)(f.Takes, 1e-3f), 0.f, 1.f);
            float tilt = FallenTilt * over * over;   // (as a thing tipping over gathers speed)
            if (over >= 1)
            {   // down: it gives a little and lies
                tilt = FallenTilt - .07f * std::sin(XM_PI * std::clamp((f.Time - f.Wait - f.Takes) / .22f, 0.f, 1.f));
                if (!f.Down)
                {
                    f.Down = true;
                    const XMFLOAT3 lies = Add(f.Feet, Mul(Norm(f.Way), .5f * Len(f.Neck)));   // (the middle of it, on the ground)
                    const float big = std::clamp(w.Height / 2.f, .5f, 2.6f);
                    RingEffect({lies.x, f.Feet.y + .05f, lies.z}, .8f * big + .5f, {.42f, .39f, .35f, 1}, .4f);
                    Burst({lies.x, f.Feet.y + .2f, lies.z}, {.5f, .46f, .4f, 1}, 8 + static_cast<int>(6 * big), 1.6f + big, .25f);
                    mShake = (std::max)(mShake, .05f + .05f * big);
                    CaucasianEagle::Sound::Play("combat/blow", (std::min)(1.f, .4f + .2f * big), .1f);
                }
            }
            // about its knees as far as it kneels (its body let down onto them), about its feet otherwise
            const float drop = Wraith::KneelDrop(f.Knee, knees), pivotUp = f.Knee * knees;
            const XMVECTOR up = XMVectorSet(0, 1, 0, 0);
            XMVECTOR axis = XMVector3Cross(up, XMVectorSet(f.Way.x, 0, f.Way.z, 0));
            axis = XMVectorGetX(XMVector3LengthSq(axis)) < 1e-8f ? XMVectorSet(1, 0, 0, 0) : XMVector3Normalize(axis);
            const XMMATRIX turn = XMMatrixRotationAxis(axis, tilt);
            const XMVECTOR pivot = XMLoadFloat3(&f.Feet) + up * (pivotUp - drop);
            const XMVECTOR feet = pivot - XMVector3TransformNormal(up, turn) * pivotUp;
            XMStoreFloat3(&w.HeldFeet, feet);
            XMStoreFloat3(&w.HeldNeck, feet + XMVector3TransformNormal(XMLoadFloat3(&f.Neck), turn));
            w.Hold = 1; w.Pinned = true;
            w.Kneel = knees * (std::max)(0.f, 1 - tilt / XM_PIDIV2);   // (its legs straighten as it goes over)
            // limp as the dying are (CreatureRig's death pose is a fall onto its back: going on its face, less of it)
            const bool onFace = f.Way.x * std::sin(w.Yaw) + f.Way.z * std::cos(w.Yaw) > 0;
            w.Limp = (onFace ? .4f : .85f) * over * over * (3 - 2 * over);
            w.Daze = f.Daze * (1 - over);
        }
        std::erase_if(mFallen, [](const Fallen& f) { return f.Until < 0 || f.Time >= f.Until; });
    }

    // ------------------------------------------------------------------ her finishing kills
    // Each is a made clip (MakeFinishes) and a plan of where she goes, how she faces and how high she is lifted over its
    // time (FinishPlan), worked out as it begins from where she and it are, how big it is, and where her blades are in
    // the clip: so that the blades go where the creature is, whatever its size. The engine moves her (the session's
    // impulse) to the plan's place a frame ahead; her picture is lifted (FinishLiftNow); the clip is played by the
    // finisher's clock. Only one whose way has room is played (FinishFits: ground under it all the way, nothing in it;
    // 2026-11-20: on Map A she goes over any edge, and a way past a creature at an edge would take her over it): one at
    // random, never the one she played last; where it has no room its other ways (the cross: past it on her other side,
    // round to beside it), then the others. Where none has room as it is doomed, her blow kills it at once (StrikeDown:
    // as before her finishers); where room was there then and is gone when her move ends, it collapses (its doom cut
    // to 0.3 s; FinishRetry keeps her from looking again before it has).
    void NyxaraKit::StartFinish(WorldState& world, int index)
    {
        auto& b = *mBody;
        auto& w = mWraiths[index];
        // CAUCASIAN_EAGLE_NYXARA_FINISHER=<1|2|3>: that one first every time; CAUCASIAN_EAGLE_NYXARA_FINISH_SIDE=<1|-1>:
        // the cross past it on her right or her left first (tests)
        static const int forced = []
        {
            char v[8]{}; size_t n = 0;
            return getenv_s(&n, v, "CAUCASIAN_EAGLE_NYXARA_FINISHER") == 0 && n > 1 ? std::clamp(std::atoi(v), 0, Finishers) : 0;
        }();
        static const int forcedSide = []
        {
            char v[8]{}; size_t n = 0;
            return getenv_s(&n, v, "CAUCASIAN_EAGLE_NYXARA_FINISH_SIDE") == 0 && n > 1 ? std::clamp(std::atoi(v), -1, 1) : 0;
        }();
        const auto named = [](int kind, float to, bool backOf) -> const char*
        {   // (the cross's way, for the log)
            if (kind != 1) return "";
            return to > 0 ? (backOf ? " right behind" : " right beside") : (backOf ? " left behind" : " left beside");
        };
        // first the one chosen (SetFinisher; the test's), or with none one of those kept at random; then the others kept
        // at random, the one she played last, last
        const int chosen = forced ? forced : mFinisher;
        std::vector<int> order;
        for (const int k : Kept) order.push_back(k);
        for (size_t i = order.size(); i > 1; --i) std::swap(order[i - 1], order[(std::min)(i - 1, static_cast<size_t>(Random() * static_cast<float>(i)))]);
        const auto move = [&](int k, bool front)
        {
            std::erase(order, k);
            if (front) order.insert(order.begin(), k); else order.push_back(k);
        };
        if (b.FinishLast >= 1 && b.FinishLast <= Finishers) move(b.FinishLast, false);
        if (chosen >= 1 && chosen <= Finishers) move(chosen, true);
        const float side = forcedSide ? static_cast<float>(forcedSide) : Random() < .5f ? 1.f : -1.f;
        struct Way { int Which; float Side; bool Behind; };
        std::vector<Way> ways;
        for (const int kind : order)
        {
            if (kind == 1)
            {
                ways.push_back({1, side, true}); ways.push_back({1, -side, true});
                ways.push_back({1, side, false}); ways.push_back({1, -side, false});
            }
            else if (More[kind].Past > 0) { ways.push_back({kind, side, true}); ways.push_back({kind, -side, true}); }
            else ways.push_back({kind, 1, true});
        }
        const int count = static_cast<int>(ways.size());
        std::string misfits;
        const auto began = std::chrono::steady_clock::now();
        bool found = false;
        for (int k = 0; k < count && !found; ++k)
        {
            const Way& way = ways[k];
            if (!SetUpFinish(world, index, way.Which, way.Side, way.Behind)) continue;
            float at = 0; std::string why;
            found = FinishFits(world, at, why);
            if (found) break;
            char said[160];
            std::snprintf(said, sizeof said, "%s%s %s at %.2f s; ", Finishes[way.Which].Clip, named(way.Which, way.Side, way.Behind), why.c_str(), at);
            misfits += said;
        }
        const double took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
        if (!found)
        {   // (room was there as it was doomed, and is gone: it does not wait out its three seconds, it collapses)
            w.Doom = (std::min)(w.Doom, .3f);
            b.Finish = 0; b.FinishTarget = -1; b.FinishRetry = .5f;
            char said[48];
            std::snprintf(said, sizeof said, " (looked in %.1f ms)", took);
            b.Log(world, ("finish none has room: " + misfits + said).c_str());
            return;
        }
        const int which = b.Finish;
        const auto& spec = Finishes[which];
        b.FinishLast = which;
        b.Attack.clear(); b.Queued = 0; b.Jumping = false; b.Step = 0;
        w.Pinned = true; w.Push = {};
        mNotice = spec.Name; mNoticeTimer = 1.2f;
        CaucasianEagle::Sound::Play("nyxara/effort", .8f, .08f);
        {
            char said[720];
            const XMFLOAT3 F = b.FinishFeet, u = b.FinishWay, r = b.FinishRight;
            const XMFLOAT3 a = Sub(b.FinishA, F), k = Sub(b.FinishStrike, F);
            std::snprintf(said, sizeof said, "finish %s%s at %d height %.2f neck %.2f knee %.2f radius %.2f depth %.2f width %.2f | blow %.3f death %.3f "
                          "lift %.2f kneel %.2f | A %.2f %.2f (along, right) strike %.2f %.2f %.2f | room looked at in %.1f ms%s%s",
                          spec.Clip, named(which, b.FinishSide, b.FinishBehind), index, b.FinishHeight, (std::max)(.3f, b.FinishNeck.y), b.FinishKnee,
                          b.FinishRadius, b.FinishDepth, w.Width, b.FinishBlowAt[0], b.FinishDeathAt, b.FinishLiftMost, b.FinishKneelTo,
                          a.x * u.x + a.z * u.z, a.x * r.x + a.z * r.z, k.x * u.x + k.z * u.z, k.x * r.x + k.z * r.z, k.y, took,
                          misfits.empty() ? "" : " | no room: ", misfits.c_str());
            b.Log(world, said);
        }
    }

    bool NyxaraKit::AnyFinishFits(WorldState& world, int index, std::string* why)
    {
        auto& b = *mBody;
        if (b.Finish) return true;   // (she finishes one already: not hers to look at now)
        bool fits = false;
        for (const int which : Kept)
            for (int k = 0; k < (which == 1 ? 4 : More[which].Past > 0 ? 2 : 1) && !fits; ++k)
            {
                if (!SetUpFinish(world, index, which, k % 2 ? -1.f : 1.f, k < 2)) continue;
                float at = 0; std::string said;
                fits = FinishFits(world, at, said);
                if (!fits && why)
                {
                    char line[160];
                    std::snprintf(line, sizeof line, "%s%s%s %s at %.2f s; ", Finishes[which].Clip, which != 1 ? "" : k % 2 ? " left" : " right",
                                  which != 1 ? "" : k < 2 ? " behind" : " beside", said.c_str(), at);
                    *why += line;
                }
            }
        b.Finish = 0; b.FinishTarget = -1;
        if (!fits && why && world.RuntimeScene)
        {   // (and where she stands: in a wall already, every way of hers would seem blocked)
            const auto& p = world.MainPlayer;
            const auto* in = FindDoorwaysRuntimeSceneBlockingProxyForPlayer(*world.RuntimeScene, p.Position, p.CollisionRadius, p.CollisionHeight);
            *why += in ? "where she stands: in " + in->Name : std::string("where she stands: clear");
        }
        return fits;
    }

    bool NyxaraKit::CanDoom(int index) const
    {
        if (index < 0 || index >= static_cast<int>(mWraiths.size())) return false;
        const auto& w = mWraiths[index];
        if (!w.Alive || !w.Finishable || w.Doom > 0 || !mHerAfoot) return false;
        const float feet = w.Pos.y - 1.3f + w.Lift;
        return std::hypot(w.Pos.x - mHerPlace.x, w.Pos.z - mHerPlace.z) - w.Radius <= FinishReach && std::abs(feet - mHerPlace.y) <= FinishHigh;
    }

    // One finisher set up in her body to be played on that creature from where she is ('side' 1 her right, -1 her left,
    // and 'behind': the cross's way past it); false where its clip is not made.
    bool NyxaraKit::SetUpFinish(WorldState& world, int index, int which, float side, bool behind)
    {
        auto& b = *mBody;
        const auto& p = world.MainPlayer;
        const auto& w = mWraiths[index];
        const auto& spec = Finishes[which];
        if (b.FinishTipAt[which][0].empty() || b.FinishTipAt[which][1].empty() || b.Duration(spec.Clip) <= 0) return false;
        const float tall = (std::max)(.01f, world.AnimatedPlayerVisual.Scale.y);
        const XMFLOAT3 S = p.Position, F = FeetOf(w), neck = NeckFrom(w);
        XMFLOAT3 u{F.x - S.x, 0, F.z - S.z};
        const float d0 = std::hypot(u.x, u.z);
        u = d0 > 1e-3f ? XMFLOAT3{u.x / d0, 0, u.z / d0} : Forward(p.Yaw);
        const XMFLOAT3 r{u.z, 0, -u.x};   // her right as she faces it
        const float R = w.Radius, H = w.Height, N = (std::max)(.3f, neck.y), K = std::clamp(w.Knee, .1f, .5f * H);
        b.Finish = which; b.FinishTarget = index; b.FinishTime = 0; b.FinishSide = side; b.FinishBehind = behind;
        b.FinishBlown[0] = b.FinishBlown[1] = false; b.FinishKilled = b.FinishLanded = b.FinishRipped = false;
        for (bool& hit : b.FinishHit) hit = false;
        b.FinishMarks = 0; b.FinishLitAny = false;
        b.FinishBlowAt[0] = spec.Blow[0]; b.FinishBlowAt[1] = spec.Blow[1]; b.FinishDeathAt = spec.Death;
        b.FinishFrom = S; b.FinishFeet = F; b.FinishWay = u; b.FinishRight = r; b.FinishNeck = neck;
        b.FinishHeight = H; b.FinishRadius = R; b.FinishKnee = K; b.FinishTall = tall; b.FinishDepth = w.Depth > 0 ? w.Depth : 1.5f * R;
        b.FinishYawFrom = p.Yaw; b.FinishYawTo = b.FinishYawEnd = std::atan2(u.x, u.z);
        b.FinishTheirYawFrom = w.Yaw; b.FinishTheirYawTo = std::atan2(-u.x, -u.z);
        b.FinishLiftMost = b.FinishLiftNow = b.FinishKneel = b.FinishKneelTo = 0; b.FinishGo = {0, 0}; b.FinishStrike = F;
        if (which == 1)
        {   // Talon Cross: in to cutting reach, two cuts crossing on it, and past it round its side ('side') to stand
            // behind it, or (where there is no room behind it) round to beside it: out to the side from before it, and
            // coming to beside it going on past it
            const XMFLOAT3 rs = Mul(r, side);
            b.FinishA = Add(S, Mul(u, (std::max)(d0 - (R + .9f), -.5f)));
            if (behind) { b.FinishB = Add(F, Mul(rs, 2 * (R + .55f))); b.FinishC = Add(Add(F, Mul(u, R + 1.7f)), Mul(rs, .2f)); }
            else { b.FinishC = Add(Add(F, Mul(rs, R + 1.f)), Mul(u, .2f)); b.FinishB = Sub(b.FinishC, Mul(u, .7f)); }
            b.FinishB.y = b.FinishC.y = S.y;
            b.FinishLiftMost = std::clamp(.5f * H - 1.4f, 0.f, 1.4f);   // (at a tall one she leaps into the cuts)
        }
        else if (which == 2)
        {   // Eagle's Plunge: her left blade's point goes into it from the front, under its neck, as the blade comes
            // down to that height (and she is lifted to it, at a tall one)
            const float high = N - .1f * H;
            b.FinishAnchor = Add(Sub(F, Mul(u, (std::max)(R * .7f - .25f, 0.f))), {0, high, 0});
            const auto& tipL = b.FinishTipAt[2][0];
            float contact = spec.Blow[0];
            for (int k = 0; k <= 15; ++k)
            {
                contact = spec.Blow[0] + static_cast<float>(k) / 60.f;
                if (Body::FlipAt(tipL, contact).y * tall <= high) break;
            }
            b.FinishBlowAt[0] = b.FinishDeathAt = contact;
            const XMFLOAT3 tip = Mul(Body::FlipAt(tipL, contact), tall);
            b.FinishA = {b.FinishAnchor.x - r.x * tip.x - u.x * tip.z, S.y, b.FinishAnchor.z - r.z * tip.x - u.z * tip.z};
            b.FinishLiftMost = (std::max)(0.f, high - tip.y);
            b.FinishLand = Sub(F, Mul(u, .5f * R + .9f)); b.FinishLand.y = S.y;   // (before its feet: clear of it lying)
        }
        else if (which == 3)
        {   // Gale Slide: past it on its right, low, her blade through its legs; it goes down on its knees; she rises
            // and her blade comes down behind her through its neck. She strikes from beside it and behind it (the way
            // she slid), outside its body: an ellipse about its middle, its back half its depth behind it (+u: it faces
            // the way she came) and its shoulders Width to either side. Where her blade cannot reach its neck from out
            // there (a big one: the Uaig's arms are 1.6 m from its neck, her blade's point 0.9 m from her) it goes into
            // its shoulder or its back short of the neck, as much lower as its top slopes. (2026-11-20: struck from
            // beside its neck, at the Uaig she stood inside its chest.)
            const auto& tipR = b.FinishTipAt[3][1];
            float low = 1e9f;
            for (int k = 0; k <= 10; ++k) low = (std::min)(low, Body::FlipAt(tipR, 1.50f + static_cast<float>(k) / 60.f).y * tall);
            float kneel = 1;   // down on its knees; a small one no lower than her blade comes
            if (N - Wraith::KneelDrop(K, 1) < low)
                kneel = std::clamp(std::acos(std::clamp(1 - (N - low) / K, -1.f, 1.f)) / Wraith::KneelShin, .3f, 1.f);
            b.FinishKneelTo = kneel;
            const float hk = N - Wraith::KneelDrop(K, kneel);
            const auto choose = [&](float high)
            {   // the moment her blade comes down nearest to that height
                float kill = 1.50f, miss = 1e9f;
                for (int k = 0; k <= 10; ++k)
                {
                    const float t = 1.50f + static_cast<float>(k) / 60.f, off = std::abs(Body::FlipAt(tipR, t).y * tall - high) + .3f * std::abs(t - 1.55f);
                    if (off < miss) { miss = off; kill = t; }
                }
                return kill;
            };
            // from its neck to her as she strikes: behind it and to its side, a big one more to the side (from behind
            // her, as the camera mostly is, it stood between the camera and her: 30 degrees off behind it, 65 at a giant)
            const float aside = XMConvertToRadians(30.f + 35.f * std::clamp((H - 2.5f) / 2.f, 0.f, 1.f));
            const XMFLOAT3 way = Norm({u.x * std::cos(aside) + r.x * std::sin(aside), 0, u.z * std::cos(aside) + r.z * std::sin(aside)});
            float clear = 0;
            {   // from its neck out along 'way' to the edge of its body
                const float Dh = (std::max)(.25f, .5f * b.FinishDepth), Wh = (std::max)(.25f, w.Width > 0 ? w.Width : R);
                const float nu = neck.x * u.x + neck.z * u.z, nr = neck.x * r.x + neck.z * r.z;
                const float wu = way.x * u.x + way.z * u.z, wr = way.x * r.x + way.z * r.z;
                const float qa = wu * wu / (Dh * Dh) + wr * wr / (Wh * Wh), qb = 2 * (nu * wu / (Dh * Dh) + nr * wr / (Wh * Wh));
                const float qc = nu * nu / (Dh * Dh) + nr * nr / (Wh * Wh) - 1;
                clear = (std::max)(0.f, (-qb + std::sqrt((std::max)(0.f, qb * qb - 4 * qa * qc))) / (2 * (std::max)(qa, 1e-6f)));
            }
            float kill = choose(hk), high = hk;
            XMFLOAT3 tip = Mul(Body::FlipAt(tipR, kill), tall);
            if (const float reach = std::hypot(tip.x, tip.z); clear + .3f > reach)
            {   // short of its neck: into its shoulder or back, its top falling away from the neck a quarter as fast as
                // it goes out (no more than 0.15 of its height below its neck)
                high = (std::max)(hk - .25f * (clear + .3f - reach), hk - .15f * H);
                kill = choose(high);
                tip = Mul(Body::FlipAt(tipR, kill), tall);
            }
            const float out = (std::max)(std::hypot(tip.x, tip.z), clear + .3f);
            b.FinishDeathAt = kill;
            const float bearing = std::atan2(way.x, way.z);
            b.FinishYawEnd = std::remainder(bearing - std::atan2(tip.x, tip.z) - XM_PI, XM_2PI);
            b.FinishA = {F.x + neck.x + way.x * out, S.y, F.z + neck.z + way.z * out};
            b.FinishB = Sub(Mul(Add(F, Mul(r, .6f * R + .35f)), 2), Mul(Add(S, b.FinishA), .5f)); b.FinishB.y = S.y;
            b.FinishLiftMost = std::clamp(high - tip.y, 0.f, 2.6f);   // (at a tall one she leaps into the blow)
            {   // where her blade's point is as it kills: her place, the point turned to her facing then, lifted
                const float c = std::cos(b.FinishYawEnd), sn = std::sin(b.FinishYawEnd);
                b.FinishStrike = {b.FinishA.x + tip.x * c + tip.z * sn, F.y + tip.y + b.FinishLiftMost, b.FinishA.z - tip.x * sn + tip.z * c};
            }
            b.FinishBlowAt[0] = .39f; b.FinishBlowAt[1] = -1;
            for (int k = 0; k <= 94; ++k) if (SlideShare(static_cast<float>(k) / 120.f) >= .5f) { b.FinishBlowAt[0] = static_cast<float>(k) / 120.f; break; }
        }
        else
        {   // the seven more (FinishMore): her blade's point, as the blow Place lands, on its front (a little into it) at
            // its height; she no nearer it than its edge and a little; lifted to it where it is tall
            const FinishMore& m = More[which];
            const int hand = m.Hand[m.Place];
            const float when = m.Hit[m.Place];
            XMFLOAT3 tip = hand == 2 ? Mul(Add(Body::FlipAt(b.FinishTipAt[which][0], when), Body::FlipAt(b.FinishTipAt[which][1], when)), .5f)
                                     : Body::FlipAt(b.FinishTipAt[which][hand], when);
            tip = Mul(tip, tall);
            b.FinishKneelTo = m.Kneels >= 0 ? m.KneelTo : 0.f;
            const float high = m.Aim * N - (m.Kneels >= 0 && m.Kneels < m.Place ? Wraith::KneelDrop(K, m.KneelTo) : 0.f);
            const XMFLOAT3 P = Sub(F, Mul(u, .35f * R));
            float along = (P.x - F.x) * u.x + (P.z - F.z) * u.z - tip.z, aside = -tip.x;
            if (const float clear = R + .35f; std::hypot(along, aside) < clear)
                along = -std::sqrt((std::max)(0.f, clear * clear - aside * aside));
            b.FinishA = Add(Add(F, Mul(u, along)), Mul(r, aside)); b.FinishA.y = S.y;
            // (on her wings up to a giant's chest: 2.6 m, more at a boss, 0.45 of its height, no more than 4.5 m)
            b.FinishLiftMost = m.Aim > 0 ? std::clamp(high - tip.y, 0.f, std::clamp(.45f * H, 2.6f, 4.5f)) : 0.f;
            b.FinishStrike = Add(Add(b.FinishA, Mul(r, tip.x)), Mul(u, tip.z));
            b.FinishStrike.y = F.y + (m.Aim > 0 ? (std::max)(tip.y, high) : tip.y);
            if (m.Past > 0)
            {   // round its side ('side') and on beyond it
                const XMFLOAT3 rs = Mul(r, side);
                b.FinishB = Add(F, Mul(rs, 2 * R + 1.f));
                b.FinishC = Add(Add(F, Mul(u, R + m.Past)), Mul(rs, .3f * R + .35f));
                b.FinishB.y = b.FinishC.y = S.y;
            }
            b.FinishBlowAt[0] = when; b.FinishBlowAt[1] = -1;
        }
        return true;
    }

    // Has the way of the finisher set up in her body room? Every 20th of a second of it: walkable ground under her
    // place, no more than a step over the ground under her the moment before and no deeper than a step down (as the
    // engine moves her: higher stops her, deeper takes her over the edge), and nothing blocking where she stands (rock,
    // a trunk, a wall). False at the first moment that fails ('at': when; 'why': what). With no scene (no ground known)
    // every way has room.
    bool NyxaraKit::FinishFits(const WorldState& world, float& at, std::string& why) const
    {
        at = 0; why.clear();
        if (!world.RuntimeScene) return true;
        const auto& b = *mBody;
        const auto& p = world.MainPlayer;
        const float seconds = Finishes[b.Finish].Seconds;
        const int steps = (std::max)(1, static_cast<int>(std::ceil(seconds * 20.f)));
        XMFLOAT3 was = b.FinishFrom;
        float under = b.FinishFrom.y;
        for (int k = 1; k <= steps; ++k)
        {
            at = seconds * static_cast<float>(k) / static_cast<float>(steps);
            XMFLOAT3 place{}; float yaw = 0, lift = 0;
            FinishPlan(at, b.FinishFeet.y - under, place, yaw, lift);
            const float moved = std::hypot(place.x - was.x, place.z - was.z);
            if (moved < .05f) continue;
            const auto hit = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*world.RuntimeScene, place.x, place.z,
                under - DoorwaysMaxWalkableStepDown - .6f * moved, under + DoorwaysMaxWalkableStepUp + .6f * moved);
            if (!hit.Hit) { why = "no ground"; return false; }
            if (const auto* in = FindDoorwaysRuntimeSceneBlockingProxyForPlayer(*world.RuntimeScene, {place.x, hit.GroundY, place.z}, p.CollisionRadius, p.CollisionHeight))
            {
                why = "blocked by " + in->Name; return false;
            }
            was = place; under = hit.GroundY;
        }
        return true;
    }

    void NyxaraKit::FinishPlan(float at, float ground, XMFLOAT3& place, float& yaw, float& lift) const
    {
        const auto& b = *mBody;
        const int which = b.Finish;
        const XMFLOAT3 S = b.FinishFrom;
        const float contact = b.FinishBlowAt[0], hopFrom = PlungeHopFrom(contact), yawU = b.FinishYawTo;
        const auto turnTo = [](float from, float to, float s) { return from + std::remainder(to - from, XM_2PI) * s; };
        const auto lerp = [](XMFLOAT3 a, XMFLOAT3 c, float s) { return Add(a, Mul(Sub(c, a), s)); };
        if (which == 1)
        {   // in (a dash), the cuts, and past (round its side, facing on: her back to it, or beside it)
            const XMFLOAT3 &A = b.FinishA, &C = b.FinishB, &E = b.FinishC;
            const float s = Smooth(.66f, .96f, at);
            place = at <= .66f ? lerp(S, A, Smooth(.04f, .38f, at)) : Curve(A, C, E, s);
            const XMFLOAT3 go = CurveWay(A, C, E, s);
            const float along = .4f * (Smooth(.62f, .72f, at) - Smooth(.92f, 1.08f, at));
            yaw = turnTo(turnTo(b.FinishYawFrom, yawU, Smooth(0, .2f, at)), std::atan2(go.x, go.z), along);
            lift = (std::max)(0.f, b.FinishLiftMost + ground) * (Smooth(.30f, .44f, at) - Smooth(.62f, .80f, at));
        }
        else if (which == 2)
        {   // the leap onto it, riding it down (PlungeRide), and back off it
            yaw = turnTo(b.FinishYawFrom, yawU, Smooth(0, .25f, at));
            if (at < contact)
            {
                place = lerp(S, b.FinishA, Smooth(.30f, contact - .04f, at));
                lift = (std::max)(0.f, b.FinishLiftMost + ground) * Smooth(.36f, .80f, at);
            }
            else if (at < hopFrom) { b.PlungeRide(at, place, lift); lift += ground; }
            else
            {
                XMFLOAT3 off; float high;
                b.PlungeRide(hopFrom, off, high);
                const float x = std::clamp((at - hopFrom) / PlungeHop, 0.f, 1.f), s = x * x * (3 - 2 * x);
                place = lerp(off, b.FinishLand, s);
                lift = (high + ground) * (1 - s) + (.3f + .1f * LenXZ(Sub(b.FinishLand, off))) * std::sin(XM_PI * x);
            }
        }
        else if (which >= 4)
        {   // the seven more (FinishMore): in to her place, and on past it; lifted to its height, and her own leap
            const FinishMore& m = More[which];
            const XMFLOAT3 &A = b.FinishA, &C = b.FinishB, &E = b.FinishC;
            yaw = turnTo(b.FinishYawFrom, yawU, Smooth(0, .2f, at));
            if (m.Past <= 0 || at <= m.PastFrom) place = lerp(S, A, Smooth(m.InFrom, m.InTo, at));
            else place = Curve(A, C, E, Smooth(m.PastFrom, m.PastTo, at));
            const float hit = m.Hit[m.Place];
            lift = (std::max)(0.f, b.FinishLiftMost + ground) * (Smooth(hit - .3f, hit - .03f, at) - Smooth(m.LiftDown - .35f, m.LiftDown, at));
            if (m.Leap > 0) lift += m.Leap * (Smooth(m.LeapFrom, m.LeapTop, at) - Smooth(m.LeapTop, m.LeapDown, at));
        }
        else
        {   // the slide past it, at the slide's own pace; up and round to the blow
            const XMFLOAT3 &A = b.FinishA, &C = b.FinishB;
            if (at <= .78f)
            {
                const float s = SlideShare(at);
                place = Curve(S, C, A, s);
                const XMFLOAT3 go = CurveWay(S, C, A, s);
                yaw = turnTo(b.FinishYawFrom, std::atan2(go.x, go.z), Smooth(0, .12f, at));
            }
            else
            {
                place = A;
                const XMFLOAT3 go = CurveWay(S, C, A, 1);
                yaw = turnTo(std::atan2(go.x, go.z), b.FinishYawEnd, Smooth(.78f, 1.12f, at));
            }
            const float kill = b.FinishDeathAt;
            lift = (std::max)(0.f, b.FinishLiftMost + ground) * (Smooth(kill - .34f, kill - .04f, at) - Smooth(kill + .06f, kill + .42f, at));
        }
    }

    void NyxaraKit::UpdateFinish(WorldState& world, float dt, bool free)
    {
        auto& b = *mBody;
        auto& p = world.MainPlayer;
        b.WingAimFlap = b.WingAimBack = 0;
        if (!b.Finish)
        {   // a doomed creature near her, and she is free on foot (a cut of hers lands first): she goes to finish it
            b.FinishLiftNow = 0; b.FinishGo = {0, 0};
            b.FinishRetry = (std::max)(0.f, b.FinishRetry - dt);
            for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
            {   // just doomed: where no finisher has room from where she is (an edge, a wall close by), her blow kills it
                // as it did before her finishers (2026-11-20: a doomed creature takes no more blows, and with no finisher
                // to come she would strike at it for its three seconds)
                auto& w = mWraiths[i];
                if (!w.Alive || w.Doom <= 0 || w.RoomLooked) continue;
                w.RoomLooked = true;
                std::string why;
                if (AnyFinishFits(world, i, &why)) continue;
                b.Log(world, ("finish: none has room, her blow kills it: " + why).c_str());
                StrikeDown(i);
            }
            // (a style's move gives way to a finisher once its last blow is in: StyleFinishAfter)
            const bool moveDone = b.InMove() && b.AttackTime >= b.MoveOf->LastHit() + StyleFinishAfter;
            if (!free || p.TraversalMode == DoorwaysPlayerTraversalMode::Flight || !p.IsGrounded || b.Jumping || mFalling || (b.Carried() && !moveDone) || !mPreview.empty()) return;
            if (!b.Attack.empty() && !b.InMove() && b.AttackTime < Body::Hit(b.Attack) + ChainAfter) return;
            if (b.FinishRetry > 0) return;   // (none had room a moment ago: StartFinish)
            int chosen = -1; float nearest = FinishReach;
            for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
            {
                const auto& w = mWraiths[i];
                if (!w.Alive || w.Doom <= 0 || i == b.Pushed) continue;
                const XMFLOAT3 feet = FeetOf(w);
                const float away = std::hypot(feet.x - p.Position.x, feet.z - p.Position.z) - w.Radius;
                if (away > nearest || std::abs(feet.y - p.Position.y) > FinishHigh) continue;
                nearest = away; chosen = i;
            }
            if (chosen < 0) return;
            StartFinish(world, chosen);
            if (!b.Finish) return;
        }
        else b.FinishTime += dt;
        const int which = b.Finish;
        const auto& spec = Finishes[which];
        if (b.FinishTarget < 0 || b.FinishTarget >= static_cast<int>(mWraiths.size()) || b.Dead ||
            (!b.FinishKilled && (!mWraiths[b.FinishTarget].Alive || mWraiths[b.FinishTarget].Doom <= 0)))
        {   // it is gone before her blow (or she is): it ends here
            b.Finish = 0; b.FinishTarget = -1; b.FinishLiftNow = 0; b.FinishGo = {0, 0};
            b.Log(world, "finish broken off");
            return;
        }
        if (!p.IsGrounded && p.VerticalVelocity < -7.f)
        {   // she went over an edge after all (FinishFits looks every 20th of a second of its way): it ends here, she falls
            b.Finish = 0; b.FinishTarget = -1; b.FinishLiftNow = 0; b.FinishGo = {0, 0};
            b.Log(world, "finish broken off: she fell");
            return;
        }
        auto& w = mWraiths[b.FinishTarget];
        const float t = b.FinishTime;
        const XMFLOAT3 F = b.FinishFeet, u = b.FinishWay, r = b.FinishRight, up{0, 1, 0};
        const float R = b.FinishRadius, H = b.FinishHeight, K = b.FinishKnee, ground = F.y - p.Position.y;
        const float contact = b.FinishBlowAt[0], hopFrom = PlungeHopFrom(contact), hop = PlungeHop;
        const auto turnTo = [](float from, float to, float s) { return from + std::remainder(to - from, XM_2PI) * s; };
        // where she is (the world's x and z), how she faces and how high her picture is lifted, now and a frame on
        XMFLOAT3 now{}, next{}; float yaw = p.Yaw, lift = 0, yawNext = 0, liftNext = 0;
        constexpr float Step = 1 / 60.f, Settle = .08f;
        FinishPlan(t, ground, now, yaw, lift);
        FinishPlan(t + Step, ground, next, yawNext, liftNext);
        p.Yaw = std::remainder(yaw, XM_2PI);
        b.FinishLiftNow = (std::max)(0.f, lift);
        b.FinishGo = {0, 0};
        if (dt > 1e-5f)
        {   // The engine moves her: as fast as the plan goes, and what she is off it made up over Settle s. (Not "to
            // where she is to be next frame": the engine moves her in steps of a 60th of a second, none in one frame
            // and two in the next, and steering by the frame overshot and swung her to and fro.)
            XMFLOAT2 go{(next.x - now.x) / Step + (now.x - p.Position.x) / Settle, (next.z - now.z) / Step + (now.z - p.Position.z) / Settle};
            const float fast = std::hypot(go.x, go.y);
            if (fast > 60) { go.x *= 60 / fast; go.y *= 60 / fast; }
            b.FinishGo = go;
        }
        {   // her wings: swept back as she goes fast past it (the cross, the slide: low, they were through its legs) and
            // into her dive (the plunge); opened as she rises to her last blow (the slide)
            float flap = 0, back = 0;
            if (which == 1) back = .5f * (Smooth(.6f, .7f, t) - Smooth(.95f, 1.15f, t));
            else if (which == 2) back = .3f * (Smooth(.75f, .9f, t) - Smooth(contact, contact + .15f, t));
            else if (which >= 4)
            {   // (swept back as she goes past it; opened as she leaps)
                const FinishMore& m = More[which];
                if (m.Past > 0) back = .55f * (Smooth(m.PastFrom - .05f, m.PastFrom + .05f, t) - Smooth(m.PastTo, m.PastTo + .3f, t));
                if (m.Leap > 0)
                {
                    const float rising = Smooth(m.LeapFrom - .1f, m.LeapFrom + .05f, t) - Smooth(m.LeapTop, m.LeapDown, t);
                    flap -= .3f * rising; back -= .2f * rising;
                }
            }
            else
            {
                const float kill = b.FinishDeathAt, slid = Smooth(.02f, .12f, t) - Smooth(.7f, .9f, t);
                const float rise = Smooth(kill - .5f, kill - .25f, t) - Smooth(kill + .1f, kill + .5f, t);
                flap = .45f * slid - .25f * rise; back = .55f * slid - .2f * rise;
            }
            b.WingAimFlap = flap; b.WingAimBack = back;
        }
        if (!b.FinishKilled)
        {   // it: dazed, turned to her, and (the slide) on its knees
            w.Pinned = true; w.Push = {};
            w.Yaw = turnTo(b.FinishTheirYawFrom, b.FinishTheirYawTo, Smooth(0, .25f, t));
            w.Daze = Smooth(-.15f, .3f, t);
            w.Kneel = b.FinishKneel;
        }

        // ---- what happens on the way
        // (Her flair, where one is on: each cut's light is its own, and the kill's: NyxaraFlair's Strike, placed on the
        // creature, and Finish; owner, 2026-11-25, "adjust finishings on the new effect systems". Then none of the
        // kit's own lines, drops, rings and bursts are made here: slash, spray and ring do nothing or ask her flair.)
        const bool flair = mFlair && mFlair->On();
        const auto slash = [&](XMFLOAT3 at, XMFLOAT3 way, float length, float life)
        {   // a long bright line where her blade went through it
            if (flair) return;
            if (b.Marks.size() > 500) b.Marks.erase(b.Marks.begin(), b.Marks.begin() + 150);
            b.Marks.push_back({at, Norm(way), life, life, length, F.y, 5});
        };
        const auto spray = [&](const XMFLOAT3& at, const XMFLOAT3& way, int count, float speed) { if (!flair) b.Spray(at, way, count, speed, F.y); };
        const auto ring = [&](const XMFLOAT3& at, float size, XMFLOAT4 colour, float life) { if (flair) mFlair->Gust(at, size); else RingEffect(at, size, colour, life); };
        const auto cutLight = [&](const XMFLOAT3& at, const XMFLOAT3& way, int hand, int weight, float ringWide = 0, bool whirl = false)
        {   // (her flair) a blow of the finisher on it: where, the way her blade goes, its blade (0 left, 1 right, 2 both)
            if (!flair) return;
            NyxaraFlair::Blow blow;
            blow.Her = p.Position; blow.Yaw = p.Yaw; blow.Hand = hand; blow.Weight = weight; blow.Ring = ringWide; blow.Whirl = whirl;
            blow.Placed = true; blow.At = at; blow.Struck.push_back({F, H, R});
            mFlair->Strike(blow);
            b.FinishLit = at; b.FinishLitWay = way; b.FinishLitAny = true;
        };
        const auto slow = [&](float seconds, float rate) { mSlow = mSlowFull = seconds; mSlowRate = rate; };
        const auto passed = [&](float when) { return when >= 0 && t >= when; };
        if (which == 1)
        {
            const float hX = std::clamp(1.3f + b.FinishLiftMost, .3f * H, .72f * H), length = std::clamp(.5f * H, .75f, 1.7f);
            const XMFLOAT3 cross = Add(Sub(F, Mul(u, .85f * R)), {0, hX, 0});
            const XMFLOAT3 ways[2] = {Norm(Sub(r, up)), Norm(Sub(Mul(r, -1), up))};   // from her upper left, from her upper right
            for (int k = 0; k < 2; ++k)
                if (!b.FinishBlown[k] && passed(b.FinishBlowAt[k]))
                {
                    b.FinishBlown[k] = true;
                    slash(cross, ways[k], length, 1.1f);
                    if (!flair) b.Wound(F, H, R, p.Position, k ? -1.f : 1.f, false, 60);
                    spray(cross, Add(Mul(ways[k], .6f), Mul(u, -.5f)), 12, 3.5f);
                    cutLight(cross, ways[k], k ? 0 : 1, 1);   // (Attack_R1's cut, then Attack_L1's)
                    w.Flash = .16f;
                    mHitStop = (std::max)(mHitStop, k ? .09f : .05f);
                    mShake = (std::max)(mShake, k ? .16f : .1f);
                    b.Log(world, k ? "finish cut 2" : "finish cut 1");
                    CaucasianEagle::Sound::Play("nyxara/swing", .55f, .15f);
                    CaucasianEagle::Sound::Play("nyxara/cut", 1.f, .06f);
                    CaucasianEagle::Sound::Play("combat/hit", .7f, .1f);
                }
            if (!b.FinishKilled && passed(b.FinishDeathAt))
            {   // the cross opens
                for (int k = 0; k < 2; ++k) slash(cross, ways[k], length * 1.15f, .45f);
                spray(cross, Add(Mul(u, -1), {0, .35f, 0}), 34, 4.5f);
                mShake = (std::max)(mShake, .22f);
                slow(.4f, .4f);
                CaucasianEagle::Sound::Play("combat/hit", 1.f, .06f);
            }
        }
        else if (which == 2)
        {
            if (!b.FinishBlown[1] && t >= .30f)
            {   // she springs: her wings open and beat
                b.FinishBlown[1] = true;
                for (auto& wing : b.WingSpring) wing.FlapGo += 3.2f;
                ring({p.Position.x, p.Position.y + .05f, p.Position.z}, 1.8f, {.62f, .56f, .48f, 1}, .4f);
                CaucasianEagle::Sound::Play("nyxara/effort", .9f, .08f);
                CaucasianEagle::Sound::Play("nyxara/wing", .75f);
                CaucasianEagle::Sound::Play("nyxara/dash", .6f, .1f);
            }
            if (!b.FinishBlown[0] && passed(contact))
            {   // her blades go in
                b.FinishBlown[0] = true;
                for (auto& wing : b.WingSpring) wing.FlapGo -= 2.6f;
                spray(b.FinishAnchor, Add(Mul(u, -.7f), {0, .6f, 0}), 40, 4.5f);
                cutLight(b.FinishAnchor, {0, -1, 0}, 2, 1);
                w.Flash = .16f;
                mHitStop = (std::max)(mHitStop, .12f); mShake = (std::max)(mShake, .35f);
                slow(.45f, .3f);
                CaucasianEagle::Sound::Play("nyxara/cut", 1.f, .05f);
                CaucasianEagle::Sound::Play("combat/blow", .9f, .08f);
            }
            if (b.FinishKilled && !b.FinishRipped && t >= hopFrom)
            {   // she pulls them out and springs back off it
                b.FinishRipped = true;
                XMFLOAT3 place, anchor; float high;
                b.PlungeRide(hopFrom, place, high, &anchor);
                spray(anchor, {0, 1, 0}, 26, 3.5f);
                for (auto& wing : b.WingSpring) wing.FlapGo += 3.f;
                CaucasianEagle::Sound::Play("nyxara/wing", .6f);
                CaucasianEagle::Sound::Play("nyxara/dash", .7f, .1f);
                CaucasianEagle::Sound::Play("nyxara/effort", .7f, .1f);
            }
            if (b.FinishRipped && !b.FinishLanded && t >= hopFrom + hop)
            {
                b.FinishLanded = true;
                ring({p.Position.x, p.Position.y + .05f, p.Position.z}, 1.6f, MoonWhite, .35f);
                mShake = (std::max)(mShake, .06f);
                CaucasianEagle::Sound::Play("nyxara/land", .8f);
            }
        }
        else if (which >= 4)
        {   // the seven more (FinishMore): each blow where her blade's point is then (brought onto it), the way it goes
            const FinishMore& m = More[which];
            const float tall = b.FinishTall;
            const auto tipAt = [&](int hand, float when)
            {
                XMFLOAT3 place{}; float y = 0, l = 0;
                FinishPlan(when, ground, place, y, l);
                const XMFLOAT3 tip = Mul(Body::FlipAt(b.FinishTipAt[which][hand], when), tall);
                const float c = std::cos(y), sn = std::sin(y);
                return XMFLOAT3{place.x + tip.x * c + tip.z * sn, p.Position.y + tip.y + (std::max)(0.f, l), place.z - tip.x * sn + tip.z * c};
            };
            const auto onto = [&](XMFLOAT3 at)
            {   // (on it: no further from its middle than most of its width, between its feet and its top)
                XMFLOAT3 off{at.x - F.x, 0, at.z - F.z};
                const float d = LenXZ(off), most = (std::max)(.2f, .85f * R);
                if (d > most) { at.x = F.x + off.x * most / d; at.z = F.z + off.z * most / d; }
                at.y = std::clamp(at.y, F.y + .15f, F.y + .95f * H);
                return at;
            };
            const float length = std::clamp(.35f * H, .7f, 1.6f);
            for (int k = 0; k < 4; ++k)
            {
                if (m.Hit[k] < 0 || b.FinishHit[k] || t < m.Hit[k]) continue;
                b.FinishHit[k] = true;
                const bool last = std::abs(m.Hit[k] - spec.Death) < 1e-3f;
                XMFLOAT3 lit{}, litWay{}; int lights = 0;
                for (int hand = 0; hand < 2; ++hand)
                {
                    if (m.Hand[k] != 2 && m.Hand[k] != hand) continue;
                    const XMFLOAT3 at = onto(tipAt(hand, m.Hit[k]));
                    XMFLOAT3 way = Sub(tipAt(hand, m.Hit[k] + 1 / 60.f), tipAt(hand, m.Hit[k] - 1 / 60.f));
                    way = Len(way) > 1e-4f ? Norm(way) : Norm(Sub(r, up));
                    slash(at, way, length, m.Show == 3 ? .35f : .9f);
                    spray(at, Add(Mul(way, .6f), Mul(u, .4f)), last ? 30 : 12, last ? 4.5f : 3.5f);
                    if (mHiggsfieldTrial && !flair) TrialBiteLight(at, u, true);
                    if (b.FinishMarks < 4) { b.FinishMark[b.FinishMarks] = at; b.FinishMarkWay[b.FinishMarks] = way; ++b.FinishMarks; }
                    lit = Add(lit, at); litWay = Add(litWay, way); ++lights;
                }
                if (flair && lights)
                {   // (her flair: one light for the blow, both blades' as one. The first two of a flurry are light blows,
                    // the rest middle ones, the last too: what the kill adds is the kill's own, Finish, below. A whole
                    // turn's cut is all round her; blades into the ground send out the style's ring)
                    int blows = 0;
                    for (const float hit : m.Hit) if (hit >= 0) ++blows;
                    const bool intoGround = last && m.Show == 2;
                    cutLight(Mul(lit, 1.f / static_cast<float>(lights)), Len(litWay) > 1e-4f ? Norm(litWay) : u, m.Hand[k], blows > 2 && k < 2 ? 0 : 1,
                             intoGround ? std::clamp(1.5f * R + 1.5f, 2.f, 5.f) : 0.f, which == 5 && k == 0);
                }
                w.Flash = .16f;
                mHitStop = (std::max)(mHitStop, last ? .1f : .05f);
                mShake = (std::max)(mShake, last ? .25f : .1f);
                CaucasianEagle::Sound::Play(last ? "nyxara/swing_heavy" : "nyxara/swing", .55f, .15f);
                CaucasianEagle::Sound::Play("nyxara/cut", 1.f, .06f);
                CaucasianEagle::Sound::Play(k == m.Kneels ? "creatures/hurt" : "combat/hit", .8f, .1f);
                b.Log(world, ("finish cut " + std::to_string(k + 1)).c_str());
            }
            if (m.Kneels >= 0) b.FinishKneel = b.FinishKneelTo * Smooth(m.Hit[m.Kneels], m.Hit[m.Kneels] + .3f, t);
            if (m.Leap > 0 && !b.FinishBlown[1] && t >= m.LeapFrom)
            {   // her wings beat her up
                b.FinishBlown[1] = true;
                for (auto& wing : b.WingSpring) wing.FlapGo += 3.2f;
                ring({p.Position.x, p.Position.y + .05f, p.Position.z}, 1.6f, {.62f, .56f, .48f, 1}, .35f);
                CaucasianEagle::Sound::Play("nyxara/wing", .75f);
                CaucasianEagle::Sound::Play("nyxara/effort", .8f, .08f);
            }
            if (m.Leap > 0 && !b.FinishLanded && t >= m.LeapDown)
            {
                b.FinishLanded = true;
                if (!flair || m.Show != 2) ring({p.Position.x, p.Position.y + .05f, p.Position.z}, 1.4f, MoonWhite, .3f);   // (her flair: blades into the ground have its ring already)
                CaucasianEagle::Sound::Play("nyxara/land", .8f);
            }
            if (m.Past > 0 && !b.FinishBlown[0] && t >= m.PastFrom - .05f)
            {   // she goes past it in a blink
                b.FinishBlown[0] = true;
                for (auto& wing : b.WingSpring) wing.FlapGo += 1.5f;
                CaucasianEagle::Sound::Play("nyxara/dash", .8f, .1f);
            }
            if (!b.FinishKilled && passed(b.FinishDeathAt))
            {   // its kill, as this one has it
                if (m.Show == 1)
                {   // (her blade's point comes out of its back as she draws it)
                    spray(Add(b.FinishStrike, Mul(u, .8f * R)), Add(u, {0, .3f, 0}), 40, 5.f);
                    if (flair) { b.FinishLit = Add(b.FinishStrike, Mul(u, .8f * R)); b.FinishLitWay = u; b.FinishLitAny = true; }
                    CaucasianEagle::Sound::Play("combat/blow", .9f, .08f);
                }
                else if (m.Show == 2)
                {   // (the ground shakes round her blades)
                    const XMFLOAT3 at{b.FinishStrike.x, F.y + .05f, b.FinishStrike.z};
                    if (!flair)
                    {
                        RingEffect(at, std::clamp(1.5f * R + 1.5f, 2.f, 5.f), {.62f, .56f, .48f, 1}, .5f);
                        RingEffect(at, std::clamp(R + 1.f, 1.2f, 3.5f), MoonWhite, .35f);
                        Burst(at, {.55f, .5f, .45f, 1}, 26, 4.f, .3f);
                        if (mHiggsfieldTrial) TrialGroundLight({at.x, at.y + .05f, at.z}, std::clamp(1.5f * R + 1.5f, 2.f, 5.f), true);
                    }
                    mShake = (std::max)(mShake, .45f);
                    CaucasianEagle::Sound::Play("combat/blow", 1.f, .06f);
                }
                else if (m.Show == 3)
                {   // (her cuts open as she stands past it)
                    for (int k = 0; k < b.FinishMarks; ++k)
                    {
                        slash(b.FinishMark[k], b.FinishMarkWay[k], length * 1.3f, .6f);
                        spray(b.FinishMark[k], Add(Mul(b.FinishMarkWay[k], .5f), {0, .4f, 0}), 40, 4.5f);
                        cutLight(b.FinishMark[k], b.FinishMarkWay[k], 1, 1);
                    }
                    w.Flash = .2f;
                    mShake = (std::max)(mShake, .3f);
                    CaucasianEagle::Sound::Play("nyxara/cut", 1.f, .05f);
                    CaucasianEagle::Sound::Play("combat/hit", 1.f, .06f);
                }
                else
                {
                    spray(b.FinishStrike, Add(Mul(u, m.Show == 4 ? .3f : .6f), {0, m.Show == 4 ? .9f : .4f, 0}), 34, 4.5f);
                    CaucasianEagle::Sound::Play("combat/blow", .8f, .08f);
                }
                slow(.45f, .35f);
            }
        }
        else
        {
            if (!b.FinishBlown[0] && passed(b.FinishBlowAt[0]))
            {   // through its legs as she passes
                b.FinishBlown[0] = true;
                const XMFLOAT3 legs = Add(Add(F, Mul(r, .5f * R)), {0, (std::max)(.25f, .8f * K), 0});
                slash(legs, u, std::clamp(.8f * R + .5f, .6f, 1.6f), .8f);
                spray(legs, Add(r, Mul(u, .5f)), 14, 3.f);
                cutLight(legs, u, 0, 1);
                w.Flash = .16f;
                mHitStop = (std::max)(mHitStop, .04f); mShake = (std::max)(mShake, .1f);
                CaucasianEagle::Sound::Play("nyxara/swing", .5f, .15f);
                CaucasianEagle::Sound::Play("nyxara/cut", .95f, .06f);
                CaucasianEagle::Sound::Play("creatures/hurt", .85f, .1f);
            }
            if (b.FinishBlown[0]) b.FinishKneel = b.FinishKneelTo * Smooth(b.FinishBlowAt[0], b.FinishBlowAt[0] + .3f, t);
            if (!b.FinishBlown[1] && b.FinishLiftMost > .4f && t >= b.FinishDeathAt - .34f)
            {   // (a tall one) her wings beat her up into the blow
                b.FinishBlown[1] = true;
                for (auto& wing : b.WingSpring) wing.FlapGo -= 2.8f;
                CaucasianEagle::Sound::Play("nyxara/wing", .65f);
                CaucasianEagle::Sound::Play("nyxara/dash", .6f, .1f);
            }
            if (!b.FinishKilled && passed(b.FinishDeathAt))
            {   // through its neck (or into its back short of it), the way her blade goes
                const float kill = b.FinishDeathAt, c = std::cos(b.FinishYawEnd), sn = std::sin(b.FinishYawEnd);
                const auto& tipR = b.FinishTipAt[3][1];
                const XMFLOAT3 v = Sub(Body::FlipAt(tipR, kill + 1 / 60.f), Body::FlipAt(tipR, kill - 1 / 60.f));
                const XMFLOAT3 way = Norm({v.x * c + v.z * sn, v.y, -v.x * sn + v.z * c});
                slash(b.FinishStrike, way, std::clamp(.35f * H, .8f, 1.8f), .9f);
                spray(b.FinishStrike, Add(Mul(way, .6f), {0, .5f, 0}), 46, 5.f);
                cutLight(b.FinishStrike, way, 1, 1);
                mHitStop = (std::max)(mHitStop, .1f); mShake = (std::max)(mShake, .3f);
                slow(.45f, .35f);
                CaucasianEagle::Sound::Play("nyxara/swing_heavy", .6f, .1f);
                CaucasianEagle::Sound::Play("nyxara/cut", 1.f, .05f);
                CaucasianEagle::Sound::Play("combat/blow", .8f, .08f);
            }
        }
        if (!b.FinishKilled && passed(b.FinishDeathAt))
        {   // it dies: down on its knees and over (the cross: on its face, away from her; the plunge: on its back, with
            // her on it; the slide: from its knees, away from her blow), and it lies a while
            b.FinishKilled = true;
            const float linger = (spec.Seconds - b.FinishDeathAt) + FallenLies;
            Fallen f;
            f.Index = b.FinishTarget; f.Feet = F; f.Neck = b.FinishNeck; f.Knee = K; f.Daze = 1;
            if (which == 1) { f.Way = Mul(u, -1); f.Wait = .12f; f.Takes = spec.Falls; f.Kneel = .3f; }
            else if (which == 2) { f.Way = u; f.Wait = 0; f.Takes = spec.Falls; f.Time = (t - contact) - dt; }   // (in step with her ride)
            else if (which >= 4)
            {
                const int falls = More[which].Falls;
                f.Way = falls == 0 ? u : falls == 1 ? Mul(u, -1) : r;
                f.Wait = .08f; f.Takes = spec.Falls;
                f.KneelFrom = f.Kneel = b.FinishKneel > .05f ? b.FinishKneel : .3f;
            }
            else
            {
                XMFLOAT3 away = Sub(F, b.FinishA); away.y = 0;
                f.Way = Norm(away); f.Wait = .08f; f.Takes = spec.Falls; f.KneelFrom = f.Kneel = b.FinishKneelTo;
            }
            KillWraith(b.FinishTarget, linger);
            f.Until = linger + .65f;
            LetFall(f);
            if (flair)
            {   // (her flair: the kill's own light, round it: where her last blow was in it, the way her blade went)
                NyxaraFlair::Kill kill;
                kill.At = b.FinishLitAny ? b.FinishLit : which == 1 ? Add(Sub(F, Mul(u, .85f * R)), {0, .5f * H, 0}) : b.FinishStrike;
                kill.Way = u; kill.Cut = b.FinishLitAny ? b.FinishLitWay : XMFLOAT3{};
                kill.It = {F, H, R}; kill.Ground = which >= 4 && More[which].Show == 2;
                mFlair->Finish(kill);
            }
            else Burst(Add(F, {0, .6f * H, 0}), ShadeViolet, 14, 3.f, .3f);
            b.Log(world, "finish kill");
        }
        if (t >= spec.Seconds)
        {
            b.Finish = 0; b.FinishTarget = -1; b.FinishLiftNow = 0; b.FinishGo = {0, 0};
            b.Log(world, "finish over");
        }
    }

    void NyxaraKit::Update(WorldState& world, WorldRenderer& worldRenderer, CombatSystem& combat, float dt, float realDt, bool inputAllowed)
    {
        mWorldRenderer = &worldRenderer;
        if (!mReady) return;
        auto& b = *mBody;
        b.Pace = b.Attack == "Skill_Slide" ? SlidePace : b.Attack == Body::SoulClip ? 1 / AttackRate : mHiggsfieldTrial ? mTrialPace : 1.f;   // (F, her slide: its own, a dash's; her soul's leaving: as made)
        const bool flair = mFlair && mFlair->On();
        const float flairDt = dt > 0 ? mFlairDt : 0.f;
        if (flair && !b.Skill())
        {   // (her flair: its own pace, of its combo's moves and of her own cuts; her skills keep theirs, as the game has them)
            const NyxaraFlair::Info info = NyxaraFlair::Describe(mFlair->Gear(), mFlair->Style(), mFlair->Own());
            b.Pace = mCombo >= 0 ? info.Pace : info.CutPace;
        }
        auto& p = world.MainPlayer;
        auto& s = GameSession();
        b.Clock += dt;
        // (shown in her window, the game standing still: her pose alone goes on in real time, standing and breathing)
        const float poseDt = mShowcase && dt <= 0 ? realDt : dt;
        if (mShowcase && dt <= 0) b.Clock += realDt;
        if (b.InMove())
        {   // the engine's 60 Hz grid kept in step with the kit's (ScaleFrameTime): how many slices it really moved her by at
            // this frame's start, seen from how far she went at the speed asked (her own move: nothing else moves her), when
            // that is plain; a difference moves the kit's grid by as many slices
            constexpr double Slice = static_cast<double>(1.f / 60.f);
            const float speed = std::hypot(s.ImpulseX, s.ImpulseZ), asked = speed / 60.f;
            if (asked > .01f)
            {
                const float along = ((p.Position.x - b.WasAt.x) * s.ImpulseX + (p.Position.z - b.WasAt.y) * s.ImpulseZ) / speed;
                const float seen = along / asked;
                const int taken = static_cast<int>(std::lround(seen));
                if (taken >= 0 && taken <= 75 && std::abs(seen - static_cast<float>(taken)) < .15f && taken != b.Slices)
                {
                    mGrid = std::clamp(mGrid + (b.Slices - taken) * Slice, 0., Slice - 1e-9);
                    b.Log(world, ("grid kept in step: " + std::to_string(taken) + " slices, not " + std::to_string(b.Slices)).c_str());
                }
            }
        }
        const bool unattended = s.Unattended;
        const bool live = inputAllowed && !mInputBlocked && !unattended;   // the desktop's own keys (steering, stepping back)
        const Keys k = ReadKeys(inputAllowed);
        const auto pressed = [](bool now, bool before) { return now && !before; };
        const bool clickL = pressed(k.Lmb, mPrev.Lmb), clickR = pressed(k.Rmb, mPrev.Rmb), keyF = pressed(k.K1, mPrev.K1),
                   keyV = mSkillsVC && pressed(k.K2, mPrev.K2), keyC = mSkillsVC && pressed(k.K3, mPrev.K3);   // (V and C: SetSkillsVC)
        // (Q, her soul: one held as a window closes, the kit let go of again, is no Q: it waits to be let go, as her blade does)
        if (mInputBlocked) mQWaits = true;
        else if (!k.Q) mQWaits = false;
        const bool keyQ = !mQWaits && pressed(k.Q, mPrev.Q);
        const bool scriptedSpace = unattended && mRealJump > 0 && s.ScriptedJump;
        const bool space = pressed(k.Space, mPrev.Space) || pressed(scriptedSpace, mScriptedSpaceWas);
        mScriptedSpaceWas = scriptedSpace;
        mPrev = k;
        const bool alive = combat.PlayerActionsAllowed();
        bool flying = p.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
        {   // ---- flying is a limited resource (NyxaraKit.h: FlightEnergySeconds)
            // A test: CAUCASIAN_EAGLE_NYXARA_FLIGHT=<seconds> in an unattended run holds Space for her, and her
            // energy lasts that many seconds: she takes off, flies, falls, lands, and again.
            static const float tried = [] { char v[32]{}; size_t n = 0; return getenv_s(&n, v, "CAUCASIAN_EAGLE_NYXARA_FLIGHT") == 0 && n > 1 ? static_cast<float>(std::atof(v)) : 0.f; }();
            const bool holds = tried > 0 ? unattended : live && k.Space;
            if (unattended && tried <= 0) { mFlightEnergy = 1; mFlightHold = 0; }
            else if (flying)
            {
                mFlightHold = 0;
                mFlightEnergy = (std::max)(0.f, mFlightEnergy - dt / (tried > 0 ? tried : mFlightSeconds));
                if (mFlightEnergy <= 0 && !b.Dead)
                {   // out of it: she is on foot in the air, and gravity brings her down
                    p.TraversalMode = DoorwaysPlayerTraversalMode::Ground; p.JumpFlight.Active = false;
                    p.VerticalVelocity = (std::min)(p.VerticalVelocity, 0.f);
                    s.FreeFlightEnd = false; s.FreeFlightLandSpeed = 0; s.FreeFlightTakeoff = false;
                    flying = false; mFalling = !p.IsGrounded;
                    b.WasFlying = false; b.Rise = b.Touch = -1;
                    mNotice = L"HER WINGS ARE SPENT"; mNoticeTimer = 1.5f;
                    b.Log(world, "fall");
                }
            }
            else
            {
                if (p.IsGrounded) mFlightEnergy = (std::min)(1.f, mFlightEnergy + dt / FlightRefillSeconds);
                const bool can = holds && alive && !b.Dead && !mFalling && !b.Cast && !b.Carried() && !b.Finish;
                if (can && mFlightEnergy < FlightLeastShare && mFlightHold <= 0) { mNotice = L"HER WINGS ARE RESTING"; mNoticeTimer = .5f; }
                mFlightHold = can && mFlightEnergy >= (tried > 0 ? .99f : FlightLeastShare) ? mFlightHold + dt : 0;
                if (mFlightHold >= FlightHoldSeconds)
                {
                    mFlightHold = 0;
                    s.FreeFlightTakeoff = true; s.FreeFlightLandSpeed = 0; s.FreeFlightEnd = false;
                    CaucasianEagle::Sound::Play("nyxara/wing", .8f);
                }
            }
            if (mFalling && (p.IsGrounded || flying || b.Dead)) mFalling = false;
        }
        for (auto& w : mWraiths) { w.Pinned = false; w.Hold = 0; w.Daze = 0; w.Kneel = 0; w.Limp = -1; }   // set again below for what she holds this frame
        mHerPlace = p.Position; mHerAfoot = alive && !flying;   // (a blow dooms near her, on foot: DamageWraith)

        // ---- the enemies as her moves see them: feet, neck (in the world), height, radius
        struct Foe { XMFLOAT3 Feet, Neck; float Height, Radius; bool Alive; };
        const auto foe = [&](int i) -> Foe
        {
            const auto& w = mWraiths[static_cast<size_t>(i)];
            const XMFLOAT3 feet{w.Pos.x, w.Pos.y - 1.3f + w.Lift, w.Pos.z};
            const float c = std::cos(w.Yaw), sn = std::sin(w.Yaw);
            return {feet, {feet.x + w.Neck.x * c + w.Neck.z * sn, feet.y + w.Neck.y, feet.z - w.Neck.x * sn + w.Neck.z * c}, w.Height, w.Radius, w.Alive};
        };
        const auto before = [&](const Foe& f)   // within FrontArc of the way she faces, its width counted
        {
            const float dx = f.Feet.x - p.Position.x, dz = f.Feet.z - p.Position.z, d = std::hypot(dx, dz);
            if (d <= f.Radius + .3f) return true;   // she stands in it
            const float off = std::abs(std::remainder(std::atan2(dx, dz) - p.Yaw, XM_2PI));
            return off <= XMConvertToRadians(FrontArc) + std::asin((std::min)(1.f, f.Radius / d));
        };
        const auto sideOf = [&] { return b.Attack.find("_L") != std::string::npos ? 1.f : b.Attack.find("_R") != std::string::npos ? -1.f : b.Rand() < .5f ? 1.f : -1.f; };
        // A blow from where she is: every enemy within reach (plus its radius) and within arc degrees of her line,
        // whose height she is not over (in the air), is struck.
        std::vector<NyxaraFlair::Blow::Body> flairStruck;   // (her flair: the creatures the last blow struck)
        const auto strike = [&](float reach, float arc, int damage, float push, float launch = 0)
        {
            int struck = 0;
            flairStruck.clear();
            const float limit = std::cos(XMConvertToRadians(arc));
            for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
            {
                if (!mWraiths[i].Alive) continue;
                const Foe f = foe(i);
                const float dx = f.Feet.x - p.Position.x, dz = f.Feet.z - p.Position.z, d = std::hypot(dx, dz);
                if (d > reach + f.Radius) continue;
                const float over = p.Position.y - f.Feet.y;
                if (over > f.Height + .5f || over < -2.5f) continue;   // over its head, or it is high over hers
                if (d > f.Radius + .3f && (dx * std::sin(p.Yaw) + dz * std::cos(p.Yaw)) / d < limit) continue;
                if (!flair) b.Wound(f.Feet, f.Height, f.Radius, p.Position, sideOf(), true, damage);   // (her flair bleeds its own)
                DamageWraith(i, damage, p.Position, push, launch);
                if (flair) flairStruck.push_back({f.Feet, f.Height, f.Radius});
                ++struck;
            }
            if (struck) b.Log(world, ("hit " + std::to_string(struck)).c_str());
            if (mBlowListener) mBlowListener(p.Position, p.Yaw, reach, 2 * arc);
            return struck;
        };
        const auto strikeOne = [&](int i, int damage)   // one enemy, wherever it is (the swords in it, the throw)
        {
            if (i < 0 || i >= static_cast<int>(mWraiths.size()) || !mWraiths[i].Alive) return 0;
            const Foe f = foe(i);
            b.Wound(f.Feet, f.Height, f.Radius, p.Position, sideOf(), true, damage);
            DamageWraith(i, damage, p.Position, 0, 0);
            return 1;
        };
        const auto ease = [](float t, float a, float c)
        {
            const float x = std::clamp((t - a) / (std::max)(c - a, 1e-3f), 0.f, 1.f);
            return x * x * (3 - 2 * x);
        };

        // ---- the clocks of the game around her moves
        for (auto& c : mSkillCooldown) c = (std::max)(0.f, c - dt);
        mGuardCooldown = (std::max)(0.f, mGuardCooldown - dt);
        mSoulCooldown = (std::max)(0.f, mSoulCooldown - dt);   // (Q, her soul)
        // ---- dead: she falls and lies; everything she was doing ends
        if (!alive && !b.Dead)
        {
            b.Dead = true; b.DeathTime = 0; b.Attack.clear(); b.Queued = 0; b.Jumping = false; b.Air = 0; b.Rise = b.Touch = -1;
            b.Cast = 0; b.SwordsOut = false; b.Pushed = -1; mGuarding = false;
            b.Finish = 0; b.FinishTarget = -1; b.FinishLiftNow = 0; b.FinishGo = {0, 0};
            if (b.Soul != 0) { b.Soul = 0; mSoulCooldown = SoulCooldownSeconds * mCooldownScale; if (mSoul) mSoul->Drop(); }   // (her soul goes with her life; Q rests)
            b.Log(world, "death");
        }
        else if (alive && b.Dead) { b.Dead = false; b.Log(world, "alive"); }
        if (b.Dead) b.DeathTime += dt;

        // ---- her soul (Q). Put elsewhere by the game (more than SoulPutAway m from where the kit last left her: a nest,
        // the start), she lets it go. It stands still while the game does (a map, her menu: Update's deltaTime 0) and
        // while her window shows her (SetShowcase: she stands over the summit then). The second Q takes her back to it
        // along its strings, from wherever she is (in the air too), whatever she is doing but finishing one or V; on her
        // way back she does nothing else and nothing touches her (Hurt). She is put on her way (not moved by the world:
        // her way is the strings', through what stands between), and stands where she left it, turned as it stands.
        const bool soulLive = dt > 0 && !mShowcase;
        if (soulLive && b.Soul != 0 && b.SoulSeenKnown &&
            std::hypot(p.Position.x - b.SoulSeen.x, p.Position.y - b.SoulSeen.y, p.Position.z - b.SoulSeen.z) > SoulPutAway)
        {
            LetSoulGo();
            b.Log(world, "soul let go: she was put elsewhere");
        }
        if (b.SoulMissDue)
        {   // (how far from it she ended, a frame after)
            b.SoulMissDue = false;
            b.SoulLastMiss = std::hypot(p.Position.x - b.SoulAt.x, p.Position.y - b.SoulAt.y, p.Position.z - b.SoulAt.z);
            b.Log(world, ("soul joined, off it " + std::to_string(b.SoulLastMiss) + " m").c_str());
        }
        if (b.Soul != 0) b.SoulClock += dt;
        // Its leaving's throw over (SoulOutTo: 6 m on, on her feet), what is left of it, her coming to her guard, is given
        // up at once for her way (W, S, A, D; a scripted run's move or turn), a cut (clicked as it threw her too: kept,
        // below), a skill, a jump, or Q (back to it at once). The owner, 2026-11-26: "after Q character slightly has delay
        // before she continues to walk - let it be able to fight right after Q right away": it held her to its end, 0.36 s more.
        if (b.Soul == 1 && b.Attack == Body::SoulClip && b.AttackTime >= SoulOutTo && alive && !b.Dead)
        {
            const bool way = (s.Unattended && (s.ScriptedMove != 0 || s.ScriptedSteer != 0)) ||
                             (live && (IsDoorwaysKeyDown('W') || IsDoorwaysKeyDown('S') || IsDoorwaysKeyDown('A') || IsDoorwaysKeyDown('D')));
            if (way || clickL || clickR || b.Queued != 0 || space || keyF || keyV || keyC || keyQ)
            {
                b.Attack.clear();
                b.Log(world, way ? "soul's leaving given up for her way" : "soul's leaving given up");
            }
        }
        if (b.Soul == 1 && b.Attack != Body::SoulClip) b.SoulGone(world);
        if (keyQ && b.Soul == 2 && mSoul && alive && !b.Dead && !b.Finish && !b.Cast)
        {
            if (b.InMove()) { b.EndMove(world, "move given up for her soul"); b.ComboButton = -1; }
            b.Attack.clear(); b.Queued = 0; b.Jumping = false; b.JumpReal = false; b.Air = 0; b.Rise = b.Touch = -1;
            if (flying)
            {   // (out of the air: on foot again as she goes, as when her wings are spent)
                p.TraversalMode = DoorwaysPlayerTraversalMode::Ground; p.JumpFlight.Active = false;
                s.FreeFlightEnd = false; s.FreeFlightLandSpeed = 0; s.FreeFlightTakeoff = false;
                flying = false; mFalling = false; b.WasFlying = false;
            }
            const float apart = std::hypot(b.SoulAt.x - p.Position.x, b.SoulAt.y - p.Position.y, b.SoulAt.z - p.Position.z);
            b.Soul = 3; b.SoulTime = 0; b.SoulFrom = p.Position; b.SoulFromYaw = p.Yaw; b.SoulGhostWay = 0; b.SoulGhostWas = p.Position;
            b.SoulBack = std::clamp(SoulBackLeast + SoulBackPer * apart, SoulBackShortest, SoulBackMost);
            {   // (pulled at her chest: her body is thrown the way it pulls, back if it is behind her)
                const float ahead = (b.SoulAt.x - p.Position.x) * std::sin(p.Yaw) + (b.SoulAt.z - p.Position.z) * std::cos(p.Yaw);
                b.FlinchGo += ahead >= 0 ? SoulFlinch : -1.25f * SoulFlinch;
            }
            mSoul->Recall();
            CaucasianEagle::Sound::Play("nyxara/skill", .6f);
            b.Log(world, ("soul recalled from " + std::to_string(apart) + " m, " + std::to_string(b.SoulBack) + " s").c_str());
        }
        if (b.Soul == 3 && soulLive)
        {
            b.SoulTime += dt;
            const float t = std::clamp(b.SoulTime / b.SoulBack, 0.f, 1.f), u = t * t * (3 - 2 * t);
            p.Position = Lerp(b.SoulFrom, b.SoulAt, u); p.VerticalVelocity = 0; s.ImpulseX = s.ImpulseZ = 0;
            {   // (her facing: as she was as she set off, turned as it stands over the last third of the way, so that she is
                // in it again as it stood)
                const auto turning = [](float v) { v = std::clamp(v, 0.f, 1.f); return v * v * (3 - 2 * v); };
                p.Yaw = std::remainder(b.SoulFromYaw + std::remainder(b.SoulYaw - b.SoulFromYaw, XM_2PI) * turning((t - .67f) / .33f), XM_2PI);
            }
            if (t >= 1)
            {   // in it again
                p.Position = b.SoulAt; p.Yaw = b.SoulYaw;
                b.Soul = 0; mSoulCooldown = SoulCooldownSeconds * mCooldownScale; b.SoulLastBack = b.SoulTime; b.SoulMissDue = true;   // (its skill over: Q rests)
                if (mSoul) mSoul->Join();
                mShake = (std::max)(mShake, .16f);
                CaucasianEagle::Sound::Play("nyxara/guard", .7f);
            }
        }
        if (soulLive) { b.SoulSeen = p.Position; b.SoulSeenKnown = true; }

        // ---- leaving the ground and coming down (the owner takes her up and lands her: R in the journey)
        b.FlyTime += dt;
        if (flying && !b.WasFlying && !b.Dead)
        {   // the engine lifts her at once: the clip from where she springs up
            b.Rise = RiseLifts; b.Touch = -1; b.Jumping = false; b.Attack.clear(); b.Queued = 0;
            b.Log(world, "takeoff");
        }
        if (b.Rise >= 0)
        {
            b.Rise += dt;
            if (b.Rise >= (std::max)(b.Duration("Fly_Rise"), RiseLifts + .05f)) { b.Rise = -1; b.FlyTime = 0; }
        }
        const bool landing = flying && (s.FreeFlightLandSpeed > 0 || s.FreeFlightEnd);
        if (landing && b.Touch < 0 && !b.Moving())
        {   // her legs reach down LandReachTime before her feet touch, and wait for the ground
            const float reach = (std::max)(.25f, s.FreeFlightLandSpeed * LandReachTime);
            if (mHeightOverGround >= 0 ? mHeightOverGround <= reach : p.IsGrounded) { b.Touch = 0; b.Log(world, "landing"); }
        }
        if (!flying && b.WasFlying && !b.Dead && b.Touch < 0) b.Touch = LandReachTime;   // down without the reach (an owner that does not say her height)
        if (b.Touch >= 0)
        {
            b.Touch += dt;
            if (flying) b.Touch = (std::min)(b.Touch, LandReachTime);
            else if (b.Touch >= b.Duration("Fly_Land")) b.Touch = -1;
        }
        b.WasFlying = flying;
        const bool busy = b.Rise >= 0 || b.Touch >= 0 || b.Air != 0 || b.Dead || b.Finish != 0 || b.Soul == 3;   // leaving the ground or coming down, finishing one: nothing else

        // ---- her wing shield: E pressed, ShieldSeconds round her whatever she does (ShieldSeconds, above)
        {
            const bool pressE = k.E && !mEWas;
            mEWas = k.E;
            if (mGuarding)
            {
                mGuardHeld += dt;
                if (mGuardHeld >= ShieldSeconds || !alive || b.Dead) { mGuarding = false; mGuardCooldown = ShieldRestSeconds * mCooldownScale; }
            }
            else if (pressE && alive && !b.Dead)
            {
                if (mGuardCooldown <= 0)
                {
                    CaucasianEagle::Sound::Play("nyxara/guard", .7f); mNotice = L"WING SHIELD"; mNoticeTimer = .8f;
                    mGuarding = true; mGuardHeld = 0; mBarrier = 1;   // (up at once: a press is a blow stopped)
                    b.Log(world, "shield");
                }
                else { mNotice = L"WING SHIELD RESTING"; mNoticeTimer = .5f; }
            }
        }

        // ---- a style's move once it is free: given up for a jump, a skill or the barrier pressed now (and for her own way:
        // the travel, below)
        if (b.MoveFree() && (space || keyF || keyV || keyC)) { b.EndMove(world, "move given up"); b.ComboButton = -1; }
        // (and Space at any moment of one, on foot: she jumps out of her combo; owner, 2026-11-21: "while rmb and lmb combos
        // if i press jump it doesnt act, so it should jump up even while doing combos")
        else if (b.InMove() && space && !flying && alive && !busy && !b.Cast) { b.EndMove(world, "move given up for a jump"); b.ComboButton = -1; }

        // ---- Space: the jump clip once (a picture of a jump, as in the lab: the engine's jump is off on foot)
        if (space && !flying && !busy && !b.Cast && !b.Jumping && !b.Carried() && alive)
        {
            b.Attack.clear(); b.Queued = 0; b.Jumping = true; b.JumpTime = 0;
            b.JumpReal = false; b.JumpAir = false; b.JumpClock = 0; b.Dropped = 0;
            float jumpLift = 0;
            // a real jump where the game asks for one: from the ground, or from the brink of an edge she stands on
            if (mRealJump > 0 && p.VerticalVelocity <= 0 &&
                (p.IsGrounded || p.GroundState == Doorways::DoorwaysPhysicsGroundState::UnsupportedAirPrepared))
            {
                // From the ground she is on (2026-11-20, the owner: "jumping is blocked while walking up on staris or
                // hills"). Going up, her feet are behind the ground they follow (the engine eases her height to it, 12 a
                // second: speed x slope / 12 under it, 10 cm at a jog up 20 degrees, 30 at a run up the stairs), and her
                // jump's first frame did not take them out of it: the ground over her feet was taken for her landing
                // (EaglesAscent::KeepOutOfRock, the engine's sweep for rising ground) and the jump was over as it began.
                if (p.IsGrounded && p.GroundY > p.Position.y && p.GroundY - p.Position.y < .6f) { jumpLift = p.GroundY - p.Position.y; p.Position.y = p.GroundY; }
                p.VerticalVelocity = mRealJump; p.IsGrounded = false;
                p.GroundState = Doorways::DoorwaysPhysicsGroundState::Airborne;
                p.MissingAuthoritativeGround = false; p.GroundY = p.TargetGroundY = p.Position.y;
                b.JumpReal = true;
            }
            char jumpSaid[48];
            std::snprintf(jumpSaid, sizeof jumpSaid, "jump %s lifted %.2f", b.JumpReal ? "real" : "clip-only", jumpLift);
            b.Log(world, jumpSaid);
        }
        else if (b.Jumping && b.JumpReal)
        {   // A jump the world moves her in (2026-11-19): the clip is played by her rise and fall, not by the clock. It
            // lasts 0.9 s and she is 1.06 s in the air on flat ground (and as long as it takes off a ledge): by the clock
            // its landing played in mid-air, and then her stride, running on nothing. Rising, the clip goes from its start
            // to its top as she loses her speed upwards; falling, from the top towards its landing as she gathers speed
            // downwards, and waits there, a leg reaching down, for the ground.
            b.JumpClock += dt;
            const bool up = !p.IsGrounded && !flying;
            if (up) b.JumpAir = true;
            if (flying) b.Jumping = false;
            else if (up || (!b.JumpAir && b.JumpClock < .15f))
            {
                const float v = p.VerticalVelocity / (mRealJump > 0 ? mRealJump : 9.5f);
                const float goal = v > 0 ? JumpApex * (1 - (std::min)(1.f, v)) : JumpApex + (JumpHold - JumpApex) * (std::min)(1.f, -v);
                b.JumpTime = std::clamp(goal, b.JumpTime, b.JumpTime + dt * 3);
                b.Dropped = v <= -1 ? b.Dropped + dt : 0;
            }
            else
            {
                // On the ground: into her stride at the moment its left foot comes down (the foot the clip lands on), or
                // into her stand; her knees take the landing (below). The clip's own landing is a deep lunge on one knee,
                // which at a run slid along the ground for a quarter of a second: it is not played.
                b.Dropped = 0; b.Jumping = false;
                b.Phase = .276f + (.381f - .276f) * b.GaitRun;
            }
        }
        else if (b.Jumping && (b.JumpTime += dt) >= b.Duration("Jump")) { b.Jumping = false; b.Phase = 0; }
        {   // Off an edge on foot (2026-11-19): she went on walking or standing in the air until the ground came. Now, a
            // moment off the ground, she is in the jump from its top on: falling, a leg reaching down, and lands as from a jump.
            b.AirSeconds = !p.IsGrounded && !flying && alive ? b.AirSeconds + dt : 0;
            if (!b.Jumping && !busy && !b.Cast && !b.Carried() && b.Attack.empty() && !mFalling && b.Duration("Jump") > JumpLand &&
                (b.AirSeconds > .18f || (b.AirSeconds > .05f && p.VerticalVelocity < -4)))
            {
                b.Jumping = true; b.JumpReal = true; b.JumpAir = true; b.JumpClock = 1; b.JumpTime = JumpApex + .03f; b.Dropped = 0;
                b.Log(world, "off an edge");
            }
        }
        {   // Her knees take a landing (2026-11-19): she came down onto straight legs. Coming to the ground on foot her hips
            // are let down by how hard she lands (12 cm from a jump on flat ground, 25 from a long drop) and come up again
            // in a quarter of a second, a spring without a bounce; her feet stay where they are (PlantFeet bends her legs).
            constexpr float Spring = 14.f, Most = .28f;
            if (!p.IsGrounded) b.FellAt = flying ? 0.f : p.VerticalVelocity;
            else if (!b.WasGrounded)
            {
                if (!flying && b.Touch < 0 && !b.Dead && !b.Carried() && !b.Cast && b.FellAt < -2)
                {
                    const float hard = -b.FellAt;
                    b.SquatGo += (std::min)(hard, 10.f) * .5f + (std::clamp(hard, 10.f, 18.f) - 10.f) * .6f;
                }
                b.FellAt = 0;
            }
            b.WasGrounded = p.IsGrounded;
            for (float left = dt; left > 1e-6f;)
            {
                const float h = (std::min)(left, 1 / 120.f); left -= h;
                b.SquatGo += (-Spring * Spring * b.Squat - 2 * Spring * b.SquatGo) * h;
                b.Squat += b.SquatGo * h;
            }
            if (b.Squat > Most) { b.Squat = Most; b.SquatGo = (std::min)(0.f, b.SquatGo); }
            if (b.Squat < 0 || flying || b.Dead) { b.Squat = 0; b.SquatGo = (std::max)(0.f, b.SquatGo); }
        }
        {   // in flight Space is the engine's climb; on foot the kit owns it
            auto& held = s.SuppressedActions;
            const auto found = std::find(held.begin(), held.end(), DoorwaysInputAction::JumpOrAscend);
            if (flying && found != held.end()) held.erase(found);
            else if (!flying && found == held.end()) held.push_back(DoorwaysInputAction::JumpOrAscend);
        }

        // ---- her finishing kills: a doomed creature near her and she free on foot, she finishes it (before anything
        // else she might begin this frame); the doomed wait, and the dead fall
        {
            const bool canFinish = alive && !busy && !b.Cast;
            UpdateFinish(world, dt, canFinish);
            UpdateDoomed(dt);
        }
        // ---- the mouse: the left sword and the right one; clicks in a row go through the cuts
        const bool free = alive && !busy && !b.Cast && !b.Finish;
        if (free && (clickL || clickR)) { b.Queued = clickL ? 1 : 2; b.QueuedAt = b.Clock; }
        // ---- C, F, V
        const auto cooling = [&](int slot)
        {
            if (mSkillCooldown[slot] <= 0) return false;
            mNotice = L"SKILL RECOVERING"; mNoticeTimer = .5f;
            return true;
        };
        if (keyV && free && !b.Jumping && !b.Carried() && !cooling(1))
        {   // the nearest living creature before her within range: without one V does nothing
            float best = flying ? CastRange * CastAirRange : CastRange;
            int chosen = -1;
            for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
            {
                if (!mWraiths[i].Alive || mWraiths[i].Doom > 0) continue;   // (a doomed one is hers to finish)
                const Foe f = foe(i);
                const float away = std::hypot(f.Feet.x - p.Position.x, f.Feet.z - p.Position.z) - f.Radius;
                if (away > best || !before(f)) continue;
                best = away; chosen = i;
            }
            if (chosen >= 0)
            {
                const Foe f = foe(chosen);
                b.CastAt = f.Feet; b.CastTarget = chosen; b.CastGround = f.Feet.y;
                b.PushStick = (std::max)(.1f, f.Neck.y - f.Feet.y);
                float tx = p.Position.x - f.Feet.x, tz = p.Position.z - f.Feet.z;   // from it towards her
                const float d = (std::max)(.01f, std::hypot(tx, tz));
                tx /= d; tz /= d;
                p.Yaw = std::atan2(-tx, -tz);
                const float high = (std::max)(CastLowest, f.Height * CastMiddle);   // the middle of its body
                // Her swords stick in it there, over half way to its middle, pointing in and a little down, side by side as her
                // fists hold them on guard (GrabGrip), at least a quarter of a metre apart: each where that hand will take it
                const float tall = (std::max)(.01f, world.AnimatedPlayerVisual.Scale.y);
                const float length = XMVectorGetX(XMVector3Length(XMLoadFloat3(&b.SwordTip[0]) - XMLoadFloat3(&b.SwordGrip[0])));
                b.CastGripAt = f.Radius + length - (std::min)(.55f * f.Radius, .55f * length);   // (a great creature's middle is far in)
                b.CastRadius = f.Radius;
                b.CastHandles = {f.Feet.x + tx * b.CastGripAt, f.Feet.y + high, f.Feet.z + tz * b.CastGripAt};
                for (int k2 = 0; k2 < 2; ++k2)
                {   // (x her right: (-tz, tx))
                    const float half = b.GrabGrip[k2].x - .5f * (b.GrabGrip[0].x + b.GrabGrip[1].x);
                    const float ox = tall * std::copysign(std::clamp(std::abs(half), .125f, .25f), k2 == 0 ? (half != 0 ? half : 1.f) : (half != 0 ? half : -1.f));
                    b.SwordTo[k2] = {b.CastHandles.x - tz * ox, b.CastHandles.y, b.CastHandles.z + tx * ox};
                    b.SwordToWay[k2] = {-tx, -.08f, -tz};
                }
                b.StartAttack(world, "Skill_Cast");
                b.SkillStrike = 1e9f;
                b.Cast = 1; b.CastTime = 0; b.CastStruck = false;
                mSkillCooldown[1] = SkillCooldown[1] * mCooldownScale;
                CaucasianEagle::Sound::Play("nyxara/skill", .8f);
                b.Log(world, ("cast at " + std::to_string(chosen) + " height " + std::to_string(f.Height)).c_str());
            }
            else { mNotice = L"NO CREATURE BEFORE HER"; mNoticeTimer = .7f; }
        }
        if ((keyC || keyF) && free && !b.Jumping && !b.Carried())
        {
            if (keyF && flying)
            {   // F in flight: the twisted dash, where it is asked for (SetFlightDash); else nothing, and a word of it
                if (!mFlightDash) { mNotice = L"THE SLIDE: FROM THE GROUND"; mNoticeTimer = .7f; }
                else if (!cooling(0))
                {
                    b.Air = 1; b.AirTime = 0; b.TwistNow = 0;
                    b.DashFrom = p.Position; b.DashStruck.clear();
                    mSkillCooldown[0] = SkillCooldown[0] * mCooldownScale;
                    CaucasianEagle::Sound::Play("nyxara/dash", .85f);
                    CaucasianEagle::Sound::Play("nyxara/wing", .6f);
                    b.Log(world, "twisted dash");
                }
            }
            else if (keyF)
            {   // F: the slide (2026-11-19: no longer the finishing throw at a broken creature; the owner: "remove f finisher
                // from nyxara we dont need it anymore")
                if (!cooling(0))
                {
                    b.SkillReach = 1;
                    b.StartAttack(world, "Skill_Slide"); b.SkillStrike = 1e9f;   // (its cuts: by her way, below)
                    b.Pace = SlidePace; b.SlideWas = p.Position; b.SlideGone = b.SlideTold = 0; b.SlideCuts.clear(); b.SlideWake = false;
                    mSkillCooldown[0] = SkillCooldown[0] * mCooldownScale;
                    CaucasianEagle::Sound::Play("nyxara/dash", .85f);
                }
            }
            else if (flying) { mNotice = L"FEATHER STORM: FROM THE GROUND"; mNoticeTimer = .7f; }
            else if (!cooling(2))
            {   // C: the feather storm
                b.SkillReach = 1;
                b.StartAttack(world, "Skill_Storm");
                b.StormVolley = 0; b.SkillStrike = b.StormVolleyAt[0]; b.StormFeathers = 0;
                mSkillCooldown[2] = SkillCooldown[2] * mCooldownScale;
                CaucasianEagle::Sound::Play("nyxara/skill", .85f);
            }
        }
        // ---- Q, her soul (THREAD STEP): on foot, free, she leaves it where she stands and is thrown on; her leaving over,
        // she is away from it. Out of her cuts and combos at any moment of them, and out of her slide (the owner, 2026-11-26:
        // "Q can be pressed anytime while fighting/combo"); not in the air, nor out of V's thrown swords or C's storm (a leap)
        if (keyQ && b.Soul == 0)
        {
            const bool fighting = b.InMove() || b.Attack == "Skill_Slide";
            if (flying) { mNotice = L"THREAD STEP: FROM THE GROUND"; mNoticeTimer = .7f; }
            else if (!mSoul || b.Duration(Body::SoulClip) <= 0) { mNotice = L"THREAD STEP: NOT MADE"; mNoticeTimer = .7f; }
            else if (mSoulCooldown > 0) { mNotice = L"SKILL RECOVERING"; mNoticeTimer = .5f; }
            else if (free && !b.Jumping && (fighting || !b.Carried()))
            {
                b.SoulOutOf = b.Attack;
                if (b.InMove()) { b.EndMove(world, "move given up for her soul"); b.ComboButton = -1; }
                b.Soul = 1; b.SoulLeaving = true; b.SoulClock = 0; b.SoulGhostWay = 0; b.SoulGhostWas = p.Position;
                b.SoulAt = p.Position; b.SoulYaw = p.Yaw;
                b.SkillReach = 1;
                b.StartAttack(world, Body::SoulClip); b.SkillStrike = 1e9f; b.Pace = 1 / AttackRate;
                mNotice = NyxaraSoul::Name; mNoticeTimer = .8f;
                CaucasianEagle::Sound::Play("nyxara/dash", .8f);
                b.Log(world, b.SoulOutOf.empty() ? "soul left" : ("soul left, out of " + b.SoulOutOf).c_str());
            }
        }
        if (b.Soul == 1 && b.Attack != Body::SoulClip) b.SoulGone(world);
        if (b.Skill() && b.Attack != Body::SoulClip) b.Queued = 0;   // (but clicked as her soul's leaving throws her: the cut as its throw is over)
        // (with an attack style, on foot: each click its button's move, aimed at the creature most before her and nearest)
        // (with a combo: its button's string, going on from the move before it if that was of this string and now or a
        // moment ago, ComboWindow, played; else from its first)
        // (on foot and in flight alike: the owner, 2026-11-20, "nyxara should be fight same way while flying as she does now,
        // while flying she no uses old technique")
        const bool comboed = mCombo >= 0 && ComboMade(mCombo);
        const bool styled = comboed || (mStyle >= 0 && StyleMade(mStyle));
        const auto startMove = [&](int button)
        {
            const StyleMove* chosen = &Styles()[static_cast<size_t>((std::max)(0, mStyle))].Move[button];
            if (comboed)
            {
                const auto& string = Combos()[static_cast<size_t>(mCombo)].String[button];
                const bool goesOn = b.ComboButton == button && b.ComboStep + 1 < static_cast<int>(string.size()) &&
                                    (b.InMove() || b.Clock - b.ComboEndedAt <= ComboWindow / b.Pace);
                b.ComboStep = goesOn ? b.ComboStep + 1 : 0; b.ComboButton = button;
                chosen = &RefMove(string[static_cast<size_t>(b.ComboStep)]);
                if (mHiggsfieldTrial && chosen == &RefMove(FlashStep) && b.Clips.count(FlashStepTrial().Clip)) chosen = &FlashStepTrial();
            }
            const StyleMove& m = *chosen;
            int target = -1; float best = 1e9f, away = -1;
            for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
            {
                if (!mWraiths[i].Alive || mWraiths[i].Doom > 0) continue;   // (a doomed one is hers to finish)
                const Foe f = foe(i);
                const float dx = f.Feet.x - p.Position.x, dz = f.Feet.z - p.Position.z, edge = (std::max)(0.f, std::hypot(dx, dz) - f.Radius);
                if (edge > StyleSeek || std::abs(f.Feet.y - p.Position.y) > 2.5f) continue;
                const float off = std::abs(std::remainder(std::atan2(dx, dz) - p.Yaw, XM_2PI));
                if (off > XMConvertToRadians(edge < StyleNear ? StyleConeNear : StyleCone)) continue;
                const float score = edge + 1.5f * off;
                if (score < best) { best = score; target = i; away = edge; }
            }
            b.StartMove(world, m, button, target, away);
        };
        if (b.Queued && free)
        {
            // (in a jump: her old leaping cut only without a combo. With one, its move, as on the ground: the old cut, and the
            // old cuts it went on into, came back after every jump; owner, 2026-11-21: "examine combo after jump because i
            // notice it still continues old skills attacks combo ... fix it with new attacks". The world goes on carrying
            // her up and down; out of the move still in the air, she falls as off an edge)
            if (b.Jumping && !styled) b.StartAttack(world, "Attack_Jump", b.JumpTime);
            else if (b.Jumping) { b.Jumping = false; startMove(b.Queued - 1); }
            else if (b.InMove())
            {   // clicked during a move: kept a moment (StyleBuffer) and begun as soon as the move is free
                if (!styled || b.Clock - b.QueuedAt > StyleBuffer / b.Pace) b.Queued = 0;   // (slower in the trial, as its moves are)
                else if (b.MoveFree()) startMove(b.Queued - 1);
            }
            else if (b.Attack.empty())
            {
                if (styled) startMove(b.Queued - 1);
                else { b.Step = 0; b.StartAttack(world, b.NextCut(b.Queued)); }
            }
        }
        else if (!free) b.Queued = 0;

        // ---- F in flight: the twisted dash. What her way goes through is struck, each once: every creature whose middle her
        // line since a frame ago passes within its width and DashWide, at the height of her body
        if (b.Air)
        {
            b.AirTime += dt;
            b.Queued = 0;
            if (!flying) b.Air = 0;
            else
            {
                const float ax = b.DashFrom.x, az = b.DashFrom.z, gx = p.Position.x - ax, gz = p.Position.z - az, gone = gx * gx + gz * gz;
                const float body = p.Position.y + b.LiftNow + .9f;   // (her body, laid flat)
                int struck = 0;
                for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                {
                    if (!mWraiths[i].Alive || std::find(b.DashStruck.begin(), b.DashStruck.end(), i) != b.DashStruck.end()) continue;
                    const Foe f = foe(i);
                    const float u = gone > 1e-8f ? std::clamp(((f.Feet.x - ax) * gx + (f.Feet.z - az) * gz) / gone, 0.f, 1.f) : 1.f;
                    const float bx = ax + gx * u, bz = az + gz * u;   // the nearest her line came to it
                    if (std::hypot(f.Feet.x - bx, f.Feet.z - bz) > f.Radius + DashWide) continue;
                    if (body < f.Feet.y - .3f || body > f.Feet.y + f.Height + .5f) continue;   // under it, or over it
                    // (thrown aside, away from her line)
                    const float len = std::sqrt((std::max)(gone, 1e-8f)), sx = -gz / len, sz = gx / len;
                    const float side = (f.Feet.x - bx) * sx + (f.Feet.z - bz) * sz >= 0 ? 1.f : -1.f;
                    const XMFLOAT3 from{bx - sx * side * .3f, p.Position.y, bz - sz * side * .3f};
                    b.Wound(f.Feet, f.Height, f.Radius, from, side, true, DashDamage);
                    DamageWraith(i, DashDamage, from, DashPush, 0);
                    b.DashStruck.push_back(i);
                    ++struck;
                }
                if (struck) b.Log(world, ("twisted dash struck " + std::to_string(struck)).c_str());
                b.DashFrom = p.Position;
                if (b.AirTime >= DashSeconds) { b.Air = 0; b.RingDue = true; }
            }
        }

        // ---- V: the thrown swords
        if (b.Cast)
        {
            if (b.CastTarget >= 0 && b.CastTarget < static_cast<int>(mWraiths.size()) && mWraiths[b.CastTarget].Alive && (b.Cast < 5 || !b.CastKicked))
                mWraiths[b.CastTarget].Pinned = true;   // her swords are in it: it waits for her
            else if (b.CastTarget >= 0 && (b.CastTarget >= static_cast<int>(mWraiths.size()) || !mWraiths[b.CastTarget].Alive))
                b.CastTarget = -1;                      // it died: the swords stay where they were
            const float release = Body::Hit("Skill_Cast");
            const float dx = b.CastAt.x - p.Position.x, dz = b.CastAt.z - p.Position.z, away = std::hypot(dx, dz);
            p.Yaw = std::atan2(dx, dz);
            b.Queued = 0;
            b.CastTime += dt;
            if (b.Cast <= 3) b.AttackTime = (std::min)(b.AttackTime + dt * CastRate, b.Duration("Skill_Cast"));
            if (b.Cast == 1 && b.AttackTime >= release)
            {
                b.Cast = 2; b.SwordsOut = b.SwordCapture = true; b.SwordFlight = b.SwordBack = 0;
                CaucasianEagle::Sound::Play("nyxara/wing", .5f);
                b.Log(world, "cast thrown");
            }
            else if (b.Cast == 2 && !b.SwordCapture)
            {
                b.SwordFlight = (std::min)(1.f, b.SwordFlight + dt * CastSwordSpeed / (std::max)(away, 1.f));
                if (b.SwordFlight >= 1)
                {
                    strikeOne(b.CastTarget, CastDamage);
                    b.Cast = 3; b.CastTime = 0;
                    b.Log(world, "cast stuck");
                }
            }
            else if (b.Cast == 3 && b.CastTime >= CastWait)
            {   // after them (2026-11-19; MakeCast, TurnBody): her picture twisting to them as she used to, feet first, until
                // her feet are on it; her place to where she will land, FlipBack short of the handles, over the same time
                b.Attack.clear();
                b.Cast = 4; b.CastTime = 0;
                const float tall = (std::max)(.01f, world.AnimatedPlayerVisual.Scale.y);
                b.CastLand = b.CastGripAt + FlipBack;
                b.CastRootFrom = p.Position;
                b.CastRootWay = away - b.CastLand;
                b.CastRootY = flying ? b.CastGround + CastAirHeight : p.Position.y;
                b.CastDiveTime = (std::max)({DiveLeast, (away - b.CastGripAt) / CastDash, std::abs(b.CastRootY - p.Position.y) / CastAirSink});
                b.CastTurns = (std::max)(1.f, std::round(b.CastDiveTime * CastSpin * 1.5f));
                b.CastTwist = b.TwistNow = 0;
                // where her feet stick on it (on its side, FeetUnder under the handles), her middle (her hips) as they do, and
                // where it is now (her picture lifted as it is)
                const float fx = std::sin(p.Yaw), fz = std::cos(p.Yaw);
                const auto inWorld = [&](XMVECTOR v) { return XMVectorSet(XMVectorGetX(v) * fz + XMVectorGetZ(v) * fx, XMVectorGetY(v), -XMVectorGetX(v) * fx + XMVectorGetZ(v) * fz, 0); };
                XMStoreFloat3(&b.CastFeetAt, XMLoadFloat3(&b.CastHandles) + inWorld(XMVectorSet(0, -FeetUnder * tall, b.CastGripAt - b.CastRadius - FlipSole, 0)));
                XMStoreFloat3(&b.CastDrillTo, XMLoadFloat3(&b.CastFeetAt) -
                                                  inWorld(tall * Body::Pitched(Body::Load3(Body::FlipAt(b.FlipFeetAt, 0)) - XMLoadFloat3(&b.DiveHips), -b.DrillLean)));
                XMStoreFloat3(&b.CastFrom, XMLoadFloat3(&p.Position) + XMVectorSet(0, b.LiftNow, 0, 0) + inWorld(tall * XMLoadFloat3(&b.DiveHips)));
                CaucasianEagle::Sound::Play("nyxara/wing", .8f);
                b.Log(world, ("cast twist " + std::to_string(static_cast<int>(b.CastTurns)) + " turns over " + std::to_string(b.CastDiveTime) + " s").c_str());
            }
            if (b.Cast == 4)
            {   // by its time: her twist gathers speed over the first third of her way and the last turn slows into her feet
                // on it (2026-11-19: it began at its fastest, and her hands went from rest to 27 m/s in a frame); her place
                // goes on the same pace as her picture (on foot the engine moves her: ForwardSpeed)
                const float come = std::clamp(b.CastTime / b.CastDiveTime, 0.f, 1.f), left = 1 - come;
                b.CastTwist = 1 - left * left * left * (1 + 3 * come);
                const float go = Body::Along(come);
                b.CastRootGo = dt > 1e-4f ? b.CastRootWay * (Body::Along((b.CastTime + dt) / b.CastDiveTime) - go) / dt : 0.f;
                if (flying)
                {   // in the air she is moved here
                    p.Position.x = b.CastRootFrom.x + std::sin(p.Yaw) * b.CastRootWay * go;
                    p.Position.z = b.CastRootFrom.z + std::cos(p.Yaw) * b.CastRootWay * go;
                    p.Position.y = b.CastRootFrom.y + (b.CastRootY - b.CastRootFrom.y) * go;
                    p.VerticalVelocity = 0;
                }
                if (come >= 1)
                {   // her feet on it: she crouches on it, takes the handles, and backflips off it
                    b.Cast = 5; b.CastTime = 0; b.CastKicked = b.CastStruck = false; b.HandleGap = 0;
                    b.StartFlip(world);
                    b.RingDue = true;   // her twist stops: the turned air is thrown out in a ring
                    CaucasianEagle::Sound::Play("nyxara/land", .55f);
                    b.Log(world, "cast feet on it");
                }
            }
            else if (b.Cast == 5)
            {
                if (flying) p.VerticalVelocity = 0;
                if (b.CastTime >= FlipPush && !b.CastKicked)
                {   // she kicks off it: it goes over backwards, away from her
                    b.CastKicked = true;
                    const int target = b.CastTarget;
                    strikeOne(target, CastKick);
                    CaucasianEagle::Sound::Play("combat/blow", .8f, .1f);
                    b.Log(world, ("cast kicked off, her hands " + std::to_string(static_cast<int>(std::lround(b.HandleGap * 100))) + " cm off the handles at most").c_str());
                    if (target >= 0 && mWraiths[target].PushScale > .5f && mWraiths[target].Doom <= 0)   // (doomed by it, it stands for her)
                    {
                        b.Pushed = target; b.PushTime = 0; b.PushGone = -1;
                        b.PushFeet = b.CastAt;
                        const float ax = b.CastAt.x - p.Position.x, az = b.CastAt.z - p.Position.z, d = (std::max)(.01f, std::hypot(ax, az));
                        b.PushWay = {ax / d, 0, az / d};
                    }
                }
                // her swords come out with her hands as she kicks off
                const float out = std::clamp((b.CastTime - FlipPush) / FlipRip, 0.f, 1.f);
                b.SwordBack = out * out * (3 - 2 * out);
                if (b.SwordBack >= 1 && !b.CastStruck)
                {
                    b.CastStruck = true; b.SwordsOut = false;
                    strikeOne(b.CastTarget, CastPull);
                    b.Log(world, "cast pulled");
                }
                if (!flying && b.CastTime >= FlipLand && b.CastTime - dt < FlipLand)
                {   // on her feet again, facing it
                    CaucasianEagle::Sound::Play("nyxara/land", .7f);
                    RingEffect({p.Position.x, p.Position.y + .05f, p.Position.z}, 1.6f, MoonWhite, .35f);
                    b.Log(world, "cast landed");
                }
                if (b.CastTime >= (flying ? FlipLand + .12f : FlipSeconds))
                {
                    b.SwordsOut = false; b.Cast = 0; b.CastTarget = -1;
                    b.Log(world, "cast over");
                }
            }
        }
        else if (b.InMove())
        {   // ---- a style's move: turned onto its creature as it begins, carried in to it (or back), its blows as they come
            const StyleMove& m = *b.MoveOf;
            const float was = b.AttackTime;
            b.AttackTime += dt * b.Pace;
            const float t = b.AttackTime;
            if (b.MoveTarget >= static_cast<int>(mWraiths.size()) || (b.MoveTarget >= 0 && !mWraiths[b.MoveTarget].Alive)) b.MoveTarget = -1;
            if (b.MoveTarget >= 0 && was < m.TrackTo)
            {   // (her aim, her facing less the move's own spin, turned onto the creature)
                const Foe f = foe(b.MoveTarget);
                const float want = std::atan2(f.Feet.x - p.Position.x, f.Feet.z - p.Position.z), most = XMConvertToRadians(StyleTrackRate) * dt;
                p.Yaw = std::remainder(p.Yaw + std::clamp(std::remainder(want - (p.Yaw - b.MoveSpun), XM_2PI), -most, most), XM_2PI);
            }
            if (m.Spin != 0)
            {
                const float spun = m.Spin * Travel(t, m.SpinFrom, m.SpinTo);
                p.Yaw = std::remainder(p.Yaw + spun - b.MoveSpun, XM_2PI);
                b.MoveSpun = spun;
            }
            {   // carried in (or back) along her aim: each frame what is left between where the move means her to be and where
                // she is. The engine moves her at the start of the next frame, in slices of 1/60 s: ScaleFrameTime asks for
                // it then, over the slices it is about to take, so that a lunge of two or three frames ends where it should
                // whatever the frames' times. What waits for a slice is kept; what she cannot reach (a wall, a body in the
                // way) is let go past 1.5 m.
                const float aim = p.Yaw - b.MoveSpun;
                const float step = b.MovePlan * (Travel(t, m.LungeFrom, m.LungeTo) - Travel(was, m.LungeFrom, m.LungeTo)) -
                                   m.Back * (Travel(t, m.BackFrom, m.BackTo) - Travel(was, m.BackFrom, m.BackTo));
                b.MoveAt.x += std::sin(aim) * step;
                b.MoveAt.y += std::cos(aim) * step;
                float owex = b.MoveAt.x - p.Position.x, owez = b.MoveAt.y - p.Position.z;
                const float owed = std::hypot(owex, owez), most = std::abs(step) + 1.5f;   // (and never more than a frame's worth and 1.5 m)
                if (owed > most)
                {
                    owex *= most / owed; owez *= most / owed;
                    b.MoveAt = {p.Position.x + owex, p.Position.z + owez};
                }
                const bool going = t <= (std::max)(m.LungeTo, m.BackTo) + .1f;
                b.MoveVel = going ? XMFLOAT2{owex * 60, owez * 60} : XMFLOAT2{0, 0};   // (asked of one slice, if nothing asks again)
                b.MoveOwe = going ? XMFLOAT2{owex, owez} : XMFLOAT2{0, 0};
            }
            const bool lit = mHiggsfieldTrial && !flair;
            if (flair)
            {   // (her flair: the wake of a move carrying her in or back, the lines of a turn round her)
                if (was <= 0) mFlairDashing = false;
                const bool dashing = (m.Lunge > .8f && t >= m.LungeFrom && was < m.LungeTo + .03f) ||
                                     (m.Back > .6f && t >= m.BackFrom && was < m.BackTo + .03f);
                const XMFLOAT3 at{p.Position.x, p.Position.y + 1.f, p.Position.z};
                if (dashing)
                {
                    const bool first = !mFlairDashing || Len(Sub(at, mFlairFrom)) > 4.f;
                    if (first) mFlairFrom = at;
                    mFlair->Dash(mFlairFrom, at, first);
                    mFlairFrom = at;
                }
                mFlairDashing = dashing;
                if (m.Spin != 0 && t >= m.SpinFrom && was < m.SpinTo) mFlair->Whirl({p.Position.x, p.Position.y + 1.05f, p.Position.z}, flairDt);
            }
            if (lit)
            {   // (the Character Lab's trial, after Higgsfield's video: every move's light is lines, not balls) speed lines
                // left along her way while a move carries her in or back, a flash where she set off; while she turns, a
                // ring of lines round her at her blades' height
                if (was <= 0) mTrialDashing = false;
                const bool dashing = (m.Lunge > .8f && t >= m.LungeFrom && was < m.LungeTo + .03f) ||
                                     (m.Back > .6f && t >= m.BackFrom && was < m.BackTo + .03f);
                const XMFLOAT3 at{p.Position.x, p.Position.y + 1.f, p.Position.z};
                if (dashing)
                {
                    if (!mTrialDashing || Len(Sub(at, mTrialFrom)) > 4.f)
                    {
                        mTrialFrom = at;
                        Fx flash; flash.Kind = FxKind::Spark; flash.Pos = at; flash.Color = Alpha(ThemeMain(), .7f); flash.Life = .12f; flash.Size = 1.1f;
                        mFx.push_back(flash);
                    }
                    TrialDashLight(mTrialFrom, at);
                    mTrialFrom = at;
                }
                mTrialDashing = dashing;
                if (m.Spin != 0 && t >= m.SpinFrom && was < m.SpinTo) TrialWhirlLight({p.Position.x, p.Position.y + 1.05f, p.Position.z}, p.Yaw, m.Spin > 0);
                if (mTrialTheme == 0 && mTrialLevel > 0 && Random() < .25f * mTrialLevel)
                {   // (her wind, as her gear grows: lines of it going round her all through the move)
                    const float a = XM_2PI * Random(), r = .9f + .5f * Random();
                    Gusts({p.Position.x + std::sin(a) * r, p.Position.y + .3f + 1.4f * Random(), p.Position.z + std::cos(a) * r},
                          {std::cos(a), .15f, -std::sin(a)}, 1, 5.f + 1.5f * mTrialLevel, .05f);
                }
            }
            for (; b.MoveHitNext < static_cast<int>(m.Hits.size()) && t >= m.Hits[static_cast<size_t>(b.MoveHitNext)].At; ++b.MoveHitNext)
            {
                const StyleHit& h = m.Hits[static_cast<size_t>(b.MoveHitNext)];
                int struck = 0;
                if (h.Shape == 1)
                {   // a line along the ground before her: every creature within Arc m of it (its width counted), Reach m long
                    const XMFLOAT3 way = Forward(p.Yaw);
                    flairStruck.clear();
                    for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                    {
                        if (!mWraiths[i].Alive) continue;
                        const Foe f = foe(i);
                        const float dx = f.Feet.x - p.Position.x, dz = f.Feet.z - p.Position.z;
                        const float along = dx * way.x + dz * way.z, across = std::abs(dx * way.z - dz * way.x);
                        if (along < -f.Radius || along > h.Reach + f.Radius || across > h.Arc + f.Radius || std::abs(f.Feet.y - p.Position.y) > 2.5f) continue;
                        if (!flair) b.Wound(f.Feet, f.Height, f.Radius, p.Position, sideOf(), true, h.Damage);
                        DamageWraith(i, h.Damage, p.Position, h.Push, h.Launch);
                        if (flair) flairStruck.push_back({f.Feet, f.Height, f.Radius});
                        ++struck;
                    }
                    b.WaveFrom = p.Position; b.WaveWay = way; b.WaveReach = h.Reach; b.WaveTime = 0; b.WaveSparks = 0;
                    if (mBlowListener)
                        for (float at = 0; at <= h.Reach; at += 1.5f)   // (the line: as blows along it)
                            mBlowListener({p.Position.x + way.x * at, p.Position.y, p.Position.z + way.z * at}, p.Yaw, (std::max)(1.5f, h.Arc), 359.f);
                }
                else struck = strike(h.Reach, h.Arc, h.Damage, h.Push, h.Launch);
                CaucasianEagle::Sound::Play(h.Damage >= 70 ? "nyxara/swing_heavy" : "nyxara/swing", .5f, .15f);
                if (flair)
                {   // (her flair: the blow's light is its own, and what it asks of the game's feel: TakeFeel, below)
                    const bool lastHit = b.MoveHitNext + 1 >= static_cast<int>(m.Hits.size());
                    const bool lastMove = comboed && b.ComboButton >= 0 &&
                                          b.ComboStep + 1 >= static_cast<int>(Combos()[static_cast<size_t>(mCombo)].String[b.ComboButton].size());
                    NyxaraFlair::Blow blow;
                    blow.Her = p.Position; blow.Yaw = p.Yaw; blow.Hand = h.Hand; blow.Ring = h.Ring; blow.Reach = h.Shape == 1 ? 2.f : h.Reach;
                    blow.Weight = (lastHit && lastMove) || h.Damage >= 85 ? 2 : h.Damage >= 55 || (lastHit && m.Hits.size() > 1) ? 1 : 0;
                    blow.Whirl = m.Spin != 0 && h.Arc >= 150.f; blow.Rising = m.Hop > .05f || h.Launch > 0;
                    blow.Struck = flairStruck;
                    mFlair->Strike(blow);
                }
                if (!flair && struck && h.Stop > 0) mHitStop = (std::max)(mHitStop, h.Stop);
                if (!flair && (struck || h.Ring > 0) && h.Shake > 0) mShake = (std::max)(mShake, h.Shake);
                if (flair) { /* (its own light, above) */ }
                else if (h.Ring > 0)
                {   // the ground struck: a ring out over it, and sparks (the trial: lines of light out along the ground and up)
                    const XMFLOAT3 at = Add(p.Position, Mul(Forward(p.Yaw), .9f));
                    RingEffect({at.x, at.y + .05f, at.z}, h.Ring, lit ? ThemeMain() : MoonWhite, .45f);
                    if (lit) TrialGroundLight({at.x, at.y + .08f, at.z}, h.Ring, struck > 0);
                    else Burst({at.x, at.y + .15f, at.z}, MoonWhite, 18, 4.f, .22f);
                }
                else if (lit)
                {   // (the trial: light where the blade bites; a turning blow's all round her; a rising one's thrown up)
                    const XMFLOAT3 fwd = Forward(p.Yaw);
                    if (m.Spin != 0 && h.Arc >= 150.f) TrialWhirlBite({p.Position.x, p.Position.y + 1.05f, p.Position.z}, struck > 0);
                    else
                    {
                        const float out = std::clamp(h.Shape == 1 ? 1.2f : h.Reach * .55f, .9f, 1.4f);
                        const XMFLOAT3 way = m.Hop > .05f ? Norm(Add(fwd, {0, .8f, 0})) : fwd;
                        TrialBiteLight(Add(Add(p.Position, Mul(fwd, out)), {0, 1.1f, 0}), way, struck > 0);
                    }
                }
                if (lit && struck) { mHitStop = (std::max)(mHitStop, .09f); mShake = (std::max)(mShake, .18f); }
                if (struck) b.Log(world, ("move blow " + std::to_string(struck)).c_str());
            }
            if (t >= m.Seconds) b.EndMove(world, "move over");
        }
        else if (b.Skill())
        {
            const float was = b.AttackTime;
            b.AttackTime += dt * AttackRate * b.Pace;
            const bool storm = b.Attack == "Skill_Storm";
            if (storm && was < StormLeave && b.AttackTime >= StormLeave)
            {   // off the ground: her wings beat down
                CaucasianEagle::Sound::Play("nyxara/wing", .9f); CaucasianEagle::Sound::Play("nyxara/effort", .7f);
                RingEffect({p.Position.x, p.Position.y + .05f, p.Position.z}, 2.4f, MoonWhite, .45f);
            }
            if (storm && was < StormDown && b.AttackTime >= StormDown)
            {   // down again
                mShake = (std::max)(mShake, .12f);
                RingEffect({p.Position.x, p.Position.y + .05f, p.Position.z}, 3.f, MoonWhite, .5f);
                CaucasianEagle::Sound::Play("nyxara/wing", .55f);
            }
            if (b.AttackTime >= b.SkillStrike)
            {
                if (storm)
                {   // half-way round: three feather daggers at every creature within reach, one of them the blow; straight
                    // at it from beside her, bending in
                    const XMFLOAT3 from{p.Position.x, p.Position.y + b.LiftNow + 1.25f, p.Position.z};
                    int aimed = 0;
                    for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                    {
                        if (!mWraiths[i].Alive || mWraiths[i].Doom > 0) continue;
                        const Foe f = foe(i);
                        const float dx = f.Feet.x - p.Position.x, dz = f.Feet.z - p.Position.z, d = std::hypot(dx, dz);
                        if (d > StormReach + f.Radius || std::abs(f.Feet.y - p.Position.y) > 6.f) continue;
                        const XMFLOAT3 way = d > 1e-3f ? XMFLOAT3{dx / d, 0, dz / d} : Forward(p.Yaw), side{way.z, 0, -way.x};
                        for (int one = 0; one < 3; ++one)
                        {
                            Fx fx; fx.Kind = FxKind::Feather; fx.Target = i; fx.Damage = one == 0 ? StormDamage : 0;
                            const float out = .8f + .4f * Random(), across = (Random() - .5f) * .6f;
                            fx.Pos = Add(from, {way.x * out + side.x * across, (Random() - .5f) * .6f, way.z * out + side.z * across});
                            fx.Vel = Add(Add(Mul(way, StormDaggerSpeed * 1.1f), Mul(side, (Random() - .5f) * 4.f)), {0, Random(), 0});
                            fx.Life = 1.5f; fx.Size = one == 0 ? .27f : .23f; fx.Roll = Random() * XM_2PI; fx.Floor = p.Position.y;
                            fx.Color = one == 0 ? MoonWhite : Random() < .5f ? FeatherSilver : FeatherDark; fx.Glow = one == 0;
                            mFx.push_back(fx);
                        }
                        ++aimed;
                    }
                    CaucasianEagle::Sound::Play("nyxara/dash", .5f, .12f);
                    b.Log(world, ("storm turn " + std::to_string(b.StormVolley + 1) + " at " + std::to_string(aimed)).c_str());
                    ++b.StormVolley;
                    b.SkillStrike = b.StormVolley < 3 ? b.StormVolleyAt[b.StormVolley] : 1e9f;
                }
                else b.SkillStrike = 1e9f;
            }
            if (b.Attack == "Skill_Slide" && b.AttackTime <= SlideTo)
            {   // F, her slide: what her way goes past is cut, by her way (above): her line since a frame ago against each
                // creature, as her twisted dash's is. Its light is her gear's (her flair's: its wake along her way, each
                // cut its bite); with no flair, her old light and its blood.
                float ax = b.SlideWas.x, az = b.SlideWas.z, gx = p.Position.x - ax, gz = p.Position.z - az, went = std::hypot(gx, gz);
                if (went >= SlidePut) { ax = p.Position.x; az = p.Position.z; gx = gz = went = 0; }   // (she was put here)
                const float hx = went > 1e-4f ? gx / went : std::sin(p.Yaw), hz = went > 1e-4f ? gz / went : std::cos(p.Yaw);
                if (flair && went > 1e-3f)
                {   // (her wake: this frame's piece of her way, at her middle, laid low as she is)
                    constexpr float Low = .6f;
                    mFlair->Dash({ax, b.SlideWas.y + Low, az}, {p.Position.x, p.Position.y + Low, p.Position.z}, !b.SlideWake);
                    b.SlideWake = true;
                }
                b.SlideWas = p.Position;
                int struck = 0;
                for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                {
                    if (!mWraiths[i].Alive) continue;
                    const Foe f = foe(i);
                    const float over = p.Position.y - f.Feet.y;
                    if (over > f.Height + .5f || over < -2.5f) continue;   // over its head, or it is high over hers
                    // (how much of this frame's way lay within reach of it: of where it stood when her way first met it)
                    auto cut = std::find_if(b.SlideCuts.begin(), b.SlideCuts.end(), [i](const Body::SlideCut& c) { return c.Who == i; });
                    const bool met = cut != b.SlideCuts.end();
                    const float reach = f.Radius + SlideWide, cx = (met ? cut->X : f.Feet.x) - ax, cz = (met ? cut->Z : f.Feet.z) - az;
                    const float away2 = cx * cx + cz * cz, ahead = cx * hx + cz * hz, beside2 = away2 - ahead * ahead;
                    if (beside2 >= reach * reach) continue;
                    const float half = std::sqrt(reach * reach - beside2), in = (std::min)(went, ahead + half) - (std::max)(0.f, ahead - half);
                    if (in < 0) continue;
                    if (!met)
                    {   // (first met before her or beside her, or right at her: not what stands behind her as she sets off)
                        if (ahead < 0 && away2 > (f.Radius + .3f) * (f.Radius + .3f)) continue;
                        b.SlideCuts.push_back(Body::SlideCut{i, 0.f, 0, f.Feet.x, f.Feet.z}); cut = b.SlideCuts.end() - 1;
                    }
                    cut->In += in;
                    // (and where on this frame's way she was nearest to it as it stands now: the blow comes from there)
                    const float nowX = f.Feet.x - ax, nowZ = f.Feet.z - az, nowAhead = nowX * hx + nowZ * hz;
                    const float nowBeside2 = nowX * nowX + nowZ * nowZ - nowAhead * nowAhead;
                    float nearest = std::clamp(nowAhead, 0.f, went);
                    if (nowBeside2 < .04f && std::abs(nowAhead - nearest) < .2f) nearest = nowAhead - .3f;   // (right through its middle: from before it)
                    const XMFLOAT3 from{ax + hx * nearest, p.Position.y, az + hz * nearest};
                    while (cut->Times < SlideCutsMost && cut->In >= static_cast<float>(cut->Times) * SlideCutEvery && mWraiths[i].Alive)
                    {
                        ++cut->Times;
                        if (flair)
                        {   // (her flair: the cut's bite as her gear has it, her blades in turn; a cut in passing)
                            NyxaraFlair::Blow blow;
                            blow.Her = from; blow.Yaw = std::atan2(hx, hz); blow.Hand = cut->Times % 2; blow.Weight = 0; blow.Reach = reach;
                            blow.Passing = true; blow.Struck.push_back({f.Feet, f.Height, f.Radius});
                            mFlair->Strike(blow);
                        }
                        else b.Wound(f.Feet, f.Height, f.Radius, from, sideOf(), true, SlideDamage);
                        DamageWraith(i, SlideDamage, from, CutPush, 0);
                        ++struck;
                    }
                }
                if (struck) b.Log(world, ("slide cut " + std::to_string(struck)).c_str());
                // (and the game is told of a blow all round her every SlideTell m of her way: the clay jars beside it)
                for (; mBlowListener && b.SlideTold <= b.SlideGone + went; b.SlideTold += SlideTell)
                {
                    const float on = std::clamp(b.SlideTold - b.SlideGone, 0.f, went);
                    mBlowListener({ax + hx * on, p.Position.y, az + hz * on}, p.Yaw, SlideWide + .2f, 360.f);
                }
                b.SlideGone += went;
            }
            if (b.AttackTime >= b.Duration(b.Attack)) b.Attack.clear();
        }
        else if (!b.Attack.empty())
        {
            const float was = b.AttackTime;
            b.AttackTime += dt * AttackRate * b.Pace;
            const float hit = Body::Hit(b.Attack);
            if (was < hit && b.AttackTime >= hit)
            {   // the blow lands: every creature in reach before her is struck
                const bool leap = b.Attack == "Attack_Jump";
                const bool last = b.Attack == LeftChain[std::size(LeftChain) - 1] || b.Attack == RightChain[std::size(RightChain) - 1];
                strike(leap ? JumpReach : SwordReach, SwordArc, leap ? JumpDamage : last ? LastDamage : CutDamage, CutPush);
                if (flair)
                {   // (her flair: her own cut's light; each chain's last cut, and her leaping one, its heaviest)
                    NyxaraFlair::Blow blow;
                    blow.Her = p.Position; blow.Yaw = p.Yaw; blow.Reach = leap ? JumpReach : SwordReach;
                    blow.Hand = leap ? 2 : b.Attack.find("_L") != std::string::npos ? 0 : 1;
                    blow.Weight = leap || last ? 2 : b.Step > 0 ? 1 : 0;
                    blow.Struck = flairStruck;
                    mFlair->Strike(blow);
                }
                CaucasianEagle::Sound::Play("nyxara/swing", .5f, .15f);
            }
            if (b.Queued && b.AttackTime >= hit + ChainAfter && styled) b.Attack.clear();   // (with a combo, a click goes on into its move, not her old cuts)
            else if (b.Queued && b.AttackTime >= hit + ChainAfter)
            {
                const char* next = b.NextCut(b.Queued);
                b.Step = b.Attack == "Attack_Jump" ? 0 : b.Step + 1;
                b.StartAttack(world, next);
            }
            else if (b.AttackTime >= (std::min)(b.Duration(b.Attack), hit + AttackTail)) b.Attack.clear();
        }

        // ---- the creatures she holds: pushed over by V, thrown by F
        // her picture's lift: up in the feather storm (and as the cast has it, below)
        if (b.WaveTime >= 0)
        {   // GROUND WAVE: its sparks run out along its line, one every half metre
            b.WaveTime += dt;
            const float reached = b.WaveReach * (std::min)(1.f, b.WaveTime / StyleWaveSeconds);
            for (; b.WaveSparks * .5f <= reached; ++b.WaveSparks)
            {
                const float d = b.WaveSparks * .5f;
                const XMFLOAT3 at = Add(b.WaveFrom, {b.WaveWay.x * d, .12f, b.WaveWay.z * d});
                if (flair) mFlair->Wave(at, b.WaveWay);
                else if (mHiggsfieldTrial)
                {   // (the trial: a spike of light up out of the ground, and hot sparks)
                    Ray(at, {(Random() - .5f) * .3f, 1, (Random() - .5f) * .3f}, .8f + .7f * Random(), .07f, ThemeMain(), .28f, true);
                    Embers(at, {0, 1, 0}, 4, ThemeCore(), 4.f);
                    if (mTrialTheme == 1) IceSpikes(at, .4f, 3);
                    if (mTrialTheme == 2) Flames(at, 3, .2f, .9f);
                }
                else Burst(at, MoonWhite, 4, 2.5f, .18f);
            }
            if (b.WaveTime >= StyleWaveSeconds) b.WaveTime = -1;
        }
        float lift = b.Finish ? b.FinishLiftNow : b.Attack == "Skill_Storm" ? Body::StormLift(b.AttackTime)
                   : b.InMove() ? b.MoveOf->Hop * HopShare(b.AttackTime, b.MoveOf->HopFrom, b.MoveOf->HopTo) : 0.f;
        if (b.Pushed >= 0)
        {
            b.PushTime += dt;
            auto& w = mWraiths[b.Pushed];
            if (!w.Alive && b.PushGone < 0) b.PushGone = b.PushTime;
            const float over = CastFallSeconds + CastDownSeconds + CastRiseSeconds;
            const bool done = b.PushTime >= over || (b.PushGone >= 0 && b.PushTime > b.PushGone + GoneSeconds);
            if (done) b.Pushed = -1;
            else
            {   // over backwards about its feet, it lies, and it gets up
                const float down = ease(b.PushTime, 0, CastFallSeconds) * (1 - ease(b.PushTime, CastFallSeconds + CastDownSeconds, over));
                const float a = CastFallen * down;
                w.Pinned = true;
                w.Hold = (std::min)(1.f, down * 4);
                w.HeldFeet = b.PushFeet;
                w.HeldNeck = {b.PushFeet.x + b.PushWay.x * b.PushStick * std::sin(a), b.PushFeet.y + b.PushStick * std::cos(a), b.PushFeet.z + b.PushWay.z * b.PushStick * std::sin(a)};
            }
        }

        // ---- her picture: leaning in flight, lifted to what she does, the camera with it
        b.Speed = std::hypot(p.ActualVelocity.x, p.ActualVelocity.z);
        const float lean = b.Air ? FlyLeanFast : b.Cast >= 4 ? 0.f
                         : flying && !landing && !b.Dead ? FlyLeanHover + std::clamp(b.Speed / FlyLeanSpeed, 0.f, 1.f) * (FlyLeanFast - FlyLeanHover) : 0.f;
        b.FlyLean += (lean - b.FlyLean) * (std::min)(1.f, dt * 4);
        {   // On foot she leans into a turn and into gathering speed, and back as she loses it (2026-11-19): she turned
            // and started as a post does. Her whole picture is tipped about her feet (PlantFeet keeps them on the ground).
            const float turned = dt > 1e-4f && b.LeanKnown ? std::remainder(p.Yaw - b.LeanYaw, XM_2PI) / dt : 0.f;
            const float gained = dt > 1e-4f && b.LeanKnown ? (b.Speed - b.LeanSpeed) / dt : 0.f;
            b.LeanYaw = p.Yaw; b.LeanSpeed = b.Speed; b.LeanKnown = true;
            const float soon = (std::min)(1.f, dt * 9);
            b.TurnRate += (std::clamp(turned, -4.f, 4.f) - b.TurnRate) * soon;
            b.Gain += (std::clamp(gained, -30.f, 30.f) - b.Gain) * soon;
            const bool afoot = !flying && p.IsGrounded && !b.Dead && !b.Carried() && !b.Cast && !b.Finish && b.Rise < 0 && b.Touch < 0;
            const float roll = afoot ? std::clamp(-b.TurnRate * b.Speed * .035f, -.22f, .22f) : 0.f;
            const float pitch = afoot ? std::clamp(b.Gain * .010f, -.05f, .09f) + b.Squat * .45f + b.StyleLean : 0.f;   // (and she bows a little into a landing; and leans as her way of going has her)
            {   // Struck, she is knocked back a little and comes upright again (2026-11-19: a blow took her health and
                // did nothing else to her), and her wings start up (below, with her guard). Away from the blow, as hard
                // as it was (2026-11-20: every blow tipped her back by the same 7 degrees, a Chinka's claw as a giant's
                // leap-slam, from behind as from before her): 4 to 13 degrees, forward, back or to a side; a heavy blow,
                // or a burst under her feet, makes her knees give.
                constexpr float Spring = 13.f, Knock = 4.5f;
                if (b.Struck && !b.Dead)
                {
                    b.FlinchGo += Knock * b.StruckWeight * b.StruckZ;
                    b.FlinchRollGo -= Knock * b.StruckWeight * b.StruckX;
                    if (afoot) b.SquatGo += 2.f * (b.StruckBelow ? b.StruckWeight : (std::max)(0.f, b.StruckWeight - 1));
                }
                for (float left = dt; left > 1e-6f;)
                {
                    const float h = (std::min)(left, 1 / 120.f); left -= h;
                    b.FlinchGo += (-Spring * Spring * b.Flinch - 2 * Spring * b.FlinchGo) * h;
                    b.Flinch += b.FlinchGo * h;
                    b.FlinchRollGo += (-Spring * Spring * b.FlinchRoll - 2 * Spring * b.FlinchRollGo) * h;
                    b.FlinchRoll += b.FlinchRollGo * h;
                }
                if (b.Dead || b.Cast || b.Finish) b.Flinch = b.FlinchGo = b.FlinchRoll = b.FlinchRollGo = 0;
            }
            b.LeanRoll += (roll - b.LeanRoll) * (std::min)(1.f, dt * 5);
            b.LeanPitch += (pitch - b.LeanPitch) * (std::min)(1.f, dt * 7);
            // in flight she banks into a turn, as a bird does (2026-11-19: she turned in the air upright, as on a spit)
            const bool winging = flying && !p.IsGrounded && !b.Dead && !b.Cast && !b.Air && b.Rise < 0 && b.Touch < 0;
            b.Bank += ((winging ? std::clamp(-b.TurnRate * .24f, -.42f, .42f) : 0.f) - b.Bank) * (std::min)(1.f, dt * (b.Air ? 12.f : 3.5f));
        }
        world.AnimatedPlayerVisual.Rotation = {b.FlyLean + b.LeanPitch + b.Flinch, 0, b.LeanRoll + b.Bank + b.FlinchRoll};
        b.Hover += ((flying && p.IsGrounded && !landing && b.Rise < 0 ? 1.f : 0.f) - b.Hover) * (std::min)(1.f, dt * 3);
        const float hover = b.Hover * FlyHover;
        b.LiftNow = lift + hover;
        world.AnimatedPlayerVisual.Translation = {0, b.LiftNow, 0};
        // V's way and backflip, the twisted dash: her whole picture turned (TurnBody)
        b.BodyMove = false; b.BodyTwist = 0;
        if (!b.Dead && (b.Cast >= 4 || b.Air == 1)) b.TurnBody(world, dt, b.LiftNow);
        if (b.BodyMove)
        {
            world.AnimatedPlayerVisual.Rotation = {b.BodyPitch, 0, b.BodyRoll};
            world.AnimatedPlayerVisual.Translation = b.BodyAt;
        }
        if (b.RingDue) b.TurnedAirRing(world);
        // The camera looks at her real place, and these lifts move only her picture: so it is told of them, smoothly
        // (in the feather storm it goes up with her by less than half: she leaps up in the picture, the ground does not drop away)
        const float shown = b.LiftNow - (b.Attack == "Skill_Storm" ? .6f * Body::StormLift(b.AttackTime) : 0.f);
        b.CameraLift += (shown - b.CameraLift) * (std::min)(1.f, dt * CameraFollowLift);
        s.CameraPivotHeight = b.PivotBase + b.CameraLift;

        // ---- travel (owner, 2026-11-18): A and D turn her round, on foot and in flight, and never the view (the mouse alone
        // turns it); W goes the way she faces and S steps back (the engine's MoveAlongFacing). A scripted run still steers
        // by the view, and she turns to where she goes.
        if (b.MoveFree())
        {   // a style's move once it is free: given up for her own way (W, S, A, D; a scripted run's move or turn)
            const bool scripted = s.Unattended && s.ScriptedAlongFacing && (s.ScriptedMove != 0 || s.ScriptedSteer != 0);
            if (scripted || (live && (IsDoorwaysKeyDown('W') || IsDoorwaysKeyDown('S') || IsDoorwaysKeyDown('A') || IsDoorwaysKeyDown('D'))))
                b.EndMove(world, "move given up for her way");
        }
        const bool carried = b.Carried();
        float impulse = !b.Dead && dt > 0 ? b.ForwardSpeed(world) : 0.f;
        s.ImpulseX = std::sin(p.Yaw) * impulse;
        s.ImpulseZ = std::cos(p.Yaw) * impulse;
        s.LongMovePiece = b.Attack == "Skill_Slide" || b.Soul == 1 ? SlideStep : !flying && mFootPace > 1 ? FootStep : 0.f;   // (F, her slide, and her going at a faster pace: up a climb as on the level)
        if (b.Finish) { s.ImpulseX = b.FinishGo.x; s.ImpulseZ = b.FinishGo.y; }   // (a finisher's plan: UpdateFinish)
        else if (b.InMove()) { s.ImpulseX = b.MoveVel.x; s.ImpulseZ = b.MoveVel.y; }   // (a style's move: its way, above)
        b.WasAt = {p.Position.x, p.Position.z};
        // (a scripted run may play as a player does: the session's ScriptedAlongFacing, with ScriptedMove and ScriptedSteer)
        const bool played = s.Unattended && s.ScriptedAlongFacing;
        const bool keyW = (live && IsDoorwaysKeyDown('W')) || (played && s.ScriptedMove == 'W'), keyS = (live && IsDoorwaysKeyDown('S')) || (played && s.ScriptedMove == 'S');
        int steer = live && !b.Dead && dt > 0 ? (IsDoorwaysKeyDown('D') ? 1 : 0) - (IsDoorwaysKeyDown('A') ? 1 : 0) : 0;
        if (played && !b.Dead && dt > 0) steer = s.ScriptedSteer;
        if (GameSession().Unattended && dt > 0)
        {   // testing: CAUCASIAN_EAGLE_NYXARA_STEER="<from>:<to>:<-1 A | 1 D>" holds A or D over those seconds of her clock
            // (she stands meanwhile); at the end how far she and the view turned goes to nyxara_steer.txt in %TEMP%
            static const std::array<float, 3> hold = []
            {
                std::array<float, 3> h{-1, -1, 0}; char t[64] = {};
                if (GetEnvironmentVariableA("CAUCASIAN_EAGLE_NYXARA_STEER", t, sizeof(t))) sscanf_s(t, "%f:%f:%f", &h[0], &h[1], &h[2]);
                return h;
            }();
            static float herFrom = 0, viewFrom = 0; static bool told = false;
            if (b.Clock >= hold[0] && b.Clock < hold[1])
            {
                if (b.Clock - dt < hold[0]) { herFrom = p.Yaw; viewFrom = world.MainCamera.Yaw; }
                steer = static_cast<int>(hold[2]); s.ScriptedMove = 0;
            }
            else if (hold[1] > 0 && b.Clock >= hold[1] && !told)
            {
                told = true; char path[MAX_PATH] = {}; GetTempPathA(MAX_PATH, path);
                std::ofstream(std::string(path) + "nyxara_steer.txt") << "her " << XMConvertToDegrees(std::remainder(p.Yaw - herFrom, XM_2PI))
                    << " view " << XMConvertToDegrees(std::remainder(world.MainCamera.Yaw - viewFrom, XM_2PI)) << " over " << hold[1] - hold[0] << " s" << std::endl;
            }
        }
        const bool rooted = carried || b.Rise >= 0 || b.Touch >= 0 || b.Cast != 0 || b.Air != 0 || b.Dead || b.Finish != 0;
        if (steer && !carried && !b.Cast && !b.Dead && !b.Finish)
            p.Yaw = std::remainder(p.Yaw + static_cast<float>(steer) * XMConvertToRadians(SteerRate) * dt, XM_2PI);
        b.Back = keyS && !keyW && !b.Dead && !flying;
        s.HoldFacing = true;
        s.MoveAlongFacing = true;
        if (s.Unattended && s.ScriptedMove && !played)
        {
            const float vx = p.ActualVelocity.x, vz = p.ActualVelocity.z;
            if (!b.Dead && !carried && !b.Cast && !b.Air && !b.Finish && dt > 0 && std::hypot(vx, vz) > .15f)
            {
                const float want = b.Back ? std::atan2(-vx, -vz) : std::atan2(vx, vz);
                float turn = std::remainder(want - p.Yaw, XM_2PI);
                const float most = XMConvertToRadians(b.Back ? 240.f : 540.f) * dt;
                turn = std::clamp(turn * (std::min)(1.f, dt * 12), -most, most);
                p.Yaw = std::remainder(p.Yaw + turn, XM_2PI);
            }
        }
        mSpeedScale = rooted ? 0.f : b.Back ? BackSpeed / CaucasianEagle::FootSpeed : 1.f;
        {   // She gathers speed and loses it (2026-11-19). The engine's travel is all or nothing: at full speed in the
            // frame a key goes down, at rest in the frame it is let go, which no pose can follow (her feet slid through
            // every start and stop). Now she starts at StartShare of her speed and has all of it GatherSeconds later; let
            // go on the ground, she is carried on by the speed she had and loses it evenly (a jog in 0.2 s and 0.3 m, a
            // run in 0.25 s and 0.7 m), as a move's impulse carries her: the ground and walls still count.
            constexpr float GatherSeconds = .3f, StartShare = .3f;
            const bool wants = p.IsMoving && !rooted && !flying;
            // Out of a move that carried her (the slide, a skill, a leap's cut) she goes on at the speed it left her with
            // (2026-11-21). She was held at no speed of her own all through it, and the frame it ended she stood still: one
            // frame of standing at ease, her stride begun again at StartShare and full only 0.3 s later, a stop she never
            // made. Key held, her own speed is taken up from that; key let go, she is carried on by it and loses it.
            b.Freed = b.WasCarried && !carried && !flying && !b.Dead;
            b.WasCarried = carried;
            if (carried && !flying) b.CarriedGo = {s.ImpulseX, s.ImpulseZ};
            const float carriedHad = b.Freed ? std::hypot(b.CarriedGo.x, b.CarriedGo.y) : 0.f;
            if (flying || rooted) { b.Throttle = flying ? 1.f : 0.f; b.Glide = {0, 0}; }
            else if (wants)
            {
                const float gliding = (std::max)(std::hypot(b.Glide.x, b.Glide.y), carriedHad) / (std::max)(.5f, b.FullSpeed);
                b.Throttle = std::clamp((std::max)({b.Throttle, StartShare, gliding}) + dt * mFootPace / GatherSeconds, 0.f, 1.f);   // (at a faster pace, sooner: SetFootPace)
                // (and this frame, before her own speed reaches the engine, what the move gave her carries her)
                if (b.Freed) { s.ImpulseX += b.CarriedGo.x; s.ImpulseZ += b.CarriedGo.y; }
                b.Glide = {0, 0}; b.Went = {p.ActualVelocity.x, p.ActualVelocity.z};
                if (b.Throttle >= 1 && b.Speed > .5f) b.FullSpeed = b.Speed;
            }
            else
            {
                if (b.Freed && carriedHad > .3f) { b.Throttle = 1; b.Went = b.CarriedGo; }   // (let go out of a move: carried on as from her own going)
                if (b.Throttle > 0)
                {
                    b.Glide = b.Went; b.Went = {0, 0};   // (the engine has stopped her already this frame)
                    float had = std::hypot(b.Glide.x, b.Glide.y);
                    const float fastest = CaucasianEagle::FootRunSpeed * mFootPace;
                    if (had > fastest) { b.Glide.x *= fastest / had; b.Glide.y *= fastest / had; had = fastest; }
                    // (lost in .12 s and .025 s a m/s she had; at a faster pace (SetFootPace) as at the pace as made, that
                    // much sooner: as far as then)
                    b.GlideLose = had * mFootPace / (.12f + .025f * had / mFootPace);
                }
                b.Throttle = 0;
                // (in the air nothing stops her: the key let go in a jump, she flies on as she went, and loses it on the ground)
                const float had = std::hypot(b.Glide.x, b.Glide.y), left = (std::max)(0.f, had - (p.IsGrounded ? b.GlideLose : 1.f) * dt);
                if (had > 1e-3f && left > 0) { b.Glide.x *= left / had; b.Glide.y *= left / had; }
                else b.Glide = {0, 0};
            }
            s.ImpulseX += b.Glide.x; s.ImpulseZ += b.Glide.y;
            if (!flying && !rooted) mSpeedScale *= b.Throttle > 0 ? b.Throttle : StartShare;
        }
        {   // up a stair she climbs at a stair's pace (the jog would play the stair clip four times over): eased in and out
            const float stair = b.Gait == "Stairs_Up" && !flying && !b.Dead ? 1.f : 0.f;
            b.OnStair += (stair - b.OnStair) * (std::min)(1.f, dt * 4);
            const float most = (live && IsDoorwaysKeyDown(VK_SHIFT)) ? StairRunPace : StairPace;
            if (mSpeedScale > 0 && p.MoveSpeed > most) mSpeedScale *= 1 + ((most / p.MoveSpeed) - 1) * b.OnStair;
        }
        p.MoveSpeed *= mSpeedScale * (flying ? 1.f : mFootPace);   // (her pace on foot: SetFootPace)
        s.SlowWalkSpeed = rooted ? 0.f : WalkPace * (flying ? 1.f : (b.Throttle > 0 ? b.Throttle : .3f) * mFootPace);
        // (not her take-off, Fly_Rise: she flies from its first moment, forward and up as the keys say; owner 2026-11-26:
        // "can she start flying up right away after 1sec")
        const bool heldInAir = rooted && !(b.Rise >= 0 && !carried && b.Touch < 0 && b.Cast == 0 && b.Air == 0 && !b.Dead && b.Finish == 0);
        if (heldInAir && flying)
        {   // in the air a move carries her itself
            if (!b.FlightHeld) { b.FlightSaved = p.FlightSpeed; b.FlightHeld = true; }
            p.FlightSpeed = 0;
        }
        else if (b.FlightHeld)
        {
            if (p.FlightSpeed == 0) p.FlightSpeed = b.FlightSaved;
            b.FlightHeld = false;
        }

        // ---- the clip and her pose
        // (her own going, or carried on after the key: still her stride. Not what a move's last push leaves her for a
        // frame: after every skill she strode for one frame, and her arms and swords set out for her back and returned)
        const bool moving = !(mShowcase && dt <= 0) && ((p.IsMoving && (b.Speed > .08f || b.Freed)) || std::hypot(b.Glide.x, b.Glide.y) > .08f);   // (shown in her window she stands)
        {   // On guard or at ease (2026-11-19). Idle is a fighter's guard, both swords up: she stood so everywhere, and
            // drew both swords every time she stopped walking. Now she stands on guard only while something threatens
            // her: she is fighting, or a creature is within GuardNear of her, and for GuardSeconds after. Otherwise she
            // stands at ease (Stand), her swords on her back, and walks off without having to put them away.
            constexpr float GuardSeconds = 5.f, GuardNear = 14.f;
            bool threat = !b.Attack.empty() || b.Cast != 0 || b.Struck || b.Finish != 0 || mGuarding || mShowcase;   // (shown in her window: on guard, both swords in her hands)
            if (b.Struck) for (auto& w : b.WingSpring) w.FlapGo += 3.f;
            b.Struck = false;
            for (const auto& w : mWraiths)
                if (!threat && w.Alive && std::hypot(w.Pos.x - p.Position.x, w.Pos.z - p.Position.z) < GuardNear && std::abs(w.Pos.y - p.Position.y) < GuardNear) threat = true;
            // And while that lasts her swords stay in her hands as she goes, jumps and falls (Armed; owner, 2026-11-19:
            // "hands are not following the skills and combos properly"). Until this they went to her back with her first
            // step, and were drawn again for every cut and every skill made on the move: in a fight on the move her
            // hands were at her shoulders half the time, and every cut began there instead of at its own wind-up.
            b.Guard = threat ? GuardSeconds : (std::max)(0.f, b.Guard - poseDt);
            b.Armed = b.Guard > 0;
        }
        std::string clip = b.Guard > 0 || b.Duration("Stand") <= 0 ? "Idle" : "Stand";
        float time = b.Clock;
        if (mGuarding && mGuardStyle >= 0 && b.Duration(GuardClip(mGuardStyle)) > 0 && !moving && !flying && b.Attack.empty() && !b.InMove() && !b.Cast && !b.Jumping && !b.Finish)
        {   // on guard (E), standing and doing nothing else: her guard's pose, swaying on it from when she raised it
            const GuardSpec& gs = Guards()[static_cast<size_t>(mGuardStyle)];
            clip = GuardClip(mGuardStyle); time = std::fmod(mGuardHeld, gs.Breath); b.GuardRaise = gs.Raise;
        }
        const bool striding = moving && !b.Dead && !flying;
        {   // how steeply she climbs (rise over ground covered, from her own height), for the stair clip
            const float rise = poseDt > 1e-4f && b.LastY > -1e8f ? (p.Position.y - b.LastY) / poseDt : 0.f;
            const float grade = b.Speed > .2f && !flying ? rise / b.Speed : 0.f;
            b.Climb += (grade - b.Climb) * (std::min)(1.f, poseDt * 6);
            b.ClimbRate += ((std::max)(0.f, rise) - b.ClimbRate) * (std::min)(1.f, poseDt * 6);
            b.LastY = p.Position.y;
        }
        if (striding)
        {
            // Going up a stair (owner, 2026-11-17): Mixamo's stair climb fitted to the map's stairs, played by her height on
            // the stair (the steps she has climbed), so that each foot lands on a tread. Only on a stair's treads (a hill is
            // walked up), in and out with a margin, so that she does not flicker between clips.
            const bool onStair = mStairTread && mFootGround && std::isfinite(mFootGround(p.Position.x, p.Position.z, p.Position.y));
            const bool climbing = StairClip && !b.Back && onStair && b.Duration("Stairs_Up") > 0 && b.Climb > (b.Gait == "Stairs_Up" ? ClimbLeave : ClimbEnter);
            b.Gait = climbing ? "Stairs_Up" : "Move";   // (backwards she walks, the stride played the other way round)
            if (climbing)
            {
                const float u = (p.Position.y - .15f) / NyxaraClips::StairsRise - NyxaraClips::StairsShift;
                b.Phase = u / 2 - std::floor(u / 2);
            }
            else if (mGaitStyle > 0)   // (in a way of going, GaitStyle: its own stride, at her own speed)
                b.StyleStride(GaitStyles[mGaitStyle], (std::max)(b.Speed, std::hypot(b.Glide.x, b.Glide.y)), mFootPace, b.Back, poseDt);
            else   // (the frame the key is let go the engine has her at rest; she is carried on from the next. At a faster
                   // pace (SetFootPace) the stride of the pace as made, stepped that many times as often)
                b.Stride((std::max)(b.Speed, std::hypot(b.Glide.x, b.Glide.y)) / mFootPace, b.Back, poseDt * mFootPace);
        }
        b.StyleFrame(mGaitStyle > 0 ? &GaitStyles[mGaitStyle] : nullptr, striding, (std::max)(b.Speed, std::hypot(b.Glide.x, b.Glide.y)), poseDt);
        {   // Turning on the spot she steps (2026-11-19: she was turned as a statue on a plate). Her legs are the walk's,
            // stepping in place (her stride drawn in to nothing), her body goes on standing.
            const bool turning = !moving && !b.Dead && !flying && p.IsGrounded && b.Attack.empty() && !b.Cast && !b.Jumping && !carried &&
                                 b.Rise < 0 && b.Touch < 0 && std::abs(b.TurnRate) > .5f;
            if (turning) { b.Gait = "Move"; b.Stride(0, false, poseDt); }
            else if (!striding) b.StrideScale += (1 - b.StrideScale) * (std::min)(1.f, poseDt * 10);   // (standing: her feet where her clip has them)
            b.TurnSteps += ((turning ? 1.f : 0.f) - b.TurnSteps) * (std::min)(1.f, poseDt * (turning ? 10.f : 6.f));
        }
        const bool cutting = !b.Attack.empty() && !carried && !b.Dead;
        if (!cutting) b.Legs = 0;
        else if (b.LastClip.rfind("Attack", 0) != 0) b.Legs = striding ? 1.f : 0.f;
        else b.Legs = std::clamp(b.Legs + (striding ? 1.f : -1.f) * poseDt / .15f, 0.f, 1.f);
        b.FlyBody = (cutting || (b.Cast >= 1 && b.Cast <= 3)) && flying;   // a cut in the air: her legs, hips and wings go on flying
        if (b.FlyBody) b.Legs = b.Sample("Fly", b.FlyTime, b.LegsPose, nullptr) ? 1.f : 0.f;
        else if (b.Legs > 0 && !b.Sample(b.Gait, b.GaitTime(), b.LegsPose, nullptr)) b.Legs = 0;
        const bool previewing = !mPreview.empty() && b.Duration(mPreview) > 0;   // (the Character Lab's clip viewer)
        if (previewing)
        {
            clip = mPreview; mPreviewClock += poseDt;
            time = mPreviewTime >= 0 ? (std::min)(mPreviewTime, b.Duration(clip)) : std::fmod(mPreviewClock, b.Duration(clip));
        }
        else if (b.Dead) { clip = "Death"; time = b.DeathTime; }
        else if (b.Soul == 3 && b.Duration(Body::SoulBackClip) > 0)
        {   // her way back to her soul, played over however long it takes
            clip = Body::SoulBackClip; time = std::clamp(b.SoulTime / b.SoulBack, 0.f, 1.f) * b.Duration(clip);
        }
        else if (b.Finish) { clip = Finishes[b.Finish].Clip; time = b.FinishTime; }
        else if (b.Rise >= 0) { clip = "Fly_Rise"; time = b.Rise; }
        else if (b.Touch >= 0) { clip = "Fly_Land"; time = b.Touch; }
        else if (mFalling && b.Attack.empty() && !b.Cast && b.Duration("Fall") > 0) { clip = "Fall"; time = std::fmod(b.Clock, b.Duration("Fall")); }
        else if (b.Cast == 4) { clip = "Cast_Drill"; time = 0; }   // (a pose held; her turns are BodyTwist)
        else if (b.Cast == 5) { clip = "Cast_Flip"; time = b.CastTime; }
        else if (!b.Attack.empty()) { clip = b.Attack; time = b.AttackTime; }
        else if (b.Air) { clip = "Dash_Twist"; time = 0; }
        else if (flying) { clip = "Fly"; time = b.FlyTime; }
        else if (b.Jumping && b.JumpReal && b.Dropped > DropSeconds && b.Duration("Fall") > 0) { clip = "Fall"; time = std::fmod(b.Clock, b.Duration("Fall")); }
        else if (b.Jumping) { clip = "Jump"; time = b.JumpTime; }
        else if (moving) { clip = b.Gait; time = b.GaitTime(); }
        b.Current = clip; mShownTime = time;
        if (!cutting && !b.FlyBody && (clip == "Stand" || clip == "Idle") && b.TurnSteps > .02f && b.Sample("Move", b.GaitTime(), b.LegsPose, nullptr)) b.Legs = b.TurnSteps;
        {
            DoorwaysSkeletonPose pose;
            std::string error;
            // her feet on the drawn ground: on foot and standing on it, not while a move lifts or carries her
            const bool planted = !flying && p.IsGrounded && !b.Dead && !b.Jumping && b.Rise < 0 && b.Touch < 0 && !b.Cast && !b.Air && !b.Carried() &&
                                 !b.Finish && std::abs(b.LiftNow) < .05f && !previewing;
            const bool composed = b.Compose(world, clip, time, poseDt, pose, &error);
            b.StyleHold = 0;
            if (composed && !flying && !b.Dead && !previewing) b.StylePose(pose, poseDt, clip == "Move", world.AnimatedPlayerVisual.Scale.y);   // (her way of going)
            const float planting = composed ? b.PlantFeet(world, mFootGround, mStairTread, pose, poseDt, planted) : 0.f;
            if (composed && !b.BodyMove) world.AnimatedPlayerVisual.Translation.y = b.LiftNow + planting;
            if (!b.BodyMove)
            {   // (her bank turns about her chest, not about her feet: her picture is put back by what the roll moves her chest)
                constexpr float Chest = 1.15f;
                world.AnimatedPlayerVisual.Translation.x = Chest * std::sin(b.Bank);
                world.AnimatedPlayerVisual.Translation.y += Chest * (1 - std::cos(b.Bank));
            }
            float toward = std::numeric_limits<float>::quiet_NaN();
            {   // the nearest creature within LookFar of her and not behind her (a third of a turn either way)
                constexpr float LookFar = 22.f, LookWide = 2.1f;
                float nearest = LookFar;
                for (const auto& w : mWraiths)
                {
                    const float dx = w.Pos.x - p.Position.x, dz = w.Pos.z - p.Position.z, d = std::hypot(dx, dz);
                    if (!w.Alive || d < .6f || d >= nearest || std::abs(w.Pos.y - p.Position.y) > LookFar) continue;
                    const float a = std::remainder(std::atan2(dx, dz) - p.Yaw, XM_2PI);
                    if (std::abs(a) < LookWide) { nearest = d; toward = a; }
                }
            }
            if (composed) b.LookAbout(pose, poseDt, b.TurnRate, toward, b.Dead || b.Cast || b.Air || b.Finish || !b.Attack.empty() || b.Rise >= 0 || b.Touch >= 0 || previewing ? 0.f : 1.f);
            {   // Standing at ease for a while she opens her wings, shakes them out and lowers them again (2026-11-19): every
                // quarter of a minute or so, not by a clock. A blow up on both wings as they open, one down after it.
                constexpr float StretchSeconds = 1.7f, Shake = .65f;
                b.AtEase = clip == "Stand" && std::abs(b.TurnRate) < .2f ? b.AtEase + poseDt : 0.f;
                if (b.AtEase <= 0) { b.NextStretch = b.Rand(9, 16); b.Stretch = -1; }
                else if (b.Stretch < 0 && b.AtEase > b.NextStretch)
                {
                    b.Stretch = 0; b.NextStretch = b.AtEase + b.Rand(14, 27);
                    for (auto& w : b.WingSpring) w.FlapGo += 2.4f;
                }
                else if (b.Stretch >= 0)
                {
                    if (b.Stretch < Shake && b.Stretch + poseDt >= Shake) for (auto& w : b.WingSpring) w.FlapGo -= 1.7f;
                    if ((b.Stretch += poseDt) > StretchSeconds) b.Stretch = -1;
                }
            }
            {   // her wing shield (E): her wings closing round her, as her shield has them
                const bool shield = mGuarding && mGuardStyle >= 0 && mGuardStyle < static_cast<int>(Guards().size());
                if (shield)
                {
                    const GuardSpec& gs = Guards()[static_cast<size_t>(mGuardStyle)];
                    for (int i = 0; i < 3; ++i) { b.CloseBack[i] = gs.Back[i]; b.CloseFlap[i] = gs.Flap[i]; }
                    b.WingCloseRate = gs.Close;
                }
                // (right round her standing; half closed while she fights or goes, so that her arms and swords are free and
                // she still has her wings; little in flight, where they carry her)
                const bool engaged = moving || !b.Attack.empty() || b.InMove() || b.Cast || b.Air || b.Jumping || b.Finish || b.Rise >= 0 || b.Touch >= 0;
                b.WingCloseTo = !shield ? 0.f : flying ? .25f : engaged ? .5f : 1.f;
            }
            if (composed)
                b.SwingWings(pose, poseDt, !p.IsGrounded && !flying ? p.VerticalVelocity : 0.f, flying ? 0.f : b.Back ? -b.Gain : b.Gain, b.TurnRate,
                             b.Dead || b.Cast || b.Air ? 0.f : b.Finish ? .6f : flying || b.Rise >= 0 || b.Touch >= 0 ? .35f : 1.f,
                             clip == "Stand" ? (b.Stretch >= 0 ? 0.f : 1.f) : clip == "Move" ? b.StyleFold(.75f - .6f * b.GaitRun) : 0.f);
            b.WingLineKnown = false;
            if (composed && b.WingClose > .02f && pose.ModelSpaceMatrices.size() == pose.LocalJoints.size() && b.WingJoint[0][2] >= 0 && b.WingJoint[1][2] >= 0)
            {   // where her wings are in the world, for the shield's lights (Draw); and its sparks
                const XMMATRIX place = BuildDoorwaysCharacterWorldMatrix(p.Position, p.Yaw, world.AnimatedPlayerVisual);
                const auto joint = [&](int j) { const auto& m = pose.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 1); };
                for (int side = 0; side < 2; ++side)
                {
                    const XMVECTOR a = joint(b.WingJoint[side][0]), c = joint(b.WingJoint[side][1]), e = joint(b.WingJoint[side][2]);
                    const XMVECTOR line[4] = {a, c, e, XMVectorSetW(e + (e - c) * 1.1f, 1)};   // (the long feathers reach past its last bone)
                    for (int i = 0; i < 4; ++i) XMStoreFloat3(&b.WingLine[side][i], XMVector3TransformCoord(line[i], place));
                }
                b.WingLineKnown = true;
                if (mGuarding && mGuardStyle >= 0 && mGuardStyle < static_cast<int>(Guards().size()))
                {
                    const GuardSpec& gs = Guards()[static_cast<size_t>(mGuardStyle)];
                    mShieldMotes += poseDt * gs.Motes * (std::min)(1.f, b.WingClose);
                    while (mShieldMotes >= 1)
                    {
                        mShieldMotes -= 1;
                        const int side = Random() < .5f ? 0 : 1, i = static_cast<int>(Random() * 2.99f);
                        const XMFLOAT3 at = Lerp(b.WingLine[side][i], b.WingLine[side][i + 1], Random());
                        Burst(at, gs.Color, 1, gs.Kind == 3 ? 1.2f : .6f, gs.Size * .45f);
                    }
                }
            }
            if (composed && b.Attack == "Skill_Storm" && b.AttackTime >= StormThrowFrom && b.AttackTime < StormThrowTo && b.Hips >= 0)
            {   // the storm: feather daggers thrown from her wings as they go round, each wing in turn; each straight out
                // (turned a little on round, the way she turns) and down to the ground, near ones steep and far ones flat
                const XMMATRIX place = BuildDoorwaysCharacterWorldMatrix(p.Position, p.Yaw, world.AnimatedPlayerVisual);
                const auto there = [&](XMVECTOR v) { XMFLOAT3 o; XMStoreFloat3(&o, XMVector3TransformCoord(v, place)); return o; };
                const auto joint = [&](int j) { const auto& m = pose.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 1); };
                const XMFLOAT3 hips = there(joint(b.Hips));
                b.StormFeathers += poseDt * StormRate;
                while (b.StormFeathers >= 1)
                {
                    b.StormFeathers -= 1;
                    b.StormWing = 1 - b.StormWing;
                    const XMVECTOR a = joint(b.WingJoint[b.StormWing][1]), c = joint(b.WingJoint[b.StormWing][2]);
                    const XMFLOAT3 at = there(XMVectorLerp(c, c + (c - a) * .9f, .3f + .6f * Random()));   // (from its long feathers, past its last bone)
                    XMFLOAT3 out{at.x - hips.x, 0, at.z - hips.z};
                    const float r = std::hypot(out.x, out.z);
                    out = r > 1e-3f ? XMFLOAT3{out.x / r, 0, out.z / r} : Forward(p.Yaw);
                    const XMFLOAT3 round{out.z, 0, -out.x};               // the way that point goes as she turns (to her left)
                    const float swirl = StormSwirl * (.7f + .6f * Random());
                    const XMFLOAT3 way = Add(Mul(out, std::cos(swirl)), Mul(round, std::sin(swirl)));
                    const float speed = StormDaggerSpeed * (.9f + .2f * Random());
                    const float reach = StormDaggerNear + (StormReach - StormDaggerNear) * Random();   // where it comes down
                    const float aloft = reach / speed, high = (std::max)(.3f, at.y - p.Position.y);
                    Fx f; f.Kind = FxKind::Feather; f.Target = -3; f.Pos = at; f.Floor = p.Position.y;
                    f.Vel = {way.x * speed, -(high - .5f * StormDrop * aloft * aloft) / aloft, way.z * speed};
                    f.Life = aloft + .5f; f.Size = .22f + .05f * Random(); f.Roll = Random() * XM_2PI;
                    const float shade = Random();   // as her wings are: most dark, some silver, a few white (shining)
                    f.Color = shade < .55f ? FeatherDark : shade < .88f ? FeatherSilver : MoonWhite;
                    f.Glow = shade >= .88f;
                    mFx.push_back(f);
                }
            }
            {   // a style's move: each blade's way a moment back, for its trail of light (Draw)
                for (auto& trail : mTrail)
                {
                    for (auto& q : trail) q.Age += poseDt;
                    while (!trail.empty() && trail.front().Age > StyleTrailSeconds) trail.erase(trail.begin());
                }
                // (her flair: its own trails, and of her own cuts and her finishing kills too, not only a style's moves)
                if (mFlair) mFlair->AgeTrails(poseDt, p.Position);
                const bool finishing = flair && b.Finish != 0;
                const bool sliding = flair && b.Attack == "Skill_Slide" && b.AttackTime <= SlideTo;   // (her slide: her blades' trails too)
                const bool bladesOut = b.InMove() || finishing || sliding || (flair && !b.Attack.empty() && !b.Skill());
                if (composed && bladesOut && (finishing || b.AttackTime >= 0) && b.SwordJoint[0] >= 0 && b.SwordJoint[1] >= 0)
                {
                    DoorwaysSkeletonPose posed = pose;
                    if (EvaluateDoorwaysSkeletonPoseMatrices(b.Skeleton, posed, nullptr))
                    {
                        const XMMATRIX place = BuildDoorwaysCharacterWorldMatrix(p.Position, p.Yaw, world.AnimatedPlayerVisual);
                        for (int k2 = 0; k2 < 2; ++k2)
                        {
                            const XMMATRIX sword = XMLoadFloat4x4(&posed.ModelSpaceMatrices[static_cast<size_t>(b.SwordJoint[k2])]) * place;
                            const XMVECTOR grip = XMVector3TransformCoord(XMLoadFloat3(&b.SwordGrip[k2]), sword);
                            const XMVECTOR tip = XMVector3TransformCoord(XMLoadFloat3(&b.SwordTip[k2]), sword);
                            if (flair)
                            {
                                XMFLOAT3 gripAt, tipAt;
                                XMStoreFloat3(&gripAt, grip); XMStoreFloat3(&tipAt, tip);
                                mFlair->Blade(k2, gripAt, tipAt, poseDt, finishing ? 1.f : b.Pace, false);   // (a finisher is played by its own clock, as made)
                                continue;
                            }
                            TrailPoint q{};
                            XMStoreFloat3(&q.Base, XMVectorLerp(grip, tip, .35f)); XMStoreFloat3(&q.Tip, tip);
                            q.Speed = mTrail[k2].empty() || poseDt <= 0 ? 0.f : Len(Sub(q.Tip, mTrail[k2].back().Tip)) / poseDt;
                            mTrail[k2].push_back(q);
                        }
                    }
                }
            }
            if (composed && mWingScale != 1.f)   // her looks: bigger wings (their roots scaled; the rest of each wing follows)
                for (int side = 0; side < 2; ++side)
                    if (const int j = b.WingJoint[side][0]; j >= 0 && j < static_cast<int>(pose.LocalJoints.size()))
                        pose.LocalJoints[static_cast<size_t>(j)].Scale = {mWingScale, mWingScale, mWingScale};
            if (composed && mSoul && !mShowcase && (b.Soul != 0 || mSoul->Out())) UpdateSoul(world, pose, poseDt);   // (her soul: Q; not while her window shows her)
            if (!composed || !world.ImportedAnimatedCharacter.ApplyGamePresentationPose(b.Skeleton, pose, &error))
            {
                static int told = 0;
                if (told++ < 5) OutputDebugStringA(("[FAIL] Nyxara pose: " + error + "\n").c_str());
            }
            if (composed) b.PoseLog(world, pose, poseDt, planted);
        }
        {   // (CAUCASIAN_EAGLE_NYXARA_LOG_EVERY=1: a line every frame, not every tenth of a second)
            static const bool every = [] { char v[4]{}; size_t n = 0; return getenv_s(&n, v, "CAUCASIAN_EAGLE_NYXARA_LOG_EVERY") == 0 && n > 1 && v[0] == '1'; }();
            if (every || b.Clock - b.LastLog > .1f) { b.Log(world, "frame"); b.LastLog = b.Clock; }
        }

        // ---- the wing boost in flight (Shift): a trail of feathers and light
        const bool boosting = flying && p.IsActuallyMoving && k.Shift;
        if (boosting && !mBoosting) { mNotice = L"WING BOOST"; mNoticeTimer = .6f; }
        mBoosting = boosting;
        if (boosting)
        {
            mTrailTimer -= dt;
            while (mTrailTimer <= 0)
            {
                mTrailTimer += .045f;
                const XMFLOAT3 back = Mul(Forward(p.Yaw), -.5f);
                const float side = (Random() - .5f) * 1.6f;
                Fx f; f.Kind = Random() < .3f ? FxKind::Feather : FxKind::Spark;
                f.Color = Random() < .5f ? MoonWhite : SoulGreen;
                f.Pos = Add(Add(p.Position, back), {std::cos(p.Yaw) * side, .9f + b.LiftNow + Random() * .5f, -std::sin(p.Yaw) * side});
                f.Vel = {(Random() - .5f) * .8f, f.Kind == FxKind::Feather ? 1.5f : .4f, (Random() - .5f) * .8f};
                f.Target = -2; f.Life = .45f + .3f * Random(); f.Size = f.Kind == FxKind::Feather ? .32f : .16f;
                mFx.push_back(f);
            }
        }
        else mTrailTimer = 0;

        if (mFlair)
        {   // her flair: its light goes on through a blow's held moment; what its blows ask of the game's feel
            mFlair->Update(flairDt);
            const NyxaraFlair::Feel feel = mFlair->TakeFeel();
            mHitStop = (std::max)(mHitStop, feel.Stop); mShake = (std::max)(mShake, feel.Shake);
            if (feel.Slow > 0 && mSlow <= 0) { mSlow = mSlowFull = feel.Slow; mSlowRate = feel.SlowRate; }
        }
        mShake *= std::exp(-8.f * realDt); if (mShake < .003f) mShake = 0;
        s.CameraShake = mShake;
        mBarrier = mGuarding ? 1.f : (std::max)(0.f, mBarrier - 6.f * dt);
        mBlockFlash = (std::max)(0.f, mBlockFlash - dt);
        UpdateWraiths(world, combat, dt);
        UpdateEffects(dt);
        if (mFeatherBlows > 0) { b.Log(world, ("feathers struck " + std::to_string(mFeatherBlows)).c_str()); mFeatherBlows = 0; }
        b.UpdateMarks(dt);
        mNoticeTimer = (std::max)(0.f, mNoticeTimer - realDt);
        PublishHud(world);
    }

    void NyxaraKit::UpdateWraiths(WorldState& world, CombatSystem& combat, float dt)
    {
        const auto& player = world.MainPlayer;
        const XMFLOAT3 chest = Add(player.Position, {0, 1.2f, 0});
        const XMFLOAT3 facing = Forward(player.Yaw);
        for (auto& w : mWraiths)
        {
            w.Phase += dt;
            w.Flash = (std::max)(0.f, w.Flash - dt);
            if (mExternalEnemies)
            {   // the journey moves its monsters; the kit keeps knock-back and launches working
                if (!w.Alive) continue;
                w.Pos = Add(w.Pos, Mul(w.Push, dt));
                w.Push = Mul(w.Push, std::exp(-4.f * dt));
                w.LiftVel -= 11.f * dt; w.Lift = (std::max)(0.f, w.Lift + w.LiftVel * dt);
                if (w.Lift <= 0 && w.LiftVel < 0) w.LiftVel = 0;
                continue;
            }
            if (!w.Alive)
            {
                w.Respawn -= dt;
                if (w.Respawn <= 0)
                {
                    w.Alive = true; w.Health = w.MaxHealth; w.Pos = w.Anchor; w.Push = {}; w.Lift = 0; w.LiftVel = 0; w.Fade = 0;
                    Burst(w.Pos, ShadeViolet, 16, 2.f, .25f);
                }
                continue;
            }
            w.Fade = (std::min)(1.f, w.Fade + dt * 1.5f);
            if (w.Pinned || w.Hold > 0) continue;   // her swords are in it, or she holds it
            XMFLOAT3 toPlayer = Sub(chest, w.Pos);
            const float distance = LenXZ(toPlayer);
            XMFLOAT3 goal = w.Anchor;
            if (distance < 17.f)
            {
                XMFLOAT3 away = Sub(w.Pos, player.Position); away.y = 0; away = Norm(away);
                goal = Add(player.Position, Mul(away, 4.2f));
                goal.y = player.Position.y + 1.35f;
            }
            goal.y += .25f * std::sin(w.Phase * 1.7f);
            const XMFLOAT3 delta = Sub(goal, w.Pos);
            const float step = (std::min)(Len(delta), 1.7f * dt);
            w.Pos = Add(w.Pos, Mul(Norm(delta), step));
            w.Pos = Add(w.Pos, Mul(w.Push, dt));
            w.Push = Mul(w.Push, std::exp(-4.f * dt));
            w.LiftVel -= 11.f * dt; w.Lift = (std::max)(0.f, w.Lift + w.LiftVel * dt);
            if (w.Lift <= 0 && w.LiftVel < 0) w.LiftVel = 0;
            w.Yaw = std::atan2(toPlayer.x, toPlayer.z);
            if (distance < 14.f)
            {
                w.Shoot -= dt;
                if (w.Shoot <= 0 && mOrbs.size() < 4)
                {
                    const XMFLOAT3 from = Add(w.Pos, {0, w.Lift, 0});
                    Orb o; o.Pos = from; o.Vel = Mul(Norm(Sub(chest, from)), 7.5f); o.Life = 3.5f;
                    mOrbs.push_back(o); w.Shoot = 4.5f + 2.5f * Random();
                }
            }
        }
        for (auto& o : mOrbs)
        {
            o.Pos = Add(o.Pos, Mul(o.Vel, dt)); o.Life -= dt;
            const XMFLOAT3 rel = Sub(o.Pos, chest);
            const float distance = Len(rel);
            const bool front = mGuarding || (rel.x * facing.x + rel.z * facing.z) > -.3f;   // (her shield is all round her)
            if (mBarrier > .5f && distance < 2.2f && front)
            {
                o.Life = 0; mBlockFlash = .25f;
                if (mHiggsfieldTrial) TrialBiteLight(o.Pos, Mul(Norm(o.Vel), -1), false);   // (the trial: lines, not balls)
                else { Burst(o.Pos, MoonWhite, 14, 4.f, .2f); Burst(o.Pos, SoulGreen, 8, 3.f, .18f); }
                mNotice = L"BLOCKED"; mNoticeTimer = .5f;
                mBody->Log(world, "blocked");
            }
            else if (distance < .65f && combat.PlayerActionsAllowed())
            {
                const XMFLOAT3 came{o.Pos.x - o.Vel.x * .1f, o.Pos.y, o.Pos.z - o.Vel.z * .1f};   // (a little back along its way)
                o.Life = 0; Hurt(world, combat, o.Damage, &came);
                if (!mHiggsfieldTrial) Burst(o.Pos, o.Color, 16, 3.f, .22f);
                else
                {   // (the trial: lines of its light shot out where it struck her, and hot sparks)
                    for (int i = 0; i < 10; ++i)
                        Ray(o.Pos, {Random() - .5f, Random() - .3f, Random() - .5f}, .5f + .6f * Random(), .05f, o.Color, .2f + .1f * Random(), true);
                    Embers(o.Pos, Norm(o.Vel), 10, o.Color, 4.f);
                }
            }
        }
        mOrbs.erase(std::remove_if(mOrbs.begin(), mOrbs.end(), [](const Orb& o) { return o.Life <= 0; }), mOrbs.end());
    }

    bool NyxaraKit::Hurt(WorldState& world, CombatSystem& combat, int damage, const XMFLOAT3* from)
    {
        if (mMercy > 0 || !combat.PlayerActionsAllowed() || mBody->Finish || mBody->Soul == 3) return false;   // (finishing one, nothing touches her)
        mMercy = CaucasianEagle::MercySeconds;
        if (mTrainingMode) ++mTrainingPlayerHits;
        else combat.ApplyIncomingDamage(world, damage);
        auto& b = *mBody;
        b.Struck = true;
        // (how hard: a Chinka's claw 0.67, a blow of 9 one, a Devi's heavy blow 1.6, a boss's at most 1.8; which way:
        // away from where it came, in her axes; from under her feet, none)
        b.StruckWeight = std::clamp(std::sqrt(static_cast<float>((std::max)(damage, 1)) / 9.f), .65f, 1.8f);
        b.StruckX = 0; b.StruckZ = -1; b.StruckBelow = false;
        if (from)
        {
            const auto& p = world.MainPlayer;
            const float dx = p.Position.x - from->x, dz = p.Position.z - from->z, d = std::hypot(dx, dz);
            if (d < .4f) { b.StruckZ = 0; b.StruckBelow = true; }
            else
            {
                const float c = std::cos(p.Yaw), sn = std::sin(p.Yaw);
                b.StruckX = (dx * c - dz * sn) / d; b.StruckZ = (dx * sn + dz * c) / d;
            }
        }
        {
            char said[96];
            std::snprintf(said, sizeof(said), "struck %d weight %.2f way %.2f %.2f%s", damage, b.StruckWeight, b.StruckX, b.StruckZ, b.StruckBelow ? " below" : "");
            b.Log(world, said);
        }
        CaucasianEagle::Sound::Play("combat/blow", .9f); CaucasianEagle::Sound::Play("nyxara/hurt", .9f);
        return true;
    }

    void NyxaraKit::UpdateEffects(float dt)
    {
        mMercy = (std::max)(0.f, mMercy - dt);
        struct FeatherHit { int Target, Damage; XMFLOAT3 At, From; XMFLOAT4 Color; };
        std::vector<FeatherHit> hits;   // (struck after the loop: a blow adds effects, and they are in the list it goes through)
        for (auto& f : mFx)
        {
            f.T += dt;
            switch (f.Kind)
            {
            case FxKind::Spark:
                f.Pos = Add(f.Pos, Mul(f.Vel, dt)); f.Vel.y -= 4.f * dt; f.Vel = Mul(f.Vel, std::exp(-1.5f * dt)); break;
            case FxKind::Ember:
                f.Pos = Add(f.Pos, Mul(f.Vel, dt));
                f.Vel.y -= 6.f * dt; f.Vel = Mul(f.Vel, std::exp(-3.f * dt)); break;
            case FxKind::Ray:
                break;
            case FxKind::Shard:
                if (f.Stuck) break;
                f.Pos = Add(f.Pos, Mul(f.Vel, dt));
                f.Vel.y -= 7.f * dt; f.Vel = Mul(f.Vel, std::exp(-1.8f * dt)); f.Roll += f.Yaw * dt; break;
            case FxKind::Gust:
                f.Pos = Add(f.Pos, Mul(f.Vel, dt)); f.Vel = Mul(f.Vel, std::exp(-2.5f * dt)); break;
            case FxKind::Flame:
                f.Pos = Add(f.Pos, Mul(f.Vel, dt)); f.Vel.y += 2.5f * dt; f.Vel.x *= std::exp(-3.f * dt); f.Vel.z *= std::exp(-3.f * dt); break;
            case FxKind::Feather:
                if (f.Target == -3)
                {   // one of the feather storm's daggers: straight on, falling a little, until the ground takes it; there it
                    // sticks where its way met the ground, its tip in, standing a little steeper than it came
                    if (f.Stuck) break;
                    const XMFLOAT3 was = f.Pos;
                    f.Vel.y -= StormDrop * dt;
                    f.Pos = Add(f.Pos, Mul(f.Vel, dt));
                    float floor = f.Floor;
                    if (mFootGround) { const float g = mFootGround(f.Pos.x, f.Pos.z, f.Floor); if (std::isfinite(g)) floor = g; }
                    if (f.Pos.y <= floor && was.y > f.Pos.y)
                    {
                        const float k = std::clamp((was.y - floor) / (was.y - f.Pos.y), 0.f, 1.f);
                        const XMFLOAT3 met = Add(was, Mul(Sub(f.Pos, was), k)), flat = Norm({f.Vel.x, 0, f.Vel.z});
                        const float slant = (std::max)(StormStuckSlant, std::atan2(-f.Vel.y, LenXZ(f.Vel)));
                        f.Vel = {flat.x * std::cos(slant), -std::sin(slant), flat.z * std::cos(slant)};   // (stuck, only its way is kept)
                        f.Pos = Sub(met, Mul(f.Vel, 2.2f * f.Size * (.5f - StormStuckIn)));                // (it is drawn about its middle)
                        f.Stuck = true; f.Life = f.T + StormStuckSeconds;
                    }
                }
                else if (f.Target >= 0)
                {   // one aimed at a creature: straight at it, bending in; it strikes where its way this frame passes nearest
                    // the creature (at its speed it would go through between frames)
                    if (f.Target < static_cast<int>(mWraiths.size()) && mWraiths[f.Target].Alive)
                    {
                        const auto& w = mWraiths[f.Target];
                        const XMFLOAT3 aim = Add(w.Pos, {0, -.15f, 0}), to = Sub(aim, f.Pos);
                        const float d = Len(to), speed = (std::max)(StormDaggerSpeed, Len(f.Vel));
                        const float bend = (std::min)(1.f, dt * (6.f + 14.f * f.T));
                        f.Vel = Add(f.Vel, Mul(Sub(Mul(to, speed / (std::max)(d, 1e-3f)), f.Vel), bend));
                        const XMFLOAT3 step = Mul(f.Vel, dt);
                        const XMFLOAT3 nearest = Add(f.Pos, Mul(step, std::clamp(Dot(to, step) / (std::max)(1e-6f, Dot(step, step)), 0.f, 1.f)));
                        if (Len(Sub(aim, nearest)) < .45f + .5f * w.Radius)
                        {
                            hits.push_back({f.Target, f.Damage, nearest, Sub(w.Pos, Mul(Norm(f.Vel), 2.f)), f.Color});
                            f.T = f.Life;
                            break;
                        }
                    }
                    else f.Target = -3;   // it is gone: the dagger flies on, and down to the ground
                    f.Pos = Add(f.Pos, Mul(f.Vel, dt));
                }
                else { f.Pos = Add(f.Pos, Mul(f.Vel, dt)); f.Vel.y -= 9.f * dt; }
                break;
            default: break;
            }
        }
        for (const auto& h : hits)
        {
            if (h.Damage > 0) { DamageWraith(h.Target, h.Damage, h.From, StormPush, 0); ++mFeatherBlows; }
            Burst(h.At, h.Color, 5, 2.5f, .14f);
        }
        mFx.erase(std::remove_if(mFx.begin(), mFx.end(), [](const Fx& f) { return f.T >= f.Life; }), mFx.end());
        if (mFx.size() > 900) mFx.erase(mFx.begin(), mFx.begin() + static_cast<std::ptrdiff_t>(mFx.size() - 900));
    }

    void NyxaraKit::PublishHud(WorldState& world) const
    {
        auto& hud = world.CombatFeedback;
        const auto skill = [&](int i)
        {
            return mSkillCooldown[i] <= 0 ? std::wstring(L"READY") : std::to_wstring(static_cast<int>(std::ceil(mSkillCooldown[i]))) + L"s";
        };
        const bool flying = world.MainPlayer.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
        hud.ActionStatus = std::wstring(L"NYXARA  TWO SWORDS  |  ") + (flying ? L"F DASH " : L"F SLIDE ") + skill(0) + L"  V SWORDS " + skill(1) + L"  C STORM " + skill(2) +
            L"  Q SOUL " + (mBody && mBody->Soul != 0 ? std::wstring(L"AWAY") : mSoulCooldown <= 0 ? std::wstring(L"READY")
                                                                                  : std::to_wstring(static_cast<int>(std::ceil(mSoulCooldown))) + L"s") +
            (mBoosting ? L"  |  WING BOOST" : L"");
        hud.Controls = L"LMB/RMB LEFT / RIGHT SWORD  F SLIDE  V THROWN SWORDS  C FEATHER STORM  E BARRIER  Q SOUL  A/D TURN  SHIFT RUN / BOOST  SPACE JUMP / CLIMB (HELD 1.5 S: FLY)  CTRL+SPACE SINK  R LAND";
        if (mNoticeTimer > 0) { hud.ActionNotice = mNotice; hud.ActionNoticeTimer = mNoticeTimer; }

        auto& h = NyxaraHud();   // the graphical HUD (DrawNyxaraHud)
        h.Valid = true;
        h.Flight = mFlightEnergy; h.FlightCharge = FlightCharge(); h.Flying = world.MainPlayer.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
        h.FlightDash = mFlightDash;
        for (int i = 0; i < 3; ++i) { h.Cooldown[i] = mSkillCooldown[i]; h.CooldownMax[i] = SkillCooldown[i] * mCooldownScale; }
        const auto& b = *mBody;
        h.ComboOpen = false; h.ComboWindow = 0; h.ComboLeft = L""; h.ComboRight = L"";
        h.Action = b.Soul == 3 ? L"TO HER SOUL" : b.Soul == 1 ? NyxaraSoul::Name : b.InMove() ? b.MoveOf->Name : b.Cast ? L"THROWN SWORDS" : b.Air == 1 ? L"TWISTED DASH"
                 : b.Attack == "Skill_Storm" ? L"FEATHER STORM" : b.Attack == "Skill_Slide" ? L"SLIDE" : L"";
        h.SoulOut = b.Soul != 0; h.SoulRest = mSoulCooldown; h.SoulRestMax = SoulCooldownSeconds * mCooldownScale;   // (her soul, Q: its slot, its rest)
        h.SkillsVC = mSkillsVC; h.Guarding = mGuarding; h.GuardCooldown = mGuarding ? 0.f : mGuardCooldown; h.GuardCooldownMax = ShieldRestSeconds * mCooldownScale;
        h.GuardLeft = mGuarding ? (std::max)(0.f, 1 - mGuardHeld / ShieldSeconds) : 0.f; h.Boosting = mBoosting; h.Swapped = false;
        h.Bow = false; h.Aim = 0; h.Shooting = false; h.OnTarget = false; h.HitMark = 0; h.HitKill = false; h.TargetVisible = false;
        h.Slashing = b.Attack.rfind("Attack_L", 0) == 0 || b.Attack == "Attack_Jump" || (b.InMove() && b.MoveButton == 0);
        h.Heavy = b.Attack.rfind("Attack_R", 0) == 0 || (b.InMove() && b.MoveButton == 1); h.Swapping = false;
        h.Notice = mNotice.c_str(); h.NoticeTimer = mNoticeTimer;
    }

    void NyxaraKit::Draw(Renderer& renderer, const WorldState& world, const XMMATRIX& viewProjection)
    {
        if (!mReady) return;
        {   // shade health bars for the HUD (the kit's own shades)
            auto& h = NyxaraHud(); h.EnemyCount = 0;
            for (const auto& w : mWraiths)
            {
                if (h.EnemyCount >= 8 || mExternalEnemies) break;
                auto& e = h.Enemies[h.EnemyCount++];
                const XMVECTOR clip = XMVector4Transform(XMVectorSet(w.Pos.x, w.Pos.y + w.Lift + 1.35f, w.Pos.z, 1), viewProjection);
                const float cw = XMVectorGetW(clip);
                e.Visible = w.Alive && cw > .2f;
                if (e.Visible)
                {
                    e.X = XMVectorGetX(clip) / cw * .5f + .5f; e.Y = .5f - XMVectorGetY(clip) / cw * .5f;
                    e.Health = static_cast<float>(w.Health) / static_cast<float>(w.MaxHealth); e.Flash = w.Flash;
                    e.Visible = e.X > -.05f && e.X < 1.05f && e.Y > -.05f && e.Y < 1.05f;
                }
            }
        }
        renderer.BindDoorwaysSkinningDisabled();
        const XMFLOAT3 eye = world.MainCamera.Position;
        const auto material = [&](XMFLOAT4 color, ID3D11ShaderResourceView* texture, float emission, bool blend)
        {
            Material m{};
            m.Diffuse = color; m.DiffuseMap = texture ? texture : renderer.mWhiteTextureView.Get();
            m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .6f; m.EmissiveStrength = emission;
            m.SpecularColor = {0, 0, 0, 0}; m.DoubleSided = true;
            m.AlphaMode = blend ? DoorwaysMaterialAlphaMode::Blend : DoorwaysMaterialAlphaMode::Opaque;
            return m;
        };
        {   // what her blows leave (opaque): blood, stains, the streak, sparks, the air she turns
            const auto& marks = mBody->Marks;
            const Material blood = material({.3f, .008f, .008f, 1}, nullptr, .1f, false), stain = material({.2f, .006f, .006f, 1}, nullptr, .04f, false),
                           streakLight = material({1, .93f, .88f, 1}, nullptr, 3.f, false), edge = material({.9f, .08f, .05f, 1}, nullptr, 1.6f, false),
                           spark = material({1, .7f, .45f, 1}, nullptr, 3.f, false), air = material({.62f, .74f, .86f, 1}, nullptr, 1.1f, false);
            for (const auto& f : marks)
            {
                const float part = std::clamp(f.Life / (std::max)(f.Full, .01f), 0.f, 1.f);
                const XMMATRIX at = XMMatrixTranslation(f.At.x, f.At.y, f.At.z);
                if (f.Kind == 0) renderer.DrawBox(XMMatrixScaling(f.Size, f.Size * 1.5f, f.Size) * at, viewProjection, blood);
                else if (f.Kind == 1)
                {
                    const float sz = f.Size * (std::min)(1.f, part * 4);
                    renderer.DrawBox(XMMatrixScaling(sz, .006f, sz * .8f) * XMMatrixRotationY(f.Go.x) * at, viewProjection, stain);
                }
                else if (f.Kind == 3) renderer.DrawBox(XMMatrixScaling(f.Size * part, f.Size * part, f.Size * part) * at, viewProjection, spark);
                else
                {   // the streak (2), turned air (4) and a finisher's cut (5): long and thin along Go
                    const XMVECTOR x = XMVector3Normalize(XMLoadFloat3(&f.Go));
                    XMVECTOR z = XMVector3Cross(x, XMVectorSet(0, 1, 0, 0));
                    if (XMVectorGetX(XMVector3Length(z)) < 1e-3f) z = XMVectorSet(1, 0, 0, 0);
                    z = XMVector3Normalize(z);
                    XMMATRIX turn = XMMatrixIdentity();
                    turn.r[0] = XMVectorSetW(x, 0); turn.r[1] = XMVectorSetW(XMVector3Cross(z, x), 0); turn.r[2] = XMVectorSetW(z, 0);
                    if (f.Kind == 4) renderer.DrawBox(XMMatrixScaling(.16f + .3f * (1 - part), f.Size * part, f.Size * part * .5f) * turn * at, viewProjection, air);
                    else if (f.Kind == 5)
                    {   // drawn across in its first 0.06 s from where the blade came in, then thinning away
                        const float drawn = std::clamp((f.Full - f.Life) / .06f, 0.f, 1.f), length = f.Size * drawn, thick = .07f * std::sqrt(part);
                        const XMMATRIX from = XMMatrixTranslation(-.5f * f.Size * (1 - drawn), 0, 0) * turn * at;
                        renderer.DrawBox(XMMatrixScaling(length, thick * 1.6f, thick * .5f) * from, viewProjection, edge);
                        renderer.DrawBox(XMMatrixScaling(length * .96f, thick * .6f, thick * .6f) * from, viewProjection, streakLight);
                    }
                    else
                    {
                        const float length = .7f + 1.3f * (1 - part), thick = .045f * part;
                        renderer.DrawBox(XMMatrixScaling(length, thick * 1.8f, thick * .5f) * turn * at, viewProjection, edge);
                        renderer.DrawBox(XMMatrixScaling(length * .92f, thick * .7f, thick * .7f) * turn * at, viewProjection, streakLight);
                    }
                }
            }
        }
        struct DrawItem { const DoorwaysGpuMesh* Mesh; XMFLOAT4X4 M; Material Mat; float Depth; };
        std::vector<DrawItem> list; list.reserve(mFx.size() * 2 + 64);
        const auto translucent = [&](const DoorwaysGpuMesh& mesh, const XMMATRIX& m, const Material& mat)
        {
            const XMVECTOR c = XMVector4Transform(XMVector4Transform(XMVectorSet(0, 0, 0, 1), m), viewProjection);
            const float depth = XMVectorGetW(c);
            if (!std::isfinite(depth)) return;
            DrawItem d{&mesh, {}, mat, depth}; XMStoreFloat4x4(&d.M, m); list.push_back(d);
        };
        const auto billboard = [&](XMFLOAT3 at, float size, XMFLOAT4 color, ID3D11ShaderResourceView* tex)
        {
            const XMFLOAT3 n = Norm(Sub(eye, at));
            XMFLOAT3 x = Cross({0, 1, 0}, n); x = Len(x) < 1e-3f ? XMFLOAT3{1, 0, 0} : Norm(x);
            const XMFLOAT3 y = Cross(n, x);
            translucent(mQuadMesh, Basis(Mul(x, size), Mul(y, size), n, at), material(color, tex, 1, true));
        };
        const auto streak = [&](const DoorwaysGpuMesh& mesh, XMFLOAT3 at, XMFLOAT3 dir, float length, float width, XMFLOAT4 color, ID3D11ShaderResourceView* tex, float emission)
        {   // card whose +Y follows dir and faces the camera around that axis
            const XMFLOAT3 y = Norm(dir);
            XMFLOAT3 x = Cross(y, Norm(Sub(eye, at))); x = Len(x) < 1e-3f ? XMFLOAT3{1, 0, 0} : Norm(x);
            const XMFLOAT3 z = Cross(x, y);
            translucent(mesh, Basis(Mul(x, width), Mul(y, length), z, at), material(color, tex, emission, true));
        };

        // the kit's own shade wraiths (opaque core) and their bars
        for (const auto& w : mWraiths)
        {
            if (!w.Alive || mExternalEnemies) continue;
            const XMFLOAT3 p = Add(w.Pos, {0, w.Lift, 0});
            const float wob = std::sin(w.Phase * 2.3f) * .08f, s = .35f + .65f * w.Fade;
            const XMMATRIX root = XMMatrixRotationY(w.Yaw) * XMMatrixTranslation(p.x, p.y, p.z);
            const float flash = w.Flash > 0 ? 1.f : 0.f;
            const auto body = material({.03f + .8f * flash, .03f + .8f * flash, .045f + .8f * flash, 1}, nullptr, .15f + .85f * flash, false);
            renderer.DrawDoorwaysGpuMesh(mSphereMesh, XMMatrixScaling(.42f * s, .62f * s, .42f * s) * root, viewProjection, body);
            renderer.DrawDoorwaysGpuMesh(mConeMesh, XMMatrixScaling(.40f * s, 1.25f * s, .40f * s) * XMMatrixRotationX(Pi + wob) *
                XMMatrixTranslation(0, -.2f * s, 0) * root, viewProjection, body);
            renderer.DrawDoorwaysGpuMesh(mConeMesh, XMMatrixScaling(.36f * s, .55f * s, .36f * s) * XMMatrixRotationX(-.25f) *
                XMMatrixTranslation(0, .38f * s, -.05f) * root, viewProjection, body);
            const auto eyeMat = material(ShadeViolet, nullptr, 1, false);
            for (const float side : {-1.f, 1.f})
                renderer.DrawDoorwaysGpuMesh(mSphereMesh, XMMatrixScaling(.055f, .035f, .04f) *
                    XMMatrixTranslation(side * .12f * s, .2f * s, .33f * s) * root, viewProjection, eyeMat);
            const float ratio = static_cast<float>(w.Health) / static_cast<float>(w.MaxHealth);
            Material bar = material({.04f, .04f, .05f, 1}, nullptr, 0, false), hp = material({.55f, .25f, 1.f, 1}, nullptr, .8f, false);
            const XMMATRIX face = XMMatrixRotationY(std::atan2(eye.x - p.x, eye.z - p.z));
            renderer.DrawBox(XMMatrixScaling(1.0f, .07f, .03f) * face * XMMatrixTranslation(p.x, p.y + 1.05f, p.z), viewProjection, bar);
            if (ratio > 0)
                renderer.DrawBox(XMMatrixScaling(.94f * ratio, .045f, .035f) * XMMatrixTranslation(-.47f * (1 - ratio), 0, 0) * face *
                    XMMatrixTranslation(p.x, p.y + 1.05f, p.z), viewProjection, hp);
            billboard(p, 2.2f * s, Alpha(ShadeViolet, .30f), mGlowTex.Get());
        }
        for (const auto& o : mOrbs)
        {
            renderer.DrawDoorwaysGpuMesh(mSphereMesh, XMMatrixScaling(.14f, .14f, .14f) * XMMatrixTranslation(o.Pos.x, o.Pos.y, o.Pos.z),
                viewProjection, material({.5f + .5f * o.Color.x, .5f + .5f * o.Color.y, .5f + .5f * o.Color.z, 1}, nullptr, 1, false));
            billboard(o.Pos, .9f, Alpha(o.Color, .8f), mGlowTex.Get());
            streak(mFeatherMesh, o.Pos, Mul(o.Vel, -1), .9f, .35f, Alpha(o.Color, .45f), mGlowTex.Get(), 1);
        }

        // her wing shield (E): blue lights about her closed wings (owner, 2026-11-20; the barrier's crossed swords, dome
        // and ring are gone), as her shield has them
        if (mBody->WingLineKnown && mGuardStyle >= 0 && mGuardStyle < static_cast<int>(Guards().size()))
        {
            const GuardSpec& gs = Guards()[static_cast<size_t>(mGuardStyle)];
            const auto& line = mBody->WingLine;
            const float shown = (std::min)(1.f, mBody->WingClose), t = static_cast<float>(world.SceneTime);
            const auto along = [&](int k, float u)
            {   // u of the way along a wing, root (0) to its feathers' tip (1)
                float len[3], all = 0;
                for (int i = 0; i < 3; ++i) { len[i] = Len(Sub(line[k][i + 1], line[k][i])); all += len[i]; }
                float d = std::clamp(u, 0.f, 1.f) * all;
                for (int i = 0; i < 3; ++i)
                {
                    if (d <= len[i] || i == 2) return Lerp(line[k][i], line[k][i + 1], len[i] > 1e-4f ? std::clamp(d / len[i], 0.f, 1.f) : 0.f);
                    d -= len[i];
                }
                return line[k][3];
            };
            const auto hash = [](int i) { const float x = std::sin(static_cast<float>(i) * 12.9898f) * 43758.547f; return x - std::floor(x); };
            const XMFLOAT3 middle = Mul(Add(Add(line[0][0], line[1][0]), Add(line[0][3], line[1][3])), .25f);
            for (int i = 0; i < gs.Lights; ++i)
            {
                const int k = i % 2, n = (std::max)(1, gs.Lights / 2), m = i / 2;
                const float r = hash(i), r2 = hash(i + 101);
                float glow = 1, size = gs.Size;
                XMFLOAT3 at{};
                switch (gs.Kind)
                {
                case 1:
                {   // round her, at the height of her wings
                    const float a = t * gs.Speed + XM_2PI * static_cast<float>(i) / static_cast<float>(gs.Lights);
                    const float reach = .75f + .25f * r + gs.Spread * std::sin(t * 1.3f + i);
                    at = Add(middle, {std::sin(a) * reach, (r2 - .5f) * 1.1f + .15f * std::sin(t * 2 + i), std::cos(a) * reach});
                    break;
                }
                case 2:
                {   // at her wing tips, and one along each wing's edge
                    const float u = m == 0 ? 1.f : m == 1 ? .66f : .35f + .3f * r;
                    at = along(k, u);
                    at.y += gs.Spread * std::sin(t * 2.2f + i);
                    size *= m == 0 ? 1.f : .6f;
                    break;
                }
                case 3:
                {   // flames rising from her wings
                    const float u = (static_cast<float>(m) + .5f) / static_cast<float>(n), rise = std::fmod(t * gs.Speed * (.7f + .6f * r) + r2, 1.f);
                    at = along(k, u);
                    at.y += rise * gs.Spread; at.x += .06f * std::sin(t * 7 + i); at.z += .06f * std::cos(t * 6 + i);
                    glow = 1 - rise; size *= .6f + .6f * (1 - rise);
                    break;
                }
                case 4:
                {   // waves from her shoulders to her wing tips
                    const float u = (static_cast<float>(m) + .5f) / static_cast<float>(n);
                    at = along(k, u);
                    glow = .25f + .75f * std::pow(.5f + .5f * std::sin(XM_2PI * (gs.Pulse * t - u * 1.4f)), 3.f);
                    break;
                }
                default:
                {   // running along her wings, each about its own little circle
                    const float u = std::fmod((static_cast<float>(m) + .5f) / static_cast<float>(n) + t * gs.Speed + 1000.f, 1.f);
                    at = along(k, u);
                    const float a = t * 3 + i * 2.4f;
                    at = Add(at, {gs.Spread * std::cos(a), gs.Spread * std::sin(a * 1.3f), gs.Spread * std::sin(a)});
                    break;
                }
                }
                if (gs.Kind != 4 && gs.Pulse > 0) glow *= .7f + .3f * std::sin(XM_2PI * gs.Pulse * t + (gs.Kind == 0 ? 0.f : i * .7f));
                if (gs.Twinkle > 0) glow *= 1 - gs.Twinkle * (.5f + .5f * std::sin(t * (9 + 7 * r) + 30 * r2));
                glow *= shown;
                if (glow < .02f) continue;
                // (a wide soft blue halo, the light itself, and its bright heart)
                billboard(at, size * 2.4f * (.7f + .3f * glow), Alpha(gs.Color, .28f * glow), mGlowTex.Get());
                billboard(at, size * (.7f + .3f * glow), Alpha(gs.Color, glow), mGlowTex.Get());
                billboard(at, size * .3f, XMFLOAT4{.75f, .9f, 1.f, .9f * glow}, mGlowTex.Get());
            }
        }

        for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
        {   // a doomed creature: a ring pulsing under it, hers to finish (and how long it has: the ring closes in)
            const auto& w = mWraiths[i];
            if (!w.Alive || w.Doom <= 0 || (mBody->Finish && mBody->FinishTarget == i)) continue;
            const float left = std::clamp(w.Doom / DoomSeconds, 0.f, 1.f), beat = .5f + .5f * std::sin(mBody->Clock * 9.f);
            const float radius = (w.Radius + .45f) * (.75f + .25f * left) * (1 + .06f * beat);
            const XMFLOAT3 feet = FeetOf(w);
            translucent(mRingMesh, XMMatrixScaling(radius, 1, radius) * XMMatrixTranslation(feet.x, feet.y + .05f, feet.z),
                material(Alpha(ShadeViolet, .45f + .4f * beat), mRingTex.Get(), 1.4f, true));
        }
        for (const auto& f : mFx)
        {
            const float t = f.T / f.Life, fade = 1 - Smooth(.55f, 1.f, t);
            switch (f.Kind)
            {
            case FxKind::Ring:
            {
                const float radius = f.Size * (.2f + .8f * Smooth(0, 1, t / .8f));
                translucent(mRingMesh, XMMatrixScaling(radius, 1, radius) * XMMatrixTranslation(f.Pos.x, f.Pos.y, f.Pos.z),
                    material(Alpha(f.Color, .9f * fade), mRingTex.Get(), 1, true));
                break;
            }
            case FxKind::Feather:
                if (f.Target == -2)
                {   // the wing boost's: light
                    streak(mFeatherMesh, Sub(f.Pos, Mul(Norm(f.Vel), f.Size)), f.Vel, f.Size * 1.6f, f.Size * .6f, Alpha(f.Color, .9f * fade), mFeatherTex.Get(), 1);
                    billboard(f.Pos, .45f, Alpha(f.Color, .6f * fade), mGlowTex.Get());
                }
                else
                {   // the storm's feather daggers: her wing feathers, lit as her wings are (a little light of their own; the
                    // white ones shine), tip first and straight the way they fly, a thin line of air behind; stuck in the
                    // ground, half in it, fading at the last
                    const float shown = f.Stuck ? 1 - Smooth(f.Life - StormStuckFade, f.Life, f.T) : 1 - Smooth(f.Life - .25f, f.Life, f.T);
                    const XMFLOAT3 way = Len(f.Vel) < 1e-3f ? XMFLOAT3{0, 1, 0} : f.Vel;
                    streak(mFeatherMesh, Sub(f.Pos, Mul(Norm(way), f.Size * 1.1f)), way, f.Size * 2.2f, f.Size * .5f, Alpha(f.Color, shown),
                           mWingFeatherTex.Get(), f.Glow ? 1.f : .2f);
                    if (!f.Stuck)
                        streak(mFeatherMesh, f.Pos, Mul(way, -1), (std::min)(1.6f, .05f * Len(way)), .05f, Alpha(MoonWhite, .22f * shown), mGlowTex.Get(), 1);
                }
                break;
            case FxKind::Spark:
                billboard(f.Pos, f.Size * (1 - .5f * t), Alpha(f.Color, fade), mGlowTex.Get());
                break;
            case FxKind::Ray:
            {   // a line of light: soft blue about a thin white heart; shot out (Mirror) it runs out and thins as it goes
                const float g = f.Mirror ? Smooth(0, 1, t / .4f) : 0.f;
                const XMFLOAT3 from = f.Mirror ? Add(f.Pos, Mul(f.Vel, f.Size * .45f * g)) : f.Pos;
                const float len = f.Size * (f.Mirror ? .3f + .7f * g : 1.f), wide = f.Roll * (f.Mirror ? 1 - .7f * t : 1 - .5f * t), shown = 1 - Smooth(.35f, 1.f, t);
                streak(mFeatherMesh, from, f.Vel, len, wide * 2.2f, Alpha(f.Color, .55f * shown * f.Color.w), mGlowTex.Get(), 1.6f);
                streak(mFeatherMesh, Add(from, Mul(f.Vel, len * .12f)), f.Vel, len * .76f, wide * .55f, Alpha(MoonWhite, .95f * shown), mGlowTex.Get(), 2.f);
                break;
            }
            case FxKind::Shard:
            {   // a crystal: a long diamond along its way (upright, stuck), turning about it, a bright heart in it
                const XMFLOAT3 axis = f.Stuck ? Norm(f.Vel) : (Len(f.Vel) > .05f ? Norm(f.Vel) : XMFLOAT3{0, 1, 0});
                const float grow = f.Stuck ? Smooth(0, .12f, f.T) : 1.f, shown = f.Stuck ? 1 - Smooth(f.Life * .6f, f.Life, f.T) : fade;
                const float len = f.Size * grow, wide = f.Size * (f.Stuck ? .2f : .26f);
                XMFLOAT3 across = Cross(axis, Norm(Sub(eye, f.Pos)));
                across = Len(across) < 1e-3f ? XMFLOAT3{1, 0, 0} : Norm(across);
                const XMFLOAT3 turned = Norm(Add(Mul(across, std::cos(f.Roll)), Mul(Norm(Cross(axis, across)), std::sin(f.Roll) * .5f)));
                const XMFLOAT3 base = f.Stuck ? f.Pos : Sub(f.Pos, Mul(axis, len * .5f)), tip = Add(base, Mul(axis, len));
                const XMFLOAT3 mid = Add(base, Mul(axis, len * .35f));
                const auto diamond = [&](float w, XMFLOAT4 c, float glow)
                {
                    const XMFLOAT3 l = Sub(mid, Mul(turned, w)), r = Add(mid, Mul(turned, w));
                    const Material mat = material(c, nullptr, glow, true);
                    for (const auto& t3 : {std::array<XMFLOAT3, 3>{base, r, tip}, std::array<XMFLOAT3, 3>{base, tip, l}})
                    {
                        const XMFLOAT3 x = Sub(t3[1], t3[0]), y = Sub(t3[2], t3[0]), n = Cross(x, y);
                        if (Len(n) > 1e-6f) translucent(mTriMesh, Basis(x, y, Norm(n), t3[0]), mat);
                    }
                };
                // (clear ice: a pale blue body you see through, a thin bright heart down its length)
                diamond(wide, Alpha(f.Color, .42f * shown), 1.2f);
                diamond(wide * .25f, Alpha(ThemeCore(), .85f * shown), 2.f);
                break;
            }
            case FxKind::Gust:
            {   // a line of wind: thin and pale, its tail behind it, coming in and going
                const float speed = Len(f.Vel);
                if (speed < .05f) break;
                const float shown = Smooth(0, .2f, t) * (1 - Smooth(.5f, 1.f, t));
                streak(mFeatherMesh, f.Pos, Mul(f.Vel, -1), f.Size * (.6f + .4f * t), f.Roll * 2.6f, Alpha(f.Color, .7f * shown), mGlowTex.Get(), 1.5f);
                streak(mFeatherMesh, f.Pos, Mul(f.Vel, -1), f.Size * .55f, f.Roll * .7f, Alpha(XMFLOAT4{1, 1, 1, 1}, .8f * shown), mGlowTex.Get(), 1.8f);
                break;
            }
            case FxKind::Flame:
            {   // a tongue of fire: tall and thin, flickering, yellow-white at its birth, orange, red and gone
                const XMFLOAT4 hot{1.f, .72f, .22f, 1}, mid{1.f, .36f, .04f, 1}, cool{.75f, .08f, .02f, 1};
                const float a = Smooth(0, .45f, t), c2 = Smooth(.45f, 1.f, t);
                const XMFLOAT4 col{hot.x + (mid.x - hot.x) * a + (cool.x - mid.x) * c2, hot.y + (mid.y - hot.y) * a + (cool.y - mid.y) * c2,
                                   hot.z + (mid.z - hot.z) * a + (cool.z - mid.z) * c2, 1};
                const float flick = .75f + .25f * std::sin(f.T * 38.f + f.Roll), rise = Smooth(0, .15f, t) * (1 - .4f * t);
                const XMFLOAT3 up = Norm(Add({0, 1, 0}, Mul(f.Vel, .15f)));
                streak(mFeatherMesh, f.Pos, up, f.Size * rise * flick, f.Size * .26f * (1 - .6f * t), Alpha(col, .85f * fade), mGlowTex.Get(), 1.5f);
                streak(mFeatherMesh, f.Pos, up, f.Size * .45f * rise, f.Size * .08f, Alpha(XMFLOAT4{1.f, .85f, .4f, 1}, .7f * fade * (1 - t)), mGlowTex.Get(), 2.f);
                break;
            }
            case FxKind::Ember:
            {
                const float speed = Len(f.Vel);
                if (speed < .05f) break;
                streak(mFeatherMesh, f.Pos, Mul(f.Vel, -1), std::clamp(speed * .045f, .06f, .4f), f.Size * 2.f, Alpha(f.Color, fade), mGlowTex.Get(), 2.f);
                break;
            }
            }
        }

        for (int k = 0; k < 2; ++k)
        {   // a style's move: each blade's trail, a ribbon of light along its way a moment back (the left green, the right
            // white; brightest where it is new and where the blade went fast; on a curve through its places, Catmull-Rom)
            const auto& tr = mTrail[k];
            if (tr.size() < 2) continue;
            const bool lit = mHiggsfieldTrial;   // (the trial: a thin bright slash at her blade's outer part, a white edge in a blue one)
            const XMFLOAT4 color = lit ? ThemeMain() : k == 0 ? SoulGreen : MoonWhite;
            const auto curve = [](const XMFLOAT3& a, const XMFLOAT3& b2, const XMFLOAT3& c, const XMFLOAT3& d, float u)
            {
                const auto one = [u](float p0, float p1, float p2, float p3)
                { return .5f * (2 * p1 + (p2 - p0) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (3 * p1 - p0 - 3 * p2 + p3) * u * u * u); };
                return XMFLOAT3{one(a.x, b2.x, c.x, d.x), one(a.y, b2.y, c.y, d.y), one(a.z, b2.z, c.z, d.z)};
            };
            const auto tri = [&](const XMFLOAT3& a, const XMFLOAT3& b2, const XMFLOAT3& c, const Material& mat)
            {
                const XMFLOAT3 x = Sub(b2, a), y = Sub(c, a), n = Cross(x, y);
                if (Len(n) < 1e-5f) return;
                translucent(mTriMesh, Basis(x, y, Norm(n), a), mat);
            };
            constexpr int Steps = 4;
            for (size_t i = 0; i + 1 < tr.size(); ++i)
            {
                const auto& p0 = tr[i == 0 ? 0 : i - 1]; const auto& p1 = tr[i]; const auto& p2 = tr[i + 1]; const auto& p3 = tr[(std::min)(i + 2, tr.size() - 1)];
                const float fast = std::clamp((p2.Speed - 6.f) / 12.f, 0.f, 1.f), shade = .55f * fast * (1 - p2.Age / StyleTrailSeconds);
                if (shade < .02f) continue;
                const auto band = [&](float inner, const Material& mat)
                {
                    XMFLOAT3 baseWas = Lerp(p1.Base, p1.Tip, inner), tipWas = p1.Tip;
                    for (int step = 1; step <= Steps; ++step)
                    {
                        const float u = static_cast<float>(step) / Steps;
                        const XMFLOAT3 tip = curve(p0.Tip, p1.Tip, p2.Tip, p3.Tip, u), base = Lerp(curve(p0.Base, p1.Base, p2.Base, p3.Base, u), tip, inner);
                        tri(baseWas, tipWas, tip, mat);
                        tri(baseWas, tip, base, mat);
                        baseWas = base; tipWas = tip;
                    }
                };
                if (!lit) band(0, material(Alpha(color, shade), nullptr, 1.6f, true));
                else
                {
                    band(.45f, material(Alpha(color, .8f * shade), nullptr, 1.8f, true));
                    band(.85f, material(Alpha(ThemeCore(), (std::min)(1.f, 1.6f * shade)), nullptr, 2.2f, true));
                }
            }
        }
        std::stable_sort(list.begin(), list.end(), [](const DrawItem& a, const DrawItem& c) { return a.Depth > c.Depth; });
        for (const auto& d : list)
        {
            renderer.BindDoorwaysMaterialTexture(d.Mat);
            renderer.SetAlphaBlendingEnabled(true);
            renderer.SetDepthWriteEnabled(false);
            const XMMATRIX m = XMLoadFloat4x4(&d.M);
            for (const auto& submesh : d.Mesh->Submeshes)
                renderer.DrawDoorwaysGpuSubmesh(*d.Mesh, submesh, m, viewProjection, d.Mat, false, true);
        }
        renderer.ResetDefaultWorldRenderState();
        renderer.BindDoorwaysSkinningDisabled();
        if (mSoul && mWorldRenderer)
        {   // her soul (Q), before her flair; nothing of it while her window shows her (she stands over the summit then)
            if (!mSoul->Loaded() && !mSoul->Failed() && !mSoul->Load(renderer, mSoulError))
                OutputDebugStringA(("[WARN] Nyxara: her soul's picture could not be loaded: " + mSoulError + "\n").c_str());
            if (mSoul->Loaded())   // (her flair, on, sets the picture's glow itself)
                mSoul->Draw(renderer, *mWorldRenderer, world.ImportedAnimatedCharacter, eye, viewProjection, mFlair && mFlair->On(), mShowcase);
        }
        if (mFlair)
        {   // her flair, over all the rest (none of it unless the Character Lab asked: SetFlair)
            if (mFlair->Chosen() && !mFlair->Loaded() && !mFlair->Failed()) mFlair->Load(renderer, mFlairError);
            mFlair->Draw(renderer, eye, viewProjection);
        }
    }
}
