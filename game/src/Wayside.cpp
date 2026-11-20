#include "Wayside.h"

#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRuntimeScene.h"
#include "DoorwaysWalkableSurfaceQuery.h"
#include "EaglesAscent.h"
#include "GameConstants.h"
#include "Renderer.h"
#include "WorldState.h"

#include <algorithm>
#include <cmath>
#include <cwctype>

namespace CaucasianEagle
{
    using namespace Doorways;
    using namespace DirectX;

    namespace
    {
        constexpr float JarStepLeast = 30.f, JarStepMore = 20.f;   // a group every 30 to 50 m along a road (the owner)
        constexpr float JarSeen = 150.f;                            // m: jars are drawn this near
        constexpr float PlaceReach = 2.4f;                          // m: she finds a place standing this near its cairn
        constexpr float ShardSeconds = 9.f;

        // the same number for the same words: what is in a jar, how it stands, are the same every time
        float Hash(const std::string& s, unsigned salt)
        {
            unsigned h = 2166136261u ^ (salt * 16777619u);
            for (const char c : s) { h ^= static_cast<unsigned char>(c); h *= 16777619u; }
            h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
            return static_cast<float>(h >> 8) / 16777216.f;
        }

        struct Builder
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
            // a turned shape: (height, radius) from its foot up, round the Y axis
            void Lathe(const std::vector<XMFLOAT2>& profile, int sides)
            {
                const u32 first = static_cast<u32>(Mesh.Vertices.size());
                const size_t n = profile.size();
                for (size_t i = 0; i < n; ++i)
                {
                    const XMFLOAT2 a = profile[i > 0 ? i - 1 : i], b = profile[i + 1 < n ? i + 1 : i];
                    float nr = b.x - a.x, ny = -(b.y - a.y);   // across the profile's way: (dy, -dr)
                    const float l = (std::max)(1e-5f, std::sqrt(nr * nr + ny * ny)); nr /= l; ny /= l;
                    for (int s = 0; s <= sides; ++s)
                    {
                        const float t = static_cast<float>(s) / static_cast<float>(sides), ang = t * XM_2PI;
                        Vertex({std::sin(ang) * profile[i].y, profile[i].x, std::cos(ang) * profile[i].y}, {std::sin(ang) * nr, ny, std::cos(ang) * nr}, t,
                               static_cast<float>(i) / static_cast<float>(n - 1));
                    }
                }
                const u32 row = static_cast<u32>(sides + 1);
                for (u32 i = 0; i + 1 < n; ++i)
                    for (u32 s = 0; s < static_cast<u32>(sides); ++s)
                        Quad(first + i * row + s, first + i * row + s + 1, first + (i + 1) * row + s + 1, first + (i + 1) * row + s);
            }
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

        // The four shapes (height, radius), a metre or so tall before each jar's own size:
        //   qvevri  the great egg-shaped wine vessel, narrowing to a point at its foot, a short neck and a lip
        //   doki    the jug: a round belly and a tall neck
        //   kotso   the wide cooking pot
        //   kvibri  the small round jar
        const std::vector<XMFLOAT2> Shapes[4] = {
            {{0, 0}, {0, .07f}, {.08f, .17f}, {.25f, .29f}, {.45f, .35f}, {.62f, .35f}, {.80f, .29f}, {.93f, .19f}, {1.0f, .15f}, {1.05f, .15f}, {1.08f, .18f}, {1.10f, .17f}},
            {{0, 0}, {0, .15f}, {.04f, .19f}, {.18f, .26f}, {.34f, .28f}, {.48f, .23f}, {.58f, .14f}, {.70f, .10f}, {.84f, .10f}, {.90f, .12f}, {.92f, .11f}},
            {{0, 0}, {0, .20f}, {.06f, .27f}, {.18f, .34f}, {.32f, .35f}, {.44f, .31f}, {.52f, .27f}, {.56f, .29f}, {.58f, .28f}},
            {{0, 0}, {0, .12f}, {.06f, .19f}, {.18f, .25f}, {.30f, .25f}, {.42f, .19f}, {.50f, .12f}, {.55f, .11f}, {.58f, .13f}, {.60f, .12f}}};
        // the dark bands painted round them (heights)
        const float Bands[4][2] = {{.52f, .80f}, {.30f, .62f}, {.40f, .22f}, {.20f, .38f}};

