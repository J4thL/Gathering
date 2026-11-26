// sprites_biome.hpp - procedural pixel art for the desert, swamp, snow and mountain biomes.
// Same rules as sprites.hpp: everything is painted in code, nothing is loaded from disk.
#pragma once
#include "core/data.hpp"
#include "art/crystal_sprite.hpp"
#include "art/snow_landmarks.hpp"

// ================================================================= ground tiles (16x16)
inline Img mkSpeckleTile(u32 base, u32 dark, u32 light, int nd, int nl, Rng& r) {
    Img im(16, 16);
    im.fill(base);
    for (int i = 0; i < nd; i++) im.set(r.irange(0, 15), r.irange(0, 15), dark);
    for (int i = 0; i < nl; i++) im.set(r.irange(0, 15), r.irange(0, 15), light);
    return im;
}
inline Img mkSand(int v, Rng& r) {
    Img im = mkSpeckleTile(HEX(0xdfc582), HEX(0xcdb06a), HEX(0xeed998), 16, 12, r);
    for (int i = 0; i < 2 + v; i++) {  // tiny wind ripples
        int x = r.irange(1, 11), y = r.irange(2, 14);
        for (int k = 0; k < 4; k++) im.set(x + k, y + (k == 1 || k == 2 ? 0 : 1), HEX(0xd2b874));
    }
    return im;
}
inline Img mkDune(int v, Rng& r) {
    Img im(16, 16);
    im.fill(HEX(0xd8ba70));
    for (int y = 0; y < 16; y++) {
        int off = (int)(sinf((y + v * 5) * 0.55f) * 2.2f);
        for (int x = 0; x < 16; x++) {
            int w = (x + off + 16) % 16;
            if (w < 5) im.set(x, y, HEX(0xc7a35c));       // shaded slope
            else if (w > 12) im.set(x, y, HEX(0xe9d28f));  // sunlit crest
        }
    }
    for (int i = 0; i < 6; i++) im.set(r.irange(0, 15), r.irange(0, 15), HEX(0xb9964f));
    return im;
}
inline Img mkMud(int v, Rng& r) {
    Img im = mkSpeckleTile(HEX(0x6c5839), HEX(0x584629), HEX(0x84704b), 20, 8, r);
    for (int i = 0; i < 1 + v; i++) {  // small puddles
        int x = r.irange(2, 11), y = r.irange(3, 12);
        for (int k = 0; k < 4; k++) im.set(x + k, y, HEX(0x4b6a5d));
        for (int k = 1; k < 3; k++) im.set(x + k, y + 1, HEX(0x3c5a4f));
        im.set(x + 1, y, HEX(0x7aa397));
    }
    return im;
}
inline Img mkMurk(int frame) {  // shallow, murky, walkable water - slow
    Img im(16, 16);
    im.fill(HEX(0x4d6e47));
    for (int k = 0; k < 5; k++) {
        int x = (k * 5 + frame * 3) % 16, y = (k * 7 + 1) % 16;
        for (int i = 0; i < 3; i++) im.set((x + i) % 16, y, HEX(0x6f9a68));
        int x2 = (k * 9 + 2 + frame * 3) % 16, y2 = (k * 5 + 9) % 16;
        im.set(x2 % 16, y2, HEX(0x3a5636));
        im.set((x2 + 1) % 16, y2, HEX(0x3a5636));
    }
    return im;
}
inline Img mkBog(int frame) {  // deep bog: dark, impassable
    Img im(16, 16);
    im.fill(HEX(0x2b4237));
    for (int k = 0; k < 4; k++) {
        int x = (k * 6 + frame * 4) % 16, y = (k * 5 + 3) % 16;
        for (int i = 0; i < 3; i++) im.set((x + i) % 16, y, HEX(0x3f6150));
        int x2 = (k * 7 + 5 + frame * 2) % 16, y2 = (k * 9 + 6) % 16;
        im.set(x2, y2, HEX(0x1f3029));
    }
    if (frame & 1) im.set(11, 4, HEX(0x7fa88e));  // a bubble
    else im.set(4, 11, HEX(0x7fa88e));
    return im;
}
inline Img mkSnow(int v, Rng& r) {
    Img im = mkSpeckleTile(HEX(0xeef3f8), HEX(0xd0dcea), HEX(0xfffdf6), 12 + v * 2, 10, r);
    for (int i = 0; i < 2; i++) {
        int x = r.irange(1, 12), y = r.irange(2, 14);
        im.set(x, y, HEX(0xbccce1));
        im.set(x + 1, y, HEX(0xbccce1));
    }
    return im;
}
// the soft pattern on the snowfields' ground: faint plus-shaped sparkles and small drifted blobs, a tone darker than the snow itself, so the
// ground has a texture without getting busy. Added on top of the speckle tile with its own random numbers, so nothing else's art shifts.
inline void snowTexture(Img& im, int variant, bool packed) {
    Rng r(0x5a0b + variant * 977 + (packed ? 31 : 0));
    const u32 plus = packed ? HEX(0xa5b9d0) : HEX(0xc0d0e5), blob = packed ? HEX(0xb3c5d9) : HEX(0xcddbec), glint = packed ? HEX(0xf2f7fb) : HEX(0xfffdf6);
    for (int i = 0; i < 3; i++) {  // drifted blobs: a lumpy 3 x 2 patch with a lit pixel
        const int x = r.irange(1, 11), y = r.irange(2, 12);
        for (int k = 0; k < 3; k++) im.set(x + k, y + (k == 1 ? 0 : 1), blob);
        im.set(x + 1, y + 1, blob);
        im.set(x + 1, y - 1 < 0 ? 0 : y - 1, blob);
    }
    for (int i = 0; i < 4; i++) {  // plus-shaped sparkles
        const int x = r.irange(2, 13), y = r.irange(2, 13);
        im.set(x, y, plus); im.set(x - 1, y, plus); im.set(x + 1, y, plus); im.set(x, y - 1, plus); im.set(x, y + 1, plus);
        im.set(x, y, glint);
    }
}
inline Img mkSnowPacked(int v, Rng& r) {
    Img im = mkSpeckleTile(HEX(0xcad7e5), HEX(0xb6c7d9), HEX(0xdde8f2), 14, 8, r);
    for (int i = 0; i < 3 + v; i++) im.set(r.irange(0, 15), r.irange(0, 15), HEX(0x9d8f7c));  // trampled earth
    return im;
}
inline Img mkIce(int v, Rng& r) {
    Img im(16, 16);
    im.fill(HEX(0xa6d6ee));
    for (int i = 0; i < 10; i++) im.set(r.irange(0, 15), r.irange(0, 15), HEX(0x93c8e4));
    for (int i = 0; i < 4; i++) im.set(r.irange(0, 15), r.irange(0, 15), HEX(0xdff4ff));
    int x = r.irange(2, 6), y = r.irange(2, 8);  // crack
    for (int k = 0; k < 8 + v * 2; k++) {
        im.set(x, y, HEX(0xe9f9ff));
        x += (k % 3 == 2) ? 1 : 1;
        y += (k % 2);
        if (x > 15 || y > 15) break;
    }
    for (int k = 0; k < 5; k++) im.set(2 + k * 2, 14 - k, HEX(0xcdeeff));  // glare streak
    return im;
}
inline Img mkStone(int v, Rng& r) {
    Img im = mkSpeckleTile(HEX(0x888b96), HEX(0x74777f), HEX(0x9ea1ac), 18, 12, r);
    for (int i = 0; i < 2 + v; i++) {
        int x = r.irange(1, 11), y = r.irange(1, 13);
        im.set(x, y, HEX(0x5f626c));
        im.set(x + 1, y + 1, HEX(0x5f626c));
        im.set(x + 2, y + 1, HEX(0x5f626c));
    }
    for (int i = 0; i < 3; i++) im.set(r.irange(1, 14), r.irange(1, 14), HEX(0x6f9a58));  // lichen
    return im;
}
// A cliff of the snowfields: warm brown rock in stacked, uneven blocks. cap = the top tile of a ridge, with a deep snow lip over the rock whose lower edge
// is scalloped (so it reads as snow lying over a ledge, the way the reference sheets draw it); not cap = the bare face below it.
inline Img mkSnowCliff(int v, bool cap, Rng& r) {
    Img im(16, 16);
    const u32 ROCK = HEX(0x6b5a4d), DK = HEX(0x4a3d35), LT = HEX(0x8a7662), SH = HEX(0x3a3029);
    im.fill(ROCK);
    for (int row = 0; row < 4; row++) {  // rows of blocks, each row offset from the one above; the joints between them dark
        const int y0 = row * 4 + (v % 2);
        for (int x = 0; x < 16; x++) if (y0 >= 0 && y0 < 16) im.set(x, y0, DK);
        const int off = (row * 5 + v * 3) % 8;
        for (int jx = off; jx < 16; jx += 8) for (int y = y0 + 1; y < y0 + 4 && y < 16; y++) im.set(jx, y, DK);
        for (int k = 0; k < 3; k++) {  // a lit top edge on a block or two, and chips
            const int x = r.irange(1, 13), y = y0 + 1;
            if (y < 16) { im.set(x, y, LT); im.set(x + 1, y, LT); }
        }
    }
    for (int i = 0; i < 6; i++) im.set(r.irange(0, 15), r.irange(0, 15), r.chance(0.5f) ? DK : LT);
    if (cap) {
        const float ph = r.f() * 6.28f;
        for (int x = 0; x < 16; x++) {
            const int h = 5 + (int)lroundf(1.6f * sinf(x * 0.9f + ph) + 1.1f * sinf(x * 2.3f + ph * 2.f));  // how far the snow reaches down this column
            for (int y = 0; y < h && y < 16; y++) im.set(x, y, y < 2 ? HEX(0xffffff) : y == h - 1 ? HEX(0xb9cde2) : HEX(0xeef4fa));
            if (h < 16) im.set(x, h, SH);  // the shadow the lip throws on the rock below it
        }
    }
    return im;
}
#include "art/leafy_birch_art.hpp"  // the snowfield's leafy birch
#include "art/snow_tent_old_art.hpp"  // ... and the one that has come down
#include "art/snow_tent_art.hpp"  // the snowfield's hand-drawn tent
#include "art/snow_props.hpp"  // the snowfield's redrawn snowmen, lamp, stumps, logs and ground clutter
// ---- snowfield props: the town's sign, lamp and fence and the forest's stump, drawn again with snow lying on them, and a snowman
inline Img mkSnowSign(const char* text) {
    Img im(32, 26);
    const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2), BL = HEX(0xaec3da);
    im.rect(14, 12, 3, 13, HEX(0x6b4423));
    im.rect(16, 12, 1, 13, HEX(0x52341a));
    im.rect(2, 2, 28, 12, HEX(0xc99a5a));
    im.rect(2, 13, 28, 1, HEX(0x8a5c34));
    im.rect(3, 3, 26, 1, HEX(0x8a5c34));
    im.text(8, 6, text, HEX(0x3b2616));
    im.set(4, 4, HEX(0x52341a));
    im.set(27, 4, HEX(0x52341a));
    // the snow on the board: a lip along the top whose lower edge is uneven, with a drift in each corner
    im.rect(2, 1, 28, 2, W);
    for (int x = 2; x < 30; x++) {
        const int h = (x * 7 + 3) % 5;
        if (h < 2) im.set(x, 3, W2);
        if (h == 0) im.set(x, 4, BL);
    }
    im.rect(2, 12, 3, 1, W); im.rect(27, 12, 3, 1, W2);
    im.rect(11, 24, 9, 1, W); im.rect(13, 23, 5, 1, W2);  // snow heaped round the post
    im.outline(OUT);
    return im;
}
inline Img mkSnowLamp() {
    Img im(10, 36);
    const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2);
    im.rect(4, 8, 2, 26, HEX(0x2e2e38));
    im.rect(3, 32, 4, 3, HEX(0x3a3a46));
    im.rect(2, 3, 6, 6, HEX(0xffe9a8));
    im.rect(2, 8, 6, 1, HEX(0x2e2e38));
    im.rect(2, 3, 1, 6, HEX(0x2e2e38));
    im.rect(7, 3, 1, 6, HEX(0x2e2e38));
    im.rect(3, 4, 4, 4, HEX(0xfff6d0));
    im.rect(2, 3, 6, 1, HEX(0x2e2e38));
    im.rect(2, 1, 6, 2, W);      // a cap of snow on the lantern
    im.set(2, 2, W2); im.set(7, 2, W2); im.set(5, 3, W2);
    im.rect(2, 33, 6, 2, W);     // and a drift round the foot
    im.rect(3, 32, 4, 1, W2);
    im.outline(OUT);
    return im;
}
inline Img mkSnowFence() {
    Img im(18, 18);
    const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2);
    im.rect(3, 3, 2, 12, HEX(0x8a5c34));
    im.rect(12, 3, 2, 12, HEX(0x8a5c34));
    im.rect(0, 6, 18, 2, HEX(0xb07a45));
    im.rect(0, 11, 18, 2, HEX(0xb07a45));
    im.rect(0, 5, 18, 2, W);                     // the upper rail carries a ridge of snow, the lower one only patches
    for (int x = 0; x < 18; x += 3) im.set(x, 7, W2);
    im.rect(1, 10, 6, 2, W); im.rect(10, 10, 5, 1, W); im.rect(11, 11, 3, 1, W2);
    im.rect(2, 2, 4, 2, W); im.rect(11, 2, 4, 2, W);  // a cap on each post
    im.set(2, 4, W2); im.set(14, 4, W2);
    im.rect(2, 14, 5, 1, W); im.rect(11, 14, 4, 1, W);  // drifts at the foot
    im.outline(OUT);
    return im;
}
inline Img mkSnowStump() {
    Img im(14, 11);
    im.rect(2, 4, 10, 6, HEX(0x6a4220));
    im.rect(2, 4, 3, 6, HEX(0x8a5a2c));
    im.set(3, 9, HEX(0x4a2c14));
    im.ellipse(7, 4, 5.4f, 2.8f, HEX(0xf4f8fc));      // the cut face is a pad of snow
    im.ellipse(7, 4.5f, 3.6f, 1.6f, HEX(0xdbe6f2));
    im.set(5, 3, HEX(0xffffff)); im.set(9, 4, HEX(0xffffff));
    im.rect(2, 8, 3, 1, HEX(0xdbe6f2)); im.rect(9, 9, 3, 1, HEX(0xf4f8fc));  // snow against the foot
    im.outline(OUT);
    return im;
}
inline Img mkSnowman() {
    Img im(16, 25);
    const u32 L = HEX(0xffffff), M = HEX(0xe3ecf6), D = HEX(0xa9bdd4), COAL = HEX(0x2a2a30), STICK = HEX(0x5a3d24);
    im.line(4, 13, 1, 9, STICK); im.line(12, 13, 15, 9, STICK);  // the arms, behind
    im.set(1, 8, STICK); im.set(15, 8, STICK); im.set(2, 11, STICK);
    im.ellipseShaded(8, 19, 5.8f, 5.0f, L, M, D);
    im.ellipseShaded(8, 12.5f, 4.4f, 3.9f, L, M, D);
    im.ellipseShaded(8, 7.5f, 3.5f, 3.3f, L, M, D);
    im.set(7, 7, COAL); im.set(10, 7, COAL);                     // eyes
    im.set(8, 8, HEX(0xe8802a)); im.set(9, 8, HEX(0xe8802a)); im.set(10, 8, HEX(0xc9631a));  // a carrot nose pointing right
    im.set(6, 9, COAL); im.set(7, 10, COAL); im.set(9, 10, COAL); im.set(10, 9, COAL);       // a smile
    im.rect(5, 10, 7, 1, HEX(0xc0392b)); im.rect(10, 11, 2, 3, HEX(0xc0392b)); im.set(10, 13, HEX(0x8f271c));  // scarf and its tail
    im.rect(5, 4, 7, 1, HEX(0x24242c)); im.rect(6, 1, 5, 3, HEX(0x24242c)); im.rect(6, 3, 5, 1, HEX(0x8f271c));  // a top hat
    im.set(8, 16, COAL); im.set(8, 19, COAL);                    // buttons
    im.outline(OUT);
    return im;
}
// ---- snowfield campsite: a fire in a ring of stones on a patch of thawed ground, a tent, and a log seat, all with snow on them
inline Img mkSnowCampfire(int frame) {
    Img im(20, 22);
    const u32 W = HEX(0xf4f8fc);
    im.ellipse(10, 17, 9.f, 4.2f, HEX(0xb7c6da));      // the snow thawed back round the fire: a wet, dark patch with a damp rim
    im.ellipse(10, 17, 7.6f, 3.4f, HEX(0x5a4c44));
    im.ellipse(10, 17.5f, 5.4f, 2.3f, HEX(0x2e2622));  // ash and char
    for (int i = 0; i < 8; i++) {                      // stones, the ones on the far side with a cap of snow
        const float a = i * 0.785f + 0.2f;
        const int sx = (int)(10 + cosf(a) * 6.5f), sy = (int)(16 + sinf(a) * 3.f);
        im.ellipse((float)sx, (float)sy, 1.9f, 1.5f, i % 2 ? HEX(0x8d929e) : HEX(0x6a6f7c));
        if (sinf(a) < 0.1f) im.set(sx, sy - 1, W);
    }
    im.line(5, 17, 14, 14, HEX(0x6b4423));
    im.line(5, 16, 14, 13, HEX(0x8d5e33));
    im.line(6, 14, 14, 18, HEX(0x52341a));
    im.line(6, 13, 14, 17, HEX(0x7a5230));
    const float hgt[3] = {8, 10, 7};
    const float h = hgt[frame % 3];
    im.ellipse(10, 13 - h * 0.35f, 4.2f - frame * 0.3f, h * 0.55f, HEX(0xe0452a));
    im.ellipse(10, 13 - h * 0.28f, 3.0f, h * 0.45f, HEX(0xff8a2b));
    im.ellipse(10 + (frame - 1) * 0.6f, 13 - h * 0.18f, 1.8f, h * 0.3f, HEX(0xffd34d));
    im.set(10, 5 + frame, HEX(0xffd34d));
    return im;
}
inline Img mkSnowTent() {
    Img im(32, 26);
    const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2), BL = HEX(0xaec3da);
    const u32 CL = HEX(0xc9694a), CD = HEX(0xa24f38), CK = HEX(0x7e3b2c), DOOR = HEX(0x2a1a1a), DOOR2 = HEX(0x4a2a24);
    for (int y = 3; y <= 22; y++) {                    // an A-frame seen from the front: the lit side left, the shaded side right
        const float hw = 0.7f + (y - 3) * 0.78f;
        for (int x = (int)(16 - hw); x <= (int)(16 + hw); x++) im.set(x, y, x < 16 ? CL : CD);
    }
    for (int y = 3; y <= 22; y += 5) im.line(16, y, 16 + (y - 3) / 3, y + 4, CK);  // seams
    im.line(16, 3, 16, 11, CK);                        // the ridge line
    for (int y = 11; y <= 22; y++) {                   // the doorway, a flap folded back on each side
        const float hw = 0.5f + (y - 11) * 0.45f;
        for (int x = (int)(16 - hw); x <= (int)(16 + hw); x++) im.set(x, y, y < 14 ? DOOR : DOOR2);
        im.set((int)(16 - hw) - 1, y, CK); im.set((int)(16 + hw) + 1, y, CK);
    }
    for (int y = 3; y <= 15; y++) {                    // snow lying along both slopes
        const float hw = 0.7f + (y - 3) * 0.78f;
        if ((y + 1) % 4 != 0) { im.set((int)(16 - hw), y, W); im.set((int)(16 + hw), y, W2); }
        if (y < 9) { im.set((int)(16 - hw) + 1, y, W2); im.set((int)(16 + hw) - 1, y, BL); }
    }
    im.rect(14, 2, 5, 2, W);                           // a cap on the ridge
    im.rect(1, 21, 30, 2, W); im.rect(3, 20, 8, 1, W); im.rect(22, 20, 7, 1, W2); im.rect(4, 22, 24, 1, W2);  // drifted up round the foot
    im.outline(OUT);
    return im;
}
inline Img mkSnowLog() {
    Img im(20, 10);
    const u32 W = HEX(0xf4f8fc), W2 = HEX(0xdbe6f2);
    im.rect(2, 3, 17, 5, HEX(0x6b4423));
    im.rect(2, 3, 17, 1, HEX(0x8d5e33));
    im.rect(2, 7, 17, 1, HEX(0x52341a));
    im.set(7, 5, HEX(0x52341a)); im.set(8, 5, HEX(0x52341a)); im.set(13, 6, HEX(0x52341a));  // bark
    im.ellipse(2.5f, 5.5f, 1.9f, 2.7f, HEX(0xc99a5a));  // the sawn end
    im.set(2, 5, HEX(0x8a5c34)); im.set(3, 6, HEX(0x8a5c34));
    im.rect(5, 2, 13, 1, W); im.rect(4, 1, 11, 1, W); im.rect(6, 3, 8, 1, W2);  // snow heaped along the top
    im.rect(3, 8, 16, 1, W2); im.rect(8, 9, 7, 1, W);                            // and at its foot
    im.outline(OUT);
    return im;
}
// ---- snowfield ground detail: frost cracks, animal tracks and half-buried rocks (flat decor, a few pixels each)
inline Img mkFrostCrack(int v) {
    Img im(14, 8);
    const u32 CR = HEX(0x8aa2c2), HI = HEX(0xf6faff);
    static const int px[2][6][2] = {{{1, 4}, {3, 3}, {5, 4}, {7, 3}, {9, 4}, {12, 3}}, {{1, 3}, {3, 4}, {6, 3}, {8, 4}, {10, 3}, {12, 4}}};
    for (int i = 0; i + 1 < 6; i++) { im.line(px[v][i][0], px[v][i][1], px[v][i + 1][0], px[v][i + 1][1], CR); }
    im.line(px[v][2][0], px[v][2][1], px[v][2][0] + 1, 6, CR);   // two short branches
    im.line(px[v][4][0], px[v][4][1], px[v][4][0] - 1, 1, CR);
    for (int i = 0; i < 6; i += 2) im.set(px[v][i][0] + 1, px[v][i][1] + 1, HI);  // a lit edge on the far side of the crack
    return im;
}
inline Img mkTracks(int v) {
    Img im(18, 9);
    const u32 SH = HEX(0x98b0cd), HI = HEX(0xf6faff);
    auto paw = [&](int x, int y) {  // a pad and four toes, with the lit rim of the snow on its upper side
        im.set(x + 1, y + 2, SH); im.set(x + 2, y + 2, SH); im.set(x + 1, y + 3, SH); im.set(x + 2, y + 3, SH);
        im.set(x, y + 1, SH); im.set(x + 1, y, SH); im.set(x + 2, y, SH); im.set(x + 3, y + 1, SH);
        im.set(x + 1, y + 1, HI);
    };
    if (v == 0) { paw(0, 4); paw(5, 1); paw(10, 4); paw(14, 1); }
    else { paw(1, 1); paw(6, 4); paw(11, 1); paw(14, 5); }
    return im;
}
inline Img mkBuriedRock(int v) {
    Img im(11, 8);
    im.ellipseShaded(5.5f, 4.6f, 4.7f, 3.0f, HEX(0xa6acba), HEX(0x838a9a), HEX(0x606677));
    im.ellipse(5.0f, 2.6f, 3.0f, 1.3f, HEX(0xf4f8fc));   // a cap of snow on the top of the rock, the grey showing round it
    im.set(8, 3, HEX(0xdde8f3)); im.set(2, 3, HEX(0xdde8f3));
    if (v) { im.set(2, 5, HEX(0x606677)); im.set(8, 6, HEX(0x606677)); }
    im.outline(OUT);
    return im;
}
// ---- snowfield tree variety: a young snow-laden pine (the bare birch is an animated tree now: art/tree_snow_birch.hpp)
inline Img mkYoungPine() {
    Img im(18, 28);
    const u32 BASE = HEX(0x2e6c5b), SHADE = HEX(0x1f4e48), LIGHT = HEX(0x4c9180), SNOW = HEX(0xf6fafd), SNOW2 = HEX(0xd5e3ee), TRUNK = HEX(0x6b4a34);
    im.rect(8, 23, 3, 4, TRUNK);
    for (int k = 0; k < 3; k++) {  // three tiers, each wider than the one above, the top one the narrowest
        const int y0 = 2 + k * 7;
        for (int y = y0; y <= y0 + 9 && y < 25; y++) {
            const int half = (int)(0.5f + (y - y0 + 1) * (0.46f + 0.04f * k)) + (k > 0 ? 1 : 0);
            for (int x = 9 - half; x <= 9 + half; x++) {
                if (x < 0 || x > 17) continue;
                const u32 c = (x - (9 - half) < 2 && y - y0 < 8) ? LIGHT : (9 + half - x < 2) ? SHADE : (y == y0 + 9 ? SHADE : BASE);
                im.set(x, y, c);
            }
        }
        for (int y = y0; y < y0 + 4; y++)  // snow on the upper part of each tier, heavier on the left
            for (int x = 9 - 2 - k; x <= 9 + 1 + k; x++)
                if (im.get(x, y) != 0 && ((x + y + k) % 3 != 0 || y == y0)) im.set(x, y, (y - y0) < 2 ? SNOW : SNOW2);
    }
    im.rect(5, 26, 8, 1, SNOW); im.rect(6, 25, 5, 1, SNOW2);   // a little drift at the foot
    im.outline(HEX(0x173f39));
    return im;
}
inline Img mkCliff(int v, Rng& r) {
    Img im(16, 16);
    im.fill(HEX(0x5a5c6e));
    for (int y = 0; y < 16; y++) {  // sedimentary strata
        int band = (y + v * 3) % 6;
        if (band == 0) for (int x = 0; x < 16; x++) im.set(x, y, HEX(0x4a4c5c));
        if (band == 3) for (int x = 0; x < 16; x++) im.set(x, y, HEX(0x6a6c80));
    }
    for (int i = 0; i < 14; i++) im.set(r.irange(0, 15), r.irange(0, 15), HEX(0x3f4150));
    for (int i = 0; i < 6; i++) im.set(r.irange(0, 15), r.irange(0, 15), HEX(0x7b7e92));
    return im;
}

