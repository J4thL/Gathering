// world_check.cpp - `wildwood --worldcheck`: every map, gate and landmark is reachable, terrain and sprint behave (moved out of main.cpp).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <string>
#include "game.hpp"

struct Reach {
    static constexpr int CELL = 8;
    int gw, gh;
    std::vector<int> d;  // distance in cells from the start, -1 = unreachable
    Reach(const World& w, V2 start, float rad = 4.f) {
        gw = (int)(w.pixW() / CELL);
        gh = (int)(w.pixH() / CELL);
        d.assign((size_t)gw * gh, -1);
        auto pass = [&](int cx, int cy) { return cx >= 0 && cy >= 0 && cx < gw && cy < gh && !w.blocked(cx * CELL + CELL / 2.f, cy * CELL + CELL / 2.f, rad); };
        int sx = (int)(start.x / CELL), sy = (int)(start.y / CELL);
        if (!pass(sx, sy)) return;
        std::queue<std::pair<int, int>> q;
        q.push({sx, sy});
        d[(size_t)sy * gw + sx] = 0;
        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (!dx && !dy) continue;
                    int nx = x + dx, ny = y + dy;
                    if (!pass(nx, ny) || d[(size_t)ny * gw + nx] >= 0) continue;
                    if (dx && dy && (!pass(x + dx, y) || !pass(x, y + dy))) continue;  // no corner cutting
                    d[(size_t)ny * gw + nx] = d[(size_t)y * gw + x] + 1;
                    q.push({nx, ny});
                }
        }
    }
    int at(float px, float py) const {
        int cx = (int)(px / CELL), cy = (int)(py / CELL);
        if (cx < 0 || cy < 0 || cx >= gw || cy >= gh) return -1;
        return d[(size_t)cy * gw + cx];
    }
    bool nearReach(float px, float py, int rad = 3) const {
        for (int dy = -rad; dy <= rad; dy++)
            for (int dx = -rad; dx <= rad; dx++)
                if (at(px + dx * CELL, py + dy * CELL) >= 0) return true;
        return false;
    }
};

// start a fresh game and stand at the entrance of map `b` (the home map is where a new game starts)
void enterMap(Game& g, int b) {
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    if (b != B_HOME) g.travelTo(b, MAPS[b].entryTx, MAPS[b].entryTy, false);
    g.travelCd = g.travelFade = 0;
    g.animals.clear();
    g.respawnT = 1e9f;
}

// drive the real player through update() along the shortest route to `target` (pixels) in the active map, until the map changes or
// the target is reached. Returns the number of frames used (-1 = never arrived).
static int driveTo(Game& g, V2 target, int stopIfMapChanges, int maxFrames = 60 * 240) {
    const World& w = g.world;
    Reach back(w, target, 6.f);  // planned with a fatter body than the real one (4px) so steering can't clip corners
    const int startMap = w.mapId;
    for (int frames = 0; frames < maxFrames; frames++) {
        int cx = (int)(g.p.pos.x / 8), cy = (int)(g.p.pos.y / 8);
        int cur = back.at(g.p.pos.x, g.p.pos.y);
        if (cur >= 0 && cur <= 2 && !stopIfMapChanges) return frames;
        int bx = cx, by = cy, bd = cur < 0 ? 1 << 30 : cur;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= back.gw || ny >= back.gh) continue;
                int nd = back.d[(size_t)ny * back.gw + nx];
                if (dx && dy && (back.d[(size_t)cy * back.gw + cx + dx] < 0 || back.d[(size_t)(cy + dy) * back.gw + cx] < 0)) continue;  // no corner cutting
                if (nd >= 0 && nd < bd) bd = nd, bx = nx, by = ny;
            }
        V2 tgt(bx * 8.f + 4, by * 8.f + 4), dv = tgt - g.p.pos;
        g.in.left = dv.x < -0.3f, g.in.right = dv.x > 0.3f, g.in.up = dv.y < -0.3f, g.in.down = dv.y > 0.3f;
        g.in.sprint = false;
        g.p.stamina = 100;
        g.p.hp = g.p.maxhp;  // this test is about terrain, not about wolves
        g.animals.clear();
        g.respawnT = 1e9f;
        g.update(1.f / 60.f);
        if (g.world.mapId != startMap) {
            g.in.left = g.in.right = g.in.up = g.in.down = false;
            return frames;
        }
    }
    g.in.left = g.in.right = g.in.up = g.in.down = false;
    return -1;
}

