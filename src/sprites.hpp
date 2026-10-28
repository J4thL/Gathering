// sprites.hpp - every sprite in the game is painted here, procedurally, as pixel art.
#pragma once
#include "data.hpp"
#include <cstring>

static const u32 OUT = HEX(0x241a14);  // universal outline colour
#include "sprites_res.hpp"
#include "sprites_biome.hpp"

// ================================================================= tiles
inline Img mkGrass(int variant, Rng& r) {
    Img im(16, 16);
    u32 base = HEX(0x5aa63f), d = HEX(0x4b9235), l = HEX(0x70bd4c);
    if (variant == 3) {
        base = HEX(0x4d9738);
        d = HEX(0x408530);
        l = HEX(0x5fae44);
    }
    im.fill(base);
    int n = 9 + variant;
    for (int i = 0; i < n; i++) {
        int x = r.irange(1, 14), y = r.irange(2, 14);
        im.set(x, y, d);
        im.set(x, y - 1, d);
    }
    for (int i = 0; i < 5; i++) {
        int x = r.irange(1, 13), y = r.irange(2, 14);
        im.set(x, y, l);
        im.set(x + 1, y - 1, l);
    }
    return im;
}
inline Img mkDirt(int variant, Rng& r) {
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
inline Img mkFlower(u32 petal) {
    Img im(8, 8);
    im.rect(4, 4, 1, 4, HEX(0x3d8a2e));
    im.set(5, 6, HEX(0x3d8a2e));
    im.set(3, 3, petal);
    im.set(5, 3, petal);
    im.set(4, 2, petal);
    im.set(4, 4, petal);
    im.set(4, 3, HEX(0xf6d54a));
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
inline Img mkPebbles() {
    Img im(8, 6);
    im.ellipse(3, 3.5f, 2.2f, 1.6f, HEX(0x9a9fa8));
    im.ellipse(5.5f, 4, 1.6f, 1.2f, HEX(0x7e838e));
    im.set(2, 2, HEX(0xc3c7ce));
    return im;
}
inline Img mkBush(Rng& r) {
    Img im(18, 14);
    im.ellipseShaded(9, 8, 8, 5, HEX(0x62b34a), HEX(0x3f8a34), HEX(0x2e6b2b));
    im.ellipseShaded(5, 8, 4, 3.5f, HEX(0x62b34a), HEX(0x3f8a34), HEX(0x2e6b2b));
    im.ellipseShaded(13, 8, 4, 3.5f, HEX(0x62b34a), HEX(0x3f8a34), HEX(0x2e6b2b));
    for (int i = 0; i < 5; i++) {
        int x = r.irange(3, 14), y = r.irange(4, 9);
        im.set(x, y, HEX(0xd8443f));
    }
    im.outline(OUT);
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
    im.text(9, 6, "SHOP", HEX(0x3b2616));
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
inline Img mkPlayer(int dir, int f) {
    Img im(18, 26);
    const u32 skin = HEX(0xf0c49a), skinS = HEX(0xd9a77a), cap = HEX(0x3e7d46), capD = HEX(0x2d5e35),
              capL = HEX(0x5ba05c), tun = HEX(0x9b6b3a), tunD = HEX(0x7a5229), belt = HEX(0x3b2616),
              pants = HEX(0x5a4a3a), boots = HEX(0x2e2118), hair = HEX(0x5a3a22), eye = HEX(0x2a1c1c),
              leather = HEX(0x6b4423);
    int bob = (f == 1 || f == 3) ? 1 : 0;
    int ox = 1;  // canvas margin
    if (dir == 0 || dir == 1) {
        // legs
        int liftL = (f == 3) ? 2 : 0, liftR = (f == 1) ? 2 : 0;
        im.rect(ox + 5, 19, 3, 4 - liftL, pants);
        im.rect(ox + 8, 19, 3, 4 - liftR, pants);
        im.rect(ox + 5, 22 - liftL, 3, 2, boots);
        im.rect(ox + 8, 22 - liftR, 3, 2, boots);
        // arms
        int aL = (f == 1) ? -1 : (f == 3 ? 1 : 0);
        im.rect(ox + 3, 12 - bob + aL, 2, 5, tunD);
        im.rect(ox + 11, 12 - bob - aL, 2, 5, tunD);
        im.set(ox + 3, 17 - bob + aL, skin);
        im.set(ox + 4, 17 - bob + aL, skin);
        im.set(ox + 11, 17 - bob - aL, skin);
        im.set(ox + 12, 17 - bob - aL, skin);
        // torso
        im.rect(ox + 5, 11 - bob, 6, 8, tun);
        im.rect(ox + 5, 11 - bob, 6, 1, HEX(0xb3834f));
        im.rect(ox + 5, 18 - bob, 6, 1, tunD);
        im.rect(ox + 5, 15 - bob, 6, 1, belt);
        im.set(ox + 8, 15 - bob, HEX(0xf2c94c));
        // head
        im.ellipse(ox + 8, 7 - bob, 3.5f, 3.5f, skin);
        if (dir == 0) {
            im.line(ox + 5, 12 - bob, ox + 10, 17 - bob, leather);  // strap
            im.rect(ox + 9, 16 - bob, 3, 3, leather);               // pouch
            im.set(ox + 10, 17 - bob, HEX(0x8d5e33));
            // cap
            for (int y = 3; y <= 6; y++)
                for (int x = 4; x <= 11; x++)
                    if (cA(im.get(ox + x, y - bob))) im.set(ox + x, y - bob, y == 6 ? capD : cap);
            im.set(ox + 6, 4 - bob, capL);
            im.set(ox + 7, 4 - bob, capL);
            im.set(ox + 5, 6 - bob, capD);
            im.set(ox + 6, 8 - bob, eye);
            im.set(ox + 9, 8 - bob, eye);
            im.set(ox + 5, 8 - bob, skinS);
            im.set(ox + 10, 8 - bob, skinS);
        } else {
            im.rect(ox + 6, 12 - bob, 4, 5, leather);  // satchel on back
            im.rect(ox + 6, 12 - bob, 4, 1, HEX(0x8d5e33));
            for (int y = 3; y <= 7; y++)
                for (int x = 4; x <= 11; x++)
                    if (cA(im.get(ox + x, y - bob))) im.set(ox + x, y - bob, y >= 7 ? capD : cap);
            im.set(ox + 8, 8 - bob, hair);
            im.set(ox + 7, 8 - bob, hair);
            im.set(ox + 9, 8 - bob, hair);
            im.set(ox + 6, 4 - bob, capL);
        }
    } else {
        // side view facing right
        int a = (f == 0 || f == 2) ? 7 : (f == 1 ? 5 : 10);
        int b = (f == 0 || f == 2) ? 8 : (f == 1 ? 10 : 5);
        im.rect(ox + a - 1, 19, 3, 4, pants);
        im.rect(ox + b - 1, 19, 3, 4, HEX(0x6b5a46));
        im.rect(ox + a - 1, 22, 3, 2, boots);
        im.rect(ox + b - 1, 22, 4, 2, boots);
        im.rect(ox + 5, 11 - bob, 6, 8, tun);
        im.rect(ox + 5, 11 - bob, 6, 1, HEX(0xb3834f));
        im.rect(ox + 5, 18 - bob, 6, 1, tunD);
        im.rect(ox + 5, 15 - bob, 6, 1, belt);
        im.rect(ox + 3, 12 - bob, 3, 5, leather);  // satchel
        im.rect(ox + 3, 12 - bob, 3, 1, HEX(0x8d5e33));
        int sw = (f == 1) ? 2 : (f == 3 ? -2 : 0);
        im.rect(ox + 7 + sw, 12 - bob, 2, 5, tunD);
        im.set(ox + 7 + sw, 17 - bob, skin);
        im.set(ox + 8 + sw, 17 - bob, skin);
        im.ellipse(ox + 8, 7 - bob, 3.5f, 3.5f, skin);
        for (int y = 3; y <= 6; y++)
            for (int x = 4; x <= 11; x++)
                if (cA(im.get(ox + x, y - bob))) im.set(ox + x, y - bob, y == 6 ? capD : cap);
        im.rect(ox + 10, 6 - bob, 3, 1, capD);  // visor
        im.set(ox + 6, 4 - bob, capL);
        im.set(ox + 7, 4 - bob, capL);
        im.set(ox + 5, 8 - bob, hair);
        im.set(ox + 5, 9 - bob, hair);
        im.set(ox + 4, 8 - bob, hair);
        im.set(ox + 10, 8 - bob, eye);
        im.set(ox + 11, 9 - bob, skinS);
    }
    im.outline(OUT);
    return im;
}

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

inline Img drawQuad(const QuadSpec& s, int frame, bool dead) {
    Img im(s.W, s.H);
    int ground = s.H - 2;
    float bodyBottom = (float)(ground - s.legLen);
    float bob = (frame == 1 || frame == 3) ? -1.f : 0.f;
    float cy = bodyBottom - s.bh / 2 + 1 + bob;
    float cx = s.cx;
    static const int liftT[4] = {0, 0, 2, 1};
    static const int shiftT[4] = {0, -1, 0, 1};
    int phA = frame & 3, phB = (frame + 2) & 3;
    auto leg = [&](float bx, int ph, u32 col) {
        int x = (int)roundf(bx) + shiftT[ph];
        int top = (int)bodyBottom - 2;
        int bot = ground - liftT[ph];
        for (int y = top; y <= bot; y++)
            for (int i = 0; i < s.legW; i++) im.set(x + i, y, (y >= bot - 1 && s.legLen > 4) ? s.dark : col);
        for (int i = 0; i < s.legW; i++) im.set(x + i, bot, s.hoof);
    };
    // far legs
    leg(cx - s.bw * 0.30f, phA, s.dark);
    leg(cx + s.bw * 0.10f, phB, s.dark);
    // tail (behind body)
    float tx = cx - s.bw / 2;
    if (s.tail == 1) im.ellipse(tx + 0.5f, cy - 1, 2.0f, 2.0f, HEX(0xf4f0ea));
    if (s.tail == 2) {
        im.ellipse(tx + 1, cy - s.bh / 2 + 2, 1.6f, 2.4f, HEX(0xf1e6d2));
    }
    if (s.tail == 3) {
        for (int i = 0; i <= 6; i++) im.ellipse(tx + 1 - i * 0.9f, cy - 1 + i * 0.7f, 2.0f - i * 0.12f, 1.8f - i * 0.1f, i > 4 ? s.belly : s.body);
    }
    if (s.tail == 4) {
        im.line((int)tx + 1, (int)cy - 2, (int)tx - 2, (int)cy - 4, s.dark);
        im.set((int)tx - 3, (int)cy - 5, s.dark);
    }
    if (s.tail == 5) {  // long thin tail (big cats, lizards)
        im.line((int)tx + 1, (int)cy - 1, (int)tx - 4, (int)cy + 1, s.body);
        im.line((int)tx - 4, (int)cy + 1, (int)tx - 6, (int)cy - 2, s.dark);
    }
    // body
    for (int y = 0; y < s.H; y++)
        for (int x = 0; x < s.W; x++) {
            float dx = (x + 0.5f - cx) / (s.bw / 2), dy = (y + 0.5f - cy) / (s.bh / 2);
            if (dx * dx + dy * dy > 1.f) continue;
            float t = (dy + 1.f) * 0.5f;
            u32 c = s.body;
            if (t < 0.26f) c = s.light;
            else if (t > 0.72f) c = s.belly;
            if (((x * 73856093) ^ (y * 19349663)) % 13 == 0 && t < 0.72f) c = s.dark;
            im.set(x, y, c);
        }
    if (s.spots) {
        int sp[5][2] = {{-4, -1}, {-1, -2}, {2, -1}, {4, 0}, {0, 0}};
        for (auto& q : sp) im.set((int)(cx + q[0]), (int)(cy + q[1]), HEX(0xf1e6d2));
    }
    if (s.ridge) {
        for (float x = cx - s.bw * 0.42f; x < cx + s.bw * 0.3f; x += 2.f) {
            im.set((int)x, (int)(cy - s.bh / 2) - 1, s.dark);
            im.set((int)x + 1, (int)(cy - s.bh / 2) - 2, s.dark);
        }
    }
    if (s.ear == 1) im.ellipse(cx - s.bw * 0.28f, cy + 1.5f, 2.2f, 2.6f, s.body);  // rabbit haunch
    // near legs
    leg(cx - s.bw * 0.12f, phB, s.body);
    leg(cx + s.bw * 0.30f, phA, s.body);
    // neck + head
    float hx = cx + s.bw / 2 + s.hdx, hy = cy - s.bh / 2 + s.hdy;
    float nx = cx + s.bw / 2 - 1.5f, ny = cy - s.bh * 0.15f;
    if (s.mane) {
        im.ellipse(hx - 1.0f, hy + 1.0f, s.hr * 1.5f, s.hr * 1.4f, s.dark);
        im.ellipse(hx - 2.0f, hy + 2.5f, s.hr * 1.2f, s.hr * 1.2f, shadeC(s.dark, 0.85f));
    }
    for (int i = 0; i <= 6; i++) {
        float t = i / 6.f;
        im.ellipse(nx + (hx - nx) * t, ny + (hy - ny) * t, s.hr * (0.85f - 0.1f * t), s.hr * (0.85f - 0.1f * t), s.body);
    }
    im.ellipse(hx, hy, s.hr, s.hr * 0.9f, s.body);
    im.ellipse(hx - 0.5f, hy - s.hr * 0.4f, s.hr * 0.6f, s.hr * 0.3f, s.light);
    if (s.snoutLen > 0) {
        im.ellipse(hx + s.hr * 0.7f + s.snoutLen * 0.3f, hy + s.hr * 0.35f, s.snoutLen * 0.7f, s.hr * 0.55f, s.snout);
        im.set((int)(hx + s.hr * 0.7f + s.snoutLen), (int)(hy + s.hr * 0.2f), HEX(0x1c1410));  // nose
    } else {
        im.set((int)(hx + s.hr), (int)(hy + 1), HEX(0xe8a0a0));
    }
    // eye
    int ex = (int)(hx + s.hr * 0.35f), ey = (int)(hy - s.hr * 0.15f);
    if (dead) {
        im.set(ex, ey, s.eye);
        im.set(ex + 1, ey + 1, s.eye);
        im.set(ex + 1, ey - 1, s.eye);
        im.set(ex - 1, ey + 1, s.eye);
        im.set(ex - 1, ey - 1, s.eye);
        if (s.snoutLen > 0) im.set((int)(hx + s.hr * 0.6f), (int)(hy + s.hr * 0.8f), HEX(0xe56b7a));  // tongue
    } else {
        im.set(ex, ey, s.eye);
        im.set(ex, ey - 1, s.eye);
    }
    // ears
    switch (s.ear) {
        case 1:
            im.ellipse(hx - 1.5f, hy - s.hr - 2.5f, 1.3f, 3.6f, s.body);
            im.ellipse(hx - 3.2f, hy - s.hr - 1.8f, 1.2f, 3.2f, s.dark);
            im.set((int)(hx - 1.5f), (int)(hy - s.hr - 2), HEX(0xe8a0a0));
            break;
        case 2:
            im.ellipse(hx - 2.2f, hy - s.hr + 0.8f, 2.3f, 1.2f, s.body);
            im.set((int)(hx - 2), (int)(hy - s.hr + 1), HEX(0xe8a0a0));
            break;
        case 3: {
            int ex2 = (int)(hx - 1), ey2 = (int)(hy - s.hr);
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4 - i; j++) im.set(ex2 - 1 + j, ey2 - 3 + i + 0, i == 3 ? s.dark : s.body), im.set(ex2 + j - 1, ey2 - 3 + i, s.body);
            im.set(ex2, ey2 - 1, s.dark);
            break;
        }
        case 4:
            im.ellipse(hx - 1.5f, hy - s.hr + 0.8f, 1.7f, 1.5f, s.dark);
            break;
        case 5:  // lynx: pointed ear with a dark tuft
            im.ellipse(hx - 1.5f, hy - s.hr - 0.5f, 1.3f, 2.2f, s.body);
            im.set((int)(hx - 1.5f), (int)(hy - s.hr - 3), s.dark);
            break;
    }
    if (s.antlers) {
        u32 ac = HEX(0xdcc79a), ad = HEX(0xb89d6a);
        int bx = (int)(hx - 1), by = (int)(hy - s.hr);
        im.line(bx, by, bx - 2, by - 8, ac);
        im.line(bx - 2, by - 4, bx + 1, by - 7, ac);
        im.line(bx - 2, by - 6, bx - 5, by - 9, ac);
        im.line(bx + 1, by, bx + 2, by - 7, ad);
        im.line(bx + 2, by - 4, bx + 5, by - 6, ad);
        if (s.antlers > 1) {  // a bigger rack: extra tines
            im.line(bx - 5, by - 9, bx - 7, by - 12, ac);
            im.line(bx - 2, by - 8, bx - 3, by - 12, ac);
            im.line(bx + 5, by - 6, bx + 7, by - 9, ad);
            im.line(bx + 2, by - 7, bx + 3, by - 11, ad);
        }
    }
    if (s.horns) {
        u32 hc = s.horn ? s.horn : HEX(0x2e2a26);
        int bx = (int)(hx - 1), by = (int)(hy - s.hr);
        if (s.horns == 1) {  // short, backward-curved goat horns
            im.line(bx, by, bx - 1, by - 4, hc);
            im.line(bx - 1, by - 4, bx - 3, by - 6, hc);
            im.line(bx + 1, by, bx, by - 4, shadeC(hc, 0.8f));
        } else if (s.horns == 2) {  // heavy curl around the ear
            static const int arc[10][2] = {{0, 0}, {-1, -1}, {-2, -1}, {-3, 0}, {-4, 1}, {-4, 2}, {-3, 3}, {-2, 3}, {-1, 2}, {0, 1}};
            for (auto& q : arc) {
                im.set(bx + q[0], by + q[1], hc);
                im.set(bx + q[0], by + q[1] - 1, hc);
            }
        } else {  // moose: flat palmate blade with points
            u32 ac = hc;
            im.line(bx, by + 1, bx - 1, by - 3, ac);  // beam
            im.line(bx + 1, by + 1, bx, by - 3, ac);
            im.ellipse((float)(bx - 2), (float)(by - 4), 4.5f, 2.0f, ac);  // palm
            for (int i = 0; i < 4; i++) im.line(bx - 5 + i * 2, by - 5, bx - 5 + i * 2, by - 7, ac);  // points
            im.ellipse((float)(bx - 2), (float)(by - 4), 2.5f, 0.9f, shadeC(ac, 0.8f));
        }
    }
    if (s.tusks) {
        int tx2 = (int)(hx + s.hr * 0.7f + s.snoutLen * 0.5f), ty2 = (int)(hy + s.hr * 0.6f);
        im.set(tx2, ty2, HEX(0xf3eccb));
        im.set(tx2 + 1, ty2 - 1, HEX(0xf3eccb));
        im.set(tx2 + 1, ty2 - 2, HEX(0xf3eccb));
    }
    im.outline(OUT);
    if (dead) im = im.flippedV();
    return im;
}

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
    s.body = HEX(0xa9744a); s.light = HEX(0xc08a5c); s.belly = HEX(0xe3cfae); s.dark = HEX(0x7b5233);
    s.hoof = HEX(0x2e2118); s.snout = HEX(0xd9bd96); s.eye = HEX(0x241a14);
    s.hr = 3.2f; s.hdx = 3.5f; s.hdy = -6.5f; s.snoutLen = 3.5f; s.ear = 2; s.tail = 2;
    s.antlers = stag; s.spots = !stag;
    return s;
}
inline QuadSpec specBoar() {
    QuadSpec s{};
    s.W = 36; s.H = 26; s.cx = 14; s.bw = 18; s.bh = 11; s.legLen = 4; s.legW = 2;
    s.body = HEX(0x6b4a3a); s.light = HEX(0x87634d); s.belly = HEX(0x4e3529); s.dark = HEX(0x3d2a20);
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

#include "sprites_fauna.hpp"

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
            u32 c[3] = {HEX(0x8a5c34), HEX(0xa87a4a), HEX(0x6e7a4a)};
            u32 d[3] = {HEX(0x5e3d22), HEX(0x7a5230), HEX(0x4a5632)};
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
            u32 c[3] = {HEX(0x6b4a2b), HEX(0xa87a4a), HEX(0xd8d2c8)};
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
            u32 c[3] = {HEX(0x9b6b3a), HEX(0xb08a5a), HEX(0xcfd3d8)};
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
inline Img mkShadow(int w, int h) {
    Img im(w, h);
    im.ellipse(w / 2.f, h / 2.f, w / 2.f, h / 2.f, RGBA(0, 0, 0, 70));
    im.ellipse(w / 2.f, h / 2.f, w / 2.f - 1.5f, h / 2.f - 1.f, RGBA(0, 0, 0, 40));
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
    Spr grass[4], dirt[2], water[4];
    Spr flower[3], tuft, mushroom, pebbles, bush[2];
    Spr oak[2], pine, rock[2];
    Spr hut, sign, crate, barrel, fire[3], keeper[2], quester[2];
    Spr player[3][4];
    Spr animal[A_COUNT][8][5];  // [kind][form + 2*tier][walk0..3, dead]
    Spr matIcon[M_COUNT], wpnIcon[W_COUNT], gearIcon[U_COUNT][3], bandage;
    Spr wpnWorld[W_COUNT], ammoIcon[AM_COUNT], ammoWorld[AM_COUNT];
    Spr coin, heart, bolt, cursor, glowWarm, glowCool, shadow[3], blood[3], vignette;
    BiomeAssets B;  // desert / swamp / snow / mountain art (sprites_biome.hpp)
    ResAssets R;    // tools, consumables, placeables, depleted resources (sprites_res.hpp)
    SDL_Texture* font = nullptr;
};

inline void buildAssets(SDL_Renderer* r, Assets& a) {
    Rng rg(1234567);
    for (int i = 0; i < 4; i++) a.grass[i] = makeSpr(r, mkGrass(i, rg), 0, 0);
    for (int i = 0; i < 2; i++) a.dirt[i] = makeSpr(r, mkDirt(i, rg), 0, 0);
    for (int i = 0; i < 4; i++) a.water[i] = makeSpr(r, mkWater(i), 0, 0);
    a.flower[0] = makeSpr(r, mkFlower(HEX(0xe8584f)), 4, 7);
    a.flower[1] = makeSpr(r, mkFlower(HEX(0xf4f0ea)), 4, 7);
    a.flower[2] = makeSpr(r, mkFlower(HEX(0x8f6fe0)), 4, 7);
    a.tuft = makeSpr(r, mkTuft(), 4, 7);
    a.mushroom = makeSpr(r, mkMushroom(), 4, 7);
    a.pebbles = makeSpr(r, mkPebbles(), 4, 5);
    for (int i = 0; i < 2; i++) a.bush[i] = makeSpr(r, mkBush(rg), 9, 12);
    a.oak[0] = makeSpr(r, mkOak(0, rg), 17, 40);
    a.oak[1] = makeSpr(r, mkOak(1, rg), 17, 40);
    a.pine = makeSpr(r, mkPine(rg), 13, 42);
    a.rock[0] = makeSpr(r, mkRock(0), 10, 13);
    a.rock[1] = makeSpr(r, mkRock(1), 12, 16);
    a.hut = makeSpr(r, mkHut(), 33, 56);
    a.sign = makeSpr(r, mkSign(), 16, 24);
    a.crate = makeSpr(r, mkCrate(), 8, 13);
    a.barrel = makeSpr(r, mkBarrel(), 7, 16);
    for (int i = 0; i < 3; i++) a.fire[i] = makeSpr(r, mkFire(i), 9, 17);
    for (int i = 0; i < 2; i++) a.keeper[i] = makeSpr(r, mkShopkeeper(i), 9, 24);
    for (int i = 0; i < 2; i++) a.quester[i] = makeSpr(r, mkQuestGiver(i), 9, 24);
    for (int d = 0; d < 3; d++)
        for (int f = 0; f < 4; f++) a.player[d][f] = makeSpr(r, mkPlayer(d, f), 9, 24, true);
    buildFauna(r, a);
    for (int m = 0; m < M_COUNT; m++) a.matIcon[m] = makeSpr(r, m < M_CARCASS_N ? mkIconMat(m) : mkIconLand(m), 8, 8);
    for (int w = 0; w < W_COUNT; w++) a.wpnIcon[w] = makeSpr(r, w < W_SPEAR ? mkIconWpn(w) : mkIconWpnNew(w), 8, 8);
    for (int k = 0; k < TL_COUNT; k++) {
        Img t = mkIconTool(k);
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
    a.R.stub = makeSpr(r, mkStub(), 6, 6);
    a.R.sprout = makeSpr(r, mkSprout(), 5, 9);
    for (int u = 0; u < U_COUNT; u++)
        for (int t = 0; t < 3; t++) a.gearIcon[u][t] = makeSpr(r, mkIconGear(u, t), 8, 8);
    a.bandage = makeSpr(r, mkIconBandage(), 8, 8);
    for (int w = 0; w < W_COUNT; w++) {
        Img im = w < W_SPEAR ? mkWpnWorld(w) : mkWpnWorldNew(w);
        a.wpnWorld[w] = makeSpr(r, im, im.w / 2, im.h / 2);
    }
    for (int k = 0; k < AM_COUNT; k++) {
        a.ammoIcon[k] = makeSpr(r, mkIconAmmo(k), 8, 8);
        Img im = mkAmmoWorld(k);
        a.ammoWorld[k] = makeSpr(r, im, im.w / 2, im.h / 2);
    }
    buildBiomeAssets(r, a.B);
    a.coin = makeSpr(r, mkCoin(), 5, 5);
    a.heart = makeSpr(r, mkHeart(), 5, 5);
    a.bolt = makeSpr(r, mkBolt(), 4, 6);
    a.cursor = makeSpr(r, mkCursor(), 1, 1);
    a.glowWarm = makeSpr(r, mkGlow(HEX(0xffb24a), 96), 48, 48);
    a.glowCool = makeSpr(r, mkGlow(HEX(0xfff2b0), 24), 12, 12);
    SDL_SetTextureBlendMode(a.glowWarm.t, SDL_BLENDMODE_ADD);
    SDL_SetTextureBlendMode(a.glowCool.t, SDL_BLENDMODE_ADD);
    a.shadow[0] = makeSpr(r, mkShadow(14, 6), 7, 3);
    a.shadow[1] = makeSpr(r, mkShadow(26, 8), 13, 4);
    a.shadow[2] = makeSpr(r, mkShadow(40, 12), 20, 6);
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
