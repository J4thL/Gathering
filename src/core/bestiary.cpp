// bestiary.cpp - the pack's BESTIARY tab: how many of each species the hunter has killed, and the coin rewards for it.
// A kill is counted when the hunter makes it (killAnimal); a species stays "???" until the first one is hunted.
#include "core/game.hpp"

static constexpr int BEST_STEP[3] = {5, 20, 50};      // kills of one species that earn a reward
static constexpr int BEST_MUL[3] = {1, 3, 8};          // reward = (8 + health / 3) coins x this

// trophies: milestones read from the bestiary data (so old saves count too); each pays coins once
struct TrophyDef { const char* name; const char* what; int kind; int need; int coins; };  // kind 0 species found, 1 legends hunted, 2 total kills, 3 fish caught
static const TrophyDef TROPHY[TROPHY_COUNT] = {
    {"NATURALIST", "FIND 10 SPECIES", 0, 10, 100},   {"FIELD GUIDE", "FIND 20 SPECIES", 0, 20, 300},   {"COMPLETE BESTIARY", "FIND EVERY SPECIES", 0, A_COUNT, 1000},
    {"LEGEND SEEKER", "HUNT A LEGENDARY", 1, 1, 150}, {"LEGEND HUNTER", "HUNT 5 LEGENDARIES", 1, 5, 500}, {"LEGEND SLAYER", "HUNT 10 LEGENDARIES", 1, 10, 1500},
    {"HUNTER", "HUNT 100 ANIMALS", 2, 100, 150},      {"VETERAN", "HUNT 500 ANIMALS", 2, 500, 600},      {"WARLORD", "HUNT 2000 ANIMALS", 2, 2000, 2000},
    {"ANGLER", "CATCH 10 FISH", 3, 10, 100},          {"FISHERMAN", "CATCH 50 FISH", 3, 50, 300},         {"MASTER ANGLER", "CATCH 200 FISH", 3, 200, 800},
};

const char* trophyName(int t) { return TROPHY[t].name; }
int trophyNeed(int t) { return TROPHY[t].need; }

int Game::trophyProgress(int t) const {
    int n = 0;
    const int kind = TROPHY[t].kind;
    if (kind == 3) return fishCaught;
    for (int k = 0; k < A_COUNT; k++) {
        if (kind == 0) n += bestKills[k] > 0;
        else if (kind == 1) n += (bestMask[k] >> TR_LEGENDARY) & 1;
        else n += bestKills[k];
    }
    return n;
}

void Game::checkTrophies() {
    for (int t = 0; t < TROPHY_COUNT; t++)
        if (!(trophyMask >> t & 1) && trophyProgress(t) >= TROPHY[t].need) {
            trophyMask |= 1 << t;
            p.coins += TROPHY[t].coins;
            msg(std::string("TROPHY: ") + TROPHY[t].name + "  +" + S(TROPHY[t].coins) + " COINS", C_GOLD);
            audio.play(S_CRAFT);
        }
}

static int bestReward(int kind, int step) { return (8 + ANIMAL[kind].hp / 3) * BEST_MUL[step]; }

static constexpr int TRIAL_MAX_PAY = 800;

// The daily hunt trial: a species the hunter has already found (a rabbit at the start), a count that grows with hunter rank, paid once.
// Warden and up are given a legendary hunt every third day. The result is saved, so reloading the day does not change it.
void Game::makeTrial(int day) {
    Rng r((uint32_t)(4242 + day * 6151));
    trial = Trial();
    if (p.sk.rank() >= 2 && day % 3 == 0) {
        trial.kind = 1;
        trial.need = 1;
        trial.pay = 400 + 100 * p.sk.rank();
        return;
    }
    if (day % 4 == 1 && ownsTool(TL_ROD)) {  // every fourth day, for hunters who carry a rod: a fishing goal
        trial.kind = 2;
        trial.need = 3 + p.sk.rank() + r.irange(0, 2);
        trial.pay = std::min(TRIAL_MAX_PAY, trial.need * 30);
        return;
    }
    std::vector<int> found;
    for (int k = 0; k < A_COUNT; k++)
        if (bestKills[k] > 0) found.push_back(k);
    trial.species = found.empty() ? (int)A_RABBIT : found[r.irange(0, (int)found.size() - 1)];
    trial.need = 3 + p.sk.rank() * 2 + r.irange(0, 2);
    trial.pay = std::min(TRIAL_MAX_PAY, trial.need * (8 + ANIMAL[trial.species].hp / 3) * 2);  // capped: big game in big numbers would out-earn everything else
}

