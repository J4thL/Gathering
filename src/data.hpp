// data.hpp - static game design data (materials, weapons, animals, recipes, upgrades).
#pragma once
#include "gfx.hpp"

// Materials come from carcasses. The first seven are the original ones; the rest drop from the new species.
enum Mat { M_MEAT, M_HIDE, M_BONE, M_ORGAN, M_FUR, M_TUSK, M_ANTLER, M_FEATHER, M_HORN, M_SCALE, M_CLAW, M_COUNT };
[[maybe_unused]] static const char* MAT_NAME[M_COUNT] = {"RAW MEAT", "HIDE", "BONE", "ORGANS", "FUR", "TUSK", "ANTLER", "FEATHER", "HORN", "SCALE", "CLAW"};
static const int MAT_PRICE[M_COUNT] = {5, 9, 3, 7, 16, 22, 18, 6, 20, 14, 24};
// order in which a carcass gives up its materials while gathering
static const int GATHER_ORDER[M_COUNT] = {M_MEAT, M_HIDE, M_ORGAN, M_BONE, M_FUR, M_TUSK, M_ANTLER, M_FEATHER, M_SCALE, M_HORN, M_CLAW};

// Weapons. The first four are thrown (and picked up again); the bow and crossbow are launchers that spend ammo instead.
enum Wpn { W_FLINT, W_BONE, W_AXE, W_JAVELIN, W_BOW, W_CROSSBOW, W_COUNT };
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

// ---- resource nodes placed in the new biomes (harvesting arrives with the item system)
enum NodeKind { RN_CACTUS, RN_DESERT_HERB, RN_REED, RN_BOGWORT, RN_FROSTBLOOM, RN_ALPINE_HERB, RN_ORE, RN_COUNT };
[[maybe_unused]] static const char* NODE_NAME[RN_COUNT] = {"CACTUS PULP", "SAGEBRUSH", "MARSH REEDS", "BOGWORT", "FROSTBLOOM", "ALPINE HERB", "IRON ORE"};

// ---- crafting
enum RecipeKind { RK_WEAPON, RK_AMMO, RK_BANDAGE };
struct Recipe {
    int kind;   // what `out` indexes: a Wpn, an Ammo, or (RK_BANDAGE) nothing
    int out;
    int count;
    int mat[3], n[3];
};
// Weapons come first (the shop's CRAFT tab shows exactly these), then the field recipes that can be made anywhere.
static const Recipe RECIPES[] = {
    {RK_WEAPON, W_BONE, 4, {M_BONE, M_HIDE, -1}, {3, 1, 0}},
    {RK_WEAPON, W_AXE, 3, {M_BONE, M_HIDE, M_TUSK}, {4, 2, 1}},
    {RK_WEAPON, W_JAVELIN, 3, {M_BONE, M_ANTLER, M_FUR}, {3, 1, 1}},
    {RK_AMMO, AM_ARROW, 6, {M_BONE, M_FEATHER, -1}, {1, 2, 0}},
    {RK_AMMO, AM_BOLT, 4, {M_BONE, M_HORN, -1}, {2, 1, 0}},
    {RK_BANDAGE, 0, 2, {M_HIDE, M_ORGAN, -1}, {2, 1, 0}},
};
constexpr int RECIPE_N = sizeof(RECIPES) / sizeof(RECIPES[0]);
constexpr int RECIPE_WEAPONS = 3;  // the first three recipes are weapons (made at the shop)

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
static const int ARMOR_HP[3] = {100, 120, 145};
static const float ARMOR_RED[3] = {0.0f, 0.15f, 0.30f};

// Weapon safety net: the cheapest way to get weapons is the shop's "FLINT KNIFE X3" for 21 coins
// (see drawShop in game_render.cpp). If the hunter can't afford that and has no weapons left,
// the merchant lends FLINT_LOAN knives so the game can never soft-lock.
constexpr int FLINT_PACK_PRICE = 21;
constexpr int FLINT_LOAN = 3;

constexpr int BANDAGE_HEAL = 45;
constexpr int BANDAGE_MAX = 9;

#include "items.hpp"
