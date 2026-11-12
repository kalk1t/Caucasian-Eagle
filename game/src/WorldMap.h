#pragma once

// The two maps of Caucasian Eagle.
//
//   minimap    always on screen, top right. Round, turned so that the way the camera looks is up:
//              the road, the next fight, chests, creatures, the cities, and marks on the rim that
//              point to the road ahead, the next city and the mountain.
//   world map  the M key. The whole road as a painted picture (assets/ui/Map/WorldMap.png, made by
//              art/map/build_map.py) with the seven cities, the fights and Nyxara drawn over it,
//              and beside it what every city is known for and what it gave her.
//
// Both are drawn from a View that Journey fills each frame; this file owns no gameplay state.
// The words of the cities are in Story.cpp. Sizes are next to the code that uses them.

#include "HudAtlas.h"
#include "Story.h"

#include <string>
#include <vector>

class Renderer;

namespace CaucasianEagle
{
    class Ui;

    namespace WorldMap
    {
        struct Point { float X = 0, Z = 0; };   // world metres

        struct View
        {
            const std::vector<Point>* Road = nullptr;   // every road sample, in order
            float Step = 2;                             // metres between samples
            float Walked = 0;                           // metres along the road where Nyxara is
            Point Player; float Heading = 0;            // her position and the way she faces (rad, 0 = +z)
            float Look = 0;                             // the way the camera looks (rad)
            std::vector<Point> Enemies, Chests;         // living creatures; closed chests she can reach
            std::vector<Point> Arenas; int ArenasWon = 0; bool Fighting = false;
            Point Cities[Story::CityCount]; bool Visited[Story::CityCount] = {};
            int City = -1;                              // the city she is in, or -1
            int NextCity = -1; float NextCityDistance = 0; // the next city ahead on the road, and the road to it in metres
            float MountainDistance = 0;                 // road left to the mountain, metres
        };

        // Loads the painted map. False (with 'error') when it is missing.
        bool Load(Renderer& renderer, std::string& error);
        // The emblem of a city (0..6).
        const HudArt::Sprite& Emblem(int city);
        // Layout units from the top of the screen to the bottom of the minimap and its label.
        float MinimapBottom();
        void DrawMinimap(Ui& ui, const View& view, float fade);
        void DrawMap(Ui& ui, const View& view, float fade);
    }
}
