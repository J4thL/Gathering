// sprites_interior.hpp - everything seen inside the town buildings, all drawn in code like the rest of the game:
// floor / wall tiles, furniture (one 16x24 sprite each) and the people who work there (one 18x26 sprite pair each).
#pragma once
#include "data.hpp"
#include "town.hpp"

// ----------------------------------------------------------------- furniture ids (Prop::ph of a P_FURN prop)
enum Furn {
    FU_FURNACE, FU_ANVIL, FU_FORGE, FU_WORKBENCH, FU_BARREL, FU_CRATE, FU_SHELF, FU_TABLE, FU_COUNTER, FU_TANRACK, FU_LEATHERTABLE,
    FU_FLOWERS, FU_POT, FU_MIRROR, FU_CHAIR, FU_TOOLS, FU_STOVE, FU_GRIND, FU_COAL, FU_LOGS, FU_HIDES, FU_SINK, FU_DISPLAY, FU_COUNT
};

// ----------------------------------------------------------------- floors and walls
inline Img mkFloor(int kind, int variant) {  // 0 wood planks, 1 flagstone, 2 checkered tile
    Img im(16, 16);
    if (kind == 0) {
        const u32 b[3] = {HEX(0xa8794a), HEX(0x9a6c40), HEX(0xb28350)};
        im.fill(HEX(0x5e3d22));
        for (int r = 0; r < 4; r++) {
            const u32 c = b[(r + variant) % 3];
            im.rect(0, r * 4, 16, 3, c);
            im.rect(0, r * 4, 16, 1, shadeC(c, 1.12f));
            const int sx = (r * 7 + variant * 5) % 14 + 1;
            im.rect(sx, r * 4, 1, 3, shadeC(c, 0.8f));
        }
    } else if (kind == 1) {
        const u32 b[3] = {HEX(0x7e7c86), HEX(0x72707a), HEX(0x8a8892)};
        im.fill(HEX(0x4a4850));
        im.rect(0, 0, 8, 8, b[variant % 3]);
        im.rect(8, 0, 8, 8, b[(variant + 1) % 3]);
        im.rect(0, 8, 8, 8, b[(variant + 2) % 3]);
        im.rect(8, 8, 8, 8, b[variant % 3]);
        for (int i = 0; i < 4; i++) im.rect((i & 1) * 8, (i >> 1) * 8, 7, 1, HEX(0x9a98a2));
    } else {
        const u32 a = variant ? HEX(0xefe8dc) : HEX(0xf4eee2), c = variant ? HEX(0xd78aa8) : HEX(0xe0a0b8);
        im.fill(a);
        im.rect(0, 0, 8, 8, c);
        im.rect(8, 8, 8, 8, c);
        im.rect(0, 0, 16, 1, shadeC(a, 0.9f));
        im.rect(0, 0, 1, 16, shadeC(a, 0.9f));
    }
    return im;
}
inline Img mkWallFace(u32 base) {  // the wall you look at: panelled wood-and-plaster with a skirting board
    Img im(16, 16);
    im.fill(base);
    im.rect(0, 0, 16, 2, shadeC(base, 1.15f));
    im.rect(0, 10, 16, 6, shadeC(base, 0.78f));
    im.rect(0, 10, 16, 1, shadeC(base, 0.6f));
    im.rect(0, 15, 16, 1, shadeC(base, 0.5f));
    for (int x = 3; x < 16; x += 8) im.rect(x, 10, 1, 6, shadeC(base, 0.62f));
    return im;
}
inline Img mkWallCap(u32 base) {  // the top / side / bottom edge of the room: a dark beam
    Img im(16, 16);
    const u32 c = shadeC(base, 0.5f);
    im.fill(c);
    im.rect(0, 0, 16, 1, shadeC(c, 1.3f));
    im.rect(0, 15, 16, 1, shadeC(c, 0.7f));
    for (int x = 4; x < 16; x += 8) im.rect(x, 0, 1, 16, shadeC(c, 0.8f));
    return im;
}
inline Img mkRug(u32 a, u32 b) {
    Img im(16, 16);
    im.fill(a);
    im.rect(0, 0, 16, 2, b);
    im.rect(0, 14, 16, 2, b);
    im.rect(7, 2, 2, 12, b);
    im.rect(3, 6, 10, 4, shadeC(a, 0.85f));
    im.rect(0, 2, 1, 12, shadeC(a, 0.8f));
    im.rect(15, 2, 1, 12, shadeC(a, 0.8f));
    return im;
}
inline Img mkDoorMat() {
    Img im(16, 16);
    im.fill(HEX(0x4a3a2a));
    im.rect(1, 3, 14, 10, HEX(0x7a5c3a));
    for (int x = 2; x < 14; x += 2) im.rect(x, 4, 1, 8, HEX(0x5e4428));
    im.rect(0, 0, 16, 1, HEX(0x2e2118));
    return im;
}

