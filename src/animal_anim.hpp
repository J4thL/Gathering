// animal_anim.hpp - animation states and clips for animals that have a full animation set.
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

inline const AnimalClips& boarClipSet() {
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

// Clips for the species that have a full animation set; nullptr for the others (they keep the four-frame walk cycle).
inline const AnimalClips* animalClips(int kind) { return kind == A_BOAR ? &boarClipSet() : nullptr; }
