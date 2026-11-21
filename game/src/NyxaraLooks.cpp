#include "NyxaraLooks.h"

#include "NyxaraKit.h"
#include "Renderer.h"
#include "WorldRenderer.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

// Looks.txt, a line each (# starts a note):
//   example <n> <title> | <what it is about>
//   <wings|swords|armour> <n> <tier 1-3> <name> | key=value ...
// keys: show=<material>[,<material>...]  parts of her mesh shown at that tier
//       lit=<material>:<0..1>            that part's own light
//       tint=<material>:<r>,<g>,<b>      that part's colour multiplied (the same piece in another metal at a later tier)
//       body= bodynormal= bodyrough= bodymetal=   her body's textures (material Nyxara), bodyglow=<0..1>
//       wings=<png> glow=<0..1> span=<scale>       her wings' plumage (material Wings), their own light, their size

namespace CaucasianEagle::NyxaraLooks
{
    namespace
    {
        // a part's light and colour at a tier: one look each (a second call for the same part would undo the first)
        struct PartLook { std::string Part; float Glow = -1; DirectX::XMFLOAT4 Tint{1, 1, 1, 1}; };
        std::vector<Example> gExamples;
        std::vector<std::vector<PartLook>> gLit;   // per example, item and tier: [((e * ItemCount) + item) * TierCount + tier]
        std::filesystem::path gRoot;

        std::wstring Wide(const std::string& s)
        {
            if (s.empty()) return {};
            const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
            std::wstring w(static_cast<size_t>((std::max)(n, 0)), L'\0');
            if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
            return w;
        }
        std::string Trim(const std::string& s)
        {
            const auto a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        }
        size_t LitIndex(int example, int item, int tier) { return (static_cast<size_t>(example) * ItemCount + static_cast<size_t>(item)) * TierCount + static_cast<size_t>(tier); }
        PartLook& PartOf(std::vector<PartLook>& parts, const std::string& part)
        {
            for (auto& p : parts) if (p.Part == part) return p;
            parts.push_back({part});
            return parts.back();
        }
    }

    bool Load(const std::filesystem::path& bundle, std::string& error)
    {
        gExamples.clear(); gLit.clear(); gRoot = bundle;
        std::ifstream in(bundle / "Looks.txt");
        if (!in) return true;
        std::string line;
        int number = 0;
        const auto fail = [&](const std::string& why) { error = "Looks.txt line " + std::to_string(number) + ": " + why; gExamples.clear(); gLit.clear(); return false; };
        while (std::getline(in, line))
        {
            ++number;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const std::string text = Trim(line);
            if (text.empty() || text[0] == '#') continue;
            const auto bar = text.find('|');
            std::istringstream head(text.substr(0, bar));
            const std::string tail = bar == std::string::npos ? std::string() : text.substr(bar + 1);
            std::string kind;
            int example = 0;
            if (!(head >> kind >> example) || example < 1 || example > 9) return fail("no example number (1 to 9)");
            if (gExamples.size() < static_cast<size_t>(example)) { gExamples.resize(static_cast<size_t>(example)); gLit.resize(gExamples.size() * ItemCount * TierCount); }
            Example& x = gExamples[static_cast<size_t>(example - 1)];
            if (kind == "example")
            {
                std::string rest; std::getline(head, rest);
                x.Title = Wide(Trim(rest)); x.Line = Wide(Trim(tail));
                continue;
            }
            const int item = kind == "wings" ? Wings : kind == "swords" ? Swords : kind == "armour" ? Armour : -1;
            int tier = 0;
            if (item < 0) return fail("not example, wings, swords or armour: " + kind);
            if (!(head >> tier) || tier < 1 || tier >= TierCount) return fail("no tier (1 to 3)");
            std::string rest; std::getline(head, rest);
            Tier& t = x.Items[item][tier];
            t.Name = Wide(Trim(rest));
            std::istringstream keys(tail);
            for (std::string pair; keys >> pair;)
            {
                const auto eq = pair.find('=');
                if (eq == std::string::npos) return fail("not key=value: " + pair);
                const std::string key = pair.substr(0, eq), value = pair.substr(eq + 1);
                if (key == "show")
                {
                    std::istringstream parts(value);
                    for (std::string part; std::getline(parts, part, ',');) if (!part.empty()) t.Show.push_back(part);
                }
                else if (key == "lit" || key == "tint")
                {
                    const auto colon = value.find(':');
                    if (colon == std::string::npos) return fail(key + " wants <material>:<value>");
                    PartLook& part = PartOf(gLit[LitIndex(example - 1, item, tier)], value.substr(0, colon));
                    if (key == "lit") part.Glow = static_cast<float>(std::atof(value.c_str() + colon + 1));
                    else if (sscanf_s(value.c_str() + colon + 1, "%f,%f,%f", &part.Tint.x, &part.Tint.y, &part.Tint.z) != 3)
                        return fail("tint wants <material>:<r>,<g>,<b>");
                }
                else if (key == "body") t.Body = value;
                else if (key == "bodynormal") t.BodyNormal = value;
                else if (key == "bodyrough") t.BodyRough = value;
                else if (key == "bodymetal") t.BodyMetal = value;
                else if (key == "bodyglow") t.BodyGlow = static_cast<float>(std::atof(value.c_str()));
                else if (key == "wings") t.Plumage = value;
                else if (key == "glow") t.Glow = static_cast<float>(std::atof(value.c_str()));
                else if (key == "span") t.Span = static_cast<float>(std::atof(value.c_str()));
                else return fail("unknown key " + key);
            }
        }
        for (auto& x : gExamples)
        {
            if (x.Title.empty()) return fail("an example has no line of its own");
            x.Items[Wings][0].Name = L"Her own wings";
            x.Items[Swords][0].Name = L"Her own swords";
            x.Items[Armour][0].Name = L"Her own armour";
            for (int i = 0; i < ItemCount; ++i)
                for (int k = 1; k < TierCount; ++k)
                    if (x.Items[i][k].Name.empty()) return fail("an example misses a tier");
        }
        return true;
    }