// ================================================================= props
inline Img mkBoulder() {
    Img im(30, 24);
    u32 lt = HEX(0xa9acb8), md = HEX(0x7d808d), dk = HEX(0x575a67);
    im.ellipseShaded(15, 13, 13, 8.5f, lt, md, dk);
    im.ellipseShaded(8, 16, 6, 4.5f, lt, md, dk);
    im.ellipseShaded(22, 16, 6, 4, lt, md, dk);
    im.line(14, 6, 17, 12, dk);
    im.line(9, 10, 11, 13, dk);
    im.set(8, 7, HEX(0xd6d8e0));
    im.set(9, 6, HEX(0xd6d8e0));
    im.set(10, 6, HEX(0xd6d8e0));
    im.outline(OUT);
    return im;
}

// A soft tinted rim for the no-black-outline plants: dark where the shape meets the ground or its own shade, a mid tone along the lit top edge.
inline void softRim(Img& im, u32 under, u32 top) {
    const Img src = im;
    auto a = [&](int x, int y) { return (src.get(x, y) >> 24) != 0; };
    for (int y = 0; y < im.h; y++)
        for (int x = 0; x < im.w; x++) {
            if (!a(x, y)) continue;
            if (a(x + 1, y) && a(x - 1, y) && a(x, y + 1) && a(x, y - 1)) continue;
            im.set(x, y, (!a(x, y + 1) || !a(x - 1, y) || !a(x + 1, y)) ? under : top);
        }
}
// Prickly pear: flat pads stacked and fanned out, pale spines, magenta fruit along the top. (Replaces the tall saguaro.)
inline Img mkPricklyPear(Rng& r) {
    Img im(24, 26);
    const u32 DK = HEX(0x2f6a45), MD = HEX(0x4f9a62), LT = HEX(0x7fc27a);
    auto pad = [&](float cx, float cy, float rx, float ry, float ang) {
        const float ca = cosf(ang), sa = sinf(ang), big = std::max(rx, ry);
        for (int pass = 0; pass < 2; pass++) {  // pass 0: a dark ring one pixel wider, so each pad is cut out from the ones behind it
            const float g = pass == 0 ? 0.9f : 0.f;
            for (int y = 0; y < im.h; y++)
                for (int x = 0; x < im.w; x++) {
                    const float dx = x + .5f - cx, dy = y + .5f - cy;
                    const float u = (dx * ca + dy * sa) / (rx + g), v = (-dx * sa + dy * ca) / (ry + g);
                    if (u * u + v * v > 1.f) continue;
                    const float s = -(dx / big) * 0.5f - (dy / big) * 0.85f;
                    im.set(x, y, pass == 0 ? DK : s > 0.30f ? LT : s > -0.30f ? MD : DK);
                }
        }
    };
    pad(17.5f, 5.0f, 2.8f, 3.6f, 0.6f);
    pad(11.0f, 7.5f, 3.9f, 5.2f, -0.1f);
    pad(6.5f, 13.0f, 4.2f, 5.3f, -0.55f);
    pad(17.5f, 12.0f, 4.2f, 5.5f, 0.45f);
    pad(12.0f, 19.5f, 6.8f, 5.6f, 0.0f);
    for (int i = 0; i < 26; i++) {  // spines
        const int x = r.irange(2, 21), y = r.irange(2, 24);
        const u32 c = im.get(x, y);
        if (c == MD || c == LT) im.set(x, y, HEX(0xeef2c8));
    }
    im.rect(0, 25, 24, 1, 0u);
    softRim(im, HEX(0x1b3a28), DK);
    const u32 F0 = HEX(0xd8447a), F1 = HEX(0xf59ab8);  // fruit on the upper edges
    for (auto [fx, fy] : {std::pair<int, int>{16, 0}, {10, 1}, {5, 7}, {19, 6}, {18, 9}}) {
        im.set(fx, fy, F1); im.set(fx + 1, fy, F0); im.set(fx, fy + 1, F0); im.set(fx + 1, fy + 1, F0);
    }
    return im;
}
// Barrel cactus: a ribbed ball seen from a little above, spines along the ribs, a small crown flower.
inline Img mkCactusBarrel() {
    Img im(16, 16);
    const u32 DK = HEX(0x2f6a45), MD = HEX(0x4f9a62), LT = HEX(0x7fc27a);
    const float cx = 8.f, cy = 9.5f, rx = 6.8f, ry = 5.9f;
    for (int y = 0; y <= 14; y++)
        for (int x = 0; x < 16; x++) {
            const float u = (x + .5f - cx) / rx, v = (y + .5f - cy) / ry;
            if (u * u + v * v > 1.f) continue;
            const float phi = asinf(std::max(-1.f, std::min(1.f, u)));
            const float k = (phi + 1.5708f) / 3.1416f * 7.f, f = k - floorf(k);  // seven ribs across the front
            const float s = -u * 0.55f - v * 0.85f;
            u32 c = s > 0.35f ? LT : s > -0.25f ? MD : DK;
            if (f < 0.16f) c = DK;
            else if (f > 0.46f && f < 0.60f && (y % 3 == 0)) c = HEX(0xeef2c8);
            im.set(x, y, c);
        }
    im.ellipse(cx, 5.f, 3.2f, 1.3f, LT);  // the flat crown
    softRim(im, HEX(0x1b3a28), DK);
    im.set(8, 3, HEX(0xf2c94c)); im.set(9, 3, HEX(0xf2c94c)); im.set(8, 4, HEX(0xf2c94c));
    im.set(7, 3, HEX(0xe8584f)); im.set(10, 3, HEX(0xe8584f)); im.set(8, 2, HEX(0xe8584f));
    return im;
}
inline Img mkPalm(Rng& r) {
    Img im(40, 52);
    for (int i = 0; i < 36; i++) {  // curved trunk
        float t = i / 35.f;
        int x = (int)(20 - 4 * sinf(t * 2.2f)), y = 50 - i;
        im.rect(x - 1, y, 3, 1, (i % 4 == 0) ? HEX(0x6b4a2a) : HEX(0x8a6238));
        im.set(x + 1, y, HEX(0x5a3d22));
    }
    int cx = (int)(20 - 4 * sinf(2.2f)), cy = 14;
    for (int k = 0; k < 8; k++) {  // fronds
        float a = k * 0.785f + 0.2f;
        for (int i = 0; i < 16; i++) {
            float t = i / 15.f;
            int x = cx + (int)(cosf(a) * i * 1.1f), y = cy + (int)(sinf(a) * i * 0.55f + t * t * 9.f);
            u32 c = (i % 3 == 0) ? HEX(0x3a8a3e) : HEX(0x58a94b);
            im.set(x, y, c);
            im.set(x, y + 1, HEX(0x2f7a3a));
        }
    }
    im.ellipse((float)cx, (float)cy, 2, 2, HEX(0x6b4a2a));
    for (int i = 0; i < 3; i++) im.set(cx - 1 + i, cy + 2, HEX(0xb48a3c));  // dates
    im.outline(OUT);
    (void)r;
    return im;
}
inline Img mkColumn() {
    Img im(16, 36);
    u32 lt = HEX(0xe3cf9c), md = HEX(0xc8b07a), dk = HEX(0xa28a58);
    im.rect(4, 6, 8, 29, md);
    im.rect(4, 6, 2, 29, lt);
    im.rect(10, 6, 2, 29, dk);
    im.rect(7, 8, 1, 25, dk);  // fluting
    im.rect(2, 32, 12, 3, md);
    im.rect(2, 32, 12, 1, lt);
    for (int x = 4; x < 12; x++) im.set(x, 5 - (x % 3 == 0 ? 2 : (x % 3 == 1 ? 0 : 1)), md);  // broken top
    im.set(5, 4, lt), im.set(9, 3, dk);
    im.set(6, 20, dk), im.set(7, 21, dk), im.set(8, 22, dk);  // crack
    im.outline(OUT);
    return im;
}
inline Img mkRuinWall(Rng& r) {
    Img im(52, 22);
    u32 lt = HEX(0xe3cf9c), md = HEX(0xc8b07a), dk = HEX(0xa28a58);
    im.rect(2, 8, 48, 12, md);
    im.rect(2, 8, 48, 2, lt);
    im.rect(2, 18, 48, 2, dk);
    for (int y = 11; y < 18; y += 3)
        for (int x = (y % 2 ? 6 : 2); x < 50; x += 10) im.rect(x, y, 1, 3, dk);  // brick joints
    for (int x = 2; x < 50; x++) {  // ragged top
        int h = 3 + (int)(sinf(x * 0.5f) * 2) + (r.chance(0.2f) ? 2 : 0);
        for (int y = 8 - h; y < 8; y++) im.set(x, y, (x % 7 < 3) ? md : lt);
    }
    im.rect(22, 8, 6, 12, 0);  // doorway gap on the wall face
    im.rect(21, 8, 1, 12, dk);
    im.rect(28, 8, 1, 12, dk);
    im.outline(OUT);
    return im;
}
inline Img mkArch() {
    Img im(56, 44);
    u32 lt = HEX(0xe3cf9c), md = HEX(0xc8b07a), dk = HEX(0xa28a58);
    for (int side = 0; side < 2; side++) {
        int x0 = side ? 40 : 4;
        im.rect(x0, 14, 10, 28, md);
        im.rect(x0, 14, 3, 28, lt);
        im.rect(x0 + 8, 14, 2, 28, dk);
        im.rect(x0 - 1, 40, 12, 3, dk);
    }
    for (int x = 4; x < 52; x++) {  // arch span
        float t = (x - 28) / 24.f;
        int top = (int)(4 + t * t * 14);
        for (int y = top; y < top + 7; y++) im.set(x, y, y < top + 2 ? lt : (y > top + 4 ? dk : md));
    }
    im.rect(14, 12, 28, 30, 0);
    // clear the inside again (re-draw the span over it)
    for (int x = 14; x < 42; x++) {
        float t = (x - 28) / 24.f;
        int top = (int)(4 + t * t * 14);
        for (int y = top; y < top + 7; y++) im.set(x, y, y < top + 2 ? lt : (y > top + 4 ? dk : md));
    }
    im.outline(OUT);
    return im;
}

