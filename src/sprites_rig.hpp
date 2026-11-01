// sprites_rig.hpp - boar, wolf and deer, drawn from one tiny 3D rig so every facing and every animation pose comes from one description.
//
// An animal is a handful of ellipsoids (torso, haunch, head, snout, ears), strokes (legs, tusks, tail) and bristle spikes in its own
// coordinates: x forward, y to its left, z up. A pose moves those parts (head dip, lunge, leg swing, a roll onto its side), the view
// turns the whole thing by a yaw angle and projects it from a camera looking down at about 40 degrees, and the parts are painted
// back to front with three-tone shading lit from the upper left. That gives the S, SE, E, NE and N views for free (the west views are
// mirrors) and keeps the art consistent between views. A RigShape describes one species (proportions, neck, ears, tail, tusks, antlers,
// gait); rarity tiers reuse the species palette and scale from fauSpec(). Adding a quadruped = a RigShape + a line in rigShape().
// Style reference: a tiny chestnut boar with a dark bristle mane, ivory tusks, a red hurt flash and a slash arc on the attack.
#pragma once
#include <algorithm>
#include <memory>
#include "animal_anim.hpp"

constexpr int AV_VIEWS = 5;  // S, SE, E, NE, N  (same views as the hunter, see anim.hpp)

struct RichAnimal {
    Spr s[8][AV_VIEWS][AN_COUNT][AN_MAXF];  // [form + 2*tier][view][state][pose]
};

// ---------------------------------------------------------------- pose
struct BPose {
    float bob = 0;            // body rise
    float headDx = 0, headDz = 0;  // head forward / up
    float bodyDx = 0;         // whole body shifted forward (a lunge)
    float phase = 0;          // walk cycle angle
    float stride = 0, lift = 0;    // how far / how high the feet swing
    float air = 0;            // all feet off the ground by this much
    float breathe = 0;        // torso swell
    float tail = 0;           // tail flick
    float ear = 0;            // ear flick
    float tuskUp = 0;         // tusks swept upward
    float tint = 0;           // 0..1 red flash
    float roll = 0;           // rolled onto its side (radians, 1.57 = lying)
    float limp = 0;           // legs hanging loose (death)
    float slash = -1;         // attack arc progress 0..1, -1 = none
    float squash = 0;         // torso flattened by this much
    float pitch = 0;          // nose up (+) / down (-): lifts the shoulders and head, lowers the haunch
    float hind = 0, fore = 0; // hoppers: how far the hind legs are stretched back / the forelegs reached forward
};

inline BPose quadPose(int st, int i) {
    BPose p;
    switch (st) {
        case AN_IDLE: {
            static const float HZ[4] = {0.f, 0.4f, 0.f, -0.35f};
            p.breathe = sinf(i * 1.5708f) * 0.35f;
            p.headDz = HZ[i];
            p.headDx = (i == 1) ? 0.3f : 0.f;
            p.ear = (i == 2) ? 1.f : 0.f;
            p.tail = (i & 1) ? 1.f : 0.f;
            break;
        }
        case AN_WALK:
            p.phase = i * 6.28318f / 6.f;
            p.stride = 2.7f;
            p.lift = 1.7f;
            p.bob = 0.45f * cosf(p.phase * 2.f);
            p.headDz = 0.3f * cosf(p.phase * 2.f);
            p.tail = (i & 1) ? 1.f : 0.f;
            break;
        case AN_RUN: {
            static const float BOB[5] = {0.3f, 0.9f, 1.6f, 0.9f, 0.3f};
            p.phase = i * 6.28318f / 5.f;
            p.stride = 4.6f;
            p.lift = 2.8f;
            p.bob = BOB[i];
            p.headDx = 1.2f;
            p.headDz = -0.9f;
            p.bodyDx = 0.6f;
            p.air = (i == 2) ? 1.2f : 0.f;
            p.tail = 1.f;
            break;
        }
        case AN_ATTACK: {
            static const float HDX[5] = {-1.4f, -0.6f, 2.6f, 3.6f, 1.4f}, HDZ[5] = {1.4f, 0.2f, -2.0f, -2.6f, -0.8f}, BDX[5] = {-1.f, -0.6f, 1.6f, 2.4f, 0.8f},
                               TU[5] = {0.f, 0.f, 0.4f, 1.f, 0.4f};
            p.headDx = HDX[i];
            p.headDz = HDZ[i];
            p.bodyDx = BDX[i];
            p.tuskUp = TU[i];
            p.squash = (i == 1) ? 0.5f : 0.f;
            p.stride = (i >= 2) ? 1.8f : 0.f;
            p.slash = i >= 3 ? (i - 3) * 0.5f : -1.f;
            break;
        }
        case AN_HURT: {
            static const float TN[4] = {0.45f, 0.9f, 1.0f, 0.5f}, BD[4] = {-1.2f, -1.8f, -1.f, -0.2f}, HZ[4] = {1.f, 1.6f, 0.9f, 0.2f};
            p.tint = TN[i];
            p.bodyDx = BD[i];
            p.headDz = HZ[i];
            p.squash = (i == 1) ? 0.4f : 0.f;
            p.tuskUp = 0.3f;
            break;
        }
        case AN_DEATH: {
            static const float TN[6] = {0.6f, 0.8f, 0.6f, 0.3f, 0.12f, 0.f}, RL[6] = {0.f, 0.45f, 1.05f, 1.4f, 1.57f, 1.57f};
            p.tint = TN[i];
            p.roll = RL[i];
            p.headDz = (i == 0) ? 1.2f : -0.6f;
            p.bodyDx = (i == 0) ? -1.f : 0.f;
            p.limp = i >= 2 ? 1.f : (i == 1 ? 0.5f : 0.f);
            p.stride = i == 0 ? 1.5f : 0.f;
            break;
        }
        default: break;
    }
    return p;
}

