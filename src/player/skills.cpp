// skills.cpp - awarding skill XP, saving it, and the pack's SKILLS tab.
// Skill data and the XP curve are in skills.hpp. Callers award XP only after an action has really completed.
#include <algorithm>
#include <cstring>
#include "core/game.hpp"

int Game::grantXp(int skill, int amount) {
    if (skill < 0 || skill >= SK_COUNT || amount <= 0 || p.dead) return 0;
    amount = std::max(1, (int)lroundf(amount * (1.f + p.sk.perkLearn(skill))));  // the QUICK STUDY perks
    const int before = p.sk.level(skill);
    const int gained = p.sk.add(skill, amount);
    if (before >= MAX_LEVEL) return 0;  // capped: no toast spam
    int live = 0;  // XP numbers still rising: stack the new one above them so two skills gained at once do not overprint
    for (auto& f : ftexts)
        if (f.col == HEX(0x9fd4ff) && f.t < 0.7f) live++;
    ftext(p.pos + V2(0, -34 - 9 * std::min(live, 3)), std::string("+") + S(amount) + " " + SKILL[skill].name, HEX(0x9fd4ff), 0.9f);
    if (gained > 0) {
        const int now = before + gained;
        msg(std::string(SKILL[skill].name) + " LEVEL " + S(now) + "!", C_GOLD);
        ftext(p.pos + V2(0, -44), "LEVEL UP!", C_GOLD, 1.4f);
        audio.play(S_CRAFT);
        for (int m : MILESTONES)
            if (before < m && now >= m) msg(std::string(SKILL[skill].name) + " MILESTONE " + S(m) + ": +" + S((int)lroundf(skillBonusAt(skill, now) * 100.f)) + "% " + SKILL[skill].effect + " - PICK A PERK IN SKILLS", C_GREEN);
    }
    return gained;
}

void Game::skillSave(std::ostream& f) const {
    for (int k = 0; k < SK_COUNT; k++)
        if (p.sk.s[k].xp > 0) f << "skill " << SKILL[k].key << ' ' << (long long)p.sk.s[k].xp << "\n";
    for (int k = 0; k < SK_COUNT; k++)
        for (int t = 0; t < PERK_TIERS; t++)
            if (p.sk.perk[k][t] >= 0) f << "perk " << SKILL[k].key << ' ' << t << ' ' << (int)p.sk.perk[k][t] << "\n";
}

void Game::perkLoadLine(std::istringstream& ss) {  // "perk <key> <step> <0|1>"; anything out of range is ignored
    std::string key;
    int t = -1, c = -1;
    if (!(ss >> key >> t >> c) || t < 0 || t >= PERK_TIERS || (c != 0 && c != 1)) return;
    for (int k = 0; k < SK_COUNT; k++)
        if (key == SKILL[k].key) {
            p.sk.perk[k][t] = (int8_t)c;
            return;
        }
}

void Game::skillLoadLine(std::istringstream& ss) {
    std::string key;
    long long xp = 0;
    if (!(ss >> key >> xp)) return;
    for (int k = 0; k < SK_COUNT; k++)
        if (key == SKILL[k].key) {
            p.sk.s[k].xp = std::max(0LL, std::min((long long)SkillState::capXp(), xp));
            return;
        }
    // an unknown key comes from a newer build: ignored, the rest of the save still loads
}

