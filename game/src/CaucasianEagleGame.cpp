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
// Prepare    replaces the engine default player with Nyxara (EagleV1 bundle: Meshy body,
//            rigged eagle wings, Moonfeather / Talonshade swords, 31 authored clips; built by
//            art/nyxara/assemble.py), sets the camera / input / movement options and starts
//            her combat kit. Collision, physics, jump and hold-Space flight stay engine rules.
// Update / Draw forward to NyxaraKit and the journey; DrawHud makes the frame's Ui (GameUi.h) and
// hands it to the hero HUD, the journey HUD and the story journal.

namespace CaucasianEagle
{
    using namespace Doorways;

    namespace
    {
        bool gVista = false; // capture mode: the camera looks level toward the mountain
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
        const std::filesystem::path bundle = GetDoorwaysAssetPathRegistry().CompiledRoot / HeroBundle;

        struct Clip { const char* Name; bool Loop; float GroundSpeed; };
        // Ground speeds are the measured planted-foot treadmill speeds of the authored clips.
        static const std::vector<Clip> clips = {
            {"Idle", true, 0}, {"Walk", true, WalkSpeed}, {"Run", true, RunClipSpeed},
            {"WalkBackward", true, WalkBackClipSpeed}, {"StrafeLeft", true, RunClipSpeed}, {"StrafeRight", true, RunClipSpeed},
            {"Jump", false, 0}, {"JumpAirborne", false, 0}, {"JumpLanding", false, 0},
            {"BasicAttack", false, 0}, {"SlashLeft", false, 0}, {"HeavyAttack", false, 0},
            {"Dodge", false, 0}, {"HitReact", false, 0}, {"Death", false, 0}, {"Respawn", false, 0},
            {"Skill1", false, 0}, {"Skill2", false, 0}, {"Skill3", false, 0},
            {"Combo1L", false, 0}, {"Combo1R", false, 0}, {"Combo2L", false, 0},
            {"Combo2R", false, 0}, {"Combo3L", false, 0}, {"Combo3R", false, 0},
            {"Guard", false, 0}, {"SwordSwap", false, 0},
            {"FlightTakeoff", false, 0}, {"FlightHover", true, 0}, {"FlightTravel", true, 0}, {"FlightLanding", false, 0}};

        DoorwaysAnimatedCharacterAssetLoadDesc desc;
        desc.Name = "Nyxara EagleV1 (daughter of the eagle)";
        desc.MeshPath = bundle / "Character.dmesh";
        desc.SkeletonPath = bundle / "Character.dskel";
        desc.IdleAnimationPath = bundle / "Idle.danim";
        desc.WalkAnimationPath = bundle / "Walk.danim";
        desc.BasicAttackAnimationPath = bundle / "BasicAttack.danim";
        desc.RequireRigSignature = true;
        desc.AllowMissingFiles = false;
        desc.MeshRigBindingPath = bundle / "Character.meshbind";
        desc.LoadPolicy = DoorwaysAnimatedCharacterAssetLoadPolicy::RequiredProduction;
        for (const auto& clip : clips)
            desc.AuthoredClips.emplace(clip.Name, DoorwaysAuthoredClipSource{bundle / (std::string(clip.Name) + ".danim"), clip.Loop, clip.GroundSpeed});

        DoorwaysAnimatedCharacterAsset asset;
        if (!LoadDoorwaysAnimatedCharacterAsset(desc, asset, &error)) return false;
        if (!host.WorldRender.InitializeSkinnedMaterials(host.Render, bundle, bundle / "Character.dmat", asset.Skeleton.Name, error))
            return false;
        auto& character = host.World.ImportedAnimatedCharacter;
        if (!character.InitializeFromAsset(asset, &error)) return false;

        host.WorldRender.InvalidateCharacterMeshForAssetChange();
        character.ResetPlaybackForSession();
        character.SetRootTransform(host.World.MainPlayer.Position, host.World.MainPlayer.Yaw);
        auto& visual = host.World.AnimatedPlayerVisual;
        visual.Enabled = true; visual.Scale = {1, 1, 1}; visual.Rotation = {0, 0, 0}; visual.Translation = {0, 0, 0}; visual.YawOffset = 0;

        // Engine options owned by this game (DoorwaysGame.h).
        auto& session = GameSession();
        session.SlowWalk = true; session.SlowWalkSpeed = WalkSpeed; // hold Ctrl to walk
        session.FastMultiplier = WingBoostMultiplier;               // Shift wing boost
        session.OrbitCamera = true;
        session.LookScale = CameraLookScale;                        // a calmer camera than the engine's own
        session.Cursor = PointerFile;                               // her own pointer, wherever the pointer shows
        session.CameraPitch = CameraPitch; session.CameraDistance = CameraDistance; session.CameraPivotHeight = CameraPivotHeight;
        session.CameraMinPitch = CameraMinPitch; session.CameraMaxPitch = CameraMaxPitch;
        session.SuppressedActions = {
            DoorwaysInputAction::Interact,                                   // E = sword barrier
            DoorwaysInputAction::CycleDebugRenderMode,                       // Q = sword swap
            DoorwaysInputAction::ToggleTextureFilter,                        // F = Talon Dash
            DoorwaysInputAction::ToggleImportedAnimatedDebugSampling,        // V = Feather Cyclone
            DoorwaysInputAction::ToggleImportedSkinningSideBySideComparison, // C = Moon Leap
            DoorwaysInputAction::BasicAttack,                                // LMB = slashes / combos
            DoorwaysInputAction::HeavyAttack,                                // RMB = heavy / combos
            DoorwaysInputAction::ToggleRuntimeObjectBounds,                  // F1 = story journal
            DoorwaysInputAction::ToggleImportedGpuSkinnedPreview,            // M = world map
            DoorwaysInputAction::ToggleInventory,                            // I = Nyxara's own window (Growth.h), not the engine inventory
            DoorwaysInputAction::Dodge};                                     // no dodge: she guards, dashes and flies instead
        session.OwnPauseMenu = true;                                         // Esc = her own menu (Journey), not the engine's pause menu
        // Game controller (engine: DoorwaysGamepad.h). The engine's defaults stay: left stick moves, right stick looks,
        // A jump / fly, B dodge, X slash, Y heavy, LT wing boost, left stick click walk, Start pause.
        // Nyxara's own keys go on what is left.
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::RightShoulder, 'F');    // Talon Dash
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::RightTrigger, 'V');     // Feather Cyclone
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::DpadUp, 'C');           // Moon Leap
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::LeftShoulder, 'E');     // sword barrier
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::DpadLeft, 'Q');         // sword swap
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::DpadRight, 'M');        // world map
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::DpadDown, VK_F1);       // story journal
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::Back, 'I');             // Nyxara's window
        BindDoorwaysGamepadButton(DoorwaysGamepadButton::RightThumb, 'Z');       // drink a potion
        host.World.MainCamera.Pitch = CameraPitch;
        host.World.MainCamera.FollowDistance = CameraDistance;

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
        OutputDebugStringA("[PASS] Caucasian Eagle: Nyxara (EagleV1) is the player and her combat kit started.\n");

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
        return character.SampleAuthoredClip("Idle", 0, &error);
    }

    float Game::ScaleFrameTime(float realDeltaTime, bool inputAllowed)
    {
        return GetNyxaraKit().ScaleFrameTime(realDeltaTime, inputAllowed) * GetJourney().TimeScale();
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