        float RadiusAt(const std::vector<XMFLOAT2>& profile, float y)
        {
            for (size_t i = 1; i < profile.size(); ++i)
                if (profile[i].x >= y && profile[i].x > profile[i - 1].x)
                    return profile[i - 1].y + (profile[i].y - profile[i - 1].y) * (y - profile[i - 1].x) / (profile[i].x - profile[i - 1].x);
            return profile.back().y;
        }

        Material Surface(ID3D11ShaderResourceView* texture, XMFLOAT4 tint, float emission, bool blend)
        {
            Material m{};
            m.Diffuse = tint; m.DiffuseMap = texture; m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .85f;
            m.SpecularColor = {0, 0, 0, 0}; m.EmissiveStrength = emission; m.DoubleSided = true; m.ReceiveFog = true;
            if (blend) m.AlphaMode = DoorwaysMaterialAlphaMode::Blend;
            return m;
        }
    }

    bool Wayside::Load(Renderer& renderer, std::string& error)
    {
        if (mMeshes) return true;
        for (int k = 0; k < 4; ++k)
        {
            Builder jar; jar.Lathe(Shapes[k], 22);
            if (!jar.Upload(renderer, mShapes[k], error)) return false;
            Builder band;
            for (const float y : Bands[k])
            {
                const float r = RadiusAt(Shapes[k], y);
                band.Lathe({{y - .025f, r + .004f}, {y - .012f, r + .014f}, {y + .012f, r + .014f}, {y + .025f, r + .004f}}, 22);
            }
            if (!band.Upload(renderer, mBands[k], error)) return false;
        }
        Builder quad;
        quad.Quad(quad.Vertex({-.5f, -.5f, 0}, {0, 0, 1}, 0, 1), quad.Vertex({.5f, -.5f, 0}, {0, 0, 1}, 1, 1),
                  quad.Vertex({.5f, .5f, 0}, {0, 0, 1}, 1, 0), quad.Vertex({-.5f, .5f, 0}, {0, 0, 1}, 0, 0));
        if (!quad.Upload(renderer, mQuad, error)) return false;
        mMeshes = true;
        return true;
    }

