// homestead_gen.hpp - generates the Homestead map (B_HOMESTEAD): a calm valley made for building, farming and raising animals.
//
// Layout (240 x 168 tiles like every map): a big open meadow in the middle (the buildable land) with a patch of dark, fertile soil beside
// the arrival plaza, a stream with fords on the west side and a pond on the east, forests along every edge with rock outcrops, berry
// bushes, herbs, mushrooms and wild flowers scattered through the meadow's rim. Trees and rocks are ordinary props (chopped with an axe,
// mined with a pickaxe, they grow back: gathering.cpp), so the whole harvest and respawn system is reused. Included at the end of worldgen.hpp.
#pragma once

// where things stand (tile coordinates): the plaza with the return portal and the market, and the fertile soil
constexpr int HS_PLAZA_X = 120, HS_PLAZA_Y = 84;
constexpr int HS_PORTAL_X = 120, HS_PORTAL_Y = 80;
constexpr int HS_MARKET_X = 126, HS_MARKET_Y = 86;

inline void World::generateHomestead() {
    Rng r(seed * 104729u + 4441u);
    const uint32_t S = seed + (uint32_t)B_HOMESTEAD * 31u;
    const V2 away(-99999.f, -99999.f);
    village = hut = door = keeper = fire = questGiver = away;
    for (int b = 0; b < B_COUNT; b++) hub[b] = away;
    hub[B_HOMESTEAD] = V2(HS_PLAZA_X * TS + 8.f, HS_PLAZA_Y * TS + 8.f);
    spawn = V2(HS_PLAZA_X * TS + 8.f, (HS_PLAZA_Y + 2) * TS + 8.f);
    hportal = V2(HS_PORTAL_X * TS + 8.f, HS_PORTAL_Y * TS + 14.f);
    hmarket = V2(HS_MARKET_X * TS + 8.f, HS_MARKET_Y * TS + 14.f);

    auto inMap = [](int x, int y) { return x >= 0 && y >= 0 && x < MW && y < MH; };
    auto inRing = [](int x, int y) { return x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2; };
    auto cl = [](float v, float a, float b) { return v < a ? a : (v > b ? b : v); };
    std::vector<uint8_t> keep((size_t)MW * MH, 0);  // 1 = plaza / path (no props), 2 = water
    auto K = [&](int x, int y) -> uint8_t& { return keep[(size_t)y * MW + x]; };

    // ---- ground: grass with darker, richer grass in patches; fertile soil (dark) near the plaza
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            biome[y][x] = B_HOMESTEAD;
            const float ff = fbm(x * 0.07f, y * 0.07f, S + 1);
            uint8_t t = ff > 0.6f ? T_DARK : T_GRASS;
            const float fx = (x - (HS_PLAZA_X + 4)) / 20.f, fy = (y - (HS_PLAZA_Y + 14)) / 11.f;  // the fertile field south-east of the plaza
            if (fx * fx + fy * fy + (fbm(x * 0.2f, y * 0.2f, S + 5) - 0.5f) * 0.5f < 1.f) t = T_DARK;
            tile[y][x] = t;
        }
    // ---- water: a pond in the east, a smaller one in the north-west and a stream with three fords in the west
    struct Pond { float cx, cy, rx, ry; } ponds[] = {{192, 56, 15, 8.5f}, {40, 38, 9, 5.5f}, {170, 128, 8, 5}};
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            for (auto& p : ponds) {
                const float dx = (x - p.cx) / p.rx, dy = (y - p.cy) / p.ry;
                if (dx * dx + dy * dy + (fbm(x * 0.22f, y * 0.22f, S + 7) - 0.5f) * 0.8f < 1.f) tile[y][x] = T_WATER;
            }
            const float sx = 62.f + sinf(y * 0.07f) * 7.f + (fbm(x * 0.1f, y * 0.1f, S + 9) - 0.5f) * 3.f;
            const bool ford = (y > 60 && y < 66) || (y > 100 && y < 106) || (y > 134 && y < 139);
            if (!ford && fabsf(x - sx) < 1.9f && y > 6 && y < MH - 6) tile[y][x] = T_WATER;
        }
    // ---- the plaza (dirt) and paths: the arrival plaza, a path to the market and the portal
    for (int dy = -6; dy <= 6; dy++)
        for (int dx = -8; dx <= 8; dx++) {
            const int x = HS_PLAZA_X + dx, y = HS_PLAZA_Y + dy;
            if (dx * dx / 64.f + dy * dy / 36.f <= 1.f) {
                tile[y][x] = T_DIRT;
                K(x, y) = 1;
            }
        }
    // a wide clearing: nothing grows within 26 x 18 tiles of the plaza, so the first thing the hunter sees is open buildable land
    auto clearing = [&](int x, int y) {
        const float dx = (x - HS_PLAZA_X) / 30.f, dy = (y - (HS_PLAZA_Y + 6)) / 22.f;
        return dx * dx + dy * dy < 1.f;
    };

    // ---- scatter: trees, rocks, bushes, flowers
    auto tileProp = [&](int kind, int tx, int ty) {
        props.push_back({kind, tx * TS + 8.f + r.range(-2, 2), ty * TS + 15.f, r.f() * 6.28f});
        solid[ty][tx] = true;
    };
    auto rockProp = [&](int kind, int tx, int ty) {
        props.push_back({kind, tx * TS + 8.f, ty * TS + 14.f, 0});
        solid[ty][tx] = true;
    };
    for (int ty = 2; ty < MH - 2; ty++)
        for (int tx = 2; tx < MW - 2; tx++) {
            if (K(tx, ty) != 0 || tileSolid(tile[ty][tx]) || clearing(tx, ty)) {
                if (!tileSolid(tile[ty][tx]) && K(tx, ty) == 0 && r.chance(0.10f)) {  // meadow: grass, flowers, a few pebbles
                    const float roll = r.f();
                    decor.push_back({roll < 0.35f ? r.irange(0, 2) : roll < 0.85f ? (int)D_TUFT : (int)D_PEBBLES, tx * TS + r.range(2, 14.f), ty * TS + r.range(6, 15.f), r.f() * 6.28f});
                }
                continue;
            }
            // forest density: heavy along the edges and in noise patches, sparse trees dotted through the rest
            const float edge = std::min(std::min(tx, MW - 1 - tx) / 36.f, std::min(ty, MH - 1 - ty) / 26.f);
            const float ff = fbm(tx * 0.06f, ty * 0.06f, S + 3);
            const float pTree = 0.006f + 0.34f * cl(1.f - edge, 0.f, 1.f) * cl((ff - 0.38f) / 0.25f, 0.f, 1.f) + 0.012f * cl((ff - 0.55f) / 0.2f, 0.f, 1.f);
            const uint8_t t = tile[ty][tx];
            if (r.chance(pTree)) tileProp(t == T_DARK ? (r.chance(0.5f) ? P_PINE : P_OAK0) : (r.chance(0.12f) ? P_PINE : (r.chance(0.5f) ? P_OAK0 : P_OAK1)), tx, ty);
            else if (r.chance(edge < 0.5f ? 0.014f : 0.004f)) rockProp(r.chance(0.28f) ? P_BOULDER : (r.chance(0.55f) ? P_ROCK0 : P_ROCK1), tx, ty);
            else if (r.chance(0.011f)) decor.push_back({6 + r.irange(0, 1), tx * TS + 8.f, ty * TS + 15.f, r.f() * 6.28f});  // bushes
            else if (r.chance(0.09f)) {
                const float roll = r.f();
                decor.push_back({roll < 0.3f ? r.irange(0, 2) : roll < 0.82f ? (int)D_TUFT : roll < 0.9f ? (int)D_MUSHROOM : (int)D_PEBBLES, tx * TS + r.range(2, 14.f), ty * TS + r.range(6, 15.f), r.f() * 6.28f});
            }
        }
    // rock outcrops (clusters, in the south-east and north-west) so stone is easy to find without covering the buildable land
    struct Outcrop { int cx, cy; } outcrops[] = {{208, 120}, {30, 100}, {96, 144}, {150, 22}, {212, 90}, {20, 60}};
    for (auto& o : outcrops)
        for (int i = 0; i < 16; i++) {
            const int tx = o.cx + r.irange(-5, 5), ty = o.cy + r.irange(-4, 4);
            if (!inMap(tx, ty) || inRing(tx, ty) || K(tx, ty) != 0 || solid[ty][tx] || tileSolid(tile[ty][tx]) || clearing(tx, ty)) continue;
            rockProp(i % 5 == 0 ? P_BOULDER : (i % 2 ? P_ROCK0 : P_ROCK1), tx, ty);
        }

    // ---- the edge wall: a dense belt of trees
    for (int ty = 0; ty < MH; ty++)
        for (int tx = 0; tx < MW; tx++) {
            if (!inRing(tx, ty)) continue;
            solid[ty][tx] = true;
            if (!r.chance(0.8f)) continue;
            props.push_back({r.chance(0.6f) ? P_PINE : P_OAK0, tx * TS + 8.f + r.range(-2, 2), ty * TS + 15.f, r.f() * 6.28f});
        }

    // ---- resource nodes (the existing node system: gather by hand, they grow back)
    auto passable = [&](int tx, int ty) { return inMap(tx, ty) && !solid[ty][tx] && !tileSolid(tile[ty][tx]); };
    auto nodeFree = [&](int tx, int ty) {
        if (inRing(tx, ty) || K(tx, ty) != 0) return false;
        return passable(tx, ty) && passable(tx + 1, ty) && passable(tx - 1, ty) && passable(tx, ty + 1) && passable(tx, ty - 1);
    };
    auto farFromNodes = [&](float x, float y, float minD) {
        for (auto& n : nodes)
            if (hypotf(n.x - x, n.y - y) < minD) return false;
        return true;
    };
    auto scatter = [&](int kind, int count, const std::function<bool(int, int)>& ok, float spacing) {
        int placed = 0;
        for (int att = 0; att < count * 80 && placed < count; att++) {
            const int tx = r.irange(3, MW - 4), ty = r.irange(3, MH - 4);
            if (!nodeFree(tx, ty) || !ok(tx, ty)) continue;
            const float px = tx * TS + 8.f + r.range(-3, 3), py = ty * TS + 12.f + r.range(-1, 2);
            if (!farFromNodes(px, py, spacing * TS)) continue;
            nodes.push_back({kind, px, py, r.f() * 6.28f});
            placed++;
        }
    };
    auto rim = [&](int x, int y) {  // the outer part of the clearing and the forest: where wild things grow
        const float dx = (x - HS_PLAZA_X) / 30.f, dy = (y - (HS_PLAZA_Y + 6)) / 22.f;
        return dx * dx + dy * dy > 0.55f;
    };
    scatter(RN_HERB, 36, [&](int x, int y) { return rim(x, y); }, 4.f);
    scatter(RN_MUSHROOM, 26, [&](int x, int y) { return rim(x, y) && tile[y][x] == T_DARK; }, 4.f);
    scatter(RN_BRANCH, 36, [&](int x, int y) { return rim(x, y); }, 4.f);
    scatter(RN_FLAX, 28, [&](int x, int y) { return rim(x, y) && tile[y][x] == T_GRASS; }, 4.f);
    scatter(RN_BERRY, 26, [&](int x, int y) { return rim(x, y); }, 5.f);
    scatter(RN_WILDFLOWER, 34, [&](int x, int y) { return tile[y][x] == T_GRASS; }, 4.f);

    // ---- the plaza furniture: the return portal and the market stall (both solid, with prompts from Game::homesteadInteract)
    auto solidTiles = [&](int tx, int ty, int w) {
        for (int i = 0; i < w; i++)
            if (inMap(tx + i, ty)) solid[ty][tx + i] = true;
    };
    props.push_back({P_HPORTAL, hportal.x, hportal.y, 0});
    solidTiles(HS_PORTAL_X - 1, HS_PORTAL_Y, 3);
    props.push_back({P_HMARKET, hmarket.x, hmarket.y, 0});
    solidTiles(HS_MARKET_X - 1, HS_MARKET_Y, 3);
    // a lamp at each end of the plaza (lit at night) and a few trees framing it
    for (int s : {-1, 1}) {
        const float lx = (HS_PLAZA_X + s * 7) * TS + 8.f, ly = (HS_PLAZA_Y + 1) * TS + 14.f;
        props.push_back({P_LAMP, lx, ly, 0});
        solid[HS_PLAZA_Y + 1][HS_PLAZA_X + s * 7] = true;
        lanterns.push_back({lx, ly - 24.f, 0.8f});
    }
    landmarks.push_back({"HOMESTEAD PLAZA", B_HOMESTEAD, HS_PLAZA_X * TS + 8.f, HS_PLAZA_Y * TS + 8.f, false});
}
