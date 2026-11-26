// tile_edges.hpp - soft, rounded borders between ground tiles ("edge blending").
// A tile never changes its own texture. Instead, where a neighbour of a HIGHER priority (a path next to grass, snow next to
// stone...) touches it, that neighbour's texture is grown a few pixels into the tile through a jagged, rounded mask:
//   - a strip along each side that touches it,
//   - a rounded disc in a corner that only the diagonal neighbour touches,
//   - a small filler where two strips meet, so the grass does not end in a sharp inner corner.
// The 8 neighbours are packed into one config byte (bit 0..7 = N E S W NE SE SW NW). The finished overlay for each
// (class, texture variant, config, profile) is baked the first time it is needed and cached as a texture.
#pragma once
#include <unordered_map>
#include "core/gfx.hpp"

// the ground types that blend (see blendClass() in game_render_world.cpp); water, cliffs, bog and every interior tile do not (ice only receives)
enum BlendCls { BL_GRASS, BL_DARK, BL_DIRT, BL_SAND, BL_DUNE, BL_MUD, BL_SNOW, BL_SNOWP, BL_STONE, BL_LEAF, BL_NEEDLES, BL_MOSS, BL_ROOTS, BL_BANK, BL_ICE, BL_COUNT };
// who grows over whom: a higher number is drawn over a lower one
// (grass, dark, dirt, sand, dune, mud, snow, snowp, stone, leaf, needles, moss, roots) - the forest floors grow over the grasses, the paths over everything
constexpr int BLEND_PRIO[BL_COUNT] = {20, 22, 50, 30, 32, 36, 34, 38, 40, 23, 24, 25, 26, 28, 10};  // (ice: lowest, whatever touches a frozen lake grows over its edge)
constexpr bool ROCKY_DIRT = true;  // paths are cobbled rock with a bluish rim; false = the old brown dirt (no rim)
constexpr u32 BLEND_RIM[BL_COUNT] = {0, 0, ROCKY_DIRT ? HEX(0x4a5878) : 0u, 0, 0, 0, 0, HEX(0xa3b8d0), 0, 0, 0, 0, 0, 0, 0};  // 0 = no outline

// how far each column / row of a side strip reaches into the tile; every profile starts and ends on 3 so strips of
// neighbouring tiles join up
constexpr int BLEND_PROFILE[4][16] = {{3, 3, 4, 4, 3, 2, 2, 3, 4, 4, 3, 3, 2, 3, 3, 3},
                                      {3, 4, 4, 3, 3, 2, 3, 3, 2, 2, 3, 4, 4, 3, 3, 3},
                                      {3, 3, 2, 3, 4, 4, 3, 3, 3, 4, 4, 3, 2, 2, 3, 3},
                                      {3, 2, 2, 3, 3, 4, 4, 4, 3, 2, 3, 3, 4, 3, 2, 3}};

