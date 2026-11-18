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
        session.WindowTitle = std::wstring(GameTitle) + L" - The Eagle's Ascent";
        session.UserDataFolder = UserDataFolder;   // the same settings as the road; nothing is saved here
    }

    bool EaglesAscentGame::Prepare(DoorwaysGameHost& host, std::string& error)
    {
        if (!mMap.Load(ResolveDoorwaysRuntimePath(AscentDataFile), error)) { error = "The Eagle's Ascent: " + error; return false; }
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
        camera.FarZ = 14000; camera.NearZ = .25f;              // the summit must be in view from the shore
        if (!Ui::LoadArt(host.Render, error)) return false;
        if (!GetNyxaraKit().Initialize(host.Render, host.WorldRender, host.World, error)) { error = "Nyxara combat kit: " + error; return false; }
        if (!mMap.LoadModels(host.Render, error)) { error = "The Eagle's Ascent: " + error; return false; }
        GetNyxaraKit().SetOnFoot(true);
        // her feet on the stair treads as drawn (she walks the ramp under them); this game lives as long as the program
        GetNyxaraKit().SetFootGround([this](float x, float z, float nearY) { return mMap.TreadTop(x, z, nearY); });
        GetNyxaraKit().SetStairTread([this](float x, float z, float top, float& cx, float& cz, float& ax, float& az) { return mMap.TreadCenter(x, z, top, cx, cz, ax, az); });

        // she begins on the shore (testing: CAUCASIAN_EAGLE_START=<station>)
        const std::string first = Env(StartEnvironmentVariable);
        const int station = first.empty() ? 0 : std::clamp(std::atoi(first.c_str()), 0, static_cast<int>(mMap.Stations.size()) - 1);
        const auto& start = mMap.Stations[static_cast<size_t>(station)];
        session.OwnSpawn = true; session.SpawnPosition = {start.P.x, start.P.y + .05f, start.P.z}; session.SpawnYaw = start.Yaw;
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

        mSmoke = Env(DemoEnvironmentVariable) == "1";   // the unattended test of the mode
        mClimb = Env(DemoEnvironmentVariable) == "2";   // the whole climb, unattended
        mLog = Env(AscentLogEnvironmentVariable);
        if (mLog.empty()) mLog = ResolveDoorwaysRuntimePath("eagles-ascent-evidence.tsv").string();
        if (mSmoke || mClimb)
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
        return mPaused ? 0 : GetNyxaraKit().ScaleFrameTime(realDeltaTime, inputAllowed);
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

    void EaglesAscentGame::Update(DoorwaysGameHost& host, float dt, float realDt, bool inputAllowed)
    {
        mClock += realDt; mNotice = std::max(0.f, mNotice - realDt);
        auto& session = GameSession(); auto& world = host.World; auto& p = world.MainPlayer; auto& kit = GetNyxaraKit();
        const bool flying = p.TraversalMode == DoorwaysPlayerTraversalMode::Flight;
        const bool alive = host.Combat.PlayerActionsAllowed();
        mMap.KeepOutOfRock(p, flying, dt);

        // Esc: the pause panel (DrawHud); its keys
        if (session.EscapePressed) { session.EscapePressed = false; mPaused = !mPaused; }
        if (mPaused)
        {
            if (Press(VK_BACK, inputAllowed)) { mPaused = false; mMap.Visit(host, mMap.Checkpoint); Notify(L"BACK AT THE CHECKPOINT", Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name), 3); }
            if (Press(VK_RETURN, inputAllowed)) { mPaused = false; session.MenuRequest = DoorwaysGameSession::Request::MainMenu; }
            if (Press('Q', inputAllowed)) session.MenuRequest = DoorwaysGameSession::Request::Exit;
            kit.SetInputBlocked(true);
            kit.Update(world, host.WorldRender, host.Combat, 0, realDt, false);
            return;
        }
        const bool allowed = inputAllowed && alive && !mSmoke && !mClimb;

        // R: take off on foot; in flight, land (she sinks at LandSpeed until she stands)
        if (Press('R', allowed))
        {
            if (flying) { session.FreeFlightEnd = true; session.FreeFlightLandSpeed = LandSpeed; }
            else { session.FreeFlightTakeoff = true; session.FreeFlightLandSpeed = 0; }
            Sound::Play("nyxara/wing", .8f);
        }
        if (flying && !mFlying) Notify(L"IN FLIGHT", L"W flies where you look. Space climbs, X sinks, R lands.", 4);
        if (!flying)
        {
            if (mFlying) { Sound::Play("nyxara/land", .6f); session.FreeFlightLandSpeed = 0; }
            session.FreeFlightEnd = false;
        }
        mFlying = flying;
        if (Press(VK_BACK, allowed)) { mMap.Visit(host, mMap.Checkpoint); mMap.Rescued = 3; Notify(L"BACK AT THE CHECKPOINT", Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name), 3); }

        // her speed: the road's. On foot a jog (Shift runs, Ctrl walks); in flight the engine uses her FlightSpeed, unchanged
        p.MoveSpeed = FootSpeed * kit.SpeedScale();
        session.FastMultiplier = flying ? WingBoostMultiplier : FootRunSpeed / FootSpeed;
        kit.SetOnFoot(!flying);
        kit.SetHeightOverGround(std::max(0.f, p.Position.y - mMap.Land(p.Position.x, p.Position.z)));
        kit.SetInputBlocked(mSmoke || mClimb);
        if (mSmoke) Smoke(host, realDt);
        if (mClimb) Climb(host, dt);
        kit.Update(world, host.WorldRender, host.Combat, dt, realDt, allowed);

        // the map: where she is, the air, the checkpoints, the safety nets
        mMap.Locate(p.Position);
        const int before = mMap.Checkpoint;
        if (const char* net = mMap.Air(host, dt, flying, !mSmoke && mClock > 5 && alive))
        {
            Notify(L"BACK AT THE CHECKPOINT", L"The mountain did not hold her: she is back at " + Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name) + L".", 4);
            if (mClimb) { Evidence(host.World, net); Evidence(host.World, "CLIMB_FAIL"); mClimb = false; PostQuitMessage(2); }
        }
        else if (mMap.Checkpoint != before && mMap.Checkpoint > mReached)
        {
            mReached = mMap.Checkpoint;
            Notify(L"CHECKPOINT", Words(mMap.Stations[static_cast<size_t>(mMap.Checkpoint)].Name), 4);
            Sound::PlayUi("ui/open", .6f);
        }

        // she fell (no monsters here, but the engine's own harm): she lies a moment and wakes at the last checkpoint
        if (!alive)
        {
            if (!mWasDead) { mDeadFor = 0; mRespawnAsk = 0; Sound::Play("nyxara/death", 1, 0); Notify(L"NYXARA HAS FALLEN", L"She wakes at the last checkpoint.", DeathSeconds + 1.5f); }
            mDeadFor += realDt;
            if (mDeadFor >= DeathSeconds && (mRespawnAsk -= realDt) <= 0)
            {
                mRespawnAsk = .5f;
                const auto& s = mMap.Stations[static_cast<size_t>(mMap.Checkpoint)];
                session.SpawnPosition = {s.P.x, s.P.y + .05f, s.P.z}; session.SpawnYaw = s.Yaw;
                const auto& life = host.Combat.PlayerLife();
                host.Combat.RequestRespawn({life.Life, life.Death.Event, life.Death.Destination});
            }
        }
        else if (mWasDead) mMap.Visit(host, mMap.Checkpoint);
        mWasDead = !alive;
    }

    void EaglesAscentGame::Draw(Renderer& renderer, const WorldState& world, const XMMATRIX& viewProjection)
    {
        mMap.DrawModels(renderer, world, viewProjection, 0);
        GetNyxaraKit().Draw(renderer, world, viewProjection);
    }

    void EaglesAscentGame::AdjustEnvironment(EnvironmentSettings& environment)
    {
        EaglesAscent::StyleEnvironment(environment, 0, 1);   // day, with the map's haze (Map A Gate 4)
    }

    void EaglesAscentGame::DrawStartScreen(const DoorwaysHudRenderContext& context, HudTextRenderer& text, bool focused)
    {
        StartScreen::Draw(context, text, focused);
    }

    bool EaglesAscentGame::DrawHud(const DoorwaysHudRenderContext& context, HudTextRenderer& text)
    {
        if (mSmoke) return true;
        Ui ui(context, text);
        const float s = std::clamp(ui.S, .8f, 1.5f);
        if (NyxaraHud().Valid && context.World.CombatFeedback.MeleeActive) DrawNyxaraHud(ui, context);
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
            if (f.Kind == "wall") mark(f.From, 4, L"THE ROAD GOES ON ABOVE  (R, then Space: fly up)", Palette::Moon);
            else if (f.Kind == "gap") mark(f.From, 4, L"THE ROAD IS BROKEN  (R: fly across)", Palette::Moon);
            else if (f.Kind == "slot") mark(f.From, 4, L"A CLEFT IN THE ROCK  (R: fly through)", Palette::Moon);
            else if (f.Kind == "updraft") mark(f.From, 4, L"RISING AIR  (R, then let it carry you)", Palette::Soul);
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
            const float w = 420 * s, h = 170 * s, px = (ui.W - w) * .5f, py = (ui.H - h) * .45f;
            ui.Rect(0, 0, ui.W, ui.H, {0, 0, 0, .35f});
            ui.Rect(px, py, w, h, {.015f, .025f, .04f, .92f});
            ui.Centered(L"PAUSED", ui.W * .5f, py + 18 * s, big, Palette::Gold);
            ui.Centered(L"Esc   back to the climb", ui.W * .5f, py + 62 * s, body, Palette::Ink);
            ui.Centered(L"Backspace   the last checkpoint", ui.W * .5f, py + 86 * s, body, Palette::Ink);
            ui.Centered(L"Enter   the start screen", ui.W * .5f, py + 110 * s, body, Palette::Ink);
            ui.Centered(L"Q   leave the game", ui.W * .5f, py + 134 * s, body, Palette::Dim);
        }
        return true;
    }
}
