#pragma once

// Nyxara, daughter of the eagle: normal-play combat kit for the Nyxara session.
//
//   LMB / RMB      sword slashes (alternating hands) / charged slash
//   1 / 2 / 3      Talon Dash (dash through + lance) / Feather Cyclone (carried, pulls shades)
//                  / Moon Leap (rise, dive onto the target, judgment pillar)
//   skill + LMB/RMB  six combos (Twin Talons, Rising Eagle, Tempest Triple,
//                    Whirlwind Talons, Soul Weave, Moonfall Dive)
//   Q              exchange the swords between her hands
//   E (hold)       two spectral swords rise as a barrier and block shade orbs
//   Shift (hold)   wing boost: faster glide/flight with a feather trail
//   WASD           upright low wing-beat flight (FlightHover pose) instead of running
//
// Session-owned and cosmetic-first: it drives the animated character through the
// presentation clip override, owns its shade wraith targets and soul effects, and
// reaches player health only through CombatSystem::ApplyIncomingDamage.

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
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
        bool Initialize(Renderer& renderer, WorldRenderer& worldRenderer, WorldState& world, std::string& error);
        bool Ready() const { return mReady; }
        // Shift held: smoothly scales the whole simulation step (bullet time).
        float ScaleFrameTime(float realDeltaTime, bool inputAllowed);
        void Update(WorldState& world, WorldRenderer& worldRenderer, CombatSystem& combat,
            float deltaTime, float realDeltaTime, bool inputAllowed);
        void Draw(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection);

        enum class Act : int
        {
            None = -1, Slash1, Slash2, Heavy, Skill1, Skill2, Skill3,
            Combo1L, Combo1R, Combo2L, Combo2R, Combo3L, Combo3R, Guard, Swap, Count
        };

        // A target of her attacks. Pos is the aim point (about chest height: feet + 1.3 m).
        // With external enemies (the journey) the owner moves, draws and replaces them; the kit only
        // applies damage, knock-back (Push) and launches (Lift).
        struct Wraith
        {
            DirectX::XMFLOAT3 Anchor{}, Pos{}, Push{};
            float Yaw = 0, Phase = 0, Flash = 0, Respawn = 0, Shoot = 3, Lift = 0, LiftVel = 0, Fade = 1;
            int Health = 160, MaxHealth = 160;
            bool Alive = true;
            float Radius = .55f;     // m, body radius added to every hit test
            float PushScale = 1;     // 0 = cannot be pushed, pulled or launched (bosses, chests)
            float Weak = 1;          // her blows against it are multiplied by this (the journey: a complete charm set)
        };
        struct Orb { DirectX::XMFLOAT3 Pos{}, Vel{}; float Life = 0; int Damage = 6; DirectX::XMFLOAT4 Color{.62f, .28f, 1.f, 1.f}; };
        struct Keys { bool Lmb = false, Rmb = false, Q = false, E = false, K1 = false, K2 = false, K3 = false, Shift = false; };

        // ---- used by the journey, which owns the monsters of the road
        static constexpr int MaxEnemies = 30;   // hit masks are 32-bit
        std::vector<Wraith>& Enemies() { return mWraiths; }
        void SetExternalEnemies(bool external);
        void SpawnOrb(DirectX::XMFLOAT3 from, DirectX::XMFLOAT3 velocity, int damage, DirectX::XMFLOAT4 color, float life = 3.5f);
        bool Blocks(const WorldState& world, const DirectX::XMFLOAT3& from); // barrier up and turned toward 'from'
        void Notice(const wchar_t* text, float seconds) { mNotice = text; mNoticeTimer = seconds; }
        bool Airborne() const { return mGlideLift > .05f || mAir > .05f; }   // gliding, or in a leap
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
        bool Busy() const { return mAct != Act::None; }
        ID3D11ShaderResourceView* GlowTexture() const { return mGlowTex.Get(); }
        const DoorwaysGpuMesh& QuadMesh() const { return mQuadMesh; }

    private:
        bool mExternalEnemies = false;
        float mDamageScale = 1, mCooldownScale = 1;
        float mMercy = 0;
        const Keys* mAutoKeys = nullptr;
        bool mInputBlocked = false;
        enum class FxKind { Arc, ArcWave, Lance, Ring, RingWave, Pillar, Sigil, Feather, Spark, Cyclone };
        struct Fx
        {
            FxKind Kind = FxKind::Spark;
            DirectX::XMFLOAT3 Pos{}, Vel{};
            float Yaw = 0, Roll = 0, Size = 1, T = 0, Life = 1;
            DirectX::XMFLOAT4 Color{1, 1, 1, 1};
            bool Mirror = false;
            int Damage = 0;          // > 0: projectile / wave that damages each shade once
            unsigned HitMask = 0;
            int Target = -1;         // homing feather target
            float Launch = 0;
            float Tick = 0;          // cyclone damage tick timer
        };
        Keys ReadKeys(bool inputAllowed) const;
        float mDemoClock = 0;

        void StartAction(WorldState& world, Act act);
        void OnHit(WorldState& world, Act act, int hit);
        void UpdateEffects(WorldState& world, float dt);
        void UpdateWraiths(WorldState& world, CombatSystem& combat, float dt);
        void DamageWraith(int index, int damage, const DirectX::XMFLOAT3& from, float push, float launch, DirectX::XMFLOAT4 color);
        void MeleeArc(const WorldState& world, float radius, float halfAngleDegrees, int damage, float launch, DirectX::XMFLOAT4 color);
        void Area(const DirectX::XMFLOAT3& center, float radius, int damage, float launch, DirectX::XMFLOAT4 color);
        void Burst(const DirectX::XMFLOAT3& at, DirectX::XMFLOAT4 color, int count, float speed, float size);
        void Impact(const DirectX::XMFLOAT3& at, int damage, DirectX::XMFLOAT4 color); // hit-stop, shake, impact flash
        void Dash(float speed, float seconds, float yaw, int damage = 0);
        float AirProfile(float clipTime) const;  // aerial height of the current action
        void ApplySwordVisibility(WorldRenderer& worldRenderer);
        void PublishHud(WorldState& world) const;
        int NearestWraith(const DirectX::XMFLOAT3& from, float maxDistance, int skipMask = 0) const;
        DirectX::XMFLOAT4 RightColor() const;
        DirectX::XMFLOAT4 LeftColor() const;

        bool mReady = false;
        Keys mPrev;
        float mTimeScale = 1;
        bool mSwapped = false;              // false: Moonfeather right, Talonshade left
        bool mSwapPending = false;
        Act mAct = Act::None;
        float mActTime = 0;
        int mNextHit = 0;
        int mSlashChain = 0;
        float mChainWindow = 0;
        int mComboSkill = 0;                // 1..3 while a combo can follow
        float mComboWindow = 0;
        bool mBuffered = false;             // LMB pressed during a slash or the heavy blow: the next slash follows
        bool mHeavyBuffered = false;        // RMB pressed during a slash: the heavy blow follows
        int mComboBuffer = 0;               // 1 LMB, 2 RMB pressed during a skill before its combo opened
        float mComboBufferTimer = 0;
        float mBarrier = 0;                 // 0..1 barrier swords risen
        float mGuardHeld = 0;
        std::array<float, 3> mSkillCooldown{};
        float mGuardCooldown = 0, mSwapCooldown = 0;
        float mNoticeTimer = 0;
        std::wstring mNotice;
        unsigned mSeed = 0x9E3779B9u;
        float Random();

        std::vector<Fx> mFx;
        std::vector<Wraith> mWraiths;
        std::vector<Orb> mOrbs;
        float mBlockFlash = 0;
        float mGlideLift = 0, mTrailTimer = 0;   // wing-glide locomotion
        int mSkillBuffer = -1; float mSkillBufferTimer = 0; // remembered skill press
        float mLeanPitch = 0, mLeanRoll = 0, mLeanPitchVel = 0, mLeanRollVel = 0, mLeanYaw = 0; // travel lean / bank spring
        float mHitStop = 0, mShake = 0;          // impact feel
        float mDashTime = 0, mDashSpeed = 0, mDashYaw = 0; int mDashDamage = 0; unsigned mDashMask = 0;
        float mAir = 0; bool mDived = false; DirectX::XMFLOAT3 mDiveTarget{};
        bool mBoosting = false;                  // Shift wing boost

        // Procedural effect meshes and alpha masks.
        DoorwaysGpuMesh mArcMesh, mRingMesh, mQuadMesh, mFeatherMesh, mDiscMesh, mCylinderMesh;
        DoorwaysGpuMesh mSphereMesh, mConeMesh, mBladeMesh;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mArcTex, mGlowTex, mRingTex, mFeatherTex, mBeamTex, mSigilTex;
    };

    NyxaraKit& GetNyxaraKit();
}
