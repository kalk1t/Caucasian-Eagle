#include "AscentGrowth.h"

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
        // without her bundle's looks: the examples' titles in the owner's order (1-3-2)
        const wchar_t* const Groups[3] = {L"THE HIGHLANDS", L"AMIRANI'S FIRE", L"MOONLIT"};

        std::wstring Times(float scale)     // 1.5 -> "x 1.50"
        {
            wchar_t buffer[32];
            std::swprintf(buffer, 32, L"x %.2f", scale);
            return buffer;
        }
        float SwordScale(int level) { return 1 + AscentSwordLevelDamage * static_cast<float>(level); }
        float ArmourScale(int level) { return 1 - AscentArmourLevelTaken * static_cast<float>(level); }
        std::wstring Seconds(int level) { return std::to_wstring(static_cast<int>(std::lround(FlightSeconds(level)))); }

        // what an item at that level does, in words: as she has it, and what its next level gives
        std::wstring Has(int item, int level)
        {
            if (item == Growth::Wings) return L"She flies " + Seconds(level) + L" seconds on them.";
            if (item == Growth::Swords) return level ? L"Her blows deal damage " + Times(SwordScale(level)) + L"." : std::wstring(L"Her blows as she set out.");
            return level ? L"The blows she takes hurt " + Times(ArmourScale(level)) + L"." : std::wstring(L"The blows she takes, as she set out.");
        }
        std::wstring Gives(int item, int level)
        {
            if (item == Growth::Wings) return L"she flies " + Seconds(level + 1) + L" seconds, not " + Seconds(level);
            if (item == Growth::Swords) return L"damage " + Times(SwordScale(level + 1)) + L", not " + Times(SwordScale(level));
            return L"damage taken " + Times(ArmourScale(level + 1)) + L", not " + Times(ArmourScale(level));
        }
    }

    int Cost(const Growth::State& s, int item, const std::vector<int>& prices)
    {
        if (item < 0 || item >= ItemCount) return 0;
        const int level = s.Level[item];
        if (level >= AscentMaxLevel || level >= static_cast<int>(prices.size())) return 0;
        return (std::max)(0, prices[static_cast<size_t>(level)]);
    }

    int Form(const Growth::State& s)
    {
        return std::clamp((std::min)({s.Level[Growth::Wings], s.Level[Growth::Swords], s.Level[Growth::Armour]}) / 3, 0, 3);
    }

    float Damage(const Growth::State& s)
    {
        return SwordScale(s.Level[Growth::Swords]) * (1 + FormDamage * static_cast<float>(AscentGrowth::Form(s)));
    }

    float DamageTaken(const Growth::State& s)
    {
        return ArmourScale(s.Level[Growth::Armour]) * (1 - FormTaken * static_cast<float>(AscentGrowth::Form(s)));
    }

    float FlightSeconds(int wingLevel) { return AscentLevelFlightSeconds[std::clamp(wingLevel, 0, AscentMaxLevel)]; }

    std::wstring LookName(int item, int level)
    {
        item = std::clamp(item, 0, ItemCount - 1);
        if (level <= 0) return Own[item];
        if (level <= NyxaraLooks::LevelCount()) return NyxaraLooks::LevelTier(item, level).Name;
        std::wstring name = Titles[item];
        std::transform(name.begin() + 1, name.end(), name.begin() + 1, [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return name + L" of level " + std::to_wstring(level);
    }

    std::wstring LookTitle(int level)
    {
        if (level <= 0) return L"AS SHE SET OUT";
        if (level <= NyxaraLooks::LevelCount()) return NyxaraLooks::LevelExample(level).Title;
        return Groups[std::clamp((level - 1) / 3, 0, 2)];
    }

    Tint LookColour(int level)
    {
        if (level <= 0) return {.58f, .68f, .67f};
        int example = -1, tier = 0;
        if (!NyxaraLooks::LevelLook(level, example, tier))
        {   // (no looks in her bundle: the owner's order, 1-3-2)
            const int group = std::clamp((level - 1) / 3, 0, 2);
            example = group == 0 ? 0 : group == 1 ? 2 : 1;
        }
        switch (example)
        {
        case 0: return {1.f, .84f, .48f};    // THE HIGHLANDS: gold
        case 1: return {.58f, .80f, 1.f};    // MOONLIT: moonlight
        case 2: return {1.f, .56f, .30f};    // AMIRANI'S FIRE: embers
        default: return {.66f, .36f, 1.f};
        }
    }

    void Clamp(Growth::State& s)
    {
        s.Crystals = (std::max)(0, s.Crystals);
        s.Potions = std::clamp(s.Potions, 0, MaxPotions);
        for (int i = 0; i < ItemCount; ++i)
        {
            s.Level[i] = std::clamp(s.Level[i], 0, AscentMaxLevel);
            s.Look[i] = std::clamp(s.Look[i], 0, s.Level[i]);
        }
        s.Level[Growth::Power] = 0; s.Level[Growth::Defence] = 0;
    }

    // Two panels, left and right, so that Nyxara stays in view between them (as the road's): on the left her three
    // things, a card each with its ten marks (her own look, then the nine levels in threes, each in its look's colour),
    // the look she wears, what it does and what the next level costs and gives, and its LEVEL UP; on the right her
    // crystals, what the nest is for, what she carries, her looks' three examples and what it all adds up to.
    void Draw(Ui& ui, const View& v)
    {
        if (v.Fade <= 0) return;
        using namespace Palette;
        const float S = ui.S, W = ui.W, a = v.Fade;
        constexpr float Margin = 22, Top = 176, LeftWidth = 404, RightWidth = 336, LeftHeight = 560, RightHeight = 494, Pad = 14;
        constexpr float Tiny = 1.55f, Small = 1.9f, Name = 2.4f, CardHeight = 152, CardGap = 6, IconBox = 46;
        const Growth::State& s = v.Now;
        const int form = AscentGrowth::Form(s);
        const Color formColour[4] = {Dim, Moon, Soul, GoldLight};
        const auto colour = [](int level) { const Tint t = LookColour(level); return Color{t.R, t.G, t.B, 1}; };
        const auto frame = [&](float x, float y, float w, float h, float line, Color c)
        {
            ui.Rect(x, y, w, line, c); ui.Rect(x, y + h - line, w, line, c);
            ui.Rect(x, y, line, h, c); ui.Rect(x + w - line, y, line, h, c);
        };
        const HudArt::Sprite* const icons[ItemCount] = {&HudArt::IconWings, &HudArt::IconSwords, &HudArt::IconArmour};

        ui.Sprite(HudArt::GradH, 0, 0, 480 * S, ui.H, Black.Fade(.55f * a));                      // shade behind each panel,
        ui.Sprite(HudArt::GradH, W - 420 * S, 0, 420 * S, ui.H, Black.Fade(.55f * a), 3.14159265f); // none over her

        const auto card = [&](int item, float x, float y, float w, float h)
        {
            const bool selected = item == v.Selected;
            const int level = s.Level[item], look = s.Look[item], cost = AscentGrowth::Cost(s, item, v.Prices);
            const bool can = cost > 0 && s.Crystals >= cost && v.AtNest;
            const float flash = item == v.FlashRow ? v.Flash : 0;
            Pointer::Add(Pointer::GrowthCard, item, x, y, w, h);
            ui.Rect(x, y, w, h, Well.Fade(a * (selected ? .92f : .62f)));
            frame(x, y, w, h, (selected ? 2 : 1) * S, selected ? GoldLight.Fade(a * (.75f + .25f * std::sin(ui.Time * 4))) : Gold.Fade(a * .28f));
            if (flash > 0) ui.Rect(x, y, w, h, GoldLight.Fade(a * flash * .35f));
            ui.Icon(*icons[item], x + (8 + IconBox * .5f) * S, y + 36 * S, IconBox * S * (1 + .25f * flash), (level ? colour(level) : Dim).Fade(a));
            const float tx = x + (IconBox + 16) * S, right = x + w - 12 * S;
            float ty = y + 9 * S;
            ui.Spaced(Titles[item], tx, ty, Tiny * S, 2.5f * S, Gold.Fade(a));
            // LEVEL UP, at the top on the right (lit only in a nest, with crystals enough), and why not beside it
            const bool enough = cost > 0 && s.Crystals >= cost;
            if (cost > 0)
            {
                const float bw = 104 * S, bh = 22 * S, bx = right - bw, by = y + 6 * S;
                ui.Rect(bx, by, bw, bh, (can ? Gold : Well).Fade(a * (can ? .25f : .6f)));
                frame(bx, by, bw, bh, 1 * S, (can ? GoldLight : Faint).Fade(a));
                ui.Centered(L"LEVEL UP", bx + bw * .5f, by + (bh - ui.Height(Tiny * S)) * .5f, Tiny * S, (can ? GoldLight : Faint).Fade(a));
                Pointer::Add(Pointer::LevelKey, item, bx, by, bw, bh);
                const std::wstring why = !v.AtNest ? std::wstring(L"in a nest only") : !enough ? std::to_wstring(cost - s.Crystals) + L" crystals more" : std::wstring();
                if (!why.empty()) ui.RightAligned(why, bx - 8 * S, by + (bh - ui.Height(Tiny * S)) * .5f, Tiny * S, Dim.Fade(a));
            }
            ty += ui.Height(Tiny * S) + 10 * S;
            // the marks: her own look, then the nine levels in threes; lit up to her level, the worn one framed. In a
            // nest a lit one can be clicked: she puts its look on. Her level beside them.
            const float mark = 15 * S, gap = 4 * S, between = 11 * S;
            float mx = tx;
            for (int l = 0; l <= AscentMaxLevel; ++l)
            {
                if (l > 0 && (l - 1) % 3 == 0) mx += between - gap;
                const bool have = l <= level, wearing = l == look;
                const float cx = mx + mark * .5f, cy = ty + mark * .5f;
                const Color c = have ? (l == 0 ? Ink : colour(l)) : Faint;
                if (wearing) frame(cx - mark * .66f, cy - mark * .66f, mark * 1.32f, mark * 1.32f, 1.5f * S, c.Fade(a * (.7f + .3f * std::sin(ui.Time * 4))));
                ui.Icon(l == 0 ? HudArt::Pip : HudArt::Diamond, cx, cy, mark * (wearing ? 1.f : .8f), c.Fade(a * (have ? 1.f : .6f)));
                if (have && v.AtNest) Pointer::Add(Pointer::LookPip, item * 16 + l, cx - mark * .62f, cy - mark * .62f, mark * 1.24f, mark * 1.24f);
                mx += mark + gap;
            }
            ui.RightAligned(L"LEVEL " + std::to_wstring(level) + L" OF " + std::to_wstring(AscentMaxLevel), right, ty + (mark - ui.Height(Tiny * S)) * .5f, Tiny * S,
                            (level ? colour(level) : Dim).Fade(a));
            ty += mark + 8 * S;
            // the look she wears, and what her level does
            ui.Text(LookName(item, look), tx, ty, Small * S, (look ? colour(look) : Ink).Fade(a));
            if (look) ui.RightAligned(LookTitle(look), right, ty + 3 * S, Tiny * S, Dim.Fade(a));
            ty += ui.Height(Small * S) + 2 * S;
            ui.Text(Has(item, level), tx, ty, Tiny * S, Dim.Fade(a));
            ty += ui.Height(Tiny * S) + 7 * S;
            // the next level: its price and its look, what it gives
            if (cost == 0)
            {
                ui.Text(level >= AscentMaxLevel ? L"As strong as it can be." : L"", tx, ty, Tiny * S, GoldLight.Fade(a));
                return y + h + CardGap * S;
            }
            const float icon = 15 * S;
            ui.Icon(HudArt::IconCrystal, tx + icon * .5f, ty + ui.Height(Tiny * S) * .5f, icon, (enough ? Violet : Faint).Fade(a));
            const std::wstring price = std::to_wstring(cost) + L":  ";
            ui.Text(price, tx + icon + 4 * S, ty, Tiny * S, (enough ? GoldLight : Faint).Fade(a));
            const float px = tx + icon + 4 * S + ui.Width(price, Tiny * S);
            ui.Text(L"level " + std::to_wstring(level + 1) + L", " + LookName(item, level + 1), px, ty, Tiny * S, (enough ? colour(level + 1) : Faint).Fade(a));
            ty += ui.Height(Tiny * S) + 2 * S;
            ui.Text(Gives(item, level), px, ty, Tiny * S, (enough ? Ink : Faint).Fade(a));
            return y + h + CardGap * S;
        };

        // ---- left: who she is, her three things
        {
            const float x = Margin * S, y = Top * S, w = LeftWidth * S;
            ui.Nine(HudArt::Panel, x, y, w, LeftHeight * S, White.Fade(a));
            ui.Spaced(L"NYXARA", x + Pad * S, y + 11 * S, Tiny * S, 3 * S, Gold.Fade(a));
            ui.Text(Growth::FormName(form), x + Pad * S, y + 23 * S, Name * S, formColour[form].Fade(a));
            for (int i = 0; i < 3; ++i)
                ui.Icon(HudArt::IconWings, x + w - Pad * S - (static_cast<float>(2 - i) + .5f) * 26 * S, y + 24 * S, 24 * S, (i < form ? GoldLight : Faint).Fade(a));
            const std::wstring rule = form >= 3 ? std::wstring(L"She has become all she can be.")
                                                : L"All three at level " + std::to_wstring(3 * (form + 1)) + L": she becomes " + Growth::FormName(form + 1) + L".";
            ui.Text(rule, x + Pad * S, y + 52 * S, Tiny * S, Dim.Fade(a));
            float cy = y + 74 * S;
            for (int item = 0; item < ItemCount; ++item) cy = card(item, x + 8 * S, cy, w - 16 * S, CardHeight * S);
        }

        // ---- right: crystals, the nest, what she carries, her looks, all together
        {
            const float w = RightWidth * S, x = W - Margin * S - w, y = Top * S, right = x + w - Pad * S;
            ui.Nine(HudArt::Panel, x, y, w, RightHeight * S, White.Fade(a));
            ui.Spaced(L"CRYSTALS", x + Pad * S, y + 11 * S, Tiny * S, 3 * S, Gold.Fade(a));
            ui.Icon(HudArt::IconCrystal, x + Pad * S + 13 * S, y + 40 * S, 26 * S, Violet.Fade(a));
            ui.Text(std::to_wstring(s.Crystals), x + Pad * S + 32 * S, y + 26 * S, Name * S, Ink.Fade(a));
            const float step = ui.Height(Tiny * S) + 2 * S;
            ui.RightAligned(L"From creatures", right, y + 25 * S, Tiny * S, Dim.Fade(a));
            ui.RightAligned(L"and clay jars.", right, y + 25 * S + step, Tiny * S, Dim.Fade(a));
            const float line = ui.Height(Tiny * S) + 6 * S;
            float cy = y + 64 * S;

            // the nest: levels are bought there, and looks put on
            int all = 0; bool top = true;
            for (int item = 0; item < ItemCount; ++item) { all += AscentGrowth::Cost(s, item, v.Prices); top = top && AscentGrowth::Cost(s, item, v.Prices) == 0; }
            ui.Spaced(v.AtNest ? L"IN THIS EAGLE NEST" : L"AT AN EAGLE NEST", x + Pad * S, cy, Tiny * S, 3 * S, (v.AtNest ? GoldLight : Gold).Fade(a));
            cy += ui.Height(Tiny * S) + 6 * S;
            cy = ui.Paragraph(v.AtNest ? L"She levels up here, and puts on any look she has reached. What she buys is saved at once."
                                       : L"Levels are bought, and looks put on, only standing in an Eagle Nest.",
                              x + Pad * S, cy, Tiny * S, w - 2 * Pad * S, 1 * S, (v.AtNest ? Ink : Dim).Fade(a)) + 6 * S;
            if (top) ui.Text(L"All three are at their last level.", x + Pad * S, cy, Tiny * S, GoldLight.Fade(a));
            else
            {
                ui.Text(L"The next level of all three", x + Pad * S, cy, Tiny * S, Dim.Fade(a));
                ui.RightAligned(std::to_wstring((std::min)(s.Crystals, all)) + L" of " + std::to_wstring(all), right, cy, Tiny * S, (s.Crystals >= all ? GoldLight : Ink).Fade(a));
                cy += line - 2 * S;
                ui.Bar(x + Pad * S, cy, w - 2 * Pad * S, 6 * S, all > 0 ? (std::min)(1.f, static_cast<float>(s.Crystals) / static_cast<float>(all)) : 1.f, Violet, 0, true);
            }
            cy += line + 4 * S;

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
            carried(HudArt::IconWings, Moon, L"Eagle Nests", L"H saves there");

            cy += 4 * S;
            ui.Spaced(L"HER LOOKS", x + Pad * S, cy, Tiny * S, 3 * S, Gold.Fade(a));
            cy += ui.Height(Tiny * S) + 6 * S;
            for (int g = 0; g < 3; ++g)
            {
                const int first = 3 * g + 1;
                ui.Icon(HudArt::Diamond, x + Pad * S + icon * .5f, cy + ui.Height(Tiny * S) * .5f, icon * .8f, colour(first).Fade(a));
                ui.Text(L"levels " + std::to_wstring(first) + L" to " + std::to_wstring(first + 2), x + Pad * S + icon + 6 * S, cy, Tiny * S, Dim.Fade(a));
                ui.RightAligned(LookTitle(first), right, cy, Tiny * S, colour(first).Fade(a));
                cy += line;
            }

            cy += 4 * S;
            ui.Spaced(L"ALL TOGETHER", x + Pad * S, cy, Tiny * S, 3 * S, Gold.Fade(a));
            cy += ui.Height(Tiny * S) + 6 * S;
            const auto total = [&](const std::wstring& what, const std::wstring& value)
            {
                ui.Text(what, x + Pad * S, cy, Tiny * S, Dim.Fade(a));
                ui.RightAligned(value, right, cy, Tiny * S, Ink.Fade(a));
                cy += line;
            };
            total(L"Damage she deals", Times(AscentGrowth::Damage(s)));
            total(L"Damage she takes", Times(AscentGrowth::DamageTaken(s)));
            total(L"Her flight", Seconds(s.Level[Growth::Wings]) + L" seconds");
        }

        // ---- keys, and under them what the mouse does (the pointer shows while this window is open)
        {
            const float y = (Top - 40) * S, h = 24 * S;
            struct Entry { const wchar_t* Label; const wchar_t* What; int Zone; float Pull; bool Lit; };
            const Entry keys[] = {{L"W", L"", -1, 20, true},
                                  {L"S", L"CHOOSE", -1, 0, true},
                                  {L"A", L"", -1, 20, v.AtNest},
                                  {L"D", L"HER LOOK", -1, 0, v.AtNest},
                                  {Ui::KeyLabel(L"ENTER", VK_RETURN), L"LEVEL UP", -1, 0, v.AtNest},
                                  {Ui::KeyLabel(L"I", 'I'), L"CLOSE", Pointer::CloseKey, 0, true}};
            constexpr int count = static_cast<int>(sizeof keys / sizeof keys[0]);
            const auto width = [&](const Entry& e) { return ui.KeyWidth(e.Label, h) + 6 * S + ui.Width(e.What, Tiny * S) + 22 * S - e.Pull * S; };
            float total = -22 * S;
            for (const auto& e : keys) total += width(e);
            const wchar_t* mouse = v.AtNest ? L"or the mouse: LEVEL UP buys a level, a lit mark puts its look on her, CLOSE leaves"
                                            : L"Levels are bought and looks put on in an Eagle Nest. The mouse: CLOSE leaves";
            const float hint = y + h + 5 * S, band = (std::max)(total, ui.Width(mouse, Tiny * S)) + 32 * S;
            ui.Rect(W * .5f - band * .5f, y - 8 * S, band, hint + ui.Height(Tiny * S) + 8 * S - (y - 8 * S), Black.Fade(.5f * a));
            float x = W * .5f - total * .5f;
            for (int i = 0; i < count; ++i)
            {
                const Entry& e = keys[i];
                const float from = x, kw = ui.KeyWidth(e.Label, h);
                ui.Key(e.Label, x + kw * .5f, y, h, (e.Lit ? White : Faint).Fade(a));
                ui.Text(e.What, x + kw + 6 * S, y + (h * .86f - ui.Height(Tiny * S)) * .5f, Tiny * S, (e.Lit ? Ink : Faint).Fade(a));
                x += width(e);
                if (e.Zone >= 0) Pointer::Add(e.Zone, 0, from, y, x - from - 22 * S, h);
            }
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
