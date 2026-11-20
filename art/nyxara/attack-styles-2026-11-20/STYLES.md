# Nyxara: ten attack styles to choose her two clicks from (2026-11-20)

What you asked for, what is in the Character Lab now, why it is built the way it is, and how to choose. Nothing in the
game has changed: the game never sets a style, so its clicks are still her own cuts. Everything is a local commit on
`map-a-aaa` (nothing pushed).

## What you asked

- "in game, just hide all monster's health bar"
- "then in character lab i want to test another attacks for right and left mouse clicks, for now this attacks are so
  slow and hard to control, also curent right/left clicks attacks contain several different atacks which concates a
  combo itself, but we could do better, i want you to examine soullike games and get me at least 5 different variants
  for each click - then we decide which two attacks we want for mouse click attacks and based on them then we build
  combos and finishers. place nyxaras with different attack styles next to each other and let me choose them with
  numbers between 0-9, so i will test them bymyself"
- "try at least each attack to be 3x faster then current one"

> **Later the same day: the five combos.** The owner kept FLASH STEP and DRAW CUT (style 2, its buttons swapped: LMB FLASH STEP, RMB DRAW CUT), PINWHEEL (4), MOUNTAIN BREAKER (7), LOW FANG (8) and SWALLOW DART (9). Keys 1 to 5 in the Character Lab now give her five combos built from them, in place of the ten styles on 0 to 9: each button begins with its style-2 move, and clicking the same button again goes on down that button's string. The strings are listed in `docs/changelog.md` and in the lab's panel.

> **Then her combo.** Of the five, the owner kept the first's LMB string and the fourth's RMB string: LMB FLASH STEP > PINWHEEL > MOUNTAIN BREAKER, RMB DRAW CUT > SWALLOW DART > PINWHEEL. She has it in the Character Lab from the start; the row of her figures is gone, so the "How to try them" notes below about the row and keys 0 to 9 no longer hold.

## How to try them

Open the Character Lab (`Test-Characters.cmd`, or `tools/character-lab.ps1`).

- **0 to 9** give her that style. Its left move is on the left mouse button and its right move on the right. **Backspace**
  gives her her own cuts again.
- **The row.** Behind the training dummy stand ten of her, one per style, numbered 0 to 9 from left to right as on the
  keyboard. Each does its style over and over: the left move, the left again as soon as it is free, then the right move
  alone. Each stands behind a post as far away as its longest move reaches. Your style's mark on the ground is gold, and
  the number, name and both moves float over each one.
- **The panel.** The top-left panel names her style and both moves, with when each blow lands and when the next press is
  taken. It also says what each move is.
- **F8** slows the lab to a quarter and **F9** pauses it, to see a move frame by frame.
- The creatures' stations take real blows: a killing blow dooms a creature, and her finishers work after any move's last
  blow.

## Why her cuts feel slow and hard to control

- **The blow is late.** It lands 0.27 to 0.47 s after the click (typically 0.35 s; 0.48 s with her swords on her back),
  because each cut plays its whole wind-up first.
- **The next click waits.** It is taken only 0.14 s after the blow, and a cut is over 0.82 s after its blow.
- **A button is not one move.** Clicks in a row walk through a chain of different cuts (L1, L2, L3, ...), so the same
  button swings a different way each time and you cannot tell where the blade will go.
- **She does not aim.** She swings the way she is facing, and her clip carries her the same distance whether the
  creature is near or far.

## What the soulslike games do

