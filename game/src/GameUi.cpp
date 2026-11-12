#define NOMINMAX
#include "GameUi.h"

#include "DoorwaysGamepad.h"
#include "DoorwaysHudTextLayout.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRenderContexts.h"
#include "DoorwaysUiTheme.h"
#include "HudTextRenderer.h"
#include "Renderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <sstream>

namespace CaucasianEagle
{
    using namespace Doorways;

    namespace
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gAtlas;
        constexpr float ShadowAlpha = .78f;   // text shadow strength
        constexpr float KeyTextPixels = .092f; // key label font size per unit of key height

        Material Paint(const DoorwaysHudRenderContext& context, Color c)
        {
            return {{c.R, c.G, c.B, c.A}, {1, 1, 0, 0}, context.Renderer.mWhiteTextureView.Get(), {0, 0, 0, 0}, 1, 1};
        }
    }

    bool Ui::LoadArt(Renderer& renderer, std::string& error)
    {
        if (gAtlas) return true;
        unsigned width = 0, height = 0;
        if (!renderer.LoadUiTexture(HudArt::AtlasPath, gAtlas, width, height))
        {
            error = std::string("HUD artwork is missing: ") + HudArt::AtlasPath;
            return false;
        }
        return true;
    }

    namespace Pointer
    {
        namespace
        {
            struct Zone { int Group, Index; float X, Y, W, H; };
            std::vector<Zone> gZones;
        }
        void Clear() { gZones.clear(); }
        void Add(int group, int index, float x, float y, float w, float h) { gZones.push_back({group, index, x, y, w, h}); }
        int Hit(int group, float x, float y)
        {
            for (const Zone& z : gZones)
                if (z.Group == group && x >= z.X && x <= z.X + z.W && y >= z.Y && y <= z.Y + z.H) return z.Index;
            return -1;
        }
    }

    Ui::Ui(const DoorwaysHudRenderContext& context, HudTextRenderer& text) : mContext(context), mText(text)
    {
        Pointer::Clear();
        mClientWidth = context.ClientWidth; mClientHeight = context.ClientHeight;
        W = static_cast<float>(mClientWidth); H = static_cast<float>(mClientHeight);
        S = CalculateDoorwaysUiScale({mClientWidth, mClientHeight, context.Dpi, context.Interface.UiScale});
        using Clock = std::chrono::steady_clock;
        static const Clock::time_point start = Clock::now();
        static float last = 0;
        Time = std::chrono::duration<float>(Clock::now() - start).count();
        Delta = std::clamp(Time - last, 0.f, .1f);
        last = Time;
    }

    // ---------------------------------------------------------------- shapes
    void Ui::Rect(float x, float y, float w, float h, Color color)
    {
        if (w <= 0 || h <= 0 || color.A <= 0) return;
        if (mClipped)
        {
            const float x1 = std::min(x + w, mClip.Right()), y1 = std::min(y + h, mClip.Bottom());
            x = std::max(x, mClip.X); y = std::max(y, mClip.Y); w = x1 - x; h = y1 - y;
            if (w <= 0 || h <= 0) return;
        }
        mContext.Renderer.DrawScreenRect(x, y, w, h, mClientWidth, mClientHeight, Paint(mContext, color));
    }

    void Ui::Sprite(const HudArt::Sprite& sprite, float x, float y, float w, float h, Color color, float rotation)
    {
        if (!gAtlas || w <= 0 || h <= 0 || color.A <= 0) return;
        if (mClipped && !mClip.Contains({x + w * .5f, y + h * .5f})) return;
        // the solid shape is the sprite without its margin; the margin keeps the same scale
        const float mx = sprite.Margin * w / (sprite.W - 2 * sprite.Margin), my = sprite.Margin * h / (sprite.H - 2 * sprite.Margin);
        mContext.Renderer.DrawUiSprite(gAtlas.Get(), {x - mx, y - my, w + 2 * mx, h + 2 * my}, {sprite.X, sprite.Y, sprite.W, sprite.H},
            {color.R, color.G, color.B, color.A}, rotation);
    }

    void Ui::Icon(const HudArt::Sprite& sprite, float centreX, float centreY, float size, Color color, float rotation)
    {
        Sprite(sprite, centreX - size * .5f, centreY - size * .5f, size, size, color, rotation);
    }

    void Ui::Line(float x0, float y0, float x1, float y1, float thickness, Color color)
    {
        const float dx = x1 - x0, dy = y1 - y0, length = std::sqrt(dx * dx + dy * dy);
        if (!gAtlas || length < .01f || color.A <= 0) return;
        const HudArt::Sprite& p = HudArt::Pixel;                          // its middle: the edges would blend into the atlas
        mContext.Renderer.DrawUiSprite(gAtlas.Get(), {(x0 + x1 - length) * .5f, (y0 + y1 - thickness) * .5f, length, thickness},
            {p.X + 4, p.Y + 4, p.W - 8, p.H - 8}, {color.R, color.G, color.B, color.A}, std::atan2(dy, dx));
    }

    void Ui::Image(ID3D11ShaderResourceView* texture, float sourceWidth, float sourceHeight, float x, float y, float w, float h, Color color)
    {
        if (!texture || w <= 0 || h <= 0 || color.A <= 0) return;
        mContext.Renderer.DrawUiSprite(texture, {x, y, w, h}, {0, 0, sourceWidth, sourceHeight}, {color.R, color.G, color.B, color.A});
    }

    void Ui::Nine(const HudArt::Sprite& sprite, float x, float y, float w, float h, Color color)
    {
        if (!gAtlas || w <= 0 || h <= 0 || color.A <= 0) return;
        const float unit = S / HudArt::TexelsPerUnit;                     // screen pixels per texel
        const float margin = sprite.Margin * unit;
        const float ox = x - margin, oy = y - margin, ow = w + 2 * margin, oh = h + 2 * margin;
        const float b = std::min({sprite.Border * unit, ow * .5f, oh * .5f}); // screen size of an edge piece
        const float t = sprite.Border;                                     // its size in texels
        const float dx[4] = {ox, ox + b, ox + ow - b, ox + ow}, dy[4] = {oy, oy + b, oy + oh - b, oy + oh};
        const float sx[4] = {sprite.X, sprite.X + t, sprite.X + sprite.W - t, sprite.X + sprite.W};
        const float sy[4] = {sprite.Y, sprite.Y + t, sprite.Y + sprite.H - t, sprite.Y + sprite.H};
        const DirectX::XMFLOAT4 tint{color.R, color.G, color.B, color.A};
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                mContext.Renderer.DrawUiSprite(gAtlas.Get(),
                    {dx[column], dy[row], dx[column + 1] - dx[column], dy[row + 1] - dy[row]},
                    {sx[column], sy[row], sx[column + 1] - sx[column], sy[row + 1] - sy[row]}, tint);
    }

    void Ui::Three(const HudArt::Sprite& sprite, float x, float y, float w, float h, Color color)
    {
        if (!gAtlas || w <= 0 || h <= 0 || color.A <= 0) return;
        const float unit = h / (sprite.H - 2 * sprite.Margin);            // the bar keeps its height; the ends keep their shape
        const float margin = sprite.Margin * unit;
        const float ox = x - margin, oy = y - margin, ow = w + 2 * margin, oh = h + 2 * margin;
        const float b = std::min(sprite.Border * unit, ow * .5f), t = sprite.Border;
        const float dx[4] = {ox, ox + b, ox + ow - b, ox + ow};
        const float sx[4] = {sprite.X, sprite.X + t, sprite.X + sprite.W - t, sprite.X + sprite.W};
        const DirectX::XMFLOAT4 tint{color.R, color.G, color.B, color.A};
        for (int column = 0; column < 3; ++column)
            mContext.Renderer.DrawUiSprite(gAtlas.Get(), {dx[column], oy, dx[column + 1] - dx[column], oh},
                {sx[column], sprite.Y, sx[column + 1] - sx[column], sprite.H}, tint);
    }

    void Ui::Bar(float x, float y, float w, float h, float fraction, Color fill, float trail, bool thin)
    {
        const HudArt::Sprite& frame = thin ? HudArt::BarFrameThin : HudArt::BarFrame;
        const float rim = h * (thin ? 2.6f : 4.f) / (frame.H - 2 * frame.Margin);  // rim thickness of the frame art
        const float ix = x + rim, iy = y + rim, iw = w - 2 * rim, ih = h - 2 * rim;
        fraction = std::clamp(fraction, 0.f, 1.f); trail = std::clamp(trail, 0.f, 1.f);
        Rect(x + rim * .5f, y + rim * .5f, w - rim, h - rim, Palette::Well.Fade(fill.A));
        if (trail > fraction) Rect(ix + iw * fraction, iy, iw * (trail - fraction), ih, Color{1.f, .93f, .78f, .85f * fill.A});
        if (fraction > 0)
        {
            Sprite(HudArt::BarFill, ix, iy, iw * fraction, ih, fill);
            Rect(ix + iw * fraction - 1.5f * S, iy, 1.5f * S, ih, Palette::White.Fade(.55f * fill.A)); // bright leading edge
        }
        Three(frame, x, y, w, h, Palette::White.Fade(fill.A));
    }

    float Ui::KeyWidth(const wchar_t* label, float height) const
    {
        return std::max(height * 1.15f, Width(label, height * KeyTextPixels) + height * .62f);
    }

    float Ui::Key(const wchar_t* label, float centreX, float y, float height, Color tint)
    {
        const float pixels = height * KeyTextPixels, w = KeyWidth(label, height);
        Three(HudArt::Keycap, centreX - w * .5f, y, w, height, tint);
        const float tw = Width(label, pixels);
        // the key face sits above the middle of the art (the skirt is at the bottom)
        Text(label, centreX - tw * .5f, y + (height * .86f - Height(pixels)) * .5f, pixels, Palette::Ink.Fade(tint.A));
        return w;
    }

    // ---------------------------------------------------------------- text
    float Ui::Height(float pixels) const { return DoorwaysHudTextHeight(pixels); }
    float Ui::Width(const std::wstring& value, float pixels) const { return MeasureDoorwaysHudTextWidth(value.c_str(), pixels); }

    void Ui::Text(const std::wstring& value, float x, float y, float pixels, Color color)
    {
        if (value.empty() || color.A <= 0.01f) return;
        x = std::round(x); y = std::round(y);
        const DoorwaysUiRect* clip = mClipped ? &mClip : nullptr;
        if (mClipped && (y > mClip.Bottom() || y + Height(pixels) < mClip.Y)) return;
        const float offset = std::max(1.f, std::round(pixels * .42f));
        mText.DrawTextIn(mContext.Renderer, value.c_str(), x + offset, y + offset, pixels, mClientWidth, mClientHeight,
            Paint(mContext, Palette::Black.Fade(ShadowAlpha * color.A)), clip);
        mText.DrawTextIn(mContext.Renderer, value.c_str(), x, y, pixels, mClientWidth, mClientHeight, Paint(mContext, color), clip);
    }

    void Ui::Centered(const std::wstring& value, float centreX, float y, float pixels, Color color)
    {
        Text(value, centreX - Width(value, pixels) * .5f, y, pixels, color);
    }

    void Ui::RightAligned(const std::wstring& value, float right, float y, float pixels, Color color)
    {
        Text(value, right - Width(value, pixels), y, pixels, color);
    }

    float Ui::SpacedWidth(const std::wstring& value, float pixels, float tracking) const
    {
        if (value.empty()) return 0;
        return Width(value, pixels) + tracking * static_cast<float>(value.size() - 1);
    }

    void Ui::Spaced(const std::wstring& value, float x, float y, float pixels, float tracking, Color color)
    {
        wchar_t letter[2] = {0, 0};
        for (const wchar_t c : value)
        {
            letter[0] = c;
            if (c != L' ') Text(letter, x, y, pixels, color);
            x += Width(letter, pixels) + tracking;
        }
    }

    void Ui::SpacedCentered(const std::wstring& value, float centreX, float y, float pixels, float tracking, Color color)
    {
        Spaced(value, centreX - SpacedWidth(value, pixels, tracking) * .5f, y, pixels, tracking, color);
    }

    std::vector<std::wstring> Ui::Wrap(const std::wstring& value, float pixels, float maxWidth) const
    {
        std::vector<std::wstring> lines; std::wstring line, word;
        std::wistringstream in(value);
        while (in >> word)
        {
            const std::wstring candidate = line.empty() ? word : line + L" " + word;
            if (!line.empty() && Width(candidate, pixels) > maxWidth) { lines.push_back(line); line = word; }
            else line = candidate;
        }
        if (!line.empty()) lines.push_back(line);
        return lines;
    }

    float Ui::Paragraph(const std::wstring& value, float x, float y, float pixels, float maxWidth, float lineGap, Color color)
    {
        for (const auto& line : Wrap(value, pixels, maxWidth)) { Text(line, x, y, pixels, color); y += Height(pixels) + lineGap; }
        return y;
    }

    const wchar_t* Ui::KeyLabel(const wchar_t* keyboard, int virtualKey)
    {
        if (!IsDoorwaysGamepadInUse()) return keyboard;
        const wchar_t* button = GetDoorwaysGamepadKeyLabel(virtualKey);
        return button ? button : keyboard;
    }

    std::wstring Ui::Upper(std::wstring value)
    {
        for (auto& c : value) c = static_cast<wchar_t>(std::towupper(c));
        return value;
    }

    const wchar_t* Ui::Roman(int number)
    {
        static const wchar_t* numerals[] = {L"", L"I", L"II", L"III", L"IV", L"V", L"VI", L"VII", L"VIII", L"IX", L"X", L"XI", L"XII"};
        return numerals[std::clamp(number, 0, 12)];
    }
}
