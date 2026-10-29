// sprites_gear.hpp - icons and in-flight sprites for the crafted weapons added with the trophy materials
// (fang dagger, reinforced spear, recurve bow, heavy axe and the four biome weapons), drawn in code like everything else.
// The older weapons live in sprites.hpp (flint .. javelin, bow, crossbow) and sprites_res.hpp (spears, longbow).
#pragma once
#include "data.hpp"
#include "items.hpp"

// weapons from W_FANGDAGGER on are "new-new": index into this table with w - W_FANGDAGGER
struct WpnArt {
    u32 haft, haftDark;      // wood / shaft colours
    u32 headL, headM, headD; // head: light, mid, dark
    u32 accent;              // feather, binding or venom tint
};
static const WpnArt WPN_ART[W_COUNT - W_FANGDAGGER] = {
    {HEX(0x8d5e33), HEX(0x6b4423), HEX(0xfaf4e0), HEX(0xe0d6b4), HEX(0xa89870), HEX(0x6b4423)},  // fang dagger: bone-white blade, leather grip
    {HEX(0x8d5e33), HEX(0x6b4423), HEX(0xeef0f6), HEX(0xb4b8c6), HEX(0x757986), HEX(0xf2ead3)},  // reinforced spear: steel head, bone lashings
    {HEX(0xc89458), HEX(0x8a5a2c), HEX(0xe8e0c8), HEX(0xb89a5a), HEX(0x8a5a2c), HEX(0xe8e0c8)},  // recurve bow: pale wood, sinew string
    {HEX(0x8d5e33), HEX(0x6b4423), HEX(0xc8ccd4), HEX(0x7e8490), HEX(0x4a4e5c), HEX(0xd8dcc0)},  // heavy throwing axe: dark iron, gator-tooth edge
    {HEX(0xb87a3c), HEX(0x8a5a2c), HEX(0xfff0a0), HEX(0xf2c94c), HEX(0xb8862a), HEX(0xd8443f)},  // sunstrike javelin: gold head, red plume
    {HEX(0x4a5a34), HEX(0x2f3a20), HEX(0xd8a8f0), HEX(0xa060d0), HEX(0x4a2a6a), HEX(0x6ab04a)},  // bogbane spear: green haft, venom tip
    {HEX(0xe8e0c8), HEX(0xb8ae90), HEX(0xf0fbff), HEX(0xa8e4ff), HEX(0x5a98c0), HEX(0x4a3e40)},  // frost harpoon: bone haft, ice barbs
    {HEX(0x8d5e33), HEX(0x6b4423), HEX(0xdfe4f0), HEX(0x8a90a8), HEX(0x4e5470), HEX(0xefe4cc)},  // cliffsplitter axe: grey stone head
};

