// bush_react.hpp - how each bush answers to being walked through, and how well it hides someone. One line per bush look; everything that reacts to a
// bush (the rustle and the leaves, the snow a snow shrub drops, the small animals it flushes, the cover it gives against an animal's eyes, how long
// it stays bent) reads this table, so a new bush is one more line here.
//
//   cover   0..1  how much of an animal's notice range it takes away from a hunter standing in it (the animal can still hear a mover)
//   rest    >= 1  how much longer than a tuft of grass it stays bent after someone has pushed through it (a trail)
//   sheds         a snow shrub: pushed through hard it drops its snow and shows its bare twigs until snow banks up again
//   leafA/leafB   the colours of what flies off it (leaves, snow, needles, dust)
//   burst         how many pieces fly off when it is brushed
#pragma once
#include "world/world.hpp"

struct BushDef {
    bool bush = false;
    bool sheds = false;
    float cover = 0.f, rest = 1.f;
    u32 leafA = 0, leafB = 0;
    int burst = 0;
};

// decorKind: a DecorKind; theme: the biome whose look the bush has (B_HOME ... B_MOUNTAIN, or -1 for the homestead's, which are the forest's round ones)
inline BushDef bushDefFor(int decorKind, int theme) {
    BushDef d;
    switch (decorKind) {
        case D_BUSH0:
        case D_BUSH1: {
            const int v = decorKind - D_BUSH0;
            d.bush = true;
            if (theme == B_SWAMP) {
                d = v == 0 ? BushDef{true, false, 0.45f, 1.6f, HEX(0x8a9a52), HEX(0x5f6e3a), 4}   // the moss mound
                           : BushDef{true, false, 0.35f, 1.4f, HEX(0x4a7a52), HEX(0x2f5a3c), 4};  // the dark fern
            } else if (theme == B_SNOW) {
                d = BushDef{true, true, v == 0 ? 0.30f : 0.22f, 1.8f, HEX(0xf4f9ff), HEX(0xcfdcef), 5};  // bare twigs with snow banked round them
            } else if (theme == B_MOUNTAIN) {
                d = v == 0 ? BushDef{true, false, 0.45f, 1.6f, HEX(0x6fae96), HEX(0x4a8470), 4}  // the alpine cushion
                           : BushDef{true, false, 0.22f, 1.4f, HEX(0x9a8a74), HEX(0x6e6252), 3}; // the scree twigs
            } else {
                d = BushDef{true, false, 0.55f, 1.7f, HEX(0x8fd05a), HEX(0x5aa63f), 5};  // the round green ones (forest, homestead)
            }
            break;
        }
        case D_DRYBUSH:
            d = BushDef{true, false, 0.20f, 1.4f, HEX(0xb89a62), HEX(0x8a6e3e), 3};  // the desert's dead bush: a few dry twigs and dust
            break;
        default: break;
    }
    return d;
}

