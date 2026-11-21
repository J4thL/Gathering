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
#include "animals/mounts.hpp"
#include "items/inventory.hpp"
#include "farming/machines.hpp"

// ---------------------------------------------------------------- pieces
enum HPKind {
    HP_WALL_WOOD, HP_WALL_STONE, HP_FLOOR_WOOD, HP_FLOOR_STONE, HP_FENCE, HP_GATE, HP_DOOR, HP_ROOF, HP_FOUNDATION,   // basic structures
    HP_SHED, HP_HOUSE, HP_BARN, HP_PEN, HP_COOP, HP_WORKSHOP, HP_WAREHOUSE, HP_GREENHOUSE,                           // functional buildings
    HP_BRIDGE, HP_WELL, HP_CAMPFIRE, HP_PLOT, HP_BENCH, HP_PLANTER, HP_BARREL, HP_SCARECROW, HP_SIGN, HP_LAMP, HP_TORCH,  // outdoor structures and decoration
    HP_TROPHY,  // trophy wall: shows the trophies the hunter has earned (appended last so older saves keep their piece numbers)
    HP_HOLE, HP_POND,  // dug with the shovel: a hole in the ground, and a hole filled with water (appended: older saves keep their numbers)
    HP_MILL, HP_DAIRY, HP_KETTLE, HP_SMOKER, HP_OVEN, HP_SAWMILL, HP_SMELTER,  // production machines (machines.hpp), in the order of MACHINES[]
    HP_COUNT
};
enum HPLayer { HL_GROUND, HL_OBJECT, HL_ROOF };
enum HPCat { HC_BASIC, HC_BUILDING, HC_OUTDOOR, HC_MACHINE, HC_COUNT };
[[maybe_unused]] static const char* HCAT_NAME[HC_COUNT] = {"BASIC", "BUILDINGS", "OUTDOOR", "MACHINES"};
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
    {"GREENHOUSE", 5, 4, HL_OBJECT, HS_NONE, HC_BUILDING, {M_PLANK, M_IRONBAR, M_ROPE}, {12, 4, 6}, 0, 0, 0, "CROPS UNDER IT GROW 1.5X FASTER, AND RAIN CANNOT SOAK THE SOIL"},
    {"WOODEN BRIDGE", 1, 1, HL_GROUND, HS_NONE, HC_OUTDOOR, {M_PLANK, -1, -1}, {2, 0, 0}, 0, 0, 0, "CROSSES WATER"},
    {"WELL", 2, 2, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_STONE, M_ROPE, M_WOOD}, {12, 2, 2}, 0, 0, 0, "E REFILLS YOUR WATERING CAN"},
    {"CAMPFIRE", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_WOOD, M_STONE, -1}, {4, 3, 0}, 0, 0, 90.f, "SMELT, COOK AND KEEP WARM"},
    {"FARM PLOT", 1, 1, HL_GROUND, HS_NONE, HC_OUTDOOR, {-1, -1, -1}, {0, 0, 0}, 0, 0, 0, "TILLED SOIL - TILL WITH THE HOE, UNTILL WITH THE SHOVEL"},
    {"BENCH", 2, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_PLANK, -1, -1}, {3, 0, 0}, 0, 0, 0, "A PLACE TO SIT"},
    {"FLOWER PLANTER", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_PLANK, M_STONE, -1}, {2, 1, 0}, 0, 0, 0, "BRIGHT FLOWERS"},
    {"BARREL", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_PLANK, M_ROPE, -1}, {4, 1, 0}, 0, 0, 0, "A FARM BARREL"},
    {"SCARECROW", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_WOOD, M_FIBER, -1}, {2, 4, 0}, 0, 0, 0, "WATCHES OVER THE FIELD"},
    {"SIGNPOST", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_PLANK, M_BRANCH, -1}, {1, 1, 0}, 0, 0, 0, "POINTS THE WAY"},
    {"LAMP POST", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_IRONBAR, M_PLANK, M_GREASE}, {1, 1, 1}, 0, 0, 80.f, "LIGHTS THE NIGHT"},
    {"TORCH", 1, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_BRANCH, M_GREASE, -1}, {2, 1, 0}, 0, 0, 56.f, "A SMALL FLAME"},
    {"TROPHY WALL", 4, 1, HL_OBJECT, HS_ALL, HC_OUTDOOR, {M_PLANK, M_ROPE, -1}, {6, 1, 0}, 0, 0, 0, "SHOWS EVERY TROPHY YOU HAVE EARNED"},
    {"DUG HOLE", 1, 1, HL_GROUND, HS_NONE, -1, {-1, -1, -1}, {0, 0, 0}, 0, 0, 0, "A HOLE - POUR WATER IN IT, OR NEXT TO WATER IT FLOODS"},   // cat -1: never in the build menu
    {"POND", 1, 1, HL_GROUND, HS_NONE, -1, {-1, -1, -1}, {0, 0, 0}, 0, 0, 0, "WADE IN IT - WATERS TILLED SOIL NEARBY"},
    {"GRAIN MILL", 3, 2, HL_OBJECT, HS_ALL, HC_MACHINE, {M_PLANK, M_STONE, M_ROPE}, {12, 6, 2}, 0, 0, 0, "E: WHEAT AND CORN BECOME FLOUR"},
    {"DAIRY PRESS", 2, 2, HL_OBJECT, HS_ALL, HC_MACHINE, {M_PLANK, M_IRONBAR, -1}, {8, 1, 0}, 0, 0, 0, "E: MILK BECOMES BUTTER AND CHEESE"},
    {"PRESERVING KETTLE", 2, 2, HL_OBJECT, HS_ALL, HC_MACHINE, {M_STONE, M_IRONBAR, M_WOOD}, {6, 2, 4}, 0, 0, 40.f, "E: BERRIES MAKE JAM, VEGETABLES PICKLES"},
    {"SMOKEHOUSE", 2, 2, HL_OBJECT, HS_ALL, HC_MACHINE, {M_PLANK, M_STONE, M_ROPE}, {10, 6, 1}, 0, 0, 35.f, "E: SMOKE MEAT INTO COOKED MEAT AND JERKY"},
    {"BAKERY OVEN", 2, 2, HL_OBJECT, HS_ALL, HC_MACHINE, {M_STONE, M_IRONBAR, M_PLANK}, {14, 1, 4}, 0, 0, 60.f, "E: BREAD, OMELETS AND PIES"},
    {"SAWMILL", 4, 2, HL_OBJECT, HS_ALL, HC_MACHINE, {M_PLANK, M_IRONBAR, M_ROPE}, {14, 3, 2}, 0, 0, 0, "E: LOGS BECOME PLANKS, FIBER BECOMES ROPE"},
    {"SMELTER", 2, 2, HL_OBJECT, HS_ALL, HC_MACHINE, {M_STONE, M_IRONBAR, M_PLANK}, {16, 2, 4}, 0, 0, 80.f, "E: ORE AND WOOD BECOME BARS"},
};
constexpr int H_MAX_PIECES = 900;

