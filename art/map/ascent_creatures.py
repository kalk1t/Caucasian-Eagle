"""The Eagle's Ascent (Map A): where its Eagle Nests, its bosses and its creatures are. The placing model (2026-11-20).

    python art/map/ascent_creatures.py                      writes game/assets/Maps/EaglesAscent.creatures.txt
    python art/map/ascent_creatures.py --pictures <folder>  and draws the plan (profile.png, map.png, PLACING.md) there

The owner's plan (2026-11-19, "THE MAP PLAN") and their answers:
  - four bosses, counted from the shore: the Gveleshapi about 1,100 m along the way, the Giant (a Uaig of a boss's size)
    about 2,250 m, the Devi Lord about 3,400 m, and the Eagle himself last, in Amiran's court (7,632 m);
  - the eight creatures of the road (Chinka, Ali, Kudiani, Ochokochi, Almasty, Kaji, Uaig, Devi), never the Rokapi, in
    small groups of one to three about every 100 m, the harder ones higher up; never more than three alive at once (the
    game wakes the groups nearest her, as many as that allows);
  - no waves: each group stands in its place or walks its stretch of the way (a patrol); she may fly away from a fight;
  - creatures in the caves beside the way too;
  - nine Eagle Nests on the way, the only places she is saved: at the nine stations between the shore and the court;
  - a creature she has beaten stays beaten (the game keeps them with her save).
And on 2026-11-21: her wings, swords and armour have nine levels, bought only in the nests; between two nests she finds 60
to 70 % of what a level of all three costs at the next on the way, the rest in the caves and around the way, where groups
now stand every 150 to 400 m (`level` and `around` lines; the crystals' ledger is printed, and the game checks it:
CAUCASIAN_EAGLE_DEMO=8).

Everything is worked out from the map's own data (its trail every 4 m, stations, wing stretches and caves), the same every
time. To move a place or change who is there, add a line to HAND by its id and run this again. The game reads the file
the next time it starts (EaglesAscent::LoadCreatures); nothing else needs building.
"""
import argparse
import math
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
STEP = 4.0                      # m between the map's trail samples (EaglesAscent::TrailStep)

KINDS = ["chinka", "ali", "kudiani", "ochokochi", "almasty", "kaji", "uaig", "devi"]
BOSS_KINDS = ["gveleshapi", "giant", "devi_lord", "eagle"]
NAMES = {"chinka": "Chinka", "ali": "Ali", "kudiani": "Kudiani", "ochokochi": "Ochokochi", "almasty": "Almasty",
         "kaji": "Kaji", "uaig": "Uaig", "devi": "Devi", "gveleshapi": "The Gveleshapi", "giant": "The Giant",
         "devi_lord": "The Devi Lord", "eagle": "The Eagle"}

# Who lives on each leg of the way (station n to n + 1) and how often each comes: the harder ones higher up.
ROSTER = [
    {"chinka": 3, "ali": 1},                                     # 0-1 the coast
    {"chinka": 3, "kudiani": 2, "ali": 1},                       # 1-2 the old village terraces
    {"ali": 3, "chinka": 2, "kudiani": 2},                       # 2-3 the waterfall gorge
    {"ochokochi": 3, "kudiani": 2, "chinka": 2, "almasty": 1},   # 3-4 the lower forest
    {"almasty": 3, "ochokochi": 2, "kaji": 1, "kudiani": 1},     # 4-5 the Eagle shrine and the ravine
    {"almasty": 2, "kaji": 2, "ochokochi": 2, "ali": 1},         # 5-6 the upper forest and the cloud line
    {"kaji": 3, "almasty": 2, "uaig": 1},                        # 6-7 the highland pasture and the stone refuge
    {"kaji": 2, "uaig": 2, "kudiani": 1, "devi": 1},             # 7-8 the cliff temples and the wind bridges
    {"uaig": 2, "devi": 2, "kaji": 1, "almasty": 1},             # 8-9 the snow line and the chain ascent
    {"devi": 2, "uaig": 1},                                      # 9-10 Amiran's summit sanctuary
]
# In the caves: those that keep to the dark (the smiths' forges, the witches, the wild men)
CAVE_LIKES = {"kudiani": 2, "kaji": 2, "almasty": 1, "chinka": 1, "devi": 1}
# How many come together: the small ones in pairs and threes, the big ones alone (a big one may bring a small one)
TOGETHER = {"chinka": (2, 3), "ali": (1, 2), "kudiani": (1, 2), "ochokochi": (1, 2), "almasty": (1, 2),
            "kaji": (1, 2), "uaig": (1, 1), "devi": (1, 1)}
WEIGHT = {k: i for i, k in enumerate(KINDS)}    # the order of the road's table is their strength
HEIGHT = {"chinka": 1.15, "ali": 2.0, "kudiani": 1.75, "ochokochi": 2.7, "almasty": 2.9, "kaji": 2.6, "uaig": 4.6,
          "devi": 5.2}                          # m (Journey's table): a cave must be higher than who lives in it

