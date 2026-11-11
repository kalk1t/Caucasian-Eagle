#define NOMINMAX
#include "AmiraniMountain.h"

#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRuntimePathResolver.h"
#include "EnvironmentSettings.h"
#include "GameConstants.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace DirectX;

namespace CaucasianEagle
{
    namespace
    {
        constexpr float Pi = 3.14159265f;

        float Smooth(float x, float a, float b)
        {
            const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
            return t * t * (3 - 2 * t);
        }

        XMVECTOR MoonlightDirection()
        {
            return XMVector3Normalize(XMVectorSet(MoonlightX, MoonlightY, MoonlightZ, 0));
        }

        XMVECTOR MoonDirection() // from the viewer toward the moon
        {
            return XMVectorSet(std::sin(MoonAzimuth) * std::cos(MoonElevation), std::sin(MoonElevation),
                               std::cos(MoonAzimuth) * std::cos(MoonElevation), 0);
        }

        Material Surface(ID3D11ShaderResourceView* texture, XMFLOAT4 tint, float emission, bool fog = true)
        {
            Material m{};
            m.Diffuse = tint; m.DiffuseMap = texture;
            m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .9f;
            m.SpecularColor = {0, 0, 0, 0}; m.EmissiveStrength = emission;
            m.DoubleSided = true; m.ReceiveFog = fog;
            return m;
        }

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

        bool LoadTexture(Renderer& renderer, const std::filesystem::path& path,
                         Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& out, std::string& error)
        {
            DoorwaysTextureLoadDesc desc;
            desc.Path = path;
            desc.Semantic = Doorways::DoorwaysMaterialTextureSemantic::BaseColor;
            desc.GenerateMips = true;
            DoorwaysTextureLoadResult result;
            if (!renderer.LoadDoorwaysTexture(desc, result, &error)) return false;
            out = result.SRV;
            return true;
        }

        // Unit box stretched from a to b (length along its local Z), 'wide' by 'thin' across.
        XMMATRIX Link(XMFLOAT3 a, XMFLOAT3 b, float wide, float thin, bool turned)
        {
            const XMVECTOR va = XMLoadFloat3(&a), vb = XMLoadFloat3(&b);
            const XMVECTOR forward = XMVector3Normalize(vb - va);
            XMVECTOR up = XMVectorSet(0, 1, 0, 0);
            if (std::abs(XMVectorGetY(forward)) > .95f) up = XMVectorSet(1, 0, 0, 0);
            const XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
            up = XMVector3Cross(forward, right);
            XMMATRIX basis = XMMatrixIdentity();
            basis.r[0] = right; basis.r[1] = up; basis.r[2] = forward;
            const float length = XMVectorGetX(XMVector3Length(vb - va));
            return XMMatrixScaling(turned ? thin : wide, turned ? wide : thin, length) * basis *
                   XMMatrixTranslationFromVector((va + vb) * .5f);
        }
    }

    AmiraniMountain& GetAmiraniMountain()
    {
        static AmiraniMountain mountain;
        return mountain;
    }

    bool AmiraniMountain::Load(Renderer& renderer, const char* name, Model& out, std::string& error)
    {
        const auto root = Doorways::ResolveDoorwaysRuntimePath(LandmarkFolder);
        Doorways::DoorwaysLoadedDMesh mesh;
        if (!Doorways::LoadDoorwaysDMesh(root / (std::string(name) + ".dmesh"), mesh, &error)) return false;
        if (mesh.Vertices.empty()) { error = std::string(name) + ".dmesh has no vertices"; return false; }
        out.Min = {1e9f, 1e9f, 1e9f}; out.Max = {-1e9f, -1e9f, -1e9f};
        for (const auto& v : mesh.Vertices)
        {
            out.Min.x = std::min(out.Min.x, v.Position.X); out.Max.x = std::max(out.Max.x, v.Position.X);
            out.Min.y = std::min(out.Min.y, v.Position.Y); out.Max.y = std::max(out.Max.y, v.Position.Y);
            out.Min.z = std::min(out.Min.z, v.Position.Z); out.Max.z = std::max(out.Max.z, v.Position.Z);
        }
        if (&out == &mMountain)
        {   // The importer may mirror Z. Find the flat cave ledge and turn it toward the play area (-Z).
            double sum = 0; int count = 0;
            for (const auto& v : mesh.Vertices)
                if (std::abs(v.Position.Y - LedgeHeight) < 1.0f && std::abs(v.Position.X) < LedgeHalfWidth * .8f)
                { sum += v.Position.Z; ++count; }
            mMountainYaw = (count > 0 && sum / count > 0) ? Pi : 0;
        }
        if (!renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(mesh, out.Mesh, &error)) return false;
        return LoadTexture(renderer, root / (std::string(name) + "BaseColor.png"), out.Texture, error);
    }

