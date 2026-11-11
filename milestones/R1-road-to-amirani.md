# R1 · The road to Amirani (v0.2)

**Who:** Claude, working alone while the author was away · **Asked for on:** 2026-11-10 · **Next:** [M2](M2-installable.md) (installer) and the open items at the bottom

## Goal

A game that can be played from start to finish: a road of at least two hours from Moonlit Sanctuary to the mountain, at least ten creatures from Caucasian folklore to fight on the way, old gold chests to break, and at the end the fight with the Eagle and the breaking of Amirani's chain. The white disc in the middle of the sanctuary removed.

This milestone was not in the original plan. It delivers, early and in a first form, parts of [M3](M3-story-spine.md) (map, prologue as text, saving, real enemies) and [M5](M5-content-and-feel.md) (the road, enemies, the Eagle fight).

## Steps

### R1.1 Research and design
- [x] Ten creatures chosen from Georgian and Caucasian folklore: Chinka, Ali, Kudiani, Ochokochi, Almasty, Kaji, Uaig, Devi, Rokapi, Gveleshapi ([art/README.md](../art/README.md) lists the sources).
- [x] Twelve regions designed, each with its own ground, weather, landmarks and creatures ([story/STORY.md](../story/STORY.md), section 5).

### R1.2 Creature and prop models (Meshy + Blender)
- [x] Concept image, then image-to-3D, for the ten creatures and the chest (429 credits).
- [x] Four scenery props: Svan tower, dead tree, fir, rock cluster (156 credits).
- [x] `art/monsters/prepare_models.py` turns each into a game model.

### R1.3 The world
- [x] `art/world/build_world.py` builds the sanctuary and a 6.3 km road as one scene: terrain, road, bridges, boardwalks, gates, ruins, walls of Kajeti, giant ribs, crystals, water, lava, collision.
- [x] Generated ground, cobble and plank textures with normal maps.
- [x] The white disc ("Arena central seal") removed from the sanctuary.
- [x] The mountain moved to the end of the road; its ledge is walkable.

### R1.4 Engine (in the engine repository)
- [x] `SceneCollision` session option: the game's scene collision rules movement (`2666e8f`), and the engine's ±999 m play-area box is off (`d9028b6`).
- [x] `CombatSystem::RestorePlayerHealth` for healing (`aec7057`).

### R1.5 The journey in code (`game/src/Journey.cpp`)
- [x] Road following, gates, 34 arenas with waves, 65 roaming packs between them.
- [x] Creature behaviour: melee with a visible windup, ranged orbs, leapers, keep-distance casters.
- [x] Three region bosses and the Eagle, each with its own moves.
- [x] 81 old gold chests: coins, memories of Amirani, Moon Tears, Eagle Feathers, Soul Embers.
- [x] Prologue, region cards, boss lines, ending.
- [x] Save file, respawn at the last arena won, F9 restart.
- [x] HUD: road panel, objective, boss bar, story cards.
- [x] Test pilot: the game plays the whole road by itself for testing.

### R1.6 Verification
See [techplan/08-testing.md](../techplan/08-testing.md), section 11.2, for exactly what was and was not verified.

### R1.7 Documentation
- [x] Manual, glossary, changelog, story bible, technical plan, art notes updated.

## ✅ Done when
- [x] The test pilot plays from the sanctuary to "THE CHAIN OPENS" without help.
- [x] At least ten folklore creatures appear and fight.
- [x] Chests break and give their contents.
- [x] The white disc is gone.
- [ ] **The author plays it** with keyboard and mouse and confirms it is winnable and long enough. Not done: nobody has played this version by hand.

## Open after this milestone

| Item | Why it is open |
|---|---|
| Difficulty for a real player | Tuned by calculation and by a pilot that cannot die. Health and damage are two constants (`MonsterHealthScale`, `MonsterDamageScale`). |
| Death and respawn | Needs a click in the engine's death screen, so it could not be tested unattended. |
| Creature animation | Creatures are rigid models moved by code. |
| Sound | None. |
| "AAA" look | Not reached. Terrain and architecture are generated shapes with generated textures; models are single Meshy generations. |
| Story decisions | The road answers several open questions provisionally; all are marked **[proposed]**. |
