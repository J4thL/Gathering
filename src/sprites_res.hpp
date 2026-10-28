// sprites_res.hpp - art for the gathering and crafting systems, painted in code like everything else:
// icons for the land materials, tools, consumables and placeables; the new resource nodes; the depleted / regrowing looks;
// the campfire and chest; the spear and longbow. Included from sprites.hpp after the outline colour OUT is defined.
#pragma once
#include <array>
#include "data.hpp"

// ----------------------------------------------------------------- material icons for the land materials (M_WOOD ...)
inline Img mkIconLand(int m) {
    Img im(16, 16);
    auto rockBody = [&](u32 l, u32 md, u32 d) { im.ellipseShaded(8, 9, 6.2f, 5.4f, l, md, d); };
    switch (m) {
        case M_WOOD:  // a short log, end grain facing us
            im.rect(3, 6, 10, 7, HEX(0x8a5a2c));
            im.rect(3, 6, 10, 2, HEX(0xa8743c));
            im.rect(3, 11, 10, 2, HEX(0x6a4220));
            im.ellipse(4, 9.5f, 2.6f, 3.6f, HEX(0xe0b878));
            im.ellipse(4, 9.5f, 1.6f, 2.4f, HEX(0xc89a58));
            im.set(4, 9, HEX(0x8a5a2c));
            im.line(7, 8, 11, 8, HEX(0x6a4220));
            im.line(7, 11, 12, 11, HEX(0x7a4e26));
            break;
        case M_BRANCH:
            im.line(3, 13, 12, 3, HEX(0x8a5a2c));
            im.line(4, 13, 13, 3, HEX(0x6a4220));
            im.line(7, 9, 4, 5, HEX(0x8a5a2c));
            im.line(9, 7, 13, 8, HEX(0x8a5a2c));
            im.set(3, 12, HEX(0xc89a58));
            im.set(2, 13, HEX(0xc89a58));
            im.set(12, 2, HEX(0x6ab84a));
            im.set(4, 4, HEX(0x6ab84a));
            break;
        case M_STONE: rockBody(HEX(0xc4c7d2), HEX(0x9a9ca8), HEX(0x6a6c78)); im.set(5, 6, HEX(0xeceef4)); im.line(8, 8, 11, 10, HEX(0x6a6c78)); break;
        case M_FIBER:
            for (int i = 0; i < 6; i++) im.line(3 + i * 2, 14, 4 + i * 2 - (i % 3), 2, i % 2 ? HEX(0xd8c890) : HEX(0xb8a870));
            im.rect(3, 8, 10, 2, HEX(0x8a5a2c));
            im.rect(3, 8, 10, 1, HEX(0xa8743c));
            break;
        case M_HERB:
            im.line(8, 14, 8, 6, HEX(0x3f8a3a));
            for (auto d : {std::pair<int, int>{5, 5}, {11, 5}, {4, 9}, {12, 9}, {8, 3}}) {
                im.ellipseShaded((float)d.first, (float)d.second, 2.6f, 2.0f, HEX(0x8ad46a), HEX(0x5cb04a), HEX(0x3f8a3a));
                im.line(8, 10, d.first, d.second, HEX(0x3f8a3a));
            }
            break;
        case M_MUSHROOM:
            im.rect(6, 8, 4, 6, HEX(0xefe6cf));
            im.rect(9, 8, 1, 6, HEX(0xcfc2a0));
            im.ellipseShaded(8, 7, 5.6f, 3.8f, HEX(0xe8584f), HEX(0xc83c3a), HEX(0x8a2a2c));
            im.set(5, 6, HEX(0xffffff));
            im.set(8, 4, HEX(0xffffff));
            im.set(10, 7, HEX(0xffffff));
            im.set(7, 6, HEX(0xffffff));
            break;
        case M_SAGE:
            for (int i = 0; i < 5; i++) im.line(8, 14, 3 + i * 2, 3 + (i % 3) * 2, i % 2 ? HEX(0x8aa886) : HEX(0xa8c2a0));
            im.set(8, 3, HEX(0xc8a0e0));
            im.set(4, 5, HEX(0xc8a0e0));
            im.set(12, 4, HEX(0xc8a0e0));
            break;
        case M_PULP:
            im.ellipseShaded(8, 9, 5.8f, 5.4f, HEX(0x72bb62), HEX(0x4a9a4c), HEX(0x2f7a3a));
            im.ellipse(10, 8, 2.8f, 2.6f, HEX(0xd8f0b8));
            im.ellipse(10, 8, 1.6f, 1.4f, HEX(0xeaf8d0));
            im.set(4, 6, HEX(0xe8f0c0));
            im.set(6, 12, HEX(0xe8f0c0));
            break;
        case M_BOGWORT:
            im.ellipseShaded(8, 11, 5.6f, 2.8f, HEX(0x78a24c), HEX(0x4f7a3a), HEX(0x2f5a2c));
            for (auto d : {std::pair<int, int>{5, 4}, {8, 2}, {11, 5}}) {
                im.line(8, 11, d.first, d.second + 1, HEX(0x4f7a3a));
                im.ellipse((float)d.first, (float)d.second, 1.7f, 1.7f, HEX(0x8a4fc0));
                im.set(d.first, d.second - 1, HEX(0xc8a0e8));
            }
            break;
        case M_NIGHTSHADE:
            im.line(8, 14, 8, 5, HEX(0x2f5a3a));
            im.line(8, 10, 4, 6, HEX(0x2f5a3a));
            im.line(8, 8, 12, 5, HEX(0x2f5a3a));
            for (auto d : {std::pair<int, int>{4, 5}, {8, 4}, {12, 4}, {6, 8}, {10, 7}}) {
                im.ellipseShaded((float)d.first, (float)d.second, 1.9f, 1.9f, HEX(0x7a4aa8), HEX(0x4a2a6a), HEX(0x241040));
                im.set(d.first - 1, d.second - 1, HEX(0xc8b0e8));
            }
            im.ellipse(8, 12, 3.f, 1.4f, HEX(0x3f7a3a));
            break;
        case M_FROSTBLOOM: {
            im.line(8, 15, 8, 8, HEX(0x6f9a9a));
            im.line(8, 12, 5, 9, HEX(0x6f9a9a));
            u32 pc = HEX(0xa8e4ff), q = HEX(0xe8faff);
            for (auto d : {std::pair<int, int>{0, -4}, {4, 0}, {-4, 0}, {0, 4}, {3, -3}, {-3, -3}, {3, 3}, {-3, 3}}) im.ellipse(8.f + d.first, 6.f + d.second, 1.6f, 1.6f, pc);
            im.ellipse(8, 6, 2.4f, 2.4f, q);
            im.set(8, 6, HEX(0xffffff));
            break;
        }
        case M_ALPINE:
            im.line(8, 15, 8, 6, HEX(0x4a7a3a));
            im.line(8, 12, 4, 8, HEX(0x4a7a3a));
            im.line(8, 10, 12, 6, HEX(0x4a7a3a));
            for (auto d : {std::pair<int, int>{8, 4}, {4, 7}, {12, 5}}) {
                im.rect(d.first - 1, d.second - 1, 3, 3, HEX(0xf2f0ff));
                im.set(d.first, d.second, HEX(0xf2c94c));
            }
            break;
        case M_COPPER:
            rockBody(HEX(0xb4b6c0), HEX(0x8a8c98), HEX(0x5c5e6a));
            for (auto d : {std::pair<int, int>{5, 7}, {9, 6}, {7, 10}, {11, 10}, {10, 8}}) {
                im.set(d.first, d.second, HEX(0xd8844a));
                im.set(d.first + 1, d.second, HEX(0xf0a868));
            }
            im.set(6, 11, HEX(0x6ab8a0));
            break;
        case M_IRON:
            rockBody(HEX(0xa4a8b4), HEX(0x757986), HEX(0x4c505c));
            for (auto d : {std::pair<int, int>{5, 7}, {9, 6}, {7, 10}, {11, 10}}) {
                im.rect(d.first, d.second, 2, 2, HEX(0x6a4a3a));
                im.set(d.first, d.second, HEX(0xa86a4a));
            }
            break;
        case M_GOLD:
            rockBody(HEX(0xb4b6c0), HEX(0x8a8c98), HEX(0x5c5e6a));
            for (auto d : {std::pair<int, int>{4, 7}, {8, 5}, {7, 9}, {10, 8}, {11, 11}}) {
                im.rect(d.first, d.second, 2, 2, HEX(0xf2c94c));
                im.set(d.first, d.second, HEX(0xfff0a0));
            }
            break;
        case M_AMBER:
            im.ellipseShaded(8, 8, 5.6f, 5.0f, HEX(0xffd070), HEX(0xf0a030), HEX(0xb06a10));
            im.set(5, 5, HEX(0xfff4c8));
            im.set(6, 5, HEX(0xfff4c8));
            im.set(5, 6, HEX(0xfff4c8));
            im.rect(9, 9, 2, 2, HEX(0x6a3a10));  // a trapped insect
            break;
        case M_CRYSTAL:
            for (auto s : {std::array<int, 4>{8, 1, 4, 5}, std::array<int, 4>{5, 5, 3, 8}, std::array<int, 4>{11, 6, 3, 7}}) {
                int cx = s[0], top = s[1], hw = s[2] / 2 + 1, len = s[3];
                for (int y = 0; y < len; y++) {
                    int w = y < 2 ? y : hw;
                    for (int x = -w; x <= w; x++) im.set(cx + x, top + y + (s[1] > 3 ? 1 : 0), x < 0 ? HEX(0xe8faff) : (x == 0 ? HEX(0xa8e4ff) : HEX(0x6ab8e8)));
                }
            }
            im.rect(3, 13, 10, 2, HEX(0x7ab8d8));
            break;
        case M_ROPE:
            for (int i = 0; i < 3; i++) {
                im.ellipse(8, 9, 6.f - i * 1.6f, 4.6f - i * 1.2f, i & 1 ? HEX(0xb89a5a) : HEX(0xd8c080));
                im.ellipse(8, 9, 4.5f - i * 1.6f, 3.2f - i * 1.2f, 0u);
            }
            im.line(12, 10, 14, 14, HEX(0xd8c080));
            im.line(11, 11, 13, 14, HEX(0xb89a5a));
            break;
        case M_COPPERBAR:
        case M_IRONBAR:
        case M_GOLDBAR: {
            u32 l = m == M_COPPERBAR ? HEX(0xf0a868) : m == M_IRONBAR ? HEX(0xdce0ea) : HEX(0xfff0a0);
            u32 md = m == M_COPPERBAR ? HEX(0xd8844a) : m == M_IRONBAR ? HEX(0xa4a8b4) : HEX(0xf2c94c);
            u32 d = m == M_COPPERBAR ? HEX(0x9a5a2a) : m == M_IRONBAR ? HEX(0x6a6e7c) : HEX(0xb88a1a);
            for (int y = 0; y < 6; y++) {
                int inset = (5 - y) / 2;
                im.rect(2 + inset, 5 + y, 12 - inset * 2, 1, y < 2 ? l : (y < 5 ? md : d));
            }
            im.rect(2, 11, 12, 2, d);
            im.set(4, 6, HEX(0xffffff));
            im.set(5, 6, HEX(0xffffff));
            break;
        }
        default: break;
    }
    im.outline(OUT);
    return im;
}

