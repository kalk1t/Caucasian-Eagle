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
        float Cooldown[3] = {}, CooldownMax[3] = {4, 7, 9};
        bool ComboOpen = false; float ComboWindow = 0;      // LMB / RMB follow-ups available
        const wchar_t* ComboLeft = L""; const wchar_t* ComboRight = L"";
        const wchar_t* Action = L"";                         // current action name
        bool Guarding = false, Boosting = false, Swapped = false, Slashing = false, Heavy = false, Swapping = false;
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
