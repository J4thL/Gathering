// sprites.hpp - every sprite in the game is painted here, procedurally, as pixel art.
#pragma once
#include "core/data.hpp"
#include "player/appearance.hpp"
#include "art/sprite_style.hpp"
#include "player/hunter_pose.hpp"
#include <cstring>

static const u32 OUT = HEX(0x241a14);  // universal outline colour
// The pixel-pattern hash of (x, y). Multiplied as unsigned so it cannot overflow (signed overflow is undefined behaviour); the int
// result wraps exactly as the old signed multiply did in practice, so every pattern stays pixel-for-pixel the same.
inline int patHash(int x, int y) { return (int)((uint32_t)x * 73856093u) ^ (int)((uint32_t)y * 19349663u); }
#include "art/sprites_res.hpp"
#include "art/sprites_trophy.hpp"
#include "art/sprites_gear.hpp"
#include "delve/sprites_delve.hpp"
#include "farming/sprites_homestead.hpp"
#include "player/hunter_chibi.hpp"
#include "art/tile_edges.hpp"
#include "art/sprites_forest.hpp"
#include "art/sprites_biome.hpp"
#include "npcs/sprites_town.hpp"
#include "art/sprites_carcass.hpp"
#include "art/sprites_ores.hpp"
#include "art/tree_lime.hpp"
#include "art/tree_pine.hpp"
#include "art/tree_snow_spruce.hpp"
#include "art/tree_dead_spruce.hpp"
#include "art/rock_sprites.hpp"
#include "art/tiles_user.hpp"
#include "art/tiles_water.hpp"
#include "art/tiles_soil.hpp"
#include "art/tiles_grasspack.hpp"

// ================================================================= tiles
// Ground grass, painted from the same green ramp as the grass around trees and bushes (tree_grass.hpp: 0x7cc450 / 0x4a963c / 0x286830,
// the tuft: 0x6cb94a / 0x3f8b30), so a tile, a tree's grass skirt and a tuft all read as one grass.
//   variant 0: little - slender, sparse blades on a lighter ground
//   variant 1: average
//   variant 2: lush - dense, taller, clumped blades with dark roots on a deeper ground
//   variant 3: the darker grass (forest floor)
// It draws the same number of random numbers from `r` as the old painter did and uses its own generator, so every sprite built
// after it is unchanged.
inline Img mkGrass(int variant, Rng& r) {
    for (int i = 0; i < 2 * (9 + variant) + 10; i++) r.next();
    Rng g(9100 + variant * 131);
    constexpr u32 G_LIGHT = HEX(0x7cc450), G_MID = HEX(0x4a963c), G_DEEP = HEX(0x286830);  // the object-grass ramp
    struct Look { u32 base, dk, lt; int nDark, nLight, nTip, hMin, hMax, nRoot; bool pairs; };
    static const Look L[4] = {
        {HEX(0x5ca843), HEX(0x4e9c3f), HEX(0x6eb84a), 4, 3, 0, 2, 2, 0, false},   // little
        {HEX(0x5aa542), HEX(0x4a963c), HEX(0x6cb94a), 8, 5, 1, 2, 3, 0, false},   // average
        {HEX(0x58a241), HEX(0x3e8638), HEX(0x68b448), 12, 8, 3, 3, 4, 5, true},   // lush
        {HEX(0x46903a), HEX(0x3c8337), HEX(0x5aa844), 8, 5, 0, 2, 3, 0, false},   // dark
    };
    const Look& lk = L[variant & 3];
    Img im(16, 16);
    im.fill(lk.base);
    auto px = [&](int x, int y, u32 c) { im.set(((x % 16) + 16) % 16, ((y % 16) + 16) % 16, c); };
    auto blade = [&](int x, int y, int h, u32 c) {
        for (int k = 0; k < h; k++) px(x, y - k, c);
    };
    for (int i = 0; i < lk.nDark; i++) {
        const int x = g.irange(0, 15), y = g.irange(3, 15), h = g.irange(lk.hMin, lk.hMax);
        blade(x, y, h, lk.dk);
        if (lk.pairs && g.chance(0.6f)) blade(x + 1, y, std::max(2, h - 1), lk.dk);  // lush grass grows in clumps
        if (i < lk.nRoot) px(x, y, G_DEEP);                                           // a dark root at the foot of some blades
    }
    for (int i = 0; i < lk.nLight; i++) blade(g.irange(0, 15), g.irange(3, 15), g.irange(lk.hMin, lk.hMax), lk.lt);
    for (int i = 0; i < lk.nTip; i++) blade(g.irange(0, 15), g.irange(3, 15), 2, G_LIGHT);
    (void)G_MID;
    return im;
}
// the old brown dirt
inline Img mkDirtBrown(int variant, Rng& r) {
    Img im(16, 16);
    im.fill(HEX(0xa87c4f));
    for (int i = 0; i < 18; i++) im.set(r.irange(0, 15), r.irange(0, 15), HEX(0x956b42));
    for (int i = 0; i < 10; i++) im.set(r.irange(0, 15), r.irange(0, 15), HEX(0xbd935f));
    for (int i = 0; i < 3 + variant; i++) {
        int x = r.irange(1, 13), y = r.irange(1, 13);
        im.set(x, y, HEX(0x8c8379));
        im.set(x + 1, y, HEX(0xa59d92));
        im.set(x, y + 1, HEX(0x6f685f));
    }
    return im;
}
// cobbled rocky ground: irregular stones with dark gaps, a lit top-left edge and a shaded underside; it wraps, so it tiles
inline Img mkDirtRock(int variant, Rng& r) {
    for (int i = 0; i < 62 + 2 * variant; i++) r.next();
    Rng g(4200 + variant * 977);
    const int N = 7 + variant;
    float sx[16], sy[16];
    u32 col[16];
    const u32 pal[4] = {HEX(0x847c5c), HEX(0x796b54), HEX(0x746454), HEX(0x8a8264)};
    for (int i = 0; i < N; i++) {
        sx[i] = g.range(0, 16);
        sy[i] = g.range(0, 16);
        col[i] = pal[g.irange(0, 3)];
    }
    auto owner = [&](int x, int y, float* gap = nullptr) {
        x = (x + 16) % 16;
        y = (y + 16) % 16;
        int best = 0;
        float bd = 1e9f, bd2 = 1e9f;
        for (int i = 0; i < N; i++) {
            float dx = fabsf(x + 0.5f - sx[i]), dy = fabsf(y + 0.5f - sy[i]);
            dx = std::min(dx, 16 - dx);
            dy = std::min(dy, 16 - dy);
            const float d = dx * dx + dy * dy * 1.3f;
            if (d < bd) { bd2 = bd; bd = d; best = i; }
            else if (d < bd2) bd2 = d;
        }
        if (gap) *gap = bd2 - bd;
        return best;
    };
    Img im(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            float gap;
            const int o = owner(x, y, &gap);
            if (gap < 3.2f) { im.set(x, y, HEX(0x5c4c4c)); continue; }
            u32 c = col[o];
            if (owner(x, y - 1) != o || owner(x - 1, y) != o) c = HEX(0x948c6c);
            else if (owner(x, y + 1) != o) c = HEX(0x6a5a4c);
            im.set(x, y, c);
        }
    return im;
}
inline Img mkDirt(int variant, Rng& r) { return ROCKY_DIRT ? mkDirtRock(variant, r) : mkDirtBrown(variant, r); }
inline Img mkWater(int frame) {
    Img im(16, 16);
    im.fill(HEX(0x3c7fc7));
    for (int k = 0; k < 4; k++) {
        int x = (k * 5 + frame * 4) % 16, y = (k * 7 + 2) % 16;
        for (int i = 0; i < 3; i++) im.set((x + i) % 16, y, HEX(0x86bdf0));
        int x2 = (k * 9 + 3 + frame * 4) % 16, y2 = (k * 5 + 9) % 16;
        for (int i = 0; i < 2; i++) im.set((x2 + i) % 16, y2, HEX(0x3270b4));
    }
    return im;
}

