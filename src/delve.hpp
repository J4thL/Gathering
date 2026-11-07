// delve.hpp - the generators of the Abandoned Mine (mountain biome) and the four levels of the Frozen Dungeon (snow biome).
//
// A "delve" is an instance map in the same tile / solid / props / nodes arrays as every other map (like a town interior), entered from a
// gate on the overworld and left again by the same way. `World::mapId` is DELVE_MAP0 + the DelveId while inside. A generator carves rooms and
// corridors into the rock (deterministic: the same seed always gives the same mine), then dresses them: timbers, rails, carts, torches,
// crystals, statues, pillars, ore deposits, chests, traps, and the spawn list for monsters (DSpawn). The game (delve.cpp) turns the
// spawn list into live monsters; difficulty grows with `depth` (0 at the entrance .. 1 at the deepest part).
#pragma once
#include <cstring>
#include <queue>

enum DelveId { DV_MINE, DV_FROST0, DV_FROST1, DV_FROST2, DV_FROST3, DV_COUNT };
constexpr int DELVE_MAP0 = B_COUNT + 8;  // keep clear of the town interiors (B_COUNT + BuildingKind)
inline bool delveIsFrost(int id) { return id >= DV_FROST0; }
inline const char* const DELVE_NAME[DV_COUNT] = {"ABANDONED MINE", "FROZEN ENTRANCE", "FROZEN HALLS", "ANCIENT CRYPT", "FROZEN THRONE"};

enum TrapKind { TR_SPIKE, TR_VENT };
constexpr int DELVE_BOSS_CHEST = 31;  // chest id of the king's treasure (appears once he has fallen)