// Poses for the hoppers: a hop is crouch, push off (legs stretched), rise, peak, fall, land. `air` is the height of the hop.
inline BPose hopPose(int st, int i) {
    BPose p;
    switch (st) {
        case AN_IDLE: {  // sitting up on the haunches: 0 sit, 1 nose twitch, 2 head down nibbling, 3 head up chewing
            p.pitch = 0.7f;
            p.squash = 0.3f;
            p.breathe = (i == 1) ? 0.25f : 0.f;
            p.ear = (i == 1) ? 1.f : 0.f;
            p.headDz = i == 2 ? -2.2f : i == 3 ? -0.9f : 0.f;
            p.headDx = i == 2 ? 1.2f : i == 3 ? 0.5f : 0.f;
            if (i == 2) p.pitch = -0.2f;
            if (i == 3) p.pitch = 0.2f;
            p.fore = 0.f;
            p.hind = 0.f;
            break;
        }
        case AN_WALK: {
            static const float AIR[6] = {0.f, 0.6f, 1.6f, 2.2f, 1.2f, 0.2f}, HIND[6] = {0.f, 1.f, 0.9f, 0.6f, 0.3f, 0.1f},
                               FORE[6] = {0.5f, 1.f, 0.9f, 0.6f, 0.3f, -0.2f}, PITCH[6] = {-0.2f, 0.8f, 0.5f, 0.f, -0.5f, -0.4f},
                               SQ[6] = {0.4f, 0.f, 0.f, 0.f, 0.f, 0.3f};
            p.air = AIR[i]; p.hind = HIND[i]; p.fore = FORE[i]; p.pitch = PITCH[i]; p.squash = SQ[i];
            p.ear = i >= 3 ? 0.f : 0.f;
            p.tail = (i == 5) ? 1.f : 0.f;
            break;
        }
        case AN_RUN: {
            static const float AIR[6] = {0.f, 1.4f, 3.2f, 4.2f, 2.6f, 0.4f}, HIND[6] = {0.f, 1.2f, 1.3f, 1.2f, 0.8f, 0.2f},
                               FORE[6] = {0.3f, 1.2f, 1.2f, 1.f, 0.8f, -0.1f}, PITCH[6] = {-0.3f, 0.6f, 0.2f, -0.1f, -0.5f, -0.4f},
                               SQ[6] = {0.6f, 0.f, 0.f, 0.f, 0.f, 0.4f};
            p.air = AIR[i]; p.hind = HIND[i]; p.fore = FORE[i]; p.pitch = PITCH[i]; p.squash = SQ[i];
            p.headDx = 0.6f;
            p.tail = 1.f;
            break;
        }
        case AN_HURT: {
            static const float TN[4] = {0.45f, 0.9f, 1.0f, 0.5f}, BD[4] = {-0.8f, -1.2f, -0.6f, 0.f}, AIR[4] = {0.6f, 1.2f, 0.5f, 0.f};
            p.tint = TN[i];
            p.bodyDx = BD[i];
            p.air = AIR[i];
            p.pitch = 0.5f;
            p.hind = 0.8f;
            p.fore = 0.6f;
            p.squash = (i == 3) ? 0.3f : 0.f;
            break;
        }
        case AN_DEATH: {
            static const float TN[6] = {0.6f, 0.8f, 0.6f, 0.3f, 0.12f, 0.f}, RL[6] = {0.f, 0.45f, 1.05f, 1.4f, 1.57f, 1.57f};
            p.tint = TN[i];
            p.roll = RL[i];
            p.air = i == 0 ? 0.8f : 0.f;
            p.limp = i >= 2 ? 1.f : (i == 1 ? 0.5f : 0.f);
            p.hind = i >= 2 ? 0.9f : 0.5f;
            p.fore = 0.3f;
            break;
        }
        default: break;  // attack: unused
    }
    return p;
}

