#include "CaucasianEagleGame.h"

#include "AmiraniMountain.h"

#include "CombatSystem.h"
#include "DoorwaysAssetPaths.h"
#include "DoorwaysRenderContexts.h"
#include "GameConstants.h"
#include "Journey.h"
#include "NyxaraHud.h"
#include "NyxaraKit.h"
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
// Update / Draw / DrawHud forward to NyxaraKit and the gameplay HUD.

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
        session.CameraPitch = CameraPitch; session.CameraDistance = CameraDistance; session.CameraPivotHeight = CameraPivotHeight;
        session.CameraMinPitch = CameraMinPitch; session.CameraMaxPitch = CameraMaxPitch;
        session.SuppressedActions = {
            DoorwaysInputAction::Interact,                                   // E = sword barrier
            DoorwaysInputAction::CycleDebugRenderMode,                       // Q = sword swap
            DoorwaysInputAction::ToggleTextureFilter,                        // F = Talon Dash
            DoorwaysInputAction::ToggleImportedAnimatedDebugSampling,        // V = Feather Cyclone
            DoorwaysInputAction::ToggleImportedSkinningSideBySideComparison, // C = Moon Leap
            DoorwaysInputAction::BasicAttack,                                // LMB = slashes / combos
            DoorwaysInputAction::HeavyAttack};                               // RMB = heavy / combos
        host.World.MainCamera.Pitch = CameraPitch;
        host.World.MainCamera.FollowDistance = CameraDistance;

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
        if (!GetJourney().Initialize(host.Render, host.World, error))
        {
            error = "Road to Amirani: " + error;
            return false;
        }
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
        if (gVista) { host.World.MainCamera.Yaw = 0; host.World.MainCamera.Pitch = VistaPitch; }
        GetNyxaraKit().Update(host.World, host.WorldRender, host.Combat, deltaTime, realDeltaTime, inputAllowed);
        GetJourney().Update(host, deltaTime, realDeltaTime);
    }

    void Game::Draw(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection)
    {
        GetAmiraniMountain().Draw(renderer, world.MainCamera.Position, viewProjection);
        GetJourney().Draw(renderer, world, viewProjection);
        if (!gVista) GetNyxaraKit().Draw(renderer, world, viewProjection); // capture mode hides her effects
    }

    void Game::AdjustEnvironment(EnvironmentSettings& environment)
    {
        AmiraniMountain::StyleEnvironment(environment);
        GetJourney().StyleEnvironment(environment);
    }

    bool Game::DrawHud(const DoorwaysHudRenderContext& context, HudTextRenderer& text)
    {
        if (gVista) return true; // capture mode: no HUD
        if (!NyxaraHud().Valid || !context.World.CombatFeedback.MeleeActive) return false;
        DrawNyxaraHud(context, text);
        GetJourney().DrawHud(context, text);
        return true;
    }
}
