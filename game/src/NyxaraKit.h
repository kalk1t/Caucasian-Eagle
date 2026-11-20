#pragma once

// Nyxara, daughter of the eagle: her body, her moves and her fight, for the game and the labs that use it.
//
// Her body is the one built from Mixamo clips (art/nyxara/blender/build_from_mixamo.py; Character Lab, bundle
// version 31): two swords in her hands, wings on her back, no bow. The kit poses her itself every frame from the
// bundle's clips (LoadBody keeps them) and gives the engine the pose (ApplyGamePresentationPose).
//
//   LMB / RMB      the left / the right sword: one click, one cut; clicks in a row go through the cuts (left 3,
//                  right 4); a click in a jump is the leaping cut
//   C              the feather storm: she leaps, turns three times in the air and throws her wing feathers all round
//                  her like daggers (to 27 m, stuck in the ground); at each turn every creature within 27 m is struck
//   F              the slide, low along the ground, cutting what is before her (in flight: the dart forward)
//   V              the thrown swords: they stick in the middle of the nearest creature before her, she flies after
//                  them twisting with her wings folded, plants her feet on it, pulls them out and kicks it over
//   E (hold)       two spectral swords rise as a barrier and block blows and shade orbs
//   Shift          on foot: run fast; in flight: the wing boost
//   A / D          turn her way (and the view with it); S steps back
//   Space          jump (in flight: climb)
//
// Session-owned: it drives the animated character, owns its shade wraith targets and its effects, and reaches
// player health only through CombatSystem::ApplyIncomingDamage.

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Rendering/DoorwaysGpuMesh.h"

class Renderer;
class WorldRenderer;
class CombatSystem;
struct WorldState;

namespace Doorways
{
    class NyxaraKit
    {
    public:
        NyxaraKit();
        ~NyxaraKit();
        // Her body: the bundle (Character.dmesh/.dskel/.meshbind/.dmat and one .danim a clip) becomes the player's
        // animated character, and its clips are kept for her moves. Before Initialize.
        bool LoadBody(Renderer& renderer, WorldRenderer& worldRenderer, WorldState& world, const std::filesystem::path& bundle, std::string& error);
        bool Initialize(Renderer& renderer, WorldRenderer& worldRenderer, WorldState& world, std::string& error);
        bool Ready() const { return mReady; }
        float ScaleFrameTime(float realDeltaTime, bool inputAllowed);
        void Update(WorldState& world, WorldRenderer& worldRenderer, CombatSystem& combat,
            float deltaTime, float realDeltaTime, bool inputAllowed);
        void Draw(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection);

        // A target of her attacks. Pos is the aim point (about chest height: feet + 1.3 m).
        // With external enemies (the journey) the owner moves, draws and replaces them; the kit only
        // applies damage, knock-back (Push) and launches (Lift), and holds them (Hold, Pinned).
        struct Wraith
        {
            DirectX::XMFLOAT3 Anchor{}, Pos{}, Push{};
            float Yaw = 0, Phase = 0, Flash = 0, Respawn = 0, Shoot = 3, Lift = 0, LiftVel = 0, Fade = 1;
            int Health = 160, MaxHealth = 160;
            bool Alive = true;
            float Radius = .55f;     // m, body radius added to every hit test
            float PushScale = 1;     // 0 = cannot be pushed, pulled or launched (bosses, chests)
            float Weak = 1;          // her blows against it are multiplied by this (the journey: a complete charm set)
            // Its build, for the moves fitted to it (the throw, the thrown swords): its height, and where its neck is
            // over its feet as it stands facing +z (its owner turns it by Yaw).
            float Height = 1.8f;
            DirectX::XMFLOAT3 Neck{0, 1.5f, 0};
            // Set by the kit every frame, cleared by it before: Pinned, her swords are in it or she is on her way to
            // throw it (it neither moves nor attacks); Hold > 0, it is held by the neck or pushed over and is drawn
            // as a stick from HeldFeet to HeldNeck (0 .. 1 blends from standing), limp.
            bool Pinned = false;
            float Hold = 0;
            DirectX::XMFLOAT3 HeldNeck{}, HeldFeet{};
        };
        struct Orb { DirectX::XMFLOAT3 Pos{}, Vel{}; float Life = 0; int Damage = 6; DirectX::XMFLOAT4 Color{.62f, .28f, 1.f, 1.f}; };
        // K1 F, K2 V, C K3. (Q and G are kept for the test pilot's scripts; they do nothing now.)
        struct Keys { bool Lmb = false, Rmb = false, Q = false, E = false, K1 = false, K2 = false, K3 = false, Shift = false, G = false, Space = false; };

