// sprites_town.hpp - pixel art for the central town: the six building exteriors, street furniture, paving, and (further down) the
// interior floors, walls, furniture and the townsfolk. Everything is painted in code like the rest of the game.
#pragma once
#include "town.hpp"
#include "sprites_interior.hpp"

// ----------------------------------------------------------------- building exteriors (104 x 84, anchored at the bottom centre)
constexpr int BSPR_W = 104, BSPR_H = 84;

// one shingle roof, a trapezoid from the ridge (y0) down to the eave (y1)
inline void townRoof(Img& im, const BuildingDef& B, int y0, int y1, int hw0, int hw1, int style) {
    const int cx = BSPR_W / 2;
    for (int y = y0; y <= y1; y++) {
        const float k = (float)(y - y0) / (float)(y1 - y0);
        const int hw = (int)(hw0 + k * (hw1 - hw0));
        for (int x = cx - hw; x < cx + hw; x++) {
            u32 c = B.roof;
            if (style == 2) {  // slate: neat bricks of tiles
                if ((y % 4) == 0) c = B.roofD;
                else if (((x + (y / 4) * 5) % 8) == 0) c = B.roofD;
                else if (((x + (y / 4) * 5) % 8) == 1) c = B.roofL;
            } else if (style == 3) {  // thatch / hip: fine strokes
                c = ((x * 7 + y * 3) % 5) < 2 ? B.roofL : B.roof;
                if ((y % 5) == 4) c = B.roofD;
            } else {  // shingles
                if ((y % 4) == 0) c = B.roofD;
                else if (((x + (y / 4) * 3) % 6) == 0) c = B.roofL;
            }
            if (x <= cx - hw + 1) c = B.roofL;
            if (x >= cx + hw - 2) c = B.roofD;
            im.set(x, y, c);
        }
    }
    im.rect(cx - hw1 - 1, y1 + 1, 2 * hw1 + 2, 3, B.roofD);
    im.rect(cx - hw1 - 1, y1 + 1, 2 * hw1 + 2, 1, B.roof);
}

