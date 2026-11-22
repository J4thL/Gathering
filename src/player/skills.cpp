// skills.cpp - awarding skill XP and saving it (the SKILLS tab is the skill tree, core/game_tree.cpp).
// Skill data and the XP curve are in skills.hpp. Callers award XP only after an action has really completed.
#include <algorithm>
#include <cstring>
#include "core/game.hpp"

int Game::grantXp(int skill, int amount) {
    if (skill < 0 || skill >= SK_COUNT || amount <= 0 || p.dead) return 0;
    amount = std::max(1, (int)lroundf(amount * (1.f + p.sk.perkLearn(skill))));  // the QUICK STUDY perks
    const int before = p.sk.level(skill), rankBefore = p.sk.rank();
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
        if (p.sk.rank() > rankBefore) {
            p.syncPack();
            msg(std::string("HUNTER RANK: ") + RANK_NAME[p.sk.rank()] + " - CARRY +" + S(RANK_CARRY_KG) + " KG", C_GOLD);
        }
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
