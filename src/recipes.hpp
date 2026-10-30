// recipes.hpp - every crafting recipe, as data. Included at the end of data.hpp (after the item registry), so it can name items.
//
// A recipe is: what it makes (kind + out + count), up to three ingredients (material, amount), a category (for the pack's filter
// chips), a station (anywhere / beside a campfire / beside the merchant), and the skill that makes it plus the level it needs
// (the recipes that existed before skills were added need level 1). XP for making one comes from recipeXp(). Progression, roughly:
//   hands -> branches, stone, fibre, herbs        (no tool needed)
//   stone axe + pickaxe                            (made anywhere)
//   campfire -> rope, copper bars, copper tools    (smelting needs a campfire: the village fire or one you place)
//   merchant's forge -> iron bars, iron tools, iron spear, crossbow, scale mail, gilded tools
#pragma once
#include <algorithm>
#include <string>
#include "data.hpp"

constexpr int RECIPE_WEAPONS = 3;  // the first three recipes are the merchant's weapons (they have their own page on the shop's CRAFT tab)

static const Recipe RECIPES[] = {
    // ---- the shop's CRAFT tab (must stay first, in this order)
    {RK_WEAPON, W_BONE, 4, {M_BONE, M_HIDE, -1}, {3, 1, 0}, RC_WEAPONS, STN_SHOP, SK_CRAFTING, 1},
    {RK_WEAPON, W_AXE, 3, {M_BONE, M_HIDE, M_TUSK}, {4, 2, 1}, RC_WEAPONS, STN_SHOP, SK_CRAFTING, 1},
    {RK_WEAPON, W_JAVELIN, 3, {M_BONE, M_ANTLER, M_FUR}, {3, 1, 1}, RC_WEAPONS, STN_SHOP, SK_CRAFTING, 1},

    // ---- gathering tools: every tier needs the one before it
    {RK_ITEM, itemTool(TL_AXE1), 1, {M_BRANCH, M_STONE, M_FIBER}, {2, 2, 1}, RC_TOOLS, STN_ANY, SK_CRAFTING, 1},
    {RK_ITEM, itemTool(TL_PICK1), 1, {M_BRANCH, M_STONE, M_FIBER}, {2, 3, 1}, RC_TOOLS, STN_ANY, SK_CRAFTING, 1},
    {RK_ITEM, itemTool(TL_AXE2), 1, {M_WOOD, M_COPPERBAR, M_ROPE}, {2, 2, 1}, RC_TOOLS, STN_FIRE, SK_SMITHING, 1},
    {RK_ITEM, itemTool(TL_PICK2), 1, {M_WOOD, M_COPPERBAR, M_ROPE}, {2, 3, 1}, RC_TOOLS, STN_FIRE, SK_SMITHING, 1},
    {RK_ITEM, itemTool(TL_AXE3), 1, {M_WOOD, M_IRONBAR, M_ROPE}, {2, 2, 1}, RC_TOOLS, STN_SHOP, SK_SMITHING, 1},
    {RK_ITEM, itemTool(TL_PICK3), 1, {M_WOOD, M_IRONBAR, M_ROPE}, {2, 3, 1}, RC_TOOLS, STN_SHOP, SK_SMITHING, 1},
    {RK_ITEM, itemTool(TL_AXE4), 1, {M_WOOD, M_IRONBAR, M_GOLDBAR}, {2, 2, 1}, RC_TOOLS, STN_SHOP, SK_SMITHING, 1},
    {RK_ITEM, itemTool(TL_PICK4), 1, {M_WOOD, M_IRONBAR, M_GOLDBAR}, {2, 3, 2}, RC_TOOLS, STN_SHOP, SK_SMITHING, 1},

    // ---- weapons and ammunition made from what the land gives
    {RK_WEAPON, W_SPEAR, 2, {M_BRANCH, M_STONE, M_FIBER}, {2, 1, 1}, RC_WEAPONS, STN_ANY, SK_CRAFTING, 1},
    {RK_WEAPON, W_IRONSPEAR, 2, {M_WOOD, M_IRONBAR, M_ROPE}, {1, 1, 1}, RC_WEAPONS, STN_SHOP, SK_SMITHING, 1},
    {RK_WEAPON, W_BOW, 1, {M_WOOD, M_ROPE, M_HIDE}, {2, 1, 1}, RC_WEAPONS, STN_ANY, SK_CRAFTING, 1},
    {RK_WEAPON, W_LONGBOW, 1, {M_WOOD, M_ROPE, M_ANTLER}, {3, 2, 1}, RC_WEAPONS, STN_ANY, SK_CRAFTING, 1},
    {RK_WEAPON, W_CROSSBOW, 1, {M_WOOD, M_IRONBAR, M_ROPE}, {3, 2, 2}, RC_WEAPONS, STN_SHOP, SK_SMITHING, 1},
    {RK_AMMO, AM_ARROW, 6, {M_BONE, M_FEATHER, -1}, {1, 2, 0}, RC_AMMO, STN_ANY, SK_CRAFTING, 1},
    {RK_AMMO, AM_ARROW, 8, {M_BRANCH, M_STONE, M_FEATHER}, {2, 1, 1}, RC_AMMO, STN_ANY, SK_CRAFTING, 1},
    {RK_AMMO, AM_BOLT, 4, {M_BONE, M_HORN, -1}, {2, 1, 0}, RC_AMMO, STN_ANY, SK_CRAFTING, 1},
    {RK_AMMO, AM_BOLT, 8, {M_WOOD, M_IRONBAR, -1}, {1, 1, 0}, RC_AMMO, STN_SHOP, SK_SMITHING, 1},

    // ---- medicine and food
    {RK_BANDAGE, 0, 2, {M_HIDE, M_ORGAN, -1}, {2, 1, 0}, RC_MEDICINE, STN_ANY, SK_ALCHEMY, 1},
    {RK_ITEM, itemCons(C_SALVE), 2, {M_HERB, M_FIBER, -1}, {2, 1, 0}, RC_MEDICINE, STN_ANY, SK_ALCHEMY, 1},
    {RK_ITEM, itemCons(C_TONIC), 1, {M_BOGWORT, M_NIGHTSHADE, M_MUSHROOM}, {2, 1, 1}, RC_MEDICINE, STN_ANY, SK_ALCHEMY, 1},
    {RK_ITEM, itemCons(C_COOKED), 2, {M_MEAT, -1, -1}, {2, 0, 0}, RC_SURVIVAL, STN_FIRE, SK_COOKING, 1},
    {RK_ITEM, itemCons(C_BREW), 1, {M_FROSTBLOOM, M_HERB, -1}, {2, 1, 0}, RC_SURVIVAL, STN_FIRE, SK_ALCHEMY, 1},
    {RK_ITEM, itemCons(C_CACTUSWATER), 2, {M_PULP, M_SAGE, -1}, {2, 1, 0}, RC_SURVIVAL, STN_ANY, SK_ALCHEMY, 1},

    // ---- survival utilities
    {RK_ITEM, itemPlace(PL_CAMPFIRE), 1, {M_WOOD, M_STONE, -1}, {4, 3, 0}, RC_SURVIVAL, STN_ANY, SK_BUILDING, 1},
    {RK_ITEM, itemPlace(PL_CHEST), 1, {M_WOOD, M_ROPE, -1}, {6, 2, 0}, RC_SURVIVAL, STN_ANY, SK_BUILDING, 1},

    // ---- processing: raw materials into components
    {RK_ITEM, itemMat(M_ROPE), 1, {M_FIBER, -1, -1}, {3, 0, 0}, RC_MATERIALS, STN_ANY, SK_CRAFTING, 1},
    {RK_ITEM, itemMat(M_COPPERBAR), 1, {M_COPPER, M_WOOD, -1}, {2, 1, 0}, RC_MATERIALS, STN_FIRE, SK_SMITHING, 1},
    {RK_ITEM, itemMat(M_IRONBAR), 1, {M_IRON, M_WOOD, -1}, {2, 2, 0}, RC_MATERIALS, STN_FIRE, SK_SMITHING, 1},
    {RK_ITEM, itemMat(M_GOLDBAR), 1, {M_GOLD, M_WOOD, -1}, {3, 2, 0}, RC_MATERIALS, STN_FIRE, SK_SMITHING, 1},

    // ---- gear: worn at once (armor and boots replace the shop's coin-cost upgrades with a material-only route)
    {RK_GEAR, U_ARMOR, 1, {M_HIDE, M_FIBER, -1}, {5, 2, 0}, RC_ARMOR, STN_ANY, SK_LEATHER, 1},
    {RK_GEAR, U_ARMOR, 2, {M_FUR, M_HIDE, M_ROPE}, {4, 4, 1}, RC_ARMOR, STN_FIRE, SK_LEATHER, 1},
    {RK_GEAR, U_ARMOR, 3, {M_SCALE, M_HIDE, M_IRONBAR}, {6, 4, 2}, RC_ARMOR, STN_SHOP, SK_LEATHER, 1},
    {RK_GEAR, U_BOOTS, 1, {M_HIDE, M_FIBER, -1}, {3, 1, 0}, RC_ARMOR, STN_ANY, SK_LEATHER, 1},
    {RK_GEAR, U_BOOTS, 2, {M_FUR, M_HIDE, M_ROPE}, {3, 2, 1}, RC_ARMOR, STN_FIRE, SK_LEATHER, 1},
    // higher tiers: leatherwork-gated, built from processed leather and the animals' exclusive trophies
    {RK_GEAR, U_BOOTS, 3, {M_LEATHER, M_RABBITFOOT, M_SNOWFOOT}, {3, 2, 2}, RC_ARMOR, STN_FIRE, SK_LEATHER, 18},
    {RK_GEAR, U_ARMOR, 4, {M_LEATHER, M_GREASE, M_IRONBAR}, {6, 2, 2}, RC_ARMOR, STN_SHOP, SK_LEATHER, 30},
    {RK_GEAR, U_ARMOR, 5, {M_LEATHER, M_FUR, M_CLAW}, {5, 6, 3}, RC_ARMOR, STN_SHOP, SK_LEATHER, 45},
    // bags: crafted, then worn (equip from the pack); each one only raises the carrying-weight limit
    {RK_ITEM, itemPack(PK_SATCHEL), 1, {M_HIDE, M_FIBER, -1}, {4, 2, 0}, RC_ARMOR, STN_ANY, SK_LEATHER, 1},
    {RK_ITEM, itemPack(PK_TRAPPER), 1, {M_HIDE, M_FUR, M_ROPE}, {6, 3, 1}, RC_ARMOR, STN_FIRE, SK_LEATHER, 10},
    {RK_ITEM, itemPack(PK_EXPLORER), 1, {M_LEATHER, M_ROPE, M_MARMOTPELT}, {8, 4, 3}, RC_ARMOR, STN_FIRE, SK_LEATHER, 35},
    {RK_ITEM, itemPack(PK_EXPEDITION), 1, {M_LEATHER, M_ROPE, M_RAMFLEECE}, {12, 6, 4}, RC_ARMOR, STN_SHOP, SK_LEATHER, 45},
    // carried gear: it works while it is in the pack
    {RK_ITEM, itemCarry(CG_QUIVER), 1, {M_LEATHER, M_BOARBRISTLE, M_ROPE}, {2, 2, 1}, RC_ARMOR, STN_ANY, SK_LEATHER, 8},
    {RK_ITEM, itemCarry(CG_WOLFCLOAK), 1, {M_FUR, M_WOLFFANG, M_FOXBRUSH}, {5, 1, 1}, RC_ARMOR, STN_FIRE, SK_LEATHER, 14},
    {RK_ITEM, itemCarry(CG_DESERTWRAPS), 1, {M_LEATHER, M_FENNECEAR, M_HARETAIL}, {3, 2, 2}, RC_ARMOR, STN_FIRE, SK_LEATHER, 20},
    {RK_ITEM, itemCarry(CG_WADERS), 1, {M_LEATHER, M_GREASE, M_BOGHOOF}, {3, 2, 2}, RC_ARMOR, STN_FIRE, SK_LEATHER, 22},
    {RK_ITEM, itemCarry(CG_PARKA), 1, {M_LEATHER, M_RAMFLEECE, M_DUCKDOWN}, {3, 2, 2}, RC_ARMOR, STN_FIRE, SK_LEATHER, 24},
    {RK_ITEM, itemCarry(CG_HARNESS), 1, {M_LEATHER, M_GOATBEARD, M_ROPE}, {3, 2, 2}, RC_ARMOR, STN_FIRE, SK_LEATHER, 26},
    {RK_ITEM, itemCarry(CG_VENOMWRAPS), 1, {M_LEATHER, M_FROGGLAND, M_RATTLE}, {3, 2, 1}, RC_ARMOR, STN_FIRE, SK_LEATHER, 28},

    // ---- processing, part 2: sawn boards, tanned leather, crushed bone, extracts and rendered fat (these feed everything below)
    {RK_ITEM, itemMat(M_PLANK), 2, {M_WOOD, -1, -1}, {2, 0, 0}, RC_MATERIALS, STN_ANY, SK_CRAFTING, 1},
    {RK_ITEM, itemMat(M_BONEFRAG), 6, {M_BONE, M_STONE, -1}, {2, 1, 0}, RC_MATERIALS, STN_ANY, SK_CRAFTING, 1},
    {RK_ITEM, itemMat(M_GREASE), 2, {M_FAT, -1, -1}, {3, 0, 0}, RC_MATERIALS, STN_FIRE, SK_COOKING, 1},
    {RK_ITEM, itemMat(M_LEATHER), 2, {M_HIDE, M_GREASE, -1}, {3, 1, 0}, RC_MATERIALS, STN_FIRE, SK_LEATHER, 1},
    {RK_ITEM, itemMat(M_EXTRACT), 1, {M_HERB, -1, -1}, {3, 0, 0}, RC_MATERIALS, STN_FIRE, SK_ALCHEMY, 1},
    {RK_ITEM, itemMat(M_EXTRACT), 2, {M_ALPINE, M_SAGE, -1}, {2, 1, 0}, RC_MATERIALS, STN_FIRE, SK_ALCHEMY, 8},

    // ---- medicine and food, part 2 (extracts and trophies make the strong ones)
    {RK_ITEM, itemCons(C_POULTICE), 2, {M_EXTRACT, M_FIBER, -1}, {1, 1, 0}, RC_MEDICINE, STN_ANY, SK_ALCHEMY, 3},
    {RK_ITEM, itemCons(C_POULTICE), 4, {M_EXTRACT, M_FIBER, M_LIZARDTAIL}, {1, 1, 1}, RC_MEDICINE, STN_ANY, SK_ALCHEMY, 6},
    {RK_ITEM, itemCons(C_MEDICINE), 1, {M_EXTRACT, M_BEARGALL, -1}, {2, 1, 0}, RC_MEDICINE, STN_FIRE, SK_ALCHEMY, 15},
    {RK_ITEM, itemCons(C_VIGOR), 1, {M_VELVET, M_EXTRACT, -1}, {1, 2, 0}, RC_MEDICINE, STN_FIRE, SK_ALCHEMY, 20},
    {RK_ITEM, itemCons(C_NIGHTSIGHT), 1, {M_PANTHEREYE, M_EXTRACT, M_NIGHTSHADE}, {1, 1, 1}, RC_MEDICINE, STN_FIRE, SK_ALCHEMY, 25},
    {RK_ITEM, itemCons(C_JERKY), 4, {M_MEAT, M_FAT, -1}, {4, 1, 0}, RC_SURVIVAL, STN_FIRE, SK_COOKING, 3},
    {RK_ITEM, itemCons(C_STEW), 2, {M_MEAT, M_MUSHROOM, M_MARROW}, {3, 2, 1}, RC_SURVIVAL, STN_FIRE, SK_COOKING, 12},

    // ---- trophy weapons: every trophy is spent on something that does a job (see the audit in main.cpp --selftest)
    {RK_WEAPON, W_FANGDAGGER, 2, {M_COYOTEFANG, M_BONEFRAG, M_FIBER}, {1, 2, 1}, RC_WEAPONS, STN_ANY, SK_CRAFTING, 4},
    {RK_WEAPON, W_FANGDAGGER, 3, {M_WOLFFANG, M_BONEFRAG, M_FIBER}, {1, 2, 1}, RC_WEAPONS, STN_ANY, SK_CRAFTING, 6},
    {RK_WEAPON, W_RSPEAR, 1, {M_PLANK, M_TUSK, M_BOARBRISTLE}, {2, 1, 1}, RC_WEAPONS, STN_ANY, SK_CRAFTING, 10},
    {RK_WEAPON, W_RECURVE, 1, {M_PLANK, M_HORN, M_DEERSINEW}, {2, 1, 2}, RC_WEAPONS, STN_ANY, SK_CRAFTING, 12},
    {RK_WEAPON, W_HVYAXE, 2, {M_IRONBAR, M_PLANK, M_GATORTOOTH}, {2, 1, 1}, RC_WEAPONS, STN_SHOP, SK_SMITHING, 15},
    {RK_WEAPON, W_BOGSPEAR, 2, {M_PLANK, M_VENOMSAC, M_FROGGLAND}, {2, 1, 1}, RC_WEAPONS, STN_SHOP, SK_CRAFTING, 18},
    {RK_WEAPON, W_SUNJAV, 2, {M_PLANK, M_ROADPLUME, M_LIONMANE}, {2, 2, 1}, RC_WEAPONS, STN_SHOP, SK_CRAFTING, 20},
    {RK_WEAPON, W_HARPOON, 2, {M_PLANK, M_BONEFRAG, M_LYNXTUFT}, {2, 3, 1}, RC_WEAPONS, STN_SHOP, SK_CRAFTING, 22},
    {RK_WEAPON, W_CLIFFAXE, 2, {M_IRONBAR, M_PLANK, M_COUGARFANG}, {1, 1, 1}, RC_WEAPONS, STN_SHOP, SK_SMITHING, 25},
    {RK_AMMO, AM_ARROW, 14, {M_BONEFRAG, M_GOLDPLUME, M_BRANCH}, {3, 1, 2}, RC_AMMO, STN_ANY, SK_CRAFTING, 14},
};
constexpr int RECIPE_N = sizeof(RECIPES) / sizeof(RECIPES[0]);

// XP for making a recipe once: a fifth of what its ingredients are worth (so a bar of iron pays more than a rope), at least 2.
inline int recipeXp(const Recipe& R) {
    int v = 0;
    for (int i = 0; i < 3; i++)
        if (R.mat[i] >= 0) v += R.n[i] * MAT_PRICE[R.mat[i]];
    return std::max(2, v / 5);
}

// the name a recipe's result goes by (gear has no item: it is the upgrade level's name)
inline std::string recipeName(const Recipe& R) {
    if (R.kind == RK_GEAR) {
        return gearName(R.out, R.count);
    }
    return item(recipeItem(R)).name;
}
