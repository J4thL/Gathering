// animal_anim.hpp - animation states and clips for animals that have a full animation set (boar, wolf, deer, rabbit, desert hare, snow hare so far).
//
// A species opts in by returning clips from animalClips(kind) and a painter in sprites_boar.hpp style (see buildRichAnimals).
// Every other species keeps the older four-frame walk cycle. The states follow what the animal's AI is doing:
//   idle (standing, grazing), walk (wandering), run (fleeing or charging), attack (the moment a bite lands),
//   hurt (a hit lands), death (one-shot, holds on its last pose as the carcass).
#pragma once
#include "anim.hpp"

enum AnimalState { AN_IDLE, AN_WALK, AN_RUN, AN_ATTACK, AN_HURT, AN_DEATH, AN_COUNT };
constexpr int AN_MAXF = 6;  // most poses any state has

struct AnimalClips {
    int poses[AN_COUNT];
    Clip clip[AN_COUNT];
};

inline const AnimalClips& quadClipSet() {
    static const AnimalClips C = {
        {4, 6, 5, 5, 4, 6},
        {
            Clip::run(0, 4, 0.24f, true),                                                       // idle: breathing, an ear flick
            Clip::run(0, 6, 0.1f, true),                                                        // walk (played at the pace of the animal's speed)
            Clip::run(0, 5, 0.075f, true),                                                      // run
            Clip::seq({0, 1, 2, 3, 4}, {0.07f, 0.06f, 0.06f, 0.09f, 0.1f}, false),             // attack: rear back, lunge, tusk sweep, recover
            Clip::run(0, 4, 0.085f, false),                                                     // hurt: a red flinch
            Clip::seq({0, 1, 2, 3, 4, 5}, {0.1f, 0.1f, 0.12f, 0.14f, 0.16f, 0.2f}, false),    // death: stagger, fall, lie still
        }};
    return C;
}

// Hoppers (rabbit, hares): idle is sitting up with a nose twitch and a nibble, walk is a small hop, run a long bounding leap.
inline const AnimalClips& hopClipSet() {
    static const AnimalClips C = {
        {4, 6, 6, 1, 4, 6},
        {
            Clip::seq({0, 1, 0, 2, 3, 2, 3, 0}, {0.7f, 0.18f, 0.5f, 0.22f, 0.18f, 0.22f, 0.18f, 0.5f}, true),  // idle: sit, twitch, nibble
            Clip::run(0, 6, 0.085f, true),                                                                       // walk: crouch, push off, rise, peak, fall, land
            Clip::run(0, 6, 0.065f, true),                                                                       // run: the same hop, longer and flatter
            Clip::run(0, 1, 0.1f, false),                                                                        // attack: unused (nothing hopping bites)
            Clip::run(0, 4, 0.085f, false),                                                                      // hurt
            Clip::seq({0, 1, 2, 3, 4, 5}, {0.1f, 0.1f, 0.12f, 0.14f, 0.16f, 0.2f}, false),                     // death
        }};
    return C;
}

// every species that has a full animation set
constexpr int RICH_KINDS[] = {A_BOAR, A_WOLF, A_DEER, A_RABBIT, A_HARE, A_SNOWHARE};

// Clips for the species that have a full animation set; nullptr for the others (they keep the four-frame walk cycle).
inline const AnimalClips* animalClips(int kind) {
    switch (kind) {
        case A_BOAR: case A_WOLF: case A_DEER: return &quadClipSet();
        case A_RABBIT: case A_HARE: case A_SNOWHARE: return &hopClipSet();
        default: return nullptr;
    }
}