BOSSES = [("gveleshapi", 1100.0), ("giant", 2250.0), ("devi_lord", 3400.0)]   # (and the Eagle in Amiran's court)
EVERY = 100.0          # m between the groups on the way
SPACING = 65.0         # m at the least between two groups on the way (each takes the levellest place of its 100 m)
FIRST = 150.0          # m of the shore before the first group
NEST_CLEAR = 45.0      # m round a nest kept free of creatures (and they do not follow her into it)
BOSS_CLEAR = 70.0      # m round a boss's place kept free of other groups
FLY_CLEAR = 12.0       # m from a stretch only wings cross (she lands there)
LEASH = 40.0           # m along the way a creature follows her from its place, at most
PATROL = 20.0          # m to either side of its place a patrol walks
CAVE_REACH = 40.0      # a cave with a mouth at the foot of the rock this near the way gets a group

# ---- her growth's crystals (the owner, 2026-11-21): her wings, swords and armour have nine levels each now, bought only
# at an Eagle Nest: "make sure between each nest nyxara earns 60-70% of the enough crystals to level up in nest - so nest
# saves and lets you level up , then an other 40-30% should be found in caves and around paths , around path didnt have
# any monsters before and you could add there also (every 150-400 meter)". Asked, they chose: all that lies between two
# nests pays for one level of all three at the second nest (each at level 9 at the ninth), the way alone about two of
# the three. So for nest n: the way (its groups, its bosses and the clay jars beside it) holds WAY_SHARE of three times
# PRICES[n - 1], and the rest lies off it: the groups in the caves, new groups around the way (AROUND_EVERY apart, each
# off the way on ground she can walk to: the game finds the place), and the clay jars in the caves and on the rings. The
# jars' crystals are not left to chance any more: this file says how many lie beside the way and how many off it
# between each two nests, and the game shares them out among the jars there (Wayside::Lay).
PRICES = [8, 14, 21, 27, 33, 39, 46, 52, 58]   # crystals for level 1 .. 9 of any one of her three things
WAY_SHARE = 0.65         # of the three levels' price: on the way itself (the owner: 60 to 70 %)
CRYSTALS = {"chinka": 1, "ali": 2, "kudiani": 2, "ochokochi": 3, "almasty": 4, "kaji": 5, "uaig": 8, "devi": 10}   # Journey's Kinds[].Cost
BOSS_CRYSTALS = 25       # a boss of the Ascent (GameConstants.h AscentBossCrystals)
AROUND_EVERY = (150.0, 400.0)   # m between two groups around the way (the owner)
AROUND_PART = 0.6        # of what the caves leave of the part off the way: the groups around it (the rest: the jars off it)
AROUND_CLEAR = 60.0      # m round a nest without them
AROUND_BOSS = 100.0      # m round a boss's place without them

# Hand changes, by id: {"id": {"along": m} to move a group on the way, "kinds": [...], "keep": "fixed"|"patrol",
# "skip": True}. Example: HAND = {"way-1800": {"kinds": ["kudiani", "chinka"]}}
HAND = {
    # (groups around the way with no ground beside it to stand on: the game found none within 120 m along the way, on
    # either side, 9 to 30 m out (2026-11-21, its crystals' ledger, CAUCASIAN_EAGLE_DEMO=8): the way climbs cliffs from
    # past the Gveleshapi to the third nest, and above the seventh. Their crystals lie in the jars off the way instead.)
    "around-1258": {"skip": True}, "around-1410": {"skip": True}, "around-1562": {"skip": True}, "around-5550": {"skip": True},
    # (and two whose only ground beside the way is within 140 m of another's, around-6608's and around-0922's, once the
    # game keeps each group around the way a fight of its own: 26 m from every other group, the nearest from where its
    # line leaves the way, and 26 m there from any group further up it: EaglesAscent::PlaceAround, 2026-11-21; of
    # around-0698 and around-0922 one had to go, and without around-0698 the first gap is 400 m, not 1,126 m on)
    "around-6852": {"skip": True}, "around-0698": {"skip": True},
    # (and one whose only ground is within 100 m of cave-14's group, the two more than the three alive at once: it held
    # the three places as she went into the cave, and the cave's group never woke: the creatures' tour, 2026-11-21)
    "around-4374": {"skip": True},
    # (and one whose only ground beside the way stood at a cliff's edge: once the map takes the carved rock as collision
    # in steep cells near the way, the engine's steep walls holding her there (2026-11-23), around-0522's one place, 9 m
    # out at 466 m, is no longer level 3 m round it (the land drops 4 m there, which the land's coarse 6 m cell had passed
    # off as level); from 402 m to 557 m it has no other, and farther up, at 730 m, it left around-0922 none)
    "around-0522": {"skip": True},
}

