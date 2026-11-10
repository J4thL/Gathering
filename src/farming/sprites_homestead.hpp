// sprites_homestead.hpp - the art of the Homestead, drawn in code like everything else: building pieces, crops in five growth stages, the
// portal and the market stall, and the icons of the new materials and tools. Top-down 3/4 view like the town, light from the top-left,
// lit tops and shaded undersides, one pixel dark contour. (The farm animals are in dom_art.hpp; they use the 3D model renderer.)
#pragma once
#include "core/data.hpp"
#include "items/items.hpp"
#include "farming/homestead.hpp"

namespace hart {

inline uint32_t nz(int x, int y, uint32_t s) { return hash2(x, y, s); }
inline float nzf(int x, int y, uint32_t s) { return (nz(x, y, s) & 0xFFFF) / 65535.f; }
inline u32 lit(u32 c) { return shadeC(c, 1.28f); }
inline u32 drk(u32 c) { return shadeC(c, 0.68f); }

// ------------------------------------------------------------------ materials for building
struct Pal { u32 L, M, D, DD; };
inline Pal woodPal() { return {HEX(0xd9a866), HEX(0xb07a42), HEX(0x84552c), HEX(0x5a371c)}; }
inline Pal stonePal() { return {HEX(0xd2d4da), HEX(0xa4a7b0), HEX(0x76798a), HEX(0x4a4c5a)}; }

// ------------------------------------------------------------------ ground pieces (16 x 16)
inline Img floorWood(bool vert) {
    Img im(16, 16);
    const Pal P = woodPal();
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const int a = vert ? x : y;  // plank index runs across
            const float n = nzf(x / 3, y / 3, 5) * 0.5f + nzf(x, y, 9) * 0.5f;
            u32 c = P.M;
            if (a % 4 == 3) c = P.D;
            else if (n > 0.8f) c = P.L;
            else if (n < 0.2f) c = mixc(P.M, P.D, 0.5f);
            im.set(x, y, c);
        }
    for (int i = 0; i < 4; i++) {  // nails and joints
        const int j = 3 + (int)(nz(i, vert, 3) % 10);
        if (vert) im.set(1 + 4 * i, j, P.DD); else im.set(j, 1 + 4 * i, P.DD);
    }
    return im;
}
inline Img floorStone() {
    Img im(16, 16);
    const Pal P = stonePal();
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const float n = nzf(x, y, 21);
            im.set(x, y, n < 0.3f ? P.D : n < 0.85f ? P.M : P.L);
        }
    for (int i = 0; i < 16; i++) {  // flagstone joints
        im.set(i, 0, P.DD); im.set(i, 8, P.DD); im.set(0, i < 8 ? i : 8 + (i - 8), P.DD);
        im.set(8, i < 8 ? i : 8, P.DD);
    }
    for (int y = 8; y < 16; y++) { im.set(4, y, P.DD); im.set(12, y, P.DD); }
    for (int y = 0; y < 8; y++) im.set(8, y, P.DD);
    for (int x = 1; x < 15; x++) { if (x % 8) im.set(x, 1, P.L), im.set(x, 9, P.L); }
    return im;
}
inline Img foundation() {
    Img im(16, 16);
    const Pal P = stonePal();
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const float n = nzf(x, y, 33);
            u32 c = n < 0.25f ? P.D : n < 0.8f ? P.M : P.L;
            if (x == 0 || y == 0) c = P.L;
            if (x == 15 || y == 15) c = P.DD;
            im.set(x, y, c);
        }
    return im;
}
inline Img bridge(bool vert) {
    Img im(16, 16);
    const Pal P = woodPal();
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const int a = vert ? y : x;
            const bool rail = vert ? (x < 2 || x > 13) : (y < 2 || y > 13);
            u32 c = (a % 4 == 3) ? P.DD : (nzf(x, y, 7) > 0.78f ? P.L : P.M);
            if (rail) c = (vert ? x : y) % 15 < 1 ? P.DD : P.D;
            im.set(x, y, c);
        }
    return im;
}
inline Img plot(bool wet) {  // tilled soil: furrows with lit ridges
    Img im(16, 16);
    const u32 a = wet ? HEX(0x3c2818) : HEX(0x7a5532), b = wet ? HEX(0x2c1c10) : HEX(0x5e3f25), c = wet ? HEX(0x543a24) : HEX(0x94683f);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const int f = y % 4;
            u32 col = f == 0 ? c : f == 3 ? b : a;
            if (nzf(x, y, 41) > 0.9f) col = f < 2 ? lit(col) : drk(col);
            im.set(x, y, col);
        }
    for (int x = 0; x < 16; x++) { im.set(x, 0, wet ? HEX(0x6a4a30) : HEX(0xb58554)); }
    im.rect(0, 15, 16, 1, HEX(0x40301f)); im.rect(0, 0, 1, 16, wet ? HEX(0x4a3420) : HEX(0x6a4a2c)); im.rect(15, 0, 1, 16, HEX(0x3a2a1a));
    if (wet) for (int i = 0; i < 6; i++) im.set(2 + (int)(nz(i, 1, 3) % 12), 2 + (int)(nz(i, 2, 3) % 12), HEX(0x8ab8e0));
    return im;
}
inline Img roofTile() {  // overhead shingles (16 x 16), drawn over the player with some transparency
    Img im(16, 16);
    const u32 a = HEX(0xb4533a), b = HEX(0x8e3e2a), c = HEX(0xd2704e), d = HEX(0x5e2618);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const int row = y / 4, off = (row % 2) * 4;
            const int sx = (x + off) % 8;
            u32 col = (y % 4 == 3) ? d : (sx == 0 ? b : (y % 4 == 0 ? c : a));
            if (nzf(x, y, 71) > 0.92f) col = lit(col);
            im.set(x, y, col);
        }
    return im;
}

