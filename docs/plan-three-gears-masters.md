# Plan: three gears, earned by her quests (replacing the 9 levels)

Owner's brief, 2026-11-22. Built in the order of the "Steps" section at the end; steps 1 to 4 are done (see there).

The first brief had three masters on the road who sold the quests and gave the gears; they were built as spirits by
nests 3, 6 and 8, then taken out the same day (owner: "we dont need master - nyxara can open quests with Q and quests
are coming one after each is done ... we can remove masters from map"; "physically we dont need master on the road").
The file keeps its name.

## 1. What changes, in one paragraph

Nyxara no longer buys 9 levels of wings, swords and armour with crystals at nests. She starts the Eagle's Ascent in her
own plain gear. Q shows her quests, anywhere. They come in three sets: first three; when all three are done she wears
a whole new gear at once, and six more are offered; when those are done, the next gear and nine more. Her look, her
attacks' effects and her strength change together with each gear. She always wears the newest. The I window no longer
shows levels or looks; it becomes her inventory, for the things quests ask her to find.

## 2. The gears

| Gear | Earned with | Her look (all three items) | Her attacks' effects |
|---|---|---|---|
| 0 Plain | (start) | her own | blue lines (as in the lab now) |
| 1 THE HIGHLANDS | 3 quests, wind crystals | Highlands tier 3: Wings of the royal eagle, Sabres of Queen Tamar, Harness of Queen Tamar | blue lines and wind |
| 2 AMIRANI'S FIRE | 6 more, fire crystals | Amirani's Fire tier 2: Ember wings, Ember blades, Obsidian and ember | fire |
| 3 MOONLIT | 9 more, ice crystals | Moonlit tier 3: Night-sky wings, Star-crystal blades, Celestial harness | ice and crystal |

Wings' size grows from gear to gear as the art has it: Highlands 1.07, Amirani's Fire 1.11, Moonlit 1.20.

### What each gear gives her (proposed numbers: say if you want them different)

Today, at the top of the 9 levels, she flies 30 s, deals about 3.2 times her first damage and takes about 0.3 of
it. The gears end at the same place, in three steps:

| Gear | Flight | Damage she deals | Damage she takes |
|---|---|---|---|
| 0 Plain | 10 s | x1.0 | x1.00 |
| 1 Highlands | 15 s | x1.6 | x0.80 |
| 2 Amirani's Fire | 22 s | x2.3 | x0.60 |
| 3 Moonlit | 30 s | x3.2 | x0.30 |

### Her attacks in the game

The lab's effects and the half speed come into the game (your choice):
- all her combo moves, old cuts and skills play at half speed, and the combo window and click buffer are stretched to
  match;
- every hit has the effects of the gear she wears, and with each gear they grow, as with U in the lab.

To watch for: the creatures and bosses were tuned to her full speed. At half speed she deals her damage more slowly
and stays longer in a move, where they can hit her. I would retune the gears' damage after playing the road once (so
the numbers above may change), not slow the creatures.

## 3. Her quests (Q)

- **Q**, anywhere, opens her quests; the climb waits while they are open. W/S or the pointer choose one, ENTER or a
  click on its TAKE buys it, Q, Esc or CLOSE closes.
- Only one set is offered at a time: the three for THE HIGHLANDS first, then the six for AMIRANI'S FIRE, then the nine
  for MOONLIT. The panel says which gear the set gives and what comes after it.
- A quest is bought with crystals of its set's kind (wind, fire, ice) and counts only once it is taken: a group cleared
  before she took the quest does not count.
- When the last quest of a set is done, she wears its gear at once: the moment where she changes (its light round
  her), and it is saved with all she has; if she falls, she wakes at the nest she last saved at, wearing it. The next
  set is offered.
- The HUD shows, under her crystals, the quests of her set she has taken (done ones in gold) and how many are done; with
  none taken, which crystals buy them. Her window (I) shows the gears in order and how far her quests are for the next.

## 4. Crystals: three kinds, to buy quests

- There are three kinds: **wind**, **fire** and **ice** crystals. The first set of quests is bought with wind, the
  second with fire, the third with ice.