// ================================================================= decor
// Meadow flowers: flat top-down blossoms, a plus of petals round a centre, with a teal shadow pixel (the look of the reference grass).
//   variant 0: orange - 2x2 orange-red centre with a dark dot, four cream-peach petals
//   variant 1: cream  - a small plus of pale yellow petals round a warm centre
inline Img mkFlower(int variant) {
    Img im(8, 8);
    const u32 PEACH = HEX(0xe4c47c), ORANGE = HEX(0xcc7c54), DARK = HEX(0xa85c28), CREAM = HEX(0xf4f4ac), WARM = HEX(0xdc9a4e), SHADE = HEX(0x4f8a80);
    if (variant == 0) {
        im.set(3, 2, PEACH); im.set(4, 2, PEACH);                                    // top petals
        im.set(2, 3, PEACH); im.set(3, 3, ORANGE); im.set(4, 3, ORANGE); im.set(5, 3, PEACH);
        im.set(2, 4, PEACH); im.set(3, 4, ORANGE); im.set(4, 4, DARK);   im.set(5, 4, PEACH);
        im.set(3, 5, PEACH); im.set(4, 5, PEACH);                                    // bottom petals
        im.set(2, 5, SHADE); im.set(3, 6, SHADE);                                    // shadow, lower left
    } else if (variant == 1) {
        im.set(3, 3, CREAM);
        im.set(2, 4, CREAM); im.set(3, 4, WARM); im.set(4, 4, CREAM);
        im.set(3, 5, CREAM);
        im.set(2, 5, SHADE);
    } else if (variant == 2) {  // violet: a plus of lilac petals round a yellow centre, a taller bloom
        const u32 LILAC = HEX(0xb08ae0), LIGHT = HEX(0xd2b8f4), YEL = HEX(0xf2d860);
        im.set(3, 1, LIGHT);
        im.set(3, 2, LILAC);
        im.set(2, 3, LILAC); im.set(3, 3, YEL); im.set(4, 3, LILAC);
        im.set(3, 4, LILAC);
        im.set(2, 4, SHADE); im.set(3, 5, SHADE);
    } else {  // poppy: four broad red petals round a dark heart
        const u32 RED = HEX(0xd8463a), LIGHTRED = HEX(0xf07a5a), HEART = HEX(0x3a2428);
        im.set(3, 2, LIGHTRED); im.set(4, 2, RED);
        im.set(2, 3, RED); im.set(3, 3, HEART); im.set(4, 3, HEART); im.set(5, 3, RED);
        im.set(2, 4, RED); im.set(3, 4, HEART); im.set(4, 4, RED); im.set(5, 4, LIGHTRED);
        im.set(3, 5, RED); im.set(4, 5, RED);
        im.set(2, 5, SHADE); im.set(3, 6, SHADE);
    }
    return im;
}
inline Img mkTuft() {
    Img im(8, 8);
    u32 a = HEX(0x3f8b30), b = HEX(0x6cb94a);
    im.line(4, 7, 2, 2, a);
    im.line(4, 7, 4, 1, b);
    im.line(4, 7, 6, 3, a);
    im.line(3, 7, 1, 4, b);
    return im;
}
inline Img mkMushroom() {
    Img im(8, 8);
    im.rect(3, 5, 2, 3, HEX(0xefe3c8));
    im.ellipse(4, 4, 3, 2.2f, HEX(0xd4443a));
    im.set(3, 3, HEX(0xffffff));
    im.set(5, 4, HEX(0xffffff));
    return im;
}
// Grass growing round the foot of a minable rock (tiny / small / large), drawn over the rock's lower edge like the grass around a tree.
// Same green ramp as the ground grass and the trees. `frame` sways the blade tips; each rock look gets its own fixed pattern of blades.
constexpr int ROCK_GRASS_FRAMES = 6;
inline Img mkRockGrass(int rockW, int kind, int look, int frame) {
    const int W = rockW + 6, H = 6 + kind * 2;              // kind 0 tiny, 1 small, 2 large: bigger rocks get taller grass
    constexpr u32 G_LIGHT = HEX(0x7cc450), G_MID = HEX(0x4a963c), G_DEEP = HEX(0x286830);
    Img im(W, H);
    Rng g(31000 + kind * 977 + look * 131);                  // fixed per look, the same in every frame
    const float sw = sinf(frame * 6.2832f / ROCK_GRASS_FRAMES);
    const int n = W / 2 + 2;
    for (int i = 0; i < n; i++) {
        const int x = g.irange(0, W - 1);
        const int edge = std::min(x, W - 1 - x);              // blades near the sides stand lower, so the clump hugs the rock
        const int baseY = H - 1 - g.irange(0, 1);
        const int hgt = std::max(2, std::min(H - 2, 2 + g.irange(0, 2 + kind) - (edge < 2 ? 1 : 0)));
        const float ph = g.f() * 6.2832f;
        const u32 col = g.chance(0.5f) ? G_MID : (g.chance(0.5f) ? G_LIGHT : G_DEEP);
        const u32 tip = g.chance(0.4f) ? G_LIGHT : col;
        for (int k = 0; k < hgt; k++) {
            const float bend = (float)k / hgt;                // the tip moves most
            const int dx = (int)lroundf((sw * 0.9f + sinf(ph) * 0.3f) * bend * 1.6f);
            im.set(x + dx, baseY - k, k == hgt - 1 ? tip : col);
        }
        if (hgt >= 3 && g.chance(0.4f)) im.set(x, baseY, G_DEEP);  // dark root
    }
    return im;
}
inline Img mkPebbles() {
    Img im(8, 6);
    im.ellipse(3, 3.5f, 2.2f, 1.6f, HEX(0x9a9fa8));
    im.ellipse(5.5f, 4, 1.6f, 1.2f, HEX(0x7e838e));
    im.set(2, 2, HEX(0xc3c7ce));
    return im;
}
// Wildwood bush: variant 0 = plain, variant 1 = orange berries.
inline Img mkBush(int variant, Rng& r) {
    const BushPal P{HEX(0x17381a), HEX(0x2b5f26), HEX(0x468a30), HEX(0x6fb046), HEX(0xa2d06c), HEX(0x7cc450)};
    Img im = fuzzyShrub(P, variant == 0 ? 11 : 10, 0.24f, variant == 0 ? 11.2f : 10.2f, variant == 0 ? 7.6f : 7.0f, 52, r);
    auto at = [&](int x, int y) { return (im.get(x, y) >> 24) != 0; };
    if (variant == 1) {  // berries: 2x2, lit top-left, kept off the rim and apart from each other
        const u32 B0 = HEX(0xe8602a), B1 = HEX(0xffd27a), B2 = HEX(0xb8401c);
        int bx[16], by[16], n = 0;
        for (int tries = 0; tries < 200 && n < 9; tries++) {
            const int x = r.irange(4, 24 - 6), y = r.irange(3, 16 - 4);
            if (!at(x, y) || !at(x + 1, y) || !at(x, y + 1) || !at(x + 1, y + 1)) continue;
            bool ok = true;
            for (int k = 0; k < n; k++) if (abs(bx[k] - x) < 4 && abs(by[k] - y) < 3) ok = false;
            if (!ok) continue;
            bx[n] = x; by[n] = y; n++;
            im.set(x, y, B1); im.set(x + 1, y, B0); im.set(x, y + 1, B0); im.set(x + 1, y + 1, B2);
        }
    } else {  // a few bright leaf tips on the lit side
        for (int i = 0; i < 7; i++) {
            const int x = r.irange(3, 24 - 8), y = r.irange(2, 8);
            if (at(x, y) && im.get(x, y) == P.lt) im.set(x, y, P.hl);
        }
    }
    return im;
}

// The homestead's flowering bush: the meadow shrub with small blue star flowers scattered over it (the plain round one is shared).
inline Img mkFlowerBush(Rng& r) {
    const BushPal P{HEX(0x17381a), HEX(0x2b5f26), HEX(0x468a30), HEX(0x6fb046), HEX(0xa2d06c), HEX(0x7cc450)};
    Img im = fuzzyShrub(P, 10, 0.24f, 10.4f, 7.2f, 52, r);
    const u32 CEN = HEX(0xf4f8ff), ARM = HEX(0x5f8cf2), ARM_DK = HEX(0x3f62c6);
    int bx[16], by[16], n = 0;
    for (int tries = 0; tries < 200 && n < 10; tries++) {
        const int x = r.irange(5, 18), y = r.irange(4, 13);
        const float dx = (x + .5f - 11.5f) / 10.4f, dy = (y + .5f - 9.5f) / 7.2f;
        if (dx * dx + dy * dy > 0.55f || (im.get(x, y) >> 24) == 0) continue;
        bool ok = true;
        for (int k = 0; k < n; k++) if (abs(bx[k] - x) < 4 && abs(by[k] - y) < 3) ok = false;
        if (!ok) continue;
        bx[n] = x; by[n] = y; n++;
        im.set(x, y, CEN);
        im.set(x - 1, y, ARM); im.set(x + 1, y, ARM); im.set(x, y - 1, ARM); im.set(x, y + 1, ARM_DK);
    }
    return im;
}

// ================================================================= trees & rocks
inline Img mkOak(int variant, Rng& r) {
    Img im(34, 42);
    u32 dk = HEX(0x2f6b2b), md = HEX(0x3f8a34), lt = HEX(0x5fb046), hi = HEX(0x86cf5f);
    if (variant == 1) {
        dk = HEX(0x4a6b22);
        md = HEX(0x6b9a2c);
        lt = HEX(0x8bbb3a);
        hi = HEX(0xb5d96a);
    }
    // trunk
    im.rect(14, 28, 6, 12, HEX(0x7a5230));
    im.rect(14, 28, 2, 12, HEX(0x93663b));
    im.rect(18, 28, 2, 12, HEX(0x5e3d22));
    im.rect(12, 38, 10, 2, HEX(0x5e3d22));
    im.set(13, 37, HEX(0x7a5230));
    im.set(20, 37, HEX(0x5e3d22));
    // canopy
    im.ellipseShaded(17, 24, 12, 7, lt, md, dk);
    im.ellipseShaded(9, 19, 8, 8, lt, md, dk);
    im.ellipseShaded(25, 19, 8, 8, lt, md, dk);
    im.ellipseShaded(17, 13, 11, 11, lt, md, dk);
    for (int i = 0; i < 26; i++) {
        int x = r.irange(6, 28), y = r.irange(4, 26);
        if (cA(im.get(x, y)) && im.get(x, y) != dk) im.set(x, y, hi);
        im.set(x + 1, y + 2, dk);
    }
    im.outline(OUT);
    return im;
}
inline Img mkPine(Rng& r) {
    Img im(26, 44);
    im.rect(11, 34, 4, 8, HEX(0x6b4423));
    im.rect(13, 34, 2, 8, HEX(0x52341a));
    u32 dk = HEX(0x1f5238), md = HEX(0x2d6f47), lt = HEX(0x4a9a58);
    struct T {
        int top, bot, hw;
    } tiers[3] = {{2, 17, 8}, {10, 28, 10}, {20, 38, 12}};
    for (auto& t : tiers) {
        for (int y = t.top; y <= t.bot; y++) {
            float k = (float)(y - t.top) / (t.bot - t.top);
            int hw = (int)(1 + k * t.hw);
            for (int x = 13 - hw; x <= 12 + hw; x++) {
                u32 c = md;
                if (x < 13 - hw + 2) c = lt;
                if (x > 12 + hw - 3) c = dk;
                if (y >= t.bot - 1) c = dk;
                im.set(x, y, c);
            }
        }
    }
    for (int i = 0; i < 14; i++) {
        int x = r.irange(6, 20), y = r.irange(6, 36);
        if (cA(im.get(x, y))) im.set(x, y, lt);
    }
    im.outline(OUT);
    return im;
}
inline Img mkRock(int variant) {
    if (variant == 0) {
        Img im(20, 15);
        im.ellipseShaded(9, 8, 8, 5.5f, HEX(0xb8bcc6), HEX(0x8d929e), HEX(0x646a78));
        im.ellipseShaded(15, 10, 4, 3, HEX(0xb8bcc6), HEX(0x8d929e), HEX(0x646a78));
        im.line(6, 5, 9, 8, HEX(0x5a5f6c));
        im.set(5, 5, HEX(0xdfe2e8));
        im.set(6, 4, HEX(0xdfe2e8));
        im.outline(OUT);
        return im;
    }
    Img im(24, 18);
    im.ellipseShaded(12, 10, 10, 6.5f, HEX(0xb8bcc6), HEX(0x8d929e), HEX(0x646a78));
    im.ellipseShaded(7, 12, 5, 3.5f, HEX(0xb8bcc6), HEX(0x8d929e), HEX(0x646a78));
    for (int i = 0; i < 9; i++) im.set(7 + i, 5 + (i % 3 == 0 ? 1 : 0), HEX(0x6ea04a));  // moss
    im.line(12, 7, 14, 11, HEX(0x5a5f6c));
    im.outline(OUT);
    return im;
}

