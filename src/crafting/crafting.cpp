// crafting.cpp - the crafting rules and the pack's CRAFTING tab.
//
// Rules (craftBlock / craft): a recipe needs its ingredients, the right station, a bag with room for the result (ingredients are used up
// first, so a recipe that frees a slot works in a full bag), and - for single-copy things like tools, launchers and gear - not already
// owning the thing. Nothing is consumed unless the result is delivered.
// The UI shows every recipe with category filters, have/need counts per ingredient (red when short), a "can make" filter, scrolling,
// and a tooltip that says what the result does and why it cannot be made yet.
#include <algorithm>
#include "core/game.hpp"

const Spr& Game::itemIcon(int id) const {
    const ItemDef& D = item(id);
    switch (D.kind) {
        case IK_MATERIAL: return A.matIcon[D.ref];
        case IK_THROWN:
        case IK_LAUNCHER: return A.wpnIcon[D.ref];
        case IK_AMMO: return A.ammoIcon[D.ref];
        case IK_BACKPACK: return A.gearIcon[U_BAG][std::min(4, D.ref + 1)];  // tiers 1-4 of the bag art
        case IK_CARRIED:
            if (D.ref >= CG_ARMOR1 && D.ref <= CG_ARMOR8) return A.gearIcon[U_ARMOR][D.ref - CG_ARMOR1 + 1];
            if (D.ref >= CG_BOOTS1) return A.gearIcon[U_BOOTS][D.ref - CG_BOOTS1 + 1];
            return D.ref >= CG_STRAWHAT ? A.H.carryIcon[D.ref - CG_STRAWHAT] : A.carryIcon[D.ref];
        case IK_BABY: return A.babyIcon[D.ref];
        case IK_FARMBABY: return A.H.babyIcon[D.ref];
        case IK_TOOL: return A.R.toolIcon[D.ref];
        case IK_PLACEABLE: return A.R.placeIcon[D.ref];
        default: return D.ref == C_BANDAGE ? A.bandage : A.R.consIcon[D.ref];
    }
}

bool Game::hasIngredients(const Recipe& R) const {
    for (int i = 0; i < 3; i++)
        if (R.mat[i] >= 0 && p.mat(R.mat[i]) < R.n[i]) return false;
    return true;
}

std::string Game::craftBlock(int r) const {
    const Recipe& R = RECIPES[r];
    if (p.sk.level(R.skill) < R.minLv) return std::string("NEEDS ") + SKILL[R.skill].name + " " + S(R.minLv);
    if (!hasIngredients(R)) return "MISSING MATERIALS";
    if (!craftAnywhere && interior != recipeBuilding(R)) return std::string("MADE AT THE ") + buildingName(recipeBuilding(R));
    const int out = recipeItem(R);
    // space: ingredients go first, then the result must fit
    Inventory test = p.inv;
    for (int i = 0; i < 3; i++)
        if (R.mat[i] >= 0) test.remove(itemMat(R.mat[i]), R.n[i]);
    if (test.add(out, R.count) < R.count) return test.blockReason(out, R.count) == 2 ? "TOO HEAVY" : "PACK FULL";
    return "";
}

