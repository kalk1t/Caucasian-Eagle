#include "AscentQuests.h"

#include "AscentGrowth.h"
#include "GameConstants.h"
#include "GameUi.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <string>
#include <vector>

namespace CaucasianEagle::AscentQuests
{
    namespace
    {
        // STAND-INS: the owner's counts (3, 6, 9), each set bought with the crystals of its kind, at prices she can pay
        // out of what the way gives (wind 130 up to the third nest, fire 297 up to the sixth, ice 545 beyond, if she takes
        // all: plan section 4). Step 6 puts the chosen quests here. (Their ids are in saves: keep them.)
        const Quest Quests[] = {
            {"wind.1", 0, Beat, 4, 8}, {"wind.2", 0, Jars, 6, 12}, {"wind.3", 0, Reach, 4, 18},
            {"fire.1", 1, Beat, 5, 12}, {"fire.2", 1, Jars, 6, 15}, {"fire.3", 1, Beat, 8, 20},
            {"fire.4", 1, Jars, 10, 24}, {"fire.5", 1, Beat, 10, 28}, {"fire.6", 1, Reach, 7, 32},
            {"ice.1", 2, Beat, 5, 12}, {"ice.2", 2, Jars, 6, 14}, {"ice.3", 2, Beat, 8, 16},
            {"ice.4", 2, Jars, 8, 18}, {"ice.5", 2, Beat, 10, 20}, {"ice.6", 2, Jars, 10, 22},
            {"ice.7", 2, Reach, 9, 24}, {"ice.8", 2, Beat, 12, 26}, {"ice.9", 2, Beat, 14, 30},
        };
        constexpr int Total = static_cast<int>(sizeof Quests / sizeof Quests[0]);
        static_assert(Total <= Growth::MaxQuests, "Growth::State holds this many quests");

        const wchar_t* Number(int n)
        {
            static const wchar_t* const words[] = {L"no", L"one", L"two", L"three", L"four", L"five", L"six", L"seven", L"eight", L"nine",
                                                   L"ten", L"eleven", L"twelve", L"thirteen", L"fourteen", L"fifteen"};
            return n >= 0 && n < 16 ? words[n] : nullptr;
        }
        std::wstring CountWords(int n) { const wchar_t* w = Number(n); return w ? std::wstring(w) : std::to_wstring(n); }
        std::wstring Nth(int n)
        {
            static const wchar_t* const words[] = {L"", L"first", L"second", L"third", L"fourth", L"fifth", L"sixth", L"seventh", L"eighth", L"ninth"};
            return n > 0 && n < 10 ? std::wstring(words[n]) : std::to_wstring(n) + L"th";
        }
        Color Tinted(int set, float alpha = 1)
        {
            const auto t = AscentGrowth::CrystalColour(Kind(set));
            return Color{t.R, t.G, t.B, alpha};
        }

        // What the panel says of a set (-1: all done)
        std::wstring Lead(int set)
        {
            switch (set)
            {
            case 0: return L"Each quest is bought with wind crystals and counts from then on. When all three are done, she wears THE HIGHLANDS: wings, sabres and harness of the old queen.";
            case 1: return L"Each is bought with fire crystals. When all six are done, she wears AMIRANI'S FIRE, ember and obsidian: her blows burn.";
            case 2: return L"Each is bought with ice crystals. When all nine are done, she wears MOONLIT: night-sky wings and star-crystal blades.";
            default: return L"All her quests are done, and she wears MOONLIT, the last gear. The summit is above her.";
            }
        }
    }

    int QuestCount() { return Total; }
    const Quest& QuestAt(int index) { return Quests[std::clamp(index, 0, Total - 1)]; }
    int QuestIndex(const std::string& id)
    {
        for (int i = 0; i < Total; ++i) if (id == Quests[i].Id) return i;
        return -1;
    }

    std::wstring QuestTitle(int index)
    {
        const Quest& q = QuestAt(index);
        switch (q.Kind)
        {
        case Beat: return L"Beat " + CountWords(q.Need) + (q.Need == 1 ? L" creature" : L" creatures");
        case Jars: return L"Break " + CountWords(q.Need) + (q.Need == 1 ? L" clay jar" : L" clay jars");
        default: return L"Reach the " + Nth(q.Need) + L" Eagle Nest";
        }
    }

