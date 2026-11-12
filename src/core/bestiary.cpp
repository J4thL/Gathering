// bestiary.cpp - the pack's BESTIARY tab: how many of each species the hunter has killed, and the coin rewards for it.
// A kill is counted when the hunter makes it (killAnimal); a species stays "???" until the first one is hunted.
#include "core/game.hpp"

static constexpr int BEST_STEP[3] = {5, 20, 50};      // kills of one species that earn a reward
static constexpr int BEST_MUL[3] = {1, 3, 8};          // reward = (8 + health / 3) coins x this

static int bestReward(int kind, int step) { return (8 + ANIMAL[kind].hp / 3) * BEST_MUL[step]; }

void Game::bestiaryKill(int kind) {
    if (kind < 0 || kind >= A_COUNT) return;
    const int n = ++bestKills[kind];
    if (n == 1) msg(std::string("BESTIARY: ") + ANIMAL[kind].name + " ADDED", C_GOLD);
    for (int s = 0; s < 3; s++)
        if (n == BEST_STEP[s]) {
            const int c = bestReward(kind, s);
            p.coins += c;
            msg(std::string("BESTIARY: ") + S(n) + " " + ANIMAL[kind].name + " HUNTED  +" + S(c) + " COINS", C_GOLD);
            audio.play(S_CRAFT);
        }
}

void Game::drawBestiaryTab(int X, int Y, int W, int H) {
    const int rowH = 10, colW = (W - 16) / 2, x0 = X + 8, cy = Y + 38, perCol = (A_COUNT + 1) / 2;
    int hover = -1, found = 0;
    for (int k = 0; k < A_COUNT; k++) {
        const int col = k / perCol, row = k % perCol;
        const int x = x0 + col * colW, y = cy + row * rowH;
        const int n = bestKills[k];
        if (n > 0) found++;
        if (inRect(x - 2, y - 1, colW - 4, rowH)) hover = k;
        if (hover == k) fillRect(x - 2, y - 1, colW - 4, rowH, HEX(0x3f3220));
        text(x, y, n > 0 ? std::string(ANIMAL[k].name) : std::string("???"), n > 0 ? C_GOLD : HEX(0x7a6a4a));
        text(x + 82, y, n > 0 ? "x" + S(n) : std::string("-"), n > 0 ? C_TEXT : C_DIM);
        for (int s = 0; s < 3; s++)  // one pip per reward reached
            fillRect(x + 118 + s * 7, y + 1, 5, 5, n >= BEST_STEP[s] ? C_GOLD : HEX(0x4a3a2a));
    }
    std::string foot = "SPECIES FOUND " + S(found) + "/" + S((int)A_COUNT) + "   HUNT 5, 20 AND 50 OF ONE FOR COINS   TAB BACK";
    if (hover >= 0 && bestKills[hover] > 0) {
        const int n = bestKills[hover];
        int next = -1;
        for (int s = 2; s >= 0; s--)
            if (n < BEST_STEP[s]) next = s;
        foot = std::string(ANIMAL[hover].name) + ": " + S(n) + " HUNTED  " + (next >= 0 ? "NEXT REWARD AT " + S(BEST_STEP[next]) + " (+" + S(bestReward(hover, next)) + " COINS)" : std::string("ALL REWARDS CLAIMED"));
    }
    textC(X + W / 2, Y + H - 8, foot, C_DIM, 1, false);
}