// ================================================================= village
inline Img mkHut() {
    Img im(66, 58);
    u32 stone = HEX(0x7d7a80);
    // chimney (behind the roof)
    im.rect(46, 0, 7, 14, stone);
    im.rect(46, 0, 7, 2, HEX(0x5d5a60));
    im.rect(50, 2, 3, 12, HEX(0x6a676e));
    // roof: trapezoid of shingles
    for (int y = 3; y <= 27; y++) {
        float k = (float)(y - 3) / 24.f;
        int hw = (int)(17 + k * 15);
        for (int x = 33 - hw; x <= 32 + hw; x++) {
            u32 c = HEX(0x9c3b2c);
            if ((y % 4) == 0) c = HEX(0x7e2e22);
            else if (((x + (y / 4) * 3) % 6) == 0) c = HEX(0xb34a37);
            if (x <= 33 - hw + 1) c = HEX(0xc2573f);
            if (x >= 32 + hw - 1) c = HEX(0x6e281d);
            im.set(x, y, c);
        }
    }
    im.rect(1, 27, 64, 3, HEX(0x5c2018));
    im.rect(1, 27, 64, 1, HEX(0x7e2e22));
    // wall
    im.rect(9, 30, 48, 26, HEX(0xb98450));
    for (int x = 9; x < 57; x += 6) im.rect(x, 30, 1, 26, HEX(0x9a6c40));
    for (int x = 12; x < 57; x += 6) im.set(x, 36 + (x % 5), HEX(0xcf9a62));
    im.rect(9, 52, 48, 4, HEX(0x77747c));
    for (int x = 9; x < 57; x += 6) im.rect(x, 52, 1, 4, HEX(0x5e5b63));
    im.rect(9, 30, 48, 1, HEX(0x8a5c34));
    // door
    im.rect(28, 36, 12, 20, HEX(0x4a2c14));
    im.rect(29, 37, 10, 19, HEX(0x6b4423));
    for (int x = 29; x < 39; x += 3) im.rect(x, 37, 1, 19, HEX(0x573619));
    im.rect(33, 37, 1, 19, HEX(0x4a2c14));
    im.set(37, 47, HEX(0xf2c94c));
    im.rect(28, 36, 12, 1, HEX(0x3b2616));
    // windows (warm light)
    for (int wx : {14, 46}) {
        im.rect(wx - 1, 35, 10, 9, HEX(0x5c3b1e));
        im.rect(wx, 36, 8, 7, HEX(0xf2d98b));
        im.rect(wx, 36, 8, 2, HEX(0xfaeab0));
        im.rect(wx + 3, 36, 1, 7, HEX(0x5c3b1e));
        im.rect(wx, 39, 8, 1, HEX(0x5c3b1e));
        im.rect(wx - 2, 44, 12, 2, HEX(0x7a5230));
        im.set(wx + 1, 43, HEX(0xd8443f));
        im.set(wx + 6, 43, HEX(0xf2c94c));
    }
    // doormat
    im.rect(29, 56, 10, 1, HEX(0x8b3a3a));
    im.outline(OUT);
    return im;
}
inline Img mkSign() {
    Img im(32, 26);
    im.rect(14, 12, 3, 13, HEX(0x6b4423));
    im.rect(16, 12, 1, 13, HEX(0x52341a));
    im.rect(2, 2, 28, 12, HEX(0xc99a5a));
    im.rect(2, 2, 28, 1, HEX(0xe3b97a));
    im.rect(2, 13, 28, 1, HEX(0x8a5c34));
    im.rect(3, 3, 26, 1, HEX(0x8a5c34));
    im.text(8, 6, "TOWN", HEX(0x3b2616));
    im.set(4, 4, HEX(0x52341a));
    im.set(27, 4, HEX(0x52341a));
    im.outline(OUT);
    return im;
}
inline Img mkCrate() {
    Img im(16, 16);
    im.rect(2, 3, 12, 11, HEX(0xb07a45));
    im.rect(2, 3, 12, 2, HEX(0xc99a5a));
    im.rect(2, 3, 2, 11, HEX(0x8a5c34));
    im.rect(12, 3, 2, 11, HEX(0x8a5c34));
    im.rect(2, 12, 12, 2, HEX(0x8a5c34));
    im.line(3, 5, 12, 12, HEX(0x8a5c34));
    im.line(12, 5, 3, 12, HEX(0x8a5c34));
    im.outline(OUT);
    return im;
}
inline Img mkBarrel() {
    Img im(14, 18);
    im.ellipse(7, 9.5f, 5.5f, 7.5f, HEX(0x8a5c34));
    im.ellipse(5.5f, 8.5f, 3, 6, HEX(0xa8743f));
    im.rect(2, 4, 10, 1, HEX(0x3b2616));
    im.rect(1, 9, 12, 1, HEX(0x3b2616));
    im.rect(2, 14, 10, 1, HEX(0x3b2616));
    im.ellipse(7, 3, 4.5f, 1.8f, HEX(0xc99a5a));
    im.outline(OUT);
    return im;
}
inline Img mkFire(int frame) {
    Img im(18, 20);
    // stones
    for (int i = 0; i < 8; i++) {
        float a = i * 0.785f;
        im.ellipse(9 + cosf(a) * 6, 15 + sinf(a) * 3, 1.8f, 1.5f, (i % 2) ? HEX(0x8d929e) : HEX(0x6a6f7c));
    }
    // logs
    im.line(4, 16, 13, 13, HEX(0x6b4423));
    im.line(4, 15, 13, 12, HEX(0x8d5e33));
    im.line(5, 13, 13, 17, HEX(0x52341a));
    im.line(5, 12, 13, 16, HEX(0x7a5230));
    // flame
    float hgt[3] = {8, 10, 7};
    float h = hgt[frame % 3];
    im.ellipse(9, 12 - h * 0.35f, 4.2f - frame * 0.3f, h * 0.55f, HEX(0xe0452a));
    im.ellipse(9, 12 - h * 0.28f, 3.0f, h * 0.45f, HEX(0xff8a2b));
    im.ellipse(9 + (frame - 1) * 0.6f, 12 - h * 0.18f, 1.8f, h * 0.3f, HEX(0xffd34d));
    im.set(9, 4 + frame, HEX(0xffd34d));
    return im;
}
inline Img mkShopkeeper(int frame) {
    Img im(18, 26);
    int bob = frame ? 1 : 0;
    u32 skin = HEX(0xf0c49a), hat = HEX(0x6b4a2b), hatD = HEX(0x4e3520);
    // legs
    im.rect(5, 19, 3, 5, HEX(0x4a3b2a));
    im.rect(10, 19, 3, 5, HEX(0x4a3b2a));
    im.rect(5, 23, 3, 1, HEX(0x2e2118));
    im.rect(10, 23, 3, 1, HEX(0x2e2118));
    // body: red shirt + cream apron
    im.rect(4, 11 - bob, 10, 9, HEX(0xb23a3a));
    im.rect(6, 13 - bob, 6, 7, HEX(0xe8dcc0));
    im.rect(6, 13 - bob, 6, 1, HEX(0xcbbf9f));
    im.rect(4, 11 - bob, 10, 1, HEX(0xd24d4d));
    // arms
    im.rect(2, 12 - bob * 2, 2, 5, HEX(0xb23a3a));
    im.rect(14, 12 - bob * 2, 2, 5, HEX(0xb23a3a));
    im.set(2, 17 - bob * 2, skin);
    im.set(3, 17 - bob * 2, skin);
    im.set(14, 17 - bob * 2, skin);
    im.set(15, 17 - bob * 2, skin);
    // head
    im.ellipse(9, 8 - bob, 3.6f, 3.4f, skin);
    im.set(7, 8 - bob, HEX(0x2a1c1c));
    im.set(11, 8 - bob, HEX(0x2a1c1c));
    im.rect(7, 10 - bob, 5, 1, HEX(0x6b4a2b));  // moustache
    // hat
    im.rect(3, 5 - bob, 12, 2, hat);
    im.rect(3, 6 - bob, 12, 1, hatD);
    im.ellipse(9, 4 - bob, 4, 3, hat);
    im.rect(6, 5 - bob, 6, 1, HEX(0xf2c94c));
    im.outline(OUT);
    return im;
}
// the quest giver: a hooded elder hunter with a white beard and a staff
inline Img mkQuestGiver(int frame) {
    Img im(18, 26);
    int bob = frame ? 1 : 0;
    const u32 skin = HEX(0xe9bd94), robe = HEX(0x3d6b78), robeD = HEX(0x2b4f5a), robeL = HEX(0x5a8d9a),
              beard = HEX(0xe9e4d6), beardD = HEX(0xc3bda9), wood = HEX(0x6b4423), woodL = HEX(0x8d5e33),
              gold = HEX(0xf2c94c), rope = HEX(0x3b2616), eye = HEX(0x2a1c1c);
    // boots
    im.rect(5, 22, 3, 2, HEX(0x2e2118));
    im.rect(10, 22, 3, 2, HEX(0x2e2118));
    // long robe, flaring toward the hem
    for (int y = 11; y <= 22; y++) {
        int hw = 4 + (y - 11) * 3 / 11;
        for (int x = 9 - hw; x < 9 + hw; x++) {
            u32 c = robe;
            if (x < 9 - hw + 2) c = robeL;
            if (x >= 9 + hw - 2) c = robeD;
            if (y == 22) c = robeD;
            im.set(x, y, c);
        }
    }
    im.rect(5, 16, 8, 1, rope);
    im.set(9, 16, gold);
    // arms: left hangs down, right holds the staff
    im.rect(3, 12, 2, 5, robeD);
    im.set(3, 17, skin);
    im.set(4, 17, skin);
    im.rect(13, 12, 2, 4, robeD);
    im.set(14, 16, skin);
    im.set(15, 16, skin);
    // staff with a golden knob
    im.rect(15, 5, 1, 19, wood);
    im.set(16, 5, woodL);
    im.ellipse(15.5f, 4, 1.8f, 1.8f, gold);
    // hood and face
    im.ellipse(9, 7 - bob, 4.6f, 4.6f, robe);
    im.ellipse(9, 6 - bob, 3.6f, 2.4f, robeL);
    im.ellipse(9, 9 - bob, 3.0f, 3.0f, skin);
    im.set(8, 9 - bob, eye);
    im.set(10, 9 - bob, eye);
    // beard over the chest
    im.rect(7, 11 - bob, 5, 3, beard);
    im.rect(8, 14 - bob, 3, 1, beard);
    im.set(8, 14 - bob, beardD);
    im.set(7, 13 - bob, beardD);
    im.outline(OUT);
    return im;
}

