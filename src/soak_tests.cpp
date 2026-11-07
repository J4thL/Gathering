// soak_tests.cpp - `wildwood --soak`: a randomised stress run of the real game code, headless.
// In every biome map a hunter with random movement, aiming, throwing and gathering is hunted by aggressive animals for 30 simulated
// seconds, and after EVERY update the state is checked for impossible values (NaN positions, health or stamina out of range, animals
// outside the map, broken stacks). It is a crash / sanity finder, not a balance test. SOAK_SEED=<n> repeats a run, SOAK_SECONDS=<n> sets the length.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include "game.hpp"

namespace {
bool okNum(float v) { return std::isfinite(v); }
}  // namespace

// Buildings and map changes through the real game code: every door in and out, saving / loading and dying inside a building, and a
// save made in each map. (Walking through the gates is in --worldcheck.)
static int transitTest(Game& g) {
    int bad = 0;
    auto check = [&](bool ok, const std::string& what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str());
        if (!ok) bad++;
    };
    auto run = [&](int n) {
        for (int i = 0; i < n; i++) {
            g.update(1.f / 60.f);
            g.in.lPressed = g.in.lReleased = g.in.ePressed = false;
        }
    };
    auto sane = [&]() {
        return std::isfinite(g.p.pos.x) && std::isfinite(g.p.pos.y) && g.p.pos.x >= 0 && g.p.pos.y >= 0 && g.p.pos.x <= MW * TS && g.p.pos.y <= MH * TS;
    };
    printf("transitions\n");
    for (int k = 0; k < BLD_COUNT; k++) {
        g.newGame();
        g.state = ST_PLAY;
        g.p.coins = 321;
        g.p.pos = V2(bldDoorX(k, COX), bldDoorY(k, COY));
        const size_t animals0 = g.animals.size(), structs0 = g.structs.size();
        const V2 door = g.p.pos;
        const bool seen = g.doorNear() == k;
        const bool in = g.enterBuilding(k);
        const bool spawnFree = !g.world.blocked(g.world.spawn.x, g.world.spawn.y, 6.f);
        check(seen && in && g.interior == k && spawnFree, std::string("the door of the ") + BUILDINGS[k].name + " is found, entering works and the hunter stands on free floor");
        run(60);
        check(g.interior == k && sane() && g.state == ST_PLAY, std::string("one second inside the ") + BUILDINGS[k].name + " is stable");
        // saving inside a building and loading it again must not strand the hunter in a room that is not there
        g.save();
        g.p.coins = 1;
        const bool ld = g.load();
        run(30);
        check(ld && g.interior == -1 && g.p.coins == 321 && sane() && !g.world.blocked(g.p.pos.x, g.p.pos.y, 4.f), std::string("a save made inside the ") + BUILDINGS[k].name + " loads outdoors on free ground");
        // back inside and out through the exit
        g.newGame();
        g.state = ST_PLAY;
        g.p.pos = door;
        g.enterBuilding(k);
        g.p.coins = 321;
        const bool out = g.exitBuilding();
        run(30);
        check(out && g.interior == -1 && g.world.mapId == B_HOME && dist(g.p.pos, door) < 1.f && g.animals.size() == animals0 && g.structs.size() == structs0 && g.p.coins == 321,
              std::string("leaving the ") + BUILDINGS[k].name + " puts the hunter back on the door step, with the village as it was");
        // knocked out inside: the hunter wakes up at the spawn point of the map they are in, which in a room is the room's entrance
        g.p.pos = door;
        g.enterBuilding(k);
        g.hurtPlayer(9999.f, g.p.pos + V2(10, 0), 0);
        const bool died = g.p.dead;
        run(600);
        check(died && !g.p.dead && g.p.hp > 0 && (g.interior == -1 || g.interior == k) && sane() && !g.world.blocked(g.p.pos.x, g.p.pos.y, 4.f),
              std::string("being knocked out inside the ") + BUILDINGS[k].name + " wakes the hunter up alive on free floor (at the room's entrance)");
    }
    for (int m = 0; m < B_COUNT; m++) {  // a save made in each map loads to a valid spot
        g.newGame();
        g.state = ST_PLAY;
        if (m != B_HOME && !g.travelTo(m, MAPS[m].entryTx, MAPS[m].entryTy, false)) { check(false, std::string("could not travel to ") + MAPS[m].name); continue; }
        g.p.coins = 777;
        g.save();
        g.p.coins = 1;
        const bool ld = g.load();
        run(60);
        check(ld && g.p.coins == 777 && sane() && !g.world.blocked(g.p.pos.x, g.p.pos.y, 4.f), std::string("a save made in ") + MAPS[m].name + " loads to a valid spot (map " + std::to_string(g.world.mapId) + ")");
    }
    return bad;
}