inline Img bakeBlend(const Img& src, int cfg, int prof, u32 rim) {
    bool m[16][16] = {};
    const bool N = cfg & 1, E = cfg & 2, S = cfg & 4, W = cfg & 8, NE = cfg & 16, SE = cfg & 32, SW = cfg & 64, NW = cfg & 128;
    const int* D = BLEND_PROFILE[prof & 3];
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (N && y < D[x]) m[y][x] = true;
            if (S && y >= 16 - D[x]) m[y][x] = true;
            if (W && x < D[y]) m[y][x] = true;
            if (E && x >= 16 - D[y]) m[y][x] = true;
        }
    // corners, painted in the north-west frame and mirrored (mx / my) for the other three
    auto corner = [&](bool diag, bool a, bool b, int mx, int my) {
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                const int px = mx ? 15 - x : x, py = my ? 15 - y : y;
                if (diag && !a && !b) {  // only the diagonal neighbour: a rounded bulge around the shared corner
                    const float dx = px + 0.5f, dy = py + 0.5f;
                    if (dx * dx + dy * dy <= 3.6f * 3.6f) m[y][x] = true;
                }
                if (a && b && px < 6 && py < 6) {  // both sides: round off the inner corner of what is left
                    const float dx = px + 0.5f - 6.f, dy = py + 0.5f - 6.f;
                    if (dx * dx + dy * dy > 9.f) m[y][x] = true;
                }
            }
    };
    corner(NW, N, W, 0, 0);
    corner(NE, N, E, 1, 0);
    corner(SW, S, W, 0, 1);
    corner(SE, S, E, 1, 1);
    Img out(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (!m[y][x]) continue;
            u32 c = src.get(x, y);
            if (rim) {  // outline = a mask pixel with a non-mask neighbour inside the tile (the tile border itself is not an edge)
                auto open = [&](int xx, int yy) { return xx >= 0 && yy >= 0 && xx < 16 && yy < 16 && !m[yy][xx]; };
                if (open(x - 1, y) || open(x + 1, y) || open(x, y - 1) || open(x, y + 1)) c = rim;
            }
            out.set(x, y, c);
        }
    return out;
}

// ---- cliff autotiling: the look of a cliff tile follows which of its 8 neighbours are not cliff (bit 0..7 = N E S W NE SE SW NW, set = open)
// A cliff tile is drawn from a plain block of rock; here the sides that face open ground are finished off:
//   - south: the dark foot of the wall (the lit top is the snow cap or the lip, which the block itself carries),
//   - west / east: the silhouette is cut 1-2 pixels in along a ragged profile (the same profile on both tiles of a wall, which starts and ends on 1, so
//     stacked tiles join up), the cut edge lit on its west side and shaded on its east side,
//   - two open sides that meet: the outer corner is rounded; only a diagonal open: a small notch is bitten into the inner corner.
// What is cut away is transparent, so the ground is drawn under the tile first.
constexpr int CLIFF_CUT[4][16] = {{1, 1, 2, 2, 1, 1, 1, 2, 2, 2, 1, 1, 2, 1, 1, 1},
                                  {1, 2, 2, 1, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 1, 1},
                                  {1, 1, 1, 2, 2, 1, 1, 1, 1, 2, 2, 2, 1, 2, 1, 1},
                                  {1, 2, 1, 1, 2, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1, 1}};
struct CliffPal {
    u32 foot, rockLit, rockDark, snowLit, snowShade;  // foot of the wall; a lit and a shaded rock edge; a lit and a shaded snow edge
    bool outlined = false;                            // the sides and the foot of the wall get a dark outline (outline), only the top edge stays lit
    u32 outline = 0;
};
constexpr CliffPal CLIFF_SNOW = {HEX(0x2e2a34), HEX(0x8a7662), HEX(0x3a3029), HEX(0xffffff), HEX(0xb9cde2)};
// the plateau faces: the snowfield's own rock colours (ROCK_PALS[B_SNOW] in rock_sprites.hpp), so the wall and the icy rocks on it match
constexpr CliffPal CLIFF_ICE = {HEX(0x2a3148), HEX(0x9aa6c2), HEX(0x3c4660), HEX(0xffffff), HEX(0xb9cde2), true, HEX(0x2a3148)};

