#include "Settings.h"

#include "DoorwaysGame.h"
#include "DoorwaysGamepad.h"
#include "GameConstants.h"
#include "GameUi.h"
#include "Sound.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace CaucasianEagle::Settings
{
    using Doorways::DoorwaysPlayerPreferences;
    using Doorways::DoorwaysWindowMode;
    using Doorways::GameSession;
    using Doorways::IsDoorwaysKeyDown;

    namespace
    {
        enum Row
        {
            RowWindow, RowSize, RowApply, RowSync, RowShadows,     // picture
            RowMaster, RowMusic, RowEffects, RowMenus,             // sound
            RowMouse, RowInvert,                                   // controls
            RowScale, RowFrames,                                   // interface
            RowDefaults, RowBack, RowCount
        };
        enum Kind { Choice, Slider, Action };
        struct Entry { const wchar_t* Section; const wchar_t* Label; Kind Type; };
        const Entry Entries[RowCount] = {
            {L"PICTURE", L"Window", Choice}, {nullptr, L"Window size", Choice}, {nullptr, L"Apply the window", Action},
            {nullptr, L"Smooth frames (vertical sync)", Choice}, {nullptr, L"Shadows", Choice},
            {L"SOUND", L"Everything", Slider}, {nullptr, L"Music", Slider}, {nullptr, L"Fights, world and voices", Slider}, {nullptr, L"Menus", Slider},
            {L"CONTROLS", L"Mouse speed", Slider}, {nullptr, L"Look up and down reversed", Choice},
            {L"INTERFACE", L"Size of the interface", Choice}, {nullptr, L"Frames a second, shown", Choice},
            {L"", L"Restore the defaults", Action}, {nullptr, L"Done", Action}};

        constexpr int KeyCount = 11;
        const int Keys[KeyCount] = {'W', VK_UP, 'S', VK_DOWN, 'A', VK_LEFT, 'D', VK_RIGHT, VK_RETURN, VK_SPACE, VK_ESCAPE};
        constexpr float RepeatAfter = .38f, RepeatEvery = .055f, ScriptEvery = .4f;

        bool gOpen = false, gBack = false;
        float gFade = 0;
        int gRow = 0;
        bool gKeys[KeyCount] = {};
        bool gMouse = false, gArmed = false;
        float gHeld = 0, gRepeat = 0;
        float gMouseX = -1, gMouseY = -1;

        // the window mode and size wait for "Apply the window"; everything else is applied as it is changed
        bool gWindowKnown = false, gWasConfirming = false;
        DoorwaysWindowMode gMode = DoorwaysWindowMode::Windowed;
        int gWidth = 0, gHeight = 0;

        // testing: CAUCASIAN_EAGLE_SETTINGS=1 holds the window open; CAUCASIAN_EAGLE_SETTINGS_KEYS plays keys into it
        // (W S A D, E = Enter, X = Esc, one every ScriptEvery seconds)
        bool gEnvRead = false, gForced = false;
        std::string gScript;
        size_t gScriptAt = 0;
        float gScriptClock = 0;
        std::string gLogged;

        void ReadEnvironment()
        {
            if (gEnvRead) return;
            gEnvRead = true;
            char value[128] = {}; size_t length = 0;
            if (getenv_s(&length, value, SettingsEnvironmentVariable) == 0 && length > 1 && std::atoi(value) > 0) { gForced = true; gOpen = true; }
            char keys[128] = {}; length = 0;
            if (getenv_s(&length, keys, SettingsKeysEnvironmentVariable) == 0 && length > 1) gScript = keys;
        }

        float Step(float value, int steps, float size, float low, float high)
        {
            const float at = value / size;      // from a value between two steps, the first press goes to the nearer one that way
            const float whole = steps > 0 ? std::floor(at + .001f) : steps < 0 ? std::ceil(at - .001f) : std::round(at);
            return std::clamp((whole + static_cast<float>(steps)) * size, low, high);
        }

        void LogState(const DoorwaysPlayerPreferences& p, bool confirming)
        {
            char line[320];
            std::snprintf(line, sizeof line,
                "CaucasianEagle settings: window=%s %dx%d sync=%d shadows=%d master=%.2f music=%.2f effects=%.2f menus=%.2f mouse=%.1f invert=%d scale=%.2f frames=%d trial=%d\n",
                p.Graphics.WindowMode == DoorwaysWindowMode::Borderless ? "full" : "windowed", p.Graphics.WindowedClientWidth, p.Graphics.WindowedClientHeight,
                p.Graphics.VSyncEnabled ? 1 : 0, static_cast<int>(p.Graphics.ShadowQualityProfile), p.Audio.MasterVolume, p.Audio.MusicVolume,
                p.Audio.EffectsVolume, p.Audio.InterfaceVolume, p.Controls.MouseSensitivity, p.Controls.InvertMouseY ? 1 : 0, p.Interface.UiScale,
                p.Interface.ShowPerformance ? 1 : 0, confirming ? 1 : 0);
            if (gLogged == line) return;
            gLogged = line;
            OutputDebugStringA(line);
        }
    }

    void Open()
    {
        gOpen = true; gRow = 0; gArmed = false; gBack = false; gWindowKnown = false; gHeld = 0;
        Sound::PlayUi("ui/open", .7f);
    }
    void Close() { if (gOpen && !gForced) { gOpen = false; gBack = false; } }
    bool IsOpen() { ReadEnvironment(); return gOpen; }
    void Back() { if (gOpen) gBack = true; }

    void Draw(Ui& ui, bool focused, bool startScreen)
    {
        using namespace Palette;
        ReadEnvironment();
        gFade = std::clamp(gFade + (gOpen ? 1.f : -1.f) * ui.Delta / JournalFadeSeconds, 0.f, 1.f);
        if (gFade <= 0) return;
        auto& link = GameSession().Settings;
        const DoorwaysPlayerPreferences& now = link.Active;
        if (!link.Ready) return;
        if (!gWindowKnown || (gWasConfirming && !link.Confirming))
        {   // on opening, and when a trial has ended either way: start from what is on screen
            gWindowKnown = true;
            gMode = now.Graphics.WindowMode; gWidth = now.Graphics.WindowedClientWidth; gHeight = now.Graphics.WindowedClientHeight;
        }
        gWasConfirming = link.Confirming;
        LogState(now, link.Confirming);

        // ---- input
        bool pressed[KeyCount] = {}, down[KeyCount] = {};
        bool anyDown = false;
        for (int i = 0; i < KeyCount; ++i)
        {
            down[i] = gOpen && focused && IsDoorwaysKeyDown(Keys[i]);
            anyDown = anyDown || down[i];
            pressed[i] = gArmed && down[i] && !gKeys[i];
            gKeys[i] = down[i];
        }
        const bool mouseDown = gOpen && focused && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const bool clicked = gArmed && mouseDown && !gMouse;
        gMouse = mouseDown;
        if (!anyDown && !mouseDown) gArmed = true;      // keys held when the window opened must be let go first
        float mx = -1, my = -1;
        if (gOpen && focused)
        {
            POINT point{};
            if (GetCursorPos(&point) && ScreenToClient(GetForegroundWindow(), &point)) { mx = static_cast<float>(point.x); my = static_cast<float>(point.y); }
        }
        const bool moved = std::abs(mx - gMouseX) + std::abs(my - gMouseY) > .5f;   // resting on a row, the pointer leaves W and S alone
        gMouseX = mx; gMouseY = my;
        if (gOpen && gScriptAt < gScript.size())
        {
            gScriptClock += ui.Delta;
            if (gScriptClock >= ScriptEvery)
            {
                gScriptClock = 0;
                switch (gScript[gScriptAt++])
                {
                case 'W': pressed[0] = true; break; case 'S': pressed[2] = true; break;
                case 'A': pressed[4] = true; break; case 'D': pressed[6] = true; break;
                case 'E': pressed[8] = true; break; case 'X': gBack = true; break;
                default: break;
                }
            }
        }
        int step = (pressed[6] || pressed[7] ? 1 : 0) - (pressed[4] || pressed[5] ? 1 : 0);
        const int heldWay = (down[6] || down[7] ? 1 : 0) - (down[4] || down[5] ? 1 : 0);
        if (heldWay != 0 && step == 0)
        {   // held: sliders run
            gHeld += ui.Delta;
            if (gHeld > RepeatAfter) { gRepeat += ui.Delta; if (gRepeat > RepeatEvery) { gRepeat = 0; if (Entries[gRow].Type == Slider) step = heldWay; } }
        }
        else { gHeld = 0; gRepeat = 0; }
        bool enter = pressed[8] || pressed[9];
        const bool escape = (startScreen && pressed[10]) || gBack;
        gBack = false;

        // ---- the panel
        constexpr float Width = 600, RowH = 30, Gap = 3, Pad = 18, Head = 62, Foot = 40, SectionH = 24, ValueW = 240, Tiny = 1.55f, Small = 1.9f, Name = 2.6f;
        float units = Head + Foot;
        for (const auto& e : Entries) units += RowH + Gap + (e.Section ? SectionH : 0);
        const float S = ui.S * (std::min)(1.f, ui.H * .97f / (units * ui.S));      // a large interface must still fit the screen
        const float a = gFade;
        const float w = Width * S, h = units * S, x = (ui.W - w) * .5f, y = (ui.H - h) * .5f;
        ui.Rect(0, 0, ui.W, ui.H, Black.Fade(.55f * a));
        ui.Rect(x, y, w, h, Black.Fade(.5f * a));
        ui.Nine(HudArt::Panel, x, y, w, h, White.Fade(a));
        ui.SpacedCentered(L"CAUCASIAN EAGLE", x + w * .5f, y + 14 * S, Tiny * S, 4 * S, Gold.Fade(a));
        ui.Centered(L"Settings", x + w * .5f, y + 30 * S, Name * S, Ink.Fade(a));

        const bool windowChanged = gMode != now.Graphics.WindowMode ||
            (gMode == DoorwaysWindowMode::Windowed && (gWidth != now.Graphics.WindowedClientWidth || gHeight != now.Graphics.WindowedClientHeight));
        const auto percent = [](float value) { wchar_t t[16]; std::swprintf(t, 16, L"%d %%", static_cast<int>(std::lround(value * 100))); return std::wstring(t); };
        const auto onOff = [](bool on) { return std::wstring(on ? L"on" : L"off"); };
        const auto value = [&](int row, float& fraction) -> std::wstring
        {
            fraction = -1;
            wchar_t t[48] = {};
            switch (row)
            {
            case RowWindow: return gMode == DoorwaysWindowMode::Borderless ? L"whole screen" : L"in a window";
            case RowSize:
                if (gMode == DoorwaysWindowMode::Borderless) std::swprintf(t, 48, L"%d x %d", link.BorderlessSize.Width, link.BorderlessSize.Height);
                else std::swprintf(t, 48, L"%d x %d", gWidth, gHeight);
                return t;
            case RowApply: return windowChanged ? L"ENTER" : L"nothing to apply";
            case RowSync: return onOff(now.Graphics.VSyncEnabled);
            case RowShadows:
                switch (now.Graphics.ShadowQualityProfile)
                { case DoorwaysShadowQualityProfileId::Low: return L"light"; case DoorwaysShadowQualityProfileId::Medium: return L"medium"; default: return L"fine"; }
            case RowMaster: fraction = now.Audio.MasterMuted ? 0 : now.Audio.MasterVolume; return percent(fraction);
            case RowMusic: fraction = now.Audio.MusicMuted ? 0 : now.Audio.MusicVolume; return percent(fraction);
            case RowEffects: fraction = now.Audio.EffectsMuted ? 0 : now.Audio.EffectsVolume; return percent(fraction);
            case RowMenus: fraction = now.Audio.InterfaceMuted ? 0 : now.Audio.InterfaceVolume; return percent(fraction);
            case RowMouse:
                fraction = (now.Controls.MouseSensitivity - Doorways::DoorwaysMouseSensitivityMinimum) /
                    (Doorways::DoorwaysMouseSensitivityMaximum - Doorways::DoorwaysMouseSensitivityMinimum);
                std::swprintf(t, 48, L"%.1f", now.Controls.MouseSensitivity); return t;
            case RowInvert: return onOff(now.Controls.InvertMouseY);
            case RowScale: return percent(now.Interface.UiScale);
            case RowFrames: return onOff(now.Interface.ShowPerformance);
            default: return L"";
            }
        };

        const bool live = gOpen && !link.Confirming;
        int clickStep = 0;
        float cy = y + Head * S;
        for (int i = 0; i < RowCount; ++i)
        {
            const Entry& e = Entries[i];
            if (e.Section)
            {
                if (e.Section[0])
                {
                    ui.Spaced(e.Section, x + Pad * S, cy + 7 * S, Tiny * S, 3 * S, Gold.Fade(a));
                    const float tw = ui.SpacedWidth(e.Section, Tiny * S, 3 * S);
                    ui.Rect(x + Pad * S + tw + 10 * S, cy + 13 * S, w - 2 * Pad * S - tw - 10 * S, 1 * S, Gold.Fade(.3f * a));
                }
                cy += SectionH * S;
            }
            const float cx = x + Pad * S, cw = w - 2 * Pad * S, ch = RowH * S;
            const float vx = cx + cw - (ValueW + 10) * S, vw = ValueW * S;
            if (live && mx >= cx && mx <= cx + cw && my >= cy && my <= cy + ch)
            {
                if ((moved || clicked) && gRow != i) { gRow = i; Sound::PlayUi("ui/move", .5f); }
                if (clicked)
                {
                    if (e.Type == Action || mx < vx) enter = true;
                    else clickStep = mx < vx + vw * .5f ? -1 : 1;
                }
            }
            const bool selected = i == gRow;
            const bool idle = (i == RowSize && gMode == DoorwaysWindowMode::Borderless) || (i == RowApply && !windowChanged);
            ui.Rect(cx, cy, cw, ch, Well.Fade(a * (selected ? .92f : .5f)));
            const Color edge = selected ? GoldLight.Fade(a * (.75f + .25f * std::sin(ui.Time * 4))) : Gold.Fade(a * .2f);
            const float line = (selected ? 2 : 1) * S;
            ui.Rect(cx, cy, cw, line, edge); ui.Rect(cx, cy + ch - line, cw, line, edge);
            ui.Rect(cx, cy, line, ch, edge); ui.Rect(cx + cw - line, cy, line, ch, edge);
            ui.Icon(HudArt::Diamond, cx + 16 * S, cy + ch * .5f, (selected ? 11 : 8) * S, (selected ? GoldLight : Faint).Fade(a));
            const Color ink = idle ? Faint : (selected ? Ink : Dim);
            const float ty = cy + (ch - ui.Height(Small * S)) * .5f;
            ui.Text(e.Label, cx + 32 * S, ty, Small * S, ink.Fade(a));
            float fraction = -1;
            const std::wstring text = value(i, fraction);
            if (e.Type == Slider)
            {
                const float bw = vw - 70 * S, bh = 12 * S;
                ui.Bar(vx, cy + (ch - bh) * .5f, bw, bh, fraction, (selected ? GoldLight : Gold).Fade(a), 0, true);
                ui.RightAligned(text, vx + vw, ty, Small * S, ink.Fade(a));
            }
            else if (e.Type == Choice)
            {
                if (selected && !idle)
                {
                    ui.Text(L"<", vx, ty, Small * S, GoldLight.Fade(a));
                    ui.RightAligned(L">", vx + vw, ty, Small * S, GoldLight.Fade(a));
                }
                ui.Centered(text, vx + vw * .5f, ty, Small * S, ((i == RowWindow || i == RowSize) && windowChanged && !idle ? GoldLight : ink).Fade(a));
            }
            else ui.RightAligned(text, vx + vw, cy + (ch - ui.Height(Tiny * S)) * .5f, Tiny * S, (i == RowApply && windowChanged ? GoldLight : Faint).Fade(a));
            cy += (RowH + Gap) * S;
        }
        ui.Centered(L"W  S  choose      A  D  change      ENTER  take      ESC  back      or the mouse", x + w * .5f, cy + 10 * S, Tiny * S, Dim.Fade(a));

        // ---- a window change on trial
        if (link.Confirming)
        {
            const float bw = 430 * S, bh = 130 * S, bx = (ui.W - bw) * .5f, by = (ui.H - bh) * .5f;
            ui.Rect(0, 0, ui.W, ui.H, Black.Fade(.6f * a));
            ui.Rect(bx, by, bw, bh, Black.Fade(.7f * a));
            ui.Nine(HudArt::Panel, bx, by, bw, bh, White.Fade(a));
            ui.Centered(L"Keep this window?", bx + bw * .5f, by + 22 * S, Name * S, Ink.Fade(a));
            wchar_t t[80]; std::swprintf(t, 80, L"It goes back by itself in %d s.", (std::max)(0, link.SecondsRemaining));
            ui.Centered(t, bx + bw * .5f, by + 62 * S, Small * S, Dim.Fade(a));
            ui.Centered(L"ENTER  keep it      ESC  go back", bx + bw * .5f, by + 94 * S, Tiny * S, GoldLight.Fade(a));
            if (gOpen && link.DisplayAnswer == Doorways::DoorwaysGameSettings::Answer::None)
            {
                if (enter) { link.DisplayAnswer = Doorways::DoorwaysGameSettings::Answer::Keep; Sound::PlayUi("ui/buy", .6f); }
                else if (escape) { link.DisplayAnswer = Doorways::DoorwaysGameSettings::Answer::Revert; Sound::PlayUi("ui/close", .7f); }
            }
            return;
        }
        if (!gOpen) return;

        // ---- keys
        if (escape) { if (!gForced) { gOpen = false; Sound::PlayUi("ui/close", .7f); } return; }
        const int move = (pressed[2] || pressed[3] ? 1 : 0) - (pressed[0] || pressed[1] ? 1 : 0);
        if (move != 0) { gRow = (gRow + move + RowCount) % RowCount; Sound::PlayUi("ui/move", .5f); }
        if (clickStep != 0) step = clickStep;
        if (step == 0 && !enter) return;

        DoorwaysPlayerPreferences wanted = now;
        wanted.Graphics.WindowMode = now.Graphics.WindowMode;
        const int toggle = step != 0 || enter ? 1 : 0;
        bool sound = true;
        switch (gRow)
        {
        case RowWindow:
            if (toggle) gMode = gMode == DoorwaysWindowMode::Borderless ? DoorwaysWindowMode::Windowed : DoorwaysWindowMode::Borderless;
            break;
        case RowSize:
            if (gMode == DoorwaysWindowMode::Windowed && !link.Resolutions.empty())
            {
                const int count = static_cast<int>(link.Resolutions.size());
                int at = 0;
                for (int i = 0; i < count; ++i)
                    if (std::abs(link.Resolutions[i].Width - gWidth) + std::abs(link.Resolutions[i].Height - gHeight) <
                        std::abs(link.Resolutions[at].Width - gWidth) + std::abs(link.Resolutions[at].Height - gHeight)) at = i;
                at = std::clamp(at + (step != 0 ? step : 1), 0, count - 1);
                gWidth = link.Resolutions[at].Width; gHeight = link.Resolutions[at].Height;
            }
            else sound = false;
            break;
        case RowApply:
            if (enter && windowChanged)
            {
                wanted.Graphics.WindowMode = gMode;
                if (gMode == DoorwaysWindowMode::Windowed) { wanted.Graphics.WindowedClientWidth = gWidth; wanted.Graphics.WindowedClientHeight = gHeight; }
            }
            else sound = false;
            break;
        case RowSync: wanted.Graphics.VSyncEnabled = !now.Graphics.VSyncEnabled; break;
        case RowShadows:
        {
            const int level = (std::min)(static_cast<int>(now.Graphics.ShadowQualityProfile), 2);
            wanted.Graphics.ShadowQualityProfile = static_cast<DoorwaysShadowQualityProfileId>(step != 0 ? std::clamp(level + step, 0, 2) : (level + 1) % 3);
            break;
        }
        case RowMaster: wanted.Audio.MasterMuted = false; wanted.Audio.MasterVolume = Step(now.Audio.MasterMuted ? 0 : now.Audio.MasterVolume, step, .05f, 0, 1); break;
        case RowMusic: wanted.Audio.MusicMuted = false; wanted.Audio.MusicVolume = Step(now.Audio.MusicMuted ? 0 : now.Audio.MusicVolume, step, .05f, 0, 1); break;
        case RowEffects: wanted.Audio.EffectsMuted = false; wanted.Audio.EffectsVolume = Step(now.Audio.EffectsMuted ? 0 : now.Audio.EffectsVolume, step, .05f, 0, 1); break;
        case RowMenus: wanted.Audio.InterfaceMuted = false; wanted.Audio.InterfaceVolume = Step(now.Audio.InterfaceMuted ? 0 : now.Audio.InterfaceVolume, step, .05f, 0, 1); break;
        case RowMouse:
            wanted.Controls.MouseSensitivity = Step(now.Controls.MouseSensitivity, step, Doorways::DoorwaysMouseSensitivityStep,
                Doorways::DoorwaysMouseSensitivityMinimum, Doorways::DoorwaysMouseSensitivityMaximum);
            break;
        case RowInvert: wanted.Controls.InvertMouseY = !now.Controls.InvertMouseY; break;
        case RowScale: wanted.Interface.UiScale = std::clamp(now.Interface.UiScale + .25f * static_cast<float>(step != 0 ? step : 1), 1.f, 2.f); break;
        case RowFrames: wanted.Interface.ShowPerformance = !now.Interface.ShowPerformance; break;
        case RowDefaults:
            if (enter)
            {   // everything but the window, which would have to go on trial
                const DoorwaysPlayerPreferences fresh;
                wanted = fresh;
                wanted.Graphics.WindowMode = now.Graphics.WindowMode;
                wanted.Graphics.WindowedClientWidth = now.Graphics.WindowedClientWidth; wanted.Graphics.WindowedClientHeight = now.Graphics.WindowedClientHeight;
                gMode = now.Graphics.WindowMode; gWidth = now.Graphics.WindowedClientWidth; gHeight = now.Graphics.WindowedClientHeight;
            }
            else sound = false;
            break;
        case RowBack:
            if (enter) { if (!gForced) gOpen = false; Sound::PlayUi("ui/close", .7f); }
            return;
        default: break;
        }
        if (wanted != now) link.Ask(wanted);
        if (sound && (Entries[gRow].Type != Slider || wanted != now)) Sound::PlayUi(Entries[gRow].Type == Slider ? "ui/move" : "ui/page", Entries[gRow].Type == Slider ? .5f : .6f);
    }
}