# Where each Eagle Nest stands beside the way at its station: station -> (m to the right of the way, m along it, the
# ground's height there). Measured by the game itself (2026-11-20, the creatures' tour, CAUCASIAN_EAGLE_DEMO=3, its
# "nestspot" rows): from the station, on the side the land rises first, 4.5 to 10.5 m out and up to 6 m along, the first
# place that is level under the whole nest, has no drop right beside it and nothing standing in it (a gate pillar, rock).
# A station not here: the nest stands in the middle of the way (she walks through it).
NEST_SPOTS = {1: (-6.0, 0.0, 180.00), 2: (-6.0, 0.0, 550.00), 3: (6.0, 0.0, 1050.00), 4: (6.0, 0.0, 1550.00), 5: (-6.0, 0.0, 2050.00),
              6: (6.0, 0.0, 2550.00), 7: (6.0, 0.0, 3050.00), 8: (6.0, 0.0, 3500.00), 9: (6.0, 0.0, 3950.00)}   # (the terraces are 44 m wide:
              # their two gate pillars stand 21 m out, clear of a nest 6 m out)


def read_map(path):
    m = {"stations": [], "trail": [], "fly": [], "caves": [], "zones": []}
    with open(path, encoding="utf-8") as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == "station":
                m["stations"].append({"name": p[1].replace("_", " "), "x": float(p[2]), "y": float(p[3]),
                                      "z": float(p[4]), "yaw": float(p[5]), "along": float(p[6]), "id": p[7]})
            elif p[0] == "trail":
                m["trail"].append((float(p[1]), float(p[2]), float(p[3])))
            elif p[0] == "fly":
                m["fly"].append((p[1], float(p[2]), float(p[3])))
            elif p[0] == "cave":
                n = int(p[2])
                pts = [tuple(float(v) for v in p[3 + 5 * i: 8 + 5 * i]) for i in range(n)]
                m["caves"].append({"kind": p[1], "line": pts, "index": len(m["caves"])})
            elif p[0] == "zone":
                m["zones"].append((float(p[1]), p[2]))
    return m


class Way:
    """The trail: a point every STEP m, from the shore (0) to Amiran's court."""

    def __init__(self, m):
        self.p = m["trail"]
        self.fly = m["fly"]
        self.nests = [s["along"] for s in m["stations"][1:-1]]
        self.length = (len(self.p) - 1) * STEP

    def at(self, s):
        i = min(max(int(round(s / STEP)), 0), len(self.p) - 1)
        return self.p[i]

    def rise(self, s, half=12.0):
        """m up per m along over s - half .. s + half (the steepest step of it): 0 on a level stretch."""
        worst = 0.0
        k = s - half
        while k < s + half:
            a, b = self.at(k), self.at(k + STEP)
            worst = max(worst, abs(b[1] - a[1]) / STEP)
            k += STEP
        return worst

    def flying(self, s, clear=FLY_CLEAR):
        return any(a - clear <= s <= b + clear for _, a, b in self.fly)

    def near_nest(self, s, clear=NEST_CLEAR):
        return any(abs(s - n) < clear for n in self.nests)

    def leg(self, s, stations):
        for i in range(len(stations) - 1):
            if s < stations[i + 1]["along"]:
                return i
        return len(stations) - 2

    def bounds(self, s, reach):
        """lo, hi: as far as reach either way, not into a stretch only wings cross nor into a nest's ground."""
        lo, hi = max(0.0, s - reach), min(self.length, s + reach)
        for _, a, b in self.fly:
            if b <= s:
                lo = max(lo, b + 4)
            elif a >= s:
                hi = min(hi, a - 4)
        for n in self.nests:
            if n <= s:
                lo = max(lo, n + NEST_CLEAR * .6)
            else:
                hi = min(hi, n - NEST_CLEAR * .6)
        return lo, hi

    def facing(self, s):
        """the yaw that looks back down the way (at her as she comes up it)"""
        a, b = self.at(s), self.at(s - 8)
        return math.atan2(b[0] - a[0], b[2] - a[2])

    def best(self, lo, hi, avoid):
        """the levellest place in lo..hi that is not too near what avoid() refuses"""
        pick, score = None, 1e9
        s = lo
        while s <= hi:
            if not avoid(s):
                r = self.rise(s) + .25 * self.rise(s, 24.0)
                if r < score:
                    pick, score = s, r
            s += STEP
        return pick


def pick_kinds(rnd, roster, big_ok=True):
    names = list(roster)
    kind = rnd.choices(names, weights=[roster[k] for k in names])[0]
    lo, hi = TOGETHER[kind]
    n = rnd.randint(lo, hi)
    kinds = [kind] * n
    if n < 3 and rnd.random() < .35:
        lighter = [k for k in names if WEIGHT[k] < WEIGHT[kind]]
        if lighter:
            kinds.append(rnd.choice(lighter))
    return kinds


