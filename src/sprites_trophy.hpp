// sprites_trophy.hpp - inventory icons for the 28 trophy materials and animal fat, drawn in code like every other sprite.
//
// Each trophy is a small SHAPE (fang, tail, feather, paw ...) in the species' own colours, so the table below is the only thing to
// edit to restyle or add one. Index = material - M_TROPHY0, which is also the species' index in the Kind enum (fauna.hpp).
#pragma once
#include "data.hpp"

enum TrophyShape { TS_FOOT, TS_FANG, TS_TUFT, TS_FEATHER, TS_TAIL, TS_GLAND, TS_HOOF, TS_SINEW, TS_RATTLE, TS_EYE, TS_BRISTLE,
                   TS_MARROW, TS_FLEECE, TS_EAR, TS_PELT, TS_DOWN, TS_VELVET, TS_GALL };
struct TrophyArt {
    int shape;
    u32 c1, c2, c3;  // main, light, dark / accent
    int var;         // shape variant: FANG 0 plain 1 thick 2 flipped, TAIL 0 slim 1 bushy, TUFT 0 fan up 1 beard hanging down
};
static const TrophyArt TROPHY_ART[M_TROPHY_N] = {
    {TS_FOOT, HEX(0xd8d0c4), HEX(0xffffff), HEX(0xe9a0a8), 0},      // rabbit foot
    {TS_SINEW, HEX(0xd8b890), HEX(0xf0dcc0), HEX(0x8a6a4a), 0},     // deer sinew
    {TS_BRISTLE, HEX(0x3a3430), HEX(0x6a5a50), HEX(0xa87a40), 0},   // boar bristle
    {TS_FANG, HEX(0xf2ead3), HEX(0xffffff), HEX(0xb8a888), 0},      // wolf fang
    {TS_TAIL, HEX(0xe0c898), HEX(0xfff4d8), HEX(0xa88a58), 2},      // sand hare tail
    {TS_EAR, HEX(0xe8b070), HEX(0xf8d8c0), HEX(0xa87038), 0},       // fennec ear
    {TS_FANG, HEX(0xe8dcc0), HEX(0xfff8e8), HEX(0xa89870), 2},      // coyote fang
    {TS_RATTLE, HEX(0xe8d8a8), HEX(0xc8b078), HEX(0x8a7440), 0},    // rattle
    {TS_FEATHER, HEX(0x4a7a9a), HEX(0xc8a070), HEX(0x4a3a2a), 0},   // road plume
    {TS_TAIL, HEX(0xb8a050), HEX(0xd8c870), HEX(0x6a5a28), 0},      // lizard tail
    {TS_TUFT, HEX(0xc8903a), HEX(0xe8b868), HEX(0x6a4a20), 0},      // lion mane
    {TS_GLAND, HEX(0x6ab04a), HEX(0xb8e890), HEX(0x2a5a2a), 0},     // frog gland
    {TS_DOWN, HEX(0xf4f0e0), HEX(0xffffff), HEX(0xc8c0a8), 0},      // duck down
    {TS_HOOF, HEX(0x5a4a3a), HEX(0x8a7860), HEX(0x2a2018), 0},      // bog hoof
    {TS_GLAND, HEX(0xa060d0), HEX(0xd8a8f0), HEX(0x4a2a6a), 0},     // venom sac
    {TS_FANG, HEX(0xd8dcc0), HEX(0xf8fce8), HEX(0x7a8a58), 1},      // gator tooth
    {TS_EYE, HEX(0xe8d040), HEX(0xfff6a0), HEX(0x2a2a30), 0},       // panther eye
    {TS_TAIL, HEX(0xb8c4d8), HEX(0xffffff), HEX(0x6a7088), 1},      // fox brush
    {TS_FOOT, HEX(0xf4f4fa), HEX(0xffffff), HEX(0xa8b8d0), 0},      // snow hare foot
    {TS_VELVET, HEX(0x9a7a5a), HEX(0xe8dcc0), HEX(0xb89a78), 0},    // velvet antler
    {TS_MARROW, HEX(0xe8a090), HEX(0xf2ead3), HEX(0xc0b090), 0},    // moose marrow
    {TS_TUFT, HEX(0x4a3e40), HEX(0x9a8a88), HEX(0xb89868), 0},      // lynx tuft
    {TS_GALL, HEX(0x7a9a3a), HEX(0xc0d878), HEX(0x3a4a1a), 0},      // bear gall
    {TS_TUFT, HEX(0xe8e4dc), HEX(0xffffff), HEX(0x8a8478), 1},      // goat beard
    {TS_FLEECE, HEX(0xf0ecdc), HEX(0xffffff), HEX(0xc0b8a0), 0},    // ram fleece
    {TS_FEATHER, HEX(0xe8b838), HEX(0xfff0a0), HEX(0x8a5a20), 0},   // golden plume
    {TS_PELT, HEX(0xb08a60), HEX(0xd8b888), HEX(0x70502c), 0},      // marmot pelt
    {TS_FANG, HEX(0xefe4cc), HEX(0xffffff), HEX(0xc0a070), 2},      // cougar fang
};

