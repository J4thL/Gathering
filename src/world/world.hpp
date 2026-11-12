// world.hpp - one procedural map at a time: terrain, lakes, forests, the village or a biome, plus collision queries.
// The game has five maps (maps.hpp), every one exactly MW x MH = 240x168 tiles: the home map (the original 88x60 Wildwood
// from generateCore, unchanged, in its centre, with home forest around it) and one map each for the desert, swamp, snow and
// mountain biomes (worldgen.hpp). `World` holds the map
// that is currently active; Game::travelTo (maps.cpp) swaps it by regenerating the destination from the world seed.
#pragma once
#include "art/sprites.hpp"

constexpr int TS = 16;
constexpr int CW = 88, CH = 60;                          // the original Wildwood area (the core of the home map)
constexpr int MW = 240, MH = 168;                        // size of EVERY map
constexpr int COX = (MW - CW) / 2, COY = (MH - CH) / 2;  // where the original area starts inside the home map (in tiles)
constexpr int LW = 400, LH = 225;                        // logical (pixel-art) resolution
constexpr int MM_DIV = 3;                                // minimap: one pixel per MM_DIV x MM_DIV tiles
constexpr int MM_W = MW / MM_DIV, MM_H = MH / MM_DIV;

struct V2 {
    float x = 0, y = 0;
    V2() = default;
    V2(float x_, float y_) : x(x_), y(y_) {}
    V2 operator+(V2 o) const { return {x + o.x, y + o.y}; }
    V2 operator-(V2 o) const { return {x - o.x, y - o.y}; }
    V2 operator*(float k) const { return {x * k, y * k}; }
    V2& operator+=(V2 o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    float len() const { return sqrtf(x * x + y * y); }
    V2 norm() const {
        float l = len();
        return l > 1e-5f ? V2(x / l, y / l) : V2(0, 0);
    }
};
inline float dist(V2 a, V2 b) { return (a - b).len(); }

enum Tile : uint8_t {
    T_GRASS, T_DARK, T_DIRT, T_WATER,                           // original tiles (keep order: saves/tools may rely on it)
    T_SAND, T_DUNE, T_MUD, T_MURK, T_BOG, T_SNOW, T_SNOWP, T_ICE, T_STONE, T_CLIFF,
    T_COBBLE,                                                   // town paving (walkable)
    T_VOID, T_WALL, T_WALLT,                                    // building interiors: outside the room, wall face, wall cap (all solid)
    T_FLOORW, T_FLOORS, T_FLOORT, T_RUG, T_MAT,                 // interior floors: wood, flagstone, tile; rug; doormat (walkable)
    T_CFLOOR, T_CWALL, T_RAIL,                                  // the Abandoned Mine: cave floor, rock wall (solid), mine track (walkable)
    T_IFLOOR, T_IWALL, T_BRICK, T_CARPET                        // the Frozen Dungeon: ice-flag floor, ice wall (solid), frozen brick, royal carpet
};
// terrain properties, one place for all of them
inline bool tileSolid(uint8_t t) { return t == T_WATER || t == T_BOG || t == T_CLIFF || t == T_VOID || t == T_WALL || t == T_WALLT || t == T_CWALL || t == T_IWALL; }
inline bool tileDeep(uint8_t t) { return t == T_WATER || t == T_BOG; }  // projectiles splash, foam is drawn
inline float tileSpeed(uint8_t t) {
    switch (t) {
        case T_SAND: return 0.92f;
        case T_DUNE: return 0.84f;
        case T_MUD: return 0.76f;
        case T_MURK: return 0.58f;
        case T_SNOW: return 0.86f;
        case T_SNOWP: return 0.93f;
        default: return 1.f;
    }
}

enum PropKind {
    P_OAK0, P_OAK1, P_PINE, P_ROCK0, P_ROCK1, P_HUT, P_FIRE, P_CRATE, P_BARREL, P_SIGN,  // original
    P_CACTUS0, P_CACTUS1, P_PALM, P_SANDROCK0, P_SANDROCK1, P_COLUMN, P_RUINWALL, P_ARCH,   // desert
    P_DEADTREE0, P_DEADTREE1, P_LOG, P_STILTHUT, P_WITCHTREE, P_GRAVE,                      // swamp
    P_SNOWPINE, P_BARETREE, P_SNOWROCK, P_CRYSTAL, P_ICEHUT, P_CAIRN,                       // snow
    P_CAVE, P_BOULDER, P_STANDING,                                                          // mountain
    P_BSIGN0, P_BSIGN1, P_BSIGN2, P_BSIGN3,                                                 // biome gate signs
    P_BLD0, P_BLD1, P_BLD2, P_BLD3, P_BLD4, P_BLD5,                                         // town buildings (BuildingKind order)
    P_LAMP, P_FENCE, P_BENCH, P_PLANTER, P_WELL,                                            // town street furniture
    P_FURN,                                                                                 // interior furniture (Prop::ph = furniture id, interiors.hpp)
    // the Abandoned Mine and the Frozen Dungeon (delve.hpp; art in sprites_delve.hpp)
    P_MINEGATE, P_FROSTGATE,                                                                // the two entrances on the overworld maps
    P_BEAM, P_MINECART, P_TORCH, P_CAVECRYSTAL, P_ICECRYSTAL, P_STATUE, P_PILLAR, P_CHEST, P_STAIRS, P_EXIT,
    P_BOSSDOOR, P_EQUIP, P_DEBRIS, P_SARCO, P_THRONE, P_BRAZIER, P_BONES, P_BANNER, P_ICICLE,
    P_HPORTAL, P_HMARKET                                                                    // the Homestead Portal (town and homestead) and the homestead's market stall
};
struct Prop {
    int kind;
    float x, y;  // ground anchor
    float ph;    // swaying trees: sway phase. Rocks: which of the 5 looks (0-4); 100 and up = the same look, but blocking only its own tile
};
// ---- rocks: the collision follows the size of the sprite (rock_sprites.hpp): tiny rocks, small rocks and boulders each have 5 looks
inline bool isRockKind(int k) { return k == P_ROCK0 || k == P_ROCK1 || k == P_BOULDER; }
inline const RockArt* rockSet(int kind) { return kind == P_ROCK0 ? ROCK_TINY : kind == P_ROCK1 ? ROCK_SMALL : ROCK_LARGE; }
inline int rockLookOf(const Prop& p) { return std::abs((int)p.ph) % 100 % ROCK_LOOKS; }
inline bool rockOwnTileOnly(const Prop& p) { return p.ph >= 100.f; }
// the tiles a rock blocks: as wide as its sprite (a little less, for the outline) and one row deep at its foot
inline void rockFoot(const Prop& p, int look, bool ownTileOnly, int& x0, int& y0, int& x1, int& y1) {
    const int tx = (int)(p.x / TS), ty = (int)(p.y / TS);
    if (ownTileOnly) { x0 = x1 = tx; y0 = y1 = ty; return; }
    const RockArt& a = rockSet(p.kind)[look];
    const float hw = a.w * 0.4f, depth = std::min(a.h * 0.25f, 12.f);
    x0 = (int)floorf((p.x - hw) / TS); x1 = (int)floorf((p.x + hw) / TS);
    y0 = (int)floorf((p.y - depth) / TS); y1 = ty;
}
inline void rockFoot(const Prop& p, int& x0, int& y0, int& x1, int& y1) { rockFoot(p, rockLookOf(p), rockOwnTileOnly(p), x0, y0, x1, y1); }
enum DecorKind {
    D_FLOWER0, D_FLOWER1, D_FLOWER2, D_TUFT, D_MUSHROOM, D_PEBBLES, D_BUSH0, D_BUSH1,  // original (0-7)
    D_DRYBUSH, D_SKULL, D_SANDTUFT, D_REEDS0, D_REEDS1, D_LILY, D_BUBBLES, D_DRIFT, D_FROSTTUFT, D_ALPINE, D_GRAVEL, D_COUNT
};
struct Decor {
    int kind;
    float x, y, ph;
};
struct ResNode {
    int kind;  // NodeKind
    float x, y, ph;
};
struct Landmark {
    const char* name;
    int biome;
    float x, y;       // where the player "discovers" it
    bool found = false;
};
struct Lantern {
    float x, y;  // warm night glow
    float r;
};
// what the generator of a delve (mine / dungeon level) asks the game to spawn: monsters, chests, traps (delve.hpp)
enum DSpawnType { DS_MONSTER, DS_CHEST, DS_TRAP, DS_BOSS };
struct DSpawn {
    int type, kind;  // kind: MonsterKind / chest tier / trap kind
    float x, y;
    int id;          // chests: its index (the opened-chests bit mask), monsters: room number
    float depth;     // 0 (the entrance) .. 1 (the deepest part): scales monsters
    bool elite;
};
// a light in the dark of a delve (warm torch, cold ice glow, violet crystal); drawn by Game::drawDelveDark
struct DLight {
    float x, y, rad;
    int tone;  // 0 warm, 1 ice, 2 violet
};

inline float vnoise(float x, float y, uint32_t seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    auto h = [&](int a, int b) { return (hash2(a, b, seed) & 0xFFFF) / 65535.f; };
    float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    float a = h(xi, yi) + (h(xi + 1, yi) - h(xi, yi)) * u;
    float b = h(xi, yi + 1) + (h(xi + 1, yi + 1) - h(xi, yi + 1)) * u;
    return a + (b - a) * v;
}
inline float fbm(float x, float y, uint32_t seed) {
    return (vnoise(x, y, seed) * 0.5f + vnoise(x * 2, y * 2, seed + 11) * 0.25f + vnoise(x * 4, y * 4, seed + 23) * 0.125f) / 0.875f;
}

struct World {
    uint8_t tile[MH][MW];
    uint8_t biome[MH][MW];
    bool solid[MH][MW];
    bool occ[MH][MW];  // tall things that stop light: buildings, huts, caves, boulders, cliffs (buildOccluders, worldgen.hpp)
    std::vector<Prop> props;  // sorted by y once generation is done
    std::vector<Decor> decor;  // sorted by y
    std::vector<ResNode> nodes;  // sorted by y
    std::vector<Landmark> landmarks;
    std::vector<Lantern> lanterns;
    V2 village, hut, door, keeper, fire, spawn, questGiver;
    V2 hub[B_COUNT];  // the trail-head / main landmark of each biome
    uint32_t seed = 7;
    int mapId = B_HOME;  // which map is active (a Biome id: B_HOME is the original map; MAP_INT0 + building = a building interior)
    // ---- building interiors (interiors.hpp): a small room generated into the same arrays
    struct Npc {
        int kind;  // BuildingKind of the building the NPC works in
        V2 pos;
    };
    std::vector<Npc> npcs;
    std::vector<V2> stations;  // spots that open the building's service (besides the NPC): anvil, tanning rack, workbench...
    V2 exitPos;                // the doormat: standing here offers LEAVE
    V2 roomCenter;             // the camera is fixed on this while inside
    bool isInterior() const { return mapId >= B_COUNT; }  // a building interior or a delve: no outside state of its own
    int interiorKind() const { return mapId - B_COUNT; }
    // ---- delves (delve.hpp): the Abandoned Mine and the four levels of the Frozen Dungeon, instance maps in the same arrays
    bool isDelve() const { return mapId >= B_COUNT + 8; }
    int delveId() const { return mapId - (B_COUNT + 8); }
    std::vector<DSpawn> dsp;
    std::vector<DLight> lights;
    V2 stairsDn, stairsUp;     // the stairs of a dungeon level
    V2 gate;                   // on the mountain / snow maps: where E enters the mine / the dungeon (x < 0: none)
    V2 hportal = V2(-99999.f, -99999.f), hmarket = V2(-99999.f, -99999.f);  // the Homestead Portal (town: the way in, homestead: the way back) and the market stall
    V2 bossPos;                // the Frostbound King's spot in the throne room
    V2 bossDoor;               // the door of the throne room (opens once the antechamber is crossed)

