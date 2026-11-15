// tiles_soil.hpp - the "raised grass" look: where grass meets lower ground (dirt, sand, paving) its soil face shows under it, and the dirt itself
// comes in light, medium and dark tones with a rocky version. Flat top-down tiles in the style of a grass pack: a lip of overhanging blades,
// brown soil with specks and pebbles, a soft shadow on the ground below.
#pragma once
#include "core/gfx.hpp"
#include "world/world.hpp"

// Raised ground: where one kind of ground stands higher than the one south of it, its face shows along the top of the lower tile.
enum LipStyle { LIP_GRASS, LIP_SAND, LIP_SNOW, LIP_STONE, LIP_CURB, LIP_COUNT };
// how high a ground stands (-1: no raised edge, e.g. water, ice, interiors) and what its face looks like
inline int tileRank(uint8_t t) {
    switch (t) {
        case T_MUD: return 0;
        case T_DIRT: return 1;
        case T_SAND: case T_DUNE: case T_SNOWP: return 2;
        case T_GRASS: case T_DARK: case T_SNOW: case T_STONE: return 3;
        case T_COBBLE: return 4;
        default: return -1;
    }
}
inline int tileLipStyle(uint8_t t) {
    switch (t) {
        case T_GRASS: case T_DARK: return LIP_GRASS;
        case T_SAND: case T_DUNE: return LIP_SAND;
        case T_SNOW: case T_SNOWP: return LIP_SNOW;
        case T_STONE: return LIP_STONE;
        default: return LIP_CURB;  // paving
    }
}

// 16 x 10: a fringe (blades, snow, a lit edge) over a face that darkens downward, then a soft shadow on the ground. `variant` (0..3)
// changes the ragged edge, the specks and the stones.
inline Img mkLip(int style, int variant) {
    static const u32 FACE[LIP_COUNT][6] = {{0x9a6238, 0x83512c, 0x6e4224, 0x5a351d, 0x472a17, 0x371f10},   // grass: brown soil
                                           {0xd9bf86, 0xc9a96a, 0xb5934f, 0x9c7a3e, 0x82652f, 0x6b5226},   // sand
                                           {0xdcebf5, 0xc4dcec, 0xa9c6de, 0x8cabc7, 0x7092b0, 0x587998},   // snow: blue-white ice
                                           {0xa6a9b8, 0x8f92a3, 0x787b8d, 0x626576, 0x4d4f5f, 0x3b3d4a},   // stone
                                           {0xb0a898, 0x988f7e, 0x80776a, 0x6a6257, 0x554e45, 0x433d36}};  // paving curb
    static const u32 FRINGE[LIP_COUNT][3] = {{0x29752a, 0x48a948, 0x6fc86e}, {0xe8d7a0, 0xf0e3b4, 0xd9c48a}, {0xf4fafd, 0xffffff, 0xe3f0f8},
                                             {0xc8cad6, 0xd6d8e2, 0xb6b9c8}, {0xd2cabb, 0xe0d9cc, 0xc2baa9}};
    Img im(16, 10);
    Rng r(0x5011u + style * 7919u + variant * 977u);
    const u32* face = FACE[style];
    const u32* fr = FRINGE[style];
    const bool blades = style == LIP_GRASS;
    for (int x = 0; x < 16; x++) {
        const int depth = 5 + r.irange(0, 1);  // face rows under the fringe: the ragged lower edge
        im.set(x, 0, fr[r.irange(1, 2)]);
        im.set(x, 1, r.irange(0, 2) == 0 ? fr[0] : (r.irange(0, 3) == 0 ? fr[1] : face[0]));  // the fringe hangs over the face
        if (blades && r.irange(0, 4) == 0) im.set(x, 2, fr[0]);
        for (int y = 2; y <= depth; y++)
            if (im.get(x, y) == 0) im.set(x, y, face[std::min(5, y - 1)]);
        im.set(x, depth + 1, RGBA(0, 0, 0, 90));  // the shadow it throws on the ground below
        im.set(x, depth + 2, RGBA(0, 0, 0, 45));
        if (depth == 5) im.set(x, depth + 3, RGBA(0, 0, 0, 20));
    }
    for (int i = 0; i < 7; i++) im.set(r.irange(0, 15), r.irange(2, 4), (face[0] + 0x0a0a0au) | 0xff000000u);  // lighter specks
    for (int i = 0; i < 6; i++) im.set(r.irange(0, 15), r.irange(3, 5), face[5] | 0xff000000u);             // dark ones
    if (style == LIP_CURB)
        for (int x = 4 + (variant & 1) * 4; x < 16; x += 8)
            for (int y = 2; y <= 5; y++) im.set(x, y, face[4] | 0xff000000u);  // mortar joints between the facing stones
    else if (style != LIP_SNOW)
        for (int k = 0; k < 2; k++) {  // pebbles set into the face
            const int px = r.irange(1, 13), py = r.irange(2, 4);
            im.set(px, py, HEX(0x9a9488));
            im.set(px + 1, py, HEX(0x7b756d));
            im.set(px, py + 1, HEX(0x5c5750));
        }
    return im;
}

// Dirt in three tones (0 light, 1 medium, 2 dark); `rocky` sets a few stones into it. Every tile is 16 x 16.
inline Img mkDirtTone(int tone, int variant, bool rocky) {
    static const u32 BASE[3] = {0xb5855a, 0x98653f, 0x7a4a2c};
    Img im(16, 16);
    Rng r(0xd177u + tone * 131u + variant * 17u + (rocky ? 9001u : 0u));
    const u32 base = BASE[tone] | 0xff000000u;
    auto shade = [&](int dr) {
        int c[3] = {(int)((BASE[tone] >> 16) & 255) + dr, (int)((BASE[tone] >> 8) & 255) + dr * 3 / 4, (int)(BASE[tone] & 255) + dr / 2};
        for (int& v : c) v = std::max(0, std::min(255, v));
        return HEX((u32)((c[0] << 16) | (c[1] << 8) | c[2]));
    };
    im.fill(base);
    for (int i = 0; i < 26; i++) im.set(r.irange(0, 15), r.irange(0, 15), shade(-22));
    for (int i = 0; i < 14; i++) im.set(r.irange(0, 15), r.irange(0, 15), shade(18));
    for (int i = 0; i < 5; i++) {  // small clods: a light top edge, a dark underside
        const int x = r.irange(1, 13), y = r.irange(1, 13);
        im.set(x, y, shade(20));
        im.set(x + 1, y, shade(12));
        im.set(x, y + 1, shade(-30));
        im.set(x + 1, y + 1, shade(-20));
    }
    if (rocky)
        for (int i = 0, n = 3 + variant % 2; i < n; i++) {
            const int x = r.irange(1, 11), y = r.irange(1, 11), w = r.irange(2, 4);
            for (int dx = 0; dx < w; dx++) im.set(x + dx, y, HEX(0xa49e93));
            for (int dx = 0; dx < w; dx++) im.set(x + dx, y + 1, HEX(0x8a847a));
            for (int dx = 1; dx <= w; dx++) im.set(x + dx, y + 2, HEX(0x5d5851));  // its shadow
            im.set(x, y + 1, HEX(0x77716a));
        }
    return im;
}
