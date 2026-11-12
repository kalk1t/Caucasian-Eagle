#pragma once

// Drawing layer of the Caucasian Eagle HUD.
//
// One Ui object is made per frame in Game::DrawHud and handed to everything that draws on
// screen (NyxaraHud, Journey, Story). It wraps the engine calls:
//   - artwork from the HUD atlas (HudAtlas.h, built by art/ui/build_hud.py), through Renderer::DrawUiSprite
//   - text in the engine's HUD font, with a soft shadow, letter spacing, wrapping and clipping
// Sizes are given in layout units and multiplied by S (1 unit = 1 px at 1280x720).
// It owns no gameplay state. Colours are in GameUi.h (Palette); sizes are next to the code that uses them.

#include "DoorwaysUiTypes.h"
#include "HudAtlas.h"

#include <string>
#include <vector>

class Renderer;
struct ID3D11ShaderResourceView;
class HudTextRenderer;
struct DoorwaysHudRenderContext;

namespace CaucasianEagle
{
    struct Color
    {
        float R = 1, G = 1, B = 1, A = 1;
        constexpr Color Fade(float alpha) const { return {R, G, B, A * alpha}; }
    };

    namespace Palette
    {
        inline constexpr Color Ink{.92f, .96f, .95f, 1};        // main text
        inline constexpr Color Dim{.58f, .68f, .67f, 1};        // secondary text
        inline constexpr Color Faint{.36f, .44f, .44f, 1};      // locked / unavailable
        inline constexpr Color Gold{.90f, .74f, .40f, 1};
        inline constexpr Color GoldLight{1.f, .90f, .64f, 1};
        inline constexpr Color Silver{.80f, .90f, 1.f, 1};
        inline constexpr Color Soul{.30f, 1.f, .58f, 1};        // health, eagle skills
        inline constexpr Color Moon{.52f, .80f, 1.f, 1};        // energy, utility
        inline constexpr Color Red{.96f, .30f, .30f, 1};
        inline constexpr Color Violet{.66f, .36f, 1.f, 1};
        inline constexpr Color Well{.012f, .026f, .028f, 1};    // inside of bars
        inline constexpr Color Black{0, 0, 0, 1};
        inline constexpr Color White{1, 1, 1, 1};
    }

    // Where the pointer can press. Windows add their rows while they draw; the game's update reads them on the
    // next frame (Journey::UpdateJournal). The list is emptied whenever a Ui is made, that is once a frame.
    namespace Pointer
    {
        enum Group { PauseRow, GrowthCard, RuneCard, PageKey };
        void Clear();
        void Add(int group, int index, float x, float y, float w, float h);
        int Hit(int group, float x, float y);   // the index of the zone under (x, y), or -1
    }

    class Ui
    {
    public:
        Ui(const DoorwaysHudRenderContext& context, HudTextRenderer& text);

        // Loads the atlas once. False (with 'error') when the artwork is missing.
        static bool LoadArt(Renderer& renderer, std::string& error);

        float W = 0, H = 0;     // client size in pixels
        float S = 1;            // pixels per layout unit
        float Time = 0;         // real seconds since the first HUD frame
        float Delta = 0;        // real seconds since the previous HUD frame

        // ---- shapes
        void Rect(float x, float y, float w, float h, Color color);
        // Artwork: (x, y, w, h) is where the solid shape goes; its soft shadow is drawn outside that.
        void Sprite(const HudArt::Sprite& sprite, float x, float y, float w, float h, Color color = Palette::White, float rotation = 0);
        void Icon(const HudArt::Sprite& sprite, float centreX, float centreY, float size, Color color = Palette::White, float rotation = 0);
        void Nine(const HudArt::Sprite& sprite, float x, float y, float w, float h, Color color = Palette::White);
        void Three(const HudArt::Sprite& sprite, float x, float y, float w, float h, Color color = Palette::White);
        // A straight line from (x0, y0) to (x1, y1).
        void Line(float x0, float y0, float x1, float y1, float thickness, Color color);
        // A whole picture loaded with Renderer::LoadUiTexture ('sourceWidth' x 'sourceHeight' texels).
        void Image(ID3D11ShaderResourceView* texture, float sourceWidth, float sourceHeight, float x, float y, float w, float h, Color color = Palette::White);
        // Framed bar. 'trail' >= 'fraction' shows what was just lost.
        void Bar(float x, float y, float w, float h, float fraction, Color fill, float trail = 0, bool thin = false);
        // Key badge with its label centred on (centreX, y .. y + height). Returns its width.
        float Key(const wchar_t* label, float centreX, float y, float height, Color tint = Palette::White);
        float KeyWidth(const wchar_t* label, float height) const;

        // ---- text ('pixels' is the engine font size; Height() of it is the line box)
        float Height(float pixels) const;
        float Width(const std::wstring& value, float pixels) const;
        void Text(const std::wstring& value, float x, float y, float pixels, Color color);
        void Centered(const std::wstring& value, float centreX, float y, float pixels, Color color);
        void RightAligned(const std::wstring& value, float right, float y, float pixels, Color color);
        // Letter-spaced capitals for labels and titles ('tracking' in pixels between letters).
        float SpacedWidth(const std::wstring& value, float pixels, float tracking) const;
        void Spaced(const std::wstring& value, float x, float y, float pixels, float tracking, Color color);
        void SpacedCentered(const std::wstring& value, float centreX, float y, float pixels, float tracking, Color color);
        std::vector<std::wstring> Wrap(const std::wstring& value, float pixels, float maxWidth) const;
        // Wrapped paragraph; returns the y under its last line.
        float Paragraph(const std::wstring& value, float x, float y, float pixels, float maxWidth, float lineGap, Color color);
        // Text (and Rect / Sprite centres) outside this rectangle are not drawn; nullptr removes it.
        void SetClip(const Doorways::DoorwaysUiRect* clip) { mClip = clip ? *clip : Doorways::DoorwaysUiRect{}; mClipped = clip != nullptr; }

        // The name to show for a key: 'keyboard', or the controller button bound to 'virtualKey' while a controller is in use.
        static const wchar_t* KeyLabel(const wchar_t* keyboard, int virtualKey);
        static std::wstring Upper(std::wstring value);
        static const wchar_t* Roman(int number); // 1..12

    private:
        const DoorwaysHudRenderContext& mContext;
        HudTextRenderer& mText;
        int mClientWidth = 0, mClientHeight = 0;
        Doorways::DoorwaysUiRect mClip{};
        bool mClipped = false;
    };
}
