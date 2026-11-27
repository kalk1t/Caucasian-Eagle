#pragma once

// Nyxara's flair (2026-11-25): the light of her blows, nine ways, three for each of her three gears.
//
// The owner, after 0.5.3: "take those three nyxaras we use in game and refine their animations and effects to have AAA
// quality ... get me at least 3 different nyxara effects for each nyxara equipment and let me check those with same
// principle 1-2-3 and u toggles between attacking sequences effects and combo effects", "in character lab", "you have
// to work first before we change anything for game". And, having seen the nine there: "i like 3'd most - so add those
// to current nyxara's ingame version, also adjust finishings on the new effect systems and update current nyxara's
// windy, fire and ice versions with 3'd effects called flowing". So the game asks for each gear's third (NyxaraKit::
// SetFlair, from EaglesAscentGame as her gear is put on), her finishing kills are lit by it (a finisher's blows:
// Strike, placed; its kill: Finish), her slide leaves its wake (Dash) and cuts in passing (Blow::Passing), and the
// Character Lab still shows all nine. Where none is asked for, none of it is loaded, updated or drawn.
//
// Before her first gear she has no light at all: Choose's plain. The owner, 2026-11-26, having seen her set out with
// THE HIGHLANDS' third in her own blue (Choose's own, kept to compare): "normal nyxara should not have effects and
// when she becomes windy - wind effects, when she becomes fire - fire effects and ice when she becomes ice". Plain,
// her blows are told to it as any gear's are, and it makes their blood and asks the game for their held moment and
// shaken view; it draws no light: no trail, no bite, no ring, no wake, no kill's light.
//
// What it draws, all of it light itself (the engine's DoorwaysMaterialMode::Light: unlit, as bright as it likes, so
// that the picture's glow takes it up), from one atlas of twenty-two shapes (art/nyxara/flair/make_flair_textures.py)
// and one mesh written anew each frame:
//   ribbons   each blade's way a moment back, as a band of light with a hot edge at the blade's point, worn away in
//             streaks toward its tail (between the frames her blade's turn is followed round, not cut across)
//   arcs      a wide crescent thrown out in the plane of a blow as it lands
//   motes     sparks drawn out along their flight, embers, flashes, glints, rays, smoke, flames, crystals, feathers,
//             wisps, chips of stone, and marks on the ground (cracks, rings, stains)
// and what it asks of the picture as a whole (the engine's DoorwaysScreenFlair): the glow, a blow's punch (a blur out
// from where it landed, colours pulled apart), a ring of bent air, a flash.
//
// The nine (Describe). THE HIGHLANDS: GALE EDGE, STONEBREAKER, SKY WHEEL. AMIRANI'S FIRE: CINDER RAZOR, FORGE HAMMER,
// FIRE DANCE. MOONLIT: SILVER THREAD, GLACIER, MOON WRAITH. In each gear the first is fast and sharp, the second heavy
// and bursting, the third flowing.

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Rendering/DoorwaysGpuMesh.h"

class Renderer;

namespace Doorways
{
    class NyxaraFlair
    {
    public:
        static constexpr int GearCount = 3, StylesPerGear = 3;
        // Her three in one (2026-11-27): chosen as a style, the gear's three by the move. Her kit tells it, as each move
        // of her combo begins, which of them the move is in (Use: 0 sharp, 1 heavy, 2 flowing); its blows, what her
        // blades shed, its pace and its feel are that one's. Her blades' trails keep the way each part of them was made
        // in, the one's bands giving way to the next's over HandOverSeconds of the trail, and as a move of another way
        // begins her blades throw the last one's light on into the new one's (HandOver).
        static constexpr int Mixed = 3;
        static constexpr float HandOverSeconds = .1f;
        struct Info
        {
            const wchar_t* Gear = L""; const wchar_t* Name = L""; const wchar_t* What = L"";
            float Pace = 1;      // how fast its combo's moves are played (1: as made)
            float CutPace = 1;   // and her own cuts in it
        };
        // (own: THE HIGHLANDS' third in her own blue, without that gear's gold: how she set out for a day, kept to
        // compare. plain: as she sets out, no light at all)
        static Info Describe(int gear, int style, bool own = false, bool plain = false);

