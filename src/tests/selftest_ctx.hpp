// selftest_ctx.hpp - shared state of the `--selftest` suite, which is split by topic into selftest_*.cpp (run in order by selfTest()).
#pragma once
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <string>
#include "core/game.hpp"
#include "art/export_sprites.hpp"

struct SelfTest {
    Game& g;
    int bad = 0;
    const int MEAT = itemMat(M_MEAT), HIDE = itemMat(M_HIDE), BONE = itemMat(M_BONE);
    int R_BONE = -1, R_ARROW = -1, R_BOLT = -1, R_BANDAGE = -1;  // the original recipes, looked up once
    std::string savedBytes;  // the player's own save, put back when the suite ends
    bool hadSave = false;

    explicit SelfTest(Game& game) : g(game) {
        R_BONE = findRecipe(RK_WEAPON, W_BONE, M_BONE);
        R_ARROW = findRecipe(RK_AMMO, AM_ARROW, M_BONE);
        R_BOLT = findRecipe(RK_AMMO, AM_BOLT, M_BONE);
        R_BANDAGE = findRecipe(RK_BANDAGE, 0, M_HIDE);
        std::ifstream in(g.savePath(), std::ios::binary);
        if (in) {
            hadSave = true;
            savedBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
    }
    void check(bool ok, const char* what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
        if (!ok) bad++;
    }
    void fillWithJunk() {
        g.p.inv.clear();
        g.p.inv.maxWeight = 1e6f;  // "full" here means all 100 slots used; weight has its own checks below
        int slotsLeft = g.p.bagCap();
        for (int m = M_TUSK; m < M_CARCASS_N && slotsLeft > 0; m++, slotsLeft--) g.p.inv.add(itemMat(m), 50);
        for (int w = 0; w < W_COUNT && slotsLeft > 0; w++, slotsLeft--) g.p.inv.add(itemWpn(w), 1);
        for (int a = 0; a < AM_COUNT && slotsLeft > 0; a++, slotsLeft--) g.p.inv.add(itemAmmo(a), 10);
        g.p.inv.add(ITEM_CONS0 + C_BANDAGE, 2);
        slotsLeft--;
        // anything left: extra bows (each its own slot) to take the last slots
        while (slotsLeft-- > 0) g.p.inv.add(itemWpn(W_BOW), 1);
    }
    int findRecipe(int kind, int out, int firstMat) const {
        for (int r = 0; r < RECIPE_N; r++)
            if (RECIPES[r].kind == kind && RECIPES[r].out == out && RECIPES[r].mat[0] == firstMat) return r;
        return -1;
    }
    void writeFile(const std::string& text) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out << text;
    }
    void removeAll() {
        std::remove(g.savePath().c_str());
        std::remove((g.savePath() + ".v1.bak").c_str());
        std::remove((g.savePath() + ".tmp").c_str());
        std::remove((g.savePath() + ".bak").c_str());
    }
};

// each topic below runs against the same Game, in this order (later topics rely on the state the earlier ones leave)
void testInventoryClass(SelfTest& t);  // the Inventory class: stacking, slots, weight
void testGameRules(SelfTest& t);  // game rules: new hunter, full pack, hotbar, weapons, recipes
void testScreensAndSaves(SelfTest& t);  // inventory screen, crafting, saving and migration
void testWorldRules(SelfTest& t);  // gathering, structures, consumables, gear, maps and travel
void testSkillsAndTrophies(SelfTest& t);  // skill bonuses, skill XP, trophies and skills
void testTownAndFeedback(SelfTest& t);  // town NPCs, HUD feedback, wildlife and audio