inline Img mkDeadTree(int v, Rng& r) {
    Img im(30, 44);
    u32 lt = HEX(0x6a5a46), md = HEX(0x4d3f2e), dk = HEX(0x33291e);
    int cx = 15 + (v ? 1 : -1);
    for (int y = 20; y < 42; y++) {
        int w = 3 + (y > 36 ? (y - 36) : 0);
        for (int x = cx - w / 2; x <= cx + w / 2; x++) im.set(x, y, x < cx ? lt : (x > cx ? dk : md));
    }
    struct B {
        int x0, y0, x1, y1;
    } br[] = {{cx, 24, cx - 9, 10}, {cx, 20, cx + 8, 6}, {cx - 5, 15, cx - 12, 14}, {cx + 4, 12, cx + 11, 14}, {cx, 20, cx + 1, 3}};
    for (auto& b : br) {
        im.line(b.x0, b.y0, b.x1, b.y1, md);
        im.line(b.x0 + 1, b.y0, b.x1 + 1, b.y1, dk);
    }
    for (int i = 0; i < 9; i++) {  // hanging moss
        int x = r.irange(cx - 10, cx + 10), y = r.irange(8, 22);
        if (cA(im.get(x, y))) {
            im.set(x, y + 1, HEX(0x7c9a5a));
            im.set(x, y + 2, HEX(0x6a8850));
            if (r.chance(0.5f)) im.set(x, y + 3, HEX(0x5a7a44));
        }
    }
    im.outline(OUT);
    return im;
}
inline Img mkLog() {
    Img im(28, 12);
    im.rect(2, 3, 24, 7, HEX(0x6b4f33));
    im.rect(2, 3, 24, 2, HEX(0x866444));
    im.rect(2, 8, 24, 2, HEX(0x4d3822));
    im.ellipse(3, 6.5f, 2, 3.5f, HEX(0xb08a5a));
    im.ellipse(3, 6.5f, 1, 2, HEX(0x8a6a40));
    for (int x = 8; x < 24; x += 5) im.set(x, 6, HEX(0x4d3822));
    for (int x = 10; x < 22; x++) im.set(x, 3, HEX(0x6f9a4a));  // moss
    im.outline(OUT);
    return im;
}
inline Img mkStiltHut() {
    Img im(64, 60);
    u32 wood = HEX(0x7a5a38), wd = HEX(0x5a4128), wl = HEX(0x987248), thatch = HEX(0xb59a52), thd = HEX(0x8a7238);
    for (int x : {8, 22, 40, 54}) {  // stilts
        im.rect(x, 36, 3, 22, wd);
        im.rect(x, 36, 1, 22, wood);
    }
    for (int y = 49; y < 52; y++)  // waterline
        for (int x = 4; x < 60; x++) if ((x + y) % 3 == 0) im.set(x, y, HEX(0x6f9a68));
    im.rect(4, 32, 56, 5, wood);  // platform
    im.rect(4, 32, 56, 1, wl);
    im.rect(4, 36, 56, 1, wd);
    im.rect(10, 16, 44, 16, wood);  // cabin
    for (int x = 10; x < 54; x += 4) im.rect(x, 16, 1, 16, wd);
    im.rect(10, 16, 44, 1, wl);
    im.rect(27, 20, 10, 12, HEX(0x2a1d12));  // door
    im.rect(27, 20, 10, 1, wd);
    im.rect(14, 20, 7, 6, HEX(0xf2c94c));  // lit window
    im.rect(14, 23, 7, 1, wd);
    im.rect(17, 20, 1, 6, wd);
    for (int y = 0; y < 17; y++) {  // thatched roof
        int hw = 6 + y * 2;
        for (int x = 32 - hw; x <= 32 + hw; x++) im.set(x, 2 + y, (y % 4 == 3) ? thd : (x < 32 - hw + 3 ? thatch : (x > 32 + hw - 3 ? thd : thatch)));
    }
    im.rect(46, 24, 3, 8, wd);  // ladder
    for (int y = 26; y < 34; y += 2) im.rect(44, y, 7, 1, wood);
    im.outline(OUT);
    return im;
}
inline Img mkWitchTree() {
    Img im(72, 76);
    u32 lt = HEX(0x6e5b44), md = HEX(0x4e3f2e), dk = HEX(0x2f251a);
    for (int y = 30; y < 72; y++) {
        int w = 10 + (y > 60 ? (y - 60) * 2 : 0) + (int)(sinf(y * 0.3f) * 1.5f);
        for (int x = 36 - w / 2; x <= 36 + w / 2; x++) im.set(x, y, x < 36 - w / 6 ? lt : (x > 36 + w / 6 ? dk : md));
    }
    im.rect(33, 44, 6, 9, HEX(0x1a120c));  // hollow
    struct B {
        int x0, y0, x1, y1;
    } br[] = {{36, 36, 8, 14}, {36, 34, 64, 12}, {36, 40, 4, 38}, {36, 40, 68, 36}, {36, 30, 24, 4}, {36, 30, 50, 2}, {20, 24, 6, 26}, {52, 22, 66, 24}};
    for (auto& b : br) {
        for (int t = 0; t < 3; t++) im.line(b.x0 + t, b.y0, b.x1 + t, b.y1, t == 0 ? lt : (t == 1 ? md : dk));
    }
    for (int i = 0; i < 26; i++) {  // dripping moss
        int x = 6 + (i * 5) % 60, y = 12 + (i * 7) % 28;
        if (cA(im.get(x, y))) {
            for (int k = 1; k < 4 + i % 4; k++) im.set(x, y + k, k % 2 ? HEX(0x7c9a5a) : HEX(0x5a7a44));
        }
    }
    for (int x : {8, 66}) {  // hanging charms
        im.line(x, 14, x, 22, HEX(0xcfc7b0));
        im.rect(x - 1, 22, 3, 3, HEX(0xe8e0c8));
    }
    im.set(35, 48, HEX(0xf2c94c));  // eyes in the hollow
    im.set(38, 48, HEX(0xf2c94c));
    im.outline(OUT);
    return im;
}
inline Img mkGrave() {
    Img im(16, 20);
    im.rect(3, 6, 10, 12, HEX(0x8d929e));
    im.ellipse(8, 7, 5, 4, HEX(0x8d929e));
    im.rect(3, 6, 2, 12, HEX(0xb0b4be));
    im.rect(11, 6, 2, 12, HEX(0x646a78));
    im.rect(7, 8, 2, 6, HEX(0x4d525e));  // cross
    im.rect(5, 10, 6, 2, HEX(0x4d525e));
    for (int x = 3; x < 13; x++) im.set(x, 17, HEX(0x6f9a4a));
    im.set(4, 12, HEX(0x6f9a4a));
    im.set(12, 9, HEX(0x6f9a4a));
    im.outline(OUT);
    return im;
}

