#pragma once

// Nyxara's horses (2026-11-27). The owner: "in characterlab , i want you to put at least 6 different kind of horses that
// nyxara will be able to sit on with with any button - any key we have free in character lab but in game it will be G
// to sit or take down from horse - also get me 6 different driving experience for each horse - so i will choose one or
// three and put them in nea the round pathes we have". Asked, the owner chose Meshy models, the Caucasus's own horses
// and its tales'.
//
// The six (made in Meshy from concept pictures, art/horses/meshy; refined by art/refine/refine_assets.py; compiled to
// assets/Compiled/Horses; rigged by art/horses/rig_horses.py: 23 joints, and its seat, stirrups and hooves, <Name>.rig):
//   TUSHETIAN  the small mountain horse of Tusheti: quick to start and to turn, high leaps, no slower uphill
//   KABARDIAN  the Kabardian warhorse: steady, and the longest wind (it tires at half the rate)
//   KARABAKH   the golden Karabakh: the fastest gallop
//   RASHI      the winged horse of the tales: its wings open as it leaps; Space held in the air, it glides
//   FIRE       a horse of fire: embers under its hooves, a burst of them as it lands
//   FROST      a horse of frost: rime under its hooves and as it lands; it slides wide in its turns
// Every horse moves by code (no clips): its gaits (walk, trot, canter, gallop: each leg on its own beat, its hoof kept
// where it is put down while it bears and lifted and swung forward after; its leg bent to it by a reach of its three
// bones), its body's rise and rock, its neck's nod, its tail, a leap's tuck, a rear, and the winged one's wings.
//
// Her six ways of riding (each on every horse: 36 to try; the owner chooses one or three):
//   1 REINS   W canter, Shift gallop, Ctrl walk; A D turn it; S stops it, then backs it
//   2 GAITS   W a gait up and S a gait down (stand, walk, trot, canter, gallop; under standing it backs), kept with no
//             key held; Shift held gallops; A D turn it
//   3 LOOK    it goes where the view looks: W canter, Shift gallop, Ctrl walk; A D step it aside; S backs it
//   4 SPURS   each W a spur: speed, more of it in time with its stride; it slows by itself; a winded horse spurred
//             rears; S reins it in; A D turn it
//   5 WEIGHT  a heavy horse: slow to gather speed and to lose it, wide turns leaning hard, sliding wide at speed
//   6 WIND    quick as the wind: at once at speed, sharp turns, S turns it round on its haunches, A or D twice a leap
//             aside, Shift a burst
// Space: a leap, in every way. Its wind runs out in a long gallop and comes back at a walk.
//
// The kit owns them (NyxaraKit::LoadHorses): it moves the one she rides by the engine's own move (the session's
// impulses, her facing, a leap's rise), poses her on it (NyxaraRide.h) and draws them. T in the Character Lab (G in the
// game, once chosen) gets her on the horse by her, or down, or calls hers to her.

#include "DoorwaysGpuSkinningTypes.h"
#include "NyxaraRide.h"
#include "Rendering/DoorwaysGpuMesh.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

class Renderer;
struct WorldState;

namespace Doorways
{
    struct DoorwaysSkeleton;
    struct DoorwaysSkeletonPose;
    enum class DoorwaysInputAction;

    // A horse's skeleton as art/horses/rig_horses.py writes it (the joint numbers must match). Mesh units; the nose along
    // +Z, the hooves on y = 0, its left at x < 0.
    struct HorseRig
    {
        enum Joint
        {
            Pelvis, Chest, Neck, Neck2, Head, Tail, Tail2,
            FrontUpL, FrontKneeL, FrontFetlockL, FrontUpR, FrontKneeR, FrontFetlockR,
            HindUpL, HindHockL, HindFetlockL, HindUpR, HindHockR, HindFetlockR,
            WingL, WingL2, WingR, WingR2,
            JointCount
        };
        int Parent[JointCount]{};
        int Order[JointCount]{};                  // parents before children
        DirectX::XMFLOAT3 Pivot[JointCount]{};
        DirectX::XMFLOAT3 Seat{};                 // the middle of its seat
        DirectX::XMFLOAT3 Stirrup[2]{};           // its left stirrup's tread, its right's
        DirectX::XMFLOAT3 Hoof[4]{};              // front left, front right, hind left, hind right (on the ground)
        float Width = .25f;                       // half its barrel's width under the seat
        float Belly = .6f;                        // the height of its belly
        bool Load(const std::filesystem::path& path, std::string& error);
    };

