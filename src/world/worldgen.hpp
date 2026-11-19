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
#include "world/maps.hpp"
#include "art/tree_lime.hpp"
#include "art/tree_pine.hpp"

inline void World::generateMap(int map) {
    if (map == B_HOMESTEAD) {  // the player's sandbox has its own generator (homestead_gen.hpp)
        generateHomestead();
        return;
    }
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
                // thin ridges of snow-capped rock with gaps in them (the same recipe as the mountains' ridge lines); trails and plazas cut through
                const float ridge = fbm(x * 0.065f, y * 0.065f, S + 311), gap = fbm(x * 0.13f + 9.f, y * 0.13f + 9.f, S + 312);
                if (fabsf(ridge - 0.5f) < 0.012f && gap < 0.64f) return T_CLIFF;
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
    // The home forest's floor from two fields. DENSITY is the field that has always set the trees' density (S + 1; its high end is the dark
    // thickets); MOISTURE (S + 31) is new. A fine jitter frays the borders so patches do not come out as smooth blobs.
    //   dense + wet -> moss            dense + dry -> pine needles          dense, mid -> dark grass
    //   woodland edge + dry -> leaf litter (oak woods)    woodland edge + wet -> moss      the rest -> meadow grass
    // how deep inside a glade a tile is: < 1 is the open clearing, up to ~1.5 its thinned rim (the fine noise frays the edge). Returns the
    // lowest value over all glades; `which` is that glade (or -1). The glades themselves are chosen in the home block below.
    auto gladeQ = [&](int x, int y, int* which = nullptr) {
        float best = 99.f;
        int bi = -1;
        for (size_t i = 0; i < glades.size(); i++) {
            const Glade& g = glades[i];
            const float dx = (x - g.cx) / g.rx, dy = (y - g.cy) / g.ry;
            const float q0 = dx * dx + dy * dy;
            if (q0 > 3.2f) continue;
            const float q = q0 + (fbm(x * 0.28f, y * 0.28f, S + 57) - 0.5f) * 0.55f;
            if (q < best) { best = q; bi = (int)i; }
        }
        if (which) *which = bi;
        return best;
    };
    // the same idea for the flower fields: < 1 is the field itself (its edge is ragged, on a bigger scale than a glade's)
    auto fieldQ = [&](int x, int y) {
        float best = 99.f;
        for (const FlowerField& f : fields) {
            const float dx = (x - f.cx) / f.rx, dy = (y - f.cy) / f.ry;
            const float q0 = dx * dx + dy * dy;
            if (q0 > 2.6f) continue;
            best = std::min(best, q0 + (fbm(x * 0.18f, y * 0.18f, S + 91) - 0.5f) * 0.5f);
        }
        return best;
    };
    auto homeFloor = [&](int x, int y) -> uint8_t {
        if (gladeQ(x, y) < 1.f || fieldQ(x, y) < 1.f) return T_GRASS;  // a clearing or a flower field is meadow
        const float jit = (fbm(x * 0.3f, y * 0.3f, S + 37) - 0.5f) * 0.07f;
        const float d = fbm((x - COX) * 0.08f, (y - COY) * 0.08f, S + 1) + jit;
        const float w = fbm((x - COX) * 0.045f, (y - COY) * 0.045f, S + 31) + jit;
        if (d > 0.60f) {
            if (w > 0.60f) return T_MOSS;
            if (w < 0.42f) return T_NEEDLES;
            return T_DARK;
        }
        if (d > 0.50f) {
            if (w < 0.44f) return T_LEAF;
            if (w > 0.64f) return T_MOSS;
        }
        return T_GRASS;
    };
    if (home) {  // the home forest around the original area: the same grass / dark-grass noise (continued from the area's own
                 // coordinates, so the meadows and thickets flow across its old edge without a seam), a few lakes
        struct Lake {
            float cx, cy, rx, ry;
        } lakes[] = {{30, 30, 12, 6}, {205, 28, 14, 7}, {30, 140, 12, 6}, {205, 142, 13, 6.5f}, {62, 84, 6, 9}, {180, 84, 6, 9}, {120, 20, 9, 4.5f}, {120, 150, 9, 4.5f}};
        if (!(genOff & GEN_NO_GLADES)) {   // ---- glades: ~14 clearings in the dense forest, away from the village, the lakes, the map edge and each other
            Rng gr(seed * 69061u + 555u);
            for (int att = 0; att < 4000 && glades.size() < 14; att++) {
                const float cx = gr.range(20.f, MW - 20.f), cy = gr.range(16.f, MH - 16.f);
                const float rx = gr.range(4.5f, 7.5f), ry = rx * gr.range(0.7f, 1.0f);
                const float wk = gr.f();  // (drawn every attempt, so the kinds do not depend on how many attempts failed)
                if (cx > COX - 8 && cx < COX + CW + 8 && cy > COY - 8 && cy < COY + CH + 8) continue;  // not by the village
                if (fbm((cx - COX) * 0.08f, (cy - COY) * 0.08f, S + 1) < 0.5f) continue;                  // forest clearings only: dense forest around
                bool ok = true;
                for (auto& l : lakes)
                    if (hypotf((cx - l.cx) / (l.rx + rx + 3.f), (cy - l.cy) / (l.ry + ry + 3.f)) < 1.f) ok = false;
                for (auto& o : glades)
                    if (hypotf(cx - o.cx, cy - o.cy) < 24.f) ok = false;
                if (!ok) continue;
                // flowers 3, old oak 2, rune stone 1.5, mushroom ring 2, herb patch 2, hunter's rest 1.5 (out of 12)
                const int kind = wk < 0.25f ? GL_FLOWERS : wk < 0.417f ? GL_OAK : wk < 0.542f ? GL_STONE : wk < 0.708f ? GL_FUNGI : wk < 0.875f ? GL_HERBS : GL_CAMP;
                glades.push_back({cx, cy, rx, ry, kind});
            }
        }
        if (!(genOff & GEN_NO_FIELDS)) {   // ---- flower fields: 3 wide meadows in open country, away from the village, lakes, glades, the edge and each other
            Rng fr(seed * 31337u + 4242u);
            for (int att = 0; att < 3000 && fields.size() < 3; att++) {
                const float cx = fr.range(26.f, MW - 26.f), cy = fr.range(22.f, MH - 22.f);
                const float rx = fr.range(9.f, 13.f), ry = rx * fr.range(0.65f, 0.9f);
                if (cx > COX - 12 && cx < COX + CW + 12 && cy > COY - 12 && cy < COY + CH + 12) continue;  // not by the village
                if (fbm((cx - COX) * 0.08f, (cy - COY) * 0.08f, S + 1) > 0.47f) continue;                  // open country only (not in the forest)
                bool ok = true;
                for (auto& l : lakes)
                    if (hypotf((cx - l.cx) / (l.rx + rx + 4.f), (cy - l.cy) / (l.ry + ry + 4.f)) < 1.f) ok = false;
                for (auto& g : glades)
                    if (hypotf(cx - g.cx, cy - g.cy) < g.rx + rx + 4.f) ok = false;
                for (auto& o : fields)
                    if (hypotf(cx - o.cx, cy - o.cy) < 40.f) ok = false;
                if (ok) fields.push_back({cx, cy, rx, ry});
            }
        }
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                if (inCore(x, y)) continue;
                uint8_t t = homeFloor(x, y);
                for (auto& l : lakes) {
                    float dx = (x - l.cx) / l.rx, dy = (y - l.cy) / l.ry;
                    if (dx * dx + dy * dy + (fbm(x * 0.22f, y * 0.22f, S + 7) - 0.5f) * 0.8f < 1.f) t = T_WATER;
                }
                tile[y][x] = t;
                biome[y][x] = B_HOME;
            }
        if (!(genOff & GEN_NO_STREAMS)) {   // ---- streams: ~9 brooks, each wandering from a spring in the wet forest to the nearest lake (they are shallow and walkable: a
            // path that meets one simply fords it). A path is only kept if it reaches a lake, so no brook ends in the middle of nowhere.
            Rng sr(seed * 40503u + 909u);
            auto nearVillage = [&](float x, float y, float m) { return x > COX - m && x < COX + CW + m && y > COY - m && y < COY + CH + m; };
            int made = 0;
            for (int att = 0; att < 900 && made < 9; att++) {
                const float sx = sr.range(14.f, MW - 14.f), sy = sr.range(12.f, MH - 12.f);
                const float amp = sr.range(0.8f, 1.5f), phase = sr.f() * 6.28f, freq = sr.range(0.16f, 0.30f);
                if (nearVillage(sx, sy, 6.f) || fbm((sx - COX) * 0.045f, (sy - COY) * 0.045f, S + 31) < 0.5f) continue;  // springs are in the wet forest
                const Lake* tgt = nullptr;
                float bestD = 1e9f;
                for (auto& l : lakes) {
                    const float d = hypotf(l.cx - sx, l.cy - sy);
                    if (d >= 22.f && d < bestD) { bestD = d; tgt = &l; }
                }
                if (!tgt || bestD > 95.f) continue;
                bool crosses = false;  // the straight line to the lake must keep clear of the village
                for (int i = 0; i <= 40; i++)
                    if (nearVillage(sx + (tgt->cx - sx) * i / 40.f, sy + (tgt->cy - sy) * i / 40.f, 6.f)) crosses = true;
                if (crosses) continue;
                std::vector<std::pair<int, int>> path;
                float px = sx, py = sy, dirx = tgt->cx - sx, diry = tgt->cy - sy;
                { const float l0 = hypotf(dirx, diry); dirx /= l0; diry /= l0; }
                bool reached = false;
                for (int step = 0; step < 520 && !reached; step++) {
                    float tx = tgt->cx - px, ty = tgt->cy - py;
                    const float dist = hypotf(tx, ty);
                    tx /= dist; ty /= dist;
                    const float m = sinf(step * freq + phase) * amp * cl(dist / 16.f, 0.f, 1.f);  // the wander dies away near the lake
                    float vx = tx - ty * m * 0.8f, vy = ty + tx * m * 0.8f;
                    dirx = dirx * 0.65f + vx * 0.35f;
                    diry = diry * 0.65f + vy * 0.35f;
                    const float dl = hypotf(dirx, diry);
                    dirx /= dl; diry /= dl;
                    px += dirx * 0.8f;
                    py += diry * 0.8f;
                    const int ix = (int)px, iy = (int)py;
                    if (ix < 4 || iy < 4 || ix >= MW - 4 || iy >= MH - 4 || nearVillage(px, py, 3.f)) break;
                    if (tile[iy][ix] == T_WATER) { reached = true; break; }
                    if (!path.empty() && path.back().first != ix && path.back().second != iy) path.push_back({ix, path.back().second});  // keep the run 4-connected: no diagonal gaps
                    path.push_back({ix, iy});
                    const bool wide = fbm(step * 0.11f, (float)made * 7.f, S + 73) > 0.44f;  // about half the way the brook is two tiles wide
                    const bool horiz = fabsf(dirx) > fabsf(diry);
                    if (wide) path.push_back({horiz ? ix : ix + 1, horiz ? iy + 1 : iy});
                }
                if (!reached || path.size() < 24) continue;
                for (auto& pt : path)
                    if (tile[pt.second][pt.first] != T_WATER && !inCore(pt.first, pt.second)) tile[pt.second][pt.first] = T_SHALLOW;
                made++;
            }
        }
        // lake shores: the ground a tile or two from the water is mossy and damp (the shore is ragged: its width follows a fine noise)
        for (int y = 2; y < MH - 2; y++)
            for (int x = 2; x < MW - 2; x++) {
                if (inCore(x, y) || (tile[y][x] != T_GRASS && tile[y][x] != T_DARK)) continue;
                const int reach = fbm(x * 0.35f, y * 0.35f, S + 41) > 0.5f ? 2 : 1;
                bool near = false;
                for (int dy = -reach; dy <= reach && !near; dy++)
                    for (int dx = -reach; dx <= reach; dx++)
                        if (tile[y + dy][x + dx] == T_WATER) { near = true; break; }
                if (!near && fbm(x * 0.4f, y * 0.4f, S + 43) > 0.38f)  // a brook has a narrower, frayed damp bank
                    for (int dy = -1; dy <= 1 && !near; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                            if (tile[y + dy][x + dx] == T_SHALLOW) { near = true; break; }
                if (near) tile[y][x] = T_MOSS;
            }
        // smoothing: noise leaves single-tile specks and one-tile spurs of a floor in the middle of another. A tile that is mostly surrounded
        // by one other floor (4 or more of its 8 neighbours, and at most 2 of its own kind) takes that floor. Three passes. Water banks, glades,
        // the village and everything that is not a floor are left alone.
        if (!(genOff & GEN_NO_SMOOTH)) {
            auto isFloor = [](uint8_t t) { return t == T_GRASS || t == T_DARK || t == T_LEAF || t == T_MOSS || t == T_NEEDLES || t == T_ROOTS; };
            for (int pass = 0; pass < 3; pass++) {
                std::vector<std::pair<int, uint8_t>> change;
                for (int y = 3; y < MH - 3; y++)
                    for (int x = 3; x < MW - 3; x++) {
                        const uint8_t t = tile[y][x];
                        if (!isFloor(t) || inCore(x, y) || gladeQ(x, y) < 1.5f) continue;
                        int n[T_SHALLOW + 1] = {};
                        bool wet = false;
                        for (int dy = -2; dy <= 2; dy++)
                            for (int dx = -2; dx <= 2; dx++) {
                                const uint8_t o = tile[y + dy][x + dx];
                                if (o == T_WATER || o == T_SHALLOW) wet = true;
                                if (abs(dx) <= 1 && abs(dy) <= 1 && (dx || dy) && isFloor(o)) n[o]++;
                            }
                        if (wet) continue;
                        int best = -1;
                        for (int k = 0; k <= T_SHALLOW; k++)
                            if (k != t && isFloor((uint8_t)k) && n[k] >= 4 && (best < 0 || n[k] > n[best])) best = k;
                        if (best >= 0 && n[t] <= 2) change.push_back({y * MW + x, (uint8_t)best});
                    }
                for (auto& c : change) tile[c.first / MW][c.first % MW] = c.second;
            }
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
    // In every map the ring is the map's edge wall; the trails are cut through it by an explicit gap.
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
        // The four original paths, continued to the edge of the map with the same wiggle formula; where they meet the map's edge wall
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
            if (inMap(tx, ty) && K(tx, ty) != 0 && tree) {  // the trail clears the trees and rocks standing on it
                solid[ty][tx] = false;  // (rocks get their wider footprint later, in fitRocks)
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
        // the Frozen Dungeon: an ancient fortress of ice, reached by a trail from the hut (delve.hpp)
        const int fx = 186, fy = 114;
        addLandmark("FROZEN FORTRESS", B_SNOW, fx, fy + 4);
        clearDisc(fx, fy, 10, 2);
        for (int y = fy - 3; y <= fy; y++)
            for (int x = fx - 3; x <= fx + 3; x++) {
                if (!inMap(x, y)) continue;
                tile[y][x] = T_SNOWP;
                solid[y][x] = true;
                K(x, y) = 3;
            }
        props.push_back({P_FROSTGATE, fx * TS + 8.f, (fy + 1) * TS + 0.f, 0});
        gate = V2(fx * TS + 8.f, (fy + 3) * TS + 4.f);
        lanterns.push_back({fx * TS + 8.f - 40, (fy + 1) * TS - 4.f, 0.7f});
        lanterns.push_back({fx * TS + 8.f + 40, (fy + 1) * TS - 4.f, 0.7f});
        spur((float)fx, (float)(fy + 3), 2, 3);
    } else if (map == B_MOUNTAIN) {
        const int hx = MD.hubTx, hy = MD.hubTy;
        addLandmark("MOUNTAIN PASS", B_MOUNTAIN, hx, hy);
        clearDisc(hx, hy, 9, 2);
        const int st[6][2] = {{-6, -3}, {-6, 3}, {6, -3}, {6, 3}, {0, -6}, {0, 6}};
        for (auto& s : st) addP(P_STANDING, hx + s[0], hy + s[1]);
        // caves: a cliff outcrop with a mouth in its south face and a trail to the foot of it
        auto makeCave = [&](int tx, int ty, const char* name, int sk, bool mine = false) {
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
            props.push_back({mine ? P_MINEGATE : P_CAVE, tx * TS + 8.f, (y1 + 1) * TS + 0.f, 0});
            if (mine) gate = V2(tx * TS + 8.f, (y1 + 2) * TS + 6.f);  // E here goes down into the Abandoned Mine (delve.hpp)
            addLandmark(name, B_MOUNTAIN, tx, y1 + 2);
            // trail: hub -> a waypoint south of the outcrop -> the cave mouth
            carveLine((float)hx, (float)hy, (float)tx, (float)(y1 + 5), 1.4f, sk);
            carveLine((float)tx, (float)(y1 + 5), (float)tx, (float)(y1 + 1), 0.f, sk);
        };
        makeCave(130, 40, "ECHO CAVE", 1);
        makeCave(130, 128, "ABANDONED MINE", 2, true);
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
                // every home-forest tile draws from a random stream of its own (seeded from the tile's position), so what is placed on a tile
                // never depends on what happened on the tiles before it: adding or removing a layer (a stream, a glade) leaves every tree it
                // does not touch exactly where it was
                if (home) r = Rng(hash2(tx, ty, S + 4001u) * 2654435761u + 977u);
                uint8_t t = tile[ty][tx];
                if (tileSolid(t)) continue;
                const float fx = tx * TS, fy = ty * TS;
                auto dx = [&]() { return fx + r.range(2, 14); };
                auto dy = [&]() { return fy + r.range(6, 15); };
                if (home && t == T_SHALLOW) {  // a brook: lilies and reeds, nothing solid
                    if (r.chance(0.04f)) addDecor(D_LILY, dx(), dy());
                    continue;
                }
                if (home) {  // the home forest outside the original area: same recipe as inside it
                    float ff = fbm((tx - COX) * 0.08f, (ty - COY) * 0.08f, S + 1);  // the same field the original area used, so the forest density flows on
                    float pTree = 0.014f + 0.36f * cl((ff - 0.5f) / 0.2f, 0, 1);
                    int gi = -1;
                    const float gq = gladeQ(tx, ty, &gi);
                    if (gq < 1.f) {  // inside a clearing: no trees or rocks, flowers in drifting clusters (the flower glades are carpeted)
                        const bool flowery = glades[gi].kind == GL_FLOWERS;
                        const bool patch = fbm(tx * 0.45f, ty * 0.45f, S + 61) > (flowery ? 0.42f : 0.56f);
                        if (r.chance(patch ? (flowery ? 0.70f : 0.50f) : 0.07f)) addDecor(r.irange(D_FLOWER0, D_FLOWER2), dx(), dy());
                        else if (r.chance(0.09f)) addDecor(D_TUFT, dx(), dy());
                        continue;
                    }
                    const float fq = fieldQ(tx, ty);
                    if (fq < 1.f) {  // inside a flower field: no trees or rocks, flowers thickest in the middle, in drifts of one colour
                        const float depth = 1.f - fq;  // 0 at the edge .. 1 in the middle
                        if (r.chance(std::min(0.95f, 0.35f + 0.9f * depth))) {
                            const float cn = fbm(tx * 0.17f, ty * 0.17f, S + 93);  // patches a few tiles across: several colours in one view
                            static const int FK[4] = {D_FLOWER0, D_FLOWER1, D_VIOLET, D_POPPY};  // orange, cream, violet, red
                            int fk = cn < 0.41f ? D_POPPY : cn < 0.50f ? D_FLOWER0 : cn < 0.59f ? D_FLOWER1 : D_VIOLET;
                            if (r.chance(0.12f)) fk = FK[r.irange(0, 3)];  // a little mixing
                            addDecor(fk, dx(), dy());
                            if (depth > 0.25f && r.chance(0.75f)) addDecor(fk, dx(), dy());  // thicker towards the middle: two and three to a tile
                            if (depth > 0.55f && r.chance(0.55f)) addDecor(fk, dx(), dy());
                        } else if (r.chance(0.12f)) addDecor(D_TUFT, dx(), dy());
                        continue;
                    }
                    if (fq < 1.4f) pTree *= 0.3f;  // the rim of a field: only a few trees
                    if (gq < 1.5f) pTree *= 0.35f;  // the rim of a clearing: the forest thins out
                    // groves: a clumping field makes the trees gather in stands with open ground between them (it averages out, so the
                    // forest keeps its overall density); a second field makes a stand mostly pines or mostly oaks
                    const float groveK = cl((fbm(tx * 0.13f, ty * 0.13f, S + 81) - 0.42f) / 0.2f, 0.f, 1.f);
                    pTree = std::min(0.5f, pTree * (0.3f + 1.7f * groveK));
                    const float standPine = (fbm(tx * 0.10f, ty * 0.10f, S + 83) - 0.5f) * 0.6f;
                    // in open country (not thicket) a tree keeps its distance from the next one: lone trees, not a dotted carpet
                    bool crowded = false;
                    if (ff < 0.55f)
                        for (int yy = ty - 2; yy <= ty + 2 && !crowded; yy++)
                            for (int xx = tx - 2; xx <= tx + 2; xx++)
                                if (inMap(xx, yy) && solid[yy][xx]) { crowded = true; break; }
                    if (crowded) pTree = 0.f;
                    const bool thicket = ff >= 0.55f || t == T_ROOTS || t == T_MOSS || t == T_NEEDLES;
                    if (r.chance(pTree)) {  // pines like needles, roots and dark thickets; oaks like leaf litter, moss and meadows
                        const float pPine = cl((t == T_NEEDLES ? 0.88f : t == T_ROOTS ? 0.62f : t == T_DARK ? 0.55f : t == T_MOSS ? 0.30f : t == T_LEAF ? 0.04f : 0.12f) + standPine, 0.02f, 0.97f);
                        const bool oakOnly = t == T_DARK || t == T_NEEDLES || t == T_ROOTS;  // (their oaks are always the dark-leaved look)
                        tileProp(r.chance(pPine) ? P_PINE : (oakOnly || r.chance(0.5f) ? P_OAK0 : P_OAK1), tx, ty);
                    }
                    else if (r.chance(t == T_ROOTS ? 0.030f : t == T_MOSS ? 0.008f : thicket ? 0.004f : 0.0012f)) tileProp(r.chance(0.5f) ? P_DEADTREE0 : P_DEADTREE1, tx, ty);  // dead trees stand in the gloomy ground
                    else if (thicket && r.chance(0.0035f)) {  // a fallen log
                        props.push_back({P_LOG, tx * TS + 8.f, ty * TS + 14.f, 0});
                        solid[ty][tx] = true;
                    }
                    else if (r.chance(thicket ? 0.0075f : 0.0020f)) addDecor(D_STUMP, tx * TS + 8.f, ty * TS + 14.f);  // an old stump
                    else if (r.chance(0.007f)) rockProp(r.chance(0.6f) ? P_ROCK0 : P_ROCK1, tx, ty);
                    else if (r.chance(0.011f)) addDecor(6 + r.irange(0, 1), tx * TS + 8.f, ty * TS + 15.f);
                    else if (r.chance(0.085f)) {
                        const float roll = r.f();
                        int dk = -1;  // -1: nothing here (the forest floors keep clean: only an odd mushroom or stone)
                        if (t == T_LEAF || t == T_NEEDLES) dk = roll < 0.10f ? D_MUSHROOM : roll < 0.16f ? D_PEBBLES : -1;
                        else if (t == T_ROOTS) dk = roll < 0.22f ? D_MUSHROOM : roll < 0.40f ? D_PEBBLES : -1;
                        else if (t == T_MOSS) dk = roll < 0.30f ? D_MUSHROOM : roll < 0.85f ? D_TUFT : D_PEBBLES;
                        else dk = roll < 0.3f ? r.irange(0, 2) : roll < 0.82f ? 3 : roll < 0.9f ? (t == T_DARK ? 4 : 3) : 5;
                        if (dk >= 0) addDecor(dk, dx(), dy());
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
                            const float rr = r.f();  // tiny (hand-mined), small, and now the odd large desert rock
                            rockProp(rr < 0.45f ? P_SANDROCK0 : rr < 0.85f ? P_SANDROCK1 : P_SANDBOULDER, tx, ty);
                            break;
                        }
                        if (r.chance(0.006f)) {  // decor rocks: scenery only, cannot be mined
                            addDecor(D_DROCK0 + r.irange(0, 3), dx(), dy());
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
                        // snow bushes: decided by their own per-tile hash and pushed directly, so they take nothing from the shared random
                        // stream and every other thing in the snowfield stays exactly where it was for an existing seed
                        else if ((hash2(tx, ty, S + 5101u) & 0xFFFF) < (uint32_t)(0.009f * 65536.f))
                            decor.push_back({D_BUSH0 + (int)(hash2(tx, ty, S + 5102u) & 1u), fx + 8.f, fy + 15.f, (hash2(tx, ty, S + 5103u) & 0xFFFF) / 65535.f * 6.28f});
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
                        // alpine cushions on the meadow, dead twig clumps on the scree: their own per-tile hash, pushed directly, so the shared
                        // random stream (and with it every other thing on the map) is unchanged for an existing seed
                        else if ((hash2(tx, ty, S + 5201u) & 0xFFFF) < (uint32_t)((meadow ? 0.012f : 0.007f) * 65536.f))
                            decor.push_back({D_BUSH0 + (meadow ? 0 : 1), fx + 8.f, fy + 15.f, (hash2(tx, ty, S + 5203u) & 0xFFFF) / 65535.f * 6.28f});
                        break;
                    }
                }
            }

    // ------------------------------------------------------------ 6. the map's edge wall 
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
        if (inRing(tx, ty) || K(tx, ty) != 0 || tile[ty][tx] == T_SHALLOW) return false;
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
        // ---- the glades' features (the clearings themselves are carved in the terrain and tree passes above)
        {
            Rng gf(seed * 7919u + 88u);
            auto gFree = [&](int x, int y) {
                return inMap(x, y) && !inRing(x, y) && (K(x, y) == 0 || K(x, y) == 2) && !solid[y][x] && !tileSolid(tile[y][x]) && tile[y][x] != T_DIRT && tile[y][x] != T_SHALLOW && gladeQ(x, y) < 0.85f;
            };
            // the free tile nearest a glade's middle (a trail may cross it)
            auto gSpot = [&](const Glade& g, int& ox, int& oy) {
                const int cx = (int)lroundf(g.cx), cy = (int)lroundf(g.cy);
                for (int rad = 0; rad <= 3; rad++)
                    for (int dy = -rad; dy <= rad; dy++)
                        for (int dx = -rad; dx <= rad; dx++)
                            if ((abs(dx) == rad || abs(dy) == rad) && gFree(cx + dx, cy + dy)) { ox = cx + dx; oy = cy + dy; return true; }
                return false;
            };
            // a node near (cx, cy) tiles, within `rad` tiles, on a free tile
            auto gNode = [&](int kind, float cx, float cy, float rad) {
                for (int att = 0; att < 80; att++) {  // (the search widens a little each try: a brook may cover the middle of the clearing)
                    const float reach = rad + att * 0.06f;
                    const int tx = (int)lroundf(cx + gf.range(-reach, reach)), ty = (int)lroundf(cy + gf.range(-reach, reach));
                    if (!gFree(tx, ty) || !passable(tx, ty) || !passable(tx + 1, ty) || !passable(tx - 1, ty) || !passable(tx, ty + 1) || !passable(tx, ty - 1)) continue;  // (a trail's cleared margin is fine)
                    const float nx = tx * TS + 8.f + gf.range(-3, 3), ny = ty * TS + 12.f + gf.range(-1, 2);
                    if (!farFromNodes(nx, ny, 14.f)) continue;
                    nodes.push_back({kind, nx, ny, gf.f() * 6.28f});
                    return true;
                }
                return false;
            };
            // decor on a ring `rad` tiles round the middle, skipping tiles that are not open meadow
            auto gRing = [&](const Glade& g, int kind, int n, float rad) {
                for (int i = 0; i < n; i++) {
                    const float a = i * 6.2832f / n + gf.range(-0.2f, 0.2f);
                    const float px = (g.cx + cosf(a) * rad) * TS + 8.f, py = (g.cy + sinf(a) * rad * 0.8f) * TS + 10.f;
                    const int tx = (int)(px / TS), ty = (int)(py / TS);
                    if (inMap(tx, ty) && K(tx, ty) == 0 && !solid[ty][tx] && tile[ty][tx] != T_DIRT && tile[ty][tx] != T_SHALLOW && !tileSolid(tile[ty][tx])) addDecor(kind, px, py);
                }
            };
            for (const Glade& g : glades) {
                int ox, oy;
                switch (g.kind) {
                    case GL_OAK:  // one great old oak in the middle of the clearing
                        if (gSpot(g, ox, oy)) tileProp(P_OAK1, ox, oy);
                        break;
                    case GL_STONE:  // a rune stone (it glows at night) with a scatter of pebbles
                        if (gSpot(g, ox, oy)) addP(P_STANDING, ox, oy);
                        gRing(g, D_PEBBLES, 7, 1.7f);
                        break;
                    case GL_FUNGI:  // a fairy ring of mushrooms, with mushrooms to gather inside it
                        gRing(g, D_MUSHROOM, 11, 2.6f);
                        for (int i = 0; i < 3; i++) gNode(RN_MUSHROOM, g.cx, g.cy, 1.4f);
                        break;
                    case GL_HERBS:  // a herb patch
                        for (int i = 0; i < 4; i++) gNode(RN_HERB, g.cx, g.cy, 2.6f);
                        for (int i = 0; i < 2; i++) gNode(RN_FLAX, g.cx, g.cy, 3.f);
                        break;
                    case GL_CAMP: {  // a hunter's rest: a ring of stones where a fire once burned, a log on each side
                        gRing(g, D_PEBBLES, 9, 1.2f);
                        for (int side = -1; side <= 1; side += 2) {
                            const int tx = (int)lroundf(g.cx) + side * 3, ty = (int)lroundf(g.cy) + 1;
                            if (gFree(tx, ty)) {
                                props.push_back({P_LOG, tx * TS + 8.f, ty * TS + 14.f, 0});
                                solid[ty][tx] = true;
                            }
                        }
                        break;
                    }
                    default: break;  // GL_FLOWERS: the flowers themselves (the tree pass) are the feature
                }
            }
            // a flower field has herbs to gather among the flowers
            for (const FlowerField& f : fields)
                for (int i = 0, placed = 0; i < 90 && placed < 7; i++) {
                    const int tx = (int)lroundf(f.cx + gf.range(-f.rx, f.rx) * 0.8f), ty = (int)lroundf(f.cy + gf.range(-f.ry, f.ry) * 0.8f);
                    if (!inMap(tx, ty) || fieldQ(tx, ty) > 0.7f || inRing(tx, ty) || K(tx, ty) != 0 || solid[ty][tx] || tile[ty][tx] != T_GRASS) continue;
                    if (!passable(tx, ty) || !passable(tx + 1, ty) || !passable(tx - 1, ty) || !passable(tx, ty + 1) || !passable(tx, ty - 1)) continue;
                    const float nx = tx * TS + 8.f + gf.range(-3, 3), ny = ty * TS + 12.f + gf.range(-1, 2);
                    if (!farFromNodes(nx, ny, 16.f)) continue;
                    nodes.push_back({RN_HERB, nx, ny, gf.f() * 6.28f});
                    placed++;
                }
        }
        auto homeOk = [&](int tx, int ty) {
            const int cx = tx - COX, cy = ty - COY;
            if (cx < 4 || cy < 4 || cx >= CW - 4 || cy >= CH - 4 || K(tx, ty) != 0) return false;
            if (hypotf(cx - 44.f, (cy - 30.f) * 1.15f) < 11.f) return false;  // the village plaza stays clear
            return passable(tx, ty) && passable(tx + 1, ty) && passable(tx - 1, ty) && passable(tx, ty + 1) && passable(tx, ty - 1) && tile[ty][tx] != T_DIRT && tile[ty][tx] != T_SHALLOW;
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
        homeScatter(RN_BRANCH, 36, [&](int x, int y) { return (tile[y][x] == T_GRASS || tile[y][x] == T_DARK) && nearTree(x * TS + 8.f, y * TS + 12.f, 40.f); });  // branches fall from trees
        homeScatter(RN_FLAX, 28, [&](int x, int y) { return tile[y][x] == T_GRASS; });
        // and the same plants through the home forest around the original area
        auto outer = [&](int x, int y) { return !inCore(x, y); };
        scatter(RN_HERB, 70, [&](int x, int y) { return outer(x, y) && (tile[y][x] == T_GRASS || tile[y][x] == T_DARK || tile[y][x] == T_MOSS || tile[y][x] == T_LEAF); }, 4.f);
        scatter(RN_MUSHROOM, 40, [&](int x, int y) { return outer(x, y) && (tile[y][x] == T_DARK || tile[y][x] == T_MOSS || tile[y][x] == T_ROOTS); }, 4.f);
        scatter(RN_BRANCH, 70, [&](int x, int y) { return outer(x, y) && tileGrassy(tile[y][x]) && nearTree(x * TS + 8.f, y * TS + 12.f, 40.f); }, 4.f);
        scatter(RN_FLAX, 50, [&](int x, int y) { return outer(x, y) && tile[y][x] == T_GRASS; }, 4.f);
        // ---- lake banks: a ring of trodden earth round every lake, one tile wide and ragged (a second tile here and there). Made after every tree,
        // plant and random choice above, so none of them depends on it; only forest floors turn into bank (paths, brooks and glades' meadows stay)
        // ---- lake depth as real tiles: round the shore the water is shallow (wade), one tile further in it is mid (wade, slow), and the rest stays
        // deep (T_WATER, solid). A brook or any dry tile counts as shore. The village area is left alone (the town is laid over it afterwards).
        {
            std::vector<std::pair<int, int>> shallow, mid;
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    if (tile[y][x] != T_WATER || inCore(x, y) || inRing(x, y)) continue;
                    int d = 3;
                    for (int dy = -2; dy <= 2; dy++)
                        for (int dx = -2; dx <= 2; dx++)
                            if (inMap(x + dx, y + dy) && tile[y + dy][x + dx] != T_WATER) d = std::min(d, std::max(std::abs(dx), std::abs(dy)));
                    if (d == 1) shallow.push_back({x, y});
                    else if (d == 2) mid.push_back({x, y});
                }
            for (auto& pt : shallow) tile[pt.second][pt.first] = T_LAKE_SHALLOW;
            for (auto& pt : mid) tile[pt.second][pt.first] = T_LAKE_MID;
        }
        {
            auto bankable = [](uint8_t t) { return t == T_GRASS || t == T_DARK || t == T_LEAF || t == T_NEEDLES || t == T_MOSS; };
            std::vector<std::pair<int, int>> todo;
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    if (!bankable(tile[y][x]) || inCore(x, y)) continue;
                    int dmin = 3;
                    for (int dy = -2; dy <= 2; dy++)
                        for (int dx = -2; dx <= 2; dx++)
                            if (inMap(x + dx, y + dy) && tileLake(tile[y + dy][x + dx])) dmin = std::min(dmin, std::max(std::abs(dx), std::abs(dy)));
                    if (dmin == 1 || (dmin == 2 && fbm(x * 0.4f, y * 0.4f, S + 9) < 0.47f)) todo.push_back({x, y});
                }
            for (auto& pt : todo) tile[pt.second][pt.first] = T_BANK;
        }
        generateTown();  // last: the town is laid over the middle of the original area
    }
}