bool Game::craft(int r) {
    if (r < 0 || r >= RECIPE_N) return false;
    const std::string why = craftBlock(r);
    if (!why.empty()) {
        msg(why, C_RED);
        audio.play(S_ERROR);
        return false;
    }
    const Recipe& R = RECIPES[r];
    for (int i = 0; i < 3; i++)
        if (R.mat[i] >= 0) p.inv.remove(itemMat(R.mat[i]), R.n[i]);
    std::string made;
    // the quality of this craft: one roll, better odds with the trade's skill level; Normal when the better tier would not fit the pack
    int q = !recipeHasQuality(R) ? (int)Q_NORMAL : craftQuality >= 0 ? clampQuality(craftQuality) : rollQuality(p.sk.level(R.skill), rng.f());
    if (q > Q_NORMAL && p.inv.room(recipeItem(R), q) < R.count) q = Q_NORMAL;
    const std::string qtag = q > Q_NORMAL ? std::string(qualityDef(q).name) + " " : std::string();
    const int out = recipeItem(R);
    int got = p.inv.add(out, R.count, q);
    if (got < R.count) {  // cannot happen after craftBlock, but never lose anything if it does
        p.inv.removeQ(out, q, got);
        for (int i = 0; i < 3; i++)
            if (R.mat[i] >= 0) p.inv.add(itemMat(R.mat[i]), R.n[i]);
        msg("PACK FULL OR TOO HEAVY", C_RED);
        audio.play(S_ERROR);
        return false;
    }
    made = "CRAFTED " + S(R.count) + " " + qtag + item(out).name;
    // skill bonus: a practised crafter sometimes gets more out of a craft. Stackable results come out one extra; one-off
    // results (weapons, gear, structures) hand back one of the first ingredient instead. Never when it would not fit.
    if (rng.chance(skillBonus(R.skill))) {
        if (!itemIsGear(out) && item(out).stack > 1 && p.inv.add(out, 1, q) == 1) {
            ftext(p.pos + V2(0, -18), "EXTRA!", C_GREEN, 1.1f);
        } else if (itemIsGear(out) && R.n[0] >= 2 && p.inv.add(itemMat(R.mat[0]), 1) == 1) {
            ftext(p.pos + V2(0, -18), "SAVED A MATERIAL!", C_GREEN, 1.1f);
        }
    }
    audio.play(S_CRAFT);
    if (R.cat == RC_MATERIALS && R.station == STN_FIRE) audio.play(S_SMELT, 0.6f);
    msg(made, C_GOLD);
    ftext(p.pos + V2(0, -26), "+" + qtag + recipeName(R), q > Q_NORMAL ? qualityCol(q) : C_GOLD, 1.2f);
    grantXp(R.skill, recipeXp(R));  // only reached after the result was delivered and the ingredients were spent
    const bool isTool = R.kind == RK_ITEM && item(R.out).kind == IK_TOOL;
    const bool isArmor = R.kind == RK_ITEM && item(R.out).kind == IK_CARRIED && item(R.out).ref >= CG_ARMOR1;
    if (R.kind == RK_WEAPON || isTool || isArmor) questEvent(Q_CRAFT, R.kind == RK_WEAPON ? R.out : -1, 1);
    return true;
}

// Craft the same recipe up to n times in a row (stops quietly at the first block: no materials, no room, locked). Returns how many crafts were made.
int Game::craftMany(int r, int n) {
    if (r < 0 || r >= RECIPE_N) return 0;
    int made = 0;
    while (made < n && craftBlock(r).empty() && craft(r)) made++;
    if (made > 1) msg("CRAFTED " + S(made) + " TIMES", C_GOLD);
    return made;
}

// ----------------------------------------------------------------- the CRAFTING tab
namespace {
// what the result does, one short line each (for the tooltip)
void recipeInfo(const Recipe& R, std::vector<std::string>& lines) {
    switch (R.kind) {
        case RK_WEAPON: {
            const WpnDef& W = WPN[R.out];
            lines.push_back(W.blurb);
            lines.push_back("DMG " + S((int)W.dmg) + "  RANGE " + S((int)W.range) + (isLauncher(R.out) ? std::string("  FIRES ") + AMMO[W.ammo].name + "S" : std::string("  THROWN")));
            break;
        }
        case RK_AMMO: lines.push_back(std::string("AMMUNITION FOR THE ") + (R.out == AM_ARROW ? "BOWS" : "CROSSBOW")); break;
        case RK_BANDAGE: lines.push_back(item(ITEM_CONS0 + C_BANDAGE).blurb); break;
        default: {
            const ItemDef& D = item(R.out);
            lines.push_back(D.blurb);
            if (D.kind == IK_TOOL) {
                const ToolDef& T = TOOLDEF[D.ref];
                lines.push_back(std::string(T.type == TT_AXE ? "CHOPS" : T.type == TT_HOE ? "TILLS AND DIGS" : T.type == TT_SHOVEL ? "DIGS AND FILLS" : T.type == TT_CAN ? "WATERS CROPS" : T.type == TT_SICKLE ? "CUTS GRAIN" : "MINES") + " TIER " + S(T.tier) + "  SPEED X" + S((int)lroundf(T.speed * 10) / 10) + "." + S((int)lroundf(T.speed * 10) % 10) + "  BONUS +" + S((int)lroundf(T.bonus * 100)) + "%");
            }
            break;
        }
    }
}
}  // namespace

