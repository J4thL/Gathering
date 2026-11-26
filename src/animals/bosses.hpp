#pragma once
// bosses.hpp - one named boss per wild biome map. It waits at its lair near the middle of the map; when the hunter comes close it appears
// (a legendary specimen with extra health) and fights with telegraphed attacks: a set of attack patterns (combat/boss_moves.hpp), each warned about on the ground, and a call for its pack.
// A slain boss pays out at once and returns after BOSS_RESPAWN seconds. Everything here is data; the behaviour is in game_boss.cpp.
#include "core/data.hpp"
#include "animals/fauna.hpp"
#include "combat/boss_moves.hpp"

struct BossDef {
    int biome, kind;
    const char* name;
    float hpMul;     // on top of the legendary tier
    int coins;       // paid on the first kill; later kills pay a little more
    int packKind;    // who answers when it calls for help
    int moves[12];   // its attack patterns (BossMove), ended by BM_COUNT; the summon and the enrage come by themselves
};
static const BossDef BOSSES[] = {
    {B_DESERT, A_DUNELION, "SANDMANE, THE SUN KING", 2.6f, 220, A_COYOTE,
     {BM_BASIC, BM_HEAVY, BM_CHARGE, BM_SWEEP, BM_JUMP, BM_MULTI, BM_ROAR, BM_METEOR, BM_COUNT}},
    {B_SWAMP, A_GATOR, "OLD SNAPJAW", 2.6f, 260, A_MOCCASIN,
     {BM_BASIC, BM_HEAVY, BM_SLAM, BM_SPIN, BM_GRAB, BM_PROJECTILE, BM_EXPLOSION, BM_MULTI, BM_COUNT}},
    {B_SNOW, A_POLARBEAR, "WHITE DEATH", 2.4f, 300, A_WOLF,
     {BM_BASIC, BM_HEAVY, BM_SWEEP, BM_SLAM, BM_CHARGE, BM_ROAR, BM_BREATH, BM_CATACLYSM, BM_COUNT}},
    {B_MOUNTAIN, A_MTLION, "CRAGSHADE", 2.4f, 340, A_WOLF,
     {BM_BASIC, BM_MULTI, BM_JUMP, BM_CHARGE, BM_PROJECTILE, BM_METEOR, BM_GRAB, BM_EXPLOSION, BM_SPIN, BM_COUNT}},
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
constexpr float BOSS_COOLDOWN = 2.2f;     // it walks the hunter down for this long between attacks (shorter when enraged)
constexpr float BOSS_SHOT_SPEED = 300.f;
