#include "Guide.h"

#include "GameUi.h"

#include <string>

namespace CaucasianEagle::Guide
{
    namespace
    {
        struct Line { const wchar_t* Head; const wchar_t* Text; };   // Head: a key or a name, in gold; Text: what it does
        struct Page { const wchar_t* Title; const wchar_t* Lead; Line Lines[12]; };

        const Page Pages[PageCount] = {
            {L"THE JOURNEY", L"You are Nyxara, daughter of the Eagle. Amirani is chained on the mountain. Follow the old road to him.",
             {{L"The road", L"About 6.3 km through twelve regions. The whole land is yours; a compass and gold lights show the way back."},
              {L"Trouble", L"Between fights: violet cursed ground (walk round), falling stones (leave the rings), blue wisps (slip between)."},
              {L"Traps", L"Nine, built for one who flies: they reach to the sky. The line under the compass says what each asks. A green light over the road opens a gate of fire far ahead, briefly: fly, with SHIFT."},
              {L"Places", L"Twenty-six beside the road. Come near one to find it; the journal (F1) keeps them."},
              {L"Arenas", L"Where walls rise you are ambushed until every wave is beaten. Packs roam between, and come from the land too."},
              {L"Chests", L"Break old gold chests with any attack: gold, crystals, potions, memories, feathers, runes."},
              {L"Cities", L"Seven Georgian cities. Nothing attacks you inside one. Each heals you once and gives a lasting gift."},
              {L"Guardians", L"Three wait on the road; at its end, the Eagle, your father. When he falls, break the two chain anchors on the ledge."},
              {L"Saving, dying", L"The game saves by itself, and F8 saves where you stand. If you fall, you wake where it was last saved."},
              {L"F9, held", L"Begins the whole journey again. Everything won on the road is lost."}}},
            {L"MOVING AND KEYS", L"Keyboard and mouse. An Xbox-style controller works too; its buttons are in the manual.",
             {{L"Space, held", L"Held for a second and a half on foot, she takes off. Her wings carry her for 30 seconds (the thin bar under her health); when they are spent she falls. On the ground they rest and the bar fills again."},
              {L"R", L"In flight, close to the ground: she lands and walks. The landing takes three seconds."},
              {L"W A S D", L"W goes the way she faces, S steps back, A and D turn her. Flying: the mouse tips her up and down. She fights in both."},
              {L"Mouse", L"Turns the view. In her windows it is a pointer: point at a row or a card and click it."},
              {L"SHIFT, held", L"On foot: run (Ctrl, held: a slow walk). Flying: wing boost, much faster."},
              {L"SPACE", L"Walking: jump. Flying: hold it to climb; hold Ctrl + Space to fly down (X sinks too)."},
              {L"Roofs", L"Fly low over a roof, a wall or a tower and press R: she lands on it and walks there."},
              {L"Z", L"Drink a potion."},
              {L"I", L"Nyxara's window: evolve wings, axes and armour. D shows what she carries."},
              {L"F1", L"The story journal."},
              {L"M", L"The world map."},
              {L"ESC", L"Her menu: continue, settings, this guide, leave. It also closes any window."}}},
            {L"FIGHTING", L"Every creature warns you before it strikes. Watch the ground.",
             {{L"Left mouse", L"A cut of the left sword. Click again during it and the cuts go on: three of the left. In a jump: the leaping cut."},
              {L"Right mouse", L"A cut of the right sword: four in a row. The last cut of either hurts most."},
              {L"A, D", L"Turn her slowly round. The view stays where the mouse puts it. S steps back."},
              {L"E, held", L"Sword barrier: blocks blows and orbs from the front, for up to 5 seconds."},
              {L"F", L"The slide: low along the ground, cutting what is before her. Flying: the twisted dash, striking every creature she goes through."},
              {L"V", L"The thrown swords: they stick in the nearest creature before her; she twists after them, sticks both feet on it, takes them in both hands and backflips off it, ripping them out."},
              {L"C", L"The feather storm: she leaps, turns three times in the air and throws her wing feathers all round her like daggers, as far as 27 m. Every creature within reach is struck at each turn."},
              {L"Red ring", L"A blow is coming. Step out of the ring, or raise the barrier toward the creature."},
              {L"Yellow ring", L"A heavy blow: slower, wider, and it hurts much more."},
              {L"Red circle", L"A giant will break the ground there in a moment. Leave it, or be in the air. Shots are aimed where you are going: turn."},
              {L"In the air", L"Blows cannot reach you over a creature's head. Spirits and witches fly up after you, small creatures leap, giants throw stones."}}},
            {L"HEALTH", L"There is no healer on the road. Know where health comes from.",
             {{L"By itself", L"Never. Health comes back only from the things below."},
              {L"A creature felled", L"Nothing. Killing does not heal."},
              {L"A wave beaten", L"20 health. An arena won: 30."},
              {L"Potions", L"From chests. She carries up to five. Z drinks one: 35 health."},
              {L"A city", L"Heals you fully the first time you enter it."},
              {L"Evolving", L"Each new form of Nyxara heals her fully."},
              {L"Chests", L"Do not heal. They hold potions, not health."},
              {L"After a hit", L"Nothing can hurt you for one second. Use it to get out of the crowd."}}},
            {L"GROWING", L"Creatures leave crystals. Spend them in her window (I): W and S choose, ENTER spends.",
             {{L"Wings", L"Three evolutions. Skills return sooner and the boost is faster."},
              {L"Axes", L"Three evolutions. Her blows deal more."},
              {L"Armour", L"Three evolutions. Blows hurt her less."},
              {L"Power, Defence", L"Eight levels of each."},
              {L"She evolves", L"When all three reach the same evolution she changes: Moonwing, Stormwing, Heir of the Eagle."},
              {L"Charms", L"Creatures drop them. Three of a kind: stronger against that kind of creature."},
              {L"Runes", L"From chests. Set one kind in her axes: fire, frost or moon."},
              {L"Feathers", L"Twelve feathers of her father lie at the edge of the road, one in each region."},
              {L"Verses, keepsakes", L"One of each from every city."},
              {L"Chain links", L"One from each guardian. With all three the story ends differently."}}},
            {L"WHAT THE GAME NEEDS", L"The game has run on one computer only, a fast one. The lowest figures below are estimates, not tests.",
             {{L"System", L"Windows 10 or Windows 11, 64-bit."},
              {L"Graphics", L"A graphics card with DirectX 11. Estimated: 2 GB of its own memory; better, 4 GB."},
              {L"Memory", L"Estimated: 4 GB; better, 8 GB."},
              {L"Disk", L"1.5 GB free."},
              {L"Screen", L"1280 x 720 or larger."},
              {L"Controls", L"Keyboard and mouse, or an Xbox-style game controller."},
              {L"Sound", L"Any sound device."},
              {L"Internet", L"Not needed."},
              {L"Tried on", L"Windows 11, Ryzen 9 9950X3D, 64 GB, GeForce RTX 5080: 60 frames a second."}}},
            {L"WHO MADE IT", L"Caucasian Eagle: the road to Amirani.",
             {{L"The game", L"Created by Giorgi Samkharadze."},
              {L"The engine", L"Built in Doorways, a custom game engine founded by Giorgi Samkharadze in February 2026. Doorways is written in C++ on Direct3D 11; the game is not made in a bought or borrowed engine."},
              {L"The legend", L"Amirani, chained on the Caucasus, and the eagle: a Georgian legend. The creatures of the road come from Georgian and Caucasian folklore; the game takes liberties with all of them."},
              {L"Models", L"Nyxara, the creatures and the scenery models were generated with Meshy and worked over in Blender. The land, the cities and the gates are built by the game's own scripts."},
              {L"Sound", L"Effects and music are synthesised by the game's own scripts. The voices are the ones that come with Windows; they are placeholders."},
              {L"Year", L"2026."}}}};
    }