- **One button, one move, set by the weapon.** In Elden Ring and Dark Souls a weapon class is a moveset: daggers stab
  and flick, straight swords sweep, katanas draw-cut, thrusting swords lunge, twinblades spin, greatswords fall with
  weight. A player picks a class for how its R1 and R2 feel. A string of R1s repeats or extends the same motion; it never
  swaps in an unrelated attack. ([Elden Ring weapons guide](https://www.techradar.com/how-to/elden-ring-weapons-guide-everything-you-need-to-know-about-each-type),
  [fastest weapons](https://gamerant.com/elden-ring-fastest-weapons/),
  [daggers](https://eldenring.wiki.fextralife.com/Daggers),
  [straight swords](https://eldenring.wiki.fextralife.com/Straight+Sword),
  [katanas](https://gameplay.tips/guides/elden-ring-katana-weapon-guide-pvp.html))
- **Light moves start fast; heavy moves pay for their damage with time.** The fastest classes land a light blow in a
  handful of frames, and the colossal ones take several times longer.
  ([Dark Souls III combat](https://darksouls3.wiki.fextralife.com/Combat),
  [attacks per second, compared](https://lr.vern.cc/r/darksouls/comments/bqk3mn/comprehensive_list_of_attackspersecond/er6tb1l/?context=3))
- **The start of a swing turns onto the target, then commits.** With a soft lock the attacker turns during the wind-up
  and not during the blow, so a fight is readable both ways.
  ([Anatomy of an enemy attack in Dark Souls 3](https://www.gamedeveloper.com/game-platforms/anatomy-of-an-enemy-attack-in-dark-souls-3))
- **The end of a move can be given up.** After the blow a roll, a step or the next attack can cut the recovery short,
  and a press made during the move is remembered for a moment.
  ([Elden Ring dodging](https://eldenring.wiki.fextralife.com/Dodging),
  [Elden Ring combat guide](https://www.bandainamcoent.com/pt_br/news/elden-ring-introduction-part-4-combat-guide))
- **Step-ins and quickstep.** Sekiro's attacks close the gap and stop short of the enemy. Bloodborne's trick weapons
  weave a cut into a quickstep, its [Blade of Mercy](https://bloodborne.wiki.fextralife.com/blade+of+mercy) splitting
  into twin daggers for speed.
  ([the art and science of Sekiro's combat](https://www.superjumpmagazine.com/the-art-and-science-of-sekiros-combat/),
  [Sekiro's developers on its combat](https://gamingbolt.com/sekiro-shadows-die-twice-dev-talks-about-how-combat-differs-from-dark-souls-and-the-challenges-of-developing-it),
  [Bloodborne trick weapons](https://bloodborne.wiki.fextralife.com/Trick+Weapons))
- **Weight is felt as a pause.** On a heavy hit both bodies freeze for a few frames (hit-stop), and the view shakes.

No published frame tables were found that could be checked, so the numbers above are the shape of those games, not
their frames.

## What every move does

- **One move per button, the same every time.** A second click during a move gives the same move again, as soon as the
  first one is free.
- **It turns her onto the creature as it begins.** It turns her at up to 2000 degrees a second until its blow is
  committed. The creature it aims at is the one most before her and nearest: within 6 m of her, and within 75 degrees of
  her facing (150 degrees when nearer than 2.5 m).
- **It carries her in and stops short.** Each move carries her at most its lunge, to just short of the creature, or half
  its lunge with no creature there. She lands where the move means her to: the engine moves her in steps of 1/60 s, and
  the kit asks each step for exactly what is left, so a dash of two or three frames ends where it should at any frame
  rate. A wall or a body in the way stops her.
- **The end is free.** After its free time W, A, S, D, Space, F, V, C or E give the move up at once. A click made during
  a move is kept 0.3 s.
- **Heavy blows pause the world** for 0.05 to 0.09 s and shake the view. The ground blows ring out over the ground.
- **Out of standing at ease** her swords come out in 0.06 s and on into the move in 0.05 s.
- **Her finishers** start 0.05 s after a move's last blow.
- **Each blade leaves a short trail of light while it moves fast**, white for the right and green for the left.
- **The moves are made of pieces of her own clips.** At load each piece is cut out at its fast stroke and retimed with
  a smooth monotone curve, and the pieces are blended where they meet. A scratch tool checked every blow against her
  clips (the blade fast and before her when it lands) and every move against her cuts.

## At least 3 times as fast

> **Later the same day, 1.5 times slower.** The owner: "make those 10 attacks in character lab 1.5x slower". Every time in
> the table below (blows, next press, over) is now 1.5 times what it shows, so each move is at least 2 times as fast as her
> cuts rather than 3 (the "At least" column divided by 1.5). Distances and damage are unchanged.

Her cuts: the blow 0.35 s after the click, the next cut taken 0.49 s after it, the cut over 1.16 s after it. Three
times as fast is the blow by 0.116 s, the next press taken by 0.163 s, and the move over by 0.388 s. A move of several
blows is held to as many of her cuts in a row. Every move passes. Its smallest margin is in the last column.

| Key | Style | Like | Left click | Right click |
|---|---|---|---|---|
| 0 | FALCON | twin daggers: Bloodborne's Blade of Mercy, Elden Ring's daggers | TALON FLICK | TWIN FANG |
| 1 | MOONBLADE | a straight sword: Dark Souls' and Elden Ring's straight swords | CRESCENT | MOONFALL |
| 2 | IAIDO | a katana: Sekiro; Elden Ring's katanas and Unsheathe | DRAW CUT | FLASH STEP |
| 3 | STING | a rapier: Lies of P's rapiers; Elden Ring's thrusting swords | NEEDLE | FLECHE |
| 4 | WHIRL | a twinblade: Elden Ring's twinblades, the Godskin Peeler | PINWHEEL | CYCLONE |
| 5 | KESTREL | aerial: Devil May Cry's launcher; Elden Ring's jumping attacks | UPDRAFT | EAGLE DIVE |
| 6 | TEMPEST | dual curved swords: Elden Ring's; Sekiro's flurries | TWIN SLASH | GALE RUSH |
| 7 | COLOSSUS | weight: Elden Ring's greatswords and colossal swords | HEAVY CLEAVE | MOUNTAIN BREAKER |
| 8 | VIPER | low and long: Ashes of War that send a wave along the ground | LOW FANG | GROUND WAVE |
| 9 | RAPTOR | hit and run: Bloodborne's quickstep; Sekiro's step-ins | HIT AND FADE | SWALLOW DART |

| Move | What it is | Blows at (s) | Next press (s) | Over (s) | Reach | Damage | Carries her | At least |
|---|---|---|---|---|---|---|---|---|
| TALON FLICK | the right blade whipped down across her | 0.075 | 0.12 | 0.30 | 1.8 m, 45° either side | 30 | in 0.5 m | 3.9x |
| TWIN FANG | both blades in an X: the right down across her, the left back through it | 0.075, 0.15 | 0.22 | 0.36 | 2.0 m, 45° then 60° | 35 + 45 | in 1.0 m | 4.5x |
| CRESCENT | a wide level sweep of the left blade, from her left round to her right | 0.09 | 0.14 | 0.34 | 2.0 m, 75° | 45 | in 0.6 m | 3.4x |
| MOONFALL | the right blade down from high, and on down into the ground | 0.075, 0.20 | 0.28 | 0.50 | 2.2 m, 50° then 2.3 m, 45° | 30 + 85, heavy | in 1.4 m | 3.3x |
| DRAW CUT | from high over her left shoulder, down and level across at the waist | 0.09 | 0.13 | 0.32 | 2.1 m, 50° | 45 | in 0.7 m | 3.6x |
| FLASH STEP | a blink in, turning, and the cut as she arrives | 0.10 | 0.14 | 0.36 | 2.2 m, 55° | 80, heavy | in 2.8 m | 3.2x |
| NEEDLE | two jabs of the left blade, far before her | 0.07, 0.155 | 0.22 | 0.34 | 2.6 m, 20° | 25 + 25 | in 0.4 m, back 0.3 m | 4.5x |
| FLECHE | she runs in low and drives the blade through it | 0.10 | 0.14 | 0.38 | 2.8 m, 20° | 85, heavy | in 3.0 m | 3.1x |
| PINWHEEL | one turn with both blades out, cutting all round her | 0.105 | 0.14 | 0.36 | 2.1 m, all round | 40 | in 0.6 m | 3.2x |
| CYCLONE | turn after turn, three blows all round her | 0.10, 0.25, 0.40 | 0.46 | 0.62 | 2.2 m, all round | 35 x 3 | in 1.0 m | 3.2x |
| UPDRAFT | a rising cut that throws it up, her feet leaving the ground | 0.09 | 0.14 | 0.32 | 2.0 m, 60° | 40, throws it up | in 0.5 m, a 0.35 m hop | 3.5x |
| EAGLE DIVE | a rising cut that takes her up, and both blades down into it as she comes down | 0.09, 0.235 | 0.30 | 0.52 | 2.0 m, 60° then 2.5 m all round | 40 + 95, heavy, a ring | in 1.8 m, a 0.25 m hop | 3.2x |
| TWIN SLASH | a cut down from high on her left, and straight back across her | 0.08, 0.195 | 0.26 | 0.40 | 2.0 m, 60° then 70° | 30 + 35 | in 0.6 m | 3.8x |
| GALE RUSH | running in with two cuts, one low and one across | 0.085, 0.21 | 0.28 | 0.42 | 2.1 m, 60° then 70° | 45 + 55 | in 2.4 m | 3.5x |
| HEAVY CLEAVE | the right blade brought down from high with all her weight | 0.10 | 0.14 | 0.38 | 2.2 m, 55° | 70, heavy | in 0.8 m | 3.1x |
| MOUNTAIN BREAKER | a cut across, then a hop and both blades into the ground: it shakes round her | 0.08, 0.235 | 0.30 | 0.54 | 2.2 m, 55° then 3.0 m all round | 50 + 120, heavy, a ring | in 1.2 m | 3.1x |
| LOW FANG | a cut down to its legs | 0.085 | 0.13 | 0.32 | 2.0 m, 55° | 40 | in 0.5 m | 3.6x |
| GROUND WAVE | the blade struck into the ground sends a wave along it, 7 m before her | 0.095 | 0.14 | 0.38 | a line 7 m long, 1.1 m either side | 70 | stays | 3.1x |
| HIT AND FADE | a cut down from her left, and a hop back out of reach | 0.08 | 0.16 | 0.34 | 2.0 m, 60° | 40 | back 1.2 m | 3.1x |
| SWALLOW DART | a dash in under its guard with a rising cut, and a hop back out | 0.10 | 0.16 | 0.38 | 2.1 m, 60° | 75, heavy | in 2.6 m, back 1.2 m | 3.1x |

The damage numbers are first guesses, to be balanced once the two moves are chosen.

## Checked in the Character Lab

- **Every move works in her hands.** On film at the training dummy, each style did its left move twice in a row and its
  right move once, and every blow struck. Her own cuts still pass the lab's unattended check (`CHECK_PASS`).
- **The lunges land on their marks.** Measured from her place: FLASH STEP carried her 2.80 m (planned 2.8), FLECHE 3.00
  (3.0), TALON FLICK 0.50 (0.5), SWALLOW DART 2.60 in and 1.20 back, CYCLONE 1.00 and EAGLE DIVE 1.80. The first version
  fell 15 to 40 % short and then slid back.
- **Spinning moves turn the right way.** CYCLONE's own turn and the clip's spin both go to her right: about two turns in
  0.45 s, and she ends facing the creature again. PINWHEEL is one turn with both blades out.
- **The jumping moves.** EAGLE DIVE rises, comes down with both blades into the ground (the ring and the held moment
  when it lands), and stands up.

Limits:

- The row's figures are drawn without shadows, and their feet are not set on the ground.
- The blade trails are drawn for her, not for the row.
- CYCLONE, PINWHEEL, TWIN SLASH and GALE RUSH turn very fast at three times the speed. Try them with F8.

## Choosing

Try each style on the creatures, then pick one move for each button. They need not come from the same style: a left
click from one and a right click from another (for example TALON FLICK and FLASH STEP) work together as they are. Once
the two are chosen, the combos and finishers are built on them, and the chosen pair replaces her cuts in the game.
