#define NOMINMAX
#include "Journey.h"

#include "AmiraniMountain.h"
#include "Nature.h"
#include "CombatSystem.h"
#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysGame.h"
#include "DoorwaysGamepad.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRenderContexts.h"
#include "DoorwaysRuntimePathResolver.h"
#include "DoorwaysSettingsPaths.h"
#include "EnvironmentSettings.h"
#include "GameConstants.h"
#include "GameUi.h"
#include "Guide.h"
#include "Settings.h"
#include "Sound.h"
#include "Story.h"
#include "Renderer.h"
#include "WorldMap.h"
#include "WorldState.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <sstream>

using namespace DirectX;

namespace CaucasianEagle
{
    using namespace Doorways;

    Journey& GetJourney()
    {
        static Journey journey;
        return journey;
    }

    namespace
    {
        constexpr float Pi = 3.14159265f;

        // ------------------------------------------------------------ monsters of the road
        enum Kind { Chinka, Ali, Kudiani, Ochokochi, Almasty, Kaji, Uaig, Devi, Rokapi, Gveleshapi, DeviLord, Eagle, ChestKind, ChainKind, KindCount };
        struct KindDef
        {
            const wchar_t* Name; const char* Model;
            float Height; int Health; float Speed;
            float Reach; int Damage; float Cooldown, Windup;       // melee
            float Range; int OrbDamage; float OrbEvery; int Volley; // ranged (Range 0 = none)
            float Keep;                                             // > 0: keeps this distance and shoots
            float Radius, PushScale; int Cost;
            XMFLOAT4 Color; bool Boss; float Hover;
        };
        // Sources for the creatures: techplan/04-components.md (journey) and story/STORY.md.
        const KindDef Kinds[KindCount] = {
            {L"CHINKA", "Monsters/Chinka", 1.15f, 70, 5.4f, 1.5f, 4, 1.3f, .40f, 0, 0, 0, 0, 0, .40f, 1.f, 1, {.5f, 1.f, .4f, 1}, false, 0},
            {L"ALI", "Monsters/Ali", 2.0f, 120, 3.0f, 1.6f, 6, 2.0f, .50f, 14, 6, 3.0f, 1, 8.f, .50f, 1.f, 2, {.5f, .9f, 1.f, 1}, false, .15f},
            {L"KUDIANI", "Monsters/Kudiani", 1.75f, 140, 2.6f, 1.6f, 6, 2.2f, .50f, 16, 7, 2.6f, 1, 9.5f, .50f, 1.f, 2, {.4f, 1.f, .3f, 1}, false, 0},
            {L"OCHOKOCHI", "Monsters/Ochokochi", 2.7f, 260, 4.4f, 2.3f, 9, 1.8f, .55f, 0, 0, 0, 0, 0, .80f, .7f, 3, {1.f, .5f, .3f, 1}, false, 0},
            {L"ALMASTY", "Monsters/Almasty", 2.9f, 320, 3.5f, 2.5f, 11, 2.1f, .60f, 15, 10, 6.0f, 1, 0, .90f, .6f, 4, {.8f, .8f, .75f, 1}, false, 0},
            {L"KAJI", "Monsters/Kaji", 2.6f, 380, 3.1f, 2.8f, 13, 2.3f, .65f, 13, 9, 5.0f, 1, 0, .85f, .6f, 5, {1.f, .45f, .1f, 1}, false, 0},
            {L"UAIG", "Monsters/Uaig", 4.6f, 700, 2.7f, 3.7f, 16, 2.8f, .80f, 0, 0, 0, 0, 0, 1.4f, .3f, 8, {.9f, .85f, .6f, 1}, false, 0},
            {L"DEVI", "Monsters/Devi", 5.2f, 900, 2.5f, 4.0f, 18, 2.6f, .85f, 0, 0, 0, 0, 0, 1.6f, .25f, 10, {.8f, .7f, .2f, 1}, false, 0},
            {L"ROKAPI, QUEEN OF WITCHES", "Monsters/Rokapi", 3.4f, 3400, 3.0f, 2.0f, 10, 2.5f, .60f, 22, 9, 2.4f, 3, 11.f, 1.0f, 0, 0, {.7f, .3f, 1.f, 1}, true, .6f},
            {L"THE GVELESHAPI", "Monsters/Gveleshapi", 9.0f, 4200, 1.6f, 6.0f, 20, 3.0f, 1.0f, 26, 8, 3.2f, 5, 0, 2.6f, 0, 0, {.3f, .8f, 1.f, 1}, true, 0},
            {L"THE DEVI LORD", "Monsters/Devi", 8.5f, 4600, 2.6f, 5.5f, 24, 2.8f, 1.0f, 0, 0, 0, 0, 0, 2.4f, 0, 0, {1.f, .8f, .25f, 1}, true, 0},
            {L"THE EAGLE, YOUR FATHER", "Landmarks/AmiraniMountain/Eagle", 8.0f, 7000, 4.2f, 4.5f, 16, 2.4f, .80f, 30, 9, 2.8f, 5, 9.f, 2.4f, 0, 0, {.8f, .95f, 1.f, 1}, true, 2.0f},
            {L"OLD GOLD CHEST", "Monsters/Chest", 1.0f, 30, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, .80f, 0, 0, {1.f, .8f, .3f, 1}, false, 0},
            {L"CHAIN ANCHOR", "", 3.0f, 500, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1.7f, 0, 0, {.7f, .85f, 1.f, 1}, false, 0},
        };
        // Who lives where: the roster of each stage (fights pick from it by cost).
        const std::vector<int> Rosters[12] = {
            {Chinka}, {Chinka, Kudiani}, {Ali, Chinka}, {Ochokochi, Chinka}, {Kudiani, Ochokochi, Almasty}, {Almasty, Chinka, Ali},
            {Kaji, Kudiani}, {Uaig, Almasty, Ochokochi}, {Ali, Chinka, Kudiani}, {Devi, Kaji, Chinka}, {Kudiani, Ali, Ochokochi}, {Kaji, Devi, Uaig}};
        const int StageBoss[12] = {-1, -1, -1, -1, -1, -1, -1, -1, Gveleshapi, DeviLord, Rokapi, -1};

        const char* PropModels[] = {"Props/DeadTree", "Props/Fir", "Props/Tower", "Props/Rocks"};
        const char* PropNames[] = {"deadtree", "fir", "tower", "rocks"};

        // ------------------------------------------------------------ atmosphere of each stage
        struct StageAir { XMFLOAT3 Fog; float FogEnd, FogStrength; XMFLOAT3 Ambient, Light; };
        const StageAir Air[12] = {
            {{.014f, .040f, .046f}, 900, .62f, {.060f, .100f, .115f}, {.50f, .68f, .74f}},
            {{.020f, .045f, .030f}, 260, .80f, {.050f, .085f, .060f}, {.40f, .60f, .50f}},
            {{.030f, .060f, .050f}, 180, .88f, {.055f, .095f, .080f}, {.38f, .58f, .52f}},
            {{.022f, .040f, .022f}, 300, .78f, {.055f, .080f, .050f}, {.45f, .60f, .45f}},
            {{.035f, .040f, .045f}, 600, .65f, {.075f, .085f, .095f}, {.60f, .66f, .70f}},
            {{.100f, .130f, .170f}, 380, .80f, {.120f, .150f, .190f}, {.62f, .74f, .90f}},
            {{.070f, .025f, .012f}, 320, .82f, {.110f, .055f, .040f}, {.70f, .45f, .32f}},
            {{.045f, .040f, .032f}, 520, .68f, {.085f, .080f, .070f}, {.66f, .62f, .55f}},
            {{.020f, .050f, .070f}, 300, .80f, {.050f, .090f, .120f}, {.45f, .65f, .85f}},
            {{.050f, .036f, .014f}, 280, .80f, {.095f, .075f, .040f}, {.70f, .58f, .36f}},
            {{.040f, .020f, .060f}, 300, .80f, {.080f, .050f, .110f}, {.55f, .42f, .75f}},
            {{.016f, .036f, .050f}, 1100, .55f, {.070f, .100f, .125f}, {.60f, .75f, .85f}}};