def place(m):
    rnd = random.Random(20261119)
    way = Way(m)
    st = m["stations"]
    nests = [{"station": i, **s} for i, s in enumerate(st) if 0 < i < len(st) - 1]
    for n in nests:   # beside the way (NEST_SPOTS), facing it
        if n["station"] in NEST_SPOTS:
            lateral, along, ground = NEST_SPOTS[n["station"]]
            fx, fz, rx, rz = math.sin(n["yaw"]), math.cos(n["yaw"]), math.cos(n["yaw"]), -math.sin(n["yaw"])
            n["x"] += rx * lateral + fx * along
            n["z"] += rz * lateral + fz * along
            n["y"] = ground
            n["yaw"] = n["yaw"] + (-math.pi / 2 if lateral > 0 else math.pi / 2)   # its front (+z) to the way
    bosses = []
    for kind, target in BOSSES:
        s = way.best(target - 100, target + 150, lambda s: way.flying(s, 30) or way.near_nest(s, 60))
        x, y, z = way.at(s)
        lo, hi = way.bounds(s, 45)
        bosses.append({"id": "boss-" + kind.replace("_", "-"), "kind": kind, "along": s, "x": x, "y": y, "z": z,
                       "yaw": way.facing(s), "lo": lo, "hi": hi})
    court = st[-1]
    bosses.append({"id": "boss-eagle", "kind": "eagle", "along": court["along"], "x": court["x"], "y": court["y"],
                   "z": court["z"], "yaw": way.facing(court["along"] - 4), "lo": court["along"] - 60, "hi": way.length})

    groups = []
    centre = FIRST + EVERY / 2
    while centre < way.length - 60:
        last = groups[-1]["along"] if groups else -1e9

        def refuse(s):
            return (way.flying(s) or way.near_nest(s) or s < FIRST or s < last + SPACING or
                    any(abs(s - b["along"]) < BOSS_CLEAR for b in bosses))
        s = way.best(centre - EVERY * .4, centre + EVERY * .4, refuse)
        gid = "way-%04d" % int(round(centre))
        hand = HAND.get(gid, {})
        if hand.get("along") is not None:
            s = float(hand["along"])
        if s is not None and not hand.get("skip"):
            leg = way.leg(s, st)
            kinds = hand.get("kinds") or pick_kinds(rnd, ROSTER[leg])
            lo, hi = way.bounds(s, LEASH)
            level = way.rise(s, PATROL) < .45 and hi - lo > PATROL * 1.6
            keep = hand.get("keep") or ("patrol" if level and rnd.random() < .45 else "fixed")
            x, y, z = way.at(s)
            groups.append({"id": gid, "keep": keep, "along": s, "x": x, "y": y, "z": z, "yaw": way.facing(s),
                           "lo": lo, "hi": hi, "roam": PATROL if keep == "patrol" else 0.0, "kinds": kinds, "leg": leg})
        centre += EVERY

    caves = []
    for c in m["caves"]:
        first, second = c["kind"].split("-")
        if "foot" not in (first, second):
            continue
        line = c["line"] if first == "foot" else list(reversed(c["line"]))
        mouth = line[0]
        near = min(range(len(way.p)), key=lambda i: (way.p[i][0] - mouth[0]) ** 2 + (way.p[i][2] - mouth[2]) ** 2)
        off = math.hypot(way.p[near][0] - mouth[0], way.p[near][2] - mouth[2])
        if off > CAVE_REACH or abs(way.p[near][1] - mouth[1]) > 15:
            continue
        if any(abs(near * STEP - b["along"]) < BOSS_CLEAR + 30 for b in bosses):
            continue      # not beside a boss's ground: its fight has the three she may face at once to itself
        lengths = [0.0]
        for a, b in zip(line, line[1:]):
            lengths.append(lengths[-1] + math.dist((a[0], a[1], a[2]), (b[0], b[1], b[2])))
        total = lengths[-1]
        chamber = second == "chamber"
        at = total - 6 if chamber else total * .35
        i = min(range(len(line)), key=lambda k: abs(lengths[k] - at))
        x, y, z = line[i][0], line[i][1], line[i][2]
        j = max(i - 2, 0)
        yaw = math.atan2(line[j][0] - x, line[j][2] - z)      # looking back to the mouth
        # its line runs from the cave's first point; at, lo, hi are metres along it from there
        flip = first != "foot"
        lo, hi = (max(0.0, at - 30), total) if chamber else (max(0.0, at - PATROL - 10), min(total, at + PATROL + 10))
        if flip:
            at, lo, hi = total - at, total - hi, total - lo
        leg = way.leg(near * STEP, st)
        # (in KINDS' order: a set's order changes from run to run with Python's string hashing, and the draws with it:
        # three runs gave 121, 123 and 124 creatures, 2026-11-20)
        roster = {k: ROSTER[leg].get(k, 0) + CAVE_LIKES.get(k, 0) for k in KINDS if k in ROSTER[leg] or k in CAVE_LIKES}
        roster = {k: v for k, v in roster.items() if v > 0 and WEIGHT[k] <= max(WEIGHT[q] for q in ROSTER[leg])}
        roof = min(q[4] for q in line[max(i - 3, 0): i + 4])      # the cave's height round its place
        roster = {k: v for k, v in roster.items() if HEIGHT[k] + .8 < roof} or {"chinka": 1}
        gid = "cave-%02d" % c["index"]
        hand = HAND.get(gid, {})
        if hand.get("skip"):
            continue
        kinds = hand.get("kinds") or pick_kinds(rnd, roster)
        keep = hand.get("keep") or ("fixed" if chamber else "patrol")
        caves.append({"id": gid, "keep": keep, "x": x, "y": y, "z": z, "yaw": yaw, "cave": c["index"], "at": at,
                      "lo": lo, "hi": hi, "roam": PATROL if keep == "patrol" else 0.0, "kinds": kinds,
                      "along": near * STEP, "off": off, "length": total, "chamber": chamber, "leg": leg})
    caves.sort(key=lambda g: g["along"])
    return nests, bosses, groups, caves, way