// ----------------------------------------------------------------- tools: one 16x16 image used as icon and as the swinging sprite
inline Img mkIconTool(int k) {
    Img im(16, 16);
    const int tier = TOOLDEF[k].tier, type = TOOLDEF[k].type;
    u32 hl, hm, hd;  // head colours (light, mid, dark)
    switch (tier) {
        case 1: hl = HEX(0xc4c7d2), hm = HEX(0x9a9ca8), hd = HEX(0x6a6c78); break;
        case 2: hl = HEX(0xf0a868), hm = HEX(0xd8844a), hd = HEX(0x9a5a2a); break;
        case 3: hl = HEX(0xeef0f6), hm = HEX(0xb4b8c6), hd = HEX(0x757986); break;
        default: hl = HEX(0xfff0a0), hm = HEX(0xf2c94c), hd = HEX(0xb88a1a); break;
    }
    // handle: lower left to upper right
    im.line(2, 14, 11, 5, HEX(0x6b4423));
    im.line(3, 14, 12, 5, HEX(0x8d5e33));
    if (tier == 1) {  // lashing
        im.set(9, 7, HEX(0xd8c890));
        im.set(10, 6, HEX(0xd8c890));
    } else {
        im.rect(8, 8, 2, 2, hd);
    }
    if (type == TT_AXE) {
        im.rect(9, 2, 5, 6, hm);
        im.rect(9, 2, 2, 6, hl);
        im.rect(13, 3, 1, 4, hd);
        im.rect(9, 7, 5, 1, hd);
        im.set(8, 3, hm);
        im.set(8, 4, hl);
    } else {
        im.line(6, 3, 13, 3, hm);  // the pick head: a bar across the top
        im.line(5, 4, 14, 4, hm);
        im.line(6, 3, 12, 3, hl);
        im.set(4, 5, hd);
        im.set(15, 5, hd);
        im.set(5, 5, hm);
        im.set(14, 5, hm);
        im.rect(10, 4, 2, 3, hd);
    }
    if (tier == 4) {  // gold band
        im.set(9, 8, HEX(0xfff0a0));
        im.set(10, 8, HEX(0xf2c94c));
    }
    im.outline(OUT);
    return im;
}