// the open tile of a gate's zone nearest its middle, as a pixel position (where a walker aims to go through the gate)
V2 portalPoint(const World& w, const Portal& pt) {
    const float mx = (pt.zone.x0 + pt.zone.x1) * 0.5f, my = (pt.zone.y0 + pt.zone.y1) * 0.5f;
    V2 best((mx + 0.5f) * TS, (my + 0.5f) * TS);
    float bd = 1e9f;
    for (int y = pt.zone.y0; y <= pt.zone.y1; y++)
        for (int x = pt.zone.x0; x <= pt.zone.x1; x++)
            if (!w.solidTile(x, y) && hypotf(x - mx, y - my) < bd) bd = hypotf(x - mx, y - my), best = V2(x * TS + 8.f, y * TS + 8.f);
    return best;
}

// Checks one map: size, biome purity, every landmark / resource / hub / gate reachable from where you arrive, edge wall closed except at the gates.
static int mapCheck(Game& g, int map, long nodeTotals[]) {
    int bad = 0;
    enterMap(g, map);
    World& w = g.world;
    const MapDef& MD = MAPS[map];
    const V2 arrive = g.p.pos;
    Reach rc(w, arrive);
    printf("%s map: %dx%d tiles (%dx%d px), mapId %d, props=%zu decor=%zu nodes=%zu landmarks=%zu\n", MD.name, MW, MH, (int)w.pixW(), (int)w.pixH(), w.mapId, w.props.size(), w.decor.size(), w.nodes.size(), w.landmarks.size());
    if (MW != 240 || MH != 168 || w.mapId != map) {
        printf("  PROBLEM: map size is not 240x168\n");
        bad++;
    }
    // purity: every tile belongs to this map's biome; the home map has only its own four ground types
    long foreign = 0, wrongGround = 0;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            if (w.biome[y][x] != map) foreign++;
            const uint8_t t = w.tile[y][x];
            if (map == B_HOME && !(t == T_GRASS || t == T_DARK || t == T_DIRT || t == T_WATER || t == T_COBBLE)) wrongGround++;
        }
    printf("  tiles of another biome: %ld%s   foreign ground in home: %ld\n", foreign, foreign ? "  <-- PROBLEM" : "", wrongGround);
    if (foreign || wrongGround) bad++;
    // reach
    long total = 0, reach = 0;
    for (int cy = 0; cy < rc.gh; cy++)
        for (int cx = 0; cx < rc.gw; cx++)
            if (!w.blocked(cx * 8.f + 4, cy * 8.f + 4, 4.f)) {
                total++;
                if (rc.d[(size_t)cy * rc.gw + cx] >= 0) reach++;
            }
    printf("  walkable cells %ld, reachable from the arrival point %ld (%.1f%%)\n", total, reach, total ? 100.0 * reach / total : 0.0);
    {
        const int d = rc.at(w.hub[map].x, w.hub[map].y);
        printf("  hub reachable: %s  path length %d px\n", d >= 0 ? "yes" : "NO", d >= 0 ? d * Reach::CELL : -1);
        if (d < 0) bad++;
    }
    for (auto& l : w.landmarks) {
        bool ok = rc.nearReach(l.x, l.y, 2);
        printf("  landmark %-18s reachable: %s\n", l.name, ok ? "yes" : "NO");
        if (!ok) bad++;
    }
    int nodeBad = 0, nodeCnt[RN_COUNT] = {0};
    for (auto& n : w.nodes) {
        nodeCnt[n.kind]++;
        nodeTotals[n.kind]++;
        if (!rc.nearReach(n.x, n.y, 2)) {
            nodeBad++;
            printf("    unreachable %s at tile (%d,%d)\n", NODE_NAME[n.kind], (int)(n.x / 16.f), (int)(n.y / 16.f));
        }
    }
    printf("  resource nodes:");
    for (int k = 0; k < RN_COUNT; k++)
        if (nodeCnt[k]) printf(" %s=%d", NODE_NAME[k], nodeCnt[k]);
    printf("\n  nodes unreachable: %d of %zu\n", nodeBad, w.nodes.size());
    if (nodeBad) bad++;
    // the edge wall must be solid everywhere except inside the gates' zones
    int hole = 0, gateOpen = 0, gateBlocked = 0;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if ((x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2) && !w.solidTile(x, y)) {
                if (portalAt(map, x, y)) gateOpen++;
                else hole++;
            }
    for (int i = 0; i < MD.nPortals; i++) {
        const Portal& pt = MD.portals[i];
        const V2 c = portalPoint(w, pt);
        const bool ok = rc.nearReach(c.x, c.y, 2);
        printf("  gate to %-9s reachable: %s\n", MAPS[pt.toMap].name, ok ? "yes" : "NO");
        if (!ok) gateBlocked++;
        // the arrival point on the other side must be a valid free spot inside that map's walls
        const V2 a = g.freeSpot(pt.arriveTx, pt.arriveTy);
        if (a.x < 3 * TS || a.y < 3 * TS || a.x > (MW - 3) * TS || a.y > (MH - 3) * TS) {
            printf("  PROBLEM: arrival point of gate to %s is outside the playable area\n", MAPS[pt.toMap].name);
            bad++;
        }
    }
    printf("  edge wall holes outside gates: %d (gate tiles open: %d)\n", hole, gateOpen);
    if (hole || gateBlocked) bad++;
    printf("  entry reachable: %s\n", rc.at(arrive.x, arrive.y) >= 0 ? "yes" : "NO");
    // walk test: the real player walks from the entry to the hub
    {
        const int f = driveTo(g, w.hub[map], 0);
        printf("  walk from the entry to the hub: %s after %.0fs (landmarks discovered %d)\n", f >= 0 ? "arrived" : "FAILED", f >= 0 ? f / 60.f : 240.f, g.landmarksFound());
        if (f < 0) bad++;
    }
    return bad;
}

