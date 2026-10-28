// data.hpp - static game design data (materials, weapons, animals, recipes, upgrades).
#pragma once
#include "gfx.hpp"

// Materials come from carcasses. The first seven are the original ones; the rest drop from the new species.
// The materials after M_CLAW come from the land (trees, rocks, plants, ore - see resources.hpp) or are processed from those.
enum Mat {
    M_MEAT, M_HIDE, M_BONE, M_ORGAN, M_FUR, M_TUSK, M_ANTLER, M_FEATHER, M_HORN, M_SCALE, M_CLAW,                   // carcass materials
    M_WOOD, M_BRANCH, M_STONE, M_FIBER,                                                                              // timber and stone
    M_HERB, M_MUSHROOM, M_SAGE, M_PULP, M_BOGWORT, M_NIGHTSHADE, M_FROSTBLOOM, M_ALPINE,                             // plants
    M_COPPER, M_IRON, M_GOLD, M_AMBER, M_CRYSTAL,                                                                    // ore and rare finds
    M_ROPE, M_COPPERBAR, M_IRONBAR, M_GOLDBAR,                                                                       // processed components
    M_COUNT
};
constexpr int M_CARCASS_N = M_CLAW + 1;  // the materials a carcass can hold
[[maybe_unused]] static const char* MAT_NAME[M_COUNT] = {
    "RAW MEAT", "HIDE", "BONE", "ORGANS", "FUR", "TUSK", "ANTLER", "FEATHER", "HORN", "SCALE", "CLAW",
    "WOOD", "BRANCH", "STONE", "FIBER",
    "WILD HERB", "MUSHROOM", "SAGE", "CACTUS PULP", "BOGWORT", "NIGHTSHADE", "FROSTBLOOM", "ALPINE HERB",
    "COPPER ORE", "IRON ORE", "GOLD ORE", "BOG AMBER", "FROST CRYSTAL",
    "ROPE", "COPPER BAR", "IRON BAR", "GOLD BAR"};
static const int MAT_PRICE[M_COUNT] = {5, 9, 3, 7, 16, 22, 18, 6, 20, 14, 24,
                                       4, 2, 3, 3,
                                       5, 6, 6, 5, 8, 12, 9, 8,
                                       9, 15, 38, 34, 36,
                                       9, 24, 40, 95};
[[maybe_unused]] static const char* MAT_BLURB[M_COUNT] = {
    "TRADE GOOD FROM A CARCASS", "TRADE GOOD FROM A CARCASS", "TRADE GOOD FROM A CARCASS", "TRADE GOOD FROM A CARCASS", "TRADE GOOD FROM A CARCASS",
    "TRADE GOOD FROM A CARCASS", "TRADE GOOD FROM A CARCASS", "TRADE GOOD FROM A CARCASS", "TRADE GOOD FROM A CARCASS", "TRADE GOOD FROM A CARCASS",
    "TRADE GOOD FROM A CARCASS",
    "CHOPPED TIMBER - TOOLS, FUEL, BUILDING", "SMALL STICKS FOR HANDLES AND SHAFTS", "ROUGH STONE FOR TOOLS AND FIRE RINGS", "PLANT FIBRE - TWIST IT INTO ROPE",
    "COMMON HEALING HERB OF THE FOREST", "FOREST MUSHROOM - MEDICINE AND FOOD", "DESERT HERB - A FAST-ACTING TONIC", "WATERY CACTUS FLESH - RESTORES STAMINA",
    "SWAMP HERB - THE BASE OF STRONG MEDICINE", "POISONOUS - HANDLE WITH CARE, DILUTE FOR MEDICINE", "COLD-HARDY BLOOM - WARMING BREWS", "MOUNTAIN HERB - SOOTHING AND LIGHT",
    "SOFT ORE - SMELT INTO COPPER BARS", "HARD ORE - SMELT INTO IRON BARS", "RARE ORE - SMELT INTO GOLD BARS", "RARE FOSSIL RESIN FROM THE BOG",
    "RARE ICE CRYSTAL - PRIZED BY TRADERS",
    "TWISTED FIBRE - BOWSTRINGS, LASHING, TOOLS", "SMELTED AT A CAMPFIRE - COPPER TOOLS", "SMELTED AT A CAMPFIRE - IRON TOOLS AND WEAPONS", "PURE GOLD - GILDED TOOLS, WORTH A FORTUNE"};
