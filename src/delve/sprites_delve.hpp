// sprites_delve.hpp - the art of the Abandoned Mine and the Frozen Dungeon, drawn in code like everything else:
// cave and ice tiles, the two entrances, timbers, carts, torches, crystals, statues, pillars, chests, stairs, doors, traps and the
// ore deposits. Everything is top-down 2D with the same pseudo-3D shading as the rest of the game: light from the top-left,
// lit faces and shaded undersides, a one pixel dark contour. (This header does not know the Tile / PropKind enums - world.hpp does.)
#pragma once
#include "core/data.hpp"
#include "items/items.hpp"

namespace dart {

inline uint32_t nz(int x, int y, uint32_t s) { return hash2(x, y, s); }
inline float nzf(int x, int y, uint32_t s) { return (nz(x, y, s) & 0xFFFF) / 65535.f; }

// ------------------------------------------------------------------ tiles (16 x 16)
inline Img caveFloor(int v) {
    Img im(16, 16);
    const u32 a = HEX(0x4a403a), b = HEX(0x433a34), c = HEX(0x3a322d), d = HEX(0x5a4e46);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            float n = nzf(x + v * 16, y, 11) * 0.6f + nzf(x / 2 + v * 8, y / 2, 17) * 0.4f;
            im.set(x, y, n < 0.25f ? c : n < 0.62f ? b : n < 0.9f ? a : d);
        }
    for (int i = 0; i < 4; i++) {  // pebbles with a lit top-left edge and a dark underside
        const int x = 2 + (int)(nz(i, v, 3) % 11), y = 2 + (int)(nz(i, v, 5) % 11);
        im.set(x, y, HEX(0x6a5e54));
        im.set(x + 1, y, HEX(0x544840));
        im.set(x, y + 1, HEX(0x2e2824));
        im.set(x + 1, y + 1, HEX(0x2e2824));
    }
    if (v == 1) im.line(3, 12, 9, 10, HEX(0x2a2420));  // a crack
    if (v == 3) { im.line(10, 3, 13, 6, HEX(0x2a2420)); im.line(13, 6, 13, 9, HEX(0x2a2420)); }
    return im;
}
inline Img caveWallFace(int v) {  // the vertical face of the rock: lit from above, dark at the foot
    Img im(16, 16);
    for (int y = 0; y < 16; y++) {
        const float t = y / 15.f;
        for (int x = 0; x < 16; x++) {
            float n = nzf(x / 2 + v * 5, y, 23);
            u32 base = mixc(HEX(0x8a7c70), HEX(0x2c2622), t * 0.9f);
            if (n > 0.8f) base = shadeC(base, 1.18f);
            else if (n < 0.2f) base = shadeC(base, 0.82f);
            im.set(x, y, base);
        }
    }
    for (int i = 0; i < 3; i++) {  // strata and cracks running down the face
        const int x = 2 + (int)(nz(i, v, 31) % 12);
        const int y0 = (int)(nz(i, v, 33) % 5);
        for (int y = y0; y < y0 + 8 && y < 16; y++) im.set(x + ((y + i) % 5 == 0 ? 1 : 0), y, HEX(0x241e1a));
    }
    im.rect(0, 0, 16, 1, HEX(0xa89888));  // the lit lip on top
    im.rect(0, 15, 16, 1, HEX(0x1c1814));
    if (v == 2) { im.set(5, 6, HEX(0xd8844a)); im.set(6, 7, HEX(0xf0a868)); im.set(11, 9, HEX(0xdfe6f2)); }  // ore flecks in the rock
    return im;
}
inline Img caveWallTop(int v) {  // the flat top of the rock mass (deep wall)
    Img im(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            float n = nzf(x + v * 7, y, 41) * 0.5f + nzf(x / 3, y / 3 + v, 43) * 0.5f;
            im.set(x, y, n < 0.3f ? HEX(0x1c1816) : n < 0.7f ? HEX(0x26211e) : HEX(0x302a26));
        }
    return im;
}
inline Img rail(bool vertical) {  // cave floor with a mine track: sleepers and two iron rails
    Img im = caveFloor(vertical ? 0 : 2);
    for (int i = 0; i < 16; i += 5) {
        if (vertical) im.rect(2, i + 1, 12, 2, HEX(0x6a4a2a)), im.rect(2, i + 1, 12, 1, HEX(0x8a643a));
        else im.rect(i + 1, 2, 2, 12, HEX(0x6a4a2a)), im.rect(i + 1, 2, 1, 12, HEX(0x8a643a));
    }
    for (int k = 0; k < 2; k++) {
        const int o = k ? 11 : 4;
        if (vertical) { im.rect(o, 0, 1, 16, HEX(0x2a2e38)); im.rect(o + 1, 0, 1, 16, HEX(0x8890a0)); }
        else { im.rect(0, o, 16, 1, HEX(0x2a2e38)); im.rect(0, o + 1, 16, 1, HEX(0x8890a0)); }
    }
    return im;
}
inline Img iceFloor(int v) {  // pale blue flagstones with frosted seams
    Img im(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            float n = nzf(x + v * 13, y, 51);
            u32 c = n < 0.3f ? HEX(0x7aa8c8) : n < 0.8f ? HEX(0x86b4d2) : HEX(0x9cc6e0);
            im.set(x, y, c);
        }
    im.rect(0, 0, 16, 1, HEX(0xc4e4f6));  // lit seams
    im.rect(0, 0, 1, 16, HEX(0xb4d8ee));
    im.rect(0, 15, 16, 1, HEX(0x5a84a8));
    im.rect(15, 0, 1, 16, HEX(0x5a84a8));
    if (v == 1) { im.line(3, 4, 7, 8, HEX(0xe8f6ff)); im.line(7, 8, 6, 12, HEX(0xe8f6ff)); }
    if (v == 2) { im.set(10, 5, HEX(0xffffff)); im.set(4, 11, HEX(0xffffff)); }
    if (v == 3) im.line(9, 3, 13, 7, HEX(0x5a84a8));
    return im;
}
inline Img iceWallFace(int v) {  // brick face of the fortress: lit blocks, deep blue joints
    Img im(16, 16);
    for (int row = 0; row < 2; row++) {
        const int y0 = row * 8, off = (row + v) % 2 ? 0 : 5;
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 8; y++) {
                const float t = (y0 + y) / 15.f;
                u32 c = mixc(HEX(0xb6d8ee), HEX(0x3c6a98), t * 0.85f);
                if (nzf(x, y0 + y, 61 + v) > 0.86f) c = shadeC(c, 1.12f);
                im.set(x, y0 + y, c);
            }
        }
        im.rect(0, y0 + 7, 16, 1, HEX(0x2a4a70));
        for (int jx = off; jx < 16; jx += 10) im.rect(jx, y0, 1, 7, HEX(0x2a4a70));
        im.rect(0, y0, 16, 1, HEX(0xe8f8ff));
    }
    if (v == 1) { im.set(4, 4, HEX(0xffffff)); im.line(11, 9, 12, 13, HEX(0xe8f8ff)); }
    im.rect(0, 15, 16, 1, HEX(0x1c3452));
    return im;
}
inline Img iceWallTop(int v) {  // flat top of an ice wall: bright slab dusted with snow
    Img im(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            float n = nzf(x / 2 + v * 3, y / 2, 71);
            im.set(x, y, n < 0.35f ? HEX(0x4a7aa4) : n < 0.8f ? HEX(0x5a8ab4) : HEX(0x6a9ac4));
        }
    for (int i = 0; i < 6; i++) im.set((int)(nz(i, v, 73) % 16), (int)(nz(i, v, 79) % 16), HEX(0xe8f6ff));
    return im;
}
inline Img brickFloor(int v) {  // the crypt: dark frozen brick
    Img im(16, 16);
    for (int row = 0; row < 2; row++) {
        const int y0 = row * 8, off = (row + v) % 2 ? 0 : 6;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 16; x++) {
                float n = nzf(x, y0 + y, 83 + v);
                im.set(x, y0 + y, n < 0.3f ? HEX(0x2e3c58) : n < 0.8f ? HEX(0x34456a) : HEX(0x3c4e76));
            }
        im.rect(0, y0, 16, 1, HEX(0x1a2238));
        for (int jx = off; jx < 16; jx += 10) im.rect(jx, y0, 1, 8, HEX(0x1a2238));
        im.rect(0, y0 + 1, 16, 1, HEX(0x4a608a));
    }
    if (v == 2) { im.set(4, 5, HEX(0xdff4ff)); im.set(5, 6, HEX(0xa8d8f8)); im.set(12, 12, HEX(0xdff4ff)); }
    return im;
}
inline Img carpet(int v) {  // the throne hall: deep blue with a silver-gold border pattern
    Img im(16, 16);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) im.set(x, y, nzf(x, y, 91) > 0.7f ? HEX(0x2a4a9a) : HEX(0x22408a));
    if (v == 0) {
        im.rect(0, 0, 3, 16, HEX(0x16285a)); im.rect(13, 0, 3, 16, HEX(0x16285a));
        im.rect(2, 0, 1, 16, HEX(0xc8d8f0)); im.rect(13, 0, 1, 16, HEX(0xc8d8f0));
        for (int y = 2; y < 16; y += 6) { im.set(7, y, HEX(0xe8c860)); im.set(8, y + 1, HEX(0xe8c860)); im.set(7, y + 2, HEX(0xe8c860)); im.set(6, y + 1, HEX(0xe8c860)); }
    } else {
        im.rect(0, 2, 16, 1, HEX(0xc8d8f0));
        im.rect(0, 0, 16, 2, HEX(0x16285a));
    }
    return im;
}

