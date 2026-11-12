#include "Collection.h"

#include "GameConstants.h"
#include "GameUi.h"

#include <Windows.h>

#include <algorithm>
#include <bit>
#include <cmath>

namespace CaucasianEagle::Collection
{
    namespace
    {
        const wchar_t* const Creatures[CharmKinds] = {L"Chinka", L"Ali", L"Kudiani", L"Ochokochi", L"Almasty", L"Kaji", L"Uaig", L"Devi"};
        const wchar_t* const Charms[CharmKinds] = {L"Chinka tooth", L"Lock of an Ali's hair", L"Kudiani's tail", L"Ochokochi's thorn",
                                                   L"Almasty fur", L"Kaji's ember", L"Uaig's eye", L"Devi horn"};
        const wchar_t* const Keepsakes[CityCount] = {L"Wine horn", L"Bath stone", L"Pillar splinter", L"Cave lamp", L"Fleece thread", L"Sailor's knot", L"Svan dagger"};
        const wchar_t* const Runes[RuneCount] = {L"Rune of fire", L"Rune of frost", L"Rune of the moon"};

        std::wstring Percent(float share) { return std::to_wstring(static_cast<int>(std::lround(share * 100))) + L" %"; }
        std::wstring Of(int have, int all) { return std::to_wstring(have) + L" of " + std::to_wstring(all); }
        int Count(unsigned bits) { return static_cast<int>(std::popcount(bits)); }
    }

    const wchar_t* CharmName(int kind) { return Charms[std::clamp(kind, 0, CharmKinds - 1)]; }
    const wchar_t* CreatureName(int kind) { return Creatures[std::clamp(kind, 0, CharmKinds - 1)]; }
    const wchar_t* KeepsakeName(int city) { return Keepsakes[std::clamp(city, 0, CityCount - 1)]; }
    const wchar_t* RuneName(int rune) { return Runes[std::clamp(rune, 0, RuneCount - 1)]; }
    bool HasSet(const State& s, int kind) { return kind >= 0 && kind < CharmKinds && s.Charms[kind] >= CharmSet; }
    int FeathersFound(const State& s) { return Count(s.Feathers); }
    int RuneLevel(const State& s, int rune) { return s.Set == rune && rune >= 0 && rune < RuneCount ? std::clamp(s.Runes[rune], 0, MaxRunes) : 0; }

    float DamageTo(const State& s, int kind) { return HasSet(s, kind) ? 1 + CharmSetDamage : 1.f; }
    float DamageFrom(const State& s, int kind) { return HasSet(s, kind) ? 1 - CharmSetTaken : 1.f; }
    float Damage(const State& s, int cities) { return (1 + KeepsakeDamage * static_cast<float>(cities)) * (1 + FireRuneDamage * static_cast<float>(RuneLevel(s, Fire))); }
    float DamageTaken(const State&, int cities) { return 1 - KeepsakeTaken * static_cast<float>(cities); }
    float Boost(const State& s) { return 1 + FatherFeatherBoost * static_cast<float>(FeathersFound(s)); }
    float Crystals(int cities) { return cities >= CityCount ? SongCrystals : 1.f; }
    float CreaturePace(const State& s) { return 1 + FrostRunePace * static_cast<float>(RuneLevel(s, Frost)); }
    int KillHeal(const State& s) { return MoonRuneHeal * RuneLevel(s, MoonRune); }

