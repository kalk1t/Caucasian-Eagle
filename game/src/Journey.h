#pragma once

// The road to Amirani: the journey from the first stone of the road to the mountain.
//
// Owns everything that happens on the road:
//   - the road itself (assets/Compiled/World/Road.txt, written by art/world/build_world.py):
//     path samples, stages, arenas, chests, scenery props and lights
//   - the monsters of Caucasus folklore (static Meshy models moved by code), their waves and bosses
//   - old gold chests and what is inside them
//   - gates, checkpoints, the save file, the finale and the journey HUD
//   - the story journal (F1): which regions, memories and endings are unlocked; the words are in Story.cpp
//   - the seven cities (assets/Compiled/World/Cities.txt, written by art/cities/build_cities.py): their
//     buildings, which of them she has entered and what each gave her; the words are in Story.cpp
//   - what the minimap and the world map (M) show; they are drawn by WorldMap.cpp
//
// Nyxara's attacks stay in NyxaraKit: every monster and chest is one of the kit's enemy slots, so
// all her slashes, skills and combos hit them without the kit knowing what they are.
// Numbers are in GameConstants.h ("journey") and in the tables at the top of Journey.cpp.

#include "Collection.h"
#include "CreatureRig.h"
#include "Growth.h"
#include "GameUi.h"
#include "NyxaraKit.h"
#include "Story.h"
#include "WorldMap.h"
#include "DoorwaysGame.h"
#include "GameConstants.h"
#include "Rendering/DoorwaysGpuMesh.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <string>
#include <unordered_map>
#include <vector>

class Renderer;
class CombatSystem;
struct WorldState;
struct EnvironmentSettings;

namespace CaucasianEagle
{
    class Journey
    {
    public:
        bool Initialize(Renderer& renderer, WorldState& world, std::string& error);
        // Isolated test fixture. Uses the campaign roster, AI and creature rigs;
        // never loads or updates the journey, loot, saves or encounters.
        struct CreatureStation
        {
            std::wstring Name;
            DirectX::XMFLOAT3 Anchor{};
            float Height=0, Radius=0, Windup=0, Strike=0;
            int Health=0, MaximumHealth=0, Shots=0;
        };
        bool InitializeCreatureStations(Renderer& renderer, std::string& error);
        void ResetCreatureStations();
        void UpdateCreatureStations(WorldState& world, CombatSystem& combat, float dt, bool attacks=true);
        void DrawCreatureStations(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection);
        std::vector<CreatureStation> CreatureStations() const;
        bool CreatureStationAnchorsValid() const;
        void Update(Doorways::DoorwaysGameHost& host, float deltaTime, float realDeltaTime, bool inputAllowed);
        void Draw(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection);
        void DrawHud(Ui& ui, const WorldState& world);
        // Fog, ambient and moonlight of the stage Nyxara is in (after AmiraniMountain::StyleEnvironment).
        void StyleEnvironment(EnvironmentSettings& environment) const;
        // > 1 only in the unattended test pilot; the road stands still while the journal or the map is open
        DirectX::XMFLOAT3 StartPoint() const;   // where a new journey begins: on the road, before its first gate
        bool HasProgress() const { return mArenasCleared > 0 || mGold > 0; }   // a journey is under way (the start screen says "Continue")
        float TimeScale() const { return mJournalOpen || mMapOpen || mGrowthOpen || mPauseOpen || mGuideOpen ? JournalTimeScale : mTimeScale; }

