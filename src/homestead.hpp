// homestead.hpp - the player's Homestead: a persistent sandbox map (B_HOMESTEAD) where the hunter builds, farms and raises animals.
//
// Everything here is DATA plus the state structs; the behaviour is in homestead.cpp (building, farming, animals, saving), the map in
// homestead_gen.hpp, the art and the menus in homestead_render.cpp / sprites_homestead.hpp.
//   * pieces   - what the hunter builds (walls, floors, fences, buildings, wells, farm plots, lamps ...). One grid tile holds at most one piece per layer.
//   * crops    - a farm plot with a seed in it; grows through stages while it is watered, and keeps growing while the hunter is away.
//   * animals  - domestic animals with their own AI: they wander in the pen / yard they are assigned to, eat, produce, breed and grow up.
// The state lives in Game::hs and is saved in the normal save file (hs* lines); it is never regenerated, so the Homestead is never reset.
#pragma once
#include <vector>
#include "inventory.hpp"

// ---------------------------------------------------------------- pieces
enum HPKind {
    HP_WALL_WOOD, HP_WALL_STONE, HP_FLOOR_WOOD, HP_FLOOR_STONE, HP_FENCE, HP_GATE, HP_DOOR, HP_ROOF, HP_FOUNDATION,   // basic structures
    HP_SHED, HP_HOUSE, HP_BARN, HP_PEN, HP_COOP, HP_WORKSHOP, HP_WAREHOUSE, HP_GREENHOUSE,                           // functional buildings
    HP_BRIDGE, HP_WELL, HP_CAMPFIRE, HP_PLOT, HP_BENCH, HP_PLANTER, HP_BARREL, HP_SCARECROW, HP_SIGN, HP_LAMP, HP_TORCH,  // outdoor structures and decoration
    HP_COUNT
};
enum HPLayer { HL_GROUND, HL_OBJECT, HL_ROOF };
enum HPCat { HC_BASIC, HC_BUILDING, HC_OUTDOOR, HC_COUNT };
[[maybe_unused]] static const char* HCAT_NAME[HC_COUNT] = {"BASIC", "BUILDINGS", "OUTDOOR"};
enum HPSolid { HS_NONE, HS_ALL, HS_RING };  // none, the whole footprint, only the outer ring of tiles (a pen)