    std::wstring QuestProgress(const Growth::State& s, int index)
    {
        const Quest& q = QuestAt(index);
        switch (s.QuestState[index])
        {
        case Taken: return q.Kind == Reach ? L"on the way" : std::to_wstring((std::min)(s.QuestCount[index], q.Need)) + L" of " + std::to_wstring(q.Need);
        case Done: return L"done";
        default: return L"";
        }
    }

    int OpenSet(const Growth::State& s) { return s.Gear >= 0 && s.Gear < Sets ? s.Gear : -1; }
    int Kind(int set) { return std::clamp(set, 0, Sets - 1); }   // (wind, fire, ice: AscentGrowth::CrystalKind)
    int QuestsOf(int set)
    {
        int n = 0;
        for (const auto& q : Quests) if (q.Set == set) ++n;
        return n;
    }
    std::vector<int> Rows(int set)
    {
        std::vector<int> rows;
        for (int i = 0; i < Total; ++i) if (Quests[i].Set == set) rows.push_back(i);
        return rows;
    }
    int DoneOf(const Growth::State& s, int set)
    {
        int n = 0;
        for (int i = 0; i < Total; ++i) if (Quests[i].Set == set && s.QuestState[i] == Done) ++n;
        return n;
    }
    bool SetDone(const Growth::State& s, int set) { return set >= 0 && set < Sets && QuestsOf(set) > 0 && DoneOf(s, set) == QuestsOf(set); }

    void Clamp(Growth::State& s)
    {
        for (int i = 0; i < Growth::MaxQuests; ++i)
        {
            s.QuestState[i] = i < Total ? std::clamp(s.QuestState[i], 0, static_cast<int>(Done)) : 0;   // (the spirits' build had 3, brought: done)
            s.QuestCount[i] = i < Total ? (std::max)(0, s.QuestCount[i]) : 0;
        }
    }

