// worldgen.hpp - builds the four biomes around the original map. Included at the end of world.hpp.
//
// Layout (tile coordinates, world is 240x168, the home area occupies x 76..163, y 54..113):
//   north: SNOWFIELDS   east: DESERT   south: SWAMP   west: MOUNTAINS
// Borders between biomes follow the map diagonals, warped by noise and speckled with the neighbour's ground.
// The home area's four dirt paths continue through gates in its tree wall to a "hub" landmark in each biome,
// with spur trails to the other landmarks. Everything is deterministic for a given world seed.
#pragma once
#include <functional>
#include <vector>

inline void World::generateBiomes() {
    Rng r(seed * 104729u + 977u);
    const uint32_t S = seed;
    auto inCore = [](int x, int y) { return x >= COX && x < COX + CW && y >= COY && y < COY + CH; };
    auto inMap = [](int x, int y) { return x >= 0 && y >= 0 && x < MW && y < MH; };
    auto inRing = [](int x, int y) { return x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2; };  // map-edge wall
    auto cl = [](float v, float a, float b) { return v < a ? a : (v > b ? b : v); };

    // keep mask: 1 = trail tile, 2 = cleared zone (no scattered props / solid terrain), 3 = landmark feature
    std::vector<uint8_t> keep((size_t)MW * MH, 0);
    auto K = [&](int x, int y) -> uint8_t& { return keep[(size_t)y * MW + x]; };

    static const int bmap[4] = {B_DESERT, B_SWAMP, B_SNOW, B_MOUNTAIN};  // index = direction (E, S, N, W)
    auto baseGround = [](int b) -> uint8_t {
        switch (b) {
            case B_DESERT: return T_SAND;
            case B_SWAMP: return T_MUD;
            case B_SNOW: return T_SNOW;
            case B_MOUNTAIN: return T_STONE;
            default: return T_GRASS;
        }
    };

    // ------------------------------------------------------------ 1. which biome is where
    auto classify = [&](int x, int y, int& best, int& second, float& margin) {
        float dx = (x + 0.5f - MW / 2.f) / (MW / 2.f), dy = (y + 0.5f - MH / 2.f) / (MH / 2.f);
        dx += (fbm(x * 0.045f, y * 0.045f, S + 500) - 0.5f) * 0.42f;
        dy += (fbm(x * 0.045f + 31.f, y * 0.045f + 17.f, S + 501) - 0.5f) * 0.42f;
        float sc[4] = {dx, dy, -dy, -dx};
        best = 0;
        for (int i = 1; i < 4; i++)
            if (sc[i] > sc[best]) best = i;
        second = best == 0 ? 1 : 0;
        for (int i = 0; i < 4; i++)
            if (i != best && sc[i] > sc[second]) second = i;
        margin = sc[best] - sc[second];
    };

    // ground of each biome at a tile
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
            default: {  // mountains: cliff massifs, thin ridge lines with gaps, alpine meadows near the home area
                float alt = 1.f - std::min(1.f, x / (float)COX);  // 0 next to home, 1 at the far west edge
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

    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            if (inCore(x, y)) continue;
            int best, second;
            float margin;
            classify(x, y, best, second, margin);
            biome[y][x] = (uint8_t)bmap[best];
            int use = best;
            if (margin < 0.12f) {  // speckle the neighbour's ground across the border
                float t = (hash2(x, y, S + 9) & 0xFFFF) / 65535.f;
                if (t < (0.12f - margin) / 0.24f) use = second;
            }
            tile[y][x] = groundFor(bmap[use], x, y);
        }
    // soft border around the home area: a band of home-style grass thins out into the biome's own ground
    std::vector<uint8_t> trans((size_t)MW * MH, 0);
    {
        const int BL = 8;
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                if (inCore(x, y)) continue;
                int dxc = x < COX ? COX - x : (x >= COX + CW ? x - (COX + CW - 1) : 0);
                int dyc = y < COY ? COY - y : (y >= COY + CH ? y - (COY + CH - 1) : 0);
                int dc = std::max(dxc, dyc);
                if (dc < 1 || dc > BL) continue;
                float pr = (1.f - (dc - 1) / (float)BL) * 0.9f;
                if ((hash2(x, y, S + 13) & 0xFFFF) / 65535.f < pr) {
                    tile[y][x] = fbm(x * 0.08f, y * 0.08f, S + 1) > 0.6f ? T_DARK : T_GRASS;
                    trans[(size_t)y * MW + x] = 1;
                }
            }
    }
    // bog shores are shallow murk, not a hard edge
    for (int y = 1; y < MH - 1; y++)
        for (int x = 1; x < MW - 1; x++)
            if (tile[y][x] == T_BOG) {
                const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& d : d4) {
                    uint8_t& n = tile[y + d[1]][x + d[0]];
                    if (!inCore(x + d[0], y + d[1]) && (n == T_MUD || n == T_DARK)) n = T_MURK;
                }
            }
    // frozen lakes
    {
        struct Lk {
            float cx, cy, rx, ry;
        } lk[] = {{84, 24, 11, 6}, {158, 30, 9, 5}, {190, 12, 8, 4.5f}, {56, 12, 7, 4}};
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                if (biome[y][x] != B_SNOW || inCore(x, y)) continue;
                for (auto& l : lk) {
                    float dx = (x - l.cx) / l.rx, dy = (y - l.cy) / l.ry;
                    float d = dx * dx + dy * dy + (fbm(x * 0.22f, y * 0.22f, S + 307) - 0.5f) * 0.7f;
                    if (d < 1.f) tile[y][x] = T_ICE;
                }
            }
    }

    // ------------------------------------------------------------ 2. trails, hubs, gates
    const V2 hubT[B_COUNT] = {V2(MW / 2, MH / 2), V2(206, 84), V2(120, 146), V2(120, 24), V2(34, 84)};  // tile coords
    for (int b = 0; b < B_COUNT; b++) hub[b] = V2(hubT[b].x * TS + 8, hubT[b].y * TS + 8);

    auto stampTrail = [&](int x, int y) {
        if (!inMap(x, y) || inRing(x, y)) return;
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
                if (!inCore(x, y) && tileSolid(tile[y][x])) tile[y][x] = baseGround(biome[y][x]);
            }
    };
    auto stampBlock = [&](int px, int py) {
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++) stampTrail(px + a, py + b);
        clearZone(px, py, 2);
        clearZone(px + 1, py + 1, 2);
    };
    // The four original paths, continued outward with the same wiggle formula.
    {
        const float vx = COX + 44, vy = COY + 30;
        const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        const int maxI[4] = {86, 86, 62, 60};  // east, west, south, north
        for (int k = 0; k < 4; k++)
            for (int i = 30; i <= maxI[k]; i++) {
                float wig = sinf(i * 0.37f + k * 2.1f) * 1.6f;
                int px = (int)(vx + dirs[k][0] * i + (dirs[k][0] ? 0 : wig));
                int py = (int)(vy + dirs[k][1] * i + (dirs[k][1] ? 0 : wig));
                stampBlock(px, py);
            }
    }
    // hub plazas
    for (int b = B_DESERT; b < B_COUNT; b++) {
        int hx = (int)hubT[b].x, hy = (int)hubT[b].y;
        for (int dy = -4; dy <= 4; dy++)
            for (int dx = -4; dx <= 4; dx++)
                if (dx * dx + dy * dy <= 12) stampTrail(hx + dx, hy + dy);
        clearZone(hx, hy, 6);
    }
    // open the home area's tree wall where the trails pass through it
    for (size_t i = 0; i < props.size();) {
        const Prop& p = props[i];
        int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
        bool tree = p.kind == P_OAK0 || p.kind == P_OAK1 || p.kind == P_PINE || p.kind == P_ROCK0 || p.kind == P_ROCK1;
        if (inCore(tx, ty) && K(tx, ty) != 0 && tree) {
            solid[ty][tx] = false;
            props.erase(props.begin() + i);
        } else
            i++;
    }
    for (size_t i = 0; i < decor.size();) {
        int tx = (int)(decor[i].x / TS), ty = (int)(decor[i].y / TS);
        if (inCore(tx, ty) && K(tx, ty) == 1) decor.erase(decor.begin() + i);
        else i++;
    }

    // ------------------------------------------------------------ helpers for landmarks
    auto clearDisc = [&](float cx, float cy, float rad, uint8_t kv) {
        for (int y = (int)(cy - rad) - 1; y <= (int)(cy + rad) + 1; y++)
            for (int x = (int)(cx - rad) - 1; x <= (int)(cx + rad) + 1; x++) {
                if (!inMap(x, y) || inRing(x, y) || inCore(x, y) || K(x, y) == 1) continue;
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
    // trail from a hub towards a landmark, stopping `reach` tiles short of its centre
    auto spur = [&](int b, float tx, float ty, float reach, int sk) {
        float hx = hubT[b].x, hy = hubT[b].y, dx = tx - hx, dy = ty - hy, len = sqrtf(dx * dx + dy * dy);
        if (len <= reach + 1) return;
        carveLine(hx, hy, tx - dx / len * reach, ty - dy / len * reach, 1.4f, sk);
    };

    // ------------------------------------------------------------ 3. desert landmarks
    {
        const int hx = 206, hy = 84;
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
        const float cx = 188, cy = 56, rx = 5.5f, ry = 3.8f;
        addLandmark("OASIS", B_DESERT, cx, cy + 6);
        clearDisc(cx, cy, 11, 2);
        for (int y = (int)(cy - 12); y <= (int)(cy + 12); y++)
            for (int x = (int)(cx - 14); x <= (int)(cx + 14); x++) {
                if (!inMap(x, y) || inRing(x, y) || inCore(x, y) || K(x, y) == 1) continue;
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
        const float bx = 222, by = 112;
        addLandmark("BONE FIELD", B_DESERT, bx, by);
        clearDisc(bx, by, 6, 2);
        for (int i = 0; i < 16; i++) {
            float a = r.range(0, 6.28f), d = r.range(0.5f, 5.2f);
            addDecor(r.chance(0.55f) ? D_SKULL : D_PEBBLES, (bx + cosf(a) * d) * TS + 8, (by + sinf(a) * d) * TS + 8);
        }
        addP(P_SANDROCK1, (int)bx - 4, (int)by - 2, 1, 8, 14);
        addP(P_SANDROCK0, (int)bx + 4, (int)by + 3, 1, 8, 14);
        // sandstone arch
        const int ax = 214, ay = 38;
        addLandmark("SANDSTONE ARCH", B_DESERT, ax, ay + 2);
        clearDisc(ax, ay, 5, 2);
        props.push_back({P_ARCH, ax * TS + 8.f, ay * TS + 15.f, 0});
        solid[ay][ax - 1] = solid[ay][ax + 1] = true;
        K(ax - 1, ay) = K(ax + 1, ay) = 3;
        spur(B_DESERT, cx, cy, 9, 1);
        spur(B_DESERT, bx, by, 3, 2);
        spur(B_DESERT, ax, ay, 3, 3);
    }

    // ------------------------------------------------------------ 4. swamp landmarks
    {
        const int hx = 120, hy = 146;
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
        const int wx = 88, wy = 138;
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
        const int gx = 158, gy = 144;
        addLandmark("SUNKEN GRAVES", B_SWAMP, gx, gy);
        clearDisc(gx, gy, 7, 2);
        for (int row = 0; row < 2; row++)
            for (int c = 0; c < 4; c++) addP(P_GRAVE, gx - 4 + c * 2 + (row ? 1 : 0), gy - 1 + row * 3);
        spur(B_SWAMP, wx, wy + 3, 3, 1);
        spur(B_SWAMP, gx, gy, 4, 2);
    }

    // ------------------------------------------------------------ 5. snow landmarks
    {
        const int hx = 120, hy = 24;
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
                if (!inMap(x, y) || K(x, y) == 1) continue;
                float dx = (x - (hx - 9.5f)) / 4.6f, dy = (y - (hy + 4.5f)) / 3.0f;
                if (dx * dx + dy * dy < 1.f) {
                    tile[y][x] = T_ICE;
                    K(x, y) = 3;
                }
            }
        // frozen lake landmark (the ice itself is generated above); crystals along its shore
        addLandmark("FROZEN LAKE", B_SNOW, 84, 31);
        int placed = 0;
        for (int i = 0; i < 40 && placed < 7; i++) {
            float a = r.range(0, 6.28f);
            int px = (int)(84 + cosf(a) * 12.5f), py = (int)(24 + sinf(a) * 7.2f);
            if (inMap(px, py) && !inRing(px, py) && K(px, py) == 0 && (tile[py][px] == T_SNOW || tile[py][px] == T_SNOWP) && !solid[py][px]) {
                addP(P_CRYSTAL, px, py);
                placed++;
            }
        }
        // watch cairn
        const int cx = 150, cy = 42;
        addLandmark("WATCH CAIRN", B_SNOW, cx, cy + 2);
        clearDisc(cx, cy, 5, 2);
        addP(P_CAIRN, cx, cy);
        addP(P_SNOWROCK, cx - 3, cy + 1, 1, 8, 14);
        addP(P_SNOWROCK, cx + 3, cy - 1, 1, 8, 14);
        spur(B_SNOW, 84, 31, 4, 1);
        spur(B_SNOW, cx, cy, 3, 2);
    }

    // ------------------------------------------------------------ 6. mountain landmarks
    {
        const int hx = 34, hy = 84;
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
                    if (!inMap(x, y) || inRing(x, y) || inCore(x, y) || K(x, y) == 1) continue;
                    if (y == ty - 3 && (x == tx - 4 || x == tx + 4)) continue;  // rounded corners
                    tile[y][x] = T_CLIFF;
                    solid[y][x] = true;
                    K(x, y) = 3;
                }
            props.push_back({P_CAVE, tx * TS + 8.f, (y1 + 1) * TS + 0.f, 0});
            addLandmark(name, B_MOUNTAIN, tx, y1 + 2);
            // trail: hub -> a waypoint south of the outcrop -> the cave mouth
            carveLine(hubT[B_MOUNTAIN].x, hubT[B_MOUNTAIN].y, (float)tx, (float)(y1 + 5), 1.4f, sk);
            carveLine((float)tx, (float)(y1 + 5), (float)tx, (float)(y1 + 1), 0.f, sk);
        };
        makeCave(20, 54, "ECHO CAVE", 1);
        makeCave(18, 112, "HOLLOW CAVE", 2);
        // summit cairn
        const int sx = 12, sy = 66;
        addLandmark("SUMMIT CAIRN", B_MOUNTAIN, sx, sy + 2);
        clearDisc(sx, sy, 5, 2);
        addP(P_CAIRN, sx, sy);
        addP(P_BOULDER, sx + 3, sy + 1, 1, 8, 14);
        addP(P_BOULDER, sx - 3, sy - 1, 1, 8, 14);
        spur(B_MOUNTAIN, sx, sy, 3, 3);
    }

    // ------------------------------------------------------------ 7. gate signs
    {
        struct G {
            int kind, tx, ty;
        } gates[4] = {{P_BSIGN0, COX + CW + 2, COY + 30 - 4}, {P_BSIGN1, COX + 44 + 4, COY + CH + 2},
                      {P_BSIGN2, COX + 44 + 4, COY - 2}, {P_BSIGN3, COX - 3, COY + 30 - 4}};
        for (auto& g : gates) {
            clearDisc(g.tx, g.ty, 1.5f, 3);
            addP(g.kind, g.tx, g.ty);
        }
    }

    // ------------------------------------------------------------ 8. scatter trees, rocks, plants
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
            if (inCore(tx, ty) || K(tx, ty) != 0 || solid[ty][tx]) continue;
            uint8_t t = tile[ty][tx];
            if (tileSolid(t)) continue;
            const float fx = tx * TS, fy = ty * TS;
            auto dx = [&]() { return fx + r.range(2, 14); };
            auto dy = [&]() { return fy + r.range(6, 15); };
            if (trans[(size_t)ty * MW + tx]) {  // border band: the home forest's own trees and flowers
                if (r.chance(0.09f)) tileProp(r.chance(0.5f) ? P_OAK0 : (r.chance(0.5f) ? P_OAK1 : P_PINE), tx, ty);
                else if (r.chance(0.05f)) addDecor(D_TUFT, dx(), dy());
                else if (r.chance(0.02f)) addDecor(r.irange(0, 2), dx(), dy());
                continue;
            }
            int pb = (t == T_SAND || t == T_DUNE) ? B_DESERT
                     : (t == T_MUD || t == T_MURK) ? B_SWAMP
                     : (t == T_SNOW || t == T_SNOWP || t == T_ICE) ? B_SNOW
                                                                  : (int)biome[ty][tx];
            switch (pb) {
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

    // ------------------------------------------------------------ 9. the world's edge wall
    for (int ty = 0; ty < MH; ty++)
        for (int tx = 0; tx < MW; tx++) {
            if (!inRing(tx, ty)) continue;
            solid[ty][tx] = true;
            if (!r.chance(0.8f)) continue;
            float px = tx * TS + 8.f + r.range(-2, 2), py = ty * TS + 15.f, ph = r.f() * 6.28f;
            switch (biome[ty][tx]) {
                case B_DESERT: props.push_back({r.chance(0.5f) ? P_SANDROCK0 : P_SANDROCK1, px, py, ph}); break;
                case B_SWAMP: props.push_back({r.chance(0.5f) ? P_DEADTREE0 : P_DEADTREE1, px, py, ph}); break;
                case B_SNOW: props.push_back({P_SNOWPINE, px, py, ph}); break;
                default: props.push_back({r.chance(0.6f) ? P_BOULDER : P_PINE, px, py, ph}); break;
            }
        }

    // ------------------------------------------------------------ 10. resource nodes
    auto nearCliff = [&](int tx, int ty, int rad) {
        for (int y = ty - rad; y <= ty + rad; y++)
            for (int x = tx - rad; x <= tx + rad; x++)
                if (inMap(x, y) && tile[y][x] == T_CLIFF) return true;
        return false;
    };
    auto passable = [&](int tx, int ty) { return inMap(tx, ty) && !solid[ty][tx] && !tileSolid(tile[ty][tx]); };
    auto nodeFree = [&](int tx, int ty) {
        if (inCore(tx, ty) || inRing(tx, ty) || K(tx, ty) != 0) return false;
        return passable(tx, ty) && passable(tx + 1, ty) && passable(tx - 1, ty) && passable(tx, ty + 1) && passable(tx, ty - 1);
    };
    auto farFromNodes = [&](float x, float y, float minD) {
        for (auto& n : nodes)
            if (hypotf(n.x - x, n.y - y) < minD) return false;
        return true;
    };
    auto scatter = [&](int kind, int b, int patches, const std::function<bool(int, int)>& ok) {
        for (int p = 0; p < patches; p++)
            for (int att = 0; att < 500; att++) {
                int tx = r.irange(3, MW - 4), ty = r.irange(3, MH - 4);
                if (biome[ty][tx] != b || !nodeFree(tx, ty) || !ok(tx, ty)) continue;
                float px = tx * TS + 8.f, py = ty * TS + 12.f;
                if (!farFromNodes(px, py, 6 * TS)) continue;
                int n = r.irange(1, 3);
                for (int i = 0; i < n; i++) {
                    int ntx = tx + (i == 0 ? 0 : r.irange(-1, 1)), nty = ty + (i == 0 ? 0 : r.irange(-1, 1));
                    if (!nodeFree(ntx, nty) || !ok(ntx, nty)) continue;
                    float nx = ntx * TS + 8.f + r.range(-3, 3), ny = nty * TS + 12.f + r.range(-1, 2);
                    if (!farFromNodes(nx, ny, 10.f)) continue;
                    nodes.push_back({kind, nx, ny, r.f() * 6.28f});
                    if (kind == RN_CACTUS || kind == RN_ORE) solid[nty][ntx] = true;
                }
                break;
            }
    };
    scatter(RN_CACTUS, B_DESERT, 16, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_DUNE; });
    scatter(RN_DESERT_HERB, B_DESERT, 14, [&](int x, int y) { return tile[y][x] == T_SAND || tile[y][x] == T_STONE; });
    scatter(RN_REED, B_SWAMP, 16, [&](int x, int y) { return tile[y][x] == T_MURK || tile[y][x] == T_MUD; });
    scatter(RN_BOGWORT, B_SWAMP, 14, [&](int x, int y) { return tile[y][x] == T_MUD || tile[y][x] == T_DARK; });
    scatter(RN_FROSTBLOOM, B_SNOW, 22, [&](int x, int y) { return tile[y][x] == T_SNOW || tile[y][x] == T_SNOWP; });
    scatter(RN_ALPINE_HERB, B_MOUNTAIN, 14, [&](int x, int y) { return tile[y][x] != T_CLIFF; });
    scatter(RN_ORE, B_MOUNTAIN, 18, [&](int x, int y) { return tile[y][x] == T_STONE && nearCliff(x, y, 2); });
}
