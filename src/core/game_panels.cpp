// game_panels.cpp - the quest, shop and pack screens.
#include <cstdio>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= quest giver
void Game::drawQuestPanel() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 140));
    const int X = 60, Y = 36, W = 280, H = 146;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, skinFill(UI_BG));
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
    fillRect(X + 3, Y + 3, W - 6, 14, skinFill(UI_BG));
    text(X + 8, Y + 7, "GENERAL STORE", C_GOLD);
    {
        std::string c = S(p.coins);
        int cw = textW(c);
        icon(A.coin, X + W - 10 - cw - 8, Y + 10, 10);
        text(X + W - 8 - cw, Y + 7, c, C_GOLD);
    }
    const char* tabs[3] = {"SELL", "BUY", "ORDERS"};  // the general store only trades: crafting lives in the workshops, gear upgrades at the smith and tanner
    for (int i = 0; i < 3; i++) {
        int ty = Y + 22 + i * 21;
        bool sel = shopTab == i;
        if (button(X + 6, ty, 66, 19, tabs[i], true, sel ? HEX(0x92693a) : HEX(0x5a4028))) shopTab = i;
        if (sel) {
            fillRect(X + 72, ty + 3, 3, 13, C_GOLD);
        }
    }
    {  // standing with the village: better prices as it grows (game_economy.cpp)
        text(X + 6, Y + 88, std::string("REP ") + repName(), C_GOLD, 1, false);
        const int nx = repNext();
        fillRect(X + 6, Y + 98, 66, 4, HEX(0x2b1d12));
        if (nx > 0) fillRect(X + 6, Y + 98, std::min(66, rep * 66 / nx), 4, C_GOLD);
        else fillRect(X + 6, Y + 98, 66, 4, C_GOLD);
        text(X + 6, Y + 104, "SELL +" + S(4 * repLevel()) + "% BUY -" + S(5 * repLevel()) + "%", C_DIM, 1, false);
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
                owned.push_back({-m - 1, 0, p.mat(m), matSellPrice(m)});
                if (isPart(m)) partsHeld = true;
                else total += p.mat(m) * matSellPrice(m);
            }
        for (const Slot& sl : p.inv.slots) {  // crafted things, one row per item and quality (listed once)
            if (sl.empty() || !itemHasQuality(sl.item)) continue;
            bool seen = false;
            for (const Row& r : owned) seen = seen || (r.id == sl.item && r.q == sl.q);
            const int n = sellable(sl.item, sl.q);
            if (!seen && n > 0) owned.push_back({sl.item, sl.q, n, bonusPrice(qualityPrice(itemSellBase(sl.item), sl.q))});
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
            // the price is tinted by demand: green when the store wants it, red when it has too much
            const float dm = isMat && demand[m] > 0 ? demand[m] : 1.f;
            text(x0 + 120, y + 5, S(row.price), dm > 1.04f ? C_GREEN : dm < 0.96f ? HEX(0xff7a6a) : C_GOLD);
            if (button(x0 + 138, y + 1, 40, 14, "SELL 1", true)) { if (isMat) sellMat(m, 1); else sellItem(row.id, row.q, 1); }
            if (button(x0 + 180, y + 1, 20, 14, "...", row.n > 1)) amountOpenSell(row.id, row.q, row.n);  // choose how many with the slider
            if (button(x0 + 202, y + 1, 36, 14, "ALL", true)) { if (isMat) sellMat(m, p.mat(m)); else sellItem(row.id, row.q, row.n); }
        }
        std::string lab = total > 0 ? std::string(partsHeld ? "SELL ALL BUT PARTS  +" : "SELL EVERYTHING  +") + S(total) + " COINS" : "NOTHING TO SELL - GO HUNT!";
        if (button(x0, Y + 30 + ROWS * 17 + 3, cw, 17, lab, total > 0, HEX(0x5d7a2e))) {
            for (int m = 0; m < M_COUNT; m++)
                if (!isPart(m)) sellMat(m, p.mat(m));
            msg("SOLD EVERYTHING FOR " + S(total) + " COINS", C_GOLD);
            save();
        }
    }
    // ---------------- BUY: a scrolling list (hunting gear, then the farm's seeds, tools and hay)
    if (shopTab == 1) {
        struct Good {
            std::string name, desc;
            int price, item, qty;
            int kind;  // 0 ordinary, 1 a launcher (one is enough), 2 a farm tool (equipped when bought, one is enough)
            int tool;
        };
        static std::vector<Good> goods;
        if (goods.empty()) {
            goods.push_back({"FLINT KNIFE X3", "THROWING KNIVES", FLINT_PACK_PRICE, ITEM_WPN0 + W_FLINT, 3, 0, -1});
            goods.push_back({"BANDAGE X1", "HEALS 45 HP - PRESS Q", 15, ITEM_CONS0 + C_BANDAGE, 1, 0, -1});
            goods.push_back({"BANDAGE X3", "SAVE A FEW COINS", 40, ITEM_CONS0 + C_BANDAGE, 3, 0, -1});
            goods.push_back({"HUNTING BOW", "HOLD TO DRAW - NEEDS ARROWS", BOW_PRICE, ITEM_WPN0 + W_BOW, 1, 1, -1});
            goods.push_back({"CROSSBOW", "HITS HARD - NEEDS BOLTS", CROSSBOW_PRICE, ITEM_WPN0 + W_CROSSBOW, 1, 1, -1});
            goods.push_back({"ARROW X10", "FOR THE HUNTING BOW", AMMO[AM_ARROW].price * AMMO[AM_ARROW].pack, ITEM_AMMO0 + AM_ARROW, AMMO[AM_ARROW].pack, 0, -1});
            goods.push_back({"BOLT X6", "FOR THE CROSSBOW", AMMO[AM_BOLT].price * AMMO[AM_BOLT].pack, ITEM_AMMO0 + AM_BOLT, AMMO[AM_BOLT].pack, 0, -1});
            for (auto& tb : TOOL_BUY) goods.push_back({TOOLDEF[tb.tool].name, "FOR YOUR HOMESTEAD", tb.price, itemTool(tb.tool), 1, 2, tb.tool});
            for (int c = 0; c < CR_COUNT; c++)
                goods.push_back({std::string(CROPS[c].name) + " SEEDS", "RIPE IN " + std::to_string((int)(CROPS[c].stageSec * (CROP_STAGES - 1))) + "S", CROPS[c].seedPrice, itemMat(cropSeed(c)), 1, 0, -1});
            goods.push_back({"HAY", "FEEDS ANY ANIMAL", HAY_PRICE, itemMat(M_HAY), 1, 0, -1});
        }
        const int NG = (int)goods.size(), VIS = 6, ROWH = 25;
        shopScroll = std::max(0, std::min(NG - VIS, shopScroll));
        auto clip = [&](std::string t, int maxW) {
            while (!t.empty() && textW(t) > maxW) t.pop_back();
            return t;
        };
        for (int i = 0; i < VIS && shopScroll + i < NG; i++) {
            const Good& g = goods[shopScroll + i];
            const int y = Y + 22 + i * ROWH;
            panel(x0, y, cw, ROWH - 2, HEX(0x3a2a1a));
            icon(itemIcon(g.item), x0 + 14, y + 11, 18);
            text(x0 + 28, y + 3, clip(g.name, 118), C_GOLD);
            text(x0 + 28, y + 13, clip(g.desc, 118), C_DIM);
            const int owned = g.kind == 2 ? (ownsTool(g.tool) ? 1 : 0) : p.inv.count(g.item);
            const int price = buyPrice(g.price);  // reputation discount
            icon(A.coin, x0 + 152, y + 6, 8);
            text(x0 + 158, y + 3, S(price), p.coins >= price ? C_GOLD : C_RED);
            text(x0 + 148, y + 13, "OWN " + S(owned), C_DIM);
            const bool oneIsEnough = g.kind != 0 && owned >= 1;
            const bool full = oneIsEnough || !p.inv.canAdd(g.item, g.qty);
            const bool ok = p.coins >= price && !full;
            const char* lab = oneIsEnough ? "OWNED" : full ? (p.inv.blockReason(g.item, g.qty) == 2 ? "HEAVY" : "FULL") : "BUY";
            if (button(x0 + 190, y + 3, 46, 16, lab, ok, HEX(0x5d7a2e))) {
                p.coins -= price;
                if (g.kind == 2) giveFarmTool(g.tool);  // a bought tool is equipped straight away
                else p.inv.add(g.item, g.qty);
                audio.play(S_COIN);
                save();
            }
        }
        if (NG > VIS) {  // a thin scroll bar on the right edge of the list, and a hint
            const int bh = VIS * ROWH - 2, th = std::max(8, bh * VIS / NG), ty = Y + 22 + (bh - th) * shopScroll / std::max(1, NG - VIS);
            fillRect(x0 + cw + 3, Y + 22, 2, bh, HEX(0x2b1d12));
            fillRect(x0 + cw + 3, ty, 2, th, C_GOLD);
            text(x0, Y + 22 + VIS * ROWH + 1, S(shopScroll + 1) + "-" + S(std::min(NG, shopScroll + VIS)) + " OF " + S(NG) + "   WHEEL TO SCROLL", C_DIM, 1, false);
        }
    }
    // ---------------- ORDERS: three crafted things the store wants today
    if (shopTab == 2) {
        for (int i = 0; i < 3; i++) {
            const Order& o = orders[i];
            const int y = Y + 22 + i * 42;
            panel(x0, y, cw, 38, HEX(0x3a2a1a));
            if (o.item < 0) {
                textC(x0 + cw / 2, y + 17, "NO ORDER", C_DIM);
                continue;
            }
            qualityFrame(x0 + 5, y + 5, 28, 28, o.q);
            icon(itemIcon(o.item), x0 + 19, y + 19, 22);
            text(x0 + 38, y + 5, item(o.item).name.substr(0, 16) + " X" + S(o.n), C_GOLD);
            text(x0 + 38, y + 14, o.q == Q_NORMAL ? std::string("ANY QUALITY") : std::string("AT LEAST ") + qualityDef(o.q).name, o.q == Q_NORMAL ? C_DIM : qualityCol(o.q));
            int have = 0;
            for (int q = o.q; q < Q_COUNT; q++) have += p.inv.countQ(o.item, q);
            text(x0 + 38, y + 24, "HAVE " + S(have), have >= o.n ? C_GREEN : C_TEXT);
            icon(A.coin, x0 + 92, y + 27, 8);
            text(x0 + 98, y + 24, S(bonusPrice(o.pay)) + " +" + S(o.rep) + " REP", C_GOLD);
            const bool can = orderCanFill(o);
            if (button(x0 + 186, y + 11, 48, 16, o.done ? "DONE" : "GIVE", can, HEX(0x5d7a2e))) fillOrder(i);
        }
        text(x0, Y + 22 + 3 * 42 + 1, "NEW ORDERS EVERY DAY - DAY " + S(lastDay), C_DIM, 1, false);
        text(x0, Y + 22 + 3 * 42 + 11, "FILLED ORDERS AND BIG SALES RAISE YOUR REPUTATION", C_DIM, 1, false);
    }
    textC(X + W / 2, Y + H - 10, "ESC OR E: LEAVE     TAB OR 1-3: SWITCH TAB     " + kgNum(p.inv.totalWeight()) + "/" + kgNum(p.inv.maxWeight) + " KG", C_DIM, 1, false);
}