// Money loops: the flower shop sells cut flowers at twice their sell value, so no recipe may turn bought flowers into something that
// sells for more than they cost, and a service must refuse what the hunter cannot pay or carry.
static int economyTest(Game& g) {
    int bad = 0;
    auto check = [&](bool ok, const std::string& what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str());
        if (!ok) bad++;
    };
    printf("shops and money\n");
    bool loopFree = true;
    std::string culprit;
    for (int i = 0; i < RECIPE_N; i++) {
        const Recipe& R = RECIPES[i];
        if (R.kind != RK_ITEM) continue;
        int boughtCost = 0;
        bool usesFlower = false;
        for (int k = 0; k < 3; k++)
            if (R.mat[k] >= M_ROSE && R.mat[k] <= M_SUNFLOWER) { usesFlower = true; boughtCost += R.n[k] * MAT_PRICE[R.mat[k]] * 2; }
        if (!usesFlower) continue;
        const int value = item(R.out).price * R.count;
        if (value > boughtCost) { loopFree = false; culprit += std::string(culprit.empty() ? "" : "; ") + item(R.out).name + ": sells " + std::to_string(value) + ", flowers cost " + std::to_string(boughtCost); }
    }
    check(loopFree, "no recipe turns bought flowers into something worth more than they cost" + (culprit.empty() ? std::string() : " (" + culprit + ")"));

    g.newGame();
    g.state = ST_PLAY;
    // flower shop
    g.p.coins = 10;
    const int cost = MAT_PRICE[M_ROSE] * 2;
    const int roses0 = g.p.mat(M_ROSE);
    check(!g.buyFlower(M_ROSE, 1000) && g.p.coins == 10 && g.p.mat(M_ROSE) == roses0, "buying more flowers than the coins allow changes nothing");
    check(!g.buyFlower(M_ROSE, 0) && !g.buyFlower(M_ROSE, -3) && !g.buyFlower(M_BOUQUET, 1) && !g.buyFlower(M_WOOD, 1) && g.p.coins == 10, "zero, negative and non-flower purchases are refused");
    g.p.coins = cost * 3;
    check(g.buyFlower(M_ROSE, 3) && g.p.coins == 0 && g.p.mat(M_ROSE) == roses0 + 3, "buying three roses costs exactly three times the price");
    // a full pack refuses the purchase and keeps the coins
    g.p.coins = 100000;
    g.p.inv.addIgnoringWeight(itemMat(M_ROSE), 999 * 200);  // far over what 100 slots / the bag can carry
    const int coinsBefore = g.p.coins;
    const bool tooMany = g.buyFlower(M_TULIP, 1);
    check(!tooMany && g.p.coins == coinsBefore, "a full pack refuses the purchase and takes no coins");
    // selling pays exactly the listed value, never more than the pack holds, and nothing for nothing
    g.newGame();
    g.state = ST_PLAY;
    g.p.inv.addIgnoringWeight(itemMat(M_STONE), 7);
    const int c0 = g.p.coins;
    g.sellMat(M_STONE, 1000);
    check(g.p.coins == c0 + 7 * MAT_PRICE[M_STONE] && g.p.mat(M_STONE) == 0, "selling more than owned sells only what is in the pack");
    g.sellMat(M_STONE, 5);
    g.sellMat(M_STONE, -5);
    check(g.p.coins == c0 + 7 * MAT_PRICE[M_STONE], "selling nothing, or a negative amount, pays nothing");
    // upgrades: refused without the means, charged once, and capped at level 2
    g.newGame();
    g.state = ST_PLAY;
    g.p.coins = 0;
    check(!g.buyUpgrade(U_BOOTS) && g.p.up[U_BOOTS] == 0 && !g.buyUpgrade(U_BAG), "an upgrade without the coins and materials is refused, and bags cannot be bought");
    g.p.coins = 1000000;
    for (int m = 0; m < M_COUNT; m++) g.p.inv.addIgnoringWeight(itemMat(m), 50);
    for (int i = 0; i < 4; i++) g.buyUpgrade(U_BOOTS);
    check(g.p.up[U_BOOTS] == 2, "an upgrade stops at level 2 however often it is bought");
    // the salon
    g.newGame();
    g.state = ST_PLAY;
    g.p.coins = SALON_FEE - 1;
    g.openSalon();
    g.salonLook = g.p.look;
    g.salonLook.hair = (g.salonLook.hair + 1) % 4;
    const HunterLook before = g.p.look;
    check(!g.confirmSalon() && g.p.look == before && g.p.coins == SALON_FEE - 1, "the salon refuses a new look the hunter cannot pay for and charges nothing");
    g.p.coins = SALON_FEE + 5;
    check(g.confirmSalon() && g.p.coins == 5 && !(g.p.look == before), "the salon charges its fee once and applies the look");
    check(!g.confirmSalon() && g.p.coins == 5, "confirming again with no change is free and does nothing");
    return bad;
}