void Game::drawSkillsTab(int X, int Y, int W, int H) {
    const int rowH = 9, x0 = X + 8, cy = Y + 38;
    int hover = -1;
    for (int k = 0; k < SK_COUNT; k++) {
        const int y = cy + k * rowH;
        const int lv = p.sk.level(k);
        if (skillSel < 0 && inRect(x0, y, W - 16, rowH)) hover = k;
        if (hover == k) fillRect(x0 - 2, y - 1, W - 12, rowH, skinFill(HEX(0x3f3220)));
        text(x0, y, SKILL[k].name, lv > 1 || p.sk.s[k].xp > 0 ? C_GOLD : HEX(0xa89060));
        const std::string lvs = "LV " + S(lv);
        text(x0 + 82, y, lvs, C_TEXT);
        if (p.sk.pending(k) > 0) text(x0 + 70, y, "!", C_GOLD);  // a perk waits for a choice
        const int bx = x0 + 112, bw = 120;
        const float f = lv >= MAX_LEVEL ? 1.f : (float)p.sk.s[k].into() / (float)std::max<int64_t>(1, xpForLevel(lv));
        bar(bx, y + 1, bw, 5, f, HEX(0x4aa3d8));
        const std::string xs = lv >= MAX_LEVEL ? std::string("MAX") : S((int)p.sk.s[k].into()) + "/" + S((int)xpForLevel(lv));
        text(bx + bw + 6, y, xs, C_DIM, 1, false);
    }
    if (hover >= 0 && in.uiClick) {  // click a skill: its perk choices open
        in.uiClick = false;
        in.lPressed = false;
        skillSel = hover;
        audio.play(S_CLICK, 0.5f);
    }
    std::string foot = "CLICK A SKILL FOR ITS PERKS   TAB BACK";
    if (hover >= 0) {
        const int lv = p.sk.level(hover), nm = nextMilestone(lv);
        const int pct = (int)lroundf(skillBonusAt(hover, lv) * 100.f);
        foot = std::string(SKILL[hover].name) + ": +" + S(pct) + "% " + SKILL[hover].effect + "  " + (nm ? "NEXT STEP AT LV " + S(nm) : std::string("ALL STEPS REACHED"));
    }
    textC(X + W / 2, Y + H - 8, foot, C_DIM, 1, false);
    if (skillSel < 0) return;
    // ---- the perk choices of one skill: a pair to pick from at each milestone level, each pick final
    const int k = skillSel, ox = X + 14, oy = Y + 30, ow = W - 28, oh = 132, bw = (ow - 62) / 2;
    panel(ox, oy, ow, oh, HEX(0x3a2a1a));
    text(ox + 8, oy + 6, std::string(SKILL[k].name) + " PERKS", C_GOLD, 1, false);
    text(ox + ow - 84, oy + 6, "LV " + S(p.sk.level(k)) + "   " + (p.sk.pending(k) ? "PICK ONE" : ""), C_DIM, 1, false);
    auto clip = [](std::string s, size_t n) { return s.size() > n ? s.substr(0, n) : s; };
    for (int t = 0; t < PERK_TIERS; t++) {
        const int y = oy + 18 + t * 26;
        const bool open = p.sk.canChoose(k, t), reached = p.sk.level(k) >= MILESTONES[t];
        text(ox + 8, y + 3, "LV " + S(MILESTONES[t]), reached ? C_TEXT : C_DIM, 1, false);
        for (int c = 0; c < 2; c++) {
            const int bx = ox + 52 + c * (bw + 4);
            const bool bSpeed = perkBIsSpeed(k);
            const std::string nm = c == 0 ? PERK_NAME_A[t] : (bSpeed ? PERK_NAME_SPEED[t] : PERK_NAME_LEARN[t]);
            std::string ds;
            if (c == 0) ds = "+" + S((int)lroundf(PERK_MASTERY_SHARE[t] * SKILL[k].cap * 100.f)) + "% " + SKILL[k].effect;
            else ds = "+" + S((int)lroundf(PERK_B_VALUE[t] * 100.f)) + "% " + (bSpeed ? (k == SK_WOODCUTTING ? "CHOPPING SPEED" : "MINING SPEED") : "XP IN THIS SKILL");
            const bool mine = p.sk.perk[k][t] == c;
            if (button(bx, y, bw, 11, nm, open, mine ? HEX(0x5d7a2e) : HEX(0x4a3a2a)) && p.sk.choose(k, t, c)) {
                msg(std::string(SKILL[k].name) + ": " + nm + " (" + ds + ")", C_GREEN);
                audio.play(S_CRAFT);
            }
            text(bx + 1, y + 13, clip(ds, (size_t)std::max(8, bw / 4)), mine ? C_GREEN : C_DIM, 1, false);
        }
    }
    if (button(ox + ow - 40, oy + oh - 14, 34, 11, "BACK", true, HEX(0x4a3a2a))) skillSel = -1;
}
