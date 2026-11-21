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
//   F              the slide, low along the ground, cutting what is before her (in flight: the twisted dash, V's
//                  twist, feet first, striking every creature it goes through)
//   V              the thrown swords: they stick in the middle of the nearest creature before her, she twists after
//                  them feet first with her wings folded, sticks both feet on its side, crouched there takes the
//                  handles in both hands, and backflips off it, kicking it and ripping them out; she lands on her
//                  feet facing it and stands still
//   E (hold)       two spectral swords rise as a barrier and block blows and shade orbs
//   Shift          on foot: run fast; in flight: the wing boost
//   A / D          turn her way (and the view with it); S steps back
//   Space          jump (in flight: climb)
//
// Her finishing kills (2026-11-20): a blow that would kill a creature (every one, its bosses too) dooms it instead;
// dazed, it waits for her (one that hovers comes down onto the ground), and as soon as she is free on foot near it she
// finishes it with one of the four the owner chose of ten, at random and never the same one twice running (each worked
// out for its size as it begins: StartFinish, so that her blades land on it from a 1.15 m Chinka to a 9 m Gveleshapi):
//   Reaper's Wheel    one whole turn with both blades out through it, and her right blade brought down from high
//   Mountain Breaker  a cut across it, a hop, and both blades down into the ground before it: it shakes round her
//   Tempest Flurry    four cuts: high from her left, back across, low at its legs (it sinks), and across its neck
//   Headsman          a cut at its legs drops it to its knees; her right blade raised high and brought down on its neck
// The other six (Talon Cross, Eagle's Plunge, Gale Slide, Heartpierce, Moonlit Flash, Rising Talon) are made too: the
// Character Lab's cue finisher <1-10> and CAUCASIAN_EAGLE_NYXARA_FINISHER play any of them.
// Doomed and not finished in time, it falls on its own.
//
// Session-owned: it drives the animated character, owns its shade wraith targets and its effects, and reaches
// player health only through CombatSystem::ApplyIncomingDamage.

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
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
    struct DoorwaysSkeleton;
    struct DoorwaysSkeletonPose;

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
        // The frame's time: slowed by a blow's moment or a finishing kill's, then by the caller's own scale (timeScale: the
        // game's or the lab's); a style's move asks here for what is left of its way, with this frame's own time
        float ScaleFrameTime(float realDeltaTime, bool inputAllowed, float timeScale = 1.f);
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
            // Her finishing kills (2026-11-19). Set by its owner: Finishable, a blow of hers that would kill it dooms it
            // instead (the journey: its creatures of the road; not bosses, chests or the Kaji press; the kit's own and
            // the labs' test creatures die at once); Knee, metres from its feet to its knees; Depth, metres front to back
            // of its model (lying, its owner raises it by 0.3 of that: the kit's rider stays on it); Width, metres from
            // its middle to the outside of its arms at its shoulders (0: its Radius). Set by the kit: Doom > 0,
            // doomed, seconds left before it falls on its own (it stays alive at 1 health meanwhile, Pinned); Linger,
            // as it kills it, seconds its body lies where it fell before it fades. Set by the kit every frame and
            // cleared by it before, for its owner's drawing: Daze (0..1) dazed, its head hanging, swaying; Kneel (0..1)
            // on its knees, its body lowered by Knee * Kneel; Limp (0..1) how far it hangs limp as the dying do
            // (< 0: as its Hold has it).
            bool Finishable = false;
            bool RoomLooked = false;   // (set by the kit: since it was doomed, whether her finishers' room was looked at)
            float Doom = 0, Knee = .45f, Depth = 0, Width = 0, Linger = 0;
            float Daze = 0, Kneel = 0, Limp = -1;
            // On its knees its shins are bent back KneelShin radians (at Kneel 1) and its body lowered so that its feet
            // (as far as they still reach down) or its knees stand on the ground (CreatureRig's pose; its owner's drawing).
            static constexpr float KneelShin = 1.65f;
            static float KneelDrop(float knee, float kneel)
            {
                const float bent = (std::min)(KneelShin * std::clamp(kneel, 0.f, 1.f), 1.5707963f);
                return knee * (1 - std::cos(bent));
            }
            // Of its Linger, the seconds its body lies once her kill of it is over (or once it is down, fallen on its
            // own); then the journey fades it. The Character Lab's stations take it away sooner (UpdateCreatureStations).
            static constexpr float LiesAfter = 1.1f;
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
        // The Eagle's Ascent (0.5, owner 2026-11-20): her wings' growth sets how long she flies: 10 s, then 15, 20, 30.
        void SetFlightSeconds(float seconds) { mFlightSeconds = seconds > 0 ? seconds : FlightEnergySeconds; }
        float FlightSeconds() const { return mFlightSeconds; }
        // Her looks (NyxaraLooks, 2026-11-21): her wings drawn this many times their size (scaled about their roots).
        void SetWingScale(float scale) { mWingScale = scale > .2f && scale < 4 ? scale : 1.f; }
        float WingScale() const { return mWingScale; }
        // Every blow she strikes (her cuts, her styles' blows; not a finisher): from where, her facing, how far it
        // reaches and its whole arc in degrees (the Eagle's Ascent breaks clay jars with them: Wayside).
        void SetBlowListener(std::function<void(const DirectX::XMFLOAT3&, float, float, float)> listener) { mBlowListener = std::move(listener); }
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
        // Every hit on the player goes through here: a hit starts a short time in which further hits do nothing (and
        // none hurts her while she finishes a creature). from: where the blow came from (the creature, the burst, a
        // point behind the orb on its way), so she is knocked away from it; none: from before her.
        bool Hurt(WorldState& world, CombatSystem& combat, int damage, const DirectX::XMFLOAT3* from = nullptr);
        // Her finishing kills. Doom: that creature is brought to the end of its health as a blow of hers would bring it
        // (the Character Lab's cue; it must be Finishable). Finishing: the finisher she plays now (0 none, 1 Talon
        // Cross, 2 Eagle's Plunge, 3 Gale Slide). CAUCASIAN_EAGLE_NYXARA_FINISHER=<1|2|3> plays that one every time.
        void Doom(int index);
        bool CanDoom(int index) const;   // would a killing blow of hers doom it now (near her and on her level, she on foot)?
        int Finishing() const;
        // (owner, 2026-11-20: ten finishers to choose three from, 0 to 9 in the Character Lab and the game) 1 to 10: the
        // finisher she plays first (where it has room); 0: one at random, as before
        static constexpr int FinishCount = 10;
        static const wchar_t* FinishName(int which);
        void SetFinisher(int which);
        int Finisher() const { return mFinisher; }
        bool FinishMade(int which) const;            // its clip was made at load
        void SetInputBlocked(bool blocked) { mInputBlocked = blocked; } // the story journal is open: no attacks or skills
        void SetAutoKeys(const Keys* keys) { mAutoKeys = keys; } // unattended runs: the journey's test pilot presses the keys
        // The Character Lab's clip viewer: one of her clips, as the kit keeps them (after its fixes at load), shown in
        // place of what she is doing; "" ends it. At seconds >= 0 it is held there, otherwise it plays round and round.
        // Her feet are not set on the ground and her head does not look about while it shows. (The game never calls it.)
        void SetPreview(const std::string& clip, float seconds = -1) { mPreview = clip; mPreviewTime = seconds; mPreviewClock = 0; }
        // Her window open (the Eagle's Ascent's I), the game standing still (Update's deltaTime 0): her pose goes on in real time
        void SetShowcase(bool on) { mShowcase = on; }
        std::vector<std::string> ClipNames() const;   // every clip she has, by name
        const std::string& ShownClip() const;          // the clip she shows now (hers, or the previewed one)
        float ShownTime() const { return mShownTime; } // and the time in it
        bool Busy() const;
        int AttacksBegun() const;   // how many blows she has begun (a cut, a skill): the creatures before her see one coming
        ID3D11ShaderResourceView* GlowTexture() const { return mGlowTex.Get(); }
        const DoorwaysGpuMesh& QuadMesh() const { return mQuadMesh; }

        // Her attack styles (owner, 2026-11-20: "i want to test another attacks for right and left mouse clicks ... get me
        // at least 5 different variants for each click ... let me choose them with numbers between 0-9"; then "try at least
        // each attack to be 3x faster then current one"). Ten styles of one move for each button: a click is that move, the
        // same every time. -1, as the game has it: her own cuts, clicks in a row going through them.
        static constexpr int StyleCount = 10;
        struct StyleInfo
        {
            const wchar_t* Name = L""; const wchar_t* Like = L"";           // the style, and the games it is taken from
            const wchar_t* Move[2] = {L"", L""}; const wchar_t* What[2] = {L"", L""};   // each button's move, and what it is
            float Seconds[2]{}, Blow[2]{}, Free[2]{};                        // all of it; its first blow; the next press taken (s)
            float Reach[2]{};                                                // how far a creature's edge may be for it to carry her all the way in (m)
            int Blows[2]{};
        };
        static StyleInfo Style(int style);
        void SetAttackStyle(int style);              // (and no combo)
        int AttackStyle() const { return mStyle; }
        bool StyleMade(int style) const;             // its two moves were made at load

        // Her combo (owner, 2026-11-20: of five made, "i like 1's LMB combo and 4's RMB , now set those"). Each button's
        // string of the styles' moves, begun by style 2's move for that button (LMB FLASH STEP, RMB DRAW CUT); the same
        // button clicked again goes on down it, on the move too. -1: none.
        static constexpr int ComboCount = 1;
        struct ComboInfo
        {
            const wchar_t* Name = L""; const wchar_t* What = L"";
            std::vector<const wchar_t*> String[2];   // each button's moves, in order
        };
        static ComboInfo Combo(int combo);
        void SetAttackCombo(int combo);              // (and no style)
        int AttackCombo() const { return mCombo; }
        bool ComboMade(int combo) const;             // all its moves were made at load

        // Her guards for E (owner, 2026-11-20: "generate 10 different defending animations for E skill and let me test them
        // with 0-9 numbers then i choose which one stays for E"): her pose while E is held. -1: as before (her guard stance).
        static constexpr int GuardCount = 1;   // (of the ten wing shields tried, the owner kept COCOON)
        struct GuardInfo { const wchar_t* Name = L""; const wchar_t* What = L""; };
        static GuardInfo Guard(int guard);
        void SetGuardStyle(int guard);
        int GuardStyle() const { return mGuardStyle; }
        bool GuardMade(int guard) const;             // its clip was made at load
        const DoorwaysSkeleton* BodySkeleton() const;

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
        // a creature at no health dies (its owner sees Alive go false), or, one she may finish, is doomed.
        void DamageWraith(int index, int damage, const DirectX::XMFLOAT3& from, float push, float launch);
        // It dies now (its owner sees Alive go false); its body lies linger seconds before it fades.
        void KillWraith(int index, float linger);
        void StrikeDown(int index);   // killed by her blow: it dies as the dead do (no finisher), with the blow's burst
        bool AnyFinishFits(WorldState& world, int index, std::string* why = nullptr);   // has any finisher room from where she is? (StartFinish's ways; why not)
        // A dead creature's body as the kit lets it fall and lie (her finishing kills; a doomed one that falls on its own).
        // Over Wait s its knees give, from KneelFrom to Kneel; then it falls toward Way (level) in Takes s, gathering speed
        // as a thing tipping over does, about its knees as far as it kneels and about its feet otherwise, its legs
        // straightening as it goes and going limp; down (the thud), it lies until Until s after it began (its owner keeps
        // it Linger + 0.55 s). Feet: its feet as it stood; Neck: its neck from them (the world's axes); Daze: its head
        // hanging as it goes; Knee: metres from its feet to its knees.
        struct Fallen
        {
            int Index = -1;
            DirectX::XMFLOAT3 Feet{}, Neck{0, 1.5f, 0}, Way{0, 0, 1};
            float Wait = 0, Takes = .6f, KneelFrom = 0, Kneel = 0, Daze = 0, Knee = .45f, Until = 2;
            float Time = 0;
            bool Down = false;
        };
        std::vector<Fallen> mFallen;
        void LetFall(const Fallen& fall);
        // Every frame: the doomed (dazed, waiting; one not finished in time falls on its own) and the fallen.
        void UpdateDoomed(float dt);
        // Her finishing kills: one begun at the nearest doomed creature in reach (when 'free'), and the one she plays.
        void UpdateFinish(WorldState& world, float dt, bool free);
        void StartFinish(WorldState& world, int index);
        // One finisher set up in her body to be played on that creature from where she is ('side' 1 her right, -1 her
        // left, and 'behind': the cross's way past it); false where its clip is not made. Has its way room (ground under
        // it all the way, nothing in it)? At the first moment it has not: when, and what it lacks.
        bool SetUpFinish(WorldState& world, int index, int which, float side, bool behind);
        bool FinishFits(const WorldState& world, float& at, std::string& why) const;
        // where the finisher set up in her body puts her 'at' s into it (the world's x and z), her facing, and how high
        // her picture is lifted ('ground': how far the creature's feet are over hers)
        void FinishPlan(float at, float ground, DirectX::XMFLOAT3& place, float& yaw, float& lift) const;
        DirectX::XMFLOAT3 mHerPlace{}; bool mHerAfoot = false;   // where she is this frame and whether on foot (a blow dooms near her only)
        float mSlow = 0, mSlowFull = 0, mSlowRate = 1;           // slow motion: real seconds left of it, in all, and how slow at its slowest
        void Burst(const DirectX::XMFLOAT3& at, DirectX::XMFLOAT4 color, int count, float speed, float size);
        void PublishHud(WorldState& world) const;

        bool mReady = false;
        Keys mPrev;
        bool mOnFoot = false;
        float mHeightOverGround = -1;
        float mFlightEnergy = 1, mFlightHold = 0; bool mFalling = false;
        float mFlightSeconds = FlightEnergySeconds;
        float mWingScale = 1;
        std::function<void(const DirectX::XMFLOAT3&, float, float, float)> mBlowListener;
        FootGround mFootGround;
        StairTread mStairTread;
        float mSpeedScale = 1;
        float mBarrier = 0;                 // 0..1 her shield up (E: her wings closed round her)
        bool mShowcase = false;             // SetShowcase
        int mFinisher = 0;                  // the finisher chosen (SetFinisher), 0 none
        float mShieldMotes = 0;             // the shield's sparks owed (a fraction of one)
        float mGuardHeld = 0, mGuardCooldown = 0;
        bool mGuarding = false, mEWas = false;   // her wing shield is up (E pressed: ShieldSeconds); E was down last frame
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
        int mStyle = -1;                         // her attack style (SetAttackStyle): -1 her own cuts
        int mCombo = -1;                         // her combo (SetAttackCombo): -1 none
        int mGuardStyle = 0;                     // her guard for E (SetGuardStyle): -1 her guard stance
        double mGrid = 0;                        // the engine's 60 Hz grid of her movement: its time left over (ScaleFrameTime)
        struct TrailPoint { DirectX::XMFLOAT3 Base, Tip; float Age = 0, Speed = 0; };
        std::vector<TrailPoint> mTrail[2];       // each blade's way a moment back in a style's move (Draw: its trail of light)

        // Procedural effect meshes and alpha masks.
        DoorwaysGpuMesh mRingMesh, mQuadMesh, mFeatherMesh, mSphereMesh, mConeMesh, mBladeMesh, mTriMesh;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mGlowTex, mRingTex, mFeatherTex, mWingFeatherTex;
    };

    NyxaraKit& GetNyxaraKit();
}
