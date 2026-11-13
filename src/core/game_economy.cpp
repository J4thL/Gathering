// Town economy: reputation with the village, the general store's market (demand swings per material) and the daily orders board.
// Everything here is saved with the game (see economySave / economyLoadLine) and reset by newGame() through economyInit().
#include <algorithm>
#include <cmath>
#include "core/game.hpp"
#include "crafting/recipes.hpp"

static const int REP_AT[Game::REP_LEVELS] = {0, 25, 75, 160};
static const char* REP_NAME[Game::REP_LEVELS] = {"STRANGER", "KNOWN", "TRUSTED", "HONORED"};
static const float DEMAND_MIN = 0.7f, DEMAND_MAX = 1.3f;

int Game::repLevel() const {
    int l = 0;
    for (int i = 1; i < REP_LEVELS; i++)
        if (rep >= REP_AT[i]) l = i;
    return l;
}
const char* Game::repName() const { return REP_NAME[repLevel()]; }
int Game::repNext() const { return repLevel() + 1 < REP_LEVELS ? REP_AT[repLevel() + 1] : -1; }

void Game::addRep(int n) {
    if (n <= 0) return;
    const int before = repLevel();
    rep = std::min(rep + n, 100000);
    if (repLevel() > before) {
        msg(std::string("TOWN REPUTATION: ") + repName() + " - BETTER PRICES", C_GOLD);
        audio.play(S_CRAFT);
    }
}

int Game::bonusPrice(int base) const { return std::max(1, (int)(base * (100 + 4 * repLevel()) / 100.0f + 0.5f)); }
int Game::buyPrice(int base) const { return std::max(1, (int)(base * (100 - 5 * repLevel()) / 100.0f + 0.5f)); }
int Game::matSellPrice(int m) const {
    const float d = demand[m] > 0 ? demand[m] : 1.f;
    return std::max(1, (int)(MAT_PRICE[m] * d * (1 + 0.04f * repLevel()) + 0.5f));
}

// Coins earned at the store: every 150 of them earns a point of reputation, and what was sold gets a little less wanted.
void Game::noteSold(int gain, int mat, int n) {
    repAcc += std::max(0, gain);
    while (repAcc >= 150) {
        repAcc -= 150;
        addRep(1);
    }
    if (mat >= 0 && mat < M_COUNT) {
        const float d = demand[mat] > 0 ? demand[mat] : 1.f;
        demand[mat] = std::max(DEMAND_MIN, d - 0.01f * n);
    }
}

void Game::economyInit() {
    rep = repAcc = 0;
    lastDay = 1;
    for (int m = 0; m < M_COUNT; m++) demand[m] = 1.f;
    makeOrders(1);
}

// A new day: demand drifts back toward normal with a small random push, and the board gets fresh orders.
void Game::newDay(int day) {
    Rng r((uint32_t)(31337 + day * 7919));
    for (int m = 0; m < M_COUNT; m++) {
        float d = demand[m] > 0 ? demand[m] : 1.f;
        d += (1.f - d) * 0.25f + r.range(-0.06f, 0.06f);
        demand[m] = std::max(DEMAND_MIN, std::min(DEMAND_MAX, d));
    }
    lastDay = day;
    makeOrders(day);
    msg("NEW DAY - THE STORE HAS NEW ORDERS", C_GOLD);
}

