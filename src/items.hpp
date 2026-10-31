// items.hpp - the central item registry.
//
// Every kind of thing a hunter can own has one ItemDef here: materials, thrown weapons, launchers, ammo,
// consumables and backpacks. The older per-category tables in data.hpp (MAT_NAME, WPN, AMMO ...) stay the
// source of the numbers; the registry is built from them, so there is exactly one list of "what items exist",
// one id space, and one place that answers name / stack size / price / category. The inventory (Stage 5)
// stores (item id, count) pairs and never needs to know which legacy table an item came from.
#pragma once
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include "data.hpp"

enum ItemKind { IK_MATERIAL, IK_THROWN, IK_LAUNCHER, IK_AMMO, IK_CONSUMABLE, IK_BACKPACK, IK_TOOL, IK_PLACEABLE, IK_CARRIED };
[[maybe_unused]] static const char* ITEM_KIND_NAME[] = {"MATERIAL", "THROWN WEAPON", "LAUNCHER", "AMMUNITION", "CONSUMABLE", "BACKPACK", "GATHERING TOOL", "PLACEABLE", "CARRIED GEAR"};

// ---- id space: contiguous ranges, in the order of the legacy enums
constexpr int ITEM_MAT0 = 0;
constexpr int ITEM_WPN0 = ITEM_MAT0 + M_COUNT;
constexpr int ITEM_AMMO0 = ITEM_WPN0 + W_COUNT;
enum ConsumableId { C_BANDAGE, C_SALVE, C_TONIC, C_COOKED, C_BREW, C_CACTUSWATER, C_POULTICE, C_MEDICINE, C_JERKY, C_STEW, C_VIGOR, C_NIGHTSIGHT, C_COUNT };
constexpr int ITEM_CONS0 = ITEM_AMMO0 + AM_COUNT;
enum PackId { PK_SATCHEL, PK_TRAPPER, PK_EXPLORER, PK_EXPEDITION, PK_COUNT };  // bags: crafted, worn in the bag slot, each raises the carrying-weight limit
constexpr int ITEM_PACK0 = ITEM_CONS0 + C_COUNT;
// gathering tools: axes cut trees, pickaxes break rock and ore; a higher tier is faster and gives more (see resources.hpp for what needs which tier)
enum ToolType { TT_AXE, TT_PICK, TT_COUNT };
enum ToolId { TL_AXE1, TL_AXE2, TL_AXE3, TL_AXE4, TL_PICK1, TL_PICK2, TL_PICK3, TL_PICK4, TL_COUNT };
struct ToolDef {
    const char* name;
    int type, tier;      // tier 1..4: stone, copper, iron, gilded
    float speed;         // swing speed multiplier
    float bonus;         // chance of one extra piece from a finished resource
    const char* blurb;
};
static const ToolDef TOOLDEF[TL_COUNT] = {
    {"STONE AXE", TT_AXE, 1, 1.0f, 0.00f, "CHOPS ANY TREE - SLOWLY"},
    {"COPPER AXE", TT_AXE, 2, 1.4f, 0.10f, "FASTER, A LITTLE MORE WOOD"},
    {"IRON AXE", TT_AXE, 3, 1.9f, 0.20f, "FAST AND STURDY"},
    {"GILDED AXE", TT_AXE, 4, 2.5f, 0.35f, "THE BEST AXE - GOLD-BANDED IRON"},
    {"STONE PICKAXE", TT_PICK, 1, 1.0f, 0.00f, "BREAKS STONE AND COPPER"},
    {"COPPER PICKAXE", TT_PICK, 2, 1.4f, 0.10f, "CAN CRACK IRON ORE"},
    {"IRON PICKAXE", TT_PICK, 3, 1.9f, 0.20f, "OPENS GOLD VEINS AND CRYSTAL"},
    {"GILDED PICKAXE", TT_PICK, 4, 2.5f, 0.35f, "THE BEST PICKAXE - FASTEST, MOST ORE"},
};
enum PlaceId { PL_CAMPFIRE, PL_CHEST, PL_COUNT };
constexpr int ITEM_TOOL0 = ITEM_PACK0 + PK_COUNT;
constexpr int ITEM_PLACE0 = ITEM_TOOL0 + TL_COUNT;
// Carried gear works while it is in the pack (like a tool: no equip step, it just has to be on you). Each takes a pack slot.
enum CarriedId { CG_WOLFCLOAK, CG_PARKA, CG_DESERTWRAPS, CG_VENOMWRAPS, CG_WADERS, CG_HARNESS, CG_QUIVER, CG_COUNT };
constexpr int ITEM_CARRY0 = ITEM_PLACE0 + PL_COUNT;
constexpr int ITEM_COUNT = ITEM_CARRY0 + CG_COUNT;
constexpr int itemCarry(int c) { return ITEM_CARRY0 + c; }
constexpr int itemTool(int t) { return ITEM_TOOL0 + t; }
constexpr int itemPlace(int k) { return ITEM_PLACE0 + k; }
constexpr int itemCons(int c) { return ITEM_CONS0 + c; }

