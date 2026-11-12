#define NOMINMAX
#include "NyxaraHud.h"

#include "DoorwaysRenderContexts.h"
#include "GameUi.h"
#include "WorldState.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <iomanip>
#include <sstream>
#include <string>

// Nyxara's part of the HUD. Presentation only: it reads the NyxaraHud() snapshot and the combat
// vitals text and owns no gameplay state (the little it keeps is for animation: the health trail,
// the hit flash and the flash of a skill that has just come back).
//
//   bottom left    emblem, health with a trail of what was just lost, energy, which sword is in which hand
//   bottom centre  eight skill slots in three groups (attacks, eagle skills, utility) with key badges
//   above the bar  the combo prompt and the name of the move just made
//   in the world   health bars over enemies
//   screen edge    red when health is low or she was just hit
void Doorways::DrawNyxaraHud(CaucasianEagle::Ui& ui, const DoorwaysHudRenderContext& context)
{
    using namespace CaucasianEagle;
    using namespace CaucasianEagle::Palette;
    const auto& hud = NyxaraHud();
    const float W = ui.W, H = ui.H, S = ui.S;

    // ---- layout, in layout units
    constexpr float Margin = 22, Emblem = 92, HealthWidth = 214, HealthHeight = 17, EnergyWidth = 168, EnergyHeight = 10;
    constexpr float SlotSize = 62, SlotGap = 7, GroupGap = 17, IconSize = 38, KeyHeight = 19, BelowSlots = 26;
    constexpr float LowHealth = .3f, TrailPerSecond = .35f, TrailDelay = .45f, ReadyFlash = .5f, HitFlash = .45f;
    constexpr float TextTiny = 1.55f, TextSmall = 1.9f, TextName = 2.05f;

    // soft shade under the HUD so it reads over bright ground
    ui.Sprite(HudArt::GradV, 0, H - 190 * S, W, 190 * S, Black.Fade(.62f));

    // ---------------------------------------------------------------- vitals
    int hp = 100, en = 100;
    swscanf_s(context.World.CombatFeedback.Vitals.c_str(), L"HP %d EN %d", &hp, &en);
    const float health = std::clamp(hp / 100.f, 0.f, 1.f), energy = std::clamp(en / 100.f, 0.f, 1.f);
    static float trail = 1, trailHold = 0, hit = 0, lastHealth = 1;
    if (health < lastHealth - .001f) { hit = HitFlash; trailHold = TrailDelay; }
    lastHealth = health;
    trailHold = std::max(0.f, trailHold - ui.Delta);
    if (trail < health) trail = health;
    else if (trailHold <= 0) trail = std::max(health, trail - TrailPerSecond * ui.Delta);
    hit = std::max(0.f, hit - ui.Delta);
    const bool low = health < LowHealth;
    const float pulse = .5f + .5f * std::sin(ui.Time * 6.f);

    const float edge = (low ? .22f + .16f * pulse : 0) + .45f * hit / HitFlash;
    if (edge > 0) ui.Sprite(HudArt::Vignette, 0, 0, W, H, Red.Fade(std::min(edge, .7f)));

    const float ex = Margin * S, ey = H - (Margin + Emblem) * S, es = Emblem * S;
    if (low) ui.Icon(HudArt::Glow, ex + es * .5f, ey + es * .5f, es * 1.7f, Red.Fade(.35f + .3f * pulse));
    ui.Sprite(HudArt::Medallion, ex, ey, es, es);
    ui.Icon(HudArt::IconWings, ex + es * .5f, ey + es * .5f, es * .66f, low ? Color{1.f, .62f, .58f, 1} : Silver);

    const float vx = ex + es + 12 * S;
    ui.Spaced(L"NYXARA", vx, ey + 9 * S, TextName * S, 3 * S, Silver);
    const float hy = ey + 34 * S;
    ui.Bar(vx, hy, HealthWidth * S, HealthHeight * S, health, low ? Red : Soul, trail);
    ui.Centered(std::to_wstring(hp), vx + HealthWidth * S * .5f, hy + (HealthHeight * S - ui.Height(TextTiny * S)) * .5f, TextTiny * S, Ink);
    const float ny = hy + (HealthHeight + 6) * S;
    ui.Bar(vx, ny, EnergyWidth * S, EnergyHeight * S, energy, Moon, 0, true);
    // which sword is in the right hand (the one that leads)
    const wchar_t* right = hud.Swapped ? L"TALONSHADE" : L"MOONFEATHER";
    const wchar_t* left = hud.Swapped ? L"MOONFEATHER" : L"TALONSHADE";
    const float wy = ny + (EnergyHeight + 7) * S;
    ui.Text(right, vx, wy, TextTiny * S, Ink);
    ui.Text(left, vx + ui.Width(right, TextTiny * S) + 12 * S, wy, TextTiny * S, Faint);

    // ---------------------------------------------------------------- skill bar
    struct Slot { const wchar_t* Key; const wchar_t* Name; const HudArt::Sprite* Art; float Cooldown, Max; bool Active, Glow; Color Accent; int Group; };
    const Slot slots[] = {
        {Ui::KeyLabel(L"LMB", VK_LBUTTON), L"SLASH", &HudArt::IconSlash, 0, 1, hud.Slashing, hud.ComboOpen, Silver, 0},
        {Ui::KeyLabel(L"RMB", VK_RBUTTON), L"HEAVY", &HudArt::IconHeavy, 0, 1, hud.Heavy, hud.ComboOpen, Silver, 0},
        {Ui::KeyLabel(L"F", 'F'), L"DASH", &HudArt::IconDash, hud.Cooldown[0], hud.CooldownMax[0], false, false, Soul, 1},
        {Ui::KeyLabel(L"V", 'V'), L"STORM", &HudArt::IconStorm, hud.Cooldown[1], hud.CooldownMax[1], false, false, Soul, 1},
        {Ui::KeyLabel(L"C", 'C'), L"LEAP", &HudArt::IconLeap, hud.Cooldown[2], hud.CooldownMax[2], false, false, Soul, 1},
        {Ui::KeyLabel(L"Q", 'Q'), L"SWAP", &HudArt::IconSwap, 0, 1, hud.Swapping, false, Moon, 2},
        {Ui::KeyLabel(L"E", 'E'), L"GUARD", &HudArt::IconGuard, 0, 1, hud.Guarding, false, Moon, 2},
        {Ui::KeyLabel(L"SHIFT", VK_SHIFT), L"BOOST", &HudArt::IconBoost, 0, 1, hud.Boosting, false, Moon, 2}};
    const int count = static_cast<int>(std::size(slots));
    const float size = SlotSize * S;
    const float barWidth = (count * SlotSize + (count - 3) * SlotGap + 2 * GroupGap) * S;
    const float vitalsRight = vx + HealthWidth * S + 16 * S;
    const float sx = std::max(vitalsRight, (W - barWidth) * .5f), sy = H - (Margin + BelowSlots) * S - size;
    static float ready[8] = {}, lastCooldown[8] = {};
    float x = sx;
    for (int i = 0; i < count; ++i)
    {
        const auto& slot = slots[i];
        if (i > 0) x += (slot.Group != slots[i - 1].Group ? GroupGap : SlotGap) * S;
        const bool cooling = slot.Cooldown > 0;
        if (lastCooldown[i] > 0 && !cooling) ready[i] = ReadyFlash;
        lastCooldown[i] = slot.Cooldown;
        ready[i] = std::max(0.f, ready[i] - ui.Delta);

        if (slot.Glow) ui.Sprite(HudArt::SlotGlow, x, sy, size, size, GoldLight.Fade(.55f + .35f * pulse));
        else if (slot.Active) ui.Sprite(HudArt::SlotGlow, x, sy, size, size, slot.Accent.Fade(.9f));
        if (ready[i] > 0) ui.Sprite(HudArt::SlotGlow, x, sy, size, size, White.Fade(ready[i] / ReadyFlash));
        ui.Sprite(HudArt::Slot, x, sy, size, size);
        const float grow = slot.Active ? 1.08f : 1.f;
        ui.Icon(*slot.Art, x + size * .5f, sy + size * .47f, IconSize * S * grow, cooling ? Faint : slot.Active || slot.Glow ? White : slot.Accent);
        if (cooling)
        {   // the well darkens from the top and clears as the skill comes back
            const float rim = 5 * S, f = std::clamp(slot.Cooldown / slot.Max, 0.f, 1.f);
            ui.Rect(x + rim, sy + rim, size - 2 * rim, (size - 2 * rim) * f, Black.Fade(.62f));
            std::wostringstream seconds; seconds << std::fixed << std::setprecision(slot.Cooldown < 1 ? 1 : 0) << slot.Cooldown;
            ui.Centered(seconds.str(), x + size * .5f, sy + (size - ui.Height(3.f * S)) * .5f - 2 * S, 3.f * S, Ink);
        }
        ui.Key(slot.Key, x + size * .5f, sy + size - KeyHeight * S * .45f, KeyHeight * S, slot.Glow ? GoldLight : White);
        ui.Centered(slot.Name, x + size * .5f, sy + size + KeyHeight * S * .62f, 1.4f * S, cooling ? Faint : Dim);
        x += size;
    }

    // ---------------------------------------------------------------- combo prompt and move name
    float noticeY = sy - 36 * S;
    if (hud.ComboOpen)
    {
        const float key = 20 * S, pad = 12 * S, row = 26 * S;
        const float keyColumn = std::max(ui.KeyWidth(L"LMB", key), ui.KeyWidth(L"RMB", key));
        const float pw = keyColumn + 10 * S + std::max(ui.Width(hud.ComboLeft, TextSmall * S), ui.Width(hud.ComboRight, TextSmall * S)) + 2 * pad;
        const float ph = 2 * row + 2 * pad + 6 * S, px = sx, py = sy - ph - 16 * S;
        ui.Nine(HudArt::Panel, px, py, pw, ph);
        ui.Spaced(L"COMBO", px + 2 * S, py - ui.Height(TextTiny * S) - 5 * S, TextTiny * S, 3 * S, GoldLight);
        const wchar_t* keys[2] = {Ui::KeyLabel(L"LMB", VK_LBUTTON), Ui::KeyLabel(L"RMB", VK_RBUTTON)}; const wchar_t* names[2] = {hud.ComboLeft, hud.ComboRight};
        for (int i = 0; i < 2; ++i)
        {
            const float ry = py + pad + static_cast<float>(i) * row;
            ui.Key(keys[i], px + pad + keyColumn * .5f, ry, key, GoldLight);
            ui.Text(names[i], px + pad + keyColumn + 10 * S, ry + (key * .86f - ui.Height(TextSmall * S)) * .5f, TextSmall * S, Ink);
        }
        const float timer = std::clamp(hud.ComboWindow, 0.f, 1.f);                    // time left to follow up
        ui.Rect(px + pad, py + ph - pad * .5f - 3 * S, (pw - 2 * pad), 3 * S, Well);
        ui.Rect(px + pad, py + ph - pad * .5f - 3 * S, (pw - 2 * pad) * timer, 3 * S, GoldLight);
        noticeY = std::min(noticeY, py - 34 * S);
    }
    if (hud.NoticeTimer > 0 && hud.Notice && hud.Notice[0])
    {
        const float fade = std::clamp(hud.NoticeTimer / .25f, 0.f, 1.f), cx = sx + barWidth * .5f;
        ui.SpacedCentered(hud.Notice, cx, noticeY, TextSmall * S, 3 * S, Ink.Fade(fade));
        ui.Sprite(HudArt::Divider, cx - 120 * S, noticeY + ui.Height(TextSmall * S) + 1 * S, 240 * S, 15 * S, White.Fade(.8f * fade));
    }

    // ---------------------------------------------------------------- enemy health bars
    for (int i = 0; i < hud.EnemyCount; ++i)
    {
        const auto& e = hud.Enemies[i];
        if (!e.Visible) continue;
        const float w = 78 * S, h = 9 * S;
        ui.Bar(e.X * W - w * .5f, e.Y * H - h, w, h, e.Health, e.Flash > 0 ? White : Red, 0, true);
    }

    if (context.ShowPerformance && context.Performance.Valid)
    {
        std::wostringstream fps; fps << std::fixed << std::setprecision(0) << context.Performance.Fps << L" FPS";
        ui.RightAligned(fps.str(), W - (Margin + 172) * S, 10 * S, TextTiny * S, Dim); // left of the minimap
    }
}