    bool AmiraniMountain::BuildSky(Renderer& renderer, std::string& error)
    {
        // Every dome normal points along the moonlight, so the engine's directional light adds
        // nothing to it and its ambient term is the same everywhere: the painting shows as painted.
        XMFLOAT3 n; XMStoreFloat3(&n, MoonlightDirection());

        // Dome: unit radius, rings by elevation. The texture's lower fifth (ridges) is squeezed
        // into the lowest 9 degrees, its upper half (deep sky) fills everything above 40 degrees.
        struct Ring { float Elevation, V; };
        const Ring rings[] = {{-35, .999f}, {-6, .999f}, {0, .985f}, {2, .945f}, {4, .905f}, {6.5f, .86f}, {9, .82f}, {13, .76f},
                              {18, .70f}, {24, .64f}, {31, .58f}, {40, .50f}, {50, .40f}, {60, .29f}, {70, .18f}, {80, .08f}, {90, .0f}};
        const int segments = 64, ringCount = static_cast<int>(std::size(rings));
        MeshBuilder dome;
        for (int r = 0; r < ringCount; ++r)
            for (int s = 0; s <= segments; ++s)
            {
                const float u = static_cast<float>(s) / segments, azimuth = u * 2 * Pi, e = rings[r].Elevation * Pi / 180;
                dome.Vertex({std::sin(azimuth) * std::cos(e), std::sin(e), std::cos(azimuth) * std::cos(e)}, n, u * SkyTextureWraps, rings[r].V);
                if (r && s) { const Doorways::u32 w = segments + 1, i = r * w + s; dome.Quad(i - w - 1, i - w, i, i - 1); }
            }
        if (!dome.Upload(renderer, mDome, error)) return false;

        MeshBuilder moon; // unit sphere, equirectangular UV; dome normals, so it shows as a full moon
        const int mr = 24, ms = 48;
        for (int y = 0; y <= mr; ++y)
            for (int x = 0; x <= ms; ++x)
            {
                const float v = static_cast<float>(y) / mr, u = static_cast<float>(x) / ms, th = v * Pi, ph = u * 2 * Pi;
                const XMFLOAT3 p{std::sin(th) * std::sin(ph), std::cos(th), std::sin(th) * std::cos(ph)};
                moon.Vertex(p, n, u, v);
                if (y && x) { const Doorways::u32 w = ms + 1, i = y * w + x; moon.Quad(i - w - 1, i - w, i, i - 1); }
            }
        if (!moon.Upload(renderer, mMoon, error)) return false;

        MeshBuilder quad; // unit quad in XY, centred
        quad.Vertex({-.5f, -.5f, 0}, n, 0, 1); quad.Vertex({-.5f, .5f, 0}, n, 0, 0);
        quad.Vertex({.5f, .5f, 0}, n, 1, 0); quad.Vertex({.5f, -.5f, 0}, n, 1, 1);
        quad.Quad(0, 1, 2, 3);
        if (!quad.Upload(renderer, mQuad, error)) return false;

        return LoadTexture(renderer, Doorways::ResolveDoorwaysRuntimePath(SkyTexture), mSkyTexture, error) &&
               LoadTexture(renderer, Doorways::ResolveDoorwaysRuntimePath(MoonTexture), mMoonTexture, error) &&
               LoadTexture(renderer, Doorways::ResolveDoorwaysRuntimePath(MoonGlowTexture), mGlowTexture, error);
    }

    bool AmiraniMountain::Initialize(Renderer& renderer, std::string& error)
    {
        mReady = BuildSky(renderer, error) && Load(renderer, "Mountain", mMountain, error) &&
                 Load(renderer, "Amirani", mAmirani, error) && Load(renderer, "Eagle", mEagle, error) &&
                 Load(renderer, "Dog", mDog, error);
        return mReady;
    }

    void AmiraniMountain::Update(float realDeltaTime)
    {
        mTime += realDeltaTime;
    }

    XMMATRIX AmiraniMountain::Place(const Model& model, float height, float yaw, XMFLOAT3 feet, float pitch)
    {
        const float scale = height / std::max(model.Max.y - model.Min.y, 1e-4f);
        return XMMatrixTranslation(-(model.Min.x + model.Max.x) * .5f, -model.Min.y, -(model.Min.z + model.Max.z) * .5f) *
               XMMatrixScaling(scale, scale, scale) * XMMatrixRotationX(pitch) * XMMatrixRotationY(yaw) *
               XMMatrixTranslation(feet.x, feet.y, feet.z);
    }