constexpr int itemMat(int m) { return ITEM_MAT0 + m; }
constexpr int itemWpn(int w) { return ITEM_WPN0 + w; }
constexpr int itemAmmo(int a) { return ITEM_AMMO0 + a; }

struct ItemDef {
    std::string name;
    int kind;
    int stack;        // largest stack in one inventory slot
    int price;        // sell value in coins per piece (0 = cannot be sold)
    int rarity;       // 0 common .. 3 legendary, for display only
    std::string blurb;
    int ref;          // index into the legacy table (Mat / Wpn / Ammo / consumable / pack)
    int carry;        // bags: the carrying-weight limit (kg) they give while worn
    float weight = 0; // kg per piece (filled in below from the weight tables, so every item has exactly one weight)
};

// The pack always has INV_SLOTS slots and every stackable item stacks to MAX_STACK; what limits a hunter is how much they can CARRY.
// Bags never add slots or change stack sizes, they only raise the weight limit.
constexpr int INV_SLOTS = 100;
constexpr int MAX_STACK = 999;
constexpr int STACK_MAT = MAX_STACK, STACK_AMMO = MAX_STACK, STACK_CONS = MAX_STACK, STACK_WPN = MAX_STACK;
constexpr int BASE_CARRY_KG = 50;                              // with no bag
constexpr int PACK_CARRY_KG[PK_COUNT] = {100, 150, 200, 300};  // satchel, trapper, explorer, expedition
constexpr int itemPack(int k) { return ITEM_PACK0 + k; }

// ---- weights in kg, per piece: the one place item weights are defined
inline float matWeight(int m) {
    static const float W[M_COUNT] = {
        /*MEAT*/ 0.8f, /*HIDE*/ 1.5f, /*BONE*/ 0.5f, /*ORGAN*/ 0.4f, /*FUR*/ 1.0f, /*TUSK*/ 1.2f, /*ANTLER*/ 1.5f, /*FEATHER*/ 0.05f, /*HORN*/ 1.0f, /*SCALE*/ 0.4f, /*CLAW*/ 0.2f,
        /*WOOD*/ 1.0f, /*BRANCH*/ 0.5f, /*STONE*/ 2.0f, /*FIBER*/ 0.1f,
        /*HERB*/ 0.1f, /*MUSHROOM*/ 0.1f, /*SAGE*/ 0.1f, /*PULP*/ 0.2f, /*BOGWORT*/ 0.1f, /*NIGHTSHADE*/ 0.1f, /*FROSTBLOOM*/ 0.1f, /*ALPINE*/ 0.1f,
        /*COPPER*/ 2.5f, /*IRON*/ 3.0f, /*GOLD*/ 4.0f, /*AMBER*/ 0.3f, /*CRYSTAL*/ 0.5f,
        /*ROPE*/ 0.4f, /*COPPERBAR*/ 2.0f, /*IRONBAR*/ 2.5f, /*GOLDBAR*/ 3.5f,
        // trophies: small, exclusive pieces
        0.2f, 0.3f, 0.4f, 0.3f,  0.1f, 0.1f, 0.2f, 0.2f, 0.1f, 0.2f, 0.4f,  0.2f, 0.1f, 0.4f, 0.2f, 0.3f, 0.3f,  0.1f, 0.2f, 0.2f, 0.3f, 0.3f, 0.3f,  0.3f, 0.4f, 0.1f, 0.3f, 0.2f,
        /*FAT*/ 0.5f,
        /*PLANK*/ 1.5f, /*LEATHER*/ 1.0f, /*BONEFRAG*/ 0.2f, /*EXTRACT*/ 0.1f, /*GREASE*/ 0.3f,
        /*ROSE*/ 0.05f, /*TULIP*/ 0.05f, /*LAVENDER*/ 0.05f, /*SUNFLOWER*/ 0.1f, /*BOUQUET*/ 0.3f};
    return W[m];
}
inline float wpnWeight(int w) {
    static const float W[W_COUNT] = {0.3f, 0.5f, 2.0f, 1.5f, 1.5f, 3.5f, 2.0f, 3.0f, 2.0f, 0.6f, 2.5f, 2.0f, 3.5f, 1.5f, 2.5f, 2.5f, 3.5f};
    return W[w];
}