        // Its textures and its mesh: asked for the first time a style is to be drawn (false: what is missing; no flair).
        bool Load(Renderer& renderer, std::string& error);
        bool Loaded() const { return mLoaded; }
        bool Failed() const { return mFailed; }
        void Choose(int gear, int style, bool own = false, bool plain = false);   // (-1: none; Mixed: her three in one)
        void Use(int style);                                                      // (Mixed) the way of the move she begins
        bool Mixing() const { return mStyle == Mixed; }
        int LookStyle() const { return Mixing() ? mUse : mStyle; }               // the way whose light is made now
        bool Chosen() const { return mGear >= 0 && mStyle >= 0; }
        bool On() const { return mLoaded && Chosen(); }
        bool Plain() const { return mPlain; }            // none of its light is made: blood, and the game's feel
        bool Lit() const { return On() && !mPlain; }
        int Gear() const { return mGear; }
        int Style() const { return mStyle; }
        bool Own() const { return mOwn; }

        // ---- what her kit tells it
        using Ground = std::function<float(float x, float z, float nearY)>;   // the ground's height (NaN: none known)
        void SetGround(const Ground* ground) { mGround = ground; }           // (her kit's own, kept by it)
        // Each frame her pose is made: its time, and where she is; then, in a cut, a move or a finishing kill, where each
        // blade is (its handle and its point, in the world). pace: how fast her moves are played. fresh: its trail is
        // not joined to what was.
        // (Where she is: the engine moves her in steps of a 60th of a second, and her pose is made every frame, so a
        // picture drawn more often than that, or unevenly, has frames in which her blade has gone on and she has not.
        // A trail's points are kept only as she is moved, or while she is not being moved at all; between them the
        // band's newest end goes with the blade as it is drawn. Kept every frame, her blade's way went up in steps as
        // she ran in to a finishing kill.)
        void AgeTrails(float poseDt, const DirectX::XMFLOAT3& her);
        void Blade(int hand, const DirectX::XMFLOAT3& grip, const DirectX::XMFLOAT3& tip, float poseDt, float pace, bool fresh);
        // A blow of hers lands (or cuts air).
        struct Blow
        {
            DirectX::XMFLOAT3 Her{};      // her feet
            float Yaw = 0;                // her facing
            int Hand = 1;                 // the blade (0 left, 1 right, 2 both)
            int Weight = 0;               // 0 a light blow, 1 a middle one, 2 a heavy one or a string's last
            float Ring = 0;               // the ground struck: so wide (m; 0 not)
            bool Whirl = false;           // all round her
            bool Rising = false;          // a rising cut
            float Reach = 2;              // how far it reaches (m)
            struct Body { DirectX::XMFLOAT3 Feet{}; float Height = 2, Radius = .5f; };
            std::vector<Body> Struck;     // the creatures struck
            bool Placed = false;          // her kit knows where it bites (a finisher's blow, brought onto its creature):
            DirectX::XMFLOAT3 At{};       // there
            bool Passing = false;         // a cut in passing (her slide): no moment is held for it, the world is not slowed
        };
        void Strike(const Blow& blow);
        // A finishing kill (her kit's finishers): its creature dies of this blow. The style's last word, made round the
        // creature and sized to it (from a 1.15 m Chinka to a 9 m Gveleshapi).
        struct Kill
        {
            DirectX::XMFLOAT3 At{};          // where her blade is in it (Ground: where her blades went into the ground)
            DirectX::XMFLOAT3 Way{0, 0, 1};  // from her to it, level
            DirectX::XMFLOAT3 Cut{};         // the way her blade went through it (nothing: not known)
            Blow::Body It;                   // the creature
            bool Ground = false;             // both her blades went into the ground before it
        };
        void Finish(const Kill& kill);
        void Gust(const DirectX::XMFLOAT3& at, float radius);   // her wings beat her up off the ground, or she comes down on it: dust thrown out over it
        void Dash(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to, bool first);   // a move carrying her: a piece of her way (her middle)
        void Whirl(const DirectX::XMFLOAT3& middle, float dt);                               // each frame of a turn with her blades out
        void Wave(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way);                // a wave along the ground: a point of its line
        // What its blows ask of the game's feel; taken (and emptied) by her kit each frame.
        struct Feel { float Stop = 0, Shake = 0, Slow = 0, SlowRate = 1; };
        Feel TakeFeel() { const Feel f = mFeel; mFeel = {}; return f; }

