# Nyxara: the polish of 2026-11-19 and 20

What you asked for, what I did, how it was checked, and what is still open. Every change is a local commit on
`map-a-aaa` (nothing pushed). Pictures 01 to 21 are frames from the Map Lab's animation bench (the creatures in its
films are not drawn); 22 to 28 are from the Character Lab; 29 to 31 are from the game itself, taken by its finishers'
tour on the mountain.

## What you asked

- "work for next 500 hours on polishing character and its animations, examine her animations and fix where it needs to
  be fixed, fit where it needs better algorithm for better result, surprise me afterwork"
- "examine how she moves her hands while moving - she leaves her elbow open and little moved to the middle ... then
  attacking right and left clicks - there are some odd gaps between attacks, during attacks also, wrist rotates not
  right, animations seems like concated wrong way"
- "hands are not following the skills and combos properly, examine it and fix it"
- "C skill ... i want nyxara to jump up make 360 degree turn 3 times and she should throw wings feathers around at the
  same time while rotating"; "also remove f finisher from nyxara we dont need it anymore"
- On the first storm: "its throwing feathers like a firework, but i want more like dagger effect and increase range 3
  times"
- "also after finalizing the character animations - update chracterlab with latest nyxara"

## V, F in flight and the take-off (2026-11-19, late)

You asked of V: "she twists, sticks with two feet on [it], grab[s the] swords and make[s] then a backflip and stand[s]
still"; of F in flight: "do same twist as it does while V, it will be called twisted-dash and it also makes damage
through"; and "reduce flying uptime from 3 seconds to 1.5s".

**V.** After her twist her feet stick on the creature's side and she crouches there, her back to the ground, her wings
raised over her. Her back bends as far as her hands need and each arm turns onto its handle (her fists 5 to 9 cm off
the handles at most). She kicks off it, ripping the swords out, flies back as a thrown body, is upright before she lands,
lands facing it and stands still. A sword no longer jumps 0.4 m as it comes back to her hand.

**F in flight** is the twisted dash: V's twist feet first, 9 m in 0.6 s, striking every creature she goes through and
throwing it aside; the HUD says DASH while she flies.

**Take-off.** Space held 1.5 s takes her up (it was 3 s), on the road, on the mountain and in the Character Lab; a tap is
still a jump.

## The three finishing kills (2026-11-20)

You asked: "i want 3 different finishing kill animations for nyxara to happen when she is killing monsters everytime and
each one happens danromly for each kill. its needs hard work", and later "keep working on 3 killing finishes and work on
that especially with hard effort".

**What happens.** Her blow that would kill a creature of the road (not a boss, she on foot, within 5.5 m of it) dooms it
instead: it stays standing at 1 health, dazed, its head hanging and swaying, a violet ring pulsing on the ground under it.
As soon as the move she is in ends, she finishes it with one of three kills, chosen at random and never the same one twice
in a row. Nothing can hurt her while it plays, and each blow freezes the moment for an instant and slows it a little. If
she does not come to it within 3 seconds (she flew off, or it was struck from afar), it falls by itself.

- **Talon Cross** (2.3 s, pictures 26): she dashes in, two cuts cross on its chest (she hops into them at a tall one),
  and she flashes past round its side. She stands with her back to it; a beat later the cross opens and it falls on its
  face.
- **Eagle's Plunge** (2.75 s, 27): she crouches and springs with her wings beating, drives both swords into it under its
  neck and rides it down onto its back, crouched on it; then she pulls them out and springs back off it. At a tall one she
  is lifted to its neck (2.5 m at a Uaig).
- **Gale Slide** (2.6 s, 28): she slides past it low, her sword through its legs, and it drops to its knees. She rises,
  turns her back to it, and her sword comes down behind her through its neck. At a giant she leaps into the blow and it
  goes into its shoulder.

**How it is made.** Each kill is a clip the kit makes when her body loads, out of pieces of her own clips (Attack_R1 into
Attack_L1; Attack_Jump; Skill_Slide into Attack_R3), so it needs no new animation file. When a kill begins it is worked
out for that creature from where she and it stand and how big it is: its height, its neck and knees (from its joints),
how deep it is front to back and how wide at the shoulders (from its model). The kit knows where her blades are in the
clip every 1/60 s, so it sets where she goes, which way she faces and how high she is lifted for the blade to meet its
neck at the moment of the blow. The creature falls as the kill has it: on its face, on its back with her riding it, or
from its knees away from her blow; its legs straighten as it goes over, and it lies a second before it fades.