// ------------------------------------------------------------------ ore deposits (18 x 18): cave rock with the vein set into it
inline Img nodeSprite(int which) {  // 0 coal, 1 silver, 2 cave crystal, 3 gems, 4 eternal ice
    Img im(18, 18);
    im.ellipseShaded(9, 11, 7.5f, 6.2f, HEX(0x7a7068), HEX(0x544c46), HEX(0x2e2824));
    im.rect(3, 13, 12, 1, HEX(0x2a2420));
    auto spot = [&](int x, int y, u32 l, u32 m, u32 d, int s = 2) {
        im.rect(x, y, s, s, m);
        im.set(x, y, l);
        im.set(x + s - 1, y + s - 1, d);
    };
    switch (which) {
        case 0:  // coal: glossy black lumps
            for (auto d : {std::pair<int, int>{4, 7}, {9, 5}, {7, 9}, {11, 9}, {5, 11}, {12, 12}}) spot(d.first, d.second, HEX(0x6a6a78), HEX(0x26262e), HEX(0x101016), 3);
            im.set(10, 6, HEX(0xffffff));
            break;
        case 1:  // silver: bright pale veins
            for (auto d : {std::pair<int, int>{4, 6}, {9, 5}, {7, 9}, {12, 8}, {5, 11}, {11, 12}}) spot(d.first, d.second, HEX(0xffffff), HEX(0xdfe6f2), HEX(0x8a94aa));
            im.line(4, 9, 8, 6, HEX(0xc8d2e4));
            im.line(10, 10, 13, 7, HEX(0xc8d2e4));
            break;
        case 2:  // cave crystal: a violet cluster growing out of the rock
            for (auto d : {std::array<int, 3>{4, 6, 3}, std::array<int, 3>{8, 2, 4}, std::array<int, 3>{12, 6, 3}, std::array<int, 3>{7, 8, 3}}) {
                im.rect(d[0], d[1], d[2], 10 - d[1] + 2, HEX(0xb070ff));
                im.rect(d[0], d[1], 1, 10 - d[1] + 2, HEX(0xf0d8ff));
                im.rect(d[0] + d[2] - 1, d[1] + 1, 1, 10 - d[1] + 1, HEX(0x5a2a98));
            }
            im.set(9, 2, HEX(0xffffff));
            break;
        case 3:  // gems: a few cut stones in different colours
            spot(4, 6, HEX(0xffd0e0), HEX(0xff4a88), HEX(0x9a1c52), 3);
            spot(10, 5, HEX(0xd0f4ff), HEX(0x4ac8ff), HEX(0x1a6a9a), 3);
            spot(8, 10, HEX(0xd8ffd8), HEX(0x4ae88a), HEX(0x1a8a4a), 3);
            spot(13, 10, HEX(0xfff0c0), HEX(0xffc84a), HEX(0x9a6a1a), 2);
            im.set(5, 6, HEX(0xffffff)); im.set(11, 5, HEX(0xffffff));
            break;
        default:  // eternal ice: a pale blue crystal vein
            for (auto d : {std::array<int, 3>{4, 6, 3}, std::array<int, 3>{9, 4, 4}, std::array<int, 3>{12, 9, 3}, std::array<int, 3>{7, 10, 3}}) {
                im.rect(d[0], d[1], d[2], d[2] + 2, HEX(0x6ac0f0));
                im.rect(d[0], d[1], 1, d[2] + 2, HEX(0xe8faff));
                im.rect(d[0] + d[2] - 1, d[1] + 1, 1, d[2] + 1, HEX(0x2a78b8));
            }
            im.set(9, 4, HEX(0xffffff));
            break;
    }
    im.outline(OUT);
    return im;
}

// a crystal cluster (width x height), colours: lit, mid, dark
inline Img crystalCluster(int w, int h, u32 l, u32 m, u32 d, int seed) {
    Img im(w, h);
    const int n = 4 + seed % 2;
    for (int i = 0; i < n; i++) {
        const int bx = 3 + (int)((w - 6) * (i + 0.5f) / n) + (int)(nz(i, seed, 7) % 3) - 1;
        const int ht = std::max(5, h - 5 - (int)(nz(i, seed, 9) % (h / 2 - 2)) - (i == n / 2 ? 0 : 3));
        const int bw = 2 + (int)(nz(i, seed, 11) % 2);
        const int lean = (int)(nz(i, seed, 13) % 3) - 1;
        for (int y = 0; y < ht; y++) {
            const int x0 = bx + (lean * (ht - y)) / (ht / 2 + 1) - bw / 2;
            const float t = (float)y / ht;
            const int ww = y < 3 ? std::max(1, bw - 1 + (y > 0)) : bw;
            im.rect(x0, h - 3 - ht + y, ww, 1, m);
            im.set(x0, h - 3 - ht + y, l);
            im.set(x0 + ww - 1, h - 3 - ht + y, t > 0.3f ? d : m);
        }
        im.set(bx + lean * 2 / 3 - bw / 2, h - 3 - ht, HEX(0xffffff));
    }
    im.ellipseShaded(w / 2.f, h - 3.f, w / 2.f - 1.f, 2.6f, shadeC(m, 0.8f), shadeC(m, 0.55f), shadeC(m, 0.35f));
    im.outline(OUT);
    return im;
}

inline Img statue(int v) {  // 0 knight, 1 mage, 2 king: carved from pale blue-grey ice stone on a plinth
    Img im(24, 38);
    const u32 L = HEX(0xdff0fb), M = HEX(0xa8c8e0), D = HEX(0x5a7a9c), DD = HEX(0x34506c);
    im.rect(4, 31, 16, 6, M); im.rect(4, 31, 16, 1, L); im.rect(4, 36, 16, 1, DD); im.rect(18, 31, 2, 6, D);  // plinth
    im.rect(6, 29, 12, 3, D);
    if (v == 1) { im.ellipseShaded(12, 24, 6.5f, 6.f, L, M, D); im.rect(6, 26, 12, 4, M); }  // robe
    else { im.rect(7, 20, 4, 10, M); im.rect(13, 20, 4, 10, D); im.rect(7, 20, 1, 10, L); }  // legs
    im.ellipseShaded(12, 15, 6.5f, 6.f, L, M, D);                                             // torso
    im.rect(6, 14, 12, 2, M);
    im.rect(4, 11, 3, 9, M); im.rect(17, 11, 3, 9, D); im.rect(4, 11, 1, 9, L);               // arms
    im.ellipseShaded(12, 7, 4.2f, 4.4f, L, M, D);                                             // head
    if (v == 0) { im.rect(8, 6, 8, 2, DD); im.rect(11, 2, 2, 3, M); }                          // helm with a visor slit
    if (v == 1) { im.rect(7, 4, 10, 3, M); im.set(12, 1, L); im.rect(11, 2, 2, 2, M); }        // pointed hood
    if (v == 2) { for (int i = 0; i < 5; i++) im.rect(7 + i * 2, 1 - (i % 2), 1, 3 + (i % 2), HEX(0xf2e8b0)); im.rect(7, 3, 10, 1, HEX(0xf2e8b0)); }  // crown
    if (v == 0) { im.rect(2, 4, 2, 24, L); im.rect(2, 4, 1, 24, HEX(0xffffff)); im.rect(1, 24, 4, 2, D); }
    if (v == 1) { im.rect(20, 3, 1, 26, D); im.ellipseShaded(20.5f, 3, 2.4f, 2.4f, HEX(0xe8faff), HEX(0x6ac8ff), HEX(0x2a78b8)); }
    if (v == 2) { im.rect(3, 8, 2, 22, L); im.rect(1, 12, 6, 2, D); im.rect(2, 8, 1, 22, HEX(0xffffff)); }
    im.set(7, 9, HEX(0xffffff)); im.set(17, 9, HEX(0xffffff)); im.set(10, 11, HEX(0xffffff));  // frost on the shoulders
    im.outline(OUT);
    return im;
}

