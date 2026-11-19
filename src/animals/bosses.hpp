#pragma once
// bosses.hpp - one named boss per wild biome map. It waits at its lair near the middle of the map; when the hunter comes close it appears
// (a legendary specimen with extra health) and fights with telegraphed attacks: a charge, a ground slam and a call for its pack.
// A slain boss pays out at once and returns after BOSS_RESPAWN seconds. Everything here is data; the behaviour is in game_boss.cpp.
#include "core/data.hpp"
#include "animals/fauna.hpp"

struct BossDef {
    int biome, kind;
    const char* name;
    float hpMul;     // on top of the legendary tier
    int coins;       // paid on the first kill; later kills pay a little more
    int packKind;    // who answers when it calls for help
};
static const BossDef BOSSES[] = {
    {B_DESERT, A_DUNELION, "SANDMANE, THE SUN KING", 2.6f, 220, A_COYOTE},
    {B_SWAMP, A_GATOR, "OLD SNAPJAW", 2.6f, 260, A_MOCCASIN},
    {B_SNOW, A_POLARBEAR, "WHITE DEATH", 2.4f, 300, A_WOLF},
    {B_MOUNTAIN, A_MTLION, "CRAGSHADE", 2.4f, 340, A_WOLF},
};
constexpr int BOSS_N = 4;

inline const BossDef* bossForBiome(int biome) {
    for (int i = 0; i < BOSS_N; i++)
        if (BOSSES[i].biome == biome) return &BOSSES[i];
    return nullptr;
}

constexpr float BOSS_RESPAWN = 600.f;       // seconds until a slain boss is back at its lair
constexpr float BOSS_WAKE_RANGE = 300.f;    // it appears when the hunter comes this close to the lair
constexpr float BOSS_SEEN_RANGE = 520.f;    // the lair is marked on the map once the hunter has come this close
constexpr float BOSS_ACT_RANGE = 230.f;     // it starts attack patterns inside this distance
constexpr float BOSS_CHARGE_TELE = 0.8f, BOSS_CHARGE_TIME = 0.45f, BOSS_CHARGE_SPEED = 3.2f;  // speed: times its run speed
constexpr float BOSS_SLAM_TELE = 0.9f, BOSS_SLAM_RADIUS = 54.f, BOSS_COOLDOWN = 3.4f;
constexpr float BOSS_CHARGE_DMG = 1.6f, BOSS_SLAM_DMG = 1.3f;  // times the species' bite damage
