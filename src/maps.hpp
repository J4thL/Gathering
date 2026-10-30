// maps.hpp - the map table: the five maps of the game and how they connect.
//
// Every map is MW x MH = 240x168 tiles. The home map (B_HOME, with the original 88x60 Wildwood in its centre) and is where a new game starts;
// the desert, swamp, snow and mountain biomes each have a map of their own (index = Biome id). Maps connect through portals:
// a portal is a rectangle of tiles in the gap of a map's edge wall. Walking into it travels to `toMap` and arrives at
// (arriveTx, arriveTy) there. Home has four gates (east, south, north, west); each biome has one gate leading back home.
// Adding a biome = one more row in MAPS, a Biome id, a ground / landmark section in worldgen.hpp and a SPAWNS table.
#pragma once
#include "data.hpp"

struct TileRect {
    int x0, y0, x1, y1;  // inclusive
    bool has(int tx, int ty) const { return tx >= x0 && tx <= x1 && ty >= y0 && ty <= y1; }
};
struct Portal {
    TileRect zone;       // the gap tiles in the edge wall that trigger the trip
    int toMap;           // destination map (a Biome id)
    int arriveTx, arriveTy;  // where you stand when you arrive (a free tile is searched around it)
    int side;            // which edge the gap is in: 0 east, 1 south, 2 north, 3 west (used for the gate art and labels)
};
constexpr int MAX_PORTALS = 4;
constexpr uint32_t WORLD_SEED = 7;  // every map is generated from this seed (plus its own salt)
struct MapDef {
    const char* name;
    const char* shortName;  // for the "TO <name>" label at a gate
    int biome;
    int nPortals;
    Portal portals[MAX_PORTALS];
    int entryTx, entryTy;   // where you first arrive from home (the biome maps' trailhead) - also where a loaded save puts you
    int hubTx, hubTy;       // the main landmark: the trail from the gate leads here
};

// the maps, indexed by Biome id (B_HOME, B_DESERT, B_SWAMP, B_SNOW, B_MOUNTAIN). All are MW x MH = 240x168 tiles.
static const MapDef MAPS[B_COUNT] = {
    {"WILDWOOD", "WILDWOOD", B_HOME, 4,
     {{{MW - 2, 80, MW - 1, 88}, B_DESERT, 5, 84, 0},          // east gate -> desert's west entrance
      {{116, MH - 2, 124, MH - 1}, B_SWAMP, 120, 5, 1},        // south gate -> swamp's north entrance
      {{116, 0, 124, 1}, B_SNOW, 120, MH - 6, 2},              // north gate -> snow's south entrance
      {{0, 80, 1, 88}, B_MOUNTAIN, MW - 6, 84, 3}},            // west gate -> mountain's east entrance
     COX + 44, COY + 30, COX + 44, COY + 30},
    {"DESERT", "DESERT", B_DESERT, 1, {{{0, 82, 1, 86}, B_HOME, MW - 6, 84, 3}}, 5, 84, 60, 84},
    {"SWAMP", "SWAMP", B_SWAMP, 1, {{{118, 0, 122, 1}, B_HOME, 120, MH - 6, 2}}, 120, 5, 120, 40},
    {"SNOW", "SNOW", B_SNOW, 1, {{{118, MH - 2, 122, MH - 1}, B_HOME, 120, 5, 1}}, 120, MH - 6, 120, 128},
    {"MOUNTAIN", "MOUNTAIN", B_MOUNTAIN, 1, {{{MW - 2, 82, MW - 1, 86}, B_HOME, 5, 84, 0}}, MW - 6, 84, 190, 84},
};

// the portal whose zone contains the tile, or nullptr
inline const Portal* portalAt(int map, int tx, int ty) {
    if (map < 0 || map >= B_COUNT) return nullptr;
    for (int i = 0; i < MAPS[map].nPortals; i++)
        if (MAPS[map].portals[i].zone.has(tx, ty)) return &MAPS[map].portals[i];
    return nullptr;
}
