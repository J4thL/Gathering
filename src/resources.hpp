// resources.hpp - everything that can be harvested from the land, as data.
//
// A resource is a tree, plant, rock or ore deposit. Each ResDef says which tool it needs (and how good), how many hits it takes, how long a
// swing lasts, what it gives, and how long it takes to grow back. Placed resources come from two places in the world:
//   * world.nodes - small things placed by worldgen (herbs, mushrooms, ore veins ...), kind = NodeKind
//   * world.props - trees and rocks that were already scenery (oaks, pines, boulders ...), kind = PropKind
// nodeRes() / propRes() map those kinds to a ResDef; the harvest state (hits left, regrow timer) lives in Game (gathering.cpp).
#pragma once
#include "world.hpp"

enum ResId {
    // trees (axe)
    RS_OAK, RS_PINE, RS_DEADTREE, RS_PALM, RS_SNOWPINE, RS_BARETREE, RS_LOG,
    // spiny / woody props
    RS_CACTUS,
    // rocks (pickaxe)
    RS_ROCK, RS_SANDROCK, RS_SNOWROCK, RS_BOULDER, RS_CRYSTAL,
    // ore veins (pickaxe)
    RS_COPPER, RS_IRON, RS_GOLD, RS_AMBER,
    // plants (by hand)
    RS_PEAR, RS_SAGE, RS_REED, RS_BOGWORT, RS_FROSTBLOOM, RS_ALPINE, RS_HERB, RS_MUSHROOM, RS_BRANCH, RS_FLAX, RS_DRYWOOD, RS_NIGHTSHADE,
    RS_COUNT
};
enum ResClass { RCL_PLANT, RCL_TREE, RCL_ROCK, RCL_ORE };  // decides the sound, the particles and how the depleted state looks

struct ResYield {
    int mat;        // Mat
    int lo, hi;     // amount rolled when the resource is finished
    float chance;   // chance this entry drops at all
    int minTier;    // needs at least this tool tier for the entry to be possible (bonus ore from rock, say)
};
struct ResDef {
    const char* name;
    int cls;
    int tool;          // ToolType, or -1 when no tool matters
    int minTier;       // lowest tool tier that can harvest it; 0 = bare hands work (slower when a tool type is listed)
    int hits;          // swings to finish it (its "durability")
    float swing;       // seconds per swing at tier-1 speed
    ResYield y[3];
    float respawn;     // seconds until it is back after being finished
    int hurt;          // HP lost when it is finished (thorns, poison)
    int safeArmor;     // armor level that protects from `hurt`
    u32 chip;          // particle colour
};