    private:
        bool mCreatureStations = false;
        struct Sample { DirectX::XMFLOAT3 P{}; float W = 0; int Stage = 0; float Tx = 0, Tz = 1, Nx = 1, Nz = 0; };
        struct Arena { int Stage = 0; float S = 0; int Kind = 0; float R = 0; DirectX::XMFLOAT3 P{}; };
        struct Chest { DirectX::XMFLOAT3 P{}; float Yaw = 0; int Tier = 0; int Arena = -1; bool Opened = false; int Slot = -1; float Open = 0; };
        struct Prop { int Type = 0; DirectX::XMFLOAT3 P{}; float Yaw = 0, Height = 1; };
        struct Light { DirectX::XMFLOAT3 P{}; DirectX::XMFLOAT4 Color{}; float Size = 1; };
        struct City { float S0 = 0, S1 = 0; DirectX::XMFLOAT3 P{}; bool Visited = false; };   // S0..S1: metres of road inside its gates
        struct Building { int Model = 0; DirectX::XMFLOAT3 P{}; float Yaw = 0, Height = 1, Far = CityDrawDistance; };   // Far: m it is drawn from
        // A place beside the road (assets/Compiled/World/Places.txt, written by art/places/build_places.py): the things
        // that stand there are Buildings; she finds it by coming within R of P. Its words are Story::Places[index].
        struct Site { DirectX::XMFLOAT3 P{}; float S = 0, R = 20; bool Found = false; };
        struct Model
        {
            Doorways::DoorwaysGpuMesh Mesh;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture;
            DirectX::XMFLOAT3 Min{}, Max{};
            std::vector<float> Top;              // buildings only: the highest surface of the mesh on a RoofGrid x RoofGrid map of its footprint (model space; -1e9 = none)
            CreatureRig Rig;                     // creatures only: the joints written by art/monsters/rig_models.py
        };
        struct Monster
        {
            int Kind = -1;
            bool Active = false, Dying = false, Encounter = false;
            float Cooldown = 1, Windup = -1, Lunge = 0, Shoot = 2, Special = 4, Phase = 0, Spawn = 0, Death = 0;
            float Ground = 0, Strafe = 1, Moving = 0, Hover = 0;
            float WindupFull = 1;   // length of the wind-up now running (a heavy blow's is longer)
            int Blow = 0;           // the blow being wound up: 0 light, 1 heavy, 2 the quick second blow
            int Shots = 0;          // volleys fired (every few are a different pattern)
            float Slam = 0, SlamWait = 3;   // giants: seconds until the marked ground is struck; until the next mark
            float Dodge = 0, Hop = 0, Round = 0;   // until it may dodge again; jump running; running round her guard
            float Air = 0;          // m its feet are over the ground (spirits and witches that follow her up; a leap)
            float Leap = 0, LeapWait = 2, LeapHigh = 0, LeapSpeed = 0;   // s left of a leap at her; until the next; its height; its speed over the ground
            bool LeapHit = false;   // this leap has struck her
            DirectX::XMFLOAT3 HopDir{};
            int Index = 0;          // nearest road sample
            int Chest = -1;         // chest index when this slot is a chest
            int Stage = 0;          // 0.. : boss phase counter
            DirectX::XMFLOAT3 Aim{}; // dive / slam target
        };
        struct Banner { std::wstring Label, Title, Line; float Seconds = 5; };   // small title at the top of the screen
        struct Toast                                                              // small panel at the right edge
        {
            const HudArt::Sprite* Art = nullptr; Color Tint; std::wstring Title, Text;
            float Seconds = 5, Age = 0, Y = -1;
        };