// ================================================================= hunter
// the original four-frame walk of the three base views (still used by the salon preview and the appearance tests)
inline Img mkPlayerPose(int dir, const HPose& P, const HunterLook& look = HunterLook()) { return mkChibiPose(dir, P, look); }  // the chibi hunter (hunter_chibi.hpp)
inline Img mkPlayer(int dir, int f, const HunterLook& look = HunterLook()) { return mkPlayerPose(dir, walkPose(f), look); }


// ================================================================= animals
struct QuadSpec {
    int W, H;
    float cx, bw, bh;
    int legLen, legW;
    u32 body, light, belly, dark, hoof, snout, eye;
    float hr, hdx, hdy, snoutLen;
    int ear, tail;
    int antlers;  // 0 none, 1 antlers, 2 big antlers
    bool tusks, ridge, spots;
    int horns;    // 0 none, 1 short goat horns, 2 curled ram horns, 3 flat palmate moose antlers
    int mane;     // 1 = mane around the head (drawn in the 'dark' colour)
    u32 horn;     // horn colour (0 = default dark)
};

inline QuadSpec specRabbit() {
    QuadSpec s{};
    s.W = 24; s.H = 18; s.cx = 9; s.bw = 10; s.bh = 6.5f; s.legLen = 3; s.legW = 1;
    s.body = HEX(0xb59a7b); s.light = HEX(0xcbb596); s.belly = HEX(0xe8dcc8); s.dark = HEX(0x8c7357);
    s.hoof = HEX(0x6d563c); s.snout = HEX(0xe8dcc8); s.eye = HEX(0x241a14);
    s.hr = 3; s.hdx = 0.5f; s.hdy = 0.5f; s.snoutLen = 0; s.ear = 1; s.tail = 1;
    return s;
}
inline QuadSpec specDeer(bool stag) {
    QuadSpec s{};
    s.W = 36; s.H = 36; s.cx = 14; s.bw = 18; s.bh = 8.5f; s.legLen = 11; s.legW = 2;
    // the pixel-art reference: two close warm browns, maroon far legs and shade, a cream belly, near-black hooves and nose
    s.body = HEX(0x823e19); s.light = HEX(0x8f4620); s.belly = HEX(0xd8a284); s.dark = HEX(0x5b131b);
    s.hoof = HEX(0x3d0d12); s.snout = HEX(0x793819); s.eye = HEX(0x2a0a0e);
    s.hr = 3.2f; s.hdx = 3.5f; s.hdy = -6.5f; s.snoutLen = 3.5f; s.ear = 2; s.tail = 2;
    s.antlers = stag; s.spots = false;
    return s;
}
inline QuadSpec specBoar() {
    QuadSpec s{};
    s.W = 36; s.H = 26; s.cx = 14; s.bw = 18; s.bh = 11; s.legLen = 4; s.legW = 2;
    s.body = HEX(0x8a4b30); s.light = HEX(0xa9663f); s.belly = HEX(0x5c3222); s.dark = HEX(0x3a2018);
    s.hoof = HEX(0x1c1410); s.snout = HEX(0xc99a86); s.eye = HEX(0xf2c94c);
    s.hr = 4.2f; s.hdx = 1.5f; s.hdy = 1.5f; s.snoutLen = 4.5f; s.ear = 4; s.tail = 4;
    s.tusks = true; s.ridge = true;
    return s;
}
inline QuadSpec specWolf() {
    QuadSpec s{};
    s.W = 38; s.H = 28; s.cx = 15; s.bw = 17; s.bh = 7.5f; s.legLen = 8; s.legW = 2;
    s.body = HEX(0x7d8591); s.light = HEX(0x9aa2ad); s.belly = HEX(0xcfd3d8); s.dark = HEX(0x545b66);
    s.hoof = HEX(0x2e323a); s.snout = HEX(0xd0cdc8); s.eye = HEX(0xf2c94c);
    s.hr = 3.5f; s.hdx = 3; s.hdy = -1.5f; s.snoutLen = 4.5f; s.ear = 3; s.tail = 3;
    return s;
}

#include "animals/sprites_fauna.hpp"
#include "animals/sprites_rig.hpp"
#include "combat/monster_art.hpp"
#include "farming/dom_art.hpp"

