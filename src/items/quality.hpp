// quality.hpp - the five quality tiers a crafted item can have, and what each one means.
//
// Every craft rolls one tier (Game::craft; a higher skill in the recipe's trade improves the odds). The tier is stored per pack slot
// (Slot::q), so stacks only merge at equal quality; it shows as a coloured border on the item's icon. Stat multipliers (damage,
// defence, gathering speed, healing, carrying) and the shop price scale with the tier.
#pragma once
#include <cstdint>

enum Quality { Q_NORMAL, Q_GOOD, Q_OUTSTANDING, Q_EXCELLENT, Q_MASTERPIECE, Q_COUNT };

struct QualityDef {
    const char* name;
    uint32_t rgb;  // the icon border: stone, iron, bronze, silver, gold
    float stat;    // multiplier on the item's strength
    float price;   // multiplier on the shop price
};
inline const QualityDef& qualityDef(int q) {
    static const QualityDef T[Q_COUNT] = {
        {"NORMAL", 0x8c8579, 1.00f, 1.0f},       // stone
        {"GOOD", 0xa9b4c2, 1.08f, 1.3f},         // iron
        {"OUTSTANDING", 0xcd7f32, 1.17f, 1.7f},  // bronze
        {"EXCELLENT", 0xe2e6ee, 1.28f, 2.3f},    // silver
        {"MASTERPIECE", 0xffd23c, 1.45f, 3.5f},  // gold
    };
    return T[q < 0 ? 0 : q >= Q_COUNT ? Q_COUNT - 1 : q];
}
// the colour as an opaque ARGB value (the text / border colour type used by the renderer)
inline uint32_t qualityCol(int q) { return 0xFF000000u | qualityDef(q).rgb; }
// what a piece sells for
inline int qualityPrice(int base, int q) { return (int)(base * qualityDef(q).price + 0.5f); }
inline int clampQuality(int q) { return q < 0 ? 0 : q >= Q_COUNT ? Q_COUNT - 1 : q; }

// The tier for a craft at a skill level (1..100) from a uniform roll r in [0,1): each tier's odds grow with the level.
// Level 1: 8% good, 3% outstanding, 1% excellent, 0.3% masterpiece. Level 50: 28% / 18% / 9% / 3%. Level 100: 48% / 33% / 17% / 6%.
inline int rollQuality(int skillLevel, float r) {
    const float lv = (float)(skillLevel < 1 ? 1 : skillLevel > 100 ? 100 : skillLevel);
    const float mp = 0.002f + 0.0006f * lv, ex = 0.010f + 0.0016f * lv, out = 0.030f + 0.0030f * lv, good = 0.080f + 0.0040f * lv;
    if (r < mp) return Q_MASTERPIECE;
    if (r < mp + ex) return Q_EXCELLENT;
    if (r < mp + ex + out) return Q_OUTSTANDING;
    if (r < mp + ex + out + good) return Q_GOOD;
    return Q_NORMAL;
}