    int Count() { return static_cast<int>(gExamples.size()); }

    const Example& Get(int example)
    {
        static const Example none{};
        return example >= 0 && example < Count() ? gExamples[static_cast<size_t>(example)] : none;
    }

    bool Apply(Renderer& renderer, WorldRenderer& worldRenderer, int example, const int tiers[ItemCount], std::string& error)
    {
        if (example < 0 || example >= Count()) { error = "no such example"; return false; }
        const Example& x = gExamples[static_cast<size_t>(example)];
        int t[ItemCount];
        for (int i = 0; i < ItemCount; ++i) t[i] = std::clamp(tiers[i], 0, TierCount - 1);
        // every look's part hidden, then those of her tiers shown; her own swords only while no others are
        for (const auto& name : worldRenderer.CharacterMaterialNames())
            if (name.rfind("Look_", 0) == 0) worldRenderer.SetCharacterMaterialVisible(name, false);
        for (int i = 0; i < ItemCount; ++i)
            for (const auto& part : x.Items[i][t[i]].Show) worldRenderer.SetCharacterMaterialVisible(part, true);
        worldRenderer.SetCharacterMaterialVisible("Sword", x.Items[Swords][t[Swords]].Show.empty());
        // her body and her wings as the tiers have them (from their own textures each time: nothing left of the last)
        worldRenderer.ResetCharacterMaterialLooks();
        bool ok = true;
        const Tier& wings = x.Items[Wings][t[Wings]];
        if (!wings.Plumage.empty() || wings.Glow >= 0)
        {
            WorldRenderer::CharacterMaterialLook look;
            look.BaseColor = wings.Plumage; look.EmissiveStrength = wings.Glow;
            ok = worldRenderer.SetCharacterMaterialLook(renderer, "Wings", gRoot, look, error) && ok;
        }
        const Tier& armour = x.Items[Armour][t[Armour]];
        if (!armour.Body.empty() || !armour.BodyRough.empty() || !armour.BodyMetal.empty() || armour.BodyGlow >= 0)
        {
            WorldRenderer::CharacterMaterialLook look;
            look.BaseColor = armour.Body; look.Normal = armour.BodyNormal; look.Roughness = armour.BodyRough; look.Metallic = armour.BodyMetal;
            look.EmissiveStrength = armour.BodyGlow;
            ok = worldRenderer.SetCharacterMaterialLook(renderer, "Nyxara", gRoot, look, error) && ok;
        }
        for (int i = 0; i < ItemCount; ++i)
            for (const auto& part : gLit[LitIndex(example, i, t[i])])
            {
                WorldRenderer::CharacterMaterialLook look;
                look.EmissiveStrength = part.Glow; look.Tint = part.Tint;
                ok = worldRenderer.SetCharacterMaterialLook(renderer, part.Part, gRoot, look, error) && ok;
            }
        worldRenderer.ReleaseUnusedCharacterLookTextures();   // (a look tried and left: its textures go)
        Doorways::GetNyxaraKit().SetWingScale(wings.Span);
        return ok;
    }
}