def stretch_along(a, nests):
    """1 on the way to the first nest .. 9 to the ninth, 10 past it"""
    return 1 + sum(1 for n in nests if n["along"] <= a)


def stretch_height(y, nests):
    """off the way (a cave, a ring): which nest's stretch, by its height against theirs"""
    return 1 + sum(1 for n in nests if n["y"] <= y)


def value(kinds):
    return sum(CRYSTALS[k] for k in kinds)


def economy(m, way, nests, bosses, groups, caves):
    """The groups around the way, the price of each level and the jars' crystals between each two nests (see PRICES)."""
    rnd = random.Random(20261121)
    st = m["stations"]
    levels, around, ledger = [], [], []
    last = None   # the last group around the way so far: m along it
    for k in range(1, len(nests) + 1):
        price = PRICES[k - 1]
        total = 3 * price
        main = int(round(WAY_SHARE * total))
        way_creatures = sum(value(g["kinds"]) for g in groups if stretch_along(g["along"], nests) == k)
        way_bosses = sum(BOSS_CRYSTALS for b in bosses if stretch_along(b["along"], nests) == k)
        way_jars = main - way_creatures - way_bosses
        if way_jars < 0:
            raise SystemExit("level %d: the way's creatures and bosses bring %d crystals, more than its %d: raise PRICES[%d]"
                             % (k, way_creatures + way_bosses, main, k - 1))
        side = total - main
        cave_creatures = sum(value(g["kinds"]) for g in caves if stretch_height(g["y"], nests) == k)
        room = max(0, side - cave_creatures)
        # the groups around the way: as many as bring about AROUND_PART of the room, AROUND_EVERY apart
        a0 = (nests[k - 2]["along"] if k > 1 else 0.0) + AROUND_CLEAR
        a1 = nests[k - 1]["along"] - AROUND_CLEAR

        def refuse(s):
            return (way.flying(s, 25) or way.near_nest(s, AROUND_CLEAR) or s < 40 or
                    any(abs(s - b["along"]) < AROUND_BOSS for b in bosses))
        leg = way.leg((a0 + a1) / 2, st)
        trial = random.Random(k)
        mean = sum(value(pick_kinds(trial, ROSTER[leg])) for _ in range(400)) / 400.0
        length = max(0.0, a1 - a0)
        least, most = max(1, math.ceil(length / AROUND_EVERY[1])), max(1, math.floor(length / AROUND_EVERY[0]))
        count = min(most, max(least, int(round(AROUND_PART * room / mean)))) if room > 0 else 0
        # (each at least AROUND_EVERY[0] past the one before, the last nest's included: where the ground is refused, a
        # nest's, a boss's or a stretch only wings cross, the gap may be longer than AROUND_EVERY[1])
        allowed = [a0 + STEP * i for i in range(int(length / STEP) + 1) if not refuse(a0 + STEP * i)]
        mine = []
        gap = length / count if count else 0.0
        for i in range(count):
            want = a0 + gap * (i + .5) + rnd.uniform(-1, 1) * min(30.0, max(0.0, (gap - AROUND_EVERY[0]) / 2))
            prev = mine[-1]["along"] if mine else last
            ok = [q for q in allowed if prev is None or q - prev >= AROUND_EVERY[0]]
            if not ok:
                break
            s = min(ok, key=lambda q: abs(q - want))
            if prev is not None and s - prev > AROUND_EVERY[1]:   # no more than that far from the last, where there is ground
                s = min([q for q in ok if q - prev <= AROUND_EVERY[1]] or ok, key=lambda q: abs(q - (prev + AROUND_EVERY[1])))
            gid = "around-%04d" % int(round(s))
            hand = HAND.get(gid, {})
            kinds = hand.get("kinds") or pick_kinds(rnd, ROSTER[way.leg(s, st)])
            mine.append({"id": gid, "keep": hand.get("keep") or "fixed", "along": s, "side": rnd.choice((1, -1)),
                         "kinds": kinds, "leg": way.leg(s, st), "stretch": k, "skip": bool(hand.get("skip"))})
        last = mine[-1]["along"] if mine else last
        # (a skipped one is left out only now: the others stand where they stood with it)
        mine = [g for g in mine if not g["skip"]]
        # never more than the room: the dearest group gives up a creature, or a lighter one comes instead
        while mine and value(sum((g["kinds"] for g in mine), [])) > room:
            g = max(mine, key=lambda q: value(q["kinds"]))
            if len(g["kinds"]) > 1:
                g["kinds"] = g["kinds"][:-1]
            else:
                lighter = sorted(ROSTER[g["leg"]], key=lambda q: CRYSTALS[q])
                if CRYSTALS[lighter[0]] < CRYSTALS[g["kinds"][0]]:
                    g["kinds"] = [lighter[0]]
                else:
                    mine.remove(g)
        around_creatures = value(sum((g["kinds"] for g in mine), []))
        side_jars = room - around_creatures
        around += mine
        levels.append({"level": k, "price": price, "way_jars": way_jars, "side_jars": side_jars})
        ledger.append({"level": k, "price": price, "way_creatures": way_creatures, "bosses": way_bosses, "way_jars": way_jars,
                       "caves": cave_creatures, "around": around_creatures, "around_groups": len(mine), "side_jars": side_jars})
    return around, levels, ledger