        // ------------------------------------------------------------ small maths
        XMFLOAT3 Add(XMFLOAT3 a, XMFLOAT3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
        XMFLOAT3 Sub(XMFLOAT3 a, XMFLOAT3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
        XMFLOAT3 Mul(XMFLOAT3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
        float LenXZ(XMFLOAT3 a) { return std::sqrt(a.x * a.x + a.z * a.z); }
        float Len(XMFLOAT3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
        XMFLOAT3 Norm(XMFLOAT3 a) { const float l = Len(a); return l > 1e-5f ? Mul(a, 1 / l) : XMFLOAT3{0, 0, 1}; }
        float Smooth(float x, float a, float b) { const float t = std::clamp((x - a) / (b - a), 0.f, 1.f); return t * t * (3 - 2 * t); }
        float AngleTo(float from, float to, float maxStep)
        {
            float d = std::fmod(to - from + Pi, 2 * Pi); if (d < 0) d += 2 * Pi; d -= Pi;
            return from + std::clamp(d, -maxStep, maxStep);
        }

        Material Surface(ID3D11ShaderResourceView* texture, XMFLOAT4 tint, float emission, bool blend = false)
        {
            Material m{};
            m.Diffuse = tint; m.DiffuseMap = texture;
            m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .85f;
            m.SpecularColor = {0, 0, 0, 0}; m.EmissiveStrength = emission;
            m.DoubleSided = true;
            if (blend) m.AlphaMode = DoorwaysMaterialAlphaMode::Blend;
            return m;
        }

        XMMATRIX Place(XMFLOAT3 min, XMFLOAT3 max, float height, float yaw, XMFLOAT3 feet, float pitch = 0, float roll = 0)
        {
            const float scale = height / std::max(max.y - min.y, 1e-4f);
            return XMMatrixTranslation(-(min.x + max.x) * .5f, -min.y, -(min.z + max.z) * .5f) * XMMatrixScaling(scale, scale, scale) *
                   XMMatrixRotationZ(roll) * XMMatrixRotationX(pitch) * XMMatrixRotationY(yaw) * XMMatrixTranslation(feet.x, feet.y, feet.z);
        }

        std::wstring Wide(const std::string& s)
        {
            std::wstring w(s.begin(), s.end());
            for (auto& c : w) c = c == L'_' ? L' ' : static_cast<wchar_t>(std::towupper(c));
            return w;
        }

        float EnvFloat(const char* name, float fallback)
        {
            char buffer[32] = {};
            if (!GetEnvironmentVariableA(name, buffer, sizeof(buffer)) || !buffer[0]) return fallback;
            return static_cast<float>(std::atof(buffer));
        }

        void Log(const char* format, ...)
        {
            char buffer[512];
            va_list args; va_start(args, format); std::vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
            OutputDebugStringA(buffer);
        }
    }

    float Journey::Random()
    {
        mSeed = mSeed * 1664525u + 1013904223u;
        return static_cast<float>((mSeed >> 8) & 0xFFFFFF) / 16777216.f;
    }

    // ================================================================ loading
    bool Journey::LoadModel(Renderer& renderer, const std::string& base, Model& out, std::string& error, ID3D11ShaderResourceView* sharedTexture, const std::string& textureBase)
    {
        const auto root = ResolveDoorwaysRuntimePath("assets/Compiled");
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
        if (sharedTexture) { out.Texture = sharedTexture; return true; }
        DoorwaysTextureLoadDesc desc;
        desc.Path = root / ((textureBase.empty() ? base : textureBase) + "BaseColor.png");
        desc.Semantic = DoorwaysMaterialTextureSemantic::BaseColor;
        desc.GenerateMips = true;
        DoorwaysTextureLoadResult result;
        if (!renderer.LoadDoorwaysTexture(desc, result, &error)) return false;
        out.Texture = result.SRV;
        return true;
    }

    bool Journey::LoadRoad(std::string& error)
    {
        const auto path = ResolveDoorwaysRuntimePath(RoadFile);
        std::ifstream file(path);
        if (!file) { error = std::string("cannot open ") + RoadFile; return false; }
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream in(line);
            std::string tag; in >> tag;
            if (tag == "STEP") in >> mStep;
            else if (tag == "S") { int index; std::string name; in >> index >> name; mStageNames.push_back(Wide(name)); }
            else if (tag == "P") { Sample s; in >> s.P.x >> s.P.y >> s.P.z >> s.W >> s.Stage; mSamples.push_back(s); }
            else if (tag == "A") { Arena a; in >> a.Stage >> a.S >> a.Kind >> a.R >> a.P.x >> a.P.y >> a.P.z; mArenas.push_back(a); }
            else if (tag == "C") { Chest c; in >> c.P.x >> c.P.y >> c.P.z >> c.Yaw >> c.Tier; mChests.push_back(c); }
            else if (tag == "T")
            {
                Prop p; std::string type; in >> type >> p.P.x >> p.P.y >> p.P.z >> p.Yaw >> p.Height;
                p.Type = -1;
                for (int i = 0; i < 4; ++i) if (type == PropNames[i]) p.Type = i;
                if (p.Type >= 0) mProps.push_back(p);
            }
            else if (tag == "L") { Light l; in >> l.P.x >> l.P.y >> l.P.z >> l.Color.x >> l.Color.y >> l.Color.z >> l.Size; l.Color.w = 1; mLights.push_back(l); }
            else if (tag == "O") { float x = 0, z = 0, r = 0; in >> x >> z >> r; AddObstacle(x, z, r); }
        }
        if (mSamples.size() < 16 || mArenas.empty() || mStageNames.size() != 12) { error = "Road.txt is incomplete"; return false; }
        const int n = static_cast<int>(mSamples.size());
        for (int i = 0; i < n; ++i)
        {
            const auto& a = mSamples[std::max(0, i - 1)].P; const auto& b = mSamples[std::min(n - 1, i + 1)].P;
            const float dx = b.x - a.x, dz = b.z - a.z, l = std::max(1e-4f, std::sqrt(dx * dx + dz * dz));
            mSamples[i].Tx = dx / l; mSamples[i].Tz = dz / l; mSamples[i].Nx = dz / l; mSamples[i].Nz = -dx / l;
        }
        // arena chests (tier 1 and 2) follow the arenas in file order
        int arena = 0;
        for (auto& c : mChests)
            if (c.Tier > 0)
            {
                while (arena < static_cast<int>(mArenas.size()) && mArenas[arena].Kind == 2) ++arena;
                c.Arena = arena++;
            }
        return true;
    }

    // The seven cities: models (one texture sheet per city), buildings and street lights. Scenery that
    // the world builder put where a city now stands is removed.
    bool Journey::LoadCities(Renderer& renderer, std::string& error)
    {
        std::ifstream file(ResolveDoorwaysRuntimePath(CitiesFile));
        if (!file) { error = std::string("cannot open ") + CitiesFile; return false; }
        std::vector<std::pair<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>> sheets;   // loaded once per city
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream in(line);
            std::string tag; in >> tag;
            if (tag == "O") { float x = 0, z = 0, r = 0; in >> x >> z >> r; AddObstacle(x, z, r); }   // a house: she walks round it
            else if (tag == "CITY") { City c; int index; std::string key, name; in >> index >> key >> name >> c.S0 >> c.S1 >> c.P.x >> c.P.y >> c.P.z; mCities.push_back(c); }
            else if (tag == "M")
            {
                int index; std::string model, sheet; in >> index >> model >> sheet;
                ID3D11ShaderResourceView* shared = nullptr;
                for (const auto& known : sheets) if (known.first == sheet) shared = known.second.Get();
                mCityModels.emplace_back();
                if (!LoadModel(renderer, model, mCityModels.back(), error, shared, sheet)) return false;
                if (!shared) sheets.emplace_back(sheet, mCityModels.back().Texture);
            }
            else if (tag == "B")
            {
                Building b; in >> b.Model >> b.P.x >> b.P.y >> b.P.z >> b.Yaw >> b.Height;
                if (b.Model >= 0 && b.Model < static_cast<int>(mCityModels.size())) mBuildings.push_back(b);
            }
            else if (tag == "L") { Light l; in >> l.P.x >> l.P.y >> l.P.z >> l.Color.x >> l.Color.y >> l.Color.z >> l.Size; l.Color.w = 1; mLights.push_back(l); }
        }
        if (static_cast<int>(mCities.size()) != Story::CityCount) { error = "Cities.txt is incomplete"; return false; }
        const auto inCity = [&](float s) { for (const auto& c : mCities) if (s > c.S0 - 6 && s < c.S1 + 6) return true; return false; };
        std::erase_if(mProps, [&](const Prop& prop) { return inCity(static_cast<float>(Nearest(prop.P.x, prop.P.z, -1, 0)) * mStep); });
        BuildTroubles();
        return true;
    }

    // A journey that begins past a city (an old save, a test start) has been through it and holds its gift.
    void Journey::MarkCitiesBehind()
    {
        const float checkpoint = mArenasCleared > 0 ? mArenas[std::min(mArenasCleared, static_cast<int>(mArenas.size())) - 1].S : 0;
        for (auto& c : mCities) if (c.S1 < checkpoint) c.Visited = true;
    }

    float Journey::DamageTaken() const { return Collection::DamageTaken(mCollection, CitiesVisited()) * MonsterDamageScale * (Gift(3) ? StoneDamageScale : 1.f) * Growth::DamageTaken(mGrowth); }
    int Journey::PotionHealth() const { return PotionHeal + (Gift(2) ? BlessingPotionHeal : 0); }
    unsigned Journey::CityBits() const
    {
        unsigned bits = 0;
        for (size_t i = 0; i < mCities.size(); ++i) if (mCities[i].Visited) bits |= 1u << i;
        return bits;
    }
    int Journey::CitiesVisited() const
    {
        int count = 0;
        for (const auto& city : mCities) count += city.Visited ? 1 : 0;
        return count;
    }
    // One feather of her father in each region: a third of the way through it, at the very edge of the road,
    // away from arenas and cities, on alternating sides.
    void Journey::PlaceFatherFeathers()
    {
        mFatherFeathers.assign(Collection::FeatherCount, XMFLOAT3{0, -1000, 0});
        for (int stage = 0; stage < Collection::FeatherCount; ++stage)
        {
            int first = -1, last = -1;
            for (int i = 0; i < static_cast<int>(mSamples.size()); ++i)
                if (mSamples[i].Stage == stage) { if (first < 0) first = i; last = i; }
            if (first < 0) continue;
            const auto taken = [&](int i)
            {
                const float s = static_cast<float>(i) * mStep;
                if (s < 60) return true;                                    // not in sight of where the road begins
                for (const auto& a : mArenas) if (std::abs(s - a.S) < a.R + ArenaCageMargin + 10) return true;
                for (const auto& c : mCities) if (s > c.S0 - 15 && s < c.S1 + 15) return true;
                return false;
            };
            int index = first + (last - first) / 3;
            while (index < last && taken(index)) index += 5;
            if (taken(index)) continue;
            const Sample& s = mSamples[index];
            const float side = stage % 2 ? -1.f : 1.f, lateral = std::max(0.f, s.W - 1.3f) * side;
            mFatherFeathers[stage] = {s.P.x + s.Nx * lateral, s.P.y + 1.1f, s.P.z + s.Nz * lateral};
        }
    }
    void Journey::UpdateFatherFeathers(WorldState& world)
    {
        for (int i = 0; i < static_cast<int>(mFatherFeathers.size()); ++i)
        {
            if (mCollection.Feathers >> i & 1u) continue;
            XMFLOAT3 to = Sub(mFatherFeathers[i], world.MainPlayer.Position);
            if (LenXZ(to) > FatherFeatherReach || std::abs(to.y) > 4) continue;
            mCollection.Feathers |= 1u << i;
            auto& kit = GetNyxaraKit();
            kit.Effect(mFatherFeathers[i], {.75f, .9f, 1.f, 1}, 40, 5.f, .26f);
            Sound::Play("world/gift", .8f, 0);
            Notify(HudArt::IconFeather, Palette::Moon, L"A FEATHER OF YOUR FATHER",
                   std::to_wstring(Collection::FeathersFound(mCollection)) + L" of " + std::to_wstring(Collection::FeatherCount) + L". Your wings carry you faster.", 6);
            Log("[JOURNEY] father's feather %d found at %.0f m.\n", i + 1, mS);
            Save();
        }
    }
    float Journey::CageEnd(const Arena& a, float end) const
    {
        return std::clamp(a.S + end * (a.R + ArenaCageMargin), 0.f, static_cast<float>(mSamples.size() - 1) * mStep);
    }

    bool Journey::Initialize(Renderer& renderer, WorldState& world, std::string& error)
    {
        mReady = false;
        if (!LoadRoad(error)) return false;
        if (!LoadCities(renderer, error) || !WorldMap::Load(renderer, error)) return false;
        PlaceFatherFeathers();
        {   // nothing grows inside a city
            std::vector<Nature::Clearing> clearings;
            for (const auto& c : mCities) clearings.push_back({c.P.x, c.P.z, (c.S1 - c.S0) * .5f + 30.f});
            GetNature().SetClearings(std::move(clearings));
        }
        for (const auto& sample : mSamples) mRoadPoints.push_back({sample.P.x, sample.P.z});
        mPatrols.clear(); mPatrolNext = 0;
        {   // roaming packs: one or two in every gap between arenas
            float previous = 60;
            for (const auto& a : mArenas)
            {
                const float lo = previous + 30, hi = a.S - a.R - 30;
                const int n = hi - lo > 60 ? 2 : (hi > lo ? 1 : 0);
                for (int k = 0; k < n; ++k) mPatrols.push_back(lo + (hi - lo) * (static_cast<float>(k) + .5f) / static_cast<float>(n));
                previous = a.S + a.R;
            }
            // no pack waits inside a city
            std::erase_if(mPatrols, [&](float s) { for (const auto& c : mCities) if (s > c.S0 - 12 && s < c.S1 + 12) return true; return false; });
        }
        mModels.resize(KindCount);
        for (int k = 0; k < KindCount; ++k)
        {
            if (!Kinds[k].Model[0]) continue;
            if (!LoadModel(renderer, Kinds[k].Model, mModels[k], error)) return false;
            if (!mModels[k].Rig.Load(ResolveDoorwaysRuntimePath("assets/Compiled") / (std::string(Kinds[k].Model) + ".rig"), error)) return false;
            if (k != ChestKind && mModels[k].Rig.Kind == CreatureRig::Type::None) Log("[CaucasianEagle] %s has no .rig: it is drawn rigid\n", Kinds[k].Model);
        }
        mPropModels.resize(4);
        for (int i = 0; i < 4; ++i)
            if (!LoadModel(renderer, PropModels[i], mPropModels[i], error)) return false;

        auto& kit = GetNyxaraKit();
        kit.SetExternalEnemies(true);
        mMonsters.clear();
        mEnv[0] = 1;

        mTimeScale = 1;
        const bool unattended = GameSession().Unattended;
        mPilot = unattended && EnvFloat(PilotEnvironmentVariable, 0) > 0;
        mShowcase = unattended && !mPilot;
        if (mPilot)
        {
            mTimeScale = std::clamp(EnvFloat(SpeedEnvironmentVariable, 1), 1.f, 12.f);
            kit.SetAutoKeys(&mPilotKeys);
        }
        const float startArena = EnvFloat(StartEnvironmentVariable, -1);
        if (!unattended && startArena < 0) Load();          // unattended runs never touch the player's save
        if (startArena >= 0) { mArenasCleared = std::clamp(static_cast<int>(startArena), 0, static_cast<int>(mArenas.size())); mFeathers = mArenasCleared; }
        if (!mCitiesInSave) MarkCitiesBehind();
        mMapForced = unattended && EnvFloat(MapEnvironmentVariable, 0) > 0;
        mPauseForced = unattended && EnvFloat(PauseEnvironmentVariable, 0) > 0;
        mGuideForced = unattended ? static_cast<int>(EnvFloat(GuideEnvironmentVariable, 0)) : 0;
        mGrowthForced = unattended && EnvFloat(GrowthEnvironmentVariable, 0) > 0;
        if (unattended && EnvFloat(GrowthEnvironmentVariable, 0) > 1)       // =2: the second page, with things to show
        {
            mGrowthPage = 1; mCollection.Links = 2; mCollection.Feathers = 0x2D; mCollection.Runes[0] = 1; mCollection.Runes[2] = 2; mCollection.Set = 2;
            for (int i = 0; i < Collection::CharmKinds; ++i) mCollection.Charms[i] = (i * 2 + 1) % (CharmSet + 1);
        }
        if (mGrowthForced) { mGrowth.Crystals = 150; mGrowth.Potions = 2; mGrowth.Level[Growth::Wings] = 2; mGrowth.Level[Growth::Swords] = 1; mGrowth.Level[Growth::Armour] = 1; mGrowth.Level[Growth::Power] = 3; }
        mFormShown = Growth::Form(mGrowth);
        if (mArenasCleared >= static_cast<int>(mArenas.size()) && !mCompleted)
        {   // the Eagle has fallen but the chain still holds (a save made between the two, or a test start)
            mFinale = Finale::Chains;
            GetAmiraniMountain().SetEagleAway(true);
            for (const float side : {-1.f, 1.f})
                Spawn(ChainKind, {MountainX + side * LedgeHalfWidth * .62f, LedgeHeight, LedgeWalkBackZ - 2.5f}, false, 1);
        }
        if (mArenasCleared > 0)
        {
            mPendingTeleport = true;
            mJournal.ChaptersReached = mArenasCleared >= static_cast<int>(mArenas.size()) ? 12 : mArenas[mArenasCleared - 1].Stage + 1;
        }
        if (mCompleted) { mJournal.ChaptersReached = 12; mJournal.EndReached = 3; }
        else if (mFinale == Finale::Chains) mJournal.EndReached = 2;
        mJournalForced = static_cast<int>(EnvFloat(JournalEnvironmentVariable, 0));
        mRestartTest = EnvFloat(RestartEnvironmentVariable, 0);
        if (mJournalForced > 10) { mJournalForced -= 10; mJournal.ChaptersReached = 12; mJournal.EndReached = 3; mLore = 12; }
        if (!unattended)
        {   // every launch: where the story is read; on a new road the legend is marked as new
            Notify(HudArt::IconBook, Palette::GoldLight, L"THE STORY", L"Press F1 at any time to read it.", 10);
            if (mArenasCleared == 0) mJournal.Unread |= 1u << Story::Legend;
        }
        if (mShowcase)
        {   // scripted showcase: a mixed pack around the arrival point
            const XMFLOAT3 o = world.MainPlayer.Position;
            const int pack[6] = {Chinka, Kudiani, Ochokochi, Ali, Chinka, Almasty};
            for (int i = 0; i < 6; ++i)
            {
                const float a = (static_cast<float>(i) + .5f) * Pi / 3, r = 8.5f + 2.5f * static_cast<float>(i % 2);
                Spawn(pack[i], {o.x + std::sin(a) * r, o.y, o.z + std::cos(a) * r}, false, 1);
            }
        }
        mReady = true;
        Log("[PASS] Journey: road %.0f m, %d arenas, %d chests, %d props, %d lights, %d cities with %d buildings; cleared %d.\n",
            static_cast<float>(mSamples.size()) * mStep, static_cast<int>(mArenas.size()), static_cast<int>(mChests.size()),
            static_cast<int>(mProps.size()), static_cast<int>(mLights.size()), static_cast<int>(mCities.size()),
            static_cast<int>(mBuildings.size()), mArenasCleared);
        return true;
    }

    // ================================================================ save file
    void Journey::Save() const
    {
        if (GameSession().Unattended) return;
        const auto path = ResolveDoorwaysSettingsPaths().Player.parent_path() / SaveFileName;
        std::error_code ignored;
        std::filesystem::create_directories(path.parent_path(), ignored);
        std::ofstream file(path, std::ios::trunc);
        if (!file) return;
        file << "CaucasianEagleJourney 1\n" << mArenasCleared << ' ' << mGold << ' ' << mFeathers << ' ' << mLore << ' ' << mDeaths << ' '
             << (mCompleted ? 1 : 0) << ' ' << static_cast<int>(mPlaySeconds) << '\n';
        for (const auto& c : mChests) file << (c.Opened ? '1' : '0');
        file << '\n';
        unsigned cities = 0;                                    // bit per city she has entered (absent in older saves)
        for (size_t i = 0; i < mCities.size(); ++i) if (mCities[i].Visited) cities |= 1u << i;
        file << cities << '\n';
        file << mGrowth.Crystals << ' ' << mGrowth.Potions;      // absent in saves from before crystals
        for (const int level : mGrowth.Level) file << ' ' << level;
        file << '\n';
        for (const int charms : mCollection.Charms) file << charms << ' ';      // absent in saves from before the collection
        file << mCollection.Links << ' ' << mCollection.Feathers;
        for (const int runes : mCollection.Runes) file << ' ' << runes;
        file << ' ' << mCollection.Set << '\n';
    }

    void Journey::Load()
    {
        const auto path = ResolveDoorwaysSettingsPaths().Player.parent_path() / SaveFileName;
        std::ifstream file(path);
        std::string magic; int version = 0;
        if (!(file >> magic >> version) || magic != "CaucasianEagleJourney" || version != 1) return;
        int completed = 0, seconds = 0;
        if (!(file >> mArenasCleared >> mGold >> mFeathers >> mLore >> mDeaths >> completed >> seconds)) { mArenasCleared = 0; return; }
        mArenasCleared = std::clamp(mArenasCleared, 0, static_cast<int>(mArenas.size()));
        mLore = std::clamp(mLore, 0, 12);
        mCompleted = completed != 0; mPlaySeconds = static_cast<float>(seconds);
        std::string opened; file >> opened;
        for (size_t i = 0; i < mChests.size() && i < opened.size(); ++i) mChests[i].Opened = opened[i] == '1';
        unsigned cities = 0;
        if (file >> cities)
        {
            mCitiesInSave = true;
            for (size_t i = 0; i < mCities.size(); ++i) mCities[i].Visited = (cities >> i & 1u) != 0;
        }
        Growth::State growth;
        bool read = static_cast<bool>(file >> growth.Crystals >> growth.Potions);
        for (int& level : growth.Level) read = read && static_cast<bool>(file >> level);
        if (read)
        {
            growth.Crystals = std::max(0, growth.Crystals); growth.Potions = std::clamp(growth.Potions, 0, MaxPotions);
            for (int row = 0; row < Growth::RowCount; ++row) growth.Level[row] = std::clamp(growth.Level[row], 0, Growth::Max(row));
            mGrowth = growth;
        }
        else mGrowth.Crystals = SaveCrystalsPerArena * mArenasCleared;   // an older journey: she is paid for the fights already won
        Collection::State kept;
        read = true;
        for (int& charms : kept.Charms) read = read && static_cast<bool>(file >> charms);
        read = read && static_cast<bool>(file >> kept.Links >> kept.Feathers);
        for (int& runes : kept.Runes) read = read && static_cast<bool>(file >> runes);
        read = read && static_cast<bool>(file >> kept.Set);
        if (read)
        {
            for (int& charms : kept.Charms) charms = std::clamp(charms, 0, CharmSet);
            kept.Links = std::clamp(kept.Links, 0, Collection::LinkCount);
            kept.Feathers &= (1u << Collection::FeatherCount) - 1;
            for (int& runes : kept.Runes) runes = std::clamp(runes, 0, MaxRunes);
            if (kept.Set < -1 || kept.Set >= Collection::RuneCount || (kept.Set >= 0 && kept.Runes[kept.Set] == 0)) kept.Set = -1;
            mCollection = kept;
        }
        if (mCompleted) { mFinale = Finale::Done; GetAmiraniMountain().SetFreed(true); }
    }

    // ================================================================ the road
    int Journey::Nearest(float x, float z, int hint, int window) const
    {
        const int n = static_cast<int>(mSamples.size());
        const int a = hint < 0 ? 0 : std::max(0, hint - window), b = hint < 0 ? n - 1 : std::min(n - 1, hint + window);
        int best = a; float bestD = 1e30f;
        for (int i = a; i <= b; ++i)
        {
            const float dx = mSamples[i].P.x - x, dz = mSamples[i].P.z - z, d = dx * dx + dz * dz;
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    float Journey::GroundAt(float x, float z, int& index) const
    {
        if (z > LedgeWalkFrontZ && std::abs(x) < LedgeWalkHalfWidth + 2) return LedgeHeight;
        index = Nearest(x, z, index, 40);
        return mSamples[index].P.y;
    }

    XMFLOAT3 Journey::StartPoint() const
    {
        if (mSamples.empty()) return {0, 0, 95};
        return mSamples[std::clamp(static_cast<int>(StartS / mStep), 0, static_cast<int>(mSamples.size()) - 1)].P;
    }

    void Journey::Teleport(WorldState& world, float s)
    {
        const int i = std::clamp(static_cast<int>(s / mStep), 0, static_cast<int>(mSamples.size()) - 1);
        auto& p = world.MainPlayer;
        p.Position = mSamples[i].P; p.GroundY = p.Position.y; p.VerticalVelocity = 0; p.IsGrounded = true;
        p.Yaw = std::atan2(mSamples[i].Tx, mSamples[i].Tz);
        world.MainCamera.Yaw = p.Yaw;
        mIndex = i; mS = s;
    }

    // Trouble on the road. Fights are not all that stands between her and the mountain: between the arenas
    // (never near one, never in a city) the road itself is against her. The places are the same in every journey.
    //   cursed ground  lies over one half of the road; it bites while she stands in it: walk round it
    //   falling stones a stretch where stones come down on where she is; a ring shows first: keep moving
    //   wisps          three lights swing across the road; a touch burns: slip between them
    void Journey::BuildTroubles()
    {
        mTroubles.clear(); mStrikes.clear();
        if (mSamples.size() < 2) return;
        const int last = static_cast<int>(mSamples.size()) - 1;
        const float total = static_cast<float>(last) * mStep;
        unsigned seed = 0x7A3D11u;
        const auto next = [&] { seed = seed * 1664525u + 1013904223u; return static_cast<float>((seed >> 8) & 0xFFFFFF) / 16777216.f; };
        for (float s = TroubleFirst; s < total - TroubleLastBefore; s += TroubleEvery * (.75f + .5f * next()))
        {
            const float side = next() < .5f ? -1.f : 1.f, phase = next() * 6;
            bool clear = true;
            for (const auto& a : mArenas) if (std::abs(s - a.S) < a.R + TroubleClear) clear = false;
            for (const auto& c : mCities) if (s > c.S0 - TroubleClear && s < c.S1 + TroubleClear) clear = false;
            if (!clear) continue;
            const Sample& at = mSamples[std::clamp(static_cast<int>(s / mStep), 0, last)];
            Trouble t; t.Kind = static_cast<int>(mTroubles.size()) % 3 /* the three kinds in turn */; t.P = at.P; t.Phase = phase; t.S = s;
            t.Tx = at.Tx; t.Tz = at.Tz; t.Nx = at.Nx; t.Nz = at.Nz;
            if (t.Kind == 0)
            {   // over one side of the road, the other half stays free
                t.R = std::clamp(at.W * .55f, 2.2f, 4.f);
                const float lateral = side * (at.W - t.R + 1.2f);
                t.P.x += at.Nx * lateral; t.P.z += at.Nz * lateral;
            }
            else if (t.Kind == 1) t.R = RockfallReach;
            else t.R = at.W + 1.5f;   // how far the wisps swing to each side
            mTroubles.push_back(t);
        }
        Log("[JOURNEY] trouble on the road: %d places.\n", static_cast<int>(mTroubles.size()));
    }

    XMFLOAT3 Journey::WispAt(const Trouble& t, int which) const
    {
        const float lateral = std::sin(t.Clock * WispSpeed + t.Phase + static_cast<float>(which) * 2.1f) * t.R;
        const float along = (static_cast<float>(which) - static_cast<float>(WispCount - 1) * .5f) * WispGap;
        return {t.P.x + t.Nx * lateral + t.Tx * along, t.P.y + 1.f, t.P.z + t.Nz * lateral + t.Tz * along};
    }

    void Journey::UpdateTroubles(WorldState& world, CombatSystem& combat, float dt)
    {
        auto& kit = GetNyxaraKit();
        const XMFLOAT3 p = world.MainPlayer.Position;
        const bool live = !mFighting && !mCompleted && combat.PlayerActionsAllowed();
        const auto harm = [&](float base) { return std::max(1, static_cast<int>(base * DamageTaken())); };
        mTroubleNear = -1;
        for (int i = 0; i < static_cast<int>(mTroubles.size()); ++i)
        {
            Trouble& t = mTroubles[i];
            const float d = LenXZ(Sub(p, t.P));
            if (d > 80 || std::abs(p.y - t.P.y) > 8) continue;
            t.Clock += dt;
            if (!live) continue;
            if (mTroubleNear < 0 && d < t.R + TroubleWarn) mTroubleNear = i;
            if (t.Kind == 0)
            {
                if (d < t.R && kit.Hurt(world, combat, harm(MireDamage))) kit.Effect({p.x, p.y + .6f, p.z}, {.62f, .3f, .95f, 1}, 10, 2.f, .14f);
            }
            else if (t.Kind == 1)
            {
                if (d >= t.R) { t.Next = .6f; continue; }
                t.Next -= dt;
                if (t.Next > 0) continue;
                t.Next = RockfallEvery * (.8f + .4f * Random());
                const float angle = Random() * 2 * Pi, r = Random() * RockfallSpread;
                Strike strike; strike.P = {p.x + std::sin(angle) * r, p.y, p.z + std::cos(angle) * r}; strike.T = RockfallWarn;
                mStrikes.push_back(strike);
                kit.RingEffect({strike.P.x, strike.P.y + .05f, strike.P.z}, RockfallRadius + .6f, {1.f, .55f, .25f, 1}, RockfallWarn);
            }
            else
                for (int k = 0; k < WispCount; ++k)
                {
                    const XMFLOAT3 wisp = WispAt(t, k);
                    if (LenXZ(Sub(p, wisp)) < WispTouch && !kit.Blocks(world, wisp) && kit.Hurt(world, combat, harm(WispDamage)))
                        kit.Effect({p.x, p.y + 1.f, p.z}, {.55f, .85f, 1.f, 1}, 14, 3.f, .16f);
                }
        }
        for (auto& strike : mStrikes)
        {
            strike.T -= dt;
            if (strike.T > 0) continue;
            kit.Effect({strike.P.x, strike.P.y + .3f, strike.P.z}, {.8f, .7f, .6f, 1}, 24, 6.f, .22f);
            if (LenXZ(Sub(p, strike.P)) < 14) { kit.Shake(.16f); Sound::Play("combat/blow", .55f); }
            if (live && LenXZ(Sub(p, strike.P)) < RockfallRadius && !kit.Blocks(world, {strike.P.x, strike.P.y + 3, strike.P.z}))
                kit.Hurt(world, combat, harm(RockfallDamage));
        }
        std::erase_if(mStrikes, [](const Strike& strike) { return strike.T <= 0; });
    }

    void Journey::AddObstacle(float x, float z, float radius)
    {
        const long long key = (static_cast<long long>(std::floor(x / 16.f)) << 32) ^ (static_cast<long long>(std::floor(z / 16.f)) & 0xFFFFFFFFll);
        mObstacleGrid[key].push_back(static_cast<int>(mObstacles.size()));
        mObstacles.push_back({x, z, radius});
    }

    // The way. The whole land is hers to walk, so the game says where the road is whenever she is off it, and
    // where the fight is that she went past: a mark on a small compass at the top of the screen with the
    // distance, and a trail of lights on the ground from her feet toward it (Draw).
    void Journey::UpdateWay(const WorldState& world, float realDt)
    {
        bool show = false;
        const XMFLOAT3 p = world.MainPlayer.Position;
        if (!mFighting && !mCompleted && !mSamples.empty())
        {
            const int last = static_cast<int>(mSamples.size()) - 1;
            int goal = last, slack = 0;
            if (mArenasCleared < static_cast<int>(mArenas.size()))
            {
                const Arena& a = mArenas[mArenasCleared];
                goal = std::clamp(static_cast<int>(a.S / mStep), 0, last); slack = static_cast<int>(a.R / mStep) + 2;
            }
            const float beyond = std::abs(mLateral) - mSamples[mIndex].W;
            const bool off = beyond > WayOffRoad;
            mWayBack = mIndex > goal + slack;
            show = off || mWayBack;
            if (show)
            {
                const int reach = static_cast<int>(WayStep / mStep);
                const int to = std::clamp(mIndex + std::clamp(goal - mIndex, -reach, reach), 0, last);
                mWayTarget = mSamples[to].P;
                mWayDistance = off ? LenXZ(Sub(mWayTarget, p)) : static_cast<float>(mIndex - goal) * mStep;
            }
        }
        mWayShow = std::clamp(mWayShow + (show ? 1.f : -1.f) * realDt / WayFadeSeconds, 0.f, 1.f);
    }

    void Journey::Leash(WorldState& world)
    {
        auto& p = world.MainPlayer;
        const float x = p.Position.x, z = p.Position.z;
        mLateral = 0;
        const bool eagleDown = mFinale == Finale::Chains || mFinale == Finale::Freed || mFinale == Finale::Done;
        if (z > LedgeWalkFrontZ - .5f && z < LedgeWalkBackZ + 1 && std::abs(x) < LedgeWalkHalfWidth + 1 && eagleDown)
        { mIndex = static_cast<int>(mSamples.size()) - 1; mS = mIndex * mStep; mStage = 11; return; }

        mIndex = Nearest(x, z, mIndex, 90);
        {
            const float dx = mSamples[mIndex].P.x - x, dz = mSamples[mIndex].P.z - z;
            if (dx * dx + dz * dz > 90.f * 90.f) mIndex = Nearest(x, z, -1, 0);
        }
        mS = mIndex * mStep;
        mStage = mSamples[mIndex].Stage;

        float minS = 0, maxS = static_cast<float>(mSamples.size() - 1) * mStep;
        if (!eagleDown) maxS -= 1.0f;
        if (mArenasCleared < static_cast<int>(mArenas.size()))
        {
            const Arena& a = mArenas[mArenasCleared];
            if (a.Kind != 2) maxS = std::min(maxS, CageEnd(a, 1));
            if (mFighting) minS = CageEnd(a, -1);
        }
        // where she is along the road to the centimetre (mS is rounded to a sample, 2 m apart)
        const float exact = mS + (x - mSamples[mIndex].P.x) * mSamples[mIndex].Tx + (z - mSamples[mIndex].P.z) * mSamples[mIndex].Tz;
        float past = exact > maxS ? exact - maxS : exact < minS ? exact - minS : 0.f;
        if (!mFighting)
        {   // Outside a fight the wall of the next arena stands across the road only. On the open land she walks
            // round it, and once she is past it, it does not pull her back.
            const float side = (x - mSamples[mIndex].P.x) * mSamples[mIndex].Nx + (z - mSamples[mIndex].P.z) * mSamples[mIndex].Nz;
            const bool atWall = past > 0 && past <= 6.f && std::abs(side) <= mSamples[mIndex].W + WallReach && !mPastWall;
            if (past > 0 && !atWall) mPastWall = true;
            else if (past <= 0) mPastWall = false;
            if (!atWall) past = 0;
        }
        else mPastWall = false;
        int index = mIndex; bool fix = false;
        if (std::abs(past) > 6.f)
        {   // far outside (a respawn, a checkpoint): put her back on the road
            index = past > 0 ? static_cast<int>(maxS / mStep) : static_cast<int>(minS / mStep) + 1; fix = true;
        }
        else if (past != 0)
        {   // at a wall: she is held exactly at it, so she and the camera do not jump
            p.Position.x -= mSamples[mIndex].Tx * past; p.Position.z -= mSamples[mIndex].Tz * past;
        }
        const Sample& s = mSamples[index];
        const float lateral = (p.Position.x - mSamples[mIndex].P.x) * mSamples[mIndex].Nx + (p.Position.z - mSamples[mIndex].P.z) * mSamples[mIndex].Nz;
        mLateral = lateral;
        if (!fix)
        {   // the edge of where she may go: she slides along it (the collision is wider, so the engine never stops her dead)
            const Sample& here = mSamples[mIndex];
            const float limit = here.W - .5f;
            if (mFighting && std::abs(lateral) > limit)   // a fight holds her in its clearing; otherwise the land is hers
            {
                const float over = lateral - std::clamp(lateral, -limit, limit);
                p.Position.x -= here.Nx * over; p.Position.z -= here.Nz * over;
                mLateral = lateral - over;
            }
            // off the road she walks round trees, boulders, towers and the piers of the gates
            const int gx = static_cast<int>(std::floor(p.Position.x / 16.f)), gz = static_cast<int>(std::floor(p.Position.z / 16.f));
            for (int cz = gz - 1; cz <= gz + 1; ++cz)
                for (int cx = gx - 1; cx <= gx + 1; ++cx)
                {
                    const auto cell = mObstacleGrid.find((static_cast<long long>(cx) << 32) ^ (static_cast<long long>(cz) & 0xFFFFFFFFll));
                    if (cell == mObstacleGrid.end()) continue;
                    for (const int i : cell->second)
                    {
                        const Obstacle& o = mObstacles[i];
                        const float keep = o.R + RoamBody, dx = p.Position.x - o.X, dz = p.Position.z - o.Z, d2 = dx * dx + dz * dz;
                        if (d2 >= keep * keep || d2 < 1e-6f) continue;
                        const float d = std::sqrt(d2);
                        p.Position.x = o.X + dx / d * keep; p.Position.z = o.Z + dz / d * keep;
                    }
                }
            if (std::abs(mLateral) > here.W) GetNature().PushFromTrees(p.Position.x, p.Position.z, RoamBody);
            return;
        }
        const float keep = std::clamp(lateral, -(s.W - .5f), s.W - .5f);
        p.Position = {s.P.x + s.Nx * keep, s.P.y, s.P.z + s.Nz * keep};
        p.GroundY = s.P.y; p.VerticalVelocity = 0;
        mIndex = index; mS = index * mStep;
    }

    // ================================================================ monsters
    int Journey::Spawn(int kind, XMFLOAT3 at, bool encounter, float healthScale)
    {
        auto& enemies = GetNyxaraKit().Enemies();
        int slot = -1;
        for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i)
            if (!mMonsters[i].Active) { slot = i; break; }
        if (slot < 0)
        {
            if (static_cast<int>(mMonsters.size()) >= NyxaraKit::MaxEnemies) return -1;
            mMonsters.emplace_back(); enemies.emplace_back();
            slot = static_cast<int>(mMonsters.size()) - 1;
        }
        const KindDef& def = Kinds[kind];
        Monster m;
        m.Kind = kind; m.Active = true; m.Encounter = encounter;
        m.Cooldown = .8f + Random(); m.Shoot = 1.5f + 2 * Random(); m.Special = def.Boss ? 5.f : 3.f + 4 * Random();
        m.Phase = Random() * 6; m.Strafe = Random() < .5f ? -1.f : 1.f; m.Hover = def.Hover;
        m.Index = Nearest(at.x, at.z, -1, 0);
        m.Ground = at.y;
        mMonsters[slot] = m;
        NyxaraKit::Wraith w;
        w.Alive = true; w.MaxHealth = w.Health = std::max(1, static_cast<int>(static_cast<float>(def.Health) * healthScale));
        w.Radius = def.Radius; w.PushScale = def.PushScale;
        w.Pos = {at.x, at.y + 1.3f, at.z}; w.Anchor = w.Pos; w.Fade = 1;
        enemies[slot] = w;
        if (kind != ChestKind && kind != ChainKind)
        {
            GetNyxaraKit().Effect(w.Pos, def.Color, def.Boss ? 60 : 18, def.Boss ? 7.f : 3.f, .3f);
            GetNyxaraKit().RingEffect({at.x, at.y + .05f, at.z}, def.Radius * 3 + 1, def.Color, .7f);
        }
        return slot;
    }

    void Journey::Release(int slot)
    {
        if (slot < 0 || slot >= static_cast<int>(mMonsters.size())) return;
        if (mMonsters[slot].Chest >= 0) mChests[mMonsters[slot].Chest].Slot = -1;
        mMonsters[slot] = Monster{};
        GetNyxaraKit().Enemies()[slot].Alive = false;
    }

    int Journey::AliveEncounterCount() const
    {
        int count = 0;
        for (const auto& m : mMonsters) if (m.Active && m.Encounter && !m.Dying) ++count;
        return count;
    }

    void Journey::BuildWave(int arena, int wave, std::vector<int>& out) const
    {
        const Arena& a = mArenas[arena];
        int inStage = 0;
        for (int i = 0; i < arena; ++i) if (mArenas[i].Stage == a.Stage) ++inStage;
        const auto& roster = Rosters[a.Stage];
        if (a.Kind == 1 && wave == mWaves - 1) { out.push_back(StageBoss[a.Stage]); return; }
        float budget = (4.f + 1.4f * static_cast<float>(a.Stage)) * (1 + .2f * static_cast<float>(inStage)) * (.8f + .2f * static_cast<float>(wave));
        int cheapest = roster[0];
        for (int k : roster) if (Kinds[k].Cost < Kinds[cheapest].Cost) cheapest = k;
        for (int pick = wave + inStage; budget >= static_cast<float>(Kinds[cheapest].Cost) && out.size() < 11; ++pick)
        {
            int kind = roster[static_cast<size_t>(pick) % roster.size()];
            if (static_cast<float>(Kinds[kind].Cost) > budget) kind = cheapest;
            out.push_back(kind); budget -= static_cast<float>(Kinds[kind].Cost);
        }
    }

    void Journey::StartEncounter(int arena)
    {
        const Arena& a = mArenas[arena];
        mFighting = true; mWave = 0;
        mWaves = a.Kind == 1 ? 2 : (a.Stage == 0 ? 2 : (a.Stage >= LateWaveStage ? 4 : 3));
        mWaveDelay = 1.0f; mQueue.clear();
        GetNyxaraKit().Notice(a.Kind == 1 ? L"SOMETHING VAST IS WAKING" : L"AMBUSH", 1.6f);
        if (!Sound::Narrating()) Sound::Play("nyxara/say_fight", 1, 0);
        Log("[JOURNEY] arena %d (stage %d) begins at %.0f s played.\n", arena, a.Stage, mPlaySeconds);
    }

    void Journey::ClearArena(WorldState& world, CombatSystem& combat)
    {
        const int arena = mArenasCleared;
        mFighting = false; ++mArenasCleared;
        combat.RestorePlayerHealth(world, static_cast<int>(ArenaHeal * (Gift(1) ? SpringsHealScale : 1.f)));
        GetNyxaraKit().Notice(L"THE WAY IS OPEN", 2.2f);
        Sound::Play("combat/victory", .9f, 0); Sound::Play("nyxara/say_win", 1, 0);
        GetNyxaraKit().RingEffect(Add(mArenas[arena].P, {0, .05f, 0}), mArenas[arena].R, {.7f, 1.f, .85f, 1}, 1.2f);
        Log("[JOURNEY] arena %d cleared at %.0f s played (gold %d, feathers %d, deaths %d).\n", arena, mPlaySeconds, mGold, mFeathers, mDeaths);
        Save();
    }

    void Journey::UpdatePatrols(WorldState&, CombatSystem& combat)
    {
        const int count = static_cast<int>(mPatrols.size());
        while (mPatrolNext < count && mPatrols[mPatrolNext] < mS + 8) ++mPatrolNext;   // already behind her (checkpoint, load)
        for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i)
        {   // a pack she has outrun gives up
            const Monster& m = mMonsters[i];
            if (m.Active && !m.Dying && !m.Encounter && m.Kind < ChestKind && static_cast<float>(mIndex - m.Index) * mStep > PatrolGiveUp) Release(i);
        }
        if (mFighting || mPatrolNext >= count || !combat.PlayerActionsAllowed()) return;
        const float s = mPatrols[mPatrolNext];
        if (s - mS > PatrolAhead) return;
        const int stage = mSamples[std::clamp(static_cast<int>(s / mStep), 0, static_cast<int>(mSamples.size()) - 1)].Stage;
        const auto& roster = Rosters[stage];
        int cheapest = roster[0];
        for (int k : roster) if (Kinds[k].Cost < Kinds[cheapest].Cost) cheapest = k;
        float budget = std::max(static_cast<float>(Kinds[cheapest].Cost) * 2, (4.f + 1.4f * static_cast<float>(stage)) * PatrolBudget);
        const float scale = MonsterHealthScale * (1 + StageHealthGrowth * static_cast<float>(stage));
        int spawned = 0;
        for (int pick = mPatrolNext; budget >= static_cast<float>(Kinds[cheapest].Cost) && spawned < 5; ++pick, ++spawned)
        {
            int kind = roster[static_cast<size_t>(pick) % roster.size()];
            if (static_cast<float>(Kinds[kind].Cost) > budget) kind = cheapest;
            budget -= static_cast<float>(Kinds[kind].Cost);
            const float at = s + (Random() * 2 - 1) * 5;
            const Sample& sample = mSamples[std::clamp(static_cast<int>(at / mStep), 0, static_cast<int>(mSamples.size()) - 1)];
            const float lateral = (Random() * 2 - 1) * std::max(.5f, sample.W - 1.5f - Kinds[kind].Radius);
            Spawn(kind, {sample.P.x + sample.Nx * lateral, sample.P.y, sample.P.z + sample.Nz * lateral}, false, scale);
        }
        Log("[JOURNEY] roaming pack %d of %d (%d creatures) at %.0f m.\n", mPatrolNext + 1, count, spawned, s);
        ++mPatrolNext;
    }

    void Journey::UpdateEncounter(WorldState& world, CombatSystem& combat, float dt)
    {
        if (mArenasCleared >= static_cast<int>(mArenas.size())) return;
        const int arena = mArenasCleared;
        const Arena& a = mArenas[arena];
        if (a.Kind == 2) return; // the finale has its own script
        if (!mFighting)
        {
            // the ambush springs when she stands in the clearing itself, not when she passes beside it on the land
            if (mS > a.S - a.R * .55f && mS < a.S + a.R && std::abs(mLateral) <= mSamples[mIndex].W && combat.PlayerActionsAllowed()) StartEncounter(arena);
            return;
        }
        if (!mQueue.empty())
        {
            mSpawnTimer -= dt;
            if (mSpawnTimer <= 0)
            {
                mSpawnTimer = SpawnInterval;
                const int kind = mQueue.back(); mQueue.pop_back();
                XMFLOAT3 at = a.P;
                for (int attempt = 0; attempt < 8; ++attempt)
                {
                    const float s = a.S + (Random() * 2 - 1) * a.R * .75f;
                    const Sample& sample = mSamples[std::clamp(static_cast<int>(s / mStep), 0, static_cast<int>(mSamples.size()) - 1)];
                    const float lateral = (Random() * 2 - 1) * (sample.W - 1.5f - Kinds[kind].Radius);
                    at = {sample.P.x + sample.Nx * lateral, sample.P.y, sample.P.z + sample.Nz * lateral};
                    if (LenXZ(Sub(at, world.MainPlayer.Position)) > 7.f + Kinds[kind].Radius) break;
                }
                const float scale = (Kinds[kind].Boss ? BossHealthScale : MonsterHealthScale) * (1 + StageHealthGrowth * static_cast<float>(a.Stage));
                const int slot = Spawn(kind, at, true, scale);
                if (slot >= 0) Sound::Play(Kinds[kind].Boss ? "creatures/boss_roar" : "creatures/growl", Kinds[kind].Boss ? 1.f : .55f, .2f);
                if (slot >= 0 && Kinds[kind].Boss)
                    Announce(L"A GUARDIAN OF THE ROAD", Kinds[kind].Name, kind == Gveleshapi ? L"The water stands up. It has teeth." :
                        kind == DeviLord ? L"\"Little bird. I have eaten eagles.\"" : L"\"He sent you. Of course he did.\"", 5.5f);
            }
            return;
        }
        if (AliveEncounterCount() > 0) return;
        mWaveDelay -= dt;
        if (mWaveDelay > 0) return;
        if (mWave >= mWaves) { ClearArena(world, combat); return; }
        BuildWave(arena, mWave, mQueue);
        if (mWave > 0) combat.RestorePlayerHealth(world, static_cast<int>(WaveHeal * (Gift(1) ? SpringsHealScale : 1.f)));
        ++mWave; mWaveDelay = WaveDelay; mSpawnTimer = 0;
        if (mWave > 1 && !(a.Kind == 1 && mWave == mWaves)) GetNyxaraKit().Notice(mWave == mWaves ? L"LAST WAVE" : L"MORE ARE COMING", 1.4f);
    }

    void Journey::OnDeath(int slot, WorldState& world, CombatSystem& combat)
    {
        Monster& m = mMonsters[slot];
        m.Dying = true; m.Death = .55f;
        const KindDef& def = Kinds[m.Kind];
        const XMFLOAT3 at = GetNyxaraKit().Enemies()[slot].Pos;
        if (m.Kind == ChestKind) { Sound::Play("world/chest", .9f); OpenChest(m.Chest, world, combat); return; }
        if (m.Kind == ChainKind) { Sound::Play("combat/chain", 1, .03f); GetNyxaraKit().Effect(at, {.8f, .9f, 1.f, 1}, 60, 8.f, .35f); GetNyxaraKit().Shake(.25f); return; }
        mGold += def.Boss ? 150 : def.Cost * 2 + 1;
        mGrowth.Crystals += def.Boss ? BossCrystals : static_cast<int>(std::ceil(static_cast<float>(def.Cost) * Collection::Crystals(CitiesVisited())));
        if (m.Kind < Collection::CharmKinds && mCollection.Charms[m.Kind] < CharmSet && Random() < CharmChance)
        {   // it drops its charm; she keeps it (her window, second page)
            const int have = ++mCollection.Charms[m.Kind];
            Sound::Play("world/charm", .8f, 0);
            Notify(HudArt::IconSkull, have >= CharmSet ? Palette::GoldLight : Palette::Silver, Ui::Upper(Collection::CharmName(m.Kind)),
                   have >= CharmSet ? std::wstring(L"The set is complete: you strike the ") + Collection::CreatureName(m.Kind) + L" harder and it hurts you less."
                                    : std::to_wstring(have) + L" of " + std::to_wstring(CharmSet) + L". Press I, then D, to see what you carry.", 5);
            Save();
        }
        if (def.Boss && m.Kind != Eagle && mCollection.Links < Collection::LinkCount)
        {
            ++mCollection.Links;
            Notify(HudArt::IconMountain, Palette::Silver, L"A LINK OF AMIRANI'S CHAIN",
                   mCollection.Links >= Collection::LinkCount ? L"You hold all three. The end of the road will not be the same."
                                                              : std::to_wstring(mCollection.Links) + L" of 3. Every guardian of the road carries one.", 8);
        }
        Sound::Play(def.Boss ? "creatures/boss_death" : "creatures/death", def.Boss ? 1.f : .7f, .18f); Sound::Play("world/crystal", .5f, .1f);
        if (!def.Boss && combat.PlayerActionsAllowed()) combat.RestorePlayerHealth(world, KillHeal + (Gift(0) ? WineKillHeal : 0) + Collection::KillHeal(mCollection));
        GetNyxaraKit().Effect(at, def.Color, def.Boss ? 90 : 14, def.Boss ? 9.f : 4.f, .3f);
        if (def.Boss)
        {
            GetNyxaraKit().Shake(.35f);
            for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i) // the pack falls with its master
                if (i != slot && mMonsters[i].Active && mMonsters[i].Encounter && !mMonsters[i].Dying) GetNyxaraKit().Enemies()[i].Health = 0, GetNyxaraKit().Enemies()[i].Alive = false;
            Log("[JOURNEY] boss %ls defeated at %.0f s played.\n", def.Name, mPlaySeconds);
        }
    }

