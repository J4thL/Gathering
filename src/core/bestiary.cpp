// bestiary.cpp - the pack's BESTIARY tab: how many of each species the hunter has killed, and the coin rewards for it.
// A kill is counted when the hunter makes it (killAnimal); a species stays "???" until the first one is hunted.
#include "core/game.hpp"

static constexpr int BEST_STEP[3] = {5, 20, 50};      // kills of one species that earn a reward
static constexpr int BEST_MUL[3] = {1, 3, 8};          // reward = (8 + health / 3) coins x this

static int bestReward(int kind, int step) { return (8 + ANIMAL[kind].hp / 3) * BEST_MUL[step]; }

void Game::bestiaryKill(int kind, int tier) {
    if (kind < 0 || kind >= A_COUNT) return;
    const int n = ++bestKills[kind];
    bestMask[kind] |= 1 << std::max(0, std::min(TR_COUNT - 1, tier));
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
    // the species under the cursor gets a card over the other column: health, temper, tiers killed and what it drops
    if (hover >= 0 && bestKills[hover] > 0) {
        const AnimalDef& D = ANIMAL[hover];
        const int cardW = colW - 6, cardH = 112;
        const int cx = (hover / perCol == 0 ? x0 + colW : x0) + 1, cyy = cy;
        panel(cx, cyy, cardW, cardH, HEX(0x2b1d12));
        text(cx + 5, cyy + 5, D.name, C_GOLD);
        text(cx + 5, cyy + 15, "HEALTH " + S(D.hp) + (D.aggro ? "  FIGHTS BACK" : "  FLEES"), D.aggro ? HEX(0xff9a7a) : C_GREEN, 1, false);
        text(cx + 5, cyy + 25, "KILLED", C_DIM, 1, false);
        for (int t = 0; t < TR_COUNT; t++) {
            const bool got = (bestMask[hover] >> t) & 1;
            fillRect(cx + 40 + t * 22, cyy + 25, 18, 6, got ? TIER[t].col : HEX(0x4a3a2a));
        }
        text(cx + 5, cyy + 35, S(__builtin_popcount(bestMask[hover])) + " OF " + S((int)TR_COUNT) + " TIERS HUNTED", C_DIM, 1, false);
        text(cx + 5, cyy + 47, "DROPS", C_DIM, 1, false);
        int ly = cyy + 56;
        auto line = [&](int m, int lo, int hi, const char* tag) {
            if (ly > cyy + cardH - 10) return;
            icon(A.matIcon[m], cx + 10, ly + 4, 9);
            text(cx + 18, ly, std::string(MAT_NAME[m]).substr(0, 12), C_TEXT, 1, false);
            text(cx + cardW - 8 - textW(S(lo) + "-" + S(hi)), ly, S(lo) + "-" + S(hi), C_GOLD, 1, false);
            if (tag[0]) text(cx + cardW - 30 - textW(tag), ly, tag, C_BLUE, 1, false);
            ly += 9;
        };
        for (int m = 0; m < M_COUNT; m++)
            if (D.drop[m][1] > 0) line(m, D.drop[m][0], D.drop[m][1], "");
        if (D.formDrop[0] >= 0) line(D.formDrop[0], D.formDrop[1], D.formDrop[2], "FORM");
        if (D.legendDrop[0] >= 0) line(D.legendDrop[0], D.legendDrop[1], D.legendDrop[2], "LEGEND");
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
