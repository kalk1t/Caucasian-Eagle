#define NOMINMAX
#include "AscentGrass.h"

#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRuntimePathResolver.h"
#include "EaglesAscent.h"
#include "GameConstants.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>

using namespace DirectX;

namespace CaucasianEagle
{
    namespace
    {
        constexpr float Pi = 3.14159265f;

        unsigned Hash(int x, int z, unsigned salt)
        {
            unsigned h = static_cast<unsigned>(x) * 0x8DA6B343u ^ static_cast<unsigned>(z) * 0xD8163841u ^ salt * 0xCB1AB31Fu;
            h ^= h >> 13; h *= 0x5BD1E995u; h ^= h >> 15; h *= 0x27D4EB2Du; h ^= h >> 13;
            return h;
        }
        float Unit(int x, int z, unsigned salt) { return static_cast<float>(Hash(x, z, salt) & 0xFFFFFF) / 16777216.f; }
        // smooth noise 0..1, features about one unit across: the field's thick and thin patches
        float Patches(float x, float z, unsigned salt)
        {
            const float fx = std::floor(x), fz = std::floor(z);
            const int ix = static_cast<int>(fx), iz = static_cast<int>(fz);
            float tx = x - fx, tz = z - fz;
            tx = tx * tx * (3 - 2 * tx); tz = tz * tz * (3 - 2 * tz);
            const float a = Unit(ix, iz, salt), b = Unit(ix + 1, iz, salt), c = Unit(ix, iz + 1, salt), d = Unit(ix + 1, iz + 1, salt);
            return (a * (1 - tx) + b * tx) * (1 - tz) + (c * (1 - tx) + d * tx) * tz;
        }
        float Smooth(float x, float a, float b) { const float t = std::clamp((x - a) / (b - a), 0.f, 1.f); return t * t * (3 - 2 * t); }
        long long Key(int cx, int cz) { return (static_cast<long long>(cx) << 32) ^ static_cast<unsigned>(cz); }

        struct MeshBuilder
        {
            Doorways::DoorwaysLoadedDMesh Mesh;
            Doorways::u32 Vertex(XMFLOAT3 p, XMFLOAT3 n, float u, float v)
            {
                Doorways::DoorwaysDMeshVertex vertex{};
                vertex.Position = {p.x, p.y, p.z}; vertex.Normal = {n.x, n.y, n.z}; vertex.Tangent = {1, 0, 0, 1};
                vertex.TexCoord0 = {u, v}; vertex.JointWeights[0] = 1;
                Mesh.Vertices.push_back(vertex);
                return static_cast<Doorways::u32>(Mesh.Vertices.size() - 1);
            }
            void Quad(Doorways::u32 a, Doorways::u32 b, Doorways::u32 c, Doorways::u32 d) { Mesh.Indices.insert(Mesh.Indices.end(), {a, b, c, a, c, d}); }
            bool Upload(Renderer& renderer, Doorways::DoorwaysGpuMesh& out)
            {
                if (Mesh.Indices.empty()) return false;
                Mesh.Submeshes = {{0, static_cast<Doorways::u32>(Mesh.Indices.size()), 0, 0}};
                Mesh.MeshHeader.VertexCount = static_cast<Doorways::u32>(Mesh.Vertices.size());
                Mesh.MeshHeader.IndexCount = static_cast<Doorways::u32>(Mesh.Indices.size());
                Mesh.MeshHeader.SubmeshCount = 1;
                Mesh.MeshHeader.VertexStride = sizeof(Doorways::DoorwaysDMeshVertex);
                std::string error;
                return renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(Mesh, out, &error);
            }
        };