struct HPieceDef {
    const char* name;
    int w, h;          // footprint in tiles (rotate 90 degrees to swap)
    int layer, solid, cat;
    int mat[3], n[3];  // construction cost (dismantling returns 60%, rounded down, at least 1 of the first material)
    int storage;       // slots of storage (0 = none)
    int housing;       // animals it can house
    float light;       // light radius in pixels (0 = none)
    const char* blurb;
};
static const HPieceDef HPDEF[HP_COUNT] = {
    {"WOODEN WALL", 1, 1, HL_OBJECT, HS_ALL, HC_BASIC, {M_PLANK, -1, -1}, {2, 0, 0}, 0, 0, 0, "SOLID TIMBER WALL"},
    {"STONE WALL", 1, 1, HL_OBJECT, HS_ALL, HC_BASIC, {M_STONE, -1, -1}, {3, 0, 0}, 0, 0, 0, "STRONG STONE WALL"},
    {"WOODEN FLOOR", 1, 1, HL_GROUND, HS_NONE, HC_BASIC, {M_PLANK, -1, -1}, {1, 0, 0}, 0, 0, 0, "PLANKED FLOOR"},
    {"STONE FLOOR", 1, 1, HL_GROUND, HS_NONE, HC_BASIC, {M_STONE, -1, -1}, {2, 0, 0}, 0, 0, 0, "FLAGSTONE FLOOR"},
    {"FENCE", 1, 1, HL_OBJECT, HS_ALL, HC_BASIC, {M_WOOD, M_BRANCH, -1}, {1, 1, 0}, 0, 0, 0, "KEEPS ANIMALS IN"},
    {"GATE", 1, 1, HL_OBJECT, HS_ALL, HC_BASIC, {M_PLANK, M_BRANCH, -1}, {2, 1, 0}, 0, 0, 0, "E OPENS AND CLOSES IT"},
    {"WOODEN DOOR", 1, 1, HL_OBJECT, HS_ALL, HC_BASIC, {M_PLANK, M_FIBER, -1}, {3, 1, 0}, 0, 0, 0, "E OPENS AND CLOSES IT"},
    {"ROOF", 1, 1, HL_ROOF, HS_NONE, HC_BASIC, {M_PLANK, M_FIBER, -1}, {2, 1, 0}, 0, 0, 0, "SEE-THROUGH WHEN YOU STAND UNDER IT"},
    {"FOUNDATION", 1, 1, HL_GROUND, HS_NONE, HC_BASIC, {M_STONE, -1, -1}, {3, 0, 0}, 0, 0, 0, "LEVEL STONE BASE FOR BUILDING"},
    {"STORAGE SHED", 3, 2, HL_OBJECT, HS_ALL, HC_BUILDING, {M_PLANK, M_STONE, -1}, {8, 4, 0}, 30, 0, 0, "30 SLOTS OF STORAGE"},
    {"HOUSE", 5, 4, HL_OBJECT, HS_ALL, HC_BUILDING, {M_PLANK, M_STONE, M_ROPE}, {24, 12, 4}, 20, 0, 70.f, "A COSY HOME - 20 SLOTS, WARM LIGHT"},
    {"BARN", 6, 4, HL_OBJECT, HS_ALL, HC_BUILDING, {M_PLANK, M_WOOD, M_ROPE}, {30, 10, 6}, 30, 10, 0, "HOUSES 10 FARM ANIMALS - 30 SLOTS"},
    {"ANIMAL PEN", 6, 5, HL_OBJECT, HS_RING, HC_BUILDING, {M_WOOD, M_BRANCH, -1}, {16, 8, 0}, 0, 8, 0, "FENCED PADDOCK FOR 8 ANIMALS"},
    {"CHICKEN COOP", 3, 2, HL_OBJECT, HS_ALL, HC_BUILDING, {M_PLANK, M_FIBER, M_BRANCH}, {8, 6, 4}, 0, 6, 0, "HOUSES 6 BIRDS OR RABBITS"},
    {"WORKSHOP", 4, 3, HL_OBJECT, HS_ALL, HC_BUILDING, {M_PLANK, M_STONE, M_IRONBAR}, {16, 8, 2}, 0, 0, 60.f, "A FORGE AND BENCH - CRAFT AND SMELT BESIDE IT"},
    {"WAREHOUSE", 5, 3, HL_OBJECT, HS_ALL, HC_BUILDING, {M_PLANK, M_STONE, M_ROPE}, {20, 10, 4}, 60, 0, 0, "60 SLOTS OF STORAGE"},
    {"GREENHOUSE", 5, 4, HL_OBJECT, HS_NONE, HC_BUILDING, {M_PLANK, M_IRONBAR, M_ROPE}, {12, 4, 6}, 0, 0, 0, "CROPS UNDER IT GROW 1.5X FASTER AND DRY OUT SLOWER"},
    {"WOODEN BRIDGE", 1, 1, HL_GROUND, HS_NONE, HC_OUTDOOR, {M_PLANK, -1, -1}, {2, 0, 0}, 0, 0, 0, "CROSSES WATER"},
    {"WELL", 2, 2, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_STONE, M_ROPE, M_WOOD}, {12, 2, 2}, 0, 0, 0, "E REFILLS YOUR WATERING CAN"},
    {"CAMPFIRE", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_WOOD, M_STONE, -1}, {4, 3, 0}, 0, 0, 90.f, "SMELT, COOK AND KEEP WARM"},
    {"FARM PLOT", 1, 1, HL_GROUND, HS_NONE, HC_OUTDOOR, {-1, -1, -1}, {0, 0, 0}, 0, 0, 0, "TILLED SOIL - NEEDS A HOE IN YOUR PACK"},
    {"BENCH", 2, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_PLANK, -1, -1}, {3, 0, 0}, 0, 0, 0, "A PLACE TO SIT"},
    {"FLOWER PLANTER", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_PLANK, M_STONE, -1}, {2, 1, 0}, 0, 0, 0, "BRIGHT FLOWERS"},
    {"BARREL", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_PLANK, M_ROPE, -1}, {4, 1, 0}, 0, 0, 0, "A FARM BARREL"},
    {"SCARECROW", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_WOOD, M_FIBER, -1}, {2, 4, 0}, 0, 0, 0, "WATCHES OVER THE FIELD"},
    {"SIGNPOST", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_PLANK, M_BRANCH, -1}, {1, 1, 0}, 0, 0, 0, "POINTS THE WAY"},
    {"LAMP POST", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_IRONBAR, M_PLANK, M_GREASE}, {1, 1, 1}, 0, 0, 80.f, "LIGHTS THE NIGHT"},
    {"TORCH", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_BRANCH, M_GREASE, -1}, {2, 1, 0}, 0, 0, 56.f, "A SMALL FLAME"},
};
constexpr int H_MAX_PIECES = 900;
constexpr int H_GREEN_AREA_BONUS = 1;