inline Img bakeCliff(const Img& base, int cfg, int prof, const CliffPal& P, int R = 4) {  // R = the radius of a rounded outer corner, in pixels
    const bool N = cfg & 1, E = cfg & 2, S = cfg & 4, W = cfg & 8, NE = cfg & 16, SE = cfg & 32, SW = cfg & 64, NW = cfg & 128;
    (void)N;
    Img im = base;
    if (S && !P.outlined) im.rect(0, 13, 16, 3, P.foot);  // an outlined wall has no dark foot: its bottom is a ragged edge, like the sides
    bool cut[16][16] = {};
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (W && x < CLIFF_CUT[prof & 3][y]) cut[y][x] = true;
            if (E && x >= 16 - CLIFF_CUT[(prof + 1) & 3][y]) cut[y][x] = true;
            if (S && P.outlined) {  // the foot melts into the snow: the ground grows up over the bottom 3-4 px through the same jagged profile as the ground blending, one dithered row above it
                const int D = BLEND_PROFILE[(prof + 3) & 3][x];
                if (y >= 16 - D || (y == 15 - D && ((x + prof) & 1))) cut[y][x] = true;
            }
        }
    auto corner = [&](bool diag, bool a, bool b, int mx, int my) {  // painted in the north-west frame, mirrored for the other three
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                const int px = mx ? 15 - x : x, py = my ? 15 - y : y;
                if (a && b && px < R && py < R) {  // two open sides: round off the outer corner
                    const float dx = px + 0.5f - R, dy = py + 0.5f - R, rr = R - 0.26f;
                    if (dx * dx + dy * dy > rr * rr) cut[y][x] = true;
                }
                if (diag && !a && !b) {  // only the diagonal is open: a notch in the inner corner
                    const float dx = px + 0.5f, dy = py + 0.5f;
                    if (dx * dx + dy * dy <= 3.1f * 3.1f) cut[y][x] = true;
                }
            }
    };
    corner(NW, N, W, 0, 0);
    corner(NE, N, E, 1, 0);
    corner(SW, S, W, 0, 1);
    corner(SE, S, E, 1, 1);
    Img out(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (cut[y][x]) continue;
            u32 c = im.get(x, y);
            const bool l = x > 0 && cut[y][x - 1], r = x < 15 && cut[y][x + 1], u = y > 0 && cut[y - 1][x], d = y < 15 && cut[y + 1][x];
            if (l || r || u || d) {  // the edge of what is left: lit towards the north-west, shaded towards the south-east
                const u32 sum = (c & 255) + (c >> 8 & 255) + (c >> 16 & 255);
                const bool snow = sum > 600;
                if (P.outlined && (l || r)) c = P.outline;  // a dark outline round the ends and the rounded corners; the bottom is left soft
                else c = (l || u) ? (snow ? P.snowLit : P.rockLit) : (snow ? P.snowShade : P.rockDark);
            }
            out.set(x, y, c);
        }
    return out;
}

