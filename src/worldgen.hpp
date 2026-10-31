// worldgen.hpp - builds one map: the home map's gates and forest, or a complete biome map. Included at the end of world.hpp.
//
// Every map is MW x MH (240x168) tiles, ringed by a 2-tile wall. The home map is the original 88x60 Wildwood (World::generateCore,
// unchanged) in the centre, surrounded by home forest; its four dirt paths run out to gaps in the edge wall (the gaps are portals, see maps.hpp).
// Each biome map is generated whole from its own rules below: ground, a trail from its gate to a hub landmark with spur trails
// to the other landmarks, scattered props and decorations, the edge wall and resource nodes. The biome tables (what grows
// where, which resources) are the ones the original ring-around-home world used; only the layout was fitted to one map.
// Everything is deterministic for a given world seed and map.
#pragma once
#include <functional>
#include <vector>
#include "maps.hpp"

inline void World::generateMap(int map) {
    Rng r(seed * 104729u + 977u + (uint32_t)map * 7919u);
    const uint32_t S = seed + (uint32_t)map * 31u;  // per-map salt for the noise fields
    const bool home = map == B_HOME;
    const MapDef& MD = MAPS[map];
    auto inCore = [](int x, int y) { return x >= COX && x < COX + CW && y >= COY && y < COY + CH; };
    auto inMap = [](int x, int y) { return x >= 0 && y >= 0 && x < MW && y < MH; };
    auto inRing = [](int x, int y) { return x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2; };  // the map's edge wall
    auto cl = [](float v, float a, float b) { return v < a ? a : (v > b ? b : v); };

    if (!home) {  // no village here: park the village-only things far outside the map so nothing draws or triggers
        const V2 away(-99999.f, -99999.f);
        village = hut = door = keeper = fire = questGiver = away;
    }
    for (int b = 0; b < B_COUNT; b++) hub[b] = V2(-99999.f, -99999.f);
    hub[map] = V2(MD.hubTx * TS + 8.f, MD.hubTy * TS + 8.f);
    if (!home) spawn = V2(MD.entryTx * TS + 8.f, MD.entryTy * TS + 8.f);

    // keep mask: 1 = trail tile, 2 = cleared zone (no scattered props / solid terrain), 3 = landmark feature
    std::vector<uint8_t> keep((size_t)MW * MH, 0);
    auto K = [&](int x, int y) -> uint8_t& { return keep[(size_t)y * MW + x]; };

    auto baseGround = [](int b) -> uint8_t {
        switch (b) {
            case B_DESERT: return T_SAND;
            case B_SWAMP: return T_MUD;
            case B_SNOW: return T_SNOW;
            case B_MOUNTAIN: return T_STONE;
            default: return T_GRASS;
        }
    };

    // ------------------------------------------------------------ 1. ground of a biome map
    auto groundFor = [&](int b, int x, int y) -> uint8_t {
        switch (b) {
            case B_DESERT: {
                float d = fbm(x * 0.06f, y * 0.12f, S + 101);
                float rk = fbm(x * 0.08f + 40.f, y * 0.08f + 40.f, S + 103);
                if (rk > 0.68f) return T_STONE;
                if (d > 0.56f) return T_DUNE;
                return T_SAND;
            }
            case B_SWAMP: {
                float bg = fbm(x * 0.055f, y * 0.055f, S + 202);
                float mr = fbm(x * 0.07f, y * 0.07f, S + 201);
                float dk = fbm(x * 0.09f, y * 0.09f, S + 203);
                if (bg > 0.64f) return T_BOG;
                if (mr > 0.53f) return T_MURK;
                if (dk > 0.55f) return T_DARK;
                return T_MUD;
            }
            case B_SNOW: {
                float p = fbm(x * 0.07f, y * 0.07f, S + 301);
                return p > 0.58f ? T_SNOWP : T_SNOW;
            }
            default: {  // mountains: cliff massifs, thin ridge lines with gaps, alpine meadows near the gate (the east side)
                float alt = 1.f - x / (float)(MW - 1);  // 0 next to the gate, 1 at the far west edge
                float m = fbm(x * 0.055f, y * 0.055f, S + 403);
                float ridge = fbm(x * 0.07f, y * 0.07f, S + 402);
                float gap = fbm(x * 0.12f + 9.f, y * 0.12f + 9.f, S + 404);
                float meadow = fbm(x * 0.07f + 50.f, y * 0.07f + 50.f, S + 401);
                if (m > 0.66f - 0.05f * alt) return T_CLIFF;
                if (fabsf(ridge - 0.5f) < 0.03f && gap < 0.56f) return T_CLIFF;
                if (alt < 0.5f && meadow > 0.52f - alt * 0.25f) return meadow > 0.64f ? T_DARK : T_GRASS;
                return T_STONE;
            }
        }
    };
    if (home) {  // the home forest around the original area: the same grass / dark-grass noise, a few lakes
        struct Lake {
            float cx, cy, rx, ry;
        } lakes[] = {{30, 30, 12, 6}, {205, 28, 14, 7}, {30, 140, 12, 6}, {205, 142, 13, 6.5f}, {62, 84, 6, 9}, {180, 84, 6, 9}, {120, 20, 9, 4.5f}, {120, 150, 9, 4.5f}};
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                if (inCore(x, y)) continue;
                uint8_t t = fbm(x * 0.08f, y * 0.08f, S + 1) > 0.6f ? T_DARK : T_GRASS;
                for (auto& l : lakes) {
                    float dx = (x - l.cx) / l.rx, dy = (y - l.cy) / l.ry;
                    if (dx * dx + dy * dy + (fbm(x * 0.22f, y * 0.22f, S + 7) - 0.5f) * 0.8f < 1.f) t = T_WATER;
                }
                tile[y][x] = t;
                biome[y][x] = B_HOME;
            }
    } else {
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                biome[y][x] = (uint8_t)map;
                tile[y][x] = groundFor(map, x, y);
            }
        if (map == B_SWAMP) {  // bog shores are shallow murk, not a hard edge
            for (int y = 1; y < MH - 1; y++)
                for (int x = 1; x < MW - 1; x++)
                    if (tile[y][x] == T_BOG) {
                        const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                        for (auto& d : d4) {
                            uint8_t& n = tile[y + d[1]][x + d[0]];
                            if (n == T_MUD || n == T_DARK) n = T_MURK;
                        }
                    }
        }
        if (map == B_SNOW) {  // frozen lakes
            struct Lk {
                float cx, cy, rx, ry;
            } lk[] = {{60, 40, 11, 6}, {190, 50, 9, 5}, {30, 110, 8, 4.5f}, {160, 20, 7, 4}, {200, 140, 10, 5.5f}};
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++)
                    for (auto& l : lk) {
                        float dx = (x - l.cx) / l.rx, dy = (y - l.cy) / l.ry;
                        float d = dx * dx + dy * dy + (fbm(x * 0.22f, y * 0.22f, S + 307) - 0.5f) * 0.7f;
                        if (d < 1.f) tile[y][x] = T_ICE;
                    }
        }
    }

    // ------------------------------------------------------------ 2. trails, hub plaza, gates
    // In the home map the trails run through the tree wall (the wall is the ring); in a biome map the ring is cut by an explicit gap.
    auto trailTile = [&](int x, int y) {
        if (!inMap(x, y)) return;
        if (inRing(x, y)) return;
        tile[y][x] = biome[y][x] == B_SNOW ? T_SNOWP : T_DIRT;
        solid[y][x] = false;
        K(x, y) = 1;
    };
    auto clearZone = [&](int cx, int cy, int rad) {
        for (int dy = -rad; dy <= rad; dy++)
            for (int dx = -rad; dx <= rad; dx++) {
                int x = cx + dx, y = cy + dy;
                if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                if (K(x, y) == 0) K(x, y) = 2;
                if (!(home && inCore(x, y)) && tileSolid(tile[y][x])) tile[y][x] = baseGround(biome[y][x]);
            }
    };
    auto stampBlock = [&](int px, int py) {
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++) trailTile(px + a, py + b);
        clearZone(px, py, 2);
        clearZone(px + 1, py + 1, 2);
    };
    auto carveLine = [&](float x0, float y0, float x1, float y1, float amp, int sk) {
        float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
        if (len < 1.f) return;
        float nx = -dy / len, ny = dx / len;
        int n = (int)len;
        for (int i = 0; i <= n; i++) {
            float t = i / len, w = sinf(i * 0.33f + sk) * amp * std::min(1.f, std::min((float)i, (float)(n - i)) / 6.f);
            stampBlock((int)(x0 + dx * t + nx * w), (int)(y0 + dy * t + ny * w));
        }
    };

    if (home) {
        // The four original paths, continued to the edge of the map with the same wiggle formula; where they meet the tree wall
        // the wall is opened (the gaps are the portals to the biome maps).
        const float vx = COX + 44, vy = COY + 30;
        const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0; k < 4; k++)
            for (int i = 30; i <= 125; i++) {
                float wig = sinf(i * 0.37f + k * 2.1f) * 1.6f;
                int px = (int)(vx + dirs[k][0] * i + (dirs[k][0] ? 0 : wig));
                int py = (int)(vy + dirs[k][1] * i + (dirs[k][1] ? 0 : wig));
                stampBlock(px, py);
            }
        for (size_t i = 0; i < props.size();) {
            const Prop& p = props[i];
            int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
            bool tree = p.kind == P_OAK0 || p.kind == P_OAK1 || p.kind == P_PINE || p.kind == P_ROCK0 || p.kind == P_ROCK1;
            if (inMap(tx, ty) && K(tx, ty) != 0 && tree) {  // the trail opens the core's tree wall
                solid[ty][tx] = false;
                props.erase(props.begin() + i);
            } else
                i++;
        }
        for (size_t i = 0; i < decor.size();) {
            int tx = (int)(decor[i].x / TS), ty = (int)(decor[i].y / TS);
            if (inMap(tx, ty) && K(tx, ty) == 1) decor.erase(decor.begin() + i);
            else i++;
        }
    } else {
        // the gate: the trail starts just inside the edge wall at the gap and runs to the hub
        for (int i = 0; i < MD.nPortals; i++) {
            const TileRect& z = MD.portals[i].zone;
            const int side = MD.portals[i].side;  // the edge the gap is in
            const int gx = (z.x0 + z.x1) / 2, gy = (z.y0 + z.y1) / 2;
            const int ix = side == 0 ? MW - 3 : side == 3 ? 2 : gx, iy = side == 1 ? MH - 3 : side == 2 ? 2 : gy;
            carveLine((float)ix, (float)iy, (float)MD.hubTx, (float)MD.hubTy, 1.2f, 4 + map);
        }
        // hub plaza
        for (int dy = -4; dy <= 4; dy++)
            for (int dx = -4; dx <= 4; dx++)
                if (dx * dx + dy * dy <= 12) trailTile(MD.hubTx + dx, MD.hubTy + dy);
        clearZone(MD.hubTx, MD.hubTy, 6);
    }

    // cut every map's edge wall open at its gates (the gap tiles are the portal zones)
    for (int i = 0; i < MD.nPortals; i++) {
        const TileRect& z = MD.portals[i].zone;
        for (int y = z.y0; y <= z.y1; y++)
            for (int x = z.x0; x <= z.x1; x++) {
                tile[y][x] = biome[y][x] == B_SNOW ? T_SNOWP : T_DIRT;
                solid[y][x] = false;
                K(x, y) = 1;
            }
    }

    // ------------------------------------------------------------ helpers for landmarks
    auto clearDisc = [&](float cx, float cy, float rad, uint8_t kv) {
        for (int y = (int)(cy - rad) - 1; y <= (int)(cy + rad) + 1; y++)
            for (int x = (int)(cx - rad) - 1; x <= (int)(cx + rad) + 1; x++) {
                if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                if (hypotf(x - cx, y - cy) > rad) continue;
                if (K(x, y) < kv) K(x, y) = kv;
                if (tileSolid(tile[y][x])) tile[y][x] = baseGround(biome[y][x]);
                solid[y][x] = false;
            }
    };
    // solid prop on a tile (w tiles wide, centred on tx)
    auto addP = [&](int kind, int tx, int ty, int w = 1, float ox = 8.f, float oy = 15.f) {
        props.push_back({kind, tx * TS + ox, ty * TS + oy, r.f() * 6.28f});
        for (int i = -(w / 2); i <= w / 2; i++)
            if (inMap(tx + i, ty)) {
                solid[ty][tx + i] = true;
                if (K(tx + i, ty) < 3) K(tx + i, ty) = 3;
            }
    };
    auto addDecor = [&](int kind, float x, float y) { decor.push_back({kind, x, y, r.f() * 6.28f}); };
    auto addLandmark = [&](const char* name, int b, float tx, float ty) { landmarks.push_back({name, b, tx * TS + 8.f, ty * TS + 8.f, false}); };
    // trail from the hub towards a landmark, stopping `reach` tiles short of its centre
    auto spur = [&](float tx, float ty, float reach, int sk) {
        float hx = (float)MD.hubTx, hy = (float)MD.hubTy, dx = tx - hx, dy = ty - hy, len = sqrtf(dx * dx + dy * dy);
        if (len <= reach + 1) return;
        carveLine(hx, hy, tx - dx / len * reach, ty - dy / len * reach, 1.4f, sk);
    };

    // ------------------------------------------------------------ 3. landmarks
    if (map == B_DESERT) {
        const int hx = MD.hubTx, hy = MD.hubTy;
        addLandmark("SUNKEN RUINS", B_DESERT, hx, hy);
        clearDisc(hx, hy, 8, 2);
        const int col[8][2] = {{-5, -4}, {-2, -5}, {3, -5}, {6, -3}, {-6, 1}, {6, 3}, {-4, 5}, {4, 6}};
        for (auto& c : col) addP(P_COLUMN, hx + c[0], hy + c[1]);
        addP(P_RUINWALL, hx - 4, hy - 2, 3);
        addP(P_RUINWALL, hx + 2, hy + 4, 3);
        addP(P_RUINWALL, hx - 1, hy - 7, 3);
        addP(P_RUINWALL, hx + 7, hy - 1, 3);
        lanterns.push_back({hx * TS + 8.f, hy * TS - 6.f, 0.7f});  // a brazier in the middle of the plaza
        // oasis
        const float cx = 120, cy = 40, rx = 5.5f, ry = 3.8f;
        addLandmark("OASIS", B_DESERT, cx, cy + 6);
        clearDisc(cx, cy, 11, 2);
        for (int y = (int)(cy - 12); y <= (int)(cy + 12); y++)
            for (int x = (int)(cx - 14); x <= (int)(cx + 14); x++) {
                if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                float dx = (x - cx) / rx, dy = (y - cy) / ry;
                float d = dx * dx + dy * dy + (fbm(x * 0.25f, y * 0.25f, S + 601) - 0.5f) * 0.5f;
                if (d < 1.f) {
                    tile[y][x] = T_WATER;
                    K(x, y) = 3;
                } else if (d < 2.6f) {
                    tile[y][x] = T_GRASS;
                    K(x, y) = 3;
                    if (r.chance(0.30f)) addDecor(r.chance(0.5f) ? D_TUFT : r.irange(0, 2), x * TS + r.range(2, 14), y * TS + r.range(6, 15));
                }
            }
        for (int i = 0; i < 9; i++) {
            float a = i * 0.7f + r.range(-0.2f, 0.2f);
            int px = (int)(cx + cosf(a) * rx * 1.7f), py = (int)(cy + sinf(a) * ry * 1.7f);
            if (inMap(px, py) && tile[py][px] == T_GRASS && K(px, py) != 1 && !solid[py][px]) addP(P_PALM, px, py);
        }
        // bone field
        const float bx = 190, by = 130;
        addLandmark("BONE FIELD", B_DESERT, bx, by);
        clearDisc(bx, by, 6, 2);
        for (int i = 0; i < 16; i++) {
            float a = r.range(0, 6.28f), d = r.range(0.5f, 5.2f);
            addDecor(r.chance(0.55f) ? D_SKULL : D_PEBBLES, (bx + cosf(a) * d) * TS + 8, (by + sinf(a) * d) * TS + 8);
        }
        addP(P_SANDROCK1, (int)bx - 4, (int)by - 2, 1, 8, 14);
        addP(P_SANDROCK0, (int)bx + 4, (int)by + 3, 1, 8, 14);
        // sandstone arch
        const int ax = 150, ay = 28;
        addLandmark("SANDSTONE ARCH", B_DESERT, ax, ay + 2);
        clearDisc(ax, ay, 5, 2);
        props.push_back({P_ARCH, ax * TS + 8.f, ay * TS + 15.f, 0});
        solid[ay][ax - 1] = solid[ay][ax + 1] = true;
        K(ax - 1, ay) = K(ax + 1, ay) = 3;
        spur(cx, cy, 9, 1);
        spur(bx, by, 3, 2);
        spur((float)ax, (float)ay, 3, 3);
    } else if (map == B_SWAMP) {
        const int hx = MD.hubTx, hy = MD.hubTy;
        addLandmark("STILT HUT", B_SWAMP, hx, hy + 2);
        clearDisc(hx, hy, 8, 2);
        props.push_back({P_STILTHUT, hx * TS + 8.f, (hy - 2) * TS + 15.f, 0});
        for (int i = -1; i <= 1; i++)
            for (int j = 2; j <= 3; j++) {
                solid[hy - j][hx + i] = true;
                K(hx + i, hy - j) = 3;
            }
        lanterns.push_back({hx * TS + 8.f + 14, (hy - 2) * TS - 18.f, 0.8f});
        addP(P_LOG, hx - 6, hy + 1);
        addP(P_DEADTREE0, hx + 7, hy - 3);
        // witch tree
        const int wx = 50, wy = 110;
        addLandmark("WITCH TREE", B_SWAMP, wx, wy + 3);
        clearDisc(wx, wy, 6, 2);
        props.push_back({P_WITCHTREE, wx * TS + 8.f, wy * TS + 15.f, 0});
        for (int i = -1; i <= 1; i++)
            for (int j = 0; j <= 1; j++) {
                solid[wy - j][wx + i] = true;
                K(wx + i, wy - j) = 3;
            }
        for (int i = 0; i < 4; i++) addDecor(D_SKULL, (wx + r.range(-4, 4)) * TS + 8, (wy + r.range(1, 4)) * TS + 8);
        // sunken graves
        const int gx = 190, gy = 120;
        addLandmark("SUNKEN GRAVES", B_SWAMP, gx, gy);
        clearDisc(gx, gy, 7, 2);
        for (int row = 0; row < 2; row++)
            for (int c = 0; c < 4; c++) addP(P_GRAVE, gx - 4 + c * 2 + (row ? 1 : 0), gy - 1 + row * 3);
        spur((float)wx, (float)(wy + 3), 3, 1);
        spur((float)gx, (float)gy, 4, 2);
    } else if (map == B_SNOW) {
        const int hx = MD.hubTx, hy = MD.hubTy;
        addLandmark("ICE FISHER'S HUT", B_SNOW, hx, hy + 2);
        clearDisc(hx, hy, 8, 2);
        props.push_back({P_ICEHUT, hx * TS + 8.f, (hy - 1) * TS + 15.f, 0});
        for (int i = -1; i <= 1; i++)
            for (int j = 1; j <= 2; j++) {
                solid[hy - j][hx + i] = true;
                K(hx + i, hy - j) = 3;
            }
        lanterns.push_back({hx * TS + 8.f - 8, (hy - 1) * TS - 6.f, 0.75f});
        // a small fishing pond next to the hut
        for (int y = hy + 1; y <= hy + 8; y++)
            for (int x = hx - 14; x <= hx - 5; x++) {
                if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                float dx = (x - (hx - 9.5f)) / 4.6f, dy = (y - (hy + 4.5f)) / 3.0f;
                if (dx * dx + dy * dy < 1.f) {
                    tile[y][x] = T_ICE;
                    K(x, y) = 3;
                }
            }
        // frozen lake landmark (the ice itself is generated above); crystals along its shore
        addLandmark("FROZEN LAKE", B_SNOW, 60, 47);
        int placed = 0;
        for (int i = 0; i < 40 && placed < 7; i++) {
            float a = r.range(0, 6.28f);
            int px = (int)(60 + cosf(a) * 12.5f), py = (int)(40 + sinf(a) * 7.2f);
            if (inMap(px, py) && !inRing(px, py) && K(px, py) == 0 && (tile[py][px] == T_SNOW || tile[py][px] == T_SNOWP) && !solid[py][px]) {
                addP(P_CRYSTAL, px, py);
                placed++;
            }
        }
        // watch cairn
        const int cx = 190, cy = 60;
        addLandmark("WATCH CAIRN", B_SNOW, cx, cy + 2);
        clearDisc(cx, cy, 5, 2);
        addP(P_CAIRN, cx, cy);
        addP(P_SNOWROCK, cx - 3, cy + 1, 1, 8, 14);
        addP(P_SNOWROCK, cx + 3, cy - 1, 1, 8, 14);
        spur(60, 47, 4, 1);
        spur((float)cx, (float)cy, 3, 2);
    } else if (map == B_MOUNTAIN) {
        const int hx = MD.hubTx, hy = MD.hubTy;
        addLandmark("MOUNTAIN PASS", B_MOUNTAIN, hx, hy);
        clearDisc(hx, hy, 9, 2);
        const int st[6][2] = {{-6, -3}, {-6, 3}, {6, -3}, {6, 3}, {0, -6}, {0, 6}};
        for (auto& s : st) addP(P_STANDING, hx + s[0], hy + s[1]);
        // caves: a cliff outcrop with a mouth in its south face and a trail to the foot of it
        auto makeCave = [&](int tx, int ty, const char* name, int sk) {
            int y1 = ty;
            clearDisc(tx, ty + 1, 8, 2);
            for (int y = ty - 3; y <= y1; y++)
                for (int x = tx - 4; x <= tx + 4; x++) {
                    if (!inMap(x, y) || inRing(x, y) || K(x, y) == 1) continue;
                    if (y == ty - 3 && (x == tx - 4 || x == tx + 4)) continue;  // rounded corners
                    tile[y][x] = T_CLIFF;
                    solid[y][x] = true;
                    K(x, y) = 3;
                }
            props.push_back({P_CAVE, tx * TS + 8.f, (y1 + 1) * TS + 0.f, 0});
            addLandmark(name, B_MOUNTAIN, tx, y1 + 2);
            // trail: hub -> a waypoint south of the outcrop -> the cave mouth
            carveLine((float)hx, (float)hy, (float)tx, (float)(y1 + 5), 1.4f, sk);
            carveLine((float)tx, (float)(y1 + 5), (float)tx, (float)(y1 + 1), 0.f, sk);
        };
        makeCave(130, 40, "ECHO CAVE", 1);
        makeCave(130, 128, "HOLLOW CAVE", 2);
        // summit cairn
        const int sx = 30, sy = 84;
        addLandmark("SUMMIT CAIRN", B_MOUNTAIN, sx, sy + 2);
        clearDisc(sx, sy, 5, 2);
        addP(P_CAIRN, sx, sy);
        addP(P_BOULDER, sx + 3, sy + 1, 1, 8, 14);
        addP(P_BOULDER, sx - 3, sy - 1, 1, 8, 14);
        spur((float)sx, (float)sy, 3, 3);
    }

    // ------------------------------------------------------------ 4. gate signs (home): one beside each gap, inside the wall
    if (home) {
        struct G {
            int kind, tx, ty;
        } gates[4] = {{P_BSIGN0, COX + CW + 2, COY + 26}, {P_BSIGN1, COX + 48, COY + CH + 2}, {P_BSIGN2, COX + 48, COY - 2}, {P_BSIGN3, COX - 3, COY + 26}};
        for (auto& g : gates) {
            clearDisc(g.tx, g.ty, 1.5f, 3);
            addP(g.kind, g.tx, g.ty);
        }
    }

    // ------------------------------------------------------------ 5. scatter trees, rocks, plants (biome maps)
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
                if (K(tx, ty) != 0 || solid[ty][tx] || (home && inCore(tx, ty))) continue;
                uint8_t t = tile[ty][tx];
                if (tileSolid(t)) continue;
                const float fx = tx * TS, fy = ty * TS;
                auto dx = [&]() { return fx + r.range(2, 14); };
                auto dy = [&]() { return fy + r.range(6, 15); };
                if (home) {  // the home forest outside the original area: same recipe as inside it
                    float ff = fbm(tx * 0.08f, ty * 0.08f, S + 1);
                    float pTree = 0.014f + 0.36f * cl((ff - 0.5f) / 0.2f, 0, 1);
                    if (r.chance(pTree)) tileProp(t == T_DARK ? (r.chance(0.55f) ? P_PINE : P_OAK0) : (r.chance(0.12f) ? P_PINE : (r.chance(0.5f) ? P_OAK0 : P_OAK1)), tx, ty);
                    else if (r.chance(0.007f)) rockProp(r.chance(0.6f) ? P_ROCK0 : P_ROCK1, tx, ty);
                    else if (r.chance(0.011f)) addDecor(6 + r.irange(0, 1), tx * TS + 8.f, ty * TS + 15.f);
                    else if (r.chance(0.085f)) {
                        float roll = r.f();
                        addDecor(roll < 0.3f ? r.irange(0, 2) : roll < 0.82f ? 3 : roll < 0.9f ? (t == T_DARK ? 4 : 3) : 5, dx(), dy());
                    }
                    continue;
                }
                switch (map) {
                    case B_DESERT: {
                        float pc = t == T_SAND ? 0.011f : (t == T_DUNE ? 0.004f : 0.f);
                        if (r.chance(pc)) {
                            tileProp(r.chance(0.55f) ? P_CACTUS0 : P_CACTUS1, tx, ty);
                            break;
                        }
                        if (r.chance(t == T_STONE ? 0.035f : 0.008f)) {
                            rockProp(r.chance(0.5f) ? P_SANDROCK0 : P_SANDROCK1, tx, ty);
                            break;
                        }
                        if (r.chance(0.035f)) addDecor(D_DRYBUSH, dx(), dy());
                        else if (r.chance(0.05f)) addDecor(D_SANDTUFT, dx(), dy());
                        else if (r.chance(0.0035f)) addDecor(D_SKULL, dx(), dy());
                        else if (r.chance(0.012f)) addDecor(D_PEBBLES, dx(), dy());
                        break;
                    }
                    case B_SWAMP: {
                        bool nearWater = tile[ty][tx - 1] == T_MURK || tile[ty][tx + 1] == T_MURK || tile[ty - 1][tx] == T_MURK || tile[ty + 1][tx] == T_MURK ||
                                         tile[ty][tx - 1] == T_BOG || tile[ty][tx + 1] == T_BOG || tile[ty - 1][tx] == T_BOG || tile[ty + 1][tx] == T_BOG;
                        if (t == T_MURK) {
                            if (r.chance(0.012f)) tileProp(P_DEADTREE1, tx, ty);
                            else if (r.chance(0.16f)) addDecor(r.chance(0.35f) ? D_REEDS1 : D_REEDS0, fx + r.range(3, 13), fy + r.range(8, 15));
                            else if (r.chance(0.07f)) addDecor(D_LILY, dx(), dy());
                            break;
                        }
                        if (r.chance(t == T_DARK ? 0.07f : 0.04f)) {
                            tileProp(r.chance(0.5f) ? P_DEADTREE0 : P_DEADTREE1, tx, ty);
                            break;
                        }
                        if (r.chance(0.004f)) {
                            props.push_back({P_LOG, fx + 8.f, fy + 14.f, 0});
                            solid[ty][tx] = true;
                            break;
                        }
                        if (t == T_DARK && r.chance(0.06f)) addDecor(D_BUSH0 + r.irange(0, 1), fx + 8.f, fy + 15.f);
                        else if (nearWater && r.chance(0.22f)) addDecor(r.chance(0.3f) ? D_REEDS1 : D_REEDS0, fx + r.range(3, 13), fy + r.range(8, 15));
                        else if (r.chance(0.035f)) addDecor(D_REEDS0, fx + r.range(3, 13), fy + r.range(8, 15));
                        else if (t == T_MUD && r.chance(0.025f)) addDecor(D_BUBBLES, dx(), dy());
                        else if (t == T_DARK && r.chance(0.035f)) addDecor(D_MUSHROOM, dx(), dy());
                        else if (t == T_DARK && r.chance(0.09f)) addDecor(D_TUFT, dx(), dy());
                        break;
                    }
                    case B_SNOW: {
                        if (t == T_ICE) break;
                        float ff = fbm(tx * 0.07f, ty * 0.07f, S + 303);
                        float pTree = 0.012f + 0.30f * cl((ff - 0.5f) / 0.2f, 0, 1);
                        if (r.chance(pTree)) {
                            tileProp(r.chance(0.82f) ? P_SNOWPINE : P_BARETREE, tx, ty);
                            break;
                        }
                        if (r.chance(0.007f)) {
                            rockProp(P_SNOWROCK, tx, ty);
                            break;
                        }
                        if (r.chance(0.0025f)) {
                            tileProp(P_CRYSTAL, tx, ty);
                            break;
                        }
                        if (r.chance(0.06f)) addDecor(D_DRIFT, dx(), dy());
                        else if (r.chance(0.05f)) addDecor(D_FROSTTUFT, dx(), dy());
                        else if (r.chance(0.008f)) addDecor(D_PEBBLES, dx(), dy());
                        break;
                    }
                    default: {  // mountains
                        float ff = fbm(tx * 0.06f, ty * 0.06f, S + 405);
                        bool meadow = t != T_STONE;
                        float pPine = meadow ? 0.07f + 0.15f * cl((ff - 0.5f) / 0.2f, 0, 1) : (ff > 0.55f ? 0.03f : 0.004f);
                        if (r.chance(pPine)) {
                            tileProp(P_PINE, tx, ty);
                            break;
                        }
                        if (!meadow && r.chance(0.011f)) {
                            rockProp(P_BOULDER, tx, ty);
                            break;
                        }
                        if (!meadow && r.chance(0.06f)) addDecor(D_GRAVEL, dx(), dy());
                        else if (r.chance(0.045f)) addDecor(D_ALPINE, dx(), dy());
                        else if (meadow && r.chance(0.08f)) addDecor(D_TUFT, dx(), dy());
                        else if (meadow && r.chance(0.04f)) addDecor(r.irange(0, 2), dx(), dy());
                        break;
                    }
                }
            }

    // ------------------------------------------------------------ 6. the map's edge wall (biome maps; home has its own tree wall)
    for (int ty = 0; ty < MH; ty++)
            for (int tx = 0; tx < MW; tx++) {
                if (!inRing(tx, ty) || K(tx, ty) == 1) continue;  // the gate gap stays open
                solid[ty][tx] = true;
                if (!r.chance(0.8f)) continue;
                float px = tx * TS + 8.f + r.range(-2, 2), py = ty * TS + 15.f, ph = r.f() * 6.28f;
                switch (map) {
                    case B_DESERT: props.push_back({r.chance(0.5f) ? P_SANDROCK0 : P_SANDROCK1, px, py, ph}); break;
                    case B_SWAMP: props.push_back({r.chance(0.5f) ? P_DEADTREE0 : P_DEADTREE1, px, py, ph}); break;
                    case B_SNOW: props.push_back({P_SNOWPINE, px, py, ph}); break;
                    case B_HOME: props.push_back({r.chance(0.6f) ? P_PINE : P_OAK0, px, py, ph}); break;
                    default: props.push_back({r.chance(0.6f) ? P_BOULDER : P_PINE, px, py, ph}); break;
                }
            }

    // ------------------------------------------------------------ 7. resource nodes
    auto nearCliff = [&](int tx, int ty, int rad) {
        for (int y = ty - rad; y <= ty + rad; y++)
            for (int x = tx - rad; x <= tx + rad; x++)
                if (inMap(x, y) && tile[y][x] == T_CLIFF) return true;
        return false;
    };
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
    // a solid vein needs open ground on every side, or a pocket of cliff can wall a player out of it
    auto openAround = [&](int tx, int ty) {
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int x = tx + dx, y = ty + dy;
                if (x < 0 || y < 0 || x >= MW || y >= MH || tile[y][x] == T_CLIFF || solid[y][x]) return false;
            }
        return true;
    };
    auto scatter = [&](int kind, int patches, const std::function<bool(int, int)>& ok, float spacing = 6.f) {
        for (int p = 0; p < patches; p++)
            for (int att = 0; att < 4000; att++) {
                int tx = r.irange(3, MW - 4), ty = r.irange(3, MH - 4);
                if (!nodeFree(tx, ty) || !ok(tx, ty)) continue;
                float px = tx * TS + 8.f, py = ty * TS + 12.f;
                if (!farFromNodes(px, py, spacing * TS)) continue;
                int n = r.irange(1, 3);
                for (int i = 0; i < n; i++) {
                    int ntx = tx + (i == 0 ? 0 : r.irange(-1, 1)), nty = ty + (i == 0 ? 0 : r.irange(-1, 1));
                    if (!nodeFree(ntx, nty) || !ok(ntx, nty)) continue;
                    float nx = ntx * TS + 8.f + r.range(-3, 3), ny = nty * TS + 12.f + r.range(-1, 2);
                    if (!farFromNodes(nx, ny, 10.f)) continue;
                    nodes.push_back({kind, nx, ny, r.f() * 6.28f});
                    if (kind == RN_CACTUS || kind == RN_ORE || kind == RN_COPPER || kind == RN_GOLD) solid[nty][ntx] = true;  // rocks and cacti block the way
                }
                break;
            }
    };
    // patch counts are the original biome's, scaled to the size of one map; rarer things are sparser and need better tools
    if (map == B_DESERT) {
        scatter(RN_CACTUS, 32, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_DUNE; });
        scatter(RN_DESERT_HERB, 28, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_STONE; });
        scatter(RN_DRYWOOD, 28, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_DUNE; }, 5.f);
        scatter(RN_COPPER, 22, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_STONE; });
    } else if (map == B_SWAMP) {
        scatter(RN_REED, 32, [&](int x, int y) { return tile[y][x] == T_MURK || tile[y][x] == T_MUD; });
        scatter(RN_BOGWORT, 28, [&](int x, int y) { return tile[y][x] == T_MUD || tile[y][x] == T_DARK; });
        scatter(RN_NIGHTSHADE, 22, [&](int x, int y) { return tile[y][x] == T_MUD || tile[y][x] == T_DARK; }, 5.f);
        scatter(RN_AMBER, 12, [&](int x, int y) { return tile[y][x] == T_MUD; }, 12.f);
    } else if (map == B_SNOW) {
        scatter(RN_FROSTBLOOM, 44, [&](int x, int y) { return tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP; });
        scatter(RN_ORE, 14, [&](int x, int y) { return tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP; }, 10.f);
    } else if (map == B_MOUNTAIN) {
        scatter(RN_ALPINE_HERB, 28, [&](int x, int y) { return tile[y][x] != T_CLIFF; });
        scatter(RN_ORE, 36, [&](int x, int y) { return tile[y][x] == T_STONE && nearCliff(x, y, 2); });
        scatter(RN_COPPER, 24, [&](int x, int y) { return tile[y][x] == T_STONE; }, 5.f);
        scatter(RN_GOLD, 12, [&](int x, int y) { return tile[y][x] == T_STONE && nearCliff(x, y, 4) && openAround(x, y); }, 7.f);
    }

    // ---- the home forest: herbs, mushrooms, fallen branches and flax. Placed with their own random stream after everything else,
    // so the original home map (terrain, trees, rocks, decor) is exactly what it always was.
    if (home) {
        Rng hr(seed * 104729u + 31u);
        auto homeOk = [&](int tx, int ty) {
            const int cx = tx - COX, cy = ty - COY;
            if (cx < 4 || cy < 4 || cx >= CW - 4 || cy >= CH - 4 || K(tx, ty) != 0) return false;
            if (hypotf(cx - 44.f, (cy - 30.f) * 1.15f) < 11.f) return false;  // the village plaza stays clear
            return passable(tx, ty) && passable(tx + 1, ty) && passable(tx - 1, ty) && passable(tx, ty + 1) && passable(tx, ty - 1) && tile[ty][tx] != T_DIRT;
        };
        auto homeScatter = [&](int kind, int count, const std::function<bool(int, int)>& ok) {
            int placed = 0;
            for (int att = 0; att < count * 60 && placed < count; att++) {
                int tx = hr.irange(COX + 4, COX + CW - 5), ty = hr.irange(COY + 4, COY + CH - 5);
                if (!homeOk(tx, ty) || !ok(tx, ty)) continue;
                float nx = tx * TS + 8.f + hr.range(-3, 3), ny = ty * TS + 12.f + hr.range(-1, 2);
                if (!farFromNodes(nx, ny, 3.f * TS)) continue;
                nodes.push_back({kind, nx, ny, hr.f() * 6.28f});
                placed++;
            }
        };
        homeScatter(RN_HERB, 34, [&](int x, int y) { return tile[y][x] == T_GRASS || tile[y][x] == T_DARK; });
        homeScatter(RN_MUSHROOM, 26, [&](int x, int y) { return tile[y][x] == T_DARK; });
        homeScatter(RN_BRANCH, 36, [&](int x, int y) { return tile[y][x] == T_GRASS || tile[y][x] == T_DARK; });
        homeScatter(RN_FLAX, 28, [&](int x, int y) { return tile[y][x] == T_GRASS; });
        // and the same plants through the home forest around the original area
        auto outer = [&](int x, int y) { return !inCore(x, y); };
        scatter(RN_HERB, 70, [&](int x, int y) { return outer(x, y) && (tile[y][x] == T_GRASS || tile[y][x] == T_DARK); }, 4.f);
        scatter(RN_MUSHROOM, 40, [&](int x, int y) { return outer(x, y) && tile[y][x] == T_DARK; }, 4.f);
        scatter(RN_BRANCH, 70, [&](int x, int y) { return outer(x, y) && (tile[y][x] == T_GRASS || tile[y][x] == T_DARK); }, 4.f);
        scatter(RN_FLAX, 50, [&](int x, int y) { return outer(x, y) && tile[y][x] == T_GRASS; }, 4.f);
        generateTown();  // last: the town is laid over the middle of the original area
    }
}

