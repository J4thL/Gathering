// selftest.cpp - `wildwood --selftest`: inventory, crafting, skills, saves, quests, animals and the rest of the game rules.
// The checks live in selftest_*.cpp, one file per topic; this runs them in order against one Game and restores the player's save.
#include "tests/selftest_ctx.hpp"

int selfTest(Game& g) {
    SelfTest t(g);
    g.craftAnywhere = true;  // the crafting rules are tested away from the buildings (the building rule has its own checks)
    testInventoryClass(t);
    testGameRules(t);
    testScreensAndSaves(t);
    testWorldRules(t);
    testSkillsAndTrophies(t);
    testTownAndFeedback(t);
    if (t.hadSave) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out << t.savedBytes;
    } else
        t.removeAll();
    printf("SELFTEST: %s (%d failed)\n", t.bad ? "FAILED" : "ok", t.bad);
    return t.bad;
}
