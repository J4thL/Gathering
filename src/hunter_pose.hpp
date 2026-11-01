// hunter_pose.hpp - the hunter's animation states: which poses each state has and how its clip plays them.
//
// mkPlayerPose() (sprites.hpp) draws one hunter from a view (S, E, N plus the SE / NE three-quarter views) and an HPose.
// A state is a list of poses; its Clip says in what order and for how long they are shown. Gameplay picks the state
// (updateHunterAnim in game_logic.cpp) and the renderer shows A.hunter[view][state][pose].
#pragma once
#include "anim.hpp"

struct HPose {
    int bob = 0;                // body rise in px (negative = crouching)
    int liftL = 0, liftR = 0;   // front / back views: how far each foot is lifted
    int aL = 0, aR = 0;         // front / back views: vertical offset of the left / right arm (negative = raised)
    int sa = 7, sb = 8;         // side view: x of the back and the front leg
    int slA = 0, slB = 0;       // side view: lift of the back / front foot
    int sw = 0, sdy = 0;        // side view: forward and vertical offset of the arm
    int lean = 0;               // side view: head and body shift forward (negative = back)
    int sway = 0;               // hair swing, -1..1
    bool blink = false;         // eyes shut
};

enum HunterState { HS_IDLE, HS_WALK, HS_RUN, HS_AIM, HS_THROW, HS_GATHER, HS_SWING, HS_USE, HS_HURT, HS_DEATH, HS_COUNT };
constexpr int HS_MAXF = 6;  // most poses any state has

inline HPose walkPose(int f) {  // the original four-frame walk
    HPose p;
    p.bob = (f == 1 || f == 3) ? 1 : 0;
    p.liftL = f == 3 ? 2 : 0;
    p.liftR = f == 1 ? 2 : 0;
    p.aL = f == 1 ? -1 : (f == 3 ? 1 : 0);
    p.aR = -p.aL;
    p.sa = (f == 0 || f == 2) ? 7 : (f == 1 ? 5 : 10);
    p.sb = (f == 0 || f == 2) ? 8 : (f == 1 ? 10 : 5);
    p.sw = f == 1 ? 2 : (f == 3 ? -2 : 0);
    p.sway = f == 1 ? 1 : (f == 3 ? -1 : 0);
    return p;
}

inline int hunterPoseCount(int st) {
    static const int N[HS_COUNT] = {3, 4, 6, 2, 2, 4, 4, 3, 2, 2};
    return N[st];
}