    float pixW() const { return MW * TS; }
    float pixH() const { return MH * TS; }
    // the original map's pixel rectangle
    float coreX0() const { return COX * TS; }
    float coreY0() const { return COY * TS; }
    float coreX1() const { return (COX + CW) * TS; }
    float coreY1() const { return (COY + CH) * TS; }

    // deep water (lakes, bog): blocks movement, splashes projectiles
    bool water(int tx, int ty) const { return tx >= 0 && ty >= 0 && tx < MW && ty < MH && tileDeep(tile[ty][tx]); }
    bool tileIs(int tx, int ty, uint8_t t) const { return tx >= 0 && ty >= 0 && tx < MW && ty < MH && tile[ty][tx] == t; }
    uint8_t tileAt(float x, float y) const {
        int tx = (int)floorf(x / TS), ty = (int)floorf(y / TS);
        tx = tx < 0 ? 0 : (tx >= MW ? MW - 1 : tx);
        ty = ty < 0 ? 0 : (ty >= MH ? MH - 1 : ty);
        return tile[ty][tx];
    }
    int biomeAtPos(float x, float y) const {
        int tx = (int)floorf(x / TS), ty = (int)floorf(y / TS);
        tx = tx < 0 ? 0 : (tx >= MW ? MW - 1 : tx);
        ty = ty < 0 ? 0 : (ty >= MH ? MH - 1 : ty);
        return biome[ty][tx];
    }
    bool solidTile(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= MW || ty >= MH) return true;
        return solid[ty][tx] || tileSolid(tile[ty][tx]);
    }
    bool blocked(float x, float y, float r) const {
        int x0 = (int)floorf((x - r) / TS), x1 = (int)floorf((x + r) / TS);
        int y0 = (int)floorf((y - r) / TS), y1 = (int)floorf((y + r) / TS);
        for (int ty = y0; ty <= y1; ty++)
            for (int tx = x0; tx <= x1; tx++)
                if (solidTile(tx, ty)) return true;
        return false;
    }
    void markSolidAt(float x, float y) {
        int tx = (int)(x / TS), ty = (int)(y / TS);
        if (tx >= 0 && ty >= 0 && tx < MW && ty < MH) solid[ty][tx] = true;
    }

