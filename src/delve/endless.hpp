#pragma once
// endless.hpp - the rules of the Endless Depths: an infinite dungeon of numbered floors, entered from the stair in the desert's Sunken Ruins.
// Every floor is a fresh maze generated from its number. Every ENDLESS_BOSS_EVERY-th floor is a boss floor (the throne hall, with a boss that
// grows with the floor). Difficulty steps up once per ten floors (a "tier"): the monsters change, get tougher, and more of them are elite.
// The pure rules live here so they can be tested; the generator is in delve.hpp, the game flow in delve.cpp.
#include "combat/monsters.hpp"

constexpr int ENDLESS_BOSS_EVERY = 10;
constexpr int ENDLESS_ARMOR_NEED = 3;  // armour tier needed to go in

inline int endlessTier(int floor) { return std::max(0, (floor - 1) / ENDLESS_BOSS_EVERY); }       // 0 for floors 1-10, 1 for 11-20 ...
inline bool endlessBossFloor(int floor) { return floor > 0 && floor % ENDLESS_BOSS_EVERY == 0; }
// monster strength (the same `depth` the mine and the Frozen Dungeon use, 0..1 there); it jumps with every tier and creeps up inside one
inline float endlessDepth(int floor) { return 0.15f + 0.45f * endlessTier(floor) + 0.025f * ((floor - 1) % ENDLESS_BOSS_EVERY); }
inline float endlessEliteChance(int tier) { return std::min(0.4f, 0.05f + 0.04f * tier); }
// the boss (the Frostbound King's moves) grows with how many bosses lie behind: floor 10 is x1.05 health, floor 100 is x5.55
inline float endlessBossHp(int floor) { return 0.55f + 0.5f * (floor / ENDLESS_BOSS_EVERY); }
inline float endlessBossDmg(int floor) { return 0.8f + 0.25f * (floor / ENDLESS_BOSS_EVERY); }
// where a new descent starts: one floor below the last boss that was beaten
inline int endlessStartFloor(int checkpoint) { return std::max(0, checkpoint) + 1; }

// what lives on a floor: bats and spiders at first, then wolves and golems, then the ice monsters, finally knights and mages
inline int endlessMonster(int tier, int floor, float roll) {
    struct E { int kind; float w; };
    static const E T0[] = {{MK_BAT, .45f}, {MK_SPIDER, .35f}, {MK_CAVEWOLF, .20f}};
    static const E T1[] = {{MK_BAT, .2f}, {MK_SPIDER, .2f}, {MK_CAVEWOLF, .25f}, {MK_GOLEM, .15f}, {MK_ICESKEL, .2f}};
    static const E T2[] = {{MK_SPIDER, .1f}, {MK_CAVEWOLF, .2f}, {MK_GOLEM, .15f}, {MK_ICESKEL, .2f}, {MK_FROSTWOLF, .2f}, {MK_MAGE, .15f}};
    static const E T3[] = {{MK_GOLEM, .15f}, {MK_ICESKEL, .15f}, {MK_FROSTWOLF, .15f}, {MK_MAGE, .2f}, {MK_ICEGOLEM, .15f}, {MK_KNIGHT, .2f}};
    static const E T4[] = {{MK_KNIGHT, .25f}, {MK_MAGE, .25f}, {MK_ICEGOLEM, .2f}, {MK_FROSTWOLF, .15f}, {MK_ICESKEL, .15f}};
    const E* t = tier <= 0 ? T0 : tier == 1 ? T1 : tier == 2 ? T2 : tier == 3 ? T3 : T4;
    const int n = tier <= 0 ? 3 : tier == 1 ? 5 : tier == 2 ? 6 : tier == 3 ? 6 : 5;
    if (tier == 0 && floor >= 6 && roll > 0.92f) return MK_GOLEM;  // the first golems turn up late in the first tier
    for (int i = 0; i < n; i++) {
        if (roll < t[i].w) return t[i].kind;
        roll -= t[i].w;
    }
    return t[n - 1].kind;
}