**What was wrong and fixed on the way.**
- She shook to and fro during a kill: the engine moves her on a 60-steps-a-second grid of its own (no step in one frame,
  two in the next), so steering her to the plan's place frame by frame overshot. She now moves at the plan's own speed
  and only what she is off it is made up, over 0.08 s.
- At the giants her slide went through them (it was worked out for a body as wide as its feet's circle), and from the
  camera behind her a Uaig hid her whole slide. She now goes round its body as deep and wide as its model is, and more to
  its side the bigger it is.
- Her wings went through its legs in the slide: they are now swept back as she goes low, and beat as she springs.

**How it was checked.** All three kills on all eight kinds of the road, 1.15 m (Chinka) to 5.2 m (Devi), filmed in the
Character Lab from the side and with the game's own camera (behind her, as you play), each picture timed against the
kill's clock. And a scripted fight in the lab with her blows taking health: every killing blow doomed its creature and
was finished, the kills in the order Plunge, Cross, Slide, Plunge.

**Sounds.** Each kill has the bite of her blade and the creature's blow and fall; since 2026-11-20 also her blade's slash
through the air at each cut (the heavy slash at the Gale Slide's killing blow) and her wings where they beat. (This
report said once that `nyxara/swing`, `nyxara/skill` and `nyxara/wing` have no files: wrong, they have, and play.)

**Still open.** Not yet played by a person: on the mountain the game has played them itself (below).

## Her kills on the mountain (2026-11-20)

On the Eagle's Ascent she goes over any edge, and a kill's way past a creature at an edge would have taken her over it.
Each kill's way is now looked at before it is played, every twentieth of a second of it: walkable ground under her, no
more than a step up or down from where she was the moment before, and nothing (rock, a trunk, a wall) where she stands.
The kill drawn at random is played where it has room; where it has none, its other ways (the Talon Cross past the
creature on her other side, or round to beside it), then the other two kills. Where none has room, her blow kills the
creature at once, as before the kills. Should she go over an edge in the middle of a kill after all, the kill ends and
she falls.

**How it was checked.** The finishers' tour: the game itself stands her before every creature of the mountain's 103
stops in turn (on the stairs, the slopes, in the caves, in the snow) and dooms it, and she finishes it as she would in
play. 115 kills (Talon Cross 36, Eagle's Plunge 40, Gale Slide 39), none in which she fell or stuck. Two had no room: one
creature at a parapet's edge, every way past it blocked; one where the tour had put her down inside a parapet, so that
every way of hers seemed blocked (she could never stand there in play). Four were out of her reach when the tour doomed
them: creatures in the middle of a hop, or on the stairs above or below her. Pictures 29 to 31 are three of the 115.

## Struck (2026-11-20)

Every blow tipped her back by the same 7 degrees: a Chinka's claw as a giant's leap-slam, and from behind or from a side
as from before her. Now each blow says where it came from (the creature, the burst of a curse or a slam, an orb's way),
and she is knocked away from it, forward, back or to a side, as hard as the blow: 4.5 degrees for a Chinka's claw, 6 for
a light blow of a Devi, 9 to 10 for its heavy ones. A heavy blow makes her knees give a little (2 to 6 cm), and a burst
under her feet makes them give without tipping her. Measured in the Character Lab from her joints, with a Devi and a
Chinka striking her from before her, from behind and from each side.

And R3: its cock before the blow (44 % of its blow's speed) was left just under the line at which the kit makes a cut's
wind-up one move (45 %). The line is now 40 %, so R3's is evened as R2's, R4's and L3's are. Her other cuts in the cut
bench's film are as they were, to the hundredth.

## The twelve creatures fight back, each its own way (2026-11-20)

You asked: "also update each monster combat skills and movements animations ,attacking skills and dodging skills for
those twelse monsters". The eight creatures of the road (Chinka, Ali, Kudiani, Ochokochi, Almasty, Kaji, Uaig, Devi) and
the four bosses (the Gveleshapi, the Giant, the Devi Lord, the Eagle) each have their own blows (seventeen in all, each
with its own wind-up, reach and look), their own moves from farther off (the Chinka pounces, the Ochokochi, the Devi
and the Devi Lord charge, the Kudiani curses the ground where you will be, the Ali shrieks and glides back, the Kaji
breathes fire, the Gveleshapi sprays and rears, the Giant leaps onto you, the Eagle throws a gust and swoops), their own
dodge when they see her blow coming (a hop aside or back, a glide, a step aside, a heavy step back, the Kaji's step
through fire; three strike back at once), and their own gait: they face her as they circle her and step sideways or
backwards, they run when they are faster than their walk, and their legs step as far as they go. The bosses gather
themselves before their great blow. The game's manual has a row for each creature of the road and says how each boss
fights; the changelog has it all. All
eleven of the Character Lab's creatures were filmed showing every move, and the creatures' tour of the mountain beats
all 125 of its creatures, every stop as it should be.

## What changed on 2026-11-19 (her body, hands, cuts, the storm, the Character Lab)

**C is the feather storm** (pictures 16 to 19). She crouches as her wings rise, leaps as they beat down, turns round
three times in the air (gathering speed and losing it, 1280 degrees a second at the fastest), her wings spread and
trailing the turn, and comes down and lands as she does out of flight. While she turns she throws her wing feathers like
daggers: 50 a second, each wing in turn, each straight out at 30 m/s with a thin line of air behind it, down to the ground
somewhere 6 to 27 m off, where it sticks for two seconds. Each time she is half-way round, three fly at every creature
within 27 m (9 before your note) and strike it, 40 each time. The clip is made by the kit when her body loads, out of her
own clips (flight's take-off and landing, flight's wing beat, her guard), so it needs no new animation file. The feather
is new too (19): long, dark, a white bar, a light shaft, as on her wings (`art/nyxara/effects/wing_feather.py` draws it).
On the skill bar C reads STORM.

**The finisher is gone.** F at a broken creature, the flying throw, the gold rings, "F: FINISH IT", the reward and the
Finisher numbers are taken out of the code, the guide, the manual and the notes. F is only the slide (the dart in
flight), and still works out of standing and out of her guard (20).

**Her arms while she walks and runs** (13). With her swords on her back her arms now hang as she stands, elbows in, and
her forearms swing with her stride, left behind a little as she speeds up.

**Her cuts** (15). Each hand's row of cuts is in the order in which they go into one another: from the end of one cut to
where the next one's swing begins her hand had up to 1.7 m to go; now at most 1.0 m, mostly 0.3 to 0.5 m. Her hand is no
longer whipped to the next swing (it goes no faster than a hand goes into a swing), the next cut is the one her hands can
reach soonest from where they are, and a cut is entered earlier where that saves her hand a rush. Out of a cut she is
given time back to her guard (she was thrown back in a fifth of a second, her hand at up to 9 m/s; now at most 6). Her
draw is slower and her swords settle into her fists.

**Her wrists** (14). The clips bend her wrists past what a wrist does through most of every cut and skill (the cutting
hand 51 to 53 degrees toward the little finger, the guard's right hand 59 toward the thumb, some skills over 100). They
are now held within what a wrist does, so her hands follow her swords, and her arms reach for the handles on her back
with the elbow going round as an elbow does.

**Each cut one move** (21). Some cuts still read as two moves ("concated wrong way"): in R2, R4 and L3 the clips cock
the blade in a tenth of a second at 9 to 11 m/s, nearly as fast as the blow, stop, and strike. Each such wind-up is now
re-timed when her body loads, through the clip's own poses, her hand gathering speed evenly into the swing (every frame
is still a pose of the clip, and the blow is where it was). And where the next cut is entered is now found by playing
each choice on her joints as the game will play it, not guessed from where her hand is. Her hand's speed on its way
into the swing, before and after (the swing's own in brackets): L1 out of her guard 9.2 and 5.2 m/s (10.7), R1 5.3 and
3.1 (10.4), L3 6.1 and 3.2 (14.7), R2 9.8 and 5.1 (13.8), R4 after L2 11.0 and 7.3 (12.4).

**V's twist.** On her way after her thrown swords she turned fastest at once, her hands from rest to 27 m/s in one
frame. She now gathers speed over the first third of the way (her left hand 4, 10, 15, 18, 19 m/s in the first frames)
and still slows into the grab.

**The Character Lab is the game's Nyxara** (22 to 25). It had its own copy of her from 2026-11-16: a spinning kick on C,
a flying throw on F, nine seconds' hold to fly. It is now built from the game's own code and runs her game model, set up
as the Eagle's Ascent sets her up: her keys, the camera, the real jump, Space held 3 s to fly, R to land, the game's
HUD of her, and the Ascent's light (F5: day, dawn, night; the lab's old light washed her white feathers out). The
dummy stands 1.7 m before her and one creature of each kind round the arena; her health stays whole, K kills her and
she rises. It keeps a clip viewer (F7 shows any of her clips as the game keeps them, round and round; F6 the next),
slow motion (F8), a pause (F9 or Esc), the camera round to her face (Home) and back to the start (F2).
`Test-Characters.cmd` builds it again by itself whenever her code is newer than the lab, so it stays the game's.

**Earlier in the day** (01 to 12, before the hands and cuts): one stride from walk to run, gathering and losing speed and
leaning into turns; at ease with her swords on her back, shaking out her wings now and then; her jump following her rise
and fall; her wings answering the world and lowered at ease; looking into a turn, stepping when turning on the spot,
banking in flight; a foot that is down staying down; rocking back when struck and her head following the nearest
creature; on a stair each foot on its own tread, her knees bending as hinges (the knee snap you saw on stairs).

## Pictures

01 at ease · 02 wing stretch · 03 stride · 04 stop · 05 turn on the spot · 06 jump · 07 off an edge · 08 bank in flight
· 09 draw and sheathe · 10 struck · 11 watching · 12 turns · 13 arms while walking · 14 guard fist · 15 right cuts in a
row · 16 feather storm from behind · 17 from far above · 18 daggers close · 19 the new feather · 20 the slide · 21 the
left cut out of her guard as one move (0.05 s apart) · 22 the Character Lab with the game's HUD · 23 her rows of cuts
and her guard at the lab's dummy · 24 the storm in the lab · 25 the lab's clip viewer on the storm · 26 Talon Cross · 27
Eagle's Plunge · 28 Gale Slide · 29 the Talon Cross on the stairs (her cross on its chest; she is past it) · 30 the
Eagle's Plunge in a cave · 31 the Gale Slide on the forest path (it on its knees, her blow coming down behind her)

## How it was checked

Each change was filmed in the Map Lab before and after and measured from its joint log. Every step was compiled alone
before its commit. The storm: three turns (1089 degrees), her place does not move, her feet 1.95 m up at the top, all nine
dagger blows land at 6 m and at 22 m. The cuts as one move were measured in the Map Lab's cut film (the same keys before
and after).

The Character Lab checks her itself, unattended (`-NyxaraCheck`, about a minute): her two rows of cuts, her guard, the
slide, the thrown swords, the storm, a walk, a jog and a run (measured), a jump, a take-off, a flight, the landing, her
death and rising. It passed three runs in a row (every clip in its row's order; walk 1.64 to 1.65 m/s against the
game's 1.66, jog 3.17 against 3.2, run 5.27 to 5.36 against 5.4; 27 blows on the dummy each time). A fight at the dummy
(11 cuts, 11 hits) and the clip viewer pass too.

On the final build the hop walk and the Map Lab's Smoke, Walk, Caves, Ram, Leap and Rings runs all pass, and the game
builds without a warning.

## Still open

- L1 out of her guard is entered 0.42 s of its clip before its blow, so that her hand goes in at 5 m/s: its blow lands
  0.16 s after the click later than with the first way of choosing the entry (0.26 s of the clip, her hand in at 9 m/s),
  and 0.08 s later than with the old fixed entry (0.34). I chose the smoother cut; `LeadCost` in NyxaraKit.cpp sets how
  much quickness is worth against her hand's speed. R4 after R1 goes in a little faster than before (6.2 m/s against 5.7).
- The storm has not been played by a person in the game itself. On the stairs and on a slope (the bench's films
  `stormstairs` and `stormslope`) she leaps and lands as on flat ground: her feet within 5 cm of the treads as she comes
  down, within 1 cm once she stands, and within 4 cm of the slope all through. Where her daggers stick on the stairs I
  could not see: the bench's camera from above is pushed down by the hillside below the stairs (they stick where the
  ground under them is found, else at the height of her feet).
- The Character Lab's own pictures (`-Film`) never show the HUD: the engine takes them before the HUD is drawn.

## To see it

In the game: C (the storm), F (the slide).

The Character Lab: `Test-Characters.cmd` in the repository's root (the first time it builds itself, a few minutes;
close it before it builds again). Its keys are at its top left. `tools/character-lab.ps1 -NyxaraCheck -Wait` runs its
check.

In the Map Lab's bench: `bash testing/MapLab/anim-all.sh <out folder> storm stormbehind stormabove stormfar skills`
(the cue files are in `testing/MapLab/anim-cues/`; `stormstairs` and `stormslope` the storm on the first stairs and on
the plain slope).

Her finishing kills: in the Character Lab, the cue words `doom` and `mortal` (its README); on the mountain, the
finishers' tour (the changelog says how to start it). Her knock when struck: the Character Lab's cues `station`, `gap`
and `face` put her before a creature with her side or her back to it (`station 8; gap 0.6; face 180`), and the kit's log
says every blow (`struck <damage> weight <how hard> way <x> <z>`).

This folder is committed with the rest (nothing pushed).
