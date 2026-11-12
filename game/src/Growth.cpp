#include "Growth.h"

#include "GameConstants.h"
#include "GameUi.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace CaucasianEagle::Growth
{
    namespace
    {
        const wchar_t* const Forms[4] = {L"Fledgling", L"Moonwing", L"Stormwing", L"Heir of the Eagle"};
        const wchar_t* const Items[3][4] = {
            {L"Fledgling wings", L"Moonlit wings", L"Storm wings", L"Wings of the Eagle"},
            {L"Moonfeather and Talonshade", L"Tempered blades", L"Moonforged blades", L"Eagle-forged blades"},
            {L"Traveller's mail", L"Svan mail", L"Moon plate", L"Harness of Amirani"}};
        const wchar_t* const Titles[RowCount] = {L"WINGS", L"SWORDS", L"ARMOUR", L"POWER", L"DEFENCE"};

        std::wstring Percent(float share)   // 0.2 -> "20 %"
        {
            return std::to_wstring(static_cast<int>(std::lround(share * 100))) + L" %";
        }
        std::wstring Times(float scale)     // 1.5 -> "x 1.50"
        {
            wchar_t buffer[32];
            std::swprintf(buffer, 32, L"x %.2f", scale);
            return buffer;
        }
        // What the next step of a row gives, in words.
        std::wstring Promise(int row)
        {
            switch (row)
            {
            case Wings: return L"skills " + Percent(WingTierCooldown) + L" sooner, boost +" + Percent(WingTierBoost);
            case Swords: return L"blows deal " + Percent(SwordTierDamage) + L" more";
            case Armour: return L"blows taken hurt " + Percent(ArmourTierTaken) + L" less";
            case Power: return L"blows deal " + Percent(PowerStep) + L" more";
            default: return L"blows taken hurt " + Percent(DefenceStep) + L" less";
            }
        }
    }

    int Max(int row) { return row <= Armour ? MaxItemTier : MaxTrainingLevel; }

    int Cost(const State& state, int row)
    {
        const int level = state.Level[row];
        if (level >= Max(row)) return 0;
        return row <= Armour ? ItemTierCost[level] : TrainingCostBase + TrainingCostStep * level;
    }

    int Form(const State& state) { return (std::min)({state.Level[Wings], state.Level[Swords], state.Level[Armour]}); }
    const wchar_t* FormName(int form) { return Forms[std::clamp(form, 0, 3)]; }
    const wchar_t* ItemName(int row, int tier) { return Items[std::clamp(row, 0, 2)][std::clamp(tier, 0, 3)]; }

    float Damage(const State& s)
    {
        return (1 + PowerStep * static_cast<float>(s.Level[Power])) * (1 + SwordTierDamage * static_cast<float>(s.Level[Swords])) *
               (1 + FormDamage * static_cast<float>(Form(s)));
    }
    float DamageTaken(const State& s)
    {
        return (1 - DefenceStep * static_cast<float>(s.Level[Defence])) * (1 - ArmourTierTaken * static_cast<float>(s.Level[Armour])) *
               (1 - FormTaken * static_cast<float>(Form(s)));
    }
    float Cooldown(const State& s) { return 1 - WingTierCooldown * static_cast<float>(s.Level[Wings]); }
    float Boost(const State& s) { return 1 + WingTierBoost * static_cast<float>(s.Level[Wings]); }

    // Two panels, left and right, so that Nyxara stays in view between them.
    void Draw(Ui& ui, const View& v)
    {
        if (v.Fade <= 0) return;
        using namespace Palette;
        const float S = ui.S, W = ui.W, a = v.Fade;
        constexpr float Margin = 22, Top = 190, PanelWidth = 372, PanelHeight = 580, Pad = 14, Tiny = 1.55f, Small = 1.9f, Name = 2.4f;
        constexpr float ItemCard = 158, TrainCard = 88, CardGap = 6, IconBox = 54;
        const State& s = v.Now;
        const int form = Form(s);
        const Color tierColor[4] = {Dim, Moon, Soul, GoldLight};

        ui.Sprite(HudArt::GradH, 0, 0, 460 * S, ui.H, Black.Fade(.55f * a));                      // shade behind each panel,
        ui.Sprite(HudArt::GradH, W - 460 * S, 0, 460 * S, ui.H, Black.Fade(.55f * a), 3.14159265f); // none over her

        // One purchasable card. Returns the y under it.
        const auto card = [&](int row, float x, float y, float w, float h, const HudArt::Sprite& icon, const std::wstring& name, const std::wstring& has)
        {
            const bool selected = row == v.Selected;
            const int level = s.Level[row], most = Max(row), cost = Cost(s, row);
            const bool can = cost > 0 && s.Crystals >= cost;
            const float flash = row == v.FlashRow ? v.Flash : 0;
            Pointer::Add(Pointer::GrowthCard, row, x, y, w, h);
            ui.Rect(x, y, w, h, Well.Fade(a * (selected ? .92f : .62f)));
            const Color edge = selected ? GoldLight.Fade(a * (.75f + .25f * std::sin(ui.Time * 4))) : Gold.Fade(a * .28f);
            const float line = (selected ? 2 : 1) * S;
            ui.Rect(x, y, w, line, edge); ui.Rect(x, y + h - line, w, line, edge);
            ui.Rect(x, y, line, h, edge); ui.Rect(x + w - line, y, line, h, edge);
            if (flash > 0) ui.Rect(x, y, w, h, GoldLight.Fade(a * flash * .35f));
            const Color tint = row <= Armour ? tierColor[level] : (row == Power ? Red : Moon);
            ui.Icon(icon, x + (10 + IconBox * .5f) * S, y + h * .5f, IconBox * S * (1 + .25f * flash), tint.Fade(a));
            const float tx = x + (IconBox + 22) * S, right = x + w - 12 * S;
            float ty = y + 9 * S;
            ui.Spaced(Titles[row], tx, ty, Tiny * S, 2.5f * S, Gold.Fade(a));
            // steps taken: one mark per step
            const float mark = row <= Armour ? 11 * S : 8 * S;
            for (int i = 0; i < most; ++i)
                ui.Icon(HudArt::Diamond, right - (static_cast<float>(most - i) - .5f) * (mark + 2 * S), ty + ui.Height(Tiny * S) * .5f, mark,
                        (i < level ? tint : Faint).Fade(a));
            ty += ui.Height(Tiny * S) + 3 * S;
            if (row <= Armour)
            {   // every stage of the thing, in order: those behind her, the one she wears, those to come
                static const wchar_t* const numerals[4] = {L"I", L"II", L"III", L"IV"};
                const float step = ui.Height(Small * S) + 1 * S;
                for (int stage = 0; stage <= most; ++stage)
                {
                    const bool worn = stage == level, behind = stage < level;
                    const Color ink = worn ? tierColor[stage] : behind ? Dim : Faint;
                    if (worn) ui.Rect(tx - 4 * S, ty - 1 * S, right - tx + 6 * S, step, tierColor[stage].Fade(a * .14f));
                    ui.Text(numerals[stage], tx, ty + (worn ? 0 : 2 * S), (worn ? Small : Tiny) * S, ink.Fade(a));
                    ui.Text(ItemName(row, stage), tx + 24 * S, ty + (worn ? 0 : 2 * S), (worn ? Small : Tiny) * S, (worn ? Ink : ink).Fade(a));
                    if (worn) ui.RightAligned(L"WORN", right, ty + 3 * S, Tiny * S, tierColor[stage].Fade(a));
                    else if (stage == level + 1 && cost > 0) ui.RightAligned(L"NEXT", right, ty + 2 * S, Tiny * S, (can ? GoldLight : Faint).Fade(a));
                    ty += step;
                }
                ty += 3 * S;
            }
            else
            {
                ui.Text(name, tx, ty, Small * S, Ink.Fade(a));
                ty += ui.Height(Small * S) + 2 * S;
            }
            ui.Text(has, tx, ty, Tiny * S, Dim.Fade(a));
            ty += ui.Height(Tiny * S) + 5 * S;
            if (cost == 0) ui.Text(L"As strong as it can be.", tx, ty, Tiny * S, GoldLight.Fade(a));
            else
            {
                const float icon2 = 15 * S;
                ui.Icon(HudArt::IconCrystal, tx + icon2 * .5f, ty + ui.Height(Tiny * S) * .5f, icon2, (can ? Violet : Faint).Fade(a));
                const std::wstring price = std::to_wstring(cost) + L":  ";
                ui.Text(price, tx + icon2 + 4 * S, ty, Tiny * S, (can ? GoldLight : Faint).Fade(a));
                const float px = tx + icon2 + 4 * S + ui.Width(price, Tiny * S);
                ui.Paragraph(Promise(row), px, ty, Tiny * S, right - px, 1 * S, (can ? Ink : Faint).Fade(a));
            }
            return y + h + CardGap * S;
        };

        // ---- left: who she is and what she wears
        {
            const float x = Margin * S, y = Top * S, w = PanelWidth * S;
            ui.Nine(HudArt::Panel, x, y, w, PanelHeight * S, White.Fade(a));
            ui.Spaced(L"NYXARA", x + Pad * S, y + 11 * S, Tiny * S, 3 * S, Gold.Fade(a));
            ui.Text(FormName(form), x + Pad * S, y + 23 * S, Name * S, tierColor[form].Fade(a));
            for (int i = 0; i < 3; ++i)
                ui.Icon(HudArt::IconWings, x + w - Pad * S - (static_cast<float>(2 - i) + .5f) * 26 * S, y + 24 * S, 24 * S, (i < form ? GoldLight : Faint).Fade(a));
            const std::wstring rule = form >= 3 ? L"She has become all she can be." : L"Evolve all three to become " + std::wstring(FormName(form + 1)) + L".";
            ui.Text(rule, x + Pad * S, y + 52 * S, Tiny * S, Dim.Fade(a));
            float cy = y + 74 * S;
            const float cx = x + 8 * S, cw = w - 16 * S;
            cy = card(Wings, cx, cy, cw, ItemCard * S, HudArt::IconWings, ItemName(Wings, s.Level[Wings]),
                      s.Level[Wings] ? L"Skills " + Times(Cooldown(s)) + L" time, boost " + Times(Boost(s)) : std::wstring(L"As she set out."));
            cy = card(Swords, cx, cy, cw, ItemCard * S, HudArt::IconSwords, ItemName(Swords, s.Level[Swords]),
                      s.Level[Swords] ? L"Damage " + Times(1 + SwordTierDamage * static_cast<float>(s.Level[Swords])) : std::wstring(L"As she set out."));
            card(Armour, cx, cy, cw, ItemCard * S, HudArt::IconArmour, ItemName(Armour, s.Level[Armour]),
                 s.Level[Armour] ? L"Damage taken " + Times(1 - ArmourTierTaken * static_cast<float>(s.Level[Armour])) : std::wstring(L"As she set out."));
        }

        // ---- right: crystals, training, what she carries, what it all adds up to
        {
            const float w = PanelWidth * S, x = W - Margin * S - w, y = Top * S;
            ui.Nine(HudArt::Panel, x, y, w, PanelHeight * S, White.Fade(a));
            ui.Spaced(L"CRYSTALS", x + Pad * S, y + 11 * S, Tiny * S, 3 * S, Gold.Fade(a));
            ui.Icon(HudArt::IconCrystal, x + Pad * S + 13 * S, y + 40 * S, 26 * S, Violet.Fade(a));
            ui.Text(std::to_wstring(s.Crystals), x + Pad * S + 32 * S, y + 26 * S, Name * S, Ink.Fade(a));
            ui.RightAligned(L"Every creature she fells leaves some.", x + w - Pad * S, y + 34 * S, Tiny * S, Dim.Fade(a));
            float cy = y + 62 * S;
            const float cx = x + 8 * S, cw = w - 16 * S;
            cy = card(Power, cx, cy, cw, TrainCard * S, HudArt::IconSlash, L"Level " + std::to_wstring(s.Level[Power]) + L" of " + std::to_wstring(MaxTrainingLevel),
                      L"Damage " + Times(1 + PowerStep * static_cast<float>(s.Level[Power])));
            cy = card(Defence, cx, cy, cw, TrainCard * S, HudArt::IconGuard, L"Level " + std::to_wstring(s.Level[Defence]) + L" of " + std::to_wstring(MaxTrainingLevel),
                      L"Damage taken " + Times(1 - DefenceStep * static_cast<float>(s.Level[Defence])));

            cy += 4 * S;
            ui.Spaced(L"SHE CARRIES", x + Pad * S, cy, Tiny * S, 3 * S, Gold.Fade(a));
            cy += ui.Height(Tiny * S) + 6 * S;
            const float line = ui.Height(Tiny * S) + 6 * S, icon = 17 * S;
            const auto carried = [&](const HudArt::Sprite& art, Color tint, const std::wstring& what, const std::wstring& note)
            {
                ui.Icon(art, x + Pad * S + icon * .5f, cy + ui.Height(Tiny * S) * .5f, icon, tint.Fade(a));
                ui.Text(what, x + Pad * S + icon + 6 * S, cy, Tiny * S, Ink.Fade(a));
                ui.RightAligned(note, x + w - Pad * S, cy, Tiny * S, Dim.Fade(a));
                cy += line;
            };
            carried(HudArt::IconPotion, Red, std::to_wstring(s.Potions) + L" of " + std::to_wstring(MaxPotions) + L" potions",
                    std::wstring(Ui::KeyLabel(L"Z", 'Z')) + L" drinks one: +" + std::to_wstring(v.PotionHeal) + L" health");
            carried(HudArt::IconFeather, Silver, std::to_wstring(v.Feathers) + L" eagle feathers", L"each sharpens her blades");
            carried(HudArt::IconCoin, GoldLight, std::to_wstring(v.Gold) + L" gold", L"");
            carried(HudArt::IconEye, Moon, std::to_wstring(v.Memories) + L" of 12 memories", L"F1 to read");

            cy += 4 * S;
            ui.Spaced(L"ALL TOGETHER", x + Pad * S, cy, Tiny * S, 3 * S, Gold.Fade(a));
            cy += ui.Height(Tiny * S) + 6 * S;
            const auto total = [&](const std::wstring& what, const std::wstring& value)
            {
                ui.Text(what, x + Pad * S, cy, Tiny * S, Dim.Fade(a));
                ui.RightAligned(value, x + w - Pad * S, cy, Tiny * S, Ink.Fade(a));
                cy += line;
            };
            total(L"Damage she deals", Times(Damage(s) * v.OtherDamage));
            total(L"Damage she takes", Times(DamageTaken(s) * v.OtherTaken));
        }

        // ---- keys
        {
            const float y = (Top - 40) * S, h = 24 * S;
            float x = W * .5f - 250 * S;
            const auto key = [&](const wchar_t* label, const wchar_t* what)
            {
                const float kw = ui.KeyWidth(label, h);
                ui.Key(label, x + kw * .5f, y, h, White.Fade(a));
                ui.Text(what, x + kw + 6 * S, y + (h * .86f - ui.Height(Tiny * S)) * .5f, Tiny * S, Ink.Fade(a));
                x += kw + 6 * S + ui.Width(what, Tiny * S) + 22 * S;
            };
            key(L"W", L""); x -= 20 * S;
            key(L"S", L"CHOOSE");
            { const float from = x; key(L"D", L"WHAT SHE CARRIES"); Pointer::Add(Pointer::PageKey, 0, from, y, x - from - 22 * S, h); }
            key(Ui::KeyLabel(L"ENTER", VK_RETURN), L"SPEND CRYSTALS");
            key(Ui::KeyLabel(L"I", 'I'), L"CLOSE");
        }
    }
}
