#include "NyxaraKit.h"

#include "CombatSystem.h"
#include "DoorwaysAnimatedCharacterAsset.h"
#include "DoorwaysAnimationSampler.h"
#include "DoorwaysCharacterPresentation.h"
#include "DoorwaysGame.h"
#include "DoorwaysGamepad.h"
#include "DoorwaysRuntimePathResolver.h"
#include "DoorwaysWalkableSurfaceQuery.h"
#include "GameConstants.h"
#include "NyxaraClips.h"
#include "NyxaraHud.h"
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
#include <cmath>
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
        constexpr float AirDashSpeed = 12, AirDashSeconds = .55f;
        constexpr float SwordReach = 1.7f, JumpReach = 2.2f, SwordArc = FrontArc;
        constexpr int CutDamage = 45, LastDamage = 80, JumpDamage = 110;
        constexpr float SteerRate = 90.f;    // degrees a second A and D turn her (owner, 2026-11-18: A/D turn her slowly round, the view stays where the mouse puts it)
        constexpr float BackSpeed = 1.25f;   // metres a second stepping backwards
        // The Jump clip (Mixamo's Running Jump as build_from_mixamo.py leaves it, 0.9 s), measured from the clip: she
        // leaves the ground at its start, is at the top at JumpApex, her first foot is down again at JumpLand; falling
        // faster than she jumped she holds the pose at JumpHold (her leg reaching down). After JumpLand: her step out.
        constexpr float JumpApex = .333f, JumpHold = .575f, JumpLand = .617f;
        constexpr float DropSeconds = .45f;  // held that long in the reaching pose, a drop becomes a fall (the Fall clip)
        constexpr float SlideFrom = .25f, SlideTo = .95f, SlideEvery = .22f, SlideReach = 1.7f, SlideArc = FrontArc;
        constexpr int SlideDamage = 35;
        constexpr float LiftFloor = .03f;    // m: a move that lowers her picture lets her lowest joint down no nearer the ground than this
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
        constexpr float CastRange = 7, CastRate = 1.7f, CastSwordSpeed = 26, CastWait = .22f, CastDash = 12, CastTake = .15f;
        constexpr float CastDropSeconds = .7f;
        constexpr float CastSpin = 2.6f, CastAirEvery = .012f, CastAirRound = .8f, CastAirLife = .3f;
        constexpr float CastFallSeconds = .35f, CastDownSeconds = .9f, CastRiseSeconds = .6f, CastFallen = 1.45f;
        constexpr float CastThrust = .43f, CastHopSpeed = 4.5f, CastHopSeconds = .45f, CastHopUp = .3f, CastBrace = .1f;
        constexpr int CastKick = 40;
        constexpr float CastLean = 0.f, CastLift = .1f, CastHold = .22f;
        constexpr float CastMiddle = .55f, CastLowest = .35f, CastHandsAt = 1.25f, CastStoop = 1.1f, CastStoopMost = .8f;
        constexpr int CastDamage = 90, CastPull = 60;
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
        constexpr float GuardMostSeconds = 5.f;               // the barrier stays up at most this long
        constexpr float GoneSeconds = .5f;                    // a creature that died is held this long (the journey frees its slot after .55 s)
        constexpr float CutPush = 1.5f;                       // m/s a cut knocks a creature back
        constexpr int HeavyBlow = 80;                         // from this much damage a blow freezes the moment

        XMFLOAT3 Add(XMFLOAT3 a, XMFLOAT3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
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
        float AttackTime = 0, SkillStrike = 0, SkillReach = 1;
        int Step = 0, Queued = 0;          // place in the combo; the button pressed during a cut (1 left, 2 right)
        int RowAt[2] = {0, 0}, LastHand = 0; bool RowUsed[2] = {false, false};   // each hand's place in its row of cuts; the hand that cut last; whether each has cut in this combo
        bool Jumping = false, Back = false, Dead = false;
        float JumpTime = 0, DeathTime = 0;
        bool JumpReal = false, JumpAir = false, WasGrounded = true;   // a jump that the world moves her in; she has left the ground in it
        float JumpClock = 0, AirSeconds = 0, Dropped = 0, FellAt = 0; // seconds of this jump; off the ground on foot; held reaching down; her fall a frame ago (m/s)
        float Squat = 0, SquatGo = 0;      // her knees taking a landing: her hips let down (m), and how fast
        bool Struck = false;               // something hurt her since her last frame
        float Flinch = 0, FlinchGo = 0;    // knocked back by it: her picture tipped back (radians), and how fast
        float FlyTime = 0, FlyLean = 0, Hover = 0, Rise = -1, Touch = -1;
        bool WasFlying = false;
        int Air = 0; float AirTime = 0;    // F in flight: 1 the dart forward

        // ---- the feather storm (C)
        std::vector<float> StormTurned;                    // how far she has turned in it (radians), every 1/120 s of the clip
        float StormRateMost = 1;                           // her fastest turn in it (radians a second of the clip)
        float StormVolleyAt[3] = {1e9f, 1e9f, 1e9f};       // when she is half-way round each time (s of the clip): feathers at every creature near
        int StormVolley = 0; float StormFeathers = 0;      // the next of those; feathers owed (thrown as whole ones)
        int StormWing = 0;                                 // the wing that threw the last

        // ---- the thrown swords (V)
        XMFLOAT3 SwordFrom[2]{}, SwordFromWay[2]{}, SwordTo[2]{}, SwordToWay[2]{};
        bool SwordsOut = false, SwordCapture = false;
        float SwordFlight = 0, SwordBack = 0;
        int Cast = 0;                      // 0 no; 1 the throw's wind-up, 2 the swords fly, 3 they stick, 4 she goes after them, 5 she takes them out
        float CastTime = 0, CastStand = 1, CastUp = 0, CastGround = 0;
        int CastTarget = -1;
        XMFLOAT3 CastAt{};
        int Pushed = -1;                   // the creature her legs pushed over
        float PushTime = 0, PushStick = 1, PushGone = -1;
        XMFLOAT3 PushWay{0, 0, 1}, PushFeet{};
        float CastWay = 1, CastTurns = 1, CastCome = 0, CastTwist = 0, CastTwistWas = 0;
        std::string WindUpsEvened, WindUpsLeft;   // the cuts whose wind-up EvenWindUps made one move, and left (for the log)
        float CastRise = 0, CastRiseNow = 0, CastAir = CastAirHeight, CastMissing = 0;
        bool CastStruck = false, CastKicked = false;

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
        bool Carried() const { return Attack == "Attack_Jump" || Skill(); }
        bool Moving() const { return !Attack.empty() || Cast != 0 || Air != 0 || Rise >= 0 || Touch >= 0; }
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
              << '\t' << LiftNow << '\t' << FlyLean << '\t' << FeetDrop << '\t' << FeetRaise[0] << '\t' << FeetRaise[1] << '\t' << SoleGap[0] << '\t' << SoleGap[1] << '\n';
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
        // wind-up is calm already (at most WindUpCalm of its blow's speed) is left as it is.
        void EvenWindUps()
        {
            constexpr float WindUpFrom = .45f, WindUpCalm = .45f, Tick = 1 / 240.f, KeysEvery = 1 / 120.f;
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
                if (!InHand || SwordsOut) continue;
                // her sword: its own turn kept, the middle of its handle where the middle of her fist now is
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
            Attack = name;
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
            if (Air) return Air == 1 ? AirDashSpeed : CastDash;
            if (Cast == 4) return world.MainPlayer.TraversalMode == DoorwaysPlayerTraversalMode::Flight ? 0.f : CastDash;
            if (Cast == 5) return CastTime >= CastThrust && CastTime < CastThrust + CastHopSeconds ? -CastHopSpeed : 0.f;
            const auto found = NyxaraClips::Attacks.find(Attack);
            if (found == NyxaraClips::Attacks.end() || found->second.Forward.size() < 2) return 0;
            const auto& path = found->second.Forward;
            const size_t i = (std::min)(static_cast<size_t>((std::max)(0.f, AttackTime) * 30), path.size() - 2);
            return (path[i + 1] - path[i]) * 30 * AttackRate * (Skill() ? SkillReach : 1.f);
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
            for (const auto& wing : WingJoint) for (const int j : wing) if (j < 0) return;
            if (dt <= 1e-5f || out.ModelSpaceMatrices.size() != out.LocalJoints.size()) return;
            WingShare += (share - WingShare) * (std::min)(1.f, dt * 6);
            float blow = WingKnown ? climb - WingClimb : 0.f;      // her rise changed at once: she jumped off, or landed
            if (std::abs(blow) < 3) blow = 0;
            WingClimb = climb; WingKnown = true;
            const float flapTo = WingShare * (climb < 0 ? (std::min)(.45f, -climb * .03f) : -(std::min)(.14f, climb * .015f));
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
                const float backTo = WingShare * std::clamp(-side * turn * .13f, -.3f, .3f);
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
                const float flaps[3] = {w.Flap + WingFold * FoldFlap[0], w.Flap2 * .5f + WingFold * FoldFlap[1], w.Flap3 * .4f + WingFold * FoldFlap[2]};
                const float backs[3] = {w.Back + WingFold * FoldBack[0], w.Back2 * .45f + WingFold * FoldBack[1], w.Back3 * .35f + WingFold * FoldBack[2]};
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
            const XMMATRIX place = XMMatrixRotationY(p.Yaw) * XMMatrixTranslation(p.Position.x, p.Position.y, p.Position.z);
            const XMMATRIX model = XMMatrixInverse(nullptr, place);
            for (int k = 0; k < 2; ++k)
            {
                const int j = SwordJoint[k], parent = Skeleton.Joints[j].ParentIndex;
                if (parent < 0) continue;
                const XMVECTOR grip = XMLoadFloat3(&SwordGrip[k]), tip = XMLoadFloat3(&SwordTip[k]);
                const XMMATRIX held = XMLoadFloat4x4(&out.ModelSpaceMatrices[j]) * place;
                const XMVECTOR handAt = XMVector3TransformCoord(grip, held);
                const XMVECTOR handWay = XMVector3Normalize(XMVector3TransformCoord(tip, held) - handAt);
                if (SwordCapture)
                {
                    XMStoreFloat3(&SwordFrom[k], handAt);
                    XMStoreFloat3(&SwordFromWay[k], handWay);
                }
                XMVECTOR at = XMVectorLerp(XMLoadFloat3(&SwordFrom[k]), XMLoadFloat3(&SwordTo[k]), SwordFlight);
                XMVECTOR way = XMVectorLerp(XMLoadFloat3(&SwordFromWay[k]), XMLoadFloat3(&SwordToWay[k]), (std::min)(1.f, SwordFlight * 4));
                at = XMVectorLerp(at, handAt, SwordBack);
                way = XMVector3Normalize(XMVectorLerp(XMVector3Normalize(way), handWay, SwordBack));
                const XMMATRIX above = XMLoadFloat4x4(&out.ModelSpaceMatrices[parent]);
                XMVECTOR aboveScale, aboveTurn, aboveAt;
                if (!XMMatrixDecompose(&aboveScale, &aboveTurn, &aboveAt, above)) continue;
                const XMVECTOR wantWay = XMVector3Normalize(XMVector3TransformNormal(way, model));
                const XMVECTOR wantAt = XMVector3TransformCoord(at, model);
                XMMATRIX m = XMMatrixScalingFromVector(aboveScale) * XMMatrixRotationQuaternion(ArcTurn(XMVector3Normalize(tip - grip), wantWay));
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
                const float raise = (FeetRaise[k] + Squat) / scale;      // (her knees taking a landing: her body is let down, her feet stay)
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
                SoleGap[k] = AnkleRest[k] > 1e8f || !std::isfinite(g) ? std::numeric_limits<float>::quiet_NaN() : XMVectorGetY(A) * scale + FeetDrop - Squat - AnkleRest[k] - g;
            }
            return FeetDrop - Squat;
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
            const float swing = XMConvertToRadians(armed ? 4 + 4 * GaitRun : 13 + 6 * GaitRun), bent = armed ? 0.f : XMConvertToRadians(3 + 8 * GaitRun) * moving;
            if (!was) ArmLag = 0;
            ArmLag += (std::clamp(-Gain * .012f, -.14f, .14f) - ArmLag) * (std::min)(1.f, dt * 6);
            const float behind = armed ? ArmLag * .5f : ArmLag - XMConvertToRadians(2 + 7 * GaitRun) * moving;
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
                const float most = armed ? arm + XMConvertToRadians(12) : arm + XMConvertToRadians(75) - (tilt2 - tilt1);
                if (ForeAt[s] < least) { ForeAt[s] = least; ForeGo[s] = (std::max)(ForeGo[s], 0.f); }
                if (ForeAt[s] > most) { ForeAt[s] = most; ForeGo[s] = (std::min)(ForeGo[s], 0.f); }
                const XMVECTOR want1 = XMVectorSet(r1.x, -across1 * std::cos(tilt1 + arm), across1 * std::sin(tilt1 + arm), 0);
                const XMVECTOR want2 = XMVectorSet(r2.x, -across2 * std::cos(tilt2 + ForeAt[s]), across2 * std::sin(tilt2 + ForeAt[s]), 0);
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
            const bool same = name == LastClip;
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
                const bool fromSkill = LastClip.rfind("Skill", 0) == 0 || LastClip == "Cast_Pull" || LastClip == "Attack_Jump";
                if (!Drawing && (fromSkill || LastClip.rfind("Attack", 0) == 0) && (name == "Idle" || name == "Stand" || name == "Move"))
                {
                    if (name == "Move") UpperOut = fromSkill ? SkillOut : CutOut;
                    else BlendSeconds = fromSkill ? SkillOut : CutOut;
                }
                // (into the feather storm or the cast her body goes a little longer, slow away and slow in: from her guard
                // her arms were flung out to the old spin's in a fifth of a second, her hands at 7 to 8 m/s in its first frames)
                const bool skillIn = !Drawing && (name == "Skill_Storm" || name == "Skill_Cast");
                if (skillIn) BlendSeconds = SkillIn;
                if (Drawing)
                {   // Her arms have their own time: up to the handles, and on to the new clip. A cut out of standing at
                    // ease took her hand from her hip to the handle over her shoulder in an eighth of a second (20 m/s at
                    // its fastest: a whip, not an arm). Now the way up takes the longer part and the cut comes down from
                    // the handle; unhurried when nothing presses, and slowest putting them away to stand at ease.
                    const bool pressed = name.rfind("Attack", 0) == 0 || name.rfind("Skill", 0) == 0 || name.rfind("Cast", 0) == 0;
                    const bool cut = name.rfind("Attack", 0) == 0 && name != "Attack_Jump";
                    // (Putting them away the blade is taken back over her shoulder: that is given its time.)
                    const bool putting = InHand, atEase = name == "Stand";
                    BlendSeconds = cut ? DrawReachPressed + DrawOnCut : pressed ? .26f : .36f;   // (into a cut her body goes with her arms: there just before the blow)
                    DrawReach = pressed ? DrawReachPressed : putting ? (atEase ? .42f : .32f) : .26f;
                    DrawSeconds = DrawReach + (cut ? DrawOnCut : pressed ? DrawOnPressed : putting ? (atEase ? .34f : .28f) : .30f);
                    Reaching[0] = Reaching[1] = ArmState{};
                }
                LastClip = name;
                // what is carried on: for every joint, the old pose against the new one, and how the old one was moving
                // (no faster than would cross that difference three times in the time of the change: no overshoot of note)
                const size_t n = out.LocalJoints.size();
                CarrySmooth = (name.rfind("Attack", 0) == 0 && name != "Attack_Jump" && (CutBlend > 0 || Drawing)) || skillIn;
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
        char log[MAX_PATH] = {};
        if (GetEnvironmentVariableA("CAUCASIAN_EAGLE_NYXARA_LOG", log, sizeof(log)) > 0 && log[0])
        {
            b.LogPath = log;
            std::ofstream(b.LogPath) << "seconds\tevent\tclip\tspeed\tx\ty\tz\tlift\tlean\tfeet_drop\tleft_raise\tright_raise\tleft_gap\tright_gap\n";
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
        if (!BuildRing(renderer, mRingMesh, error) || !BuildQuad(renderer, mQuadMesh, error, -.5f) || !BuildQuad(renderer, mFeatherMesh, error, 0) ||
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
        if (rel.x * facing.x + rel.z * facing.z < -.3f) return false;
        mBlockFlash = .25f;
        const XMFLOAT3 at = Add(world.MainPlayer.Position, Add(Mul(facing, 1.2f), {0, 1.2f, 0}));
        Burst(at, MoonWhite, 14, 4.f, .2f); Burst(at, SoulGreen, 8, 3.f, .18f);
        mNotice = L"BLOCKED"; mNoticeTimer = .5f;
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

    bool NyxaraKit::Busy() const { return mBody->Moving() || mGuarding; }

    NyxaraKit::Keys NyxaraKit::ReadKeys(bool inputAllowed) const
    {
        Keys k;
        if (mInputBlocked) return k;
        if (GameSession().Unattended && mAutoKeys) return *mAutoKeys; // the journey's test pilot
        if (GameSession().Unattended)
        {   // CAUCASIAN_EAGLE_NYXARA_KEYS="<seconds>:<key>,...": each key is down for a tenth of a second from that moment
            // of her clock (L, R the mouse buttons, F, V, C, E, J for Space). Never reads the desktop.
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
                    default: break;
                    }
            return k;
        }
        if (!inputAllowed) return k;
        const auto down = [](int vk) { return IsDoorwaysKeyDown(vk); };   // the key, or the controller button bound to it
        k.Lmb = down(VK_LBUTTON); k.Rmb = down(VK_RBUTTON); k.E = down('E'); k.K1 = down('F'); k.K2 = down('V'); k.K3 = down('C');
        k.Shift = down(VK_SHIFT); k.Space = down(VK_SPACE);
        return k;
    }

    float NyxaraKit::ScaleFrameTime(float realDeltaTime, bool inputAllowed)
    {
        (void)inputAllowed;
        if (!mReady) return realDeltaTime;
        if (mHitStop > 0) { mHitStop -= realDeltaTime; return realDeltaTime * .06f; }   // the moment a heavy blow lands
        return realDeltaTime;
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

    void NyxaraKit::DamageWraith(int index, int damage, const XMFLOAT3& from, float push, float launch)
    {
        auto& w = mWraiths[index];
        if (!w.Alive) return;
        damage = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(damage) * mDamageScale * w.Weak)));
        if (!mTrainingMode) w.Health -= damage;
        else ++mTrainingTargetHits;
        w.Flash = .16f;
        XMFLOAT3 away = Sub(w.Pos, from); away.y = 0; away = Norm(away);
        if (!w.Pinned && w.Hold <= 0)
        {
            w.Push = Add(w.Push, Mul(away, push * w.PushScale));
            if (launch > 0 && w.PushScale > .5f) w.LiftVel = (std::max)(w.LiftVel, launch);
        }
        const float heavy = std::clamp(static_cast<float>(damage) / 120.f, 0.f, 1.f);
        mShake = (std::max)(mShake, .02f + .08f * heavy);
        if (damage >= HeavyBlow) mHitStop = (std::max)(mHitStop, .04f);
        CaucasianEagle::Sound::Play("nyxara/cut", .85f + .15f * heavy, .08f);   // the 'meat heavy' the user chose in the lab
        if (w.Health > 0 && Random() < .4f) CaucasianEagle::Sound::Play("creatures/hurt", .6f, .15f);
        if (w.Health <= 0)
        {
            w.Alive = false; w.Respawn = 8.f; w.Health = 0;
            Burst(w.Pos, ShadeViolet, 26, 6.f, .35f);
            Fx ring; ring.Kind = FxKind::Ring; ring.Pos = {w.Pos.x, w.Pos.y - w.Lift - 1.2f, w.Pos.z}; ring.Size = 2.2f;
            ring.Life = .6f; ring.Color = ShadeViolet; mFx.push_back(ring);
        }
    }

    void NyxaraKit::Update(WorldState& world, WorldRenderer& worldRenderer, CombatSystem& combat, float dt, float realDt, bool inputAllowed)
    {
        (void)worldRenderer;
        if (!mReady) return;
        auto& b = *mBody;
        auto& p = world.MainPlayer;
        auto& s = GameSession();
        b.Clock += dt;
        const bool unattended = s.Unattended;
        const bool live = inputAllowed && !mInputBlocked && !unattended;   // the desktop's own keys (steering, stepping back)
        const Keys k = ReadKeys(inputAllowed);
        const auto pressed = [](bool now, bool before) { return now && !before; };
        const bool clickL = pressed(k.Lmb, mPrev.Lmb), clickR = pressed(k.Rmb, mPrev.Rmb), keyF = pressed(k.K1, mPrev.K1),
                   keyV = pressed(k.K2, mPrev.K2), keyC = pressed(k.K3, mPrev.K3);
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
                mFlightEnergy = (std::max)(0.f, mFlightEnergy - dt / (tried > 0 ? tried : FlightEnergySeconds));
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
                const bool can = holds && alive && !b.Dead && !mFalling && !b.Cast && !b.Carried() && !mGuarding;
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
        for (auto& w : mWraiths) { w.Pinned = false; w.Hold = 0; }   // set again below for what she holds this frame

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
        const auto strike = [&](float reach, float arc, int damage, float push)
        {
            int struck = 0;
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
                b.Wound(f.Feet, f.Height, f.Radius, p.Position, sideOf(), true, damage);
                DamageWraith(i, damage, p.Position, push, 0);
                ++struck;
            }
            if (struck) b.Log(world, ("hit " + std::to_string(struck)).c_str());
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
        // ---- dead: she falls and lies; everything she was doing ends
        if (!alive && !b.Dead)
        {
            b.Dead = true; b.DeathTime = 0; b.Attack.clear(); b.Queued = 0; b.Jumping = false; b.Air = 0; b.Rise = b.Touch = -1;
            b.Cast = 0; b.SwordsOut = false; b.Pushed = -1; mGuarding = false;
            b.Log(world, "death");
        }
        else if (alive && b.Dead) { b.Dead = false; b.Log(world, "alive"); }
        if (b.Dead) b.DeathTime += dt;

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
        const bool busy = b.Rise >= 0 || b.Touch >= 0 || b.Air != 0 || b.Dead;   // leaving the ground or coming down: nothing else

        // ---- the barrier: E held, standing her ground
        const bool canGuard = alive && !b.Moving() && !b.Jumping && !busy;
        if (k.E && canGuard && (mGuarding || mGuardCooldown <= 0) && mGuardHeld < GuardMostSeconds)
        {
            if (!mGuarding) { CaucasianEagle::Sound::Play("nyxara/guard", .7f); mNotice = L"SWORD BARRIER"; mNoticeTimer = .8f; }
            mGuarding = true; mGuardHeld += dt;
        }
        else
        {
            if (mGuarding) mGuardCooldown = CaucasianEagle::GuardCooldownSeconds;
            mGuarding = false;
            if (!k.E) mGuardHeld = 0;
        }

        // ---- Space: the jump clip once (a picture of a jump, as in the lab: the engine's jump is off on foot)
        if (space && !flying && !busy && !b.Cast && !b.Jumping && !b.Carried() && !mGuarding && alive)
        {
            b.Attack.clear(); b.Queued = 0; b.Jumping = true; b.JumpTime = 0;
            b.JumpReal = false; b.JumpAir = false; b.JumpClock = 0; b.Dropped = 0;
            b.Log(world, "jump");
            // a real jump where the game asks for one: from the ground, or from the brink of an edge she stands on
            if (mRealJump > 0 && p.VerticalVelocity <= 0 &&
                (p.IsGrounded || p.GroundState == Doorways::DoorwaysPhysicsGroundState::UnsupportedAirPrepared))
            {
                p.VerticalVelocity = mRealJump; p.IsGrounded = false;
                p.GroundState = Doorways::DoorwaysPhysicsGroundState::Airborne;
                p.MissingAuthoritativeGround = false; p.GroundY = p.TargetGroundY = p.Position.y;
                b.JumpReal = true;
            }
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
            if (!b.Jumping && !busy && !b.Cast && !b.Carried() && b.Attack.empty() && !mFalling && !mGuarding && b.Duration("Jump") > JumpLand &&
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

        // ---- the mouse: the left sword and the right one; clicks in a row go through the cuts
        const bool free = alive && !busy && !b.Cast && !mGuarding;
        if (free && (clickL || clickR)) b.Queued = clickL ? 1 : 2;
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
                if (!mWraiths[i].Alive) continue;
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
                b.CastRise = high - CastHandsAt;
                b.CastAir = std::clamp(CastAirHeight + b.CastRise, .2f, 40.f);
                for (int k2 = 0; k2 < 2; ++k2)
                {
                    const float length = XMVectorGetX(XMVector3Length(XMLoadFloat3(&b.SwordTip[k2]) - XMLoadFloat3(&b.SwordGrip[k2])));
                    const float side = k2 == 0 ? -.16f : .16f;   // across her line
                    const XMFLOAT3 point{f.Feet.x + tx * f.Radius * .3f - tz * side, f.Feet.y + high + (k2 ? .12f : 0.f), f.Feet.z + tz * f.Radius * .3f + tx * side};
                    b.SwordToWay[k2] = {-tx, -.08f, -tz};
                    b.SwordTo[k2] = {point.x + tx * length, point.y + .08f * length, point.z + tz * length};
                    b.CastStand = f.Radius * .3f + length + .5f;   // where her hands reach the handles
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
            {   // F in flight: the dart forward
                if (!cooling(0))
                {
                    b.Air = 1; b.AirTime = 0; b.SkillStrike = .1f;
                    mSkillCooldown[0] = SkillCooldown[0] * mCooldownScale;
                    CaucasianEagle::Sound::Play("nyxara/dash", .85f);
                    b.Log(world, "air dart");
                }
            }
            else if (keyF)
            {   // F: the slide (2026-11-19: no longer the finishing throw at a broken creature; the owner: "remove f finisher
                // from nyxara we dont need it anymore")
                if (!cooling(0))
                {
                    b.SkillReach = 1;
                    b.StartAttack(world, "Skill_Slide"); b.SkillStrike = SlideFrom;
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
        if (b.Skill()) b.Queued = 0;
        if (b.Queued && free)
        {
            if (b.Jumping) b.StartAttack(world, "Attack_Jump", b.JumpTime);
            else if (b.Attack.empty()) { b.Step = 0; b.StartAttack(world, b.NextCut(b.Queued)); }
        }
        else if (!free) b.Queued = 0;

        // ---- F in flight
        if (b.Air)
        {
            b.AirTime += dt;
            b.Queued = 0;
            if (!flying) b.Air = 0;
            else if (b.Air == 1)
            {
                if (b.AirTime >= b.SkillStrike) { b.SkillStrike += SlideEvery; strike(SlideReach, SlideArc, SlideDamage, CutPush); }
                if (b.AirTime >= AirDashSeconds) b.Air = 0;
            }
        }

        // ---- V: the thrown swords
        if (b.Cast)
        {
            if (b.CastTarget >= 0 && b.CastTarget < static_cast<int>(mWraiths.size()) && mWraiths[b.CastTarget].Alive && b.Cast < 5)
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
            {
                b.Attack.clear();
                b.Cast = 4; b.CastTime = 0;
                b.CastWay = (std::max)(.05f, away - b.CastStand - .05f);   // how many turns she makes on her way
                b.CastTurns = (std::max)(1.f, std::round(b.CastWay / CastDash * CastSpin * 1.5f));
                b.CastTwist = b.CastTwistWas = b.CastCome = 0;
                CaucasianEagle::Sound::Play("nyxara/wing", .8f);
                b.Log(world, ("cast twist " + std::to_string(static_cast<int>(b.CastTurns)) + " turns over " + std::to_string(b.CastWay) + " m").c_str());
            }
            if (b.Cast == 4)
            {   // the twist, by how far she has come (it never goes back); it gathers speed over the first third of her way
                // and the last turn slows into the grab (2026-11-19: it began at its fastest, 6.8 turns a second out of
                // standing still, and her hands went from rest to 27 m/s in one frame)
                const float come = std::clamp(1 - (std::max)(0.f, away - b.CastStand - .05f) / b.CastWay, 0.f, 1.f), left = 1 - come;
                b.CastTwistWas = b.CastTwist;
                b.CastTwist = (std::max)(b.CastTwist, 1 - left * left * left * (1 + 3 * come));
                b.CastCome = (std::max)(b.CastCome, come);
            }
            if (b.Cast == 4 && dt > 1e-4f && away > .05f)
            {   // the air she turns: streaks on a circle round her, going round as she does
                const float wx = dx / away, wz = dz / away;
                const float from = XM_2PI * b.CastTurns * b.CastTwistWas, to = XM_2PI * b.CastTurns * b.CastTwist;
                const int count = (std::max)(1, static_cast<int>(dt / CastAirEvery));
                for (int n = 0; n < count; ++n)
                    for (int arm = 0; arm < 2; ++arm)
                    {
                        const float a = from + (to - from) * static_cast<float>(n + 1) / static_cast<float>(count) + static_cast<float>(arm) * XM_PI;
                        const float c = std::cos(a), sn = std::sin(a);
                        const float round = CastAirRound * b.Rand(.8f, 1.1f), back = b.Rand(-.5f, .5f);
                        b.Marks.push_back({{p.Position.x + wz * sn * round + wx * back, p.Position.y + b.LiftNow + .95f + c * round, p.Position.z - wx * sn * round + wz * back},
                                           {wz * c * 3, -sn * 3, -wx * c * 3}, CastAirLife, CastAirLife, .016f, 0, 4});
                    }
            }
            if (b.Cast == 4 && flying && dt > 0)
            {   // in the air she is moved here: forward to her swords and down to them
                const float stepped = (std::min)(CastDash * dt, (std::max)(0.f, away - b.CastStand));
                p.Position.x += std::sin(p.Yaw) * stepped;
                p.Position.z += std::cos(p.Yaw) * stepped;
                p.Position.y += std::clamp(b.CastGround + b.CastAir - p.Position.y, -CastAirSink * dt, CastAirSink * dt);
                p.VerticalVelocity = 0;
            }
            if (b.Cast == 4 && ((away <= b.CastStand + .05f && (!flying || std::abs(p.Position.y - b.CastGround - b.CastAir) < .3f)) || b.CastTime > 4))
            {
                b.Cast = 5; b.CastTime = 0; b.CastKicked = false;
                for (int n = 0; n < 28; ++n)
                {   // her wings open: the turned air is thrown out in a ring
                    const float a = XM_2PI * static_cast<float>(n) / 28, c = std::cos(a), sn = std::sin(a), wx = std::sin(p.Yaw), wz = std::cos(p.Yaw);
                    b.Marks.push_back({{p.Position.x + wz * sn * .5f, p.Position.y + b.LiftNow + .95f + c * .5f, p.Position.z - wx * sn * .5f},
                                       {wz * sn * 5, c * 5, -wx * sn * 5}, .35f, .35f, .02f, 0, 4});
                }
                b.Log(world, ("cast reached twist " + std::to_string(b.CastTurns * b.CastTwist) + " of " + std::to_string(b.CastTurns)).c_str());
            }
            else if (b.Cast == 5)
            {
                b.SwordBack = std::clamp((b.CastTime - CastHold) / CastTake, 0.f, 1.f);
                if (b.SwordBack >= 1 && !b.CastStruck)
                {   // her hands are on them: out they come
                    b.CastStruck = true; b.SwordsOut = false;
                    strikeOne(b.CastTarget, CastPull);
                    b.Log(world, "cast pulled");
                }
                if (b.CastTime >= CastThrust && !b.CastKicked)
                {
                    b.CastKicked = true;
                    const int target = b.CastTarget;
                    strikeOne(target, CastKick);
                    b.Log(world, "cast kicked");
                    if (target >= 0 && mWraiths[target].PushScale > .5f)
                    {   // pushed over backwards, away from her
                        b.Pushed = target; b.PushTime = 0; b.PushGone = -1;
                        b.PushFeet = b.CastAt;
                        const float ax = b.CastAt.x - p.Position.x, az = b.CastAt.z - p.Position.z, d = (std::max)(.01f, std::hypot(ax, az));
                        b.PushWay = {ax / d, 0, az / d};
                    }
                }
                if (b.CastTime >= (std::max)(b.Duration("Cast_Pull"), CastThrust + CastHopSeconds))
                {
                    b.SwordsOut = false; b.Cast = 0; b.CastTarget = -1;
                    b.Log(world, "cast over");
                }
            }
        }
        else if (b.Skill())
        {
            const float was = b.AttackTime;
            b.AttackTime += dt * AttackRate;
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
                        if (!mWraiths[i].Alive) continue;
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
                else
                {
                    const bool slide = b.Attack == "Skill_Slide";
                    if (slide) strike(SlideReach, SlideArc, SlideDamage, CutPush);
                    b.SkillStrike = slide && b.SkillStrike + SlideEvery <= SlideTo ? b.SkillStrike + SlideEvery : 1e9f;
                }
            }
            if (b.AttackTime >= b.Duration(b.Attack)) b.Attack.clear();
        }
        else if (!b.Attack.empty())
        {
            const float was = b.AttackTime;
            b.AttackTime += dt * AttackRate;
            const float hit = Body::Hit(b.Attack);
            if (was < hit && b.AttackTime >= hit)
            {   // the blow lands: every creature in reach before her is struck
                const bool leap = b.Attack == "Attack_Jump";
                const bool last = b.Attack == LeftChain[std::size(LeftChain) - 1] || b.Attack == RightChain[std::size(RightChain) - 1];
                strike(leap ? JumpReach : SwordReach, SwordArc, leap ? JumpDamage : last ? LastDamage : CutDamage, CutPush);
                CaucasianEagle::Sound::Play("nyxara/swing", .5f, .15f);
            }
            if (b.Queued && b.AttackTime >= hit + ChainAfter)
            {
                const char* next = b.NextCut(b.Queued);
                b.Step = b.Attack == "Attack_Jump" ? 0 : b.Step + 1;
                b.StartAttack(world, next);
            }
            else if (b.AttackTime >= (std::min)(b.Duration(b.Attack), hit + AttackTail)) b.Attack.clear();
        }

        // ---- the creatures she holds: pushed over by V, thrown by F
        // her picture's lift: up in the feather storm (and as the cast has it, below)
        float lift = b.Attack == "Skill_Storm" ? Body::StormLift(b.AttackTime) : 0.f;
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
        const bool braced = b.Cast == 5 && b.CastTime < CastThrust;
        b.Speed = std::hypot(p.ActualVelocity.x, p.ActualVelocity.z);
        const float lean = b.Air ? FlyLeanFast : braced ? (std::min)(CastStoopMost, CastStoop * b.CastMissing) : b.Cast == 4 ? CastLean
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
            const bool afoot = !flying && p.IsGrounded && !b.Dead && !b.Carried() && !b.Cast && b.Rise < 0 && b.Touch < 0;
            const float roll = afoot ? std::clamp(-b.TurnRate * b.Speed * .035f, -.22f, .22f) : 0.f;
            const float pitch = afoot ? std::clamp(b.Gain * .010f, -.05f, .09f) + b.Squat * .45f : 0.f;   // (and she bows a little into a landing)
            {   // Struck, she is knocked back a little and comes upright again (2026-11-19: a blow took her health and
                // did nothing else to her), and her wings start up (below, with her guard).
                constexpr float Spring = 13.f;
                if (b.Struck && !b.Dead) b.FlinchGo -= 4.5f;
                for (float left = dt; left > 1e-6f;)
                {
                    const float h = (std::min)(left, 1 / 120.f); left -= h;
                    b.FlinchGo += (-Spring * Spring * b.Flinch - 2 * Spring * b.FlinchGo) * h;
                    b.Flinch += b.FlinchGo * h;
                }
                if (b.Dead || b.Cast) b.Flinch = b.FlinchGo = 0;
            }
            b.LeanRoll += (roll - b.LeanRoll) * (std::min)(1.f, dt * 5);
            b.LeanPitch += (pitch - b.LeanPitch) * (std::min)(1.f, dt * 7);
            // in flight she banks into a turn, as a bird does (2026-11-19: she turned in the air upright, as on a spit)
            const bool winging = flying && !p.IsGrounded && !b.Dead && !b.Cast && b.Rise < 0 && b.Touch < 0;
            b.Bank += ((winging ? std::clamp(-b.TurnRate * .24f, -.42f, .42f) : 0.f) - b.Bank) * (std::min)(1.f, dt * 3.5f);
        }
        world.AnimatedPlayerVisual.Rotation = {b.FlyLean + b.LeanPitch + b.Flinch, 0, b.LeanRoll + b.Bank};
        b.Hover += ((flying && p.IsGrounded && !landing && b.Rise < 0 ? 1.f : 0.f) - b.Hover) * (std::min)(1.f, dt * 3);
        const float up = b.Cast == 4 ? 1.f : b.Cast == 5 && b.CastTime < CastThrust && !flying ? CastBrace / CastLift : 0.f;
        b.CastUp += (up - b.CastUp) * (std::min)(1.f, dt * 7);
        if (b.Cast == 5 && b.CastTime >= CastThrust && b.CastTime < CastThrust + CastHopSeconds)   // her way back through the air
            lift += CastHopUp * std::sin(XM_PI * (b.CastTime - CastThrust) / CastHopSeconds);
        {   // V: up along her way to where the swords are in this creature, as far as the ground lets her (the rest she stoops),
            // and down again after she has pushed off, falling over CastDropSeconds
            const float full = flying ? b.CastRise - (b.CastAir - CastAirHeight) : b.CastRise;
            if (b.Cast == 4) b.CastRiseNow = full * b.CastCome * b.CastCome * (3 - 2 * b.CastCome);
            else if (b.Cast == 5)
            {
                const float t = std::clamp((b.CastTime - CastThrust) / CastDropSeconds, 0.f, 1.f);
                b.CastRiseNow = full * (1 - t * t);
            }
            else b.CastRiseNow += (0.f - b.CastRiseNow) * (std::min)(1.f, dt * 7);
            const float here = b.Cast ? p.Position.y - b.CastGround : flying ? (mHeightOverGround >= 0 ? mHeightOverGround : 100.f) : 0.f;
            const float wanted = b.CastUp * CastLift + b.CastRiseNow;
            const float given = (std::max)(wanted, (std::min)(0.f, LiftFloor - b.Lowest - here - lift));
            b.CastMissing = given - wanted;
            lift += given - b.CastUp * CastLift;
        }
        const float hover = b.Hover * FlyHover;
        b.LiftNow = lift + hover + b.CastUp * CastLift;
        world.AnimatedPlayerVisual.Translation = {0, b.LiftNow, 0};
        // The camera looks at her real place, and these lifts move only her picture: so it is told of them, smoothly
        // (in the feather storm it goes up with her by less than half: she leaps up in the picture, the ground does not drop away)
        const float shown = b.LiftNow - (b.Attack == "Skill_Storm" ? .6f * Body::StormLift(b.AttackTime) : 0.f);
        b.CameraLift += (shown - b.CameraLift) * (std::min)(1.f, dt * CameraFollowLift);
        s.CameraPivotHeight = b.PivotBase + b.CameraLift;

        // ---- travel (owner, 2026-11-18): A and D turn her round, on foot and in flight, and never the view (the mouse alone
        // turns it); W goes the way she faces and S steps back (the engine's MoveAlongFacing). A scripted run still steers
        // by the view, and she turns to where she goes.
        const bool carried = b.Carried();
        float impulse = !b.Dead && dt > 0 ? b.ForwardSpeed(world) : 0.f;
        s.ImpulseX = std::sin(p.Yaw) * impulse;
        s.ImpulseZ = std::cos(p.Yaw) * impulse;
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
        const bool rooted = carried || b.Rise >= 0 || b.Touch >= 0 || b.Cast != 0 || b.Air != 0 || b.Dead || mGuarding;
        if (steer && !carried && !b.Cast && !b.Dead)
            p.Yaw = std::remainder(p.Yaw + static_cast<float>(steer) * XMConvertToRadians(SteerRate) * dt, XM_2PI);
        b.Back = keyS && !keyW && !b.Dead && !flying;
        s.HoldFacing = true;
        s.MoveAlongFacing = true;
        if (s.Unattended && s.ScriptedMove && !played)
        {
            const float vx = p.ActualVelocity.x, vz = p.ActualVelocity.z;
            if (!b.Dead && !carried && !b.Cast && !b.Air && dt > 0 && std::hypot(vx, vz) > .15f)
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
            if (flying || rooted) { b.Throttle = flying ? 1.f : 0.f; b.Glide = {0, 0}; }
            else if (wants)
            {
                const float gliding = std::hypot(b.Glide.x, b.Glide.y) / (std::max)(.5f, b.FullSpeed);
                b.Throttle = std::clamp((std::max)({b.Throttle, StartShare, gliding}) + dt / GatherSeconds, 0.f, 1.f);
                b.Glide = {0, 0}; b.Went = {p.ActualVelocity.x, p.ActualVelocity.z};
                if (b.Throttle >= 1 && b.Speed > .5f) b.FullSpeed = b.Speed;
            }
            else
            {
                if (b.Throttle > 0)
                {
                    b.Glide = b.Went; b.Went = {0, 0};   // (the engine has stopped her already this frame)
                    float had = std::hypot(b.Glide.x, b.Glide.y);
                    if (had > CaucasianEagle::FootRunSpeed) { b.Glide.x *= CaucasianEagle::FootRunSpeed / had; b.Glide.y *= CaucasianEagle::FootRunSpeed / had; had = CaucasianEagle::FootRunSpeed; }
                    b.GlideLose = had / (.12f + .025f * had);
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
        p.MoveSpeed *= mSpeedScale;
        s.SlowWalkSpeed = rooted ? 0.f : WalkPace * (flying ? 1.f : b.Throttle > 0 ? b.Throttle : .3f);
        if (rooted && flying)
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
        const bool moving = (p.IsMoving && b.Speed > .08f) || std::hypot(b.Glide.x, b.Glide.y) > .08f;
        {   // On guard or at ease (2026-11-19). Idle is a fighter's guard, both swords up: she stood so everywhere, and
            // drew both swords every time she stopped walking. Now she stands on guard only while something threatens
            // her: she is fighting, or a creature is within GuardNear of her, and for GuardSeconds after. Otherwise she
            // stands at ease (Stand), her swords on her back, and walks off without having to put them away.
            constexpr float GuardSeconds = 5.f, GuardNear = 14.f;
            bool threat = !b.Attack.empty() || b.Cast != 0 || b.Struck;
            if (b.Struck) for (auto& w : b.WingSpring) w.FlapGo += 3.f;
            b.Struck = false;
            for (const auto& w : mWraiths)
                if (!threat && w.Alive && std::hypot(w.Pos.x - p.Position.x, w.Pos.z - p.Position.z) < GuardNear && std::abs(w.Pos.y - p.Position.y) < GuardNear) threat = true;
            // And while that lasts her swords stay in her hands as she goes, jumps and falls (Armed; owner, 2026-11-19:
            // "hands are not following the skills and combos properly"). Until this they went to her back with her first
            // step, and were drawn again for every cut and every skill made on the move: in a fight on the move her
            // hands were at her shoulders half the time, and every cut began there instead of at its own wind-up.
            b.Guard = threat ? GuardSeconds : (std::max)(0.f, b.Guard - dt);
            b.Armed = b.Guard > 0;
        }
        std::string clip = b.Guard > 0 || b.Duration("Stand") <= 0 ? "Idle" : "Stand";
        float time = b.Clock;
        const bool striding = moving && !b.Dead && !flying;
        {   // how steeply she climbs (rise over ground covered, from her own height), for the stair clip
            const float rise = dt > 1e-4f && b.LastY > -1e8f ? (p.Position.y - b.LastY) / dt : 0.f;
            const float grade = b.Speed > .2f && !flying ? rise / b.Speed : 0.f;
            b.Climb += (grade - b.Climb) * (std::min)(1.f, dt * 6);
            b.ClimbRate += ((std::max)(0.f, rise) - b.ClimbRate) * (std::min)(1.f, dt * 6);
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
            else b.Stride((std::max)(b.Speed, std::hypot(b.Glide.x, b.Glide.y)), b.Back, dt);   // (the frame the key is let go the engine has her at rest; she is carried on from the next)
        }
        {   // Turning on the spot she steps (2026-11-19: she was turned as a statue on a plate). Her legs are the walk's,
            // stepping in place (her stride drawn in to nothing), her body goes on standing.
            const bool turning = !moving && !b.Dead && !flying && p.IsGrounded && b.Attack.empty() && !b.Cast && !b.Jumping && !carried &&
                                 b.Rise < 0 && b.Touch < 0 && std::abs(b.TurnRate) > .5f;
            if (turning) { b.Gait = "Move"; b.Stride(0, false, dt); }
            else if (!striding) b.StrideScale += (1 - b.StrideScale) * (std::min)(1.f, dt * 10);   // (standing: her feet where her clip has them)
            b.TurnSteps += ((turning ? 1.f : 0.f) - b.TurnSteps) * (std::min)(1.f, dt * (turning ? 10.f : 6.f));
        }
        const bool cutting = !b.Attack.empty() && !carried && !b.Dead;
        if (!cutting) b.Legs = 0;
        else if (b.LastClip.rfind("Attack", 0) != 0) b.Legs = striding ? 1.f : 0.f;
        else b.Legs = std::clamp(b.Legs + (striding ? 1.f : -1.f) * dt / .15f, 0.f, 1.f);
        b.FlyBody = (cutting || (b.Cast >= 1 && b.Cast <= 3)) && flying;   // a cut in the air: her legs, hips and wings go on flying
        if (b.FlyBody) b.Legs = b.Sample("Fly", b.FlyTime, b.LegsPose, nullptr) ? 1.f : 0.f;
        else if (b.Legs > 0 && !b.Sample(b.Gait, b.GaitTime(), b.LegsPose, nullptr)) b.Legs = 0;
        if (b.Dead) { clip = "Death"; time = b.DeathTime; }
        else if (b.Rise >= 0) { clip = "Fly_Rise"; time = b.Rise; }
        else if (b.Touch >= 0) { clip = "Fly_Land"; time = b.Touch; }
        else if (mFalling && b.Attack.empty() && !b.Cast && b.Duration("Fall") > 0) { clip = "Fall"; time = std::fmod(b.Clock, b.Duration("Fall")); }
        else if (b.Cast == 4) { clip = "Cast_Fly"; time = std::fmod(b.CastTurns * b.CastTwist, 1.f) * b.Duration(clip); }   // (a whole turn: her first pose)
        else if (b.Cast == 5) { clip = "Cast_Pull"; time = b.CastTime; }
        else if (!b.Attack.empty()) { clip = b.Attack; time = b.AttackTime; }
        else if (b.Air) { clip = "Fly"; time = b.FlyTime; }
        else if (flying) { clip = "Fly"; time = b.FlyTime; }
        else if (b.Jumping && b.JumpReal && b.Dropped > DropSeconds && b.Duration("Fall") > 0) { clip = "Fall"; time = std::fmod(b.Clock, b.Duration("Fall")); }
        else if (b.Jumping) { clip = "Jump"; time = b.JumpTime; }
        else if (moving) { clip = b.Gait; time = b.GaitTime(); }
        b.Current = clip;
        if (!cutting && !b.FlyBody && (clip == "Stand" || clip == "Idle") && b.TurnSteps > .02f && b.Sample("Move", b.GaitTime(), b.LegsPose, nullptr)) b.Legs = b.TurnSteps;
        {
            DoorwaysSkeletonPose pose;
            std::string error;
            // her feet on the drawn ground: on foot and standing on it, not while a move lifts or carries her
            const bool planted = !flying && p.IsGrounded && !b.Dead && !b.Jumping && b.Rise < 0 && b.Touch < 0 && !b.Cast && !b.Air && !b.Carried() &&
                                 std::abs(b.LiftNow) < .05f;
            const bool composed = b.Compose(world, clip, time, dt, pose, &error);
            if (composed) world.AnimatedPlayerVisual.Translation.y = b.LiftNow + b.PlantFeet(world, mFootGround, mStairTread, pose, dt, planted);
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
            if (composed) b.LookAbout(pose, dt, b.TurnRate, toward, b.Dead || b.Cast || !b.Attack.empty() || b.Rise >= 0 || b.Touch >= 0 ? 0.f : 1.f);
            {   // Standing at ease for a while she opens her wings, shakes them out and lowers them again (2026-11-19): every
                // quarter of a minute or so, not by a clock. A blow up on both wings as they open, one down after it.
                constexpr float StretchSeconds = 1.7f, Shake = .65f;
                b.AtEase = clip == "Stand" && std::abs(b.TurnRate) < .2f ? b.AtEase + dt : 0.f;
                if (b.AtEase <= 0) { b.NextStretch = b.Rand(9, 16); b.Stretch = -1; }
                else if (b.Stretch < 0 && b.AtEase > b.NextStretch)
                {
                    b.Stretch = 0; b.NextStretch = b.AtEase + b.Rand(14, 27);
                    for (auto& w : b.WingSpring) w.FlapGo += 2.4f;
                }
                else if (b.Stretch >= 0)
                {
                    if (b.Stretch < Shake && b.Stretch + dt >= Shake) for (auto& w : b.WingSpring) w.FlapGo -= 1.7f;
                    if ((b.Stretch += dt) > StretchSeconds) b.Stretch = -1;
                }
            }
            if (composed)
                b.SwingWings(pose, dt, !p.IsGrounded && !flying ? p.VerticalVelocity : 0.f, flying ? 0.f : b.Back ? -b.Gain : b.Gain, b.TurnRate,
                             b.Dead || b.Cast ? 0.f : flying || b.Rise >= 0 || b.Touch >= 0 ? .35f : 1.f,
                             clip == "Stand" ? (b.Stretch >= 0 ? 0.f : 1.f) : clip == "Move" ? .75f - .6f * b.GaitRun : 0.f);
            if (composed && b.Attack == "Skill_Storm" && b.AttackTime >= StormThrowFrom && b.AttackTime < StormThrowTo && b.Hips >= 0)
            {   // the storm: feather daggers thrown from her wings as they go round, each wing in turn; each straight out
                // (turned a little on round, the way she turns) and down to the ground, near ones steep and far ones flat
                const XMMATRIX place = BuildDoorwaysCharacterWorldMatrix(p.Position, p.Yaw, world.AnimatedPlayerVisual);
                const auto there = [&](XMVECTOR v) { XMFLOAT3 o; XMStoreFloat3(&o, XMVector3TransformCoord(v, place)); return o; };
                const auto joint = [&](int j) { const auto& m = pose.ModelSpaceMatrices[j]; return XMVectorSet(m._41, m._42, m._43, 1); };
                const XMFLOAT3 hips = there(joint(b.Hips));
                b.StormFeathers += dt * StormRate;
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
            if (!composed || !world.ImportedAnimatedCharacter.ApplyGamePresentationPose(b.Skeleton, pose, &error))
            {
                static int told = 0;
                if (told++ < 5) OutputDebugStringA(("[FAIL] Nyxara pose: " + error + "\n").c_str());
            }
            if (composed) b.PoseLog(world, pose, dt, planted);
        }
        if (b.Clock - b.LastLog > .1f) { b.Log(world, "frame"); b.LastLog = b.Clock; }

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

        mShake *= std::exp(-8.f * realDt); if (mShake < .003f) mShake = 0;
        s.CameraShake = mShake;
        mBarrier = std::clamp(mBarrier + (mGuarding ? 4.5f : -3.f) * dt, 0.f, 1.f);
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
            const bool front = (rel.x * facing.x + rel.z * facing.z) > -.3f;
            if (mBarrier > .5f && distance < 2.2f && front)
            {
                o.Life = 0; mBlockFlash = .25f;
                Burst(o.Pos, MoonWhite, 14, 4.f, .2f); Burst(o.Pos, SoulGreen, 8, 3.f, .18f);
                mNotice = L"BLOCKED"; mNoticeTimer = .5f;
            }
            else if (distance < .65f && combat.PlayerActionsAllowed())
            {
                o.Life = 0; Hurt(world, combat, o.Damage);
                Burst(o.Pos, o.Color, 16, 3.f, .22f);
            }
        }
        mOrbs.erase(std::remove_if(mOrbs.begin(), mOrbs.end(), [](const Orb& o) { return o.Life <= 0; }), mOrbs.end());
    }

    bool NyxaraKit::Hurt(WorldState& world, CombatSystem& combat, int damage)
    {
        if (mMercy > 0 || !combat.PlayerActionsAllowed()) return false;
        mMercy = CaucasianEagle::MercySeconds;
        if (mTrainingMode) ++mTrainingPlayerHits;
        else combat.ApplyIncomingDamage(world, damage);
        mBody->Struck = true;
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
        hud.ActionStatus = std::wstring(L"NYXARA  TWO SWORDS  |  F SLIDE ") + skill(0) + L"  V SWORDS " + skill(1) + L"  C SPIN " + skill(2) +
            (mBoosting ? L"  |  WING BOOST" : L"");
        hud.Controls = L"LMB/RMB LEFT / RIGHT SWORD  F SLIDE  V THROWN SWORDS  C FEATHER STORM  E BARRIER  A/D TURN  SHIFT RUN / BOOST  SPACE JUMP / CLIMB (HELD 3 S: FLY)  CTRL+SPACE SINK  R LAND";
        if (mNoticeTimer > 0) { hud.ActionNotice = mNotice; hud.ActionNoticeTimer = mNoticeTimer; }

        auto& h = NyxaraHud();   // the graphical HUD (DrawNyxaraHud)
        h.Valid = true;
        h.Flight = mFlightEnergy; h.FlightCharge = FlightCharge(); h.Flying = world.MainPlayer.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
        for (int i = 0; i < 3; ++i) { h.Cooldown[i] = mSkillCooldown[i]; h.CooldownMax[i] = SkillCooldown[i] * mCooldownScale; }
        const auto& b = *mBody;
        h.ComboOpen = false; h.ComboWindow = 0; h.ComboLeft = L""; h.ComboRight = L"";
        h.Action = b.Cast ? L"THROWN SWORDS" : b.Attack == "Skill_Storm" ? L"FEATHER STORM" : b.Attack == "Skill_Slide" || b.Air == 1 ? L"SLIDE" : L"";
        h.Guarding = mGuarding; h.Boosting = mBoosting; h.Swapped = false;
        h.Bow = false; h.Aim = 0; h.Shooting = false; h.OnTarget = false; h.HitMark = 0; h.HitKill = false; h.TargetVisible = false;
        h.Slashing = b.Attack.rfind("Attack_L", 0) == 0 || b.Attack == "Attack_Jump"; h.Heavy = b.Attack.rfind("Attack_R", 0) == 0; h.Swapping = false;
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
                {   // the streak (2) and turned air (4): long and thin along Go
                    const XMVECTOR x = XMVector3Normalize(XMLoadFloat3(&f.Go));
                    XMVECTOR z = XMVector3Cross(x, XMVectorSet(0, 1, 0, 0));
                    if (XMVectorGetX(XMVector3Length(z)) < 1e-3f) z = XMVectorSet(1, 0, 0, 0);
                    z = XMVector3Normalize(z);
                    XMMATRIX turn = XMMatrixIdentity();
                    turn.r[0] = XMVectorSetW(x, 0); turn.r[1] = XMVectorSetW(XMVector3Cross(z, x), 0); turn.r[2] = XMVectorSetW(z, 0);
                    if (f.Kind == 4) renderer.DrawBox(XMMatrixScaling(.16f + .3f * (1 - part), f.Size * part, f.Size * part * .5f) * turn * at, viewProjection, air);
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

        // the barrier: two spectral swords rise crossed in front of her, with a moonlit dome
        if (mBarrier > 0)
        {
            const auto& p = world.MainPlayer;
            const XMFLOAT3 f = Forward(p.Yaw);
            const float rise = Smooth(0, 1, mBarrier), height = 2.7f;
            const float pulse = .75f + .25f * std::sin(world.SceneTime * 9.f) + mBlockFlash * 2.f;
            for (const float side : {-1.f, 1.f})
            {
                const XMFLOAT4 color = side > 0 ? MoonWhite : SoulGreen;
                const XMMATRIX m = XMMatrixScaling(height, height, height) * XMMatrixRotationZ(-side * .30f) *
                    XMMatrixTranslation(side * .38f, height * .23f - height * 1.25f * (1 - rise), 0) * XMMatrixRotationY(p.Yaw) *
                    XMMatrixTranslation(p.Position.x + f.x * 1.15f, p.Position.y, p.Position.z + f.z * 1.15f);
                translucent(mBladeMesh, m, material(Alpha(color, .55f * rise * (std::min)(1.f, pulse)), nullptr, 1, true));
                const XMFLOAT3 tip = {p.Position.x + f.x * 1.15f + std::cos(p.Yaw) * side * .38f, p.Position.y + height * rise, p.Position.z + f.z * 1.15f - std::sin(p.Yaw) * side * .38f};
                billboard(tip, .9f * rise, Alpha(color, .6f * rise), mGlowTex.Get());
            }
            translucent(mSphereMesh, XMMatrixScaling(1.9f, 1.9f * rise, 1.9f) * XMMatrixTranslation(p.Position.x, p.Position.y, p.Position.z),
                material(Alpha(MoonWhite, (.07f + .2f * mBlockFlash) * rise), mGlowTex.Get(), 1, true));
            translucent(mRingMesh, XMMatrixScaling(2.f, 1, 2.f) * XMMatrixTranslation(p.Position.x, p.Position.y + .04f, p.Position.z),
                material(Alpha(SoulGreen, .55f * rise), mRingTex.Get(), 1, true));
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
    }
}
