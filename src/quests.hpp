// quests.hpp - quest data: a short story chain, then endless scaling bounties.
// Quests are pure data. Progress is tracked by Game (questIdx / questActive / questProg).
#pragma once
#include <algorithm>
#include <string>
#include "data.hpp"

enum QuestType { Q_KILL, Q_GATHER, Q_CRAFT, Q_EARN };

struct Quest {
    std::string title, desc;
    int type = Q_KILL;
    int target = -1;  // animal kind (Q_KILL) or material (Q_GATHER); -1 = anything
    int goal = 1;
    int coins = 0;
    int mat = -1, matN = 0;  // optional material reward
    int bandages = 0;        // optional bandage reward
};

constexpr int QUEST_CHAIN_N = 8;  // story quests before the endless bounties begin

inline Quest makeQuest(int type, int target, int goal, const std::string& title, const std::string& desc, int coins,
                       int mat = -1, int matN = 0, int bandages = 0) {
    Quest q;
    q.type = type;
    q.target = target;
    q.goal = goal;
    q.title = title;
    q.desc = desc;
    q.coins = coins;
    q.mat = mat;
    q.matN = matN;
    q.bandages = bandages;
    return q;
}

// Quest number idx (0-based). The same idx always gives the same quest, so saving just the index works.
inline Quest questAt(int idx) {
    using std::to_string;
    switch (idx) {
        case 0: return makeQuest(Q_KILL, A_RABBIT, 3, "FIRST HUNT", "HUNT 3 RABBITS NEAR THE VILLAGE", 25);
        case 1: return makeQuest(Q_GATHER, M_MEAT, 5, "FRESH MEAT", "GATHER 5 RAW MEAT FROM CARCASSES", 30, -1, 0, 1);
        case 2: return makeQuest(Q_KILL, A_DEER, 1, "DEER TRACKS", "HUNT A DEER - THROW FROM RANGE", 40, M_BONE, 2);
        case 3: return makeQuest(Q_GATHER, M_HIDE, 4, "TANNER'S ORDER", "GATHER 4 HIDES FOR THE TANNER", 45, M_BONE, 2);
        case 4: return makeQuest(Q_CRAFT, -1, 1, "BETTER BLADES", "CRAFT A WEAPON OR A TOOL", 50, -1, 0, 1);
        case 5: return makeQuest(Q_KILL, A_BOAR, 1, "TUSKED TERROR", "HUNT A BOAR - IT FIGHTS BACK", 80, -1, 0, 2);
        case 6: return makeQuest(Q_EARN, -1, 100, "MARKET DAY", "SELL GOODS WORTH 100 COINS", 60, M_FUR, 1);
        case 7: return makeQuest(Q_KILL, A_WOLF, 1, "WOLF HUNTER", "HUNT A WOLF - THEY ROAM AT NIGHT", 150, M_FUR, 2);
        default: break;
    }
    // endless bounties: four kinds in rotation, a little harder every lap
    int i = std::max(0, idx - QUEST_CHAIN_N), tier = i / 4;
    switch (i % 4) {
        case 0: {
            int g = std::min(12, 4 + tier * 2);
            return makeQuest(Q_KILL, A_RABBIT, g, "BOUNTY: RABBITS", "HUNT " + to_string(g) + " RABBITS", 20 + g * 6);
        }
        case 1: {
            int g = std::min(6, 2 + tier);
            return makeQuest(Q_KILL, A_DEER, g, "BOUNTY: DEER", "HUNT " + to_string(g) + " DEER", 30 + g * 20);
        }
        case 2: {
            int g = std::min(14, 4 + tier * 2);
            return makeQuest(Q_GATHER, M_HIDE, g, "BOUNTY: HIDES", "GATHER " + to_string(g) + " HIDES", 25 + g * 8);
        }
        default: {
            bool wolf = (tier % 2) == 1;
            int g = std::min(4, 1 + tier / 2);
            return makeQuest(Q_KILL, wolf ? A_WOLF : A_BOAR, g, wolf ? "BOUNTY: WOLVES" : "BOUNTY: BOARS",
                             "HUNT " + to_string(g) + (wolf ? " WOLVES" : " BOARS"), (wolf ? 70 : 60) + g * 40, -1, 0, 1);
        }
    }
}
