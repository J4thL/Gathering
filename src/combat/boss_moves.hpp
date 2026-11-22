#pragma once
// boss_moves.hpp - the attack patterns of the wild-biome bosses and how each one is warned about (data only; the behaviour is in game_boss.cpp).
//
// Every attack runs the same sequence, so the hunter always has time to react:
//   1 PREP     the boss readies the attack (it stops, turns to the hunter and a "!" shows over its head)
//   2 WARN     the warning appears on the ground (the telegraph); the direction and target are locked when it starts
//   3 EXEC     the attack happens (this is the only time it can hurt)
//   4 RECOVER  the boss is open for a moment before it moves again
#include "core/data.hpp"

enum BossMove {
    BM_BASIC,       // normal melee attack
    BM_HEAVY,       // high-damage slow attack
    BM_CHARGE,      // rushes toward the hunter
    BM_SWEEP,       // attacks a wide area in front
    BM_SLAM,        // hits the ground: damage all round the boss
    BM_JUMP,        // leaps to where the hunter stood and lands with an area blast
    BM_PROJECTILE,  // launches ranged shots
    BM_MULTI,       // three consecutive attacks
    BM_SPIN,        // spins and damages everything near
    BM_GRAB,        // grabs the hunter and holds them
    BM_EXPLOSION,   // an expanding damage zone
    BM_SUMMON,      // calls more creatures
    BM_ROAR,        // stuns and weakens the hunter if near
    BM_ENRAGE,      // faster and harder from then on (once, at half health)
    BM_BREATH,      // a frontal cone of breath
    BM_METEOR,      // several circles hit the ground in turn
    BM_CATACLYSM,   // the whole area is hit except a safe zone the hunter must stand in
    BM_COUNT
};

// what the warning looks like on the ground
enum BossShape {
    BS_NONE,
    BS_CIRCLE,     // red circle (slam, spin, grab)
    BS_CONE,       // red cone (frontal attacks and breath)
    BS_LINE,       // red line (charge, shots)
    BS_CIRCLES,    // several circles (meteor shower)
    BS_EXPAND,     // an expanding circle (explosion, roar, summoning)
    BS_CROSSHAIR,  // a crosshair on a target spot (the leap)
    BS_SAFE        // a red area with a green safe zone to stand in
};

struct BossMoveDef {
    const char* name;
    int shape;
    float prep, warn, exec, rec;  // seconds of each step
    float dmg;                    // times the species' attack damage
    float size, size2;            // circle radius / cone reach / line length  |  cone half-angle (rad) / line half-width / safe-zone radius
    float minD, maxD;             // the boss only picks it with the hunter this far away
};

static const BossMoveDef BMOVE[BM_COUNT] = {
    /* BASIC     */ {"STRIKE", BS_CONE, 0.10f, 0.50f, 0.15f, 0.50f, 1.0f, 44.f, 0.55f, 0.f, 62.f},
    /* HEAVY     */ {"HEAVY ATTACK", BS_CONE, 0.25f, 0.85f, 0.20f, 1.05f, 2.0f, 54.f, 0.70f, 0.f, 72.f},
    /* CHARGE    */ {"CHARGE", BS_LINE, 0.20f, 0.80f, 0.45f, 0.95f, 1.6f, 150.f, 11.f, 62.f, 230.f},
    /* SWEEP     */ {"SWEEP", BS_CONE, 0.20f, 0.75f, 0.25f, 0.80f, 1.3f, 66.f, 1.45f, 0.f, 84.f},
    /* SLAM      */ {"SLAM", BS_CIRCLE, 0.20f, 0.90f, 0.15f, 0.95f, 1.5f, 54.f, 0.f, 0.f, 78.f},
    /* JUMP      */ {"LEAP", BS_CROSSHAIR, 0.25f, 0.95f, 0.50f, 0.90f, 1.5f, 40.f, 0.f, 70.f, 230.f},
    /* PROJECTILE*/ {"SHOT", BS_LINE, 0.20f, 0.65f, 0.15f, 0.85f, 0.9f, 210.f, 5.f, 80.f, 240.f},
    /* MULTI     */ {"COMBO", BS_CONE, 0.15f, 0.55f, 1.00f, 0.95f, 0.8f, 46.f, 0.60f, 0.f, 64.f},
    /* SPIN      */ {"SPIN", BS_CIRCLE, 0.20f, 0.65f, 1.30f, 1.05f, 0.5f, 52.f, 0.f, 0.f, 70.f},
    /* GRAB      */ {"GRAB", BS_CIRCLE, 0.15f, 0.65f, 0.20f, 1.10f, 1.5f, 28.f, 0.f, 0.f, 54.f},
    /* EXPLOSION */ {"EXPLOSION", BS_EXPAND, 0.25f, 1.20f, 0.80f, 1.00f, 1.7f, 96.f, 0.f, 0.f, 150.f},
    /* SUMMON    */ {"SUMMON", BS_EXPAND, 0.25f, 0.90f, 0.20f, 0.85f, 0.0f, 40.f, 0.f, 0.f, 999.f},
    /* ROAR      */ {"ROAR", BS_EXPAND, 0.25f, 0.85f, 0.25f, 0.85f, 0.3f, 112.f, 0.f, 0.f, 150.f},
    /* ENRAGE    */ {"ENRAGE", BS_EXPAND, 0.20f, 0.90f, 0.30f, 0.60f, 0.0f, 30.f, 0.f, 0.f, 999.f},
    /* BREATH    */ {"BREATH", BS_CONE, 0.20f, 1.00f, 0.90f, 1.00f, 0.4f, 150.f, 0.42f, 20.f, 150.f},
    /* METEOR    */ {"METEOR SHOWER", BS_CIRCLES, 0.20f, 1.15f, 1.00f, 0.95f, 1.4f, 23.f, 0.f, 40.f, 240.f},
    /* CATACLYSM */ {"STAND IN THE SAFE ZONE!", BS_SAFE, 0.25f, 1.90f, 0.25f, 1.10f, 2.2f, 158.f, 26.f, 0.f, 220.f},
};

// attack phases
enum BossPhase { BP_IDLE, BP_PREP, BP_WARN, BP_EXEC, BP_RECOVER };

constexpr int BOSS_METEORS = 6;  // circles of a meteor shower (7 when enraged)
constexpr float BOSS_ENRAGE_HP = 0.5f;        // below this share of its health a boss is enraged...
constexpr float BOSS_ENRAGE_DMG = 1.25f;      // ...hits this much harder
constexpr float BOSS_ENRAGE_SPEED = 0.8f;     // ...and winds up / rests this much faster (a warning never drops below BOSS_MIN_WARN)
constexpr float BOSS_MIN_WARN = 0.45f;
