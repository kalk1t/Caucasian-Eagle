#define NOMINMAX
#include "AscentTrees.h"

#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRuntimePathResolver.h"
#include "Renderer.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace DirectX;

namespace CaucasianEagle
{
    namespace
    {
        constexpr float Pi = 3.14159265f;
        long long Key(int cx, int cz) { return (static_cast<long long>(cx) << 32) ^ static_cast<unsigned>(cz); }

        // the six planes of a view-projection (row vectors, D3D depth 0..w), each normalized: a sphere is seen when it is
        // not wholly behind any of them
        struct Frustum
        {
            XMFLOAT4 P[6];
            explicit Frustum(const XMMATRIX& vp)
            {
                XMFLOAT4X4 m; XMStoreFloat4x4(&m, vp);
                auto col = [&](int j) { return XMFLOAT4(m.m[0][j], m.m[1][j], m.m[2][j], m.m[3][j]); };
                const XMFLOAT4 c0 = col(0), c1 = col(1), c2 = col(2), c3 = col(3);
                const XMFLOAT4 raw[6] = {{c3.x + c0.x, c3.y + c0.y, c3.z + c0.z, c3.w + c0.w}, {c3.x - c0.x, c3.y - c0.y, c3.z - c0.z, c3.w - c0.w},
                                         {c3.x + c1.x, c3.y + c1.y, c3.z + c1.z, c3.w + c1.w}, {c3.x - c1.x, c3.y - c1.y, c3.z - c1.z, c3.w - c1.w},
                                         c2, {c3.x - c2.x, c3.y - c2.y, c3.z - c2.z, c3.w - c2.w}};
                for (int i = 0; i < 6; ++i)
                {
                    const float l = std::sqrt(raw[i].x * raw[i].x + raw[i].y * raw[i].y + raw[i].z * raw[i].z);
                    P[i] = l > 1e-9f ? XMFLOAT4(raw[i].x / l, raw[i].y / l, raw[i].z / l, raw[i].w / l) : raw[i];
                }
            }
            bool Sees(const XMFLOAT3& c, float r) const
            {
                for (const auto& p : P) if (p.x * c.x + p.y * c.y + p.z * c.z + p.w < -r) return false;
                return true;
            }
        };

        bool GameTreesWanted()
        {
            char value[8] = {}; size_t got = 0;
            return !(getenv_s(&got, value, sizeof(value), "CAUCASIAN_EAGLE_GAME_TREES") == 0 && got > 1 && value[0] == '0');
        }
    }

    bool AscentTrees::Initialize(Renderer& renderer, const std::filesystem::path& dataFile, std::string& error)
    {
        mReady = false; mTrees.clear(); mGroups.clear(); mBlocks.clear(); Merged = 0;
        if (!GameTreesWanted()) return true;
        auto plants = dataFile; plants.replace_extension(".plants");
        const auto folder = Doorways::ResolveDoorwaysRuntimePath("assets/Trees");
        if (!std::filesystem::exists(plants) || !std::filesystem::exists(folder / "trees.txt")) return true;   // a map that bakes its trees
        if (!LoadLibrary(renderer, folder, error)) { error = "trees: " + error; return false; }
        LoadPlants(plants);
        mBuckets.assign(Bucket(static_cast<int>(mModels.size()), 0, 0, 0), {});
        mReady = true;
        return true;
    }