inline Img mkSnowPine(Rng& r) {
    Img im(28, 46);
    im.rect(12, 36, 4, 8, HEX(0x6b4423));
    im.rect(14, 36, 2, 8, HEX(0x52341a));
    u32 dk = HEX(0x1d4a3a), md = HEX(0x2b6650), lt = HEX(0x3f8268);
    struct T {
        int top, bot, hw;
    } tiers[3] = {{2, 17, 8}, {10, 28, 10}, {20, 38, 12}};
    for (auto& t : tiers)
        for (int y = t.top; y <= t.bot; y++) {
            float k = (float)(y - t.top) / (t.bot - t.top);
            int hw = (int)(1 + k * t.hw);
            for (int x = 14 - hw; x <= 13 + hw; x++) {
                u32 c = md;
                if (x < 14 - hw + 2) c = lt;
                if (x > 13 + hw - 3) c = dk;
                if (y >= t.bot - 1) c = dk;
                if (k < 0.55f && x > 14 - hw + 1 && x < 13 + hw - 1) c = (x < 14) ? HEX(0xffffff) : HEX(0xdde8f2);  // snow on the top of each tier
                im.set(x, y, c);
            }
        }
    for (int i = 0; i < 10; i++) {
        int x = r.irange(6, 22), y = r.irange(6, 36);
        if (cA(im.get(x, y)) && im.get(x, y) != HEX(0xffffff)) im.set(x, y, HEX(0xf2f7fb));
    }
    im.outline(OUT);
    return im;
}
inline Img mkBareTree(Rng& r) {
    Img im(28, 40);
    u32 lt = HEX(0x8a7a68), md = HEX(0x6a5a48), dk = HEX(0x463a2e);
    for (int y = 16; y < 38; y++)
        for (int x = 12; x <= 15; x++) im.set(x, y, x == 12 ? lt : (x == 15 ? dk : md));
    struct B {
        int x0, y0, x1, y1;
    } br[] = {{13, 22, 4, 10}, {14, 18, 24, 6}, {13, 14, 13, 2}, {13, 26, 22, 20}, {13, 28, 5, 24}};
    for (auto& b : br) im.line(b.x0, b.y0, b.x1, b.y1, md);
    for (auto& b : br) {  // snow along the branch tops
        for (int i = 0; i < 5; i++) {
            int x = b.x0 + (b.x1 - b.x0) * i / 5, y = b.y0 + (b.y1 - b.y0) * i / 5 - 1;
            im.set(x, y, HEX(0xffffff));
        }
        im.set(b.x1, b.y1 - 1, HEX(0xffffff));
    }
    im.rect(11, 36, 6, 2, HEX(0xe6eef5));
    (void)r;
    im.outline(OUT);
    return im;
}
inline Img mkIceHut() {
    Img im(56, 48);
    im.ellipse(28, 34, 24, 13, HEX(0xdce8f2));  // dome of ice blocks
    im.ellipseShaded(28, 28, 22, 17, HEX(0xf6fbff), HEX(0xd8e8f4), HEX(0xa8c4dc));
    for (int y = 14; y < 44; y += 5)
        for (int x = 7; x < 50; x++) {
            if (cA(im.get(x, y)) && hypotf(x - 28.f, (y - 28.f) * 1.25f) < 21) im.set(x, y, HEX(0xa8c4dc));
        }
    for (int y = 17; y < 44; y += 5)
        for (int x = (y % 2 ? 12 : 17); x < 50; x += 10)
            if (cA(im.get(x, y)) && hypotf(x - 28.f, (y - 28.f) * 1.25f) < 21) im.rect(x, y, 1, 5, HEX(0xa8c4dc));
    im.rect(23, 30, 10, 14, HEX(0x1d2a38));  // doorway
    im.ellipse(28, 30, 5, 3, HEX(0x1d2a38));
    im.rect(40, 6, 5, 10, HEX(0x7d7a80));  // chimney with a little smoke
    im.rect(40, 6, 2, 10, HEX(0xa5a2a8));
    im.set(42, 3, HEX(0xdcdce0));
    im.set(43, 1, HEX(0xdcdce0));
    im.outline(HEX(0x2a4a63));
    return im;
}
inline Img mkCairn() {
    Img im(18, 24);
    int w[5] = {12, 10, 8, 6, 3}, y = 22;
    for (int i = 0; i < 5; i++) {
        int hgt = 4 + (i == 4 ? 0 : 1);
        im.ellipseShaded(9, (float)y - hgt / 2.f, w[i] / 2.f + 0.5f, hgt / 2.f + 1.f, i % 2 ? HEX(0xc2c5ce) : HEX(0xb0b3bd), HEX(0x8d929e), HEX(0x646a78));
        y -= hgt;
    }
    im.set(8, 2, HEX(0xe8584f));  // a tied scrap of red cloth
    im.set(9, 2, HEX(0xe8584f));
    im.set(10, 3, HEX(0xe8584f));
    im.outline(OUT);
    return im;
}
inline Img mkCave() {
    Img im(52, 40);
    u32 lt = HEX(0x7b7e92), md = HEX(0x5a5c6e), dk = HEX(0x3f4150);
    im.rect(0, 0, 52, 40, md);
    for (int y = 0; y < 40; y += 6) im.rect(0, y, 52, 1, dk);
    for (int x = 0; x < 52; x += 7)
        for (int y = 0; y < 40; y += 6) im.set(x + (y % 12 ? 3 : 0), y + 2, lt);
    im.ellipse(26, 40, 17, 28, HEX(0x1a1620));  // the mouth
    im.ellipse(26, 40, 14, 24, HEX(0x0d0a12));
    for (int x = 8; x < 44; x++) {  // frame stones
        int y = (int)(40 - 29 * sqrtf(std::max(0.f, 1.f - (x - 26.f) * (x - 26.f) / (17.f * 17.f)))) - 1;
        if (y >= 0 && y < 40) im.set(x, y, lt);
    }
    im.rect(20, 34, 12, 6, HEX(0x0d0a12));
    im.set(26, 30, HEX(0xf2c94c));  // eyes? no - a distant ember
    im.outline(OUT);
    // drop the ember pixel's outline artefacts
    return im;
}
inline Img mkStandingStone() {
    Img im(16, 36);
    im.rect(4, 4, 8, 30, HEX(0x7d808d));
    im.rect(4, 4, 2, 30, HEX(0xa9acb8));
    im.rect(10, 4, 2, 30, HEX(0x575a67));
    for (int x = 4; x < 12; x++) im.set(x, 3 - (x % 3 == 0 ? 1 : 0), HEX(0x7d808d));
    u32 rune = HEX(0x7fd6e8);
    im.rect(7, 12, 2, 1, rune);
    im.rect(7, 14, 1, 4, rune);
    im.rect(7, 20, 2, 1, rune);
    im.set(9, 18, rune);
    im.set(8, 23, rune);
    im.rect(2, 32, 12, 3, HEX(0x575a67));
    im.outline(OUT);
    return im;
}
inline Img mkBiomeSign(const char* label, u32 accent) {
    Img im(36, 34);
    im.rect(16, 14, 4, 19, HEX(0x6b4423));
    im.rect(16, 14, 1, 19, HEX(0x8a5e30));
    im.rect(3, 4, 30, 12, HEX(0x8a5e30));
    im.rect(3, 4, 30, 2, accent);
    im.rect(3, 15, 30, 1, HEX(0x5a3a1c));
    int w = (int)strlen(label) * 4 - 1;
    im.text(18 - w / 2, 8, label, HEX(0xf3e9d2));
    im.outline(OUT);
    return im;
}