    // The panel, in the middle (the world waits behind it): what the set gives and asks, its quests in rows (each: its
    // mark, its title, how far she is with it, its price and TAKE), what comes after, the keys under.
    void Draw(Ui& ui, const View& v)
    {
        if (v.Fade <= 0) return;
        using namespace Palette;
        const float W = ui.W, a = v.Fade;
        constexpr float Width = 640, Pad = 16, Tiny = 1.55f, Small = 1.9f, Big = 2.6f;
        const Growth::State& s = v.Now;
        const int set = OpenSet(s), kind = Kind(set);
        const Color tint = set >= 0 ? Tinted(set) : Moon;
        const std::vector<int> rows = set >= 0 ? Rows(set) : std::vector<int>{};

        // Its measures at a scale: the window's own, or a smaller one where the panel and its keys would not fit under
        // each other in the window (nine quests in a window 720 high, or the interface made larger in the settings).
        std::vector<std::wstring> said;
        float S = ui.S, w = 0, x = 0, inner = 0, right = 0, rowH = 0, line = 0, head = 0, words = 0, h = 0, keys = 0;
        const auto measure = [&](float scale)
        {
            S = scale;
            w = Width * S; x = (W - w) * .5f; inner = w - 2 * Pad * S; right = x + w - Pad * S;
            said = ui.Wrap(Lead(set), Small * S, inner);
            rowH = 46 * S; line = ui.Height(Small * S) + 3 * S;
            head = 92 * S; words = static_cast<float>(said.size()) * line + 14 * S;
            const float list = set >= 0 ? ui.Height(Tiny * S) + 10 * S + static_cast<float>(rows.size()) * (rowH + 4 * S) + ui.Height(Tiny * S) + 8 * S : 0.f;
            h = head + words + list + 14 * S; keys = 24 * S + ui.Height(Tiny * S) + 30 * S;
        };
        measure(ui.S);
        const float air = 8 * ui.S;   // (over and under it, at the least)
        if (h + keys > ui.H - 2 * air) measure(ui.S * (ui.H - 2 * air) / (h + keys));
        const float y = (std::max)((std::min)(90 * S, ui.H - h - keys - air), (ui.H - h - keys) * .5f);

        ui.Rect(0, 0, W, ui.H, Black.Fade(.42f * a));
        ui.Nine(HudArt::Panel, x, y, w, h, White.Fade(a));
        ui.Rect(x + 6 * S, y + 6 * S, w - 12 * S, 3 * S, tint.Fade(.8f * a));

        // its name; the gear it gives; what she has of its crystals
        ui.Icon(HudArt::Glow, x + Pad * S + 20 * S, y + 38 * S, 70 * S, tint.Fade(.35f * a));
        ui.Icon(set >= 0 ? HudArt::IconCrystal : HudArt::IconWings, x + Pad * S + 20 * S, y + 38 * S, 30 * S, tint.Fade(a));
        ui.Text(L"HER QUESTS", x + Pad * S + 46 * S, y + 16 * S, Big * S, tint.Fade(a));
        const std::wstring sub = set >= 0 ? std::wstring(L"For ") + AscentGrowth::GearTitle(set + 1) + L", her " + Nth(set + 1) + L" gear of three." : std::wstring(L"All done.");
        ui.Text(sub, x + Pad * S + 46 * S, y + 50 * S, Tiny * S, Dim.Fade(a));
        if (set >= 0)
        {
            const std::wstring have = std::to_wstring(s.AscentCrystals[kind]);
            const float hw = ui.Width(have, Small * S);
            ui.RightAligned(have, right, y + 16 * S, Small * S, Ink.Fade(a));
            ui.Icon(HudArt::IconCrystal, right - hw - 12 * S, y + 16 * S + ui.Height(Small * S) * .5f, 18 * S, tint.Fade(a));
            ui.RightAligned(std::wstring(AscentGrowth::CrystalName(kind)) + L": she has", right, y + 16 * S + ui.Height(Small * S) + 2 * S, Tiny * S, Dim.Fade(a));
        }

        float cy = y + head;
        for (const auto& l : said) { ui.Text(l, x + Pad * S, cy, Small * S, Ink.Fade(a)); cy += line; }
        cy += 14 * S;

        // its quests
        if (set >= 0)
        {
            ui.Spaced(L"QUESTS", x + Pad * S, cy, Tiny * S, 3 * S, Gold.Fade(a));
            ui.RightAligned(std::to_wstring(DoneOf(s, set)) + L" of " + std::to_wstring(rows.size()) + L" done", right, cy, Tiny * S, Dim.Fade(a));
            cy += ui.Height(Tiny * S) + 10 * S;
            for (int r = 0; r < static_cast<int>(rows.size()); ++r)
            {
                const int i = rows[static_cast<size_t>(r)];
                const Quest& q = Quests[i];
                const int state = s.QuestState[i];
                const bool chosen = r == v.Selected, flash = r == v.FlashRow && v.Flash > 0;
                ui.Rect(x + 6 * S, cy, w - 12 * S, rowH, (chosen ? tint.Fade(.16f) : Well.Fade(.55f)).Fade(a));
                if (chosen) ui.Rect(x + 6 * S, cy, 3 * S, rowH, tint.Fade(a));
                if (flash) ui.Rect(x + 6 * S, cy, w - 12 * S, rowH, tint.Fade(a * .3f * v.Flash));
                Pointer::Add(Pointer::QuestRow, r, x + 6 * S, cy, w - 12 * S, rowH);
                const float my = cy + rowH * .5f, ty = my - ui.Height(Small * S) * .5f;
                // its mark: offered a faint diamond, taken its colour, done gold
                const Color mark = state == Done ? GoldLight : state == Taken ? tint : Faint;
                ui.Icon(HudArt::Diamond, x + Pad * S + 9 * S, my, (state == Offered ? 13.f : 16.f) * S, mark.Fade(a));
                ui.Text(QuestTitle(i), x + Pad * S + 26 * S, ty, Small * S, Ink.Fade(a));
                if (state == Offered)
                {   // its price, and TAKE (dim while she has too few of its kind)
                    const bool can = s.AscentCrystals[kind] >= q.Price;
                    const float kh = 24 * S;
                    const wchar_t* take = L"TAKE";
                    const float tw = ui.Width(take, Tiny * S) + 26 * S, tx = right - tw;
                    ui.Rect(tx, my - kh * .5f, tw, kh, (can ? tint.Fade(chosen ? .55f : .3f) : Faint.Fade(.25f)).Fade(a));
                    ui.Centered(take, tx + tw * .5f, my - ui.Height(Tiny * S) * .5f, Tiny * S, (can ? White : Faint).Fade(a));
                    Pointer::Add(Pointer::QuestTake, r, tx, my - kh * .5f, tw, kh);
                    const std::wstring price = std::to_wstring(q.Price);
                    const float pw = ui.Width(price, Small * S);
                    ui.RightAligned(price, tx - 10 * S, ty, Small * S, (can ? Ink : Faint).Fade(a));
                    ui.Icon(HudArt::IconCrystal, tx - 10 * S - pw - 11 * S, my, 17 * S, tint.Fade(a * (can ? 1.f : .5f)));
                }
                else
                    ui.RightAligned(QuestProgress(s, i), right, ty + (ui.Height(Small * S) - ui.Height(Tiny * S)) * .5f, Tiny * S, (state == Taken ? tint : GoldLight).Fade(a));
                cy += rowH + 4 * S;
            }
            // what comes after
            cy += 4 * S;
            const std::wstring after = set + 1 < Sets
                ? L"Then " + CountWords(QuestsOf(set + 1)) + L" more, for " + AscentGrowth::GearTitle(set + 2) + L", bought with " + AscentGrowth::CrystalName(Kind(set + 1)) + L" crystals."
                : std::wstring(L"The last of her quests.");
            ui.Text(after, x + Pad * S, cy, Tiny * S, Dim.Fade(a));
        }

        // keys, under the panel
        {
            const float ky = y + h + 12 * S, kh = 24 * S;
            float kx = x + Pad * S;
            ui.Rect(x, ky - 7 * S, w, kh + ui.Height(Tiny * S) + 22 * S, Color{.015f, .025f, .04f, .75f * a});
            const auto key = [&](const wchar_t* label, const wchar_t* what)
            {
                const float kw = ui.KeyWidth(label, kh);
                ui.Key(label, kx + kw * .5f, ky, kh, White.Fade(a));
                ui.Text(what, kx + kw + 6 * S, ky + (kh * .86f - ui.Height(Tiny * S)) * .5f, Tiny * S, Ink.Fade(a));
                const float total = kw + 6 * S + ui.Width(what, Tiny * S);
                kx += total + 22 * S;
                return total;
            };
            if (set >= 0)
            {
                key(Ui::KeyLabel(L"W S", 'W'), L"CHOOSE");
                key(Ui::KeyLabel(L"ENTER", VK_RETURN), L"TAKE");
            }
            const float closeX = kx;
            const float cw = key(Ui::KeyLabel(L"TAB", VK_TAB), L"CLOSE");
            Pointer::Add(Pointer::CloseKey, 0, closeX, ky, cw, kh);
            ui.Text(set >= 0 ? L"or the mouse: a quest's TAKE, or CLOSE" : L"or the mouse: CLOSE", x + Pad * S, ky + kh + 8 * S, Tiny * S, Dim.Fade(a));
        }
    }

