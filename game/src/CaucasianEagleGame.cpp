#include "CaucasianEagleGame.h"

#include "AmiraniMountain.h"
#include "Nature.h"

#include "CombatSystem.h"
#include "DoorwaysAssetPaths.h"
#include "DoorwaysGamepad.h"
#include "DoorwaysRenderContexts.h"
#include "GameConstants.h"
#include "GameUi.h"
#include "Journey.h"
#include "NyxaraHud.h"
#include "NyxaraKit.h"
#include "Sound.h"
#include "StartScreen.h"
#include "Renderer.h"
#include "WorldRenderer.h"
#include "WorldState.h"

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

// Caucasian Eagle on the Doorways engine.
//
// Configure  picks the start scene, window title and user-data folder before the engine starts.
// Prepare    replaces the engine default player with Nyxara (MixamoV1 bundle: built from Mixamo clips by
//            art/nyxara/blender/build_from_mixamo.py, two swords and wings, no bow; NyxaraKit::LoadBody), sets the
//            camera / input / movement options and starts her combat kit. Collision and physics stay engine rules;
//            her jump is a clip on foot (the kit), R takes her up and lands her (the journey).
// Update / Draw forward to NyxaraKit and the journey; DrawHud makes the frame's Ui (GameUi.h) and
// hands it to the hero HUD, the journey HUD and the story journal.

namespace CaucasianEagle
{
    using namespace Doorways;

    namespace
    {
        bool gVista = false; // capture mode: the camera looks level toward the mountain
    }