// Three crafted things the store wants today. Only things the hunter can make (skill level) are asked for; the quality asked for grows with reputation.
void Game::makeOrders(int day) {
    std::vector<int> pool;
    for (int r = 0; r < RECIPE_N; r++) {
        const Recipe& R = RECIPES[r];
        if (R.kind == RK_GEAR || !recipeHasQuality(R) || R.minLv > p.sk.level(R.skill) + 5) continue;
        const int id = recipeItem(R);
        const int k = item(id).kind;
        if (k != IK_CONSUMABLE && k != IK_AMMO && k != IK_PLACEABLE) continue;  // gear you carry or hold is never ordered
        if (itemSellBase(id) <= 0 || std::find(pool.begin(), pool.end(), id) != pool.end()) continue;
        pool.push_back(id);
    }
    for (Order& o : orders) o = Order();
    Rng r((uint32_t)(9001 + day * 104729));
    const int maxQ = std::min((int)Q_COUNT - 1, repLevel());
    for (int i = 0; i < 3 && !pool.empty(); i++) {
        const int pi = r.irange(0, (int)pool.size() - 1);
        Order& o = orders[i];
        o.item = pool[pi];
        pool.erase(pool.begin() + pi);
        o.q = r.irange(0, maxQ);
        o.n = r.irange(1, 3);
        o.pay = (int)(qualityPrice(itemSellBase(o.item), o.q) * o.n * 1.5f + 0.5f);
        o.rep = 6 + 2 * o.q;
    }
}

bool Game::orderCanFill(const Order& o) const {
    if (o.item < 0 || o.done) return false;
    int have = 0;
    for (int q = o.q; q < Q_COUNT; q++) have += p.inv.countQ(o.item, q);
    return have >= o.n;
}

// Hand over the cheapest items that are good enough.
void Game::fillOrder(int i) {
    if (i < 0 || i >= 3 || !orderCanFill(orders[i])) {
        audio.play(S_ERROR);
        return;
    }
    Order& o = orders[i];
    int left = o.n;
    for (int q = o.q; q < Q_COUNT && left > 0; q++) left -= p.inv.removeQ(o.item, q, left);
    const int pay = bonusPrice(o.pay);
    p.coins += pay;
    sold += pay;
    o.done = true;
    addRep(o.rep);
    audio.play(S_COIN);
    msg("ORDER FILLED: +" + S(pay) + " COINS  +" + S(o.rep) + " REPUTATION", C_GOLD);
    ftext(p.pos + V2(0, -24), "+" + S(pay) + " COINS", C_GOLD, 1.0f);
    questEvent(Q_EARN, 0, pay);
    save();
}

void Game::economySave(std::ostream& f) const {
    f << "rep " << rep << ' ' << repAcc << ' ' << lastDay << "\ndemand";
    for (int m = 0; m < M_COUNT; m++) f << ' ' << (int)(((demand[m] > 0 ? demand[m] : 1.f) * 1000.f) + 0.5f);
    f << "\n";
    for (int i = 0; i < 3; i++)
        if (orders[i].item >= 0)
            f << "order " << i << ' ' << itemKey(orders[i].item) << ' ' << orders[i].q << ' ' << orders[i].n << ' ' << orders[i].pay << ' ' << orders[i].rep << ' ' << (orders[i].done ? 1 : 0) << "\n";
}

void Game::economyLoadLine(const std::string& k, std::istringstream& ss) {
    if (k == "rep") {
        long long a = 0, b = 0, d = 1;
        if (ss >> a >> b >> d) {
            rep = (int)std::max(0LL, std::min(100000LL, a));
            repAcc = (int)std::max(0LL, std::min(149LL, b));
            lastDay = (int)std::max(1LL, std::min(1000000LL, d));
        }
    } else if (k == "demand") {
        for (int m = 0; m < M_COUNT; m++) {
            long long v = 1000;
            if (!(ss >> v)) break;
            demand[m] = std::max(DEMAND_MIN, std::min(DEMAND_MAX, (float)v / 1000.f));
        }
    } else if (k == "order") {
        long long i = -1, q = 0, n = 0, pay = 0, rp = 0, dn = 0;
        std::string key;
        if (!(ss >> i >> key >> q >> n >> pay >> rp >> dn) || i < 0 || i > 2) return;
        const int id = itemFromKey(key);
        if (!itemValid(id)) return;
        Order& o = orders[i];
        o.item = id;
        o.q = clampQuality((int)q);
        o.n = (int)std::max(1LL, std::min(99LL, n));
        o.pay = (int)std::max(0LL, std::min(1000000LL, pay));
        o.rep = (int)std::max(0LL, std::min(100LL, rp));
        o.done = dn != 0;
    }
}