inline Img mkIconTrophy(int m) {
    Img im(16, 16);
    if (m == M_FAT) {  // a block of lard: pale slab with a lit top edge and a slow drip
        im.rect(3, 6, 10, 7, HEX(0xefe3c0));
        im.rect(3, 6, 10, 2, HEX(0xfff6dc));
        im.rect(3, 11, 10, 2, HEX(0xd0c096));
        im.rect(11, 7, 2, 6, HEX(0xdccca0));
        im.rect(5, 13, 2, 2, HEX(0xefe3c0));
        im.set(6, 15, HEX(0xefe3c0));
        im.set(5, 8, HEX(0xffffff));
        return im;
    }
    const TrophyArt& T = TROPHY_ART[m - M_TROPHY0];
    const u32 c1 = T.c1, c2 = T.c2, c3 = T.c3;
    switch (T.shape) {
        case TS_FOOT:  // a hind foot laid on the diagonal: long furred sole, splayed toes, a ring to carry it
            im.ellipse(4.8f, 12, 2.8f, 2.5f, c1);
            im.ellipse(6.8f, 9.6f, 2.8f, 2.6f, c1);
            im.ellipse(8.8f, 7.2f, 2.6f, 2.5f, c1);
            im.ellipse(9.6f, 3.9f, 1.3f, 1.7f, c1);
            im.ellipse(12, 5, 1.3f, 1.7f, c1);
            im.ellipse(12.6f, 7.6f, 1.3f, 1.6f, c1);
            im.set(9, 1, c3); im.set(12, 2, c3); im.set(14, 6, c3);
            im.ellipse(6.2f, 9.2f, 1.2f, 2.4f, c2);
            im.ellipse(2.6f, 14, 1.8f, 1.4f, HEX(0xb8bcc8));
            im.ellipse(2.6f, 14, 0.9f, 0.6f, 0u);
            break;
        case TS_FANG: {  // a curved tooth: root at the top, tip curling to the bottom (var 1 = thick, 2 = mirrored)
            const float wide = T.var == 1 ? 1.35f : 1.0f;
            for (int y = 1; y <= 14; y++) {
                const float f = (y - 1) / 13.f;
                const float cx = 5.5f + 4.5f * f * f, hw = (3.2f * powf(1.f - f, 0.8f) + 0.4f) * wide;
                for (int x = (int)floorf(cx - hw); x <= (int)ceilf(cx + hw); x++) {
                    if (fabsf(x + 0.5f - cx) > hw) continue;
                    u32 c = x + 0.5f < cx - hw * 0.35f ? c2 : (x + 0.5f > cx + hw * 0.45f ? c3 : c1);
                    im.set(T.var == 2 ? 15 - x : x, y, c);
                }
            }
            for (int x = 2; x <= 9; x++) im.set(T.var == 2 ? 15 - x : x, 1, c3);  // root
            break;
        }
        case TS_TUFT: {  // a bound bunch of hair: fanning up (mane, ear tuft) or hanging down (beard)
            const int tx[5] = {3, 5, 8, 11, 13};
            const int ty[5] = {4, 2, 1, 2, 4};
            for (int i = 0; i < 5; i++) {
                if (T.var == 1) im.line(8, 3, tx[i], 15 - ty[i], i % 2 ? c2 : c1);
                else im.line(8, 12, tx[i], ty[i], i % 2 ? c2 : c1);
            }
            im.rect(6, T.var == 1 ? 2 : 10, 4, 2, c3);
            break;
        }
        case TS_FEATHER:  // a quill with a barred vane
            im.line(3, 14, 12, 2, c3);
            for (int i = 0; i < 7; i++) {
                const float t = i / 6.f;
                const float x = 5 + 6 * t, y = 12 - 9 * t;
                im.ellipse(x - 1.2f, y, 2.4f, 1.5f, i % 2 ? c2 : c1);
                im.ellipse(x + 1.6f, y + 1.2f, 2.2f, 1.4f, i % 2 ? c1 : c2);
            }
            im.line(3, 14, 12, 2, c3);
            break;
        case TS_TAIL: {  // var 0 segmented, tapering tail; var 1 bushy brush with a pale tip; var 2 round puff
            if (T.var == 2) {
                im.ellipse(8, 8.5f, 5.2f, 4.6f, c3);
                im.ellipse(8, 8, 5, 4.4f, c1);
                im.ellipse(6.5f, 6.5f, 2.6f, 1.8f, c2);
                im.set(2, 8, c1); im.set(13, 7, c1); im.set(5, 3, c1); im.set(11, 3, c1); im.set(8, 13, c1);
                im.rect(7, 12, 3, 3, c3);
                break;
            }
            for (int i = 0; i <= 8; i++) {
                const float t = i / 8.f;
                const float x = 3 + 10 * t, y = 12.5f - 8 * t - 3 * sinf(3.14159f * t);
                float r = T.var == 1 ? 2.2f + 1.9f * sinf(3.14159f * t) : 2.2f * (1.f - t) + 0.6f;
                u32 c = T.var == 1 ? (i >= 7 ? c2 : c1) : (i % 2 ? c3 : c1);
                im.ellipse(x, y, r, r, c);
                if (T.var == 0 && i % 2 == 0) im.set((int)x, (int)(y - r * 0.5f), c2);
            }
            im.rect(2, 12, 3, 3, c3);
            break;
        }
        case TS_GLAND:  // a swollen gland / sac: glossy blob with a duct
            im.rect(7, 2, 2, 4, c3);
            im.ellipse(8, 9.5f, 5, 4.4f, c1);
            im.ellipse(6.4f, 7.6f, 2.2f, 1.4f, c2);
            im.set(10, 10, c3); im.set(7, 12, c3); im.set(11, 8, c3);
            break;
        case TS_HOOF:  // a cloven hoof: dark hairline, tapered horn wall, a cleft down the front, a flat sole
            im.rect(4, 2, 8, 2, c3);
            for (int y = 4; y <= 12; y++) {
                const int w = 3 + (y - 4) / 3;
                im.rect(8 - w, y, 2 * w + 1, 1, c1);
                im.set(8 - w, y, c2);
                im.set(8 + w, y, c3);
            }
            im.line(8, 5, 8, 11, c3);
            im.rect(3, 13, 11, 2, c3);
            im.rect(4, 13, 3, 1, c2);
            break;
        case TS_SINEW:  // a coil of twisted cord, seen from above
            im.ellipse(8, 8, 6.6f, 5.4f, c3);
            im.ellipse(8, 8, 5.6f, 4.4f, c1);
            im.ellipse(8, 8, 4.2f, 3.1f, c3);
            im.ellipse(8, 8, 3.2f, 2.1f, c1);
            im.ellipse(8, 8, 1.3f, 0.8f, 0u);
            im.set(4, 5, c2); im.set(7, 3, c2); im.set(11, 4, c2); im.set(13, 8, c2); im.set(10, 12, c2); im.set(5, 11, c2);
            im.set(6, 7, c2); im.set(10, 9, c2);
            break;
        case TS_RATTLE:  // stacked horny segments, small to large
            for (int i = 0; i < 5; i++) {
                const float r = 1.4f + i * 0.55f;
                im.ellipse(3.5f + i * 2.3f, 8, r * 0.8f, r + 0.6f, i % 2 ? c2 : c1);
            }
            im.line(3, 11, 13, 12, c3);
            im.set(3, 7, c3);
            break;
        case TS_EYE:  // a cat's eye, polished: dark rim, bright iris, slit pupil
            im.ellipse(8, 8, 6.4f, 4.8f, c3);
            im.ellipse(8, 8, 5.2f, 3.8f, c1);
            im.ellipse(7, 6.8f, 2.4f, 1.2f, c2);
            im.rect(7, 5, 2, 6, c3);
            im.set(6, 6, HEX(0xffffff));
            break;
        case TS_BRISTLE:  // a bundle of coarse bristles tied with leather
            for (int i = 0; i < 6; i++) im.line(3 + i * 2, 2 + (i & 1), 3 + i * 2, 13, i % 2 ? c2 : c1);
            im.rect(2, 8, 12, 2, c3);
            break;
        case TS_MARROW: {  // a cracked bone showing its marrow
            im.rect(3, 6, 10, 4, c2);
            im.ellipse(3, 6, 2, 2, c2); im.ellipse(3, 10, 2, 2, c2);
            im.ellipse(13, 6, 2, 2, c2); im.ellipse(13, 10, 2, 2, c2);
            im.rect(4, 7, 8, 2, c1);
            im.rect(3, 10, 10, 1, c3);
            im.set(5, 6, HEX(0xffffff));
            break;
        }
        case TS_FLEECE:  // a puff of wool
            im.ellipse(5.5f, 9, 3.6f, 3.4f, c1);
            im.ellipse(10.5f, 9, 3.6f, 3.4f, c1);
            im.ellipse(8, 6.5f, 4, 3.4f, c1);
            im.ellipse(8, 11, 4.4f, 2.6f, c3);
            im.ellipse(8, 10, 4.2f, 2.2f, c1);
            im.set(6, 5, c2); im.set(10, 7, c2); im.set(4, 8, c2);
            break;
        case TS_EAR:  // a tall pointed ear with a pale inside and a dark rim
            for (int y = 1; y <= 13; y++) {
                const int w = 1 + (y - 1) * 4 / 12;
                im.rect(8 - w, y, 2 * w + 1, 1, c1);
                if (y > 3 && w > 1) im.rect(8 - w + 1, y, 2 * w - 1, 1, c2);
                im.set(8 - w, y, c3);
                im.set(8 + w, y, c3);
            }
            im.rect(3, 14, 11, 1, c3);
            break;
        case TS_PELT:  // a small patch of fur with four stubby legs
            im.ellipse(8, 8, 5.4f, 4.6f, c1);
            im.rect(2, 3, 3, 3, c1); im.rect(11, 3, 3, 3, c1);
            im.rect(2, 10, 3, 3, c1); im.rect(11, 10, 3, 3, c1);
            im.ellipse(6.5f, 7, 3, 2.2f, c2);
            im.set(10, 9, c3); im.set(9, 11, c3); im.set(5, 10, c3);
            break;
        case TS_DOWN:  // a drift of soft feathers
            im.ellipse(6, 9.5f, 3.6f, 3.2f, c3);
            im.ellipse(10.5f, 8.5f, 3.6f, 3.2f, c3);
            im.ellipse(6, 9, 3.5f, 3.1f, c1);
            im.ellipse(10.5f, 8, 3.5f, 3.1f, c1);
            im.ellipse(8, 5.5f, 3, 2.6f, c1);
            im.ellipse(8, 11, 3.4f, 2.2f, c1);
            im.set(5, 8, c2); im.set(9, 4, c2); im.set(11, 7, c2);
            break;
        case TS_VELVET:  // an antler tine still wrapped in velvet
            im.rect(7, 7, 3, 8, c1);
            im.line(8, 9, 4, 4, c1); im.line(9, 9, 4, 4, c1);
            im.line(9, 7, 12, 2, c1); im.line(8, 7, 11, 2, c1);
            im.line(8, 6, 8, 1, c1);
            im.set(4, 3, c2); im.set(12, 1, c2); im.set(8, 0, c2);
            for (int i = 0; i < 6; i++) im.set(7 + (i * 5) % 3, 8 + i, c3);
            break;
        case TS_GALL:  // a gall bladder: teardrop with a sheen
            im.line(8, 2, 6, 6, c1); im.line(8, 2, 10, 6, c1);
            im.ellipse(8, 9.5f, 4, 4.4f, c1);
            im.rect(7, 1, 2, 3, c3);
            im.ellipse(6.4f, 8, 1.6f, 2.2f, c2);
            im.set(10, 12, c3); im.set(9, 13, c3);
            break;
    }
    return im;
}

