// skills.hpp - the player skill system as data: one table of skills, one XP curve, one level cap.
//
// A skill is only listed here when the game has an action that can train it (see the XP hooks in skills.cpp's callers).
// Progress is stored as TOTAL XP per skill; the level is derived from it, so the curve can be retuned without breaking saves.
// Tuning lives in the constants below: MAX_LEVEL, XP_BASE, XP_EXP, MILESTONES.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

enum SkillId {
    SK_WOODCUTTING, SK_MINING, SK_FORAGING,          // gathering
    SK_HUNTING, SK_SKINNING, SK_BUTCHERING,          // hunting and survival
    SK_CRAFTING, SK_SMITHING, SK_LEATHER, SK_COOKING, SK_ALCHEMY,  // processing
    SK_RANGED, SK_DEFENSE,                           // combat
    SK_SURVIVAL, SK_BUILDING,                        // other
    SK_COUNT
};

constexpr int MAX_LEVEL = 100;
// XP needed to go from level L to L+1 = XP_BASE * L^XP_EXP (rounded). With 5 / 1.7: lv1->2 = 5, lv10 needs ~800 XP in total, lv25 ~11k,
// lv50 ~72k, lv100 ~460k. Typical actions are worth 3-40 XP (see the awards in skills.cpp / the hooks), so level 10 is an hour of play, not a day.
constexpr double XP_BASE = 5.0, XP_EXP = 1.7;
// levels at which a skill grants a milestone unlock (what each one gives is defined where the bonus is used)
constexpr int MILESTONES[4] = {10, 25, 50, 75};

struct SkillDef {
    const char* key;    // stable save key: never rename, never reorder-by-meaning (saves use the key, not the index)
    const char* name;
    const char* group;
    const char* effect;  // what the skill's bonus does (shown in the skills tab)
    float cap;           // the bonus at level 100 (a fraction): 60% of it grows evenly with level, 10% arrives at each milestone
};
static const SkillDef SKILL[SK_COUNT] = {
    {"woodcutting", "WOODCUTTING", "GATHERING", "CHOPPING SPEED", 0.30f}, {"mining", "MINING", "GATHERING", "MINING SPEED", 0.30f},
    {"foraging", "FORAGING", "GATHERING", "CHANCE OF EXTRA PLANTS", 0.30f},
    {"hunting", "HUNTING", "SURVIVAL", "WEAPON DAMAGE", 0.20f}, {"skinning", "SKINNING", "SURVIVAL", "CHANCE OF EXTRA PELTS", 0.30f},
    {"butchering", "BUTCHERING", "SURVIVAL", "CHANCE OF EXTRA MEAT", 0.30f},
    {"crafting", "CRAFTING", "CRAFTING", "CHANCE OF SAVING A CRAFT", 0.25f}, {"smithing", "SMITHING", "CRAFTING", "CHANCE OF SAVING A CRAFT", 0.25f},
    {"leather", "LEATHERWORK", "CRAFTING", "CHANCE OF SAVING A CRAFT", 0.25f}, {"cooking", "COOKING", "CRAFTING", "CHANCE OF SAVING A CRAFT", 0.25f},
    {"alchemy", "ALCHEMY", "CRAFTING", "CHANCE OF SAVING A CRAFT", 0.25f},
    {"ranged", "RANGED", "COMBAT", "CHANCE TO KEEP AMMO", 0.30f}, {"defense", "DEFENSE", "COMBAT", "DAMAGE TAKEN LESS", 0.15f},
    {"survival", "SURVIVAL", "SURVIVAL", "STRONGER HEALING", 0.30f}, {"building", "BUILDING", "CRAFTING", "CHANCE OF SAVING A CRAFT", 0.25f},
};


// XP to climb from `level` to level + 1 (0 at the cap)
inline int64_t xpForLevel(int level) {
    if (level < 1) level = 1;
    if (level >= MAX_LEVEL) return 0;
    return (int64_t)std::llround(XP_BASE * std::pow((double)level, XP_EXP));
}

// All progress of one skill, as total XP. Level and "XP into the level" are derived.
struct SkillState {
    int64_t xp = 0;
    int level() const {
        int64_t left = xp;
        int l = 1;
        while (l < MAX_LEVEL && left >= xpForLevel(l)) left -= xpForLevel(l++);
        return l;
    }
    int64_t into() const {  // XP earned inside the current level
        int64_t left = xp;
        for (int l = 1; l < MAX_LEVEL && left >= xpForLevel(l); l++) left -= xpForLevel(l);
        return left;
    }
    static int64_t capXp() {
        int64_t t = 0;
        for (int l = 1; l < MAX_LEVEL; l++) t += xpForLevel(l);
        return t;
    }
};

struct SkillSet {
    SkillState s[SK_COUNT];
    int level(int k) const { return s[k].level(); }
    // Adds XP (clamped so a skill never passes the cap) and returns how many levels were gained.
    int add(int k, int64_t amount) {
        if (k < 0 || k >= SK_COUNT || amount <= 0) return 0;
        const int before = s[k].level();
        s[k].xp = std::min(s[k].xp + amount, SkillState::capXp());
        return s[k].level() - before;
    }
};

// next milestone level above `level`, or 0 when all are reached
inline int nextMilestone(int level) {
    for (int m : MILESTONES)
        if (level < m) return m;
    return 0;
}

// Anti-farming: XP from a source that is far below your level shrinks (never below 20%), so grinding rabbits at level 60 is pointless.
// `recommended` is the level at which the source stops being a challenge; below it the full amount is paid.
inline double xpFalloff(int level, int recommended) {
    if (level <= recommended) return 1.0;
    return std::max(0.2, 1.0 - (level - recommended) / 25.0);
}

// The bonus a skill gives at `level`, as a fraction (0.12 = +12%). Level 1 gives nothing; it grows evenly to 60% of the cap at level 100,
// and each milestone (10 / 25 / 50 / 75) adds a further 10% of the cap, so the full cap is reached only at level 100.
inline float skillBonusAt(int skill, int level) {
    if (skill < 0 || skill >= SK_COUNT) return 0.f;
    int ms = 0;
    for (int m : MILESTONES)
        if (level >= m) ms++;
    const float f = 0.6f * (float)(std::max(1, std::min(level, MAX_LEVEL)) - 1) / (float)(MAX_LEVEL - 1) + 0.1f * (float)ms;
    return SKILL[skill].cap * std::min(1.f, f);
}
