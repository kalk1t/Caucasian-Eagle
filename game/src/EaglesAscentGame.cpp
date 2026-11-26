#ifndef NOMINMAX
#define NOMINMAX   // std::min / std::max, not the Windows macros
#endif
#include "EaglesAscentGame.h"

#include "CaucasianEagleGame.h"
#include "CombatSystem.h"
#include "DoorwaysAssetPaths.h"
#include "DoorwaysApplication.h"
#include "DoorwaysGamepad.h"
#include "DoorwaysHudTextLayout.h"
#include "DoorwaysRenderContexts.h"
#include "DoorwaysRuntimePathResolver.h"
#include "DoorwaysRuntimeScene.h"
#include "DoorwaysSettingsPaths.h"
#include "DoorwaysWalkableSurfaceQuery.h"
#include "AscentQuests.h"
#include "GameConstants.h"
#include "GameUi.h"
#include "NyxaraHud.h"
#include "NyxaraKit.h"
#include "NyxaraLooks.h"
#include "Settings.h"
#include "Sound.h"
#include "StartScreen.h"
#include "Renderer.h"
#include "WorldRenderer.h"
#include "WorldState.h"

#include <chrono>
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <sstream>

// The Eagle's Ascent mode (EaglesAscentGame.h). Map A Gate 5, 2026-11-17.
namespace CaucasianEagle
{
    using namespace Doorways;
    using namespace DirectX;

    namespace
    {
        constexpr float LandSpeed = 6;   // m/s she sinks when R lands her: the mountain is 4 km high, so there is no "too high to land"
        std::wstring Words(std::string s) { std::replace(s.begin(), s.end(), '_', ' '); return {s.begin(), s.end()}; }
        std::string Env(const char* name)
        {
            char value[1024] = {};
            const DWORD size = GetEnvironmentVariableA(name, value, sizeof(value));
            return size > 0 && size < sizeof(value) ? std::string(value) : std::string();
        }
    }

