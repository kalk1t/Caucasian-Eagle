# Caucasian Eagle: player manual (v0.3)

[← Project index](../README.md)

## What the game needs

| | Lowest (estimated, not tested) | Better |
|---|---|---|
| System | Windows 10, 64-bit | Windows 11, 64-bit |
| Graphics | A graphics card with DirectX 11 and 2 GB of its own memory | 4 GB |
| Memory | 4 GB | 8 GB |
| Disk | 1.5 GB free | the same |
| Screen | 1280 x 720 | 1920 x 1080 |
| Controls | Keyboard and mouse | or an Xbox-style (XInput) controller |
| Sound | Any sound device | |
| Internet | Not needed | |

The game has run on one computer only: Windows 11, Ryzen 9 9950X3D, 64 GB of memory, GeForce RTX 5080, where it holds 60 frames a second. The "lowest" column is an estimate from what the game uses there (about 1 GB on disk, under 1 GB of memory); no weaker computer has been tried.

## Installing

Download `CaucasianEagle-0.4.0-Setup.exe` from the [releases page](https://github.com/kalk1t/Caucasian-Eagle/releases) and run it. The file is not signed, so Windows shows "Windows protected your PC": choose **More info**, then **Run anyway**. The game installs for your Windows user only (no administrator question), by default into `%LOCALAPPDATA%\Programs\Caucasian Eagle`, and adds **Caucasian Eagle** to the Start menu (a desktop shortcut if you tick it). To remove it, use **Installed apps** in Windows settings; your settings and your saved journey in `%LOCALAPPDATA%\CaucasianEagle` are kept. Instead of installing you can unpack `CaucasianEagle-0.3.0-win64.zip` anywhere and start `CaucasianEagle.exe` in it.

## Starting

The start screen is drawn in the same panel, gold and ink as Nyxara's windows. At the left it has five rows: **Begin the journey** (or **Continue the journey**), **Settings**, **Guide**, **Credits** and **Leave the game**. Choose with **W** and **S** or the arrow keys and take with **Enter** or **Space**, or point and click with the mouse. **Guide** opens a short version of this manual inside the game, eight pages: the journey, moving and keys, fighting, the bow, health, growing, what the game needs, and who made it. **Credits** opens the last of them. **A** and **D** turn the pages; **Esc** closes the guide. Along the bottom of the start screen stands who made the game and its engine.
**Settings** opens the game's own settings window (the same window opens from her menu, **Esc**, in play). It has what this game uses and nothing else: the window (whole screen or in a window, and its size; these two wait for **Apply the window**, and a changed window asks "Keep this window?" and goes back by itself after 15 seconds if you do not answer), smooth frames (vertical sync), shadows, four volumes (everything, music, fights/world/voices, menus), mouse speed, look up/down reversed, the size of the interface, and the frame counter. **W** / **S** choose, **A** / **D** change, **Enter** takes, **Esc** goes back; the mouse works too (click the left or right half of a value). Everything except the window is applied and saved as you change it. **Restore the defaults** resets all but the window.

**The pointer.** The mouse pointer over the game is the game's own gold-rimmed arrow. In play the pointer is hidden and the mouse turns the camera; while her menu, the settings, Nyxara's window, the story or the map is open the pointer is free. In her menu and in Nyxara's window you can point at a row or a card and click it: a click on a card spends crystals on it (or sets a rune), as **Enter** does. The camera turns a little slower than before; "Mouse speed" in the settings changes it.

Caucasian Eagle is created by **Giorgi Samkharadze**. It is built in **Doorways**, a custom game engine founded by Giorgi Samkharadze in February 2026 (C++ on Direct3D 11); it is not made in a bought or borrowed engine.

Start the game. The start screen shows the Eagle over the mountain of Amirani, with the menu at the left. Choose **Begin the journey** (or **Continue the journey** when you have played before). You arrive as Nyxara on the old road, at night, before its first gate; Telavi is the first city ahead. Press **F1** to read how the road began.

## The story (F1)

Press **F1** at any time to open the story journal; press it again to close it. The road stands still while you read.

| Page | What it holds |
|---|---|
| The Legend | The legend of Amirani and why you are on the road. Complete from the start. |
| The Road | The twelve regions. Each one is written when you enter it. |
| Memories | What Amirani said to you in the cave. Each is written when you find it in an old gold chest. |
| The End | Written as you play the ending. |

**A** and **D** (or the arrow keys) change the page. **W** and **S** scroll when a page is longer than the screen. The F1 key at the bottom right of the screen glows when the journal holds something you have not read.

## Growing stronger

Creatures get stronger the farther you go: by the last chapter they have about three times the health and hit almost twice as hard. Nyxara has to grow with them.

**Crystals.** Every creature you fell leaves crystals: one for a Chinka, up to ten for a Devi, 60 for a guardian. Chests hold a few more. The number is at the top left, beside your gold.

**Press I** to open Nyxara's window. The game slows almost to a stop while it is open. Choose with **W** and **S**, spend with **Enter** or **Space**, close with **I**.

| What | Steps | Crystals | Each step gives |
|---|---|---|---|
| Wings | 3 evolutions | 40, 140, 320 | Skills return 12 % sooner, wing boost 8 % faster |
| Axes | 3 evolutions | 40, 140, 320 | Blows deal 20 % more |
| Armour | 3 evolutions | 40, 140, 320 | Blows taken hurt 12 % less |
| Power | 8 levels | 10, 20 ... 80 | Blows deal 7 % more |
| Defence | 8 levels | 10, 20 ... 80 | Blows taken hurt 5 % less |

**Nyxara evolves three times.** When her wings, axes and armour have all reached the same evolution, she herself changes: Fledgling, then Moonwing, Stormwing, Heir of the Eagle. Each form deals 10 % more, takes 8 % less, stands a little taller, and heals her fully when she reaches it.

Evolved wings, axes and armour glow in the colour of their tier: blue, then green, then gold. Their shapes do not change.

**Potions.** Every old gold chest holds a healing potion (feather chests two, a king's hoard three). She carries up to five; one more than she can carry stays behind. Press **Z** to drink one: 35 health. The potions you carry are shown at the bottom right.

### What she carries (I, then D)

Her window has a second page. Press **I**, then **D** to see what she has picked up; **A** goes back to the page where she grows.

| Thing | Where it comes from | What it does |
|---|---|---|
| **Charms** (Chinka tooth, lock of an Ali's hair, Kudiani's tail, Ochokochi's thorn, Almasty fur, Kaji's ember, Uaig's eye, Devi horn) | A creature drops its charm about one time in three | Three of one kind are a set: she strikes that kind of creature 15 % harder and its blows hurt her 15 % less |
| **Links of Amirani's chain** | Each of the three guardians before the Eagle leaves one | With all three the end of the story changes: a fourth part is added |
| **Verses of the road song** | One in each city, the first time she enters | All seven: creatures leave a quarter more crystals |
| **Feathers of her father** | Twelve, one in each region, lying at the edge of the road; each glows | Each makes her boost 2.5 % stronger |
| **Keepsakes of the cities** | One from each city | Each: 2 % more damage, 2 % less damage taken |
| **Runes for her axes** (fire, frost, moon) | Old gold chests, sometimes; feather chests and hoards, always | One kind is set at a time (W / S to choose, Enter to set). Fire: blows 6 % harder per rune. Frost: creatures strike 8 % less often per rune. Moon: 4 more health for every wave and arena won per rune. Up to three of each kind. |

Everything on this page is kept in the save. A new journey (F9) starts without them.

## The road to Amirani

Your goal is the mountain where Amirani is chained. The road is about 6.3 km long and passes through twelve regions.

1. **Go through the first gate.** You begin on the road itself, under the full moon, facing the mountain. (The Moonlit Sanctuary of version 0.1 is no longer in the game.)
2. **Follow the old road.** It is an earth road, worn by feet, hooves and cart wheels; only the causeway at its beginning and the last climb to the mountain are laid stone. Lanterns mark it. You may leave it: the whole land is open to walk and fly over, as far as the map reaches. Water, cliffs too steep to stand on, and the far mountains stop you. You walk round trees, boulders, towers, the piers of the gates and the houses of a city. Only the causeway out of the sanctuary and the last climb to the mountain have nothing beside them. **The way back is always shown:** when you are off the road, a small compass at the top of the screen points to it with the distance, and a trail of gold lights runs along the ground from your feet toward it. Creatures stay on the road, and an ambush springs only when you step into the clearing itself. You can walk round a clearing over the land, but nothing is won by it: the arenas must be won in order, the compass then points back ("A fight waits behind you"), and the mountain does not open until every fight is won. During a fight the clearing holds you. A city counts as entered (its name, its healing, its gift) when you are in its streets, not on the hill above it.
3. **Mind the trouble on the road.** Between the fights the road itself is against you, about every 85 m, never near an arena or a city. The line under the compass names what you are coming to.
   - **Cursed ground:** a ring of violet lights over one half of the road. It bites for as long as you stand in it. Walk round it.
   - **Falling stones:** a stretch with grit sifting down and an ember at each end. Stones fall where you are; an orange ring shows on the ground about a second before each lands. Keep moving and leave the rings. Her barrier, turned up, stops a stone.
   - **Wisps:** three blue lights swinging across the road. A touch burns. Slip between them, or go round over the land.
   Their harm is lessened by everything that lessens a creature's blows. None of them follows you off the road.
   A **gate** stands over the road where each region begins: two buttressed piers, a round stone arch with a keystone, and a wall with merlons above it.
   **Traps.** Nine larger traps stand on the road. The line under the compass names the one you are coming to and says what to do. Each is beaten once and gives 30 crystals; you may also fly round one over the land and leave it unbeaten.
   - **The gate run** (630 m): touch the light that hangs over the road. A clock starts (about 37 seconds) and guards wake. Fly the 195 m to the stone arch before the door of fire shuts, over and under the five bars of fire that cross the road. Too late: the door holds you out of the arch; touch the light again.
   - **The watchers** (1775 m, 5676 m): two statues sweep sheets of red light over the road. Pass when a sheet has gone by. A touch hurts and wakes two guards.
   - **Rune stones** (2320 m, 3446 m): three stones throw fans of orbs. Change height and side between the fans.
   - **The wind teeth** (2870 m): a gust pushes you towards walls of ice-wind. Fly in the middle and hold against the gust; go forward in the calm.
   - **The cracked seal** (4008 m, 5125 m): stones fall on you at any height. Stand over the seal until a stone is coming, then leave: three stones on the seal break it.
   - **The Kaji press** (3590 m): a hammer falls every 3.6 seconds and throws orbs in rings. The link above it can be hurt only just after a blow. Break the link.
   **Places.** 26 named places stand beside the road: an inn, arches, a chapel, statues, a throne. Coming to one for the first time gives 8 crystals and a line in the journal (F1, page PLACES).
   **Open clearings.** Nine of the clearings ringed with standing stones are empty: nobody ambushes you there, and the chest is yours.
   **In a fight** the creatures follow you all the way to the walls; there is no safe corner behind the clearing.
   **Creatures and your wings.** Height alone does not save you. Ali and Kudiani fly up to you, the small creatures leap, the giants throw stones, and packs follow you off the road.
3. **Arenas.** In each round clearing ringed with standing stones you are ambushed. Stone walls rise out of the ground across the road, 20 m beyond the clearing at each end, so you have the clearing and a stretch of road on both sides to fight in. They stand until every wave is beaten; then they sink back. Winning restores some health and saves the game.
4. **Old gold chests** stand beside the road and in every arena. Break one with any attack.
5. **Cities.** The road runs through seven cities (below). Nothing attacks you inside one.
6. **Bosses** wait at the Dragon Lake, in Devi's Gorge and on the Witches' Hill.
7. **At the mountain** you fight the Eagle, your father. When he falls, climb to the ledge and break the two chain anchors.

The panel at the top left shows the region, how far along the road you are, your gold, Eagle Feathers and memories found. The line under it says what to do next.

### What chests hold

| Find | Effect |
|---|---|
| Old gold coins | Counted in the panel. There is nothing to spend them on yet. |
| A memory of the cave | One of twelve things Amirani said to you. One new memory per region. |
| Moon Shard | 15 crystals |
| Eagle Feather | Your attacks hit 3% harder, for the rest of the journey |
| Soul Ember | Your skills are ready again at once and you hit 50% harder for 20 seconds |

The chest in each arena always holds an Eagle Feather; a boss's hoard holds three and heals you fully.

### Creatures

| Creature | How it fights | Advice |
|---|---|---|
| Chinka | Small, fast, in packs | Feather Cyclone (V) gathers them |
| Ali, Kudiani | Keep away and throw orbs | Hold the Axe Barrier (E), or close in with Talon Dash (F) |
| Ochokochi | Fast, leaps at you | Step aside when the ring under him flares |
| Almasty, Kaji | Heavy blows, thrown rocks and embers | Hit, move, hit |
| Uaig, Devi | Slow, very strong | Never stand still in front of them |

A bright ring under a creature means its blow is about to land. Move out of it.

### How creatures fight

- **Light and heavy blows.** A red ring on the ground is an ordinary blow. A wider **yellow** ring is a heavy blow: it comes slower, reaches farther and hurts 60 % more. Step out of it or guard.
- **Two blows.** Quick creatures (the Chinka, spirits, witches, the Ochokochi) sometimes strike a second time straight after the first.
- **Hit and run.** A Chinka springs back out of reach after it strikes, and jumps aside now and then when you attack it from the front. A small creature that is badly hurt keeps its distance and comes in only to strike.
- **Round your guard.** A creature whose blow you blocked runs round to your back.
- **Giants strike the ground.** A Uaig or a Devi that cannot reach you marks a red circle where you stand and breaks the ground there a second later. Leave the circle, or be in the air.
- **Shots are aimed ahead of you.** Spirits and witches shoot at where you are running to; every third volley is a fan of three. The Kaji breathes a ring of fire round itself every third shot.

### The seven cities

The road is the main street of seven Georgian cities. Walking through a city gate shows its name. The first time you enter a city you are fully healed and you keep its gift until the journey is restarted.

| City | Known for | Its gift |
|---|---|---|
| Telavi (Kakheti) | Wine: vines, pergolas and clay jars | Kakhetian wine: every potion you drink heals 10 more |
| Tbilisi (Kartli) | The warm sulphur baths under the old fortress | Warm springs: winning a wave or an arena heals twice as much |
| Mtskheta (Kartli) | The walled cathedral and the church on the rock | Blessing of the pillar: every potion heals 15 more |
| Gori (Shida Kartli) | The hill fortress and the cave town cut into rock | Heart of stone: you take a fifth less damage |
| Kutaisi (Colchis) | The hall of the Golden Fleece | Gold of Colchis: chests hold twice the gold |
| Batumi (Adjara) | The port: lighthouse, piers and boats | Sea wind: three Eagle Feathers |
| Mestia (Svaneti) | The stone towers of the Svans | Svan steel: your attacks do a quarter more damage |

### The maps

- **Minimap** (top right). It turns with the camera: what is ahead of the view is at the top. Gold line: the road ahead; grey line: the road behind you. Crossed swords: the next fight. Small diamonds: chests. Red dots: creatures. A coin with an emblem: a city. On the rim, a pulsing arrowhead points along the road ahead, and small marks point to the next city, the next fight and the mountain when they are out of range. Under it: the city you are in, or the next city and the distance to it along the road.
- **World map** (press **M**; press it again to close). The whole road from its beginning to the mountain: the green line is what you have walked, the white arrow is Nyxara, crossed swords mark the next fight. All seven cities are named on it. The list at the right says what each city is known for and, once you have been there, what it gave you. The road stands still while the map is open.

### Dying, saving and starting again

- Health never comes back by itself. Felling a creature gives none; a beaten wave gives 20 and a won arena 30. Chests do not heal. They hold potions that she carries; press Z to drink one (see Growing stronger).
- After you are hit, nothing can hurt you for one second. Use it to get out of the crowd.
- If you fall, Nyxara lies for a few seconds and wakes where the game was last saved; the fight you lost starts again. Nothing you carry is lost.
- The game saves by itself (after each fight, chest, place and trap, each purchase, and when you leave). **F8** saves where you stand, at any moment outside a fight: that is then where you wake, and where the next session begins.
- The game saves after every arena and every chest. Starting the game again continues from the last arena you won.
- To begin the whole journey again, hold **F9**. A bar fills for a second and a half; let go before it is full to cancel. Everything won on the road is lost. It does not work while you are dead or while the journal or the map is open.

## Moving

| Input | What happens |
|---|---|
| R, walking | Nyxara takes wing: she rises 6 m in 3 seconds and is flying. The key list at the bottom right shows what R will do (**FLY** or **LAND**). |
| W A S D, flying | **W** flies where the camera looks: look up with the mouse and she climbs, look down and she dives, look level (the view the game begins with) and she flies level. Going forward she leans along her flight like an eagle, wings held wide; with no key she hovers. A, S and D slide her sideways and back. She can climb 45 m above where she took off. Flying is a way of going, not a height: she stays flying when she fights, when she touches the ground and when a window is open. Resting on the ground in flight, look up and press W and she lifts again. |
| W A S D, walking | She goes on foot at an easy jog (3.2 m/s), her back low and her wings half open, beating with her stride. Hold **Shift** to run (5.4 m/s), **Ctrl** to stalk slowly (1.4 m/s). Her legs keep pace with the ground at each. |
| Shift (hold) | Walking: run. Flying: wing boost, much faster, with a trail of feathers. |
| Ctrl (hold) | A slow walk |
| Space | Walking: jump. Flying: hold it to climb straight up, as far as the ceiling (45 m over the ground under you). **X** held sinks. W still goes where the camera looks. |
| R, flying | Lands her: the change takes 3 seconds, then she walks. It works only within 6 m of what is under her (the ground or a roof); higher, the game says "too high to land": look down and fly lower first. Only R ends a flight (and death). |
| Roofs | Fly low over a building and press R: she comes down on its roof, wall or tower and walks there. Walk off the edge and she falls to the ground. From the ground a building's walls still stop her. |
| Mouse | Turn the view. Move the mouse down to look from high above, up to look level or slightly upward. |

You can keep moving while attacking. Nyxara keeps facing her target and her legs and wings keep flying.

## Game controller

An Xbox-layout controller (anything Windows sees as an XInput controller) works as soon as it is connected. The key names on the screen change to the controller's buttons when you use it and back when you touch the keyboard or mouse.

| Controller | What it does |
|---|---|
| Left stick | Move |
| Right stick | Turn the view |
| A | Jump |
| B | Nothing (there is no controller button for R yet: taking wing and landing need the keyboard) |
| X | Slash (left mouse button) |
| Y | Heavy attack (right mouse button) |
| RB | Talon Dash (F) |
| RT | Feather Cyclone (V) |
| D-pad up | Moon Leap (C) |
| LB (hold) | Axe barrier (E) |
| D-pad left | Swap axes (Q) |
| LT (hold) | Wing boost (Shift) |
| Left stick click (hold) | Walk on foot (Ctrl) |
| D-pad right | World map (M) |
| D-pad down | Story journal (F1); the left stick turns its pages and scrolls |
| Back / View | Nyxara's window (I) |
| Right stick click | Drink a potion (Z) |
| Start / Menu | Her menu (Esc) |

In menus: D-pad or left stick moves, **A** chooses, **B** goes back. Starting the journey again (hold F9) is on the keyboard only.

## Bow

**Q** changes between the axes and the Eldritch longbow. Whichever she is not using is carried on her back: the bow across it, or the two axes crossed.

| Input | With the bow out |
|---|---|
| Right click (hold) | **Aim**: the camera comes in over her right shoulder, the view narrows a little, and a mark shows at the middle of the screen. As she raises the bow the view turns by itself to the creature nearest the middle (within 22 degrees); with none there it comes up level. She turns with the camera. The mouse turns the view more slowly while she aims, and more slowly still while the mark is on a creature. Let go and the view goes back to where it was |
| Left click (hold) | **Arrows**, five a second, as long as the button is held. Aimed, an arrow flies to the mark and does 27. Not aimed, it finds the nearest creature within 32 m ahead of the camera and does 18. An arrow bends a little toward the creature it was loosed at, if that moves |
| F / V / C | Her three skills are axe work: pressing one takes the axes out again |
| E (hold) | The barrier works with the bow too |

Arrows reach 60 m. They do not stop the game for a moment as axe blows do. She can walk, run or fly while she shoots.

What the screen tells you: four corners stand round the creature the arrows will go to (pale when she is not aiming, red when the mark is on it); the mark itself turns red on a creature; four slanting ticks flash when an arrow hits, gold when it fells the creature.

She holds the bow in her left hand and draws with her right: side-on to the target, the bow arm out, the string hand at her chin. The string bends to her hand and an arrow of light lies on it; her wings are lowered and swept back so that you can see her arms from behind.

## Axes

| Input | Attack |
|---|---|
| Left click | **Right Talon**, then **Left Talon**: each click swings the other axe, as long as you keep clicking |
| Right click | **Charged Slash**: slower, wider, hits harder and knocks enemies back |
| G | **Axe Exchange**: Inferno and Serpentcoil swap hands |
| E (hold) | **Axe Barrier**: two spectral blades rise in front of her and block orbs, for up to 5 seconds |

If an enemy is close but out of reach, an axe attack steps in toward it.

How the axe attacks link:

- **Click early.** A left click during a slash is kept and the other axe swings as soon as the first has landed. A right click during a slash is kept too and the Charged Slash follows: slash, slash, heavy.
- **After the Charged Slash** a left click made during it starts a slash a moment after it lands, without waiting for the swing to finish.
- **The barrier comes first.** E raises the Axe Barrier at any moment of a slash, the Charged Slash or the Axe Exchange. There is no dodge; this is how you save yourself in the middle of an attack.

## Skills

Skills fire the moment you press them, even in the middle of an axe attack. Each has a cooldown shown in the skill bar.

| Key | Skill | Cooldown | What it does |
|---|---|---|---|
| F | **Talon Dash** | 4 s | Dashes the way you steer, cutting each enemy she passes through, then throws a lance. The direction is the **W A S D** keys you hold (or the left stick); with none held, the way she faces. The camera does not aim it. |
| V | **Feather Cyclone** | 7 s | A cyclone of feathers that she carries with her; it drags nearby enemies into her axes |
| C | **Moon Leap** | 9 s | She rises, dives onto her target, and a pillar of moonlight strikes where she lands |

## Combos

From a skill's first hit the gold **COMBO** prompt shows its two follow-ups. Press left or right click while it is showing. A click made while the skill is still striking is kept, and the combo starts the moment the skill's last hit has landed, so a skill never loses a hit to its combo. After the skill ends you still have 1.2 seconds.

A skill or a combo has a flourish after its last hit. You do not have to wait it out: a slash, the Charged Slash, the Axe Barrier or the Axe Exchange cuts it short.

| After | Left click | Right click |
|---|---|---|
| F · Talon Dash | **Twin Talons**: three lunging slash waves | **Rising Eagle**: an uppercut that launches enemies |
| V · Feather Cyclone | **Tempest Triple**: three spinning slashes | **Whirlwind Talons**: throws the cyclone forward; it travels, pulls and shreds |
| C · Moon Leap | **Soul Weave**: volleys of homing feathers from the air | **Moonfall Dive**: a higher leap and a bigger slam |

## The screen

- **Bottom left:** your health (green, red when low; the pale part is what you just lost), your soul bar (blue), and which axe is in which hand. The edge of the screen turns red when you are hit or low on health.
- **Bottom centre:** eight slots for your actions with their keys, in three groups: attacks, eagle skills, and swap, guard and boost. A darkened slot with a number is cooling down; it flashes when it is ready again.
- **Above the slots:** the COMBO prompt, with a bar showing how long it stays open.
- **Over each creature:** its health. A boss has a large bar at the top of the screen.
- **Top left:** the chapter and region, how much of the road is walked, your gold, feathers and memories, and under them what to do now. In a fight it shows the wave and one red dot for each creature left.
- **Top centre:** the title of a region when you enter it, small, for a few seconds. Nothing else is written over the fight; the story is in the journal.
- **Top right:** the minimap. Under it: what a chest held or a city gave.
- **Just above the slots:** the name of the move you just used.
- **Bottom right:** the keys for the journal (F1), the map (M), Nyxara's window (I; it lights up when you have crystals to spend) and starting again (F9), with your potions above them.

## Sound


The game has music, the sounds of the fight and of the road, and a storyteller.

- **Music** changes with where you are: the start screen, the road, a city, a fight, a guardian.
- **The storyteller** speaks when you enter a region, when you enter a city for the first time, when a chest gives you a memory of the cave, and at the end. Music and the world turn down while he speaks. On the Legend page of the journal, press **R** to hear the legend.
- **Nyxara** speaks a few words when a fight begins, when it is won and when she evolves.

Volumes are in **Settings**, **Audio**: Master, Effects (fight, world, voices), Interface (windows) and Music.

All sounds in this version are placeholders made by the computer: synthesised effects and music, and the voices that come with Windows. The music follows Georgian song: a held bass voice, two voices above it, a panduri, a flute and a drum; the city plays a round dance in 6/8 and a fight the war dance in 5/8. Sound stops while the game window is not in front.

## Her menu (Esc)

**Esc** closes whatever window is open. With nothing open it opens Nyxara's own menu; the road stands still behind it. Choose with **W** and **S**, take a row with **Enter** or **Space**, and press **Esc** again to go back to the road.

| Row | What it does |
|---|---|
| Continue the journey | Closes the menu |
| Nyxara | Her window (the same as I) |
| The story | The journal (F1) |
| The map | The world map (M) |
| Settings | Graphics, audio, controls, interface. Leaving them brings you back to this menu |
| Guide | The eight pages of the guide |
| To the start screen | Saves and returns to the start screen |
| Leave the game | Saves and closes the game |

There is no dodge any more: Nyxara has her barrier (E), her wings and her skills. The O key does nothing.

## Settings

Graphics, audio, controls and interface settings are in the start screen and in her menu under **Settings**. They are saved for your Windows user and survive reinstalling the game.

## The land

Grass grows beside the road, thick in the marsh and the meadows, thin on snow, ash and rock. Trees stand back from the road: broadleaf woods in the lowlands, firs in Ochokochi's Wood and the high country. Clouds drift across the night sky in front of the moon. None of it can be touched: Nyxara stays on the road.