- The creatures and jars of each stretch of the road give its kind: wind on the way up to Eagle Nest 3, fire from there
  up to nest 6, ice beyond (GameConstants `CrystalNests`). Off the way (caves, the rings round the mountain) a stretch
  goes by height against the nests. (Confirmed by the owner, 2026-11-22, when the borders were the masters' places.)
- The HUD shows the three counts, each in its colour, the kind of where she is underlined.

What the creatures and jars of each stretch hold, measured by the crystals' ledger (DEMO=8), if she takes everything:

| Stretch (on the way to) | Kind | Crystals |
|---|---|---|
| Nests 1 to 3 | wind | 25 + 42 + 63 = **130** |
| Nests 4 to 6 | fire | 81 + 99 + 117 = **297** |
| Nests 7 to 9 and the summit | ice | 138 + 156 + 174 + 77 = **545** |

The quests' prices (step 6) are set from these numbers (what she really picks up is less: not every jar is broken).
A crystal of a kind she cannot spend yet (fire while her first set is open) waits for its set.

## 5. The I inventory

- The I window becomes her inventory: everything she finds on the way (feathers, crystals, keys, relics, a quest's
  things), shown with what it is for.
- Potions stay as they are, on Z, apart from the inventory.
- A "find" quest is done when its thing is in her inventory.

## 6. Quests: what kinds the game can already support

To choose together later; these are the kinds the game has the parts for, from least to most work.

1. **Defeat a group**: clear one of the creature groups by the road or in a cave (the 17 groups stay).
2. **Defeat a boss**: one of the bosses on the road.
3. **Break jars**: break N clay jars in a stretch of the road.
4. **Find**: things lying in the world (a feather, a stone, a key, a relic) picked up into the inventory. Needs the
   things' models.
5. **Reach a place**: get to a high ledge, a cave's end, a peak; some only by flight.
6. **Finish with a finisher**: kill N creatures with finishers.
7. **A timed flight**: fly from one point to another within the flight her gear gives.

18 quests in all (3 + 6 + 9).

## 7. What stays, what goes

- **Stays:** the nests (H saves there), the 17 creature groups and the cave groups, clay jars and potions (Z), death
  rolling back to the save.
- **Goes:** the 9 levels, buying them at nests, the levels' look choice, the level ledger (DEMO=8), the I window as it
  is now, the one kind of crystal; the masters.
- **Saves:** old saves start again (owner's choice). The save carries her gear, her three crystal counts, her
  inventory, and each quest's state.
- **Release:** not yet; when it is, it will be **0.5.3**.

## 8. Steps

1. **Done (2026-11-22).** Gears in the game: the 3 gears, their look, power and effects, half speed. To try one before
   earning it: `tools\play-gear.ps1 -Gear 0..3` (the lab: 1, 2, 3 and U, as before).
2. **Done (2026-11-22).** The 9 levels removed; nests kept for saving; the new save ("rules 3": her gear; an older save
   starts again). The I window shows her gear, what it gives, the gears in order, her crystals and what she carries,
   until it becomes the inventory (step 5).
3. **Done (2026-11-22).** Three kinds of crystals by stretch (above): creatures and jars leave their stretch's kind (a
   potion she has no room for: a crystal of where she is); the HUD shows the three counts, the kind of where she is
   underlined; the I window lists them with the gear each buys quests for; the save keeps all three. Tests: the ledger
   (DEMO=8) counts each kind; the jars' test (DEMO=4) breaks a jar of each kind; the creatures' tour (DEMO=3) checks
   every group leaves its stretch's kind.
4. **Done (2026-11-22).** Her quests (game/src/AscentQuests.cpp), as in section 3:
   - **Q panel:** in the middle of the screen; its set's gear, her crystals of its kind, a line on the rule, the quests
     with their prices and TAKE, how many are done, and the set after it.
   - **Quests:** these are **stand-ins** until step 6: beat creatures, break jars, reach a nest (that nest or one above
     it: she may take it when she is already past). There are 3, 6 and 9 of them, priced 38 wind, 131 fire and 182 ice
     in all. A "QUEST DONE" notice says how many of the set are done. The save keeps each quest (id, state, count), and a
     fall rolls them back with the rest of her.
   - **A set done:** the gear at once, saved where she last saved; the gear's notice: "Her quests are done: wings, swords
     and armour, all new. Saved."
   - **The masters:** built first as three spirits by nests 3, 6 and 8 (floating winged figures of feathers of light,
     with map signs, a Q talk beside each, done quests brought back to them), then taken out the same day at the
     owner's word. Nothing of them is left in the game; a copy of that version was kept outside the project.
   - **Guide:** the GROWING page tells this, and Q is on the keys page. The JOURNEY and HEALTH pages still describe the
     old road: for step 7.
   - **Test:** the quests' test (DEMO=11, `tools\quest-test.ps1`). Q opens the first three anywhere; it buys them with
     Enter, a click, and S then Enter, checks too few crystals buy nothing and nothing counts before a quest is taken;
     finishes them and checks THE HIGHLANDS is worn at once and saved; reads the save back; checks the six are offered,
     a nest above the one asked counts, and a quest taken after the save is rolled back. Pictures of the panel, the HUD
     and the window with the HUD. QUESTS_PASS.
5. The I inventory and things to find.
6. Quests: we choose the 18 together, set by set, and I build them.
7. A full play of the road, retune the numbers, release as 0.5.3.

## 9. Confirmed

1. Crystals by stretch of road, as in section 4.
2. No masters: Q shows her quests anywhere, one set at a time; a set done gives the next look (owner, 2026-11-22).
