#ifndef NOMINMAX
#define NOMINMAX   // std::min / std::max, not the Windows macros
#endif
#include "EaglesAscent.h"

#include "CombatSystem.h"
#include "DoorwaysAsset/DoorwaysDMeshLoader.h"
#include "DoorwaysAssetPaths.h"
#include "DoorwaysGame.h"
#include "DoorwaysMaterialRuntime.h"
#include "DoorwaysRuntimePathResolver.h"
#include "DoorwaysWalkableSurfaceQuery.h"
#include "EnvironmentSettings.h"
#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

// The rules of a mountain map (EaglesAscent.h). Moved here from Map Lab on 2026-11-17 (Map A Gate 5), unchanged.
namespace CaucasianEagle
{
    using namespace Doorways;
    using namespace DirectX;

    namespace
    {
        constexpr float PropNear = 520, PropFar = 3200, PropBig = 9;   // m: small models are drawn to PropNear, models taller than PropBig to PropFar
    }

    bool EaglesAscent::Load(const std::filesystem::path& path, std::string& error)
    {
        std::ifstream file(path);
        if (!file) { error = "map data missing: " + path.string(); return false; }
        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream in(line); std::string word; in >> word;
            if (word == "summit") in >> Summit;
            else if (word == "length") in >> TrailLength;
            else if (word == "zone") { Zone z; in >> z.From >> z.Name; Zones.push_back(z); }
            else if (word == "station")
            {
                Station s; int check = 1; in >> s.Name >> s.P.x >> s.P.y >> s.P.z >> s.Yaw >> s.Along;
                if (in >> s.Id >> check) s.Checkpoint = check != 0; else s.Id.clear();
                Stations.push_back(s);
            }
            else if (word == "prop")
            {
                std::string model; Prop prop; in >> model >> prop.P.x >> prop.P.y >> prop.P.z >> prop.Yaw >> prop.Height;
                auto found = std::find(mPropNames.begin(), mPropNames.end(), model);
                if (found == mPropNames.end()) { mPropNames.push_back(model); found = mPropNames.end() - 1; }
                prop.Model = static_cast<int>(found - mPropNames.begin()); mProps.push_back(prop);
            }
            else if (word == "flight") { Flight f; in >> f.Id >> f.Kind >> f.From.x >> f.From.y >> f.From.z >> f.To.x >> f.To.y >> f.To.z; Flights.push_back(f); }
            else if (word == "updraft") { Updraft u; in >> u.Id >> u.P.x >> u.P.y >> u.P.z >> u.Radius >> u.Top >> u.Lift; Updrafts.push_back(u); }
            else if (word == "wind") { Wind w; in >> w.Id >> w.P.x >> w.P.y >> w.P.z >> w.Radius >> w.X >> w.Z >> w.Speed; Winds.push_back(w); }
            else if (word == "fly") { Fly f; in >> f.Kind >> f.From >> f.To; FlyStretches.push_back(f); }
            else if (word == "heights")
            {
                in >> mLandX >> mLandZ >> mLandStep >> mLandNx >> mLandNz;
                auto file2 = path; file2.replace_extension(".heights");
                std::ifstream bin(file2, std::ios::binary); mLand.assign(static_cast<size_t>(std::max(mLandNx, 0)) * static_cast<size_t>(std::max(mLandNz, 0)), 0.f);
                if (!bin || mLandNx < 2 || mLandNz < 2 || !bin.read(reinterpret_cast<char*>(mLand.data()), static_cast<std::streamsize>(mLand.size() * sizeof(float)))) mLand.clear();
                int grids = 1; if (!(in >> grids)) grids = 1;
                mFloor.clear();
                if (grids >= 2 && !mLand.empty())
                {
                    mFloor.assign(mLand.size(), 0.f);
                    if (!bin.read(reinterpret_cast<char*>(mFloor.data()), static_cast<std::streamsize>(mFloor.size() * sizeof(float)))) mFloor.clear();
                }
            }
            else if (word == "patch")
            {   // a fine grid in <map>.heights at a float offset (written after the land's grids)
                Patch q; long long offset = 0; in >> q.X >> q.Z >> q.Step >> q.Nx >> q.Nz >> offset;
                auto file2 = path; file2.replace_extension(".heights");
                std::ifstream bin(file2, std::ios::binary);
                if (q.Nx >= 2 && q.Nz >= 2 && q.Step > 0 && bin && bin.seekg(offset * static_cast<long long>(sizeof(float))))
                {
                    q.H.resize(static_cast<size_t>(q.Nx) * static_cast<size_t>(q.Nz));
                    if (bin.read(reinterpret_cast<char*>(q.H.data()), static_cast<std::streamsize>(q.H.size() * sizeof(float)))) mPatches.push_back(std::move(q));
                }
            }
            else if (word == "tops")
            {   // the drawn top round rock and steep land (2026-11-22), in <map>.heights at a float offset: the cells' indices,
                // then each cell's per x per points
                long long cells = 0, offset = 0; int per = 0; in >> cells >> offset >> per;
                auto file2 = path; file2.replace_extension(".heights");
                std::ifstream bin(file2, std::ios::binary);
                if (cells > 0 && per > 0 && mLandNx >= 2 && mLandNz >= 2 && bin && bin.seekg(offset * static_cast<long long>(sizeof(float))))
                {
                    std::vector<float> index(static_cast<size_t>(cells)); mTops.resize(static_cast<size_t>(cells) * per * per);
                    if (bin.read(reinterpret_cast<char*>(index.data()), static_cast<std::streamsize>(index.size() * sizeof(float)))
                        && bin.read(reinterpret_cast<char*>(mTops.data()), static_cast<std::streamsize>(mTops.size() * sizeof(float))))
                    {
                        mTopCell.assign(static_cast<size_t>(mLandNx - 1) * static_cast<size_t>(mLandNz - 1), -1); mTopsPer = per;
                        for (size_t k = 0; k < index.size(); ++k)
                        {
                            const long long c = static_cast<long long>(index[k]);
                            if (c >= 0 && c < static_cast<long long>(mTopCell.size())) mTopCell[static_cast<size_t>(c)] = static_cast<int>(k);
                        }
                    }
                    else { mTops.clear(); mTopCell.clear(); }
                }
            }
            else if (word == "outer")
            {   // the rest of the mountain's heights (2026-11-18), in <map>.heights at a float offset: Land() off the near grid
                Patch q; long long offset = 0; in >> q.X >> q.Z >> q.Step >> q.Nx >> q.Nz >> offset;
                auto file2 = path; file2.replace_extension(".heights");
                std::ifstream bin(file2, std::ios::binary);
                if (q.Nx >= 2 && q.Nz >= 2 && q.Step > 0 && bin && bin.seekg(offset * static_cast<long long>(sizeof(float))))
                {
                    q.H.resize(static_cast<size_t>(q.Nx) * static_cast<size_t>(q.Nz));
                    if (bin.read(reinterpret_cast<char*>(q.H.data()), static_cast<std::streamsize>(q.H.size() * sizeof(float)))) mOuter = std::move(q);
                }
            }
            else if (word == "moment") { Moment m; in >> m.Name >> m.P.x >> m.P.y >> m.P.z; std::replace(m.Name.begin(), m.Name.end(), '_', ' '); Moments.push_back(m); }
            else if (word == "trail") { XMFLOAT3 p; in >> p.x >> p.y >> p.z; Trail.push_back(p); }
            else if (word == "tread") { Tread r; if (in >> r.X >> r.Z >> r.Ax >> r.Az >> r.Along >> r.Across >> r.Top) mTreads.push_back(r); }
            else if (word == "ring")
            {
                int n = 0; std::vector<XMFLOAT3> r;
                if (in >> n) for (int i = 0; i < n; ++i) { XMFLOAT3 q; if (!(in >> q.x >> q.y >> q.z)) break; r.push_back(q); }
                if (r.size() >= 2) mRings.push_back(std::move(r));
            }
            else if (word == "cave")
            {
                std::string kind; int n = 0; Cave c;
                if (in >> kind >> n && n >= 2)
                {
                    c.Kind = kind;
                    c.MinX = c.MinZ = c.MinY = 1e30f; c.MaxX = c.MaxZ = c.MaxY = -1e30f;
                    for (int i = 0; i < n; ++i)
                    {
                        CavePoint q; if (!(in >> q.X >> q.Y >> q.Z >> q.Half >> q.Tall)) break;
                        c.MinX = std::min(c.MinX, q.X - q.Half - 2); c.MaxX = std::max(c.MaxX, q.X + q.Half + 2);
                        c.MinZ = std::min(c.MinZ, q.Z - q.Half - 2); c.MaxZ = std::max(c.MaxZ, q.Z + q.Half + 2);
                        c.MinY = std::min(c.MinY, q.Y - 2); c.MaxY = std::max(c.MaxY, q.Y + q.Tall + 2);
                        c.Line.push_back(q);
                    }
                    if (c.Line.size() >= 2) mCaves.push_back(std::move(c));
                }
            }
            else if (word == "terrain")
            {
                mHasTerrain = static_cast<bool>(in >> mTerrain[0] >> mTerrain[1] >> mTerrain[2] >> mTerrain[3] >> mTerrain[4] >> mTerrain[5]);
                if (!(in >> mTerrain[6])) mTerrain[6] = 0;     // the height the forest stands to (2026-11-18); absent in older files: no forest
            }
            else if (word == "ranges")
            {
                if (!(in >> mRanges[0] >> mRanges[1] >> mRanges[2])) mRanges[0] = mRanges[1] = mRanges[2] = 0;   // absent in older files: none
            }
        }
        mDataPath = path;
        if (Stations.empty()) { error = "map data has no station: " + path.string(); return false; }
        mTreadCells.clear();
        for (int i = 0; i < static_cast<int>(mTreads.size()); ++i)
        {   // each tread in every 4 m cell its corners reach
            const auto& r = mTreads[static_cast<size_t>(i)]; const float reach = .5f * std::hypot(r.Along, r.Across);
            for (int cx = static_cast<int>(std::floor((r.X - reach) / 4)); cx <= static_cast<int>(std::floor((r.X + reach) / 4)); ++cx)
                for (int cz = static_cast<int>(std::floor((r.Z - reach) / 4)); cz <= static_cast<int>(std::floor((r.Z + reach) / 4)); ++cz)
                    mTreadCells[(static_cast<long long>(cx) << 32) ^ static_cast<unsigned>(cz)].push_back(i);
        }
        if (Summit <= 0) for (const auto& s : Stations) Summit = std::max(Summit, s.P.y);
        ModelCount = static_cast<int>(mProps.size());
        CaveEnds();
        { auto trees = path; trees.replace_extension(".trees"); LoadTrees(trees); }   // (absent: no trees hold her)
        return true;
    }

    void EaglesAscent::CaveEnds()
    {
        // An end is open where it meets another cave (a junction: within that one's width), or where the land a few
        // metres out beyond it is lower than its roof (a mouth: the open air). Else it ends in the rock. In doubt open:
        // an end taken for rock would shut her in.
        for (size_t c = 0; c < mCaves.size(); ++c)
        {
            auto& cave = mCaves[c]; const auto& L = cave.Line;
            if (L.size() < 2) continue;
            for (int e = 0; e < 2; ++e)
            {
                const CavePoint& end = e == 0 ? L.front() : L.back();
                const CavePoint& inner = e == 0 ? L[1] : L[L.size() - 2];
                bool open = false;
                for (size_t o = 0; o < mCaves.size() && !open; ++o)
                {
                    if (o == c) continue;
                    for (const auto& q : mCaves[o].Line)
                        if (std::abs(q.Y - end.Y) < 4 && std::hypot(q.X - end.X, q.Z - end.Z) < q.Half + end.Half + 1.5f) { open = true; break; }
                }
                float ox = end.X - inner.X, oz = end.Z - inner.Z; const float ol = std::max(std::hypot(ox, oz), .01f); ox /= ol; oz /= ol;
                for (const float out : {2.f, 4.f, 7.f, 10.f})
                    if (!open && Land(end.X + ox * out, end.Z + oz * out) < end.Y + end.Tall * .9f) open = true;
                cave.Open[e] = open;
            }
        }
    }

    void EaglesAscent::LoadTrees(const std::filesystem::path& path)
    {
        mTrunks.clear(); mTrunkCells.clear();
        {   // CAUCASIAN_EAGLE_TREES=0: the trunks are read, but none holds her (the nature test's baseline)
            char value[8] = {}; size_t got = 0;
            TreesHold = !(getenv_s(&got, value, sizeof(value), "CAUCASIAN_EAGLE_TREES") == 0 && got > 1 && value[0] == '0');
        }
        std::ifstream file(path, std::ios::binary);
        if (!file) return;
        const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        const char* at = text.c_str();
        while (*at)
        {
            if (*at == '#') { while (*at && *at != '\n') ++at; if (*at) ++at; continue; }
            char* end = nullptr; Trunk t;
            t.X = std::strtof(at, &end); if (end == at) { ++at; continue; } at = end;
            t.Y = std::strtof(at, &end); at = end; t.Z = std::strtof(at, &end); at = end;
            t.R = std::strtof(at, &end); at = end; t.H = std::strtof(at, &end); at = end;
            while (*at && *at != '\n') ++at;
            if (*at) ++at;
            if (t.R <= 0) continue;
            const unsigned index = static_cast<unsigned>(mTrunks.size());
            mTrunks.push_back(t);
            mTrunkCells[TrunkKey(static_cast<int>(std::floor(t.X / TrunkCell)), static_cast<int>(std::floor(t.Z / TrunkCell)))].push_back(index);
        }
    }

    bool EaglesAscent::NearestTrunk(float x, float y, float z, float reach, Trunk& out) const
    {
        const int ex = static_cast<int>(std::floor(x / TrunkCell)), ez = static_cast<int>(std::floor(z / TrunkCell));
        float best = reach;
        bool found = false;
        for (int cz = ez - 1; cz <= ez + 1; ++cz)
            for (int cx = ex - 1; cx <= ex + 1; ++cx)
            {
                const auto cell = mTrunkCells.find(TrunkKey(cx, cz));
                if (cell == mTrunkCells.end()) continue;
                for (const unsigned i : cell->second)
                {
                    const Trunk& t = mTrunks[i];
                    if (y < t.Y - 1.5f || y > TrunkTop(t)) continue;
                    const float d = std::hypot(x - t.X, z - t.Z) - t.R;
                    if (d < best) { best = d; out = t; found = true; }
                }
            }
        return found;
    }

    void EaglesAscent::HoldOffTrees(Player& p, bool flying) const
    {
        if (mTrunks.empty() || !TreesHold) return;
        const float body = flying ? .6f : .42f;   // her half width; on wings their spread
        const int ex = static_cast<int>(std::floor(p.Position.x / TrunkCell)), ez = static_cast<int>(std::floor(p.Position.z / TrunkCell));
        for (int pass = 0; pass < 2; ++pass)      // (between two trunks: once more)
            for (int cz = ez - 1; cz <= ez + 1; ++cz)
                for (int cx = ex - 1; cx <= ex + 1; ++cx)
                {
                    const auto cell = mTrunkCells.find(TrunkKey(cx, cz));
                    if (cell == mTrunkCells.end()) continue;
                    for (const unsigned i : cell->second)
                    {
                        const Trunk& t = mTrunks[i];
                        if (p.Position.y < t.Y - 1.5f || p.Position.y > TrunkTop(t)) continue;
                        const float keep = t.R + body, dx = p.Position.x - t.X, dz = p.Position.z - t.Z, d2 = dx * dx + dz * dz;
                        if (d2 >= keep * keep) continue;
                        const float d = std::sqrt(d2);
                        if (d < 1e-3f) { p.Position.x = t.X + keep; continue; }
                        p.Position.x = t.X + dx / d * keep; p.Position.z = t.Z + dz / d * keep;
                    }
                }
    }

    void EaglesAscent::HoldInCave(Player& p) const
    {
        constexpr float Body = .75f;   // her body's half width and the wall's rough rock (up to 0.35 m into the tube)
        bool inside = false, atWall = false;
        float pushX = 0, pushZ = 0, worst = 0, roof = 1e30f;
        for (const auto& c : mCaves)
        {
            if (p.Position.x < c.MinX - 4 || p.Position.x > c.MaxX + 4 || p.Position.z < c.MinZ - 4 || p.Position.z > c.MaxZ + 4 ||
                p.Position.y < c.MinY - 3 || p.Position.y > c.MaxY + 8) continue;
            const size_t n = c.Line.size();
            float best = 1e30f, bx = 0, bz = 0, bHalf = 0, bRoof = 0; bool open = false;
            for (size_t i = 0; i + 1 < n; ++i)
            {
                const auto& a = c.Line[i]; const auto& b = c.Line[i + 1];
                const float ex = b.X - a.X, ez = b.Z - a.Z, l2 = ex * ex + ez * ez;
                if (l2 < 1e-6f) continue;
                const float raw = ((p.Position.x - a.X) * ex + (p.Position.z - a.Z) * ez) / l2, t = std::clamp(raw, 0.f, 1.f);
                const float floor = a.Y + (b.Y - a.Y) * t, tall = a.Tall + (b.Tall - a.Tall) * t, half = a.Half + (b.Half - a.Half) * t;
                if (p.Position.y < floor - 1.5f || p.Position.y > floor + tall + .5f) continue;
                const float cx = a.X + ex * t, cz = a.Z + ez * t, d = std::hypot(p.Position.x - cx, p.Position.z - cz);
                if (d < best)
                {
                    best = d; bx = cx; bz = cz; bHalf = half; bRoof = floor + tall;
                    open = (i == 0 && raw < 0 && c.Open[0]) || (i + 2 == n && raw > 1 && c.Open[1]);   // out through a mouth or into another cave
                }
            }
            if (best > 1e29f) continue;
            if (open) { if (best <= bHalf + .6f) inside = true; continue; }   // (past an open end: her own way out)
            const float room = std::max(bHalf - Body, .3f);
            if (best <= room) { inside = true; roof = std::min(roof, bRoof); continue; }
            if (best <= bHalf + .9f && best - room > worst)
            {   // at a wall, or just through it: back to the wall's inside
                atWall = true; worst = best - room;
                const float k = room / std::max(best, 1e-3f);
                pushX = bx + (p.Position.x - bx) * k; pushZ = bz + (p.Position.z - bz) * k;
                roof = std::min(roof, bRoof);
            }
        }
        if (!inside && atWall) { p.Position.x = pushX; p.Position.z = pushZ; }
        if ((inside || atWall) && roof < 1e29f && p.VerticalVelocity > 0 && p.Position.y + 1.9f > roof - .2f)
            p.VerticalVelocity = 0;   // her head at the roof (a jump in a low cave): she rises no more, and falls back
    }

    // nest <station> <x> <y> <z> <yaw> <along>
    // boss <id> <kind> <x> <y> <z> <yaw> <along> <lo> <hi>
    // group <id> <fixed|patrol> <x> <y> <z> <yaw> <along> <lo> <hi> <roam> <count> <kind>...
    // cave <id> <fixed|patrol> <x> <y> <z> <yaw> <cave> <at> <lo> <hi> <roam> <count> <kind>...
    bool EaglesAscent::LoadCreatures(const std::filesystem::path& path, std::string& error)
    {
        Nests.clear(); Placements.clear(); Levels.clear();
        std::ifstream in(path);
        if (!in) { error = "Cannot read " + path.string(); return false; }
        std::string line;
        int number = 0;
        while (std::getline(in, line))
        {
            ++number;
            std::istringstream s(line);
            std::string word;
            if (!(s >> word) || word[0] == '#') continue;
            bool good = true;
            if (word == "nest")
            {
                Nest n;
                good = static_cast<bool>(s >> n.Station >> n.P.x >> n.P.y >> n.P.z >> n.Yaw >> n.Along) && n.Station >= 0 && n.Station < static_cast<int>(Stations.size());
                if (good) Nests.push_back(n);
            }
            else if (word == "level")
            {   // level <n> <price> <way jars> <side jars>
                int n = 0; Level v;
                good = static_cast<bool>(s >> n >> v.Price >> v.WayJars >> v.SideJars) && n >= 1 && n <= 99 && v.Price > 0 && v.WayJars >= 0 && v.SideJars >= 0;
                if (good)
                {
                    if (Levels.size() < static_cast<size_t>(n)) Levels.resize(static_cast<size_t>(n));
                    Levels[static_cast<size_t>(n - 1)] = v;
                }
            }
            else if (word == "around")
            {   // around <id> <fixed|patrol> <along> <side> <count> <kind>...: its place is found once the ground is up
                Placed p; p.Around = true;
                std::string keep, kind; int count = 0;
                good = static_cast<bool>(s >> p.Id >> keep >> p.At >> p.Side >> count) && count > 0 && count <= 3 && Trail.size() >= 3;
                for (int k = 0; good && k < count; ++k)
                {
                    good = static_cast<bool>(s >> kind);
                    if (good) p.Kinds.push_back(kind);
                }
                if (good)
                {   // until then: at the way's edge on its side, looking back down the way
                    p.Patrol = keep == "patrol";
                    const int i = std::clamp(static_cast<int>(std::lround(p.At / TrailStep)), 1, static_cast<int>(Trail.size()) - 1);
                    const XMFLOAT3 a = Trail[static_cast<size_t>(i - 1)], b = Trail[static_cast<size_t>(i)];
                    const float run = (std::max)(1e-3f, std::hypot(b.x - a.x, b.z - a.z)), rx = (b.z - a.z) / run, rz = -(b.x - a.x) / run;
                    const float side = p.Side >= 0 ? 2.5f : -2.5f;
                    p.P = {b.x + rx * side, b.y, b.z + rz * side};
                    p.Yaw = std::atan2(a.x - b.x, a.z - b.z);
                    p.Lo = (std::max)(0.f, p.At - 30.f); p.Hi = p.At + 30.f;
                    Placements.push_back(p);
                }
            }
            else if (word == "boss" || word == "group" || word == "cave")
            {
                Placed p; p.Boss = word == "boss";
                std::string keep, kind;
                if (p.Boss)
                {
                    good = static_cast<bool>(s >> p.Id >> kind >> p.P.x >> p.P.y >> p.P.z >> p.Yaw >> p.At >> p.Lo >> p.Hi);
                    p.Kinds = {kind};
                }
                else
                {
                    int count = 0;
                    good = static_cast<bool>(s >> p.Id >> keep >> p.P.x >> p.P.y >> p.P.z >> p.Yaw);
                    if (good && word == "cave") good = static_cast<bool>(s >> p.Cave) && p.Cave >= 0 && p.Cave < static_cast<int>(mCaves.size());
                    good = good && static_cast<bool>(s >> p.At >> p.Lo >> p.Hi >> p.Roam >> count) && count > 0 && count <= 3;
                    for (int k = 0; good && k < count; ++k)
                    {
                        good = static_cast<bool>(s >> kind);
                        if (good) p.Kinds.push_back(kind);
                    }
                    p.Patrol = keep == "patrol";
                }
                if (good) Placements.push_back(p);
            }
            if (!good) { error = path.filename().string() + " line " + std::to_string(number) + ": cannot read \"" + line.substr(0, 60) + "\""; return false; }
        }
        for (size_t i = 0; i < Levels.size(); ++i)
            if (Levels[i].Price <= 0) { error = path.filename().string() + ": no \"level " + std::to_string(i + 1) + "\" line"; return false; }
        // the nests are the only checkpoints (the owner, 2026-11-19: "special saved points only"); she begins on the shore
        // (EaglesAscentGame sets it), but going back down to it does not make it the place she wakes at
        if (!Nests.empty())
        {
            for (auto& station : Stations) station.Checkpoint = false;
            for (const auto& n : Nests)
            {
                auto& station = Stations[static_cast<size_t>(n.Station)];
                station.Checkpoint = true; station.HasNest = true; station.Nest = n.P;
            }
        }
        // each nest is a game model as made (art/map/eagle_nest/build_eagle_nest.py), when it is there: LoadModels loads it
        NestModels = false;
        if (!Nests.empty() && std::filesystem::exists(ResolveDoorwaysRuntimePath("assets/Compiled") / (std::string(NestModel) + ".dmesh")))
        {
            auto found = std::find(mPropNames.begin(), mPropNames.end(), NestModel);
            if (found == mPropNames.end()) { mPropNames.push_back(NestModel); found = mPropNames.end() - 1; }
            for (const auto& n : Nests)
            {
                Prop prop; prop.Model = static_cast<int>(found - mPropNames.begin()); prop.P = n.P; prop.Yaw = n.Yaw; prop.Height = 0;   // its own size
                mProps.push_back(prop);
            }
            ModelCount = static_cast<int>(mProps.size());
            NestModels = true;
        }
        return true;
    }

    int EaglesAscent::StretchAlong(float along) const
    {
        int k = 1;
        for (const auto& n : Nests) if (n.Along <= along) ++k;
        return k;
    }

    int EaglesAscent::StretchHeight(float y) const
    {
        int k = 1;
        for (const auto& n : Nests) if (n.P.y <= y) ++k;
        return k;
    }

    // A group around the way: a place near where the placing model put it (up to AroundReach up or down the way, inside
    // its stretch, clear of the nests and the bosses), out from the way's middle on its side first, where the ground is
    // walkable and level enough for its group to stand round it, she can walk out to it from the way's edge (no step of a
    // metre over 1.5 m on the way there), no trunk stands within 2.5 m, it is not in a cave, no other group at all (on the
    // way, in a cave, a boss) nor a cave's mouth with creatures in it is within OtherClear of it, and where its line leaves
    // the way no group further up the way or in a cave is within OtherClear, nor any group within WayMargin of as near as
    // it is, and it is CaveRoom from a cave's group that it and it together are more than three. The way climbs cliffs and crosses gorges: beside it there is often no ground at all, so first every place
    // each group could have is found; then, up the way, each takes the one nearest its plan that is AroundGap from the
    // group before it (or at least AroundGap[0] less 10 m), no other group around the way within 25 m, and that leaves the
    // group after it a place of its own. Its At becomes that place's m along the way.
    void EaglesAscent::PlaceAround(const DoorwaysRuntimeScene& scene, std::vector<std::string>* report)
    {
        constexpr float Edge = 4.5f;                                       // m from the way's middle to just past its edge
        constexpr float Out[] = {14.f, 18.f, 11.f, 22.f, 9.f, 26.f, 30.f}; // m from the way's middle
        constexpr float AroundReach = 120.f, AroundGap[2] = {150.f, 400.f}, GapLeast = AroundGap[0] - 10.f, NestClear = 60.f, BossClear = 100.f;
        // A group around the way is a fight of its own: no other group's place, nor the mouth of a cave with creatures in
        // it, within OtherClear of its place, and from where its line leaves the way it is nearer than any other group by
        // WayMargin, so it is the one that wakes as she comes by; nor, within OtherClear of there, a group further up the
        // way or in a cave, that she has not met yet (those below it she has passed: coming up the way she meets them
        // first). OtherClear: more than the tallest of the creatures sees (MountainSight and a Devi's 5.2 m), measured
        // across the ground as they see. Nearer, the two are one fight, and whichever is a little nearer her wakes first
        // and takes the three places among the living (2026-11-21, the creatures' tour: around-0698 stood 14 m along
        // from way-0700 and never woke, and way-0700 came at her off the way; way-5800's Devi saw her 22 m off, where
        // around-5842's line leaves the way below it). (40 m from every other group left half of them no ground at all,
        // 30 m three; 26 m from every group where the line leaves the way, two.)
        constexpr float OtherClear = 26.f, WayMargin = 8.f;
        // And from a cave's group, when the two are more than the three alive at once (Journey::MountainMostAlive):
        // CaveRoom, so that wherever she fights the one, the other is farther from her than MountainGiveWay (70 m) and,
        // calm, gives way to it (70 m, more than the tallest creature sees and its group's spread round its place). A cave's
        // group stands deep off the way: from the way it is farther from her than a group around the way just up it,
        // which woke first, held the three places as she went into the cave, and the cave's never woke (2026-11-21, the
        // creatures' tour: around-4374, three, 26 m up the way from where cave-14, two, is visited).
        constexpr float CaveRoom = 100.f;
        constexpr size_t MostAlive = 3;
        const auto ground = [&](float x, float z, float lo, float hi, float flat, float& y)
        {
            const auto hit = QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(scene, x, z, lo, hi);
            if (!hit.Hit || hit.Normal.y < flat) return false;
            y = hit.GroundY;
            return true;
        };
        const float length = Trail.empty() ? 0.f : static_cast<float>(Trail.size() - 1) * TrailStep;
        std::vector<float> bosses;
        for (const auto& q : Placements) if (q.Boss && q.Cave < 0) bosses.push_back(q.At);
        struct Other { XMFLOAT3 P; float Along; size_t Count; bool InCave; };   // (a group in a cave: Along further up than any)
        std::vector<Other> groups;                   // (the other groups' places)
        std::vector<XMFLOAT3> mouths;                // (the mouths of the caves with creatures)
        std::vector<bool> peopled(mCaves.size(), false);
        for (const auto& q : Placements)
        {
            if (q.Around) continue;
            groups.push_back({q.P, q.Cave >= 0 ? 1e9f : q.At, q.Kinds.size(), q.Cave >= 0});
            if (q.Cave >= 0 && q.Cave < static_cast<int>(mCaves.size())) peopled[static_cast<size_t>(q.Cave)] = true;
        }
        for (size_t c = 0; c < mCaves.size(); ++c)
        {
            const auto& line = mCaves[c].Line;
            if (!peopled[c] || line.empty()) continue;
            if (mCaves[c].Open[0]) mouths.push_back({line.front().X, line.front().Y, line.front().Z});
            if (mCaves[c].Open[1]) mouths.push_back({line.back().X, line.back().Y, line.back().Z});
        }
        const auto apart = [](const XMFLOAT3& u, const XMFLOAT3& v) { return std::sqrt((u.x - v.x) * (u.x - v.x) + (u.y - v.y) * (u.y - v.y) + (u.z - v.z) * (u.z - v.z)); };
        const auto flat = [](const XMFLOAT3& u, const XMFLOAT3& v) { return std::hypot(u.x - v.x, u.z - v.z); };   // (as they see)
        enum { Wings, NoGround, NotLevel, NoWalk, Tree, Cave, Near, CaveGroup, Gap, Crowded, Reasons };
        struct Spot { float At = 0, Out = 0, Dy = 0, SpurAt = 0, Yaw = 0; int Side = 1; XMFLOAT3 P{}; std::vector<XMFLOAT3> Spur; };
        struct Mine { Placed* Group = nullptr; float Hint = 0; std::vector<Spot> Spots; int Why[Reasons] = {}; };
        std::vector<Mine> mine;
        for (auto& p : Placements)
            if (p.Around) { p.Spur.clear(); p.SpurAt = 0; mine.push_back({&p, p.At}); }
        std::stable_sort(mine.begin(), mine.end(), [](const Mine& u, const Mine& v) { return u.Hint < v.Hint; });

        // ---- every place each could have, nearest its plan first (at each m along the way the first side and distance
        // out that will do)
        for (auto& m : mine)
        {
            const Placed& p = *m.Group;
            int* why = m.Why;
            const float hint = m.Hint;
            const int k = StretchAlong(hint);
            const float a0 = (k >= 2 && k - 2 < static_cast<int>(Nests.size()) ? Nests[static_cast<size_t>(k - 2)].Along : 0.f) + NestClear;
            const float a1 = (k - 1 < static_cast<int>(Nests.size()) ? Nests[static_cast<size_t>(k - 1)].Along : length) - NestClear;
            const auto open = [&](float s)
            {
                if (s < (std::max)(a0, 40.f) || s > (std::min)(a1, length - 8.f)) return false;
                for (const auto& f : FlyStretches) if (s >= f.From - 25 && s <= f.To + 25) { ++why[Wings]; return false; }
                for (const float b : bosses) if (std::abs(s - b) < BossClear) return false;
                return true;
            };
            std::vector<float> tries;
            for (float s = hint; s >= hint - AroundReach; s -= TrailStep) tries.push_back(s);
            for (float s = hint + TrailStep; s <= hint + AroundReach; s += TrailStep) tries.push_back(s);
            std::stable_sort(tries.begin(), tries.end(), [&](float u, float v) { return std::abs(u - hint) < std::abs(v - hint); });
            const int first = p.Side >= 0 ? 1 : -1;
            for (const float at : tries)
            {
                if (!open(at)) continue;
                const int i = std::clamp(static_cast<int>(std::lround(at / TrailStep)), 1, static_cast<int>(Trail.size()) - 2);
                const XMFLOAT3 r = Trail[static_cast<size_t>(i)], a = Trail[static_cast<size_t>(i - 1)], b = Trail[static_cast<size_t>(i + 1)];
                const float run = std::hypot(b.x - a.x, b.z - a.z);
                if (run < 1e-3f) continue;
                const float rx = (b.z - a.z) / run, rz = -(b.x - a.x) / run;   // the way's right, going up it
                bool found = false;
                for (const int sgn : {first, -first})
                {
                    if (found) break;
                    const float sf = static_cast<float>(sgn);
                    for (const float out : Out)
                    {
                        const float x = r.x + rx * out * sf, z = r.z + rz * out * sf;
                        float y = 0;
                        if (!ground(x, z, r.y - 8.f, r.y + 8.f, .8f, y)) { ++why[NoGround]; continue; }
                        bool level = true;
                        for (int n = 0; n < 8 && level; ++n)
                        {
                            const float angle = XM_PIDIV4 * static_cast<float>(n);
                            float yy = 0;
                            level = ground(x + 3.f * std::cos(angle), z + 3.f * std::sin(angle), y - 1.4f, y + 1.4f, .72f, yy);
                        }
                        if (!level) { ++why[NotLevel]; continue; }
                        std::vector<XMFLOAT3> walk{r};
                        float last = r.y;
                        bool walkable = true;
                        for (float d = Edge; d < out - .75f && walkable; d += 1.5f)
                        {
                            const float wx = r.x + rx * d * sf, wz = r.z + rz * d * sf;
                            float yy = 0;
                            walkable = ground(wx, wz, last - 1.8f, last + 1.8f, .68f, yy) && std::abs(yy - last) < (d == Edge ? 1.6f : 1.f);
                            if (walkable) { walk.push_back({wx, yy, wz}); last = yy; }
                        }
                        if (!walkable || std::abs(y - last) > 1.3f) { ++why[NoWalk]; continue; }
                        Trunk trunk;
                        if (NearestTrunk(x, y, z, 2.5f, trunk)) { ++why[Tree]; continue; }
                        if (InCave({x, y + 1.f, z})) { ++why[Cave]; continue; }
                        bool closeBy = false;
                        const XMFLOAT3 here{x, y, z};
                        const float fromWay = apart(r, here);
                        for (const auto& q : groups)
                            if (flat(q.P, here) < OtherClear || apart(q.P, r) < fromWay + WayMargin || (q.Along > at && flat(q.P, r) < OtherClear)) closeBy = true;
                        for (const auto& q : mouths) if (flat(q, here) < OtherClear) closeBy = true;
                        if (closeBy) { ++why[Near]; continue; }
                        for (const auto& q : groups)
                            if (q.InCave && p.Kinds.size() + q.Count > MostAlive && apart(q.P, here) < CaveRoom) closeBy = true;
                        if (closeBy) { ++why[CaveGroup]; continue; }
                        Spot spot;
                        walk.push_back(here);
                        for (size_t n = 1; n < walk.size(); ++n) spot.SpurAt += apart(walk[n - 1], walk[n]);
                        for (float d = 2.f; d <= 6.f; d += 2.f)
                        {   // and on past it a little: its group stands round its place, not at the end of its line
                            const float px = x + rx * d * sf, pz = z + rz * d * sf;
                            float yy = y;
                            ground(px, pz, y - 1.5f, y + 1.5f, .6f, yy);
                            walk.push_back({px, yy, pz});
                        }
                        spot.At = at; spot.Out = out; spot.Dy = y - r.y; spot.Side = sgn; spot.P = here;
                        spot.Yaw = std::atan2(r.x - x, r.z - z);   // looking at the way
                        spot.Spur = std::move(walk);
                        m.Spots.push_back(std::move(spot));
                        found = true;
                        break;
                    }
                }
            }
        }

        // ---- up the way: each the place nearest its plan that keeps its distance from the one before it, and (when it
        // can) leaves the next one a place of its own
        std::vector<XMFLOAT3> given;
        float prev = -1e9f;   // m along the way of the last group placed
        for (size_t g = 0; g < mine.size(); ++g)
        {
            auto& m = mine[g];
            const auto crowded = [&](const Spot& s)
            {
                for (const auto& q : given) if (std::hypot(q.x - s.P.x, q.z - s.P.z) < 25.f && std::abs(q.y - s.P.y) < 8.f) return true;
                return false;
            };
            const auto leaves = [&](const Spot& s)
            {   // a place for the next group, far enough on from this one
                if (g + 1 >= mine.size()) return true;
                for (const auto& t : mine[g + 1].Spots) if (t.At - s.At >= GapLeast) return true;
                return mine[g + 1].Spots.empty();   // (it has none anyway)
            };
            std::vector<const Spot*> order;   // within AroundGap of the one before first, then any not nearer it than GapLeast
            for (int pass = 0; pass < 2; ++pass)
                for (const auto& s : m.Spots)
                {
                    const float gap = s.At - prev;
                    const bool fits = pass == 0 ? gap >= AroundGap[0] && gap <= AroundGap[1] : gap >= GapLeast;
                    if (fits && std::find(order.begin(), order.end(), &s) == order.end()) order.push_back(&s);
                }
            for (const auto& s : m.Spots) if (s.At - prev < GapLeast) ++m.Why[Gap];
            const Spot* chosen = nullptr;
            for (const Spot* s : order) if (!crowded(*s) && leaves(*s)) { chosen = s; break; }
            if (!chosen) for (const Spot* s : order) if (!crowded(*s)) { chosen = s; break; }
            for (const Spot* s : order) if (crowded(*s)) ++m.Why[Crowded];
            Placed& p = *m.Group;
            if (!chosen)
            {
                if (report)
                {
                    char line[280];
                    std::snprintf(line, sizeof line, "around\t%s\tNOT_PLACED\tplaces=%d\twings=%d\tno_ground=%d\tnot_level=%d\tno_walk=%d\ttree=%d\tcave=%d\tnear_group=%d\tnear_a_cave_group=%d\ttoo_near_the_last=%d\tcrowded=%d",
                                  p.Id.c_str(), static_cast<int>(m.Spots.size()), m.Why[Wings], m.Why[NoGround], m.Why[NotLevel], m.Why[NoWalk], m.Why[Tree], m.Why[Cave],
                                  m.Why[Near], m.Why[CaveGroup], m.Why[Gap], m.Why[Crowded]);
                    report->push_back(line);
                }
                continue;
            }
            p.Spur = chosen->Spur; p.SpurAt = chosen->SpurAt; p.P = chosen->P; p.Yaw = chosen->Yaw; p.Side = chosen->Side; p.At = chosen->At;
            given.push_back(p.P);
            if (report)
            {
                float nearest = 1e9f;
                for (const auto& q : groups) nearest = (std::min)(nearest, flat(q.P, p.P));
                for (const auto& q : mouths) nearest = (std::min)(nearest, flat(q, p.P));
                char line[240];
                std::snprintf(line, sizeof line, "around\t%s\tplaced\tat=%.0f\tmoved=%.0f\tgap=%.0f\tside=%d\tout=%.0f\tdy=%.1f\tnearest_group=%.0f\tplaces=%d", p.Id.c_str(),
                              p.At, p.At - m.Hint, prev > -1e8f ? p.At - prev : 0.f, p.Side, chosen->Out, chosen->Dy, nearest, static_cast<int>(m.Spots.size()));
                report->push_back(line);
            }
            prev = p.At;
        }
    }

    bool EaglesAscent::LoadModels(Renderer& renderer, std::string& error)
    {
        const auto root = ResolveDoorwaysRuntimePath("assets/Compiled");
        mPropModels.resize(mPropNames.size());
        for (size_t i = 0; i < mPropNames.size(); ++i)
        {
            auto& out = mPropModels[i]; const std::string& base = mPropNames[i];
            DoorwaysLoadedDMesh mesh;
            if (!LoadDoorwaysDMesh(root / (base + ".dmesh"), mesh, &error)) return false;
            if (mesh.Vertices.empty()) { error = base + ".dmesh has no vertices"; return false; }
            out.Min = {1e9f, 1e9f, 1e9f}; out.Max = {-1e9f, -1e9f, -1e9f};
            for (const auto& v : mesh.Vertices)
            {
                out.Min.x = std::min(out.Min.x, v.Position.X); out.Max.x = std::max(out.Max.x, v.Position.X);
                out.Min.y = std::min(out.Min.y, v.Position.Y); out.Max.y = std::max(out.Max.y, v.Position.Y);
                out.Min.z = std::min(out.Min.z, v.Position.Z); out.Max.z = std::max(out.Max.z, v.Position.Z);
            }
            if (!renderer.CreateDoorwaysGpuMeshFromLoadedDMesh(mesh, out.Mesh, &error)) return false;
            // a city's models share one picture: Cities/BatumiGate -> Cities/BatumiBaseColor.png
            std::string picture = base;
            if (base.rfind("Cities/", 0) == 0)
                for (const char* city : {"Batumi", "Gori", "Kutaisi", "Mestia", "Mtskheta", "Tbilisi", "Telavi"})
                    if (base.rfind(std::string("Cities/") + city, 0) == 0) picture = std::string("Cities/") + city;
            DoorwaysTextureLoadDesc desc; desc.Path = root / (picture + "BaseColor.png");
            desc.Semantic = DoorwaysMaterialTextureSemantic::BaseColor; desc.GenerateMips = true;
            DoorwaysTextureLoadResult result;
            if (!renderer.LoadDoorwaysTexture(desc, result, &error)) return false;
            out.Texture = result.SRV;
        }
        // The ground (2026-11-18): the shared CC0 layers (Compiled/Textures/Terrain) and this map's splat. Without them
        // the ML_Terrain* materials draw their own pictures, as before: nothing here is required.
        renderer.UnloadTerrain();
        const auto layers = root / "Textures" / "Terrain";
        if (mHasTerrain && std::filesystem::exists(layers / "albedo.dds") && mTerrain[2] > 0 && mTerrain[3] > 0)
        {
            auto a = mDataPath; a.replace_extension(".splat_a.dds");
            auto b = mDataPath; b.replace_extension(".splat_b.dds");
            const bool splat = std::filesystem::exists(a) && std::filesystem::exists(b);
            std::string terrainError;
            if (renderer.LoadTerrain(layers, splat ? a : std::filesystem::path(), splat ? b : std::filesystem::path(), terrainError))
            {
                float debug = 0;   // CAUCASIAN_EAGLE_TERRAIN_DEBUG=1: the ground's weights as colours (review shots)
                char value[8] = {}; size_t got = 0;
                if (getenv_s(&got, value, sizeof(value), "CAUCASIAN_EAGLE_TERRAIN_DEBUG") == 0 && got > 1) debug = static_cast<float>(std::atof(value));
                renderer.SetTerrainParams({mTerrain[0], mTerrain[1], 1.f / mTerrain[2], 1.f / mTerrain[3]},
                                          {mTerrain[4], 150.f, mTerrain[5], mTerrain[6]}, {7.f, 41.f, 3.5f, 19.f}, {1.f, 1.f, debug, 0.f},
                                          {mRanges[0], mRanges[1], mRanges[2], mRanges[2] > 0 ? FarPlane * .943f : 0.f});
            }
        }
        return true;
    }

    float EaglesAscent::OuterLand(float x, float z, bool lowest) const
    {
        const auto& q = mOuter; if (q.H.empty()) return -1e9f;
        const float fx = (x - q.X) / q.Step, fz = (z - q.Z) / q.Step;
        if (fx < 0 || fz < 0 || fx > static_cast<float>(q.Nx - 1) || fz > static_cast<float>(q.Nz - 1)) return -1e9f;
        const int ix = std::min(static_cast<int>(fx), q.Nx - 2), iz = std::min(static_cast<int>(fz), q.Nz - 2);
        const float u = fx - static_cast<float>(ix), v = fz - static_cast<float>(iz);
        const float* r0 = &q.H[static_cast<size_t>(iz) * static_cast<size_t>(q.Nx) + static_cast<size_t>(ix)]; const float* r1 = r0 + q.Nx;
        if (lowest) return std::min(std::min(r0[0], r0[1]), std::min(r1[0], r1[1]));
        return (r0[0] * (1 - u) + r0[1] * u) * (1 - v) + (r1[0] * (1 - u) + r1[1] * u) * v;
    }

    void EaglesAscent::LandExtent(float& x0, float& z0, float& x1, float& z1) const
    {
        x0 = mLandX; z0 = mLandZ; x1 = mLandX + mLandStep * static_cast<float>(std::max(mLandNx - 1, 0)); z1 = mLandZ + mLandStep * static_cast<float>(std::max(mLandNz - 1, 0));
        if (!mOuter.H.empty())
        {
            x0 = std::min(x0, mOuter.X); z0 = std::min(z0, mOuter.Z);
            x1 = std::max(x1, mOuter.X + mOuter.Step * static_cast<float>(mOuter.Nx - 1)); z1 = std::max(z1, mOuter.Z + mOuter.Step * static_cast<float>(mOuter.Nz - 1));
        }
    }

    float EaglesAscent::Land(float x, float z) const
    {
        if (mLand.empty()) return -1e9f;
        const float fx = (x - mLandX) / mLandStep, fz = (z - mLandZ) / mLandStep;
        if (fx < 0 || fz < 0 || fx > static_cast<float>(mLandNx - 1) || fz > static_cast<float>(mLandNz - 1)) return OuterLand(x, z, false);
        const int ix = std::min(static_cast<int>(fx), mLandNx - 2), iz = std::min(static_cast<int>(fz), mLandNz - 2);
        const float u = fx - static_cast<float>(ix), v = fz - static_cast<float>(iz);
        const float* r0 = &mLand[static_cast<size_t>(iz) * static_cast<size_t>(mLandNx) + static_cast<size_t>(ix)]; const float* r1 = r0 + mLandNx;
        return (r0[0] * (1 - u) + r0[1] * u) * (1 - v) + (r1[0] * (1 - u) + r1[1] * u) * v;
    }

    float EaglesAscent::FlightLand(float x, float z) const
    {
        for (const auto& q : mPatches)
        {
            const float fx = (x - q.X) / q.Step, fz = (z - q.Z) / q.Step;
            if (fx < 0 || fz < 0 || fx > static_cast<float>(q.Nx - 1) || fz > static_cast<float>(q.Nz - 1)) continue;
            const int ix = std::min(static_cast<int>(fx), q.Nx - 2), iz = std::min(static_cast<int>(fz), q.Nz - 2);
            const float u = fx - static_cast<float>(ix), v = fz - static_cast<float>(iz);
            const float* r0 = &q.H[static_cast<size_t>(iz) * static_cast<size_t>(q.Nx) + static_cast<size_t>(ix)]; const float* r1 = r0 + q.Nx;
            return (r0[0] * (1 - u) + r0[1] * u) * (1 - v) + (r1[0] * (1 - u) + r1[1] * u) * v;
        }
        return Land(x, z);
    }

    float EaglesAscent::DrawnTop(float x, float z) const
    {
        constexpr float none = std::numeric_limits<float>::quiet_NaN();
        if (mTopCell.empty() || mTopsPer <= 0) return none;
        const float step = mLandStep / static_cast<float>(mTopsPer);
        const float sx = (x - mLandX) / step - .5f, sz = (z - mLandZ) / step - .5f;     // (the points are at their squares' middles)
        if (sx < 0 || sz < 0) return none;
        const int gx = static_cast<int>(sx), gz = static_cast<int>(sz);
        const auto at = [&](int ax, int az)
        {
            const int cj = ax / mTopsPer, ci = az / mTopsPer;
            if (cj >= mLandNx - 1 || ci >= mLandNz - 1) return none;
            const int b = mTopCell[static_cast<size_t>(ci) * static_cast<size_t>(mLandNx - 1) + static_cast<size_t>(cj)];
            if (b < 0) return none;
            return mTops[(static_cast<size_t>(b) * mTopsPer + static_cast<size_t>(az % mTopsPer)) * mTopsPer + static_cast<size_t>(ax % mTopsPer)];
        };
        const float u = sx - static_cast<float>(gx), v = sz - static_cast<float>(gz);
        return (at(gx, gz) * (1 - u) + at(gx + 1, gz) * u) * (1 - v) + (at(gx, gz + 1) * (1 - u) + at(gx + 1, gz + 1) * u) * v;
    }

    float EaglesAscent::LandLow(float x, float z) const
    {
        if (mLand.empty()) return -1e9f;
        const float fx = (x - mLandX) / mLandStep, fz = (z - mLandZ) / mLandStep;
        if (fx < 0 || fz < 0 || fx > static_cast<float>(mLandNx - 1) || fz > static_cast<float>(mLandNz - 1)) return OuterLand(x, z, true);
        const int ix = std::min(static_cast<int>(fx), mLandNx - 2), iz = std::min(static_cast<int>(fz), mLandNz - 2);
        const auto& grid = mFloor.size() == mLand.size() ? mFloor : mLand;
        const float* r0 = &grid[static_cast<size_t>(iz) * static_cast<size_t>(mLandNx) + static_cast<size_t>(ix)]; const float* r1 = r0 + mLandNx;
        return std::min(std::min(r0[0], r0[1]), std::min(r1[0], r1[1]));
    }

    float EaglesAscent::TreadTop(float x, float z, float nearY) const
    {
        float best = std::numeric_limits<float>::quiet_NaN();
        const auto found = mTreadCells.find((static_cast<long long>(std::floor(x / 4)) << 32) ^ static_cast<unsigned>(static_cast<int>(std::floor(z / 4))));
        if (found == mTreadCells.end()) return best;
        for (const int i : found->second)
        {
            const auto& r = mTreads[static_cast<size_t>(i)];
            const float dx = x - r.X, dz = z - r.Z, along = dx * r.Ax + dz * r.Az, across = dx * r.Az - dz * r.Ax;   // (as build_mountain.py turns them)
            if (std::abs(along) > r.Along / 2 || std::abs(across) > r.Across / 2 || std::abs(r.Top - nearY) > .45f) continue;
            // (the higher of two that both hold the point: a tread's box reaches a little under the nose of the next, and
            // what is drawn there is the upper one; 2026-11-19: the nearer to her own height was taken, the hidden one)
            if (!std::isfinite(best) || r.Top > best) best = r.Top;
        }
        return best;
    }

    bool EaglesAscent::TreadCenter(float x, float z, float top, float& cx, float& cz, float& ax, float& az) const
    {
        float best = 2.f;   // m: the nearest tread of that height within this
        for (int i = -1; i <= 1; ++i)
            for (int j = -1; j <= 1; ++j)
            {
                const auto found = mTreadCells.find((static_cast<long long>(std::floor(x / 4) + i) << 32) ^ static_cast<unsigned>(static_cast<int>(std::floor(z / 4)) + j));
                if (found == mTreadCells.end()) continue;
                for (const int n : found->second)
                {
                    const auto& r = mTreads[static_cast<size_t>(n)];
                    if (std::abs(r.Top - top) > .05f) continue;
                    const float dx = x - r.X, dz = z - r.Z, across = std::abs(dx * r.Az - dz * r.Ax);
                    if (across > r.Across / 2) continue;
                    const float d = std::abs(dx * r.Ax + dz * r.Az);
                    if (d < best) { best = d; cx = r.X; cz = r.Z; ax = r.Ax; az = r.Az; }
                }
            }
        return best < 2.f;
    }

    const EaglesAscent::Zone* EaglesAscent::ZoneAt(float y) const
    {
        const Zone* zone = nullptr;
        for (const auto& z : Zones) if (y >= z.From || !zone) zone = &z;
        return zone;
    }

    float EaglesAscent::RingDistance(const XMFLOAT3& p) const
    {
        float best = 1e30f;
        for (const auto& r : mRings)
            for (const auto& q : r) best = std::min(best, (q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y) + (q.z - p.z) * (q.z - p.z));
        return best < 1e29f ? std::sqrt(best) : 1e30f;
    }

    bool EaglesAscent::InCave(const XMFLOAT3& p, CaveSpot* spot) const
    {
        float best = 1e30f; bool found = false;
        for (const auto& c : mCaves)
        {
            if (p.x < c.MinX || p.x > c.MaxX || p.z < c.MinZ || p.z > c.MaxZ || p.y < c.MinY || p.y > c.MaxY) continue;
            for (size_t i = 0; i + 1 < c.Line.size(); ++i)
            {
                const auto& a = c.Line[i]; const auto& b = c.Line[i + 1];
                const float ex = b.X - a.X, ez = b.Z - a.Z, l2 = ex * ex + ez * ez;
                if (l2 < 1e-6f) continue;
                const float t = std::clamp(((p.x - a.X) * ex + (p.z - a.Z) * ez) / l2, 0.f, 1.f);
                const float l = std::sqrt(l2), rx = ez / l, rz = -ex / l;          // level, to the right of a -> b
                const float cx = a.X + ex * t, cz = a.Z + ez * t;
                const float across = (p.x - cx) * rx + (p.z - cz) * rz;
                const float half = a.Half + (b.Half - a.Half) * t, floor = a.Y + (b.Y - a.Y) * t, tall = a.Tall + (b.Tall - a.Tall) * t;
                if (std::abs(across) > half + .6f || p.y < floor - 1.5f || p.y > floor + tall + .5f) continue;
                const float d = std::abs(across) / std::max(half, .1f) + std::abs(p.y - floor - tall * .4f) / std::max(tall, .1f);
                if (d < best)
                {
                    best = d; found = true;
                    if (spot) *spot = {floor, floor + tall, across, half, rx, rz};
                }
            }
        }
        return found;
    }

    void EaglesAscent::KeepOutOfRock(Player& p, bool flying, float dt, const DoorwaysRuntimeScene* scene)
    {
        Sliding = false;
        if (p.IsGrounded || flying) { mAir = 0; mJumped = false; }       // (how long she has been off the ground on foot, and
        else { if (mAir <= 0 && p.VerticalVelocity > .5f) mJumped = true; mAir += dt; }   //  whether she left it by a jump)
        if (!flying) HoldInCave(p);
        HoldOffTrees(p, flying);
        CaveSpot cave;
        if (InCave(p.Position, &cave))
        {   // In a cave (2026-11-18): the land's rules do not hold under the mountain. The tube is her collision on foot;
            // in flight she is kept inside it: off the floor, under the roof, off the walls.
            if (flying)
            {
                const float room = std::max(cave.Half - .9f, 0.f);
                if (std::abs(cave.Across) > room)
                {
                    const float push = (std::abs(cave.Across) - room) * (cave.Across > 0 ? 1.f : -1.f);
                    p.Position.x -= cave.RightX * push; p.Position.z -= cave.RightZ * push;
                }
                p.Position.y = std::clamp(p.Position.y, cave.Floor + .05f, std::max(cave.Floor + .05f, cave.Roof - 1.9f));
            }
            else if (p.Position.y < cave.Floor - .3f) { p.Position.y = cave.Floor; p.VerticalVelocity = std::max(p.VerticalVelocity, 0.f); }
            mSlideStill = 0; mSlidAgo = 1;
            mFlown = p.Position; mHasFlown = true;
            if (p.IsGrounded) { mStood = p.Position; mHasStood = true; }
            return;
        }
        // On foot a rock face or a bank is a wall where it is drawn (2026-11-22, the owner: "i want whole mountain to be
        // walkable beside the paths we have"). The engine stands her on ground under 60 degrees and stops her at nothing
        // steeper; the land's 6 m cells (two flat triangles, one under the face) and the rough ground reach on under rock
        // as drawn. At a cliff's foot she walked into the face, her body in the rock, until that ground ended and she fell
        // inside the mountain (the off-the-way test, CAUCASIAN_EAGLE_DEMO=14: 14 of its first 61 runs). Now where the drawn
        // top WallAhead m ahead of her (her front) would be more than IntoRock over her feet she is held where she was,
        // sliding along the face where one way is free. Not if she is in it already (she walks out), nor near the way.
        if (RockRules && !flying && mHasFlown && !mTopCell.empty() && !NearTheWay(p.Position) && !(DrawnTop(mFlown.x, mFlown.z) > mFlown.y + IntoRock))
        {
            const auto wall = [&](float x, float z)
            {
                const float dx = x - mFlown.x, dz = z - mFlown.z, d = std::hypot(dx, dz);
                if (d < 1e-5f) return false;
                return DrawnTop(x + dx / d * WallAhead, z + dz / d * WallAhead) > p.Position.y + IntoRock;   // (NaN: none known, no wall)
            };
            if (wall(p.Position.x, p.Position.z))
            {
                if (!wall(p.Position.x, mFlown.z)) p.Position.z = mFlown.z;
                else if (!wall(mFlown.x, p.Position.z)) p.Position.x = mFlown.x;
                else { p.Position.x = mFlown.x; p.Position.z = mFlown.z; }
                ++WallHolds;
            }
        }
        // Nor does she walk into the mountain where it is steeper than she can stand on (2026-11-22). The engine neither
        // stands her on such a face nor stops her at it: at a cliff's foot she walked on into the rock, on the ground
        // that reaches in under it, until that ended and she fell inside the mountain (the off-the-way test's film at
        // 340 m: along the cliff's foot, 2 m under the land, then down 9 m in the rock). A step on foot that leaves
        // the ground (no jump: within half a second of leaving it on foot, and not rising; the engine lets go of her a few
        // frames before the ground under her ends) to where there is no ground she can stand on within 6 m under her
        // feet, and the land's lowest there is not 1.5 m under them (no air under her: the mountain's inside), is not
        // taken: she stays where she was, or slides along the face where one way is free, and is put back on her feet.
        // Off a real edge (air under it, or ground to land on) she walks off as before.
        if (RockRules && !flying && mHasFlown && mHasStood && !p.IsGrounded && !mJumped && mAir < .5f && p.VerticalVelocity <= .5f && scene)
        {
            const float y = p.Position.y;
            const auto into = [&](float x, float z)
            {   // air under her there: the drawn top under her feet where it is known (the 6 m grid's lowest corner can lie
                // 7 m under her in a cell that is all rock face over her: at 380 m she fell into the mountain so)
                const float top = DrawnTop(x, z);
                if (std::isfinite(top) ? top < y - .5f : LandLow(x, z) < y - 1.5f) return false;
                return !Doorways::QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*scene, x, z, y - 6.f, y + .6f).Hit;
            };
            if (into(p.Position.x, p.Position.z))
            {
                if (!into(p.Position.x, mFlown.z)) p.Position.z = mFlown.z;
                else if (!into(mFlown.x, p.Position.z)) p.Position.x = mFlown.x;
                else { p.Position.x = mFlown.x; p.Position.z = mFlown.z; }
                const auto under = Doorways::QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*scene, p.Position.x, p.Position.z, y - 6.f, y + .6f);
                if (under.Hit && under.GroundY > y - .6f) p.Position.y = under.GroundY;   // on her feet there (along the face, or back)
                else p.Position = mStood;                                                 // or where she last stood
                p.IsGrounded = true; p.VerticalVelocity = 0; mAir = 0;
                p.GroundState = Doorways::DoorwaysPhysicsGroundState::Grounded; p.MissingAuthoritativeGround = false;
                p.GroundY = p.TargetGroundY = p.Position.y;
                ++IntoHolds;
            }
        }
        if (flying)
        {
            const float clear = .05f;                    // m over the land (more, and where the land is level with the path a landing hovers over it and never touches down)
            const auto blockedAt = [&](float x, float z, float fx, float fz)
            {   // under the land there, and the land rises more steeply from where she was than feet could climb
                const float h = FlightLand(x, z); if (p.Position.y >= h + clear) return false;
                const float run = std::hypot(x - fx, z - fz);
                return h - std::max(FlightLand(fx, fz), mFlown.y - clear) > .95f * run + .05f;   // the stairs climb .83 at most: she is lifted along them, rock stops her
            };
            const auto blocked = [&](float x, float z)
            {   // her middle, and a ring 0.9 m round it (her body and most of her wings; 2026-11-18, the cleft probe)
                if (blockedAt(x, z, mFlown.x, mFlown.z)) return true;
                constexpr float r = .9f;
                for (const auto& [ox, oz] : {std::pair{r, 0.f}, std::pair{-r, 0.f}, std::pair{0.f, r}, std::pair{0.f, -r}})
                    if (blockedAt(x + ox, z + oz, mFlown.x + ox, mFlown.z + oz)) return true;
                return false;
            };
            if (mHasFlown && blocked(p.Position.x, p.Position.z))
            {   // slide along the rock where one direction is free
                if (!blocked(p.Position.x, mFlown.z)) p.Position.z = mFlown.z;
                else if (!blocked(mFlown.x, p.Position.z)) p.Position.x = mFlown.x;
                else { p.Position.x = mFlown.x; p.Position.z = mFlown.z; }
            }
            // lifted onto what is under her; in a fine patch only a little: there the highest drawn rock can be an overhang
            // over her (the grid sees rock from above), and she must stay under it, not be put through it
            const float h = FlightLand(p.Position.x, p.Position.z), low = Land(p.Position.x, p.Position.z);
            if (p.Position.y < h + clear && (h - p.Position.y < 2.f || h <= low + .01f)) p.Position.y = h + clear;
            else if (p.Position.y < low + clear) p.Position.y = low + clear;
        }
        else if (!p.IsGrounded && mHasFlown)
        {   // On foot in the air: a jump, or a fall off an edge (the session's WalkOffLedges). The engine stands her only on
            // ground under 60 degrees; on steeper rock she would fall into the mountain and be fetched back to the
            // checkpoint (13 of 156 falls in the leap test, 2026-11-18). She is held on the rock's face instead and slides
            // down it, the steeper the faster, until she reaches ground she can stand on. Only when she has just gone in
            // (from above it a step before): never up through an overhang she jumped under.
            const float h = FlightLand(p.Position.x, p.Position.z);
            const float was = FlightLand(mFlown.x, mFlown.z);
            // Ground she can stand on comes first (2026-11-18, the owner: "some path has gaps when nyxara jumps up it
            // falls inside those gaps of roads"). The land's grid is 6 m coarse: on the stairs it lies up to 4 m over the
            // steps in places and up to 0.6 m under them in others. A jump on a stair ended under this rule: she was
            // taken for in the rock, put on the grid and slid down it, over or under the stair, for metres (the hopping
            // walk, 2026-11-18: from 113 m back to 104 m, again and again). Where the scene has walkable ground within
            // reach under her feet, or less than a step over them, this rule has no say: the engine lands her on it. If
            // it is over her feet she is in it by less than a step, and is put on it.
            bool standable = false;
            if (scene)
            {
                const auto under = Doorways::QueryDoorwaysRuntimeSceneWalkableGroundInHeightRange(*scene, p.Position.x, p.Position.z, p.Position.y - 2.5f, p.Position.y + .6f);
                if (under.Hit)
                {
                    standable = true;
                    // (rising, she goes on rising from it: in a jump's first moments up a stair, the next tread over her
                    // feet stopped her jump there, 2026-11-20)
                    if (under.GroundY > p.Position.y) p.Position.y = under.GroundY;
                }
            }
            if (!standable && p.Position.y < h - .05f && mFlown.y >= was - .5f)
            {
                constexpr float e = .75f;
                const float gx = (FlightLand(p.Position.x + e, p.Position.z) - FlightLand(p.Position.x - e, p.Position.z)) / (2 * e);
                const float gz = (FlightLand(p.Position.x, p.Position.z + e) - FlightLand(p.Position.x, p.Position.z - e)) / (2 * e);
                const float g = std::hypot(gx, gz);
                if (g > .01f)
                {
                    const float slide = std::min(16.f, 5.f + 4.f * g);   // m/s down the face
                    p.Position.x -= gx / g * slide * dt; p.Position.z -= gz / g * slide * dt;
                }
                p.Position.y = std::max(p.Position.y, FlightLand(p.Position.x, p.Position.z) + .02f);
                p.VerticalVelocity = std::max(p.VerticalVelocity, -14.f);   // the rock checks her fall
                Sliding = true; mSlidAgo = 0;
            }
            else mSlidAgo += dt;
            // At the bottom of a notch both faces are too steep to stand on and she would slide to and fro for ever,
            // never on her feet (the slots of FLY05, FLY15 and FLY20 in the miss test). Slid to rest (within 1.5 m of one
            // spot for 0.5 s, sliding at least every 0.3 s: she rocks across the notch's floor, a step on the rock, a step
            // over it): she stands there, and can jump or take wing as from any ground.
            const float went = std::hypot(p.Position.x - mSlideAnchor.x, p.Position.z - mSlideAnchor.z) + std::abs(p.Position.y - mSlideAnchor.y);
            if (mSlidAgo > .3f) mSlideStill = 0;
            else if (mSlideStill <= 0 || went > 1.5f) { mSlideAnchor = p.Position; mSlideStill = dt; }
            else mSlideStill += dt;
            if (mSlideStill > .5f)
            {
                p.IsGrounded = true; p.VerticalVelocity = 0;
                p.GroundState = Doorways::DoorwaysPhysicsGroundState::Grounded; p.MissingAuthoritativeGround = false;
                p.GroundY = p.TargetGroundY = p.Position.y;
            }
        }
        else { mSlideStill = 0; mSlidAgo = 1; }
        mFlown = p.Position; mHasFlown = true;
        if (p.IsGrounded && !Sliding) { mStood = p.Position; mHasStood = true; }
    }

    bool EaglesAscent::NearTheWay(const XMFLOAT3& p) const
    {
        if (std::isfinite(TreadTop(p.x, p.z, p.y))) return true;
        for (const auto& c : mCaves)
            for (int end = 0; end < 2; ++end)
            {
                if (!c.Open[end] || c.Line.empty()) continue;
                const auto& q = end == 0 ? c.Line.front() : c.Line.back();
                if (std::abs(p.y - q.Y) < 8.f && std::hypot(p.x - q.X, p.z - q.Z) < 12.f) return true;
            }
        for (size_t i = 0; i + 1 < Trail.size(); ++i)
        {   // the way's line, piece by piece: within WayReach of it across, at about its height
            const XMFLOAT3& a = Trail[i]; const XMFLOAT3& b = Trail[i + 1];
            if (std::abs(p.x - a.x) > 12.f || std::abs(p.z - a.z) > 12.f) continue;
            const float dx = b.x - a.x, dz = b.z - a.z, ll = std::max(dx * dx + dz * dz, 1e-6f);
            const float t = std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) / ll, 0.f, 1.f);
            if (std::hypot(p.x - a.x - t * dx, p.z - a.z - t * dz) < WayReach && std::abs(p.y - (a.y + t * (b.y - a.y))) < 5.f) return true;
        }
        return false;
    }

    void EaglesAscent::Locate(const XMFLOAT3& p)
    {
        float best = 1e30f; size_t at = 0;
        for (size_t i = 0; i < Trail.size(); ++i)
        {
            const float dx = Trail[i].x - p.x, dy = Trail[i].y - p.y, dz = Trail[i].z - p.z;
            const float d = dx * dx + dy * dy + dz * dz;
            if (d < best) { best = d; at = i; }
        }
        if (Trail.empty()) return;
        OffTrail = std::sqrt(best); Along = static_cast<float>(at) * TrailStep;
        const size_t a = at > 0 ? at - 1 : at, b = std::min(at + 1, Trail.size() - 1);
        const float run = std::hypot(Trail[b].x - Trail[a].x, Trail[b].z - Trail[a].z);
        Slope = run > .01f ? (Trail[b].y - Trail[a].y) / run : 0;
    }

    void EaglesAscent::Visit(DoorwaysGameHost& host, int station)
    {
        station = std::clamp(station, 0, static_cast<int>(Stations.size()) - 1);
        const auto& s = Stations[static_cast<size_t>(station)];
        auto& p = host.World.MainPlayer;
        const XMFLOAT3& at = Rest(station);
        p.Position = {at.x, at.y + .05f, at.z}; p.Yaw = s.Yaw; p.VerticalVelocity = 0;
        p.TraversalMode = DoorwaysPlayerTraversalMode::Ground;
        GameSession().FreeFlightEnd = true; GameSession().ImpulseX = GameSession().ImpulseZ = 0;
        host.World.MainCamera.Yaw = s.Yaw; host.World.MainCamera.Pitch = -.12f;
        if (HealOnVisit) host.Combat.RestorePlayerHealth(host.World, 10000);
        mHasFlown = false; mInRock = 0;
        if (s.Checkpoint) Checkpoint = station;
    }

    const char* EaglesAscent::Air(DoorwaysGameHost& host, float dt, bool flying, bool rescue)
    {
        auto& p = host.World.MainPlayer; AirNow = L""; Rescued = std::max(0.f, Rescued - dt);
        const bool landing = GameSession().FreeFlightLandSpeed > 0 || GameSession().FreeFlightEnd;
        if (flying)
        {
            for (const auto& u : Updrafts)   // rising air carries her up, but not while she lands (it would hold her aloft for ever)
                if (!landing && std::hypot(p.Position.x - u.P.x, p.Position.z - u.P.z) < u.Radius && p.Position.y > u.P.y - 2 && p.Position.y < u.Top)
                { p.Position.y += u.Lift * dt; AirNow = L"UPDRAFT"; }
            for (const auto& w : Winds)
            {
                const float dx = p.Position.x - w.P.x, dy = p.Position.y - w.P.y, dz = p.Position.z - w.P.z;
                if (dx * dx + dy * dy + dz * dz < w.Radius * w.Radius) { GameSession().ImpulseX += w.X * w.Speed; GameSession().ImpulseZ += w.Z * w.Speed; AirNow = L"CROSSWIND"; }
            }
        }
        if (!flying && p.IsGrounded)
            for (size_t i = 0; i < Stations.size(); ++i)
            {   // a station's terrace; a nest's station only in its nest (she steps into it)
                const XMFLOAT3& at = Stations[i].HasNest ? Stations[i].Nest : Stations[i].P;
                if (Stations[i].Checkpoint && std::abs(p.Position.y - at.y) < 3 &&
                    std::hypot(p.Position.x - at.x, p.Position.z - at.z) < (Stations[i].HasNest ? NestReach : StationReach))
                    Checkpoint = static_cast<int>(i);
            }
        // falling inside the mountain: on foot, not grounded and 8 m under the land's lowest grid corner
        mInRock = !flying && !p.IsGrounded && p.Position.y < LandLow(p.Position.x, p.Position.z) - 8 && !InCave(p.Position) ? mInRock + dt : 0;
        // far from the trail is lost only when far from the rings too (2026-11-18: they go round the back of the mountain)
        // ... and only off the mountain altogether (2026-11-18: the whole round mountain has heights now, Land(), and is hers to explore)
        if (rescue && !Trail.empty() && (p.Position.y < -6 || (OffTrail > 1500 && RingDistance(p.Position) > 600 && Land(p.Position.x, p.Position.z) < -1e8f) || mInRock > 1))
        {
            const char* event = mInRock > 1 ? "rescued_from_inside_the_mountain" : "rescued";
            Visit(host, Checkpoint); Rescued = 3;
            return event;
        }
        return nullptr;
    }

    void EaglesAscent::DrawModels(Renderer& renderer, const WorldState& world, const XMMATRIX& vp, int light)
    {
        ModelsDrawn = 0;
        if (mProps.empty()) return;
        renderer.ResetDefaultWorldRenderState(); renderer.BindDoorwaysSkinningDisabled();
        const XMFLOAT3 eye = world.MainCamera.Position;
        Material m{}; m.Diffuse = {1, 1, 1, 1}; m.TexTransform = {1, 1, 0, 0}; m.MetallicFactor = 0; m.RoughnessFactor = .9f;
        m.SpecularColor = {0, 0, 0, 0}; m.EmissiveStrength = light == 2 ? .25f : .04f; m.DoubleSided = true; m.ReceiveFog = true;
        for (const auto& prop : mProps)
        {
            if (prop.Hidden) continue;
            const float dx = prop.P.x - eye.x, dy = prop.P.y - eye.y, dz = prop.P.z - eye.z;
            const float limit = prop.Height > PropBig ? PropFar : PropNear;
            if (dx * dx + dy * dy + dz * dz > limit * limit) continue;
            const auto& model = mPropModels[static_cast<size_t>(prop.Model)];
            const float scale = prop.Height > 0 ? prop.Height / std::max(model.Max.y - model.Min.y, 1e-4f) : 1.f;   // (0: as made)
            m.DiffuseMap = model.Texture.Get();
            renderer.DrawDoorwaysGpuMesh(model.Mesh,
                XMMatrixTranslation(-(model.Min.x + model.Max.x) * .5f, -model.Min.y, -(model.Min.z + model.Max.z) * .5f) * XMMatrixScaling(scale, scale, scale) *
                XMMatrixRotationY(prop.Yaw) * XMMatrixTranslation(prop.P.x, prop.P.y, prop.P.z), vp, m);
            ++ModelsDrawn;
        }
    }

    std::vector<XMFLOAT3> EaglesAscent::BuiltCircles() const
    {
        std::vector<XMFLOAT3> out;
        for (const auto& prop : mProps)
        {
            const auto& model = mPropModels[static_cast<size_t>(prop.Model)];
            const float scale = prop.Height > 0 ? prop.Height / std::max(model.Max.y - model.Min.y, 1e-4f) : 1.f;
            const float r = .5f * std::hypot(model.Max.x - model.Min.x, model.Max.z - model.Min.z) * scale;
            if (r > .3f) out.push_back({prop.P.x, prop.P.z, r + .4f});
        }
        return out;
    }

    const XMFLOAT3& EaglesAscent::Rest(int station) const
    {
        const auto& s = Stations[static_cast<size_t>(std::clamp(station, 0, static_cast<int>(Stations.size()) - 1))];
        return s.HasNest ? s.Nest : s.P;
    }

    int EaglesAscent::HideModels(const std::string& name, const XMFLOAT3& p, float radius, bool hidden)
    {
        int count = 0;
        for (auto& prop : mProps)
        {
            const float dx = prop.P.x - p.x, dy = prop.P.y - p.y, dz = prop.P.z - p.z;
            if (mPropNames[static_cast<size_t>(prop.Model)] != name || dx * dx + dy * dy + dz * dz > radius * radius) continue;
            prop.Hidden = hidden; ++count;
        }
        return count;
    }

    bool EaglesAscent::FindModel(const std::string& name, const XMFLOAT3& p, float radius, XMFLOAT3& at, float& height) const
    {
        float best = radius * radius;
        bool found = false;
        for (const auto& prop : mProps)
        {
            const float dx = prop.P.x - p.x, dy = prop.P.y - p.y, dz = prop.P.z - p.z, d = dx * dx + dy * dy + dz * dz;
            if (mPropNames[static_cast<size_t>(prop.Model)] != name || d > best) continue;
            best = d; at = prop.P; height = prop.Height; found = true;
        }
        return found;
    }

    void EaglesAscent::StyleEnvironment(EnvironmentSettings& e, int light, int fog)
    {
        // day, dawn, night. Day is the owner's target picture (Gate 2): a warm, low afternoon sun from over the sea
        // (the west), a pale hazy sky, cool shadows.
        static const XMFLOAT4 sky[3]{{.62f, .71f, .82f, 1}, {.78f, .56f, .44f, 1}, {.012f, .022f, .05f, 1}};
        static const XMFLOAT4 sun[3]{{1, .87f, .68f, 1}, {1, .70f, .48f, 1}, {.50f, .68f, .74f, 1}};
        static const XMFLOAT4 ambient[3]{{.38f, .43f, .54f, 1}, {.36f, .30f, .30f, 1}, {.07f, .11f, .13f, 1}};
        static const XMFLOAT4 direction[3]{{.72f, -.52f, .46f, 0}, {.85f, -.28f, .40f, 0}, {-.40f, -.70f, .55f, 0}};
        const auto i = static_cast<size_t>(std::clamp(light, 0, 2));
        e.ClearColor = sky[i];
        e.Lighting.DirectionalLight.Direction = direction[i]; e.Lighting.DirectionalLight.Color = sun[i]; e.Lighting.DirectionalLight.Intensity = 1.6f;
        e.Lighting.AmbientLight.Color = ambient[i]; e.Lighting.AmbientLight.Intensity = 1;
        // The engine's sky box is a room round the origin; the map is far bigger. It is pushed out of sight, so the sky is the clear colour.
        e.Sky.Distance = 40000; e.Sky.Height = 80000; e.Sky.FrontBackPanelWidth = 80000; e.Sky.SidePanelLength = 80000; e.Sky.CenterY = 0; e.Sky.TopY = 40000;
        e.Sky.PanelDiffuse = sky[i]; e.Sky.PanelEmissiveStrength = 1; e.Sky.HorizonDiffuse = {0, 0, 0, 0}; e.Sky.HorizonCenterY = -30000;
        // By day and at dawn the engine's procedural sky (2026-11-18): the haze colour at the horizon, deeper overhead,
        // the sun and clouds. Night keeps the clear colour.
        static const XMFLOAT4 zenith[3]{{.30f, .47f, .74f, 1}, {.36f, .42f, .62f, 1}, {.01f, .02f, .05f, 1}};
        e.Sky.Procedural = i < 2; e.Sky.ZenithColor = zenith[i]; e.Sky.CloudCover = i == 0 ? .42f : .55f;
        e.Fog.Color = sky[i]; e.Fog.HeightFogEnabled = 0; e.Fog.HeightFogStrength = 0; e.Fog.MoonHazeStrength = 0; e.Fog.BaseFogAmount = 0;
        if (fog == 0) e.Fog.Strength = 0;
        // haze: the map's aerial perspective (Gate 4): nothing on the path, about a seventh at 5 km (the far mountain
        // from the sea), cool sky colour. Was 300-7,000 m at 0.75, which washed the whole mountain out from C0; 0.6 was still pale.
        else if (fog == 1) { e.Fog.StartDistance = 800; e.Fog.EndDistance = 12000; e.Fog.Strength = .45f; e.Fog.Density = 1; e.Fog.DistanceCurvePower = 1; }
        else { e.Fog.StartDistance = 30; e.Fog.EndDistance = 700; e.Fog.Strength = 1; }
    }
}
