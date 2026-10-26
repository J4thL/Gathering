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

enum ItemKind { IK_MATERIAL, IK_THROWN, IK_LAUNCHER, IK_AMMO, IK_CONSUMABLE, IK_BACKPACK };
[[maybe_unused]] static const char* ITEM_KIND_NAME[] = {"MATERIAL", "THROWN WEAPON", "LAUNCHER", "AMMUNITION", "CONSUMABLE", "BACKPACK"};

// ---- id space: contiguous ranges, in the order of the legacy enums
constexpr int ITEM_MAT0 = 0;
constexpr int ITEM_WPN0 = ITEM_MAT0 + M_COUNT;
constexpr int ITEM_AMMO0 = ITEM_WPN0 + W_COUNT;
enum ConsumableId { C_BANDAGE, C_COUNT };
constexpr int ITEM_CONS0 = ITEM_AMMO0 + AM_COUNT;
enum PackId { PK_SATCHEL, PK_TRAPPER, PK_EXPLORER, PK_COUNT };
constexpr int ITEM_PACK0 = ITEM_CONS0 + C_COUNT;
constexpr int ITEM_COUNT = ITEM_PACK0 + PK_COUNT;

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

constexpr int STACK_MAT = 99, STACK_AMMO = 99, STACK_CONS = 20, STACK_WPN = 99;
// backpacks only add slots; they never change how many fit in a stack
constexpr int PACK_SLOTS[PK_COUNT] = {20, 30, 40};

inline const std::array<ItemDef, ITEM_COUNT>& itemTable() {
    static const std::array<ItemDef, ITEM_COUNT> T = [] {
        std::array<ItemDef, ITEM_COUNT> t{};
        for (int m = 0; m < M_COUNT; m++) t[itemMat(m)] = {MAT_NAME[m], IK_MATERIAL, STACK_MAT, MAT_PRICE[m], MAT_PRICE[m] >= 20 ? 2 : (MAT_PRICE[m] >= 12 ? 1 : 0), "TRADE GOOD FROM A CARCASS", m, 0};
        for (int w = 0; w < W_COUNT; w++)
            t[itemWpn(w)] = {WPN[w].name, isLauncher(w) ? IK_LAUNCHER : IK_THROWN, isLauncher(w) ? 1 : STACK_WPN, 0, 0, WPN[w].blurb, w, 0};
        for (int a = 0; a < AM_COUNT; a++) t[itemAmmo(a)] = {AMMO[a].name, IK_AMMO, STACK_AMMO, 1, 0, "SPENT WHEN FIRED, SOME CAN BE RECOVERED", a, 0};
        t[ITEM_CONS0 + C_BANDAGE] = {"BANDAGE", IK_CONSUMABLE, STACK_CONS, 0, 0, "HEALS 45 HP", C_BANDAGE, 0};
        static const char* pn[PK_COUNT] = {"HIDE SATCHEL", "TRAPPER PACK", "EXPLORER PACK"};
        for (int k = 0; k < PK_COUNT; k++) t[ITEM_PACK0 + k] = {pn[k], IK_BACKPACK, 1, 0, k, "MORE INVENTORY SLOTS", k, PACK_SLOTS[k]};
        return t;
    }();
    return T;
}
inline const ItemDef& item(int id) { return itemTable()[id]; }
inline bool itemValid(int id) { return id >= 0 && id < ITEM_COUNT; }
