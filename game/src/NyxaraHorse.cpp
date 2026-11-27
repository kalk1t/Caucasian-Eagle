#include "NyxaraHorse.h"

#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysCharacterPresentation.h"
#include "DoorwaysGame.h"
#include "DoorwaysInputActionMap.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRuntimePathResolver.h"
#include "DoorwaysRuntimeScene.h"
#include "DoorwaysWalkableSurfaceQuery.h"
#include "Renderer.h"
#include "Sound.h"
#include "WorldState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace DirectX;

namespace Doorways
{
    namespace
    {
        constexpr float Pi = 3.14159265f;
        constexpr float Degree = Pi / 180;
        float Smooth(float a, float b, float x) { const float t = std::clamp((x - a) / (b - a), 0.f, 1.f); return t * t * (3 - 2 * t); }
        float Ease(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3 - 2 * t); }
        float Frac(float v) { return v - std::floor(v); }
        float Toward(float dt, float rate) { return 1 - std::exp(-rate * dt); }
        float Wrap(float a) { return std::remainder(a, 2 * Pi); }
        XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
        XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
        XMFLOAT3 Mul(const XMFLOAT3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
        XMFLOAT3 Ahead(float yaw) { return {std::sin(yaw), 0, std::cos(yaw)}; }
        XMFLOAT3 RightOf(float yaw) { return {std::cos(yaw), 0, -std::sin(yaw)}; }
        XMFLOAT3 Point(const XMFLOAT3& v, const XMMATRIX& m) { XMFLOAT3 o; XMStoreFloat3(&o, XMVector3TransformCoord(XMLoadFloat3(&v), m)); return o; }
        XMFLOAT3 Dir(const XMFLOAT3& v, const XMMATRIX& m) { XMFLOAT3 o; XMStoreFloat3(&o, XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&v), m))); return o; }
        XMMATRIX About(const XMFLOAT3& p, const XMMATRIX& turn) { return XMMatrixTranslation(-p.x, -p.y, -p.z) * turn * XMMatrixTranslation(p.x, p.y, p.z); }

        // ---- the six
        struct KindSpec
        {
            const wchar_t* Name;
            const wchar_t* Line;
            const char* Model;
            float Seat;      // m its seat over the ground (its size)
            float Top;       // m/s its gallop
            float Gather;    // how quickly it gathers speed (times the usual)
            float Turn;      // how sharply it turns
            float Leap;      // m/s it leaps up
            float Wind;      // s of gallop in it
            float Glow;      // its picture's own light
            XMFLOAT4 Spark;  // its hooves' sparks (no alpha: none)
            bool Wings;
        };
        enum { Tushetian, Kabardian, Karabakh, Rashi, Fire, Frost };
        const KindSpec Kinds[NyxaraHorses::KindCount] = {
            {L"TUSHETIAN", L"the small mountain horse of Tusheti: quick to start and turn, high leaps, no slower uphill", "Horses/Tushetian",
             1.32f, 19.f, 1.35f, 1.3f, 8.f, 30, 0, {0, 0, 0, 0}, false},
            {L"KABARDIAN", L"the Kabardian warhorse: steady, the longest wind", "Horses/Kabardian", 1.52f, 21.f, 1.f, 1.f, 7.f, 70, 0, {0, 0, 0, 0}, false},
            {L"KARABAKH", L"the golden Karabakh: the fastest gallop", "Horses/Karabakh", 1.47f, 25.f, 1.1f, 1.05f, 7.2f, 28, 0, {0, 0, 0, 0}, false},
            {L"RASHI", L"the winged horse of the tales: its wings open as it leaps; Space held in the air, it glides; again, a beat", "Horses/Rashi",
             1.55f, 22.f, 1.f, 1.f, 8.f, 40, .04f, {.92f, .96f, 1.f, .7f}, true},
            {L"FIRE", L"a horse of fire: embers under its hooves, a burst of them as it lands", "Horses/FireHorse", 1.53f, 23.f, 1.25f, .95f, 7.4f, 32, .45f,
             {1.f, .42f, .08f, 1}, false},
            {L"FROST", L"a horse of frost: rime under its hooves; it slides wide in its turns", "Horses/FrostHorse", 1.53f, 22.f, 1.f, .9f, 7.2f, 38, .16f,
             {.62f, .86f, 1.f, .9f}, false},
        };
        struct WaySpec { const wchar_t* Name; const wchar_t* Line; };
        const WaySpec Ways[NyxaraHorses::WayCount] = {
            {L"REINS", L"W canter, Shift gallop, Ctrl walk; A D turn it; S stops it, then backs it"},
            {L"GAITS", L"W a gait up, S a gait down, kept with no key held; Shift held gallops; A D turn it"},
            {L"LOOK", L"it goes where the view looks: W canter, Shift gallop, Ctrl walk; A D aside; S backs it"},
            {L"SPURS", L"each W a spur, more in time with its stride; it slows by itself; S held reins it in; A D turn it"},
            {L"WEIGHT", L"a heavy horse: slow to gather speed and to lose it, wide leaning turns, sliding at speed"},
            {L"WIND", L"at once at speed, sharp turns; S turns it round; A or D twice a leap aside; Shift a burst"},
        };
        enum { Reins, Gaits, Look, Spurs, Weight, Wind };

        // ---- its gaits. A stride: each hoof down for Duty of it from its Offset (the share of the stride at which it comes
        // down), then lifted and swung forward. Its body lowest at BobAt (BobBeats times a stride), its nose highest at
        // RockAt, its neck lowest at NodAt; its back bent most at FlexAt. Her lean and how much of its rise and fall her knees
        // take (Absorb) are the gait's too. (Hooves: front left, front right, hind left, hind right.)
        struct GaitSpec
        {
            const char* Name;
            float Speed, Rate, Duty, LiftFront, LiftHind;
            float Offset[4];
            float Bob, BobBeats, BobAt;
            float Rock, RockAt;
            float Nod, NodBeats, NodAt;
            float Flex, FlexAt;
            float Lean, Absorb;
        };
        const GaitSpec GaitSpecs[5] = {
            {"stand", 0, .9f, 1, 0, 0, {0, 0, 0, 0}, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, .03f, 0},
            // walk: four beats, each hind before the fore of its side
            {"walk", 1.7f, .95f, .62f, .11f, .085f, {.25f, .75f, 0, .5f}, .014f, 2, .1f, .012f, .3f, .07f, 2, .3f, 0, 0, .05f, 0},
            // trot: the diagonals together
            {"trot", 3.9f, 1.4f, .42f, .17f, .13f, {0, .5f, .5f, 0}, .035f, 2, .21f, .01f, 0, .025f, 2, .21f, 0, 0, .1f, .45f},
            // canter, its right lead: left hind; right hind with left fore; right fore; then all off the ground
            {"canter", 7.5f, 1.7f, .32f, .22f, .16f, {.28f, .55f, 0, .28f}, .045f, 1, .4f, .07f, .12f, .12f, 1, .5f, .04f, .2f, .17f, .5f},
            // gallop, its right lead: left hind, right hind, left fore, right fore; then all off the ground
            {"gallop", 14, 2.15f, .22f, .24f, .19f, {.45f, .57f, 0, .11f}, .055f, 1, .38f, .055f, .1f, .14f, 1, .42f, .09f, .25f, .34f, .85f},
        };
        constexpr float MaxStride = 1.8f;      // m a hoof goes back under it while it is down (at most: then a faster stride)
        constexpr float MountReach = 3.6f;     // m from her to a horse for T to get her on it
        constexpr float MountSeconds = .75f, DismountSeconds = .6f;
        constexpr int LegJoints[4][3] = {{HorseRig::FrontUpL, HorseRig::FrontKneeL, HorseRig::FrontFetlockL},
                                         {HorseRig::FrontUpR, HorseRig::FrontKneeR, HorseRig::FrontFetlockR},
                                         {HorseRig::HindUpL, HorseRig::HindHockL, HorseRig::HindFetlockL},
                                         {HorseRig::HindUpR, HorseRig::HindHockR, HorseRig::HindFetlockR}};
    }

    bool HorseRig::Load(const std::filesystem::path& path, std::string& error)
    {
        std::ifstream file(path);
        if (!file) { error = path.string() + ": cannot open"; return false; }
        for (int j = 0; j < JointCount; ++j) { Parent[j] = -2; Pivot[j] = {}; }
        bool horse = false;
        int hooves = 0, stirrups = 0;
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream in(line);
            std::string tag; in >> tag;
            if (tag == "TYPE") { std::string kind; in >> kind; horse = kind == "horse"; }
            else if (tag == "J")
            {
                int joint = -1, parent = -1; XMFLOAT3 p{};
                in >> joint >> parent >> p.x >> p.y >> p.z;
                if (!in || joint < 0 || joint >= JointCount || parent >= JointCount) { error = path.string() + ": bad joint line"; return false; }
                Parent[joint] = parent; Pivot[joint] = p;
            }
            else if (tag == "SEAT") in >> Seat.x >> Seat.y >> Seat.z;
            else if (tag == "STIRRUP")
            {
                int side = 0; XMFLOAT3 p{};
                in >> side >> p.x >> p.y >> p.z;
                if (in) { Stirrup[side < 0 ? 0 : 1] = p; ++stirrups; }
            }
            else if (tag == "HOOF")
            {
                int leg = -1; XMFLOAT3 p{};
                in >> leg >> p.x >> p.y >> p.z;
                if (in && leg >= 0 && leg < 4) { Hoof[leg] = p; ++hooves; }
            }
            else if (tag == "WIDTH") in >> Width;
            else if (tag == "BELLY") in >> Belly;
        }
        if (!horse || hooves != 4 || stirrups != 2 || Seat.y <= 0) { error = path.string() + ": not a horse's rig"; return false; }
        int count = 0; bool placed[JointCount]{};
        for (int j = 0; j < JointCount; ++j)
            if (Parent[j] < -1) { error = path.string() + ": a joint is missing"; return false; }
        for (int pass = 0; pass < JointCount && count < JointCount; ++pass)
            for (int j = 0; j < JointCount; ++j)
                if (!placed[j] && (Parent[j] < 0 || placed[Parent[j]])) { placed[j] = true; Order[count++] = j; }
        if (count != JointCount) { error = path.string() + ": joints form a loop"; return false; }
        return true;
    }

    // ---- a horse
    struct NyxaraHorses::Horse
    {
        int Kind = 0;
        HorseRig Rig;
        DoorwaysGpuMesh Mesh;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture, Normal;
        float Scale = 1;                     // m a mesh unit
        XMFLOAT3 Centre{};                   // (mesh) the middle of its hooves, on the ground
        float LegHigh = .8f;                 // (mesh) its legs' height
        float Nose = 1.4f, Haunch = 1.f;     // m from its middle to the front of its head, and to its haunches
        // where it is and how it goes
        XMFLOAT3 Pos{};
        float Yaw = 0, TravelYaw = 0;
        float Speed = 0;                     // m/s asked along its facing (under 0: back)
        float Fwd = 0, Side = 0;             // m/s it goes, along its facing and to its right (as measured)
        float TurnRate = 0;                  // rad/s
        float YawWas = 0;
        bool Grounded = true;
        float Vy = 0;                        // m/s up
        float AirTime = 0, LeapTime = -1;
        int AirBeats = 0;
        bool Gliding = false;
        float Land = 0;                      // 1 as it comes down from a leap, then less
        float Slope = 0;                     // rad its body pitched with the ground (nose up +)
        float Roll = 0;                      // rad leaned into its turn
        float Rear = 0, RearTime = -1;       // rearing
        float Graze = 0, GrazeTime = 0, GrazeNext = 6;
        float Swish = 0, SwishGo = 0, SwishNext = 3;
        float Clock = 0;
        float Wind = 1;                      // its wind (stamina), 0 .. 1
        bool Winded = false;
        int Gear = 0;                        // (GAITS) -1 back, 0 stand, 1 walk, 2 trot, 3 canter, 4 gallop
        float Spurred = 0;                   // (SPURS) the speed its spurs ask
        float Burst = 0, BurstRest = 0;      // (WIND) a burst, and its rest
        float TurnAbout = -1;                // (WIND) s into turning round on its haunches
        float TurnAboutWay = 1;
        float AsideTime = -1, AsideWay = 0;  // (WIND) a leap aside
        float TapA = -9, TapD = -9;
        float Blocked = 0;
        NyxaraHorses::Controls Was;
        bool Called = false;
        // its stride
        float Phase = 0;
        float Weight[GaitCount]{1, 0, 0, 0, 0};
        int Gait = Stand;
        float StrideWas[4]{};
        // its pose this frame
        float Bob = 0, Pitch = 0, Nod = 0, Flex = 0, Look = 0, TailSwing = 0, TailLift = 0;
        float WingOpen = 0, WingBeat = 0, WingBeatPhase = 0, WingAmp = 0;
        XMFLOAT3 HoofAt[4]{};                // (mesh, its pose's ground) where each hoof is put
        float Flexed[4]{};
        bool Down[4]{};
        DoorwaysGpuSkinningPalette Palette;
        XMFLOAT4X4 Global[HorseRig::JointCount]{};
        XMFLOAT4X4 World{};
    };

    NyxaraHorses::NyxaraHorses() = default;
    NyxaraHorses::~NyxaraHorses() = default;

    const wchar_t* NyxaraHorses::KindName(int kind) { return Kinds[std::clamp(kind, 0, KindCount - 1)].Name; }
    const wchar_t* NyxaraHorses::KindLine(int kind) { return Kinds[std::clamp(kind, 0, KindCount - 1)].Line; }
    const wchar_t* NyxaraHorses::WayName(int way) { return Ways[std::clamp(way, 0, WayCount - 1)].Name; }
    const wchar_t* NyxaraHorses::WayLine(int way) { return Ways[std::clamp(way, 0, WayCount - 1)].Line; }
    void NyxaraHorses::SetKind(int kind) { mKind = std::clamp(kind, 0, KindCount - 1); }
    XMFLOAT3 NyxaraHorses::At(int kind) const { return mHorses.empty() ? XMFLOAT3{} : mHorses[static_cast<size_t>(std::clamp(kind, 0, KindCount - 1))].Pos; }
    float NyxaraHorses::YawOf(int kind) const { return mHorses.empty() ? 0.f : mHorses[static_cast<size_t>(std::clamp(kind, 0, KindCount - 1))].Yaw; }
    void NyxaraHorses::SetWay(int way) { mWay = std::clamp(way, 0, WayCount - 1); }

    bool NyxaraHorses::Load(Renderer& renderer, std::string& error)
    {
        if (mLoaded) return true;
        const auto root = ResolveDoorwaysRuntimePath("assets/Compiled");
        mHorses.clear();
        mHorses.resize(KindCount);
        for (int k = 0; k < KindCount; ++k)
        {
            Horse& h = mHorses[static_cast<size_t>(k)];
            h.Kind = k;
            const std::string base = Kinds[k].Model;
            if (!h.Rig.Load(root / (base + ".rig"), error)) return false;
            DoorwaysLoadedDMesh mesh;
            if (!LoadDoorwaysDMesh(root / (base + ".dmesh"), mesh, &error)) return false;
            if (mesh.Vertices.empty()) { error = base + ".dmesh has no vertices"; return false; }
            if (!renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(mesh, h.Mesh, &error)) return false;
            for (int t = 0; t < 2; ++t)
            {
                const auto path = root / (base + (t ? "Normal.png" : "BaseColor.png"));
                if (t && !std::filesystem::exists(path)) continue;
                DoorwaysTextureLoadDesc desc;
                desc.Path = path;
                desc.Semantic = t ? DoorwaysMaterialTextureSemantic::Normal : DoorwaysMaterialTextureSemantic::BaseColor;
                desc.GenerateMips = true;
                DoorwaysTextureLoadResult result;
                if (!renderer.LoadDoorwaysTexture(desc, result, &error)) return false;
                (t ? h.Normal : h.Texture) = result.SRV;
            }
            const HorseRig& r = h.Rig;
            h.Scale = Kinds[k].Seat / r.Seat.y;
            h.Centre = {(r.Hoof[0].x + r.Hoof[1].x + r.Hoof[2].x + r.Hoof[3].x) * .25f, 0, (r.Hoof[0].z + r.Hoof[1].z + r.Hoof[2].z + r.Hoof[3].z) * .25f};
            h.LegHigh = .5f * (r.Pivot[HorseRig::FrontUpL].y + r.Pivot[HorseRig::HindUpL].y);
            h.Nose = (r.Pivot[HorseRig::Head].z - h.Centre.z) * h.Scale + .45f;
            h.Haunch = (h.Centre.z - r.Pivot[HorseRig::Tail].z) * h.Scale + .1f;
            h.GrazeNext = 5.f + 3.f * static_cast<float>(k);
            h.SwishNext = 2.f + 1.3f * static_cast<float>(k);
            h.Palette.JointCount = HorseRig::JointCount;
            for (int j = 0; j < HorseRig::JointCount; ++j) XMStoreFloat4x4(&h.Palette.FinalSkinningMatrices[j], XMMatrixIdentity());
        }
        mLoaded = true;
        return true;
    }

    float NyxaraHorses::GroundAt(const WorldState& world, float x, float z, float near, float fallback) const
    {
        if (!world.RuntimeScene) return fallback;
        const auto g = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*world.RuntimeScene, x, z, near - 2.5f, near + 2.5f);
        return g.Hit ? g.GroundY : fallback;
    }

    void NyxaraHorses::Place(const WorldState& world, XMFLOAT3 at, float yaw, float apart)
    {
        const XMFLOAT3 right = RightOf(yaw);
        for (size_t i = 0; i < mHorses.size(); ++i)
        {
            Horse& h = mHorses[i];
            if (static_cast<int>(i) == mRidden) continue;
            h.Pos = Add(at, Mul(right, apart * static_cast<float>(i)));
            h.Pos.y = GroundAt(world, h.Pos.x, h.Pos.z, at.y, at.y);
            h.Yaw = h.TravelYaw = h.YawWas = yaw;
            h.Speed = h.Fwd = h.Side = 0; h.Called = false;
        }
    }

    // ---- the engine's moves while she rides: the horse's keys are the kit's to read
    void NyxaraHorses::Hold(bool on)
    {
        auto& held = GameSession().SuppressedActions;
        if (on)
        {
            for (const auto action : {DoorwaysInputAction::MoveForward, DoorwaysInputAction::MoveBackward, DoorwaysInputAction::MoveLeft,
                                      DoorwaysInputAction::MoveRight, DoorwaysInputAction::MoveFast, DoorwaysInputAction::MoveSlow})
                if (std::find(held.begin(), held.end(), action) == held.end()) { held.push_back(action); mHeld.push_back(action); }
        }
        else
        {
            for (const auto action : mHeld)
                if (const auto found = std::find(held.begin(), held.end(), action); found != held.end()) held.erase(found);
            mHeld.clear();
        }
    }

    void NyxaraHorses::Drop(WorldState& world)
    {
        if (mRide == Ride::None) return;
        if (mRidden >= 0)
        {
            Horse& h = mHorses[static_cast<size_t>(mRidden)];
            h.Pos = world.MainPlayer.Position; h.Speed = 0; h.Called = false;
        }
        mRide = Ride::None; mRidden = -1; mWantsDown = false;
        Hold(false);
        if (mFollowWas > 0) world.MainCamera.FollowDistance = mFollowWas;
        mFollowWas = -1;
    }

    void NyxaraHorses::Update(WorldState& world, const Controls& keys, float dt, std::wstring& notice)
    {
        mSparks.clear();
        if (!mLoaded || mHorses.empty()) return;
        auto& p = world.MainPlayer;
        auto& s = GameSession();
        const bool flying = p.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
        // ---- T (G): on, down, or called
        if (mAsked)
        {
            mAsked = false;
            if (mRide == Ride::Seated)
            {   // standing, she gets down; going, it is reined in first (T again: on)
                const Horse& h = mHorses[static_cast<size_t>(mRidden)];
                if (std::abs(h.Fwd) > 1.5f || !p.IsGrounded)
                {
                    mWantsDown = !mWantsDown;
                    notice = mWantsDown ? L"WHOA" : L"ON";
                }
                else mWantsDown = true;
            }
            else if (mRide == Ride::None)
            {
                int nearest = -1; float best = MountReach;
                for (size_t i = 0; i < mHorses.size(); ++i)
                {
                    const Horse& h = mHorses[i];
                    const float d = std::hypot(h.Pos.x - p.Position.x, h.Pos.z - p.Position.z);
                    if (d < best && std::abs(h.Pos.y - p.Position.y) < 1.5f) { best = d; nearest = static_cast<int>(i); }
                }
                if (flying || !p.IsGrounded) notice = L"ON THE GROUND ONLY";
                else if (nearest >= 0)
                {   // up: a leap into its saddle
                    mRidden = nearest; mKind = nearest; mWantsDown = false;
                    Horse& h = mHorses[static_cast<size_t>(nearest)];
                    mRide = Ride::Mounting; mRideTime = 0;
                    mFrom = p.Position; mFromYaw = p.Yaw; mTo = h.Pos;
                    h.Called = false; h.Speed = 0; h.Gear = 0; h.Spurred = 0; h.Graze = 0; h.GrazeTime = 0;
                    Hold(true);
                    mFollowWas = world.MainCamera.FollowDistance;
                    world.MainCamera.FollowDistance = (std::max)(mFollowWas, 6.2f);
                    CaucasianEagle::Sound::Play("nyxara/wing", .5f);
                    notice = std::wstring(KindName(nearest)) + L": " + WayName(mWay);
                }
                else
                {   // hers called: it comes to her side
                    Horse& h = mHorses[static_cast<size_t>(mKind)];
                    const float d = std::hypot(h.Pos.x - p.Position.x, h.Pos.z - p.Position.z);
                    if (d > 80)
                    {   // (too far to come: it is brought behind her, out of the view's way)
                        h.Pos = Add(p.Position, Mul(Ahead(p.Yaw), -25));
                        h.Pos.y = GroundAt(world, h.Pos.x, h.Pos.z, p.Position.y, p.Position.y);
                    }
                    h.Called = true; h.Graze = 0; h.GrazeTime = 0;
                    CaucasianEagle::Sound::Play("horse/whistle", .7f);
                    CaucasianEagle::Sound::Play("horse/neigh", .45f);
                    notice = std::wstring(KindName(mKind)) + L" COMES";
                }
            }
        }
        // ---- getting on and down: her place from where she was to where she goes, her pose blended (PoseRider)
        if (mRide == Ride::Mounting || mRide == Ride::Dismounting)
        {
            Horse& h = mHorses[static_cast<size_t>(mRidden)];
            const bool on = mRide == Ride::Mounting;
            const float span = on ? MountSeconds : DismountSeconds;
            mRideTime += dt;
            const float t = std::clamp(mRideTime / span, 0.f, 1.f), e = Ease(t);
            p.Position = {mFrom.x + (mTo.x - mFrom.x) * e, mFrom.y + (mTo.y - mFrom.y) * e, mFrom.z + (mTo.z - mFrom.z) * e};
            p.VerticalVelocity = 0;
            s.ImpulseX = s.ImpulseZ = 0;
            if (on) p.Yaw = Wrap(mFromYaw + Wrap(h.Yaw - mFromYaw) * e);
            mBlend.Weight = on ? e : 1 - e;
            mBlend.Arc = (on ? .55f : .35f) * std::sin(Pi * t);
            mBlend.Tuck = on ? .7f * std::sin(Pi * Smooth(.1f, .9f, t)) : .5f * std::sin(Pi * t);
            if (t >= 1)
            {
                if (on) { mRide = Ride::Seated; mSeatKnown = false; CaucasianEagle::Sound::Play("horse/snort", .5f); }
                else
                {
                    mRide = Ride::None; mRidden = -1;
                    Hold(false);
                    if (mFollowWas > 0) world.MainCamera.FollowDistance = mFollowWas;
                    mFollowWas = -1;
                }
                mBlend = {};
            }
        }
        // ---- each horse
        for (size_t i = 0; i < mHorses.size(); ++i)
        {
            Horse& h = mHorses[i];
            const bool ridden = static_cast<int>(i) == mRidden && mRide == Ride::Seated;
            if (ridden) Move(world, h, keys, dt, notice);
            else Wander(world, h, dt);
            Animate(h, dt, ridden);
            BuildPose(h);
        }
        mWingBounce = 0;
        if (mRidden >= 0) Seat(world, mHorses[static_cast<size_t>(mRidden)], dt);
        if (mWantsDown && mRide == Ride::Seated)
        {   // down on its left, or its right, or where it stands, once it stands
            Horse& h = mHorses[static_cast<size_t>(mRidden)];
            if (std::abs(h.Fwd) < 1.2f && p.IsGrounded)
            {
                mWantsDown = false;
                mFrom = p.Position;
                mTo = mFrom;
                for (const float side : {-1.3f, 1.3f})
                {
                    const XMFLOAT3 to = Add(p.Position, Mul(RightOf(h.Yaw), side));
                    if (!world.RuntimeScene) { mTo = to; break; }
                    const auto g = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*world.RuntimeScene, to.x, to.z, p.Position.y - 1.2f, p.Position.y + 1.2f);
                    if (g.Hit && !IsDoorwaysRuntimeSceneBlockedForPlayer(*world.RuntimeScene, {to.x, g.GroundY + .3f, to.z}, .35f, 1.4f))
                    {
                        mTo = {to.x, g.GroundY, to.z};
                        break;
                    }
                }
                mRide = Ride::Dismounting; mRideTime = 0;
                h.Pos = p.Position; h.Speed = 0;
                CaucasianEagle::Sound::Play("nyxara/land", .35f);
            }
        }
        // ---- the view: lifted to her as she sits
        const float lift = mRidden >= 0 ? (std::max)(0.f, Kinds[mHorses[static_cast<size_t>(mRidden)].Kind].Seat - 1.f) * mBlend.Weight : 0.f;
        mCamLift += (lift - mCamLift) * Toward(dt, 5);
        if (mCamLift > 1e-3f) s.CameraPivotHeight += mCamLift;
    }

    // ---- the horse she rides: her keys, its way of being ridden, its own nature
    void NyxaraHorses::Move(WorldState& world, Horse& h, const Controls& c, float dt, std::wstring& notice)
    {
        auto& p = world.MainPlayer;
        auto& s = GameSession();
        const KindSpec& k = Kinds[h.Kind];
        const float size = k.Seat / 1.5f;
        h.Clock += dt;
        h.Pos = p.Position;
        h.Grounded = p.IsGrounded;
        h.Vy = p.VerticalVelocity;
        const bool pressW = c.W && !h.Was.W, pressS = c.S && !h.Was.S, pressA = c.A && !h.Was.A, pressD = c.D && !h.Was.D;
        const bool pressShift = c.Shift && !h.Was.Shift, pressSpace = c.Space && !h.Was.Space;
        h.Was = c;
        // its paces
        const float canter = .5f * k.Top, trot = 3.9f * size, walk = 1.7f * size, back = -1.3f * size;
        if (h.Burst > 0) h.Burst = (std::max)(0.f, h.Burst - dt);
        h.BurstRest = (std::max)(0.f, h.BurstRest - dt);
        const float top = k.Top * (h.Burst > 0 ? 1.35f : 1.f);
        const float most = h.Winded ? canter : top;
        const float pace = std::clamp(std::abs(h.Speed) / k.Top, 0.f, 1.f);
        const int steer = (c.D ? 1 : 0) - (c.A ? 1 : 0);
        const float turnRate = (130.f - 85.f * pace) * Degree * k.Turn;
        float want = 0, turn = 0, side = 0, gather = 4.5f * k.Gather, lose = 5.5f;
        const bool rearing = h.RearTime >= 0;
        switch (mWay)
        {
        case Reins:
            want = c.W ? (c.Shift ? most : c.Ctrl ? walk : canter) : c.S ? (h.Speed > .4f ? 0.f : back) : 0.f;
            if (c.S && h.Speed > .4f) lose = 10;
            turn = static_cast<float>(steer) * turnRate;
            break;
        case Gaits:
        {
            if (pressW) h.Gear = (std::min)(h.Gear + 1, 4);
            if (pressS) h.Gear = (std::max)(h.Gear - 1, -1);
            const float gears[6] = {back, 0, walk, trot, canter, most};
            want = c.Shift ? most : gears[h.Gear + 1];
            if (pressW || pressS)
            {
                const wchar_t* named[6] = {L"BACK", L"STAND", L"WALK", L"TROT", L"CANTER", L"GALLOP"};
                notice = std::wstring(L"GAIT: ") + named[h.Gear + 1];
            }
            turn = static_cast<float>(steer) * turnRate;
            break;
        }
        case Look:
        {
            want = c.W ? (c.Shift ? most : c.Ctrl ? walk : canter) : c.S ? back : 0.f;
            if (c.W)
            {
                const float off = Wrap(c.ViewYaw - h.Yaw), rate = (170.f - 100.f * pace) * Degree * k.Turn;
                turn = std::clamp(off * 4.f, -rate, rate);
                if (std::abs(off) > 1.2f) want = (std::min)(want, trot);   // (round to the view first)
            }
            side = static_cast<float>(steer) * 1.7f * size * (std::abs(h.Speed) < trot ? 1.f : .4f);
            break;
        }
        case Spurs:
        {
            h.Spurred = (std::max)(0.f, h.Spurred - .05f * k.Top * dt);
            if (pressW)
            {
                // in time with its stride: as its leading fore comes down (in a canter or gallop), or any time slower
                const GaitSpec& g = GaitSpecs[h.Gait];
                const float at = Frac(h.Phase - g.Offset[1]);
                const bool inTime = h.Gait < Canter || at < .14f || at > .92f;
                if (h.Winded && h.Speed > canter * .9f)
                {
                    h.RearTime = 0; h.Spurred = 0;
                    notice = L"WINDED: IT REARS";
                    CaucasianEagle::Sound::Play("horse/neigh", .7f);
                }
                else
                {
                    h.Spurred = (std::min)(most, (std::max)(h.Spurred, h.Speed) + (inTime ? .16f : .09f) * k.Top * k.Gather);
                    if (h.Speed > canter) h.Wind -= .025f;
                    notice = inTime ? L"SPUR: IN TIME" : L"SPUR";
                }
            }
            if (c.S) h.Spurred = (std::max)(0.f, h.Spurred - 9.f * dt);
            want = c.S && h.Speed < .3f ? back : h.Spurred;
            if (c.S && h.Speed > .4f) lose = 9;
            gather = 9.f * k.Gather;
            lose = (std::max)(lose, 1.2f);
            turn = static_cast<float>(steer) * turnRate;
            break;
        }
        case Weight:
        {
            want = c.W ? (c.Shift ? most : c.Ctrl ? walk : canter) : 0.f;
            if (c.S && h.Speed < .3f) want = back * .7f;
            gather = 1.7f * k.Gather;
            lose = c.S ? 3.6f : .75f;
            const float lateral = 9.f * k.Turn;    // m/s2 it can turn with: at speed, a wide arc
            turn = static_cast<float>(steer) * (std::min)(85.f * Degree, lateral / (std::max)(std::abs(h.Speed), .1f));
            break;
        }
        case Wind:
        default:
        {
            gather = 16; lose = 14;
            want = c.W ? (c.Ctrl ? walk : most) : c.S && std::abs(h.Speed) < 3 ? back : 0.f;
            if (pressS && h.Speed > 3 && h.TurnAbout < 0) { h.TurnAbout = 0; h.TurnAboutWay = steer != 0 ? static_cast<float>(steer) : 1.f; }
            if (pressShift && h.BurstRest <= 0 && !h.Winded) { h.Burst = 1.2f; h.BurstRest = 3.5f; notice = L"BURST"; }
            if ((pressA && h.Clock - h.TapA < .3f) || (pressD && h.Clock - h.TapD < .3f))
            {
                h.AsideTime = 0; h.AsideWay = pressD ? 1.f : -1.f;
                if (p.IsGrounded) { p.VerticalVelocity = 3.4f; p.IsGrounded = false; p.GroundState = DoorwaysPhysicsGroundState::Airborne;
                                    p.MissingAuthoritativeGround = false; p.GroundY = p.TargetGroundY = p.Position.y; }
            }
            if (pressA) h.TapA = h.Clock;
            if (pressD) h.TapD = h.Clock;
            turn = static_cast<float>(steer) * (220.f - 120.f * pace) * Degree * k.Turn;
            if (h.TurnAbout >= 0)
            {   // round on its haunches: half a turn in 0.45 s, slowed and on again
                constexpr float Span = .45f;
                h.TurnAbout += dt;
                turn = h.TurnAboutWay * Pi / Span;
                want = (std::max)(want, .4f * h.Speed);
                if (h.TurnAbout >= Span) h.TurnAbout = -1;
            }
            if (h.AsideTime >= 0)
            {
                h.AsideTime += dt;
                side = h.AsideWay * 7.f * (1 - Smooth(.15f, .35f, h.AsideTime));
                if (h.AsideTime > .35f) h.AsideTime = -1;
            }
            break;
        }
        }
        if (mWantsDown) { want = 0; side = 0; lose = (std::max)(lose, 9.f); h.Spurred = 0; h.Gear = 0; h.Burst = 0; }   // (T at speed: reined in)
        // its nature: the mountain horse is no slower uphill; the others are, on a steep way
        if (h.Kind != Tushetian && want > walk && h.Slope > .2f) want *= 1 - .55f * Smooth(.2f, .55f, h.Slope);
        if (rearing) { want = 0; turn = 0; side = 0; }
        if (!h.Grounded) { turn *= .3f; }
        // its speed
        if (h.Grounded || mWay == Wind)
        {
            const float rate = want > h.Speed ? (h.Speed < 0 ? lose : gather) : (h.Speed > 0 ? lose : gather * .6f);
            h.Speed = want > h.Speed ? (std::min)(want, h.Speed + rate * dt) : (std::max)(want, h.Speed - rate * dt);
        }
        {   // run into something, it is stopped
            const float measured = std::hypot(p.ActualVelocity.x, p.ActualVelocity.z);
            h.Blocked = h.Grounded && std::abs(h.Speed) > 2.5f && measured < .45f * std::abs(h.Speed) ? h.Blocked + dt : 0.f;
            if (h.Blocked > .25f) { h.Speed = std::copysign(measured, h.Speed); h.Spurred = (std::min)(h.Spurred, measured); h.Blocked = 0; }
        }
        // its turn and its slide (the heavy and the frost horse slide wide: their going lags their facing)
        h.Yaw = Wrap(h.Yaw + turn * dt);
        const float slide = mWay == Weight ? (h.Kind == Frost ? 1.3f : 2.2f) : h.Kind == Frost ? 3.f : 0.f;
        if (slide > 0 && std::abs(h.Speed) > 2.5f) h.TravelYaw = Wrap(h.TravelYaw + Wrap(h.Yaw - h.TravelYaw) * Toward(dt, slide * k.Turn));
        else h.TravelYaw = h.Yaw;
        {
            const float drift = std::abs(Wrap(h.Yaw - h.TravelYaw));
            if (drift > .18f && h.Grounded && k.Spark.w > 0 && std::fmod(h.Clock, .08f) < dt)
                for (int leg = 2; leg < 4; ++leg) mSparks.push_back({Point(h.HoofAt[leg], XMLoadFloat4x4(&h.World)), k.Spark, 2, 1.2f, .12f});
        }
        // its wind
        if (std::abs(h.Speed) > canter * 1.08f) h.Wind -= dt / k.Wind;
        else if (std::abs(h.Speed) < trot * 1.1f) h.Wind += dt / 7;
        else if (h.Kind == Kabardian) h.Wind += dt / 14;
        else h.Wind += dt / 28;
        h.Wind = std::clamp(h.Wind, 0.f, 1.f);
        if (!h.Winded && h.Wind <= 0) { h.Winded = true; notice = L"WINDED"; CaucasianEagle::Sound::Play("horse/snort", .7f); }
        if (h.Winded && h.Wind > .35f) h.Winded = false;
        // a leap; the winged one's glide and beat
        if (pressSpace && h.Grounded && !rearing)
        {
            const float up = k.Leap * (mWay == Wind ? 1.2f : 1.f) * (std::abs(h.Speed) < 1 ? .8f : 1.f);
            p.VerticalVelocity = up; p.IsGrounded = false;
            p.GroundState = DoorwaysPhysicsGroundState::Airborne;
            p.MissingAuthoritativeGround = false; p.GroundY = p.TargetGroundY = p.Position.y;
            h.LeapTime = 0; h.AirBeats = k.Wings ? 1 : 0; h.Grounded = false;
            CaucasianEagle::Sound::Play("horse/hoof", .8f);
            if (k.Wings) CaucasianEagle::Sound::Play("nyxara/wing", .7f);
        }
        else if (pressSpace && !h.Grounded && h.AirBeats > 0)
        {
            --h.AirBeats;
            p.VerticalVelocity = (std::max)(p.VerticalVelocity, 5.5f);
            h.WingBeat = 1;
            CaucasianEagle::Sound::Play("nyxara/wing", .8f);
        }
        h.Gliding = k.Wings && !h.Grounded && c.Space && p.VerticalVelocity < 0;
        if (h.Gliding) p.VerticalVelocity = (std::max)(p.VerticalVelocity, -2.2f);
        if (h.LeapTime >= 0) h.LeapTime += dt;
        // its head at a wall (its haunches, backing): it stops there. The engine stops her body; its front would go on into it.
        // (in a leap too: 2026-11-27, a leap at the gallop carried its head into the arena's wall)
        if (world.RuntimeScene && std::abs(h.Speed) > .2f)
        {
            const float dir = h.Speed > 0 ? 1.f : -1.f, reach = dir > 0 ? h.Nose : h.Haunch;
            const XMFLOAT3 f = Ahead(h.TravelYaw);
            const XMFLOAT3 probe{p.Position.x + f.x * reach * dir, p.Position.y + .45f, p.Position.z + f.z * reach * dir};
            if (IsDoorwaysRuntimeSceneBlockedForPlayer(*world.RuntimeScene, probe, .3f, 1.1f))
            {
                if (std::abs(h.Speed) > 6) CaucasianEagle::Sound::Play("horse/snort", .6f);
                h.Speed = 0; h.Spurred = 0; h.Gear = (std::min)(h.Gear, 0); h.Burst = 0;
            }
        }
        // its going: the engine moves her (and it with her) by it
        const XMFLOAT3 go = Add(Mul(Ahead(h.TravelYaw), h.Speed), Mul(RightOf(h.Yaw), side));
        s.ImpulseX = go.x; s.ImpulseZ = go.z;
        if (s.LongMovePiece <= 0 && std::abs(h.Speed) > 6) s.LongMovePiece = .25f;   // (fast: up a climb as on the level)
        p.Yaw = h.Yaw;
        {   // as measured (for its stride): along its facing and to its right
            const XMFLOAT3 f = Ahead(h.Yaw), r = RightOf(h.Yaw);
            const float fwd = p.ActualVelocity.x * f.x + p.ActualVelocity.z * f.z, aside = p.ActualVelocity.x * r.x + p.ActualVelocity.z * r.z;
            const float k12 = Toward(dt, 12);
            h.Fwd += ((std::abs(fwd) > 60 ? h.Fwd : fwd) - h.Fwd) * k12;
            h.Side += ((std::abs(aside) > 60 ? h.Side : aside) - h.Side) * k12;
        }
        // the view behind it, as it turns (not while the mouse turns the view, nor in LOOK, where the view steers it)
        if (mWay != Look && !s.Unattended)
        {
            if (mViewSet < 1e8f && std::abs(Wrap(world.MainCamera.Yaw - mViewSet)) > 1e-4f) mMouseRest = 1.2f;
            mMouseRest = (std::max)(0.f, mMouseRest - dt);
            if (mMouseRest <= 0 && std::abs(h.Speed) > 1.5f)
                world.MainCamera.Yaw = Wrap(world.MainCamera.Yaw + Wrap(h.Yaw - world.MainCamera.Yaw) * Toward(dt, .6f + 1.6f * pace));
            mViewSet = world.MainCamera.Yaw;
        }
        else mViewSet = 1e9f;
        // its slope ahead (for its body, and its pace uphill)
        if (h.Grounded)
        {
            const XMFLOAT3 f = Ahead(h.Yaw);
            const float half = .5f * std::abs(h.Rig.Hoof[0].z + h.Rig.Hoof[1].z - h.Rig.Hoof[2].z - h.Rig.Hoof[3].z) * .5f * h.Scale;
            const float front = GroundAt(world, h.Pos.x + f.x * half, h.Pos.z + f.z * half, h.Pos.y, h.Pos.y);
            const float hind = GroundAt(world, h.Pos.x - f.x * half, h.Pos.z - f.z * half, h.Pos.y, h.Pos.y);
            const float slope = std::clamp(std::atan2(front - hind, 2 * half), -.6f, .6f);
            h.Slope += (slope - h.Slope) * Toward(dt, 8);
        }
        else h.Slope += (0 - h.Slope) * Toward(dt, 2);
    }

    // ---- a horse she does not ride: standing, grazing, or coming to her
    void NyxaraHorses::Wander(WorldState& world, Horse& h, float dt)
    {
        const auto& p = world.MainPlayer;
        const KindSpec& k = Kinds[h.Kind];
        const float size = k.Seat / 1.5f;
        h.Clock += dt;
        h.Grounded = true; h.Vy = 0; h.LeapTime = -1; h.Gliding = false;
        const bool held = static_cast<size_t>(mRidden) < mHorses.size() && &mHorses[static_cast<size_t>(mRidden)] == &h;   // (she gets on or down)
        float want = 0, turn = 0;
        if (h.Called && !held)
        {   // to her side (her left), facing as she faces
            const XMFLOAT3 goal = Add(p.Position, Mul(RightOf(p.Yaw), -1.9f));
            const float dx = goal.x - h.Pos.x, dz = goal.z - h.Pos.z, d = std::hypot(dx, dz);
            const float face = d > 1.2f ? std::atan2(dx, dz) : p.Yaw;
            turn = std::clamp(Wrap(face - h.Yaw) * 3.f, -2.2f, 2.2f);
            // (a gallop from afar, a canter near, a trot to its last two metres: 2026-11-27, walking its last six it took 9 s
            // to come 25 m)
            want = d < .35f ? 0.f : d > 14 ? .55f * k.Top : d > 6 ? 7.5f * size : d > 2 ? 3.9f * size : (std::min)(1.7f * size, d * 1.5f);
            if (std::abs(Wrap(face - h.Yaw)) > 1.4f) want = (std::min)(want, 1.2f * size);
            if (d < .35f && std::abs(Wrap(p.Yaw - h.Yaw)) < .1f) { h.Called = false; CaucasianEagle::Sound::Play("horse/snort", .4f); }
        }
        const float rate = want > h.Speed ? 4.f : 5.f;
        h.Speed = want > h.Speed ? (std::min)(want, h.Speed + rate * dt) : (std::max)(want, h.Speed - rate * dt);
        h.Yaw = Wrap(h.Yaw + turn * dt);
        h.TravelYaw = h.Yaw;
        if (!held && std::abs(h.Speed) > 1e-3f)
        {
            const XMFLOAT3 f = Ahead(h.Yaw);
            h.Pos.x += f.x * h.Speed * dt; h.Pos.z += f.z * h.Speed * dt;
            const float y = GroundAt(world, h.Pos.x, h.Pos.z, h.Pos.y, h.Pos.y);
            h.Pos.y += (y - h.Pos.y) * Toward(dt, 14);
        }
        h.Fwd = h.Speed; h.Side = 0;
        h.Wind = (std::min)(1.f, h.Wind + dt / 7);
        if (h.Winded && h.Wind > .35f) h.Winded = false;
        h.Slope += (0 - h.Slope) * Toward(dt, 3);
    }

    // ---- its stride and its body: from how it goes
    void NyxaraHorses::Animate(Horse& h, float dt, bool ridden)
    {
        const KindSpec& k = Kinds[h.Kind];
        const float size = k.Seat / 1.5f;
        const float speed = std::hypot(h.Fwd, h.Side);
        h.TurnRate += ((dt > 1e-5f ? Wrap(h.Yaw - h.YawWas) / dt : 0.f) - h.TurnRate) * Toward(dt, 10);
        h.YawWas = h.Yaw;
        const bool air = !h.Grounded;
        if (air) h.AirTime += dt;
        else
        {
            if (h.AirTime > .25f)
            {   // down from a leap
                h.Land = 1; h.LeapTime = -1;
                CaucasianEagle::Sound::Play("horse/hoof", .9f);
                CaucasianEagle::Sound::Play("horse/hoof", .7f);
                if (k.Spark.w > 0)
                    for (int leg = 0; leg < 4; ++leg) mSparks.push_back({Point(h.HoofAt[leg], XMLoadFloat4x4(&h.World)), k.Spark, h.Kind == Fire || h.Kind == Frost ? 9 : 4, 3.f, .2f});
            }
            h.AirTime = 0;
        }
        h.Land = (std::max)(0.f, h.Land - dt * 3.5f);
        // its gait (by its speed, the way up a little faster than the way down)
        const bool turning = speed < .5f && std::abs(h.TurnRate) > .35f;
        if (!air)
        {
            const float up[4] = {.15f, 2.7f * size, 5.6f * size, .62f * k.Top};
            int g = h.Gait;
            while (g < Gallop && speed > up[g]) ++g;
            while (g > Stand && speed < up[g - 1] - (g == Walk ? .05f : g == Gallop ? .04f * k.Top : .5f * size)) --g;
            if (g == Stand && turning) g = Walk;
            h.Gait = g;
        }
        float total = 0;
        for (int g = 0; g < GaitCount; ++g)
        {
            h.Weight[g] += ((g == h.Gait ? 1.f : 0.f) - h.Weight[g]) * Toward(dt, 6);
            total += h.Weight[g];
        }
        for (float& w : h.Weight) w /= (std::max)(total, 1e-4f);
        // its stride's rate
        float rate = 0;
        for (int g = 0; g < GaitCount; ++g)
        {
            const GaitSpec& gs = GaitSpecs[g];
            float f = gs.Rate;
            if (g > Stand)
            {
                f = gs.Rate * std::clamp(std::pow((std::max)(speed, .05f) / (gs.Speed * size), .35f), .55f, 1.4f);
                f = (std::max)(f, speed * gs.Duty / (MaxStride * size));
            }
            rate += h.Weight[g] * f;
        }
        h.Phase = Frac(h.Phase + rate * dt * (air ? .35f : 1.f));
        // its rear (SPURS: a winded horse spurred)
        if (h.RearTime >= 0)
        {
            h.RearTime += dt;
            h.Rear = Smooth(0, .35f, h.RearTime) * (1 - Smooth(.85f, 1.3f, h.RearTime));
            if (h.RearTime > 1.3f) { h.RearTime = -1; h.Rear = 0; }
        }
        // its head and tail at rest: grazing now and then (not with her on it), a swish of its tail
        const bool still = h.Weight[Stand] > .6f && !air;
        if (still && !ridden && !h.Called)
        {
            h.GrazeTime += dt;
            const float graze = h.GrazeTime > h.GrazeNext ? 1.f : 0.f;
            h.Graze += (graze - h.Graze) * Toward(dt, 1.6f);
            if (h.GrazeTime > h.GrazeNext + 5.f) { h.GrazeTime = 0; h.GrazeNext = 6.f + std::fmod(h.Clock * 7.3f, 9.f); }
        }
        else { h.GrazeTime = 0; h.Graze += (0 - h.Graze) * Toward(dt, 3); }
        h.SwishNext -= dt;
        if (h.SwishNext <= 0) { h.SwishGo += 5.f; h.SwishNext = 2.5f + std::fmod(h.Clock * 3.7f, 5.f); }
        for (float left = dt; left > 1e-6f;)
        {
            const float step = (std::min)(left, 1 / 120.f); left -= step;
            h.SwishGo += (-60.f * h.Swish - 6.f * h.SwishGo) * step;
            h.Swish += h.SwishGo * step;
        }
        // its body over the stride
        float bob = 0, rock = 0, nod = 0, flex = 0;
        for (int g = Walk; g < GaitCount; ++g)
        {
            const GaitSpec& gs = GaitSpecs[g];
            const float w = h.Weight[g] * (std::min)(1.f, speed / (gs.Speed * size)) * (air ? 0.f : 1.f);
            bob += w * -gs.Bob * size * std::cos(2 * Pi * gs.BobBeats * (h.Phase - gs.BobAt));
            rock += w * gs.Rock * std::cos(2 * Pi * (h.Phase - gs.RockAt));
            nod += w * gs.Nod * std::cos(2 * Pi * gs.NodBeats * (h.Phase - gs.NodAt));
            flex += w * gs.Flex * std::sin(2 * Pi * (h.Phase - gs.FlexAt));
        }
        // a leap: up nose first, level, the nose down to come down; a landing's weight
        float leap = 0;
        if (air && h.LeapTime >= 0) leap = .28f * (1 - Smooth(.05f, .35f, h.LeapTime)) - .12f * Smooth(-1.f, -5.f, h.Vy);
        else if (air) leap = -.1f * Smooth(-1.f, -5.f, h.Vy);
        bob -= .09f * size * h.Land * (1 - h.Land) * 4;
        h.Bob = bob / h.Scale;
        h.Pitch = rock + leap;
        h.Nod = nod - .35f * h.Rear - .3f * leap;
        h.Flex = flex;
        // lean into its turn, as fast as it goes
        const float lean = std::clamp(-std::atan(h.Fwd * h.TurnRate / 9.81f) * (mWay == Weight && ridden ? 1.1f : .75f), -.32f, .32f);
        h.Roll += (lean - h.Roll) * Toward(dt, 6);
        h.Look += (std::clamp(h.TurnRate * .3f, -.5f, .5f) + .15f * std::sin(h.Clock * .37f) * h.Weight[Stand] - h.Look) * Toward(dt, 4);
        // its tail: swung with its stride, lifted as it gallops, streaming in a leap
        const float run = h.Weight[Canter] * .5f + h.Weight[Gallop];
        h.TailSwing = .5f * h.Swish + .12f * std::sin(2 * Pi * h.Phase) * (1 - h.Weight[Stand]);
        h.TailLift += ((.35f * run + (air ? .5f : 0.f)) - h.TailLift) * Toward(dt, 4);
        // the winged one's wings: folded at rest, half open at a gallop, open in the air; a beat as it leaps and as asked
        if (k.Wings)
        {
            const float open = air ? 1.f : h.Rear > .2f ? .8f : .3f * run;
            h.WingOpen += (open - h.WingOpen) * Toward(dt, air ? 7.f : 3.f);
            const bool beating = air && (h.Vy > 0 || h.WingBeat > .05f) && !h.Gliding;
            h.WingBeatPhase = Frac(h.WingBeatPhase + dt * 2.3f);
            const float amp = beating ? 1.f : h.Gliding ? .08f : air ? .25f : .12f * run;
            h.WingBeat = (std::max)(0.f, h.WingBeat - dt * 1.5f);
            h.WingAmp = amp;
        }
        // its hooves (in its pose's frame: the ground at y = 0; leaned into a turn, the ground tilts under them)
        const HorseRig& r = h.Rig;
        const float frontZ = .5f * (r.Hoof[0].z + r.Hoof[1].z), hindZ = .5f * (r.Hoof[2].z + r.Hoof[3].z);
        const float tilt = std::tan(h.Roll);
        int lead = Stand;
        for (int g = Walk; g < GaitCount; ++g) if (h.Weight[g] > (lead == Stand ? .25f : h.Weight[lead])) lead = g;
        for (int leg = 0; leg < 4; ++leg)
        {
            const bool front = leg < 2;
            float dx = 0, dy = 0, dz = 0, flexed = 0;
            for (int g = Walk; g < GaitCount; ++g)
            {
                const float w = h.Weight[g];
                if (w < 1e-3f) continue;
                const GaitSpec& gs = GaitSpecs[g];
                const float f = (std::max)(gs.Rate * .55f, (std::max)(speed, .05f) * gs.Duty / (MaxStride * size));
                const float fr = (std::max)(f, gs.Rate * std::clamp(std::pow((std::max)(speed, .05f) / (gs.Speed * size), .35f), .55f, 1.4f));
                const float strideZ = h.Fwd * gs.Duty / fr / h.Scale, strideX = h.Side * gs.Duty / fr / h.Scale;
                const float lift = (front ? gs.LiftFront : gs.LiftHind) * size / h.Scale * Smooth(0.f, .8f, speed + (turning ? 1.f : 0.f));
                const float p = Frac(h.Phase - gs.Offset[leg]);
                float u, along, up, bend;
                if (p < gs.Duty) { u = p / gs.Duty; along = .5f - u; up = 0; bend = 0; }
                else
                {
                    u = (p - gs.Duty) / (1 - gs.Duty);
                    along = -.5f + Ease(u);
                    up = std::sin(Pi * (front ? std::pow(u, .8f) : u));
                    bend = std::sin(Pi * std::pow(u, .7f));
                }
                dz += w * along * strideZ;
                dx += w * along * strideX;
                dy += w * up * lift;
                flexed += w * bend * (front ? 1.5f : 1.05f) * Smooth(0.f, .8f, speed + (turning ? 1.f : 0.f));
            }
            XMFLOAT3 at{r.Hoof[leg].x + dx, 0, (front ? frontZ : hindZ) + dz};
            at.y = dy - (at.x - h.Centre.x) * tilt;
            // in the air: tucked under it, then the forelegs reaching down for the ground
            if (air)
            {
                const float a = Smooth(.03f, .18f, h.AirTime);
                const float reach = Smooth(-1.f, -4.f, h.Vy) * (front ? 1.f : .4f);
                const XMFLOAT3 tuck = front ? XMFLOAT3{r.Hoof[leg].x, .42f * h.LegHigh, frontZ - .12f * h.LegHigh}
                                            : XMFLOAT3{r.Hoof[leg].x, .32f * h.LegHigh, hindZ + .18f * h.LegHigh};
                const XMFLOAT3 down = front ? XMFLOAT3{r.Hoof[leg].x, .05f * h.LegHigh, frontZ + .25f * h.LegHigh}
                                            : XMFLOAT3{r.Hoof[leg].x, .1f * h.LegHigh, hindZ - .05f * h.LegHigh};
                const XMFLOAT3 held{tuck.x + (down.x - tuck.x) * reach, tuck.y + (down.y - tuck.y) * reach, tuck.z + (down.z - tuck.z) * reach};
                at = {at.x + (held.x - at.x) * a, at.y + (held.y - at.y) * a, at.z + (held.z - at.z) * a};
                flexed = flexed + ((front ? 1.7f : 1.2f) * (1 - reach) - flexed) * a;
            }
            if (front && h.Rear > 0) flexed += (1.6f - flexed) * h.Rear;   // (reared: its forelegs off the ground, BuildPose)
            h.HoofAt[leg] = at;
            h.Flexed[leg] = flexed;
            // a hoof coming down: its sound, and the fire's and the frost's sparks
            const GaitSpec& ls = GaitSpecs[lead];
            const float p = lead == Stand ? 0.f : Frac(h.Phase - ls.Offset[leg]);
            const bool down = lead != Stand && !air && p < ls.Duty;
            if (down && !h.Down[leg] && speed > .3f)
            {
                const float loud = (.18f + .5f * std::clamp(speed / k.Top, 0.f, 1.f)) * (ridden ? 1.f : .5f);
                CaucasianEagle::Sound::Play("horse/hoof", loud, .12f);
                if (k.Spark.w > 0 && (h.Kind == Fire || h.Kind == Frost || h.Gait >= Canter))
                    mSparks.push_back({Point(at, XMLoadFloat4x4(&h.World)), k.Spark, h.Gait >= Canter ? 4 : 2, 1.6f, .13f});
            }
            h.Down[leg] = down;
        }
    }

    // ---- its joints: the palette the engine skins it with, and where it is in the world
    void NyxaraHorses::BuildPose(Horse& h)
    {
        const HorseRig& r = h.Rig;
        const KindSpec& k = Kinds[h.Kind];
        constexpr int J = HorseRig::JointCount;
        XMMATRIX local[J];
        for (auto& m : local) m = XMMatrixIdentity();
        // its body: risen and rocked about its middle; reared about its hind hooves
        {
            const XMFLOAT3& pv = r.Pivot[HorseRig::Pelvis];
            XMMATRIX body = About(pv, XMMatrixRotationX(-h.Pitch)) * XMMatrixTranslation(0, h.Bob, 0);
            if (h.Rear > 0)
            {
                const XMFLOAT3 hind{0, 0, .5f * (r.Hoof[2].z + r.Hoof[3].z)};
                body = body * About(hind, XMMatrixRotationX(-h.Rear * .8f));
            }
            local[HorseRig::Pelvis] = body;
        }
        local[HorseRig::Chest] = About(r.Pivot[HorseRig::Chest], XMMatrixRotationX(h.Flex));
        const float neck = h.Nod + .95f * h.Graze;
        local[HorseRig::Neck] = About(r.Pivot[HorseRig::Neck], XMMatrixRotationX(.55f * neck) * XMMatrixRotationY(.4f * h.Look));
        local[HorseRig::Neck2] = About(r.Pivot[HorseRig::Neck2], XMMatrixRotationX(.45f * neck) * XMMatrixRotationY(.35f * h.Look));
        local[HorseRig::Head] = About(r.Pivot[HorseRig::Head], XMMatrixRotationX(.25f * h.Graze - .2f * h.Nod) * XMMatrixRotationY(.25f * h.Look));
        local[HorseRig::Tail] = About(r.Pivot[HorseRig::Tail], XMMatrixRotationX(h.TailLift) * XMMatrixRotationY(h.TailSwing));   // (lifted: up and back)
        local[HorseRig::Tail2] = About(r.Pivot[HorseRig::Tail2], XMMatrixRotationX(.5f * h.TailLift) * XMMatrixRotationY(.8f * h.TailSwing));
        if (k.Wings)
        {   // each wing from as it was made (raised) to folded along its flank, or spread level, or up and down in a beat
            for (int side = 0; side < 2; ++side)
            {
                const int root = side ? HorseRig::WingR : HorseRig::WingL, tip = side ? HorseRig::WingR2 : HorseRig::WingL2;
                const float out = side ? 1.f : -1.f;
                const XMVECTOR made = XMVector3Normalize(XMLoadFloat3(&r.Pivot[tip]) - XMLoadFloat3(&r.Pivot[root]));
                const auto turnTo = [&](float x, float y, float z) {
                    const XMVECTOR to = XMVector3Normalize(XMVectorSet(out * x, y, z, 0));
                    const XMVECTOR axis = XMVector3Cross(made, to);
                    const float sn = XMVectorGetX(XMVector3Length(axis)), cs = XMVectorGetX(XMVector3Dot(made, to));
                    return sn < 1e-5f ? XMQuaternionIdentity() : XMQuaternionRotationAxis(axis / sn, std::atan2(sn, cs));
                };
                const XMVECTOR fold = turnTo(.3f, -.25f, -1.f), spread = turnTo(1.f, .1f, -.25f), up = turnTo(.4f, 1.f, -.2f), down = turnTo(1.f, -.5f, -.1f);
                const float beat = std::sin(2 * Pi * h.WingBeatPhase) * (std::max)(h.WingAmp, h.WingBeat);
                const XMVECTOR open = beat >= 0 ? XMQuaternionSlerp(spread, up, beat) : XMQuaternionSlerp(spread, down, -beat);
                const XMVECTOR q = XMQuaternionSlerp(fold, open, h.WingOpen);
                local[root] = About(r.Pivot[root], XMMatrixRotationQuaternion(q));
                const float lag = -.35f * std::sin(2 * Pi * h.WingBeatPhase - .9f) * (std::max)(h.WingAmp, h.WingBeat) * h.WingOpen;
                const float tuck = (1 - h.WingOpen) * .5f;
                local[tip] = About(r.Pivot[tip], XMMatrixRotationZ(out * lag) * XMMatrixRotationY(-out * tuck));
            }
        }
        // the body's joints first (the legs below need their parents)
        XMMATRIX global[J];
        for (int n = 0; n < J; ++n)
        {
            const int j = r.Order[n];
            global[j] = r.Parent[j] >= 0 ? local[j] * global[r.Parent[j]] : local[j];
        }
        // its legs: each hoof where it is put, its leg bent to it (its top, its knee or hock, its fetlock)
        for (int leg = 0; leg < 4; ++leg)
        {
            const int top = LegJoints[leg][0], mid = LegJoints[leg][1], low = LegJoints[leg][2];
            const bool front = leg < 2;
            const XMMATRIX parent = global[r.Parent[top]];
            if (front && h.Rear > 0)
            {   // reared: its forelegs off the ground under its shoulders, pawing
                const float paw = std::sin(h.Clock * 9.f + static_cast<float>(leg) * 1.7f);
                const XMFLOAT3 shoulder = Point(r.Pivot[top], parent);
                const XMFLOAT3 up{shoulder.x, shoulder.y - (.55f - .1f * paw) * h.LegHigh, shoulder.z + .25f * h.LegHigh};
                XMFLOAT3& at = h.HoofAt[leg];
                at = {at.x + (up.x - at.x) * h.Rear, at.y + (up.y - at.y) * h.Rear, at.z + (up.z - at.z) * h.Rear};
            }
            const XMFLOAT3 goal = Point(h.HoofAt[leg], XMMatrixInverse(nullptr, parent));   // (in its rest frame)
            const XMFLOAT3 &A = r.Pivot[top], &B = r.Pivot[mid], &C = r.Pivot[low], &H = r.Hoof[leg];
            const float l1 = std::hypot(B.y - A.y, B.z - A.z), l2 = std::hypot(C.y - B.y, C.z - B.z);
            const float pastern = std::hypot(H.y - C.y, H.z - C.z);
            const float rest1 = std::atan2(B.z - A.z, B.y - A.y), rest2 = std::atan2(C.z - B.z, C.y - B.y), rest3 = std::atan2(H.z - C.z, H.y - C.y);
            const float way3 = rest3 + h.Flexed[leg];
            const float fy = goal.y - pastern * std::cos(way3), fz = goal.z - pastern * std::sin(way3);
            const float dy = fy - A.y, dz = fz - A.z;
            const float len = std::clamp(std::hypot(dy, dz), std::abs(l1 - l2) + 1e-4f, (l1 + l2) * .999f);
            const float toward = std::atan2(dz, dy);
            const float bend = std::acos(std::clamp((l1 * l1 + len * len - l2 * l2) / (2 * l1 * len), -1.f, 1.f));
            const float way1 = front ? toward - bend : toward + bend;   // (its knee forward; its hock back)
            const float by = A.y + l1 * std::cos(way1), bz = A.z + l1 * std::sin(way1);
            const float ey = A.y + len * std::cos(toward), ez = A.z + len * std::sin(toward);
            const float way2 = std::atan2(ez - bz, ey - by);
            const float t1 = Wrap(way1 - rest1), t2 = Wrap(way2 - rest2 - t1), t3 = Wrap(way3 - rest3 - t1 - t2);
            const float aside = std::atan2(goal.x - H.x, (std::max)(.2f, A.y - goal.y));
            local[top] = About(A, XMMatrixRotationX(t1) * XMMatrixRotationZ(aside));
            local[mid] = About(B, XMMatrixRotationX(t2));
            local[low] = About(C, XMMatrixRotationX(t3));
            global[top] = local[top] * parent;
            global[mid] = local[mid] * global[top];
            global[low] = local[low] * global[mid];
        }
        h.Palette.JointCount = J;
        for (int j = 0; j < J; ++j) { XMStoreFloat4x4(&h.Palette.FinalSkinningMatrices[j], global[j]); XMStoreFloat4x4(&h.Global[j], global[j]); }
        // where it is: its hooves' middle on its place, its size, leaned into its turn, pitched with the ground, turned
        const XMMATRIX world = XMMatrixTranslation(-h.Centre.x, 0, -h.Centre.z) * XMMatrixScaling(h.Scale, h.Scale, h.Scale) * XMMatrixRotationZ(h.Roll) *
                               XMMatrixRotationX(-h.Slope) * XMMatrixRotationY(h.Yaw) * XMMatrixTranslation(h.Pos.x, h.Pos.y, h.Pos.z);
        XMStoreFloat4x4(&h.World, world);
    }

    // ---- her seat on it this frame (for her pose)
    void NyxaraHorses::Seat(const WorldState& world, Horse& h, float dt)
    {
        const HorseRig& r = h.Rig;
        const XMMATRIX w = XMLoadFloat4x4(&h.World);
        const XMMATRIX pelvis = XMLoadFloat4x4(&h.Global[HorseRig::Pelvis]) * w, chest = XMLoadFloat4x4(&h.Global[HorseRig::Chest]) * w;
        const XMFLOAT3 a = Point(r.Seat, pelvis), b = Point(r.Seat, chest);
        auto& sd = mSaddle;
        sd.Seat = {.5f * (a.x + b.x), .5f * (a.y + b.y), .5f * (a.z + b.z)};
        sd.Forward = Dir({0, 0, 1}, pelvis);
        sd.Up = Dir({0, 1, 0}, pelvis);
        sd.Right = Dir({1, 0, 0}, pelvis);
        for (int s = 0; s < 2; ++s) sd.Stirrup[s] = Point(r.Stirrup[s], chest);
        sd.Yaw = world.MainPlayer.Yaw;
        // her body: leaned as the gait has her, back as it stops, into a leap and back as it comes down; her knees taking
        // its rise and fall as the gait has them, out of the seat at a gallop, rising at the trot
        float lean = 0, absorb = 0;
        for (int g = 0; g < GaitCount; ++g) { lean += h.Weight[g] * GaitSpecs[g].Lean; absorb += h.Weight[g] * GaitSpecs[g].Absorb; }
        const float braking = std::clamp((h.Fwd - h.Speed) / 6.f, 0.f, 1.f);
        if (h.LeapTime >= 0) lean += .2f * (1 - Smooth(.1f, .4f, h.LeapTime));
        lean += -.22f * braking - .12f * h.Land - .4f * h.Rear;
        sd.Lean = lean;
        const float post = .07f * h.Weight[Trot] * (std::max)(0.f, std::sin(2 * Pi * h.Phase));
        sd.Rise = .09f * h.Weight[Gallop] + post - absorb * h.Bob * h.Scale;
        const XMFLOAT3 at = Add(sd.Seat, Mul(sd.Up, sd.Rise));
        for (int s = 0; s < 2; ++s)
        {   // (her hands low over its withers and near together: 2026-11-27, 34 cm on and 30 up they looked held out)
            const float out = s ? 1.f : -1.f;
            sd.Rein[s] = Add(Add(Add(at, Mul(sd.Forward, .28f + .1f * lean)), Mul(sd.Up, .24f - .08f * lean)), Mul(sd.Right, .09f * out));
        }
        const float high = at.y;   // (her hips' rise and fall, m/s: her wings spring with it)
        mWingBounce = mSeatKnown && dt > 1e-4f ? std::clamp((high - mSeatWas) / dt, -6.f, 6.f) : 0.f;
        mSeatWas = high; mSeatKnown = true;
    }

    void NyxaraHorses::PoseRider(const WorldState& world, const DoorwaysSkeleton& skeleton, DoorwaysSkeletonPose& pose, const DoorwaysSkeletonPose* grip)
    {
        if (!mPoserTried) { mPoserTried = true; mPoser.Bind(skeleton); }
        if (!mPoser.Bound() || mRidden < 0 || mRide == Ride::None) return;
        const auto& p = world.MainPlayer;
        mSaddle.Yaw = p.Yaw;
        NyxaraRide::Blend blend = mBlend;
        if (mRide == Ride::Seated) blend = {};
        const XMMATRIX picture = BuildDoorwaysCharacterWorldMatrix(p.Position, p.Yaw, world.AnimatedPlayerVisual);
        mPoser.Pose(mSaddle, blend, picture, pose, grip);
    }

    void NyxaraHorses::Draw(Renderer& renderer, const XMMATRIX& viewProjection)
    {
        if (!mLoaded) return;
        for (const Horse& h : mHorses)
        {
            if (!h.Texture) continue;
            Material m{};
            m.Diffuse = {1, 1, 1, 1}; m.DiffuseMap = h.Texture.Get(); m.NormalMap = h.Normal.Get();
            m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .82f;
            m.SpecularColor = {0, 0, 0, 0}; m.EmissiveStrength = Kinds[h.Kind].Glow;
            m.DoubleSided = true;
            renderer.DrawDoorwaysGpuMeshSkinned(h.Mesh, XMLoadFloat4x4(&h.World), viewProjection, h.Palette, m);
        }
        renderer.BindDoorwaysSkinningDisabled();
    }

    std::wstring NyxaraHorses::Note() const
    {
        if (!mLoaded) return L"";
        const int at = mRidden >= 0 ? mRidden : mKind;
        const Horse& h = mHorses[static_cast<size_t>(at)];
        wchar_t line[200];
        const wchar_t* gait = h.Gait == Stand ? L"standing" : h.Gait == Walk ? L"walk" : h.Gait == Trot ? L"trot" : h.Gait == Canter ? L"canter" : L"gallop";
        if (mRide == Ride::None)
            std::swprintf(line, std::size(line), L"%ls (T: on it, or called)   riding way %d %ls", KindName(at), mWay + 1, WayName(mWay));
        else
            std::swprintf(line, std::size(line), L"%ls   %d %ls   %ls %.1f m/s   wind %d%%%ls%ls%ls", KindName(at), mWay + 1, WayName(mWay),
                          h.Grounded ? gait : L"in the air", std::abs(h.Fwd), static_cast<int>(h.Wind * 100 + .5f), h.Winded ? L"  WINDED" : L"",
                          h.Gliding ? L"  GLIDING" : L"", h.Burst > 0 ? L"  BURST" : L"");
        return line;
    }

    std::string NyxaraHorses::Log() const
    {
        if (!mLoaded || mRidden < 0) return {};
        const Horse& h = mHorses[static_cast<size_t>(mRidden)];
        char line[200];
        std::snprintf(line, sizeof line, "ride %d way %d state %d gait %s speed %.2f asked %.2f wind %.2f air %d phase %.2f roll %.2f slope %.2f",
                      h.Kind, mWay, static_cast<int>(mRide), GaitSpecs[h.Gait].Name, h.Fwd, h.Speed, h.Wind, h.Grounded ? 0 : 1, h.Phase, h.Roll, h.Slope);
        return line;
    }
}