// ---------------------------------------------------------------- the rig
struct RPart {
    int type;                 // 0 ellipsoid, 1 spike, 2 stroke (two points), 3 dot
    float x, y, z;            // centre / first point (boar coordinates)
    float rx, ry, rz;         // ellipsoid radii, or the second point for a stroke (x2, y2, z2); spike: rz = height
    u32 c0, c1, c2;           // light, mid, dark (stroke: c0 colour, c1 the shaded side; dot: c0)
    int w;                    // stroke width in px
    float sx, sy, sd;         // projected centre and depth (filled by the painter)
};

inline u32 tintC(u32 c, float t) { return t <= 0 ? c : mixc(c, HEX(0xe03a30), std::min(1.f, t)); }

// ---------------------------------------------------------------- species shapes
struct RigShape {
    float tx, ty, tz;          // torso radii
    float zb;                  // height of the torso centre above the feet
    float legFx, legBx, legY;  // hip x of the front / back legs, and the half-width between left and right
    int legW;                  // leg thickness in px (1 = a thin line)
    float hdx, hdz;            // head centre relative to the front of the torso (x) and the torso centre (z)
    float hrx, hry, hrz;       // head radii
    float snx, snz, snr;       // snout: forward offset from the head, drop, radius scale
    int ear;                   // 0 small round (boar), 1 pointed upright (wolf), 2 leaf-shaped to the side (deer)
    float earH;
    float neck;                // > 0: the neck is drawn as a thick tapered stroke from the shoulders to the head
    int tail;                  // 0 short curl, 1 bushy, 2 short flag
    bool tusks, mane, ridge, antlers, spots;
    bool hopper;               // hops on folded hind legs (rabbit, hares): own leg model, ear 3 (tall upright), tail 3 (cotton ball)
    float strideMul, liftMul, airMul;
    int W, H;                  // canvas at scale 1
};