    void Wayside::Lay(const DoorwaysRuntimeScene& scene, const EaglesAscent& map)
    {
        mJars.clear(); mPlaces.clear(); mShards.clear();
        // the ground there, near y: walkable, and level enough for a jar to stand
        const auto ground = [&](float x, float z, float y, float& out) -> bool
        {
            const auto hit = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(scene, x, z, y - 1.6f, y + 1.6f);
            if (!hit.Hit || hit.Normal.y < .8f) return false;
            out = hit.GroundY;
            return true;
        };
        const auto nearNest = [&](const XMFLOAT3& p)
        {
            for (const auto& n : map.Nests)
                if (std::hypot(p.x - n.P.x, p.z - n.P.z) < 9 && std::abs(p.y - n.P.y) < 6) return true;
            return false;
        };
        // One road: a group of 3 to 5 every 30 to 50 m, beside it (first on one side, else the other; nearer or further)
        const auto road = [&](const std::vector<XMFLOAT3>& line, const std::vector<float>& halves, const std::string& tag, bool cave, bool trail)
        {
            if (line.size() < 2) return;
            float walked = 0, next = 15 + 15 * Hash(tag, 1);
            for (size_t i = 1; i < line.size(); ++i)
            {
                const XMFLOAT3 a = line[i - 1], b = line[i];
                const float dx = b.x - a.x, dz = b.z - a.z, run = std::sqrt(dx * dx + dz * dz), seg = std::sqrt(run * run + (b.y - a.y) * (b.y - a.y));
                while (next <= walked + seg && run > 1e-3f)
                {
                    const float t = (next - walked) / (std::max)(seg, 1e-3f);
                    const XMFLOAT3 at{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
                    const float here = next;
                    const std::string id = tag + "." + std::to_string(static_cast<int>(here));
                    next += JarStepLeast + JarStepMore * Hash(id, 2);
                    bool fly = false;
                    if (trail)
                        for (const auto& f : map.FlyStretches)
                            if (here >= f.From - 10 && here <= f.To + 10) fly = true;   // (a stretch only wings cross: no road to stand on)
                    if (fly || nearNest(at)) continue;
                    const float half = halves.empty() ? 0.f : halves[(std::min)(i, halves.size() - 1)];
                    const float rx = dz / run, rz = -dx / run;   // her right, going up the road
                    const float yaw = std::atan2(dx, dz);
                    const int count = 3 + static_cast<int>(Hash(id, 3) * 2.999f);
                    const float first = Hash(id, 4) < .5f ? 1.f : -1.f;
                    const float aside[] = {cave ? (std::max)(.7f, half - .75f) : 3.4f, cave ? (std::max)(.6f, half - 1.1f) : 4.6f, cave ? (std::max)(.6f, half - .5f) : 2.6f};
                    std::vector<Jar> group;
                    for (int side = 0; side < 2 && group.size() < 3; ++side)
                        for (const float off : aside)
                        {
                            const float sgn = side == 0 ? first : -first;
                            const XMFLOAT3 middle{at.x + rx * off * sgn, at.y, at.z + rz * off * sgn};
                            group.clear();
                            for (int k = 0; k < count; ++k)
                            {
                                Jar j;
                                j.Id = id + "." + std::to_string(k);
                                // round the group's middle: one in the middle, the others about it, along the road more than across
                                const float ang = XM_2PI * (static_cast<float>(k) / static_cast<float>(count) + .2f * Hash(j.Id, 5));
                                const float r = k == 0 ? 0.f : .75f + .35f * Hash(j.Id, 6);
                                const float along = std::cos(ang) * r * 1.2f, across = std::sin(ang) * r * .55f;
                                j.P = {middle.x + dx / run * along + rx * sgn * across, middle.y, middle.z + dz / run * along + rz * sgn * across};
                                float y = 0;
                                if (!ground(j.P.x, j.P.z, middle.y, y))
                                {
                                    if (!cave) continue;
                                    y = middle.y;
                                }
                                j.P.y = y;
                                const float shape = Hash(j.Id, 7);
                                j.Shape = k == 0 ? (shape < .6f ? 0 : 1) : shape < .3f ? 1 : shape < .6f ? 2 : shape < .85f ? 3 : 0;
                                j.Size = 1.3f * (j.Shape == 0 ? .9f : 1.f) * (.85f + .3f * Hash(j.Id, 8)) * (k == 0 ? 1.1f : 1.f);
                                j.Yaw = yaw + XM_2PI * Hash(j.Id, 9);
                                j.Tint = Hash(j.Id, 10);
                                // what is inside (found out only by breaking it): a crystal or two in three of ten, a potion in
                                // one of twelve, the rest empty (all the jars of the mountain: about the crystals the creatures leave)
                                const float inside = Hash(j.Id, 11);
                                if (inside < .08f) j.Potion = true;
                                else if (inside < .38f) j.Crystals = Hash(j.Id, 12) < .7f ? 1 : 2;
                                group.push_back(j);
                            }
                            if (group.size() >= 3) break;
                        }
                    if (group.size() >= 3) mJars.insert(mJars.end(), group.begin(), group.end());
                }
                walked += seg;
            }
        };
        road(map.Trail, {}, "t", false, true);
        for (size_t c = 0; c < map.Caves().size(); ++c)
        {
            std::vector<XMFLOAT3> line; std::vector<float> halves;
            for (const auto& p : map.Caves()[c].Line) { line.push_back({p.X, p.Y, p.Z}); halves.push_back(p.Half); }
            road(line, halves, "c" + std::to_string(c), true, false);
        }
        for (size_t r = 0; r < map.Rings().size(); ++r) road(map.Rings()[r], {}, "r" + std::to_string(r), false, false);

        // the hidden places: a cairn in each long cave's heart (beside its way), and one off the way at each named place
        for (size_t c = 0; c < map.Caves().size(); ++c)
        {
            const auto& line = map.Caves()[c].Line;
            float length = 0;
            for (size_t i = 1; i < line.size(); ++i) length += std::hypot(line[i].X - line[i - 1].X, line[i].Z - line[i - 1].Z);
            if (line.size() < 3 || length < 40) continue;
            const size_t m = line.size() / 2;
            const float dx = line[m + 1 < line.size() ? m + 1 : m].X - line[m - 1].X, dz = line[m + 1 < line.size() ? m + 1 : m].Z - line[m - 1].Z;
            const float run = (std::max)(1e-3f, std::sqrt(dx * dx + dz * dz)), off = (std::max)(.5f, line[m].Half - .7f);
            Place p; p.Id = "cave" + std::to_string(c);
            p.Name = std::wstring(L"THE HEART OF ") + (map.Caves()[c].Kind == "tunnel" ? L"THE TUNNEL" : L"THE CAVE");
            p.P = {line[m].X - dz / run * off, line[m].Y, line[m].Z + dx / run * off};
            float y = 0;
            const auto hit = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(scene, p.P.x, p.P.z, p.P.y - 1.6f, p.P.y + 1.6f);
            if (hit.Hit) y = hit.GroundY; else y = p.P.y;
            p.P.y = y;
            mPlaces.push_back(p);
        }
        for (size_t i = 0; i < map.Moments.size(); ++i)
        {
            const auto& m = map.Moments[i];
            Place p; p.Id = "moment" + std::to_string(i);
            std::wstring name(m.Name.begin(), m.Name.end());
            if (const size_t cut = name.find(L':'); cut != std::wstring::npos) name = name.substr(0, cut);
            for (auto& ch : name) ch = static_cast<wchar_t>(std::towupper(ch));
            p.Name = name;
            // a few metres off it, where she can stand (or at it, when there is nowhere else)
            p.P = m.P;
            for (int k = 0; k < 8; ++k)
            {
                const float ang = XM_2PI * (static_cast<float>(k) + Hash(p.Id, 1)) / 8.f;
                const float x = m.P.x + std::sin(ang) * 5.f, z = m.P.z + std::cos(ang) * 5.f;
                float y = 0;
                if (ground(x, z, m.P.y, y)) { p.P = {x, y, z}; break; }
            }
            mPlaces.push_back(p);
        }
        mLaid = true;
    }

    Wayside::Gain Wayside::Strike(const XMFLOAT3& from, float yaw, float reach, float arcDegrees)
    {
        Gain gain; gain.At = from;
        const float ax = std::sin(yaw), az = std::cos(yaw), cosArc = std::cos(XMConvertToRadians((std::min)(arcDegrees, 359.f) * .5f));
        for (auto& j : mJars)
        {
            if (j.Broken) continue;
            const float dx = j.P.x - from.x, dz = j.P.z - from.z, dy = j.P.y - from.y;
            if (std::abs(dx) > reach + 2 || std::abs(dz) > reach + 2 || dy < -2.5f || dy > 1.5f) continue;
            const float d = std::sqrt(dx * dx + dz * dz), r = .32f * j.Size;
            if (d > reach + r) continue;
            if (d > r + .3f && arcDegrees < 359.f && (dx * ax + dz * az) / d < cosArc) continue;
            j.Broken = true;
            gain.Crystals += j.Crystals; gain.Potions += j.Potion ? 1 : 0; ++gain.Jars;
            gain.At = {j.P.x, j.P.y + .5f * j.Size, j.P.z};
            // its sherds: they fly from where it stood, fall to the ground and lie there a while
            const float height = (j.Shape == 0 ? 1.1f : j.Shape == 1 ? .92f : .6f) * j.Size;
            for (int k = 0; k < 16; ++k)
            {
                const std::string key = j.Id + "#" + std::to_string(k);
                const float ang = XM_2PI * Hash(key, 1), up = Hash(key, 2);
                Shard s;
                s.P = {j.P.x + std::sin(ang) * .25f * j.Size, j.P.y + height * (.15f + .7f * up), j.P.z + std::cos(ang) * .25f * j.Size};
                const float speed = 1.6f + 2.4f * Hash(key, 3);
                s.V = {std::sin(ang) * speed + (dx / (std::max)(d, .1f)) * 1.2f, 1.5f + 2.5f * Hash(key, 4), std::cos(ang) * speed + (dz / (std::max)(d, .1f)) * 1.2f};
                s.Ground = j.P.y; s.Spin = XM_2PI * Hash(key, 5); s.Size = (.06f + .1f * Hash(key, 6)) * j.Size; s.Tint = j.Tint;
                mShards.push_back(s);
            }
        }
        return gain;
    }

    Wayside::Gain Wayside::Visit(const XMFLOAT3& her)
    {
        Gain gain; gain.At = her;
        for (auto& p : mPlaces)
        {
            if (p.Found || std::hypot(her.x - p.P.x, her.z - p.P.z) > PlaceReach || std::abs(her.y - p.P.y) > 2.5f) continue;
            p.Found = true;
            gain.Crystals += PlaceCrystals; gain.Place = p.Name; gain.At = {p.P.x, p.P.y + 1.4f, p.P.z};
        }
        return gain;
    }

    void Wayside::Update(float dt)
    {
        mTime += dt;
        for (auto& s : mShards)
        {
            s.Life += dt;
            if (s.Resting) continue;
            s.V.y -= 16.f * dt;
            s.P.x += s.V.x * dt; s.P.y += s.V.y * dt; s.P.z += s.V.z * dt;
            s.Spin += dt * 9.f;
            if (s.P.y <= s.Ground + s.Size * .3f) { s.P.y = s.Ground + s.Size * .3f; s.Resting = true; }
        }
        mShards.erase(std::remove_if(mShards.begin(), mShards.end(), [](const Shard& s) { return s.Life > ShardSeconds; }), mShards.end());
    }

    void Wayside::Draw(Renderer& renderer, const WorldState& world, const XMMATRIX& viewProjection, ID3D11ShaderResourceView* glowTexture)
    {
        if (!mMeshes) { mGlows.clear(); return; }
        renderer.ResetDefaultWorldRenderState(); renderer.BindDoorwaysSkinningDisabled();
        const XMFLOAT3 eye = world.MainCamera.Position;
        ID3D11ShaderResourceView* white = renderer.mWhiteTextureView.Get();
        // the clay: fired red-brown, from orange to dark (each jar its own)
        const auto clay = [&](float tint)
        {
            return Surface(white, {.36f + .12f * tint, .17f + .06f * tint, .09f + .03f * tint, 1}, .01f, false);
        };
        const Material band = Surface(white, {.13f, .07f, .05f, 1}, .01f, false);
        for (const auto& j : mJars)
        {
            if (j.Broken) continue;
            const float dx = j.P.x - eye.x, dz = j.P.z - eye.z;
            if (dx * dx + dz * dz > JarSeen * JarSeen) continue;
            const XMMATRIX where = XMMatrixScaling(j.Size, j.Size, j.Size) * XMMatrixRotationY(j.Yaw) * XMMatrixTranslation(j.P.x, j.P.y - .03f * j.Size, j.P.z);
            renderer.DrawDoorwaysGpuMesh(mShapes[j.Shape], where, viewProjection, clay(j.Tint));
            renderer.DrawDoorwaysGpuMesh(mBands[j.Shape], where, viewProjection, band);
        }
        for (const auto& s : mShards)
        {
            const float fade = std::clamp((ShardSeconds - s.Life) / 1.5f, 0.f, 1.f);
            const float size = s.Size * fade;
            if (size <= .001f) continue;
            renderer.DrawBox(XMMatrixScaling(size, size * .35f, size * .8f) * XMMatrixRotationRollPitchYaw(s.Spin * .7f, s.Spin, s.Spin * .4f) *
                             XMMatrixTranslation(s.P.x, s.P.y, s.P.z), viewProjection, clay(s.Tint));
        }
        // the hidden places' cairns: stones heaped, and a gold light on them until she has found it
        const Material stone[2] = {Surface(white, {.42f, .40f, .37f, 1}, .02f, false), Surface(white, {.33f, .31f, .29f, 1}, .02f, false)};
        std::vector<GlowLight> glows = mGlows;
        mGlows.clear();
        for (const auto& p : mPlaces)
        {
            const float dx = p.P.x - eye.x, dz = p.P.z - eye.z;
            if (dx * dx + dz * dz > 300.f * 300.f) continue;
            float y = p.P.y;
            for (int k = 0; k < 5; ++k)
            {
                const float w = .9f - .15f * static_cast<float>(k), h = .22f;
                renderer.DrawBox(XMMatrixScaling(w, h, w * .85f) * XMMatrixRotationY(.7f * static_cast<float>(k) + Hash(p.Id, 3)) *
                                 XMMatrixTranslation(p.P.x, y + h * .5f, p.P.z), viewProjection, stone[k % 2]);
                y += h * .92f;
            }
            if (!p.Found) glows.push_back({{p.P.x, y + .35f + .08f * std::sin(mTime * 2.4f), p.P.z}, 1.1f + .15f * std::sin(mTime * 3.1f), {1.f, .78f, .35f, .9f}});
        }
        if (!glowTexture) return;
        for (const auto& g : glows)
        {
            XMVECTOR n = XMVectorSet(eye.x - g.At.x, eye.y - g.At.y, eye.z - g.At.z, 0);
            if (XMVectorGetX(XMVector3Length(n)) < 1e-3f) continue;
            n = XMVector3Normalize(n);
            XMVECTOR x = XMVector3Cross(XMVectorSet(0, 1, 0, 0), n);
            x = XMVectorGetX(XMVector3Length(x)) < 1e-3f ? XMVectorSet(1, 0, 0, 0) : XMVector3Normalize(x);
            const XMVECTOR y = XMVector3Cross(n, x);
            XMMATRIX m = XMMatrixIdentity();
            m.r[0] = x * g.Size; m.r[1] = y * g.Size; m.r[2] = n; m.r[3] = XMVectorSet(g.At.x, g.At.y, g.At.z, 1);
            Material light = Surface(glowTexture, g.Color, 1, true);
            light.ReceiveFog = false;
            renderer.DrawDoorwaysGpuMesh(mQuad, m, viewProjection, light);
        }
    }

    std::vector<std::string> Wayside::BrokenIds() const
    {
        std::vector<std::string> ids;
        for (const auto& j : mJars) if (j.Broken) ids.push_back(j.Id);
        return ids;
    }

    std::vector<std::string> Wayside::FoundIds() const
    {
        std::vector<std::string> ids;
        for (const auto& p : mPlaces) if (p.Found) ids.push_back(p.Id);
        return ids;
    }

    void Wayside::Restore(const std::vector<std::string>& broken, const std::vector<std::string>& found)
    {
        for (auto& j : mJars) j.Broken = std::find(broken.begin(), broken.end(), j.Id) != broken.end();
        for (auto& p : mPlaces) p.Found = std::find(found.begin(), found.end(), p.Id) != found.end();
        mShards.clear();
    }
}