    std::vector<int> Listed(const Growth::State& s, const Leaving& leaving)
    {
        const int set = OpenSet(s);
        std::vector<int> rows;
        for (int i = 0; i < Total; ++i)
            if ((Quests[i].Set == set && s.QuestState[i] == Taken) || (s.QuestState[i] == Done && leaving.Left[i] > 0)) rows.push_back(i);
        return rows;
    }

    void DrawTaken(Ui& ui, const Growth::State& s, const Leaving& leaving, float x, float y, float bottom)
    {   // the quests she is on, every one, in her set's colour and with how far she is; one just done: gold, a line drawn
        // through it, out to the left, and the rows under it close up. None on the way: where they are (Tab).
        using namespace Palette;
        const int set = OpenSet(s);
        const std::vector<int> rows = Listed(s, leaving);
        if (set < 0 && rows.empty()) return;   // (all her quests done, and the last gone from the list)
        constexpr float Tiny = 1.55f, Least = .72f;
        // a done row's going, 0 .. 1 of LeaveSeconds: its moment of light; the line through it; held to be read; out to
        // the left, fading; and its place closed
        constexpr float PopEnd = .14f, StruckFrom = .04f, StruckEnd = .30f, OutFrom = .58f, OutEnd = .84f;
        const auto ease = [](float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3 - 2 * t); };
        const auto going = [&](int i) { return s.QuestState[i] == Done ? std::clamp(1 - leaving.Left[i] / LeaveSeconds, 0.f, 1.f) : -1.f; };
        const auto share = [&](int i) { const float u = going(i); return u < OutEnd ? 1.f : 1 - ease((u - OutEnd) / (1 - OutEnd)); };

        const float S = ui.S, w = 396 * S, head = 22 * S, foot = 4 * S;
        float lines = 0;
        for (const int i : rows) lines += share(i);
        // its rows made smaller where they would go under 'bottom'; where that is not enough, the last of them counted
        const float full = ui.Height(Tiny * S) + 5 * S, room = bottom - y - head - foot;
        const float fit = lines * full > room && room > 0 ? (std::max)(Least, room / (lines * full)) : 1.f;
        const float size = Tiny * S * fit, line = ui.Height(size) + 5 * S * fit;
        size_t shown = rows.size();
        if (lines * line > room + .5f && room > 0) shown = static_cast<size_t>((std::max)(1.f, std::floor(room / line) - 1));
        const bool more = shown < rows.size();
        float tall = more ? 1.f : 0.f;
        for (size_t k = 0; k < shown; ++k) tall += share(rows[k]);
        tall = (std::max)(tall, 1.f);