// ----------------------------------------------------------------- consumables
inline Img mkIconCons(int c) {
    Img im(16, 16);
    switch (c) {
        case C_SALVE:  // a leaf-wrapped poultice tied with twine
            im.ellipseShaded(8, 9, 6.f, 4.6f, HEX(0x8ad46a), HEX(0x5cb04a), HEX(0x3f8a3a));
            im.line(3, 9, 13, 9, HEX(0x3f8a3a));
            im.rect(7, 4, 2, 11, HEX(0xd8c890));
            im.rect(7, 4, 2, 1, HEX(0xb89a5a));
            im.set(5, 7, HEX(0xc8f0a8));
            break;
        case C_TONIC:  // a stoppered purple bottle
            im.rect(6, 2, 4, 3, HEX(0xb89a5a));
            im.rect(6, 5, 4, 2, HEX(0xdce6ee));
            im.ellipseShaded(8, 10, 4.8f, 4.4f, HEX(0xb07ad8), HEX(0x7a4aa8), HEX(0x4a2a6a));
            im.rect(5, 8, 1, 3, HEX(0xe8d0f8));
            im.set(9, 12, HEX(0xe8d0f8));
            break;
        case C_COOKED:  // a browned drumstick
            im.ellipseShaded(7, 8, 5.2f, 4.4f, HEX(0xd89050), HEX(0xb06a30), HEX(0x7a4218));
            im.line(10, 10, 14, 14, HEX(0xf2ead3));
            im.rect(13, 13, 3, 2, HEX(0xf2ead3));
            im.set(5, 6, HEX(0xf0c080));
            im.set(6, 6, HEX(0xf0c080));
            break;
        case C_BREW:  // a warm cup, steam rising
            im.rect(4, 7, 8, 7, HEX(0xb06a30));
            im.rect(4, 7, 8, 2, HEX(0xe8a060));
            im.rect(5, 8, 6, 2, HEX(0xa8e4ff));
            im.rect(12, 8, 2, 4, HEX(0x8a5224));
            im.set(6, 4, HEX(0xeef6ff));
            im.set(7, 3, HEX(0xeef6ff));
            im.set(9, 4, HEX(0xeef6ff));
            im.set(10, 2, HEX(0xeef6ff));
            break;
        default:  // cactus water: a green gourd flask with a droplet
            im.rect(7, 2, 2, 3, HEX(0xb89a5a));
            im.ellipseShaded(8, 10, 5.f, 4.8f, HEX(0x8ad46a), HEX(0x4a9a4c), HEX(0x2f7a3a));
            im.rect(5, 9, 6, 3, HEX(0x7ad8e8));
            im.set(6, 8, HEX(0xe8f8ff));
            im.set(11, 4, HEX(0x7ad8e8));
            break;
    }
    im.outline(OUT);
    return im;
}

