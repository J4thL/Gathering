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

// 16 x 16 flat grass: the shade's colour with a few lighter and darker specks and tiny blade marks (pack: light, medium, dark)
inline Img mkGrassTile(int shade, int variant) {
    static const u32 BASE[GRASS_SHADES] = {0x6fc86e, 0x48a948, 0x29752a};
    Img im(16, 16);
    Rng r(0x9a55u + shade * 331u + variant * 57u);
    const u32 base = BASE[shade];
    im.fill(HEX(base));
    const u32 lite = mixRGB(base, 0xffffff, 0.16f), dark = mixRGB(base, 0x0a2a0c, 0.22f);
    for (int i = 0; i < 7; i++) im.set(r.irange(0, 15), r.irange(0, 15), lite);
    for (int i = 0; i < 9; i++) im.set(r.irange(0, 15), r.irange(0, 15), dark);
    for (int i = 0; i < 3; i++) {  // a blade mark: two pixels, one above the other
        const int x = r.irange(1, 14), y = r.irange(2, 14);
        im.set(x, y, dark);
        im.set(x, y - 1, lite);
    }
    return im;
}

// Grass made of thin straight blades, not rounded tufts: every blade is a one pixel line from the ground to its tip, leaning a little and bending
// further at the tip as it sways. The tree-root grass and the meadow's clumps are both drawn from this, in the set's own three greens (a light, b mid, c dark).
inline Img mkLineGrass(int w, int h, const u32* pal, int frame, uint32_t seed) {
    Img im(w, h);
    Rng r(seed);
    const float ph = frame * 6.2832f / 24.f;
    const int n = std::max(5, w * 7 / 10);
    for (int i = 0; i < n; i++) {
        const int x0 = 1 + (int)((w - 2) * (i + r.irange(0, 6) / 10.f) / n), bh = std::max(2, h - 2 - r.irange(0, std::max(1, h / 2)));
        const float lean = (r.irange(0, 10) - 5) * 0.07f, phase = r.irange(0, 628) / 100.f;
        const float sway = sinf(ph + phase + x0 * 0.25f) * 1.3f;
        for (int y = 0; y < bh; y++) {
            const float f = (float)y / bh;
            const int x = x0 + (int)lroundf(lean * y + sway * f * f), py = h - 1 - y;
            if (x < 0 || x >= w) continue;
            im.set(x, py, pal[y == bh - 1 ? 0 : (f > 0.5f ? 1 : 2)]);  // dark at the root, mid, light at the tip
        }
    }
    return im;
}

// A patch of bare dark soil in the grass: rounded and a little ragged, with specks and grass blades round its rim. 16 x 16.
inline Img mkSoilPatch(int variant) {
    static const u32 SOIL[4] = {0x41362f, 0x4e3f38, 0x5a4a42, 0x2e2520};
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