        bool LoadRoad(std::string& error);
        bool LoadCities(Renderer& renderer, std::string& error);
        bool LoadPlaces(Renderer& renderer, std::string& error);
        void UpdatePlaces(WorldState& world);
        unsigned long long PlaceBits() const;   // bit per place she has found
        // The texture is 'sharedTexture' when given, else <textureBase or base>BaseColor.png
        bool LoadModel(Renderer& renderer, const std::string& base, Model& out, std::string& error, ID3D11ShaderResourceView* sharedTexture = nullptr,
                       const std::string& textureBase = {});
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
        void Announce(const std::wstring& label, const std::wstring& title, const std::wstring& line, float seconds);
        void Notify(const HudArt::Sprite& icon, Color tint, const std::wstring& title, const std::wstring& text, float seconds);
        void ReachEnd(int part);
        void Restart(WorldState& world, CombatSystem& combat);
        void UpdateJournal(float realDeltaTime, bool inputAllowed);
        void UpdateCities(WorldState& world, CombatSystem& combat);
        bool Gift(int city) const { return city < static_cast<int>(mCities.size()) && mCities[city].Visited; }   // she has what that city gives
        float DamageTaken() const;         // scale of every monster's damage
        float CageEnd(const Arena& arena, float end) const;   // metres along the road where a fight is closed: end = -1 behind, +1 ahead
        int PotionHealth() const;          // what one potion gives now
        unsigned CityBits() const;         // bit per city she has entered
        int CitiesVisited() const;
        void PlaceFatherFeathers();        // one hidden at the road's edge in each region
        void UpdateFatherFeathers(WorldState& world);
        void MarkCitiesBehind();           // a journey that starts past a city has been through it
        void FillMapView(const WorldState& world);
        void Save() const;
        void Load();

        bool mReady = false;
        std::vector<Sample> mSamples;
        std::vector<std::wstring> mStageNames;
        std::vector<Arena> mArenas;
        std::vector<Chest> mChests;
        std::vector<Prop> mProps;
        // what she walks round off the road: boulders, props, gate piers (Road.txt "O"), in 16 m squares
        struct Obstacle { float X = 0, Z = 0, R = 0; bool House = false; };   // House: a building, which she may stand on (TopAt)
        std::vector<Obstacle> mObstacles;
        std::unordered_map<long long, std::vector<int>> mObstacleGrid;
        void AddObstacle(float x, float z, float radius, bool house = false);
        float TopAt(float x, float z) const;   // the highest surface of a city building over (x, z), world height; -1e9 where there is none
        float mLateral = 0;                // m to the right of the road's middle, from Leash
        bool mPilotHold = false;           // testing: the pilot stands still (TroubleEnvironmentVariable)
        bool mPastWall = false;            // she went round the wall of an unwon arena: it does not pull her back
        // trouble on the road between the fights: 0 cursed ground, 1 falling stones, 2 wisps
        struct Trouble { int Kind = 0; DirectX::XMFLOAT3 P{}; float R = 0, Tx = 0, Tz = 1, Nx = 1, Nz = 0, Clock = 0, Phase = 0, Next = 0, S = 0; };
        struct Strike { DirectX::XMFLOAT3 P{}; float T = 0; int Trap = -1; float Drop = 18; };   // Trap: the seal trap that sent it; Drop: m it falls
        std::vector<Trouble> mTroubles;
        std::vector<Strike> mStrikes;      // stones on their way down
        int mTroubleNear = -1;             // the one she is coming to (the HUD names it)
        void BuildTroubles();
        void UpdateTroubles(WorldState& world, CombatSystem& combat, float dt);
        DirectX::XMFLOAT3 WispAt(const Trouble& trouble, int which) const;
        // Traps (BuildTraps; the kinds and where they stand are in the table at the top of Journey.cpp). They are made
        // for a traveller on the wing: each reaches past her ceiling. One that is beaten stays beaten (the save keeps it).
        struct Trap
        {
            int Kind = 0;
            float S = 0, Length = 0;                     // m along the road of its middle (the gate run: of its switch); m of road it takes
            DirectX::XMFLOAT3 P{}, End{};                // on the road at S; the gate run: where the gate stands
            float Tx = 0, Tz = 1, Nx = 1, Nz = 0, W = 6; // the road's direction there, and half the width of the way through
            float Ex = 0, Ez = 1;                        // the gate run: the road's direction at the gate
            bool Won = false, In = false, Entered = false;   // In: she is in it now; Entered: she came in at one of its ends
            int State = 0, Count = 0, Slot = -1, Look = 0;   // gate run: 1 while a run lasts; hits on the seal / the last gust; the press: its link's enemy slot; which statue
            float Clock = 0, Timer = 0, Full = 1, Next = 0, Side = 0;
            DirectX::XMFLOAT3 Post[3]{};                 // its stones or statues
            float Fire[3] = {};                          // rune stones: s until each throws; watchers: where each sheet began
        };
        std::vector<Trap> mTraps;
        int mTrapNear = -1;                // the one she is at (the HUD names it)
        void BuildTraps();
        void UpdateTraps(WorldState& world, CombatSystem& combat, float dt);
        void WinTrap(Trap& trap, const wchar_t* name, const wchar_t* line);
        unsigned TrapBits() const;         // bit per trap she has beaten
        int Stand(const char* model, DirectX::XMFLOAT3 at, float yaw, float height, float seenFrom);   // one more thing of the places' models; -1: no such model
        std::vector<int> mArenaRows;       // the kind of every "A" row of Road.txt in file order (3 = an open clearing, a fight in older versions)
        // the way: shown when she is off the road, or has gone past a fight that still waits
        void UpdateWay(const WorldState& world, float realDt);
        float mWayShow = 0, mWayDistance = 0;
        bool mWayBack = false;             // the fight she must win is behind her
        DirectX::XMFLOAT3 mWayTarget{};
        std::vector<Light> mLights;
        float mStep = 2;
        std::vector<Model> mModels;        // one per monster kind (shared models repeat)
        std::vector<Model> mPropModels;
        std::vector<Monster> mMonsters;    // parallel to NyxaraKit::Enemies()