// The mine and the dungeon: every level must be connected from its entrance to its exits, stairs, chests, ores and monsters, and the
// two overworld entrances must be reachable on foot from the map's own entry.
int delveCheck(Game& g) {
    int bad = 0;
    auto flood = [&](const World& w, V2 from, std::vector<char>& seen) {
        seen.assign((size_t)MW * MH, 0);
        std::vector<int> q;
        const int sx = (int)(from.x / TS), sy = (int)(from.y / TS);
        q.push_back(sy * MW + sx);
        seen[sy * MW + sx] = 1;
        for (size_t h = 0; h < q.size(); h++) {
            const int c = q[h], x = c % MW, y = c / MW;
            const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                const int nx = x + DX[k], ny = y + DY[k];
                if (nx < 1 || ny < 1 || nx >= MW - 1 || ny >= MH - 1 || seen[ny * MW + nx] || w.solidTile(nx, ny)) continue;
                seen[ny * MW + nx] = 1;
                q.push_back(ny * MW + nx);
            }
        }
    };
    long ores[RN_COUNT] = {0};
    std::vector<char> seen;
    for (int id = 0; id < DV_COUNT; id++) {
        World w;
        w.generateDelve(id);
        if (id == DV_FROST3) for (int x = 64; x <= 66; x++) w.solid[92][x] = false;  // (the king's door is opened by the hunter)
        flood(w, w.spawn, seen);
        auto reach = [&](V2 v) { return v.x >= 0 && seen[(int)(v.y / TS) * MW + (int)(v.x / TS)]; };
        int mon = 0, chest = 0, trap = 0, unreach = 0, boss = 0;
        for (const DSpawn& s : w.dsp) {
            const bool ok = reach(V2(s.x, s.y));
            if (!ok) { unreach++; if (getenv("DELVE_DEBUG")) printf("   spawn type %d kind %d at %d,%d\n", s.type, s.kind, (int)(s.x / TS), (int)(s.y / TS)); }
            if (s.type == DS_MONSTER) mon++;
            else if (s.type == DS_CHEST) chest++;
            else if (s.type == DS_TRAP) trap++;
            else boss++;
        }
        for (auto& n : w.nodes) {
            ores[n.kind]++;
            const int nx = (int)(n.x / TS), ny = (int)(n.y / TS);
            bool ok = false;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (seen[(ny + dy) * MW + nx + dx]) ok = true;
            if (!ok) { unreach++; if (getenv("DELVE_DEBUG")) printf("   node kind %d at %d,%d\n", n.kind, nx, ny); }
        }
        printf("delve %d %-18s: %d monsters, %d chests, %d traps, %d nodes, %d boss, lights %d, exit %s, stairs down %s, up %s, %d unreachable\n", id, DELVE_NAME[id], mon, chest, trap,
               (int)w.nodes.size(), boss, (int)w.lights.size(), reach(w.exitPos) ? "ok" : "NO", w.stairsDn.x < 0 ? "-" : (reach(w.stairsDn) ? "ok" : "NO"), w.stairsUp.x < 0 ? "-" : (reach(w.stairsUp) ? "ok" : "NO"), unreach);
        if (!reach(w.exitPos) || unreach) { printf("  PROBLEM: part of %s cannot be reached\n", DELVE_NAME[id]); bad++; }
        if (id != DV_FROST3 && id != DV_MINE && w.stairsDn.x < 0) { printf("  PROBLEM: no way down from %s\n", DELVE_NAME[id]); bad++; }
        if (id == DV_FROST3 && (boss != 1 || w.bossDoor.x < 0)) { printf("  PROBLEM: the throne room has no king or no door\n"); bad++; }
        if (id == DV_FROST3) {  // the throne hall is behind the door: open the door tiles and the king must become reachable
            for (int x = 64; x <= 66; x++) w.solid[92][x] = false;
            flood(w, w.spawn, seen);
            if (!seen[(int)(w.bossPos.y / TS) * MW + (int)(w.bossPos.x / TS)]) { printf("  PROBLEM: the king cannot be reached even with the door open\n"); bad++; }
        }
        if (mon < 4 && id != DV_FROST3) { printf("  PROBLEM: %s has almost no monsters\n", DELVE_NAME[id]); bad++; }
    }
    for (int k = RN_COAL; k < RN_BERRY; k++)
        if (!ores[k]) { printf("  PROBLEM: no %s in any delve\n", NODE_NAME[k]); bad++; }
    // the entrances on the overworld
    for (int m : {B_MOUNTAIN, B_SNOW}) {
        g.loadMapWorld(m);
        flood(g.world, g.freeSpot(MAPS[m].entryTx, MAPS[m].entryTy), seen);
        const V2 gt = g.world.gate;
        bool ok = gt.x >= 0;
        if (ok) {  // some walkable tile right next to the gate must be reachable
            ok = false;
            for (int dy = -2; dy <= 2 && !ok; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    const int tx = (int)(gt.x / TS) + dx, ty = (int)(gt.y / TS) + dy;
                    if (tx > 0 && ty > 0 && tx < MW && ty < MH && seen[ty * MW + tx]) { ok = true; break; }
                }
        }
        printf("entrance on map %d at (%d,%d): %s\n", m, (int)(gt.x / TS), (int)(gt.y / TS), ok ? "reachable" : "NOT REACHABLE");
        if (!ok) { printf("  PROBLEM: the delve entrance on map %d cannot be walked to\n", m); bad++; }
    }
    g.loadMapWorld(B_HOME);
    return bad;
}