// ----------------------------------------------------------------- furniture
inline Img mkFurn(int id) {
    Img im(18, 26);  // drawn standing on the bottom edge (y = 24); the sprite is anchored at (9, 24)
    const u32 wood = HEX(0x8a5c34), woodL = HEX(0xb07a45), woodD = HEX(0x5e3d22), iron = HEX(0x4a4a56), ironL = HEX(0x7a7a8a), stone = HEX(0x77757f), stoneL = HEX(0x9a98a4);
    switch (id) {
        case FU_FURNACE:  // a stone furnace with a glowing mouth and a flue
            im.rect(2, 8, 14, 16, stone);
            im.rect(2, 8, 14, 2, stoneL);
            im.rect(6, 1, 6, 8, HEX(0x5e5c66));
            im.rect(6, 1, 6, 1, stoneL);
            im.rect(4, 13, 10, 8, HEX(0x1c1210));
            im.rect(5, 15, 8, 5, HEX(0xe8742a));
            im.rect(6, 17, 6, 3, HEX(0xffc85a));
            im.rect(3, 22, 12, 2, HEX(0x5e5c66));
            break;
        case FU_ANVIL:
            im.rect(3, 12, 12, 4, iron);
            im.rect(3, 12, 12, 1, ironL);
            im.rect(0, 13, 4, 2, iron);
            im.rect(6, 16, 6, 4, HEX(0x3a3a46));
            im.rect(4, 20, 10, 4, woodD);
            im.rect(4, 20, 10, 1, wood);
            break;
        case FU_FORGE:  // forging station: a low trough of embers with tongs and a hammer hung on it
            im.rect(1, 14, 16, 9, HEX(0x5a5864));
            im.rect(1, 14, 16, 2, stoneL);
            im.rect(3, 16, 12, 4, HEX(0x241614));
            im.rect(4, 17, 10, 2, HEX(0xe8742a));
            im.rect(6, 17, 3, 1, HEX(0xffc85a));
            im.rect(2, 6, 1, 9, iron);
            im.rect(15, 6, 1, 9, iron);
            im.rect(2, 6, 14, 1, iron);
            im.rect(5, 7, 1, 5, ironL);
            im.rect(11, 7, 2, 3, ironL);
            break;
        case FU_WORKBENCH:
            im.rect(1, 12, 16, 4, woodL);
            im.rect(1, 12, 16, 1, HEX(0xd8a060));
            im.rect(2, 16, 2, 8, wood);
            im.rect(14, 16, 2, 8, wood);
            im.rect(4, 19, 10, 2, woodD);
            im.rect(3, 9, 5, 3, HEX(0xa0a0aa));  // saw blade
            im.rect(10, 8, 4, 4, HEX(0xc9a56a));  // a plank
            im.rect(12, 6, 1, 3, HEX(0x6b4423));  // hammer
            im.rect(11, 5, 3, 1, iron);
            break;
        case FU_BARREL:
            im.ellipse(9, 14, 7, 9, wood);
            im.rect(2, 8, 14, 12, wood);
            im.rect(2, 10, 14, 1, iron);
            im.rect(2, 17, 14, 1, iron);
            im.rect(2, 8, 1, 12, woodD);
            im.rect(4, 6, 10, 2, woodL);
            im.rect(4, 6, 10, 1, HEX(0xd8a060));
            break;
        case FU_CRATE:
            im.rect(2, 9, 14, 15, woodL);
            im.rect(2, 9, 14, 2, HEX(0xd8a060));
            im.rect(2, 22, 14, 2, woodD);
            im.rect(2, 9, 2, 15, wood);
            im.rect(14, 9, 2, 15, wood);
            im.rect(4, 15, 10, 2, wood);
            break;
        case FU_SHELF: {  // a tall shelf with jars, boxes and bottles
            im.rect(1, 2, 16, 22, woodD);
            im.rect(2, 3, 14, 20, HEX(0x7a5230));
            for (int r = 0; r < 3; r++) {
                const int y = 8 + r * 7;
                im.rect(1, y, 16, 1, woodL);
                const u32 c[3] = {HEX(0xe8584f), HEX(0x4a8ad8), HEX(0xf2c94c)};
                im.rect(3, y - 4, 3, 4, c[r % 3]);
                im.rect(8, y - 5, 3, 5, c[(r + 1) % 3]);
                im.rect(13, y - 3, 2, 3, c[(r + 2) % 3]);
            }
            break;
        }
        case FU_TABLE:
            im.rect(1, 11, 16, 3, woodL);
            im.rect(1, 11, 16, 1, HEX(0xd8a060));
            im.rect(2, 14, 2, 9, wood);
            im.rect(14, 14, 2, 9, wood);
            im.rect(6, 7, 6, 4, HEX(0xe8dcc0));  // a cloth with a jug
            im.rect(12, 5, 3, 6, HEX(0x8a4a24));
            break;
        case FU_COUNTER:
            im.rect(0, 9, 18, 15, wood);
            im.rect(0, 9, 18, 3, HEX(0xd8a060));
            im.rect(0, 12, 18, 1, woodD);
            im.rect(2, 15, 6, 7, HEX(0x7a5230));
            im.rect(10, 15, 6, 7, HEX(0x7a5230));
            im.rect(2, 6, 4, 3, HEX(0xf2c94c));  // coins and a ledger
            im.rect(9, 6, 6, 3, HEX(0xe8dcc0));
            break;
        case FU_TANRACK:  // a wooden frame with a stretched hide
            im.rect(1, 3, 2, 21, wood);
            im.rect(15, 3, 2, 21, wood);
            im.rect(1, 3, 16, 2, woodL);
            im.rect(3, 6, 12, 14, HEX(0xc99a6a));
            im.rect(3, 6, 12, 2, HEX(0xdcb07e));
            im.rect(5, 10, 4, 6, HEX(0xa8794a));
            im.rect(10, 12, 3, 5, HEX(0xb88858));
            for (int x = 4; x < 15; x += 4) im.set(x, 5, HEX(0x3b2616));
            break;
        case FU_LEATHERTABLE:
            im.rect(1, 11, 16, 3, HEX(0xa8794a));
            im.rect(1, 11, 16, 1, HEX(0xd8a060));
            im.rect(2, 14, 2, 9, wood);
            im.rect(14, 14, 2, 9, wood);
            im.rect(3, 8, 6, 3, HEX(0xb88858));  // folded leather
            im.rect(3, 8, 6, 1, HEX(0xdcb07e));
            im.rect(11, 6, 1, 5, ironL);  // awl
            im.rect(13, 9, 3, 2, HEX(0xe8584f));  // thread
            break;
        case FU_FLOWERS: {  // buckets of cut flowers on a stand
            im.rect(1, 17, 16, 2, woodL);
            im.rect(2, 19, 2, 5, wood);
            im.rect(14, 19, 2, 5, wood);
            const u32 fc[3] = {HEX(0xe8584f), HEX(0xf2c94c), HEX(0xa070e0)};
            for (int b = 0; b < 3; b++) {
                const int bx = 2 + b * 5;
                im.rect(bx, 14, 4, 4, HEX(0x6a8aa8));
                im.rect(bx, 14, 4, 1, HEX(0x9ab4cc));
                im.rect(bx + 1, 8, 1, 6, HEX(0x3f8a42));
                im.ellipse(bx + 1.5f, 7, 2.6f, 2.6f, fc[b]);
                im.set(bx + 1, 7, HEX(0xfff6d0));
                im.rect(bx + 3, 10, 1, 4, HEX(0x3f8a42));
                im.ellipse(bx + 3.5f, 9, 1.8f, 1.8f, fc[(b + 1) % 3]);
            }
            break;
        }
        case FU_POT:
            im.rect(5, 16, 8, 8, HEX(0xb8643c));
            im.rect(4, 16, 10, 2, HEX(0xd0784a));
            im.rect(6, 22, 6, 2, HEX(0x8a4a2c));
            im.ellipse(9, 12, 6, 6, HEX(0x3f8a42));
            im.ellipse(7, 9, 3, 3, HEX(0x58a85a));
            im.ellipse(12, 10, 3, 3, HEX(0x58a85a));
            im.set(9, 7, HEX(0xe88ab8));
            im.set(9, 6, HEX(0xe88ab8));
            break;
        case FU_MIRROR:  // a standing mirror in a wooden frame
            im.rect(2, 1, 14, 20, woodD);
            im.rect(4, 3, 10, 16, HEX(0xbfe0ea));
            im.rect(4, 3, 3, 16, HEX(0xe8f6fa));
            im.rect(8, 5, 1, 4, HEX(0xffffff));
            im.rect(4, 21, 10, 3, wood);
            break;
        case FU_CHAIR:  // a salon chair: cushion seat, tall back, footrest
            im.rect(3, 4, 12, 10, HEX(0xb03a5a));
            im.rect(3, 4, 12, 2, HEX(0xd85a7a));
            im.rect(2, 13, 14, 5, HEX(0xc84a6a));
            im.rect(2, 17, 14, 1, HEX(0x7a2a3e));
            im.rect(7, 18, 4, 5, HEX(0x8a8a96));
            im.rect(4, 23, 10, 1, HEX(0x5a5a66));
            im.rect(0, 12, 3, 2, woodD);
            im.rect(15, 12, 3, 2, woodD);
            break;
        case FU_TOOLS:  // a rack of finished tools and blades on the wall
            im.rect(1, 6, 16, 3, woodD);
            im.rect(2, 9, 2, 10, ironL);
            im.rect(2, 8, 4, 2, iron);
            im.rect(8, 9, 1, 12, woodL);
            im.rect(6, 9, 5, 2, ironL);
            im.rect(13, 9, 2, 8, ironL);
            im.rect(12, 16, 4, 2, woodL);
            break;
        case FU_STOVE:  // a cast-iron stove with a pot
            im.rect(2, 8, 14, 16, HEX(0x3a3a46));
            im.rect(2, 8, 14, 2, iron);
            im.rect(4, 14, 10, 6, HEX(0x1c1210));
            im.rect(6, 16, 6, 3, HEX(0xe8742a));
            im.rect(4, 3, 10, 5, HEX(0x6a6a78));
            im.rect(3, 2, 12, 1, ironL);
            im.rect(14, 0, 2, 4, HEX(0x2e2e38));
            break;
        case FU_GRIND:  // a grindstone on a wooden frame
            im.rect(2, 10, 2, 14, wood);
            im.rect(14, 10, 2, 14, wood);
            im.ellipse(9, 13, 6, 6, HEX(0x9a98a4));
            im.ellipse(9, 13, 3, 3, HEX(0x6a6a74));
            im.rect(9, 12, 1, 2, HEX(0x3a3a46));
            im.rect(6, 20, 6, 2, woodD);
            break;
        case FU_COAL:  // a bin of coal
            im.rect(1, 12, 16, 12, woodD);
            im.rect(1, 12, 16, 2, wood);
            im.ellipse(9, 11, 7, 4, HEX(0x1e1e24));
            for (int i = 0; i < 6; i++) im.set(4 + i * 2, 9 + (i % 2), HEX(0x4a4a54));
            break;
        case FU_LOGS:
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3 - r; c++) {
                    im.ellipse(4 + c * 5 + r * 2.5f, 20 - r * 4, 3, 3, HEX(0x8a5c34));
                    im.ellipse(4 + c * 5 + r * 2.5f, 20 - r * 4, 1.6f, 1.6f, HEX(0xd8a868));
                }
            break;
        case FU_HIDES:  // a pile of hides
            im.ellipse(9, 20, 8, 4, HEX(0xb88858));
            im.ellipse(8, 17, 7, 3.5f, HEX(0xc99a6a));
            im.ellipse(10, 14, 6, 3, HEX(0xdcb07e));
            im.rect(5, 17, 3, 1, HEX(0x8a5c34));
            break;
        case FU_SINK:
            im.rect(2, 10, 14, 14, HEX(0x9aa0aa));
            im.rect(2, 10, 14, 2, HEX(0xc8ced8));
            im.rect(4, 12, 10, 5, HEX(0x4a7aa8));
            im.rect(8, 5, 2, 5, HEX(0x8a8a96));
            im.rect(8, 5, 5, 1, HEX(0x8a8a96));
            break;
        case FU_DISPLAY:  // a glass display case
            im.rect(1, 8, 16, 16, HEX(0x8a5c34));
            im.rect(2, 9, 14, 8, HEX(0xcfe8f0));
            im.rect(2, 9, 14, 1, HEX(0xffffff));
            im.rect(4, 12, 3, 4, HEX(0xf2c94c));
            im.rect(9, 11, 3, 5, HEX(0x7a5230));
            im.rect(13, 13, 2, 3, HEX(0xe8584f));
            im.rect(1, 18, 16, 1, woodD);
            break;
    }
    im.outline(OUT);
    return im;
}

