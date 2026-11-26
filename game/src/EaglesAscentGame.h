#pragma once

#include "AscentGrowth.h"
#include "AscentQuests.h"
#include "DoorwaysGame.h"
#include "EaglesAscent.h"
#include "Growth.h"
#include "Journey.h"
#include "Wayside.h"
#include "AscentGrass.h"
#include "AscentTrees.h"
#include "MountainView.h"
#include "HerShowcase.h"
#include "AscentMap.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <map>
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
// wings are her flight (10 s, then 15, 20, 30); crystals come from the creatures and from clay jars beside every road
// (Wayside); nothing heals her but potions (Z), found in the jars. H at any Eagle Nest saves her, as often as she likes:
// the nest, her growth, the creatures she has beaten, the jars she has broken.
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
        void DrawShadowCasters(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& lightViewProjection) override;
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
        void SaveHere(int station, bool quiet = false);   // H at a nest: all she is and has done, saved (WriteSave); quiet: a level bought, a look put on
        // I in a nest: the next level of that item bought (its crystals paid, its look put on, saved there); false when
        // she cannot (not in a nest, at the top, crystals too few)
        void RollBack();                           // back to the save: her growth, the creatures, the jars
        int NestAt(const DirectX::XMFLOAT3& p) const;   // the station of the nest she stands in; -1
        void Collect(const Wayside::Gain& gain);   // what a jar or a place gave her
        void GiveCrystals(int kind, int count);    // (AscentGrowth::CrystalKind)
        // her quests (0.5.3, AscentQuests): Tab shows them, anywhere (Q until 0.5.4); a set of them done, its gear
        void QuestsOpen();                                   // the panel (Tab), on the first quest she can take
        bool TakeQuest(int quest);                           // bought with crystals of its set's kind
        void CountQuests(int kind, int count);               // (AscentQuests::QuestKind) creatures beaten, jars broken
        void ReachNest(int station);                         // she stands in that nest (a quest's nest, or one above it, reached)
        void QuestDone(int quest);
        void GearIfEarned();                                 // her set all done: its gear at once, saved (she wakes at her last nest)
        bool Pressed(int key, bool allowed);                 // Press, or the quests' test's key (mTestKey; its Tab is read in Update)
        // The quests' test (CAUCASIAN_EAGLE_DEMO=11; quests.tsv beside the evidence, and a "picture" row where it holds for one
        // with the HUD, tools\quest-test.ps1): Tab opens her quests anywhere, the first three offered; bought (Enter, a click, S
        // then Enter), too few crystals refused, counted only once taken; all three done: THE HIGHLANDS at once, and saved;
        // the save read back; the next six offered, a nest above the one asked counted; a quest taken after the save rolled
        // back with her.
        void QuestTest(Doorways::DoorwaysGameHost& host, float dt);
        int GroupStretch(int group) const;         // the stretch a group of creatures stands in (its placement's)
        int HereKind(const DirectX::XMFLOAT3& at) const;   // the kind of crystal where she is (mMap.Locate'd there)
        void DrawNests(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection);
        std::wstring BackTitle() const;            // "BACK AT THE EAGLE NEST" (or the shore, or a checkpoint: no nests)
        // The creatures' tour (CAUCASIAN_EAGLE_DEMO=3, unattended): she is put on the way before every group and boss in
        // turn, up the mountain. Each must wake, stand on its ground, see her and come at her; then it falls and must be
        // beaten for good. Then every Eagle Nest is stood in and must save her there; then the save is read back and
        // nothing she beat may wake again. Writes tour.tsv beside the evidence file (and pictures into
        // CAUCASIAN_EAGLE_TOUR_SHOTS); ends CREATURES_PASS or CREATURES_FAIL.
        void Tour(Doorways::DoorwaysGameHost& host, float dt);
        // A boss filmed (CAUCASIAN_EAGLE_DEMO=12, unattended): she is put on the way before one boss
        // (CAUCASIAN_EAGLE_BOSS=<its id>, boss-eagle if none), kept whole, and it fights her for
        // CAUCASIAN_EAGLE_BOSS_SECONDS (40); CAUCASIAN_EAGLE_BOSS_FIGHT=1: she cuts back when it is close; =2 (a creature
        // of the way): she walks at it up the way, as a player goes, and cuts within reach, her left string and her
        // right in turn. Every 0.1 s
        // boss.tsv (beside the evidence) has its place, the ground it stands on, the highest walkable floor under it and
        // her place; a frame every CAUCASIAN_EAGLE_BOSS_EVERY s (0.5) into CAUCASIAN_EAGLE_TOUR_SHOTS. Ends BOSS_DONE.
        void BossFilm(Doorways::DoorwaysGameHost& host, float dt);
        bool mBossFilm = false, mBossFight = false, mBossWalk = false; std::string mBossId; float mBossFor = 40, mBossEvery = .5f, mBossClock = 0, mBossLog = 0, mBossShot = 0;
        int mBossPhase = 0, mBossShots = 0; Doorways::NyxaraKit::Keys mBossKeys;
        // CAUCASIAN_EAGLE_BOSS_KEYS="<seconds>:<key>,...": her keys by the film's clock, each down a tenth of a second (L, R
        // the mouse buttons, F, V, C, E, J for Space)
        std::vector<std::pair<float, char>> mBossTaps;
        // The camera swept (CAUCASIAN_EAGLE_DEMO=13, unattended): she stands on the way every CAUCASIAN_EAGLE_CAMERA_EVERY m
        // (600) facing up it, and the camera's pitch goes from its lowest to its highest in steps; at each, camera.tsv
        // (beside the evidence) has the pitch and the camera's distance and height from her, and a picture is taken.
        void CameraTest(Doorways::DoorwaysGameHost& host, float dt);
        bool mCameraTest = false; float mCamEvery = 600, mCamClock = 0, mCamAlong = 0; int mCamStep = -1;
        // CAUCASIAN_EAGLE_CAMERA_WALK="a:b,c:d": instead, she walks up the way from a to b m and back down, for each; every
        // frame camera-walk.tsv has her height and the camera's (its jumps on stairs, its distance from her) and where she
        // is in its view, a picture every 0.5 s
        std::vector<std::pair<float, float>> mCamWalks; int mCamWalk = 0, mCamWay = 0; float mCamShot = 0, mCamLastY = 0;
        // Off the way (CAUCASIAN_EAGLE_DEMO=14, unattended, 0.5.3; the owner: "i want whole mountain to be walkable beside
        // the paths we have"): every CAUCASIAN_EAGLE_SIDE_EVERY m (40) of the way, from CAUCASIAN_EAGLE_SIDE_FROM to _TO, she
        // is stood on it and runs straight off it, to its left and then to its right (going up), until she is 30 m off it, is
        // stopped, falls under the land, or 8 s pass. side.tsv (beside the evidence) has each run: how far she got, how far
        // she dropped, and why she stopped: free (30 m off), steep (ground too steep to stand on ahead), tree, water,
        // step (ground ahead more than a step over her), BLOCKED (stopped where the ground ahead is walkable: something
        // unseen holds her), FELL_THROUGH (under the land); a picture of each BLOCKED, FELL_THROUGH and timeout.
        void SideWalk(Doorways::DoorwaysGameHost& host, float dt);
        bool mSideWalk = false; int mSideSide = 0, mSideBad = 0; float mSideEvery = 40, mSideFrom = 20, mSideTo = 1e9f, mSideAlong = -1;
        float mSideClock = -1, mSideStill = 0, mSideUnder = 0, mSideLow = 0, mSideYaw = 0; DirectX::XMFLOAT3 mSideStart{}, mSideLast{};
        std::map<std::string, int> mSideCount;
        // CAUCASIAN_EAGLE_SIDE_FILM=1: every frame of each run in side-trace.tsv (where she is, grounded, the land's height
        // and its lowest, the walkable ground under her and over her), and a picture every 0.25 s
        bool mSideFilm = false; float mSideShot = 0; int mSideInto = 0, mSideWall = 0;
        // CAUCASIAN_EAGLE_SIDE_TRACE=1: the trace of every frame without the film's pictures;
        // CAUCASIAN_EAGLE_SIDE_FILM_EVERY=<s>: a picture so often (0.25)
        bool mSideTrace = false; float mSideShotEvery = .25f;
        bool mSideAhead = false;   // CAUCASIAN_EAGLE_SIDE_AHEAD=1: one run a place, up the way itself ("ahead"), not off it
        bool mSideJog = false;     // CAUCASIAN_EAGLE_SIDE_JOG=1: at her easy pace (3.2 m/s), not her run's (5.4); 14 s a run
        // CAUCASIAN_EAGLE_SIDE_DASH=1: she slides (F) off the way and runs on from where it leaves her: her slide against
        // the mountain's walls, at every place the test stands her
        bool mSideDash = false; Doorways::NyxaraKit::Keys mSideKeys;
        // CAUCASIAN_EAGLE_SLOW_MS=<n> (the unattended tests): every frame waits n thousandths of a second more, a slow
        // computer's frames. Measured where a test runs at 200 frames a second and more: 16, about 31 a second (the
        // engine makes two of its steps in each); 33, about 21 (three); 60, about 12 (five); 100 and more, 10 (six):
        // the engine's frame is a tenth of a second at the longest (App.cpp), and slower than that the game plays slowly
        int mSlowMs = 0;
        unsigned long long mSideSteep = 0, mSideSteepLast = 0;   // (the engine's steep walls' held moves, at the run's start and last frame)
        void Put(Doorways::DoorwaysGameHost& host, const DirectX::XMFLOAT3& at, float yaw);
        // The jars' test (CAUCASIAN_EAGLE_DEMO=4, unattended, 0.5): the jars laid; she is stood before groups
        // of jars up the mountain and cuts (her own blows: the kit's test pilot keys), and they must break; then saved at a
        // nest, more broken and crystals gained, and rolled back (as when she falls): all must be as saved; then the save
        // read back. Writes ways.tsv beside the evidence file (pictures into CAUCASIAN_EAGLE_TOUR_SHOTS); WAYS_PASS or WAYS_FAIL.
        void Ways(Doorways::DoorwaysGameHost& host, float dt);
        bool mWays = false; int mWaysPhase = 0, mWaysPick = 0, mWaysBad = 0; float mWaysClock = 0; int mWaysHad[3] = {}, mWaysKinds[3] = {};
        // CAUCASIAN_EAGLE_WAYS_DASH=1: at each group she stands on the way beside it, facing it, and slides (F) through it
        // once in place of her three cuts (2026-11-25: her slide is a dash, and breaks the jars beside her way)
        bool mWaysDash = false; float mWaysFrom = 0;
        int mHereKind = 0;                         // the kind of crystal where she stands (kept while her window's stage holds her)
        float mCrystalFlash[3] = {};               // 1 a crystal of that kind just gained .. 0 (the HUD's count lights)
        bool mQuestsOpen = false;                  // her quests' panel (Tab)
        AscentQuests::Leaving mQuestLeaving;       // the quests just done: their rows' going from the list on the HUD
        int mQuestRow = 0, mQuestFlashRow = -1;
        float mQuestsFade = 0, mQuestFlash = 0;
        bool mQuestTest = false; int mTestKey = 0; // (DEMO=11; a key it presses)
        // The nature test (CAUCASIAN_EAGLE_DEMO=5, 2026-11-20): the grass fields seen at spots up the mountain (pictures,
        // tufts drawn, frame time), then she runs at trunks beside the way and must not go into or through any.
        // nature.tsv beside the evidence; NATURE_PASS or NATURE_FAIL.
        void Nature(Doorways::DoorwaysGameHost& host, float dt);
        // The ground probe (CAUCASIAN_EAGLE_DEMO=6, 2026-11-21, the owner: "whole mountain's grounds or walls should have
        // collisions"): every 16 m over the near and the outer land, where the land is above the sea, the engine's
        // collision is asked for ground she can stand on within 8 m of the land drawn there. ground.tsv; GROUND_PASS
        // when there is no hole.
        void GroundProbe(Doorways::DoorwaysGameHost& host);
        bool mGround = false; int mGroundWait = 0;
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
        Growth::State mGrowth, mSavedGrowth;       // her growth now, and as saved (0.5.2: AscentGrowth's rules)
        // her looks (NyxaraLooks, from her bundle's Looks.txt): the levels worn now, as last put on (-1: not yet)
        bool mLooksOn = false; int mGearShown = -1;   // (the gear whose look and light she has on)
        bool mFlairOn = false;                        // her gear's flair is asked for (not in the tests that measure her own cuts)
        bool mFlairBlue = false;                      // (a comparison's: before her first gear her flair in her own blue, not plain)
        bool mAroundPlaced = false;                // the groups around the way found their places (EaglesAscent::PlaceAround)
        std::vector<std::string> mAroundReport;    // where each was placed, or why not (the ledger writes it)
        std::vector<std::string> mSavedJars;
        Wayside mWayside; bool mWaysideOn = false;
        AscentGrass mGrass;   // the grass fields (2026-11-20)
        AscentTrees mTrees;   // the trees near the path as 3D models (0.5.2)
        bool mGrowthOpen = false; float mGrowthFade = 0, mGrowthFlash = 0; int mGrowthRow = 0, mGrowthFlashRow = -1, mFormShown = 0;
        float mSavedMark = 0;                      // the "SAVED" mark's seconds left
        std::filesystem::path mSavePath; bool mSaveTest = false;
        bool mTrace = false;                       // CAUCASIAN_EAGLE_MAP_LOG set: the creatures' and nests' events go to the evidence file too
        // the creatures' tour (Tour)
        struct TourStop { int Placement = 0; float Along = 0; };
        bool mTour = false; std::vector<TourStop> mTourStops; int mTourAt = -1, mTourPhase = 0, mTourBad = 0, mTourWoke = 0, mTourCalm = 0, mTourAgain = 0;
        float mTourClock = 0, mTourDamage = 0, mTourOff = 0, mTourFirst = 0; DirectX::XMFLOAT3 mTourStand{};
        int mTourHad[3] = {};   // her crystals as a stop began
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
        int mMenuRow = 0; float mMenuFade = 0; bool mFocused = true;
        void DrawMenu(Ui& ui) const;
        // The pointer, for her menu and her window (I; 2026-11-21, the owner: "lets user use mouse cursor when he/she
        // opens I- inventory"): where it is in the game's window, its button held, whether that button went down this
        // frame. Read once a frame (ReadMouse), so that a button already held when a window opens is not a click.
        struct Mouse { float X = -1, Y = -1; bool Down = false, Click = false, Moved = false, Right = false; float Wheel = 0; };   // (Wheel: notches this frame, forward +)
        Mouse ReadMouse(bool inputAllowed) const;
        Mouse mMouse;
        bool mHoldBlade = false;   // a window closed (a click may have closed it): no sword blow until the button is let go
        // The pointer test (CAUCASIAN_EAGLE_DEMO=7, unattended): her window opened; the pointer moved over each card in
        // turn and pressed (each must be chosen, then grow one step and cost its crystals, once however long the button
        // is held); a click where there is no card (nothing); CLOSE clicked (the window closes, the pointer is caught
        // again, her swords wait for the button); then her menu: rows chosen by the pointer, Continue clicked, Nyxara
        // taken (her window opens, the pointer still free), Esc. pointer.tsv beside the evidence; POINTER_PASS or
        // POINTER_FAIL. At "picture" it holds 3 s with a card chosen (its runner takes a picture of the window).
        void PointerTest(Doorways::DoorwaysGameHost& host, float dt);
        // The showcase test (CAUCASIAN_EAGLE_DEMO=9, unattended): in the first Eagle Nest, every level and three looks, her
        // window open; the view of her turned round, tilted, closer and further, a few seconds each (its runner pictures the
        // window). showcase.tsv beside the evidence: each view and where the camera stood; SHOWCASE_PASS or SHOWCASE_FAIL.
        // The map test (CAUCASIAN_EAGLE_DEMO=10, unattended): M opens the detailed map at her; the wheel takes it out to the
        // whole mountain; the pointer over a nest; a drag moves the map; the wheel in; Space back to her; Tab to the 3D view
        // and back; Esc closes it. maptest.tsv beside the evidence; MAP_PASS or MAP_FAIL. Pictures are taken by its runner.
        bool mMapTest = false; int mMapStep = -1, mMapBad = 0; float mMapClock = 0, mPtWheel = 0, mMapWas[3] = {};
        void MapTest(Doorways::DoorwaysGameHost& host, float realDt);
        DirectX::XMFLOAT3 mShowWas{}; bool mShowClosed = false;   // (the showcase test: where she stood, and her window closed again)
        bool mShowTest = false; int mShowStep = -1, mShowBad = 0; float mShowClock = 0;
        void ShowTest(Doorways::DoorwaysGameHost& host, float realDt);
        bool mPointerTest = false; int mPtStep = 0, mPtWait = 0, mPtBad = 0, mPtCrystals = 0, mPtLevel = 0, mPtCost = 0, mPtStation = -1;
        // The crystals' ledger (CAUCASIAN_EAGLE_DEMO=8, unattended, 0.5.2): once the map's ground is up and the jars laid
        // and the groups around the way placed, what lies between each two nests is counted: on the way (its groups, its
        // bosses, the jars beside it) and off it (the caves' groups, the groups around the way, the jars in the caves and
        // on the rings). Each nest's must pay for a level of all three, the way's part 60 to 70 % of it, every jar
        // crystal the map gives laid, every group around the way placed. ledger.tsv; LEDGER_PASS or LEDGER_FAIL.
        void Ledger(Doorways::DoorwaysGameHost& host);
        bool mLedger = false;
        float mPtX = -1, mPtY = -1, mPtHold = 0, mHudW = 0, mHudH = 0, mHudS = 1; bool mPtDown = false, mPtRight = false, mViewHome = false;
        AscentMap mChart;                          // M: the detailed map, to move over and zoom with the mouse (AscentMap.h)
        HerShowcase mShow;
        // Her window's stage (owner, 2026-11-21: "when I happens just let nyxara stay where she stands and just bring her
        // swords in hands pose in front view in front of player to rotate her and choose look"; and behind her a clean
        // backdrop, nothing of the world): while it is open she is up in the open sky over where she stood (StudioHeight),
        // and when it closes she is put back as she was, to the last value. Where she really is (a nest) is mStudioFrom.
        static constexpr float StudioHeight = 5600.f;
        bool mStudio = false; Player mStudioFrom{};
        void StudioIn(WorldState& world);
        void StudioOut(WorldState& world);
        void StudioKeep(WorldState& world);   // moved while it is open (sent to a nest): that is where she stands now                         // her window open: the camera before her, turned by the mouse (HerShowcase.h)
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