inline Img mkBuilding(int k) {
    const BuildingDef& B = BUILDINGS[k];
    Img im(BSPR_W, BSPR_H);
    const int cx = BSPR_W / 2;
    const u32 glass = HEX(0xf2d98b), glassL = HEX(0xfaeab0), frame = B.trim;
    // ---- behind-the-roof extras
    if (k == BLD_SMITH) {  // a tall stone chimney with a glow
        im.rect(74, 0, 12, 22, HEX(0x6e6e78));
        im.rect(74, 0, 12, 3, HEX(0x4a4a54));
        im.rect(82, 3, 4, 19, HEX(0x58585f));
        for (int y = 5; y < 20; y += 4) im.rect(75, y, 10, 1, HEX(0x5a5a64));
        im.set(79, 1, HEX(0xff8a2b));
        im.set(80, 1, HEX(0xffd34d));
    } else if (k == BLD_TANNER) {  // a squat brick flue
        im.rect(20, 6, 9, 16, HEX(0x9a5a3a));
        im.rect(20, 6, 9, 2, HEX(0x6a3a22));
        for (int y = 9; y < 22; y += 3) im.rect(20, y, 9, 1, HEX(0x7a4228));
    } else if (k == BLD_WORKSHOP) {  // a little roof vent with a weather vane
        im.rect(48, 2, 8, 10, HEX(0x5a7a6a));
        im.rect(51, 0, 2, 3, HEX(0x2e2e38));
    }
    // ---- roof
    const int style = k == BLD_SMITH ? 2 : (k == BLD_FLOWER ? 3 : (k == BLD_SALON ? 2 : 1));
    if (k == BLD_SHOP || k == BLD_SALON) townRoof(im, B, 12, 33, 28, 51, style);   // steeper, narrower ridge: a gabled front
    else townRoof(im, B, 6, 33, 34, 51, style);
    if (k == BLD_FLOWER) {  // round dormer with a flower
        im.ellipse(cx, 20, 9, 8, B.wall);
        im.ellipse(cx, 20, 6, 5.5f, glass);
        im.rect(cx - 1, 15, 2, 11, frame);
        im.rect(cx - 6, 20, 12, 1, frame);
        im.set(cx - 3, 16, HEX(0xe8584f));
        im.set(cx + 3, 24, HEX(0xf2c94c));
    } else if (k == BLD_SALON) {  // a big pink scissors emblem on the gable
        im.ellipse(cx, 24, 10, 8, HEX(0xefe2f4));
        im.line(cx - 6, 29, cx + 5, 19, HEX(0x4a3a5a));
        im.line(cx + 6, 29, cx - 5, 19, HEX(0x4a3a5a));
        im.rect(cx - 8, 28, 4, 3, HEX(0xb04a8a));
        im.rect(cx + 5, 28, 4, 3, HEX(0xb04a8a));
    } else if (k == BLD_WORKSHOP) {  // a round saw blade hung on the gable
        im.ellipse(cx, 22, 8, 8, HEX(0xc8ccd4));
        im.ellipse(cx, 22, 2.5f, 2.5f, HEX(0x3a3a42));
        for (int i = 0; i < 12; i++) im.set((int)(cx + cosf(i * 0.5236f) * 8.8f), (int)(22 + sinf(i * 0.5236f) * 8.8f), HEX(0xeef0f4));
    }
    // ---- walls
    const int wy0 = 34, wy1 = 79;
    im.rect(6, wy0, 92, wy1 - wy0 + 1, B.wall);
    if (k == BLD_SMITH) {  // stone blocks
        for (int y = wy0; y <= wy1; y += 6) {
            im.rect(6, y, 92, 1, B.wallD);
            for (int x = 6 + ((y / 6) % 2) * 6; x < 98; x += 12) im.rect(x, y, 1, 6, B.wallD);
        }
    } else if (k == BLD_SHOP || k == BLD_SALON || k == BLD_FLOWER) {  // plaster with timber posts / panels
        for (int x = 6; x < 98; x += 23) im.rect(x, wy0, 2, wy1 - wy0 + 1, B.wallD);
        im.rect(6, wy0, 92, 2, B.wallD);
        for (int y = wy0 + 4; y < wy1; y += 7)
            for (int x = 9; x < 96; x += 11) im.set(x + (y % 3), y, B.wallD);
    } else {  // planks
        for (int x = 6; x < 98; x += 6) im.rect(x, wy0, 1, wy1 - wy0 + 1, B.wallD);
        for (int x = 9; x < 98; x += 6) im.set(x, wy0 + 6 + (x % 7) * 4, B.wallD);
    }
    im.rect(6, wy0, 92, 1, B.wallD);
    im.rect(6, wy1 - 3, 92, 4, k == BLD_SMITH ? HEX(0x4a4a54) : HEX(0x77747c));  // stone footing
    for (int x = 6; x < 98; x += 8) im.rect(x, wy1 - 3, 1, 4, HEX(0x5e5b63));
    // ---- door (centre), with a lintel and a step
    im.rect(44, 52, 16, 28, frame);
    im.rect(46, 54, 12, 26, B.door);
    for (int x = 46; x < 58; x += 3) im.rect(x, 54, 1, 26, shadeC(B.door, 0.8f));
    im.rect(51, 54, 1, 26, shadeC(B.door, 0.65f));
    im.set(56, 68, HEX(0xf2c94c));
    im.rect(43, 51, 18, 2, shadeC(frame, 0.8f));
    im.rect(42, 80, 20, 3, HEX(0x9a9aa4));
    im.rect(42, 80, 20, 1, HEX(0xc2c2cc));
    // ---- windows (one each side of the door), kind-specific frames
    for (int wx : {14, 78}) {
        im.rect(wx - 1, 48, 14, 12, frame);
        im.rect(wx, 49, 12, 10, glass);
        im.rect(wx, 49, 12, 3, glassL);
        im.rect(wx + 5, 49, 2, 10, frame);
        im.rect(wx, 54, 12, 1, frame);
        im.rect(wx - 2, 60, 16, 2, shadeC(frame, 0.85f));
    }
    // ---- sign board hung above the door, painted with the building's name
    const std::string label = B.sign;
    const int tw = (int)label.size() * 4 - 1, bw = std::max(28, tw + 8), bx = cx - bw / 2;
    im.rect(cx - 1, 33, 2, 3, HEX(0x3b2616));
    im.rect(bx, 36, bw, 11, HEX(0xc99a5a));
    im.rect(bx, 36, bw, 1, HEX(0xe3b97a));
    im.rect(bx, 46, bw, 1, HEX(0x8a5c34));
    im.rect(bx, 36, 1, 11, HEX(0x8a5c34));
    im.rect(bx + bw - 1, 36, 1, 11, HEX(0x8a5c34));
    im.text(cx - tw / 2, 40, label, HEX(0x3b2616));
    // ---- kind-specific dressing
    if (k == BLD_SMITH) {
        for (int wx : {14, 78}) {  // furnace-orange glow in the windows
            im.rect(wx, 49, 12, 10, HEX(0xe07a2a));
            im.rect(wx, 49, 12, 3, HEX(0xffb24a));
            im.rect(wx + 5, 49, 2, 10, frame);
            im.rect(wx, 54, 12, 1, frame);
        }
        im.rect(4, 70, 14, 10, HEX(0x3a3a42));   // coal heap
        im.ellipse(11, 70, 7, 3, HEX(0x2a2a30));
        im.set(8, 69, HEX(0xe0452a));
        im.rect(86, 70, 10, 10, HEX(0x58585f));  // anvil block by the wall
        im.rect(84, 68, 14, 4, HEX(0x8d929e));
        im.rect(84, 68, 14, 1, HEX(0xc8ccd4));
        im.rect(98, 69, 3, 2, HEX(0x8d929e));
    } else if (k == BLD_TANNER) {
        im.rect(0, 40, 3, 40, HEX(0x6b4423));    // drying frame with a stretched hide
        im.rect(0, 40, 14, 3, HEX(0x6b4423));
        im.rect(11, 40, 3, 40, HEX(0x6b4423));
        im.rect(3, 43, 8, 24, HEX(0xc89a62));
        im.rect(3, 43, 8, 2, HEX(0xe3bd86));
        im.set(5, 52, HEX(0x8a5c34));
        im.set(8, 58, HEX(0x8a5c34));
        im.rect(90, 62, 12, 18, HEX(0x8a5c34));  // two barrels
        im.rect(90, 66, 12, 1, HEX(0x3b2616));
        im.rect(90, 74, 12, 1, HEX(0x3b2616));
        im.rect(86, 70, 8, 10, HEX(0xa8743f));
    } else if (k == BLD_WORKSHOP) {
        for (int i = 0; i < 4; i++) {            // a stack of logs
            im.ellipse(8 + (i % 2) * 8, 76 - (i / 2) * 6, 4, 3, HEX(0xb07a45));
            im.ellipse(8 + (i % 2) * 8, 76 - (i / 2) * 6, 1.6f, 1.2f, HEX(0xe3b97a));
        }
        im.rect(88, 64, 14, 16, HEX(0xb07a45));  // tool rack on the wall: hammer and saw
        im.rect(88, 64, 14, 1, HEX(0xe3b97a));
        im.line(90, 80, 96, 66, HEX(0x6b4423));
        im.rect(94, 65, 6, 3, HEX(0x8d929e));
        im.line(98, 80, 100, 68, HEX(0xc8ccd4));
    } else if (k == BLD_SHOP) {
        for (int x = 8; x < 96; x += 8) {        // striped awning over the windows and the door
            const u32 c = ((x / 8) % 2) ? HEX(0xf4f0ea) : HEX(0xd8443f);
            im.rect(x, 46, 8, 5, c);
            im.rect(x, 51, 8, 1, shadeC(c, 0.8f));
        }
        im.rect(4, 70, 12, 10, HEX(0xb07a45));   // crates of goods
        im.rect(4, 70, 12, 2, HEX(0xc99a5a));
        im.set(8, 68, HEX(0xe8584f));
        im.set(11, 68, HEX(0xf2c94c));
        im.ellipse(94, 76, 5, 4, HEX(0xc9b88a)); // a sack of grain
        im.rect(92, 71, 4, 2, HEX(0x8a5c34));
    } else if (k == BLD_FLOWER) {
        for (int wx : {14, 78}) {                // flower boxes under both windows, overflowing
            im.rect(wx - 2, 62, 16, 5, HEX(0x8a5c34));
            for (int i = 0; i < 6; i++) {
                const u32 fc[3] = {HEX(0xe8584f), HEX(0xf2c94c), HEX(0xf4f0ea)};
                im.set(wx - 1 + i * 3, 60 + (i % 2), fc[i % 3]);
                im.set(wx - 1 + i * 3, 61, HEX(0x4f9a52));
            }
        }
        for (int x = 40; x < 66; x += 9) im.ellipse(x + 4, 76, 5, 4, HEX(0x4f9a52));  // potted shrubs by the door
        im.set(46, 74, HEX(0xe8584f));
        im.set(60, 76, HEX(0xf2c94c));
    } else if (k == BLD_SALON) {
        for (int x = 8; x < 40; x += 8) {        // mirror-bright window and a little awning
            im.rect(x, 46, 8, 4, ((x / 8) % 2) ? HEX(0xffffff) : HEX(0xa05ab0));
        }
        im.rect(90, 56, 5, 24, HEX(0xf4f0ea));   // barber pole: red / white / blue spiral
        for (int y = 56; y < 80; y++) {
            const int ph = (y + 56) % 9;
            if (ph < 3) im.rect(90, y, 5, 1, HEX(0xd8443f));
            else if (ph < 6) im.rect(90, y, 5, 1, HEX(0x3c7fc7));
        }
        im.ellipse(92, 55, 3.5f, 2, HEX(0xf2c94c));
        im.ellipse(92, 80, 3.5f, 2, HEX(0xf2c94c));
    }
    im.outline(OUT);
    return im;
}

