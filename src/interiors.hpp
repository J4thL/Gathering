// interiors.hpp - the inside of each town building.
//
// An interior is a small room (INT_W x INT_H tiles) generated into the same tile / solid / props arrays as any other map, so walking,
// collision, depth sorting and drawing all work unchanged. `World::mapId` is B_COUNT + the BuildingKind while inside. Each room is
// described as a few rows of text ("puts") in a 22 x 11 grid of floor tiles; the walls, the doormat and the camera are added around it.
//
// Legend:  . floor   r rug   N the building's person   F furnace  A anvil  V forge  W workbench  B barrel  C crate  S shelf  T table
//          K counter  R tanning rack  L leather table  Q flower stand  P potted plant  M mirror  H salon chair  X tool rack
//          O stove  G grindstone  Y coal bin  U logs  I hides  Z sink  E display case
// The doormat is the two tiles at the bottom wall; keep x = 10..11 clear above it (the test in main.cpp --town checks every room).
#pragma once
#include <cstring>
#include "sprites_town.hpp"

constexpr int INT_RX = 8, INT_RY = 8;   // top-left tile of the room inside the map arrays
constexpr int INT_W = 24, INT_H = 14;   // the whole room including its walls: 384 x 224 px, about one screen
constexpr int INT_GW = 22, INT_GH = 11; // the floor grid inside the walls

namespace interior_detail {
struct Put {
    int x, y;
    const char* s;
};
struct Room {
    const Put* puts;
    int n;
    int floorKind;  // 0 wood, 1 flagstone, 2 tile
};
#define PUTS(name, ...) static const Put name[] = {__VA_ARGS__}

PUTS(SMITH, {0, 0, "FFYY.XXXX.....S.S.CCBB"}, {2, 2, "N"}, {2, 3, "A"}, {4, 3, "V"}, {9, 2, "rrrrrr"}, {9, 3, "rrrrrr"}, {19, 2, "U"}, {19, 3, "U"},
     {1, 5, "G"}, {13, 5, "W"}, {15, 5, "W"}, {1, 7, "B"}, {15, 7, "T"}, {17, 7, "T"}, {21, 7, "C"}, {9, 9, "rrrr"}, {10, 10, "rr"});
PUTS(TANNER, {0, 0, "RR.RR.SSSSSS.II.BB.CC."}, {3, 2, "L"}, {6, 2, "L"}, {15, 2, "N"}, {1, 4, "R"}, {3, 4, "R"}, {11, 4, "rrrrr"}, {11, 5, "rrrrr"},
     {11, 6, "rrrrr"}, {2, 6, "L"}, {19, 6, "Z"}, {0, 8, "BB"}, {18, 8, "UU"}, {10, 10, "rr"});
PUTS(WORKSHOP, {0, 0, "WWW.XX.SSSS.XX.SS.UUBB"}, {2, 2, "O"}, {10, 2, "N"}, {1, 4, "T"}, {4, 4, "T"}, {11, 4, "rrrr"}, {11, 5, "rrrr"}, {11, 6, "rrrr"},
     {19, 4, "C"}, {21, 4, "C"}, {1, 6, "W"}, {4, 6, "W"}, {1, 8, "G"}, {12, 8, "G"}, {21, 8, "B"}, {10, 10, "rr"});
PUTS(SHOP, {0, 0, "SSSS.SSSS..SSSS.SSSS.."}, {1, 2, "B"}, {3, 2, "C"}, {11, 2, "N"}, {17, 2, "EEE"}, {8, 3, "KKKKKK"}, {1, 5, "P"}, {20, 5, "P"},
     {9, 5, "rrrrrr"}, {9, 6, "rrrrrr"}, {2, 6, "EE"}, {1, 8, "B"}, {3, 8, "B"}, {12, 8, "T"}, {14, 8, "T"}, {21, 8, "C"}, {10, 10, "rr"});
PUTS(FLOWER, {1, 0, "PP.QQ.QQ.QQ.QQ.QQ.PP"}, {10, 2, "N"}, {8, 3, "KKKKKK"}, {1, 2, "Q"}, {20, 2, "Q"}, {2, 5, "Q"}, {5, 5, "Q"}, {16, 5, "Q"}, {19, 5, "Q"},
     {7, 5, "rrrrrrrr"}, {7, 6, "rrrrrrrr"}, {1, 8, "PP"}, {19, 8, "PP"}, {3, 7, "T"}, {17, 7, "T"}, {12, 8, "Q"}, {10, 10, "rr"});
PUTS(SALON, {0, 0, "MMM.MMM......MMM.MMM."}, {2, 2, "H"}, {6, 2, "H"}, {15, 2, "H"}, {19, 2, "H"}, {10, 4, "N"}, {8, 5, "KKKKKK"}, {1, 5, "Z"}, {20, 5, "Z"},
     {1, 7, "S"}, {20, 7, "S"}, {2, 8, "T"}, {18, 8, "T"}, {9, 7, "rrrrrr"}, {9, 8, "rrrrrr"}, {10, 10, "rr"});
#undef PUTS

static const Room ROOMS[BLD_COUNT] = {
    {SMITH, (int)(sizeof(SMITH) / sizeof(Put)), 1},   {TANNER, (int)(sizeof(TANNER) / sizeof(Put)), 0}, {WORKSHOP, (int)(sizeof(WORKSHOP) / sizeof(Put)), 0},
    {SHOP, (int)(sizeof(SHOP) / sizeof(Put)), 0},     {FLOWER, (int)(sizeof(FLOWER) / sizeof(Put)), 2}, {SALON, (int)(sizeof(SALON) / sizeof(Put)), 2},
};

inline int furnOf(char c) {
    switch (c) {
        case 'F': return FU_FURNACE; case 'A': return FU_ANVIL; case 'V': return FU_FORGE; case 'W': return FU_WORKBENCH;
        case 'B': return FU_BARREL; case 'C': return FU_CRATE; case 'S': return FU_SHELF; case 'T': return FU_TABLE;
        case 'K': return FU_COUNTER; case 'R': return FU_TANRACK; case 'L': return FU_LEATHERTABLE; case 'Q': return FU_FLOWERS;
        case 'P': return FU_POT; case 'M': return FU_MIRROR; case 'H': return FU_CHAIR; case 'X': return FU_TOOLS;
        case 'O': return FU_STOVE; case 'G': return FU_GRIND; case 'Y': return FU_COAL; case 'U': return FU_LOGS;
        case 'I': return FU_HIDES; case 'Z': return FU_SINK; case 'E': return FU_DISPLAY;
        default: return -1;
    }
}
// furniture that also opens the building's service when the hunter stands next to it
inline bool isStation(int building, int furn) {
    switch (building) {
        case BLD_SMITH: return furn == FU_ANVIL || furn == FU_FORGE || furn == FU_FURNACE;
        case BLD_TANNER: return furn == FU_TANRACK || furn == FU_LEATHERTABLE;
        case BLD_WORKSHOP: return furn == FU_WORKBENCH || furn == FU_STOVE;
        default: return false;
    }
}
}  // namespace interior_detail

