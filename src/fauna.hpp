// fauna.hpp - every animal species as data: stats, behaviour flags, drops, art family, and the rarity tiers.
// Adding a species = one row in makeAnimals() (+ a spawn entry in spawns.hpp + a spec in sprites_fauna.hpp).
// Included from data.hpp, after the material enum.
#pragma once
#include <array>
#include <string>

enum Kind {
    A_RABBIT, A_DEER, A_BOAR, A_WOLF,  // original four (indices are used by quests and saves: do not reorder)
    A_HARE, A_FENNEC, A_COYOTE, A_RATTLER, A_ROADRUNNER, A_LIZARD, A_DUNELION,           // desert
    A_FROG, A_DUCK, A_MARSHDEER, A_MOCCASIN, A_GATOR, A_PANTHER,                         // swamp
    A_ARCTICFOX, A_SNOWHARE, A_REINDEER, A_MOOSE, A_LYNX, A_POLARBEAR,                   // snow
    A_GOAT, A_BIGHORN, A_EAGLE, A_MARMOT, A_MTLION,                                      // mountain
    A_COUNT
};

enum Art { ART_QUAD, ART_BIRD, ART_SNAKE, ART_FROG, ART_LIZARD, ART_GATOR };

// behaviour flags
enum {
    BH_AMBUSH = 1,      // lies still ("lurks") until you come close, then strikes and backs off
    BH_NIGHT_AGGRO = 2, // only hunts the player at night; by day it behaves like prey
    BH_FLIES = 4        // lifts off the ground while fleeing (harder to hit)
};

// How a rarity tier recolours a species. Which look each tier uses is data (AnimalDef::looks).
enum Look { LK_NONE, LK_DARK, LK_PALE, LK_RUSTY, LK_ALBINO, LK_MELANISTIC, LK_GOLD, LK_GHOST, LK_FROST, LK_EMBER, LK_COUNT };
static const char* LOOK_PREFIX[LK_COUNT] = {"", "DARK ", "PALE ", "RUSTY ", "ALBINO ", "MELANISTIC ", "GOLDEN ", "GHOST ", "FROSTED ", "EMBER "};

struct AnimalDef {
    const char* name;
    int hp;
    float wander, run, notice, hitR;
    bool aggro;
    float dmg, atkCd, gatherTime;
    int drop[M_COUNT][2];
    // ---- everything below has a default that reproduces the original four animals' behaviour
    int art = ART_QUAD;
    int forms = 1;              // 2 = a second form exists (antlered buck / white wolf ...), chosen at spawn
    unsigned flags = 0;
    float alertMul = 1.f;       // aggressive animals only turn on you inside notice * alertMul
    float fleeHp = 0.f;         // aggressive animals break off below this hp fraction (0 = never)
    float leash = 240.f;        // aggressive animals give up when you are this far away
    float restChance = 0.f;     // when idle: chance to lie down / graze instead of wandering (0 = never)
    float feedChance = 0.f;
    float bodyH = 6.f;          // height of the hit centre above the feet
    int decal = 1;              // blood stain size on death (0-2)
    int shadow = 1;             // shadow sprite (0 small, 1 medium, 2 large)
    bool hop = false;           // moves in hops (rabbit, hare, frog)
    float animK = 0.075f;       // walk-cycle speed per pixel moved
    float hpBarY = 8.f;
    int formDrop[3] = {-1, 0, 0};    // extra drop for form 1: {material, min, max}
    int legendDrop[3] = {-1, 0, 0};  // extra drop for a legendary specimen
    int looks[3] = {LK_DARK, LK_ALBINO, LK_GOLD};  // look of uncommon / rare / legendary specimens
    const char* legendName = nullptr;              // special name for the legendary one
};

