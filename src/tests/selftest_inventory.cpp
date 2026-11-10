// selftest_inventory.cpp - `--selftest`: the Inventory class: stacking, slots, weight.
#include "tests/selftest_ctx.hpp"

void testInventoryClass(SelfTest& t) {
    [[maybe_unused]] Game& g = t.g;
    [[maybe_unused]] auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    [[maybe_unused]] const int MEAT = t.MEAT, HIDE = t.HIDE, BONE = t.BONE;
    [[maybe_unused]] const int R_BONE = t.R_BONE, R_ARROW = t.R_ARROW, R_BOLT = t.R_BOLT, R_BANDAGE = t.R_BANDAGE;
    [[maybe_unused]] auto fillWithJunk = [&]() { t.fillWithJunk(); };
    [[maybe_unused]] auto findRecipe = [&](int kind, int out, int firstMat) { return t.findRecipe(kind, out, firstMat); };
    [[maybe_unused]] auto writeFile = [&](const std::string& text) { t.writeFile(text); };
    [[maybe_unused]] auto removeAll = [&]() { t.removeAll(); };
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
}