// order in which a carcass gives up its materials while gathering (land materials never appear in a carcass; they sit at the end)
static const int GATHER_ORDER[M_COUNT] = {M_MEAT, M_HIDE, M_ORGAN, M_BONE, M_FUR, M_TUSK, M_ANTLER, M_FEATHER, M_SCALE, M_HORN, M_CLAW,
                                          M_WOOD, M_BRANCH, M_STONE, M_FIBER, M_HERB, M_MUSHROOM, M_SAGE, M_PULP, M_BOGWORT, M_NIGHTSHADE,
                                          M_FROSTBLOOM, M_ALPINE, M_COPPER, M_IRON, M_GOLD, M_AMBER, M_CRYSTAL, M_ROPE, M_COPPERBAR, M_IRONBAR, M_GOLDBAR};

// Weapons. The first four are thrown (and picked up again); the bow and crossbow are launchers that spend ammo instead.
enum Wpn { W_FLINT, W_BONE, W_AXE, W_JAVELIN, W_BOW, W_CROSSBOW, W_SPEAR, W_IRONSPEAR, W_LONGBOW, W_COUNT };
enum Ammo { AM_ARROW, AM_BOLT, AM_COUNT };
struct WpnDef {
    const char* name;
    float dmg, speed, range, cd, brk;
    const char* blurb;
    int ammo = -1;       // -1: thrown weapon; otherwise the ammo type a launcher fires
    float draw = 0.f;    // seconds to draw a full shot (0 = fires on click; cd is then the reload time)
};
static const WpnDef WPN[W_COUNT] = {
    {"FLINT KNIFE", 10, 250, 150, 0.30f, 0.12f, "CHEAP AND LIGHT"},
    {"BONE KNIFE", 15, 275, 175, 0.28f, 0.08f, "FASTER, SHARPER"},
    {"HAND AXE", 24, 225, 135, 0.50f, 0.06f, "HEAVY HITTER"},
    {"JAVELIN", 34, 320, 235, 0.70f, 0.08f, "LONG RANGE KILLER"},
    {"HUNTING BOW", 30, 430, 330, 0.45f, 0.f, "HOLD TO DRAW - USES ARROWS", AM_ARROW, 0.85f},
    {"CROSSBOW", 46, 560, 360, 1.60f, 0.f, "SLOW RELOAD, HITS HARD - USES BOLTS", AM_BOLT, 0.f},
    {"WOOD SPEAR", 28, 300, 210, 0.60f, 0.10f, "CHEAP, THROWN, EASY TO MAKE"},
    {"IRON SPEAR", 42, 340, 250, 0.65f, 0.05f, "HEAVY IRON TIP, RARELY BREAKS"},
    {"LONGBOW", 38, 470, 380, 0.45f, 0.f, "LONG DRAW, LONG RANGE - USES ARROWS", AM_ARROW, 1.0f},
};
struct AmmoDef {
    const char* name;
    int price;       // coins per piece at the shop
    int pack;        // pieces per shop pack
    float lost;      // chance that a shot that hit is broken instead of dropping to the ground
};
static const AmmoDef AMMO[AM_COUNT] = {{"ARROW", 2, 10, 0.35f}, {"BOLT", 3, 6, 0.30f}};
constexpr int WPN_MAX = 30;
constexpr int AMMO_MAX = 60;
constexpr int BOW_PRICE = 90, CROSSBOW_PRICE = 220;
inline bool isLauncher(int w) { return WPN[w].ammo >= 0; }

// Animal species, rarity tiers and variant looks live in fauna.hpp (stats are data; behaviour flags drive the AI).
#include "fauna.hpp"

// ---- biomes (the original map is B_HOME; the four new regions surround it)
enum Biome { B_HOME, B_DESERT, B_SWAMP, B_SNOW, B_MOUNTAIN, B_COUNT };
struct BiomeDef {
    const char* name;
    const char* climate;  // short HUD tag for the biome's stamina/movement effect ("" = none)
    u32 col;              // accent colour (banner, minimap markers)
};
static const BiomeDef BIOME[B_COUNT] = {
    {"WILDWOOD", "", HEX(0x86d85e)},
    {"DESERT", "HEAT", HEX(0xf0c060)},
    {"SWAMP", "MUD", HEX(0x8cc06a)},
    {"SNOWFIELDS", "COLD", HEX(0xbfe3ff)},
    {"MOUNTAINS", "THIN AIR", HEX(0xc4c6d4)},
};