inline std::array<AnimalDef, A_COUNT> makeAnimals() {
    std::array<AnimalDef, A_COUNT> t{};
    auto S = [&](int k, const AnimalDef& d) -> AnimalDef& {
        t[k] = d;
        return t[k];
    };
    // drop columns: MEAT, HIDE, BONE, ORGAN, FUR, TUSK, ANTLER  {min, max}
    // ---------------------------------------------------------------- the original four (values unchanged)
    {
        auto& d = S(A_RABBIT, {"RABBIT", 14, 22, 98, 85, 6, false, 0, 0, 0.40f, {{1, 2}, {1, 1}, {1, 1}, {0, 1}, {0, 0}, {0, 0}, {0, 0}}});
        d.bodyH = 4; d.decal = 0; d.shadow = 0; d.hop = true; d.animK = 0.09f;
        d.looks[0] = LK_PALE; d.looks[2] = LK_GHOST; d.legendName = "MOON RABBIT";
    }
    {
        auto& d = S(A_DEER, {"DEER", 46, 24, 90, 115, 9, false, 0, 0, 0.60f, {{3, 4}, {2, 2}, {2, 3}, {1, 2}, {0, 0}, {0, 0}, {0, 0}}});
        d.forms = 2; d.bodyH = 12; d.decal = 2; d.hpBarY = 14;
        d.formDrop[0] = M_ANTLER; d.formDrop[1] = 1; d.formDrop[2] = 2;
        d.looks[0] = LK_RUSTY; d.looks[2] = LK_GOLD; d.legendName = "GOLDEN STAG";
    }
    {
        auto& d = S(A_BOAR, {"BOAR", 78, 20, 108, 75, 10, true, 15, 1.1f, 0.65f, {{3, 5}, {2, 3}, {2, 2}, {1, 2}, {0, 0}, {1, 2}, {0, 0}}});
        d.alertMul = 0.62f; d.bodyH = 7; d.decal = 1;
        d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_EMBER; d.legendName = "IRONTUSK BOAR";
        d.legendDrop[0] = M_TUSK; d.legendDrop[1] = 2; d.legendDrop[2] = 3;
    }
    {
        auto& d = S(A_WOLF, {"WOLF", 58, 28, 104, 125, 9, true, 11, 0.85f, 0.55f, {{2, 3}, {0, 0}, {2, 2}, {1, 1}, {2, 3}, {0, 0}, {0, 0}}});
        d.forms = 2; d.fleeHp = 0.25f; d.leash = 300; d.bodyH = 9; d.decal = 1;  // form 1 = arctic (white) wolf
        d.looks[0] = LK_DARK; d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_GHOST; d.legendName = "SPIRIT WOLF";
        d.legendDrop[0] = M_FUR; d.legendDrop[1] = 3; d.legendDrop[2] = 4;
    }
    // ---------------------------------------------------------------- desert
    {
        auto& d = S(A_HARE, {"DESERT HARE", 12, 24, 118, 100, 6, false, 0, 0, 0.35f, {{1, 1}, {1, 1}, {0, 1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}});
        d.bodyH = 5; d.decal = 0; d.shadow = 0; d.hop = true; d.animK = 0.09f; d.feedChance = 0.35f; d.restChance = 0.1f;
        d.looks[0] = LK_PALE; d.looks[2] = LK_GHOST; d.legendName = "MIRAGE HARE";
    }
    {
        auto& d = S(A_FENNEC, {"FENNEC FOX", 16, 26, 112, 130, 6, false, 0, 0, 0.35f, {{1, 2}, {0, 1}, {0, 1}, {0, 0}, {1, 1}, {0, 0}, {0, 0}}});
        d.bodyH = 5; d.decal = 0; d.shadow = 0; d.restChance = 0.3f; d.feedChance = 0.2f;
        d.looks[0] = LK_RUSTY; d.looks[2] = LK_EMBER;
    }
    {
        auto& d = S(A_COYOTE, {"COYOTE", 40, 28, 100, 120, 8, true, 9, 0.9f, 0.50f, {{2, 3}, {1, 1}, {1, 2}, {0, 1}, {1, 2}, {0, 0}, {0, 0}}});
        d.flags = BH_NIGHT_AGGRO; d.alertMul = 0.8f; d.fleeHp = 0.3f; d.leash = 260; d.bodyH = 8; d.restChance = 0.3f;
        d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_EMBER; d.legendName = "ASH COYOTE";
    }
    {
        auto& d = S(A_RATTLER, {"RATTLESNAKE", 22, 14, 90, 70, 6, true, 12, 1.0f, 0.40f, {{1, 1}, {1, 2}, {0, 1}, {0, 1}, {0, 0}, {0, 0}, {0, 0}}});
        d.art = ART_SNAKE; d.flags = BH_AMBUSH; d.alertMul = 0.9f; d.leash = 130; d.bodyH = 3; d.decal = 0; d.shadow = 0;
        d.restChance = 0.8f; d.animK = 0.09f;
        d.looks[0] = LK_PALE; d.looks[1] = LK_ALBINO; d.looks[2] = LK_EMBER; d.legendName = "SANDFANG";
    }
    {
        auto& d = S(A_ROADRUNNER, {"ROADRUNNER", 16, 26, 135, 120, 7, false, 0, 0, 0.35f, {{1, 2}, {0, 0}, {0, 1}, {0, 1}, {0, 0}, {0, 0}, {0, 0}}});
        d.art = ART_BIRD; d.bodyH = 7; d.decal = 0; d.shadow = 0; d.feedChance = 0.4f; d.animK = 0.1f;
        d.looks[0] = LK_RUSTY; d.looks[2] = LK_GOLD;
    }
    {
        auto& d = S(A_LIZARD, {"DESERT LIZARD", 8, 20, 110, 80, 5, false, 0, 0, 0.30f, {{0, 1}, {0, 1}, {0, 1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}});
        d.art = ART_LIZARD; d.bodyH = 3; d.decal = 0; d.shadow = 0; d.restChance = 0.5f; d.animK = 0.1f;
        d.looks[0] = LK_RUSTY; d.looks[1] = LK_ALBINO; d.looks[2] = LK_GOLD;
    }
    {
        auto& d = S(A_DUNELION, {"DUNE LION", 120, 26, 110, 150, 12, true, 24, 1.1f, 0.70f, {{4, 6}, {3, 4}, {3, 4}, {2, 3}, {2, 3}, {0, 0}, {0, 0}}});
        d.alertMul = 0.85f; d.fleeHp = 0.15f; d.leash = 360; d.bodyH = 10; d.decal = 2; d.shadow = 2; d.restChance = 0.35f; d.hpBarY = 12;
        d.looks[1] = LK_ALBINO; d.looks[2] = LK_GOLD; d.legendName = "SUN LION";
        d.legendDrop[0] = M_FUR; d.legendDrop[1] = 3; d.legendDrop[2] = 5;
    }
    // ---------------------------------------------------------------- swamp
    {
        auto& d = S(A_FROG, {"MARSH FROG", 6, 14, 80, 55, 4, false, 0, 0, 0.25f, {{0, 1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}});
        d.art = ART_FROG; d.bodyH = 3; d.decal = 0; d.shadow = 0; d.hop = true; d.animK = 0.1f; d.restChance = 0.5f;
        d.looks[0] = LK_PALE; d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_GOLD; d.legendName = "GOLDEN TOAD";
    }
    {
        auto& d = S(A_DUCK, {"WILD DUCK", 12, 18, 125, 120, 6, false, 0, 0, 0.35f, {{1, 2}, {0, 0}, {0, 1}, {0, 1}, {0, 0}, {0, 0}, {0, 0}}});
        d.art = ART_BIRD; d.flags = BH_FLIES; d.bodyH = 6; d.decal = 0; d.shadow = 0; d.feedChance = 0.4f; d.restChance = 0.15f;
        d.looks[0] = LK_PALE; d.looks[1] = LK_ALBINO; d.looks[2] = LK_GOLD;
    }
    {
        auto& d = S(A_MARSHDEER, {"MARSH DEER", 52, 22, 92, 120, 9, false, 0, 0, 0.60f, {{3, 4}, {2, 2}, {2, 3}, {1, 2}, {0, 0}, {0, 0}, {0, 0}}});
        d.forms = 2; d.bodyH = 11; d.decal = 2; d.hpBarY = 13; d.restChance = 0.25f; d.feedChance = 0.4f;
        d.formDrop[0] = M_ANTLER; d.formDrop[1] = 1; d.formDrop[2] = 2;
        d.looks[0] = LK_DARK; d.looks[2] = LK_GHOST; d.legendName = "MIST STAG";
    }
    {
        auto& d = S(A_MOCCASIN, {"WATER MOCCASIN", 18, 13, 88, 65, 6, true, 10, 1.0f, 0.40f, {{1, 1}, {1, 1}, {0, 0}, {0, 1}, {0, 0}, {0, 0}, {0, 0}}});
        d.art = ART_SNAKE; d.flags = BH_AMBUSH; d.alertMul = 0.9f; d.leash = 120; d.bodyH = 3; d.decal = 0; d.shadow = 0;
        d.restChance = 0.8f; d.animK = 0.09f;
        d.looks[0] = LK_PALE; d.looks[1] = LK_ALBINO; d.looks[2] = LK_EMBER; d.legendName = "BOG VIPER";
    }
    {
        auto& d = S(A_GATOR, {"ALLIGATOR", 110, 12, 96, 85, 11, true, 20, 1.5f, 0.70f, {{3, 4}, {3, 4}, {2, 3}, {1, 2}, {0, 0}, {1, 2}, {0, 0}}});
        d.art = ART_GATOR; d.flags = BH_AMBUSH; d.alertMul = 0.8f; d.leash = 170; d.bodyH = 6; d.decal = 2; d.shadow = 2; d.restChance = 0.7f;
        d.hpBarY = 6; d.animK = 0.06f;
        d.looks[1] = LK_ALBINO; d.looks[2] = LK_GOLD; d.legendName = "ELDER GATOR";
        d.legendDrop[0] = M_TUSK; d.legendDrop[1] = 3; d.legendDrop[2] = 4;
    }
    {
        auto& d = S(A_PANTHER, {"BOG PANTHER", 85, 28, 112, 140, 10, true, 18, 0.8f, 0.60f, {{3, 4}, {2, 3}, {2, 2}, {1, 2}, {2, 3}, {0, 0}, {0, 0}}});
        d.alertMul = 0.8f; d.fleeHp = 0.2f; d.leash = 340; d.bodyH = 9; d.decal = 1; d.restChance = 0.3f;
        d.looks[0] = LK_DARK; d.looks[1] = LK_ALBINO; d.looks[2] = LK_GHOST; d.legendName = "SHADE PANTHER";
        d.legendDrop[0] = M_FUR; d.legendDrop[1] = 3; d.legendDrop[2] = 4;
    }
    // ---------------------------------------------------------------- snow
    {
        auto& d = S(A_ARCTICFOX, {"ARCTIC FOX", 18, 26, 118, 125, 6, false, 0, 0, 0.35f, {{1, 2}, {0, 1}, {0, 1}, {0, 0}, {2, 2}, {0, 0}, {0, 0}}});
        d.bodyH = 5; d.decal = 0; d.shadow = 0; d.restChance = 0.3f; d.feedChance = 0.2f;
        d.looks[0] = LK_DARK; d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_GHOST; d.legendName = "AURORA FOX";
    }
    {
        auto& d = S(A_SNOWHARE, {"SNOW HARE", 12, 24, 120, 105, 6, false, 0, 0, 0.35f, {{1, 1}, {1, 1}, {0, 1}, {0, 0}, {1, 1}, {0, 0}, {0, 0}}});
        d.bodyH = 5; d.decal = 0; d.shadow = 0; d.hop = true; d.animK = 0.09f; d.feedChance = 0.3f; d.restChance = 0.15f;
        d.looks[0] = LK_RUSTY; d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_GHOST;
    }
    {
        auto& d = S(A_REINDEER, {"REINDEER", 56, 24, 94, 120, 10, false, 0, 0, 0.65f, {{3, 5}, {2, 3}, {2, 3}, {1, 2}, {0, 0}, {0, 0}, {0, 0}}});
        d.forms = 2; d.bodyH = 12; d.decal = 2; d.hpBarY = 14; d.restChance = 0.25f; d.feedChance = 0.4f;
        d.formDrop[0] = M_ANTLER; d.formDrop[1] = 1; d.formDrop[2] = 3;
        d.looks[0] = LK_DARK; d.looks[1] = LK_ALBINO; d.looks[2] = LK_FROST; d.legendName = "FROST STAG";
    }
    {
        auto& d = S(A_MOOSE, {"MOOSE", 130, 18, 100, 100, 13, true, 22, 1.4f, 0.80f, {{6, 8}, {3, 4}, {3, 4}, {2, 3}, {1, 2}, {0, 0}, {0, 0}}});
        d.forms = 2; d.alertMul = 0.42f; d.leash = 240; d.bodyH = 15; d.decal = 2; d.shadow = 2; d.hpBarY = 18;
        d.restChance = 0.2f; d.feedChance = 0.45f;
        d.formDrop[0] = M_ANTLER; d.formDrop[1] = 2; d.formDrop[2] = 3;
        d.looks[0] = LK_DARK; d.looks[1] = LK_ALBINO; d.looks[2] = LK_FROST; d.legendName = "ELDER MOOSE";
        d.legendDrop[0] = M_ANTLER; d.legendDrop[1] = 3; d.legendDrop[2] = 4;
    }
    {
        auto& d = S(A_LYNX, {"LYNX", 44, 28, 108, 135, 8, true, 13, 0.75f, 0.50f, {{2, 3}, {1, 2}, {1, 2}, {0, 1}, {2, 3}, {0, 0}, {0, 0}}});
        d.alertMul = 0.75f; d.fleeHp = 0.3f; d.leash = 300; d.bodyH = 7; d.decal = 1; d.restChance = 0.35f;
        d.looks[0] = LK_PALE; d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_FROST;
    }
    {
        auto& d = S(A_POLARBEAR, {"POLAR BEAR", 180, 20, 98, 130, 14, true, 30, 1.5f, 0.90f, {{6, 8}, {4, 5}, {3, 4}, {2, 3}, {4, 5}, {0, 0}, {0, 0}}});
        d.alertMul = 0.8f; d.fleeHp = 0.1f; d.leash = 320; d.bodyH = 12; d.decal = 2; d.shadow = 2; d.restChance = 0.3f; d.hpBarY = 16;
        d.looks[0] = LK_RUSTY; d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_FROST; d.legendName = "GLACIER BEAR";
        d.legendDrop[0] = M_FUR; d.legendDrop[1] = 4; d.legendDrop[2] = 6;
    }
    // ---------------------------------------------------------------- mountain
    {
        auto& d = S(A_GOAT, {"MOUNTAIN GOAT", 40, 24, 100, 125, 8, false, 0, 0, 0.55f, {{2, 3}, {2, 2}, {2, 2}, {0, 1}, {0, 0}, {0, 0}, {0, 1}}});
        d.bodyH = 10; d.decal = 1; d.restChance = 0.2f; d.feedChance = 0.4f; d.hpBarY = 12;
        d.looks[0] = LK_DARK; d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_GOLD; d.legendName = "CLOUD GOAT";
    }
    {
        auto& d = S(A_BIGHORN, {"BIGHORN SHEEP", 62, 22, 95, 115, 10, true, 14, 1.3f, 0.65f, {{3, 4}, {2, 3}, {2, 3}, {1, 1}, {0, 0}, {0, 0}, {1, 2}}});
        d.alertMul = 0.35f; d.leash = 180; d.bodyH = 11; d.decal = 2; d.restChance = 0.2f; d.feedChance = 0.4f; d.hpBarY = 13;
        d.looks[0] = LK_DARK; d.looks[1] = LK_ALBINO; d.looks[2] = LK_GOLD; d.legendName = "GOLDEN RAM";
        d.legendDrop[0] = M_ANTLER; d.legendDrop[1] = 2; d.legendDrop[2] = 3;
    }
    {
        auto& d = S(A_EAGLE, {"GOLDEN EAGLE", 22, 18, 135, 150, 8, false, 0, 0, 0.40f, {{1, 1}, {0, 0}, {1, 2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}});
        d.art = ART_BIRD; d.flags = BH_FLIES; d.bodyH = 8; d.decal = 0; d.shadow = 1; d.restChance = 0.35f; d.animK = 0.1f; d.hpBarY = 12;
        d.looks[0] = LK_DARK; d.looks[1] = LK_ALBINO; d.looks[2] = LK_GOLD; d.legendName = "SKY KING";
    }
    {
        auto& d = S(A_MARMOT, {"MARMOT", 16, 16, 90, 95, 6, false, 0, 0, 0.35f, {{1, 2}, {1, 1}, {0, 1}, {0, 0}, {0, 1}, {0, 0}, {0, 0}}});
        d.bodyH = 5; d.decal = 0; d.shadow = 0; d.feedChance = 0.45f; d.restChance = 0.25f;
        d.looks[0] = LK_PALE; d.looks[1] = LK_ALBINO; d.looks[2] = LK_GOLD;
    }
    {
        auto& d = S(A_MTLION, {"MOUNTAIN LION", 105, 26, 110, 150, 11, true, 22, 1.0f, 0.70f, {{4, 5}, {3, 4}, {3, 3}, {2, 2}, {3, 4}, {0, 0}, {0, 0}}});
        d.alertMul = 0.85f; d.fleeHp = 0.15f; d.leash = 340; d.bodyH = 10; d.decal = 2; d.shadow = 2; d.restChance = 0.35f; d.hpBarY = 12;
        d.looks[0] = LK_DARK; d.looks[1] = LK_MELANISTIC; d.looks[2] = LK_FROST; d.legendName = "STORM CAT";
        d.legendDrop[0] = M_FUR; d.legendDrop[1] = 3; d.legendDrop[2] = 5;
    }
    // ---- new materials (feathers, scales, horns, claws) - set after the table so the original rows stay untouched
    auto drop = [&](int k, int m, int lo, int hi) { t[k].drop[m][0] = lo; t[k].drop[m][1] = hi; };
    for (int k : {A_DUCK, A_ROADRUNNER}) drop(k, M_FEATHER, 1, 3);
    drop(A_EAGLE, M_FEATHER, 2, 4);
    drop(A_EAGLE, M_CLAW, 0, 1);
    for (int k : {A_RATTLER, A_LIZARD, A_MOCCASIN}) drop(k, M_SCALE, 1, 2);
    drop(A_GATOR, M_SCALE, 2, 4);
    for (int k : {A_GOAT, A_BIGHORN}) drop(k, M_HORN, 1, 2);
    for (int k : {A_LYNX, A_COYOTE, A_FENNEC}) drop(k, M_CLAW, 0, 1);
    for (int k : {A_DUNELION, A_PANTHER, A_MTLION, A_POLARBEAR}) drop(k, M_CLAW, 1, 2);
    // ---- trophies: every species carries exactly one material no other species drops (see M_TROPHY0 in data.hpp).
    // Small, harmless animals give theirs half the time; fighters and ambushers always; apex animals give one or two.
    struct Trophy { int kind, mat, lo, hi; };
    static const Trophy TROPHY[] = {
        {A_RABBIT, M_RABBITFOOT, 0, 1}, {A_DEER, M_DEERSINEW, 1, 1}, {A_BOAR, M_BOARBRISTLE, 1, 1}, {A_WOLF, M_WOLFFANG, 1, 1},
        {A_HARE, M_HARETAIL, 0, 1}, {A_FENNEC, M_FENNECEAR, 0, 1}, {A_COYOTE, M_COYOTEFANG, 1, 1}, {A_RATTLER, M_RATTLE, 1, 1},
        {A_ROADRUNNER, M_ROADPLUME, 0, 1}, {A_LIZARD, M_LIZARDTAIL, 0, 1}, {A_DUNELION, M_LIONMANE, 1, 2},
        {A_FROG, M_FROGGLAND, 0, 1}, {A_DUCK, M_DUCKDOWN, 0, 1}, {A_MARSHDEER, M_BOGHOOF, 1, 1}, {A_MOCCASIN, M_VENOMSAC, 1, 1},
        {A_GATOR, M_GATORTOOTH, 1, 2}, {A_PANTHER, M_PANTHEREYE, 1, 1},
        {A_ARCTICFOX, M_FOXBRUSH, 0, 1}, {A_SNOWHARE, M_SNOWFOOT, 0, 1}, {A_REINDEER, M_VELVET, 1, 1}, {A_MOOSE, M_MARROW, 1, 2},
        {A_LYNX, M_LYNXTUFT, 1, 1}, {A_POLARBEAR, M_BEARGALL, 1, 2},
        {A_GOAT, M_GOATBEARD, 1, 1}, {A_BIGHORN, M_RAMFLEECE, 1, 1}, {A_EAGLE, M_GOLDPLUME, 1, 1}, {A_MARMOT, M_MARMOTPELT, 0, 1},
        {A_MTLION, M_COUGARFANG, 1, 2},
    };
    for (const Trophy& tr : TROPHY) drop(tr.kind, tr.mat, tr.lo, tr.hi);
    // ---- animal fat: from anything with some body to it (fuel and grease, see recipes.hpp)
    for (int k = 0; k < A_COUNT; k++) {
        if (t[k].hp >= 100) drop(k, M_FAT, 2, 3);
        else if (t[k].hp >= 40) drop(k, M_FAT, 1, 2);
    }
    return t;
}
static const std::array<AnimalDef, A_COUNT> ANIMAL = makeAnimals();
// the trophy block of the material enum lists the species in Kind order: M_TROPHY0 + kind is that species' trophy
static_assert(M_TROPHY_N == A_COUNT, "one trophy material per species");
static_assert(M_RABBITFOOT == M_TROPHY0 + A_RABBIT && M_WOLFFANG == M_TROPHY0 + A_WOLF && M_LIONMANE == M_TROPHY0 + A_DUNELION &&
              M_GATORTOOTH == M_TROPHY0 + A_GATOR && M_BEARGALL == M_TROPHY0 + A_POLARBEAR && M_COUGARFANG == M_TROPHY0 + A_MTLION,
              "trophy materials must stay in species order");

// ---------------------------------------------------------------- rarity tiers
// A specimen's tier is rolled when it spawns. Multipliers are deliberately modest: a rare animal is a better
// trophy and a warier target, not a different game. The legendary tier also has hard spawn limits (see spawns.hpp).
enum Tier { TR_COMMON, TR_UNCOMMON, TR_RARE, TR_LEGENDARY, TR_COUNT };
struct TierDef {
    const char* name;
    float chance;    // chance per spawn (legendary additionally needs a free legendary slot, see LEGEND_*)
    float scale;     // body size
    float hp, speed, notice, dmg;
    float drops;     // multiplier on every material drop
    int bounty;      // coins paid on the kill
    u32 col;         // name / marker colour
};
static const TierDef TIER[TR_COUNT] = {
    {"COMMON", 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.0f, 0, HEX(0xf3e9d2)},
    {"UNCOMMON", 0.15f, 1.00f, 1.10f, 1.00f, 1.00f, 1.00f, 1.25f, 5, HEX(0x86d85e)},
    {"RARE", 0.04f, 1.10f, 1.30f, 1.06f, 1.20f, 1.12f, 1.6f, 30, HEX(0x7ab8f0)},
    {"LEGENDARY", 0.003f, 1.25f, 2.00f, 1.12f, 1.40f, 1.30f, 3.0f, 200, HEX(0xf2c94c)},
};
constexpr int LEGEND_MAX_ALIVE = 1;       // never more than one legendary at a time...
constexpr float LEGEND_COOLDOWN = 300.f;  // ...and at least this many seconds between legendary spawns

inline std::string animalName(int kind, int tier) {
    const AnimalDef& D = ANIMAL[kind];
    if (tier <= 0) return D.name;
    if (tier == TR_LEGENDARY && D.legendName) return D.legendName;
    return std::string(LOOK_PREFIX[D.looks[tier - 1]]) + D.name;
}
