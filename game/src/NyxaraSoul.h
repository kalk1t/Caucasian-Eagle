#pragma once

// Nyxara's soul, left where she stood: its picture.
//
// Q (2026-11-26; the owner: "i want nyxara to be able to keep her soul where she is while doing q skill and she moves
// forward with 5-7 meter and she can get back to her sould whenever she presses q again - her soul is untouchable that
// time and she strings from souls still are lined to nyxara's current body"). Her kit moves her (NyxaraKit.cpp: her
// Soul); this draws what is seen of it: her own body once more where she left it, as light (the engine's Echo material:
// WorldRenderer::DrawCharacterEcho), the strings from it to her, the mark it stands on, the light of her leaving it, of
// her way back and of her being in it again. Nothing here is part of the game's world: no creature sees it or can
// touch it.
//
// Made in the Character Lab: three forms of it, then four ways of the one the owner chose; the owner chose THREAD STEP
// ("i like first better - take first and make the strings little brighter and based on first make 3 different q
// skill", then "first is the best . add it ingame", 2026-11-26). Her soul is she as she stood, held still, in pale blue
// glass, on a ring of light; six hair-thin threads from its head, chest, hands and wing tips to hers, drawn tight,
// humming as she moves fast; afterimages of her along her ways out and back. (The other ways are kept in
// build/soul-skill-draft/.)
// The light is drawn with her flair's atlas and noise (assets/Compiled/Effects/Nyxara/Flair) in the engine's Light
// material, as her flair is: its colours are light before the picture's tone curve, and are kept far from white.

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

#include "DoorwaysAnimationPose.h"
#include "Rendering/DoorwaysGpuMesh.h"

class Renderer;
class WorldRenderer;

namespace Doorways
{
    class DoorwaysAnimatedCharacterInstance;

    class NyxaraSoul
    {
    public:
        static constexpr const wchar_t* Name = L"THREAD STEP";   // (what the game calls it)
        // Afterimages of her on her way out (back: on her way back to it): one every so many metres she goes, each seen
        // for so many seconds.
        static float GhostEvery(bool back);
        static float GhostSeconds(bool back);

        // The places on a body its strings are tied to, in the world: hers, and her soul's.
        enum Tie { Chest, Head, Hips, HandL, HandR, WingL, WingR, TipL, TipR, FootL, FootR, TieCount };
        struct Body
        {
            DirectX::XMFLOAT3 At[TieCount]{};
            DirectX::XMFLOAT3 Feet{};            // the ground under it
        };

        bool Load(Renderer& renderer, std::string& error);
        bool Loaded() const { return mLoaded; }
        bool Failed() const { return mFailed; }

        // She leaves it: her soul stays, posed so, its picture placed so (her picture's matrix in the world).
        void Leave(const DoorwaysSkeletonPose& pose, const DirectX::XMFLOAT4X4& place, const Body& soul);
        // A soul that moves where it stands (it hangs in the air, it breathes): its pose and place now.
        void Pose(const DoorwaysSkeletonPose& pose, const DirectX::XMFLOAT4X4& place, const Body& soul);
        // An afterimage of her, as she is now.
        void Ghost(const DoorwaysSkeletonPose& pose, const DirectX::XMFLOAT4X4& place, float seconds);
        void Recall();                           // Q again: she is on her way back to it
        void Join();                             // she is in it again: it goes, in a light
        void Drop();                             // it goes without a word (she died; she was put elsewhere)
        bool Kept() const { return mStands && mState != State::Joined; }   // it stands (its strings are drawn)
        bool Out() const { return mStands || !mMotes.empty() || !mGhosts.empty(); }   // something of it is still seen

        // her: her own body now; pull: -1 while she is away, 0 to 1 on her way back
        void Update(float dt, const Body& her, float pull);
        // glowSet: the picture's glow is already set this frame (her flair's); otherwise it is set here while it is out.
        // unseen: nothing of it is drawn this frame (her window shows her, over the summit), its glow taken away.
        void Draw(Renderer& renderer, WorldRenderer& worldRenderer, const DoorwaysAnimatedCharacterInstance& character,
                  const DirectX::XMFLOAT3& eye, const DirectX::XMMATRIX& viewProjection, bool glowSet, bool unseen = false);
        int QuadsDrawn() const { return mQuadsDrawn; }