inline Img pillar() {
    Img im(16, 44);
    const u32 L = HEX(0xdff0fb), M = HEX(0xa8c8e0), D = HEX(0x6a8aac), DD = HEX(0x34506c);
    im.rect(2, 38, 12, 5, M); im.rect(2, 38, 12, 1, L); im.rect(2, 42, 12, 1, DD);   // base
    im.rect(4, 5, 8, 34, M);
    im.rect(4, 5, 3, 34, L);
    im.rect(10, 5, 2, 34, D);
    for (int y = 8; y < 36; y += 9) { im.rect(3, y, 10, 2, M); im.rect(3, y, 10, 1, L); im.rect(3, y + 1, 10, 1, D); }  // carved bands
    im.rect(1, 1, 14, 5, M); im.rect(1, 1, 14, 1, L); im.rect(1, 5, 14, 1, DD); im.rect(12, 1, 3, 5, D);               // capital
    for (int i = 0; i < 4; i++) im.set(5 + i, 14 + (i % 2) * 7, HEX(0xffffff));
    im.line(9, 18, 8, 24, DD);  // a crack
    im.outline(OUT);
    return im;
}

inline Img chest(int tier, bool open) {  // 0 plain wood, 1 iron-bound, 2 locked (silver), 3 royal (gold + ice)
    Img im(20, 18);
    const u32 wood[4] = {HEX(0x8a5a2c), HEX(0x6a4a30), HEX(0x5a6078), HEX(0x3a5a98)};
    const u32 woodD[4] = {HEX(0x5e3d1c), HEX(0x4a3220), HEX(0x3a4058), HEX(0x22386a)};
    const u32 band[4] = {HEX(0x8a7a50), HEX(0xa0a8b8), HEX(0xdfe6f2), HEX(0xf2d060)};
    const u32 W = wood[tier], WD = woodD[tier], B = band[tier];
    im.rect(1, 8, 18, 9, W);                       // body
    im.rect(1, 8, 18, 1, shadeC(W, 1.3f));
    im.rect(1, 15, 18, 2, WD);
    im.rect(1, 8, 2, 9, WD); im.rect(17, 8, 2, 9, WD);
    im.rect(4, 8, 2, 9, B); im.rect(14, 8, 2, 9, B);  // bands
    if (!open) {
        im.rect(1, 3, 18, 6, W);                    // lid
        im.rect(1, 3, 18, 1, shadeC(W, 1.35f));
        im.rect(1, 7, 18, 2, WD);
        im.rect(4, 3, 2, 6, B); im.rect(14, 3, 2, 6, B);
        im.rect(8, 7, 4, 4, B);                      // lock plate
        im.rect(9, 8, 2, 2, tier == 2 ? HEX(0x2a2e38) : HEX(0x241a14));
        if (tier >= 2) { im.set(9, 7, HEX(0xffffff)); im.set(2, 4, HEX(0xffffff)); }
    } else {
        im.rect(2, 1, 16, 3, WD);                    // lid thrown back
        im.rect(2, 1, 16, 1, W);
        im.rect(3, 8, 14, 3, HEX(0x241a14));         // dark inside ...
        im.rect(5, 9, 3, 1, tier >= 2 ? HEX(0xa8e4ff) : HEX(0xf2c94c));  // ... with a glint of treasure
        im.rect(10, 9, 4, 1, HEX(0xf2c94c));
    }
    im.outline(OUT);
    return im;
}

inline Img mineGate() {  // the Abandoned Mine's mouth: an outcrop with a timbered tunnel mouth, rails and a loaded cart (72 x 60)
    Img im(72, 60);
    const u32 rl = HEX(0x9a8c80), rm = HEX(0x6a5e56), rd = HEX(0x3a322e);
    im.ellipseShaded(36, 24, 34, 22, rl, rm, rd);
    for (int i = 0; i < 10; i++) im.ellipseShaded(8 + i * 6.2f, 16 + (i % 3) * 4, 8.f, 7.f, shadeC(rl, 0.95f + (i % 2) * 0.1f), rm, rd);
    im.rect(4, 36, 64, 22, rm);
    for (int y = 36; y < 58; y++)
        for (int x = 4; x < 68; x++) {
            if (nzf(x, y, 101) > 0.82f) im.set(x, y, rl);
            else if (nzf(x, y, 103) < 0.12f) im.set(x, y, rd);
        }
    im.rect(21, 22, 30, 36, HEX(0x0c0a0a));  // the dark opening
    im.ellipse(36, 22, 15, 6, HEX(0x0c0a0a));
    for (int y = 24; y < 58; y++) { im.set(22, y, HEX(0x1c1816)); im.set(49, y, HEX(0x1c1816)); }
    const u32 tl = HEX(0xa8723c), tm = HEX(0x7a5028), td = HEX(0x4a2e16);  // timber frame: two posts, a lintel, braces
    im.rect(16, 20, 6, 38, tm); im.rect(16, 20, 2, 38, tl); im.rect(20, 20, 2, 38, td);
    im.rect(50, 20, 6, 38, tm); im.rect(50, 20, 2, 38, tl); im.rect(54, 20, 2, 38, td);
    im.rect(13, 14, 46, 7, tm); im.rect(13, 14, 46, 2, tl); im.rect(13, 19, 46, 2, td);
    im.line(22, 30, 30, 21, tm); im.line(22, 31, 30, 22, td);
    im.line(49, 30, 41, 21, tm); im.line(49, 31, 41, 22, td);
    for (int i = 0; i < 5; i++) im.set(16 + i * 11, 17, HEX(0x241a14));  // nails
    im.rect(34, 21, 1, 4, HEX(0x2a2a30));                                 // a hanging lantern
    im.rect(32, 25, 5, 6, HEX(0x3a3a44)); im.rect(33, 26, 3, 4, HEX(0xffc860)); im.set(34, 27, HEX(0xffffff));
    for (int y = 40; y < 59; y += 3) im.rect(26, y, 20, 1, HEX(0x6a4a2a));  // rails and a cart
    im.rect(28, 40, 1, 19, HEX(0x8890a0)); im.rect(43, 40, 1, 19, HEX(0x8890a0));
    im.rect(55, 46, 13, 9, HEX(0x5a5e6a)); im.rect(55, 46, 13, 2, HEX(0x8a90a0)); im.rect(55, 53, 13, 2, HEX(0x2e323c));
    im.ellipseShaded(61, 46, 5.5f, 2.6f, HEX(0x6a6a78), HEX(0x34343c), HEX(0x1a1a20));
    im.rect(56, 55, 3, 3, HEX(0x1c1c22)); im.rect(64, 55, 3, 3, HEX(0x1c1c22));
    im.ellipseShaded(10, 52, 5.f, 3.f, rl, rm, rd);
    im.ellipseShaded(15, 55, 3.f, 2.f, rl, rm, rd);
    im.outline(OUT);
    return im;
}

inline Img statue(int v);
inline Img crystalCluster(int w, int h, u32 l, u32 m, u32 d, int seed);