    void Journey::UpdateBoss(int slot, WorldState& world, CombatSystem& combat, float dt, float distance, XMFLOAT3 toPlayer)
    {
        auto& kit = GetNyxaraKit();
        Monster& m = mMonsters[slot];
        auto& e = kit.Enemies()[slot];
        const KindDef& def = Kinds[m.Kind];
        const float health = static_cast<float>(e.Health) / static_cast<float>(e.MaxHealth);
        const XMFLOAT3 chest = Add(world.MainPlayer.Position, {0, 1.2f, 0});
        const auto ring = [&](int count, float speed, int damage)
        {
            for (int i = 0; i < count; ++i)
            {
                const float a = 2 * Pi * (static_cast<float>(i) + .5f * static_cast<float>(m.Stage % 2)) / static_cast<float>(count);
                kit.SpawnOrb(Add(e.Pos, {std::sin(a) * def.Radius, 0, std::cos(a) * def.Radius}), {std::sin(a) * speed, 0, std::cos(a) * speed}, damage, def.Color, 3.2f);
            }
            kit.RingEffect({e.Pos.x, m.Ground + .05f, e.Pos.z}, def.Radius * 2.5f, def.Color, .6f);
        };
        const auto summon = [&](int kind, int count)
        {
            if (mArenasCleared >= static_cast<int>(mArenas.size())) return;
            const Arena& a = mArenas[mArenasCleared];
            for (int i = 0; i < count && AliveEncounterCount() < 9; ++i)
            {
                const float ang = Random() * 2 * Pi, r = a.R * (.3f + .5f * Random());
                int index = m.Index;
                const XMFLOAT3 at{a.P.x + std::sin(ang) * r, 0, a.P.z + std::cos(ang) * r};
                Spawn(kind, {at.x, GroundAt(at.x, at.z, index), at.z}, true, MonsterHealthScale * (1 + StageHealthGrowth * static_cast<float>(a.Stage)));
            }
        };
        // dive / slam in flight
        if (m.Stage >= 100)
        {   // Eagle dive: a marked circle, then he comes through it
            m.Special -= dt;
            if (m.Special <= 0)
            {
                e.Pos = {m.Aim.x, m.Aim.y + 1.3f, m.Aim.z};
                kit.RingEffect({m.Aim.x, m.Aim.y + .05f, m.Aim.z}, 4.5f, {1, 1, 1, 1}, .5f);
                kit.Effect(e.Pos, def.Color, 40, 8.f, .3f); kit.Shake(.3f);
                if (LenXZ(Sub(world.MainPlayer.Position, m.Aim)) < 3.6f && combat.PlayerActionsAllowed() && !kit.Blocks(world, e.Pos))
                    kit.Hurt(world, combat, static_cast<int>(22 * DamageTaken()));
                m.Stage -= 100; m.Special = health < .5f ? 4.5f : 6.5f; m.Lunge = .4f;
            }
            return;
        }
        m.Special -= dt;
        if (m.Special > 0) return;
        switch (m.Kind)
        {
        case Rokapi:
        {
            if (mArenasCleared < static_cast<int>(mArenas.size()))
            {   // she steps through the dark to another place on the hill
                const Arena& a = mArenas[mArenasCleared];
                kit.Effect(e.Pos, def.Color, 30, 5.f, .3f);
                const float ang = Random() * 2 * Pi, r = a.R * .6f;
                e.Pos.x = a.P.x + std::sin(ang) * r; e.Pos.z = a.P.z + std::cos(ang) * r;
                kit.Effect(e.Pos, def.Color, 30, 5.f, .3f);
            }
            if (m.Stage % 3 == 0 && AliveEncounterCount() < 4) summon(Kudiani, 2);   // her court comes now and then, not without end
            m.Special = health < .5f ? 9.f : 12.f; ++m.Stage;
            break;
        }
        case Gveleshapi:
            if (health < .5f && m.Stage < 50) { summon(Ali, 3); m.Stage += 50; kit.Notice(L"THE LAKE ANSWERS ITS MASTER", 2); }
            ring(10, 6.5f, static_cast<int>(8 * DamageTaken())); ++m.Stage;
            m.Special = health < .5f ? 6.f : 9.f;
            break;
        case DeviLord:
            if ((health < .66f && m.Stage < 30) || (health < .33f && m.Stage < 60)) { summon(Chinka, 4); m.Stage += 30; }
            ring(10, 7.f, static_cast<int>(9 * DamageTaken())); kit.Shake(.2f); ++m.Stage;
            m.Special = 8.f;
            break;
        case Eagle:
            if (m.Stage % 2 == 0)
            {   // dive
                m.Aim = world.MainPlayer.Position;
                kit.RingEffect({m.Aim.x, m.Aim.y + .05f, m.Aim.z}, 4.2f, {1.f, .35f, .25f, 1}, 1.0f);
                kit.Notice(L"HE DIVES", .9f);
                m.Stage += 101; m.Special = 1.0f;
            }
            else
            {
                ring(health < .5f ? 16 : 12, 7.5f, static_cast<int>(9 * DamageTaken())); ++m.Stage;
                m.Special = health < .5f ? 4.5f : 6.5f;
            }
            break;
        default: m.Special = 5; break;
        }
        (void)distance; (void)toPlayer; (void)chest;
    }

