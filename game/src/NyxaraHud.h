#pragma once

// Caucasian Eagle gameplay HUD: snapshot published by NyxaraKit, drawn by DrawNyxaraHud.

struct DoorwaysHudRenderContext;
namespace CaucasianEagle { class Ui; }

namespace Doorways
{
    // Presentation-only snapshot for the Nyxara HUD (filled by NyxaraKit, drawn by HudRenderer).
    struct NyxaraHudState
    {
        bool Valid = false;
        float Flight = 1, FlightCharge = 0; bool Flying = false;   // her flying energy 0..1; the take-off hold 0..1 (Space held); she is in flight
        bool FlightDash = false;                            // F works in flight too (the twisted dash); else its slot is dim there
        float Cooldown[3] = {}, CooldownMax[3] = {4, 7, 9};
        bool SkillsVC = false;                              // V and C are hers (their slots are shown)
        float GuardCooldown = 0, GuardCooldownMax = 7, GuardLeft = 0;   // her wing shield (E): resting; its rest; 1..0 of its time left while up
        float SoulRest = 0, SoulRestMax = 15; bool SoulOut = false;    // her soul (Q): resting, 15 s from her return to it (or its loss); its rest; it stands away from her
        bool ComboOpen = false; float ComboWindow = 0;      // LMB / RMB follow-ups available
        const wchar_t* ComboLeft = L""; const wchar_t* ComboRight = L"";
        const wchar_t* Action = L"";                         // current action name
        bool Guarding = false, Boosting = false, Swapped = false, Slashing = false, Heavy = false, Swapping = false;
        bool OnTarget = false, HitKill = false; float HitMark = 0;   // the mark is on an enemy; an arrow just hit (1 fading to 0); it felled it
        bool TargetVisible = false; float TargetX = 0, TargetY = 0, TargetSize = 0;   // the enemy the arrows will go to, on the screen (0..1)
        bool Bow = false, Shooting = false; float Aim = 0;   // the bow is out; arrows are flying; 0..1 she is aiming (the mark shows)
        const wchar_t* Notice = L""; float NoticeTimer = 0;
        struct Enemy { float X = 0, Y = 0, Health = 0, Flash = 0; bool Visible = false; };
        Enemy Enemies[8]; int EnemyCount = 0;                // X/Y in 0..1 screen space
    };
    inline NyxaraHudState& NyxaraHud()
    {
        static NyxaraHudState state;
        return state;
    }

    // Draws Nyxara's part of the HUD: vitals, skill bar, combo prompt, enemy bars and the move name.
    void DrawNyxaraHud(CaucasianEagle::Ui& ui, const DoorwaysHudRenderContext& context);
}
