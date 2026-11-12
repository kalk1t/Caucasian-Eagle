#pragma once

// How Nyxara grows on the road, and the window that shows it (the I key).
//
// Creatures drop crystals. Crystals buy five things:
//   three items, each evolved up to three times     Wings, Swords, Armour   (tier 0..3)
//   two kinds of training, each up to eight levels  Power, Defence          (level 0..8)
// Her form is the lowest tier of her three items: when all three reach a tier she herself evolves
// (form 0..3), which strengthens everything again.
// Chests give healing potions; she carries a few and drinks one with a key.
//
// This file owns the rules (costs and what each step is worth; the numbers are in GameConstants.h) and
// the drawing of the window. Journey owns the State, saves it, spends from it and applies the results.

#include <string>

namespace CaucasianEagle
{
    class Ui;

    namespace Growth
    {
        enum Row { Wings, Swords, Armour, Power, Defence, RowCount };

        struct State
        {
            int Crystals = 0;
            int Potions = 0;
            int Level[RowCount] = {};   // tier of the three items, level of the two kinds of training
        };

        int Max(int row);                           // 3 for items, 8 for training
        int Cost(const State& state, int row);      // crystals for the next step; 0 when there is none
        int Form(const State& state);               // 0..3
        const wchar_t* FormName(int form);
        const wchar_t* ItemName(int row, int tier); // rows Wings..Armour

        // What she has become, as multipliers (1 = a new journey).
        float Damage(const State& state);           // of every blow she deals
        float DamageTaken(const State& state);      // of every blow she takes
        float Cooldown(const State& state);         // of the time her skills take to return
        float Boost(const State& state);            // of her wing-boost speed

        struct View
        {
            State Now;
            int Selected = 0;
            float Fade = 0;          // 0 closed .. 1 open
            float Flash = 0;         // 1 just bought .. 0
            int FlashRow = -1;
            int Feathers = 0, Gold = 0, Memories = 0;
            int PotionHeal = 0;      // what one potion gives now
            float OtherDamage = 1;   // feathers, embers and city gifts, on top of Damage()
            float OtherTaken = 1;    // city gifts and the game's base scale, on top of DamageTaken()
        };
        void Draw(Ui& ui, const View& view);
    }
}