// ================================================================= icons & UI art
inline Img mkIconMat(int m) {
    Img im(16, 16);
    switch (m) {
        case M_MEAT:
            im.ellipse(7.5f, 8.5f, 5.5f, 4.5f, HEX(0xd65a56));
            im.ellipse(6.5f, 7.5f, 3.5f, 2.2f, HEX(0xe8807b));
            im.line(4, 9, 9, 10, HEX(0xf4c0b8));
            im.rect(11, 5, 2, 5, HEX(0xf2ead3));
            im.set(12, 4, HEX(0xf2ead3));
            im.set(13, 4, HEX(0xf2ead3));
            im.set(12, 10, HEX(0xf2ead3));
            im.set(13, 10, HEX(0xf2ead3));
            break;
        case M_HIDE:
            im.ellipse(8, 8, 5.5f, 5, HEX(0xc08b5a));
            im.rect(2, 4, 3, 3, HEX(0xc08b5a));
            im.rect(11, 4, 3, 3, HEX(0xc08b5a));
            im.rect(3, 10, 3, 3, HEX(0xc08b5a));
            im.rect(10, 10, 3, 3, HEX(0xc08b5a));
            im.ellipse(7, 7, 3, 2.5f, HEX(0xd6a373));
            im.line(5, 9, 11, 9, HEX(0x9b6a3f));
            im.set(8, 5, HEX(0x9b6a3f));
            break;
        case M_BONE:
            im.line(4, 11, 11, 4, HEX(0xf2ead3));
            im.line(5, 11, 12, 4, HEX(0xd9cfae));
            im.ellipse(3.5f, 11.5f, 2, 2, HEX(0xf2ead3));
            im.ellipse(5, 13, 2, 1.5f, HEX(0xf2ead3));
            im.ellipse(12.5f, 3.5f, 2, 2, HEX(0xf2ead3));
            im.ellipse(11, 2, 2, 1.5f, HEX(0xf2ead3));
            im.set(3, 11, HEX(0xffffff));
            break;
        case M_ORGAN:
            im.ellipse(8, 8, 5.5f, 4.5f, HEX(0x9b2d3c));
            im.ellipse(6.5f, 6.5f, 3, 2, HEX(0xc24457));
            im.rect(7, 2, 2, 3, HEX(0x7a1f2c));
            im.set(5, 6, HEX(0xe88a99));
            im.set(11, 10, HEX(0x6e1a26));
            break;
        case M_FUR:
            im.ellipse(8, 8, 5.5f, 5, HEX(0xb9bec7));
            im.ellipse(6.5f, 6.5f, 3.5f, 3, HEX(0xdde0e6));
            for (int x = 3; x <= 12; x += 2) {
                im.set(x, 13, HEX(0xb9bec7));
                im.set(x, 14, HEX(0x8f95a0));
            }
            im.set(10, 9, HEX(0x8f95a0));
            im.set(6, 10, HEX(0x8f95a0));
            break;
        case M_TUSK:
            for (int y = 2; y <= 13; y++) {
                float t = (y - 2) / 11.f;
                int x = (int)(3 + sinf(t * 1.6f) * 9 * t);
                im.set(x, y, HEX(0xf3eccb));
                im.set(x + 1, y, HEX(0xf3eccb));
                if (t < 0.8f) im.set(x + 2, y, HEX(0xcfc59a));
                if (t < 0.3f) im.set(x + 3, y, HEX(0xcfc59a));
            }
            im.set(3, 2, HEX(0xffffff));
            break;
        case M_ANTLER:
            im.line(8, 14, 8, 6, HEX(0xdcc79a));
            im.line(9, 14, 9, 6, HEX(0xb89d6a));
            im.line(8, 9, 4, 4, HEX(0xdcc79a));
            im.line(9, 8, 13, 3, HEX(0xdcc79a));
            im.line(8, 6, 7, 1, HEX(0xdcc79a));
            im.line(5, 6, 2, 6, HEX(0xb89d6a));
            im.line(12, 5, 14, 7, HEX(0xb89d6a));
            break;
        case M_FEATHER:
            im.ellipse(8.5f, 7.5f, 2.6f, 5.8f, HEX(0xd9b27a));
            im.ellipse(8.5f, 7.5f, 1.4f, 4.5f, HEX(0xf1d9a8));
            im.line(5, 14, 11, 2, HEX(0x7a5a3a));
            im.line(9, 5, 12, 6, HEX(0xb48a56));
            im.line(8, 8, 11, 9, HEX(0xb48a56));
            break;
        case M_HORN:
            for (int y = 3; y <= 13; y++) {
                float t = (y - 3) / 10.f;
                int x = (int)(4 + t * 3 + sinf(t * 2.4f) * 4);
                im.set(x, y, HEX(0xcdbb94));
                im.set(x + 1, y, HEX(0xa8946a));
                if (t < 0.7f) im.set(x + 2, y, HEX(0x7e6c48));
            }
            im.rect(3, 12, 6, 2, HEX(0x4a3c28));
            im.set(5, 3, HEX(0xf3ead0));
            break;
        case M_SCALE:
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++) {
                    int cx = 4 + c * 4 + (r & 1) * 2, cy = 4 + r * 3;
                    im.ellipse((float)cx, (float)cy, 2.3f, 2.0f, (r + c) & 1 ? HEX(0x6f9a58) : HEX(0x8fba6e));
                    im.set(cx - 1, cy - 1, HEX(0xc7e3a4));
                }
            break;
        case M_CLAW:
            for (int y = 2; y <= 13; y++) {
                float t = (y - 2) / 11.f;
                int x = (int)(10 - t * 6 + t * t * 3);
                int w = t < 0.6f ? 3 : (t < 0.85f ? 2 : 1);
                for (int k = 0; k < w; k++) im.set(x + k, y, k == 0 ? HEX(0xf3eccb) : HEX(0xcfc59a));
            }
            im.rect(8, 1, 4, 2, HEX(0x6b5a44));
            break;
    }
    im.outline(OUT);
    return im;
}
inline Img mkIconWpn(int w) {
    Img im(16, 16);
    switch (w) {
        case W_FLINT:
            im.line(5, 10, 12, 3, HEX(0xc8ccd4));
            im.line(6, 10, 12, 4, HEX(0x9aa0ac));
            im.line(4, 10, 11, 3, HEX(0xeef0f4));
            im.line(3, 12, 5, 10, HEX(0x6b4423));
            im.line(2, 13, 4, 11, HEX(0x8d5e33));
            im.rect(5, 9, 2, 2, HEX(0x3b2616));
            break;
        case W_BONE:
            im.line(5, 10, 12, 3, HEX(0xf2ead3));
            im.line(6, 10, 12, 4, HEX(0xd9cfae));
            im.line(3, 12, 5, 10, HEX(0x6b4423));
            im.line(2, 13, 4, 11, HEX(0x8d5e33));
            im.rect(5, 9, 2, 2, HEX(0x3b2616));
            im.set(13, 2, HEX(0xffffff));
            break;
        case W_AXE:
            im.line(3, 13, 11, 5, HEX(0x6b4423));
            im.line(4, 13, 12, 5, HEX(0x8d5e33));
            im.rect(8, 2, 5, 5, HEX(0x9aa0ac));
            im.rect(8, 2, 2, 5, HEX(0xc8ccd4));
            im.rect(12, 3, 1, 3, HEX(0x646a78));
            im.set(7, 3, HEX(0xeef0f4));
            im.set(7, 4, HEX(0xc8ccd4));
            break;
        case W_JAVELIN:
            im.line(2, 14, 12, 4, HEX(0x8d5e33));
            im.line(3, 14, 13, 4, HEX(0x6b4423));
            im.line(11, 5, 14, 1, HEX(0xc8ccd4));
            im.line(12, 5, 14, 2, HEX(0x9aa0ac));
            im.set(2, 12, HEX(0xd8443f));
            im.set(3, 11, HEX(0xd8443f));
            im.set(4, 13, HEX(0xd8443f));
            break;
        case W_BOW:
            for (int y = 1; y <= 14; y++) {  // limb: a curve bulging to the left, string on the right
                float t = (y - 1) / 13.f;
                int x = (int)(11 - sinf(t * 3.14159f) * 6);
                im.set(x, y, HEX(0x8d5e33));
                im.set(x + 1, y, HEX(0x6b4423));
            }
            im.line(11, 1, 11, 14, HEX(0xe8e2d0));
            im.line(3, 8, 11, 8, HEX(0xc8ccd4));  // nocked arrow
            im.set(2, 8, HEX(0xd8443f));
            im.set(11, 8, HEX(0xeef0f4));
            break;
        case W_CROSSBOW:
            im.rect(2, 7, 11, 2, HEX(0x6b4423));  // stock
            im.rect(2, 6, 11, 1, HEX(0x8d5e33));
            im.rect(2, 9, 4, 2, HEX(0x4a3017));   // grip
            im.line(11, 3, 13, 7, HEX(0x8d5e33));  // prod (the bow part)
            im.line(11, 12, 13, 8, HEX(0x8d5e33));
            im.line(11, 3, 9, 7, HEX(0xe8e2d0));
            im.line(11, 12, 9, 8, HEX(0xe8e2d0));
            im.line(5, 6, 12, 6, HEX(0xc8ccd4));  // bolt on the rail
            break;
    }
    im.outline(OUT);
    return im;
}
inline Img mkIconAmmo(int a) {
    Img im(16, 16);
    if (a == AM_ARROW) {
        im.line(2, 13, 12, 3, HEX(0x8d5e33));
        im.line(3, 13, 13, 3, HEX(0x6b4423));
        im.line(11, 4, 14, 1, HEX(0xc8ccd4));
        im.line(11, 5, 14, 2, HEX(0x9aa0ac));
        im.line(2, 11, 4, 13, HEX(0xf2ead3));
        im.line(3, 10, 5, 12, HEX(0xd8443f));
    } else {
        im.line(3, 12, 11, 4, HEX(0x6b4423));
        im.line(4, 12, 12, 4, HEX(0x8d5e33));
        im.rect(11, 2, 3, 3, HEX(0xc8ccd4));
        im.rect(12, 1, 2, 2, HEX(0xeef0f4));
        im.line(3, 11, 3, 14, HEX(0xd8443f));
    }
    im.outline(OUT);
    return im;
}
// gear icons - 3 tiers each
inline Img mkIconGear(int up, int tier) {
    Img im(16, 16);
    switch (up) {
        case U_TOOL: {
            u32 bl[3] = {HEX(0xaab0bb), HEX(0xf2ead3), HEX(0xd5e3f2)};
            u32 bd[3] = {HEX(0x7e8490), HEX(0xcfc59a), HEX(0x8fa8c4)};
            im.line(4, 10, 12, 3, bl[tier]);
            im.line(5, 11, 12, 4, bd[tier]);
            im.line(3, 11, 11, 3, bl[tier]);
            im.line(2, 14, 5, 11, HEX(0x6b4423));
            im.line(3, 14, 6, 11, HEX(0x8d5e33));
            im.rect(5, 10, 2, 2, HEX(0x3b2616));
            if (tier == 2) im.set(13, 2, HEX(0xffffff));
            break;
        }
        case U_BAG: {
            u32 c[5] = {HEX(0x8a5c34), HEX(0xa87a4a), HEX(0x6e7a4a), HEX(0x3f7f86), HEX(0x7a4a8a)};
            u32 d[5] = {HEX(0x5e3d22), HEX(0x7a5230), HEX(0x4a5632), HEX(0x2a4f58), HEX(0x4e2c5e)};
            im.ellipse(8, 9, 5.5f, 5.5f, c[tier]);
            im.rect(3, 9, 10, 4, c[tier]);
            im.rect(3, 12, 10, 2, d[tier]);
            im.rect(4, 3, 8, 3, d[tier]);
            im.rect(6, 7, 4, 3, HEX(0xf2c94c));
            im.rect(7, 8, 2, 1, d[tier]);
            im.rect(2, 8, 1, 4, HEX(0xd6b886));
            im.rect(13, 8, 1, 4, HEX(0xd6b886));
            break;
        }
        case U_BOOTS: {
            u32 c[4] = {HEX(0x6b4a2b), HEX(0xa87a4a), HEX(0xd8d2c8), HEX(0xd8a23c)};
            im.rect(3, 3, 4, 8, c[tier]);
            im.rect(2, 10, 7, 3, c[tier]);
            im.rect(9, 5, 4, 8, shadeC(c[tier], 0.85f));
            im.rect(8, 11, 7, 2, shadeC(c[tier], 0.85f));
            im.rect(2, 13, 7, 1, HEX(0x2e2118));
            im.rect(8, 13, 7, 1, HEX(0x2e2118));
            im.rect(3, 3, 4, 1, shadeC(c[tier], 1.3f));
            im.rect(9, 5, 4, 1, shadeC(c[tier], 1.3f));
            break;
        }
        case U_ARMOR: {
            u32 c[9] = {HEX(0x9b6b3a), HEX(0xb08a5a), HEX(0xcfd3d8), HEX(0x6a4a30), HEX(0x5a3a22), HEX(0x4a2e1c), HEX(0x8a7a6a), HEX(0x8ec8e8), HEX(0x5a78c8)};
            im.rect(3, 3, 10, 11, c[tier]);
            im.rect(1, 3, 3, 4, c[tier]);
            im.rect(12, 3, 3, 4, c[tier]);
            im.rect(6, 2, 4, 3, HEX(0x241a14));
            im.rect(3, 12, 10, 2, shadeC(c[tier], 0.75f));
            im.rect(7, 5, 2, 9, shadeC(c[tier], 0.8f));
            im.set(5, 8, shadeC(c[tier], 1.25f));
            im.set(10, 9, shadeC(c[tier], 1.25f));
            break;
        }
    }
    im.outline(OUT);
    return im;
}
inline Img mkIconBandage() {
    Img im(16, 16);
    im.rect(2, 5, 12, 7, HEX(0xf0ead8));
    im.rect(2, 5, 12, 1, HEX(0xffffff));
    im.rect(2, 11, 12, 1, HEX(0xc9c0a6));
    im.rect(7, 4, 2, 9, HEX(0xd8443f));
    im.rect(5, 7, 6, 2, HEX(0xd8443f));
    im.outline(OUT);
    return im;
}
inline Img mkCoin() {
    Img im(10, 10);
    im.ellipse(5, 5, 4, 4, HEX(0xc99320));
    im.ellipse(5, 5, 3.2f, 3.2f, HEX(0xf2c94c));
    im.rect(4, 3, 2, 4, HEX(0xc99320));
    im.set(3, 3, HEX(0xfff1a8));
    im.outline(OUT);
    return im;
}
inline Img mkHeart() {
    Img im(11, 10);
    im.ellipse(3.5f, 3.5f, 2.8f, 2.6f, HEX(0xe0453f));
    im.ellipse(7.5f, 3.5f, 2.8f, 2.6f, HEX(0xe0453f));
    for (int y = 4; y < 9; y++) im.rect(1 + (y - 4), y, 9 - 2 * (y - 4), 1, HEX(0xe0453f));
    im.set(2, 2, HEX(0xff9a94));
    im.set(3, 2, HEX(0xff9a94));
    im.outline(OUT);
    return im;
}
inline Img mkBolt() {
    Img im(9, 12);
    im.line(5, 1, 2, 6, HEX(0xf2c94c));
    im.line(6, 1, 3, 6, HEX(0xf2c94c));
    im.line(2, 6, 6, 6, HEX(0xf2c94c));
    im.line(6, 6, 3, 10, HEX(0xf2c94c));
    im.line(7, 6, 4, 10, HEX(0xc99320));
    im.outline(OUT);
    return im;
}
inline Img mkCursor() {
    Img im(10, 14);
    for (int y = 0; y < 11; y++) im.rect(1, 1 + y, std::min(1 + y / 1, 7) , 1, HEX(0xffffff));
    im.rect(1, 1, 1, 11, HEX(0xffffff));
    for (int y = 0; y < 8; y++) im.rect(1, 1 + y, 1 + y, 1, HEX(0xffffff));
    im.rect(3, 9, 2, 3, HEX(0xffffff));
    im.outline(OUT);
    return im;
}
inline Img mkGlow(u32 col, int sz) {
    Img im(sz, sz);
    float c = sz / 2.f;
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++) {
            float d = sqrtf((x + 0.5f - c) * (x + 0.5f - c) + (y + 0.5f - c) * (y + 0.5f - c)) / c;
            float a = std::max(0.f, 1.f - d);
            a = a * a;
            im.set(x, y, withA(col, (int)(a * 255)));
        }
    return im;
}
// The grass around the snow spruce's roots: the forest's spruce grass, recoloured. Every pixel keeps its brightness but is mapped
// onto a frosty ramp, from a dark blue-green in the shade to nearly white in the light.
inline Img mkSnowGrassFrame(int frame) {
    Img im = mkGrassFrame(GRASS_SPRUCE, frame);
    const int dr = 0x2a, dg = 0x62, db = 0x6c, lr = 0xb8, lg = 0xd8, lb = 0xe0;  // dark and light ends of the ramp
    for (int y = 0; y < im.h; y++)
        for (int x = 0; x < im.w; x++) {
            const u32 c = im.get(x, y);
            if (cA(c) == 0) continue;
            float l = (cR(c) * 0.30f + cG(c) * 0.59f + cB(c) * 0.11f) / 170.f;  // the forest grass tops out around 170
            l = l < 0.f ? 0.f : l > 1.f ? 1.f : l;
            im.set(x, y, RGBA((int)(dr + (lr - dr) * l), (int)(dg + (lg - dg) * l), (int)(db + (lb - db) * l), cA(c)));
        }
    return im;
}

