#pragma once

#include "DoorwaysGame.h"
#include "EaglesAscent.h"
#include "Journey.h"
#include "MountainView.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// The Eagle's Ascent: Map A on its own, another way into the game than the road (CaucasianEagle.exe --eagles-ascent).
// Nyxara climbs from the sea to Amiran's court at 4,200 m, on foot and in flight, with the road's controls, start screen,
// settings, sound and HUD. The map's rules are EaglesAscent (EaglesAscent.h), the same code Map Lab tests.
// Since 2026-11-20 (the owner's map plan) it has the road's creatures, in small groups along the way and in caves beside
// it, never more than three alive at once, and four bosses: the Gveleshapi, the Giant, the Devi Lord and the Eagle in
// Amiran's court (Journey::UpdateMountain; where they are: art/map/ascent_creatures.py). Nine Eagle Nests are the only
// places she is saved: the last nest she rested at and every creature she has beaten (a beaten one stays beaten). She
// wakes at the last nest when she falls.
namespace CaucasianEagle
{
    // The switch or the environment variable asks for the Eagle's Ascent (main.cpp registers the module).
    bool EaglesAscentRequested();

    class EaglesAscentGame final : public Doorways::IDoorwaysGame
    {
    public:
        void Configure(Doorways::DoorwaysGameSession& session) override;
        bool Prepare(Doorways::DoorwaysGameHost& host, std::string& error) override;
        float ScaleFrameTime(float realDeltaTime, bool inputAllowed) override;
        void Update(Doorways::DoorwaysGameHost& host, float deltaTime, float realDeltaTime, bool inputAllowed) override;
        void Draw(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection) override;
        void AdjustEnvironment(EnvironmentSettings& environment) override;
        bool DrawHud(const DoorwaysHudRenderContext& context, HudTextRenderer& text) override;
        void DrawStartScreen(const DoorwaysHudRenderContext& context, HudTextRenderer& text, bool focused) override;

    private:
        bool Press(int key, bool allowed);
        void Notify(const std::wstring& title, const std::wstring& line, float seconds);
        void Smoke(Doorways::DoorwaysGameHost& host, float realDeltaTime);
        void Climb(Doorways::DoorwaysGameHost& host, float dt);
        void Evidence(const WorldState& world, const char* event);
        void StartCreatures();                     // the placing model's creatures on their lines (Journey::SetMountainLines)
        void LoadSave();                           // the last nest and the beaten (AscentSaveFileName)
        void WriteSave() const;
        void NewClimb(Doorways::DoorwaysGameHost& host);   // from the shore, the nests and the beaten forgotten
        void DrawNests(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection);
        std::wstring BackTitle() const;            // "BACK AT THE EAGLE NEST" (or the shore, or a checkpoint: no nests)
        // The creatures' tour (CAUCASIAN_EAGLE_DEMO=3, unattended): she is put on the way before every group and boss in
        // turn, up the mountain. Each must wake, stand on its ground, see her and come at her; then it falls and must be
        // beaten for good. Then every Eagle Nest is stood in and must save her there; then the save is read back and
        // nothing she beat may wake again. Writes tour.tsv beside the evidence file (and pictures into
        // CAUCASIAN_EAGLE_TOUR_SHOTS); ends CREATURES_PASS or CREATURES_FAIL.
        void Tour(Doorways::DoorwaysGameHost& host, float dt);
        void Put(Doorways::DoorwaysGameHost& host, const DirectX::XMFLOAT3& at, float yaw);   // stands her there (no checkpoint, no heal)
        // How a nest would sit at (x, z), about nearY: its worst step of ground under it (m), or -1 where there is no
        // ground, it is not level, the ground drops away right beside it, or something stands there (a pillar, rock)
        static float NestFooting(const DoorwaysRuntimeScene& scene, float x, float z, float nearY, float& groundY);
        // She can stand there (the tour): walkable ground within 1.5 m of p, and all round it 1.2 m out (1.6 m up or down: stairs)
        static bool Standable(const DoorwaysRuntimeScene& scene, const DirectX::XMFLOAT3& p, float& groundY);

        EaglesAscent mMap;
        Journey mCreatures;                        // the road's creatures on the mountain (Journey::InitializeMountain)
        bool mCreaturesOn = false;                 // CAUCASIAN_EAGLE_CREATURES=0 and the map's own tests: none
        std::vector<Journey::MountainCreature> mCreatureList;
        int mSavedNest = 0;                        // the station of the last Eagle Nest she rested at (0: the shore)
        std::vector<std::string> mSavedBeaten;
        size_t mBeatenWritten = 0;                 // the beaten in the save: every creature she beats is saved at once
        std::filesystem::path mSavePath; bool mSaveTest = false;
        bool mTrace = false;                       // CAUCASIAN_EAGLE_MAP_LOG set: the creatures' and nests' events go to the evidence file too
        // the creatures' tour (Tour)
        struct TourStop { int Placement = 0; float Along = 0; };
        bool mTour = false; std::vector<TourStop> mTourStops; int mTourAt = -1, mTourPhase = 0, mTourBad = 0, mTourWoke = 0, mTourCalm = 0, mTourAgain = 0;
        float mTourClock = 0, mTourDamage = 0, mTourOff = 0, mTourFirst = 0; DirectX::XMFLOAT3 mTourStand{};
        std::string mTourNote, mTourRow; std::filesystem::path mTourShots; std::wstring mTourShot;
        // the finishers' tour (CAUCASIAN_EAGLE_TOUR_FINISH=1 with the creatures' tour): her finishing kill of every creature
        // of the road where it stands; the one she finishes, which finisher, how many of each, none with room, nowhere
        // to stand, she off her ground
        bool mTourFinish = false, mTourFinShot = false; int mTourVictim = -1, mTourVictimKind = 0, mTourKind = 0;
        int mTourFinPlayed[4] = {0, 0, 0, 0}, mTourFinNoRoom = 0, mTourFinNoPlace = 0, mTourFinFell = 0, mTourFinOut = 0;
        float mTourFinClock = 0, mTourFinStart = 0, mTourFinAir = 0, mTourVictimY = 0; std::string mTourVictimId;
        Renderer* mRenderer = nullptr;
        std::array<bool, 256> mKeys{};
        bool mPaused = false, mFlying = false, mWasDead = false;
        MountainView mView;                        // M: the whole mountain from outside (MountainView.h)
        float mDeadFor = 0, mRespawnAsk = 0, mClock = 0;
        std::wstring mNoticeTitle, mNoticeLine; float mNotice = 0;
        int mReached = 0;                          // the highest station she has stood at
        // the smoke test (CAUCASIAN_EAGLE_DEMO=1): every station stood on, then a take-off
        bool mSmoke = false; int mSmokeStation = -1, mSmokeBad = 0; float mSmokeClock = 0, mSmokeY = 0;
        // the whole climb (CAUCASIAN_EAGLE_DEMO=2): on foot up the trail, every stretch only wings cross flown with the keys
        bool mClimb = false; size_t mClimbFly = 0; int mClimbPhase = 0, mClimbNext = 1; float mClimbBest = 0, mClimbStuck = 0, mClimbPhaseClock = 0, mClimbLastY = 0; bool mClimbFell = false;
        std::string mLog;
    };
}