inline RigShape boarShape() {
    RigShape r{};
    r.tx = 9; r.ty = 5.2f; r.tz = 5.f; r.zb = 7.6f;
    r.legFx = 5.2f; r.legBx = -5.f; r.legY = 3.4f; r.legW = 2;
    r.hdx = 0.f; r.hdz = -1.3f; r.hrx = 4.2f; r.hry = 3.7f; r.hrz = 3.6f;
    r.snx = 3.8f; r.snz = -1.7f; r.snr = 1.f;
    r.ear = 0; r.earH = 1.9f; r.neck = 0; r.tail = 0;
    r.tusks = true; r.mane = true;
    r.strideMul = r.liftMul = r.airMul = 1.f;
    r.W = 46; r.H = 36;
    return r;
}
inline RigShape wolfShape() {
    RigShape r{};
    r.tx = 8.6f; r.ty = 3.9f; r.tz = 4.2f; r.zb = 11.f;
    r.legFx = 5.4f; r.legBx = -5.2f; r.legY = 2.5f; r.legW = 2;
    r.hdx = 0.6f; r.hdz = 0.9f; r.hrx = 3.6f; r.hry = 3.1f; r.hrz = 3.1f;
    r.snx = 3.9f; r.snz = -0.9f; r.snr = 0.85f;
    r.ear = 1; r.earH = 2.6f; r.neck = 0; r.tail = 1;
    r.ridge = true;
    r.strideMul = 1.25f; r.liftMul = 1.2f; r.airMul = 1.2f;
    r.W = 52; r.H = 42;
    return r;
}
inline RigShape deerShape(bool stag) {
    RigShape r{};
    r.tx = 8.3f; r.ty = 3.5f; r.tz = 4.f; r.zb = 14.4f;
    r.legFx = 5.4f; r.legBx = -5.2f; r.legY = 2.3f; r.legW = 1;
    r.hdx = 3.2f; r.hdz = 6.4f; r.hrx = 3.3f; r.hry = 2.5f; r.hrz = 2.7f;
    r.snx = 3.2f; r.snz = -0.9f; r.snr = 0.8f;
    r.ear = 2; r.earH = 2.1f; r.neck = 2.3f; r.tail = 2;
    r.antlers = stag; r.spots = !stag;
    r.strideMul = 1.35f; r.liftMul = 1.5f; r.airMul = 1.7f;
    r.W = 54; r.H = 58;
    return r;
}
inline RigShape rabbitShape() {
    RigShape r{};
    r.tx = 4.8f; r.ty = 3.2f; r.tz = 3.4f; r.zb = 4.6f;
    r.legFx = 3.2f; r.legBx = -2.6f; r.legY = 1.7f; r.legW = 1;
    r.hdx = 1.2f; r.hdz = 1.5f; r.hrx = 2.7f; r.hry = 2.3f; r.hrz = 2.4f;
    r.snx = 2.1f; r.snz = -0.6f; r.snr = 0.55f;
    r.ear = 3; r.earH = 4.4f; r.neck = 0; r.tail = 3;
    r.hopper = true;
    r.strideMul = r.liftMul = r.airMul = 1.f;
    r.W = 30; r.H = 30;
    return r;
}
inline RigShape hareShape() {  // leaner, longer in the leg and ear
    RigShape r = rabbitShape();
    r.tx = 5.6f; r.ty = 3.1f; r.tz = 3.5f; r.zb = 5.4f;
    r.legFx = 3.8f; r.legBx = -3.0f;
    r.hdx = 1.5f; r.hdz = 1.7f; r.hrx = 2.9f; r.hry = 2.2f; r.hrz = 2.5f;
    r.earH = 5.6f;
    r.airMul = 1.25f;
    r.W = 34; r.H = 34;
    return r;
}
inline RigShape rigShape(int kind, int form) {
    switch (kind) {
        case A_WOLF: return wolfShape();
        case A_DEER: return deerShape(form == 1);
        case A_RABBIT: return rabbitShape();
        case A_HARE: case A_SNOWHARE: return hareShape();
        default: return boarShape();
    }
}

