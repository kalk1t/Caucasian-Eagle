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
#include "GameConstants.h"
#include "GameUi.h"
#include "NyxaraHud.h"
#include "NyxaraKit.h"
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
        PrepareNyxaraControls(host);   // the road's controls, camera and controller buttons
        auto& session = GameSession();
        session.FreeFlightCeiling = 0;                         // no ceiling: the mountain is 4 km high
        session.FastMultiplier = FootRunSpeed / FootSpeed;     // she starts on foot
        host.World.MainPlayer.MoveSpeed = FootSpeed;
        host.World.MainPlayer.JumpSpeed = EaglesAscent::JumpSpeed;
        session.WalkOffLedges = true;                          // she can go over any edge (and jump from its brink)
        GetNyxaraKit().SetRealJump(EaglesAscent::JumpSpeed);   // Space on foot is a real jump here
        auto& camera = host.World.MainCamera;
        camera.FarZ = EaglesAscent::FarPlane; camera.NearZ = .25f;   // the summit must be in view from the shore
        if (!Ui::LoadArt(host.Render, error)) return false;
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
        mLog = Env(AscentLogEnvironmentVariable);
        mTrace = !mLog.empty();
        if (mLog.empty()) mLog = ResolveDoorwaysRuntimePath("eagles-ascent-evidence.tsv").string();
        LoadSave();
        if (mTour)
        {   // the tour begins a new climb, its save beside its evidence (never a player's)
            mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear(); mSavedPlaces.clear();
            if (!mSaveTest) { mSavePath = std::filesystem::path(mLog).parent_path() / "tour.sav"; mSaveTest = true; }
            std::error_code ignored; std::filesystem::remove(mSavePath, ignored);
            mTourShots = Env("CAUCASIAN_EAGLE_TOUR_SHOTS");
        }
        if (mWays)
        {   // (its own save beside its evidence)
            mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear(); mSavedPlaces.clear();
            if (!mSaveTest) { mSavePath = std::filesystem::path(mLog).parent_path() / "ways.sav"; mSaveTest = true; }
            std::error_code ignored; std::filesystem::remove(mSavePath, ignored);
            mTourShots = Env("CAUCASIAN_EAGLE_TOUR_SHOTS");
            GetNyxaraKit().SetAutoKeys(&mWaysKeys);
        }
        mGrowth = mSavedGrowth; mFormShown = Growth::Form(mGrowth);
        mWaysideOn = !mSmoke && !mClimb && !mTour;   // (the map's and the creatures' tests: no jars)
        mCreaturesOn = !mMap.Placements.empty() && Env(AscentCreaturesEnvironmentVariable) != "0" && !mSmoke && !mClimb;
        if (mCreaturesOn)
        {
            if (!mCreatures.InitializeMountain(host.Render, error)) { error = "The Eagle's Ascent's creatures: " + error; return false; }
            mCreatures.SetMountainTracing(mTrace);
            StartCreatures();
            // the Eagle at Amiran's court is the last boss now: his statue there is not drawn (beaten, he is not there either)
            for (const auto& p : mMap.Placements)
                if (p.Boss && !p.Kinds.empty() && p.Kinds[0] == "eagle") mMap.HideModels("Landmarks/AmiraniMountain/Eagle", p.P, 30.f, true);
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
        if (!mSmoke && !mClimb && !mTour && !mWays && !mNature) GetNyxaraKit().SetAttackCombo(0);
        if (mNature) mTourShots = Env("CAUCASIAN_EAGLE_TOUR_SHOTS");
        if (mSmoke || mClimb || mTour || mWays || mNature || mGround)
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
        return mPaused || mView.Open || mGrowthOpen ? 0 : GetNyxaraKit().ScaleFrameTime(realDeltaTime, inputAllowed);
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
        const auto finish = [&](bool ok) { Evidence(host.World, ok ? "CLIMB_PASS" : "CLIMB_FAIL"); mClimb = false; PostQuitMessage(ok ? 0 : 2); };
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
                std::to_string(potions) + "\tplaces=" + std::to_string(mWayside.Places().size()));
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
                const XMFLOAT3 at{jar.x + ax * 1.5f, jar.y, jar.z + az * 1.5f};
                Put(host, at, std::atan2(-ax, -az));
                world.MainCamera.Yaw = std::atan2(-ax, -az);
            }
            if (mWaysClock > 1.4f && mWaysClock - dt <= 1.4f) mTourShot = L"jars_" + std::to_wstring(mWaysPick) + L"_whole";
            for (const float at : {1.6f, 2.3f, 3.0f}) if (mWaysClock > at && mWaysClock < at + .1f) mWaysKeys.Lmb = true;
            if (mWaysClock > 4.2f && mWaysClock - dt <= 4.2f) mTourShot = L"jars_" + std::to_wstring(mWaysPick) + L"_broken";
            if (mWaysClock > 4.4f)
            {
                const int broken = brokenIn(j);
                const auto& her = world.MainPlayer.Position;
                std::ostringstream line; line << std::fixed << std::setprecision(2) << "cut\t" << group(j) << "\tbroken=" << broken << "\tcrystals=" << mGrowth.Crystals
                                              << "\tpotions=" << mGrowth.Potions << "\ther_dy=" << her.y - jar.y << "\tjar=" << jar.x << "," << jar.y << "," << jar.z;
                if (broken == 0) { ++mWaysBad; line << "\tNONE_BROKEN"; }
                row(line.str());
                ++mWaysPick; mWaysClock = 0;
            }
            return;
        }
        if (mWaysPhase == 2)
        {   // saved at the first nest; then more jars broken and a place found; then she falls (RollBack): all as saved
            const int nest = mMap.Nests.empty() ? 0 : mMap.Nests.front().Station;
            SaveHere(nest);
            const Growth::State saved = mGrowth;
            const size_t brokenSaved = mWayside.BrokenIds().size();
            int more = 0;
            for (const auto& jar : jars)
                if (!jar.Broken && more < 12) { Collect(mWayside.Strike(jar.P, 0, .2f, 359.f)); ++more; }
            if (!mWayside.Places().empty()) Collect(mWayside.Visit(mWayside.Places().front().P));
            const int gained = mGrowth.Crystals - saved.Crystals; const size_t brokenThen = mWayside.BrokenIds().size(), foundThen = mWayside.FoundIds().size();
            RollBack();
            const bool back = mGrowth.Crystals == saved.Crystals && mGrowth.Potions == saved.Potions && mWayside.BrokenIds().size() == brokenSaved && mWayside.FoundIds().empty();
            row("rollback\tsaved_crystals=" + std::to_string(saved.Crystals) + "\tgained_after=" + std::to_string(gained) + "\tbroken_saved=" + std::to_string(brokenSaved) +
                "\tbroken_then=" + std::to_string(brokenThen) + "\tfound_then=" + std::to_string(foundThen) + "\tcrystals_now=" + std::to_string(mGrowth.Crystals) +
                "\tbroken_now=" + std::to_string(mWayside.BrokenIds().size()) + (back && gained > 0 && brokenThen > brokenSaved ? "" : "\tROLLBACK_FAIL"));
            if (!(back && gained > 0 && brokenThen > brokenSaved)) ++mWaysBad;
            LoadSave();
            const bool read = mSavedNest == nest && mSavedGrowth.Crystals == saved.Crystals && mSavedJars.size() == brokenSaved;
            row("reload\tnest=" + std::to_string(mSavedNest) + "\tcrystals=" + std::to_string(mSavedGrowth.Crystals) + "\tjars=" + std::to_string(mSavedJars.size()) + (read ? "" : "\tRELOAD_FAIL"));
            if (!read) ++mWaysBad;
            row(mWaysBad == 0 ? "WAYS_PASS" : "WAYS_FAIL\tbad=" + std::to_string(mWaysBad));
            Evidence(world, mWaysBad == 0 ? "WAYS_PASS" : "WAYS_FAIL");
            mWays = false; PostQuitMessage(mWaysBad == 0 ? 0 : 2);
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
                XMFLOAT3 at{};
                const float back = pl.Boss ? 24.f : pl.Cave >= 0 ? 9.f : 14.f;
                if (pl.Cave < 0)
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
        mClock += realDt; mNotice = std::max(0.f, mNotice - realDt);
        auto& session = GameSession(); auto& world = host.World; auto& p = world.MainPlayer; auto& kit = GetNyxaraKit();
        const bool flying = p.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
        const bool alive = host.Combat.PlayerActionsAllowed();
        mMap.KeepOutOfRock(p, flying, dt, world.RuntimeScene);

        // Esc: the pause panel (DrawHud); its keys
        // M: the whole mountain from outside, to turn and look over (MountainView.h). The game stands still; M or Esc closes it.
        if (MountainView::Tried() && !mView.Open && mClock > 1 && mClock < 1.5f) mView.Toggle(session, world.MainCamera, mMap.Trail, mMap.Summit);
        if (!mPaused && !mSmoke && !mClimb && !mTour && Press('M', inputAllowed)) mView.Toggle(session, world.MainCamera, mMap.Trail, mMap.Summit);
        if (mView.Open)
        {
            if (session.EscapePressed) { session.EscapePressed = false; mView.Hide(session, world.MainCamera); }
            else
            {
                mView.Update(world.MainCamera, p.Position, realDt, inputAllowed);
                kit.SetInputBlocked(true);
                kit.Update(world, host.WorldRender, host.Combat, 0, realDt, false);
                return;
            }
        }
        // the jars and places along the roads (once the map's ground is up), as the save has them
        if (mWaysideOn && !mWayside.Laid() && world.RuntimeScene)
        {
            mWayside.Lay(*world.RuntimeScene, mMap);
            mWayside.Restore(mSavedJars, mSavedPlaces);
            char line[160]; std::snprintf(line, sizeof line, "[EAGLES ASCENT] %zu clay jars and %zu hidden places laid.\n", mWayside.Jars().size(), mWayside.Places().size());
            OutputDebugStringA(line);
            if (mTrace) Evidence(world, ("wayside_jars_" + std::to_string(mWayside.Jars().size()) + "_places_" + std::to_string(mWayside.Places().size())).c_str());
        }
        // I: her growth (as on the road): W / S choose, Enter or Space spends crystals; I or Esc closes. The climb waits.
        mGrowthFade = std::clamp(mGrowthFade + (mGrowthOpen ? 1.f : -1.f) * realDt / .18f, 0.f, 1.f);
        mGrowthFlash = std::max(0.f, mGrowthFlash - realDt * 2.5f);
        mSavedMark = std::max(0.f, mSavedMark - realDt);
        if (!mPaused && !mSmoke && !mClimb && !mTour && alive && Press('I', inputAllowed))
        {
            mGrowthOpen = !mGrowthOpen; Sound::PlayUi(mGrowthOpen ? "ui/open" : "ui/close", .6f);
        }
        if (mGrowthOpen)
        {
            if (session.EscapePressed || !alive) { session.EscapePressed = false; mGrowthOpen = false; Sound::PlayUi("ui/close", .6f); }
            const int move = (Press('S', inputAllowed) || Press(VK_DOWN, inputAllowed) ? 1 : 0) - (Press('W', inputAllowed) || Press(VK_UP, inputAllowed) ? 1 : 0);
            mGrowthRow = (mGrowthRow + move + Growth::RowCount) % Growth::RowCount;
            if (move != 0) Sound::PlayUi("ui/move", .5f);
            if (Press(VK_RETURN, inputAllowed) || Press(VK_SPACE, inputAllowed))
            {
                const int cost = Growth::Cost(mGrowth, mGrowthRow);
                if (cost > 0 && mGrowth.Crystals >= cost)
                {
                    mGrowth.Crystals -= cost; ++mGrowth.Level[mGrowthRow];
                    mGrowthFlash = 1; mGrowthFlashRow = mGrowthRow; Sound::PlayUi("ui/buy", .9f);
                }
                else Sound::PlayUi("ui/deny", .7f);
            }
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
            else { mPaused = !mPaused; mMenuRow = 0; Sound::PlayUi(mPaused ? "ui/open" : "ui/close", .7f); }
        }
        session.FreeCursor = mPaused;   // the pointer shows and the mouse leaves the camera alone
        if (!mPaused && settings) Settings::Close();
        if (mPaused)
        {
            if (!settings)
            {
                bool take = Press(VK_RETURN, inputAllowed) || Press(VK_SPACE, inputAllowed);
                {   // the pointer: the row it moves over is chosen, a click takes it
                    float mx = -1, my = -1; bool button = false;
                    if (inputAllowed)
                    {
                        POINT point{};
                        if (GetCursorPos(&point) && ScreenToClient(GetForegroundWindow(), &point)) { mx = static_cast<float>(point.x); my = static_cast<float>(point.y); }
                        button = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                    }
                    const bool click = button && !mMouseWas, moved = std::abs(mx - mMouseX) + std::abs(my - mMouseY) > .5f;
                    mMouseWas = button; mMouseX = mx; mMouseY = my;
                    if (mx >= 0 && (moved || click))
                    {
                        const int hit = Pointer::Hit(Pointer::PauseRow, mx, my);
                        if (hit >= 0)
                        {
                            if (hit != mMenuRow) { mMenuRow = hit; Sound::PlayUi("ui/move", .5f); }
                            if (click) take = true;
                        }
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
                    case 2: mPaused = false; mView.Toggle(session, world.MainCamera, mMap.Trail, mMap.Summit); break;
                    case 3: Settings::Open(); break;   // her menu stays open under it
                    case 4: mPaused = false; mMap.Visit(host, mMap.Checkpoint); Notify(BackTitle(), Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name), 3); break;
                    case 5: if (!mMap.Nests.empty()) { mPaused = false; NewClimb(host); } break;
                    case 6: mPaused = false; session.MenuRequest = Request::MainMenu; break;
                    default: session.MenuRequest = Request::Exit; break;
                    }
                    if (!mPaused) session.FreeCursor = false;
                }
            }
            if (mNature) Nature(host, realDt);   // (the nature test pictures her menu: it goes on while she is paused)
            kit.SetInputBlocked(true);
            kit.Update(world, host.WorldRender, host.Combat, 0, realDt, false);
            return;
        }
        const bool allowed = inputAllowed && alive && !mSmoke && !mClimb && !mTour;

        // Space held 1.5 s takes her up (NyxaraKit: her flying energy lasts 30 s, then she falls). R in flight lands her
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
        kit.SetInputBlocked(mSmoke || mClimb || mTour);
        {   // her growth (0.5): her blades, the blows she takes, how long she flies; her form, taller
            kit.SetDamageScale(Growth::Damage(mGrowth));
            kit.SetFlightSeconds(Growth::FlightSeconds(mGrowth.Level[Growth::Wings]));
            mCreatures.SetMountainGrowth(mGrowth);
            const int form = Growth::Form(mGrowth);
            const float tall = 1 + FormScale * static_cast<float>(form);
            world.AnimatedPlayerVisual.Scale = {tall, tall, tall};
            if (form > mFormShown && alive)
            {   // all three of her things reached a new tier: she herself changes
                const XMFLOAT4 light = form == 1 ? XMFLOAT4{.55f, .82f, 1.f, 1} : form == 2 ? XMFLOAT4{.35f, 1.f, .62f, 1} : XMFLOAT4{1.f, .86f, .45f, 1};
                kit.Effect({p.Position.x, p.Position.y + 1.3f, p.Position.z}, light, 120, 9.f, .34f);
                kit.RingEffect({p.Position.x, p.Position.y + .05f, p.Position.z}, 7.f, light, 1.6f);
                Notify(L"NYXARA EVOLVES", std::wstring(Growth::FormName(form)) + L". Wings, swords and armour answer her as one.", 6);
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
        if (mSmoke) Smoke(host, realDt);
        if (mClimb) Climb(host, dt);
        if (mTour) Tour(host, dt);
        if (mWays) Ways(host, realDt);
        if (mNature) Nature(host, realDt);
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
                    if (c.Id == id) { mGrowth.Crystals += Journey::KindCrystals(c.Kind); break; }
                Sound::Play("world/crystal", .6f, .1f);
                if (mTrace) Evidence(world, ("beaten_" + id).c_str());
            }
        }

        // the map: where she is, the air, the checkpoints, the safety nets
        mMap.Locate(p.Position);
        const int before = mMap.Checkpoint;
        if (const char* net = mMap.Air(host, dt, flying, !mSmoke && !mTour && mClock > 5 && alive))
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

        if (mWaysideOn && mWayside.Laid())
        {
            mWayside.Update(dt);
            if (alive) Collect(mWayside.Visit(p.Position));
        }

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
        mTrees.Draw(renderer, world.MainCamera.Position, viewProjection);
        mGrass.Draw(renderer, world.MainCamera.Position, viewProjection);
        DrawNests(renderer, world, viewProjection);
        if (mCreaturesOn) mCreatures.DrawMountain(renderer, world, viewProjection);
        {   // her growth's lights: on her wings, at her blades and at her feet, in the colour of each tier (as on the road)
            const XMFLOAT4 tierLight[4] = {{0, 0, 0, 0}, {.50f, .78f, 1.f, 1}, {.30f, 1.f, .60f, 1}, {1.f, .84f, .42f, 1}};
            const auto& her = world.MainPlayer;
            const float tall = 1 + FormScale * static_cast<float>(Growth::Form(mGrowth));
            const XMFLOAT3 forward{std::sin(her.Yaw), 0, std::cos(her.Yaw)}, side{std::cos(her.Yaw), 0, -std::sin(her.Yaw)};
            const auto light = [&](int tier, XMFLOAT3 local, float size, float alpha)
            {
                XMFLOAT4 color = tierLight[tier]; color.w = alpha * (.8f + .2f * std::sin(mClock * 2.6f + local.x * 3));
                mWayside.Glow({her.Position.x + side.x * local.x + forward.x * local.z, her.Position.y + local.y * tall, her.Position.z + side.z * local.x + forward.z * local.z},
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
        const float s = std::clamp(ui.S, .8f, 1.5f);
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
        if (NyxaraHud().Valid && context.World.CombatFeedback.MeleeActive) DrawNyxaraHud(ui, context);
        else if (NyxaraHud().Valid)
        {   // her flying energy (NyxaraKit: Space held 1.5 s takes her up; 30 s of flight), at the bottom in the middle
            const auto& wings = NyxaraHud();
            const float bw = 260 * s, bx = (ui.W - bw) * .5f, by = ui.H - 46 * s;
            ui.Rect(bx - 6 * s, by - 20 * s, bw + 12 * s, 36 * s, {.015f, .025f, .04f, .7f});
            ui.Text(wings.Flying ? L"WINGS  (R lands)" : wings.FlightCharge > 0 ? L"WINGS  taking off..." : L"WINGS  (hold Space 1.5 s to fly)", bx, by - 17 * s, 11 * s / DoorwaysUiFontHeight, Palette::Ink);
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

        // the way: where the road needs wings, in the world
        const auto view = context.World.MainCamera.BuildViewProjectionMatrix(ui.W / ui.H);
        const auto mark = [&](const XMFLOAT3& point, float up, const std::wstring& words, Color color)
        {
            XMFLOAT4 c; XMStoreFloat4(&c, XMVector4Transform(XMVectorSet(point.x, point.y + up, point.z, 1), view));
            if (c.w <= 0 || c.w > 300) return;
            const float nx = c.x / c.w, ny = c.y / c.w; if (std::abs(nx) > 1 || std::abs(ny) > 1) return;
            ui.Centered(words, (nx + 1) * ui.W * .5f, (1 - ny) * ui.H * .5f, tiny, color);
        };
        for (const auto& f : mMap.Flights)
        {
            if (f.Kind == "wall") mark(f.From, 4, L"THE ROAD GOES ON ABOVE  (hold Space: fly up)", Palette::Moon);
            else if (f.Kind == "gap") mark(f.From, 4, L"THE ROAD IS BROKEN  (hold Space: fly across)", Palette::Moon);
            else if (f.Kind == "slot") mark(f.From, 4, L"A CLEFT IN THE ROCK  (hold Space: fly through)", Palette::Moon);
            else if (f.Kind == "updraft") mark(f.From, 4, L"RISING AIR  (hold Space, then let it carry you)", Palette::Soul);
        }
        for (const auto& n : mMap.Nests)   // the Eagle Nests, the only places she is saved (H)
            mark(n.P, 3.2f, n.Station == mSavedNest ? L"EAGLE NEST  (saved here)" : L"EAGLE NEST  (H here saves)", Palette::Gold);
        if (mWaysideOn)
            for (const auto& pl : mWayside.Places())
                if (!pl.Found) mark(pl.P, 2.6f, L"A HIDDEN PLACE", Palette::GoldLight);

        {   // what she carries (0.5): crystals, potions (Z), and her window (I), under where she is
            const float py = 76 * s, kh = 20 * s, icon = 19 * s;
            ui.Rect(14 * s, py, 330 * s, 34 * s, {.015f, .025f, .04f, .7f});
            ui.Icon(HudArt::IconCrystal, 34 * s, py + 17 * s, icon, Palette::Violet);
            ui.Text(std::to_wstring(mGrowth.Crystals), 48 * s, py + 9 * s, body, Palette::Ink);
            const wchar_t* z = Ui::KeyLabel(L"Z", PotionKey);
            const float zw = ui.KeyWidth(z, kh), zx = 112 * s + zw * .5f;
            ui.Key(z, zx, py + 7 * s, kh, mGrowth.Potions > 0 ? Palette::White : Palette::Faint);
            for (int i = 0; i < MaxPotions; ++i)
                ui.Icon(HudArt::IconPotion, zx + zw * .5f + 6 * s + (static_cast<float>(i) + .5f) * 17 * s, py + 17 * s, 17 * s, i < mGrowth.Potions ? Palette::Red : Palette::Faint.Fade(.6f));
            const float ix = 290 * s;
            ui.Key(Ui::KeyLabel(L"I", 'I'), ix, py + 7 * s, kh, Palette::White);
            ui.Text(L"GROW", ix + 14 * s, py + 10 * s, tiny, Palette::Dim);
        }
        if (NestAt(p.Position) > 0 && !mGrowthOpen)
        {   // in a nest: H saves
            const float y = ui.H * .70f, kh = 26 * s;
            const std::wstring words = NestAt(p.Position) == mSavedNest ? L"SAVE AGAIN AT THIS EAGLE NEST" : L"SAVE AT THIS EAGLE NEST";
            const float tw = ui.Width(words, body), kw = ui.KeyWidth(L"H", kh), w = kw + 10 * s + tw;
            ui.Rect(ui.W * .5f - w * .5f - 12 * s, y - 6 * s, w + 24 * s, kh + 12 * s, {.015f, .025f, .04f, .75f});
            ui.Key(L"H", ui.W * .5f - w * .5f + kw * .5f, y, kh, Palette::GoldLight);
            ui.Text(words, ui.W * .5f - w * .5f + kw + 10 * s, y + (kh - ui.Height(body)) * .5f, body, Palette::Ink);
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
        if (mGrowthFade > 0)
        {   // her window (I), as on the road
            Growth::View v;
            v.Now = mGrowth; v.Selected = mGrowthRow; v.Fade = mGrowthFade; v.Flash = mGrowthFlash; v.FlashRow = mGrowthFlashRow;
            v.PotionHeal = PotionHeal; v.Ascent = true;
            for (const auto& pl : mWayside.Places()) v.Places += pl.Found ? 1 : 0;
            Growth::Draw(ui, v);
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
            const XMFLOAT3 ahead{std::sin(p.Yaw), 0, std::cos(p.Yaw)}, right{std::cos(p.Yaw), 0, -std::sin(p.Yaw)};
            for (size_t k = 0; k < p.Kinds.size(); ++k)
            {
                Journey::MountainCreature c;
                c.Kind = Journey::KindByName(p.Kinds[k]);
                if (c.Kind < 0) continue;
                c.Id = p.Boss ? p.Id : p.Id + "." + std::to_string(k);
                c.Group = group; c.Line = line; c.Yaw = p.Yaw; c.At = p.At; c.Lo = p.Lo; c.Hi = p.Hi; c.Roam = p.Roam;
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

    // eagles-ascent.sav: "nest <station>", then "beaten <id>" for every creature she has beaten
    void EaglesAscentGame::LoadSave()
    {
        if (mSavePath.empty())
        {   // (the tour's own save, once set, stays: Tour reads it back)
            const std::string named = Env(AscentSaveEnvironmentVariable);
            mSaveTest = !named.empty();
            mSavePath = mSaveTest ? std::filesystem::path(named) : ResolveDoorwaysSettingsPaths().Player.parent_path() / AscentSaveFileName;
        }
        mSavedNest = 0; mSavedBeaten.clear(); mSavedGrowth = {}; mSavedJars.clear(); mSavedPlaces.clear();
        if (mMap.Nests.empty()) return;
        std::ifstream in(mSavePath);
        std::string word;
        while (in >> word)
        {
            if (word == "nest") in >> mSavedNest;
            else if (word == "beaten") { std::string id; if (in >> id) mSavedBeaten.push_back(id); }
            else if (word == "crystals") in >> mSavedGrowth.Crystals;
            else if (word == "potions") in >> mSavedGrowth.Potions;
            else if (word == "level") { int row = -1, level = 0; if (in >> row >> level && row >= 0 && row < Growth::RowCount) mSavedGrowth.Level[row] = std::clamp(level, 0, Growth::Max(row)); }
            else if (word == "jar") { std::string id; if (in >> id) mSavedJars.push_back(id); }
            else if (word == "place") { std::string id; if (in >> id) mSavedPlaces.push_back(id); }
            else std::getline(in, word);
        }
        mSavedGrowth.Crystals = std::max(0, mSavedGrowth.Crystals); mSavedGrowth.Potions = std::clamp(mSavedGrowth.Potions, 0, MaxPotions);
        if (mSavedNest < 0 || mSavedNest >= static_cast<int>(mMap.Stations.size()) || !mMap.Stations[static_cast<size_t>(mSavedNest)].Checkpoint) mSavedNest = 0;
    }

    void EaglesAscentGame::WriteSave() const
    {
        if (GameSession().Unattended && !mSaveTest) return;   // (an unattended run never touches a player's save)
        std::error_code ignored;
        std::filesystem::create_directories(mSavePath.parent_path(), ignored);
        std::ofstream out(mSavePath, std::ios::trunc);
        out << "# The Eagle's Ascent (0.5): the Eagle Nest she saved at (H), her growth, the creatures she has beaten, the clay\n";
        out << "# jars she has broken and the hidden places she has found, as they were when she saved\n";
        out << "nest " << mSavedNest << "\n";
        out << "crystals " << mSavedGrowth.Crystals << "\npotions " << mSavedGrowth.Potions << "\n";
        for (int row = 0; row < Growth::RowCount; ++row) out << "level " << row << " " << mSavedGrowth.Level[row] << "\n";
        for (const auto& id : mSavedBeaten) out << "beaten " << id << "\n";
        for (const auto& id : mSavedJars) out << "jar " << id << "\n";
        for (const auto& id : mSavedPlaces) out << "place " << id << "\n";
    }

    void EaglesAscentGame::SaveHere(int station)
    {
        mSavedNest = station; mMap.Checkpoint = station;
        mSavedGrowth = mGrowth;
        if (mCreaturesOn) mSavedBeaten = mCreatures.MountainBeaten();
        if (mWayside.Laid()) { mSavedJars = mWayside.BrokenIds(); mSavedPlaces = mWayside.FoundIds(); }
        WriteSave();
        mSavedMark = 2.5f;
        Notify(L"SAVED", Words(mMap.Stations[static_cast<size_t>(station)].Name) + L". If she falls, she wakes here, as she is now.", 4);
        Sound::Play("world/bell", .7f, 0);
    }

    void EaglesAscentGame::RollBack()
    {
        mGrowth = mSavedGrowth;
        mFormShown = Growth::Form(mGrowth);
        if (mWayside.Laid()) mWayside.Restore(mSavedJars, mSavedPlaces);
        if (mCreaturesOn) { mCreatures.SetMountainCreatures(mCreatureList, mSavedBeaten); mBeatenWritten = mCreatures.MountainBeaten().size(); }
        mMap.Checkpoint = mSavedNest;
    }

    int EaglesAscentGame::NestAt(const XMFLOAT3& p) const
    {
        for (const auto& n : mMap.Nests)
            if (std::hypot(p.x - n.P.x, p.z - n.P.z) < EaglesAscent::NestReach + 1.2f && std::abs(p.y - n.P.y) < 3) return n.Station;
        return -1;
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
        int potions = gain.Potions, crystals = gain.Crystals;
        while (potions > 0 && mGrowth.Potions + potions > MaxPotions) { --potions; crystals += 1; }   // (she carries five: more is a crystal)
        mGrowth.Potions += potions;
        mGrowth.Crystals += crystals;
        if (crystals > 0)
        {
            Sound::Play("world/crystal", .8f, .1f);
            kit.Effect(gain.At, {.66f, .36f, 1.f, 1}, 16 + 4 * crystals, 4.f, .22f);
        }
        if (potions > 0) { Sound::Play("world/gift", .7f, 0); kit.Effect(gain.At, {1.f, .35f, .4f, 1}, 24, 3.5f, .24f); }
        if (!gain.Place.empty())
        {
            Notify(L"HIDDEN PLACE FOUND", gain.Place + L". +" + std::to_wstring(gain.Crystals) + L" crystals.", 5);
            Sound::Play("world/bell", .6f, 0);
        }
        else if (potions > 0 || crystals > 0)
        {
            std::wstring what;
            if (crystals > 0) what = L"+" + std::to_wstring(crystals) + (crystals == 1 ? L" crystal" : L" crystals");
            if (potions > 0) what += (what.empty() ? L"" : L"   ") + std::wstring(L"+") + std::to_wstring(potions) + (potions == 1 ? L" potion" : L" potions");
            kit.Notice(what.c_str(), 1.8f);
        }
        else if (gain.Jars > 0) kit.Notice(gain.Jars == 1 ? L"Empty." : L"Nothing in them.", 1.2f);
    }

    void EaglesAscentGame::NewClimb(DoorwaysGameHost& host)
    {
        mSavedNest = 0; mSavedBeaten.clear(); mReached = 0;
        mSavedGrowth = {}; mGrowth = {}; mFormShown = 0; mSavedJars.clear(); mSavedPlaces.clear();
        if (mWayside.Laid()) mWayside.Restore({}, {});
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
