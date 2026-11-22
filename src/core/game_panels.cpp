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
            for (int k = 0; k < DA_COUNT; k++)  // baby farm animals: carried home in the pack, set down at the homestead
                goods.push_back({std::string("BABY ") + DOMS[k].name, std::string(DOMS[k].prodName ? DOMS[k].prodName : "") + (DOMS[k].small ? " - COOP OK" : " - BARN OR PEN"), DOMS[k].price, itemFarmBaby(k), 1, 0, -1});
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
// The paper doll on the right page of the pack: the worn carried gear around a silhouette (head, body, cloak, belt, feet / bag, back, two
// charms), then the weapon in hand, the seven tools and the mount. Clicking a worn piece takes it off; a stack dragged from the grid
// onto a matching slot wears it. What is under the cursor is left in `dh` for the details card.
static bool dollAccepts(int what, int idx, int id) {
    const ItemDef& D = item(id);
    switch (what) {
        case 0: return D.kind == IK_CARRIED && (CG_SLOT[D.ref] == idx || (CG_SLOT[D.ref] == EQ_ACC1 && idx == EQ_ACC2));
        case 1: return D.kind == IK_BACKPACK;
        case 2: return D.kind == IK_TOOL && TOOLDEF[D.ref].type == idx;
        case 3: return D.kind == IK_THROWN || D.kind == IK_LAUNCHER;
        default: return false;
    }
}

void Game::dollTakeOff() {
    switch (dh.what) {
        case 0: unequipCarried(dh.idx); break;
        case 1: unequipBag(); break;
        case 2: {
            if (dh.idx == TT_AXE || dh.idx == TT_PICK) unequipGatherTool(dh.idx);
            else if (isFarmType(dh.idx)) unequipFarmTool(dh.idx);
            else if (p.hotSlotOf(dh.id) >= 0) unequipHot(p.hotSlotOf(dh.id));
            break;
        }
        case 3:
            if (p.hotSlotOf(dh.id) >= 0) unequipHot(p.hotSlotOf(dh.id));
            break;
        default: break;
    }
}