// ------------------------------------------------------------------------------------------------------------------------------
// The central town (town.hpp). Laid over the middle of the original Wildwood area after everything else is generated, so the surrounding
// terrain, forest and resources are exactly as before: only the town's own rectangle is cleared (trees, rocks, plants and the old hut go),
// paved with streets and filled with the six buildings and their street furniture. Streets run to the plaza and join the old dirt paths.
inline void World::generateTown() {
    const int X0 = COX + TOWN_X0, Y0 = COY + TOWN_Y0, X1 = COX + TOWN_X1, Y1 = COY + TOWN_Y1;  // map tiles, X1 / Y1 exclusive
    auto inRect = [&](float px, float py) { return px >= X0 * TS && px < X1 * TS && py > Y0 * TS && py <= Y1 * TS; };
    // 1. clear the rectangle: props (the campfire stays), decor, resource nodes, solid marks, water
    props.erase(std::remove_if(props.begin(), props.end(), [&](const Prop& p) { return p.kind != P_FIRE && inRect(p.x, p.y); }), props.end());
    decor.erase(std::remove_if(decor.begin(), decor.end(), [&](const Decor& d) { return inRect(d.x, d.y); }), decor.end());
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const ResNode& n) { return inRect(n.x, n.y); }), nodes.end());
    for (int y = Y0; y < Y1; y++)
        for (int x = X0; x < X1; x++) {
            solid[y][x] = false;
            if (tile[y][x] == T_WATER || tile[y][x] == T_DIRT) tile[y][x] = T_GRASS;
        }
    markSolidAt(fire.x, fire.y - 3);
    // 2. streets: two east-west streets in front of the building rows, a north-south road through the middle, a paved plaza
    auto paved = [&](int cx0, int cy0, int cx1, int cy1) {  // core tile rectangle, inclusive
        for (int y = COY + cy0; y <= COY + cy1; y++)
            for (int x = COX + cx0; x <= COX + cx1; x++)
                if (x >= 2 && y >= 2 && x < MW - 2 && y < MH - 2 && tile[y][x] != T_WATER) tile[y][x] = T_COBBLE;
    };
    paved(TOWN_X0, 24, TOWN_X1 - 1, 26);   // street A, in front of the north row
    paved(TOWN_X0, 37, TOWN_X1 - 1, 38);   // street B, in front of the south row
    paved(43, TOWN_Y0, 44, TOWN_Y1 - 1);   // the road through the middle
    paved(38, 27, 50, 33);                 // the plaza around the campfire
    // the four old dirt paths run out of the town: pave their first stretch so they meet the streets
    for (int y = COY + 8; y < COY + TOWN_Y0; y++)
        for (int x = COX + 43; x <= COX + 44; x++) tile[y][x] = T_COBBLE;
    for (int y = COY + TOWN_Y1; y < COY + 52; y++)
        for (int x = COX + 43; x <= COX + 44; x++) tile[y][x] = T_COBBLE;
    for (int x = COX + 12; x < COX + TOWN_X0; x++)
        for (int y = COY + 24; y <= COY + 26; y++) tile[y][x] = T_COBBLE;
    for (int x = COX + TOWN_X1; x < COX + 76; x++)
        for (int y = COY + 24; y <= COY + 26; y++) tile[y][x] = T_COBBLE;
    // 3. the buildings
    for (int k = 0; k < BLD_COUNT; k++) {
        props.push_back({P_BLD0 + k, bldCenterX(k, COX), bldBaseY(k, COY), 0});
        for (int y = 0; y < BLD_H; y++)
            for (int x = 0; x < BLD_W; x++) solid[COY + BUILDINGS[k].cy + y][COX + BUILDINGS[k].cx + x] = true;
        // a lamp by every door, lit at night
        const float lx = bldCenterX(k, COX) + 34.f, ly = bldBaseY(k, COY) + 12.f;
        props.push_back({P_LAMP, lx, ly, 0});
        lanterns.push_back({lx, ly - 24.f, 0.8f});
    }
    // 4. street furniture: lamps along both streets, fences behind the north row and along the south edge (gaps for the roads), benches, planters, the well, trees
    auto at = [&](int cx, float cyf) { return V2((COX + cx) * (float)TS + 8.f, (COY + cyf) * (float)TS); };
    for (int cx : {28, 36, 41, 46, 52, 60}) {
        const V2 a = at(cx, 27.f);
        props.push_back({P_LAMP, a.x, a.y - 2.f, 0});
        lanterns.push_back({a.x, a.y - 26.f, 0.75f});
    }
    for (int cx : {28, 40, 47, 56}) {
        const V2 a = at(cx, 39.f);
        props.push_back({P_LAMP, a.x, a.y - 2.f, 0});
        lanterns.push_back({a.x, a.y - 26.f, 0.75f});
    }
    for (int cx = TOWN_X0; cx < TOWN_X1; cx++) {
        if (cx >= 42 && cx <= 45) continue;  // the road
        for (int cy : {18, 42}) {
            const V2 a = at(cx, cy + 0.9f);
            props.push_back({P_FENCE, a.x, a.y, 0});
            solid[COY + cy][COX + cx] = true;
        }
    }
    const int bench[][2] = {{40, 28}, {47, 33}, {40, 33}, {50, 28}};
    for (auto& b : bench) {
        const V2 a = at(b[0], b[1] + 0.9f);
        props.push_back({P_BENCH, a.x + 4.f, a.y, 0});
        solid[COY + b[1]][COX + b[0]] = true;
    }
    const int planters[][2] = {{29, 37}, {36, 37}, {51, 37}, {58, 37}, {27, 27}, {62, 27}};
    for (auto& b : planters) {
        const V2 a = at(b[0], b[1] + 0.95f);
        props.push_back({P_PLANTER, a.x, a.y, 0});
        solid[COY + b[1]][COX + b[0]] = true;
    }
    {
        const V2 a = at(38, 31.9f);
        props.push_back({P_WELL, a.x + 8.f, a.y, 0});
        solid[COY + 31][COX + 38] = solid[COY + 31][COX + 39] = true;
    }
    {   // town signs where the streets leave the town
        const V2 w = at(TOWN_X0 - 1, 28.6f), e = at(TOWN_X1, 28.6f);
        props.push_back({P_SIGN, w.x, w.y, 0});
        props.push_back({P_SIGN, e.x, e.y, 0});
        markSolidAt(w.x, w.y - 2.f);
        markSolidAt(e.x, e.y - 2.f);
    }
    Rng tr(seed * 31337u + 4);
    const int trees[][2] = {{26, 31}, {61, 31}, {25, 40}, {62, 40}, {35, 18}, {52, 18}, {27, 17}, {60, 17}};
    for (auto& t : trees) {
        const V2 a = at(t[0], t[1] + 0.9f);
        props.push_back({tr.chance(0.5f) ? P_OAK0 : P_PINE, a.x, a.y, tr.f() * 6.28f});
        solid[COY + t[1]][COX + t[0]] = true;
    }
    // flowers along the plaza edge (decor)
    for (int i = 0; i < 40; i++) {
        const int cx = 24 + (int)(tr.f() * 40), cy = 27 + (int)(tr.f() * 7);
        if (tile[COY + cy][COX + cx] == T_COBBLE || solid[COY + cy][COX + cx]) continue;
        decor.push_back({tr.irange(0, 2), (COX + cx) * (float)TS + tr.range(2, 14), (COY + cy) * (float)TS + tr.range(6, 15), tr.f() * 6.28f});
    }
    // 5. where things are now: the hunter starts on the plaza; the old shop hut is gone (the General Store is a building)
    spawn = V2((COX + 43.5f) * TS, (COY + 30.f) * TS);
    questGiver = V2((COX + 45.75f) * TS, (COY + 31.6f) * TS);
    hut = keeper = V2(-99999.f, -99999.f);
    door = V2(bldDoorX(BLD_SHOP, COX), bldDoorY(BLD_SHOP, COY));
}