    // Her travel, flight, camera, keys and controller buttons: the game's own engine options (DoorwaysGame.h). Both ways
    // into the game use them: the road (Game) and the Eagle's Ascent (EaglesAscentGame.cpp).
    void PrepareNyxaraControls(DoorwaysGameHost& host)
    {
        auto& session = GameSession();
        session.SlowWalk = true; session.SlowWalkSpeed = WalkSpeed; // hold Ctrl to walk
        session.FastMultiplier = WingBoostMultiplier;               // Shift wing boost
        session.FreeFlight = true;                                  // R takes her up and lands her (Journey)
        session.FreeFlightCeiling = FlightCeiling;
        session.FreeFlightHoldSeconds = 1.0e9f; session.FreeFlightKeyClimb = true; session.FreeFlightKeepAloft = true;   // Space never takes off (on foot it jumps); in flight Space climbs, X sinks
        session.FreeFlightTakeoffHeight = FlightTakeoffHeight; session.FreeFlightTakeoffSeconds = FlightTakeoffSeconds;
        session.FreeFlightSlowJumpSinks = true;                     // Ctrl + Space flies her down, as Space flies her up (owner, 2026-11-18)
        session.FreeFlightSteer = true; session.FreeFlightLevelPitch = CameraPitch; session.FreeFlightSteerGain = FlightSteerGain;
        session.OrbitCamera = true;
        session.LookScale = CameraLookScale;                        // a calmer camera than the engine's own
        session.Cursor = PointerFile;                               // her own pointer, wherever the pointer shows
        session.CameraPitch = CameraPitch; session.CameraDistance = CameraDistance; session.CameraPivotHeight = CameraPivotHeight;
        session.CameraMinPitch = CameraMinPitch; session.CameraMaxPitch = CameraMaxPitch;
        session.CameraLookUpNear = CameraLookUpNear; session.CameraLookUpKeep = CameraLookUpKeep; session.CameraLookUpClear = CameraLookUpClear; session.CameraLookUpSide = CameraLookUpSide;
        session.CameraPivotSmoothing = CameraPivotSmoothing; session.CameraLiftUnder = CameraLiftUnder; session.CameraPivotLagMost = CameraPivotLagMost;
        if (char lag[16]{}; GetEnvironmentVariableA("CAUCASIAN_EAGLE_PIVOT_LAG", lag, sizeof lag))
            session.CameraPivotLagMost = static_cast<float>(std::atof(lag));   // (tests only; 0: the camera's height as far behind hers as in 0.5.3)
        if (char old[4]{}; GetEnvironmentVariableA("CAUCASIAN_EAGLE_CAMERA_OLD", old, sizeof old) && old[0] == '1')
        {   // (tests only: the camera as it was before 2026-11-22, to measure the new one against)
            session.CameraLookUpNear = 0; session.CameraLookUpSide = 0; session.CameraPivotSmoothing = 0; session.CameraLiftUnder = 0; session.CameraMaxPitch = .45f;
        }
        session.SuppressedActions = {
            DoorwaysInputAction::ResetPlayer,                                // R = land from flight
            DoorwaysInputAction::Interact,                                   // E = sword barrier
            DoorwaysInputAction::CycleDebugRenderMode,                       // Q = nothing (it was the bow)
            DoorwaysInputAction::ToggleDebugAnimationSampling,               // G = nothing (it changed her swords)
            DoorwaysInputAction::ToggleTextureFilter,                        // F = the slide (in flight the twisted dash)
            DoorwaysInputAction::ToggleImportedAnimatedDebugSampling,        // V = the thrown swords
            DoorwaysInputAction::ToggleImportedSkinningSideBySideComparison, // C = the feather storm
            DoorwaysInputAction::RuntimeSceneSummary,                        // F8 = save the game here
            DoorwaysInputAction::BasicAttack,                                // LMB = the left sword
            DoorwaysInputAction::HeavyAttack,                                // RMB = the right sword
            DoorwaysInputAction::ToggleRuntimeObjectBounds,                  // F1 = story journal
            DoorwaysInputAction::ToggleImportedGpuSkinnedPreview,            // M = world map
            DoorwaysInputAction::ToggleInventory,                            // I = Nyxara's own window (Growth.h), not the engine inventory
            DoorwaysInputAction::Dodge};                                     // no dodge: she guards, dashes and flies instead
        session.OwnPauseMenu = true;                                         // Esc = her own menu (Journey), not the engine's pause menu
        // Game controller (engine: DoorwaysGamepad.h). The engine's defaults stay: left stick moves, right stick looks,
        // A jump / fly, B dodge, X slash, Y heavy, LT wing boost, left stick click walk, Start pause.
        // Nyxara's own keys go on what is left.
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::RightShoulder, 'F');    // the slide / the twisted dash
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::RightTrigger, 'V');     // the thrown swords
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::DpadUp, 'C');           // the feather storm
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::LeftShoulder, 'E');     // sword barrier
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::DpadRight, 'M');        // world map
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::DpadDown, VK_F1);       // story journal
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::Back, 'I');             // Nyxara's window
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::RightThumb, 'Z');       // drink a potion
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::B, 'X');                // sink in free flight (B was the dodge)
        host.World.MainCamera.Pitch = CameraPitch;
        host.World.MainCamera.MaxFollowDistance = CameraMaxDistance;   // before the distance: the engine holds it under this
        host.World.MainCamera.FollowDistance = CameraDistance;
    }

    void Game::Configure(DoorwaysGameSession& session)
    {
        session.SceneId = StartSceneId;
        session.SceneCollision = true; // the road is the only ground: Nyxara cannot leave it
        session.WindowTitle = GameTitle;
        session.UserDataFolder = UserDataFolder;
    }

    bool Game::Prepare(DoorwaysGameHost& host, std::string& error)
    {
        char hero[32] = {};   // CAUCASIAN_EAGLE_HERO=<bundle>: another build of her body (it needs the Mixamo build's clips)
        const bool other = GetEnvironmentVariableA(HeroEnvironmentVariable, hero, sizeof(hero)) > 0 && hero[0] && std::string(hero).find_first_of("/\\.") == std::string::npos &&
            std::filesystem::exists(GetDoorwaysAssetPathRegistry().CompiledRoot / HeroBundleFolder / hero / "Character.dmesh");
        const std::filesystem::path bundle = GetDoorwaysAssetPathRegistry().CompiledRoot / (other ? (std::filesystem::path(HeroBundleFolder) / hero) : std::filesystem::path(HeroBundle));

        // Her body (the Mixamo build, bundle MixamoV1): the kit loads it, keeps its clips and poses her itself.
        if (!GetNyxaraKit().LoadBody(host.Render, host.WorldRender, host.World, bundle, error))
        {
            error = "Nyxara: " + error;
            return false;
        }

        PrepareNyxaraControls(host);   // her controls, camera and controller buttons (shared with the Eagle's Ascent)
        auto& session = GameSession();

        if (!Ui::LoadArt(host.Render, error)) return false;
        if (!GetAmiraniMountain().Initialize(host.Render, error))
        {
            error = "Mountain of Amirani: " + error;
            return false;
        }
        if (!GetNyxaraKit().Initialize(host.Render, host.WorldRender, host.World, error))
        {
            error = "Nyxara combat kit: " + error;
            return false;
        }
        OutputDebugStringA(("[PASS] Caucasian Eagle: Nyxara (" + bundle.filename().string() + ") is the player and her combat kit started.\n").c_str());

        char demo[8] = {};
        const bool unattended = GetEnvironmentVariableA(DemoEnvironmentVariable, demo, sizeof(demo)) && demo[0] == '1';
        if (unattended)
        {   // Scripted run for capture and testing: never reads the desktop.
            session.Unattended = true;
            char vista[8] = {};
            gVista = GetEnvironmentVariableA(VistaEnvironmentVariable, vista, sizeof(vista)) && vista[0] == '1';
        }
        if (!GetNature().Initialize(host.Render, error))
        {
            error = "Nature: " + error;
            return false;
        }
        if (!GetJourney().Initialize(host.Render, host.World, error))
        {
            error = "Road to Amirani: " + error;
            return false;
        }
        // The start screen: the engine's main menu with this game's words, at the left of the painted picture.
        session.MenuTitle = L"CAUCASIAN EAGLE";
        session.MenuSubtitle = L"The road to Amirani";
        // she begins on the road itself (and returns there when she falls before the first arena is won)
        session.OwnDeath = true;   // no engine panel when she falls: she lies a moment and wakes at the last saved point (Journey)
        session.OwnSpawn = true; session.SpawnPosition = GetJourney().StartPoint(); session.SpawnYaw = 0;
        host.World.MainPlayerSpawn.Position = session.SpawnPosition; host.World.MainPlayerSpawn.Yaw = 0;
        session.MenuPlayLabel = GetJourney().HasProgress() ? L"Continue the journey" : L"Begin the journey";
        // who made it; these also show on the engine's Credits screen should the game's own start screen ever be off
        session.MenuCredits = {L"Caucasian Eagle", L"A game by Giorgi Samkharadze", L"Built in Doorways, a custom engine",
                               L"founded by Giorgi Samkharadze", L"in February 2026"};
        session.MenuHideLinks = true;
        session.MenuGuideLabel = L"Guide";                                   // how to play and what the game needs (Guide.cpp)
        session.OwnSettings = true;                                          // and so is the settings window (Settings.cpp); the engine's screens never open
        session.OwnStartScreen = true;                                       // the start screen is the game's own (StartScreen.cpp)
        Sound::Initialize();
        Sound::StartMusic("music/menu");   // the start screen's music; the journey fades into its own when play begins
        session.MenuBackdrop = StartScreenPicture;
        session.MenuPanelSide = -1;
        if (unattended) host.StartGame();
        return true;
    }

    float Game::ScaleFrameTime(float realDeltaTime, bool inputAllowed)
    {
        return GetNyxaraKit().ScaleFrameTime(realDeltaTime, inputAllowed, GetJourney().TimeScale());
    }

    void Game::Update(DoorwaysGameHost& host, float deltaTime, float realDeltaTime, bool inputAllowed)
    {
        GetAmiraniMountain().Update(realDeltaTime);
        GetNature().Update(realDeltaTime);
        if (gVista) { host.World.MainCamera.Yaw = 0; host.World.MainCamera.Pitch = VistaPitch; }
        GetNyxaraKit().Update(host.World, host.WorldRender, host.Combat, deltaTime, realDeltaTime, inputAllowed);
        GetJourney().Update(host, deltaTime, realDeltaTime, inputAllowed);
    }

    void Game::Draw(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection)
    {
        GetAmiraniMountain().Draw(renderer, world.MainCamera.Position, viewProjection);
        GetNature().DrawClouds(renderer, world.MainCamera.Position, viewProjection);
        GetNature().Draw(renderer, world.MainCamera.Position, viewProjection);
        GetJourney().Draw(renderer, world, viewProjection);
        if (!gVista) GetNyxaraKit().Draw(renderer, world, viewProjection); // capture mode hides her effects
    }

    void Game::AdjustEnvironment(EnvironmentSettings& environment)
    {
        AmiraniMountain::StyleEnvironment(environment);
        GetJourney().StyleEnvironment(environment);
    }

    void Game::DrawStartScreen(const DoorwaysHudRenderContext& context, HudTextRenderer& text, bool focused)
    {
        StartScreen::Draw(context, text, focused);
    }

    bool Game::DrawHud(const DoorwaysHudRenderContext& context, HudTextRenderer& text)
    {
        if (gVista) return true; // capture mode: no HUD
        if (!NyxaraHud().Valid || !context.World.CombatFeedback.MeleeActive) return false;
        Ui ui(context, text);
        DrawNyxaraHud(ui, context);
        GetJourney().DrawHud(ui, context.World);
        return true;
    }
}