// the flower shop's goods: a cut flower with its stem, and a tied bouquet
inline Img mkIconFlower(int m) {
    Img im(16, 16);
    const u32 stem = HEX(0x3f8a42), stemL = HEX(0x58a85a);
    if (m == M_BOUQUET) {
        im.rect(7, 8, 2, 7, stem);
        im.rect(5, 9, 6, 1, stem);
        im.rect(6, 12, 4, 2, HEX(0xe8584f));  // ribbon
        im.rect(7, 12, 2, 4, HEX(0xe8584f));
        const u32 fc[5] = {HEX(0xe8584f), HEX(0xf2c94c), HEX(0xa070e0), HEX(0xf4f0ea), HEX(0xe88ab8)};
        const int fx[5] = {4, 8, 12, 6, 10}, fy[5] = {6, 4, 6, 8, 8};
        for (int i = 0; i < 5; i++) {
            im.ellipse(fx[i], fy[i], 2.4f, 2.4f, fc[i]);
            im.set(fx[i], fy[i], HEX(0xfff6d0));
        }
    } else {
        im.rect(7, 7, 2, 9, stem);
        im.rect(9, 11, 3, 1, stemL);
        im.rect(4, 10, 3, 1, stemL);
        if (m == M_ROSE) {
            im.ellipse(8, 5, 4, 4, HEX(0xd8384a));
            im.ellipse(8, 5, 2.5f, 2.5f, HEX(0xf0586a));
            im.set(8, 5, HEX(0xa02030));
        } else if (m == M_TULIP) {
            im.rect(5, 2, 6, 6, HEX(0xf2a03a));
            im.rect(5, 2, 1, 3, HEX(0xf2a03a));
            im.set(5, 1, HEX(0xf2a03a));
            im.set(8, 1, HEX(0xf2a03a));
            im.set(10, 1, HEX(0xf2a03a));
            im.rect(7, 3, 2, 4, HEX(0xffc870));
        } else if (m == M_LAVENDER) {
            for (int i = 0; i < 6; i++) {
                im.rect(7, 1 + i, 2, 1, i & 1 ? HEX(0x8f6fe0) : HEX(0xa488f0));
                im.set(6 + (i & 1) * 3, 2 + i, HEX(0x8f6fe0));
            }
        } else {  // sunflower
            im.ellipse(8, 5, 5, 5, HEX(0xf2c94c));
            im.ellipse(8, 5, 2.6f, 2.6f, HEX(0x6b4423));
            im.set(8, 5, HEX(0x4a2e18));
        }
    }
    im.outline(OUT);
    return im;
}