// ---------------------------------------------------------------- crops
enum CropId { CR_WHEAT, CR_CARROT, CR_POTATO, CR_CABBAGE, CR_CORN, CR_TOMATO, CR_PUMPKIN, CR_STRAWBERRY, CR_EGGPLANT, CR_TURNIP, CR_PEPPER, CR_CUCUMBER, CR_BEET, CR_LEEK, CR_COUNT };
static_assert(CR_COUNT == CROP_N, "crops and the produce / seed materials share one order");
constexpr int CROP_STAGES = 5;  // 0 seed, 1 sprout, 2 growing, 3 flowering/almost, 4 ripe
struct CropDef {
    const char* name;
    float stageSec;     // seconds the watered crop needs for each of the 4 growth steps (total = 4x)
    int yLo, yHi;       // produce when harvested
    int seedsLo, seedsHi;  // seeds that come back with the harvest
    int tool;           // ToolType needed to harvest it (-1 = bare hands)
    int xp;             // farming XP
    int seedPrice;      // coins for one seed at the General Store
    bool regrow;        // strawberries and tomatoes bear again after being picked (back to stage 3)
};
static const CropDef CROPS[CR_COUNT] = {
    {"WHEAT", 40, 3, 5, 1, 2, TT_SICKLE, 4, 3, false},
    {"CARROT", 32, 2, 4, 1, 1, TT_HOE, 4, 3, false},
    {"POTATO", 45, 3, 5, 1, 1, TT_HOE, 5, 4, false},
    {"CABBAGE", 55, 1, 2, 1, 2, TT_HOE, 7, 6, false},
    {"CORN", 62, 2, 4, 1, 2, TT_SICKLE, 8, 7, false},
    {"TOMATO", 58, 3, 5, 1, 2, -1, 8, 8, true},
    {"PUMPKIN", 95, 1, 1, 2, 4, TT_AXE, 16, 12, false},
    {"STRAWBERRY", 70, 3, 6, 1, 2, -1, 10, 10, true},
    {"EGGPLANT", 80, 1, 3, 1, 2, TT_HOE, 14, 10, true},
    {"TURNIP", 28, 2, 3, 1, 1, TT_HOE, 4, 3, false},
    {"PEPPER", 60, 2, 4, 1, 2, TT_HOE, 10, 8, true},
    {"CUCUMBER", 50, 2, 4, 1, 2, TT_HOE, 8, 6, true},
    {"BEET", 38, 2, 3, 1, 2, TT_HOE, 6, 5, false},
    {"LEEK", 48, 2, 3, 1, 2, TT_HOE, 7, 6, false},
};
inline int cropProduce(int c) { return cropProduceMat(c); }
inline int cropSeed(int c) { return cropSeedMat(c); }
// Watering: one watering lasts a crop's whole growth (the soil dries again when the crop is ripe). Bare tilled soil dries slowly,
// and rain soaks every open-air plot.
constexpr float PLOT_DRY_SEC = 100.f;        // a watered plot with nothing planted dries after this long
constexpr float GREENHOUSE_GROWTH = 1.5f;
constexpr float RAIN_SOAK_RATE = 0.12f;      // wetness per second added to open-air plots by a storm (1.0 = soaked)
constexpr int HAY_PRICE = 4;                 // coins for a bundle of hay at the General Store
struct ToolBuy { int tool; int price; };
static const ToolBuy TOOL_BUY[] = {{TL_HOE, 30}, {TL_CAN, 25}, {TL_SICKLE, 30}, {TL_SHOVEL, 30}};  // the farm tools the General Store sells
constexpr float CAN_CAPACITY = 12.f;
constexpr float HOLE_WATER = 4.f;            // watering-can water that turns a hole into a pond
constexpr float HOLE_FLOOD_SEC = 6.f;        // a hole next to water floods itself after this long
constexpr int PLOT_WATER_REACH = 2;          // tilled soil this near water (tiles) waters itself         // plots one full watering can fills

