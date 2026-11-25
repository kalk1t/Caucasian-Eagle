#include "AscentGrowth.h"

#include "AscentQuests.h"
#include "GameConstants.h"
#include "GameUi.h"
#include "NyxaraLooks.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <string>

namespace CaucasianEagle::AscentGrowth
{
    namespace
    {
        const wchar_t* const Titles[ItemCount] = {L"WINGS", L"SWORDS", L"ARMOUR"};
        const wchar_t* const Own[ItemCount] = {L"Her own wings", L"Her own swords", L"Her own armour"};

        int Clamped(int gear) { return std::clamp(gear, 0, GearCount - 1); }

        std::wstring Times(float scale)     // 1.5 -> "x 1.50"
        {
            wchar_t buffer[32];
            std::swprintf(buffer, 32, L"x %.2f", scale);
            return buffer;
        }
        std::wstring Seconds(int gear) { return std::to_wstring(static_cast<int>(std::lround(FlightSeconds(gear)))); }
    }

    int Form(const Growth::State& s) { return Clamped(s.Gear); }
    float Damage(const Growth::State& s) { return GearDamage[Clamped(s.Gear)]; }
    float DamageTaken(const Growth::State& s) { return GearTaken[Clamped(s.Gear)]; }
    float FlightSeconds(int gear) { return GearFlightSeconds[Clamped(gear)]; }

    const wchar_t* GearTitle(int gear)
    {
        static const wchar_t* const titles[GearCount] = {L"AS SHE SET OUT", L"THE HIGHLANDS", L"AMIRANI'S FIRE", L"MOONLIT"};
        return titles[Clamped(gear)];
    }

    const wchar_t* GearEarned(int gear)
    {
        static const wchar_t* const words[GearCount] = {L"", L"Earned with her first three quests.", L"Earned with six more quests.", L"Earned with her last nine quests."};
        return words[Clamped(gear)];
    }

    void GearLook(int gear, int& example, int& tier)
    {   // Looks.txt's examples: 1 THE HIGHLANDS, 2 MOONLIT, 3 AMIRANI'S FIRE (0-based here)
        switch (Clamped(gear))
        {
        case 1: example = 0; tier = 3; break;
        case 2: example = 2; tier = 2; break;
        case 3: example = 1; tier = 3; break;
        default: example = 0; tier = 0; break;
        }
    }

    void GearStyle(int gear, int& theme, int& level)
    {   // the Character Lab's: theme 0 her blue (wind with its level), 1 ice, 2 fire; level the tier the look is of
        switch (Clamped(gear))
        {
        case 1: theme = 0; level = 3; break;
        case 2: theme = 2; level = 2; break;
        case 3: theme = 1; level = 3; break;
        default: theme = 0; level = 0; break;
        }
    }

    std::wstring GearItem(int gear, int item)
    {
        item = std::clamp(item, 0, ItemCount - 1);
        int example = 0, tier = 0;
        GearLook(gear, example, tier);
        if (tier <= 0 || example >= NyxaraLooks::Count()) return tier <= 0 ? Own[item] : std::wstring(GearTitle(gear)) + L" " + Titles[item];
        return NyxaraLooks::Get(example).Items[item][tier].Name;
    }

    Tint GearColour(int gear)
    {
        switch (Clamped(gear))
        {
        case 1: return {1.f, .84f, .48f};    // THE HIGHLANDS: gold
        case 2: return {1.f, .56f, .30f};    // AMIRANI'S FIRE: embers
        case 3: return {.58f, .80f, 1.f};    // MOONLIT: moonlight
        default: return {.58f, .68f, .67f};
        }
    }

    int CrystalKindOf(int stretch)
    {   // stretch k: on the way to nest k
        return stretch <= CrystalNests[0] ? WindCrystal : stretch <= CrystalNests[1] ? FireCrystal : IceCrystal;
    }

    const wchar_t* CrystalName(int kind)
    {
        static const wchar_t* const names[CrystalKinds] = {L"wind", L"fire", L"ice"};
        return names[std::clamp(kind, 0, CrystalKinds - 1)];
    }

    const char* CrystalId(int kind)
    {
        static const char* const ids[CrystalKinds] = {"wind", "fire", "ice"};
        return ids[std::clamp(kind, 0, CrystalKinds - 1)];
    }

