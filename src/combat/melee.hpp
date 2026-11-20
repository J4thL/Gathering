#pragma once
// melee.hpp - close-range fighting with the selected weapon (right mouse button or the melee key): a quick swing, a three-hit combo and a
// charged heavy swing. Each weapon class has its own reach, arc, damage and speed. Nothing is used up by swinging.
#include "core/data.hpp"

struct MeleeDef {
    float reach;    // px from the hunter's hand
    float arcDeg;   // width of the swing
    float dmgMul;   // times the weapon's throwing damage
    float dur;      // seconds a swing lasts (and the pause before the next)
    float stamina;  // cost of one swing
};

inline MeleeDef meleeDef(int w) {
    if (w < 0) return {20.f, 80.f, 0.f, 0.32f, 4.f};  // bare fists: damage comes from MELEE_FIST_DMG
    switch (w) {
        case W_FLINT: case W_BONE: case W_FANGDAGGER: case W_SILVERDAGGER: return {26.f, 95.f, 0.95f, 0.26f, 4.f};      // knives: fast
        case W_AXE: case W_HVYAXE: case W_CLIFFAXE: return {31.f, 125.f, 1.5f, 0.52f, 8.f};                              // axes: slow and wide
        case W_FROSTBRAND: return {35.f, 115.f, 1.6f, 0.40f, 6.f};
        case W_JAVELIN: case W_SPEAR: case W_IRONSPEAR: case W_RSPEAR: case W_SUNJAV: case W_BOGSPEAR: case W_HARPOON: case W_STEELSPEAR:
            return {44.f, 38.f, 1.1f, 0.42f, 6.f};                                                                         // spears: long and narrow
        default: break;
    }
    if (isLauncher(w)) return {20.f, 70.f, 0.25f, 0.45f, 5.f};  // bashing with a bow is a last resort
    return {30.f, 100.f, 1.0f, 0.40f, 6.f};
}

constexpr float MELEE_FIST_DMG = 5.f;
constexpr float MELEE_HEAVY_HOLD = 0.45f;     // hold this long for a heavy swing
constexpr float MELEE_HEAVY_MUL = 2.2f, MELEE_HEAVY_REACH = 8.f, MELEE_HEAVY_ARC = 1.35f, MELEE_HEAVY_STAMINA = 18.f, MELEE_HEAVY_DUR = 0.7f;
constexpr float MELEE_COMBO_WINDOW = 0.65f;   // a swing within this long of the last one continues the combo
constexpr float MELEE_FINISHER_MUL = 1.5f;    // the third hit of the combo
constexpr int MELEE_MAX_TARGETS = 3;
