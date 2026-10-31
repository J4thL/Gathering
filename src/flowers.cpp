// flowers.cpp - Mary Rose's flower shop. Cut flowers are bought here (ordinary materials in the ordinary pack: slots, weight and
// stack limits apply) and tied into bouquets at the workshop; bouquets and flowers can be sold back here or at the general store.
#include <algorithm>
#include "game.hpp"

namespace {
constexpr int FLOWERS[4] = {M_ROSE, M_TULIP, M_LAVENDER, M_SUNFLOWER};
constexpr int GOODS[5] = {M_ROSE, M_TULIP, M_LAVENDER, M_SUNFLOWER, M_BOUQUET};  // what she will buy back
inline int flowerBuyPrice(int m) { return MAT_PRICE[m] * 2; }
}  // namespace

// buy n cut flowers (all or nothing): needs the coins and room in the pack (slots and weight)
bool Game::buyFlower(int m, int n) {
    if (m < M_ROSE || m > M_SUNFLOWER || n <= 0) return false;
    const int cost = flowerBuyPrice(m) * n;
    if (p.coins < cost) {
        msg("NOT ENOUGH COINS", C_RED);
        audio.play(S_ERROR);
        return false;
    }
    if (!p.inv.canAdd(itemMat(m), n)) {
        notifyBlocked(itemMat(m), n);
        audio.play(S_ERROR);
        return false;
    }
    p.coins -= cost;
    p.inv.add(itemMat(m), n);
    audio.play(S_COIN);
    return true;
}

void Game::drawFlowerShop() {
    const int X = 36, Y = 12, W = 328, H = 201;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, "FLOWER SHOP", C_GOLD);
    {
        std::string c = S(p.coins);
        const int cw = textW(c);
        icon(A.coin, X + W - 10 - cw - 8, Y + 10, 10);
        text(X + W - 8 - cw, Y + 7, c, C_GOLD);
    }
    const char* tabs[2] = {"BUY", "SELL"};
    for (int i = 0; i < 2; i++) {
        const int ty = Y + 22 + i * 22;
        if (button(X + 6, ty, 66, 19, tabs[i], true, svcTab == i ? HEX(0x92693a) : HEX(0x5a4028))) svcTab = i;
    }
    panel(X + 6, Y + 112, 66, 72, HEX(0x2b1d12));
    {
        const Spr& k = A.T.npc[BLD_FLOWER][((int)(timeAcc * 1.6f)) & 1];
        SDL_Rect d{X + 6 + 33 - k.w, Y + 114, k.w * 2, k.h * 2};
        SDL_RenderCopy(ren, k.t, nullptr, &d);
        textC(X + 39, Y + 168, "MARY ROSE", C_DIM, 1, false);
        const char* lines[] = {"FRESH TODAY!", "LOVELY, ISN'T IT?", "TRY A BOUQUET!", "SAY IT WITH FLOWERS"};
        textC(X + 39, Y + 176, lines[((int)(titleT + timeAcc * 0.15f)) % 4], C_TEXT, 1, false);
    }
    const int x0 = X + 80, cw = 240;
    if (svcTab == 0) {
        text(x0, Y + 22, "CUT FLOWERS - TIE THEM INTO BOUQUETS AT THE WORKSHOP", C_DIM, 1, false);
        for (int i = 0; i < 4; i++) {
            const int m = FLOWERS[i], price = flowerBuyPrice(m), y = Y + 32 + i * 36;
            panel(x0, y, cw, 33, HEX(0x3a2a1a));
            icon(A.matIcon[m], x0 + 16, y + 16, 24);
            text(x0 + 34, y + 5, MAT_NAME[m], C_GOLD);
            static const char* const blurb[4] = {"CLASSIC RED ROSE", "BRIGHT SPRING TULIP", "SWEET-SCENTED SPRIGS", "A BIG SUNNY BLOOM"};
            text(x0 + 34, y + 14, blurb[i], C_DIM, 1, false);
            text(x0 + 34, y + 23, "OWNED " + S(p.mat(m)), C_TEXT, 1, false);
            chip(x0 + 90, y + 20, A.coin, p.coins, price);
            const bool fit = p.inv.canAdd(itemMat(m), 1);
            const char* lab = fit ? "BUY" : (p.inv.blockReason(itemMat(m), 1) == 2 ? "HEAVY" : "FULL");
            if (button(x0 + 144, y + 15, 44, 15, lab, p.coins >= price && fit, HEX(0x5d7a2e))) buyFlower(m, 1);
            const int n5 = std::min(5, p.inv.room(itemMat(m)));
            if (button(x0 + 192, y + 15, 44, 15, "BUY 5", n5 >= 5 && p.coins >= price * 5, HEX(0x5d7a2e))) buyFlower(m, 5);
        }
    } else {
        text(x0, Y + 22, "SHE BUYS FLOWERS AND BOUQUETS", C_DIM, 1, false);
        int row = 0;
        for (int m : GOODS) {
            if (p.mat(m) <= 0) continue;
            const int y = Y + 32 + row++ * 22;
            panel(x0, y, cw, 20, HEX(0x3a2a1a));
            icon(A.matIcon[m], x0 + 12, y + 10, 14);
            text(x0 + 24, y + 6, MAT_NAME[m], C_TEXT);
            text(x0 + 92, y + 6, "X" + S(p.mat(m)), C_TEXT);
            icon(A.coin, x0 + 122, y + 10, 8);
            text(x0 + 128, y + 6, S(MAT_PRICE[m]), C_GOLD);
            if (button(x0 + 146, y + 3, 44, 14, "SELL 1", true)) sellMat(m, 1);
            if (button(x0 + 194, y + 3, 44, 14, "SELL ALL", true)) sellMat(m, p.mat(m));
        }
        if (row == 0) textC(x0 + cw / 2, Y + 80, "NO FLOWERS OR BOUQUETS TO SELL", C_DIM);
    }
    textC(X + W / 2, Y + H - 10, "ESC OR E: LEAVE     TAB: SWITCH     " + kgNum(p.inv.totalWeight()) + "/" + kgNum(p.inv.maxWeight) + " KG", C_DIM, 1, false);
}
