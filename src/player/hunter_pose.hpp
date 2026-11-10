// hunter_pose.hpp - the hunter's animation states: which poses each state has and how its clip plays them.
//
// mkPlayerPose() (sprites.hpp) draws one hunter from a view (S, E, N plus the SE / NE three-quarter views) and an HPose.
// A state is a list of poses; its Clip says in what order and for how long they are shown. Gameplay picks the state
// (updateHunterAnim in game_logic.cpp) and the renderer shows A.hunter[view][state][pose].
#pragma once
#include "core/anim.hpp"

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

// HS_SWING is the axe stroke (chopping), HS_MINE the pickaxe stroke, HS_FORAGE picking plants by hand and HS_GATHER kneeling at a carcass
// (skinning). SWING, MINE and FORAGE play raise, top, strike, impact (the frame the resource takes damage) and then a recovery
// pose; the first four follow the harvest timer (see hunterSwingSpan), the recovery shows right after each hit.
enum HunterState { HS_IDLE, HS_WALK, HS_RUN, HS_AIM, HS_THROW, HS_GATHER, HS_SWING, HS_USE, HS_HURT, HS_DEATH, HS_MINE, HS_FORAGE, HS_COUNT };
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
    static const int N[HS_COUNT] = {3, 4, 6, 2, 2, 4, 5, 3, 2, 2, 5, 5};
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
        case HS_SWING: {  // an axe stroke: slow raise, fast strike, impact, then a recovery pose (the clip's timing follows the harvest timer)
            static const int AR[5] = {-5, -6, -1, 3, 1}, BOB[5] = {0, 1, -1, -2, -1}, SW[5] = {-2, -2, 2, 3, 1}, SDY[5] = {-5, -6, -2, 1, 0}, LN[5] = {-1, -1, 1, 1, 0};
            p.aR = AR[i];
            p.bob = BOB[i];
            p.sw = SW[i];
            p.sdy = SDY[i];
            p.lean = LN[i];
            break;
        }
        case HS_MINE: {  // a pickaxe: heavier than the axe - a high two-handed lift, a long fall, a hard stop on the rock
            static const int AR[5] = {-6, -8, 0, 4, 2}, BOB[5] = {0, 1, -1, -2, -1}, SW[5] = {-2, -3, 3, 4, 2}, SDY[5] = {-6, -8, -1, 2, 0}, LN[5] = {-1, -2, 1, 2, 1};
            p.aR = AR[i];
            p.aL = AR[i] / 2;
            p.bob = BOB[i];
            p.sw = SW[i];
            p.sdy = SDY[i];
            p.lean = LN[i];
            break;
        }
        case HS_FORAGE: {  // picking a plant: bend down, reach, pluck, gather it in, straighten up
            static const int BOB[5] = {-2, -3, -3, -3, -1}, SW[5] = {1, 3, 4, 2, 1}, SDY[5] = {1, 3, 4, 2, -1}, AR[5] = {1, 2, 3, 1, -1}, LN[5] = {1, 2, 2, 1, 0};
            p.bob = BOB[i];
            p.sw = SW[i];
            p.sdy = SDY[i];
            p.aR = AR[i];
            p.aL = 1;
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
        Clip::seq({0, 1, 2, 3, 4}, {0.3f, 0.25f, 0.15f, 0.1f, 0.25f}, false),                   // axe: raise, top, strike, impact, recover
        Clip::seq({0, 1, 1, 2}, {0.12f, 0.18f, 0.12f, 0.12f}, false),                           // use an item
        Clip::seq({0, 1}, {0.14f, 0.18f}, false),                                               // hurt
        Clip::seq({0, 1}, {0.16f, 0.16f}, false),                                               // death
        Clip::seq({0, 1, 2, 3, 4}, {0.3f, 0.3f, 0.15f, 0.1f, 0.25f}, false),                    // mine: lift, top, strike, impact, recover
        Clip::seq({0, 1, 2, 3, 4}, {0.25f, 0.25f, 0.2f, 0.15f, 0.25f}, false),                  // forage: bend, reach, pluck, collect, rise
    };
    return C[st];
}

// The harvest timer drives the first four poses of SWING / MINE / FORAGE: progress 0..1 of a swing maps onto this share of the clip.
// (The last pose, the recovery, is shown on its own for a moment after each hit lands.)
inline float hunterSwingSpan(int st) {
    const Clip& c = hunterClip(st);
    if (c.n < 2) return 1.f;
    return (c.total() - c.dur[c.n - 1]) / c.total();
}
// A farm tool or seeds used on the soil: the hoe and the sickle play the axe stroke, seeds the bend of picking a plant, and the
// watering can the hand-raising use pose (the can itself is drawn tipping). One short swing per click.
constexpr float FARM_SWING_T = 0.45f;
inline bool isToolStroke(int st) { return st == HS_SWING || st == HS_MINE || st == HS_FORAGE; }