// ---- the rim of a snow plateau: a row of rounded snow clumps (now and then a grey stone among them) just inside the tile's open sides, like the cobbled rim
// of a raised ground in a top-down tileset. cfg: N E S W NE SE SW NW set where that side / diagonal looks onto plain ground.
// An overlay on the snow tile. Every side is built from 4 px clumps (so the clumps of neighbouring tiles line up), each 5 px deep, lit on its north-west
// side and shaded on its south-east side with a thin blue outline against the ground; outer corners are rounded, an inner corner is filled with a clump.
inline Img bakeLip(int cfg, int prof) {
    const bool N = cfg & 1, E = cfg & 2, S = cfg & 4, W = cfg & 8, NE = cfg & 16, SE = cfg & 32, SW = cfg & 64, NW = cfg & 128;
    bool M[16][16] = {}, outer[16][16] = {}, stone[16][16] = {};
    // each clump: how far its outer edge stands off the tile border (0 or 1), from the profile; the two end columns of a clump are one px rounder
    static const int OFF[4][4] = {{0, 1, 0, 0}, {1, 0, 0, 1}, {0, 0, 1, 0}, {0, 1, 1, 0}};
    auto isStone = [&](int side, int c) { return (hash2(c * 7 + side * 13 + (prof & 3) * 29, side, 6217u) % 3u) == 0u; };
    auto band = [&](int side, int x, int y) {  // side 0 N, 1 E, 2 S, 3 W: is this pixel in that side's band?
        const int t = (side & 1) ? y : x, a = side == 0 ? y : side == 1 ? 15 - x : side == 2 ? 15 - y : x;
        const int c = t >> 2, r = t & 3, o = OFF[(prof + side) & 3][c];
        const bool end = r == 0 || r == 3;
        const int s0 = o + (end ? 1 : 0), e0 = o + 5 - (end ? 1 : 0);
        return a >= s0 && a < e0;
    };
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const bool on[4] = {N, E, S, W};
            for (int side = 0; side < 4; side++)
                if (on[side] && band(side, x, y)) {
                    M[y][x] = true;
                    stone[y][x] = isStone(side, ((side & 1) ? y : x) >> 2);
                }
        }
    auto corner = [&](bool diag, bool a, bool b, int mx, int my) {  // painted in the north-west frame, mirrored for the other three
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                const int px = mx ? 15 - x : x, py = my ? 15 - y : y;
                const float dx = px + 0.5f, dy = py + 0.5f;
                if (a && b) {  // two open sides meet: the outer corner is rounded off, and the two bands are joined
                    if (px < 5 && py < 5) {
                        const float ex = dx - 5.f, ey = dy - 5.f;
                        M[y][x] = ex * ex + ey * ey <= 4.6f * 4.6f + 3.f;
                        if (M[y][x]) stone[y][x] = false;
                    }
                }
                if (diag && !a && !b) {  // only the diagonal is open: a clump fills the inner corner, with a small bulge of ground bitten out of it
                    if (dx * dx + dy * dy <= 5.0f * 5.0f) M[y][x] = true, stone[y][x] = false;
                    if (dx * dx + dy * dy <= 1.7f * 1.7f) M[y][x] = false, outer[y][x] = true;
                }
            }
    };
    corner(NW, N, W, 0, 0);
    corner(NE, N, E, 1, 0);
    corner(SW, S, W, 0, 1);
    corner(SE, S, E, 1, 1);
    // what lies outside the rim: flood the pixels that are not clumps from the open borders (and from the bitten-out ground)
    int qx[256], qy[256], qn = 0, qh = 0;
    auto seed = [&](int x, int y) {
        if (x < 0 || y < 0 || x > 15 || y > 15 || M[y][x] || outer[y][x]) return;
        outer[y][x] = true;
        qx[qn] = x, qy[qn++] = y;
    };
    for (int i = 0; i < 16; i++) {
        if (N) seed(i, 0);
        if (S) seed(i, 15);
        if (W) seed(0, i);
        if (E) seed(15, i);
    }
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            if (outer[y][x] && !M[y][x]) qx[qn] = x, qy[qn++] = y;  // (the bitten-out bulge, already marked)
    while (qh < qn) {
        const int x = qx[qh], y = qy[qh++];
        static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            const int nx = x + DX[k], ny = y + DY[k];
            if (nx < 0 || ny < 0 || nx > 15 || ny > 15 || M[ny][nx] || outer[ny][nx]) continue;
            outer[ny][nx] = true;
            qx[qn] = nx, qy[qn++] = ny;
        }
    }
    const u32 LINE = HEX(0x5f7aa6), LIT = HEX(0xf4f8fd), BODY = HEX(0xc8d7ea), SHADE = HEX(0x9db3d3), INNER = HEX(0xb1c4df);  // (frosted clumps: darker than the flat snow, or the rim would not show)
    const u32 S_LINE = HEX(0x4a5575), S_LIT = HEX(0xb2bed6), S_BODY = HEX(0x7a86a6), S_SHADE = HEX(0x5d6a8c), S_INNER = HEX(0x65729a);
    auto isOuter = [&](int x, int y) {  // outside the rim: the flooded ground, or beyond the tile across one of its open sides
        if (x < 0) return W;
        if (x > 15) return E;
        if (y < 0) return N;
        if (y > 15) return S;
        return outer[y][x];
    };
    Img out(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (!M[y][x]) continue;
            const bool st = stone[y][x];
            const bool l = isOuter(x - 1, y), r = isOuter(x + 1, y), u = isOuter(x, y - 1), d = isOuter(x, y + 1);
            u32 c;
            if (l || r || u || d) c = st ? S_LINE : LINE;
            else {
                // one px inside the outline: lit on the north-west sides, shaded on the south-east ones
                auto ol = [&](int xx, int yy) { return xx >= 0 && yy >= 0 && xx < 16 && yy < 16 && M[yy][xx] && (isOuter(xx - 1, yy) || isOuter(xx + 1, yy) || isOuter(xx, yy - 1) || isOuter(xx, yy + 1)); };
                const bool nl = ol(x - 1, y), nu = ol(x, y - 1), nr = ol(x + 1, y), nd = ol(x, y + 1);
                const bool inner = (x > 0 && !M[y][x - 1] && !outer[y][x - 1]) || (x < 15 && !M[y][x + 1] && !outer[y][x + 1]) ||
                                   (y > 0 && !M[y - 1][x] && !outer[y - 1][x]) || (y < 15 && !M[y + 1][x] && !outer[y + 1][x]);
                if (nl || nu) c = st ? S_LIT : LIT;
                else if (nr || nd) c = st ? S_SHADE : SHADE;
                else if (inner) c = st ? S_INNER : INNER;
                else c = st ? S_BODY : BODY;
                // the joint between two clumps: a darker pixel run across the clump's inner rows
                bool gap = false;
                for (int side = 0; side < 4; side++) {
                    const bool on[4] = {N, E, S, W};
                    if (!on[side] || !band(side, x, y)) continue;
                    const int t = (side & 1) ? y : x;
                    if ((t & 3) == 0 && !inner) gap = true;
                }
                if (gap && !(nl || nu)) c = st ? S_SHADE : SHADE;
            }
            out.set(x, y, c);
        }
    return out;
}