        void Update(float dt);
        void Draw(Renderer& renderer, const DirectX::XMFLOAT3& eye, const DirectX::XMMATRIX& viewProjection);
        void Clear();
        // (the Character Lab's evidence) how much is alive now, and how much the last frame drew
        int Motes() const { return static_cast<int>(mMotes.size()); }
        int Quads() const { return mQuadsDrawn; }

    private:
        enum class Cell : std::uint8_t
        {   // (the atlas's cells, row by row)
            Glow, Streak, SlashRazor, SlashSilk, SlashHeavy, Smoke, Star, Ring, Flame, Shard, Feather, Crack, Burst, Debris, Stain, Wisp,
            Needle, Splat, Swipe, Shock, Smoke2, Flame2
        };
        // How a mote is turned: to the eye; drawn out along its flight; flat on the ground; held as it was made (its
        // two axes); standing along an axis and turned about it to the eye.
        enum class Face : std::uint8_t { Eye, Along, Ground, Fixed, Axis };
        struct Mote
        {
            Cell Shape = Cell::Glow; Face Facing = Face::Eye;
            DirectX::XMFLOAT3 Pos{}, Vel{}, AxisX{1, 0, 0}, AxisY{0, 1, 0};
            float Size0 = .2f, Size1 = .2f;        // across (m), born and gone
            float Long = 1, LongBySpeed = 0;       // its length: Long times its size, and so many seconds of its flight
            float Length = 0;                      // (or, over 0, its length itself, m)
            float Sprout = 0;                      // (Axis) grown to its length in this time (s)
            bool Base = false;                     // (Axis) it stands on Pos (else Pos is its middle)
            bool LongV = false;                    // its texture's long way is down its cell (a flame, a crystal, a feather)
            bool Flip = false;                     // its texture turned over, left for right
            float Roll = 0, Spin = 0;
            float T = 0, Life = .5f;               // (T under 0: not yet)
            DirectX::XMFLOAT3 Color0{1, 1, 1}, Color1{1, 1, 1};
            float Alpha = 1, In = .05f, Out = .5f; // its most; the shares of its life it comes and goes in
            float Worn0 = 0, Worn1 = 0, Soft = 1;  // worn away (0 none, 1 gone), born and gone; how soft that edge
            float Light = 1;                       // 1 added light, 0 a covering (smoke, stone)
            float Fall = 0, Drag = 0, Wander = 0, Swirl = 0, Flicker = 0;
            bool Bounce = false, Stick = false, Stuck = false;   // off the ground; lying where it lands
            bool Splat = false;                                  // (lying: as a spot on the ground)
            float Floor = -1e9f;
            float Seed = 0;
        };
        struct Arc
        {   // a crescent in a plane: round Middle, A where its angle is 0 and B the way it sweeps
            DirectX::XMFLOAT3 Middle{}, A{1, 0, 0}, B{0, 0, 1}, Drift{};
            float Radius = 2, Wide = 1, From = 0, To = 1.5f;      // m; radians
            float T = 0, Life = .3f, Sweep = .08f, Grow = .08f;   // s; drawn across in Sweep s; grown by this share
            Cell Shape = Cell::SlashHeavy;
            DirectX::XMFLOAT3 Color0{1, 1, 1}, Color1{1, 1, 1};
            float Alpha = 1, Worn1 = .9f, Soft = .3f, Light = 1;
            bool Pointed = true;                                  // narrowing to a point at both its ends
            int Order = 0;                                        // (of those that cover: the later made over the earlier)
        };
        struct TrailPoint { DirectX::XMFLOAT3 Grip{}, Tip{}; float Age = 0, Shown = 0; bool Fresh = false; int Style = 0; };   // (Style: the way it was made in)
        struct Ribbon
        {   // one band of a blade's trail
            Cell Shape = Cell::SlashRazor;
            float Life = .2f, Inner = .3f, Outer = 1.1f;   // s; along the blade (0 its handle, 1 its point)
            DirectX::XMFLOAT3 Color0{1, 1, 1}, Color1{1, 1, 1};
            float Alpha = 1, Worn0 = 0, Worn1 = .6f, Soft = .35f, Light = 1;
        };
        struct Cut
        {   // a blow as its blade has it: where it bites, the way the blade's point goes, along the blade, across both
            DirectX::XMFLOAT3 At{}, Way{1, 0, 0}, Edge{0, 0, 1}, Normal{0, 1, 0}, Middle{}, Forward{0, 0, 1}, Side{1, 0, 0}, Floor{};
            float Radius = 1.5f;
            int W = 0;             // its weight (0 to 2)
            bool Hit = false;      // it is in a creature
        };
        struct Look;
        static const Look& LookOf(int gear, int style, bool own = false);
        const Look& Now() const { return LookOf(mGear, LookStyle(), mOwn); }
        int Which() const { return mGear * StylesPerGear + LookStyle(); }   // (which of the nine: 0 to 8)
        // (SKY WHEEL's gold, its hot lights and what they cool to: in its own form, her own blue instead)
        DirectX::XMFLOAT3 WheelGold() const;
        DirectX::XMFLOAT3 WheelHot() const;
        DirectX::XMFLOAT3 WheelEmber() const;
        Cut CutOf(const Blow& blow) const;

