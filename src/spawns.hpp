// spawns.hpp - which species live where, and how many. Included after world.hpp (it needs the tile enum).
//
// The home area keeps its original ring-around-the-village population (see Game::populate). The four new
// biomes use "streaming" spawns: while the player is in a biome, species from its table are spawned just
// off-screen on suitable terrain, up to a local cap, with a per-species cooldown, and far-away animals are
// removed again. That keeps the number of live animals small no matter how big the world is.
#pragma once
#include "world.hpp"

constexpr unsigned TM(int t) { return 1u << t; }

struct SpawnDef {
    int kind;
    int cap;             // max live animals of this species within SPAWN_AREA of the player
    float cooldown;      // seconds before this species may spawn again after a spawn
    float chance;        // chance that a due spawn actually happens (rare predators use small values)
    unsigned terrain;    // allowed ground tiles (bit per Tile); 0 = any walkable ground
    float form1;         // chance that the animal uses its second form
    bool nearWater;      // must be within 3 tiles of deep water / bog
    float capNight;      // extra cap at night (0 = same as day)
};

constexpr float SPAWN_MIN = 250.f, SPAWN_MAX = 520.f;  // spawn ring around the player (off-screen)
constexpr float SPAWN_AREA = 800.f;                    // radius the caps are counted in
constexpr float DESPAWN_DIST = 920.f;                  // live animals this far from the player are removed
constexpr int MAX_LIVE_ANIMALS = 60;                   // hard ceiling on live animals (carcasses excluded)
constexpr float SPAWN_TICK = 1.2f;

static const std::vector<SpawnDef> SPAWNS[B_COUNT] = {
    // ---- home: handled by Game::populate (original behaviour)
    {},
    // ---- desert
    {
        {A_HARE, 6, 6, 1.0f, TM(T_SAND) | TM(T_DUNE), 0, false, 0},
        {A_FENNEC, 3, 14, 1.0f, TM(T_SAND) | TM(T_STONE), 0, false, 0},
        {A_COYOTE, 3, 16, 1.0f, TM(T_SAND) | TM(T_DUNE) | TM(T_STONE), 0, false, 2},
        {A_RATTLER, 3, 14, 1.0f, TM(T_SAND) | TM(T_STONE), 0, false, 0},
        {A_ROADRUNNER, 3, 12, 1.0f, TM(T_SAND) | TM(T_DUNE), 0, false, 0},
        {A_LIZARD, 5, 8, 1.0f, TM(T_SAND) | TM(T_STONE), 0, false, 0},
        {A_DUNELION, 1, 240, 0.12f, TM(T_SAND) | TM(T_DUNE), 0, false, 0},
    },
    // ---- swamp
    {
        {A_FROG, 7, 5, 1.0f, TM(T_MUD) | TM(T_MURK), 0, true, 0},
        {A_DUCK, 4, 10, 1.0f, TM(T_MURK), 0, false, 0},
        {A_MARSHDEER, 4, 16, 1.0f, TM(T_MUD) | TM(T_DARK) | TM(T_GRASS), 0.45f, false, 0},
        {A_BOAR, 3, 18, 1.0f, TM(T_MUD) | TM(T_DARK), 0, false, 0},
        {A_MOCCASIN, 3, 12, 1.0f, TM(T_MUD) | TM(T_MURK), 0, true, 0},
        {A_GATOR, 2, 30, 0.6f, TM(T_MURK), 0, true, 0},
        {A_PANTHER, 1, 240, 0.12f, TM(T_MUD) | TM(T_DARK), 0, false, 0},
    },
    // ---- snow
    {
        {A_ARCTICFOX, 4, 12, 1.0f, TM(T_SNOW) | TM(T_SNOWP), 0, false, 0},
        {A_SNOWHARE, 6, 6, 1.0f, TM(T_SNOW) | TM(T_SNOWP), 0, false, 0},
        {A_REINDEER, 4, 15, 1.0f, TM(T_SNOW) | TM(T_SNOWP), 0.5f, false, 0},
        {A_WOLF, 3, 20, 1.0f, TM(T_SNOW) | TM(T_SNOWP), 1.0f, false, 2},  // all snow wolves are the white form
        {A_MOOSE, 2, 40, 0.6f, TM(T_SNOW) | TM(T_SNOWP), 0.5f, false, 0},
        {A_LYNX, 2, 40, 0.6f, TM(T_SNOW) | TM(T_SNOWP), 0, false, 0},
        {A_POLARBEAR, 1, 240, 0.12f, TM(T_SNOW) | TM(T_SNOWP), 0, false, 0},
    },
    // ---- mountain
    {
        {A_GOAT, 5, 10, 1.0f, TM(T_STONE) | TM(T_GRASS), 0, false, 0},
        {A_BIGHORN, 3, 18, 1.0f, TM(T_STONE) | TM(T_GRASS), 0, false, 0},
        {A_DEER, 3, 18, 1.0f, TM(T_GRASS) | TM(T_DARK), 0.4f, false, 0},
        {A_EAGLE, 2, 20, 1.0f, TM(T_STONE) | TM(T_GRASS), 0, false, 0},
        {A_MARMOT, 4, 10, 1.0f, TM(T_STONE) | TM(T_GRASS), 0, false, 0},
        {A_WOLF, 2, 25, 1.0f, TM(T_STONE) | TM(T_GRASS) | TM(T_DARK), 0, false, 2},
        {A_MTLION, 1, 240, 0.12f, TM(T_STONE), 0, false, 0},
    },
};