    bool EaglesAscentRequested()
    {   // 0.5: the Ascent unless the road is asked for (GameConstants.h: RoadSwitch)
        if (Env(AscentEnvironmentVariable) == "1") return true;
        if (Env(AscentEnvironmentVariable) == "0" || Env(RoadEnvironmentVariable) == "1" || Env(PilotEnvironmentVariable) == "1") return false;
        std::wstring line = GetCommandLineW();
        std::transform(line.begin(), line.end(), line.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        if (line.find(AscentSwitch) != std::wstring::npos) return true;
        return line.find(RoadSwitch) == std::wstring::npos;
    }

    void EaglesAscentGame::Configure(DoorwaysGameSession& session)
    {
        session.SceneId = AscentSceneId;
        session.SceneCollision = true;   // the mountain's paths, stairs and terraces are the ground
        session.CameraStopsAtGround = true;   // the camera stays out of the rock and inside the caves (owner 2026-11-18)
        session.WindowTitle = std::wstring(GameTitle) + L" - The Eagle's Ascent";
        session.UserDataFolder = UserDataFolder;   // the same settings as the road; its own save beside the road's (AscentSaveFileName)
        // (a test's own folder under %LOCALAPPDATA%, so that the settings it changes and the save it writes are not the
        // player's: CAUCASIAN_EAGLE_USER_FOLDER=<name>; and an unattended run held to so many frames a second, which it
        // is not otherwise: CAUCASIAN_EAGLE_FRAME_LIMIT=<n>, -1 for the limit in the settings)
        if (const std::string folder = Env("CAUCASIAN_EAGLE_USER_FOLDER"); !folder.empty()) session.UserDataFolder = std::wstring(folder.begin(), folder.end());
        session.UnattendedFrameRateLimit = std::atoi(Env("CAUCASIAN_EAGLE_FRAME_LIMIT").c_str());
    }

    bool EaglesAscentGame::Prepare(DoorwaysGameHost& host, std::string& error)
    {
        if (!mMap.Load(ResolveDoorwaysRuntimePath(AscentDataFile), error)) { error = "The Eagle's Ascent: " + error; return false; }
        // its Eagle Nests, bosses and creatures (before its models: the nests are models too)
        if (const auto placing = ResolveDoorwaysRuntimePath(AscentCreaturesFile); std::filesystem::exists(placing) && !mMap.LoadCreatures(placing, error))
        {
            error = "The Eagle's Ascent: " + error;
            return false;
        }
        if (!GetNyxaraKit().LoadBody(host.Render, host.WorldRender, host.World, GetDoorwaysAssetPathRegistry().CompiledRoot / HeroBundle, error))
        {
            error = "Nyxara: " + error;
            return false;
        }
        {   // her looks, the three examples her gears are of (NyxaraLooks: her bundle's Looks.txt); a bundle without them:
            // her gears make her as strong, and she looks as she set out
            std::string looks;
            if (!NyxaraLooks::Load(GetDoorwaysAssetPathRegistry().CompiledRoot / HeroBundle, looks))
                OutputDebugStringA(("[EAGLES ASCENT] Nyxara's looks: " + looks + "\n").c_str());
            mLooksOn = NyxaraLooks::Count() >= 3;
        }
        PrepareNyxaraControls(host);   // the road's controls, camera and controller buttons
        auto& session = GameSession();
        session.FreeFlightCeiling = 0;                         // no ceiling: the mountain is 4 km high
        session.FastMultiplier = FootRunSpeed / FootSpeed;     // she starts on foot
        host.World.MainPlayer.MoveSpeed = FootSpeed;
        host.World.MainPlayer.JumpSpeed = EaglesAscent::JumpSpeed;
        session.WalkOffLedges = true;                          // she can go over any edge (and jump from its brink)
        // Rock faces, banks and cave walls stop her: the walkable collision's faces too steep to stand on are walls in the
        // engine (2026-11-23; she slides along them, and down a slope too steep to stand on). A test can turn them off
        // (CAUCASIAN_EAGLE_STEEP_WALLS=0), or the game's own rules at rock (CAUCASIAN_EAGLE_ROCK_RULES=0).
        session.SteepGroundBlocks = Env("CAUCASIAN_EAGLE_STEEP_WALLS") != "0";
        mMap.RockRules = Env("CAUCASIAN_EAGLE_ROCK_RULES") != "0";
        mMap.LongSteps = Env("CAUCASIAN_EAGLE_LONG_STEPS") != "0";   // (a test's baseline: EaglesAscent::LongSteps)
        GetNyxaraKit().SetRealJump(EaglesAscent::JumpSpeed);   // Space on foot is a real jump here
        auto& camera = host.World.MainCamera;
        camera.FarZ = EaglesAscent::FarPlane; camera.NearZ = .25f;   // the summit must be in view from the shore
        if (!Ui::LoadArt(host.Render, error)) return false;
        {   // the detailed map (M); without its picture M shows the mountain in 3D, as before
            std::string why;
            if (!mChart.Load(host.Render, why)) OutputDebugStringA(("[EAGLES ASCENT] " + why + "\n").c_str());
        }
        if (!GetNyxaraKit().Initialize(host.Render, host.WorldRender, host.World, error)) { error = "Nyxara combat kit: " + error; return false; }
        if (!mMap.LoadModels(host.Render, error)) { error = "The Eagle's Ascent: " + error; return false; }
        if (!mWayside.Load(host.Render, error)) { error = "The Eagle's Ascent's clay jars: " + error; return false; }
        if (!mGrass.Initialize(host.Render, mMap, ResolveDoorwaysRuntimePath(AscentDataFile), error)) { error = "The Eagle's Ascent: " + error; return false; }
        if (!mTrees.Initialize(host.Render, ResolveDoorwaysRuntimePath(AscentDataFile), error)) { error = "The Eagle's Ascent: " + error; return false; }
        mMap.HealOnVisit = false;   // (0.5: nothing heals her but potions)
        GetNyxaraKit().SetOnFoot(true);
        // her blows break the clay jars beside the road (Wayside)
        GetNyxaraKit().SetBlowListener([this](const XMFLOAT3& from, float yaw, float reach, float arc)
        {
            if (mWaysideOn && mWayside.Laid()) Collect(mWayside.Strike(from, yaw, reach, arc));
        });
        // her feet on the stair treads as drawn (she walks the ramp under them); this game lives as long as the program
        GetNyxaraKit().SetFootGround([this](float x, float z, float nearY) { return mMap.TreadTop(x, z, nearY); });
        GetNyxaraKit().SetStairTread([this](float x, float z, float top, float& cx, float& cz, float& ax, float& az) { return mMap.TreadCenter(x, z, top, cx, cz, ax, az); });

        // its Eagle Nests, bosses and creatures (EaglesAscent::LoadCreatures), and her save; the map's own tests (the
        // smoke test, the whole climb) run without the creatures
        mSmoke = Env(DemoEnvironmentVariable) == "1";   // the unattended test of the mode
        mClimb = Env(DemoEnvironmentVariable) == "2";   // the whole climb, unattended
        mTour = Env(DemoEnvironmentVariable) == "3";    // the creatures' tour, unattended (Tour)
        mTourFinish = mTour && Env("CAUCASIAN_EAGLE_TOUR_FINISH") == "1";   // (and her finishing kill of each creature)
        mWays = Env(DemoEnvironmentVariable) == "4";    // the jars' test, unattended (Ways)
        mNature = Env(DemoEnvironmentVariable) == "5";  // the grass and the trees, unattended (Nature)
        mGround = Env(DemoEnvironmentVariable) == "6";  // the ground probe, unattended (GroundProbe)
        mShowTest = Env(DemoEnvironmentVariable) == "9";
        mMapTest = Env(DemoEnvironmentVariable) == "10";     // the detailed map by the pointer, unattended (MapTest)      // her window showing her, turned round, unattended (ShowTest)
        mPointerTest = Env(DemoEnvironmentVariable) == "7";   // her window and her menu by the pointer, unattended (PointerTest)
        mLedger = Env(DemoEnvironmentVariable) == "8";        // the crystals between the nests, counted, unattended (Ledger)
        mQuestTest = Env(DemoEnvironmentVariable) == "11";   // her quests: Tab, bought, counted, listed, a gear earned (QuestTest)
        mBossFilm = Env(DemoEnvironmentVariable) == "12";    // a boss filmed as it fights her (BossFilm)
        mCameraTest = Env(DemoEnvironmentVariable) == "13";  // the camera's pitch swept along the way (CameraTest)
        mSideWalk = Env(DemoEnvironmentVariable) == "14";    // she runs off the way to both sides along it (SideWalk)
        if (!Env(DemoEnvironmentVariable).empty()) mSlowMs = std::clamp(std::atoi(Env("CAUCASIAN_EAGLE_SLOW_MS").c_str()), 0, 200);   // (a test's slow frames)
        mTrees.SeeThroughOff = Env("CAUCASIAN_EAGLE_CAMERA_OLD") == "1";   // (tests: the trees as they were before 2026-11-22)
        mLog = Env(AscentLogEnvironmentVariable);
        mTrace = !mLog.empty();
        if (mLog.empty()) mLog = ResolveDoorwaysRuntimePath("eagles-ascent-evidence.tsv").string();
        LoadSave();
        if (mTour)
        {   // the tour begins a new climb, its save beside its evidence (never a player's)
            mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear();
            if (!mSaveTest) { mSavePath = std::filesystem::path(mLog).parent_path() / "tour.sav"; mSaveTest = true; }
            std::error_code ignored; std::filesystem::remove(mSavePath, ignored);
            mTourShots = Env("CAUCASIAN_EAGLE_TOUR_SHOTS");
        }
        if (mWays)
        {   // (its own save beside its evidence)
            mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear();
            if (!mSaveTest) { mSavePath = std::filesystem::path(mLog).parent_path() / "ways.sav"; mSaveTest = true; }
            std::error_code ignored; std::filesystem::remove(mSavePath, ignored);
            mTourShots = Env("CAUCASIAN_EAGLE_TOUR_SHOTS");
            mWaysDash = Env("CAUCASIAN_EAGLE_WAYS_DASH") == "1";
            GetNyxaraKit().SetAutoKeys(&mWaysKeys);
        }
        if (mBossFilm)
        {   // (a new climb, its save beside its evidence: never a player's)
            mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear();
            if (!mSaveTest) { mSavePath = std::filesystem::path(mLog).parent_path() / "boss.sav"; mSaveTest = true; }
            std::error_code ignored; std::filesystem::remove(mSavePath, ignored);
            mBossId = Env("CAUCASIAN_EAGLE_BOSS"); if (mBossId.empty()) mBossId = "boss-eagle";
            if (const std::string t = Env("CAUCASIAN_EAGLE_BOSS_SECONDS"); !t.empty()) mBossFor = static_cast<float>(std::atof(t.c_str()));
            if (const std::string t = Env("CAUCASIAN_EAGLE_BOSS_EVERY"); !t.empty()) mBossEvery = std::max(.05f, static_cast<float>(std::atof(t.c_str())));
            mBossFight = Env("CAUCASIAN_EAGLE_BOSS_FIGHT") == "1";
            mBossWalk = Env("CAUCASIAN_EAGLE_BOSS_FIGHT") == "2";
            const std::string keys = Env("CAUCASIAN_EAGLE_BOSS_KEYS");   // (her keys by the film's clock: "4:F,6.5:L")
            for (size_t from = 0; from < keys.size();)
            {
                const size_t to = (std::min)(keys.find(',', from), keys.size()), colon = keys.find(':', from);
                if (colon != std::string::npos && colon + 1 < to) mBossTaps.push_back({static_cast<float>(std::atof(keys.substr(from, colon - from).c_str())), keys[colon + 1]});
                from = to + 1;
            }
            GetNyxaraKit().SetAutoKeys(&mBossKeys);
        }
        if (mCameraTest)
        {   // (a new climb, its save beside its evidence; no creatures to come at her)
            mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear();
            if (!mSaveTest) { mSavePath = std::filesystem::path(mLog).parent_path() / "camera.sav"; mSaveTest = true; }
            std::error_code ignored; std::filesystem::remove(mSavePath, ignored);
            if (const std::string t = Env("CAUCASIAN_EAGLE_CAMERA_EVERY"); !t.empty()) mCamEvery = std::max(20.f, static_cast<float>(std::atof(t.c_str())));
            {   // "a:b,c:d"
                std::istringstream list(Env("CAUCASIAN_EAGLE_CAMERA_WALK")); std::string part;
                while (std::getline(list, part, ','))
                    if (const auto colon = part.find(':'); colon != std::string::npos)
                        mCamWalks.push_back({static_cast<float>(std::atof(part.substr(0, colon).c_str())), static_cast<float>(std::atof(part.substr(colon + 1).c_str()))});
            }
            mTourShots = Env("CAUCASIAN_EAGLE_TOUR_SHOTS");
        }
        if (mSideWalk)
        {   // (a new climb, its save beside its evidence; no creatures to come at her)
            mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear();
            if (!mSaveTest) { mSavePath = std::filesystem::path(mLog).parent_path() / "side.sav"; mSaveTest = true; }
            std::error_code ignored; std::filesystem::remove(mSavePath, ignored);
            if (const std::string t = Env("CAUCASIAN_EAGLE_SIDE_EVERY"); !t.empty()) mSideEvery = std::max(4.f, static_cast<float>(std::atof(t.c_str())));
            if (const std::string t = Env("CAUCASIAN_EAGLE_SIDE_FROM"); !t.empty()) mSideFrom = static_cast<float>(std::atof(t.c_str()));
            if (const std::string t = Env("CAUCASIAN_EAGLE_SIDE_TO"); !t.empty()) mSideTo = static_cast<float>(std::atof(t.c_str()));
            mSideFilm = Env("CAUCASIAN_EAGLE_SIDE_FILM") == "1";
            mSideTrace = mSideFilm || Env("CAUCASIAN_EAGLE_SIDE_TRACE") == "1";
            mSideAhead = Env("CAUCASIAN_EAGLE_SIDE_AHEAD") == "1";
            mSideJog = Env("CAUCASIAN_EAGLE_SIDE_JOG") == "1";
            if (const std::string t = Env("CAUCASIAN_EAGLE_SIDE_FILM_EVERY"); !t.empty()) mSideShotEvery = std::clamp(static_cast<float>(std::atof(t.c_str())), .02f, 5.f);
            mSideDash = Env("CAUCASIAN_EAGLE_SIDE_DASH") == "1";
            if (mSideDash) GetNyxaraKit().SetAutoKeys(&mSideKeys);
            mTourShots = Env("CAUCASIAN_EAGLE_TOUR_SHOTS");
        }
        if (mPointerTest || mLedger || mQuestTest)
        {   // a new climb, its save beside its evidence (never a player's)
            mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear();
            if (!mSaveTest) { mSavePath = std::filesystem::path(mLog).parent_path() / (mLedger ? "ledger.sav" : mQuestTest ? "quests.sav" : "pointer.sav"); mSaveTest = true; }
            std::error_code ignored; std::filesystem::remove(mSavePath, ignored);
        }
        if (const std::string gear = Env("CAUCASIAN_EAGLE_GEAR"); !gear.empty()) mSavedGrowth.Gear = std::atoi(gear.c_str());   // (a test's gear)
        mGrowth = mSavedGrowth; AscentGrowth::Clamp(mGrowth); mFormShown = AscentGrowth::Form(mGrowth);
        mWaysideOn = !mSmoke && !mClimb && !mTour;   // (the map's and the creatures' tests: no jars)
        mCreaturesOn = !mMap.Placements.empty() && Env(AscentCreaturesEnvironmentVariable) != "0" && !mSmoke && !mClimb && !mPointerTest && !mQuestTest && !mCameraTest && !mSideWalk;   // (the quests' test counts its creatures itself: none to come at her while it stands still)
        if (mCreaturesOn)
        {
            if (!mCreatures.InitializeMountain(host.Render, error)) { error = "The Eagle's Ascent's creatures: " + error; return false; }
            mCreatures.SetMountainTracing(mTrace);
            StartCreatures();
            // the Eagle at Amiran's court is the last boss now: his statue there is not drawn (beaten, he is not there either)
            for (const auto& p : mMap.Placements)
                if (p.Boss && !p.Kinds.empty() && p.Kinds[0] == "eagle")
                {
                    mMap.HideModels("Landmarks/AmiraniMountain/Eagle", p.P, 30.f, true);
                    // the court he fights over (Journey::UpdateEagle): before Amirani, 14 m round, and not through him
                    XMFLOAT3 amirani{}; float tall = 0;
                    if (mMap.FindModel("Landmarks/AmiraniMountain/Amirani", p.P, 40.f, amirani, tall))
                    {
                        XMFLOAT3 out{p.P.x - amirani.x, 0, p.P.z - amirani.z};
                        const float outward = std::max(.1f, std::hypot(out.x, out.z));
                        const XMFLOAT3 middle{p.P.x + out.x / outward * 4.f, p.P.y, p.P.z + out.z / outward * 4.f};
                        mCreatures.SetEagleCourt(middle, 14.f, amirani, 7.f);
                    }
                    else mCreatures.SetEagleCourt(p.P, 14.f, p.P, 0.f);
                }
        }

        // she begins at the last Eagle Nest she rested at, or on the shore (testing: CAUCASIAN_EAGLE_START=<station>)
        const std::string first = Env(StartEnvironmentVariable);
        const int station = first.empty() ? mSavedNest : std::clamp(std::atoi(first.c_str()), 0, static_cast<int>(mMap.Stations.size()) - 1);
        const auto& start = mMap.Stations[static_cast<size_t>(station)];
        const XMFLOAT3& rest = mMap.Rest(station);   // (a nest's station: in its nest)
        session.OwnSpawn = true; session.SpawnPosition = {rest.x, rest.y + .05f, rest.z}; session.SpawnYaw = start.Yaw;
        host.World.MainPlayerSpawn.Position = session.SpawnPosition; host.World.MainPlayerSpawn.Yaw = start.Yaw;
        camera.Yaw = start.Yaw; mMap.Checkpoint = station; mReached = station;
        session.OwnDeath = true;       // she wakes at the last checkpoint
        session.OwnPauseMenu = true;   // Esc: the Ascent's own pause (resume, last checkpoint, main menu, leave)

        session.MenuTitle = L"CAUCASIAN EAGLE";
        session.MenuSubtitle = L"The Eagle's Ascent";
        session.MenuPlayLabel = L"Climb the Eagle's Ascent";
        StartScreen::SetPlay(L"The Eagle's Ascent", L"Climb the Eagle's Ascent", L"from the sea to Amiran's court");
        session.MenuCredits = {L"Caucasian Eagle", L"A game by Giorgi Samkharadze", L"Built in Doorways, a custom engine",
                               L"founded by Giorgi Samkharadze", L"in February 2026"};
        session.MenuHideLinks = true;
        session.MenuGuideLabel = L"Guide";
        session.OwnSettings = true;
        session.OwnStartScreen = true;
        session.MenuBackdrop = StartScreenPicture;
        session.MenuPanelSide = -1;
        Sound::Initialize();
        Sound::StartMusic("music/menu");

        // her combo, as in the Character Lab (owner, 2026-11-20: "update EaglesAscent's game with newest nyxara"); the
        // unattended tests keep her own cuts, as they were measured with
        if (!mSmoke && !mClimb && !mTour && !mWays && !mNature)
        {   // her combo, and her attacks as her gears have them (0.5.3: the Character Lab's light, at half speed)
            GetNyxaraKit().SetAttackCombo(0);
            GetNyxaraKit().SetHiggsfieldTrial(true, GearAttackPace);
            // and her gear's flair, with its own combo and pace, as her gear is put on (Update; owner, 2026-11-25: "i like
            // 3'd most - so add those to current nyxara's ingame version"; before her first gear plain, no light:
            // 2026-11-26, "normal nyxara should not have effects"). CAUCASIAN_EAGLE_NYXARA_FLAIR=0: her light
            // and her combo as they were before it, to compare.
            mFlairOn = Env("CAUCASIAN_EAGLE_NYXARA_FLAIR") != "0";
            mFlairBlue = Env("CAUCASIAN_EAGLE_NYXARA_FLAIR") == "blue";   // (to compare: before her first gear SKY WHEEL in her own blue, as it was for a day)
        }
        if (mNature || mShowTest || mQuestTest || mBossFilm) mTourShots = Env("CAUCASIAN_EAGLE_TOUR_SHOTS");
        if (mSmoke || mClimb || mTour || mWays || mNature || mGround || mPointerTest || mLedger || mShowTest || mMapTest || mQuestTest || mBossFilm || mCameraTest || mSideWalk)
        {
            session.Unattended = true;
            std::ofstream(mLog) << "seconds\tevent\tstation\tx\ty\tz\tgrounded\tmissing_ground\tflight\n";
            host.StartGame();
        }
        OutputDebugStringA("[PASS] Caucasian Eagle: the Eagle's Ascent is prepared.\n");
        return true;
    }

    bool EaglesAscentGame::Press(int key, bool allowed)
    {
        const bool down = allowed && !GameSession().Unattended && IsDoorwaysKeyDown(key);
        const bool pressed = down && !mKeys[static_cast<size_t>(key)];
        mKeys[static_cast<size_t>(key)] = down;
        return pressed;
    }

    void EaglesAscentGame::Notify(const std::wstring& title, const std::wstring& line, float seconds)
    {
        mNoticeTitle = title; mNoticeLine = line; mNotice = seconds;
    }

    void EaglesAscentGame::Evidence(const WorldState& world, const char* event)
    {
        std::ofstream log(mLog, std::ios::app); if (!log) return;
        const auto& p = world.MainPlayer;
        log << std::fixed << std::setprecision(2) << mClock << '\t' << event << '\t' << (mSmokeStation >= 0 ? mSmokeStation : mMap.Checkpoint) << '\t'
            << p.Position.x << '\t' << p.Position.y << '\t' << p.Position.z << '\t' << p.IsGrounded << '\t' << p.MissingAuthoritativeGround << '\t'
            << (p.TraversalMode == DoorwaysPlayerTraversalMode::Flight) << '\n';
    }

    float EaglesAscentGame::ScaleFrameTime(float realDeltaTime, bool inputAllowed)
    {
        return mPaused || mView.Open || mChart.Open || mGrowthOpen || mQuestsOpen ? 0 : GetNyxaraKit().ScaleFrameTime(realDeltaTime, inputAllowed);
    }

    // Unattended (CAUCASIAN_EAGLE_DEMO=1): she is put on every station and must stand on its ground at its height; at
    // the last one she takes off and must rise. The evidence file ends with ASCENT_PASS or ASCENT_FAIL.
    void EaglesAscentGame::Smoke(DoorwaysGameHost& host, float realDt)
    {
        auto& p = host.World.MainPlayer; auto& session = GameSession();
        mSmokeClock += realDt;
        if (mClock < 3) return;
        const int stations = static_cast<int>(mMap.Stations.size());
        if (mSmokeStation < 0 || (mSmokeStation < stations && mSmokeClock > 1.5f))
        {
            if (mSmokeStation >= 0)
            {
                const auto& s = mMap.Stations[static_cast<size_t>(mSmokeStation)];
                const bool ok = p.IsGrounded && !p.MissingAuthoritativeGround && std::abs(p.Position.y - s.P.y) < 3;
                if (!ok) ++mSmokeBad;
                Evidence(host.World, ok ? "stood" : "stood_FAIL");
            }
            if (++mSmokeStation < stations) { mMap.Visit(host, mSmokeStation); Evidence(host.World, "station"); }
            else { session.FreeFlightTakeoff = true; mSmokeY = p.Position.y; Evidence(host.World, "takeoff"); }
            mSmokeClock = 0;
        }
        else if (mSmokeStation >= stations && mSmokeClock > 4)
        {
            const bool flew = p.TraversalMode == DoorwaysPlayerTraversalMode::Flight && p.Position.y - mSmokeY > 3;
            if (!flew) ++mSmokeBad;
            Evidence(host.World, flew ? "flew" : "flew_FAIL");
            Evidence(host.World, mSmokeBad == 0 ? "ASCENT_PASS" : "ASCENT_FAIL");
            mSmoke = false; PostQuitMessage(mSmokeBad == 0 ? 0 : 2);
        }
    }

    // Unattended (CAUCASIAN_EAGLE_DEMO=2, 2026-11-18): the whole climb from the shore, as a player would make it. On foot she
    // runs up the trail (W, Shift; the camera turned to the trail 8 m ahead). At each stretch only wings cross she takes
    // off at its launch mark (R), turns to its LAND mark, climbs over what is ahead (Space), flies (W), sinks to it (X),
    // lets W go over the mark and lands (R); then runs on. Nothing moves her but her keys. Fails if she makes no way for
    // 25 s, if a safety net takes her, or a crossing takes over 90 s. Ends CLIMB_PASS at the summit's station.
    void EaglesAscentGame::Climb(DoorwaysGameHost& host, float dt)
    {
        auto& session = GameSession(); auto& p = host.World.MainPlayer; auto& camera = host.World.MainCamera;
        session.ScriptedMove = 0; session.ScriptedFast = false; session.ScriptedJump = session.ScriptedDescend = false;
        if (mClock < 3 || mMap.Trail.size() < 4) return;
        const bool flying = p.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
        const auto finish = [&](bool ok)
        {   // (and how often the rules held her on the way: at a face as drawn, from walking into the mountain; 2026-11-22)
            Evidence(host.World, ("climb_holds_wall_" + std::to_string(mMap.WallHolds) + "_into_" + std::to_string(mMap.IntoHolds) +
                                  "_steep_" + std::to_string(GetDoorwaysRuntimeSteepGroundStats().HeldMoves) +
                                  "_of_faces_" + std::to_string(GetDoorwaysRuntimeSteepGroundStats().Faces)).c_str());
            Evidence(host.World, ok ? "CLIMB_PASS" : "CLIMB_FAIL"); mClimb = false; PostQuitMessage(ok ? 0 : 2);
        };
        std::vector<const EaglesAscent::Flight*> flies;   // the stretches only wings cross, in order (flight lines FLYnn)
        for (const auto& f : mMap.Flights) if (f.Id.rfind("FLY", 0) == 0) flies.push_back(&f);
        const int stations = static_cast<int>(mMap.Stations.size());
        if (mClimbNext < stations)
        {
            const auto& s = mMap.Stations[static_cast<size_t>(mClimbNext)];
            if (!flying && std::hypot(p.Position.x - s.P.x, p.Position.z - s.P.z) < 12 && std::abs(p.Position.y - s.P.y) < 3)
            {
                mSmokeStation = mClimbNext; Evidence(host.World, "climb_station"); mSmokeStation = -1;
                if (++mClimbNext >= stations) { finish(true); return; }
            }
        }
        mClimbPhaseClock += dt;
        static float sampleAt = 0; if (mClock - sampleAt >= 1) { sampleAt = mClock; Evidence(host.World, ("climb_sample_" + std::to_string(static_cast<int>(mMap.Along)) + "_" + std::to_string(mClimbPhase)).c_str()); }
        if (mClimbPhase == 0)
        {   // on foot
            // the trail 8 m ahead, running, bends too (a careless runner: she cuts every corner; the parapets over a lower
            // leg must hold her). CAUCASIAN_EAGLE_CLIMB_CAREFUL=1: in a sharp bend 4 m ahead, at a jog.
            static const bool careful = Env("CAUCASIAN_EAGLE_CLIMB_CAREFUL") == "1";
            const size_t at = std::min(static_cast<size_t>(mMap.Along / EaglesAscent::TrailStep), mMap.Trail.size() - 4);
            const auto& t0 = mMap.Trail[at]; const auto& t1 = mMap.Trail[at + 1]; const auto& t3 = mMap.Trail[at + 3];
            const float h0 = std::atan2(t1.x - t0.x, t1.z - t0.z), h3 = std::atan2(t3.x - mMap.Trail[at + 2].x, t3.z - mMap.Trail[at + 2].z);
            const bool bend = careful && std::abs(std::remainder(h3 - h0, 6.2831853f)) > .87f;
            XMFLOAT3 aim = mMap.Trail[at + (bend ? 1 : 2)];
            if (!mClimbFell && mClimbLastY - p.Position.y > 2.5f && p.IsGrounded) { mClimbFell = true; Evidence(host.World, ("climb_fell_off_at_" + std::to_string(static_cast<int>(mMap.Along))).c_str()); }
            if (p.IsGrounded) { mClimbLastY = p.Position.y; mClimbFell = false; }
            if (mClimbFly < flies.size())
            {
                const auto& f = *flies[mClimbFly];
                const float d = std::hypot(p.Position.x - f.From.x, p.Position.z - f.From.z);
                if (d < 2.5f && std::abs(p.Position.y - f.From.y) < 3)
                { mClimbPhase = 1; mClimbPhaseClock = 0; Evidence(host.World, ("climb_launch_" + f.Id).c_str()); return; }
                if (d < 14) aim = f.From;                 // the launch mark when it is near: the trail goes no further
            }
            const float to = std::hypot(aim.x - p.Position.x, aim.z - p.Position.z);
            camera.Yaw = std::atan2(aim.x - p.Position.x, aim.z - p.Position.z);
            session.ScriptedMove = 'W'; session.ScriptedFast = to > 6 && !bend;
            if (mMap.Along > mClimbBest + 1) { mClimbBest = mMap.Along; mClimbStuck = 0; } else mClimbStuck += dt;
            if (mClimbStuck > 25) { Evidence(host.World, "climb_stuck"); finish(false); }
            return;
        }
        const auto& f = *flies[mClimbFly];
        const float dx = f.To.x - p.Position.x, dz = f.To.z - p.Position.z, flat = std::hypot(dx, dz);
        switch (mClimbPhase)
        {
        case 1:                                           // R
            session.FreeFlightEnd = false; session.FreeFlightTakeoff = true; session.FreeFlightLandSpeed = 0; mClimbPhase = 2; mClimbPhaseClock = 0; break;
        case 2:
            if (flying) mClimbPhase = 3;
            else if (mClimbPhaseClock > 6) { Evidence(host.World, "climb_no_takeoff"); finish(false); }
            break;
        case 3:
        {
            camera.Yaw = std::atan2(dx, dz);
            float need = f.To.y + 3, ahead = -1e9f;
            for (int i = 0; i <= 32; ++i)
            {
                const float t = static_cast<float>(i) / 32, h = mMap.FlightLand(p.Position.x + dx * t, p.Position.z + dz * t);
                if (flat * t > 3) need = std::max(need, h + 3);
                if (flat * t < 8) ahead = std::max(ahead, h);
            }
            if (p.Position.y < need - .3f) session.ScriptedJump = true;
            else if (flat < 30 && p.Position.y > f.To.y + 6) session.ScriptedDescend = true;
            if ((p.Position.y >= need - 1 || p.Position.y > ahead + 1.5f) && flat > .8f) session.ScriptedMove = 'W';
            else if (flat <= .8f && std::abs(p.Position.y - (f.To.y + 3)) < 3)
            { session.FreeFlightEnd = true; session.FreeFlightLandSpeed = LandSpeed; mClimbPhase = 4; mClimbPhaseClock = 0; }
            if (mClimbPhaseClock > 90) { Evidence(host.World, ("climb_never_crossed_" + f.Id).c_str()); finish(false); }
            break;
        }
        default:                                          // down onto the far side, then on foot again
            if (!flying && p.IsGrounded)
            {
                const bool ok = flat < 6 && std::abs(p.Position.y - f.To.y) < 3;
                Evidence(host.World, ((ok ? "climb_crossed_" : "climb_landed_off_") + f.Id).c_str());
                if (!ok) { finish(false); return; }
                ++mClimbFly; mClimbPhase = 0; mClimbBest = mMap.Along; mClimbStuck = 0;
            }
            else if (mClimbPhaseClock > 15) { Evidence(host.World, ("climb_no_landing_" + f.Id).c_str()); finish(false); }
        }
    }

    bool EaglesAscentGame::Standable(const DoorwaysRuntimeScene& scene, const XMFLOAT3& p, float& groundY)
    {
        const auto hit = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(scene, p.x, p.z, p.y - 1.5f, p.y + 1.5f);
        if (!hit.Hit) return false;
        for (int k = 0; k < 6; ++k)
        {   // ground all round her too (not the lip of a drop)
            const float a = 6.2831853f * static_cast<float>(k) / 6;
            if (!QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(scene, p.x + 1.2f * std::cos(a), p.z + 1.2f * std::sin(a), hit.GroundY - 1.6f, hit.GroundY + 1.6f).Hit) return false;   // (stairs of 45 degrees)
        }
        groundY = hit.GroundY;
        return true;
    }

    float EaglesAscentGame::NestFooting(const DoorwaysRuntimeScene& scene, float x, float z, float nearY, float& groundY)
    {
        const auto ground = [&](float px, float pz, float lo, float hi, float& y)
        {
            const auto hit = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(scene, px, pz, lo, hi);
            y = hit.GroundY;
            return hit.Hit;
        };
        float middle = 0, y = 0, worst = 0;
        if (!ground(x, z, nearY - .8f, nearY + .8f, middle)) return -1;
        for (int k = 0; k < 12; ++k)
        {
            const float a = 6.2831853f * static_cast<float>(k) / 12;
            for (const float r : {1.1f, 2.0f})
            {   // level under it
                if (!ground(x + r * std::cos(a), z + r * std::sin(a), middle - .6f, middle + .6f, y)) return -1;
                worst = std::max(worst, std::abs(y - middle));
            }
            if (!ground(x + 2.9f * std::cos(a), z + 2.9f * std::sin(a), middle - 1.5f, middle + .8f, y)) return -1;   // no drop beside it
        }
        if (worst > .3f) return -1;
        if (IsDoorwaysRuntimeSceneBlockedForPlayer(scene, {x, middle + .25f, z}, 1.95f, 1.4f)) return -1;   // nothing standing in it
        groundY = middle;
        return worst;
    }

    void EaglesAscentGame::Put(DoorwaysGameHost& host, const XMFLOAT3& at, float yaw)
    {
        auto& p = host.World.MainPlayer; auto& session = GameSession();
        p.Position = {at.x, at.y + .05f, at.z}; p.Yaw = yaw; p.VerticalVelocity = 0;
        p.TraversalMode = DoorwaysPlayerTraversalMode::Ground;
        session.FreeFlightEnd = true; session.ImpulseX = session.ImpulseZ = 0;
        host.World.MainCamera.Yaw = yaw; host.World.MainCamera.Pitch = -.12f;
    }

    namespace
    {
        float WallMs()   // milliseconds since the last call: a frame's real length in an unattended run
        {
            static auto last = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const float ms = std::chrono::duration<float, std::milli>(now - last).count(); last = now; return ms;
        }
    }

    void EaglesAscentGame::Nature(DoorwaysGameHost& host, float dt)
    {
        auto& world = host.World; auto& session = GameSession(); auto& p = world.MainPlayer;
        session.ScriptedMove = 0; session.ScriptedFast = false;
        if (mClock < 3) return;
        mNatClock += dt;
        host.Combat.RestorePlayerHealth(world, 10000);
        const auto table = std::filesystem::path(mLog).parent_path() / "nature.tsv";
        const auto row = [&](const std::string& line) { std::ofstream(table, std::ios::app) << line << '\n'; };
        if (mNatPhase == 0)
        {   // eight spots up the way where grass grows beside it; trunks within 25 m of the way, one every 250 m or so
            std::ofstream(table, std::ios::trunc) << "what\tvalues\n";
            row("map\ttrees=" + std::to_string(mMap.TreeCount()) + "\tgrass=" + (mGrass.On() ? "on" : "off") + "\tmodels=" + (mTrees.On() ? "on" : "off"));
            const auto& T = mMap.Trail;
            std::vector<XMFLOAT3> green;
            for (size_t i = 0; i < T.size(); i += 12)
            {
                float y = 0; int n = 0;
                for (const float side : {-6.f, 6.f})
                {
                    const XMFLOAT3& a = T[i]; const XMFLOAT3& b = T[std::min(i + 1, T.size() - 1)];
                    const float ax = b.x - a.x, az = b.z - a.z, l = std::max(.01f, std::hypot(ax, az));
                    n += mGrass.Grows(a.x - az / l * side, a.z + ax / l * side, y) > .3f ? 1 : 0;
                }
                if (n >= 1) green.push_back(T[i]);
            }
            for (int k = 0; k < 8 && !green.empty(); ++k) mNatSpots.push_back(green[(green.size() - 1) * static_cast<size_t>(k) / 7]);
            float lastAt = -1e9f;
            for (size_t i = 0; i < T.size() && mNatTrunks.size() < 24; i += 4)
            {
                const float at = static_cast<float>(i) * EaglesAscent::TrailStep;
                if (at - lastAt < 250) continue;
                EaglesAscent::Trunk t;
                if (!mMap.NearestTrunk(T[i].x, T[i].y, T[i].z, 25.f, t) || t.R < .15f) continue;
                if (std::abs(mMap.Land(t.X, t.Z) - t.Y) > 1.2f) continue;          // (on the land, not a rock face)
                float dx = T[i].x - t.X, dz = T[i].z - t.Z; const float l = std::max(.01f, std::hypot(dx, dz)); dx /= l; dz /= l;
                const float sx = t.X + dx * (t.R + 3.5f), sz = t.Z + dz * (t.R + 3.5f), sy = mMap.Land(sx, sz);
                const float slope = std::abs(sy - t.Y) / (t.R + 3.5f);
                if (slope > .45f) continue;
                EaglesAscent::Trunk other;   // (no second trunk on the way in)
                if (mMap.NearestTrunk(t.X + dx * (t.R + 1.8f), sy, t.Z + dz * (t.R + 1.8f), .9f, other) && (other.X != t.X || other.Z != t.Z)) continue;
                mNatTrunks.push_back(t); lastAt = at;
            }
            row("picked\tspots=" + std::to_string(mNatSpots.size()) + "\ttrunks=" + std::to_string(mNatTrunks.size()));
            if (mNatSpots.empty()) { ++mNatBad; row("NO_GREEN_SPOT"); }
            mNatPhase = 1; mNatPick = 0; mNatClock = 0; mNatMs = 0; mNatFrames = 0;
            return;
        }
        if (mNatPhase == 1)
        {   // grass: stood on the way, looking along it; then to the side
            if (mNatPick >= static_cast<int>(mNatSpots.size())) { mNatPhase = 2; mNatPick = 0; mNatClock = 0; return; }
            const auto& T = mMap.Trail;
            const XMFLOAT3 at = mNatSpots[static_cast<size_t>(mNatPick)];
            size_t i = 0; float best = 1e30f;
            for (size_t k = 0; k < T.size(); ++k) { const float d = (T[k].x - at.x) * (T[k].x - at.x) + (T[k].z - at.z) * (T[k].z - at.z); if (d < best) { best = d; i = k; } }
            const XMFLOAT3& b = T[std::min(i + 2, T.size() - 1)];
            const float yaw = std::atan2(b.x - at.x, b.z - at.z);
            if (mNatClock < dt * 1.5f) Put(host, at, yaw);
            world.MainCamera.Yaw = mNatClock < 4.5f ? yaw : yaw + 1.3f; world.MainCamera.Pitch = -.18f;
            if (mNatClock > 2.5f && mNatClock < 4.f) { mNatMs += WallMs(); ++mNatFrames; } else WallMs();   // (wall-clock: unattended runs step a fixed dt)
            if (mNatClock > 4.f && mNatClock - dt <= 4.f)
            {
                mTourShot = L"grass_" + std::to_wstring(mNatPick) + L"_along";
                std::ostringstream line; line << std::fixed << std::setprecision(1) << "grass\t" << mNatPick << "\ty=" << at.y << "\ttufts=" << mGrass.TuftsDrawn
                                              << "\tsquares=" << mGrass.CellsDrawn << "\tframe_ms=" << (mNatFrames ? mNatMs / static_cast<float>(mNatFrames) : 0.f);
                line << "\t" << mTrees.Report();
                if (mGrass.TuftsDrawn < 2000) { ++mNatBad; line << "\tTHIN"; }
                row(line.str());
            }
            if (mNatClock > 6.f && mNatClock - dt <= 6.f) mTourShot = L"grass_" + std::to_wstring(mNatPick) + L"_side";
            if (mNatClock > 6.3f) { ++mNatPick; mNatClock = 0; mNatMs = 0; mNatFrames = 0; }
            return;
        }
        if (mNatPhase == 3) { NatureMenu(host); return; }
        if (mNatPhase == 2)
        {   // trees: 3.5 m from a trunk's bark, running at it for 2.4 s
            if (mNatPick >= static_cast<int>(mNatTrunks.size()))
            {
                row("trees\ttries=" + std::to_string(mNatTrunks.size()) + "\treached=" + std::to_string(mNatReached) + "\tthrough=" + std::to_string(mNatThrough));
                if (mNatTrunks.empty()) { ++mNatBad; row("NO_TRUNKS"); }
                if (mMap.TreesHold && mNatThrough > 0) ++mNatBad;
                row(std::string("trees_hold\t") + (mMap.TreesHold ? "yes" : "no (CAUCASIAN_EAGLE_TREES=0: the baseline)"));
                mNatPhase = 3; mNatClock = 0;   // then her menu (Esc) and its settings window, pictured
                return;
            }
            const auto& t = mNatTrunks[static_cast<size_t>(mNatPick)];
            if (mNatClock < dt * 1.5f)
            {
                const XMFLOAT3 trail = mMap.Trail[0];
                (void)trail;
                float best = 1e30f; XMFLOAT3 near2{};
                for (const auto& q : mMap.Trail) { const float d = (q.x - t.X) * (q.x - t.X) + (q.z - t.Z) * (q.z - t.Z); if (d < best) { best = d; near2 = q; } }
                float dx = near2.x - t.X, dz = near2.z - t.Z; const float l = std::max(.01f, std::hypot(dx, dz)); dx /= l; dz /= l;
                mNatFrom = {t.X + dx * (t.R + 3.5f), 0, t.Z + dz * (t.R + 3.5f)}; mNatFrom.y = mMap.Land(mNatFrom.x, mNatFrom.z);
                Put(host, mNatFrom, std::atan2(-dx, -dz));
                mNatClosest = 1e30f;
            }
            const float yaw = std::atan2(t.X - mNatFrom.x, t.Z - mNatFrom.z);
            world.MainCamera.Yaw = yaw; p.Yaw = yaw;
            if (mNatClock > .3f) { session.ScriptedMove = 'W'; session.ScriptedFast = true; }
            mNatClosest = std::min(mNatClosest, std::hypot(p.Position.x - t.X, p.Position.z - t.Z));
            if (mNatClock > 1.2f && mNatClock - dt <= 1.2f && mNatPick < 3) mTourShot = L"trunk_" + std::to_wstring(mNatPick);
            if (mNatClock > 2.7f)
            {
                const float fx = t.X - mNatFrom.x, fz = t.Z - mNatFrom.z, fl = std::max(.01f, std::hypot(fx, fz));
                const float past = ((p.Position.x - t.X) * fx + (p.Position.z - t.Z) * fz) / fl;   // > 0: beyond the trunk's middle
                // into the bark (her middle nearer the trunk's than its radius and a little of her own width): she went in or
                // through. Held at the bark and then slid round it is what a body against a round trunk does.
                const bool through = mNatClosest < t.R + .15f;
                const bool reached = mNatClosest < t.R + 1.0f;
                mNatThrough += through ? 1 : 0; mNatReached += reached ? 1 : 0;
                std::ostringstream line; line << std::fixed << std::setprecision(2) << "trunk\t" << mNatPick << "\tr=" << t.R << "\th=" << t.H << "\tclosest=" << mNatClosest
                                              << "\tpast=" << past << "\t" << (through ? "THROUGH" : reached ? (past > t.R ? "held_slid_round" : "held") : "not_reached") << "\tat=" << t.X << "," << t.Y << "," << t.Z;
                row(line.str());
                ++mNatPick; mNatClock = 0;
            }
        }
    }

    // (the nature test's last phase: her menu and its settings window open; tools\release\Save-GameWindow.ps1 pictures them)
    void EaglesAscentGame::NatureMenu(DoorwaysGameHost& host)
    {
        auto& world = host.World;
        const auto table = std::filesystem::path(mLog).parent_path() / "nature.tsv";
        const auto row = [&](const std::string& line) { std::ofstream(table, std::ios::app) << line << '\n'; };
        if (mNatClock < .1f) { mPaused = true; mMenuRow = 3; }                      // Esc: her menu, Settings chosen
        if (mNatClock > 1.f && mNatPick != 100 && mNatPick < 101) { mNatPick = 100; row(std::string("menu\topen=") + (mPaused ? "yes" : "no") + "\trows=" + std::to_string(MenuRows)); if (!mPaused) ++mNatBad; }
        if (mNatClock > 4.f && mNatPick == 100) { mNatPick = 101; Settings::Open(); }   // Settings taken
        if (mNatClock > 5.f && mNatPick == 101) { mNatPick = 102; row(std::string("settings\topen=") + (Settings::IsOpen() ? "yes" : "no")); if (!Settings::IsOpen()) ++mNatBad; }
        if (mNatClock > 8.f && mNatPick == 102)
        {
            mNatPick = 103; Settings::Close(); mPaused = false;
            row(mNatBad == 0 ? "NATURE_PASS" : "NATURE_FAIL\tbad=" + std::to_string(mNatBad));
            Evidence(world, mNatBad == 0 ? "NATURE_PASS" : "NATURE_FAIL");
            mNature = false; PostQuitMessage(mNatBad == 0 ? 0 : 2);
        }
    }

    // The pointer in the game's window, for her menu and her window (I): client pixels, -1 where there is none (the
    // window not in front, an unattended run); the pointer test's own pointer in its place.
    EaglesAscentGame::Mouse EaglesAscentGame::ReadMouse(bool inputAllowed) const
    {
        Mouse m;
        if (mPointerTest || mMapTest || mQuestTest) { m.X = mPtX; m.Y = mPtY; m.Down = mPtDown; m.Right = mPtRight; m.Wheel = mPtWheel; }
        else if (inputAllowed && !GameSession().Unattended)
        {
            POINT point{};
            if (GetCursorPos(&point) && ScreenToClient(GetForegroundWindow(), &point)) { m.X = static_cast<float>(point.x); m.Y = static_cast<float>(point.y); }
            m.Down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            m.Right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            m.Wheel = static_cast<float>(GameSession().PointerWheel) / 120.f;
        }
        m.Click = m.Down && !mMouse.Down;
        m.Moved = std::abs(m.X - mMouse.X) + std::abs(m.Y - mMouse.Y) > .5f;
        return m;
    }

    void EaglesAscentGame::MapTest(DoorwaysGameHost& host, float realDt)
    {
        auto& world = host.World; auto& session = GameSession(); const auto& p = world.MainPlayer;
        const auto table = std::filesystem::path(mLog.empty() ? std::string("map-evidence.tsv") : mLog).parent_path() / "maptest.tsv";
        const auto row = [&](const std::string& line) { std::ofstream(table, std::ios::app) << line << '\n'; };
        const auto check = [&](bool ok, const std::string& what)
        {
            std::ostringstream l; l << std::fixed << std::setprecision(2) << mMapClock << '\t' << what << '\t' << mChart.CentreX() << '\t' << mChart.CentreZ() << '\t'
                                    << mChart.Scale() << '\t' << (ok ? "ok" : "FAIL");
            row(l.str()); if (!ok) ++mMapBad;
        };
        mPtWheel = 0;
        if (mMapStep < 0)
        {
            if (mClock < 6 || !world.RuntimeScene || !world.RuntimeScene->Loaded || mHudW <= 0)
            {   // (what it waits for, every 5 s: in waiting.tsv beside its table)
                if (std::fmod(mClock, 5.f) < realDt)
                    std::ofstream(table.parent_path() / "waiting.tsv", std::ios::app) << mClock << "\tscene " << (world.RuntimeScene ? 1 : 0) << "\tloaded "
                        << (world.RuntimeScene && world.RuntimeScene->Loaded ? 1 : 0) << "\thud_w " << mHudW << "\tpaused " << mPaused << '\n';
                return;
            }
            std::ofstream(table) << "seconds\tstep\tcentre_x\tcentre_z\tpx_a_metre\tresult\n";
            if (!mMap.Nests.empty()) mMap.Visit(host, mMap.Nests[std::min<size_t>(1, mMap.Nests.size() - 1)].Station);   // (her, a little up the way)
            mMapStep = 0; mMapClock = 0; mPtX = mHudW * .4f; mPtY = mHudH * .5f; mPtDown = false;
            return;
        }
        mMapClock += realDt;
        constexpr float Each = 2.5f;
        const int at = static_cast<int>(mMapClock / Each);
        const float in = std::fmod(mMapClock, Each);
        const bool first = at != mMapStep;
        mMapStep = at;
        const float cx = mHudW * .4f, cy = mHudH * .5f;
        const auto when = [&](float s) { return in > s && in - realDt <= s; };
        const auto flat = [&](const DirectX::XMFLOAT3& q) { return std::hypot(mView.Focus.x - q.x, mView.Focus.z - q.z); };
        // M: the mountain in 3D, gone over with the pointer (MountainView::Explore); then Tab: the detailed map
        switch (at)
        {
        case 0:
            if (first) { mPtX = cx; mPtY = cy; mPtDown = mPtRight = false; }
            if (when(.3f)) { mView.Toggle(session, world.MainCamera, mMap.Trail, mMap.Summit); mView.Explored = true; }   // (as M does)
            if (when(2.f))
            {
                check(mView.Open && !mChart.Open, "m_opens_the_mountain_in_3d");
                check(session.FreeCursor, "pointer_free");
                check(mView.Eye.y > mMap.Summit * .5f, "camera_high_over_it_y_" + std::to_string(static_cast<int>(mView.Eye.y)));
            }
            break;
        case 1:
            if (first) { mMapWas[0] = mView.Focus.x; mMapWas[1] = mView.Focus.z; mMapWas[2] = mView.Dist; }
            if (in < .2f) mPtDown = true;
            else if (in < .9f) { mPtDown = true; mPtX = cx - 400.f * (in - .2f) / .7f; mPtY = cy + 150.f * (in - .2f) / .7f; }
            else mPtDown = false;
            if (when(2.f)) check(std::hypot(mView.Focus.x - mMapWas[0], mView.Focus.z - mMapWas[1]) > mMapWas[2] * .2f, "drag_goes_over_the_mountain");
            break;
        case 2:
            if (first) { mPtX = cx; mPtY = cy; mMapWas[0] = mView.Yaw; mMapWas[1] = mView.Pitch; }
            if (in < .2f) mPtRight = true;
            else if (in < .9f) { mPtRight = true; mPtX = cx + 300.f * (in - .2f) / .7f; mPtY = cy - 120.f * (in - .2f) / .7f; }
            else mPtRight = false;
            if (when(2.f)) check(std::abs(mView.Yaw - mMapWas[0]) > .3f && mView.Pitch != mMapWas[1], "right_drag_turns_and_tilts");
            break;
        case 3:
            if (first) { mPtX = cx; mPtY = cy; mMapWas[2] = mView.Dist; }
            if (in < .6f) mPtWheel = 1;
            if (when(2.f)) check(mView.Dist < mMapWas[2] * .25f && mView.Eye.y > mMap.Land(mView.Eye.x, mView.Eye.z) + 3, "wheel_in_close_never_in_the_rock");
            break;
        case 4:
            if (when(.2f)) mViewHome = true;   // (Space)
            if (when(2.f)) check(flat(p.Position) < 5 && mView.Dist <= 260.f, "space_to_her");
            break;
        case 5:
            if (when(.2f)) { mView.Hide(session, world.MainCamera); mChart.Show(p.Position); }   // (Tab)
            if (when(2.f)) check(mChart.Open && !mView.Open && session.FreeCursor && std::hypot(mChart.CentreX() - p.Position.x, mChart.CentreZ() - p.Position.z) < 5, "tab_the_detailed_map_at_her");
            break;
        case 6:
            if (first) { mPtX = cx; mPtY = cy; mPtDown = false; mMapWas[0] = mChart.CentreX(); mMapWas[1] = mChart.CentreZ(); mMapWas[2] = mChart.Scale(); }
            else if (in < .2f) mPtDown = true;
            else if (in < .9f) { mPtDown = true; mPtX = cx - 400.f * (in - .2f) / .7f; mPtY = cy + 150.f * (in - .2f) / .7f; }
            else mPtDown = false;
            if (when(2.f)) check(mChart.CentreX() > mMapWas[0] + 250.f / mMapWas[2] && mChart.CentreZ() > mMapWas[1] + 80.f / mMapWas[2], "map_drag_moves_it");
            break;
        case 7:
            if (first) mMapWas[2] = mChart.Scale();
            if (in < .6f) mPtWheel = -2;
            if (when(2.f)) check(mChart.Scale() < mMapWas[2] * .5f, "map_wheel_out_to_the_whole");
            break;
        case 8:
            if (when(.2f)) { mChart.Hide(); mView.Toggle(session, world.MainCamera, mMap.Trail, mMap.Summit); mView.Explored = true; }   // (Tab again)
            if (when(1.5f)) check(mView.Open && !mChart.Open && session.FreeCursor, "tab_back_to_3d");
            break;
        case 9:
            if (when(.2f)) session.EscapePressed = true;
            if (when(1.f)) check(!mView.Open && !mChart.Open && !session.FreeCursor, "esc_closes_pointer_caught");
            break;
        default:
            row(mMapBad == 0 ? "MAP_PASS" : "MAP_FAIL");
            mMapTest = false; PostQuitMessage(mMapBad == 0 ? 0 : 2);
            break;
        }
    }

    void EaglesAscentGame::ShowTest(DoorwaysGameHost& host, float realDt)
    {
        auto& world = host.World; auto& camera = world.MainCamera; const auto& p = world.MainPlayer;
        const auto table = std::filesystem::path(mLog.empty() ? std::string("showcase-evidence.tsv") : mLog).parent_path() / "showcase.tsv";
        const auto row = [&](const std::string& line) { std::ofstream(table, std::ios::app) << line << '\n'; };
        mShowClock += realDt;
        if (mShowStep < 0)
        {   // the map's ground up, then into the first nest, her window opened
            if (mClock < 6 || mMap.Nests.empty() || !world.RuntimeScene || !world.RuntimeScene->Loaded) return;
            std::ofstream(table) << "seconds\tview\tturn\ttilt\tdist\tcam_x\tcam_y\tcam_z\ther_x\ther_y\ther_z\tresult\n";
            mMap.Visit(host, mMap.Nests[0].Station);
            mGrowth.Gear = 0; mFormShown = 0;   // (no burst of her becoming: it would hang in the still air of the window)
            mShowStep = 0; mShowClock = 0;
            return;
        }
        struct View { const char* Name; float Turn, Tilt, Dist; int Gear; };
        static const View views[] = {
            {"front", 0.f, -.08f, 3.9f, 0}, {"front_highlands", 0.f, -.08f, 3.9f, 1}, {"left", 1.57f, -.08f, 3.9f, 1},
            {"back", 3.14f, -.08f, 3.9f, 1}, {"right", 4.71f, -.08f, 3.9f, 1}, {"moonlit_close", .2f, -.05f, 1.7f, 3},
            {"fire_far", .6f, -.45f, 6.f, 2}, {"from_below", .35f, .25f, 2.6f, 3}};
        constexpr float Each = 3.f;
        const int at = static_cast<int>(mShowClock / Each);
        if (at == 0 && !mGrowthOpen && mShowClock > .5f) mGrowthOpen = true;
        if (at >= static_cast<int>(std::size(views)))
        {   // closed: she is back where she stood, in the nest, on the ground
            if (!mShowClosed) { mShowWas = mStudioFrom.Position; mShowClosed = true; mGrowthOpen = false; return; }
            if (mStudio) return;   // (put back as the window closes, after this)
            const bool back = !mStudio && std::hypot(p.Position.x - mShowWas.x, p.Position.y - mShowWas.y, p.Position.z - mShowWas.z) < .01f && NestAt(p.Position) > 0 && p.Position.y < StudioHeight - 100;
            if (!back) ++mShowBad;
            std::ostringstream line; line << std::fixed << std::setprecision(2) << mShowClock << "	closed_back_where_she_stood							" << p.Position.x << '	' << p.Position.y << '	' << p.Position.z << '	' << (back ? "ok" : "FAIL");
            row(line.str());
            row(mShowBad == 0 ? "SHOWCASE_PASS" : "SHOWCASE_FAIL");
            mShowTest = false; PostQuitMessage(mShowBad == 0 ? 0 : 2);
            return;
        }
        const View& v = views[at];
        if (at != mShowStep || mShowClock < .6f)
        {
            mShow.Turn = v.Turn; mShow.Tilt = v.Tilt; mShow.Dist = v.Dist;
            mGrowth.Gear = v.Gear; mFormShown = v.Gear;
        }
        if (at != mShowStep) { mShowStep = at; }
        if (std::fmod(mShowClock, Each) > 2.2f && std::fmod(mShowClock - realDt, Each) <= 2.2f)
        {   // the view settled: the camera before her at its distance, over the ground, looking at her
            const float dx = camera.Position.x - p.Position.x, dz = camera.Position.z - p.Position.z, dist = std::hypot(dx, camera.Position.y - p.Position.y - 1.2f, dz);
            const float facing = std::atan2(camera.Position.x - p.Position.x, camera.Position.z - p.Position.z);   // where the camera stands from her
            const float off = std::remainder(facing - p.Yaw - v.Turn, XM_2PI);
            const bool ok = mGrowthOpen && mShow.On && GameSession().FreeCursor && dist > .35f && dist < v.Dist * 1.5f && std::abs(off) < .25f &&
                            camera.Position.y > mMap.Land(camera.Position.x, camera.Position.z);
            if (!ok) ++mShowBad;
            if (!mTourShots.empty()) { std::string n = v.Name; mTourShot = std::wstring(n.begin(), n.end()); }   // (its picture)
            std::ostringstream line; line << std::fixed << std::setprecision(2) << mShowClock << '\t' << v.Name << '\t' << v.Turn << '\t' << v.Tilt << '\t' << v.Dist << '\t'
                << camera.Position.x << '\t' << camera.Position.y << '\t' << camera.Position.z << '\t' << p.Position.x << '\t' << p.Position.y << '\t' << p.Position.z << '\t' << (ok ? "ok" : "FAIL");
            row(line.str());
        }
    }

    // The pointer test (CAUCASIAN_EAGLE_DEMO=7, 2026-11-21; 0.5.3: her window shows her gear, nothing in it to buy): her
    // window (I) and her menu (Esc) worked by a pointer this test moves and presses in the desktop's place (ReadMouse).
    // Each step waits a few frames, so that the windows are drawn and their keys and rows are where the pointer looks.
    void EaglesAscentGame::PointerTest(DoorwaysGameHost& host, float dt)
    {
        auto& session = GameSession(); auto& world = host.World;
        if (mClock < 4 || !world.RuntimeScene || !world.RuntimeScene->Loaded || mHudW <= 0) return;
        if (mPtHold > 0) { mPtHold -= dt; return; }
        if (mPtWait > 0) { --mPtWait; return; }
        const auto table = std::filesystem::path(mLog).parent_path() / "pointer.tsv";
        const auto row = [&](const std::string& line) { std::ofstream(table, std::ios::app) << line << '\n'; };
        const auto check = [&](bool ok, const std::string& what) { row(what + (ok ? "\tok" : "\tFAIL")); if (!ok) ++mPtBad; };
        const auto point = [&](int group, int index, const std::string& what) { check(Pointer::Centre(group, index, mPtX, mPtY), what + "_found"); };
        constexpr int Frames = 4;
        const int step = mPtStep++;
        mPtWait = Frames;
        switch (step)
        {
        case 0:   // her window, open on the shore
            std::ofstream(table) << "step\tresult\n";
            mGrowthOpen = true; Sound::PlayUi("ui/open", .6f);
            mPtWait = 30;
            break;
        case 1:   // where there is nothing of the window's (over her, between the panels): a click turns her, nothing more
            check(session.FreeCursor, "pointer_free_while_open");
            mPtX = mHudW * .5f; mPtY = mHudH * .5f; mPtDown = false; mPtCrystals = AscentGrowth::AllCrystals(mGrowth); break;
        case 2: mPtDown = true; break;
        case 3: check(mGrowthOpen && AscentGrowth::AllCrystals(mGrowth) == mPtCrystals, "click_on_nothing_does_nothing"); mPtDown = false; break;
        case 4: row("picture"); mPtHold = 3; break;
        case 5: point(Pointer::CloseKey, 0, "close_key"); break;
        case 6: mPtDown = true; break;   // CLOSE clicked, the button held
        case 7: check(!mGrowthOpen && !session.FreeCursor && mHoldBlade, "close_clicked_window_closed_pointer_caught_swords_wait"); break;
        case 8: mPtDown = false; break;
        case 9: check(!mHoldBlade, "swords_free_once_let_go"); session.EscapePressed = true; break;   // her menu
        case 10: check(mPaused && session.FreeCursor, "menu_open_pointer_free"); point(Pointer::PauseRow, 3, "menu_row_3"); break;
        case 11: check(mMenuRow == 3, "menu_row_3_chosen"); point(Pointer::PauseRow, 0, "menu_row_0"); break;
        case 12: check(mMenuRow == 0, "menu_row_0_chosen"); mPtDown = true; break;   // Continue the climb, clicked
        case 13: check(!mPaused && !session.FreeCursor && mHoldBlade, "continue_clicked_menu_closed_swords_wait"); mPtDown = false; break;
        case 14: check(!mHoldBlade, "swords_free_once_let_go_2"); session.EscapePressed = true; break;
        case 15: point(Pointer::PauseRow, 1, "menu_row_1"); break;
        case 16: check(mMenuRow == 1, "menu_row_1_chosen"); mPtCrystals = AscentGrowth::AllCrystals(mGrowth); mPtDown = true; break;   // Nyxara, clicked
        case 17: check(mGrowthOpen && !mPaused && session.FreeCursor && AscentGrowth::AllCrystals(mGrowth) == mPtCrystals, "nyxara_taken_window_open_pointer_free"); mPtDown = false; break;
        case 18: session.EscapePressed = true; break;   // Esc closes it
        case 19:
            check(!mGrowthOpen && !session.FreeCursor, "esc_closed_window_pointer_caught");
            row(mPtBad == 0 ? "POINTER_PASS" : "POINTER_FAIL\tbad=" + std::to_string(mPtBad));
            Evidence(world, mPtBad == 0 ? "POINTER_PASS" : "POINTER_FAIL");
            mPointerTest = false; PostQuitMessage(mPtBad == 0 ? 0 : 2);
            break;
        default: break;
        }
    }

    // The quests' test (CAUCASIAN_EAGLE_DEMO=11): see EaglesAscentGame.h.
    void EaglesAscentGame::QuestTest(DoorwaysGameHost& host, float dt)
    {
        auto& session = GameSession(); auto& world = host.World;
        if (mClock < 4 || !world.RuntimeScene || !world.RuntimeScene->Loaded || mHudW <= 0 || mMap.Nests.size() < 8 || mMap.Trail.size() < 100 ||
            (mCreaturesOn && !mAroundPlaced)) return;
        if (mPtHold > 0) { mPtHold -= dt; return; }
        if (mPtWait > 0) { --mPtWait; return; }
        const auto table = std::filesystem::path(mLog).parent_path() / "quests.tsv";
        const auto row = [&](const std::string& line) { std::ofstream(table, std::ios::app) << line << '\n'; };
        const auto check = [&](bool ok, const std::string& what) { row(what + (ok ? "\tok" : "\tFAIL")); if (!ok) ++mPtBad; };
        const auto picture = [&](const std::string& name) { row("picture\t" + name); mPtHold = 2.5f; };   // (the runner's, with the HUD)
        const auto stand = [&]()
        {   // on the way a little up it, out of any nest, facing up it
            size_t i = mMap.Trail.size() / 12;
            while (i + 2 < mMap.Trail.size() && NestAt(mMap.Trail[i]) > 0) i += 5;
            const XMFLOAT3 a = mMap.Trail[i], b = mMap.Trail[i + 1];
            const float y = std::isnan(mMap.TreadTop(a.x, a.z, a.y)) ? a.y : mMap.TreadTop(a.x, a.z, a.y);
            const float yaw = std::atan2(b.x - a.x, b.z - a.z);
            Put(host, {a.x, y, a.z}, yaw);
            world.MainCamera.Yaw = yaw; world.MainCamera.Pitch = .12f;
        };
        const auto quest = [&](const char* id) { return AscentQuests::QuestIndex(id); };
        const auto state = [&](const char* id) { const int q = quest(id); return q >= 0 ? mGrowth.QuestState[q] : -1; };
        const auto counted = [&](const char* id) { const int q = quest(id); return q >= 0 ? mGrowth.QuestCount[q] : -1; };
        const auto point = [&](int group, int index, const std::string& what) { check(Pointer::Centre(group, index, mPtX, mPtY), what + "_found"); };
        const int wind = AscentGrowth::WindCrystal, fire = AscentGrowth::FireCrystal, ice = AscentGrowth::IceCrystal;
        const auto listed = [&]() { return AscentQuests::Listed(mGrowth, mQuestLeaving).size(); };   // (the HUD's list: its rows)
        constexpr int Frames = 4;
        const int step = mPtStep++;
        mPtWait = Frames;
        switch (step)
        {
        case 0:
            std::ofstream(table) << "step\tresult\n";
            check(mGrowth.Gear == 0 && AscentQuests::OpenSet(mGrowth) == 0, "a_new_climb_gear_0_first_set_open");
            stand(); mPtWait = 90;
            break;
        case 1:   // a quest not taken counts nothing (the owner: only once taken)
            CountQuests(AscentQuests::Beat, 5);
            check(counted("wind.1") == 0 && state("wind.1") == AscentQuests::Offered, "not_taken_not_counted");
            mGrowth.AscentCrystals[wind] = 40; mGrowth.AscentCrystals[fire] = 20;   // (8 + 12 + 18 for the three)
            picture("hud_none_taken");
            break;
        case 2: mTestKey = VK_TAB; break;   // Tab, anywhere (Q until 0.5.4)
        case 3:
            check(mQuestsOpen && session.FreeCursor && mQuestRow == 0 && AscentQuests::Rows(0).size() == 3, "tab_opens_the_first_three");
            picture("quests_offered");
            break;
        case 4: mTestKey = VK_RETURN; break;   // the first, with Enter
        case 5:
            check(state("wind.1") == AscentQuests::Taken && mGrowth.AscentCrystals[wind] == 32, "enter_took_wind_1_for_8");
            point(Pointer::QuestTake, 1, "take_key_of_row_2");
            break;
        case 6: mPtDown = true; break;   // the second, with a click
        case 7:
            check(state("wind.2") == AscentQuests::Taken && mGrowth.AscentCrystals[wind] == 20 && mQuestRow == 1, "click_took_wind_2_for_12");
            mPtDown = false;
            break;
        case 8: mTestKey = 'S'; break;
        case 9: check(mQuestRow == 2, "s_chose_row_3"); mGrowth.AscentCrystals[wind] = 10; mTestKey = VK_RETURN; break;   // (too few for it)
        case 10:
            check(state("wind.3") == AscentQuests::Offered && mGrowth.AscentCrystals[wind] == 10, "too_few_crystals_not_taken");
            mGrowth.AscentCrystals[wind] = 20; mTestKey = VK_RETURN;
            break;
        case 11: check(state("wind.3") == AscentQuests::Taken && mGrowth.AscentCrystals[wind] == 2, "enter_took_wind_3_for_18"); picture("quests_taken"); break;
        case 12: point(Pointer::CloseKey, 0, "close_key"); break;
        case 13: mPtDown = true; break;
        case 14: check(!mQuestsOpen && mHoldBlade && !session.FreeCursor, "close_clicked"); mPtDown = false; break;
        case 15:   // counted only once taken: four creatures, six jars, the fourth nest
            CountQuests(AscentQuests::Beat, 3);
            check(counted("wind.1") == 3 && state("wind.1") == AscentQuests::Taken, "three_beaten_counted");
            check(listed() == 3, "three_taken_three_listed");
            picture("hud_taken");
            break;
        case 16:
            CountQuests(AscentQuests::Beat, 1); CountQuests(AscentQuests::Jars, 6);
            check(state("wind.1") == AscentQuests::Done && state("wind.2") == AscentQuests::Done && counted("fire.1") == 0, "beat_4_and_jars_6_done");
            ReachNest(mMap.Nests[2].Station);
            check(state("wind.3") == AscentQuests::Taken && mGrowth.Gear == 0, "the_third_nest_is_not_the_fourth");
            check(listed() == 3, "two_done_their_rows_still_going");   // (gold, a line through them, then out: the picture is of that)
            picture("hud_two_done");
            break;
        case 17:   // 2.5 s on: the done ones are gone from the list, and done still (the market keeps them)
            check(listed() == 1 && state("wind.1") == AscentQuests::Done && state("wind.2") == AscentQuests::Done, "done_rows_gone_from_the_list");
            picture("hud_done_gone");
            break;
        case 18:
            ReachNest(mMap.Nests[3].Station);
            check(state("wind.3") == AscentQuests::Done, "reach_nest_4_done");
            break;
        case 19:   // all three done: THE HIGHLANDS at once, saved where she last saved (the shore, in this new climb)
            check(mGrowth.Gear == 1 && AscentQuests::OpenSet(mGrowth) == 1, "three_done_gear_1_at_once");
            check(mSavedGrowth.Gear == 1 && mSavedNest == 0, "saved_waking_at_her_last_nest");
            mPtWait = 60;
            break;
        case 20: check(mFormShown == 1, "gear_moment_played"); picture("gear_1_earned"); break;
        case 21: mGrowthOpen = true; mPtWait = 30; break;   // her window: her gears, how far her quests are
        case 22: picture("window_gears"); break;
        case 23: mGrowthOpen = false; mPtWait = 20; break;
        case 24:
        {   // read back
            LoadSave();
            const int w1 = quest("wind.1");
            check(mSavedGrowth.Gear == 1 && w1 >= 0 && mSavedGrowth.QuestState[w1] == AscentQuests::Done && mSavedGrowth.AscentCrystals[fire] == 20, "reload_gear_and_quests");
            mGrowth.AscentCrystals[fire] = 100;
            mTestKey = VK_TAB;
            break;
        }
        case 25:
            check(mQuestsOpen && AscentQuests::Rows(1).size() == 6 && mQuestRow == 0, "the_next_six_offered");
            picture("quests_second_set");
            mTestKey = 'W';   // (up from the first: round to the last, reach the seventh nest)
            break;
        case 26: check(mQuestRow == 5, "w_round_to_row_6"); mTestKey = VK_RETURN; break;
        case 27: check(state("fire.6") == AscentQuests::Taken && mGrowth.AscentCrystals[fire] == 68, "fire_6_taken_for_32"); mTestKey = VK_TAB; break;
        case 28:
            ReachNest(mMap.Nests[7].Station);   // the eighth: above the seventh it asks
            check(!mQuestsOpen && state("fire.6") == AscentQuests::Done, "a_nest_above_the_one_asked_counts");
            mTestKey = VK_TAB;
            break;
        case 29: check(mQuestsOpen && mQuestRow == 0, "tab_again_on_the_first_offered"); mTestKey = VK_RETURN; break;
        case 30: check(state("fire.1") == AscentQuests::Taken && mGrowth.AscentCrystals[fire] == 56, "fire_1_taken_for_12"); mTestKey = VK_TAB; break;
        case 31:
        {   // six bought: six rows (until 0.5.4 the list had five at the most). The owner, 2026-11-26: "i bought 6 or 9
            // quests - it only showed 5". (The sixth is done already: for the picture it is on the way again.)
            mGrowth.AscentCrystals[fire] = 500;
            for (const int i : AscentQuests::Rows(1)) TakeQuest(i);
            mGrowth.QuestState[quest("fire.6")] = AscentQuests::Taken;
            check(listed() == 6, "six_taken_six_listed");
            picture("hud_six_taken");
            break;
        }
        case 32:   // nine bought (the third set, the second as if she had done it): nine rows
            for (const int i : AscentQuests::Rows(1)) mGrowth.QuestState[i] = AscentQuests::Done;
            mQuestLeaving = {};
            mGrowth.Gear = 2; mGrowth.AscentCrystals[ice] = 500;
            for (const int i : AscentQuests::Rows(2)) TakeQuest(i);
            check(AscentQuests::OpenSet(mGrowth) == 2 && listed() == 9, "nine_taken_nine_listed");
            picture("hud_nine_taken");
            break;
        case 33:   // one of the nine done (five creatures): its row says so and goes
            CountQuests(AscentQuests::Beat, 5);
            check(state("ice.1") == AscentQuests::Done && listed() == 9, "one_of_nine_done_its_row_going");
            picture("hud_nine_one_done");
            break;
        case 34: check(listed() == 8 && state("ice.1") == AscentQuests::Done, "gone_from_the_list_of_nine"); mTestKey = VK_TAB; break;
        case 35:   // the market keeps it, as done (and the nine rows and its keys fit the window)
            check(mQuestsOpen && state("ice.1") == AscentQuests::Done && AscentQuests::Rows(2).size() == 9, "the_market_keeps_the_done_one");
            picture("quests_nine_one_done");
            break;
        case 36: mTestKey = VK_TAB; break;
        case 37:   // she falls: back to the save, the quests after it undone, her crystals as they were
            check(!mQuestsOpen, "tab_closed_the_market");
            RollBack();
            check(state("fire.1") == AscentQuests::Offered && state("fire.6") == AscentQuests::Offered && mGrowth.AscentCrystals[fire] == 20 && mGrowth.Gear == 1 &&
                  state("wind.3") == AscentQuests::Done, "rollback_undoes_the_quests_after_the_save");
            row(mPtBad == 0 ? "QUESTS_PASS" : "QUESTS_FAIL\tbad=" + std::to_string(mPtBad));
            Evidence(world, mPtBad == 0 ? "QUESTS_PASS" : "QUESTS_FAIL");
            mQuestTest = false; PostQuitMessage(mPtBad == 0 ? 0 : 2);
            break;
        default: break;
        }
    }

    // The crystals' ledger (CAUCASIAN_EAGLE_DEMO=8): see EaglesAscentGame.h.
    void EaglesAscentGame::Ledger(DoorwaysGameHost& host)
    {
        auto& world = host.World;
        if (!mWayside.Laid() || (mCreaturesOn && !mAroundPlaced)) return;
        const auto table = std::filesystem::path(mLog).parent_path() / "ledger.tsv";
        std::ofstream out(table, std::ios::trunc);
        int bad = 0;
        const auto fail = [&](const std::string& why) { out << "FAIL\t" << why << '\n'; ++bad; };
        const int nests = static_cast<int>(mMap.Nests.size());
        if (mMap.Levels.size() != static_cast<size_t>(nests) || nests < AscentMaxLevel) fail("jar_budgets_" + std::to_string(mMap.Levels.size()) + "_nests_" + std::to_string(nests));
        std::vector<int> wayCreatures(nests + 2, 0), bosses(nests + 2, 0), caves(nests + 2, 0), around(nests + 2, 0), wayJars(nests + 2, 0), sideJars(nests + 2, 0), groups(nests + 2, 0);
        int placed = 0, aroundGroups = 0;
        for (int g = 0; g < static_cast<int>(mMap.Placements.size()); ++g)
        {
            const auto& pl = mMap.Placements[static_cast<size_t>(g)];
            int crystals = 0;
            for (const auto& kind : pl.Kinds) crystals += Journey::KindCrystals(Journey::KindByName(kind));
            const int k = std::clamp(GroupStretch(g), 1, nests + 1);   // (the stretch whose kind its creatures leave)
            if (pl.Boss) bosses[k] += crystals;
            else if (pl.Cave >= 0) caves[k] += crystals;
            else if (pl.Around) { around[k] += crystals; ++groups[k]; ++aroundGroups; if (!pl.Spur.empty()) ++placed; }
            else wayCreatures[k] += crystals;
        }
        for (const auto& j : mWayside.Jars())
        {
            const int k = std::clamp(j.Stretch, 1, nests + 1);
            (j.Way ? wayJars : sideJars)[k] += j.Crystals;
        }
        // (0.5.3: no prices of levels now; what lies in each stretch, its kind's, is what her quests will cost)
        out << "stretch\tto_nest\tkind\tway_creatures\tbosses\tway_jars\tway\tcave_creatures\taround_creatures\taround_groups\tside_jars\tside\ttotal\tway_part\n";
        int kinds[AscentGrowth::CrystalKinds] = {};
        for (int k = 1; k <= nests + 1; ++k)
        {
            const int way = wayCreatures[k] + bosses[k] + wayJars[k], side = caves[k] + around[k] + sideJars[k], total = way + side;
            const float part = total > 0 ? static_cast<float>(way) / static_cast<float>(total) : 0.f;
            const int kind = AscentGrowth::CrystalKindOf(k);
            kinds[kind] += total;
            out << k << '\t' << (k <= nests ? std::to_string(k) : std::string("summit")) << '\t' << AscentGrowth::CrystalId(kind) << '\t' << wayCreatures[k] << '\t'
                << bosses[k] << '\t' << wayJars[k] << '\t' << way << '\t' << caves[k] << '\t' << around[k] << '\t' << groups[k] << '\t' << sideJars[k] << '\t' << side << '\t'
                << total << '\t' << std::fixed << std::setprecision(3) << part << '\n';
            if (k > nests) continue;   // (past the last nest: the summit, its boss and none in jars)
            if (part < .60f || part > .70f) fail("stretch_" + std::to_string(k) + "_way_part_" + std::to_string(part));
            const auto& budget = mMap.Levels[static_cast<size_t>(k - 1)];
            if (wayJars[k] != budget.WayJars) fail("stretch_" + std::to_string(k) + "_way_jars_" + std::to_string(wayJars[k]) + "_not_" + std::to_string(budget.WayJars));
            if (sideJars[k] != budget.SideJars) fail("stretch_" + std::to_string(k) + "_side_jars_" + std::to_string(sideJars[k]) + "_not_" + std::to_string(budget.SideJars));
        }
        out << "kinds\twind\t" << kinds[0] << "\tfire\t" << kinds[1] << "\tice\t" << kinds[2] << '\n';
        for (int kind = 0; kind < AscentGrowth::CrystalKinds; ++kind)
            if (kinds[kind] <= 0) fail(std::string("no_crystals_of_kind_") + std::to_string(kind));
        if (AscentGrowth::CrystalKindOf(CrystalNests[0]) != AscentGrowth::WindCrystal || AscentGrowth::CrystalKindOf(CrystalNests[0] + 1) != AscentGrowth::FireCrystal ||
            AscentGrowth::CrystalKindOf(CrystalNests[1]) != AscentGrowth::FireCrystal || AscentGrowth::CrystalKindOf(CrystalNests[1] + 1) != AscentGrowth::IceCrystal)
            fail("kinds_not_changing_at_nests_3_and_6");
        if (wayJars[nests + 1] + sideJars[nests + 1] != 0) fail("jar_crystals_past_the_last_nest_" + std::to_string(wayJars[nests + 1] + sideJars[nests + 1]));
        if (mWayside.Unplaced != 0) fail("jar_crystals_with_no_jar_" + std::to_string(mWayside.Unplaced));
        out << "around_groups\t" << aroundGroups << "\tplaced\t" << placed << '\n';
        if (placed != aroundGroups) fail("around_not_placed_" + std::to_string(aroundGroups - placed));
        // and every group around the way: where it stands, how far off the way, how high over it; the gaps between them
        // (the owner: "every 150-400 meter"; never under 140 m, over 400 m only where there is no ground for one)
        for (const auto& line : mAroundReport) out << line << '\n';
        float last = -1; int longer = 0;
        for (const auto& pl : mMap.Placements)
        {
            if (!pl.Around || pl.Spur.empty()) continue;
            if (last >= 0)
            {
                const float gap = pl.At - last;
                if (gap < 140 - .5f) fail("around_gap_" + std::to_string(static_cast<int>(gap)) + "_before_" + pl.Id);
                if (gap > 400 + .5f) ++longer;
            }
            last = pl.At;
        }
        out << "around_gaps_over_400\t" << longer << '\n';
        // and each a fight of its own (EaglesAscent::PlaceAround; the creatures' tour of 2026-11-21 found two groups 14 m
        // apart one fight): no other group's place, nor the mouth of a cave with creatures in it, within 26 m of its place
        // across the ground, and from where its line leaves the way (Spur[0]) it the nearest group by 8 m and no group
        // further up the way, or in a cave, within 26 m across the ground
        const auto apart = [](const XMFLOAT3& u, const XMFLOAT3& v) { return std::sqrt((u.x - v.x) * (u.x - v.x) + (u.y - v.y) * (u.y - v.y) + (u.z - v.z) * (u.z - v.z)); };
        const auto flat = [](const XMFLOAT3& u, const XMFLOAT3& v) { return std::hypot(u.x - v.x, u.z - v.z); };
        float least = 1e9f, margin = 1e9f, start = 1e9f, room = 1e9f; std::string leastId, marginId, startId, roomId;
        for (const auto& pl : mMap.Placements)
        {
            if (!pl.Around || pl.Spur.empty()) continue;
            const float fromWay = apart(pl.Spur[0], pl.P);
            for (const auto& q : mMap.Placements)
            {
                if (q.Around) continue;
                if (const float d = flat(q.P, pl.P); d < least) { least = d; leastId = pl.Id; }
                if (const float m = apart(q.P, pl.Spur[0]) - fromWay; m < margin) { margin = m; marginId = pl.Id; }
                if (q.Cave >= 0 || q.At > pl.At)
                    if (const float s = flat(q.P, pl.Spur[0]); s < start) { start = s; startId = pl.Id; }
                // (and 100 m from a cave's group when the two are more than the three alive at once: else the one holds
                // the three places while she fights the other, and that one never wakes)
                if (q.Cave >= 0 && pl.Kinds.size() + q.Kinds.size() > 3)
                    if (const float r = apart(q.P, pl.P); r < room) { room = r; roomId = pl.Id + "_" + q.Id; }
                if (q.Cave < 0 || q.Cave >= static_cast<int>(mMap.Caves().size())) continue;
                const auto& cave = mMap.Caves()[static_cast<size_t>(q.Cave)];
                if (cave.Line.empty()) continue;
                for (int end = 0; end < 2; ++end)
                {
                    if (!cave.Open[end]) continue;
                    const auto& c = end == 0 ? cave.Line.front() : cave.Line.back();
                    if (const float d = flat({c.X, c.Y, c.Z}, pl.P); d < least) { least = d; leastId = pl.Id; }
                }
            }
        }
        out << "around_nearest_other_group\t" << static_cast<int>(least) << "\t" << leastId << '\n';
        out << "around_nearest_from_the_way_by\t" << static_cast<int>(margin) << "\t" << marginId << '\n';
        out << "around_group_further_up_from_its_line_start\t" << static_cast<int>(start) << "\t" << startId << '\n';
        out << "around_from_a_cave_group_too_many_with_it\t" << (room > 1e8f ? -1 : static_cast<int>(room)) << "\t" << roomId << '\n';
        if (room < 100.f - .5f) fail("around_" + roomId + "_" + std::to_string(static_cast<int>(room)) + "m_apart_more_than_three");
        if (least < 26.f - .5f) fail("around_" + leastId + "_" + std::to_string(static_cast<int>(least)) + "m_from_another_group");
        if (start < 26.f - .5f) fail("around_" + startId + "_" + std::to_string(static_cast<int>(start)) + "m_from_a_group_further_up_where_its_line_leaves_the_way");
        if (margin < 8.f - .5f) fail("around_" + marginId + "_not_the_nearest_from_the_way");
        out << (bad == 0 ? "LEDGER_PASS" : "LEDGER_FAIL\tbad=" + std::to_string(bad)) << '\n';
        out.close();
        Evidence(world, bad == 0 ? "LEDGER_PASS" : "LEDGER_FAIL");
        mLedger = false; PostQuitMessage(bad == 0 ? 0 : 2);
    }

    void EaglesAscentGame::Ways(DoorwaysGameHost& host, float dt)
    {
        auto& world = host.World;
        mWaysKeys = {};
        if (mClock < 3 || !mWayside.Laid()) return;
        mWaysClock += dt;
        host.Combat.RestorePlayerHealth(world, 10000);
        const auto table = std::filesystem::path(mLog).parent_path() / "ways.tsv";
        const auto row = [&](const std::string& line) { std::ofstream(table, std::ios::app) << line << '\n'; };
        const auto& jars = mWayside.Jars();
        const auto group = [&](size_t j) { const std::string& id = jars[j].Id; return id.substr(0, id.rfind('.')); };
        const auto brokenIn = [&](size_t j)
        {
            int n = 0; const std::string g = group(j);
            for (size_t i = j; i < jars.size() && group(i) == g; ++i) n += jars[i].Broken ? 1 : 0;
            return n;
        };
        if (mWaysPhase == 0)
        {
            int roads[3] = {}, potions = 0, crystals = 0, groups = 0;
            for (size_t i = 0; i < jars.size(); ++i)
            {
                roads[jars[i].Id[0] == 't' ? 0 : jars[i].Id[0] == 'c' ? 1 : 2]++;
                potions += jars[i].Potion ? 1 : 0; crystals += jars[i].Crystals;
                if (i == 0 || group(i) != group(i - 1)) ++groups;
            }
            std::ofstream(table, std::ios::trunc) << "what\tvalues\n";
            row("laid\tjars=" + std::to_string(jars.size()) + "\tgroups=" + std::to_string(groups) + "\tway=" + std::to_string(roads[0]) + "\tcaves=" +
                std::to_string(roads[1]) + "\trings=" + std::to_string(roads[2]) + "\tcrystals_in_them=" + std::to_string(crystals) + "\tpotions_in_them=" +
                std::to_string(potions));
            if (jars.size() < 100) { ++mWaysBad; row("FEW_JARS"); }
            // eight groups of the way, from the shore to the top, and two in caves
            std::vector<size_t> starts, caves;
            for (size_t i = 0; i < jars.size(); ++i)
                if (i == 0 || group(i) != group(i - 1)) (jars[i].Id[0] == 'c' ? caves : starts).push_back(i);
            for (int k = 0; k < 8 && !starts.empty(); ++k) mWaysPicks.push_back(starts[(starts.size() - 1) * static_cast<size_t>(k) / 7]);
            for (int k = 0; k < 2 && !caves.empty(); ++k) mWaysPicks.push_back(caves[(caves.size() - 1) * static_cast<size_t>(k)]);
            mWaysPhase = 1; mWaysPick = 0; mWaysClock = 0;
            return;
        }
        if (mWaysPhase == 1)
        {
            if (mWaysPick >= static_cast<int>(mWaysPicks.size())) { mWaysPhase = 2; mWaysClock = 0; return; }
            const size_t j = mWaysPicks[static_cast<size_t>(mWaysPick)];
            const XMFLOAT3 jar = jars[j].P;
            if (mWaysClock < dt * 1.5f)
            {   // she stands 1.5 m from the group's middle jar, on the road's side of it, facing it
                XMFLOAT3 road = jar; float best = 1e30f;
                const auto nearest = [&](const XMFLOAT3& q) { const float d = (q.x - jar.x) * (q.x - jar.x) + (q.z - jar.z) * (q.z - jar.z); if (d < best && std::abs(q.y - jar.y) < 4) { best = d; road = q; } };
                if (jars[j].Id[0] == 'c') { for (const auto& cave : mMap.Caves()) for (const auto& q : cave.Line) nearest({q.X, q.Y, q.Z}); }
                else for (const auto& q : mMap.Trail) nearest(q);
                float ax = road.x - jar.x, az = road.z - jar.z; const float l = std::max(.01f, std::sqrt(ax * ax + az * az)); ax /= l; az /= l;
                // (with her slide: on the way itself where it is nearest the jar, at the way's own height; a group of the
                // rings has no way within 6 m of it, and she stands by it as for her cuts)
                const XMFLOAT3 at = mWaysDash && best < 36.f ? road : XMFLOAT3{jar.x + ax * 1.5f, jar.y, jar.z + az * 1.5f};
                mWaysFrom = std::hypot(at.x - jar.x, at.z - jar.z);
                Put(host, at, std::atan2(-ax, -az));
                world.MainCamera.Yaw = std::atan2(-ax, -az);
                std::copy(std::begin(mGrowth.AscentCrystals), std::end(mGrowth.AscentCrystals), std::begin(mWaysHad));
            }
            if (mWaysClock > 1.4f && mWaysClock - dt <= 1.4f) mTourShot = L"jars_" + std::to_wstring(mWaysPick) + L"_whole";
            if (mWaysDash) mWaysKeys.K1 = mWaysClock > 1.6f && mWaysClock < 1.7f;   // (F once)
            else for (const float at : {1.6f, 2.3f, 3.0f}) if (mWaysClock > at && mWaysClock < at + .1f) mWaysKeys.Lmb = true;
            if (mWaysClock > 4.2f && mWaysClock - dt <= 4.2f) mTourShot = L"jars_" + std::to_wstring(mWaysPick) + L"_broken";
            if (mWaysClock > 4.4f)
            {
                const int broken = brokenIn(j);
                const auto& her = world.MainPlayer.Position;
                // what it gave: only crystals of its stretch's kind (a potion she has no room for: of where she is, the same)
                const int kind = AscentGrowth::CrystalKindOf(jars[j].Stretch);
                int others = 0, gained = 0;
                for (int k = 0; k < AscentGrowth::CrystalKinds; ++k)
                {
                    const int d = mGrowth.AscentCrystals[k] - mWaysHad[k];
                    (k == kind ? gained : others) += d; mWaysKinds[k] += d;
                }
                std::ostringstream line; line << std::fixed << std::setprecision(2) << "cut\t" << group(j) << "\tbroken=" << broken << "\tstretch=" << jars[j].Stretch
                                              << "\tkind=" << AscentGrowth::CrystalId(kind)
                                              << "\tgained=" << gained << "\tcrystals=" << mGrowth.AscentCrystals[0] << "/" << mGrowth.AscentCrystals[1] << "/" << mGrowth.AscentCrystals[2]
                                              << "\tpotions=" << mGrowth.Potions << "\ther_dy=" << her.y - jar.y << "\tjar=" << jar.x << "," << jar.y << "," << jar.z;
                if (mWaysDash) line << "\tby_her_slide_from=" << mWaysFrom << "\tshe_went=" << std::hypot(her.x - jar.x, her.z - jar.z);
                if (mWaysDash && jars[j].Id[0] == 'c')
                {   // (in a cave she slid at its wall: she must be in the cave still)
                    const bool in = mMap.InCave(her);
                    line << "\tin_the_cave=" << (in ? 1 : 0);
                    if (!in) { ++mWaysBad; line << "\tOUT_OF_THE_CAVE"; }
                }
                if (broken == 0) { ++mWaysBad; line << "\tNONE_BROKEN"; }
                if (others != 0) { ++mWaysBad; line << "\tOTHER_KIND_" << others; }
                row(line.str());
                ++mWaysPick; mWaysClock = 0;
            }
            return;
        }
        if (mWaysPhase == 2)
        {   // saved at the first nest; then more jars broken; then she falls (RollBack): all as saved
            const int nest = mMap.Nests.empty() ? 0 : mMap.Nests.front().Station;
            SaveHere(nest);
            const Growth::State saved = mGrowth;
            const size_t brokenSaved = mWayside.BrokenIds().size();
            int more = 0;
            for (const auto& jar : jars)
                if (!jar.Broken && more < 12) { Collect(mWayside.Strike(jar.P, 0, .2f, 359.f)); ++more; }
            for (int kind = 0; kind < AscentGrowth::CrystalKinds; ++kind)
            {   // and a jar of each kind's stretches that holds crystals, broken where it stands: its crystals of that kind only
                const Wayside::Jar* pick = nullptr;
                for (const auto& jar : jars)
                    if (!jar.Broken && jar.Crystals > 0 && AscentGrowth::CrystalKindOf(jar.Stretch) == kind) { pick = &jar; break; }
                if (!pick) { ++mWaysBad; row(std::string("kind_jar\t") + AscentGrowth::CrystalId(kind) + "\tNO_JAR"); continue; }
                int had[AscentGrowth::CrystalKinds]; std::copy(std::begin(mGrowth.AscentCrystals), std::end(mGrowth.AscentCrystals), had);
                const int held = pick->Crystals, stretch = pick->Stretch; const std::string id = pick->Id;
                const auto gain = mWayside.Strike(pick->P, 0, .2f, 359.f);
                const int potions = mGrowth.Potions;
                Collect(gain);
                int mine = 0, others = 0;
                for (int k = 0; k < AscentGrowth::CrystalKinds; ++k) (k == kind ? mine : others) += mGrowth.AscentCrystals[k] - had[k];
                const bool ok = mine >= held && (others == 0 || potions + gain.Potions > MaxPotions);   // (others: only a potion she had no room for)
                row(std::string("kind_jar\t") + AscentGrowth::CrystalId(kind) + "\t" + id + "\tstretch=" + std::to_string(stretch) + "\theld=" + std::to_string(held) +
                    "\tgained=" + std::to_string(mine) + "\tother_kinds=" + std::to_string(others) + (ok ? "" : "\tKIND_FAIL"));
                if (!ok) ++mWaysBad;
            }
            const int gained = AscentGrowth::AllCrystals(mGrowth) - AscentGrowth::AllCrystals(saved); const size_t brokenThen = mWayside.BrokenIds().size();
            RollBack();
            const bool back = std::equal(std::begin(mGrowth.AscentCrystals), std::end(mGrowth.AscentCrystals), std::begin(saved.AscentCrystals)) &&
                              mGrowth.Potions == saved.Potions && mWayside.BrokenIds().size() == brokenSaved;
            row("kinds_gained\twind=" + std::to_string(mWaysKinds[0]) + "\tfire=" + std::to_string(mWaysKinds[1]) + "\tice=" + std::to_string(mWaysKinds[2]));
            row("rollback\tsaved_crystals=" + std::to_string(AscentGrowth::AllCrystals(saved)) + "\tgained_after=" + std::to_string(gained) + "\tbroken_saved=" + std::to_string(brokenSaved) +
                "\tbroken_then=" + std::to_string(brokenThen) + "\tcrystals_now=" + std::to_string(AscentGrowth::AllCrystals(mGrowth)) +
                "\tbroken_now=" + std::to_string(mWayside.BrokenIds().size()) + (back && gained > 0 && brokenThen > brokenSaved ? "" : "\tROLLBACK_FAIL"));
            if (!(back && gained > 0 && brokenThen > brokenSaved)) ++mWaysBad;
            LoadSave();
            const bool read = mSavedNest == nest && std::equal(std::begin(mSavedGrowth.AscentCrystals), std::end(mSavedGrowth.AscentCrystals), std::begin(saved.AscentCrystals)) &&
                              mSavedGrowth.Gear == saved.Gear && mSavedJars.size() == brokenSaved;
            row("reload\tnest=" + std::to_string(mSavedNest) + "\tcrystals=" + std::to_string(mSavedGrowth.AscentCrystals[0]) + "/" +
                std::to_string(mSavedGrowth.AscentCrystals[1]) + "/" + std::to_string(mSavedGrowth.AscentCrystals[2]) + "\tjars=" + std::to_string(mSavedJars.size()) + (read ? "" : "\tRELOAD_FAIL"));
            if (!read) ++mWaysBad;
            row(mWaysBad == 0 ? "WAYS_PASS" : "WAYS_FAIL\tbad=" + std::to_string(mWaysBad));
            Evidence(world, mWaysBad == 0 ? "WAYS_PASS" : "WAYS_FAIL");
            mWays = false; PostQuitMessage(mWaysBad == 0 ? 0 : 2);
        }
    }

    void EaglesAscentGame::CameraTest(DoorwaysGameHost& host, float dt)
    {
        auto& world = host.World;
        if (mClock < 3 || !world.RuntimeScene || !world.RuntimeScene->Loaded) return;
        const auto table = std::filesystem::path(mLog).parent_path() / "camera.tsv";
        if (!mCamWalks.empty())
        {   // her walks up and down (stairs): the camera as a player has it, following her
            auto& session = GameSession(); auto& p = world.MainPlayer; auto& c = world.MainCamera;
            const auto walks = std::filesystem::path(mLog).parent_path() / "camera-walk.tsv";
            const auto trailAt = [&](float along) { return mMap.Trail[static_cast<size_t>(std::clamp(along / EaglesAscent::TrailStep, 0.f, static_cast<float>(mMap.Trail.size() - 1)))]; };
            if (mCamStep < 0) { std::ofstream(walks, std::ios::trunc) << "walk\tway\tt\talong\ther_y\tcamera_y\tcamera_distance\ther_on_screen\tview_pitch\ttrees_hiding_her\n"; mCamStep = 0; mCamWay = 0; mCamClock = -1; }
            if (mCamWalk >= static_cast<int>(mCamWalks.size()))
            {
                session.ScriptedMove = 0;
                std::ofstream(walks, std::ios::app) << "CAMERA_DONE\n"; Evidence(world, "CAMERA_DONE"); mCameraTest = false; PostQuitMessage(0); return;
            }
            const auto [from, to] = mCamWalks[static_cast<size_t>(mCamWalk)];
            const float start = mCamWay == 0 ? from : to, goal = mCamWay == 0 ? to : from, step = mCamWay == 0 ? 1.f : -1.f;
            if (mCamClock < 0)
            {   // at its foot (or top), facing the way she goes, the camera as it is at the start
                XMFLOAT3 at = trailAt(start), next = trailAt(start + 8.f * step); float y = at.y;
                if (Standable(*world.RuntimeScene, at, y)) at.y = y;
                Put(host, at, std::atan2(next.x - at.x, next.z - at.z));
                c.Pitch = CameraPitch; c.FollowDistance = CameraDistance;
                session.ScriptedMove = 0; mCamClock = 0; mCamShot = 0; mCamLastY = at.y;
                return;
            }
            mCamClock += dt;
            if (mCamClock < 1.f) return;   // (the camera settles)
            // where she is along the way, and on toward the goal
            float best = 1e30f; size_t closest = 0;
            for (size_t i = 0; i < mMap.Trail.size(); ++i)
            {
                const float d = std::hypot(mMap.Trail[i].x - p.Position.x, mMap.Trail[i].z - p.Position.z) + std::abs(mMap.Trail[i].y - p.Position.y) * .5f;
                if (d < best) { best = d; closest = i; }
            }
            const float along = static_cast<float>(closest) * EaglesAscent::TrailStep;
            const XMFLOAT3 aim = trailAt(along + 6.f * step);
            c.Yaw = std::atan2(aim.x - p.Position.x, aim.z - p.Position.z);
            session.ScriptedMove = 'W'; session.ScriptedFast = false;
            {
                const XMFLOAT3 chest{p.Position.x, p.Position.y + 1.2f, p.Position.z};
                const float dx = chest.x - c.Position.x, dy = chest.y - c.Position.y, dz = chest.z - c.Position.z, d = std::sqrt(dx * dx + dy * dy + dz * dz);
                const float off = (std::atan2(dy, std::hypot(dx, dz)) - (c.Pitch + c.ViewPitchOffset)) / (c.FieldOfViewY * .5f);
                char line[220]; std::snprintf(line, sizeof line, "%d\t%s\t%.3f\t%.1f\t%.3f\t%.3f\t%.2f\t%.2f\t%.3f\t%d\n", mCamWalk, mCamWay == 0 ? "up" : "down", mCamClock, along,
                                                p.Position.y, c.Position.y, d, off, c.Pitch + c.ViewPitchOffset, mTrees.SeeThrough);
                std::ofstream(walks, std::ios::app) << line;
            }
            mCamShot += dt;
            if (mCamShot >= .5f && !mTourShots.empty())
            {
                mCamShot = 0;
                wchar_t name[64]; swprintf_s(name, L"walk_%d_%s_%05.1f", mCamWalk, mCamWay == 0 ? L"up" : L"down", mCamClock); mTourShot = name;
            }
            if ((step > 0 ? along >= goal : along <= goal) || mCamClock > 60.f)
            {
                session.ScriptedMove = 0; mCamClock = -1;
                if (++mCamWay > 1) { mCamWay = 0; ++mCamWalk; }
            }
            return;
        }
        constexpr float Pitches[] = {-1.2f, -.8f, -.5f, -.25f, -.04f, .15f, .3f, .45f, .6f, .75f, .9f, 1.1f};
        constexpr int Steps = static_cast<int>(sizeof(Pitches) / sizeof(Pitches[0]));
        const float end = static_cast<float>(mMap.Trail.size() - 1) * EaglesAscent::TrailStep;
        if (mCamStep < 0)
        {
            std::ofstream(table, std::ios::trunc) << "along\tpitch\tset_pitch\tcamera_distance\tcamera_over_her\ther_on_screen\n";
            mCamAlong = 60.f; mCamStep = Steps;
        }
        if (mCamStep >= Steps)
        {   // the next place: on the way, facing up it, where she can stand
            if (mCamAlong > end - 20.f) { std::ofstream(table, std::ios::app) << "CAMERA_DONE\n"; Evidence(world, "CAMERA_DONE"); mCameraTest = false; PostQuitMessage(0); return; }
            const auto trailAt = [&](float along) { return mMap.Trail[static_cast<size_t>(std::clamp(along / EaglesAscent::TrailStep, 0.f, static_cast<float>(mMap.Trail.size() - 1)))]; };
            XMFLOAT3 p = trailAt(mCamAlong), q = trailAt(mCamAlong + 8.f); float y = p.y;
            if (Standable(*world.RuntimeScene, p, y)) p.y = y;
            Put(host, p, std::atan2(q.x - p.x, q.z - p.z));
            mCamStep = 0; mCamClock = 0; mCamAlong += mCamEvery;
            return;
        }
        mCamClock += dt;
        world.MainCamera.Pitch = Pitches[mCamStep]; world.MainCamera.Yaw = world.MainPlayer.Yaw;
        if (mCamClock > .9f && mCamClock - dt <= .9f && !mTourShots.empty())
        {
            wchar_t name[64]; swprintf_s(name, L"cam_%05d_%d", static_cast<int>(mCamAlong - mCamEvery), mCamStep); mTourShot = name;
        }
        if (mCamClock > 1.1f)
        {   // where the camera came to, and where she is in its view (0 the middle .. 1 the top or bottom edge; > 1 out)
            const auto& c = world.MainCamera; const auto& her = world.MainPlayer.Position;
            const XMFLOAT3 chest{her.x, her.y + 1.2f, her.z};
            const float dx = chest.x - c.Position.x, dy = chest.y - c.Position.y, dz = chest.z - c.Position.z, d = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float toHer = std::atan2(dy, std::hypot(dx, dz)), off = std::abs(toHer - (c.Pitch + c.ViewPitchOffset)) / (c.FieldOfViewY * .5f);
            char line[200]; std::snprintf(line, sizeof line, "%.0f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n", mCamAlong - mCamEvery, c.Pitch, Pitches[mCamStep], d, c.Position.y - her.y, off);
            std::ofstream(table, std::ios::app) << line;
            ++mCamStep; mCamClock = 0;
        }
    }

    void EaglesAscentGame::SideWalk(DoorwaysGameHost& host, float dt)
    {
        auto& world = host.World; auto& session = GameSession(); auto& p = world.MainPlayer;
        if (mClock < 3 || !world.RuntimeScene || !world.RuntimeScene->Loaded || mMap.Trail.empty()) return;
        host.Combat.RestorePlayerHealth(world, 10000);   // (she is kept whole: a long drop is not a death here)
        const auto table = std::filesystem::path(mLog).parent_path() / "side.tsv";
        const float end = static_cast<float>(mMap.Trail.size() - 1) * EaglesAscent::TrailStep;
        const auto trailAt = [&](float along) { return mMap.Trail[static_cast<size_t>(std::clamp(along / EaglesAscent::TrailStep, 0.f, static_cast<float>(mMap.Trail.size() - 1)))]; };
        const auto flown = [&](float along) { for (const auto& f : mMap.FlyStretches) if (along > f.From - 6.f && along < f.To + 6.f) return true; return false; };
        if (mSideAlong < 0)
        {
            std::ofstream(table, std::ios::trunc) << "along\tside\tresult\toff_m\tdrop_m\tseconds\tx\ty\tz\tslope_ahead\tstep_ahead\ton_stair\tsteep_held\tgame_wall\tgame_into\n";
            if (mSideTrace) std::ofstream(std::filesystem::path(mLog).parent_path() / "side-trace.tsv", std::ios::trunc) << "along\tside\tt\toff\tx\ty\tz\tgrounded\tland\tland_low\tground_under\tground_over\tcam_x\tcam_y\tcam_z\tcam_land\tcam_cave\ther_cave\n";
            mSideAlong = std::max(mSideFrom, 8.f); mSideSide = 0; mSideClock = -1;
        }
        if (mSideClock < 0)
        {   // the next run: on the way, facing straight off it
            while (mSideAlong <= std::min(mSideTo, end - 8.f) && flown(mSideAlong)) mSideAlong += mSideEvery;
            if (mSideAlong > std::min(mSideTo, end - 8.f))
            {
                session.ScriptedMove = 0;
                std::ofstream out(table, std::ios::app);
                for (const auto& [what, n] : mSideCount) out << "count\t" << what << '\t' << n << '\n';
                out << (mSideBad == 0 ? "SIDE_PASS" : "SIDE_FAIL") << "\tbad=" << mSideBad << '\n';
                out.close();
                Evidence(world, mSideBad == 0 ? "SIDE_PASS" : "SIDE_FAIL"); mSideWalk = false; PostQuitMessage(mSideBad == 0 ? 0 : 2); return;
            }
            const XMFLOAT3 a = trailAt(mSideAlong - 4.f), b = trailAt(mSideAlong + 4.f);
            XMFLOAT3 at = trailAt(mSideAlong); float y = at.y;
            if (Standable(*world.RuntimeScene, at, y)) at.y = y;
            mSideYaw = std::atan2(b.x - a.x, b.z - a.z) + (mSideAhead ? 0.f : mSideSide == 0 ? -XM_PIDIV2 : XM_PIDIV2);
            Put(host, at, mSideYaw);
            session.ScriptedMove = 0; mSideClock = 0; mSideStill = 0; mSideUnder = 0; mSideStart = at; mSideLast = at; mSideLow = at.y;
            mSideKeys = {}; GetNyxaraKit().ResetCooldowns();
            mSideSteep = mSideSteepLast = GetDoorwaysRuntimeSteepGroundStats().HeldMoves; mSideWall = mMap.WallHolds;
            return;
        }
        mSideClock += dt;
        world.MainCamera.Yaw = mSideYaw; world.MainCamera.Pitch = -.12f; p.Yaw = mSideYaw;
        if (mSideDash && mSideClock < 1.2f)
        {   // (with her slide: held where she was stood until a slide of the run before is over; it takes a second)
            Put(host, mSideStart, mSideYaw); session.ScriptedMove = 0; mSideLast = p.Position; mSideLow = mSideStart.y; return;
        }
        if (mSideClock < (mSideDash ? 1.5f : .4f)) { session.ScriptedMove = 0; mSideLast = p.Position; return; }
        session.ScriptedMove = 'W'; session.ScriptedFast = !mSideJog;
        mSideKeys.K1 = mSideDash && mSideClock < 1.6f;   // (F, as she sets off)
        const float fx = std::sin(mSideYaw), fz = std::cos(mSideYaw);
        const float off = std::hypot(p.Position.x - mSideStart.x, p.Position.z - mSideStart.z);
        const float moved = std::hypot(p.Position.x - mSideLast.x, p.Position.z - mSideLast.z);
        mSideLast = p.Position; mSideLow = std::min(mSideLow, p.Position.y);
        mSideStill = (dt > 0 && moved / dt < .6f) ? mSideStill + dt : 0;
        if (mSideTrace)
        {   // the frame: and the walkable ground under her (to 40 m down) and over her (to 40 m up), as the engine finds it;
            // the camera's place, the land at it, and whether it and she are in a cave (the camera under the land outside one)
            const float x = p.Position.x, z = p.Position.z, y = p.Position.y;
            const auto below = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*world.RuntimeScene, x, z, y - 40.f, y + .3f);
            const auto above = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*world.RuntimeScene, x, z, y + .3f, y + 40.f);
            const XMFLOAT3 eye = world.MainCamera.Position;
            char row[420];
            std::snprintf(row, sizeof row, "%.0f\t%s\t%.3f\t%.2f\t%.2f\t%.2f\t%.2f\t%d\t%.2f\t%.2f\t%s\t%s\t%.2f\t%.2f\t%.2f\t%.2f\t%d\t%d\n", mSideAlong, mSideAhead ? "ahead" : mSideSide == 0 ? "left" : "right", mSideClock, off, x, y, z,
                          p.IsGrounded ? 1 : 0, mMap.Land(x, z), mMap.LandLow(x, z), below.Hit ? std::to_string(below.GroundY).substr(0, 7).c_str() : "-", above.Hit ? std::to_string(above.GroundY).substr(0, 7).c_str() : "-",
                          eye.x, eye.y, eye.z, mMap.Land(eye.x, eye.z), mMap.InCave(eye) ? 1 : 0, mMap.InCave({x, y + 1.f, z}) ? 1 : 0);
            std::ofstream(std::filesystem::path(mLog).parent_path() / "side-trace.tsv", std::ios::app) << row;
            mSideShot += dt;
            if (mSideFilm && mSideShot >= mSideShotEvery && !mTourShots.empty())
            {
                mSideShot = 0;
                wchar_t name[64]; swprintf_s(name, L"film_%05d_%s_%05.2f", static_cast<int>(mSideAlong), mSideAhead ? L"ahead" : mSideSide == 0 ? L"left" : L"right", mSideClock); mTourShot = name;
            }
        }
        const auto steepHeld = GetDoorwaysRuntimeSteepGroundStats().HeldMoves;   // (the engine's steep walls held her this frame)
        const bool steepNow = steepHeld != mSideSteepLast; mSideSteepLast = steepHeld;
        const bool under = !p.IsGrounded && p.Position.y < mMap.LandLow(p.Position.x, p.Position.z) - 4.f && !mMap.InCave(p.Position);
        mSideUnder = under ? mSideUnder + dt : 0;
        std::string result;
        if (off >= 30.f) result = "free";
        else if (mSideUnder > .3f) result = "FELL_THROUGH";
        else if (mSideClock > (mSideJog ? 14.f : mSideDash ? 9.5f : 8.4f)) result = "timeout";
        else if (mSideStill > .8f && p.IsGrounded)
        {
            EaglesAscent::Trunk t;
            const float l0 = mMap.Land(p.Position.x + fx * .5f, p.Position.z + fz * .5f), l1 = mMap.Land(p.Position.x + fx * 3.f, p.Position.z + fz * 3.f);
            const float slope = (l1 - l0) / 2.5f;
            const auto ahead = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*world.RuntimeScene, p.Position.x + fx * .9f, p.Position.z + fz * .9f, p.Position.y - 3.f, p.Position.y + 4.f);
            if (mMap.NearestTrunk(p.Position.x + fx * .6f, p.Position.y, p.Position.z + fz * .6f, 1.2f, t)) result = "tree";
            else if (mMap.DrawnTop(p.Position.x + fx * .6f, p.Position.z + fz * .6f) > p.Position.y + EaglesAscent::IntoRock) result = "wall";   // a face as drawn
            else if (mMap.IntoHolds != mSideInto) result = "held_at_edge";   // held from walking into the mountain where its ground ended
            else if (steepNow) result = "steep_wall";                         // the engine's steep walls (a face of the collision)
            else if (l1 < .5f) result = "water";
            else if (slope > 1.2f || mMap.Sliding) result = "steep";
            else if (ahead.Hit && ahead.GroundY > p.Position.y + .45f) result = "step";
            else result = "BLOCKED";
        }
        if (result.empty()) return;
        const float l0 = mMap.Land(p.Position.x + fx * .5f, p.Position.z + fz * .5f), l1 = mMap.Land(p.Position.x + fx * 3.f, p.Position.z + fz * 3.f);
        const auto ahead = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*world.RuntimeScene, p.Position.x + fx * .9f, p.Position.z + fz * .9f, p.Position.y - 3.f, p.Position.y + 4.f);
        const bool stair = std::isfinite(mMap.TreadTop(mSideStart.x, mSideStart.z, mSideStart.y));
        const bool bad = result == "BLOCKED" || result == "FELL_THROUGH";
        mSideBad += bad ? 1 : 0; ++mSideCount[result + (stair ? "_on_stair" : "")];
        char line[360];
        std::snprintf(line, sizeof line, "%.0f\t%s\t%s\t%.1f\t%.1f\t%.1f\t%.2f\t%.2f\t%.2f\t%.2f\t%s\t%d\t%llu\t%d\t%d\n", mSideAlong, mSideAhead ? "ahead" : mSideSide == 0 ? "left" : "right", result.c_str(), off,
                      mSideStart.y - mSideLow, mSideClock, p.Position.x, p.Position.y, p.Position.z, (l1 - l0) / 2.5f,
                      ahead.Hit ? std::to_string(ahead.GroundY - p.Position.y).substr(0, 5).c_str() : "-", stair ? 1 : 0,
                      static_cast<unsigned long long>(steepHeld - mSideSteep), mMap.WallHolds - mSideWall, mMap.IntoHolds - mSideInto);
        std::ofstream(table, std::ios::app) << line;
        if ((bad || result == "timeout") && !mTourShots.empty())
        {
            wchar_t name[64]; swprintf_s(name, L"side_%05d_%s_%S", static_cast<int>(mSideAlong), mSideAhead ? L"ahead" : mSideSide == 0 ? L"left" : L"right", result.c_str()); mTourShot = name;
        }
        session.ScriptedMove = 0; mSideClock = -1; mSideInto = mMap.IntoHolds;
        if (mSideAhead || ++mSideSide > 1) { mSideSide = 0; mSideAlong += mSideEvery; }
    }

    void EaglesAscentGame::BossFilm(DoorwaysGameHost& host, float dt)
    {
        auto& world = host.World; auto& her = world.MainPlayer;
        if (mClock < 3 || !world.RuntimeScene || !world.RuntimeScene->Loaded || (mCreaturesOn && !mAroundPlaced)) return;
        const auto table = std::filesystem::path(mLog).parent_path() / "boss.tsv";
        const auto row = [&](const std::string& line) { std::ofstream(table, std::ios::app) << line << '\n'; };
        host.Combat.RestorePlayerHealth(world, 10000);   // (she is kept whole)
        int index = -1;
        for (int i = 0; i < static_cast<int>(mMap.Placements.size()); ++i) if (mMap.Placements[static_cast<size_t>(i)].Id == mBossId) index = i;
        if (index < 0) { row("BOSS_FAIL\tno_placement_" + mBossId); PostQuitMessage(2); mBossFilm = false; return; }
        const auto& pl = mMap.Placements[static_cast<size_t>(index)];
        if (mBossPhase == 0)
        {   // on the way before it, where she can stand, within its sight
            std::ofstream(table, std::ios::trunc) << "t\tx\ty\tz\tground\tfloor\tfeet_under_floor\ther_x\ther_y\ther_z\tdist\thealth\tcalm\n";
            const auto trailAt = [&](float along) { return mMap.Trail[static_cast<size_t>(std::clamp(along / EaglesAscent::TrailStep, 0.f, static_cast<float>(mMap.Trail.size() - 1)))]; };
            XMFLOAT3 at = trailAt(pl.At - 20.f);
            for (const float back : {20.f, 16.f, 24.f, 12.f, 28.f, 8.f})
            {
                const XMFLOAT3 q = trailAt(pl.At - back); float y = 0;
                if (Standable(*world.RuntimeScene, q, y) && std::hypot(q.x - pl.P.x, y - pl.P.y, q.z - pl.P.z) < 34.f) { at = {q.x, y, q.z}; break; }
            }
            Put(host, at, std::atan2(pl.P.x - at.x, pl.P.z - at.z));
            mBossPhase = 1; mBossClock = 0; mBossLog = 0; mBossShot = 0;
            return;
        }
        mBossClock += dt;
        const auto looks = mCreatures.MountainLooks();
        const Journey::MountainLook* boss = nullptr;
        for (const auto& l : looks) if (l.Group == index) boss = &l;
        mBossKeys = {};
        for (const auto& [at, key] : mBossTaps)
            if (mBossClock >= at && mBossClock < at + .1f)
                switch (key)
                {
                case 'L': mBossKeys.Lmb = true; break; case 'R': mBossKeys.Rmb = true; break; case 'F': mBossKeys.K1 = true; break;
                case 'V': mBossKeys.K2 = true; break; case 'C': mBossKeys.K3 = true; break; case 'E': mBossKeys.E = true; break;
                case 'J': mBossKeys.Space = true; break;
                default: break;
                }
        if (mBossWalk && !boss) GameSession().ScriptedMove = 0;   // (it is beaten: she stands)
        if (boss)
        {
            // the camera behind her, the boss in the picture
            const float toward = std::atan2(boss->Pos.x - her.Position.x, boss->Pos.z - her.Position.z);
            const float d = std::hypot(boss->Pos.x - her.Position.x, boss->Pos.z - her.Position.z);
            world.MainCamera.Yaw = toward;   // (and up at it, as high as it is)
            world.MainCamera.Pitch = std::clamp(std::atan2(boss->Pos.y + 2.5f - (her.Position.y + 1.6f), d + 5.f) - .05f, -.12f, .75f);
            if (mBossFight)
            {   // she goes at it when it is low, and cuts when it is close
                const bool low = boss->Pos.y - 1.3f - her.Position.y < 2.5f;
                if (low && d > 4.f && d < 16.f) { her.Position.x += std::sin(toward) * 5.f * dt; her.Position.z += std::cos(toward) * 5.f * dt; }
                if (d < 7.f) { her.Yaw = toward; if (std::fmod(mBossClock, .5f) < dt) mBossKeys.Lmb = true; }
            }
            if (mBossWalk)
            {   // on foot at it, up the way as a player goes (W by the view, which looks at it); within reach she cuts,
                // her left string for three seconds and her right for three, in turn. While a slide of the film's keys
                // carries her she is not turned and does not cut: it goes on through the creature, as a player's does
                auto& session = GameSession();
                bool sliding = false;
                for (const auto& [at, key] : mBossTaps) sliding = sliding || (key == 'F' && mBossClock >= at && mBossClock < at + 1.4f);
                session.ScriptedMove = d > 2.4f && !sliding ? 'W' : 0; session.ScriptedFast = d > 9.f;
                if (d < 3.4f && !sliding)
                {
                    her.Yaw = toward;
                    if (std::fmod(mBossClock, .4f) < dt) (std::fmod(mBossClock, 6.f) < 3.f ? mBossKeys.Lmb : mBossKeys.Rmb) = true;
                }
            }
            mBossLog += dt;
            if (mBossLog >= .1f)
            {
                mBossLog = 0;
                float floor = std::numeric_limits<float>::quiet_NaN();   // the highest walkable floor under it, from 12 m over it down
                for (float top = boss->Pos.y + 12.f; top > boss->Pos.y - 14.f; top -= 2.f)
                {
                    const auto hit = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*world.RuntimeScene, boss->Pos.x, boss->Pos.z, top - 2.f, top);
                    if (hit.Hit) { floor = hit.GroundY; break; }
                }
                char line[300];
                std::snprintf(line, sizeof line, "%.1f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.1f\t%.2f\t%d", mBossClock, boss->Pos.x, boss->Pos.y, boss->Pos.z, boss->Ground,
                              floor, std::isnan(floor) ? 0.f : floor - (boss->Pos.y - 1.3f), her.Position.x, her.Position.y, her.Position.z, d, boss->Health, boss->Calm);
                row(line);
            }
        }
        mBossShot += dt;
        if (mBossShot >= mBossEvery && !mTourShots.empty())
        {
            mBossShot = 0;
            wchar_t name[48]; swprintf_s(name, L"boss_%03d", mBossShots++);
            mTourShot = name;
        }
        if (mBossClock > mBossFor)
        {
            row("BOSS_DONE");
            Evidence(world, "BOSS_DONE");
            mBossFilm = false; PostQuitMessage(0);
        }
    }

    void EaglesAscentGame::Tour(DoorwaysGameHost& host, float dt)
    {
        auto& world = host.World; const auto& her = world.MainPlayer.Position;
        if (mClock < 3) return;
        const auto table = std::filesystem::path(mLog).parent_path() / "tour.tsv";
        const auto row = [&](const std::string& line) { std::ofstream(table, std::ios::app) << line << '\n'; };
        const auto bad = [&](const std::string& why) { ++mTourBad; mTourNote += (mTourNote.empty() ? "" : ",") + why; };
        const auto finish = [&]()
        {
            row(std::string(mTourBad == 0 ? "CREATURES_PASS" : "CREATURES_FAIL") + "\tbad=" + std::to_string(mTourBad));
            Evidence(world, mTourBad == 0 ? "CREATURES_PASS" : "CREATURES_FAIL");
            mTour = false; PostQuitMessage(mTourBad == 0 ? 0 : 2);
        };
        mTourDamage += static_cast<float>(host.Combat.RestorePlayerHealth(world, 10000));   // she is kept whole: what they took is counted
        mTourClock += dt; mTourFinClock += dt;

        if (mTourAt < 0)
        {   // the stops: every placement, by where it is along the way (a cave's: where its mouth is nearest the way)
            std::ofstream(table, std::ios::trunc) << "stop\tid\tkinds\talong\twoke\tground_off_m\troused\tclosed_m\tdamage\ther_dy\tbeaten\tnote\n";
            for (int i = 0; i < static_cast<int>(mMap.Placements.size()); ++i)
            {
                const auto& pl = mMap.Placements[static_cast<size_t>(i)];
                float best = 1e30f, along = 0;
                for (size_t t = 0; t < mMap.Trail.size(); ++t)
                {
                    const float dx = mMap.Trail[t].x - pl.P.x, dy = mMap.Trail[t].y - pl.P.y, dz = mMap.Trail[t].z - pl.P.z, d = dx * dx + dy * dy + dz * dz;
                    if (d < best) { best = d; along = static_cast<float>(t) * EaglesAscent::TrailStep; }
                }
                mTourStops.push_back({i, pl.Cave < 0 ? pl.At : along});
            }
            std::stable_sort(mTourStops.begin(), mTourStops.end(), [](const TourStop& a, const TourStop& b) { return a.Along < b.Along; });
            mTourAt = 0; mTourPhase = 0; mTourClock = 0;
        }

        if (mTourAt < static_cast<int>(mTourStops.size()))
        {   // a stop: put her before it, watch, strike, check
            const int index = mTourStops[static_cast<size_t>(mTourAt)].Placement;
            const auto& pl = mMap.Placements[static_cast<size_t>(index)];
            const auto looks = mCreatures.MountainLooks();
            std::vector<const Journey::MountainLook*> mine;
            for (const auto& l : looks) if (l.Group == index && !l.Dying) mine.push_back(&l);
            const auto closest = [&]()
            {
                float d = 1e30f;
                for (const auto* l : mine) d = std::min(d, std::hypot(l->Pos.x - her.x, l->Pos.z - her.z));
                return d;
            };
            if (mTourPhase == 0)
            {   // where she stands: on the way (a cave's line) this far before it, facing it
                mTourNote.clear();
                std::copy(std::begin(mGrowth.AscentCrystals), std::end(mGrowth.AscentCrystals), std::begin(mTourHad));
                XMFLOAT3 at{};
                const float back = pl.Boss ? 24.f : pl.Cave >= 0 ? 9.f : 14.f;
                if (pl.Cave < 0 && pl.Spur.empty())
                {   // the first point back along the way, from `back`, where she can stand (not over a stretch only wings
                    // cross: tour 2 put her in the air before way-1800 and way-2800, and she fell 56 and 77 m)
                    const auto trailAt = [&](float along) { return mMap.Trail[static_cast<size_t>(std::clamp(along / EaglesAscent::TrailStep, 0.f, static_cast<float>(mMap.Trail.size() - 1)))]; };
                    // and within sight of it (tour 3: before way-5100 the way climbs a cliff on wings; 26 m back she
                    // stood at its foot, 36 m off and 31 m below, out of its sight): if need be past it, coming down
                    at = trailAt(pl.At - back);
                    bool stood = false;
                    const float reach = pl.Boss ? 34.f : 24.f;
                    for (const float more : {0.f, -4.f, 4.f, -8.f, 8.f, 12.f, -2.f * back + 4.f, -2.f * back})
                    {
                        const XMFLOAT3 p = trailAt(pl.At - back - more);
                        float y = 0;
                        if (!world.RuntimeScene || !Standable(*world.RuntimeScene, p, y)) continue;
                        if (std::hypot(p.x - pl.P.x, y - pl.P.y, p.z - pl.P.z) > reach) continue;
                        at = {p.x, y, p.z}; stood = true; break;
                    }
                    if (!stood) bad("no_place_to_stand");
                }
                else if (pl.Cave < 0)
                {   // a group around the way: on its line out from the way (PlaceAround), `back` before its place
                    const float want = std::max(0.f, pl.SpurAt - back);
                    float along = 0;
                    at = pl.Spur[0];
                    for (size_t i = 1; i < pl.Spur.size(); ++i)
                    {
                        const XMFLOAT3 a = pl.Spur[i - 1], b = pl.Spur[i];
                        const float step = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z));
                        if (along + step >= want && step > 1e-3f)
                        {
                            const float u = (want - along) / step;
                            at = {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u};
                            break;
                        }
                        along += step;
                    }
                }
                else
                {
                    const auto& line = mMap.Caves()[static_cast<size_t>(pl.Cave)].Line;
                    float along = 0, want = std::max(0.f, pl.At - back);
                    at = {line[0].X, line[0].Y, line[0].Z};
                    for (size_t i = 1; i < line.size(); ++i)
                    {
                        const float step = std::sqrt((line[i].X - line[i - 1].X) * (line[i].X - line[i - 1].X) + (line[i].Y - line[i - 1].Y) * (line[i].Y - line[i - 1].Y) +
                                                     (line[i].Z - line[i - 1].Z) * (line[i].Z - line[i - 1].Z));
                        if (along + step >= want && step > 1e-3f)
                        {
                            const float t = (want - along) / step;
                            at = {line[i - 1].X + (line[i].X - line[i - 1].X) * t, line[i - 1].Y + (line[i].Y - line[i - 1].Y) * t, line[i - 1].Z + (line[i].Z - line[i - 1].Z) * t};
                            break;
                        }
                        along += step;
                    }
                }
                Put(host, at, std::atan2(pl.P.x - at.x, pl.P.z - at.z)); mTourStand = at;
                mTourPhase = 1; mTourClock = 0; mTourDamage = 0; mTourWoke = 0; mTourFirst = 0; mTourOff = -1;
                return;
            }
            if (mTourPhase == 1 && mTourOff < 0 && !mine.empty())
            {   // the frame it wakes: it stands on its ground (later it may already be on its way down the stairs to her)
                mTourOff = 0;
                for (const auto* l : mine) mTourOff = std::max(mTourOff, std::abs(l->Ground - l->Home.y));
            }
            if (mTourPhase == 1 && mTourClock > .8f)
            {   // it woke, and it stood on its ground
                mTourWoke = static_cast<int>(mine.size());
                mTourOff = std::max(mTourOff, 0.f);
                if (mine.empty()) bad("never_woke");
                if (mTourOff > 2.5f) bad("off_its_ground");
                mTourFirst = closest();
                mTourPhase = 2;
            }
            else if (mTourPhase == 2 && mTourClock > 2.2f)
            {   // a picture of it, as it has seen her
                if (!mTourShots.empty())
                {
                    wchar_t name[96]; swprintf_s(name, L"%03d_%S", mTourAt, pl.Id.c_str());
                    mTourShot = name;
                }
                mTourPhase = 3;
            }
            else if (mTourPhase == 3 && mTourClock > 2.6f)
            {   // it has seen her: none of it waits any more
                mTourCalm = 0;
                for (const auto* l : mine) if (l->Calm > 0) ++mTourCalm;
                if (mTourCalm > 0) bad("never_roused");
                mTourPhase = 4;
            }
            else if (mTourPhase == 4 && mTourClock > 5.f)
            {   // it came at her (those that shoot, to their distance); then it falls
                const float closed = mTourFirst - closest();
                const bool keeps = pl.Kinds.size() == 1 && (pl.Kinds[0] == "ali" || pl.Kinds[0] == "kudiani");
                if (!mine.empty() && closed < 2.5f && closest() > (keeps ? 10.5f : 5.f) && mTourDamage <= 0) bad("never_came");
                std::ostringstream line; line << std::fixed << std::setprecision(1);
                line << mTourAt << '\t' << pl.Id << '\t';
                for (size_t k = 0; k < pl.Kinds.size(); ++k) line << (k ? "+" : "") << pl.Kinds[k];
                line << '\t' << mTourStops[static_cast<size_t>(mTourAt)].Along << '\t' << mTourWoke << '\t' << mTourOff << '\t'
                     << (mTourWoke - mTourCalm) << '\t' << closed << '\t' << mTourDamage << '\t' << her.y - mTourStand.y << '\t';
                if (her.y - mTourStand.y < -3) bad("she_fell");
                mTourRow = line.str();
                if (mTourFinish) { mTourPhase = 10; return; }   // (her finishing kill of each: below)
                mCreatures.MountainStrike(index);
                mTourPhase = 5;
            }
            else if (mTourPhase == 5 && mTourClock > 6.2f)
            {   // beaten for good
                int beaten = 0, wanted = 0;
                const auto& done = mCreatures.MountainBeaten();
                for (const auto& c : mCreatureList)
                    if (c.Group == index) { ++wanted; if (std::find(done.begin(), done.end(), c.Id) != done.end()) ++beaten; }
                if (beaten != wanted) bad("not_beaten");
                {   // what they left: crystals of their stretch's kind only (AscentGrowth::CrystalKindOf)
                    const int kind = AscentGrowth::CrystalKindOf(GroupStretch(index));
                    int left = 0, others = 0;
                    for (int k = 0; k < AscentGrowth::CrystalKinds; ++k) (k == kind ? left : others) += mGrowth.AscentCrystals[k] - mTourHad[k];
                    mTourNote += (mTourNote.empty() ? "" : ",") + std::string("crystals_") + AscentGrowth::CrystalId(kind) + "_" + std::to_string(left);
                    if (others != 0) bad("crystals_of_another_kind_" + std::to_string(others));
                    if (beaten > 0 && left <= 0) bad("no_crystals_left");
                }
                row(mTourRow + std::to_string(beaten) + "/" + std::to_string(wanted) + '\t' + mTourNote);
                ++mTourAt; mTourPhase = 0; mTourClock = 0;
            }
            else if (mTourPhase == 10)
            {   // the finishers' tour: the next creature of it she may finish; she is put 2.5 m before its edge where she can
                // stand (from the side she is on first), facing it
                auto& kit = GetNyxaraKit();
                auto& enemies = kit.Enemies();
                const Journey::MountainLook* next = nullptr;
                for (const auto* l : mine)
                {
                    if (l->Slot < 0 || l->Slot >= static_cast<int>(enemies.size())) continue;
                    const auto& e = enemies[static_cast<size_t>(l->Slot)];
                    if (e.Finishable && e.Alive && e.Doom <= 0) { next = l; break; }
                }
                if (!next)
                {   // none left of it she may finish (a boss): the rest struck down, as in the creatures' tour
                    mCreatures.MountainStrike(index);
                    mTourPhase = 5; mTourClock = 5.6f;
                    return;
                }
                const XMFLOAT3 feet{next->Pos.x, next->Ground, next->Pos.z};
                const float from = std::atan2(her.x - feet.x, her.z - feet.z);
                bool put = false;
                for (const float turn : {0.f, .7f, -.7f, 1.4f, -1.4f, 2.1f, -2.1f, 3.1416f})
                {
                    const float edge = enemies[static_cast<size_t>(next->Slot)].Radius;   // (from its edge: a boss is wide)
                    for (const float d : {edge + 2.5f, edge + 1.9f, edge + 3.1f})
                    {
                        const XMFLOAT3 at{feet.x + d * std::sin(from + turn), feet.y, feet.z + d * std::cos(from + turn)};
                        float y = 0;
                        if (!world.RuntimeScene || !Standable(*world.RuntimeScene, at, y) || std::abs(y - feet.y) > 1.2f) continue;
                        // (and not in a wall: she could never walk there, and from there every way of hers seems blocked)
                        if (FindDoorwaysRuntimeSceneBlockingProxyForPlayer(*world.RuntimeScene, {at.x, y, at.z}, world.MainPlayer.CollisionRadius,
                                                                           world.MainPlayer.CollisionHeight)) continue;
                        Put(host, {at.x, y, at.z}, std::atan2(feet.x - at.x, feet.z - at.z));
                        mTourVictimY = y; put = true; break;
                    }
                    if (put) break;
                }
                mTourVictim = next->Slot; mTourVictimId = next->Id; mTourVictimKind = next->Kind;
                mTourKind = 0; mTourFinClock = 0; mTourFinStart = 0; mTourFinAir = 0; mTourFinShot = false;
                if (!put)
                {   // nowhere before it to stand: it is struck down
                    row("finish\t" + std::to_string(mTourAt) + '\t' + next->Id + "\tkind=" + std::to_string(next->Kind) + "\tno_place");
                    auto& e = enemies[static_cast<size_t>(next->Slot)]; e.Health = 0; e.Alive = false;
                    ++mTourFinNoPlace; mTourPhase = 13;
                    return;
                }
                mTourPhase = 11;
            }
            else if (mTourPhase == 11 && mTourFinClock > .35f)
            {   // she stands: it is doomed, as a blow of hers that would kill it dooms it (one that would not, out of her
                // reach or over her head, as a spirit riding the air: struck down, and counted apart)
                auto& kit = GetNyxaraKit();
                if (kit.CanDoom(mTourVictim))
                {
                    kit.Doom(mTourVictim);
                    mTourPhase = 12; mTourFinClock = 0;
                }
                else
                {
                    auto& e = kit.Enemies()[static_cast<size_t>(mTourVictim)];
                    std::ostringstream line; line << std::fixed << std::setprecision(2);
                    line << "finish\t" << mTourAt << '\t' << mTourVictimId << "\tkind=" << mTourVictimKind << "\tout_of_reach\tgap="
                         << std::hypot(e.Pos.x - her.x, e.Pos.z - her.z) - e.Radius << "\tits_dy=" << e.Pos.y - 1.3f + e.Lift - her.y;
                    row(line.str());
                    e.Health = 0; e.Alive = false; ++mTourFinOut;
                    mTourPhase = 13; mTourFinClock = 0;
                }
            }
            else if (mTourPhase == 12)
            {   // her finisher: which; a picture of it as it kills; she must stay on her ground (not over an edge)
                auto& kit = GetNyxaraKit();
                auto& e = kit.Enemies()[static_cast<size_t>(mTourVictim)];
                const int now = kit.Finishing();
                if (now && !mTourKind) { mTourKind = now; mTourFinStart = mTourFinClock; }
                const float into = mTourFinClock - mTourFinStart;
                if (mTourKind && !world.MainPlayer.IsGrounded) mTourFinAir += dt;
                constexpr float ShotAt[11] = {0, 1.f, 1.2f, 1.6f, .75f, 1.35f, 1.1f, 1.6f, 1.55f, 1.45f, .55f};   // (the cross past it; the plunge's blades in; the slide's blow; the others' kills)
                if (mTourKind && !mTourFinShot && into >= ShotAt[mTourKind] && !mTourShots.empty())
                {
                    wchar_t name[128]; swprintf_s(name, L"%03d_%S_f%d_%S", mTourAt, pl.Id.c_str(), mTourKind, mTourVictimId.c_str());
                    mTourShot = name; mTourFinShot = true;
                }
                const bool over = mTourKind && !now, none = !mTourKind && mTourFinClock > 1.2f, stuck = mTourKind && now && into > 4.5f;
                if (over || none || stuck)
                {
                    std::ostringstream line; line << std::fixed << std::setprecision(2);
                    line << "finish\t" << mTourAt << '\t' << mTourVictimId << "\tkind=" << mTourVictimKind << "\tfinisher=" << mTourKind
                         << "\ther_dy=" << her.y - mTourVictimY << "\tair=" << mTourFinAir;
                    if (none)
                    {   // (how far it was from her, how much higher, whether it was doomed; then it is struck down)
                        const XMFLOAT3 its{e.Pos.x, e.Pos.y - 1.3f + e.Lift, e.Pos.z};
                        line << "\tno_room\tgap=" << std::hypot(its.x - her.x, its.z - her.z) - e.Radius << "\tits_dy=" << its.y - her.y
                             << "\tdoomed=" << (e.Doom > 0 ? 1 : 0) << "\tgrounded=" << (world.MainPlayer.IsGrounded ? 1 : 0);
                        ++mTourFinNoRoom; e.Health = 0; e.Alive = false;
                    }
                    else ++mTourFinPlayed[mTourKind];
                    if (stuck) { line << "\tSTUCK"; bad("finisher_stuck"); }
                    if (mTourFinAir > .3f) { line << "\tFELL"; bad("finisher_fell"); ++mTourFinFell; }
                    row(line.str());
                    mTourPhase = 13; mTourFinClock = 0;
                }
            }
            else if (mTourPhase == 13 && mTourFinClock > .7f) mTourPhase = 10;
            return;
        }
        if (mTourFinish)
        {   // the finishers' tour ends with the stops (the nests and the save are the creatures' tour's)
            row("finishers\twheel=" + std::to_string(mTourFinPlayed[5]) + "\tbreaker=" + std::to_string(mTourFinPlayed[6]) + "\tflurry=" +
                std::to_string(mTourFinPlayed[8]) + "\theadsman=" + std::to_string(mTourFinPlayed[9]) + "\tothers=" +
                std::to_string(mTourFinPlayed[1] + mTourFinPlayed[2] + mTourFinPlayed[3] + mTourFinPlayed[4] + mTourFinPlayed[7] + mTourFinPlayed[10]) + "\tno_room=" + std::to_string(mTourFinNoRoom) + "\tno_place=" + std::to_string(mTourFinNoPlace) +
                "\tfell=" + std::to_string(mTourFinFell) + "\tout=" + std::to_string(mTourFinOut));
            finish();
            return;
        }

        const int nests = static_cast<int>(mMap.Nests.size());
        if (mTourAt < static_cast<int>(mTourStops.size()) + nests)
        {   // every Eagle Nest: she stands in it; it must become the place she wakes at, and the save must say so
            const auto& n = mMap.Nests[static_cast<size_t>(mTourAt - static_cast<int>(mTourStops.size()))];
            if (mTourPhase == 0) { Put(host, mMap.Rest(n.Station), mMap.Stations[static_cast<size_t>(n.Station)].Yaw); mTourPhase = 1; mTourClock = 0; return; }   // into the nest
            if (mTourPhase == 1 && mTourClock > 1.6f)
            {
                if (!mTourShots.empty()) { wchar_t name[64]; swprintf_s(name, L"nest_%d", n.Station); mTourShot = name; }
                if (world.RuntimeScene)
                {   // how the nest sits where it is, and the best place beside the way at its station (for the placing model):
                    // the side the land rises first (against the mountain), 4.5 to 10.5 m out, up to 6 m along
                    const auto& st = mMap.Stations[static_cast<size_t>(n.Station)];
                    const float fx = std::sin(st.Yaw), fz = std::cos(st.Yaw), rx = std::cos(st.Yaw), rz = -std::sin(st.Yaw);
                    float y = 0;
                    const float here = NestFooting(*world.RuntimeScene, n.P.x, n.P.z, n.P.y, y);
                    const float up = mMap.Land(st.P.x + rx * 15, st.P.z + rz * 15) >= mMap.Land(st.P.x - rx * 15, st.P.z - rz * 15) ? 1.f : -1.f;
                    std::ostringstream line; line << std::fixed << std::setprecision(2) << "nestspot\t" << n.Station << "\there=" << here;
                    bool found = false;
                    for (const float side : {up, -up})
                    {
                        for (const float lateral : {6.f, 7.5f, 4.5f, 9.f, 10.5f})
                        {
                            for (const float along : {0.f, 3.f, -3.f, 6.f, -6.f})
                            {
                                const float x = st.P.x + rx * side * lateral + fx * along, z = st.P.z + rz * side * lateral + fz * along;
                                const float worst = NestFooting(*world.RuntimeScene, x, z, st.P.y, y);
                                if (worst < 0) continue;
                                line << "\tlateral=" << side * lateral << "\talong=" << along << "\ty=" << y << "\tstep=" << worst << "\tx=" << x << "\tz=" << z;
                                found = true; break;
                            }
                            if (found) break;
                        }
                        if (found) break;
                    }
                    if (!found) line << "\tNONE";
                    row(line.str());
                }
                if (mMap.Checkpoint == n.Station) SaveHere(n.Station);   // (as she would with H)
                int saved = -1;
                { std::ifstream in(mSavePath); std::string word; while (in >> word) if (word == "nest") in >> saved; }
                const bool ok = mMap.Checkpoint == n.Station && saved == n.Station;
                if (!ok) ++mTourBad;
                row("nest\t" + mMap.Stations[static_cast<size_t>(n.Station)].Name + "\tcheckpoint=" + std::to_string(mMap.Checkpoint) + "\tsaved=" + std::to_string(saved) + (ok ? "" : "\tNEST_FAIL"));
                ++mTourAt; mTourPhase = 0; mTourClock = 0;
            }
            return;
        }

        // the save read back, as when the game is started again: all she beat stays beaten, and nothing of it wakes
        if (mTourPhase == 0)
        {
            const size_t before = mCreatures.MountainBeaten().size();
            LoadSave(); StartCreatures();
            const bool ok = mSavedBeaten.size() == before && mCreatures.MountainBeaten().size() == before && before > 0;
            if (!ok) ++mTourBad;
            row("reload\tbeaten=" + std::to_string(before) + "\tsaved=" + std::to_string(mSavedBeaten.size()) + "\tnest=" + std::to_string(mSavedNest) + (ok ? "" : "\tRELOAD_FAIL"));
            mTourAgain = 0; mTourPhase = 1; mTourClock = 0;
            return;
        }
        if (mTourPhase == 1)
        {   // five of the stops again
            const int stops = static_cast<int>(mTourStops.size());
            if (mTourAgain >= 5 || stops == 0) { finish(); return; }
            const auto& pl = mMap.Placements[static_cast<size_t>(mTourStops[static_cast<size_t>((mTourAgain * 17 + 3) % stops)].Placement)];
            Put(host, pl.P, pl.Yaw); mTourPhase = 2; mTourClock = 0;
            return;
        }
        if (mTourPhase == 2 && mTourClock > 1.2f)
        {
            const int awake = static_cast<int>(mCreatures.MountainLooks().size());
            if (awake > 0) ++mTourBad;
            row("again\t" + std::to_string(mTourAgain) + "\tawake=" + std::to_string(awake) + (awake > 0 ? "\tAGAIN_FAIL" : ""));
            ++mTourAgain; mTourPhase = 1;
        }
    }

    void EaglesAscentGame::GroundProbe(DoorwaysGameHost& host)
    {
        const DoorwaysRuntimeScene* scene = host.World.RuntimeScene;
        if (!scene || !scene->Loaded) { if (++mGroundWait > 600) { std::ofstream(std::filesystem::path(mLog).parent_path() / "ground.tsv") << "GROUND_FAIL\tno scene\n"; mGround = false; PostQuitMessage(2); } return; }
        float x0, z0, x1, z1; mMap.LandExtent(x0, z0, x1, z1);
        const auto t0 = std::chrono::steady_clock::now();
        std::ofstream out(std::filesystem::path(mLog).parent_path() / "ground.tsv");
        out << "x\tz\tland\tground\tresult\n";
        long long columns = 0, land = 0, held = 0, holes = 0, off = 0, caves = 0, walls = 0;
        constexpr float Step = 16.f, Near = 8.f;
        for (float z = z0 + Step * .5f; z < z1; z += Step)
            for (float x = x0 + Step * .5f; x < x1; x += Step)
            {
                ++columns;
                const float l = mMap.Land(x, z);
                if (l < 0.5f) continue;                          // the sea, the shore's water, off the heights
                ++land;
                if (mMap.InCave({x, l, z}) || mMap.InCave({x, l - 3.f, z})) { ++caves; continue; }
                const DoorwaysGroundQueryResult g = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*scene, x, z, l - Near, l + Near);
                if (g.Hit) { ++held; continue; }
                // too steep to stand on (over ~58 degrees anywhere within 6 m): a wall. The engine's ground is no steeper
                // than 60 degrees; on such a face she is held by EaglesAscent::KeepOutOfRock from these same heights.
                float steep = 0;
                for (float dz = -6.f; dz <= 6.f; dz += 3.f)
                    for (float dx = -6.f; dx <= 6.f; dx += 3.f)
                    {
                        const float gx = (mMap.Land(x + dx + 1.5f, z + dz) - mMap.Land(x + dx - 1.5f, z + dz)) / 3.f;
                        const float gz = (mMap.Land(x + dx, z + dz + 1.5f) - mMap.Land(x + dx, z + dz - 1.5f)) / 3.f;
                        steep = std::max(steep, std::sqrt(gx * gx + gz * gz));
                    }
                if (steep > 1.6f) { ++walls; continue; }
                const DoorwaysGroundQueryResult w = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*scene, x, z, l - 400.f, l + 400.f);
                if (w.Hit) { ++off; out << x << '\t' << z << '\t' << l << '\t' << w.GroundY << "\tfar_off\n"; }
                else { ++holes; out << x << '\t' << z << '\t' << l << "\t-\thole\n"; }
            }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        out << "columns\t" << columns << "\tover_land\t" << land << "\theld\t" << held << "\tcaves\t" << caves << "\twalls\t" << walls << "\tfar_off\t" << off << "\tholes\t" << holes << "\tseconds\t" << secs << '\n';
        out << (holes == 0 ? "GROUND_PASS" : "GROUND_FAIL") << "\tholes=" << holes << "\tfar_off=" << off << '\n';
        out.close();
        mGround = false; PostQuitMessage(holes == 0 ? 0 : 2);
    }

    void EaglesAscentGame::Update(DoorwaysGameHost& host, float dt, float realDt, bool inputAllowed)
    {
        if (mGround) { GroundProbe(host); return; }
        if (mSlowMs > 0) Sleep(static_cast<DWORD>(mSlowMs));   // (a test on a slow computer's frames)
        mClock += realDt; mNotice = std::max(0.f, mNotice - realDt);
        auto& session = GameSession(); auto& world = host.World; auto& p = world.MainPlayer; auto& kit = GetNyxaraKit();
        const bool flying = p.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
        const bool alive = host.Combat.PlayerActionsAllowed();
        mMap.KeepOutOfRock(p, flying, dt, world.RuntimeScene);
        if (mPointerTest) PointerTest(host, realDt);
        if (mQuestTest) QuestTest(host, realDt);
        mMouse = ReadMouse(inputAllowed);   // (her menu and her window read it)

        // Esc: the pause panel (DrawHud); its keys
        // M: the whole mountain from outside, to turn and look over (MountainView.h). The game stands still; M or Esc closes it.
        if (MountainView::Tried() && !mView.Open && mClock > 1 && mClock < 1.5f) mView.Toggle(session, world.MainCamera, mMap.Trail, mMap.Summit);
        if (mMapTest) MapTest(host, realDt);
        // M: the mountain in 3D, as before 0.5.2's detailed map (owner, 2026-11-21: "i like old map better for M"); Tab
        // from it the detailed map (AscentMap) and back; M or Esc closes either
        // Tab otherwise: her quests (0.5.4; below). It is read here once a frame, and never with Alt held (Alt+Tab, away to
        // another window, would leave her quests open behind her).
        const bool keyM = !mPaused && !mSmoke && !mClimb && !mTour && !mGrowthOpen && Press('M', inputAllowed);
        const bool tabKey = Press(VK_TAB, inputAllowed && !IsDoorwaysKeyDown(VK_MENU));
        const bool tabTest = mTestKey == VK_TAB;   // (the quests' test's)
        if (tabTest) mTestKey = 0;
        const bool keyTab = (mChart.Open || mView.Open) && tabKey;
        if (keyM)
        {
            if (mChart.Open) { mChart.Hide(); session.FreeCursor = false; mHoldBlade = true; Sound::PlayUi("ui/close", .6f); }
            else if (mView.Open) { mView.Hide(session, world.MainCamera); session.FreeCursor = false; mHoldBlade = true; Sound::PlayUi("ui/close", .6f); }
            else { mView.Toggle(session, world.MainCamera, mMap.Trail, mMap.Summit); mView.Explored = true; Sound::PlayUi("ui/open", .6f); }
        }
        else if (keyTab && mChart.Open) { mChart.Hide(); mView.Toggle(session, world.MainCamera, mMap.Trail, mMap.Summit); mView.Explored = true; }
        else if (keyTab && mView.Open && mChart.Loaded()) { mView.Hide(session, world.MainCamera); mChart.Show(p.Position); }
        if (mChart.Open)
        {
            if (session.EscapePressed) { session.EscapePressed = false; mChart.Hide(); session.FreeCursor = false; mHoldBlade = true; Sound::PlayUi("ui/close", .6f); }
            else
            {
                session.FreeCursor = true;
                const bool live = inputAllowed && !session.Unattended;
                const auto held = [&](int a, int b2) { return live && (IsDoorwaysKeyDown(a) || IsDoorwaysKeyDown(b2)); };
                const float keyX = (held('D', VK_RIGHT) ? 1.f : 0.f) - (held('A', VK_LEFT) ? 1.f : 0.f);
                const float keyZ = (held('W', VK_UP) ? 1.f : 0.f) - (held('S', VK_DOWN) ? 1.f : 0.f);
                const float zoom = (held(VK_OEM_PLUS, VK_ADD) || held('E', 'E') ? 1.f : 0.f) - (held(VK_OEM_MINUS, VK_SUBTRACT) || held('Q', 'Q') ? 1.f : 0.f);
                mChart.Update(mMouse.X, mMouse.Y, mMouse.Down, mMouse.Click, mMouse.Wheel, keyX, keyZ, zoom, Press(VK_SPACE, inputAllowed), p.Position, realDt, mHudW, mHudH, mHudS);
                kit.SetInputBlocked(true);
                kit.Update(world, host.WorldRender, host.Combat, 0, realDt, false);
                return;
            }
        }
        if (mView.Open)
        {
            if (session.EscapePressed) { session.EscapePressed = false; mView.Hide(session, world.MainCamera); session.FreeCursor = false; mHoldBlade = true; Sound::PlayUi("ui/close", .6f); }
            else
            {   // gone over with the pointer, as the detailed map is (MountainView::Explore)
                session.FreeCursor = true;
                const bool live = inputAllowed && !session.Unattended;
                const auto held = [&](int a, int b2) { return live && (IsDoorwaysKeyDown(a) || IsDoorwaysKeyDown(b2)); };
                MountainView::Input in;
                in.X = mMouse.X; in.Y = mMouse.Y; in.Left = mMouse.Down; in.Right = mMouse.Right; in.Wheel = mMouse.Wheel;
                in.KeyX = (held('D', VK_RIGHT) ? 1.f : 0.f) - (held('A', VK_LEFT) ? 1.f : 0.f);
                in.KeyZ = (held('W', VK_UP) ? 1.f : 0.f) - (held('S', VK_DOWN) ? 1.f : 0.f);
                in.Turn = (held('E', 'E') ? 1.f : 0.f) - (held('Q', 'Q') ? 1.f : 0.f);
                in.Zoom = (held(VK_OEM_PLUS, VK_ADD) ? 1.f : 0.f) - (held(VK_OEM_MINUS, VK_SUBTRACT) ? 1.f : 0.f);
                in.Home = Press(VK_SPACE, inputAllowed) || mViewHome; mViewHome = false;
                mView.Explore(world.MainCamera, in, p.Position, realDt, mHudH, [this](float x, float z) { return mMap.Land(x, z); });
                kit.SetInputBlocked(true);
                kit.Update(world, host.WorldRender, host.Combat, 0, realDt, false);
                return;
            }
        }
        // the groups around the way (0.5.2): their places on the ground once it is up, and the creatures on their lines again
        if (mCreaturesOn && !mAroundPlaced && world.RuntimeScene && world.RuntimeScene->Loaded)
        {
            mAroundReport.clear();
            mMap.PlaceAround(*world.RuntimeScene, &mAroundReport);
            mAroundPlaced = true;
            // and the engine's steep walls found now, before her first step (0.4 s on this map)
            if (GameSession().SteepGroundBlocks) PrepareDoorwaysRuntimeSceneSteepGround(*world.RuntimeScene);
            StartCreatures();
            int placed = 0, groups = 0;
            for (const auto& pl : mMap.Placements) if (pl.Around) { ++groups; if (!pl.Spur.empty()) ++placed; }
            char line[160]; std::snprintf(line, sizeof line, "[EAGLES ASCENT] %d of %d groups around the way placed.\n", placed, groups);
            OutputDebugStringA(line);
            if (mTrace) Evidence(world, ("around_placed_" + std::to_string(placed) + "_of_" + std::to_string(groups)).c_str());
        }
        // her gear's look (NyxaraLooks), and its light on her attacks (NyxaraKit): at the start, a gear given, back to the save
        if (mGearShown != mGrowth.Gear)
        {
            int example = 0, tier = 0, theme = 0, level = 0;
            AscentGrowth::GearLook(mGrowth.Gear, example, tier);
            AscentGrowth::GearStyle(mGrowth.Gear, theme, level);
            if (mLooksOn)
            {
                const int tiers[AscentGrowth::ItemCount] = {tier, tier, tier};
                std::string why;
                if (!NyxaraLooks::Apply(host.Render, host.WorldRender, example, tiers, why)) OutputDebugStringA(("[EAGLES ASCENT] her look: " + why + "\n").c_str());
            }
            GetNyxaraKit().SetTrialTheme(theme, level);
            if (mFlairOn)
            {   // her gear's flair (each gear's third, "flowing": AscentGrowth::GearFlair), and the combo that is its own
                int flairGear = 0, style = 0; bool own = false, plain = false;
                AscentGrowth::GearFlair(mGrowth.Gear, flairGear, style, own, plain);
                GetNyxaraKit().SetFlair(flairGear, style, own, plain && !mFlairBlue);
                GetNyxaraKit().SetAttackCombo(NyxaraKit::FlairCombo(flairGear, style));
            }
            mGearShown = mGrowth.Gear;
            if (mTrace) Evidence(world, ("gear_" + std::to_string(mGrowth.Gear)).c_str());
        }
        // the jars along the roads (once the map's ground is up), as the save has them
        if (mWaysideOn && !mWayside.Laid() && world.RuntimeScene)
        {
            mWayside.Lay(*world.RuntimeScene, mMap);
            mWayside.Restore(mSavedJars);
            char line[160]; std::snprintf(line, sizeof line, "[EAGLES ASCENT] %zu clay jars laid.\n", mWayside.Jars().size());
            OutputDebugStringA(line);
            if (mTrace) Evidence(world, ("wayside_jars_" + std::to_string(mWayside.Jars().size())).c_str());
        }
        // I: her window (AscentGrowth): her gear, what it gives, her gears to come; her crystals and what she carries. I, Esc
        // or a click on CLOSE closes. While it is open the pointer shows and the mouse leaves the camera alone. The climb waits.
        if (!mStudio) mHereKind = HereKind(p.Position);   // (her window's stage is over the summit: what is gathered where she really stands)
        mGrowthFade = std::clamp(mGrowthFade + (mGrowthOpen ? 1.f : -1.f) * realDt / .18f, 0.f, 1.f);
        mGrowthFlash = std::max(0.f, mGrowthFlash - realDt * 2.5f);
        mSavedMark = std::max(0.f, mSavedMark - realDt);
        for (float& f : mCrystalFlash) f = std::max(0.f, f - realDt * 2.f);
        if (!mPaused && !mSmoke && !mClimb && !mTour && !mQuestsOpen && alive && Press('I', inputAllowed))
        {
            mGrowthOpen = !mGrowthOpen; Sound::PlayUi(mGrowthOpen ? "ui/open" : "ui/close", .6f);
            if (!mGrowthOpen) { session.FreeCursor = false; mHoldBlade = true; }
        }
        StudioKeep(world);
        if (mShowTest && !mGrowthOpen) ShowTest(host, realDt);
        // her window shows her (HerShowcase): the camera before her while it is open, given back when it closes
        if (mGrowthOpen != mShow.On)
        {
            if (mGrowthOpen) { StudioIn(world); mShow.Begin(world.MainCamera); } else { mShow.End(world.MainCamera); StudioOut(world); }
            kit.SetShowcase(mGrowthOpen);
        }
        if (mGrowthOpen)
        {
            session.FreeCursor = true;
            bool close = session.EscapePressed || !alive;
            session.EscapePressed = false;
            if (mMouse.X >= 0 && mMouse.Click)
            {   // a click on CLOSE closes; anywhere else, she is turned (HerShowcase)
                if (Pointer::Hit(Pointer::CloseKey, mMouse.X, mMouse.Y) >= 0) close = true;
                else mShow.Grab(mMouse.X, mMouse.Y);
            }
            if (mShow.Dragging) { if (mMouse.Down && mMouse.X >= 0) mShow.Drag(mMouse.X, mMouse.Y, realDt, mHudH); else mShow.Release(); }
            if (mMouse.Wheel != 0) mShow.Wheel(mMouse.Wheel);
            if (close) { mGrowthOpen = false; session.FreeCursor = false; mHoldBlade = true; Sound::PlayUi("ui/close", .6f); mShow.End(world.MainCamera); StudioOut(world); kit.SetShowcase(false); }
            kit.SetInputBlocked(true);
            kit.Update(world, host.WorldRender, host.Combat, 0, realDt, false);
            if (mGrowthOpen)
            {
                const float tall = 1 + FormScale * static_cast<float>(AscentGrowth::Form(mGrowth));
                const auto* scene = world.RuntimeScene;
                mShow.Place(world.MainCamera, p.Position, p.Yaw, tall, realDt, [this](float x, float z) { return mMap.Land(x, z); },
                            [this, scene](const XMFLOAT3& from, const XMFLOAT3& to)
                            {   // the scene's collision, as the engine's own follow camera keeps out of it; and the drawn rock,
                                // which is not all in it (a cliff beside a nest): along the line, the first point under the
                                // rock's top there (the map's 1 m grid of it, FlightLand), less a margin
                                float room = -1;
                                DoorwaysRuntimeCameraObstructionResult hit;
                                if (scene && scene->Loaded && QueryDoorwaysRuntimeSceneCameraObstruction(*scene, from, to, .25f, .15f, hit) && hit.Hit) room = hit.ResolvedDistance;
                                const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z, length = std::sqrt(dx * dx + dy * dy + dz * dz);
                                for (float t = .3f; t <= length + .3f && length > 1e-3f; t += .1f)
                                {
                                    const float u = t / length, x = from.x + dx * u, y = from.y + dy * u, z = from.z + dz * u;
                                    if (y < mMap.FlightLand(x, z) + .2f) { const float r = (std::max)(0.f, t - .45f); room = room < 0 ? r : (std::min)(room, r); break; }
                                }
                                return room;
                            });
            }
            if (mShowTest) ShowTest(host, realDt);   // (its views are judged where the camera has just been put)
            return;
        }
        // Tab: her quests (AscentQuests), anywhere: the quest market (Q until 0.5.4; the owner, 2026-11-26: "i also want to
        // change quest market buton and it will be tab, keep tab changing maps while M"). W / S or the pointer choose one,
        // Enter or a click on its TAKE buys it; Tab, Esc or a click on CLOSE closes. The climb waits while they are open, and
        // the pointer shows. (A quest just done goes from the list on the HUD: its row's seconds pass here, so not while a
        // map or her window is open over it.)
        mQuestsFade = std::clamp(mQuestsFade + (mQuestsOpen ? 1.f : -1.f) * realDt / .18f, 0.f, 1.f);
        mQuestFlash = std::max(0.f, mQuestFlash - realDt * 2.f);
        if (!mQuestsOpen) mQuestLeaving.Pass(realDt);
        if (mQuestsOpen)
        {
            session.FreeCursor = true;
            const int set = AscentQuests::OpenSet(mGrowth);
            const std::vector<int> rows = set >= 0 ? AscentQuests::Rows(set) : std::vector<int>{};
            const int count = static_cast<int>(rows.size());
            bool close = session.EscapePressed || !alive || tabKey || tabTest;
            session.EscapePressed = false;
            bool take = Pressed(VK_RETURN, inputAllowed) || Pressed(VK_SPACE, inputAllowed);
            const int move = (Pressed('S', inputAllowed) || Pressed(VK_DOWN, inputAllowed) ? 1 : 0) - (Pressed('W', inputAllowed) || Pressed(VK_UP, inputAllowed) ? 1 : 0);
            if (move != 0 && count > 0) { mQuestRow = (mQuestRow + move + count) % count; Sound::PlayUi("ui/move", .5f); }
            if (mMouse.X >= 0 && (mMouse.Moved || mMouse.Click))
            {   // the pointer: the quest it is over is chosen; a click on its TAKE buys it, on CLOSE closes
                const int key = Pointer::Hit(Pointer::QuestTake, mMouse.X, mMouse.Y), row = key >= 0 ? key : Pointer::Hit(Pointer::QuestRow, mMouse.X, mMouse.Y);
                if (row >= 0 && row < count && row != mQuestRow) { mQuestRow = row; Sound::PlayUi("ui/move", .5f); }
                if (mMouse.Click)
                {
                    if (key >= 0) take = true;
                    else if (Pointer::Hit(Pointer::CloseKey, mMouse.X, mMouse.Y) >= 0) close = true;
                }
            }
            if (take && mQuestRow >= 0 && mQuestRow < count && !TakeQuest(rows[static_cast<size_t>(mQuestRow)])) Sound::PlayUi("ui/deny", .7f);
            if (close) { mQuestsOpen = false; session.FreeCursor = false; mHoldBlade = true; Sound::PlayUi("ui/close", .6f); }
            kit.SetInputBlocked(true);
            kit.Update(world, host.WorldRender, host.Combat, 0, realDt, false);
            return;
        }
        // Esc: her menu (DrawMenu). Its settings window stands over it and has the keys while it is open.
        mFocused = inputAllowed;
        mMenuFade = std::clamp(mMenuFade + (mPaused ? 1.f : -1.f) * realDt / .15f, 0.f, 1.f);
        const bool settings = Settings::IsOpen();
        if (session.EscapePressed)
        {
            session.EscapePressed = false;
            if (settings) Settings::Back();
            else { mPaused = !mPaused; mMenuRow = 0; mHoldBlade = true; Sound::PlayUi(mPaused ? "ui/open" : "ui/close", .7f); }
        }
        session.FreeCursor = mPaused;   // the pointer shows and the mouse leaves the camera alone
        if (!mPaused && settings) Settings::Close();
        if (mPaused)
        {
            if (!settings)
            {
                bool take = Press(VK_RETURN, inputAllowed) || Press(VK_SPACE, inputAllowed);
                if (mMouse.X >= 0 && (mMouse.Moved || mMouse.Click))
                {   // the pointer: the row it moves over is chosen, a click takes it
                    const int hit = Pointer::Hit(Pointer::PauseRow, mMouse.X, mMouse.Y);
                    if (hit >= 0)
                    {
                        if (hit != mMenuRow) { mMenuRow = hit; Sound::PlayUi("ui/move", .5f); }
                        if (mMouse.Click) take = true;
                    }
                }
                const int move = (Press('S', inputAllowed) || Press(VK_DOWN, inputAllowed) ? 1 : 0) - (Press('W', inputAllowed) || Press(VK_UP, inputAllowed) ? 1 : 0);
                if (move != 0) { mMenuRow = (mMenuRow + move + MenuRows) % MenuRows; Sound::PlayUi("ui/move", .5f); }
                if (take)
                {
                    using Request = DoorwaysGameSession::Request;
                    switch (mMenuRow)
                    {
                    case 0: mPaused = false; break;
                    case 1: mPaused = false; if (alive) { mGrowthOpen = true; Sound::PlayUi("ui/open", .6f); } break;
                    case 2: mPaused = false; mView.Toggle(session, world.MainCamera, mMap.Trail, mMap.Summit); mView.Explored = true; break;
                    case 3: Settings::Open(); break;   // her menu stays open under it
                    case 4: mPaused = false; mMap.Visit(host, mMap.Checkpoint); Notify(BackTitle(), Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name), 3); break;
                    case 5: if (!mMap.Nests.empty()) { mPaused = false; NewClimb(host); } break;
                    case 6: mPaused = false; session.MenuRequest = Request::MainMenu; break;
                    default: session.MenuRequest = Request::Exit; break;
                    }
                    if (!mPaused) { session.FreeCursor = mGrowthOpen; mHoldBlade = true; }   // (Nyxara: her window, the pointer still free)
                }
            }
            if (mNature) Nature(host, realDt);   // (the nature test pictures her menu: it goes on while she is paused)
            kit.SetInputBlocked(true);
            kit.Update(world, host.WorldRender, host.Combat, 0, realDt, false);
            return;
        }
        const bool allowed = inputAllowed && alive && !mSmoke && !mClimb && !mTour;

        // Space held 1 s takes her up (NyxaraKit: her flying energy lasts 30 s, then she falls). R in flight lands her
        // early (she sinks at LandSpeed until she stands); on foot R does nothing (owner, 2026-11-18).
        if (Press('R', allowed) && flying)
        {
            session.FreeFlightEnd = true; session.FreeFlightLandSpeed = LandSpeed;
            Sound::Play("nyxara/wing", .8f);
        }
        if (flying && !mFlying)
            Notify(L"IN FLIGHT", L"W flies where you look. Space climbs, Ctrl+Space sinks, R lands. Her wings last " +
                   std::to_wstring(static_cast<int>(kit.FlightSeconds())) + L" seconds (I: grow them).", 4);
        if (!flying)
        {
            if (mFlying) { Sound::Play("nyxara/land", .6f); session.FreeFlightLandSpeed = 0; }
            session.FreeFlightEnd = false;
        }
        mFlying = flying;
        if (Press(VK_BACK, allowed)) { mMap.Visit(host, mMap.Checkpoint); mMap.Rescued = 3; Notify(BackTitle(), Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name), 3); }

        // her speed: the road's. On foot a jog (Shift runs, Ctrl walks); in flight the engine uses her FlightSpeed, unchanged
        p.MoveSpeed = FootSpeed * kit.SpeedScale();
        session.FastMultiplier = flying ? WingBoostMultiplier : FootRunSpeed / FootSpeed;
        kit.SetOnFoot(!flying);
        kit.SetHeightOverGround(std::max(0.f, p.Position.y - mMap.Land(p.Position.x, p.Position.z)));
        if (mHoldBlade && !mMouse.Down) mHoldBlade = false;   // the click that closed a window is no sword blow: they wait for the button to be let go
        kit.SetInputBlocked(mSmoke || mClimb || mTour || mHoldBlade);
        {   // her gear (0.5.3, AscentGrowth): her blades, the blows she takes, how long she flies; a little taller with each
            kit.SetDamageScale(AscentGrowth::Damage(mGrowth));
            kit.SetFlightSeconds(AscentGrowth::FlightSeconds(mGrowth.Gear));
            mCreatures.SetMountainTaken(AscentGrowth::DamageTaken(mGrowth));
            const int form = AscentGrowth::Form(mGrowth);
            const float tall = 1 + FormScale * static_cast<float>(form);
            world.AnimatedPlayerVisual.Scale = {tall, tall, tall};
            if (form > mFormShown && alive)
            {   // a gear given: she changes, in its light
                const auto tint = AscentGrowth::GearColour(form);
                const XMFLOAT4 light{tint.R, tint.G, tint.B, 1};
                kit.Effect({p.Position.x, p.Position.y + 1.3f, p.Position.z}, light, 120, 9.f, .34f);
                kit.RingEffect({p.Position.x, p.Position.y + .05f, p.Position.z}, 7.f, light, 1.6f);
                Notify(AscentGrowth::GearTitle(form), L"Her quests are done: wings, swords and armour, all new. Saved.", 6);
                mGrowthFlash = 1;
                Sound::Play("world/gift", .9f, 0);
            }
            mFormShown = form;
        }
        // Z: a potion, the only healing there is (0.5)
        if (Press(PotionKey, allowed))
        {
            if (mGrowth.Potions <= 0) kit.Notice(L"No potions. Clay jars beside the road hold them.", 2.2f);
            else if (host.Combat.RestorePlayerHealth(world, PotionHeal) > 0)
            {
                --mGrowth.Potions; Sound::Play("nyxara/potion", .9f, 0);
                kit.Effect({p.Position.x, p.Position.y + 1.2f, p.Position.z}, {1.f, .35f, .4f, 1}, 26, 3.5f, .24f);
            }
            else kit.Notice(L"She is not wounded.", 1.8f);
        }
        // H: saved, at any Eagle Nest, as often as she likes
        if (Press('H', allowed) && !mMap.Nests.empty())
        {
            const int station = NestAt(p.Position);
            if (station > 0) SaveHere(station);
            else Notify(L"NOT SAVED", L"She can save only standing in an Eagle Nest.", 3);
        }
        // Tab: her quests, anywhere (the climb waits while they are open); a nest a quest asks her to reach; a set done, its gear
        if (alive && (tabTest || (tabKey && allowed))) { QuestsOpen(); return; }
        if (alive) if (const int station = NestAt(p.Position); station > 0) ReachNest(station);
        if (alive) GearIfEarned();
        if (mSmoke) Smoke(host, realDt);
        if (mClimb) Climb(host, dt);
        if (mTour) Tour(host, dt);
        if (mBossFilm) BossFilm(host, realDt);
        if (mCameraTest) CameraTest(host, realDt);
        if (mSideWalk) SideWalk(host, realDt);
        if (mWays) Ways(host, realDt);
        if (mNature) Nature(host, realDt);
        if (mLedger) Ledger(host);
        kit.Update(world, host.WorldRender, host.Combat, dt, realDt, allowed);
        if (mCreaturesOn)
        {   // the creatures after her (as in the Character Lab): they wake, wait, fight, go home and sleep
            mCreatures.UpdateMountain(world, host.Combat, dt);
            for (const auto& news : mCreatures.TakeMountainNews())
            {
                Notify(news.Title, news.Line, news.Beaten ? 6.f : 4.5f);
                if (mTrace) Evidence(world, news.Beaten ? "boss_beaten" : "boss_roused");
            }
            if (mTrace)
                for (const auto& line : mCreatures.TakeMountainTrace()) Evidence(world, ("creature_" + line).c_str());
            mBeatenWritten = std::min(mBeatenWritten, mCreatures.MountainBeaten().size());
            while (mCreatures.MountainBeaten().size() > mBeatenWritten)
            {   // a creature she beat leaves its crystals (0.5: it stays beaten only once she saves at a nest)
                const std::string id = mCreatures.MountainBeaten()[mBeatenWritten++];
                for (const auto& c : mCreatureList)
                    if (c.Id == id) { GiveCrystals(AscentGrowth::CrystalKindOf(GroupStretch(c.Group)), Journey::KindCrystals(c.Kind)); break; }
                Sound::Play("world/crystal", .6f, .1f);
                CountQuests(AscentQuests::Beat, 1);
                if (mTrace) Evidence(world, ("beaten_" + id).c_str());
            }
        }

        // the map: where she is, the air, the checkpoints, the safety nets
        mMap.Locate(p.Position);
        const int before = mMap.Checkpoint;
        if (const char* net = mMap.Air(host, dt, flying, !mSmoke && !mTour && !mSideWalk && mClock > 5 && alive))
        {
            Notify(BackTitle(), L"The mountain did not hold her: she is back at " + Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name) + L".", 4);
            if (mClimb) { Evidence(host.World, net); Evidence(host.World, "CLIMB_FAIL"); mClimb = false; PostQuitMessage(2); }
        }
        else if (mMap.Checkpoint != before && (mMap.Checkpoint > mReached || !mMap.Nests.empty()))
        {   // an Eagle Nest: H saves her here (0.5: it no longer heals, nor saves by itself)
            mReached = std::max(mReached, mMap.Checkpoint);
            const bool nest = !mMap.Nests.empty() && mMap.Checkpoint > 0;
            const std::wstring where = Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name);
            if (nest)
            {
                Notify(L"EAGLE NEST", where + L". Press H to save here.", 4.5f);
                if (mTrace) Evidence(world, "nest_rested");
            }
            else Notify(L"CHECKPOINT", where, 4);
            Sound::PlayUi("ui/open", .6f);
        }

        if (mWaysideOn && mWayside.Laid()) mWayside.Update(dt);
        // she fell (to the creatures, or the engine's own harm): she lies a moment and wakes at the Eagle Nest she last
        // saved at, whole; all since that save is undone (RollBack)
        if (!alive)
        {
            if (!mWasDead)
            {
                mDeadFor = 0; mRespawnAsk = 0; Sound::Play("nyxara/death", 1, 0);
                Notify(L"NYXARA HAS FALLEN", mMap.Nests.empty() ? L"She wakes at the last checkpoint." : L"She wakes at the Eagle Nest she last saved at. All since then is undone.", DeathSeconds + 1.5f);
            }
            mDeadFor += realDt;
            if (mDeadFor >= DeathSeconds && (mRespawnAsk -= realDt) <= 0)
            {
                mRespawnAsk = .5f;
                const int wake = mMap.Nests.empty() ? mMap.Checkpoint : mSavedNest;
                const auto& s = mMap.Stations[static_cast<size_t>(wake)];
                const XMFLOAT3& rest = mMap.Rest(wake);   // she wakes in the nest
                session.SpawnPosition = {rest.x, rest.y + .05f, rest.z}; session.SpawnYaw = s.Yaw;
                const auto& life = host.Combat.PlayerLife();
                host.Combat.RequestRespawn({life.Life, life.Death.Event, life.Death.Destination});
            }
        }
        else if (mWasDead)
        {
            if (!mMap.Nests.empty()) RollBack();
            mMap.Visit(host, mMap.Checkpoint);
            host.Combat.RestorePlayerHealth(world, 10000);   // (she wakes whole)
        }
        mWasDead = !alive;
    }

    void EaglesAscentGame::DrawShadowCasters(Renderer& renderer, const WorldState& world, const XMMATRIX& lightViewProjection)
    {
        mTrees.DrawShadows(renderer, world.MainCamera.Position, lightViewProjection);
    }

    void EaglesAscentGame::Draw(Renderer& renderer, const WorldState& world, const XMMATRIX& viewProjection)
    {
        mRenderer = &renderer;
        mMap.DrawModels(renderer, world, viewProjection, 0);
        {   // (the trees between the camera and her drawn see-through, while the camera follows her)
            const auto& her = world.MainPlayer.Position; const auto& eye = world.MainCamera.Position;
            const XMFLOAT3 chest{her.x, her.y + 1.2f, her.z};
            const float dx = chest.x - eye.x, dy = chest.y - eye.y, dz = chest.z - eye.z;
            mTrees.SetFocus(chest, dx * dx + dy * dy + dz * dz < 30.f * 30.f);
        }
        mTrees.Draw(renderer, world.MainCamera.Position, viewProjection);
        mGrass.Draw(renderer, world.MainCamera.Position, viewProjection);
        DrawNests(renderer, world, viewProjection);
        if (mCreaturesOn) mCreatures.DrawMountain(renderer, world, viewProjection);
        {   // her gear's lights: on her wings, at her blades and at her feet, in its colour (THE HIGHLANDS gold, AMIRANI'S
            // FIRE embers, MOONLIT moonlight)
            const auto& her = world.MainPlayer;
            const float tall = 1 + FormScale * static_cast<float>(AscentGrowth::Form(mGrowth));
            const XMFLOAT3 forward{std::sin(her.Yaw), 0, std::cos(her.Yaw)}, side{std::cos(her.Yaw), 0, -std::sin(her.Yaw)};
            const auto light = [&](const AscentGrowth::Tint& tint, float t, XMFLOAT3 local, float size, float alpha)
            {
                const XMFLOAT4 color{tint.R, tint.G, tint.B, alpha * (.8f + .2f * std::sin(mClock * 2.6f + local.x * 3))};
                mWayside.Glow({her.Position.x + side.x * local.x + forward.x * local.z, her.Position.y + local.y * tall, her.Position.z + side.z * local.x + forward.z * local.z},
                              size * (1 + .12f * t), color);
            };
            if (const int gear = AscentGrowth::Form(mGrowth))
            {   // her gear's light, stronger with each
                const auto tint = AscentGrowth::GearColour(gear);
                const float t = static_cast<float>(gear);
                for (const float x : {-1.f, 1.f})
                {
                    light(tint, t, {x * .75f, 1.75f, -.45f}, .55f, .10f + .05f * t);
                    light(tint, t, {x * 1.45f, 2.15f, -.55f}, .40f, .08f + .04f * t);
                    light(tint, t, {x * .42f, .95f, .30f}, .26f, .10f + .05f * t);
                }
                light(tint, t, {0, 1.2f, 0}, .60f, .04f + .025f * t);
                light(tint, t, {0, .12f, 0}, .85f, .08f + .04f * t);
            }
        }
        mWayside.Draw(renderer, world, viewProjection, GetNyxaraKit().GlowTexture());
        GetNyxaraKit().Draw(renderer, world, viewProjection);
    }

    void EaglesAscentGame::AdjustEnvironment(EnvironmentSettings& environment)
    {
        EaglesAscent::StyleEnvironment(environment, 0, mView.Open ? 0 : 1);   // day, with the map's haze (Map A Gate 4); clear air for the whole mountain (M)
    }

    void EaglesAscentGame::DrawStartScreen(const DoorwaysHudRenderContext& context, HudTextRenderer& text, bool focused)
    {
        StartScreen::Draw(context, text, focused);
    }

    bool EaglesAscentGame::DrawHud(const DoorwaysHudRenderContext& context, HudTextRenderer& text)
    {
        if (!mTourShot.empty() && mRenderer && !mTourShots.empty())
        {   // the tour's picture of this stop (the world as drawn, before the HUD): BMP, made PNG by its runner
            std::error_code ignored; std::filesystem::create_directories(mTourShots, ignored);
            mRenderer->CaptureFrameToBmpAsync(mTourShots / (mTourShot + L".bmp"));
        }
        mTourShot.clear();
        if (mSmoke) return true;
        Ui ui(context, text);
        mHudW = ui.W; mHudH = ui.H; mHudS = ui.S;
        const float s = std::clamp(ui.S, .8f, 1.5f);
        if (mChart.Open)
        {   // M: the detailed map (AscentMap): the way, the rings, the caves, the nests, the bosses, the creatures left, her
            const auto& p = context.World.MainPlayer;
            AscentMap::Data d;
            d.Trail = &mMap.Trail; d.Rings = &mMap.Rings();
            for (const auto& cave : mMap.Caves())
            {
                std::vector<XMFLOAT3> line;
                for (const auto& q : cave.Line) line.push_back({q.X, q.Y, q.Z});
                d.Caves.push_back(std::move(line));
            }
            const auto height = [](float y) { return std::to_wstring(static_cast<int>(std::lround((std::max)(0.f, y)))) + L" m high"; };
            for (size_t i = 0; i < mMap.Nests.size(); ++i)
            {
                const auto& n = mMap.Nests[i];
                const bool saved = n.Station == mSavedNest;
                d.Marks.push_back({n.P, saved ? AscentMap::NestSaved : AscentMap::Nest, L"Eagle Nest " + std::to_wstring(i + 1),
                                   height(n.P.y) + (saved ? L"  -  her save is here" : L"  -  H saves"), 0});
            }
            if (!mMap.Stations.empty())
            {
                const auto top = std::max_element(mMap.Stations.begin(), mMap.Stations.end(), [](const auto& a, const auto& b) { return a.P.y < b.P.y; });
                d.Marks.push_back({top->P, AscentMap::Summit, L"Amiran's summit", height(top->P.y) + L"  -  the end of the way", 0});
                d.Marks.push_back({mMap.Stations.front().P, AscentMap::Start, L"The shore", L"Where the climb begins", 0});
            }
            if (mCreaturesOn)
            {
                const auto& beaten = mCreatures.MountainBeaten();
                std::vector<int> left(mMap.Placements.size(), 0);
                std::vector<std::vector<std::pair<int, int>>> kinds(mMap.Placements.size());   // (kind, how many) left in each group
                for (const auto& c : mCreatureList)
                {
                    if (c.Group < 0 || c.Group >= static_cast<int>(left.size()) || std::find(beaten.begin(), beaten.end(), c.Id) != beaten.end()) continue;
                    ++left[static_cast<size_t>(c.Group)];
                    auto& k = kinds[static_cast<size_t>(c.Group)];
                    auto found = std::find_if(k.begin(), k.end(), [&](const auto& e) { return e.first == c.Kind; });
                    if (found == k.end()) k.push_back({c.Kind, 1}); else ++found->second;
                }
                for (size_t i = 0; i < mMap.Placements.size(); ++i)
                {
                    const auto& pl = mMap.Placements[i];
                    std::wstring who;
                    for (const auto& [kind, count] : kinds[i]) who += (who.empty() ? L"" : L", ") + (count > 1 ? std::to_wstring(count) + L" " : L"") + Journey::KindTitle(kind);
                    if (pl.Boss)
                        d.Marks.push_back({pl.P, left[i] ? AscentMap::Boss : AscentMap::BossBeaten, std::wstring(Journey::KindTitle(Journey::KindByName(pl.Kinds[0]))),
                                           left[i] ? height(pl.P.y) + L"  -  a boss" : L"Beaten", left[i]});
                    else if (left[i])
                        d.Marks.push_back({pl.P, pl.Cave >= 0 ? AscentMap::CaveGroup : AscentMap::Group, who, height(pl.P.y) + (pl.Cave >= 0 ? L"  -  in a cave" : L""), left[i]});
                }
            }
            d.Her = p.Position; d.HerYaw = p.Yaw; d.LookYaw = context.World.MainCamera.Yaw;
            d.Flying = p.TraversalMode == DoorwaysPlayerTraversalMode::Flight; d.Summit = mMap.Summit;
            d.Land = [this](float x, float z) { return mMap.Land(x, z); };
            d.ZoneAt = [this](float y)
            {
                const auto* zone = mMap.ZoneAt(y);
                std::wstring place = zone ? Words(zone->Name) : L"";
                if (const size_t cut = place.find(L' '); cut != std::wstring::npos && place.size() > 5 && place[0] == L'M' && place[1] == L'A') place = place.substr(cut + 1);
                if (const size_t cut = place.find(L' '); cut != std::wstring::npos && place.size() > 1 && std::iswupper(place[0]) && std::iswupper(place[1])) place = place.substr(cut + 1);
                return place;
            };
            mChart.Draw(ui, d);
            DrawMenu(ui);
            return true;
        }
        if (mView.Open)
        {   // the M view: the nests, the bosses (beaten: dim) and the groups not yet beaten (dots)
            std::vector<MountainView::Mark> marks;
            for (const auto& n : mMap.Nests)
                marks.push_back({n.P, n.Station == mMap.Checkpoint ? L"EAGLE NEST (saved here)" : L"EAGLE NEST", Palette::Gold, 8});
            if (mCreaturesOn)
            {
                const auto& beaten = mCreatures.MountainBeaten();
                std::vector<int> left(mMap.Placements.size(), 0);
                for (const auto& c : mCreatureList)
                    if (c.Group >= 0 && c.Group < static_cast<int>(left.size()) && std::find(beaten.begin(), beaten.end(), c.Id) == beaten.end()) ++left[static_cast<size_t>(c.Group)];
                for (size_t i = 0; i < mMap.Placements.size(); ++i)
                {
                    const auto& pl = mMap.Placements[i];
                    if (pl.Boss)
                        marks.push_back({pl.P, std::wstring(Journey::KindTitle(Journey::KindByName(pl.Kinds[0]))) + (left[i] ? L"" : L"  (beaten)"), left[i] ? Palette::Red : Palette::Dim, 10});
                    else if (left[i])
                        marks.push_back({pl.P, L"", pl.Cave >= 0 ? Color{.85f, .45f, .3f, .9f} : Color{.8f, .3f, .3f, .9f}, 4});
                }
            }
            mView.DrawHud(ui, context.World.MainCamera, context.World.MainPlayer.Position, mMap.Summit, marks);
            return true;
        }
        if (mGrowthOpen && mGrowthFade > .5f)
        {   // her window open, she shown in it (HerShowcase): nothing of the climb's HUD over her, only the window
            AscentGrowth::View v;
            v.Now = mGrowth; v.Fade = mGrowthFade; v.Flash = mGrowthFlash;
            v.PotionHeal = PotionHeal; v.AtNest = NestAt(context.World.MainPlayer.Position) > 0; v.HereKind = mHereKind;
            AscentGrowth::Draw(ui, v);
            DrawMenu(ui);
            Settings::Draw(ui, mFocused, false);
            return true;
        }
        if (NyxaraHud().Valid && context.World.CombatFeedback.MeleeActive) DrawNyxaraHud(ui, context);
        else if (NyxaraHud().Valid)
        {   // her flying energy (NyxaraKit: Space held 1 s takes her up; 30 s of flight), at the bottom in the middle
            const auto& wings = NyxaraHud();
            const float bw = 260 * s, bx = (ui.W - bw) * .5f, by = ui.H - 46 * s;
            ui.Rect(bx - 6 * s, by - 20 * s, bw + 12 * s, 36 * s, {.015f, .025f, .04f, .7f});
            ui.Text(wings.Flying ? L"WINGS  (R lands)" : wings.FlightCharge > 0 ? L"WINGS  taking off..." : L"WINGS  (hold Space 1 s to fly)", bx, by - 17 * s, 11 * s / DoorwaysUiFontHeight, Palette::Ink);
            ui.Bar(bx, by, bw, 8 * s, wings.Flight, wings.Flight < .1f ? Palette::Red : Palette::Soul, 0, true);
            if (wings.FlightCharge > 0) ui.Rect(bx, by + 10 * s, bw * (std::min)(1.f, wings.FlightCharge), 2 * s, Palette::Gold);
        }
        const float body = 13 * s / DoorwaysUiFontHeight, title = 16 * s / DoorwaysUiFontHeight, big = 24 * s / DoorwaysUiFontHeight, tiny = 12 * s / DoorwaysUiFontHeight;
        const auto& p = context.World.MainPlayer;

        // where she is: the height of the mountain and its zone, at the top left
        const auto* zone = mMap.ZoneAt(p.Position.y);
        std::wstring place = zone ? Words(zone->Name) : L"";
        if (const size_t cut = place.find(L' '); cut != std::wstring::npos && place.size() > 5 && place[0] == L'M' && place[1] == L'A') place = place.substr(cut + 1);   // "MA05 SHRINE Eagle shrine..." -> without the id
        if (const size_t cut = place.find(L' '); cut != std::wstring::npos && !place.empty() && std::iswupper(place[0]) && std::iswupper(place[1])) place = place.substr(cut + 1);
        std::wostringstream line; line << std::fixed << std::setprecision(0) << std::max(0.f, p.Position.y) << L" m  of  " << mMap.Summit << L" m";
        ui.Rect(14 * s, 14 * s, 330 * s, 54 * s, {.015f, .025f, .04f, .7f});
        ui.Text(line.str(), 24 * s, 20 * s, title, Palette::Gold);
        ui.Text(place + (mMap.AirNow[0] ? std::wstring(L"   |   ") + mMap.AirNow : L""), 24 * s, 44 * s, body, Palette::Ink);

        // the height ruler at the right: the checkpoints and where she is
        const float top = 90 * s, bottom = ui.H - 90 * s, x = ui.W - 30 * s;
        const auto at = [&](float y) { return bottom - (bottom - top) * std::clamp(y / std::max(mMap.Summit, 1.f), 0.f, 1.03f); };
        ui.Rect(x - 2 * s, top, 4 * s, bottom - top, {.02f, .03f, .05f, .6f});
        for (size_t i = 0; i < mMap.Stations.size(); ++i)
            if (mMap.Stations[i].Checkpoint)
                ui.Rect(x - 7 * s, at(mMap.Stations[i].P.y) - 1 * s, 14 * s, 2 * s, static_cast<int>(i) == mMap.Checkpoint ? Palette::Gold : Palette::Dim);
        ui.Rect(x - 11 * s, at(p.Position.y) - 2 * s, 22 * s, 4 * s, Palette::Soul);

        // (no words hang over the way any more: the nests and where the road needs wings were titled in the world until
        // 2026-11-22; the owner: "no titles on roads ... leave it visible on M map". The M view still names the nests.)

        {   // what she carries (0.5.3): wind, fire and ice crystals, potions (Z), and her window (I), under where she is
            const float py = 76 * s, kh = 20 * s, icon = 19 * s, cw = 58 * s;
            ui.Rect(14 * s, py, 396 * s, 34 * s, {.015f, .025f, .04f, .7f});
            const int here = HereKind(p.Position);
            for (int k = 0; k < AscentGrowth::CrystalKinds; ++k)
            {
                const auto t = AscentGrowth::CrystalColour(k);
                const Color c{t.R, t.G, t.B, 1};
                const float cx = 34 * s + static_cast<float>(k) * cw, flash = mCrystalFlash[k];
                ui.Icon(HudArt::IconCrystal, cx, py + 17 * s, icon * (1 + .35f * flash), c);
                ui.Text(std::to_wstring(mGrowth.AscentCrystals[k]), cx + 13 * s, py + 9 * s, body, flash > 0 ? Palette::White : Palette::Ink);
                if (k == here) ui.Rect(cx - 10 * s, py + 30 * s, cw - 8 * s, 2 * s, c.Fade(.85f));   // (what she gathers here)
            }
            const wchar_t* z = Ui::KeyLabel(L"Z", PotionKey);
            const float zw = ui.KeyWidth(z, kh), zx = 34 * s + 3 * cw + 6 * s + zw * .5f;
            ui.Key(z, zx, py + 7 * s, kh, mGrowth.Potions > 0 ? Palette::White : Palette::Faint);
            for (int i = 0; i < MaxPotions; ++i)
                ui.Icon(HudArt::IconPotion, zx + zw * .5f + 6 * s + (static_cast<float>(i) + .5f) * 17 * s, py + 17 * s, 17 * s, i < mGrowth.Potions ? Palette::Red : Palette::Faint.Fade(.6f));
            const float ix = 352 * s;
            ui.Key(Ui::KeyLabel(L"I", 'I'), ix, py + 7 * s, kh, Palette::White);
            ui.Text(L"GEAR", ix + 14 * s, py + 10 * s, tiny, Palette::Dim);
            // (the quests she is on, down to over her health at the most: NyxaraHud's emblem is 114 units up from the bottom)
            if (!mQuestsOpen) AscentQuests::DrawTaken(ui, mGrowth, mQuestLeaving, 14 * s, py + 40 * s, ui.H - 124 * s);
        }
        if (NestAt(p.Position) > 0 && !mGrowthOpen && !mQuestsOpen)
        {   // in a nest: H saves
            const float y = ui.H * .70f, kh = 26 * s;
            const std::wstring words = NestAt(p.Position) == mSavedNest ? L"SAVE AGAIN AT THIS EAGLE NEST" : L"SAVE AT THIS EAGLE NEST";
            const float kw = ui.KeyWidth(L"H", kh), w = kw + 10 * s + ui.Width(words, body);
            const float left = ui.W * .5f - w * .5f;
            ui.Rect(left - 12 * s, y - 6 * s, w + 24 * s, kh + 12 * s, {.015f, .025f, .04f, .75f});
            ui.Key(L"H", left + kw * .5f, y, kh, Palette::GoldLight);
            ui.Text(words, left + kw + 10 * s, y + (kh - ui.Height(body)) * .5f, body, Palette::Ink);
        }
        if (mSavedMark > 0)
        {   // saved: a gold word under the notice
            const float a = std::clamp(mSavedMark, 0.f, 1.f);
            ui.Centered(L"SAVED", ui.W * .5f, ui.H * .62f, big, Palette::GoldLight.Fade(a));
        }

        {   // the boss she fights: its name and its health, at the top in the middle
            std::wstring name; float health = 0;
            if (mCreaturesOn && mCreatures.MountainBoss(name, health))
            {
                const float bw = 520 * s, bx = (ui.W - bw) * .5f, by = 44 * s;
                ui.Rect(bx - 8 * s, by - 28 * s, bw + 16 * s, 46 * s, {.015f, .025f, .04f, .75f});
                ui.Centered(name, ui.W * .5f, by - 24 * s, title, Palette::Gold);
                ui.Bar(bx, by, bw, 10 * s, health, Palette::Red, 0, true);
            }
        }

        // a notice in the middle, under the top
        if (mNotice > 0)
        {
            const float a = std::clamp(mNotice, 0.f, 1.f);
            ui.Centered(mNoticeTitle, ui.W * .5f, ui.H * .16f, big, Palette::Gold.Fade(a));
            ui.Centered(mNoticeLine, ui.W * .5f, ui.H * .16f + 30 * s, body, Palette::Ink.Fade(a));
        }
        if (mQuestsFade > 0)
        {   // her quests (Tab)
            AscentQuests::View v;
            v.Now = mGrowth; v.Selected = mQuestRow; v.Fade = mQuestsFade; v.Flash = mQuestFlash; v.FlashRow = mQuestFlashRow;
            AscentQuests::Draw(ui, v);
        }
        if (mGrowthFade > 0)
        {   // her window (I): her gear (AscentGrowth)
            AscentGrowth::View v;
            v.Now = mGrowth; v.Fade = mGrowthFade; v.Flash = mGrowthFlash;
            v.PotionHeal = PotionHeal; v.AtNest = NestAt(p.Position) > 0; v.HereKind = mHereKind;
            AscentGrowth::Draw(ui, v);
        }
        DrawMenu(ui);
        Settings::Draw(ui, mFocused, false);
        return true;
    }

    // Her menu (Esc): the road's panel (Journey::DrawPause), with the climb's rows.
    void EaglesAscentGame::DrawMenu(Ui& ui) const
    {
        if (mMenuFade <= 0) return;
        using namespace Palette;
        const float S = ui.S, a = mMenuFade;
        constexpr float Width = 420, Row = 46, Gap = 5, Pad = 16, Head = 96, Foot = 44, Tiny = 1.55f, Small = 2.0f, Name = 2.6f;
        struct Entry { const wchar_t* Label; const wchar_t* Note; };
        const Entry entries[MenuRows] = {
            {L"Continue the climb", L""}, {L"Nyxara", L"wings, swords, armour"}, {L"The mountain", L"seen from outside"},
            {L"Settings", L"picture, sound, mouse"}, {L"Back to the last nest", L"her last nest"},
            {L"A new climb", L"forgets the save"}, {L"To the start screen", L"H saves at nests"},
            {L"Leave the game", L"H saves at nests"}};
        const float w = Width * S, h = (Head + MenuRows * (Row + Gap) + Foot) * S;
        const float x = (ui.W - w) * .5f, y = (ui.H - h) * .5f;
        ui.Rect(0, 0, ui.W, ui.H, Black.Fade(.5f * a));
        ui.Nine(HudArt::Panel, x, y, w, h, White.Fade(a));
        ui.SpacedCentered(L"CAUCASIAN EAGLE", x + w * .5f, y + 16 * S, Tiny * S, 4 * S, Gold.Fade(a));
        ui.Centered(L"The mountain waits", x + w * .5f, y + 34 * S, Name * S, Ink.Fade(a));
        ui.Centered(L"Nothing moves while this is open.", x + w * .5f, y + 66 * S, Tiny * S, Dim.Fade(a));
        float cy = y + Head * S;
        for (int i = 0; i < MenuRows; ++i)
        {
            const bool selected = i == mMenuRow, off = i == 5 && mMap.Nests.empty();
            const float cx = x + Pad * S, cw = w - 2 * Pad * S, ch = Row * S;
            if (mPaused) Pointer::Add(Pointer::PauseRow, i, cx, cy, cw, ch);
            ui.Rect(cx, cy, cw, ch, Well.Fade(a * (selected ? .92f : .55f)));
            const Color edge = selected ? GoldLight.Fade(a * (.75f + .25f * std::sin(ui.Time * 4))) : Gold.Fade(a * .25f);
            const float line = (selected ? 2 : 1) * S;
            ui.Rect(cx, cy, cw, line, edge); ui.Rect(cx, cy + ch - line, cw, line, edge);
            ui.Rect(cx, cy, line, ch, edge); ui.Rect(cx + cw - line, cy, line, ch, edge);
            ui.Icon(HudArt::Diamond, cx + 20 * S, cy + ch * .5f, (selected ? 13 : 9) * S, (selected ? GoldLight : Faint).Fade(a));
            const Color ink = off ? Faint : i >= 5 ? (selected ? Red : Dim) : (selected ? Ink : Dim);
            ui.Text(entries[i].Label, cx + 40 * S, cy + (ch - ui.Height(Small * S)) * .5f, Small * S, ink.Fade(a));
            ui.RightAligned(entries[i].Note, cx + cw - 14 * S, cy + (ch - ui.Height(Tiny * S)) * .5f, Tiny * S, (selected ? Dim : Faint).Fade(a));
            cy += (Row + Gap) * S;
        }
        ui.Centered(L"W  S  or the mouse      ENTER  take      ESC  back", x + w * .5f, cy + 10 * S, Tiny * S, Dim.Fade(a));
    }

    // The placing model's creatures on their lines (Journey::SetMountainLines): the way is line 0, its points every
    // TrailStep m; each cave with creatures in it is a line of its own, measured along it as the placing model measures
    // it. The creatures of a group stand round its place: the first in the middle, the others to either side across the
    // way and a step behind, no farther out than the way (WayHalf) keeps them.
    void EaglesAscentGame::StartCreatures()
    {
        constexpr float WayHalf = 3.5f;   // m to either side of the way's middle its creatures keep to (it is paved 8 m wide)
        constexpr float AroundHalf = 5.f; // m to either side of a group around the way's line (from the way out to it)
        std::vector<std::vector<Journey::MountainPoint>> lines(1);
        for (size_t i = 0; i < mMap.Trail.size(); ++i) lines[0].push_back({mMap.Trail[i], WayHalf, static_cast<float>(i) * EaglesAscent::TrailStep});
        std::vector<int> caveLine(mMap.Caves().size(), -1);
        mCreatureList.clear();
        int group = 0;
        for (const auto& p : mMap.Placements)
        {
            int line = 0;
            float half = WayHalf;
            if (p.Cave >= 0)
            {
                const auto& cave = mMap.Caves()[static_cast<size_t>(p.Cave)].Line;
                if (caveLine[static_cast<size_t>(p.Cave)] < 0)
                {
                    caveLine[static_cast<size_t>(p.Cave)] = static_cast<int>(lines.size());
                    lines.emplace_back();
                    float along = 0;
                    for (size_t i = 0; i < cave.size(); ++i)
                    {
                        if (i > 0) along += std::sqrt((cave[i].X - cave[i - 1].X) * (cave[i].X - cave[i - 1].X) + (cave[i].Y - cave[i - 1].Y) * (cave[i].Y - cave[i - 1].Y) +
                                                      (cave[i].Z - cave[i - 1].Z) * (cave[i].Z - cave[i - 1].Z));
                        lines.back().push_back({{cave[i].X, cave[i].Y, cave[i].Z}, std::max(.6f, cave[i].Half - .6f), along});
                    }
                }
                line = caveLine[static_cast<size_t>(p.Cave)];
                for (const auto& point : cave) half = std::min(half, std::max(.6f, point.Half - .6f));
            }
            float at = p.At, lo = p.Lo, hi = p.Hi;
            if (p.Around && p.Spur.size() >= 2)
            {   // a group around the way: its own line, from the way's middle out to its place and on past it (PlaceAround)
                line = static_cast<int>(lines.size());
                lines.emplace_back();
                float along = 0;
                for (size_t i = 0; i < p.Spur.size(); ++i)
                {
                    if (i > 0)
                    {
                        const XMFLOAT3 a = p.Spur[i - 1], b = p.Spur[i];
                        along += std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z));
                    }
                    lines.back().push_back({p.Spur[i], AroundHalf, along});
                }
                half = AroundHalf; at = p.SpurAt; lo = 0; hi = along;
            }
            const XMFLOAT3 ahead{std::sin(p.Yaw), 0, std::cos(p.Yaw)}, right{std::cos(p.Yaw), 0, -std::sin(p.Yaw)};
            for (size_t k = 0; k < p.Kinds.size(); ++k)
            {
                Journey::MountainCreature c;
                c.Kind = Journey::KindByName(p.Kinds[k]);
                if (c.Kind < 0) continue;
                c.Id = p.Boss ? p.Id : p.Id + "." + std::to_string(k);
                c.Group = group; c.Line = line; c.Yaw = p.Yaw; c.At = at; c.Lo = lo; c.Hi = hi; c.Roam = p.Roam;
                const float side = k == 0 ? 0.f : (k == 1 ? -1.f : 1.f) * std::min(1.1f + .45f * Journey::KindHeight(c.Kind), half - .3f);
                const float back = k == 0 ? 0.f : -1.4f;
                c.Home = {p.P.x + right.x * side + ahead.x * back, p.P.y, p.P.z + right.z * side + ahead.z * back};
                mCreatureList.push_back(c);
            }
            ++group;
        }
        mCreatures.SetMountainLines(lines);
        mCreatures.SetMountainCreatures(mCreatureList, mSavedBeaten);
        mBeatenWritten = mCreatures.MountainBeaten().size();
    }

    std::wstring EaglesAscentGame::BackTitle() const
    {
        return mMap.Nests.empty() ? L"BACK AT THE CHECKPOINT" : mMap.Checkpoint == 0 ? L"BACK ON THE SHORE" : L"BACK AT THE EAGLE NEST";
    }

    // eagles-ascent.sav: "rules 2" (0.5.2: her nine levels), "nest <station>", her crystals and potions, "level <item>
    // <level>" and "look <item> <level>" for her wings, swords and armour, then "beaten <id>" for every creature she has
    // beaten and "jar <id>" for every clay jar she has broken
    void EaglesAscentGame::LoadSave()
    {
        if (mSavePath.empty())
        {   // (the tour's own save, once set, stays: Tour reads it back)
            const std::string named = Env(AscentSaveEnvironmentVariable);
            mSaveTest = !named.empty();
            mSavePath = mSaveTest ? std::filesystem::path(named) : ResolveDoorwaysSettingsPaths().Player.parent_path() / AscentSaveFileName;
        }
        mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear();
        if (mMap.Nests.empty()) return;
        std::ifstream in(mSavePath);
        if (!in) return;
        std::string word;
        int rules = 1; bool any = false;
        while (in >> word)
        {
            if (word[0] == '#') { std::getline(in, word); continue; }
            any = true;
            if (word == "rules") in >> rules;
            else if (word == "nest") in >> mSavedNest;
            else if (word == "beaten") { std::string id; if (in >> id) mSavedBeaten.push_back(id); }
            else if (word == "crystals")
            {   // wind, fire, ice (0.5.3)
                std::string rest; std::getline(in, rest); std::istringstream counts(rest);
                for (int& c : mSavedGrowth.AscentCrystals) if (!(counts >> c)) { c = 0; break; }
            }
            else if (word == "potions") in >> mSavedGrowth.Potions;
            else if (word == "gear") in >> mSavedGrowth.Gear;
            else if (word == "quest")
            {   // its id, its state, how far (AscentQuests)
                std::string id; int state = 0, count = 0;
                if (in >> id >> state >> count) if (const int q = AscentQuests::QuestIndex(id); q >= 0) { mSavedGrowth.QuestState[q] = state; mSavedGrowth.QuestCount[q] = count; }
            }
            else if (word == "jar") { std::string id; if (in >> id) mSavedJars.push_back(id); }
            // (a "place <id>" line, in a save from before 2026-11-21's hidden places were taken away: skipped)
            else std::getline(in, word);
        }
        if (mSavedNest < 0 || mSavedNest >= static_cast<int>(mMap.Stations.size()) || !mMap.Stations[static_cast<size_t>(mSavedNest)].Checkpoint) mSavedNest = 0;
        if (any && rules < 3)
        {   // a save from before her gears (the nine levels, 0.5.2, or older): she starts the climb again (the owner, 2026-11-22)
            mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear();
        }
        AscentGrowth::Clamp(mSavedGrowth);
    }

    void EaglesAscentGame::WriteSave() const
    {
        if (GameSession().Unattended && !mSaveTest) return;   // (an unattended run never touches a player's save)
        std::error_code ignored;
        std::filesystem::create_directories(mSavePath.parent_path(), ignored);
        std::ofstream out(mSavePath, std::ios::trunc);
        out << "# The Eagle's Ascent: the Eagle Nest she saved at (H there), her gear (0.5.3: 0 her own .. 3), her crystals (wind fire ice)\n";
        out << "# and potions, her quests she has taken, the creatures she has beaten, the clay jars she has broken, as they were when she saved\n";
        out << "rules 3\n";
        out << "nest " << mSavedNest << "\n";
        out << "crystals " << mSavedGrowth.AscentCrystals[0] << " " << mSavedGrowth.AscentCrystals[1] << " " << mSavedGrowth.AscentCrystals[2]
            << "\npotions " << mSavedGrowth.Potions << "\n";
        out << "gear " << mSavedGrowth.Gear << "\n";
        for (int q = 0; q < AscentQuests::QuestCount(); ++q)   // (her quests she has taken: id, state, how far)
            if (mSavedGrowth.QuestState[q] != 0 || mSavedGrowth.QuestCount[q] != 0)
                out << "quest " << AscentQuests::QuestAt(q).Id << " " << mSavedGrowth.QuestState[q] << " " << mSavedGrowth.QuestCount[q] << "\n";
        for (const auto& id : mSavedBeaten) out << "beaten " << id << "\n";
        for (const auto& id : mSavedJars) out << "jar " << id << "\n";
    }

    void EaglesAscentGame::SaveHere(int station, bool quiet)
    {
        mSavedNest = station; mMap.Checkpoint = station;
        mSavedGrowth = mGrowth;
        if (mCreaturesOn) mSavedBeaten = mCreatures.MountainBeaten();
        if (mWayside.Laid()) mSavedJars = mWayside.BrokenIds();
        WriteSave();
        if (quiet) return;
        mSavedMark = 2.5f;
        Notify(L"SAVED", Words(mMap.Stations[static_cast<size_t>(station)].Name) + L". If she falls, she wakes here, as she is now.", 4);
        Sound::Play("world/bell", .7f, 0);
    }

    void EaglesAscentGame::RollBack()
    {
        mGrowth = mSavedGrowth;
        mFormShown = AscentGrowth::Form(mGrowth);
        if (mWayside.Laid()) mWayside.Restore(mSavedJars);
        if (mCreaturesOn) { mCreatures.SetMountainCreatures(mCreatureList, mSavedBeaten); mBeatenWritten = mCreatures.MountainBeaten().size(); }
        mMap.Checkpoint = mSavedNest;
    }

    void EaglesAscentGame::StudioIn(WorldState& world)
    {
        if (mStudio) return;
        auto& p = world.MainPlayer;
        mStudioFrom = p; mStudio = true;
        p.Position.y = StudioHeight;   // (over the summit: round her only sky, and the mountain far under her in the haze)
        p.GroundY = p.TargetGroundY = StudioHeight; p.IsGrounded = true; p.VerticalVelocity = 0;
    }

    void EaglesAscentGame::StudioKeep(WorldState& world)
    {
        if (!mStudio) return;
        const auto& p = world.MainPlayer;
        if (std::abs(p.Position.x - mStudioFrom.Position.x) < .001f && std::abs(p.Position.z - mStudioFrom.Position.z) < .001f && std::abs(p.Position.y - StudioHeight) < .001f) return;
        mStudio = false; StudioIn(world);
    }

    void EaglesAscentGame::StudioOut(WorldState& world)
    {
        if (!mStudio) return;
        world.MainPlayer = mStudioFrom; mStudio = false;
    }

    int EaglesAscentGame::NestAt(const XMFLOAT3& at) const
    {
        const XMFLOAT3& p = mStudio ? mStudioFrom.Position : at;   // (on her window's stage: where she really stands)
        for (const auto& n : mMap.Nests)
            if (std::hypot(p.x - n.P.x, p.z - n.P.z) < EaglesAscent::NestReach + 1.2f && std::abs(p.y - n.P.y) < 3) return n.Station;
        return -1;
    }

    bool EaglesAscentGame::Pressed(int key, bool allowed)
    {
        if (mTestKey == key) { mTestKey = 0; return true; }
        return Press(key, allowed);
    }

    void EaglesAscentGame::QuestsOpen()
    {
        mQuestsOpen = true; mQuestFlash = 0; mQuestFlashRow = -1; mQuestRow = 0;
        Sound::PlayUi("ui/open", .6f);
        const int set = AscentQuests::OpenSet(mGrowth);
        if (set < 0) return;
        const auto rows = AscentQuests::Rows(set);
        for (int r = 0; r < static_cast<int>(rows.size()); ++r)   // (on the first she can take)
            if (mGrowth.QuestState[rows[static_cast<size_t>(r)]] == AscentQuests::Offered) { mQuestRow = r; break; }
    }

    bool EaglesAscentGame::TakeQuest(int quest)
    {
        if (quest < 0 || quest >= AscentQuests::QuestCount()) return false;
        const auto& q = AscentQuests::QuestAt(quest);
        const int kind = AscentQuests::Kind(q.Set);
        if (mGrowth.QuestState[quest] != AscentQuests::Offered || AscentQuests::OpenSet(mGrowth) != q.Set) return false;
        if (mGrowth.AscentCrystals[kind] < q.Price)
        {
            GetNyxaraKit().Notice((L"She has too few " + std::wstring(AscentGrowth::CrystalName(kind)) + L" crystals for it.").c_str(), 2.2f);
            return false;
        }
        mGrowth.AscentCrystals[kind] -= q.Price;
        mGrowth.QuestState[quest] = AscentQuests::Taken; mGrowth.QuestCount[quest] = 0;
        mQuestFlash = 1; mQuestFlashRow = mQuestRow;
        Sound::PlayUi("ui/buy", .9f);
        return true;
    }

    void EaglesAscentGame::QuestDone(int quest)
    {   // (the last of a set: its gear's moment says it, the next frame: GearIfEarned)
        mGrowth.QuestState[quest] = AscentQuests::Done;
        mQuestLeaving.Start(quest);   // (its row on the HUD says so and goes; the panel keeps it, as done)
        const int set = AscentQuests::QuestAt(quest).Set;
        Notify(L"QUEST DONE", AscentQuests::QuestTitle(quest) + L".   " + std::to_wstring(AscentQuests::DoneOf(mGrowth, set)) + L" of " +
                              std::to_wstring(AscentQuests::QuestsOf(set)) + L" done.", 4.5f);
        Sound::Play("world/gift", .7f, 0);
    }

    void EaglesAscentGame::CountQuests(int kind, int count)
    {   // (a quest counts only once it is taken: the owner, 2026-11-22)
        for (int i = 0; i < AscentQuests::QuestCount(); ++i)
        {
            const auto& q = AscentQuests::QuestAt(i);
            if (q.Kind != kind || mGrowth.QuestState[i] != AscentQuests::Taken) continue;
            mGrowth.QuestCount[i] += count;
            if (mGrowth.QuestCount[i] >= q.Need) QuestDone(i);
        }
    }

    void EaglesAscentGame::ReachNest(int station)
    {   // the nest it asks, or one above it (she may take it when she is past it already)
        int reached = 0;
        for (size_t n = 0; n < mMap.Nests.size(); ++n) if (mMap.Nests[n].Station == station) { reached = static_cast<int>(n) + 1; break; }
        if (reached <= 0) return;
        for (int i = 0; i < AscentQuests::QuestCount(); ++i)
        {
            const auto& q = AscentQuests::QuestAt(i);
            if (q.Kind == AscentQuests::Reach && mGrowth.QuestState[i] == AscentQuests::Taken && reached >= q.Need) QuestDone(i);
        }
    }

    void EaglesAscentGame::GearIfEarned()
    {   // her set all done: she wears its gear at once (its moment: the gear's block in Update) and it is saved, with all she
        // has; if she falls, she wakes at the nest she last saved at, wearing it. The next set is offered.
        const int set = AscentQuests::OpenSet(mGrowth);
        if (!AscentQuests::SetDone(mGrowth, set)) return;
        mGrowth.Gear = set + 1;
        SaveHere(mSavedNest, true);
        Sound::Play("world/bell", .8f, 0);
    }

    void EaglesAscentGame::GiveCrystals(int kind, int count)
    {
        if (count <= 0 || kind < 0 || kind >= AscentGrowth::CrystalKinds) return;
        mGrowth.AscentCrystals[kind] += count;
        mCrystalFlash[kind] = 1;
    }

    int EaglesAscentGame::GroupStretch(int group) const
    {
        if (group < 0 || group >= static_cast<int>(mMap.Placements.size())) return 1;
        const auto& pl = mMap.Placements[static_cast<size_t>(group)];
        return pl.Cave >= 0 ? mMap.StretchHeight(pl.P.y) : mMap.StretchAlong(pl.At);
    }

    int EaglesAscentGame::HereKind(const XMFLOAT3& at) const
    {   // on the way or by it: the stretch of the way she is at; away from it (a cave, a ring): by her height, as the jars and
        // creatures there have it
        return AscentGrowth::CrystalKindOf(mMap.OffTrail < 25.f ? mMap.StretchAlong(mMap.Along) : mMap.StretchHeight(at.y));
    }

    void EaglesAscentGame::Collect(const Wayside::Gain& gain)
    {
        auto& kit = GetNyxaraKit();
        if (gain.Jars > 0)
        {
            Sound::Play("combat/blow_2", .7f, .15f);
            Sound::Play("world/chest", .45f, .2f);
            kit.Effect(gain.At, {.75f, .45f, .28f, 1}, 22 * gain.Jars, 3.f, .2f);   // the clay's dust
        }
        if (gain.Jars > 0) CountQuests(AscentQuests::Jars, gain.Jars);
        int potions = gain.Potions, crystals = gain.Crystals;
        int kinds[AscentGrowth::CrystalKinds] = {}, known = 0;
        for (const auto& [stretch, count] : gain.Stretches) { kinds[AscentGrowth::CrystalKindOf(stretch)] += count; known += count; }
        while (potions > 0 && mGrowth.Potions + potions > MaxPotions) { --potions; crystals += 1; }   // (she carries five: more is a crystal)
        kinds[HereKind(gain.At)] += crystals - known;   // (that, and any without a jar's stretch: the kind of where she is)
        mGrowth.Potions += potions;
        int most = 0;
        for (int k = 0; k < AscentGrowth::CrystalKinds; ++k) { GiveCrystals(k, kinds[k]); if (kinds[k] > kinds[most]) most = k; }
        if (crystals > 0)
        {
            const auto t = AscentGrowth::CrystalColour(most);
            Sound::Play("world/crystal", .8f, .1f);
            kit.Effect(gain.At, {t.R, t.G, t.B, 1}, 16 + 4 * crystals, 4.f, .22f);
        }
        if (potions > 0) { Sound::Play("world/gift", .7f, 0); kit.Effect(gain.At, {1.f, .35f, .4f, 1}, 24, 3.5f, .24f); }
        if (potions > 0 || crystals > 0)
        {
            std::wstring what;
            for (int k = 0; k < AscentGrowth::CrystalKinds; ++k)
                if (kinds[k] > 0)
                    what += (what.empty() ? L"" : L"   ") + std::wstring(L"+") + std::to_wstring(kinds[k]) + L" " + AscentGrowth::CrystalName(k) +
                            (kinds[k] == 1 ? L" crystal" : L" crystals");
            if (potions > 0) what += (what.empty() ? L"" : L"   ") + std::wstring(L"+") + std::to_wstring(potions) + (potions == 1 ? L" potion" : L" potions");
            kit.Notice(what.c_str(), 1.8f);
        }
        else if (gain.Jars > 0) kit.Notice(gain.Jars == 1 ? L"Empty." : L"Nothing in them.", 1.2f);
    }

    void EaglesAscentGame::NewClimb(DoorwaysGameHost& host)
    {
        mSavedNest = 0; mSavedBeaten.clear(); mReached = 0;
        mSavedGrowth = {}; mGrowth = {}; mFormShown = 0; mSavedJars.clear();
        if (mWayside.Laid()) mWayside.Restore({});
        std::error_code ignored;
        if (!GameSession().Unattended || mSaveTest) std::filesystem::remove(mSavePath, ignored);
        if (mCreaturesOn) mCreatures.SetMountainCreatures(mCreatureList, {});
        mBeatenWritten = 0;
        mMap.Checkpoint = 0;
        mMap.Visit(host, 0);
        Notify(L"A NEW CLIMB", L"From the shore. Her save is forgotten: her growth, the creatures she beat, the jars.", 4);
    }

    // An Eagle Nest (the owner, 2026-11-19: "place something like Eagle Nest that will be save spots for nyxara"): the
    // map's own model (EaglesAscent::NestModel, art/map/eagle_nest), or, when it has not been compiled, a ring of sticks
    // drawn here. Out of every nest but the one she rested in last a pale gold light rises, so she sees it from afar.
    void EaglesAscentGame::DrawNests(Renderer& renderer, const WorldState& world, const XMMATRIX& viewProjection)
    {
        if (mMap.Nests.empty()) return;
        renderer.ResetDefaultWorldRenderState(); renderer.BindDoorwaysSkinningDisabled();
        const XMFLOAT3 eye = world.MainCamera.Position;
        ID3D11ShaderResourceView* white = renderer.mWhiteTextureView.Get();
        const auto surface = [](ID3D11ShaderResourceView* texture, XMFLOAT4 tint, float emission, bool blend)
        {
            Material m{};
            m.Diffuse = tint; m.DiffuseMap = texture; m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .9f;
            m.SpecularColor = {0, 0, 0, 0}; m.EmissiveStrength = emission; m.DoubleSided = true; m.ReceiveFog = true;
            if (blend) m.AlphaMode = DoorwaysMaterialAlphaMode::Blend;
            return m;
        };
        const Material wood[3] = {surface(white, {.30f, .22f, .14f, 1}, .03f, false), surface(white, {.21f, .15f, .10f, 1}, .03f, false),
                                  surface(white, {.40f, .35f, .29f, 1}, .03f, false)};
        const Material floor = surface(white, {.16f, .12f, .08f, 1}, .02f, false);
        const Material down = surface(white, {.86f, .84f, .80f, 1}, .10f, false), tip = surface(white, {.10f, .09f, .09f, 1}, .02f, false);
        std::vector<std::pair<XMFLOAT3, float>> lights;
        for (const auto& n : mMap.Nests)
        {
            const float dx = n.P.x - eye.x, dz = n.P.z - eye.z;
            if (dx * dx + dz * dz > 450.f * 450.f) continue;
            if (n.Station != mMap.Checkpoint) lights.push_back({n.P, std::sqrt(dx * dx + dz * dz)});
            if (mMap.NestModels) continue;   // the nest is the map's model (EaglesAscent::NestModel); else these sticks
            const XMMATRIX where = XMMatrixRotationY(n.Yaw) * XMMatrixTranslation(n.P.x, n.P.y, n.P.z);
            unsigned seed = 2654435761u * static_cast<unsigned>(n.Station + 1);
            const auto random = [&seed]() { seed = seed * 1664525u + 1013904223u; return static_cast<float>(seed >> 8) / 16777216.f; };
            // the rim: three rounds of sticks laid along the ring, each tipped and turned a little
            for (int layer = 0; layer < 3; ++layer)
            {
                const float r = 1.55f - .15f * static_cast<float>(layer), y = .13f + .17f * static_cast<float>(layer);
                const int count = 30 - 4 * layer;
                for (int i = 0; i < count; ++i)
                {
                    const float a = XM_2PI * (static_cast<float>(i) + .37f * static_cast<float>(layer) + .3f * random()) / static_cast<float>(count);
                    const float length = .85f + .55f * random(), thick = .055f + .045f * random();
                    const XMMATRIX stick = XMMatrixScaling(thick, thick, length) * XMMatrixRotationX((random() - .5f) * .55f) *
                                           XMMatrixRotationY(a + XM_PIDIV2 + (random() - .5f) * .75f) *
                                           XMMatrixTranslation(std::sin(a) * r, y + (random() - .5f) * .08f, std::cos(a) * r) * where;
                    renderer.DrawBox(stick, viewProjection, wood[(i + layer) % 3]);
                }
            }
            // its floor: short sticks matted across it, on a bed of dark twigs
            for (int k = 0; k < 4; ++k)
                renderer.DrawBox(XMMatrixScaling(2.35f, .06f, 2.35f) * XMMatrixRotationY(XM_PIDIV4 * .5f * static_cast<float>(k)) * XMMatrixTranslation(0, .05f, 0) * where,
                                 viewProjection, floor);
            for (int i = 0; i < 18; ++i)
            {
                const float a = XM_2PI * random(), r = 1.15f * std::sqrt(random());
                renderer.DrawBox(XMMatrixScaling(.05f, .05f, .6f + .5f * random()) * XMMatrixRotationY(XM_2PI * random()) *
                                 XMMatrixTranslation(std::sin(a) * r, .1f, std::cos(a) * r) * where, viewProjection, wood[i % 3]);
            }
            // three white feathers with dark tips, caught in the rim and lying in it
            for (int f = 0; f < 3; ++f)
            {
                const float a = XM_2PI * (static_cast<float>(f) + random() * .6f) / 3.f, r = f == 2 ? .5f : 1.3f;
                const XMMATRIX lie = XMMatrixRotationX(f == 2 ? .08f : -.5f) * XMMatrixRotationY(a + 1.2f * random()) *
                                     XMMatrixTranslation(std::sin(a) * r, f == 2 ? .14f : .55f, std::cos(a) * r) * where;
                renderer.DrawBox(XMMatrixScaling(.11f, .012f, .5f) * lie, viewProjection, down);
                renderer.DrawBox(XMMatrixScaling(.11f, .014f, .1f) * XMMatrixTranslation(0, 0, .28f) * lie, viewProjection, tip);
            }
        }
        // the light out of the nests she has not rested in: soft gold cards, one over another, facing the camera
        auto& kit = GetNyxaraKit();
        if (!lights.empty() && kit.GlowTexture())
        {
            renderer.SetAlphaBlendingEnabled(true);
            renderer.SetDepthWriteEnabled(false);
            for (const auto& [at, distance] : lights)
            {
                const float pulse = .75f + .25f * std::sin(mClock * 1.6f + at.x), fade = std::clamp((450.f - distance) / 120.f, 0.f, 1.f) * std::clamp((distance - 2.f) / 6.f, 0.f, 1.f);
                for (int k = 0; k < 4; ++k)
                {
                    const XMFLOAT3 c{at.x, at.y + .6f + 1.1f * static_cast<float>(k), at.z};
                    XMFLOAT3 to{eye.x - c.x, 0, eye.z - c.z};
                    const float len = std::max(1e-3f, std::sqrt(to.x * to.x + to.z * to.z)); to.x /= len; to.z /= len;
                    const float size = 1.6f - .3f * static_cast<float>(k);
                    XMMATRIX m = XMMatrixIdentity();
                    m.r[0] = XMVectorSet(to.z * size, 0, -to.x * size, 0); m.r[1] = XMVectorSet(0, size * 1.6f, 0, 0);
                    m.r[2] = XMVectorSet(to.x, 0, to.z, 0); m.r[3] = XMVectorSet(c.x, c.y, c.z, 1);
                    const Material glow = surface(kit.GlowTexture(), {1.f, .82f, .42f, (.34f - .06f * static_cast<float>(k)) * pulse * fade}, 1, true);
                    renderer.BindDoorwaysMaterialTexture(glow);
                    for (const auto& submesh : kit.QuadMesh().Submeshes)
                        renderer.DrawDoorwaysGpuSubmesh(kit.QuadMesh(), submesh, m, viewProjection, glow, false, true);
                }
            }
        }
        renderer.ResetDefaultWorldRenderState(); renderer.BindDoorwaysSkinningDisabled();
    }
}