inline const std::array<ItemDef, ITEM_COUNT>& itemTable() {
    static const std::array<ItemDef, ITEM_COUNT> T = [] {
        std::array<ItemDef, ITEM_COUNT> t{};
        for (int m = 0; m < M_COUNT; m++) t[itemMat(m)] = {MAT_NAME[m], IK_MATERIAL, STACK_MAT, MAT_PRICE[m], MAT_PRICE[m] >= 20 ? 2 : (MAT_PRICE[m] >= 12 ? 1 : 0), MAT_BLURB[m], m, 0};
        for (int w = 0; w < W_COUNT; w++)
            t[itemWpn(w)] = {WPN[w].name, isLauncher(w) ? IK_LAUNCHER : IK_THROWN, isLauncher(w) ? 1 : STACK_WPN, 0, 0, WPN[w].blurb, w, 0};
        for (int a = 0; a < AM_COUNT; a++) t[itemAmmo(a)] = {AMMO[a].name, IK_AMMO, STACK_AMMO, 1, 0, "SPENT WHEN FIRED, SOME CAN BE RECOVERED", a, 0};
        t[ITEM_CONS0 + C_BANDAGE] = {"BANDAGE", IK_CONSUMABLE, STACK_CONS, 0, 0, "HEALS 45 HP", C_BANDAGE, 0};
        t[ITEM_CONS0 + C_SALVE] = {"HERBAL SALVE", IK_CONSUMABLE, STACK_CONS, 0, 0, "HEALS 30 HP - MADE FROM WILD HERBS", C_SALVE, 0};
        t[ITEM_CONS0 + C_TONIC] = {"BOG TONIC", IK_CONSUMABLE, STACK_CONS, 0, 1, "STRONG MEDICINE - HEALS 90 HP", C_TONIC, 0};
        t[ITEM_CONS0 + C_COOKED] = {"COOKED MEAT", IK_CONSUMABLE, STACK_CONS, 0, 0, "HEALS 35 HP AND RESTORES STAMINA", C_COOKED, 0};
        t[ITEM_CONS0 + C_BREW] = {"WARM BREW", IK_CONSUMABLE, STACK_CONS, 0, 1, "NO COLD DRAIN IN THE SNOW FOR 3 MINUTES", C_BREW, 0};
        t[ITEM_CONS0 + C_CACTUSWATER] = {"CACTUS WATER", IK_CONSUMABLE, STACK_CONS, 0, 0, "RESTORES STAMINA, NO HEAT DRAIN FOR 2 MINUTES", C_CACTUSWATER, 0};
        t[ITEM_CONS0 + C_POULTICE] = {"HEALING POULTICE", IK_CONSUMABLE, STACK_CONS, 0, 0, "HEALS 60 HP", C_POULTICE, 0};
        t[ITEM_CONS0 + C_MEDICINE] = {"HERBAL MEDICINE", IK_CONSUMABLE, STACK_CONS, 0, 2, "STRONG MEDICINE - HEALS 120 HP", C_MEDICINE, 0};
        t[ITEM_CONS0 + C_JERKY] = {"DRIED MEAT", IK_CONSUMABLE, STACK_CONS, 0, 0, "HEALS 20 HP AND RESTORES STAMINA", C_JERKY, 0};
        t[ITEM_CONS0 + C_STEW] = {"HUNTER'S STEW", IK_CONSUMABLE, STACK_CONS, 0, 1, "HEALS 85 HP AND RESTORES STAMINA", C_STEW, 0};
        t[ITEM_CONS0 + C_VIGOR] = {"VELVET TONIC", IK_CONSUMABLE, STACK_CONS, 0, 2, "HEALS 55 HP, FULL STAMINA, 15% FASTER FOR 90 SECONDS", C_VIGOR, 0};
        t[ITEM_CONS0 + C_NIGHTSIGHT] = {"NIGHTSIGHT TONIC", IK_CONSUMABLE, STACK_CONS, 0, 2, "THE NIGHT LOOKS LIKE DUSK FOR 5 MINUTES", C_NIGHTSIGHT, 0};
        static const char* pn[PK_COUNT] = {"HIDE SATCHEL", "TRAPPER PACK", "EXPLORER PACK", "EXPEDITION PACK"};
        static const float pw[PK_COUNT] = {1.0f, 1.5f, 2.0f, 2.5f};
        for (int k = 0; k < PK_COUNT; k++) {
            t[itemPack(k)] = {pn[k], IK_BACKPACK, 1, 0, k, "", k, PACK_CARRY_KG[k], pw[k]};
            t[itemPack(k)].blurb = "WEARABLE BAG - CARRY UP TO " + std::to_string(PACK_CARRY_KG[k]) + " KG";
        }
        for (int k = 0; k < TL_COUNT; k++) t[itemTool(k)] = {TOOLDEF[k].name, IK_TOOL, 1, 0, TOOLDEF[k].tier >= 3 ? 2 : 1, TOOLDEF[k].blurb, k, 0};
        t[itemPlace(PL_CAMPFIRE)] = {"CAMPFIRE", IK_PLACEABLE, STACK_CONS, 0, 0, "SMELT, COOK AND KEEP WARM - USE TO PLACE", PL_CAMPFIRE, 0};
        t[itemPlace(PL_CHEST)] = {"STORAGE CHEST", IK_PLACEABLE, STACK_CONS, 0, 0, "20 SLOTS OF STORAGE - USE TO PLACE", PL_CHEST, 0};
        struct CarriedText { const char* name; const char* blurb; int rarity; };
        static const CarriedText ct[CG_COUNT] = {
            {"WOLF PELT CLOAK", "CARRIED: HALVES THE COLD DRAIN IN THE SNOWFIELDS", 1},
            {"COLD-WEATHER PARKA", "CARRIED: NO COLD DRAIN IN THE SNOWFIELDS", 2},
            {"DESERT PROTECTION GEAR", "CARRIED: 70% LESS HEAT DRAIN, IMMUNE TO THORNS", 1},
            {"POISON-RESISTANT WRAPS", "CARRIED: IMMUNE TO POISON PLANTS, -35% SNAKE BITES", 2},
            {"BOG WADERS", "CARRIED: MUD AND MURK NO LONGER SLOW YOU", 1},
            {"CLIMBER'S HARNESS", "CARRIED: 60% LESS THIN-AIR DRAIN IN THE MOUNTAINS", 1},
            {"REINFORCED QUIVER", "CARRIED: HALF AS MANY ARROWS AND BOLTS ARE LOST", 1},
        };
        for (int k = 0; k < CG_COUNT; k++) t[itemCarry(k)] = {ct[k].name, IK_CARRIED, 1, 0, ct[k].rarity, ct[k].blurb, k, 0};
        // weights: one table per category (materials / weapons above); the rest by kind
        for (int m = 0; m < M_COUNT; m++) t[itemMat(m)].weight = matWeight(m);
        for (int w = 0; w < W_COUNT; w++) t[itemWpn(w)].weight = wpnWeight(w);
        for (int a = 0; a < AM_COUNT; a++) t[itemAmmo(a)].weight = 0.05f;
        static const float cw[C_COUNT] = {0.1f, 0.2f, 0.4f, 0.5f, 0.5f, 0.4f, 0.2f, 0.3f, 0.3f, 0.6f, 0.3f, 0.3f};
        for (int c = 0; c < C_COUNT; c++) t[ITEM_CONS0 + c].weight = cw[c];
        for (int k = 0; k < TL_COUNT; k++) t[itemTool(k)].weight = TOOLDEF[k].type == TT_AXE ? 2.0f + 0.25f * TOOLDEF[k].tier : 2.5f + 0.25f * TOOLDEF[k].tier;
        t[itemPlace(PL_CAMPFIRE)].weight = 4.0f;
        t[itemPlace(PL_CHEST)].weight = 8.0f;
        for (int k = 0; k < CG_COUNT; k++) t[itemCarry(k)].weight = k == CG_QUIVER ? 1.0f : 1.5f;
        return t;
    }();
    return T;
}
inline const ItemDef& item(int id) { return itemTable()[id]; }
inline bool itemValid(int id) { return id >= 0 && id < ITEM_COUNT; }

