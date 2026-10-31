// town.hpp - the central town: which buildings exist, where they stand on the home map and what they look like from outside.
//
// The town fills the middle of the original Wildwood area (core tile coordinates, see COX / COY in world.hpp). Six buildings stand in two
// rows either side of a plaza; each is a prop with a solid footprint, a door the hunter walks up to (E enters), and a sign.
// Interiors (interiors.hpp) are separate maps; this file is only the exterior facts, shared by world generation, art and game code.
#pragma once
#include "data.hpp"

enum BuildingKind { BLD_SMITH, BLD_TANNER, BLD_WORKSHOP, BLD_SHOP, BLD_FLOWER, BLD_SALON, BLD_COUNT };

struct BuildingDef {
    const char* name;  // shown in prompts and on the sign
    const char* sign;  // text painted on the sign board (kept short: 4 px per letter)
    int cx, cy;        // top-left tile of the solid footprint, in CORE coordinates (add COX / COY for the map)
    // exterior palette
    u32 wall, wallD, roof, roofD, roofL, trim, door;
};
constexpr int BLD_W = 6, BLD_H = 3;  // footprint in tiles (96 x 48 px); the roof and sign are drawn above it
constexpr int TOWN_X0 = 24, TOWN_Y0 = 16, TOWN_X1 = 64, TOWN_Y1 = 44;  // the town's rectangle (core tiles, X1/Y1 exclusive)

static const BuildingDef BUILDINGS[BLD_COUNT] = {
    {"BLACKSMITH", "BLACKSMITH", 25, 21, HEX(0x8a8a94), HEX(0x5e5e68), HEX(0x4a566e), HEX(0x333c52), HEX(0x6a7a96), HEX(0x2e2e38), HEX(0x3a3a42)},
    {"LEATHERWORKS", "LEATHERWORKS", 33, 21, HEX(0xd9b98a), HEX(0xa98a5e), HEX(0x8a4a24), HEX(0x62341a), HEX(0xb06a38), HEX(0x6b4423), HEX(0x5a3a1e)},
    {"WORKSHOP", "WORKSHOP", 49, 21, HEX(0xc9a56a), HEX(0x9a7a46), HEX(0x2f7a6a), HEX(0x215a4e), HEX(0x4aa08c), HEX(0x6b4423), HEX(0x7a5230)},
    {"GENERAL STORE", "GENERAL STORE", 57, 21, HEX(0xefe2c0), HEX(0xc9b88a), HEX(0xb8352c), HEX(0x84241e), HEX(0xd9584a), HEX(0x6b4423), HEX(0x8a2e2a)},
    {"FLOWER SHOP", "FLOWER SHOP", 30, 34, HEX(0xf3d4e0), HEX(0xcfa2b8), HEX(0x9a7ac8), HEX(0x6e529a), HEX(0xb89ae0), HEX(0xf4f0ea), HEX(0x7a9a4a)},
    {"SALON", "SALON", 52, 34, HEX(0xcfeff0), HEX(0x9ccbd0), HEX(0x7a3a8a), HEX(0x56276a), HEX(0xa05ab0), HEX(0xffffff), HEX(0x3a6a8a)},
};

// Tile / pixel positions derived from the table (core tile + the core offset COX / COY supplied by the caller, in tiles).
inline float bldCenterX(int k, int ox) { return (ox + BUILDINGS[k].cx + BLD_W / 2.f) * 16.f; }
inline float bldBaseY(int k, int oy) { return (oy + BUILDINGS[k].cy + BLD_H) * 16.f; }  // bottom edge of the footprint
// where the hunter stands to use the door (just in front of it), and how close counts
inline float bldDoorX(int k, int ox) { return bldCenterX(k, ox); }
inline float bldDoorY(int k, int oy) { return bldBaseY(k, oy) + 10.f; }
constexpr float BLD_DOOR_REACH = 22.f;

// the crafting book is split between the buildings by skill: metal work, leather work, everything else
inline const char* buildingName(int k) { return BUILDINGS[k].name; }
