#pragma once

// Her quests on the Eagle's Ascent (0.5.3), and their panel (the Q key). The owner, 2026-11-22: "we dont need master -
// nyxara can open quests with Q and quests are coming one after each is done. so first nyxara can buy these three tasks,
// then when she completes she becomes new look and then she can buy 6 and then after finish these 6 then 9".
//
// Three sets of quests, one for each gear: 3 for THE HIGHLANDS, then 6 for AMIRANI'S FIRE, then 9 for MOONLIT
// (docs/plan-three-gears-masters.md). Only her gear's set is offered: Q, anywhere, shows it. A quest is bought with
// crystals of its set's kind (wind, fire, ice: AscentGrowth::CrystalKind) and counts only once taken. When all of a set
// are done she wears its gear at once (EaglesAscentGame::GearIfEarned), it is saved, and the next set is offered.
//
// The quests here are STAND-INS: beat creatures, break clay jars, reach a nest. The owner and Claude choose the real
// eighteen in step 6; only the table in AscentQuests.cpp changes then.
//
// The state is in Growth::State (QuestState, QuestCount), so that it is saved and rolled back with the rest of her.

#include "Growth.h"

#include <string>
#include <vector>

namespace CaucasianEagle
{
    class Ui;

    namespace AscentQuests
    {
        constexpr int Sets = 3;   // one for each gear: set k gives gear k + 1, and is bought with crystals of kind k

        enum QuestKind { Beat, Jars, Reach };
        enum QuestState { Offered, Taken, Done };
        struct Quest
        {
            const char* Id;        // in her save ("wind.1")
            int Set;
            QuestKind Kind;
            int Need;              // creatures, jars; Reach: the nest (1 the first up the way), or one above it
            int Price;             // crystals of its set's kind
        };
        int QuestCount();                                   // all of them, every set's
        const Quest& QuestAt(int index);
        int QuestIndex(const std::string& id);              // -1: none
        std::wstring QuestTitle(int index);                 // "Beat four creatures"
        std::wstring QuestProgress(const Growth::State& s, int index);   // "2 of 4", "on the way", "done", ""

        int OpenSet(const Growth::State& s);                // the set on offer: her gear's (-1: all done, she wears the last)
        int Kind(int set);                                  // the crystals its quests are bought with
        int QuestsOf(int set);                              // how many it has
        std::vector<int> Rows(int set);                     // its quests (indices), in order
        int DoneOf(const Growth::State& s, int set);        // how many of them are done
        bool SetDone(const Growth::State& s, int set);

        // the state made whole (an older save's "brought" is done)
        void Clamp(Growth::State& s);

        // ---- the panel (Q)
        struct View
        {
            Growth::State Now;
            int Selected = 0;          // its row
            float Fade = 0;            // 0 closed .. 1 open
            float Flash = 0;           // 1 a quest just taken .. 0
            int FlashRow = -1;
        };
        void Draw(Ui& ui, const View& view);

        // ---- on the HUD: the quests she has taken of her set, under what she carries
        void DrawTaken(Ui& ui, const Growth::State& s, float x, float y);
    }
}
