// monster_art.hpp - the cave creatures and the frozen dead as posed 3D models (model3d.hpp), painted for 8 true directions.
//
// Every monster is a handful of ellipsoids on a real skeleton (two-bone IK for the legs and arms), drawn per direction and per animation frame:
// idle 2, walk 4, attack 3, hurt 1, death 4, cast 3 (the cast clip raises the arms: the mage's bolt, the golems' slam, the king's spells).
// Units are pixels of the monster at its in-game size: X forward, Y left, Z up, the origin is the ground point under the feet.
// The wolves reuse the animal skeleton (quad3d.hpp) with their own colours. Building all of it takes a moment, so it is done on first entry
// into a delve (buildMonsterBank) on several threads.
#pragma once
#include <future>
#include <thread>
#include "sprites.hpp"

enum MState { MS_IDLE, MS_WALK, MS_ATK, MS_HURT, MS_DEATH, MS_CAST, MS_COUNT };
constexpr int MS_FRAMES[MS_COUNT] = {2, 4, 3, 1, 4, 3};
constexpr int MS_MAXF = 4;

namespace mart {
using m3::V;
using m3::Prim;
using m3::Mat;
using m3::along;
using m3::ball;
using m3::oriented;

enum { MB, MA, MD, MG, MM, MX, MC };  // body, accent, dark, glow, metal, extra, cloth

struct Pose {
    float walk = 0, amp = 0;  // gait phase and how big the steps are
    float bob = 0;            // body rise
    float atk = -1;           // -1 none; 0 windup .. 1 follow-through
    float cast = 0;           // arms raised 0..1
    float lean = 0;           // body pitch forward (rad)
    float fall = 0;           // 0..1 toppling backwards
    float tint = 0;
    float breathe = 0;
};

inline Pose poseFor(int kind, int st, int i) {
    Pose p;
    const float PI2 = 6.28318f;
    switch (st) {
        case MS_IDLE:
            p.breathe = i ? 1.f : 0.f;
            p.bob = i ? 0.5f : 0.f;
            p.walk = i ? 1.5f : 0.f;  // the bat flaps, the spider shifts
            break;
        case MS_WALK:
            p.walk = i * PI2 / 4.f;
            p.amp = 1.f;
            p.bob = 0.6f * fabsf(sinf(p.walk));
            break;
        case MS_ATK: {
            static const float A[3] = {0.f, 0.55f, 1.f};
            p.atk = A[i];
            p.lean = i == 0 ? -0.12f : (i == 1 ? 0.3f : 0.12f);
            break;
        }
        case MS_HURT:
            p.lean = -0.32f;
            p.tint = 0.75f;
            p.bob = 0.5f;
            break;
        case MS_DEATH: {
            static const float F[4] = {0.18f, 0.5f, 0.85f, 1.f};
            p.fall = F[i];
            p.tint = i == 0 ? 0.5f : 0.f;
            break;
        }
        case MS_CAST: {
            static const float C[3] = {0.4f, 1.f, 0.8f};
            p.cast = C[i];
            p.lean = i == 1 ? 0.08f : -0.06f;
            break;
        }
    }
    (void)kind;
    return p;
}

inline std::vector<Mat> mats(int kind, float tint) {
    auto T = [&](u32 c) { return tint > 0.f ? mixc(c, HEX(0xe03a30), std::min(1.f, tint)) : c; };
    auto M = [&](u32 base, u32 under, u32 top, float fur = 0.f, bool glow = false) { return Mat{glow ? base : T(base), T(under), T(top), fur, glow}; };
    std::vector<Mat> m(7);
    switch (kind) {
        case MK_BAT:
            m = {M(HEX(0x4a3a4c), HEX(0x2e2431), HEX(0x6a566a), 0.4f), M(HEX(0x6e4a6a), HEX(0x3c2a3c), HEX(0x8a6488)), M(HEX(0x1e1822), HEX(0x1e1822), HEX(0x1e1822)),
                 M(HEX(0xff5040), 0, 0, 0, true), M(HEX(0xe8e0d0), HEX(0xb8b0a0), HEX(0xfff8e8)), M(HEX(0x5a465c), HEX(0x34283a), HEX(0x6e5870)), M(HEX(0x5a465c), 0, 0)};
            break;
        case MK_SPIDER:
            m = {M(HEX(0x3c3446), HEX(0x261f2e), HEX(0x564a66), 0.3f), M(HEX(0xa04aa8), HEX(0x6a2a72), HEX(0xc46acc)), M(HEX(0x221c2a), HEX(0x221c2a), HEX(0x221c2a), 0.2f),
                 M(HEX(0xff4a3a), 0, 0, 0, true), M(HEX(0xe8e0d0), HEX(0xb8b0a0), HEX(0xfff8e8)), M(HEX(0x4a4058), HEX(0x2e2638), HEX(0x645878)), M(0, 0, 0)};
            break;
        case MK_GOLEM:
            m = {M(HEX(0x8a7a68), HEX(0x5e5246), HEX(0xa89a84)), M(HEX(0xc07a3a), HEX(0x8a4e22), HEX(0xe0a060)), M(HEX(0x3a3128), HEX(0x3a3128), HEX(0x4a4036)),
                 M(HEX(0xffb040), 0, 0, 0, true), M(HEX(0x6f6458), HEX(0x4a4036), HEX(0x8a7e6e)), M(HEX(0x9c8e78), HEX(0x6a5e4e), HEX(0xb8aa92)), M(0, 0, 0)};
            break;
        case MK_ICESKEL:
            m = {M(HEX(0xd2e4ee), HEX(0x92acbe), HEX(0xf0fbff)), M(HEX(0x9ad8f0), HEX(0x5a98b8), HEX(0xd0f4ff)), M(HEX(0x2a3a4a), HEX(0x2a3a4a), HEX(0x2a3a4a)),
                 M(HEX(0x7fe8ff), 0, 0, 0, true), M(HEX(0x9db4c4), HEX(0x627c8e), HEX(0xcfe2ee)), M(HEX(0x6a8498), HEX(0x405466), HEX(0x8aa4b8)), M(0, 0, 0)};
            break;
        case MK_ICEGOLEM:
            m = {M(HEX(0x8ecff0), HEX(0x4f8fbf), HEX(0xd8f4ff)), M(HEX(0xc4eeff), HEX(0x7ab8d8), HEX(0xf4fdff)), M(HEX(0x2a5075), HEX(0x2a5075), HEX(0x35648c)),
                 M(HEX(0xd8faff), 0, 0, 0, true), M(HEX(0x5f9ec8), HEX(0x3a6e94), HEX(0x8ccaee)), M(HEX(0xa8dcf4), HEX(0x6aa6c8), HEX(0xe0f8ff)), M(0, 0, 0)};
            break;
        case MK_KNIGHT:
            m = {M(HEX(0x7f93a8), HEX(0x4e5f70), HEX(0xb4c6d8)), M(HEX(0xd9e4ee), HEX(0x9aa8b6), HEX(0xffffff)), M(HEX(0x34414f), HEX(0x34414f), HEX(0x445466)),
                 M(HEX(0x8feaff), 0, 0, 0, true), M(HEX(0xa9b8c8), HEX(0x6c7c8c), HEX(0xe0ecf6)), M(HEX(0x2a3a78), HEX(0x1c2652), HEX(0x4056a0)), M(HEX(0x3a4a66), HEX(0x28344a), HEX(0x56688a))};
            break;
        case MK_MAGE:
            m = {M(HEX(0x3c5a9a), HEX(0x2a3f73), HEX(0x5c80c8)), M(HEX(0xcfe8ff), HEX(0x8ab0d0), HEX(0xffffff)), M(HEX(0x141c34), HEX(0x141c34), HEX(0x1c2848)),
                 M(HEX(0x9ff4ff), 0, 0, 0, true), M(HEX(0x8a6a4a), HEX(0x5a4430), HEX(0xa88862)), M(HEX(0xb4cce4), HEX(0x7a94ae), HEX(0xdcecfa)), M(HEX(0x4a6ab0), HEX(0x33497c), HEX(0x6a8ad0))};
            break;
        case MK_KING:
            m = {M(HEX(0x5a7ea8), HEX(0x34506e), HEX(0x92b8dc)), M(HEX(0xdff6ff), HEX(0x9ac8e0), HEX(0xffffff)), M(HEX(0x1e2c46), HEX(0x1e2c46), HEX(0x2c3e5e)),
                 M(HEX(0xb8f8ff), 0, 0, 0, true), M(HEX(0x9ec6e4), HEX(0x5a86a8), HEX(0xe4f6ff)), M(HEX(0x2a3a78), HEX(0x1a2450), HEX(0x4458a8)), M(HEX(0x384e74), HEX(0x24344e), HEX(0x4e688e))};
            break;
    }
    return m;
}

// rotate the whole model about the Y axis through the origin (toppling backwards: +Z falls towards -X)
inline void topple(std::vector<Prim>& ps, float th) {
    const float c = cosf(th), s = sinf(th);
    auto R = [&](V v) { return V{v.x * c - v.z * s, v.y, v.x * s + v.z * c}; };
    for (Prim& p : ps) {
        p.c = R(p.c);
        for (V& a : p.ax) a = R(a);
    }
}
inline void roll(std::vector<Prim>& ps, float th, float cz) {  // about the X axis through (0,0,cz)
    const float c = cosf(th), s = sinf(th);
    auto R = [&](V v) { return V{v.x, v.y * c - v.z * s, v.y * s + v.z * c}; };
    for (Prim& p : ps) {
        V q = p.c;
        q.z -= cz;
        q = R(q);
        q.z += cz;
        p.c = q;
        for (V& a : p.ax) a = R(a);
    }
}
inline void scaleAll(std::vector<Prim>& ps, float k) {
    for (Prim& p : ps) p.c = p.c * k, p.r = p.r * k;
}
inline void limb(std::vector<Prim>& o, V a, V b, V c, float r0, float r1, int mat) {
    o.push_back(along(a, b, r0, mat));
    o.push_back(along(b, c, r1, mat));
    o.push_back(ball(b, {r0 * 1.05f, r0 * 1.05f, r0 * 1.05f}, mat));
}
// two-bone chain in the plane through J and T whose "up" is Z; the middle joint goes up/forward when `fwd`
inline V joint3(V J, V T, float L1, float L2, bool upBend) {
    V h{T.x - J.x, T.y - J.y, 0};
    const float hl = std::sqrt(h.x * h.x + h.y * h.y);
    V hd = hl > 1e-4f ? V{h.x / hl, h.y / hl, 0} : V{1, 0, 0};
    V k, an;
    m3::ik2(V{0, 0, J.z}, V{hl, 0, T.z}, L1, L2, upBend ? -1.f : 1.f, k, an);
    // ik2 picks by x; for an upward bend take the higher of the two solutions instead
    if (upBend) {
        const float dx = hl, dz = T.z - J.z;
        float d = std::sqrt(dx * dx + dz * dz);
        d = std::max(std::fabs(L1 - L2) + 0.05f, std::min(L1 + L2 - 0.05f, d));
        const float th0 = std::atan2(dz, dx), A = std::acos(std::max(-1.f, std::min(1.f, (L1 * L1 + d * d - L2 * L2) / (2.f * L1 * d))));
        const float z1 = L1 * std::sin(th0 + A), z2 = L1 * std::sin(th0 - A);
        const float ang = z1 > z2 ? th0 + A : th0 - A;
        k = V{L1 * std::cos(ang), 0, J.z + L1 * std::sin(ang)};
    }
    return V{J.x + hd.x * k.x, J.y + hd.y * k.x, k.z};
}

struct Arm {  // one arm: shoulder, hand target
    V sh, hand;
};

// ---------------------------------------------------------------- bipeds
struct Biped {
    float legL, legR;           // each leg bone, thickness
    float torsoZ;               // height of the torso centre above the hip
    V torso;                    // torso radii
    float shY, shZ;             // shoulder half-width, height above the torso centre
    float armL, armR;           // each arm bone, thickness
    V head;                     // head radii
    float hdz;                  // head centre above the torso centre
    float step = 3.4f, lift = 2.2f;
};

inline void biped(std::vector<Prim>& o, int kind, const Pose& P) {
    Biped B{};
    switch (kind) {
        case MK_ICESKEL: B = {6.4f, 0.95f, 4.2f, {2.4f, 3.0f, 4.2f}, 3.4f, 3.4f, 5.6f, 0.9f, {3.4f, 2.9f, 3.5f}, 7.6f, 3.2f, 2.0f}; break;
        case MK_KNIGHT: B = {7.2f, 2.0f, 4.6f, {4.4f, 5.6f, 6.0f}, 5.6f, 4.4f, 5.4f, 1.8f, {3.6f, 3.2f, 3.6f}, 8.8f, 3.0f, 1.8f}; break;
        case MK_KING: B = {7.2f, 2.2f, 4.8f, {4.8f, 6.2f, 6.4f}, 6.2f, 4.6f, 5.8f, 2.1f, {3.7f, 3.4f, 3.8f}, 9.2f, 3.0f, 1.8f}; break;
        case MK_MAGE: B = {5.0f, 1.8f, 5.0f, {3.4f, 4.4f, 5.6f}, 4.4f, 4.2f, 5.0f, 1.5f, {3.4f, 3.0f, 3.4f}, 8.2f, 1.6f, 0.8f}; break;
        default: B = {6.2f, 3.6f, 5.4f, {6.6f, 8.2f, 7.4f}, 9.6f, 5.4f, 7.6f, 3.4f, {3.8f, 3.6f, 3.6f}, 8.8f, 1.8f, 1.2f}; break;  // golems
    }
    const bool robe = kind == MK_MAGE;
    const bool golem = kind == MK_GOLEM || kind == MK_ICEGOLEM;
    const bool kk = kind == MK_KING || kind == MK_KNIGHT;
    const float hipZ = B.legL * 2.f * 0.95f + P.bob - P.breathe * 0.0f;
    const float tz = hipZ + B.torsoZ;
    const float lean = P.lean;
    const float cl = cosf(lean), sl = sinf(lean);
    auto atTorso = [&](float x, float y, float z) { return V{x * cl + z * sl, y, tz + (-x * sl + z * cl)}; };  // a point in torso space, leaned
    const int MBc = MB, MDc = MD;
    // legs
    if (!robe) {
        for (int s = -1; s <= 1; s += 2) {
            const float ph = P.walk + (s > 0 ? 0.f : 3.14159f);
            float fx = P.amp * B.step * sinf(ph), fz = P.amp * B.lift * std::max(0.f, cosf(ph));
            if (P.atk >= 0.f) fx = (s > 0 ? 2.2f : -2.2f);
            const V hip{0, s * B.shY * 0.55f, hipZ};
            const V foot{fx, s * B.shY * 0.62f, fz + 0.4f};
            V kn, an;
            m3::ik2(hip, V{foot.x, hip.y, foot.z + B.legR * 0.6f}, B.legL, B.legL, 1.f, kn, an);
            kn.y = hip.y;
            const V ft{an.x, foot.y, an.z - B.legR * 0.6f};
            limb(o, hip, V{kn.x, foot.y, kn.z}, an, B.legR, B.legR * 0.9f, kind == MK_ICESKEL ? MB : (golem ? MB : MD));
            o.push_back(oriented({an.x + 1.2f, foot.y, ft.z + B.legR * 0.7f}, {1, 0, 0}, {0, 0, 1}, {B.legR * 1.9f, B.legR * 1.2f, B.legR * 0.8f}, golem ? MB : MDc));
        }
        o.push_back(ball({0, 0, hipZ}, {B.torso.x * 0.9f, B.torso.y * 0.8f, B.torso.z * 0.35f}, MDc));  // pelvis
    } else {
        // the robe: a stack of ellipsoids from the hem up, swaying a little with the gait
        const float sw = P.amp * 1.2f * sinf(P.walk);
        for (int i = 0; i < 5; i++) {
            const float t = i / 4.f;
            o.push_back(ball({sw * (1 - t) * 0.6f, 0, 2.4f + t * (hipZ + B.torsoZ - 2.4f)}, {4.6f - 1.5f * t, 5.6f - 1.5f * t, 3.4f}, MC));
        }
        o.push_back(ball({1.6f + sw, 2.0f, 1.0f}, {2.2f, 1.2f, 1.0f}, MD));  // boots under the hem
        o.push_back(ball({1.6f - sw, -2.0f, 1.0f}, {2.2f, 1.2f, 1.0f}, MD));
    }
    // torso
    const float br = 1.f + 0.04f * P.breathe;
    if (kind == MK_ICESKEL) {
        o.push_back(along(atTorso(0, 0, -B.torso.z), atTorso(0, 0, B.torso.z), 1.0f, MB));  // spine
        for (int i = 0; i < 3; i++) o.push_back(ball(atTorso(0.4f, 0, 1.8f - i * 2.1f), {1.6f, 3.4f * br, 1.0f}, MB));  // ribs
        o.push_back(ball(atTorso(-0.5f, 0, 5.2f), {1.6f, 4.4f, 1.0f}, MB));  // collar
    } else if (golem) {
        o.push_back(ball(atTorso(0, 0, 0), {B.torso.x * br, B.torso.y * br, B.torso.z}, MB));
        o.push_back(ball(atTorso(0.5f, 0, -3.5f), {B.torso.x * 0.9f, B.torso.y * 0.85f, 3.2f}, MM));
        o.push_back(ball(atTorso(B.torso.x * 0.82f, 0, 1.2f), {1.8f, 3.4f, 3.2f}, MG));  // the core
        // rock plates / ice shards on the back
        for (int i = -1; i <= 1; i++) o.push_back(oriented(atTorso(-B.torso.x * 0.7f, i * 3.6f, B.torso.z * 0.5f + 1.f), {-0.5f, 0, 1}, {0, 1, 0}, {3.6f, 1.6f, 1.6f}, MX));
    } else if (robe) {
        o.push_back(ball(atTorso(0, 0, 0), {B.torso.x * br, B.torso.y * br, B.torso.z}, MC));
        o.push_back(ball(atTorso(0.4f, 0, -1.0f), {B.torso.x * 0.9f, B.torso.y * 1.05f, 1.2f}, MA));  // belt / sash
    } else {
        o.push_back(ball(atTorso(0, 0, 0), {B.torso.x * br, B.torso.y * br, B.torso.z}, MB));
        o.push_back(ball(atTorso(0.9f, 0, 0.8f), {B.torso.x * 0.85f, B.torso.y * 0.8f, B.torso.z * 0.7f}, MM));  // breastplate
        o.push_back(ball(atTorso(0.3f, 0, -B.torso.z * 0.8f), {B.torso.x * 0.95f, B.torso.y * 0.9f, 1.4f}, MA));  // belt
    }
    // head
    const V hc = atTorso(0.6f + (P.fall > 0 ? 0 : 0), 0, B.hdz);
    if (kind == MK_ICESKEL) {
        o.push_back(ball(hc, B.head, MB));
        o.push_back(ball(hc + V{1.8f, 0, -2.4f}, {2.2f, 1.9f, 1.0f}, MB));  // jaw
        o.push_back(ball(hc + V{2.9f, 1.2f, 0.5f}, {0.7f, 0.7f, 0.7f}, MG));
        o.push_back(ball(hc + V{2.9f, -1.2f, 0.5f}, {0.7f, 0.7f, 0.7f}, MG));
        o.push_back(ball(hc + V{3.2f, 0, -0.6f}, {0.6f, 0.5f, 0.9f}, MD));
    } else if (golem) {
        o.push_back(ball(hc, B.head, MB));
        o.push_back(ball(hc + V{2.4f, 0, 1.4f}, {1.8f, 3.2f, 1.2f}, MD));  // brow
        o.push_back(ball(hc + V{3.1f, 1.3f, 0.2f}, {0.8f, 0.8f, 0.8f}, MG));
        o.push_back(ball(hc + V{3.1f, -1.3f, 0.2f}, {0.8f, 0.8f, 0.8f}, MG));
    } else if (robe) {
        o.push_back(ball(hc, {B.head.x * 1.25f, B.head.y * 1.25f, B.head.z * 1.2f}, MC));  // hood
        o.push_back(ball(hc + V{1.6f, 0, -0.2f}, {2.6f, 2.4f, 2.6f}, MD));                  // the dark face
        o.push_back(ball(hc + V{3.0f, 1.0f, 0.2f}, {0.65f, 0.65f, 0.65f}, MG));
        o.push_back(ball(hc + V{3.0f, -1.0f, 0.2f}, {0.65f, 0.65f, 0.65f}, MG));
        o.push_back(oriented(hc + V{-2.2f, 0, 2.2f}, {-1, 0, 1.2f}, {0, 1, 0}, {3.2f, 1.6f, 1.6f}, MC));  // pointed hood tip
    } else {
        o.push_back(ball(hc, {B.head.x * 1.12f, B.head.y * 1.12f, B.head.z * 1.1f}, MM));  // helm
        o.push_back(ball(hc + V{2.8f, 0, 0.1f}, {1.0f, 2.6f, 0.7f}, MD));                    // visor slit
        o.push_back(ball(hc + V{3.1f, 0.9f, 0.15f}, {0.55f, 0.55f, 0.55f}, MG));
        o.push_back(ball(hc + V{3.1f, -0.9f, 0.15f}, {0.55f, 0.55f, 0.55f}, MG));
        o.push_back(oriented(hc + V{-0.4f, 0, B.head.z * 1.1f}, {1, 0, 0}, {0, 1, 0}, {3.2f, 0.7f, 1.4f}, MA));  // crest
        if (kind == MK_KING) {
            for (int i = 0; i < 5; i++) {  // the crown: five ice spikes
                const float a = (i - 2) * 0.62f;
                o.push_back(oriented(hc + V{-0.5f + 2.6f * cosf(a + 1.57f) * 0.0f, 3.0f * sinf(a), B.head.z * 1.1f + 1.8f}, {0, 0, 1}, {1, 0, 0}, {2.8f + (i == 2 ? 1.4f : 0.f), 0.8f, 0.8f}, MA));
            }
            o.push_back(ball(hc + V{-0.4f, 0, B.head.z * 1.0f + 0.4f}, {3.6f, 3.6f, 1.0f}, MA));
        }
    }
    // arms
    const bool twoHand = kind == MK_GOLEM || kind == MK_ICEGOLEM;
    V weaponHand{0, 0, 0}, shieldHand{0, 0, 0};
    float wAng = 1.1f;
    for (int s = -1; s <= 1; s += 2) {
        const V sh = atTorso(-0.4f, s * B.shY, B.shZ);
        if (golem || kk) o.push_back(ball(sh + V{0, s * 0.8f, 1.0f}, kind == MK_KING ? V{3.6f, 3.6f, 3.2f} : (golem ? V{4.4f, 4.4f, 4.0f} : V{3.0f, 3.0f, 2.8f}), kk ? MM : MX));
        else o.push_back(ball(sh, {B.armR * 1.3f, B.armR * 1.3f, B.armR * 1.3f}, robe ? MC : MB));
        // where the hand goes
        const float ph = P.walk + (s > 0 ? 3.14159f : 0.f);  // arms swing against the legs
        V hand{P.amp * 2.2f * sinf(ph) + 1.5f, s * (B.shY + 1.0f), sh.z - B.armL * 1.8f + 0.6f};
        const bool weaponArm = s < 0 && !twoHand && kind != MK_MAGE;  // the hunter-side right hand holds the weapon
        const bool mageStaff = robe && s < 0;
        if (P.cast > 0.f) {
            const float c = P.cast;
            if (twoHand) hand = V{-2.0f + 6.f * (1 - c) * 0.f, s * (B.shY - 0.5f), sh.z + 4.f + c * 9.f};  // both fists overhead
            else hand = V{3.0f + 2.f * c, s * 3.2f, sh.z + 2.f + c * 8.f};
        } else if (P.atk >= 0.f) {
            const float a = P.atk;
            if (twoHand) hand = V{-1.5f + a * a * 14.f, s * (B.shY - 0.5f), sh.z + 11.f - a * a * 18.f + (a < 0.3f ? 3.f : 0.f)};
            else if (weaponArm) hand = V{-4.0f + a * 17.f, s * (B.shY - 0.5f), sh.z + 9.f - a * a * 15.f};
            else if (mageStaff) hand = V{3.0f + a * 6.f, s * 3.f, sh.z + 5.f - a * 5.f};
            else hand = V{2.0f + a * 4.f, s * (B.shY - 0.5f), sh.z - 2.f + a * 3.f};
        }
        V el = joint3(sh, hand, B.armL, B.armL, false);
        // joint3 bends upward-first when asked; the elbow of a hanging arm should be a little back/out
        el.y = (sh.y + hand.y) * 0.5f + s * 0.8f;
        limb(o, sh, el, hand, B.armR, B.armR * 0.92f, golem ? MB : (kind == MK_ICESKEL ? MB : (robe ? MC : (kk ? MM : MB))));
        o.push_back(ball(hand, {B.armR * 1.25f, B.armR * 1.25f, B.armR * 1.25f}, golem ? MM : (robe ? MA : MD)));  // fist / glove
        if (weaponArm) {
            weaponHand = hand;
            wAng = P.atk >= 0.f ? (1.9f - P.atk * 2.5f) : 1.15f;
        }
        if (s > 0 && kk && kind == MK_KNIGHT) shieldHand = hand;
        if (mageStaff) weaponHand = hand;
    }
    // weapons
    if (kind == MK_ICESKEL || kind == MK_KNIGHT || kind == MK_KING) {
        const float wl = kind == MK_KING ? 19.f : (kind == MK_KNIGHT ? 12.f : 9.f);
        const V tip = weaponHand + V{cosf(wAng) * wl, 0, sinf(wAng) * wl};
        const V back = weaponHand + V{-cosf(wAng) * 2.5f, 0, -sinf(wAng) * 2.5f};
        o.push_back(along(back, tip, kind == MK_KING ? 1.7f : 1.1f, MM, 1.0f, 0.5f));
        o.push_back(along(weaponHand + V{-0.3f, -2.2f, 0}, weaponHand + V{-0.3f, 2.2f, 0}, 0.8f, MA));  // guard
        if (kind == MK_KING) o.push_back(ball(tip, {1.4f, 1.4f, 1.4f}, MG));
    }
    if (kind == MK_KNIGHT) {  // the kite shield on the left arm
        const V sc = shieldHand + V{2.4f, 2.2f, 0.f};
        o.push_back(oriented(sc, {1, 0, 0}, {0, 0, 1}, {1.4f, 4.6f, 6.2f}, MB));
        o.push_back(oriented(sc + V{1.2f, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0.6f, 1.2f, 4.4f}, MA));
        o.push_back(oriented(sc + V{1.2f, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0.6f, 3.2f, 1.1f}, MA));
    }
    if (kind == MK_KING) {  // the cape
        const float sw = P.amp * 1.0f * sinf(P.walk * 2.f);
        o.push_back(oriented(atTorso(-B.torso.x - 1.2f, 0, -2.0f) + V{sw, 0, 0}, {0, 0, 1}, {1, 0, 0}, {B.torso.z * 1.9f, B.torso.y * 1.1f, 0.9f}, MX));
    }
    if (robe) {  // the staff with its glowing orb (a cast lifts it)
        const V g = weaponHand;
        const V top = g + V{1.2f, 0, 9.f + P.cast * 3.f};
        o.push_back(along(V{g.x, g.y, std::max(0.f, g.z - 10.f)}, top, 0.7f, MM, 1.0f));
        o.push_back(ball(top + V{0, 0, 1.4f}, {2.2f + P.cast * 0.9f, 2.2f + P.cast * 0.9f, 2.2f + P.cast * 0.9f}, MG));
        o.push_back(ball(top + V{0, 0, -0.4f}, {1.6f, 1.6f, 0.7f}, MA));
    }
    if (golem) {  // chunky knuckle accents
        o.push_back(ball(hc + V{-1.6f, 0, 3.4f}, {1.6f, 1.6f, 2.4f}, MX));
    }
}

// ---------------------------------------------------------------- the cave spider
inline void spider(std::vector<Prim>& o, const Pose& P) {
    const float z = 5.2f + P.bob * 0.5f;
    const float lunge = P.atk >= 0 ? 3.2f * P.atk - 1.f : 0.f;
    const V th{1.8f + lunge, 0, z};
    o.push_back(ball({-5.4f + lunge * 0.6f, 0, z + 0.9f}, {5.6f, 4.4f, 4.2f * (1.f + 0.04f * P.breathe)}, MB));  // abdomen
    o.push_back(ball({-5.8f + lunge * 0.6f, 0, z + 3.9f}, {2.4f, 1.1f, 0.8f}, MA));                              // the mark on its back
    o.push_back(ball(th, {3.4f, 3.0f, 2.8f}, MD));                                                                 // cephalothorax
    o.push_back(ball({4.8f + lunge, 0, z - 0.2f}, {2.2f, 2.2f, 2.0f}, MD));
    o.push_back(ball({6.4f + lunge, 1.0f, z + 0.8f}, {0.9f, 0.9f, 0.9f}, MG));
    o.push_back(ball({6.4f + lunge, -1.0f, z + 0.8f}, {0.9f, 0.9f, 0.9f}, MG));
    o.push_back(ball({6.0f + lunge, 2.1f, z + 1.4f}, {0.55f, 0.55f, 0.55f}, MG));
    o.push_back(ball({6.0f + lunge, -2.1f, z + 1.4f}, {0.55f, 0.55f, 0.55f}, MG));
    const float bite = P.atk >= 0 ? P.atk : 0.f;
    o.push_back(along({6.2f + lunge, 0.9f, z - 1.2f}, {7.2f + lunge + bite, 0.4f, z - 2.8f - bite}, 0.6f, MM));  // fangs
    o.push_back(along({6.2f + lunge, -0.9f, z - 1.2f}, {7.2f + lunge + bite, -0.4f, z - 2.8f - bite}, 0.6f, MM));
    // eight legs: pair k at angle ANG[k] from the nose, alternating gait groups
    static const float ANG[4] = {0.75f, 1.35f, 1.9f, 2.5f}, HX[4] = {2.8f, 1.2f, -0.4f, -2.2f};
    for (int s = -1; s <= 1; s += 2)
        for (int k = 0; k < 4; k++) {
            const float grp = ((k + (s > 0 ? 1 : 0)) & 1) ? 0.f : 3.14159f;
            const float ph = P.walk + grp;
            const float a = ANG[k];
            const float reach = 9.5f + (k == 0 ? 1.f : 0.f);
            V hip{HX[k] + lunge, s * 2.2f, z - 0.2f};
            V foot{hip.x + cosf(a) * reach + P.amp * 2.2f * sinf(ph), s * (2.2f + sinf(a) * reach), std::max(0.f, P.amp * 2.0f * cosf(ph))};
            if (P.atk >= 0 && k == 0) foot = V{hip.x + 8.f, s * 4.f, 5.f + 4.f * (1.f - P.atk)};  // the front legs reach for the hunter
            if (P.fall > 0.f) {  // curling up
                foot.x = hip.x + cosf(a) * reach * (1.f - 0.6f * P.fall);
                foot.y = s * (2.2f + sinf(a) * reach * (1.f - 0.6f * P.fall));
                foot.z = 3.f + 7.f * P.fall;
            }
            const V kn = joint3(hip, foot, 6.f, 6.f, true);
            limb(o, hip, kn, foot, 0.75f, 0.6f, MD);
        }
}

// ---------------------------------------------------------------- the cave bat
inline void bat(std::vector<Prim>& o, const Pose& P, int st, int i) {
    float flap;  // wing angle: +up .. -down
    if (st == MS_WALK) flap = sinf(P.walk + 1.2f) * 1.0f;
    else if (st == MS_IDLE) flap = i ? -0.35f : 0.35f;
    else if (st == MS_ATK) flap = i == 0 ? 0.9f : (i == 1 ? -0.8f : 0.2f);
    else if (st == MS_HURT) flap = 0.9f;
    else flap = 1.2f;
    const float z = 13.f + P.bob + (st == MS_WALK ? sinf(P.walk) * 1.2f : 0.f);
    const float lunge = P.atk >= 0 ? 4.f * P.atk : 0.f;
    const float pitch = P.atk >= 0 ? -0.5f * P.atk : 0.f;
    o.push_back(oriented({0, 0, z}, {1, 0, pitch}, {0, 0, 1}, {3.8f, 2.7f, 2.7f}, MB));
    o.push_back(ball({4.0f + lunge, 0, z + 0.4f + pitch * 2.f}, {2.5f, 2.2f, 2.2f}, MB));
    o.push_back(ball({5.8f + lunge, 0, z - 0.1f}, {1.3f, 1.2f, 1.0f}, MX));
    o.push_back(oriented({3.6f + lunge, 1.5f, z + 3.0f}, {0, 0.3f, 1}, {1, 0, 0}, {2.4f, 0.9f, 0.6f}, MB));  // ears
    o.push_back(oriented({3.6f + lunge, -1.5f, z + 3.0f}, {0, -0.3f, 1}, {1, 0, 0}, {2.4f, 0.9f, 0.6f}, MB));
    o.push_back(ball({5.6f + lunge, 0.9f, z + 0.9f}, {0.55f, 0.55f, 0.55f}, MG));
    o.push_back(ball({5.6f + lunge, -0.9f, z + 0.9f}, {0.55f, 0.55f, 0.55f}, MG));
    if (P.atk >= 0.f) {  // the bite
        o.push_back(ball({6.4f + lunge, 0.5f, z - 1.0f}, {0.45f, 0.45f, 0.9f}, MM));
        o.push_back(ball({6.4f + lunge, -0.5f, z - 1.0f}, {0.45f, 0.45f, 0.9f}, MM));
    }
    for (int s = -1; s <= 1; s += 2) {
        const V sh{0.4f, s * 2.2f, z + 0.6f};
        // bone: shoulder -> wrist -> tip, the membrane is a flat ellipsoid between the finger bones
        const float up = flap, fold = (st == MS_HURT || st == MS_DEATH) ? 0.7f : 0.f;
        const V wr = sh + V{-0.6f, s * 6.5f * cosf(up) * (1 - fold * 0.6f), 6.5f * sinf(up)};
        const V tp = wr + V{-3.2f, s * 8.f * cosf(up * 1.25f) * (1 - fold * 0.7f), 8.f * sinf(up * 1.25f) - 1.f};
        o.push_back(along(sh, wr, 0.8f, MD));
        o.push_back(along(wr, tp, 0.6f, MD));
        const V mid = (sh + tp) * 0.5f + V{-2.8f, 0, 0};
        Prim m = oriented(mid, tp - sh, {1, 0, 0}, {len(tp - sh) * 0.5f, 3.6f, 0.45f}, MA);
        o.push_back(m);
        o.push_back(oriented((wr + tp) * 0.5f + V{-3.0f, 0, 0}, tp - wr, {1, 0, 0}, {len(tp - wr) * 0.5f, 2.8f, 0.4f}, MA));
    }
}
}  // namespace mart

