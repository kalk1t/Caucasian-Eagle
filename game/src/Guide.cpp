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
            {L"THE JOURNEY", L"You are Nyxara, daughter of the Eagle. Amirani is chained at the top of the mountain. Climb to him.",
             {{L"The way up", L"From the sea to Amiran's court at 4,200 m: about 7.6 km of streets, stairs and terraces, rings round the mountain, 89 caves."},
              {L"Off the way", L"The whole mountain beside the path is yours to walk. Rock faces and cave walls stop her; so does every tree."},
              {L"Wings needed", L"A cliff where the road goes on above, a gap where it is broken, a cleft in the rock: take wing."},
              {L"Creatures", L"A small group about every 100 m, more in the caves, and groups off the way. Stronger the higher she climbs."},
              {L"Bosses", L"The Gveleshapi from the water, the Giant, the Devi Lord; at the top the Eagle, your father, in the air over his court."},
              {L"Clay jars", L"Beside every road and in the caves. Break them: crystals and potions."},
              {L"Eagle Nests", L"Nine beside the way. Standing in one, H saves. The M map shows them in gold."},
              {L"Beaten", L"She wakes at the nest she saved at, and everything since that save is undone."},
              {L"Backspace", L"Back to the last Eagle Nest she stood in."},
              {L"The road", L"The old road of 0.4 is still in the game: Caucasian Eagle - The Road (0.4) in the Start menu."}}},
            {L"MOVING AND KEYS", L"Keyboard and mouse. An Xbox-style controller works too; its buttons are in the manual.",
             {{L"Space, held", L"Held a second, she takes off. Her wings (the bar under her health) last 10 s, and longer with each gear: 30 s in the last; on the ground they fill again."},
              {L"R", L"In flight, close to the ground, a roof or a wall: she lands and walks there. The landing takes three seconds."},
              {L"W A S D", L"W goes the way she faces, S steps back, A and D turn her. Flying: the mouse tips her up and down. She fights in both."},
              {L"Mouse", L"Turns the view. In her windows it is a pointer: point at a row or a card and click it."},
              {L"SHIFT, held", L"On foot: run (Ctrl, held: a slow walk). Flying: wing boost, much faster."},
              {L"SPACE", L"Walking: jump. Flying: hold it to climb; hold Ctrl + Space to fly down (X sinks too)."},
              {L"Z", L"Drink a potion."},
              {L"I", L"Nyxara's window: her gear, what it gives her, her crystals."},
              {L"TAB", L"Her quests, anywhere: what she can take, how far she is with those taken."},
              {L"F1", L"The story journal."},
              {L"M", L"The mountain in 3D, gone over with the mouse. Tab: the map."},
              {L"ESC", L"Her menu: continue, settings, this guide, leave. It also closes any window."}}},
            {L"FIGHTING", L"Every creature warns you before it strikes. Watch the ground.",
             {{L"Left mouse", L"Her combo of the left: click again during a move and the next one follows. On foot, flying or in a jump. Its moves and its light are her gear's."},
              {L"Right mouse", L"Her combo of the right. Each gear fights its own way: the Highlands' sky wheel, the fire dance, the moon wraith."},
              {L"A, D", L"Turn her slowly round. The view stays where the mouse puts it. S steps back."},
              {L"E", L"Wing shield: for 3 seconds it stops blows and orbs from every side while she goes on fighting. Then it rests 7 seconds."},
              {L"F", L"The slide, her dash on foot: low along the ground, 13 m in a second, cutting what she passes. Not while she flies."},
              {L"Red ring", L"A blow is coming. Step out of the ring, or raise her wing shield (E)."},
              {L"Yellow ring", L"A heavy blow: slower, wider, and it hurts much more."},
              {L"Red circle", L"A giant will break the ground there in a moment. Leave it, or be in the air. Shots are aimed where you are going: turn."},
              {L"In the air", L"Blows cannot reach you over a creature's head. Spirits and witches fly up after you, small creatures leap, giants throw stones."}}},
            {L"HEALTH", L"Nothing on the mountain heals her but potions. Know where they are.",
             {{L"By itself", L"Never: not in a nest, not at rest."},
              {L"A creature beaten", L"Nothing. Beating one does not heal."},
              {L"Potions", L"In the clay jars. She carries five; one found with five carried is worth a crystal. Z drinks one: 35 health."},
              {L"Her gear", L"Each gear she earns takes more of every blow off her; in the last she takes less than a third."},
              {L"Wing shield", L"E: for 3 seconds blows and orbs from every side are stopped."},
              {L"Finishing kills", L"About one kill in two ends in one. Nothing can hurt her while it plays."},
              {L"Beaten", L"She wakes whole at the nest she last saved at; what she won since is undone."},
              {L"Into the sea", L"Or out of the world: back to the last nest she stood in, as she is."}}},
            {L"GROWING", L"Nyxara grows by her quests: each set of them done, she wears a new gear.",
             {{L"Her quests (TAB)", L"TAB shows them, anywhere: W and S choose, ENTER takes. Each is bought with crystals. Those she is on are listed at the left."},
              {L"Three sets", L"Three quests first, for the Highlands. When all three are done, six for Amirani's fire, then nine for Moonlit."},
              {L"Crystals", L"Creatures and clay jars leave them, by the stretch of the way: wind up to the third Eagle Nest, fire up to the sixth, then ice."},
              {L"Their price", L"The first set is bought with wind crystals, the second with fire, the third with ice."},
              {L"Taken first", L"A quest counts only once she has taken it: creatures beaten and jars broken before do not count."},
              {L"A set done", L"She wears its gear at once, and it is saved. If she falls, she wakes at the nest she last saved at, wearing it."},
              {L"Her gears", L"The Highlands, Amirani's fire, Moonlit: new wings, swords and armour each. She deals more, takes less and flies longer."},
              {L"Her window (I)", L"Her gear and what it gives her, how far her quests are for the next, her crystals of each kind."}}},
            {L"WHAT THE GAME NEEDS", L"The game has run on one computer only, a fast one. The lowest figures below are estimates, not tests.",
             {{L"System", L"Windows 10 or 11, 64-bit; a 4-core processor (better, 8)."},
              {L"Graphics", L"DirectX 11. Estimated: 4 GB of its own memory (it uses about 2); better, 6 GB."},
              {L"Memory", L"Estimated: 8 GB (it uses about 1.3); better, 16 GB."},
              {L"Disk", L"2 GB free (installed: 1.46 GB)."},
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
