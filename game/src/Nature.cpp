#define NOMINMAX
#include "Nature.h"

#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRuntimePathResolver.h"
#include "GameConstants.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

using namespace DirectX;

namespace CaucasianEagle
{
    Nature& GetNature()
    {
        static Nature nature;
        return nature;
    }

    namespace
    {
        constexpr float Pi = 3.14159265f;

        // ------------------------------------------------------------ what grows in each region
        struct Growth
        {
            float Grass;        // tufts per square metre where grass grows at all
            float Blade;        // m, height of a tuft
            XMFLOAT3 Tint;      // colour of the grass (the texture is green)
            float Trees;        // share of the 15 m squares that hold a tree
            bool Conifer;
            XMFLOAT3 Leaf;      // colour of the leaves
        };
        const Growth Regions[12] = {
            {.55f, .50f, {.80f, 1.00f, .86f}, .10f, false, {.80f, 1.00f, .85f}},   // Moonlit Causeway: mossy stone
            {.90f, .60f, {.78f, .95f, .70f}, .50f, false, {.74f, .92f, .66f}},     // Whispering Wood
            {1.20f, .95f, {.80f, .92f, .62f}, .06f, false, {.78f, .90f, .62f}},    // Drowned Marsh: reeds
            {.70f, .55f, {.70f, .92f, .68f}, .62f, true, {.52f, .78f, .62f}},      // Ochokochi's Wood: firs
            {1.10f, .45f, {.86f, .98f, .66f}, .14f, true, {.55f, .80f, .62f}},     // Stone Towers: highland meadow
            {.12f, .30f, {1.00f, 1.00f, 1.00f}, .18f, true, {.70f, .86f, .90f}},   // Snow Pass: firs in snow
            {.10f, .35f, {.62f, .50f, .40f}, 0, false, {.6f, .5f, .4f}},           // Kajeti: ash
            {.35f, .40f, {.92f, .86f, .58f}, .05f, true, {.55f, .76f, .60f}},      // Giants' Valley: scree
            {.90f, .60f, {.72f, .98f, .84f}, .16f, false, {.70f, .95f, .80f}},     // Dragon Lake
            {.30f, .45f, {1.00f, .86f, .50f}, .03f, false, {.90f, .80f, .50f}},    // Devi's Gorge: dry ochre
            {1.00f, .40f, {.86f, .74f, .96f}, .10f, false, {.72f, .70f, .84f}},    // Witches' Hill: heath
            {.10f, .30f, {.80f, .90f, .90f}, 0, false, {.8f, .9f, .9f}}};          // Chain Road: bare rock
        // Where the two lakes stand (art/world/build_world.py WATER): nothing grows under water.
        float WaterLevel(int region) { return region == 2 ? -13.1f : region == 8 ? 17.1f : -1e9f; }

        // ------------------------------------------------------------ a rule instead of a list
        unsigned Hash(int x, int z, unsigned salt)
        {
            unsigned h = static_cast<unsigned>(x) * 0x8DA6B343u ^ static_cast<unsigned>(z) * 0xD8163841u ^ salt * 0xCB1AB31Fu;
            h ^= h >> 13; h *= 0x5BD1E995u; h ^= h >> 15; h *= 0x27D4EB2Du; h ^= h >> 13;
            return h;
        }
        float Unit(int x, int z, unsigned salt) { return static_cast<float>(Hash(x, z, salt) & 0xFFFFFF) / 16777216.f; }
        // Smooth noise 0..1 with features about one unit across: where grass stands thick and where it thins out.
        float Patches(float x, float z)
        {
            const float fx = std::floor(x), fz = std::floor(z);
            const int ix = static_cast<int>(fx), iz = static_cast<int>(fz);
            float tx = x - fx, tz = z - fz;
            tx = tx * tx * (3 - 2 * tx); tz = tz * tz * (3 - 2 * tz);
            const float a = Unit(ix, iz, 7), b = Unit(ix + 1, iz, 7), c = Unit(ix, iz + 1, 7), d = Unit(ix + 1, iz + 1, 7);
            return (a * (1 - tx) + b * tx) * (1 - tz) + (c * (1 - tx) + d * tx) * tz;
        }
        float Smooth(float x, float a, float b)
        {
            const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
            return t * t * (3 - 2 * t);
        }