        // one RGBA8 DDS with the DX10 header, as terrain_splat.py writes it
        bool ReadSplat(const std::filesystem::path& path, std::vector<unsigned char>& out, int& w, int& h)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file) return false;
            unsigned char head[148]{};
            if (!file.read(reinterpret_cast<char*>(head), sizeof head) || std::memcmp(head, "DDS ", 4) != 0) return false;
            std::uint32_t hh = 0, ww = 0; std::memcpy(&hh, head + 12, 4); std::memcpy(&ww, head + 16, 4);
            if (ww < 2 || hh < 2 || ww > 8192 || hh > 8192) return false;
            out.resize(static_cast<size_t>(ww) * hh * 4);
            if (!file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()))) return false;
            w = static_cast<int>(ww); h = static_cast<int>(hh);
            return true;
        }
    }

    bool AscentGrass::Initialize(Renderer& renderer, const EaglesAscent& map, const std::filesystem::path& dataFile, std::string& error)
    {
        mMap = &map; mReady = false; mSquares.clear();
        char value[8] = {}; size_t got = 0;
        if (getenv_s(&got, value, sizeof(value), "CAUCASIAN_EAGLE_GRASS") == 0 && got > 1 && value[0] == '0') return true;
        DoorwaysTextureLoadDesc desc;
        desc.Path = Doorways::ResolveDoorwaysRuntimePath(std::string(NatureTextureFolder) + "/Grass.png");
        desc.Semantic = Doorways::DoorwaysMaterialTextureSemantic::BaseColor;
        desc.GenerateMips = true;
        DoorwaysTextureLoadResult result;
        if (!renderer.LoadDoorwaysTexture(desc, result, &error)) { error = "grass: " + error; return false; }
        mTexture = result.SRV;

        float terrain[7] = {};
        if (map.TerrainInfo(terrain))
        {
            mX0 = terrain[0]; mZ0 = terrain[1]; mSnow = terrain[4]; mSea = terrain[5];
            auto a = dataFile; a.replace_extension(".splat_a.dds");
            auto b = dataFile; b.replace_extension(".splat_b.dds");
            int wa = 0, ha = 0, wb = 0, hb = 0;
            if (ReadSplat(a, mA, wa, ha) && ReadSplat(b, mB, wb, hb) && wa == wb && ha == hb)
            {
                mW = wa; mH = ha; mRes = terrain[2] / static_cast<float>(mW);
            }
            else { mA.clear(); mB.clear(); }
        }
        mBuilt.clear(); mBuiltCells.clear();
        for (const auto& c : map.BuiltCircles())
        {
            const unsigned index = static_cast<unsigned>(mBuilt.size());
            mBuilt.push_back({c.x, c.y, c.z});
            const int x0 = static_cast<int>(std::floor((c.x - c.z) / 48.f)), x1 = static_cast<int>(std::floor((c.x + c.z) / 48.f));
            const int z0 = static_cast<int>(std::floor((c.y - c.z) / 48.f)), z1 = static_cast<int>(std::floor((c.y + c.z) / 48.f));
            for (int cz = z0; cz <= z1; ++cz) for (int cx = x0; cx <= x1; ++cx) mBuiltCells[Key(cx, cz)].push_back(index);
        }
        mReady = true;
        return true;
    }

    float AscentGrass::Splat(const std::vector<unsigned char>& picture, int channel, float x, float z) const
    {
        const float fx = std::clamp((x - mX0) / mRes - .5f, 0.f, static_cast<float>(mW) - 1.001f);
        const float fz = std::clamp((z - mZ0) / mRes - .5f, 0.f, static_cast<float>(mH) - 1.001f);
        const int ix = static_cast<int>(fx), iz = static_cast<int>(fz);
        const float tx = fx - static_cast<float>(ix), tz = fz - static_cast<float>(iz);
        const auto at = [&](int i, int j) { return static_cast<float>(picture[(static_cast<size_t>(j) * mW + i) * 4 + channel]) / 255.f; };
        return (at(ix, iz) * (1 - tx) + at(ix + 1, iz) * tx) * (1 - tz) + (at(ix, iz + 1) * (1 - tx) + at(ix + 1, iz + 1) * tx) * tz;
    }

    float AscentGrass::Grows(float x, float z, float& y) const
    {
        y = mMap->Land(x, z);
        if (y < -1e8f || y < mSea + 1.2f) return 0;
        // steep ground is rock in the terrain shader (it adds rock by slope): no grass there
        const float slope = std::max(std::abs(mMap->Land(x + 1.f, z) - mMap->Land(x - 1.f, z)), std::abs(mMap->Land(x, z + 1.f) - mMap->Land(x, z - 1.f))) * .5f;
        if (slope > .78f) return 0;
        float w = 0;
        const bool inSplat = !mA.empty() && x > mX0 + mRes && z > mZ0 + mRes && x < mX0 + mRes * static_cast<float>(mW - 1) && z < mZ0 + mRes * static_cast<float>(mH - 1);
        if (inSplat)
        {
            const float meadow = Splat(mA, 0, x, z), forest = Splat(mA, 1, x, z), scree = Splat(mA, 2, x, z), sand = Splat(mA, 3, x, z);
            const float trail = Splat(mB, 0, x, z), moss = Splat(mB, 1, x, z), snow = Splat(mB, 2, x, z), rock = Splat(mB, 3, x, z);
            if (trail > .12f) return 0;
            w = meadow + .5f * forest + .35f * moss - 1.6f * scree - 2.5f * sand - 2.5f * snow - 2.f * rock - 4.f * trail;
        }
        else
        {
            if (mMap->RingDistance({x, y, z}) < 3.f) return 0;
            w = .75f;
        }
        w *= 1 - Smooth(slope, .5f, .78f);
        w *= 1 - Smooth(y, mSnow - 450.f, mSnow - 120.f);   // thinning toward the snow line
        if (w <= .02f) return 0;
        const auto built = mBuiltCells.find(Key(static_cast<int>(std::floor(x / 48.f)), static_cast<int>(std::floor(z / 48.f))));
        if (built != mBuiltCells.end())
            for (const unsigned i : built->second)
            {
                const Built& b = mBuilt[i];
                if ((x - b.X) * (x - b.X) + (z - b.Z) * (z - b.Z) < b.R * b.R) return 0;
            }
        return std::min(w, 1.f);
    }

    // One square: the light layer (one place in three) or the thick one (the other two). A tuft is three upright cards
    // through one point, 60 degrees apart, standing on the land; taller and greener low on the mountain, short and yellow
    // high up.
    void AscentGrass::Build(Renderer& renderer, int cx, int cz, bool thick, Square& square)
    {
        const float x0 = static_cast<float>(cx) * Cell, z0 = static_cast<float>(cz) * Cell;
        const int lanes = static_cast<int>(Cell * std::sqrt(Density));
        const float pitch = Cell / static_cast<float>(lanes);
        MeshBuilder mesh;
        mesh.Mesh.Vertices.reserve(thick ? 24000 : 12000);
        const XMFLOAT3 up{0, 1, 0};   // lit like the ground under it
        int tufts = 0;
        for (int j = 0; j < lanes; ++j)
            for (int i = 0; i < lanes; ++i)
            {
                const int gx = cx * 977 + i, gz = cz * 977 + j;
                if ((Unit(gx, gz, 9) < .34f) == thick) continue;   // its layer
                const float x = x0 + (static_cast<float>(i) + Unit(gx, gz, 1)) * pitch, z = z0 + (static_cast<float>(j) + Unit(gx, gz, 2)) * pitch;
                float y = 0;
                const float w = Grows(x, z, y);
                if (w <= 0) continue;
                // fields in patches: thick swathes and thinner ground between, with small clumps through them
                const float patch = Patches(x / 17.f, z / 17.f, 7) * .65f + Patches(x / 3.3f, z / 3.3f, 8) * .35f;
                if (Unit(gx, gz, 3) > w * (.35f + .9f * patch)) continue;
                const float high = Smooth(y, 1200.f, 3200.f);
                const float tall = (.42f + .55f * patch) * (1.f - .45f * high) * (.7f + .6f * Unit(gx, gz, 4)) * (.75f + .25f * w);
                const float half = tall * (.5f + .25f * Unit(gx, gz, 5));
                const float turn = Unit(gx, gz, 6) * Pi, lean = (Unit(gx, gz, 10) - .5f) * .25f * tall;
                const int picture = static_cast<int>(Hash(gx, gz, 11) & 3);
                const float u0 = static_cast<float>(picture) / 4 + .004f, u1 = static_cast<float>(picture + 1) / 4 - .004f;
                const float ly = y - .06f;
                for (int card = 0; card < 3; ++card)
                {
                    const float a = turn + static_cast<float>(card) * Pi / 3, dx = std::cos(a) * half, dz = std::sin(a) * half;
                    const Doorways::u32 v = mesh.Vertex({x - dx, ly, z - dz}, up, u0, 1);
                    mesh.Vertex({x - dx + lean, ly + tall, z - dz + lean}, up, u0, 0);
                    mesh.Vertex({x + dx + lean, ly + tall, z + dz + lean}, up, u1, 0);
                    mesh.Vertex({x + dx, ly, z + dz}, up, u1, 1);
                    mesh.Quad(v, v + 1, v + 2, v + 3);
                }
                ++tufts;
            }
        // the square's colour: green below, toward straw near the snow; a little different square to square
        const float y = mMap->Land(x0 + Cell * .5f, z0 + Cell * .5f);
        const float high = Smooth(y, 900.f, 3300.f), v = .9f + .2f * Unit(cx, cz, 12);
        square.Tint = {(.66f + .24f * high) * v, (.74f + .02f * high) * v, (.42f + .04f * high) * v};   // (meadow, not lawn: 2026-11-20 pictures)
        if (thick) { square.HasThick = true; square.ThickTufts = mesh.Upload(renderer, square.Thick) ? tufts : 0; }
        else square.LightTufts = mesh.Upload(renderer, square.Light) ? tufts : 0;
    }

    void AscentGrass::Draw(Renderer& renderer, const XMFLOAT3& eye, const XMMATRIX& viewProjection)
    {
        TuftsDrawn = 0; CellsDrawn = 0;
        if (!mReady || !mTexture) return;
        ++mFrame;
        Material m{};
        m.DiffuseMap = mTexture.Get(); m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .95f;
        m.SpecularColor = {0, 0, 0, 0}; m.EmissiveStrength = .03f; m.DoubleSided = true; m.ReceiveFog = true;
        m.AlphaMode = Doorways::DoorwaysMaterialAlphaMode::Mask; m.AlphaCutoff = .42f;
        // (high in the air the land is far below: no grass is built for squares she cannot see it in)
        const float ground = mMap->Land(eye.x, eye.z);
        if (ground > -1e8f && eye.y - ground > FarDistance) return;
        const int reach = static_cast<int>(FarDistance / Cell) + 1;
        const int ex = static_cast<int>(std::floor(eye.x / Cell)), ez = static_cast<int>(std::floor(eye.z / Cell));
        int built = 0;
        for (int ring = 0; ring <= reach; ++ring)           // nearest squares first
            for (int cz = ez - ring; cz <= ez + ring; ++cz)
                for (int cx = ex - ring; cx <= ex + ring; ++cx)
                {
                    if (std::max(std::abs(cx - ex), std::abs(cz - ez)) != ring) continue;
                    const float mx = (static_cast<float>(cx) + .5f) * Cell - eye.x, mz = (static_cast<float>(cz) + .5f) * Cell - eye.z;
                    const float d = std::sqrt(mx * mx + mz * mz);
                    if (d > FarDistance + Cell * .7f) continue;
                    const long long key = Key(cx, cz);
                    auto found = mSquares.find(key);
                    if (found == mSquares.end())
                    {
                        if (built >= BuildsPerFrame) continue;
                        ++built;
                        found = mSquares.emplace(key, Square{}).first;
                        Build(renderer, cx, cz, false, found->second);
                    }
                    Square& square = found->second;
                    square.Used = mFrame;
                    const bool close = d < NearDistance + Cell * .7f;
                    if (close && !square.HasThick && built < BuildsPerFrame) { ++built; Build(renderer, cx, cz, true, square); }
                    if (!close && square.HasThick && d > NearDistance + Cell * 2.5f) { square.Thick = {}; square.HasThick = false; square.ThickTufts = 0; }   // (left behind)
                    m.Diffuse = {square.Tint.x, square.Tint.y, square.Tint.z, 1};
                    if (square.LightTufts > 0) { renderer.DrawDoorwaysGpuMesh(square.Light, XMMatrixIdentity(), viewProjection, m); TuftsDrawn += square.LightTufts; ++CellsDrawn; }
                    if (close && square.ThickTufts > 0) { renderer.DrawDoorwaysGpuMesh(square.Thick, XMMatrixIdentity(), viewProjection, m); TuftsDrawn += square.ThickTufts; }
                }
        if (mSquares.size() > 260)     // squares she left behind are let go
            std::erase_if(mSquares, [&](const auto& entry) { return mFrame - entry.second.Used > 120; });
    }
}