inline Img frostGate() {  // the ancient frozen fortress: ice walls, two towers, a glowing gate, statues, crystals (112 x 84)
    Img im(112, 84);
    const u32 L = HEX(0xe8f8ff), M = HEX(0xa8d0ea), D = HEX(0x5a8ab4), DD = HEX(0x2a4a70);
    auto block = [&](int x, int y, int w, int h) {   // a lit ice-brick wall face
        for (int yy = 0; yy < h; yy++)
            for (int xx = 0; xx < w; xx++) {
                const float t = (float)yy / h;
                u32 c = mixc(HEX(0xc4e4f6), HEX(0x4a7aa8), t * 0.8f);
                if (nzf(x + xx, y + yy, 111) > 0.88f) c = shadeC(c, 1.12f);
                im.set(x + xx, y + yy, c);
            }
        for (int yy = 5; yy < h; yy += 7) im.rect(x, y + yy, w, 1, DD);
        for (int yy = 0, r = 0; yy < h; yy += 7, r++)
            for (int xx = (r % 2) * 6; xx < w; xx += 12) im.rect(x + xx, y + yy, 1, 6, DD);
        im.rect(x, y, w, 1, L);
    };
    block(6, 40, 100, 42);  // curtain wall
    for (int x = 6; x < 106; x += 10) { im.rect(x, 33, 7, 8, M); im.rect(x, 33, 7, 1, L); im.rect(x + 5, 33, 2, 8, D); }  // battlements
    for (int side = 0; side < 2; side++) {  // two towers
        const int tx = side ? 80 : 10;
        block(tx, 14, 22, 68);
        im.rect(tx - 3, 8, 28, 8, M); im.rect(tx - 3, 8, 28, 2, L); im.rect(tx - 3, 14, 28, 2, DD);
        for (int i = 0; i < 5; i++) { im.rect(tx - 3 + i * 6, 3, 4, 6, M); im.rect(tx - 3 + i * 6, 3, 4, 1, L); }
        im.rect(tx + 9, 24, 4, 9, HEX(0x0c1a30)); im.rect(tx + 9, 24, 4, 1, HEX(0x6ac8ff)); im.rect(tx + 10, 26, 2, 5, HEX(0x6ac8ff));   // arrow slits, glowing
        im.rect(tx + 9, 46, 4, 9, HEX(0x0c1a30)); im.rect(tx + 10, 48, 2, 5, HEX(0x4a98d8));
    }
    im.rect(38, 36, 36, 46, HEX(0x0a1428));  // the gate: a tall arch with a cold blue glow inside
    im.ellipse(56, 38, 18, 9, HEX(0x0a1428));
    for (int y = 40; y < 82; y++)
        for (int x = 40; x < 72; x++) {
            const float g = 1.f - fabsf(x - 56.f) / 18.f;
            const float v = g * (0.3f + 0.7f * (y - 40) / 42.f);
            if (v > 0.2f) im.set(x, y, mixc(HEX(0x0a1428), HEX(0x4aa8e8), std::min(1.f, v * 0.9f)));
        }
    for (int x = 42; x < 71; x += 5) im.rect(x, 36, 1, 40, HEX(0x6a7a98));   // portcullis, half raised
    for (int y = 40; y < 76; y += 9) im.rect(40, y, 32, 1, HEX(0x5a6a88));
    for (int i = 0; i < 9; i++) {  // arch stones and a carved skull above
        const float a = 3.14159f + i * 3.14159f / 8.f;
        const int ax2 = 56 + (int)(cosf(a) * 21), ay2 = 40 + (int)(sinf(a) * 12);
        im.rect(ax2 - 2, ay2 - 2, 5, 5, i % 2 ? M : L);
        im.rect(ax2, ay2 + 1, 3, 2, D);
    }
    im.rect(53, 22, 7, 9, M); im.rect(53, 22, 7, 1, L); im.rect(54, 25, 2, 2, DD); im.rect(57, 25, 2, 2, DD); im.rect(55, 28, 3, 2, DD);
    for (int i = 0; i < 4; i++) im.rect(36 - i * 2, 78 + i, 40 + i * 4, 1, i % 2 ? M : L);  // stairs to the threshold
    im.blit(statue(0), 20, 44);                       // flanking statues and glowing crystals
    im.blit(statue(2), 68, 44);
    im.blit(crystalCluster(18, 22, HEX(0xd8f4ff), HEX(0x4ac0ff), HEX(0x1a68b0), 1), 2, 62);
    im.blit(crystalCluster(18, 22, HEX(0xd8f4ff), HEX(0x4ac0ff), HEX(0x1a68b0), 2), 92, 62);
    for (int side = 0; side < 2; side++) {  // banners
        const int bx = side ? 66 : 42;
        im.rect(bx, 12, 6, 22, HEX(0x24408a)); im.rect(bx, 12, 6, 2, HEX(0xc8d8f0)); im.rect(bx + 2, 18, 2, 6, HEX(0xe8c860));
        for (int k = 0; k < 3; k++) im.set(bx + k * 2, 34 + (k % 2), HEX(0x24408a));
    }
    for (int i = 0; i < 12; i++) { const int x = 8 + (int)(nz(i, 0, 5) % 96); const int len = 2 + (int)(nz(i, 1, 5) % 5); im.rect(x, 41, 1, len, L); im.set(x, 41 + len, M); }  // icicles
    im.outline(OUT);
    return im;
}

inline Img throne() {  // the Frostbound King's seat: a tall ice throne on a dais (44 x 56)
    Img im(44, 56);
    const u32 L = HEX(0xe8f8ff), M = HEX(0xa0c8e8), D = HEX(0x5a88b4), DD = HEX(0x2a4a70);
    im.rect(2, 44, 40, 11, M); im.rect(2, 44, 40, 1, L); im.rect(2, 54, 40, 1, DD); im.rect(30, 44, 12, 11, D);  // dais
    im.rect(6, 38, 32, 7, M); im.rect(6, 38, 32, 1, L); im.rect(6, 44, 32, 1, DD);
    im.rect(10, 12, 24, 28, D);
    im.rect(10, 12, 24, 2, L);
    for (int i = 0; i < 5; i++) { const int x = 10 + i * 5; im.rect(x, 2 + (i % 2) * 2, 4, 11 - (i % 2) * 2, i % 2 ? M : L); im.rect(x + 2, 2 + (i % 2) * 2, 2, 11 - (i % 2) * 2, D); im.set(x, 1 + (i % 2) * 2, HEX(0xffffff)); }  // spiked crest
    im.rect(14, 16, 16, 22, HEX(0x24408a));  // blue cushion
    im.rect(14, 16, 16, 1, HEX(0x4a68b8)); im.rect(21, 22, 2, 8, HEX(0xe8c860));
    im.rect(6, 26, 6, 14, M); im.rect(32, 26, 6, 14, D); im.rect(6, 26, 6, 1, L);   // arms
    im.rect(14, 34, 16, 6, HEX(0x2e4a9a));
    for (int i = 0; i < 6; i++) im.set(12 + i * 4, 20 + (i * 5) % 14, HEX(0xffffff));
    im.outline(OUT);
    return im;
}

inline Img bossDoor(bool open) {  // the throne room's double doors (52 x 50)
    Img im(52, 50);
    const u32 L = HEX(0xdff0fb), M = HEX(0x8fb4d0), D = HEX(0x4a7aa4), DD = HEX(0x24406a);
    im.rect(0, 10, 52, 40, M); im.rect(0, 10, 52, 1, L); im.rect(0, 10, 4, 40, D); im.rect(48, 10, 4, 40, D);
    im.ellipse(26, 10, 26, 10, M);
    im.rect(4, 14, 44, 36, HEX(0x0a1428));
    if (!open) {
        for (int s = 0; s < 2; s++) {
            const int x0 = s ? 26 : 4;
            im.rect(x0, 14, 22, 36, HEX(0x3a5a88)); im.rect(x0, 14, 22, 2, HEX(0x6a8ab8));
            im.rect(x0 + 2, 18, 18, 28, HEX(0x2e4a74));
            for (int y = 20; y < 44; y += 6) im.rect(x0 + 3, y, 16, 1, HEX(0x5a7aa8));
            im.rect(s ? 26 : 21, 30, 2, 8, HEX(0xe8c860));
        }
        im.rect(22, 18, 8, 9, M); im.rect(22, 18, 8, 1, L); im.rect(24, 21, 2, 2, DD); im.rect(27, 21, 2, 2, DD); im.rect(25, 24, 2, 2, DD);  // skull seal
    } else {
        for (int y = 14; y < 50; y++)
            for (int x = 4; x < 48; x++) im.set(x, y, mixc(HEX(0x0a1428), HEX(0x3a88c8), 0.5f * (1.f - fabsf(x - 26.f) / 22.f) * (y - 14) / 36.f));
        im.rect(4, 14, 4, 36, HEX(0x3a5a88)); im.rect(44, 14, 4, 36, HEX(0x3a5a88));
    }
    for (int i = 0; i < 4; i++) im.rect(2 + i * 14, 4, 4, 8, i % 2 ? M : L);
    im.outline(OUT);
    return im;
}