        // ------------------------------------------------------------ meshes
        struct MeshBuilder
        {
            Doorways::DoorwaysLoadedDMesh Mesh;
            Doorways::u32 Vertex(XMFLOAT3 p, XMFLOAT3 n, float u, float v)
            {
                Doorways::DoorwaysDMeshVertex vertex{};
                vertex.Position = {p.x, p.y, p.z};
                vertex.Normal = {n.x, n.y, n.z};
                vertex.Tangent = {1, 0, 0, 1};
                vertex.TexCoord0 = {u, v};
                vertex.JointWeights[0] = 1;
                Mesh.Vertices.push_back(vertex);
                return static_cast<Doorways::u32>(Mesh.Vertices.size() - 1);
            }
            void Quad(Doorways::u32 a, Doorways::u32 b, Doorways::u32 c, Doorways::u32 d)
            {
                Mesh.Indices.insert(Mesh.Indices.end(), {a, b, c, a, c, d});
            }
            bool Empty() const { return Mesh.Indices.empty(); }
            bool Upload(Renderer& renderer, Doorways::DoorwaysGpuMesh& out, std::string& error)
            {
                Mesh.Submeshes = {{0, static_cast<Doorways::u32>(Mesh.Indices.size()), 0, 0}};
                Mesh.MeshHeader.VertexCount = static_cast<Doorways::u32>(Mesh.Vertices.size());
                Mesh.MeshHeader.IndexCount = static_cast<Doorways::u32>(Mesh.Indices.size());
                Mesh.MeshHeader.SubmeshCount = 1;
                Mesh.MeshHeader.VertexStride = sizeof(Doorways::DoorwaysDMeshVertex);
                return renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(Mesh, out, &error);
            }
        };

        bool LoadTexture(Renderer& renderer, const char* file, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& out, std::string& error)
        {
            DoorwaysTextureLoadDesc desc;
            desc.Path = Doorways::ResolveDoorwaysRuntimePath(std::string(NatureTextureFolder) + "/" + file);
            desc.Semantic = Doorways::DoorwaysMaterialTextureSemantic::BaseColor;
            desc.GenerateMips = true;
            DoorwaysTextureLoadResult result;
            if (!renderer.LoadDoorwaysTexture(desc, result, &error)) return false;
            out = result.SRV;
            return true;
        }

        Material Surface(ID3D11ShaderResourceView* texture, XMFLOAT4 tint, float emission, Doorways::DoorwaysMaterialAlphaMode alpha, bool fog = true)
        {
            Material m{};
            m.Diffuse = tint; m.DiffuseMap = texture;
            m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .95f;
            m.SpecularColor = {0, 0, 0, 0}; m.EmissiveStrength = emission;
            m.DoubleSided = true; m.ReceiveFog = fog;
            m.AlphaMode = alpha; m.AlphaCutoff = .42f;
            return m;
        }

        // A tapering tube from a to b, six sides, bark wrapped round it.
        void Limb(MeshBuilder& mesh, XMFLOAT3 a, XMFLOAT3 b, float radiusA, float radiusB)
        {
            const XMVECTOR va = XMLoadFloat3(&a), vb = XMLoadFloat3(&b), axis = XMVector3Normalize(vb - va);
            const XMVECTOR other = std::abs(XMVectorGetY(axis)) > .9f ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
            const XMVECTOR side = XMVector3Normalize(XMVector3Cross(axis, other)), up = XMVector3Cross(side, axis);
            const float length = XMVectorGetX(XMVector3Length(vb - va));
            const int sides = 6;
            Doorways::u32 first = 0;
            for (int i = 0; i <= sides; ++i)
            {
                const float angle = static_cast<float>(i) / sides * 2 * Pi;
                const XMVECTOR out = side * std::cos(angle) + up * std::sin(angle);
                XMFLOAT3 n, p0, p1;
                XMStoreFloat3(&n, out); XMStoreFloat3(&p0, va + out * radiusA); XMStoreFloat3(&p1, vb + out * radiusB);
                const float u = static_cast<float>(i) / sides * 2;
                const Doorways::u32 lower = mesh.Vertex(p0, n, u, 0);
                mesh.Vertex(p1, n, u, length * 3);
                if (i == 0) first = lower;
                else mesh.Quad(lower - 2, lower - 1, lower + 1, lower);
            }
            (void)first;
        }

