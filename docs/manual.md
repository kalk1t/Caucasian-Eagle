# Caucasian Eagle: player manual (v0.5.3)

[← Project index](../README.md)

## What the game needs

| | Lowest (estimated, not tested) | Better |
|---|---|---|
| System | Windows 10, 64-bit | Windows 11, 64-bit |
| Processor | 64-bit, 4 cores | 8 cores or more |
| Graphics | A graphics card with DirectX 11 and 4 GB of its own memory | 6 GB or more |
| Memory | 8 GB | 16 GB |
| Disk | 2 GB free | the same, on an SSD |
| Screen | 1280 x 720 | 1920 x 1080 |
| Controls | Keyboard and mouse | or an Xbox-style (XInput) controller |
| Sound | Any sound device | |
| Internet | Not needed | |

The game has run on one computer only: Windows 11, Ryzen 9 9950X3D, 64 GB of memory, GeForce RTX 5080, where it holds 60 frames a second. The "lowest" column is an estimate from what version 0.5.3 was measured to use there (2026-11-25, the whole climb from the shore to the summit): 1.46 GB on disk once installed; about 1.3 GB of memory in use at once (1.0 GB of its own); about 2 GB of the graphics card's own memory (0.5.2 used 0.4 GB on the climb: 0.5.3's trees, camera and light take more); and while it loads it keeps many processor cores busy. No weaker computer has been tried: on a card with less than 4 GB of its own memory the game may run slowly or not at all.

## Installing