inline Img stairs(bool down) {  // flat steps leading down (dark) or up (bright), 24 x 20
    Img im(24, 20);
    for (int i = 0; i < 5; i++) {
        const int y = 2 + i * 3;
        const float t = down ? 1.f - i / 4.f : i / 4.f;
        const int x0 = 2 + (down ? i : 4 - i), w = 20 - (down ? 2 * i : 2 * (4 - i));
        im.rect(x0, y, w, 3, mixc(HEX(0x1a2a44), HEX(0x8ab8d8), t));
        im.rect(x0, y, w, 1, mixc(HEX(0x34506c), HEX(0xe8f8ff), t));
    }
    im.rect(0, 0, 24, 2, HEX(0x5a88b4)); im.rect(0, 18, 24, 2, HEX(0x5a88b4));
    if (down) im.rect(8, 14, 8, 4, HEX(0x06101e)); else im.rect(8, 2, 8, 3, HEX(0xe8faff));
    im.outline(OUT);
    return im;
}
inline Img exitDoor(bool frost) {  // the way back up to the surface (32 x 34)
    Img im(32, 34);
    if (!frost) {
        im.rect(2, 4, 28, 30, HEX(0x7a5028)); im.rect(2, 4, 28, 2, HEX(0xa8723c));
        for (int y = 8; y < 34; y++) for (int x = 6; x < 26; x++) im.set(x, y, mixc(HEX(0xfff4c8), HEX(0x8ab8e8), (y - 8) / 26.f));  // daylight from above
        im.rect(14, 8, 3, 26, HEX(0x6a4a2a));
        for (int y = 12; y < 34; y += 5) im.rect(8, y, 16, 1, HEX(0x6a4a2a));  // a ladder
        im.rect(8, 8, 1, 26, HEX(0x6a4a2a)); im.rect(23, 8, 1, 26, HEX(0x6a4a2a));
    } else {
        im.rect(2, 4, 28, 30, HEX(0x8fb4d0)); im.rect(2, 4, 28, 2, HEX(0xe8f8ff));
        im.ellipse(16, 8, 11, 6, HEX(0x8fb4d0));
        for (int y = 10; y < 34; y++) for (int x = 6; x < 26; x++) im.set(x, y, mixc(HEX(0xffffff), HEX(0x9ad0f0), (y - 10) / 24.f));
        for (int i = 0; i < 4; i++) im.rect(4 + i * 7, 4, 3, 3, HEX(0xe8f8ff));
    }
    im.outline(OUT);
    return im;
}

inline Img mineCart() {
    Img im(26, 20);
    im.rect(2, 8, 22, 9, HEX(0x5a5e6a)); im.rect(2, 8, 22, 2, HEX(0x9aa0b0)); im.rect(2, 15, 22, 2, HEX(0x2e323c)); im.rect(21, 8, 3, 9, HEX(0x3e424e));
    im.rect(2, 12, 22, 1, HEX(0x3e424e));
    for (int i = 0; i < 4; i++) im.set(5 + i * 5, 10, HEX(0xc8ccd8));  // rivets
    im.ellipseShaded(12, 8, 10.f, 3.5f, HEX(0x7a6a5a), HEX(0x4a4036), HEX(0x2a241e));   // a heap of ore
    for (auto d : {std::pair<int, int>{7, 5}, {13, 4}, {17, 6}}) { im.rect(d.first, d.second, 2, 2, HEX(0xd8844a)); im.set(d.first, d.second, HEX(0xf0a868)); }
    im.rect(4, 17, 4, 3, HEX(0x1c1c22)); im.rect(17, 17, 4, 3, HEX(0x1c1c22)); im.set(5, 18, HEX(0x8890a0)); im.set(18, 18, HEX(0x8890a0));
    im.outline(OUT);
    return im;
}

