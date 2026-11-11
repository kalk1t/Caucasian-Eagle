#define NOMINMAX
#include "Journey.h"

#include "AmiraniMountain.h"
#include "CombatSystem.h"
#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysGame.h"
#include "DoorwaysHudTextLayout.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRenderContexts.h"
#include "DoorwaysRuntimePathResolver.h"
#include "DoorwaysSettingsPaths.h"
#include "DoorwaysUiTheme.h"
#include "EnvironmentSettings.h"
#include "GameConstants.h"
#include "HudTextRenderer.h"
#include "Renderer.h"
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

        // ------------------------------------------------------------ words
        struct StageText { const wchar_t* Title; const wchar_t* Body; };
        const StageText StageCards[12] = {
            {L"THE MOONLIT CAUSEWAY", L"YEAR ONE. THE GATE STANDS OPEN. THE MOUNTAIN WAITS AT THE END OF THE ROAD."},
            {L"THE WHISPERING WOOD", L"CHINKA LAUGH IN THE DEAD BRANCHES. THE KUDIANI WITCHES HAVE LIT THEIR GREEN FIRES."},
            {L"THE DROWNED MARSH", L"DO NOT ANSWER THE WOMEN IN THE WATER. THE ALI KNOW YOUR NAME."},
            {L"OCHOKOCHI'S WOOD", L"THE GOAT-MAN HUNTS HERE, WITH THE BLADE THAT GROWS FROM HIS CHEST."},
            {L"THE STONE TOWERS", L"THE PEOPLE OF THE TOWERS LEFT LONG AGO. THE WILD MEN OF THE HEIGHTS DID NOT."},
            {L"THE SNOW PASS", L"YEAR FOUR. THE ALMASTY THROW THE MOUNTAIN AT THOSE WHO CLIMB IT."},
            {L"KAJETI", L"THE FORTRESS OF THE KAJI SMITHS. THEIR HAMMERS RENEW THE CHAIN EVERY YEAR."},
            {L"THE GIANTS' VALLEY", L"THE ONE-EYED UAIG GUARD THE BONES OF THEIR FATHERS."},
            {L"THE DRAGON LAKE", L"THE GVELESHAPI SLEEPS UNDER THE BLACK WATER. THE ROAD CROSSES ITS BACK."},
            {L"DEVI'S GORGE", L"THREE HEADS, ONE HUNGER. THE DEVI KEEP THE GOLD OF EVERY TRAVELLER WHO CAME BEFORE YOU."},
            {L"THE WITCHES' HILL", L"ROKAPI, QUEEN OF THE KUDIANI, IS CHAINED TO THIS HILL AS HE IS CHAINED TO HIS."},
            {L"THE CHAIN ROAD", L"YEAR SEVEN. THE CAVE STANDS OPEN. YOUR FATHER HAS SEEN YOU COMING."}};
        const wchar_t* Lore[12] = {
            L"\"YOU HAVE HIS EYES,\" AMIRANI SAID. \"BUT YOU LOOKED AT ME. HE NEVER HAS.\"",
            L"\"I STOLE NOTHING. I ASKED WHY MEN SHOULD KNEEL. FOR THAT, A CHAIN.\"",
            L"\"MY DOG HAS LICKED THIS IRON THIN A THOUSAND TIMES. EACH YEAR THE SMITHS STRIKE, AND IT IS WHOLE AGAIN.\"",
            L"\"SEVEN YEARS, LITTLE EAGLE. LEARN THE SWORD. LEARN THE SKY. THEN COME BACK FOR ME.\"",
            L"\"YOUR FATHER DOES NOT HATE ME. THAT IS THE WORST OF IT. HE ONLY OBEYS.\"",
            L"YOU ASKED HIM IF IT HURT. HE LAUGHED UNTIL THE CHAIN RANG.",
            L"\"WHEN I AM FREE I WILL REMEMBER WHO OPENED THE CHAIN.\" HE DID NOT SAY HOW.",
            L"THE OLD WOMEN SAY: IF AMIRANI WALKS, THE SMITHS' FIRES GO OUT FOR EVER. THEY DO NOT SAY IF THAT IS GOOD.",
            L"YOUR FATHER TAUGHT YOU TO FLY BEFORE YOU COULD WALK. HE NEVER TOLD YOU WHERE HE WENT AT DAWN.",
            L"\"HE WILL NOT STRIKE TO KILL YOU,\" AMIRANI SAID. \"USE THAT.\"",
            L"ROKAPI'S CURSE: \"HE CHARMED ME TOO, GIRL. LOOK WHERE I AM.\"",
            L"THE DOG WATCHES YOU CLIMB. IT HAS STOPPED LICKING THE CHAIN. IT IS WAITING."};

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
    bool Journey::LoadModel(Renderer& renderer, const std::string& base, Model& out, std::string& error)
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
        DoorwaysTextureLoadDesc desc;
        desc.Path = root / (base + "BaseColor.png");
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

    bool Journey::Initialize(Renderer& renderer, WorldState& world, std::string& error)
    {
        mReady = false;
        if (!LoadRoad(error)) return false;
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
        }
        mModels.resize(KindCount);
        for (int k = 0; k < KindCount; ++k)
            if (Kinds[k].Model[0] && !LoadModel(renderer, Kinds[k].Model, mModels[k], error)) return false;
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
        if (mArenasCleared >= static_cast<int>(mArenas.size()) && !mCompleted)
        {   // the Eagle has fallen but the chain still holds (a save made between the two, or a test start)
            mFinale = Finale::Chains;
            GetAmiraniMountain().SetEagleAway(true);
            for (const float side : {-1.f, 1.f})
                Spawn(ChainKind, {MountainX + side * LedgeHalfWidth * .62f, LedgeHeight, LedgeWalkBackZ - 2.5f}, false, 1);
        }
        if (mArenasCleared > 0) mPendingTeleport = true;
        else if (!unattended)
        {
            Say(L"CAUCASIAN EAGLE", L"ONCE IN SEVEN YEARS THE CAVE ON THE MOUNTAIN OPENS, AND AMIRANI CAN BE SEEN IN HIS CHAINS.", 7, true);
            Say(L"", L"YOU ARE NYXARA, DAUGHTER OF THE EAGLE THAT FEEDS ON HIM. SEVEN YEARS AGO YOU SAW YOUR FATHER AT HIS WORK, AND AMIRANI SPOKE TO YOU.", 8);
            Say(L"", L"HE ASKED YOU TO TRAIN, AND TO RETURN WHEN THE CAVE OPENS AGAIN: TO FIGHT YOUR FATHER AND OPEN THE CHAIN.", 8);
            Say(L"THE ROAD", L"LEAVE THE SANCTUARY BY THE GATE THAT FACES THE MOUNTAIN. BREAK THE OLD GOLD CHESTS. NOTHING ON THIS ROAD WANTS YOU TO ARRIVE.", 9);
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
        Log("[PASS] Journey: road %.0f m, %d arenas, %d chests, %d props, %d lights; cleared %d.\n",
            static_cast<float>(mSamples.size()) * mStep, static_cast<int>(mArenas.size()), static_cast<int>(mChests.size()),
            static_cast<int>(mProps.size()), static_cast<int>(mLights.size()), mArenasCleared);
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
        if (std::sqrt(x * x + z * z) < SanctuaryRadius && z < 93.f) return 0;
        index = Nearest(x, z, index, 40);
        return mSamples[index].P.y;
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

    void Journey::Leash(WorldState& world)
    {
        auto& p = world.MainPlayer;
        const float x = p.Position.x, z = p.Position.z;
        if (std::sqrt(x * x + z * z) < SanctuaryRadius && z < 93.5f) { mIndex = 0; mS = 0; mStage = 0; return; }
        const bool eagleDown = mFinale == Finale::Chains || mFinale == Finale::Freed || mFinale == Finale::Done;
        if (z > LedgeWalkFrontZ - .5f && z < LedgeWalkBackZ + 1 && std::abs(x) < LedgeWalkHalfWidth + 1 && eagleDown)
        { mIndex = static_cast<int>(mSamples.size()) - 1; mS = mIndex * mStep; mStage = 11; return; }

        mIndex = Nearest(x, z, mIndex, 90);
        {
            const float dx = mSamples[mIndex].P.x - x, dz = mSamples[mIndex].P.z - z;
            if (dx * dx + dz * dz > 40.f * 40.f) mIndex = Nearest(x, z, -1, 0);
        }
        mS = mIndex * mStep;
        mStage = mSamples[mIndex].Stage;

        float minS = 0, maxS = static_cast<float>(mSamples.size() - 1) * mStep;
        if (!eagleDown) maxS -= 1.0f;
        if (mArenasCleared < static_cast<int>(mArenas.size()))
        {
            const Arena& a = mArenas[mArenasCleared];
            if (a.Kind != 2) maxS = std::min(maxS, a.S + a.R * .92f);
            if (mFighting) minS = a.S - a.R * .95f;
        }
        int index = mIndex; bool fix = false;
        if (mS > maxS) { index = static_cast<int>(maxS / mStep); fix = true; }
        else if (mS < minS) { index = static_cast<int>(minS / mStep) + 1; fix = true; }
        const Sample& s = mSamples[index];
        const float lateral = (x - mSamples[mIndex].P.x) * mSamples[mIndex].Nx + (z - mSamples[mIndex].P.z) * mSamples[mIndex].Nz;
        if (!fix)
        {   // the road's edge: she slides along it (the collision is wider than the road, so the engine never stops her dead)
            const Sample& here = mSamples[mIndex];
            const float limit = here.W - .5f;
            if (std::abs(lateral) <= limit) return;
            const float over = lateral - std::clamp(lateral, -limit, limit);
            p.Position.x -= here.Nx * over; p.Position.z -= here.Nz * over;
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
        Log("[JOURNEY] arena %d (stage %d) begins at %.0f s played.\n", arena, a.Stage, mPlaySeconds);
    }

    void Journey::ClearArena(WorldState& world, CombatSystem& combat)
    {
        const int arena = mArenasCleared;
        mFighting = false; ++mArenasCleared;
        combat.RestorePlayerHealth(world, ArenaHeal);
        GetNyxaraKit().Notice(L"THE WAY IS OPEN", 2.2f);
        GetNyxaraKit().RingEffect(Add(mArenas[arena].P, {0, .05f, 0}), mArenas[arena].R, {.7f, 1.f, .85f, 1}, 1.2f);
        Log("[JOURNEY] arena %d cleared at %.0f s played (gold %d, feathers %d, deaths %d).\n", arena, mPlaySeconds, mGold, mFeathers, mDeaths);
        Save();
    }

    void Journey::UpdatePatrols(WorldState& world, CombatSystem& combat)
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
            if (mS > a.S - a.R * .55f && mS < a.S + a.R && combat.PlayerActionsAllowed()) StartEncounter(arena);
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
                if (slot >= 0 && Kinds[kind].Boss)
                    Say(Kinds[kind].Name, kind == Gveleshapi ? L"THE WATER STANDS UP. IT HAS TEETH." :
                        kind == DeviLord ? L"\"LITTLE BIRD. I HAVE EATEN EAGLES.\"" : L"\"HE SENT YOU. OF COURSE HE DID.\"", 5);
            }
            return;
        }
        if (AliveEncounterCount() > 0) return;
        mWaveDelay -= dt;
        if (mWaveDelay > 0) return;
        if (mWave >= mWaves) { ClearArena(world, combat); return; }
        BuildWave(arena, mWave, mQueue);
        if (mWave > 0) combat.RestorePlayerHealth(world, WaveHeal);
        ++mWave; mWaveDelay = WaveDelay; mSpawnTimer = 0;
        if (mWave > 1 && !(a.Kind == 1 && mWave == mWaves)) GetNyxaraKit().Notice(mWave == mWaves ? L"LAST WAVE" : L"MORE ARE COMING", 1.4f);
    }

    void Journey::OnDeath(int slot, WorldState& world, CombatSystem& combat)
    {
        Monster& m = mMonsters[slot];
        m.Dying = true; m.Death = .55f;
        const KindDef& def = Kinds[m.Kind];
        const XMFLOAT3 at = GetNyxaraKit().Enemies()[slot].Pos;
        if (m.Kind == ChestKind) { OpenChest(m.Chest, world, combat); return; }
        if (m.Kind == ChainKind) { GetNyxaraKit().Effect(at, {.8f, .9f, 1.f, 1}, 60, 8.f, .35f); GetNyxaraKit().Shake(.25f); return; }
        mGold += def.Boss ? 150 : def.Cost * 2 + 1;
        if (!def.Boss && combat.PlayerActionsAllowed()) combat.RestorePlayerHealth(world, KillHealBase + KillHealPerCost * def.Cost);
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
                    kit.Hurt(world, combat, static_cast<int>(22 * MonsterDamageScale));
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
            ring(10, 6.5f, static_cast<int>(8 * MonsterDamageScale)); ++m.Stage;
            m.Special = health < .5f ? 6.f : 9.f;
            break;
        case DeviLord:
            if ((health < .66f && m.Stage < 30) || (health < .33f && m.Stage < 60)) { summon(Chinka, 4); m.Stage += 30; }
            ring(10, 7.f, static_cast<int>(9 * MonsterDamageScale)); kit.Shake(.2f); ++m.Stage;
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
                ring(health < .5f ? 16 : 12, 7.5f, static_cast<int>(9 * MonsterDamageScale)); ++m.Stage;
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

        int windingUp = 0;   // ordinary creatures in the middle of a blow
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
            const float damageScale = MonsterDamageScale * (1 + StageDamageGrowth * static_cast<float>(mSamples[m.Index].Stage));

            XMFLOAT3 to = Sub(player.Position, e.Pos); to.y = 0;
            const float distance = LenXZ(to);
            const XMFLOAT3 dir = Norm(to);
            e.Yaw = AngleTo(e.Yaw, std::atan2(dir.x, dir.z), 6.f * dt);
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
                    m.Lunge = .28f; m.Cooldown = def.Cooldown * (.85f + .3f * Random());
                    if (distance < def.Reach + def.Radius + .9f && alive && !kit.Blocks(world, e.Pos))
                    {
                        kit.Hurt(world, combat, std::max(1, static_cast<int>(static_cast<float>(def.Damage) * damageScale)));
                        kit.Effect(chest, {1.f, .3f, .25f, 1}, 10, 3.f, .2f); kit.Shake(def.Damage > 12 ? .22f : .1f);
                    }
                    if (def.Radius > 1.2f) kit.RingEffect({e.Pos.x, m.Ground + .05f, e.Pos.z}, def.Reach + def.Radius, def.Color, .45f);
                }
            }
            else if (alive && m.Cooldown <= 0 && distance < def.Reach + def.Radius + .2f && def.Reach > 0 && !def.Boss && windingUp >= MaxAttackers)
                m.Cooldown = .25f + .35f * Random();   // others are already striking: wait for a turn
            else if (alive && m.Cooldown <= 0 && distance < def.Reach + def.Radius + .2f && def.Reach > 0)
            {
                if (!def.Boss) ++windingUp;
                m.Windup = def.Windup; rooted = true;
                kit.RingEffect({e.Pos.x, m.Ground + .05f, e.Pos.z}, def.Reach + def.Radius, {1.f, .35f, .25f, 1}, def.Windup);
            }

            // ---- ranged
            if (def.Range > 0 && alive)
            {
                m.Shoot -= dt;
                if (m.Shoot <= 0 && distance < def.Range && (def.Keep > 0 || distance > 5.f) && !rooted)
                {
                    const XMFLOAT3 from = Add(e.Pos, {0, def.Height * .25f, 0});
                    const float base = std::atan2(chest.x - from.x, chest.z - from.z);
                    const float fall = (chest.y - from.y) / std::max(distance, 1.f);
                    const float speed = def.Boss ? 9.5f : 8.f;
                    for (int v = 0; v < def.Volley; ++v)
                    {
                        const float a = base + (static_cast<float>(v) - static_cast<float>(def.Volley - 1) * .5f) * .2f;
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
                float speed = def.Speed;
                if (!def.Boss && def.Range == 0)
                {   // leapers close the last metres in a rush
                    m.Special -= dt;
                    if (m.Special <= 0 && distance > 4 && distance < 13) { m.Special = 5 + 3 * Random(); m.Hover = -1; }
                    if (m.Hover < 0) { speed *= 2.4f; m.Hover += dt * 1.2f; if (m.Hover > -.05f || distance < def.Reach + def.Radius) m.Hover = 0; }
                }
                if (def.Keep > 0)
                {
                    if (distance > def.Keep + 1.5f) move = Mul(dir, speed);
                    else if (distance < def.Keep - 2.f) move = Mul(dir, -speed * .7f);
                    move = Add(move, Mul(XMFLOAT3{dir.z, 0, -dir.x}, m.Strafe * speed * .45f));
                    if (Random() < dt * .25f) m.Strafe = -m.Strafe;
                }
                else if (distance > def.Reach + def.Radius - .3f) move = Mul(dir, speed);
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
        mGold += gold;
        const std::wstring coins = std::to_wstring(gold) + L" OLD GOLD COINS";
        const int stage = mSamples[Nearest(c.P.x, c.P.z, -1, 0)].Stage;
        if (c.Tier == 2)
        {
            mFeathers += 3; combat.RestorePlayerHealth(world, 100);
            Say(L"A KING'S HOARD", (coins + L". THREE EAGLE FEATHERS: YOUR BLADES BITE DEEPER. YOUR WOUNDS CLOSE.").c_str(), 6);
        }
        else if (c.Tier == 1)
        {
            ++mFeathers; combat.RestorePlayerHealth(world, 30);
            Say(L"OLD GOLD CHEST", (coins + L". AN EAGLE FEATHER: YOUR BLADES BITE DEEPER.").c_str(), 4.5f);
        }
        else if (mLore <= stage && mLore < 12)
        {
            Say((L"A MEMORY OF THE CAVE  " + std::to_wstring(mLore + 1) + L" / 12").c_str(), (std::wstring(Lore[mLore]) + L"  (" + coins + L")").c_str(), 9);
            ++mLore;
        }
        else switch ((hash >> 16) % 3)
        {
        case 0:
            combat.RestorePlayerHealth(world, MoonTearHeal);
            Say(L"OLD GOLD CHEST", (coins + L". A MOON TEAR: YOUR WOUNDS CLOSE.").c_str(), 4.5f); break;
        case 1:
            ++mFeathers;
            Say(L"OLD GOLD CHEST", (coins + L". AN EAGLE FEATHER: YOUR BLADES BITE DEEPER.").c_str(), 4.5f); break;
        default:
            mEmber = EmberSeconds; kit.ResetCooldowns();
            Say(L"OLD GOLD CHEST", (coins + L". A SOUL EMBER: YOUR SKILLS RETURN AND YOUR BLADES BURN.").c_str(), 4.5f); break;
        }
        if (!mCards.empty()) mCards.back().Toast = true;   // chest finds do not cover the fight
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
            if (mArenasCleared == last && mS > mArenas[last].S - mArenas[last].R * .5f && combat.PlayerActionsAllowed())
            {
                mFinale = Finale::Words; mFinaleTimer = 9.f;
                Say(L"FATHER AND DAUGHTER", L"THE EAGLE LIFTS HIS HEAD FROM THE WOUND. \"SEVEN YEARS HE HAS WHISPERED TO YOU. I HAVE HEARD EVERY WORD.\"", 5, true);
                Say(L"", L"\"I WAS SET HERE. I DO NOT CHOOSE. BUT I WILL NOT LET YOU PASS, DAUGHTER.\"", 4.5f);
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
                Say(L"THE EAGLE FALLS", L"HE FOLDS HIS WINGS ON THE LEDGE. \"YOU FLY AS I TAUGHT YOU,\" HE SAYS, AND IS STILL.", 7, true);
                Say(L"THE CHAIN", L"TWO IRON ANCHORS HOLD AMIRANI TO THE MOUNTAIN. CLIMB TO THE LEDGE AND BREAK THEM.", 8);
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
                Say(L"THE CHAIN OPENS", L"AMIRANI STANDS. THE MOUNTAIN IS SILENT FOR THE FIRST TIME IN AN AGE.", 8, true);
                Say(L"", L"HE LOOKS DOWN AT YOU AND SMILES. YOU CANNOT TELL WHAT KIND OF SMILE IT IS.", 8);
                Say(L"CAUCASIAN EAGLE", L"THE ROAD IS WALKED. THANK YOU FOR PLAYING.", 10, true);
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
            if (std::sqrt(p.x * p.x + p.z * p.z) < SanctuaryRadius && p.z < 93.5f)
                goal = std::abs(p.x) > 5 && p.z < 60 ? XMFLOAT3{0, 0, p.z} : XMFLOAT3{0, 0, 96};
            if (mFinale == Finale::Chains) goal = {MountainX, LedgeHeight, LedgeWalkBackZ - 4};
            session.ScriptedMove = 'W'; session.ScriptedFast = true;
            if (mCompleted) { goal = {MountainX, LedgeHeight, MountainZ}; session.ScriptedMove = 0; }   // the road is walked: stand and look at Amirani
        }
        world.MainCamera.Yaw = std::atan2(goal.x - p.x, goal.z - p.z);
        static int frames = 0; static float span = 0;
        ++frames; span += realDt;
        mPilotLog -= realDt;
        if (mPilotLog <= 0)
        {
            mPilotLog = 20;
            Log("[PILOT] t=%.0f s played, road %.0f m, stage %d, arenas %d, fighting %d wave %d/%d, enemies %d, gold %d, feathers %d, fps %.0f\n",
                mPlaySeconds, mS, mStage, mArenasCleared, mFighting ? 1 : 0, mWave, mWaves, AliveEncounterCount(), mGold, mFeathers,
                span > 0 ? static_cast<float>(frames) / span : 0.f);
            Log("[PILOT]   at (%.1f %.1f %.1f) target %d kind %d distance %.1f goal (%.1f %.1f) grounded %d\n", p.x, p.y, p.z, target,
                target >= 0 ? mMonsters[target].Kind : -1, best, goal.x, goal.z, world.MainPlayer.IsGrounded ? 1 : 0);
            frames = 0; span = 0;
        }
    }

    void Journey::Say(const wchar_t* title, const wchar_t* body, float seconds, bool big)
    {
        if (mCards.size() > 8) return;
        mCards.push_back({title, body, seconds, big});
    }

    // ================================================================ update
    void Journey::Update(DoorwaysGameHost& host, float dt, float realDt)
    {
        if (!mReady) return;
        auto& world = host.World; auto& combat = host.Combat;
        auto& kit = GetNyxaraKit();
        mTime += realDt;
        mPlaySeconds += dt;
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
        { if (mArenasCleared > 0) Teleport(world, mArenas[std::min(mArenasCleared, static_cast<int>(mArenas.size())) - 1].S); };
        if (!alive && !mWasDead)
        {
            mWasDead = true; ++mDeaths; undoFight();
            Log("[JOURNEY] death %d at %.0f m, %.0f s played (arenas won %d).\n", mDeaths, mS, mPlaySeconds, mArenasCleared);
        }
        else if (alive && mWasDead)
        {
            mWasDead = false; toCheckpoint();
            Log("[JOURNEY] respawned: back at %.0f m.\n", mS);
        }

        const float before = mS;
        if (!mShowcase) Leash(world);
        if (!mShowcase && alive && mS == 0 && before > 150 && mArenasCleared > 0)
        {   // she was far down the road and is suddenly back in the sanctuary: the engine respawned her there.
            // (Second net for the death handling above, in case the game is not updated while the death screen shows.)
            if (!mWasDead) ++mDeaths;
            mWasDead = false; undoFight(); toCheckpoint();
            Log("[JOURNEY] respawn seen in the sanctuary: back at %.0f m.\n", mS);
        }
        if (mStage != mShownStage && !mShowcase && mS > 1)
        {
            mShownStage = mStage;
            Say(StageCards[mStage].Title, StageCards[mStage].Body, 6.5f, true);
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
        }
        UpdateMonsters(world, combat, dt);

        // feathers and embers sharpen her blades; she mends between fights
        mEmber = std::max(0.f, mEmber - dt);
        kit.SetDamageScale((1 + FeatherDamageBonus * static_cast<float>(mFeathers)) * (mEmber > 0 ? EmberDamage : 1.f));
        if (!mFighting && alive)
        {
            mRegen += RegenPerSecond * dt;
            if (mRegen >= 1) { const int whole = static_cast<int>(mRegen); combat.RestorePlayerHealth(world, whole); mRegen -= static_cast<float>(whole); }
        }

        if (!mCards.empty())
        {
            mCardTime += realDt;
            if (mCardTime >= mCards.front().Seconds) { mCards.erase(mCards.begin()); mCardTime = 0; }
        }
        // hold F9 for two seconds: begin the journey again
        if (!GameSession().Unattended && (GetAsyncKeyState(VK_F9) & 0x8000))
        {
            mRestartHold += realDt;
            if (mRestartHold > 2)
            {
                mRestartHold = 0;
                for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i) Release(i);
                for (auto& c : mChests) { c.Opened = false; c.Open = 0; c.Slot = -1; }
                mArenasCleared = mGold = mFeathers = mLore = mDeaths = 0; mPlaySeconds = 0; mFighting = false; mCompleted = false;
                mFinale = Finale::None; mShownStage = -1; mPatrolNext = 0;
                GetAmiraniMountain().SetFreed(false); GetAmiraniMountain().SetEagleAway(false);
                auto& p = world.MainPlayer; p.Position = {0, 0, 0}; p.GroundY = 0; p.VerticalVelocity = 0;
                Say(L"THE ROAD BEGINS AGAIN", L"SEVEN YEARS LIE AHEAD OF YOU ONCE MORE.", 5, true);
                Save();
            }
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
            if (m.Windup >= 0) { const float t = 1 - m.Windup / def.Windup; pitch = -.32f * std::sin(t * Pi * .5f); lift += .04f * def.Height * t; }
            if (m.Lunge > 0) { const float t = m.Lunge / .28f; pitch = .45f * t; forward = .5f * def.Radius * t + .3f * t; }
            if (m.Hover < 0) pitch += .25f;
            const float hover = std::max(0.f, def.Hover) * (1 + .25f * std::sin(m.Phase * 2.2f)) + (m.Stage >= 100 ? 6.f * std::min(1.f, m.Special) : 0.f);
            const XMFLOAT3 at{feet.x + std::sin(e.Yaw) * forward, feet.y + lift + hover - (m.Dying ? (1 - fade) * def.Height * .3f : 0.f), feet.z + std::cos(e.Yaw) * forward};
            const float height = def.Height * (m.Dying ? .35f + .65f * fade : .25f + .75f * fade);
            const XMFLOAT4 tint = flash ? XMFLOAT4{3, 3, 3, 1} : XMFLOAT4{MonsterTint, MonsterTint, MonsterTint, 1};
            renderer.DrawDoorwaysGpuMesh(model.Mesh, Place(model.Min, model.Max, height, e.Yaw, at, pitch + (m.Dying ? (1 - fade) * -1.2f : 0.f), roll),
                                         viewProjection, Surface(model.Texture.Get(), tint, flash ? 1.f : MonsterEmission));
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

        // ---- translucent: gate barriers and lights, far to near
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
        for (const auto& c : mChests)
            if (!c.Opened && !(c.Arena >= mArenasCleared && c.Tier > 0) && LenXZ(Sub(c.P, eye)) < 70)
                glow(Add(c.P, {0, .8f, 0}), 2.2f + .3f * std::sin(mTime * 2.4f + c.P.z), {1.f, .78f, .3f, .38f});
        if (mArenasCleared < static_cast<int>(mArenas.size()) && mArenas[mArenasCleared].Kind != 2)
        {   // soul chains across the road: ahead always, behind while the fight lasts
            const Arena& a = mArenas[mArenasCleared];
            const XMFLOAT4 color = mFighting ? XMFLOAT4{1.f, .35f, .3f, .55f} : XMFLOAT4{.5f, .9f, 1.f, .40f};
            for (const float end : {1.f, -1.f})
            {
                if (end < 0 && !mFighting) continue;
                const Sample& s = mSamples[std::clamp(static_cast<int>((a.S + end * a.R * .97f) / mStep), 0, static_cast<int>(mSamples.size()) - 1)];
                if (LenXZ(Sub(s.P, eye)) > 150) continue;
                for (float l = -s.W; l <= s.W; l += 1.6f)
                {
                    const float h = 4.2f + .7f * std::sin(mTime * 2 + l);
                    card({s.P.x + s.Nx * l, s.P.y + h * .5f, s.P.z + s.Nz * l}, {s.Nx * 1.1f, 0, s.Nz * 1.1f}, {0, h, 0}, color);
                }
                for (const float h : {1.0f, 2.4f})
                    card({s.P.x, s.P.y + h, s.P.z}, {s.Nx * 2.2f * s.W, 0, s.Nz * 2.2f * s.W}, {0, .9f, 0}, color);
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
    void Journey::DrawHud(const DoorwaysHudRenderContext& context, HudTextRenderer& textRenderer)
    {
        if (!mReady) return;
        const int cw = context.ClientWidth, ch = context.ClientHeight;
        const float W = static_cast<float>(cw), H = static_cast<float>(ch);
        const float s = CalculateDoorwaysUiScale({cw, ch, context.Dpi, context.Interface.UiScale});
        const auto mat = [&](float r, float g, float b, float a = 1.f) -> Material
        { return {{r, g, b, a}, {1, 1, 0, 0}, context.Renderer.mWhiteTextureView.Get(), {0, 0, 0, 0}, 1, 1}; };
        const Material panel = mat(.035f, .070f, .062f, .86f), edge = mat(.16f, .30f, .26f), ink = mat(.90f, .97f, .95f), dim = mat(.52f, .64f, .62f),
            gold = mat(.92f, .76f, .38f), dark = mat(.012f, .028f, .024f, .92f), shadow = mat(0, 0, 0, .85f), red = mat(.95f, .30f, .32f),
            soul = mat(.22f, 1.f, .52f), veil = mat(0, 0, 0, .55f);
        const auto rect = [&](float x, float y, float w, float h, const Material& m)
        { if (w > 0 && h > 0) context.Renderer.DrawScreenRect(x, y, w, h, cw, ch, m); };
        const auto text = [&](const std::wstring& value, float x, float y, float pixels, const Material& m)
        { textRenderer.DrawTextWithShadow(context.Renderer, value.c_str(), x, y, pixels, cw, ch, m, shadow); };
        const auto width = [&](const std::wstring& value, float pixels) { return MeasureDoorwaysHudTextWidth(value.c_str(), pixels); };
        const auto centered = [&](const std::wstring& value, float cx, float y, float pixels, const Material& m)
        { text(value, cx - width(value, pixels) * .5f, y, pixels, m); };
        const auto wrap = [&](const std::wstring& value, float pixels, float maxWidth)
        {
            std::vector<std::wstring> lines; std::wstring line, word;
            std::wistringstream in(value);
            while (in >> word)
            {
                const std::wstring candidate = line.empty() ? word : line + L" " + word;
                if (!line.empty() && width(candidate, pixels) > maxWidth) { lines.push_back(line); line = word; }
                else line = candidate;
            }
            if (!line.empty()) lines.push_back(line);
            return lines;
        };
        const float m = 20 * s, tiny = 2.f * s, body = 2.5f * s;

        // ---- road panel (top left, under the key hints)
        {
            const float x = m, y = m + 22 * s, w = 412 * s, h = 74 * s, pad = 9 * s;
            rect(x - 1.5f * s, y - 1.5f * s, w + 3 * s, h + 3 * s, edge); rect(x, y, w, h, panel);
            rect(x, y, 4 * s, h, gold);
            const std::wstring stage = mS <= 1 && mArenasCleared == 0 ? L"MOONLIT SANCTUARY" : mStageNames[mStage];
            text(stage, x + pad + 4 * s, y + pad * .8f, body, ink);
            const float total = static_cast<float>(mSamples.size() - 1) * mStep;
            const float f = mCompleted ? 1.f : std::clamp(mS / total, 0.f, 1.f);
            const float bx = x + pad + 4 * s, bw = w - 2 * pad - 4 * s, by = y + pad + DoorwaysHudTextHeight(body) + 5 * s, bh = 8 * s;
            rect(bx, by, bw, bh, dark); rect(bx, by, bw * f, bh, gold);
            for (int i = 1; i < 12; ++i) rect(bx + bw * static_cast<float>(i) / 12.f, by, 1.5f * s, bh, dark);
            std::wostringstream way, line;
            way << static_cast<int>(mS) << L" / " << static_cast<int>(total) << L" M";
            text(way.str(), bx + bw - width(way.str(), tiny), y + pad, tiny, dim);
            line << L"GOLD " << mGold << L"    FEATHERS " << mFeathers << L"    MEMORIES " << mLore << L"/12";
            text(line.str(), bx, by + bh + 5 * s, tiny, dim);
        }
        // ---- objective (under the road panel)
        {
            std::wstring objective;
            if (mFinale == Finale::Chains) objective = L"BREAK THE TWO CHAIN ANCHORS ON THE LEDGE";
            else if (mFinale == Finale::Freed || mFinale == Finale::Done) objective = L"AMIRANI IS FREE.  HOLD F9 TO WALK THE ROAD AGAIN";
            else if (mFinale == Finale::Fight || mFinale == Finale::Words) objective = L"DEFEAT THE EAGLE";
            else if (mFighting) objective = L"WAVE " + std::to_wstring(std::max(1, mWave)) + L" / " + std::to_wstring(mWaves) + L"   ENEMIES " + std::to_wstring(AliveEncounterCount() + static_cast<int>(mQueue.size()));
            else if (mS <= 1 && mArenasCleared == 0) objective = L"LEAVE BY THE GATE THAT FACES THE MOUNTAIN";
            else objective = L"FOLLOW THE ROAD TO THE MOUNTAIN";
            text(objective, m, m + 22 * s + 74 * s + 8 * s, tiny, mFighting ? red : gold);
            if (mEmber > 0) text(L"SOUL EMBER " + std::to_wstring(static_cast<int>(mEmber) + 1) + L" S", m, m + 22 * s + 74 * s + 8 * s + DoorwaysHudTextHeight(tiny) + 5 * s, tiny, soul);
        }
        // ---- boss bar (top, right of the road panel)
        const auto& enemies = GetNyxaraKit().Enemies();
        for (int i = 0; i < static_cast<int>(mMonsters.size()); ++i)
        {
            const Monster& monster = mMonsters[i];
            if (!monster.Active || monster.Dying || !(Kinds[monster.Kind].Boss || monster.Kind == ChainKind)) continue;
            if (monster.Kind == ChainKind && i > 0 && mMonsters[i - 1].Active && mMonsters[i - 1].Kind == ChainKind) continue;
            const float bx = 470 * s, bw = W - bx - 30 * s, bh = 12 * s, by = 72 * s;   // right of the road panel, above the move banner
            const float f = std::clamp(static_cast<float>(enemies[i].Health) / static_cast<float>(enemies[i].MaxHealth), 0.f, 1.f);
            text(Kinds[monster.Kind].Name, bx, by - DoorwaysHudTextHeight(body) - 5 * s, body, gold);
            rect(bx - 2 * s, by - 2 * s, bw + 4 * s, bh + 4 * s, shadow); rect(bx, by, bw, bh, dark);
            rect(bx, by, bw * f, bh, enemies[i].Flash > 0 ? ink : red);
            break;
        }
        // ---- story card
        if (!mCards.empty())
        {
            const Card& card = mCards.front();
            const float fade = std::min({1.f, mCardTime / .4f, (card.Seconds - mCardTime) / .5f});
            if (fade > .05f)
            {
                const bool toast = card.Toast;
                const float pixels = toast ? 2.5f * s : 3.f * s, maxWidth = toast ? 360 * s : std::min(W * .62f, 900 * s);
                const float side = toast ? 16 * s : 30 * s;
                const auto lines = wrap(card.Body, pixels, maxWidth);
                const float lineHeight = DoorwaysHudTextHeight(pixels) + 6 * s;
                const float titlePixels = card.Big ? 6.f * s : (toast ? 2.5f * s : 3.5f * s);
                const float titleHeight = card.Title.empty() ? 0 : DoorwaysHudTextHeight(titlePixels) + 12 * s;
                const float h = titleHeight + lineHeight * static_cast<float>(lines.size()) + 26 * s;
                const float cx = toast ? W - 30 * s - side - maxWidth * .5f : W * .5f;      // toast: right edge, under the boss bar
                const float y = toast ? 150 * s : (card.Big ? H * .26f : H * .225f);
                rect(cx - maxWidth * .5f - side, y, maxWidth + 2 * side, h, veil);
                rect(cx - maxWidth * .5f - side, y, maxWidth + 2 * side, 2 * s, gold);
                rect(cx - maxWidth * .5f - side, y + h - 2 * s, maxWidth + 2 * side, 2 * s, gold);
                float ty = y + 13 * s;
                if (!card.Title.empty()) { centered(card.Title, cx, ty, titlePixels, gold); ty += titleHeight; }
                for (const auto& l : lines) { centered(l, cx, ty, pixels, ink); ty += lineHeight; }
            }
        }
        if (mRestartHold > .2f) centered(L"HOLD F9 TO BEGIN THE JOURNEY AGAIN", W * .5f, H * .45f, 3 * s, red);
    }
}
