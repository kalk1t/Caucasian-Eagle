#include "NyxaraKit.h"

#include "CombatSystem.h"
#include "DoorwaysAnimatedCharacterAsset.h"
#include "DoorwaysAnimationSampler.h"
#include "DoorwaysCharacterPresentation.h"
#include "DoorwaysGame.h"
#include "DoorwaysGamepad.h"
#include "DoorwaysRuntimePathResolver.h"
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
#include <cmath>
#include <fstream>
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
        const XMFLOAT4 Gold{1.f, .78f, .25f, 1.f};          // the finishing throw

        // ---- the lab's numbers (testing/CharacterLab/CharacterLab.cpp)
        using NyxaraClips::RunPace;
        using NyxaraClips::WalkPace;
        constexpr float AttackRate = 0.98f;  // how much faster than made
        constexpr float AttackLead = 0.34f;  // clip seconds before the hit at which a cut is entered
        constexpr float ChainAfter = 0.14f;  // clip seconds after the hit at which the next cut starts
        constexpr float AttackTail = 0.80f;  // clip seconds after the hit at which the last cut is over
        constexpr const char* LeftChain[] = {"Attack_L1", "Attack_L2", "Attack_L3"};
        constexpr const char* RightChain[] = {"Attack_R1", "Attack_R2", "Attack_R3", "Attack_R4"};
        constexpr float HipsShare = .35f;    // of the cut's own turn of her hips that her hips keep while she walks or runs
        constexpr float FrontArc = 30.f;     // degrees to either side of her line: what is before her
        constexpr float CastAirRange = 3, CastAirHeight = .35f, CastAirSink = 9;
        constexpr float AirDashSpeed = 12, AirDashSeconds = .55f;
        constexpr float SwordReach = 1.7f, JumpReach = 2.2f, SwordArc = FrontArc;
        constexpr int CutDamage = 45, LastDamage = 80, JumpDamage = 110;
        constexpr float SteerRate = 110.f;   // degrees a second A and D turn her way
        constexpr float BackSpeed = 1.25f;   // metres a second stepping backwards
        constexpr float KickFirst = .2f, KickEvery = .23f, KickReach = 1.6f, ThrowLands = 2.35f;
        constexpr int KickDamage = 30, ThrowDamage = 1000000;   // the throw is a finisher: it kills
        constexpr float SlideFrom = .25f, SlideTo = .95f, SlideEvery = .22f, SlideReach = 1.7f, SlideArc = FrontArc;
        constexpr int SlideDamage = 35;
        constexpr float ThrowRange = 10.f, ThrowGrab = 1.3f, ThrowBefore = .28f;
        constexpr float ThrowClamp = .35f, ThrowNeckWas = 1.64f, ThrowHoldFrom = 1.08f, ThrowHoldTo = 1.3f;
        constexpr float ThrowWeight = 22.f, ThrowSwing = .97f;
        constexpr float ThrowSettleFrom = 2.0f, ThrowSettleTo = 2.4f, ThrowFloor = .03f, ThrowCorrect = 6.f;
        constexpr float ThrowRiseFrom = .65f, ThrowRiseTo = 1.05f, ThrowFallFrom = 1.75f;
        constexpr float ThrowLies = 3.3f, ThrowUp = 4.3f;
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
        constexpr float CameraFollowLift = 25.f;
        // ---- the game's
        constexpr float SkillCooldown[3] = {4.f, 7.f, 9.f};   // F the slide, V the thrown swords, C the spinning swords
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
        XMFLOAT3 SwordGrip[2]{}, SwordTip[2]{};            // in the sword bone's axes: the handle's middle, the blade's point

        // ---- the pose
        DoorwaysSkeletonPose Previous, BlendFrom, LegsPose;
        std::string Gait = "Walk", LastClip, Current = "Idle";
        float Legs = 0, Phase = 0, BlendTime = 0, BlendSeconds = 0, Speed = 0, Clock = 0;
        bool FlyBody = false;
        XMFLOAT3 Clamp{0, 1.64f, .28f};    // the place between her thighs, in her own space (from the last pose)
        float Lowest = 0, LiftNow = 0;     // her lowest joint in her own space; metres her picture is raised now

        // ---- what she is doing
        std::string Attack;                // the cut or skill being played, empty: none
        float AttackTime = 0, SkillStrike = 0, SkillReach = 1;
        int Step = 0, Queued = 0;          // place in the combo; the button pressed during a cut (1 left, 2 right)
        bool Jumping = false, Back = false, Dead = false;
        float JumpTime = 0, DeathTime = 0;
        float FlyTime = 0, FlyLean = 0, Hover = 0, Rise = -1, Touch = -1;
        bool WasFlying = false;
        int Air = 0; float AirTime = 0;    // F in flight: 1 the dart forward, 2 flying to the creature she will throw

        // ---- the throw (F at a broken creature)
        int Thrown = -1;
        float ThrowTime = 0, ThrowNeck = 1.64f, ThrowStick = 1, ThrowGround = 0, ThrowGone = -1;
        XMFLOAT3 ThrowFrom{}, ThrowWay{0, 0, 1}, ThrowHeld{}, ThrowFeet{}, ThrowFeetWas{}, ThrowStood{};

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
            return name == "Idle" || name == "Walk" || name == "Run" || name == "Fly" || name == "Cast_Fly";
        }
        float Duration(const std::string& name) const
        {
            const auto found = Clips.find(name);
            return found == Clips.end() ? 0.f : found->second.DurationSeconds;
        }
        bool Sample(const std::string& name, float time, DoorwaysSkeletonPose& out, std::string* error) const
        {
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
              << '\t' << LiftNow << '\t' << FlyLean << '\n';
        }

        void StartAttack(const WorldState& world, const std::string& name, float into = 0)
        {
            Attack = name;
            AttackTime = name.rfind("Skill_", 0) == 0 ? 0.f : name == "Attack_Jump" ? (std::min)(.25f + into, (std::max)(.25f, Hit(name) - .15f))
                                                                                     : (std::max)(0.f, Hit(name) - AttackLead);
            Queued = 0;
            Jumping = false;
            Log(world, ("attack " + name).c_str());
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
            float speed = (path[i + 1] - path[i]) * 30 * AttackRate * (Skill() ? SkillReach : 1.f);
            if (Attack == "Skill_Throw" && Thrown >= 0 && AttackTime < ThrowGrab + .15f)
            {   // the throw: kept on the clip's path, so that her legs close where the creature's neck is
                const float part = (std::max)(0.f, AttackTime) * 30 - static_cast<float>(i);
                const float should = (path[i] + (path[i + 1] - path[i]) * (std::min)(part, 1.f)) * SkillReach;
                const auto& p = world.MainPlayer;
                const float gone = (p.Position.x - ThrowFrom.x) * ThrowWay.x + (p.Position.z - ThrowFrom.z) * ThrowWay.z;
                speed += ThrowCorrect * (should - gone);
            }
            return speed;
        }

        static XMVECTOR ArcTurn(XMVECTOR from, XMVECTOR to)   // the shortest turn that takes one direction to another
        {
            const float d = XMVectorGetX(XMVector3Dot(from, to));
            if (d > .9999f) return XMQuaternionIdentity();
            if (d < -.9999f)
            {
                XMVECTOR axis = XMVector3Cross(from, XMVectorSet(1, 0, 0, 0));
                if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-4f) axis = XMVector3Cross(from, XMVectorSet(0, 1, 0, 0));
                return XMQuaternionRotationAxis(XMVector3Normalize(axis), XM_PI);
            }
            return XMQuaternionRotationAxis(XMVector3Normalize(XMVector3Cross(from, to)), std::acos(d));
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
                if (n.find("Wing") == std::string::npos && n.find("Sword") == std::string::npos) BodyJoints.push_back(static_cast<int>(i));
                if (n.ends_with("SwordLeft")) SwordJoint[0] = static_cast<int>(i);
                if (n.ends_with("SwordRight")) SwordJoint[1] = static_cast<int>(i);
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
            if (name != LastClip)
            {
                BlendFrom = Previous;
                BlendTime = 0;
                BlendSeconds = name == "Death" || name == "Jump" ? .12f : name.rfind("Attack", 0) == 0 ? .17f : .2f;
                LastClip = name;
            }
            BlendTime += (std::max)(0.f, dt);
            if (BlendSeconds > 0 && BlendTime < BlendSeconds && BlendFrom.LocalJoints.size() == out.LocalJoints.size())
            {
                float w = std::clamp(BlendTime / BlendSeconds, 0.f, 1.f);
                w = w * w * (3 - 2 * w);
                for (size_t i = 0; i < out.LocalJoints.size(); ++i)
                {
                    auto& to = out.LocalJoints[i];
                    const auto& from = BlendFrom.LocalJoints[i];
                    XMStoreFloat3(&to.Translation, XMVectorLerp(XMLoadFloat3(&from.Translation), XMLoadFloat3(&to.Translation), w));
                    XMStoreFloat4(&to.Rotation, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&from.Rotation), XMLoadFloat4(&to.Rotation), w)));
                    XMStoreFloat3(&to.Scale, XMVectorLerp(XMLoadFloat3(&from.Scale), XMLoadFloat3(&to.Scale), w));
                }
            }
            PlaceSwords(world, out);
            if (!EvaluateDoorwaysSkeletonPoseMatrices(Skeleton, out, error)) return false;
            if (LegJoint[0] >= 0)
            {   // where a neck is held between her thighs, and her lowest joint
                const auto at = [&](int k) { const auto& m = out.ModelSpaceMatrices[LegJoint[k]]; return XMVectorSet(m._41, m._42, m._43, 0); };
                XMStoreFloat3(&Clamp, XMVectorLerp((at(0) + at(1)) * .5f, (at(2) + at(3)) * .5f, ThrowClamp));
                Lowest = 1e9f;
                for (const int i : BodyJoints) Lowest = (std::min)(Lowest, out.ModelSpaceMatrices[i]._42);
            }
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
        for (const char* need : {"Idle", "Walk", "Run", "Death", "Jump", "Fly", "Attack_L1", "Attack_R1", "Skill_Kick", "Skill_Throw", "Skill_Slide", "Skill_Cast", "Cast_Fly", "Cast_Pull"})
            if (b.Duration(need) <= 0) { error = std::string("her body has no clip ") + need + " (" + bundle.string() + ")"; return false; }
        b.FindSwords();
        if (b.SwordJoint[0] < 0) { error = "her body has no SwordLeft / SwordRight bones (" + bundle.string() + ")"; return false; }
        if (!b.Sample("Idle", 0, b.Previous, &error)) return false;
        char log[MAX_PATH] = {};
        if (GetEnvironmentVariableA("CAUCASIAN_EAGLE_NYXARA_LOG", log, sizeof(log)) > 0 && log[0])
        {
            b.LogPath = log;
            std::ofstream(b.LogPath) << "seconds\tevent\tclip\tspeed\tx\ty\tz\tlift\tlean\n";
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
        if (!load("Glow.png", mGlowTex) || !load("Ring.png", mRingTex) || !load("Feather.png", mFeatherTex)) return false;
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
        // A and D turn her way (and the view) instead of stepping sideways; she turns herself (HoldFacing)
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

    bool NyxaraKit::Broken(const Wraith& w) const
    {
        if (!w.Alive || w.Weak < .01f || w.MaxHealth < CaucasianEagle::FinisherMinHealth) return false;
        const float part = w.PushScale > .5f ? CaucasianEagle::FinisherHealthPart : CaucasianEagle::FinisherBossPart;
        return static_cast<float>(w.Health) <= part * static_cast<float>(w.MaxHealth);
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
        const bool flying = p.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
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
        if (mFinishReward)
        {   // the finishing throw felled it: a little health, and her skills come back sooner
            mFinishReward = false;
            if (alive) combat.RestorePlayerHealth(world, CaucasianEagle::FinisherHeal);
            for (auto& c : mSkillCooldown) c *= CaucasianEagle::FinisherCooldownPart;
            mNotice = L"FINISHED"; mNoticeTimer = 1.2f;
        }
        if ((mBrokenPulse -= realDt) <= 0)
        {   // a gold ring pulses under every broken enemy
            mBrokenPulse = .5f;
            for (const auto& w : mWraiths)
            {
                if (!Broken(w)) continue;
                Fx mark; mark.Kind = FxKind::Ring; mark.Pos = {w.Pos.x, w.Pos.y - w.Lift - 1.2f, w.Pos.z}; mark.Size = 1.4f + 2.f * w.Radius;
                mark.Life = .5f; mark.Color = Gold; mFx.push_back(mark);
                Burst(Add(w.Pos, {0, .9f, 0}), Gold, 3, 1.5f, .14f);
            }
        }

        // ---- dead: she falls and lies; everything she was doing ends
        if (!alive && !b.Dead)
        {
            b.Dead = true; b.DeathTime = 0; b.Attack.clear(); b.Queued = 0; b.Jumping = false; b.Air = 0; b.Rise = b.Touch = -1;
            b.Cast = 0; b.SwordsOut = false; b.Thrown = -1; b.Pushed = -1; mGuarding = false;
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
            b.Log(world, "jump");
            // a real jump where the game asks for one: from the ground, or from the brink of an edge she stands on
            if (mRealJump > 0 && p.VerticalVelocity <= 0 &&
                (p.IsGrounded || p.GroundState == Doorways::DoorwaysPhysicsGroundState::UnsupportedAirPrepared))
            {
                p.VerticalVelocity = mRealJump; p.IsGrounded = false;
                p.GroundState = Doorways::DoorwaysPhysicsGroundState::Airborne;
                p.MissingAuthoritativeGround = false; p.GroundY = p.TargetGroundY = p.Position.y;
            }
        }
        else if (b.Jumping && (b.JumpTime += dt) >= b.Duration("Jump")) { b.Jumping = false; b.Phase = 0; }
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
            bool throwing = false;
            if (keyF)
            {   // the nearest broken creature before her in range: the throw; else F is the slide (or, flying, the dart)
                float best = ThrowRange;
                for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                {
                    if (!Broken(mWraiths[i])) continue;
                    const Foe f = foe(i);
                    const float away = std::hypot(f.Feet.x - p.Position.x, f.Feet.z - p.Position.z) - f.Radius;
                    if (away > best || !before(f)) continue;
                    best = away; b.Thrown = i; throwing = true;
                }
                if (throwing)
                {
                    const Foe f = foe(b.Thrown);
                    const float middle = std::hypot(f.Neck.x - p.Position.x, f.Neck.z - p.Position.z);
                    p.Yaw = std::atan2(f.Feet.x - p.Position.x, f.Feet.z - p.Position.z);
                    const auto& path = NyxaraClips::Attacks.at("Skill_Throw").Forward;
                    const float grab = path[(std::min)(static_cast<size_t>(ThrowGrab * 30), path.size() - 1)];
                    b.SkillReach = std::clamp((middle - ThrowBefore) / (std::max)(grab, .1f), .05f, 3.f);
                    b.ThrowNeck = f.Neck.y - f.Feet.y;
                    b.ThrowGround = f.Feet.y;
                    b.ThrowWay = {std::sin(p.Yaw), 0, std::cos(p.Yaw)};
                    b.ThrowFrom = p.Position;
                    b.ThrowStood = b.ThrowFeet = b.ThrowFeetWas = f.Feet;
                    b.ThrowStick = (std::max)(.1f, std::sqrt((f.Neck.x - f.Feet.x) * (f.Neck.x - f.Feet.x) + (f.Neck.y - f.Feet.y) * (f.Neck.y - f.Feet.y) +
                                                             (f.Neck.z - f.Feet.z) * (f.Neck.z - f.Feet.z)));
                    b.ThrowTime = 0; b.ThrowGone = -1;
                    mFinishTarget = b.Thrown;
                    mMercy = (std::max)(mMercy, CaucasianEagle::FinisherMercy);   // nothing can hurt her while she throws it
                    b.Log(world, ("throw at " + std::to_string(b.Thrown) + " neck " + std::to_string(b.ThrowNeck) + " reach " + std::to_string(b.SkillReach)).c_str());
                }
                else b.Thrown = -1;
            }
            if (keyF && flying)
            {
                if (throwing || !cooling(0))
                {
                    b.Air = throwing ? 2 : 1; b.AirTime = 0; b.SkillStrike = .1f;
                    if (!throwing) mSkillCooldown[0] = SkillCooldown[0] * mCooldownScale;
                    CaucasianEagle::Sound::Play("nyxara/dash", .85f);
                    b.Log(world, throwing ? "air throw" : "air dart");
                }
            }
            else if (keyF && !throwing)
            {
                if (!cooling(0))
                {
                    b.SkillReach = 1;
                    b.StartAttack(world, "Skill_Slide"); b.SkillStrike = SlideFrom;
                    mSkillCooldown[0] = SkillCooldown[0] * mCooldownScale;
                    CaucasianEagle::Sound::Play("nyxara/dash", .85f);
                }
            }
            else if (keyF)
            {
                b.StartAttack(world, "Skill_Throw"); b.SkillStrike = ThrowLands;
                CaucasianEagle::Sound::Play("nyxara/skill", .85f); CaucasianEagle::Sound::Play("nyxara/effort", .8f);
                mNotice = L"EAGLE'S THROW"; mNoticeTimer = 1.f;
            }
            else if (!cooling(2))
            {
                b.SkillReach = 1;
                b.StartAttack(world, "Skill_Kick"); b.SkillStrike = KickFirst;
                mSkillCooldown[2] = SkillCooldown[2] * mCooldownScale;
                CaucasianEagle::Sound::Play("nyxara/skill", .85f);
            }
        }
        if (b.Skill()) b.Queued = 0;
        const auto chain = [](int h, int step) -> const char*
        {
            return h == 1 ? LeftChain[step % static_cast<int>(std::size(LeftChain))] : RightChain[step % static_cast<int>(std::size(RightChain))];
        };
        if (b.Queued && free)
        {
            if (b.Jumping) b.StartAttack(world, "Attack_Jump", b.JumpTime);
            else if (b.Attack.empty()) { b.Step = 0; b.StartAttack(world, chain(b.Queued, 0)); }
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
            else if (b.Thrown < 0 || b.Thrown >= static_cast<int>(mWraiths.size()) || !mWraiths[b.Thrown].Alive) { b.Air = 0; b.Thrown = -1; }
            else
            {   // she flies to it, down to its height, and the throw begins at its leap
                const Foe f = foe(b.Thrown);
                mWraiths[b.Thrown].Pinned = true;
                const float middle = std::hypot(f.Neck.x - p.Position.x, f.Neck.z - p.Position.z);
                p.Yaw = std::atan2(f.Feet.x - p.Position.x, f.Feet.z - p.Position.z);
                p.Position.y += std::clamp(f.Feet.y + CastAirHeight - p.Position.y, -CastAirSink * dt, CastAirSink * dt);
                p.VerticalVelocity = 0;
                b.ThrowTime = 0;
                const auto& path = NyxaraClips::Attacks.at("Skill_Throw").Forward;
                const float begun = path[(std::min)(static_cast<size_t>(ThrowRiseFrom * 30), path.size() - 1)];
                const float leap = (std::max)(.1f, path[(std::min)(static_cast<size_t>(ThrowGrab * 30), path.size() - 1)] - begun);
                if (middle - ThrowBefore <= leap || b.AirTime > 3)
                {
                    b.Air = 0;
                    b.StartAttack(world, "Skill_Throw");
                    b.AttackTime = b.ThrowTime = ThrowRiseFrom;
                    b.SkillStrike = ThrowLands;
                    b.SkillReach = std::clamp((middle - ThrowBefore) / leap, .05f, 1.5f);
                    b.ThrowNeck = f.Neck.y - f.Feet.y;
                    b.ThrowGround = f.Feet.y;
                    b.ThrowWay = {std::sin(p.Yaw), 0, std::cos(p.Yaw)};
                    b.ThrowFrom = {p.Position.x - b.ThrowWay.x * begun * b.SkillReach, p.Position.y, p.Position.z - b.ThrowWay.z * begun * b.SkillReach};
                    b.ThrowStood = b.ThrowFeet = b.ThrowFeetWas = f.Feet;
                    b.ThrowStick = (std::max)(.1f, std::sqrt((f.Neck.x - f.Feet.x) * (f.Neck.x - f.Feet.x) + (f.Neck.y - f.Feet.y) * (f.Neck.y - f.Feet.y) +
                                                             (f.Neck.z - f.Feet.z) * (f.Neck.z - f.Feet.z)));
                }
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
            {   // the twist, by how far she has come (it never goes back); the last turn slows into the grab
                const float come = std::clamp(1 - (std::max)(0.f, away - b.CastStand - .05f) / b.CastWay, 0.f, 1.f);
                b.CastTwistWas = b.CastTwist;
                b.CastTwist = (std::max)(b.CastTwist, 1 - (1 - come) * (1 - come));
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
                    if (target >= 0 && target != b.Thrown && mWraiths[target].PushScale > .5f)
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
            b.AttackTime += dt * AttackRate;
            if (b.AttackTime >= b.SkillStrike)
            {
                const bool kick = b.Attack == "Skill_Kick", slide = b.Attack == "Skill_Slide";
                if (kick) strike(KickReach, 180, KickDamage, CutPush);
                else if (slide) strike(SlideReach, SlideArc, SlideDamage, CutPush);
                else if (b.Thrown >= 0 && mWraiths[b.Thrown].Alive)
                {   // the throw: it is brought down, and dies
                    strikeOne(b.Thrown, ThrowDamage);
                    if (!mWraiths[b.Thrown].Alive) mFinishReward = true;
                    mShake = (std::max)(mShake, .3f);
                    Burst(b.ThrowHeld, Gold, 24, 5.f, .3f);
                    RingEffect({b.ThrowHeld.x, b.ThrowGround + .05f, b.ThrowHeld.z}, 3.f, Gold, .6f);
                }
                b.SkillStrike = kick ? b.SkillStrike + KickEvery : slide && b.SkillStrike + SlideEvery <= SlideTo ? b.SkillStrike + SlideEvery : 1e9f;
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
                b.Step = b.Attack == "Attack_Jump" ? 0 : b.Step + 1;
                b.StartAttack(world, chain(b.Queued, b.Step));
            }
            else if (b.AttackTime >= (std::min)(b.Duration(b.Attack), hit + AttackTail)) b.Attack.clear();
        }

        // ---- the creatures she holds: pushed over by V, thrown by F
        float lift = 0;
        if (b.Pushed >= 0)
        {
            b.PushTime += dt;
            auto& w = mWraiths[b.Pushed];
            if (!w.Alive && b.PushGone < 0) b.PushGone = b.PushTime;
            const float over = CastFallSeconds + CastDownSeconds + CastRiseSeconds;
            const bool done = b.PushTime >= over || b.Thrown == b.Pushed || (b.PushGone >= 0 && b.PushTime > b.PushGone + GoneSeconds);
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
        if (b.Thrown >= 0 && b.Air != 2)
        {
            b.ThrowTime += dt * AttackRate;
            auto& w = mWraiths[b.Thrown];
            if (!w.Alive && b.ThrowGone < 0) b.ThrowGone = b.ThrowTime;
            const bool hers = b.Attack == "Skill_Throw";
            const float here = p.Position.y - b.ThrowGround;   // her height over the ground it stands on
            if (hers)
            {   // her height: the clip's, scaled to this creature's neck, and down to the ground if she began in the air
                const float fitted = ease(b.ThrowTime, ThrowRiseFrom, ThrowRiseTo) * (1 - ease(b.ThrowTime, ThrowSettleFrom, ThrowSettleTo));
                lift = fitted * (b.ThrowNeck / ThrowNeckWas - 1) * b.Clamp.y - ease(b.ThrowTime, ThrowRiseFrom, ThrowRiseTo) * (1 - ease(b.ThrowTime, ThrowLands + .5f, ThrowLies + .2f)) * here;
                lift = (std::max)(lift, (std::min)(0.f, ThrowFloor - b.Lowest - here));
            }
            if ((!hers && (b.ThrowTime < ThrowFallFrom || b.ThrowTime >= ThrowUp)) || b.Dead || (b.ThrowGone >= 0 && b.ThrowTime > b.ThrowGone + GoneSeconds))
                b.Thrown = -1;   // broken off before it went down, or over, or it is gone
            else
            {
                if (b.ThrowTime < ThrowLands)
                {   // in her legs
                    const float c = std::cos(p.Yaw), sn = std::sin(p.Yaw);
                    b.ThrowHeld = {p.Position.x + b.Clamp.x * c + b.Clamp.z * sn, p.Position.y + b.LiftNow + b.Clamp.y, p.Position.z - b.Clamp.x * sn + b.Clamp.z * c};
                }
                else b.ThrowHeld.y += (b.ThrowGround - b.ThrowHeld.y) * (std::min)(1.f, dt * 9);   // down: it lies where it came down
                const float hold = ease(b.ThrowTime, ThrowHoldFrom, ThrowHoldTo) * (1 - ease(b.ThrowTime, ThrowLies, ThrowUp));
                if (b.ThrowTime < ThrowHoldFrom) b.ThrowFeet = b.ThrowFeetWas = b.ThrowStood;
                else if (dt > 1e-4f)
                {   // its feet: carried on by their own speed and weight, kept the stick's length from its neck, and on the ground
                    const XMVECTOR neck = XMLoadFloat3(&b.ThrowHeld), was = XMLoadFloat3(&b.ThrowFeet);
                    XMVECTOR feet = was + (was - XMLoadFloat3(&b.ThrowFeetWas)) * ThrowSwing + XMVectorSet(0, -ThrowWeight * dt * dt, 0, 0);
                    XMVECTOR from = feet - neck;
                    if (XMVectorGetX(XMVector3Length(from)) < 1e-4f) from = XMVectorSet(0, -1, 0, 0);
                    feet = neck + XMVector3Normalize(from) * b.ThrowStick;
                    XMVECTOR back = was;
                    if (XMVectorGetY(feet) <= b.ThrowGround)
                    {   // on the ground: as far from under its neck as the stick is long, on the side they are on
                        const float high = std::clamp(b.ThrowHeld.y - b.ThrowGround, 0.f, b.ThrowStick), reach = std::sqrt(b.ThrowStick * b.ThrowStick - high * high);
                        XMVECTOR level = XMVectorSetY(feet - neck, 0);
                        if (XMVectorGetX(XMVector3Length(level)) < 1e-3f) level = -XMLoadFloat3(&b.ThrowWay);
                        feet = XMVectorSetY(neck + XMVector3Normalize(level) * reach, b.ThrowGround);
                        back = XMVectorLerp(back, feet, .5f);   // they drag
                    }
                    XMStoreFloat3(&b.ThrowFeetWas, back);
                    XMStoreFloat3(&b.ThrowFeet, feet);
                }
                w.Pinned = true;
                w.Hold = hold;
                w.HeldNeck = b.ThrowHeld;
                w.HeldFeet = b.ThrowFeet;
            }
        }
        if (b.Thrown >= 0 && b.Attack == "Skill_Throw") mWraiths[b.Thrown].Pinned = true;   // on her way to it: it waits

        // ---- her picture: leaning in flight, lifted to what she does, the camera with it
        const bool braced = b.Cast == 5 && b.CastTime < CastThrust;
        b.Speed = std::hypot(p.ActualVelocity.x, p.ActualVelocity.z);
        const float lean = b.Air ? FlyLeanFast : braced ? (std::min)(CastStoopMost, CastStoop * b.CastMissing) : b.Cast == 4 ? CastLean
                         : flying && !landing && !b.Dead ? FlyLeanHover + std::clamp(b.Speed / FlyLeanSpeed, 0.f, 1.f) * (FlyLeanFast - FlyLeanHover) : 0.f;
        b.FlyLean += (lean - b.FlyLean) * (std::min)(1.f, dt * 4);
        world.AnimatedPlayerVisual.Rotation = {b.FlyLean, 0, 0};
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
            const float given = (std::max)(wanted, (std::min)(0.f, ThrowFloor - b.Lowest - here - lift));
            b.CastMissing = given - wanted;
            lift += given - b.CastUp * CastLift;
        }
        const float hover = b.Thrown >= 0 && b.Attack == "Skill_Throw" ? 0.f : b.Hover * FlyHover;   // (the throw is fitted exactly)
        b.LiftNow = lift + hover + b.CastUp * CastLift;
        world.AnimatedPlayerVisual.Translation = {0, b.LiftNow, 0};
        // The camera looks at her real place, and these lifts move only her picture: so it is told of them, smoothly
        b.CameraLift += (b.LiftNow - b.CameraLift) * (std::min)(1.f, dt * CameraFollowLift);
        s.CameraPivotHeight = b.PivotBase + b.CameraLift;

        // ---- travel: A and D steer (they turn the view and her way); S steps back; she turns to where she goes
        const bool carried = b.Carried();
        float impulse = !b.Dead && dt > 0 ? b.ForwardSpeed(world) : 0.f;
        s.ImpulseX = std::sin(p.Yaw) * impulse;
        s.ImpulseZ = std::cos(p.Yaw) * impulse;
        const bool keyW = live && IsDoorwaysKeyDown('W'), keyS = live && IsDoorwaysKeyDown('S');
        const int steer = live && !b.Dead && dt > 0 ? (IsDoorwaysKeyDown('D') ? 1 : 0) - (IsDoorwaysKeyDown('A') ? 1 : 0) : 0;
        const bool rooted = carried || b.Rise >= 0 || b.Touch >= 0 || b.Cast != 0 || b.Air != 0 || b.Dead || mGuarding;
        if (steer)
        {
            auto& view = world.MainCamera;
            view.Yaw = std::remainder(view.Yaw + static_cast<float>(steer) * XMConvertToRadians(SteerRate) * dt, XM_2PI);
            if (!keyW && !keyS && !rooted)
            {   // alone they also carry her forward, so she goes round in a curve
                const float pace = (live && IsDoorwaysKeyDown(VK_CONTROL)) ? WalkPace : flying ? p.MoveSpeed : (live && IsDoorwaysKeyDown(VK_SHIFT)) ? CaucasianEagle::FootRunSpeed : CaucasianEagle::FootSpeed;
                s.ImpulseX += std::sin(view.Yaw) * pace;
                s.ImpulseZ += std::cos(view.Yaw) * pace;
            }
        }
        b.Back = keyS && !keyW && !b.Dead && !flying;
        s.HoldFacing = true;
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
        p.MoveSpeed *= mSpeedScale;
        s.SlowWalkSpeed = rooted ? 0.f : WalkPace;
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
        const bool moving = b.Speed > .08f, running = b.Speed > RunFrom;
        std::string clip = "Idle";
        float time = b.Clock;
        const bool striding = moving && !b.Dead && !flying;
        if (striding)
        {
            const bool runs = running && !b.Back;   // backwards she walks, the stride played the other way round
            b.Gait = runs ? "Run" : "Walk";
            const float rate = b.Speed / (runs ? RunPace : WalkPace);
            b.Phase = std::fmod(b.Phase + (b.Back ? -1.f : 1.f) * dt * rate / b.Duration(b.Gait) + 1.f, 1.f);
        }
        const bool cutting = !b.Attack.empty() && !carried && !b.Dead;
        if (!cutting) b.Legs = 0;
        else if (b.LastClip.rfind("Attack", 0) != 0) b.Legs = striding ? 1.f : 0.f;
        else b.Legs = std::clamp(b.Legs + (striding ? 1.f : -1.f) * dt / .15f, 0.f, 1.f);
        b.FlyBody = (cutting || (b.Cast >= 1 && b.Cast <= 3)) && flying;   // a cut in the air: her legs, hips and wings go on flying
        if (b.FlyBody) b.Legs = b.Sample("Fly", b.FlyTime, b.LegsPose, nullptr) ? 1.f : 0.f;
        else if (b.Legs > 0 && !b.Sample(b.Gait, b.Phase * b.Duration(b.Gait), b.LegsPose, nullptr)) b.Legs = 0;
        if (b.Dead) { clip = "Death"; time = b.DeathTime; }
        else if (b.Rise >= 0) { clip = "Fly_Rise"; time = b.Rise; }
        else if (b.Touch >= 0) { clip = "Fly_Land"; time = b.Touch; }
        else if (b.Cast == 4) { clip = "Cast_Fly"; time = std::fmod(b.CastTurns * b.CastTwist, 1.f) * b.Duration(clip); }   // (a whole turn: her first pose)
        else if (b.Cast == 5) { clip = "Cast_Pull"; time = b.CastTime; }
        else if (!b.Attack.empty()) { clip = b.Attack; time = b.AttackTime; }
        else if (b.Air) { clip = "Fly"; time = b.FlyTime; }
        else if (flying) { clip = "Fly"; time = b.FlyTime; }
        else if (b.Jumping) { clip = "Jump"; time = b.JumpTime; }
        else if (moving) { clip = b.Gait; time = b.Phase * b.Duration(clip); }
        b.Current = clip;
        {
            DoorwaysSkeletonPose pose;
            std::string error;
            if (!b.Compose(world, clip, time, dt, pose, &error) || !world.ImportedAnimatedCharacter.ApplyGamePresentationPose(b.Skeleton, pose, &error))
            {
                static int told = 0;
                if (told++ < 5) OutputDebugStringA(("[FAIL] Nyxara pose: " + error + "\n").c_str());
            }
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
        if (alive && !b.Moving() && mNoticeTimer <= 0)
            for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                if (Broken(mWraiths[i]))
                {
                    const Foe f = foe(i);
                    if (std::hypot(f.Feet.x - p.Position.x, f.Feet.z - p.Position.z) - f.Radius <= ThrowRange && before(f)) { mNotice = L"F: FINISH IT"; mNoticeTimer = .25f; break; }
                }

        mShake *= std::exp(-8.f * realDt); if (mShake < .003f) mShake = 0;
        s.CameraShake = mShake;
        mBarrier = std::clamp(mBarrier + (mGuarding ? 4.5f : -3.f) * dt, 0.f, 1.f);
        mBlockFlash = (std::max)(0.f, mBlockFlash - dt);
        UpdateWraiths(world, combat, dt);
        UpdateEffects(dt);
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
        CaucasianEagle::Sound::Play("combat/blow", .9f); CaucasianEagle::Sound::Play("nyxara/hurt", .9f);
        return true;
    }

    void NyxaraKit::UpdateEffects(float dt)
    {
        mMercy = (std::max)(0.f, mMercy - dt);
        for (auto& f : mFx)
        {
            f.T += dt;
            switch (f.Kind)
            {
            case FxKind::Spark:
                f.Pos = Add(f.Pos, Mul(f.Vel, dt)); f.Vel.y -= 4.f * dt; f.Vel = Mul(f.Vel, std::exp(-1.5f * dt)); break;
            case FxKind::Feather:
                f.Pos = Add(f.Pos, Mul(f.Vel, dt)); f.Vel.y -= 9.f * dt; break;
            default: break;
            }
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
        hud.Controls = L"LMB/RMB LEFT / RIGHT SWORD  F SLIDE (BROKEN: THROW)  V THROWN SWORDS  C SPINNING SWORDS  E BARRIER  A/D TURN  SHIFT RUN / BOOST  SPACE JUMP / CLIMB  R FLY";
        if (mNoticeTimer > 0) { hud.ActionNotice = mNotice; hud.ActionNoticeTimer = mNoticeTimer; }

        auto& h = NyxaraHud();   // the graphical HUD (DrawNyxaraHud)
        h.Valid = true;
        for (int i = 0; i < 3; ++i) { h.Cooldown[i] = mSkillCooldown[i]; h.CooldownMax[i] = SkillCooldown[i] * mCooldownScale; }
        const auto& b = *mBody;
        h.ComboOpen = false; h.ComboWindow = 0; h.ComboLeft = L""; h.ComboRight = L"";
        h.Action = b.Attack == "Skill_Throw" ? L"EAGLE'S THROW" : b.Cast ? L"THROWN SWORDS" : b.Attack == "Skill_Kick" ? L"SPINNING SWORDS"
                 : b.Attack == "Skill_Slide" || b.Air == 1 ? L"SLIDE" : L"";
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
        const auto streak = [&](const DoorwaysGpuMesh& mesh, XMFLOAT3 at, XMFLOAT3 dir, float length, float width, XMFLOAT4 color, ID3D11ShaderResourceView* tex)
        {   // card whose +Y follows dir and faces the camera around that axis
            const XMFLOAT3 y = Norm(dir);
            XMFLOAT3 x = Cross(y, Norm(Sub(eye, at))); x = Len(x) < 1e-3f ? XMFLOAT3{1, 0, 0} : Norm(x);
            const XMFLOAT3 z = Cross(x, y);
            translucent(mesh, Basis(Mul(x, width), Mul(y, length), z, at), material(color, tex, 1, true));
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
            streak(mFeatherMesh, o.Pos, Mul(o.Vel, -1), .9f, .35f, Alpha(o.Color, .45f), mGlowTex.Get());
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
                streak(mFeatherMesh, Sub(f.Pos, Mul(Norm(f.Vel), f.Size)), f.Vel, f.Size * 1.6f, f.Size * .6f, Alpha(f.Color, .9f * fade), mFeatherTex.Get());
                billboard(f.Pos, .45f, Alpha(f.Color, .6f * fade), mGlowTex.Get());
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