// ---------------------------------------------------------------- the bank
struct MonsterArt {
    int cell;
    float pxu;
};
inline MonsterArt monsterArt(int k) {
    static const MonsterArt A[MK_COUNT] = {{56, 2.0f}, {64, 2.0f}, {88, 1.8f}, {64, 0.f}, {72, 1.9f}, {64, 0.f}, {96, 1.8f}, {72, 1.9f}, {72, 1.9f}, {128, 1.6f}};
    return A[k];
}
// the wolves are the animal skeleton in other colours
inline QuadSpec monsterWolfSpec(int kind) {
    QuadSpec s = specWolf();
    if (kind == MK_CAVEWOLF) {
        s.body = HEX(0x4e4852); s.light = HEX(0x6a6270); s.belly = HEX(0x8a8294); s.dark = HEX(0x2c2832);
        s.snout = HEX(0x7a7280); s.eye = HEX(0xff7a40);
    } else {
        s.body = HEX(0xb4d2e8); s.light = HEX(0xdceefa); s.belly = HEX(0xf2fbff); s.dark = HEX(0x6a96b8);
        s.snout = HEX(0xe8f4fc); s.eye = HEX(0x7fefff);
    }
    return s;
}

struct MonsterBank {
    Spr s[MK_COUNT][AV_VIEWS][MS_COUNT][MS_MAXF];
    bool built = false;
    bool has(int k, int st) const { return k >= 0 && k < MK_COUNT && s[k][0][st][0].ok(); }
    const Spr& get(int k, int view, int st, int fr) const {
        fr = std::max(0, std::min(MS_FRAMES[st] - 1, fr));
        const Spr& r = s[k][view][st][fr];
        if (r.ok()) return r;
        return s[k][view][MS_IDLE][0];  // CAST falls back to ATK for the kinds that have no cast clip
    }
};