struct TileBlend {
    std::unordered_map<uint32_t, Spr> lipCache;
    const Spr* lip(SDL_Renderer* r, int cfg, int prof) {
        const uint32_t key = (uint32_t)(cfg & 255) | (uint32_t)(prof & 3) << 8;
        auto it = lipCache.find(key);
        if (it == lipCache.end()) it = lipCache.emplace(key, makeSpr(r, bakeLip(cfg, prof), 0, 0)).first;
        return &it->second;
    }
    Img src[BL_COUNT][4];  // the finished texture of every variant of every blending ground type
    Img cliffSrc[2][3];    // the snowfield's plain cliff block: [0] bare rock face, [1] with the snow cap
    std::unordered_map<uint32_t, Spr> cliffCache;
    const Spr* cliff(SDL_Renderer* r, int cap, int var, int cfg, int prof) {
        var %= 3;
        if (cliffSrc[cap & 1][var].w <= 0) return nullptr;
        const uint32_t key = (uint32_t)(cap & 1) | (uint32_t)var << 1 | (uint32_t)(cfg & 255) << 4 | (uint32_t)(prof & 3) << 12;
        auto it = cliffCache.find(key);
        if (it == cliffCache.end()) it = cliffCache.emplace(key, makeSpr(r, bakeCliff(cliffSrc[cap & 1][var], cfg, prof, CLIFF_SNOW), 0, 0)).first;
        return &it->second;
    }
    int nvar[BL_COUNT] = {};
    std::unordered_map<uint32_t, Spr> cache;
    const Spr* get(SDL_Renderer* r, int cls, int var, int cfg, int prof) {
        if (cls < 0 || cls >= BL_COUNT || nvar[cls] <= 0) return nullptr;
        var %= nvar[cls];
        const uint32_t key = (uint32_t)cls | (uint32_t)var << 4 | (uint32_t)(cfg & 255) << 8 | (uint32_t)(prof & 3) << 16;
        auto it = cache.find(key);
        if (it == cache.end()) it = cache.emplace(key, makeSpr(r, bakeBlend(src[cls][var], cfg, prof, BLEND_RIM[cls]), 0, 0)).first;
        return &it->second;
    }
};