    void Journey::UpdateMonsters(WorldState& world, CombatSystem& combat, float dt)
    {
        auto& kit = GetNyxaraKit();
        auto& enemies = kit.Enemies();
        const auto& player = world.MainPlayer;
        const XMFLOAT3 chest = Add(player.Position, {0, 1.2f, 0});
        const bool alive = combat.PlayerActionsAllowed();
        const int count = static_cast<int>(mMonsters.size());
        const Arena* arena = mFighting && mArenasCleared < static_cast<int>(mArenas.size()) ? &mArenas[mArenasCleared] : nullptr;

        {   // her velocity, smoothed; a jump in position (respawn, checkpoint) is not movement
            XMFLOAT3 step = Sub(player.Position, mPlayerLast); step.y = 0;
            const XMFLOAT3 now = dt > 1e-4f && LenXZ(step) < 3.f ? Mul(step, 1 / dt) : XMFLOAT3{0, 0, 0};
            const float blend = std::min(1.f, dt * 8);
            mPlayerVelocity = Add(Mul(mPlayerVelocity, 1 - blend), Mul(now, blend));
            mPlayerLast = player.Position;
        }
        int windingUp = 0;   // ordinary creatures in the middle of a blow
        int pack = 0;        // creatures alive
        for (int i = 0; i < count; ++i)
            if (mMonsters[i].Active && !mMonsters[i].Dying && mMonsters[i].Kind < ChestKind) ++pack;
        const XMFLOAT3 facing{std::sin(player.Yaw), 0, std::cos(player.Yaw)};
        for (int i = 0; i < count; ++i)
            if (mMonsters[i].Active && !mMonsters[i].Dying && mMonsters[i].Kind < ChestKind && mMonsters[i].Windup >= 0 && !Kinds[mMonsters[i].Kind].Boss) ++windingUp;

        for (int i = 0; i < count; ++i)
        {
            Monster& m = mMonsters[i];
            if (!m.Active) continue;
            auto& e = enemies[i];
            if (m.Dying)
            {
                m.Death -= dt;
                if (m.Death <= 0) Release(i);
                continue;
            }
            if (!e.Alive) { OnDeath(i, world, combat); continue; }
            m.Spawn = std::min(1.f, m.Spawn + dt * 2.2f);
            m.Phase += dt;
            m.Lunge = std::max(0.f, m.Lunge - dt);
            if (m.Kind == ChestKind || m.Kind == ChainKind) continue;
            const KindDef& def = Kinds[m.Kind];
            const float damageScale = DamageTaken() * (1 + StageDamageGrowth * static_cast<float>(mSamples[m.Index].Stage)) * Collection::DamageFrom(mCollection, m.Kind);
            e.Weak = Collection::DamageTo(mCollection, m.Kind);

            XMFLOAT3 to = Sub(player.Position, e.Pos); to.y = 0;
            const float distance = LenXZ(to);
            const XMFLOAT3 dir = Norm(to);
            e.Yaw = AngleTo(e.Yaw, std::atan2(dir.x, dir.z), MonsterTurnRate * dt);
            if (def.Boss) UpdateBoss(i, world, combat, dt, distance, to);
            if (m.Stage >= 100) { e.Pos.y = m.Ground + 1.3f; continue; } // mid-dive

            // ---- melee
            m.Cooldown -= dt;
            bool rooted = false;
            if (m.Windup >= 0)
            {
                rooted = true;
                m.Windup -= dt;
                if (m.Windup < 0)
                {
                    const int blow = m.Blow; m.Blow = 0;
                    const bool heavy = blow == 1;
                    const float lands = def.Reach + def.Radius + (heavy ? HeavyBlowReach : .9f);
                    m.Lunge = .28f; m.Cooldown = def.Cooldown * MonsterAttackPace * Collection::CreaturePace(mCollection) * (.85f + .3f * Random()) * (heavy ? 1.25f : 1.f);
                    if (distance < lands && alive)
                    {
                        if (kit.Blocks(world, e.Pos)) { if (!def.Boss) m.Round = RoundGuardSeconds; }   // her guard is up: go round it
                        else
                        {
                            kit.Hurt(world, combat, std::max(1, static_cast<int>(static_cast<float>(def.Damage) * damageScale * (heavy ? HeavyBlowDamage : 1.f))));
                            kit.Effect(chest, {1.f, .3f, .25f, 1}, heavy ? 22 : 10, 3.f, .2f); kit.Shake(def.Damage > 12 || heavy ? .22f : .1f);
                        }
                    }
                    if (def.Radius > 1.2f || heavy) kit.RingEffect({e.Pos.x, m.Ground + .05f, e.Pos.z}, lands, def.Color, .45f);
                    if (!def.Boss && blow == 0 && alive)
                    {
                        if (def.Windup < QuickWindup && distance < lands + 1.5f && Random() < FollowUpChance)
                        {   // quick creatures strike twice
                            m.Blow = 2; m.Windup = m.WindupFull = def.Windup * FollowUpWindup; rooted = true;
                            kit.RingEffect({e.Pos.x, m.Ground + .05f, e.Pos.z}, def.Reach + def.Radius, {1.f, .35f, .25f, 1}, m.Windup);
                        }
                        else if (def.Radius < .6f && def.Keep == 0)
                        {   // small ones strike and spring back out of her reach
                            m.Hop = HopSeconds; m.HopDir = Norm(Add(Mul(dir, -1.f), Mul(XMFLOAT3{dir.z, 0, -dir.x}, Random() - .5f)));
                        }
                    }
                }
            }
            else if (alive && m.Cooldown <= 0 && distance < def.Reach + def.Radius + .2f && def.Reach > 0 && !def.Boss && windingUp >= MaxAttackers)
                m.Cooldown = .25f + .35f * Random();   // others are already striking: wait for a turn
            else if (alive && m.Cooldown <= 0 && distance < def.Reach + def.Radius + .2f && def.Reach > 0 && m.Slam <= 0)   // not in the middle of striking the ground
            {
                if (!def.Boss) ++windingUp;
                m.Blow = !def.Boss && Random() < HeavyBlowChance ? 1 : 0;
                m.Windup = m.WindupFull = def.Windup * (m.Blow == 1 ? HeavyBlowWindup : 1.f); rooted = true; Sound::Play("creatures/attack", m.Blow == 1 ? .8f : .6f, .2f);
                if (m.Blow == 1) kit.RingEffect({e.Pos.x, m.Ground + .05f, e.Pos.z}, def.Reach + def.Radius + HeavyBlowReach, {1.f, .72f, .15f, 1}, m.Windup);   // heavy: a wider, yellow ring
                else kit.RingEffect({e.Pos.x, m.Ground + .05f, e.Pos.z}, def.Reach + def.Radius, {1.f, .35f, .25f, 1}, m.Windup);
            }

            // ---- giants strike the ground where she stands, from a distance
            if (!def.Boss && def.Radius > 1.2f)
            {
                if (m.Slam > 0)
                {
                    rooted = true; m.Slam -= dt;
                    if (m.Slam <= 0)
                    {
                        m.Slam = 0; m.SlamWait = SlamEvery * (.8f + .4f * Random()); m.Lunge = .28f;
                        kit.Effect(Add(m.Aim, {0, .6f, 0}), def.Color, 36, 7.f, .3f); kit.Shake(.25f);
                        kit.RingEffect({m.Aim.x, m.Aim.y + .05f, m.Aim.z}, SlamRadius * 1.4f, def.Color, .5f);
                        Sound::Play("combat/blow", .8f, .15f);
                        if (alive && LenXZ(Sub(player.Position, m.Aim)) < SlamRadius && !kit.Airborne() && !kit.Blocks(world, e.Pos))
                            kit.Hurt(world, combat, std::max(1, static_cast<int>(static_cast<float>(def.Damage) * damageScale * SlamDamage)));
                    }
                }
                else if (alive && m.Windup < 0)
                {
                    m.SlamWait -= dt;
                    if (m.SlamWait <= 0 && distance > 5.f && distance < SlamRange)
                    {
                        m.Aim = Add(player.Position, Mul(mPlayerVelocity, .45f)); m.Slam = SlamSeconds; rooted = true;
                        kit.RingEffect({m.Aim.x, m.Aim.y + .05f, m.Aim.z}, SlamRadius, {1.f, .35f, .25f, 1}, SlamSeconds);
                        Sound::Play("creatures/attack", .8f, .2f);
                    }
                }
            }

            // ---- ranged
            if (def.Range > 0 && alive)
            {
                m.Shoot -= dt;
                if (m.Shoot <= 0 && distance < def.Range && (def.Keep > 0 || distance > 5.f) && !rooted)
                {
                    const XMFLOAT3 from = Add(e.Pos, {0, def.Height * .25f, 0});
                    const float speed = def.Boss ? 9.5f : 8.f;
                    // aimed at where she will be when the shot arrives
                    const XMFLOAT3 mark = Add(chest, Mul(mPlayerVelocity, RangedLead * std::min(distance / speed, 1.2f)));
                    const float base = std::atan2(mark.x - from.x, mark.z - from.z);
                    const float fall = (chest.y - from.y) / std::max(distance, 1.f);
                    ++m.Shots;
                    int volley = def.Volley;
                    if (!def.Boss && def.Keep > 0 && m.Shots % FanEvery == 0) volley = 3;          // spirits and witches: a fan
                    if (!def.Boss && m.Kind == Kaji && m.Shots % 3 == 0)
                    {   // the Kaji breathes a ring of fire round itself
                        volley = 0;
                        for (int v = 0; v < KajiNovaOrbs; ++v)
                        {
                            const float a = 2 * Pi * static_cast<float>(v) / static_cast<float>(KajiNovaOrbs);
                            kit.SpawnOrb(from, {std::sin(a) * 6.f, 0, std::cos(a) * 6.f}, std::max(1, static_cast<int>(static_cast<float>(def.OrbDamage) * damageScale)), def.Color, 2.4f);
                        }
                        kit.RingEffect({e.Pos.x, m.Ground + .05f, e.Pos.z}, def.Radius * 3, def.Color, .5f);
                    }
                    for (int v = 0; v < volley; ++v)
                    {
                        const float a = base + (static_cast<float>(v) - static_cast<float>(volley - 1) * .5f) * .2f;
                        kit.SpawnOrb(from, {std::sin(a) * speed, fall * speed, std::cos(a) * speed}, std::max(1, static_cast<int>(static_cast<float>(def.OrbDamage) * damageScale)), def.Color);
                    }
                    m.Shoot = def.OrbEvery * (.8f + .4f * Random());
                    m.Lunge = .2f;
                }
            }

            // ---- movement
            XMFLOAT3 move{0, 0, 0};
            if (!rooted && alive)
            {
                float speed = def.Speed * (def.Boss ? 1.f : MonsterSpeedScale);
                if (!def.Boss && distance > MonsterSprintDistance) speed *= MonsterSprint;      // left behind: catch up
                // run to where she is going, not to where she was
                XMFLOAT3 ahead = Add(to, Mul(mPlayerVelocity, std::min(MonsterLeadSeconds, distance / std::max(speed, .1f))));
                const XMFLOAT3 chase = LenXZ(ahead) > .5f ? Norm(ahead) : dir;
                const XMFLOAT3 side{dir.z * (i % 2 ? 1.f : -1.f), 0, -dir.x * (i % 2 ? 1.f : -1.f)};   // odd and even creatures take opposite sides
                if (!def.Boss && def.Range == 0)
                {   // leapers close the last metres in a rush
                    m.Special -= dt;
                    if (m.Special <= 0 && distance > 4 && distance < 13) { m.Special = 5 + 3 * Random(); m.Hover = -1; }
                    if (m.Hover < 0) { speed *= 2.4f; m.Hover += dt * 1.2f; if (m.Hover > -.05f || distance < def.Reach + def.Radius) m.Hover = 0; }
                }
                // toward her back: the side that leads round behind her
                const XMFLOAT3 round = (side.x * facing.x + side.z * facing.z) < 0 ? side : Mul(side, -1.f);
                const bool behind = dir.x * facing.x + dir.z * facing.z > .3f;      // it is behind her already
                if (m.Round > 0) { m.Round -= dt; if (behind) m.Round = 0; }
                m.Dodge -= dt;
                if (!def.Boss && def.Radius < .6f && m.Dodge <= 0 && kit.Busy() && distance < DodgeDistance && dir.x * facing.x + dir.z * facing.z < -.5f)
                {   // she attacks and it is in front of her: now and then it jumps aside
                    m.Dodge = DodgeEvery * (.7f + .6f * Random());
                    if (Random() < DodgeChance) { m.Hop = HopSeconds; m.HopDir = Norm(Add(Mul(side, Random() < .5f ? 1.f : -1.f), Mul(dir, -.6f))); }
                }
                const bool wounded = !def.Boss && def.Cost <= 2 && def.Keep == 0 && pack > 1 &&
                                     static_cast<float>(e.Health) < WoundedShare * static_cast<float>(e.MaxHealth);
                if (m.Hop > 0) { m.Hop -= dt; move = Mul(m.HopDir, speed * HopSpeed); }
                else if (m.Round > 0 && distance < def.Reach + def.Radius + 3.f)
                    move = Mul(Norm(Add(round, Mul(dir, distance > def.Reach + def.Radius ? .5f : -.15f))), speed * 1.15f);
                else if (wounded && m.Cooldown > .7f)
                {   // badly hurt: it stays out of her reach and comes in only to strike
                    if (distance < WoundedKeep) move = Mul(Norm(Add(Mul(dir, -1.f), Mul(side, .6f))), speed * .9f);
                    else move = Mul(side, speed * MonsterCircleSpeed);
                }
                else if (def.Keep > 0)
                {
                    if (distance > def.Keep + 1.5f) move = Mul(chase, speed);
                    else if (distance < def.Keep - 2.f) move = Mul(dir, -speed * .9f);       // she closes in: fall back, nearly at full speed
                    move = Add(move, Mul(XMFLOAT3{dir.z, 0, -dir.x}, m.Strafe * speed * .65f));
                    if (Random() < dt * .25f) m.Strafe = -m.Strafe;
                }
                else if (distance > def.Reach + def.Radius - .3f)
                {   // close in; over the last metres swing out to a side, so a pack comes at her from around instead of in a line
                    const float gap = distance - (def.Reach + def.Radius);
                    const float swing = def.Boss ? 0.f : MonsterFlank * Smooth(MonsterFlankDistance - gap, 0, MonsterFlankDistance) * Smooth(gap, 0, 1.5f);
                    move = Mul(Norm(Add(chase, Mul(side, swing))), speed);
                }
                else if (!def.Boss && m.Cooldown > 0) move = Mul(side, speed * MonsterCircleSpeed);   // in reach, between blows or waiting for a turn: circle her
            }
            for (int j = 0; j < count; ++j)
            {   // keep apart
                if (j == i || !mMonsters[j].Active || mMonsters[j].Dying || mMonsters[j].Kind == ChestKind) continue;
                XMFLOAT3 away = Sub(e.Pos, enemies[j].Pos); away.y = 0;
                const float d = LenXZ(away), want = def.Radius + enemies[j].Radius + .25f;
                if (d < want && def.PushScale > 0) move = Add(move, Mul(Norm(d > .01f ? away : XMFLOAT3{Random() - .5f, 0, Random() - .5f}), (want - d) * 4.f));
            }
            XMFLOAT3 next = Add(e.Pos, Mul(move, dt));
            int index = Nearest(next.x, next.z, m.Index, 30);
            if (!(next.z > LedgeWalkFrontZ && std::abs(next.x) < LedgeWalkHalfWidth))
            {   // stay on the road, and inside the arena while its fight lasts
                const Sample& s = mSamples[index];
                const float lateral = (next.x - s.P.x) * s.Nx + (next.z - s.P.z) * s.Nz;
                const float along = (next.x - s.P.x) * s.Tx + (next.z - s.P.z) * s.Tz;
                const float limit = std::max(.5f, s.W - def.Radius - .3f);
                const float keep = std::clamp(lateral, -limit, limit);
                next.x = s.P.x + s.Nx * keep + s.Tx * std::clamp(along, -mStep, mStep);
                next.z = s.P.z + s.Nz * keep + s.Tz * std::clamp(along, -mStep, mStep);
                if (arena && m.Encounter)
                {
                    const float sAt = static_cast<float>(index) * mStep;
                    const float lo = arena->S - arena->R * .9f, hi = arena->S + arena->R * .9f;
                    if (sAt > hi || sAt < lo)
                    {   // knocked or wandered out of the arena: back to its edge, or the fight could never end
                        index = std::clamp(static_cast<int>((sAt > hi ? hi : lo) / mStep), 0, static_cast<int>(mSamples.size()) - 1);
                        const Sample& rim = mSamples[index];
                        const float inside = std::clamp(keep, -std::max(.5f, rim.W - def.Radius - .3f), std::max(.5f, rim.W - def.Radius - .3f));
                        next.x = rim.P.x + rim.Nx * inside; next.z = rim.P.z + rim.Nz * inside;
                    }
                }
            }
            m.Moving = LenXZ(Sub(next, e.Pos)) / std::max(dt, 1e-4f);
            m.Index = index;
            m.Ground = GroundAt(next.x, next.z, m.Index);
            e.Pos = {next.x, m.Ground + 1.3f, next.z};
        }
    }

    // ================================================================ chests
    void Journey::UpdateChests(WorldState& world)
    {
        const XMFLOAT3 p = world.MainPlayer.Position;
        for (int i = 0; i < static_cast<int>(mChests.size()); ++i)
        {
            Chest& c = mChests[i];
            if (c.Opened) { c.Open = std::min(1.f, c.Open + .02f); continue; }
            if (c.Arena >= mArenasCleared && c.Tier > 0) continue; // an arena's chest appears when the arena is won
            const float d = LenXZ(Sub(c.P, p));
            if (c.Slot < 0 && d < ChestWakeDistance)
            {
                c.Slot = Spawn(ChestKind, c.P, false, 1);
                if (c.Slot >= 0) mMonsters[c.Slot].Chest = i;
            }
            else if (c.Slot >= 0 && d > ChestWakeDistance + 15) Release(c.Slot);
        }
    }

    void Journey::OpenChest(int index, WorldState& world, CombatSystem& combat)
    {
        (void)world; (void)combat;   // chests no longer heal or strike
        if (index < 0) return;
        Chest& c = mChests[index];
        c.Opened = true; c.Slot = -1;
        auto& kit = GetNyxaraKit();
        const XMFLOAT3 at = Add(c.P, {0, .8f, 0});
        kit.Effect(at, {1.f, .82f, .3f, 1}, c.Tier == 2 ? 90 : 36, 6.f, .28f);
        kit.RingEffect(Add(c.P, {0, .05f, 0}), 2.4f, {1.f, .85f, .35f, 1}, .7f);
        const unsigned hash = static_cast<unsigned>(index) * 2654435761u;
        int gold = 15 + static_cast<int>((hash >> 8) % 30);
        if (c.Tier == 1) gold += 50; else if (c.Tier == 2) gold += 200;
        if (Gift(4)) gold *= FleeceGoldScale;
        mGold += gold;
        mGrowth.Crystals += ChestCrystals * (c.Tier == 2 ? 8 : c.Tier == 1 ? 3 : 1);
        // every chest holds potions; one she cannot carry stays behind (chests themselves never heal)
        int potions = c.Tier == 2 ? 3 : c.Tier == 1 ? 2 : 1;
        mGrowth.Potions = std::min(MaxPotions, mGrowth.Potions + potions);
        const std::wstring coins = L"+" + std::to_wstring(gold) + L" gold, +" + std::to_wstring(potions) + (potions == 1 ? L" potion. " : L" potions. ");
        const int stage = mSamples[Nearest(c.P.x, c.P.z, -1, 0)].Stage;
        if (c.Tier == 2)
        {
            mFeathers += 3;
            Notify(HudArt::IconChest, Palette::GoldLight, L"A KING'S HOARD", coins + L"Three eagle feathers.", 6);
        }
        else if (c.Tier == 1)
        {
            ++mFeathers;
            Notify(HudArt::IconFeather, Palette::Silver, L"EAGLE FEATHER", coins + L"Your blades bite deeper.", 4.5f);
        }
        else if (mLore <= stage && mLore < 12)
        {
            ++mLore;
            { char line[32]; std::snprintf(line, sizeof line, "voice/memory_%02d", mLore); Sound::Narrate(line); }
            Notify(HudArt::IconEye, Palette::GoldLight, std::wstring(L"MEMORY OF THE CAVE  ") + Ui::Roman(mLore), coins + L"Press F1 to read it.", 6);
            mJournal.Unread |= 1u << Story::MemoriesPage;
        }
        else switch ((hash >> 16) % 3)
        {
        case 0:
            mGrowth.Crystals += MoonShardCrystals;
            Notify(HudArt::IconTear, Palette::Moon, L"MOON SHARD", coins + L"+" + std::to_wstring(MoonShardCrystals) + L" crystals.", 4.5f); break;
        case 1:
            ++mFeathers;
            Notify(HudArt::IconFeather, Palette::Silver, L"EAGLE FEATHER", coins + L"Your blades bite deeper.", 4.5f); break;
        default:
            mEmber = EmberSeconds; kit.ResetCooldowns();
            Notify(HudArt::IconEmber, Palette::Soul, L"SOUL EMBER", coins + L"Your skills return and your blades burn.", 4.5f); break;
        }
        if (c.Tier >= 1 && (c.Tier == 2 || Random() < RuneChance))
        {   // a rune for her blades: the kind she has fewest of
            int rune = static_cast<int>(Random() * static_cast<float>(Collection::RuneCount)) % static_cast<int>(Collection::RuneCount);
            for (int i = 0; i < Collection::RuneCount; ++i) if (mCollection.Runes[i] < mCollection.Runes[rune]) rune = i;
            if (mCollection.Runes[rune] < MaxRunes)
            {
                ++mCollection.Runes[rune];
                if (mCollection.Set < 0) mCollection.Set = rune;
                Notify(HudArt::IconEmber, Palette::GoldLight, Ui::Upper(Collection::RuneName(rune)),
                       std::to_wstring(mCollection.Runes[rune]) + L" of " + std::to_wstring(MaxRunes) + L". Press I, then D, to set a rune in your blades.", 6);
            }
        }
        Save();
    }

