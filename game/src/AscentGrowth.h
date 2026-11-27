#pragma once

// How Nyxara grows on the Eagle's Ascent (0.5.3, the owner 2026-11-22), and her window there (the I key).
//
// "we will only have 3 gears ... we dont need 9level system anymore"; "nyxara can open quests with Q ... first nyxara can
// buy these three tasks, then when she completes she becomes new look and then she can buy 6 and then after finish
// these 6 then 9". She sets out in her own gear (0); each set of her quests (AscentQuests) done gives her a whole gear,
// in this order (docs/plan-three-gears-masters.md):
//   1 THE HIGHLANDS    3 quests, wind crystals   (THE HIGHLANDS' third tier of wings, swords and armour)    her blue, and wind
//   2 AMIRANI'S FIRE   6 quests, fire crystals   (AMIRANI'S FIRE's second tier)                             fire
//   3 MOONLIT          9 quests, ice crystals    (MOONLIT's third tier)                                     ice and crystal
// She always wears the newest. A gear is her look, her attacks' light and her combo (her flair: NyxaraKit::SetFlair,
// her three in one, the Character Lab's 7th: GearFlair; her skills' light NyxaraKit::SetTrialTheme), and her strength: how long she
// flies, the damage she deals and takes (GameConstants: Gear...), and a little taller with each (Form).
// The state is Growth::State::Gear: EaglesAscentGame keeps it and saves it.
//
// Crystals (0.5.3) are of three kinds, wind, fire and ice, each the price of a set of her quests. The creatures and clay
// jars of a stretch of the way (between two nests) leave its kind: wind up to the third nest, fire up to the sixth, ice
// beyond (GameConstants CrystalNests). Growth::State::AscentCrystals.

#include "Growth.h"

#include <string>

namespace CaucasianEagle
{
    class Ui;

    namespace AscentGrowth
    {
        constexpr int ItemCount = 3;   // Growth::Wings, Growth::Swords, Growth::Armour
        constexpr int GearCount = 4;   // 0 her own, then the three

        int Form(const Growth::State& state);              // her gear, 0..3 (how tall she stands)
        float Damage(const Growth::State& state);          // of every blow she deals (1 at the start)
        float DamageTaken(const Growth::State& state);     // of every blow she takes
        float FlightSeconds(int gear);

        const wchar_t* GearTitle(int gear);                // "AS SHE SET OUT", "THE HIGHLANDS", ...
        const wchar_t* GearEarned(int gear);               // "", "Earned with her first three quests.", ...
        std::wstring GearItem(int gear, int item);         // "Wings of the royal eagle", ...
        void GearLook(int gear, int& example, int& tier);  // NyxaraLooks: its example (0-based) and tier (0 her own)
        void GearStyle(int gear, int& theme, int& level);  // NyxaraKit::SetTrialTheme's
        // NyxaraKit::SetFlair's, and its combo (NyxaraKit::FlairCombo): each gear's 7th, her three in one (GearFlairStyle);
        // before her first gear plain, no light at all, THE HIGHLANDS' 7th laid under it
        void GearFlair(int gear, int& flairGear, int& style, bool& own, bool& plain);
        struct Tint { float R, G, B; };
        Tint GearColour(int gear);                         // her own grey, THE HIGHLANDS gold, the fire's embers, moonlight

        enum CrystalKind { WindCrystal, FireCrystal, IceCrystal };
        constexpr int CrystalKinds = 3;
        int CrystalKindOf(int stretch);                    // a stretch's kind (EaglesAscent::StretchAlong / StretchHeight)
        const wchar_t* CrystalName(int kind);              // "wind", "fire", "ice"
        const char* CrystalId(int kind);                   // the same, for the tests' tables
        const wchar_t* CrystalFor(int kind);               // "for the Highlands", ...: the gear whose quests it buys
        Tint CrystalColour(int kind);
        int AllCrystals(const Growth::State& state);       // the three together

        // her state made whole: gear 0..3, crystals and potions in their bounds
        void Clamp(Growth::State& state);

        struct View
        {
            Growth::State Now;
            float Fade = 0;              // 0 closed .. 1 open
            float Flash = 0;             // 1 a gear just given .. 0
            bool AtNest = false;
            int PotionHeal = 0;
            int HereKind = 0;            // the kind of crystal where she is
        };
        void Draw(Ui& ui, const View& view);
    }
}