// ================================================================= decor
// A burst of twigs from a dark root clump: n twigs fanned over the upper half-circle, each a little kinked, dark at the root and
// lighter at the tip (pal = root, base, mid, tip). Used by the desert's dead bush and the snowfield's bare shrubs.
inline void twigBurst(Img& im, const u32 pal[4], float bx, float by, int n, float lenBase, float lenVar, Rng& r) {
    im.ellipse(bx, by - 0.5f, 3.4f, 1.7f, pal[0]);
    for (int i = 0; i < n; i++) {
        const float a = -1.45f + 2.9f * i / (float)(n - 1) + r.range(-0.07f, 0.07f);
        const float len = (lenBase + lenVar * (1.f - fabsf(a) / 1.6f)) * r.range(0.85f, 1.1f);
        const float kink = r.range(-0.35f, 0.35f);
        float x = bx, y = by - 1.f;
        const int steps = (int)(len * 2.f);
        for (int s = 0; s < steps; s++) {
            const float t = (float)s / steps, ang = a + kink * t;
            x += sinf(ang) * 0.5f;
            y -= cosf(ang) * 0.5f * 0.85f;
            const int px = (int)lroundf(x), py = (int)lroundf(y);
            im.set(px, py, t < 0.35f ? pal[1] : t < 0.75f ? pal[2] : pal[3]);
            if (t < 0.40f) im.set(px + 1, py, t < 0.2f ? pal[0] : pal[1]);
        }
    }
}
inline Img mkDryBush(Rng& r) {  // a dead, spiky burst of twigs from a dark root clump
    Img im(18, 14);
    const u32 pal[4] = {HEX(0x2e1e14), HEX(0x4d3622), HEX(0x6f4f2f), HEX(0x93703f)};
    twigBurst(im, pal, 9.f, 12.f, 15, 5.5f, 6.f, r);
    return im;
}
inline Img mkSkull() {
    Img im(10, 8);
    im.ellipse(5, 3.5f, 3, 2.6f, HEX(0xe8e0c8));
    im.rect(3, 5, 4, 2, HEX(0xe8e0c8));
    im.set(4, 3, HEX(0x3a2f24));
    im.set(6, 3, HEX(0x3a2f24));
    im.set(5, 5, HEX(0x8d8470));
    im.line(1, 7, 3, 6, HEX(0xd4ccb4));  // a rib
    return im;
}
inline Img mkAgave(Rng& r) {  // a rosette of pointed blue-green leaves with a tall pink bloom (replaces the sand tuft)
    Img im(22, 22);
    const u32 DK = HEX(0x2f5f4a), MD = HEX(0x4f8f6a), LT = HEX(0x7fbf8a), HL = HEX(0xb5e0a8);
    const float cx = 11.f, cy = 16.f;
    struct Lf { float a; };
    float ang[11];
    for (int k = 0; k < 11; k++) ang[k] = 6.2832f * k / 11.f + r.range(-0.12f, 0.12f);
    std::sort(ang, ang + 11, [](float p, float q) { return sinf(p) < sinf(q); });  // far (upper) leaves first, near ones over them
    for (int k = 0; k < 11; k++) {
        const float a = ang[k], L = 8.8f * r.range(0.86f, 1.04f);
        const float dx = cosf(a), dy = sinf(a) * 0.55f, dl = sqrtf(dx * dx + dy * dy);
        const float ux = dx / dl, uy = dy / dl, px = -uy, py = ux;
        const float lit = -ux * 0.5f - uy * 0.85f;
        const u32 body = lit > -0.05f ? LT : lit > -0.55f ? MD : DK, edgeC = body == LT ? MD : DK, rib = body == DK ? MD : HL;
        const int n = (int)(L * 2.4f);
        for (int s = 1; s <= n; s++) {
            const float t = (float)s / n, w = 2.5f * powf(1.f - t, 0.85f) * (t < 0.15f ? t / 0.15f : 1.f);
            const float x = cx + dx * L * t, y = cy + dy * L * t;
            for (float o = -w; o <= w + 0.01f; o += 0.5f) {
                const bool edge = fabsf(o) > w - 0.55f && t > 0.12f;
                im.set((int)lroundf(x + px * o), (int)lroundf(y + py * o), edge ? edgeC : (fabsf(o) < 0.6f && t > 0.15f && t < 0.9f) ? rib : body);
            }
        }
    }
    im.ellipse(cx, cy, 2.f, 1.4f, DK);
    softRim(im, HEX(0x1f3f33), DK);
    // the bloom: a thin stalk and a pink spike with a darker rim
    for (int y = 9; y <= 15; y++) im.set(11, y, MD);
    im.ellipse(11.f, 5.5f, 2.6f, 4.4f, HEX(0x7a2a52));
    im.ellipse(11.f, 5.5f, 1.7f, 3.6f, HEX(0xd8608a));
    for (int y = 3; y <= 7; y++) im.set(10, y, HEX(0xf6a0c4));
    for (int y = 5; y <= 8; y++) im.set(12, y, HEX(0xa8386a));
    im.set(8, 8, HEX(0xd8608a)); im.set(14, 7, HEX(0xd8608a)); im.set(8, 9, HEX(0x7a2a52)); im.set(14, 8, HEX(0x7a2a52));
    return im;
}
// A fuzzy, spiky-edged shrub seen from slightly above, in the same soft no-black-outline style as the meadow ground:
// an ellipse silhouette with leaf points on its rim, lit from the top-left, flecked with small leaf ticks that point away
// from the middle, a soft tinted rim, a flat dark base and a short grass skirt. Every biome's bush is this body in its own
// palette (spikes/amp set how pointed the rim is). 24x19, sitting on row 16.
struct BushPal { u32 rim, dk, md, lt, hl, sk; };
inline Img fuzzyShrub(const BushPal& P, int spikes, float amp, float rx, float ry, int ticks, Rng& r) {
    constexpr int W = 24, H = 19, BASE = 16;
    Img im(W, H);
    const float cx = 11.5f, cy = 9.5f;
    const float ph = r.range(0.f, 6.28f);
    bool in[W * H] = {};
    float nx[W * H] = {}, ny[W * H] = {};
    for (int y = 0; y <= BASE; y++)
        for (int x = 0; x < W; x++) {
            const float dx = (x + .5f - cx) / rx, dy = (y + .5f - cy) / ry, d = sqrtf(dx * dx + dy * dy), th = atan2f(dy, dx);
            const float tip = 1.f - fabsf(sinf(th * spikes * 0.5f + ph));  // 1 at a leaf point, 0 in the notch between two
            const float R = 0.82f + amp * powf(tip, 1.3f) + 0.035f * sinf(th * 3.f + ph * 2.f);
            if (d <= R) { in[y * W + x] = true; nx[y * W + x] = dx; ny[y * W + x] = dy; }
        }
    auto at = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && in[y * W + x]; };
    auto up = [&](u32 c) { return c == P.dk ? P.md : c == P.md ? P.lt : P.hl; };
    auto dn = [&](u32 c) { return c == P.hl ? P.lt : c == P.lt ? P.md : P.dk; };
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (!in[y * W + x]) continue;
            const float s = -nx[y * W + x] * 0.5f - ny[y * W + x] * 0.85f;
            im.set(x, y, s < -0.22f ? P.dk : s < 0.40f ? P.md : s < 0.78f ? P.lt : P.hl);
        }
    for (int i = 0; i < ticks; i++) {  // leaf ticks: two pixels pointing outward, lighter on the lit side, darker on the shaded side
        const int x = r.irange(2, W - 3), y = r.irange(2, BASE - 1);
        if (!at(x, y)) continue;
        float dx = x + .5f - cx, dy = (y + .5f - cy) * (rx / ry);
        const float l = sqrtf(dx * dx + dy * dy) + 1e-3f;
        const int x2 = x + (int)lroundf(dx / l), y2 = y + (int)lroundf(dy / l);
        if (!at(x2, y2) || (x2 == x && y2 == y)) continue;
        const u32 c = im.get(x, y);
        const bool lit = (-dx * 0.5f - dy * 0.85f) > 0.1f;
        const u32 t = lit ? up(c) : dn(c);
        im.set(x, y, t);
        if (r.chance(0.6f)) im.set(x2, y2, t);
    }
    for (int y = 0; y < H; y++)  // soft tinted rim instead of a black outline: dark underneath and at the sides, a mid green along the lit top
        for (int x = 0; x < W; x++) {
            if (!at(x, y)) continue;
            if (at(x + 1, y) && at(x - 1, y) && at(x, y + 1) && at(x, y - 1)) continue;
            im.set(x, y, ny[y * W + x] > -0.25f ? P.rim : P.dk);
        }
    for (int x = 0; x < W; x++)
        if (at(x, BASE)) im.set(x, BASE, P.rim);
    for (int x = 2; x < W - 2; x++)
        if (at(x, BASE) && r.chance(0.45f)) im.set(x, BASE + 1, r.chance(0.7f) ? P.sk : P.dk);
    return im;
}
// Swamp bush, variant 0: an olive moss mound -- round and soft, with pale lichen and a few dark pits.
inline Img mkMossBush(Rng& r) {
    const BushPal P{HEX(0x1e2a12), HEX(0x4a5a1e), HEX(0x6e8030), HEX(0x98a848), HEX(0xc8cf6a), HEX(0x8a9a3a)};
    Img im = fuzzyShrub(P, 15, 0.12f, 11.f, 7.4f, 84, r);
    for (int i = 0; i < 60; i++) {
        const int x = r.irange(3, 20), y = r.irange(3, 14);
        const u32 c = im.get(x, y);
        if (c == P.md && r.chance(0.35f)) im.set(x, y, P.dk);
        else if (c == P.lt && r.chance(0.45f)) im.set(x, y, HEX(0xdcdc98));  // lichen
    }
    return im;
}
// Swamp bush, variant 1: a dark fern -- feathery fronds radiating from a dark crown.
inline Img mkSwampFern(Rng& r) {
    Img im(24, 20);
    const u32 DK = HEX(0x1d4a26), MD = HEX(0x4f9f3e), LT = HEX(0x86c850), HL = HEX(0xc8ee80);  // yellower than the teal swamp floor, so it stands off it
    const float cx = 12.f, cy = 13.f;
    constexpr int N = 8;
    float ang[N];
    for (int k = 0; k < N; k++) ang[k] = 6.2832f * k / N + r.range(-0.15f, 0.15f);
    std::sort(ang, ang + N, [](float p, float q) { return sinf(p) < sinf(q); });  // far fronds first, near ones over them
    for (int k = 0; k < N; k++) {
        const float a = ang[k], L = 10.8f * r.range(0.85f, 1.05f);
        float ux = cosf(a), uy = sinf(a) * 0.6f;
        const float ul = sqrtf(ux * ux + uy * uy);
        ux /= ul; uy /= ul;
        const float px = -uy, py = ux, lit = -ux * 0.5f - uy * 0.85f;
        const u32 rib = lit > 0.05f ? LT : MD, pin = lit > 0.05f ? LT : lit > -0.5f ? MD : DK;
        const int n = (int)(L * 2.f);
        for (int s = 1; s <= n; s++) {
            const float t = (float)s / n;
            const float x = cx + cosf(a) * L * t, y = cy + sinf(a) * 0.6f * L * t + t * t * 1.6f;
            im.set((int)lroundf(x), (int)lroundf(y), rib);
            if (s % 3 == 0 && t > 0.12f) {  // a pair of pinnae angled forward
                const float pl = 4.6f * powf(1.f - t, 0.7f) + 0.9f;
                for (int sd = -1; sd <= 1; sd += 2) {
                    const float ex = x + (px * sd * 0.85f + ux * 0.55f) * pl, ey = y + (py * sd * 0.85f + uy * 0.55f) * pl;
                    im.line((int)lroundf(x), (int)lroundf(y), (int)lroundf(ex), (int)lroundf(ey), (s % 6 == 0) ? pin : (pin == LT ? MD : pin));
                }
            }
        }
        im.set((int)lroundf(cx + cosf(a) * L), (int)lroundf(cy + sinf(a) * 0.6f * L + 1.6f), lit > 0.05f ? HL : LT);
    }
    im.ellipse(cx, cy, 2.2f, 1.5f, DK);
    softRim(im, HEX(0x12301a), DK);
    return im;
}
// Snowfield bush, variant 0: a bare twig shrub with a heap of snow banked round its foot -- the twigs fan out above it, frosted on top.
// With snowy == false the same shrub after it has shaken its snow off: the same twigs (the random stream is used the same way), no frost, and only a thin smear of snow at the root.
inline Img mkSnowBush(Rng& r, bool snowy = true) {
    Img im(24, 19);
    const u32 pal[4] = {HEX(0x2a2018), HEX(0x4a3a2c), HEX(0x6e5a46), HEX(0x9a866a)};
    twigBurst(im, pal, 11.5f, 16.f, 17, 9.f, 8.f, r);
    const Img twigs = im;  // frost on the upward-facing side of the twigs
    for (int y = 1; y < im.h; y++)
        for (int x = 0; x < im.w; x++)
            if ((twigs.get(x, y) >> 24) != 0 && (twigs.get(x, y - 1) >> 24) == 0 && r.chance(0.3f) && snowy) im.set(x, y, HEX(0xeef4fc));
    const u32 RIM = HEX(0x6f84ac), LT = HEX(0xf8fcff), MD = HEX(0xdbe7f6), DK = HEX(0xa5b8d6);
    const float lump[5][3] = {{11.5f, 13.2f, 3.0f}, {8.f, 14.4f, 2.6f}, {15.2f, 14.2f, 2.8f}, {5.f, 15.4f, 1.9f}, {18.5f, 15.4f, 2.0f}};
    for (const auto& l : lump) {  // back to front: a ring one pixel wider cuts each lump out from the ones behind it
        const float cx = l[0] + r.range(-0.3f, 0.3f), cy = l[1] + r.range(-0.2f, 0.2f), rx = l[2], ry = l[2] * 0.8f;
        if (!snowy) continue;
        im.ellipse(cx, cy, rx + 0.9f, ry + 0.9f, RIM);
        im.ellipseShaded(cx, cy, rx, ry, LT, MD, DK);
    }
    if (snowy) {
        im.ellipse(11.5f, 15.8f, 6.6f, 1.5f, RIM);
        im.ellipseShaded(11.5f, 15.8f, 5.8f, 1.0f, MD, MD, DK);
    } else {  // the little that is left: a thin pale smear round the root
        im.ellipse(11.5f, 15.6f, 4.6f, 1.2f, MD);
        im.ellipse(11.5f, 15.8f, 3.4f, 0.8f, DK);
    }
    for (int y = 17; y < im.h; y++) for (int x = 0; x < im.w; x++) im.set(x, y, 0u);
    return im;
}
// Snowfield bush, variant 1: a dead twig clump with a little frost on its upper sides and a snow drift round its root.
inline Img mkFrostTwigs(Rng& r, bool snowy = true) {
    Img im(18, 14);
    const u32 pal[4] = {HEX(0x2a2018), HEX(0x4a3a2c), HEX(0x6e5a46), HEX(0x9a866a)};
    im.ellipse(9.f, 11.8f, snowy ? 5.6f : 3.6f, snowy ? 1.8f : 1.1f, HEX(0xe6eef8));
    twigBurst(im, pal, 9.f, 12.f, 15, 5.5f, 6.f, r);
    const Img src = im;
    for (int y = 1; y < im.h; y++)
        for (int x = 0; x < im.w; x++)
            if ((src.get(x, y) >> 24) != 0 && (src.get(x, y - 1) >> 24) == 0 && r.chance(0.55f) && snowy) im.set(x, y, HEX(0xeef4fc));
    return im;
}
// Mountain bush, variant 0: a cool blue-green alpine cushion, dotted with small blue star flowers.
inline Img mkAlpineCushion(Rng& r) {
    const BushPal P{HEX(0x14302a), HEX(0x2f5a44), HEX(0x4a8060), HEX(0x6fa67c), HEX(0x9fcf98), HEX(0x4a8060)};
    Img im = fuzzyShrub(P, 17, 0.11f, 10.5f, 7.2f, 80, r);
    const u32 CEN = HEX(0xd8ecff), ARM = HEX(0x4f8fe8), ARM_DK = HEX(0x3a6cc0);
    int placed = 0;
    for (int tries = 0; tries < 80 && placed < 9; tries++) {
        const int x = r.irange(4, 19), y = r.irange(3, 13);
        const float dx = (x + .5f - 11.5f) / 10.5f, dy = (y + .5f - 9.5f) / 7.2f;
        if (dx * dx + dy * dy > 0.5f) continue;  // keep flowers well inside the body
        if ((im.get(x, y) >> 24) == 0) continue;
        im.set(x, y, CEN);  // a plus-shaped bloom: pale centre, blue petals
        const u32 a = (y > 9) ? ARM_DK : ARM;
        im.set(x - 1, y, a); im.set(x + 1, y, a); im.set(x, y - 1, a); im.set(x, y + 1, ARM_DK);
        placed++;
    }
    return im;
}
// Mountain bush, variant 1: a dead grey-brown twig clump wedged in a little pile of scree.
inline Img mkScreeTwigs(Rng& r) {
    Img im(18, 14);
    const u32 pal[4] = {HEX(0x241c16), HEX(0x3e3128), HEX(0x5c4a3c), HEX(0x84705a)};
    const u32 ST_DK = HEX(0x3c444a), ST_MD = HEX(0x6b777c), ST_LT = HEX(0x9aa6a6);
    for (int i = 0; i < 7; i++) {  // pebbles round the root
        const int x = 9 + r.irange(-6, 6), y = 11 + r.irange(0, 1);
        im.set(x, y, ST_MD); im.set(x + 1, y, ST_LT); im.set(x, y + 1, ST_DK); im.set(x + 1, y + 1, ST_DK);
    }
    twigBurst(im, pal, 9.f, 12.f, 17, 6.f, 6.5f, r);
    return im;
}
inline Img mkReeds(int v) {
    Img im(12, 18);
    u32 a = HEX(0x4f7a3a), b = HEX(0x78a24c);
    int xs[3] = {3, 6, 9}, hs[3] = {13, 17, 11};
    if (v) hs[0] = 16, hs[1] = 12, hs[2] = 15;
    for (int i = 0; i < 3; i++) {
        im.line(xs[i], 17, xs[i] + (i - 1), 17 - hs[i], i == 1 ? b : a);
        im.rect(xs[i] + (i - 1) - 1, 17 - hs[i] - 1, 2, 4, HEX(0x6b4423));  // cattail head
        im.set(xs[i] + (i - 1) - 1, 17 - hs[i] - 1, HEX(0x8a5e30));
    }
    return im;
}
inline Img mkLily() {
    Img im(10, 6);
    im.ellipse(5, 3, 4, 2.2f, HEX(0x5f9a48));
    im.set(5, 3, HEX(0x4a7a38));
    im.set(6, 2, HEX(0xf6a0c4));
    im.set(5, 2, HEX(0xffffff));
    return im;
}
inline Img mkBubbles() {
    Img im(10, 6);
    im.ellipse(5, 3, 4, 2, HEX(0x4f6a58));
    im.set(4, 2, HEX(0x9ac0a8));
    im.set(6, 3, HEX(0x9ac0a8));
    im.set(5, 1, HEX(0xc8e0d0));
    return im;
}
inline Img mkDrift() {
    Img im(14, 7);
    im.ellipse(7, 4.5f, 6, 2.6f, HEX(0xf6fafd));
    for (int x = 3; x < 12; x++) im.set(x, 6, HEX(0xc9d8e6));
    im.set(5, 3, HEX(0xffffff));
    return im;
}
inline Img mkFrostTuft() {
    Img im(8, 8);
    im.line(4, 7, 2, 2, HEX(0x6f9a9a));
    im.line(4, 7, 4, 1, HEX(0xa8d0d0));
    im.line(4, 7, 6, 3, HEX(0x6f9a9a));
    im.set(4, 0, HEX(0xffffff));
    im.set(2, 1, HEX(0xffffff));
    return im;
}
inline Img mkAlpine() {
    Img im(8, 8);
    im.rect(4, 4, 1, 4, HEX(0x4a7a3a));
    im.set(3, 3, HEX(0xa07be8));
    im.set(5, 3, HEX(0xa07be8));
    im.set(4, 2, HEX(0xa07be8));
    im.set(4, 3, HEX(0xf2f0ff));
    im.set(2, 6, HEX(0xa07be8));
    im.set(1, 7, HEX(0x4a7a3a));
    return im;
}
inline Img mkGravel() {
    Img im(10, 6);
    im.ellipse(3, 3.5f, 1.8f, 1.4f, HEX(0x9a9ca8));
    im.ellipse(6.5f, 3, 1.6f, 1.2f, HEX(0x74777f));
    im.ellipse(8, 4.5f, 1.2f, 1.0f, HEX(0xb0b2be));
    return im;
}

