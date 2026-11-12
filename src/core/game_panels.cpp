// game_panels.cpp - the quest, shop and pack screens.
#include <cstdio>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= quest giver
void Game::drawQuestPanel() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 140));
    const int X = 60, Y = 36, W = 280, H = 146;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, "ELDER HOLT - HUNTING QUESTS", C_GOLD);
    std::string qn = "QUEST " + S(questIdx + 1);
    text(X + W - 8 - textW(qn), Y + 7, qn, C_DIM);
    // portrait of the elder
    panel(X + 6, Y + 22, 60, 78, HEX(0x2b1d12));
    {
        const Spr& k = A.quester[((int)(timeAcc * 1.3f)) & 1];
        SDL_Rect d{X + 6 + 30 - k.w, Y + 26, k.w * 2, k.h * 2};
        SDL_RenderCopy(ren, k.t, nullptr, &d);
        textC(X + 36, Y + 88, "ELDER HOLT", C_DIM, 1, false);
    }
    // the quest itself
    const int x0 = X + 74;
    bool ready = questReady();
    text(x0, Y + 23, quest.title, C_GOLD, 2);
    text(x0, Y + 38, quest.desc, C_TEXT);
    float frac = questActive ? (float)questProg / quest.goal : 0.f;
    bar(x0, Y + 52, 150, 6, frac, ready ? C_GREEN : HEX(0xe0a93a));
    text(x0 + 156, Y + 52, questActive ? S(questProg) + "/" + S(quest.goal) : "GOAL " + S(quest.goal), ready ? C_GREEN : C_TEXT);
    // rewards
    text(x0, Y + 66, "REWARD", C_DIM, 1, false);
    int cx = x0;
    std::string cs = S(quest.coins);
    icon(A.coin, cx + 5, Y + 79, 10);
    text(cx + 12, Y + 76, cs, C_GOLD);
    cx += 12 + textW(cs) + 8;
    if (quest.mat >= 0 && quest.matN > 0) {
        std::string ms = "X" + S(quest.matN);
        icon(A.matIcon[quest.mat], cx + 6, Y + 79, 12);
        text(cx + 14, Y + 76, ms, C_TEXT);
        cx += 14 + textW(ms) + 8;
    }
    if (quest.bandages > 0) {
        std::string bs = "X" + S(quest.bandages);
        icon(A.bandage, cx + 6, Y + 79, 12);
        text(cx + 14, Y + 76, bs, C_TEXT);
    }
    // status line + buttons
    if (!questActive) text(x0, Y + 90, "ACCEPT TO START THIS QUEST", C_DIM, 1, false);
    else if (ready) text(x0, Y + 90, "WELL DONE, HUNTER!", C_GREEN, 1, false);
    else text(x0, Y + 90, "KEEP HUNTING - COME BACK WHEN DONE", C_DIM, 1, false);
    const int by = Y + 106;
    if (!questActive) {
        if (button(x0, by, 96, 18, "ACCEPT", true, HEX(0x5d7a2e))) acceptQuest();
    } else if (ready) {
        if (button(x0, by, 96, 18, "TURN IN", true, HEX(0x5d7a2e))) turnInQuest();
    } else {
        button(x0, by, 96, 18, "IN PROGRESS", false);
    }
    if (button(x0 + 102, by, 60, 18, "CLOSE")) {
        state = ST_PLAY;
        save();
    }
    textC(X + W / 2, Y + H - 12, "ESC OR E: CLOSE", C_DIM, 1, false);
}

