// crafting.cpp - the crafting rules and the pack's CRAFTING tab.
//
// Rules (craftBlock / craft): a recipe needs its ingredients, the right station, a bag with room for the result (ingredients are used up
// first, so a recipe that frees a slot works in a full bag), and - for single-copy things like tools, launchers and gear - not already
// owning the thing. Nothing is consumed unless the result is delivered.
// The UI shows every recipe with category filters, have/need counts per ingredient (red when short), a "can make" filter, scrolling,
// and a tooltip that says what the result does and why it cannot be made yet.
#include <algorithm>
#include "game.hpp"

const Spr& Game::itemIcon(int id) const {
    const ItemDef& D = item(id);
    switch (D.kind) {
        case IK_MATERIAL: return A.matIcon[D.ref];
        case IK_THROWN:
        case IK_LAUNCHER: return A.wpnIcon[D.ref];
        case IK_AMMO: return A.ammoIcon[D.ref];
        case IK_BACKPACK: return A.gearIcon[U_BAG][std::min(4, D.ref + 1)];  // tiers 1-4 of the bag art
        case IK_CARRIED: return A.carryIcon[D.ref];
        case IK_TOOL: return A.R.toolIcon[D.ref];
        case IK_PLACEABLE: return A.R.placeIcon[D.ref];
        default: return D.ref == C_BANDAGE ? A.bandage : A.R.consIcon[D.ref];
    }
}

bool Game::nearStation(int station) const {
    switch (station) {
        default: return interior >= 0;  // the building's own forge, racks, stove and benches provide every station
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
    if (R.kind == RK_GEAR) {
        if (p.up[R.out] >= R.count) return "YOU WEAR BETTER ALREADY";
        if (p.up[R.out] < R.count - 1) return "MAKE THE PREVIOUS LEVEL FIRST";
        return "";
    }
    const int out = recipeItem(R);
    if ((R.kind == RK_ITEM || R.kind == RK_WEAPON) && item(out).stack == 1 && p.inv.count(out) > 0) return "YOU ALREADY HAVE ONE";
    if (R.kind == RK_ITEM && item(out).kind == IK_BACKPACK && p.bag == item(out).ref) return "YOU ALREADY WEAR ONE";
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
    if (R.kind == RK_GEAR) {
        p.up[R.out] = R.count;
        if (R.out == U_ARMOR) {  // the extra health arrives at once (updatePlayer tops up current HP by the difference)
            p.maxhp = std::max(p.maxhp, (float)ARMOR_HP[p.up[U_ARMOR]]);
        }
        made = "NOW WEARING " + recipeName(R);
    } else {
        const int out = recipeItem(R);
        int got = p.inv.add(out, R.count);
        if (got < R.count) {  // cannot happen after craftBlock, but never lose anything if it does
            p.inv.remove(out, got);
            for (int i = 0; i < 3; i++)
                if (R.mat[i] >= 0) p.inv.add(itemMat(R.mat[i]), R.n[i]);
            msg("PACK FULL OR TOO HEAVY", C_RED);
            audio.play(S_ERROR);
            return false;
        }
        made = "CRAFTED " + S(R.count) + " " + item(out).name;
    }
    // skill bonus: a practised crafter sometimes gets more out of a craft. Stackable results come out one extra; one-off
    // results (weapons, gear, structures) hand back one of the first ingredient instead. Never when it would not fit.
    if (R.kind != RK_GEAR && rng.chance(skillBonus(R.skill))) {
        const int out = recipeItem(R);
        if (item(out).stack > 1 && p.inv.add(out, 1) == 1) {
            ftext(p.pos + V2(0, -18), "EXTRA!", C_GREEN, 1.1f);
        } else if (item(out).stack == 1 && R.n[0] >= 2 && p.inv.add(itemMat(R.mat[0]), 1) == 1) {
            ftext(p.pos + V2(0, -18), "SAVED A MATERIAL!", C_GREEN, 1.1f);
        }
    }
    audio.play(S_CRAFT);
    if (R.cat == RC_MATERIALS && R.station == STN_FIRE) audio.play(S_SMELT, 0.6f);
    msg(made, C_GOLD);
    ftext(p.pos + V2(0, -26), "+" + recipeName(R), C_GOLD, 1.2f);
    grantXp(R.skill, recipeXp(R));  // only reached after the result was delivered and the ingredients were spent
    const bool isTool = R.kind == RK_ITEM && item(R.out).kind == IK_TOOL;
    if (R.kind == RK_WEAPON || isTool || R.kind == RK_GEAR) questEvent(Q_CRAFT, R.kind == RK_WEAPON ? R.out : -1, 1);
    return true;
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
        case RK_GEAR: {
            lines.push_back(gearDesc(R.out, R.count));
            break;
        }
        case RK_BANDAGE: lines.push_back(item(ITEM_CONS0 + C_BANDAGE).blurb); break;
        default: {
            const ItemDef& D = item(R.out);
            lines.push_back(D.blurb);
            if (D.kind == IK_TOOL) {
                const ToolDef& T = TOOLDEF[D.ref];
                lines.push_back(std::string(T.type == TT_AXE ? "CHOPS" : "MINES") + " TIER " + S(T.tier) + "  SPEED X" + S((int)lroundf(T.speed * 10) / 10) + "." + S((int)lroundf(T.speed * 10) % 10) + "  BONUS +" + S((int)lroundf(T.bonus * 100)) + "%");
            }
            break;
        }
    }
}
}  // namespace

