// services.cpp - what the people in the town buildings do for the hunter.
//
//  Blacksmith, Leatherworks, Workshop: the crafting book (crafting.cpp) filtered to what that building makes (armor and boots are
//  ordinary crafted items now: no coin upgrades). It runs on the same recipes, inventory, weight and slot rules as anywhere else: this file only draws the screens and opens / closes them.
//  General Store: the existing SELL / BUY screen (drawShop in game_render.cpp), trading only.
//  Flower Shop and Salon: see flowers.cpp and salon.cpp.
#include <algorithm>
#include "core/game.hpp"

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
    if (craftBld >= 0) craftScroll -= in.wheel;
}

void Game::drawCraftService() {
    SkinScope skin(packSkin);  // the same tan and copper look as the pack
    const int X = 8, Y = 12, W = 384, H = 201;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, skinFill(UI_BG));
    fillRect(X + 3, Y + 16, W - 6, 1, HEX(0x6b665c));
    text(X + 8, Y + 7, std::string("CRAFT - ") + BUILDINGS[svc].name, C_GOLD);
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
    text(X + 8, Y + 24, "TODAY'S", C_DIM, 1, false);
    text(X + 8, Y + 33, "WORK", C_DIM, 1, false);
    drawStationCraft(X + 68, Y, W - 68, H);
    textC(X + W / 2, Y + H - 4, "ESC OR E: LEAVE     " + kgNum(p.inv.totalWeight()) + "/" + kgNum(p.inv.maxWeight) + " KG", C_DIM, 1, false);
}

void Game::drawService() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 140));
    if (svc == BLD_SMITH || svc == BLD_TANNER || svc == BLD_WORKSHOP) drawCraftService();
    else if (svc == BLD_FLOWER) drawFlowerShop();
    else if (svc == BLD_SALON) drawSalon();
}