// ----------------------------------------------------------------- campfire / chest: the item icon and the world sprite
inline Img mkCampfire(int frame) {
    Img im(18, 18);
    for (int i = 0; i < 7; i++) {  // stone ring
        float a = i / 7.f * 6.2832f;
        im.ellipseShaded(9 + cosf(a) * 6.2f, 13 + sinf(a) * 2.8f, 2.1f, 1.8f, HEX(0xc4c7d2), HEX(0x9a9ca8), HEX(0x6a6c78));
    }
    im.line(5, 14, 12, 11, HEX(0x6b4423));
    im.line(13, 14, 6, 11, HEX(0x8d5e33));
    // flames: three tongues that flicker with the frame
    int h1 = 7 + (frame % 3), h2 = 5 + ((frame + 1) % 3), h3 = 6 + ((frame + 2) % 3);
    im.ellipse(9, 11 - h1 / 2.f, 2.8f, h1 / 2.f + 1, HEX(0xe8584f));
    im.ellipse(6.5f, 11 - h2 / 2.f, 1.8f, h2 / 2.f + 0.5f, HEX(0xf0953a));
    im.ellipse(11.5f, 11 - h3 / 2.f, 1.8f, h3 / 2.f + 0.5f, HEX(0xf0953a));
    im.ellipse(9, 11 - h1 / 3.f, 1.6f, h1 / 3.f + 0.5f, HEX(0xf2c94c));
    im.set(9, 9, HEX(0xfff4c8));
    return im;
}
inline Img mkChest() {
    Img im(16, 14);
    im.rect(1, 4, 14, 9, HEX(0x8a5a2c));
    im.rect(1, 4, 14, 2, HEX(0xa8743c));
    im.rect(1, 11, 14, 2, HEX(0x6a4220));
    im.ellipse(8, 4, 7.f, 3.f, HEX(0x9a6a34));
    im.rect(1, 4, 14, 1, HEX(0x6a4220));
    im.rect(0, 5, 2, 7, HEX(0x757986));  // iron straps
    im.rect(14, 5, 2, 7, HEX(0x757986));
    im.rect(7, 4, 2, 4, HEX(0xb4b8c6));  // lock
    im.set(7, 5, HEX(0xf2c94c));
    im.line(3, 8, 6, 8, HEX(0x6a4220));
    im.line(10, 8, 13, 8, HEX(0x6a4220));
    im.outline(OUT);
    return im;
}

