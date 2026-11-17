#ifndef NOMINMAX
#define NOMINMAX   // std::min / std::max, not the Windows macros
#endif
#include "EaglesAscent.h"

#include "CombatSystem.h"
#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysAssetPaths.h"
#include "DoorwaysGame.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRuntimePathResolver.h"
#include "EnvironmentSettings.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

// The rules of a mountain map (EaglesAscent.h). Moved here from Map Lab on 2026-11-17 (Map A Gate 5), unchanged.
namespace CaucasianEagle
{
    using namespace Doorways;
    using namespace DirectX;

    namespace
    {
        constexpr float PropNear = 520, PropFar = 3200, PropBig = 9;   // m: small models are drawn to PropNear, models taller than PropBig to PropFar
    }

    bool EaglesAscent::Load(const std::filesystem::path& path, std::string& error)
    {
        std::ifstream file(path);
        if (!file) { error = "map data missing: " + path.string(); return false; }
        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream in(line); std::string word; in >> word;
            if (word == "summit") in >> Summit;
            else if (word == "length") in >> TrailLength;
            else if (word == "zone") { Zone z; in >> z.From >> z.Name; Zones.push_back(z); }
            else if (word == "station")
            {
                Station s; int check = 1; in >> s.Name >> s.P.x >> s.P.y >> s.P.z >> s.Yaw >> s.Along;
                if (in >> s.Id >> check) s.Checkpoint = check != 0; else s.Id.clear();
                Stations.push_back(s);
            }
            else if (word == "prop")
            {
                std::string model; Prop prop; in >> model >> prop.P.x >> prop.P.y >> prop.P.z >> prop.Yaw >> prop.Height;
                auto found = std::find(mPropNames.begin(), mPropNames.end(), model);
                if (found == mPropNames.end()) { mPropNames.push_back(model); found = mPropNames.end() - 1; }
                prop.Model = static_cast<int>(found - mPropNames.begin()); mProps.push_back(prop);
            }
            else if (word == "flight") { Flight f; in >> f.Id >> f.Kind >> f.From.x >> f.From.y >> f.From.z >> f.To.x >> f.To.y >> f.To.z; Flights.push_back(f); }
            else if (word == "updraft") { Updraft u; in >> u.Id >> u.P.x >> u.P.y >> u.P.z >> u.Radius >> u.Top >> u.Lift; Updrafts.push_back(u); }
            else if (word == "wind") { Wind w; in >> w.Id >> w.P.x >> w.P.y >> w.P.z >> w.Radius >> w.X >> w.Z >> w.Speed; Winds.push_back(w); }
            else if (word == "fly") { Fly f; in >> f.Kind >> f.From >> f.To; FlyStretches.push_back(f); }
            else if (word == "heights")
            {
                in >> mLandX >> mLandZ >> mLandStep >> mLandNx >> mLandNz;
                auto file2 = path; file2.replace_extension(".heights");
                std::ifstream bin(file2, std::ios::binary); mLand.assign(static_cast<size_t>(std::max(mLandNx, 0)) * static_cast<size_t>(std::max(mLandNz, 0)), 0.f);
                if (!bin || mLandNx < 2 || mLandNz < 2 || !bin.read(reinterpret_cast<char*>(mLand.data()), static_cast<std::streamsize>(mLand.size() * sizeof(float)))) mLand.clear();
                int grids = 1; if (!(in >> grids)) grids = 1;
                mFloor.clear();
                if (grids >= 2 && !mLand.empty())
                {
                    mFloor.assign(mLand.size(), 0.f);
                    if (!bin.read(reinterpret_cast<char*>(mFloor.data()), static_cast<std::streamsize>(mFloor.size() * sizeof(float)))) mFloor.clear();
                }
            }
            else if (word == "moment") { Moment m; in >> m.Name >> m.P.x >> m.P.y >> m.P.z; std::replace(m.Name.begin(), m.Name.end(), '_', ' '); Moments.push_back(m); }
            else if (word == "trail") { XMFLOAT3 p; in >> p.x >> p.y >> p.z; Trail.push_back(p); }
        }
        if (Stations.empty()) { error = "map data has no station: " + path.string(); return false; }
        if (Summit <= 0) for (const auto& s : Stations) Summit = std::max(Summit, s.P.y);
        ModelCount = static_cast<int>(mProps.size());
        return true;
    }

    bool EaglesAscent::LoadModels(Renderer& renderer, std::string& error)
    {
        const auto root = ResolveDoorwaysRuntimePath("assets/Compiled");
        mPropModels.resize(mPropNames.size());
        for (size_t i = 0; i < mPropNames.size(); ++i)
        {
            auto& out = mPropModels[i]; const std::string& base = mPropNames[i];
            DoorwaysLoadedDMesh mesh;
            if (!LoadDoorwaysDMesh(root / (base + ".dmesh"), mesh, &error)) return false;
            if (mesh.Vertices.empty()) { error = base + ".dmesh has no vertices"; return false; }
            out.Min = {1e9f, 1e9f, 1e9f}; out.Max = {-1e9f, -1e9f, -1e9f};
            for (const auto& v : mesh.Vertices)
            {
                out.Min.x = std::min(out.Min.x, v.Position.X); out.Max.x = std::max(out.Max.x, v.Position.X);
                out.Min.y = std::min(out.Min.y, v.Position.Y); out.Max.y = std::max(out.Max.y, v.Position.Y);
                out.Min.z = std::min(out.Min.z, v.Position.Z); out.Max.z = std::max(out.Max.z, v.Position.Z);
            }
            if (!renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(mesh, out.Mesh, &error)) return false;
            // a city's models share one picture: Cities/BatumiGate -> Cities/BatumiBaseColor.png
            std::string picture = base;
            if (base.rfind("Cities/", 0) == 0)
                for (const char* city : {"Batumi", "Gori", "Kutaisi", "Mestia", "Mtskheta", "Tbilisi", "Telavi"})
                    if (base.rfind(std::string("Cities/") + city, 0) == 0) picture = std::string("Cities/") + city;
            DoorwaysTextureLoadDesc desc; desc.Path = root / (picture + "BaseColor.png");
            desc.Semantic = DoorwaysMaterialTextureSemantic::BaseColor; desc.GenerateMips = true;
            DoorwaysTextureLoadResult result;
            if (!renderer.LoadDoorwaysTexture(desc, result, &error)) return false;
            out.Texture = result.SRV;
        }
        return true;
    }

    float EaglesAscent::Land(float x, float z) const
    {
        if (mLand.empty()) return -1e9f;
        const float fx = (x - mLandX) / mLandStep, fz = (z - mLandZ) / mLandStep;
        if (fx < 0 || fz < 0 || fx > static_cast<float>(mLandNx - 1) || fz > static_cast<float>(mLandNz - 1)) return -1e9f;
        const int ix = std::min(static_cast<int>(fx), mLandNx - 2), iz = std::min(static_cast<int>(fz), mLandNz - 2);
        const float u = fx - static_cast<float>(ix), v = fz - static_cast<float>(iz);
        const float* r0 = &mLand[static_cast<size_t>(iz) * static_cast<size_t>(mLandNx) + static_cast<size_t>(ix)]; const float* r1 = r0 + mLandNx;
        return (r0[0] * (1 - u) + r0[1] * u) * (1 - v) + (r1[0] * (1 - u) + r1[1] * u) * v;
    }

    float EaglesAscent::LandLow(float x, float z) const
    {
        if (mLand.empty()) return -1e9f;
        const float fx = (x - mLandX) / mLandStep, fz = (z - mLandZ) / mLandStep;
        if (fx < 0 || fz < 0 || fx > static_cast<float>(mLandNx - 1) || fz > static_cast<float>(mLandNz - 1)) return -1e9f;
        const int ix = std::min(static_cast<int>(fx), mLandNx - 2), iz = std::min(static_cast<int>(fz), mLandNz - 2);
        const auto& grid = mFloor.size() == mLand.size() ? mFloor : mLand;
        const float* r0 = &grid[static_cast<size_t>(iz) * static_cast<size_t>(mLandNx) + static_cast<size_t>(ix)]; const float* r1 = r0 + mLandNx;
        return std::min(std::min(r0[0], r0[1]), std::min(r1[0], r1[1]));
    }

    const EaglesAscent::Zone* EaglesAscent::ZoneAt(float y) const
    {
        const Zone* zone = nullptr;
        for (const auto& z : Zones) if (y >= z.From || !zone) zone = &z;
        return zone;
    }

    void EaglesAscent::KeepOutOfRock(Player& p, bool flying)
    {
        if (flying)
        {
            const float clear = .05f;                    // m over the land (more, and where the land is level with the path a landing hovers over it and never touches down)
            const auto blocked = [&](float x, float z)
            {   // under the land there, and the land rises more steeply from where she was than feet could climb
                const float h = Land(x, z); if (p.Position.y >= h + clear) return false;
                const float run = std::hypot(x - mFlown.x, z - mFlown.z);
                return h - std::max(Land(mFlown.x, mFlown.z), mFlown.y - clear) > .95f * run + .05f;   // the stairs climb .83 at most: she is lifted along them, rock stops her
            };
            if (mHasFlown && blocked(p.Position.x, p.Position.z))
            {   // slide along the rock where one direction is free
                if (!blocked(p.Position.x, mFlown.z)) p.Position.z = mFlown.z;
                else if (!blocked(mFlown.x, p.Position.z)) p.Position.x = mFlown.x;
                else { p.Position.x = mFlown.x; p.Position.z = mFlown.z; }
            }
            const float h = Land(p.Position.x, p.Position.z); if (p.Position.y < h + clear) p.Position.y = h + clear;
        }
        mFlown = p.Position; mHasFlown = true;
    }

    void EaglesAscent::Locate(const XMFLOAT3& p)
    {
        float best = 1e30f; size_t at = 0;
        for (size_t i = 0; i < Trail.size(); ++i)
        {
            const float dx = Trail[i].x - p.x, dy = Trail[i].y - p.y, dz = Trail[i].z - p.z;
            const float d = dx * dx + dy * dy + dz * dz;
            if (d < best) { best = d; at = i; }
        }
        if (Trail.empty()) return;
        OffTrail = std::sqrt(best); Along = static_cast<float>(at) * TrailStep;
        const size_t a = at > 0 ? at - 1 : at, b = std::min(at + 1, Trail.size() - 1);
        const float run = std::hypot(Trail[b].x - Trail[a].x, Trail[b].z - Trail[a].z);
        Slope = run > .01f ? (Trail[b].y - Trail[a].y) / run : 0;
    }

    void EaglesAscent::Visit(DoorwaysGameHost& host, int station)
    {
        station = std::clamp(station, 0, static_cast<int>(Stations.size()) - 1);
        const auto& s = Stations[static_cast<size_t>(station)];
        auto& p = host.World.MainPlayer;
        p.Position = {s.P.x, s.P.y + .05f, s.P.z}; p.Yaw = s.Yaw; p.VerticalVelocity = 0;
        p.TraversalMode = DoorwaysPlayerTraversalMode::Ground;
        GameSession().FreeFlightEnd = true; GameSession().ImpulseX = GameSession().ImpulseZ = 0;
        host.World.MainCamera.Yaw = s.Yaw; host.World.MainCamera.Pitch = -.12f;
        host.Combat.RestorePlayerHealth(host.World, 10000);
        mHasFlown = false; mInRock = 0;
        if (s.Checkpoint) Checkpoint = station;
    }

    const char* EaglesAscent::Air(DoorwaysGameHost& host, float dt, bool flying, bool rescue)
    {
        auto& p = host.World.MainPlayer; AirNow = L""; Rescued = std::max(0.f, Rescued - dt);
        const bool landing = GameSession().FreeFlightLandSpeed > 0 || GameSession().FreeFlightEnd;
        if (flying)
        {
            for (const auto& u : Updrafts)   // rising air carries her up, but not while she lands (it would hold her aloft for ever)
                if (!landing && std::hypot(p.Position.x - u.P.x, p.Position.z - u.P.z) < u.Radius && p.Position.y > u.P.y - 2 && p.Position.y < u.Top)
                { p.Position.y += u.Lift * dt; AirNow = L"UPDRAFT"; }
            for (const auto& w : Winds)
            {
                const float dx = p.Position.x - w.P.x, dy = p.Position.y - w.P.y, dz = p.Position.z - w.P.z;
                if (dx * dx + dy * dy + dz * dz < w.Radius * w.Radius) { GameSession().ImpulseX += w.X * w.Speed; GameSession().ImpulseZ += w.Z * w.Speed; AirNow = L"CROSSWIND"; }
            }
        }
        if (!flying && p.IsGrounded)
            for (size_t i = 0; i < Stations.size(); ++i)
                if (Stations[i].Checkpoint && std::abs(p.Position.y - Stations[i].P.y) < 3 &&
                    std::hypot(p.Position.x - Stations[i].P.x, p.Position.z - Stations[i].P.z) < 22)
                    Checkpoint = static_cast<int>(i);
        // falling inside the mountain: on foot, not grounded and 8 m under the land's lowest grid corner
        mInRock = !flying && !p.IsGrounded && p.Position.y < LandLow(p.Position.x, p.Position.z) - 8 ? mInRock + dt : 0;
        if (rescue && !Trail.empty() && (p.Position.y < -6 || OffTrail > 1500 || mInRock > 1))
        {
            const char* event = mInRock > 1 ? "rescued_from_inside_the_mountain" : "rescued";
            Visit(host, Checkpoint); Rescued = 3;
            return event;
        }
        return nullptr;
    }

    void EaglesAscent::DrawModels(Renderer& renderer, const WorldState& world, const XMMATRIX& vp, int light)
    {
        ModelsDrawn = 0;
        if (mProps.empty()) return;
        renderer.ResetDefaultWorldRenderState(); renderer.BindDoorwaysSkinningDisabled();
        const XMFLOAT3 eye = world.MainCamera.Position;
        Material m{}; m.Diffuse = {1, 1, 1, 1}; m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .9f;
        m.SpecularColor = {0, 0, 0, 0}; m.EmissiveStrength = light == 2 ? .25f : .04f; m.DoubleSided = true; m.ReceiveFog = true;
        for (const auto& prop : mProps)
        {
            const float dx = prop.P.x - eye.x, dy = prop.P.y - eye.y, dz = prop.P.z - eye.z;
            const float limit = prop.Height > PropBig ? PropFar : PropNear;
            if (dx * dx + dy * dy + dz * dz > limit * limit) continue;
            const auto& model = mPropModels[static_cast<size_t>(prop.Model)];
            const float scale = prop.Height / std::max(model.Max.y - model.Min.y, 1e-4f);
            m.DiffuseMap = model.Texture.Get();
            renderer.DrawDoorwaysGpuMesh(model.Mesh,
                XMMatrixTranslation(-(model.Min.x + model.Max.x) * .5f, -model.Min.y, -(model.Min.z + model.Max.z) * .5f) * XMMatrixScaling(scale, scale, scale) *
                XMMatrixRotationY(prop.Yaw) * XMMatrixTranslation(prop.P.x, prop.P.y, prop.P.z), vp, m);
            ++ModelsDrawn;
        }
    }

    void EaglesAscent::StyleEnvironment(EnvironmentSettings& e, int light, int fog)
    {
        // day, dawn, night. Day is the owner's target picture (Gate 2): a warm, low afternoon sun from over the sea
        // (the west), a pale hazy sky, cool shadows.
        static const XMFLOAT4 sky[3]{{.62f, .71f, .82f, 1}, {.78f, .56f, .44f, 1}, {.012f, .022f, .05f, 1}};
        static const XMFLOAT4 sun[3]{{1, .87f, .68f, 1}, {1, .70f, .48f, 1}, {.50f, .68f, .74f, 1}};
        static const XMFLOAT4 ambient[3]{{.38f, .43f, .54f, 1}, {.36f, .30f, .30f, 1}, {.07f, .11f, .13f, 1}};
        static const XMFLOAT4 direction[3]{{.72f, -.52f, .46f, 0}, {.85f, -.28f, .40f, 0}, {-.40f, -.70f, .55f, 0}};
        const auto i = static_cast<size_t>(std::clamp(light, 0, 2));
        e.ClearColor = sky[i];
        e.Lighting.DirectionalLight.Direction = direction[i]; e.Lighting.DirectionalLight.Color = sun[i]; e.Lighting.DirectionalLight.Intensity = 1.6f;
        e.Lighting.AmbientLight.Color = ambient[i]; e.Lighting.AmbientLight.Intensity = 1;
        // The engine's sky box is a room round the origin; the map is far bigger. It is pushed out of sight, so the sky is the clear colour.
        e.Sky.Distance = 40000; e.Sky.Height = 80000; e.Sky.FrontBackPanelWidth = 80000; e.Sky.SidePanelLength = 80000; e.Sky.CenterY = 0; e.Sky.TopY = 40000;
        e.Sky.PanelDiffuse = sky[i]; e.Sky.PanelEmissiveStrength = 1; e.Sky.HorizonDiffuse = {0, 0, 0, 0}; e.Sky.HorizonCenterY = -30000;
        e.Fog.Color = sky[i]; e.Fog.HeightFogEnabled = 0; e.Fog.HeightFogStrength = 0; e.Fog.MoonHazeStrength = 0; e.Fog.BaseFogAmount = 0;
        if (fog == 0) e.Fog.Strength = 0;
        // haze: the map's aerial perspective (Gate 4): nothing on the path, about a seventh at 5 km (the far mountain
        // from the sea), cool sky colour. Was 300-7,000 m at 0.75, which washed the whole mountain out from C0; 0.6 was still pale.
        else if (fog == 1) { e.Fog.StartDistance = 800; e.Fog.EndDistance = 12000; e.Fog.Strength = .45f; e.Fog.Density = 1; e.Fog.DistanceCurvePower = 1; }
        else { e.Fog.StartDistance = 30; e.Fog.EndDistance = 700; e.Fog.Strength = 1; }
    }
}
