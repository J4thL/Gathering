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
constexpr u32 BLEND_RIM[BL_COUNT] = {0, 0, ROCKY_DIRT ? HEX(0x4a5878) : 0u, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // 0 = no outline

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

struct TileBlend {
    Img src[BL_COUNT][4];  // the finished texture of every variant of every blending ground type
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
