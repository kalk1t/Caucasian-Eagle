#define NOMINMAX
#include "WorldMap.h"

#include "DoorwaysRuntimePathResolver.h"
#include "GameConstants.h"
#include "GameUi.h"
#include "Renderer.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace CaucasianEagle::WorldMap
{
    namespace
    {
        constexpr float Pi = 3.14159265f;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gPicture;
        float gWidth = 1, gHeight = 1;                              // picture size in texels
        float gMinX = -1, gMaxX = 1, gMinZ = -1, gMaxZ = 1;         // the world it shows

        // minimap layout, in layout units
        constexpr float MiniSize = 150, MiniTop = 16, MiniRight = 22, MiniLabelGap = 7, MiniLabelPixels = 1.6f;

        std::wstring Metres(float metres)
        {
            if (metres >= 1000)
            {
                const int tenths = static_cast<int>(metres / 100 + .5f);
                return std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10) + L" km";
            }
            return std::to_wstring(static_cast<int>(metres / 10 + .5f) * 10) + L" m";
        }
    }

    bool Load(Renderer& renderer, std::string& error)
    {
        if (gPicture) return true;
        std::ifstream file(Doorways::ResolveDoorwaysRuntimePath(MapFile));
        std::string line; bool extent = false;
        while (std::getline(file, line))
        {
            std::istringstream in(line); std::string tag; in >> tag;
            if (tag == "EXTENT") extent = static_cast<bool>(in >> gMinX >> gMaxX >> gMinZ >> gMaxZ);
        }
        if (!extent || gMaxX <= gMinX || gMaxZ <= gMinZ) { error = std::string("world map is missing or damaged: ") + MapFile; return false; }
        unsigned width = 0, height = 0;
        if (!renderer.LoadUiTexture(MapTexture, gPicture, width, height)) { error = std::string("world map picture is missing: ") + MapTexture; return false; }
        gWidth = static_cast<float>(width); gHeight = static_cast<float>(height);
        return true;
    }

    const HudArt::Sprite& Emblem(int city)
    {
        static const HudArt::Sprite* const emblems[Story::CityCount] = {&HudArt::IconGrapes, &HudArt::IconBaths, &HudArt::IconCross,
            &HudArt::IconFort, &HudArt::IconFleece, &HudArt::IconAnchor, &HudArt::IconTower};
        return *emblems[std::clamp(city, 0, Story::CityCount - 1)];
    }

    float MinimapBottom() { return MiniTop + MiniSize + MiniLabelGap + 16; }

    // ================================================================ minimap
    void DrawMinimap(Ui& ui, const View& v, float fade)
    {
        if (!v.Road || fade <= .01f) return;
        using namespace Palette;
        const float S = ui.S, R = MiniSize * .5f * S, cx = ui.W - MiniRight * S - R, cy = MiniTop * S + R;
        const float scale = R / MinimapRange;                         // pixels per metre
        const float inner = R - 9 * S;                                // what lies beyond this is under the rim
        const float sy = std::sin(v.Look), cy2 = std::cos(v.Look);
        // world -> offset from the centre of the minimap, in pixels (the camera's way is up)
        const auto offset = [&](Point p, float& x, float& y)
        {
            const float dx = p.X - v.Player.X, dz = p.Z - v.Player.Z;
            x = (dx * cy2 - dz * sy) * scale; y = -(dx * sy + dz * cy2) * scale;
        };
        const auto inside = [&](float x, float y, float margin) { return x * x + y * y < (inner - margin) * (inner - margin); };

        ui.Sprite(HudArt::MapDisc, cx - R, cy - R, 2 * R, 2 * R, White.Fade(fade));

        // ---- the road: walked part dim, the way ahead bright
        const auto& road = *v.Road;
        const int count = static_cast<int>(road.size()), here = static_cast<int>(v.Walked / v.Step);
        const int reach = static_cast<int>(MinimapRange * 3 / v.Step);   // the road winds: look this far along it both ways
        for (int i = std::max(0, here - reach); i + 2 < std::min(count, here + reach); i += 2)
        {
            float x0, y0, x1, y1; offset(road[i], x0, y0); offset(road[i + 2], x1, y1);
            if (!inside((x0 + x1) * .5f, (y0 + y1) * .5f, 0)) continue;
            ui.Line(cx + x0, cy + y0, cx + x1, cy + y1, 3.2f * S, (i < here ? Faint : Gold).Fade(fade));
        }
        // ---- cities in range: their emblem on a dark coin
        for (int i = 0; i < Story::CityCount; ++i)
        {
            float x, y; offset(v.Cities[i], x, y);
            if (!inside(x, y, 9 * S)) continue;
            ui.Icon(HudArt::Pip, cx + x, cy + y, 30 * S, Black.Fade(.8f * fade));
            ui.Icon(Emblem(i), cx + x, cy + y, 17 * S, (v.Visited[i] ? GoldLight : Ink).Fade(fade));
        }
        // ---- the next fight, chests, creatures
        if (v.ArenasWon < static_cast<int>(v.Arenas.size()))
        {
            float x, y; offset(v.Arenas[v.ArenasWon], x, y);
            if (inside(x, y, 8 * S)) ui.Icon(HudArt::IconSwords, cx + x, cy + y, 17 * S, (v.Fighting ? Red : GoldLight).Fade(fade));
        }
        for (const Point& chest : v.Chests)
        {
            float x, y; offset(chest, x, y);
            if (inside(x, y, 3 * S)) ui.Icon(HudArt::Diamond, cx + x, cy + y, 9 * S, White.Fade(fade));
        }
        for (const Point& enemy : v.Enemies)
        {
            float x, y; offset(enemy, x, y);
            if (inside(x, y, 3 * S)) ui.Icon(HudArt::Pip, cx + x, cy + y, 9 * S, Red.Fade(fade));
        }
        // ---- Nyxara
        ui.Icon(HudArt::IconArrow, cx, cy, 17 * S, Soul.Fade(fade), v.Heading - v.Look);
        ui.Sprite(HudArt::MapRim, cx - R, cy - R, 2 * R, 2 * R, White.Fade(fade));

        // ---- marks on the rim: what is out of sight, and which way it lies
        const auto rim = [&](Point p, const HudArt::Sprite& art, float size, Color tint, bool turn)
        {
            float x, y; offset(p, x, y);
            const float length = std::sqrt(x * x + y * y);
            if (length < inner - 9 * S) return;                                 // in sight: drawn on the map itself
            const float nx = x / length, ny = y / length, at = R - 3 * S;
            if (!turn) ui.Icon(HudArt::Pip, cx + nx * at, cy + ny * at, size * 1.55f, Black.Fade(.85f * fade));
            ui.Icon(art, cx + nx * at, cy + ny * at, size, tint.Fade(fade), turn ? std::atan2(nx, -ny) : 0.f);
        };
        rim({MountainX, MountainZ}, HudArt::IconMountain, 15 * S, Silver, false);
        if (v.NextCity >= 0) rim(v.Cities[v.NextCity], Emblem(v.NextCity), 14 * S, GoldLight, false);
        if (v.ArenasWon < static_cast<int>(v.Arenas.size())) rim(v.Arenas[v.ArenasWon], HudArt::IconSwords, 14 * S, v.Fighting ? Red : GoldLight, false);
        {   // the road ahead: a chevron where it leaves the minimap
            const int ahead = std::min(count - 1, here + static_cast<int>(MinimapRange * .8f / v.Step));
            float x, y; offset(road[ahead], x, y);
            const float length = std::max(1e-3f, std::sqrt(x * x + y * y));
            const float nx = x / length, ny = y / length, at = R + 9 * S, pulse = .65f + .35f * std::sin(ui.Time * 4);
            if (ahead > here) ui.Icon(HudArt::IconChevron, cx + nx * at, cy + ny * at, 15 * S, GoldLight.Fade(fade * pulse), std::atan2(nx, -ny));
        }

        // ---- under it: where she is, or what comes next
        std::wstring label; Color tint = Dim;
        if (v.City >= 0) { label = Ui::Upper(Story::Cities[v.City].Name); tint = GoldLight; }
        else if (v.NextCity >= 0) label = Ui::Upper(Story::Cities[v.NextCity].Name) + L"  " + Metres(v.NextCityDistance);
        else label = L"THE MOUNTAIN  " + Metres(v.MountainDistance);
        ui.SpacedCentered(label, cx, cy + R + MiniLabelGap * S, MiniLabelPixels * S, 1.5f * S, tint.Fade(fade));
    }

    // ================================================================ world map (M)
    void DrawMap(Ui& ui, const View& v, float a)
    {
        if (!v.Road || !gPicture || a <= .01f) return;
        using namespace Palette;
        const float W = ui.W, H = ui.H, S = ui.S;
        // layout, in layout units
        constexpr float Side = 34, Top = 78, Bottom = 118, LegendWidth = 318, Gap = 22, Tiny = 1.5f, Small = 1.85f, Title = 3.0f;
        ui.Rect(0, 0, W, H, Black.Fade(.86f * a));
        ui.Sprite(HudArt::Vignette, 0, 0, W, H, Black.Fade(.7f * a));
        ui.SpacedCentered(L"THE ROAD TO AMIRANI", W * .5f, 22 * S, Title * S, 6 * S, Ink.Fade(a));
        ui.Sprite(HudArt::Divider, W * .5f - 210 * S, 22 * S + ui.Height(Title * S) + 1 * S, 420 * S, 24 * S, White.Fade(a));

        // ---- the picture, as large as fits beside the legend
        const float aspect = gWidth / gHeight;
        float mw = W - (2 * Side + LegendWidth + Gap) * S, mh = mw / aspect;
        const float room = H - (Top + Bottom) * S;
        if (mh > room) { mh = room; mw = mh * aspect; }
        const float mx = Side * S + (W - (2 * Side + LegendWidth + Gap) * S - mw) * .5f, my = Top * S + (room - mh) * .5f;
        ui.Nine(HudArt::Panel, mx - 5 * S, my - 5 * S, mw + 10 * S, mh + 10 * S, White.Fade(a));
        ui.Image(gPicture.Get(), gWidth, gHeight, mx, my, mw, mh, White.Fade(a));
        const auto at = [&](Point p, float& x, float& y)
        {
            x = mx + (p.Z - gMinZ) / (gMaxZ - gMinZ) * mw; y = my + (p.X - gMinX) / (gMaxX - gMinX) * mh;
        };

        // ---- the road she has walked
        const auto& road = *v.Road;
        const int here = std::min(static_cast<int>(road.size()) - 1, static_cast<int>(v.Walked / v.Step));
        for (int i = 0; i + 4 <= here; i += 4)
        {
            float x0, y0, x1, y1; at(road[i], x0, y0); at(road[i + 4], x1, y1);
            ui.Line(x0, y0, x1, y1, 2.6f * S, Soul.Fade(.9f * a));
        }
        // ---- fights: won, next, still ahead
        for (int i = 0; i < static_cast<int>(v.Arenas.size()); ++i)
        {
            float x, y; at(v.Arenas[i], x, y);
            if (i < v.ArenasWon) ui.Icon(HudArt::Pip, x, y, 9 * S, Soul.Fade(a));
            else if (i == v.ArenasWon)
            {
                ui.Icon(HudArt::Glow, x, y, 46 * S, Red.Fade(a * (.35f + .25f * std::sin(ui.Time * 4))));
                ui.Icon(HudArt::IconSwords, x, y, 19 * S, GoldLight.Fade(a));
            }
        }
        // ---- the ends of the road
        {
            float x, y; at({0, 93}, x, y);
            ui.SpacedCentered(L"THE ROAD BEGINS", std::max(x, mx + 78 * S), y + 52 * S, Tiny * S, 1.5f * S, Silver.Fade(a));
            at({MountainX, MountainZ}, x, y);
            ui.SpacedCentered(L"AMIRANI", std::min(x, mx + mw - 50 * S), y - 150 * S, Small * S, 3 * S, Silver.Fade(a));
        }
        // ---- the seven cities
        for (int i = 0; i < Story::CityCount; ++i)
        {
            float x, y; at(v.Cities[i], x, y);
            const bool inCity = v.City == i, next = v.NextCity == i && v.City < 0;
            if (inCity || next) ui.Icon(HudArt::Glow, x, y, 78 * S, GoldLight.Fade(a * (.30f + .20f * std::sin(ui.Time * 3))));
            ui.Icon(HudArt::Medallion, x, y, 30 * S, White.Fade(a));
            ui.Icon(Emblem(i), x, y, 17 * S, (v.Visited[i] ? GoldLight : Ink).Fade(a));
            const std::wstring name = Ui::Upper(Story::Cities[i].Name);
            const float below = y + 19 * S, tw = ui.SpacedWidth(name, Small * S, 2 * S);
            ui.Sprite(HudArt::Glow, x - tw * .5f - 26 * S, below - 9 * S, tw + 52 * S, ui.Height(Small * S) + 18 * S, Black.Fade(.75f * a));
            ui.SpacedCentered(name, x, below, Small * S, 2 * S, (v.Visited[i] ? GoldLight : Ink).Fade(a));
        }
        // ---- Nyxara
        {
            float x, y; at(v.Player, x, y);
            ui.Icon(HudArt::Glow, x, y, 52 * S, Soul.Fade(a * (.45f + .25f * std::sin(ui.Time * 5))));
            ui.Icon(HudArt::IconArrow, x, y, 21 * S, White.Fade(a), v.Heading + Pi * .5f);   // on the picture +z is right
        }

        // ---- the seven cities, in words
        const float lx = mx + mw + (Gap + 5) * S, lw = W - Side * S - lx, ly = my - 5 * S, lh = mh + 10 * S;
        ui.Nine(HudArt::Panel, lx, ly, lw, lh, White.Fade(a));
        float y = ly + 14 * S;
        ui.SpacedCentered(L"THE SEVEN CITIES", lx + lw * .5f, y, Small * S, 3 * S, Gold.Fade(a));
        y += ui.Height(Small * S) + 10 * S;
        const float rowHeight = (ly + lh - 8 * S - y) / static_cast<float>(Story::CityCount), art = 30 * S, tx = lx + 16 * S + art + 10 * S, tw = lx + lw - 14 * S - tx;
        for (int i = 0; i < Story::CityCount; ++i, y += rowHeight)
        {
            const Story::City& city = Story::Cities[i];
            const bool visited = v.Visited[i], current = v.City == i;
            if (current) ui.Rect(lx + 6 * S, y - 3 * S, lw - 12 * S, rowHeight - 3 * S, Gold.Fade(.10f * a));
            ui.Rect(lx + 14 * S, y - 5 * S, lw - 28 * S, 1 * S, White.Fade(.07f * a));
            ui.Icon(HudArt::Medallion, lx + 16 * S + art * .5f, y + art * .5f + 2 * S, art, White.Fade(a));
            ui.Icon(Emblem(i), lx + 16 * S + art * .5f, y + art * .5f + 2 * S, art * .56f, (visited ? GoldLight : Faint).Fade(a));
            ui.Text(city.Name, tx, y, Small * S, (visited ? Ink : Dim).Fade(a));
            ui.RightAligned(Ui::Upper(city.Land), tx + tw, y + 2 * S, Tiny * S * .9f, Faint.Fade(a));
            float ty = y + ui.Height(Small * S) + 1 * S;
            ui.Text(city.Known, tx, ty, Tiny * S, Dim.Fade(a));
            ty += ui.Height(Tiny * S) + 2 * S;
            if (visited) ui.Paragraph(std::wstring(city.Gift) + L": " + city.GiftText, tx, ty, Tiny * S, tw, 1 * S, Gold.Fade(a));
            else ui.Text(L"Not yet reached.", tx, ty, Tiny * S, Faint.Fade(a));
        }

        // ---- under the picture: the city she is in, or the next one
        const int about = v.City >= 0 ? v.City : v.NextCity;
        float by = my + mh + 16 * S;
        if (about >= 0)
        {
            const Story::City& city = Story::Cities[about];
            const std::wstring heading = Ui::Upper(city.Name) + (v.City >= 0 ? L"" : L"  -  " + Metres(v.NextCityDistance) + L" AHEAD");
            ui.Icon(Emblem(about), mx + 12 * S, by + 12 * S, 24 * S, GoldLight.Fade(a));
            ui.Spaced(heading, mx + 34 * S, by, Small * S, 2.5f * S, GoldLight.Fade(a));
            ui.Paragraph(city.Text, mx + 34 * S, by + ui.Height(Small * S) + 5 * S, Small * S, mw - 40 * S, 2 * S, Ink.Fade(a));
        }
        else
        {
            ui.Icon(HudArt::IconMountain, mx + 12 * S, by + 12 * S, 24 * S, Silver.Fade(a));
            ui.Spaced(L"THE MOUNTAIN  -  " + Metres(v.MountainDistance) + L" AHEAD", mx + 34 * S, by, Small * S, 2.5f * S, Silver.Fade(a));
        }

        // ---- keys
        const float keyHeight = 22 * S, labelPixels = 1.6f * S, fy = H - 40 * S, labelY = fy + (keyHeight * .86f - ui.Height(labelPixels)) * .5f;
        float kx = W - Side * S;
        const auto hint = [&](const wchar_t* label, const wchar_t* key)
        {
            kx -= ui.Width(label, labelPixels);
            ui.Text(label, kx, labelY, labelPixels, Dim.Fade(a));
            const float w = ui.KeyWidth(key, keyHeight);
            kx -= 8 * S + w;
            ui.Key(key, kx + w * .5f, fy, keyHeight, White.Fade(a));
            kx -= 22 * S;
        };
        hint(L"CLOSE", Ui::KeyLabel(L"M", 'M'));
        hint(L"STORY", Ui::KeyLabel(L"F1", VK_F1));
    }
}