def write(path, nests, bosses, groups, caves, around, levels):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# The Eagle's Ascent: its Eagle Nests, bosses and creatures. Written by art/map/ascent_creatures.py from\n")
        f.write("# Maps/EaglesAscent.txt; do not edit: change the script's HAND table and run it again.\n")
        f.write("# nest <station> <x> <y> <z> <yaw> <along>\n")
        f.write("# boss <id> <kind> <x> <y> <z> <yaw> <along> <lo> <hi>   (lo..hi: m along the way it keeps to)\n")
        f.write("# group <id> <fixed|patrol> <x> <y> <z> <yaw> <along> <lo> <hi> <roam> <count> <kind>...\n")
        f.write("# cave <id> <fixed|patrol> <x> <y> <z> <yaw> <cave> <at> <lo> <hi> <roam> <count> <kind>...\n")
        f.write("#   (<cave>: its number among the map's cave records; at, lo, hi: m along its line from its first point)\n")
        f.write("# around <id> <fixed|patrol> <along> <side> <count> <kind>...   (a group around the way, at m along it, off it\n")
        f.write("#   to its side: 1 its right going up, -1 its left; the game finds its place there on ground she can walk to)\n")
        f.write("# level <n> <price> <way jars> <side jars>   (her growth: the price of level n of her wings, swords or armour, at an\n")
        f.write("#   Eagle Nest; and the crystals the clay jars hold on the way to nest n: those beside the way, and those off it)\n")
        for lv in levels:
            f.write("level %d %d %d %d\n" % (lv["level"], lv["price"], lv["way_jars"], lv["side_jars"]))
        for n in nests:
            f.write("nest %d %.2f %.2f %.2f %.4f %.1f\n" % (n["station"], n["x"], n["y"], n["z"], n["yaw"], n["along"]))
        for b in bosses:
            f.write("boss %s %s %.2f %.2f %.2f %.4f %.1f %.1f %.1f\n" % (b["id"], b["kind"], b["x"], b["y"], b["z"],
                                                                     b["yaw"], b["along"], b["lo"], b["hi"]))
        for g in groups:
            f.write("group %s %s %.2f %.2f %.2f %.4f %.1f %.1f %.1f %.1f %d %s\n" % (
                g["id"], g["keep"], g["x"], g["y"], g["z"], g["yaw"], g["along"], g["lo"], g["hi"], g["roam"],
                len(g["kinds"]), " ".join(g["kinds"])))
        for g in caves:
            f.write("cave %s %s %.2f %.2f %.2f %.4f %d %.1f %.1f %.1f %.1f %d %s\n" % (
                g["id"], g["keep"], g["x"], g["y"], g["z"], g["yaw"], g["cave"], g["at"], g["lo"], g["hi"], g["roam"],
                len(g["kinds"]), " ".join(g["kinds"])))
        for g in around:
            f.write("around %s %s %.1f %d %d %s\n" % (g["id"], g["keep"], g["along"], g["side"], len(g["kinds"]), " ".join(g["kinds"])))


COLOURS = {"chinka": (120, 220, 90), "ali": (110, 190, 255), "kudiani": (90, 200, 60), "ochokochi": (240, 130, 70),
           "almasty": (200, 200, 185), "kaji": (255, 120, 20), "uaig": (230, 215, 150), "devi": (210, 170, 40)}