        const Color c = set >= 0 ? Tinted(set) : GoldLight;
        ui.Rect(x, y, w, head + tall * line + foot, {.015f, .025f, .04f, .62f});
        ui.Spaced(L"QUESTS", x + 12 * S, y + 6 * S, Tiny * S * .9f, 2.5f * S, Gold);
        if (set >= 0)
        {   // its key and how many of the set are done
            const std::wstring done = std::to_wstring(DoneOf(s, set)) + L" of " + std::to_wstring(QuestsOf(set)) + L" done";
            const wchar_t* tab = Ui::KeyLabel(L"TAB", VK_TAB);
            const float kh = 15 * S, kw = ui.KeyWidth(tab, kh), dw = ui.Width(done, Tiny * S * .9f);
            ui.RightAligned(done, x + w - 12 * S, y + 6 * S, Tiny * S * .9f, Dim);
            ui.Key(tab, x + w - 12 * S - dw - 7 * S - kw * .5f, y + 4 * S, kh, White);
        }
        else ui.RightAligned(L"all done", x + w - 12 * S, y + 6 * S, Tiny * S * .9f, GoldLight);
        float cy = y + head;
        if (rows.empty())
        {   // none on the way: how many she can take, and with what
            int offered = 0;
            for (const int i : Rows(set)) if (s.QuestState[i] == Offered) ++offered;
            const std::wstring crystals = std::wstring(AscentGrowth::CrystalName(Kind(set))) + L" crystals.";
            if (offered == QuestsOf(set)) ui.Text(L"None taken. They are bought with " + crystals, x + 14 * S, cy, Tiny * S, Dim);
            else if (offered > 0)
            {
                std::wstring count = CountWords(offered);
                count[0] = static_cast<wchar_t>(std::towupper(count[0]));
                ui.Text(count + L" more to take, with " + crystals, x + 14 * S, cy, Tiny * S, Dim);
            }
            return;
        }
        const float textH = ui.Height(size);
        for (size_t k = 0; k < shown; ++k)
        {
            const int i = rows[k];
            const float u = going(i), mid = cy + textH * .5f;
            const std::wstring title = QuestTitle(i);
            if (u < 0)
            {   // on the way
                const Color own = Quests[i].Set == set ? c : Tinted(Quests[i].Set);
                ui.Icon(HudArt::Diamond, x + 20 * S, mid, 11 * S * fit, own);
                ui.Text(title, x + 32 * S, cy, size, Ink);
                ui.RightAligned(QuestProgress(s, i), x + w - 12 * S, cy, size, own);
            }
            else if (u < OutEnd)
            {   // done: its moment
                const float pop = 1 - std::clamp(u / PopEnd, 0.f, 1.f), struck = ease((u - StruckFrom) / (StruckEnd - StruckFrom));
                const float out = ease((u - OutFrom) / (OutEnd - OutFrom)), seen = 1 - out, dx = -52 * S * out;
                ui.Rect(x + 4 * S, cy - 2.5f * S * fit, w - 8 * S, line, GoldLight.Fade((.10f + .40f * pop) * seen));
                if (pop > 0) ui.Icon(HudArt::Glow, x + 20 * S + dx, mid, 44 * S * fit, GoldLight.Fade(.55f * pop));
                ui.Icon(HudArt::Diamond, x + 20 * S + dx, mid, 11 * S * fit * (1 + .9f * pop), GoldLight.Fade(seen));
                ui.Text(title, x + 32 * S + dx, cy, size, GoldLight.Fade(seen));
                if (struck > 0) ui.Rect(x + 30 * S + dx, std::round(mid), (ui.Width(title, size) + 4 * S) * struck, (std::max)(1.f, std::round(1.4f * S * fit)), GoldLight.Fade(.9f * seen));
                ui.RightAligned(L"done", x + w - 12 * S + dx, cy, size, GoldLight.Fade(seen));
            }
            cy += line * share(i);
        }
        if (more) ui.Text(L"and " + std::to_wstring(rows.size() - shown) + L" more", x + 32 * S, cy, size, Dim);
    }
}
