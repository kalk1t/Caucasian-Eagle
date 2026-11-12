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
             {{L"The road", L"About 6.3 km through twelve regions. Lanterns mark it. The whole land is yours to walk; a compass and gold lights show the way back."},
              {L"Trouble", L"Between fights: violet cursed ground (walk round), falling stones (leave the rings), blue wisps (slip between)."},
              {L"Arenas", L"In each clearing ringed with standing stones you are ambushed. Walls rise until every wave is beaten."},
              {L"Chests", L"Break old gold chests with any attack: gold, crystals, potions, memories, feathers, runes."},
              {L"Cities", L"Seven Georgian cities. Nothing attacks you inside one. Each heals you once and gives a lasting gift."},
              {L"Guardians", L"Three wait: at the Dragon Lake, in Devi's Gorge, on the Witches' Hill. At the end, the Eagle, your father."},
              {L"The mountain", L"When the Eagle falls, climb to the ledge and break the two chain anchors."},
              {L"Saving", L"The game saves by itself after every arena and every chest. You continue where you were."},
              {L"Dying", L"Choose respawn: you return to the last arena you won. Nothing you carry is lost."},
              {L"F9, held", L"Begins the whole journey again. Everything won on the road is lost."}}},
            {L"MOVING AND KEYS", L"Keyboard and mouse. An Xbox-style controller works too; its buttons are in the manual.",
             {{L"W A S D", L"She lifts off and flies upright, low over the ground. Let go and she lands."},
              {L"Mouse", L"Turns the view. In her windows it is a pointer: point at a row or a card and click it."},
              {L"SHIFT, held", L"Wing boost: much faster."},
              {L"CTRL, held", L"Walk on foot."},
              {L"SPACE", L"Jump. Hold it to rise into free flight."},
              {L"Z", L"Drink a potion."},
              {L"I", L"Nyxara's window: evolve wings, swords and armour. D shows what she carries."},
              {L"F1", L"The story journal."},
              {L"M", L"The world map."},
              {L"ESC", L"Her menu: continue, settings, this guide, leave. It also closes any window."}}},
            {L"FIGHTING", L"Every creature warns you before it strikes. Watch the ground.",
             {{L"Left mouse", L"Sword slash: left sword, then right, for as long as you keep clicking."},
              {L"Right mouse", L"Charged slash: slower, wider, hits harder and knocks creatures back."},
              {L"E, held", L"Sword barrier: blocks blows and orbs from the front, for up to 5 seconds."},
              {L"Q", L"The two swords swap hands."},
              {L"F", L"Talon Dash: a rush through the creatures in front of her."},
              {L"V", L"Feather Cyclone: drags nearby creatures into her blades."},
              {L"C", L"Moon Leap: she rises and dives onto her target."},
              {L"COMBO", L"During or right after a skill, left or right mouse gives its two follow-ups. A click made early is kept."},
              {L"Red ring", L"A blow is coming. Step out of the ring, or raise the barrier toward the creature."},
              {L"Yellow ring", L"A heavy blow: slower, wider, and it hurts much more."},
              {L"Red circle", L"A giant will break the ground there in a moment. Leave it, or be in the air."},
              {L"Shots", L"Spirits and witches aim where you are running to. Change direction."}}},
            {L"HEALTH", L"There is no healer on the road. Know where health comes from.",
             {{L"By itself", L"1 health every 2 seconds, always."},
              {L"A creature felled", L"10 health."},
              {L"A wave beaten", L"20 health. An arena won: 30."},
              {L"Potions", L"From chests. She carries up to five. Z drinks one: 35 health."},
              {L"A city", L"Heals you fully the first time you enter it."},
              {L"Evolving", L"Each new form of Nyxara heals her fully."},
              {L"Chests", L"Do not heal. They hold potions, not health."},
              {L"After a hit", L"Nothing can hurt you for one second. Use it to get out of the crowd."}}},
            {L"GROWING", L"Creatures leave crystals. Spend them in her window (I): W and S choose, ENTER spends.",
             {{L"Wings", L"Three evolutions. Skills return sooner and the boost is faster."},
              {L"Swords", L"Three evolutions. Her blows deal more."},
              {L"Armour", L"Three evolutions. Blows hurt her less."},
              {L"Power, Defence", L"Eight levels of each."},
              {L"She evolves", L"When all three reach the same evolution she changes: Moonwing, Stormwing, Heir of the Eagle."},
              {L"Charms", L"Creatures drop them. Three of a kind: stronger against that kind of creature."},
              {L"Runes", L"From chests. Set one kind in her blades: fire, frost or moon."},
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
