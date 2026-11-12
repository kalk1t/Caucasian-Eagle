# Sounds of Caucasian Eagle

Every sound the game plays is a file in this folder. To change a sound, replace its file. To add a variant, add a file with the next number. No code changes are needed.

**File format:** WAV, PCM 16-bit, mono or stereo, any sample rate from 8,000 to 96,000 Hz. Other formats (MP3, OGG, 24-bit or float WAV) are not played; the engine writes one line to the debug output and stays silent for that file.

**Names:** the game asks for a sound by folder and name, for example `combat/hit`. It then plays one of `combat/hit_1.wav`, `combat/hit_2.wav`, … chosen at random, never the same one twice in a row. A sound with one file needs no number (`combat/walls.wav`). Files in `voice/` are spoken lines; their numbers say which line it is, not a variant.

**Where the files come from:** all files here are placeholders made on this computer:

| Made by | What |
|---|---|
| `python art/sounds/build_sounds.py` | Effects, loops and music, synthesised from noise, sine waves, plucked strings and sung vowels. The music is built from scale steps over a held bass, in the manner of Georgian three-voice song (see the comment above `music()` in the script) |
| `python art/sounds/build_voice.py` | Spoken lines, read from `game/src/Story.cpp` by the voices that come with Windows (David is the storyteller, Zira is Nyxara) |

Running a script again overwrites its files, including any recording saved under the same name. Keep recordings elsewhere too, or do not run the script again after replacing files.

## nyxara/

| Name | Files | Plays when |
|---|---|---|
| `swing` | 3 | A sword slash or a combo starts |
| `swing_heavy` | 2 | The charged slash |
| `dash` | 1 | Talon Dash (first skill) |
| `skill` | 2 | The second and third skill and the leaping combos |
| `guard` | 1 | The two-sword barrier goes up |
| `swap` | 1 | She swaps her swords |
| `effort` | 3 | Her voice on a heavy slash or a skill |
| `hurt` | 2 | Her voice when a blow lands on her |
| `death` | 1 | Her voice when she falls |
| `step` | 4 | Every 1.7 m she walks on the ground |
| `wing` | 2 | She leaves the ground |
| `land` | 1 | She comes back to the ground |
| `flight` | 1, loop | While she is in the air |
| `evolve` | 1 | She reaches a new form |
| `potion` | 1 | She drinks a potion |
| `say_fight` | 4 | Her words when the walls of a fight rise |
| `say_win` | 3 | Her words when a fight is won |
| `say_evolve` | 1 | Her words when she evolves |

## creatures/

| Name | Files | Plays when |
|---|---|---|
| `growl` | 3 | A creature appears in a fight |
| `attack` | 2 | A creature draws back to strike |
| `hurt` | 2 | A creature is hit (four times in ten) |
| `death` | 2 | A creature falls |
| `boss_roar` | 1 | A guardian of the road appears |
| `boss_death` | 1 | A guardian falls |

All creatures share these files. A sound per creature kind is not built yet.

## combat/

| Name | Files | Plays when |
|---|---|---|
| `hit` | 3 | Her blade or skill hits something |
| `blow` | 2 | A blow lands on her |
| `walls` | 1 | The walls of a fight rise |
| `victory` | 1 | A fight is won |
| `chain` | 1 | A chain anchor breaks in the finale |

## world/

| Name | Files | Plays when |
|---|---|---|
| `wind` | 1, loop | Always; quieter inside a city |
| `city` | 1, loop | Inside a city |
| `fire` | 1, loop | While the walls of a fight stand |
| `chest` | 1 | A chest breaks |
| `crystal` | 1 | A creature falls and leaves crystals |
| `charm` | 1 | A creature drops its charm |
| `bell` | 1 | She enters a city |
| `gift` | 1 | A city gives its gift |

## ui/

| Name | Plays when |
|---|---|
| `open`, `close` | The journal, the map or Nyxara's window opens or closes |
| `page` | A journal page is turned |
| `move` | Another row is chosen in Nyxara's window |
| `buy` | Crystals are spent |
| `deny` | Not enough crystals, or the row is complete |

These follow the Interface volume setting. All other effects follow Effects.

## music/

Loops. One piece fades into the next over 2.5 seconds. They follow the Music volume setting.

| File | Plays |
|---|---|
| `menu.wav` | On the start screen |
| `road.wav` | On the road |
| `city.wav` | Inside a city |
| `battle.wav` | In a fight |
| `boss.wav` | In a fight with a guardian, and in the finale |

## voice/

The storyteller. One line at a time; a new line cuts the one being spoken. While he speaks, music and the world are turned down. They follow the Effects volume setting.

| File | Words | Spoken when |
|---|---|---|
| `legend.wav` | The three passages of the legend | **R** is pressed on the Legend page of the journal |
| `chapter_01.wav` … `chapter_12.wav` | The name and text of a region | She enters the region |
| `city_1.wav` … `city_7.wav` | The city, its land and what she finds there | She enters the city for the first time |
| `memory_01.wav` … `memory_12.wav` | What Amirani said in the cave | A chest gives the memory |
| `end_1.wav` … `end_4.wav` | The parts of the end (the fourth only with all three chain links) | Each part of the finale |
