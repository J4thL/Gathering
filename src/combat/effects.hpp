#pragma once
// effects.hpp - status effects that weapons put on animals: stun (frozen in place), fear (bolts from the hunter), poison (damage over time).
// Legendary animals shrug effects off: half the duration.
#include "core/data.hpp"

struct HitEffect {
    float stunChance = 0, stunT = 0;
    float fearChance = 0, fearT = 0;
    float poisonT = 0;  // always poisons when > 0
};
constexpr float POISON_TICK_FRAC = 0.035f;  // of max health per second
constexpr float EFFECT_CRIT_MUL = 2.f;      // a critical hit doubles stun and fear chances

inline HitEffect weaponEffect(int w) {
    HitEffect e;
    switch (w) {
        case W_AXE: e.stunChance = 0.10f; e.stunT = 1.0f; break;
        case W_HVYAXE: e.stunChance = 0.20f; e.stunT = 1.3f; break;
        case W_CLIFFAXE: e.stunChance = 0.18f; e.stunT = 1.5f; break;
        case W_SUNJAV: e.fearChance = 0.25f; e.fearT = 3.f; break;   // the blinding flash of the desert javelin
        case W_BOGSPEAR: e.poisonT = 6.f; break;                      // swamp venom
        default: break;
    }
    return e;
}
