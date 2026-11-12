// selftest_screens.cpp - `--selftest`: inventory screen, crafting, saving and migration.
#include "tests/selftest_ctx.hpp"
#include "world/gates.hpp"

void testScreensAndSaves(SelfTest& t) {
    [[maybe_unused]] Game& g = t.g;
    [[maybe_unused]] auto check = [&](bool ok, const char* what) { t.check(ok, what); };
    [[maybe_unused]] const int MEAT = t.MEAT, HIDE = t.HIDE, BONE = t.BONE;
    [[maybe_unused]] const int R_BONE = t.R_BONE, R_ARROW = t.R_ARROW, R_BOLT = t.R_BOLT, R_BANDAGE = t.R_BANDAGE;
    [[maybe_unused]] auto fillWithJunk = [&]() { t.fillWithJunk(); };
    [[maybe_unused]] auto findRecipe = [&](int kind, int out, int firstMat) { return t.findRecipe(kind, out, firstMat); };
    [[maybe_unused]] auto writeFile = [&](const std::string& text) { t.writeFile(text); };
    [[maybe_unused]] auto removeAll = [&]() { t.removeAll(); };
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
        g.in.my = 12 + 50 + 8;
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
    {   // sealed gates
        removeAll();
        // ---- sealed gates: a gate asks for an offering (hunting level + materials, in a chain) before it opens
        g.newGame();
        g.state = ST_PLAY;
        g.travelCd = 0;
        for (int b = 0; b < B_COUNT; b++) g.gateOpen[b] = b == B_HOME || b == B_HOMESTEAD;
        {
            const Portal& east = MAPS[B_HOME].portals[0];
            g.p.pos = V2((east.zone.x0 + 0.5f) * TS, 84.5f * TS);
            const float x0 = g.p.pos.x;
            g.checkPortals(0.f);
            check(g.world.mapId == B_HOME && g.state == ST_GATE && g.gateTarget == B_DESERT && g.p.pos.x < x0, "a sealed gate does not travel: it opens the offering panel and steps the hunter back");
            check(!g.offerAtGate(B_DESERT), "the offering is refused without the materials and level");
            g.p.inv.clear();
            g.p.sk.s[SK_HUNTING].xp = 100000;
            const GateReq* dg = gateFor(B_DESERT);
            for (int i = 0; i < dg->nMat; i++) g.p.inv.addIgnoringWeight(itemMat(dg->mats[i].mat), dg->mats[i].n + 1);
            check(!g.offerAtGate(B_SWAMP) && !g.gateOpen[B_SWAMP], "a gate cannot be opened before the one it follows in the chain");
            check(g.offerAtGate(B_DESERT) && g.gateOpen[B_DESERT], "the right offering opens the desert gate");
            check(g.p.mat(dg->mats[0].mat) == 1, "the offering takes exactly what the gate asks for");
            g.state = ST_PLAY;
            g.travelCd = 0;
            g.p.pos = V2((east.zone.x0 + 0.5f) * TS, 84.5f * TS);
            g.checkPortals(0.f);
            check(g.world.mapId == B_DESERT, "an opened gate travels");
            g.travelTo(B_HOME, 40, 30, false);
            g.travelCd = 0;
            // gates stay open over a save, and a save from before the gates existed keeps everything open
            g.save();
            g.newGame();
            check(!g.gateOpen[B_DESERT], "a new game starts with the gates sealed");
            check(g.load() && g.gateOpen[B_DESERT] && !g.gateOpen[B_SWAMP], "open gates are saved and loaded");
            removeAll();
            writeFile("version 2\ncoins 5\nup 0 0 0 0\n");
            check(g.load() && g.gateOpen[B_SWAMP] && g.gateOpen[B_MOUNTAIN], "a save from before the gates existed keeps every gate open");
            removeAll();
        }
        removeAll();
    }
    {   // settings: rebinding swaps, reserved keys are refused, and settings are saved and loaded
        g.newGame();
        g.cfg = Settings();
        g.rebind = K_MAP;
        g.rebindKey(SDL_SCANCODE_J);
        check(g.cfg.key[K_MAP] == SDL_SCANCODE_J && g.rebind < 0, "a control takes the key pressed");
        g.rebind = K_HEAL;
        g.rebindKey(SDL_SCANCODE_J);
        check(g.cfg.key[K_HEAL] == SDL_SCANCODE_J && g.cfg.key[K_MAP] == SDL_SCANCODE_Q, "binding a key already in use swaps the two controls");
        g.rebind = K_USE;
        g.rebindKey(SDL_SCANCODE_TAB);
        g.rebind = K_USE;
        g.rebindKey(SDL_SCANCODE_ESCAPE);
        check(g.cfg.key[K_USE] == SDL_SCANCODE_E, "reserved keys and Esc leave the binding alone");
        g.cfg.sfx = 3;
        g.cfg.amb = 0;
        g.saveSettings();
        g.cfg = Settings();
        g.loadSettings();
        check(g.cfg.sfx == 3 && g.cfg.amb == 0 && g.cfg.key[K_HEAL] == SDL_SCANCODE_J && g.cfg.key[K_MAP] == SDL_SCANCODE_Q, "settings are saved and loaded");
        g.cfg = Settings();
        g.saveSettings();
        g.loadSettings();
        // the gate list on the map opens a read-only view that returns to the map
        g.state = ST_MAP;
        g.openGateView(B_SNOW);
        check(g.state == ST_GATE && g.gateView, "the map's gate list opens the gate view");
        g.updateGatePanel(0.f);
        g.in.escPressed = true;
        g.updateGatePanel(0.f);
        g.in.escPressed = false;
        check(g.state == ST_MAP, "closing the gate view returns to the map");
        g.gateView = false;
        g.state = ST_PLAY;
    }
    {   // the shop's sell slider opens only for a row of 2 or more
        g.newGame();
        g.state = ST_SHOP;
        g.amountOpenSell(-M_HIDE - 1, 0, 1);
        check(g.amtCtx == Game::AMT_NONE, "no slider for a single item");
        g.amountOpenSell(-M_HIDE - 1, 0, 9);
        check(g.amtCtx == Game::AMT_SELL && g.amtMax == 9 && g.amtVal == 4, "the sell slider opens at half the row");
        g.amountClose();
        g.state = ST_PLAY;
    }
    {   // the bestiary counts hunter kills per species, pays coins at 5 / 20 / 50, and is saved
        removeAll();
        g.newGame();
        g.p.coins = 0;
        for (int i = 0; i < 4; i++) g.bestiaryKill(A_DEER);
        check(g.bestKills[A_DEER] == 4 && g.p.coins == 0, "the bestiary counts kills (no reward before 5)");
        g.bestiaryKill(A_DEER);
        check(g.p.coins == 8 + ANIMAL[A_DEER].hp / 3, "the 5th kill of a species pays the first reward");
        g.bestKills[A_WOLF] = 7;
        g.save();
        g.newGame();
        check(g.bestKills[A_DEER] == 0, "a new game starts with an empty bestiary");
        check(g.load() && g.bestKills[A_DEER] == 5 && g.bestKills[A_WOLF] == 7, "the bestiary is saved and loaded");
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
    printf("full map screen\n");
    {
        g.state = ST_PLAY;
        g.showMap = true;
        g.in.mapToggle = true;
        g.update(1.f / 60.f);
        g.in.mapToggle = false;
        check(g.state == ST_MAP, "M opens the full map screen");
        g.render();  // draws the map with the fog of war, landmarks and gates (no crash)
        g.in.mapToggle = true;
        g.update(1.f / 60.f);
        g.in.mapToggle = false;
        check(g.state == ST_PLAY, "M closes it again");
        g.in.mapToggle = true;
        g.update(1.f / 60.f);
        g.in.mapToggle = false;
        g.in.escPressed = true;
        g.update(1.f / 60.f);
        g.in.escPressed = false;
        check(g.state == ST_PLAY, "ESC closes the map too (and does not pause)");
        g.in.tabPressed = true;
        g.update(1.f / 60.f);
        g.in.tabPressed = false;
        check(!g.showMap, "TAB switches the minimap off");
        g.in.tabPressed = true;
        g.update(1.f / 60.f);
        g.in.tabPressed = false;
        check(g.showMap, "...and on again");
    }
}