    void Draw(Ui& ui, int page, float fade)
    {
        if (fade <= 0) return;
        using namespace Palette;
        const float S = ui.S, a = fade;
        constexpr float Width = 760, Height = 740, Pad = 26, Tiny = 1.55f, Small = 1.85f, Name = 2.6f, HeadColumn = 190;
        page = page < 0 ? 0 : page >= PageCount ? PageCount - 1 : page;
        const Page& p = Pages[page];
        const float w = Width * S, h = Height * S, x = (ui.W - w) * .5f, y = (ui.H - h) * .5f;
        ui.Rect(0, 0, ui.W, ui.H, Black.Fade(.62f * a));
        ui.Nine(HudArt::Panel, x, y, w, h, White.Fade(a));
        ui.SpacedCentered(L"GUIDE", x + w * .5f, y + 16 * S, Tiny * S, 4 * S, Gold.Fade(a));
        ui.Centered(p.Title, x + w * .5f, y + 34 * S, Name * S, Ink.Fade(a));
        // one mark per page
        for (int i = 0; i < PageCount; ++i)
            ui.Icon(HudArt::Diamond, x + w * .5f + (static_cast<float>(i) - (PageCount - 1) * .5f) * 18 * S, y + 72 * S, (i == page ? 12 : 8) * S,
                    (i == page ? GoldLight : Faint).Fade(a));
        float ty = y + 92 * S;
        ty = ui.Paragraph(p.Lead, x + Pad * S, ty, Small * S, w - 2 * Pad * S, 2 * S, Dim.Fade(a)) + 12 * S;
        ui.Rect(x + Pad * S, ty - 6 * S, w - 2 * Pad * S, 1 * S, Gold.Fade(a * .3f));
        for (const Line& line : p.Lines)
        {
            if (!line.Head) break;
            ui.Text(line.Head, x + Pad * S, ty, Small * S, GoldLight.Fade(a));
            const float bottom = ui.Paragraph(line.Text, x + (Pad + HeadColumn) * S, ty, Small * S, w - (2 * Pad + HeadColumn) * S, 2 * S, Ink.Fade(a));
            ty = bottom + 7 * S;
        }
        ui.Centered(std::wstring(L"A  D  turn the page  (") + std::to_wstring(page + 1) + L" of " + std::to_wstring(PageCount) + L")      ESC  close",
                    x + w * .5f, y + h - 30 * S, Tiny * S, Dim.Fade(a));
    }
}
