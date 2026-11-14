#pragma once

// The words of Caucasian Eagle and the story journal (F1).
//
// All story text the game shows lives in Story.cpp and agrees with story/STORY.md.
// The journal is where the story is read: nothing narrative is printed over the fight.
// It has five pages. The legend is always complete; the road, the memories, the places and the end
// fill in as Nyxara reaches a region, finds a memory in a chest, comes to a place beside the road, or plays the finale.

namespace CaucasianEagle
{
    class Ui;

    namespace Story
    {
        struct Chapter { const wchar_t* Title; const wchar_t* Text; };
        extern const Chapter Chapters[12];      // the twelve regions of the road, in order
        extern const wchar_t* const Memories[12]; // what Amirani said in the cave, found in old gold chests

        // The seven cities on the road, in the order she walks through them (placed by art/cities/build_cities.py).
        struct City
        {
            const wchar_t* Name; const wchar_t* Land;   // the city and the old land it stands in
            const wchar_t* Known;                       // what it is known for, in a few words
            const wchar_t* Text;                        // what she finds there
            const wchar_t* Gift; const wchar_t* GiftText; // what the city gives her, once, for the rest of the road
        };
        inline constexpr int CityCount = 7;
        extern const City Cities[CityCount];

        // The places beside the road, in the order she comes to them (they stand where art/places/build_places.py
        // puts them; that script checks its names against these).
        struct Place { const wchar_t* Name; const wchar_t* Text; };
        inline constexpr int PlaceCount = 26;
        extern const Place Places[PlaceCount];

        enum Page { Legend, Road, MemoriesPage, PlacesPage, End, PageCount };

        struct Journal
        {
            int Page = Legend;
            float Open = 0;         // 0..1 fade (drawn while > 0)
            float Scroll = 0;       // layout units scrolled down
            float Overflow = 0;     // layout units of content below the view (set by Draw)
            int ChaptersReached = 0; // 0..12 regions she has entered
            int CurrentChapter = -1; // region she is in now
            int MemoriesFound = 0;  // 0..12
            unsigned long long PlacesFound = 0; // bit per place beside the road she has found
            int EndReached = 0;     // 0 none, 1 father and daughter, 2 the Eagle falls, 3 the chain opens
            unsigned Unread = 0;    // bit per Page with something new
        };

        void DrawJournal(Ui& ui, Journal& journal);
    }
}
