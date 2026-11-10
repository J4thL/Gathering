// monsters.hpp - the creatures of the Abandoned Mine and the Frozen Dungeon: their stats, drops and the data the AI works on.
//
// They are not Animals: they live only inside a delve (delve.cpp spawns them from world.dsp), follow the hunter around the walls with a
// flow field, and fight with timed attacks that show a warning first (a windup, a red circle on the floor). monsters.cpp holds the AI,
// the combat and the boss; the models are in monster_art.hpp.
#pragma once
#include "core/data.hpp"

enum MonFlag {
    MF_FLY = 1, MF_POISON = 2, MF_UNDEAD = 4, MF_ICE = 8, MF_RANGED = 16, MF_BLOCK = 32, MF_BOSS = 64, MF_LUNGE = 128, MF_SLAM = 256, MF_PACK = 512
};

struct MonDrop {
    int mat;
    float chance;
    int lo, hi;
};
struct MonsterDef {
    const char* name;
    int hp;
    float dmg, spd;       // damage of one hit, walking speed (px/s)
    float hitR, bodyH;    // hit radius, height of the body centre above the feet (for aiming and the damage numbers)
    float aggro, range;   // sight, melee reach (or shooting range for the ranged ones)
    float windup, recover, cd;  // seconds: telegraph before the hit, after it, and the pause between attacks
    int xp, coins;        // hunting XP and coins (before the depth scaling)
    int flags;
    MonDrop drops[4];
};

// hp and damage grow with the depth of the room the creature stands in: x(1 + 1.2 d) and x(1 + 0.8 d)
inline const MonsterDef& MONDEF(int k) {
    static const MonsterDef D[MK_COUNT] = {
        {"CAVE BAT", 22, 7, 78, 6, 12, 120, 11, 0.30f, 0.45f, 0.9f, 8, 2, MF_FLY, {{M_BATWING, 0.70f, 1, 2}, {M_BONE, 0.15f, 1, 1}, {}, {}}},
        {"CAVE SPIDER", 40, 9, 58, 7, 5, 110, 13, 0.40f, 0.5f, 1.1f, 12, 3, MF_POISON, {{M_SILK, 0.80f, 1, 3}, {M_SPIDERFANG, 0.35f, 1, 1}, {}, {}}},
        {"ROCK GOLEM", 220, 24, 28, 11, 12, 100, 22, 0.80f, 1.0f, 2.2f, 48, 12, MF_SLAM,
         {{M_GOLEMCORE, 0.40f, 1, 1}, {M_COAL, 0.55f, 1, 3}, {M_IRON, 0.30f, 1, 2}, {M_GEM, 0.05f, 1, 1}}},
        {"UNDERGROUND WOLF", 70, 14, 82, 8, 7, 150, 14, 0.38f, 0.55f, 1.2f, 26, 6, MF_LUNGE | MF_PACK, {{M_CAVEPELT, 0.70f, 1, 1}, {M_MEAT, 0.60f, 1, 2}, {M_BONE, 0.30f, 1, 1}, {}}},
        {"ICE SKELETON", 60, 14, 52, 7, 11, 130, 15, 0.50f, 0.6f, 1.1f, 22, 5, MF_UNDEAD | MF_ICE, {{M_ICEBONE, 0.70f, 1, 2}, {M_ICESHARD, 0.25f, 1, 1}, {M_BONE, 0.30f, 1, 2}, {}}},
        {"FROST WOLF", 85, 17, 88, 8, 7, 160, 14, 0.36f, 0.5f, 1.1f, 30, 7, MF_LUNGE | MF_PACK | MF_ICE, {{M_FROSTFANG, 0.60f, 1, 1}, {M_FUR, 0.60f, 1, 2}, {M_MEAT, 0.50f, 1, 2}, {}}},
        {"ICE GOLEM", 320, 30, 26, 12, 14, 100, 24, 0.90f, 1.0f, 2.4f, 70, 18, MF_SLAM | MF_ICE, {{M_ICESHARD, 0.85f, 2, 4}, {M_ETERNALICE, 0.18f, 1, 1}, {M_CRYSTAL, 0.20f, 1, 1}, {}}},
        {"FROZEN KNIGHT", 210, 26, 40, 8, 13, 120, 18, 0.60f, 0.8f, 1.6f, 64, 16, MF_UNDEAD | MF_ICE | MF_BLOCK, {{M_KNIGHTPLATE, 0.50f, 1, 1}, {M_FROSTRUNE, 0.12f, 1, 1}, {M_IRONBAR, 0.20f, 1, 1}, {M_ICEBONE, 0.5f, 1, 2}}},
        {"FROST MAGE", 95, 18, 34, 7, 13, 170, 130, 0.90f, 0.8f, 2.2f, 56, 14, MF_UNDEAD | MF_ICE | MF_RANGED, {{M_FROSTRUNE, 0.35f, 1, 1}, {M_ICESHARD, 0.55f, 1, 2}, {M_ICEBONE, 0.3f, 1, 1}, {}}},
        {"THE FROSTBOUND KING", 2400, 38, 40, 14, 20, 260, 30, 0.80f, 1.0f, 1.8f, 2500, 800, MF_UNDEAD | MF_ICE | MF_BOSS | MF_BLOCK, {{M_FROSTRUNE, 1.0f, 3, 5}, {M_ETERNALICE, 1.0f, 3, 5}, {M_KNIGHTPLATE, 1.0f, 2, 3}, {M_ICESHARD, 1.0f, 6, 10}}},
    };
    return D[k];
}

enum MonState { MN_IDLE, MN_CHASE, MN_ATK, MN_HURT, MN_DEAD, MN_CAST };

struct Monster {
    int kind = 0, id = 0;  // id: index of its spawn in world.dsp (so a cleared spawn is not made again)
    V2 pos, vel, kb, home;
    int hp = 1, maxhp = 1;
    float dmg = 1, spd = 1;
    float depth = 0;
    bool elite = false;
    int state = MN_IDLE;
    float stT = 0;        // time in the current state
    float animT = 0, flash = 0, atkCd = 0, hpShow = 0, deadT = 0;
    int face8 = 0;
    bool struck = false;  // this attack's hit has landed already
    int atkType = 0;      // which attack is playing (the boss has several)
    float poisonT = 0, slowT = 0, stunT = 0;
    int phase = 1;        // boss: 1..3
    float special = 0;    // boss: time to the next special attack
    V2 aim;               // direction locked at the start of an attack
    bool awake = false;
    int summons = 0;
    int packId = 0;
    float hurtT = 0;
    float bias = 1;
};

enum HazKind { HZ_ROCK, HZ_SLAM, HZ_BURST, HZ_NOVA, HZ_BLIZZ };
struct Hazard {
    int kind = HZ_BURST;
    V2 pos;
    float r = 12, delay = 1, t = 0, dmg = 10;
    bool fired = false;
    float after = 0.35f;  // seconds the impact lingers
    bool slow = false;    // frost: slows the hunter
    bool fromPlayerSide = false;
};
struct MProj {
    V2 pos, dir;
    float speed = 110, dmg = 10, life = 2.5f, t = 0;
    int kind = 0;  // 0 ice bolt
};
struct DTrap {
    int kind = 0;  // TrapKind
    V2 pos;
    float t = 0;   // position in the cycle
    float depth = 0;
};
struct DChest {
    V2 pos;
    int tier = 0, id = 0;
    bool locked = false;
};

constexpr float TRAP_CYCLE = 3.4f;
