#pragma once

// Nyxara's looks (the owner, 2026-11-21: "each upgrade of Wings changes her wings, each upgrade of armor upgrades and
// changes her armor and whole set she wears and upgrading swords replaces her swords with different swords").
//
// Her growth has three items, each evolved three times (Growth: wings, swords, armour; tier 0 is her as she set out).
// A look gives every tier its own sight: other swords in her hands, her wings in another plumage and bigger, her armour
// in another metal with pieces of its own. Several examples (ways she could grow) are tried in the Character Lab (keys
// 1, 2, 3...), the owner chooses one, and the game takes that one.
//
// A bundle with looks carries Looks.txt (art/nyxara/looks writes it) and, in its mesh, the looks' parts as materials
// named Look_... (hidden at load by NyxaraKit::LoadBody). A tier shows its parts (show=), gives her body and her wings
// other textures (WorldRenderer::SetCharacterMaterialLook: no second copy of either mesh), and sets her wings' size
// (NyxaraKit::SetWingScale). Her own swords (material Sword) are hidden while any other swords are shown.

#include <DirectXMath.h>
#include <filesystem>
#include <string>
#include <vector>

class Renderer;
class WorldRenderer;

namespace CaucasianEagle::NyxaraLooks
{
    enum Item { Wings, Swords, Armour, ItemCount };   // in Growth's order
    constexpr int TierCount = 4;                       // 0 as she set out, then three evolutions

    struct Tier
    {
        std::wstring Name;                       // what the tier is called ("Lammergeier wings")
        std::vector<std::string> Show;           // parts of her mesh shown (Look_...)
        std::filesystem::path Body, BodyNormal, BodyRough, BodyMetal;   // her body's textures (relative to the bundle)
        std::filesystem::path Plumage;           // her wings' colour texture
        float Glow = -1;                         // her wings' own light (0..1; < 0: none)
        float Span = 1;                          // her wings' size
        float BodyGlow = -1;                     // her body's own light (0..1; < 0: none)
    };
    struct Example
    {
        std::wstring Title, Line;                // "MOONLIT", what it is about
        Tier Items[ItemCount][TierCount];
    };

    // Reads <bundle>/Looks.txt. A bundle without it has no examples (Count() 0); false only when the file is wrong.
    bool Load(const std::filesystem::path& bundle, std::string& error);
    int Count();
    const Example& Get(int example);             // 0-based

    // Her look: tiers[Wings], tiers[Swords], tiers[Armour] of that example (0 to 3 each). False with 'error' when a
    // texture does not load (she is then as far as it got).
    bool Apply(Renderer& renderer, WorldRenderer& worldRenderer, int example, const int tiers[ItemCount], std::string& error);
    // The same, each item from an example of its own: examples[i] (0-based) at tiers[i].
    bool Apply(Renderer& renderer, WorldRenderer& worldRenderer, const int examples[ItemCount], const int tiers[ItemCount], std::string& error);

    // Her levels in the Eagle's Ascent (the owner, 2026-11-21: "make the upgrade 9 times instead of 3 ... the make the list
    // like this 1-3-2 / first 1 2 3 is nyxara's level 1 2 3, then level 4 5 6 will be thirds 1 2 3 and then last level 7 8
    // 9 will be second's 1 2 3"): Looks.txt's line "levels 1 3 2" gives the examples in that order, three tiers each.
    // Level 0 is her own; level L (1 ..) is tier (L - 1) % 3 + 1 of the example it falls in. No such line: no levels.
    int LevelCount();                                       // 9 with "levels 1 3 2"; 0 without the line
    bool LevelLook(int level, int& example, int& tier);     // false past LevelCount()
    const Tier& LevelTier(int item, int level);             // its name, what it shows
    int LevelGroup(int level);                              // 0 her own, then 1, 2, 3: the example's place in that order
    const Example& LevelExample(int level);                 // the example a level is of (level 0: the first's)
    // Her look with each item at a level (0 .. LevelCount()), as the Ascent has her wear it
    bool ApplyLevels(Renderer& renderer, WorldRenderer& worldRenderer, const int levels[ItemCount], std::string& error);
}