// ----------------------------------------------------------------- street furniture and paving
inline Img mkLamp() {
    Img im(10, 36);
    im.rect(4, 8, 2, 26, HEX(0x2e2e38));
    im.rect(3, 32, 4, 3, HEX(0x3a3a46));
    im.rect(2, 3, 6, 6, HEX(0xffe9a8));
    im.rect(2, 3, 6, 1, HEX(0x2e2e38));
    im.rect(2, 8, 6, 1, HEX(0x2e2e38));
    im.rect(2, 3, 1, 6, HEX(0x2e2e38));
    im.rect(7, 3, 1, 6, HEX(0x2e2e38));
    im.rect(3, 4, 4, 4, HEX(0xfff6d0));
    im.rect(3, 1, 4, 2, HEX(0x2e2e38));
    im.outline(OUT);
    return im;
}
inline Img mkFence() {  // 16 px of wooden fence: two posts and two rails
    Img im(18, 18);
    im.rect(3, 3, 2, 12, HEX(0x8a5c34));
    im.rect(12, 3, 2, 12, HEX(0x8a5c34));
    im.rect(3, 3, 2, 1, HEX(0xc99a5a));
    im.rect(12, 3, 2, 1, HEX(0xc99a5a));
    im.rect(0, 6, 18, 2, HEX(0xb07a45));
    im.rect(0, 11, 18, 2, HEX(0xb07a45));
    im.rect(0, 6, 18, 1, HEX(0xd8a060));
    im.rect(0, 11, 18, 1, HEX(0xd8a060));
    im.outline(OUT);
    return im;
}
inline Img mkBench() {
    Img im(26, 16);
    im.rect(2, 8, 22, 3, HEX(0xb07a45));
    im.rect(2, 8, 22, 1, HEX(0xd8a060));
    im.rect(2, 3, 22, 3, HEX(0x9a6a3a));
    im.rect(3, 11, 2, 4, HEX(0x5a3a1e));
    im.rect(21, 11, 2, 4, HEX(0x5a3a1e));
    im.rect(3, 3, 2, 8, HEX(0x5a3a1e));
    im.rect(21, 3, 2, 8, HEX(0x5a3a1e));
    im.outline(OUT);
    return im;
}
inline Img mkPlanter() {  // a tub of flowers
    Img im(18, 18);
    im.rect(2, 9, 14, 7, HEX(0x8a5c34));
    im.rect(2, 9, 14, 2, HEX(0xa8743f));
    im.rect(2, 14, 14, 1, HEX(0x5e3d22));
    im.ellipse(9, 8, 7, 4, HEX(0x4f9a52));
    const u32 fc[5] = {HEX(0xe8584f), HEX(0xf2c94c), HEX(0xf4f0ea), HEX(0x8f6fe0), HEX(0xe88ab8)};
    for (int i = 0; i < 5; i++) {
        im.set(4 + i * 2, 5 + (i % 2) * 2, fc[i]);
        im.set(4 + i * 2, 6 + (i % 2) * 2, fc[i]);
    }
    im.outline(OUT);
    return im;
}
inline Img mkWell() {
    Img im(30, 34);
    im.ellipse(15, 26, 12, 6, HEX(0x8d929e));
    im.ellipse(15, 25, 10, 4.5f, HEX(0x2a3a4a));
    im.rect(3, 18, 24, 8, HEX(0x9a9fac));
    for (int x = 3; x < 27; x += 6) im.rect(x, 18, 1, 8, HEX(0x6a6f7c));
    im.rect(3, 18, 24, 1, HEX(0xc2c6d0));
    im.rect(4, 6, 2, 14, HEX(0x6b4423));
    im.rect(24, 6, 2, 14, HEX(0x6b4423));
    im.rect(2, 4, 26, 3, HEX(0x8a4a24));
    im.rect(2, 4, 26, 1, HEX(0xb06a38));
    im.rect(14, 7, 1, 9, HEX(0xc99a5a));
    im.rect(12, 15, 5, 3, HEX(0x8a5c34));
    im.outline(OUT);
    return im;
}
inline Img mkCobble(int variant, Rng& r) {  // road paving: tan and grey stones with dark mortar
    Img im(16, 16);
    const u32 base[3] = {HEX(0xa49a88), HEX(0x9a9080), HEX(0xaaa090)};
    im.fill(HEX(0x6e6556));
    const int off = variant * 3;
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 3; col++) {
            const int x = ((col * 6 + (row % 2) * 3 + off) % 18) - 2, y = row * 4;
            u32 c = base[(row + col + variant) % 3];
            if (r.chance(0.3f)) c = shadeC(c, 0.92f);
            im.rect(x, y, 5, 3, c);
            im.rect(x, y, 5, 1, shadeC(c, 1.12f));
        }
    return im;
}