// ------------------------------------------------------------------ walls, fences, doors (16 wide, 16 tall face + cap)
inline Img wall(int mat, bool cap) {  // cap: a top surface above the face (the top of a run); otherwise the face only
    const Pal P = mat == 0 ? woodPal() : stonePal();
    const int ch = cap ? 5 : 0;
    Img im(16, 16 + ch + 2);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            const float t = y / 15.f;
            u32 c;
            if (mat == 0) {  // vertical timber boards
                c = (x % 4 == 3) ? P.D : mixc(P.L, P.M, t);
                if (nzf(x, y / 5, 13) > 0.85f) c = lit(c);
                if (y % 8 == 0 && x % 4 != 3) c = P.M;
            } else {  // stacked stone blocks
                const int row = y / 5, off = (row % 2) * 4;
                const bool joint = (y % 5 == 4) || ((x + off) % 8 == 7);
                c = joint ? P.D : mixc(P.L, P.M, t * 0.9f);
                if (!joint && nzf(x, y, 17) > 0.88f) c = lit(c);
            }
            if (y > 12) c = mixc(c, P.DD, (y - 12) / 6.f);
            im.set(x, y + ch + 1, c);
        }
    if (cap) {
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 16; x++) im.set(x, y + 1, y == 0 ? lit(P.L) : mat == 0 ? (x % 4 == 3 ? P.M : P.L) : (((x + y * 3) % 8 == 7) ? P.M : P.L));
        im.rect(0, 5, 16, 1, P.D);
    }
    im.outline(OUT);
    return im;
}
inline Img fence(bool vert) {
    const Pal P = woodPal();
    Img im(vert ? 8 : 16, vert ? 20 : 18);
    if (!vert) {
        for (int x = 0; x < 16; x += 14) { im.rect(x + 1, 3, 3, 14, P.M); im.rect(x + 1, 3, 1, 14, P.L); im.rect(x + 3, 3, 1, 14, P.D); im.rect(x + 1, 2, 3, 1, P.L); }
        im.rect(0, 6, 16, 3, P.M); im.rect(0, 6, 16, 1, P.L); im.rect(0, 8, 16, 1, P.D);
        im.rect(0, 11, 16, 3, P.M); im.rect(0, 11, 16, 1, P.L); im.rect(0, 13, 16, 1, P.D);
        im.rect(7, 3, 3, 14, P.M); im.rect(7, 3, 1, 14, P.L); im.rect(9, 3, 1, 14, P.D);
    } else {  // seen along its length: a post with the rails going away
        im.rect(2, 2, 4, 16, P.M); im.rect(2, 2, 1, 16, P.L); im.rect(5, 2, 1, 16, P.D);
        im.rect(1, 1, 6, 2, P.L);
        im.rect(2, 6, 4, 1, P.DD); im.rect(2, 11, 4, 1, P.DD);
    }
    im.outline(OUT);
    return im;
}
inline Img gate(bool vert, bool open) {
    const Pal P = woodPal();
    if (vert) {
        Img im(8, 20);
        im.rect(2, 2, 4, 16, open ? P.D : P.M); im.rect(2, 2, 1, 16, P.L); im.rect(1, 1, 6, 2, P.L);
        if (!open) { im.rect(2, 8, 4, 2, P.DD); im.set(4, 13, HEX(0xdfe6f2)); }
        im.outline(OUT);
        return im;
    }
    Img im(16, 18);
    if (!open) {
        im.rect(0, 3, 3, 14, P.D); im.rect(13, 3, 3, 14, P.D); im.rect(0, 2, 3, 1, P.L); im.rect(13, 2, 3, 1, P.L);
        im.rect(3, 5, 10, 2, P.M); im.rect(3, 12, 10, 2, P.M); im.rect(3, 5, 10, 1, P.L); im.rect(3, 12, 10, 1, P.L);
        for (int i = 0; i < 10; i++) im.set(3 + i, 7 + (i * 5) / 10, P.L), im.set(3 + i, 7 + (i * 5) / 10 + 1, P.D);  // diagonal brace
        im.set(8, 9, HEX(0xdfe6f2)); im.set(9, 9, HEX(0xdfe6f2));
    } else {  // swung open: the leaf stands edge-on against the left post
        im.rect(0, 3, 3, 14, P.D); im.rect(13, 3, 3, 14, P.D); im.rect(0, 2, 3, 1, P.L); im.rect(13, 2, 3, 1, P.L);
        im.rect(3, 4, 2, 13, P.M); im.rect(3, 4, 1, 13, P.L); im.rect(4, 8, 2, 2, P.DD);
    }
    im.outline(OUT);
    return im;
}
inline Img door(bool open) {
    const Pal P = woodPal();
    Img im(16, 24);
    im.rect(0, 2, 16, 22, HEX(0x6a4426));                 // frame
    im.rect(0, 2, 16, 2, P.M);
    if (!open) {
        for (int y = 4; y < 24; y++)
            for (int x = 2; x < 14; x++) im.set(x, y, (x % 4 == 1) ? P.D : mixc(P.L, P.M, (y - 4) / 20.f));
        im.rect(2, 11, 12, 2, P.DD); im.rect(2, 18, 12, 1, P.DD);
        im.set(11, 14, HEX(0xf2d060)); im.set(11, 15, HEX(0xb08a30));
    } else {
        im.rect(2, 4, 12, 20, HEX(0x241a14)); im.rect(3, 5, 10, 19, HEX(0x34261c));
        im.rect(2, 4, 3, 20, P.D); im.rect(2, 4, 1, 20, P.L);  // the leaf, seen edge-on
    }
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------ buildings (footprint w x h tiles; door always in the south wall)
struct BStyle { u32 wall, wallD, roof, roofD, roofL, trim, doorC; };
inline BStyle bstyle(int kind) {
    switch (kind) {
        case HP_SHED: return {HEX(0xb08a5a), HEX(0x84643a), HEX(0x6a6e7a), HEX(0x484c58), HEX(0x9096a4), HEX(0x5a3a1e), HEX(0x6a4426)};
        case HP_HOUSE: return {HEX(0xe8d4a8), HEX(0xbfa772), HEX(0xb8452e), HEX(0x84301e), HEX(0xd86a4a), HEX(0x6b4423), HEX(0x5a3a1e)};
        case HP_BARN: return {HEX(0xb8483a), HEX(0x8a2e26), HEX(0x5a5e6c), HEX(0x3a3e4a), HEX(0x7e8494), HEX(0xf2ead2), HEX(0xe6dcc0)};
        case HP_COOP: return {HEX(0xd4a06a), HEX(0xa07440), HEX(0x4a8a5a), HEX(0x2f6a40), HEX(0x6ab07a), HEX(0x6b4423), HEX(0x5a3a1e)};
        case HP_WORKSHOP: return {HEX(0xa8a8b4), HEX(0x7a7a88), HEX(0x2f6a7a), HEX(0x1e4a58), HEX(0x4a9aae), HEX(0x3a3a42), HEX(0x4a3a2a)};
        default: return {HEX(0xcab08a), HEX(0x9a7e58), HEX(0x7a5a8a), HEX(0x56406a), HEX(0x9a7aac), HEX(0x5a3a1e), HEX(0x6b4423)};  // warehouse
    }
}
inline Img building(int kind, int w, int h) {
    const BStyle S = bstyle(kind);
    const int PW = w * 16, FH = 22;                   // front wall height
    const int roofH = h * 16 + 12 - FH + 10;          // the roof fills the rest
    const int H = FH + roofH + 4;
    Img im(PW + 2, H);
    const int ox = 1, wallTop = roofH + 2;
    // roof (a pitched slab seen from above: lit left half, shaded right half, ridge line)
    for (int y = 1; y < roofH + 3; y++)
        for (int x = 0; x < PW; x++) {
            const float ty = (y - 1) / (float)(roofH + 2);
            const bool sh = x > PW * 0.62f;
            u32 c = sh ? S.roofD : mixc(S.roofL, S.roof, ty);
            const int row = (y - 1) / 4, off = (row % 2) * 3;
            if ((y - 1) % 4 == 3) c = drk(c);
            else if (((x + off) % 6) == 5) c = drk(c);
            if (nzf(x, y, 13) > 0.93f) c = lit(c);
            im.set(ox + x, y, c);
        }
    for (int x = 0; x < PW; x++) { im.set(ox + x, 1, lit(S.roofL)); im.set(ox + x, roofH + 2, S.roofD); }
    im.rect(ox, 0, PW, 1, S.trim);
    // wall
    for (int y = 0; y < FH; y++)
        for (int x = 0; x < PW; x++) {
            const float t = y / (float)(FH - 1);
            u32 c = mixc(S.wall, S.wallD, t * 0.7f);
            if (kind == HP_BARN || kind == HP_SHED || kind == HP_COOP) { if (x % 4 == 3) c = S.wallD; }
            else if (kind == HP_WORKSHOP || kind == HP_WAREHOUSE) { if ((y % 6 == 5) || ((x + (y / 6) * 5) % 10 == 9)) c = S.wallD; }
            else if (nzf(x, y, 5) > 0.9f) c = S.wallD;
            im.set(ox + x, wallTop + y, c);
        }
    im.rect(ox, wallTop, PW, 2, S.trim);                    // eaves shadow line
    im.rect(ox, wallTop + 2, PW, 2, drk(S.wall));
    im.rect(ox, wallTop + FH - 2, PW, 2, drk(S.wallD));
    // door (centre) and windows
    const int dw = kind == HP_BARN ? 14 : kind == HP_WAREHOUSE ? 12 : 8, dh = kind == HP_COOP ? 10 : 15;
    const int dx = PW / 2 - dw / 2, dy = wallTop + FH - dh;
    im.rect(ox + dx - 1, dy - 1, dw + 2, dh + 1, S.trim);
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) {
            u32 c = (kind == HP_BARN) ? ((x == dw / 2 || x == dw / 2 - 1) ? S.trim : mixc(S.doorC, drk(S.doorC), y / (float)dh)) : mixc(S.doorC, drk(S.doorC), y / (float)dh * 0.8f);
            if (kind == HP_WAREHOUSE) c = (y % 3 == 0) ? drk(S.doorC) : S.doorC;
            im.set(ox + dx + x, dy + y, c);
        }
    if (kind == HP_BARN) {  // white X braces on the big doors and a hay loft window
        for (int i = 0; i < dw / 2 - 1; i++) { im.set(ox + dx + 1 + i, dy + 1 + i * dh / (dw / 2), S.trim); im.set(ox + dx + dw / 2 - 1 - 1 - i + dw / 2, dy + 1 + i * dh / (dw / 2), S.trim); }
        im.rect(ox + dx + dw / 2 - 3, wallTop + 3, 6, 4, HEX(0x2a1a14)); im.rect(ox + dx + dw / 2 - 3, wallTop + 6, 6, 1, HEX(0xe0c060));
    } else if (kind != HP_COOP && kind != HP_WAREHOUSE) im.set(ox + dx + dw - 2, dy + dh / 2, HEX(0xf2d060));
    auto window = [&](int wx) {
        im.rect(ox + wx - 1, wallTop + 7, 8, 8, S.trim);
        im.rect(ox + wx, wallTop + 8, 6, 6, HEX(0x7ab8e0)); im.rect(ox + wx, wallTop + 8, 6, 2, HEX(0xc8e4f4));
        im.rect(ox + wx + 2, wallTop + 8, 1, 6, S.trim); im.rect(ox + wx, wallTop + 10, 6, 1, S.trim);
    };
    if (kind == HP_HOUSE || kind == HP_WORKSHOP) { window(3); if (PW - 11 > dx + dw) window(PW - 10); }
    if (kind == HP_HOUSE && w >= 4) window(dx - 12 > 3 ? dx - 12 : 3);
    if (kind == HP_HOUSE) {  // chimney with a bit of smoke-stain
        im.rect(ox + PW - 10, 0, 5, 8, HEX(0x8a7a70)); im.rect(ox + PW - 10, 0, 1, 8, HEX(0xb0a090)); im.rect(ox + PW - 11, 0, 7, 1, HEX(0x5a4a40));
    }
    if (kind == HP_WORKSHOP) {  // forge chimney and an anvil by the door
        im.rect(ox + 5, 0, 5, 10, HEX(0x6a6a74)); im.rect(ox + 5, 0, 1, 10, HEX(0x9a9aa8)); im.rect(ox + 4, 0, 7, 1, HEX(0x3a3a42));
        im.rect(ox + dx + dw + 2, wallTop + FH - 6, 6, 3, HEX(0x484852)); im.rect(ox + dx + dw + 3, wallTop + FH - 3, 4, 3, HEX(0x34343c));
    }
    if (kind == HP_COOP) {  // a little ramp and straw
        im.rect(ox + dx + 1, wallTop + FH - 3, dw - 2, 3, HEX(0xd8b858));
        for (int i = 0; i < 6; i++) im.set(ox + 3 + i * 2, wallTop + FH - 2, HEX(0xf0d878));
    }
    if (kind == HP_BARN || kind == HP_SHED) im.rect(ox, wallTop + FH - 1, PW, 1, S.wallD);
    // the front edge of the roof throws a shadow on the wall
    for (int x = 0; x < PW; x++) im.set(ox + x, wallTop + 4, drk(im.get(ox + x, wallTop + 4)));
    im.outline(OUT);
    return im;
}
inline Img greenhouse(int w, int h) {  // a glass frame: low front wall, corner posts and a glazed roof ridge; see-through (you walk inside)
    Img im(w * 16 + 2, h * 16 + 14);
    const u32 glass = RGBA(190, 230, 245, 70), glassHi = RGBA(255, 255, 255, 120), frame = HEX(0xe6e8ec), frameD = HEX(0x8a909c);
    const int PW = w * 16;
    // glazed back wall
    im.rect(1, 2, PW, 10, glass);
    for (int x = 0; x < PW; x += 8) im.rect(1 + x, 2, 1, 10, frame);
    im.rect(1, 2, PW, 1, frame); im.rect(1, 11, PW, 1, frameD);
    for (int x = 3; x < PW; x += 16) im.line(1 + x, 9, 1 + x + 4, 4, glassHi);
    // the ridge beam
    im.rect(1, 0, PW, 2, frameD); im.rect(1, 0, PW, 1, frame);
    // side posts
    for (int s = 0; s < 2; s++) {
        const int x = s ? PW - 1 : 1;
        im.rect(x, 2, 2, h * 16 + 11, frame); im.rect(x + (s ? 1 : 0), 2, 1, h * 16 + 11, frameD);
    }
    // the front low wall (glass over a stone sill) with a gap for the door
    const int fy = h * 16 + 1;
    im.rect(1, fy, PW, 5, HEX(0x8a8e98)); im.rect(1, fy, PW, 1, HEX(0xc4c8d0)); im.rect(1, fy + 4, PW, 1, HEX(0x5a5e68));
    im.rect(1, fy + 5, PW, 7, glass);
    for (int x = 0; x < PW; x += 8) im.rect(1 + x, fy + 5, 1, 7, frame);
    im.rect(1 + PW / 2 - 6, fy, 12, 12, 0u);  // the doorway: clear
    im.rect(1 + PW / 2 - 7, fy, 1, 12, frame); im.rect(1 + PW / 2 + 6, fy, 1, 12, frame);
    im.outline(HEX(0x4a505c));
    return im;
}