void Game::drawCraftTab(int X, int Y, int W, int H) {
    const int cy = Y + 38;
    // ---- filter chips and the "can make" switch
    int cx = X + 8;
    for (int c = -1; c < RC_COUNT; c++) {
        std::string lab = c < 0 ? "ALL" : RECIPE_CAT_NAME[c];
        int w = textW(lab) + 10;
        if (button(cx, cy, w, 11, lab, true, craftCat == c ? HEX(0x8a6a3a) : HEX(0x4a3a2a))) {
            craftCat = c;
            craftScroll = 0;
        }
        cx += w + 2;
    }
    {
        int w = textW("CAN MAKE") + 10;
        if (button(X + W - 8 - w, cy, w, 11, "CAN MAKE", true, craftOnly ? HEX(0x5d7a2e) : HEX(0x4a3a2a))) {
            craftOnly = !craftOnly;
            craftScroll = 0;
        }
    }
    // ---- the visible list
    std::vector<int> list;
    for (int r = 0; r < RECIPE_N; r++) {
        if (craftBld >= 0 && recipeBuilding(RECIPES[r]) != craftBld) continue;
        if (craftCat >= 0 && RECIPES[r].cat != craftCat) continue;
        if (craftOnly && !craftBlock(r).empty()) continue;
        list.push_back(r);
    }
    const int ry0 = cy + 14, rowH = 22, rows = (Y + H - 16 - ry0) / rowH;
    const int maxScroll = std::max(0, (int)list.size() - rows);
    craftScroll = std::max(0, std::min(maxScroll, craftScroll));
    const int x = X + 8, w = W - 16 - 5;
    int hover = -1;
    if (list.empty()) {
        textC(X + W / 2, ry0 + 30, craftOnly ? "NOTHING CAN BE MADE RIGHT NOW" : "NO RECIPES", C_DIM);
        if (craftOnly) textC(X + W / 2, ry0 + 42, "GATHER MORE MATERIALS OR SWITCH CAN MAKE OFF", C_DIM, 1, false);
    }
    for (int i = 0; i < rows && craftScroll + i < (int)list.size(); i++) {
        const int r = list[craftScroll + i];
        const Recipe& R = RECIPES[r];
        const int y = ry0 + i * rowH;
        const std::string why = craftBlock(r);
        const bool can = why.empty();
        panel(x, y, w, rowH - 1, can ? HEX(0x3f3220) : HEX(0x33261a));
        if (inRect(x, y, w, rowH - 1)) hover = r;
        if (R.kind == RK_GEAR) icon(A.gearIcon[R.out][std::min(5, R.count)], x + 11, y + 10, 16);
        else icon(itemIcon(recipeItem(R)), x + 11, y + 10, 16);
        text(x + 24, y + 3, recipeName(R) + (R.count > 1 && R.kind != RK_GEAR ? "  X" + S(R.count) : std::string("")), can ? C_GOLD : HEX(0xa89060));
        if (R.kind != RK_GEAR) {
            std::string ow = "OWNED " + S(p.inv.count(recipeItem(R)));
            text(x + w - 58 - textW(ow), y + 3, ow, C_DIM, 1, false);
        }
        int ix = x + 24;
        for (int k = 0; k < 3; k++)
            if (R.mat[k] >= 0) ix = chip(ix, y + 11, A.matIcon[R.mat[k]], p.mat(R.mat[k]), R.n[k]);
        if (craftBld < 0) {  // the pack's book: say where each one is made
            const char* sn = recipeBuilding(R) == BLD_SMITH ? "SMITH" : recipeBuilding(R) == BLD_TANNER ? "LEATHER" : "WORKSHOP";
            text(ix, y + 14, sn, interior == recipeBuilding(R) ? C_GREEN : C_ORANGE, 1, false);
            ix += textW(sn) + 5;
        }
        if (R.minLv > 1) text(ix, y + 14, std::string(SKILL[R.skill].name) + " " + S(R.minLv), p.sk.level(R.skill) >= R.minLv ? C_GREEN : C_RED, 1, false);
        std::string lab = "CRAFT";
        if (why.compare(0, 6, "NEEDS ") == 0) lab = "LOCKED";
        else if (!can && why.compare(0, 8, "MADE AT ") == 0) lab = "IN TOWN";
        else if (why == "YOU ALREADY HAVE ONE" || why == "YOU WEAR BETTER ALREADY") lab = "OWNED";
        if (button(x + w - 54, y + 4, 50, 13, lab, can, HEX(0x5d7a2e))) craft(r);
    }
    // ---- scrollbar
    if (maxScroll > 0) {
        const int sbx = X + W - 9, sby = ry0, sbh = rows * rowH - 1;
        fillRect(sbx, sby, 3, sbh, HEX(0x241a10));
        int th = std::max(8, sbh * rows / (int)list.size());
        int ty = sby + (sbh - th) * craftScroll / maxScroll;
        fillRect(sbx, ty, 3, th, HEX(0x92693a));
    }
    // ---- tooltip
    if (hover >= 0) {
        const Recipe& R = RECIPES[hover];
        std::vector<std::string> lines;
        recipeInfo(R, lines);
        lines.push_back(std::string("MADE AT THE ") + buildingName(recipeBuilding(R)) + "   " + SKILL[R.skill].name + " LV " + S(R.minLv) + " (YOU " + S(p.sk.level(R.skill)) + ")");
        lines.push_back(std::string("EARNS ") + S(recipeXp(R)) + " " + SKILL[R.skill].name + " XP");
        const std::string why = craftBlock(hover);
        std::vector<std::string> missing;  // exactly what is short, one line each
        for (int k = 0; k < 3; k++)
            if (R.mat[k] >= 0 && p.mat(R.mat[k]) < R.n[k]) missing.push_back("MISSING " + S(R.n[k] - p.mat(R.mat[k])) + " " + MAT_NAME[R.mat[k]]);
        int tw = textW(recipeName(R)) + 6;
        for (auto& l : lines) tw = std::max(tw, textW(l) + 6);
        for (auto& l : missing) tw = std::max(tw, textW(l) + 6);
        if (!why.empty()) tw = std::max(tw, textW(why) + 6);
        int th = 12 + (int)(lines.size() + missing.size()) * 9 + (why.empty() ? 0 : 9);
        int tx = std::min((int)in.mx + 10, LW - tw - 2), ty = std::min((int)in.my + 10, LH - th - 2);
        panel(tx, ty, tw, th, HEX(0x241a10));
        text(tx + 3, ty + 3, recipeName(R), C_GOLD);
        int yy = ty + 12;
        for (size_t i = 0; i < lines.size(); i++, yy += 9) text(tx + 3, yy, lines[i], i + 2 >= lines.size() ? C_DIM : C_TEXT, 1, false);
        for (auto& l : missing) text(tx + 3, yy, l, C_ORANGE, 1, false), yy += 9;
        if (!why.empty()) text(tx + 3, yy, why, C_RED, 1, false);
    }
    if (craftBld < 0) textC(X + W / 2, Y + H - 8, "CRAFTING IS DONE INSIDE THE TOWN BUILDINGS   WHEEL SCROLLS   TAB BACK", C_DIM, 1, false);
}
