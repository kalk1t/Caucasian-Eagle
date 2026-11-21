#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "AscentMap.h"

#include "DoorwaysRuntimePathResolver.h"
#include "HudAtlas.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

using namespace DirectX;

namespace CaucasianEagle
{
    namespace
    {
        constexpr const char* MapPicture = "assets/ui/Map/AscentMap.jpg";
        constexpr const char* MapExtent = "assets/ui/Map/AscentMap.txt";
        constexpr float PanelWidth = 318;       // layout units: the panel at the right
        constexpr float Tiny = 1.55f, Small = 1.9f, Name = 2.4f;
        constexpr Color Sea{24 / 255.f, 46 / 255.f, 64 / 255.f, 1};   // (the picture's own edge: build_ascent_map.py's deep sea at 0.7)
        constexpr Color WayGold{1.f, .80f, .38f, 1}, RingLine{.93f, .90f, .78f, 1}, CaveLine{.86f, .42f, .26f, 1};
        constexpr Color GroupRed{.92f, .30f, .26f, 1}, CaveOrange{1.f, .58f, .24f, 1};

        // a line kept inside a rectangle (Liang-Barsky); false: none of it is
        bool ClipLine(float& x0, float& y0, float& x1, float& y1, float l, float t, float r, float b)
        {
            const float dx = x1 - x0, dy = y1 - y0;
            float u0 = 0, u1 = 1;
            const float p[4] = {-dx, dx, -dy, dy}, q[4] = {x0 - l, r - x0, y0 - t, b - y0};
            for (int i = 0; i < 4; ++i)
            {
                if (std::abs(p[i]) < 1e-6f) { if (q[i] < 0) return false; continue; }
                const float u = q[i] / p[i];
                if (p[i] < 0) u0 = (std::max)(u0, u); else u1 = (std::min)(u1, u);
                if (u0 > u1) return false;
            }
            const float ax = x0 + dx * u0, ay = y0 + dy * u0, bx = x0 + dx * u1, by = y0 + dy * u1;
            x0 = ax; y0 = ay; x1 = bx; y1 = by;
            return true;
        }

