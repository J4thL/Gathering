// anim.hpp - small reusable animation core: clips, a state animator and 4-way (N, S, E, W) facing.
//
// A Clip is a list of frames (indices into whatever sprite strip the owner keeps) with a duration each; it loops or plays once.
// An Animator plays one clip at a time. play() with the state that is already playing does nothing, so calling it every frame
// never restarts an animation. A one-shot clip sets done() when it reaches its end (justDone() is true for exactly one update).
// scrub() lets gameplay drive a clip by progress instead of time (a tool swing that follows the harvest timer).
// Facing is one of 4 directions (S, E, N, W); west is the east art mirrored where the art is a flat sprite (see FacingView).
#pragma once
#include <algorithm>
#include <cmath>

constexpr int CLIP_MAX = 12;

struct Clip {
    int n = 0;
    int frame[CLIP_MAX] = {0};
    float dur[CLIP_MAX] = {0};
    bool loop = true;

    float total() const {
        float t = 0;
        for (int i = 0; i < n; i++) t += dur[i];
        return t;
    }
    // frames first .. first+count-1, each shown for `dt` seconds
    static Clip run(int first, int count, float dt, bool loop = true) {
        Clip c;
        c.n = std::min(count, CLIP_MAX);
        c.loop = loop;
        for (int i = 0; i < c.n; i++) {
            c.frame[i] = first + i;
            c.dur[i] = dt;
        }
        return c;
    }
    // an explicit sequence: frame indices and how long each one is shown
    static Clip seq(std::initializer_list<int> f, std::initializer_list<float> d, bool loop = true) {
        Clip c;
        c.loop = loop;
        auto fi = f.begin();
        auto di = d.begin();
        for (; fi != f.end() && di != d.end() && c.n < CLIP_MAX; ++fi, ++di) {
            c.frame[c.n] = *fi;
            c.dur[c.n] = *di;
            c.n++;
        }
        return c;
    }
};

struct Animator {
    const Clip* clip = nullptr;
    int state = -1;
    float t = 0;
    bool done = false;
    bool justDone = false;

    // Start `c` as state `st`. Does nothing while that state is already playing unless restart is set.
    void play(int st, const Clip* c, bool restart = false) {
        if (st == state && !restart) return;
        state = st;
        clip = c;
        t = 0;
        done = false;
        justDone = false;
    }
    void update(float dt) {
        justDone = false;
        if (!clip || clip->n == 0 || done) return;
        t += dt;
        const float tot = clip->total();
        if (tot <= 0) return;
        if (clip->loop) {
            t = fmodf(t, tot);
        } else if (t >= tot) {
            t = tot;
            done = true;
            justDone = true;
        }
    }
    // jump to a progress 0..1 through the clip (the clip is then driven by gameplay, not by the clock)
    void scrub(float frac) {
        if (!clip) return;
        const float tot = clip->total();
        t = std::max(0.f, std::min(0.999f, frac)) * tot;
    }
    // the frame index of the clip at the current time
    int frame() const {
        if (!clip || clip->n == 0) return 0;
        float acc = 0;
        for (int i = 0; i < clip->n; i++) {
            acc += clip->dur[i];
            if (t < acc) return clip->frame[i];
        }
        return clip->frame[clip->n - 1];
    }
};

// ---------------------------------------------------------------- 4-way facing
// Every living thing (the hunter, the animals, the monsters) faces one of four ways: south, east, north or west. A diagonal movement or
// aim turns the entity to face along the dominant axis. East and west share one drawn view (west is the mirror), or, for the rotated 3D
// models, are two renders of the model.
enum Facing { F_S, F_E, F_N, F_W, F_COUNT };
constexpr int FACING_VIEWS = 3;  // drawn views of a mirrored sprite: S, E, N  (west is the east view mirrored)

struct FacingView {
    int view;
    bool mirror;
};
inline FacingView facingView(int f) {
    static const FacingView V[F_COUNT] = {{0, false}, {1, false}, {2, false}, {1, true}};
    return V[(f % F_COUNT + F_COUNT) % F_COUNT];
}
// the unit vector a facing looks along (screen space, y grows downwards)
inline void facingVec(int f, float& dx, float& dy) {
    static const float X[F_COUNT] = {0, 1, 0, -1};
    static const float Y[F_COUNT] = {1, 0, -1, 0};
    f = (f % F_COUNT + F_COUNT) % F_COUNT;
    dx = X[f];
    dy = Y[f];
}
// The facing for a direction (dx, dy): the dominant axis wins (a perfect diagonal goes to the horizontal axis). `cur` is the facing so far:
// it is kept while its own axis is still dominant or nearly so, and it still points the way the direction goes, so a stick wobbling on the
// diagonal doesn't make the sprite flicker, and an entity never faces away from where it is heading.
inline int facingFromDir(float dx, float dy, int cur = -1) {
    if (fabsf(dx) < 1e-4f && fabsf(dy) < 1e-4f) return cur < 0 ? (int)F_S : cur;
    const float ax = fabsf(dx), ay = fabsf(dy);
    if (cur >= 0) {
        float fx, fy;
        facingVec(cur, fx, fy);
        const bool horiz = cur == F_E || cur == F_W;
        const bool stillDominant = horiz ? ay < ax * 1.25f : ax < ay * 1.25f;
        if (stillDominant && fx * dx + fy * dy > 0.f) return cur;
    }
    if (ax >= ay) return dx >= 0.f ? F_E : F_W;
    return dy >= 0.f ? F_S : F_N;
}