Download `CaucasianEagle-0.5.3-Setup.exe` from the [releases page](https://github.com/kalk1t/Caucasian-Eagle/releases) and run it. The file is not signed yet, so Windows may show "Windows protected your PC": choose **More info**, then **Run anyway** (see [Why Windows warns](#why-windows-warns)). The game installs for your Windows user only (no administrator question), by default into `%LOCALAPPDATA%\Programs\Caucasian Eagle`, and adds **Caucasian Eagle** (the Eagle's Ascent, the game of 0.5) and **Caucasian Eagle - The Road (0.4)** to the Start menu, and a desktop shortcut to the Ascent if you tick it. To remove it, use **Installed apps** in Windows settings; your settings and your saved journey in `%LOCALAPPDATA%\CaucasianEagle` are kept. Instead of installing you can unpack `CaucasianEagle-0.5.1-win64.zip` anywhere and start `CaucasianEagle.exe` (the Eagle's Ascent) or `Play-TheRoad.cmd` (the road of 0.4) in it.

### Why Windows warns

"Windows protected your PC" (SmartScreen) means Windows does not know the file yet: it is not signed with a code-signing certificate, so Windows cannot say who made it, and it has not been downloaded often enough to have a reputation. It is not a finding of a virus: Microsoft Defender finds nothing in the 0.5.0 or 0.5.1 files. The game asks for no administrator rights, adds nothing that starts with Windows, and installs no service. Where Smart App Control is on, Windows may refuse an unsigned program outright; Smart App Control has no "run anyway". Signing is planned.

To check a download: the release page gives a `.sha256` file beside each download. In PowerShell, `Get-FileHash CaucasianEagle-0.5.3-Setup.exe` must print the same SHA-256. That shows the file is the one the release page lists, unbroken; it does not show who made it (a signature would).

## Starting

The start screen is drawn in the same panel, gold and ink as Nyxara's windows. At the left it has five rows: **Begin the journey** (or **Continue the journey**), **Settings**, **Guide**, **Credits** and **Leave the game**. Choose with **W** and **S** or the arrow keys and take with **Enter** or **Space**, or point and click with the mouse. **Guide** opens a short version of this manual inside the game, seven pages: the journey, moving and keys, fighting, health, growing, what the game needs, and who made it. **Credits** opens the last of them. **A** and **D** turn the pages; **Esc** closes the guide. Along the bottom of the start screen stands who made the game and its engine.
**Settings** opens the game's own settings window (the same window opens from her menu, **Esc**, in play). It has what this game uses and nothing else: the window (whole screen or in a window, and its size; these two wait for **Apply the window**, and a changed window asks "Keep this window?" and goes back by itself after 15 seconds if you do not answer), smooth frames (vertical sync), frames a second at most (30, 60, 120 or no limit; since 0.5.4), shadows, four volumes (everything, music, fights/world/voices, menus), mouse speed, look up/down reversed, the size of the interface, and the frame counter. **W** / **S** choose, **A** / **D** change, **Enter** takes, **Esc** goes back; the mouse works too (click the left or right half of a value). Everything except the window is applied and saved as you change it. **Restore the defaults** resets all but the window. With vertical sync on, each frame waits for the screen, so the screen's own rate is the most whatever the limit says, and the picture never tears; with it off nothing but the limit holds the frames, and over the screen's rate the picture may tear. The line under the rows says which of the two holds while you stand on either row.

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
| Swords | 3 evolutions | 40, 140, 320 | Blows deal 20 % more |
| Armour | 3 evolutions | 40, 140, 320 | Blows taken hurt 12 % less |
| Power | 8 levels | 10, 20 ... 80 | Blows deal 7 % more |
| Defence | 8 levels | 10, 20 ... 80 | Blows taken hurt 5 % less |

**Nyxara evolves three times.** When her wings, swords and armour have all reached the same evolution, she herself changes: Fledgling, then Moonwing, Stormwing, Heir of the Eagle. Each form deals 10 % more, takes 8 % less, stands a little taller, and heals her fully when she reaches it.

Evolved wings, swords and armour glow in the colour of their tier: blue, then green, then gold. Their shapes do not change.

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
| Chinka | Small and fast, in packs. Claws, often twice in a row; pounces on you from a few metres off; springs back after it strikes; hops aside from your blows and claws straight back | The feather storm (C) strikes every one near her |
| Ali | Keeps away and throws orbs. Come close and it shrieks a ring of cold round itself and glides back; it glides aside from your blows | Hold the Sword Barrier (E), or reach it with the thrown swords (V) |
| Kudiani | Keeps away and throws orbs. Curses the ground where you will be: a red circle that bursts a moment later. Hops back from your blows | Leave the circle; reach her with the thrown swords (V) |
| Ochokochi | Claws, slams both fists down, sweeps wide; charges at you from farther off; steps aside from your blows and strikes back | Step aside when the ring under him flares |
| Almasty | Heavy blows; close in under it and it pounds the ground all round itself; throws rocks; steps aside or back from your blows | Hit, move, hit |
| Kaji | Hammer blows; throws embers; breathes a short fan of fire at you, and now and then lets a ring of fire go round itself; vanishes into its fire out of your blows and steps out beside you to strike | Hit, move, hit |
| Uaig | Slow and very strong: slams, wide sweeps, a stamp when you are close under it; throws rocks up at you when you are above it; takes a heavy step back from your blows | Never stand still in front of it |
| Devi | As the Uaig, and charges at you from far off | Never stand still in front of it |

A bright ring under a creature means its blow is about to land. Move out of it.

### How creatures fight

- **Light and heavy blows.** A red ring on the ground is an ordinary blow. A wider **yellow** ring is a heavy blow: it comes slower, reaches farther and hurts 60 % more. Step out of it or guard.
- **Two blows.** Quick creatures (the Chinka, spirits, witches, the Ochokochi) sometimes strike a second time straight after the first.
- **Hit and run.** A Chinka springs back out of reach after it strikes. A small creature that is badly hurt keeps its distance and comes in only to strike.
- **They dodge.** A creature that sees your blow coming (you begin it facing the creature, near it) may get out of its way, each kind its own way: the Chinka hops aside, the Kudiani mostly hops back, the Ali glides aside, the Ochokochi steps aside and the Almasty aside or back, the Kaji vanishes into its fire and steps out a few metres off, the Uaig and the Devi mostly take a heavy step back. The Chinka, the Ochokochi and the Kaji strike back at once. Having dodged, a creature does not dodge again for a few seconds.
- **From farther off.** The Chinka pounces, the Ochokochi and the Devi charge (a yellow ring round its feet: it is coming), the Kudiani curses the ground where you will be, the Kaji breathes fire, the Ali shrieks when you come close.
- **They face you as they go.** Circling you between their blows, or falling back to shoot, creatures keep their eyes on you and step sideways or backwards.
- **Round your guard.** A creature whose blow you blocked runs round to your back.
- **Giants strike the ground.** A Uaig or a Devi that cannot reach you marks a red circle where you stand and breaks the ground there a second later. Leave the circle, or be in the air.
- **Shots are aimed ahead of you.** Spirits and witches shoot at where you are running to; every third volley is a fan of three.

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

## The Eagle's Ascent

The game of version 0.5: a mountain to climb, apart from the road, from the sea to Amiran's court at 4,200 m, on foot and on her wings, past the creatures of the mountain. Since 0.5 it is what **Caucasian Eagle** in the Start menu, and `CaucasianEagle.exe` itself, start. The road to Amirani of 0.4 is still in the game: **Caucasian Eagle - The Road (0.4)**, `Play-TheRoad.cmd`, or `CaucasianEagle.exe --road`. The start screen is the game's own.

- **The way up** is a path of streets, stairs and terraces about 7.6 km long, with rings round the mountain and 89 caves beside it. Where it needs wings the way itself shows it: a cliff where the road goes on above (take wing, then Space to climb), a gap where the road is broken (take wing and fly across), a cleft in the rock (fly through it). Rising air carries her up; a crosswind pushes her on one ridge. No words hang over the way (since 0.5.3): the **M** map names the nests.
- **Grass fields** cover the meadows, the terraces and the slopes beside the way, green below and straw-coloured high up; they thin out on the forest floor and stop at the path, rock and snow. **Every tree stops her**, on foot and on her wings: she cannot walk or fly through a trunk (the crowns' leaves let her through).
- **Off the way** (since 0.5.3): the whole mountain beside the path is hers to walk, on the meadows, the slopes and the ridges. A rock face, a bank or a slope too steep to stand on is a wall: she slides along it, or down it, instead of walking into the rock, and she still walks off a ledge and falls. She cannot walk into the mountain through the ground at a cliff's foot.
- **The camera** (since 0.5.3): move the mouse up to look up, nearly straight up: the camera swings down behind her and in close, over her right shoulder, until she sinks to the bottom of the view. On stairs it glides with her instead of jumping at each step; where a slope or a wall behind her would bring it into her it rises over it and looks down. Trees between the camera and her are drawn see-through.
- **Its forests** change as she climbs: pines with wild oranges and pomegranates by the sea; orchards on the village terraces; beech, oak, maple and hornbeam in the gorge and the great forest; Nordmann firs and oriental spruces above; birches bent by the snow among junipers and rhododendrons; and last the firs under snow.
- **Her wings.** **Space** held 1 second on foot takes her up at once: she springs 6 m into the air in under a second, and while Space is still held she climbs on (a tap jumps, even in the middle of a combo). Her wings carry her **10 seconds** at first, and each gear she earns (see *Growing*) longer: 15, 22 and, in the last, 30 seconds. Then she falls; on the ground they fill again in 20. In flight **R** lands her wherever she is. There is no ceiling: fly as high as you like. Rock stops her in flight, and in the caves the walls and the roof hold her on foot and in flight.
- **Fighting.** Her two swords, the same as in the Character Lab: **left click** and **right click** each go on through a string of moves, also after a jump; the strings, and the light of her blows, are her gear's (since 0.5.4; see *Her gear's way of fighting*); **Space** jumps out of a move at any moment; **F** is her slide, a dash low along the ground, on foot only (since 0.5.4 F does nothing while she flies); **E** wraps her wings round her for 3 seconds while she goes on fighting (see *Her swords*). V and C are not hers on the mountain any more (since 0.5.2). About one in two of her blows that would kill a creature, any creature, the bosses too, brings it down dazed instead, and she finishes it with one of four finishing kills; the others kill it at once (since 0.5.4 by chance; see *Her finishing kills*).
- **Growing: her quests (Tab) and her gears (since 0.5.3; her quests' key was Q until 0.5.4).** She sets out in her own plain gear and grows by her quests; the nine levels of 0.5.2, and buying them in a nest, are gone. **Tab** opens her quests anywhere, and the climb waits: **W** / **S** (or the pointer) choose one, **Enter** (or its **TAKE**) buys it with crystals, **Tab** or **Esc** closes. (With the mountain or the map open, **Tab** changes between those two, as before.) They come in three sets, one at a time: first **three** (bought with wind crystals), and when all three are done she wears **THE HIGHLANDS** at once; then **six** (fire crystals), for **AMIRANI'S FIRE**; then **nine** (ice crystals), for **MOONLIT**. A quest asks her to beat so many creatures, break so many clay jars, or reach an Eagle Nest (that nest or any higher one counts), and it counts only from the moment she takes it: what she did before does not count. A set's quests cost 38 wind, 131 fire and 182 ice crystals in all. Under her crystals, at the left of the screen, every quest she has taken and not yet done is listed, however many (until 0.5.4 five at the most, and the done ones stayed), under *QUESTS*, the **Tab** key and *n of m done*. A quest done turns gold there, a line is drawn through it, and it leaves the list, all in two seconds; in her quests' window it stays, marked done. *QUEST DONE* shows when one is. A set done, the new gear is hers at once and saved where she last saved (*Her quests are done: wings, swords and armour, all new. Saved.*); if she falls, she wakes at that nest wearing it.
- **Her gears.** Each gear is new wings, swords and armour, and makes her stronger: **THE HIGHLANDS** (royal eagle wings, the sabres of Queen Tamar, her harness): her blows deal x 1.6, the blows she takes hurt x 0.8, 15 s of flight; **AMIRANI'S FIRE** (ember wings, ember blades, obsidian and ember armour: her blows burn): x 2.3, x 0.6, 22 s; **MOONLIT** (night-sky wings, star-crystal blades, the celestial harness): x 3.2, x 0.3, 30 s, as strong as level 9 of 0.5.2. With each she stands a little taller, and the lights on her wings, at her blades and at her feet take its colour (the Highlands' gold, the fire's embers, the moonlight). She always wears her newest gear. Each gear also fights its own way (since 0.5.4): its own strings of moves, and its own light on her blows and on her finishing kills (see *Her gear's way of fighting*). Her strings play at three quarters of the speed they were made at (in 0.5.3 all her moves played at half); her slide (F) at one and a half times it, three times as fast as it was.
- **Her window (I)** opens anywhere and the climb waits: she stands front-on on a stage of open sky, both swords in her hands (drag to turn round her, the wheel closer and further), between two panels: on the left her gear, what it gives her and the three gears in their order with how far her quests are for the next; on the right her crystals of each kind (and which kind the creatures and jars leave where she is) and what she carries. **I**, **Esc** or a click on **CLOSE** close it, and she is back where she stood. There is nothing to buy in it now. There is no gold on the mountain.
- **Crystals** come from two things. **The creatures**: each one she beats leaves its crystals (a Chinka one, a Devi ten, a boss 25). **Clay jars**: beside every road, in the caves and on the rings round the mountain, every 30 to 50 m, a group of three to five jars stands: the qvevri (the great egg-shaped wine vessel of Georgia, kept since the eighth millennium BC), the doki jug, the kotso pot and the small kvibri. Her blades break them; what is in one shows only then: a healing potion in about one of twelve, a crystal or two in about a third of those beside the way and in far fewer of those in the caves and on the rings, the rest empty. **Three kinds** (since 0.5.3), by the stretch of the way where they are found: **wind** up to the third Eagle Nest, **fire** from there up to the sixth, **ice** beyond. The mountain holds about 130 wind, 297 fire and 545 ice crystals in all, about two thirds of each stretch's on the way itself (its creatures, its boss, its jars) and the rest off it (the caves' creatures and jars, the groups standing beside the way, the jars on the rings). Her three counts are at the top left, under the height, the kind of where she is underlined.
- **Health.** Nothing heals her but potions: no nest, no rest, no beaten creature. **Z** drinks one (35 health). She carries five; a potion found with five already carried is worth a crystal instead. Her potions are shown beside the crystals. Her health bar shows no number, only the bar.
- **The creatures**: a small group of one to three about every 100 m along the way, more in the caves, and (since 0.5.2) sixteen groups standing off the way (seventeen in 0.5.2) beside it, about every 150 to 400 m where the ground lets one stand, each on its own short path out from the way and well apart from the others (a fight of its own): Chinka, Ali, Kudiani, Ochokochi, Almasty, Kaji, Uaig and Devi, stronger the higher she climbs. A group comes at her when one of it sees her or when she strikes one; it follows her only 45 m from its place, then goes home and its wounds close. Never more than three creatures are awake at once.
- **The bosses**: the Gveleshapi rises out of the water about 1,150 m along the way, the Giant waits at about 2,150 m, the Devi Lord at about 3,500 m, and the Eagle, her father, in Amiran's court at the top. While she fights one its name and health are at the top of the screen. Each fights its own way, and gathers itself for a moment before its great blow. **The Eagle** (its own fight since 0.5.3) fights in the air over its court: it rises with a ring of feathers and circles her high, out of her swords' reach, throwing feathers; it swoops down through where she stands and up beyond her (low, she can strike it, and her wing shield throws it down onto the court); it marks her place and dives onto it, and lies there winded, its wings folded, for a moment: hers; it hangs before her and throws a fan of feathers. Hurt to two thirds it calls a storm of feathers down on marks round her and swoops twice in a row; at one third, in its fury, it dives three times in a row and stays down longer after.
- **Eagle Nests and saving (H)**: nine nests beside the way (the Tide Gate top, the village square, the gorge shelf, the forest clearing, the Eagle shrine, the cloud-line rest, the stone refuge, the temple balcony, the snow-line gate). Standing in one, **H** saves, as often as you like, at any nest: *SAVED* shows, and the save holds that nest, her gear, her quests, her crystals of each kind and her potions, every creature she has beaten and every jar she has broken. Standing in a nest, the screen says *SAVE AT THIS EAGLE NEST* (in the one you saved at, *SAVE AGAIN*) with its **H**; the nests are in gold on the **M** map (no words over them on the way since 0.5.3). A save from before 0.5.3 (her nine levels) is not read: the climb starts again from the shore. A nest neither heals nor saves by itself.
- **Falling**: when the creatures beat her she wakes in the nest she last saved at (on the shore before any save), whole, and **everything since that save is undone**: the creatures she beat since stand on the way again, the jars she broke since are whole, the crystals and potions she gained since are gone. Falling into the sea, or out of the world, brings her back to the last nest she stood in, as she is. **Backspace** does the same at any time.
- **The save** is `eagles-ascent.sav`, beside the road's save. Starting the Ascent again goes on from it.
- **Esc** opens the menu and the climb waits: **Continue the climb**, **Nyxara** (her growth), **The mountain**, **Settings** (the game's settings window), **Back to the last nest**, **A new climb** (from the shore; it forgets the save), **To the start screen**, **Leave the game** (only H at a nest saves). **W** / **S**, the arrows or the mouse choose; **Enter**, **Space** or a click take; **Esc** goes back.
- **M** shows the whole mountain in 3D, from outside, with the nests in gold, the bosses in red, the groups not yet beaten as dots and Nyxara with her height. Go over it like a map: **drag** (left button) to move over the whole mountain, **right drag** to turn and tilt, the **wheel** (or **+** **-**) from the whole mountain down to 40 m over the ground, **W A S D** to move, **Q** **E** to turn, **Space** to her. **Tab** shows the detailed map from above (north up: the way, the rings, the caves, the nests, bosses and creatures; drag, wheel, Space as in 3D; the pointer over a mark says what it is) and back. **M** or **Esc** closes either; the climb waits.

## Moving

| Input | What happens |
|---|---|
| Space held, walking | Held 1 second, Nyxara takes wing at once: 6 m up in under a second, climbing on while it is held (a tap jumps). Her wings carry her 30 seconds (the thin bar under her health); when they are spent she falls. On the ground they fill again in 20 seconds. |
| W A S D, flying | **W** flies where the camera looks: look up with the mouse and she climbs, look down and she dives, look level (the view the game begins with) and she flies level. Going forward she leans along her flight like an eagle, wings held wide; with no key she hovers. A, S and D slide her sideways and back. She can climb 45 m above where she took off. Flying is a way of going, not a height: she stays flying when she fights, when she touches the ground and when a window is open. Resting on the ground in flight, look up and press W and she lifts again. |
| W A S D, walking | She goes on foot fast and low, in her way of going called the shadow dash (since 0.5.4): crouched and bowed far forward, her arms swept back, her wings swept back, in long low strides with a flight between each. **W** 9.6 m/s; hold **Shift** to run (16.2 m/s), **Ctrl** to go slower (5.0 m/s). Her feet keep pace with the ground at each, on stairs and slopes too. With her swords in her hands (on guard: fighting, a creature near her, and for 5 seconds after) her arms hold them as she goes. A running jump carries her three times as far as before, as high: about 10 m from W, 17 m from Shift on level ground. Until 0.5.4 she went at a third of these, an easy jog of 3.2 m/s, a run of 5.4 and a slow walk of 1.7, her back low and her wings half open. |
| Shift (hold) | Walking: run (16.2 m/s). Flying: wing boost, much faster, with a trail of feathers. |
| Ctrl (hold) | Walking: slower (5.0 m/s; until 0.5.4 a slow walk) |
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
| X | Left sword (left mouse button) |
| Y | Right sword (right mouse button) |
| RB | The slide (F), on foot |
| RT | Her soul, the thread step (Q; since 0.5.4: nothing before, V not being in the game since 0.5.2) |
| D-pad up | Nothing (C is not in the game since 0.5.2) |
| LB | Wing shield (E) |
| LT (hold) | Wing boost (Shift) |
| Left stick click (hold) | Slower on foot (Ctrl) |
| D-pad right | The map (M) |
| D-pad down | Story journal (F1); the left stick turns its pages and scrolls |
| Back / View | Nyxara's window (I) |
| Right stick click | Drink a potion (Z) |
| Start / Menu | Her menu (Esc) |

In menus: D-pad or left stick moves, **A** chooses, **B** goes back. Starting the journey again (hold F9) is on the keyboard only.

## Her swords

Nyxara fights with two swords, one in each hand, and her wings. (Before this version she had two axes and a longbow; Q and G did those. G does nothing now; since 0.5.4 Q is her soul, below.) Her moves were made and tried in the Character Lab (`Test-Characters.cmd`), from Mixamo animations.

| Input | What she does |
|---|---|
| Left click | On the Eagle's Ascent: her gear's left string, a click for each move (see *Her gear's way of fighting*). On the road: a cut of the **left** sword; clicks during it go on through the left cuts, three in a row |
| Right click | On the Eagle's Ascent: her gear's right string. On the road: a cut of the **right** sword, four in a row. The last cut of either chain hurts most (80; the others 45) |
| Space, then a click | On the Eagle's Ascent: her string's moves, in the jump (since 0.5.2). On the road: **the leaping cut**, the hardest cut (110) and the longest reach |
| Space, in a move | She leaves the move at once and jumps (since 0.5.2) |
| F | **The slide**, her dash on foot (since 0.5.4; in 0.5.3 it was a slow slide of 6 m): low along the ground the way she faces, 13 m in a second, cutting every creature within a blade's length of her way (35 a cut, and again for every metre she goes on past or through it, four times at the most) and breaking the clay jars beside it. In her gears it leaves their light behind her. A rock face stops it and a tree stops it or turns it aside, as they do her on foot; it goes up any slope she can go up on foot; over a ledge it carries her off, at a bend of the way too. A creature it would kill dies of the cut or, about one time in two, is left dazed behind her, and then falls by itself in 3 seconds unless she is back beside it for her finishing kill. Not while she flies: there F does nothing, and the screen says *THE SLIDE: FROM THE GROUND*. 4 s to come back |
| E | **The wing shield**: one press, and both wings wrap round her with blue light running along them for **3 seconds**; blows and orbs from every side are stopped, and she goes on cutting, using F, walking, jumping and flying under it. Then it rests 7 seconds (its slot shows the count) |
| Q | **Her soul: the thread step** (since 0.5.4), on foot: her soul stays where she stands, she as she stood in pale blue light on a ring of light, and she is thrown 6 m on in a blink. Six thin threads of light run from it to her, wherever she goes and for as long as she likes, fighting, jumping and flying. **Q again**, from anywhere, in the air too: she is pulled back to it along them in a second at most, through whatever stands between, nothing touching her on the way, and stands where she left it, turned as it stands. Then it rests 15 s (its slot counts it down). Q works in the middle of a cut, a combo or her slide too, and she is free to go on or fight as soon as she is thrown. While her soul waits, its slot reads RETURN and is lit. Taken back to a nest or the start, or beaten, she lets it go |
| V, C | Nothing since 0.5.2: the thrown swords and the feather storm are taken from her until new skills come |
| A / D | Turn her way, and the view with it; alone they also carry her round in a curve |
| S | Step back, slowly, still facing ahead |

You can walk and run while you cut: her legs keep their stride and her body cuts. While a skill or the thrown swords play, the move itself carries her. In the air her cuts are made in flight, her wings still beating; her blows cannot reach a creature she is high over.

What a blow leaves on the road: a bright streak along the cut, sparks, and blood thrown the way the blade went, which falls and lies on the ground for a few seconds. On the Eagle's Ascent the light is her gear's (below); the blood is the same.

### Her gear's way of fighting

On the Eagle's Ascent each gear fights its own way (since 0.5.4): its own strings of moves, and its own light behind her blades, where a blow bites and round a creature she kills. Three ways were made for each gear in the Character Lab (`Test-Characters.cmd`: **G** the gear, **1 2 3** the way), fast and sharp, heavy, and flowing; the owner chose the flowing one of each for the game.

| Her gear | Its way | Left click | Right click | Its light |
|---|---|---|---|---|
| As she set out | **Her blades alone** | Pinwheel, Crescent, Cyclone | Swallow Dart, Twin Slash, Pinwheel | None: her cuts, their blood and their weight. Her gears bring her the wind, the fire and the ice |
| The Highlands | **Sky Wheel** | Pinwheel, Crescent, Cyclone | Swallow Dart, Twin Slash, Pinwheel | The wind: wide blue trails that hang behind her blades, a golden edge on them, golden rings, feathers |
| Amirani's fire | **Fire Dance** | Twin Slash, Pinwheel, Gale Rush, Cyclone | Crescent, Hit and Fade, Swallow Dart, Pinwheel | Sheets of flame behind her blades, embers turning round her, the ground scorched |
| Moonlit | **Moon Wraith** | Crescent, Twin Slash, Cyclone | Flash Step, Pinwheel, Swallow Dart, Eagle Dive | Teal trails and their pale echo a moment after, dark smoke, wisps round what she strikes |

The moves: **Pinwheel**, one turn with both blades out, cutting all round her; **Crescent**, a wide level sweep of the left blade; **Cyclone**, turn after turn, three times round; **Swallow Dart**, a dash in under its guard with a rising cut, and a hop back out; **Twin Slash**, a cut down from high on her left and straight back across; **Gale Rush**, running in with two cuts; **Hit and Fade**, a cut down from her left and a hop back out of reach; **Flash Step**, a blink in and the cut as she arrives; **Eagle Dive**, a rising cut that takes her up, and both blades down as she comes down. The name of the move she is making shows over her slots.

Her slide (**F**) has her gear's light too: its wake along the ground behind her, the trails of her blades, and its bite where she cuts in passing. As she set out it has none. Her wing shield (**E**) looks as it did.

### Her finishing kills

About one in two of her blows that would kill a creature, while she is on foot and near it, brings it down instead (since 0.5.4 by chance; until then every such blow did): it stands dazed, its head hanging, a violet ring pulsing under it (one that flies comes down to the ground). As soon as her move ends she finishes it with one of four finishing kills, chosen at random and never the same one twice running. All four work on every creature, from the 1.15 m Chinka to the 9 m Gveleshapi and the bosses:

| Finishing kill | What she does |
|---|---|
| **Reaper's Wheel** | One whole turn with both blades out through it, and her right blade brought down from high |
| **Mountain Breaker** | A cut across it, a hop, and both blades driven into the ground before it: the ground shakes round her |
| **Tempest Flurry** | Four cuts: high from her left, back across, low at its legs (it sinks), and across its neck |
| **Headsman** | A cut at its legs drops it to its knees; her right blade raised high and brought down on its neck |

A finishing kill takes two to three seconds and nothing can hurt her while it plays. Each is fitted to the creature's size: at a tall one she rises on her wings into the blow. One creature is doomed at a time; a doomed creature she does not come to within 3 seconds falls by itself. Where none of the four has room (at the edge of a drop, against a wall) her blow kills it at once, as does a blow from afar or in flight, and as does every blow that chance does not turn into a finishing kill.

On the Eagle's Ascent the finishing kills are lit by her gear's way (since 0.5.4). Her blades leave its trails as she goes in, each cut bites with its light, and the kill has its own last light round the creature, made to its size: rings going up it one after another, golden, in the Highlands; in Amirani's fire a ring of fire over the ground and flames standing up round it, the ground left scorched; in Moonlit a teal ring and its pale echo, and pale wisps rising round it. A creature she kills without a finishing kill has the same last light. As she set out, before her first gear, none of this is lit: her blades, the blood and the weight of her blows. The violet ring under a doomed creature is as it was (red and yellow rings on the ground are the creatures' own warnings).

## The screen

- **Bottom left:** your health, a bar without a number (green, red when low; the pale part is what you just lost), your soul bar (blue), and which axe is in which hand. The edge of the screen turns red when you are hit or low on health.
- **Bottom centre:** eight slots for your actions with their keys, in three groups: attacks, eagle skills, and swap, guard and boost. A darkened slot with a number is cooling down; it flashes when it is ready again. On the Eagle's Ascent (since 0.5.4) Q's slot, SOUL, reads RETURN and is lit while her soul waits for her.
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

There is no dodge any more: Nyxara has her wing shield (E), her wings and her slide. The O key does nothing.

## Settings

Graphics, audio, controls and interface settings are in the start screen and in her menu under **Settings**. They are saved for your Windows user and survive reinstalling the game.

## The land

Grass grows beside the road, thick in the marsh and the meadows, thin on snow, ash and rock. Trees stand back from the road: broadleaf woods in the lowlands, firs in Ochokochi's Wood and the high country. Clouds drift across the night sky in front of the moon. None of it can be touched: Nyxara stays on the road.