// ---- recipes in terms of items
inline int recipeItem(const Recipe& R) {
    switch (R.kind) {
        case RK_WEAPON: return itemWpn(R.out);
        case RK_AMMO: return itemAmmo(R.out);
        case RK_ITEM: return R.out;
        case RK_GEAR: return -1;  // worn at once, no item
        default: return ITEM_CONS0 + C_BANDAGE;
    }
}

// "85 KG" / "12.5 KG" style number for weights (one decimal only when needed)
inline std::string kgNum(float kg) {
    char b[24];
    if (kg < 0) kg = 0;
    if (fabsf(kg - roundf(kg)) < 0.05f) snprintf(b, sizeof b, "%d", (int)roundf(kg));
    else snprintf(b, sizeof b, "%.1f", kg);
    return b;
}

// ---- the hotbar: 8 quick-access slots, each accepting one kind of item. Only items the hunter has equipped there show up on it.
constexpr int HOT_N = 8;
enum HotKind { HK_WEAPON, HK_HEAL, HK_FOOD, HK_BUFF, HK_UTIL };
constexpr int HOT_KIND[HOT_N] = {HK_WEAPON, HK_WEAPON, HK_WEAPON, HK_HEAL, HK_HEAL, HK_FOOD, HK_BUFF, HK_UTIL};
[[maybe_unused]] static const char* HOT_KIND_NAME[] = {"WEAPON", "HEALING", "FOOD", "BUFF", "UTILITY"};
// which hotbar kind a consumable belongs to
inline int consumableHotKind(int c) {
    switch (c) {
        case C_COOKED: case C_JERKY: case C_STEW: return HK_FOOD;
        case C_BREW: case C_CACTUSWATER: case C_VIGOR: case C_NIGHTSIGHT: return HK_BUFF;
        default: return HK_HEAL;
    }
}
// can this item go in a hotbar slot of that kind? (weapons, consumables and placeables only; the UTILITY slot takes anything usable but weapons)
inline bool hotAccepts(int kind, int itemId) {
    if (!itemValid(itemId)) return false;
    const ItemDef& D = item(itemId);
    switch (D.kind) {
        case IK_THROWN: case IK_LAUNCHER: return kind == HK_WEAPON;
        case IK_CONSUMABLE: return kind == HK_UTIL || kind == consumableHotKind(D.ref);
        case IK_PLACEABLE: return kind == HK_UTIL;
        default: return false;
    }
}

#include "recipes.hpp"  // the recipe table needs the item ids above