    void generate(uint32_t sd, int map = B_HOME) {
        seed = sd;
        mapId = map;
        props.clear();
        decor.clear();
        nodes.clear();
        landmarks.clear();
        lanterns.clear();
        npcs.clear();
        stations.clear();
        dsp.clear();
        lights.clear();
        gate = stairsDn = stairsUp = bossPos = bossDoor = V2(-1, -1);
        hportal = hmarket = V2(-99999.f, -99999.f);
        memset(solid, 0, sizeof(solid));
        memset(occ, 0, sizeof(occ));
        memset(biome, 0, sizeof(biome));
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) tile[y][x] = T_GRASS;
        if (map == B_HOME) generateCore();
        generateMap(map);  // worldgen.hpp: the home map's gates and forest, or a whole biome map
        if (map == B_HOME) addTownBelt();  // worldgen.hpp: more rows of trees outside the box around the town
        thinTrees();       // worldgen.hpp: no oaks or pines standing on top of each other
        // a branch whose tree was thinned out would lie alone: drop it
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const ResNode& n) { return n.kind == RN_BRANCH && !nearTree(n.x, n.y, 48.f); }), nodes.end());
        fitRocks();        // worldgen.hpp: every rock blocks as wide as its sprite is, where there is room
        buildOccluders();  // worldgen.hpp: what stops light (game_light.cpp)
        std::sort(props.begin(), props.end(), [](const Prop& a, const Prop& b) { return a.y < b.y; });
        std::sort(decor.begin(), decor.end(), [](const Decor& a, const Decor& b) { return a.y < b.y; });
        std::sort(nodes.begin(), nodes.end(), [](const ResNode& a, const ResNode& b) { return a.y < b.y; });
    }

    // The original map. This is the pre-expansion generator, unchanged except that every position is
    // shifted by the core offset so the random sequence (and therefore the start area) is identical.
    void generateCore() {
        Rng r(seed * 7919u + 13);
        const float vx = 44, vy = 30;
        const float OX = COX * TS, OY = COY * TS;
        auto T = [&](int x, int y) -> uint8_t& { return tile[COY + y][COX + x]; };
        auto Sd = [&](int x, int y) -> bool& { return solid[COY + y][COX + x]; };
        village = V2(vx * TS + 8 + OX, vy * TS + 8 + OY);

        struct Lake {
            float cx, cy, rx, ry;
        } lakes[] = {{16, 13, 9, 5.5f}, {70, 46, 11, 6.5f}, {72, 12, 7, 4.5f}, {13, 47, 8, 5.5f}, {34, 53, 5, 3.5f}, {58, 8, 3.5f, 2.5f}};
        for (int ty = 0; ty < CH; ty++)
            for (int tx = 0; tx < CW; tx++) {
                uint8_t t = T_GRASS;
                if (fbm(tx * 0.08f, ty * 0.08f, seed + 1) > 0.6f) t = T_DARK;
                for (auto& l : lakes) {
                    float dx = (tx - l.cx) / l.rx, dy = (ty - l.cy) / l.ry;
                    float d = dx * dx + dy * dy + (fbm(tx * 0.22f, ty * 0.22f, seed + 7) - 0.5f) * 0.8f;
                    if (d < 1.f) t = T_WATER;
                }
                T(tx, ty) = t;
            }
        // village plaza + dirt paths
        for (int ty = 0; ty < CH; ty++)
            for (int tx = 0; tx < CW; tx++) {
                float d = hypotf(tx - vx, (ty - vy) * 1.15f);
                float th = 6.3f + (vnoise(tx * 0.5f, ty * 0.5f, seed + 3) - 0.5f) * 2.2f;
                if (d < th) T(tx, ty) = T_DIRT;
            }
        int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0; k < 4; k++)
            for (int i = 5; i < 30; i++) {
                float wig = sinf(i * 0.37f + k * 2.1f) * 1.6f;
                int px = (int)(vx + dirs[k][0] * i + (dirs[k][0] ? 0 : wig));
                int py = (int)(vy + dirs[k][1] * i + (dirs[k][1] ? 0 : wig));
                for (int a = 0; a < 2; a++)
                    for (int b = 0; b < 2; b++) {
                        int x = px + a, y = py + b;
                        if (x > 1 && y > 1 && x < CW - 2 && y < CH - 2 && T(x, y) != T_WATER) T(x, y) = T_DIRT;
                    }
            }

        // village props
        int htx = 42, hty = 25;
        hut = V2((htx + 1.5f) * TS + OX, (hty + 2) * TS + OY);
        for (int y = hty; y < hty + 2; y++)
            for (int x = htx; x < htx + 3; x++) Sd(x, y) = true;
        props.push_back({P_HUT, hut.x, hut.y, 0});
        door = V2(hut.x, hut.y + 12);
        keeper = V2(hut.x + 40, hut.y + 8);
        fire = V2(hut.x + 76, hut.y + 40);
        spawn = V2(hut.x, hut.y + 48);
        questGiver = V2(fire.x - 40, fire.y + 34);  // the elder sits south-west of the campfire, clear of the merchant
        props.push_back({P_FIRE, fire.x, fire.y, 0});
        markSolidAt(fire.x, fire.y - 3);
        props.push_back({P_CRATE, hut.x - 40, hut.y + 4, 0});
        markSolidAt(hut.x - 40, hut.y + 2);
        props.push_back({P_BARREL, hut.x - 56, hut.y + 6, 0});
        markSolidAt(hut.x - 56, hut.y + 4);
        props.push_back({P_SIGN, hut.x - 26, hut.y + 20, 0});
        markSolidAt(hut.x - 26, hut.y + 18);
        props.push_back({P_CRATE, hut.x - 38, hut.y - 10, 0});

        // trees, rocks, decor
        for (int ty = 0; ty < CH; ty++)
            for (int tx = 0; tx < CW; tx++) {
                uint8_t t = T(tx, ty);
                const float bx = OX + tx * TS, by = OY + ty * TS;
                bool border = tx < 2 || ty < 2 || tx >= CW - 2 || ty >= CH - 2;
                if (border) {
                    Sd(tx, ty) = true;
                    props.push_back({r.chance(0.6f) ? P_PINE : P_OAK0, bx + 8.f + r.range(-2, 2), by + 15.f, r.f() * 6.28f});
                    continue;
                }
                if (t == T_WATER || Sd(tx, ty)) continue;
                float dv = hypotf(tx - vx, ty - vy);
                if (t == T_DIRT || dv < 9) {
                    if (t != T_DIRT && r.chance(0.04f))
                        decor.push_back({r.chance(0.5f) ? 3 : r.irange(0, 2), bx + r.range(2, 14), by + r.range(6, 15), r.f() * 6.28f});
                    continue;
                }
                float ff = fbm(tx * 0.08f, ty * 0.08f, seed + 1);
                float pTree = 0.014f + 0.36f * std::min(1.f, std::max(0.f, (ff - 0.5f) / 0.2f));
                if (r.chance(pTree)) {
                    int kind = (t == T_DARK) ? (r.chance(0.55f) ? P_PINE : P_OAK0) : (r.chance(0.12f) ? P_PINE : (r.chance(0.5f) ? P_OAK0 : P_OAK1));
                    props.push_back({kind, bx + 8.f + r.range(-2, 2), by + 15.f, r.f() * 6.28f});
                    Sd(tx, ty) = true;
                    continue;
                }
                if (r.chance(0.007f)) {
                    props.push_back({r.chance(0.6f) ? P_ROCK0 : P_ROCK1, bx + 8.f, by + 14.f, 0});
                    Sd(tx, ty) = true;
                    continue;
                }
                if (r.chance(0.011f)) {
                    decor.push_back({6 + r.irange(0, 1), bx + 8.f, by + 15.f, r.f() * 6.28f});
                    continue;
                }
                if (r.chance(0.085f)) {
                    float roll = r.f();
                    int kind = roll < 0.3f ? r.irange(0, 2) : roll < 0.82f ? 3 : roll < 0.9f ? (t == T_DARK ? 4 : 3) : 5;
                    decor.push_back({kind, bx + r.range(2, 14), by + r.range(6, 15), r.f() * 6.28f});
                }
            }
        for (int ty = 0; ty < CH; ty++)
            for (int tx = 0; tx < CW; tx++) biome[COY + ty][COX + tx] = B_HOME;
    }

    void generateMap(int map);  // worldgen.hpp
    void thinTrees();           // worldgen.hpp
    void addTownBelt();         // worldgen.hpp
    void fitRocks();            // worldgen.hpp
    void buildOccluders();      // worldgen.hpp
    // true when a standing tree (oak, pine, spruce, dead or bare tree) is within maxDist px of the point: where fallen branches may lie
    bool nearTree(float x, float y, float maxDist) const {
        for (const Prop& p : props) {
            if (p.kind != P_OAK0 && p.kind != P_OAK1 && p.kind != P_PINE && p.kind != P_SNOWPINE && p.kind != P_DEADTREE0 &&
                p.kind != P_DEADTREE1 && p.kind != P_BARETREE)
                continue;
            const float dx = p.x - x, dy = p.y - y;
            if (dx * dx + dy * dy <= maxDist * maxDist) return true;
        }
        return false;
    }
    // true when something tall stands between the two points (the first 20 px from the start are ignored: a lamp on a wall lights outwards)
    bool lightBlocked(float x0, float y0, float x1, float y1) const {
        const float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
        const int steps = (int)(len / 5.f);
        for (int i = 4; i < steps; i++) {
            const float f = (float)i / steps;
            const int tx = (int)((x0 + dx * f) / TS), ty = (int)((y0 + dy * f) / TS);
            if (tx >= 0 && ty >= 0 && tx < MW && ty < MH && occ[ty][tx]) return true;
        }
        return false;
    }
    void setPropSolid(const Prop& p, bool v);  // a tree's tile, or all of a rock's tiles (felled and broken things stop blocking)
    void generateTown();        // worldgen.hpp: the central town on the home map
    // inside the town's rectangle (home map only), `margin` pixels more generous: no animal spawns or walks in, nothing grows there
    bool inTown(float x, float y, float margin = 0.f) const;
    void generateHomestead();   // homestead_gen.hpp: the player's sandbox map
    void generateInterior(int kind);  // interiors.hpp: the inside of one town building
    void generateDelve(int id);       // delve.hpp: the mine / one dungeon level

    // colour of a tile / prop on the minimap
    static u32 miniTileColor(uint8_t t) {
        switch (t) {
            case T_DARK: return HEX(0x3e8a30);
            case T_DIRT: return HEX(0xb08556);
            case T_WATER: return HEX(0x3c7fc7);
            case T_SAND: return HEX(0xdcc07a);
            case T_DUNE: return HEX(0xc9a45c);
            case T_MUD: return HEX(0x6b5438);
            case T_MURK: return HEX(0x4a6b44);
            case T_BOG: return HEX(0x2e4535);
            case T_SNOW: return HEX(0xeaf1f7);
            case T_SNOWP: return HEX(0xd3e0ec);
            case T_ICE: return HEX(0x9fd0ea);
            case T_STONE: return HEX(0x858893);
            case T_CLIFF: return HEX(0x55586a);
            case T_COBBLE: return HEX(0xa49a88);
            case T_VOID: return HEX(0x000000);
            case T_WALL: case T_WALLT: return HEX(0x6a4a30);
            case T_FLOORW: case T_FLOORS: case T_FLOORT: case T_RUG: case T_MAT: return HEX(0xb08556);
            default: return HEX(0x5aa63f);
        }
    }

    // minimap image: MM_W x MM_H, each pixel is the average of MM_DIV x MM_DIV tiles (props included)
    Img minimap() const {
        Img full(MW, MH);
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) full.set(x, y, miniTileColor(tile[y][x]));
        for (auto& p : props) {
            int x = (int)(p.x / TS), y = (int)(p.y / TS);
            u32 c = 0;
            switch (p.kind) {
                case P_OAK0: case P_OAK1: case P_PINE: c = HEX(0x24602a); break;
                case P_ROCK0: case P_ROCK1: c = HEX(0x8d929e); break;
                case P_SNOWPINE: c = HEX(0x4d7d6c); break;
                case P_BLD0: case P_BLD1: case P_BLD2: case P_BLD3: case P_BLD4: case P_BLD5: c = HEX(0x8a4a24); break;
                case P_DEADTREE0: case P_DEADTREE1: case P_WITCHTREE: c = HEX(0x3a2f24); break;
                case P_CACTUS0: case P_CACTUS1: case P_PALM: c = HEX(0x4f9a52); break;
                default: break;
            }
            if (c) full.set(x, y, c);
        }
        Img im(MM_W, MM_H);
        for (int y = 0; y < MM_H; y++)
            for (int x = 0; x < MM_W; x++) {
                int r = 0, g = 0, b = 0;
                for (int j = 0; j < MM_DIV; j++)
                    for (int i = 0; i < MM_DIV; i++) {
                        u32 c = full.get(x * MM_DIV + i, y * MM_DIV + j);
                        r += cR(c);
                        g += cG(c);
                        b += cB(c);
                    }
                int n = MM_DIV * MM_DIV;
                im.set(x, y, RGBA(r / n, g / n, b / n));
            }
        return im;
    }
};

#include "world/maps.hpp"
#include "world/worldgen.hpp"
#include "world/interiors.hpp"
#include "delve/delve.hpp"