inline Img mkIconWpnGear(int w) {
    Img im(16, 16);
    const WpnArt& A = WPN_ART[w - W_FANGDAGGER];
    switch (w) {
        case W_FANGDAGGER:  // a short curved blade made from a fang, wrapped grip
            im.line(5, 10, 13, 2, A.headM);
            im.line(6, 10, 13, 3, A.headD);
            im.line(4, 10, 12, 2, A.headL);
            im.set(13, 1, A.headL);
            im.rect(4, 10, 3, 3, A.haftDark);  // guard
            im.line(2, 14, 5, 11, A.haft);
            im.line(3, 14, 6, 11, A.haftDark);
            im.set(2, 14, A.accent);
            break;
        case W_RSPEAR:  // steel head, tusk barbs, bone lashings
            im.line(2, 14, 11, 5, A.haftDark);
            im.line(3, 14, 12, 5, A.haft);
            for (int i = 0; i < 5; i++) {  // a long leaf head
                im.set(11 + i, 5 - i, A.headL);
                im.set(12 + i, 5 - i, A.headM);
                im.set(11 + i, 4 - i, A.headM);
                im.set(12 + i, 4 - i, A.headD);
            }
            im.set(15, 0, A.headL);
            im.set(9, 7, A.accent); im.set(10, 8, A.accent); im.set(7, 9, A.accent); im.set(8, 10, A.accent);  // lashings
            im.set(12, 7, A.headD); im.set(10, 5, A.headD);  // barbs
            break;
        case W_RECURVE:  // an S-curved limb with flicked tips and a pale string
            for (int y = 1; y <= 14; y++) {
                const float t = (y - 1) / 13.f;
                const int x = (int)lroundf(4 + sinf(t * 3.14159f) * 6.f - 2.f * sinf(t * 6.2832f) * 0.6f + (y <= 2 || y >= 13 ? -1.f : 0.f));
                im.set(x, y, A.haft);
                im.set(x + 1, y, A.haftDark);
            }
            im.line(4, 1, 4, 14, A.accent);
            im.rect(9, 7, 2, 2, HEX(0x3b2616));
            im.set(4, 7, HEX(0xd8c890));
            break;
        case W_HVYAXE:  // a broad head on a thick haft, edge set with a tooth
            im.line(3, 14, 10, 7, A.haftDark);
            im.line(4, 14, 11, 7, A.haft);
            im.line(3, 13, 10, 6, A.haft);
            im.rect(7, 1, 7, 7, A.headM);
            im.rect(7, 1, 3, 7, A.headL);
            im.rect(13, 2, 1, 5, A.headD);
            im.rect(7, 7, 7, 1, A.headD);
            im.set(14, 4, A.accent); im.set(14, 3, A.accent);
            break;
        case W_SUNJAV:  // gold-tipped javelin with a red plume
            im.line(2, 14, 12, 4, A.haftDark);
            im.line(3, 14, 13, 4, A.haft);
            im.line(11, 5, 14, 1, A.headM);
            im.line(12, 5, 15, 1, A.headL);
            im.line(10, 5, 13, 1, A.headD);
            im.set(14, 0, A.headL);
            im.rect(1, 12, 3, 3, A.accent); im.set(0, 13, A.accent);
            break;
        case W_BOGSPEAR:  // dark haft, venom-dipped head with a green drip
            im.line(2, 14, 11, 5, A.haftDark);
            im.line(3, 14, 12, 5, A.haft);
            for (int i = 0; i < 4; i++) {
                im.set(11 + i, 5 - i, A.headL);
                im.set(12 + i, 5 - i, A.headM);
                im.set(11 + i, 4 - i, A.headM);
            }
            im.set(15, 0, A.headL); im.set(12, 6, A.headD); im.set(10, 4, A.headD);
            im.set(12, 8, A.accent); im.set(12, 9, A.accent);
            break;
        case W_HARPOON:  // bone shaft, ice-blue head with two back-swept barbs
            im.line(2, 14, 11, 5, A.haftDark);
            im.line(3, 14, 12, 5, A.haft);
            im.line(11, 5, 15, 1, A.headM);
            im.line(12, 5, 15, 2, A.headL);
            im.line(11, 5, 9, 3, A.headM); im.line(11, 5, 13, 7, A.headM);  // barbs
            im.set(15, 0, A.headL); im.set(9, 3, A.headD); im.set(13, 7, A.headD);
            im.set(3, 12, A.accent);
            break;
        case W_CLIFFAXE:  // a grey stone head lashed to a wooden haft
            im.line(3, 14, 11, 5, A.haftDark);
            im.line(4, 14, 12, 5, A.haft);
            im.rect(8, 2, 5, 5, A.headM);
            im.rect(8, 2, 2, 5, A.headL);
            im.rect(12, 3, 1, 3, A.headD);
            im.set(7, 3, A.headL); im.set(7, 4, A.headL);
            im.set(10, 8, A.accent); im.set(11, 7, A.accent);
            break;
    }
    im.outline(OUT);
    return im;
}

inline Img mkWpnWorldGear(int w) {
    const WpnArt& A = WPN_ART[w - W_FANGDAGGER];
    switch (w) {
        case W_FANGDAGGER: {
            Img im(14, 5);
            im.rect(1, 2, 4, 1, A.haft);
            im.rect(1, 1, 4, 1, A.haftDark);
            im.rect(5, 0, 1, 4, A.haftDark);
            for (int x = 6; x <= 11; x++) im.rect(x, x >= 10 ? 2 : 1, 1, x >= 10 ? 1 : 2, x % 2 ? A.headM : A.headL);
            im.set(12, 2, A.headL);
            im.outline(OUT);
            return im;
        }
        case W_RECURVE: {  // limb seen from the side while drawn
            Img im(12, 14);
            for (int y = 0; y < 14; y++) {
                const float t = y / 13.f;
                const int x = (int)(2 + sinf(t * 3.14159f) * 6.f);
                im.set(x, y, A.haft);
                im.set(x + 1, y, A.haftDark);
            }
            im.line(2, 0, 2, 13, A.accent);
            im.outline(OUT);
            return im;
        }
        case W_HVYAXE:
        case W_CLIFFAXE: {
            const bool heavy = w == W_HVYAXE;
            Img im(heavy ? 18 : 16, heavy ? 14 : 12);
            const int hx = heavy ? 10 : 9, hh = heavy ? 10 : 8;
            im.rect(1, heavy ? 7 : 6, hx + 2, 1, A.haft);
            im.rect(1, heavy ? 6 : 5, hx + 2, 1, A.haftDark);
            im.rect(hx, 2, 5, hh, A.headM);
            im.rect(hx + 3, 3, 1, hh - 2, A.headL);
            im.rect(hx, 2 + hh - 2, 5, 2, A.headD);
            im.outline(OUT);
            return im;
        }
        default: {  // the spear family: a long haft, a coloured head, an accent near the grip
            Img im(22, 5);
            im.rect(1, 2, 16, 1, A.haft);
            im.rect(1, 1, 16, 1, A.haftDark);
            im.rect(17, 1, 4, 3, A.headM);
            im.rect(17, 1, 3, 1, A.headL);
            im.set(21, 2, A.headL);
            im.set(15, 3, A.accent); im.set(16, 3, A.accent);
            if (w == W_HARPOON) { im.set(16, 0, A.headM); im.set(16, 4, A.headM); }
            if (w == W_SUNJAV) { im.rect(0, 1, 2, 3, A.accent); }
            im.outline(OUT);
            return im;
        }
    }
}