inline Img torch(int frame) {  // a wall torch in an iron sconce, 3 flame frames (10 x 22)
    Img im(10, 22);
    im.rect(4, 10, 2, 11, HEX(0x6a4a2a)); im.rect(4, 10, 1, 11, HEX(0x8a643a));
    im.rect(2, 9, 6, 3, HEX(0x3a3a44)); im.rect(2, 9, 6, 1, HEX(0x6a6a78));
    const int fh = 5 + (frame % 3);
    for (int y = 0; y < fh; y++) {
        const float t = (float)y / fh;
        const int w = y < 2 ? 2 : (y < fh - 1 ? 4 : 2);
        const int x0 = 5 - w / 2 + (frame == 1 ? (y > 2 ? 1 : 0) : (frame == 2 ? (y > 2 ? -1 : 0) : 0));
        im.rect(x0, 8 - y, w, 1, t < 0.3f ? HEX(0xffe880) : t < 0.7f ? HEX(0xffa030) : HEX(0xe05a20));
    }
    im.set(5, 7, HEX(0xffffff));
    im.outline(OUT);
    return im;
}
inline Img brazier(int frame) {  // an iron brazier burning cold blue fire (14 x 24)
    Img im(14, 24);
    im.rect(6, 12, 2, 10, HEX(0x3a3e4a)); im.rect(3, 21, 8, 2, HEX(0x2a2e38));
    im.ellipseShaded(7, 11, 6.f, 2.8f, HEX(0x8a90a0), HEX(0x4a4e5c), HEX(0x2a2e38));
    im.ellipse(7, 10, 4.8f, 1.8f, HEX(0x0a1830));
    const int fh = 7 + (frame % 3) * 2;
    for (int y = 0; y < fh; y++) {
        const float t = (float)y / fh;
        const int w = y < 2 ? 5 : (t < 0.7f ? 4 : 2);
        const int x0 = 7 - w / 2 + (frame == 1 && y > 3 ? 1 : (frame == 2 && y > 3 ? -1 : 0));
        im.rect(x0, 10 - y, w, 1, t < 0.3f ? HEX(0xe8faff) : t < 0.7f ? HEX(0x6ac8ff) : HEX(0x2a78d8));
    }
    im.outline(OUT);
    return im;
}
inline Img sarcophagus() {
    Img im(36, 24);
    const u32 L = HEX(0xdff0fb), M = HEX(0xa0bcd4), D = HEX(0x5a7898), DD = HEX(0x2e4660);
    im.rect(1, 8, 34, 14, M); im.rect(1, 8, 34, 2, L); im.rect(1, 20, 34, 2, DD); im.rect(29, 8, 6, 14, D);
    im.rect(3, 4, 30, 6, M); im.rect(3, 4, 30, 1, L); im.rect(3, 9, 30, 1, D);       // lid
    im.ellipseShaded(9, 8, 4.f, 3.2f, L, M, D);                                         // carved head
    im.rect(14, 6, 16, 2, D); im.rect(16, 11, 12, 1, DD); im.rect(18, 14, 8, 1, DD);
    im.set(8, 7, DD); im.set(10, 7, DD);
    im.rect(0, 22, 36, 2, HEX(0x1c3452));
    im.outline(OUT);
    return im;
}
inline Img beamPost() {  // an overhead timber arch across a three tile corridor (48 x 40): posts and a lintel; drawn above the hunter
    Img im(48, 40);
    const u32 tl = HEX(0xa8723c), tm = HEX(0x7a5028), td = HEX(0x4a2e16);
    im.rect(0, 0, 48, 7, tm); im.rect(0, 0, 48, 2, tl); im.rect(0, 5, 48, 2, td);
    im.rect(0, 0, 5, 40, tm); im.rect(0, 0, 2, 40, tl); im.rect(3, 0, 2, 40, td);
    im.rect(43, 0, 5, 40, tm); im.rect(43, 0, 2, 40, tl); im.rect(46, 0, 2, 40, td);
    im.line(5, 20, 14, 7, tm); im.line(5, 21, 14, 8, td);
    im.line(42, 20, 33, 7, tm); im.line(42, 21, 33, 8, td);
    for (int i = 0; i < 3; i++) im.set(12 + i * 12, 3, HEX(0x241a14));
    im.outline(OUT);
    return im;
}
inline Img beamSide() {  // a cross beam seen along a horizontal corridor (16 x 52): two stubby posts and a beam running between them
    Img im(16, 52);
    const u32 tl = HEX(0xa8723c), tm = HEX(0x7a5028), td = HEX(0x4a2e16);
    im.rect(4, 4, 8, 44, tm); im.rect(4, 4, 3, 44, tl); im.rect(10, 4, 2, 44, td);
    im.rect(1, 0, 14, 8, tm); im.rect(1, 0, 14, 2, tl); im.rect(1, 6, 14, 2, td);
    im.rect(1, 44, 14, 8, tm); im.rect(1, 44, 14, 2, tl); im.rect(1, 50, 14, 2, td);
    im.outline(OUT);
    return im;
}
inline Img equip(int v) {  // 0 pickaxe leaning on a crate, 1 shovel and bucket, 2 lantern on a barrel, 3 coiled rope and tools
    Img im(20, 22);
    const u32 wl = HEX(0x9a6a3a), wd = HEX(0x5e3d1c), st = HEX(0xaab0bb), sd = HEX(0x5a5e6a);
    if (v == 0) {
        im.rect(2, 12, 12, 9, wl); im.rect(2, 12, 12, 1, shadeC(wl, 1.3f)); im.rect(2, 19, 12, 2, wd); im.rect(2, 15, 12, 1, wd);
        im.line(15, 20, 12, 3, wd); im.line(16, 20, 13, 3, wl);
        im.line(9, 3, 17, 5, st); im.line(9, 4, 17, 6, sd); im.set(8, 4, st); im.set(18, 6, sd);
    } else if (v == 1) {
        im.line(5, 20, 8, 3, wd); im.line(6, 20, 9, 3, wl);
        im.rect(6, 1, 6, 4, st); im.rect(6, 1, 6, 1, HEX(0xdfe6f2)); im.rect(6, 4, 6, 1, sd);
        im.ellipseShaded(15, 16, 4.f, 4.5f, HEX(0x8a7a6a), HEX(0x5a4e42), HEX(0x34302a));
        im.ellipse(15, 13, 3.2f, 1.2f, HEX(0x241a14));
    } else if (v == 2) {
        im.rect(3, 9, 12, 12, HEX(0x7a5030)); im.rect(3, 9, 12, 1, HEX(0xa8723c)); im.rect(3, 13, 12, 1, HEX(0x3a3a44)); im.rect(3, 18, 12, 1, HEX(0x3a3a44)); im.rect(12, 9, 3, 12, HEX(0x4a2e16));
        im.rect(7, 4, 5, 5, HEX(0x3a3a44)); im.rect(8, 5, 3, 3, HEX(0xffc860)); im.set(8, 5, HEX(0xffffff)); im.rect(8, 2, 3, 2, HEX(0x6a6a78));
    } else {
        im.ellipseShaded(8, 15, 7.f, 4.f, HEX(0xd8c090), HEX(0xa88858), HEX(0x6a5030));
        im.ellipse(8, 14, 3.f, 1.2f, HEX(0x241a14));
        for (int i = 0; i < 4; i++) im.line(2 + i * 3, 14, 4 + i * 3, 18, HEX(0x7a6038));
        im.line(15, 20, 17, 8, wd); im.rect(15, 5, 5, 3, st); im.set(15, 5, HEX(0xdfe6f2));
    }
    im.outline(OUT);
    return im;
}
inline Img debris(int v) {
    Img im(20, 12);
    const u32 l = HEX(0x8a7c70), m = HEX(0x5e5248), d = HEX(0x342c28);
    if (v == 2) {  // broken planks
        im.line(2, 9, 16, 4, HEX(0x7a5028)); im.line(2, 10, 16, 5, HEX(0x4a2e16));
        im.line(4, 5, 17, 9, HEX(0x9a6a3a)); im.line(4, 6, 17, 10, HEX(0x5e3d1c));
        im.set(1, 9, HEX(0xc8a070));
    } else {
        im.ellipseShaded(7, 7, 5.5f, 3.8f, l, m, d);
        im.ellipseShaded(13, 8, 4.5f, 3.f, l, m, d);
        im.ellipseShaded(10, 5, 3.2f, 2.4f, shadeC(l, 1.1f), m, d);
        if (v == 1) { im.ellipseShaded(16, 9, 2.5f, 1.8f, l, m, d); im.set(5, 6, HEX(0xdfe6f2)); }
    }
    im.outline(OUT);
    return im;
}
inline Img bones(int v) {
    Img im(18, 10);
    const u32 L = HEX(0xf2ead2), M = HEX(0xc8bea0), D = HEX(0x8a8068);
    im.line(2, 7, 11, 4, M); im.line(2, 6, 11, 3, L); im.rect(1, 6, 2, 2, L); im.rect(10, 2, 2, 2, L);
    im.line(5, 3, 14, 8, M); im.line(5, 2, 14, 7, L); im.rect(13, 7, 2, 2, L);
    if (v) { im.ellipseShaded(14, 4, 3.f, 2.6f, L, M, D); im.set(13, 4, D); im.set(15, 4, D); }
    im.outline(OUT);
    return im;
}
inline Img banner() {
    Img im(14, 40);
    im.rect(0, 0, 14, 3, HEX(0x6a6a78)); im.rect(0, 0, 14, 1, HEX(0xaab0bb));
    im.rect(1, 3, 12, 30, HEX(0x24408a)); im.rect(1, 3, 2, 30, HEX(0x3a5ab4)); im.rect(11, 3, 2, 30, HEX(0x16285a));
    im.rect(6, 9, 2, 12, HEX(0xdfe6f2)); im.rect(3, 13, 8, 2, HEX(0xdfe6f2)); im.set(7, 8, HEX(0xffffff));
    for (int i = 0; i < 4; i++) im.rect(1 + i * 3, 33, 3, 3 + (i % 2) * 3, HEX(0x24408a));
    im.outline(OUT);
    return im;
}
inline Img icicles() {
    Img im(16, 16);
    for (int i = 0; i < 4; i++) {
        const int x = 2 + i * 3 + (i % 2), h = 6 + (int)(nz(i, 4, 8) % 6);
        for (int y = 0; y < h; y++) {
            const int w = y < h / 2 ? 3 : (y < h - 2 ? 2 : 1);
            im.rect(x - w / 2 + 1, 15 - y, w, 1, y % 3 == 0 ? HEX(0xe8faff) : HEX(0x9ad0f0));
        }
    }
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------ floor traps (frames: 0 safe, 1 warning, 2 sprung) and attack effects
inline Img trapSpike(int state) {
    Img im(16, 16);
    im.rect(1, 1, 14, 14, HEX(0x2e3c58)); im.rect(1, 1, 14, 1, HEX(0x6a88b0)); im.rect(1, 14, 14, 1, HEX(0x16203a));
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) {
            const int x = 3 + a * 4, y = 4 + b * 4;
            im.rect(x, y, 2, 2, HEX(0x0c1428));
            if (state == 1) im.set(x, y, HEX(0xff8a50));
            if (state == 2) {
                im.rect(x, y - 3, 2, 5, HEX(0xdfe6f2)); im.set(x, y - 3, HEX(0xffffff)); im.rect(x + 1, y - 1, 1, 3, HEX(0x7a8aa8));
            }
        }
    if (state == 0) im.set(2, 2, HEX(0x8ab4d8));
    return im;
}
inline Img trapVent(int state) {
    Img im(16, 16);
    im.ellipseShaded(8, 8, 7.f, 6.f, HEX(0x9ac4e0), HEX(0x6a98c0), HEX(0x34506c));
    im.ellipse(8, 8, 3.5f, 3.f, HEX(0x0a1830));
    if (state >= 1) { im.ellipse(8, 8, 3.5f, 3.f, HEX(0x6ac8ff)); im.set(7, 7, HEX(0xffffff)); }
    if (state == 2) { for (int i = 0; i < 5; i++) im.set(5 + (i * 2) % 7, 1 + i, HEX(0xe8faff)); im.rect(6, 0, 4, 3, HEX(0xdff4ff)); }
    return im;
}
inline Img iceSpikeFx(int frame) {  // a burst of ice spikes (boss and mage attacks), 3 growth frames (22 x 30)
    Img im(22, 30);
    const int hmax[3] = {10, 20, 26};
    const int h = hmax[frame];
    for (int i = 0; i < 5; i++) {
        const int x = 3 + i * 4 + (i % 2), hh = std::max(4, h - (i % 2) * 5 - (i == 2 ? 0 : 3));
        for (int y = 0; y < hh; y++) {
            const int w = y > hh - 4 ? 1 : (y > hh - 10 ? 2 : 3);
            im.rect(x - w / 2 + 1, 28 - y, w, 1, y % 5 == 0 ? HEX(0xe8faff) : (x - w / 2 < 8 ? HEX(0xb8e4ff) : HEX(0x5ab0e8)));
        }
        im.set(x + 1, 28 - hh + 1, HEX(0xffffff));
    }
    im.ellipse(11, 28, 9.f, 2.f, HEX(0x9ad0f0));
    im.outline(OUT);
    return im;
}
inline Img iceBolt() {  // a glowing shard thrown by mages and the king: drawn pointing right, rotated by its angle
    Img im(14, 8);
    for (int x = 1; x < 13; x++) {
        const int h = x < 8 ? 1 + x / 3 : std::max(0, 3 - (x - 7) / 2);
        im.rect(x, 4 - h, 1, h * 2, x > 9 ? HEX(0xffffff) : (x > 5 ? HEX(0xb8e8ff) : HEX(0x4ab0f0)));
    }
    im.rect(0, 3, 2, 2, HEX(0x2a78d8));
    return im;
}
inline Img rockChunk() {
    Img im(12, 12);
    im.ellipseShaded(6, 6, 5.f, 5.f, HEX(0x9a8c80), HEX(0x6a5e56), HEX(0x34302a));
    im.set(4, 4, HEX(0xc8b8a8));
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------ item icons (16 x 16)
inline Img iconDelveMat(int m) {
    Img im(16, 16);
    auto gem = [&](int cx, int cy, u32 l, u32 mm, u32 d, int r) {
        for (int y = -r; y <= r; y++)
            for (int x = -r; x <= r; x++) {
                if (abs(x) + abs(y) > r + r / 2) continue;
                im.set(cx + x, cy + y, (x + y < -r / 2) ? l : (x + y > r / 2 ? d : mm));
            }
        im.set(cx - r / 2, cy - r / 2, HEX(0xffffff));
    };
    auto lump = [&](int cx, int cy, int rx, int ry, u32 l, u32 mm, u32 d) { im.ellipseShaded((float)cx, (float)cy, (float)rx, (float)ry, l, mm, d); };
    auto bar = [&](u32 l, u32 mm, u32 d) {
        im.rect(2, 7, 12, 6, mm); im.rect(2, 7, 12, 1, l); im.rect(2, 12, 12, 1, d); im.rect(13, 7, 1, 6, d);
        im.rect(4, 4, 8, 4, mm); im.rect(4, 4, 8, 1, l); im.rect(11, 4, 1, 4, d);
        im.set(3, 8, HEX(0xffffff));
    };
    switch (m) {
        case M_COAL: lump(6, 9, 4, 3, HEX(0x6a6a78), HEX(0x2e2e36), HEX(0x14141a)); lump(11, 8, 3, 3, HEX(0x6a6a78), HEX(0x2e2e36), HEX(0x14141a)); lump(8, 5, 3, 2, HEX(0x7a7a88), HEX(0x34343c), HEX(0x14141a)); im.set(5, 8, HEX(0xffffff)); break;
        case M_SILVER: lump(8, 9, 6, 4, HEX(0x8a8478), HEX(0x5e584e), HEX(0x34302a)); for (auto d : {std::pair<int, int>{5, 8}, {9, 7}, {7, 11}, {11, 10}}) { im.rect(d.first, d.second, 2, 2, HEX(0xdfe6f2)); im.set(d.first, d.second, HEX(0xffffff)); } break;
        case M_RARECRYSTAL: for (int i = 0; i < 3; i++) { const int x = 4 + i * 4, y0 = i == 1 ? 3 : 6, hh = i == 1 ? 10 : 7; im.rect(x, y0, 3, hh, HEX(0xb070ff)); im.rect(x, y0, 1, hh, HEX(0xe8c8ff)); im.rect(x + 2, y0 + 2, 1, hh - 2, HEX(0x6a3aa8)); } im.set(9, 3, HEX(0xffffff)); break;
        case M_GEM: gem(8, 8, HEX(0xffd0e8), HEX(0xff4a98), HEX(0x9a1c62), 5); break;
        case M_STEELBAR: bar(HEX(0xe8eef8), HEX(0x8a94a8), HEX(0x4a5266)); break;
        case M_SILVERBAR: bar(HEX(0xffffff), HEX(0xc8d2e4), HEX(0x7a849a)); break;
        case M_BATWING: for (int i = 0; i < 5; i++) im.line(2, 13, 4 + i * 3, 3 + (i % 2) * 2, HEX(0x3a2a3a)); im.rect(3, 6, 10, 6, HEX(0x5a4258)); im.rect(3, 6, 10, 1, HEX(0x8a6a86)); for (int x = 3; x < 13; x += 3) im.rect(x, 12, 2, 1, HEX(0x2a1a2a)); break;
        case M_SILK: for (int i = 0; i < 5; i++) im.line(3, 3 + i * 2, 13, 5 + i * 2 - (i % 2) * 2, i % 2 ? HEX(0xe8eaf2) : HEX(0xc8ccd8)); im.line(3, 3, 3, 13, HEX(0xdfe2ec)); break;
        case M_SPIDERFANG: for (int y = 2; y < 14; y++) { const int w = std::max(1, 4 - y / 3); im.rect(5 + y / 4, y, w, 1, y < 4 ? HEX(0x5a4a3a) : (y > 10 ? HEX(0xe8e0c8) : HEX(0xb8a888))); } im.set(6, 6, HEX(0x7ae06a)); im.set(7, 12, HEX(0x7ae06a)); break;
        case M_GOLEMCORE: lump(8, 8, 6, 6, HEX(0x9a8c80), HEX(0x6a5e56), HEX(0x34302a)); im.ellipseShaded(8, 8, 3.2f, 3.2f, HEX(0xfff0a0), HEX(0xffa030), HEX(0xc04a10)); im.set(7, 7, HEX(0xffffff)); break;
        case M_CAVEPELT: im.ellipseShaded(8, 9, 6.5f, 4.5f, HEX(0x7a7a84), HEX(0x4a4a54), HEX(0x2a2a34)); for (int i = 0; i < 6; i++) im.set(4 + (i * 5) % 9, 6 + (i * 3) % 6, HEX(0x9a9aa8)); im.rect(1, 6, 2, 3, HEX(0x4a4a54)); im.rect(13, 6, 2, 3, HEX(0x4a4a54)); break;
        case M_ICEBONE: im.line(3, 12, 12, 3, HEX(0xc8e8f8)); im.line(3, 11, 11, 3, HEX(0xf4fcff)); im.rect(1, 11, 3, 3, HEX(0xdff4ff)); im.rect(11, 1, 3, 3, HEX(0xdff4ff)); im.set(2, 12, HEX(0x6ac8ff)); break;
        case M_FROSTFANG: for (int y = 2; y < 14; y++) { const int w = std::max(1, 4 - y / 3); im.rect(5 + y / 4, y, w, 1, y < 4 ? HEX(0x8ab4d8) : (y > 10 ? HEX(0xffffff) : HEX(0xc8ecff))); } im.set(6, 5, HEX(0x6ac8ff)); break;
        case M_ICESHARD: for (int i = 0; i < 3; i++) { const int x = 3 + i * 4; for (int y = 0; y < 10 - i * 2; y++) { const int w = std::max(1, 3 - y / 3); im.rect(x, 13 - y, w, 1, y % 4 == 0 ? HEX(0xffffff) : HEX(0x8fd8ff)); } } break;
        case M_KNIGHTPLATE: im.rect(3, 3, 10, 9, HEX(0x7a8aa8)); im.rect(3, 3, 10, 2, HEX(0xdfe6f2)); im.rect(3, 10, 10, 2, HEX(0x34406a)); im.rect(7, 3, 2, 9, HEX(0xdfe6f2)); im.rect(6, 12, 4, 2, HEX(0x5a6a88)); im.set(4, 4, HEX(0xffffff)); for (int i = 0; i < 3; i++) im.set(4 + i * 3, 8, HEX(0x6ac8ff)); break;
        case M_FROSTRUNE: im.ellipseShaded(8, 8, 6.f, 6.5f, HEX(0x9aa8b8), HEX(0x5a6678), HEX(0x2a3040)); im.line(8, 4, 8, 12, HEX(0x6ae0ff)); im.line(5, 6, 11, 10, HEX(0x6ae0ff)); im.line(11, 6, 5, 10, HEX(0x6ae0ff)); im.set(8, 8, HEX(0xffffff)); break;
        case M_ETERNALICE: for (auto d : {std::array<int, 3>{3, 7, 4}, std::array<int, 3>{7, 2, 5}, std::array<int, 3>{11, 7, 3}}) { im.rect(d[0], d[1], d[2], 13 - d[1], HEX(0x5ac0f8)); im.rect(d[0], d[1], 1, 13 - d[1], HEX(0xe8faff)); im.rect(d[0] + d[2] - 1, d[1] + 1, 1, 12 - d[1], HEX(0x2a78b8)); } im.set(8, 2, HEX(0xffffff)); break;
        default: im.rect(5, 2, 4, 4, HEX(0xdff4ff)); im.rect(6, 5, 2, 9, HEX(0xdff4ff)); im.rect(6, 9, 4, 2, HEX(0xdff4ff)); im.rect(6, 12, 4, 2, HEX(0xdff4ff)); im.set(6, 3, HEX(0x6ac8ff)); break;  // M_CRYPTKEY
    }
    im.outline(OUT);
    return im;
}
inline Img iconDelveCarry(int c) {  // CG_LANTERN .. CG_HEART (carried gear ids 7..12)
    Img im(16, 16);
    switch (c) {
        case 7:  // miner's lantern
            im.rect(6, 1, 4, 2, HEX(0x6a6a78)); im.rect(7, 3, 2, 1, HEX(0x6a6a78));
            im.rect(4, 4, 8, 10, HEX(0x3a3a44)); im.rect(5, 5, 6, 8, HEX(0xffc860)); im.rect(5, 5, 2, 8, HEX(0xfff0a0)); im.rect(4, 13, 8, 1, HEX(0x241a14));
            im.rect(8, 6, 1, 6, HEX(0x3a3a44)); im.set(6, 6, HEX(0xffffff));
            break;
        case 8:  // gem ring
            im.ellipseShaded(8, 10, 5.f, 4.5f, HEX(0xfff0a0), HEX(0xe0c050), HEX(0xa07a20));
            im.ellipse(8, 11, 2.8f, 2.4f, 0u);
            im.rect(6, 3, 5, 4, HEX(0xff4a98)); im.rect(6, 3, 2, 2, HEX(0xffd0e8)); im.rect(9, 5, 2, 2, HEX(0x9a1c62)); im.set(6, 3, HEX(0xffffff));
            break;
        case 9:  // frostward cloak
            im.rect(3, 3, 10, 11, HEX(0x7aa8d0)); im.rect(3, 3, 10, 3, HEX(0xdff4ff)); im.rect(3, 12, 10, 2, HEX(0x34608a)); im.rect(7, 4, 2, 10, HEX(0x5a88b4)); im.rect(1, 6, 2, 8, HEX(0x6a98c0)); im.rect(13, 6, 2, 8, HEX(0x4a78a0));
            im.rect(6, 3, 4, 2, HEX(0xf2e8b0)); im.set(4, 4, HEX(0xffffff)); im.set(11, 4, HEX(0xffffff));
            break;
        case 10:  // silver charm
            im.rect(7, 1, 2, 4, HEX(0x8a94aa)); im.ellipseShaded(8, 9, 5.5f, 5.5f, HEX(0xffffff), HEX(0xc8d2e4), HEX(0x7a849a));
            im.rect(7, 6, 2, 7, HEX(0x6a748a)); im.rect(5, 8, 6, 2, HEX(0x6a748a)); im.set(6, 6, HEX(0xffffff));
            break;
        case 11:  // the king's crown
            im.rect(2, 8, 12, 5, HEX(0xdfe6f2)); im.rect(2, 8, 12, 1, HEX(0xffffff)); im.rect(2, 12, 12, 1, HEX(0x7a849a));
            for (int i = 0; i < 4; i++) { im.rect(2 + i * 3 + (i > 1 ? 1 : 0), 3 + (i % 2), 2, 6, HEX(0xdff4ff)); im.set(2 + i * 3 + (i > 1 ? 1 : 0), 2 + (i % 2), HEX(0xffffff)); }
            im.rect(7, 9, 2, 2, HEX(0x4ac0ff)); im.set(7, 9, HEX(0xffffff)); im.set(3, 10, HEX(0x6ac8ff)); im.set(12, 10, HEX(0x6ac8ff));
            break;
        default:  // the frozen heart
            im.ellipseShaded(5.5f, 6, 3.8f, 3.8f, HEX(0xdff4ff), HEX(0x6ac8ff), HEX(0x2a78b8));
            im.ellipseShaded(10.5f, 6, 3.8f, 3.8f, HEX(0xdff4ff), HEX(0x6ac8ff), HEX(0x2a78b8));
            for (int y = 0; y < 6; y++) im.rect(2 + y, 8 + y, 12 - 2 * y, 1, y < 2 ? HEX(0x6ac8ff) : HEX(0x2a78b8));
            im.set(4, 4, HEX(0xffffff)); im.set(9, 4, HEX(0xffffff)); im.line(8, 6, 7, 10, HEX(0xe8faff));
            break;
    }
    im.outline(OUT);
    return im;
}

}  // namespace dart

