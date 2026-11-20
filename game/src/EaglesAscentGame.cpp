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
#include "Sound.h"
#include "StartScreen.h"
#include "Renderer.h"
#include "WorldRenderer.h"
#include "WorldState.h"

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
    {
        if (Env(AscentEnvironmentVariable) == "1") return true;
        std::wstring line = GetCommandLineW();
        std::transform(line.begin(), line.end(), line.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return line.find(AscentSwitch) != std::wstring::npos;
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
        GetNyxaraKit().SetOnFoot(true);
        // her feet on the stair treads as drawn (she walks the ramp under them); this game lives as long as the program
        GetNyxaraKit().SetFootGround([this](float x, float z, float nearY) { return mMap.TreadTop(x, z, nearY); });
        GetNyxaraKit().SetStairTread([this](float x, float z, float top, float& cx, float& cz, float& ax, float& az) { return mMap.TreadCenter(x, z, top, cx, cz, ax, az); });

        // its Eagle Nests, bosses and creatures (EaglesAscent::LoadCreatures), and her save; the map's own tests (the
        // smoke test, the whole climb) run without the creatures
        mSmoke = Env(DemoEnvironmentVariable) == "1";   // the unattended test of the mode
        mClimb = Env(DemoEnvironmentVariable) == "2";   // the whole climb, unattended
        mTour = Env(DemoEnvironmentVariable) == "3";    // the creatures' tour, unattended (Tour)
        mLog = Env(AscentLogEnvironmentVariable);
        mTrace = !mLog.empty();
        if (mLog.empty()) mLog = ResolveDoorwaysRuntimePath("eagles-ascent-evidence.tsv").string();
        LoadSave();
        if (mTour)
        {   // the tour begins a new climb, its save beside its evidence (never a player's)
            mSavedNest = 0; mSavedBeaten.clear();
            if (!mSaveTest) { mSavePath = std::filesystem::path(mLog).parent_path() / "tour.sav"; mSaveTest = true; }
            std::error_code ignored; std::filesystem::remove(mSavePath, ignored);
            mTourShots = Env("CAUCASIAN_EAGLE_TOUR_SHOTS");
        }
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

        if (mSmoke || mClimb || mTour)
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
        return mPaused || mView.Open ? 0 : GetNyxaraKit().ScaleFrameTime(realDeltaTime, inputAllowed);
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
        mTourClock += dt;

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

    void EaglesAscentGame::Update(DoorwaysGameHost& host, float dt, float realDt, bool inputAllowed)
    {
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
        if (session.EscapePressed) { session.EscapePressed = false; mPaused = !mPaused; }
        if (mPaused)
        {
            if (Press(VK_BACK, inputAllowed)) { mPaused = false; mMap.Visit(host, mMap.Checkpoint); Notify(BackTitle(), Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name), 3); }
            if (Press(VK_RETURN, inputAllowed)) { mPaused = false; session.MenuRequest = DoorwaysGameSession::Request::MainMenu; }
            if (Press('N', inputAllowed) && !mMap.Nests.empty()) { mPaused = false; NewClimb(host); }
            if (Press('Q', inputAllowed)) session.MenuRequest = DoorwaysGameSession::Request::Exit;
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
        if (flying && !mFlying) Notify(L"IN FLIGHT", L"W flies where you look. Space climbs, Ctrl+Space sinks, R lands. Her wings last 30 seconds.", 4);
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
        if (mSmoke) Smoke(host, realDt);
        if (mClimb) Climb(host, dt);
        if (mTour) Tour(host, dt);
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
            if (mCreatures.MountainBeaten().size() != mBeatenWritten)
            {   // a creature she beat stays beaten: in the save at once (where she wakes is still the last nest)
                mBeatenWritten = mCreatures.MountainBeaten().size();
                WriteSave();
                if (mTrace) Evidence(world, ("beaten_" + mCreatures.MountainBeaten().back()).c_str());
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
        {   // an Eagle Nest: she rests in it, her wounds close, and she is saved here (the shore saves nothing)
            mReached = std::max(mReached, mMap.Checkpoint);
            const bool nest = !mMap.Nests.empty() && mMap.Checkpoint > 0;
            const std::wstring where = Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name);
            if (nest)
            {
                host.Combat.RestorePlayerHealth(world, 10000);
                WriteSave();
                Notify(L"EAGLE NEST", where + L". She rests here: healed, and saved.", 4.5f);
                Sound::Play("world/bell", .7f, 0);
                if (mTrace) Evidence(world, "nest_rested");
            }
            else Notify(L"CHECKPOINT", where, 4);
            Sound::PlayUi("ui/open", .6f);
        }

        // she fell (to the creatures, or the engine's own harm): she lies a moment and wakes at the last Eagle Nest she
        // rested at (the creatures she has beaten stay beaten: Journey::MountainBeaten)
        if (!alive)
        {
            if (!mWasDead)
            {
                mDeadFor = 0; mRespawnAsk = 0; Sound::Play("nyxara/death", 1, 0);
                Notify(L"NYXARA HAS FALLEN", mMap.Nests.empty() ? L"She wakes at the last checkpoint." : L"She wakes at the last Eagle Nest she rested in.", DeathSeconds + 1.5f);
            }
            mDeadFor += realDt;
            if (mDeadFor >= DeathSeconds && (mRespawnAsk -= realDt) <= 0)
            {
                mRespawnAsk = .5f;
                const auto& s = mMap.Stations[static_cast<size_t>(mMap.Checkpoint)];
                const XMFLOAT3& rest = mMap.Rest(mMap.Checkpoint);   // she wakes in the nest
                session.SpawnPosition = {rest.x, rest.y + .05f, rest.z}; session.SpawnYaw = s.Yaw;
                const auto& life = host.Combat.PlayerLife();
                host.Combat.RequestRespawn({life.Life, life.Death.Event, life.Death.Destination});
            }
        }
        else if (mWasDead) mMap.Visit(host, mMap.Checkpoint);
        mWasDead = !alive;
    }

    void EaglesAscentGame::Draw(Renderer& renderer, const WorldState& world, const XMMATRIX& viewProjection)
    {
        mRenderer = &renderer;
        mMap.DrawModels(renderer, world, viewProjection, 0);
        DrawNests(renderer, world, viewProjection);
        if (mCreaturesOn) mCreatures.DrawMountain(renderer, world, viewProjection);
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
        for (const auto& n : mMap.Nests)   // the Eagle Nests, the only places she is saved
            mark(n.P, 3.2f, n.Station == mMap.Checkpoint ? L"EAGLE NEST  (rested)" : L"EAGLE NEST  (rest here: saved)", Palette::Gold);

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
        if (mPaused)
        {
            const bool nests = !mMap.Nests.empty();
            const float w = 460 * s, h = (nests ? 194 : 170) * s, px = (ui.W - w) * .5f, py = (ui.H - h) * .45f;
            ui.Rect(0, 0, ui.W, ui.H, {0, 0, 0, .35f});
            ui.Rect(px, py, w, h, {.015f, .025f, .04f, .92f});
            ui.Centered(L"PAUSED", ui.W * .5f, py + 18 * s, big, Palette::Gold);
            ui.Centered(L"Esc   back to the climb", ui.W * .5f, py + 62 * s, body, Palette::Ink);
            ui.Centered(nests ? L"Backspace   the last Eagle Nest" : L"Backspace   the last checkpoint", ui.W * .5f, py + 86 * s, body, Palette::Ink);
            ui.Centered(L"Enter   the start screen", ui.W * .5f, py + 110 * s, body, Palette::Ink);
            if (nests) ui.Centered(L"N   a new climb from the shore (forgets the nests and the beaten)", ui.W * .5f, py + 134 * s, body, Palette::Ink);
            ui.Centered(L"Q   leave the game", ui.W * .5f, py + (nests ? 158 : 134) * s, body, Palette::Dim);
        }
        return true;
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
        mSavedNest = 0; mSavedBeaten.clear();
        if (mMap.Nests.empty()) return;
        std::ifstream in(mSavePath);
        std::string word;
        while (in >> word)
        {
            if (word == "nest") in >> mSavedNest;
            else if (word == "beaten") { std::string id; if (in >> id) mSavedBeaten.push_back(id); }
            else std::getline(in, word);
        }
        if (mSavedNest < 0 || mSavedNest >= static_cast<int>(mMap.Stations.size()) || !mMap.Stations[static_cast<size_t>(mSavedNest)].Checkpoint) mSavedNest = 0;
    }

    void EaglesAscentGame::WriteSave() const
    {
        if (GameSession().Unattended && !mSaveTest) return;   // (an unattended run never touches a player's save)
        std::error_code ignored;
        std::filesystem::create_directories(mSavePath.parent_path(), ignored);
        std::ofstream out(mSavePath, std::ios::trunc);
        out << "# The Eagle's Ascent: the last Eagle Nest she rested in, and the creatures she has beaten\n";
        out << "nest " << mMap.Checkpoint << "\n";
        if (mCreaturesOn)
            for (const auto& id : mCreatures.MountainBeaten()) out << "beaten " << id << "\n";
        else
            for (const auto& id : mSavedBeaten) out << "beaten " << id << "\n";
    }

    void EaglesAscentGame::NewClimb(DoorwaysGameHost& host)
    {
        mSavedNest = 0; mSavedBeaten.clear(); mReached = 0;
        std::error_code ignored;
        if (!GameSession().Unattended || mSaveTest) std::filesystem::remove(mSavePath, ignored);
        if (mCreaturesOn) mCreatures.SetMountainCreatures(mCreatureList, {});
        mBeatenWritten = 0;
        mMap.Checkpoint = 0;
        mMap.Visit(host, 0);
        Notify(L"A NEW CLIMB", L"From the shore. The nests and the creatures she beat are forgotten.", 4);
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