// processed crafting components (planks, leather, bone fragments, extract, grease)
inline Img mkIconProcessed(int m) {
    Img im(16, 16);
    switch (m) {
        case M_PLANK:  // two sawn boards, end grain and a nail
            im.rect(2, 4, 12, 4, HEX(0xc89458));
            im.rect(2, 4, 12, 1, HEX(0xe0b878));
            im.rect(2, 7, 12, 1, HEX(0x8a5a2c));
            im.rect(3, 9, 12, 4, HEX(0xb07e44));
            im.rect(3, 9, 12, 1, HEX(0xd0a468));
            im.rect(3, 12, 12, 1, HEX(0x7a4e26));
            im.line(5, 5, 9, 6, HEX(0x9a6a36)); im.line(6, 10, 12, 11, HEX(0x8a5a2c));
            im.set(12, 5, HEX(0x5a5a64));
            break;
        case M_LEATHER:  // a flat tanned sheet with stitched edges
            im.rect(2, 3, 12, 10, HEX(0xa8703c));
            im.rect(2, 3, 12, 2, HEX(0xc48a50));
            im.rect(2, 11, 12, 2, HEX(0x7a4e26));
            for (int x = 3; x < 13; x += 2) im.set(x, 4, HEX(0xe8c890));
            for (int x = 3; x < 13; x += 2) im.set(x, 12, HEX(0x5a3a1c));
            im.line(5, 7, 10, 8, HEX(0x8a5a2c));
            break;
        case M_BONEFRAG:  // a handful of bone chips
            im.rect(3, 4, 4, 2, HEX(0xf2ead3)); im.rect(8, 3, 3, 3, HEX(0xd9cfae));
            im.rect(5, 8, 5, 2, HEX(0xf2ead3)); im.rect(11, 8, 2, 4, HEX(0xd9cfae));
            im.rect(3, 11, 3, 2, HEX(0xd9cfae)); im.rect(7, 11, 3, 2, HEX(0xf2ead3));
            im.set(4, 4, HEX(0xffffff)); im.set(9, 3, HEX(0xffffff));
            break;
        case M_EXTRACT:  // a small corked vial of green liquid
            im.rect(6, 2, 4, 2, HEX(0x8a5a2c));
            im.rect(6, 4, 4, 2, HEX(0xcfe6ee));
            im.ellipse(8, 10, 4.5f, 4, HEX(0xcfe6ee));
            im.ellipse(8, 10.6f, 3.6f, 3.1f, HEX(0x5aa83c));
            im.rect(5, 9, 6, 1, HEX(0x86d85e));
            im.set(6, 8, HEX(0xffffff));
            break;
        case M_GREASE:  // a clay pot of pale fat
            im.rect(4, 6, 8, 7, HEX(0xa8703c));
            im.rect(3, 5, 10, 2, HEX(0xc48a50));
            im.rect(5, 6, 6, 1, HEX(0xf4ecc8));
            im.rect(4, 12, 8, 1, HEX(0x7a4e26));
            im.set(5, 8, HEX(0xc48a50));
            break;
    }
    return im;
}