int worldCheck(Game& g) {
    int bad = 0;
    long nodeTotals[RN_COUNT] = {0};
    printf("maps: %d, each %dx%d tiles (the original Wildwood area is %dx%d at tile (%d,%d) of the home map)\n", B_COUNT, MW, MH, CW, CH, COX, COY);
    for (int m = 0; m < B_COUNT; m++) bad += mapCheck(g, m, nodeTotals);
    {
        printf("all resource nodes over the five maps:");
        for (int k = 0; k < RN_COUNT; k++) printf(" %s=%ld", NODE_NAME[k], nodeTotals[k]);
        printf("\n");
        for (int k = 0; k < RN_COAL; k++)  // (the mine / dungeon ores live in the delves, checked below)
            if (!nodeTotals[k]) {
                printf("  PROBLEM: no %s anywhere\n", NODE_NAME[k]);
                bad++;
            }
    }
    bad += delveCheck(g);
    // travel test: walk through every gate with the real player, check the arrival, then walk back through the gate home
    for (int m = B_DESERT; m < B_HOMESTEAD; m++) {
        enterMap(g, B_HOME);
        g.p.pos = g.world.spawn;
        g.p.coins = 777;
        g.p.inv.add(itemMat(M_WOOD), 5);
        g.p.hp = 77;
        g.p.sk.s[SK_MINING].xp = 123;
        int gi = -1;
        for (int i = 0; i < MAPS[B_HOME].nPortals; i++)
            if (MAPS[B_HOME].portals[i].toMap == m) gi = i;
        const Portal& pt = MAPS[B_HOME].portals[gi];
        const V2 c = portalPoint(g.world, pt);
        const int f = driveTo(g, c, 1);
        const bool there = f >= 0 && g.world.mapId == m && g.curBiome == m;
        const V2 pp = g.p.pos;
        const bool valid = !g.world.blocked(pp.x, pp.y, 5.f) && pp.x > 2 * TS && pp.y > 2 * TS && pp.x < (MW - 2) * TS && pp.y < (MH - 2) * TS;
        const bool kept = g.p.coins == 777 && g.p.mat(M_WOOD) == 5 && g.p.sk.s[SK_MINING].xp == 123 && (int)g.p.hp <= 100;
        printf("  walk through the gate home -> %-9s: %s after %.1fs, map %d, valid spot %s, hunter's state kept %s\n", MAPS[m].name, there ? "arrived" : "FAILED", f / 60.f, g.world.mapId, valid ? "yes" : "NO", kept ? "yes" : "NO");
        if (!there || !valid || !kept) bad++;
        // and back
        g.travelCd = 0;
        const Portal& back = MAPS[m].portals[0];
        const V2 bc = portalPoint(g.world, back);
        const int f2 = driveTo(g, bc, 1);
        const bool home = f2 >= 0 && g.world.mapId == B_HOME;
        const V2 q = g.p.pos;
        const bool valid2 = !g.world.blocked(q.x, q.y, 5.f);
        printf("  walk through the gate %-9s -> home: %s after %.1fs, valid spot %s\n", MAPS[m].name, home ? "arrived" : "FAILED", f2 / 60.f, valid2 ? "yes" : "NO");
        if (!home || !valid2) bad++;
    }

    // terrain + climate mechanics, measured through the real player update (each in the map where that ground exists)
    {
        auto findPatch = [&](const World& w, uint8_t t, V2& out, int rad = 4) {
            for (int y = 8; y < MH - 8; y++)
                for (int x = 8; x < MW - 8; x++) {
                    bool ok = true;
                    for (int j = -rad; j <= rad && ok; j++)
                        for (int i = -rad; i <= rad && ok; i++)
                            if (w.tile[y + j][x + i] != t || w.solid[y + j][x + i]) ok = false;
                    if (ok) {
                        out = V2(x * TS + 8.f, y * TS + 8.f);
                        return true;
                    }
                }
            return false;
        };
        struct Row {
            const char* name;
            uint8_t tile;
            int biome;
        } rows[] = {{"grass", T_GRASS, B_HOME}, {"sand", T_SAND, B_DESERT}, {"dune", T_DUNE, B_DESERT}, {"mud", T_MUD, B_SWAMP},
                    {"murk", T_MURK, B_SWAMP}, {"snow", T_SNOW, B_SNOW}, {"packed snow", T_SNOWP, B_SNOW}, {"ice", T_ICE, B_SNOW}};
        float grassD = 0;
        for (auto& rw : rows) {
            enterMap(g, rw.biome);
            V2 at;
            if (!findPatch(g.world, rw.tile, at, rw.tile == T_MUD ? 2 : 4)) {
                printf("  terrain %-12s no 9x9 patch found (skipped)\n", rw.name);
                continue;
            }
            g.p.pos = at;
            g.in.right = true;
            for (int f = 0; f < 30; f++) {
                g.p.hp = g.p.maxhp;
                g.update(1.f / 60.f);
            }
            float moved = g.p.pos.x - at.x;
            g.in.right = false;
            V2 rel = g.p.pos;
            for (int f = 0; f < 30; f++) g.update(1.f / 60.f);
            float coast = g.p.pos.x - rel.x;
            if (rw.tile == T_GRASS) grassD = moved;
            printf("  terrain %-12s walked %.1f px in 0.5s (%.0f%% of grass), coasted %.1f px after release\n", rw.name, moved, grassD > 0 ? 100.f * moved / grassD : 100.f, coast);
        }
        // stamina: sprint for 1s in each climate (midday), starting from full
        struct Cl {
            int biome;
            const char* name;
            uint8_t tile;
        } cls[] = {{B_HOME, "home", T_GRASS}, {B_DESERT, "desert", T_SAND}, {B_SWAMP, "swamp", T_MUD}, {B_SNOW, "snow", T_SNOW}, {B_MOUNTAIN, "mountain", T_STONE}};
        for (int armor = 0; armor <= 2; armor += 2)
            for (auto& c : cls) {
                if (armor == 2 && c.biome != B_SNOW) continue;
                enterMap(g, c.biome);
                V2 at;
                if (!findPatch(g.world, c.tile, at, c.tile == T_MUD ? 2 : 4)) continue;
                g.dayT = 60;  // midday
                g.p.up[U_ARMOR] = armor;
                g.p.pos = at;
                g.in.right = true;
                g.in.sprint = true;
                float s0 = g.p.stamina;
                for (int f = 0; f < 60; f++) {
                    g.p.hp = g.p.maxhp;
                    g.dayT = 60;
                    g.update(1.f / 60.f);
                }
                g.in.right = g.in.sprint = false;
                printf("  sprint 1s in %-8s%s: stamina %.1f -> %.1f (drain %.1f)\n", c.name, armor == 2 ? " + fur coat" : "", s0, g.p.stamina, s0 - g.p.stamina);
            }
        g.in.right = g.in.sprint = false;
    }
    printf(bad ? "WORLDCHECK: %d PROBLEM(S)\n" : "WORLDCHECK: ok (%d problems)\n", bad);
    return bad;
}