// ================================================================= shop
void Game::drawShop() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 140));
    const int X = 36, Y = 12, W = 328, H = 201;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, "GENERAL STORE", C_GOLD);
    {
        std::string c = S(p.coins);
        int cw = textW(c);
        icon(A.coin, X + W - 10 - cw - 8, Y + 10, 10);
        text(X + W - 8 - cw, Y + 7, c, C_GOLD);
    }
    const char* tabs[2] = {"SELL", "BUY"};  // the general store only trades: crafting lives in the workshops, gear upgrades at the smith and tanner
    for (int i = 0; i < 2; i++) {
        int ty = Y + 22 + i * 22;
        bool sel = shopTab == i;
        if (button(X + 6, ty, 66, 19, tabs[i], true, sel ? HEX(0x92693a) : HEX(0x5a4028))) shopTab = i;
        if (sel) {
            fillRect(X + 72, ty + 3, 3, 13, C_GOLD);
        }
    }
    // shopkeeper portrait
    panel(X + 6, Y + 112, 66, 72, HEX(0x2b1d12));
    {
        const Spr& k = A.T.npc[BLD_SHOP][((int)(timeAcc * 1.6f)) & 1];
        SDL_Rect d{X + 6 + 33 - k.w, Y + 114, k.w * 2, k.h * 2};
        SDL_RenderCopy(ren, k.t, nullptr, &d);
        textC(X + 39, Y + 168, "GARRICK", C_DIM, 1, false);
        const char* lines[] = {"GOOD HUNTING!", "FRESH HIDES?", "I PAY WELL!", "SHARP KNIVES!"};
        textC(X + 39, Y + 176, lines[((int)(titleT + timeAcc * 0.15f)) % 4], C_TEXT, 1, false);
    }
    const int x0 = X + 80, cw = 240;
    // ---------------- SELL: only what you carry, scrollable (there are 32 materials now)
    if (shopTab == 0) {
        text(x0 + 20, Y + 22, "ITEM", C_DIM, 1, false);
        text(x0 + 82, Y + 22, "HAVE", C_DIM, 1, false);
        text(x0 + 112, Y + 22, "EACH", C_DIM, 1, false);
        struct Row { int id, q, n, price; };  // id < 0: material -id-1 (sold by sellMat); otherwise a crafted item of quality q
        std::vector<Row> owned;
        int total = 0;
        bool partsHeld = false;
        auto isPart = [](int m) { return isComponent(m); };  // crafting components: kept by SELL EVERYTHING
        for (int m = 0; m < M_COUNT; m++)
            if (p.mat(m) > 0) {
                owned.push_back({-m - 1, 0, p.mat(m), MAT_PRICE[m]});
                if (isPart(m)) partsHeld = true;
                else total += p.mat(m) * MAT_PRICE[m];
            }
        for (const Slot& sl : p.inv.slots) {  // crafted things, one row per item and quality (listed once)
            if (sl.empty() || !itemHasQuality(sl.item)) continue;
            bool seen = false;
            for (const Row& r : owned) seen = seen || (r.id == sl.item && r.q == sl.q);
            const int n = sellable(sl.item, sl.q);
            if (!seen && n > 0) owned.push_back({sl.item, sl.q, n, qualityPrice(itemSellBase(sl.item), sl.q)});
        }
        const int ROWS = 8, maxScroll = std::max(0, (int)owned.size() - ROWS);
        sellScroll = std::max(0, std::min(maxScroll, sellScroll));
        if (maxScroll > 0) text(x0 + 150, Y + 22, "WHEEL: MORE (" + S(sellScroll + 1) + "-" + S(std::min((int)owned.size(), sellScroll + ROWS)) + "/" + S((int)owned.size()) + ")", C_DIM, 1, false);
        if (owned.empty()) {
            textC(x0 + cw / 2, Y + 70, "NOTHING TO SELL YET", C_DIM);
            textC(x0 + cw / 2, Y + 82, "HUNT ANIMALS OR GATHER FROM THE LAND", C_DIM, 1, false);
        }
        for (int i = 0; i < ROWS && sellScroll + i < (int)owned.size(); i++) {
            const Row row = owned[sellScroll + i];
            const bool isMat = row.id < 0;
            const int m = -row.id - 1;
            int y = Y + 30 + i * 17;
            fillRect(x0, y, cw, 16, (i & 1) ? RGBA(0, 0, 0, 40) : RGBA(0, 0, 0, 80));
            if (isMat) icon(A.matIcon[m], x0 + 10, y + 8, 14);
            else {
                qualityFrame(x0 + 3, y + 1, 14, 14, row.q);
                icon(itemIcon(row.id), x0 + 10, y + 8, 11);
            }
            text(x0 + 21, y + 5, isMat ? std::string(MAT_NAME[m]) : item(row.id).name.substr(0, 14), isMat || row.q == Q_NORMAL ? C_TEXT : qualityCol(row.q));
            text(x0 + 84, y + 5, S(row.n), C_TEXT);
            icon(A.coin, x0 + 114, y + 8, 8);
            text(x0 + 120, y + 5, S(row.price), C_GOLD);
            if (button(x0 + 138, y + 1, 46, 14, "SELL 1", true)) { if (isMat) sellMat(m, 1); else sellItem(row.id, row.q, 1); }
            if (button(x0 + 188, y + 1, 50, 14, "SELL ALL", true)) { if (isMat) sellMat(m, p.mat(m)); else sellItem(row.id, row.q, row.n); }
        }
        std::string lab = total > 0 ? std::string(partsHeld ? "SELL ALL BUT PARTS  +" : "SELL EVERYTHING  +") + S(total) + " COINS" : "NOTHING TO SELL - GO HUNT!";
        if (button(x0, Y + 30 + ROWS * 17 + 3, cw, 17, lab, total > 0, HEX(0x5d7a2e))) {
            for (int m = 0; m < M_COUNT; m++)
                if (!isPart(m)) sellMat(m, p.mat(m));
            msg("SOLD EVERYTHING FOR " + S(total) + " COINS", C_GOLD);
            save();
        }
    }
    // ---------------- BUY
    if (shopTab == 1) {
        struct Good {
            const char* name;
            const char* desc;
            int price;
            int kind;  // 0 flint x3, 1 bandage x1, 2 bandage x3, 3 bow, 4 crossbow, 5 arrows, 6 bolts
        };
        static const Good goods[] = {{"FLINT KNIFE X3", "CHEAP THROWING KNIVES", FLINT_PACK_PRICE, 0},
                                     {"BANDAGE X1", "HEALS 45 HP - PRESS Q", 15, 1},
                                     {"BANDAGE X3", "SAVE A FEW COINS", 40, 2},
                                     {"HUNTING BOW", "HOLD TO DRAW - NEEDS ARROWS", BOW_PRICE, 3},
                                     {"CROSSBOW", "SLOW RELOAD, HITS HARD - NEEDS BOLTS", CROSSBOW_PRICE, 4},
                                     {"ARROW X10", "FOR THE HUNTING BOW", AMMO[AM_ARROW].price * AMMO[AM_ARROW].pack, 5},
                                     {"BOLT X6", "FOR THE CROSSBOW", AMMO[AM_BOLT].price * AMMO[AM_BOLT].pack, 6}};
        constexpr int NG = sizeof(goods) / sizeof(goods[0]), PER = 3, PAGES = (NG + PER - 1) / PER;
        for (int i = 0; i < PER; i++) {
            int gi = shopPage * PER + i;
            if (gi >= NG) break;
            const Good& g = goods[gi];
            int y = Y + 22 + i * 42;
            panel(x0, y, cw, 38, HEX(0x3a2a1a));
            const Spr& ic = g.kind == 0 ? A.wpnIcon[W_FLINT] : g.kind == 3 ? A.wpnIcon[W_BOW] : g.kind == 4 ? A.wpnIcon[W_CROSSBOW] : g.kind == 5 ? A.ammoIcon[AM_ARROW] : g.kind == 6 ? A.ammoIcon[AM_BOLT] : A.bandage;
            icon(ic, x0 + 18, y + 19, 28);
            text(x0 + 36, y + 6, g.name, C_GOLD);
            text(x0 + 36, y + 15, g.desc, C_DIM);
            // what the item is, and how many a purchase adds
            static const int goodItem[7] = {ITEM_WPN0 + W_FLINT, ITEM_CONS0 + C_BANDAGE, ITEM_CONS0 + C_BANDAGE, ITEM_WPN0 + W_BOW,
                                            ITEM_WPN0 + W_CROSSBOW, ITEM_AMMO0 + AM_ARROW, ITEM_AMMO0 + AM_BOLT};
            const int goodQty[7] = {3, 1, 3, 1, 1, AMMO[AM_ARROW].pack, AMMO[AM_BOLT].pack};
            const int gItem = goodItem[g.kind], gQty = goodQty[g.kind];
            int owned = p.inv.count(gItem);
            text(x0 + 36, y + 24, "OWNED " + S(owned), C_TEXT);
            chip(x0 + 90, y + 21, A.coin, p.coins, g.price);
            const bool launcherOwned = (g.kind == 3 || g.kind == 4) && owned >= 1;
            const bool full = launcherOwned || !p.inv.canAdd(gItem, gQty);
            bool ok = p.coins >= g.price && !full;
            const char* lab = launcherOwned ? "OWNED" : full ? (p.inv.blockReason(gItem, gQty) == 2 ? "HEAVY" : "FULL") : "BUY";
            if (button(x0 + 186, y + 11, 48, 16, lab, ok, HEX(0x5d7a2e))) {
                p.coins -= g.price;
                p.inv.add(gItem, gQty);
                audio.play(S_COIN);
                save();
            }
        }
        if (button(x0, Y + 22 + PER * 42 - 2, 70, 12, shopPage + 1 < PAGES ? "MORE GOODS >" : "< BACK TO START", true, HEX(0x4a3a5e))) shopPage = (shopPage + 1) % PAGES;
        text(x0 + 76, Y + 22 + PER * 42 + 1, "PAGE " + S(shopPage + 1) + "/" + S(PAGES), C_DIM, 1, false);
    }
    textC(X + W / 2, Y + H - 10, "ESC OR E: LEAVE     TAB OR 1-2: SWITCH TAB     " + kgNum(p.inv.totalWeight()) + "/" + kgNum(p.inv.maxWeight) + " KG", C_DIM, 1, false);
}

