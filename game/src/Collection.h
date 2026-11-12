#pragma once

// What Nyxara picks up on the road and keeps, and the second page of her window (I, then D) that shows it.
//
//   Charms            creatures drop them (tooth, horn, ...). Three of one kind are a set:
//                     she hits that kind harder and its blows hurt her less.
//   Chain links       one from each guardian of the road. All three change the ending.
//   Verses            one in each city. Seven are a song: creatures leave more crystals.
//   Father's feathers hidden at the edge of the road, one in each region. Each makes her wing boost faster.
//   Keepsakes         one from each city. Each makes her a little stronger and tougher.
//   Runes             fire, frost and moon, from old gold chests. One kind is set in her blades at a time.
//
// This file owns the rules (the numbers are in GameConstants.h) and the drawing of the page.
// Journey owns the State, saves it, adds to it and applies the results.

#include <string>

namespace CaucasianEagle
{
    class Ui;

    namespace Collection
    {
        inline constexpr int CharmKinds = 8;    // the eight creatures of the road, in the order of Journey's creature table
        inline constexpr int LinkCount = 3;     // the three guardians of the road
        inline constexpr int FeatherCount = 12; // one in each region
        inline constexpr int CityCount = 7;
        enum Rune { Fire, Frost, MoonRune, RuneCount };

        struct State
        {
            int Charms[CharmKinds] = {};
            int Links = 0;
            unsigned Feathers = 0;      // bit per region
            int Runes[RuneCount] = {};  // how many of each she has found (0..MaxRunes)
            int Set = -1;               // the rune in her blades, -1 none
        };

        const wchar_t* CharmName(int kind);
        const wchar_t* CreatureName(int kind);
        const wchar_t* KeepsakeName(int city);
        const wchar_t* RuneName(int rune);
        bool HasSet(const State& state, int kind);
        int FeathersFound(const State& state);
        int RuneLevel(const State& state, int rune);   // 0 unless that rune is set in her blades

        // What it all gives, as multipliers (1 = nothing collected) or plain numbers.
        float DamageTo(const State& state, int kind);          // her blows against that creature kind
        float DamageFrom(const State& state, int kind);        // that kind's blows against her
        float Damage(const State& state, int cities);          // all her blows: keepsakes and the fire rune
        float DamageTaken(const State& state, int cities);     // all blows against her: keepsakes
        float Boost(const State& state);                       // wing-boost speed: her father's feathers
        float Crystals(int cities);                            // crystals from creatures: the song of seven verses
        float CreaturePace(const State& state);                // time between a creature's attacks: the frost rune
        int KillHeal(const State& state);                      // extra health for every creature felled: the moon rune

        struct View
        {
            State Now;
            unsigned Cities = 0;     // bit per city she has entered (a verse and a keepsake each)
            int Selected = 0;        // which rune row is chosen
            float Fade = 0;          // 0 closed .. 1 open
            float Flash = 0;         // 1 just changed .. 0
        };
        void Draw(Ui& ui, const View& view);
    }
}