// Paint one animal for one view (0 S, 1 SE, 2 E, 3 NE, 4 N) and pose. `k` is the tier's body scale.
inline Img paintQuad(const QuadSpec& sp, const RigShape& R, float k, int view, const BPose& P0) {
    BPose P = P0;
    P.stride *= R.strideMul;
    P.lift *= R.liftMul;
    P.air *= R.airMul;
    static const float PSI[AV_VIEWS] = {1.5708f, 0.7854f, 0.f, -0.7854f, -1.5708f};
    const float psi = PSI[view], cp = cosf(psi), sn = sinf(psi);
    const float phi = 0.70f, cph = cosf(phi), sph = sinf(phi);  // the camera looks down at 40 degrees
    const int CW = (int)ceilf(R.W * k), CH = (int)ceilf(R.H * k);
    const float ax = CW / 2.f, ay = CH - 6.f * k;                // where the ground point under the boar lands
    Img im(CW, CH);

    const u32 body = tintC(sp.body, P.tint), light = tintC(sp.light, P.tint), belly = tintC(sp.belly, P.tint),
              dk = tintC(mixc(sp.body, sp.dark, 0.7f), P.tint), hoof = tintC(sp.hoof, P.tint), snout = tintC(sp.snout, P.tint);
    const u32 mane = tintC(mixc(sp.dark, HEX(0x1c2a3c), 0.55f), P.tint * 0.5f), maneL = tintC(mixc(sp.dark, HEX(0x4a6078), 0.55f), P.tint * 0.5f);
    const u32 ivory = HEX(0xf2ead2), ivoryD = HEX(0xc8bea0);

    std::vector<RPart> parts;
    auto ell = [&](float x, float y, float z, float rx, float ry, float rz, u32 c0, u32 c1, u32 c2) {
        parts.push_back({0, x * k, y * k, z * k, rx * k, ry * k, rz * k, c0, c1, c2, 0, 0, 0, 0});
    };
    auto spike = [&](float x, float y, float z, float h, u32 c0, u32 c1) { parts.push_back({1, x * k, y * k, z * k, 0, 0, h * k, c0, c1, 0, 0, 0, 0, 0}); };
    auto stroke = [&](float x, float y, float z, float x2, float y2, float z2, u32 c0, u32 c1, int w) {
        parts.push_back({2, x * k, y * k, z * k, x2 * k, y2 * k, z2 * k, c0, c1, 0, w, 0, 0, 0});
    };
    auto dot = [&](float x, float y, float z, u32 c) { parts.push_back({3, x * k, y * k, z * k, 0, 0, 0, c, 0, 0, 0, 0, 0, 0}); };

    // ---- the body (animal coordinates, k = 1)
    const float zb = R.zb + P.bob + P.air;
    const float bx = P.bodyDx;
    const float hx = bx + R.tx * 0.95f + R.hdx + P.headDx;          // head centre x
    const float hz = zb + R.hdz + P.headDz + P.pitch * 1.3f;
    const float sqz = P.squash;
    const float pz = P.pitch;
    ell(bx, 0, zb, R.tx, R.ty, R.tz - sqz + P.breathe, light, body, dk);                                              // torso
    ell(bx - R.tx * 0.51f, 0, zb - 0.5f - pz * 0.8f, R.tx * (R.hopper ? 0.8f : 0.62f), R.ty * (R.hopper ? 1.15f : 0.96f), R.tz * 0.92f - sqz, light, body, dk);  // haunch
    ell(bx + R.tx * 0.36f, 0, zb + 0.7f + pz * 1.0f, R.tx * 0.58f, R.ty * 0.92f, R.tz - sqz + P.breathe, light, body, dk);  // shoulder
    ell(bx, 0, zb - R.tz * 0.52f, R.tx * 0.82f, R.ty * 0.8f, R.tz * 0.4f, belly, belly, dk);                          // belly
    if (R.neck > 0) {  // a long neck: two overlapping blobs from the shoulders up to the head
        const float sx0 = bx + R.tx * 0.6f, sz0 = zb + R.tz * 0.5f;
        for (int i = 1; i <= 3; i++) {
            const float t = i / 4.f;
            ell(sx0 + (hx - 1.2f - sx0) * t, 0, sz0 + (hz - sz0) * t, R.neck * (1.15f - 0.35f * t), R.neck * (1.05f - 0.3f * t), R.neck * 1.3f, light, body, dk);
        }
    }
    ell(hx, 0, hz, R.hrx, R.hry, R.hrz, light, body, dk);                                                              // head
    ell(hx + R.snx, 0, hz + R.snz, 2.9f * R.snr, 2.4f * R.snr, 2.1f * R.snr, tintC(mixc(sp.snout, sp.body, 0.15f), P.tint), snout, dk);  // snout
    dot(hx + R.snx + 2.6f * R.snr, 0.9f * R.snr, hz + R.snz + 0.2f, dk);                                               // nostrils
    dot(hx + R.snx + 2.6f * R.snr, -0.9f * R.snr, hz + R.snz + 0.2f, dk);
    const float earUp = P.ear * 0.9f;
    if (R.ear == 3) {  // long upright ears: they lean back when running and lie flat after a hit
        const float lean = (P.stride > 3.f || P.air > 2.5f) ? -1.6f : 0.f, up = P.ear * 0.7f;
        for (int sd = -1; sd <= 1; sd += 2) {
            ell(hx - 0.8f + lean, sd * 1.5f, hz + R.hrz + R.earH * 0.5f + up, 0.9f, 0.7f, R.earH * 0.62f, body, body, dk);
            dot(hx - 0.2f + lean, sd * 1.5f, hz + R.hrz + R.earH * 0.5f + up, belly);  // the pale inside of the ear
        }
    } else if (R.ear == 1) {  // upright pointed ears
        ell(hx - 0.8f, 2.0f, hz + R.hrz + 0.6f + earUp, 1.0f, 0.9f, R.earH, body, body, dk);
        ell(hx - 0.8f, -2.0f, hz + R.hrz + 0.6f - earUp * 0.4f, 1.0f, 0.9f, R.earH, body, body, dk);
    } else if (R.ear == 2) {  // leaf ears held out to the sides
        ell(hx - 1.0f, 2.9f, hz + R.hrz - 0.2f + earUp, 1.0f, 1.9f, 1.3f, body, body, dk);
        ell(hx - 1.0f, -2.9f, hz + R.hrz - 0.2f - earUp * 0.4f, 1.0f, 1.9f, 1.3f, body, body, dk);
    } else {
        ell(hx - 0.4f, 2.9f, hz + 3.0f + earUp, 1.3f, 1.0f, R.earH, body, body, dk);
        ell(hx - 0.4f, -2.9f, hz + 3.0f - earUp * 0.4f, 1.3f, 1.0f, R.earH, body, body, dk);
    }
    const u32 eye = P.tint > 0.5f ? HEX(0xffe0d8) : sp.eye;
    dot(hx + R.hrx * 0.64f, R.hry * 0.66f, hz + 0.9f, eye);                                                            // eyes
    dot(hx + R.hrx * 0.64f, -R.hry * 0.66f, hz + 0.9f, eye);
    if (R.tusks) {  // tusks curl up from the lower jaw; a lunge sweeps them upward
        const float tu = P.tuskUp * 1.8f;
        stroke(hx + 2.8f, 2.0f, hz - 2.8f, hx + 3.5f, 2.7f, hz - 0.9f + tu, ivory, ivoryD, 1);
        stroke(hx + 2.8f, -2.0f, hz - 2.8f, hx + 3.5f, -2.7f, hz - 0.9f + tu, ivory, ivoryD, 1);
    }
    if (R.mane) {  // bristle mane along the spine, taller over the shoulders
        for (int i = 0; i < 7; i++) {
            const float t = i / 6.f, x = bx + 6.f - t * 13.f, h = 2.6f - 1.2f * fabsf(t - 0.25f) * 1.6f;
            spike(x, 0, zb + R.tz - sqz + P.breathe - 0.3f - fabsf(t - 0.3f) * 1.2f, h, maneL, mane);
        }
        spike(hx - 1.2f, 0, hz + 3.4f, 2.2f, maneL, mane);  // a tuft between the ears
    }
    if (R.ridge) {  // a darker fur ridge along the back
        for (int i = 0; i < 8; i++) {
            const float x = bx + 6.f - i * 1.7f;
            spike(x, 0, zb + R.tz - sqz + P.breathe - 0.3f, 1.4f, light, dk);
        }
    }
    if (R.antlers) {  // a stag's antlers: a main beam and two tines each side
        const u32 ant = tintC(HEX(0xcdb98a), P.tint * 0.5f), antD = tintC(HEX(0xa08c5e), P.tint * 0.5f);
        for (int sd = -1; sd <= 1; sd += 2) {
            const float y0 = sd * 1.4f;
            stroke(hx - 0.6f, y0, hz + R.hrz - 0.4f, hx - 1.6f, sd * 3.6f, hz + R.hrz + 5.2f, ant, antD, 1);
            stroke(hx - 1.1f, sd * 2.5f, hz + R.hrz + 2.0f, hx + 1.2f, sd * 3.4f, hz + R.hrz + 4.2f, ant, antD, 1);
            stroke(hx - 1.5f, sd * 3.3f, hz + R.hrz + 4.0f, hx - 3.0f, sd * 4.6f, hz + R.hrz + 6.4f, ant, antD, 1);
        }
    }
    if (R.spots) {  // a fawn's coat: cream flecks on the back
        static const float SP[6][3] = {{4, 2.0f, 3.5f}, {1, -2.3f, 3.6f}, {-2, 2.4f, 3.4f}, {-5, -1.5f, 3.3f}, {6, -1.2f, 3.1f}, {-1, 0.5f, 4.0f}};
        for (auto& q : SP) dot(bx + q[0], q[1] * (R.ty / 3.5f), zb + q[2] * (R.tz / 4.f) - sqz, belly);
    }
    // tail
    const float tw = P.tail * 1.2f;
    if (R.tail == 1) {  // a bushy wolf tail: hangs when relaxed, streams straight back when running
        const float up = P.stride > 3.f ? 0.4f : -1.6f;
        ell(bx - R.tx - 1.6f, tw * 0.5f, zb + 1.0f + up * 0.5f, 2.6f, 1.5f, 1.5f, body, body, dk);
        ell(bx - R.tx - 4.2f, tw, zb + 0.4f + up, 2.4f, 1.4f, 1.4f, light, body, dk);
        ell(bx - R.tx - 6.2f, tw * 1.3f, zb + 0.1f + up * 1.2f, 1.6f, 1.2f, 1.2f, dk, dk, dk);
    } else if (R.tail == 3) {  // a cotton-ball tail
        ell(bx - R.tx - 0.4f, 0, zb + 0.9f + P.tail * 0.5f, 1.5f, 1.5f, 1.5f, belly, belly, light);
    } else if (R.tail == 2) {  // a short flag with a pale underside
        ell(bx - R.tx - 0.6f, 0, zb + 1.8f, 1.1f, 1.0f, 1.8f, belly, belly, body);
    } else {
        stroke(bx - 9.f, 0, zb + 2.f, bx - 10.6f, tw, zb + 4.4f, mane, dk, 1);
        dot(bx - 10.9f, tw, zb + 5.0f, mane);
    }
    // legs: front pair at +x, rear pair at -x; diagonal pairs swing together
    const float hipZ = zb - R.tz * 0.5f;
    if (R.hopper) {
        // hind legs: a big thigh and a long foot that folds under the body or stretches out behind it; forelegs: short reaches
        const float hind = P.limp > 0 ? 0.9f : P.hind, fore = P.limp > 0 ? 0.3f : P.fore;
        for (int sd = -1; sd <= 1; sd += 2) {
            const bool near_ = sd < 0;
            const u32 lc = near_ ? body : dk;
            const float hx0 = bx + R.legBx, hy = sd * R.legY * 1.35f, ground = P.air * 0.9f + (P.limp > 0 ? P.limp * 0.6f : 0.f);
            ell(hx0, hy, zb - 0.6f - pz * 0.6f, R.tx * 0.45f, 1.2f, R.tz * 0.6f, light, body, dk);  // thigh
            stroke(hx0 + 0.4f, hy, zb - 1.6f, hx0 + 2.4f - 4.8f * hind, hy, ground + 0.4f + 0.8f * hind, lc, dk, 1);
            stroke(hx0 + 2.4f - 4.8f * hind, hy, ground + 0.4f + 0.8f * hind, hx0 + 3.4f - 5.4f * hind, hy, ground + 0.2f, lc, dk, 1);
            dot(hx0 + 3.4f - 5.4f * hind, hy, ground + 0.1f, hoof);
            const float fx0 = bx + R.legFx + 0.4f, fy = sd * R.legY * 0.9f;
            stroke(fx0, fy, zb - 1.4f + pz * 0.8f, fx0 + 1.9f * fore, fy, ground + 0.3f + 0.7f * std::max(0.f, fore), lc, dk, 1);
            dot(fx0 + 1.9f * fore, fy, ground + 0.2f + 0.7f * std::max(0.f, fore), hoof);
        }
    } else
    for (int leg = 0; leg < 4; leg++) {
        const bool front = leg < 2, left = (leg & 1) == 0;
        const float lx = bx + (front ? R.legFx : R.legBx), ly = left ? R.legY : -R.legY;
        const float ph = P.phase + ((front == left) ? 0.f : 3.14159f);
        float sw = sinf(ph) * P.stride, lf = std::max(0.f, cosf(ph)) * P.lift + P.air;
        if (P.stride <= 0.f) { sw = 0; lf = P.air; }
        float fz = lf, fx = lx + sw;
        if (P.limp > 0) {  // loose legs: stretched out sideways and a little bent
            fx = lx + (front ? 1.2f : -1.2f) * P.limp;
            fz = 0.f + P.limp * 1.2f;
        }
        stroke(lx, ly, hipZ, fx, ly * (1.f + 0.15f * P.limp), fz + 0.6f, left ? body : dk, dk, R.legW);
        dot(fx, ly, fz, hoof);
    }

    // ---- roll onto its side (death): rotate about the forward axis through the torso, then drop to the ground
    if (P.roll != 0.f) {
        const float cr = cosf(P.roll), sr = sinf(P.roll), cz = R.zb * k;
        for (auto& p : parts) {
            auto rot = [&](float& y, float& z) {
                const float dz = z - cz, ny = y * cr - dz * sr, nz = cz + y * sr + dz * cr;
                y = ny;
                z = nz;
            };
            rot(p.y, p.z);
            if (p.type == 2) rot(p.ry, p.rz);
            if (p.type == 0) {  // the radii swap as the body turns
                const float a = fabsf(cr), b = fabsf(sr), ry = p.ry * a + p.rz * b, rz = p.rz * a + p.ry * b;
                p.ry = ry;
                p.rz = rz;
            }
        }
        float lo = 1e9f;
        for (auto& p : parts) {
            lo = std::min(lo, p.type == 0 ? p.z - p.rz : p.z);
            if (p.type == 2) lo = std::min(lo, p.rz);
        }
        for (auto& p : parts) {
            p.z -= lo;
            if (p.type == 2) p.rz -= lo;
        }
    }

    // ---- project
    auto proj = [&](float x, float y, float z, float& sx, float& sy, float& d) {
        const float X = x * cp + y * sn, D = x * sn - y * cp;
        sx = ax + X;
        sy = ay + D * sph - z * cph;
        d = D;
    };
    for (auto& p : parts) {
        proj(p.x, p.y, p.z, p.sx, p.sy, p.sd);
        if (p.type == 1) p.sd += 0.01f;
    }
    std::stable_sort(parts.begin(), parts.end(), [](const RPart& a, const RPart& b) { return a.sd < b.sd; });

    for (const auto& p : parts) {
        switch (p.type) {
            case 0: {
                const float hr = sqrtf(p.rx * p.rx * cp * cp + p.ry * p.ry * sn * sn);
                const float dD = sqrtf(p.rx * p.rx * sn * sn + p.ry * p.ry * cp * cp);
                const float vr = sqrtf(p.rz * p.rz * cph * cph + dD * dD * sph * sph);
                im.ellipseShaded(p.sx, p.sy, std::max(0.9f, hr), std::max(0.9f, vr), p.c0, p.c1, p.c2);
                break;
            }
            case 1: {  // bristle: a short vertical stroke with a bright tip
                const int x = (int)floorf(p.sx), y = (int)floorf(p.sy), h = std::max(1, (int)roundf(p.rz * cph));
                for (int j = 0; j < h; j++) im.set(x, y - j, j == h - 1 ? p.c0 : p.c1);
                break;
            }
            case 2: {
                float x2, y2, d2;
                proj(p.rx, p.ry, p.rz, x2, y2, d2);
                const int a = (int)floorf(p.sx), b = (int)floorf(p.sy), c = (int)floorf(x2), d = (int)floorf(y2);
                im.line(a, b, c, d, p.c0);
                if (p.w > 1) im.line(a + 1, b, c + 1, d, p.c1);
                break;
            }
            default: im.set((int)floorf(p.sx), (int)floorf(p.sy), p.c0); break;
        }
    }
    im.outline(OUT);
    // slash arc: drawn after the outline so it stays a clean white crescent in front of the head
    if (P.slash >= 0.f) {
        float hx2, hy2, hd;
        proj(hx + 4.f, 0, hz - 1.f, hx2, hy2, hd);
        const float fx = cp, fy = sn * sph * 0.8f;  // the facing on screen
        const float base = atan2f(fy, fx);
        const float r = (7.f + 3.f * P.slash) * k;
        for (int i = 0; i <= 14; i++) {
            const float a = base - 1.1f + i * (2.2f / 14.f);
            const int x = (int)roundf(hx2 + cosf(a) * r + fx * 2.f), y = (int)roundf(hy2 + sinf(a) * r * 0.8f + fy * 2.f);
            const int al = P.slash > 0.25f ? 170 : 235;
            im.set(x, y, RGBA(255, 255, 255, al));
            if (i > 3 && i < 11) im.set(x + (fx >= 0 ? -1 : 1) * (fx == 0 ? 0 : 1), y, RGBA(255, 255, 255, al / 2));
        }
    }
    return im;
}

// Build every form / tier / view / state / pose of every species that has a full animation set.
template <class AssetsT>
inline void buildRichAnimals(SDL_Renderer* r, AssetsT& a) {
    for (int kind : RICH_KINDS) {
        auto bank = std::make_shared<RichAnimal>();
        const AnimalClips& C = *animalClips(kind);
        for (int form = 0; form < ANIMAL[kind].forms; form++) {
            const RigShape R = rigShape(kind, form);
            for (int tier = 0; tier < TR_COUNT; tier++) {
                const QuadSpec sp = fauSpec(kind, form, tier);
                const float k = TIER[tier].scale;
                for (int v = 0; v < AV_VIEWS; v++)
                    for (int st = 0; st < AN_COUNT; st++)
                        for (int i = 0; i < C.poses[st]; i++) {
                            Img im = paintQuad(sp, R, k, v, R.hopper ? hopPose(st, i) : quadPose(st, i));
                            styleActor(im, OUT, 0);  // dark contour tinted from the fill, as in the style reference (the hunter uses the same pass)
                            bank->s[form + 2 * tier][v][st][i] = makeSpr(r, im, im.w / 2, (int)(im.h - 6.f * k), true);
                        }
            }
        }
        a.rich[kind] = bank;
    }
}