// ================================================================= inventory screen
void Game::drawInventory() {
    constexpr int COLS = INV_COLS, CELL = 16;
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 150));
    const int X = 24, Y = 12, W = 352, H = 200;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, "PACK", C_GOLD);
    const int cap = p.bagCap(), used = p.bagUsed();
    textC(X + W / 2, Y + 7, "I / ESC CLOSE  TAB NEXT PAGE", C_DIM, 1, false);
    {
        std::string cs = S(p.coins);
        text(X + W - 10 - textW(cs), Y + 7, cs, C_GOLD);
        icon(A.coin, X + W - 16 - textW(cs), Y + 10, 8);
    }
    {
        static const char* tabs[3] = {"BACKPACK", "CRAFTING", "SKILLS"};
        int tx = X + 8;
        for (int i = 0; i < 3; i++) {
            const int w = textW(tabs[i]) + 10;
            if (button(tx, Y + 20, w, 13, tabs[i], true, invTab == i ? HEX(0x8a6a3a) : HEX(0x4a3a2a))) invTab = i;
            tx += w + 2;
        }
    }
    // category filter for the grid: items outside the chosen category are dimmed (the grid itself never moves)
    auto filterOf = [](int id) {
        switch (item(id).kind) {
            case IK_MATERIAL: return 1;
            case IK_THROWN: case IK_LAUNCHER: case IK_AMMO: return 2;
            case IK_TOOL: return 3;
            default: return 4;
        }
    };
    if (invTab == 0) {
        static const char* fl[5] = {"ALL", "MATERIALS", "WEAPONS", "TOOLS", "SUPPLIES"};
        int fx = X + 132;
        for (int i = 0; i < 5; i++) {
            int w = textW(fl[i]) + 10;
            if (button(fx, Y + 20, w, 13, fl[i], true, invFilter == i ? HEX(0x8a6a3a) : HEX(0x4a3a2a))) invFilter = i;
            fx += w + 2;
        }
    }
    const int cy = Y + 38;
    auto iconOf = [&](int id) -> const Spr& { return itemIcon(id); };
    auto sellPriceOf = [&](int id, int q) { return itemHasQuality(id) ? qualityPrice(itemSellBase(id), q) : item(id).price; };

    if (invTab == 0) {
        // ---- the 100-slot grid (10 x 10): always exactly 100 slots, bags never change it
        const int gx = X + 8, gy = cy;
        int hover = -1;
        for (int i = 0; i < cap; i++) {
            int x = gx + (i % COLS) * CELL, y = gy + (i / COLS) * CELL;
            if (inRect(x, y, CELL - 1, CELL - 1)) hover = i;
            if (hover == i && in.uiClick) {
                invSel = i;
                if (!p.inv.slots[i].empty()) {  // pressing a stack also starts dragging it
                    invDrag = i;
                    invDragMoved = false;
                    invDragX = in.mx;
                    invDragY = in.my;
                }
                in.uiClick = false;
                in.lPressed = false;
                audio.play(S_CLICK, 0.5f);
            }
            if (hover == i && in.rPressed) {  // right click: use / equip / place / wear it
                in.rPressed = false;
                invSel = i;
                useSlot(i);
            }
        }
        if (invDrag >= 0 && in.l && std::abs(in.mx - invDragX) + std::abs(in.my - invDragY) > 3) invDragMoved = true;
        // a drag released over another grid slot moves / merges / swaps; over a hotbar or tool slot it equips (handled with those slots)
        const bool dropNow = invDrag >= 0 && invDragMoved && !in.l;
        if (dropNow && hover >= 0 && hover != invDrag) {
            if (p.inv.moveSlot(invDrag, hover, p.inv.slots[invDrag].n) > 0) {
                invSel = hover;
                audio.play(S_CLICK, 0.5f);
            }
        }
        for (int i = 0; i < cap; i++) {
            int x = gx + (i % COLS) * CELL, y = gy + (i / COLS) * CELL;
            const Slot& sl = p.inv.slots[i];
            bool sel = i == invSel;
            panel(x, y, CELL - 1, CELL - 1, sel ? HEX(0x6a4a2a) : hover == i ? HEX(0x4a3624) : HEX(0x33241a));
            if (sel) {
                fillRect(x, y + 1, 1, CELL - 3, C_GOLD);
                fillRect(x + CELL - 2, y + 1, 1, CELL - 3, C_GOLD);
                fillRect(x + 1, y, CELL - 3, 1, C_GOLD);
                fillRect(x + 1, y + CELL - 2, CELL - 3, 1, C_GOLD);
            }
            if (!sl.empty()) {
                if (itemHasQuality(sl.item) && !sel) qualityFrame(x, y, CELL - 1, CELL - 1, sl.q);
                icon(iconOf(sl.item), x + (CELL - 1) / 2, y + (CELL - 1) / 2 - 1, 11, (invFilter == 0 || filterOf(sl.item) == invFilter) ? 1.f : 0.25f);
                if (sl.n > 1) {
                    std::string n = S(sl.n);
                    text(x + CELL - 1 - textW(n), y + CELL - 7, n, C_TEXT);
                }
                if (p.hotSlotOf(sl.item) >= 0) fillRect(x + 2, y + 2, 2, 2, C_GREEN);  // equipped on the hotbar
            }
        }
        // ---- carrying weight, worn bag, slots
        const int dx = gx + COLS * CELL + 8, dw = X + W - 8 - dx;
        const float tw = p.inv.totalWeight(), mw = p.inv.maxWeight;
        const float frac = std::max(0.f, std::min(1.f, mw > 0 ? tw / mw : 1.f));
        const u32 wcol = tw > mw + 1e-3f ? C_RED : frac >= 0.85f ? C_ORANGE : frac >= 0.6f ? C_GOLD : C_GREEN;
        text(dx, cy + 1, "CARRYING WEIGHT", C_DIM);
        const std::string ws = kgNum(tw) + " / " + kgNum(mw) + " KG";
        text(dx + dw - textW(ws), cy + 1, ws, wcol);
        fillRect(dx, cy + 10, dw, 9, HEX(0x1a120b));
        fillRect(dx + 1, cy + 11, (int)((dw - 2) * frac), 7, wcol);
        textC(dx + dw / 2, cy + 12, S((int)lroundf(mw > 0 ? 100.f * tw / mw : 100.f)) + "%", C_TEXT, 1, false);
        text(dx, cy + 24, p.bag >= 0 ? item(itemPack(p.bag)).name : "NO BAG WORN", p.bag >= 0 ? C_TEXT : C_DIM);
        if (button(dx + dw - 44, cy + 22, 44, 11, "TAKE OFF", p.bag >= 0, HEX(0x4a3a5e))) unequipBag();
        text(dx, cy + 35, "SLOTS " + S(used) + "/" + S(cap) + "  STACKS TO " + S(MAX_STACK), used >= cap ? C_RED : C_DIM, 1, false);
        // ---- details of the hovered (or selected) slot
        const int py = cy + 44, ph = 56;
        panel(dx, py, dw, ph, HEX(0x3a2a1a));
        const int show = std::max(0, std::min(cap - 1, hover >= 0 ? hover : invSel));
        const Slot& sl = p.inv.slots[show];
        if (sl.empty()) {
            text(dx + 6, py + 8, "EMPTY SLOT", C_DIM);
            text(dx + 6, py + 20, "DRAG TO MOVE OR EQUIP, RIGHT CLICK TO USE", C_DIM, 1, false);
        } else {
            const ItemDef& D = item(sl.item);
            icon(iconOf(sl.item), dx + 14, py + 14, 20);
            const bool hq = itemHasQuality(sl.item);
            text(dx + 28, py + 5, D.name, hq && sl.q > Q_NORMAL ? qualityCol(sl.q) : C_GOLD);
            text(dx + 28, py + 14, std::string(ITEM_KIND_NAME[D.kind]) + (hq ? std::string(" - ") + qualityDef(sl.q).name : std::string()), hq ? qualityCol(sl.q) : C_DIM, 1, false);
            // equip / use / place / wear button for the selected item, in its info card
            const bool canAct = D.kind == IK_THROWN || D.kind == IK_LAUNCHER || D.kind == IK_AMMO || D.kind == IK_CONSUMABLE || D.kind == IK_PLACEABLE || D.kind == IK_BACKPACK || seedCropOf(sl.item) >= 0 || (D.kind == IK_TOOL && isFarmType(TOOLDEF[D.ref].type));
            if (canAct && show == invSel) {
                const char* lab = D.kind == IK_CONSUMABLE ? "USE" : D.kind == IK_PLACEABLE ? "PLACE" : D.kind == IK_BACKPACK ? "WEAR" : "EQUIP";
                if (button(dx + dw - 42, py + 4, 38, 13, lab, true, HEX(0x5d7a2e))) useSlot(invSel);
            }
            int ty = py + 26;
            text(dx + 6, ty, "QTY " + S(sl.n) + "  EACH " + kgNum(D.weight) + " KG", C_TEXT, 1, false);
            ty += 8;
            text(dx + 6, ty, "STACK " + kgNum(D.weight * sl.n) + " KG" + (sellPriceOf(sl.item, sl.q) > 0 ? "  SELLS " + S(sellPriceOf(sl.item, sl.q)) : std::string("")), C_TEXT, 1, false);
            ty += 8;
            auto wrap = [&](const std::string& txt, u32 col) {
                const size_t maxc = (size_t)std::max(8, (dw - 10) / 4);
                size_t pos = 0;
                while (pos < txt.size() && ty <= py + ph - 8) {
                    size_t len = std::min(maxc, txt.size() - pos);
                    if (pos + len < txt.size()) {
                        size_t sp = txt.rfind(' ', pos + len);
                        if (sp != std::string::npos && sp > pos) len = sp - pos;
                    }
                    text(dx + 6, ty, txt.substr(pos, len), col, 1, false);
                    ty += 8;
                    pos += len;
                    while (pos < txt.size() && txt[pos] == ' ') pos++;
                }
            };
            if (D.kind == IK_THROWN || D.kind == IK_LAUNCHER) {
                const WpnDef& Wd = WPN[D.ref];
                wrap("DMG " + S((int)Wd.dmg) + "  RANGE " + S((int)Wd.range) + (D.kind == IK_LAUNCHER ? std::string("  FIRES ") + AMMO[Wd.ammo].name + "S" : std::string("")), C_TEXT);
                if (Wd.biome >= 0) wrap(std::string("+30% IN THE ") + BIOME[Wd.biome].name, BIOME[Wd.biome].col);
                if (p.equippedWpn(D.ref)) wrap(D.ref == p.sel ? "EQUIPPED - IN HAND" : "EQUIPPED ON THE HOTBAR", C_GREEN);
            } else if (D.kind == IK_AMMO) {
                for (int w = 0; w < W_COUNT; w++)
                    if (WPN[w].ammo == D.ref) {
                        wrap(std::string("FIRED BY THE ") + WPN[w].name, C_TEXT);
                        break;
                    }
            } else if (D.kind == IK_TOOL) {
                const ToolDef& T = TOOLDEF[D.ref];
                wrap(D.blurb, C_DIM);
                wrap(std::string(T.type == TT_AXE ? "CHOPS" : T.type == TT_HOE ? "TILLS AND DIGS" : T.type == TT_CAN ? "WATERS CROPS" : T.type == TT_SICKLE ? "CUTS GRAIN" : "MINES") + " TIER " + S(T.tier) + "  BONUS +" + S((int)lroundf(T.bonus * 100)) + "%", C_TEXT);
            } else {
                wrap(D.blurb, C_DIM);
                if (D.kind == IK_CONSUMABLE && p.hotSlotOf(sl.item) >= 0) wrap("EQUIPPED ON THE HOTBAR", C_GREEN);
            }
        }
        // ---- actions on the selected slot
        const Slot& cur = p.inv.slots[std::max(0, std::min(cap - 1, invSel))];
        const ItemDef* cd = cur.empty() ? nullptr : &item(cur.item);
        int by = py + ph + 2;
        // the amount slider: how many a DROP or SPLIT takes from the selected stack
        if (invAmtSlot != invSel) {
            invAmtSlot = invSel;
            invAmt = 1;
        }
        invAmt = std::max(1, std::min(std::max(1, cur.n), invAmt));
        {
            const int sx = dx + 2, sw = dw - 32, sy = by + 5;
            const bool many = cd && cur.n > 1;
            fillRect(sx, sy, sw, 4, HEX(0x1a120b));
            const float f = many ? (invAmt - 1) / (float)(cur.n - 1) : 0.f;
            if (many) fillRect(sx, sy, (int)(sw * f), 4, HEX(0x8ec2f0));
            fillRect(sx + (int)((sw - 3) * f), sy - 2, 4, 8, many ? (invAmtDrag ? C_GOLD : HEX(0xe8dcc0)) : HEX(0x5a4a3a));
            if (many && in.uiClick && inRect(sx - 3, sy - 5, sw + 6, 14)) {
                invAmtDrag = true;
                in.uiClick = false;
                in.lPressed = false;
            }
            if (!in.l) invAmtDrag = false;
            if (invAmtDrag) invAmt = std::max(1, std::min(cur.n, 1 + (int)lroundf((in.mx - sx) / (float)(sw - 1) * (cur.n - 1))));
            if (many && inRect(sx, by, dw, 13)) invAmt = std::max(1, std::min(cur.n, invAmt - in.wheel));
            textC(dx + dw - 14, by + 3, cd ? S(invAmt) + "/" + S(cur.n) : "--", cd ? C_GOLD : C_DIM, 1, false);
        }
        by += 14;
        if (button(dx, by, 33, 13, "SPLIT", cd != nullptr && cur.n > 1, HEX(0x4a5a7a))) {  // the chosen amount goes into the first free slot
            int to = -1;
            for (int i = 0; i < cap && to < 0; i++)
                if (p.inv.slots[i].empty()) to = i;
            if (to < 0) {
                msg("NO FREE SLOT TO SPLIT INTO", C_RED);
                audio.play(S_ERROR, 0.4f);
            } else {
                p.inv.moveSlot(invSel, to, std::min(invAmt, cur.n - 1));
                audio.play(S_CLICK, 0.5f);
            }
        }
        if (button(dx + 36, by, 33, 13, "DROP", cd != nullptr, HEX(0x7a4a2e))) dropSlot(invSel, invAmt);
        if (button(dx + 72, by, 33, 13, "SORT", true, HEX(0x4a3a5e))) {
            p.inv.compact();
            msg("PACK SORTED", C_DIM);
        }
        // ---- the hotbar: click a slot to equip the selected item there, or to unequip what is in it
        const int toolRowY = by;  // the farming tool slots share the row of the SORT button
        by += 14;
        text(dx, by, "HOTBAR", C_DIM, 1, false);
        for (int i = 0; i < HOT_N; i++) {
            const int hx = dx + i * 21, hy = by + 7;
            const int id = p.hot[i];
            const bool accepts = cd && hotAccepts(HOT_KIND[i], cur.item);
            const bool over = inRect(hx, hy, 19, 16);
            panel(hx, hy, 19, 16, accepts ? HEX(0x4e6a3a) : over ? HEX(0x4a3624) : HEX(0x33241a));
            if (id >= 0) {
                if (itemHasQuality(id) && p.inv.count(id) > 0) qualityFrame(hx, hy, 19, 16, p.inv.bestQuality(id));
                icon(itemIcon(id), hx + 10, hy + 8, 11, p.inv.count(id) > 0 ? 1.f : 0.35f);
                if (item(id).kind == IK_THROWN || item(id).kind == IK_LAUNCHER) {
                    if (item(id).ref == p.sel) fillRect(hx + 1, hy + 14, 17, 1, C_GREEN);
                }
                else if (item(id).kind == IK_TOOL && p.tool[TOOLDEF[item(id).ref].type] == item(id).ref) fillRect(hx + 1, hy + 14, 17, 1, C_GREEN);
            }
            text(hx + 2, hy + 1, S(i + 1), HEX(0x9a8a6a), 1, false);
            if (over && dropNow && accepts) {  // dragged here from the pack
                equipHot(cur.item, i);
            } else if (over && in.uiClick) {
                in.uiClick = false;
                in.lPressed = false;
                if (accepts) equipHot(cur.item, i);
                else if (id >= 0) unequipHot(i);
                else {
                    msg(std::string("SLOT ") + S(i + 1) + " TAKES " + HOT_KIND_NAME[HOT_KIND[i]] + " ITEMS", C_DIM);
                    audio.play(S_ERROR, 0.4f);
                }
            }
        }
        // ---- the farming tools: one slot each for the hoe, watering can and sickle. Click a slot to equip the selected tool there, or to take
        // what is in it off (it goes back into the pack).
        {
            const int fy = toolRowY;
            static const int FT[3] = {TT_HOE, TT_CAN, TT_SICKLE};
            static const int FT_DEFAULT[3] = {TL_HOE, TL_CAN, TL_SICKLE};
            std::string info = "FARM TOOLS: CLICK A SLOT";
            for (int i = 0; i < 3; i++) {
                const int hx = dx + 108 + i * 21, type = FT[i], id = p.tool[type];
                const bool fits = cd && cd->kind == IK_TOOL && TOOLDEF[cd->ref].type == type;
                const bool over = inRect(hx, fy, 19, 13);
                panel(hx, fy, 19, 13, fits ? HEX(0x4e6a3a) : over ? HEX(0x4a3624) : HEX(0x33241a));
                if (id >= 0) icon(itemIcon(itemTool(id)), hx + 10, fy + 6, 11);
                else icon(itemIcon(itemTool(FT_DEFAULT[i])), hx + 10, fy + 6, 11, 0.25f);
                if (over) info = id >= 0 ? std::string(TOOLDEF[id].name) + " - CLICK TO TAKE OFF" : std::string(TOOLDEF[FT_DEFAULT[i]].name) + " SLOT - EMPTY";
                if (over && dropNow && fits) {
                    equipFarmTool(invSel);
                } else if (over && in.uiClick) {
                    in.uiClick = false;
                    in.lPressed = false;
                    if (fits) equipFarmTool(invSel);
                    else if (id >= 0) unequipFarmTool(type);
                    else {
                        msg(std::string("THIS SLOT TAKES THE ") + TOOLDEF[FT_DEFAULT[i]].name, C_DIM);
                        audio.play(S_ERROR, 0.4f);
                    }
                }
            }
            text(dx + 40, fy + 14, info, C_DIM, 1, false);  // on the HOTBAR label line
        }
        // ---- the dragged stack follows the mouse
        if (invDrag >= 0 && invDragMoved && in.l && !p.inv.slots[invDrag].empty()) icon(iconOf(p.inv.slots[invDrag].item), in.mx, in.my, 12);
        if (!in.l) invDrag = -1;
    } else if (invTab == 1) {
        drawCraftTab(X, Y, W, H);
    } else {
        drawSkillsTab(X, Y, W, H);
    }
}