        // ---- used by the journey, which owns the monsters of the road
        static constexpr int MaxEnemies = 30;   // hit masks are 32-bit
        std::vector<Wraith>& Enemies() { return mWraiths; }
        void SetExternalEnemies(bool external);
        // Explicit isolated training sessions: preserve impacts/reactions without health loss.
        void SetTrainingMode(bool enabled) { mTrainingMode=enabled; }
        unsigned TrainingTargetHits() const { return mTrainingTargetHits; }
        unsigned TrainingPlayerHits() const { return mTrainingPlayerHits; }
        void SpawnOrb(DirectX::XMFLOAT3 from, DirectX::XMFLOAT3 velocity, int damage, DirectX::XMFLOAT4 color, float life = 3.5f);
        bool Blocks(const WorldState& world, const DirectX::XMFLOAT3& from); // barrier up and turned toward 'from'
        void Notice(const wchar_t* text, float seconds) { mNotice = text; mNoticeTimer = seconds; }
        bool Airborne() const;   // in the air: flying, jumping, or lifted by a move
        void SetOnFoot(bool onFoot) { mOnFoot = onFoot; }
        // > 0: on foot Space is a real jump at this speed (m/s up), not only the jump clip (The Eagle's Ascent, 2026-11-18:
        // she must be able to jump off the mountain). 0, the default: the clip alone, as on the road. Unattended, the
        // session's ScriptedJump presses it.
        void SetRealJump(float speed) { mRealJump = speed; }
        // Flying is a limited resource (owner, 2026-11-18). Space held FlightHoldSeconds on foot takes her up; she can
        // fly FlightEnergySeconds on full energy; out of it, gravity brings her down (the Fall clip) and she lands on
        // foot. On the ground it comes back in FlightRefillSeconds. Unattended runs (the tests) are not limited.
        // (FlightHoldSeconds 3 until 2026-11-19; the owner: "reduce flying uptime from 3 seconds to 1.5s")
        static constexpr float FlightHoldSeconds = 1.5f, FlightEnergySeconds = 30, FlightRefillSeconds = 20, FlightLeastShare = .1f;
        float FlightEnergy() const { return mFlightEnergy; }                    // 0..1
        float FlightCharge() const { return mFlightHold / FlightHoldSeconds; }  // 0..1: how far the take-off hold has come
        bool Falling() const { return mFalling; }
        // Her height over the ground under her (the journey knows it; elsewhere it is not known and not needed): for
        // her landing, whose clip reaches for the ground before she touches it.
        void SetHeightOverGround(float height) { mHeightOverGround = height; }
        // On foot her feet are set on the ground as it is drawn under each of them (rough land, stair treads): a foot
        // is raised by bending its leg, and her body is lowered when a foot has to reach down. The owner may give the
        // drawn surface where it differs from the collision she walks on (Map A's treads over their ramp): height at
        // (x, z) nearest 'nearY', or NaN where it has none. Without one, or where it gives NaN, the scene's walkable
        // ground is used; with neither, her feet stay as the clip has them.
        using FootGround = std::function<float(float x, float z, float nearY)>;
        void SetFootGround(FootGround ground) { mFootGround = std::move(ground); }
        // Stairs (Map A): the middle of the tread of a given top nearest (x, z), and the stair's way along the ground.
        // Climbing a stair she plays Stairs_Up by her height on it, and each foot standing is slid onto the middle of
        // its tread. Without it (or off a stair) she walks up as on any slope.
        using StairTread = std::function<bool(float x, float z, float top, float& cx, float& cz, float& ax, float& az)>;
        void SetStairTread(StairTread stair) { mStairTread = std::move(stair); }
        // Her travel speed is multiplied by this (0 while a move carries her, slower stepping back). The kit applies
        // it to MoveSpeed itself; an owner that sets MoveSpeed after the kit's Update multiplies by it too.
        float SpeedScale() const { return mSpeedScale; }
        void Effect(const DirectX::XMFLOAT3& at, DirectX::XMFLOAT4 color, int count, float speed, float size) { Burst(at, color, count, speed, size); }
        void RingEffect(const DirectX::XMFLOAT3& at, float size, DirectX::XMFLOAT4 color, float life);
        void SetDamageScale(float scale) { mDamageScale = scale; }
        void SetCooldownScale(float scale) { mCooldownScale = scale; }   // of the time her three skills take to return
        void ResetCooldowns() { mSkillCooldown = {}; }
        void Shake(float amount) { mShake = (std::max)(mShake, amount); }
        // Every hit on the player goes through here: a hit starts a short time in which further hits do nothing.
        bool Hurt(WorldState& world, CombatSystem& combat, int damage);
        void SetInputBlocked(bool blocked) { mInputBlocked = blocked; } // the story journal is open: no attacks or skills
        void SetAutoKeys(const Keys* keys) { mAutoKeys = keys; } // unattended runs: the journey's test pilot presses the keys
        // The Character Lab's clip viewer: one of her clips, as the kit keeps them (after its fixes at load), shown in
        // place of what she is doing; "" ends it. At seconds >= 0 it is held there, otherwise it plays round and round.
        // Her feet are not set on the ground and her head does not look about while it shows. (The game never calls it.)
        void SetPreview(const std::string& clip, float seconds = -1) { mPreview = clip; mPreviewTime = seconds; mPreviewClock = 0; }
        std::vector<std::string> ClipNames() const;   // every clip she has, by name
        const std::string& ShownClip() const;          // the clip she shows now (hers, or the previewed one)
        float ShownTime() const { return mShownTime; } // and the time in it
        bool Busy() const;
        ID3D11ShaderResourceView* GlowTexture() const { return mGlowTex.Get(); }
        const DoorwaysGpuMesh& QuadMesh() const { return mQuadMesh; }