    // ================================================================ finale
    void Journey::UpdateFinale(WorldState& world, CombatSystem& combat, float dt)
    {
        auto& mountain = GetAmiraniMountain();
        const int last = static_cast<int>(mArenas.size()) - 1;
        switch (mFinale)
        {
        case Finale::None:
            if (mArenasCleared == last && mS > mArenas[last].S - mArenas[last].R * .5f && std::abs(mLateral) <= mSamples[mIndex].W && combat.PlayerActionsAllowed())
            {
                mFinale = Finale::Words; mFinaleTimer = 9.f;
                Announce(L"THE CAVE", L"FATHER AND DAUGHTER", L"The Eagle lifts his head from the wound.", 6);
                ReachEnd(1);
            }
            break;
        case Finale::Words:
            mFinaleTimer -= dt;
            if (mFinaleTimer <= 0 || mPilot)
            {
                mountain.SetEagleAway(true);
                const Arena& a = mArenas[last];
                Spawn(Eagle, {a.P.x, a.P.y, a.P.z + 8}, true, BossHealthScale * (1 + StageHealthGrowth * 11));
                mFighting = true; mFinale = Finale::Fight;
                Log("[JOURNEY] the Eagle fight begins at %.0f s played.\n", mPlaySeconds);
            }
            break;
        case Finale::Fight:
            if (AliveEncounterCount() == 0)
            {
                bool dying = false;
                for (const auto& m : mMonsters) if (m.Active && m.Kind == Eagle) dying = true;
                if (dying) break;
                mFighting = false; mArenasCleared = last + 1; mFinale = Finale::Chains;
                combat.RestorePlayerHealth(world, 100);
                Announce(L"", L"THE EAGLE FALLS", L"Climb to the ledge and break the two chain anchors.", 8);
                ReachEnd(2);
                for (const float side : {-1.f, 1.f})
                    Spawn(ChainKind, {MountainX + side * LedgeHalfWidth * .62f, LedgeHeight, LedgeWalkBackZ - 2.5f}, false, 1);
                Save();
            }
            break;
        case Finale::Chains:
        {
            int anchors = 0;
            for (const auto& m : mMonsters) if (m.Active && m.Kind == ChainKind) ++anchors;
            if (anchors == 0)
            {
                mFinale = Finale::Freed; mFinaleTimer = 26; mCompleted = true;
                mountain.SetFreed(true);
                GetNyxaraKit().Shake(.5f);
                Announce(L"", L"THE CHAIN OPENS", L"Amirani stands.", 8);
                if (mCollection.Links >= Collection::LinkCount) Announce(L"THE TRUE ENDING", L"THE THREE LINKS", L"He lays them on your father's wing. The Eagle breathes.", 9);
                Announce(L"CAUCASIAN EAGLE", L"THE ROAD IS WALKED", L"Thank you for playing. Press F1 to read the end.", 12);
                ReachEnd(3);
                if (mCollection.Links >= Collection::LinkCount) ReachEnd(4);
                Log("[JOURNEY] COMPLETE at %.0f s played: gold %d, feathers %d, lore %d, deaths %d.\n", mPlaySeconds, mGold, mFeathers, mLore, mDeaths);
                Save();
            }
            break;
        }
        case Finale::Freed:
            mFinaleTimer -= dt;
            if (mFinaleTimer <= 0) mFinale = Finale::Done;
            break;
        case Finale::Done: break;
        }
    }

    // ================================================================ unattended test pilot
    void Journey::UpdatePilot(WorldState& world, CombatSystem& combat, float realDt)
    {
        // Walks the road, fights everything and breaks every chest. It cannot die (it is a test of the
        // journey's logic and length, not of difficulty): health is refilled every frame.
        static const bool mortal = EnvFloat(MortalEnvironmentVariable, 0) > 0;
        static float deadFor = 0;
        auto& session = GameSession();
        if (!mortal) combat.RestorePlayerHealth(world, 100);
        else if (!combat.PlayerActionsAllowed())
        {   // mortal pilot: lies dead for two seconds, then asks the engine for a respawn, as the death screen's button does
            session.ScriptedMove = 0; mPilotKeys = {};
            deadFor += realDt;
            if (deadFor > 2)
            {
                deadFor = 0;
                const auto& life = combat.PlayerLife();
                const bool asked = combat.RequestRespawn({life.Life, life.Death.Event, life.Death.Destination});
                Log("[PILOT] dead at %.0f m (arenas %d, deaths %d): respawn requested %d\n", mS, mArenasCleared, mDeaths, asked ? 1 : 0);
            }
            return;
        }
        else deadFor = 0;
        mPilotClock += realDt * mTimeScale;
        const auto& enemies = GetNyxaraKit().Enemies();
        const XMFLOAT3 p = world.MainPlayer.Position;
        int target = -1; float best = 60;
        for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i)
        {
            if (!mMonsters[i].Active || mMonsters[i].Dying || !enemies[i].Alive) continue;
            float d = LenXZ(Sub(enemies[i].Pos, p)) - enemies[i].Radius;
            if (d < 60 && Kinds[mMonsters[i].Kind].Boss) d = std::min(d, 5.f + d * .1f);   // like a player, it goes for the boss
            if (d < best) { best = d; target = i; }
        }
        XMFLOAT3 goal;
        mPilotKeys = {};
        if (target >= 0)
        {
            goal = enemies[target].Pos;
            // far targets are approached along the road, not through its edge
            const int at = Nearest(goal.x, goal.z, mIndex, 60);
            if (at > mIndex + 3) goal = mSamples[mIndex + 3].P; else if (at < mIndex - 3) goal = mSamples[mIndex - 3].P;
            session.ScriptedMove = best > 1.6f ? 'W' : 0; session.ScriptedFast = best > 12;
            const int beat = static_cast<int>(mPilotClock * 5);
            if (best < 3.2f) { mPilotKeys.Lmb = beat % 2 == 0; mPilotKeys.Rmb = beat % 9 == 4; }
            if (best < 9) { mPilotKeys.K1 = beat % 23 == 1; mPilotKeys.K3 = beat % 31 == 7; }
            if (best < 4) mPilotKeys.K2 = beat % 27 == 13;
        }
        else
        {
            int ahead = std::min(static_cast<int>(mSamples.size()) - 1, mIndex + 4);
            goal = mSamples[ahead].P;
            static const float roam = EnvFloat(RoamEnvironmentVariable, 0);
            // held by water or a cliff out on the land: back to the road for a while, as a player would turn
            static XMFLOAT3 heldAt{}; static float heldFor = 0, home = 0;
            if (LenXZ(Sub(p, heldAt)) > 1.5f) { heldAt = p; heldFor = 0; } else heldFor += realDt * mTimeScale;
            if (heldFor > 2.5f) { home = 12; heldFor = 0; }
            home = std::max(0.f, home - realDt * mTimeScale);
            if (roam > 0 && home <= 0)
            {   // weave over the land beside the road, as far as it lets her
                const Sample& s = mSamples[ahead];
                const float side = std::sin(static_cast<float>(ahead) * mStep / 45.f) * roam;
                goal.x += s.Nx * side; goal.z += s.Nz * side;
            }
            if (mFinale == Finale::Chains) goal = {MountainX, LedgeHeight, LedgeWalkBackZ - 4};
            session.ScriptedMove = 'W'; session.ScriptedFast = true;
            if (mCompleted) { goal = {MountainX, LedgeHeight, MountainZ}; session.ScriptedMove = 0; }   // the road is walked: stand and look at Amirani
        }
        world.MainCamera.Yaw = std::atan2(goal.x - p.x, goal.z - p.z);
        if (mPilotHold) { session.ScriptedMove = 0; session.ScriptedFast = false; if (target < 0) world.MainCamera.Yaw = std::atan2(mSamples[mIndex].Tx, mSamples[mIndex].Tz); }
        static int frames = 0; static float span = 0;
        ++frames; span += realDt;
        mPilotLog -= realDt;
        if (mPilotLog <= 0)
        {
            mPilotLog = 20;
            Log("[PILOT] t=%.0f s played, road %.0f m, stage %d, arenas %d, fighting %d wave %d/%d, enemies %d, gold %d, feathers %d, fps %.0f\n",
                mPlaySeconds, mS, mStage, mArenasCleared, mFighting ? 1 : 0, mWave, mWaves, AliveEncounterCount(), mGold, mFeathers,
                span > 0 ? static_cast<float>(frames) / span : 0.f);
            Log("[PILOT]   at (%.1f %.1f %.1f) target %d kind %d distance %.1f goal (%.1f %.1f) grounded %d lateral %.1f land %.1f way %.1f (%.0f m) trouble %d\n", p.x, p.y, p.z, target,
                target >= 0 ? mMonsters[target].Kind : -1, best, goal.x, goal.z, world.MainPlayer.IsGrounded ? 1 : 0, mLateral, GetNature().Height(p.x, p.z), mWayShow, mWayDistance, mTroubleNear >= 0 ? mTroubles[mTroubleNear].Kind : -1);
            frames = 0; span = 0;
        }
    }

    // A title shown small at the top of the screen for a few seconds (a region, a guardian, the finale).
    void Journey::Announce(const std::wstring& label, const std::wstring& title, const std::wstring& line, float seconds)
    {
        if (mBanners.size() < 4) mBanners.push_back({label, title, line, seconds});
    }

    // A small panel at the right edge: what a chest held, or that the journal has something new.
    void Journey::Notify(const HudArt::Sprite& icon, Color tint, const std::wstring& title, const std::wstring& text, float seconds)
    {
        if (mToasts.size() >= static_cast<size_t>(MaxToasts)) mToasts.erase(mToasts.begin());
        mToasts.push_back({&icon, tint, title, text, seconds});
    }

    void Journey::ReachEnd(int part)
    {
        { char line[32]; std::snprintf(line, sizeof line, "voice/end_%d", part); Sound::Narrate(line); }
        if (part <= mJournal.EndReached) return;
        mJournal.EndReached = part; mJournal.Unread |= 1u << Story::End;
    }

    // F1 opens and closes the story journal, M the world map (one at a time). While either is open the road
    // stands still; the journal's keys turn its pages.
    void Journey::UpdateJournal(float realDt, bool inputAllowed)
    {
        auto& session = GameSession();
        const bool focused = inputAllowed && !session.Unattended;
        const int keys[JournalKeyCount] = {VK_F1, 'A', VK_LEFT, 'D', VK_RIGHT, 'W', VK_UP, 'S', VK_DOWN, VK_PRIOR, VK_NEXT, 'M', 'I', VK_RETURN, VK_SPACE};
        bool down[JournalKeyCount] = {}, pressed[JournalKeyCount] = {};
        for (int i = 0; i < JournalKeyCount; ++i)
        {
            down[i] = focused && IsDoorwaysKeyDown(keys[i]);
            pressed[i] = down[i] && !mJournalKeys[i];
            mJournalKeys[i] = down[i];
        }
        bool journal = mJournalOpen, map = mMapOpen, growth = mGrowthOpen, pause = mPauseOpen, guide = mGuideOpen;
        if (session.GuideRequested && inputAllowed)
        {   // "Guide" on the start screen: play has started only to show it
            session.GuideRequested = false;
            guide = true; mGuideFromStart = true; mGuidePage = 0;
        }
        const bool escape = session.EscapePressed;             // Esc, or Start on a controller (the engine tells us)
        session.EscapePressed = false;
        const bool settings = Settings::IsOpen();              // her settings window is over her menu and has the keys
        mUiFocused = focused;
        {   // the pointer: while one of her windows is open it is free (FreeCursor); it chooses what it moves over, a click takes it
            float mx = -1, my = -1;
            bool button = false;
            if (focused && (mPauseOpen || mGrowthOpen))
            {
                POINT point{};
                if (GetCursorPos(&point) && ScreenToClient(GetForegroundWindow(), &point)) { mx = static_cast<float>(point.x); my = static_cast<float>(point.y); }
                button = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            }
            const bool click = button && !mMouseWas, moved = std::abs(mx - mMouseX) + std::abs(my - mMouseY) > .5f;
            mMouseWas = button; mMouseX = mx; mMouseY = my;
            if (!settings && mx >= 0 && (moved || click))
            {
                const auto point = [&](int group, int& row)
                {
                    const int hit = Pointer::Hit(group, mx, my);
                    if (hit < 0) return;
                    if (hit != row) { row = hit; Sound::PlayUi("ui/move", .5f); }
                    if (click) pressed[13] = true;             // as Enter on that row
                };
                if (mPauseOpen) point(Pointer::PauseRow, mPauseRow);
                else if (mGrowthOpen)
                {
                    if (mGrowthPage == 0) point(Pointer::GrowthCard, mGrowthRow); else point(Pointer::RuneCard, mRuneRow);
                    if (click && Pointer::Hit(Pointer::PageKey, mx, my) >= 0) pressed[3] = true;   // the other page, as D
                }
            }
        }
        if (settings)
        {
            if (escape) Settings::Back();
            for (bool& p : pressed) p = false;
        }
        else if (escape)
        {   // Esc closes whatever is open; with nothing open it opens her menu
            if (journal || map || growth || pause || guide) journal = map = growth = pause = guide = false;
            else { pause = true; mPauseRow = 0; }
        }
        else if (mPauseOpen)
        {   // her menu: W / S choose, Enter or Space takes the row
            const int move = (pressed[7] || pressed[8] ? 1 : 0) - (pressed[5] || pressed[6] ? 1 : 0);
            mPauseRow = (mPauseRow + move + PauseRows) % PauseRows;
            if (move != 0) Sound::PlayUi("ui/move", .5f);
            if (pressed[13] || pressed[14])
            {
                using Request = DoorwaysGameSession::Request;
                switch (mPauseRow)
                {
                case 0: pause = false; break;
                case 1: pause = false; growth = true; break;
                case 2: pause = false; journal = true; break;
                case 3: pause = false; map = true; break;
                case 4: Settings::Open(); break;                   // her own settings window; her menu is still open when it closes
                case 5: pause = false; guide = true; mGuideFromStart = false; mGuidePage = 0; break;
                case 6: pause = false; Save(); session.MenuRequest = Request::MainMenu; break;
                default: Save(); session.MenuRequest = Request::Exit; break;
                }
                pressed[13] = pressed[14] = false;                 // the same press must not also buy or set something
            }
            pressed[0] = pressed[11] = pressed[12] = false;        // F1, M and I wait while her menu is open
        }
        else if (mGuideOpen)
        {   // the guide: A / D turn the page
            const int turn = (pressed[3] || pressed[4] ? 1 : 0) - (pressed[1] || pressed[2] ? 1 : 0);
            const int page = std::clamp(mGuidePage + turn, 0, Guide::PageCount - 1);
            if (page != mGuidePage) { mGuidePage = page; Sound::PlayUi("ui/page", .6f); }
            pressed[0] = pressed[11] = pressed[12] = false;
        }
        if (pressed[0]) { journal = !journal; map = growth = false; }
        if (pressed[11]) { map = !map; journal = growth = false; }
        if (pressed[12]) { growth = !growth; journal = map = false; }
        // a menu or another window took over: everything closes
        if (!inputAllowed) journal = map = growth = guide = pause = false;
        if (journal || map || growth) guide = false;
        if (mJournalForced > 0) journal = mTime > 1.5f;        // capture modes
        if (mMapForced) map = mTime > 1.5f;
        if (mGrowthForced) growth = mTime > 1.5f;
        if (mPauseForced || (settings && session.Unattended)) pause = mTime > 1.5f;   // (the settings window held open for a capture stands over her menu)
        if (!pause && settings && !session.Unattended) Settings::Close();
        if (mGuideForced > 0) { guide = mTime > 1.5f; mGuidePage = std::clamp(mGuideForced - 1, 0, Guide::PageCount - 1); }
        const bool was = mJournalOpen || mMapOpen || mGrowthOpen || mPauseOpen || mGuideOpen, now = journal || map || growth || pause || guide;
        if (journal != mJournalOpen || map != mMapOpen || growth != mGrowthOpen || pause != mPauseOpen || guide != mGuideOpen) Sound::PlayUi(now ? "ui/open" : "ui/close", .7f);
        if (mGuideOpen && !guide && mGuideFromStart)
        {   // it came from the start screen and goes back there
            mGuideFromStart = false;
            session.MenuRequest = DoorwaysGameSession::Request::MainMenu;
        }
        if (mJournalOpen && !journal) Sound::StopNarration();
        if (now != was)
        {
            if (now) GetNyxaraKit().SetInputBlocked(true); else mKitRelease = true;
            if (now)
            {   // she cannot walk, jump or open the inventory behind the journal or the map
                mSuppressedInPlay = session.SuppressedActions;
                for (const auto action : {DoorwaysInputAction::MoveForward, DoorwaysInputAction::MoveBackward, DoorwaysInputAction::MoveLeft,
                         DoorwaysInputAction::MoveRight, DoorwaysInputAction::JumpOrAscend, DoorwaysInputAction::DescendFlight,
                         DoorwaysInputAction::Dodge, DoorwaysInputAction::ToggleInventory})
                    session.SuppressedActions.push_back(action);
            }
            else session.SuppressedActions = mSuppressedInPlay;
        }
        session.FreeCursor = now;                              // the pointer shows and the mouse leaves the camera alone
        if (now) mKitRelease = false;
        else if (mKitRelease && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
        {   // the click that closed a window must not also be a sword blow
            mKitRelease = false; GetNyxaraKit().SetInputBlocked(false);
        }
        if (journal && !mJournalOpen)
        {
            mJournal.Scroll = 0;
            if (mJournalForced > 0) mJournal.Page = std::clamp(mJournalForced - 1, 0, Story::PageCount - 1);
            else for (int page = Story::PageCount - 1; page >= 0; --page)
                if (mJournal.Unread & (1u << page)) mJournal.Page = page;   // open on what is new
        }
        mJournalOpen = journal; mMapOpen = map; mGrowthOpen = growth; mPauseOpen = pause; mGuideOpen = guide;
        mGuideFade = std::clamp(mGuideFade + (mGuideOpen ? 1.f : -1.f) * realDt / JournalFadeSeconds, 0.f, 1.f);
        mPauseFade = std::clamp(mPauseFade + (mPauseOpen ? 1.f : -1.f) * realDt / JournalFadeSeconds, 0.f, 1.f);
        mGrowthFade = std::clamp(mGrowthFade + (mGrowthOpen ? 1.f : -1.f) * realDt / JournalFadeSeconds, 0.f, 1.f);
        mGrowthFlash = std::max(0.f, mGrowthFlash - realDt * 2.5f);
        if (mGrowthOpen && (pressed[1] || pressed[2] || pressed[3] || pressed[4]))
        {   // A / D: the other page of her window
            mGrowthPage = 1 - mGrowthPage; Sound::PlayUi("ui/page", .6f);
        }
        if (mGrowthOpen && mGrowthPage == 1)
        {   // what she carries: W / S choose a rune, Enter or Space sets it in her blades (or takes it out)
            const int move = (pressed[7] || pressed[8] ? 1 : 0) - (pressed[5] || pressed[6] ? 1 : 0);
            mRuneRow = (mRuneRow + move + Collection::RuneCount) % Collection::RuneCount;
            if (move != 0) Sound::PlayUi("ui/move", .5f);
            if (pressed[13] || pressed[14])
            {
                if (mCollection.Runes[mRuneRow] > 0)
                {
                    mCollection.Set = mCollection.Set == mRuneRow ? -1 : mRuneRow;
                    mGrowthFlash = 1; Sound::PlayUi("ui/buy", .9f); Save();
                }
                else Sound::PlayUi("ui/deny", .7f);
            }
        }
        if (mGrowthOpen && mGrowthPage == 0)
        {   // W / S choose, Enter or Space spends crystals on the chosen row
            const int move = (pressed[7] || pressed[8] ? 1 : 0) - (pressed[5] || pressed[6] ? 1 : 0);
            mGrowthRow = (mGrowthRow + move + Growth::RowCount) % Growth::RowCount;
            if (move != 0) Sound::PlayUi("ui/move", .5f);
            if ((pressed[13] || pressed[14]) && !(Growth::Cost(mGrowth, mGrowthRow) > 0 && mGrowth.Crystals >= Growth::Cost(mGrowth, mGrowthRow))) Sound::PlayUi("ui/deny", .7f);
            const int cost = Growth::Cost(mGrowth, mGrowthRow);
            if ((pressed[13] || pressed[14]) && cost > 0 && mGrowth.Crystals >= cost)
            {
                mGrowth.Crystals -= cost; ++mGrowth.Level[mGrowthRow];
                mGrowthFlash = 1; mGrowthFlashRow = mGrowthRow; Sound::PlayUi("ui/buy", .9f);
                Log("[JOURNEY] growth: row %d to %d for %d crystals (%d left).\n", mGrowthRow, mGrowth.Level[mGrowthRow], cost, mGrowth.Crystals);
                Save();
            }
        }
        mJournal.Open = std::clamp(mJournal.Open + (mJournalOpen ? 1.f : -1.f) * realDt / JournalFadeSeconds, 0.f, 1.f);
        mMapFade = std::clamp(mMapFade + (mMapOpen ? 1.f : -1.f) * realDt / JournalFadeSeconds, 0.f, 1.f);
        mJournal.CurrentChapter = mStage;
        mJournal.MemoriesFound = mLore;
        if (!mJournalOpen) return;
        const int turn = (pressed[3] || pressed[4] ? 1 : 0) - (pressed[1] || pressed[2] ? 1 : 0);
        if (turn != 0) { mJournal.Page = (mJournal.Page + turn + Story::PageCount) % Story::PageCount; mJournal.Scroll = 0; Sound::PlayUi("ui/page", .6f); Sound::StopNarration(); }
        // R: the storyteller reads the legend aloud (the other pages are told on the road, as they happen)
        if (mJournal.Page == Story::Legend && focused && IsDoorwaysKeyDown('R') && !Sound::Narrating()) Sound::Narrate("voice/legend");
        const float scroll = (down[7] || down[8] ? 1.f : 0.f) - (down[5] || down[6] ? 1.f : 0.f) + (pressed[10] ? 8.f : 0.f) - (pressed[9] ? 8.f : 0.f);
        mJournal.Scroll = std::clamp(mJournal.Scroll + scroll * JournalScrollSpeed * realDt, 0.f, mJournal.Overflow);
        mJournal.Unread &= ~(1u << mJournal.Page);
    }

    // Which city she is in. Entering one shows its name; the first time, it also heals her and gives its gift
    // (Story::Cities; the gifts take effect through Gift() where the numbers are used).
    void Journey::UpdateCities(WorldState& world, CombatSystem& combat)
    {
        int city = -1;
        if (!(mS <= 1 && mArenasCleared == 0))
            for (int i = 0; i < static_cast<int>(mCities.size()); ++i)
                if (mS >= mCities[i].S0 && mS <= mCities[i].S1 && std::abs(mLateral) <= mSamples[mIndex].W + CityReach) city = i;   // in its streets, not on the hill above it
        if (city == mCity) return;
        mCity = city;
        if (city < 0) return;
        const Story::City& words = Story::Cities[city];
        Announce(std::wstring(L"CITY ") + Ui::Roman(city + 1) + L" OF VII  -  " + Ui::Upper(words.Land), Ui::Upper(words.Name), words.Known, 5.5f);
        Sound::Play("world/bell", .8f, 0);
        if (mCities[city].Visited) return;
        mCities[city].Visited = true;
        { char line[32]; std::snprintf(line, sizeof line, "voice/city_%d", city + 1); Sound::Narrate(line); }
        Sound::Play("world/gift", .8f, 0);
        if (combat.PlayerActionsAllowed()) combat.RestorePlayerHealth(world, 100);
        if (city == 5) mFeathers += SeaWindFeathers;
        Notify(WorldMap::Emblem(city), Palette::GoldLight, Ui::Upper(words.Gift), std::wstring(words.GiftText) + L" Press M for the map.", 9);
        Notify(HudArt::IconBook, Palette::GoldLight, Ui::Upper(Collection::KeepsakeName(city)),
               L"A keepsake, and verse " + std::to_wstring(CitiesVisited()) + L" of 7 of the road song." + (CitiesVisited() >= 7 ? L" The song is whole: creatures leave more crystals." : L""), 8);
        GetNyxaraKit().RingEffect(Add(world.MainPlayer.Position, {0, .05f, 0}), 5.f, {1.f, .85f, .45f, 1}, 1.2f);
        Log("[JOURNEY] city %d reached at %.0f m: %ls.\n", city + 1, mS, words.Gift);
        Save();
    }

    // What the minimap and the world map show this frame.
    void Journey::FillMapView(const WorldState& world)
    {
        WorldMap::View& v = mMapView;
        v.Road = &mRoadPoints; v.Step = mStep; v.Walked = mS;
        v.Player = {world.MainPlayer.Position.x, world.MainPlayer.Position.z};
        v.Heading = world.MainPlayer.Yaw; v.Look = world.MainCamera.Yaw;
        v.Enemies.clear(); v.Chests.clear();
        const auto& enemies = GetNyxaraKit().Enemies();
        for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i)
            if (mMonsters[i].Active && !mMonsters[i].Dying && mMonsters[i].Kind != ChestKind) v.Enemies.push_back({enemies[i].Pos.x, enemies[i].Pos.z});
        for (const auto& c : mChests)
            if (!c.Opened && !(c.Arena >= mArenasCleared && c.Tier > 0) && std::abs(c.P.x - v.Player.X) + std::abs(c.P.z - v.Player.Z) < MinimapRange * 2)
                v.Chests.push_back({c.P.x, c.P.z});
        if (v.Arenas.empty()) for (const auto& a : mArenas) v.Arenas.push_back({a.P.x, a.P.z});
        v.ArenasWon = mArenasCleared; v.Fighting = mFighting;
        v.City = mCity; v.NextCity = -1; v.NextCityDistance = 0;
        for (int i = 0; i < static_cast<int>(mCities.size()); ++i)
        {
            v.Cities[i] = {mCities[i].P.x, mCities[i].P.z}; v.Visited[i] = mCities[i].Visited;
            if (v.NextCity < 0 && mCities[i].S0 > mS) { v.NextCity = i; v.NextCityDistance = mCities[i].S0 - mS; }
        }
        v.MountainDistance = std::max(0.f, static_cast<float>(mSamples.size() - 1) * mStep - mS);
    }