static const ResDef RES[RS_COUNT] = {
    // name           cls        tool     min hits swing  yields (mat, lo, hi, chance, minTier)                                        respawn hurt safe chip
    {"OAK TREE",      RCL_TREE,  TT_AXE,  1, 4, 0.90f, {{M_WOOD, 3, 4, 1.f, 0}, {M_BRANCH, 1, 2, 0.6f, 0}, {-1, 0, 0, 0, 0}},        360, 0, 0, 0x8a5a2c},
    {"PINE TREE",     RCL_TREE,  TT_AXE,  1, 4, 0.85f, {{M_WOOD, 2, 3, 1.f, 0}, {M_BRANCH, 1, 2, 0.6f, 0}, {-1, 0, 0, 0, 0}},        330, 0, 0, 0x6a4a2a},
    {"DEAD TREE",     RCL_TREE,  TT_AXE,  1, 2, 0.70f, {{M_WOOD, 1, 2, 1.f, 0}, {M_BRANCH, 1, 2, 0.8f, 0}, {-1, 0, 0, 0, 0}},        600, 0, 0, 0x5a4a3a},
    {"PALM TREE",     RCL_TREE,  TT_AXE,  1, 3, 0.80f, {{M_WOOD, 2, 3, 1.f, 0}, {M_FIBER, 1, 2, 0.7f, 0}, {-1, 0, 0, 0, 0}},         420, 0, 0, 0xa8884a},
    {"SNOW PINE",     RCL_TREE,  TT_AXE,  1, 5, 1.00f, {{M_WOOD, 3, 4, 1.f, 0}, {M_BRANCH, 1, 1, 0.5f, 0}, {-1, 0, 0, 0, 0}},        420, 0, 0, 0xc8d8e0},
    {"BARE TREE",     RCL_TREE,  TT_AXE,  1, 3, 0.80f, {{M_WOOD, 2, 3, 1.f, 0}, {M_BRANCH, 1, 2, 0.6f, 0}, {-1, 0, 0, 0, 0}},        400, 0, 0, 0x7a6a5a},
    {"FALLEN LOG",    RCL_TREE,  TT_AXE,  1, 3, 0.80f, {{M_WOOD, 2, 3, 1.f, 0}, {M_MUSHROOM, 1, 1, 0.3f, 0}, {-1, 0, 0, 0, 0}},       480, 0, 0, 0x6a4a2a},
    {"CACTUS",        RCL_TREE,  TT_AXE,  1, 2, 0.80f, {{M_PULP, 2, 3, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                   300, 2, 1, 0x72bb62},

    {"ROCK",          RCL_ROCK,  TT_PICK, 0, 3, 0.90f, {{M_STONE, 1, 2, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                  300, 0, 0, 0xa9acb8},
    {"SANDSTONE",     RCL_ROCK,  TT_PICK, 1, 4, 0.90f, {{M_STONE, 1, 2, 1.f, 0}, {M_COPPER, 1, 1, 0.35f, 1}, {-1, 0, 0, 0, 0}},        360, 0, 0, 0xd8b878},
    {"ICY ROCK",      RCL_ROCK,  TT_PICK, 1, 4, 0.95f, {{M_STONE, 1, 2, 1.f, 0}, {M_IRON, 1, 1, 0.20f, 2}, {-1, 0, 0, 0, 0}},          360, 0, 0, 0xcfe3f0},
    {"BOULDER",       RCL_ROCK,  TT_PICK, 1, 5, 1.00f, {{M_STONE, 2, 3, 1.f, 0}, {M_COPPER, 1, 2, 0.40f, 1}, {M_IRON, 1, 1, 0.25f, 2}}, 420, 0, 0, 0x9a9ca8},
    {"FROST CRYSTAL", RCL_ORE,   TT_PICK, 3, 6, 1.00f, {{M_CRYSTAL, 1, 2, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                900, 0, 0, 0xa8e4ff},

    {"COPPER ORE",    RCL_ORE,   TT_PICK, 1, 4, 0.90f, {{M_COPPER, 1, 3, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                 420, 0, 0, 0xd8844a},
    {"IRON ORE",      RCL_ORE,   TT_PICK, 2, 5, 1.00f, {{M_IRON, 1, 2, 1.f, 0}, {M_STONE, 1, 1, 0.4f, 0}, {-1, 0, 0, 0, 0}},           480, 0, 0, 0xf0953a},
    {"GOLD VEIN",     RCL_ORE,   TT_PICK, 3, 7, 1.10f, {{M_GOLD, 1, 2, 1.f, 0}, {M_STONE, 1, 1, 0.3f, 0}, {-1, 0, 0, 0, 0}},           720, 0, 0, 0xf2c94c},
    {"BOG AMBER",     RCL_ORE,   TT_PICK, 1, 3, 0.90f, {{M_AMBER, 1, 1, 1.f, 0}, {M_AMBER, 1, 1, 0.25f, 0}, {-1, 0, 0, 0, 0}},         900, 0, 0, 0xf0b040},

    {"PRICKLY PEAR",  RCL_PLANT, -1,      0, 1, 1.00f, {{M_PULP, 1, 2, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                   150, 2, 1, 0x72bb62},
    {"SAGEBRUSH",     RCL_PLANT, -1,      0, 1, 0.70f, {{M_SAGE, 1, 2, 1.f, 0}, {M_SAGE, 1, 1, 0.25f, 0}, {-1, 0, 0, 0, 0}},          120, 0, 0, 0xa8c2a0},
    {"MARSH REEDS",   RCL_PLANT, -1,      0, 1, 0.80f, {{M_FIBER, 2, 3, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                  100, 0, 0, 0x78a24c},
    {"BOGWORT",       RCL_PLANT, -1,      0, 1, 0.80f, {{M_BOGWORT, 1, 2, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                140, 0, 0, 0x8a4fc0},
    {"FROSTBLOOM",    RCL_PLANT, -1,      0, 1, 0.80f, {{M_FROSTBLOOM, 1, 2, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},              140, 0, 0, 0xa8e4ff},
    {"ALPINE HERB",   RCL_PLANT, -1,      0, 1, 0.80f, {{M_ALPINE, 1, 2, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                  140, 0, 0, 0xf2f0ff},
    {"WILD HERB",     RCL_PLANT, -1,      0, 1, 0.70f, {{M_HERB, 1, 2, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                    110, 0, 0, 0x6ab84a},
    {"MUSHROOMS",     RCL_PLANT, -1,      0, 1, 0.70f, {{M_MUSHROOM, 1, 2, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                 130, 0, 0, 0xe8dcc0},
    {"FALLEN BRANCH", RCL_PLANT, -1,      0, 2, 0.50f, {{M_BRANCH, 2, 3, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                  180, 0, 0, 0x8a6a3c},
    {"WILD FLAX",     RCL_PLANT, -1,      0, 1, 0.70f, {{M_FIBER, 1, 3, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},                   120, 0, 0, 0xb8c878},
    {"DRY WOOD",      RCL_PLANT, -1,      0, 2, 0.60f, {{M_WOOD, 1, 2, 1.f, 0}, {M_BRANCH, 1, 1, 0.5f, 0}, {-1, 0, 0, 0, 0}},           260, 0, 0, 0xc8a878},
    {"NIGHTSHADE",    RCL_PLANT, -1,      0, 1, 1.00f, {{M_NIGHTSHADE, 1, 2, 1.f, 0}, {-1, 0, 0, 0, 0}, {-1, 0, 0, 0, 0}},               200, 5, 2, 0x6a2a8a},
};

// resource of a world node kind (always one)
inline int nodeRes(int nodeKind) {
    static const int T[RN_COUNT] = {RS_PEAR, RS_SAGE, RS_REED, RS_BOGWORT, RS_FROSTBLOOM, RS_ALPINE, RS_IRON,
                                    RS_HERB, RS_MUSHROOM, RS_BRANCH, RS_FLAX, RS_DRYWOOD, RS_COPPER, RS_NIGHTSHADE, RS_AMBER, RS_GOLD};
    return T[nodeKind];
}
// resource of a prop kind, or -1 when the prop is scenery (huts, signs, ruins, the edge wall's rocks ...)
inline int propRes(int propKind) {
    switch (propKind) {
        case P_OAK0: case P_OAK1: return RS_OAK;
        case P_PINE: return RS_PINE;
        case P_DEADTREE0: case P_DEADTREE1: return RS_DEADTREE;
        case P_PALM: return RS_PALM;
        case P_SNOWPINE: return RS_SNOWPINE;
        case P_BARETREE: return RS_BARETREE;
        case P_LOG: return RS_LOG;
        case P_CACTUS0: case P_CACTUS1: return RS_CACTUS;
        case P_ROCK0: case P_ROCK1: return RS_ROCK;
        case P_SANDROCK0: case P_SANDROCK1: return RS_SANDROCK;
        case P_SNOWROCK: return RS_SNOWROCK;
        case P_BOULDER: return RS_BOULDER;
        case P_CRYSTAL: return RS_CRYSTAL;
        default: return -1;
    }
}

// Does a prop tile count as a protected boundary? The map's edge wall and the home forest's outer tree wall stay standing
// so that felling trees can never open the world's border or the biome gates.
inline bool protectedTile(int tx, int ty, int map = B_HOME) {
    if (tx < 3 || ty < 3 || tx >= MW - 3 || ty >= MH - 3) return true;  // every map has the same edge wall
    if (map == B_HOME && tx >= COX && tx < COX + CW && ty >= COY && ty < COY + CH) {  // the original area's own tree wall
        int cx = tx - COX, cy = ty - COY;
        if (cx < 3 || cy < 3 || cx >= CW - 3 || cy >= CH - 3) return true;
    }
    return false;
}

// tool speed with bare hands when the resource has a tool type but allows no tool (tier-0 rocks)
constexpr float HAND_SPEED = 0.5f;
constexpr float HARVEST_REACH_NODE = 20.f, HARVEST_REACH_PROP = 27.f;
constexpr float HARVEST_HEAL_AFTER = 25.f;  // a resource left half-chopped recovers after this many seconds untouched
constexpr float REGROW_FADE = 4.f;          // seconds of fade-in before a resource is solid / harvestable again
