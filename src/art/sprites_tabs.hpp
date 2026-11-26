// sprites_tabs.hpp - the small icons of the book-style pack: the main tabs along the top (backpack, crafting, skills, bestiary,
// journal) and the category tabs down the left side (all, materials, weapons, tools, gear, supplies). 16 x 16, painted in code.
#pragma once
#include "core/data.hpp"

enum TabIcon { TI_PACK, TI_CRAFT, TI_SKILLS, TI_BEASTS, TI_JOURNAL, TI_ALL, TI_MATS, TI_WEAPONS, TI_TOOLS, TI_GEAR, TI_SUPPLIES, TI_COUNT };

inline Img mkTabIcon(int k) {
    Img im(16, 16);
    switch (k) {
        case TI_PACK:  // a satchel with a flap and buckle
            im.ellipseShaded(8, 9, 6.0f, 5.4f, HEX(0xc88a4a), HEX(0x9a6030), HEX(0x5a3418));
            im.rect(4, 4, 8, 4, HEX(0xa8703c)); im.rect(4, 8, 8, 1, HEX(0x5a3418));
            im.rect(7, 7, 2, 3, HEX(0xf2d060)); im.rect(5, 12, 6, 1, HEX(0x7a4a24));
            break;
        case TI_CRAFT:  // a hammer on an anvil
            im.rect(3, 11, 10, 2, HEX(0x6a6e7a)); im.rect(5, 9, 6, 2, HEX(0x8a8e9a)); im.rect(4, 13, 8, 1, HEX(0x4a4e5a));
            im.rect(7, 2, 2, 7, HEX(0x8a5a2c)); im.rect(4, 2, 8, 3, HEX(0xb0b4c0)); im.rect(4, 2, 8, 1, HEX(0xe0e4ee));
            break;
        case TI_SKILLS:  // a gold star
            im.rect(7, 2, 2, 12, HEX(0xf2c94c)); im.rect(2, 6, 12, 3, HEX(0xf2c94c));
            im.rect(4, 4, 8, 7, HEX(0xf2c94c)); im.rect(5, 12, 2, 2, HEX(0xf2c94c)); im.rect(9, 12, 2, 2, HEX(0xf2c94c));
            im.rect(6, 5, 2, 2, HEX(0xfff0a0));
            break;
        case TI_BEASTS:  // a paw print
            im.ellipse(8, 10.5f, 3.6f, 3.0f, HEX(0x8a5a3a));
            im.ellipse(3.5f, 6.5f, 1.5f, 2.0f, HEX(0x8a5a3a)); im.ellipse(6.5f, 3.5f, 1.5f, 2.0f, HEX(0x8a5a3a));
            im.ellipse(9.5f, 3.5f, 1.5f, 2.0f, HEX(0x8a5a3a)); im.ellipse(12.5f, 6.5f, 1.5f, 2.0f, HEX(0x8a5a3a));
            im.rect(6, 9, 3, 1, HEX(0xb07a52));
            break;
        case TI_JOURNAL:  // a closed book with a ribbon
            im.rect(3, 2, 10, 12, HEX(0x7a3a2a)); im.rect(3, 2, 2, 12, HEX(0x5a2a1e));
            im.rect(6, 4, 5, 1, HEX(0xe8c86a)); im.rect(6, 6, 5, 1, HEX(0xe8c86a));
            im.rect(10, 11, 2, 4, HEX(0xd8483c));
            break;
        case TI_ALL:  // a 2 x 2 grid of slots
            for (int y = 0; y < 2; y++)
                for (int x = 0; x < 2; x++) { im.rect(2 + x * 6, 2 + y * 6, 5, 5, HEX(0x8a8478)); im.rect(3 + x * 6, 3 + y * 6, 3, 3, HEX(0xd8d2c4)); }
            break;
        case TI_MATS:  // a log and a stone
            im.rect(2, 8, 9, 5, HEX(0x8a5a2c)); im.rect(2, 8, 9, 1, HEX(0xa8743c)); im.ellipse(10.5f, 10.5f, 2.2f, 2.5f, HEX(0xc8a060));
            im.ellipseShaded(11, 5.5f, 3.2f, 2.8f, HEX(0xc8ccd6), HEX(0x9a9eaa), HEX(0x6a6e7a));
            break;
        case TI_WEAPONS:  // a sword
            im.rect(7, 1, 2, 9, HEX(0xdfe4ee)); im.rect(7, 1, 1, 9, HEX(0xffffff));
            im.rect(4, 10, 8, 2, HEX(0xc89a3a)); im.rect(7, 12, 2, 3, HEX(0x7a4a26));
            break;
        case TI_TOOLS:  // a pickaxe
            im.rect(7, 4, 2, 11, HEX(0x8a5a2c));
            im.rect(2, 3, 12, 2, HEX(0xa8acb8)); im.rect(2, 3, 2, 4, HEX(0xa8acb8)); im.rect(12, 3, 2, 4, HEX(0xa8acb8));
            im.rect(5, 3, 6, 1, HEX(0xe0e4ee));
            break;
        case TI_GEAR:  // a tunic
            im.rect(4, 3, 8, 11, HEX(0x6a8ad0)); im.rect(1, 3, 4, 4, HEX(0x6a8ad0)); im.rect(11, 3, 4, 4, HEX(0x6a8ad0));
            im.rect(6, 3, 4, 2, HEX(0x2a3a6a)); im.rect(4, 10, 8, 1, HEX(0xc89a3a)); im.rect(4, 3, 1, 11, HEX(0x8aaae8));
            break;
        default:  // supplies: a red potion
            im.rect(6, 1, 4, 3, HEX(0xb89a5a)); im.rect(6, 4, 4, 2, HEX(0xdce6ee));
            im.ellipseShaded(8, 10, 5.0f, 4.6f, HEX(0xf07a6a), HEX(0xd8483c), HEX(0x8a2a22)); im.rect(5, 8, 1, 3, HEX(0xffd0c8));
            break;
    }
    im.outline(OUT);
    return im;
}