    class NyxaraHorses
    {
      public:
        static constexpr int KindCount = 6, WayCount = 6;
        static const wchar_t* KindName(int kind);
        static const wchar_t* KindLine(int kind);
        static const wchar_t* WayName(int way);
        static const wchar_t* WayLine(int way);

        NyxaraHorses();
        ~NyxaraHorses();
        bool Load(Renderer& renderer, std::string& error);
        bool Loaded() const { return mLoaded; }
        // the six in a row from there, 'apart' metres apart, toward its right, each facing 'yaw'
        void Place(const WorldState& world, DirectX::XMFLOAT3 at, float yaw, float apart);

        void SetKind(int kind);                   // hers: the one called
        int Kind() const { return mKind; }
        void SetWay(int way);
        int Way() const { return mWay; }
        void Ask() { mAsked = true; }             // T (G): on the horse by her, down from it, or hers called to her
        bool Riding() const { return mRide != Ride::None; }    // getting on, on, or getting down
        bool Seated() const { return mRide == Ride::Seated; }
        int Ridden() const { return mRidden; }    // which horse (-1 none)
        DirectX::XMFLOAT3 At(int kind) const;     // where that horse stands (its hooves' middle, on the ground)
        float YawOf(int kind) const;              // and which way it faces
        void Drop(WorldState& world);             // down at once, where she is (her death; her being put elsewhere)

        struct Controls
        {
            bool W = false, S = false, A = false, D = false, Shift = false, Ctrl = false, Space = false;
            float ViewYaw = 0;
        };
        // A frame, after the kit's own travel and before her pose: what T asked; the horse she rides moved by her keys (the
        // engine moves it: the session's impulses, her facing, a leap's rise); the others standing or coming to her.
        // notice: a line to show, when there is one.
        void Update(WorldState& world, const Controls& keys, float dt, std::wstring& notice);
        // Her pose on it, after her clip's (her picture's own offsets put to nothing first: the kit)
        void PoseRider(const WorldState& world, const DoorwaysSkeleton& skeleton, DoorwaysSkeletonPose& pose, const DoorwaysSkeletonPose* grip = nullptr);   // (grip: a pose of hers with her fists closed)
        void Draw(Renderer& renderer, const DirectX::XMMATRIX& viewProjection);

        // effects asked for this frame (the kit makes them, as its sparks)
        struct Spark
        {
            DirectX::XMFLOAT3 At{};
            DirectX::XMFLOAT4 Color{1, 1, 1, 1};
            int Count = 1;
            float Speed = 1, Size = .1f;
        };
        std::vector<Spark>& Sparks() { return mSparks; }
        float WingBounce() const { return mWingBounce; }   // (her wings: how her seat goes up and down, m/s)
        std::wstring Note() const;                // the lab's panel: the horse, its gait, speed and wind
        std::string Log() const;                  // a log's line

      private:
        enum class Ride { None, Mounting, Seated, Dismounting };
        enum Gait { Stand, Walk, Trot, Canter, Gallop, GaitCount };
        struct Horse;
        void Move(WorldState& world, Horse& h, const Controls& c, float dt, std::wstring& notice);
        void Wander(WorldState& world, Horse& h, float dt);
        void Animate(Horse& h, float dt, bool ridden);
        void BuildPose(Horse& h);
        void Seat(const WorldState& world, Horse& h, float dt);
        void Hold(bool on);
        float GroundAt(const WorldState& world, float x, float z, float near, float fallback) const;

        bool mLoaded = false, mAsked = false, mWantsDown = false;   // (T at speed: reined in, then down)
        int mKind = 0, mWay = 0, mRidden = -1;
        Ride mRide = Ride::None;
        float mRideTime = 0;                      // s into getting on or down
        DirectX::XMFLOAT3 mFrom{}, mTo{};         // where she goes from and to as she gets on or down (her place)
        float mFromYaw = 0;
        float mCamLift = 0, mFollowWas = -1;
        float mWingBounce = 0, mSeatWas = 0;
        bool mSeatKnown = false;
        float mViewSet = 1e9f, mMouseRest = 0;    // (the view behind the horse as it turns: not while the mouse turns it)
        std::vector<Horse> mHorses;
        std::vector<Spark> mSparks;
        std::vector<DoorwaysInputAction> mHeld;   // the engine's moves taken away while she rides (given back as found)
        NyxaraRide::Poser mPoser;
        NyxaraRide::Saddle mSaddle;
        NyxaraRide::Blend mBlend;
        bool mPoserTried = false;
    };
}
