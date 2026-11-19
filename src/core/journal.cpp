// journal.cpp - the pack's JOURNAL tab: everything the hunter is working on, on one page (read only; each thing is still handed in where it always was).
#include "core/game.hpp"
#include "core/trophies.hpp"

void Game::drawJournalTab(int X, int Y, int W, int H) {
    (void)H;
    const int x0 = X + 12;
    int y = Y + 40;
    auto head = [&](const char* s) {
        text(x0, y, s, C_GOLD, 1, false);
        fillRect(x0, y + 9, W - 24, 1, HEX(0x4a2a22));
        y += 13;
    };

    head("QUEST - ELDER HOLT");
    if (!questActive) text(x0 + 4, y, "NONE ACTIVE - TALK TO THE ELDER", C_DIM, 1, false);
    else text(x0 + 4, y, quest.title + "  " + S(questProg) + "/" + S(quest.goal) + (questReady() ? "  READY TO TURN IN" : ""), questReady() ? C_GREEN : C_TEXT, 1, false);
    y += 16;

    head("TODAY'S HUNT TRIAL");
    {
        std::string s = trialLabel();
        s += trial.done ? "  DONE" : "  " + S(trial.got) + "/" + S(trial.need) + "  +" + S(trial.pay) + " COINS";
        text(x0 + 4, y, s, trial.done ? C_GREEN : C_TEXT, 1, false);
    }
    y += 16;

    head("STORE ORDERS");
    for (int i = 0; i < 3; i++) {
        const Order& o = orders[i];
        if (o.item < 0) continue;
        int have = 0;
        for (int q = o.q; q < Q_COUNT; q++) have += p.inv.countQ(o.item, q);
        std::string s = item(o.item).name.substr(0, 18) + " X" + S(o.n);
        if (o.q != Q_NORMAL) s += std::string(" ") + qualityDef(o.q).name;
        text(x0 + 4, y, s, o.done ? C_DIM : C_TEXT, 1, false);
        const std::string r = o.done ? std::string("DONE") : std::string("HAVE ") + S(have) + "  +" + S(bonusPrice(o.pay));
        text(X + W - 12 - textW(r), y, r, o.done ? C_GREEN : (have >= o.n ? C_GREEN : C_DIM), 1, false);
        y += 11;
    }
    y += 5;

    head("NEXT GOALS");
    {
        // the nearest unpaid trophy
        int best = -1;
        float bf = -1.f;
        for (int t = 0; t < TROPHY_COUNT; t++) {
            if (trophyMask >> t & 1) continue;
            const float f = std::min(1.f, (float)trophyProgress(t) / (float)trophyNeed(t));
            if (f > bf) bf = f, best = t;
        }
        if (best >= 0) text(x0 + 4, y, std::string("TROPHY ") + trophyName(best) + "  " + S(std::min(trophyProgress(best), trophyNeed(best))) + "/" + S(trophyNeed(best)), C_TEXT, 1, false);
        else text(x0 + 4, y, "EVERY TROPHY EARNED", C_GREEN, 1, false);
        y += 11;
        const int rk = p.sk.rank(), tot = p.sk.totalLevels();
        if (rk + 1 < RANK_COUNT) text(x0 + 4, y, std::string("RANK ") + RANK_NAME[rk + 1] + "  " + S(tot) + "/" + S(RANK_AT[rk + 1]) + " SKILL LEVELS", C_TEXT, 1, false);
        else text(x0 + 4, y, std::string("RANK ") + RANK_NAME[rk] + " - THE HIGHEST", C_GREEN, 1, false);
        y += 11;
        if (kingTier > 0) {
            text(x0 + 4, y, std::string("FROSTBOUND KING RETURNS - TIER ") + S(kingTier) + "/" + S(KING_TIERS), C_TEXT, 1, false);
            y += 11;
        }
        {
            int slain = 0, total = 0;
            for (int i = 0; i < BOSS_N; i++) { total++; if (bossKills[BOSSES[i].biome] > 0) slain++; }
            text(x0 + 4, y, std::string("BOSSES SLAIN ") + S(slain) + "/" + S(total), slain == total ? C_GREEN : C_TEXT, 1, false);
            y += 11;
        }
        int pend = 0;
        for (int k = 0; k < SK_COUNT; k++) pend += p.sk.pending(k);
        if (pend > 0) text(x0 + 4, y, S(pend) + " PERK CHOICES WAITING - SKILLS TAB", C_GOLD, 1, false);
    }
}