// A long soft shadow for the time-of-day tree shadows: 64 x 16, wide at the left end (the foot) and tapering to a soft point.
inline Img mkSoftShadow() {
    Img im(64, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 64; x++) {
            const float u = x / 63.f, hw = 7.5f * (1.f - u * 0.75f);
            float a = 1.f - fabsf(y + 0.5f - 8.f) / hw;
            if (a <= 0.f) continue;
            a = std::min(1.f, a * 1.6f) * (1.f - u * u * 0.8f) * std::min(1.f, 0.35f + u * 8.f);
            im.set(x, y, RGBA(8, 6, 22, (int)(a * 255)));
        }
    return im;
}


inline Img mkBlood(int w, int h) {
    Img im(w, h);
    im.ellipse(w / 2.f, h / 2.f, w / 2.f, h / 2.f, RGBA(120, 24, 28, 150));
    im.ellipse(w / 2.f - 1, h / 2.f, w / 3.f, h / 3.f, RGBA(150, 30, 34, 170));
    return im;
}
inline Img mkVignette(int w, int h) {
    Img im(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = (x + 0.5f - w / 2.f) / (w / 2.f), dy = (y + 0.5f - h / 2.f) / (h / 2.f);
            float d = sqrtf(dx * dx * 0.8f + dy * dy * 1.0f);
            float a = std::min(1.f, std::max(0.f, (d - 0.6f) / 0.7f));
            im.set(x, y, RGBA(8, 6, 16, (int)(a * a * 120)));
        }
    return im;
}

// ----- weapons as they look in the world (pointing right)
inline Img mkWpnWorld(int w) {
    switch (w) {
        case W_FLINT:
        case W_BONE: {
            Img im(14, 7);
            u32 bl = w == W_FLINT ? HEX(0xc8ccd4) : HEX(0xf2ead3), bd = w == W_FLINT ? HEX(0x8d93a0) : HEX(0xcfc59a);
            im.rect(1, 3, 4, 1, HEX(0x8d5e33));
            im.rect(1, 2, 4, 1, HEX(0x6b4423));
            im.rect(5, 1, 1, 4, HEX(0x3b2616));
            for (int x = 6; x <= 11; x++) {
                int hh = x >= 10 ? 2 : 3;
                im.rect(x, 3 - hh / 2 - (hh == 3 ? 0 : 0), 1, 1, bl);
                im.rect(x, 3, 1, hh - 1, bd);
            }
            im.set(12, 3, bl);
            im.outline(OUT);
            return im;
        }
        case W_AXE: {
            Img im(16, 12);
            im.rect(1, 6, 11, 1, HEX(0x8d5e33));
            im.rect(1, 5, 11, 1, HEX(0x6b4423));
            im.rect(9, 2, 4, 8, HEX(0x9aa0ac));
            im.rect(12, 3, 1, 6, HEX(0xeef0f4));
            im.rect(9, 8, 4, 2, HEX(0x646a78));
            im.outline(OUT);
            return im;
        }
        case W_BOW: {  // held bow, pointing right: limb curves back, string is the vertical line
            Img im(9, 17);
            for (int y = 0; y < 17; y++) {
                float t = y / 16.f;
                int x = (int)(5 - sinf(t * 3.14159f) * 4);
                im.set(x, y, HEX(0x8d5e33));
                im.set(x + 1, y, HEX(0x6b4423));
            }
            im.line(5, 0, 5, 16, HEX(0xe8e2d0));
            im.outline(OUT);
            return im;
        }
        case W_CROSSBOW: {
            Img im(15, 11);
            im.rect(1, 5, 11, 2, HEX(0x6b4423));
            im.rect(1, 4, 11, 1, HEX(0x8d5e33));
            im.rect(1, 7, 3, 2, HEX(0x4a3017));
            im.line(10, 0, 12, 5, HEX(0x8d5e33));
            im.line(10, 10, 12, 5, HEX(0x8d5e33));
            im.line(10, 0, 8, 5, HEX(0xe8e2d0));
            im.line(10, 10, 8, 5, HEX(0xe8e2d0));
            im.line(3, 3, 13, 3, HEX(0xc8ccd4));
            im.outline(OUT);
            return im;
        }
        default: {
            Img im(26, 7);
            im.rect(1, 3, 20, 1, HEX(0x8d5e33));
            im.rect(1, 2, 20, 1, HEX(0x6b4423));
            im.rect(1, 1, 3, 4, HEX(0xd8443f));
            im.rect(2, 0, 2, 1, HEX(0xd8443f));
            for (int x = 20; x <= 24; x++) im.rect(x, 3 - (x < 23 ? 2 : (x == 23 ? 1 : 0)), 1, (x < 23 ? 4 : (x == 23 ? 2 : 1)), x < 23 ? HEX(0xc8ccd4) : HEX(0xeef0f4));
            im.outline(OUT);
            return im;
        }
    }
}

// arrows and bolts in flight / on the ground (pointing right)
inline Img mkAmmoWorld(int a) {
    if (a == AM_ARROW) {
        Img im(20, 5);
        im.rect(1, 2, 15, 1, HEX(0x8d5e33));
        im.rect(1, 1, 3, 1, HEX(0xf2ead3));
        im.rect(1, 3, 3, 1, HEX(0xd8443f));
        im.rect(15, 1, 3, 3, HEX(0xc8ccd4));
        im.rect(18, 2, 1, 1, HEX(0xeef0f4));
        im.outline(OUT);
        return im;
    }
    Img im(14, 5);
    im.rect(1, 2, 9, 1, HEX(0x6b4423));
    im.rect(1, 1, 2, 3, HEX(0xd8443f));
    im.rect(9, 1, 3, 3, HEX(0xc8ccd4));
    im.rect(12, 2, 1, 1, HEX(0xeef0f4));
    im.outline(OUT);
    return im;
}