// ================================================================= harvestable resource nodes (16x16-ish)
inline Img mkNode(int kind) {
    Img im(18, 18);
    switch (kind) {
        case RN_CACTUS: {  // prickly pear with red fruit
            im.ellipseShaded(6, 11, 4, 5, HEX(0x72bb62), HEX(0x4a9a4c), HEX(0x2f7a3a));
            im.ellipseShaded(12, 8, 3.5f, 4.5f, HEX(0x72bb62), HEX(0x4a9a4c), HEX(0x2f7a3a));
            im.set(5, 6, HEX(0xe8446a)), im.set(12, 3, HEX(0xe8446a)), im.set(13, 4, HEX(0xe8446a));
            im.set(6, 9, HEX(0xe8f0c0)), im.set(11, 7, HEX(0xe8f0c0)), im.set(7, 13, HEX(0xe8f0c0));
            break;
        }
        case RN_DESERT_HERB: {  // sage
            for (int i = 0; i < 7; i++) im.line(9, 16, 3 + i * 2, 4 + (i % 3) * 2, i % 2 ? HEX(0x8aa886) : HEX(0xa8c2a0));
            im.set(9, 4, HEX(0xc8a0e0));
            im.set(5, 6, HEX(0xc8a0e0));
            im.set(13, 5, HEX(0xc8a0e0));
            break;
        }
        case RN_REED: {
            for (int i = 0; i < 4; i++) {
                int x = 4 + i * 3, h = 10 + (i % 2) * 3;
                im.line(x, 16, x + 1 - i % 3, 16 - h, HEX(0x78a24c));
                im.rect(x - (i % 3), 16 - h - 2, 2, 4, HEX(0x6b4423));
            }
            break;
        }
        case RN_BOGWORT: {
            im.ellipseShaded(9, 12, 6, 3.5f, HEX(0x78a24c), HEX(0x4f7a3a), HEX(0x2f5a2c));
            im.line(9, 12, 5, 4, HEX(0x4f7a3a));
            im.line(9, 12, 9, 2, HEX(0x4f7a3a));
            im.line(9, 12, 13, 5, HEX(0x4f7a3a));
            for (auto p : {std::pair<int, int>{5, 3}, {9, 1}, {13, 4}}) {
                im.ellipse((float)p.first, (float)p.second, 1.6f, 1.6f, HEX(0x8a4fc0));
                im.set(p.first, p.second - 1, HEX(0xc8a0e8));
            }
            break;
        }
        case RN_FROSTBLOOM: {
            im.line(9, 16, 9, 8, HEX(0x6f9a9a));
            im.line(9, 13, 6, 10, HEX(0x6f9a9a));
            u32 p = HEX(0xa8e4ff), q = HEX(0xe8faff);
            for (auto d : {std::pair<int, int>{0, -3}, {3, 0}, {-3, 0}, {0, 3}, {2, -2}, {-2, -2}, {2, 2}, {-2, 2}}) im.set(9 + d.first, 6 + d.second, p);
            im.rect(8, 5, 3, 3, q);
            im.set(9, 6, HEX(0xffffff));
            break;
        }
        case RN_ALPINE_HERB: {
            im.line(9, 16, 9, 7, HEX(0x4a7a3a));
            im.line(9, 14, 5, 10, HEX(0x4a7a3a));
            im.line(9, 12, 13, 8, HEX(0x4a7a3a));
            for (auto d : {std::pair<int, int>{9, 5}, {5, 8}, {13, 6}}) {
                im.rect(d.first - 1, d.second - 1, 3, 3, HEX(0xf2f0ff));
                im.set(d.first, d.second, HEX(0xf2c94c));
            }
            break;
        }
        default: {  // RN_ORE: a grey rock streaked with orange ore
            im.ellipseShaded(9, 10, 7, 6, HEX(0xa9acb8), HEX(0x7d808d), HEX(0x575a67));
            for (auto d : {std::pair<int, int>{6, 7}, {10, 6}, {8, 10}, {12, 10}, {5, 11}}) {
                im.set(d.first, d.second, HEX(0xf0953a));
                im.set(d.first + 1, d.second, HEX(0xf2c94c));
            }
            break;
        }
    }
    im.outline(OUT);
    return im;
}