        float Random();
        float Random(float a, float b) { return a + (b - a) * Random(); }
        DirectX::XMFLOAT3 RandomWay();
        DirectX::XMFLOAT3 Cone(const DirectX::XMFLOAT3& about, float spread);
        float GroundAt(const DirectX::XMFLOAT3& at, float fallback) const;
        Mote& Add(Cell shape, Face facing, const DirectX::XMFLOAT3& at);

        // (the pieces the nine are made of)
        void Sparks(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way, int count, float speed, float spread, const DirectX::XMFLOAT3& c0,
                    const DirectX::XMFLOAT3& c1, float size, float life, float fall, bool bounce);
        void Embers(const DirectX::XMFLOAT3& at, int count, float spread, float speed, const DirectX::XMFLOAT3& c0, const DirectX::XMFLOAT3& c1, float size, float life,
                    float rise, float swirl);
        void Flash(const DirectX::XMFLOAT3& at, float size, const DirectX::XMFLOAT3& colour, float life, float alpha = 1, float light = 1);
        void Glint(const DirectX::XMFLOAT3& at, float size, const DirectX::XMFLOAT3& colour, float life, float wide, float roll = 0);
        void Rays(const DirectX::XMFLOAT3& at, int count, float length, const DirectX::XMFLOAT3& c0, const DirectX::XMFLOAT3& c1, float life, float speed, float thick,
                  const DirectX::XMFLOAT3* about = nullptr);
        void Lines(const DirectX::XMFLOAT3& at, float size, const DirectX::XMFLOAT3& colour, float life, float grow);
        void Line(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way, float length, float thick, const DirectX::XMFLOAT3& c0, const DirectX::XMFLOAT3& c1,
                  float life, float wait = 0, float sprout = .03f);
        void Beam(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way, float length, float thick, const DirectX::XMFLOAT3& colour, float alpha, float life,
                  float light, float wait = 0);
        float ScreenRoll(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way) const;
        void Swipe(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way, const DirectX::XMFLOAT3& bow, float length, const DirectX::XMFLOAT3& c0,
                   const DirectX::XMFLOAT3& c1, float alpha, float life, float light = 1, float grow = 1.2f);
        void ShockRing(const DirectX::XMFLOAT3& at, float from, float to, const DirectX::XMFLOAT3& colour, float alpha, float life, float light,
                       const DirectX::XMFLOAT3* axisX = nullptr, const DirectX::XMFLOAT3* axisY = nullptr);
        void Gash(const Cut& cut, float length, const DirectX::XMFLOAT3& body, const DirectX::XMFLOAT3& deep, const DirectX::XMFLOAT3& core, float life, float light);
        void Puffs(const DirectX::XMFLOAT3& at, int count, float spread, float size, const DirectX::XMFLOAT3& colour, float alpha, float life, float rise, float light,
                   const DirectX::XMFLOAT3& push = {0, 0, 0});
        void RingOut(const DirectX::XMFLOAT3& at, float from, float to, const DirectX::XMFLOAT3& colour, float life, const DirectX::XMFLOAT3* axisX = nullptr,
                     const DirectX::XMFLOAT3* axisY = nullptr);
        void Mark(const DirectX::XMFLOAT3& at, Cell shape, float size, const DirectX::XMFLOAT3& c0, const DirectX::XMFLOAT3& c1, float alpha, float life, float light,
                  float in = .03f, float out = .5f, float roll = -1);
        void Crystals(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way, int count, float speed, float spread, const DirectX::XMFLOAT3& colour, float size);
        void Spikes(const DirectX::XMFLOAT3& at, int count, float reach, float tall, const DirectX::XMFLOAT3& colour, float life);
        void Feathers(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way, int count, float speed, const DirectX::XMFLOAT3& colour, float light);
        void Flames(const DirectX::XMFLOAT3& at, int count, float spread, float size, const DirectX::XMFLOAT3& c0, const DirectX::XMFLOAT3& c1, float life, float rise);
        void Wisps(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way, int count, float spread, float size, const DirectX::XMFLOAT3& colour, float life, float speed);
        void Chips(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way, int count, float speed, const DirectX::XMFLOAT3& colour, float size);
        void Crescent(const Cut& cut, Cell shape, float scale, float wide, float back, float ahead, float sweep, const DirectX::XMFLOAT3& c0,
                      const DirectX::XMFLOAT3& c1, float alpha, float life, float drift = 0, float grow = .1f, float light = 1);
        void Slash(const Cut& cut, Cell shape, float scale, float wide, float back, float ahead, float sweep, const DirectX::XMFLOAT3& body,
                   const DirectX::XMFLOAT3& tail, const DirectX::XMFLOAT3& under, const DirectX::XMFLOAT3& rim, float life, float drift = 0, float grow = .1f,
                   float underAlpha = .55f);
        void Wheel(const DirectX::XMFLOAT3& middle, const Cut& cut, Cell shape, float radius, float wide, const DirectX::XMFLOAT3& c0, const DirectX::XMFLOAT3& c1,
                   float alpha, float life);
        void GroundGlow(const DirectX::XMFLOAT3& at, float size, const DirectX::XMFLOAT3& colour, float life);
        void Bleed(const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& way, int count);
        void Punch(const DirectX::XMFLOAT3& at, float blur, float split);
        void Shock(const DirectX::XMFLOAT3& at, float strength, float reach, float life);
        void Tint(const DirectX::XMFLOAT3& colour, float amount);
        void Later(float in, const DirectX::XMFLOAT3& at, float blur, float split, float stop, float shake);
        void Felt(const Blow& blow, const Cut& cut);