    void AmiraniMountain::DrawSky(Renderer& renderer, XMFLOAT3 eye, const XMMATRIX& viewProjection) const
    {
        // The dome and the moon travel with the camera: they are infinitely far away.
        const XMMATRIX centre = XMMatrixTranslation(eye.x, eye.y, eye.z);
        renderer.DrawDoorwaysGpuMesh(mDome, XMMatrixScaling(SkyRadius, SkyRadius, SkyRadius) * centre, viewProjection,
                                     Surface(mSkyTexture.Get(), {SkyBrightness, SkyBrightness, SkyBrightness, 1}, SkyEmission, false));

        const XMVECTOR toMoon = MoonDirection();
        const float distance = SkyRadius * .93f, radius = distance * std::tan(MoonAngularSize * .5f);
        const XMVECTOR position = XMLoadFloat3(&eye) + toMoon * distance;

        // Halo: a camera-facing quad just behind the moon.
        const XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), toMoon));
        const XMVECTOR up = XMVector3Cross(toMoon, right);
        XMMATRIX facing = XMMatrixIdentity();
        facing.r[0] = right; facing.r[1] = up; facing.r[2] = toMoon;
        Material halo = Surface(mGlowTexture.Get(), {MoonHaloR, MoonHaloG, MoonHaloB, MoonHaloAlpha}, 1, false);
        halo.AlphaMode = Doorways::DoorwaysMaterialAlphaMode::Blend;
        const float haloSize = radius * 2 * MoonHaloSize;
        renderer.BindDoorwaysMaterialTexture(halo);
        renderer.SetAlphaBlendingEnabled(true);
        renderer.SetDepthWriteEnabled(false);
        for (const auto& submesh : mQuad.Submeshes)
            renderer.DrawDoorwaysGpuSubmesh(mQuad, submesh, XMMatrixScaling(haloSize, haloSize, 1) * facing *
                                            XMMatrixTranslationFromVector(position + toMoon * (radius * 1.5f)), viewProjection, halo, false, true);
        renderer.ResetDefaultWorldRenderState();
        renderer.BindDoorwaysSkinningDisabled();

        renderer.DrawDoorwaysGpuMesh(mMoon, XMMatrixScaling(radius, radius, radius) * XMMatrixRotationY(MoonFaceYaw) *
                                     XMMatrixTranslationFromVector(position), viewProjection,
                                     Surface(mMoonTexture.Get(), {MoonBrightness, MoonBrightness * 1.04f, MoonBrightness * 1.02f, 1}, MoonEmission, false));
    }

    void AmiraniMountain::Draw(Renderer& renderer, XMFLOAT3 eye, const XMMATRIX& viewProjection) const
    {
        if (!mReady) return;
        renderer.ResetDefaultWorldRenderState();
        renderer.BindDoorwaysSkinningDisabled();
        DrawSky(renderer, eye, viewProjection);

        const XMFLOAT3 base{MountainX, MountainY, MountainZ};
        const float ledgeY = base.y + LedgeHeight;
        const float wallZ = base.z + LedgeBackZ;                 // back wall of the cave
        const XMFLOAT3 feet{base.x, ledgeY, wallZ - AmiraniFromWall};

        // The mountain must be in view from every region of the road, so the regions' fog does not touch it.
        // The game dims it with distance instead (the fog colours are night colours, so dimming reads the same).
        const float away = std::sqrt((eye.x - base.x) * (eye.x - base.x) + (eye.z - base.z) * (eye.z - base.z));
        const float haze = 1 - MountainFarDim * Smooth(away, MountainDimStart, MountainDimEnd);
        const auto dim = [haze](float r, float g, float b) { return XMFLOAT4{r * haze, g * haze, b * haze, 1}; };

        renderer.DrawDoorwaysGpuMesh(mMountain.Mesh, XMMatrixRotationY(mMountainYaw) * XMMatrixTranslation(base.x, base.y, base.z),
                                     viewProjection, Surface(mMountain.Texture.Get(), dim(MountainTint, MountainTint, MountainTint), MountainEmission, false));

        // Amirani: breathing is the only motion a chained titan has left.
        const float breath = 1 + .006f * std::sin(mTime * .9f);
        renderer.DrawDoorwaysGpuMesh(mAmirani.Mesh, Place(mAmirani, AmiraniHeight * breath, AmiraniYaw, feet), viewProjection,
                                     Surface(mAmirani.Texture.Get(), dim(1.15f, 1.1f, 1.05f), FigureEmission, false));

        // The eagle beats its wings at his right side and lunges at the wound every few seconds.
        const float cycle = std::fmod(mTime, EaglePeckPeriod) / EaglePeckPeriod;
        const float lunge = Smooth(cycle, .52f, .62f) * (1 - Smooth(cycle, .70f, .86f));
        const float beat = std::sin(mTime * 5.0f);
        const XMFLOAT3 eagleFeet{feet.x + EagleSide * (1 - .40f * lunge), ledgeY + AmiraniHeight * EagleHeightOnBody + beat * 1.1f - lunge * 2.5f,
                                 feet.z - EagleFromBody * (1 - .45f * lunge)};
        if (!mEagleAway && !mFreed)
            renderer.DrawDoorwaysGpuMesh(mEagle.Mesh, Place(mEagle, EagleHeight, EagleYaw, eagleFeet, EaglePitch + lunge * .40f), viewProjection,
                                         Surface(mEagle.Texture.Get(), dim(1.5f, 1.6f, 1.6f), EagleEmission, false));

        // The dog works at the ankle chain.
        const XMFLOAT3 dogFeet{feet.x + DogSide, ledgeY, feet.z - DogFromBody};
        renderer.DrawDoorwaysGpuMesh(mDog.Mesh, Place(mDog, DogHeight, DogYaw, dogFeet, .05f * std::sin(mTime * 2.6f)), viewProjection,
                                     Surface(mDog.Texture.Get(), dim(1.5f, 1.5f, 1.5f), EagleEmission, false));

        // Chains: wrists to the cave wall, ankles to the ledge.
        const Material iron = Surface(renderer.mWhiteTextureView.Get(), {.07f, .08f, .09f, 1}, .05f, false);
        const auto chain = [&](XMFLOAT3 from, XMFLOAT3 to, float sag)
        {
            const float length = XMVectorGetX(XMVector3Length(XMLoadFloat3(&to) - XMLoadFloat3(&from)));
            const int links = std::max(2, static_cast<int>(length / ChainLinkLength));
            XMFLOAT3 previous = from;
            for (int i = 1; i <= links; ++i)
            {
                const float t = static_cast<float>(i) / links;
                const XMFLOAT3 point{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t - sag * 4 * t * (1 - t),
                                     from.z + (to.z - from.z) * t};
                renderer.DrawBox(Link(previous, point, ChainLinkWidth, ChainLinkWidth * .34f, i % 2 == 0), viewProjection, iron);
                previous = point;
            }
            renderer.DrawBox(XMMatrixScaling(ChainLinkWidth * 2.2f, ChainLinkWidth * 2.2f, ChainLinkWidth * 2.2f) *
                             XMMatrixTranslation(to.x, to.y, to.z), viewProjection, iron); // anchor block
        };
        for (const float side : {-1.f, 1.f})
        {
            if (mFreed) break;
            const XMFLOAT3 wrist{feet.x + side * AmiraniHeight * WristOut, ledgeY + AmiraniHeight * WristUp, feet.z};
            chain(wrist, {feet.x + side * LedgeHalfWidth * .96f, ledgeY + AmiraniHeight * 1.18f, wallZ - 1.5f}, 3.5f);
            const XMFLOAT3 ankle{feet.x + side * AmiraniHeight * AnkleOut, ledgeY + AmiraniHeight * .07f, feet.z};
            chain(ankle, {feet.x + side * LedgeHalfWidth * .62f, ledgeY + .6f, feet.z - 9.f}, 1.2f);
        }
        renderer.ResetDefaultWorldRenderState();
        renderer.BindDoorwaysSkinningDisabled();
    }

    void AmiraniMountain::StyleEnvironment(EnvironmentSettings& e)
    {
        e.ClearColor = {.004f, .010f, .020f, 1};

        // The engine's sky panels and moon are switched off; DrawSky replaces them.
        e.Sky.PanelDiffuse = {0, 0, 0, 1};
        e.Sky.PanelEmissiveStrength = 0;
        e.Sky.HorizonDiffuse = {0, 0, 0, 0};
        e.Moon.BodyScale = {.001f, .001f, .001f};
        e.Moon.GlowScale = {.001f, .001f, .001f};

        // Moonlight sweeps across the mountain face from the moon's side.
        XMStoreFloat4(&e.Lighting.DirectionalLight.Direction, MoonlightDirection());
        e.Lighting.DirectionalLight.Color = {.50f, .68f, .74f, 1};
        e.Lighting.AmbientLight.Color = {.060f, .100f, .115f, 1};

        // Haze: thin enough that the mountain reads from the play area.
        e.Fog.Color = {.014f, .040f, .046f, 1};
        e.Fog.StartDistance = HazeStart;
        e.Fog.EndDistance = HazeEnd;
        e.Fog.Strength = HazeStrength;
        e.Fog.HeightFogStrength = .22f;
    }
}