inline bool monsterHasCast(int k) { return k == MK_MAGE || k == MK_KING || k == MK_GOLEM || k == MK_ICEGOLEM; }

struct MonsterImg { Img im; int ax = 0, ay = 0; float sc = 1.f; };
inline void paintMonsterFrame(int k, int v, int st, int i, MonsterImg& o) {
    static const float ALPHA[AV_VIEWS] = {1.5708f, 0.7854f, 0.f, -0.7854f, -1.5708f, -2.3562f, 3.14159f, 2.3562f};
        const MonsterArt A = monsterArt(k);
        std::vector<m3::Prim> prims;
        std::vector<m3::Mat> ms;
        float pxu = A.pxu, ox = A.cell / 2.f, oy = A.cell - 9.f;
        if (k == MK_CAVEWOLF || k == MK_FROSTWOLF) {
            const m3::Q3& Q = wolfQ3();
            // wolf poses come from the animal clips: idle 2, walk 4 of the 6, attack 3 of the 5, hurt, death 4 of the 6
            static const int WALKI[4] = {0, 1, 3, 4}, ATKI[3] = {1, 3, 4}, DEATHI[4] = {0, 2, 4, 5};
            BPose P;
            if (st == MS_IDLE) P = quadPose(AN_IDLE, i * 2);
            else if (st == MS_WALK) P = quadPose(AN_RUN, WALKI[i] % 5);
            else if (st == MS_ATK) P = quadPose(AN_ATTACK, ATKI[i]);
            else if (st == MS_HURT) P = quadPose(AN_HURT, 1);
            else P = quadPose(AN_DEATH, DEATHI[i]);
            m3::buildQuad(Q, P, 1.f, 1.f, 1.f, false, prims);
            const QuadSpec sp = monsterWolfSpec(k);
            ms = m3::quadMats(sp, P.tint, 0.15f);
            pxu = Q.pxu; ox = Q.ox; oy = Q.oy;
            const int cell = 64;
            o.im = m3::render(prims, ms, ALPHA[v], 0.70f, pxu, cell, cell, ox, oy, 3);
        } else {
            const mart::Pose P = mart::poseFor(k, st, i);
            if (k == MK_BAT) mart::bat(prims, P, st, i);
            else if (k == MK_SPIDER) mart::spider(prims, P);
            else mart::biped(prims, k, P);
            if (P.fall > 0.f) {
                if (k == MK_SPIDER) mart::roll(prims, 3.14159f * P.fall, 4.f);
                else if (k == MK_BAT) { mart::roll(prims, 1.9f * P.fall, 12.f); for (m3::Prim& p : prims) p.c.z -= 10.f * P.fall; }
                else { mart::topple(prims, 1.50f * P.fall); for (m3::Prim& p : prims) p.c.z += 1.2f * P.fall; }
            }
            if (k == MK_KING) mart::scaleAll(prims, 1.0f);
            ms = mart::mats(k, P.tint);
            o.im = m3::render(prims, ms, ALPHA[v], 0.70f, pxu, A.cell, A.cell, ox, oy, 3);
        }
        styleActor(o.im, OUT, 0);
        int x0 = o.im.w, y0 = o.im.h, x1 = -1, y1 = -1;
        for (int y = 0; y < o.im.h; y++)
            for (int x = 0; x < o.im.w; x++)
                if (cA(o.im.p[(size_t)y * o.im.w + x])) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
        o.ax = (int)ox; o.ay = (int)oy; o.sc = 1.f / pxu;
        if (x1 >= x0 && y1 >= y0) {
            Img c(x1 - x0 + 1, y1 - y0 + 1);
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) c.p[(size_t)(y - y0) * c.w + (x - x0)] = o.im.p[(size_t)y * o.im.w + x];
            o.im = c; o.ax -= x0; o.ay -= y0;
        }
    }