inline HPose hunterPose(int st, int i) {
    HPose p;
    switch (st) {
        case HS_IDLE:  // 0 rest, 1 breathe in, 2 blink
            if (i == 1) p.bob = 1;
            if (i == 2) p.blink = true;
            break;
        case HS_WALK: p = walkPose(i); break;
        case HS_RUN: {  // six poses: reach, push, fly, reach (other leg), push, fly - bigger stride, arms pumping, body leaning in
            static const int BOB[6] = {0, 1, 2, 0, 1, 2};
            p.bob = BOB[i];
            p.lean = 1;
            const bool b = i >= 3;
            const int ph = i % 3;
            p.sa = ph == 0 ? (b ? 11 : 4) : ph == 1 ? (b ? 9 : 6) : 8;
            p.sb = ph == 0 ? (b ? 4 : 11) : ph == 1 ? (b ? 6 : 9) : 8;
            p.slA = ph == 2 ? 2 : (b ? 1 : 0);
            p.slB = ph == 2 ? 2 : (b ? 0 : 1);
            if (ph == 1) (b ? p.slA : p.slB) = 2;
            p.liftL = ph == 2 ? 2 : (b ? (ph == 1 ? 3 : 1) : 0);
            p.liftR = ph == 2 ? 2 : (b ? 0 : (ph == 1 ? 3 : 1));
            p.aL = ph == 2 ? 0 : (b ? 2 : -2) / (ph == 1 ? 2 : 1);
            p.aR = -p.aL;
            p.sw = ph == 2 ? 0 : (b ? -3 : 3) / (ph == 1 ? 3 : 1);
            p.sway = b ? -1 : 1;
            break;
        }
        case HS_AIM:  // winding up a throw: arm drawn back, body leaning away from the target
            p.aR = i == 0 ? -3 : -6;
            p.sw = i == 0 ? -1 : -2;
            p.sdy = i == 0 ? -3 : -5;
            p.lean = i == 0 ? 0 : -1;
            break;
        case HS_THROW:  // release and follow-through
            p.aR = i == 0 ? 2 : 3;
            p.sw = i == 0 ? 4 : 3;
            p.sdy = i == 0 ? -2 : 1;
            p.lean = 1;
            p.bob = i == 0 ? 0 : -1;
            break;
        case HS_GATHER: {  // kneeling to skin or pick: the hands work back and forth
            static const int AL[4] = {2, 1, 0, 1}, AR[4] = {0, 1, 2, 1}, SW[4] = {2, 3, 2, 1};
            p.bob = -2;
            p.aL = AL[i];
            p.aR = AR[i];
            p.sw = SW[i];
            p.sdy = 2 + (i & 1);
            p.lean = 1;
            break;
        }
        case HS_SWING: {  // a tool stroke: slow raise, fast strike (the clip's timing follows the harvest timer)
            static const int AR[4] = {-5, -6, -1, 3}, BOB[4] = {0, 1, -1, -2}, SW[4] = {-2, -2, 2, 3}, SDY[4] = {-5, -6, -2, 1}, LN[4] = {-1, -1, 1, 1};
            p.aR = AR[i];
            p.bob = BOB[i];
            p.sw = SW[i];
            p.sdy = SDY[i];
            p.lean = LN[i];
            break;
        }
        case HS_USE: {  // a hand to the chest or mouth: bandage, food, tonic
            static const int AR[3] = {-3, -5, -2};
            p.aR = AR[i];
            p.sw = 1;
            p.sdy = AR[i];
            break;
        }
        case HS_HURT:  // flinch, then a recoil
            p.blink = true;
            p.bob = i == 0 ? -1 : 0;
            p.aL = p.aR = i == 0 ? -3 : -1;
            p.sw = i == 0 ? -2 : -1;
            p.sdy = i == 0 ? -3 : -1;
            p.lean = i == 0 ? -2 : -1;
            break;
        case HS_DEATH:  // stagger, then sink to the knees (the renderer lays the last pose down)
            p.blink = true;
            p.bob = i == 0 ? -1 : -3;
            p.aL = p.aR = i == 0 ? -3 : 2;
            p.sw = i == 0 ? -2 : 1;
            p.sdy = i == 0 ? -3 : 2;
            p.lean = -2;
            break;
        default: break;
    }
    return p;
}

inline const Clip& hunterClip(int st) {
    static const Clip C[HS_COUNT] = {
        Clip::seq({0, 1, 0, 1, 0, 2, 0}, {0.6f, 0.6f, 0.6f, 0.6f, 0.5f, 0.12f, 0.4f}, true),  // idle: breathing with a blink
        Clip::run(0, 4, 0.118f, true),                                                          // walk
        Clip::run(0, 6, 0.07f, true),                                                           // run
        Clip::run(0, 2, 0.2f, false),                                                           // aim (scrubbed by the charge)
        Clip::seq({0, 1}, {0.12f, 0.22f}, false),                                               // throw
        Clip::run(0, 4, 0.11f, true),                                                           // gather
        Clip::seq({0, 1, 2, 3}, {0.35f, 0.35f, 0.15f, 0.15f}, false),                           // swing (scrubbed by the harvest timer)
        Clip::seq({0, 1, 1, 2}, {0.12f, 0.18f, 0.12f, 0.12f}, false),                           // use an item
        Clip::seq({0, 1}, {0.14f, 0.18f}, false),                                               // hurt
        Clip::seq({0, 1}, {0.16f, 0.16f}, false),                                               // death
    };
    return C[st];
}