int soakTest(Game& g) {
    int bad = 0;
    auto check = [&](bool ok, const std::string& what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what.c_str());
        if (!ok) bad++;
    };
    const unsigned seed = getenv("SOAK_SEED") ? (unsigned)atoi(getenv("SOAK_SEED")) : 12345u;
    const int seconds = getenv("SOAK_SECONDS") ? std::max(1, atoi(getenv("SOAK_SECONDS"))) : 30;
    std::mt19937 rng(seed);
    auto rnd = [&](int n) { return (int)(rng() % (unsigned)n); };
    printf("soak: seed %u, %d simulated seconds per map\n", seed, seconds);

    for (int map = 0; map < B_COUNT; map++) {
        g.newGame();
        g.state = ST_PLAY;
        if (map != B_HOME && !g.travelTo(map, MAPS[map].entryTx, MAPS[map].entryTy, false)) {
            check(false, std::string("could not travel to ") + MAPS[map].name);
            continue;
        }
        g.dayT = (float)rnd(1500);  // some maps are played by day, some at night
        g.p.inv.addIgnoringWeight(itemWpn(W_FLINT), 300);
        g.p.hot[0] = itemWpn(W_FLINT);
        std::string firstBad;
        int badSteps = 0, kills0 = g.kills, deaths = 0, menus = 0;
        bool wasDead = false;
        int dirX = 0, dirY = 0, holdLeft = 0;
        const int steps = seconds * 60;
        for (int s = 0; s < steps; s++) {
            // keep a pack of aggressive (and a few shy) animals around the hunter
            int alive = 0;
            for (auto& a : g.animals) alive += a.dead ? 0 : 1;
            if (s % 300 == 0 || alive < 3)
                for (int n = 0; n < 4; n++) {
                    int kind = rnd(A_COUNT);
                    for (int tries = 0; tries < 12 && !ANIMAL[kind].aggro && rnd(3) != 0; tries++) kind = rnd(A_COUNT);
                    for (int tries = 0; tries < 20; tries++) {
                        const float ang = (rng() % 628) / 100.f, r = 40.f + (float)rnd(90);
                        const V2 at = g.p.pos + V2(cosf(ang) * r, sinf(ang) * r);
                        if (!g.world.blocked(at.x, at.y, 8.f)) { g.spawnAnimal(kind, at, 0, 0); break; }
                    }
                }
            // random input
            if (s % 30 == 0) {
                dirX = rnd(3) - 1;
                dirY = rnd(3) - 1;
                g.in.sprint = rnd(3) == 0;
            }
            g.in.left = dirX < 0; g.in.right = dirX > 0; g.in.up = dirY < 0; g.in.down = dirY > 0;
            const Animal* near = nullptr;
            float nd = 1e9f;
            for (auto& a : g.animals)
                if (!a.dead && dist(a.pos, g.p.pos) < nd) { nd = dist(a.pos, g.p.pos); near = &a; }
            if (near) { g.in.mx = near->pos.x - g.camX; g.in.my = near->pos.y - 8.f - g.camY; }
            if (holdLeft > 0) {
                if (--holdLeft == 0) { g.in.l = false; g.in.lReleased = true; }
            } else if (s % 40 == 20) {
                g.in.l = true; g.in.lPressed = true;
                holdLeft = 8 + rnd(25);
            }
            g.in.e = rnd(5) == 0;
            g.in.ePressed = g.in.e && rnd(2) == 0;
            g.update(1.f / 60.f);
            g.in.lPressed = g.in.lReleased = g.in.ePressed = false;
            if (s % 150 == 0) g.render();
            if (g.p.dead && !wasDead) deaths++;
            wasDead = g.p.dead;

            if (g.state != ST_PLAY) {  // random walking can enter a shop or building and open its menu: close it and carry on
                menus++;
                g.state = ST_PLAY;
            }
            // ---- invariants
            std::string why;
            if (!okNum(g.p.pos.x) || !okNum(g.p.pos.y)) why = "hunter position is not a number";
            else if (g.p.hp < 0.f || g.p.hp > g.p.maxhp + 0.01f || !okNum(g.p.hp)) why = "hunter health out of range";
            else if (g.p.stamina < -0.01f || g.p.stamina > 100.01f || !okNum(g.p.stamina)) why = "stamina out of range";
            else if (g.p.pos.x < 0.f || g.p.pos.y < 0.f || g.p.pos.x > MW * TS || g.p.pos.y > MH * TS) why = "hunter left the map";
            else if (g.animals.size() > 600) why = "animal list is growing without limit";
            else {
                for (auto& a : g.animals)
                    if (!okNum(a.pos.x) || !okNum(a.pos.y) || a.pos.x < -1.f || a.pos.y < -1.f || a.pos.x > MW * TS + 1.f || a.pos.y > MH * TS + 1.f) {
                        why = "an animal is outside the map or not a number (" + std::string(ANIMAL[a.kind].name) + ")";
                        break;
                    }
                if (why.empty() && (int)g.p.inv.slots.size() != INV_SLOTS) why = "pack size changed";
                if (why.empty())
                    for (auto& sl : g.p.inv.slots)
                        if (!sl.empty() && (sl.n < 1 || sl.n > MAX_STACK)) { why = "a pack stack is outside 1.." + std::to_string(MAX_STACK); break; }
            }
            if (!why.empty()) {
                if (firstBad.empty()) firstBad = why + " at step " + std::to_string(s);
                badSteps++;
            }
        }
        check(badSteps == 0, std::string(MAPS[map].name) + ": " + std::to_string(steps) + " steps (" + std::to_string(g.kills - kills0) + " kills, " +
                                 std::to_string(deaths) + " knock-outs, " + std::to_string(menus) + " menus closed), state sane" + (firstBad.empty() ? "" : " - FIRST PROBLEM: " + firstBad));
        g.in = Input();
    }
    bad += transitTest(g);
    bad += economyTest(g);
    printf("SOAK: %s (%d failed)\n", bad ? "FAILED" : "ok", bad);
    return bad;
}