// carried gear: worn-looking bundles, each with its own silhouette so the pack grid reads at a glance
inline Img mkIconCarry(int c) {
    Img im(16, 16);
    switch (c) {
        case CG_WOLFCLOAK: {  // grey pelt with a ruff and a clasp
            const u32 a = HEX(0x8a8f99), b = HEX(0xaab0ba), d = HEX(0x5a5e68);
            im.rect(3, 4, 10, 10, a);
            im.rect(2, 11, 12, 3, a);
            im.rect(2, 3, 12, 3, b);
            im.rect(4, 6, 2, 7, b); im.rect(10, 6, 2, 8, d);
            for (int x = 2; x < 14; x += 2) im.set(x, 14, d);
            im.set(7, 5, HEX(0xf2c94c)); im.set(8, 5, HEX(0xf2c94c));
            break;
        }
        case CG_PARKA: {  // thick blue-grey coat with a white fur hood
            const u32 a = HEX(0x5a7898), d = HEX(0x3a526c);
            im.rect(3, 4, 10, 10, a);
            im.rect(1, 5, 3, 7, a); im.rect(12, 5, 3, 7, a);
            im.rect(4, 1, 8, 4, HEX(0xf2f2ee));
            im.rect(6, 3, 4, 2, d);
            im.rect(3, 12, 10, 2, d);
            im.rect(7, 6, 2, 8, d);
            im.set(5, 9, HEX(0xf2f2ee)); im.set(10, 9, HEX(0xf2f2ee));
            break;
        }
        case CG_DESERTWRAPS: {  // sand-coloured cloth bands
            const u32 a = HEX(0xd8b878), d = HEX(0xa68448);
            for (int i = 0; i < 4; i++) {
                im.rect(3, 3 + i * 3, 10, 2, i % 2 ? a : d);
                im.rect(2 + (i % 2) * 2, 4 + i * 3, 4, 1, HEX(0xf2dca0));
            }
            im.rect(12, 2, 2, 12, d);
            break;
        }
        case CG_VENOMWRAPS: {  // dark green scaled wraps with a venom-yellow stitch
            const u32 a = HEX(0x4a6a3a), d = HEX(0x2e4524);
            for (int i = 0; i < 4; i++) im.rect(3, 3 + i * 3, 10, 2, i % 2 ? a : d);
            for (int y = 3; y < 14; y += 3) for (int x = 4; x < 13; x += 3) im.set(x, y, HEX(0x8ab05a));
            im.rect(12, 2, 2, 12, HEX(0xd8c43a));
            break;
        }
        case CG_WADERS: {  // tall greased boots joined at the hip
            const u32 a = HEX(0x6a5a2e), d = HEX(0x463b1e);
            im.rect(3, 1, 4, 11, a); im.rect(9, 1, 4, 11, a);
            im.rect(2, 11, 6, 3, d); im.rect(8, 11, 6, 3, d);
            im.rect(3, 1, 10, 2, d);
            im.rect(3, 4, 1, 6, HEX(0x8a7a44)); im.rect(9, 4, 1, 6, HEX(0x8a7a44));
            im.rect(6, 5, 4, 1, HEX(0xf2c94c));
            break;
        }
        case CG_HARNESS: {  // crossed straps, a buckle and a coiled rope loop
            const u32 a = HEX(0x8a5a2c), d = HEX(0x5e3d22);
            im.line(3, 2, 12, 13, a); im.line(4, 2, 13, 13, d);
            im.line(12, 2, 3, 13, a); im.line(11, 2, 2, 13, d);
            im.rect(6, 6, 4, 4, HEX(0xc8ccd4)); im.rect(7, 7, 2, 2, d);
            im.rect(1, 1, 3, 3, HEX(0xd6b886));
            break;
        }
        case CG_QUIVER: {  // a tall leather tube with fletched arrows poking out
            const u32 a = HEX(0x8a5a2c), d = HEX(0x5e3d22);
            im.rect(5, 5, 6, 10, a); im.rect(5, 12, 6, 3, d); im.rect(5, 5, 6, 1, d);
            im.rect(10, 6, 1, 8, HEX(0xb07a44));
            im.line(6, 5, 4, 0, HEX(0xd8443f)); im.line(8, 5, 8, 0, HEX(0xf2ead3)); im.line(10, 5, 12, 1, HEX(0xd8443f));
            im.set(7, 9, HEX(0xf2c94c));
            break;
        }
    }
    im.outline(OUT);
    return im;
}
