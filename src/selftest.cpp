// selftest.cpp - `wildwood --selftest`: inventory, crafting, skills, saves, quests, animals and the rest of the game rules (moved out of main.cpp).
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
#include "game.hpp"
#include "export_sprites.hpp"

// ---- inventory verification (flag: --selftest): the Inventory class, the game rules built on it, and save v2 / migration
int selfTest(Game& g) {
    int bad = 0;
    g.craftAnywhere = true;  // the crafting rules below are tested away from the buildings (the building rule has its own checks)
    auto check = [&](bool ok, const char* what) {
        printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
        if (!ok) bad++;
    };
    const int MEAT = itemMat(M_MEAT), HIDE = itemMat(M_HIDE), BONE = itemMat(M_BONE);
    printf("inventory class\n");
    {
        const float NOLIM = 1e9f;  // these checks are about slots and stacks, so the weight limit is out of the way
        Inventory inv(10, NOLIM);
        check(inv.add(MEAT, 2500) == 2500 && inv.usedSlots() == 3 && inv.count(MEAT) == 2500, "2500 meat = stacks of 999, 999, 502 (3 slots)");
        check(inv.slots[0].n == 999 && inv.slots[1].n == 999 && inv.slots[2].n == 502, "stacks fill up to exactly 999");
        check(inv.remove(MEAT, 600) == 600 && inv.count(MEAT) == 1900 && inv.usedSlots() == 2, "removing 600 empties the 502-stack first, then 98 from another");
        check(inv.remove(MEAT, 99999) == 1900 && inv.usedSlots() == 0, "removing more than owned removes only what exists");
        Inventory tiny(2, NOLIM);
        check(tiny.add(MEAT, 3000) == 1998 && tiny.room(MEAT) == 0 && !tiny.canAdd(HIDE, 1), "a full pack takes a partial stack and then refuses (nothing lost: the caller keeps the rest)");
        check(tiny.add(MEAT, 1) == 0 && tiny.blockReason(MEAT, 1) == 1, "adding to a full pack adds nothing and the reason is 'no slot'");
        Inventory bag(5, NOLIM);
        check(bag.add(itemWpn(W_BOW), 3) == 3 && bag.usedSlots() == 3, "launchers never stack: 3 bows use 3 slots");
        check(bag.add(itemWpn(W_FLINT), 1001) == 1001 && bag.usedSlots() == 5, "thrown weapons stack to 999: 1001 knives use 2 slots");
        check(bag.add(MEAT, 1) == 0, "no slot left for anything new");
        Inventory c(3, NOLIM);
        c.add(MEAT, 400);
        c.add(HIDE, 5);
        c.add(MEAT, 700);  // tops up the first stack to 999, remainder into a new slot -> all 3 slots in use
        check(c.usedSlots() == 3 && c.count(MEAT) == 1100 && c.slots[0].n == 999, "an existing partial stack is topped up before a new slot is used");
        c.remove(MEAT, 1000);
        c.compact();
        check(c.usedSlots() == 2 && c.count(MEAT) == 100 && c.count(HIDE) == 5, "compact merges partial stacks");
        check(Inventory().capacity() == 100 && INV_SLOTS == 100 && MAX_STACK == 999, "the pack always has exactly 100 slots and stacks to 999");
        // different items never share a slot
        {
            Inventory mix(100, NOLIM);
            mix.add(MEAT, 5);
            mix.add(HIDE, 5);
            mix.add(BONE, 5);
            bool one = true;
            for (auto& sl : mix.slots)
                if (!sl.empty() && sl.n != 5) one = false;
            check(one && mix.usedSlots() == 3, "different items never stack together");
        }
        // weight
        {
            check(item(itemMat(M_WOOD)).weight == 1.0f && item(itemMat(M_STONE)).weight == 2.0f && item(itemMat(M_IRON)).weight == 3.0f &&
                      item(itemMat(M_HERB)).weight == 0.1f && item(itemMat(M_HIDE)).weight == 1.5f && item(itemMat(M_MEAT)).weight == 0.8f && item(itemMat(M_BONE)).weight == 0.5f,
                  "item weights follow the spec (wood 1, stone 2, iron ore 3, herbs 0.1, hide 1.5, meat 0.8, bone 0.5 kg)");
            bool allW = true;
            for (int i = 0; i < ITEM_COUNT; i++) allW = allW && item(i).weight > 0.f;
            check(allW, "every item has a positive weight");
            Inventory w(100, 50.f);
            w.add(itemMat(M_WOOD), 10);
            w.add(itemMat(M_STONE), 5);
            check(fabsf(w.totalWeight() - 20.f) < 1e-4f && fabsf(w.stackWeight(0) - 10.f) < 1e-4f, "stack weight = unit weight x quantity; total is the sum of all stacks");
            check(w.add(itemMat(M_STONE), 100) == 15 && fabsf(w.totalWeight() - 50.f) < 1e-3f, "an add stops at the weight limit (only 15 more stone fit under 50 kg)");
            check(w.add(itemMat(M_HERB), 1) == 0 && w.blockReason(itemMat(M_HERB), 1) == 2 && !w.canAdd(itemMat(M_WOOD), 1), "at the limit nothing more is taken, and the reason is 'too heavy'");
            w.maxWeight = 100.f;
            check(w.canAdd(itemMat(M_WOOD), 50) && !w.canAdd(itemMat(M_WOOD), 51), "raising the limit makes room again, recalculated at once");
            w.maxWeight = 30.f;
            check(w.overweight() && w.count(itemMat(M_STONE)) == 20 && w.totalWeight() > 30.f, "a lowered limit leaves the hunter overweight - nothing is deleted");
        }
        // random operations against a reference model
        Inventory z(20, 1e9f);
        std::map<int, long> ref;
        Rng r(12345);
        bool okInv = true;
        for (int i = 0; i < 40000 && okInv; i++) {
            int it = r.irange(0, ITEM_COUNT - 1);
            if (r.chance(0.55f)) {
                int n = r.irange(1, 1500), got = z.add(it, n);
                ref[it] += got;
                if (got < n && z.room(it) != 0) okInv = false;  // a short add must mean the item really has no room left
            } else {
                int n = r.irange(1, 1600), got = z.remove(it, n);
                if (got != std::min<long>(n, ref[it])) okInv = false;
                ref[it] -= got;
            }
            long total = 0;
            for (auto& sl : z.slots) {
                if (sl.item >= 0 && (sl.n <= 0 || sl.n > item(sl.item).stack)) okInv = false;
                if (sl.item < 0 && sl.n != 0) okInv = false;
                total += sl.n;
            }
            if (z.count(it) != ref[it]) okInv = false;
            if (z.usedSlots() > z.capacity()) okInv = false;
        }
        check(okInv, "40000 random adds/removes: counts match a reference model, no stack over its limit, no phantom slots");
        // keys
        bool keys = true;
        std::set<std::string> ks;
        for (int i = 0; i < ITEM_COUNT; i++) {
            keys = keys && itemFromKey(itemKey(i)) == i;
            ks.insert(itemKey(i));
        }
        check(keys && (int)ks.size() == ITEM_COUNT && itemFromKey("no_such_item") == -1, "every item has a unique stable save key that maps back to it");
        check(item(MEAT).stack == 999 && item(itemMat(M_CLAW)).stack == 999 && item(itemWpn(W_FLINT)).stack == 999 && item(itemAmmo(AM_ARROW)).stack == 999, "stackable items stack to 999");
        check(item(itemWpn(W_BOW)).stack == 1 && item(itemTool(TL_AXE1)).stack == 1 && item(itemPack(PK_SATCHEL)).stack == 1 && item(itemCarry(CG_QUIVER)).stack == 1, "launchers, tools, bags and carried gear take a slot each");
        check(BASE_CARRY_KG == 50 && PACK_CARRY_KG[0] == 100 && PACK_CARRY_KG[1] == 150 && PACK_CARRY_KG[2] == 200 && PACK_CARRY_KG[3] == 300, "carrying limits: no bag 50, satchel 100, trapper 150, explorer 200, expedition 300 kg");
    }
    // ---- game rules
    std::string savedBytes;
    bool hadSave = false;
    {
        std::ifstream in(g.savePath(), std::ios::binary);
        if (in) {
            hadSave = true;
            savedBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
    }
    printf("game rules\n");
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    check(g.p.bagCap() == 100 && g.p.bagUsed() == 2 && g.p.wpn(W_FLINT) == 8 && g.p.bandages() == 1, "a new hunter has a 100-slot pack with 8 flint knives and 1 bandage (2 slots)");
    check(g.p.bag < 0 && g.p.carryLimit() == 50 && g.p.hot[0] == itemWpn(W_FLINT) && g.p.hot[3] == ITEM_CONS0 + C_BANDAGE && g.p.equippedWpn(W_FLINT), "...no bag (50 kg), and the knife and the bandage start equipped on the hotbar");
    // fill every slot with something other than meat/bone/hide
    auto fillWithJunk = [&]() {
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
    };
    fillWithJunk();
    check(g.p.bagUsed() == g.p.bagCap(), "test setup: every slot is in use");
    {
        g.animals.clear();
        Animal* a = g.spawnAnimal(A_DEER, g.p.pos + V2(10, 0), 0, 0);
        g.killAnimal(*a);
        Animal& c = g.animals[0];
        int before = g.p.mat(M_MEAT);
        c.remain[M_MEAT] = 5;
        c.totalRemain += 5;
        int remBefore = c.remain[M_MEAT];
        g.bagWarn = 0;
        const int64_t xpFull = g.p.sk.s[SK_BUTCHERING].xp;
        g.gatherStep(c);
        check(g.p.mat(M_MEAT) == before && c.remain[M_MEAT] == remBefore, "gathering is refused when the bag is full and no stack can take it");
        check(g.p.sk.s[SK_BUTCHERING].xp == xpFull, "a refused (bag full) carcass step earns no XP");
        g.p.inv.remove(itemMat(M_TUSK), 50);  // frees one slot
        g.gatherStep(c);
        check(g.p.mat(M_MEAT) >= before + 1, "after freeing a slot, gathering works again");
        check(g.p.sk.s[SK_BUTCHERING].xp > xpFull, "...and cutting meat trains Butchering");
        // a partly filled meat stack can still take more in a full bag
        g.p.inv.clear();
        fillWithJunk();
        g.p.inv.remove(itemMat(M_TUSK), 50);
        g.p.inv.add(MEAT, 10);
        int free0 = g.p.inv.freeSlots();
        c.remain[M_MEAT] = 3;
        g.gatherStep(c);
        check(free0 == 0 && g.p.mat(M_MEAT) > 10, "a full bag still accepts items that fit an existing stack");
    }
    // the recipe table grew: find the original recipes by what they make instead of by position
    auto findRecipe = [&](int kind, int out, int firstMat) {
        for (int r = 0; r < RECIPE_N; r++)
            if (RECIPES[r].kind == kind && RECIPES[r].out == out && RECIPES[r].mat[0] == firstMat) return r;
        return -1;
    };
    const int R_BONE = findRecipe(RK_WEAPON, W_BONE, M_BONE), R_ARROW = findRecipe(RK_AMMO, AM_ARROW, M_BONE),
              R_BOLT = findRecipe(RK_AMMO, AM_BOLT, M_BONE), R_BANDAGE = findRecipe(RK_BANDAGE, 0, M_HIDE);
    check(R_BONE == 0 && R_ARROW > 0 && R_BOLT > 0 && R_BANDAGE > 0, "the original recipes are all still in the table");
    {   // crafting in a full bag
        g.p.pos = g.world.spawn;  // bone knives are made at the shop
        g.p.inv.clear();
        fillWithJunk();
        g.p.inv.remove(itemMat(M_TUSK), 50);
        g.p.inv.remove(itemMat(M_ANTLER), 50);
        g.p.inv.remove(itemWpn(W_BONE), 1);  // three free slots: bone, hide and a bit of meat fill them, so the bag is full again
        g.p.inv.add(BONE, 3);
        g.p.inv.add(HIDE, 1);
        g.p.inv.add(MEAT, 1);
        int bonesBefore = g.p.mat(M_BONE);
        bool ok = g.craft(R_BONE);  // BONE KNIFE x4: needs 3 bone + 1 hide, the two ingredient stacks vanish, the knives take a slot
        check(ok && g.p.wpn(W_BONE) == 4 && g.p.mat(M_BONE) == bonesBefore - 3, "crafting in a full bag works when the ingredients free up the slot it needs");
        g.p.inv.clear();
        fillWithJunk();
        g.p.inv.remove(itemMat(M_TUSK), 50);
        g.p.inv.remove(itemMat(M_ANTLER), 50);
        g.p.inv.remove(itemWpn(W_BONE), 1);
        g.p.inv.add(BONE, 10);  // the free slots go to bone, hide and meat; the recipe uses only part of the bone and hide stacks,
        g.p.inv.add(HIDE, 5);   // so nothing is freed and the finished knives have nowhere to go
        g.p.inv.add(MEAT, 1);
        int free1 = g.p.inv.freeSlots();
        int b0 = g.p.mat(M_BONE), h0 = g.p.mat(M_HIDE);
        bool ok2 = g.craft(R_BONE);
        check(free1 == 0 && !ok2 && g.p.mat(M_BONE) == b0 && g.p.mat(M_HIDE) == h0 && g.p.wpn(W_BONE) == 0, "crafting with no room is refused and the ingredients are returned untouched");
    }
    {   // picking up thrown weapons
        g.p.inv.clear();
        fillWithJunk();
        g.p.inv.remove(itemWpn(W_JAVELIN), 1);  // free exactly one slot, then take it with another item
        g.p.inv.add(MEAT, 1);
        g.items.clear();
        g.items.push_back({g.p.pos, W_JAVELIN, 0.f, 0, -1});
        for (int i = 0; i < 5; i++) g.update(1.f / 60.f);
        check(g.items.size() == 1 && g.p.wpn(W_JAVELIN) == 0, "a thrown weapon stays on the ground while the bag is full");
        g.p.inv.remove(MEAT, 1);
        for (int i = 0; i < 5; i++) g.update(1.f / 60.f);
        check(g.items.empty() && g.p.wpn(W_JAVELIN) == 1, "and is picked up as soon as there is room");
    }
    {   // backpack upgrades only add slots
        g.newGame();
        g.state = ST_PLAY;
        g.p.coins = 5000;
        g.p.inv.add(HIDE, 50);
        g.p.inv.add(itemMat(M_FUR), 50);
        g.p.inv.add(BONE, 50);
        g.p.inv.add(itemMat(M_TUSK), 50);
        check(!g.buyUpgrade(U_BAG) && g.p.up[U_BAG] == 0, "the backpack is no longer a shop upgrade (bags are crafted and worn)");
    }
    {   // bags: crafted items, worn in the bag slot; they only raise the carrying-weight limit
        g.newGame();
        g.state = ST_PLAY;
        g.p.coins = 0;
        const int SAT = itemPack(PK_SATCHEL), TRAP = itemPack(PK_TRAPPER), EXPL = itemPack(PK_EXPLORER), EXPE = itemPack(PK_EXPEDITION);
        int rSat = -1, rTrap = -1, rExpl = -1, rExpe = -1;
        for (int r = 0; r < RECIPE_N; r++) {
            if (RECIPES[r].kind == RK_ITEM && RECIPES[r].out == SAT) rSat = r;
            if (RECIPES[r].kind == RK_ITEM && RECIPES[r].out == TRAP) rTrap = r;
            if (RECIPES[r].kind == RK_ITEM && RECIPES[r].out == EXPL) rExpl = r;
            if (RECIPES[r].kind == RK_ITEM && RECIPES[r].out == EXPE) rExpe = r;
        }
        check(rSat >= 0 && rTrap >= 0 && rExpl >= 0 && rExpe >= 0, "every bag has a crafting recipe");
        g.p.inv.add(HIDE, 4);
        g.p.inv.add(itemMat(M_FIBER), 2);
        check(g.craft(rSat) && g.p.inv.count(SAT) == 1 && g.p.bag < 0 && g.p.carryLimit() == 50, "crafting a bag puts the item in the pack - it does nothing until worn");
        int satSlot = -1;
        for (int i = 0; i < g.p.bagCap(); i++)
            if (g.p.inv.slots[i].item == SAT) satSlot = i;
        const int used0 = g.p.bagUsed();
        check(g.useSlot(satSlot) && g.p.bag == PK_SATCHEL && g.p.inv.count(SAT) == 0 && g.p.carryLimit() == 100 && g.p.inv.maxWeight == 100.f, "wearing the satchel raises the limit to 100 kg and takes it out of the pack");
        check(g.p.bagCap() == 100 && g.p.bagUsed() == used0 - 1 && item(HIDE).stack == MAX_STACK, "...the pack still has exactly 100 slots and stacks still hold 999");
        g.p.inv.add(TRAP, 1);
        int trapSlot = -1;
        for (int i = 0; i < g.p.bagCap(); i++)
            if (g.p.inv.slots[i].item == TRAP) trapSlot = i;
        check(g.useSlot(trapSlot) && g.p.bag == PK_TRAPPER && g.p.carryLimit() == 150 && g.p.inv.count(SAT) == 1 && g.p.inv.count(TRAP) == 0, "wearing another bag swaps: the trapper pack gives 150 kg, the satchel goes back into the pack");
        g.p.inv.add(EXPL, 1);
        g.p.inv.add(EXPE, 1);
        for (int k : {PK_EXPLORER, PK_EXPEDITION}) {
            int sl = -1;
            for (int i = 0; i < g.p.bagCap(); i++)
                if (g.p.inv.slots[i].item == itemPack(k)) sl = i;
            g.useSlot(sl);
            check(g.p.bag == k && g.p.carryLimit() == PACK_CARRY_KG[k] && g.p.bagCap() == 100, k == PK_EXPLORER ? "the explorer pack gives 200 kg" : "the expedition pack gives 300 kg");
        }
        check(g.unequipBag() && g.p.bag < 0 && g.p.carryLimit() == 50 && g.p.inv.count(EXPE) == 1, "taking the bag off drops the limit back to 50 kg and returns it to the pack");
        // bag weight never counts while worn; one bag at a time
        g.p.inv.clear();
        g.p.inv.add(EXPL, 1);
        g.useSlot(0);
        check(g.p.inv.totalWeight() == 0.f && g.p.inv.usedSlots() == 0, "a worn bag is not carried: it has no weight and uses no slot");
        // wearing a smaller bag while overweight keeps every item
        g.p.inv.add(itemMat(M_STONE), 90);  // 180 kg under a 200 kg bag
        g.p.inv.add(SAT, 1);
        int sl2 = -1;
        for (int i = 0; i < g.p.bagCap(); i++)
            if (g.p.inv.slots[i].item == SAT) sl2 = i;
        g.useSlot(sl2);
        check(g.p.bag == PK_SATCHEL && g.p.inv.overweight() && g.p.mat(M_STONE) == 90, "swapping to a smaller bag while heavy leaves the hunter overweight - nothing is deleted");
        check(g.p.inv.add(itemMat(M_HERB), 1) == 0, "...and nothing more can be picked up until the load is lighter");
        g.p.inv.remove(itemMat(M_STONE), 50);  // back to 80 kg (under 100)
        check(!g.p.inv.overweight() && g.p.inv.add(itemMat(M_HERB), 1) == 1, "after dropping weight it recalculates and pickups work again");
    }
    {   // weight rules: every way of getting an item respects the carrying limit; nothing is ever deleted
        printf("weight, hotbar and dropping\n");
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.p.inv.clear();
        g.p.hot[0] = itemWpn(W_FLINT);
        g.p.inv.add(itemMat(M_STONE), 25);  // 50 kg: exactly the no-bag limit
        check(fabsf(g.p.inv.totalWeight() - 50.f) < 1e-3f && !g.p.inv.overweight(), "25 stone is exactly 50 kg: allowed");
        g.animals.clear();
        Animal* d = g.spawnAnimal(A_DEER, g.p.pos + V2(10, 0), 0, 0);
        g.killAnimal(*d);
        d->remain[M_MEAT] = 4;
        d->totalRemain += 4;
        const int meat0 = g.p.mat(M_MEAT);
        g.bagWarn = 0;
        g.gatherStep(*d);
        check(g.p.mat(M_MEAT) == meat0 && g.p.mat(M_STONE) == 25, "skinning / butchering is refused when the load would be too heavy (nothing is lost)");
        g.bagWarn = 0;
        g.gatherStep(*d);
        // ground pickup (a thrown knife): stays while too heavy, taken when lighter
        g.items.clear();
        g.items.push_back({g.p.pos, W_JAVELIN, 0.f, 0, -1});
        for (int i = 0; i < 5; i++) g.update(1.f / 60.f);
        check(g.items.size() == 1 && g.p.wpn(W_JAVELIN) == 0, "a thrown weapon stays on the ground while the load is at the limit");
        // dropping: weight goes down, the stack lies on the ground, walking over it later takes it back
        int stoneSlot = -1;
        for (int i = 0; i < g.p.bagCap(); i++)
            if (g.p.inv.slots[i].item == itemMat(M_STONE)) stoneSlot = i;
        check(g.dropSlot(stoneSlot, 10) && g.p.mat(M_STONE) == 15 && fabsf(g.p.inv.totalWeight() - 30.f) < 1e-3f, "dropping 10 stone leaves 15 (30 kg)");
        bool dropped = false;
        for (auto& gi : g.items)
            if (gi.item == itemMat(M_STONE) && gi.n == 10) dropped = true;
        check(dropped, "the dropped stack lies on the ground (nothing was destroyed)");
        for (int i = 0; i < 5; i++) g.update(1.f / 60.f);
        check(g.p.wpn(W_JAVELIN) == 1, "with the load lighter the thrown weapon is picked up");
        for (int i = 0; i < 120; i++) g.update(1.f / 60.f);  // the drop lock (1.5 s) runs out while standing on it
        check(g.p.mat(M_STONE) == 24 && g.items.size() == 1 && g.items[0].n == 1 && !g.p.inv.overweight(), "walking over a dropped stack takes it back only as far as the weight limit allows (9 of 10; the last one stays on the ground)");
        // selling and storing reduce weight
        const float wBeforeSell = g.p.inv.totalWeight();
        g.sellMat(M_STONE, 5);
        check(g.p.mat(M_STONE) == 19 && fabsf(wBeforeSell - g.p.inv.totalWeight() - 10.f) < 1e-3f, "selling 5 stone reduces the weight by 10 kg at once");
        g.p.inv.clear();
        g.p.inv.add(itemPlace(PL_CHEST), 1);
        check(g.placeStructure(PL_CHEST) && g.structs.size() >= 1, "(setup) a chest is placed");
        g.chestOpen = (int)g.structs.size() - 1;
        g.p.inv.add(itemMat(M_STONE), 24);  // 48 kg
        int ss = -1;
        for (int i = 0; i < g.p.bagCap(); i++)
            if (g.p.inv.slots[i].item == itemMat(M_STONE)) ss = i;
        check(g.chestMove(1, ss) && g.p.inv.totalWeight() < 1e-3f && g.structs[g.chestOpen].inv.count(itemMat(M_STONE)) == 24, "storing a stack in a chest takes its weight off the hunter (chests have no weight limit)");
        g.p.inv.add(itemMat(M_IRON), 10);  // 30 kg in the pack
        int cs = -1;
        for (int i = 0; i < g.structs[g.chestOpen].inv.capacity(); i++)
            if (g.structs[g.chestOpen].inv.slots[i].item == itemMat(M_STONE)) cs = i;
        g.chestMove(0, cs);  // only 10 of the 24 stone fit (20 kg): the rest stays in the chest
        check(g.p.mat(M_STONE) == 10 && g.structs[g.chestOpen].inv.count(itemMat(M_STONE)) == 14, "taking from a chest stops at the weight limit; the rest stays in the chest");
        g.chestOpen = -1;
        // crafting a heavy result
        {
            g.p.inv.clear();
            g.p.inv.addIgnoringWeight(itemMat(M_STONE), 23);  // 46 kg
            g.p.inv.addIgnoringWeight(itemMat(M_WOOD), 6);
            g.p.inv.addIgnoringWeight(itemMat(M_ROPE), 2);
            int rChest = -1;
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == RK_ITEM && RECIPES[r].out == itemPlace(PL_CHEST)) rChest = r;
            check(rChest >= 0 && g.craftBlock(rChest) == "TOO HEAVY", "crafting an 8 kg chest is refused when the result would push the load over the limit");
            g.p.inv.remove(itemMat(M_STONE), 10);
            check(g.craftBlock(rChest).empty() && g.craft(rChest) && g.p.inv.count(itemPlace(PL_CHEST)) == 1 && g.p.mat(M_WOOD) == 0, "...and works once the load is lighter (the ingredients' weight is freed first)");
        }
        // pack full (slots) is reported separately from too heavy
        g.p.inv.clear();
        g.p.inv.maxWeight = 1e6f;
        for (int i = 0; i < 100; i++) g.p.inv.add(itemWpn(W_BOW), 1);
        check(g.p.inv.blockReason(itemMat(M_HERB), 1) == 1, "all 100 slots used: the reason is 'pack full', not weight");
        // ---- hotbar
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.add(itemMat(M_MEAT), 5);
        g.p.inv.add(itemWpn(W_BOW), 1);
        g.p.inv.add(itemAmmo(AM_ARROW), 20);
        g.p.inv.add(itemCons(C_COOKED), 3);
        g.p.inv.add(itemCons(C_BREW), 1);
        check(g.p.hotSlotOf(itemMat(M_MEAT)) < 0 && g.p.hotSlotOf(itemWpn(W_BOW)) < 0 && g.p.hotSlotOf(itemCons(C_COOKED)) < 0, "collected items are never auto-assigned to the hotbar");
        check(!g.equipHot(itemMat(M_MEAT)) && !g.equipHot(itemAmmo(AM_ARROW)) && !g.equipHot(itemTool(TL_AXE1)), "materials, ammunition and tools cannot be equipped on the hotbar");
        check(!g.equipHot(itemWpn(W_BOW), 3) && !g.equipHot(itemCons(C_COOKED), 0) && !g.equipHot(itemCons(C_BREW), 5), "an item is refused by a slot of the wrong kind (weapon in a healing slot, food in a weapon slot, buff in the food slot)");
        check(!g.canUse(W_BOW), "an unequipped weapon cannot be used even though it is in the pack");
        check(g.equipHot(itemWpn(W_BOW)) && g.p.hot[1] == itemWpn(W_BOW) && g.p.sel == W_BOW && g.canUse(W_BOW), "equipping the bow puts it in the first free weapon slot and takes it in hand");
        check(g.equipHot(itemCons(C_COOKED)) && g.p.hot[5] == itemCons(C_COOKED) && g.equipHot(itemCons(C_BREW)) && g.p.hot[6] == itemCons(C_BREW), "food and buffs go to their own slots");
        check(!g.equipHot(itemCons(C_COOKED)), "equipping the same item twice is refused");
        g.hotUse(0);
        check(g.p.sel == W_FLINT, "hotbar key 1 selects the knife");
        g.p.hp = 40;
        g.hotUse(5);
        check(g.p.hp == 75 && g.p.inv.count(itemCons(C_COOKED)) == 2, "a hotbar key uses the equipped food (+35 HP, one used up)");
        g.p.hp = 40;
        g.p.inv.remove(ITEM_CONS0 + C_BANDAGE, 1);
        const int heal0 = (int)g.p.hp;
        g.hotUse(3);
        check((int)g.p.hp == heal0 && g.p.inv.count(ITEM_CONS0 + C_BANDAGE) == 0, "an equipped slot with none left does nothing (and keeps its assignment)");
        g.p.inv.add(ITEM_CONS0 + C_BANDAGE, 2);
        g.p.hp = 40;
        g.in.bandage = true;
        g.update(1.f / 60.f);
        g.in.bandage = false;
        check(g.p.hp >= 40 + BANDAGE_HEAL - 2 && g.p.inv.count(ITEM_CONS0 + C_BANDAGE) == 1, "Q heals with the healing item equipped on the hotbar");
        g.unequipHot(3);
        g.unequipHot(5);  // the cooked meat is healing too
        g.p.hp = 40;
        g.in.bandage = true;
        g.update(1.f / 60.f);
        g.in.bandage = false;
        check(g.p.hp < 50 && g.p.inv.count(ITEM_CONS0 + C_BANDAGE) == 1, "unequipped healing items are not used by Q");
        g.unequipHot(0);
        g.p.sel = W_FLINT;
        g.fixSel();
        check(g.p.sel == W_BOW && !g.canUse(W_FLINT), "unequipping the selected knife switches to the other equipped weapon; the knife cannot be thrown");
        {   // wheel cycles only through equipped weapons
            g.equipHot(itemWpn(W_FLINT), 0);
            g.p.sel = W_FLINT;
            g.in.wheel = -1;
            g.update(1.f / 60.f);
            g.in.wheel = 0;
            check(g.p.sel == W_BOW, "the mouse wheel moves between equipped weapons only");
            g.p.inv.add(itemWpn(W_SPEAR), 1);
            g.in.wheel = -1;
            g.update(1.f / 60.f);
            g.in.wheel = 0;
            check(g.p.sel == W_FLINT, "...an unequipped spear in the pack is skipped");
        }
        {   // empty slots stay empty; the hotbar renders in every state
            for (int i = 0; i < HOT_N; i++) g.p.hot[i] = -1;
            g.render();
            check(g.p.hotSlotOf(itemWpn(W_FLINT)) < 0, "an empty hotbar renders and stays empty (nothing is auto-filled)");
            g.p.hot[0] = itemWpn(W_FLINT);
            g.p.hot[1] = itemWpn(W_BOW);
            g.p.hot[3] = ITEM_CONS0 + C_BANDAGE;
            g.p.inv.add(itemMat(M_HERB), 9);
            g.render();
            check(true, "a hotbar with weapons and consumables renders");
        }
        // ---- saves hold no derived values
        g.save();
        std::ifstream sf(g.savePath());
        std::string sall((std::istreambuf_iterator<char>(sf)), std::istreambuf_iterator<char>());
        check(sall.find("weight") == std::string::npos && sall.find("capacity") == std::string::npos && sall.find("hotbar ") != std::string::npos && sall.find("bag ") != std::string::npos, "the save stores the bag and hotbar assignments but no weights or capacity");
        // a save written before the hotbar existed: what it owns is equipped once
        {
            std::string oldv2 = "version 2\ncoins 10\nup 0 0 0 0\nslot flint_knife 4\nslot hunting_bow 1\nslot bandage 2\nslot herbal_salve 1\nslot raw_meat 5\n";
            std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
            out << oldv2;
            out.close();
            bool ld = g.load();
            check(ld && g.p.bag < 0 && g.p.equippedWpn(W_FLINT) && g.p.equippedWpn(W_BOW) && g.p.hotSlotOf(ITEM_CONS0 + C_BANDAGE) >= 0 && g.p.hotSlotOf(itemCons(C_SALVE)) >= 0 && g.p.hotSlotOf(itemMat(M_MEAT)) < 0,
                  "an old save without a hotbar loads with its weapons and consumables equipped once (materials never)");
        }
        std::remove(g.savePath().c_str());
        g.newGame();
        g.state = ST_PLAY;
    }
    // ---- inventory screen (key I) and crafting
    printf("inventory screen and crafting\n");
    {
        auto frame = [&](int n = 1) {
            for (int i = 0; i < n; i++) {
                g.update(1.f / 60.f);
                g.in.lPressed = g.in.lReleased = g.in.ePressed = g.in.escPressed = g.in.enterPressed = g.in.tabPressed = false;
                g.in.invToggle = false;
                g.in.selectKey = -1;
            }
        };
        auto shotDir = getenv("INV_SHOTS");
        auto snap = [&](const char* name) {
            g.render();
            if (shotDir) g.saveCanvas((std::string(shotDir) + "/" + name + ".bmp").c_str());
        };
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.animals.clear();
        g.items.clear();
        g.p.pos = g.world.spawn;
        // ---- open / close
        g.in.invToggle = true;
        frame();
        check(g.state == ST_INVENTORY, "pressing I opens the inventory screen");
        g.in.invToggle = true;
        frame();
        check(g.state == ST_PLAY, "pressing I again closes it");
        g.in.invToggle = true;
        frame();
        g.in.escPressed = true;
        frame();
        check(g.state == ST_PLAY, "ESC also closes it (and does not open the pause menu)");
        // can't open while charging a throw
        g.p.charging = true;
        g.in.invToggle = true;
        frame();
        check(g.state == ST_PLAY, "it does not open in the middle of a throw");
        g.p.charging = false;
        // ---- time stands still
        g.animals.clear();
        Animal* w = g.spawnAnimal(A_DEER, g.p.pos + V2(60, 0), 0, 0);
        w->st = AS_WANDER;
        w->idleT = 0;
        w->target = w->pos + V2(80, 0);
        g.in.invToggle = true;
        frame();
        float day0 = g.dayT;
        V2 apos = g.animals[0].pos;
        frame(120);
        check(g.dayT == day0 && g.animals[0].pos.x == apos.x && g.animals[0].pos.y == apos.y, "the world is paused while the pack is open (clock and animals do not move)");
        // ---- cursor movement
        g.p.inv.clear();
        g.p.inv.add(itemWpn(W_FLINT), 8);
        g.p.inv.add(itemWpn(W_BONE), 3);
        g.p.inv.add(itemWpn(W_BOW), 1);
        g.p.inv.add(itemAmmo(AM_ARROW), 12);
        g.p.inv.add(ITEM_CONS0 + C_BANDAGE, 2);
        g.invSel = 0;
        auto press = [&](bool* key) {
            *key = true;
            frame();
            *key = false;
            frame();
        };
        press(&g.in.right);
        press(&g.in.right);
        check(g.invSel == 2, "right arrow moves the cursor one slot per key press");
        g.in.right = true;
        frame(30);
        g.in.right = false;
        check(g.invSel == 3, "holding a key does not make the cursor run away (one step, not repeating)");
        press(&g.in.down);
        check(g.invSel == 13, "down moves one row (10 columns)");
        for (int i = 0; i < 12; i++) press(&g.in.left);
        check(g.invSel % 10 == 0, "the cursor stops at the left edge");
        for (int i = 0; i < 12; i++) press(&g.in.down);
        check(g.invSel / 10 == 9 && g.invSel < 100, "the cursor stops at the last row of the 100-slot pack");
        // ---- equip / use with Enter
        g.invSel = 1;  // bone knives
        g.in.enterPressed = true;
        frame();
        check(g.p.sel == W_BONE, "ENTER on a weapon equips it");
        g.invSel = 2;
        g.in.enterPressed = true;
        frame();
        check(g.p.sel == W_BOW, "ENTER on the bow equips it");
        g.p.sel = W_FLINT;
        g.invSel = 3;
        g.in.enterPressed = true;
        frame();
        check(g.p.sel == W_BOW, "ENTER on arrows equips the bow that fires them");
        g.p.inv.remove(itemWpn(W_BOW), 1);
        g.p.sel = W_FLINT;
        g.in.enterPressed = true;
        frame();
        check(g.p.sel == W_FLINT, "arrows with no bow do nothing");
        g.p.hp = g.p.maxhp;
        g.invSel = 4;
        int b0 = g.p.bandages();
        g.in.enterPressed = true;
        frame();
        check(g.p.bandages() == b0 && g.p.hp == g.p.maxhp, "a bandage is not wasted at full health");
        g.p.hp = 30;
        g.in.enterPressed = true;
        frame();
        check(g.p.bandages() == b0 - 1 && g.p.hp == 30 + BANDAGE_HEAL, "ENTER on a bandage heals 45 HP and uses one");
        // ---- mouse: click a slot
        g.in.mx = 24 + 8 + 5 * 16 + 8;  // slot 5, as laid out by drawInventory (16-pixel cells, 10 columns)
        g.in.my = 12 + 38 + 8;
        g.in.uiClick = true;
        g.render();
        check(g.invSel == 5, "clicking a slot selects it");
        g.in.uiClick = false;
        // ---- sort
        g.p.inv.clear();
        g.p.inv.slots[0] = {MEAT, 30};  // partial stacks as they can appear in an old or edited save
        g.p.inv.slots[1] = {HIDE, 5};
        g.p.inv.slots[3] = {MEAT, 20};
        int usedBefore = g.p.bagUsed();
        g.p.inv.compact();
        check(usedBefore == 3 && g.p.bagUsed() == 2 && g.p.mat(M_MEAT) == 50 && g.p.mat(M_HIDE) == 5 && g.p.inv.slots[2].empty(), "SORT merges split stacks to the front without losing anything");
        // ---- screenshots of the screens (only written when INV_SHOTS is set)
        g.p.inv.clear();
        g.p.inv.add(itemWpn(W_FLINT), 8);
        g.p.inv.add(itemWpn(W_JAVELIN), 4);
        g.p.inv.add(itemWpn(W_BOW), 1);
        g.p.inv.add(itemWpn(W_CROSSBOW), 1);
        g.p.inv.add(itemAmmo(AM_ARROW), 24);
        g.p.inv.add(itemAmmo(AM_BOLT), 6);
        g.p.inv.add(ITEM_CONS0 + C_BANDAGE, 3);
        for (int m = 0; m < M_CARCASS_N; m++) g.p.inv.add(itemMat(m), 3 + m * 9);
        g.p.sel = W_BOW;
        g.invTab = 0;
        g.invSel = 2;
        g.in.mx = 24 + 8 + 4 * 21 + 8;
        g.in.my = 12 + 38 + 2 + 8;
        snap("inv_backpack");
        g.invTab = 1;
        g.in.mx = 5;
        g.in.my = 5;
        snap("inv_crafting");
        g.invTab = 0;
        g.p.bag = PK_TRAPPER;
        g.p.syncPack();
        g.p.inv.add(itemMat(M_WOOD), 40);
        g.p.inv.add(itemWpn(W_BOW), 1);
        g.invSel = 30;
        snap("inv_pack_100");
        g.p.bag = -1;
        g.p.syncPack();
        // ---- crafting from the screen
        g.invTab = 1;
        g.p.inv.clear();
        g.p.inv.add(BONE, 5);
        g.p.inv.add(itemMat(M_FEATHER), 4);
        g.p.inv.add(itemMat(M_HORN), 1);
        g.p.inv.add(HIDE, 4);
        g.p.inv.add(itemMat(M_ORGAN), 2);
        g.p.inv.add(itemMat(M_TUSK), 1);
        g.p.inv.add(itemMat(M_ANTLER), 1);
        g.p.inv.add(itemMat(M_FUR), 1);
        auto clickRecipe = [&](int r) {  // click the CRAFT button of recipe r: pick its category filter, then its row in that list
            g.craftCat = RECIPES[r].cat;
            g.craftScroll = 0;
            g.craftOnly = false;
            int row = 0;
            for (int q = 0; q < r; q++)
                if (RECIPES[q].cat == RECIPES[r].cat) row++;
            g.in.mx = 24 + 8 + (352 - 16 - 5) - 54 + 25;
            g.in.my = 12 + 38 + 14 + row * 22 + 4 + 6;
            g.in.uiClick = true;
            g.render();
            g.in.uiClick = false;
        };
        // crafting happens only inside the matching town building: outdoors every recipe is refused, in the right building it works
        g.craftAnywhere = false;
        g.p.pos = g.world.spawn;
        g.interior = -1;
        check(!g.nearShop(), "test setup: the hunter is outdoors");
        clickRecipe(R_BONE);
        check(g.p.wpn(W_BONE) == 0 && g.p.mat(M_BONE) == 5, "weapon recipes are refused outdoors");
        clickRecipe(R_ARROW);
        check(g.p.ammo(AM_ARROW) == 0 && g.p.mat(M_BONE) == 5, "so is every other recipe (all crafting is done in the town buildings)");
        g.interior = recipeBuilding(RECIPES[R_ARROW]);
        clickRecipe(R_ARROW);
        check(g.p.ammo(AM_ARROW) == 6 && g.p.mat(M_BONE) == 4 && g.p.mat(M_FEATHER) == 2, "ARROWS x6 are crafted in the workshop: 1 bone + 2 feathers");
        clickRecipe(R_BOLT);
        check(g.p.ammo(AM_BOLT) == 4 && g.p.mat(M_HORN) == 0 && g.p.mat(M_BONE) == 2, "BOLTS x4: 2 bone + 1 horn");
        clickRecipe(R_BOLT);
        check(g.p.ammo(AM_BOLT) == 4 && g.p.mat(M_BONE) == 2, "a recipe without its ingredients does nothing");
        clickRecipe(R_BANDAGE);
        check(g.p.bandages() == 2 && g.p.mat(M_HIDE) == 2 && g.p.mat(M_ORGAN) == 1, "BANDAGE x2: 2 hide + 1 organs");
        // bandages stack to 9: with a free slot the extra ones start a second stack; with a full bag only 1 more fits, so the recipe is refused
        g.p.inv.set(ITEM_CONS0 + C_BANDAGE, 8);
        g.p.inv.add(HIDE, 4);
        g.p.inv.add(itemMat(M_ORGAN), 2);
        bool cr = g.craft(R_BANDAGE);
        check(cr && g.p.bandages() == 10, "bandages stack (up to 999) and crafted bandages join the existing stack");
        g.p.inv.clear();
        g.p.inv.maxWeight = 1e6f;
        for (int i = 0; i < g.p.bagCap() - 2; i++) g.p.inv.add(itemWpn(W_BOW), 1);  // one slot per bow: nearly full
        g.p.inv.add(HIDE, 5);
        g.p.inv.add(itemMat(M_ORGAN), 3);
        int hide0 = g.p.mat(M_HIDE), org0 = g.p.mat(M_ORGAN), fs = g.p.inv.freeSlots();
        bool cr2 = g.craft(R_BANDAGE);
        check(fs == 0 && !cr2 && g.p.bandages() == 0 && g.p.mat(M_HIDE) == hide0 && g.p.mat(M_ORGAN) == org0, "in a full bag the bandage recipe is refused and the ingredients are returned");
        // near the shop: weapons allowed
        g.p.pos = g.world.spawn;
        g.p.inv.clear();
        g.p.inv.add(HIDE, 2);
        g.p.inv.add(BONE, 4);
        g.interior = BLD_WORKSHOP;
        check(g.nearShop(), "test setup: the hunter is inside the workshop");
        clickRecipe(R_BONE);
        check(g.p.wpn(W_BONE) == 4, "in the workshop the same screen crafts weapons");
        g.interior = -1;
        g.craftAnywhere = true;
        // ---- the quest about crafting a weapon only counts weapons
        g.questIdx = 4;
        g.refreshQuest();
        g.questActive = true;
        g.questProg = 0;
        g.p.inv.add(BONE, 6);
        g.p.inv.add(itemMat(M_FEATHER), 4);
        g.craft(R_ARROW);
        check(g.questProg == 0, "crafting arrows does not count for 'craft a weapon'");
        g.p.inv.add(HIDE, 2);
        g.craft(R_BONE);
        check(g.questProg == 1, "crafting a weapon does");
        // ---- every screen renders in every state without trouble
        bool rendered = true;
        for (int lvl = 0; lvl <= PK_COUNT; lvl++)
            for (int tab = 0; tab < 2; tab++)
                for (int fill = 0; fill < 2; fill++) {
                    g.p.bag = lvl - 1;
                    g.p.syncPack();
                    g.p.inv.maxWeight = 1e6f;
                    g.p.inv.clear();
                    if (fill)
                        for (int i = 0; i < g.p.bagCap(); i++) g.p.inv.add(i < ITEM_COUNT ? i : itemWpn(W_BOW), 1);
                    g.invTab = tab;
                    g.invSel = g.p.bagCap() - 1;
                    g.state = ST_INVENTORY;
                    g.render();
                    rendered = rendered && g.state == ST_INVENTORY;
                }
        check(rendered, "all tabs render with no bag and every bag, empty and completely full (100 slots)");
        g.state = ST_PLAY;
    }
    // ---- saving
    printf("saving and migration\n");
    check(!std::getenv("WILDWOOD_SAVE_DIR") || g.savePath().rfind(std::getenv("WILDWOOD_SAVE_DIR"), 0) == 0, "the suites use their own save folder, not the player's real save");
    auto writeFile = [&](const std::string& text) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out << text;
    };
    auto removeAll = [&]() {
        std::remove(g.savePath().c_str());
        std::remove((g.savePath() + ".v1.bak").c_str());
        std::remove((g.savePath() + ".tmp").c_str());
        std::remove((g.savePath() + ".bak").c_str());
    };
    removeAll();
    {   // v2 round trip, including a non-default backpack, partial stacks and several stacks of one item
        g.newGame();
        g.p.bag = PK_SATCHEL;
        g.p.syncPack();
        g.p.inv.maxWeight = 1e6f;  // 250 meat is 200 kg: more than a satchel allows, but a loaded save must keep every item
        g.p.hot[1] = itemWpn(W_BOW);
        g.p.hot[4] = itemCons(C_SALVE);
        g.p.inv.clear();
        g.p.inv.add(MEAT, 250);
        g.p.inv.add(itemAmmo(AM_ARROW), 37);
        g.p.inv.add(itemWpn(W_BOW), 1);
        g.p.inv.add(itemWpn(W_FLINT), 31);
        g.p.inv.add(itemCons(C_SALVE), 2);
        g.p.hot[0] = itemWpn(W_FLINT);
        g.p.coins = 777;
        g.save();
        Player keep = g.p;
        g.newGame();
        bool ld = g.load();
        bool same = ld && g.loadReport.version == 2 && !g.loadReport.migrated && g.loadReport.dropped == 0 && g.p.coins == 777 && g.p.bagCap() == 100 && g.p.bag == PK_SATCHEL && g.p.carryLimit() == 100;
        same = same && g.p.hot[0] == itemWpn(W_FLINT) && g.p.hot[1] == itemWpn(W_BOW) && g.p.hot[4] == itemCons(C_SALVE) && g.p.hot[2] < 0;
        for (int i = 0; i < ITEM_COUNT; i++) same = same && g.p.inv.count(i) == keep.inv.count(i);
        same = same && g.p.bagUsed() == keep.bagUsed();
        check(same, "v2 save / load round trip: coins, worn bag, hotbar assignments and every item count identical (even above the weight limit)");
        std::ifstream f(g.savePath());
        std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        check(all.find("version 2") != std::string::npos && all.find("slot raw_meat 250") != std::string::npos && all.find("slot hunting_bow 1") != std::string::npos, "the file is readable text: version line and \"slot <name> <count>\" lines");
        check(!std::ifstream(g.savePath() + ".tmp"), "no temporary file left behind by the write-then-rename save");
    }
    {   // v1 (current layout) migration
        removeAll();
        std::string v1 = "coins 321\nmats 1 2 3 4 5 6 7 8 9 10 11\nwpn 8 3 2 4 1 1\nammo 25 9\nup 2 1 0 1\nbandages 4\nday 99.5\nkills 17\nquest 2 1 3\ndisc 10100000000000\n";
        writeFile(v1);
        bool ld = g.load();
        bool same = ld && g.loadReport.migrated && g.loadReport.version == 1 && g.loadReport.dropped == 0 && g.p.coins == 321 && g.p.bag == PK_SATCHEL && g.p.up[U_BAG] == 0 && g.p.bagCap() == 100 && g.p.carryLimit() == 100 && g.p.up[U_TOOL] == 2 && g.kills == 17;
        const int exMat[M_COUNT] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
        for (int m = 0; m < M_COUNT; m++) same = same && g.p.mat(m) == exMat[m];
        const int exW[W_COUNT] = {8, 3, 2, 4, 1, 1};
        for (int w = 0; w < W_COUNT; w++) same = same && g.p.wpn(w) == exW[w];
        same = same && g.p.ammo(AM_ARROW) == 25 && g.p.ammo(AM_BOLT) == 9 && g.p.bandages() == 4;
        check(same, "a v1 save migrates: every material, weapon, ammo type, bandage, coin and upgrade preserved (its backpack level becomes the worn bag)");
        check(g.p.hotSlotOf(itemCons(C_BANDAGE)) >= 0 && g.p.equippedWpn(W_JAVELIN) && g.p.hotSlotOf(itemWpn(W_BOW)) >= 0, "...and what it owned is equipped on the hotbar once (strongest weapons first)");
        std::ifstream b(g.savePath() + ".v1.bak");
        std::string bak((std::istreambuf_iterator<char>(b)), std::istreambuf_iterator<char>());
        check(bak == v1, "the original v1 file was backed up byte-for-byte as save.v1.bak");
        g.save();
        removeAll();
        check(true, "(cleanup)");
    }
    {   // oldest layout: 7 materials, 4 weapons, no ammo / quest / disc lines
        std::string old = "coins 50\nmats 3 2 1 0 0 0 0\nwpn 5 0 0 2\nup 0 0 0 0\nbandages 1\nday 10\nkills 2\n";
        writeFile(old);
        bool ld = g.load();
        bool ok = ld && g.p.mat(M_MEAT) == 3 && g.p.mat(M_HIDE) == 2 && g.p.mat(M_BONE) == 1 && g.p.wpn(W_FLINT) == 5 && g.p.wpn(W_JAVELIN) == 2 &&
                  g.p.wpn(W_BOW) == 0 && g.p.mat(M_FEATHER) == 0 && g.p.ammo(AM_ARROW) == 0 && g.p.bandages() == 1 && g.p.coins == 50;
        check(ok, "an early v1 save (7 materials, 4 weapons, no ammo line) migrates; the new items start at zero");
        removeAll();
    }
    {   // damaged v1: garbage, negative and enormous numbers
        writeFile("coins -5\nmats 4 abc 9 -7 999999999 1\nwpn 1000000 2\nammo\nbandages zz\nkills 3\n");
        bool ld = g.load();
        bool sane = ld && g.p.coins >= 0 && g.p.bagUsed() <= g.p.bagCap();
        for (int m = 0; m < M_COUNT; m++) sane = sane && g.p.mat(m) >= 0;
        sane = sane && g.p.mat(M_MEAT) == 4 && g.p.mat(M_BONE) == 0 && g.p.mat(M_ORGAN) == 0;  // "abc" stops that line; the rest stay zero
        check(sane, "a damaged v1 save loads without crashing, no negative counts, and never overfills the bag");
        check(g.loadReport.dropped > 0, "items that cannot fit (absurd counts) are reported as dropped instead of lost silently");
        removeAll();
    }
    {   // saving twice keeps the previous save; a save lost between the two renames is recovered; odd day / kills / quest numbers are clamped
        removeAll();
        g.newGame();
        g.p.coins = 111;
        g.save();
        g.p.coins = 222;
        g.save();
        std::ifstream b(g.savePath() + ".bak");
        std::string bs((std::istreambuf_iterator<char>(b)), std::istreambuf_iterator<char>());
        check(bs.find("coins 111") != std::string::npos && !std::ifstream(g.savePath() + ".tmp"), "a second save keeps the first as save.txt.bak (and leaves no .tmp)");
        std::remove(g.savePath().c_str());  // as if the game stopped between the two renames
        check(g.load() && g.p.coins == 111, "a missing save is recovered from save.txt.bak");
        removeAll();
        writeFile("version 2\ncoins 5\nup 0 0 0 0\nday 1e30\nkills -9\nquest -4 1 -7\n");
        bool ld = g.load();
        check(ld && std::isfinite(g.dayT) && g.dayT >= 0.f && g.dayT <= 1.0e8f && g.kills == 0 && g.questIdx == 0, "absurd day / kills / quest values load as safe numbers");
        removeAll();
        writeFile("version 2\ncoins 5\nup 0 0 0 0\nday -50\nkills 99999999999\nquest 99999999 1 3\n");
        ld = g.load();
        check(ld && g.dayT == 0.f && g.kills == 1000000000 && g.questIdx == 1000000, "negative day and enormous kills / quest index are clamped");
        removeAll();
    }
    {   // v2 with an item from the future, and more slots than the pack has
        std::string s2 = "version 2\ncoins 10\nup 0 0 0 0\nslot raw_meat 12\nslot unicorn_horn 3\nslot hide 4\n";
        for (int i = 0; i < 105; i++) s2 += std::string("slot hunting_bow 1\n");
        writeFile(s2);
        bool ld = g.load();
        check(ld && g.loadReport.unknown == 1 && g.p.mat(M_MEAT) == 12 && g.p.mat(M_HIDE) == 4, "an unknown item in a v2 file is skipped; the rest of the save loads");
        check(g.loadReport.dropped == 105 - (100 - 2) && g.p.bagUsed() == 100, "more stacks than the pack holds: what fits is kept, the rest is counted as dropped");
        removeAll();
    }
    {   // not a save at all
        writeFile("hello world\nthis is not a save\n");
        g.p.coins = 4242;
        bool ld = g.load();
        check(!ld && g.loadReport.version == 0, "a file that is not a save is rejected (load returns false)");
        removeAll();
        check(!g.load(), "no file: load returns false");
    }
    // ======================================================== gathering, crafting, structures
    printf("gathering, crafting and structures\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.helpT = 0;
        g.p.inv.clear();
        auto recipeFor = [&](int kind, int out, int firstMat, int count = -1) {
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == kind && RECIPES[r].out == out && RECIPES[r].mat[0] == firstMat && (count < 0 || RECIPES[r].count == count)) return r;
            return -1;
        };
        // stand beside resource `rs` such that findHarvest picks exactly it
        auto standAt = [&](int rs, bool& node, int& idx, int skip = 0) {
            for (int pass = 0; pass < 2; pass++) {
                const bool wantNode = pass == 0;
                const size_t n = wantNode ? g.world.nodes.size() : g.world.props.size();
                for (size_t i = 0; i < n; i++) {
                    const int res = wantNode ? nodeRes(g.world.nodes[i].kind) : g.propRsId[i];
                    if (res != rs) continue;
                    const float x = wantNode ? g.world.nodes[i].x : g.world.props[i].x, y = wantNode ? g.world.nodes[i].y : g.world.props[i].y;
                    g.p.pos = V2(x, y + 12);
                    HTarget t;
                    if (g.findHarvest(t) && t.node == wantNode && t.idx == (int)i && skip-- <= 0) {
                        node = wantNode;
                        idx = (int)i;
                        return true;
                    }
                }
            }
            return false;
        };
        auto swingUntilGone = [&](bool node, int idx, int& steps) {
            steps = 0;
            while (!g.hstate(node, idx).gone && steps < 4000) {
                HTarget t;
                if (!g.findHarvest(t) || t.idx != idx || t.node != node) return false;
                g.harvestStep(t, 0.05f);
                steps++;
            }
            return g.hstate(node, idx).gone;
        };
        auto canHarvest = [&](int rs) {
            const ResDef& RD = RES[rs];
            return RD.tool < 0 || g.bestTool(RD.tool) >= RD.minTier;
        };
        bool node = false;
        int idx = -1, steps = 0;

        // ---- tool tiers gate ore (iron is found in the mountain map)
        g.travelTo(B_MOUNTAIN, MAPS[B_MOUNTAIN].entryTx, MAPS[B_MOUNTAIN].entryTy, false);
        g.travelCd = 0;
        check(standAt(RS_IRON, node, idx), "an iron vein exists and can be reached by line of sight");
        check(!canHarvest(RS_IRON), "iron cannot be mined with bare hands");
        g.p.inv.add(itemTool(TL_PICK1), 1);
        check(!canHarvest(RS_IRON), "a stone pickaxe (tier 1) is not enough for iron");
        g.p.inv.add(itemTool(TL_PICK2), 1);
        check(canHarvest(RS_IRON) && !canHarvest(RS_GOLD), "a copper pickaxe mines iron but not gold");
        check(g.bestTool(TT_PICK) == 2 && g.bestTool(TT_AXE) == 0, "bestTool picks the highest tier of the right type");
        // ---- swing, finish, reward, depletion
        const int ironBefore = g.p.mat(M_IRON);
        const bool done = swingUntilGone(node, idx, steps);
        check(done, "holding the swing finishes the vein");
        check(g.p.mat(M_IRON) > ironBefore, "finishing the vein pays out iron ore");
        check(g.p.sk.s[SK_MINING].xp == 2 + RES[RS_IRON].hits * 2 + RES[RS_IRON].minTier * 4, "finishing a vein earns Mining XP once, not per swing");
        const float minSecs = (RES[RS_IRON].hits - 1) * RES[RS_IRON].swing / TOOLDEF[TL_PICK2].speed;
        check(steps * 0.05f >= minSecs, "a vein takes at least hits x swing time (no free rewards)");
        {
            HTarget t;
            const bool still = g.findHarvest(t) && t.node == node && t.idx == idx;
            check(!still, "a depleted vein cannot be harvested again (no duplicate rewards)");
            const int had = g.p.mat(M_IRON);
            HTarget fake;
            fake.valid = true; fake.node = node; fake.idx = idx; fake.res = RS_IRON; fake.pos = g.p.pos;
            for (int i = 0; i < 200; i++) g.harvestStep(fake, 0.05f);
            check(g.p.mat(M_IRON) == had, "forcing swings at a depleted vein gives nothing");
        }
        // ---- respawn timing
        {
            for (float t = 0; t < RES[RS_IRON].respawn * 0.5f; t += 1.f) g.updateHarvest(1.f);
            check(g.hstate(node, idx).gone, "it is still depleted halfway through the respawn time");
            for (float t = 0; t < RES[RS_IRON].respawn * 0.7f + REGROW_FADE + 2.f; t += 1.f) g.updateHarvest(1.f);
            check(g.harvestUp(node, idx), "it has grown back after the respawn time");
            HTarget t;
            check(g.findHarvest(t) && t.idx == idx, "and can be harvested again");
        }
        g.travelTo(B_HOME, MAPS[B_MOUNTAIN].portals[0].arriveTx, MAPS[B_MOUNTAIN].portals[0].arriveTy, false);  // back to the home map for the trees and herbs
        g.travelCd = 0;
        // ---- trees: solid tile cleared while felled, restored when regrown, border trees stay
        {
            g.p.inv.add(itemTool(TL_AXE1), 1);
            check(standAt(RS_OAK, node, idx, 5), "an oak is reachable");
            const Prop& pr = g.world.props[idx];
            const int tx = (int)(pr.x / TS), ty = (int)(pr.y / TS);
            check(g.world.solid[ty][tx], "a standing oak is a solid tile");
            const int woodBefore = g.p.mat(M_WOOD);
            check(swingUntilGone(node, idx, steps), "an oak can be chopped down");
            check(g.p.impactT > 0, "the blow that lands starts the hunter's impact / recovery pose");
            check(g.p.mat(M_WOOD) > woodBefore, "felling an oak gives wood");
            check(g.p.sk.s[SK_WOODCUTTING].xp == 2 + RES[RS_OAK].hits * 2 + RES[RS_OAK].minTier * 4, "felling an oak earns Woodcutting XP once");
            check(!g.world.solid[ty][tx], "a felled tree no longer blocks the way");
            g.p.pos = V2(pr.x + 120, pr.y + 120);  // walk away: nobody blocks the regrowth
            for (float t = 0; t < RES[RS_OAK].respawn * 1.3f + REGROW_FADE + 5.f; t += 1.f) g.updateHarvest(1.f);
            check(g.harvestUp(node, idx) && g.world.solid[ty][tx], "the oak regrows and is solid again");
        }
        {
            int bad = 0, harvestable = 0;
            for (size_t i = 0; i < g.world.props.size(); i++) {
                const int tx = (int)(g.world.props[i].x / TS), ty = (int)(g.world.props[i].y / TS);
                if (g.propRsId[i] >= 0) {
                    harvestable++;
                    if (protectedTile(tx, ty, g.world.mapId)) bad++;
                }
            }
            check(harvestable > 0 && bad == 0, "trees on the map border and around the home core are never harvestable");
        }
        // ---- no gathering through obstacles
        {
            check(standAt(RS_HERB, node, idx), "a wild herb is reachable");
            const V2 target(g.world.nodes[idx].x, g.world.nodes[idx].y - 4);
            // put a solid wall tile between the hunter and the plant
            const int wx = (int)((g.p.pos.x + target.x) / 2 / TS), wy = (int)((g.p.pos.y + target.y) / 2 / TS);
            const bool had = g.world.solid[wy][wx];
            const int ptx = (int)(target.x / TS), pty = (int)(target.y / TS);
            if ((wx != ptx || wy != pty)) {
                g.world.solid[wy][wx] = true;
                g.p.pos = target + V2(0, 24);
                g.world.solid[(int)((g.p.pos.y + target.y) / 2 / TS)][(int)(target.x / TS)] = true;
                HTarget t;
                const bool seen = g.findHarvest(t) && t.node && t.idx == idx;
                check(!seen, "a solid tile between the hunter and the plant blocks gathering");
                g.world.solid[(int)((g.p.pos.y + target.y) / 2 / TS)][(int)(target.x / TS)] = had;
            }
            g.world.solid[wy][wx] = had;
        }
        // ---- a full bag never swallows a resource
        {
            check(standAt(RS_HERB, node, idx, 1), "another herb is reachable");
            g.p.inv.clear();
            g.p.inv.maxWeight = 1e6f;
            for (int i = 0; i < g.p.bagCap(); i++) g.p.inv.add(itemWpn(W_BOW), 1);
            check(g.p.bagUsed() == g.p.bagCap(), "test setup: the bag is full");
            for (int i = 0; i < 200; i++) {
                HTarget t;
                if (!g.findHarvest(t)) break;
                g.harvestStep(t, 0.05f);
            }
            check(!g.hstate(node, idx).gone && g.p.mat(M_HERB) == 0, "with a full bag the plant stays and nothing is lost");
            g.p.inv.clear();
            check(swingUntilGone(node, idx, steps) && g.p.mat(M_HERB) > 0, "with room again the same plant gives its reward");
        }
        // ---- crafting rules
        g.p.inv.clear();
        g.structs.clear();
        {
            // a spot well away from the village: nothing is "near" a fire or the shop there
            bool found = false;
            for (int ty = 20; ty < MH - 20 && !found; ty += 5)
                for (int tx = 20; tx < MW - 20 && !found; tx += 5) {
                    g.p.pos = V2(tx * TS + 8.f, ty * TS + 8.f - 18.f);
                    g.p.dir = 0;
                    if (dist(g.p.pos, g.world.fire) > 150 && dist(g.p.pos, g.world.keeper) > 150 && g.canPlaceAt(tx, ty) && !g.world.solidTile(tx, ty - 2)) found = true;
                }
            check(found, "test setup: an open spot far from the village");
            const int rCamp = recipeFor(RK_ITEM, itemPlace(PL_CAMPFIRE), M_WOOD);
            const int rCopperBar = recipeFor(RK_ITEM, itemMat(M_COPPERBAR), M_COPPER);
            const int rRope = recipeFor(RK_ITEM, itemMat(M_ROPE), M_FIBER);
            const int rCuAxe = recipeFor(RK_ITEM, itemTool(TL_AXE2), M_WOOD);
            const int rArmor1 = recipeFor(RK_GEAR, U_ARMOR, M_HIDE, 1), rArmor2 = recipeFor(RK_GEAR, U_ARMOR, M_FUR, 2);
            const int rIronAxe = recipeFor(RK_ITEM, itemTool(TL_AXE3), M_WOOD);
            check(rCamp >= 0 && rCopperBar >= 0 && rRope >= 0 && rCuAxe >= 0 && rArmor1 >= 0 && rArmor2 >= 0 && rIronAxe >= 0, "the key recipes are all in the table");
            check(g.craftBlock(rCamp) == "MISSING MATERIALS", "a recipe without its materials says so");
            // every recipe in the table produces something real and names real ingredients
            int badRecipes = 0;
            for (int r = 0; r < RECIPE_N; r++) {
                const Recipe& R = RECIPES[r];
                bool ok = R.count > 0 && !recipeName(R).empty();
                int ing = 0;
                for (int k = 0; k < 3; k++)
                    if (R.mat[k] >= 0) {
                        ing++;
                        ok = ok && R.mat[k] < M_COUNT && R.n[k] > 0;
                    }
                ok = ok && ing > 0;
                if (R.kind != RK_GEAR && R.kind != RK_BANDAGE) ok = ok && recipeItem(R) > 0 && recipeItem(R) < ITEM_COUNT;
                if (!ok) {
                    badRecipes++;
                    printf("    bad recipe #%d\n", r);
                }
            }
            check(badRecipes == 0, "every recipe has valid ingredients and a real output");
            // each raw material that a recipe needs can be gathered, dropped or made
            bool producible[M_COUNT] = {false};
            for (int rs = 0; rs < RS_COUNT; rs++)
                for (int k = 0; k < 3; k++)
                    if (RES[rs].y[k].mat >= 0) producible[RES[rs].y[k].mat] = true;
            for (int a = 0; a < A_COUNT; a++)
                for (int m = 0; m < M_COUNT; m++)
                    if (ANIMAL[a].drop[m][1] > 0) producible[m] = true;
            for (int k = 0; k < MK_COUNT; k++)  // the delve creatures' drops
                for (const MonDrop& d : MONDEF(k).drops)
                    if (d.chance > 0) producible[d.mat] = true;
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == RK_ITEM && item(RECIPES[r].out).kind == IK_MATERIAL) producible[item(RECIPES[r].out).ref] = true;
            for (int m : {M_ROSE, M_TULIP, M_LAVENDER, M_SUNFLOWER}) producible[m] = true;  // bought at the flower shop
            int unmakeable = 0;
            for (int r = 0; r < RECIPE_N; r++)
                for (int k = 0; k < 3; k++)
                    if (RECIPES[r].mat[k] >= 0 && !producible[RECIPES[r].mat[k]]) {
                        unmakeable++;
                        printf("    recipe #%d needs %s, which nothing provides\n", r, MAT_NAME[RECIPES[r].mat[k]]);
                    }
            check(unmakeable == 0, "every ingredient of every recipe can actually be obtained");

            g.p.inv.add(itemMat(M_COPPER), 2);
            g.p.inv.add(itemMat(M_WOOD), 12);
            g.p.inv.add(itemMat(M_STONE), 6);
            g.craftAnywhere = false;
            check(g.craftBlock(rCopperBar) == "MADE AT THE BLACKSMITH", "smelting outdoors is refused: it is done at the blacksmith");
            check(!g.craft(rCopperBar) && g.p.mat(M_COPPER) == 2, "...and consumes nothing");
            g.interior = BLD_WORKSHOP;
            check(g.craftBlock(rCopperBar) == "MADE AT THE BLACKSMITH", "the wrong building refuses it too");
            g.interior = -1;
            g.craftAnywhere = true;
            check(g.craft(rCamp) && g.p.inv.count(itemPlace(PL_CAMPFIRE)) == 1 && g.p.mat(M_STONE) == 3, "a campfire is crafted from wood and stone");
            check(g.placeStructure(PL_CAMPFIRE) && g.structs.size() == 1 && g.p.inv.count(itemPlace(PL_CAMPFIRE)) == 0, "the campfire is placed");
            check(g.nearFire(), "standing by the placed campfire counts as a fire");
            g.craftAnywhere = false;
            g.interior = BLD_SMITH;
            check(g.craft(rCopperBar) && g.p.mat(M_COPPERBAR) == 1 && g.p.mat(M_COPPER) == 0, "copper smelts into a bar at the blacksmith");
            g.interior = -1;
            g.craftAnywhere = true;
            // rope, then the copper axe
            g.p.inv.add(itemMat(M_FIBER), 3);
            check(g.craft(rRope) && g.p.mat(M_ROPE) == 1 && g.p.mat(M_FIBER) == 0, "fibre twists into rope");
            check(g.p.mat(M_COPPERBAR) == 1, "(bar still in the bag)");
            g.p.inv.add(itemMat(M_COPPERBAR), 1);
            check(g.craft(rCuAxe) && g.p.inv.count(itemTool(TL_AXE2)) == 1, "a copper axe is crafted from wood, a copper bar and rope");
            g.p.inv.add(itemMat(M_COPPERBAR), 2);
            g.p.inv.add(itemMat(M_ROPE), 1);
            g.p.inv.add(itemMat(M_WOOD), 2);
            check(g.craftBlock(rCuAxe) == "YOU ALREADY HAVE ONE", "a second copy of a single tool is refused");
            // iron tools need the shop's forge
            g.p.inv.add(itemMat(M_IRONBAR), 2);
            g.craftAnywhere = false;
            check(g.craftBlock(rIronAxe) == "MADE AT THE BLACKSMITH", "iron tools are made at the blacksmith");
            g.craftAnywhere = true;
            // armor order
            g.p.inv.add(itemMat(M_FUR), 4);
            g.p.inv.add(itemMat(M_HIDE), 12);
            g.p.inv.add(itemMat(M_FIBER), 2);
            check(g.craftBlock(rArmor2) == "MAKE THE PREVIOUS LEVEL FIRST", "armor level 2 needs level 1 first");
            check(g.craft(rArmor1) && g.p.up[U_ARMOR] == 1, "hide armor is worn at once");
            check(g.craft(rArmor2) && g.p.up[U_ARMOR] == 2, "then the second level");
            check(g.craftBlock(rArmor1) == "YOU WEAR BETTER ALREADY" || g.craftBlock(rArmor1) == "MISSING MATERIALS", "an older armor level cannot be crafted again");
            // consumables
            g.p.inv.clear();
            g.p.hp = g.p.maxhp - 50;
            g.p.inv.add(itemCons(C_SALVE), 1);
            g.p.inv.add(itemCons(C_BREW), 1);
            g.p.inv.add(itemCons(C_CACTUSWATER), 1);
            const float h0 = g.p.hp;
            check(g.useConsumable(C_SALVE) && g.p.hp > h0 && g.p.inv.count(itemCons(C_SALVE)) == 0, "a salve heals and is used up");
            g.p.hp = g.p.maxhp;
            check(!g.useConsumable(C_TONIC), "using something you do not own does nothing");
            check(g.useConsumable(C_BREW) && g.p.warmT > 100.f, "a warm brew gives warmth");
            check(g.useConsumable(C_CACTUSWATER) && g.p.coolT > 60.f && g.p.stamina >= 99.f, "cactus water refreshes");
            // chest: move items both ways, keeps the stack rules
            g.p.inv.clear();
            g.p.inv.add(itemMat(M_WOOD), 5);
            g.p.inv.add(itemMat(M_WOOD), 0);
            g.p.inv.add(itemPlace(PL_CHEST), 1);
            check(g.placeStructure(PL_CHEST) && g.structs.size() == 2, "a chest is placed");
            g.chestOpen = 1;
            int woodSlot = -1;
            for (int i = 0; i < g.p.bagCap(); i++)
                if (g.p.inv.slots[i].item == itemMat(M_WOOD)) woodSlot = i;
            check(woodSlot >= 0 && g.chestMove(1, woodSlot) && g.p.mat(M_WOOD) == 0 && g.structs[1].inv.count(itemMat(M_WOOD)) == 5, "wood moves from the pack into the chest");
            // save / load: depleted resources, structures and chest contents come back
            check(standAt(RS_HERB, node, idx, 2) && swingUntilGone(node, idx, steps), "(setup) one more plant is picked");
            g.save();
            const size_t nStructs = g.structs.size();
            g.structs.clear();
            g.resetHarvest();
            check(g.harvestUp(node, idx), "(setup) after a reset the plant is back");
            check(g.load(), "the save loads");
            check(g.structs.size() == nStructs, "placed structures survive a save / load");
            bool chestOk = false;
            for (auto& s : g.structs)
                if (s.kind == PL_CHEST && s.inv.count(itemMat(M_WOOD)) == 5) chestOk = true;
            check(chestOk, "chest contents survive a save / load");
            check(g.hstate(node, idx).gone, "a picked plant is still waiting to regrow after a save / load");
        }
    }
    printf("consumables and skill-gated crafting\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.clear();
        auto cons = [&](int c) { g.p.inv.add(itemCons(c), 1); };
        g.p.maxhp = 200;
        g.p.hp = 20;
        cons(C_POULTICE);
        check(g.useConsumable(C_POULTICE) && (int)g.p.hp == 80 && g.p.inv.count(itemCons(C_POULTICE)) == 0, "a poultice heals 60 and is used up");
        cons(C_MEDICINE);
        check(g.useConsumable(C_MEDICINE) && (int)g.p.hp == 200, "herbal medicine heals 120 (capped at max health)");
        cons(C_POULTICE);
        check(!g.useConsumable(C_POULTICE) && g.p.inv.count(itemCons(C_POULTICE)) == 1, "healing items are refused (and kept) at full health");
        g.p.hp = 100;
        g.p.stamina = 5.f;
        g.p.exhausted = true;
        cons(C_JERKY);
        check(g.useConsumable(C_JERKY) && (int)g.p.hp == 120 && g.p.stamina >= 99.f && !g.p.exhausted, "dried meat heals 20 and restores stamina");
        g.p.stamina = 5.f;
        cons(C_STEW);
        check(g.useConsumable(C_STEW) && (int)g.p.hp == 200 && g.p.stamina >= 99.f, "hunter's stew heals 85 and restores stamina");
        g.p.hp = 50;
        cons(C_VIGOR);
        check(g.useConsumable(C_VIGOR) && (int)g.p.hp == 105 && g.p.swiftT > 85.f, "velvet tonic heals 55 and grants a speed boost");
        cons(C_NIGHTSIGHT);
        check(g.useConsumable(C_NIGHTSIGHT) && g.p.nightT > 295.f, "nightsight tonic starts a five-minute timer");
        cons(C_NIGHTSIGHT);
        check(!g.useConsumable(C_NIGHTSIGHT) && g.p.inv.count(itemCons(C_NIGHTSIGHT)) == 1, "a second nightsight tonic is refused while one is active (nothing wasted)");
        // skill gating
        auto find = [&](int kind, int out, int firstMat, int count) {
            for (int r = 0; r < RECIPE_N; r++)
                if (RECIPES[r].kind == kind && RECIPES[r].out == out && RECIPES[r].mat[0] == firstMat && RECIPES[r].count == count) return r;
            return -1;
        };
        const int rPoul = find(RK_ITEM, itemCons(C_POULTICE), M_EXTRACT, 2);
        check(rPoul >= 0, "(setup) the poultice recipe exists");
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.clear();
        g.p.inv.add(itemMat(M_EXTRACT), 4);
        g.p.inv.add(itemMat(M_FIBER), 4);
        check(g.craftBlock(rPoul) == "NEEDS ALCHEMY 3", "a recipe above your skill level says which skill and level it needs");
        check(!g.craft(rPoul) && g.p.mat(M_EXTRACT) == 4 && g.p.mat(M_FIBER) == 4 && g.p.inv.count(itemCons(C_POULTICE)) == 0, "...and a locked recipe consumes nothing");
        g.p.sk.s[SK_ALCHEMY].xp = xpForLevel(1) + xpForLevel(2);  // exactly level 3
        check(g.p.sk.level(SK_ALCHEMY) == 3 && g.craftBlock(rPoul).empty(), "reaching the level unlocks it");
        const int64_t before = g.p.sk.s[SK_ALCHEMY].xp;
        check(g.craft(rPoul) && g.p.inv.count(itemCons(C_POULTICE)) == 2 && g.p.mat(M_EXTRACT) == 3 && g.p.mat(M_FIBER) == 3, "crafting delivers the result and spends the ingredients");
        check(g.p.sk.s[SK_ALCHEMY].xp == before + recipeXp(RECIPES[rPoul]), "a successful craft earns the recipe's XP in the recipe's skill");
        g.p.inv.clear();
        const int64_t b2 = g.p.sk.s[SK_ALCHEMY].xp;
        check(!g.craft(rPoul) && g.p.sk.s[SK_ALCHEMY].xp == b2, "a failed craft earns no XP");
    }
    printf("gear effects and recipe audit\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.p.inv.clear();
        float dr = 1, rg = 1;
        g.curBiome = B_DESERT;
        g.dayT = 0.5f;
        g.climateMods(dr, rg);
        const float plainHeat = dr;
        g.p.inv.add(itemCarry(CG_DESERTWRAPS), 1);
        g.climateMods(dr, rg);
        check(plainHeat > 1.f && dr < plainHeat && dr >= 1.f, "desert wraps cut the heat drain while carried");
        g.p.inv.clear();
        g.curBiome = B_SNOW;
        g.p.warmT = 0;
        g.climateMods(dr, rg);
        const float cold = dr;
        g.p.inv.add(itemCarry(CG_WOLFCLOAK), 1);
        g.climateMods(dr, rg);
        const float cloak = dr;
        g.p.inv.add(itemCarry(CG_PARKA), 1);
        g.climateMods(dr, rg);
        check(cold > cloak && cloak > dr && dr == 1.f, "the wolf cloak halves the cold and the parka removes it");
        g.p.inv.clear();
        g.curBiome = B_MOUNTAIN;
        g.climateMods(dr, rg);
        const float thin = dr;
        g.p.inv.add(itemCarry(CG_HARNESS), 1);
        g.climateMods(dr, rg);
        check(dr < thin, "the climbing harness eases thin air");
        // snake wraps: only snake bites are softened
        g.p.inv.clear();
        g.p.hp = g.p.maxhp = 200;
        g.hurtPlayer(20, V2(0, 0), A_RATTLER);
        const float plain = 200 - g.p.hp;
        g.p.hp = 200;
        g.p.invuln = 0;
        g.p.inv.add(itemCarry(CG_VENOMWRAPS), 1);
        g.hurtPlayer(20, V2(0, 0), A_RATTLER);
        const float wrapped = 200 - g.p.hp;
        g.p.hp = 200;
        g.p.invuln = 0;
        g.hurtPlayer(20, V2(0, 0), A_WOLF);
        check(wrapped < plain && 200 - g.p.hp == plain, "venom wraps soften snake bites but not other attacks");
        // gear tiers
        g.p.inv.clear();
        bool tiers = true;
        for (int u = 0; u < 4; u++) tiers = tiers && (u == U_BAG || std::string(gearName(u, UP_MAX[u])).size() > 0);  // bags are items now
        check(tiers, "every gear line's top level has a name");
        g.newGame();
        g.state = ST_PLAY;
        g.p.sk.s[SK_LEATHER].xp = SkillState::capXp();
        g.p.inv.add(itemMat(M_LEATHER), 8);
        g.p.inv.add(itemMat(M_ROPE), 4);
        g.p.inv.add(itemMat(M_MARMOTPELT), 3);
        int rPack = -1;
        for (int r = 0; r < RECIPE_N; r++)
            if (RECIPES[r].kind == RK_ITEM && RECIPES[r].out == itemPack(PK_EXPLORER)) rPack = r;
        g.p.sk.s[SK_LEATHER].xp = 0;
        check(rPack >= 0 && g.craftBlock(rPack).find("NEEDS LEATHERWORK") == 0, "the explorer pack needs leatherwork skill");
        g.p.sk.s[SK_LEATHER].xp = SkillState::capXp();
        g.p.pos = g.world.fire;  // the recipe needs a campfire
        check(g.craft(rPack) && g.p.inv.count(itemPack(PK_EXPLORER)) == 1 && g.p.bag < 0, "crafting the explorer pack makes a bag item (worn from the pack, not automatically)");
        check(g.craftBlock(rPack).find("MISSING") == 0 || g.craftBlock(rPack).find("ALREADY") != std::string::npos, "a second copy of a bag you already own is refused");
        // audit
        bool valid = true, skilled = true;
        bool used[M_COUNT] = {};
        for (int r = 0; r < RECIPE_N; r++) {
            const Recipe& R = RECIPES[r];
            valid = valid && R.count > 0 && !recipeName(R).empty();
            skilled = skilled && R.skill >= 0 && R.skill < SK_COUNT && R.minLv >= 1 && R.minLv <= MAX_LEVEL;
            for (int i = 0; i < 3; i++)
                if (R.mat[i] >= 0) {
                    valid = valid && R.mat[i] < M_COUNT && R.n[i] > 0;
                    used[R.mat[i]] = true;
                }
        }
        check(valid && skilled, "every recipe has a name, a count, valid ingredients and a valid skill level");
        bool trophiesUsed = true;
        for (int m = M_TROPHY0; m < M_TROPHY0 + A_COUNT; m++)
            if (!used[m]) {
                trophiesUsed = false;
                printf("    %s is used by no recipe\n", MAT_NAME[m]);
            }
        check(trophiesUsed, "every animal's exclusive material is an ingredient in at least one recipe");
        bool compUsed = true;
        for (int m : {M_PLANK, M_LEATHER, M_BONEFRAG, M_EXTRACT, M_GREASE, M_FAT})
            if (!used[m]) {
                compUsed = false;
                printf("    %s is used by no recipe\n", MAT_NAME[m]);
            }
        check(compUsed, "every processed material is consumed by a recipe");
        bool biomeOk = true;
        for (int w = W_FANGDAGGER; w < W_COUNT; w++) biomeOk = biomeOk && (WPN[w].biome < 0 || WPN[w].biomeMul > 1.f);
        check(biomeOk, "every biome weapon has a damage bonus for its biome");
    }
    printf("separate maps and travel\n");
    {
        auto packTotal = [&]() {
            long n = 0;
            for (auto& sl : g.p.inv.slots) n += sl.empty() ? 0 : sl.n;
            return n;
        };
        auto spotOk = [&]() {
            const V2 q = g.p.pos;
            return !g.world.blocked(q.x, q.y, 5.f) && q.x > 2 * TS && q.y > 2 * TS && q.x < (MW - 2) * TS && q.y < (MH - 2) * TS;
        };
        check(MW == 240 && MH == 168 && CW == 88 && CH == 60, "every map is 240x168 tiles; the original 88x60 Wildwood area sits inside the home map");
        bool allSame = true, allWalled = true, pure = true;
        for (int m = 0; m < B_COUNT; m++) {
            g.loadMapWorld(m);
            allSame = allSame && g.world.mapId == m && g.world.pixW() == 240 * TS && g.world.pixH() == 168 * TS;
            for (int y = 0; y < MH; y++)
                for (int x = 0; x < MW; x++) {
                    pure = pure && g.world.biome[y][x] == m;
                    if ((x < 2 || y < 2 || x >= MW - 2 || y >= MH - 2) && !g.world.solidTile(x, y) && !portalAt(m, x, y)) allWalled = false;
                }
        }
        check(allSame && pure, "all five maps have the same pixel size and only their own biome");
        check(allWalled, "every map's edge wall is closed except at its gates");
        g.loadMapWorld(B_HOME);
        bool homeSame = true;  // the same seed always builds the same map
        {
            World a, b;
            a.generate(WORLD_SEED, B_DESERT);
            b.generate(WORLD_SEED, B_DESERT);
            homeSame = a.props.size() == b.props.size() && a.nodes.size() == b.nodes.size() && memcmp(a.tile, b.tile, sizeof a.tile) == 0;
        }
        check(homeSame, "a map is rebuilt identically every time it is entered");
        // ---- travel keeps the hunter, and each map keeps its own state
        g.newGame();
        g.state = ST_PLAY;
        g.p.coins = 500;
        g.p.inv.add(itemMat(M_WOOD), 7);
        g.p.inv.add(itemMat(M_STONE), 5);
        g.p.inv.add(itemPlace(PL_CAMPFIRE), 2);
        g.p.hp = 55;
        g.p.sk.s[SK_HUNTING].xp = 300;
        g.questIdx = 2;
        g.dayT = 77;
        const long packBefore = packTotal();
        check(g.world.mapId == B_HOME && g.placeStructure(PL_CAMPFIRE) && g.structs.size() == 1, "(setup) a campfire is placed in the home map");
        const int homeTx = g.structs[0].tx, homeTy = g.structs[0].ty;
        g.p.inv.add(itemPlace(PL_CAMPFIRE), 0);
        const long packMid = packTotal();
        check(packMid <= packBefore, "(setup) placing a structure never adds items to the pack");
        check(g.travelTo(B_DESERT, MAPS[B_DESERT].entryTx, MAPS[B_DESERT].entryTy, false) && g.world.mapId == B_DESERT && g.curBiome == B_DESERT, "travelling loads the desert map");
        check(spotOk(), "the hunter arrives on a free tile inside the map");
        check(packTotal() == packMid && g.p.coins == 500 && (int)g.p.hp == 55 && g.p.sk.s[SK_HUNTING].xp == 300 && g.questIdx == 2 && g.dayT == 77.f, "pack, coins, health, skills, quest and time are unchanged by a trip");
        check(g.structs.empty(), "the home campfire is not in the desert (maps do not mix)");
        g.travelCd = 0;
        check(g.placeStructure(PL_CAMPFIRE) && g.structs.size() == 1, "(setup) a campfire is placed in the desert");
        const int dTx = g.structs[0].tx, dTy = g.structs[0].ty;
        const long packNow = packTotal();  // (the placed campfire left the pack)
        check(g.world.landmarks.size() == 4 && g.landmarksFound() == 0, "the desert has its own four landmarks, none found yet");
        g.world.landmarks[1].found = true;
        int herb = -1;
        for (size_t i = 0; i < g.world.nodes.size(); i++)
            if (nodeRes(g.world.nodes[i].kind) >= 0) {
                herb = (int)i;
                break;
            }
        check(herb >= 0, "(setup) the desert has a gatherable node");
        g.hstate(true, herb).gone = true;
        g.hstate(true, herb).t = 500;
        g.hActive.push_back((1 << 28) | herb);
        g.items.push_back({g.p.pos + V2(20, 0), W_FLINT, 0.f, 0.f});
        const V2 hubPos = g.world.hub[B_DESERT];
        g.travelTo(B_HOME, MAPS[B_DESERT].portals[0].arriveTx, MAPS[B_DESERT].portals[0].arriveTy, false);
        check(g.world.mapId == B_HOME && spotOk(), "the way back leads home to a valid spot");
        check(g.structs.size() == 1 && g.structs[0].tx == homeTx && g.structs[0].ty == homeTy, "the home map still has only its own campfire");
        check(g.items.empty() && g.landmarksFound() == 0 && g.world.landmarks.empty(), "the desert's dropped weapon and landmarks are not in the home map");
        g.travelCd = 0;
        g.travelTo(B_DESERT, MAPS[B_DESERT].entryTx, MAPS[B_DESERT].entryTy, false);
        check(g.structs.size() == 1 && g.structs[0].tx == dTx && g.structs[0].ty == dTy && g.world.solid[dTy][dTx], "returning to the desert restores its campfire (and it blocks the tile)");
        check(g.hstate(true, herb).gone && g.landmarksFound() == 1 && g.world.landmarks[1].found, "...its harvested resource and its discovered landmark");
        check(g.items.size() == 1 && g.items[0].wpn == W_FLINT, "...and the weapon that lay on the ground");
        check(packTotal() == packNow && g.world.hub[B_DESERT].x == hubPos.x, "nothing was duplicated or lost over two round trips");
        g.travelCd = 0;
        check(!g.travelTo(-1, 5, 5, false) && !g.travelTo(B_COUNT, 5, 5, false) && g.world.mapId == B_DESERT, "an invalid destination is refused");
        g.p.dead = true;
        check(!g.travelTo(B_HOME, 40, 30, false) && g.world.mapId == B_DESERT, "a dead hunter cannot travel");
        g.p.dead = false;
        // ---- a gate trigger: standing in a gate's zone travels, once
        g.travelTo(B_HOME, 40, 30, false);
        g.travelCd = 0;
        {
            const Portal& east = MAPS[B_HOME].portals[0];
            g.p.pos = V2((east.zone.x0 + 0.5f) * TS, 84.5f * TS);
            g.checkPortals(0.f);
            check(g.world.mapId == B_DESERT, "stepping into the east gate leads to the desert");
            g.checkPortals(0.f);
            check(g.world.mapId == B_DESERT && g.travelCd > 0, "arriving does not bounce the hunter straight back");
        }
        // ---- saving in another map and loading
        removeAll();
        g.travelCd = 0;
        g.save();
        g.newGame();
        check(g.world.mapId == B_HOME && !g.mapMem[B_DESERT].visited && g.structs.empty(), "a new game starts in the home map with every map reset");
        const bool ld = g.load();
        check(ld && g.world.mapId == B_DESERT && g.curBiome == B_DESERT && spotOk(), "loading a save made in the desert continues in the desert");
        check(g.structs.size() == 1 && g.structs[0].tx == dTx && g.hstate(true, herb).gone && g.world.landmarks[1].found, "the desert's campfire, harvest and discovered landmark are restored");
        g.travelTo(B_HOME, 40, 30, false);
        check(g.structs.size() == 1 && g.structs[0].tx == homeTx && packTotal() == packNow, "the home map's campfire and the pack survive the save / load too");
        removeAll();
        // ---- a save from before the separate maps (one 240x168 world, home area at tile 76,54)
        {
            g.newGame();
            int hn = -1;
            for (size_t i = 0; i < g.world.nodes.size(); i++)
                if (g.world.nodes[i].kind == RN_HERB && (int)(g.world.nodes[i].x / TS) < COX + CW - 5 && (int)(g.world.nodes[i].x / TS) >= COX && (int)(g.world.nodes[i].y / TS) >= COY && (int)(g.world.nodes[i].y / TS) < COY + CH) {
                    hn = (int)i;
                    break;
                }
            const int htx = (int)(g.world.nodes[hn].x / TS), hty = (int)(g.world.nodes[hn].y / TS);
            std::string t = "version 2\ncoins 40\nup 0 0 0 0\nday 30\nkills 0\nquest 0 0 0\ndisc 10000100001000\n";
            t += std::string("slot ") + itemKey(itemWpn(W_FLINT)) + " 8\n";
            t += "hv n " + S(htx) + " " + S(hty) + " " + S(nodeRes(RN_HERB)) + " 120\n";
            t += "hv n 9 9 0 50\n";  // a resource that was out in a biome: ignored
            t += "struct " + S(PL_CHEST) + " " + S(30 + 76) + " " + S(20 + 54) + "\n";                 // a chest in the home area
            t += "struct " + S(PL_CHEST) + " 200 100\n";                                               // a chest out in the biomes
            t += std::string("cslot 0 ") + itemKey(itemMat(M_WOOD)) + " 5\n";
            t += std::string("cslot 1 ") + itemKey(itemMat(M_STONE)) + " 3\n";
            writeFile(t);
            const bool ok = g.load();
            check(ok && g.world.mapId == B_HOME && g.p.coins == 40, "an old save still loads");
            check(g.structs.size() == 1 && g.structs[0].kind == PL_CHEST && g.structs[0].tx == COX + 30 && g.structs[0].ty == COY + 20 && g.structs[0].inv.count(itemMat(M_WOOD)) == 5, "its home chest moves into the home map with its contents");
            check(g.hstate(true, hn).gone, "its harvested herb is still harvested");
            check(g.p.inv.count(itemPlace(PL_CHEST)) == 1 && g.p.mat(M_STONE) == 3, "a chest that stood out in the biomes comes back to the pack with its contents (nothing is lost)");
            check(g.mapMem[B_DESERT].disc == "1000" && g.mapMem[B_SWAMP].disc == "010" && g.mapMem[B_MOUNTAIN].disc == "1000", "old landmark discoveries are split between the maps");
            removeAll();
        }
    }
    printf("skill bonuses\n");
    {
        bool zero = true, mono = true, capped = true, steps = true;
        for (int k = 0; k < SK_COUNT; k++) {
            zero = zero && skillBonusAt(k, 1) == 0.f;
            capped = capped && fabsf(skillBonusAt(k, MAX_LEVEL) - SKILL[k].cap) < 1e-4f && skillBonusAt(k, 500) <= SKILL[k].cap + 1e-4f;
            for (int l = 2; l <= MAX_LEVEL; l++) mono = mono && skillBonusAt(k, l) >= skillBonusAt(k, l - 1);
            for (int m : MILESTONES) steps = steps && skillBonusAt(k, m) - skillBonusAt(k, m - 1) > SKILL[k].cap * 0.09f;  // a milestone is a visible jump
        }
        check(zero, "level 1 gives no bonus");
        check(mono, "bonuses never shrink as you level");
        check(capped, "every bonus tops out exactly at its cap at level 100 and never beyond");
        check(steps, "reaching a milestone level gives a step up in the bonus");
        g.newGame();
        g.state = ST_PLAY;
        g.p.sk.s[SK_DEFENSE].xp = SkillState::capXp();
        g.p.up[U_ARMOR] = 0;
        g.p.hp = g.p.maxhp = 100;
        g.p.invuln = 0;
        g.hurtPlayer(100, V2(0, 0), A_WOLF);
        check((int)g.p.hp == 15, "max Defense cuts incoming damage by its capped 15%");
        g.newGame();
        g.state = ST_PLAY;
        g.p.hp = 10;
        g.p.maxhp = 200;
        g.p.sk.s[SK_SURVIVAL].xp = SkillState::capXp();
        g.p.inv.add(itemCons(C_TONIC), 1);
        g.useConsumable(C_TONIC);
        check((int)g.p.hp == 10 + 117, "max Survival boosts a tonic's healing by its capped 30%");
    }
    printf("skill XP from actions\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.animals.clear();
        Animal* a = g.spawnAnimal(A_RABBIT, g.p.pos + V2(10, 0), 0, 0);
        g.killAnimal(*a);
        check(g.p.sk.s[SK_HUNTING].xp > 0 && g.p.sk.s[SK_RANGED].xp > 0, "a kill trains Hunting and Ranged");
        const int64_t easy = g.p.sk.s[SK_HUNTING].xp;
        Animal* w = g.spawnAnimal(A_WOLF, g.p.pos + V2(30, 0), 0, 0);
        const int64_t b1 = g.p.sk.s[SK_HUNTING].xp;
        g.killAnimal(*w);
        check(g.p.sk.s[SK_HUNTING].xp - b1 > easy, "a wolf is worth more Hunting XP than a rabbit");
        g.p.sk.s[SK_HUNTING].xp = SkillState::capXp() / 2;
        const int hi = g.p.sk.level(SK_HUNTING);
        check(hi > 40 && xpFalloff(hi, 10) < 1.0 && xpFalloff(hi, 10) >= 0.2 && xpFalloff(5, 10) == 1.0, "farming weak animals at a high level pays less (floor 20%)");
        g.p.dead = true;
        const int64_t bd = g.p.sk.s[SK_DEFENSE].xp;
        g.hurtPlayer(10, V2(0, 0), A_WOLF);
        check(g.p.sk.s[SK_DEFENSE].xp == bd, "no Defense XP while dead");
        g.p.dead = false;
        g.p.up[U_ARMOR] = 1;
        g.p.hp = g.p.maxhp = 100;
        g.p.invuln = 0;
        g.hurtPlayer(40, V2(0, 0), A_WOLF);
        check(g.p.sk.s[SK_DEFENSE].xp > bd, "surviving a hit in armor trains Defense");
        g.p.invuln = 0;
        g.p.hp = 5;
        const int64_t bk = g.p.sk.s[SK_DEFENSE].xp;
        g.hurtPlayer(40, V2(0, 0), A_WOLF);
        check(g.p.sk.s[SK_DEFENSE].xp == bk, "a fatal hit earns no Defense XP");
    }
    printf("trophies\n");
    {
        auto owners = [](int m) {  // how many species can drop material m (base drop, form drop or legendary drop)
            int n = 0;
            for (int k = 0; k < A_COUNT; k++)
                if (ANIMAL[k].drop[m][1] > 0 || ANIMAL[k].formDrop[0] == m || ANIMAL[k].legendDrop[0] == m) n++;
            return n;
        };
        bool oneEach = true, exclusive = true;
        for (int k = 0; k < A_COUNT; k++) {
            const int tm = M_TROPHY0 + k;
            if (ANIMAL[k].drop[tm][1] <= 0) {
                oneEach = false;
                printf("    %s has no trophy drop\n", ANIMAL[k].name);
            }
            if (owners(tm) != 1) {
                exclusive = false;
                printf("    %s is dropped by %d species\n", MAT_NAME[tm], owners(tm));
            }
        }
        check(oneEach && M_TROPHY_N == A_COUNT, "every one of the 28 species drops its own trophy");
        check(exclusive, "every trophy is dropped by exactly one species");
        bool noStray = true;  // no species drops a trophy that belongs to another
        for (int k = 0; k < A_COUNT; k++)
            for (int m = M_TROPHY0; m < M_TROPHY0 + M_TROPHY_N; m++)
                if (m != M_TROPHY0 + k && (ANIMAL[k].drop[m][1] > 0 || ANIMAL[k].formDrop[0] == m || ANIMAL[k].legendDrop[0] == m)) noStray = false;
        check(noStray, "no species drops another species' trophy");
        bool tables = true, ranges = true;
        for (int m = 0; m < M_COUNT; m++) {
            tables = tables && MAT_NAME[m] && *MAT_NAME[m] && MAT_BLURB[m] && *MAT_BLURB[m] && (MAT_PRICE[m] > 0 || m == M_CRYPTKEY); /* the crypt key is not for sale */
            for (int k = 0; k < A_COUNT; k++) ranges = ranges && ANIMAL[k].drop[m][0] >= 0 && ANIMAL[k].drop[m][0] <= ANIMAL[k].drop[m][1];
        }
        check(tables, "every material has a name, a blurb and a sell price (no table is short)");
        check(ranges, "every drop range is valid (0 <= min <= max)");
        bool permutation = true;
        int seen[M_COUNT] = {0};
        for (int i = 0; i < M_COUNT; i++) {
            if (GATHER_ORDER[i] < 0 || GATHER_ORDER[i] >= M_COUNT || seen[GATHER_ORDER[i]]++) permutation = false;
        }
        check(permutation, "the gather order lists every material exactly once");
        bool keysOk = true;  // save keys are item names: a duplicate would make two items share a key
        for (int a = 0; a < ITEM_COUNT && keysOk; a++)
            for (int b = a + 1; b < ITEM_COUNT; b++)
                if (itemKey(a) == itemKey(b)) {
                    keysOk = false;
                    printf("    duplicate item key %s\n", itemKey(a).c_str());
                    break;
                }
        check(keysOk, "item names are unique, so save keys are unique");
        bool icons = true;
        for (int m = 0; m < M_COUNT; m++) icons = icons && g.A.matIcon[m].t != nullptr;
        check(icons, "every material has an inventory icon");
        bool classes = isCarcassMat(M_MEAT) && isCarcassMat(M_CLAW) && isCarcassMat(M_WOLFFANG) && isCarcassMat(M_FAT) && !isCarcassMat(M_WOOD) && !isCarcassMat(M_GOLDBAR) &&
                       isTrophy(M_COUGARFANG) && !isTrophy(M_FAT) && !isTrophy(M_CLAW);
        check(classes, "carcass / trophy classification is right at every boundary");
        // the real thing: kill every species 60 times and look at what the carcass holds
        g.newGame();
        g.state = ST_PLAY;
        bool ownDrops = true, guaranteed = true, strayKill = true;
        for (int k = 0; k < A_COUNT; k++) {
            const int tm = M_TROPHY0 + k;
            int got = 0;
            for (int i = 0; i < 60; i++) {
                g.animals.clear();
                Animal* a = g.spawnAnimal(k, g.p.pos + V2(40, 0), 0, 0);
                g.killAnimal(*a);
                const Animal& c = g.animals[0];
                got += c.remain[tm];
                if (ANIMAL[k].drop[tm][0] >= 1 && c.remain[tm] < 1) guaranteed = false;
                for (int m = M_TROPHY0; m < M_TROPHY0 + M_TROPHY_N; m++)
                    if (m != tm && c.remain[m] > 0) strayKill = false;
            }
            if (got <= 0) {
                ownDrops = false;
                printf("    %s never dropped %s in 60 kills\n", ANIMAL[k].name, MAT_NAME[tm]);
            }
        }
        check(ownDrops, "killing each species yields its trophy");
        check(guaranteed, "species with a guaranteed trophy always carry it");
        check(strayKill, "a carcass never holds another species' trophy");
        // gathering moves trophies and fat into the pack
        g.newGame();
        g.state = ST_PLAY;
        Animal* w = g.spawnAnimal(A_WOLF, g.p.pos + V2(10, 0), 0, 0);
        g.killAnimal(*w);
        Animal& wc = *w;
        for (int i = 0; i < 100 && wc.totalRemain > 0; i++) g.gatherStep(wc);
        check(wc.totalRemain == 0 && g.p.mat(M_WOLFFANG) >= 1 && g.p.mat(M_FAT) >= 1, "gathering a wolf carcass delivers its fang and fat to the pack");
        // trophies stack, sell and save like any material
        g.p.inv.maxWeight = 1e6f;
        g.p.inv.add(itemMat(M_LIONMANE), 1200);
        check(g.p.mat(M_LIONMANE) == 1200 && g.p.inv.slots[0].n <= 999 && g.p.inv.usedSlots() >= 2, "trophies stack to 999 per slot");
        const int coinsBefore = g.p.coins;
        g.sellMat(M_LIONMANE, 20);
        check(g.p.mat(M_LIONMANE) == 1180 && g.p.coins == coinsBefore + 20 * MAT_PRICE[M_LIONMANE], "trophies sell at their listed price");
        g.p.inv.clear();
        g.p.inv.add(itemMat(M_BEARGALL), 3);
        g.p.inv.add(itemMat(M_FAT), 7);
        g.save();
        g.newGame();
        check(g.load() && g.p.mat(M_BEARGALL) == 3 && g.p.mat(M_FAT) == 7, "trophies and fat survive a save / load");
    }
    printf("skills\n");
    {
        // the curve: strictly increasing, nothing past the cap, level derived from total XP
        bool inc = true;
        for (int l = 1; l + 1 < MAX_LEVEL; l++) inc = inc && xpForLevel(l + 1) > xpForLevel(l);
        check(inc && xpForLevel(MAX_LEVEL) == 0 && xpForLevel(1) == (int64_t)XP_BASE, "XP per level rises with level and stops at the cap");
        SkillSet ss;
        check(ss.level(SK_HUNTING) == 1 && ss.add(SK_HUNTING, xpForLevel(1) - 1) == 0 && ss.level(SK_HUNTING) == 1, "one XP short of a level does not level up");
        check(ss.add(SK_HUNTING, 1) == 1 && ss.level(SK_HUNTING) == 2 && ss.s[SK_HUNTING].into() == 0, "exact XP levels up and carries zero over");
        check(ss.add(SK_HUNTING, xpForLevel(2) + xpForLevel(3) + 5) == 2 && ss.level(SK_HUNTING) == 4 && ss.s[SK_HUNTING].into() == 5, "a big award can gain several levels at once");
        check(ss.add(SK_MINING, 0) == 0 && ss.add(SK_MINING, -50) == 0 && ss.add(99, 10) == 0 && ss.add(-1, 10) == 0 && ss.s[SK_MINING].xp == 0, "zero, negative and invalid awards do nothing");
        ss.add(SK_RANGED, (int64_t)1 << 40);
        check(ss.level(SK_RANGED) == MAX_LEVEL && ss.s[SK_RANGED].xp == SkillState::capXp(), "XP is clamped at the level cap");
        bool keys = true;
        for (int a = 0; a < SK_COUNT; a++)
            for (int b = a + 1; b < SK_COUNT; b++) keys = keys && strcmp(SKILL[a].key, SKILL[b].key) != 0 && strcmp(SKILL[a].name, SKILL[b].name) != 0;
        check(keys, "skill keys and names are unique");
        // in the game: level-up reporting, dead players earn nothing
        g.newGame();
        g.state = ST_PLAY;
        check(g.grantXp(SK_WOODCUTTING, 3) == 0 && g.p.sk.s[SK_WOODCUTTING].xp == 3, "grantXp adds XP below a level-up");
        check(g.grantXp(SK_WOODCUTTING, (int)xpForLevel(1)) == 1 && g.p.sk.level(SK_WOODCUTTING) == 2, "grantXp reports a level-up");
        g.p.dead = true;
        check(g.grantXp(SK_MINING, 50) == 0 && g.p.sk.s[SK_MINING].xp == 0, "a dead hunter earns no XP");
        g.p.dead = false;
        // persistence
        g.p.sk.s[SK_COOKING].xp = 12345;
        g.p.sk.s[SK_DEFENSE].xp = 7;
        g.save();
        g.newGame();
        check(g.p.sk.s[SK_COOKING].xp == 0, "(setup) a new game starts with no skill XP");
        check(g.load() && g.p.sk.s[SK_COOKING].xp == 12345 && g.p.sk.s[SK_DEFENSE].xp == 7 && g.p.sk.s[SK_WOODCUTTING].xp > 0, "skill XP survives a save / load");
        check(g.p.sk.s[SK_MINING].xp == 0, "skills that were never trained stay at zero");
        {   // damaged / foreign lines: clamped or ignored, the rest of the save still loads
            std::ifstream in(g.savePath(), std::ios::binary);
            std::string txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
            std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
            out << txt << "skill futureskill 99\nskill hunting -5\nskill ranged 99999999999999\nskill\nskill mining\n";
        }
        check(g.load() && g.p.sk.s[SK_HUNTING].xp >= 0 && g.p.sk.s[SK_RANGED].xp == SkillState::capXp() && g.p.sk.s[SK_COOKING].xp == 12345, "unknown, negative, oversized and truncated skill lines are handled");
        {   // a save from before skills existed has no skill lines: everything loads at level 1
            std::ifstream in(g.savePath(), std::ios::binary);
            std::string line, kept;
            while (std::getline(in, line))
                if (line.compare(0, 5, "skill") != 0) kept += line + "\n";
            in.close();
            std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
            out << kept;
        }
        bool allOne = g.load();
        for (int k = 0; k < SK_COUNT; k++) allOne = allOne && g.p.sk.level(k) == 1;
        check(allOne, "an older save without skill lines loads with every skill at level 1");
    }
    {  // ---- hunter animation: facing, clips, the state machine
        const float X[8] = {0, 1, 1, 1, 0, -1, -1, -1}, Y[8] = {1, 1, 0, -1, -1, -1, 0, 1};  // S SE E NE N NW W SW
        bool all8 = true;
        for (int f = 0; f < 8; f++) all8 = all8 && facingFromDir(X[f], Y[f]) == f;
        check(all8, "the eight directions map to the eight facings");
        check(facingView(F_W).view == facingView(F_E).view && facingView(F_W).mirror && !facingView(F_E).mirror, "west is the east view mirrored");
        check(facingView(F_NW).view == facingView(F_NE).view && facingView(F_NW).mirror && facingView(F_SW).view == facingView(F_SE).view && facingView(F_SW).mirror, "the west diagonals are the east diagonals mirrored");
        check(facingFromDir(1.f, 0.55f, F_SE) == F_SE && facingFromDir(1.f, 1.5f, F_SE) == F_SE, "a direction wobbling on a sector border keeps the facing (no flicker)");
        check(facingFromDir(1.f, -0.3f, F_SE) == F_E, "a clear turn changes the facing");
        {
            Animator an;
            const Clip loopC = Clip::run(0, 4, 0.1f, true), once = Clip::run(0, 3, 0.1f, false);
            an.play(1, &loopC);
            an.update(0.25f);
            const float t1 = an.t;
            an.play(1, &loopC);  // same state again: must not restart
            check(an.t == t1 && an.frame() == 2, "playing the state that is already playing does not restart it");
            an.update(0.3f);
            check(an.frame() < 4 && !an.done, "a looping clip wraps");
            an.play(2, &once);
            int firedN = 0;
            for (int i = 0; i < 20; i++) {
                an.update(0.05f);
                if (an.justDone) firedN++;
            }
            check(firedN == 1 && an.done && an.frame() == 2, "a one-shot clip finishes once and holds its last frame");
            an.play(2, &once, true);
            an.scrub(0.5f);
            check(an.frame() == 1, "scrub picks the frame by progress");
        }
        bool clipsOk = true, spritesOk = true;
        for (int st = 0; st < HS_COUNT; st++) {
            const Clip& c = hunterClip(st);
            clipsOk = clipsOk && c.n > 0 && hunterPoseCount(st) <= HS_MAXF;
            for (int i = 0; i < c.n; i++) clipsOk = clipsOk && c.frame[i] >= 0 && c.frame[i] < hunterPoseCount(st);
            for (int v = 0; v < FACING_VIEWS; v++)
                for (int i = 0; i < hunterPoseCount(st); i++) spritesOk = spritesOk && g.A.hunter[v][st][i].ok();
        }
        check(clipsOk, "every hunter clip only uses poses that exist");
        check(spritesOk, "every hunter pose is drawn in all five views");
        // the state machine in play
        g.newGame();
        g.state = ST_PLAY;
        g.animals.clear();
        g.p.inv.clear();
        auto run = [&](int n) { for (int i = 0; i < n; i++) g.updatePlayer(0.02f); };
        g.in = Input();
        run(10);
        check(g.p.anim.state == HS_IDLE, "standing still plays idle");
        g.in.right = true;
        run(10);
        check(g.p.anim.state == HS_WALK && g.p.face8 == F_E, "walking right plays walk facing east");
        g.in.down = true;
        run(10);
        check(g.p.face8 == F_SE, "right + down faces south-east");
        g.in.right = false;
        g.in.left = true;
        run(10);
        check(g.p.face8 == F_SW && facingView(g.p.face8).mirror, "left + down faces south-west (mirrored view)");
        g.in.sprint = true;
        run(10);
        check(g.p.anim.state == HS_RUN, "sprinting plays run");
        g.in = Input();
        run(10);
        check(g.p.anim.state == HS_IDLE && g.p.face8 == F_SW, "stopping returns to idle and keeps the facing");
        g.p.invuln = 0;
        g.p.hp = g.p.maxhp;
        g.hurtPlayer(5, g.p.pos + V2(10, 0));
        run(2);
        check(g.p.anim.state == HS_HURT, "a hit plays the hurt animation");
        run(30);
        check(g.p.anim.state == HS_IDLE, "the hurt animation ends and idle resumes");
        g.p.hp = 10;
        g.p.inv.add(itemCons(C_BANDAGE), 1);
        g.useConsumable(C_BANDAGE);
        run(2);
        check(g.p.anim.state == HS_USE, "using a bandage plays the use animation");
        run(40);
        g.p.invuln = 0;
        g.p.sinceHurt = 99;
        g.p.hp = 1;
        g.hurtPlayer(50, g.p.pos + V2(10, 0));
        run(2);
        check(g.p.dead && g.p.anim.state == HS_DEATH, "being knocked out plays the death animation");
        for (int i = 0; i < 400 && g.p.dead; i++) g.updatePlayer(0.02f);
        check(!g.p.dead, "the hunter gets back up after the death animation");
        run(2);
        check(g.p.anim.state == HS_IDLE, "idle resumes after respawning");
        {  // gathering: one animation per kind of work, timed to the swing
            g.p.dead = false;
            g.p.hp = g.p.maxhp;
            g.p.impactT = 0;
            g.p.anim = Animator();
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.f, RCL_TREE);
            check(g.p.anim.state == HS_SWING, "an axe on a tree plays the chopping stroke");
            g.updateHunterAnim(0.02f, false, false, false, true, 4, 0.f, RCL_ROCK);
            check(g.p.anim.state == HS_MINE, "a pickaxe on rock plays the mining stroke");
            g.updateHunterAnim(0.02f, false, false, false, true, 4, 0.f, RCL_ORE);
            check(g.p.anim.state == HS_MINE, "a pickaxe on ore plays the mining stroke");
            g.updateHunterAnim(0.02f, false, false, false, true, -1, 0.f, RCL_PLANT);
            check(g.p.anim.state == HS_FORAGE, "picking a plant by hand plays the foraging motion");
            g.updateHunterAnim(0.02f, false, false, true, false, -1, 0.f, -1);
            check(g.p.anim.state == HS_GATHER, "kneeling at a carcass plays the skinning motion");
            // the swing follows the harvest timer: the raise at the start, the strike just before the hit, the recovery after it
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.f, RCL_TREE);
            check(g.p.anim.frame() == 0, "the swing starts with the raise");
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.98f, RCL_TREE);
            check(g.p.anim.frame() == 3, "the hit lands on the impact frame");
            g.p.impactT = 0.1f;
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.f, RCL_TREE);
            check(g.p.anim.frame() == 4, "the recovery pose follows the hit");
            g.p.impactT = 0;
            g.updateHunterAnim(0.02f, false, false, false, true, 0, 0.f, RCL_TREE);
            check(g.p.anim.frame() == 0 && g.p.anim.state == HS_SWING, "and the next swing starts again from the raise");
            g.updateHunterAnim(0.02f, false, false, false, false, -1, 0.f, -1);
            check(g.p.anim.state == HS_IDLE, "stopping returns to idle");
        }
    }
    {  // ---- the boar's full animation set
        const AnimalClips* bc = animalClips(A_BOAR);
        check(bc && animalClips(A_WOLF) && animalClips(A_DEER) && animalClips(A_RABBIT) && animalClips(A_HARE) && animalClips(A_SNOWHARE) && !animalClips(A_FENNEC) && g.A.rich[A_BOAR] && g.A.rich[A_RABBIT] && !g.A.rich[A_FENNEC], "boar, wolf, deer, rabbit and the hares have a full animation set; the others keep the walk cycle");
        check(g.A.rich[A_BOAR]->built[0] && g.A.rich[A_EAGLE]->built[0], "the common sprites of every full-set species are drawn at start-up");
        {  // rarer tiers are drawn when the first one spawns
            const int legIdx = 2 * TR_LEGENDARY;
            g.animals.clear();
            Animal* an = g.spawnAnimal(A_EAGLE, g.p.pos + V2(100, 0), 0, TR_LEGENDARY);
            check(an && g.A.rich[A_EAGLE]->built[legIdx] && g.A.rich[A_EAGLE]->s[legIdx][0][AN_FLY][0].ok(), "a legendary spawning draws its sprites on demand");
            g.animals.clear();
        }
        buildAllRichBanks(g.ren, g.A);  // the rarer tiers are drawn lazily; the completeness check wants them all
        bool bankOk = bc != nullptr, clipsOk = bc != nullptr;
        if (bc) {
            for (int kind : RICH_KINDS)
                for (int form = 0; form < ANIMAL[kind].forms; form++)
                    for (int tier = 0; tier < TR_COUNT; tier++)
                        for (int v = 0; v < AV_VIEWS; v++)
                            for (int st = 0; st < AN_COUNT; st++)
                                for (int i = 0; i < animalClips(kind)->poses[st]; i++) bankOk = bankOk && g.A.rich[kind]->s[form + 2 * tier][v][st][i].ok();
            for (int kind : RICH_KINDS) {
                const AnimalClips* kc = animalClips(kind);
                for (int st = 0; st < AN_COUNT; st++) {
                    clipsOk = clipsOk && kc->poses[st] <= AN_MAXF && (kc->poses[st] == 0) == (kc->clip[st].n == 0);  // a state has a clip exactly when it has poses
                    for (int i = 0; i < kc->clip[st].n; i++) clipsOk = clipsOk && kc->clip[st].frame[i] < kc->poses[st];
                }
            }
            clipsOk = clipsOk && bc->clip[AN_IDLE].loop && bc->clip[AN_WALK].loop && bc->clip[AN_RUN].loop && !bc->clip[AN_ATTACK].loop && !bc->clip[AN_HURT].loop && !bc->clip[AN_DEATH].loop;
        }
        check(bankOk, "every boar, wolf, deer, rabbit and hare form / rarity / view / state / pose is drawn");
        {  // birds: the flight states and the air logic
            check(animalClips(A_DUCK) && animalClips(A_EAGLE) && animalClips(A_ROADRUNNER) && g.A.rich[A_EAGLE] && animalClips(A_EAGLE)->poses[AN_FLY] == 6 && animalClips(A_EAGLE)->clip[AN_FLY].loop && !animalClips(A_EAGLE)->clip[AN_TAKEOFF].loop && !animalClips(A_EAGLE)->clip[AN_LAND].loop, "duck, eagle and roadrunner have bird sets with looping flight and one-shot takeoff / landing");
            g.animals.clear();
            g.p.pos = V2(0, 0);
            Animal d;
            d.kind = A_DUCK;
            d.hp = d.maxhp = 12;
            d.pos = d.target = V2(300, 300);
            d.bodyH = ANIMAL[A_DUCK].bodyH;
            d.st = AS_WANDER;
            g.animals.push_back(d);
            auto D_ = [&]() -> Animal& { return g.animals[0]; };
            for (int i = 0; i < 20; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == 0.f && D_().anim.state == AN_IDLE, "a resting duck stands on the ground");
            D_().vel = V2(20, 0);
            for (int i = 0; i < 5; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == 0.f && D_().anim.state == AN_WALK, "a wandering duck walks");
            D_().st = AS_FLEE;
            D_().vel = V2(100, 0);
            g.updateAnimalAnim(D_(), 0.02f);
            check(D_().anim.state == AN_TAKEOFF && D_().air > 0.f, "a fleeing duck takes off");
            float prevAir = D_().air;
            bool rises = true;
            for (int i = 0; i < 12; i++) {
                g.updateAnimalAnim(D_(), 0.02f);
                rises = rises && D_().air >= prevAir;
                prevAir = D_().air;
            }
            check(rises, "it climbs smoothly");
            for (int i = 0; i < 60; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == FLY_HEIGHT && D_().anim.state == AN_FLY, "then cruises with the wing-beat cycle");
            D_().st = AS_WANDER;
            D_().vel = V2();
            g.updateAnimalAnim(D_(), 0.02f);
            check(D_().anim.state == AN_LAND, "once calm it lands");
            for (int i = 0; i < 80; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == 0.f && D_().anim.state == AN_IDLE, "and is back to standing on the ground");
            D_().st = AS_FLEE;
            for (int i = 0; i < 80; i++) g.updateAnimalAnim(D_(), 0.02f);
            g.hitAnimal(D_(), 1.f, V2(1, 0), false);
            g.updateAnimalAnim(D_(), 0.02f);
            check(D_().anim.state == AN_HURT && D_().air > 0.f, "a hit flyer flinches in the air");
            g.animals[0].kind = A_EAGLE;
            g.animals[0].st = AS_WANDER;
            g.animals[0].vel = V2(30, 0);
            g.animals[0].hurtT = 0;
            for (int i = 0; i < 80; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().anim.state == AN_FLY && D_().air == FLY_HEIGHT, "a soaring eagle stays aloft while it moves");
            D_().st = AS_REST;
            D_().vel = V2();
            for (int i = 0; i < 80; i++) g.updateAnimalAnim(D_(), 0.02f);
            check(D_().air == 0.f, "and lands to rest");
            g.animals.clear();
        }
        {  // the exporter: PNG sheets for all eight directions of every state, plus JSON metadata
            const std::string dir = (std::filesystem::temp_directory_path() / "wildwood_export_test").string();
            std::filesystem::remove_all(dir);
            const int n = sprexp::exportAll(dir);
            check(n > 0, "the sprite exporter writes its files");
            int pngs = 0, bad = 0;
            for (auto& e : std::filesystem::recursive_directory_iterator(dir))
                if (e.path().extension() == ".png") {
                    pngs++;
                    FILE* f = fopen(e.path().string().c_str(), "rb");
                    unsigned char h[24] = {0};
                    if (!f || fread(h, 1, 24, f) != 24 || memcmp(h, "\x89PNG\r\n\x1a\n", 8) != 0 || ((h[16] << 24) | (h[17] << 16) | (h[18] << 8) | h[19]) < 8) bad++;
                    if (f) fclose(f);
                }
            int expect = 0;
            for (int st = 0; st < HS_COUNT; st++) expect += F_COUNT;
            for (int kind : RICH_KINDS)
                for (int form = 0; form < ANIMAL[kind].forms; form++)
                    for (int st = 0; st < AN_COUNT; st++)
                        if (animalClips(kind)->poses[st] > 0) expect += F_COUNT;
            check(pngs == expect && bad == 0, "one valid PNG per state and direction (8 directions each)");
            check(std::filesystem::exists(dir + "/sprites/animals/wild_duck/wild_duck_fly_northwest.png") && std::filesystem::exists(dir + "/animations/animals/deer_walk.json") &&
                      std::filesystem::exists(dir + "/sprites/player/gathering/chopping/player_swing_east.png") && std::filesystem::exists(dir + "/animations/manifest.json"),
                  "files are named species_state_direction and have metadata");
            std::filesystem::remove_all(dir);
        }
        {  // calm states (graze, rest) and the startle before a prey animal bolts
            g.animals.clear();
            g.p.dead = false;
            Animal d;
            d.kind = A_DEER;
            d.hp = d.maxhp = ANIMAL[A_DEER].hp;
            d.bodyH = ANIMAL[A_DEER].bodyH;
            d.pos = d.target = g.p.pos + V2(400, 0);
            d.st = AS_FEED;
            g.animals.push_back(d);
            auto E_ = [&]() -> Animal& { return g.animals[0]; };
            g.updateAnimalAnim(E_(), 0.02f);
            check(E_().anim.state == AN_EAT, "a grazing animal plays the eating animation");
            E_().st = AS_REST;
            g.updateAnimalAnim(E_(), 0.02f);
            check(E_().anim.state == AN_REST, "a resting animal lies down");
            bool poses = true;
            for (int kind : RICH_KINDS)
                for (int st : {AN_EAT, AN_REST, AN_ALERT}) poses = poses && animalClips(kind)->poses[st] > 0 && animalClips(kind)->clip[st].n > 0;
            check(poses, "every species with a full set can eat, rest and be alert");
            // try a few spots around the hunter (the first may be walled in): the first one it can run from is checked
            bool startled = false, froze = true, bolted = false, dust = false;
            for (const V2 off : {V2(40, 0), V2(-40, 0), V2(0, 40), V2(0, -40), V2(60, 60), V2(-60, -60)}) {
                if (bolted) break;
                E_().st = AS_WANDER;
                E_().alertT = 0;
                E_().vel = V2();
                E_().pos = E_().target = g.p.pos + off;
                E_().anim = Animator();
                const V2 p0 = E_().pos;
                g.updateAnimals(0.02f);
                startled = E_().st == AS_FLEE && E_().alertT > 0 && E_().anim.state == AN_ALERT;
                if (!startled) continue;
                for (int i = 0; i < 10; i++) g.updateAnimals(0.02f);
                froze = dist(p0, E_().pos) < 0.5f && E_().anim.state == AN_ALERT;
                for (int i = 0; i < 60; i++) g.updateAnimals(0.02f);
                bolted = E_().alertT <= 0 && dist(p0, E_().pos) > 4.f && E_().anim.state == AN_RUN;
                if (bolted) {  // a running animal kicks up dust
                    g.parts.clear();
                    for (int i = 0; i < 150; i++) g.updateAnimals(0.02f);
                    dust = !g.parts.empty();
                }
            }
            check(dust, "a running animal kicks up dust");
            check(startled, "a deer that notices the hunter is startled first");
            check(froze, "it freezes and looks while the alert plays");
            check(bolted, "then it bolts");
            g.animals.clear();
        }
        check(clipsOk, "every species' clips only use poses that exist; the boar's idle, walk and run loop, attack, hurt and death play once");
        if (bc) {
            g.newGame();
            g.state = ST_PLAY;
            g.animals.clear();
            Animal* a = g.spawnAnimal(A_BOAR, g.p.pos + V2(80, 0), 0, 0);
            check(a != nullptr, "test setup: a boar");
            if (a) {
                const int idx = (int)(a - &g.animals[0]);
                auto A_ = [&]() -> Animal& { return g.animals[idx]; };
                A_().hp = A_().maxhp = 5000;
                A_().st = AS_WANDER;
                A_().vel = V2();
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state == AN_IDLE, "a standing boar plays idle");
                A_().vel = V2(20, 0);
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state == AN_WALK && A_().face8 == F_E, "a wandering boar plays walk facing the way it moves");
                const float t1 = A_().anim.t;
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.t > t1, "walking keeps advancing (no restart every frame)");
                A_().vel = V2(-15, 15);
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().face8 == F_SW, "moving down-left faces south-west");
                A_().st = AS_FLEE;
                A_().vel = V2(60, 0);
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state == AN_RUN, "a fleeing boar plays run");
                // a real bite: put a charging boar next to the hunter
                g.p.invuln = 0;
                g.p.hp = g.p.maxhp;
                A_().st = AS_CHASE;
                A_().atkCd = 0;
                A_().pos = g.p.pos + V2(8, 0);
                g.updateAnimals(0.02f);
                check(A_().atkT > 0 && A_().anim.state == AN_ATTACK && g.p.hp < g.p.maxhp, "a charging boar bites: the attack animation plays as the hit lands");
                check(A_().face8 == F_W, "the boar faces the hunter while it attacks");
                for (int i = 0; i < 40; i++) g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state != AN_ATTACK, "the attack animation ends");
                g.hitAnimal(A_(), 1.f, V2(1, 0), false);
                g.updateAnimalAnim(A_(), 0.02f);
                check(A_().anim.state == AN_HURT, "a hit plays the hurt animation");
                g.killAnimal(A_());
                for (int i = 0; i < 80; i++) g.updateAnimalAnim(A_(), 0.02f);
                check(A_().dead && A_().anim.state == AN_DEATH && A_().anim.done && A_().anim.frame() == bc->poses[AN_DEATH] - 1, "a dead boar plays death once and stays on the last pose");
                {  // the death slide: momentum carries the body a short way, slows smoothly and stops; the corpse stays
                    Animal& b = A_();
                    const V2 p0 = b.pos;
                    const float sp0 = b.kb.len();
                    float prev = sp0, spMid = 0;
                    bool mono = true;
                    for (int i = 0; i < 300 && b.kb.len() > 0; i++) {
                        g.updateAnimals(0.02f);
                        const float s = b.kb.len();
                        mono = mono && s <= prev + 1e-3f;
                        prev = s;
                        if (i == 10) spMid = s;
                    }
                    const float d = dist(p0, b.pos);
                    check(sp0 > 50.f, "the killing blow starts a slide");
                    check(d > 6.f && d < 60.f, "the corpse slides a short distance");
                    check(mono && spMid > 0.f && spMid < sp0, "the slide slows down smoothly");
                    check(b.kb.len() == 0.f, "the slide comes to a complete stop");
                    const V2 pStop = b.pos;
                    for (int i = 0; i < 50; i++) g.updateAnimals(0.02f);
                    check(dist(pStop, b.pos) == 0.f && !b.gone && b.totalRemain > 0 && b.anim.state == AN_DEATH && b.anim.done, "the stopped corpse stays put, gatherable, on its last death pose");
                }
            }
        }
    }
    printf("town: strolling villagers, shop talk\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.updateVillagers(1.f / 30.f);
        check(g.villagers.size() == 5, "five townspeople stand in the town");
        const int tx0 = COX + TOWN_X0, ty0 = COY + TOWN_Y0, tx1 = COX + TOWN_X1, ty1 = COY + TOWN_Y1;
        auto onCobbles = [&](const Villager& v) {
            const int tx = (int)(v.pos.x / TS), ty = (int)(v.pos.y / TS);
            return tx >= tx0 && ty >= ty0 && tx < tx1 && ty < ty1 && g.world.tile[ty][tx] == T_COBBLE && !g.world.blocked(v.pos.x, v.pos.y, 4.f);
        };
        bool allOn = true;
        for (auto& v : g.villagers) allOn = allOn && onCobbles(v);
        check(allOn, "they start on open cobbles inside the town");
        std::vector<V2> start;
        for (auto& v : g.villagers) start.push_back(v.pos);
        const V2 pp = g.p.pos;
        bool stayed = true, apart = true;
        float minGap = 1e9f;
        for (int i = 0; i < 30 * 90; i++) {  // 90 seconds of strolling
            g.p.pos = pp;
            g.updateVillagers(1.f / 30.f);
            for (size_t a2 = 0; a2 < g.villagers.size(); a2++) {
                stayed = stayed && onCobbles(g.villagers[a2]);
                for (size_t b2 = a2 + 1; b2 < g.villagers.size(); b2++) minGap = std::min(minGap, dist(g.villagers[a2].pos, g.villagers[b2].pos));
            }
        }
        apart = minGap > 5.f;
        int moved = 0;
        for (size_t i = 0; i < start.size(); i++) moved += dist(start[i], g.villagers[i].pos) > 8.f;
        check(stayed, "in 90 seconds none of them leaves the cobbles or walks into a wall");
        check(moved >= 3, "most of them actually stroll somewhere");
        check(apart, "they do not walk through one another");
        // greeting the hunter
        Villager& v0 = g.villagers[0];
        g.p.pos = v0.pos + V2(18, 0);
        v0.chatCd = 0;
        v0.sayT = 0;
        g.updateVillagers(1.f / 30.f);
        check(g.villagers[0].sayT > 3.f && !g.villagers[0].moving && g.villagers[0].faceLeft == false, "a villager the hunter walks up to stops, turns and speaks");
        // not outside: nothing moves while the hunter is in a building
        std::vector<V2> before;
        for (auto& v : g.villagers) before.push_back(v.pos);
        g.enterBuilding(BLD_SMITH);
        for (int i = 0; i < 60; i++) g.updateVillagers(1.f / 30.f);
        bool still = true;
        for (size_t i = 0; i < before.size(); i++) still = still && dist(before[i], g.villagers[i].pos) < 0.01f;
        check(still, "the townspeople wait outside while the hunter is in a shop");
        // the shopkeeper greets, then chats
        g.p.pos = g.world.npcs[0].pos + V2(0, 30);
        for (int i = 0; i < 30; i++) g.updateShopTalk(1.f / 30.f);
        check(g.npcSayT > 0 && g.npcSayLine == 0, "the shopkeeper greets the hunter a moment after they come in");
        for (int i = 0; i < 30 * 14; i++) g.updateShopTalk(1.f / 30.f);
        check(g.npcSayLine != 0, "and goes on to idle remarks");
        g.exitBuilding();
        // the walk cycle is a real animation: the frames differ, and the standing frame is stable
        bool framesDiffer = true;
        for (int v = 0; v < 5; v++)
            framesDiffer = framesDiffer && mkVillager(v, 0).p != mkVillager(v, 1).p && mkVillager(v, 1).p != mkVillager(v, 3).p && mkVillager(v, 0).p == mkVillager(v, 2).p;
        check(framesDiffer, "every villager has distinct walk frames");
        check(mkNpc(BLD_SMITH, 0).p != mkNpc(BLD_SMITH, 1).p && mkNpc(BLD_FLOWER, 0).p != mkVillager(3, 0).p, "the shopkeepers still have their own look and idle breathing");
    }
    printf("HUD feedback\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        g.msgs.clear();
        g.msg("PACK FULL", C_RED);
        g.msg("PACK FULL", C_RED);
        g.msg("PACK FULL", C_RED);
        check(g.msgs.size() == 1 && g.msgs.back().n == 3, "the same message posted three times in a row is one line counted X3");
        g.msg("PACK FULL", C_GREEN);
        g.msg("SOMETHING ELSE", C_RED);
        check(g.msgs.size() == 3 && g.msgs[0].n == 3 && g.msgs[1].n == 1, "a different colour or text starts a new line");
        g.msgs.back().t = 0.1f;
        g.msg("SOMETHING ELSE", C_RED);
        check(g.msgs.back().t > 4.f, "a repeat refreshes the line's time on screen");
        for (int i = 0; i < 10; i++) g.msg(std::string("LINE ") + std::to_string(i), C_TEXT);
        check(g.msgs.size() <= 4, "the message area never holds more than four lines");
        g.ftexts.clear();
        g.grantXp(SK_HUNTING, 3);
        g.grantXp(SK_RANGED, 2);
        check(g.ftexts.size() >= 2 && g.ftexts[0].p.y != g.ftexts[1].p.y, "two XP gains at once are stacked, not printed on top of each other");
    }
    printf("wildlife: food chain and herd alarm\n");
    {
        g.newGame();
        g.state = ST_PLAY;
        auto at = [&](V2 v) { return g.freeSpot((int)(v.x / TS), (int)(v.y / TS)); };
        const V2 far = g.p.pos + V2(420, 0);  // well outside everyone's notice range of the hunter
        // 1. a wolf stalks a rabbit, the rabbit flees, the wolf bites and kills it
        g.animals.clear();
        g.animals.reserve(16);
        int wi = (int)(g.spawnAnimal(A_WOLF, at(far), 0, 0) - &g.animals[0]);
        int ri = (int)(g.spawnAnimal(A_RABBIT, at(far + V2(50, 0)), 0, 0) - &g.animals[0]);
        g.animals[wi].biome = g.animals[ri].biome = B_HOME;
        g.animals[wi].hp = g.animals[wi].maxhp = 100;
        bool hunted = false, fled = false, killed = false;
        const int kills0 = g.kills;
        const int64_t xp0 = g.p.sk.s[SK_HUNTING].xp, rx0 = g.p.sk.s[SK_RANGED].xp;
        for (int i = 0; i < 1800 && !killed; i++) {
            g.updateAnimals(1.f / 30.f);
            hunted = hunted || g.animals[wi].st == AS_HUNT;
            fled = fled || (g.animals[ri].st == AS_FLEE && g.animals[ri].fleeAnimal);
            killed = g.animals[ri].dead;
        }
        check(hunted, "a wolf that sees a rabbit starts to hunt it");
        check(fled, "the rabbit notices the hunting wolf and flees from it (not from the hunter)");
        check(killed && g.animals[ri].totalRemain > 0, "the wolf catches the rabbit and leaves a carcass to find");
        check(g.kills == kills0 && g.p.sk.s[SK_HUNTING].xp == xp0 && g.p.sk.s[SK_RANGED].xp == rx0, "a predator's kill gives the hunter no kill count and no XP");
        check(g.animals[wi].st == AS_FEED && g.animals[wi].huntCd > 20.f, "after the kill the wolf feeds, then rests before hunting again");
        // 2. rare specimens are never prey; other biomes' animals are not prey either
        g.animals.clear();
        wi = (int)(g.spawnAnimal(A_WOLF, at(far), 0, 0) - &g.animals[0]);
        ri = (int)(g.spawnAnimal(A_RABBIT, at(far + V2(40, 0)), 0, 2) - &g.animals[0]);
        bool everHunted = false;
        for (int i = 0; i < 600; i++) {
            g.updateAnimals(1.f / 30.f);
            everHunted = everHunted || g.animals[wi].st == AS_HUNT;
        }
        check(!everHunted && !g.animals[ri].dead, "a rare rabbit is not hunted by wolves");
        // 3. a wolf only hunts what is on its menu
        g.animals.clear();
        wi = (int)(g.spawnAnimal(A_WOLF, at(far), 0, 0) - &g.animals[0]);
        g.spawnAnimal(A_BOAR, at(far + V2(40, 0)), 0, 0);
        everHunted = false;
        for (int i = 0; i < 600; i++) {
            g.updateAnimals(1.f / 30.f);
            everHunted = everHunted || g.animals[wi].st == AS_HUNT;
        }
        check(!everHunted, "a wolf does not hunt a boar (not on its menu)");
        // 4. the hunter outranks prey: a stalking wolf turns on a hunter who comes close
        g.animals.clear();
        wi = (int)(g.spawnAnimal(A_WOLF, g.p.pos + V2(60, 0), 0, 0) - &g.animals[0]);
        g.animals[wi].st = AS_HUNT;
        g.animals[wi].stT = 8.f;
        g.updateAnimals(1.f / 30.f);
        check(g.animals[wi].st == AS_CHASE, "a hunting wolf that notices the hunter attacks the hunter instead");
        // 5. herd alarm: one deer startled by the hunter sets its neighbours running
        g.animals.clear();
        g.animals.reserve(8);
        const V2 d1 = g.freeSpot((int)((g.p.pos.x + 60) / TS), (int)(g.p.pos.y / TS));
        const float deerNotice = ANIMAL[A_DEER].notice;
        g.spawnAnimal(A_DEER, d1, 0, 0);
        g.spawnAnimal(A_DEER, d1 + V2(66, 0), 0, 0);
        g.spawnAnimal(A_DEER, d1 + V2(64, 16), 0, 0);
        for (auto& an : g.animals) an.biome = B_HOME, an.st = AS_WANDER;
        const bool setupOk = dist(g.animals[0].pos, g.p.pos) < deerNotice && dist(g.animals[1].pos, g.p.pos) > deerNotice * 1.1f &&
                             dist(g.animals[2].pos, g.p.pos) > deerNotice * 1.1f && dist(g.animals[0].pos, g.animals[1].pos) < 70.f && dist(g.animals[0].pos, g.animals[2].pos) < 70.f;
        check(setupOk, "(setup) one deer sees the hunter; its two herd mates are beyond sight but within the alarm radius");
        g.updateAnimals(1.f / 30.f);
        check(g.animals[0].st == AS_FLEE, "the deer next to the hunter bolts");
        check(g.animals[1].st == AS_FLEE && g.animals[2].st == AS_FLEE && !g.animals[1].fleeAnimal, "its herd mates, who cannot see the hunter, bolt with it");
    }
    printf("audio: ambience beds and footsteps (synthesised offline, no sound device)\n");
    {
        Audio au;
        au.synth();
        bool stepsOk = true;
        for (int sfx : {(int)S_STEP_GRASS, (int)S_STEP_STONE, (int)S_STEP_WOOD, (int)S_STEP_SAND, (int)S_STEP_SNOW, (int)S_STEP_MUD}) {
            float peak = 0;
            for (float v : au.bank[sfx]) peak = std::max(peak, fabsf(v));
            stepsOk = stepsOk && !au.bank[sfx].empty() && peak > 0.05f && peak <= 1.f;
        }
        check(stepsOk, "every footstep sound exists, is audible and does not clip");
        bool bedsOk = true, distinct = true;
        double energy[AMB_COUNT] = {0};
        for (int k = AMB_DAY; k < AMB_COUNT; k++) {
            Audio b;
            b.ambNow = k;
            float peak = 0;
            bool finite = true;
            for (int i = 0; i < 44100 * 12; i++) {  // 12 seconds: long enough for a bird, frog or drip to sound
                const float v = b.ambSample();
                finite = finite && std::isfinite(v);
                peak = std::max(peak, fabsf(v));
                energy[k] += (double)v * v;
            }
            bedsOk = bedsOk && finite && peak > 0.01f && peak < 2.f;
        }
        check(bedsOk, "every ambient bed produces finite, audible, bounded sound");
        for (int a2 = AMB_DAY; a2 < AMB_COUNT; a2++)
            for (int b2 = a2 + 1; b2 < AMB_COUNT; b2++) distinct = distinct && fabs(energy[a2] - energy[b2]) > 1e-6;
        check(distinct, "the beds differ from each other (biome-specific)");
        Audio none;
        none.ambNow = AMB_NONE;
        check(none.ambSample() == 0.f, "no ambience means silence");
        // the scene choice: outdoors by biome (day/night), rooms quiet, delves cave
        const int saveBiome = g.curBiome, saveInterior = g.interior;
        const float saveDay = g.dayT;
        float vol = 0;
        g.interior = -1;
        g.curBiome = B_SNOW;
        check(g.ambienceFor(vol) == AMB_SNOW, "the snow biome has the snow-wind bed");
        g.curBiome = B_DESERT;
        check(g.ambienceFor(vol) == AMB_DESERT, "the desert has the dry-wind bed");
        g.curBiome = B_SWAMP;
        check(g.ambienceFor(vol) == AMB_SWAMP, "the swamp has the frog bed");
        g.curBiome = B_MOUNTAIN;
        check(g.ambienceFor(vol) == AMB_MOUNTAIN, "the mountains have the mountain bed");
        g.curBiome = B_HOME;
        const int homeKind = g.ambienceFor(vol);
        check(homeKind == AMB_DAY || homeKind == AMB_NIGHT, "the home meadow has a day or a night bed");
        g.interior = 0;
        check(g.ambienceFor(vol) == AMB_NONE, "inside a building the world is quiet");
        g.interior = saveInterior;
        g.curBiome = saveBiome;
        g.dayT = saveDay;
        // footsteps: the sound follows the tile under the hunter
        check(g.world.tileAt(g.p.pos.x, g.p.pos.y) < 255, "a footstep reads the tile under the hunter (no crash)");
    }
    if (hadSave) {
        std::ofstream out(g.savePath(), std::ios::binary | std::ios::trunc);
        out << savedBytes;
    } else
        removeAll();
    printf("SELFTEST: %s (%d failed)\n", bad ? "FAILED" : "ok", bad);
    return bad;
}