// ================================================================= the sprite bank
struct Assets {
    Spr grass[4], dirt[2];
    Spr lip[LIP_COUNT][4], dirtTone[3][4], dirtRock[3][2];  // the face of raised ground (grass, sand, snow, stone, paving); dirt in three tones, and rocky dirt
    Spr grassT[GRASS_SHADES][GRASS_VARIANTS], soilPatch[SOIL_PATCHES];  // pack-style grass tiles in three shades, and soil patches (built for the tests; the world is drawn with the generated grass)
    WaterArt water;  // the lakes' and brooks' water and the bank round them, baked per tile (sprites_forest.hpp)
    Spr waterW[WATER_TEX_W], bankW[BANK_TEX_W / 16];  // the author's tile art (tiles_user.hpp): grass / dark grass crops of the 10 x 10 texture, scrolled water crops, bank tiles for water with land to its north
    Spr leaf[3], needles[3], moss[3], bank[3], bankWet[3];  // the home forest's floors (sprites_forest.hpp); bankWet is the bank right at the waterline
    Spr flower[3], flowerX[2], tuft, mushroom, pebbles, bush[2], homeBush;  // flowerX: violet and poppy (the flower fields)
    Spr oak[OAK_VARIANTS][OAK_FRAMES], pine[PINE_FRAMES], treeGrass[GRASS_SETS][GRASS_FRAMES], rock[2];
    Spr snowGrass[GRASS_FRAMES], snowSpruce[SNOW_SPRUCE_FRAMES], deadSpruce[DEAD_SPRUCE_FRAMES];  // animated snow and dead spruces
    Spr rockGrass[3][ROCK_LOOKS][ROCK_GRASS_FRAMES];  // swaying grass round minable rocks (tiny, small, large), see mkRockGrass
    // [theme] = the biome's colours (ROCK_PALS); the picture is the same everywhere. pebbleSet lies around small rocks and boulders, like the grass around trees
    Spr rockTiny[ROCK_THEMES][ROCK_LOOKS], rockSmall[ROCK_THEMES][ROCK_LOOKS], rockLarge[ROCK_THEMES][ROCK_LOOKS], pebbleSet[ROCK_THEMES][ROCK_LOOKS];
    Spr desertDecor[DROCK_DECOR_N];  // the desert's scenery-only rocks (grey, not minable)
    Spr rubbleT[ROCK_THEMES];  // what a broken scenery rock leaves, in the theme's colours
      // rock_sprites.hpp
    Spr hut, sign, crate, barrel, fire[3], keeper[2], quester[2];
    Spr hunter[FACING_VIEWS][HS_COUNT][HS_MAXF];  // [view S E N][state][pose]; west is the east view mirrored
    HunterLook look;  // the appearance player[][] was drawn with (see rebuildPlayerSprites)
    std::shared_ptr<RichAnimal> rich[A_COUNT];  // full animation sets (animal_anim.hpp): only species that have one are set
    Spr animal[A_COUNT][8][5];  // [kind][form + 2*tier][walk0..3, dead]: the side view (west = mirror)
    Spr animalNS[A_COUNT][8][2][5];  // the crawlers' front (0) and back (1) views, same indexing
    Spr matIcon[M_COUNT], wpnIcon[W_COUNT], gearIcon[U_COUNT][9], carryIcon[CG_COUNT], babyIcon[A_COUNT], bandage;
    Spr wpnWorld[W_COUNT], ammoIcon[AM_COUNT], ammoWorld[AM_COUNT];
    Spr coin, heart, bolt, cursor, glowWarm, glowCool, shadowSoft, blood[3], vignette;
    DelveAssets D;
    hart::HomeAssets H;  // the Homestead: building pieces, crops and portal (sprites_homestead.hpp)
    DomBank Dom;    // the farm animals of the Homestead: built on first entry (buildDomBank)
    MonsterBank M;  // the delve creatures: built on first entry (buildMonsterBank)  // the Abandoned Mine and the Frozen Dungeon (sprites_delve.hpp)
    TileBlend blend;  // soft borders between ground tiles (tile_edges.hpp)
    BiomeAssets B;  // desert / swamp / snow / mountain art (sprites_biome.hpp)
    TownAssets T;   // the central town: building exteriors, street furniture, paving, interiors (sprites_town.hpp)
    ResAssets R;    // tools, consumables, placeables, depleted resources (sprites_res.hpp)
    SDL_Texture* font = nullptr;
};

// Draw every hunter pose in every view for one appearance (the salon redraws them all). Frees any old textures first.
inline void buildHunterSprites(SDL_Renderer* r, Assets& a, const HunterLook& look) {
    static const int DIR_OF_VIEW[FACING_VIEWS] = {0, 2, 1};  // S, E, N -> mkPlayerPose dir (0 front, 1 back, 2 side)
    for (int v = 0; v < FACING_VIEWS; v++)
        for (int st = 0; st < HS_COUNT; st++)
            for (int i = 0; i < hunterPoseCount(st); i++) {
                Spr& s = a.hunter[v][st][i];
                if (s.t) SDL_DestroyTexture(s.t);
                if (s.flash) SDL_DestroyTexture(s.flash);
                Img im = mkPlayerPose(DIR_OF_VIEW[v], hunterPose(st, i), look);
                styleActor(im, OUT, 0);
                s = makeSpr(r, im, 9, 24, true);
            }
    a.look = look;
}
inline void rebuildPlayerSprites(SDL_Renderer* r, Assets& a, const HunterLook& look) { buildHunterSprites(r, a, look); }