inline void World::generateDelve(int id) {
    if (id < 0 || id >= DV_COUNT) id = 0;
    mapId = DELVE_MAP0 + id;
    const bool frost = delveIsFrost(id);
    const int lvl = id - DV_FROST0;  // 0..3 for the dungeon
    props.clear();
    decor.clear();
    nodes.clear();
    landmarks.clear();
    lanterns.clear();
    npcs.clear();
    stations.clear();
    dsp.clear();
    lights.clear();
    memset(solid, 0, sizeof(solid));
    memset(biome, frost ? B_SNOW : B_MOUNTAIN, sizeof(biome));
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) tile[y][x] = T_VOID;
    const V2 away(-99999, -99999);
    village = hut = door = keeper = fire = questGiver = away;
    gate = stairsDn = stairsUp = bossPos = bossDoor = V2(-1, -1);
    Rng r(WORLD_SEED * 131u + 977u + (uint32_t)id * 7919u);

    static bool open[MH][MW], corr[MH][MW], railc[MH][MW], busy[MH][MW];  // static: too big for the stack
    memset(open, 0, sizeof(open));
    memset(corr, 0, sizeof(corr));
    memset(railc, 0, sizeof(railc));
    memset(busy, 0, sizeof(busy));

    struct Room {
        int x, y, w, h;
        float depth = 0;
        int kind = 0;  // 0 normal, 1 start, 2 end, 3 treasure, 4 locked treasure
        int cx() const { return x + w / 2; }
        int cy() const { return y + h / 2; }
    };
    std::vector<Room> rooms;
    struct Leg {  // a straight corridor piece, remembered for beams and rails
        int x0, y0, x1, y1;
        bool vertical;
    };
    std::vector<Leg> legs;
    auto carve = [&](int x0, int y0, int x1, int y1, bool isCorr) {
        for (int y = std::max(4, y0); y <= std::min(MH - 5, y1); y++)
            for (int x = std::max(4, x0); x <= std::min(MW - 5, x1); x++) {
                open[y][x] = true;
                if (isCorr) corr[y][x] = true;
            }
    };
    auto roomAt = [&](const Room& R) { carve(R.x, R.y, R.x + R.w - 1, R.y + R.h - 1, false); };
    // a corridor `cw` tiles wide between two room centres: horizontal first, then vertical
    auto connect = [&](const Room& A, const Room& B, int cw) {
        const int h0 = cw / 2, h1 = cw - 1 - cw / 2;
        const int ax = A.cx(), ay = A.cy(), bx = B.cx(), by = B.cy();
        carve(std::min(ax, bx), ay - h0, std::max(ax, bx), ay + h1, true);
        carve(bx - h0, std::min(ay, by), bx + h1, std::max(ay, by), true);
        legs.push_back({std::min(ax, bx), ay, std::max(ax, bx), ay, false});
        legs.push_back({bx, std::min(ay, by), bx, std::max(ay, by), true});
    };
    // random room-and-corridor layout; room 0 is the entrance at the left, the last room the deepest
    auto layout = [&](int want, int wMin, int wMax, int hMin, int hMax, int cw, int extra) {
        rooms.push_back({8, 76 - hMax / 2, wMax - 2, hMax - 1, 0, 1});
        for (int tries = 0; tries < 900 && (int)rooms.size() < want; tries++) {
            Room R;
            R.w = r.irange(wMin, wMax);
            R.h = r.irange(hMin, hMax);
            R.x = r.irange(8, MW - 8 - R.w);
            R.y = r.irange(8, MH - 8 - R.h);
            bool ok = true;
            for (auto& o : rooms)
                if (R.x < o.x + o.w + 4 && R.x + R.w + 4 > o.x && R.y < o.y + o.h + 4 && R.y + R.h + 4 > o.y) { ok = false; break; }
            if (ok) rooms.push_back(R);
        }
        // Prim: each room joins its nearest room that is already in the tree
        std::vector<int> inTree{0};
        std::vector<std::pair<int, int>> edges;
        std::vector<bool> done(rooms.size(), false);
        done[0] = true;
        while (inTree.size() < rooms.size()) {
            int bi = -1, bj = -1, bd = 1 << 30;
            for (int i : inTree)
                for (size_t j = 0; j < rooms.size(); j++) {
                    if (done[j]) continue;
                    const int d = abs(rooms[i].cx() - rooms[j].cx()) + abs(rooms[i].cy() - rooms[j].cy());
                    if (d < bd) bd = d, bi = i, bj = (int)j;
                }
            done[bj] = true;
            inTree.push_back(bj);
            edges.push_back({bi, bj});
        }
        for (int k = 0; k < extra && rooms.size() > 3; k++) {  // a few loops, so the place is not one long tube
            const int a = r.irange(0, (int)rooms.size() - 1), b = r.irange(0, (int)rooms.size() - 1);
            if (a != b && abs(rooms[a].cx() - rooms[b].cx()) + abs(rooms[a].cy() - rooms[b].cy()) < 90) edges.push_back({a, b});
        }
        for (auto& R : rooms) roomAt(R);
        for (auto& e : edges) connect(rooms[e.first], rooms[e.second], cw);
        // depth = walking distance from the entrance (over the room graph), 0..1; the deepest room is the end
        std::vector<float> dist(rooms.size(), 1e9f);
        dist[0] = 0;
        for (int pass = 0; pass < (int)rooms.size(); pass++)
            for (auto& e : edges) {
                const float d = (float)(abs(rooms[e.first].cx() - rooms[e.second].cx()) + abs(rooms[e.first].cy() - rooms[e.second].cy()));
                if (dist[e.first] + d < dist[e.second]) dist[e.second] = dist[e.first] + d;
                if (dist[e.second] + d < dist[e.first]) dist[e.first] = dist[e.second] + d;
            }
        float mx = 1;
        int deep = 0;
        for (size_t i = 0; i < rooms.size(); i++)
            if (dist[i] > mx) mx = dist[i], deep = (int)i;
        for (size_t i = 0; i < rooms.size(); i++) rooms[i].depth = std::min(1.f, dist[i] / mx);
        if (deep > 0) rooms[deep].kind = 2;
    };

    // ---------------------------------------------------------------- 1. the rooms
    if (!frost) layout(15, 7, 15, 6, 11, 3, 4);
    else if (lvl == 0) layout(9, 11, 17, 8, 12, 3, 2);
    else if (lvl == 1) layout(10, 12, 19, 9, 13, 3, 3);
    else if (lvl == 2) layout(14, 6, 10, 5, 8, 2, 3);
    else {  // the throne level is hand-made: the stairs hall, the antechamber, the door and the throne hall
        rooms.push_back({14, 100, 18, 12, 0.0f, 1});
        rooms.push_back({52, 100, 26, 20, 0.35f, 0});
        rooms.push_back({44, 58, 42, 30, 1.0f, 2});
        for (auto& R : rooms) roomAt(R);
        carve(32, 105, 51, 107, true);
        legs.push_back({32, 106, 51, 106, false});
        carve(64, 88, 66, 99, true);
        legs.push_back({65, 88, 65, 99, true});
    }

    // ---------------------------------------------------------------- 2. floor, walls, solid rock
    const uint8_t floorT = !frost ? T_CFLOOR : (lvl == 2 ? T_BRICK : T_IFLOOR);
    const uint8_t wallT = frost ? T_IWALL : T_CWALL;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if (open[y][x]) tile[y][x] = floorT;
    for (int y = 2; y < MH - 2; y++)
        for (int x = 2; x < MW - 2; x++) {
            if (open[y][x]) continue;
            bool near = false;
            for (int dy = -2; dy <= 2 && !near; dy++)
                for (int dx = -2; dx <= 2; dx++)
                    if (open[y + dy][x + dx]) { near = true; break; }
            if (near) tile[y][x] = wallT;
        }
    auto isFloor = [&](int x, int y) { return x >= 0 && y >= 0 && x < MW && y < MH && open[y][x]; };
    auto freeTile = [&](int x, int y) { return isFloor(x, y) && !busy[y][x] && !solid[y][x]; };
    auto cx16 = [&](int x) { return x * TS + 8.f; };
    auto prop = [&](int kind, int tx, int ty, float ph, bool sol, float ox = 8.f, float oy = 15.f) {
        props.push_back({kind, tx * TS + ox, ty * TS + oy, ph});
        busy[ty][tx] = true;
        if (sol) solid[ty][tx] = true;
    };
    auto light = [&](float x, float y, float rad, int tone) { lights.push_back({x, y, rad, tone}); };
    auto nearCorr = [&](int x, int y, int d) {
        for (int dy = -d; dy <= d; dy++)
            for (int dx = -d; dx <= d; dx++)
                if (x + dx >= 0 && y + dy >= 0 && x + dx < MW && y + dy < MH && corr[y + dy][x + dx] && !(x + dx >= 0 && false)) return true;
        return false;
    };
    // tiles along the inside of a room's border (where things stand against the wall, away from the corridor mouths)
    auto edgeSpot = [&](const Room& R, int tries, int clear) -> std::pair<int, int> {
        for (int t = 0; t < tries; t++) {
            int x, y;
            switch (r.irange(0, 3)) {
                case 0: x = R.x + r.irange(0, R.w - 1); y = R.y; break;
                case 1: x = R.x + r.irange(0, R.w - 1); y = R.y + R.h - 1; break;
                case 2: x = R.x; y = R.y + r.irange(0, R.h - 1); break;
                default: x = R.x + R.w - 1; y = R.y + r.irange(0, R.h - 1); break;
            }
            if (!freeTile(x, y) || nearCorr(x, y, clear)) continue;
            return {x, y};
        }
        return {-1, -1};
    };
    auto inner = [&](const Room& R, int margin) -> std::pair<int, int> {
        for (int t = 0; t < 30; t++) {
            const int x = R.x + margin + r.irange(0, std::max(0, R.w - 1 - 2 * margin)), y = R.y + margin + r.irange(0, std::max(0, R.h - 1 - 2 * margin));
            if (freeTile(x, y)) return {x, y};
        }
        return {-1, -1};
    };
    auto node = [&](int kind, int tx, int ty) {
        nodes.push_back({kind, tx * TS + 8.f, ty * TS + 14.f, r.f() * 6.28f});
        solid[ty][tx] = true;
        busy[ty][tx] = true;
    };
    auto monster = [&](int kind, const Room& R, float depth, bool elite = false) {
        auto s = inner(R, 1);
        if (s.first < 0) return;
        dsp.push_back({DS_MONSTER, kind, cx16(s.first), s.second * TS + 10.f, (int)(&R - &rooms[0]), depth, elite});
    };
    const float lvBase = frost ? lvl * 0.25f : 0.f, lvSpan = frost ? 0.25f : 1.f;
    auto gdepth = [&](const Room& R) { return std::min(1.f, lvBase + R.depth * lvSpan); };

    // ---------------------------------------------------------------- 3. the entrance and the stairs
    Room& R0 = rooms[0];
    const int ex = R0.x, ey = R0.cy();
    if (lvl == 3 && frost) {}  // (the throne level has its own entrance: the stairs up)
    if (!frost || lvl == 0) {
        // the way out: a doorway on the west wall of the first room
        prop(P_EXIT, ex, ey, frost ? 1.f : 0.f, false, 8.f, 14.f);
        exitPos = V2(cx16(ex) + 14.f, ey * TS + 10.f);
        spawn = V2(cx16(ex + 2), ey * TS + 10.f);
        light(cx16(ex), ey * TS, 90, 0);
    } else {
        stairsUp = V2(cx16(R0.x + 1), R0.cy() * TS + 10.f);
        prop(P_STAIRS, R0.x + 1, R0.cy(), 1.f, false, 8.f, 14.f);
        spawn = V2(cx16(R0.x + 3), R0.cy() * TS + 10.f);
        exitPos = stairsUp;
        light(stairsUp.x, stairsUp.y, 80, 1);
    }
    roomCenter = V2(R0.cx() * (float)TS, R0.cy() * (float)TS);
    busy[ey][ex] = true;
    for (int dy = -1; dy <= 1; dy++) for (int dx = 0; dx <= 3; dx++) busy[ey + dy][ex + dx] = true;  // keep the arrival clear
    if (frost && lvl < 3) {  // stairs down in the deepest room
        for (auto& R : rooms)
            if (R.kind == 2) {
                stairsDn = V2(cx16(R.cx()), R.cy() * TS + 10.f);
                prop(P_STAIRS, R.cx(), R.cy(), 0.f, false, 8.f, 14.f);
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) busy[R.cy() + dy][R.cx() + dx] = true;
                light(stairsDn.x, stairsDn.y, 90, 1);
            }
    }

    // ---------------------------------------------------------------- 4. corridors: timbers, rails, torches
    if (!frost) {
        for (auto& L : legs) {
            const int len = L.vertical ? L.y1 - L.y0 : L.x1 - L.x0;
            for (int i = 4; i < len - 3; i += 7) {
                if (L.vertical) prop(P_BEAM, L.x0, L.y0 + i, 0.f, false, 8.f, 15.f);
                else prop(P_BEAM, L.x0 + i, L.y0, 1.f, false, 8.f, 30.f);
            }
        }
        // a track from the entrance along the first corridors, with an abandoned cart or two
        for (size_t k = 0; k < legs.size() && k < 6; k++) {
            const Leg& L = legs[k];
            for (int y = L.y0; y <= L.y1; y++)
                for (int x = L.x0; x <= L.x1; x++)
                    if (corr[y][x] && open[y][x]) railc[y][x] = true;
        }
        for (int x = R0.x + 2; x < R0.x + R0.w; x++) railc[R0.cy()][x] = true;
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++)
                if (railc[y][x] && open[y][x] && tile[y][x] == T_CFLOOR) tile[y][x] = T_RAIL;
        int carts = 0;
        for (int tries = 0; tries < 400 && carts < 5; tries++) {
            const int x = r.irange(6, MW - 6), y = r.irange(6, MH - 6);
            if (tile[y][x] == T_RAIL && freeTile(x, y) && (x > R0.x + R0.w + 2) && !isFloor(x - 1, y - 1) == false) {
                // only on rail tiles whose both side tiles stay open (a corridor three wide keeps a gap beside the cart)
                if (isFloor(x, y - 1) && isFloor(x, y + 1) && isFloor(x - 1, y) && isFloor(x + 1, y)) { prop(P_MINECART, x, y, 0.f, true); carts++; }
            }
        }
    }
    // torches on the north walls (a wall face above a floor tile), spaced out
    {
        int since = 0;
        for (int y = 5; y < MH - 5; y++)
            for (int x = 5; x < MW - 5; x++) {
                if (!open[y][x] || open[y - 1][x] || busy[y][x]) continue;
                if (++since < 11 + (int)(hash2(x, y, 5) % 6)) continue;
                since = 0;
                if (!frost) {
                    prop(P_TORCH, x, y, 0.f, false, 8.f, 6.f);
                    light(cx16(x), y * TS - 6.f, 74, 0);
                } else if (lvl != 2 || hash2(x, y, 9) % 3 == 0) {
                    prop(P_BRAZIER, x, y, (float)(hash2(x, y, 3) % 3), true, 8.f, 14.f);
                    light(cx16(x), y * TS, 84, 1);
                } else {
                    prop(P_TORCH, x, y, 0.f, false, 8.f, 6.f);
                    light(cx16(x), y * TS - 6.f, 60, 1);
                }
            }
    }

    // ---------------------------------------------------------------- 5. the rooms
    struct W { int kind; float w; float minDepth; };
    for (size_t ri = 1; ri < rooms.size(); ri++) {
        Room& R = rooms[ri];
        if (frost && lvl == 3) break;
        const float d = R.depth;
        const int area = R.w * R.h;
        if (!frost) {
            // monsters: shallow = bats and spiders, deeper = wolves, the deepest = rock golems
            const int n = std::max(1, area / 45 + (d > 0.5f ? 1 : 0));
            for (int i = 0; i < n; i++) {
                const float roll = r.f();
                int k;
                if (d < 0.3f) k = roll < 0.55f ? MK_BAT : MK_SPIDER;
                else if (d < 0.6f) k = roll < 0.3f ? MK_BAT : roll < 0.65f ? MK_SPIDER : roll < 0.9f ? MK_CAVEWOLF : MK_GOLEM;
                else k = roll < 0.15f ? MK_BAT : roll < 0.4f ? MK_SPIDER : roll < 0.7f ? MK_CAVEWOLF : MK_GOLEM;
                monster(k, R, d);
            }
            if (R.kind == 2) { monster(MK_GOLEM, R, 1.f, true); monster(MK_GOLEM, R, 1.f); }
            if (R.kind == 2) {  // the deepest chamber always holds a little of everything rare
                for (int kind : {RN_GEM, RN_GEM, RN_CAVECRYSTAL, RN_GOLD, RN_SILVER}) { auto s = edgeSpot(R, 30, 2); if (s.first >= 0) node(kind, s.first, s.second); }
            }
            if (R.kind != 2 && r.chance(0.35f)) {  // an old miner's chest here and there
                auto s = edgeSpot(R, 30, 2);
                if (s.first >= 0) { dsp.push_back({DS_CHEST, d > 0.5f ? 1 : 0, cx16(s.first), s.second * TS + 14.f, (int)ri, d, false}); busy[s.second][s.first] = true; }
            }
            // ore: rarer metals lie deeper and are less common; every deposit sits against a wall, away from the corridors
            const W table[7] = {{RN_COAL, 30, 0.f}, {RN_COPPER, 26, 0.f}, {RN_ORE, 20, 0.15f}, {RN_SILVER, 11, 0.35f}, {RN_GOLD, 8, 0.5f}, {RN_CAVECRYSTAL, 6, 0.55f}, {RN_GEM, 3, 0.75f}};
            const int nOre = 2 + area / 24 + (R.kind == 2 ? 3 : 0);
            for (int i = 0; i < nOre; i++) {
                float tot = 0;
                for (auto& w : table) if (d >= w.minDepth) tot += w.w;
                float pick = r.f() * tot;
                int kind = RN_COAL;
                for (auto& w : table) {
                    if (d < w.minDepth) continue;
                    if (pick < w.w) { kind = w.kind; break; }
                    pick -= w.w;
                }
                auto s = edgeSpot(R, 20, 2);
                if (s.first >= 0) node(kind, s.first, s.second);
            }
            // glowing cave crystals (scenery), equipment and debris
            for (int i = 0; i < 1 + (d > 0.4f ? 2 : 0); i++) {
                auto s = edgeSpot(R, 20, 2);
                if (s.first < 0) continue;
                prop(P_CAVECRYSTAL, s.first, s.second, (float)(r.irange(0, 2)), true);
                light(cx16(s.first), s.second * TS, 52, 2);
            }
            for (int i = 0; i < 2 + area / 40; i++) {
                auto s = edgeSpot(R, 20, 2);
                if (s.first < 0) continue;
                const int pk = r.irange(0, 5);
                if (pk == 0) prop(P_BARREL, s.first, s.second, 0.f, true);
                else if (pk == 1) prop(P_CRATE, s.first, s.second, 0.f, true);
                else if (pk == 2) prop(P_EQUIP, s.first, s.second, (float)r.irange(0, 3), true);
                else { auto q = inner(R, 1); if (q.first >= 0) { props.push_back({P_DEBRIS, cx16(q.first), q.second * TS + 12.f, (float)r.irange(0, 2)}); busy[q.second][q.first] = true; } }
            }
        } else {
            // ---- the Frozen Dungeon: what lives and lies in a room depends on the level
            const int nm = std::max(1, area / (lvl == 2 ? 28 : 50));
            for (int i = 0; i < nm; i++) {
                const float roll = r.f();
                int k;
                bool elite = false;
                if (lvl == 0) k = roll < 0.6f ? MK_ICESKEL : MK_FROSTWOLF;
                else if (lvl == 1) k = roll < 0.3f ? MK_ICESKEL : roll < 0.5f ? MK_FROSTWOLF : roll < 0.75f ? MK_ICEGOLEM : MK_MAGE;
                else { k = roll < 0.3f ? MK_KNIGHT : roll < 0.55f ? MK_MAGE : roll < 0.8f ? MK_ICESKEL : MK_ICEGOLEM; elite = r.chance(0.3f); }
                monster(k, R, gdepth(R), elite);
            }
            if (lvl == 2 && R.kind == 2) { monster(MK_KNIGHT, R, gdepth(R), true); monster(MK_KNIGHT, R, gdepth(R), true); }
            // pillars in pairs, statues, crystals, icicles, bones
            if (R.w >= 10 && R.h >= 8 && lvl < 2) {
                for (int x = R.x + 2; x < R.x + R.w - 2; x += 4) {
                    if (freeTile(x, R.y + 2) && !nearCorr(x, R.y + 2, 1)) prop(P_PILLAR, x, R.y + 2, 0.f, true, 8.f, 15.f);
                    if (freeTile(x, R.y + R.h - 3) && !nearCorr(x, R.y + R.h - 3, 1)) prop(P_PILLAR, x, R.y + R.h - 3, 0.f, true, 8.f, 15.f);
                }
            }
            for (int i = 0; i < 1 + area / 60; i++) {
                auto s = edgeSpot(R, 20, 2);
                if (s.first < 0) continue;
                const int pk = r.irange(0, 5);
                if (pk <= 1) { prop(P_STATUE, s.first, s.second, (float)r.irange(0, 2), true, 8.f, 15.f); }
                else if (pk == 2) { prop(P_ICECRYSTAL, s.first, s.second, (float)r.irange(0, 2), true); light(cx16(s.first), s.second * TS, 58, 1); }
                else if (pk == 3) { prop(P_BANNER, s.first, s.second, 0.f, false, 8.f, 6.f); }
                else if (pk == 4 && lvl == 2) { prop(P_SARCO, s.first, s.second, 0.f, true); }
                else { auto q = inner(R, 1); if (q.first >= 0) { props.push_back({P_BONES, cx16(q.first), q.second * TS + 10.f, (float)r.irange(0, 1)}); busy[q.second][q.first] = true; } }
            }
            // traps: spike plates and frost vents on the floor of halls and crypt corridors (levels 2 and 3 of the dungeon)
            if (lvl >= 1) {
                for (int i = 0; i < 2 + area / 30; i++) {
                    auto s = inner(R, 1);
                    if (s.first < 0 || nearCorr(s.first, s.second, 0)) continue;
                    dsp.push_back({DS_TRAP, r.chance(0.55f) ? TR_SPIKE : TR_VENT, cx16(s.first), s.second * TS + 8.f, 0, gdepth(R), false});
                    busy[s.second][s.first] = true;
                }
            }
            // eternal ice in the crypt
            if (lvl == 2) {
                const int nI = R.kind == 2 ? 3 : (r.chance(0.35f) ? 1 : 0);
                for (int i = 0; i < nI; i++) { auto s = edgeSpot(R, 20, 2); if (s.first >= 0) node(RN_ETERNALICE, s.first, s.second); }
            }
            // chests: basic ones on the first level, better below, locked treasure rooms in the crypt
            const int tier = lvl == 0 ? 0 : (lvl == 1 ? 1 : 2);
            const bool wantChest = lvl == 2 ? R.kind == 3 || R.kind == 2 : r.chance(0.42f) || R.kind == 2;
            if (wantChest) {
                auto s = edgeSpot(R, 30, 2);
                if (s.first >= 0) { dsp.push_back({DS_CHEST, tier, cx16(s.first), s.second * TS + 14.f, (int)ri, gdepth(R), false}); busy[s.second][s.first] = true; }
            }
        }
        if (R.kind == 2 && !frost) {  // the deepest part of the mine: a richer vein and a chest of old pay
            auto s = inner(R, 2);
            if (s.first >= 0) { dsp.push_back({DS_CHEST, 1, cx16(s.first), s.second * TS + 14.f, (int)ri, 1.f, false}); busy[s.second][s.first] = true; }
        }
    }
    // the mine's first room: a little equipment lying around, so the way in feels used
    if (!frost) {
        for (int i = 0; i < 5; i++) {
            auto s = edgeSpot(R0, 20, 2);
            if (s.first < 0) continue;
            const int pk = r.irange(0, 3);
            prop(pk == 0 ? P_BARREL : pk == 1 ? P_CRATE : P_EQUIP, s.first, s.second, (float)r.irange(0, 3), true);
        }
        for (int i = 0; i < 3; i++) { auto q = inner(R0, 1); if (q.first >= 0) { props.push_back({P_DEBRIS, cx16(q.first), q.second * TS + 12.f, (float)r.irange(0, 2)}); busy[q.second][q.first] = true; } }
        // resources close to the entrance so a new miner starts earning at once
        for (int i = 0; i < 3; i++) { auto s = edgeSpot(R0, 30, 2); if (s.first >= 0) node(i == 2 ? RN_ORE : (i ? RN_COPPER : RN_COAL), s.first, s.second); }
    }

    // ---------------------------------------------------------------- 6. dungeon level specials
    if (frost && lvl == 2) {  // two locked treasure rooms: dead ends off the crypt, each with a chest that needs a crypt key
        int made = 0;
        for (size_t ri = rooms.size(); ri-- > 1 && made < 2;)
            if (rooms[ri].kind == 0) { rooms[ri].kind = 3; made++; }
        for (auto& R : rooms) {
            if (R.kind != 3) continue;
            auto s = inner(R, 1);
            if (s.first >= 0) {
                dsp.push_back({DS_CHEST, 2, cx16(R.cx()), R.cy() * TS + 14.f, (int)(&R - &rooms[0]), gdepth(R), true});
                busy[R.cy()][R.cx()] = true;
            }
        }
    }
    if (frost && lvl == 3) {
        const Room& B = rooms[1];  // the antechamber: a carpet strip between two rows of pillars and braziers
        for (int y = B.y + 1; y < B.y + B.h - 1; y++) for (int x = B.cx() - 2; x <= B.cx() + 2; x++) tile[y][x] = T_CARPET;
        for (int y = B.y + 3; y < B.y + B.h - 2; y += 5) {
            prop(P_PILLAR, B.cx() - 4, y, 0.f, true, 8.f, 15.f);
            prop(P_PILLAR, B.cx() + 4, y, 0.f, true, 8.f, 15.f);
            prop(P_BRAZIER, B.cx() - 6, y, (float)(y % 3), true, 8.f, 14.f); light(cx16(B.cx() - 6), y * TS, 90, 1);
            prop(P_BRAZIER, B.cx() + 6, y, (float)((y + 1) % 3), true, 8.f, 14.f); light(cx16(B.cx() + 6), y * TS, 90, 1);
        }
        for (int i = 0; i < 4; i++) monster(i % 2 ? MK_KNIGHT : MK_MAGE, B, 0.8f, i == 0);
        monster(MK_ICEGOLEM, B, 0.85f);
        // the door: three tiles wide in the vertical corridor; solid until the hunter opens it (delve.cpp)
        bossDoor = V2(cx16(65), 92 * TS + 15.f);
        prop(P_BOSSDOOR, 65, 92, 0.f, false, 8.f, 15.f);
        for (int x = 64; x <= 66; x++) solid[92][x] = true;
        // the throne hall: carpet runs from the door to the throne; pillars and braziers frame it
        const Room& C = rooms[2];
        for (int y = C.y + 1; y < C.y + C.h; y++) for (int x = 63; x <= 67; x++) tile[y][x] = T_CARPET;
        for (int y = C.y + 8; y < C.y + C.h - 1; y += 6) { prop(P_PILLAR, 56, y, 0.f, true, 8.f, 15.f); prop(P_PILLAR, 74, y, 0.f, true, 8.f, 15.f); }
        for (int i = 0; i < 4; i++) {
            const int bx = i % 2 ? 72 : 58, by = C.y + 6 + (i / 2) * 12;
            prop(P_BRAZIER, bx, by, (float)i, true, 8.f, 14.f);
            light(cx16(bx), by * TS, 100, 1);
        }
        prop(P_THRONE, 65, C.y + 3, 0.f, false, 8.f, 15.f);
        for (int x = 64; x <= 66; x++) for (int y = C.y + 1; y <= C.y + 3; y++) solid[y][x] = true;
        for (int x : {52, 78}) { prop(P_STATUE, x, C.y + 2, (float)(x > 65 ? 2 : 0), true, 8.f, 15.f); prop(P_BANNER, x + (x > 65 ? -2 : 2), C.y, 0.f, false, 8.f, 6.f); }
        light(cx16(65), (C.y + 3) * TS, 140, 1);
        bossPos = V2(cx16(65), (C.y + 12) * TS);
        dsp.push_back({DS_BOSS, MK_KING, bossPos.x, bossPos.y, 0, 1.f, true});
        dsp.push_back({DS_CHEST, 3, cx16(65), (C.y + 6) * TS + 14.f, DELVE_BOSS_CHEST, 1.f, false});
        // ice crystals along the throne hall's walls
        for (int i = 0; i < 6; i++) { auto s = edgeSpot(C, 30, 1); if (s.first >= 0) { prop(P_ICECRYSTAL, s.first, s.second, (float)r.irange(0, 2), true); light(cx16(s.first), s.second * TS, 60, 1); } }
    }

    // ---------------------------------------------------------------- 7. camera anchor and a clean-up of the props list
    std::sort(props.begin(), props.end(), [](const Prop& a, const Prop& b) { return a.y < b.y; });
    std::sort(decor.begin(), decor.end(), [](const Decor& a, const Decor& b) { return a.y < b.y; });
    std::sort(nodes.begin(), nodes.end(), [](const ResNode& a, const ResNode& b) { return a.y < b.y; });
}