// ----------------------------------------------------------------- the asset set
struct DelveAssets {
    Spr cfloor[4], cwallFace[3], cwallTop[3], rail[2], ifloor[4], iwallFace[3], iwallTop[3], brick[3], carpet[2];
    Spr mineGate, frostGate, beamArch, beamSide, cart, torch[3], brazier[3], crystalViolet[3], crystalIce[3], statue[3], pillar;
    Spr chest[4][2], stairs[2], exitDoor[2], bossDoor[2], equip[4], debris[3], sarco, throne, bones[2], banner, icicles;
    Spr spike[3], vent[3], iceSpike[3], iceBolt, rockChunk;
    Spr node[5];
};

inline void buildDelveAssets(SDL_Renderer* r, DelveAssets& d) {
    using namespace dart;
    for (int i = 0; i < 4; i++) d.cfloor[i] = makeSpr(r, caveFloor(i), 0, 0), d.ifloor[i] = makeSpr(r, iceFloor(i), 0, 0);
    for (int i = 0; i < 3; i++) {
        d.cwallFace[i] = makeSpr(r, caveWallFace(i), 0, 0);
        d.cwallTop[i] = makeSpr(r, caveWallTop(i), 0, 0);
        d.iwallFace[i] = makeSpr(r, iceWallFace(i), 0, 0);
        d.iwallTop[i] = makeSpr(r, iceWallTop(i), 0, 0);
        d.brick[i] = makeSpr(r, brickFloor(i), 0, 0);
    }
    for (int i = 0; i < 2; i++) d.rail[i] = makeSpr(r, rail(i == 1), 0, 0), d.carpet[i] = makeSpr(r, carpet(i), 0, 0);
    d.mineGate = makeSpr(r, mineGate(), 36, 58);
    d.frostGate = makeSpr(r, frostGate(), 56, 82);
    d.beamArch = makeSpr(r, beamPost(), 24, 39);
    d.beamSide = makeSpr(r, beamSide(), 8, 40);
    d.cart = makeSpr(r, mineCart(), 13, 19);
    for (int i = 0; i < 3; i++) {
        d.torch[i] = makeSpr(r, torch(i), 5, 21);
        d.brazier[i] = makeSpr(r, brazier(i), 7, 22);
        d.crystalViolet[i] = makeSpr(r, crystalCluster(18, 22, HEX(0xf0d8ff), HEX(0xb070ff), HEX(0x5a2a98), i + 1), 9, 20);
        d.crystalIce[i] = makeSpr(r, crystalCluster(20, 24, HEX(0xd8f4ff), HEX(0x4ac0ff), HEX(0x1a68b0), i + 3), 10, 22);
        d.statue[i] = makeSpr(r, statue(i), 12, 36);
        d.spike[i] = makeSpr(r, trapSpike(i), 8, 8);
        d.vent[i] = makeSpr(r, trapVent(i), 8, 8);
        d.iceSpike[i] = makeSpr(r, iceSpikeFx(i), 11, 28);
        d.debris[i] = makeSpr(r, debris(i), 10, 10);
    }
    d.pillar = makeSpr(r, pillar(), 8, 42);
    for (int t = 0; t < 4; t++)
        for (int o = 0; o < 2; o++) d.chest[t][o] = makeSpr(r, chest(t, o == 1), 10, 17);
    for (int i = 0; i < 2; i++) {
        d.stairs[i] = makeSpr(r, stairs(i == 0), 12, 17);
        d.exitDoor[i] = makeSpr(r, exitDoor(i == 1), 16, 32);
        d.bossDoor[i] = makeSpr(r, bossDoor(i == 1), 26, 48);
        d.bones[i] = makeSpr(r, bones(i), 9, 8);
    }
    for (int i = 0; i < 4; i++) d.equip[i] = makeSpr(r, equip(i), 10, 20);
    d.sarco = makeSpr(r, sarcophagus(), 18, 22);
    d.throne = makeSpr(r, throne(), 22, 54);
    d.banner = makeSpr(r, banner(), 7, 38);
    d.icicles = makeSpr(r, icicles(), 8, 15);
    d.iceBolt = makeSpr(r, iceBolt(), 7, 4);
    d.rockChunk = makeSpr(r, rockChunk(), 6, 6);
    for (int i = 0; i < 5; i++) d.node[i] = makeSpr(r, nodeSprite(i), 9, 16);
}