// ----------------------------------------------------------------- resource nodes added to the original seven
inline Img mkNodeNew(int kind) {
    Img im(18, 18);
    auto oreRock = [&](u32 l, u32 md, u32 d) { im.ellipseShaded(9, 10, 7, 6, l, md, d); };
    switch (kind) {
        case RN_HERB:
            im.line(9, 16, 9, 9, HEX(0x3f8a3a));
            for (auto d : {std::pair<int, int>{5, 11}, {13, 11}, {6, 7}, {12, 7}, {9, 5}}) {
                im.ellipseShaded((float)d.first, (float)d.second, 3.0f, 2.3f, HEX(0x8ad46a), HEX(0x5cb04a), HEX(0x3f8a3a));
                im.line(9, 14, d.first, d.second, HEX(0x3f8a3a));
            }
            im.set(9, 4, HEX(0xffffff));
            im.set(6, 6, HEX(0xffe880));
            break;
        case RN_MUSHROOM:
            for (auto d : {std::array<int, 3>{5, 12, 3}, std::array<int, 3>{11, 13, 4}, std::array<int, 3>{8, 10, 2}}) {
                im.rect(d[0] - 1, d[1], 2, 4, HEX(0xefe6cf));
                im.ellipseShaded((float)d[0], (float)d[1], d[2] + 0.6f, d[2] * 0.7f + 0.6f, HEX(0xe8584f), HEX(0xc83c3a), HEX(0x8a2a2c));
                im.set(d[0] - 1, d[1] - 1, HEX(0xffffff));
            }
            break;
        case RN_BRANCH:
            im.line(2, 14, 14, 11, HEX(0x6b4423));
            im.line(2, 13, 14, 10, HEX(0x8d5e33));
            im.line(6, 12, 4, 8, HEX(0x8d5e33));
            im.line(10, 11, 13, 7, HEX(0x8d5e33));
            im.line(4, 11, 12, 15, HEX(0x6b4423));
            im.set(4, 7, HEX(0x6ab84a));
            im.set(13, 6, HEX(0x6ab84a));
            break;
        case RN_FLAX:
            for (int i = 0; i < 5; i++) {
                int x = 4 + i * 2;
                im.line(x, 16, x + (i % 2 ? 1 : -1), 4 + (i % 3) * 2, HEX(0x8ab858));
                im.set(x + (i % 2 ? 1 : -1), 3 + (i % 3) * 2, HEX(0x7aa8f0));
                im.set(x + (i % 2 ? 1 : -1), 4 + (i % 3) * 2, HEX(0xb8d0ff));
            }
            break;
        case RN_DRYWOOD:
            im.line(2, 13, 15, 10, HEX(0x9a8a70));
            im.line(2, 12, 15, 9, HEX(0xc8b898));
            im.line(4, 15, 13, 14, HEX(0x8a7a60));
            im.line(4, 14, 13, 13, HEX(0xb8a888));
            im.line(7, 11, 6, 6, HEX(0xb8a888));
            im.set(6, 5, HEX(0xd8c8a8));
            break;
        case RN_COPPER:
            oreRock(HEX(0xb4b6c0), HEX(0x8a8c98), HEX(0x5c5e6a));
            for (auto d : {std::pair<int, int>{5, 7}, {10, 6}, {8, 10}, {12, 10}, {6, 12}}) {
                im.rect(d.first, d.second, 2, 2, HEX(0xd8844a));
                im.set(d.first, d.second, HEX(0xf0a868));
            }
            im.set(11, 8, HEX(0x6ab8a0));
            break;
        case RN_NIGHTSHADE:
            im.ellipseShaded(9, 13, 6.5f, 3.2f, HEX(0x4f8a4a), HEX(0x356a38), HEX(0x244a2a));
            for (auto d : {std::pair<int, int>{4, 8}, {9, 5}, {14, 8}, {7, 10}, {12, 11}}) {
                im.line(9, 13, d.first, d.second + 1, HEX(0x356a38));
                im.ellipseShaded((float)d.first, (float)d.second, 2.f, 2.f, HEX(0x7a4aa8), HEX(0x4a2a6a), HEX(0x241040));
                im.set(d.first - 1, d.second - 1, HEX(0xc8b0e8));
            }
            break;
        case RN_AMBER:
            im.ellipseShaded(9, 13, 7.f, 3.f, HEX(0x7a6448), HEX(0x5a4630), HEX(0x3a2c1c));
            for (auto d : {std::array<float, 3>{6, 9, 3.f}, std::array<float, 3>{11.5f, 8, 3.6f}, std::array<float, 3>{9, 5.5f, 2.4f}})
                im.ellipseShaded(d[0], d[1], d[2], d[2] * 0.9f, HEX(0xffd070), HEX(0xf0a030), HEX(0xb06a10));
            im.set(5, 8, HEX(0xfff4c8));
            im.set(10, 7, HEX(0xfff4c8));
            im.set(8, 4, HEX(0xfff4c8));
            break;
        default:  // RN_GOLD
            oreRock(HEX(0xa0a4b2), HEX(0x747886), HEX(0x4a4e5c));
            for (auto d : {std::pair<int, int>{4, 6}, {9, 5}, {7, 9}, {12, 8}, {5, 11}, {11, 12}}) {
                im.rect(d.first, d.second, 2, 2, HEX(0xf2c94c));
                im.set(d.first, d.second, HEX(0xfff0a0));
            }
            break;
    }
    im.outline(OUT);
    return im;
}

