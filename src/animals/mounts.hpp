// mounts.hpp - which wild animals can be raised from a baby and ridden, and what a mount gives the hunter.
//
// Any killed animal of a listed species has a small chance to leave a baby (the item ITEM_BABY0 + kind, its rarity tier stored in the slot's q).
// At the homestead the baby grows up like the farm animals do; an adult can be saddled, and the hunter rides it (Game::toggleMount):
// faster travel and extra carrying weight, scaled by the species and its rarity tier. Mounts cannot enter buildings or caves.
#pragma once
#include <algorithm>
#include "animals/fauna.hpp"

struct MountDef {
    int kind;        // the wild species (A_*)
    float speed;     // travel speed multiplier of a common adult
    int carry;       // extra carrying weight in kg of a common adult
    float chance;    // chance that a kill leaves a baby (common tier)
    float growSec;   // seconds from baby to adult at the homestead
};
static const MountDef MOUNTS[] = {
    {A_BOAR, 1.30f, 40, 0.050f, 420},      {A_DEER, 1.50f, 20, 0.040f, 360},     {A_MARSHDEER, 1.35f, 30, 0.040f, 380},
    {A_REINDEER, 1.40f, 40, 0.040f, 420},  {A_MOOSE, 1.30f, 80, 0.030f, 600},    {A_GOAT, 1.25f, 30, 0.050f, 300},
    {A_BIGHORN, 1.35f, 40, 0.040f, 400},   {A_WOLF, 1.55f, 20, 0.030f, 420},     {A_COYOTE, 1.50f, 15, 0.040f, 360},
    {A_PANTHER, 1.65f, 20, 0.020f, 480},   {A_LYNX, 1.55f, 15, 0.030f, 420},    {A_DUNELION, 1.55f, 40, 0.020f, 540},
    {A_MTLION, 1.60f, 40, 0.020f, 540},    {A_POLARBEAR, 1.30f, 100, 0.015f, 700},
};
constexpr int MOUNT_N = (int)(sizeof(MOUNTS) / sizeof(MOUNTS[0]));

inline const MountDef* mountDef(int kind) {
    for (const MountDef& m : MOUNTS)
        if (m.kind == kind) return &m;
    return nullptr;
}
inline bool mountable(int kind) { return mountDef(kind) != nullptr; }

// rarity tier (0 common .. 3 legendary) scales the numbers: faster, stronger, and a much better chance of a baby from a rare animal
inline float mountSpeed(int kind, int tier) {
    const MountDef* m = mountDef(kind);
    return m ? m->speed + 0.05f * std::max(0, std::min(3, tier)) : 1.f;
}
inline int mountCarry(int kind, int tier) {
    const MountDef* m = mountDef(kind);
    return m ? (int)(m->carry * (1.f + 0.25f * std::max(0, std::min(3, tier)))) : 0;
}
inline float babyChance(int kind, int tier) {
    static const float mul[4] = {1.f, 1.6f, 3.f, 8.f};
    const MountDef* m = mountDef(kind);
    return m ? std::min(0.5f, m->chance * mul[std::max(0, std::min(3, tier))]) : 0.f;
}
