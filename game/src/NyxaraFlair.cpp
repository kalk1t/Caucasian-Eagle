#include "NyxaraFlair.h"

#include "DoorwaysRuntimePathResolver.h"
#include "Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace DirectX;

namespace Doorways
{
    namespace
    {
        using V3 = XMFLOAT3;
        constexpr float Pi = 3.14159265f, TwoPi = 6.2831853f;
        constexpr std::size_t MaxMotes = 6000, MaxQuads = 14000;
        constexpr int AtlasCols = 4, AtlasRows = 6;   // the atlas's cells (art/nyxara/flair/make_flair_textures.py)
        // The picture's glow: what is brighter than this (the scene's own light, before exposure) bleeds out round it.
        constexpr float GlowFrom = 1.35f, GlowKnee = .6f;
        constexpr V3 Up{0, 1, 0}, White{1, 1, 1};

        inline V3 operator+(const V3& a, const V3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
        inline V3 operator-(const V3& a, const V3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
        inline V3 operator*(const V3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
        inline float Dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        inline V3 Cross(const V3& a, const V3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
        inline float Len(const V3& a) { return std::sqrt(Dot(a, a)); }
        inline V3 Norm(const V3& a, const V3& otherwise = {0, 1, 0}) { const float l = Len(a); return l > 1e-6f ? a * (1 / l) : otherwise; }
        inline float Mix(float a, float b, float t) { return a + (b - a) * t; }
        inline V3 Mix(const V3& a, const V3& b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; }
        inline float Smooth(float a, float b, float x)
        {
            const float t = std::clamp((x - a) / (std::max)(b - a, 1e-5f), 0.f, 1.f);
            return t * t * (3 - 2 * t);
        }
        inline float F(int i) { return static_cast<float>(i); }
        inline V3 Turn(const V3& v, const V3& axis, float angle)   // (about a unit axis)
        {
            const float c = std::cos(angle), s = std::sin(angle);
            return v * c + Cross(axis, v) * s + axis * (Dot(axis, v) * (1 - c));
        }
        // A curve through b and c, going by a and d (Catmull-Rom).
        inline V3 Curve(const V3& a, const V3& b, const V3& c, const V3& d, float u)
        {
            const float u2 = u * u, u3 = u2 * u;
            const float wa = .5f * (-u3 + 2 * u2 - u), wb = .5f * (3 * u3 - 5 * u2 + 2), wc = .5f * (-3 * u3 + 4 * u2 + u), wd = .5f * (u3 - u2);
            return a * wa + b * wb + c * wc + d * wd;
        }
        inline V3 Round(float angle, float y = 0) { return {std::cos(angle), y, std::sin(angle)}; }
    }

    // One of the nine: its words, its colours, the bands of its blades' trails, the feel of its blows.
    //
    // Its colours are light before the picture's tone curve, and are kept far from white: under that curve a light keeps
    // its hue only while its other two channels stay low, however bright it is (a pale colour made brighter is white:
    // the first film of these, 2026-11-25, was all white). So a body of light is one strong channel at 1 to 3, and only
    // what is thin (an edge, a line, a spark newly struck) is brighter and whiter; the glow carries the colour out.
    struct NyxaraFlair::Look
    {
        Info Text;
        V3 Main{}, Core{}, Deep{}, Dark{};   // its light's body, its hottest, its tail; its smoke
        std::vector<Ribbon> Ribbons;
        float Fast0 = 4, Fast1 = 11;         // her blade's point: the speed (m/s, as her moves are made) its trail begins to show at, and shows whole
        float Glow = .8f;                    // the picture's glow, how strong
        float NoiseSize = .55f, NoiseX = .1f, NoiseY = .35f, NoiseWear = .5f;   // the noise that wears its light unevenly: its size, its drift, how much
        float Stop[3]{.03f, .045f, .085f}, Shake[3]{.05f, .09f, .2f};          // a blow in a creature, by its weight: the moment held (s), the view shaken
        float Slow = 0, SlowRate = 1;        // its heaviest blow in a creature: the world slowed so long, to this
        float ShedEvery = 0;                 // her blade sheds a mote each so many metres its point goes (0: none)
    };

    const NyxaraFlair::Look& NyxaraFlair::LookOf(int gear, int style, bool own)
    {
        static const std::array<Look, 10> all = []
        {
            std::array<Look, 10> a;
            const auto band = [](Cell shape, float life, float inner, float outer, const V3& c0, const V3& c1, float alpha, float worn0, float worn1, float soft, float light)
            {
                Ribbon r; r.Shape = shape; r.Life = life; r.Inner = inner; r.Outer = outer; r.Color0 = c0; r.Color1 = c1;
                r.Alpha = alpha; r.Worn0 = worn0; r.Worn1 = worn1; r.Soft = soft; r.Light = light;
                return r;
            };
            const auto fast = [](Look& l) { const float stop[3]{.03f, .045f, .085f}, shake[3]{.05f, .09f, .2f}; std::copy(stop, stop + 3, l.Stop); std::copy(shake, shake + 3, l.Shake); };
            const auto heavy = [](Look& l)
            {
                const float stop[3]{.075f, .11f, .17f}, shake[3]{.15f, .24f, .45f};
                std::copy(stop, stop + 3, l.Stop); std::copy(shake, shake + 3, l.Shake); l.Slow = .3f; l.SlowRate = .3f;
            };
            const auto flowing = [](Look& l) { const float stop[3]{.02f, .03f, .065f}, shake[3]{.04f, .07f, .16f}; std::copy(stop, stop + 3, l.Stop); std::copy(shake, shake + 3, l.Shake); };
            // Each trail in layers: first what covers (its colour deep and strong, so that it is its colour over a bright
            // sky too), then its body of light, then its white-hot edge at the blade's point.
            {   // ---- THE HIGHLANDS: wind and sky, and the gold of her harness
                Look& l = a[0];
                l.Text = {L"THE HIGHLANDS", L"GALE EDGE", L"fast and sharp: thin blue trails, a glint and a line of wind through what she cuts", .85f, 1.15f};
                l.Main = {.05f, .3f, 1.f}; l.Core = {.5f, .78f, 1.f}; l.Deep = {.02f, .1f, .9f}; l.Dark = {.6f, .62f, .66f};
                l.Ribbons = {band(Cell::SlashSilk, .17f, .2f, 1.08f, l.Deep * .55f, l.Deep * .3f, .5f, 0, .8f, .4f, 0),
                             band(Cell::SlashRazor, .16f, .25f, 1.06f, l.Main * 2.2f, l.Deep * 1.1f, .9f, 0, .75f, .25f, .4f),
                             band(Cell::SlashRazor, .09f, .86f, 1.05f, l.Core * 5.f, l.Main * 2.5f, 1, 0, .5f, .3f, 1)};
                l.NoiseWear = .35f; l.ShedEvery = .5f; l.Glow = .8f; fast(l);
            }
            {
                Look& l = a[1];
                l.Text = {L"THE HIGHLANDS", L"STONEBREAKER", L"heavy: golden bursts of rays, dust and chips of stone, the ground cracked", .6f, .9f};
                l.Main = {1.f, .42f, .04f}; l.Core = {1.f, .78f, .38f}; l.Deep = {1.f, .2f, .015f}; l.Dark = {.5f, .44f, .36f};
                l.Ribbons = {band(Cell::SlashHeavy, .28f, 0, 1.15f, {1.f, .3f, .02f}, {.4f, .08f, .008f}, .75f, 0, .85f, .4f, 0),
                             band(Cell::SlashHeavy, .26f, .05f, 1.12f, l.Main * 2.4f, l.Deep * 1.1f, .92f, 0, .8f, .3f, .4f),
                             band(Cell::SlashRazor, .1f, .85f, 1.1f, l.Core * 5.f, l.Main * 2.5f, 1, 0, .5f, .3f, 1)};
                l.NoiseWear = .5f; l.ShedEvery = .45f; l.Glow = .85f; heavy(l);
            }
            {
                Look& l = a[2];
                l.Text = {L"THE HIGHLANDS", L"SKY WHEEL", L"flowing: wide blue trails that hang, golden rings and drifting lights, feathers", .75f, 1.f};
                l.Main = {.05f, .3f, 1.f}; l.Core = {.5f, .78f, 1.f}; l.Deep = {.02f, .1f, .9f}; l.Dark = {.6f, .62f, .66f};
                l.Ribbons = {band(Cell::SlashSilk, .45f, -.05f, 1.15f, l.Deep * .5f, l.Deep * .25f, .45f, .05f, .8f, .5f, 0),
                             band(Cell::SlashSilk, .42f, 0, 1.1f, l.Main * 2.f, l.Deep * 1.f, .85f, .05f, .7f, .5f, .4f),
                             band(Cell::SlashRazor, .3f, .8f, 1.08f, V3{1.f, .42f, .04f} * 3.5f, V3{1.f, .2f, .015f} * 1.2f, .9f, 0, .6f, .3f, 1)};
                l.NoiseWear = .45f; l.NoiseY = .2f; l.ShedEvery = .22f; l.Glow = .85f; flowing(l);
            }
            {   // ---- AMIRANI'S FIRE
                Look& l = a[3];
                l.Text = {L"AMIRANI'S FIRE", L"CINDER RAZOR", L"fast and sharp: a white-hot edge, a burning line left in the air, sparks that bounce", .85f, 1.15f};
                l.Main = {1.f, .14f, .01f}; l.Core = {1.f, .78f, .38f}; l.Deep = {1.f, .05f, .01f}; l.Dark = {.05f, .02f, .02f};
                l.Ribbons = {band(Cell::SlashSilk, .16f, .3f, 1.08f, {1.1f, .07f, .008f}, {.5f, .02f, .004f}, .8f, 0, .8f, .4f, 0),
                             band(Cell::SlashRazor, .14f, .4f, 1.05f, l.Main * 2.6f, l.Deep * 1.3f, .9f, 0, .7f, .22f, .45f),
                             band(Cell::SlashRazor, .55f, .9f, 1.03f, l.Core * 5.f, l.Deep * 1.2f, 1, 0, .5f, .3f, 1)};
                l.NoiseWear = .4f; l.NoiseY = .5f; l.ShedEvery = .4f; l.Glow = .9f; fast(l);
            }
            {
                Look& l = a[4];
                l.Text = {L"AMIRANI'S FIRE", L"FORGE HAMMER", L"heavy: bursts of fire, smoke and embers thrown up, the ground scorched and cracked", .6f, .9f};
                l.Main = {1.f, .25f, .02f}; l.Core = {1.f, .78f, .38f}; l.Deep = {.8f, .04f, .008f}; l.Dark = {.05f, .025f, .02f};
                l.Ribbons = {band(Cell::SlashSilk, .5f, .1f, 1.2f, l.Dark, l.Dark * .7f, .55f, .2f, .9f, .5f, 0),
                             band(Cell::SlashHeavy, .3f, 0, 1.15f, l.Main * 2.8f, l.Deep * 1.1f, 1, 0, .85f, .3f, .45f),
                             band(Cell::SlashRazor, .1f, .86f, 1.1f, l.Core * 5.f, l.Main * 2.5f, 1, 0, .5f, .3f, 1)};
                l.NoiseSize = .8f; l.NoiseY = .9f; l.NoiseWear = .7f; l.ShedEvery = .25f; l.Glow = .95f; heavy(l);
            }
            {
                Look& l = a[5];
                l.Text = {L"AMIRANI'S FIRE", L"FIRE DANCE", L"flowing: sheets of flame behind her blades, embers turning round her", .75f, 1.f};
                l.Main = {1.f, .2f, .015f}; l.Core = {1.f, .78f, .38f}; l.Deep = {.9f, .05f, .01f}; l.Dark = {.05f, .025f, .02f};
                l.Ribbons = {band(Cell::SlashSilk, .5f, -.1f, 1.2f, {1.f, .06f, .008f}, {.4f, .015f, .003f}, .75f, .1f, .85f, .5f, 0),
                             band(Cell::SlashSilk, .5f, -.05f, 1.15f, l.Main * 2.4f, l.Deep * 1.1f, .9f, .1f, .8f, .45f, .45f),
                             band(Cell::SlashRazor, .2f, .7f, 1.06f, l.Core * 4.5f, V3{1.f, .25f, .03f} * 1.8f, 1, 0, .6f, .3f, 1)};
                l.NoiseSize = .9f; l.NoiseX = .15f; l.NoiseY = 1.1f; l.NoiseWear = .85f; l.ShedEvery = .16f; l.Glow = .9f; flowing(l);
            }
            {   // ---- MOONLIT
                Look& l = a[6];
                l.Text = {L"MOONLIT", L"SILVER THREAD", L"fast and sharp: hairlines of silver, a violet arc left hanging, needles of ice, a web of cuts at the end", .85f, 1.15f};
                l.Main = {.3f, .1f, 1.f}; l.Core = {.7f, .82f, 1.f}; l.Deep = {.12f, .03f, .9f}; l.Dark = {.5f, .55f, .7f};
                l.Ribbons = {band(Cell::SlashSilk, .13f, .25f, 1.06f, {.1f, .03f, .5f}, {.04f, .01f, .25f}, .42f, 0, .8f, .4f, 0),
                             band(Cell::SlashRazor, .1f, .3f, 1.05f, l.Main * 2.f, l.Deep * 1.1f, .85f, 0, .6f, .3f, .45f),
                             band(Cell::SlashRazor, .6f, .93f, 1.03f, V3{.5f, .4f, 1.f} * 3.f, l.Main * 1.2f, 1, 0, .4f, .3f, 1),
                             band(Cell::SlashRazor, .12f, .82f, 1.04f, l.Core * 6.f, V3{.5f, .55f, 1.f} * 2.5f, 1, 0, .5f, .3f, 1)};
                l.NoiseWear = .25f; l.ShedEvery = .3f; l.Glow = .9f; fast(l);
            }
            {
                Look& l = a[7];
                l.Text = {L"MOONLIT", L"GLACIER", L"heavy: great pale crescents, spikes of ice out of the ground, snow thrown up", .6f, .9f};
                l.Main = {.1f, .4f, 1.f}; l.Core = {.55f, .82f, 1.f}; l.Deep = {.04f, .18f, 1.f}; l.Dark = {.82f, .88f, .96f};
                l.Ribbons = {band(Cell::SlashSilk, .34f, -.12f, 1.28f, {.03f, .12f, .6f}, {.01f, .05f, .3f}, .45f, .05f, .85f, .45f, 0),
                             band(Cell::SlashSilk, .55f, .1f, 1.25f, V3{.6f, .78f, 1.f} * .8f, V3{.6f, .78f, 1.f} * .6f, .4f, .15f, .9f, .5f, .35f),
                             band(Cell::SlashHeavy, .3f, -.1f, 1.25f, V3{.14f, .5f, 1.f} * 2.4f, V3{.08f, .35f, 1.f} * 1.1f, .95f, 0, .8f, .35f, .45f),
                             band(Cell::SlashRazor, .1f, .9f, 1.27f, White * 4.5f, l.Core * 2.f, 1, 0, .5f, .3f, 1)};
                l.NoiseWear = .5f; l.NoiseY = .15f; l.ShedEvery = .22f; l.Glow = .9f; heavy(l);
            }
            {
                Look& l = a[8];
                l.Text = {L"MOONLIT", L"MOON WRAITH", L"flowing: teal trails and their pale echo a moment after, dark smoke, wisps round what she strikes", .75f, 1.f};
                l.Main = {.03f, .85f, .6f}; l.Core = {.55f, 1.f, .9f}; l.Deep = {.3f, .1f, 1.f}; l.Dark = {.012f, .05f, .06f};
                l.Ribbons = {band(Cell::SlashSilk, .45f, 0, 1.1f, l.Dark, l.Dark * .7f, .6f, .25f, .95f, .5f, 0),
                             band(Cell::SlashSilk, .55f, -.1f, 1.15f, l.Main * 2.f, l.Deep * .9f, .88f, .1f, .75f, .55f, .45f),
                             band(Cell::SlashRazor, .25f, .85f, 1.05f, l.Core * 4.f, l.Main * 1.5f, 1, 0, .6f, .3f, 1)};
                l.NoiseSize = .7f; l.NoiseX = .1f; l.NoiseY = .3f; l.NoiseWear = .6f; l.ShedEvery = .25f; l.Glow = .9f; flowing(l);
            }
            {   // ---- SKY WHEEL in her own blue, THE HIGHLANDS' gold not yet hers (Choose's own): how she set out for a day
                // (2026-11-25), kept to compare. As she sets out now she has no light (Choose's plain), and of this only
                // its pace and its feel.
                Look& l = a[9];
                l = a[2];
                l.Text = {L"AS SHE SET OUT", L"SKY WHEEL", L"flowing, in her own blue: wide trails that hang, pale rings and drifting lights", .75f, 1.f};
                l.Ribbons[2] = band(Cell::SlashRazor, .3f, .8f, 1.08f, l.Core * 4.f, l.Main * 1.5f, .9f, 0, .6f, .3f, 1);
                l.ShedEvery = .3f; l.Glow = .8f;
            }
            return a;
        }();
        const int g = std::clamp(gear, 0, GearCount - 1), s = std::clamp(style, 0, StylesPerGear - 1);
        return all[own && g == 0 && s == 2 ? std::size_t{9} : static_cast<std::size_t>(g * StylesPerGear + s)];
    }

    NyxaraFlair::Info NyxaraFlair::Describe(int gear, int style, bool own, bool plain)
    {
        Info info = LookOf(gear, style, own).Text;
        if (plain)
        {
            info.Gear = L"AS SHE SET OUT"; info.Name = L"HER BLADES ALONE";
            info.What = L"no light yet: her cuts, their blood and their weight; her gears bring her the wind, the fire and the ice";
        }
        return info;
    }
    // (SKY WHEEL's gold: as she set out, her own blue, paler)
    V3 NyxaraFlair::WheelGold() const { return mOwn ? V3{.16f, .5f, 1.f} : V3{1.f, .42f, .04f}; }
    V3 NyxaraFlair::WheelHot() const { return mOwn ? V3{.5f, .78f, 1.f} : V3{1.f, .78f, .38f}; }
    V3 NyxaraFlair::WheelEmber() const { return mOwn ? V3{.1f, .4f, 1.f} : V3{1.f, .5f, .07f}; }

    bool NyxaraFlair::Load(Renderer& renderer, std::string& error)
    {
        if (mLoaded) return true;
        if (mFailed) return false;
        const auto root = ResolveDoorwaysRuntimePath("assets/Compiled/Effects/Nyxara/Flair");
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
        if (!load("FlairAtlas.png", mAtlas) || !load("FlairNoise.png", mNoise) ||
            !renderer.CreateDoorwaysDynamicGpuMesh(static_cast<unsigned int>(MaxQuads * 4), static_cast<unsigned int>(MaxQuads * 6), mMesh, &error))
        {
            mFailed = true;
            return false;
        }
        mVertices.reserve(MaxQuads * 4); mIndices.reserve(MaxQuads * 6);
        mLoaded = true;
        return true;
    }

    void NyxaraFlair::Choose(int gear, int style, bool own, bool plain)
    {
        const int g = gear < 0 || style < 0 ? -1 : std::clamp(gear, 0, GearCount - 1), s = g < 0 ? -1 : std::clamp(style, 0, StylesPerGear - 1);
        const bool o = own && g == 0 && s == 2, bare = plain && g >= 0;
        if (g == mGear && s == mStyle && o == mOwn && bare == mPlain) return;
        mGear = g; mStyle = s; mOwn = o; mPlain = bare;
        Clear();
    }

    void NyxaraFlair::Clear()
    {
        mMotes.clear(); mArcs.clear(); mPending.clear();
        for (int k = 0; k < 2; ++k) { mTrail[k].clear(); mShedOwed[k] = 0; mUnseen[k] = 1; mSince[k] = 0; mBent[k] = {}; mHeadOn[k] = false; mMovedSince[k] = false; }
        mWhirlOwed = mDashOwed = mWait = 0;
        mDashFirst = false;
        mFeel = {};
        mBlur = mSplit = mTintAmount = 0;
    }

    float NyxaraFlair::Random()
    {
        mSeed ^= mSeed << 13; mSeed ^= mSeed >> 17; mSeed ^= mSeed << 5;
        return static_cast<float>(mSeed & 0xFFFFFFu) / 16777216.f;
    }
    V3 NyxaraFlair::RandomWay()
    {   // any way at all, evenly
        const float y = Random(-1.f, 1.f), a = Random(0.f, TwoPi), r = std::sqrt((std::max)(0.f, 1 - y * y));
        return {r * std::cos(a), y, r * std::sin(a)};
    }
    V3 NyxaraFlair::Cone(const V3& about, float spread) { return Norm(Norm(about) + RandomWay() * spread); }
    float NyxaraFlair::GroundAt(const V3& at, float fallback) const
    {
        if (mGround && *mGround) { const float y = (*mGround)(at.x, at.z, at.y); if (std::isfinite(y)) return y; }
        return fallback;
    }

    // ------------------------------------------------------------------ her blades' trails
    void NyxaraFlair::AgeTrails(float poseDt, const V3& her)
    {
        if (poseDt <= 0) return;
        // (has the engine moved her since the last frame?)
        const bool moved = mHerTold && Len(her - mHerAt) > 1e-4f;
        mHerAt = her; mHerTold = true;
        mSinceMoved = moved ? 0.f : mSinceMoved + poseDt;
        if (moved) for (bool& m : mMovedSince) m = true;
        float longest = 0;
        if (Chosen()) for (const Ribbon& r : Now().Ribbons) longest = (std::max)(longest, r.Life);
        for (int k = 0; k < 2; ++k)
        {
            mUnseen[k] += poseDt; mSince[k] += poseDt;
            auto& trail = mTrail[k];
            for (TrailPoint& q : trail) q.Age += poseDt;
            mHead[k].Age += poseDt;
            // (the oldest go; the last one too old is kept while one after it is young, so that the band reaches its end)
            std::size_t gone = 0;
            while (gone + 1 < trail.size() && trail[gone + 1].Age > longest) ++gone;
            if (!trail.empty() && gone + 1 == trail.size() && trail.back().Age > longest) gone = trail.size();
            if (gone) trail.erase(trail.begin(), trail.begin() + static_cast<std::ptrdiff_t>(gone));
            if (!trail.empty()) trail.front().Fresh = true;
        }
    }

    void NyxaraFlair::Blade(int hand, const V3& grip, const V3& tip, float poseDt, float pace, bool fresh)
    {
        if (!On() || hand < 0 || hand > 1) return;
        auto& trail = mTrail[hand];
        const Look& look = Now();
        TrailPoint q; q.Grip = grip; q.Tip = tip;
        q.Fresh = fresh || trail.empty() || mUnseen[hand] > poseDt * 1.5f + .02f;
        mUnseen[hand] = 0;
        if (q.Fresh && mHeadOn[hand] && !trail.empty()) trail.push_back(mHead[hand]);   // (what was its end stays its end)
        mHeadOn[hand] = false;
        if (!q.Fresh)
        {
            const TrailPoint& was = trail.back();
            const float went = Len(tip - was.Tip);
            if (went < .003f && Len(grip - was.Grip) < .003f) return;   // (held still: the moment a blow is held)
            const float speed = went / (std::max)(mSince[hand], 1e-4f) / (std::max)(pace, .1f);
            q.Shown = Smooth(look.Fast0, look.Fast1, speed);
            // She is being moved (within the last step and a half), and has not been since this trail's last point:
            // her blade has gone on and she has not yet. This is the band's end for now, not one of its points.
            if (mSinceMoved < 1.5f / 60.f && !mMovedSince[hand]) { mHead[hand] = q; mHeadOn[hand] = true; return; }
            if (q.Shown > .05f && !mPlain) Shed(hand, was.Tip, tip, q.Shown);   // (plain: her blade's way is kept for a cut's blood, and nothing shed or drawn)
        }
        mSince[hand] = 0; mMovedSince[hand] = false;
        trail.push_back(q);
        if (trail.size() > 400) trail.erase(trail.begin(), trail.begin() + 100);
    }

    void NyxaraFlair::DrawRibbons(const V3& eye)
    {
        const Look& look = Now();
        struct Fine { V3 Grip, Dir; float Length, Age, Shown; bool Fresh; };
        std::vector<Fine> fine;
        for (int hand = 0; hand < 2; ++hand)
        {
            const std::vector<TrailPoint>* drawn = &mTrail[hand];
            if (mHeadOn[hand] && !mTrail[hand].empty())
            {   // (its newest end, where that is not one of its points: Blade)
                mTrailDrawn = mTrail[hand];
                mTrailDrawn.push_back(mHead[hand]);
                drawn = &mTrailDrawn;
            }
            const auto& trail = *drawn;
            if (trail.size() < 2) continue;
            fine.clear();
            const auto dirOf = [&](std::size_t i) { return Norm(trail[i].Tip - trail[i].Grip); };
            // Between two frames her blade may have turned a long way: its handle is taken along a curve through where
            // it was, its direction along one through what it was, so that the band goes round with the blade.
            for (std::size_t i = 0; i < trail.size(); ++i)
            {
                const TrailPoint& p2 = trail[i];
                const V3 d2 = dirOf(i);
                if (i == 0 || p2.Fresh) { fine.push_back({p2.Grip, d2, Len(p2.Tip - p2.Grip), p2.Age, p2.Shown, true}); continue; }
                const TrailPoint& p1 = trail[i - 1];
                const std::size_t i0 = i >= 2 && !p1.Fresh ? i - 2 : i - 1, i3 = i + 1 < trail.size() && !trail[i + 1].Fresh ? i + 1 : i;
                const V3 d0 = dirOf(i0), d1 = dirOf(i - 1), d3 = dirOf(i3);
                const float angle = std::acos(std::clamp(Dot(d1, d2), -1.f, 1.f)), went = Len(p2.Tip - p1.Tip);
                const int steps = std::clamp(static_cast<int>(std::ceil((std::max)(angle / .09f, went / .2f))), 1, 28);
                const float l1 = Len(p1.Tip - p1.Grip), l2 = Len(p2.Tip - p2.Grip);
                for (int s = 1; s <= steps; ++s)
                {
                    const float u = F(s) / F(steps);
                    fine.push_back({Curve(trail[i0].Grip, p1.Grip, p2.Grip, trail[i3].Grip, u), Norm(Curve(d0, d1, d2, d3, u), d2), Mix(l1, l2, u),
                                    Mix(p1.Age, p2.Age, u), Mix(p1.Shown, p2.Shown, u), false});
                }
            }
            for (std::size_t k = 0; k < look.Ribbons.size(); ++k)
            {
                const Ribbon& r = look.Ribbons[k];
                const float over = 1 - .002f * static_cast<float>(k);   // (of its bands that cover, the later over the earlier)
                for (std::size_t j = 0; j + 1 < fine.size(); ++j)
                {
                    const Fine& f0 = fine[j]; const Fine& f1 = fine[j + 1];   // (f1 the younger)
                    if (f1.Fresh || (f0.Shown <= .01f && f1.Shown <= .01f)) continue;
                    const float u1 = f1.Age / r.Life, u0 = (std::min)(f0.Age / r.Life, 1.f);
                    if (u1 >= 1) continue;
                    const V3 o0 = f0.Grip + f0.Dir * (f0.Length * r.Outer), i0 = f0.Grip + f0.Dir * (f0.Length * r.Inner);
                    const V3 o1 = f1.Grip + f1.Dir * (f1.Length * r.Outer), i1 = f1.Grip + f1.Dir * (f1.Length * r.Inner);
                    const float a0 = r.Alpha * f0.Shown * (1 - Smooth(.7f, 1.f, u0)), a1 = r.Alpha * f1.Shown * (1 - Smooth(.7f, 1.f, u1));
                    const V3 toEye = eye - (o0 + i1) * .5f;
                    Quad(o0, o1, i1, i0, r.Shape, u0, u1, Mix(r.Color0, r.Color1, u0), Mix(r.Color0, r.Color1, u1), a0, a1,
                         Mix(r.Worn0, r.Worn1, u0), Mix(r.Worn0, r.Worn1, u1), r.Soft, r.Light, Dot(toEye, toEye) * over);
                }
            }
        }
    }

    // ------------------------------------------------------------------ crescents
    void NyxaraFlair::Crescent(const Cut& cut, Cell shape, float scale, float wide, float back, float ahead, float sweep, const V3& c0, const V3& c1, float alpha,
                               float life, float drift, float grow, float light)
    {
        if (mArcs.size() > 64) mArcs.erase(mArcs.begin());
        Arc a; a.Shape = shape; a.Middle = cut.Middle; a.A = cut.Edge; a.B = cut.Way;
        a.Radius = cut.Radius * scale; a.Wide = (std::min)(wide, a.Radius * .85f); a.From = -back; a.To = ahead;
        a.Sweep = sweep; a.Life = life; a.Grow = grow; a.Drift = cut.Forward * drift; a.T = -mWait;
        a.Color0 = c0; a.Color1 = c1; a.Alpha = alpha; a.Light = light; a.Order = mArcsMade++ % 16;
        mArcs.push_back(a);
    }
    // A blow's crescent in its layers: what covers (under: deep and strong, so that it is its colour over a bright sky
    // too), its body (mostly covering as well: its own colour whatever is behind it), and a white-hot rim along its
    // outer edge, gone first.
    void NyxaraFlair::Slash(const Cut& cut, Cell shape, float scale, float wide, float back, float ahead, float sweep, const V3& body, const V3& tail,
                            const V3& under, const V3& rim, float life, float drift, float grow, float underAlpha)
    {
        Crescent(cut, Cell::SlashSilk, scale * 1.03f, wide * 1.15f, back, ahead, sweep, under, under * .5f, underAlpha, life * 1.1f, drift, grow, 0);
        Crescent(cut, shape, scale, wide, back, ahead, sweep, body, tail, .92f, life, drift, grow, .4f);
        Crescent(cut, Cell::SlashRazor, scale * 1.01f, wide * .35f, back, ahead, sweep, rim, body, 1, life * .75f, drift, grow, 1);
    }
    // ... and a whole ring of it round her, level, as she turns
    void NyxaraFlair::Wheel(const V3& middle, const Cut& cut, Cell shape, float radius, float wide, const V3& c0, const V3& c1, float alpha, float life)
    {
        if (mArcs.size() > 64) mArcs.erase(mArcs.begin());
        Arc a; a.Shape = shape; a.Middle = middle; a.A = cut.Forward; a.B = cut.Side;
        a.Radius = radius; a.Wide = wide; a.From = 0; a.To = TwoPi * .97f; a.Pointed = false;
        a.Sweep = .12f; a.Life = life; a.Grow = .22f; a.T = -mWait;
        a.Color0 = c0; a.Color1 = c1; a.Alpha = alpha; a.Light = .45f; a.Order = mArcsMade++ % 16;
        mArcs.push_back(a);
    }

    void NyxaraFlair::DrawArcs(const V3& eye)
    {
        for (const Arc& a : mArcs)
        {
            if (a.T < 0 || a.T >= a.Life) continue;
            const float t = a.T / a.Life, whole = a.To - a.From;
            const float head = a.From + whole * Smooth(0, (std::max)(a.Sweep, 1e-3f), a.T), span = head - a.From;
            if (std::abs(span) < .02f || std::abs(whole) < .02f) continue;
            const float radius = a.Radius * (1 + a.Grow * (1 - (1 - t) * (1 - t)));
            const float worn = a.Worn1 * Smooth(.12f, 1.f, t), alpha = a.Alpha * (1 - Smooth(.5f, 1.f, t));
            const int n = std::clamp(static_cast<int>(std::abs(span) / .08f) + 1, 2, 90);
            const auto point = [&](float angle, float r) { return a.Middle + (a.A * std::cos(angle) + a.B * std::sin(angle)) * r; };
            const auto wideAt = [&](float angle)
            {
                if (!a.Pointed) return a.Wide;
                return a.Wide * std::pow((std::max)(0.f, std::sin(Pi * std::clamp((angle - a.From) / whole, 0.f, 1.f))), .55f);
            };
            const V3 toEye = eye - a.Middle;
            const float depth = Dot(toEye, toEye) * (1 - .002f * F(a.Order));   // (of two that cover at one place, the later made is drawn over)
            for (int i = 0; i < n; ++i)
            {
                const float th0 = a.From + span * F(i) / F(n), th1 = a.From + span * F(i + 1) / F(n);
                const float u0 = (head - th0) / whole, u1 = (head - th1) / whole;   // (0 at its head: the texture's bright end)
                Quad(point(th0, radius), point(th1, radius), point(th1, radius - wideAt(th1)), point(th0, radius - wideAt(th0)), a.Shape, u0, u1,
                     Mix(a.Color0, a.Color1, (std::max)(u0, t)), Mix(a.Color0, a.Color1, (std::max)(u1, t)), alpha, alpha, worn, worn, a.Soft, a.Light, depth);
            }
        }
    }

    // ------------------------------------------------------------------ motes
    NyxaraFlair::Mote& NyxaraFlair::Add(Cell shape, Face facing, const V3& at)
    {
        if (mMotes.size() >= MaxMotes) mMotes.erase(mMotes.begin(), mMotes.begin() + static_cast<std::ptrdiff_t>(MaxMotes / 8));
        Mote m; m.Shape = shape; m.Facing = facing; m.Pos = at; m.Seed = Random(0.f, 100.f); m.T = -mWait;
        mMotes.push_back(m);
        return mMotes.back();
    }

    // Sparks: thin (size: how thick), drawn out along their flight, falling, and (bounce) off the ground.
    void NyxaraFlair::Sparks(const V3& at, const V3& way, int count, float speed, float spread, const V3& c0, const V3& c1, float size, float life, float fall, bool bounce)
    {
        const float floor = GroundAt(at, mFloor);
        for (int i = 0; i < count; ++i)
        {
            Mote& m = Add(Cell::Needle, Face::Along, at);
            m.Vel = Cone(way, spread) * (speed * Random(.3f, 1.f));
            m.Size0 = size * Random(.7f, 1.3f); m.Size1 = m.Size0 * .35f; m.Long = 3; m.LongBySpeed = .03f;
            m.Life = life * Random(.45f, 1.f); m.Color0 = c0; m.Color1 = c1; m.In = .02f; m.Out = .5f; m.Soft = .6f;
            m.Fall = fall; m.Drag = 1.6f; m.Bounce = bounce; m.Floor = floor; m.Flicker = .25f;
        }
    }
    // Embers and other small lights adrift: rising (rise over 0) or thrown and falling (under 0: so fast), wandering, turning (swirl).
    void NyxaraFlair::Embers(const V3& at, int count, float spread, float speed, const V3& c0, const V3& c1, float size, float life, float rise, float swirl)
    {
        const float floor = GroundAt(at, mFloor);
        for (int i = 0; i < count; ++i)
        {
            Mote& m = Add(Cell::Glow, Face::Eye, at + RandomWay() * (spread * Random()));
            m.Vel = RandomWay() * (speed * Random(.2f, 1.f));
            m.Vel.y = std::abs(m.Vel.y) * .8f + (rise > 0 ? rise * Random(.3f, 1.f) : 0.f);
            m.Size0 = size * Random(.6f, 1.4f); m.Size1 = m.Size0 * .25f;
            m.Life = life * Random(.45f, 1.f); m.Color0 = c0; m.Color1 = c1; m.In = .05f; m.Out = .5f;
            m.Fall = rise > 0 ? -rise * .6f : -rise; m.Drag = 1.8f; m.Wander = 2.5f; m.Swirl = swirl * Random(.5f, 1.f); m.Flicker = .45f;
            m.Bounce = rise < 0; m.Floor = floor;
        }
    }
    // A flash: a soft ball of light for a moment (light under 1: so much of it covers what is behind, and keeps its colour over a bright sky).
    void NyxaraFlair::Flash(const V3& at, float size, const V3& colour, float life, float alpha, float light)
    {
        Mote& m = Add(Cell::Glow, Face::Eye, at + Norm(mEye - at) * .3f);
        m.Size0 = size * .55f; m.Size1 = size * 1.2f; m.Life = life; m.Color0 = colour; m.Color1 = colour * .5f; m.Alpha = alpha; m.In = .1f; m.Out = .75f; m.Light = light;
    }
    // A glint: a star with a long thin line through it, level across the picture (roll: turned).
    void NyxaraFlair::Glint(const V3& at, float size, const V3& colour, float life, float wide, float roll)
    {
        Mote& m = Add(Cell::Star, Face::Eye, at + Norm(mEye - at) * .35f);
        m.Size0 = size; m.Size1 = size * .35f; m.Long = wide; m.Roll = roll;
        m.Life = life; m.Color0 = colour; m.Color1 = colour * .6f; m.In = .12f; m.Out = .6f;
    }
    // Rays shot out from a point (thick: how thick): all round it (seen across the eye's line), or about a way.
    void NyxaraFlair::Rays(const V3& at, int count, float length, const V3& c0, const V3& c1, float life, float speed, float thick, const V3* about)
    {
        const V3 toEye = Norm(mEye - at);
        for (int i = 0; i < count; ++i)
        {
            V3 dir = about ? Cone(*about, 1.1f) : RandomWay();
            if (!about) dir = Norm(dir - toEye * (Dot(dir, toEye) * .75f));
            Mote& m = Add(Cell::Needle, Face::Axis, at + dir * Random(.05f, .25f));
            m.AxisY = dir; m.Base = true; m.Vel = dir * (speed * Random(.5f, 1.f)); m.Drag = 6;
            m.Size0 = thick * Random(.6f, 1.2f); m.Size1 = m.Size0 * .25f; m.Length = length * Random(.45f, 1.f); m.Sprout = .04f;
            m.Life = life * Random(.6f, 1.f); m.Color0 = c0; m.Color1 = c1; m.In = .03f; m.Out = .6f;
        }
    }
    // A burst of many thin lines, as one picture turned to the eye, opening out.
    void NyxaraFlair::Lines(const V3& at, float size, const V3& colour, float life, float grow)
    {
        Mote& m = Add(Cell::Burst, Face::Eye, at + Norm(mEye - at) * .3f);
        m.Size0 = size * .45f; m.Size1 = size * grow; m.Roll = Random(0.f, TwoPi);
        m.Life = life; m.Color0 = colour; m.Color1 = colour * .4f; m.In = .05f; m.Out = .6f; m.Worn1 = .35f; m.Soft = .6f;
    }
    // One line along a way, its middle at a point (thick: how thick it is at its middle; pointed at both ends): drawn
    // across from one end (sprout), then thinning away.
    void NyxaraFlair::Line(const V3& at, const V3& way, float length, float thick, const V3& c0, const V3& c1, float life, float wait, float sprout)
    {
        Mote& m = Add(Cell::Needle, Face::Axis, at);
        m.AxisY = Norm(way, {1, 0, 0}); m.Length = length; m.Sprout = sprout;
        m.Size0 = thick; m.Size1 = thick * .15f; m.Life = life; m.T -= wait;
        m.Color0 = c0; m.Color1 = c1; m.In = .02f; m.Out = .55f;
    }
    // The glow of a line: wide and soft about a hairline (thick: the whole of it across), along a way, its middle at a point.
    void NyxaraFlair::Beam(const V3& at, const V3& way, float length, float thick, const V3& colour, float alpha, float life, float light, float wait)
    {
        Mote& m = Add(Cell::Streak, Face::Axis, at);
        m.AxisY = Norm(way, {1, 0, 0}); m.Length = length; m.Sprout = .03f;
        m.Size0 = thick; m.Size1 = thick * .55f; m.Life = life; m.T -= wait;
        m.Color0 = colour; m.Color1 = colour * .5f; m.Alpha = alpha; m.In = .03f; m.Out = .7f; m.Light = light;
    }
    // How a mote turned to the eye is rolled to lie along a way, as the eye sees it (as DrawMotes turns one).
    float NyxaraFlair::ScreenRoll(const V3& at, const V3& way) const
    {
        const V3 look = Norm(mEye - at), right = Norm(Cross(Up, look), {1, 0, 0}), over = Cross(look, right);
        return std::atan2(Dot(way, over), Dot(way, right));
    }
    // The mark of a cut at what it bites: a crescent turned to the eye and laid along the way the blade went as the eye
    // sees it, bowed to the outside of the stroke (bow), there in a moment and then torn away in streaks.
    void NyxaraFlair::Swipe(const V3& at, const V3& way, const V3& bow, float length, const V3& c0, const V3& c1, float alpha, float life, float light, float grow)
    {
        Mote& m = Add(Cell::Swipe, Face::Eye, at);
        m.Roll = ScreenRoll(at, way);
        {   // (its texture bows upward: where the stroke's outside is downward as it lies, it is turned right over and its ends changed back)
            const V3 look = Norm(mEye - at), right = Norm(Cross(Up, look), {1, 0, 0}), over = Cross(look, right);
            if (Dot(bow, over * std::cos(m.Roll) - right * std::sin(m.Roll)) < 0) { m.Roll += Pi; m.Flip = true; }
        }
        m.Size0 = length * .8f; m.Size1 = length * grow; m.Life = life; m.Color0 = c0; m.Color1 = c1; m.Alpha = alpha; m.In = .08f; m.Out = .6f;
        m.Worn0 = 0; m.Worn1 = .85f; m.Soft = .35f; m.Light = light;
    }
    // Smoke, dust, snow: soft, swelling, worn away (light: how much of it is light of its own).
    void NyxaraFlair::Puffs(const V3& at, int count, float spread, float size, const V3& colour, float alpha, float life, float rise, float light, const V3& push)
    {
        for (int i = 0; i < count; ++i)
        {
            Mote& m = Add(Random() < .5f ? Cell::Smoke : Cell::Smoke2, Face::Eye, at + RandomWay() * (spread * Random()));
            m.Vel = push * Random(.3f, 1.f) + RandomWay() * .6f + Up * (rise * Random(.5f, 1.f));
            m.Size0 = size * Random(.45f, .7f); m.Size1 = size * Random(1.3f, 1.9f);
            m.Roll = Random(0.f, TwoPi); m.Spin = Random(-.7f, .7f); m.Flip = Random() < .5f;
            m.Life = life * Random(.65f, 1.f); m.Color0 = colour; m.Color1 = colour * .65f; m.Alpha = alpha; m.In = .1f; m.Out = .65f;
            m.Worn0 = .05f; m.Worn1 = .7f; m.Soft = .5f; m.Light = light; m.Drag = 2.2f;
        }
    }
    // A ring opening out: on the ground, or in the plane of two axes. Thin, and worn away as it opens.
    void NyxaraFlair::RingOut(const V3& at, float from, float to, const V3& colour, float life, const V3* axisX, const V3* axisY)
    {
        const bool held = axisX && axisY;
        Mote& m = Add(Cell::Ring, held ? Face::Fixed : Face::Ground, at);
        if (held) { m.AxisX = Norm(*axisX, {1, 0, 0}); m.AxisY = Norm(*axisY, {0, 0, 1}); }
        else { m.AxisX = {1, 0, 0}; m.AxisY = {0, 0, 1}; m.Pos.y += .045f; }
        m.Size0 = from * 2.5f; m.Size1 = to * 2.5f;   // (the ring is at four fifths of its cell's half)
        m.Life = life; m.Color0 = colour; m.Color1 = colour * .3f; m.Alpha = .9f; m.In = .05f; m.Out = .75f; m.Worn0 = .05f; m.Worn1 = .55f; m.Soft = .5f;
    }
    // A ring of dust or of force going out, thick and broken, streaked the way it goes: on the ground, or in the plane
    // of two axes (light 0: dust, snow, smoke; 1: light).
    void NyxaraFlair::ShockRing(const V3& at, float from, float to, const V3& colour, float alpha, float life, float light, const V3* axisX, const V3* axisY)
    {
        const bool held = axisX && axisY;
        Mote& m = Add(Cell::Shock, held ? Face::Fixed : Face::Ground, at);
        if (held) { m.AxisX = Norm(*axisX, {1, 0, 0}); m.AxisY = Norm(*axisY, {0, 0, 1}); }
        else { m.AxisX = {1, 0, 0}; m.AxisY = {0, 0, 1}; m.Pos.y += .05f; }
        m.Roll = Random(0.f, TwoPi);
        m.Size0 = from * 2.35f; m.Size1 = to * 2.35f;   // (its front is at .85 of its cell's half)
        m.Life = life; m.Color0 = colour; m.Color1 = colour * .5f; m.Alpha = alpha; m.In = .06f; m.Out = .7f; m.Worn0 = .05f; m.Worn1 = .7f; m.Soft = .5f; m.Light = light;
    }
    // A mark lying on the ground as it slopes there: cracks, a stain.
    void NyxaraFlair::Mark(const V3& at, Cell shape, float size, const V3& c0, const V3& c1, float alpha, float life, float light, float in, float out, float roll)
    {
        const float y = GroundAt(at, at.y), e = .35f;
        const float hx = std::clamp(GroundAt({at.x + e, at.y, at.z}, y) - GroundAt({at.x - e, at.y, at.z}, y), -.4f, .4f);
        const float hz = std::clamp(GroundAt({at.x, at.y, at.z + e}, y) - GroundAt({at.x, at.y, at.z - e}, y), -.4f, .4f);
        const V3 normal = Norm({-hx / (2 * e), 1, -hz / (2 * e)});
        Mote& m = Add(shape, Face::Ground, {at.x, y + .035f + .012f * Random(), at.z});
        m.AxisX = Norm(Cross(normal, {0, 0, 1}), {1, 0, 0}); m.AxisY = Cross(m.AxisX, normal);
        m.Roll = roll >= 0 ? roll : Random(0.f, TwoPi);
        m.Size0 = m.Size1 = size; m.Life = life; m.Color0 = c0; m.Color1 = c1; m.Alpha = alpha; m.In = in; m.Out = out; m.Light = light; m.Soft = .6f;
    }
    // Crystals of ice thrown: long, falling, lying where they land.
    void NyxaraFlair::Crystals(const V3& at, const V3& way, int count, float speed, float spread, const V3& colour, float size)
    {
        const float floor = GroundAt(at, mFloor);
        for (int i = 0; i < count; ++i)
        {
            Mote& m = Add(Cell::Shard, Face::Along, at);
            m.LongV = true; m.Vel = Cone(way, spread) * (speed * Random(.35f, 1.f));
            m.Size0 = m.Size1 = size * Random(.5f, 1.1f); m.Long = 3.4f;
            m.Life = Random(.6f, 1.1f); m.Color0 = colour; m.Color1 = colour * .7f; m.In = .02f; m.Out = .3f; m.Soft = .5f;
            m.Fall = 9; m.Drag = 1; m.Stick = true; m.Floor = floor; m.Light = .85f;
        }
    }
    // Spikes of ice out of the ground round a point, leaning outward, one after another.
    void NyxaraFlair::Spikes(const V3& at, int count, float reach, float tall, const V3& colour, float life)
    {
        for (int i = 0; i < count; ++i)
        {
            const float a = TwoPi * (F(i) + Random()) / F((std::max)(count, 1)), r = i == 0 ? 0.f : reach * Random(.35f, 1.f);
            const V3 out = Round(a);
            V3 foot = at + out * r;
            foot.y = GroundAt(foot, at.y) - .05f;
            const float lean = Random(.1f, .5f), share = i == 0 ? 1.f : Random(.45f, .9f), thick = Random(.2f, .3f);
            Mote& m = Add(Cell::Shard, Face::Axis, foot);
            m.Base = true; m.LongV = true; m.AxisY = Norm(Up + out * lean);
            m.Length = tall * share; m.Size0 = m.Size1 = m.Length * thick;
            m.Sprout = .07f; m.T -= r * .06f;
            m.Life = life * Random(.75f, 1.f); m.Color0 = colour; m.Color1 = colour * .8f; m.In = .01f; m.Out = .25f; m.Worn1 = .25f; m.Soft = .5f; m.Light = .55f;
        }
    }
    // Feathers shed: small, turning, drifting down. Most are her wings' dark ones; the rest the colour given.
    void NyxaraFlair::Feathers(const V3& at, const V3& way, int count, float speed, const V3& colour, float light)
    {
        for (int i = 0; i < count; ++i)
        {
            const bool dark = Random() < .6f;
            Mote& m = Add(Cell::Feather, Face::Eye, at + RandomWay() * .2f);
            m.Vel = Cone(way, .9f) * (speed * Random(.4f, 1.f)); m.Size0 = m.Size1 = Random(.13f, .21f);
            m.Roll = Random(0.f, TwoPi); m.Spin = Random(-7.f, 7.f); m.Life = Random(.8f, 1.4f);
            m.Color0 = m.Color1 = dark ? V3{.04f, .03f, .025f} : colour; m.Alpha = .95f; m.In = .03f; m.Out = .35f; m.Soft = .5f;
            m.Fall = 3.2f; m.Drag = 2.4f; m.Wander = 3; m.Light = dark ? 0.f : light;
        }
    }
    // Tongues of flame standing up from where they are, rising, worn away from their edges.
    void NyxaraFlair::Flames(const V3& at, int count, float spread, float size, const V3& c0, const V3& c1, float life, float rise)
    {
        for (int i = 0; i < count; ++i)
        {
            const V3 foot = at + V3{Random(-1.f, 1.f) * spread, Random(-.1f, .1f) * spread, Random(-1.f, 1.f) * spread};
            const V3 axis = Norm(Up + RandomWay() * .25f);
            Mote& m = Add(Random() < .5f ? Cell::Flame : Cell::Flame2, Face::Axis, foot);
            m.Base = true; m.LongV = true; m.Flip = Random() < .5f; m.AxisY = axis; m.Vel = axis * (rise * Random(.15f, .4f)); m.Drag = 1.5f;
            m.Size0 = size * Random(.7f, 1.f); m.Size1 = m.Size0 * .7f; m.Length = size * Random(1.1f, 1.8f); m.Sprout = .09f;
            m.Life = life * Random(.55f, 1.f); m.Color0 = c0; m.Color1 = c1; m.In = .12f; m.Out = .55f;
            m.Worn0 = .05f; m.Worn1 = .8f; m.Soft = .35f; m.Flicker = .3f; m.Light = .6f;
        }
    }
    // Wisps: curls of wind or of pale light, drawn out along their way.
    void NyxaraFlair::Wisps(const V3& at, const V3& way, int count, float spread, float size, const V3& colour, float life, float speed)
    {
        for (int i = 0; i < count; ++i)
        {
            Mote& m = Add(Cell::Wisp, Face::Along, at + RandomWay() * (spread * Random()));
            m.Vel = Cone(way, .5f) * (speed * Random(.5f, 1.f)); m.AxisY = Norm(m.Vel); m.Flip = Random() < .5f;
            m.Size0 = size * Random(.3f, .45f); m.Size1 = m.Size0 * .6f; m.Length = size * Random(1.4f, 2.4f);
            m.Life = life * Random(.6f, 1.f); m.Color0 = colour; m.Color1 = colour * .5f; m.In = .15f; m.Out = .55f;
            m.Worn1 = .6f; m.Soft = .6f; m.Drag = 2; m.Wander = 2; m.Light = .6f;
        }
    }
    // Chips of stone, ash: small dark things thrown, turning, falling, off the ground.
    void NyxaraFlair::Chips(const V3& at, const V3& way, int count, float speed, const V3& colour, float size)
    {
        const float floor = GroundAt(at, mFloor);
        for (int i = 0; i < count; ++i)
        {
            Mote& m = Add(Cell::Debris, Face::Eye, at);
            m.Vel = Cone(Norm(way + Up * .6f), .9f) * (speed * Random(.4f, 1.f));
            m.Size0 = m.Size1 = size * Random(.5f, 1.3f); m.Roll = Random(0.f, TwoPi); m.Spin = Random(-12.f, 12.f);
            m.Life = Random(.9f, 1.5f); m.Color0 = colour; m.Color1 = colour; m.In = .01f; m.Out = .2f; m.Soft = .15f;
            m.Fall = 14; m.Drag = .5f; m.Bounce = true; m.Floor = floor; m.Light = 0;
        }
    }
    // The light a burst throws on the ground under it.
    void NyxaraFlair::GroundGlow(const V3& at, float size, const V3& colour, float life)
    {
        Mote& m = Add(Cell::Glow, Face::Ground, {at.x, GroundAt(at, at.y) + .05f, at.z});
        m.AxisX = {1, 0, 0}; m.AxisY = {0, 0, 1};
        m.Size0 = size * .7f; m.Size1 = size; m.Life = life; m.Color0 = colour; m.Color1 = colour * .3f; m.Alpha = .55f; m.In = .08f; m.Out = .8f;
    }
    // Blood from what she cuts: a blot of it thrown open at the wound for a moment, drops thrown the way the blade went,
    // falling, each a spot on the ground where it lands, and a fine mist of it. (In place of her kit's own, little boxes.)
    void NyxaraFlair::Bleed(const V3& at, const V3& way, int count)
    {
        const float floor = GroundAt(at, mFloor) + .02f;
        const V3 blood{.09f, .004f, .004f};
        for (int i = 0; i < count; ++i)
        {
            Mote& m = Add(Cell::Needle, Face::Along, at + RandomWay() * .08f);
            m.Vel = Cone(Norm(way + Up * .45f), .75f) * Random(1.5f, 6.f);
            m.Size0 = m.Size1 = Random(.012f, .032f); m.Long = 2.5f; m.LongBySpeed = .03f;
            m.Life = Random(3.f, 5.f); m.Color0 = blood; m.Color1 = blood * .7f; m.In = .005f; m.Out = .25f; m.Soft = .3f;
            m.Fall = 10; m.Drag = .4f; m.Stick = true; m.Splat = true; m.Floor = floor; m.Light = 0;
        }
        Mote& blot = Add(Cell::Splat, Face::Eye, at + Norm(mEye - at) * .25f + way * .15f);
        blot.Roll = Random(0.f, TwoPi); blot.Flip = Random() < .5f; blot.Size0 = .3f; blot.Size1 = .75f + .04f * F(count); blot.Life = .24f;
        blot.Color0 = blood * 1.3f; blot.Color1 = blood; blot.Alpha = .9f; blot.In = .1f; blot.Out = .55f; blot.Worn1 = .6f; blot.Soft = .3f; blot.Light = 0;
        Mote& mist = Add(Cell::Smoke2, Face::Eye, at);
        mist.Vel = way * .8f; mist.Size0 = .25f; mist.Size1 = .7f; mist.Roll = Random(0.f, TwoPi);
        mist.Life = .4f; mist.Color0 = blood * 1.2f; mist.Color1 = blood; mist.Alpha = .4f; mist.In = .1f; mist.Out = .7f;
        mist.Worn0 = .1f; mist.Worn1 = .7f; mist.Soft = .5f; mist.Light = 0; mist.Drag = 3;
    }
    // The cut itself, through what it bites: a wide soft glow along the blade's way, the stroke as a crescent turned to
    // the eye, and a white-hot line drawn across in it. (body, deep: its colours; core: the line's.)
    void NyxaraFlair::Gash(const Cut& cut, float length, const V3& body, const V3& deep, const V3& core, float life, float light)
    {
        const V3 seen = cut.At + Norm(mEye - cut.At) * .4f;
        Beam(seen, cut.Way, length * 1.3f, length * .36f, deep * 1.2f + body * .6f, .65f, life * 1.25f, .4f);
        Swipe(seen, cut.Way, cut.Edge, length, body * 2.4f, deep * 1.2f, .95f, life, light);
        Line(seen, cut.Way, length * 1.1f, .05f, core, body * 2.5f, life * .85f);
    }

    // ------------------------------------------------------------------ the picture as a whole, and the game's feel
    void NyxaraFlair::Punch(const V3& at, float blur, float split) { mPunchAt = at; mBlur = (std::max)(mBlur, blur * .65f); mSplit = (std::max)(mSplit, split * .7f); }
    void NyxaraFlair::Shock(const V3& at, float strength, float reach, float life)
    {
        Bent& b = mBent[0].Life <= 0 || (mBent[1].Life > 0 && mBent[0].T > mBent[1].T) ? mBent[0] : mBent[1];
        b.At = at; b.T = 0; b.Life = life; b.Strength = strength; b.Reach = reach;
    }
    void NyxaraFlair::Tint(const V3& colour, float amount) { mTint = colour; mTintAmount = (std::max)(mTintAmount, amount * .4f); }
    void NyxaraFlair::Later(float in, const V3& at, float blur, float split, float stop, float shake) { mPending.push_back({in, at, blur, split, stop, shake}); }
    void NyxaraFlair::Felt(const Blow& blow, const Cut& cut)
    {
        const Look& look = Now();
        if (cut.Hit)
        {
            if (!blow.Passing) mFeel.Stop = (std::max)(mFeel.Stop, look.Stop[cut.W]);
            mFeel.Shake = (std::max)(mFeel.Shake, look.Shake[cut.W]);
            if (cut.W == 2 && look.Slow > 0 && !blow.Passing) { mFeel.Slow = look.Slow; mFeel.SlowRate = look.SlowRate; }
        }
        else if (blow.Ring > 0) mFeel.Shake = (std::max)(mFeel.Shake, look.Shake[cut.W] * .8f);
    }

    // ------------------------------------------------------------------ a blow
    NyxaraFlair::Cut NyxaraFlair::CutOf(const Blow& blow) const
    {
        Cut c;
        c.W = std::clamp(blow.Weight, 0, 2);
        c.Forward = {std::sin(blow.Yaw), 0, std::cos(blow.Yaw)}; c.Side = {std::cos(blow.Yaw), 0, -std::sin(blow.Yaw)};
        const int hand = blow.Hand == 0 ? 0 : 1;
        const auto& trail = mTrail[hand];
        const V3 chest = blow.Her + Up * 1.2f;
        V3 tip = chest + c.Forward * (std::min)(blow.Reach * .6f, 1.4f), grip = chest + c.Forward * .4f;
        c.Way = hand == 0 ? c.Side : c.Side * -1.f;   // (till her blade says: her left blade from her left to her right)
        bool turned = false;   // (the turn of her blade's point is known: the crescent of the blow lies on it)
        if (!trail.empty() && trail.back().Age < .1f)
        {
            const TrailPoint& last = trail.back();
            tip = last.Tip; grip = last.Grip;
            // where its point was three and seven hundredths of a second back (in this stroke)
            const TrailPoint* p1 = nullptr; const TrailPoint* p2 = nullptr;
            if (!last.Fresh)
                for (std::size_t i = trail.size() - 1; i-- > 0;)
                {
                    const TrailPoint& q = trail[i];
                    const float back = q.Age - last.Age;
                    if (!p1) { if (back >= .03f || q.Fresh || i == 0) p1 = &q; }
                    else if (back >= .07f || q.Fresh || i == 0) { p2 = &q; break; }
                    if (q.Fresh) break;
                }
            if (p1 && Len(tip - p1->Tip) > .05f) c.Way = Norm(tip - p1->Tip);
            if (p1 && p2)
            {   // the circle through the three: its middle, and the plane it lies in
                const V3 a = p1->Tip - tip, b = p2->Tip - tip, n = Cross(a, b);
                const float n2 = Dot(n, n), la = Dot(a, a), lb = Dot(b, b);
                if (la > .0025f && lb > .0025f && n2 > .0064f * la * lb)
                {
                    const V3 middle = tip + (Cross(b, n) * la + Cross(n, a) * lb) * (.5f / n2);
                    const float radius = Len(tip - middle);
                    if (radius > .6f && radius < 3.5f)
                    {
                        c.Middle = middle; c.Radius = radius; c.Edge = (tip - middle) * (1 / radius);
                        V3 along = Cross(Norm(n), c.Edge);
                        if (Dot(along, c.Way) < 0) along = along * -1.f;
                        c.Way = along; c.Normal = Norm(Cross(c.Edge, c.Way));
                        turned = true;
                    }
                }
            }
        }
        if (!turned)
        {   // (a thrust, or nothing known of it: about her shoulder, along her blade)
            c.Edge = Norm(tip - grip, c.Forward);
            c.Way = Norm(c.Way - c.Edge * Dot(c.Way, c.Edge), c.Side);
            c.Normal = Norm(Cross(c.Edge, c.Way));
            c.Middle = grip - c.Edge * .45f;
            c.Radius = Len(tip - c.Middle);
        }
        const V3 before = blow.Her + c.Forward * .9f;
        c.Floor = {before.x, GroundAt(before, blow.Her.y) + .03f, before.z};
        c.At = tip;
        const Blow::Body* nearest = nullptr; float least = 1e9f;
        for (const Blow::Body& s : blow.Struck)
        {
            const float d = std::hypot(s.Feet.x - blow.Her.x, s.Feet.z - blow.Her.z) - s.Radius;
            if (d < least) { least = d; nearest = &s; }
        }
        if (nearest)
        {   // in the creature nearest her: on its side toward her, at the height of her blade's point (within its own)
            const V3 toHer = Norm({blow.Her.x - nearest->Feet.x, 0, blow.Her.z - nearest->Feet.z}, c.Forward * -1.f);
            const float y = std::clamp(tip.y, nearest->Feet.y + .25f * nearest->Height, nearest->Feet.y + .85f * nearest->Height);
            c.At = {nearest->Feet.x + toHer.x * nearest->Radius * .7f, y, nearest->Feet.z + toHer.z * nearest->Radius * .7f};
            c.Hit = true;
        }
        if (blow.Placed) c.At = blow.At;   // (a finisher's blow: her kit has brought it onto its creature)
        return c;
    }

    void NyxaraFlair::Strike(const Blow& blow)
    {
        if (!On()) return;
        const Cut cut = CutOf(blow);
        mFloor = cut.Floor.y - .03f;
        const std::size_t asked = mPending.size();
        if (!mPlain)   // (plain: no light of it; its blood and its feel, below)
            switch (Which())
            {
            case 0: StrikeGale(blow, cut); break;
            case 1: StrikeStone(blow, cut); break;
            case 2: StrikeWheel(blow, cut); break;
            case 3: StrikeCinder(blow, cut); break;
            case 4: StrikeForge(blow, cut); break;
            case 5: StrikeDance(blow, cut); break;
            case 6: StrikeThread(blow, cut); break;
            case 7: StrikeGlacier(blow, cut); break;
            default: StrikeWraith(blow, cut); break;
            }
        if (blow.Passing)   // (a cut in passing holds no moment, now or a moment after)
            for (std::size_t i = asked; i < mPending.size(); ++i) mPending[i].Stop = 0;
        if (cut.Hit) Bleed(cut.At, cut.Way, 5 + 4 * cut.W);
        if (blow.Struck.size() > 1)
        {   // the others struck with it: a smaller light on each
            const Look& look = Now();
            int more = 0;
            for (const Blow::Body& s : blow.Struck)
            {
                const V3 toHer = Norm({blow.Her.x - s.Feet.x, 0, blow.Her.z - s.Feet.z}, cut.Forward * -1.f);
                const V3 at{s.Feet.x + toHer.x * s.Radius * .7f, s.Feet.y + .55f * s.Height, s.Feet.z + toHer.z * s.Radius * .7f};
                if (Len(at - cut.At) < .6f || ++more > 3) continue;
                if (!mPlain)
                {
                    Flash(at, .7f, look.Main * 1.6f, .11f);
                    Glint(at, .5f, White * 5.f, .1f, 2.f);
                    Sparks(at, cut.Way, 5, 7.f, .5f, look.Core * 6.f, look.Main * 2.f, .022f, .32f, 8.f, false);
                }
                Bleed(at, cut.Way, 4);
            }
        }
        Felt(blow, cut);
    }

    // THE HIGHLANDS, GALE EDGE. Fast and sharp: nothing bursts; a thin crescent of air, the cut itself as a white line
    // through what it bites in a crescent of blue, a glint across it, warm sparks thrown along it, wind whipped past.
    // Its last blow: three more cuts of air through it, one after another, and a ring of wind thrown out.
    void NyxaraFlair::StrikeGale(const Blow& blow, const Cut& cut)
    {
        const V3 sky{.05f, .3f, 1.f}, pale{.5f, .78f, 1.f}, deep{.02f, .1f, .9f}, warm{1.f, .8f, .5f}, amber{1.f, .4f, .08f}, feather{.55f, .5f, .42f};
        const float w = F(cut.W);
        Slash(cut, Cell::SlashRazor, 1.06f, .6f, 1.3f, .5f, .04f, sky * 2.2f, deep * 1.1f, deep * .5f, pale * 5.f, .18f, 0, .1f);
        if (cut.Hit)
        {
            Gash(cut, 2.8f + .6f * w, sky, deep, White * 9.f, .17f, .45f);
            Glint(cut.At, 1.1f + .35f * w, White * 6.f, .12f, 2.4f);
            Flash(cut.At, .9f + .3f * w, sky * 1.8f, .1f, .7f, .6f);
            Flash(cut.At, .32f, White * 6.f, .06f);
            Sparks(cut.At, cut.Way, 16 + 9 * cut.W, 13.f, .32f, warm * 8.f, amber * 2.5f, .016f, .38f, 7.f, false);
            Wisps(cut.At, cut.Way, 4 + 2 * cut.W, .5f, .9f, sky * 2.2f, .32f, 8.f);
            if (Random() < .35f) Feathers(cut.At, cut.Way, 1, 2.2f, feather, 0);
            if (cut.W == 2)
            {
                const V3 toEye = Norm(mEye - cut.At), seen = cut.At + toEye * .45f;
                for (int k = 0; k < 3; ++k)
                {
                    const V3 way = Turn(cut.Way, toEye, Random(.5f, 1.3f) * (k % 2 ? -1.f : 1.f));
                    mWait = .045f * F(k + 1);
                    Swipe(seen + RandomWay() * .2f, way, cut.Edge, 2.6f, sky * 2.2f, deep * 1.1f, .9f, .16f, .65f);
                    Line(seen + RandomWay() * .15f, way, 3.2f, .04f, White * 9.f, sky * 2.5f, .16f);
                }
                mWait = 0;
                const V3 ax = cut.Way, ay = cut.Normal;
                ShockRing(cut.At, .3f, 1.9f, sky * 2.f, .6f, .28f, .6f, &ax, &ay);
                Wisps(cut.At, cut.Edge, 8, .6f, 1.1f, pale * 1.6f, .4f, 9.f);
                Feathers(cut.At, Up, 2, 2.6f, feather, 0);
                Punch(cut.At, .32f, .22f);
            }
        }
        if (blow.Whirl) Wheel(blow.Her + Up * 1.15f, cut, Cell::SlashRazor, 2.4f, .7f, pale * 2.4f, sky * 1.f, .8f, .3f);
        if (blow.Ring > 0)
        {   // (the ground struck: a ring of wind over it, dust, wind thrown out along it)
            ShockRing(cut.Floor, .4f, blow.Ring * .85f, {.62f, .64f, .66f}, .45f, .5f, 0);
            RingOut(cut.Floor, .4f, blow.Ring * .7f, pale * 1.8f, .35f);
            Puffs(cut.Floor + Up * .25f, 6, blow.Ring * .3f, 1.f, {.62f, .64f, .66f}, .35f, .8f, .5f, 0);
            for (int k = 0; k < 12; ++k) Wisps(cut.Floor + Up * .3f, Round(TwoPi * Random(), .15f), 1, .2f, .9f, pale * 1.6f, .35f, 8.f);
            Punch(cut.Floor, .25f, .15f);
        }
    }

    // THE HIGHLANDS, STONEBREAKER. Heavy: a broad golden crescent thrown forward; in a creature a burst of golden rays,
    // sparks that fall and bounce, chips of stone, dust; the ground struck is cracked and glows, a ring of dust goes
    // out over it, a dome of dust and stone goes up from it, the air is bent round it.
    void NyxaraFlair::StrikeStone(const Blow& blow, const Cut& cut)
    {
        const V3 gold{1.f, .42f, .04f}, amber{1.f, .2f, .015f}, hot{1.f, .78f, .38f}, dust{.5f, .44f, .36f}, stone{.3f, .28f, .26f};
        const float w = F(cut.W);
        Slash(cut, Cell::SlashHeavy, 1.45f + .2f * w, 1.1f, 1.5f, .7f, .07f, gold * 2.4f, amber * 1.1f, {1.f, .3f, .02f}, hot * 5.f, .34f, 1.5f, .2f, .75f);
        if (cut.Hit)
        {
            Gash(cut, 3.4f + .8f * w, gold, amber, hot * 9.f, .2f, .45f);
            Flash(cut.At, 1.5f + .5f * w, gold * 1.8f, .14f, .8f, .6f);
            Flash(cut.At, .5f, White * 6.f, .06f);
            Lines(cut.At, 2.2f + .9f * w, gold * 2.6f, .18f, 1.4f);
            Rays(cut.At, 8 + 5 * cut.W, 1.5f + .5f * w, gold * 3.4f, amber * 1.6f, .22f, 9.f, .032f);
            Sparks(cut.At, Norm(cut.Way + Up * .4f), 26 + 14 * cut.W, 12.f, .7f, hot * 8.f, amber * 2.5f, .02f, .6f, 12.f, true);
            Chips(cut.At, cut.Way, 6 + 4 * cut.W, 6.f, stone, .1f);
            Puffs(cut.At, 4 + 2 * cut.W, .4f, 1.1f, dust, .5f, 1.f, .5f, 0, cut.Way * 1.5f);
            Punch(cut.At, .25f + .18f * w, .2f + .12f * w);
            if (cut.W == 2) { Shock(cut.At, 1.f, 2.6f, .4f); Tint(gold * 1.5f, .1f); }
        }
        if (blow.Whirl) Wheel(blow.Her + Up * 1.15f, cut, Cell::SlashHeavy, 2.6f, 1.f, gold * 2.6f, amber * 1.1f, .85f, .36f);
        if (blow.Ring > 0)
        {
            const V3 g = cut.Floor; const float r = blow.Ring, roll = Random(0.f, TwoPi);
            Mark(g, Cell::Stain, r * 1.3f, {.16f, .14f, .12f}, {.16f, .14f, .12f}, .5f, 5.f, 0, .02f, .3f);
            Mark(g, Cell::Crack, r * 1.6f, {.02f, .02f, .02f}, {.02f, .02f, .02f}, .9f, 5.f, 0, .02f, .3f, roll);
            Mark(g, Cell::Crack, r * 1.6f, gold * 3.2f, amber * 1.3f, 1.f, 1.5f, 1, .02f, .7f, roll);
            GroundGlow(g, r * 2.4f, gold * 1.5f, .3f);
            ShockRing(g, .5f, r, dust, .6f, .6f, 0);
            RingOut(g, .5f, r * .8f, gold * 2.2f, .35f);
            Rays(g + Up * .1f, 12, 2.f, gold * 3.4f, amber * 1.6f, .28f, 7.f, .035f, &Up);
            Sparks(g + Up * .1f, Up, 36, 10.f, 1.1f, hot * 8.f, amber * 2.5f, .02f, .8f, 12.f, true);
            Chips(g + Up * .1f, Up, 16, 7.5f, stone, .13f);
            for (int k = 0; k < 10; ++k)
            {
                const float a = TwoPi * F(k) / 10.f;
                Puffs(g + Round(a, .7f) * (r * .35f), 1, .2f, 1.5f, dust, .5f, 1.4f, .7f, 0, Round(a) * 2.4f);
            }
            Flash(g + Up * .3f, r * 1.1f, gold * 1.8f, .16f, .8f, .6f);
            Flash(g + Up * .3f, .7f, White * 6.f, .07f);
            Shock(g, 1.2f, r * 1.2f, .45f); Punch(g, .5f, .3f); Tint(gold * 1.5f, .12f);
        }
    }

    // THE HIGHLANDS, SKY WHEEL. Flowing: a wide blue crescent that hangs, a golden ring opening in the blow's plane; in
    // a creature the cut in blue, small golden lights that turn as they rise, a feather or two. Nothing is held long:
    // its blows run on. Its last: three golden rings through each other, a cloud of lights.
    void NyxaraFlair::StrikeWheel(const Blow& blow, const Cut& cut)
    {
        const V3 sky{.05f, .3f, 1.f}, pale{.5f, .78f, 1.f}, deep{.02f, .1f, .9f}, gold = WheelGold(), hot = WheelHot(), feather{.55f, .5f, .42f};
        const float w = F(cut.W);
        const auto some = [this](int count) { return mOwn ? (count * 7 + 5) / 10 : count; };   // (as she set out: fewer of its lights, and no feathers)
        Slash(cut, Cell::SlashSilk, 1.3f, .95f, 2.f, .9f, .1f, sky * 2.f, deep * 1.f, deep * .5f, gold * 4.f, .5f, .4f, .18f);
        {
            const V3 ax = cut.Edge, ay = cut.Way;
            RingOut(cut.Middle + cut.Edge * (cut.Radius * .6f), .35f, 1.2f + .35f * w, gold * 2.4f, .42f, &ax, &ay);
        }
        if (cut.Hit)
        {
            Gash(cut, 2.8f + .6f * w, sky, deep, pale * 7.f, .2f, .45f);
            Flash(cut.At, 1.2f + .4f * w, sky * 1.7f, .16f, .7f, .6f);
            Glint(cut.At, .9f + .3f * w, White * 5.f, .16f, 1.8f);
            Embers(cut.At, some(16 + 8 * cut.W), .35f, 3.f, hot * 5.f, gold * 1.8f, .06f, 1.1f, .7f, 3.f);
            Sparks(cut.At, cut.Way, some(8 + 4 * cut.W), 9.f, .5f, hot * 7.f, gold * 2.f, .014f, .35f, 5.f, false);
            if (!mOwn) Feathers(cut.At, cut.Way, 1 + cut.W, 2.2f, feather, 0);
            if (cut.W == 2)
            {
                for (int k = 0; k < (mOwn ? 2 : 3); ++k)
                {
                    const V3 ax = Turn(cut.Way, cut.Edge, 1.05f * F(k)), ay = cut.Edge;
                    mWait = .05f * F(k);
                    RingOut(cut.At, .3f, 2.f, gold * 2.6f, .5f, &ax, &ay);
                }
                mWait = 0;
                Embers(cut.At, some(34), .5f, 4.f, hot * 5.f, gold * 1.8f, .065f, 1.4f, .9f, 4.f);
                Shock(cut.At, .6f, 2.2f, .4f); Punch(cut.At, .2f, .14f);
            }
        }
        if (blow.Whirl)
        {
            const V3 chest = blow.Her + Up * 1.15f, ax = cut.Forward, ay = cut.Side;
            Wheel(chest, cut, Cell::SlashSilk, 2.5f, 1.f, sky * 2.f, deep * .9f, .7f, .55f);
            RingOut(chest, .8f, 2.6f, gold * 2.4f, .5f, &ax, &ay);
        }
        if (blow.Ring > 0)
        {
            const V3 g = cut.Floor; const float r = blow.Ring;
            ShockRing(g, .5f, r * .85f, sky * 1.6f, .7f, .5f, .6f);
            mWait = .08f; RingOut(g, .3f, r * .6f, gold * 2.4f, .6f); mWait = 0;
            GroundGlow(g, r * 2.f, sky * 1.4f, .35f);
            for (int k = 0; k < some(20); ++k) Embers(g + Round(TwoPi * F(k) / F(some(20)), .3f) * (r * .4f), 1, .1f, 1.f, hot * 5.f, gold * 1.8f, .06f, 1.4f, 1.2f, 2.f);
            if (!mOwn) Feathers(g + Up * .4f, Up, 4, 3.f, feather, 0);
            Punch(g, .22f, .14f);
        }
    }

    // AMIRANI'S FIRE, CINDER RAZOR. Fast and sharp: the cut is a white-hot line through what it bites in a crescent of
    // red, a glint across it, sparks that fall and bounce off the ground; a heavier blow is crossed by a second line.
    // Its last blow: a red flare, a long streak of light across the picture, a shower of sparks.
    void NyxaraFlair::StrikeCinder(const Blow& blow, const Cut& cut)
    {
        const V3 hot{1.f, .78f, .38f}, orange{1.f, .14f, .01f}, red{1.f, .05f, .01f};
        const float w = F(cut.W);
        Slash(cut, Cell::SlashRazor, 1.1f, .5f, 1.3f, .5f, .045f, orange * 2.6f, red * 1.3f, {1.2f, .07f, .008f}, V3{1.f, .9f, .7f} * 5.f, .2f, 0, .08f, .8f);
        if (cut.Hit)
        {
            const V3 toEye = Norm(mEye - cut.At), seen = cut.At + toEye * .45f;
            Gash(cut, 3.f + .8f * w, orange, red, V3{1.f, .9f, .7f} * 10.f, .2f, .45f);
            if (cut.W >= 1)
            {
                const V3 way = Turn(cut.Way, toEye, 1.15f);
                mWait = .045f;
                Swipe(seen, way, cut.Edge, 2.2f, orange * 2.2f, red * 1.1f, .9f, .16f, .7f);
                Line(seen, way, 2.6f, .04f, hot * 9.f, red * 3.f, .18f);
                mWait = 0;
            }
            Glint(cut.At, 1.1f + .45f * w, hot * 6.f, .12f, 2.6f);
            Flash(cut.At, 1.f + .35f * w, orange * 1.9f, .1f, .75f, .6f);
            Flash(cut.At, .32f, V3{1.f, .9f, .7f} * 6.f, .06f);
            Sparks(cut.At, cut.Way, 18 + 10 * cut.W, 13.f, .3f, hot * 9.f, orange * 3.f, .016f, .55f, 12.f, true);
            Embers(cut.At, 4 + 3 * cut.W, .2f, 1.5f, orange * 5.f, red * 1.8f, .045f, .8f, .8f, 0);
            if (cut.W == 2)
            {
                Flash(cut.At, 2.6f, red * 1.9f, .2f, .8f, .5f);
                Glint(cut.At, 2.6f, hot * 6.f, .2f, 3.6f);
                Sparks(cut.At, Norm(cut.Way + Up * .3f), 40, 14.f, .8f, hot * 9.f, orange * 3.f, .018f, .75f, 12.f, true);
                Embers(cut.At, 16, .4f, 3.f, orange * 5.f, red * 1.8f, .05f, 1.1f, .8f, 1.5f);
                Puffs(cut.At, 3, .25f, .9f, {.05f, .02f, .02f}, .5f, .8f, .6f, 0, cut.Way);
                Punch(cut.At, .35f, .3f); Tint(red * 1.2f, .08f);
            }
        }
        if (blow.Whirl) Wheel(blow.Her + Up * 1.15f, cut, Cell::SlashRazor, 2.4f, .6f, hot * 3.2f, red * 1.4f, .85f, .3f);
        if (blow.Ring > 0)
        {   // (the ground struck: scored red-hot, sparking)
            const float roll = Random(0.f, TwoPi);
            Mark(cut.Floor, Cell::Crack, blow.Ring * 1.3f, {.02f, .01f, .01f}, {.02f, .01f, .01f}, .85f, 5.f, 0, .02f, .3f, roll);
            Mark(cut.Floor, Cell::Crack, blow.Ring * 1.3f, orange * 3.2f, red * 1.4f, 1.f, 1.6f, 1, .02f, .7f, roll);
            Sparks(cut.Floor + Up * .1f, Up, 44, 11.f, 1.2f, hot * 9.f, orange * 3.f, .018f, .85f, 12.f, true);
            ShockRing(cut.Floor, .4f, blow.Ring * .8f, orange * 2.2f, .8f, .4f, 1);
            GroundGlow(cut.Floor, blow.Ring * 2.f, orange * 1.5f, .25f);
            Punch(cut.Floor, .3f, .2f);
        }
    }

    // AMIRANI'S FIRE, FORGE HAMMER. Heavy: a thick crescent of fire with smoke in its wake; in a creature a burst of
    // flame, spikes of light, embers thrown up in a fountain and falling, black smoke; the ground struck is scorched and
    // cracked, the cracks glowing as they cool, a ring of fire goes out over it, flames stand up round it, smoke rises.
    void NyxaraFlair::StrikeForge(const Blow& blow, const Cut& cut)
    {
        const V3 hot{1.f, .78f, .38f}, fire{1.f, .25f, .02f}, orange{1.f, .16f, .012f}, red{.8f, .04f, .008f}, soot{.05f, .025f, .02f};
        const float w = F(cut.W);
        Crescent(cut, Cell::SlashSilk, 1.55f + .2f * w, 1.35f, 1.5f, .7f, .08f, soot, soot * .7f, .6f, .5f, .9f, .25f, 0);
        Slash(cut, Cell::SlashHeavy, 1.45f + .2f * w, 1.15f, 1.5f, .7f, .07f, fire * 2.8f, red * 1.2f, {1.1f, .08f, .008f}, V3{1.f, .9f, .7f} * 5.f, .36f, 1.2f, .2f, .8f);
        if (cut.Hit)
        {
            Gash(cut, 3.4f + .8f * w, fire, red, V3{1.f, .9f, .7f} * 9.f, .22f, .45f);
            Flash(cut.At, 2.2f + .8f * w, orange * 2.f, .17f, .85f, .6f);
            Flash(cut.At, .65f, V3{1.f, .9f, .7f} * 6.f, .07f);
            Flames(cut.At, 6 + 4 * cut.W, .35f, 1.f + .3f * w, fire * 2.4f, red * 1.3f, .48f, 2.4f);
            Rays(cut.At, 7 + 4 * cut.W, 1.4f + .4f * w, fire * 3.4f, red * 1.8f, .2f, 9.f, .034f);
            Embers(cut.At, 22 + 14 * cut.W, .35f, 5.5f, hot * 6.f, red * 1.8f, .055f, 1.2f, -7.f, 0);
            Sparks(cut.At, Norm(cut.Way + Up * .5f), 16 + 10 * cut.W, 10.f, .8f, hot * 9.f, orange * 3.f, .02f, .65f, 12.f, true);
            Puffs(cut.At + Up * .2f, 4 + 3 * cut.W, .4f, 1.2f, soot, .65f, 1.2f, .9f, 0, cut.Way * 1.2f);
            Punch(cut.At, .3f + .18f * w, .25f + .12f * w);
            if (cut.W == 2) { Shock(cut.At, 1.1f, 2.8f, .4f); Tint(orange * 1.5f, .12f); }
        }
        if (blow.Whirl) Wheel(blow.Her + Up * 1.15f, cut, Cell::SlashHeavy, 2.6f, 1.f, fire * 3.2f, red * 1.1f, .9f, .38f);
        if (blow.Ring > 0)
        {
            const V3 g = cut.Floor; const float r = blow.Ring, roll = Random(0.f, TwoPi);
            Mark(g, Cell::Stain, r * 1.5f, {.02f, .015f, .01f}, {.02f, .015f, .01f}, .8f, 6.f, 0, .02f, .3f);
            Mark(g, Cell::Crack, r * 1.6f, {.01f, .01f, .01f}, {.01f, .01f, .01f}, .9f, 6.f, 0, .02f, .3f, roll);
            Mark(g, Cell::Crack, r * 1.6f, fire * 3.4f, red * 1.4f, 1.f, 2.4f, 1, .02f, .75f, roll);
            GroundGlow(g, r * 2.6f, orange * 1.7f, .35f);
            ShockRing(g, .5f, r * .95f, fire * 2.6f, .9f, .45f, 1);
            mWait = .05f; ShockRing(g, .4f, r * 1.1f, soot, .55f, .8f, 0); mWait = 0;
            for (int k = 0; k < 12; ++k)
            {
                mWait = .012f * F(k);
                Flames(g + Round(TwoPi * (F(k) + Random()) / 12.f) * (r * Random(.2f, .5f)), 1, .05f, 1.4f, fire * 2.4f, red * 1.3f, .65f, 3.f);
            }
            mWait = 0;
            Flames(g, 5, .25f, 1.8f, fire * 2.6f, red * 1.3f, .6f, 3.5f);
            Embers(g + Up * .2f, 50, .5f, 7.5f, hot * 6.f, red * 1.8f, .06f, 1.5f, -7.f, 0);
            Sparks(g + Up * .1f, Up, 26, 10.f, 1.1f, hot * 9.f, orange * 3.f, .02f, .85f, 12.f, true);
            Puffs(g + Up * .4f, 10, r * .35f, 1.6f, soot, .6f, 1.7f, 1.4f, 0);
            Flash(g + Up * .4f, r * 1.3f, orange * 2.f, .18f, .85f, .6f);
            Flash(g + Up * .3f, .8f, V3{1.f, .9f, .7f} * 6.f, .07f);
            Shock(g, 1.3f, r * 1.2f, .45f); Punch(g, .55f, .35f); Tint(orange * 1.5f, .14f);
        }
    }

    // AMIRANI'S FIRE, FIRE DANCE. Flowing: a sheet of flame that hangs in the blow's plane, a ring of fire opening in
    // it; in a creature the cut in flame, tongues of it, embers that turn as they rise. Its last blow: embers going up
    // round her in a spiral. A turn: flames standing all round her.
    void NyxaraFlair::StrikeDance(const Blow& blow, const Cut& cut)
    {
        const V3 hot{1.f, .78f, .38f}, fire{1.f, .2f, .015f}, orange{1.f, .14f, .01f}, red{.9f, .05f, .01f};
        const float w = F(cut.W);
        Slash(cut, Cell::SlashSilk, 1.3f, 1.f, 2.f, .9f, .1f, fire * 2.4f, red * 1.1f, {1.1f, .07f, .008f}, hot * 4.5f, .55f, .5f, .2f, .75f);
        {
            const V3 ax = cut.Edge, ay = cut.Way;
            ShockRing(cut.Middle + cut.Edge * (cut.Radius * .6f), .3f, 1.2f + .4f * w, orange * 2.6f, .9f, .42f, 1, &ax, &ay);
        }
        if (cut.Hit)
        {
            Gash(cut, 2.8f + .6f * w, fire, red, hot * 8.f, .2f, .45f);
            Flash(cut.At, 1.3f + .45f * w, orange * 1.7f, .18f, .75f, .6f);
            Flames(cut.At, 4 + 3 * cut.W, .25f, .85f, fire * 2.4f, red * 1.2f, .5f, 2.5f);
            Embers(cut.At, 18 + 10 * cut.W, .4f, 3.f, hot * 5.5f, red * 1.7f, .055f, 1.2f, 1.2f, 4.f);
            if (cut.W == 2)
            {
                const V3 feet = blow.Her + Up * .2f;
                for (int k = 0; k < 48; ++k)
                {
                    const float a = TwoPi * F(k) / 16.f;
                    mWait = .01f * F(k);
                    Embers(feet + V3{std::cos(a) * 1.3f, 2.4f * F(k) / 48.f, std::sin(a) * 1.3f}, 1, .05f, .6f, hot * 6.f, red * 1.7f, .07f, 1.1f, 1.4f, 5.f);
                }
                mWait = 0;
                Flames(cut.At, 8, .45f, 1.2f, fire * 2.6f, red * 1.2f, .6f, 3.f);
                Shock(cut.At, .5f, 2.f, .4f); Punch(cut.At, .2f, .14f);
            }
        }
        if (blow.Whirl)
        {
            const V3 feet{blow.Her.x, cut.Floor.y, blow.Her.z};
            Wheel(blow.Her + Up * 1.15f, cut, Cell::SlashSilk, 2.5f, 1.f, fire * 2.4f, red * 1.f, .75f, .55f);
            for (int k = 0; k < 12; ++k)
            {
                mWait = .015f * F(k);
                Flames(feet + Round(TwoPi * (F(k) + Random()) / 12.f) * 1.9f, 1, .05f, 1.1f, fire * 2.4f, red * 1.2f, .55f, 2.5f);
            }
            mWait = 0;
        }
        if (blow.Ring > 0)
        {
            const V3 g = cut.Floor; const float r = blow.Ring;
            Mark(g, Cell::Stain, r * 1.3f, {.02f, .015f, .01f}, {.02f, .015f, .01f}, .7f, 5.f, 0, .02f, .3f);
            ShockRing(g, .4f, r * .85f, fire * 2.6f, .9f, .5f, 1);
            mWait = .08f; RingOut(g, .3f, r * .5f, hot * 2.4f, .6f); mWait = 0;
            GroundGlow(g, r * 2.2f, orange * 1.5f, .35f);
            for (int k = 0; k < 14; ++k)
            {
                mWait = .01f * F(k);
                Flames(g + Round(TwoPi * F(k) / 14.f) * (r * .42f), 1, .05f, 1.2f, fire * 2.4f, red * 1.2f, .6f, 2.5f);
            }
            mWait = 0;
            Embers(g + Up * .2f, 36, r * .3f, 3.f, hot * 5.5f, red * 1.7f, .06f, 1.5f, 1.4f, 4.f);
            Punch(g, .25f, .15f);
        }
    }

    // MOONLIT, SILVER THREAD. Fast and sharp, and the thinnest: a hairline of silver through what it bites in a narrow
    // crescent of violet, a star on it and small stars about it, needles of ice. Its last blow: a web of hairlines
    // through the creature, drawn one after another and held; then all of them break at once.
    void NyxaraFlair::StrikeThread(const Blow& blow, const Cut& cut)
    {
        const V3 silver{.7f, .82f, 1.f}, violet{.3f, .1f, 1.f}, lilac{.42f, .2f, 1.f}, ice{.45f, .72f, 1.f}, dusk{.1f, .03f, .5f};
        const float w = F(cut.W);
        Slash(cut, Cell::SlashRazor, 1.12f, .36f, 1.3f, .5f, .04f, violet * 2.2f, dusk * 1.8f, dusk, silver * 6.f, .38f, 0, .05f);
        if (cut.Hit)
        {
            const V3 toEye = Norm(mEye - cut.At), seen = cut.At + toEye * .45f;
            Beam(seen, cut.Way, 4.f + 1.f * w, 1.f, violet * 1.6f, .65f, .3f, .4f);
            Swipe(seen, cut.Way, cut.Edge, 2.4f + .5f * w, lilac * 2.2f, violet * 1.1f, .85f, .16f, .75f);
            Line(seen, cut.Way, 3.6f + .9f * w, .035f, White * 10.f, lilac * 3.f, .26f);
            Glint(cut.At, 1.f + .4f * w, silver * 7.f, .14f, 2.8f);
            for (int k = 0; k < 4 + cut.W; ++k)
            {
                mWait = Random(.02f, .18f);
                Glint(cut.At + RandomWay() * Random(.2f, .7f), Random(.24f, .4f), silver * 6.f, Random(.12f, .2f), 1.4f, Random(0.f, Pi));
            }
            mWait = 0;
            Crystals(cut.At, cut.Way, 8 + 5 * cut.W, 10.f, .4f, ice * 2.6f, .06f);
            Flash(cut.At, .8f + .25f * w, violet * 1.9f, .09f, .7f, .6f);
            Flash(cut.At, .28f, White * 6.f, .06f);
            if (cut.W == 2)
            {
                const float burst = .42f;
                for (int k = 0; k < 9; ++k)
                {
                    const float wait = .035f * F(k);
                    const V3 way = Turn(cut.Way, toEye, Random(0.f, Pi)), at = seen + RandomWay() * .3f;
                    Line(at, way, Random(3.f, 4.2f), .032f, White * 10.f, lilac * 3.5f, burst - wait + .06f, wait, .035f);
                    Beam(at, way, 3.6f, .6f, violet * 1.5f, .5f, burst - wait + .06f, .5f, wait);
                }
                mWait = burst;
                Flash(cut.At, 1.9f, violet * 1.8f, .16f, .7f, .5f);
                Flash(cut.At, .45f, White * 6.f, .07f);
                Glint(cut.At, 2.1f, silver * 6.f, .2f, 3.2f);
                Lines(cut.At, 3.2f, lilac * 3.2f, .22f, 1.5f);
                Crystals(cut.At, Up, 30, 10.f, 1.4f, ice * 2.6f, .07f);
                mWait = 0;
                Later(burst, cut.At, .4f, .3f, .09f, .25f);
            }
        }
        if (blow.Whirl) Wheel(blow.Her + Up * 1.15f, cut, Cell::SlashRazor, 2.4f, .45f, silver * 3.2f, violet * 1.4f, .85f, .45f);
        if (blow.Ring > 0)
        {   // (the ground struck: a star of silver cracks, needles thrown up)
            Mark(cut.Floor, Cell::Crack, blow.Ring * 1.3f, lilac * 3.f, violet * 1.4f, 1.f, 1.4f, 1, .02f, .7f);
            RingOut(cut.Floor, .4f, blow.Ring * .7f, lilac * 2.4f, .35f);
            Crystals(cut.Floor + Up * .2f, Up, 24, 9.f, 1.2f, ice * 2.6f, .07f);
            Glint(cut.Floor + Up * .3f, 1.8f, White * 7.f, .16f, 2.8f);
            GroundGlow(cut.Floor, blow.Ring * 2.f, violet * 1.5f, .25f);
            Punch(cut.Floor, .28f, .2f);
        }
    }

    // MOONLIT, GLACIER. Heavy: a great pale crescent thrown forward, frost in its wake; in a creature the cut in ice, a
    // long level glint with a star at its middle, a shell of frost round it for a moment, crystals and snow thrown off
    // it and falling after; the ground struck sends up spikes of ice, one after another outward, and is left frosted.
    void NyxaraFlair::StrikeGlacier(const Blow& blow, const Cut& cut)
    {
        const V3 ice{.55f, .82f, 1.f}, blue{.1f, .4f, 1.f}, frost{.14f, .5f, 1.f}, snow{.82f, .88f, .96f}, depth{.03f, .12f, .6f};
        const float w = F(cut.W);
        Slash(cut, Cell::SlashHeavy, 1.65f + .25f * w, 1.3f, 1.6f, .8f, .07f, frost * 2.4f, blue * 1.1f, depth, White * 5.f, .42f, 2.f, .25f);
        if (cut.Hit)
        {
            Gash(cut, 3.6f + .8f * w, frost, blue, White * 9.f, .22f, .45f);
            Flash(cut.At, 2.f + .7f * w, frost * 1.7f, .16f, .8f, .6f);
            Flash(cut.At, .6f, White * 6.f, .07f);
            Glint(cut.At, 1.8f + .6f * w, White * 6.f, .17f, 3.4f);
            Crystals(cut.At, Norm(cut.Way + Up * .3f), 14 + 10 * cut.W, 9.f, .7f, ice * 2.5f, .11f);
            Puffs(cut.At, 3 + 2 * cut.W, .45f, .9f, snow, .4f, .8f, .3f, .3f, cut.Way * 1.5f);
            Embers(cut.At, 24 + 12 * cut.W, .5f, 3.f, ice * 2.2f, blue * 1.f, .035f, 1.3f, -1.6f, 1.f);
            {   // (a shell of frost round it, for a moment)
                Mote& m = Add(Cell::Shock, Face::Eye, cut.At + Norm(mEye - cut.At) * .3f);
                m.Roll = Random(0.f, TwoPi); m.Size0 = 1.2f; m.Size1 = 2.3f + .5f * w; m.Life = .4f; m.Color0 = frost * 1.8f; m.Color1 = blue * .8f; m.Alpha = .55f;
                m.In = .08f; m.Out = .7f; m.Worn0 = .1f; m.Worn1 = .7f; m.Soft = .5f; m.Light = .6f;
            }
            Punch(cut.At, .3f + .18f * w, .2f + .12f * w);
            if (cut.W == 2) { Shock(cut.At, 1.f, 2.8f, .4f); Tint(frost * 1.4f, .12f); }
        }
        if (blow.Whirl) Wheel(blow.Her + Up * 1.15f, cut, Cell::SlashHeavy, 2.7f, 1.1f, ice * 2.6f, blue * 1.1f, .9f, .4f);
        if (blow.Ring > 0)
        {
            const V3 g = cut.Floor; const float r = blow.Ring;
            Mark(g, Cell::Stain, r * 1.6f, snow, snow * .8f, .5f, 5.f, .3f, .03f, .35f);
            Mark(g, Cell::Crack, r * 1.6f, frost * 2.6f, blue * 1.f, 1.f, 1.8f, 1, .02f, .7f);
            Spikes(g, 9, r * .5f, 2.4f, frost * 1.7f, 1.f);
            GroundGlow(g, r * 2.4f, blue * 1.5f, .35f);
            ShockRing(g, .5f, r, snow, .6f, .6f, .3f);
            RingOut(g, .5f, r * .8f, ice * 2.4f, .35f);
            Crystals(g + Up * .2f, Up, 28, 9.f, 1.2f, ice * 2.5f, .11f);
            for (int k = 0; k < 10; ++k)
            {
                const float a = TwoPi * F(k) / 10.f;
                if (k % 3 != 2) Puffs(g + Round(a, .7f) * (r * .35f), 1, .2f, 1.15f, snow, .38f, 1.1f, .5f, .3f, Round(a) * 2.4f);
            }
            Embers(g + Up * .5f, 50, r * .4f, 4.f, ice * 2.2f, blue * 1.f, .035f, 1.6f, -1.6f, 1.f);
            Flash(g + Up * .4f, r * 1.2f, frost * 1.8f, .17f, .8f, .6f);
            Flash(g + Up * .3f, .7f, White * 6.f, .07f);
            Shock(g, 1.2f, r * 1.2f, .45f); Punch(g, .5f, .3f); Tint(frost * 1.4f, .14f);
        }
    }

    // MOONLIT, MOON WRAITH. Flowing, and of two: every cut is made again a moment after by a paler one, wider, as if a
    // second of her struck behind the first. A soft teal crescent over dark smoke, and its pale echo; in a creature
    // the cut in teal, dark smoke, pale wisps curling round it, ash flying off it, a dark stain left under it; then the
    // echo's cut through it.
    void NyxaraFlair::StrikeWraith(const Blow& blow, const Cut& cut)
    {
        const V3 teal{.03f, .85f, .6f}, ghost{.55f, 1.f, .9f}, violet{.3f, .1f, 1.f}, dark{.012f, .05f, .06f}, ash{.02f, .025f, .03f};
        const float w = F(cut.W), echo = .16f;
        Slash(cut, Cell::SlashSilk, 1.35f, 1.f, 2.f, .9f, .1f, teal * 2.f, violet * 1.f, dark, ghost * 4.f, .6f, .3f, .2f);
        mWait = echo;
        Crescent(cut, Cell::SlashRazor, 1.6f, .8f, 2.f, .9f, .08f, ghost * 2.f, teal * .8f, .7f, .45f, .6f, .25f, .8f);
        mWait = 0;
        if (cut.Hit)
        {
            const V3 toEye = Norm(mEye - cut.At), seen = cut.At + toEye * .45f;
            Gash(cut, 2.8f + .6f * w, teal, violet, ghost * 8.f, .22f, .45f);
            Flash(cut.At, 1.3f + .45f * w, teal * 1.7f, .2f, .75f, .6f);
            Puffs(cut.At, 5 + 3 * cut.W, .4f, 1.1f, dark, .7f, 1.f, .5f, 0, cut.Way * 1.2f);
            for (int k = 0; k < 3 + 2 * cut.W; ++k) Wisps(cut.At + Up * Random(0.f, .5f), RandomWay(), 1, .45f, .9f, ghost * 1.3f, .75f, 1.3f);
            Chips(cut.At, cut.Way, 8 + 4 * cut.W, 4.5f, ash, .07f);
            Embers(cut.At, 10 + 6 * cut.W, .3f, 1.5f, teal * 4.f, violet * 1.6f, .05f, 1.1f, .5f, 2.f);
            Mark({cut.At.x, cut.Floor.y, cut.At.z}, Cell::Splat, 1.3f + .4f * w, {.01f, .03f, .035f}, {.01f, .03f, .035f}, .75f, 6.f, 0, .1f, .3f);
            mWait = echo;
            Swipe(seen + cut.Way * .15f, cut.Way, cut.Edge, 3.4f + .7f * w, ghost * 2.f, teal * .7f, .7f, .2f, .9f, 1.3f);
            Line(seen, cut.Way, 3.2f + .6f * w, .045f, ghost * 8.f, teal * 2.f, .2f);
            Flash(cut.At, .9f, ghost * 1.5f, .14f, .6f);
            mWait = 0;
            Later(echo, cut.At, .12f + .06f * w, .1f, .02f, .05f);
            if (cut.W == 2)
            {
                Puffs(cut.At, 10, .6f, 1.4f, dark, .7f, 1.3f, .7f, 0);
                for (int k = 0; k < 7; ++k) Wisps(cut.At + RandomWay() * .4f, RandomWay(), 1, .5f, 1.1f, ghost * 1.4f, 1.f, 1.6f);
                Embers(cut.At, 26, .5f, 2.5f, teal * 4.f, violet * 1.6f, .055f, 1.4f, .6f, 3.f);
                Shock(cut.At, .7f, 2.4f, .45f); Punch(cut.At, .25f, .2f); Tint(teal * 1.f, .08f);
            }
        }
        if (blow.Whirl)
        {
            const V3 chest = blow.Her + Up * 1.15f;
            Wheel(chest, cut, Cell::SlashSilk, 2.5f, 1.f, teal * 2.f, violet * .9f, .7f, .6f);
            mWait = echo; Wheel(chest, cut, Cell::SlashRazor, 2.9f, .7f, ghost * 2.2f, teal * .8f, .6f, .45f); mWait = 0;
        }
        if (blow.Ring > 0)
        {
            const V3 g = cut.Floor; const float r = blow.Ring;
            Mark(g, Cell::Stain, r * 1.5f, {.01f, .03f, .035f}, {.01f, .03f, .035f}, .75f, 6.f, 0, .05f, .3f);
            ShockRing(g, .5f, r * .9f, dark, .7f, .6f, 0);
            RingOut(g, .5f, r * .75f, teal * 2.4f, .5f);
            mWait = echo; RingOut(g, .5f, r * .9f, ghost * 1.8f, .5f); mWait = 0;
            GroundGlow(g, r * 2.2f, teal * 1.4f, .4f);
            for (int k = 0; k < 10; ++k)
            {
                const float a = TwoPi * F(k) / 10.f;
                Puffs(g + Round(a, .6f) * (r * .35f), 1, .2f, 1.4f, dark, .7f, 1.3f, .6f, 0, Round(a) * 1.8f);
                Wisps(g + Round(a, .4f) * (r * .3f), Up, 1, .1f, 1.f, ghost * 1.3f, .9f, 1.8f);
            }
            Embers(g + Up * .3f, 30, r * .35f, 2.f, teal * 4.f, violet * 1.6f, .055f, 1.5f, .8f, 3.f);
            Shock(g, .8f, r * 1.2f, .5f); Punch(g, .3f, .2f);
        }
    }

    // ------------------------------------------------------------------ her finishing kills
    // The kill of a finisher: the style's last word, made round the creature as it dies and sized to it. (The blow
    // itself, the cut through it, is a Strike like any other: this is what follows it.) Each gear's third, the game's:
    //   SKY WHEEL   the wheel is round the creature: a blue one, and golden rings going up it one after another; lights
    //               that turn as they rise, wind thrown out round it, feathers let fall
    //   FIRE DANCE  the dance closes round it: a ring of fire out over the ground, flames standing up round it one
    //               after another, embers going up round it in a spiral, a sheet of flame turned round it
    //   MOON WRAITH the blow is made again, paler, a moment after; dark smoke out of it, pale wisps going up round it
    //               one after another, a ring of teal over the ground and its echo
    // The other six (the Character Lab's): a burst of their own colours.
    void NyxaraFlair::Finish(const Kill& kill)
    {
        if (!On()) return;
        const Look& look = Now();
        const Blow::Body& it = kill.It;
        const float H = (std::max)(it.Height, .6f), R = (std::max)(it.Radius, .25f);
        const float s = std::clamp(.5f * H, .8f, 2.2f), round = R + .9f * s;   // its size, for what is made round it; how far out its rings go
        const V3 way = Norm({kill.Way.x, 0, kill.Way.z}, {0, 0, 1}), side{way.z, 0, -way.x};
        const V3 feet{it.Feet.x, GroundAt(it.Feet, it.Feet.y) + .03f, it.Feet.z}, mid = it.Feet + Up * (.5f * H), top = it.Feet + Up * (.9f * H);
        const V3 across = Len(kill.Cut) > .5f ? Norm(kill.Cut) : side;         // the way her blade went through it
        const V3 X{1, 0, 0}, Z{0, 0, 1};
        Cut level;   // (a wheel round it lies level)
        level.Forward = way; level.Side = side;
        mFloor = feet.y - .03f;
        if (!kill.Ground) Bleed(kill.At, across, 18);
        if (mPlain) return;   // (its blood, and no light)
        switch (Which())
        {
        case 2:
        {
            const V3 sky{.05f, .3f, 1.f}, pale{.5f, .78f, 1.f}, deep{.02f, .1f, .9f}, gold = WheelGold(), hot = WheelHot(), feather{.55f, .5f, .42f};
            Wheel(mid, level, Cell::SlashSilk, round + .3f, .9f * s, sky * 2.f, deep * .9f, .7f, .7f);
            for (int k = 0; k < (mOwn ? 2 : 3); ++k)
            {   // (begun outside its body: inside it a ring was a disc on its chest for a moment)
                mWait = .09f * F(k);
                RingOut(it.Feet + Up * (H * (.2f + .3f * F(k))), R + .15f, round + .25f * F(k), gold * 2.6f, .6f, &X, &Z);
            }
            mWait = 0;
            Embers(mid, mOwn ? 30 : 44, R + .3f, 3.5f, hot * 5.f, gold * 1.8f, .065f, 1.7f, 1.1f, 4.f);
            for (int k = 0; k < 10; ++k)
            {
                const float a = TwoPi * (F(k) + Random()) / 10.f;
                Wisps(mid + Round(a) * (R + .2f) + Up * Random(-.3f * H, .3f * H), Round(a + 1.2f, .1f), 1, .2f, 1.1f, pale * 1.5f, .5f, 7.f);
            }
            if (!mOwn) Feathers(top, Up, 6, 3.f, feather, 0);
            Glint(kill.At, 1.5f + .4f * s, White * 5.f, .2f, 2.6f);
            Flash(kill.At, 1.4f + .6f * s, sky * 1.7f, .2f, .7f, .6f);
            RingOut(feet, R * .6f, round + .8f, gold * 2.2f, .7f);
            GroundGlow(feet, (round + .8f) * 2.f, sky * 1.4f, .45f);
            Shock(mid, .8f, 1.6f + s, .45f); Punch(kill.At, .26f, .16f); Tint(sky * 1.2f, .06f);
            break;
        }
        case 5:
        {
            const V3 hot{1.f, .78f, .38f}, fire{1.f, .2f, .015f}, orange{1.f, .14f, .01f}, red{.9f, .05f, .01f}, soot{.02f, .015f, .01f};
            Mark(feet, Cell::Stain, (round + .4f) * 1.5f, soot, soot, .7f, 6.f, 0, .02f, .3f);
            ShockRing(feet, R * .6f, round + .9f, fire * 2.6f, .9f, .55f, 1);
            mWait = .1f; RingOut(feet, .3f, round + .2f, hot * 2.4f, .6f); mWait = 0;
            GroundGlow(feet, (round + .9f) * 2.2f, orange * 1.5f, .45f);
            Wheel(mid, level, Cell::SlashSilk, round + .2f, 1.f * s, fire * 2.4f, red * 1.f, .75f, .6f);
            const int stand = 12 + static_cast<int>(4 * s);
            for (int k = 0; k < stand; ++k)
            {
                mWait = .016f * F(k);
                Flames(feet + Round(TwoPi * F(k) / F(stand)) * (R + .45f), 1, .05f, .9f + .35f * s, fire * 2.4f, red * 1.2f, .65f, 2.5f);
            }
            for (int k = 0; k < 48; ++k)
            {
                const float a = TwoPi * F(k) / 16.f;
                mWait = .012f * F(k);
                Embers(it.Feet + V3{std::cos(a) * (R + .6f), 1.1f * H * F(k) / 48.f, std::sin(a) * (R + .6f)}, 1, .05f, .6f, hot * 6.f, red * 1.7f, .07f, 1.2f, 1.4f, 5.f);
            }
            mWait = 0;
            Flames(kill.At, 8, .45f, 1.1f + .2f * s, fire * 2.6f, red * 1.2f, .6f, 3.f);
            Flash(kill.At, 1.6f + .7f * s, orange * 1.7f, .2f, .75f, .6f);
            Flash(kill.At, .5f, V3{1.f, .9f, .7f} * 6.f, .07f);
            Shock(mid, .7f, 1.6f + s, .45f); Punch(kill.At, .3f, .2f); Tint(orange * 1.4f, .1f);
            break;
        }
        case 8:
        {
            const V3 teal{.03f, .85f, .6f}, ghost{.55f, 1.f, .9f}, violet{.3f, .1f, 1.f}, dark{.012f, .05f, .06f}, ash{.02f, .025f, .03f}, stain{.01f, .03f, .035f};
            const float echo = .16f;
            const V3 seen = kill.At + Norm(mEye - kill.At) * .45f;
            Mark(feet, Cell::Stain, (round + .5f) * 1.5f, stain, stain, .75f, 6.f, 0, .05f, .3f);
            ShockRing(feet, .5f, round + .9f, dark, .7f, .6f, 0);
            RingOut(feet, .5f, round + .5f, teal * 2.4f, .55f);
            mWait = echo; RingOut(feet, .5f, round + 1.f, ghost * 1.8f, .55f); mWait = 0;
            GroundGlow(feet, (round + .9f) * 2.2f, teal * 1.4f, .45f);
            Wheel(mid, level, Cell::SlashSilk, round + .2f, 1.f * s, teal * 2.f, violet * .9f, .7f, .6f);
            mWait = echo; Wheel(mid, level, Cell::SlashRazor, round + .6f, .7f * s, ghost * 2.2f, teal * .8f, .6f, .45f); mWait = 0;
            // (its smoke thin and little: a cloud of it hid the creature as it fell)
            Puffs(top, 3 + static_cast<int>(s), R * .6f, .55f + .1f * s, dark, .45f, 1.2f, 1.f, 0);
            const int rise = 14 + static_cast<int>(4 * s);
            for (int k = 0; k < rise; ++k)
            {
                const float a = TwoPi * 1.7f * F(k) / F(rise);
                mWait = .03f * F(k);
                Wisps(it.Feet + Round(a) * (R + .25f) + Up * (H * Random(.05f, .7f)), Norm(Up + Round(a + 1.5f) * .5f), 1, .1f, 1.3f, ghost * 1.6f, 1.1f, 2.4f);
            }
            mWait = 0;
            Embers(mid, 36, R + .3f, 2.5f, teal * 4.f, violet * 1.6f, .055f, 1.7f, 1.f, 3.f);
            Chips(kill.At, way, 14, 5.f, ash, .07f);
            Flash(kill.At, 1.5f + .6f * s, teal * 1.7f, .22f, .75f, .6f);
            // (the blow's own echo is the blow's: Strike. The kill's is its rings' and its wheel's, and a thin pale line through it)
            mWait = echo;
            Line(seen, across, 3.4f + .8f * s, .04f, ghost * 6.f, teal * 2.f, .22f);
            mWait = 0;
            Shock(mid, .8f, 1.7f + s, .5f); Punch(kill.At, .3f, .22f); Tint(teal * 1.f, .08f);
            break;
        }
        default:
            Flash(kill.At, 1.6f + .6f * s, look.Main * 1.7f, .2f, .75f, .6f);
            Flash(kill.At, .5f, White * 6.f, .07f);
            Glint(kill.At, 1.6f + .4f * s, look.Core * 6.f, .2f, 3.f);
            Lines(kill.At, 2.6f + .8f * s, look.Main * 2.6f, .2f, 1.5f);
            Rays(kill.At, 12, 1.4f + .5f * s, look.Main * 3.4f, look.Deep * 1.6f, .24f, 9.f, .034f);
            Embers(mid, 30, R + .3f, 3.5f, look.Core * 5.f, look.Deep * 1.7f, .055f, 1.4f, .9f, 2.f);
            RingOut(feet, R * .6f, round + .8f, look.Main * 2.2f, .6f);
            GroundGlow(feet, (round + .8f) * 2.f, look.Main * 1.4f, .4f);
            Shock(mid, 1.f, 1.8f + s, .45f); Punch(kill.At, .4f, .28f); Tint(look.Main * 1.3f, .1f);
            break;
        }
    }

    // Her wings beat her up off the ground in a finisher, or she comes down on it: dust thrown out over it, and a thin
    // ring of the style's light. (In place of her kit's own ring there.)
    void NyxaraFlair::Gust(const V3& at, float radius)
    {
        if (!Lit()) return;
        const Look& look = Now();
        const V3 g{at.x, GroundAt(at, at.y) + .03f, at.z};
        mFloor = g.y - .03f;
        const V3 dust{.55f, .52f, .48f};
        ShockRing(g, .3f, radius, dust, .4f, .5f, 0);
        RingOut(g, .3f, radius * .8f, look.Main * 1.3f, .35f);
        for (int k = 0; k < 6; ++k)
        {
            const float a = TwoPi * (F(k) + Random()) / 6.f;
            Puffs(g + Round(a) * (radius * .3f) + Up * .12f, 1, .08f, .45f, dust, .3f, .7f, .25f, 0, Round(a) * 1.6f);
        }
    }

    // What her blades shed as they go, each of the nine its own.
    void NyxaraFlair::Shed(int hand, const V3& from, const V3& to, float shown)
    {
        const Look& look = Now();
        if (look.ShedEvery <= 0) return;
        mShedOwed[hand] += Len(to - from) * shown;
        const V3 way = Norm(to - from);
        for (int guard = 0; mShedOwed[hand] >= look.ShedEvery && guard < 24; ++guard)
        {
            mShedOwed[hand] -= look.ShedEvery;
            const V3 at = Mix(from, to, Random());
            switch (Which())
            {
            case 0: Wisps(at, way, 1, .1f, .35f, look.Core * 1.1f, .22f, 4.f); break;
            case 1: Sparks(at, Norm(way + V3{0, -.4f, 0}), 1, 3.f, .5f, look.Core * 6.f, look.Main * 2.5f, .02f, .45f, 9.f, true); break;
            case 2:
                Embers(at, 1, .05f, .5f, WheelHot() * 4.f, WheelEmber() * 1.5f, .04f, 1.1f, .3f, 1.5f);
                if (!mOwn && Random() < .06f) Feathers(at, way, 1, 1.5f, {.55f, .5f, .42f}, 0);
                break;
            case 3: Sparks(at, Norm(way + V3{0, -.6f, 0}), 1, 2.5f, .4f, look.Core * 8.f, look.Main * 3.f, .018f, .5f, 10.f, true); break;
            case 4:
                Embers(at, 1, .06f, 1.f, look.Core * 6.f, look.Deep * 2.f, .045f, .9f, .6f, 0);
                if (Random() < .3f) Puffs(at, 1, .05f, .45f, look.Dark, .35f, .7f, .5f, 0);
                break;
            case 5:
                Flames(at, 1, .03f, .45f, look.Main * 2.4f, look.Deep * 1.2f, .4f, 1.6f);
                if (Random() < .5f) Embers(at, 1, .05f, .8f, look.Core * 5.f, look.Deep * 1.8f, .04f, 1.f, .9f, 3.f);
                break;
            case 6:
                mWait = Random(0.f, .12f);
                Glint(at, Random(.12f, .22f), look.Core * 6.f, Random(.12f, .2f), 1.3f, Random(0.f, Pi));
                mWait = 0;
                break;
            case 7: Embers(at, 1, .08f, .6f, look.Core * 1.8f, look.Main * .8f, .028f, 1.2f, -1.4f, .8f); break;
            default:
                if (Random() < .5f) Wisps(at, RandomWay(), 1, .05f, .35f, look.Core * 1.6f, .6f, .8f);
                else Embers(at, 1, .05f, .4f, look.Main * 3.f, look.Deep * 1.3f, .035f, 1.f, .3f, 1.5f);
                break;
            }
        }
        if (mShedOwed[hand] > look.ShedEvery) mShedOwed[hand] = 0;
    }

    // A move carrying her fast: lines of speed left along her way, a flash and a ring of air where she set off, and
    // each of the nine its own wake.
    void NyxaraFlair::Dash(const V3& from, const V3& to, bool first)
    {
        if (!Lit()) return;
        const Look& look = Now();
        if (first) { mDashFirst = true; mDashOwed = 0; }
        const V3 d = to - from;
        const float len = Len(d);
        if (len < .03f) return;
        const V3 way = d * (1 / len), side = Norm(Cross(way, Up), {1, 0, 0}), over = Cross(side, way);
        const int which = Which();
        const float feetY = GroundAt(from, from.y - 1.f) + .08f;
        mFloor = feetY - .08f;
        if (mDashFirst)
        {
            mDashFirst = false;
            Flash(from, 1.2f, look.Main * 1.6f, .12f);
            RingOut(from, .3f, 1.4f, look.Core * 1.3f, .25f, &side, &over);
            if (which == 8) Puffs(from, 6, .5f, 1.1f, look.Dark, .6f, 1.f, .4f, 0, way * -1.5f);
            if (which == 1 || which == 4) Puffs({from.x, feetY + .2f, from.z}, 4, .4f, .9f, look.Dark, .4f, .9f, .4f, 0, way * -2.f);
        }
        mDashOwed += len;
        for (int guard = 0; mDashOwed >= .22f && guard < 40; ++guard)
        {
            mDashOwed -= .22f;
            const V3 q = Mix(from, to, Random());
            Line(q + side * Random(-.45f, .45f) + Up * Random(-.7f, .6f), way, Random(1.1f, 2.2f), Random(.02f, .045f), look.Core * 4.f, look.Main * 1.3f, Random(.14f, .26f), 0, 0);
            const V3 feet{q.x, feetY, q.z};
            switch (which)
            {
            case 0: Wisps(q + Up * Random(-.5f, .5f), way, 1, .4f, .6f, look.Core * 1.2f, .3f, 6.f); break;
            case 1: if (Random() < .5f) Puffs(feet + Up * .15f, 1, .2f, .7f, look.Dark, .35f, .8f, .3f, 0, way * -1.f); break;
            case 2:
                Embers(q, 2, .5f, .6f, WheelHot() * 4.f, WheelEmber() * 1.5f, .045f, 1.1f, .3f, 2.f);
                if (!mOwn && Random() < .2f) Feathers(q, way * -1.f, 1, 1.5f, {.55f, .5f, .42f}, 0);
                break;
            case 3: Sparks(feet, Norm(way * -1.f + Up * .7f), 2, 4.f, .6f, look.Core * 8.f, look.Main * 3.f, .02f, .5f, 10.f, true); break;
            case 4:
            case 5: Flames(feet, 1, .2f, .7f, look.Main * 2.4f, look.Deep * 1.2f, .5f, 1.8f); break;
            case 6:
                mWait = Random(0.f, .1f);
                Glint(q + RandomWay() * .5f, Random(.15f, .28f), look.Core * 6.f, Random(.12f, .2f), 1.3f, Random(0.f, Pi));
                mWait = 0;
                break;
            case 7:
                Puffs(feet + Up * .2f, 1, .25f, .8f, look.Dark, .35f, .9f, .2f, .3f);
                Embers(q, 2, .5f, .8f, look.Core * 1.8f, look.Main * .8f, .028f, 1.2f, -1.4f, .8f);
                break;
            default:
                Puffs(q, 1, .35f, .9f, look.Dark, .6f, .8f, .2f, 0);
                Flash(q, .9f, look.Main * 1.f, .25f, .5f);
                break;
            }
        }
    }

    // A turn with her blades out: lines of light going round her at their height.
    void NyxaraFlair::Whirl(const V3& middle, float dt)
    {
        if (!Lit() || dt <= 0) return;
        const Look& look = Now();
        mWhirlOwed += dt * 45;
        for (int guard = 0; mWhirlOwed >= 1 && guard < 12; ++guard)
        {
            mWhirlOwed -= 1;
            const float a = Random(0.f, TwoPi), r = Random(1.5f, 2.1f);
            const V3 at = middle + V3{std::cos(a) * r, Random(-.3f, .4f), std::sin(a) * r};
            Line(at, {-std::sin(a), 0, std::cos(a)}, Random(.9f, 1.6f), Random(.02f, .04f), look.Core * 4.f, look.Main * 1.3f, Random(.12f, .2f), 0, 0);
        }
    }

    // A wave along the ground: a point of its line, and what comes up there.
    void NyxaraFlair::Wave(const V3& at, const V3& way)
    {
        if (!Lit()) return;
        const Look& look = Now();
        const V3 g{at.x, GroundAt(at, at.y) + .03f, at.z};
        const V3 along = Norm(way, {0, 0, 1});
        mFloor = g.y - .03f;
        Line(g + Up * .06f, along, 1.3f, .07f, look.Core * 6.f, look.Main * 1.8f, .22f, 0, .02f);
        GroundGlow(g, 1.3f, look.Main * 1.3f, .25f);
        switch (Which())
        {
        case 0: Wisps(g + Up * .3f, Norm(along + Up * .5f), 2, .2f, .6f, look.Core * 1.3f, .3f, 6.f); Sparks(g, Up, 3, 5.f, .8f, V3{1.f, .8f, .5f} * 7.f, V3{1.f, .4f, .08f} * 2.f, .02f, .3f, 6.f, false); break;
        case 1:
            Chips(g, Up, 3, 6.f, {.3f, .28f, .26f}, .1f); Puffs(g + Up * .2f, 1, .15f, .9f, look.Dark, .4f, .9f, .6f, 0);
            Rays(g, 3, 1.1f, look.Main * 3.4f, look.Deep * 1.6f, .22f, 5.f, .035f, &Up);
            Mark(g, Cell::Crack, 1.1f, look.Main * 3.2f, look.Deep * 1.3f, 1.f, 1.2f, 1, .02f, .7f);
            break;
        case 2: Embers(g + Up * .1f, 3, .2f, 1.f, WheelHot() * 4.f, WheelEmber() * 1.5f, .05f, 1.2f, 1.2f, 2.f); RingOut(g, .1f, .7f, look.Main * 1.8f, .35f); break;
        case 3: Sparks(g, Up, 5, 7.f, .9f, look.Core * 8.f, look.Main * 3.f, .022f, .6f, 12.f, true); Mark(g, Cell::Crack, .9f, look.Main * 3.2f, look.Deep * 1.4f, 1.f, 1.2f, 1, .02f, .7f); break;
        case 4:
            Flames(g, 2, .12f, 1.3f, look.Main * 2.4f, look.Deep * 1.3f, .6f, 3.f);
            Mark(g, Cell::Stain, 1.3f, {.02f, .015f, .01f}, {.02f, .015f, .01f}, .7f, 5.f, 0, .02f, .3f);
            Embers(g + Up * .2f, 5, .2f, 4.f, look.Core * 6.f, look.Deep * 1.8f, .045f, 1.1f, -7.f, 0);
            if (Random() < .6f) Puffs(g + Up * .5f, 1, .15f, 1.f, look.Dark, .5f, 1.2f, 1.2f, 0);
            break;
        case 5: Flames(g, 2, .15f, 1.f, look.Main * 2.4f, look.Deep * 1.2f, .55f, 2.5f); Embers(g + Up * .2f, 3, .2f, 1.5f, look.Core * 5.f, look.Deep * 1.6f, .045f, 1.2f, 1.2f, 4.f); break;
        case 6: Crystals(g + Up * .1f, Up, 3, 6.f, .9f, V3{.45f, .72f, 1.f} * 2.5f, .05f); Glint(g + Up * .25f, .5f, look.Core * 6.f, .15f, 1.8f); break;
        case 7:
            Spikes(g, 2, .3f, 1.3f, V3{.14f, .5f, 1.f} * 1.7f, .8f);
            Puffs(g + Up * .2f, 1, .2f, .9f, look.Dark, .4f, .9f, .4f, .3f);
            Mark(g, Cell::Stain, 1.2f, look.Dark, look.Dark * .8f, .45f, 5.f, .3f, .03f, .35f);
            break;
        default: Puffs(g + Up * .3f, 1, .2f, .9f, look.Dark, .6f, 1.f, .5f, 0); Wisps(g + Up * .2f, Up, 2, .2f, .6f, look.Core * 2.f, .8f, 1.6f); break;
        }
    }

    // ------------------------------------------------------------------ every frame
    void NyxaraFlair::Update(float dt)
    {
        if (dt <= 0) return;
        for (auto it = mPending.begin(); it != mPending.end();)
        {
            it->In -= dt;
            if (it->In > 0) { ++it; continue; }
            Punch(it->At, it->Blur, it->Split);
            mFeel.Stop = (std::max)(mFeel.Stop, it->Stop); mFeel.Shake = (std::max)(mFeel.Shake, it->Shake);
            it = mPending.erase(it);
        }
        for (Mote& m : mMotes)
        {
            m.T += dt;
            if (m.T < 0 || m.Stuck) continue;
            m.Vel.y -= m.Fall * dt;
            if (m.Wander > 0)
            {
                m.Vel.x += std::sin(m.T * 5.3f + m.Seed) * m.Wander * dt;
                m.Vel.z += std::cos(m.T * 4.1f + m.Seed * 1.7f) * m.Wander * dt;
            }
            if (m.Swirl != 0)
            {
                const float a = m.Swirl * dt, c = std::cos(a), s = std::sin(a), x = m.Vel.x * c - m.Vel.z * s, z = m.Vel.x * s + m.Vel.z * c;
                m.Vel.x = x; m.Vel.z = z;
            }
            if (m.Drag > 0) m.Vel = m.Vel * std::exp(-m.Drag * dt);
            m.Pos = m.Pos + m.Vel * dt;
            m.Roll += m.Spin * dt;
            const float speed = Len(m.Vel);
            if (m.Facing == Face::Along && speed > .2f) m.AxisY = m.Vel * (1 / speed);
            if ((m.Bounce || m.Stick) && m.Pos.y < m.Floor && m.Vel.y < 0)
            {
                m.Pos.y = m.Floor;
                if (m.Splat)
                {   // (blood: a spot on the ground where it fell)
                    m.Shape = Cell::Splat; m.Facing = Face::Ground; m.AxisX = {1, 0, 0}; m.AxisY = {0, 0, 1}; m.Flip = Random() < .5f;
                    m.Size0 = m.Size1 = m.Size0 * Random(3.5f, 8.f); m.Long = 1; m.LongBySpeed = 0; m.Length = 0; m.Roll = Random(0.f, TwoPi);
                    m.Stuck = true; m.Vel = {}; m.Spin = 0;
                }
                else if (m.Stick) { m.Stuck = true; m.Vel = {}; m.Spin = 0; }
                else { m.Vel = {m.Vel.x * .6f, -m.Vel.y * .38f, m.Vel.z * .6f}; m.Spin *= .5f; }
            }
        }
        std::erase_if(mMotes, [](const Mote& m) { return m.T >= m.Life; });
        for (Arc& a : mArcs)
        {
            a.T += dt;
            if (a.T > 0) { a.Middle = a.Middle + a.Drift * dt; a.Drift = a.Drift * std::exp(-3.5f * dt); }
        }
        std::erase_if(mArcs, [](const Arc& a) { return a.T >= a.Life; });
        mBlur *= std::exp(-15.f * dt); mSplit *= std::exp(-11.f * dt); mTintAmount *= std::exp(-16.f * dt);
        for (Bent& b : mBent)
            if (b.Life > 0) { b.T += dt; if (b.T >= b.Life) b = {}; }
    }

    // ------------------------------------------------------------------ drawing
    void NyxaraFlair::Quad(const V3& a, const V3& b, const V3& c, const V3& d, Cell shape, float u0, float u1, const V3& colourA, const V3& colourB,
                           float alphaA, float alphaB, float wornA, float wornB, float soft, float light, float depth)
    {
        if (mVertices.size() + 4 > MaxQuads * 4) return;
        const int cell = static_cast<int>(shape);
        const float wide = 1.f / F(AtlasCols), high = 1.f / F(AtlasRows), cu = F(cell % AtlasCols) * wide, cv = F(cell / AtlasCols) * high;
        const float ua = cu + std::clamp(u0, .004f, .996f) * wide, ub = cu + std::clamp(u1, .004f, .996f) * wide, va = cv + .004f * high, vb = cv + .996f * high;
        const std::uint32_t first = static_cast<std::uint32_t>(mVertices.size());
        const auto put = [&](const V3& p, float u, float v, const V3& colour, float alpha, float worn)
        {
            DoorwaysGpuDMeshVertex x{};
            x.Position = p; x.Normal = {worn, soft, light}; x.TexCoord = {u, v}; x.Tangent = {colour.x, colour.y, colour.z, alpha};
            mVertices.push_back(x);
        };
        put(a, ua, va, colourA, alphaA, wornA); put(b, ub, va, colourB, alphaB, wornB);
        put(c, ub, vb, colourB, alphaB, wornB); put(d, ua, vb, colourA, alphaA, wornA);
        if (light >= .999f) mLights.push_back(first);
        else mCovering.push_back({depth, first});
    }

    void NyxaraFlair::DrawMotes(const V3& eye)
    {
        for (const Mote& m : mMotes)
        {
            if (m.T < 0 || m.T >= m.Life) continue;
            const float t = m.T / m.Life;
            const float size = Mix(m.Size0, m.Size1, 1 - (1 - t) * (1 - t));
            float alpha = m.Alpha * Smooth(0, (std::max)(m.In, 1e-3f), t) * (1 - Smooth(1 - m.Out, 1.f, t));
            if (m.Flicker > 0) alpha *= 1 - m.Flicker * (.5f + .5f * std::sin(m.T * 47.f + m.Seed * 13.f));
            if (alpha <= .003f || size <= 1e-4f) continue;
            const V3 colour = Mix(m.Color0, m.Color1, t), toEye = eye - m.Pos;
            const float worn = Mix(m.Worn0, m.Worn1, t), whole = m.Length > 0 ? m.Length : size * m.Long;
            V3 a, b, c, d;
            switch (m.Facing)
            {
            case Face::Eye:
            {
                const V3 look = Norm(toEye), right = Norm(Cross(Up, look), {1, 0, 0}), over = Cross(look, right);
                const float cs = std::cos(m.Roll), sn = std::sin(m.Roll);
                const V3 x = (right * cs + over * sn) * (.5f * size * m.Long), y = (over * cs - right * sn) * (.5f * size);
                a = m.Pos - x + y; b = m.Pos + x + y; c = m.Pos + x - y; d = m.Pos - x - y;
                break;
            }
            case Face::Along:
            {
                const float speed = Len(m.Vel);
                const V3 dir = speed > .2f ? m.Vel * (1 / speed) : m.AxisY;
                const V3 side = Norm(Cross(dir, toEye), {1, 0, 0}) * (.5f * size), head = m.Pos, tail = m.Pos - dir * (whole + speed * m.LongBySpeed);
                if (m.LongV) { a = head + side; b = head - side; c = tail - side; d = tail + side; }
                else { a = tail + side; b = head + side; c = head - side; d = tail - side; }
                break;
            }
            case Face::Axis:
            {
                const float grown = m.Sprout > 0 ? Smooth(0, m.Sprout, m.T) : 1.f;
                const V3 side = Norm(Cross(m.AxisY, toEye), {1, 0, 0}) * (.5f * size);
                const V3 foot = m.Base ? m.Pos : m.Pos - m.AxisY * (.5f * whole), top = foot + m.AxisY * (whole * grown);
                if (m.LongV) { a = top - side; b = top + side; c = foot + side; d = foot - side; }
                else { a = foot + side; b = top + side; c = top - side; d = foot - side; }
                break;
            }
            default:   // on the ground, or held as it was made
            {
                const float cs = std::cos(m.Roll), sn = std::sin(m.Roll);
                const V3 x = (m.AxisX * cs + m.AxisY * sn) * (.5f * size * m.Long), y = (m.AxisY * cs - m.AxisX * sn) * (.5f * size);
                a = m.Pos - x + y; b = m.Pos + x + y; c = m.Pos + x - y; d = m.Pos - x - y;
                break;
            }
            }
            Quad(a, b, c, d, m.Shape, m.Flip ? 1.f : 0.f, m.Flip ? 0.f : 1.f, colour, colour, alpha, alpha, worn, worn, m.Soft, m.Light, Dot(toEye, toEye));
        }
    }

    bool NyxaraFlair::Seen(const V3& at, float& u, float& v) const
    {
        if (!mSeenOnce) return false;
        const XMVECTOR clip = XMVector4Transform(XMVectorSet(at.x, at.y, at.z, 1), XMLoadFloat4x4(&mViewProjection));
        const float w = XMVectorGetW(clip);
        if (w < .1f) return false;
        u = XMVectorGetX(clip) / w * .5f + .5f; v = .5f - XMVectorGetY(clip) / w * .5f;
        return true;
    }

    void NyxaraFlair::Draw(Renderer& renderer, const V3& eye, const XMMATRIX& viewProjection)
    {
        XMStoreFloat4x4(&mViewProjection, viewProjection); mEye = eye; mSeenOnce = true;
        mQuadsDrawn = 0;
        if (!On())
        {
            if (mScreenSet) { renderer.SetDoorwaysScreenFlair(DoorwaysScreenFlair{}); mScreenSet = false; }
            return;
        }
        const Look& look = Now();
        {   // the picture as a whole: its glow, a blow's punch, the air bent, a flash
            DoorwaysScreenFlair f;
            f.GlowStrength = look.Glow; f.GlowThreshold = GlowFrom; f.GlowKnee = GlowKnee;
            float u = .5f, v = .5f;
            if ((mBlur > .004f || mSplit > .004f) && Seen(mPunchAt, u, v)) { f.Blur = mBlur; f.Split = mSplit; f.BlurX = u; f.BlurY = v; }
            for (int i = 0; i < 2; ++i)
            {
                const Bent& b = mBent[i];
                float u2 = 0, v2 = 0;
                if (b.Life <= 0 || b.T >= b.Life || !Seen(b.At, u, v)) continue;
                const float k = b.T / b.Life;
                if (!Seen(b.At + Up * (b.Reach * (1 - (1 - k) * (1 - k))), u2, v2)) continue;
                f.Wave[i][0] = u; f.Wave[i][1] = v; f.Wave[i][2] = std::abs(v2 - v); f.Wave[i][3] = b.Strength * (1 - k) * (1 - k);
            }
            f.Flash[0] = mTint.x; f.Flash[1] = mTint.y; f.Flash[2] = mTint.z; f.Flash[3] = mTintAmount > .004f ? mTintAmount : 0.f;
            renderer.SetDoorwaysScreenFlair(f);
            mScreenSet = true;
        }
        mVertices.clear(); mCovering.clear(); mLights.clear(); mIndices.clear();
        if (!mPlain) { DrawRibbons(eye); DrawArcs(eye); }   // (plain: her blood alone)
        DrawMotes(eye);
        if (mVertices.empty()) return;
        // What covers (smoke, stone) first, the far before the near; then all that is light, which adds in any order.
        std::sort(mCovering.begin(), mCovering.end(), [](const Piece& x, const Piece& y) { return x.Depth > y.Depth; });
        const auto six = [&](std::uint32_t first) { for (const std::uint32_t k : {0u, 1u, 2u, 0u, 2u, 3u}) mIndices.push_back(first + k); };
        for (const Piece& p : mCovering) six(p.First);
        for (const std::uint32_t first : mLights) six(first);
        if (!renderer.UpdateDoorwaysDynamicGpuMesh(mMesh, mVertices.data(), static_cast<unsigned int>(mVertices.size()), mIndices.data(), static_cast<unsigned int>(mIndices.size()))) return;
        Material material{};
        material.Diffuse = {1, 1, 1, 1}; material.DiffuseMap = mAtlas.Get(); material.NormalMap = mNoise.Get(); material.TexTransform = {1, 1, 0, 0};
        material.SpecularColor = {look.NoiseSize, look.NoiseX, look.NoiseY, look.NoiseWear};
        material.DoubleSided = true; material.AlphaMode = DoorwaysMaterialAlphaMode::Blend; material.Mode = DoorwaysMaterialMode::Light; material.ReceiveFog = false;
        DoorwaysDMeshSubmesh submesh{};
        submesh.StartIndex = 0; submesh.IndexCount = static_cast<u32>(mIndices.size());
        renderer.BindDoorwaysSkinningDisabled();
        renderer.SetPremultipliedBlendingEnabled(true);
        renderer.SetDepthWriteEnabled(false);
        renderer.DrawDoorwaysGpuSubmesh(mMesh, submesh, XMMatrixIdentity(), viewProjection, material, false, true);
        renderer.ResetDefaultWorldRenderState();
        renderer.BindDoorwaysSkinningDisabled();
        mQuadsDrawn = static_cast<int>(mVertices.size() / 4);
    }
}
