// skills.cpp - awarding skill XP, saving it, and the pack's SKILLS tab.
// Skill data and the XP curve are in skills.hpp. Callers award XP only after an action has really completed.
#include <algorithm>
#include <cstring>
#include "game.hpp"

int Game::grantXp(int skill, int amount) {
    if (skill < 0 || skill >= SK_COUNT || amount <= 0 || p.dead) return 0;
    const int before = p.sk.level(skill);
    const int gained = p.sk.add(skill, amount);
    if (before >= MAX_LEVEL) return 0;  // capped: no toast spam
    ftext(p.pos + V2(0, -34), std::string("+") + S(amount) + " " + SKILL[skill].name, HEX(0x9fd4ff), 0.9f);
    if (gained > 0) {
        const int now = before + gained;
        msg(std::string(SKILL[skill].name) + " LEVEL " + S(now) + "!", C_GOLD);
        ftext(p.pos + V2(0, -44), "LEVEL UP!", C_GOLD, 1.4f);
        audio.play(S_CRAFT);
        for (int m : MILESTONES)
            if (before < m && now >= m) msg(std::string(SKILL[skill].name) + " MILESTONE " + S(m) + ": +" + S((int)lroundf(skillBonusAt(skill, now) * 100.f)) + "% " + SKILL[skill].effect, C_GREEN);
    }
    return gained;
}

void Game::skillSave(std::ostream& f) const {
    for (int k = 0; k < SK_COUNT; k++)
        if (p.sk.s[k].xp > 0) f << "skill " << SKILL[k].key << ' ' << (long long)p.sk.s[k].xp << "\n";
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
        if (inRect(x0, y, W - 16, rowH)) hover = k;
        if (hover == k) fillRect(x0 - 2, y - 1, W - 12, rowH, HEX(0x3f3220));
        text(x0, y, SKILL[k].name, lv > 1 || p.sk.s[k].xp > 0 ? C_GOLD : HEX(0xa89060));
        const std::string lvs = "LV " + S(lv);
        text(x0 + 82, y, lvs, C_TEXT);
        const int bx = x0 + 112, bw = 120;
        const float f = lv >= MAX_LEVEL ? 1.f : (float)p.sk.s[k].into() / (float)std::max<int64_t>(1, xpForLevel(lv));
        bar(bx, y + 1, bw, 5, f, HEX(0x4aa3d8));
        const std::string xs = lv >= MAX_LEVEL ? std::string("MAX") : S((int)p.sk.s[k].into()) + "/" + S((int)xpForLevel(lv));
        text(bx + bw + 6, y, xs, C_DIM, 1, false);
    }
    std::string foot = "DO THINGS TO LEVEL THEM UP   TAB BACK";
    if (hover >= 0) {
        const int lv = p.sk.level(hover), nm = nextMilestone(lv);
        const int pct = (int)lroundf(skillBonusAt(hover, lv) * 100.f);
        foot = std::string(SKILL[hover].name) + ": +" + S(pct) + "% " + SKILL[hover].effect + "  " + (nm ? "NEXT STEP AT LV " + S(nm) : std::string("ALL STEPS REACHED"));
    }
    textC(X + W / 2, Y + H - 8, foot, C_DIM, 1, false);
}
