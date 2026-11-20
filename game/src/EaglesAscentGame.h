#pragma once

#include "DoorwaysGame.h"
#include "EaglesAscent.h"
#include "Growth.h"
#include "Journey.h"
#include "Wayside.h"
#include "AscentGrass.h"
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
// places she is saved.
// 0.5 (the owner, 2026-11-20): she grows as on the road (I: wings, swords, armour, power, defence, for crystals), her
// wings are her flight (10 s, then 15, 20, 30); crystals come from the creatures, from clay jars beside every road and
// from hidden places (Wayside); nothing heals her but potions (Z), found in the jars. H at any Eagle Nest saves her, as
// often as she likes: the nest, her growth, the creatures she has beaten, the jars she has broken, the places found.
// When she falls she wakes at the nest she saved at, whole, and all since that save is undone: the creatures she beat
// since stand on the way again, the jars she broke since are whole.
namespace CaucasianEagle
{
    class Ui;
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
        void SaveHere(int station);                // H at a nest: all she is and has done, saved (WriteSave)
        void RollBack();                           // back to the save: her growth, the creatures, the jars, the places
        int NestAt(const DirectX::XMFLOAT3& p) const;   // the station of the nest she stands in; -1
        void Collect(const Wayside::Gain& gain);   // what a jar or a place gave her
        void DrawNests(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection);
        std::wstring BackTitle() const;            // "BACK AT THE EAGLE NEST" (or the shore, or a checkpoint: no nests)
        // The creatures' tour (CAUCASIAN_EAGLE_DEMO=3, unattended): she is put on the way before every group and boss in
        // turn, up the mountain. Each must wake, stand on its ground, see her and come at her; then it falls and must be
        // beaten for good. Then every Eagle Nest is stood in and must save her there; then the save is read back and
        // nothing she beat may wake again. Writes tour.tsv beside the evidence file (and pictures into
        // CAUCASIAN_EAGLE_TOUR_SHOTS); ends CREATURES_PASS or CREATURES_FAIL.
        void Tour(Doorways::DoorwaysGameHost& host, float dt);
        void Put(Doorways::DoorwaysGameHost& host, const DirectX::XMFLOAT3& at, float yaw);
        // The jars' test (CAUCASIAN_EAGLE_DEMO=4, unattended, 0.5): the jars and places laid; she is stood before groups
        // of jars up the mountain and cuts (her own blows: the kit's test pilot keys), and they must break; then saved at a
        // nest, more broken and crystals gained, and rolled back (as when she falls): all must be as saved; then the save
        // read back. Writes ways.tsv beside the evidence file (pictures into CAUCASIAN_EAGLE_TOUR_SHOTS); WAYS_PASS or WAYS_FAIL.
        void Ways(Doorways::DoorwaysGameHost& host, float dt);
        bool mWays = false; int mWaysPhase = 0, mWaysPick = 0, mWaysBad = 0; float mWaysClock = 0;
        // The nature test (CAUCASIAN_EAGLE_DEMO=5, 2026-11-20): the grass fields seen at spots up the mountain (pictures,
        // tufts drawn, frame time), then she runs at trunks beside the way and must not go into or through any.
        // nature.tsv beside the evidence; NATURE_PASS or NATURE_FAIL.
        void Nature(Doorways::DoorwaysGameHost& host, float dt);
        void NatureMenu(Doorways::DoorwaysGameHost& host);
        bool mNature = false; int mNatPhase = 0, mNatPick = 0, mNatBad = 0, mNatFrames = 0, mNatThrough = 0, mNatReached = 0;
        float mNatClock = 0, mNatMs = 0, mNatClosest = 0;
        std::vector<DirectX::XMFLOAT3> mNatSpots; std::vector<EaglesAscent::Trunk> mNatTrunks; DirectX::XMFLOAT3 mNatFrom{};
        std::vector<size_t> mWaysPicks; Doorways::NyxaraKit::Keys mWaysKeys;   // stands her there (no checkpoint, no heal)
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
        size_t mBeatenWritten = 0;                 // the beaten counted (for their crystals)
        Growth::State mGrowth, mSavedGrowth;       // (0.5) her growth now, and as saved
        std::vector<std::string> mSavedJars, mSavedPlaces;
        Wayside mWayside; bool mWaysideOn = false;
        AscentGrass mGrass;   // the grass fields (2026-11-20)
        bool mGrowthOpen = false; float mGrowthFade = 0, mGrowthFlash = 0; int mGrowthRow = 0, mGrowthFlashRow = -1, mFormShown = 0;
        float mSavedMark = 0;                      // the "SAVED" mark's seconds left
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
        int mTourFinPlayed[11]{}, mTourFinNoRoom = 0, mTourFinNoPlace = 0, mTourFinFell = 0, mTourFinOut = 0;
        float mTourFinClock = 0, mTourFinStart = 0, mTourFinAir = 0, mTourVictimY = 0; std::string mTourVictimId;
        Renderer* mRenderer = nullptr;
        std::array<bool, 256> mKeys{};
        bool mPaused = false, mFlying = false, mWasDead = false;
        // Her menu (Esc, 2026-11-20, the owner: Esc gave "the testing lab options"): the road's panel, gold and ink, its rows
        // chosen with W / S or the mouse; Settings opens the game's own settings window (Settings.h), as on the road.
        static constexpr int MenuRows = 8;   // continue, Nyxara, the mountain, settings, last nest, new climb, start screen, leave
        int mMenuRow = 0; float mMenuFade = 0, mMouseX = -1, mMouseY = -1; bool mMouseWas = false, mFocused = true;
        void DrawMenu(Ui& ui) const;
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
