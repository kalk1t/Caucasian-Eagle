#include "NyxaraHud.h"
#include "HudTextRenderer.h"
#include "DoorwaysHudTextLayout.h"
#include "DoorwaysRenderContexts.h"
#include "DoorwaysUiTheme.h"
#include "Renderer.h"
#include "WorldState.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <iomanip>
#include <sstream>
#include <string>

// Caucasian Eagle gameplay HUD: vitals bars, an eight-slot skill bar with cooldown sweeps and key badges,
// a combo prompt, shade health bars and a centred action banner. Presentation only: it reads
// the NyxaraHud() snapshot and the combat vitals text; it owns no gameplay state.
void Doorways::DrawNyxaraHud(const DoorwaysHudRenderContext& context, HudTextRenderer& mTextRenderer)
{
    using namespace Doorways;
    const auto& hud = NyxaraHud();
    const auto& world = context.World;
    const int cw = context.ClientWidth, ch = context.ClientHeight;
    const float W = static_cast<float>(cw), H = static_cast<float>(ch);
    const float s = CalculateDoorwaysUiScale({cw, ch, context.Dpi, context.Interface.UiScale});

    const auto mat = [&](float r, float g, float b, float a = 1.f) -> Material
    { return {{r, g, b, a}, {1, 1, 0, 0}, context.Renderer.mWhiteTextureView.Get(), {0, 0, 0, 0}, 1, 1}; };
    const Material panel = mat(.035f, .070f, .062f, .86f), panelEdge = mat(.16f, .30f, .26f), ink = mat(.90f, .97f, .95f),
        dim = mat(.52f, .64f, .62f), silver = mat(.80f, .90f, 1.f), soul = mat(.22f, 1.f, .52f), gold = mat(.85f, .73f, .45f),
        dark = mat(.012f, .028f, .024f, .92f), shadow = mat(0, 0, 0, .85f), red = mat(.95f, .30f, .32f), violet = mat(.62f, .28f, 1.f),
        moon = mat(.55f, .82f, 1.f), white = mat(1, 1, 1);
    const auto rect = [&](float x, float y, float w, float h, const Material& m)
    { if (w > 0 && h > 0) context.Renderer.DrawScreenRect(x, y, w, h, cw, ch, m); };
    const auto frame = [&](float x, float y, float w, float h, const Material& fill, const Material& edge, float t)
    { rect(x - t, y - t, w + 2 * t, h + 2 * t, edge); rect(x, y, w, h, fill); };
    const auto text = [&](const wchar_t* value, float x, float y, float pixels, const Material& m)
    { mTextRenderer.DrawTextWithShadow(context.Renderer, value, x, y, pixels, cw, ch, m, shadow); };
    const auto width = [&](const wchar_t* value, float pixels) { return MeasureDoorwaysHudTextWidth(value, pixels); };
    const auto centered = [&](const wchar_t* value, float cx, float y, float pixels, const Material& m)
    { text(value, cx - width(value, pixels) * .5f, y, pixels, m); };
    const float tiny = 2.f * s, body = 2.5f * s, line = DoorwaysHudTextHeight(body);

    // ---------------------------------------------------------------- vitals (bottom left)
    int hp = 100, en = 100;
    swscanf_s(world.CombatFeedback.Vitals.c_str(), L"HP %d EN %d", &hp, &en);
    const float hpFraction = std::clamp(hp / 100.f, 0.f, 1.f), enFraction = std::clamp(en / 100.f, 0.f, 1.f);
    const float m = 20 * s, vw = 330 * s, vh = 104 * s, vx = m, vy = H - m - vh, pad = 10 * s;
    frame(vx, vy, vw, vh, panel, panelEdge, 1.5f * s);
    rect(vx, vy, 4 * s, vh, soul);
    text(L"NYXARA", vx + pad + 4 * s, vy + pad * .7f, body, silver);
    const std::wstring swords = hud.Swapped ? L"R TALONSHADE   L MOONFEATHER" : L"R MOONFEATHER   L TALONSHADE";
    const float bx = vx + pad + 4 * s, bw = vw - 2 * pad - 4 * s, by = vy + pad + line + 6 * s, bh = 18 * s;
    rect(bx, by, bw, bh, dark);
    rect(bx, by, bw * hpFraction, bh, hpFraction < .3f ? red : soul);
    rect(bx, by, bw * hpFraction, 3 * s, mat(1, 1, 1, .35f));
    const std::wstring hpText = std::to_wstring(hp) + L" / 100";
    centered(hpText.c_str(), bx + bw * .5f, by + (bh - DoorwaysHudTextHeight(tiny)) * .5f, tiny, ink);
    const float ey = by + bh + 6 * s, eh = 9 * s;
    rect(bx, ey, bw, eh, dark); rect(bx, ey, bw * enFraction, eh, moon);
    text(swords.c_str(), bx, ey + eh + 6 * s, tiny, dim);

    // ---------------------------------------------------------------- skill bar (bottom centre)
    struct Slot { const wchar_t* Key; const wchar_t* Name; float Cooldown, Max; bool Active, Glow; const Material* Accent; };
    const Slot slots[] = {
        {L"LMB", L"SLASH", 0, 1, hud.Slashing, hud.ComboOpen, &silver}, {L"RMB", L"HEAVY", 0, 1, hud.Heavy, hud.ComboOpen, &silver},
        {L"F", L"DASH", hud.Cooldown[0], hud.CooldownMax[0], false, false, &soul},
        {L"V", L"STORM", hud.Cooldown[1], hud.CooldownMax[1], false, false, &soul},
        {L"C", L"LEAP", hud.Cooldown[2], hud.CooldownMax[2], false, false, &soul},
        {L"Q", L"SWAP", 0, 1, hud.Swapping, false, &moon}, {L"E", L"GUARD", 0, 1, hud.Guarding, false, &moon},
        {L"SHIFT", L"BOOST", 0, 1, hud.Boosting, false, &moon}};
    const int count = static_cast<int>(std::size(slots));
    const float size = 80 * s, gap = 8 * s, barWidth = count * size + (count - 1) * gap;
    const float sx = (std::max)(vx + vw + 16 * s, (W - barWidth) * .5f), sy = H - m - size;
    for (int i = 0; i < count; ++i)
    {
        const auto& slot = slots[i];
        const float x = sx + i * (size + gap);
        const bool cooling = slot.Cooldown > 0;
        frame(x, sy, size, size, panel, slot.Glow ? gold : slot.Active ? *slot.Accent : panelEdge, (slot.Glow || slot.Active ? 3.f : 1.5f) * s);
        rect(x, sy + size - 4 * s, size, 4 * s, cooling ? dim : *slot.Accent);
        if (cooling)
        {   // cooldown sweep drains downwards
            const float f = std::clamp(slot.Cooldown / slot.Max, 0.f, 1.f);
            rect(x, sy, size, (size - 4 * s) * f, dark);
            std::wostringstream seconds; seconds << std::fixed << std::setprecision(slot.Cooldown < 1 ? 1 : 0) << slot.Cooldown;
            centered(seconds.str().c_str(), x + size * .5f, sy + size * .34f, 3.5f * s, ink);
        }
        const float keyWidth = width(slot.Key, tiny) + 6 * s;
        rect(x + 4 * s, sy + 4 * s, keyWidth, DoorwaysHudTextHeight(tiny) + 4 * s, slot.Glow ? gold : dark);
        text(slot.Key, x + 7 * s, sy + 6 * s, tiny, slot.Glow ? dark : gold);
        centered(slot.Name, x + size * .5f, sy + size - 8 * s - DoorwaysHudTextHeight(tiny), tiny, cooling ? dim : ink);
    }
    if (hud.ComboOpen)
    {   // combo prompt above the mouse slots
        const std::wstring left = std::wstring(L"LMB  ") + hud.ComboLeft, right = std::wstring(L"RMB  ") + hud.ComboRight;
        const float pw = (std::max)(width(left.c_str(), body), width(right.c_str(), body)) + 2 * pad, ph = 2 * line + 3 * pad;
        const float px = sx, py = sy - ph - 10 * s;
        frame(px, py, pw, ph, panel, gold, 2 * s);
        text(L"COMBO", px + pad, py - DoorwaysHudTextHeight(tiny) - 4 * s, tiny, gold);
        text(left.c_str(), px + pad, py + pad, body, ink);
        text(right.c_str(), px + pad, py + pad + line + pad * .6f, body, ink);
        rect(px, py + ph - 3 * s, pw * std::clamp(hud.ComboWindow, 0.f, 1.f), 3 * s, gold);
    }

    // ---------------------------------------------------------------- shade health bars
    for (int i = 0; i < hud.EnemyCount; ++i)
    {
        const auto& e = hud.Enemies[i];
        if (!e.Visible) continue;
        const float w = 78 * s, h = 7 * s, x = e.X * W - w * .5f, y = e.Y * H - h;
        frame(x, y, w, h, dark, shadow, 1.5f * s);
        rect(x, y, w * std::clamp(e.Health, 0.f, 1.f), h, e.Flash > 0 ? white : violet);
    }

    // ---------------------------------------------------------------- action banner (top centre)
    if (hud.NoticeTimer > 0 && hud.Notice && hud.Notice[0])
    {
        const float pixels = 4.5f * s, tw = width(hud.Notice, pixels), th = DoorwaysHudTextHeight(pixels);
        const float bxc = W * .5f, byc = H * .13f;
        rect(bxc - tw * .5f - 22 * s, byc + th + 6 * s, tw + 44 * s, 2 * s, soul);
        centered(hud.Notice, bxc, byc, pixels, ink);
    }

    // ---------------------------------------------------------------- performance / hints
    if (context.ShowPerformance && context.Performance.Valid)
    {
        std::wostringstream fps; fps << std::fixed << std::setprecision(0) << context.Performance.Fps << L" FPS";
        text(fps.str().c_str(), W - m - width(fps.str().c_str(), tiny), m, tiny, dim);
    }
    const wchar_t* hint = L"WASD FLY   SPACE JUMP / HOLD RISE   SKILL THEN LMB / RMB = COMBO   I INVENTORY";
    if (context.Interface.ShowInteractionPrompts)
        text(hint, m, m, tiny, dim);
}