// ------------------------------------------------------------------------------------------------------------------------------
// The central town (town.hpp). Laid over the middle of the original Wildwood area after everything else is generated, so the surrounding
// terrain, forest and resources are exactly as before: only the town's own rectangle is cleared (trees, rocks, plants and the old hut go),
// paved with streets and filled with the six buildings and their street furniture. Streets run to the plaza and join the old dirt paths.
inline bool World::inTown(float x, float y, float margin) const {
    return mapId == B_HOME && x >= (COX + TOWN_X0) * TS - margin && x < (COX + TOWN_X1) * TS + margin && y >= (COY + TOWN_Y0) * TS - margin &&
           y < (COY + TOWN_Y1) * TS + margin;
}

inline void World::generateTown() {
    const int X0 = COX + TOWN_X0, Y0 = COY + TOWN_Y0, X1 = COX + TOWN_X1, Y1 = COY + TOWN_Y1;  // map tiles, X1 / Y1 exclusive
    auto inRect = [&](float px, float py) { return px >= X0 * TS && px < X1 * TS && py > Y0 * TS && py <= Y1 * TS; };
    // 0. nothing gatherable in the town, beside it or on its four roads: no tree, rock, plant or ore to chop, mine or pick (the thick
    // forest around the town is ordinary forest)
    {
        const int M = 2;  // tiles of clearing around the rectangle
        auto zone = [&](float px, float py) {
            const int tx = (int)(px / TS) - COX, ty = (int)((py - 1.f) / TS) - COY;
            if (tx >= TOWN_X0 - M && tx < TOWN_X1 + M && ty >= TOWN_Y0 - M && ty < TOWN_Y1 + M) return true;  // the town and its margin
            if (ty >= 23 && ty <= 27 && ((tx >= 12 && tx < TOWN_X0) || (tx >= TOWN_X1 && tx < 76))) return true;        // west / east road
            if (tx >= 42 && tx <= 45 && ((ty >= 8 && ty < TOWN_Y0) || (ty >= TOWN_Y1 && ty < 52))) return true;         // north / south road
            return false;
        };
        auto gatherable = [](int kind) { return kind == P_OAK0 || kind == P_OAK1 || kind == P_PINE || kind == P_ROCK0 || kind == P_ROCK1 || kind == P_LOG; };
        for (size_t i = 0; i < props.size();) {
            if (gatherable(props[i].kind) && zone(props[i].x, props[i].y)) {
                solid[(int)((props[i].y - 1.f) / TS)][(int)(props[i].x / TS)] = false;
                props.erase(props.begin() + i);
            } else
                i++;
        }
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const ResNode& n) { return zone(n.x, n.y); }), nodes.end());
    }
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
    {   // the Homestead Portal: a standing-stone gate on the west side of the plaza, between the flower shop and the well
        const V2 a = at(33, 31.9f);
        props.push_back({P_HPORTAL, a.x, a.y, 0});
        for (int i = 0; i < 3; i++) solid[COY + 31][COX + 32 + i] = true;
        hportal = a;
        lanterns.push_back({a.x, a.y - 22.f, 0.9f});
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

#include "farming/homestead_gen.hpp"

// Keeps trees (oaks, pines, snow spruces, dead and bare trees) from piling on top of each other. Runs once after a map is generated: walking the trees in order, a tree is
// dropped when its canopy would cover more than TREE_OVERLAP (a share, 0 to 1) of an already kept tree's canopy (tree_lime.hpp). Rocks, bushes and everything else are left alone. Nothing random is used, so the map stays the same every run.
inline void World::thinTrees() {
    struct T { float x, y, w, c; };  // c = height of the leafy part
    const float CELL = 64.f;
    const int gw = (int)((MW * TS) / CELL) + 2, gh = (int)((MH * TS) / CELL) + 2;
    std::vector<std::vector<T>> grid((size_t)gw * gh);
    std::vector<Prop> out;
    out.reserve(props.size());
    for (const Prop& p : props) {
        const bool tree = p.kind == P_OAK0 || p.kind == P_OAK1 || p.kind == P_PINE || p.kind == P_SNOWPINE || p.kind == P_DEADTREE0 ||
                          p.kind == P_DEADTREE1 || p.kind == P_BARETREE;
        if (!tree) { out.push_back(p); continue; }
        const int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
        float w = PINE_W, h = PINE_H;  // the size of the tree's sprite: how far its top reaches sideways and up
        if (p.kind == P_OAK0 || p.kind == P_OAK1) { const OakVar& v = OAK_VAR[oakVariantFor(tx, ty)]; w = (float)v.w; h = (float)v.h; }
        else if (p.kind == P_SNOWPINE) { w = SNOW_SPRUCE_W; h = SNOW_SPRUCE_H; }
        else if (p.kind != P_PINE) { w = DEAD_SPRUCE_W; h = DEAD_SPRUCE_H; }
        const T t{p.x, p.y, w, h * 0.7f};
        const int cx = (int)(p.x / CELL), cy = (int)(p.y / CELL);
        // the trees of the map's edge wall stay: they are walls, not forest
        const bool wall = tx < 2 || ty < 2 || tx >= MW - 2 || ty >= MH - 2;
        bool crowded = false;
        for (int gy = std::max(0, cy - 1); gy <= std::min(gh - 1, cy + 1) && !crowded; gy++)
            for (int gx = std::max(0, cx - 1); gx <= std::min(gw - 1, cx + 1) && !crowded; gx++)
                for (const T& o : grid[(size_t)gy * gw + gx]) {
                    // how far the two tops cover each other sideways and up/down (0 = not at all, 1 = fully)
                    const float ow = std::max(0.f, std::min(1.f, ((t.w + o.w) * 0.5f - std::fabs(t.x - o.x)) / std::min(t.w, o.w)));
                    const float oh = std::max(0.f, std::min(1.f, ((t.c + o.c) * 0.5f - std::fabs(t.y - o.y)) / std::min(t.c, o.c)));
                    // ow * oh is the share of the smaller top that is covered. A tree straight behind another (same sideways spot)
                    // covers a lot, so it must stand far back. A tree shifted to the side covers less, so it may stand closer:
                    // neighbours end up staggered in rows, like layers.
                    if (!wall && ow * oh > TREE_OVERLAP) { crowded = true; break; }
                }
        if (crowded) {
            // the tree is gone, so is its blocking tile
            if (tx >= 0 && ty >= 0 && tx < MW && ty < MH) solid[ty][tx] = false;
            continue;
        }
        grid[(size_t)cy * gw + cx].push_back(t);
        out.push_back(p);
    }
    props.swap(out);
}

// Gives every rock one of its 5 looks and makes it block as wide as that sprite is. A look is used only when all the extra tiles it
// needs are free, plain ground (not a trail, water, a cliff or something already blocking); otherwise the next look is tried, and a
// rock with no room blocks only its own tile. The map's edge wall keeps one tile per boulder, it is a wall already.
inline void World::fitRocks() {
    auto plain = [&](int tx, int ty) {
        if (tx < 2 || ty < 2 || tx >= MW - 2 || ty >= MH - 2) return false;
        if (solid[ty][tx] || tileSolid(tile[ty][tx])) return false;
        const uint8_t t = tile[ty][tx];
        return t != T_DIRT && t != T_COBBLE && t != T_WATER && t != T_CLIFF && t != T_RAIL && t != T_ICE;
    };
    // The sprite boxes of the rocks kept so far. A rock whose picture would cover more than ROCK_OVERLAP of a kept rock's picture
    // (the share of the smaller one) tries its other looks, and is dropped when none fits: boulders first, then small, then tiny rocks.
    struct Box { float x0, y0, x1, y1; };
    std::vector<Box> kept;
    auto boxOf = [&](const Prop& p, int look) {
        const RockArt& a = rockSet(p.kind)[look];
        return Box{p.x - a.w * 0.5f, p.y - a.h + 2.f, p.x + a.w * 0.5f, p.y + 2.f};
    };
    auto crowds = [&](const Box& b) {
        const float ba = (b.x1 - b.x0) * (b.y1 - b.y0);
        for (const Box& o : kept) {
            const float iw = std::min(b.x1, o.x1) - std::max(b.x0, o.x0), ih = std::min(b.y1, o.y1) - std::max(b.y0, o.y0);
            if (iw <= 0 || ih <= 0) continue;
            const float oa = (o.x1 - o.x0) * (o.y1 - o.y0);
            if (iw * ih > ROCK_OVERLAP * std::min(ba, oa)) return true;
        }
        return false;
    };
    std::vector<size_t> order;
    for (int pass = 0; pass < 3; pass++)
        for (size_t i = 0; i < props.size(); i++) {
            const int k = props[i].kind;
            const int rc = rockClass(k);
            if (rc >= 0 && rc == 2 - pass) order.push_back(i);
        }
    std::vector<bool> drop(props.size(), false);
    for (size_t i : order) {
        Prop& p = props[i];
        const int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
        const int start = std::abs((tx * 7 + ty * 13)) % ROCK_LOOKS;
        if (tx < 2 || ty < 2 || tx >= MW - 2 || ty >= MH - 2) { p.ph = 100.f + start; kept.push_back(boxOf(p, start)); continue; }  // the edge wall stays
        bool done = false;
        for (int k = 0; k < ROCK_LOOKS && !done; k++) {
            const int look = (start + k) % ROCK_LOOKS;
            if (crowds(boxOf(p, look))) continue;
            int x0, y0, x1, y1;
            rockFoot(p, look, false, x0, y0, x1, y1);
            bool ok = true;
            for (int y = y0; y <= y1 && ok; y++)
                for (int x = x0; x <= x1 && ok; x++)
                    if (!(x == tx && y == ty) && !plain(x, y)) ok = false;
            if (!ok) continue;
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) solid[y][x] = true;
            p.ph = (float)look;
            kept.push_back(boxOf(p, look));
            done = true;
        }
        if (done) continue;
        // no look fits without a crowded picture or a blocked footprint: a boulder stays as it is (blocking its own tile), the rest go
        bool free = true;
        for (int k = 0; k < ROCK_LOOKS && free; k++) free = !crowds(boxOf(p, (start + k) % ROCK_LOOKS));
        if (p.kind == P_BOULDER || p.kind == P_SANDBOULDER || free) {
            p.ph = 100.f + start;
            solid[ty][tx] = true;
            kept.push_back(boxOf(p, start));
        } else {
            drop[i] = true;
            solid[ty][tx] = false;  // it blocked its own tile (placed that way): the tile is free again
        }
    }
    std::vector<Prop> out;
    out.reserve(props.size());
    for (size_t i = 0; i < props.size(); i++)
        if (!drop[i]) out.push_back(props[i]);
    props.swap(out);
}