    bool AscentTrees::LoadLibrary(Renderer& renderer, const std::filesystem::path& folder, std::string& error)
    {
        std::ifstream list(folder / "trees.txt");
        if (!list) { error = "cannot read " + (folder / "trees.txt").string(); return false; }
        auto texture = [&](const std::string& name, bool normal, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& out, bool required) -> bool
        {
            DoorwaysTextureLoadDesc desc; desc.Path = folder / "Textures" / name; desc.GenerateMips = true;
            desc.Semantic = normal ? Doorways::DoorwaysMaterialTextureSemantic::Normal : Doorways::DoorwaysMaterialTextureSemantic::BaseColor;
            if (!std::filesystem::exists(desc.Path)) { if (required) error = "missing " + desc.Path.string(); return !required; }
            DoorwaysTextureLoadResult result;
            if (!renderer.LoadDoorwaysTexture(desc, result, &error)) return false;
            out = result.SRV; return true;
        };
        auto part = [&](const std::string& file, Part& out) -> bool
        {
            const auto path = folder / "Models" / file;
            if (!std::filesystem::exists(path)) return true;   // (a bare tree has no leaves)
            Doorways::DoorwaysLoadedDMesh mesh;
            if (!Doorways::LoadDoorwaysDMesh(path, mesh, &error)) return false;
            if (mesh.Submeshes.empty() || mesh.Indices.empty()) return true;
            if (!renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(mesh, out.Mesh, &error)) return false;
            out.Sub = mesh.Submeshes[0]; out.Valid = true; return true;
        };
        std::string line;
        while (std::getline(list, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream in(line);
            std::string first; in >> first;
            if (first == "impostor")
            {
                std::string name; int v = 0, snow = 0; Impostor imp;
                in >> name >> v >> snow >> imp.U0 >> imp.V0 >> imp.U1 >> imp.V1 >> imp.W >> imp.H;
                for (auto& m : mModels)
                    if (m.Name == name && v >= 0 && v < static_cast<int>(m.Variants.size()) && (snow == 0 || snow == 1))
                    {
                        m.Variants[v].Far[snow] = imp;
                        if (!BuildImpostor(renderer, m.Variants[v].Far[snow])) { error = "impostor " + name; return false; }
                    }
                continue;
            }
            Model model; model.Name = first;
            std::string kind; int variants = 0, conifer = 0, snow = 0;
            in >> kind >> variants >> conifer >> snow;
            if (variants <= 0 || variants > 4) continue;
            model.Conifer = conifer != 0; model.Snow = snow != 0;
            auto found = std::find_if(mKinds.begin(), mKinds.end(), [&](const Kind& k) { return k.Name == kind; });
            if (found == mKinds.end())
            {
                Kind k; k.Name = kind;
                if (!texture(kind + "_bark.png", false, k.Bark, true) || !texture(kind + "_bark_n.png", true, k.BarkNormal, false) ||
                    !texture(kind + "_leaves.png", false, k.Leaves, false) || !texture(kind + "_leaves_snow.png", false, k.LeavesSnow, false)) return false;
                mKinds.push_back(std::move(k)); found = mKinds.end() - 1;
            }
            model.Kind = static_cast<int>(found - mKinds.begin());
            model.Variants.resize(variants);
            for (int v = 0; v < variants; ++v)
            {
                in >> model.Variants[v].Height;
                if (!(model.Variants[v].Height > .1f)) model.Variants[v].Height = 1;
                for (int lod = 0; lod < 4; ++lod)
                    for (int p = 0; p < 2; ++p)
                        if (!part(model.Name + "_" + std::to_string(v) + "_" + std::to_string(lod) + (p ? "_leaves" : "_bark") + ".dmesh", model.Variants[v].Lod[lod][p])) return false;
            }
            mModels.push_back(std::move(model));
        }
        if (mModels.empty()) { error = "trees.txt lists no models"; return false; }
        return texture("impostors.png", false, mImpostors, false);
    }

    bool AscentTrees::BuildImpostor(Renderer& renderer, Impostor& imp)
    {
        // three upright quads through the trunk, 60 degrees apart, each the tree's picture (W x H m, its foot at the bottom's middle)
        Doorways::DoorwaysLoadedDMesh mesh;
        for (int k = 0; k < 3; ++k)
        {
            const float a = static_cast<float>(k) * Pi / 3, dx = std::cos(a) * imp.W * .5f, dz = std::sin(a) * imp.W * .5f;
            const Doorways::u32 base = static_cast<Doorways::u32>(mesh.Vertices.size());
            const float pos[4][3] = {{-dx, 0, -dz}, {dx, 0, dz}, {dx, imp.H, dz}, {-dx, imp.H, -dz}};
            const float uv[4][2] = {{imp.U0, imp.V1}, {imp.U1, imp.V1}, {imp.U1, imp.V0}, {imp.U0, imp.V0}};
            for (int i = 0; i < 4; ++i)
            {
                Doorways::DoorwaysDMeshVertex v{};
                v.Position = {pos[i][0], pos[i][1], pos[i][2]};
                // lit as a crown: out from its middle, mostly up
                const float nx = pos[i][0] / std::max(imp.W, .1f), ny = (pos[i][1] - imp.H * .45f) / std::max(imp.H, .1f) + .6f, nz = pos[i][2] / std::max(imp.W, .1f);
                const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
                v.Normal = {nx / nl, ny / nl, nz / nl}; v.Tangent = {std::cos(a), 0, std::sin(a), 1};
                v.TexCoord0 = {uv[i][0], uv[i][1]};
                mesh.Vertices.push_back(v);
            }
            mesh.Indices.insert(mesh.Indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
        mesh.Submeshes = {{0, static_cast<Doorways::u32>(mesh.Indices.size()), 0, 0}};
        mesh.MeshHeader.VertexCount = static_cast<Doorways::u32>(mesh.Vertices.size());
        mesh.MeshHeader.IndexCount = static_cast<Doorways::u32>(mesh.Indices.size());
        mesh.MeshHeader.SubmeshCount = 1; mesh.MeshHeader.VertexStride = sizeof(Doorways::DoorwaysDMeshVertex);
        std::string error;
        imp.Valid = renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(mesh, imp.Mesh, &error);
        return imp.Valid;
    }

    void AscentTrees::LoadPlants(const std::filesystem::path& file)
    {
        std::ifstream in(file);
        std::string line;
        std::unordered_map<std::string, int> byName;
        for (size_t i = 0; i < mModels.size(); ++i) byName[mModels[i].Name] = static_cast<int>(i);
        std::unordered_map<long long, int> squareGroup[2];          // a square's group of each size
        auto floorDiv = [](int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
        auto add = [](Group& g, unsigned index, float reach, const XMFLOAT3& lo, const XMFLOAT3& hi)
        {
            g.Trees.push_back(index); g.Reach = std::max(g.Reach, reach);
            g.Min = {std::min(g.Min.x, lo.x), std::min(g.Min.y, lo.y), std::min(g.Min.z, lo.z)};
            g.Max = {std::max(g.Max.x, hi.x), std::max(g.Max.y, hi.y), std::max(g.Max.z, hi.z)};
        };
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream s(line);
            std::string name; int v = 0, snow = 0; float x = 0, y = 0, z = 0, h = 0, yaw = 0, lx = 0, lz = 0;
            if (!(s >> name >> v >> x >> y >> z >> h >> yaw >> lx >> lz >> snow)) continue;
            const auto m = byName.find(name);
            if (m == byName.end() || h <= 0) continue;
            const Model& model = mModels[m->second];
            v = std::clamp(v, 0, static_cast<int>(model.Variants.size()) - 1);
            Tree t;
            t.Model = static_cast<unsigned short>(m->second); t.Variant = static_cast<unsigned char>(v);
            t.Snow = static_cast<unsigned char>(snow && model.Snow ? 1 : 0);
            t.Scale = h / model.Variants[v].Height;
            // foot at (x, y, z), turned by yaw, leaning out (a tree on a rock face: x += lx per metre up, as the old forest)
            const XMMATRIX lean(1, 0, 0, 0, lx, 1, lz, 0, 0, 0, 1, 0, 0, 0, 0, 1);
            const XMMATRIX world = XMMatrixScaling(t.Scale, t.Scale, t.Scale) * XMMatrixRotationY(yaw) * lean * XMMatrixTranslation(x, y, z);
            XMStoreFloat4x4(&t.World, world);
            XMStoreFloat4x4(&t.WorldInvTranspose, XMMatrixTranspose(XMMatrixInverse(nullptr, world)));
            const Impostor& farPic = model.Variants[v].Far[0];
            const float tall = h, wide = farPic.Valid ? farPic.W * t.Scale : h * .6f;
            t.Centre = {x + lx * tall * .5f, y + tall * .5f, z + lz * tall * .5f};
            t.Radius = .5f * std::sqrt(tall * tall + wide * wide);
            t.Reach = std::clamp(t.Radius, 8.f, 30.f);
            const unsigned index = static_cast<unsigned>(mTrees.size());
            mTrees.push_back(t);
            // its box: from the foot to the (leaning) top, the crown's width each side
            const XMFLOAT3 lo{std::min(x, x + lx * tall) - wide, y, std::min(z, z + lz * tall) - wide};
            const XMFLOAT3 hi{std::max(x, x + lx * tall) + wide, y + tall, std::max(z, z + lz * tall) + wide};
            const int sx = static_cast<int>(std::floor(x / Cell)), sz = static_cast<int>(std::floor(z / Cell));
            Block& block = mBlocks[Key(floorDiv(sx, BlockSquares), floorDiv(sz, BlockSquares))];
            if (block.All < 0) { block.All = static_cast<int>(mGroups.size()); mGroups.emplace_back(); }
            const int size = t.Reach > BigReach ? 1 : 0;
            const auto found = squareGroup[size].find(Key(sx, sz));
            int g = 0;
            if (found != squareGroup[size].end()) g = found->second;
            else { g = static_cast<int>(mGroups.size()); mGroups.emplace_back(); squareGroup[size][Key(sx, sz)] = g; block.Parts.push_back(g); }
            add(mGroups[static_cast<size_t>(g)], index, t.Reach, lo, hi);
            add(mGroups[static_cast<size_t>(block.All)], index, t.Reach, lo, hi);
        }
    }

    void AscentTrees::BuildFar(Renderer& renderer, Group& group)
    {
        // every tree of the group as its picture on two upright quads through its trunk, in world space, one mesh (a tree
        // merged is under ~25 px: two quads are enough, and hold a third less than three)
        group.FarBuilt = true; group.FarTrees = 0; group.LastDrawn = mFrame;
        Doorways::DoorwaysLoadedDMesh mesh;
        mesh.Vertices.reserve(group.Trees.size() * 8); mesh.Indices.reserve(group.Trees.size() * 12);
        for (const unsigned i : group.Trees)
        {
            const Tree& t = mTrees[i];
            const Variant& var = mModels[t.Model].Variants[t.Variant];
            const Impostor& imp = var.Far[t.Snow].Valid ? var.Far[t.Snow] : var.Far[0];
            if (!imp.Valid) continue;
            const XMMATRIX world = XMLoadFloat4x4(&t.World), normals = XMLoadFloat4x4(&t.WorldInvTranspose);
            for (int k = 0; k < 2; ++k)
            {
                const float a = static_cast<float>(k) * Pi / 2, dx = std::cos(a) * imp.W * .5f, dz = std::sin(a) * imp.W * .5f;
                const Doorways::u32 base = static_cast<Doorways::u32>(mesh.Vertices.size());
                const float pos[4][3] = {{-dx, 0, -dz}, {dx, 0, dz}, {dx, imp.H, dz}, {-dx, imp.H, -dz}};
                const float uv[4][2] = {{imp.U0, imp.V1}, {imp.U1, imp.V1}, {imp.U1, imp.V0}, {imp.U0, imp.V0}};
                for (int q = 0; q < 4; ++q)
                {
                    // lit as a crown: out from its middle, mostly up
                    const float nx = pos[q][0] / std::max(imp.W, .1f), ny = (pos[q][1] - imp.H * .45f) / std::max(imp.H, .1f) + .6f, nz = pos[q][2] / std::max(imp.W, .1f);
                    XMFLOAT3 p, n;
                    XMStoreFloat3(&p, XMVector3TransformCoord(XMVectorSet(pos[q][0], pos[q][1], pos[q][2], 1), world));
                    XMStoreFloat3(&n, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(nx, ny, nz, 0), normals)));
                    Doorways::DoorwaysDMeshVertex v{};
                    v.Position = {p.x, p.y, p.z}; v.Normal = {n.x, n.y, n.z}; v.Tangent = {1, 0, 0, 1}; v.TexCoord0 = {uv[q][0], uv[q][1]};
                    mesh.Vertices.push_back(v);
                }
                mesh.Indices.insert(mesh.Indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
            }
            ++group.FarTrees;
        }
        if (mesh.Indices.empty()) { group.FarTrees = 0; return; }
        mesh.Submeshes = {{0, static_cast<Doorways::u32>(mesh.Indices.size()), 0, 0}};
        mesh.MeshHeader.VertexCount = static_cast<Doorways::u32>(mesh.Vertices.size());
        mesh.MeshHeader.IndexCount = static_cast<Doorways::u32>(mesh.Indices.size());
        mesh.MeshHeader.SubmeshCount = 1; mesh.MeshHeader.VertexStride = sizeof(Doorways::DoorwaysDMeshVertex);
        std::string error;
        if (!renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(mesh, group.FarMesh, &error)) { group.FarTrees = 0; return; }
        Merged += group.FarTrees;
    }

    void AscentTrees::Gather(const XMFLOAT3& eye, const XMMATRIX& viewProjection, bool shadow)
    {
        // sorts the trees to draw one by one into their buckets (model, variant, level, snow); the groups wholly of
        // pictures into mFar (not for the shadows: only trees within ShadowReach cast one, each by its 3D model)
        for (auto& b : mBuckets) b.clear();
        mFar.clear();
        const Frustum frustum(viewProjection);
        auto distance = [&](const Group& g)   // from the eye to the group's box
        {
            const float dx = std::max({g.Min.x - eye.x, 0.f, eye.x - g.Max.x}), dy = std::max({g.Min.y - eye.y, 0.f, eye.y - g.Max.y}),
                        dz = std::max({g.Min.z - eye.z, 0.f, eye.z - g.Max.z});
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        };
        auto sees = [&](const Group& g)
        {
            const XMFLOAT3 c{(g.Min.x + g.Max.x) * .5f, (g.Min.y + g.Max.y) * .5f, (g.Min.z + g.Max.z) * .5f};
            const float ex = g.Max.x - g.Min.x, ey = g.Max.y - g.Min.y, ez = g.Max.z - g.Min.z;
            return frustum.Sees(c, .5f * std::sqrt(ex * ex + ey * ey + ez * ez));
        };
        for (const auto& [key, block] : mBlocks)
        {
            const Group& all = mGroups[static_cast<size_t>(block.All)];
            const float bd = distance(all);
            if ((shadow && bd > ShadowReach) || !sees(all)) continue;
            // every tree of the block a picture (seen from its nearest corner): the block merged
            if (!shadow && bd * 14.f / all.Reach >= Picture) { mFar.emplace_back(bd, block.All); continue; }
            for (const int part : block.Parts)
            {
                const Group& g = mGroups[static_cast<size_t>(part)];
                const float gd = distance(g);
                if ((shadow && gd > ShadowReach) || !sees(g)) continue;
                if (!shadow && gd * 14.f / g.Reach >= Picture) { mFar.emplace_back(gd, part); continue; }
                for (const unsigned i : g.Trees)
                {
                    const Tree& t = mTrees[i];
                    const float ex = t.Centre.x - eye.x, ey = t.Centre.y - eye.y, ez = t.Centre.z - eye.z;
                    const float d = std::sqrt(ex * ex + ey * ey + ez * ez);
                    if ((shadow && d > ShadowReach + t.Radius) || !frustum.Sees(t.Centre, t.Radius)) continue;
                    // the level by how large it looks: a tall tree keeps its detail further, a shrub loses it sooner
                    const float seen = d * 14.f / t.Reach;
                    int lod = seen < Detail[0] ? 0 : seen < Detail[1] ? 1 : seen < Detail[2] ? 2 : seen < Picture ? 3 : 4;
                    if (shadow) lod = std::clamp(lod, 1, 2);
                    mBuckets[Bucket(t.Model, t.Variant, lod, t.Snow)].push_back(i);
                }
            }
        }
    }

    void AscentTrees::Draw(Renderer& renderer, const XMFLOAT3& eye, const XMMATRIX& viewProjection)
    {
        for (int& d : Drawn) d = 0;
        Draws = 0; FarGroups = 0; Built = 0; ++mFrame;
        if (!On()) return;
        using Clock = std::chrono::steady_clock;
        const auto t0 = Clock::now();
        Gather(eye, viewProjection, false);
        const auto t1 = Clock::now();
        Material bark{}, leaves{}, farPic{};
        bark.TexTransform = {1, 1, 0, 0}; bark.MetallicFactor = 0; bark.RoughnessFactor = .92f; bark.SpecularColor = {.04f, .04f, .04f, 1}; bark.ReceiveFog = true;
        leaves = bark; leaves.RoughnessFactor = .8f; leaves.SpecularColor = {.06f, .06f, .06f, 1};
        leaves.AlphaMode = Doorways::DoorwaysMaterialAlphaMode::Mask; leaves.AlphaCutoff = .5f; leaves.DoubleSided = true;
        farPic = leaves; farPic.DiffuseMap = mImpostors.Get(); farPic.AlphaCutoff = .45f; farPic.RoughnessFactor = .95f; farPic.SpecularColor = {0, 0, 0, 1};
        for (size_t m = 0; m < mModels.size(); ++m)
        {
            const Model& model = mModels[m]; const Kind& kind = mKinds[model.Kind];
            bark.DiffuseMap = kind.Bark.Get(); bark.NormalMap = kind.BarkNormal.Get();
            for (size_t v = 0; v < model.Variants.size(); ++v)
                for (int lod = 0; lod < 5; ++lod)
                    for (int snow = 0; snow < 2; ++snow)
                    {
                        const auto& ids = mBuckets[Bucket(static_cast<int>(m), static_cast<int>(v), lod, snow)];
                        if (ids.empty()) continue;
                        mInstances.clear(); mInstances.reserve(ids.size());
                        for (const unsigned i : ids)
                        {
                            const Tree& t = mTrees[i];
                            DoorwaysDMeshInstanceGpuData d;
                            XMStoreFloat4x4(&d.WorldViewProj, XMLoadFloat4x4(&t.World) * viewProjection);
                            d.World = t.World; d.WorldInvTranspose = t.WorldInvTranspose;
                            mInstances.push_back(d);
                        }
                        const Variant& var = model.Variants[v];
                        if (lod == 4)
                        {
                            const Impostor& imp = var.Far[snow].Valid ? var.Far[snow] : var.Far[0];
                            if (!imp.Valid || !mImpostors) continue;
                            renderer.BeginDoorwaysMaskedPass();
                            if (renderer.DrawDoorwaysGpuSubmeshInstanced(imp.Mesh, imp.Mesh.Submeshes[0], mInstances.data(), mInstances.size(), farPic, false)) ++Draws;
                        }
                        else
                        {
                            const Part& b = var.Lod[lod][0]; const Part& l = var.Lod[lod][1];
                            // (each draw opens its pass: the instanced call sets no blend state of its own, and an opaque
                            // bark drawn in whatever pass came before was blended with the sky behind it: blue trunks)
                            renderer.BeginDoorwaysOpaquePass();
                            if (b.Valid && renderer.DrawDoorwaysGpuSubmeshInstanced(b.Mesh, b.Sub, mInstances.data(), mInstances.size(), bark, false)) ++Draws;
                            renderer.BeginDoorwaysMaskedPass();
                            leaves.DiffuseMap = snow && kind.LeavesSnow ? kind.LeavesSnow.Get() : kind.Leaves.Get();
                            if (l.Valid && leaves.DiffuseMap && renderer.DrawDoorwaysGpuSubmeshInstanced(l.Mesh, l.Sub, mInstances.data(), mInstances.size(), leaves, false)) ++Draws;
                        }
                        Drawn[lod] += static_cast<int>(ids.size());
                    }
        }
        const auto t2 = Clock::now();
        // the groups of pictures, nearest first: one draw each; those not merged yet, merged a few thousand trees a frame
        std::sort(mFar.begin(), mFar.end());
        int budget = BuildTreesPerFrame; bool masked = false;
        for (const auto& [distance, g] : mFar)
        {
            Group& group = mGroups[static_cast<size_t>(g)];
            if (!group.FarBuilt)
            {
                if (budget <= 0) continue;
                budget -= static_cast<int>(group.Trees.size()); Built += static_cast<int>(group.Trees.size());
                BuildFar(renderer, group);
            }
            if (group.FarTrees <= 0 || !mImpostors) continue;
            if (!masked) { renderer.BeginDoorwaysMaskedPass(); masked = true; }
            renderer.DrawDoorwaysGpuMesh(group.FarMesh, XMMatrixIdentity(), viewProjection, farPic);
            group.LastDrawn = mFrame;
            Drawn[4] += group.FarTrees; ++FarGroups; ++Draws;
        }
        // a merged mesh not drawn for a while is let go (a block or a square left far behind, or now near)
        if (mFrame % 120 == 0)
            for (Group& group : mGroups)
                if (group.FarBuilt && mFrame - group.LastDrawn > KeepFrames)
                {
                    Merged -= group.FarTrees;
                    group.FarMesh = Doorways::DoorwaysGpuMesh{}; group.FarTrees = 0; group.FarBuilt = false;
                }
        renderer.ResetDefaultWorldRenderState();
        const auto t3 = Clock::now();
        GatherMs = std::chrono::duration<double, std::milli>(t1 - t0).count(); NearMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
        FarMs = std::chrono::duration<double, std::milli>(t3 - t2).count();
    }

    void AscentTrees::DrawShadows(Renderer& renderer, const XMFLOAT3& eye, const XMMATRIX& lightViewProjection)
    {
        Shadowed = 0;
        if (!On()) return;
        Gather(eye, lightViewProjection, true);
        Material bark{}, leaves{};
        bark.TexTransform = {1, 1, 0, 0};
        leaves = bark; leaves.AlphaMode = Doorways::DoorwaysMaterialAlphaMode::Mask; leaves.AlphaCutoff = .5f; leaves.DoubleSided = true;
        for (size_t m = 0; m < mModels.size(); ++m)
        {
            const Model& model = mModels[m]; const Kind& kind = mKinds[model.Kind];
            for (size_t v = 0; v < model.Variants.size(); ++v)
                for (int lod = 1; lod < 3; ++lod)
                    for (int snow = 0; snow < 2; ++snow)
                    {
                        const auto& ids = mBuckets[Bucket(static_cast<int>(m), static_cast<int>(v), lod, snow)];
                        if (ids.empty()) continue;
                        mInstances.clear();
                        for (const unsigned i : ids) { DoorwaysDMeshInstanceGpuData d; d.WorldViewProj = mTrees[i].World; d.World = mTrees[i].World; d.WorldInvTranspose = mTrees[i].WorldInvTranspose; mInstances.push_back(d); }
                        const Part& b = model.Variants[v].Lod[lod][0]; const Part& l = model.Variants[v].Lod[lod][1];
                        leaves.DiffuseMap = kind.Leaves.Get();
                        auto cast = [&](const Part& p, const Material& mat, bool masked)
                        {
                            if (!p.Valid) return;
                            if (renderer.DrawDoorwaysGpuSubmeshShadowInstanced(p.Mesh, p.Sub, mInstances.data(), mInstances.size(), lightViewProjection, mat, masked)) return;
                            for (const auto& d : mInstances)   // (no instanced shadow shader: one by one)
                                renderer.DrawDoorwaysGpuSubmeshShadow(p.Mesh, p.Sub, XMLoadFloat4x4(&d.World), lightViewProjection, mat, masked);
                        };
                        cast(b, bark, false);
                        if (leaves.DiffuseMap) cast(l, leaves, true);
                        Shadowed += static_cast<int>(ids.size());
                    }
        }
    }

    std::string AscentTrees::Report() const
    {
        std::ostringstream s;
        s << "trees=" << mTrees.size() << "\tmodels=" << mModels.size() << "\tgroups=" << mGroups.size() << "\tblocks=" << mBlocks.size()
          << "\tnear=" << Drawn[0] << "/" << Drawn[1] << "/" << Drawn[2] << "/" << Drawn[3] << "\tpictures=" << Drawn[4] << "\tfar_groups=" << FarGroups
          << "\tmerged=" << Merged << "\tdraws=" << Draws << "\tshadowed=" << Shadowed << std::fixed << std::setprecision(2) << "\tgather_ms=" << GatherMs
          << "\tnear_ms=" << NearMs << "\tfar_ms=" << FarMs << "\tbuilt=" << Built;
        return s.str();
    }
}
