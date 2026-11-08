// weapon_tests.cpp - `wildwood --weapons`: registry consistency, bow / crossbow behaviour, ammo and throwing (moved out of main.cpp).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <string>
#include "game.hpp"

// ---- weapon / item verification (flag: --weapons): registry consistency, bow and crossbow behaviour, drops, save round trip
// test helper: put a weapon on the hotbar (weapons can only be used when equipped) and take it in hand
static void wield(Game& g, int w) {
    if (!g.p.equippedWpn(w)) {
        for (int i = 0; i < HOT_N; i++)
            if (g.p.hot[i] < 0) {
                g.p.hot[i] = itemWpn(w);
                break;
            }
        if (!g.p.equippedWpn(w)) g.p.hot[2] = itemWpn(w);
    }
    g.p.sel = w;
}

int weaponTest(Game& g) {
    int bad = 0;
    // the save round trip below writes the save file: keep the player's real one safe and put it back afterwards
    std::string savedBytes;
    bool hadSave = false;
    {
        std::ifstream in(g.savePath(), std::ios::binary);
        if (in) {
            hadSave = true;
            savedBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
    }
    auto check = [&](bool ok, const char* what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
        if (!ok) bad++;
    };
    // --- registry
    {
        std::set<std::string> names;
        bool ids = true, stacks = true, prices = true, refs = true;
        for (int i = 0; i < ITEM_COUNT; i++) {
            const ItemDef& d = item(i);
            names.insert(d.name);
            if (d.name.empty()) ids = false;
            if (d.stack <= 0) stacks = false;
            if (d.kind == IK_MATERIAL && d.price != MAT_PRICE[d.ref]) prices = false;
            if (d.kind == IK_MATERIAL && (itemMat(d.ref) != i || d.stack != MAX_STACK)) refs = false;
            if (d.kind == IK_LAUNCHER && d.stack != 1) refs = false;
            if (d.kind == IK_THROWN && itemWpn(d.ref) != i) refs = false;
            if (d.kind == IK_AMMO && itemAmmo(d.ref) != i) refs = false;
        }
        printf("item registry: %d items\n", ITEM_COUNT);
        check(ids && (int)names.size() == ITEM_COUNT, "every item has a unique, non-empty name");
        check(stacks, "every stack size is positive");
        check(prices, "material prices in the registry match the price table");
        check(refs, "ids map back to their legacy tables; materials stack to 999; launchers do not stack");
        check(item(itemPack(0)).carry == 100 && item(itemPack(3)).carry == 300 && item(itemPack(1)).kind == IK_BACKPACK, "bags are items that raise the carrying limit");
        check(!isLauncher(W_FLINT) && !isLauncher(W_JAVELIN) && isLauncher(W_BOW) && isLauncher(W_CROSSBOW), "only the bow and crossbow are launchers");
    }
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    g.animals.clear();
    g.items.clear();
    g.p.hp = g.p.maxhp = 1e6f;
    auto step = [&](int n) {
        for (int i = 0; i < n; i++) {
            g.update(1.f / 60.f);
            g.in.lPressed = g.in.lReleased = g.in.ePressed = false;
        }
    };
    // spawn a sturdy, unaware deer 150 px to the right and aim at it
    // find an open lane (no props or water for 260 px to the right) so obstacles don't get in the way of the test
    V2 lane = g.world.spawn;
    for (int ty = COY + 10; ty < COY + CH - 10; ty++) {
        bool found = false;
        for (int tx = COX + 12; tx < COX + CW - 30 && !found; tx++) {
            bool clear = true;
            for (float x = 0; x <= 260 && clear; x += 8)
                for (float dy : {-12.f, 0.f, 9.f, 14.f}) clear = clear && !g.world.blocked(tx * TS + x, ty * TS + dy, 7);
            if (clear) {
                lane = V2(tx * TS + 8.f, ty * TS + 8.f);
                found = true;
            }
        }
        if (found) break;
    }
    auto target = [&]() -> Animal* {
        g.animals.clear();
        g.p.pos = lane;
        g.setCam(0, true);
        Animal* a = g.spawnAnimal(A_DEER, g.p.pos + V2(150, 0), 0, 0);
        a->maxhp = a->hp = 100000;
        a->st = AS_REST;
        a->stT = 1e9f;
        a->idleT = 0;
        g.in.mx = a->pos.x - g.camX;
        g.in.my = a->pos.y - a->bodyH - g.camY;
        return a;
    };
    // hold the button for `secs`, then release. Returns damage dealt to the target.
    auto shoot = [&](float secs) -> int {
        Animal* a = target();
        int hp0 = a->hp;
        g.in.l = true;
        g.in.lPressed = true;
        step(1);
        int n = (int)(secs * 60);
        if (n > 1) step(n - 1);
        g.in.l = false;
        g.in.lReleased = true;
        step(1);
        step(40);  // flight
        return hp0 - g.animals[0].hp;
    };
    g.p.cd = 0;
    // --- bow (set WEAPON_SHOTS=dir to also write two screenshots: bow mid-draw and an arrow in flight)
    if (const char* sd = getenv("WEAPON_SHOTS")) {
        g.p.inv.set(itemWpn(W_BOW), 1);
        wield(g, W_BOW);
        g.p.inv.set(itemAmmo(AM_ARROW), 12);
        g.p.inv.set(itemWpn(W_CROSSBOW), 1);
        g.p.inv.set(itemAmmo(AM_BOLT), 6);
        Animal* a = target();
        (void)a;
        g.p.cd = 0;
        g.in.l = true; g.in.lPressed = true; step(1); step(35);
        g.render();
        g.saveCanvas((std::string(sd) + "/bow_draw.bmp").c_str());
        g.in.l = false; g.in.lReleased = true; step(1); step(4);
        g.render();
        g.saveCanvas((std::string(sd) + "/bow_flight.bmp").c_str());
        step(60);
        wield(g, W_CROSSBOW);
        g.p.cd = 0;
        g.in.l = true; g.in.lPressed = true; step(1); g.in.l = false; g.in.lReleased = true; step(3);
        g.render();
        g.saveCanvas((std::string(sd) + "/crossbow_flight.bmp").c_str());
        g.items.clear();
    }
    g.p.inv.set(itemWpn(W_BOW), 1);
    wield(g, W_BOW);
    g.p.inv.set(itemAmmo(AM_ARROW), 200);
    long full = 0, tap = 0;
    int fullHits = 0, tapHits = 0;
    const int N = 40;
    for (int i = 0; i < N; i++) {
        g.p.inv.set(itemAmmo(AM_ARROW), 50);
        g.p.cd = 0;
        int d = shoot(1.0f);
        full += d;
        fullHits += d > 0;
        g.p.cd = 0;
        g.p.inv.set(itemAmmo(AM_ARROW), 50);
        d = shoot(0.1f);
        tap += d;
        tapHits += d > 0;
    }
    printf("bow: full draw avg %.1f dmg (%d/%d hit), quick tap avg %.1f dmg (%d/%d hit)\n", (double)full / N, fullHits, N, (double)tap / N, tapHits, N);
    check(fullHits == N, "a full draw reaches and hits a target 150px away every time");
    check((double)full / N > (double)tap / N * 1.4, "a full draw hits clearly harder than a quick tap");
    // ammo use
    g.p.inv.set(itemAmmo(AM_ARROW), 5);
    g.p.cd = 0;
    shoot(1.0f);
    check(g.p.ammo(AM_ARROW) == 4, "one shot costs exactly one arrow");
    check(g.p.wpn(W_BOW) == 1, "the bow itself is not used up");
    // recovery: arrows that hit either break or drop; walking over a dropped one gives it back
    g.p.inv.set(itemAmmo(AM_ARROW), 20);
    int fired = 0, recovered = 0;
    for (int i = 0; i < 30; i++) {
        g.items.clear();
        g.p.cd = 0;
        int before = g.p.ammo(AM_ARROW);
        shoot(1.0f);
        fired++;
        if (!g.items.empty()) {
            g.p.pos = g.items[0].pos;
            step(2);
            if (g.p.ammo(AM_ARROW) == before) recovered++;
        }
    }
    printf("arrows: %d fired at a target, %d recovered by walking over them (expected about 65%%)\n", fired, recovered);
    check(recovered > 8 && recovered < fired, "some arrows are recovered and some break");
    // no ammo
    g.p.inv.set(itemAmmo(AM_ARROW), 0);
    g.p.cd = 0;
    int dmg0 = shoot(1.0f);
    check(dmg0 == 0 && g.p.ammo(AM_ARROW) == 0 && g.projs.empty(), "no arrows: the bow does not fire");
    // --- crossbow
    g.p.inv.set(itemWpn(W_CROSSBOW), 1);
    wield(g, W_CROSSBOW);
    g.p.inv.set(itemAmmo(AM_BOLT), 10);
    g.p.cd = 0;
    int cd1 = shoot(0.05f);
    check(cd1 > 0 && g.p.ammo(AM_BOLT) == 9, "the crossbow fires at once on click and uses one bolt");
    {
        Animal* a = target();
        (void)a;
        g.p.cd = 0;
        g.in.l = true; g.in.lPressed = true; step(1); g.in.l = false; g.in.lReleased = true; step(1);
        int afterFirst = g.p.ammo(AM_BOLT);
        step(30);  // 0.5 s later: still reloading
        g.in.l = true; g.in.lPressed = true; step(1); g.in.l = false; g.in.lReleased = true; step(1);
        check(g.p.ammo(AM_BOLT) == afterFirst, "no second shot while the crossbow reloads (0.5 s)");
        step(75);  // past the 1.6 s reload
        g.in.l = true; g.in.lPressed = true; step(1); g.in.l = false; g.in.lReleased = true; step(1);
        check(g.p.ammo(AM_BOLT) == afterFirst - 1, "it fires again once reloaded (about 1.6 s)");
    }
    long cb = 0;
    for (int i = 0; i < 20; i++) {
        g.p.inv.set(itemAmmo(AM_BOLT), 10);
        g.p.cd = 0;
        cb += shoot(0.05f);
    }
    printf("crossbow avg %.1f dmg vs bow full draw %.1f, javelin base %d\n", (double)cb / 20, (double)full / N, (int)WPN[W_JAVELIN].dmg);
    check((double)cb / 20 > (double)full / N, "a crossbow bolt hits harder than a full-draw arrow");
    // --- thrown weapons are unchanged
    wield(g, W_JAVELIN);
    g.p.inv.set(itemWpn(W_JAVELIN), 3);
    g.p.cd = 0;
    int jd = shoot(0.5f);
    check(jd > 0 && g.p.wpn(W_JAVELIN) == 2, "a javelin is still thrown, hits, and is used up (to be picked up again)");
    // --- safety net: only a bow with no arrows counts as having no weapon
    for (int w = 0; w < W_COUNT; w++) g.p.inv.set(itemWpn(w), 0);
    g.p.inv.set(itemWpn(W_BOW), 1);
    g.p.inv.set(itemAmmo(AM_ARROW), 0);
    g.p.inv.set(itemAmmo(AM_BOLT), 0);
    g.p.coins = 0;
    for (int m = 0; m < M_COUNT; m++) g.p.inv.set(itemMat(m), 0);
    g.projs.clear();
    g.items.clear();
    g.stuckCheckT = 0;
    step(70);
    check(g.p.wpn(W_FLINT) == FLINT_LOAN, "stranded with a bow, no arrows and no money: the merchant lends flint knives");
    // --- new materials drop from the new species
    {
        struct E { int kind, mat; } ex[] = {{A_DUCK, M_FEATHER}, {A_EAGLE, M_FEATHER}, {A_RATTLER, M_SCALE}, {A_GATOR, M_SCALE},
                                            {A_GOAT, M_HORN}, {A_BIGHORN, M_HORN}, {A_MTLION, M_CLAW}, {A_POLARBEAR, M_CLAW}};
        bool all = true;
        for (auto& e : ex) {
            int got = 0;
            for (int i = 0; i < 12; i++) {
                g.animals.clear();
                Animal* a = g.spawnAnimal(e.kind, g.p.pos + V2(40, 0), 0, 0);
                g.killAnimal(*a);
                got += g.animals[0].remain[e.mat];
            }
            if (got <= 0) {
                all = false;
                printf("    %s never dropped %s in 12 kills\n", animalName(e.kind, 0).c_str(), MAT_NAME[e.mat]);
            }
        }
        check(all, "feathers, scales, horns and claws drop from the species they were assigned to");
        int orig = 0;
        for (int k : {A_RABBIT, A_DEER, A_BOAR, A_WOLF})
            for (int m = M_FEATHER; m < M_CARCASS_N; m++) orig += ANIMAL[k].drop[m][1];
        check(orig == 0, "the original four animals drop no new materials");
    }
    // --- save round trip
    {
        g.p.coins = 123;
        g.p.inv.maxWeight = 1e6f;  // this check is about slots / items, not weight
        for (int m = 0; m < M_CARCASS_N; m++) g.p.inv.set(itemMat(m), m + 1);
        for (int w = 0; w < W_COUNT; w++) g.p.inv.set(itemWpn(w), isLauncher(w) ? 1 : w + 2);
        g.p.inv.set(itemAmmo(AM_ARROW), 37);
        g.p.inv.set(itemAmmo(AM_BOLT), 11);
        g.save();
        Player keep = g.p;
        g.p.coins = 0;
        for (int m = 0; m < M_COUNT; m++) g.p.inv.set(itemMat(m), 0);
        for (int w = 0; w < W_COUNT; w++) g.p.inv.set(itemWpn(w), 0);
        g.p.inv.clear();
        bool ld = g.load();
        bool same = ld && g.p.coins == keep.coins && g.p.ammo(AM_ARROW) == 37 && g.p.ammo(AM_BOLT) == 11;
        for (int m = 0; m < M_CARCASS_N; m++) same = same && g.p.mat(m) == keep.mat(m);
        for (int w = 0; w < W_COUNT; w++) same = same && g.p.wpn(w) == keep.wpn(w);
        check(same, "save / load round trip keeps coins, all 11 animal materials, all 9 weapons and both ammo types");
    }
    if (hadSave) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out << savedBytes;
    } else
        std::remove(g.savePath().c_str());
    printf("WEAPONS: %s (%d failed)\n", bad ? "FAILED" : "ok", bad);
    return bad;
}