inline void buildAssets(SDL_Renderer* r, Assets& a) {
    Rng rg(1234567);
    for (int i = 0; i < 4; i++) a.grass[i] = makeSpr(r, mkGrass(i, rg), 0, 0);  // (the older generated tiles; the world is drawn with grassT)
    // edge blending (tile_edges.hpp) uses the same pack-style tiles the world is drawn with: two light and two medium variants, one dark
    for (int i = 0; i < 4; i++) a.blend.src[BL_GRASS][i] = mkGrassTile(i / 2, i % 2);
    a.blend.src[BL_DARK][0] = mkGrassTile(2, 0);
    a.blend.nvar[BL_GRASS] = 4;
    a.blend.nvar[BL_DARK] = 1;
    for (int i = 0; i < 2; i++) {
        const Img im = mkDirt(i, rg);
        a.dirt[i] = makeSpr(r, im, 0, 0);
        a.blend.src[BL_DIRT][i] = im;
    }
    a.blend.nvar[BL_DIRT] = 2;
    buildForestFloors(r, a.leaf, a.needles, a.moss, a.bank, a.blend);
    a.water.init();
    for (int i = 0; i < 3; i++) a.bankWet[i] = makeSpr(r, forestfloor::mkBankWet(i), 0, 0);
    for (int st = 0; st < LIP_COUNT; st++)
        for (int i = 0; i < 4; i++) a.lip[st][i] = makeSpr(r, mkLip(st, i), 0, 0);
    for (int t = 0; t < 3; t++) {
        for (int v = 0; v < 4; v++) a.dirtTone[t][v] = makeSpr(r, mkDirtTone(t, v, false), 0, 0);
        for (int v = 0; v < 2; v++) a.dirtRock[t][v] = makeSpr(r, mkDirtTone(t, v, true), 0, 0);
    }
    for (int sh = 0; sh < GRASS_SHADES; sh++)
        for (int v = 0; v < GRASS_VARIANTS; v++) a.grassT[sh][v] = makeSpr(r, mkGrassTile(sh, v), 0, 0);
    for (int v = 0; v < SOIL_PATCHES; v++) a.soilPatch[v] = makeSpr(r, mkSoilPatch(v), 0, 0);
    for (int i = 0; i < WATER_TEX_W; i++) a.waterW[i] = makeSpr(r, tileCrop(WATER_ROWS, WATER_TEX_W, WATER_TEX_H, i, 0, 16, 16), 0, 0);
    for (int i = 0; i < BANK_TEX_W / 16; i++) a.bankW[i] = makeSpr(r, tileCrop(BANK_ROWS, BANK_TEX_W, BANK_TEX_H, i * 16, 0, 16, 16), 0, 0);
    a.flower[0] = makeSpr(r, mkFlower(0), 4, 7);
    a.flower[1] = makeSpr(r, mkFlower(1), 4, 7);
    a.flower[2] = makeSpr(r, mkFlower(0), 4, 7);  // decor kind 2 is no longer generated; kept so old data still draws
    a.flowerX[0] = makeSpr(r, mkFlower(2), 4, 7);  // the flower fields' violet and poppy
    a.flowerX[1] = makeSpr(r, mkFlower(3), 4, 7);
    a.tuft = makeSpr(r, mkTuft(), 4, 7);
    a.mushroom = makeSpr(r, mkMushroom(), 4, 7);
    a.pebbles = makeSpr(r, mkPebbles(), 4, 5);
    for (int i = 0; i < 2; i++) a.bush[i] = makeSpr(r, mkBush(i, rg), 12, 16);
    { Rng hr(20241120); a.homeBush = makeSpr(r, mkFlowerBush(hr), 12, 16); }  // the homestead's flowering bush
    for (int v = 0; v < OAK_VARIANTS; v++)
        for (int f = 0; f < OAK_FRAMES; f++) a.oak[v][f] = makeSpr(r, mkOakFrame(v, f), OAK_VAR[v].ax, OAK_VAR[v].ay, false, true);  // with bottoms: the roots cast their own shadow base
    for (int f = 0; f < PINE_FRAMES; f++) a.pine[f] = makeSpr(r, mkPineFrame(f), PINE_AX, PINE_AY);
    for (int g = 0; g < GRASS_SETS; g++)
        for (int f = 0; f < GRASS_FRAMES; f++) a.treeGrass[g][f] = makeSpr(r, mkGrassFrame(g, f), GRASS_SET[g].ax, GRASS_SET[g].ay);  // round tufts round the roots
    for (int f = 0; f < GRASS_FRAMES; f++) a.snowGrass[f] = makeSpr(r, mkSnowGrassFrame(f), GRASS_SET[GRASS_SPRUCE].ax, GRASS_SET[GRASS_SPRUCE].ay);
    for (int f = 0; f < SNOW_SPRUCE_FRAMES; f++) a.snowSpruce[f] = makeSpr(r, mkSnowSpruceFrame(f), SNOW_SPRUCE_AX, SNOW_SPRUCE_AY);
    for (int f = 0; f < DEAD_SPRUCE_FRAMES; f++) a.deadSpruce[f] = makeSpr(r, mkDeadSpruceFrame(f), DEAD_SPRUCE_AX, DEAD_SPRUCE_AY);
    for (int i = 0; i < ROCK_LOOKS; i++) {
        for (int th = 0; th < ROCK_THEMES; th++) {
            const bool des = th == ROCK_THEME_DESERT;  // the desert has its own pictures and palette; the other biomes recolour the forest ones
            const RockArt &rt = des ? DROCK_TINY[i] : ROCK_TINY[i], &rs = des ? DROCK_SMALL[i] : ROCK_SMALL[i], &rl = des ? DROCK_LARGE[i] : ROCK_LARGE[i], &rp = des ? DROCK_PEBBLES[i] : ROCK_PEBBLES[i];
            a.rockTiny[th][i] = makeSpr(r, des ? mkPalArt(rt, DROCK_PAL) : mkRockArt(rt, th), rt.w / 2, rt.h - 2, false, true);
            a.rockSmall[th][i] = makeSpr(r, des ? mkPalArt(rs, DROCK_PAL) : mkRockArt(rs, th), rs.w / 2, rs.h - 2, false, true);
            a.rockLarge[th][i] = makeSpr(r, des ? mkPalArt(rl, DROCK_PAL) : mkRockArt(rl, th), rl.w / 2, rl.h - 2, false, true);
            a.pebbleSet[th][i] = makeSpr(r, des ? mkPalArt(rp, DROCK_PAL) : mkRockArt(rp, th), rp.w / 2, rp.h - 1);
        }
        const int gw[3] = {ROCK_TINY[i].w, ROCK_SMALL[i].w, ROCK_LARGE[i].w};
        for (int k = 0; k < 3; k++)
            for (int f = 0; f < ROCK_GRASS_FRAMES; f++) {
                const Img gi = mkRockGrass(gw[k], k, i, f);
                a.rockGrass[k][i][f] = makeSpr(r, gi, gi.w / 2, gi.h - 1);  // anchor: bottom centre, drawn 2px below the rock's foot
            }
    }
    for (int i = 0; i < DROCK_DECOR_N; i++) a.desertDecor[i] = makeSpr(r, mkPalArt(DROCK_DECOR[i], DROCK_DECOR_PAL), DROCK_DECOR[i].w / 2, DROCK_DECOR[i].h - 2, false, true);
    a.rock[0] = makeSpr(r, mkRock(0), 10, 13);
    a.rock[1] = makeSpr(r, mkRock(1), 12, 16);
    a.hut = makeSpr(r, mkHut(), 33, 56);
    a.sign = makeSpr(r, mkSign(), 16, 24);
    a.crate = makeSpr(r, mkCrate(), 8, 13);
    a.barrel = makeSpr(r, mkBarrel(), 7, 16);
    for (int i = 0; i < 3; i++) a.fire[i] = makeSpr(r, mkFire(i), 9, 17);
    for (int i = 0; i < 2; i++) a.keeper[i] = makeSpr(r, mkShopkeeper(i), 9, 24);
    for (int i = 0; i < 2; i++) a.quester[i] = makeSpr(r, mkQuestGiver(i), 9, 24);
    buildHunterSprites(r, a, HunterLook());
    buildFauna(r, a);
    buildRichAnimals(r, a);
    for (int m = 0; m < M_COUNT; m++) a.matIcon[m] = makeSpr(r, carcassArt(m) ? artToImg(*carcassArt(m)) : oreArt(m) ? artToImg(*oreArt(m)) : m >= M_VOIDSHARD ? dart::iconDeepMat(m) : m >= M_BERRY ? hart::iconMat(m) : m >= M_COAL ? dart::iconDelveMat(m) : m >= M_ROSE ? mkIconFlower(m) : m < M_CARCASS_N ? mkIconMat(m) : (isCarcassMat(m) ? mkIconTrophy(m) : (m >= M_PLANK ? mkIconProcessed(m) : mkIconLand(m))), 8, 8);
    for (int w = 0; w < W_COUNT; w++) a.wpnIcon[w] = makeSpr(r, w < W_SPEAR ? mkIconWpn(w) : (w < W_FANGDAGGER ? mkIconWpnNew(w) : mkIconWpnGear(w)), 8, 8);
    for (int k = 0; k < TL_COUNT; k++) {
        Img t = k >= TL_HOE ? hart::iconTool(k) : mkIconTool(k);
        a.R.toolIcon[k] = makeSpr(r, t, 8, 8);
        a.R.toolSwing[k] = makeSpr(r, t, 2, 14);
    }
    for (int c = 0; c < C_COUNT; c++) a.R.consIcon[c] = c == C_BANDAGE ? Spr() : makeSpr(r, mkIconCons(c), 8, 8);
    for (int f = 0; f < 3; f++) a.R.campfire[f] = makeSpr(r, mkCampfire(f), 9, 15);
    a.R.chest = makeSpr(r, mkChest(), 8, 12);
    a.R.placeIcon[PL_CAMPFIRE] = makeSpr(r, mkCampfire(0), 9, 9);
    a.R.placeIcon[PL_CHEST] = makeSpr(r, mkChest(), 8, 7);
    a.R.stump = makeSpr(r, mkStump(), 7, 8);
    a.R.rubble = makeSpr(r, mkRubble(), 7, 6);
    for (int th = 0; th < ROCK_THEMES; th++) a.rubbleT[th] = makeSpr(r, retintRock(mkRubble(), th), 7, 6);
    a.R.stub = makeSpr(r, mkStub(), 6, 6);
    a.R.sprout = makeSpr(r, mkSprout(), 5, 9);
    for (int u = 0; u < U_COUNT; u++)
        for (int t = 0; t < 9; t++) a.gearIcon[u][t] = makeSpr(r, mkIconGear(u, std::min(t, UP_MAX[u])), 8, 8);
    for (int k = 0; k < A_COUNT; k++) {  // a baby animal's face in the species' own colours
        const QuadSpec q = fauSpec(k, 0, 0);
        Img im(16, 16);
        im.ellipse(4.f, 4.5f, 2.4f, 2.8f, q.body);
        im.ellipse(12.f, 4.5f, 2.4f, 2.8f, q.body);
        im.ellipse(4.f, 5.f, 1.1f, 1.5f, q.dark);
        im.ellipse(12.f, 5.f, 1.1f, 1.5f, q.dark);
        im.ellipse(8.f, 9.f, 6.2f, 5.6f, q.body);
        im.ellipse(8.f, 7.4f, 4.2f, 2.4f, q.light);
        im.ellipse(8.f, 11.6f, 3.2f, 2.3f, q.belly);
        im.set(5, 8, q.eye); im.set(10, 8, q.eye);
        im.set(7, 11, q.dark); im.set(8, 11, q.dark);
        im.outline(OUT);
        a.babyIcon[k] = makeSpr(r, im, 8, 8);
    }
    for (int c = 0; c < CG_STRAWHAT; c++) a.carryIcon[c] = makeSpr(r, (c >= 7 && c != CG_SADDLEBAG) ? dart::iconDelveCarry(c) : mkIconCarry(c), 8, 8);
    a.bandage = makeSpr(r, mkIconBandage(), 8, 8);
    for (int w = 0; w < W_COUNT; w++) {
        Img im = w < W_SPEAR ? mkWpnWorld(w) : (w < W_FANGDAGGER ? mkWpnWorldNew(w) : mkWpnWorldGear(w));
        a.wpnWorld[w] = makeSpr(r, im, im.w / 2, im.h / 2);
    }
    for (int k = 0; k < AM_COUNT; k++) {
        a.ammoIcon[k] = makeSpr(r, mkIconAmmo(k), 8, 8);
        Img im = mkAmmoWorld(k);
        a.ammoWorld[k] = makeSpr(r, im, im.w / 2, im.h / 2);
    }
    buildBiomeAssets(r, a.B, &a.blend);
    buildDelveAssets(r, a.D);
    hart::buildHomeAssets(r, a.H);
    buildTownAssets(r, a.T);
    a.coin = makeSpr(r, mkCoin(), 5, 5);
    a.heart = makeSpr(r, mkHeart(), 5, 5);
    a.bolt = makeSpr(r, mkBolt(), 4, 6);
    a.cursor = makeSpr(r, mkCursor(), 1, 1);
    a.glowWarm = makeSpr(r, mkGlow(HEX(0xffb24a), 96), 48, 48);
    a.glowCool = makeSpr(r, mkGlow(HEX(0xfff2b0), 24), 12, 12);
    SDL_SetTextureBlendMode(a.glowWarm.t, SDL_BLENDMODE_ADD);
    SDL_SetTextureBlendMode(a.glowCool.t, SDL_BLENDMODE_ADD);
    a.shadowSoft = makeSpr(r, mkSoftShadow(), 0, 8);
    a.blood[0] = makeSpr(r, mkBlood(12, 7), 6, 3);
    a.blood[1] = makeSpr(r, mkBlood(20, 10), 10, 5);
    a.blood[2] = makeSpr(r, mkBlood(30, 14), 15, 7);
    a.vignette = makeSpr(r, mkVignette(400, 225), 0, 0);
    // font atlas: one row of 4px-wide cells (3 wide + 1 gap)
    Img f(FONT_N * 4, 5);
    for (int i = 0; i < FONT_N; i++)
        for (int row = 0; row < 5; row++)
            for (int c = 0; c < 3; c++)
                if ((FONT[i].r[row] >> (2 - c)) & 1) f.set(i * 4 + c, row, HEX(0xFFFFFF));
    a.font = toTex(r, f);
}