    // Begins the journey again from the start of the road: every arena, chest, memory and the finale are as on the first day.
    void Journey::Restart(WorldState& world, CombatSystem& combat)
    {
        for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i) Release(i);
        for (auto& c : mChests) { c.Opened = false; c.Open = 0; c.Slot = -1; }
        mArenasCleared = mGold = mFeathers = mLore = mDeaths = 0; mPlaySeconds = 0; mEmber = 0; mGrowth = {}; mCollection = {}; mFormShown = 0;
        mFighting = false; mCompleted = false; mWave = mWaves = 0; mWaveDelay = 0; mQueue.clear();
        mFinale = Finale::None; mShownStage = -1; mPatrolNext = 0; mPendingTeleport = false; mWasDead = false;
        GetAmiraniMountain().SetFreed(false); GetAmiraniMountain().SetEagleAway(false);
        Teleport(world, StartS); mStage = 0;
        for (auto& c : mCities) c.Visited = false;
        mCity = -1; mWall = 0; mWallArena = -1;
        combat.RestorePlayerHealth(world, 100);
        GetNyxaraKit().ResetCooldowns();
        mBanners.clear(); mBannerTime = 0; mToasts.clear();
        mJournal.ChaptersReached = mJournal.EndReached = 0; mJournal.Unread = 0;
        Announce(L"", L"THE ROAD BEGINS AGAIN", L"Seven years lie ahead of you once more.", 5);
        Log("[JOURNEY] restarted: back at the start of the road, nothing won.\n");
        Save();
    }

    // ================================================================ update
    void Journey::Update(DoorwaysGameHost& host, float dt, float realDt, bool inputAllowed)
    {
        if (!mReady) return;
        auto& world = host.World; auto& combat = host.Combat;
        auto& kit = GetNyxaraKit();
        mTime += realDt;
        mPlaySeconds += dt;
        if (static bool placed = false; !placed && mPilot)
        {   // testing: stand her before a trouble (or beside it, off the road) for a capture
            placed = true;
            const int which = static_cast<int>(EnvFloat(TroubleEnvironmentVariable, 0));
            if (which != 0 && std::abs(which) <= static_cast<int>(mTroubles.size()))
            {
                const Trouble& t = mTroubles[std::abs(which) - 1];
                Teleport(world, t.S - (t.Kind == 1 ? 6.f : 13.f));
                if (which < 0)
                {
                    auto& p = world.MainPlayer;
                    p.Position.x += mSamples[mIndex].Nx * 30; p.Position.z += mSamples[mIndex].Nz * 30;
                    p.Position.y = GetNature().Height(p.Position.x, p.Position.z) + .3f; p.GroundY = p.Position.y;
                }
                mPilotHold = true;
                Log("[JOURNEY] test: at trouble %d, kind %d, %.0f m.\n", std::abs(which), t.Kind, t.S);
            }
        }
        if (!mPlaced)
        {   // a new journey begins on the road, before its first gate (the engine's spawn point is the same place)
            mPlaced = true;
            if (mArenasCleared == 0 && !mPendingTeleport) Teleport(world, StartS);
        }
        if (mPendingTeleport)
        {
            mPendingTeleport = false;
            Teleport(world, mArenas[std::min(mArenasCleared, static_cast<int>(mArenas.size())) - 1].S);
        }
        // death: the fight is undone, she wakes at the last arena she won
        const bool alive = combat.PlayerActionsAllowed();
        const auto undoFight = [&]
        {
            for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i)
                if (mMonsters[i].Active && mMonsters[i].Kind < ChestKind) Release(i);
            mFighting = false; mQueue.clear();
            if (mFinale == Finale::Fight || mFinale == Finale::Words) { mFinale = Finale::None; GetAmiraniMountain().SetEagleAway(false); }
        };
        const auto toCheckpoint = [&]
        { Teleport(world, mArenasCleared > 0 ? mArenas[std::min(mArenasCleared, static_cast<int>(mArenas.size())) - 1].S : StartS); };
        if (!alive && !mWasDead)
        {
            mWasDead = true; ++mDeaths; undoFight();
            Sound::Play("nyxara/death", 1, 0); Sound::StopNarration();
            Log("[JOURNEY] death %d at %.0f m, %.0f s played (arenas won %d).\n", mDeaths, mS, mPlaySeconds, mArenasCleared);
        }
        else if (alive && mWasDead)
        {
            mWasDead = false; toCheckpoint();
            Log("[JOURNEY] respawned: back at %.0f m.\n", mS);
        }

        const float before = mS;
        if (!mShowcase) Leash(world);
        UpdateWay(world, realDt);
        if (!mShowcase && alive && mS < 20 && before > 150 && mArenasCleared > 0)
        {   // she was far down the road and is suddenly back at its start: the engine respawned her there.
            // (Second net for the death handling above, in case the game is not updated while the death screen shows.)
            // The engine can hold her there for several frames: that is one death, counted and reported once.
            const bool first = mTime - mRespawnSeen > 3.f;
            mRespawnSeen = mTime;
            if (!mWasDead && first) ++mDeaths;
            mWasDead = false; undoFight(); toCheckpoint();
            if (first) Log("[JOURNEY] respawn seen at the start of the road: back at %.0f m.\n", mS);
        }
        if (mStage != mShownStage && !mShowcase && mS > 1)
        {
            mShownStage = mStage;
            Announce(std::wstring(L"CHAPTER ") + Ui::Roman(mStage + 1), Ui::Upper(Story::Chapters[mStage].Title), L"", 5);
            { char line[32]; std::snprintf(line, sizeof line, "voice/chapter_%02d", mStage + 1); Sound::Narrate(line); }
            if (mStage + 1 > mJournal.ChaptersReached) { mJournal.ChaptersReached = mStage + 1; mJournal.Unread |= 1u << Story::Road; }
        }
        for (int i = 0; i < 12; ++i)
        {
            const float want = i == mStage ? 1.f : 0.f;
            mEnv[i] += (want - mEnv[i]) * std::min(1.f, realDt * .6f);
        }

        if (!mShowcase)
        {
            UpdatePatrols(world, combat);
            UpdateEncounter(world, combat, dt);
            UpdateFinale(world, combat, dt);
            UpdateChests(world);
            UpdateCities(world, combat);
            UpdateTroubles(world, combat, dt);
        }
        UpdateMonsters(world, combat, dt);

        // feathers and embers sharpen her blades (health never comes back by itself: only creatures, chests and won fights give it)
        mEmber = std::max(0.f, mEmber - dt);
        kit.SetDamageScale((1 + FeatherDamageBonus * static_cast<float>(mFeathers)) * (mEmber > 0 ? EmberDamage : 1.f) * (Gift(6) ? SvanSteelDamage : 1.f) * Growth::Damage(mGrowth) * Collection::Damage(mCollection, CitiesVisited()));
        kit.SetCooldownScale(Growth::Cooldown(mGrowth));
        GameSession().FastMultiplier = WingBoostMultiplier * Growth::Boost(mGrowth) * Collection::Boost(mCollection);
        if (!mShowcase && alive) UpdateFatherFeathers(world);
        const int form = Growth::Form(mGrowth);
        {
            const float tall = 1 + FormScale * static_cast<float>(form);
            world.AnimatedPlayerVisual.Scale = {tall, tall, tall};
        }
        if (alive)
        {   // her health comes back by itself, slowly
            mRegen += dt;
            while (mRegen >= RegenSeconds) { mRegen -= RegenSeconds; combat.RestorePlayerHealth(world, RegenHealth); }
        }
        else mRegen = 0;
        if (form > mFormShown && alive)
        {   // all three of her things reached a new tier: she herself changes
            mFormShown = form;
            combat.RestorePlayerHealth(world, 100);
            const XMFLOAT4 light = form == 1 ? XMFLOAT4{.55f, .82f, 1.f, 1} : form == 2 ? XMFLOAT4{.35f, 1.f, .62f, 1} : XMFLOAT4{1.f, .86f, .45f, 1};
            kit.Effect(Add(world.MainPlayer.Position, {0, 1.3f, 0}), light, 120, 9.f, .34f);
            kit.RingEffect(Add(world.MainPlayer.Position, {0, .05f, 0}), 7.f, light, 1.6f);
            kit.Shake(.3f); Sound::Play("nyxara/evolve", 1, 0); Sound::Play("nyxara/say_evolve", 1, 0);
            Announce(L"NYXARA EVOLVES", Ui::Upper(Growth::FormName(form)), L"Wings, blades and armour answer her as one. Her wounds close.", 6);
            Log("[JOURNEY] form %d reached at %.0f s played.\n", form, mPlaySeconds);
        }
        else if (form < mFormShown) mFormShown = form;
        {   // a potion
            const bool key = inputAllowed && !GameSession().Unattended && !mJournalOpen && !mMapOpen && !mGrowthOpen && !mPauseOpen && !mGuideOpen && IsDoorwaysKeyDown(PotionKey);
            if (key && !mPotionKey && alive)
            {
                if (mGrowth.Potions <= 0) kit.Notice(L"No potions. Old gold chests hold them.", 2.2f);
                else if (combat.RestorePlayerHealth(world, PotionHealth()) > 0)
                {
                    --mGrowth.Potions; Sound::Play("nyxara/potion", .9f, 0);
                    kit.Effect(Add(world.MainPlayer.Position, {0, 1.2f, 0}), {1.f, .35f, .4f, 1}, 26, 3.5f, .24f);
                    Save();
                }
                else kit.Notice(L"She is not wounded.", 1.8f);
            }
            mPotionKey = key;
        }

        // arena walls: up while the fight lasts, then back into the ground
        if (mFighting && mArenasCleared < static_cast<int>(mArenas.size()) && mArenas[mArenasCleared].Kind != 2)
        {
            if (mWallArena != mArenasCleared || mWall <= 0) { mWallArena = mArenasCleared; kit.Shake(.35f); Sound::Play("combat/walls", 1, 0); }
            mWall = std::min(1.f, mWall + dt / WallRiseSeconds);
        }
        else mWall = std::max(0.f, mWall - dt / WallSinkSeconds);

        {   // what is heard all the time, and her feet and wings
            const bool boss = mFinale == Finale::Fight || (mFighting && mArenasCleared < static_cast<int>(mArenas.size()) && mArenas[mArenasCleared].Kind == 1);
            Sound::Scene scene;
            scene.Music = boss ? "music/boss" : mFighting ? "music/battle" : mCity >= 0 ? "music/city" : "music/road";
            scene.Wind = mCity >= 0 ? .3f : 1.f;
            scene.City = mCity >= 0 ? 1.f : 0.f;
            scene.Fire = mWall;
            const bool air = alive && kit.Airborne();
            scene.Flight = air ? 1.f : 0.f;
            if (air && !mSoundAir) Sound::Play("nyxara/wing", .8f);
            const float moved = LenXZ(Sub(world.MainPlayer.Position, mSoundAt));
            Sound::Walk(alive && !air && moved < 2.f ? moved : 0.f);
            if (!air && mSoundAir && alive) Sound::Play("nyxara/land", .6f);
            mSoundAir = air; mSoundAt = world.MainPlayer.Position;
            Sound::Update(realDt, scene);
        }
        UpdateJournal(realDt, inputAllowed);
        if (!mBanners.empty())
        {
            mBannerTime += realDt;
            if (mBannerTime >= mBanners.front().Seconds) { mBanners.erase(mBanners.begin()); mBannerTime = 0; }
        }
        for (auto& toast : mToasts) toast.Age += realDt;
        std::erase_if(mToasts, [](const Toast& toast) { return toast.Age >= toast.Seconds; });
        // hold F9: begin the journey again (shown on screen while held; not while she is dead)
        const bool restartKey = GameSession().Unattended
            ? mRestartTest > 0 && mTime > mRestartTest && mTime < mRestartTest + RestartHoldSeconds + 1
            : inputAllowed && (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (restartKey && alive && !mJournalOpen && !mMapOpen && !mPauseOpen && !mGuideOpen)
        {
            mRestartHold += realDt;
            if (mRestartHold >= RestartHoldSeconds) { mRestartHold = 0; mRestartTest = 0; Restart(world, combat); }
        }
        else mRestartHold = 0;

        if (mPilot) UpdatePilot(world, combat, realDt);
    }

    // ================================================================ atmosphere
    void Journey::StyleEnvironment(EnvironmentSettings& e) const
    {
        if (!mReady) return;
        XMFLOAT3 fog{}, ambient{}, light{}; float end = 0, strength = 0, total = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float w = mEnv[i]; total += w;
            fog = Add(fog, Mul(Air[i].Fog, w)); ambient = Add(ambient, Mul(Air[i].Ambient, w)); light = Add(light, Mul(Air[i].Light, w));
            end += Air[i].FogEnd * w; strength += Air[i].FogStrength * w;
        }
        if (total < 1e-3f) return;
        const float inv = 1 / total;
        e.Fog.Color = {fog.x * inv, fog.y * inv, fog.z * inv, 1};
        e.Fog.EndDistance = end * inv;
        e.Fog.Strength = strength * inv;
        e.Lighting.AmbientLight.Color = {ambient.x * inv, ambient.y * inv, ambient.z * inv, 1};
        e.Lighting.DirectionalLight.Color = {light.x * inv, light.y * inv, light.z * inv, 1};
    }

    // ================================================================ draw
    void Journey::Draw(Renderer& renderer, const WorldState& world, const XMMATRIX& viewProjection)
    {
        if (!mReady) return;
        renderer.ResetDefaultWorldRenderState();
        renderer.BindDoorwaysSkinningDisabled();
        const XMFLOAT3 eye = world.MainCamera.Position;
        auto& kit = GetNyxaraKit();
        const auto& enemies = kit.Enemies();
        ID3D11ShaderResourceView* white = renderer.mWhiteTextureView.Get();

        // ---- scenery props (Meshy models placed by the world builder)
        for (const auto& prop : mProps)
        {
            const float limit = prop.Type == 2 ? TowerDrawDistance : PropDrawDistance;
            const float dx = prop.P.x - eye.x, dz = prop.P.z - eye.z, d = std::sqrt(dx * dx + dz * dz);
            if (d > limit) continue;
            const Model& model = mPropModels[prop.Type];
            const float grow = Smooth(limit - d, 0, 30);
            renderer.DrawDoorwaysGpuMesh(model.Mesh, Place(model.Min, model.Max, prop.Height * grow, prop.Yaw, prop.P), viewProjection,
                                         Surface(model.Texture.Get(), {PropTint, PropTint, PropTint, 1}, .03f));
        }

        // ---- the cities (models and placement: art/cities/build_cities.py)
        for (const auto& b : mBuildings)
        {
            const float dx = b.P.x - eye.x, dz = b.P.z - eye.z, d = std::sqrt(dx * dx + dz * dz);
            if (d > CityDrawDistance) continue;
            const Model& model = mCityModels[b.Model];
            const float fade = Smooth(CityDrawDistance - d, 0, 60);                 // far buildings come out of the dark
            renderer.DrawDoorwaysGpuMesh(model.Mesh, Place(model.Min, model.Max, b.Height, b.Yaw, b.P), viewProjection,
                                         Surface(model.Texture.Get(), {CityTint * fade, CityTint * fade, CityTint * fade, 1}, CityEmission * fade));
        }

        // ---- arena walls: rough stone blocks across the road at both ends, each rising a moment after its neighbour
        if (mWall > 0 && mWallArena >= 0 && mWallArena < static_cast<int>(mArenas.size()))
        {
            const Arena& a = mArenas[mWallArena];
            const Model& rock = mPropModels[3];
            for (const float end : {1.f, -1.f})
            {
                const Sample& s = mSamples[std::clamp(static_cast<int>((CageEnd(a, end) + end * WallStandOff) / mStep), 0, static_cast<int>(mSamples.size()) - 1)];
                if (LenXZ(Sub(s.P, eye)) > 220) continue;
                // the line of sight from the camera to her, stopping a little short of her
                const XMFLOAT3 her = world.MainPlayer.Position;
                XMFLOAT3 sight = Sub(her, eye); sight.y = 0;
                const float sightLength = std::max(.01f, LenXZ(sight) - 1.2f);
                sight = Norm(sight);
                const float yaw = std::atan2(s.Tx, s.Tz), half = s.W + 1.5f;
                const int blocks = std::max(2, static_cast<int>(2 * half / WallBlockWidth) + 1);
                for (int i = 0; i < blocks; ++i)
                {
                    const float l = -half + 2 * half * (static_cast<float>(i) + .5f) / static_cast<float>(blocks);
                    const float jitter = std::sin(static_cast<float>(i) * 12.9898f + end * 4.1f) * .5f + .5f;        // 0..1, fixed per block
                    const float delay = std::abs(l) / half * .45f;                                                 // the middle comes up first
                    float rise = Smooth(mWall * 1.45f - delay, 0, 1);
                    {   // a block that stands between the camera and her sinks, so the wall never hides her
                        const float bx = s.P.x + s.Nx * l - eye.x, bz = s.P.z + s.Nz * l - eye.z;
                        const float along = std::clamp(bx * sight.x + bz * sight.z, 0.f, sightLength);
                        const float off = std::sqrt((bx - sight.x * along) * (bx - sight.x * along) + (bz - sight.z * along) * (bz - sight.z * along));
                        // ...and so does a block the camera stands next to, so it never fills the screen
                        const float beside = Smooth(std::sqrt(bx * bx + bz * bz), WallCameraNear, WallCameraFar);
                        rise *= .06f + .94f * std::min(Smooth(off, WallSightNear, WallSightFar), beside);
                    }
                    if (rise <= 0) continue;
                    const float height = WallHeight * (.78f + .22f * jitter), width = 2 * half / static_cast<float>(blocks) * 1.06f;
                    const float y = s.P.y - height + height * rise + .15f * std::sin(mTime * 40 + static_cast<float>(i)) * (rise < 1 ? 1.f - rise : 0.f);
                    const float shade = .34f + .16f * jitter;
                    renderer.DrawBox(XMMatrixScaling(width, height, WallBlockDepth * (.85f + .3f * jitter)) * XMMatrixRotationY(yaw + (jitter - .5f) * .12f) *
                                     XMMatrixTranslation(s.P.x + s.Nx * l, y + height * .5f, s.P.z + s.Nz * l), viewProjection,
                                     Surface(rock.Texture.Get(), {shade, shade, shade * 1.08f, 1}, .03f));
                }
            }
        }

        // ---- chests
        const Model& chestModel = mModels[ChestKind];
        for (const auto& c : mChests)
        {
            if (c.Arena >= mArenasCleared && c.Tier > 0) continue;
            if (LenXZ(Sub(c.P, eye)) > 160) continue;
            const float size = c.Tier == 2 ? 1.7f : c.Tier == 1 ? 1.25f : 1.0f;
            if (!c.Opened)
            {
                const bool flash = c.Slot >= 0 && enemies[c.Slot].Flash > 0;
                const float pulse = .28f + .10f * std::sin(mTime * 3 + c.P.x);
                renderer.DrawDoorwaysGpuMesh(chestModel.Mesh, Place(chestModel.Min, chestModel.Max, size, c.Yaw, c.P), viewProjection,
                                             Surface(chestModel.Texture.Get(), flash ? XMFLOAT4{3, 3, 3, 1} : XMFLOAT4{1.3f, 1.2f, 1.f, 1}, flash ? 1.f : pulse));
            }
            else // broken open: it lies tipped over, dark
                renderer.DrawDoorwaysGpuMesh(chestModel.Mesh, Place(chestModel.Min, chestModel.Max, size * .9f, c.Yaw, Add(c.P, {0, -.12f, 0}), .0f, 1.1f * c.Open),
                                             viewProjection, Surface(chestModel.Texture.Get(), {.35f, .33f, .30f, 1}, .02f));
        }

        // ---- monsters
        for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i)
        {
            const Monster& m = mMonsters[i];
            if (!m.Active || m.Kind == ChestKind) continue;
            const auto& e = enemies[i];
            const KindDef& def = Kinds[m.Kind];
            const float fade = m.Dying ? std::max(0.f, m.Death / .55f) : Smooth(m.Spawn, 0, 1);
            const XMFLOAT3 feet{e.Pos.x, e.Pos.y - 1.3f + e.Lift, e.Pos.z};
            const bool flash = e.Flash > 0 && !m.Dying;
            if (m.Kind == ChainKind)
            {   // an iron anchor block wound with chain
                const Material iron = Surface(white, flash ? XMFLOAT4{.42f, .46f, .52f, 1} : XMFLOAT4{.10f, .11f, .13f, 1}, flash ? .35f : .08f);
                renderer.DrawBox(XMMatrixScaling(3.2f * fade, 3.0f * fade, 3.2f * fade) * XMMatrixRotationY(.4f) * XMMatrixTranslation(feet.x, feet.y + 1.5f, feet.z), viewProjection, iron);
                renderer.DrawBox(XMMatrixScaling(4.0f * fade, .6f, 4.0f * fade) * XMMatrixTranslation(feet.x, feet.y + .3f, feet.z), viewProjection, iron);
                continue;
            }
            const Model& model = mModels[m.Kind];
            const float walk = std::min(1.f, m.Moving / std::max(def.Speed, .1f));
            const float stride = def.Height > 4 ? 3.2f : 7.5f;
            float pitch = .10f * walk, roll = .05f * walk * std::sin(m.Phase * stride);
            float lift = std::abs(std::sin(m.Phase * stride)) * .07f * def.Height * walk * (def.Hover > 0 ? 0.f : 1.f);
            float forward = 0;
            if (m.Windup >= 0) { const float t = 1 - m.Windup / std::max(m.WindupFull, .01f); pitch = -.32f * std::sin(t * Pi * .5f); lift += .04f * def.Height * t; }
            if (m.Lunge > 0) { const float t = m.Lunge / .28f; pitch = .45f * t; forward = .5f * def.Radius * t + .3f * t; }
            const bool rigged = model.Rig.Kind != CreatureRig::Type::None;
            if (rigged) { pitch *= .25f; roll *= .4f; }   // the joints do the leaning; the whole body only follows a little
            if (m.Hover < 0) pitch += .25f;
            const float hover = std::max(0.f, def.Hover) * (1 + .25f * std::sin(m.Phase * 2.2f)) + (m.Stage >= 100 ? 6.f * std::min(1.f, m.Special) : 0.f);
            const XMFLOAT3 at{feet.x + std::sin(e.Yaw) * forward, feet.y + lift + hover - (m.Dying ? (1 - fade) * def.Height * .3f : 0.f), feet.z + std::cos(e.Yaw) * forward};
            const float height = def.Height * (m.Dying ? .35f + .65f * fade : .25f + .75f * fade);
            const XMFLOAT4 tint = flash ? XMFLOAT4{3, 3, 3, 1} : XMFLOAT4{MonsterTint, MonsterTint, MonsterTint, 1};
            const XMMATRIX place = Place(model.Min, model.Max, height, e.Yaw, at, pitch + (m.Dying ? (1 - fade) * -1.2f : 0.f), roll);
            const Material skin = Surface(model.Texture.Get(), tint, flash ? 1.f : MonsterEmission);
            if (rigged)
            {   // limbs, spine and head move on their own joints
                CreaturePose pose;
                pose.Walk = m.Dying ? 0.f : walk; pose.Stride = m.Phase * stride; pose.Time = m.Phase;
                pose.Windup = m.Windup >= 0 && !m.Dying ? 1 - m.Windup / std::max(m.WindupFull, .01f) : m.Slam > 0 && !m.Dying ? 1 - m.Slam / SlamSeconds : 0.f;
                pose.Strike = m.Dying ? 0.f : std::min(1.f, m.Lunge / .28f);
                pose.Recoil = m.Dying ? 0.f : std::min(1.f, e.Flash / .16f);
                pose.Death = m.Dying ? 1 - fade : 0.f;
                pose.Attack = def.Keep > 0 || model.Rig.Kind == CreatureRig::Type::Robed ? CreatureRig::Attack::Cast
                            : def.Height > 4 ? CreatureRig::Attack::Slam : CreatureRig::Attack::Swing;
                static Doorways::DoorwaysGpuSkinningPalette palette;   // 128 matrices: too large to build on the stack every creature
                BuildCreaturePalette(model.Rig, pose, palette);
                renderer.DrawDoorwaysGpuMeshSkinned(model.Mesh, place, viewProjection, palette, skin);
            }
            else renderer.DrawDoorwaysGpuMesh(model.Mesh, place, viewProjection, skin);
            if (!m.Dying && !def.Boss)
            {   // health bar
                const float ratio = static_cast<float>(e.Health) / static_cast<float>(e.MaxHealth);
                const float top = feet.y + def.Height + hover + .35f, width = std::min(2.4f, .9f + def.Radius);
                const XMMATRIX face = XMMatrixRotationY(std::atan2(eye.x - feet.x, eye.z - feet.z));
                renderer.DrawBox(XMMatrixScaling(width, .09f, .03f) * face * XMMatrixTranslation(feet.x, top, feet.z), viewProjection, Surface(white, {.03f, .03f, .04f, 1}, 0));
                if (ratio > 0)
                    renderer.DrawBox(XMMatrixScaling(width * .96f * ratio, .06f, .04f) * XMMatrixTranslation(-width * .48f * (1 - ratio), 0, 0) * face *
                                     XMMatrixTranslation(feet.x, top, feet.z), viewProjection, Surface(white, {def.Color.x, def.Color.y, def.Color.z, 1}, .9f));
            }
        }

        // ---- translucent: the light on the arena walls, lanterns and chest glows, far to near
        struct Item { XMFLOAT4X4 M; Material Mat; float Depth; };
        std::vector<Item> items;
        const auto push = [&](const XMMATRIX& m, const Material& mat)
        {
            const XMVECTOR c = XMVector4Transform(XMVector4Transform(XMVectorSet(0, 0, 0, 1), m), viewProjection);
            Item item; XMStoreFloat4x4(&item.M, m); item.Mat = mat; item.Depth = XMVectorGetW(c);
            if (std::isfinite(item.Depth)) items.push_back(item);
        };
        // glowing card: centre, half-extent axes across and up
        const auto card = [&](XMFLOAT3 at, XMFLOAT3 across, XMFLOAT3 up, XMFLOAT4 color)
        {
            XMMATRIX m = XMMatrixIdentity();
            m.r[0] = XMVectorSet(across.x, across.y, across.z, 0); m.r[1] = XMVectorSet(up.x, up.y, up.z, 0);
            m.r[2] = XMVector3Normalize(XMVector3Cross(m.r[0], m.r[1])); m.r[3] = XMVectorSet(at.x, at.y, at.z, 1);
            push(m, Surface(kit.GlowTexture(), color, 1, true));
        };
        const auto glow = [&](XMFLOAT3 at, float size, XMFLOAT4 color)
        {
            const XMFLOAT3 n = Norm(Sub(eye, at));
            XMVECTOR x = XMVector3Cross(XMVectorSet(0, 1, 0, 0), XMLoadFloat3(&n));
            x = XMVectorGetX(XMVector3Length(x)) < 1e-3f ? XMVectorSet(1, 0, 0, 0) : XMVector3Normalize(x);
            const XMVECTOR y = XMVector3Cross(XMLoadFloat3(&n), x);
            XMMATRIX m = XMMatrixIdentity();
            m.r[0] = x * size; m.r[1] = y * size; m.r[2] = XMLoadFloat3(&n); m.r[3] = XMVectorSet(at.x, at.y, at.z, 1);
            push(m, Surface(kit.GlowTexture(), color, 1, true));
        };
        for (const auto& l : mLights)
        {
            const float d = Len(Sub(l.P, eye));
            if (d > LightDrawDistance) continue;
            const float flicker = .85f + .15f * std::sin(mTime * 5.3f + l.P.x * .7f + l.P.z * .3f);
            // fades in the distance, and close to the camera so a lantern beside her does not wash the view out
            const float fade = Smooth(LightDrawDistance - d, 0, 25) * (.25f + .75f * Smooth(d, 4, 16));
            glow(l.P, l.Size * 1.25f, {l.Color.x, l.Color.y, l.Color.z, .60f * flicker * fade});
            glow(l.P, l.Size * .40f, {1, 1, 1, .85f * flicker * fade});
        }
        for (int i = 0; i < static_cast<int>(mFatherFeathers.size()); ++i)
        {   // her father's feathers: a pale light that rises and falls at the road's edge
            if (mCollection.Feathers >> i & 1u) continue;
            const float d = Len(Sub(mFatherFeathers[i], eye));
            if (d > FatherFeatherSight) continue;
            const float fade = Smooth(FatherFeatherSight - d, 0, 30), bob = .25f * std::sin(mTime * 1.7f + static_cast<float>(i));
            const XMFLOAT3 at = Add(mFatherFeathers[i], {0, bob, 0});
            glow(at, 2.4f + .3f * std::sin(mTime * 3.1f), {.6f, .85f, 1.f, .5f * fade});
            glow(at, .7f, {1, 1, 1, .9f * fade});
        }
        for (const auto& c : mChests)
            if (!c.Opened && !(c.Arena >= mArenasCleared && c.Tier > 0) && LenXZ(Sub(c.P, eye)) < 70)
                glow(Add(c.P, {0, .8f, 0}), 2.2f + .3f * std::sin(mTime * 2.4f + c.P.z), {1.f, .78f, .3f, .38f});
        {   // her evolutions: light on her wings, at her blades and at her feet, in the colour of each tier
            const XMFLOAT4 tierLight[4] = {{0, 0, 0, 0}, {.50f, .78f, 1.f, 1}, {.30f, 1.f, .60f, 1}, {1.f, .84f, .42f, 1}};
            const auto& her = world.MainPlayer;
            const float tall = 1 + FormScale * static_cast<float>(Growth::Form(mGrowth));
            const XMFLOAT3 forward{std::sin(her.Yaw), 0, std::cos(her.Yaw)}, side{std::cos(her.Yaw), 0, -std::sin(her.Yaw)};
            const auto light = [&](int tier, XMFLOAT3 local, float size, float alpha)
            {   // local: x to her right, y up, z ahead
                XMFLOAT4 color = tierLight[tier]; color.w = alpha * (.8f + .2f * std::sin(mTime * 2.6f + local.x * 3));
                glow({her.Position.x + side.x * local.x + forward.x * local.z, her.Position.y + local.y * tall, her.Position.z + side.z * local.x + forward.z * local.z},
                     size * (1 + .12f * static_cast<float>(tier)), color);
            };
            if (const int tier = mGrowth.Level[Growth::Wings])
                for (const float x : {-1.f, 1.f})
                {
                    light(tier, {x * .75f, 1.75f, -.45f}, .55f, .10f + .05f * static_cast<float>(tier));
                    light(tier, {x * 1.45f, 2.15f, -.55f}, .40f, .08f + .04f * static_cast<float>(tier));
                }
            if (const int tier = mGrowth.Level[Growth::Swords])
                for (const float x : {-1.f, 1.f}) light(tier, {x * .42f, .95f, .30f}, .26f, .10f + .05f * static_cast<float>(tier));
            if (const int tier = mGrowth.Level[Growth::Armour]) light(tier, {0, 1.2f, 0}, .60f, .04f + .025f * static_cast<float>(tier));
            if (const int tier = Growth::Form(mGrowth)) light(tier, {0, .12f, 0}, .85f, .08f + .04f * static_cast<float>(tier));
        }
        for (const Trouble& t : mTroubles)
        {   // trouble on the road: seen from far enough to choose a way round
            const XMFLOAT3 her = world.MainPlayer.Position;
            if (LenXZ(Sub(her, t.P)) > 130) continue;
            const float lit = mFighting ? .35f : 1.f;
            if (t.Kind == 0)
            {
                for (int k = 0; k < 14; ++k)
                {
                    const float a = static_cast<float>(k) * 2 * Pi / 14 + mTime * .3f;
                    glow({t.P.x + std::sin(a) * t.R, t.P.y + .3f, t.P.z + std::cos(a) * t.R}, .5f, {.62f, .3f, .95f, .75f * lit});
                }
                for (int k = 0; k < 7; ++k)
                {
                    const float a = static_cast<float>(k) * 2.4f - mTime * .5f, r = t.R * (.15f + .11f * static_cast<float>(k));
                    glow({t.P.x + std::sin(a) * r, t.P.y + .35f + .25f * std::sin(mTime * 1.7f + static_cast<float>(k)), t.P.z + std::cos(a) * r},
                         1.1f, {.45f, .16f, .75f, (.35f + .2f * std::sin(mTime * 2 + static_cast<float>(k) * 1.3f)) * lit});
                }
            }
            else if (t.Kind == 1)
            {   // grit sifts down over the stretch, and an ember stands at each end of it
                for (int k = 0; k < 8; ++k)
                {
                    const float along = (static_cast<float>(k) - 3.5f) * (t.R / 4), across = std::sin(static_cast<float>(k) * 1.7f + t.Phase) * 4;
                    const float fall = std::fmod(mTime * 2.2f + static_cast<float>(k) * 1.3f, 7.f);
                    glow({t.P.x + t.Tx * along + t.Nx * across, t.P.y + 7 - fall, t.P.z + t.Tz * along + t.Nz * across}, .28f, {.8f, .72f, .62f, .5f * lit});
                }
                for (const float end : {-1.f, 1.f})
                    glow({t.P.x + t.Tx * t.R * end, t.P.y + .5f, t.P.z + t.Tz * t.R * end}, .8f, {1.f, .5f, .2f, (.5f + .3f * std::sin(mTime * 3)) * lit});
            }
            else
                for (int k = 0; k < WispCount; ++k)
                {
                    const XMFLOAT3 wisp = WispAt(t, k);
                    glow(wisp, .8f, {.6f, .88f, 1.f, .95f * lit});
                    glow(wisp, 2.2f, {.35f, .65f, 1.f, .28f * lit});
                }
        }
        for (const Strike& strike : mStrikes)   // the stone on its way down
            glow({strike.P.x, strike.P.y + .6f + 18 * std::max(0.f, strike.T) / RockfallWarn, strike.P.z}, 1.1f, {.8f, .66f, .52f, .95f});
        if (mWayShow > .01f)
        {   // the way: lights run along the ground from her feet toward the road
            const XMFLOAT3 from = world.MainPlayer.Position;
            XMFLOAT3 to = Sub(mWayTarget, from); to.y = 0;
            const float distance = LenXZ(to);
            if (distance > 3)
            {
                const XMFLOAT3 dir = Norm(to);
                for (int k = 0; k < 10; ++k)
                {
                    const float d = 2 + std::fmod(static_cast<float>(k) * (WayTrail / 10) + mTime * 5, WayTrail);
                    if (d > distance) continue;
                    const float px = from.x + dir.x * d, pz = from.z + dir.z * d;
                    const float fade = mWayShow * Smooth(d, 2, 5) * (1 - Smooth(d, WayTrail * .6f, WayTrail));
                    glow({px, std::max(GetNature().Height(px, pz), from.y - 1.5f) + .8f, pz}, .55f, {1.f, .82f, .40f, .85f * fade});
                }
            }
        }
        if (mWall > 0 && mWallArena >= 0 && mWallArena < static_cast<int>(mArenas.size()))
        {   // a band of light burns along each arena wall, and dust lies at its foot while it moves
            const Arena& a = mArenas[mWallArena];
            const float lit = Smooth(mWall, .5f, 1);
            for (const float end : {1.f, -1.f})
            {
                const Sample& s = mSamples[std::clamp(static_cast<int>((CageEnd(a, end) + end * WallStandOff) / mStep), 0, static_cast<int>(mSamples.size()) - 1)];
                if (LenXZ(Sub(s.P, eye)) > 150) continue;
                if (((eye.x - s.P.x) * s.Tx + (eye.z - s.P.z) * s.Tz) * end > -WallBlockDepth) continue;   // the camera is at or behind this wall: no band of fire in its face
                const float half = s.W + 1.5f, face = -end * (WallBlockDepth * .5f + .12f);     // on the side that faces the arena
                const XMFLOAT3 at{s.P.x + s.Tx * face, s.P.y, s.P.z + s.Tz * face};
                if (lit > 0)
                    for (const float h : {1.5f, 3.1f})
                        card({at.x, at.y + h, at.z}, {s.Nx * half, 0, s.Nz * half}, {0, .16f + .05f * std::sin(mTime * 3 + h), 0},
                             {1.f, .42f, .25f, .75f * lit * (.8f + .2f * std::sin(mTime * 6 + h))});
                if (mWall < 1)
                    for (float l = -half; l <= half; l += 2.4f)
                        glow({s.P.x + s.Nx * l, s.P.y + .5f, s.P.z + s.Nz * l}, 2.6f, {.62f, .58f, .52f, .30f * std::sin(mWall * Pi)});
            }
        }
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.Depth > b.Depth; });
        for (const auto& item : items)
        {
            renderer.BindDoorwaysMaterialTexture(item.Mat);
            renderer.SetAlphaBlendingEnabled(true);
            renderer.SetDepthWriteEnabled(false);
            const XMMATRIX m = XMLoadFloat4x4(&item.M);
            for (const auto& submesh : kit.QuadMesh().Submeshes)
                renderer.DrawDoorwaysGpuSubmesh(kit.QuadMesh(), submesh, m, viewProjection, item.Mat, false, true);
        }
        renderer.ResetDefaultWorldRenderState();
        renderer.BindDoorwaysSkinningDisabled();
    }

    // ================================================================ HUD
    // The journey's part of the HUD (the hero's own part is NyxaraHud.cpp):
    //   top left      where she is: chapter, region, the road walked, what she carries, what to do now
    //   top centre    a guardian's health; under it the title of a new region or story beat, small and brief
    //   top right     the minimap (WorldMap.cpp); under it what a chest or a city gave
    //   bottom right  the keys of the journal and the inventory
    //   over all      the world map (M, WorldMap.cpp) or the story journal (F1, Story.cpp)
    void Journey::DrawHud(Ui& ui, const WorldState& world)
    {
        if (!mReady) return;
        FillMapView(world);
        using namespace Palette;
        const float W = ui.W, H = ui.H, S = ui.S;
        // layout, in layout units
        constexpr float Margin = 22, Emblem = 66, RoadWidth = 236, RoadHeight = 11, Tiny = 1.55f, Small = 1.9f, Name = 2.55f;
        constexpr float BossTop = 50, BossHeight = 17, BossMaxWidth = 520, BossSide = 392, BossTrailPerSecond = .25f;
        constexpr float ToastWidth = 300, ToastGap = 8, MaxEnemyPips = 14;
        const float ToastTop = WorldMap::MinimapBottom() + 10;   // under the minimap
        const float overlay = std::max({mJournal.Open, mMapFade, mGrowthFade});  // 0..1: the journal, the map or her window is open

        ui.Sprite(HudArt::GradV, 0, 0, W, 150 * S, Black.Fade(.5f), Pi);   // shade behind the top of the HUD

        // ---- where she is
        const float ex = Margin * S, ey = 16 * S, es = Emblem * S;
        ui.Sprite(HudArt::Medallion, ex, ey, es, es);
        ui.Centered(Ui::Roman(mStage + 1), ex + es * .5f, ey + (es - ui.Height(2.6f * S)) * .5f, 2.6f * S, GoldLight);
        const float tx = ex + es + 12 * S;
        const bool inCity = mCity >= 0;
        ui.Spaced(inCity ? std::wstring(L"CITY ") + Ui::Roman(mCity + 1) + L" OF VII"
                : std::wstring(L"CHAPTER ") + Ui::Roman(mStage + 1) + L" OF XII", tx, ey + 1 * S, Tiny * S, 2.5f * S, Gold);
        ui.Text(inCity ? Story::Cities[mCity].Name : Story::Chapters[mStage].Title, tx, ey + 15 * S, Name * S, Ink);
        const float total = static_cast<float>(mSamples.size() - 1) * mStep;
        const float walked = mCompleted ? 1.f : std::clamp(mS / total, 0.f, 1.f);
        const float bx = tx, by = ey + 43 * S, bw = RoadWidth * S, bh = RoadHeight * S;
        ui.Bar(bx, by, bw, bh, walked, Gold, 0, true);
        for (int i = 1; i < 12; ++i) ui.Rect(bx + bw * static_cast<float>(i) / 12.f, by + 2 * S, 1 * S, bh - 4 * S, Black.Fade(.55f));
        ui.Icon(HudArt::Marker, bx + 3 * S + (bw - 6 * S) * walked, by + bh * .5f, 20 * S);
        ui.Icon(HudArt::IconMountain, bx + bw + 15 * S, by + bh * .5f - 1 * S, 20 * S, Silver);
        // what she carries
        float rx = tx; const float ry = by + bh + 8 * S, icon = 17 * S;
        const auto carried = [&](const HudArt::Sprite& art, Color tint, const std::wstring& value)
        {
            ui.Icon(art, rx + icon * .5f, ry + ui.Height(Small * S) * .5f, icon, tint);
            ui.Text(value, rx + icon + 4 * S, ry, Small * S, Ink);
            rx += icon + 4 * S + ui.Width(value, Small * S) + 16 * S;
        };
        carried(HudArt::IconCoin, GoldLight, std::to_wstring(mGold));
        carried(HudArt::IconCrystal, Violet, std::to_wstring(mGrowth.Crystals));
        carried(HudArt::IconFeather, Silver, std::to_wstring(mFeathers));
        carried(HudArt::IconEye, Moon, std::to_wstring(mLore) + L"/12");

        // ---- what to do now
        {
            const float oy = ey + es + 22 * S, ox = Margin * S, middle = oy + ui.Height(Small * S) * .5f;
            std::wstring objective;
            if (mFinale == Finale::Chains) objective = L"Break the two chain anchors on the ledge";
            else if (mFinale == Finale::Freed || mFinale == Finale::Done) objective = L"Amirani is free. Hold F9 to walk the road again";
            else if (mFinale == Finale::Fight || mFinale == Finale::Words) objective = L"Defeat the Eagle";
            else if (mFighting) objective = L"Wave " + std::to_wstring(std::max(1, mWave)) + L" of " + std::to_wstring(mWaves);
            else if (mTroubleNear >= 0 && mTroubles[mTroubleNear].Kind == 0) objective = L"Cursed ground. Walk round it";
            else if (mTroubleNear >= 0 && mTroubles[mTroubleNear].Kind == 1) objective = L"Stones fall here. Keep moving, leave the rings";
            else if (mTroubleNear >= 0) objective = L"Wisps cross the road. Slip between them";
            else if (mWayShow > .5f && mWayBack) objective = L"A fight waits behind you. Go back along the road";
            else if (mWayShow > .5f) objective = L"The land is yours. The lights lead back to the road";
            else if (mMapView.NextCity >= 0 && mCity < 0) objective = std::wstring(L"Follow the road to ") + Story::Cities[mMapView.NextCity].Name;
            else objective = L"Follow the road to the mountain";
            if (mFighting && mFinale == Finale::None)
            {
                ui.Icon(HudArt::IconSkull, ox + 9 * S, middle, 18 * S, Red);
                ui.Text(objective, ox + 24 * S, oy, Small * S, Ink);
                const int left = AliveEncounterCount() + static_cast<int>(mQueue.size());
                float px = ox + 24 * S + ui.Width(objective, Small * S) + 16 * S;
                for (int i = 0; i < std::min(left, static_cast<int>(MaxEnemyPips)); ++i, px += 11 * S) ui.Icon(HudArt::Pip, px, middle, 11 * S, Red);
                if (left > static_cast<int>(MaxEnemyPips)) ui.Text(L"+" + std::to_wstring(left - static_cast<int>(MaxEnemyPips)), px, oy, Tiny * S, Dim);
            }
            else
            {
                ui.Icon(HudArt::Diamond, ox + 9 * S, middle, 13 * S);
                ui.Text(objective, ox + 24 * S, oy, Small * S, GoldLight);
            }
            if (mEmber > 0)
            {
                const float y2 = oy + ui.Height(Small * S) + 8 * S;
                ui.Icon(HudArt::IconEmber, ox + 9 * S, y2 + ui.Height(Tiny * S) * .5f, 18 * S, Soul);
                ui.Spaced(L"SOUL EMBER", ox + 24 * S, y2, Tiny * S, 2 * S, Soul);
                ui.Bar(ox + 24 * S + ui.SpacedWidth(L"SOUL EMBER", Tiny * S, 2 * S) + 10 * S, y2 + 2 * S, 90 * S, 9 * S, mEmber / EmberSeconds, Soul, 0, true);
            }
        }

        // ---- a guardian's health
        bool boss = false;
        const auto& enemies = GetNyxaraKit().Enemies();
        for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i)
        {
            const Monster& monster = mMonsters[i];
            if (!monster.Active || monster.Dying || !(Kinds[monster.Kind].Boss || monster.Kind == ChainKind)) continue;
            if (monster.Kind == ChainKind && i > 0 && mMonsters[i - 1].Active && mMonsters[i - 1].Kind == ChainKind) continue;
            const float bossWidth = std::clamp(W - 2 * BossSide * S, 200 * S, BossMaxWidth * S), x0 = (W - bossWidth) * .5f, y0 = BossTop * S;
            const float f = std::clamp(static_cast<float>(enemies[i].Health) / static_cast<float>(enemies[i].MaxHealth), 0.f, 1.f);
            if (mBossSlot != i) { mBossSlot = i; mBossTrail = f; }
            mBossTrail = f > mBossTrail ? f : std::max(f, mBossTrail - BossTrailPerSecond * ui.Delta);
            ui.SpacedCentered(Kinds[monster.Kind].Name, W * .5f, y0 - ui.Height(Small * S) - 8 * S, Small * S, 3 * S, GoldLight);
            ui.Bar(x0, y0, bossWidth, BossHeight * S, f, enemies[i].Flash > 0 ? White : Red, mBossTrail);
            ui.Icon(HudArt::Diamond, x0, y0 + BossHeight * S * .5f, 22 * S);
            ui.Icon(HudArt::Diamond, x0 + bossWidth, y0 + BossHeight * S * .5f, 22 * S);
            boss = true;
            break;
        }
        if (!boss) mBossSlot = -1;

        // ---- the way: a small compass at the top, turned with the camera, with the mark where the road is
        if (mWayShow > .02f && overlay < 1 && mBanners.empty() && !boss)
        {
            const float fade = mWayShow * (1 - overlay), cx = W * .5f, cy = 86 * S, r = 34 * S;
            const XMFLOAT3 her = world.MainPlayer.Position;
            const float rel = std::atan2(mWayTarget.x - her.x, mWayTarget.z - her.z) - world.MainCamera.Yaw;
            ui.Sprite(HudArt::Glow, cx - 170 * S, cy - 64 * S, 340 * S, 190 * S, Black.Fade(.75f * fade));
            for (int k = 0; k < 24; ++k)
            {
                const float a = static_cast<float>(k) * 2 * Pi / 24;
                ui.Icon(HudArt::Pip, cx + std::sin(a) * r, cy - std::cos(a) * r, (k % 6 == 0 ? 10 : 6) * S, (k % 6 == 0 ? GoldLight : Silver).Fade(fade));
            }
            ui.Icon(HudArt::IconWings, cx, cy, 26 * S, Silver.Fade(fade));
            ui.Icon(HudArt::Diamond, cx + std::sin(rel) * r, cy - std::cos(rel) * r, 22 * S, GoldLight.Fade(fade));
            const std::wstring line = std::wstring(mWayBack ? L"A FIGHT WAITS BEHIND YOU" : L"THE ROAD") + L"   " + std::to_wstring(static_cast<int>(mWayDistance)) + L" m";
            ui.SpacedCentered(line, cx, cy + r + 14 * S, Tiny * S, 2.5f * S, GoldLight.Fade(fade));
        }

        // ---- title of a new region or story beat: small, at the top, gone in a few seconds
        if (!mBanners.empty() && overlay < 1)
        {
            const Banner& banner = mBanners.front();
            const float fade = std::min({1.f, mBannerTime / .5f, (banner.Seconds - mBannerTime) / .7f}) * (1 - overlay);
            if (fade > .02f)
            {
                const float cx = W * .5f, titlePixels = 3.f * S, tracking = 5 * S;
                float y = (boss ? BossTop + BossHeight + 18 : 46) * S - (1 - std::min(1.f, mBannerTime / .5f)) * 8 * S;
                const float span = std::max(ui.SpacedWidth(banner.Title, titlePixels, tracking), ui.Width(banner.Line, Small * S)) + 240 * S;
                ui.Sprite(HudArt::Glow, cx - span * .5f, y - 34 * S, span, 150 * S, Black.Fade(.55f * fade));
                if (!banner.Label.empty()) ui.SpacedCentered(banner.Label, cx, y, Tiny * S, 4 * S, Gold.Fade(fade));
                y += ui.Height(Tiny * S) + 5 * S;
                ui.SpacedCentered(banner.Title, cx, y, titlePixels, tracking, Ink.Fade(fade));
                y += ui.Height(titlePixels) + 2 * S;
                ui.Sprite(HudArt::Divider, cx - 190 * S, y, 380 * S, 24 * S, White.Fade(fade));
                if (!banner.Line.empty()) ui.Centered(banner.Line, cx, y + 26 * S, Small * S, Dim.Fade(fade));
            }
        }

        // ---- what a chest held (right edge, newest at the bottom)
        {
            const float tw = ToastWidth * S, pad = 11 * S, art = 30 * S, textWidth = tw - art - 3 * pad;
            float y = ToastTop * S;
            for (auto& toast : mToasts)
            {
                const auto lines = ui.Wrap(toast.Text, Small * S, textWidth);
                const float th = 2 * pad + ui.Height(Tiny * S) + 4 * S + static_cast<float>(lines.size()) * (ui.Height(Small * S) + 2 * S);
                const float in = std::min(1.f, toast.Age / .25f), out = std::min(1.f, (toast.Seconds - toast.Age) / .4f);
                const float fade = in * out * (1 - overlay);
                toast.Y = toast.Y < 0 ? y : toast.Y + (y - toast.Y) * std::min(1.f, ui.Delta * 12);
                const float x0 = W - Margin * S - tw + (1 - in) * 40 * S, y0 = toast.Y;
                ui.Nine(HudArt::Panel, x0, y0, tw, th, White.Fade(fade));
                ui.Rect(x0 + 2 * S, y0 + 5 * S, 2.5f * S, th - 10 * S, toast.Tint.Fade(fade));
                ui.Icon(*toast.Art, x0 + pad + art * .5f, y0 + th * .5f, art, toast.Tint.Fade(fade));
                float ty = y0 + pad;
                ui.Spaced(toast.Title, x0 + art + 2 * pad, ty, Tiny * S, 2 * S, GoldLight.Fade(fade));
                ty += ui.Height(Tiny * S) + 4 * S;
                for (const auto& line : lines) { ui.Text(line, x0 + art + 2 * pad, ty, Small * S, Ink.Fade(fade)); ty += ui.Height(Small * S) + 2 * S; }
                y += th + ToastGap * S;
            }
        }

        // ---- keys (bottom right, one under another): the journal key glows when it has something new
        {
            const float keyHeight = 20 * S, row = 25 * S, keyColumn = W - Margin * S - 104 * S;
            float y = H - Margin * S - keyHeight - 3 * row;
            {   // potions, above the key list
                const float py = y - row - 6 * S, px = keyColumn - 8 * S;
                const wchar_t* label = Ui::KeyLabel(L"Z", PotionKey);
                const float kw = ui.KeyWidth(label, keyHeight);
                ui.Key(label, px - kw * .5f, py, keyHeight, mGrowth.Potions > 0 ? White : Faint);
                for (int i = 0; i < MaxPotions; ++i)
                    ui.Icon(HudArt::IconPotion, keyColumn + (static_cast<float>(i) + .5f) * 19 * S, py + keyHeight * .5f, 19 * S, i < mGrowth.Potions ? Red : Faint.Fade(.6f));
            }
            const auto hint = [&](const wchar_t* key, const std::wstring& label, bool lit)
            {
                const float kw = ui.KeyWidth(key, keyHeight), kx = keyColumn - kw * .5f - 8 * S;
                if (lit) ui.Icon(HudArt::Glow, kx, y + keyHeight * .5f, 62 * S, GoldLight.Fade(.35f + .3f * std::sin(ui.Time * 5)));
                ui.Key(key, kx, y, keyHeight, lit ? GoldLight : White);
                ui.Text(label, keyColumn, y + (keyHeight * .86f - ui.Height(Tiny * S)) * .5f, Tiny * S, lit ? GoldLight : Dim);
                y += row;
            };
            hint(Ui::KeyLabel(L"F1", VK_F1), mJournal.Unread ? L"STORY - NEW" : L"STORY", mJournal.Unread != 0);
            hint(Ui::KeyLabel(L"M", 'M'), L"MAP", false);
            bool canGrow = false;
            for (int item = 0; item < Growth::RowCount; ++item) { const int cost = Growth::Cost(mGrowth, item); canGrow = canGrow || (cost > 0 && mGrowth.Crystals >= cost); }
            hint(Ui::KeyLabel(L"I", 'I'), canGrow ? L"NYXARA - GROW" : L"NYXARA", canGrow);
            hint(L"F9", L"HOLD: RESTART", false);
        }
        if (mRestartHold > .1f)
        {   // restart: a bar that fills while F9 is held
            const float pw = 380 * S, ph = 66 * S, px = (W - pw) * .5f, py = H * .60f;
            ui.Nine(HudArt::Panel, px, py, pw, ph);
            ui.SpacedCentered(L"BEGIN THE JOURNEY AGAIN", W * .5f, py + 11 * S, Small * S, 3 * S, GoldLight);
            ui.Bar(px + 22 * S, py + 36 * S, pw - 44 * S, 12 * S, mRestartHold / RestartHoldSeconds, Red, 0, true);
            ui.Centered(L"Keep holding F9. Everything won on the road is lost.", W * .5f, py + ph + 8 * S, Tiny * S, Dim);
        }

        WorldMap::DrawMinimap(ui, mMapView, 1 - overlay);
        if (mGrowthFade > 0)
        {
            if (mGrowthPage == 1)
            {
                Collection::View kept;
                kept.Now = mCollection; kept.Cities = CityBits(); kept.Selected = mRuneRow; kept.Fade = mGrowthFade; kept.Flash = mGrowthFlash;
                Collection::Draw(ui, kept);
            }
            Growth::View view;
            view.Now = mGrowth; view.Selected = mGrowthRow; view.Fade = mGrowthFade; view.Flash = mGrowthFlash; view.FlashRow = mGrowthFlashRow;
            view.Feathers = mFeathers; view.Gold = mGold; view.Memories = mLore; view.PotionHeal = PotionHealth();
            view.OtherDamage = (1 + FeatherDamageBonus * static_cast<float>(mFeathers)) * (Gift(6) ? SvanSteelDamage : 1.f);
            view.OtherDamage *= Collection::Damage(mCollection, CitiesVisited());
            view.OtherTaken = (Gift(3) ? StoneDamageScale : 1.f) * Collection::DamageTaken(mCollection, CitiesVisited());
            if (mGrowthPage == 0) Growth::Draw(ui, view);
        }
        WorldMap::DrawMap(ui, mMapView, mMapFade);
        Story::DrawJournal(ui, mJournal);
        DrawPause(ui);
        Settings::Draw(ui, mUiFocused, false);
        Guide::Draw(ui, mGuidePage, mGuideFade);
    }

    // Her own menu (Esc): the same panel, gold and ink as her other windows. The engine's pause menu is not used.
    void Journey::DrawPause(Ui& ui) const
    {
        if (mPauseFade <= 0) return;
        using namespace Palette;
        const float S = ui.S, a = mPauseFade;
        constexpr float Width = 400, Row = 46, Gap = 5, Pad = 16, Head = 96, Foot = 44, Tiny = 1.55f, Small = 2.0f, Name = 2.6f;
        struct Entry { const wchar_t* Label; const wchar_t* Note; };
        const Entry entries[PauseRows] = {
            {L"Continue the journey", L""}, {L"Nyxara", L"wings, swords, armour"}, {L"The story", L"legend and memories"}, {L"The map", L""},
            {L"Settings", L"picture, sound, mouse"}, {L"Guide", L"how to play, what it needs"}, {L"To the start screen", L"the road is saved"},
            {L"Leave the game", L"the road is saved"}};
        const float w = Width * S, h = (Head + PauseRows * (Row + Gap) + Foot) * S;
        const float x = (ui.W - w) * .5f, y = (ui.H - h) * .5f;
        ui.Rect(0, 0, ui.W, ui.H, Black.Fade(.5f * a));
        ui.Nine(HudArt::Panel, x, y, w, h, White.Fade(a));
        ui.SpacedCentered(L"CAUCASIAN EAGLE", x + w * .5f, y + 16 * S, Tiny * S, 4 * S, Gold.Fade(a));
        ui.Centered(L"The road waits", x + w * .5f, y + 34 * S, Name * S, Ink.Fade(a));
        ui.Centered(L"Nothing moves while this is open.", x + w * .5f, y + 66 * S, Tiny * S, Dim.Fade(a));
        float cy = y + Head * S;
        for (int i = 0; i < PauseRows; ++i)
        {
            const bool selected = i == mPauseRow;
            const float cx = x + Pad * S, cw = w - 2 * Pad * S, ch = Row * S;
            if (mPauseOpen) Pointer::Add(Pointer::PauseRow, i, cx, cy, cw, ch);
            ui.Rect(cx, cy, cw, ch, Well.Fade(a * (selected ? .92f : .55f)));
            const Color edge = selected ? GoldLight.Fade(a * (.75f + .25f * std::sin(ui.Time * 4))) : Gold.Fade(a * .25f);
            const float line = (selected ? 2 : 1) * S;
            ui.Rect(cx, cy, cw, line, edge); ui.Rect(cx, cy + ch - line, cw, line, edge);
            ui.Rect(cx, cy, line, ch, edge); ui.Rect(cx + cw - line, cy, line, ch, edge);
            ui.Icon(HudArt::Diamond, cx + 20 * S, cy + ch * .5f, (selected ? 13 : 9) * S, (selected ? GoldLight : Faint).Fade(a));
            const Color ink = i >= 6 ? (selected ? Red : Dim) : (selected ? Ink : Dim);
            ui.Text(entries[i].Label, cx + 40 * S, cy + (ch - ui.Height(Small * S)) * .5f, Small * S, ink.Fade(a));
            ui.RightAligned(entries[i].Note, cx + cw - 14 * S, cy + (ch - ui.Height(Tiny * S)) * .5f, Tiny * S, (selected ? Dim : Faint).Fade(a));
            cy += (Row + Gap) * S;
        }
        ui.Centered(L"W  S  or the mouse      ENTER  take      ESC  back", x + w * .5f, cy + 10 * S, Tiny * S, Dim.Fade(a));
    }
}
