#define NOMINMAX
#include "Story.h"

#include "GameUi.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace CaucasianEagle::Story
{
    // ------------------------------------------------------------ words (story/STORY.md, sections 1, 2 and 5)
    const Chapter Chapters[12] = {
        {L"The Moonlit Causeway", L"Year one. The gate stands open. The mountain waits at the end of the road."},
        {L"The Whispering Wood", L"Chinka laugh in the dead branches. The Kudiani witches have lit their green fires."},
        {L"The Drowned Marsh", L"Do not answer the women in the water. The Ali know your name."},
        {L"Ochokochi's Wood", L"The goat-man hunts here, with the blade that grows from his chest."},
        {L"The Stone Towers", L"The people of the towers left long ago. The wild men of the heights did not."},
        {L"The Snow Pass", L"Year four. The Almasty throw the mountain at those who climb it."},
        {L"Kajeti", L"The fortress of the Kaji smiths. Their hammers renew the chain every year."},
        {L"The Giants' Valley", L"The one-eyed Uaig guard the bones of their fathers."},
        {L"The Dragon Lake", L"The Gveleshapi sleeps under the black water. The road crosses its back."},
        {L"Devi's Gorge", L"Three heads, one hunger. The Devi keep the gold of every traveller who came before you."},
        {L"The Witches' Hill", L"Rokapi, queen of the Kudiani, is chained to this hill as he is chained to his."},
        {L"The Chain Road", L"Year seven. The cave stands open. Your father has seen you coming."}};

    // The seven cities. What each one gives is applied in Journey.cpp (EnterCity); the numbers are in GameConstants.h.
        const City Cities[CityCount] = {
            {L"Telavi", L"Kakheti", L"The wine of Kakheti",
             L"Vines climb every wall and clay jars as tall as a man sleep in the ground. They pour for anyone who walks toward the mountain.",
             L"Kakhetian wine", L"Every creature you fell gives you 5 more health."},
            {L"Tbilisi", L"Kartli", L"The warm springs",
             L"The city was built where a king's falcon fell into hot water. Brick domes steam over the baths under the old fortress.",
             L"Warm springs", L"Every wave and every arena you win heals you far more."},
            {L"Mtskheta", L"Kartli", L"The oldest church",
             L"The first cathedral stands inside its own wall where two rivers meet, and a small church watches from the rock across the water.",
             L"Blessing of the pillar", L"Every potion heals you 15 more."},
            {L"Gori", L"Shida Kartli", L"The fortress and the cave town",
             L"A fortress crowns the hill, and beside it a whole town is cut into the rock: halls, streets and hearths older than any king.",
             L"Heart of stone", L"Every blow that lands on you is weaker."},
            {L"Kutaisi", L"Colchis", L"The Golden Fleece",
             L"This was the kingdom the Argonauts sailed to. The Fleece still hangs in a hall of white columns, and gold runs in the rivers.",
             L"Gold of Colchis", L"Every chest holds twice the gold."},
            {L"Batumi", L"Adjara", L"The port on the Black Sea",
             L"Boats rock at the piers under a striped lighthouse. Sailors know every wind, and they know the eagle's.",
             L"Sea wind", L"Three eagle feathers: your blades bite deeper."},
            {L"Mestia", L"Svaneti", L"The towers of Svaneti",
             L"Every family here has its own stone tower and has never been conquered. The smiths forge for the last climb.",
             L"Svan steel", L"Your blades strike much harder."}};

        const wchar_t* const Memories[12] = {
        L"\"You have his eyes,\" Amirani said. \"But you looked at me. He never has.\"",
        L"\"I stole nothing. I asked why men should kneel. For that, a chain.\"",
        L"\"My dog has licked this iron thin a thousand times. Each year the smiths strike, and it is whole again.\"",
        L"\"Seven years, little eagle. Learn the sword. Learn the sky. Then come back for me.\"",
        L"\"Your father does not hate me. That is the worst of it. He only obeys.\"",
        L"You asked him if it hurt. He laughed until the chain rang.",
        L"\"When I am free I will remember who opened the chain.\" He did not say how.",
        L"The old women say: if Amirani walks, the smiths' fires go out for ever. They do not say if that is good.",
        L"Your father taught you to fly before you could walk. He never told you where he went at dawn.",
        L"\"He will not strike to kill you,\" Amirani said. \"Use that.\"",
        L"Rokapi's curse: \"He charmed me too, girl. Look where I am.\"",
        L"The dog watches you climb. It has stopped licking the chain. It is waiting."};

    namespace
    {
        struct Passage { const wchar_t* Heading; const wchar_t* Text; };
        const Passage LegendText[] = {
            {L"THE LEGEND",
             L"Amirani fought against God. For that he is chained in a cave on the Caucasus Range, and an eagle feeds on his liver "
             L"without end. His faithful dog licks the chain to wear it thin. Every year the smiths appointed by God strike it whole "
             L"again. Once in seven years the cave opens, and Amirani can be seen in his chains."},
            {L"THE DAUGHTER OF THE EAGLE",
             L"You are Nyxara, daughter of the Eagle that feeds on him. Seven years ago, on the day the cave stood open, you saw your "
             L"father at his work, and Amirani spoke to you. He asked you to train, and to return when the cave opens again: to fight "
             L"your father and open the chain."},
            {L"THE ROAD",
             L"The seven years are over. The road begins under the full moon, at the gate that faces the mountain. Break the old gold chests you find. "
             L"Nothing on this road wants you to arrive."}};
        const Passage EndText[4] = {
            {L"FATHER AND DAUGHTER",
             L"The Eagle lifts his head from the wound. \"Seven years he has whispered to you. I have heard every word. I was set "
             L"here. I do not choose. But I will not let you pass, daughter.\""},
            {L"THE EAGLE FALLS",
             L"He folds his wings on the ledge. \"You fly as I taught you,\" he says, and is still. Two iron anchors hold Amirani to "
             L"the mountain."},
            {L"THE CHAIN OPENS",
             L"Amirani stands. The mountain is silent for the first time in an age. He looks down at you and smiles. You cannot tell "
             L"what kind of smile it is."},
            {L"THE THREE LINKS",
             L"You hold up the three links the guardians of the road carried. Amirani looks at them for a long time. \"You broke "
             L"more than my chain, little eagle,\" he says, and this smile you can read. He kneels beside your father and lays the "
             L"links on his wing. The Eagle breathes."}};
        const wchar_t* PageNames[PageCount] = {L"THE LEGEND", L"THE ROAD", L"MEMORIES", L"THE END"};

        // ---- journal layout, in layout units
        constexpr float PanelWidth = 1060, PanelHeight = 664, PanelSideGap = 34, PanelTopGap = 22;
        constexpr float HeaderY = 24, TabsY = 76, ContentTop = 116, ContentBottom = 58, ContentSide = 64;
        constexpr float BodyPixels = 2.25f, HeadingPixels = 1.75f, EntryTitlePixels = 2.15f, EntryPixels = 1.95f;
        constexpr float LineGap = 5, ColumnGap = 44, NumeralWidth = 46, EntryGap = 9, ProseWidth = 780;
    }

    void DrawJournal(Ui& ui, Journal& j)
    {
        using namespace Palette;
        if (j.Open <= 0) return;
        const float a = j.Open, S = ui.S;
        ui.Rect(0, 0, ui.W, ui.H, Black.Fade(.74f * a));
        ui.Sprite(HudArt::Vignette, 0, 0, ui.W, ui.H, Black.Fade(.7f * a));

        const float pw = std::min(ui.W - 2 * PanelSideGap * S, PanelWidth * S), ph = std::min(ui.H - 2 * PanelTopGap * S, PanelHeight * S);
        const float px = (ui.W - pw) * .5f, py = (ui.H - ph) * .5f + (1 - a) * 14 * S;
        ui.Nine(HudArt::Panel, px, py, pw, ph, White.Fade(a));
        const float corner = 62 * S, inset = 7 * S;
        const float cornerX[4] = {px + inset, px + pw - inset - corner, px + pw - inset - corner, px + inset};
        const float cornerY[4] = {py + inset, py + inset, py + ph - inset - corner, py + ph - inset - corner};
        for (int i = 0; i < 4; ++i) ui.Sprite(HudArt::Corner, cornerX[i], cornerY[i], corner, corner, White.Fade(.9f * a), 1.5707963f * static_cast<float>(i));

        // ---- header and page tabs
        const float cx = px + pw * .5f;
        ui.SpacedCentered(L"THE ROAD TO AMIRANI", cx, py + HeaderY * S, 3.f * S, 5 * S, GoldLight.Fade(a));
        ui.Sprite(HudArt::Divider, cx - 250 * S, py + (HeaderY + 28) * S, 500 * S, 30 * S, White.Fade(a));
        float tabsWidth = 0;
        const float tabPixels = 1.9f * S, tabTracking = 2.5f * S, tabGap = 46 * S;
        for (int i = 0; i < PageCount; ++i) tabsWidth += ui.SpacedWidth(PageNames[i], tabPixels, tabTracking) + (i ? tabGap : 0);
        float tx = cx - tabsWidth * .5f;
        for (int i = 0; i < PageCount; ++i)
        {
            const float tw = ui.SpacedWidth(PageNames[i], tabPixels, tabTracking), ty = py + TabsY * S;
            const bool active = i == j.Page, locked = i == End && j.EndReached == 0;
            ui.Spaced(PageNames[i], tx, ty, tabPixels, tabTracking, (active ? GoldLight : locked ? Faint : Dim).Fade(a));
            if (active)
            {
                ui.Rect(tx, ty + ui.Height(tabPixels) + 5 * S, tw, 2 * S, Gold.Fade(a));
                ui.Icon(HudArt::Diamond, tx + tw * .5f, ty + ui.Height(tabPixels) + 6 * S, 12 * S, White.Fade(a));
            }
            if (j.Unread & (1u << i)) ui.Icon(HudArt::Marker, tx + tw + 9 * S, ty + 3 * S, 16 * S, White.Fade(a * (.7f + .3f * std::sin(ui.Time * 5))));
            tx += tw + tabGap;
        }

        // ---- page
        const Doorways::DoorwaysUiRect view{px + ContentSide * S, py + ContentTop * S, pw - 2 * ContentSide * S, ph - (ContentTop + ContentBottom) * S};
        ui.SetClip(&view);
        const float top = view.Y - j.Scroll * S;
        float bottom = top;
        const auto prose = [&](const Passage* passages, int count, int unlocked, const wchar_t* lockedText)
        {
            const float width = std::min(view.Width, ProseWidth * S), x = view.X + (view.Width - width) * .5f;
            float y = top + 6 * S;
            for (int i = 0; i < count; ++i)
            {
                if (i >= unlocked)
                {
                    ui.Spaced(passages[i].Heading, x, y, HeadingPixels * S, 3 * S, Faint.Fade(a));
                    y = ui.Paragraph(lockedText, x, y + ui.Height(HeadingPixels * S) + 8 * S, BodyPixels * S, width, LineGap * S, Faint.Fade(a)) + 22 * S;
                    continue;
                }
                ui.Icon(HudArt::Diamond, x - 16 * S, y + ui.Height(HeadingPixels * S) * .5f, 11 * S, White.Fade(a));
                ui.Spaced(passages[i].Heading, x, y, HeadingPixels * S, 3 * S, Gold.Fade(a));
                y = ui.Paragraph(passages[i].Text, x, y + ui.Height(HeadingPixels * S) + 8 * S, BodyPixels * S, width, LineGap * S, Ink.Fade(a)) + 22 * S;
            }
            bottom = y;
        };
        // twelve numbered entries in two columns of six
        const auto entries = [&](int unlocked, int current, const auto& title, const auto& body, const wchar_t* lockedText)
        {
            const float columnWidth = (view.Width - ColumnGap * S) * .5f, textWidth = columnWidth - NumeralWidth * S;
            for (int column = 0; column < 2; ++column)
            {
                const float x = view.X + static_cast<float>(column) * (columnWidth + ColumnGap * S);
                float y = top + 4 * S;
                for (int i = column * 6; i < column * 6 + 6; ++i)
                {
                    const bool open = i < unlocked;
                    const std::wstring numeral = Ui::Roman(i + 1);
                    ui.RightAligned(numeral, x + (NumeralWidth - 14) * S, y, EntryTitlePixels * S, (open ? Gold : Faint).Fade(a));
                    if (i == current) ui.Icon(HudArt::Marker, x - 12 * S, y + ui.Height(EntryTitlePixels * S) * .5f, 20 * S, White.Fade(a));
                    const float tx2 = x + NumeralWidth * S;
                    const wchar_t* heading = open ? title(i) : nullptr;
                    if (heading) { ui.Text(heading, tx2, y, EntryTitlePixels * S, Ink.Fade(a)); y += ui.Height(EntryTitlePixels * S) + 3 * S; }
                    y = ui.Paragraph(open ? body(i) : lockedText, tx2, y, EntryPixels * S, textWidth, 3 * S, (open ? (heading ? Dim : Ink) : Faint).Fade(a)) + EntryGap * S;
                }
                bottom = std::max(bottom, y);
            }
        };
        switch (j.Page)
        {
        case Legend: prose(LegendText, 3, 3, L""); break;
        case Road:
            entries(j.ChaptersReached, j.CurrentChapter, [](int i) { return Chapters[i].Title; }, [](int i) { return Chapters[i].Text; },
                L"Not yet walked.");
            break;
        case MemoriesPage:
            entries(j.MemoriesFound, -1, [](int) { return static_cast<const wchar_t*>(nullptr); }, [](int i) { return Memories[i]; },
                L"Still in an old gold chest, somewhere on the road.");
            break;
        default: prose(EndText, 4, j.EndReached, L"Not yet lived."); break;
        }
        ui.SetClip(nullptr);
        j.Overflow = std::max(0.f, (bottom - top - view.Height) / S);
        if (j.Overflow > 0)
        {   // scroll bar at the right edge of the page
            const float trackX = view.Right() + 18 * S, thumb = view.Height * view.Height / (view.Height + j.Overflow * S);
            ui.Rect(trackX, view.Y, 3 * S, view.Height, White.Fade(.08f * a));
            ui.Rect(trackX, view.Y + (view.Height - thumb) * std::clamp(j.Scroll / j.Overflow, 0.f, 1.f), 3 * S, thumb, Gold.Fade(.8f * a));
        }

        // ---- footer: what is found, and the keys
        const float fy = py + ph - 44 * S, keyHeight = 22 * S, labelPixels = 1.6f * S, labelY = fy + (keyHeight * .86f - ui.Height(labelPixels)) * .5f;
        const std::wstring found = L"REGIONS " + std::to_wstring(j.ChaptersReached) + L" / 12     MEMORIES " + std::to_wstring(j.MemoriesFound) + L" / 12";
        ui.Text(found, px + ContentSide * S, labelY, labelPixels, Dim.Fade(a));
        float kx = px + pw - ContentSide * S;
        const auto hint = [&](const wchar_t* label, std::initializer_list<const wchar_t*> keys)
        {
            kx -= ui.Width(label, labelPixels);
            ui.Text(label, kx, labelY, labelPixels, Dim.Fade(a));
            kx -= 8 * S;
            for (auto it = std::rbegin(keys); it != std::rend(keys); ++it)
            {
                const float w = ui.KeyWidth(*it, keyHeight);
                ui.Key(*it, kx - w * .5f, fy, keyHeight, White.Fade(a));
                kx -= w + 4 * S;
            }
            kx -= 18 * S;
        };
        hint(L"CLOSE", {L"F1"});
        if (j.Overflow > 0) hint(L"SCROLL", {L"W", L"S"});
        hint(L"PAGE", {L"A", L"D"});
        if (j.Page == Legend) hint(L"HEAR IT", {L"R"});
    }
}