        std::vector<City> mCities;         // in road order; the words of city i are Story::Cities[i]
        std::vector<Building> mBuildings;
        std::vector<Model> mCityModels;
        std::vector<Site> mSites;          // the places beside the road, in road order
        std::unordered_map<std::string, int> mPlaceModel;   // "Archway" -> its index in mCityModels (the traps stand some of them too)
        struct Glade { float X = 0, Z = 0, R = 0; };
        std::vector<Glade> mGlades;        // nothing grows where a place stands, and the world's own scenery is taken away
        int mCity = -1;                    // the city she is in, or -1
        bool mCitiesInSave = false;        // the save file said which cities she has entered

        // progress
        int mIndex = 0;                    // nearest road sample to Nyxara
        float mS = 0;                      // metres along the road
        int mStage = 0, mShownStage = -1;
        int mArenasCleared = 0;
        int mGold = 0, mFeathers = 0, mLore = 0, mDeaths = 0;
        float mEmber = 0;                  // seconds of Soul Ember left
        bool mWasDead = false, mPendingTeleport = false;
        // The save point: where she stood when the game was last saved outside a fight. She wakes there after a death
        // and at the start of a session. (Saves made during a fight keep the point they had.)
        mutable DirectX::XMFLOAT3 mSavePoint{}; mutable bool mHasSavePoint = false;
        DirectX::XMFLOAT3 mHere{};       // where she is this frame
        bool mAlive = true, mSaveKey = false;
        float mDeadFor = 0, mRespawnAsk = 0;
        bool CanMark() const;            // the save point may be moved to where she is
        void WakeAtSavePoint(WorldState& world);
        float mPlaySeconds = 0;

        // encounter
        bool mFighting = false;
        int mWave = 0, mWaves = 0;
        float mWaveDelay = 0, mSpawnTimer = 0;
        std::vector<int> mQueue;
        DirectX::XMFLOAT3 mPlayerLast{}, mPlayerVelocity{};   // where she was last frame; her smoothed velocity (creatures lead her)
        int mWallArena = -1;               // the arena whose walls stand (or are still sinking)
        float mWall = 0;                   // 0 in the ground .. 1 fully risen
        std::vector<float> mPatrols;       // road distances where a roaming pack waits (between the arenas)
        int mPatrolNext = 0;

        // finale
        enum class Finale { None, Words, Fight, Chains, Freed, Done };
        Finale mFinale = Finale::None;
        float mFinaleTimer = 0;
        bool mCompleted = false;

