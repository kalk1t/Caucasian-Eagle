#include "StartScreen.h"

#include "DoorwaysGame.h"
#include "DoorwaysGamepad.h"
#include "DoorwaysRenderContexts.h"
#include "GameConstants.h"
#include "GameUi.h"
#include "Guide.h"
#include "Journey.h"
#include "Renderer.h"
#include "Settings.h"
#include "Sound.h"

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

// The start screen, drawn by the game in the same panel, gold and ink as Nyxara's windows and her menu.
// The engine calls Draw every frame while its main menu's root would show (DoorwaysGameSession::OwnStartScreen);
// the game's Update does not run then, so the keys and the mouse are read here.
namespace CaucasianEagle::StartScreen
{
    using Doorways::GameSession;
    using Doorways::IsDoorwaysKeyDown;

    namespace
    {
        constexpr int Rows = 5;                     // play, settings, guide, credits, leave
        constexpr int KeyCount = 11;
        const int Keys[KeyCount] = {'W', VK_UP, 'S', VK_DOWN, VK_RETURN, VK_SPACE, VK_ESCAPE, 'A', VK_LEFT, 'D', VK_RIGHT};

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gPicture;
        unsigned gPictureWidth = 0, gPictureHeight = 0;
        bool gPictureTried = false;

        int gRow = 0;
        bool gGuide = false;                        // the guide is open over the menu (Credits opens its last page)
        int gGuidePage = 0;
        float gGuideFade = 0;
        bool gKeys[KeyCount] = {};
        bool gMouse = false;
        float gMouseX = -1, gMouseY = -1;
        bool gArmed = false;                        // keys held when the screen appeared must be let go first
        bool gForced = false, gForcedRead = false;  // capture mode
    }