// The crafting panel of a station (smith, tanner, workshop), laid out like a crafting window: category tabs and a grid of recipe icons on the left,
// the ingredients with a CREATE button in the middle, and the description of the picked recipe on the right.
void Game::drawStationCraft(int X, int Y, int W, int H) {
    auto recipeIcon = [&](const Recipe& R, int cx, int cy, int size, float a = 1.f) {
        icon(itemIcon(recipeItem(R)), cx, cy, size, a);
    };
    // ---- category tabs and the "can make" switch
    int cx = X + 6;
    const int tabY = Y + 21;
    for (int c = -1; c < RC_COUNT; c++) {
        const std::string lab = c < 0 ? "ALL" : RECIPE_CAT_NAME[c];
        const int w = textW(lab) + 8;
        if (cx + w > X + W - 6) break;
        if (button(cx, tabY, w, 11, lab, true, craftCat == c ? HEX(0x8a6a3a) : HEX(0x4a3a2a))) {
            craftCat = c;
            craftScroll = 0;
        }
        cx += w + 2;
    }
    // ---- the recipes this station makes
    std::vector<int> list;
    for (int r = 0; r < RECIPE_N; r++) {
        if (craftBld >= 0 && recipeBuilding(RECIPES[r]) != craftBld) continue;
        if (craftCat >= 0 && RECIPES[r].cat != craftCat) continue;
        if (craftOnly && !craftBlock(r).empty()) continue;
        list.push_back(r);
    }
    if (std::find(list.begin(), list.end(), craftSel) == list.end()) craftSel = list.empty() ? -1 : list[0];
    // ---- the grid
    const int COLS = 4, CELL = 20, gx = X + 6, gy = Y + 37;
    const int rows = (Y + H - 18 - gy) / CELL, maxScroll = std::max(0, ((int)list.size() + COLS - 1) / COLS - rows);
    craftScroll = std::max(0, std::min(maxScroll, craftScroll));
    if (inRect(gx, gy, COLS * CELL, rows * CELL)) craftScroll = std::max(0, std::min(maxScroll, craftScroll - in.wheel));
    for (int i = 0; i < rows * COLS; i++) {
        const int k = craftScroll * COLS + i, x = gx + (i % COLS) * CELL, y = gy + (i / COLS) * CELL;
        const bool has = k < (int)list.size();
        const int r = has ? list[k] : -1;
        const bool over = has && inRect(x, y, CELL - 1, CELL - 1);
        panel(x, y, CELL - 1, CELL - 1, r == craftSel ? HEX(0x6a4a2a) : over ? HEX(0x4a3624) : HEX(0x33241a));
        if (!has) continue;
        if (r == craftSel) {
            fillRect(x, y + 1, 1, CELL - 3, HEX(0x3f9a2a));
            fillRect(x + CELL - 2, y + 1, 1, CELL - 3, HEX(0x3f9a2a));
            fillRect(x + 1, y, CELL - 3, 1, HEX(0x3f9a2a));
            fillRect(x + 1, y + CELL - 2, CELL - 3, 1, HEX(0x3f9a2a));
        }
        recipeIcon(RECIPES[r], x + (CELL - 1) / 2, y + (CELL - 1) / 2, 14, craftBlock(r).empty() ? 1.f : 0.4f);
        if (over && in.uiClick) {
            in.uiClick = false;
            in.lPressed = false;
            craftSel = r;
            audio.play(S_CLICK, 0.5f);
        }
    }
    if (maxScroll > 0) {  // scrollbar beside the grid
        const int sbx = gx + COLS * CELL, sbh = rows * CELL - 1, th = std::max(8, sbh * rows / (((int)list.size() + COLS - 1) / COLS));
        fillRect(sbx, gy, 3, sbh, HEX(0x6b665c));
        fillRect(sbx, gy + (sbh - th) * craftScroll / maxScroll, 3, th, HEX(0xeeeae0));
    }
    if (list.empty()) textC(gx + COLS * CELL / 2, gy + 20, craftOnly ? "NOTHING TO MAKE" : "NO RECIPES", C_DIM, 1, false);
    // ---- the picked recipe: ingredients and CREATE (middle), description (right)
    const int mx = gx + COLS * CELL + 8, mw = 96, dx = mx + mw + 6, dw = X + W - 6 - dx, top = gy, bottom = Y + H - 18;
    panel(mx, top, mw, bottom - top, HEX(0x3a2a1a));
    panel(dx, top, dw, bottom - top, HEX(0x3a2a1a));
    text(mx + 6, top + 5, "INGREDIENTS", C_DIM, 1, false);
    text(dx + 6, top + 5, "DESCRIPTION", C_DIM, 1, false);
    {
        if (button(mx + 6, bottom - 18, mw - 12, 12, "CAN MAKE ONLY", true, craftOnly ? HEX(0x5d7a2e) : HEX(0x4a3a2a))) {
            craftOnly = !craftOnly;
            craftScroll = 0;
        }
    }
    if (craftSel < 0) {
        textC(dx + dw / 2, top + 40, "PICK A RECIPE", C_DIM, 1, false);
        return;
    }
    const Recipe& R = RECIPES[craftSel];
    const std::string why = craftBlock(craftSel);
    const bool can = why.empty();
    for (int k = 0; k < 3; k++) {  // up to three ingredient slots: the material, and how much you have of what it takes
        const int sx = mx + 6 + k * 29, sy = top + 16;
        panel(sx, sy, 26, 26, HEX(0x33241a));
        if (R.mat[k] < 0) continue;
        icon(A.matIcon[R.mat[k]], sx + 13, sy + 11, 16);
        const std::string t = S(p.mat(R.mat[k])) + "/" + S(R.n[k]);
        textC(sx + 13, sy + 29, t, p.mat(R.mat[k]) >= R.n[k] ? C_GREEN : C_RED, 1, false);
    }
    panel(mx + 6, top + 50, 26, 26, HEX(0x33241a));  // what it makes
    recipeIcon(R, mx + 19, top + 63, 20);
    text(mx + 36, top + 54, recipeName(R) + (R.count > 1 ? " X" + S(R.count) : std::string("")), can ? C_GOLD : C_DIM, 1, false);
    text(mx + 36, top + 64, "OWNED " + S(p.inv.count(recipeItem(R))), C_DIM, 1, false);
    std::string lab = "CREATE";
    if (why.compare(0, 6, "NEEDS ") == 0) lab = "LOCKED";
    else if (why == "YOU WEAR BETTER ALREADY") lab = "OWNED";
    if (button(mx + 6, top + 82, mw - 12, 16, lab, can, HEX(0x5d7a2e))) craft(craftSel);
    if (button(mx + 6, top + 100, mw - 12, 12, "CREATE MAX", can, HEX(0x7a5d2e))) craftMany(craftSel, 999);  // everything the materials allow
    text(mx + 6, top + 116, std::string(SKILL[R.skill].name) + " LV " + S(R.minLv), p.sk.level(R.skill) >= R.minLv ? C_GREEN : C_RED, 1, false);
    // ---- description
    int ty = top + 16;
    auto wrap = [&](const std::string& txt, u32 col) {
        const size_t maxc = (size_t)std::max(8, (dw - 12) / 4);
        size_t pos = 0;
        while (pos < txt.size() && ty <= bottom - 8) {
            size_t len = std::min(maxc, txt.size() - pos);
            if (pos + len < txt.size()) {
                const size_t sp = txt.rfind(' ', pos + len);
                if (sp != std::string::npos && sp > pos) len = sp - pos;
            }
            text(dx + 6, ty, txt.substr(pos, len), col, 1, false);
            ty += 8;
            pos += len;
            while (pos < txt.size() && txt[pos] == ' ') pos++;
        }
        ty += 2;
    };
    std::vector<std::string> lines;
    recipeInfo(R, lines);
    for (size_t i = 0; i < lines.size(); i++) wrap(lines[i], i == 0 ? C_TEXT : C_DIM);
    wrap("EARNS " + S(recipeXp(R)) + " " + SKILL[R.skill].name + " XP", C_DIM);
    for (int k = 0; k < 3; k++)
        if (R.mat[k] >= 0 && p.mat(R.mat[k]) < R.n[k]) wrap("MISSING " + S(R.n[k] - p.mat(R.mat[k])) + " " + MAT_NAME[R.mat[k]], C_ORANGE);
    if (!can) wrap(why, C_RED);
}