// ---------------------------------------------------------------- domestic animals
enum DomKind { DA_CHICKEN, DA_COW, DA_SHEEP, DA_PIG, DA_GOAT, DA_DUCK, DA_RABBIT, DA_COUNT };
// A raised wild animal (a mount, mounts.hpp) is one more kind of homestead animal: it lives, eats and grows like the others but looks like its
// species (HAnimal::species / tier). It is not sold in the shop and does not breed. DA_COUNT stays the number of farm animals.
constexpr int DA_MOUNT = DA_COUNT;
static_assert(FARM_BABIES == DA_COUNT, "every farm animal has a baby item (items/items.hpp)");
struct DomDef {
    const char* name;
    const char* babyName;
    float growSec;       // seconds from newborn to adult
    int product;         // Mat the adult makes (-1 none)
    float prodSec;       // seconds per product (at full happiness)
    const char* prodName;
    int price;           // coins at the General Store (selling back pays half)
    bool small;          // fits the coop
    float speed;         // walking speed px/s
    float size;          // sprite scale (1 = a calf-sized cow)
    int feedA, feedB;    // favourite feed items (Mat) - any of the feed list works, these are loved
    int prodMax;         // how many pieces wait to be collected
};
static const DomDef DOMS[DA_COUNT + 1] = {
    {"CHICKEN", "CHICK", 150, M_EGG, 90, "EGG", 40, true, 20.f, 0.75f, M_WHEAT, M_CORN, 6},
    {"COW", "CALF", 420, M_MILK, 150, "MILK", 220, false, 17.f, 1.35f, M_HAY, M_CARROT, 4},
    {"SHEEP", "LAMB", 330, M_WOOL, 190, "WOOL", 120, false, 17.f, 1.0f, M_HAY, M_CABBAGE, 4},
    {"PIG", "PIGLET", 300, M_TRUFFLE, 360, "TRUFFLE", 130, false, 19.f, 1.0f, M_POTATO, M_PUMPKIN, 2},
    {"GOAT", "KID", 300, M_MILK, 170, "MILK", 110, false, 21.f, 0.95f, M_HAY, M_CARROT, 4},
    {"DUCK", "DUCKLING", 170, M_DUCKEGG, 110, "DUCK EGG", 50, true, 19.f, 0.7f, M_WHEAT, M_CABBAGE, 5},
    {"RABBIT", "KIT", 200, M_FUR, 240, "FUR", 45, true, 26.f, 0.55f, M_CARROT, M_CABBAGE, 3},
    {"MOUNT", "BABY", 420, -1, 0, nullptr, 0, false, 19.f, 1.0f, M_HAY, M_CARROT, 0},  // growSec / name come from the species
};
constexpr int H_MAX_ANIMALS = 26;
constexpr float FEED_FULL = 100.f, FEED_DECAY = 0.30f;  // fullness lost per second (a full belly lasts ~5 minutes)
constexpr float BREED_CD = 300.f;
constexpr float MOUNT_BREED_UPGRADE = 0.12f;  // chance that a raised foal is one rarity tier above its better parent

