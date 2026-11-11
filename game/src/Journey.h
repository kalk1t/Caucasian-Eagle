#pragma once

// The road to Amirani: the journey from the Moonlit Sanctuary to the mountain.
//
// Owns everything that happens on the road:
//   - the road itself (assets/Compiled/World/Road.txt, written by art/world/build_world.py):
//     path samples, stages, arenas, chests, scenery props and lights
//   - the monsters of Caucasus folklore (static Meshy models moved by code), their waves and bosses
//   - old gold chests and what is inside them
//   - gates, checkpoints, the save file, the story cards, the finale and the journey HUD
//
// Nyxara's attacks stay in NyxaraKit: every monster and chest is one of the kit's enemy slots, so
// all her slashes, skills and combos hit them without the kit knowing what they are.
// Numbers are in GameConstants.h ("journey") and in the tables at the top of Journey.cpp.

#include "NyxaraKit.h"
#include "Rendering/DoorwaysGpuMesh.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <string>
#include <vector>

class Renderer;
class CombatSystem;
class HudTextRenderer;
struct WorldState;
struct DoorwaysHudRenderContext;
struct EnvironmentSettings;
namespace Doorways { struct DoorwaysGameHost; }

namespace CaucasianEagle
{
    class Journey
    {
    public:
        bool Initialize(Renderer& renderer, WorldState& world, std::string& error);
        void Update(Doorways::DoorwaysGameHost& host, float deltaTime, float realDeltaTime);
        void Draw(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection);
        void DrawHud(const DoorwaysHudRenderContext& context, HudTextRenderer& text);
        // Fog, ambient and moonlight of the stage Nyxara is in (after AmiraniMountain::StyleEnvironment).
        void StyleEnvironment(EnvironmentSettings& environment) const;
        float TimeScale() const { return mTimeScale; } // > 1 only in the unattended test pilot

    private:
        struct Sample { DirectX::XMFLOAT3 P{}; float W = 0; int Stage = 0; float Tx = 0, Tz = 1, Nx = 1, Nz = 0; };
        struct Arena { int Stage = 0; float S = 0; int Kind = 0; float R = 0; DirectX::XMFLOAT3 P{}; };
        struct Chest { DirectX::XMFLOAT3 P{}; float Yaw = 0; int Tier = 0; int Arena = -1; bool Opened = false; int Slot = -1; float Open = 0; };
        struct Prop { int Type = 0; DirectX::XMFLOAT3 P{}; float Yaw = 0, Height = 1; };
        struct Light { DirectX::XMFLOAT3 P{}; DirectX::XMFLOAT4 Color{}; float Size = 1; };
        struct Model
        {
            Doorways::DoorwaysGpuMesh Mesh;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture;
            DirectX::XMFLOAT3 Min{}, Max{};
        };
        struct Monster
        {
            int Kind = -1;
            bool Active = false, Dying = false, Encounter = false;
            float Cooldown = 1, Windup = -1, Lunge = 0, Shoot = 2, Special = 4, Phase = 0, Spawn = 0, Death = 0;
            float Ground = 0, Strafe = 1, Moving = 0, Hover = 0;
            int Index = 0;          // nearest road sample
            int Chest = -1;         // chest index when this slot is a chest
            int Stage = 0;          // 0.. : boss phase counter
            DirectX::XMFLOAT3 Aim{}; // dive / slam target
        };
        struct Card { std::wstring Title, Body; float Seconds = 5; bool Big = false; bool Toast = false; }; // Toast: small panel at the right edge

        bool LoadRoad(std::string& error);
        bool LoadModel(Renderer& renderer, const std::string& base, Model& out, std::string& error);
        int Nearest(float x, float z, int hint, int window) const;
        float GroundAt(float x, float z, int& index) const;
        void Leash(WorldState& world);
        void Teleport(WorldState& world, float s);

        int Spawn(int kind, DirectX::XMFLOAT3 at, bool encounter, float healthScale);
        void Release(int slot);
        int AliveEncounterCount() const;
        void StartEncounter(int arena);
        void UpdateEncounter(WorldState& world, CombatSystem& combat, float dt);
        void UpdatePatrols(WorldState& world, CombatSystem& combat);
        void BuildWave(int arena, int wave, std::vector<int>& out) const;
        void ClearArena(WorldState& world, CombatSystem& combat);
        void UpdateMonsters(WorldState& world, CombatSystem& combat, float dt);
        void UpdateBoss(int slot, WorldState& world, CombatSystem& combat, float dt, float distance, DirectX::XMFLOAT3 toPlayer);
        void OnDeath(int slot, WorldState& world, CombatSystem& combat);
        void UpdateChests(WorldState& world);
        void OpenChest(int chest, WorldState& world, CombatSystem& combat);
        void UpdateFinale(WorldState& world, CombatSystem& combat, float dt);
        void UpdatePilot(WorldState& world, CombatSystem& combat, float realDt);
        void Say(const wchar_t* title, const wchar_t* body, float seconds, bool big = false);
        void Save() const;
        void Load();

        bool mReady = false;
        std::vector<Sample> mSamples;
        std::vector<std::wstring> mStageNames;
        std::vector<Arena> mArenas;
        std::vector<Chest> mChests;
        std::vector<Prop> mProps;
        std::vector<Light> mLights;
        float mStep = 2;
        std::vector<Model> mModels;        // one per monster kind (shared models repeat)
        std::vector<Model> mPropModels;
        std::vector<Monster> mMonsters;    // parallel to NyxaraKit::Enemies()

        // progress
        int mIndex = 0;                    // nearest road sample to Nyxara
        float mS = 0;                      // metres along the road
        int mStage = 0, mShownStage = -1;
        int mArenasCleared = 0;
        int mGold = 0, mFeathers = 0, mLore = 0, mDeaths = 0;
        float mEmber = 0;                  // seconds of Soul Ember left
        float mRegen = 0;
        bool mWasDead = false, mPendingTeleport = false;
        float mPlaySeconds = 0;

        // encounter
        bool mFighting = false;
        int mWave = 0, mWaves = 0;
        float mWaveDelay = 0, mSpawnTimer = 0;
        std::vector<int> mQueue;
        std::vector<float> mPatrols;       // road distances where a roaming pack waits (between the arenas)
        int mPatrolNext = 0;

        // finale
        enum class Finale { None, Words, Fight, Chains, Freed, Done };
        Finale mFinale = Finale::None;
        float mFinaleTimer = 0;
        bool mCompleted = false;

        // presentation
        std::vector<Card> mCards;
        float mCardTime = 0;
        float mTime = 0;
        float mEnv[12] = {};               // blended stage weights
        float mRestartHold = 0;

        // unattended test pilot
        bool mPilot = false, mShowcase = false;
        float mTimeScale = 1, mPilotClock = 0, mPilotLog = 0;
        Doorways::NyxaraKit::Keys mPilotKeys;
        unsigned mSeed = 0xC0FFEEu;
        float Random();
    };

    Journey& GetJourney();
}