inline void buildMonsterBank(SDL_Renderer* r, MonsterBank& bank) {
    static const float ALPHA[AV_VIEWS] = {1.5708f, 0.7854f, 0.f, -0.7854f, -1.5708f, -2.3562f, 3.14159f, 2.3562f};
    struct Job { int k, v, st, i; };
    struct Out { Img im; int ax = 0, ay = 0; float sc = 1.f; };
    std::vector<Job> jobs;
    for (int k = 0; k < MK_COUNT; k++)
        for (int v = 0; v < AV_VIEWS; v++)
            for (int st = 0; st < MS_COUNT; st++) {
                if (st == MS_CAST && !monsterHasCast(k)) continue;
                for (int i = 0; i < MS_FRAMES[st]; i++) jobs.push_back({k, v, st, i});
            }
    std::vector<Out> outs(jobs.size());
    auto gen = [&](size_t j) {
        const Job& jb = jobs[j];
        MonsterImg mi;
        paintMonsterFrame(jb.k, jb.v, jb.st, jb.i, mi);
        outs[j].im = std::move(mi.im); outs[j].ax = mi.ax; outs[j].ay = mi.ay; outs[j].sc = mi.sc;
    };
    std::atomic<size_t> next{0};
    const unsigned nt = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    std::vector<std::thread> th;
    for (unsigned t = 0; t < nt; t++)
        th.emplace_back([&] { for (size_t j; (j = next++) < jobs.size();) gen(j); });
    for (auto& t : th) t.join();
    for (size_t j = 0; j < jobs.size(); j++) {
        const Job& jb = jobs[j];
        Spr sp = makeSpr(r, outs[j].im, outs[j].ax, outs[j].ay, true);
        sp.sc = outs[j].sc;
        bank.s[jb.k][jb.v][jb.st][jb.i] = sp;
    }
    bank.built = true;
}