// soft fog puff (alpha-blended, tinted at draw time with the colour mod)
inline Img mkFog() {
    Img im(112, 44);
    for (int y = 0; y < 44; y++)
        for (int x = 0; x < 112; x++) {
            float dx = (x + 0.5f - 56.f) / 56.f, dy = (y + 0.5f - 22.f) / 22.f;
            float d = sqrtf(dx * dx + dy * dy);
            float n = (float)(hash2(x / 4, y / 3, 77) & 255) / 255.f * 0.35f;  // lumpy edge
            float a = std::max(0.f, 1.f - d - n * d);
            im.set(x, y, RGBA(255, 255, 255, (int)(a * a * 120.f)));
        }
    return im;
}

// ================================================================= bank
struct BiomeAssets {
    Spr sand[3], dune[2], mud[2], murk[4], bog[4], snow[3], snowp[2], ice[2], stone[3], cliff[3], snowCliff[3], snowCliffCap[3], lookout, fishRack, firewood, tentOld[2], ashRing, fishHole, snowSign[4], snowLamp, snowFence, snowStump[3], snowman[4], snowFire[3], snowTent, snowTentSnowy, snowLog[3], frostCrack[4], tracks[4], buriedRock[4], leafyBirch[LEAFY_BIRCH_FRAMES], youngPine;
    Spr cactus[2], palm, column, ruinwall, arch;
    Spr deadtree[2], log, stilthut, witchtree, grave;
    Spr snowpine, baretree, crystal, icehut, cairn;
    Spr cave, boulder, standing, bsign[4];
    Spr swampBush[2], snowBush[2], snowBushBare[2], mountainBush[2], drybush, skull, sandtuft, reeds[2], lily, bubbles, drift, frosttuft, alpine, gravel;
    Spr node[RN_COUNT];
    Spr fog;
};