// ---------------------------------------------------------------- state
struct HPiece {
    int id = 0, kind = 0;
    int tx = 0, ty = 0, rot = 0;
    bool open = false;               // doors and gates
    bool rich = false;               // a plot on fertile (dark) soil grows faster
    int crop = -1, stage = 0;        // farm plots
    float growT = 0, water = 0;      // seconds into the current stage; 0..1 how wet the soil is
    Inventory inv{0, 1e9f};          // storage buildings
    int mrec = -1, mq = 0, mdone = 0;  // production machines (machines.hpp): the chosen recipe, batches still to make, batches finished and waiting
    float mt = 0;                      // seconds into the batch being made
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
    int species = -1, tier = 0;      // DA_MOUNT only: the wild species (A_*) and its rarity tier
    bool saddled = false;            // DA_MOUNT only: the one the hunter rides (Game::toggleMount)
    bool companion = false;          // DA_MOUNT only: the one that walks along with the hunter (companion.cpp)
    bool away = false;               // with the hunter outside the homestead (not simulated or drawn as a pen animal)
    int bond = 0;                    // kills the hunter made with it close by: sets its level
    bool ridden = false;             // currently carrying the hunter (not simulated or drawn as a pen animal)
    float growSec(const DomDef& d) const {
        const MountDef* m = kind == DA_MOUNT ? mountDef(species) : nullptr;
        return m ? m->growSec : d.growSec;
    }
    bool adult(const DomDef& d) const { return age >= growSec(d); }
    std::string label(const DomDef& d) const {  // "WOLF" / "BABY WOLF", "COW" / "CALF"
        if (kind == DA_MOUNT && species >= 0 && species < A_COUNT) return std::string(adult(d) ? "" : "BABY ") + ANIMAL[species].name;
        return adult(d) ? d.name : d.babyName;
    }
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