inline void World::generateInterior(int kind) {
    using namespace interior_detail;
    if (kind < 0 || kind >= BLD_COUNT) kind = 0;
    const Room& R = ROOMS[kind];
    mapId = B_COUNT + kind;
    props.clear();
    decor.clear();
    nodes.clear();
    landmarks.clear();
    lanterns.clear();
    npcs.clear();
    stations.clear();
    memset(solid, 0, sizeof(solid));
    memset(biome, 0, sizeof(biome));
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) tile[y][x] = T_VOID;
    const uint8_t floorT = R.floorKind == 1 ? T_FLOORS : R.floorKind == 2 ? T_FLOORT : T_FLOORW;
    for (int y = 0; y < INT_H; y++)
        for (int x = 0; x < INT_W; x++) {
            uint8_t t = T_WALLT;
            if (y == 1 && x > 0 && x < INT_W - 1) t = T_WALL;
            else if (y >= 2 && y < INT_H - 1 && x > 0 && x < INT_W - 1) t = floorT;
            tile[INT_RY + y][INT_RX + x] = t;
        }
    const int doorX = INT_W / 2 - 1;  // the doormat is two tiles wide at the bottom wall
    tile[INT_RY + INT_H - 1][INT_RX + doorX] = T_MAT;
    tile[INT_RY + INT_H - 1][INT_RX + doorX + 1] = T_MAT;

    char grid[INT_GH][INT_GW + 1];
    for (auto& row : grid) {
        memset(row, '.', INT_GW);
        row[INT_GW] = 0;
    }
    for (int i = 0; i < R.n; i++) {
        const Put& p = R.puts[i];
        for (int k = 0; p.s[k]; k++)
            if (p.x + k < INT_GW && p.y < INT_GH && p.s[k] != '.') grid[p.y][p.x + k] = p.s[k];
    }
    for (int gy = 0; gy < INT_GH; gy++)
        for (int gx = 0; gx < INT_GW; gx++) {
            const char c = grid[gy][gx];
            const int tx = INT_RX + 1 + gx, ty = INT_RY + 2 + gy;
            if (c == '.') continue;
            if (c == 'r') {
                tile[ty][tx] = T_RUG;
                continue;
            }
            const V2 foot((tx + 0.5f) * TS, (ty + 1) * TS - 1.f);
            if (c == 'N') {
                npcs.push_back({kind, foot + V2(0, -1)});
                solid[ty][tx] = true;
                continue;
            }
            const int f = furnOf(c);
            if (f < 0) continue;
            props.push_back({P_FURN, foot.x, foot.y, (float)f});
            solid[ty][tx] = true;
            if (isStation(kind, f)) stations.push_back(foot);
        }
    exitPos = V2((INT_RX + doorX + 1) * (float)TS, (INT_RY + INT_H - 1) * (float)TS + 8.f);
    roomCenter = V2((INT_RX + INT_W / 2.f) * TS, (INT_RY + INT_H / 2.f) * TS);
    // everything of the outside world is parked far away so no prompt or draw call can find it
    const V2 away(-99999, -99999);
    village = hut = door = keeper = fire = questGiver = away;
    spawn = V2(exitPos.x, (INT_RY + INT_H - 2) * (float)TS + 8.f);
    std::sort(props.begin(), props.end(), [](const Prop& a, const Prop& b) { return a.y < b.y; });
}