// ---------------------------------------------------------------- crops
enum CropId { CR_WHEAT, CR_CARROT, CR_POTATO, CR_CABBAGE, CR_CORN, CR_TOMATO, CR_PUMPKIN, CR_STRAWBERRY, CR_COUNT };
static_assert(CR_COUNT == CROP_N, "crops and the produce / seed materials share one order");
constexpr int CROP_STAGES = 5;  // 0 seed, 1 sprout, 2 growing, 3 flowering/almost, 4 ripe
struct CropDef {
    const char* name;
    float stageSec;     // seconds the watered crop needs for each of the 4 growth steps (total = 4x)
    float thirst;       // how fast the soil dries (1 = a watering lasts 100 s)
    int yLo, yHi;       // produce when harvested
    int seedsLo, seedsHi;  // seeds that come back with the harvest
    int tool;           // ToolType needed to harvest it (-1 = bare hands)
    int xp;             // farming XP
    int seedPrice;      // coins for one seed at the Homestead Market
    bool regrow;        // strawberries and tomatoes bear again after being picked (back to stage 3)
};
static const CropDef CROPS[CR_COUNT] = {
    {"WHEAT", 40, 0.9f, 3, 5, 1, 2, TT_SICKLE, 4, 3, false},
    {"CARROT", 32, 1.0f, 2, 4, 1, 1, TT_HOE, 4, 3, false},
    {"POTATO", 45, 0.8f, 3, 5, 1, 1, TT_HOE, 5, 4, false},
    {"CABBAGE", 55, 1.1f, 1, 2, 1, 2, TT_HOE, 7, 6, false},
    {"CORN", 62, 1.2f, 2, 4, 1, 2, TT_SICKLE, 8, 7, false},
    {"TOMATO", 58, 1.3f, 3, 5, 1, 2, -1, 8, 8, true},
    {"PUMPKIN", 95, 1.0f, 1, 1, 2, 4, TT_AXE, 16, 12, false},
    {"STRAWBERRY", 70, 1.4f, 3, 6, 1, 2, -1, 10, 10, true},
};
inline int cropProduce(int c) { return M_CROP0 + c; }
inline int cropSeed(int c) { return M_SEED0 + c; }
inline int seedCrop(int mat) { return mat >= M_SEED0 && mat < M_SEED0 + CROP_N ? mat - M_SEED0 : -1; }
constexpr float PLOT_DRY_SEC = 100.f;        // a full watering lasts this long at thirst 1
constexpr float GREENHOUSE_GROWTH = 1.5f, GREENHOUSE_THIRST = 0.6f;
constexpr float CAN_CAPACITY = 12.f;         // plots one full watering can fills