        std::wstring Metres(float m)
        {
            const int v = static_cast<int>(std::lround(m));
            std::wstring s = std::to_wstring(std::abs(v));
            for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), L",");
            return (v < 0 ? L"-" : L"") + s + L" m";
        }
    }

    bool AscentMap::Load(Renderer& renderer, std::string& error)
    {
        if (mPicture) return true;
        std::ifstream file(Doorways::ResolveDoorwaysRuntimePath(MapExtent));
        std::string line; bool extent = false;
        while (std::getline(file, line))
        {
            std::istringstream in(line); std::string tag; in >> tag;
            if (tag == "EXTENT") extent = static_cast<bool>(in >> mX0 >> mX1 >> mZ0 >> mZ1);
        }
        if (!extent || mX1 <= mX0 || mZ1 <= mZ0) { error = std::string("the mountain's map is missing or damaged: ") + MapExtent; return false; }
        unsigned width = 0, height = 0;
        if (!renderer.LoadUiTexture(MapPicture, mPicture, width, height)) { error = std::string("the mountain's map picture is missing: ") + MapPicture; return false; }
        mW = static_cast<float>(width); mH = static_cast<float>(height);
        return true;
    }

    float AscentMap::FitScale() const
    {   // the whole picture in the map's area
        return (std::min)(mAreaW / (mX1 - mX0), mAreaH / (mZ1 - mZ0)) * .96f;
    }

    void AscentMap::Clamp()
    {
        mWantScale = std::clamp(mWantScale, FitScale(), MostScale());
        // the middle stays over the picture: at the fit it is the picture's middle
        const float halfW = mAreaW * .5f / mWantScale, halfH = mAreaH * .5f / mWantScale;
        const float mx = (mX0 + mX1) * .5f, mz = (mZ0 + mZ1) * .5f;
        // (taken out to the whole it stays in the middle; closer in, its edge may come halfway across the screen)
        const float keep = .9f - .45f * std::clamp((mWantScale / FitScale() - 1) / 1.5f, 0.f, 1.f);
        const float roomX = (std::max)(0.f, (mX1 - mX0) * .5f - halfW * keep), roomZ = (std::max)(0.f, (mZ1 - mZ0) * .5f - halfH * keep);
        mWantX = std::clamp(mWantX, mx - roomX, mx + roomX);
        mWantZ = std::clamp(mWantZ, mz - roomZ, mz + roomZ);
    }

    void AscentMap::Show(const XMFLOAT3& her)
    {
        if (!mPicture) return;   // (no picture, no extent: the game shows the mountain in 3D instead)
        Open = true; mDragging = false; mGoX = mGoZ = 0;
        mWantX = mCX = her.x; mWantZ = mCZ = her.z;
        mWantScale = mScale = .55f;   // (about 1.6 km of the mountain across a 1080 p screen's map: her surroundings)
        Clamp(); mCX = mWantX; mCZ = mWantZ; mScale = mWantScale;
    }

    XMFLOAT2 AscentMap::ToScreen(float x, float z) const
    {
        return {mAreaX + mAreaW * .5f + (x - mCX) * mScale, mAreaY + mAreaH * .5f - (z - mCZ) * mScale};
    }

    XMFLOAT2 AscentMap::ToWorld(float sx, float sy) const
    {
        return {mCX + (sx - mAreaX - mAreaW * .5f) / mScale, mCZ - (sy - mAreaY - mAreaH * .5f) / mScale};
    }

    void AscentMap::Update(float x, float y, bool down, bool click, float wheel, float keyX, float keyZ, float zoomKeys, bool home,
                           const XMFLOAT3& her, float realDt, float screenW, float screenH, float S)
    {
        if (!Open) return;
        mAreaX = 0; mAreaY = 0; mAreaW = (std::max)(100.f, screenW - PanelWidth * S); mAreaH = (std::max)(100.f, screenH);
        const bool over = x >= mAreaX && x < mAreaX + mAreaW && y >= mAreaY && y < mAreaY + mAreaH;
        mPointerX = x; mPointerY = y;
        // drag: the world point under the pointer stays under it
        if (click && over) { mDragging = true; mLastX = x; mLastY = y; mGoX = mGoZ = 0; }
        if (mDragging)
        {
            if (!down || x < 0) mDragging = false;
            else
            {
                const float dx = (x - mLastX) / mScale, dz = (y - mLastY) / mScale;
                mWantX -= dx; mWantZ += dz; mCX -= dx; mCZ += dz;
                if (realDt > 1e-4f)
                {
                    const float k = (std::min)(1.f, realDt * 16);
                    mGoX += (-dx / realDt - mGoX) * k; mGoZ += (dz / realDt - mGoZ) * k;
                }
                mLastX = x; mLastY = y;
            }
        }
        else if (std::abs(mGoX) + std::abs(mGoZ) > 1e-3f)
        {   // runs on after a drag, slowing
            mWantX += mGoX * realDt; mWantZ += mGoZ * realDt;
            const float keep = std::exp(-realDt * 4.5f);
            mGoX *= keep; mGoZ *= keep;
        }
        // the wheel (and + / -): closer or further about the pointer (about the middle without one)
        const float notches = wheel + zoomKeys * realDt * 4.f;
        if (notches != 0)
        {
            const float ax = over ? x : mAreaX + mAreaW * .5f, ay = over ? y : mAreaY + mAreaH * .5f;
            const float before = mWantScale;
            mWantScale = std::clamp(mWantScale * std::pow(1.3f, notches), FitScale(), MostScale());
            // the point under the pointer, as the wanted view has it, stays there
            const float px = mWantX + (ax - mAreaX - mAreaW * .5f) / before, pz = mWantZ - (ay - mAreaY - mAreaH * .5f) / before;
            mWantX = px - (ax - mAreaX - mAreaW * .5f) / mWantScale;
            mWantZ = pz + (ay - mAreaY - mAreaH * .5f) / mWantScale;
        }
        if (keyX != 0 || keyZ != 0) { const float speed = 700.f / mWantScale; mWantX += keyX * speed * realDt; mWantZ += keyZ * speed * realDt; mGoX = mGoZ = 0; }
        if (home) { mWantX = her.x; mWantZ = her.z; mGoX = mGoZ = 0; mWantScale = (std::max)(mWantScale, .55f); }
        Clamp();
        // shown: eased to what is wanted (the drag itself is followed at once)
        const float k = (std::min)(1.f, realDt * 12);
        mScale += (mWantScale - mScale) * k;
        if (!mDragging) { mCX += (mWantX - mCX) * k; mCZ += (mWantZ - mCZ) * k; }
    }

    void AscentMap::Draw(Ui& ui, const Data& d)
    {
        if (!Open) return;
        using namespace Palette;
        const float S = ui.S, W = ui.W, Hh = ui.H;
        mAreaW = (std::max)(100.f, W - PanelWidth * S); mAreaH = (std::max)(100.f, Hh);
        const float l = mAreaX, t = mAreaY, r = mAreaX + mAreaW, b = mAreaY + mAreaH;

        // ---- the picture
        ui.Rect(0, 0, W, Hh, Sea);
        if (mPicture)
        {
            const XMFLOAT2 tl = ToWorld(l, t), br = ToWorld(r, b);   // (x left, z top) and (x right, z bottom)
            float u0 = (tl.x - mX0) / (mX1 - mX0) * mW, u1 = (br.x - mX0) / (mX1 - mX0) * mW;
            float v0 = (mZ1 - tl.y) / (mZ1 - mZ0) * mH, v1 = (mZ1 - br.y) / (mZ1 - mZ0) * mH;
            float dl = l, dt = t, dr = r, db = b;
            const float perU = (r - l) / (u1 - u0), perV = (b - t) / (v1 - v0);
            if (u0 < 0) { dl += -u0 * perU; u0 = 0; }
            if (u1 > mW) { dr -= (u1 - mW) * perU; u1 = mW; }
            if (v0 < 0) { dt += -v0 * perV; v0 = 0; }
            if (v1 > mH) { db -= (v1 - mH) * perV; v1 = mH; }
            if (dr > dl && db > dt) ui.ImagePart(mPicture.Get(), {u0, v0, u1 - u0, v1 - v0}, dl, dt, dr - dl, db - dt);
        }

        // ---- lines: the rings, the caves, the way
        const auto path = [&](const std::vector<XMFLOAT3>& line, float thickness, Color c, float dash)
        {
            if (line.size() < 2) return;
            XMFLOAT2 a = ToScreen(line[0].x, line[0].z);
            float run = 0;
            for (size_t i = 1; i < line.size(); ++i)
            {
                const XMFLOAT2 q = ToScreen(line[i].x, line[i].z);
                const float len = std::hypot(q.x - a.x, q.y - a.y);
                if (len < 2.5f && i + 1 < line.size()) continue;   // (far out: many samples to a pixel)
                run += len;
                const bool on = dash <= 0 || std::fmod(run, dash * 2) < dash;
                float x0 = a.x, y0 = a.y, x1 = q.x, y1 = q.y;
                if (on && ClipLine(x0, y0, x1, y1, l, t, r, b)) ui.Line(x0, y0, x1, y1, thickness, c);
                a = q;
            }
        };
        if (d.Rings)
            for (const auto& ring : *d.Rings) { path(ring, 3.6f * S, Black.Fade(.35f), 0); path(ring, 1.6f * S, RingLine.Fade(.85f), 0); }
        for (const auto& cave : d.Caves) { path(cave, 4.5f * S, Black.Fade(.45f), 0); path(cave, 2.6f * S, CaveLine.Fade(.9f), 4 * S); }
        if (d.Trail) { path(*d.Trail, 5.2f * S, Black.Fade(.5f), 0); path(*d.Trail, 2.6f * S, WayGold, 0); }

        // ---- the marks
        const float zoomed = std::clamp((mScale - FitScale()) / (.6f - FitScale() + 1e-4f), 0.f, 1.f);   // 0 the whole, 1 close
        const Mark* hover = nullptr; float hoverD = 18 * S;
        for (const auto& m : d.Marks)
        {
            const XMFLOAT2 p = ToScreen(m.P.x, m.P.z);
            if (p.x < l - 30 * S || p.x > r + 30 * S || p.y < t - 30 * S || p.y > b + 30 * S) continue;
            const float pd = std::hypot(p.x - mPointerX, p.y - mPointerY);
            if (mPointerX >= 0 && mPointerX < r && pd < hoverD) { hoverD = pd; hover = &m; }
            const auto sign = [&](const HudArt::Sprite& icon, float size, Color c)
            {
                ui.Icon(HudArt::Glow, p.x, p.y, size * 2.1f, Black.Fade(.75f));
                ui.Icon(icon, p.x, p.y, size, c);
            };
            switch (m.What)
            {
            case Nest: sign(HudArt::IconWings, 24 * S, Gold); break;
            case NestSaved:
                ui.Icon(HudArt::Glow, p.x, p.y, 70 * S, GoldLight.Fade(.35f + .15f * std::sin(ui.Time * 3)));
                sign(HudArt::IconWings, 28 * S, GoldLight); break;
            case Boss: sign(HudArt::IconSkull, 26 * S, Red); break;
            case BossBeaten: sign(HudArt::IconSkull, 20 * S, Dim); break;
            case Summit: sign(HudArt::IconMountain, 28 * S, White); break;
            case Start: sign(HudArt::IconAnchor, 22 * S, Moon); break;
            case Group: case CaveGroup:
            {
                const Color c = m.What == CaveGroup ? CaveOrange : GroupRed;
                const float rr = (4.5f + 1.2f * (std::min)(m.Count, 4)) * S * (.75f + .25f * zoomed);
                ui.Icon(HudArt::Glow, p.x, p.y, rr * 4.f, Black.Fade(.6f));
                ui.Icon(HudArt::Pip, p.x, p.y, rr * 2.f, c);
                if (zoomed > .35f && m.Count > 1) ui.Centered(std::to_wstring(m.Count), p.x + rr * 1.9f, p.y - rr * 2.4f, Tiny * S, c.Fade(zoomed));
                break;
            }
            }
            // names beside the nests when close enough
            if ((m.What == Nest || m.What == NestSaved || m.What == Summit) && zoomed > .25f)
                ui.Text(m.Title, p.x + 18 * S, p.y - 9 * S, Tiny * S, (m.What == Summit ? White : GoldLight).Fade(.85f * zoomed));
        }

        // ---- her: the way she faces, a ring that breathes
        {
            const XMFLOAT2 p = ToScreen(d.Her.x, d.Her.z);
            const XMFLOAT2 c{std::clamp(p.x, l + 16 * S, r - 16 * S), std::clamp(p.y, t + 16 * S, b - 16 * S)};
            const bool inside = c.x == p.x && c.y == p.y;
            const float pulse = .5f + .5f * std::sin(ui.Time * 3.2f);
            if (inside)
            {   // the camera's view as a pale wedge before her
                const float a0 = d.LookYaw - .45f, a1 = d.LookYaw + .45f, reach = 46 * S;
                ui.Line(p.x, p.y, p.x + std::sin(a0) * reach, p.y - std::cos(a0) * reach, 1.5f * S, Soul.Fade(.45f));
                ui.Line(p.x, p.y, p.x + std::sin(a1) * reach, p.y - std::cos(a1) * reach, 1.5f * S, Soul.Fade(.45f));
                ui.Icon(HudArt::Glow, p.x, p.y, (46 + 18 * pulse) * S, Soul.Fade(.45f));
                ui.Icon(HudArt::Glow, p.x, p.y, 30 * S, Black.Fade(.7f));
                ui.Icon(HudArt::IconArrow, p.x, p.y, 24 * S, White, d.HerYaw);
            }
            else ui.Icon(HudArt::IconChevron, c.x, c.y, 20 * S, Soul.Fade(.7f + .3f * pulse), std::atan2(p.x - c.x, -(p.y - c.y)));   // (off the map's edge: the way to her)
        }

        // ---- the pointer: what it is over
        if (mPointerX >= l && mPointerX < r && mPointerY >= t && mPointerY < b && !mDragging)
        {
            std::wstring title, line;
            if (hover) { title = hover->Title; line = hover->Line; }
            if (!title.empty())
            {
                const float w = (std::max)(ui.Width(title, Small * S), ui.Width(line, Tiny * S)) + 24 * S;
                const float h = ui.Height(Small * S) + (line.empty() ? 0 : ui.Height(Tiny * S) + 4 * S) + 16 * S;
                float x = mPointerX + 18 * S, y = mPointerY + 14 * S;
                if (x + w > r - 6 * S) x = mPointerX - 18 * S - w;
                if (y + h > b - 6 * S) y = mPointerY - 14 * S - h;
                ui.Nine(HudArt::Panel, x, y, w, h, White);
                ui.Text(title, x + 12 * S, y + 8 * S, Small * S, GoldLight);
                if (!line.empty()) ui.Text(line, x + 12 * S, y + 10 * S + ui.Height(Small * S), Tiny * S, Ink);
            }
        }

        // ---- the north, and the scale
        {
            const float x = l + 40 * S, y = t + 92 * S;
            ui.Icon(HudArt::Glow, x, y, 64 * S, Black.Fade(.55f));
            ui.Icon(HudArt::IconArrow, x, y + 4 * S, 22 * S, White, 0);
            ui.Centered(L"N", x, y - 32 * S, Small * S, White);
            // a round length that is 80 to 200 px long
            float metres = 10;
            for (const float step : {10.f, 20.f, 50.f, 100.f, 200.f, 250.f, 500.f, 1000.f, 2000.f}) { metres = step; if (step * mScale >= 80 * S) break; }
            const float len = metres * mScale, sx = l + 24 * S, sy = b - 34 * S;
            ui.Rect(sx - 8 * S, sy - 26 * S, len + 16 * S + 70 * S, 40 * S, Black.Fade(.5f));
            ui.Rect(sx, sy, len, 3 * S, White);
            ui.Rect(sx, sy - 6 * S, 2 * S, 9 * S, White); ui.Rect(sx + len - 2 * S, sy - 6 * S, 2 * S, 9 * S, White);
            ui.Text(metres >= 1000 ? std::to_wstring(static_cast<int>(metres / 1000)) + L" km" : std::to_wstring(static_cast<int>(metres)) + L" m", sx + len + 10 * S, sy - 12 * S, Tiny * S, Ink);
        }

        // ---- the title
        {
            ui.Sprite(HudArt::GradV, l, t, mAreaW, 74 * S, Black.Fade(.65f));
            ui.SpacedCentered(L"THE EAGLE'S ASCENT", l + mAreaW * .5f, t + 12 * S, Name * S, 4 * S, Gold);
            ui.Centered(L"Drag: move the map     Wheel: closer, further     Space: to her     Tab: the mountain in 3D     M: close",
                        l + mAreaW * .5f, t + 44 * S, Tiny * S, Ink);
        }

        // ---- the panel: where she is, what the pointer is over, what the signs mean
        {
            const float pw = PanelWidth * S, x = W - pw, pad = 18 * S;
            ui.Rect(x, 0, pw, Hh, {.02f, .03f, .045f, .93f});
            ui.Rect(x, 0, 2 * S, Hh, Gold.Fade(.35f));
            float y = 22 * S;
            const float line = ui.Height(Tiny * S) + 7 * S;
            ui.Spaced(L"NYXARA", x + pad, y, Tiny * S, 3 * S, Gold); y += line;
            ui.Text(Metres((std::max)(0.f, d.Her.y)) + L"  of  " + Metres(d.Summit), x + pad, y, Name * S, Ink); y += ui.Height(Name * S) + 4 * S;
            if (d.ZoneAt) { ui.Text(d.ZoneAt(d.Her.y), x + pad, y, Tiny * S, Dim); y += line; }
            ui.Text(d.Flying ? L"On her wings" : L"On foot", x + pad, y, Tiny * S, Dim); y += line + 10 * S;

            ui.Spaced(L"UNDER THE POINTER", x + pad, y, Tiny * S, 3 * S, Gold); y += line;
            if (mPointerX >= l && mPointerX < r && mPointerY >= t && mPointerY < b)
            {
                const XMFLOAT2 w = ToWorld(mPointerX, mPointerY);
                const float h = d.Land ? d.Land(w.x, w.y) : -1e9f;
                if (h > -1e8f && h > 0) ui.Text(Metres(h) + L" high", x + pad, y, Small * S, Ink);
                else ui.Text(L"The sea", x + pad, y, Small * S, Moon);
                y += ui.Height(Small * S) + 2 * S;
                if (h > 0 && d.ZoneAt) { ui.Text(d.ZoneAt(h), x + pad, y, Tiny * S, Dim); y += line; }
                const float dist = std::hypot(w.x - d.Her.x, w.y - d.Her.z);
                ui.Text(Metres(dist) + L" from her", x + pad, y, Tiny * S, Dim);
            }
            else ui.Text(L"-", x + pad, y, Small * S, Faint);
            y += line + 14 * S;

            ui.Spaced(L"THE SIGNS", x + pad, y, Tiny * S, 3 * S, Gold); y += line + 2 * S;
            const auto legend = [&](const HudArt::Sprite* icon, Color c, const wchar_t* what, int kind)
            {
                const float cx = x + pad + 12 * S, cy = y + ui.Height(Tiny * S) * .5f;
                if (kind == 1) { ui.Rect(cx - 11 * S, cy - 2.5f * S, 22 * S, 5 * S, Black.Fade(.6f)); ui.Rect(cx - 11 * S, cy - 1.3f * S, 22 * S, 2.6f * S, c); }
                else if (kind == 2) { ui.Rect(cx - 11 * S, cy - 1 * S, 9 * S, 2.6f * S, c); ui.Rect(cx + 2 * S, cy - 1 * S, 9 * S, 2.6f * S, c); }
                else if (kind == 3) ui.Icon(HudArt::Pip, cx, cy, 12 * S, c);
                else if (icon) ui.Icon(*icon, cx, cy, 20 * S, c);
                ui.Text(what, x + pad + 32 * S, y, Tiny * S, Ink);
                y += line + 3 * S;
            };
            legend(&HudArt::IconArrow, White, L"Nyxara, the way she faces", 0);
            legend(nullptr, WayGold, L"The way up", 1);
            legend(nullptr, RingLine, L"The rings round the mountain", 1);
            legend(nullptr, CaveLine, L"Caves, under the rock", 2);
            legend(&HudArt::IconWings, Gold, L"Eagle Nest: levels, looks, H saves", 0);
            legend(&HudArt::IconSkull, Red, L"A boss", 0);
            legend(nullptr, GroupRed, L"Creatures waiting", 3);
            legend(nullptr, CaveOrange, L"Creatures in a cave", 3);
            legend(&HudArt::IconMountain, White, L"Amiran's summit", 0);
        }
    }
}
