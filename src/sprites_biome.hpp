// sprites_biome.hpp - procedural pixel art for the desert, swamp, snow and mountain biomes.
// Same rules as sprites.hpp: everything is painted in code, nothing is loaded from disk.
#pragma once
#include "data.hpp"

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
    Img im = mkSpeckleTile(HEX(0xedf2f7), HEX(0xd8e2ec), HEX(0xffffff), 12 + v * 2, 10, r);
    for (int i = 0; i < 2; i++) {
        int x = r.irange(1, 12), y = r.irange(2, 14);
        im.set(x, y, HEX(0xc9d8e6));
        im.set(x + 1, y, HEX(0xc9d8e6));
    }
    return im;
}
inline Img mkSnowPacked(int v, Rng& r) {
    Img im = mkSpeckleTile(HEX(0xd5e0ea), HEX(0xc0cfdd), HEX(0xe6eef5), 14, 8, r);
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
// rock tinted with three colours, optional snow cap
inline Img mkRockC(int variant, u32 lt, u32 md, u32 dk, bool snow) {
    Img im(variant == 0 ? 20 : 26, variant == 0 ? 15 : 19);
    if (variant == 0) {
        im.ellipseShaded(9, 8, 8, 5.5f, lt, md, dk);
        im.ellipseShaded(15, 10, 4, 3, lt, md, dk);
        im.line(6, 5, 9, 8, dk);
    } else {
        im.ellipseShaded(13, 10, 11, 7, lt, md, dk);
        im.ellipseShaded(7, 13, 5, 3.5f, lt, md, dk);
        im.line(12, 7, 14, 11, dk);
    }
    if (snow) {
        for (int x = 2; x < im.w - 2; x++)
            for (int y = 2; y < 7; y++)
                if (cA(im.get(x, y)) && cA(im.get(x, y - 1)) == 0 && y < 8) {
                    im.set(x, y, HEX(0xf6fafd));
                    if (cA(im.get(x, y + 1))) im.set(x, y + 1, HEX(0xdde8f2));
                }
    }
    im.outline(OUT);
    return im;
}
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

inline Img mkCactusTall(Rng& r) {
    Img im(20, 30);
    u32 dk = HEX(0x2f7a3a), md = HEX(0x4a9a4c), lt = HEX(0x72bb62);
    auto limb = [&](int x, int y0, int y1, int w) {
        for (int y = y0; y <= y1; y++)
            for (int x2 = x; x2 < x + w; x2++) im.set(x2, y, x2 == x ? lt : (x2 == x + w - 1 ? dk : md));
    };
    limb(8, 5, 27, 5);
    im.set(10, 4, md), im.set(9, 4, lt), im.set(11, 4, dk);
    limb(3, 12, 20, 3);  // left arm
    im.rect(3, 19, 7, 3, md);
    im.rect(3, 19, 7, 1, lt);
    limb(14, 8, 16, 3);  // right arm
    im.rect(10, 15, 5, 3, md);
    im.rect(10, 15, 5, 1, lt);
    for (int i = 0; i < 12; i++) {  // spines
        int x = r.irange(3, 16), y = r.irange(5, 26);
        if (cA(im.get(x, y))) im.set(x, y, HEX(0xe8f0c0));
    }
    im.set(10, 3, HEX(0xe86aa0));  // flower
    im.set(9, 3, HEX(0xf6a0c4));
    im.outline(OUT);
    return im;
}
inline Img mkCactusBarrel() {
    Img im(16, 16);
    im.ellipseShaded(8, 9, 6, 6, HEX(0x72bb62), HEX(0x4a9a4c), HEX(0x2f7a3a));
    for (int x = 4; x <= 12; x += 2) im.line(x, 4, x, 14, HEX(0x2f7a3a));
    im.set(8, 3, HEX(0xf2c94c));
    im.set(7, 3, HEX(0xe8584f));
    im.set(9, 3, HEX(0xe8584f));
    im.outline(OUT);
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
inline Img mkCrystal() {
    Img im(18, 26);
    auto shard = [&](int cx, int base, int h, int w, u32 lt, u32 md, u32 dk) {
        for (int y = 0; y < h; y++) {
            float k = y / (float)h;
            int hw = (int)(w * (k < 0.3f ? k / 0.3f : 1.f));
            for (int x = cx - hw; x <= cx + hw; x++) im.set(x, base - h + y, x < cx ? lt : (x > cx ? dk : md));
        }
    };
    shard(5, 23, 14, 2, HEX(0xd8f4ff), HEX(0xa6dcf4), HEX(0x6fb4dc));
    shard(11, 24, 20, 3, HEX(0xe8faff), HEX(0xb4e4f8), HEX(0x74bce0));
    shard(15, 23, 10, 1, HEX(0xd8f4ff), HEX(0xa6dcf4), HEX(0x6fb4dc));
    im.set(10, 8, HEX(0xffffff));
    im.set(10, 9, HEX(0xffffff));
    im.outline(HEX(0x2a4a63));
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
inline Img mkDryBush() {
    Img im(12, 10);
    u32 a = HEX(0x8a6a3a), b = HEX(0xb08a4c);
    im.line(6, 9, 2, 3, a);
    im.line(6, 9, 6, 1, b);
    im.line(6, 9, 10, 3, a);
    im.line(6, 9, 4, 5, b);
    im.line(6, 9, 9, 5, b);
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
inline Img mkSandTuft() {
    Img im(8, 8);
    im.line(4, 7, 2, 3, HEX(0x9aa65a));
    im.line(4, 7, 4, 2, HEX(0xb8c46c));
    im.line(4, 7, 6, 3, HEX(0x9aa65a));
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
    Spr sand[3], dune[2], mud[2], murk[4], bog[4], snow[3], snowp[2], ice[2], stone[3], cliff[3];
    Spr cactus[2], palm, sandrock[2], column, ruinwall, arch;
    Spr deadtree[2], log, stilthut, witchtree, grave;
    Spr snowpine, baretree, snowrock[2], crystal, icehut, cairn;
    Spr cave, boulder, standing, bsign[4];
    Spr drybush, skull, sandtuft, reeds[2], lily, bubbles, drift, frosttuft, alpine, gravel;
    Spr node[RN_COUNT];
    Spr fog;
};

inline void buildBiomeAssets(SDL_Renderer* r, BiomeAssets& b) {
    Rng rg(7654321);
    for (int i = 0; i < 3; i++) b.sand[i] = makeSpr(r, mkSand(i, rg), 0, 0);
    for (int i = 0; i < 2; i++) b.dune[i] = makeSpr(r, mkDune(i, rg), 0, 0);
    for (int i = 0; i < 2; i++) b.mud[i] = makeSpr(r, mkMud(i, rg), 0, 0);
    for (int i = 0; i < 4; i++) b.murk[i] = makeSpr(r, mkMurk(i), 0, 0);
    for (int i = 0; i < 4; i++) b.bog[i] = makeSpr(r, mkBog(i), 0, 0);
    for (int i = 0; i < 3; i++) b.snow[i] = makeSpr(r, mkSnow(i, rg), 0, 0);
    for (int i = 0; i < 2; i++) b.snowp[i] = makeSpr(r, mkSnowPacked(i, rg), 0, 0);
    for (int i = 0; i < 2; i++) b.ice[i] = makeSpr(r, mkIce(i, rg), 0, 0);
    for (int i = 0; i < 3; i++) b.stone[i] = makeSpr(r, mkStone(i, rg), 0, 0);
    for (int i = 0; i < 3; i++) b.cliff[i] = makeSpr(r, mkCliff(i, rg), 0, 0);

    b.cactus[0] = makeSpr(r, mkCactusTall(rg), 10, 28);
    b.cactus[1] = makeSpr(r, mkCactusBarrel(), 8, 14);
    b.palm = makeSpr(r, mkPalm(rg), 20, 50);
    b.sandrock[0] = makeSpr(r, mkRockC(0, HEX(0xe3cf9c), HEX(0xc2a56e), HEX(0x9a804e), false), 10, 13);
    b.sandrock[1] = makeSpr(r, mkRockC(1, HEX(0xe3cf9c), HEX(0xc2a56e), HEX(0x9a804e), false), 13, 16);
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
    b.snowrock[0] = makeSpr(r, mkRockC(0, HEX(0xd2d8e2), HEX(0x9aa2b2), HEX(0x6e768a), true), 10, 13);
    b.snowrock[1] = makeSpr(r, mkRockC(1, HEX(0xd2d8e2), HEX(0x9aa2b2), HEX(0x6e768a), true), 13, 16);
    b.crystal = makeSpr(r, mkCrystal(), 9, 24);
    b.icehut = makeSpr(r, mkIceHut(), 28, 44);
    b.cairn = makeSpr(r, mkCairn(), 9, 22);
    b.cave = makeSpr(r, mkCave(), 26, 39);
    b.boulder = makeSpr(r, mkBoulder(), 15, 20);
    b.standing = makeSpr(r, mkStandingStone(), 8, 34);
    static const char* lbl[4] = {"DESERT", "SWAMP", "SNOW", "PEAKS"};
    for (int i = 0; i < 4; i++) b.bsign[i] = makeSpr(r, mkBiomeSign(lbl[i], BIOME[i == 0 ? B_DESERT : i == 1 ? B_SWAMP : i == 2 ? B_SNOW : B_MOUNTAIN].col), 18, 32);

    b.drybush = makeSpr(r, mkDryBush(), 6, 9);
    b.skull = makeSpr(r, mkSkull(), 5, 6);
    b.sandtuft = makeSpr(r, mkSandTuft(), 4, 7);
    b.reeds[0] = makeSpr(r, mkReeds(0), 6, 17);
    b.reeds[1] = makeSpr(r, mkReeds(1), 6, 17);
    b.lily = makeSpr(r, mkLily(), 5, 4);
    b.bubbles = makeSpr(r, mkBubbles(), 5, 4);
    b.drift = makeSpr(r, mkDrift(), 7, 5);
    b.frosttuft = makeSpr(r, mkFrostTuft(), 4, 7);
    b.alpine = makeSpr(r, mkAlpine(), 4, 7);
    b.gravel = makeSpr(r, mkGravel(), 5, 5);
    for (int k = 0; k < RN_COUNT; k++) b.node[k] = makeSpr(r, mkNode(k), 9, 16);
    b.fog = makeSpr(r, mkFog(), 56, 22);
}
