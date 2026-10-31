// services.cpp - what the people in the town buildings do for the hunter.
//
//  Blacksmith, Leatherworks, Workshop: the crafting book (crafting.cpp) filtered to what that building makes, plus the coin upgrades
//  that used to be on the shop's UPGRADE tab (tool skinning at the smith, armor and boots at the tanner). All of it runs on the same
//  recipes, inventory, weight and slot rules as anywhere else: this file only draws the screens and opens / closes them.
//  General Store: the existing SELL / BUY screen (drawShop in game_render.cpp), trading only.
//  Flower Shop and Salon: see flowers.cpp and salon.cpp.
#include <algorithm>
#include "game.hpp"

const char* npcName(int k);  // town.cpp

void Game::openService(int k) {
    if (k == BLD_SHOP) {
        state = ST_SHOP;
        shopTab = 0;
        audio.play(S_CLICK);
        return;
    }
    svc = k;
    svcTab = 0;
    craftCat = -1;
    craftScroll = 0;
    craftOnly = false;
    if (k == BLD_SALON) openSalon();
    craftBld = (k == BLD_SMITH || k == BLD_TANNER || k == BLD_WORKSHOP) ? k : -1;
    state = ST_SERVICE;
    audio.play(S_CLICK);
}

void Game::closeService() {
    state = ST_PLAY;
    svc = -1;
    craftBld = -1;
    save();
    audio.play(S_CLICK);
}

void Game::updateService(float dt) {
    updateFx(dt);
    if (in.escPressed || in.ePressed) {
        closeService();
        return;
    }
    if (svc == BLD_FLOWER && (in.tabPressed || (in.selectKey >= 0 && in.selectKey < 2))) svcTab = in.tabPressed ? 1 - svcTab : in.selectKey;
    if (craftBld >= 0) {
        craftScroll -= in.wheel;
        const bool hasUp = svc == BLD_SMITH || svc == BLD_TANNER;
        if (hasUp && (in.tabPressed || (in.selectKey >= 0 && in.selectKey < 2))) svcTab = in.tabPressed ? 1 - svcTab : in.selectKey;
    }
}

// the coin upgrades (they used to be the shop's UPGRADE tab): a list of gear slots U_*
void Game::drawUpgradeRows(int x0, int Y, int cw, const int* list, int n) {
    for (int row = 0; row < n; row++) {
        const int u = list[row];
        const int y = Y + 4 + row * 44;
        const int lv = p.up[u];
        panel(x0, y, cw, 40, HEX(0x3a2a1a));
        icon(A.gearIcon[u][std::min(5, lv)], x0 + 20, y + 20, 28, lv >= 2 ? 0.8f : 1.f);
        text(x0 + 40, y + 4, UPS[u].title, C_DIM);
        for (int i = 0; i < 2; i++) fillRect(x0 + cw - 20 + i * 8, y + 4, 6, 5, i < lv ? C_GOLD : HEX(0x1a120b));
        if (lv >= 2) {
            text(x0 + 40, y + 14, gearName(u, lv), C_GOLD);
            text(x0 + 40, y + 23, gearDesc(u, lv), C_TEXT);
            text(x0 + 40, y + 32, lv > 2 ? "CRAFTED GEAR" : "FULLY UPGRADED", C_GREEN);
            continue;
        }
        const UpLevel& L = UPS[u].lv[lv];
        text(x0 + 40, y + 14, L.name, C_GOLD);
        text(x0 + 40, y + 23, L.desc, C_TEXT);
        int cx = x0 + 40;
        cx = chip(cx, y + 28, A.coin, p.coins, L.coins);
        if (L.m1 >= 0) cx = chip(cx, y + 28, A.matIcon[L.m1], p.mat(L.m1), L.n1);
        if (L.m2 >= 0) cx = chip(cx, y + 28, A.matIcon[L.m2], p.mat(L.m2), L.n2);
        if (button(x0 + cw - 52, y + 21, 48, 16, "BUY", canAfford(L.coins, L.m1, L.n1, L.m2, L.n2), HEX(0x5d7a2e))) buyUpgrade(u);
    }
}

void Game::drawCraftService() {
    const int X = 8, Y = 12, W = 384, H = 201;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, BUILDINGS[svc].name, C_GOLD);
    {
        std::string c = S(p.coins);
        int cw = textW(c);
        icon(A.coin, X + W - 10 - cw - 8, Y + 10, 10);
        text(X + W - 8 - cw, Y + 7, c, C_GOLD);
    }
    // the person on the left
    panel(X + 6, Y + 112, 58, 72, HEX(0x2b1d12));
    {
        const Spr& k = A.T.npc[svc][((int)(timeAcc * 1.6f)) & 1];
        SDL_Rect d{X + 6 + 29 - k.w, Y + 114, k.w * 2, k.h * 2};
        SDL_RenderCopy(ren, k.t, nullptr, &d);
        textC(X + 35, Y + 168, npcName(svc), C_DIM, 1, false);
    }
    const bool hasUp = svc == BLD_SMITH || svc == BLD_TANNER;
    if (hasUp) {
        const char* tabs[2] = {"CRAFT", "UPGRADE"};
        for (int i = 0; i < 2; i++)
            if (button(X + 6, Y + 22 + i * 22, 58, 19, tabs[i], true, svcTab == i ? HEX(0x92693a) : HEX(0x5a4028))) svcTab = i;
    } else {
        text(X + 8, Y + 24, "TODAY'S", C_DIM, 1, false);
        text(X + 8, Y + 33, "WORK", C_DIM, 1, false);
    }
    if (!hasUp || svcTab == 0) {
        drawCraftTab(X + 68, Y - 16, W - 68, H + 16);
    } else {
        static const int smithUp[] = {U_TOOL}, tanUp[] = {U_ARMOR, U_BOOTS};
        if (svc == BLD_SMITH) drawUpgradeRows(X + 74, Y + 20, W - 84, smithUp, 1);
        else drawUpgradeRows(X + 74, Y + 20, W - 84, tanUp, 2);
        textC(X + 68 + (W - 68) / 2, Y + H - 12, "ARMOR AND BOOTS ALSO HAVE MATERIAL RECIPES ON THE CRAFT TAB", C_DIM, 1, false);
    }
    textC(X + W / 2, Y + H - 4, "ESC OR E: LEAVE     TAB: SWITCH     " + kgNum(p.inv.totalWeight()) + "/" + kgNum(p.inv.maxWeight) + " KG", C_DIM, 1, false);
}

void Game::drawService() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 140));
    if (svc == BLD_SMITH || svc == BLD_TANNER || svc == BLD_WORKSHOP) drawCraftService();
    else if (svc == BLD_FLOWER) drawFlowerShop();
    else if (svc == BLD_SALON) drawSalon();
}