// ================================================================= inventory screen
// The equipment column of the pack: weapon in hand, worn bag, the seven tools and the mount. Counts show how many of a piece are stacked in the
// pack; clicking the bag takes it off, clicking the mount mounts / dismounts / summons it (the same as its key).
void Game::drawGearColumn(int x0, int y0) {
    struct GearSlot { int id; bool on; const char* lab; };
    GearSlot gs[10];
    int n = 0;
    gs[n++] = {itemWpn(p.sel), true, "WEAPON"};
    gs[n++] = {p.bag >= 0 ? itemPack(p.bag) : -1, p.bag >= 0, "BAG"};
    static const char* TL[TT_COUNT] = {"AXE", "PICKAXE", "HOE", "CAN", "SICKLE", "ROD", "SHOVEL"};
    for (int t = 0; t < TT_COUNT; t++) gs[n++] = {p.tool[t] >= 0 ? itemTool(p.tool[t]) : -1, p.tool[t] >= 0, TL[t]};
    HAnimal* sm = saddledMount();
    const bool riding = riddenMount() != nullptr;
    gs[n++] = {sm ? itemBaby(sm->species) : -1, riding || parked.active, "MOUNT"};
    text(x0, y0 + 1, "GEAR", C_DIM, 1, false);
    int hover = -1;
    for (int i = 0; i < n; i++) {
        const int x = x0 + (i % 2) * 20, y = y0 + 9 + (i / 2) * 18;
        const bool over = inRect(x, y, 19, 17);
        if (over) hover = i;
        panel(x, y, 19, 17, over ? HEX(0x4a3624) : HEX(0x33241a));
        const int id = gs[i].id;
        if (id >= 0) {
            if (itemHasQuality(id) && p.inv.count(id) > 0) qualityFrame(x, y, 19, 17, p.inv.bestQuality(id));
            icon(itemIcon(id), x + 10, y + 8, 12);
            const int have = p.inv.count(id);
            if (have > 1) {
                const std::string c = S(have);
                text(x + 18 - textW(c), y + 10, c, C_TEXT, 1, false);
            }
            if (gs[i].on) fillRect(x + 1, y + 15, 17, 1, riding && i == 9 ? C_GREEN : HEX(0x4a8ad0));
        } else {
            textC(x + 10, y + 6, "-", HEX(0x6a5a44), 1, false);
        }
        if (over && in.uiClick) {
            if (i == 1 && p.bag >= 0) { in.uiClick = false; in.lPressed = false; unequipBag(); }
            else if (i == 9) { in.uiClick = false; in.lPressed = false; toggleMount(); }
        }
    }
    // name of the hovered slot (or the mount's state), wrapped to the column
    const int ty = y0 + 9 + 5 * 18 + 3;
    std::string a, b;
    if (hover >= 0) {
        a = gs[hover].id >= 0 ? item(gs[hover].id).name : std::string(gs[hover].lab);
        if (gs[hover].id < 0) b = "EMPTY";
        else if (hover == 9) b = riding ? "RIDING - CLICK TO GET OFF" : parked.active ? "WAITING - CLICK TO MOUNT" : "CLICK TO CALL IT";
        else if (hover == 1) b = "CLICK TO TAKE OFF";
    } else {
        a = sm ? (riding ? "RIDING" : parked.active ? "MOUNT WAITS" : "MOUNT AT HOME") : "NO MOUNT";
    }
    auto line = [&](const std::string& s, int yy, u32 col) {
        size_t pos = 0;
        while (pos < s.size() && yy < y0 + 9 + 5 * 18 + 40) {
            size_t len = std::min<size_t>(10, s.size() - pos);
            if (pos + len < s.size()) {
                size_t sp = s.rfind(' ', pos + len);
                if (sp != std::string::npos && sp > pos) len = sp - pos;
            }
            text(x0, yy, s.substr(pos, len), col, 1, false);
            yy += 8;
            pos += len;
            while (pos < s.size() && s[pos] == ' ') pos++;
        }
        return yy;
    };
    const int e = line(a, ty, hover >= 0 ? C_GOLD : C_DIM);
    if (!b.empty()) line(b, e, C_DIM);
}

