#pragma once

// How Nyxara grows on the Eagle's Ascent (0.5.2, the owner 2026-11-21), and her window there (the I key).
//
// "make it buy as now with crystals , make nest only place where she can upgrade, make the upgrade 9 times instead of
// 3 ... so player will be able to upgrade each item individually at the nest - I still opens the inventory". Her wings,
// her swords and her armour have nine levels each, bought one at a time with crystals, only standing in an Eagle Nest.
// Each level is a look of its own (NyxaraLooks: levels 1-3 THE HIGHLANDS, 4-6 AMIRANI'S FIRE, 7-9 MOONLIT), and at a
// nest she may wear any look she has reached ("choose own look after leveling up only and at the nest only"): looks
// change nothing but how she looks. The trainings (power, defence) are gone; their strength is in the levels.
//   wings    how long she flies: 10 s at level 0 to 30 s at level 9 (AscentLevelFlightSeconds)
//   swords   the damage of her blows: + AscentSwordLevelDamage a level
//   armour   the damage she takes: - AscentArmourLevelTaken a level
//   form     one for each three levels all three have (levels 3, 6, 9): more damage, less taken, taller (FormDamage...)
// A level's price is the map's (EaglesAscent::Levels): what lies between two nests pays for one level of all three.
// The state is Growth::State (Level[Wings .. Armour] and Look[]): EaglesAscentGame keeps it, saves it and spends it.

#include "Growth.h"

#include <vector>

namespace CaucasianEagle
{
    class Ui;

    namespace AscentGrowth
    {
        constexpr int ItemCount = 3;   // Growth::Wings, Growth::Swords, Growth::Armour

        // prices[k]: crystals for level k + 1 of any one item. 0: none to buy (at the top, or no price for it).
        int Cost(const Growth::State& state, int item, const std::vector<int>& prices);
        int Form(const Growth::State& state);              // 0..3
        float Damage(const Growth::State& state);          // of every blow she deals (1 at the start)
        float DamageTaken(const Growth::State& state);     // of every blow she takes
        float FlightSeconds(int wingLevel);
        // the look a level is: its name ("Lammergeier wings"), its example's title ("THE HIGHLANDS") and its colour
        std::wstring LookName(int item, int level);
        std::wstring LookTitle(int level);
        // her state made whole: levels 0..9, looks 0..level
        void Clamp(Growth::State& state);

        struct View
        {
            Growth::State Now;
            std::vector<int> Prices;
            int Selected = 0;            // the item chosen
            float Fade = 0;              // 0 closed .. 1 open
            float Flash = 0;             // 1 just bought (or a look put on) .. 0
            int FlashRow = -1;
            bool AtNest = false;         // levels are bought and looks chosen only then
            int PotionHeal = 0;
        };
        void Draw(Ui& ui, const View& view);
        // the colour of a level's look: her own, THE HIGHLANDS gold, MOONLIT moonlight, AMIRANI'S FIRE embers
        struct Tint { float R, G, B; };
        Tint LookColour(int level);
    }
}