void Game::drawPaperDoll(int x, int y, int w, bool dropNow, int dragSlot) {
    struct DS { int what, idx, x, y, w, h; int id, q; bool on; const char* lab; };
    DS ds[24];
    int n = 0;
    const int cw = 22, chh = 16, gap = 17;
    const int xl = x, xr = x + w - cw;
    static const int LEFT[5] = {EQ_HEAD, EQ_BODY, EQ_CLOAK, EQ_BELT, EQ_FEET};
    static const int RIGHT[3] = {EQ_BACK, EQ_ACC1, EQ_ACC2};
    for (int i = 0; i < 5; i++) {
        const int e = LEFT[i];
        ds[n++] = {0, e, xl, y + i * gap, cw, chh, p.worn[e], p.wornQ[e], p.worn[e] >= 0, EQ_NAME[e]};
    }
    ds[n++] = {1, 0, xr, y, cw, chh, p.bag >= 0 ? itemPack(p.bag) : -1, 0, p.bag >= 0, "BAG"};
    for (int i = 0; i < 3; i++) {
        const int e = RIGHT[i];
        ds[n++] = {0, e, xr, y + (i + 1) * gap, cw, chh, p.worn[e], p.wornQ[e], p.worn[e] >= 0, EQ_NAME[e]};
    }
    // the weapon, the seven tools and the mount in one row under the doll
    static const char* TL[TT_COUNT] = {"AX", "PK", "HO", "CN", "SK", "RD", "SV"};
    const int ry = y + 5 * gap + 2, rw = w / 9;
    ds[n++] = {3, 0, x, ry, rw - 1, 19, itemWpn(p.sel), 0, true, "WP"};
    for (int t = 0; t < TT_COUNT; t++) ds[n++] = {2, t, x + (t + 1) * rw, ry, rw - 1, 19, p.tool[t] >= 0 ? itemTool(p.tool[t]) : -1, 0, p.tool[t] >= 0, TL[t]};
    HAnimal* sm = saddledMount();
    const bool riding = riddenMount() != nullptr;
    ds[n++] = {4, 0, x + 8 * rw, ry, rw - 1, 19, sm ? itemBaby(sm->species) : -1, 0, riding || parked.active, "MT"};
    // the silhouette between the two columns
    {
        const int cx = x + w / 2, top = y + 1;
        const u32 sil = HEX(0xcbc6ba), sil2 = HEX(0xc1bcaf);
        fillRect(cx - 5, top, 10, 13, sil); fillRect(cx - 6, top + 2, 12, 9, sil);
        fillRect(cx - 2, top + 13, 4, 4, sil2);
        fillRect(cx - 12, top + 17, 24, 3, sil); fillRect(cx - 10, top + 20, 20, 24, sil);
        fillRect(cx - 16, top + 19, 5, 26, sil2); fillRect(cx + 11, top + 19, 5, 26, sil2);
        fillRect(cx - 9, top + 44, 8, 33, sil); fillRect(cx + 1, top + 44, 8, 33, sil);
    }
    dh = DollHover();
    for (int i = 0; i < n; i++) {
        const DS& d = ds[i];
        const bool over = inRect(d.x, d.y, d.w, d.h);
        const bool accepts = dragSlot >= 0 && !p.inv.slots[dragSlot].empty() && dollAccepts(d.what, d.idx, p.inv.slots[dragSlot].item);
        panel(d.x, d.y, d.w, d.h, accepts ? HEX(0x4e6a3a) : over ? HEX(0x4a3624) : HEX(0x33241a));
        if (d.id >= 0) {
            if (itemHasQuality(d.id) && d.what == 0) qualityFrame(d.x, d.y, d.w, d.h, d.q);
            icon(itemIcon(d.id), d.x + d.w / 2, d.y + d.h / 2, d.w >= 20 ? 13 : 11);
            if (d.what == 4) fillRect(d.x + 1, d.y + d.h - 2, d.w - 2, 1, riding ? C_GREEN : HEX(0x4a8ad0));
        } else {
            textC(d.x + d.w / 2, d.y + d.h / 2 - 3, d.lab, HEX(0x8a8476), 1, false);
        }
        if (over) {
            dh.what = d.what;
            dh.idx = d.idx;
            dh.id = d.id;
            dh.q = d.q;
            dh.lab = d.lab;
            if (in.uiClick) {
                in.uiClick = false;
                in.lPressed = false;
                if (d.what == 4) toggleMount();
                else if (d.id >= 0) dollTakeOff();
                else {
                    msg(std::string("NOTHING WORN ON THE ") + d.lab, C_DIM);
                    audio.play(S_ERROR, 0.4f);
                }
            }
            if (dropNow && accepts) {  // a stack dragged here from the grid is worn / equipped
                if (d.what == 0) equipCarried(dragSlot, d.idx);
                else useSlot(dragSlot);
            }
        }
    }
}