        // (the nine: a blow, what her blades shed as they go, a piece of a dash, a turn, a point of a wave)
        void StrikeGale(const Blow& blow, const Cut& cut);
        void StrikeStone(const Blow& blow, const Cut& cut);
        void StrikeWheel(const Blow& blow, const Cut& cut);
        void StrikeCinder(const Blow& blow, const Cut& cut);
        void StrikeForge(const Blow& blow, const Cut& cut);
        void StrikeDance(const Blow& blow, const Cut& cut);
        void StrikeThread(const Blow& blow, const Cut& cut);
        void StrikeGlacier(const Blow& blow, const Cut& cut);
        void StrikeWraith(const Blow& blow, const Cut& cut);
        void Shed(int hand, const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to, float shown);
        void HandOver(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to);   // (Mixed) a blade's point as a move of another way begins

        // ---- drawing: every piece into one mesh
        void Quad(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, const DirectX::XMFLOAT3& c, const DirectX::XMFLOAT3& d, Cell shape, float u0, float u1,
                  const DirectX::XMFLOAT3& colourA, const DirectX::XMFLOAT3& colourB, float alphaA, float alphaB, float wornA, float wornB, float soft, float light, float depth);
        void DrawMotes(const DirectX::XMFLOAT3& eye);
        void DrawRibbons(const DirectX::XMFLOAT3& eye);
        void DrawArcs(const DirectX::XMFLOAT3& eye);
        bool Seen(const DirectX::XMFLOAT3& at, float& u, float& v) const;