        // presentation
        std::vector<Banner> mBanners;
        float mBannerTime = 0;
        std::vector<Toast> mToasts;
        int mBossSlot = -1; float mBossTrail = 1;   // the guardian bar's trail of lost health

        // story journal (F1)
        Story::Journal mJournal;
        bool mJournalOpen = false;
        int mJournalForced = 0;            // capture mode: page + 1 held open (JournalEnvironmentVariable)
        bool mJournalKeys[JournalKeyCount] = {};
        std::vector<Doorways::DoorwaysInputAction> mSuppressedInPlay; // the session's list while the journal is closed
        float mTime = 0;
        bool mFlying = false, mLanding = false, mLandAsk = false;   // free flight: in the air; R is bringing her down; a test asks to land
        bool mOnFoot = false, mModeKey = false;   // R: on foot (walks, Space jumps) or on the wing (the camera aims her); R last frame
        float mWingSpeed = 0;            // her travel speed on the wing, kept while she is on foot
        float mLandTime = 0;             // s left of the landing
        float mRageAt = -100;            // mTime of her last cry of fury on a kill
        float mEnv[12] = {};               // blended stage weights
        bool mMapOpen = false, mMapForced = false;   // world map (M); forced: capture mode (MapEnvironmentVariable)
        float mMapFade = 0;
        std::vector<WorldMap::Point> mRoadPoints;
        WorldMap::View mMapView;
        // growth (Growth.h): crystals, evolutions, training, potions; her window (I)
        Growth::State mGrowth;
        bool mGrowthOpen = false, mGrowthForced = false;   // forced: capture mode (GrowthEnvironmentVariable)
        float mGrowthFade = 0, mGrowthFlash = 0;
        bool mPauseOpen = false;           // her own menu (Esc)
        int mPauseRow = 0;
        float mPauseFade = 0;
        void DrawPause(Ui& ui) const;
        bool mMouseWas = false, mKitRelease = false;   // the left button last frame; her kit waits for it to be let go after a window closes
        float mMouseX = -1, mMouseY = -1;  // where the pointer was last frame (hovering chooses a row only when it moves)
        bool mPlaced = false;              // she has been put at the start of the road (a new journey)
        bool mUiFocused = false;           // the game window had the keys at the last update (her settings window reads its own)
        bool mPauseForced = false;         // capture mode (PauseEnvironmentVariable)
        bool mGuideOpen = false;           // the guide (Guide.cpp): from her menu, or from the start screen
        bool mGuideFromStart = false;      // opened from the start screen: closing it goes back there
        int mGuidePage = 0, mGuideForced = 0;   // forced: capture mode (GuideEnvironmentVariable), page + 1
        float mGuideFade = 0;
        int mGrowthRow = 0, mGrowthFlashRow = -1;
        int mFormShown = 0;                // the form she was last announced in
        // what she collects (Collection.h): charms, chain links, her father's feathers, runes; page 2 of her window
        Collection::State mCollection;
        int mGrowthPage = 0;               // 0 grow, 1 what she carries
        int mRuneRow = 0;
        std::vector<DirectX::XMFLOAT3> mFatherFeathers;   // where each region's feather lies (index = region)
        bool mPotionKey = false;
        float mRespawnSeen = -10;        // mTime when she was last found back at the start of the road after a death
        DirectX::XMFLOAT3 mSoundAt{};   // where she stood last frame (footsteps)
        bool mSoundAir = false;          // she was in the air last frame (the wing beat of taking off)
        float mRestartHold = 0;            // seconds F9 has been held
        float mRestartTest = 0;            // testing: an unattended run holds F9 from this second (RestartEnvironmentVariable)

        // unattended test pilot
        bool mPilot = false, mShowcase = false;
        float mTimeScale = 1, mPilotClock = 0, mPilotLog = 0;
        Doorways::NyxaraKit::Keys mPilotKeys;
        unsigned mSeed = 0xC0FFEEu;
        float Random();
    };

    Journey& GetJourney();
}