    private:
        struct Look;
        static const Look& TheLook();
        enum class State : std::uint8_t { Kept, Recalled, Joined };
        enum class Cell : std::uint8_t
        {   // (the flair atlas's cells, row by row: NyxaraFlair.h)
            Glow, Streak, SlashRazor, SlashSilk, SlashHeavy, Smoke, Star, Ring, Flame, Shard, Feather, Crack, Burst, Debris, Stain, Wisp,
            Needle, Splat, Swipe, Shock, Smoke2, Flame2
        };
        struct Mote
        {
            DirectX::XMFLOAT3 Pos{}, Vel{}, AxisX{1, 0, 0}, AxisZ{0, 0, 1};
            DirectX::XMFLOAT3 Color0{1, 1, 1}, Color1{1, 1, 1};
            float T = 0, Life = 1;
            float Size0 = .1f, Size1 = .1f, Long = 1;      // across (m), born and gone; its length, times that
            float Alpha = 1, In = .1f, Out = .5f;          // and the shares of its life it comes and goes over
            float Roll = 0, Spin = 0;
            float Rise = 0, Drag = 0;                      // m/s2 up (or down); how fast it loses its speed (1/s)
            float Worn0 = 0, Worn1 = 0, Soft = .3f;        // its shape worn away, born and gone; how soft that edge is
            float Light = 1;                               // 1 light, 0 a covering (smoke)
            Cell Shape = Cell::Glow;
            std::uint8_t Facing = 0;                       // 0 to the eye, 1 flat (AxisX, AxisZ), 2 drawn out along its flight
        };
        struct Shade { DoorwaysSkeletonPose Pose; DirectX::XMFLOAT4X4 Place{}; float T = 0, Life = 1; };

        float Random();
        float Random(float a, float b) { return a + (b - a) * Random(); }
        DirectX::XMFLOAT3 RandomWay();
        Mote& Add(Cell shape, const DirectX::XMFLOAT3& at);
        void Flash(const DirectX::XMFLOAT3& at, float size, const DirectX::XMFLOAT3& colour, float life, float alpha);
        void RingOut(const DirectX::XMFLOAT3& at, float from, float to, const DirectX::XMFLOAT3& colour, float life, float alpha, bool flat);
        void Sparks(const DirectX::XMFLOAT3& at, int count, float speed, const DirectX::XMFLOAT3& c0, const DirectX::XMFLOAT3& c1, float size, float life, float rise);
        void Shafts(const DirectX::XMFLOAT3& feet, int count, float reach, const DirectX::XMFLOAT3& c0, const DirectX::XMFLOAT3& c1, float life);
        void Shed(float dt);                     // what it gives off while it stands
        void Quad(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, const DirectX::XMFLOAT3& c, const DirectX::XMFLOAT3& d, Cell shape, float u0, float u1,
                  const DirectX::XMFLOAT3& colourA, const DirectX::XMFLOAT3& colourB, float alphaA, float alphaB, float worn, float soft, float light, float depth);
        // a band of light along a line of points, turned to the eye
        void Band(const std::vector<DirectX::XMFLOAT3>& line, const std::vector<float>& wide, const std::vector<float>& alpha,
                  const DirectX::XMFLOAT3& colour, const DirectX::XMFLOAT3& hot, const std::vector<float>& heat, float worn, float soft);
        void Flat(const DirectX::XMFLOAT3& at, float size, float roll, Cell shape, const DirectX::XMFLOAT3& colour, float alpha, float worn, float soft, float light);
        void Eye(const DirectX::XMFLOAT3& at, float size, float roll, Cell shape, const DirectX::XMFLOAT3& colour, float alpha, float soft);
        void Hang(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, int pieces, float sag, float floor, std::vector<DirectX::XMFLOAT3>& out) const;
        void DrawThreads(float shown);
        void DrawMark(float shown);
        void DrawMotes();
        bool DrawBody(Renderer& renderer, WorldRenderer& worldRenderer, const DoorwaysAnimatedCharacterInstance& character, const DirectX::XMMATRIX& viewProjection,
                      const DoorwaysSkeletonPose& pose, const DirectX::XMFLOAT4X4& place, float presence, float worn, float bright);

        bool mLoaded = false, mFailed = false;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mAtlas, mNoise;
        DoorwaysGpuMesh mMesh;
        std::vector<DoorwaysGpuDMeshVertex> mVertices;
        struct Piece { float Depth; std::uint32_t First; };   // a covering quad: sorted, far ones first
        std::vector<Piece> mCovering;
        std::vector<std::uint32_t> mLights, mIndices;
        std::vector<DirectX::XMFLOAT3> mLine;                 // (a thread's points, as drawn)
        std::vector<float> mWide, mAlpha, mHeat;
        std::vector<DirectX::XMFLOAT3> mDrawn[6];             // (each thread as it was last drawn: what it gives off is put on it)
        DirectX::XMFLOAT3 mEye{};
        int mQuadsDrawn = 0;
        bool mScreenSet = false;   // (the picture's glow was set here last frame: to be taken away when it goes)

        bool mStands = false;      // she left it, and it is not yet gone
        State mState = State::Kept;
        float mAge = 0;            // s since she left it
        float mGone = 0;           // s since she was in it again
        float mPull = -1;          // her way back, 0 to 1 (-1: she is away)
        float mClock = 0;          // (its lights' own time, going on always)
        float mHum = 0;            // how much her threads hum: she has just moved fast
        float mOwed[4]{};          // (what it gives off: the parts of a mote not yet made)
        DoorwaysSkeletonPose mPose;
        DirectX::XMFLOAT4X4 mPlace{};
        Body mSoul, mHer;
        DirectX::XMFLOAT3 mHerWas{}; bool mHerKnown = false;
        std::vector<Mote> mMotes;
        std::vector<Shade> mGhosts;
        std::uint32_t mSeed = 0x6C8E9CF5u;
    };
}