inline void World::setPropSolid(const Prop& p, bool v) {
    int x0, y0, x1, y1;
    if (isRockKind(p.kind)) rockFoot(p, x0, y0, x1, y1);
    else x0 = x1 = (int)(p.x / TS), y0 = y1 = (int)(p.y / TS);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (x >= 0 && y >= 0 && x < MW && y < MH) solid[y][x] = v;
}

// Marks the tiles that stop light. Buildings, huts, cave mouths and wide rocks stand on solid tiles: around each one the solid tiles
// become occluders (trees do not, a forest would be pitch black). Cliffs and walls count too.
inline void World::buildOccluders() {
    memset(occ, 0, sizeof(occ));
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if (tile[y][x] == T_CLIFF) occ[y][x] = true;
    auto mark = [&](const Prop& p, int rx, int up) {
        const int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
        for (int y = ty - up; y <= ty; y++)
            for (int x = tx - rx; x <= tx + rx; x++)
                if (x >= 0 && y >= 0 && x < MW && y < MH && solid[y][x]) occ[y][x] = true;
    };
    for (const Prop& p : props) {
        if (p.kind >= P_BLD0 && p.kind <= P_BLD5) mark(p, 5, 5);
        else if (p.kind == P_HUT || p.kind == P_ICEHUT) mark(p, 2, 2);
        else if (p.kind == P_CAVE) mark(p, 3, 3);
        else if (isRockKind(p.kind)) {
            int x0, y0, x1, y1;
            rockFoot(p, x0, y0, x1, y1);
            if (x1 > x0 || p.kind == P_BOULDER || p.kind == P_SANDBOULDER)
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                        if (x >= 0 && y >= 0 && x < MW && y < MH) occ[y][x] = true;
        }
    }
}
