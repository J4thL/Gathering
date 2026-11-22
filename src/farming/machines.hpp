// machines.hpp - the Homestead's production machines (mill, dairy press, preserving kettle, smokehouse, bakery oven, sawmill, smelter).
//
// A machine turns what the hunter loads into something better, over time, while they are at the Homestead. It works on ONE recipe at a time:
// the hunter picks a recipe, loads batches from the pack (up to MACH_MAX_BATCH), the machine works through them and the finished goods wait
// in it until they are collected. Its state lives in the HPiece (mrec / mq / mdone / mt) and is saved with it. See homestead.cpp / homestead_ui.cpp.
#pragma once
#include "core/data.hpp"
#include "items/items.hpp"

constexpr int MACH_MAX_BATCH = 10;  // batches a machine holds (queued + finished)

// One recipe: up to two inputs per batch (Mat ids, -1 = none), and what a batch makes. `out` >= 0 is a Mat, `out` < 0 is a consumable (-2 - C_x).
struct MRecipe {
    int in[2], n[2];
    int out, outN;
    float sec;  // seconds per batch
};
struct MachineDef {
    int nrec;
    MRecipe rec[4];
};
constexpr int machOutItem(const MRecipe& r) { return r.out >= 0 ? itemMat(r.out) : itemCons(-2 - r.out); }
constexpr int MC(int c) { return -2 - c; }  // a consumable as a recipe output

// indexed by (piece kind - HP_MILL), see HPKind in homestead.hpp
static const MachineDef MACHINES[] = {
    // GRAIN MILL
    {2, {{{M_WHEAT, -1}, {3, 0}, M_FLOUR, 2, 20.f}, {{M_CORN, -1}, {3, 0}, M_FLOUR, 2, 20.f}}},
    // DAIRY PRESS
    {2, {{{M_MILK, -1}, {2, 0}, M_BUTTER, 1, 30.f}, {{M_MILK, -1}, {4, 0}, M_CHEESE, 1, 60.f}}},
    // PRESERVING KETTLE
    {4, {{{M_STRAWBERRY, -1}, {3, 0}, M_JAM, 1, 35.f}, {{M_BERRY, -1}, {4, 0}, M_JAM, 1, 35.f},
         {{M_CUCUMBER, -1}, {3, 0}, M_PICKLES, 1, 35.f}, {{M_CABBAGE, -1}, {3, 0}, M_PICKLES, 1, 35.f}}},
    // SMOKEHOUSE
    {2, {{{M_MEAT, -1}, {3, 0}, MC(C_COOKED), 3, 30.f}, {{M_MEAT, M_HERB}, {2, 1}, MC(C_JERKY), 2, 40.f}}},
    // BAKERY OVEN
    {3, {{{M_FLOUR, -1}, {2, 0}, MC(C_BREAD), 2, 30.f}, {{M_EGG, -1}, {2, 0}, MC(C_OMELET), 2, 20.f}, {{M_FLOUR, M_PUMPKIN}, {2, 1}, MC(C_PIE), 1, 50.f}}},
    // SAWMILL
    {2, {{{M_WOOD, -1}, {3, 0}, M_PLANK, 2, 25.f}, {{M_FIBER, -1}, {3, 0}, M_ROPE, 1, 20.f}}},
    // SMELTER
    {3, {{{M_COPPER, M_WOOD}, {2, 1}, M_COPPERBAR, 1, 30.f}, {{M_IRON, M_WOOD}, {2, 1}, M_IRONBAR, 1, 40.f}, {{M_GOLD, M_WOOD}, {2, 1}, M_GOLDBAR, 1, 60.f}}},
};
