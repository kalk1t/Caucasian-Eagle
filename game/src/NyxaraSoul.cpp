#include "NyxaraSoul.h"

#include "DoorwaysAnimatedCharacterInstance.h"
#include "DoorwaysRuntimePathResolver.h"
#include "Renderer.h"
#include "WorldRenderer.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace Doorways
{
    namespace
    {
        using V3 = XMFLOAT3;
        constexpr float Pi = 3.14159265f, TwoPi = 6.2831853f;
        constexpr std::size_t MaxQuads = 6000, MaxMotes = 1500, MaxGhosts = 14;
        constexpr int AtlasCols = 4, AtlasRows = 6;   // the flair atlas's cells (art/nyxara/flair/make_flair_textures.py)
        // The picture's glow, as her flair has it: what is brighter than this bleeds out round it.
        constexpr float GlowFrom = 1.35f, GlowKnee = .6f;
        // Her soul comes to be over FormSeconds as she leaves it, and is gone JoinSeconds after she is in it again.
        constexpr float FormSeconds = .32f, JoinSeconds = .34f;
        constexpr V3 Up{0, 1, 0};

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
        // a run of light along a string: 0 between two runs, 1 in the middle of one
        inline float Bead(float along, float sharp) { return std::pow(.5f + .5f * std::sin(TwoPi * along), sharp); }
    }

    // Its look. Its colours are light before the picture's tone curve, kept far from white (one strong channel, the others
    // low: a pale colour made brighter is white); what is thin (a thread, a spark) is brighter.
    struct NyxaraSoul::Look
    {
        V3 Rim, Inside;                 // her soul's body: the light of its edge, and of its inside
        float Cover;                    // how much of what stands behind it its inside covers (0: light alone)
        float RimPower, Bright;         // how thin that edge is; how bright all of it
        float NoiseSize, NoiseRise, NoiseShow, Bands;   // the noise in it (1/m), rising (m/s), how much of it shows; bands of light running up it
        float Worn, Soft;               // how much of it is worn away as it stands, and how soft that edge is
        V3 String, Hot, Other, Spark;   // its strings, their bright runs, a second colour of them, what it gives off
        V3 Mark;                        // the mark it stands on
        float Glow;                     // the picture's glow while it is out (where her flair sets none)
        float Wear[4];                  // its strings and motes: the noise that wears them, its size (1/m), its speed (x, z), how much
        float GhostEvery, GhostSeconds, BackEvery, BackSeconds;   // her afterimages on her way out (one every so many m, each
                                                                  // seen so many s), and on her way back
    };

    const NyxaraSoul::Look& NyxaraSoul::TheLook()
    {
        // her soul in pale blue glass, its threads, its ring; her afterimages one every 1.2 m out, 1 m back, each seen .2 s
        static const Look look{{.20f, .52f, 1.f}, {.012f, .045f, .15f}, 0.f, 2.2f, 2.5f, 1.7f, .10f, .45f, .55f, 0.f, .22f,
                               {.14f, .46f, 1.f}, {.5f, .8f, 1.f}, {.3f, .3f, 1.f}, {.3f, .65f, 1.f}, {.10f, .40f, 1.f}, .85f, {2.2f, .05f, .3f, .5f},
                               1.2f, .2f, 1.f, .2f};
        return look;
    }

    float NyxaraSoul::GhostEvery(bool back) { return back ? TheLook().BackEvery : TheLook().GhostEvery; }
    float NyxaraSoul::GhostSeconds(bool back) { return back ? TheLook().BackSeconds : TheLook().GhostSeconds; }

    bool NyxaraSoul::Load(Renderer& renderer, std::string& error)
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

    float NyxaraSoul::Random()
    {
        mSeed ^= mSeed << 13; mSeed ^= mSeed >> 17; mSeed ^= mSeed << 5;
        return static_cast<float>(mSeed & 0xFFFFFFu) / 16777216.f;
    }
    V3 NyxaraSoul::RandomWay()
    {
        const float y = Random(-1.f, 1.f), a = Random(0.f, TwoPi), r = std::sqrt((std::max)(0.f, 1 - y * y));
        return {r * std::cos(a), y, r * std::sin(a)};
    }

    // ------------------------------------------------------------------ what happens to it
    void NyxaraSoul::Leave(const DoorwaysSkeletonPose& pose, const XMFLOAT4X4& place, const Body& soul)
    {
        mStands = true;
        mState = State::Kept; mAge = 0; mGone = 0; mPull = -1; mHum = 0;
        mPose = pose; mPlace = place; mSoul = soul;
        for (float& owed : mOwed) owed = 0;
        for (auto& line : mDrawn) line.clear();
        const Look& look = TheLook();
        const V3 chest = soul.At[Chest];
        Flash(chest, .9f, look.Hot, .16f, .75f);
        RingOut({soul.Feet.x, soul.Feet.y + .04f, soul.Feet.z}, .4f, 2.6f, look.Mark * 1.4f, .5f, .7f, true);
        Shafts(soul.Feet, 14, 1.f, look.Hot * 2.f, look.String, .5f);
        Sparks(chest, 16, 3.4f, look.Hot * 2.f, look.Spark, .08f, .5f, -1.5f);
    }

    void NyxaraSoul::Pose(const DoorwaysSkeletonPose& pose, const XMFLOAT4X4& place, const Body& soul)
    {
        if (!mStands || mState == State::Joined) return;
        mPose = pose; mPlace = place; mSoul = soul;
    }

    void NyxaraSoul::Ghost(const DoorwaysSkeletonPose& pose, const XMFLOAT4X4& place, float seconds)
    {
        if (mGhosts.size() >= MaxGhosts || seconds <= 0) return;
        Shade g;
        g.Pose = pose; g.Place = place; g.Life = seconds;
        mGhosts.push_back(std::move(g));
    }

    void NyxaraSoul::Recall()
    {
        if (!mStands || mState != State::Kept) return;
        mState = State::Recalled; mPull = 0;
        const Look& look = TheLook();
        Flash(mSoul.At[Chest], .8f, look.Hot, .18f, .7f);
        Flash(mHer.At[Chest], .6f, look.Hot, .14f, .55f);
    }

    void NyxaraSoul::Join()
    {
        if (!mStands || mState == State::Joined) return;
        mState = State::Joined; mGone = 0;
        const Look& look = TheLook();
        const V3 chest = mSoul.At[Chest];
        Flash(chest, 1.2f, look.Hot, .2f, .85f);
        RingOut({mSoul.Feet.x, mSoul.Feet.y + .04f, mSoul.Feet.z}, .6f, 3.6f, look.Mark * 1.6f, .55f, .8f, true);
        Shafts(mSoul.Feet, 24, 1.5f, look.Hot * 2.2f, look.String, .7f);
        Sparks(chest, 22, 4.2f, look.Hot * 2.f, look.Spark, .09f, .6f, 1.2f);
    }

    void NyxaraSoul::Drop()
    {
        mStands = false; mPull = -1;
        mGhosts.clear();
    }

    // ------------------------------------------------------------------ its lights
    NyxaraSoul::Mote& NyxaraSoul::Add(Cell shape, const V3& at)
    {
        if (mMotes.size() >= MaxMotes) mMotes.erase(mMotes.begin(), mMotes.begin() + static_cast<std::ptrdiff_t>(MaxMotes / 8));
        Mote m;
        m.Shape = shape; m.Pos = at;
        mMotes.push_back(m);
        return mMotes.back();
    }

    void NyxaraSoul::Flash(const V3& at, float size, const V3& colour, float life, float alpha)
    {
        Mote& m = Add(Cell::Glow, at);
        m.Size0 = size * .5f; m.Size1 = size; m.Life = life; m.Color0 = colour * 2.4f; m.Color1 = colour; m.Alpha = alpha; m.In = .12f; m.Out = .8f; m.Soft = 1;
        Mote& s = Add(Cell::Star, at);
        s.Size0 = size * .9f; s.Size1 = size * 1.6f; s.Life = life * 1.3f; s.Color0 = colour * 3.f; s.Color1 = colour; s.Alpha = alpha; s.In = .1f; s.Out = .7f; s.Roll = Random(0, Pi);
    }

    void NyxaraSoul::RingOut(const V3& at, float from, float to, const V3& colour, float life, float alpha, bool flat)
    {
        Mote& m = Add(flat ? Cell::Shock : Cell::Ring, at);
        m.Size0 = from; m.Size1 = to; m.Life = life; m.Color0 = colour * 1.6f; m.Color1 = colour; m.Alpha = alpha; m.In = .08f; m.Out = .7f;
        m.Facing = flat ? std::uint8_t{1} : std::uint8_t{0}; m.Soft = .6f; m.Worn1 = flat ? .35f : .2f;
    }

    void NyxaraSoul::Sparks(const V3& at, int count, float speed, const V3& c0, const V3& c1, float size, float life, float rise)
    {
        for (int i = 0; i < count; ++i)
        {
            Mote& m = Add(Cell::Glow, at + RandomWay() * .15f);
            m.Vel = RandomWay() * (speed * Random(.35f, 1.f));
            m.Size0 = size * Random(.7f, 1.3f); m.Size1 = 0; m.Long = 3.2f; m.Facing = 2;
            m.Life = life * Random(.6f, 1.2f); m.Color0 = c0; m.Color1 = c1; m.Alpha = 1; m.In = .05f; m.Out = .6f; m.Rise = rise; m.Drag = 2.6f; m.Soft = .5f;
        }
    }

    // Light going up out of the ground where it stands (its mark flaring): thin rays, drawn out along their way
    void NyxaraSoul::Shafts(const V3& feet, int count, float reach, const V3& c0, const V3& c1, float life)
    {
        for (int i = 0; i < count; ++i)
        {
            const float a = Random(0, TwoPi), r = Random(.12f, .6f);
            Mote& m = Add(Cell::Needle, {feet.x + std::cos(a) * r, feet.y + Random(0.f, .3f), feet.z + std::sin(a) * r});
            m.Vel = {std::cos(a) * .35f, reach * Random(4.f, 7.5f), std::sin(a) * .35f};
            m.Size0 = Random(.025f, .045f); m.Size1 = .01f; m.Long = Random(14.f, 22.f); m.Facing = 2;
            m.Life = life * Random(.55f, 1.f); m.Color0 = c0; m.Color1 = c1; m.Alpha = .9f; m.In = .04f; m.Out = .55f; m.Drag = 3.f; m.Soft = .4f;
        }
    }

    // What it gives off while it stands: glints along its threads (where they were last drawn), and motes of light going
    // up from where it stands.
    void NyxaraSoul::Shed(float dt)
    {
        const Look& look = TheLook();
        const auto due = [&](int slot, float rate)
        {
            mOwed[slot] += rate * dt;
            const int count = static_cast<int>(mOwed[slot]);
            mOwed[slot] -= F(count);
            return (std::min)(count, 40);
        };
        static constexpr Tie ties[] = {Head, Chest, HandL, HandR, TipL, TipR};
        for (int i = due(0, 22.f + 70.f * mHum); i > 0; --i)
        {
            const int k = (std::min)(static_cast<int>(Random() * 6), 5);
            const std::vector<V3>& line = mDrawn[k];
            const V3 on = line.size() >= 2 ? line[(std::min)(static_cast<std::size_t>(Random() * static_cast<float>(line.size())), line.size() - 1)]
                                           : Mix(mSoul.At[ties[k]], mHer.At[ties[k]], Random());
            Mote& m = Add(Cell::Star, on + RandomWay() * (.01f + .05f * mHum));
            m.Size0 = Random(.07f, .15f); m.Size1 = 0; m.Life = Random(.18f, .4f); m.Color0 = look.Hot * 2.6f; m.Color1 = look.Spark; m.Alpha = .95f;
            m.Roll = Random(0, Pi); m.In = .2f; m.Out = .6f;
        }
        for (int i = due(1, 5.f); i > 0; --i)
        {   // (and a mote of it goes up from where it stands)
            const float a = Random(0, TwoPi), r = Random(.2f, .7f);
            Mote& m = Add(Cell::Glow, {mSoul.Feet.x + std::cos(a) * r, mSoul.Feet.y + Random(.05f, 1.6f), mSoul.Feet.z + std::sin(a) * r});
            m.Vel = {0, Random(.25f, .6f), 0}; m.Size0 = Random(.03f, .06f); m.Size1 = 0; m.Life = Random(.8f, 1.5f);
            m.Color0 = look.Spark * 2.f; m.Color1 = look.String; m.Alpha = .8f; m.In = .2f; m.Out = .5f;
        }
    }

    void NyxaraSoul::Update(float dt, const Body& her, float pull)
    {
        mClock += dt;
        if (mHerKnown && dt > 1e-5f)
        {   // (her threads hum as she moves fast, and come to rest)
            const float speed = Len(her.At[Chest] - mHerWas) / dt;
            mHum = (std::max)(mHum * std::exp(-3.5f * dt), std::clamp((speed - 2.f) / 10.f, 0.f, 1.f));
        }
        mHerWas = her.At[Chest]; mHerKnown = true;
        mHer = her; mPull = pull;
        if (mStands)
        {
            mAge += dt;
            if (mState == State::Joined) { if ((mGone += dt) >= JoinSeconds) mStands = false; }
            else if (dt > 0) Shed(dt);
        }
        for (Mote& m : mMotes)
        {
            m.T += dt;
            m.Vel.y += m.Rise * dt;
            const float keep = std::exp(-m.Drag * dt);
            m.Vel = m.Vel * keep;
            m.Pos = m.Pos + m.Vel * dt;
            m.Roll += m.Spin * dt;
        }
        std::erase_if(mMotes, [](const Mote& m) { return m.T >= m.Life; });
        for (Shade& g : mGhosts) g.T += dt;
        std::erase_if(mGhosts, [](const Shade& g) { return g.T >= g.Life; });
    }

    // ------------------------------------------------------------------ drawing
    void NyxaraSoul::Quad(const V3& a, const V3& b, const V3& c, const V3& d, Cell shape, float u0, float u1, const V3& colourA, const V3& colourB,
                          float alphaA, float alphaB, float worn, float soft, float light, float depth)
    {
        if (mVertices.size() + 4 > MaxQuads * 4) return;
        const int cell = static_cast<int>(shape);
        const float wide = 1.f / F(AtlasCols), high = 1.f / F(AtlasRows), cu = F(cell % AtlasCols) * wide, cv = F(cell / AtlasCols) * high;
        const float ua = cu + std::clamp(u0, .004f, .996f) * wide, ub = cu + std::clamp(u1, .004f, .996f) * wide, va = cv + .004f * high, vb = cv + .996f * high;
        const std::uint32_t first = static_cast<std::uint32_t>(mVertices.size());
        const auto put = [&](const V3& p, float u, float v, const V3& colour, float alpha)
        {
            DoorwaysGpuDMeshVertex x{};
            x.Position = p; x.Normal = {worn, soft, light}; x.TexCoord = {u, v}; x.Tangent = {colour.x, colour.y, colour.z, alpha};
            mVertices.push_back(x);
        };
        put(a, ua, va, colourA, alphaA); put(b, ub, va, colourB, alphaB);
        put(c, ub, vb, colourB, alphaB); put(d, ua, vb, colourA, alphaA);
        if (light >= .999f) mLights.push_back(first);
        else mCovering.push_back({depth, first});
    }

    // A band of light along a line of points, turned to the eye: at each point so wide (the light itself is the middle
    // two fifths of that: the atlas's soft round, cut through its middle) and so much of it; its colour goes from
    // colour to hot by heat.
    void NyxaraSoul::Band(const std::vector<V3>& line, const std::vector<float>& wide, const std::vector<float>& alpha, const V3& colour, const V3& hot,
                          const std::vector<float>& heat, float worn, float soft)
    {
        const std::size_t n = line.size();
        if (n < 2 || wide.size() != n || alpha.size() != n || heat.size() != n) return;
        V3 sideWas{};
        for (std::size_t i = 0; i < n; ++i)
        {
            const V3 along = Norm(line[(std::min)(i + 1, n - 1)] - line[i > 0 ? i - 1 : 0], Up);
            const V3 side = Norm(Cross(along, mEye - line[i]), {1, 0, 0}) * (.5f * wide[i]);
            if (i > 0 && (alpha[i - 1] > .003f || alpha[i] > .003f))
                Quad(line[i - 1] + sideWas, line[i] + side, line[i] - side, line[i - 1] - sideWas, Cell::Glow, .5f, .5f,
                     Mix(colour, hot, std::clamp(heat[i - 1], 0.f, 1.f)), Mix(colour, hot, std::clamp(heat[i], 0.f, 1.f)), alpha[i - 1], alpha[i], worn, soft, 1.f, 0.f);
            sideWas = side;
        }
    }

    void NyxaraSoul::Flat(const V3& at, float size, float roll, Cell shape, const V3& colour, float alpha, float worn, float soft, float light)
    {
        if (alpha <= .003f || size <= 1e-4f) return;
        const float c = std::cos(roll) * .5f * size, s = std::sin(roll) * .5f * size;
        const V3 x{c, 0, s}, z{-s, 0, c}, toEye = mEye - at;
        Quad(at - x + z, at + x + z, at + x - z, at - x - z, shape, 0, 1, colour, colour, alpha, alpha, worn, soft, light, Dot(toEye, toEye));
    }

    void NyxaraSoul::Eye(const V3& at, float size, float roll, Cell shape, const V3& colour, float alpha, float soft)
    {
        if (alpha <= .003f || size <= 1e-4f) return;
        const V3 look = Norm(mEye - at), right = Norm(Cross(Up, look), {1, 0, 0}), over = Cross(look, right);
        const float c = std::cos(roll) * .5f * size, s = std::sin(roll) * .5f * size;
        const V3 x = right * c + over * s, y = over * c - right * s;
        Quad(at - x + y, at + x + y, at + x - y, at - x - y, shape, 0, 1, colour, colour, alpha, alpha, 0, soft, 1.f, 0.f);
    }

    // The line of a string from a to b, in so many pieces: hanging by sag (m, at its middle), never under the ground.
    void NyxaraSoul::Hang(const V3& a, const V3& b, int pieces, float sag, float floor, std::vector<V3>& out) const
    {
        out.clear();
        for (int i = 0; i <= pieces; ++i)
        {
            const float u = F(i) / F(pieces);
            V3 p = Mix(a, b, u);
            p.y = (std::max)(p.y - sag * 4 * u * (1 - u), (std::min)(floor, p.y));
            out.push_back(p);
        }
    }

    // Her threads (brighter since the owner chose them: "make the strings little brighter"): six, from her soul's head,
    // chest, hands and wing tips to hers, each a band of light with bright runs along it, running from her soul to her,
    // and a breath of light about it, a glint where it is tied. Drawn tight, they hum as a plucked string does while she
    // moves fast (standing waves across them, dying away); on her way back they are straight and bright. (Far from her
    // soul, as the game lets her be, a thread has no more runs than one 40 m long, drawn out along it, and is drawn in
    // more pieces: up to 160.)
    void NyxaraSoul::DrawThreads(float shown)
    {
        static constexpr Tie ties[] = {Head, Chest, HandL, HandR, TipL, TipR};
        const Look& look = TheLook();
        const float pull = (std::max)(mPull, 0.f), taut = mState == State::Recalled ? 1.f : 0.f;
        const float ground = (std::min)(mSoul.Feet.y, mHer.Feet.y);
        int which = 0;
        for (const Tie tie : ties)
        {
            const int k = which++;
            const float own = F(k);
            mDrawn[k].clear();
            const V3 a = mSoul.At[tie], b = mHer.At[tie];
            const float length = Len(b - a);
            if (length < .08f) continue;
            const int pieces = std::clamp(static_cast<int>(length * 4) + 8, 10, 160);
            const float runs = (std::min)(length, 40.f) * .42f;   // (how many bright runs along it)
            Hang(a, b, pieces, (std::min)(.006f * length * length, .12f) * (1 - taut), ground + .05f, mLine);
            const V3 along = Norm(b - a), side = Norm(Cross(along, Up), {1, 0, 0}), over = Cross(side, along);
            const float hum = (.004f + .055f * mHum) * (1 - .85f * taut);
            const float apart = Smooth(.08f, .7f, length);   // (her soul's own length from it they are not yet seen)
            mWide.clear(); mAlpha.clear(); mHeat.clear();
            for (int i = 0; i <= pieces; ++i)
            {
                const float u = F(i) / F(pieces);
                V3& p = mLine[static_cast<std::size_t>(i)];
                const float across = std::sin(3 * Pi * u) * std::cos(mClock * 31.f + own * 1.7f), up = std::sin(5 * Pi * u) * std::cos(mClock * 47.f + own * 2.9f);
                p = p + side * (hum * across) + over * (hum * up);
                const float bead = Bead(u * runs - mClock * (2.1f + 3.f * pull) + own * .37f, 7.f);
                mWide.push_back(.055f + .035f * bead + .05f * pull);
                mAlpha.push_back(shown * apart * (.62f + .5f * bead + .7f * pull));
                mHeat.push_back(bead);
            }
            Band(mLine, mWide, mAlpha, look.String * (2.2f + 1.5f * pull), look.Hot * 3.2f, mHeat, 0.f, .42f);
            mDrawn[k] = mLine;
            // (a breath of light about it)
            for (float& w : mWide) w *= 5.f;
            for (float& al : mAlpha) al *= .17f;
            Band(mLine, mWide, mAlpha, look.String, look.String, mHeat, 0.f, 1.f);
            // where it is tied: a glint on her, a smaller one on her soul
            const float twinkle = .75f + .25f * std::sin(mClock * 9.f + own * 2.1f);
            Eye(b, (.2f + .1f * pull) * twinkle, own * .6f, Cell::Star, look.Hot * 2.6f, shown * apart, .6f);
            Eye(a, .13f * twinkle, own * .6f + .4f, Cell::Star, look.Hot * 2.f, shown * apart * .8f, .6f);
        }
    }

    // The mark her soul stands on: a thin ring and a pale light within.
    void NyxaraSoul::DrawMark(float shown)
    {
        const Look& look = TheLook();
        const V3 at{mSoul.Feet.x, mSoul.Feet.y + .035f, mSoul.Feet.z};
        const float beat = .5f + .5f * std::sin(mClock * 2.1f), pull = (std::max)(mPull, 0.f);
        Flat(at, 1.5f, 0, Cell::Ring, look.Mark * 1.8f, shown * (.6f + .2f * beat + .3f * pull), 0, .5f, 1);
        Flat(at, 2.2f, 0, Cell::Glow, look.Mark * .7f, shown * .28f, 0, 1, 1);
    }

    void NyxaraSoul::DrawMotes()
    {
        for (const Mote& m : mMotes)
        {
            if (m.T < 0 || m.T >= m.Life) continue;
            const float t = m.T / m.Life;
            const float size = Mix(m.Size0, m.Size1, 1 - (1 - t) * (1 - t));
            const float alpha = m.Alpha * Smooth(0, (std::max)(m.In, 1e-3f), t) * (1 - Smooth(1 - m.Out, 1.f, t));
            if (alpha <= .003f || size <= 1e-4f) continue;
            const V3 colour = Mix(m.Color0, m.Color1, t), toEye = mEye - m.Pos;
            const float worn = Mix(m.Worn0, m.Worn1, t);
            V3 a, b, c, d;
            if (m.Facing == 1)
            {   // flat, as it was made
                const float cs = std::cos(m.Roll) * .5f * size, sn = std::sin(m.Roll) * .5f * size;
                const V3 x = m.AxisX * cs + m.AxisZ * sn, z = m.AxisZ * cs - m.AxisX * sn;
                a = m.Pos - x + z; b = m.Pos + x + z; c = m.Pos + x - z; d = m.Pos - x - z;
            }
            else if (m.Facing == 2 && Len(m.Vel) > .2f)
            {   // drawn out along its flight
                const V3 dir = Norm(m.Vel), side = Norm(Cross(dir, toEye), {1, 0, 0}) * (.5f * size), half = dir * (.5f * size * m.Long);
                a = m.Pos - half + side; b = m.Pos + half + side; c = m.Pos + half - side; d = m.Pos - half - side;
            }
            else
            {   // to the eye
                const V3 look = Norm(toEye), right = Norm(Cross(Up, look), {1, 0, 0}), over = Cross(look, right);
                const float cs = std::cos(m.Roll) * .5f * size, sn = std::sin(m.Roll) * .5f * size;
                const V3 x = right * cs + over * sn, y = over * cs - right * sn;
                a = m.Pos - x + y; b = m.Pos + x + y; c = m.Pos + x - y; d = m.Pos - x - y;
            }
            Quad(a, b, c, d, m.Shape, 0, 1, colour, colour, alpha, alpha, worn, m.Soft, m.Light, Dot(toEye, toEye));
        }
    }

    // Her body once more, as light (the engine's Echo material): presence 0 to 1 how much of it is there, worn 0 to 1 how
    // much of it the noise has taken away, bright how bright against the look's own.
    bool NyxaraSoul::DrawBody(Renderer& renderer, WorldRenderer& worldRenderer, const DoorwaysAnimatedCharacterInstance& character, const XMMATRIX& viewProjection,
                              const DoorwaysSkeletonPose& pose, const XMFLOAT4X4& place, float presence, float worn, float bright)
    {
        if (presence <= .004f || worn >= .999f || pose.LocalJoints.empty()) return false;
        const Look& look = TheLook();
        Material material{};
        material.Diffuse = {look.Rim.x, look.Rim.y, look.Rim.z, std::clamp(presence, 0.f, 1.f)};
        material.SpecularColor = {look.Inside.x, look.Inside.y, look.Inside.z, look.Cover};
        material.SpecularPower = look.RimPower;
        material.EmissiveStrength = look.Bright * bright;
        material.NormalMap = mNoise.Get();
        material.TexTransform = {look.NoiseSize, look.NoiseRise, look.NoiseShow, look.Bands};
        material.MetallicFactor = std::clamp(worn, 0.f, 1.f); material.RoughnessFactor = look.Soft;
        material.DoubleSided = false; material.AlphaMode = DoorwaysMaterialAlphaMode::Blend; material.Mode = DoorwaysMaterialMode::Echo; material.ReceiveFog = false;
        renderer.SetPremultipliedBlendingEnabled(true);
        renderer.SetDepthWriteEnabled(false);
        const bool drawn = worldRenderer.DrawCharacterEcho(renderer, character, pose, XMLoadFloat4x4(&place), viewProjection, material);
        renderer.ResetDefaultWorldRenderState();
        renderer.BindDoorwaysSkinningDisabled();
        return drawn;
    }

    void NyxaraSoul::Draw(Renderer& renderer, WorldRenderer& worldRenderer, const DoorwaysAnimatedCharacterInstance& character, const V3& eye,
                          const XMMATRIX& viewProjection, bool glowSet, bool unseen)
    {
        mEye = eye; mQuadsDrawn = 0;
        const bool out = mLoaded && Out() && !unseen;
        if (out && !glowSet)
        {   // the picture's glow, where her flair has set none
            DoorwaysScreenFlair f;
            f.GlowStrength = TheLook().Glow; f.GlowThreshold = GlowFrom; f.GlowKnee = GlowKnee;
            renderer.SetDoorwaysScreenFlair(f);
            mScreenSet = true;
        }
        else if (mScreenSet)
        {   // (her flair sets it itself now, or nothing of her soul is left: what was set here is taken away)
            if (!glowSet) renderer.SetDoorwaysScreenFlair(DoorwaysScreenFlair{});
            mScreenSet = false;
        }
        if (!out) return;
        float shown = 0;
        if (mStands)
        {
            const Look& look = TheLook();
            const float formed = Smooth(0, FormSeconds, mAge), pull = (std::max)(mPull, 0.f);
            float presence = formed, worn = Mix(1.f, look.Worn, formed), bright = 1 + .08f * std::sin(mClock * 2.3f) + .5f * pull;
            shown = formed;
            if (mState == State::Joined)
            {   // she is in it again: it flares, and the noise takes it
                const float gone = std::clamp(mGone / JoinSeconds, 0.f, 1.f);
                worn = Mix(look.Worn, 1.f, gone); bright += 1.6f * (1 - gone); presence = 1 - gone * gone; shown = 1 - gone;
            }
            DrawBody(renderer, worldRenderer, character, viewProjection, mPose, mPlace, presence, worn, bright);
        }
        for (const Shade& g : mGhosts)
        {   // her afterimages: each fainter and more worn until it is gone
            const float gone = std::clamp(g.T / g.Life, 0.f, 1.f);
            DrawBody(renderer, worldRenderer, character, viewProjection, g.Pose, g.Place, .85f * (1 - gone) * (1 - gone), Mix(.04f, .85f, gone), 1.f);
        }
        mVertices.clear(); mCovering.clear(); mLights.clear(); mIndices.clear();
        if (mStands)
        {
            DrawMark(shown);
            DrawThreads(shown);
        }
        DrawMotes();
        if (mVertices.empty()) return;
        // What covers (smoke, the stain) first, the far ones before the nearer; then all that is light, which adds in any order.
        std::sort(mCovering.begin(), mCovering.end(), [](const Piece& x, const Piece& y) { return x.Depth > y.Depth; });
        const auto six = [&](std::uint32_t first) { for (const std::uint32_t corner : {0u, 1u, 2u, 0u, 2u, 3u}) mIndices.push_back(first + corner); };
        for (const Piece& piece : mCovering) six(piece.First);
        for (const std::uint32_t first : mLights) six(first);
        if (!renderer.UpdateDoorwaysDynamicGpuMesh(mMesh, mVertices.data(), static_cast<unsigned int>(mVertices.size()), mIndices.data(), static_cast<unsigned int>(mIndices.size()))) return;
        const Look& look = TheLook();
        Material material{};
        material.Diffuse = {1, 1, 1, 1}; material.DiffuseMap = mAtlas.Get(); material.NormalMap = mNoise.Get(); material.TexTransform = {1, 1, 0, 0};
        material.SpecularColor = {look.Wear[0], look.Wear[1], look.Wear[2], look.Wear[3]};
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