    private:
        struct Body;                        // her pose and her moves (NyxaraKit.cpp)
        std::unique_ptr<Body> mBody;
        bool mExternalEnemies = false;
        bool mTrainingMode = false;
        unsigned mTrainingTargetHits = 0, mTrainingPlayerHits = 0;
        float mDamageScale = 1, mCooldownScale = 1;
        float mMercy = 0;
        const Keys* mAutoKeys = nullptr;
        std::string mPreview; float mPreviewTime = -1, mPreviewClock = 0, mShownTime = 0;
        bool mInputBlocked = false;
        float mRealJump = 0; bool mScriptedSpaceWas = false;
        enum class FxKind { Ring, Spark, Feather };
        struct Fx
        {
            FxKind Kind = FxKind::Spark;
            DirectX::XMFLOAT3 Pos{}, Vel{};
            float Yaw = 0, Roll = 0, Size = 1, T = 0, Life = 1;
            DirectX::XMFLOAT4 Color{1, 1, 1, 1};
            bool Mirror = false;
            bool Air = false;        // drawn as cut air (pale, no glow), not as light
            int Target = -1;         // -2: a feather of the wing boost, which falls; -3: one of the feather storm's; 0 on: aimed at that creature
            int Damage = 0;          // (aimed) what it does to the creature
            bool Glow = true;        // a light about it
            float Floor = -1e9f;     // (the storm's) the ground where it was thrown, if no ground is found under it
            bool Stuck = false;      // (the storm's) in the ground
        };
        Keys ReadKeys(bool inputAllowed) const;

        void UpdateWraiths(WorldState& world, CombatSystem& combat, float dt);
        void UpdateEffects(float dt);
        int mFeatherBlows = 0;                                             // blows her storm's feathers struck since the last frame (for the log)
        // A blow of hers at one enemy: damage (scaled), the hit's flash, push and launch, a short freeze and shake;
        // a creature at no health dies (its owner sees Alive go false).
        void DamageWraith(int index, int damage, const DirectX::XMFLOAT3& from, float push, float launch);
        void Burst(const DirectX::XMFLOAT3& at, DirectX::XMFLOAT4 color, int count, float speed, float size);
        void PublishHud(WorldState& world) const;

        bool mReady = false;
        Keys mPrev;
        bool mOnFoot = false;
        float mHeightOverGround = -1;
        float mFlightEnergy = 1, mFlightHold = 0; bool mFalling = false;
        FootGround mFootGround;
        StairTread mStairTread;
        float mSpeedScale = 1;
        float mBarrier = 0;                 // 0..1 barrier swords risen
        float mGuardHeld = 0, mGuardCooldown = 0;
        bool mGuarding = false;
        std::array<float, 3> mSkillCooldown{};
        float mNoticeTimer = 0;
        std::wstring mNotice;
        unsigned mSeed = 0x9E3779B9u;
        float Random();

        std::vector<Fx> mFx;
        std::vector<Wraith> mWraiths;
        std::vector<Orb> mOrbs;
        float mBlockFlash = 0;
        float mTrailTimer = 0;
        bool mBoosting = false;                  // Shift wing boost in flight
        float mHitStop = 0, mShake = 0;          // impact feel

        // Procedural effect meshes and alpha masks.
        DoorwaysGpuMesh mRingMesh, mQuadMesh, mFeatherMesh, mSphereMesh, mConeMesh, mBladeMesh;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mGlowTex, mRingTex, mFeatherTex, mWingFeatherTex;
    };

    NyxaraKit& GetNyxaraKit();
}