def pictures(folder, m, nests, bosses, groups, caves, way):
    from PIL import Image, ImageDraw
    os.makedirs(folder, exist_ok=True)
    st = m["stations"]
    # ---- the profile: metres along the way, and height
    W, H, L, R, T, B = 2600, 820, 70, 30, 60, 90
    sx = (W - L - R) / way.length
    sy = (H - T - B) / 4300.0
    X = lambda s: L + s * sx
    Y = lambda y: H - B - y * sy
    im = Image.new("RGB", (W, H), (24, 27, 33))
    d = ImageDraw.Draw(im)
    for _, a, b in way.fly:
        d.rectangle([X(a), T, X(b), H - B], fill=(40, 58, 78))
    for i in range(len(st) - 1):
        a, b = st[i]["along"], st[i + 1]["along"]
        if i % 2:
            d.rectangle([X(a), H - B + 2, X(b), H - B + 14], fill=(46, 50, 58))
    for k in range(0, int(way.length) + 1, 500):
        d.line([X(k), H - B, X(k), H - B + 6], fill=(150, 150, 150))
        d.text((X(k) - 12, H - B + 18), "%d m" % k, fill=(170, 170, 170))
    for y in range(0, 4300, 500):
        d.line([L - 6, Y(y), L, Y(y)], fill=(150, 150, 150))
        d.text((4, Y(y) - 6), "%d" % y, fill=(170, 170, 170))
    d.line([(X(i * STEP), Y(p[1])) for i, p in enumerate(way.p)], fill=(210, 210, 210), width=2)
    for g in groups:
        x0, y0 = X(g["along"]), Y(g["y"])
        for j, k in enumerate(g["kinds"]):
            r = 3 + WEIGHT[k] * .6
            cy = y0 - 10 - j * 9
            d.ellipse([x0 - r, cy - r, x0 + r, cy + r], fill=COLOURS[k])
        if g["keep"] == "patrol":
            d.line([X(g["along"] - g["roam"]), y0 - 4, X(g["along"] + g["roam"]), y0 - 4], fill=(255, 255, 255))
    for g in caves:
        x0, y0 = X(g["along"]), Y(g["y"])
        d.rectangle([x0 - 5, y0 + 6, x0 + 5, y0 + 16], outline=(120, 170, 255), width=2)
        for j, k in enumerate(g["kinds"]):
            r = 3 + WEIGHT[k] * .6
            cy = y0 + 24 + j * 9
            d.ellipse([x0 - r, cy - r, x0 + r, cy + r], fill=COLOURS[k])
    for n in nests:
        x0, y0 = X(n["along"]), Y(n["y"])
        d.polygon([(x0, y0 - 30), (x0 - 9, y0 - 14), (x0 + 9, y0 - 14)], fill=(255, 205, 60))
        d.text((x0 - 20, y0 - 46), "nest %d" % n["station"], fill=(255, 205, 60))
    for b in bosses:
        x0, y0 = X(b["along"]), Y(b["y"])
        d.ellipse([x0 - 11, y0 - 11, x0 + 11, y0 + 11], outline=(255, 70, 60), width=3)
        d.text((x0 + 14, y0 + 8), "%s (%d m)" % (NAMES[b["kind"]], b["along"]), fill=(255, 110, 100))
    lx = L + 10
    d.text((lx, 14), "The Eagle's Ascent: nests (gold), bosses (red), groups on the way (above the line; a white bar: its "
                     "patrol), groups in caves (blue box, below). Blue bands: stretches only wings cross.", fill=(230, 230, 230))
    for k in KINDS:
        d.ellipse([lx, 36, lx + 10, 46], fill=COLOURS[k])
        d.text((lx + 14, 34), NAMES[k], fill=(220, 220, 220))
        lx += 110
    im.save(os.path.join(folder, "profile.png"))

    # ---- the map from above
    xs = [p[0] for p in way.p] + [b["x"] for b in bosses]
    zs = [p[2] for p in way.p] + [b["z"] for b in bosses]
    for g in caves:
        line = m["caves"][g["cave"]]["line"]
        xs += [q[0] for q in line]
        zs += [q[2] for q in line]
    x0, x1, z0, z1 = min(xs) - 40, max(xs) + 40, min(zs) - 40, max(zs) + 40
    scale = 1400.0 / (z1 - z0)
    MW, MH = int((x1 - x0) * scale) + 380, 1400 + 40
    P = lambda x, z: (20 + (x - x0) * scale, 20 + (z1 - z) * scale)
    im = Image.new("RGB", (MW, MH), (24, 27, 33))
    d = ImageDraw.Draw(im)
    for g in caves:
        line = m["caves"][g["cave"]]["line"]
        d.line([P(q[0], q[2]) for q in line], fill=(80, 120, 200), width=2)
    pts = [P(p[0], p[2]) for p in way.p]
    for i in range(len(pts) - 1):
        s = i * STEP
        d.line([pts[i], pts[i + 1]], fill=(70, 100, 140) if way.flying(s, 0) else (200, 200, 200), width=2)
    for g in groups + caves:
        cx, cy = P(g["x"], g["z"])
        k = max(g["kinds"], key=lambda q: WEIGHT[q])
        r = 3 + len(g["kinds"])
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=COLOURS[k])
    for n in nests:
        cx, cy = P(n["x"], n["z"])
        d.polygon([(cx, cy - 12), (cx - 8, cy + 5), (cx + 8, cy + 5)], fill=(255, 205, 60))
        d.text((cx + 10, cy - 6), "%d %s" % (n["station"], n["name"]), fill=(255, 205, 60))
    for b in bosses:
        cx, cy = P(b["x"], b["z"])
        d.ellipse([cx - 9, cy - 9, cx + 9, cy + 9], outline=(255, 70, 60), width=3)
        d.text((cx + 12, cy + 4), NAMES[b["kind"]], fill=(255, 110, 100))
    d.text((MW - 360, 20), "From above (north up). Grey: the way;", fill=(230, 230, 230))
    d.text((MW - 360, 36), "dark blue: where only wings cross;", fill=(230, 230, 230))
    d.text((MW - 360, 52), "blue lines: caves with a group.", fill=(230, 230, 230))
    im.save(os.path.join(folder, "map.png"))


