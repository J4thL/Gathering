// tiles_water.hpp - the three looks of water, all from the author's light blue (tiles_user.hpp keeps the normal water):
//   shallow: a see-through light aqua film with fine bright ripples and a few glints; the bed (trodden earth) shows through it
//   normal:  the author's own water art, unchanged (WATER_ROWS)
//   deep:    a deeper, calmer blue: broad slow wave bands and few, soft glints (no bright sparkle)
// Each is a 64 x 16 band that repeats horizontally, scrolled by cropping 16 x 16 tiles out of it (like WATER_ROWS).
#pragma once
#include <cmath>
#include "core/gfx.hpp"
#include "art/tiles_user.hpp"

constexpr int WATER_BAND_W = 64, WATER_BAND_H = 16;
constexpr int WATER_ROW_SHIFT = 19;  // each tile row scrolls the water texture a little further sideways, so its glints do not stack in columns

// shallow: mostly transparent. Light aqua streaks (short, horizontal, wrapping at the edges), a few brighter glints, and soft blue ripple shadows.
inline Img mkWaterShallowBand() {
    Img im(WATER_BAND_W, WATER_BAND_H);
    Rng g(4242);
    for (int i = 0; i < 20; i++) {  // soft blue ripple shadows under the streaks
        const int x = g.irange(0, WATER_BAND_W - 1), y = g.irange(1, WATER_BAND_H - 1), len = g.irange(3, 6);
        for (int k = 0; k < len; k++) im.set((x + k) % WATER_BAND_W, y, RGBA(60, 140, 200, 70));
    }
    for (int i = 0; i < 18; i++) {  // pale streaks
        const int x = g.irange(0, WATER_BAND_W - 1), y = g.irange(0, WATER_BAND_H - 2), len = g.irange(2, 5);
        for (int k = 0; k < len; k++) im.set((x + k) % WATER_BAND_W, y, RGBA(190, 238, 248, 150));
    }
    for (int i = 0; i < 7; i++) {  // glints
        const int x = g.irange(0, WATER_BAND_W - 2), y = g.irange(0, WATER_BAND_H - 1);
        im.set(x, y, RGBA(236, 250, 250, 220));
        im.set(x + 1, y, RGBA(236, 250, 250, 170));
    }
    return im;
}

// deep: a darker, calmer blue than the normal water, in the same language: broad, flat, slowly drifting wave streaks (the normal water has short
// horizontal streaks; these are the same idea, wider and softer), and a few grey-blue dashes where the light catches. Wraps exactly.
inline Img mkWaterDeepBand() {
    Img im(WATER_BAND_W, WATER_BAND_H);
    constexpr float TAU = 6.2831853f;
    for (int y = 0; y < WATER_BAND_H; y++)
        for (int x = 0; x < WATER_BAND_W; x++) {
            const float yy = y * TAU / WATER_BAND_H, xx = x * TAU / WATER_BAND_W;  // wavy horizontal bands (they wrap both ways)
            const float n = sinf(yy + 1.1f * sinf(xx * 2.f) + 0.6f * sinf(xx * 5.f + 0.7f)) + 0.7f * sinf(yy * 2.f + xx * 3.f + 0.4f);
            u32 c = HEX(0x2a68a0);                       // base
            if (n > 1.0f) c = HEX(0x3374ac);             // a lighter wave crest
            else if (n < -1.0f) c = HEX(0x225c94);       // a darker trough
            im.set(x, y, c);
        }
    Rng g(9191);
    for (int i = 0; i < 6; i++) {  // a few soft light dashes, like the normal water's streaks (not single pixels)
        const int x = g.irange(0, WATER_BAND_W - 5), y = g.irange(0, WATER_BAND_H - 1), len = g.irange(2, 4);
        for (int k = 0; k < len; k++) im.set(x + k, y, k == 0 ? HEX(0x6d93ad) : HEX(0x7c9baf));
    }
    return im;
}

// the dithered fade that goes on a normal / deep water tile along a side that touches shallows: 3 rows (or columns) deep, densest at the edge.
// side 0 = shallows to the north, 1 = south, 2 = west, 3 = east.
inline Img mkWaterEdge(int side, u32 c = RGBA(120, 184, 220, 190)) {
    Img im(16, 16);
    for (int d = 0; d < 3; d++)
        for (int k = 0; k < 16; k++) {
            const bool on = d == 0 ? ((k + 0) % 4 != 0) : d == 1 ? (k % 2 == 0) : (k % 4 == 1);  // 75% / 50% / 25%
            if (!on) continue;
            switch (side) {
                case 0: im.set(k, d, c); break;
                case 1: im.set(k, 15 - d, c); break;
                case 2: im.set(d, k, c); break;
                default: im.set(15 - d, k, c); break;
            }
        }
    return im;
}

// one 16 x 16 tile of a band, starting at column x0 (wraps)
inline Img waterBandCrop(const Img& band, int x0) {
    Img im(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) im.set(x, y, band.get((x0 + x) % band.w, y % band.h));
    return im;
}
