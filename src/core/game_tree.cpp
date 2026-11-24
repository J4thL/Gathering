// game_tree.cpp - the SKILLS and CRAFT tabs as radial trees: a hub in the middle, branches spreading outward to nodes (pan with the mouse, wheel zooms).
//   skills: hunter rank -> skill groups -> skills -> the four milestone perk pairs of each skill (click a lit node to take the perk)
//   crafting: the crafting hub -> one branch per crafting skill -> its recipes, nearest the hub = the easiest (click a node to see it and craft it)
// The layout is fixed data built once; what changes with play (levels, perks, what can be made) only changes how the nodes are drawn.
#include <algorithm>
#include <cmath>
#include "core/game.hpp"

namespace {
constexpr u32 TG = 0xfff0c850, TD = 0xffb4a584, TT = 0xffeee2c4;  // text on the dark tree: gold, dim, plain
constexpr float TAU = 6.2831853f;
enum TNType { TN_HUB, TN_GROUP, TN_SKILL, TN_PERK, TN_RECIPE };
struct TNode {
    float x = 0, y = 0;
    int type = TN_HUB, a = 0, b = 0, c = 0;  // skill: a = skill; perk: a = skill, b = tier, c = 0 mastery / 1 other; recipe: a = recipe index, b = skill
    int parent = -1;
    u32 col = 0;
};
std::vector<TNode> LAYOUT[2];
float RING[2][8] = {};  // radii of the guide rings
int RINGS[2] = {0, 0};

u32 groupCol(const char* g) {
    const std::string s = g;
    return s == "GATHERING" ? HEX(0x7cc24e) : s == "SURVIVAL" ? HEX(0xd8703c) : s == "CRAFTING" ? HEX(0xe3b341) : HEX(0x5aa0e0);
}
void polar(TNode& n, float ang, float r) { n.x = cosf(ang) * r; n.y = sinf(ang) * r; }

void buildSkillTree() {
    std::vector<TNode>& L = LAYOUT[0];
    L.clear();
    TNode hub; hub.type = TN_HUB; L.push_back(hub);
    static const int G[4][6] = {{SK_WOODCUTTING, SK_MINING, SK_FORAGING, SK_FARMING, SK_FISHING, -1},
                                {SK_HUNTING, SK_SKINNING, SK_BUTCHERING, SK_SURVIVAL, SK_TRACKING, -1},
                                {SK_CRAFTING, SK_SMITHING, SK_LEATHER, SK_COOKING, SK_ALCHEMY, SK_BUILDING},
                                {SK_RANGED, SK_DEFENSE, -1, -1, -1, -1}};
    static_assert(SK_COUNT == 18, "the skill tree lays out 18 skills in four groups: add a new skill to a group in buildSkillTree");
    const float step = TAU / SK_COUNT;
    int idx = 0;
    for (int g = 0; g < 4; g++) {
        int first = idx, n = 0;
        for (int i = 0; i < 6 && G[g][i] >= 0; i++, n++) {}
        TNode gn; gn.type = TN_GROUP; gn.a = G[g][0]; gn.parent = 0; gn.col = groupCol(SKILL[G[g][0]].group);
        polar(gn, -TAU / 4 + (first + n * 0.5f) * step, 36.f);
        const int gi = (int)L.size();
        L.push_back(gn);
        for (int i = 0; i < n; i++, idx++) {
            const int k = G[g][i];
            const float ang = -TAU / 4 + (idx + 0.5f) * step;
            TNode sn; sn.type = TN_SKILL; sn.a = k; sn.parent = gi; sn.col = gn.col;
            polar(sn, ang, 76.f);
            const int si = (int)L.size();
            L.push_back(sn);
            for (int t = 0; t < PERK_TIERS; t++)
                for (int c = 0; c < 2; c++) {
                    TNode pn; pn.type = TN_PERK; pn.a = k; pn.b = t; pn.c = c; pn.col = gn.col;
                    const float r = 106.f + 26.f * t, side = c == 0 ? -8.5f : 8.5f;
                    pn.x = cosf(ang) * r - sinf(ang) * side;
                    pn.y = sinf(ang) * r + cosf(ang) * side;
                    pn.parent = t == 0 ? si : (int)L.size() - 2;  // the same side of the step before
                    L.push_back(pn);
                }
        }
    }
    RINGS[0] = 3;
    RING[0][0] = 36.f; RING[0][1] = 76.f; RING[0][2] = 106.f + 26.f * 1.5f;
}

void buildCraftTree() {
    std::vector<TNode>& L = LAYOUT[1];
    L.clear();
    TNode hub; hub.type = TN_HUB; L.push_back(hub);
    int skills[SK_COUNT], ns = 0;
    for (int k = 0; k < SK_COUNT; k++) {
        bool any = false;
        for (int r = 0; r < RECIPE_N; r++) any = any || RECIPES[r].skill == k;
        if (any) skills[ns++] = k;
    }
    const float sector = TAU / std::max(1, ns);
    for (int s = 0; s < ns; s++) {
        const int k = skills[s];
        const float mid = -TAU / 4 + (s + 0.5f) * sector;
        TNode sn; sn.type = TN_SKILL; sn.a = k; sn.parent = 0; sn.col = groupCol(SKILL[k].group);
        polar(sn, mid, 52.f);
        const int si = (int)L.size();
        L.push_back(sn);
        std::vector<int> rs;
        for (int r = 0; r < RECIPE_N; r++) if (RECIPES[r].skill == k) rs.push_back(r);
        std::stable_sort(rs.begin(), rs.end(), [](int a, int b) { return RECIPES[a].minLv < RECIPES[b].minLv; });
        size_t at = 0;
        int prevFirst = -1, prevCount = 0;
        for (int ring = 0; at < rs.size(); ring++) {
            const float r = 82.f + 26.f * ring;
            const int cap = std::max(1, (int)(sector * 0.88f * r / 21.f));
            const int n = (int)std::min<size_t>(cap, rs.size() - at);
            const int first = (int)L.size();
            for (int i = 0; i < n; i++, at++) {
                const float ang = n == 1 ? mid : mid - sector * 0.44f + sector * 0.88f * i / (n - 1);
                TNode rn; rn.type = TN_RECIPE; rn.a = rs[at]; rn.b = k; rn.col = sn.col;
                polar(rn, ang, r);
                if (ring == 0) rn.parent = si;
                else {  // joined to the nearest node of the ring before
                    float best = 1e9f;
                    for (int j = 0; j < prevCount; j++) {
                        const TNode& q = L[prevFirst + j];
                        const float d = (q.x - rn.x) * (q.x - rn.x) + (q.y - rn.y) * (q.y - rn.y);
                        if (d < best) best = d, rn.parent = prevFirst + j;
                    }
                }
                L.push_back(rn);
            }
            prevFirst = first;
            prevCount = n;
        }
    }
    RINGS[1] = 0;
}

std::string perkName(int k, int t, int c) { return c == 0 ? PERK_NAME_A[t] : (perkBIsSpeed(k) ? PERK_NAME_SPEED[t] : PERK_NAME_LEARN[t]); }
std::string perkDesc(int k, int t, int c) {
    if (c == 0) return "+" + S((int)lroundf(PERK_MASTERY_SHARE[t] * SKILL[k].cap * 100.f)) + "% " + SKILL[k].effect;
    return "+" + S((int)lroundf(PERK_B_VALUE[t] * 100.f)) + "% " + (perkBIsSpeed(k) ? (k == SK_WOODCUTTING ? "CHOPPING SPEED" : "MINING SPEED") : "XP IN THIS SKILL");
}
}  // namespace