void Game::drawInventory() {
    constexpr int COLS = INV_COLS, CELL = 16;
    SkinScope skin(packSkin);  // the tan and copper look of the reference, for everything drawn in the pack
    fillRect(0, 0, LW, LH, RGBA(24, 18, 36, 215));  // the dark navy behind the pack
    const int X = invTab == 0 ? 4 : 24, Y = 12, W = invTab == 0 ? 392 : 352, H = invTab == 0 ? 212 : 200;  // the backpack page is taller: the category filters sit in a second row
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, HEX(0xe0a878));
    fillRect(X + 3, Y + 16, W - 6, 1, HEX(0x4a2a22));
    text(X + 8, Y + 7, "PACK", C_GOLD);
    const int cap = p.bagCap(), used = p.bagUsed();
    textC(X + W / 2, Y + 7, "I / ESC CLOSE  TAB NEXT PAGE", C_DIM, 1, false);
    {
        std::string cs = S(p.coins);
        text(X + W - 10 - textW(cs), Y + 7, cs, C_GOLD);
        icon(A.coin, X + W - 16 - textW(cs), Y + 10, 8);
    }
    {
        static const char* tabs[5] = {"BACKPACK", "CRAFTING", "SKILLS", "BESTIARY", "JOURNAL"};
        int tx = X + 8;
        for (int i = 0; i < 5; i++) {
            const int w = textW(tabs[i]) + 10;
            if (button(tx, Y + 20, w, 13, tabs[i], true, invTab == i ? HEX(0xffeec8) : HEX(0x4a3a2a))) invTab = i;
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
        int fx = X + 8;
        for (int i = 0; i < 5; i++) {
            int w = textW(fl[i]) + 10;
            if (button(fx, Y + 34, w, 13, fl[i], true, invFilter == i ? HEX(0xffeec8) : HEX(0x4a3a2a))) invFilter = i;
            fx += w + 2;
        }
    }
    if (invTab != 3) trophyOpen = false;
    if (invTab != 2) skillSel = -1;  // the perk choices close when you leave the SKILLS tab
    const int cy = invTab == 0 ? Y + 50 : Y + 38;
    auto iconOf = [&](int id) -> const Spr& { return itemIcon(id); };
    auto sellPriceOf = [&](int id, int q) { return itemHasQuality(id) ? qualityPrice(itemSellBase(id), q) : item(id).price; };

    if (invTab == 0) {
        // ---- the 100-slot grid (10 x 10): always exactly 100 slots, bags never change it
        constexpr int GEARW = 42;  // the equipment column left of the grid
        const int gx = X + 8 + GEARW, gy = cy;
        drawGearColumn(X + 8, cy);
        int hover = -1;
        for (int i = 0; i < cap; i++) {
            int x = gx + (i % COLS) * CELL, y = gy + (i / COLS) * CELL;
            if (inRect(x, y, CELL - 1, CELL - 1)) hover = i;
            if (hover == i && in.uiClick) {
                // shortcuts: double click, or shift + click, uses / equips the stack (like a right click)
                const bool quick = !p.inv.slots[i].empty() && (in.sprint || (invClickSlot == i && timeAcc - invClickT < 0.35f));
                invClickSlot = quick ? -1 : i;
                invClickT = timeAcc;
                invSel = i;
                if (quick) {
                    in.uiClick = false;
                    in.lPressed = false;
                    useSlot(i);
                    continue;
                }
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
            if (hover == i && in.selectKey >= 0 && in.selectKey < 8 && !p.inv.slots[i].empty()) {  // hover a stack and press 1-8: put it on that hotbar slot
                const int hs = in.selectKey;
                in.selectKey = -1;
                if (hotAccepts(HOT_KIND[hs], p.inv.slots[i].item)) equipHot(p.inv.slots[i].item, hs);
                else {
                    msg("THAT HOTBAR SLOT TAKES SOMETHING ELSE", C_RED);
                    audio.play(S_ERROR, 0.4f);
                }
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
            if (sel) {  // a bright green frame, like the reference's selected slot
                fillRect(x, y + 1, 1, CELL - 3, HEX(0x3f9a2a));
                fillRect(x + CELL - 2, y + 1, 1, CELL - 3, HEX(0x3f9a2a));
                fillRect(x + 1, y, CELL - 3, 1, HEX(0x3f9a2a));
                fillRect(x + 1, y + CELL - 2, CELL - 3, 1, HEX(0x3f9a2a));
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
        fillRect(dx - 1, cy + 9, dw + 2, 11, HEX(0x4a2a22));
        fillRect(dx, cy + 10, dw, 9, HEX(0xe0a878));
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
            const bool baby = D.kind == IK_BABY;  // a baby shows its animal rarity tier instead of a quality
            const int bt = std::max(0, std::min(TR_COUNT - 1, sl.q));
            text(dx + 28, py + 14, std::string(ITEM_KIND_NAME[D.kind]) + (hq ? std::string(" - ") + qualityDef(sl.q).name : baby ? std::string(" - ") + TIER[bt].name : std::string()), hq ? qualityCol(sl.q) : baby ? TIER[bt].col : C_DIM, 1, false);
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
                wrap(std::string(T.type == TT_AXE ? "CHOPS" : T.type == TT_HOE ? "TILLS AND DIGS" : T.type == TT_SHOVEL ? "DIGS AND FILLS" : T.type == TT_CAN ? "WATERS CROPS" : T.type == TT_SICKLE ? "CUTS GRAIN" : "MINES") + " TIER " + S(T.tier) + "  BONUS +" + S((int)lroundf(T.bonus * 100)) + "%", C_TEXT);
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
            const int sx = dx + 2, sw = dw - 32;
            const bool many = cd && cur.n > 1;
            sliderBar(sx, by + 5, sw, invAmt, many ? cur.n : 1, invAmtDrag);
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
    } else if (invTab == 1) {
        drawCraftTab(X, Y, W, H);
    } else if (invTab == 2) {
        drawSkillsTab(X, Y, W, H);
    } else if (invTab == 3) {
        drawBestiaryTab(X, Y, W, H);
    } else {
        drawJournalTab(X, Y, W, H);
    }
}