def report(folder, m, nests, bosses, groups, caves, way):
    st = m["stations"]
    count = {k: 0 for k in KINDS}
    for g in groups + caves:
        for k in g["kinds"]:
            count[k] += 1
    lines = ["# The Eagle's Ascent: the placing model (written by art/map/ascent_creatures.py)", "",
             "Pictures: `profile.png` (the way from the shore to Amiran's court, its height, and everything on it) and "
             "`map.png` (from above).", "",
             "**%d groups on the way** (%d creatures) and **%d in caves** (%d creatures): %s. Four bosses. Nine Eagle Nests." % (
                 len(groups), sum(len(g["kinds"]) for g in groups), len(caves), sum(len(g["kinds"]) for g in caves),
                 ", ".join("%d %s" % (count[k], NAMES[k]) for k in KINDS)), "",
             "## Eagle Nests (the only saves)", "", "| Nest | Station | Metres along | Height |", "|---|---|---|---|"]
    for n in nests:
        lines.append("| %d | %s | %d | %d |" % (n["station"], n["name"], n["along"], n["y"]))
    lines += ["", "## Bosses", "", "| Boss | Metres along | Height | Keeps to (m along) |", "|---|---|---|---|"]
    for b in bosses:
        lines.append("| %s | %d | %d | %d to %d |" % (NAMES[b["kind"]], b["along"], b["y"], b["lo"], b["hi"]))
    lines += ["", "## Groups on the way", "", "| Id | Leg | Metres along | Height | Who | Keeps | Follows her (m along) |",
              "|---|---|---|---|---|---|---|"]
    for g in groups:
        lines.append("| %s | %d-%d | %d | %d | %s | %s | %d to %d |" % (
            g["id"], g["leg"], g["leg"] + 1, g["along"], g["y"], ", ".join(NAMES[k] for k in g["kinds"]),
            "walks %d m to and fro" % (2 * g["roam"]) if g["keep"] == "patrol" else "stands", g["lo"], g["hi"]))
    lines += ["", "## Groups in caves", "", "| Id | Beside the way at | Mouth off the way | The cave | Who | Keeps |",
              "|---|---|---|---|---|---|"]
    for g in caves:
        lines.append("| %s | %d m | %d m | %s, %d m | %s | %s |" % (
            g["id"], g["along"], g["off"], "ends in a chamber" if g["chamber"] else "goes right through", g["length"],
            ", ".join(NAMES[k] for k in g["kinds"]), "waits in its chamber" if g["keep"] == "fixed" else "walks its tunnel"))
    with open(os.path.join(folder, "PLACING.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", default=os.path.join(ROOT, "game", "assets", "Maps", "EaglesAscent.txt"))
    ap.add_argument("--out", default=os.path.join(ROOT, "game", "assets", "Maps", "EaglesAscent.creatures.txt"))
    ap.add_argument("--pictures", default="")
    a = ap.parse_args()
    m = read_map(a.map)
    nests, bosses, groups, caves, way = place(m)
    around, levels, ledger = economy(m, way, nests, bosses, groups, caves)
    write(a.out, nests, bosses, groups, caves, around, levels)
    alive = sum(len(g["kinds"]) for g in groups + caves + around)
    print("%d nests, %d bosses, %d groups on the way, %d in caves, %d around the way, %d creatures -> %s" % (
        len(nests), len(bosses), len(groups), len(caves), len(around), alive, a.out))
    print("level price | way: creatures bosses jars = main | off it: caves around(groups) jars = side | the way's part")
    for r in ledger:
        main = r["way_creatures"] + r["bosses"] + r["way_jars"]
        side = r["caves"] + r["around"] + r["side_jars"]
        print("%5d %5d | %9d %7d %4d = %4d | %12d %6d(%d) %5d = %4d | %3.0f %% of %d" % (
            r["level"], r["price"], r["way_creatures"], r["bosses"], r["way_jars"], main, r["caves"], r["around"],
            r["around_groups"], r["side_jars"], side, 100.0 * main / max(1, main + side), main + side))
    if a.pictures:
        pictures(a.pictures, m, nests, bosses, groups, caves, way)
        report(a.pictures, m, nests, bosses, groups, caves, way)
        print("pictures and PLACING.md -> " + a.pictures)


if __name__ == "__main__":
    main()