// ----------------------------------------------------------------- people
// One parametrised body for the six townsfolk: palette and a few accessories make each a different person.
// Mary Rose and Salona Mia are the two with a unique look (see below).
struct NpcLook {
    u32 skin, hair, top, topD, bottom, accent;
    int hairStyle;  // 0 short, 1 long, 2 bun, 3 bald + beard (the smith), 4 cap
    int accessory;  // 0 none, 1 apron, 2 flower crown, 3 scissors belt, 4 goggles
    bool beard;
};
inline NpcLook npcLook(int k) {
    switch (k) {
        case BLD_SMITH: return {HEX(0xd9a47a), HEX(0x3a2a22), HEX(0x6a6a74), HEX(0x4a4a54), HEX(0x3a2e28), HEX(0xe8742a), 3, 1, true};
        case BLD_TANNER: return {HEX(0xe9bd94), HEX(0x7a4a2a), HEX(0x8a5c34), HEX(0x6a4424), HEX(0x4a3b2a), HEX(0xdcb07e), 2, 1, false};
        case BLD_WORKSHOP: return {HEX(0xf0c49a), HEX(0x8a6a3a), HEX(0x2f7a6a), HEX(0x215a4e), HEX(0x4a3b2a), HEX(0xc9a56a), 4, 4, true};
        case BLD_SHOP: return {HEX(0xf0c49a), HEX(0x6b4a2b), HEX(0xb23a3a), HEX(0x8a2e2e), HEX(0x4a3b2a), HEX(0xe8dcc0), 4, 1, false};
        case BLD_FLOWER: return {HEX(0xf4cfae), HEX(0xb8542a), HEX(0xe88ab8), HEX(0xc06a96), HEX(0x6a4a8a), HEX(0xfff0a0), 1, 2, false};  // Mary Rose: long copper hair, rose dress, flower crown
        default: return {HEX(0xcf9a74), HEX(0x6a2a9a), HEX(0x2a6a8a), HEX(0x1c4a66), HEX(0x2e2a3a), HEX(0xf4f0ea), 1, 3, false};          // Salona Mia: violet hair, teal smock, scissors belt
    }
}
inline Img mkNpc(int k, int frame) {
    const NpcLook L = npcLook(k);
    Img im(18, 26);
    const int bob = frame ? 1 : 0;
    const u32 eye = HEX(0x2a1c1c);
    const bool dress = k == BLD_FLOWER;
    // legs / skirt
    if (dress) {
        for (int y = 15; y <= 23; y++) {
            const int hw = 4 + (y - 15) / 2;
            im.rect(9 - hw, y, hw * 2, 1, y > 21 ? L.topD : L.top);
        }
        im.rect(6, 23, 2, 1, HEX(0x2e2118));
        im.rect(10, 23, 2, 1, HEX(0x2e2118));
    } else {
        im.rect(5, 19, 3, 5, L.bottom);
        im.rect(10, 19, 3, 5, L.bottom);
        im.rect(5, 23, 3, 1, HEX(0x2e2118));
        im.rect(10, 23, 3, 1, HEX(0x2e2118));
    }
    // torso and arms
    im.rect(4, 11 - bob, 10, 9, L.top);
    im.rect(4, 11 - bob, 10, 1, shadeC(L.top, 1.2f));
    im.rect(4, 19 - bob, 10, 1, L.topD);
    im.rect(2, 12 - bob * 2, 2, 5, L.top);
    im.rect(14, 12 - bob * 2, 2, 5, L.top);
    for (int x : {2, 3, 14, 15}) im.set(x, 17 - bob * 2, L.skin);
    // accessories on the body
    if (L.accessory == 1) {  // apron
        im.rect(6, 13 - bob, 6, 8, L.accent);
        im.rect(6, 13 - bob, 6, 1, shadeC(L.accent, 0.85f));
    } else if (L.accessory == 3) {  // belt with scissors
        im.rect(4, 17 - bob, 10, 1, HEX(0x8a5c34));
        im.rect(12, 14 - bob, 1, 4, HEX(0xc8ced8));
        im.rect(13, 14 - bob, 1, 3, HEX(0xc8ced8));
        im.set(12, 18 - bob, HEX(0xe8584f));
    } else if (L.accessory == 4) {  // tool belt and straps
        im.rect(4, 17 - bob, 10, 1, HEX(0x6a4424));
        im.rect(6, 11 - bob, 1, 6, HEX(0x6a4424));
        im.rect(11, 11 - bob, 1, 6, HEX(0x6a4424));
    }
    // head
    im.ellipse(9, 8 - bob, 3.6f, 3.4f, L.skin);
    im.set(7, 8 - bob, eye);
    im.set(11, 8 - bob, eye);
    if (L.beard) {
        im.rect(6, 10 - bob, 7, 2, L.hair);
        im.rect(7, 12 - bob, 5, 1, L.hair);
    } else
        im.rect(8, 10 - bob, 3, 1, shadeC(L.skin, 0.8f));  // a small smile
    // hair
    switch (L.hairStyle) {
        case 0: im.ellipse(9, 5 - bob, 4, 2.6f, L.hair); break;
        case 1:  // long hair falling past the shoulders
            im.ellipse(9, 5 - bob, 4.4f, 3.2f, L.hair);
            im.rect(4, 6 - bob, 2, 9, L.hair);
            im.rect(12, 6 - bob, 2, 9, L.hair);
            im.rect(4, 6 - bob, 1, 9, shadeC(L.hair, 1.25f));
            break;
        case 2:  // a bun
            im.ellipse(9, 5 - bob, 4, 2.6f, L.hair);
            im.ellipse(9, 2 - bob, 2.2f, 2.2f, L.hair);
            break;
        case 3:  // bald with a leather headband
            im.rect(5, 5 - bob, 8, 1, HEX(0x6a4424));
            break;
        case 4:  // a cap
            im.rect(4, 4 - bob, 10, 2, L.accent);
            im.ellipse(9, 4 - bob, 4, 2.8f, L.top);
            im.rect(4, 5 - bob, 12, 1, L.topD);
            break;
    }
    if (L.accessory == 2) {  // flower crown
        const u32 fc[4] = {HEX(0xf4f0ea), HEX(0xf2c94c), HEX(0xe8584f), HEX(0xa070e0)};
        for (int i = 0; i < 4; i++) {
            im.set(5 + i * 3, 3 - bob, fc[i]);
            im.set(5 + i * 3, 4 - bob, HEX(0x3f8a42));
        }
    } else if (L.accessory == 4) {  // goggles pushed up
        im.rect(6, 3 - bob, 6, 2, HEX(0x6a6a78));
        im.set(7, 3 - bob, HEX(0xbfe0ea));
        im.set(10, 3 - bob, HEX(0xbfe0ea));
    }
    if (k == BLD_FLOWER) {  // she carries a little bouquet
        im.rect(1, 15 - bob * 2, 1, 4, HEX(0x3f8a42));
        im.ellipse(1.5f, 14 - bob * 2, 1.8f, 1.8f, HEX(0xe8584f));
    }
    im.outline(OUT);
    return im;
}
