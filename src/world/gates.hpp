// gates.hpp - what the hunter must bring to a gate before it opens onto the next biome.
// The chain is linear: desert -> swamp -> snow -> mountain. Each gate asks for a hunting level and an offering of materials (taken from the
// pack when offered). Tune the table to rebalance; nothing else needs to change.
#pragma once
#include "core/data.hpp"
#include "player/skills.hpp"

struct GateReq {
    int map;          // the biome this gate opens onto
    int prereq;       // the biome whose gate must be open first (-1 = none)
    int huntLevel;    // hunting level needed
    int nMat;
    struct { int mat, n; } mats[3];
};

static const GateReq GATES[] = {
    {B_DESERT, -1, 2, 3, {{M_WOOD, 12}, {M_STONE, 8}, {M_HIDE, 4}}},
    {B_SWAMP, B_DESERT, 4, 3, {{M_COPPERBAR, 5}, {M_LEATHER, 6}, {M_ROPE, 3}}},
    {B_SNOW, B_SWAMP, 7, 3, {{M_IRONBAR, 5}, {M_FUR, 8}, {M_ROPE, 4}}},
    {B_MOUNTAIN, B_SNOW, 10, 3, {{M_IRONBAR, 6}, {M_GOLDBAR, 2}, {M_PLANK, 8}}},
};
constexpr int GATE_N = (int)(sizeof(GATES) / sizeof(GATES[0]));

inline const GateReq* gateFor(int map) {
    for (int i = 0; i < GATE_N; i++)
        if (GATES[i].map == map) return &GATES[i];
    return nullptr;  // home and the homestead have no gate to open
}