void Game::treePickRecipe(int r) {
    if (LAYOUT[1].empty()) buildCraftTree();
    for (size_t i = 0; i < LAYOUT[1].size(); i++)
        if (LAYOUT[1][i].type == TN_RECIPE && LAYOUT[1][i].a == r) {
            treeSel[1] = (int)i;
            treeZoom[1] = 1.f;
            treePan[1] = V2(-LAYOUT[1][i].x, -LAYOUT[1][i].y);
            return;
        }
}

void Game::drawTree(int X, int Y, int W, int H, bool crafts) {
    const bool craft = crafts;
    const bool skin = packSkin;
    packSkin = false;  // the tree is a dark map, drawn in the plain light-on-dark colours rather than the parchment skin
    const int m = craft ? 1 : 0;
    if (LAYOUT[m].empty()) { if (craft) buildCraftTree(); else buildSkillTree(); }
    const std::vector<TNode>& L = LAYOUT[m];
    const int ax = X + 5, ay = Y + 28, aw = W - 10, ah = H - 42;
    const int cx = ax + aw / 2, cy = ay + ah / 2;
    float& zoom = treeZoom[m];
    V2& pan = treePan[m];
    int& sel = treeSel[m];
    if (zoom <= 0.f) zoom = 1.f;
    // ---- the picked node's card (bottom left): decided first, so a click on it does not start a drag
    const int cw = 176, ch = craft ? 64 : 52, cdx = ax + 3, cdy = ay + ah - ch - 3;
    const bool card = sel >= 0 && sel < (int)L.size() && L[sel].type != TN_HUB && L[sel].type != TN_GROUP;
    const bool overCard = card && inRect(cdx, cdy, cw, ch);
    const bool overBtn = inRect(ax + aw - 44, ay + 3, 41, 11);
    // ---- pan (drag) and zoom (wheel)
    const bool inArea = inRect(ax, ay, aw, ah);
    if (inArea && in.wheel != 0 && !overCard) {
        const float wx = (in.mx - cx) / zoom - pan.x, wy = (in.my - cy) / zoom - pan.y;
        zoom = std::max(0.6f, std::min(2.2f, zoom * (in.wheel > 0 ? 1.15f : 1.f / 1.15f)));
        pan.x = (in.mx - cx) / zoom - wx;
        pan.y = (in.my - cy) / zoom - wy;
        in.wheel = 0;
    }
    if (in.lPressed && inArea && !overCard && !overBtn) { treeDown = true; treeMoved = false; treeDX = in.mx; treeDY = in.my; treeLX = in.mx; treeLY = in.my; }
    if (treeDown && in.l) {
        if (fabsf(in.mx - treeDX) + fabsf(in.my - treeDY) > 3.f) treeMoved = true;
        if (treeMoved) { pan.x += (in.mx - treeLX) / zoom; pan.y += (in.my - treeLY) / zoom; }
        treeLX = in.mx; treeLY = in.my;
    }
    auto sx = [&](const TNode& n) { return cx + (int)lroundf((n.x + pan.x) * zoom); };
    auto sy = [&](const TNode& n) { return cy + (int)lroundf((n.y + pan.y) * zoom); };
    // ---- the backdrop and the guide rings
    fillRect(ax, ay, aw, ah, HEX(0x1b140d));
    fillRect(ax, ay, aw, 1, HEX(0x6a5030)); fillRect(ax, ay + ah - 1, aw, 1, HEX(0x6a5030));
    fillRect(ax, ay, 1, ah, HEX(0x6a5030)); fillRect(ax + aw - 1, ay, 1, ah, HEX(0x6a5030));
    auto put = [&](int x, int y, int w, u32 c) { if (x >= ax + 1 && y >= ay + 1 && x + w <= ax + aw - 1 && y < ay + ah - 1) fillRect(x, y, w, w, c); };
    for (int i = 0; i < RINGS[m]; i++) {
        const float r = RING[m][i] * zoom;
        const int n = std::max(24, (int)(r * 0.9f));
        for (int k = 0; k < n; k++) put(cx + (int)lroundf(pan.x * zoom + cosf(k * TAU / n) * r), cy + (int)lroundf(pan.y * zoom + sinf(k * TAU / n) * r), 1, HEX(0x33261a));
    }
    // ---- node look: which state it is in
    const int rank = p.sk.rank();
    auto recState = [&](int r) {  // 0 locked (skill level), 1 short of something, 2 can be made now
        if (RECIPES[r].minLv > p.sk.level(RECIPES[r].skill)) return 0;
        return craftBlock(r).empty() ? 2 : 1;
    };
    auto perkState = [&](int k, int t, int c) {  // 0 locked, 1 can be picked, 2 taken, 3 passed over (the other one was taken)
        if (p.sk.perk[k][t] == c) return 2;
        if (p.sk.perk[k][t] >= 0) return 3;
        return p.sk.canChoose(k, t) ? 1 : 0;
    };
    auto lit = [&](const TNode& n) {  // is the branch up to this node open?
        if (n.type == TN_RECIPE) return recState(n.a) > 0;
        if (n.type == TN_PERK) return perkState(n.a, n.b, n.c) == 2;
        return true;
    };
    // ---- the branches
    for (size_t i = 1; i < L.size(); i++) {
        const TNode& n = L[i];
        const TNode& q = L[n.parent];
        const int x0 = sx(q), y0 = sy(q), x1 = sx(n), y1 = sy(n);
        if (std::max(x0, x1) < ax || std::min(x0, x1) > ax + aw || std::max(y0, y1) < ay || std::min(y0, y1) > ay + ah) continue;
        const bool on = lit(n) && (n.type != TN_PERK || n.b == 0 || perkState(n.a, n.b - 1, p.sk.perk[n.a][n.b - 1]) == 2 || true);
        const u32 c = on ? RGBA(cR(n.col), cG(n.col), cB(n.col), 230) : HEX(0x4a3a28);
        const int steps = std::max(abs(x1 - x0), abs(y1 - y0));
        for (int s = 0; s <= steps; s++) {
            const int px = x0 + (x1 - x0) * s / std::max(1, steps), py = y0 + (y1 - y0) * s / std::max(1, steps);
            put(px, py, 1, c);
            if (on) put(px + 1, py, 1, RGBA(cR(n.col), cG(n.col), cB(n.col), 90));
        }
    }
    // ---- the nodes
    int hover = -1;
    const float pulse = 0.5f + 0.5f * sinf(timeAcc * 5.f);
    const int base = craft ? 15 : 14;
    for (size_t i = 0; i < L.size(); i++) {
        const TNode& n = L[i];
        const int x = sx(n), y = sy(n);
        int sz = n.type == TN_HUB ? 36 : n.type == TN_GROUP ? 12 : n.type == TN_SKILL ? 18 : n.type == TN_PERK ? 11 : base;
        sz = std::max(5, (int)lroundf(sz * std::min(1.5f, zoom)));
        const int h = sz / 2;
        if (x - h < ax + 1 || y - h < ay + 1 || x + h > ax + aw - 1 || y + h > ay + ah - 1) continue;
        const bool over = inRect(x - h, y - h, sz, sz) && !overCard && !overBtn;
        if (over && n.type != TN_GROUP) hover = (int)i;
        const bool picked = (int)i == sel;
        if (n.type == TN_HUB) {
            for (int yy = -h; yy <= h; yy++) {
                const int w = (int)sqrtf((float)(h * h - yy * yy));
                fillRect(x - w, y + yy, 2 * w + 1, 1, HEX(0x2c2014));
            }
            const u32 rim = HEX(0xe3b341);
            for (int k = 0; k < 40; k++) put(x + (int)lroundf(cosf(k * TAU / 40) * h), y + (int)lroundf(sinf(k * TAU / 40) * h), 1, rim);
            if (craft) { text(x - textW("CRAFT") / 2, y - 4, "CRAFT", TG, 1, false); }
            else { text(x - textW("RANK") / 2, y - 7, "RANK", TD, 1, false); const std::string rn = RANK_NAME[rank]; text(x - textW(rn) / 2, y + 1, rn, TG, 1, false); }
        } else if (n.type == TN_GROUP) {
            for (int yy = -h; yy <= h; yy++) { const int w = h - abs(yy); fillRect(x - w, y + yy, 2 * w + 1, 1, n.col); }
            if (zoom >= 0.8f) { const std::string g = SKILL[n.a].group; text(x - textW(g) / 2, y + h + 2, g, n.col, 1, false); }
        } else if (n.type == TN_SKILL) {
            const int lv = p.sk.level(n.a);
            const bool known = lv > 1 || p.sk.s[n.a].xp > 0;
            fillRect(x - h - 1, y - h - 1, sz + 2, sz + 2, picked ? HEX(0xffffff) : known ? n.col : HEX(0x6a5a44));
            fillRect(x - h, y - h, sz, sz, over ? HEX(0x4a3a28) : HEX(0x2c2014));
            std::string ab = std::string(SKILL[n.a].name).substr(0, 3);
            if (zoom >= 0.8f) {
                text(x - textW(ab) / 2, y - h + 2, ab, known ? TG : HEX(0xa89060), 1, false);
                if (!craft) text(x - textW(S(lv)) / 2, y + 1, S(lv), TT, 1, false);
            }
            if (!craft) {
                const float f = lv >= MAX_LEVEL ? 1.f : (float)p.sk.s[n.a].into() / (float)std::max<int64_t>(1, xpForLevel(lv));
                fillRect(x - h + 1, y + h - 2, std::max(0, (int)((sz - 2) * f)), 1, HEX(0x4aa3d8));
                if (p.sk.pending(n.a) > 0) fillRect(x + h - 2, y - h - 3, 5, 5, pulse > 0.5f ? TG : HEX(0xa07a20));
            }
        } else if (n.type == TN_PERK) {
            const int st = perkState(n.a, n.b, n.c);
            u32 fill = st == 2 ? n.col : st == 1 ? (pulse > 0.5f ? TG : HEX(0xa07a20)) : st == 3 ? HEX(0x3a3026) : HEX(0x2c2014);
            u32 rim = st == 2 ? HEX(0xffffff) : st == 1 ? TG : HEX(0x5a4a38);
            if (picked) rim = HEX(0xffffff);
            for (int yy = -h - 1; yy <= h + 1; yy++) { const int w = h + 1 - abs(yy); fillRect(x - w, y + yy, 2 * w + 1, 1, rim); }
            for (int yy = -h; yy <= h; yy++) { const int w = h - abs(yy); if (w >= 0) fillRect(x - w, y + yy, 2 * w + 1, 1, over && st != 3 ? HEX(0xc8b890) : fill); }
            if (zoom >= 0.8f) text(x - 2, y - 2, n.c == 0 ? "+" : "*", st == 2 ? HEX(0x10200a) : st == 1 ? HEX(0x10200a) : HEX(0x6a5a44), 1, false);
        } else {  // recipe
            const int st = recState(n.a);
            const u32 rim = picked ? HEX(0xffffff) : st == 2 ? TG : st == 1 ? HEX(0xa8a8b0) : HEX(0x4a3a28);
            fillRect(x - h - 1, y - h - 1, sz + 2, sz + 2, rim);
            fillRect(x - h, y - h, sz, sz, over ? HEX(0x4a3a28) : HEX(0x2c2014));
            icon(itemIcon(recipeItem(RECIPES[n.a])), x, y, std::max(6, sz - 3), st == 0 ? 0.3f : st == 1 ? 0.7f : 1.f);
        }
    }
    // ---- a hover label
    if (hover >= 0) {
        const TNode& n = L[hover];
        std::string t1, t2;
        if (n.type == TN_SKILL) { t1 = SKILL[n.a].name; t2 = "LV " + S(p.sk.level(n.a)) + (craft ? "" : "  +" + S((int)lroundf(skillBonusAt(n.a, p.sk.level(n.a)) * 100.f)) + "% " + SKILL[n.a].effect); }
        else if (n.type == TN_PERK) { t1 = perkName(n.a, n.b, n.c); t2 = "LV " + S(MILESTONES[n.b]) + "  " + perkDesc(n.a, n.b, n.c); }
        else if (n.type == TN_RECIPE) { t1 = recipeName(RECIPES[n.a]); const std::string w = craftBlock(n.a); t2 = w.empty() ? "READY TO CRAFT" : w; }
        if (!t1.empty()) {
            const int tw = std::max(textW(t1), textW(t2)) + 8;
            const int tx = std::min((int)in.mx + 10, ax + aw - tw - 2), ty = std::min((int)in.my + 10, ay + ah - 26);
            panel(tx, ty, tw, 24, HEX(0x241a10));
            text(tx + 4, ty + 4, t1, TG, 1, false);
            text(tx + 4, ty + 13, t2, TD, 1, false);
        }
    }
    // ---- a click (not a drag) on a node picks it; on the empty ground it clears the pick
    if (treeDown && in.lReleased) {
        if (!treeMoved) {
            sel = hover;
            if (hover >= 0) audio.play(S_CLICK, 0.5f);
        }
        treeDown = false;
    }
    if (treeDown && !in.l) treeDown = false;
    // ---- the picked node's card
    if (card) {
        const TNode& n = L[sel];
        panel(cdx, cdy, cw, ch, HEX(0x3a2a1a));
        if (n.type == TN_SKILL) {
            const int k = n.a, lv = p.sk.level(k);
            text(cdx + 6, cdy + 5, SKILL[k].name, TG, 1, false);
            text(cdx + cw - 6 - textW("LV " + S(lv)), cdy + 5, "LV " + S(lv), TT, 1, false);
            if (craft) {
                text(cdx + 6, cdy + 17, std::string(SKILL[k].group) + " - RECIPES OPEN AS IT RISES", TD, 1, false);
                int made = 0, open = 0;
                for (int r = 0; r < RECIPE_N; r++) if (RECIPES[r].skill == k) { made++; open += RECIPES[r].minLv <= lv; }
                text(cdx + 6, cdy + 28, S(open) + " OF " + S(made) + " RECIPES OPEN", TT, 1, false);
            } else {
                const int nm = nextMilestone(lv);
                text(cdx + 6, cdy + 17, "+" + S((int)lroundf(skillBonusAt(k, lv) * 100.f)) + "% " + SKILL[k].effect, TT, 1, false);
                text(cdx + 6, cdy + 28, nm ? "NEXT PERK AT LV " + S(nm) : std::string("ALL PERKS REACHED"), TD, 1, false);
                const float f = lv >= MAX_LEVEL ? 1.f : (float)p.sk.s[k].into() / (float)std::max<int64_t>(1, xpForLevel(lv));
                bar(cdx + 6, cdy + 40, cw - 12, 5, f, HEX(0x4aa3d8));
            }
        } else if (n.type == TN_PERK) {
            const int k = n.a, t = n.b, c = n.c, st = perkState(k, t, c);
            text(cdx + 6, cdy + 5, perkName(k, t, c), TG, 1, false);
            text(cdx + cw - 6 - textW(std::string(SKILL[k].name)), cdy + 5, SKILL[k].name, TD, 1, false);
            text(cdx + 6, cdy + 17, perkDesc(k, t, c), TT, 1, false);
            if (st == 1) {
                if (button(cdx + 6, cdy + 30, 70, 14, "TAKE PERK", true, HEX(0x5d7a2e)) && p.sk.choose(k, t, c)) {
                    msg(std::string(SKILL[k].name) + ": " + perkName(k, t, c) + " (" + perkDesc(k, t, c) + ")", C_GREEN);
                    audio.play(S_CRAFT);
                }
                text(cdx + 82, cdy + 34, "FINAL - THE OTHER IS LOST", TD, 1, false);
            } else text(cdx + 6, cdy + 33, st == 2 ? "TAKEN" : st == 3 ? "NOT TAKEN" : "REACH SKILL LV " + S(MILESTONES[t]) + " (YOU " + S(p.sk.level(k)) + ")", st == 2 ? C_GREEN : TD, 1, false);
        } else {  // recipe
            const Recipe& R = RECIPES[n.a];
            const std::string why = craftBlock(n.a);
            const bool can = why.empty();
            icon(itemIcon(recipeItem(R)), cdx + 12, cdy + 12, 16);
            text(cdx + 24, cdy + 4, recipeName(R) + (R.count > 1 ? " X" + S(R.count) : std::string("")), can ? TG : HEX(0xa89060), 1, false);
            text(cdx + 24, cdy + 14, std::string(SKILL[R.skill].name) + " " + S(R.minLv), p.sk.level(R.skill) >= R.minLv ? C_GREEN : C_RED, 1, false);
            int ix = cdx + 6;
            for (int k = 0; k < 3; k++)
                if (R.mat[k] >= 0) ix = chip(ix, cdy + 31, A.matIcon[R.mat[k]], p.mat(R.mat[k]), R.n[k]);
            treeCraftBtn = V2((float)(cdx + cw - 31), (float)(cdy + 24));
            std::string lab = "CRAFT";
            if (why.compare(0, 6, "NEEDS ") == 0) lab = "LOCKED";
            else if (!can && why.compare(0, 8, "MADE AT ") == 0) lab = "IN TOWN";
            if (button(cdx + cw - 56, cdy + 18, 50, 13, lab, can, HEX(0x5d7a2e))) this->craft(n.a);
            if (can && button(cdx + cw - 84, cdy + 18, 26, 13, "MAX", true, HEX(0x7a5d2e))) craftMany(n.a, 999);
            if (!can) text(cdx + 6, cdy + 51, why, C_RED, 1, false);
        }
    }
    if (button(ax + aw - 44, ay + 3, 41, 11, "CENTER", true, HEX(0x4a3a2a))) { pan = V2(0, 0); zoom = 1.f; }
    text(ax + 6, ay + 5, craft ? "CRAFTING TREE" : "SKILL TREE", TG, 1, false);
    packSkin = skin;
    textC(X + W / 2, Y + H - 8, craft ? "DRAG TO MOVE   WHEEL ZOOMS   CLICK A RECIPE   CRAFTING IS DONE IN THE TOWN BUILDINGS" : "DRAG TO MOVE   WHEEL ZOOMS   CLICK A NODE   LIT NODES CAN BE TAKEN", C_DIM, 1, false);
}