    // Two panels, left and right, the same size and place as the Grow page, so Nyxara stays in view between them.
    void Draw(Ui& ui, const View& v)
    {
        if (v.Fade <= 0) return;
        using namespace Palette;
        const float S = ui.S, W = ui.W, a = v.Fade;
        constexpr float Margin = 22, Top = 190, PanelWidth = 372, PanelHeight = 440, Pad = 14, Tiny = 1.55f, Small = 1.9f;
        const State& s = v.Now;
        const int cities = Count(v.Cities);
        const float line = ui.Height(Tiny * S) + 6 * S, icon = 17 * S;

        ui.Sprite(HudArt::GradH, 0, 0, 460 * S, ui.H, Black.Fade(.55f * a));
        ui.Sprite(HudArt::GradH, W - 460 * S, 0, 460 * S, ui.H, Black.Fade(.55f * a), 3.14159265f);

        // ---- left: what creatures dropped
        {
            const float x = Margin * S, y = Top * S, w = PanelWidth * S;
            ui.Nine(HudArt::Panel, x, y, w, PanelHeight * S, White.Fade(a));
            ui.Spaced(L"CHARMS", x + Pad * S, y + 11 * S, Tiny * S, 3 * S, Gold.Fade(a));
            ui.Paragraph(L"Creatures drop them. Three of one kind are a set: she strikes that kind " + Percent(CharmSetDamage) +
                         L" harder and its blows hurt her " + Percent(CharmSetTaken) + L" less.",
                         x + Pad * S, y + 26 * S, Tiny * S, w - 2 * Pad * S, 2 * S, Dim.Fade(a));
            float cy = y + 76 * S;
            const float rowHeight = 43 * S;
            for (int kind = 0; kind < CharmKinds; ++kind)
            {
                const int have = (std::min)(s.Charms[kind], CharmSet);
                const bool set = have >= CharmSet, any = s.Charms[kind] > 0;
                ui.Rect(x + 8 * S, cy, w - 16 * S, rowHeight - 4 * S, Well.Fade(a * (set ? .9f : .55f)));
                if (set) ui.Rect(x + 8 * S, cy, 3 * S, rowHeight - 4 * S, GoldLight.Fade(a));
                ui.Icon(HudArt::IconSkull, x + 30 * S, cy + (rowHeight - 4 * S) * .5f, 24 * S, (set ? GoldLight : any ? Ink : Faint).Fade(a));
                ui.Text(any ? CharmName(kind) : L"Not found yet", x + 50 * S, cy + 4 * S, Small * S, (any ? Ink : Faint).Fade(a));
                ui.Text(set ? std::wstring(L"Set complete against the ") + CreatureName(kind) : std::wstring(L"From the ") + CreatureName(kind),
                        x + 50 * S, cy + 4 * S + ui.Height(Small * S) + 1 * S, Tiny * S, (set ? GoldLight : Dim).Fade(a));
                for (int i = 0; i < CharmSet; ++i)
                    ui.Icon(HudArt::Diamond, x + w - 20 * S - static_cast<float>(CharmSet - 1 - i) * 15 * S, cy + (rowHeight - 4 * S) * .5f, 12 * S,
                            (i < have ? (set ? GoldLight : Moon) : Faint).Fade(a));
                cy += rowHeight;
            }
        }

        // ---- right: what the road gave, and the runes in her blades
        {
            const float w = PanelWidth * S, x = W - Margin * S - w, y = Top * S;
            ui.Nine(HudArt::Panel, x, y, w, PanelHeight * S, White.Fade(a));
            float cy = y + 11 * S;
            const auto heading = [&](const wchar_t* title)
            {
                ui.Spaced(title, x + Pad * S, cy, Tiny * S, 3 * S, Gold.Fade(a));
                cy += ui.Height(Tiny * S) + 6 * S;
            };
            const auto row = [&](const HudArt::Sprite& art, Color tint, const std::wstring& what, const std::wstring& note, bool lit)
            {
                ui.Icon(art, x + Pad * S + icon * .5f, cy + ui.Height(Tiny * S) * .5f, icon, (lit ? tint : Faint).Fade(a));
                ui.Text(what, x + Pad * S + icon + 6 * S, cy, Tiny * S, (lit ? Ink : Dim).Fade(a));
                ui.RightAligned(note, x + w - Pad * S, cy, Tiny * S, Dim.Fade(a));
                cy += line;
            };
            heading(L"FROM THE ROAD");
            row(HudArt::IconMountain, Silver, Of(s.Links, LinkCount) + L" chain links",
                s.Links >= LinkCount ? L"the ending is changed" : L"one per guardian", s.Links > 0);
            row(HudArt::IconBook, GoldLight, Of(cities, CityCount) + L" verses of the road song",
                cities >= CityCount ? L"crystals +" + Percent(SongCrystals - 1) : L"one in each city", cities > 0);
            row(HudArt::IconFeather, Moon, Of(FeathersFound(s), FeatherCount) + L" feathers of her father",
                FeathersFound(s) ? L"boost +" + Percent(Boost(s) - 1) : L"hidden by the road", FeathersFound(s) > 0);

            cy += 6 * S;
            heading(L"KEEPSAKES OF THE CITIES");
            ui.Text(cities ? L"Each: blows +" + Percent(KeepsakeDamage) + L", blows taken -" + Percent(KeepsakeTaken) + L"." : std::wstring(L"Each city gives one."),
                    x + Pad * S, cy, Tiny * S, Dim.Fade(a));
            cy += line;
            {   // seven names in two columns
                const float column = (w - 2 * Pad * S) * .5f;
                for (int city = 0; city < CityCount; ++city)
                {
                    const bool have = (v.Cities >> city & 1u) != 0;
                    const float cx = x + Pad * S + (city % 2) * column, ry = cy + static_cast<float>(city / 2) * line;
                    ui.Icon(HudArt::Diamond, cx + 5 * S, ry + ui.Height(Tiny * S) * .5f, 9 * S, (have ? GoldLight : Faint).Fade(a));
                    ui.Text(have ? KeepsakeName(city) : L"...", cx + 14 * S, ry, Tiny * S, (have ? Ink : Faint).Fade(a));
                }
                cy += 4 * line;
            }

            cy += 6 * S;
            heading(L"RUNES FOR HER BLADES");
            ui.Text(L"From old gold chests. One kind is set.", x + Pad * S, cy, Tiny * S, Dim.Fade(a));
            cy += line + 2 * S;
            const Color runeColor[RuneCount] = {{1.f, .55f, .25f, 1}, {.55f, .85f, 1.f, 1}, {.85f, .80f, 1.f, 1}};
            const std::wstring gives[RuneCount] = {
                L"blows +" + Percent(FireRuneDamage) + L" each",
                L"creatures strike " + Percent(FrostRunePace) + L" slower each",
                L"+" + std::to_wstring(MoonRuneHeal) + L" health a kill, each"};
            const float cardHeight = 40 * S;
            for (int rune = 0; rune < RuneCount; ++rune)
            {
                const bool selected = rune == v.Selected, set = s.Set == rune, have = s.Runes[rune] > 0;
                const float cx = x + 8 * S, cw = w - 16 * S;
                Pointer::Add(Pointer::RuneCard, rune, cx, cy, cw, cardHeight);
                ui.Rect(cx, cy, cw, cardHeight, Well.Fade(a * (selected ? .92f : .6f)));
                const Color edge = selected ? GoldLight.Fade(a * (.75f + .25f * std::sin(ui.Time * 4))) : Gold.Fade(a * .28f);
                const float t = (selected ? 2 : 1) * S;
                ui.Rect(cx, cy, cw, t, edge); ui.Rect(cx, cy + cardHeight - t, cw, t, edge);
                ui.Rect(cx, cy, t, cardHeight, edge); ui.Rect(cx + cw - t, cy, t, cardHeight, edge);
                if (set && v.Flash > 0) ui.Rect(cx, cy, cw, cardHeight, runeColor[rune].Fade(a * v.Flash * .35f));
                ui.Icon(HudArt::IconEmber, cx + 20 * S, cy + cardHeight * .5f, 24 * S, (have ? runeColor[rune] : Faint).Fade(a));
                ui.Text(RuneName(rune), cx + 40 * S, cy + 4 * S, Small * S, (have ? Ink : Faint).Fade(a));
                ui.Text(have ? gives[rune] : L"Not found yet", cx + 40 * S, cy + 4 * S + ui.Height(Small * S) + 1 * S, Tiny * S, Dim.Fade(a));
                for (int i = 0; i < MaxRunes; ++i)
                    ui.Icon(HudArt::Diamond, cx + cw - 14 * S - static_cast<float>(MaxRunes - 1 - i) * 14 * S, cy + 12 * S, 11 * S,
                            (i < s.Runes[rune] ? runeColor[rune] : Faint).Fade(a));
                if (set) ui.RightAligned(L"SET", cx + cw - 8 * S, cy + cardHeight - ui.Height(Tiny * S) - 4 * S, Tiny * S, GoldLight.Fade(a));
                cy += cardHeight + 5 * S;
            }
        }

        // ---- keys
        {
            const float y = (Top - 40) * S, h = 24 * S;
            float x = W * .5f - 230 * S;
            const auto key = [&](const wchar_t* label, const wchar_t* what)
            {
                const float kw = ui.KeyWidth(label, h);
                ui.Key(label, x + kw * .5f, y, h, White.Fade(a));
                ui.Text(what, x + kw + 6 * S, y + (h * .86f - ui.Height(Tiny * S)) * .5f, Tiny * S, Ink.Fade(a));
                x += kw + 6 * S + ui.Width(what, Tiny * S) + 22 * S;
            };
            { const float from = x; key(L"A", L"GROW"); Pointer::Add(Pointer::PageKey, 0, from, y, x - from - 22 * S, h); }
            key(L"W", L""); x -= 20 * S;
            key(L"S", L"CHOOSE RUNE");
            key(Ui::KeyLabel(L"ENTER", VK_RETURN), L"SET IN BLADES");
            key(Ui::KeyLabel(L"I", 'I'), L"CLOSE");
        }
    }
}