std::string Game::trialLabel() const {
    if (trial.kind == 1) return "HUNT A LEGENDARY";
    if (trial.kind == 2) return "CATCH " + S(trial.need) + " FISH";
    return "HUNT " + S(trial.need) + " " + ANIMAL[trial.species].name;
}

void Game::trialFish(int n) {
    if (trial.kind == 2) trialAdvance(n);
}

void Game::trialKill(int kind, int tier) {
    if (trial.kind == 2) return;
    if (trial.kind == 1 ? tier != TR_LEGENDARY : kind != trial.species) return;
    trialAdvance(1);
}

void Game::trialAdvance(int n) {
    if (trial.done || trial.need <= 0) return;
    trial.got += n;
    if (trial.got < trial.need) return;
    trial.got = trial.need;
    trial.done = true;
    p.coins += trial.pay;
    addRep(6);
    msg("TRIAL DONE: +" + S(trial.pay) + " COINS", C_GOLD);
    audio.play(S_CRAFT);
}

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
    trialKill(kind, tier);
    checkTrophies();
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
        if (hover == k) fillRect(x - 2, y - 1, colW - 4, rowH, skinFill(HEX(0x3f3220)));
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
    {  // the TROPHIES button opens the list over the page
        int got = 0;
        for (int t = 0; t < TROPHY_COUNT; t++) got += trophyMask >> t & 1;
        const int bw = 84, bh = 11, bx = X + W - bw - 10, by = Y + H - 24;
        if (!trophyOpen && button(bx, by, bw, bh, "TROPHIES " + S(got) + "/" + S((int)TROPHY_COUNT))) trophyOpen = true;
    }
    {  // today's hunt trial, beside the TROPHIES button
        std::string s = "TODAY: " + trialLabel();
        s += trial.done ? "  DONE" : "  " + S(trial.got) + "/" + S(trial.need) + "  +" + S(trial.pay);
        text(X + 10, Y + H - 22, s, trial.done ? C_GREEN : C_TEXT, 1, false);
    }
    if (trophyOpen) {
        const int ox = X + 14, oy = Y + 30, ow = W - 28, oh = 162;
        panel(ox, oy, ow, oh, HEX(0x3a2a1a));
        text(ox + 8, oy + 6, "TROPHIES", C_GOLD, 1, false);
        for (int t = 0; t < TROPHY_COUNT; t++) {
            const int y = oy + 18 + t * 10;
            const bool done = trophyMask >> t & 1;
            const int have = std::min(trophyProgress(t), TROPHY[t].need);
            fillRect(ox + 8, y + 1, 6, 6, done ? C_GOLD : HEX(0x4a3a2a));
            text(ox + 20, y, TROPHY[t].name, done ? C_GOLD : C_TEXT, 1, false);
            text(ox + 112, y, TROPHY[t].what, C_DIM, 1, false);
            const std::string r = done ? std::string("DONE") : S(have) + "/" + S(TROPHY[t].need) + "  +" + S(TROPHY[t].coins);
            text(ox + ow - 8 - textW(r), y, r, done ? C_GREEN : C_DIM, 1, false);
        }
        if (button(ox + ow - 50, oy + oh - 15, 42, 11, "BACK")) trophyOpen = false;
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