void Game::drawInventory() {
    constexpr int COLS = INV_COLS, CELL = 14;
    SkinScope skin(packSkin);  // the grey paper and ink look of the book, for everything drawn in the pack
    fillRect(0, 0, LW, LH, RGBA(24, 18, 36, 215));  // the dark navy behind the pack
    const int X = 4, Y = 12, W = 392, H = 212;  // one size for every tab of the book
    // ---- the book: a leather cover, two pages and the spine
    {
        const u32 cover = HEX(0x4a3a30), coverHi = HEX(0x6a5646), page = HEX(0xd9d5ca), pageEdge = HEX(0xb4afa2);
        fillRect(X + 1, Y, W - 2, H, cover);
        fillRect(X, Y + 1, W, H - 2, cover);
        fillRect(X + 1, Y + 1, W - 2, 1, coverHi);
        const int pw = (W - 12) / 2;
        const int lx = X + 5, rx = X + W - 5 - pw, py0 = Y + 4, ph0 = H - 8;
        if (invTab == 0) {
            fillRect(lx - 1, py0 - 1, pw + 2, ph0 + 2, pageEdge);
            fillRect(rx - 1, py0 - 1, pw + 2, ph0 + 2, pageEdge);
            fillRect(lx, py0, pw, ph0, page);
            fillRect(rx, py0, pw, ph0, page);
            for (int i = 0; i < 6; i++) {  // the spine: the pages darken toward the fold
                const u32 sh = HEX((u32)(0xd9 - i * 9) << 16 | (u32)(0xd5 - i * 9) << 8 | (u32)(0xca - i * 9));
                fillRect(lx + pw - 1 - i, py0, 1, ph0, sh);
                fillRect(rx + i, py0, 1, ph0, sh);
            }
        } else {
            fillRect(X + 4, Y + 3, W - 8, H - 6, pageEdge);
            fillRect(X + 5, Y + 4, W - 10, H - 8, page);
        }
    }
    const int cap = p.bagCap(), used = p.bagUsed();
    // ---- the main tabs along the top (icons); the coins sit at the right
    {
        static const int TI[5] = {TI_PACK, TI_CRAFT, TI_SKILLS, TI_BEASTS, TI_JOURNAL};
        static const char* NM[5] = {"BACKPACK", "CRAFTING", "SKILLS", "BESTIARY", "JOURNAL"};
        text(X + 6, 4, "TAB", C_DIM, 1, false);
        int tx = X + 24;
        for (int i = 0; i < 5; i++) {
            const bool on = invTab == i, over = inRect(tx, 0, 24, 14);
            panel(tx, on ? 0 : 1, 24, on ? 15 : 13, on ? HEX(0xf6f3ea) : over ? HEX(0xe4e0d6) : HEX(0xb4afa2));
            icon(A.tabIcon[TI[i]], tx + 12, on ? 7 : 7, 11, on ? 1.f : 0.75f);
            if (over && in.uiClick) {
                in.uiClick = false;
                in.lPressed = false;
                invTab = i;
                audio.play(S_CLICK, 0.5f);
            }
            if (over) text(X + 24 + 5 * 26 + 4, 4, NM[i], C_GOLD, 1, false);
            tx += 26;
        }
        std::string cs = S(p.coins);
        text(X + W - 10 - textW(cs), 4, cs, C_GOLD);
        icon(A.coin, X + W - 16 - textW(cs), 7, 8);
        text(X + W - 70 - textW(cs), 4, "ESC CLOSE", C_DIM, 1, false);
    }
    if (invTab != 0) {  // the other pages of the book: a heading over the page (the pack's two pages need none)
        static const char* HEAD[5] = {"BACKPACK", "CRAFTING", "SKILLS", "BESTIARY", "JOURNAL"};
        text(X + 14, Y + 10, HEAD[invTab], C_GOLD);
        fillRect(X + 12, Y + 21, W - 24, 1, HEX(0x6b665c));
    }
    // category filter for the grid: only the items of the chosen category are listed (the stacks keep their slots)
    auto filterOf = [](int id) {
        switch (item(id).kind) {
            case IK_MATERIAL: return 1;
            case IK_THROWN: case IK_LAUNCHER: case IK_AMMO: return 2;
            case IK_TOOL: return 3;
            case IK_BACKPACK: case IK_CARRIED: return 4;
            default: return 5;
        }
    };
    if (invTab != 3) trophyOpen = false;
    if (invTab != 2) skillSel = -1;  // the perk choices close when you leave the SKILLS tab
    const int cy = invTab == 0 ? Y + 10 : Y + 24;
    auto iconOf = [&](int id) -> const Spr& { return itemIcon(id); };
    auto sellPriceOf = [&](int id, int q) { return itemHasQuality(id) ? qualityPrice(itemSellBase(id), q) : item(id).price; };

    if (invTab == 0) {
        // ---- left page: the category tabs down the side, the 100-slot grid (10 x 10: bags never change it) and the weight
        static const int SUBI[6] = {TI_ALL, TI_MATS, TI_WEAPONS, TI_TOOLS, TI_GEAR, TI_SUPPLIES};
        static const char* SUBN[6] = {"ALL ITEMS", "MATERIALS", "WEAPONS", "TOOLS", "GEAR", "SUPPLIES"};
        const int gx = X + 31, gy = cy + 12, ax = gx, aw = 150;
        for (int i = 0; i < 6; i++) {
            const bool on = invFilter == i;
            const int sx = X + 6 + (on ? 0 : -2), sy = gy + i * 22, sw = on ? 24 : 20;
            const bool over = inRect(X + 4, sy, 24, 20);
            panel(sx, sy, sw, 20, on ? HEX(0xf6f3ea) : over ? HEX(0xe4e0d6) : HEX(0xb4afa2));
            icon(A.tabIcon[SUBI[i]], sx + 10, sy + 10, 12, on ? 1.f : 0.75f);
            if (over && in.uiClick) {
                in.uiClick = false;
                in.lPressed = false;
                invFilter = i;
                audio.play(S_CLICK, 0.5f);
            }
        }
        text(gx, cy + 3, SUBN[invFilter], C_GOLD, 1, false);
        {
            const std::string us = "SLOTS " + S(used) + "/" + S(cap);
            text(gx + aw - textW(us), cy + 3, us, used >= cap ? C_RED : C_DIM, 1, false);
        }
        // the cells shown: every slot (ALL) or just the stacks of the chosen category, packed together
        std::vector<int> view(cap, -1);
        {
            int vn = 0;
            for (int i = 0; i < cap; i++)
                if (invFilter == 0) view[vn++] = i;
                else if (!p.inv.slots[i].empty() && filterOf(p.inv.slots[i].item) == invFilter) view[vn++] = i;
        }
        int hover = -1;
        for (int vi = 0; vi < cap; vi++) {
            const int i = view[vi];
            if (i < 0) continue;
            int x = gx + (vi % COLS) * CELL, y = gy + (vi / COLS) * CELL;
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
        // a drag released over another grid slot moves / merges / swaps; over a hotbar or doll slot it equips (handled with those slots)
        const bool dropNow = invDrag >= 0 && invDragMoved && !in.l;
        if (dropNow && hover >= 0 && hover != invDrag && invFilter == 0) {
            if (p.inv.moveSlot(invDrag, hover, p.inv.slots[invDrag].n) > 0) {
                invSel = hover;
                audio.play(S_CLICK, 0.5f);
            }
        }
        for (int vi = 0; vi < cap; vi++) {
            const int i = view[vi];
            int x = gx + (vi % COLS) * CELL, y = gy + (vi / COLS) * CELL;
            const bool sel = i >= 0 && i == invSel;
            panel(x, y, CELL - 1, CELL - 1, sel ? HEX(0x6a4a2a) : hover == i && i >= 0 ? HEX(0x4a3624) : HEX(0x33241a));
            if (i < 0) continue;
            const Slot& sl = p.inv.slots[i];
            if (sel) {  // a bright green frame, like the reference's selected slot
                fillRect(x, y + 1, 1, CELL - 3, HEX(0x3f9a2a));
                fillRect(x + CELL - 2, y + 1, 1, CELL - 3, HEX(0x3f9a2a));
                fillRect(x + 1, y, CELL - 3, 1, HEX(0x3f9a2a));
                fillRect(x + 1, y + CELL - 2, CELL - 3, 1, HEX(0x3f9a2a));
            }
            if (!sl.empty()) {
                if (itemHasQuality(sl.item) && !sel) qualityFrame(x, y, CELL - 1, CELL - 1, sl.q);
                icon(iconOf(sl.item), x + (CELL - 1) / 2, y + (CELL - 1) / 2 - 1, 11);
                if (sl.n > 1) {
                    std::string n = S(sl.n);
                    text(x + CELL - 1 - textW(n), y + CELL - 7, n, C_TEXT);
                }
                if (p.hotSlotOf(sl.item) >= 0) fillRect(x + 2, y + 2, 2, 2, C_GREEN);  // equipped on the hotbar
            }
        }
        // ---- carrying weight (under the grid)
        const float tw = p.inv.totalWeight(), mw = p.inv.maxWeight;
        const float frac = std::max(0.f, std::min(1.f, mw > 0 ? tw / mw : 1.f));
        const u32 wcol = tw > mw + 1e-3f ? C_RED : frac >= 0.85f ? C_ORANGE : frac >= 0.6f ? C_GOLD : C_GREEN;
        const int wy = gy + 10 * CELL + 3;
        text(ax, wy, "CARRYING", C_DIM, 1, false);
        const std::string ws = kgNum(tw) + " / " + kgNum(mw) + " KG";
        text(ax + aw - textW(ws), wy, ws, wcol, 1, false);
        fillRect(ax - 1, wy + 8, aw + 2, 9, HEX(0x6b665c));
        fillRect(ax, wy + 9, aw, 7, HEX(0xd9d5ca));
        fillRect(ax + 1, wy + 10, (int)((aw - 2) * frac), 5, wcol);
        // ---- right page: the paper doll, then the details of the hovered (or selected) slot
        const int dx = X + W / 2 + 8, dw = X + W - 9 - dx;
        drawPaperDoll(dx, Y + 10, dw, dropNow, invDrag);
        // ---- details of the hovered (or selected) slot
        const int py = 130, ph = 62;
        panel(dx, py, dw, ph, HEX(0x3a2a1a));
        const bool dollShow = dh.what >= 0 && dh.id >= 0;  // a worn piece under the cursor is shown instead of a pack slot
        const int show = std::max(0, std::min(cap - 1, hover >= 0 ? hover : invSel));
        Slot dsl;
        if (dollShow) dsl.item = dh.id, dsl.n = 1, dsl.q = dh.q;
        const Slot& sl = dollShow ? dsl : p.inv.slots[show];
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
            if (dollShow) {
                if (button(dx + dw - 52, py + 4, 48, 13, "TAKE OFF", true, HEX(0x4a3a5e))) dollTakeOff();
            } else if (canAct && show == invSel) {
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
        int by = 196;
        // the amount slider: how many a DROP or SPLIT takes from the selected stack
        if (invAmtSlot != invSel) {
            invAmtSlot = invSel;
            invAmt = 1;
        }
        invAmt = std::max(1, std::min(std::max(1, cur.n), invAmt));
        {
            const int sx = ax + 2, sw = aw - 36;
            const bool many = cd && cur.n > 1;
            sliderBar(sx, by + 5, sw, invAmt, many ? cur.n : 1, invAmtDrag);
            if (many && inRect(sx, by, aw, 13)) invAmt = std::max(1, std::min(cur.n, invAmt - in.wheel));
            textC(ax + aw - 16, by + 3, cd ? S(invAmt) + "/" + S(cur.n) : "--", cd ? C_GOLD : C_DIM, 1, false);
        }
        by += 13;
        if (button(ax, by, 33, 13, "SPLIT", cd != nullptr && cur.n > 1, HEX(0x4a5a7a))) {  // the chosen amount goes into the first free slot
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
        if (button(ax + 36, by, 33, 13, "DROP", cd != nullptr, HEX(0x7a4a2e))) dropSlot(invSel, invAmt);
        if (button(ax + 72, by, 33, 13, "SORT", true, HEX(0x4a3a5e))) {
            p.inv.compact();
            msg("PACK SORTED", C_DIM);
        }
        // ---- the hotbar: click a slot to equip the selected item there, or to unequip what is in it
        by = 198;
        for (int i = 0; i < HOT_N; i++) {
            const int hx = dx + 2 + i * 22, hy = by;
            const int id = p.hot[i];
            const bool accepts = cd && hotAccepts(HOT_KIND[i], cur.item);
            const bool over = inRect(hx, hy, 20, 20);
            panel(hx, hy, 20, 20, accepts ? HEX(0x4e6a3a) : over ? HEX(0x4a3624) : HEX(0x33241a));
            if (id >= 0) {
                if (itemHasQuality(id) && p.inv.count(id) > 0) qualityFrame(hx, hy, 20, 20, p.inv.bestQuality(id));
                icon(itemIcon(id), hx + 10, hy + 11, 12, p.inv.count(id) > 0 ? 1.f : 0.35f);
                if (item(id).kind == IK_THROWN || item(id).kind == IK_LAUNCHER) {
                    if (item(id).ref == p.sel) fillRect(hx + 1, hy + 18, 18, 1, C_GREEN);
                }
                else if (item(id).kind == IK_TOOL && p.tool[TOOLDEF[item(id).ref].type] == item(id).ref) fillRect(hx + 1, hy + 18, 18, 1, C_GREEN);
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
        drawTree(X, Y - 14, W, H + 7, true);
    } else if (invTab == 2) {
        drawTree(X, Y - 14, W, H + 7, false);
    } else if (invTab == 3) {
        drawBestiaryTab(X, Y - 14, W, H + 7);
    } else {
        drawJournalTab(X, Y - 14, W, H + 7);
    }
}
