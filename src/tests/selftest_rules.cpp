// selftest_rules.cpp - `--selftest`: game rules: new hunter, full pack, hotbar, weapons, recipes.
#include "tests/selftest_ctx.hpp"

void testGameRules(SelfTest& t) {
    [[maybe_unused]] Game& g = t.g;
    [[maybe_unused]] auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    [[maybe_unused]] const int MEAT = t.MEAT, HIDE = t.HIDE, BONE = t.BONE;
    [[maybe_unused]] const int R_BONE = t.R_BONE, R_ARROW = t.R_ARROW, R_BOLT = t.R_BOLT, R_BANDAGE = t.R_BANDAGE;
    [[maybe_unused]] auto fillWithJunk = [&]() { t.fillWithJunk(); };
    [[maybe_unused]] auto findRecipe = [&](int kind, int out, int firstMat) { return t.findRecipe(kind, out, firstMat); };
    [[maybe_unused]] auto writeFile = [&](const std::string& text) { t.writeFile(text); };
    [[maybe_unused]] auto removeAll = [&]() { t.removeAll(); };
    printf("game rules\n");
    g.newGame();
    g.state = ST_PLAY;
    g.helpT = 0;
    check(g.p.bagCap() == 100 && g.p.bagUsed() == 2 && g.p.wpn(W_FLINT) == 8 && g.p.bandages() == 1, "a new hunter has a 100-slot pack with 8 flint knives and 1 bandage (2 slots)");
    check(g.p.bag < 0 && g.p.carryLimit() == 50 && g.p.hot[0] == itemWpn(W_FLINT) && g.p.hot[3] == ITEM_CONS0 + C_BANDAGE && g.p.equippedWpn(W_FLINT), "...no bag (50 kg), and the knife and the bandage start equipped on the hotbar");
    // fill every slot with something other than meat/bone/hide
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
    {   // crafted quality: a forced tier gives the item that tier; the roll is otherwise random and skill-based
        g.p.inv.clear();
        g.p.inv.maxWeight = 1e6f;
        g.p.inv.add(BONE, 30);
        g.p.inv.add(HIDE, 10);
        g.craftQuality = Q_MASTERPIECE;
        check(g.craft(R_BONE) && g.p.inv.countQ(itemWpn(W_BONE), Q_MASTERPIECE) == 4 && g.p.inv.bestQuality(itemWpn(W_BONE)) == Q_MASTERPIECE, "a masterpiece craft yields masterpiece pieces");
        g.craftQuality = Q_NORMAL;
        check(g.craft(R_BONE) && g.p.inv.countQ(itemWpn(W_BONE), Q_NORMAL) == 4 && g.p.inv.usedSlots() >= 3, "a normal craft stacks separately from the masterpiece ones");
        g.craftQuality = -1;
        int seen[Q_COUNT] = {0, 0, 0, 0, 0};
        for (int i = 0; i < 40; i++) {
            g.p.inv.clear();
            g.p.inv.add(BONE, 3);
            g.p.inv.add(HIDE, 1);
            if (!g.craft(R_BONE)) break;
            seen[g.p.inv.bestQuality(itemWpn(W_BONE))]++;
        }
        check(seen[Q_NORMAL] > 0 && seen[Q_NORMAL] + seen[Q_GOOD] + seen[Q_OUTSTANDING] + seen[Q_EXCELLENT] + seen[Q_MASTERPIECE] == 40, "unforced crafts roll a tier each time (mostly normal at level 1)");
        g.craftQuality = Q_NORMAL;
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
        check(!g.equipHot(itemWpn(W_SPEAR)) && !g.equipHot(-1), "an item that is not in the pack, or no item, cannot be equipped");
        {   // every category can go on the hotbar: materials, ammunition and tools too
            const bool all = g.equipHot(itemMat(M_MEAT)) && g.equipHot(itemAmmo(AM_ARROW)) && g.equipHot(itemTool(TL_AXE1)) == false;  // (no axe in the pack yet)
            g.p.inv.add(itemTool(TL_AXE1), 1);
            const bool axeOk = g.equipHot(itemTool(TL_AXE1));
            check(all && axeOk && g.p.hotSlotOf(itemMat(M_MEAT)) >= 0 && g.p.hotSlotOf(itemAmmo(AM_ARROW)) >= 0 && g.p.hotSlotOf(itemTool(TL_AXE1)) >= 0, "every category of item (material, ammunition, tool) can be put on the hotbar");
            g.unequipHot(g.p.hotSlotOf(itemMat(M_MEAT)));
            g.unequipHot(g.p.hotSlotOf(itemAmmo(AM_ARROW)));
            g.unequipHot(g.p.hotSlotOf(itemTool(TL_AXE1)));
            g.p.inv.remove(itemTool(TL_AXE1), 1);
        }
        check(!g.equipHot(itemWpn(W_BOW), HOT_N) && !g.equipHot(itemWpn(W_BOW), 99) && g.p.hotSlotOf(itemWpn(W_BOW)) < 0, "a slot that does not exist refuses the item");
        check(g.equipHot(itemCons(C_COOKED), 7) && g.p.hot[7] == itemCons(C_COOKED) && g.equipHot(itemWpn(W_BOW), 6) && g.p.hot[6] == itemWpn(W_BOW), "every hotbar slot is universal: food in slot 8, a weapon in slot 7");
        g.unequipHot(7);
        g.unequipHot(6);
        g.p.sel = W_FLINT;
        check(!g.canUse(W_BOW), "an unequipped weapon cannot be used even though it is in the pack");
        check(g.equipHot(itemWpn(W_BOW)) && g.p.hot[1] == itemWpn(W_BOW) && g.p.sel == W_BOW && g.canUse(W_BOW), "equipping the bow puts it in the first free weapon slot and takes it in hand");
        check(g.equipHot(itemCons(C_COOKED), 5) && g.p.hot[5] == itemCons(C_COOKED) && g.equipHot(itemCons(C_BREW), 6) && g.p.hot[6] == itemCons(C_BREW), "food and buffs can be put in any chosen slot");
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
            check(g.p.sel == W_BOW && g.p.hotSel == g.p.hotSlotOf(itemWpn(W_BOW)), "the mouse wheel moves to the next hotbar slot and takes a weapon in hand");
            g.p.inv.add(itemWpn(W_SPEAR), 1);
            bool reachedFlint = false, spearSeen = false;
            for (int k = 0; k < HOT_N + 1; k++) {  // the wheel visits every equipped item (food, healing...) and never an unequipped spear
                g.in.wheel = -1;
                g.update(1.f / 60.f);
                g.in.wheel = 0;
                if (g.p.hotSel >= 0 && g.p.hot[g.p.hotSel] == itemWpn(W_SPEAR)) spearSeen = true;
                if (g.p.hand < 0 && g.p.sel == W_FLINT && g.p.hotSel == g.p.hotSlotOf(itemWpn(W_FLINT))) reachedFlint = true;
            }
            check(reachedFlint && !spearSeen, "...it comes back round to the knife and skips an unequipped spear in the pack");
            // a consumable reached by the wheel is held, not eaten; a left click uses it
            g.equipHot(itemCons(C_COOKED), 5);
            const int foodSlot = g.p.hotSlotOf(itemCons(C_COOKED));
            const int cookedN = g.p.inv.count(itemCons(C_COOKED));
            g.hotSelect(foodSlot, false);
            check(g.p.hand == HAND_ITEM && g.p.inv.count(itemCons(C_COOKED)) == cookedN, "food selected with the wheel is held, not used");
            g.p.hp = 40;
            g.in.lPressed = true;
            g.update(1.f / 60.f);
            g.in.lPressed = false;
            check(g.p.inv.count(itemCons(C_COOKED)) == cookedN - 1 && g.p.hp > 40 && !g.p.charging, "a left click uses the food in hand (and does not throw a weapon)");
            g.hotSelect(g.p.hotSlotOf(itemWpn(W_FLINT)), false);
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
        // ---- quality survives a save and load; a save without it loads as normal
        {
            g.newGame();
            g.p.inv.clear();
            g.p.inv.maxWeight = 1e6f;
            g.p.inv.add(itemWpn(W_FLINT), 3, Q_GOOD);
            g.p.inv.add(itemWpn(W_FLINT), 2, Q_NORMAL);
            g.p.inv.add(itemPack(PK_SATCHEL), 1, Q_EXCELLENT);
            g.p.armorQ = Q_OUTSTANDING;
            g.p.bootsQ = Q_MASTERPIECE;
            GroundItem gi{};
            gi.pos = g.p.pos + V2(20, 0);
            gi.item = itemWpn(W_FLINT);
            gi.n = 2;
            gi.q = Q_MASTERPIECE;
            g.items.push_back(gi);
            g.useSlot(2);  // wear the excellent satchel
            check(g.p.bag == PK_SATCHEL && g.p.bagQ == Q_EXCELLENT && g.p.carryLimit() > PACK_CARRY_KG[PK_SATCHEL], "an excellent bag carries more than a normal one");
            g.save();
            g.newGame();
            bool ld = g.load();
            check(ld && g.p.inv.countQ(itemWpn(W_FLINT), Q_GOOD) == 3 && g.p.inv.countQ(itemWpn(W_FLINT), Q_NORMAL) == 2, "pack stacks keep their quality through a save");
            check(g.p.armorQ == Q_OUTSTANDING && g.p.bootsQ == Q_MASTERPIECE && g.p.bagQ == Q_EXCELLENT && g.p.bag == PK_SATCHEL && g.p.carryLimit() == (int)lroundf(PACK_CARRY_KG[PK_SATCHEL] * qualityDef(Q_EXCELLENT).stat), "worn armor, boots and bag keep their quality");
            bool dropOk = false;
            for (auto& d : g.items) dropOk = dropOk || (d.item == itemWpn(W_FLINT) && d.q == Q_MASTERPIECE && d.n == 2);
            check(dropOk, "a dropped stack keeps its quality through a save");
            g.p.inv.clear();
            g.dropSlot(0, 1);
            g.p.inv.add(itemWpn(W_FLINT), 2, Q_GOOD);
            g.p.inv.add(itemWpn(W_FLINT), 1, Q_EXCELLENT);
            const int before = (int)g.items.size();
            const int slotE = g.p.inv.slots[0].q == Q_EXCELLENT ? 0 : 1;
            check(g.dropSlot(slotE, 1) && (int)g.items.size() == before + 1 && g.items.back().q == Q_EXCELLENT && g.p.inv.countQ(itemWpn(W_FLINT), Q_EXCELLENT) == 0, "dropping a stack drops that quality and keeps the rest");
            // an old save: no quality fields at all
            writeFile("version 2\ncoins 10\nup 0 0 0 0\nslot flint_knife 4\nslot raw_meat 5\n");
            ld = g.load();
            check(ld && g.p.inv.countQ(itemWpn(W_FLINT), Q_NORMAL) == 4 && g.p.armorQ == Q_NORMAL && g.p.bagQ == Q_NORMAL, "a save from before quality loads everything as normal");
        }
        std::remove(g.savePath().c_str());
        g.newGame();
        g.state = ST_PLAY;
    }
    // ---- inventory screen (key I) and crafting
}
