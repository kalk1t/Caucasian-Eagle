# M3 · Story spine (v0.3)

**Who:** You (story decisions) + Claude · **Needs:** answers to the story questions in [story/STORY.md](../story/STORY.md) section 5, at least 1, 2 and 6 · **Next:** [M4](M4-data-and-structure.md)

## Goal
The game tells the start of its story and can carry the rest: the player sees Amirani chained on the mountain, plays the opening, and their progress is saved.

## Steps

### 1. Story decisions (You)
- [ ] Answer the open story questions (honesty of Amirani, fate of the Eagle, languages).
- [ ] Approve or change the chapter structure in `story/STORY.md` section 4.

### 2. The map (You + Claude)
- [x] The chained mountain, with Amirani, the eagle and the dog, is visible from the play area under a new night sky and moon. First version: scenery only, rigid figures (debt D16-D18).
- [ ] Rework the Moonlit Sanctuary play area itself into the first stretch of her road.
- [ ] Decide what the sanctuary is in the story (her training ground in Year One is the proposal).
- [ ] Rebuild and recompile the scene ([techplan/03-workflows.md](../techplan/03-workflows.md) section 6.5).

### 3. The Open Cave prologue (Claude)
- [ ] A short opening: the cave open, Nyxara sees her father at Amirani's liver, the conversation, her promise.
- [ ] Simplest workable form first: in-engine camera shots with text lines. Voice and animation later.

### 4. Dialogue and chapter flow (Claude)
- [ ] A text box and line sequencer in the game HUD.
- [ ] A chapter list in data: map, opening lines, goal, ending lines.
- [ ] A scene-change path from one chapter to the next **(engine, if the current scene transition is not enough)**.

### 5. Saving (Claude)
- [ ] Game save in `%LOCALAPPDATA%\CaucasianEagle`: current chapter and what is unlocked. Continue from the menu.

### 6. Real enemies (Claude)
- [ ] Move shades from the kit's private list to actors that collide with the world and can have different behaviours **(engine: decide whether to use the engine's monster system or a game-side actor layer)**.
- [ ] One new enemy type that needs a different answer than the shade.

## ✅ Done when
- [ ] A new player starts the game, sees the prologue, and understands who Nyxara is and what she has promised.
- [ ] The mountain is on screen during play.
- [ ] Quitting and continuing returns to the same chapter.