        bool mLoaded = false, mFailed = false;
        int mGear = -1, mStyle = -1;
        bool mOwn = false;   // (Choose's own: only THE HIGHLANDS' third has such a form)
        bool mPlain = false; // (Choose's plain: as she sets out)
        int mUse = 0, mHandFrom = 0;   // (Mixed: the way of the move she is in, and of the one before it)
        bool mHandOwed[2]{};           // (Mixed: a blade's hand-over is owed at its next point)
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mAtlas, mNoise;
        DoorwaysGpuMesh mMesh;
        std::vector<DoorwaysGpuDMeshVertex> mVertices;
        struct Piece { float Depth; std::uint32_t First; };   // a covering quad: sorted, far ones first
        std::vector<Piece> mCovering;
        std::vector<std::uint32_t> mLights, mIndices;
        DirectX::XMFLOAT4X4 mViewProjection{};
        DirectX::XMFLOAT3 mEye{};
        bool mSeenOnce = false;
        int mQuadsDrawn = 0;

        std::vector<Mote> mMotes;
        std::vector<Arc> mArcs;
        std::vector<TrailPoint> mTrail[2];
        float mUnseen[2]{1, 1}, mSince[2]{};   // s since a blade was last told of, and since its trail's last point
        TrailPoint mHead[2]; bool mHeadOn[2]{};   // a trail's newest end, where it is not one of its points (AgeTrails)
        std::vector<TrailPoint> mTrailDrawn;      // (a trail and its end, as drawn)
        DirectX::XMFLOAT3 mHerAt{}; bool mHerTold = false;
        float mSinceMoved = 1;                    // s since the engine last moved her
        bool mMovedSince[2]{};                    // she has been moved since that trail's last point
        float mShedOwed[2]{}, mWhirlOwed = 0, mDashOwed = 0;
        bool mDashFirst = false;                // (a dash was begun: its flash is owed as soon as its way is known)
        float mWait = 0;                        // (what is added now begins so much later)
        int mArcsMade = 0;
        float mFloor = 0;                       // the ground's height where the last blow was (where none is told: the lab)
        struct Pending { float In; DirectX::XMFLOAT3 At; float Blur, Split, Stop, Shake; };
        std::vector<Pending> mPending;
        Feel mFeel;
        const Ground* mGround = nullptr;
        std::uint32_t mSeed = 0x2545F491u;
        // the picture as a whole
        float mBlur = 0, mSplit = 0; DirectX::XMFLOAT3 mPunchAt{};
        struct Bent { DirectX::XMFLOAT3 At{}; float T = 0, Life = 0, Strength = 0, Reach = 1; };
        Bent mBent[2];
        DirectX::XMFLOAT3 mTint{}; float mTintAmount = 0;
        bool mScreenSet = false;   // (the picture's flair was set last frame: to be taken away when it goes)
    };
}
