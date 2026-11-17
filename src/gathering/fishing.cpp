// fishing.cpp - the fishing rod: cast at any water, wait for the bobber to dip, press again in time to land the fish.
//
//  phase 0 idle  ->  click on water (within reach): the line is cast, phase 1 (waiting; a click now pulls it in empty)
//  phase 1 waits a few seconds, then a bite: phase 2 (the bobber dips) for a short window; a click in the window lands a fish, a late one lets it go
// Which fish bites depends on the water's biome and the season; the FISHING skill speeds the bites up, gives a chance of a second fish, and pays XP.
#include "core/game.hpp"

namespace {
struct Bite { int mat; int w[SEASON_COUNT]; };  // weights per season (spring, summer, autumn, winter)
const std::vector<Bite>& biteTable(int biome) {
    static const std::vector<Bite> HOME = {{M_TROUT, {6, 4, 4, 3}}, {M_BASS, {3, 6, 4, 1}}, {M_EEL, {2, 2, 3, 1}}};
    static const std::vector<Bite> SWAMP = {{M_CATFISH, {4, 6, 4, 2}}, {M_BASS, {3, 5, 3, 1}}, {M_EEL, {4, 3, 4, 2}}};
    static const std::vector<Bite> SNOW = {{M_CHAR, {5, 4, 5, 8}}, {M_PIKE, {2, 2, 3, 5}}};
    static const std::vector<Bite> MOUNTAIN = {{M_TROUT, {6, 5, 5, 3}}, {M_PIKE, {3, 3, 5, 5}}, {M_CHAR, {2, 2, 3, 4}}};
    static const std::vector<Bite> DESERT = {{M_CATFISH, {5, 5, 4, 4}}, {M_BASS, {3, 4, 3, 3}}};
    switch (biome) {
        case B_SWAMP: return SWAMP;
        case B_SNOW: return SNOW;
        case B_MOUNTAIN: return MOUNTAIN;
        case B_DESERT: return DESERT;
        default: return HOME;
    }
}
constexpr float CAST_REACH = 130.f;   // how far the line can be cast
constexpr float LINE_BREAK = 190.f;   // walking this far from the bobber reels the line in
constexpr float BITE_WINDOW = 0.85f;  // seconds to react once the bobber dips
}  // namespace

int Game::fishPick(int biome) {
    const auto& tab = biteTable(biome);
    int total = 0;
    for (auto& b : tab) total += b.w[season()];
    int r = frng.irange(0, std::max(0, total - 1));
    for (auto& b : tab) {
        r -= b.w[season()];
        if (r < 0) return b.mat;
    }
    return tab.back().mat;
}

void Game::fishStop(const char* why) {
    if (fishing.phase && why) msg(why, C_DIM);
    fishing.phase = 0;
}

// A left click with the rod in hand.
bool Game::fishClick() {
    Fishing& F = fishing;
    if (F.phase == 1) { fishStop("YOU REELED IN - NOTHING BIT"); return true; }
    if (F.phase == 2) {
        const int mat = fishPick(world.biomeAtPos(F.at.x, F.at.y));
        int n = 1;
        if (rng.chance(skillBonus(SK_FISHING))) n++;  // a bigger catch
        const int got = p.inv.add(itemMat(mat), n);
        if (got < n) p.inv.addIgnoringWeight(itemMat(mat), n - got);
        fishCaught += n;
        grantXp(SK_FISHING, 6 + MAT_PRICE[mat] / 2);
        ftext(F.at + V2(0, -10), "+" + S(n) + " " + MAT_NAME[mat], C_GOLD, 1.6f);
        puff(F.at, 5, HEX(0xbfe3ff));
        audio.play(S_CRAFT, 0.8f);
        questEvent(Q_GATHER, mat, n);
        checkTrophies();
        trialFish(n);
        F.phase = 0;
        return true;
    }
    // cast: at the water under the cursor, within reach
    const V2 mw(in.mx + camX, in.my + camY);
    const int tx = (int)floorf(mw.x / TS), ty = (int)floorf(mw.y / TS);
    if (!world.water(tx, ty)) { msg("AIM AT WATER AND CLICK TO CAST", C_RED); audio.play(S_ERROR, 0.5f); return true; }
    const V2 target((tx + 0.5f) * TS, (ty + 0.5f) * TS);
    if (dist(p.pos, target) > CAST_REACH) { msg("TOO FAR - MOVE CLOSER TO THE WATER", C_RED); audio.play(S_ERROR, 0.5f); return true; }
    F.phase = 1;
    F.t = 0;
    F.at = target;
    F.biteAt = rng.range(2.5f, 7.f) / (1.f + skillBonus(SK_FISHING));
    F.window = BITE_WINDOW;
    p.aim = (target - p.pos).norm();
    puff(F.at, 4, HEX(0xbfe3ff));
    audio.play(S_CLICK, 0.7f);
    return true;
}

void Game::updateFishing(float dt) {
    Fishing& F = fishing;
    if (!F.phase) return;
    const bool rodInHand = p.hand == HAND_ITEM && p.hotSel >= 0 && p.hot[p.hotSel] == itemTool(TL_ROD);
    if (p.dead || state != ST_PLAY || interior >= 0 || delve >= 0 || !rodInHand) { F.phase = 0; return; }
    if (dist(p.pos, F.at) > LINE_BREAK) { fishStop("THE LINE WENT SLACK - YOU WALKED TOO FAR"); return; }
    F.t += dt;
    if (F.phase == 1 && F.t >= F.biteAt) {  // a bite: the bobber dips
        F.phase = 2;
        F.t = 0;
        puff(F.at, 3, HEX(0xffffff));
        ftext(F.at + V2(0, -12), "!", C_GOLD, 0.8f);
        audio.play(S_CLICK, 1.f);
    } else if (F.phase == 2 && F.t >= F.window) {
        fishStop("IT GOT AWAY");
    }
}

// The rod's line from the hunter's hand to the bobber, and the bobber itself (it dips when a fish bites).
void Game::drawFishing() {
    const Fishing& F = fishing;
    if (!F.phase) return;
    const int hx = (int)p.pos.x - camX + (int)(p.aim.x * 6.f), hy = (int)p.pos.y - camY - 8;
    const int bx = (int)F.at.x - camX, by = (int)F.at.y - camY;
    int x0 = hx, y0 = hy, dx = abs(bx - x0), sx = x0 < bx ? 1 : -1, dy = -abs(by - y0), sy = y0 < by ? 1 : -1, err = dx + dy;
    for (int guard = 0; guard < 400; guard++) {  // the line (a sagging straight line: one pixel each step)
        fillRect(x0, y0, 1, 1, RGBA(235, 235, 235, 190));
        if (x0 == bx && y0 == by) break;
        const int e2 = 2 * err;
        if (e2 >= dy) err += dy, x0 += sx;
        if (e2 <= dx) err += dx, y0 += sy;
    }
    const int bob = F.phase == 2 ? 2 + (int)(timeAcc * 30.f) % 2 : (int)(sinf(timeAcc * 3.f) * 0.6f);  // the dip shakes
    fillRect(bx - 1, by + bob - 1, 3, 3, HEX(0xf05050));
    fillRect(bx - 1, by + bob - 1, 3, 1, HEX(0xffffff));
    if (F.phase == 2) fillRect(bx - 5, by + 3, 11, 1, RGBA(255, 255, 255, 170));  // ripples
}