    void Draw(const DoorwaysHudRenderContext& context, HudTextRenderer& text, bool focused)
    {
        using namespace Palette;
        auto& session = GameSession();
        Ui ui(context, text);
        const float S = ui.S;
        const bool shown = focused;                 // the window is in front

        if (!gForcedRead)
        {   // testing: CAUCASIAN_EAGLE_GUIDE=N on the start screen holds the guide open on page N
            gForcedRead = true;
            char value[16] = {}; size_t length = 0;
            if (getenv_s(&length, value, GuideEnvironmentVariable) == 0 && length > 1 && std::atoi(value) > 0)
            { gForced = true; gGuide = true; gGuidePage = std::clamp(std::atoi(value) - 1, 0, Guide::PageCount - 1); }
        }

        // ---- the picture, scaled to cover the screen
        if (!gPictureTried)
        {
            gPictureTried = true;
            context.Renderer.LoadUiTexture(StartScreenPicture, gPicture, gPictureWidth, gPictureHeight);
        }
        ui.Rect(0, 0, ui.W, ui.H, Black);
        if (gPicture && gPictureWidth > 0 && gPictureHeight > 0)
        {
            const float scale = (std::max)(ui.W / static_cast<float>(gPictureWidth), ui.H / static_cast<float>(gPictureHeight));
            const float w = static_cast<float>(gPictureWidth) * scale, h = static_cast<float>(gPictureHeight) * scale;
            ui.Image(gPicture.Get(), static_cast<float>(gPictureWidth), static_cast<float>(gPictureHeight), (ui.W - w) * .5f, (ui.H - h) * .5f, w, h, White);
        }

        // ---- input (the settings window, while open, takes it all)
        const bool settings = Settings::IsOpen();
        if (settings) { focused = false; gArmed = false; }
        bool pressed[KeyCount] = {};
        bool anyDown = false;
        for (int i = 0; i < KeyCount; ++i)
        {
            const bool down = focused && IsDoorwaysKeyDown(Keys[i]);
            anyDown = anyDown || down;
            pressed[i] = gArmed && down && !gKeys[i];
            gKeys[i] = down;
        }
        const bool mouseDown = focused && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const bool clicked = gArmed && mouseDown && !gMouse;
        gMouse = mouseDown;
        if (!anyDown && !mouseDown && !settings) gArmed = true;
        float mx = -1, my = -1;
        if (focused)
        {
            POINT point{};
            if (GetCursorPos(&point) && ScreenToClient(GetForegroundWindow(), &point)) { mx = static_cast<float>(point.x); my = static_cast<float>(point.y); }
        }

        const bool moved = std::abs(mx - gMouseX) + std::abs(my - gMouseY) > .5f;   // resting on a row, the pointer leaves W and S alone
        gMouseX = mx; gMouseY = my;

        // ---- the panel at the left
        constexpr float Width = 450, Row = 50, Gap = 6, Pad = 18, Head = 150, Foot = 46, Tiny = 1.55f, Small = 2.05f, Title = 3.1f;
        struct Entry { const wchar_t* Label; const wchar_t* Note; };
        const bool progress = GetJourney().HasProgress();
        const Entry entries[Rows] = {
            {progress ? L"Continue the journey" : L"Begin the journey", progress ? L"last arena won" : L"the road to Telavi"},
            {L"Settings", L"picture, sound, mouse"}, {L"Guide", L"how to play, what it needs"},
            {L"Credits", L"who made it"}, {L"Leave the game", L""}};
        const float w = Width * S, h = (Head + Rows * (Row + Gap) + Foot) * S;
        const float x = 64 * S, y = (ui.H - h) * .46f;
        ui.Rect(x, y, w, h, Black.Fade(.45f));
        ui.Nine(HudArt::Panel, x, y, w, h, White);
        ui.Icon(HudArt::IconWings, x + w * .5f, y + 40 * S, 46 * S, Silver);
        ui.SpacedCentered(L"CAUCASIAN EAGLE", x + w * .5f, y + 70 * S, Title * S, 3 * S, GoldLight);
        ui.Centered(L"The road to Amirani", x + w * .5f, y + 106 * S, Small * S, Dim);
        ui.Rect(x + Pad * S, y + 134 * S, w - 2 * Pad * S, 1 * S, Gold.Fade(.35f));

        int take = -1;
        float cy = y + Head * S;
        for (int i = 0; i < Rows; ++i)
        {
            const float cx = x + Pad * S, cw = w - 2 * Pad * S, ch = Row * S;
            if (!gGuide && mx >= cx && mx <= cx + cw && my >= cy && my <= cy + ch)
            {
                if ((moved || clicked) && gRow != i) { gRow = i; Sound::PlayUi("ui/move", .5f); }
                if (clicked) take = i;
            }
            const bool selected = i == gRow;
            ui.Rect(cx, cy, cw, ch, Well.Fade(selected ? .92f : .6f));
            const Color edge = selected ? GoldLight.Fade(.75f + .25f * std::sin(ui.Time * 4)) : Gold.Fade(.25f);
            const float line = (selected ? 2 : 1) * S;
            ui.Rect(cx, cy, cw, line, edge); ui.Rect(cx, cy + ch - line, cw, line, edge);
            ui.Rect(cx, cy, line, ch, edge); ui.Rect(cx + cw - line, cy, line, ch, edge);
            ui.Icon(HudArt::Diamond, cx + 20 * S, cy + ch * .5f, (selected ? 13 : 9) * S, (selected ? GoldLight : Faint));
            const Color ink = i == Rows - 1 ? (selected ? Red : Dim) : (selected ? Ink : Dim);
            ui.Text(entries[i].Label, cx + 40 * S, cy + (ch - ui.Height(Small * S)) * .5f, Small * S, ink);
            ui.RightAligned(entries[i].Note, cx + cw - 14 * S, cy + (ch - ui.Height(Tiny * S)) * .5f, Tiny * S, selected ? Dim : Faint);
            cy += (Row + Gap) * S;
        }
        ui.Centered(L"W  S  or the mouse      ENTER  take", x + w * .5f, cy + 12 * S, Tiny * S, Dim);

        // ---- who made it, always in sight
        const float by = ui.H - 58 * S;
        ui.Rect(0, by - 10 * S, ui.W, 68 * S, Black.Fade(.5f));
        ui.Spaced(L"A GAME BY GIORGI SAMKHARADZE", 64 * S, by, Tiny * S, 2.5f * S, GoldLight);
        ui.Text(L"Built in Doorways, a custom engine founded by Giorgi Samkharadze in February 2026", 64 * S, by + 18 * S, Tiny * S, Dim);
        ui.RightAligned(std::wstring(L"version ") + std::wstring(GameVersion, GameVersion + std::char_traits<char>::length(GameVersion)), ui.W - 64 * S, by + 18 * S, Tiny * S, Faint);

        // ---- keys
        if (gGuide)
        {
            const int turn = (pressed[9] || pressed[10] ? 1 : 0) - (pressed[7] || pressed[8] ? 1 : 0);
            const int page = std::clamp(gGuidePage + turn, 0, Guide::PageCount - 1);
            if (page != gGuidePage) { gGuidePage = page; Sound::PlayUi("ui/page", .6f); }
            if ((pressed[6] || pressed[4]) && !gForced) { gGuide = false; Sound::PlayUi("ui/close", .7f); }
        }
        else
        {
            const int move = (pressed[2] || pressed[3] ? 1 : 0) - (pressed[0] || pressed[1] ? 1 : 0);
            if (move != 0) { gRow = (gRow + move + Rows) % Rows; Sound::PlayUi("ui/move", .5f); }
            if (pressed[4] || pressed[5]) take = gRow;
            using Start = Doorways::DoorwaysGameSession::Start;
            switch (take)
            {
            case 0: session.StartRequest = Start::Play; gArmed = false; break;
            case 1: Settings::Open(); gArmed = false; break;                  // the game's own window; the engine's screens are not used
            case 2: gGuide = true; gGuidePage = 0; Sound::PlayUi("ui/open", .7f); break;
            case 3: gGuide = true; gGuidePage = Guide::PageCount - 1; Sound::PlayUi("ui/open", .7f); break;   // the last page says who made it
            case 4: session.StartRequest = Start::Exit; break;
            default: break;
            }
        }
        gGuideFade = std::clamp(gGuideFade + (gGuide ? 1.f : -1.f) * ui.Delta / JournalFadeSeconds, 0.f, 1.f);
        Guide::Draw(ui, gGuidePage, gGuideFade);
        Settings::Draw(ui, shown, true);
    }
}
