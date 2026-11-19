// worldfix.hpp - the repair pass that runs on every freshly generated overworld map (World::generate calls it).
//
// A random seed can build a map in which something you must reach is cut off: the hub, a landmark, a gate to another map. This pass
// walks the map the way the hunter does (the same 8 px grid and 4 px body as `--worldcheck`) from the place you arrive, and for
// everything that cannot be reached it carves a short corridor to the nearest reachable spot: trees, rocks, cacti and the like on the
// way are removed, water and cliffs become ground. Resource nodes that are still cut off are dropped. Buildings and the map's edge
// wall are never touched. It changes nothing on a map that is already fine, and it is a plain function of the generated map, so a
// saved game that regenerates the same seed gets the same repaired map.
//
// repairLeft = how many of those places are still unreachable afterwards; the caller may then try another seed (Game::findSoundSeed).
#pragma once
#include <algorithm>
#include <queue>
#include <vector>

inline int World::repairReach() {
    repairLeft = 0;
    if (mapId < 0 || mapId >= B_COUNT || mapId == B_HOMESTEAD) return 0;  // the Homestead has its own generator
    const MapDef& MD = MAPS[mapId];
    constexpr int CELL = 8, MAX_PASSES = 12;
    const int gw = MW * TS / CELL, gh = MH * TS / CELL;
    std::vector<int> dist((size_t)gw * gh, -1);
    auto pass = [&](int cx, int cy) { return cx >= 0 && cy >= 0 && cx < gw && cy < gh && !blocked(cx * CELL + CELL / 2.f, cy * CELL + CELL / 2.f, 4.f); };
    auto flood = [&](V2 start) {  // distance in cells from `start`, -1 = unreachable (no corner cutting, like the hunter)
        std::fill(dist.begin(), dist.end(), -1);
        const int sx = (int)(start.x / CELL), sy = (int)(start.y / CELL);
        if (!pass(sx, sy)) return;
        std::queue<std::pair<int, int>> q;
        q.push({sx, sy});
        dist[(size_t)sy * gw + sx] = 0;
        while (!q.empty()) {
            const auto [x, y] = q.front();
            q.pop();
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (!dx && !dy) continue;
                    const int nx = x + dx, ny = y + dy;
                    if (!pass(nx, ny) || dist[(size_t)ny * gw + nx] >= 0) continue;
                    if (dx && dy && (!pass(x + dx, y) || !pass(x, y + dy))) continue;
                    dist[(size_t)ny * gw + nx] = dist[(size_t)y * gw + x] + 1;
                    q.push({nx, ny});
                }
        }
    };
    auto reached = [&](float px, float py, int rad) {  // some cell within `rad` cells of the point is reachable
        for (int dy = -rad; dy <= rad; dy++)
            for (int dx = -rad; dx <= rad; dx++) {
                const int cx = (int)(px / CELL) + dx, cy = (int)(py / CELL) + dy;
                if (cx >= 0 && cy >= 0 && cx < gw && cy < gh && dist[(size_t)cy * gw + cx] >= 0) return true;
            }
        return false;
    };
    const uint8_t groundOf = mapId == B_DESERT ? T_SAND : mapId == B_SWAMP ? T_MUD : mapId == B_SNOW ? T_SNOW : mapId == B_MOUNTAIN ? T_STONE : T_GRASS;

    // what the pass may remove: things that grow or lie on the ground, never buildings or landmarks
    auto clearable = [](int k) {
        return isRockKind(k) || isTreeKind(k) || k == P_LOG || k == P_SANDROCK0 || k == P_SANDROCK1 || k == P_SNOWROCK || k == P_CRYSTAL ||
               k == P_CACTUS0 || k == P_CACTUS1 || k == P_GRAVE;
    };
    // opens one tile: removes what stands on it (only things that can be cleared) and turns water / cliff into ground
    auto carve = [&](int tx, int ty, uint8_t ground) {
        if (tx < 0 || ty < 0 || tx >= MW || ty >= MH) return;
        if (!portalAt(mapId, tx, ty) && (tx < 3 || ty < 3 || tx >= MW - 3 || ty >= MH - 3)) return;  // the edge wall stays shut
        bool explained = false;
        for (size_t i = 0; i < props.size();) {
            const Prop& p = props[i];
            const int k = p.kind;
            bool hit = false;
            int rx0 = 0, ry0 = 0, rx1 = -1, ry1 = -1;
            if (isRockKind(k)) {  // a rock blocks its whole footprint
                rockFoot(p, rx0, ry0, rx1, ry1);
                hit = tx >= rx0 && tx <= rx1 && ty >= ry0 && ty <= ry1;
            } else if (clearable(k))
                hit = (int)(p.x / TS) == tx && (int)((p.y - 1.f) / TS) == ty;
            if (!hit) { i++; continue; }
            props.erase(props.begin() + i);
            explained = true;
            for (int y = ry0; y <= ry1; y++)
                for (int x = rx0; x <= rx1; x++)
                    if (x >= 0 && y >= 0 && x < MW && y < MH && !(x < 3 || y < 3 || x >= MW - 3 || y >= MH - 3)) solid[y][x] = false;
        }
        const size_t nBefore = nodes.size();
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const ResNode& n) { return (int)(n.x / TS) == tx && (int)(n.y / TS) == ty; }), nodes.end());
        if (nodes.size() != nBefore) explained = true;
        if (solid[ty][tx] && !explained && !tileSolid(tile[ty][tx])) return;  // a building or a wall: left alone
        if (tileSolid(tile[ty][tx])) tile[ty][tx] = ground;
        solid[ty][tx] = false;
    };
    // a corridor of 4-connected tiles between two tiles
    auto carvePath = [&](int x0, int y0, int x1, int y1, uint8_t ground) {
        int x = x0, y = y0;
        carve(x, y, ground);
        while (x != x1 || y != y1) {
            const int dx = x1 - x, dy = y1 - y;
            if (std::abs(dx) >= std::abs(dy)) x += dx > 0 ? 1 : -1;
            else y += dy > 0 ? 1 : -1;
            carve(x, y, ground);
        }
    };

    // where the hunter arrives, and what must be reachable from there
    const V2 arrive = mapId == B_HOME ? spawn : V2((MD.entryTx + 0.5f) * TS, (MD.entryTy + 0.5f) * TS);
    std::vector<V2> targets;
    targets.push_back(hub[mapId]);
    for (const Landmark& l : landmarks) targets.push_back(V2(l.x, l.y));
    for (int i = 0; i < MD.nPortals; i++) {  // the open tile of each gate nearest its middle
        const TileRect& z = MD.portals[i].zone;
        const float mx = (z.x0 + z.x1) * 0.5f, my = (z.y0 + z.y1) * 0.5f;
        V2 best((mx + 0.5f) * TS, (my + 0.5f) * TS);
        float bd = 1e9f;
        for (int y = z.y0; y <= z.y1; y++)
            for (int x = z.x0; x <= z.x1; x++)
                if (!solidTile(x, y) && hypotf(x - mx, y - my) < bd) bd = hypotf(x - mx, y - my), best = V2(x * TS + 8.f, y * TS + 8.f);
        targets.push_back(best);
    }
    if (blocked(arrive.x, arrive.y, 4.f)) carve((int)(arrive.x / TS), (int)(arrive.y / TS), groundOf);

    for (int round = 0; round < MAX_PASSES; round++) {
        flood(arrive);
        bool any = false;
        for (const V2& t : targets) {
            if (reached(t.x, t.y, 2)) continue;
            any = true;
            long long best = -1, bestD2 = 1LL << 60;  // the nearest reachable cell
            for (int cy = 0; cy < gh; cy++)
                for (int cx = 0; cx < gw; cx++) {
                    if (dist[(size_t)cy * gw + cx] < 0) continue;
                    const long long dx = cx * CELL + CELL / 2 - (long long)t.x, dy = cy * CELL + CELL / 2 - (long long)t.y;
                    if (dx * dx + dy * dy < bestD2) bestD2 = dx * dx + dy * dy, best = (long long)cy * gw + cx;
                }
            if (best < 0) continue;  // nothing reachable at all (the arrival itself is walled in)
            const int bx = (int)((best % gw) * CELL / TS), by = (int)((best / gw) * CELL / TS);
            uint8_t ground = tile[by][bx];
            if (tileSolid(ground)) ground = groundOf;
            carvePath((int)(t.x / TS), (int)(t.y / TS), bx, by, ground);
        }
        if (!any) break;
    }
    flood(arrive);
    for (const V2& t : targets)
        if (!reached(t.x, t.y, 2)) repairLeft++;
    // trees, rocks and the like standing in water or on a cliff are stuck: nobody can reach or use them, so they go
    props.erase(std::remove_if(props.begin(), props.end(),
                               [&](const Prop& p) {
                                   const int tx = (int)(p.x / TS), ty = (int)((p.y - 1.f) / TS);
                                   return clearable(p.kind) && tx >= 0 && ty >= 0 && tx < MW && ty < MH && tileSolid(tile[ty][tx]);
                               }),
                props.end());
    // resource nodes that are still cut off are not worth keeping
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const ResNode& n) { return !reached(n.x, n.y, 2); }), nodes.end());
    return repairLeft;
}