// ----------------------------------------------------------------- the depleted and regrowing looks
inline Img mkStump() {
    Img im(14, 11);
    im.rect(2, 4, 10, 6, HEX(0x6a4220));
    im.rect(2, 4, 3, 6, HEX(0x8a5a2c));
    im.ellipse(7, 4, 5.f, 2.6f, HEX(0xe0b878));
    im.ellipse(7, 4, 3.2f, 1.6f, HEX(0xc89a58));
    im.set(7, 4, HEX(0x8a5a2c));
    im.set(3, 9, HEX(0x4a2c14));
    im.set(11, 10, HEX(0x4a2c14));
    im.outline(OUT);
    return im;
}
inline Img mkRubble() {
    Img im(14, 8);
    im.ellipseShaded(5, 5, 3.4f, 2.4f, HEX(0xb4b6c0), HEX(0x8a8c98), HEX(0x5c5e6a));
    im.ellipseShaded(10, 5, 2.6f, 2.0f, HEX(0xa4a6b2), HEX(0x7a7c88), HEX(0x4c4e5a));
    im.ellipseShaded(7.5f, 3, 2.f, 1.6f, HEX(0xc4c7d2), HEX(0x9a9ca8), HEX(0x6a6c78));
    im.outline(OUT);
    return im;
}
inline Img mkStub() {
    Img im(12, 8);
    im.ellipse(6, 6, 5.f, 1.6f, HEX(0x6a5030));
    im.line(4, 6, 3, 3, HEX(0x7a9a4a));
    im.line(7, 6, 8, 2, HEX(0x7a9a4a));
    im.line(9, 6, 10, 4, HEX(0x7a9a4a));
    im.set(3, 3, HEX(0xa8c868));
    return im;
}
inline Img mkSprout() {
    Img im(10, 10);
    im.line(5, 9, 5, 4, HEX(0x6a8a3a));
    im.ellipseShaded(3, 4, 2.2f, 1.5f, HEX(0x9ad46a), HEX(0x6ab84a), HEX(0x4a8a3a));
    im.ellipseShaded(7, 3, 2.2f, 1.5f, HEX(0x9ad46a), HEX(0x6ab84a), HEX(0x4a8a3a));
    im.ellipse(5, 9, 3.f, 1.f, HEX(0x6a5030));
    return im;
}