        // One spray of leaves: a square card facing 'facing', lit as if it were part of a round crown.
        void Spray(MeshBuilder& mesh, XMFLOAT3 centre, XMFLOAT3 facing, float size, float turn, int picture, XMFLOAT3 crown)
        {
            const XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&facing));
            const XMVECTOR other = std::abs(XMVectorGetY(f)) > .9f ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 1, 0, 0);
            XMVECTOR right = XMVector3Normalize(XMVector3Cross(other, f)), up = XMVector3Cross(f, right);
            const XMVECTOR r2 = right * std::cos(turn) + up * std::sin(turn), u2 = up * std::cos(turn) - right * std::sin(turn);
            const XMVECTOR c = XMLoadFloat3(&centre);
            XMFLOAT3 n; XMStoreFloat3(&n, XMVector3Normalize(c - XMLoadFloat3(&crown) + XMVectorSet(0, size * .6f, 0, 0)));   // outward and a little up
            const float u0 = static_cast<float>(picture) / 4 + .004f, u1 = static_cast<float>(picture + 1) / 4 - .004f;
            XMFLOAT3 p[4];
            XMStoreFloat3(&p[0], c - r2 * size - u2 * size); XMStoreFloat3(&p[1], c - r2 * size + u2 * size);
            XMStoreFloat3(&p[2], c + r2 * size + u2 * size); XMStoreFloat3(&p[3], c + r2 * size - u2 * size);
            const Doorways::u32 i = mesh.Vertex(p[0], n, u0, 1);
            mesh.Vertex(p[1], n, u0, 0); mesh.Vertex(p[2], n, u1, 0); mesh.Vertex(p[3], n, u1, 1);
            mesh.Quad(i, i + 1, i + 2, i + 3);
        }
    }

    // ================================================================ the land
    bool Nature::Initialize(Renderer& renderer, std::string& error)
    {
        const auto path = Doorways::ResolveDoorwaysRuntimePath("assets/Compiled/World/Terrain.bin");
        std::ifstream file(path, std::ios::binary);
        char magic[4]{};
        if (!file.read(magic, 4) || std::memcmp(magic, "CETR", 4) != 0) { error = "Terrain.bin is missing (run art/world/build_nature.py)"; return false; }
        file.read(reinterpret_cast<char*>(&mNx), 4); file.read(reinterpret_cast<char*>(&mNz), 4);
        file.read(reinterpret_cast<char*>(&mX0), 4); file.read(reinterpret_cast<char*>(&mZ0), 4); file.read(reinterpret_cast<char*>(&mGrid), 4);
        if (mNx < 2 || mNz < 2 || mNx > 4096 || mNz > 4096 || mGrid <= 0) { error = "Terrain.bin has a bad header"; return false; }
        const size_t count = static_cast<size_t>(mNx) * mNz;
        mHeight.resize(count); mRegion.resize(count);
        std::vector<unsigned char> beyond(count);
        file.read(reinterpret_cast<char*>(mHeight.data()), count * 4);
        file.read(reinterpret_cast<char*>(beyond.data()), count);
        if (!file.read(reinterpret_cast<char*>(mRegion.data()), count)) { error = "Terrain.bin is cut short"; return false; }
        mBeyond.assign(beyond.begin(), beyond.end());

        if (!LoadTexture(renderer, "Grass.png", mGrassTexture, error) || !LoadTexture(renderer, "Leaves.png", mLeafTexture, error) ||
            !LoadTexture(renderer, "Bark.png", mBarkTexture, error) || !LoadTexture(renderer, "Clouds.png", mCloudTexture, error)) return false;
        mReady = BuildTrees(renderer, error) && BuildClouds(renderer, error);
        return mReady;
    }

    float Nature::Sample(const std::vector<float>& field, float x, float z) const
    {
        const float fx = std::clamp((x - mX0) / mGrid, 0.f, static_cast<float>(mNx) - 1.001f);
        const float fz = std::clamp((z - mZ0) / mGrid, 0.f, static_cast<float>(mNz) - 1.001f);
        const int ix = static_cast<int>(fx), iz = static_cast<int>(fz);
        const float tx = fx - static_cast<float>(ix), tz = fz - static_cast<float>(iz);
        const float* row = field.data() + static_cast<size_t>(iz) * mNx + ix;
        return (row[0] * (1 - tx) + row[1] * tx) * (1 - tz) + (row[mNx] * (1 - tx) + row[mNx + 1] * tx) * tz;
    }
    float Nature::Height(float x, float z) const { return mHeight.empty() ? 0 : Sample(mHeight, x, z); }
    float Nature::Beyond(float x, float z) const { return mBeyond.empty() ? 0 : Sample(mBeyond, x, z); }
    int Nature::Region(float x, float z) const
    {
        if (mRegion.empty()) return 0;
        const int ix = std::clamp(static_cast<int>((x - mX0) / mGrid + .5f), 0, mNx - 1), iz = std::clamp(static_cast<int>((z - mZ0) / mGrid + .5f), 0, mNz - 1);
        return std::min<int>(mRegion[static_cast<size_t>(iz) * mNx + ix], 11);
    }

    bool Nature::Grows(float x, float z, int region) const
    {
        if (x < mX0 + mGrid || z < mZ0 + mGrid || x > mX0 + mGrid * static_cast<float>(mNx - 2) || z > mZ0 + mGrid * static_cast<float>(mNz - 2)) return false;
        if (z > MountainClearZ && std::abs(x) < 120) return false;                           // the foot of Amirani's mountain
        const float land = Height(x, z);
        if (land < WaterLevel(region) + .25f) return false;
        const float slope = std::max(std::abs(Height(x + 1.5f, z) - Height(x - 1.5f, z)), std::abs(Height(x, z + 1.5f) - Height(x, z - 1.5f))) / 3.f;
        if (slope > NatureMaxSlope) return false;
        for (const auto& c : mClearings)
            if ((x - c.X) * (x - c.X) + (z - c.Z) * (z - c.Z) < c.Radius * c.Radius) return false;
        return true;
    }

    // ================================================================ grass
    // One 20 m square of tufts as one mesh. A tuft is three upright cards through one point, 60 degrees apart.
    bool Nature::BuildCell(Renderer& renderer, int cx, int cz, Cell& cell)
    {
        const float x0 = static_cast<float>(cx) * GrassCell, z0 = static_cast<float>(cz) * GrassCell;
        cell.Region = Region(x0 + GrassCell * .5f, z0 + GrassCell * .5f);
        const Growth& growth = Regions[cell.Region];
        const int lanes = std::max(1, static_cast<int>(GrassCell * std::sqrt(growth.Grass) * 1.35f));   // candidates per side; about half survive
        const float pitch = GrassCell / static_cast<float>(lanes);
        MeshBuilder mesh;
        const XMFLOAT3 up{0, 1, 0};   // lit like the ground it stands on
        for (int j = 0; j < lanes; ++j)
            for (int i = 0; i < lanes; ++i)
            {
                const int gx = cx * 131 + i, gz = cz * 131 + j;
                const float x = x0 + (static_cast<float>(i) + Unit(gx, gz, 1)) * pitch, z = z0 + (static_cast<float>(j) + Unit(gx, gz, 2)) * pitch;
                const float beyond = Beyond(x, z);
                if (beyond < GrassFromRoad) continue;
                // grass stands in patches, thickest a little way from the road and thinning toward the far hills
                const float thick = Patches(x / 11.f, z / 11.f) * .75f + Patches(x / 3.1f, z / 3.1f) * .25f;
                const float keep = (.25f + .75f * Smooth(beyond, GrassFromRoad, GrassFromRoad + 4)) * (1 - .6f * Smooth(beyond, 60, 160));
                if (thick < 1 - keep * .78f) continue;
                if (!Grows(x, z, cell.Region)) continue;
                const float y = Height(x, z) - .04f;
                const float tall = growth.Blade * (.65f + .7f * Unit(gx, gz, 3)) * (.7f + .6f * thick), half = tall * (.55f + .25f * Unit(gx, gz, 4));
                const float turn = Unit(gx, gz, 5) * Pi;
                const int picture = static_cast<int>(Hash(gx, gz, 6) & 3);
                const float u0 = static_cast<float>(picture) / 4 + .004f, u1 = static_cast<float>(picture + 1) / 4 - .004f;
                for (int card = 0; card < 3; ++card)
                {
                    const float a = turn + static_cast<float>(card) * Pi / 3, dx = std::cos(a) * half, dz = std::sin(a) * half;
                    const Doorways::u32 v = mesh.Vertex({x - dx, y, z - dz}, up, u0, 1);
                    mesh.Vertex({x - dx, y + tall, z - dz}, up, u0, 0);
                    mesh.Vertex({x + dx, y + tall, z + dz}, up, u1, 0);
                    mesh.Vertex({x + dx, y, z + dz}, up, u1, 1);
                    mesh.Quad(v, v + 1, v + 2, v + 3);
                }
            }
        cell.Empty = mesh.Empty();
        if (cell.Empty) return true;
        std::string error;
        return mesh.Upload(renderer, cell.Mesh, error);
    }

    // ================================================================ trees
    bool Nature::TreeAt(int cx, int cz, float& x, float& z, int& region, float& tall, float& wide) const
    {
        x = (static_cast<float>(cx) + .15f + .7f * Unit(cx, cz, 21)) * TreeCell; z = (static_cast<float>(cz) + .15f + .7f * Unit(cx, cz, 22)) * TreeCell;
        region = Region(x, z);
        const Growth& growth = Regions[region];
        if (growth.Trees <= 0) return false;
        // woods are thick in places and open in others
        const float wood = Patches(x / 70.f, z / 70.f);
        if (Unit(cx, cz, 23) > growth.Trees * (.35f + 1.3f * wood)) return false;
        const float beyond = Beyond(x, z);
        if (beyond < TreeFromRoad || beyond > TreeToRoad || !Grows(x, z, region)) return false;
        tall = (growth.Conifer ? ConiferHeight : BroadleafHeight) * (.7f + .6f * Unit(cx, cz, 25));
        wide = tall * (.85f + .3f * Unit(cx, cz, 26));
        return true;
    }

    void Nature::PushFromTrees(float& x, float& z, float radius) const
    {
        if (!mReady) return;
        const int ex = static_cast<int>(std::floor(x / TreeCell)), ez = static_cast<int>(std::floor(z / TreeCell));
        for (int cz = ez - 1; cz <= ez + 1; ++cz)
            for (int cx = ex - 1; cx <= ex + 1; ++cx)
            {
                float tx = 0, tz = 0, tall = 0, wide = 0; int region = 0;
                if (!TreeAt(cx, cz, tx, tz, region, tall, wide)) continue;
                const float keep = wide * TrunkRadius + radius, dx = x - tx, dz = z - tz, d2 = dx * dx + dz * dz;
                if (d2 >= keep * keep) continue;
                const float d = std::sqrt(d2);
                if (d < 1e-3f) { x = tx + keep; continue; }
                x = tx + dx / d * keep; z = tz + dz / d * keep;
            }
    }

    // Trees are one unit tall here and are scaled when drawn. Three shapes of each kind.
    bool Nature::BuildTrees(Renderer& renderer, std::string& error)
    {
        for (int variant = 0; variant < 3; ++variant)
        {   // broadleaf: a trunk that forks into a few limbs, a round crown of leaf sprays
            MeshBuilder trunk, leaves;
            const auto r = [&](unsigned salt) { return Unit(variant, 91, salt); };
            const float fork = .34f + .1f * r(1);
            Limb(trunk, {0, -.03f, 0}, {.012f * (r(2) - .5f), fork, .012f * (r(3) - .5f)}, .042f, .028f);
            const XMFLOAT3 crown{0, .64f, 0};
            const int limbs = 4 + variant % 2;
            for (int i = 0; i < limbs; ++i)
            {
                const float a = (static_cast<float>(i) + r(10 + i)) / static_cast<float>(limbs) * 2 * Pi, reach = .16f + .12f * r(20 + i);
                const XMFLOAT3 tip{std::cos(a) * reach, fork + .2f + .16f * r(30 + i), std::sin(a) * reach};
                Limb(trunk, {0, fork - .02f, 0}, tip, .018f, .007f);
            }
            Limb(trunk, {0, fork - .02f, 0}, {0, .8f, 0}, .02f, .006f);
            const int sprays = 44;
            for (int i = 0; i < sprays; ++i)
            {
                // points spread over a flattened ball, more of them near its surface
                const float a = r(100 + i) * 2 * Pi, h = r(200 + i) * 2 - 1, ring = std::sqrt(1 - h * h), depth = .55f + .45f * std::sqrt(r(300 + i));
                const XMFLOAT3 out{std::cos(a) * ring, h * .82f, std::sin(a) * ring};
                const XMFLOAT3 at{crown.x + out.x * .37f * depth, crown.y + out.y * .37f * depth, crown.z + out.z * .37f * depth};
                Spray(leaves, at, {out.x + (r(400 + i) - .5f) * .8f, out.y + .25f, out.z + (r(500 + i) - .5f) * .8f}, .16f + .07f * r(600 + i), r(700 + i) * 2 * Pi,
                      static_cast<int>(Hash(variant, i, 9) & 3), crown);
            }
            Tree tree;
            if (!trunk.Upload(renderer, tree.Trunk, error) || !leaves.Upload(renderer, tree.Leaves, error)) return false;
            mBroadleaf.push_back(std::move(tree));
        }
        for (int variant = 0; variant < 3; ++variant)
        {   // conifer: a straight trunk, tiers of drooping boughs that shorten toward the top
            MeshBuilder trunk, leaves;
            const auto r = [&](unsigned salt) { return Unit(variant, 57, salt); };
            Limb(trunk, {0, -.03f, 0}, {0, .97f, 0}, .026f, .004f);
            const int tiers = 9 + variant;
            for (int tier = 0; tier < tiers; ++tier)
            {
                const float t = static_cast<float>(tier) / static_cast<float>(tiers - 1);       // 0 lowest .. 1 top
                const float y = .2f + .76f * t, reach = (.23f + .03f * r(tier)) * (1 - t * .86f);
                const int boughs = t > .8f ? 4 : 6;
                for (int i = 0; i < boughs; ++i)
                {
                    const float a = (static_cast<float>(i) + .5f * static_cast<float>(tier % 2) + .3f * r(40 + tier * 7 + i)) / static_cast<float>(boughs) * 2 * Pi;
                    const XMFLOAT3 out{std::cos(a), 0, std::sin(a)};
                    // the card lies along the bough, tilted down away from the trunk
                    const XMFLOAT3 at{out.x * reach * .55f, y - reach * .22f, out.z * reach * .55f};
                    const XMFLOAT3 facing{out.x * .35f, 1, out.z * .35f};
                    Spray(leaves, at, facing, reach * .62f, a + Pi * .25f, static_cast<int>(Hash(variant, tier * 8 + i, 11) & 3), {0, y - .3f, 0});
                }
            }
            Spray(leaves, {0, .96f, 0}, {1, .2f, 0}, .05f, 0, 0, {0, .8f, 0});
            Spray(leaves, {0, .96f, 0}, {0, .2f, 1}, .05f, 0, 1, {0, .8f, 0});
            Tree tree;
            if (!trunk.Upload(renderer, tree.Trunk, error) || !leaves.Upload(renderer, tree.Leaves, error)) return false;
            mConifer.push_back(std::move(tree));
        }
        return true;
    }

    // ================================================================ clouds
    // A dome in rings. Each ring is its own mesh so the rings near the horizon can be drawn fainter:
    // the cloud layer melts into the haze instead of ending at a line.
    bool Nature::BuildClouds(Renderer& renderer, std::string& error)
    {
        const XMFLOAT3 n{0, -1, 0};   // seen from below
        const float edges[] = {2.5f, 5, 8, 12, 17, 24, 33, 45, 60, 75, 90};
        const int segments = 48;
        for (int band = 0; band + 1 < static_cast<int>(std::size(edges)); ++band)
        {
            MeshBuilder mesh;
            for (int ring = 0; ring < 2; ++ring)
                for (int s = 0; s <= segments; ++s)
                {
                    const float azimuth = static_cast<float>(s) / segments * 2 * Pi, e = edges[band + ring] * Pi / 180;
                    const XMFLOAT3 d{std::sin(azimuth) * std::cos(e), std::sin(e), std::cos(azimuth) * std::cos(e)};
                    // as if the clouds were a flat sheet high above: they crowd together toward the horizon
                    const float flat = CloudTextureScale / (d.y + CloudFlatness);
                    mesh.Vertex(d, n, d.x * flat, d.z * flat);
                    if (ring && s) { const Doorways::u32 w = segments + 1, i = w + s; mesh.Quad(i - w - 1, i - w, i, i - 1); }
                }
            Doorways::DoorwaysGpuMesh gpu;
            if (!mesh.Upload(renderer, gpu, error)) return false;
            mCloudBands.push_back(std::move(gpu));
        }
        return true;
    }

    void Nature::DrawClouds(Renderer& renderer, XMFLOAT3 eye, const XMMATRIX& viewProjection) const
    {
        if (!mReady) return;
        const XMMATRIX world = XMMatrixScaling(CloudRadius, CloudRadius, CloudRadius) * XMMatrixTranslation(eye.x, eye.y, eye.z);
        const int bands = static_cast<int>(mCloudBands.size());
        for (int band = 0; band < bands; ++band)
        {
            const float rise = Smooth(static_cast<float>(band) + .5f, 0, 5.5f);    // faint at the horizon, full from about 25 degrees up
            Material m = Surface(mCloudTexture.Get(), {CloudBrightness, CloudBrightness * 1.03f, CloudBrightness * 1.1f, CloudCover * rise}, CloudEmission,
                                 Doorways::DoorwaysMaterialAlphaMode::Blend, false);
            m.TexTransform = {1, 1, mTime * CloudDriftU, mTime * CloudDriftV};
            renderer.DrawDoorwaysGpuMesh(mCloudBands[band], world, viewProjection, m);
        }
    }

    // ================================================================ draw
    void Nature::Draw(Renderer& renderer, XMFLOAT3 eye, const XMMATRIX& viewProjection)
    {
        if (!mReady) return;
        ++mFrame;

        // ---- trees: one candidate place in every 15 m square
        {
            const int reach = static_cast<int>(TreeDistance / TreeCell) + 1;
            const int ex = static_cast<int>(std::floor(eye.x / TreeCell)), ez = static_cast<int>(std::floor(eye.z / TreeCell));
            for (int cz = ez - reach; cz <= ez + reach; ++cz)
                for (int cx = ex - reach; cx <= ex + reach; ++cx)
                {
                    {   // cheap test first: most squares are out of sight
                        const float mx = (static_cast<float>(cx) + .5f) * TreeCell - eye.x, mz = (static_cast<float>(cz) + .5f) * TreeCell - eye.z;
                        if (mx * mx + mz * mz > (TreeDistance + TreeCell) * (TreeDistance + TreeCell)) continue;
                    }
                    float x = 0, z = 0, full = 0, girth = 0; int region = 0;
                    if (!TreeAt(cx, cz, x, z, region, full, girth)) continue;
                    const float dx = x - eye.x, dz = z - eye.z, distance = std::sqrt(dx * dx + dz * dz);
                    if (distance > TreeDistance) continue;
                    const Growth& growth = Regions[region];
                    const auto& kinds = growth.Conifer ? mConifer : mBroadleaf;
                    const Tree& tree = kinds[Hash(cx, cz, 24) % kinds.size()];
                    const float fade = Smooth(TreeDistance - distance, 0, 30);
                    const float tall = full * fade, wide = girth * fade;
                    const XMMATRIX world = XMMatrixScaling(wide, tall, wide) * XMMatrixRotationY(Unit(cx, cz, 27) * 2 * Pi) *
                                           XMMatrixTranslation(x, Height(x, z) - .1f, z);
                    renderer.DrawDoorwaysGpuMesh(tree.Trunk, world, viewProjection,
                                                 Surface(mBarkTexture.Get(), {NatureTint, NatureTint, NatureTint, 1}, .02f, Doorways::DoorwaysMaterialAlphaMode::Opaque));
                    const float shade = NatureTint * (.85f + .3f * Unit(cx, cz, 28));
                    renderer.DrawDoorwaysGpuMesh(tree.Leaves, world, viewProjection,
                                                 Surface(mLeafTexture.Get(), {growth.Leaf.x * shade, growth.Leaf.y * shade, growth.Leaf.z * shade, 1}, .03f,
                                                         Doorways::DoorwaysMaterialAlphaMode::Mask));
                }
        }

        // ---- grass: the squares round the camera; new ones are built a few per frame
        {
            const int reach = static_cast<int>(GrassDistance / GrassCell) + 1;
            const int ex = static_cast<int>(std::floor(eye.x / GrassCell)), ez = static_cast<int>(std::floor(eye.z / GrassCell));
            int built = 0;
            for (int ring = 0; ring <= reach; ++ring)          // nearest squares first
                for (int cz = ez - ring; cz <= ez + ring; ++cz)
                    for (int cx = ex - ring; cx <= ex + ring; ++cx)
                    {
                        if (std::max(std::abs(cx - ex), std::abs(cz - ez)) != ring) continue;
                        const float mx = (static_cast<float>(cx) + .5f) * GrassCell - eye.x, mz = (static_cast<float>(cz) + .5f) * GrassCell - eye.z;
                        if (mx * mx + mz * mz > (GrassDistance + GrassCell * .7f) * (GrassDistance + GrassCell * .7f)) continue;
                        const long long key = (static_cast<long long>(cx) << 32) ^ static_cast<unsigned>(cz);
                        auto found = mCells.find(key);
                        if (found == mCells.end())
                        {
                            if (built >= GrassCellsPerFrame) continue;
                            ++built;
                            found = mCells.emplace(key, Cell{}).first;
                            if (!BuildCell(renderer, cx, cz, found->second)) found->second.Empty = true;
                        }
                        Cell& cell = found->second;
                        cell.Used = mFrame;
                        if (cell.Empty) continue;
                        const Growth& growth = Regions[cell.Region];
                        renderer.DrawDoorwaysGpuMesh(cell.Mesh, XMMatrixIdentity(), viewProjection,
                                                     Surface(mGrassTexture.Get(), {growth.Tint.x * NatureTint, growth.Tint.y * NatureTint, growth.Tint.z * NatureTint, 1}, .03f,
                                                             Doorways::DoorwaysMaterialAlphaMode::Mask));
                    }
            if (mCells.size() > static_cast<size_t>(GrassCellLimit))     // squares she left behind are let go
                std::erase_if(mCells, [&](const auto& entry) { return mFrame - entry.second.Used > 240; });
        }
    }
}
