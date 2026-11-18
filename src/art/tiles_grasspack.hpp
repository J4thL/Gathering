// tiles_grasspack.hpp - the grass in the style of the "Grass Pack" reference: flat bright tiles in three shades (light, medium, dark), tall upright
// blades that sway, and patches of dark soil showing through here and there. Everything is generated from the reference's own colours.
#pragma once
#include "core/gfx.hpp"
#include "world/world.hpp"

constexpr int GRASS_SHADES = 3, GRASS_VARIANTS = 6, SOIL_PATCHES = 4;

inline u32 mixRGB(u32 a, u32 b, float t) {  // t = 0 -> a, 1 -> b
    auto ch = [&](int s) { return (int)(((a >> s) & 255) * (1.f - t) + ((b >> s) & 255) * t); };
    return HEX((u32)((ch(16) << 16) | (ch(8) << 8) | ch(0)));
}

// The meadow's greens, in the bright lime of the new reference: four tones per shade, darkest to lightest
static const u32 MEADOW_TONE[GRASS_SHADES][4] = {{0x5fa83e, 0x7cc450, 0x95d460, 0xb6e486},   // light meadow (the tree / tuft grass ramp, 0x7cc450 / 0x4a963c / 0x286830, a step either side)
                                                 {0x3f8b30, 0x5fa83e, 0x7cc450, 0x95d460},   // medium
                                                 {0x286830, 0x3a7c34, 0x4a963c, 0x66ad48}};  // dark grass  // dark grass

// 16 x 16 grass covered end to end in blades, tufted like the reference: soft light and dark patches across the tile, small fans of three blades
// (one upright, two leaning out) with pale tips, and dark gaps between them. Strokes wrap at the edges, so tiles join up seamlessly.
inline Img mkGrassTile(int shade, int variant) {
    Img im(16, 16);
    Rng r(0x9a55u + shade * 331u + variant * 57u);
    const u32* tn = MEADOW_TONE[shade];
    auto put = [&](int x, int y, u32 c) { im.set(((x % 16) + 16) % 16, ((y % 16) + 16) % 16, HEX(c)); };
    const float ph = r.irange(0, 628) / 100.f;
    for (int y = 0; y < 16; y++)  // soft patches: the base swings between the two middle tones (wraps every 16 pixels)
        for (int x = 0; x < 16; x++) {
            const float n = sinf(x * 0.3927f * 1.f + ph) + sinf(y * 0.3927f * 2.f - ph) + sinf((x + y) * 0.3927f + ph * 2.f);
            put(x, y, n > 0.7f ? tn[2] : (n < -0.9f ? tn[0] : tn[1]));
        }
    for (int i = 0; i < 28; i++) {  // dark gaps between the blades, drawn first so the lighter blades overlap them
        const int x = r.irange(0, 15), y = r.irange(0, 15);
        put(x, y, tn[0]);
        put(x, y + 1, tn[0]);
    }
    for (int i = 0; i < 9; i++) {  // a tuft: a fan of three blades from one root
        const int x = r.irange(0, 15), y = r.irange(2, 17), h = r.irange(3, 5);
        for (int k = 0; k < h; k++) {
            put(x, y - k, tn[k >= h - 1 ? 3 : (k < h / 2 ? 1 : 2)]);
            if (k >= 1) put(x - (k + 1) / 2, y - k, tn[k >= h - 1 ? 3 : 2]);
            if (k >= 1) put(x + (k + 1) / 2, y - k, tn[k >= h - 1 ? 3 : 1]);
        }
    }
    for (int i = 0; i < 24; i++) {  // single short blades filling the rest
        const int x = r.irange(0, 15), y = r.irange(0, 15), len = r.irange(2, 3);
        for (int k = 0; k < len; k++) put(x, y - k, tn[k == len - 1 ? 3 : 1 + (k & 1)]);
    }
    return im;
}

// A patch of bare dark soil in the grass: rounded and a little ragged, with specks and grass blades round its rim. 16 x 16.
inline Img mkSoilPatch(int variant) {
    static const u32 SOIL[4] = {0x5e4128, 0x6c4a2c, 0x8a6339, 0x3f2a1a};  // warm browns, from the leaf-floor and path dirt (the old grey-brown read as scorch marks)
    Img im(16, 16);
    Rng r(0x50a1u + variant * 811u);
    const float rx = 5.2f + r.irange(0, 20) / 10.f, ry = 4.4f + r.irange(0, 18) / 10.f, ph = r.irange(0, 628) / 100.f;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const float dx = (x - 7.5f) / rx, dy = (y - 8.f) / ry;
            const float ang = atan2f(dy, dx), edge = 1.f + 0.16f * sinf(ang * 3.f + ph) + 0.1f * sinf(ang * 5.f - ph);
            const float d = sqrtf(dx * dx + dy * dy) / edge;
            if (d < 1.f) im.set(x, y, HEX(d > 0.8f ? SOIL[3] : SOIL[r.irange(0, 9) < 6 ? 0 : 1]));
            else if (d < 1.22f && (x + y + variant) % 2 == 0) im.set(x, y, HEX(0x1f5a22));  // a darker rim of flattened grass
        }
    for (int i = 0; i < 8; i++) {  // lighter specks and small stones
        const int x = r.irange(3, 12), y = r.irange(4, 12);
        if (im.get(x, y) != 0) im.set(x, y, HEX(SOIL[2]));
    }
    for (int i = 0; i < 4; i++) {  // blades standing up round the rim
        const int a = r.irange(0, 628);
        const int x = (int)lroundf(7.5f + cosf(a / 100.f) * (rx + 0.8f)), y = (int)lroundf(8.f + sinf(a / 100.f) * (ry + 0.8f));
        im.set(x, y, HEX(0x3a7a38));
        im.set(x, y - 1, HEX(0x4b9a4b));
        im.set(x, y - 2, HEX(0x6fc86e));
    }
    return im;
}

// Which grass tiles hold a soil patch: some areas of the map are scattered with them. Patches only sit well inside open grass.
inline bool soilPatchAt(const World& w, int tx, int ty) {
    if (tx < 1 || ty < 1 || tx >= MW - 1 || ty >= MH - 1) return false;
    auto grassy = [&](int x, int y) { const uint8_t t = w.tile[y][x]; return (t == T_GRASS || t == T_DARK) && !w.solid[y][x]; };
    if (!grassy(tx, ty) || !grassy(tx - 1, ty) || !grassy(tx + 1, ty) || !grassy(tx, ty - 1) || !grassy(tx, ty + 1)) return false;
    if (hash2(tx / 5, ty / 5, 55) % 6 != 0) return false;  // only in some areas (about one in six)
    return hash2(tx, ty, 56) % 100 < 36;
}