// ---- resource nodes placed in the world (what each one gives is the table in resources.hpp)
enum NodeKind {
    RN_CACTUS, RN_DESERT_HERB, RN_REED, RN_BOGWORT, RN_FROSTBLOOM, RN_ALPINE_HERB, RN_ORE,  // the original seven (RN_ORE = iron)
    RN_HERB, RN_MUSHROOM, RN_BRANCH, RN_FLAX,                                                  // forest
    RN_DRYWOOD, RN_COPPER,                                                                      // desert (copper also in the mountains)
    RN_NIGHTSHADE, RN_AMBER,                                                                    // swamp
    RN_GOLD,                                                                                    // mountain rare vein
    RN_COUNT
};
[[maybe_unused]] static const char* NODE_NAME[RN_COUNT] = {"PRICKLY PEAR", "SAGEBRUSH", "MARSH REEDS", "BOGWORT", "FROSTBLOOM", "ALPINE HERB", "IRON ORE",
                                                           "WILD HERB", "MUSHROOMS", "FALLEN BRANCH", "WILD FLAX", "DRY WOOD", "COPPER ORE",
                                                           "NIGHTSHADE", "BOG AMBER", "GOLD VEIN"};

// ---- crafting
// RK_ITEM: `out` is a registry item id (tools, consumables, placeables, processed materials).
// RK_GEAR: `out` is an Up index and `count` the level it brings the hunter to (armor, boots) - there is no item, the gear is worn at once.
enum RecipeKind { RK_WEAPON, RK_AMMO, RK_BANDAGE, RK_ITEM, RK_GEAR };
enum RecipeCat { RC_TOOLS, RC_WEAPONS, RC_AMMO, RC_MEDICINE, RC_SURVIVAL, RC_MATERIALS, RC_ARMOR, RC_COUNT };
[[maybe_unused]] static const char* RECIPE_CAT_NAME[RC_COUNT] = {"TOOLS", "WEAPONS", "AMMO", "MEDICINE", "SURVIVAL", "PARTS", "GEAR"};
enum Station { STN_ANY, STN_FIRE, STN_SHOP };  // where a recipe can be made: anywhere, beside a campfire, beside the merchant's forge
[[maybe_unused]] static const char* STATION_NAME[3] = {"ANYWHERE", "AT A CAMPFIRE", "AT THE SHOP"};
struct Recipe {
    int kind;   // what `out` indexes: a Wpn, an Ammo, or (RK_BANDAGE) nothing
    int out;
    int count;
    int mat[3], n[3];
    int cat = RC_WEAPONS;
    int station = STN_ANY;
};

// ---- gear upgrades (two tiers each)
enum Up { U_TOOL, U_BAG, U_BOOTS, U_ARMOR, U_COUNT };
struct UpLevel {
    const char* name;
    const char* desc;
    int coins;
    int m1, n1, m2, n2;
};
struct UpDef {
    const char* title;
    UpLevel lv[2];
};
static const UpDef UPS[U_COUNT] = {
    {"SKINNING KNIFE",
     {{"BONE SKINNER", "GATHER 1.6X FASTER +25% BONUS", 60, M_BONE, 4, M_HIDE, 2},
      {"STEEL CLEAVER", "GATHER 2.4X FASTER +50% BONUS", 200, M_BONE, 6, M_TUSK, 2}}},
    {"BACKPACK",
     {{"HIDE SATCHEL", "30 INVENTORY SLOTS", 70, M_HIDE, 4, -1, 0},
      {"TRAPPER PACK", "40 INVENTORY SLOTS", 220, M_HIDE, 6, M_FUR, 3}}},
    {"BOOTS",
     {{"HIDE BOOTS", "MOVE 12% FASTER", 60, M_HIDE, 3, -1, 0}, {"FUR BOOTS", "MOVE 25% FASTER", 160, M_FUR, 2, M_HIDE, 3}}},
    {"ARMOR",
     {{"HIDE VEST", "+20 HP, -15% DAMAGE", 60, M_HIDE, 5, -1, 0},
      {"FUR COAT", "+45 HP, -30% DAMAGE", 180, M_FUR, 4, M_HIDE, 4}}},
};
static const float TOOL_SPEED[3] = {1.0f, 1.6f, 2.4f};
static const float TOOL_BONUS[3] = {0.0f, 0.25f, 0.5f};
static const float BOOT_MUL[3] = {1.0f, 1.12f, 1.25f};
static const int ARMOR_HP[4] = {100, 120, 145, 175};  // level 3 (scale mail) can only be crafted
static const float ARMOR_RED[4] = {0.0f, 0.15f, 0.30f, 0.42f};

// Weapon safety net: the cheapest way to get weapons is the shop's "FLINT KNIFE X3" for 21 coins
// (see drawShop in game_render.cpp). If the hunter can't afford that and has no weapons left,
// the merchant lends FLINT_LOAN knives so the game can never soft-lock.
constexpr int FLINT_PACK_PRICE = 21;
constexpr int FLINT_LOAN = 3;

constexpr int BANDAGE_HEAL = 45;
constexpr int BANDAGE_MAX = 9;

#include "items.hpp"