struct TownAssets {
    Spr bld[BLD_COUNT];
    Spr lamp, fence, bench, planter, well, cobble[3];
    // interiors
    Spr floorW[3], floorS[3], floorT[2], wallFace[BLD_COUNT], wallCap[BLD_COUNT], rug[BLD_COUNT], mat;
    Spr furn[FU_COUNT];
    Spr npc[BLD_COUNT][2];
};

inline void buildTownAssets(SDL_Renderer* r, TownAssets& T) {
    Rng rg(424242);
    for (int k = 0; k < BLD_COUNT; k++) T.bld[k] = makeSpr(r, mkBuilding(k), BSPR_W / 2, BSPR_H - 1);
    T.lamp = makeSpr(r, mkLamp(), 5, 35);
    T.fence = makeSpr(r, mkFence(), 9, 16);
    T.bench = makeSpr(r, mkBench(), 13, 14);
    T.planter = makeSpr(r, mkPlanter(), 9, 16);
    T.well = makeSpr(r, mkWell(), 15, 31);
    for (int i = 0; i < 3; i++) T.cobble[i] = makeSpr(r, mkCobble(i, rg), 0, 0);
    for (int i = 0; i < 3; i++) T.floorW[i] = makeSpr(r, mkFloor(0, i), 0, 0);
    for (int i = 0; i < 3; i++) T.floorS[i] = makeSpr(r, mkFloor(1, i), 0, 0);
    for (int i = 0; i < 2; i++) T.floorT[i] = makeSpr(r, mkFloor(2, i), 0, 0);
    for (int k = 0; k < BLD_COUNT; k++) {
        T.wallFace[k] = makeSpr(r, mkWallFace(shadeC(BUILDINGS[k].wallD, 0.95f)), 0, 0);
        T.wallCap[k] = makeSpr(r, mkWallCap(BUILDINGS[k].wallD), 0, 0);
        T.rug[k] = makeSpr(r, mkRug(shadeC(BUILDINGS[k].roof, 1.0f), BUILDINGS[k].roofL), 0, 0);
        for (int f = 0; f < 2; f++) T.npc[k][f] = makeSpr(r, mkNpc(k, f), 9, 24);
    }
    T.mat = makeSpr(r, mkDoorMat(), 0, 0);
    for (int i = 0; i < FU_COUNT; i++) T.furn[i] = makeSpr(r, mkFurn(i), 9, 24);
}
