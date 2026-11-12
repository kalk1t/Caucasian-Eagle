#include "NyxaraKit.h"

#include "DoorwaysGamepad.h"

#include "CombatSystem.h"
#include "DoorwaysGame.h"
#include "GameConstants.h"
#include "NyxaraHud.h"
#include "Sound.h"
#include "DoorwaysRuntimePathResolver.h"
#include "Renderer.h"
#include "WorldRenderer.h"
#include "WorldState.h"
#include "DoorwaysAsset/DoorwaysDMeshLoader.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <functional>

using namespace DirectX;

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
        const XMFLOAT4 Moonfeather{.80f, .90f, 1.f, 1.f};   // silver moonlight
        const XMFLOAT4 Talonshade{.22f, 1.f, .52f, 1.f};    // soul green
        const XMFLOAT4 MoonWhite{.86f, .97f, 1.f, 1.f};
        const XMFLOAT4 ShadeViolet{.62f, .28f, 1.f, 1.f};

        struct ActDef
        {
            const wchar_t* Name;
            const char* Clip;
            float Rate;              // clip seconds per real second
            float End;               // clip time at which control returns
            std::array<float, 4> Hits;
            int HitCount;
        };
        // Hit times are the measured hand-speed peaks of each authored clip.
        const std::array<ActDef, static_cast<int>(NyxaraKit::Act::Count)> Acts = {{
            {L"LEFT TALON", "BasicAttack", 1.45f, 1.00f, {.53f}, 1},
            {L"RIGHT TALON", "SlashLeft", 1.45f, 1.02f, {.63f}, 1},
            {L"CHARGED SLASH", "HeavyAttack", 1.30f, 1.75f, {1.20f}, 1},
            {L"TALON DASH", "Skill1", 1.30f, 2.30f, {.83f, 1.43f}, 2},
            {L"FEATHER CYCLONE", "Skill2", 1.25f, 2.25f, {.40f, .90f, 1.27f, 1.60f}, 4},
            {L"MOON LEAP", "Skill3", 1.20f, 2.10f, {1.00f}, 1},
            {L"TWIN TALONS", "Combo1L", 1.30f, 2.20f, {.60f, 1.17f, 1.77f}, 3},
            {L"RISING EAGLE", "Combo1R", 1.25f, 1.90f, {1.03f}, 1},
            {L"TEMPEST TRIPLE", "Combo2L", 1.30f, 2.70f, {.73f, 1.50f, 2.20f}, 3},
            {L"WHIRLWIND TALONS", "Combo2R", 1.20f, 1.80f, {.97f}, 1},
            {L"SOUL WEAVE", "Combo3L", 1.35f, 3.40f, {1.17f, 2.00f, 3.10f}, 3},
            {L"MOONFALL DIVE", "Combo3R", 1.15f, 2.40f, {1.32f}, 1},
            {L"SWORD BARRIER", "Guard", 1.00f, 1.33f, {}, 0},
            {L"SWORD EXCHANGE", "SwordSwap", 1.10f, .86f, {.43f}, 1},
        }};
        const ActDef& Def(NyxaraKit::Act act) { return Acts[static_cast<int>(act)]; }
        constexpr float SkillCooldown[3] = {4.f, 7.f, 9.f};
        constexpr float GuardHoldClip = .42f;

        XMFLOAT3 Add(XMFLOAT3 a, XMFLOAT3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
        XMFLOAT3 Sub(XMFLOAT3 a, XMFLOAT3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
        XMFLOAT3 Mul(XMFLOAT3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
        float Len(XMFLOAT3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
        float LenXZ(XMFLOAT3 a) { return std::sqrt(a.x * a.x + a.z * a.z); }
        XMFLOAT3 Norm(XMFLOAT3 a) { const float l = Len(a); return l > 1e-5f ? Mul(a, 1 / l) : XMFLOAT3{0, 0, 1}; }
        XMFLOAT3 Cross(XMFLOAT3 a, XMFLOAT3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
        XMFLOAT3 Forward(float yaw) { return {std::sin(yaw), 0, std::cos(yaw)}; }
        XMFLOAT3 RightOf(float yaw) { return {std::cos(yaw), 0, -std::sin(yaw)}; }
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

        bool BuildArc(Renderer& r, DoorwaysGpuMesh& out, std::string& e)
        {   // crescent in XZ around +Z, -80..+80 degrees; u along swing, v inner->outer
            MeshBuilder b; const int n = 40;
            for (int i = 0; i <= n; ++i)
            {
                const float u = static_cast<float>(i) / n, a = (-80 + 160 * u) * Pi / 180;
                const float taper = .70f + .3f * std::sin(u * Pi);
                b.Vertex({std::sin(a) * (1 - .32f * taper), 0, std::cos(a) * (1 - .32f * taper)}, {0, 1, 0}, u, 0);
                b.Vertex({std::sin(a), 0, std::cos(a)}, {0, 1, 0}, u, 1);
                if (i) b.Quad(2 * i - 2, 2 * i - 1, 2 * i + 1, 2 * i);
            }
            return b.Upload(r, out, e);
        }
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
        bool BuildDisc(Renderer& r, DoorwaysGpuMesh& out, std::string& e)
        {   // XZ plane radius-1 square with planar UV
            MeshBuilder b;
            b.Quad(b.Vertex({-1, 0, -1}, {0, 1, 0}, 0, 1), b.Vertex({1, 0, -1}, {0, 1, 0}, 1, 1),
                b.Vertex({1, 0, 1}, {0, 1, 0}, 1, 0), b.Vertex({-1, 0, 1}, {0, 1, 0}, 0, 0));
            return b.Upload(r, out, e);
        }
        bool BuildCylinder(Renderer& r, DoorwaysGpuMesh& out, std::string& e)
        {
            MeshBuilder b; const int n = 28;
            for (int i = 0; i <= n; ++i)
            {
                const float u = static_cast<float>(i) / n, a = u * 2 * Pi;
                const XMFLOAT3 nrm{std::sin(a), 0, std::cos(a)};
                b.Vertex({nrm.x, 0, nrm.z}, nrm, u, 0); b.Vertex({nrm.x, 1, nrm.z}, nrm, u, 1);
                if (i) b.Quad(2 * i - 2, 2 * i, 2 * i + 1, 2 * i - 1);
            }
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

    float NyxaraKit::Random()
    {
        mSeed = mSeed * 1664525u + 1013904223u;
        return static_cast<float>((mSeed >> 8) & 0xFFFFFF) / 16777216.f;
    }

    XMFLOAT4 NyxaraKit::RightColor() const { return mSwapped ? Talonshade : Moonfeather; }
    XMFLOAT4 NyxaraKit::LeftColor() const { return mSwapped ? Moonfeather : Talonshade; }

    bool NyxaraKit::Initialize(Renderer& renderer, WorldRenderer& worldRenderer, WorldState& world, std::string& error)
    {
        mReady = false;
        if (!BuildArc(renderer, mArcMesh, error) || !BuildRing(renderer, mRingMesh, error) ||
            !BuildQuad(renderer, mQuadMesh, error, -.5f) || !BuildQuad(renderer, mFeatherMesh, error, 0) ||
            !BuildDisc(renderer, mDiscMesh, error) || !BuildCylinder(renderer, mCylinderMesh, error) ||
            !BuildSphere(renderer, mSphereMesh, error) || !BuildCone(renderer, mConeMesh, error) ||
            !BuildBlade(renderer, mBladeMesh, error))
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
        if (!load("SoulArc.png", mArcTex) || !load("Glow.png", mGlowTex) || !load("Ring.png", mRingTex) ||
            !load("Feather.png", mFeatherTex) || !load("Beam.png", mBeamTex) || !load("MoonSigil.png", mSigilTex))
            return false;

        mSwapped = false;
        ApplySwordVisibility(worldRenderer);
        // Six shade wraiths hover in a ring around the arrival point.
        mWraiths.clear();
        const XMFLOAT3 origin = world.MainPlayer.Position;
        for (int i = 0; i < 6; ++i)
        {
            Wraith w;
            const float a = (static_cast<float>(i) + .5f) * Pi / 3, radius = 8.5f + 2.5f * static_cast<float>(i % 2);
            w.Anchor = {origin.x + std::sin(a) * radius, origin.y + 1.3f, origin.z + std::cos(a) * radius};
            w.Pos = w.Anchor; w.Phase = static_cast<float>(i) * 1.7f; w.Shoot = 3.f + static_cast<float>(i) * .9f;
            mWraiths.push_back(w);
        }
        mFx.clear(); mOrbs.clear();
        mAct = Act::None;
        GameSession().Active = true;
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
        Burst(at, MoonWhite, 14, 4.f, .2f); Burst(at, Talonshade, 8, 3.f, .18f);
        mNotice = L"BLOCKED"; mNoticeTimer = .5f;
        return true;
    }

    void NyxaraKit::RingEffect(const XMFLOAT3& at, float size, XMFLOAT4 color, float life)
    {
        Fx ring; ring.Kind = FxKind::Ring; ring.Pos = at; ring.Size = size; ring.Life = life; ring.Color = color;
        mFx.push_back(ring);
    }

    void NyxaraKit::ApplySwordVisibility(WorldRenderer& worldRenderer)
    {
        worldRenderer.SetCharacterMaterialVisible("Slot_sword_right__Moonfeather", !mSwapped);
        worldRenderer.SetCharacterMaterialVisible("Slot_sword_left__Talonshade", !mSwapped);
        worldRenderer.SetCharacterMaterialVisible("Slot_sword_right__Talonshade", mSwapped);
        worldRenderer.SetCharacterMaterialVisible("Slot_sword_left__Moonfeather", mSwapped);
    }

    NyxaraKit::Keys NyxaraKit::ReadKeys(bool inputAllowed) const
    {
        Keys k;
        if (mInputBlocked) return k;
        if (GameSession().Unattended && mAutoKeys) return *mAutoKeys; // the journey's test pilot
        if (GameSession().Unattended)
        {   // Scripted showcase: (start, end, key) windows on the kit clock; never reads the desktop.
            struct Step { float A, B; char Key; };
            static const Step steps[] = {
                {2.0f, 2.1f, '1'}, {3.3f, 3.4f, 'L'}, {6.6f, 6.7f, '3'}, {8.0f, 8.1f, 'R'},
                {11.f, 13.2f, 'E'}, {14.f, 14.1f, 'Q'}, {15.6f, 15.7f, '2'}, {17.4f, 17.5f, 'R'},
                {20.f, 20.1f, 'L'}, {20.5f, 20.6f, 'L'}, {21.6f, 21.7f, 'R'}, {27.6f, 30.5f, 'S'},
                {23.7f, 23.8f, '3'}, {25.f, 25.1f, 'L'}, {28.f, 28.1f, '2'}, {29.3f, 29.4f, 'L'}};
            for (const auto& step : steps)
                if (mDemoClock >= step.A && mDemoClock < step.B)
                    switch (step.Key)
                    {
                    case '1': k.K1 = true; break; case '2': k.K2 = true; break; case '3': k.K3 = true; break;
                    case 'L': k.Lmb = true; break; case 'R': k.Rmb = true; break; case 'E': k.E = true; break;
                    case 'Q': k.Q = true; break; case 'S': k.Shift = true; break;
                    }
            // Travel while attacking: run forward, strafe right, backpedal, strafe left.
            struct Move { float A, B; char Key; };
            static const Move moves[] = {{1.6f, 5.6f, 'W'}, {15.2f, 19.f, 'D'}, {19.8f, 22.6f, 'S'}, {27.6f, 30.5f, 'A'}};
            GameSession().ScriptedMove = 0;
            for (const auto& move : moves)
                if (mDemoClock >= move.A && mDemoClock < move.B) GameSession().ScriptedMove = move.Key;
            return k;
        }
        if (!inputAllowed) return k;
        const auto down = [](int vk) { return IsDoorwaysKeyDown(vk); };   // the key, or the controller button bound to it
        return {down(VK_LBUTTON), down(VK_RBUTTON), down('Q'), down('E'), down('F'), down('V'), down('C'), down(VK_SHIFT)};
    }

    float NyxaraKit::ScaleFrameTime(float realDeltaTime, bool inputAllowed)
    {
        if (!mReady) return realDeltaTime;
        (void)inputAllowed; // Shift is the wing boost (faster travel); the world runs at normal speed
        mTimeScale = 1;     // except for the short hit-stop freeze on impacts.
        if (mHitStop > 0) { mHitStop -= realDeltaTime; return realDeltaTime * .06f; }
        return realDeltaTime;
    }

    int NyxaraKit::NearestWraith(const XMFLOAT3& from, float maxDistance, int skipMask) const
    {
        int best = -1; float bestDistance = maxDistance;
        for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
        {
            if (!mWraiths[i].Alive || (skipMask & (1 << i))) continue;
            const float d = Len(Sub(mWraiths[i].Pos, from));
            if (d < bestDistance) { bestDistance = d; best = i; }
        }
        return best;
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

    void NyxaraKit::Impact(const XMFLOAT3& at, int damage, XMFLOAT4 color)
    {   // Fight feel: freeze frames + camera shake scaled by the blow, a cross-slash scar and a shock flash.
        const float heavy = std::clamp(static_cast<float>(damage) / 60.f, 0.f, 1.f);
        mHitStop = (std::max)(mHitStop, .035f + .075f * heavy);
        mShake = (std::max)(mShake, .03f + .13f * heavy);
        Burst(at, color, 10 + static_cast<int>(18 * heavy), 4.5f + 4.f * heavy, .2f + .12f * heavy);
        Burst(at, MoonWhite, 6 + static_cast<int>(6 * heavy), 8.f, .12f);
        for (int i = 0; i < 2; ++i)
        {
            Fx a; a.Kind = FxKind::Arc; a.Pos = at; a.Yaw = Random() * 2 * Pi; a.Roll = (i ? 55.f : -55.f) * Pi / 180;
            a.Size = .8f + .7f * heavy; a.Life = .15f + .1f * heavy; a.Color = i ? MoonWhite : color; a.Mirror = i == 1;
            mFx.push_back(a);
        }
        Fx g; g.Kind = FxKind::Ring; g.Pos = at; g.Size = .8f + 1.6f * heavy; g.Life = .2f + .1f * heavy; g.Color = MoonWhite; mFx.push_back(g);
    }

    void NyxaraKit::Dash(float speed, float seconds, float yaw, int damage)
    {
        mDashSpeed = speed; mDashTime = seconds; mDashYaw = yaw; mDashDamage = damage; mDashMask = 0;
    }

    float NyxaraKit::AirProfile(float t) const
    {   // Aerial height (m) of the current action over its clip time.
        if (mAct == Act::None) return 0;
        const auto& d = Def(mAct);
        const float h0 = d.HitCount ? d.Hits[0] : 0.f;
        switch (mAct)
        {
        case Act::Skill3:  return 2.4f * Smooth(0, h0 - .3f, t) * (1 - Smooth(h0 - .2f, h0, t));
        case Act::Combo3R: return 3.3f * Smooth(0, h0 - .3f, t) * (1 - Smooth(h0 - .2f, h0, t));
        case Act::Combo1R: return 2.3f * Smooth(h0 - .35f, h0 + .1f, t) * (1 - Smooth(d.End - .55f, d.End, t));
        case Act::Combo3L: return 1.9f * Smooth(0, .5f, t) * (1 - Smooth(d.End - .5f, d.End, t));
        case Act::Skill2:  return .55f * Smooth(0, .4f, t) * (1 - Smooth(d.End - .4f, d.End, t));
        default: return 0;
        }
    }

    void NyxaraKit::DamageWraith(int index, int damage, const XMFLOAT3& from, float push, float launch, XMFLOAT4 color)
    {
        auto& w = mWraiths[index];
        if (!w.Alive) return;
        damage = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(damage) * mDamageScale * w.Weak)));
        w.Health -= damage; w.Flash = .16f;
        XMFLOAT3 away = Sub(w.Pos, from); away.y = 0; away = Norm(away);
        w.Push = Add(w.Push, Mul(away, push * w.PushScale));
        if (launch > 0 && w.PushScale > .5f) w.LiftVel = (std::max)(w.LiftVel, launch);
        Impact(Add(w.Pos, {0, w.Lift, 0}), damage, color);
        CaucasianEagle::Sound::Play("combat/hit", .75f, .12f);
        if (w.Health > 0 && Random() < .4f) CaucasianEagle::Sound::Play("creatures/hurt", .6f, .15f);
        if (w.Health <= 0)
        {
            w.Alive = false; w.Respawn = 8.f; w.Health = 0;
            Burst(w.Pos, ShadeViolet, 26, 6.f, .35f);
            Burst(w.Pos, color, 18, 3.f, .3f);
            Fx ring; ring.Kind = FxKind::Ring; ring.Pos = {w.Pos.x, w.Pos.y - w.Lift - 1.2f, w.Pos.z}; ring.Size = 2.2f;
            ring.Life = .6f; ring.Color = ShadeViolet; mFx.push_back(ring);
        }
    }

    void NyxaraKit::MeleeArc(const WorldState& world, float radius, float halfAngleDegrees, int damage, float launch, XMFLOAT4 color)
    {
        const auto& p = world.MainPlayer;
        const XMFLOAT3 f = Forward(p.Yaw);
        const float cosLimit = std::cos(halfAngleDegrees * Pi / 180);
        for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
        {
            const auto& w = mWraiths[i];
            if (!w.Alive) continue;
            XMFLOAT3 d = Sub(w.Pos, p.Position);
            if (std::abs(d.y - 1.2f) > 2.6f) continue;
            d.y = 0;
            const float dist = Len(d);
            if (dist > radius + w.Radius) continue;
            if (dist > .6f && (d.x * f.x + d.z * f.z) / dist < cosLimit) continue;
            DamageWraith(i, damage, p.Position, 3.5f, launch, color);
        }
    }

    void NyxaraKit::Area(const XMFLOAT3& center, float radius, int damage, float launch, XMFLOAT4 color)
    {
        for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
        {
            const auto& w = mWraiths[i];
            if (!w.Alive) continue;
            XMFLOAT3 d = Sub(w.Pos, center);
            if (std::abs(d.y) > 3.5f) continue;
            d.y = 0;
            if (Len(d) <= radius + w.Radius) DamageWraith(i, damage, center, 4.f, launch, color);
        }
    }

    void NyxaraKit::StartAction(WorldState& world, Act act)
    {
        mAct = act; mActTime = 0; mNextHit = 0; mBuffered = false; mHeavyBuffered = false; mDived = false;
        auto& player = world.MainPlayer;
        // Aim: camera direction, snapped onto a shade in front when one is close.
        float yaw = world.MainCamera.Yaw;
        if (act != Act::Swap)
        {
            const XMFLOAT3 f = Forward(yaw);
            int best = -1; float bestScore = 0;
            for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
            {
                if (!mWraiths[i].Alive) continue;
                XMFLOAT3 d = Sub(mWraiths[i].Pos, player.Position); d.y = 0;
                const float dist = Len(d);
                if (dist < .3f || dist > 9.f) continue;
                const float c = (d.x * f.x + d.z * f.z) / dist;
                const float score = c - dist * .04f;
                if (c > .5f && score > bestScore) { bestScore = score; best = i; }
            }
            float targetDistance = 0;
            if (best >= 0) { XMFLOAT3 d = Sub(mWraiths[best].Pos, player.Position); d.y = 0; yaw = std::atan2(d.x, d.z); targetDistance = Len(d); }
            player.Yaw = yaw;
            // Mechanics: blades step in to reach the target, Talon Dash cuts through, leaps pick a dive point.
            const bool melee = act == Act::Slash1 || act == Act::Slash2 || act == Act::Heavy || act == Act::Combo2L;
            if (melee && best >= 0 && targetDistance > 1.8f)
                Dash((std::min)((targetDistance - 1.5f) / .16f, act == Act::Heavy ? 20.f : 14.f), .16f, yaw);
            if (act == Act::Skill1)
            {
                Dash(26.f, .34f, yaw, 26);
                Burst(Add(player.Position, {0, 1.1f, 0}), Talonshade, 18, 5.f, .22f);
            }
            if (act == Act::Skill3 || act == Act::Combo3R)
            {
                mDiveTarget = best >= 0 && targetDistance < 9.f ? mWraiths[best].Pos : Add(player.Position, Mul(Forward(yaw), 5.f));
                mDiveTarget.y = player.Position.y;
            }
        }
        mNotice = Def(act).Name; mNoticeTimer = act == Act::Slash1 || act == Act::Slash2 ? .5f : 1.2f;
        switch (act)
        {
        case Act::Slash1: case Act::Slash2: case Act::Combo1L: case Act::Combo1R: case Act::Combo2L: case Act::Combo2R: CaucasianEagle::Sound::Play("nyxara/swing", .7f); break;
        case Act::Heavy: CaucasianEagle::Sound::Play("nyxara/swing_heavy", .85f); CaucasianEagle::Sound::Play("nyxara/effort", .8f); break;
        case Act::Skill1: CaucasianEagle::Sound::Play("nyxara/dash", .85f); break;
        case Act::Skill2: case Act::Skill3: case Act::Combo3L: case Act::Combo3R: CaucasianEagle::Sound::Play("nyxara/skill", .85f); CaucasianEagle::Sound::Play("nyxara/effort", .8f); break;
        case Act::Guard: CaucasianEagle::Sound::Play("nyxara/guard", .7f); break;
        case Act::Swap: CaucasianEagle::Sound::Play("nyxara/swap", .6f); break;
        default: break;
        }
    }

    void NyxaraKit::OnHit(WorldState& world, Act act, int hit)
    {
        const auto& p = world.MainPlayer;
        const XMFLOAT3 f = Forward(p.Yaw), r = RightOf(p.Yaw);
        const XMFLOAT3 chest = Add(p.Position, {0, 1.15f, 0});
        const auto arc = [&](XMFLOAT4 color, float roll, float size, bool mirror, float life, float forward = .25f, float height = 1.15f)
        {
            Fx a; a.Kind = FxKind::Arc; a.Pos = Add(Add(p.Position, {0, height, 0}), Mul(f, forward)); a.Yaw = p.Yaw;
            a.Roll = roll * Pi / 180; a.Size = size; a.Mirror = mirror; a.Life = life; a.Color = color; mFx.push_back(a);
        };
        const auto ring = [&](XMFLOAT3 at, float size, XMFLOAT4 color, float life, int damage = 0)
        {
            Fx g; g.Kind = damage ? FxKind::RingWave : FxKind::Ring; g.Pos = {at.x, p.Position.y + .04f, at.z};
            g.Size = size; g.Color = color; g.Life = life; g.Damage = damage; mFx.push_back(g);
        };
        const auto wave = [&](XMFLOAT4 color, float roll, bool mirror, int damage)
        {
            Fx w; w.Kind = FxKind::ArcWave; w.Pos = Add(chest, Mul(f, .4f)); w.Vel = Mul(f, 17.f); w.Yaw = p.Yaw;
            w.Roll = roll * Pi / 180; w.Size = 1.9f; w.Mirror = mirror; w.Life = .62f; w.Color = color; w.Damage = damage;
            mFx.push_back(w);
        };
        const auto pillar = [&](XMFLOAT3 at, float radius, float height, float life)
        {
            Fx b; b.Kind = FxKind::Pillar; b.Pos = {at.x, p.Position.y, at.z}; b.Size = radius; b.Roll = height;
            b.Life = life; b.Color = MoonWhite; mFx.push_back(b);
            Fx s; s.Kind = FxKind::Sigil; s.Pos = {at.x, p.Position.y + .05f, at.z}; s.Size = radius * 2.6f;
            s.Life = life * 1.6f; s.Color = Talonshade; mFx.push_back(s);
        };
        const auto feathers = [&](int count, XMFLOAT4 color, int damage)
        {
            int mask = 0;
            for (int i = 0; i < count; ++i)
            {
                Fx fe; fe.Kind = FxKind::Feather; fe.Color = color; fe.Damage = damage; fe.Life = 1.6f; fe.Size = .55f;
                const float side = (static_cast<float>(i) - (count - 1) * .5f) * .55f;
                fe.Pos = Add(Add(chest, Mul(r, side)), {0, .25f * static_cast<float>(i % 2), 0});
                fe.Vel = Add(Mul(f, 9.f), Add(Mul(r, side * 4.f), {0, 3.f, 0}));
                fe.Target = NearestWraith(fe.Pos, 22.f, mask);
                if (fe.Target >= 0) mask |= 1 << fe.Target;
                if (static_cast<unsigned>(mask) == (1u << (std::min)(static_cast<int>(mWraiths.size()), 31)) - 1) mask = 0;
                mFx.push_back(fe);
            }
        };
        const XMFLOAT4 cR = RightColor(), cL = LeftColor();
        switch (act)
        {
        case Act::Slash1:
            MeleeArc(world, 2.9f, 70, 16, 0, cL); arc(cL, 22, 1.9f, true, .32f); break;   // left sword
        case Act::Slash2:
            MeleeArc(world, 2.9f, 70, 16, 0, cR); arc(cR, -28, 1.9f, false, .32f); break; // right sword
        case Act::Heavy:
            MeleeArc(world, 3.5f, 85, 34, 2.f, cR);
            arc(cR, -12, 2.7f, false, .42f); arc(cL, 38, 2.4f, true, .42f, .35f, 1.3f);
            ring(Add(p.Position, Mul(f, 1.2f)), 3.f, MoonWhite, .45f); break;
        case Act::Skill1:
            if (hit == 0) { MeleeArc(world, 2.6f, 60, 10, 0, cL); arc(cL, 30, 1.6f, true, .28f); break; }
            {
                Fx l; l.Kind = FxKind::Lance; l.Pos = Add(chest, Mul(f, .7f)); l.Vel = Mul(f, 26.f); l.Yaw = p.Yaw;
                l.Life = .72f; l.Size = 2.6f; l.Color = Talonshade; l.Damage = 38; l.Launch = 2.f; mFx.push_back(l);
                Burst(Add(chest, Mul(f, .8f)), cR, 14, 3.5f, .2f);
            }
            break;
        case Act::Skill2:
            for (auto& w : mWraiths) // the carried cyclone drags shades into her blades
                if (w.Alive && LenXZ(Sub(w.Pos, p.Position)) < 8.f) { XMFLOAT3 in = Sub(p.Position, w.Pos); in.y = 0; w.Push = Add(w.Push, Mul(Norm(in), 7.f * w.PushScale)); }
            Area(p.Position, 3.6f, 12, hit == 3 ? 3.f : 0, hit % 2 ? cL : cR);
            ring(p.Position, 3.6f, hit % 2 ? cL : cR, .4f);
            if (hit == 0)
            {
                Fx c; c.Kind = FxKind::Cyclone; c.Pos = p.Position; c.Life = 1.75f; c.Size = 2.3f; c.Color = Talonshade; mFx.push_back(c);
            }
            break;
        case Act::Skill3:
        {   // She has dived onto the target point: the judgment pillar strikes where she lands.
            const XMFLOAT3 at = p.Position;
            mShake = (std::max)(mShake, .2f);
            pillar(at, 1.15f, 16.f, 1.1f);
            ring(at, 3.4f, MoonWhite, .55f); ring(at, 2.2f, Talonshade, .7f);
            Area(at, 3.3f, 55, 5.5f, MoonWhite);
            Burst(Add(at, {0, .4f, 0}), MoonWhite, 30, 6.f, .28f);
            break;
        }
        case Act::Combo1L:
            Dash(11.f, .14f, p.Yaw); // each talon lunges forward
            wave(hit % 2 ? cL : cR, hit % 2 ? 35.f : -35.f, hit % 2 == 1, 24);
            arc(hit % 2 ? cL : cR, hit % 2 ? 35.f : -35.f, 2.0f, hit % 2 == 1, .3f);
            if (hit == 2) { wave(cL, 35, true, 24); }
            break;
        case Act::Combo1R:
            MeleeArc(world, 3.7f, 75, 48, 8.f, cR);
            arc(cR, 90, 2.6f, false, .45f, .5f, 1.4f); arc(cL, 75, 2.2f, true, .45f, .4f, 1.3f);
            for (int i = 0; i < 12; ++i)
            {
                Fx s; s.Kind = FxKind::Feather; s.Color = i % 2 ? cL : MoonWhite; s.Life = .9f; s.Size = .45f;
                const float a = static_cast<float>(i) / 12 * 2 * Pi;
                s.Pos = Add(Add(p.Position, Mul(f, 1.4f)), {0, .5f, 0});
                s.Vel = {std::sin(a) * 3.f, 7.f + 2.f * Random(), std::cos(a) * 3.f}; s.Target = -2; mFx.push_back(s);
            }
            ring(Add(p.Position, Mul(f, 1.4f)), 2.6f, MoonWhite, .5f); break;
        case Act::Combo2L:
            MeleeArc(world, 3.3f, 78, hit == 2 ? 30 : 20, hit == 2 ? 3.f : 0, hit % 2 ? cL : cR);
            arc(hit % 2 ? cL : cR, hit == 0 ? -30.f : hit == 1 ? 30.f : 0.f, hit == 2 ? 2.8f : 2.1f, hit % 2 == 1, .36f);
            if (hit == 2)
            {
                ring(p.Position, 3.4f, Talonshade, .5f);
                Fx c; c.Kind = FxKind::Cyclone; c.Pos = p.Position; c.Life = .8f; c.Size = 1.8f; c.Color = MoonWhite; mFx.push_back(c);
            }
            break;
        case Act::Combo2R:
        {   // Whirlwind Talons: hurl the cyclone forward; it travels, drags and shreds shades.
            Fx c; c.Kind = FxKind::Cyclone; c.Pos = Add(p.Position, Mul(f, 1.2f)); c.Vel = Mul(f, 8.5f); c.Life = 2.4f;
            c.Size = 2.1f; c.Color = Talonshade; c.Damage = 13; mFx.push_back(c);
            ring(p.Position, 3.5f, MoonWhite, .4f);
        }
            for (int i = 0; i < 4; ++i)
            {
                Fx a; a.Kind = FxKind::Arc; a.Pos = Add(p.Position, {0, 1.f, 0}); a.Yaw = p.Yaw + static_cast<float>(i) * Pi / 2;
                a.Size = 2.4f; a.Life = .4f; a.Color = i % 2 ? cL : cR; mFx.push_back(a);
            }
            break;
        case Act::Combo3L:
            feathers(5, hit % 2 ? cL : Talonshade, 16); // airborne feather volleys
            arc(hit % 2 ? cL : cR, hit % 2 ? 25.f : -25.f, 1.8f, hit % 2 == 1, .3f); break;
        case Act::Combo3R:
        {
            const XMFLOAT3 at = p.Position; // Moonfall: slam where the dive lands
            mShake = (std::max)(mShake, .3f); mHitStop = (std::max)(mHitStop, .12f);
            pillar(at, 2.1f, 22.f, 1.3f);
            ring(at, 6.2f, MoonWhite, .6f, 0); ring(at, 5.f, Talonshade, .75f); ring(at, 3.f, MoonWhite, .5f);
            Area(at, 6.f, 72, 6.5f, MoonWhite);
            Burst(Add(at, {0, .3f, 0}), MoonWhite, 40, 8.f, .32f); Burst(Add(at, {0, .3f, 0}), Talonshade, 30, 6.f, .28f);
            break;
        }
        case Act::Swap:
        {
            mSwapped = !mSwapped; mSwapPending = true;
            const XMFLOAT3 hands = Add(Add(p.Position, {0, 1.2f, 0}), Mul(Forward(p.Yaw), .35f));
            Burst(hands, Moonfeather, 12, 2.5f, .16f); Burst(hands, Talonshade, 12, 2.5f, .16f);
            Fx g; g.Kind = FxKind::Ring; g.Pos = {hands.x, hands.y - .1f, hands.z}; g.Size = .8f; g.Life = .35f; g.Color = MoonWhite; mFx.push_back(g);
            mNotice = mSwapped ? L"TALONSHADE RIGHT / MOONFEATHER LEFT" : L"MOONFEATHER RIGHT / TALONSHADE LEFT";
            mNoticeTimer = 1.2f;
            break;
        }
        default: break;
        }
    }

    void NyxaraKit::Update(WorldState& world, WorldRenderer& worldRenderer, CombatSystem& combat,
        float dt, float realDt, bool inputAllowed)
    {
        if (!mReady) return;
        if (GameSession().Unattended) mDemoClock += realDt;
        const Keys k = ReadKeys(inputAllowed);
        const auto pressed = [&](bool now, bool before) { return now && !before; };
        const bool lmb = pressed(k.Lmb, mPrev.Lmb), rmb = pressed(k.Rmb, mPrev.Rmb), q = pressed(k.Q, mPrev.Q),
            e = pressed(k.E, mPrev.E), k1 = pressed(k.K1, mPrev.K1), k2 = pressed(k.K2, mPrev.K2), k3 = pressed(k.K3, mPrev.K3);
        mPrev = k;

        {   // Wing-glide locomotion: travelling on the ground lifts her into a low wing-beat glide
            // (upright FlightHover pose), Shift boosts it. Presentation only; collision stays on the ground rules.
            const auto& p = world.MainPlayer;
            const bool gliding = p.TraversalMode == DoorwaysPlayerTraversalMode::Ground && p.IsActuallyMoving;
            const bool boosting = gliding && k.Shift;
            if (boosting && !mBoosting) { mNotice = L"WING BOOST"; mNoticeTimer = .6f; }
            mBoosting = boosting;
            const bool acting = mAct != Act::None && mAct != Act::Swap; // skills fight in the flying pose
            const float lift = gliding || acting ? (boosting ? .62f : .45f) : 0.f;
            mGlideLift += (lift - mGlideLift) * (std::min)(1.f, realDt * (gliding || acting ? 6.f : 4.f));
            const float air = AirProfile(mActTime * (mAct != Act::None ? Def(mAct).Rate : 0.f));
            mAir += (air - mAir) * (std::min)(1.f, dt * (air < mAir ? 22.f : 9.f)); // dives drop fast
            world.AnimatedPlayerVisual.Translation.y = .03f + mGlideLift + mAir; // .03: standing soles rest on the floor
            {   // Lean into travel and bank through turns/strafes on a soft spring (overshoots a little on stops).
                const float step = (std::min)(realDt, 1.f / 30.f);
                const float vf = p.ActualVelocity.x * std::sin(p.Yaw) + p.ActualVelocity.z * std::cos(p.Yaw);
                const float vr = p.ActualVelocity.x * std::cos(p.Yaw) - p.ActualVelocity.z * std::sin(p.Yaw);
                float turn = p.Yaw - mLeanYaw; mLeanYaw = p.Yaw;
                while (turn > Pi) turn -= 2 * Pi; while (turn < -Pi) turn += 2 * Pi;
                const float turnRate = step > 0 ? std::clamp(turn / step, -6.f, 6.f) : 0.f;
                const float airborne = mGlideLift > .05f || mAir > .05f ? 1.f : 0.f;
                const float targetPitch = airborne * .15f * std::clamp(vf / 5.f, -1.f, 1.7f);
                const float targetRoll = airborne * (-.17f * std::clamp(vr / 5.f, -1.3f, 1.3f) - .035f * turnRate);
                const auto spring = [&](float& x, float& v, float target)
                { v += ((target - x) * 70.f - v * 9.5f) * step; x += v * step; x = std::clamp(x, -.45f, .45f); };
                spring(mLeanPitch, mLeanPitchVel, targetPitch); spring(mLeanRoll, mLeanRollVel, std::clamp(targetRoll, -.3f, .3f));
                world.AnimatedPlayerVisual.Rotation.x = mLeanPitch; world.AnimatedPlayerVisual.Rotation.z = mLeanRoll;
            }
            world.ImportedAnimatedCharacter.SetGroundTravelOverride("FlightHover", boosting ? 1.8f : 1.25f); // upright wing-beat flight
            if (boosting)
            {
                mTrailTimer -= dt;
                while (mTrailTimer <= 0)
                {
                    mTrailTimer += .045f;
                    const XMFLOAT3 back = Mul(Forward(p.Yaw), -.5f);
                    const float side = (Random() - .5f) * 1.6f;
                    Fx s; s.Kind = Random() < .3f ? FxKind::Feather : FxKind::Spark;
                    s.Color = Random() < .5f ? MoonWhite : Talonshade;
                    s.Pos = Add(Add(p.Position, back), {std::cos(p.Yaw) * side, .9f + mGlideLift + Random() * .5f, -std::sin(p.Yaw) * side});
                    s.Vel = {(Random() - .5f) * .8f, s.Kind == FxKind::Feather ? 1.5f : .4f, (Random() - .5f) * .8f};
                    s.Target = -2; s.Life = .45f + .3f * Random(); s.Size = s.Kind == FxKind::Feather ? .32f : .16f;
                    mFx.push_back(s);
                }
            }
            else mTrailTimer = 0;
        }

        for (auto& c : mSkillCooldown) c = (std::max)(0.f, c - dt);
        mGuardCooldown = (std::max)(0.f, mGuardCooldown - dt);
        mSwapCooldown = (std::max)(0.f, mSwapCooldown - dt);
        mChainWindow = (std::max)(0.f, mChainWindow - dt);
        if (mAct == Act::None) mComboWindow = (std::max)(0.f, mComboWindow - dt);

        const bool alive = combat.PlayerActionsAllowed();
        auto& character = world.ImportedAnimatedCharacter;
        if (!alive)
        {
            mAct = Act::None; character.ClearPresentationClipOverride(); mDashTime = 0;
        }
        else
        {
            // The recovery of a skill or combo (everything after its last hit, once CaucasianEagle::RecoveryCancel has passed) can be
            // cut by a slash, the heavy blow, the barrier or the swap, so a long flourish never leaves her helpless.
            const auto recovered = [&]
            {
                if (mAct < Act::Skill1 || mAct > Act::Combo3R) return false;
                const auto& d = Def(mAct);
                return mNextHit >= d.HitCount && mActTime * d.Rate >= d.Hits[d.HitCount - 1] + CaucasianEagle::RecoveryCancel;
            };
            const auto canStart = [&] { return mAct == Act::None || recovered(); };
            // the barrier is her only defence on the ground: it also cuts a sword attack or the swap at any moment
            const auto canGuard = [&] { return canStart() || mAct == Act::Slash1 || mAct == Act::Slash2 || mAct == Act::Heavy || mAct == Act::Swap; };
            // Skills fire the moment they are pressed: they cancel sword attacks, the swap and the guard at any
            // point, and cut into the recovery of another skill or combo once its last hit has landed.
            // A press that cannot fire yet is remembered briefly and fires at the first opening.
            const auto canSkill = [&] { return mAct == Act::None || mAct == Act::Slash1 || mAct == Act::Slash2 || mAct == Act::Heavy ||
                mAct == Act::Swap || mAct == Act::Guard || mNextHit >= Def(mAct).HitCount; };
            const int skillKey = k1 ? 0 : k2 ? 1 : k3 ? 2 : -1;
            if (skillKey >= 0) { mSkillBuffer = skillKey; mSkillBufferTimer = CaucasianEagle::SkillBufferSeconds; }
            else if (mSkillBufferTimer > 0 && (mSkillBufferTimer -= realDt) <= 0) mSkillBuffer = -1;
            const auto skillPressed = mSkillBuffer;
            // A combo follows its skill once the skill's last hit has landed (never before: the skill keeps every hit),
            // and until CaucasianEagle::ComboWindowSeconds after the skill ends. A click made earlier in the skill is remembered.
            const bool inSkill = mAct >= Act::Skill1 && mAct <= Act::Skill3;
            const bool comboOpen = mComboSkill > 0 && (mComboWindow > 0 || (inSkill && mNextHit >= Def(mAct).HitCount));
            if (inSkill && mComboSkill > 0 && !comboOpen && (lmb || rmb) && mNextHit >= 1) { mComboBuffer = rmb ? 2 : 1; mComboBufferTimer = CaucasianEagle::ComboBufferSeconds; }
            else if (mComboBufferTimer > 0 && (mComboBufferTimer -= realDt) <= 0) mComboBuffer = 0;
            if (!inSkill && mComboWindow <= 0) mComboBuffer = 0;
            if ((lmb || rmb || mComboBuffer != 0) && comboOpen)
            {
                static const Act combos[3][2] = {{Act::Combo1L, Act::Combo1R}, {Act::Combo2L, Act::Combo2R}, {Act::Combo3L, Act::Combo3R}};
                const bool right = lmb || rmb ? rmb : mComboBuffer == 2;
                const Act combo = combos[mComboSkill - 1][right ? 1 : 0];
                mComboSkill = 0; mComboWindow = 0; mComboBuffer = 0; mComboBufferTimer = 0;
                StartAction(world, combo);
            }
            else if (skillPressed >= 0 && canSkill() && mSkillCooldown[skillPressed] <= 0)
            {
                mSkillBuffer = -1; mSkillBufferTimer = 0; mDashTime = 0;
                mSkillCooldown[skillPressed] = SkillCooldown[skillPressed] * mCooldownScale;
                mComboSkill = skillPressed + 1;
                StartAction(world, static_cast<Act>(static_cast<int>(Act::Skill1) + skillPressed));
            }
            else if (skillKey >= 0 && mSkillCooldown[skillKey] > 0)
            {
                mSkillBuffer = -1; mSkillBufferTimer = 0;
                mNotice = L"SKILL RECOVERING"; mNoticeTimer = .5f;
            }
            else if (e && canGuard() && mGuardCooldown <= 0)
            {
                mGuardHeld = 0; StartAction(world, Act::Guard);
            }
            else if (q && canStart() && mSwapCooldown <= 0)
            {
                mSwapCooldown = .4f; StartAction(world, Act::Swap);
            }
            else if (rmb && canStart())
            {
                StartAction(world, Act::Heavy);
            }
            else if (lmb)
            {
                if (canStart())
                {
                    StartAction(world, mChainWindow > 0 && mSlashChain == 0 ? Act::Slash2 : Act::Slash1);
                    mSlashChain = mAct == Act::Slash1 ? 0 : 1;
                }
                else if (mAct == Act::Slash1 || mAct == Act::Slash2 || mAct == Act::Heavy) mBuffered = true;
            }
            else if (rmb && (mAct == Act::Slash1 || mAct == Act::Slash2)) mHeavyBuffered = true;   // slash, slash, heavy
        }

        if (mAct != Act::None)
        {
            const auto& def = Def(mAct);
            mActTime += dt;
            float clipTime = mActTime * def.Rate;
            if (mAct == Act::Guard)
            {
                mGuardHeld += dt;
                if (k.E && mGuardHeld < 5.f) clipTime = (std::min)(clipTime, GuardHoldClip);
                else if (clipTime < GuardHoldClip) clipTime = GuardHoldClip, mActTime = GuardHoldClip / def.Rate;
            }
            while (mNextHit < def.HitCount && clipTime >= def.Hits[mNextHit]) OnHit(world, mAct, mNextHit++);
            if (mSwapPending) { ApplySwordVisibility(worldRenderer); mSwapPending = false; }
            const bool slash = mAct == Act::Slash1 || mAct == Act::Slash2;
            if (slash && mHeavyBuffered && clipTime >= def.Hits[0] + CaucasianEagle::SlashChainAfterHit) StartAction(world, Act::Heavy);
            else if (slash && mBuffered && clipTime >= def.Hits[0] + CaucasianEagle::SlashChainAfterHit)
            {
                const Act next = mAct == Act::Slash1 ? Act::Slash2 : Act::Slash1;
                StartAction(world, next); mSlashChain = next == Act::Slash1 ? 0 : 1;
            }
            else if (mAct == Act::Heavy && mBuffered && clipTime >= def.Hits[0] + CaucasianEagle::HeavyChainAfterHit)
            {
                StartAction(world, Act::Slash1); mSlashChain = 0;
            }
            else if (clipTime >= def.End)
            {
                if (slash) { mChainWindow = CaucasianEagle::SlashChainSeconds; } // LMB alternates left, right, left ... while clicks keep coming
                if (mAct >= Act::Skill1 && mAct <= Act::Skill3) mComboWindow = CaucasianEagle::ComboWindowSeconds;
                if (mAct == Act::Guard) mGuardCooldown = CaucasianEagle::GuardCooldownSeconds;
                mAct = Act::None;
            }
            if (mAct != Act::None)
                character.SetPresentationClipOverride(Def(mAct).Clip, (std::min)(mActTime * Def(mAct).Rate,
                    mAct == Act::Guard && k.E && mGuardHeld < 5.f ? GuardHoldClip : 99.f));
            else character.ClearPresentationClipOverride();
        }
        GameSession().HoldFacing = mAct != Act::None && mAct != Act::Swap; // WASD keeps moving; legs layer under the attack

        // Leap skills dive onto their target point just before the strike lands.
        if ((mAct == Act::Skill3 || mAct == Act::Combo3R) && !mDived && mActTime * Def(mAct).Rate >= Def(mAct).Hits[0] - .2f)
        {
            mDived = true;
            XMFLOAT3 d = Sub(mDiveTarget, world.MainPlayer.Position); d.y = 0;
            const float distance = Len(d);
            if (distance > .8f) { world.MainPlayer.Yaw = std::atan2(d.x, d.z); Dash((distance - .6f) / .18f, .18f, world.MainPlayer.Yaw); }
        }
        // Skill dash: collision-resolved by the movement controller; a damaging dash cuts every shade it passes.
        if (mDashTime > 0)
        {
            mDashTime -= dt;
            GameSession().ImpulseX = std::sin(mDashYaw) * mDashSpeed;
            GameSession().ImpulseZ = std::cos(mDashYaw) * mDashSpeed;
            const auto& p = world.MainPlayer;
            if (mDashDamage > 0)
                for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                    if (mWraiths[i].Alive && !(mDashMask & (1u << i)) && LenXZ(Sub(mWraiths[i].Pos, p.Position)) < 1.45f + mWraiths[i].Radius)
                    { mDashMask |= 1u << i; DamageWraith(i, mDashDamage, p.Position, 9.f, 2.f, Talonshade); }
            for (int i = 0; i < 3; ++i)
            {
                Fx s; s.Kind = FxKind::Spark; s.Color = Random() < .5f ? Talonshade : MoonWhite;
                s.Pos = Add(p.Position, {(Random() - .5f) * .6f, .6f + Random() * 1.2f + mGlideLift + mAir, (Random() - .5f) * .6f});
                s.Vel = {0, .3f, 0}; s.Life = .3f; s.Size = .2f; mFx.push_back(s);
            }
        }
        else GameSession().ImpulseX = GameSession().ImpulseZ = 0;
        mShake *= std::exp(-8.f * realDt); if (mShake < .003f) mShake = 0;
        GameSession().CameraShake = mShake;

        // Barrier swords rise while guarding.
        const bool guarding = mAct == Act::Guard;
        mBarrier = std::clamp(mBarrier + (guarding ? 4.5f : -3.f) * dt, 0.f, 1.f);
        mBlockFlash = (std::max)(0.f, mBlockFlash - dt);

        UpdateWraiths(world, combat, dt);
        UpdateEffects(world, dt);
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
                Burst(o.Pos, MoonWhite, 14, 4.f, .2f); Burst(o.Pos, Talonshade, 8, 3.f, .18f);
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
        combat.ApplyIncomingDamage(world, damage);
        CaucasianEagle::Sound::Play("combat/blow", .9f); CaucasianEagle::Sound::Play("nyxara/hurt", .9f);
        return true;
    }

    void NyxaraKit::UpdateEffects(WorldState& world, float dt)
    {
        mMercy = (std::max)(0.f, mMercy - dt);
        const auto& player = world.MainPlayer;
        for (auto& f : mFx)
        {
            f.T += dt;
            switch (f.Kind)
            {
            case FxKind::Spark:
                f.Pos = Add(f.Pos, Mul(f.Vel, dt)); f.Vel.y -= 4.f * dt; f.Vel = Mul(f.Vel, std::exp(-1.5f * dt)); break;
            case FxKind::ArcWave:
            case FxKind::Lance:
                f.Pos = Add(f.Pos, Mul(f.Vel, dt));
                for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                {
                    if (!mWraiths[i].Alive || (f.HitMask & (1u << i))) continue;
                    const XMFLOAT3 d = Sub(mWraiths[i].Pos, f.Pos);
                    if (LenXZ(d) < (f.Kind == FxKind::Lance ? 1.1f : f.Size * .75f) + mWraiths[i].Radius - .55f && std::abs(d.y) < 2.f)
                    {
                        f.HitMask |= 1u << i; DamageWraith(i, f.Damage, Sub(f.Pos, f.Vel), 4.f, f.Launch, f.Color);
                    }
                }
                if (f.Kind == FxKind::Lance && Random() < .9f)
                {
                    Fx s; s.Kind = FxKind::Spark; s.Pos = f.Pos; s.Color = Random() < .5f ? MoonWhite : Talonshade;
                    s.Vel = {Random() - .5f, Random() * .8f, Random() - .5f}; s.Life = .35f; s.Size = .14f; mFx.push_back(s);
                }
                break;
            case FxKind::RingWave:
            {
                const float radius = f.Size * Smooth(0, 1, f.T / (f.Life * .8f));
                for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                {
                    if (!mWraiths[i].Alive || (f.HitMask & (1u << i))) continue;
                    const float d = LenXZ(Sub(mWraiths[i].Pos, f.Pos));
                    if (d < radius + mWraiths[i].Radius) { f.HitMask |= 1u << i; DamageWraith(i, f.Damage, f.Pos, 6.f, 3.f, f.Color); }
                }
                break;
            }
            case FxKind::Feather:
                if (f.Target == -2) { f.Pos = Add(f.Pos, Mul(f.Vel, dt)); f.Vel.y -= 9.f * dt; break; }
                if (f.Target >= 0 && f.Target < static_cast<int>(mWraiths.size()) && mWraiths[f.Target].Alive)
                {
                    const XMFLOAT3 want = Mul(Norm(Sub(mWraiths[f.Target].Pos, f.Pos)), 15.f);
                    const float t = (std::min)(1.f, 5.f * dt);
                    f.Vel = Add(Mul(f.Vel, 1 - t), Mul(want, t));
                    if (Len(Sub(mWraiths[f.Target].Pos, f.Pos)) < .15f + mWraiths[f.Target].Radius)
                    {
                        DamageWraith(f.Target, f.Damage, Sub(f.Pos, f.Vel), 2.5f, 0, f.Color);
                        f.Life = 0;
                    }
                }
                f.Pos = Add(f.Pos, Mul(f.Vel, dt));
                break;
            case FxKind::Cyclone:
            {   // Carried (Damage 0) or hurled (Damage > 0); both drag nearby shades inwards.
                if (f.Damage > 0) f.Pos = Add(f.Pos, Mul(f.Vel, dt)); else f.Pos = player.Position;
                f.Tick -= dt;
                const bool tick = f.Damage > 0 && f.Tick <= 0;
                if (tick) f.Tick = .28f;
                for (int i = 0; i < static_cast<int>(mWraiths.size()); ++i)
                {
                    auto& w = mWraiths[i];
                    if (!w.Alive) continue;
                    XMFLOAT3 in = Sub(f.Pos, w.Pos); in.y = 0;
                    const float d = Len(in);
                    if (d < 6.f && d > .8f) w.Pos = Add(w.Pos, Mul(Norm(in), (f.Damage > 0 ? 5.f : 2.5f) * dt * w.PushScale));
                    if (tick && d < f.Size + w.Radius) DamageWraith(i, f.Damage, f.Pos, 0, 1.2f, f.Color);
                }
                break;
            }
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
            return mSkillCooldown[i] <= 0 ? std::wstring(L"READY") :
                std::to_wstring(static_cast<int>(std::ceil(mSkillCooldown[i]))) + L"s";
        };
        hud.ActionStatus = std::wstring(L"NYXARA  R: ") + (mSwapped ? L"TALONSHADE" : L"MOONFEATHER") +
            L"  L: " + (mSwapped ? L"MOONFEATHER" : L"TALONSHADE") +
            L"  |  F DASH " + skill(0) + L"  V CYCLONE " + skill(1) + L"  C MOON LEAP " + skill(2) +
            (mBoosting ? L"  |  WING BOOST" : L"");
        hud.Controls = L"LMB/RMB SLASH  1-2-3 SKILLS (+LMB/RMB COMBO)  Q SWAP  E BARRIER  SHIFT BOOST  SPACE JUMP / HOLD FLY";
        if (mNoticeTimer > 0) { hud.ActionNotice = mNotice; hud.ActionNoticeTimer = mNoticeTimer; }

        // Graphical HUD snapshot (HudRenderer::DrawNyxaraHud).
        auto& h = NyxaraHud();
        h.Valid = true;
        for (int i = 0; i < 3; ++i) { h.Cooldown[i] = mSkillCooldown[i]; h.CooldownMax[i] = SkillCooldown[i] * mCooldownScale; }
        static const Act combos[3][2] = {{Act::Combo1L, Act::Combo1R}, {Act::Combo2L, Act::Combo2R}, {Act::Combo3L, Act::Combo3R}};
        // shown from the skill's first hit (a click is remembered from then on) until the window closes
        h.ComboOpen = mComboSkill > 0 && (mComboWindow > 0 || (mAct >= Act::Skill1 && mAct <= Act::Skill3 && mNextHit >= 1));
        h.ComboWindow = mAct == Act::None ? mComboWindow / CaucasianEagle::ComboWindowSeconds : 1.f;
        h.ComboLeft = h.ComboOpen ? Def(combos[mComboSkill - 1][0]).Name : L"";
        h.ComboRight = h.ComboOpen ? Def(combos[mComboSkill - 1][1]).Name : L"";
        h.Action = mAct != Act::None ? Def(mAct).Name : L"";
        h.Guarding = mAct == Act::Guard; h.Boosting = mBoosting; h.Swapped = mSwapped;
        h.Slashing = mAct == Act::Slash1 || mAct == Act::Slash2; h.Heavy = mAct == Act::Heavy; h.Swapping = mAct == Act::Swap;
        h.Notice = mNotice.c_str(); h.NoticeTimer = mNoticeTimer;
    }

    void NyxaraKit::Draw(Renderer& renderer, const WorldState& world, const XMMATRIX& viewProjection)
    {
        if (!mReady) return;
        {   // Project shade health bars for the HUD.
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
        const auto streak = [&](const DoorwaysGpuMesh& mesh, XMFLOAT3 at, XMFLOAT3 dir, float length, float width,
            XMFLOAT4 color, ID3D11ShaderResourceView* tex)
        {   // card whose +Y follows dir and faces the camera around that axis
            const XMFLOAT3 y = Norm(dir);
            XMFLOAT3 x = Cross(y, Norm(Sub(eye, at))); x = Len(x) < 1e-3f ? XMFLOAT3{1, 0, 0} : Norm(x);
            const XMFLOAT3 z = Cross(x, y);
            translucent(mesh, Basis(Mul(x, width), Mul(y, length), z, at), material(color, tex, 1, true));
        };

        // Shade wraiths (opaque core) and their bars.
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
            for (int i = 0; i < 3; ++i)
            {
                const float a = w.Phase * 1.6f + static_cast<float>(i) * 2.094f;
                renderer.DrawDoorwaysGpuMesh(mConeMesh, XMMatrixScaling(.07f, .22f, .07f) * XMMatrixRotationZ(a) *
                    XMMatrixTranslation(std::sin(a) * .75f, .1f * std::sin(a * 2), std::cos(a) * .75f) *
                    XMMatrixTranslation(p.x, p.y, p.z), viewProjection, material({.12f, .06f, .2f, 1}, nullptr, .6f, false));
            }
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

        // Barrier: two spectral swords rise crossed in front of Nyxara, with a moonlit dome.
        if (mBarrier > 0)
        {
            const auto& p = world.MainPlayer;
            const XMFLOAT3 f = Forward(p.Yaw);
            const float rise = Smooth(0, 1, mBarrier), height = 2.7f;
            const float pulse = .75f + .25f * std::sin(world.SceneTime * 9.f) + mBlockFlash * 2.f;
            for (const float side : {-1.f, 1.f})
            {
                const XMFLOAT4 color = side > 0 ? RightColor() : LeftColor();
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
                material(Alpha(Talonshade, .55f * rise), mRingTex.Get(), 1, true));
        }

        for (const auto& f : mFx)
        {
            const float t = f.T / f.Life, fade = 1 - Smooth(.55f, 1.f, t);
            switch (f.Kind)
            {
            case FxKind::Arc:
            case FxKind::ArcWave:
            {
                const float grow = f.Kind == FxKind::Arc ? .85f + .35f * Smooth(0, .5f, t) : 1.f;
                const float s = f.Size * grow;
                const XMMATRIX m = XMMatrixScaling(f.Mirror ? -s : s, 1, s) * XMMatrixRotationZ(f.Roll) * XMMatrixRotationY(f.Yaw) *
                    XMMatrixTranslation(f.Pos.x, f.Pos.y, f.Pos.z);
                translucent(mArcMesh, m, material(Alpha(f.Color, .95f * fade), mArcTex.Get(), 1, true));
                translucent(mArcMesh, XMMatrixScaling(f.Mirror ? -s * 1.08f : s * 1.08f, 1, s * 1.08f) * XMMatrixRotationZ(f.Roll) *
                    XMMatrixRotationY(f.Yaw) * XMMatrixTranslation(f.Pos.x, f.Pos.y, f.Pos.z), material(Alpha(MoonWhite, .35f * fade), mArcTex.Get(), 1, true));
                break;
            }
            case FxKind::Lance:
            {
                const XMFLOAT3 dir = Norm(f.Vel);
                for (int i = 0; i < 3; ++i)
                {
                    XMFLOAT3 side = Cross(dir, {0, 1, 0}); side = Norm(side);
                    const float a = static_cast<float>(i) * Pi / 3;
                    const XMFLOAT3 x = Add(Mul(side, std::cos(a)), Mul(XMFLOAT3{0, 1, 0}, std::sin(a)));
                    const XMFLOAT3 tail = Sub(f.Pos, Mul(dir, f.Size));
                    translucent(mFeatherMesh, Basis(Mul(x, .75f), Mul(dir, f.Size), Cross(x, dir), tail),
                        material(Alpha(i ? MoonWhite : f.Color, .8f * fade), mFeatherTex.Get(), 1, true));
                }
                billboard(f.Pos, 1.3f, Alpha(f.Color, .9f * fade), mGlowTex.Get());
                streak(mFeatherMesh, f.Pos, Mul(f.Vel, -1), 3.5f, .5f, Alpha(f.Color, .5f * fade), mGlowTex.Get());
                break;
            }
            case FxKind::Ring:
            case FxKind::RingWave:
            {
                const float radius = f.Size * (.2f + .8f * Smooth(0, 1, t / .8f));
                translucent(mRingMesh, XMMatrixScaling(radius, 1, radius) * XMMatrixTranslation(f.Pos.x, f.Pos.y, f.Pos.z),
                    material(Alpha(f.Color, .9f * fade), mRingTex.Get(), 1, true));
                if (f.Kind == FxKind::RingWave)
                    translucent(mCylinderMesh, XMMatrixScaling(radius, .9f * fade, radius) * XMMatrixTranslation(f.Pos.x, f.Pos.y, f.Pos.z),
                        material(Alpha(f.Color, .35f * fade), mBeamTex.Get(), 1, true));
                break;
            }
            case FxKind::Pillar:
            {
                const float open = Smooth(0, .12f, t), r = f.Size * (.4f + .6f * open) * (1 - .5f * Smooth(.6f, 1, t));
                translucent(mCylinderMesh, XMMatrixScaling(r, f.Roll, r) * XMMatrixTranslation(f.Pos.x, f.Pos.y, f.Pos.z),
                    material(Alpha(MoonWhite, .5f * fade), mBeamTex.Get(), 1, true));
                translucent(mCylinderMesh, XMMatrixScaling(r * .45f, f.Roll, r * .45f) * XMMatrixTranslation(f.Pos.x, f.Pos.y, f.Pos.z),
                    material(Alpha({1, 1, 1, 1}, .7f * fade), mBeamTex.Get(), 1, true));
                translucent(mCylinderMesh, XMMatrixScaling(r * 1.6f, f.Roll * .4f, r * 1.6f) * XMMatrixTranslation(f.Pos.x, f.Pos.y, f.Pos.z),
                    material(Alpha(Talonshade, .3f * fade), mBeamTex.Get(), 1, true));
                break;
            }
            case FxKind::Sigil:
                translucent(mDiscMesh, XMMatrixScaling(f.Size, 1, f.Size) * XMMatrixRotationY(f.T * .9f) *
                    XMMatrixTranslation(f.Pos.x, f.Pos.y, f.Pos.z), material(Alpha(f.Color, .85f * fade), mSigilTex.Get(), 1, true));
                break;
            case FxKind::Feather:
                streak(mFeatherMesh, Sub(f.Pos, Mul(Norm(f.Vel), f.Size)), f.Vel, f.Size * 1.6f, f.Size * .6f,
                    Alpha(f.Color, .9f * fade), mFeatherTex.Get());
                billboard(f.Pos, .45f, Alpha(f.Color, .6f * fade), mGlowTex.Get());
                break;
            case FxKind::Spark:
                billboard(f.Pos, f.Size * (1 - .5f * t), Alpha(f.Color, fade), mGlowTex.Get());
                break;
            case FxKind::Cyclone:
                for (int i = 0; i < 12; ++i)
                {
                    const float a = static_cast<float>(i) / 12 * 2 * Pi + f.T * 9.f;
                    const float rad = f.Size * (.7f + .3f * std::sin(f.T * 3 + static_cast<float>(i)));
                    const XMFLOAT3 at = {f.Pos.x + std::sin(a) * rad, f.Pos.y + .35f + .9f * static_cast<float>(i % 3) * .5f + f.T * .4f,
                        f.Pos.z + std::cos(a) * rad};
                    streak(mFeatherMesh, at, {std::cos(a), .15f, -std::sin(a)}, .8f, .32f,
                        Alpha(i % 2 ? f.Color : MoonWhite, .85f * fade), mFeatherTex.Get());
                }
                translucent(mRingMesh, XMMatrixScaling(f.Size * 1.2f, 1, f.Size * 1.2f) * XMMatrixRotationY(f.T * 6) *
                    XMMatrixTranslation(f.Pos.x, f.Pos.y + .05f, f.Pos.z), material(Alpha(f.Color, .6f * fade), mRingTex.Get(), 1, true));
                break;
            }
        }

        std::stable_sort(list.begin(), list.end(), [](const DrawItem& a, const DrawItem& b) { return a.Depth > b.Depth; });
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