// ----------------------------------------------------------------- new weapons: icons (16x16) and the sprites seen in flight
inline Img mkIconWpnNew(int w) {
    Img im(16, 16);
    switch (w) {
        case W_SPEAR:
        case W_IRONSPEAR: {
            u32 l = w == W_SPEAR ? HEX(0xc4c7d2) : HEX(0xeef0f6), md = w == W_SPEAR ? HEX(0x9a9ca8) : HEX(0xb4b8c6), d = w == W_SPEAR ? HEX(0x6a6c78) : HEX(0x757986);
            im.line(2, 14, 11, 5, HEX(0x6b4423));
            im.line(3, 14, 12, 5, HEX(0x8d5e33));
            im.line(10, 7, 11, 6, w == W_SPEAR ? HEX(0xd8c890) : d);
            for (int i = 0; i < 4; i++) {  // a leaf-shaped head
                im.set(11 + i, 5 - i, l);
                im.set(12 + i, 5 - i, md);
                im.set(11 + i, 4 - i, md);
            }
            im.set(14, 1, l);
            im.set(15, 0, l);
            im.set(12, 6, d);
            im.set(10, 4, d);
            break;
        }
        default: {  // W_LONGBOW
            for (int y = 1; y <= 14; y++) {
                float t = (y - 1) / 13.f;
                int x = (int)(4 + sinf(t * 3.14159f) * 6.f);
                im.set(x, y, HEX(0x8d5e33));
                im.set(x + 1, y, HEX(0x6b4423));
            }
            im.line(4, 1, 4, 14, HEX(0xe8e0c8));
            im.rect(9, 7, 2, 2, HEX(0x3b2616));
            im.set(4, 5, HEX(0xb89a5a));
            im.set(4, 10, HEX(0xb89a5a));
            break;
        }
    }
    im.outline(OUT);
    return im;
}
inline Img mkWpnWorldNew(int w) {
    switch (w) {
        case W_SPEAR:
        case W_IRONSPEAR: {
            Img im(20, 5);
            u32 l = w == W_SPEAR ? HEX(0xc4c7d2) : HEX(0xeef0f6), md = w == W_SPEAR ? HEX(0x9a9ca8) : HEX(0xb4b8c6);
            im.rect(1, 2, 14, 1, HEX(0x8d5e33));
            im.rect(1, 1, 14, 1, HEX(0x6b4423));
            im.rect(15, 1, 4, 3, md);
            im.rect(15, 1, 3, 1, l);
            im.set(19, 2, l);
            im.outline(OUT);
            return im;
        }
        default: {  // longbow arc seen from the side while drawn
            Img im(12, 14);
            for (int y = 0; y < 14; y++) {
                float t = y / 13.f;
                int x = (int)(2 + sinf(t * 3.14159f) * 6.f);
                im.set(x, y, HEX(0x8d5e33));
                im.set(x + 1, y, HEX(0x6b4423));
            }
            im.line(2, 0, 2, 13, HEX(0xe8e0c8));
            im.outline(OUT);
            return im;
        }
    }
}

struct ResAssets {
    Spr toolIcon[TL_COUNT], toolSwing[TL_COUNT];  // swing sprites pivot on the handle's end
    Spr consIcon[C_COUNT], placeIcon[PL_COUNT];
    Spr campfire[3], chest;
    Spr stump, rubble, stub, sprout;
};