inline void buildBiomeAssets(SDL_Renderer* r, BiomeAssets& b, TileBlend* tb = nullptr) {
    Rng rg(7654321);
    for (int i = 0; i < 3; i++) {
        const Img im = mkSand(i, rg);
        b.sand[i] = makeSpr(r, im, 0, 0);
        if (tb) tb->src[BL_SAND][i] = im;
    }
    if (tb) tb->nvar[BL_SAND] = 3;
    for (int i = 0; i < 2; i++) {
        const Img im = mkDune(i, rg);
        b.dune[i] = makeSpr(r, im, 0, 0);
        if (tb) tb->src[BL_DUNE][i] = im;
    }
    if (tb) tb->nvar[BL_DUNE] = 2;
    for (int i = 0; i < 2; i++) {
        const Img im = mkMud(i, rg);
        b.mud[i] = makeSpr(r, im, 0, 0);
        if (tb) tb->src[BL_MUD][i] = im;
    }
    if (tb) tb->nvar[BL_MUD] = 2;
    for (int i = 0; i < 4; i++) b.murk[i] = makeSpr(r, mkMurk(i), 0, 0);
    for (int i = 0; i < 4; i++) b.bog[i] = makeSpr(r, mkBog(i), 0, 0);
    for (int i = 0; i < 3; i++) {
        Img im = mkSnow(i, rg);
        snowTexture(im, i, false);
        b.snow[i] = makeSpr(r, im, 0, 0);
        if (tb) tb->src[BL_SNOW][i] = im;
    }
    if (tb) tb->nvar[BL_SNOW] = 3;
    for (int i = 0; i < 2; i++) {
        Img im = mkSnowPacked(i, rg);
        snowTexture(im, i, true);
        b.snowp[i] = makeSpr(r, im, 0, 0);
        if (tb) tb->src[BL_SNOWP][i] = im;
    }
    if (tb) tb->nvar[BL_SNOWP] = 2;
    for (int i = 0; i < 2; i++) b.ice[i] = makeSpr(r, mkIce(i, rg), 0, 0);
    for (int i = 0; i < 3; i++) {
        const Img im = mkStone(i, rg);
        b.stone[i] = makeSpr(r, im, 0, 0);
        if (tb) tb->src[BL_STONE][i] = im;
    }
    {
        Rng sc(20241122);  // the snowfields' cliffs, with their own random numbers
        for (int i = 0; i < 3; i++) {
            b.snowCliff[i] = makeSpr(r, mkSnowCliff(i, false, sc), 0, 0);
            b.snowCliffCap[i] = makeSpr(r, mkSnowCliff(i, true, sc), 0, 0);
        }
        static const char* signText[4] = {"HUT", "LAKE", "CAIRN", "FORT"};
        for (int i = 0; i < 4; i++) b.snowSign[i] = makeSpr(r, mkSnowSign(signText[i]), 16, 24);
        b.snowLamp = makeSpr(r, mkSnowLampV(), 5, 35);
        b.snowFence = makeSpr(r, mkSnowFence(), 9, 16);
        for (int i = 0; i < 3; i++) b.snowStump[i] = makeSpr(r, mkSnowStumpV(i), 7, 8);
        for (int i = 0; i < 4; i++) b.snowman[i] = makeSpr(r, mkSnowmanV(i), 8, 23);
        for (int i = 0; i < 3; i++) b.snowFire[i] = makeSpr(r, mkSnowCampfire(i), 10, 18);
        b.snowTent = makeSpr(r, mkSnowTentArt(), 16, 29);
        b.snowTentSnowy = makeSpr(r, mkSnowTentSnowyArt(), 16, 29);
        for (int i = 0; i < 3; i++) b.snowLog[i] = makeSpr(r, mkSnowLogV(i), 10, 8);
        b.lookout = makeSpr(r, mkLookout(), 16, 62);
        b.fishRack = makeSpr(r, mkFishRack(), 13, 20);
        b.firewood = makeSpr(r, mkFirewood(), 10, 12);
        for (int i = 0; i < 2; i++) b.tentOld[i] = makeSpr(r, mkSnowTentOldArt(i), 16, 29);
        b.ashRing = makeSpr(r, mkAshRing(), 11, 8);
        b.fishHole = makeSpr(r, mkFishHole(), 11, 8);
        for (int i = 0; i < LEAFY_BIRCH_FRAMES; i++) b.leafyBirch[i] = makeSpr(r, mkLeafyBirchFrame(i), 33, 65);
        b.youngPine = makeSpr(r, mkYoungPine(), 9, 25);
        for (int i = 0; i < 4; i++) {
            b.frostCrack[i] = makeSpr(r, mkFrostCrackV(i), 9, 5);
            b.tracks[i] = makeSpr(r, mkTracksV(i), 10, 5);
            b.buriedRock[i] = makeSpr(r, mkBuriedRockV(i), 7, 8);
        }
    }
    if (tb) tb->nvar[BL_STONE] = 3;
    for (int i = 0; i < 3; i++) b.cliff[i] = makeSpr(r, mkCliff(i, rg), 0, 0);
    {
        Rng sc(20241122);  // the snowfields' cliffs, with their own random numbers
        for (int i = 0; i < 3; i++) {
            const Img bare = mkSnowCliff(i, false, sc), cap = mkSnowCliff(i, true, sc);
            b.snowCliff[i] = makeSpr(r, bare, 0, 0);
            b.snowCliffCap[i] = makeSpr(r, cap, 0, 0);
            if (tb) tb->cliffSrc[0][i] = bare, tb->cliffSrc[1][i] = cap;
        }
        static const char* signText[4] = {"HUT", "LAKE", "CAIRN", "FORT"};
        for (int i = 0; i < 4; i++) b.snowSign[i] = makeSpr(r, mkSnowSign(signText[i]), 16, 24);
        b.snowLamp = makeSpr(r, mkSnowLampV(), 5, 35);
        b.snowFence = makeSpr(r, mkSnowFence(), 9, 16);
        for (int i = 0; i < 3; i++) b.snowStump[i] = makeSpr(r, mkSnowStumpV(i), 7, 8);
        for (int i = 0; i < 4; i++) b.snowman[i] = makeSpr(r, mkSnowmanV(i), 8, 23);
        for (int i = 0; i < 3; i++) b.snowFire[i] = makeSpr(r, mkSnowCampfire(i), 10, 18);
        b.snowTent = makeSpr(r, mkSnowTentArt(), 16, 29);
        b.snowTentSnowy = makeSpr(r, mkSnowTentSnowyArt(), 16, 29);
        for (int i = 0; i < 3; i++) b.snowLog[i] = makeSpr(r, mkSnowLogV(i), 10, 8);
        for (int i = 0; i < LEAFY_BIRCH_FRAMES; i++) b.leafyBirch[i] = makeSpr(r, mkLeafyBirchFrame(i), 33, 65);
        b.youngPine = makeSpr(r, mkYoungPine(), 9, 25);
        for (int i = 0; i < 4; i++) {
            b.frostCrack[i] = makeSpr(r, mkFrostCrackV(i), 9, 5);
            b.tracks[i] = makeSpr(r, mkTracksV(i), 10, 5);
            b.buriedRock[i] = makeSpr(r, mkBuriedRockV(i), 7, 8);
        }
    }

    Rng dr(20241116);  // its own random stream, so the desert plants don't shift what the sprites after them get
    b.cactus[0] = makeSpr(r, mkPricklyPear(dr), 12, 24);
    b.cactus[1] = makeSpr(r, mkCactusBarrel(), 8, 14);
    b.palm = makeSpr(r, mkPalm(rg), 20, 50);
    b.column = makeSpr(r, mkColumn(), 8, 34);
    b.ruinwall = makeSpr(r, mkRuinWall(rg), 26, 20);
    b.arch = makeSpr(r, mkArch(), 28, 42);
    b.deadtree[0] = makeSpr(r, mkDeadTree(0, rg), 15, 42);
    b.deadtree[1] = makeSpr(r, mkDeadTree(1, rg), 15, 42);
    b.log = makeSpr(r, mkLog(), 14, 10);
    b.stilthut = makeSpr(r, mkStiltHut(), 32, 56);
    b.witchtree = makeSpr(r, mkWitchTree(), 36, 72);
    b.grave = makeSpr(r, mkGrave(), 8, 18);
    b.snowpine = makeSpr(r, mkSnowPine(rg), 14, 44);
    b.baretree = makeSpr(r, mkBareTree(rg), 14, 38);
    b.crystal = makeSpr(r, mkCrystal(), 14, 30);
    b.icehut = makeSpr(r, mkIceHut(), 28, 44);
    b.cairn = makeSpr(r, mkCairn(), 9, 22);
    b.cave = makeSpr(r, mkCave(), 26, 39);
    b.boulder = makeSpr(r, mkBoulder(), 15, 20);
    b.standing = makeSpr(r, mkStandingStone(), 8, 34);
    static const char* lbl[4] = {"DESERT", "SWAMP", "SNOW", "PEAKS"};
    for (int i = 0; i < 4; i++) b.bsign[i] = makeSpr(r, mkBiomeSign(lbl[i], BIOME[i == 0 ? B_DESERT : i == 1 ? B_SWAMP : i == 2 ? B_SNOW : B_MOUNTAIN].col), 18, 32);

    b.drybush = makeSpr(r, mkDryBush(dr), 9, 12);
    b.skull = makeSpr(r, mkSkull(), 5, 6);
    b.sandtuft = makeSpr(r, mkAgave(dr), 11, 19);
    Rng sr(20241117);  // the swamp's two bushes (the swamp reuses the bush decor kinds; the renderer picks these when you are in the swamp)
    b.swampBush[0] = makeSpr(r, mkMossBush(sr), 12, 16);
    b.swampBush[1] = makeSpr(r, mkSwampFern(sr), 12, 15);
    Rng wr(20241118);  // the snowfield's two bushes
    Rng wb = wr;       // (and the same two once they have shaken their snow off: the same twigs, drawn from the same stream)
    b.snowBush[0] = makeSpr(r, mkSnowBush(wr), 12, 16);
    b.snowBush[1] = makeSpr(r, mkFrostTwigs(wr), 9, 12);
    Rng mr(20241119);  // the mountains' two bushes
    b.mountainBush[0] = makeSpr(r, mkAlpineCushion(mr), 12, 16);
    b.mountainBush[1] = makeSpr(r, mkScreeTwigs(mr), 9, 12);
    b.reeds[0] = makeSpr(r, mkReeds(0), 6, 17);
    b.reeds[1] = makeSpr(r, mkReeds(1), 6, 17);
    b.lily = makeSpr(r, mkLily(), 5, 4);
    b.bubbles = makeSpr(r, mkBubbles(), 5, 4);
    b.drift = makeSpr(r, mkDrift(), 7, 5);
    b.frosttuft = makeSpr(r, mkFrostTuft(), 4, 7);
    b.alpine = makeSpr(r, mkAlpine(), 4, 7);
    b.gravel = makeSpr(r, mkGravel(), 5, 5);
    for (int k = 0; k < RN_COUNT; k++) b.node[k] = makeSpr(r, k < RN_HERB ? mkNode(k) : mkNodeNew(k), 9, 16);
    b.fog = makeSpr(r, mkFog(), 56, 22);
    b.snowBushBare[0] = makeSpr(r, mkSnowBush(wb, false), 12, 16);  // (last, so the pictures drawn after them in the stream do not change)
    b.snowBushBare[1] = makeSpr(r, mkFrostTwigs(wb, false), 9, 12);
}
