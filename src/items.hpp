// items.hpp - the central item registry.
//
// Every kind of thing a hunter can own has one ItemDef here: materials, thrown weapons, launchers, ammo,
// consumables and backpacks. The older per-category tables in data.hpp (MAT_NAME, WPN, AMMO ...) stay the
// source of the numbers; the registry is built from them, so there is exactly one list of "what items exist",
// one id space, and one place that answers name / stack size / price / category. The inventory (Stage 5)
// stores (item id, count) pairs and never needs to know which legacy table an item came from.
#pragma once
#include <array>
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
enum PackId { PK_BASIC, PK_SATCHEL, PK_TRAPPER, PK_EXPLORER, PK_COUNT };  // = the levels of the BACKPACK upgrade (the last is craft-only)
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
    int slots;        // backpacks: inventory slots they provide
};

constexpr int STACK_MAT = 99, STACK_AMMO = 99, STACK_CONS = 9, STACK_WPN = 30;  // bandages / thrown weapons keep their old carry limits
// backpacks only add slots; they never change how many fit in a stack
constexpr int PACK_SLOTS[PK_COUNT] = {20, 30, 40, 50};

inline const std::array<ItemDef, ITEM_COUNT>& itemTable() {
    static const std::array<ItemDef, ITEM_COUNT> T = [] {
        std::array<ItemDef, ITEM_COUNT> t{};
        for (int m = 0; m < M_COUNT; m++) t[itemMat(m)] = {MAT_NAME[m], IK_MATERIAL, STACK_MAT, MAT_PRICE[m], MAT_PRICE[m] >= 20 ? 2 : (MAT_PRICE[m] >= 12 ? 1 : 0), MAT_BLURB[m], m, 0};
        for (int w = 0; w < W_COUNT; w++)
            t[itemWpn(w)] = {WPN[w].name, isLauncher(w) ? IK_LAUNCHER : IK_THROWN, isLauncher(w) ? 1 : STACK_WPN, 0, 0, WPN[w].blurb, w, 0};
        for (int a = 0; a < AM_COUNT; a++) t[itemAmmo(a)] = {AMMO[a].name, IK_AMMO, STACK_AMMO, 1, 0, "SPENT WHEN FIRED, SOME CAN BE RECOVERED", a, 0};
        t[ITEM_CONS0 + C_BANDAGE] = {"BANDAGE", IK_CONSUMABLE, STACK_CONS, 0, 0, "HEALS 45 HP", C_BANDAGE, 0};
        t[ITEM_CONS0 + C_SALVE] = {"HERBAL SALVE", IK_CONSUMABLE, STACK_CONS, 0, 0, "HEALS 30 HP - MADE FROM WILD HERBS", C_SALVE, 0};
        t[ITEM_CONS0 + C_TONIC] = {"BOG TONIC", IK_CONSUMABLE, 5, 0, 1, "STRONG MEDICINE - HEALS 90 HP", C_TONIC, 0};
        t[ITEM_CONS0 + C_COOKED] = {"COOKED MEAT", IK_CONSUMABLE, 20, 0, 0, "HEALS 35 HP AND RESTORES STAMINA", C_COOKED, 0};
        t[ITEM_CONS0 + C_BREW] = {"WARM BREW", IK_CONSUMABLE, 5, 0, 1, "NO COLD DRAIN IN THE SNOW FOR 3 MINUTES", C_BREW, 0};
        t[ITEM_CONS0 + C_CACTUSWATER] = {"CACTUS WATER", IK_CONSUMABLE, STACK_CONS, 0, 0, "RESTORES STAMINA, NO HEAT DRAIN FOR 2 MINUTES", C_CACTUSWATER, 0};
        t[ITEM_CONS0 + C_POULTICE] = {"HEALING POULTICE", IK_CONSUMABLE, STACK_CONS, 0, 0, "HEALS 60 HP", C_POULTICE, 0};
        t[ITEM_CONS0 + C_MEDICINE] = {"HERBAL MEDICINE", IK_CONSUMABLE, 5, 0, 2, "STRONG MEDICINE - HEALS 120 HP", C_MEDICINE, 0};
        t[ITEM_CONS0 + C_JERKY] = {"DRIED MEAT", IK_CONSUMABLE, 20, 0, 0, "HEALS 20 HP AND RESTORES STAMINA", C_JERKY, 0};
        t[ITEM_CONS0 + C_STEW] = {"HUNTER'S STEW", IK_CONSUMABLE, 5, 0, 1, "HEALS 85 HP AND RESTORES STAMINA", C_STEW, 0};
        t[ITEM_CONS0 + C_VIGOR] = {"VELVET TONIC", IK_CONSUMABLE, 5, 0, 2, "HEALS 55 HP, FULL STAMINA, 15% FASTER FOR 90 SECONDS", C_VIGOR, 0};
        t[ITEM_CONS0 + C_NIGHTSIGHT] = {"NIGHTSIGHT TONIC", IK_CONSUMABLE, 5, 0, 2, "THE NIGHT LOOKS LIKE DUSK FOR 5 MINUTES", C_NIGHTSIGHT, 0};
        static const char* pn[PK_COUNT] = {"BASIC PACK", "HIDE SATCHEL", "TRAPPER PACK", "EXPLORER PACK"};
        for (int k = 0; k < PK_COUNT; k++) t[ITEM_PACK0 + k] = {pn[k], IK_BACKPACK, 1, 0, k, "MORE INVENTORY SLOTS", k, PACK_SLOTS[k]};
        for (int k = 0; k < TL_COUNT; k++) t[itemTool(k)] = {TOOLDEF[k].name, IK_TOOL, 1, 0, TOOLDEF[k].tier >= 3 ? 2 : 1, TOOLDEF[k].blurb, k, 0};
        t[itemPlace(PL_CAMPFIRE)] = {"CAMPFIRE", IK_PLACEABLE, 5, 0, 0, "SMELT, COOK AND KEEP WARM - USE TO PLACE", PL_CAMPFIRE, 0};
        t[itemPlace(PL_CHEST)] = {"STORAGE CHEST", IK_PLACEABLE, 5, 0, 0, "20 SLOTS OF STORAGE - USE TO PLACE", PL_CHEST, 0};
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

#include "recipes.hpp"  // the recipe table needs the item ids above