    const wchar_t* CrystalFor(int kind)
    {
        static const wchar_t* const words[CrystalKinds] = {L"for the Highlands", L"for Amirani's fire", L"for Moonlit"};
        return words[std::clamp(kind, 0, CrystalKinds - 1)];
    }

    Tint CrystalColour(int kind)
    {
        switch (kind)
        {
        case WindCrystal: return {.62f, 1.f, .76f};    // jade, as wind over the grass
        case FireCrystal: return {1.f, .52f, .18f};
        default: return {.50f, .80f, 1.f};             // ice
        }
    }

    int AllCrystals(const Growth::State& s) { return s.AscentCrystals[0] + s.AscentCrystals[1] + s.AscentCrystals[2]; }

    void Clamp(Growth::State& s)
    {
        s.Crystals = (std::max)(0, s.Crystals);
        for (int& c : s.AscentCrystals) c = (std::max)(0, c);
        s.Potions = std::clamp(s.Potions, 0, MaxPotions);
        s.Gear = Clamped(s.Gear);
        for (int i = 0; i < Growth::RowCount; ++i) s.Level[i] = 0;
        for (int i = 0; i < ItemCount; ++i) s.Look[i] = 0;
        AscentQuests::Clamp(s);
    }

    // Two panels, left and right, so that Nyxara stays in view between them: on the left her gear (its name, its three
    // things, what it gives) and the three gears in their order with her quests for the next, hers lit; on the right her crystals and what
    // she carries.
    void Draw(Ui& ui, const View& v)
    {
        if (v.Fade <= 0) return;
        using namespace Palette;
        const float S = ui.S, W = ui.W, a = v.Fade;
        constexpr float Margin = 22, Top = 176, LeftWidth = 404, RightWidth = 336, LeftHeight = 462, RightHeight = 252, Pad = 14;
        constexpr float Tiny = 1.55f, Small = 1.9f, Name = 2.4f;
        const Growth::State& s = v.Now;
        const int gear = Clamped(s.Gear);
        const auto colour = [](int g) { const Tint t = GearColour(g); return Color{t.R, t.G, t.B, 1}; };
        const auto frame = [&](float x, float y, float w, float h, float line, Color c)
        {
            ui.Rect(x, y, w, line, c); ui.Rect(x, y + h - line, w, line, c);
            ui.Rect(x, y, line, h, c); ui.Rect(x + w - line, y, line, h, c);
        };
        const HudArt::Sprite* const icons[ItemCount] = {&HudArt::IconWings, &HudArt::IconSwords, &HudArt::IconArmour};

        ui.Sprite(HudArt::GradH, 0, 0, 480 * S, ui.H, Black.Fade(.55f * a));                      // shade behind each panel,
        ui.Sprite(HudArt::GradH, W - 420 * S, 0, 420 * S, ui.H, Black.Fade(.55f * a), 3.14159265f); // none over her

        // ---- left: her gear
        {
            const float x = Margin * S, y = Top * S, w = LeftWidth * S, right = x + w - Pad * S;
            ui.Nine(HudArt::Panel, x, y, w, LeftHeight * S, White.Fade(a));
            if (v.Flash > 0) ui.Rect(x, y, w, LeftHeight * S, colour(gear).Fade(a * v.Flash * .25f));
            ui.Spaced(L"NYXARA WEARS", x + Pad * S, y + 11 * S, Tiny * S, 3 * S, Gold.Fade(a));
            ui.Text(GearTitle(gear), x + Pad * S, y + 27 * S, Name * S, (gear ? colour(gear) : Ink).Fade(a));
            if (gear) ui.Text(GearEarned(gear), x + Pad * S, y + 56 * S, Tiny * S, Dim.Fade(a));
            float cy = y + 80 * S;
            const float line = ui.Height(Tiny * S) + 6 * S;
            for (int item = 0; item < ItemCount; ++item)
            {   // its three things
                const float box = 38 * S;
                ui.Rect(x + 8 * S, cy, w - 16 * S, box + 10 * S, Well.Fade(a * .62f));
                ui.Icon(*icons[item], x + 8 * S + 6 * S + box * .5f, cy + 5 * S + box * .5f, box, (gear ? colour(gear) : Dim).Fade(a));
                ui.Spaced(Titles[item], x + 8 * S + box + 18 * S, cy + 7 * S, Tiny * S, 2.5f * S, Gold.Fade(a));
                ui.Text(GearItem(gear, item), x + 8 * S + box + 18 * S, cy + 7 * S + ui.Height(Tiny * S) + 4 * S, Small * S, (gear ? colour(gear) : Ink).Fade(a));
                cy += box + 16 * S;
            }
            cy += 4 * S;
            ui.Spaced(L"WHAT IT GIVES HER", x + Pad * S, cy, Tiny * S, 3 * S, Gold.Fade(a));
            cy += ui.Height(Tiny * S) + 6 * S;
            const auto total = [&](const std::wstring& what, const std::wstring& value)
            {
                ui.Text(what, x + Pad * S, cy, Tiny * S, Dim.Fade(a));
                ui.RightAligned(value, right, cy, Tiny * S, Ink.Fade(a));
                cy += line;
            };
            total(L"Damage she deals", Times(AscentGrowth::Damage(s)));
            total(L"Damage she takes", Times(AscentGrowth::DamageTaken(s)));
            total(L"Her flight", Seconds(gear) + L" seconds");

            cy += 6 * S;
            ui.Spaced(L"HER GEARS, BY HER QUESTS (Q)", x + Pad * S, cy, Tiny * S, 3 * S, Gold.Fade(a));
            cy += ui.Height(Tiny * S) + 6 * S;
            for (int g = 1; g < GearCount; ++g)
            {   // each gear, in order: hers and those before, earned; the next, how far her quests for it are; the last
                const bool has = g <= gear, next = g == gear + 1;
                const float icon = 15 * S, cx = x + Pad * S + icon * .5f, my = cy + ui.Height(Tiny * S) * .5f;
                if (g == gear) frame(cx - icon * .66f, my - icon * .66f, icon * 1.32f, icon * 1.32f, 1.5f * S, colour(g).Fade(a * (.7f + .3f * std::sin(ui.Time * 4))));
                ui.Icon(HudArt::Diamond, cx, my, icon * .8f, (has ? colour(g) : Faint).Fade(a));
                ui.Text(GearTitle(g), x + Pad * S + icon + 8 * S, cy, Tiny * S, (has ? colour(g) : next ? Ink : Faint).Fade(a));
                const std::wstring count = std::to_wstring(AscentQuests::QuestsOf(g - 1));
                const std::wstring done = std::to_wstring(AscentQuests::DoneOf(s, g - 1)) + L" of " + count + L" quests done";
                ui.RightAligned(has ? std::wstring(L"earned") : next ? done : count + L" quests, after", right, cy, Tiny * S, (has ? Dim : next ? GoldLight : Faint).Fade(a));
                cy += line;
            }
        }

        // ---- right: crystals and what she carries
        {
            const float w = RightWidth * S, x = W - Margin * S - w, y = Top * S, right = x + w - Pad * S;
            ui.Nine(HudArt::Panel, x, y, w, RightHeight * S, White.Fade(a));
            ui.Spaced(L"CRYSTALS", x + Pad * S, y + 11 * S, Tiny * S, 3 * S, Gold.Fade(a));
            const float line = ui.Height(Tiny * S) + 6 * S;
            float cy = y + 30 * S;
            for (int kind = 0; kind < CrystalKinds; ++kind)
            {   // each kind: how many, and the gear whose quests it buys; the kind of where she is marked
                const Tint t = CrystalColour(kind);
                const Color c{t.R, t.G, t.B, 1};
                const float rowH = ui.Height(Small * S) + 6 * S, my = cy + rowH * .5f;
                if (kind == v.HereKind) ui.Rect(x + 6 * S, cy, w - 12 * S, rowH, c.Fade(a * .10f));
                ui.Icon(HudArt::IconCrystal, x + Pad * S + 10 * S, my, 20 * S, c.Fade(a));
                ui.Text(std::to_wstring(s.AscentCrystals[kind]), x + Pad * S + 26 * S, cy + 3 * S, Small * S, Ink.Fade(a));
                std::wstring name = CrystalName(kind); name[0] = static_cast<wchar_t>(std::towupper(name[0]));
                ui.Text(name, x + Pad * S + 78 * S, cy + 3 * S + (ui.Height(Small * S) - ui.Height(Tiny * S)) * .5f, Tiny * S, c.Fade(a));
                ui.RightAligned(CrystalFor(kind), right, cy + 3 * S + (ui.Height(Small * S) - ui.Height(Tiny * S)) * .5f, Tiny * S, Dim.Fade(a));
                cy += rowH + 2 * S;
            }
            cy += 4 * S;
            cy = ui.Paragraph(std::wstring(L"Here creatures and clay jars leave ") + CrystalName(v.HereKind) + L" crystals.",
                              x + Pad * S, cy, Tiny * S, w - 2 * Pad * S, 1 * S, Dim.Fade(a)) + 10 * S;

            ui.Spaced(L"SHE CARRIES", x + Pad * S, cy, Tiny * S, 3 * S, Gold.Fade(a));
            cy += ui.Height(Tiny * S) + 6 * S;
            const float icon = 17 * S;
            const auto carried = [&](const HudArt::Sprite& art, Color tint, const std::wstring& what, const std::wstring& note)
            {
                ui.Icon(art, x + Pad * S + icon * .5f, cy + ui.Height(Tiny * S) * .5f, icon, tint.Fade(a));
                ui.Text(what, x + Pad * S + icon + 6 * S, cy, Tiny * S, Ink.Fade(a));
                if (!note.empty() && right - ui.Width(note, Tiny * S) < x + Pad * S + icon + 6 * S + ui.Width(what, Tiny * S) + 12 * S)
                    cy += line - 4 * S;   // no room beside it: under it
                ui.RightAligned(note, right, cy, Tiny * S, Dim.Fade(a));
                cy += line;
            };
            carried(HudArt::IconPotion, Red, std::to_wstring(s.Potions) + L" of " + std::to_wstring(MaxPotions) + L" potions",
                    std::wstring(Ui::KeyLabel(L"Z", 'Z')) + L" drinks one: +" + std::to_wstring(v.PotionHeal));
            carried(HudArt::IconPotion, Faint, L"Clay jars", L"crystals, potions: break them");
            carried(HudArt::IconWings, Moon, L"Eagle Nests", v.AtNest ? L"H saves here" : L"H saves there");
        }

        // ---- keys, and under them what the mouse does (the pointer shows while this window is open)
        {
            const float y = (Top - 40) * S, h = 24 * S;
            const wchar_t* label = Ui::KeyLabel(L"I", 'I');
            const float kw = ui.KeyWidth(label, h), total = kw + 6 * S + ui.Width(L"CLOSE", Tiny * S);
            const wchar_t* mouse = L"or the mouse: CLOSE leaves";
            const float hint = y + h + 5 * S, band = (std::max)(total, ui.Width(mouse, Tiny * S)) + 32 * S;
            ui.Rect(W * .5f - band * .5f, y - 8 * S, band, hint + ui.Height(Tiny * S) + 8 * S - (y - 8 * S), Black.Fade(.5f * a));
            const float x = W * .5f - total * .5f;
            ui.Key(label, x + kw * .5f, y, h, White.Fade(a));
            ui.Text(L"CLOSE", x + kw + 6 * S, y + (h * .86f - ui.Height(Tiny * S)) * .5f, Tiny * S, Ink.Fade(a));
            Pointer::Add(Pointer::CloseKey, 0, x, y, total, h);
            ui.Centered(mouse, W * .5f, hint, Tiny * S, Dim.Fade(a));
        }

        // ---- under her (HerShowcase): what the mouse does to the view of her
        {
            const wchar_t* turn = L"DRAG: TURN ROUND HER     WHEEL: CLOSER, FURTHER";
            const float w = ui.SpacedWidth(turn, Tiny * S, 2 * S) + 28 * S, h = ui.Height(Tiny * S) + 12 * S, y = ui.H - 64 * S;
            ui.Rect(W * .5f - w * .5f, y, w, h, Black.Fade(.45f * a));
            ui.SpacedCentered(turn, W * .5f, y + 6 * S, Tiny * S, 2 * S, Ink.Fade(a * .9f));
        }
    }
}