// ------------------------------------------------------------------ small furniture
inline Img well() {  // 2 x 2 tiles: round stone ring with a roof frame
    Img im(32, 44);
    const Pal P = stonePal();
    im.ellipseShaded(16, 32, 13, 8, P.L, P.M, P.D);
    im.ellipse(16, 31, 9, 5, HEX(0x1a3a5a)); im.ellipse(16, 31.5f, 7.5f, 3.5f, HEX(0x3a78b0)); im.rect(12, 30, 4, 1, HEX(0x9ad0f0));
    for (int s = 0; s < 2; s++) {
        const int x = s ? 25 : 5;
        im.rect(x, 10, 3, 24, HEX(0x8a5a30)); im.rect(x, 10, 1, 24, HEX(0xb07a42));
    }
    im.rect(4, 8, 24, 3, HEX(0x6a4426)); im.rect(4, 8, 24, 1, HEX(0xb07a42));
    im.rect(15, 11, 2, 12, HEX(0xd8c8a0));  // the rope
    im.rect(13, 22, 6, 5, HEX(0x8a5a30)); im.rect(13, 22, 6, 1, HEX(0xb07a42));  // the bucket
    for (int x = 3; x < 29; x++) { im.set(x, 6 - (x < 16 ? (x - 3) / 4 : (28 - x) / 4), HEX(0xb8452e)); }
    for (int y = 0; y < 4; y++) for (int x = 6 + y * 3; x < 26 - y * 3; x++) im.set(x, 4 + y, y == 0 ? HEX(0xd86a4a) : HEX(0xb8452e));
    im.outline(OUT);
    return im;
}
inline Img scarecrow() {
    Img im(22, 30);
    im.rect(10, 8, 2, 20, HEX(0x6a4426)); im.rect(10, 8, 1, 20, HEX(0x9a6a3a));
    im.rect(2, 12, 18, 2, HEX(0x6a4426)); im.rect(2, 12, 18, 1, HEX(0x9a6a3a));
    im.rect(6, 12, 10, 9, HEX(0xb8452e)); im.rect(6, 12, 3, 9, HEX(0xd86a4a)); im.rect(13, 12, 3, 9, HEX(0x84301e));  // shirt
    for (int i = 0; i < 4; i++) im.rect(2 + i * 5 / 2, 14 + (i % 2), 1, 4, HEX(0xe0c060)), im.rect(19 - i * 5 / 2, 14 + (i % 2), 1, 4, HEX(0xe0c060));  // straw
    im.ellipseShaded(11, 8, 4.5f, 4.5f, HEX(0xf0d8a0), HEX(0xd8b878), HEX(0xa8844a));
    im.set(9, 8, HEX(0x241a14)); im.set(13, 8, HEX(0x241a14)); im.rect(10, 10, 3, 1, HEX(0x241a14));
    im.rect(5, 2, 13, 2, HEX(0x6a4a2a)); im.rect(8, 0, 7, 3, HEX(0x84603a)); im.rect(8, 0, 7, 1, HEX(0xb08850));  // hat
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------ crops: 5 stages, 18 x 24, base at (9, 23)
inline Img crop(int c, int stage) {
    Img im(18, 24);
    const u32 G1 = HEX(0x4a9a3a), G2 = HEX(0x6ab84a), G3 = HEX(0x8ad868), GD = HEX(0x2e6a2a);
    auto leaf = [&](int cx, int cy, int len, float ang, u32 col) {
        const int ex = cx + (int)lroundf(cosf(ang) * len), ey = cy + (int)lroundf(-sinf(ang) * len);
        im.line(cx, cy, ex, ey, col);
        im.set(ex, ey, lit(col));
        if (len > 3) { im.set((cx + ex) / 2 + 1, (cy + ey) / 2, col); }
    };
    auto mound = [&](int w) { im.ellipseShaded(9, 22, w, 2.2f, HEX(0x94683f), HEX(0x7a5532), HEX(0x5e3f25)); };
    if (stage == 0) {  // freshly sown: a mound with a seed
        mound(5);
        im.set(8, 21, HEX(0xe8d8a0)); im.set(10, 22, HEX(0xe8d8a0));
        im.outline(OUT);
        return im;
    }
    if (stage == 1) {  // sprout
        mound(5);
        im.line(9, 21, 9, 18, G1); leaf(9, 18, 3, 2.4f, G2); leaf(9, 18, 3, 0.7f, G3);
        im.outline(OUT);
        return im;
    }
    mound(6);
    const int H = stage == 2 ? 8 : stage == 3 ? 12 : 14;
    switch (c) {
        case CR_WHEAT:
            for (int i = 0; i < 6; i++) {
                const int x = 4 + i * 2 + (i % 2), top = 22 - H - (i % 3);
                const bool ripe = stage == 4;
                im.line(x, 22, x + (i % 2 ? 1 : -1) * (stage - 1) / 2, top + 3, ripe ? HEX(0xd8b84a) : G2);
                if (stage >= 3) { im.rect(x + (i % 2 ? 1 : -1) * (stage - 1) / 2 - 1, top, 2, 4, ripe ? HEX(0xf2d868) : HEX(0xb8d868)); im.set(x + (i % 2 ? 1 : -1) * (stage - 1) / 2, top - 1, ripe ? HEX(0xfff0a0) : G3); }
                else leaf(x, 22, H / 2, 1.2f + (i % 3) * 0.3f, G2);
            }
            break;
        case CR_CARROT:
            for (int i = 0; i < 5; i++) leaf(9, 21, H - 2 - (i % 2) * 2, 0.5f + i * 0.5f, i % 2 ? G1 : G2);
            if (stage == 4) { im.rect(8, 20, 3, 3, HEX(0xf08a2a)); im.rect(8, 20, 1, 3, HEX(0xffb050)); im.set(9, 23, HEX(0xc86a1a)); }
            break;
        case CR_POTATO:
            for (int i = 0; i < 6; i++) leaf(9, 21, H - 3 - (i % 2) * 2, 0.4f + i * 0.45f, i % 2 ? G1 : G2);
            if (stage >= 3) for (int i = 0; i < 3; i++) { im.set(6 + i * 3, 22 - H + 1, HEX(0xf8f4ff)); im.set(6 + i * 3, 22 - H + 2, HEX(0xd8c0f0)); }
            if (stage == 4) { im.ellipseShaded(6, 22, 2.4f, 1.8f, HEX(0xe0c08a), HEX(0xc09a5a), HEX(0x8a6a3a)); im.ellipseShaded(12, 22, 2.4f, 1.8f, HEX(0xe0c08a), HEX(0xc09a5a), HEX(0x8a6a3a)); }
            break;
        case CR_CABBAGE: {
            const float r = stage == 2 ? 3.5f : stage == 3 ? 5.f : 6.5f;
            im.ellipseShaded(9, 19, r + 1.5f, r * 0.8f, G3, G2, G1);
            if (stage >= 3) im.ellipseShaded(9, 18, r - 1.f, (r - 1.f) * 0.9f, HEX(0xc8f0a0), HEX(0xa0d878), HEX(0x70b050));
            if (stage == 4) { im.line(9, 14, 9, 21, HEX(0xe0f8c0)); im.line(6, 16, 8, 20, GD); im.line(12, 16, 10, 20, GD); }
            break;
        }
        case CR_CORN:
            im.rect(8, 22 - H - 1, 2, H + 2, G2); im.rect(8, 22 - H - 1, 1, H + 2, G3);
            for (int i = 0; i < 3; i++) { leaf(9, 21 - i * 3, 5, 0.4f + (i % 2) * 2.3f, G1); leaf(9, 20 - i * 3, 5, 2.7f - (i % 2) * 2.2f, G2); }
            if (stage >= 3) im.rect(7, 22 - H - 2, 4, 3, stage == 4 ? HEX(0xf2d040) : G3);
            if (stage == 4) { im.ellipseShaded(12, 12, 1.8f, 3.5f, HEX(0xffe870), HEX(0xf2d040), HEX(0xb08a20)); im.ellipseShaded(6, 15, 1.8f, 3.5f, HEX(0xffe870), HEX(0xf2d040), HEX(0xb08a20)); }
            break;
        case CR_TOMATO:
            im.line(5, 22, 5, 22 - H - 2, HEX(0x8a5a30)); im.line(13, 22, 13, 22 - H - 2, HEX(0x8a5a30));  // stakes
            for (int i = 0; i < 6; i++) leaf(9, 21, H - 1 - (i % 2) * 2, 0.3f + i * 0.5f, i % 2 ? G1 : G2);
            if (stage >= 3) for (int i = 0; i < 4; i++) { const int x = 6 + (i % 2) * 5, y = 22 - H / 2 - 2 + i * 2; im.ellipseShaded(x, y, 1.8f, 1.8f, stage == 4 ? HEX(0xff7a6a) : HEX(0xb8d868), stage == 4 ? HEX(0xe03a2a) : HEX(0x8ab848), stage == 4 ? HEX(0x9a1a14) : HEX(0x5a8a30)); }
            break;
        case CR_PUMPKIN:
            for (int i = 0; i < 4; i++) leaf(9, 21, 5 + i % 2 * 2, 0.4f + i * 0.9f, i % 2 ? G1 : G2);
            if (stage >= 3) { const float r = stage == 3 ? 3.5f : 6.f; im.ellipseShaded(9, 19, r + 1.f, r * 0.8f, stage == 4 ? HEX(0xffa84a) : HEX(0xa8c868), stage == 4 ? HEX(0xf08a2a) : HEX(0x78a848), stage == 4 ? HEX(0xb85a14) : HEX(0x507a30)); if (stage == 4) { im.line(9, 14, 9, 22, HEX(0xc86a1a)); im.line(6, 15, 6, 21, HEX(0xc86a1a)); im.line(12, 15, 12, 21, HEX(0xc86a1a)); im.rect(8, 12, 2, 3, HEX(0x5a8a30)); } }
            break;
        case CR_STRAWBERRY:
            for (int i = 0; i < 5; i++) leaf(9, 21, 4 + (i % 2) * 2, 0.3f + i * 0.65f, i % 2 ? G1 : G3);
            if (stage >= 3) for (int i = 0; i < 4; i++) { const int x = 5 + i * 3, y = 19 + (i % 2); if (stage == 4) { im.ellipseShaded(x, y, 1.6f, 2.0f, HEX(0xff6a7a), HEX(0xe0283a), HEX(0x8a1020)); im.set(x - 1, y - 1, HEX(0xfff0c0)); } else { im.set(x, y, HEX(0xf8f8ff)); im.set(x + 1, y, HEX(0xf2d040)); } }
            break;
    }
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------ the portal (4 swirl frames, 48 x 60) and the market stall (56 x 46)
inline Img portal(int frame) {
    Img im(48, 60);
    const Pal P = stonePal();
    const float cx = 24.f, cy = 30.f, rx = 14.f, ry = 21.f;
    // the stone steps
    im.rect(4, 52, 40, 6, P.M); im.rect(4, 52, 40, 1, P.L); im.rect(4, 57, 40, 1, P.DD); im.rect(8, 49, 32, 4, P.D); im.rect(8, 49, 32, 1, P.L);
    // the energy: a swirling disc
    for (int y = 0; y < 52; y++)
        for (int x = 0; x < 48; x++) {
            const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            const float d = dx * dx + dy * dy;
            if (d > 1.f) continue;
            const float ang = atan2f(dy, dx), rad = sqrtf(d);
            const float sw = sinf(ang * 3.f + rad * 9.f - frame * 1.5708f) * 0.5f + 0.5f;
            const u32 deep = HEX(0x2a2a9a), mid = HEX(0x5a6af0), hi = HEX(0xa8d0ff), core = HEX(0xf0f8ff);
            u32 c = sw < 0.35f ? deep : sw < 0.7f ? mid : hi;
            if (rad < 0.22f) c = mixc(core, c, rad / 0.22f);
            if (rad > 0.85f) c = mixc(c, deep, (rad - 0.85f) / 0.15f);
            im.set(x, y, c);
        }
    // two pillars with a carved arch
    for (int s = 0; s < 2; s++) {
        const int x = s ? 36 : 2;
        im.rect(x, 12, 10, 42, P.M); im.rect(x, 12, 3, 42, P.L); im.rect(x + 8, 12, 2, 42, P.D);
        for (int y = 16; y < 50; y += 8) { im.rect(x, y, 10, 2, P.D); im.rect(x, y, 10, 1, P.L); }
        im.rect(x - 1, 8, 12, 5, P.M); im.rect(x - 1, 8, 12, 1, P.L); im.rect(x - 1, 12, 12, 1, P.DD);
        im.rect(x - 1, 52, 12, 3, P.D);
        const u32 rune = (frame + s) % 2 ? HEX(0x9ad0ff) : HEX(0x5a8ad8);  // glowing runes
        im.set(x + 4, 24, rune); im.set(x + 5, 25, rune); im.set(x + 4, 26, rune); im.set(x + 4, 34, rune); im.set(x + 5, 35, rune); im.set(x + 3, 36, rune);
    }
    for (int x = 10; x < 38; x++) {  // arch
        const float t = (x - 10) / 27.f, y = 3.f + 14.f * (1.f - sinf(t * 3.14159f));
        for (int k = 0; k < 6; k++) im.set(x, (int)y + k, k == 0 ? P.L : k == 5 ? P.DD : P.M);
    }
    im.rect(21, 0, 6, 6, P.M); im.rect(21, 0, 6, 1, P.L);  // keystone with a gem
    im.set(23, 2, HEX(0xffd8ff)); im.set(24, 3, HEX(0xc070ff)); im.set(23, 3, HEX(0xc070ff));
    im.outline(OUT);
    return im;
}
inline Img market() {
    Img im(60, 50);
    // counter and back shelves
    im.rect(4, 22, 52, 22, HEX(0x8a5a30)); im.rect(4, 22, 52, 2, HEX(0xb07a42));
    for (int x = 4; x < 56; x += 6) im.rect(x, 24, 1, 20, HEX(0x6a4426));
    im.rect(2, 40, 56, 5, HEX(0xb07a42)); im.rect(2, 40, 56, 1, HEX(0xd9a866)); im.rect(2, 44, 56, 1, HEX(0x5a371c));
    // goods on the counter: crates of produce, a sack of seeds, a hanging sign
    auto crate = [&](int x, u32 a, u32 b) { im.rect(x, 33, 12, 8, HEX(0x6a4426)); im.rect(x, 33, 12, 1, HEX(0xb07a42)); for (int i = 0; i < 4; i++) im.ellipseShaded(x + 2.5f + i * 2.4f, 33.f, 1.9f, 1.9f, lit(a), a, b); };
    crate(7, HEX(0xe03a2a), HEX(0x9a1a14)); crate(21, HEX(0xf08a2a), HEX(0xb85a14)); crate(35, HEX(0x8ad868), HEX(0x4a9a3a)); crate(47, HEX(0xf2d040), HEX(0xb08a20));
    // awning: striped cloth on two posts
    im.rect(2, 8, 3, 36, HEX(0x6a4426)); im.rect(55, 8, 3, 36, HEX(0x6a4426));
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 60; x++) {
            const int stripe = ((x + y / 2) / 5) % 2;
            u32 c = stripe ? HEX(0xf4f0ea) : HEX(0x3a9a5a);
            if (y == 11) c = drk(c);
            if (y == 0) c = lit(c);
            if (y >= 9 && (x % 5) > 2) continue;  // scalloped hem
            im.set(x, 6 + y, c);
        }
    im.rect(14, 19, 32, 8, HEX(0x5a371c)); im.rect(15, 20, 30, 6, HEX(0xc89a58));
    im.text(17, 21, "MARKET", HEX(0x3a2210));
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------ icons (16 x 16) for the new materials and tools
inline Img iconMat(int m) {
    Img im(16, 16);
    auto seedPacket = [&](u32 label) {
        im.rect(3, 3, 10, 12, HEX(0xe8d8a8)); im.rect(3, 3, 10, 1, HEX(0xfff0c8)); im.rect(3, 14, 10, 1, HEX(0xb09860));
        im.rect(3, 3, 10, 3, drk(HEX(0xe8d8a8))); im.rect(5, 7, 6, 5, label); im.rect(5, 7, 6, 1, lit(label));
        im.set(7, 9, HEX(0x241a14)); im.set(9, 10, HEX(0x241a14));
        im.outline(OUT);
    };
    if (m >= M_SEED0 && m < M_SEED0 + CROP_N) {
        static const u32 lab[CROP_N] = {HEX(0xe8c040), HEX(0xf08a2a), HEX(0xc09a5a), HEX(0x6ab84a), HEX(0xf2d040), HEX(0xe03a2a), HEX(0xf08a2a), HEX(0xe0283a)};
        seedPacket(lab[m - M_SEED0]);
        return im;
    }
    switch (m) {
        case M_BERRY:
            for (int i = 0; i < 5; i++) im.ellipseShaded(5 + (i % 3) * 3.f, 8 + (i / 3) * 3.f, 2.2f, 2.2f, HEX(0xc08aff), HEX(0x7a3ad8), HEX(0x3a1a8a));
            im.rect(7, 3, 2, 3, HEX(0x4a9a3a)); im.set(5, 5, HEX(0x6ab84a)); im.set(10, 5, HEX(0x6ab84a));
            break;
        case M_WHEAT:
            for (int i = 0; i < 3; i++) { const int x = 4 + i * 3; im.line(x, 14, x + (i - 1), 6, HEX(0xc8a43a)); im.rect(x + (i - 1) - 1, 2, 2, 5, HEX(0xf2d868)); im.set(x + (i - 1), 1, HEX(0xfff0a0)); im.set(x + (i - 1) - 1, 4, HEX(0xb08a20)); }
            break;
        case M_CARROT:
            im.line(8, 3, 8, 7, HEX(0x4a9a3a)); im.line(6, 3, 8, 7, HEX(0x6ab84a)); im.line(10, 3, 8, 7, HEX(0x6ab84a));
            for (int y = 6; y < 15; y++) { const int w = std::max(1, 5 - (y - 6) / 2); im.rect(8 - w / 2, y, w, 1, y < 9 ? HEX(0xffa040) : HEX(0xf08a2a)); }
            im.set(7, 8, HEX(0xffd080)); im.set(9, 12, HEX(0xc86a1a));
            break;
        case M_POTATO:
            im.ellipseShaded(8, 9, 5.5f, 4.2f, HEX(0xe8cc94), HEX(0xc09a5a), HEX(0x8a6a3a)); im.set(6, 8, HEX(0x6a4a24)); im.set(10, 10, HEX(0x6a4a24)); im.set(8, 6, HEX(0x6a4a24));
            break;
        case M_CABBAGE:
            im.ellipseShaded(8, 9, 6, 5.5f, HEX(0xd8f8b0), HEX(0x8ad868), HEX(0x4a9a3a)); im.line(8, 4, 8, 14, HEX(0xe8ffd0)); im.line(4, 7, 7, 12, HEX(0x4a9a3a)); im.line(12, 7, 9, 12, HEX(0x4a9a3a));
            break;
        case M_CORN:
            im.ellipseShaded(8, 8, 2.8f, 5.8f, HEX(0xffe870), HEX(0xf2d040), HEX(0xb08a20));
            for (int y = 4; y < 13; y += 2) for (int x = 6; x < 11; x += 2) im.set(x, y, HEX(0xc89a20));
            im.line(8, 14, 4, 8, HEX(0x4a9a3a)); im.line(8, 14, 12, 8, HEX(0x6ab84a)); im.line(8, 14, 8, 10, HEX(0x8ad868));
            break;
        case M_TOMATO:
            im.ellipseShaded(8, 9, 5.5f, 5, HEX(0xff8a7a), HEX(0xe03a2a), HEX(0x9a1a14)); im.rect(7, 3, 2, 2, HEX(0x4a9a3a)); im.set(5, 5, HEX(0x6ab84a)); im.set(10, 5, HEX(0x6ab84a)); im.set(6, 7, HEX(0xffd0c8));
            break;
        case M_PUMPKIN:
            im.ellipseShaded(8, 9, 6.5f, 5.5f, HEX(0xffb860), HEX(0xf08a2a), HEX(0xb85a14)); im.line(8, 4, 8, 14, HEX(0xc86a1a)); im.line(5, 5, 4, 12, HEX(0xc86a1a)); im.line(11, 5, 12, 12, HEX(0xc86a1a)); im.rect(7, 2, 2, 3, HEX(0x5a8a30));
            break;
        case M_STRAWBERRY:
            im.ellipseShaded(8, 9, 4.5f, 5.5f, HEX(0xff7a8a), HEX(0xe0283a), HEX(0x8a1020)); im.rect(6, 3, 5, 2, HEX(0x4a9a3a)); im.set(8, 2, HEX(0x6ab84a));
            for (int i = 0; i < 4; i++) im.set(6 + (i % 2) * 3, 7 + i * 2 / 1 % 5, HEX(0xfff0c0));
            break;
        case M_EGG:
            im.ellipseShaded(8, 9, 4.2f, 5.2f, HEX(0xfff4e0), HEX(0xf0dcc0), HEX(0xc0a888)); im.set(6, 6, HEX(0xffffff));
            break;
        case M_DUCKEGG:
            im.ellipseShaded(8, 9, 4.4f, 5.4f, HEX(0xe8fff0), HEX(0xc8ecd8), HEX(0x90b8a8)); im.set(6, 6, HEX(0xffffff));
            break;
        case M_MILK:
            im.rect(5, 6, 6, 9, HEX(0xf4f4f8)); im.rect(5, 6, 1, 9, HEX(0xffffff)); im.rect(10, 6, 1, 9, HEX(0xc0c4d0)); im.rect(6, 3, 4, 3, HEX(0xdfe6f2)); im.rect(6, 2, 4, 1, HEX(0x8a5a30));
            im.rect(6, 9, 4, 3, HEX(0x7ab8e0));
            break;
        case M_WOOL:
            for (int i = 0; i < 6; i++) im.ellipseShaded(4 + (i % 3) * 3.5f, 6 + (i / 3) * 4.f, 3.4f, 3.2f, HEX(0xffffff), HEX(0xeceff4), HEX(0xb8bece));
            break;
        case M_TRUFFLE:
            im.ellipseShaded(8, 9, 5.2f, 4.5f, HEX(0x6a5648), HEX(0x4a3a30), HEX(0x2a201a));
            for (int i = 0; i < 6; i++) im.set(4 + (int)(nz(i, 1, 4) % 8), 6 + (int)(nz(i, 2, 4) % 6), HEX(0x8a7466));
            break;
        case M_HAY:
            for (int i = 0; i < 9; i++) { const int x = 3 + i; im.line(x, 14, x + (i % 3) - 1, 4 + (i % 4), i % 2 ? HEX(0xe0c060) : HEX(0xc8a43a)); }
            im.rect(3, 9, 10, 2, HEX(0x8a5a30));
            break;
        default: break;
    }
    im.outline(OUT);
    return im;
}
inline Img iconTool(int k) {
    Img im(16, 16);
    const u32 W1 = HEX(0xb07a42), W2 = HEX(0x84552c), M1 = HEX(0xc4c8d4), M2 = HEX(0x8a90a4);
    if (k == TL_HOE) {
        im.line(3, 14, 12, 3, W1); im.line(4, 14, 13, 3, W2);
        im.rect(10, 2, 5, 2, M1); im.rect(13, 3, 2, 5, M1); im.rect(13, 3, 1, 5, M2); im.rect(10, 3, 5, 1, M2);
    } else if (k == TL_CAN) {
        im.rect(3, 6, 8, 7, HEX(0x7a8ca8)); im.rect(3, 6, 8, 1, HEX(0xb0c4de)); im.rect(10, 6, 1, 7, HEX(0x4a5a78)); im.rect(3, 12, 8, 1, HEX(0x4a5a78));
        im.line(11, 8, 15, 4, HEX(0x7a8ca8)); im.line(11, 9, 15, 5, HEX(0x4a5a78)); im.rect(14, 3, 2, 2, HEX(0xb0c4de));
        im.rect(4, 3, 6, 1, HEX(0x4a5a78)); im.set(4, 4, HEX(0x4a5a78)); im.set(9, 4, HEX(0x4a5a78)); im.set(4, 5, HEX(0x4a5a78)); im.set(9, 5, HEX(0x4a5a78));
        im.set(15, 6, HEX(0x7ab8e0)); im.set(15, 8, HEX(0x7ab8e0));
    } else {  // sickle
        im.line(3, 14, 6, 9, W1); im.line(4, 14, 7, 9, W2);
        for (int a = 0; a < 12; a++) { const float t = a / 11.f * 3.1f - 0.3f; im.set(7 + (int)lroundf(cosf(t) * 5.f), 8 - (int)lroundf(sinf(t) * 5.f), M1); im.set(7 + (int)lroundf(cosf(t) * 5.f), 9 - (int)lroundf(sinf(t) * 5.f), M2); }
    }
    im.outline(OUT);
    return im;
}
inline Img iconPiece(int kind) {  // a tiny picture of the piece for the build menu (24 x 24)
    Img im(24, 24);
    const Pal P = woodPal();
    switch (kind) {
        case HP_WALL_WOOD: im.blit(wall(0, true), 4, 1); break;
        case HP_WALL_STONE: im.blit(wall(1, true), 4, 1); break;
        case HP_FLOOR_WOOD: im.blit(floorWood(false), 4, 4); im.outline(OUT); break;
        case HP_FLOOR_STONE: im.blit(floorStone(), 4, 4); im.outline(OUT); break;
        case HP_FOUNDATION: im.blit(foundation(), 4, 4); im.outline(OUT); break;
        case HP_BRIDGE: im.blit(bridge(false), 4, 4); im.outline(OUT); break;
        case HP_ROOF: im.blit(roofTile(), 4, 4); im.outline(OUT); break;
        case HP_PLOT: im.blit(plot(false), 4, 4); im.outline(OUT); break;
        case HP_FENCE: im.blit(fence(false), 4, 3); break;
        case HP_GATE: im.blit(gate(false, false), 4, 3); break;
        case HP_DOOR: im.blit(door(false), 4, 0); break;
        case HP_WELL: { Img w = well(); im.blit(w, -4, -10); im.outline(OUT); break; }
        case HP_SCARECROW: im.blit(scarecrow(), 1, -2); break;
        case HP_CAMPFIRE: im.ellipse(12, 16, 7, 3, HEX(0x6a6a74)); im.rect(8, 10, 8, 6, HEX(0xf0953a)); im.rect(10, 8, 4, 4, HEX(0xffd060)); im.rect(7, 15, 10, 2, HEX(0x6a4426)); im.outline(OUT); break;
        case HP_BENCH: im.rect(3, 10, 18, 4, P.M); im.rect(3, 10, 18, 1, P.L); im.rect(4, 14, 2, 5, P.D); im.rect(18, 14, 2, 5, P.D); im.rect(3, 6, 18, 3, P.L); im.outline(OUT); break;
        case HP_PLANTER: im.rect(5, 12, 14, 7, P.M); im.rect(5, 12, 14, 1, P.L); im.rect(5, 18, 14, 1, P.D); for (int i = 0; i < 5; i++) { im.rect(6 + i * 3, 7 + (i % 2), 1, 5, HEX(0x4a9a3a)); im.rect(5 + i * 3, 5 + (i % 2) * 2, 3, 3, i % 2 ? HEX(0xff7a8a) : HEX(0xf2d040)); } im.outline(OUT); break;
        case HP_BARREL: im.rect(6, 5, 12, 15, P.M); im.rect(6, 5, 3, 15, P.L); im.rect(15, 5, 3, 15, P.D); im.rect(6, 8, 12, 1, HEX(0x6a6a74)); im.rect(6, 16, 12, 1, HEX(0x6a6a74)); im.outline(OUT); break;
        case HP_SIGN: im.rect(11, 8, 3, 13, P.D); im.rect(4, 4, 16, 8, P.M); im.rect(4, 4, 16, 1, P.L); im.rect(6, 7, 12, 1, P.DD); im.rect(6, 9, 8, 1, P.DD); im.outline(OUT); break;
        case HP_LAMP: im.rect(11, 9, 2, 12, HEX(0x4a4a54)); im.rect(8, 4, 8, 6, HEX(0xffe48a)); im.rect(8, 3, 8, 1, HEX(0x4a4a54)); im.rect(8, 10, 8, 1, HEX(0x4a4a54)); im.set(10, 6, HEX(0xffffff)); im.outline(OUT); break;
        case HP_TORCH: im.rect(11, 10, 2, 10, P.D); im.ellipse(12, 7, 3.2f, 4.f, HEX(0xf0953a)); im.ellipse(12, 8, 1.8f, 2.6f, HEX(0xffe060)); im.outline(OUT); break;
        default: {  // buildings: a miniature
            const int w = HPDEF[kind].w, h = HPDEF[kind].h;
            if (kind == HP_PEN) { for (int i = 0; i < 4; i++) { im.rect(3 + i * 5, 6, 2, 12, P.M); } im.rect(3, 8, 18, 2, P.L); im.rect(3, 13, 18, 2, P.L); im.outline(OUT); break; }
            if (kind == HP_GREENHOUSE) { im.rect(3, 6, 18, 12, RGBA(190, 230, 245, 200)); im.rect(3, 6, 18, 1, HEX(0xe6e8ec)); im.rect(3, 17, 18, 1, HEX(0x8a909c)); for (int x = 3; x < 22; x += 6) im.rect(x, 6, 1, 12, HEX(0xe6e8ec)); im.outline(OUT); break; }
            Img b = building(kind, std::min(w, 3), std::min(h, 2));
            // fit into 24 x 24 by sampling
            for (int y = 0; y < 24; y++)
                for (int x = 0; x < 24; x++) {
                    const int sx = x * b.w / 24, sy = y * b.h / 24;
                    im.set(x, y, b.get(sx, sy));
                }
            break;
        }
    }
    return im;
}

// ------------------------------------------------------------------ wild gathering nodes (18 x 18, base at (9, 16))
inline Img nodeSprite(int i) {
    Img im(18, 18);
    if (i == 0) {  // berry bush with ripe purple berries
        im.ellipseShaded(9, 11, 8, 5.5f, HEX(0x7ad060), HEX(0x4a9a3a), HEX(0x2e6a2a));
        im.ellipseShaded(5, 9, 4.5f, 3.5f, HEX(0x8ae070), HEX(0x58aa44), HEX(0x34782e));
        im.ellipseShaded(13, 9, 4.5f, 3.5f, HEX(0x8ae070), HEX(0x58aa44), HEX(0x34782e));
        for (int k = 0; k < 9; k++) {
            const int x = 3 + (int)(nz(k, 3, 8) % 12), y = 6 + (int)(nz(k, 4, 8) % 8);
            im.ellipseShaded(x, y, 1.6f, 1.6f, HEX(0xc08aff), HEX(0x7a3ad8), HEX(0x3a1a8a));
        }
    } else {  // a patch of wild flowers
        for (int k = 0; k < 11; k++) {
            const int x = 2 + (int)(nz(k, 5, 8) % 14), y = 5 + (int)(nz(k, 6, 8) % 9);
            im.line(x, y + 3, x, 15, HEX(0x4a9a3a));
            static const u32 C[4] = {HEX(0xff7a8a), HEX(0xf2d040), HEX(0xffffff), HEX(0xc08aff)};
            const u32 c = C[k % 4];
            im.rect(x - 1, y - 1, 3, 3, c); im.set(x, y, HEX(0xffe880)); im.set(x - 1, y - 1, lit(c));
        }
        im.rect(1, 14, 16, 2, HEX(0x4a9a3a));
    }
    im.outline(OUT);
    return im;
}

// ------------------------------------------------------------------ the asset set
struct HomeAssets {
    Spr wall[2][2];            // [wood / stone][capped / face only]
    Spr floorWood[2], floorStone, foundation, bridge[2], plot[2], roof;
    Spr fence[2], gate[2][2], door[2];   // [horizontal / vertical][closed / open]  (door: closed / open)
    Spr building[HP_COUNT][2]; // [kind][rot & 1]
    Spr greenhouse[2];
    Spr well, scarecrow;
    Spr crop[CR_COUNT][CROP_STAGES];
    Spr portal[4], market, node[2];
    Spr pieceIcon[HP_COUNT];
    Spr matIcon[M_COUNT], toolIcon[TL_COUNT];  // only the homestead's entries are filled
    bool ok = false;
};
inline void buildHomeAssets(SDL_Renderer* r, HomeAssets& a) {
    for (int m = 0; m < 2; m++)
        for (int c = 0; c < 2; c++) a.wall[m][c] = makeSpr(r, wall(m, c == 0), 8, c == 0 ? 20 : 15);
    for (int v = 0; v < 2; v++) {
        a.floorWood[v] = makeSpr(r, floorWood(v == 1), 0, 0);
        a.bridge[v] = makeSpr(r, bridge(v == 1), 0, 0);
        a.fence[v] = makeSpr(r, fence(v == 1), v ? 4 : 8, v ? 17 : 16);
        for (int o = 0; o < 2; o++) a.gate[v][o] = makeSpr(r, gate(v == 1, o == 1), v ? 4 : 8, v ? 17 : 16);
        a.plot[v] = makeSpr(r, plot(v == 1), 0, 0);
        a.door[v] = makeSpr(r, door(v == 1), 8, 23);
    }
    a.floorStone = makeSpr(r, floorStone(), 0, 0);
    a.foundation = makeSpr(r, foundation(), 0, 0);
    a.roof = makeSpr(r, roofTile(), 0, 0);
    static const int B[] = {HP_SHED, HP_HOUSE, HP_BARN, HP_COOP, HP_WORKSHOP, HP_WAREHOUSE};
    for (int k : B)
        for (int o = 0; o < 2; o++) {
            const int w = o ? HPDEF[k].h : HPDEF[k].w, h = o ? HPDEF[k].w : HPDEF[k].h;
            Img b = building(k, w, h);
            a.building[k][o] = makeSpr(r, b, b.w / 2, b.h - 1);
        }
    for (int o = 0; o < 2; o++) {
        const int w = o ? HPDEF[HP_GREENHOUSE].h : HPDEF[HP_GREENHOUSE].w, h = o ? HPDEF[HP_GREENHOUSE].w : HPDEF[HP_GREENHOUSE].h;
        Img g = greenhouse(w, h);
        a.greenhouse[o] = makeSpr(r, g, 1, g.h - 1);
    }
    a.well = makeSpr(r, well(), 16, 43);
    a.scarecrow = makeSpr(r, scarecrow(), 11, 29);
    for (int c = 0; c < CR_COUNT; c++)
        for (int s = 0; s < CROP_STAGES; s++) a.crop[c][s] = makeSpr(r, crop(c, s), 9, 23);
    for (int f = 0; f < 4; f++) a.portal[f] = makeSpr(r, portal(f), 24, 57);
    a.market = makeSpr(r, market(), 30, 46);
    for (int i = 0; i < 2; i++) a.node[i] = makeSpr(r, nodeSprite(i), 9, 16);
    for (int k = 0; k < HP_COUNT; k++) a.pieceIcon[k] = makeSpr(r, iconPiece(k), 12, 12);
    for (int m = M_BERRY; m < M_COUNT; m++) a.matIcon[m] = makeSpr(r, iconMat(m), 8, 8);
    for (int k = TL_HOE; k < TL_COUNT; k++) a.toolIcon[k] = makeSpr(r, iconTool(k), 8, 8);
    a.ok = true;
}

}  // namespace hart
