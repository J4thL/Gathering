// anim.hpp - small reusable animation core: clips, a state animator and 8-way facing.
//
// A Clip is a list of frames (indices into whatever sprite strip the owner keeps) with a duration each; it loops or plays once.
// An Animator plays one clip at a time. play() with the state that is already playing does nothing, so calling it every frame
// never restarts an animation. A one-shot clip sets done() when it reaches its end (justDone() is true for exactly one update).
// scrub() lets gameplay drive a clip by progress instead of time (a tool swing that follows the harvest timer).
// Facing is one of 8 directions; the art draws 5 of them and mirrors the rest (see FacingView).
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

// ---------------------------------------------------------------- 8-way facing
enum Facing { F_S, F_SE, F_E, F_NE, F_N, F_NW, F_W, F_SW, F_COUNT };
constexpr int FACING_VIEWS = 5;  // drawn views: S, SE, E, NE, N  (the west-side facings are mirrors)

struct FacingView {
    int view;
    bool mirror;
};
inline FacingView facingView(int f) {
    static const FacingView V[F_COUNT] = {{0, false}, {1, false}, {2, false}, {3, false}, {4, false}, {3, true}, {2, true}, {1, true}};
    return V[(f % F_COUNT + F_COUNT) % F_COUNT];
}
// 8-way facing for a direction (dx, dy; y grows downwards). `cur` is the facing so far: it is kept until the direction has
// clearly moved into another sector, so a stick wobbling on a sector border doesn't make the sprite flicker.
inline int facingFromDir(float dx, float dy, int cur = -1) {
    if (fabsf(dx) < 1e-4f && fabsf(dy) < 1e-4f) return cur < 0 ? (int)F_S : cur;
    const float a = atan2f(dx, dy) * 57.29578f;  // 0 = south, +90 = east, 180 = north, -90 = west
    if (cur >= 0) {
        float d = fabsf(a - cur * 45.f);
        d = fmodf(d, 360.f);
        if (d > 180.f) d = 360.f - d;
        if (d < 22.5f + 6.f) return cur;
    }
    int s = (int)floorf(a / 45.f + 0.5f);
    return ((s % 8) + 8) % 8;
}
// the unit vector a facing looks along (screen space)
inline void facingVec(int f, float& dx, float& dy) {
    static const float X[F_COUNT] = {0, 0.7071f, 1, 0.7071f, 0, -0.7071f, -1, -0.7071f};
    static const float Y[F_COUNT] = {1, 0.7071f, 0, -0.7071f, -1, -0.7071f, 0, 0.7071f};
    f = (f % F_COUNT + F_COUNT) % F_COUNT;
    dx = X[f];
    dy = Y[f];
}
