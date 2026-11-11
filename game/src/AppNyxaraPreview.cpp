#include "App.h"
#include "WorldState.h"
#include "DoorwaysAssetPaths.h"
#include "DoorwaysRuntimeSceneSelection.h"
#include "DoorwaysNyxaraPreview.h"
#include "NyxaraKit.h"

#include <string>
#include <vector>

// Nyxara normal-play session: Nyxara, daughter of the eagle.
//
// Replaces the default production player visual after a successful App::Initialize with
// the EagleV1 bundle (Meshy body + rigged eagle wings + Moonfeather/Talonshade swords,
// 30 joints, 31 authored clips; source: Nyxara/blender/assemble.py) and starts the
// Nyxara combat kit (NyxaraKit.h). Collision, physics, jump and hold-Space flight stay
// on the normal-play rules. Disabled by setting DoorwaysNyxaraPlayerPreview to 0 in
// DoorwaysRuntimeSceneSelection.h.
bool App::PrepareNyxaraPlayerPreview()
{
#if DoorwaysNyxaraPlayerPreview
    using namespace Doorways;
    const std::filesystem::path bundle =
        GetDoorwaysAssetPathRegistry().CompiledRoot / "Characters/Nyxara/EagleV1";

    struct Clip { const char* Name; bool Loop; float GroundSpeed; };
    // Ground speeds are the measured planted-foot treadmill speeds of the authored clips.
    static const std::vector<Clip> clips = {
        {"Idle", true, 0}, {"Walk", true, NyxaraPreviewWalkSpeed}, {"Run", true, 4.6f},
        {"WalkBackward", true, 1.33f}, {"StrafeLeft", true, 4.6f}, {"StrafeRight", true, 4.6f},
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

    std::string error;
    const auto fail = [&](const std::string& message) {
        const std::string text = "Nyxara character failed to load:\n" + message +
            "\n\nSet DoorwaysNyxaraPlayerPreview to 0 in DoorwaysRuntimeSceneSelection.h to use the previous character.";
        OutputDebugStringA((text + "\n").c_str());
        MessageBoxA(nullptr, text.c_str(), "Doorways - Nyxara", MB_OK | MB_ICONERROR);
        return false;
    };

    DoorwaysAnimatedCharacterAsset asset;
    if (!LoadDoorwaysAnimatedCharacterAsset(desc, asset, &error)) return fail(error);
    if (!mWorldRenderer.InitializeSkinnedMaterials(mRenderer, bundle, bundle / "Character.dmat", asset.Skeleton.Name, error))
        return fail(error);
    if (!mWorld.ImportedAnimatedCharacter.InitializeFromAsset(asset, &error)) return fail(error);

    mWorldRenderer.InvalidateCharacterMeshForAssetChange();
    mWorld.ImportedAnimatedCharacter.ResetPlaybackForSession();
    mWorld.ImportedAnimatedCharacter.SetRootTransform(mWorld.MainPlayer.Position, mWorld.MainPlayer.Yaw);
    auto& visual = mWorld.AnimatedPlayerVisual;
    visual.Enabled = true; visual.Scale = {1, 1, 1}; visual.Rotation = {0, 0, 0}; visual.Translation = {0, 0, 0}; visual.YawOffset = 0;
    SetNyxaraPreviewWalkEnabled(true); // Hold Ctrl (MoveSlow) to walk.
    mWorld.MainCamera.Pitch = NyxaraCameraPitch; // orbit camera looking down from above
    mWorld.MainCamera.FollowDistance = NyxaraCameraDistance;

    if (!GetNyxaraKit().Initialize(mRenderer, mWorldRenderer, mWorld, error)) return fail("Nyxara combat kit: " + error);
    mWorldRenderer.SetNyxaraDrawCallback([this](Renderer& renderer, const DirectX::XMMATRIX& viewProjection)
        { GetNyxaraKit().Draw(renderer, mWorld, viewProjection); });
    OutputDebugStringA("[PASS] Nyxara (EagleV1) replaced the normal-play player and started her combat kit.\n");
    char demo[8] = {};
    if (GetEnvironmentVariableA("DOORWAYS_NYXARA_DEMO", demo, sizeof(demo)) && demo[0] == '1')
    {   // Scripted showcase for capture: starts play and drives the kit from its own timeline.
        NyxaraSession().Demo = true;
        ApplyMenuAction(DoorwaysMenuAction::StartGame);
    }
    return mWorld.ImportedAnimatedCharacter.SampleAuthoredClip("Idle", 0, &error) || fail(error);
#else
    return true;
#endif
}