// ---------------------------------------------------------------- domestic animals
enum DomKind { DA_CHICKEN, DA_COW, DA_SHEEP, DA_PIG, DA_GOAT, DA_DUCK, DA_RABBIT, DA_COUNT };
enum DomHousing { DH_ANY = 0, DH_FOWL = 1 };  // fowl and rabbits fit the coop; the big ones need a barn or a pen
struct DomDef {
    const char* name;
    const char* babyName;
    float growSec;       // seconds from newborn to adult
    int product;         // Mat the adult makes (-1 none)
    float prodSec;       // seconds per product (at full happiness)
    const char* prodName;
    int price;           // coins at the market (selling back pays half)
    bool small;          // fits the coop
    float speed;         // walking speed px/s
    float size;          // sprite scale (1 = a calf-sized cow)
    int feedA, feedB;    // favourite feed items (Mat) - any of the feed list works, these are loved
    int prodMax;         // how many pieces wait to be collected
};
static const DomDef DOMS[DA_COUNT] = {
    {"CHICKEN", "CHICK", 150, M_EGG, 90, "EGG", 40, true, 20.f, 0.75f, M_WHEAT, M_CORN, 6},
    {"COW", "CALF", 420, M_MILK, 150, "MILK", 220, false, 17.f, 1.35f, M_HAY, M_CARROT, 4},
    {"SHEEP", "LAMB", 330, M_WOOL, 190, "WOOL", 120, false, 17.f, 1.0f, M_HAY, M_CABBAGE, 4},
    {"PIG", "PIGLET", 300, M_TRUFFLE, 360, "TRUFFLE", 130, false, 19.f, 1.0f, M_POTATO, M_PUMPKIN, 2},
    {"GOAT", "KID", 300, M_MILK, 170, "MILK", 110, false, 21.f, 0.95f, M_HAY, M_CARROT, 4},
    {"DUCK", "DUCKLING", 170, M_DUCKEGG, 110, "DUCK EGG", 50, true, 19.f, 0.7f, M_WHEAT, M_CABBAGE, 5},
    {"RABBIT", "KIT", 200, M_FUR, 240, "FUR", 45, true, 26.f, 0.55f, M_CARROT, M_CABBAGE, 3},
};
// every farm animal eats these (the favourite twice as well)
inline bool isFeedMat(int m) {
    return m == M_HAY || m == M_WHEAT || m == M_CARROT || m == M_CABBAGE || m == M_CORN || m == M_POTATO || m == M_PUMPKIN || m == M_BERRY;
}
constexpr int H_MAX_ANIMALS = 26;
constexpr float FEED_FULL = 100.f, FEED_DECAY = 0.30f;  // fullness lost per second (a full belly lasts ~5 minutes)
constexpr float BREED_CD = 300.f;

// ---------------------------------------------------------------- state
struct HPiece {
    int id = 0, kind = 0;
    int tx = 0, ty = 0, rot = 0;
    bool open = false;               // doors and gates
    bool rich = false;               // a plot on fertile (dark) soil grows faster
    int crop = -1, stage = 0;        // farm plots
    float growT = 0, water = 0;      // seconds into the current stage; 0..1 how wet the soil is
    Inventory inv{0, 1e9f};          // storage buildings
    int w() const { return (rot & 1) ? HPDEF[kind].h : HPDEF[kind].w; }
    int h() const { return (rot & 1) ? HPDEF[kind].w : HPDEF[kind].h; }
    bool covers(int x, int y) const { return x >= tx && x < tx + w() && y >= ty && y < ty + h(); }
};

struct HAnimal {
    int id = 0, kind = 0;
    float px = 0, py = 0;            // position in pixels
    float tx = 0, ty = 0;            // where it is walking to
    int state = 0;                   // 0 idle, 1 walk, 2 eat
    float stT = 0, animT = 0;
    int face8 = 0;
    float age = 0;                   // seconds since birth
    float full = 70, happy = 70, health = 100;
    float prodT = 0;
    int stock = 0;                   // product waiting to be collected
    float breedCd = 60;
    int home = -1;                   // id of the pen / coop / barn it lives in (-1 = free)
    bool adult(const DomDef& d) const { return age >= d.growSec; }
};

struct Homestead {
    std::vector<HPiece> pieces;
    std::vector<HAnimal> animals;
    int nextId = 1;
    float can = 0;                   // water in the watering can (plots' worth)
    bool visited = false, gaveStarter = false;
    float clock = 0;                 // seconds the homestead has simulated (shown in the animal menu)
    float breedT = 0;
};
