// recipes.hpp - every crafting recipe, as data. Included at the end of data.hpp (after the item registry), so it can name items.
//
// A recipe is: what it makes (kind + out + count), up to three ingredients (material, amount), a category (for the pack's filter
// chips) and a station (anywhere / beside a campfire / beside the merchant). Progression, roughly:
//   hands -> branches, stone, fibre, herbs        (no tool needed)
//   stone axe + pickaxe                            (made anywhere)
//   campfire -> rope, copper bars, copper tools    (smelting needs a campfire: the village fire or one you place)
//   merchant's forge -> iron bars, iron tools, iron spear, crossbow, scale mail, gilded tools
#pragma once
#include <string>
#include "data.hpp"

constexpr int RECIPE_WEAPONS = 3;  // the first three recipes are the merchant's weapons (they have their own page on the shop's CRAFT tab)

static const Recipe RECIPES[] = {
    // ---- the shop's CRAFT tab (must stay first, in this order)
    {RK_WEAPON, W_BONE, 4, {M_BONE, M_HIDE, -1}, {3, 1, 0}, RC_WEAPONS, STN_SHOP},
    {RK_WEAPON, W_AXE, 3, {M_BONE, M_HIDE, M_TUSK}, {4, 2, 1}, RC_WEAPONS, STN_SHOP},
    {RK_WEAPON, W_JAVELIN, 3, {M_BONE, M_ANTLER, M_FUR}, {3, 1, 1}, RC_WEAPONS, STN_SHOP},

    // ---- gathering tools: every tier needs the one before it
    {RK_ITEM, itemTool(TL_AXE1), 1, {M_BRANCH, M_STONE, M_FIBER}, {2, 2, 1}, RC_TOOLS, STN_ANY},
    {RK_ITEM, itemTool(TL_PICK1), 1, {M_BRANCH, M_STONE, M_FIBER}, {2, 3, 1}, RC_TOOLS, STN_ANY},
    {RK_ITEM, itemTool(TL_AXE2), 1, {M_WOOD, M_COPPERBAR, M_ROPE}, {2, 2, 1}, RC_TOOLS, STN_FIRE},
    {RK_ITEM, itemTool(TL_PICK2), 1, {M_WOOD, M_COPPERBAR, M_ROPE}, {2, 3, 1}, RC_TOOLS, STN_FIRE},
    {RK_ITEM, itemTool(TL_AXE3), 1, {M_WOOD, M_IRONBAR, M_ROPE}, {2, 2, 1}, RC_TOOLS, STN_SHOP},
    {RK_ITEM, itemTool(TL_PICK3), 1, {M_WOOD, M_IRONBAR, M_ROPE}, {2, 3, 1}, RC_TOOLS, STN_SHOP},
    {RK_ITEM, itemTool(TL_AXE4), 1, {M_WOOD, M_IRONBAR, M_GOLDBAR}, {2, 2, 1}, RC_TOOLS, STN_SHOP},
    {RK_ITEM, itemTool(TL_PICK4), 1, {M_WOOD, M_IRONBAR, M_GOLDBAR}, {2, 3, 2}, RC_TOOLS, STN_SHOP},

    // ---- weapons and ammunition made from what the land gives
    {RK_WEAPON, W_SPEAR, 2, {M_BRANCH, M_STONE, M_FIBER}, {2, 1, 1}, RC_WEAPONS, STN_ANY},
    {RK_WEAPON, W_IRONSPEAR, 2, {M_WOOD, M_IRONBAR, M_ROPE}, {1, 1, 1}, RC_WEAPONS, STN_SHOP},
    {RK_WEAPON, W_BOW, 1, {M_WOOD, M_ROPE, M_HIDE}, {2, 1, 1}, RC_WEAPONS, STN_ANY},
    {RK_WEAPON, W_LONGBOW, 1, {M_WOOD, M_ROPE, M_ANTLER}, {3, 2, 1}, RC_WEAPONS, STN_ANY},
    {RK_WEAPON, W_CROSSBOW, 1, {M_WOOD, M_IRONBAR, M_ROPE}, {3, 2, 2}, RC_WEAPONS, STN_SHOP},
    {RK_AMMO, AM_ARROW, 6, {M_BONE, M_FEATHER, -1}, {1, 2, 0}, RC_AMMO, STN_ANY},
    {RK_AMMO, AM_ARROW, 8, {M_BRANCH, M_STONE, M_FEATHER}, {2, 1, 1}, RC_AMMO, STN_ANY},
    {RK_AMMO, AM_BOLT, 4, {M_BONE, M_HORN, -1}, {2, 1, 0}, RC_AMMO, STN_ANY},
    {RK_AMMO, AM_BOLT, 8, {M_WOOD, M_IRONBAR, -1}, {1, 1, 0}, RC_AMMO, STN_SHOP},

    // ---- medicine and food
    {RK_BANDAGE, 0, 2, {M_HIDE, M_ORGAN, -1}, {2, 1, 0}, RC_MEDICINE, STN_ANY},
    {RK_ITEM, itemCons(C_SALVE), 2, {M_HERB, M_FIBER, -1}, {2, 1, 0}, RC_MEDICINE, STN_ANY},
    {RK_ITEM, itemCons(C_TONIC), 1, {M_BOGWORT, M_NIGHTSHADE, M_MUSHROOM}, {2, 1, 1}, RC_MEDICINE, STN_ANY},
    {RK_ITEM, itemCons(C_COOKED), 2, {M_MEAT, -1, -1}, {2, 0, 0}, RC_SURVIVAL, STN_FIRE},
    {RK_ITEM, itemCons(C_BREW), 1, {M_FROSTBLOOM, M_HERB, -1}, {2, 1, 0}, RC_SURVIVAL, STN_FIRE},
    {RK_ITEM, itemCons(C_CACTUSWATER), 2, {M_PULP, M_SAGE, -1}, {2, 1, 0}, RC_SURVIVAL, STN_ANY},

    // ---- survival utilities
    {RK_ITEM, itemPlace(PL_CAMPFIRE), 1, {M_WOOD, M_STONE, -1}, {4, 3, 0}, RC_SURVIVAL, STN_ANY},
    {RK_ITEM, itemPlace(PL_CHEST), 1, {M_WOOD, M_ROPE, -1}, {6, 2, 0}, RC_SURVIVAL, STN_ANY},

    // ---- processing: raw materials into components
    {RK_ITEM, itemMat(M_ROPE), 1, {M_FIBER, -1, -1}, {3, 0, 0}, RC_MATERIALS, STN_ANY},
    {RK_ITEM, itemMat(M_COPPERBAR), 1, {M_COPPER, M_WOOD, -1}, {2, 1, 0}, RC_MATERIALS, STN_FIRE},
    {RK_ITEM, itemMat(M_IRONBAR), 1, {M_IRON, M_WOOD, -1}, {2, 2, 0}, RC_MATERIALS, STN_FIRE},
    {RK_ITEM, itemMat(M_GOLDBAR), 1, {M_GOLD, M_WOOD, -1}, {3, 2, 0}, RC_MATERIALS, STN_FIRE},

    // ---- gear: worn at once (armor and boots replace the shop's coin-cost upgrades with a material-only route)
    {RK_GEAR, U_ARMOR, 1, {M_HIDE, M_FIBER, -1}, {5, 2, 0}, RC_ARMOR, STN_ANY},
    {RK_GEAR, U_ARMOR, 2, {M_FUR, M_HIDE, M_ROPE}, {4, 4, 1}, RC_ARMOR, STN_FIRE},
    {RK_GEAR, U_ARMOR, 3, {M_SCALE, M_HIDE, M_IRONBAR}, {6, 4, 2}, RC_ARMOR, STN_SHOP},
    {RK_GEAR, U_BOOTS, 1, {M_HIDE, M_FIBER, -1}, {3, 1, 0}, RC_ARMOR, STN_ANY},
    {RK_GEAR, U_BOOTS, 2, {M_FUR, M_HIDE, M_ROPE}, {3, 2, 1}, RC_ARMOR, STN_FIRE},
};
constexpr int RECIPE_N = sizeof(RECIPES) / sizeof(RECIPES[0]);

// the name a recipe's result goes by (gear has no item: it is the upgrade level's name)
inline std::string recipeName(const Recipe& R) {
    if (R.kind == RK_GEAR) {
        if (R.out == U_ARMOR && R.count == 3) return "SCALE MAIL";
        return UPS[R.out].lv[R.count - 1 < 1 ? R.count - 1 : 1].name;
    }
    return item(recipeItem(R)).name;
}
