// dom_art.hpp - the farm animals of the Homestead, drawn with the same 3D model renderer as the wildlife (model3d.hpp / quad3d.hpp):
// cow, sheep, pig and goat reuse the four-legged skeleton in their own colours and proportions (wool puffs, spots, horns, beard are
// added as extra ellipsoids); chicken, duck and rabbit are small models of their own. Every animal is rendered in 8 directions and
// three clips: idle (2 poses), walk (4) and eat (2). Babies are the same sprites drawn smaller.
#pragma once
#include <atomic>
#include <thread>
#include "homestead.hpp"
#include "sprites_rig.hpp"

enum DomState { DS_IDLE, DS_WALK, DS_EAT, DS_COUNT };
constexpr int DS_FRAMES[DS_COUNT] = {2, 4, 2};
constexpr int DS_MAXF = 4;

struct DomBank {
    Spr s[DA_COUNT][AV_VIEWS][DS_COUNT][DS_MAXF];
    bool built = false;
    const Spr& get(int k, int view, int st, int fr) const {
        fr = std::max(0, std::min(DS_FRAMES[st] - 1, fr));
        return s[k][std::max(0, std::min((int)AV_VIEWS - 1, view))][st][fr];
    }
};

namespace dart2 {
using m3::V;
using m3::Prim;
using m3::Mat;

inline m3::Q3 scaled(m3::Q3 q, float k) {  // every length of the skeleton scaled (the pixel scale and the anchor are kept)
    q.hipX *= k; q.shX *= k; q.backZ *= k;
    q.rumpR = q.rumpR * k; q.ribR = q.ribR * k; q.chestR = q.chestR * k;
    q.chestLift *= k; q.neckLen *= k; q.neckR0 *= k; q.neckR1 *= k;
    q.skull = q.skull * k; q.muzzleLen *= k; q.muzzleW *= k; q.muzzleH *= k;
    q.earH *= k; q.earW *= k; q.tailLen *= k; q.tailR *= k;
    q.fL1 *= k; q.fL2 *= k; q.fPaw *= k; q.hL1 *= k; q.hL2 *= k; q.hPaw *= k; q.legR *= k; q.stance *= k;
    return q;
}
inline m3::Q3 cowQ() {
    m3::Q3 q = scaled(boarQ3(), 1.22f);
    q.chestLift = 1.0f; q.neckLen = 5.0f; q.neckAng0 = -0.12f; q.skull = {6.6f, 3.4f, 3.8f}; q.muzzleLen = 3.6f; q.muzzleW = 3.1f; q.muzzleH = 2.9f;
    q.earH = 3.4f; q.earW = 2.6f; q.earSplay = 1.5f; q.tailLen = 9.f; q.tailR = 0.8f; q.tailCurl = 0.02f; q.tailAng0 = 1.2f;
    q.backZ = 15.5f; q.fL1 = 5.5f; q.fL2 = 5.6f; q.hL1 = 5.6f; q.hL2 = 5.8f; q.flags = m3::QF_HOOFED; q.legDark = 0.2f;
    return q;
}
inline m3::Q3 pigQ() {
    m3::Q3 q = scaled(boarQ3(), 0.95f);
    q.flags = m3::QF_HOOFED; q.chestLift = 0.6f; q.neckLen = 2.4f; q.muzzleLen = 4.4f; q.muzzleW = 2.6f; q.muzzleH = 2.3f;
    q.earH = 4.4f; q.earW = 3.6f; q.earSplay = 0.7f; q.tailLen = 4.4f; q.tailCurl = 0.55f; q.tailSegs = 4; q.tailR = 0.7f;
    q.backZ = 9.8f; q.legDark = 0.f;
    return q;
}
inline m3::Q3 sheepQ() {
    m3::Q3 q = scaled(boarQ3(), 1.0f);
    q.flags = m3::QF_HOOFED; q.chestLift = 0.8f; q.neckLen = 4.0f; q.neckAng0 = 0.35f; q.skull = {4.2f, 2.5f, 2.8f}; q.muzzleLen = 3.0f; q.muzzleW = 1.8f; q.muzzleH = 2.0f;
    q.earH = 3.6f; q.earW = 2.2f; q.earSplay = 1.55f; q.tailLen = 3.0f; q.tailR = 1.2f; q.tailCurl = 0.f; q.tailAng0 = 0.9f;
    q.backZ = 13.f; q.fL1 = 4.6f; q.fL2 = 4.4f; q.hL1 = 4.6f; q.hL2 = 4.6f; q.legDark = 0.f;
    return q;
}
inline m3::Q3 goatQ() {
    m3::Q3 q = scaled(deerQ3(), 0.78f);
    q.chestLift = 0.8f; q.neckLen = 5.4f; q.skull = {3.9f, 2.0f, 2.3f}; q.muzzleLen = 3.4f; q.muzzleW = 1.5f; q.muzzleH = 1.7f;
    q.earH = 3.4f; q.earW = 1.8f; q.earSplay = 1.5f; q.tailLen = 3.0f; q.tailR = 1.0f; q.tailCurl = 0.f; q.tailAng0 = 1.2f;
    q.backZ = 18.f; q.legDark = 0.3f;
    return q;
}
inline QuadSpec specFor(int kind) {
    QuadSpec s = specBoar();
    s.tusks = false; s.ridge = false;
    switch (kind) {
        case DA_COW: s.body = HEX(0xf2eee6); s.light = HEX(0xffffff); s.belly = HEX(0xe2dacc); s.dark = HEX(0x2e2824); s.snout = HEX(0xe8b0a0); s.eye = HEX(0x241a14); s.hoof = HEX(0x3a3028); break;
        case DA_SHEEP: s.body = HEX(0xf4f1ea); s.light = HEX(0xffffff); s.belly = HEX(0xe4e0d8); s.dark = HEX(0x4a4448); s.snout = HEX(0x6a6064); s.eye = HEX(0xe8c850); s.hoof = HEX(0x2a2428); break;
        case DA_PIG: s.body = HEX(0xf2a8a2); s.light = HEX(0xfac4be); s.belly = HEX(0xfad0ca); s.dark = HEX(0xc47a74); s.snout = HEX(0xe88e88); s.eye = HEX(0x241a14); s.hoof = HEX(0x6a4a44); break;
        default: s.body = HEX(0xd8ccb4); s.light = HEX(0xece4d2); s.belly = HEX(0xf2ecdc); s.dark = HEX(0x6a5a48); s.snout = HEX(0xc4b49c); s.eye = HEX(0xe8c850); s.hoof = HEX(0x3a3028); break;  // goat
    }
    return s;
}

// the head frame of a quad model, copied from buildQuad's formulas (standing pose), to hang horns / a beard on it
struct HeadFrame { V Hc, fh, uh; };
inline HeadFrame headFrame(const m3::Q3& Q, const BPose& P) {
    float hipZ = Q.backZ + P.bob * 0.8f, shZ = hipZ + Q.chestLift;
    const V Nb{Q.shX + Q.chestR.x * 0.5f, 0, shZ + Q.chestR.z * 0.55f};
    const float ang = std::max(-1.25f, std::min(1.45f, Q.neckAng0 + 0.11f * P.headDz));
    const V Hc = Nb + V{std::cos(ang), 0, std::sin(ang)} * Q.neckLen + V{P.headDx * 0.7f, 0, 0};
    const float hang = ang - 0.52f;
    return {Hc, V{std::cos(hang), 0, std::sin(hang)}, V{-std::sin(hang), 0, std::cos(hang)}};
}
inline void quadExtras(int kind, const m3::Q3& Q, const BPose& P, std::vector<Prim>& out) {
    using namespace m3;
    const float hipZ = Q.backZ + P.bob * 0.8f, shZ = hipZ + Q.chestLift;
    const HeadFrame H = headFrame(Q, P);
    const V Y{0, 1, 0};
    if (kind == DA_COW) {  // black patches on the flanks and back, a bell would be cute but the pixels are few
        const float xs[3] = {Q.hipX + 1.f, (Q.hipX + Q.shX) * 0.5f + 1.5f, Q.shX - 1.f};
        for (int i = 0; i < 3; i++)
            for (int s = -1; s <= 1; s += 2)
                out.push_back(oriented({xs[i] + (i == 1 ? 0.8f : 0.f), s * Q.ribR.y * (0.92f + 0.03f * i), hipZ + (shZ - hipZ) * (0.2f + 0.3f * i) + 0.5f * ((i + s) & 1)}, V{1, 0, 0}, Y, {2.6f + (i & 1) * 1.2f, 0.9f, 2.4f + (i == 1) * 1.1f}, MT_DARK));
        out.push_back(oriented({Q.shX - 0.5f, 0, shZ + Q.ribR.z * 0.9f}, V{1, 0, 0}, Y, {3.2f, 2.2f, 1.1f}, MT_DARK));
        for (int s = -1; s <= 1; s += 2)  // small horn nubs
            out.push_back(along(H.Hc + H.uh * (Q.skull.z * 0.8f) + Y * (s * Q.skull.y * 0.6f), H.Hc + H.uh * (Q.skull.z * 0.8f + 2.2f) + Y * (s * (Q.skull.y * 0.6f + 1.4f)), 0.7f, MT_IVORY, 1.f));
        out.push_back(oriented(H.Hc + H.fh * (Q.skull.x * 0.8f + Q.muzzleLen * 0.5f) + H.uh * (-Q.skull.z * 0.25f), H.fh, Y, {Q.muzzleLen * 0.6f, Q.muzzleH * 1.05f, Q.muzzleW * 1.05f}, MT_SNOUT));
    } else if (kind == DA_SHEEP) {  // the fleece: big overlapping puffs over the torso, a topknot and fluffy legs' tops
        const int N = 9;
        for (int i = 0; i < N; i++) {
            const float t = i / (float)(N - 1);
            const float x = Q.hipX - 1.f + (Q.shX - Q.hipX + 2.f) * t;
            const float z = hipZ + (shZ - hipZ) * t;
            for (int s = -1; s <= 1; s += 2)
                out.push_back(ball({x, s * Q.ribR.y * 0.55f, z + Q.ribR.z * (0.35f + 0.18f * ((i + (s > 0)) & 1))}, {3.6f, 3.4f, 3.4f}, MT_LIGHT));
            out.push_back(ball({x, 0, z + Q.ribR.z * 0.8f}, {3.6f, 3.2f, 2.8f}, MT_LIGHT));
        }
        out.push_back(ball({Q.hipX - Q.rumpR.x * 0.7f, 0, hipZ + 1.5f}, {3.6f, 4.2f, 4.4f}, MT_LIGHT));
        out.push_back(ball(H.Hc + H.uh * (Q.skull.z * 0.85f) - H.fh * 0.3f, {2.6f, 2.4f, 2.0f}, MT_LIGHT));  // topknot
        out.push_back(oriented(H.Hc + H.fh * 0.5f, H.fh, Y, {Q.skull.x * 0.95f, Q.skull.z * 0.95f, Q.skull.y * 0.95f}, MT_DARK));  // the dark face
        out.push_back(oriented(H.Hc + H.fh * (Q.skull.x * 0.78f + Q.muzzleLen * 0.5f) - H.uh * (Q.skull.z * 0.28f), H.fh, Y, {Q.muzzleLen * 0.55f + 1.f, Q.muzzleH, Q.muzzleW}, MT_DARK));
    } else if (kind == DA_GOAT) {  // swept-back horns and a beard
        const float hs = 1.f;
        for (int s = -1; s <= 1; s += 2) {
            const V a0 = H.Hc + H.uh * (Q.skull.z * 0.85f) + Y * (s * Q.skull.y * 0.5f) - H.fh * 0.2f;
            const V a1 = a0 + H.uh * (3.4f * hs) - H.fh * 1.2f + Y * (s * 0.9f);
            const V a2 = a1 + H.uh * (2.4f * hs) - H.fh * 2.8f + Y * (s * 0.8f);
            out.push_back(along(a0, a1, 0.8f, MT_IVORY, 1.f));
            out.push_back(along(a1, a2, 0.62f, MT_IVORY, 1.f));
        }
        out.push_back(ball(H.Hc + H.fh * (Q.skull.x * 0.75f + Q.muzzleLen * 0.6f) - H.uh * (Q.skull.z * 0.9f), {0.8f, 0.9f, 2.2f}, MT_DARK));  // beard
    } else if (kind == DA_PIG) {
        out.push_back(oriented(H.Hc + H.fh * (Q.skull.x * 0.78f + Q.muzzleLen + 0.2f) - H.uh * (Q.skull.z * 0.14f), H.fh, Y, {0.8f, 1.9f, 1.6f}, MT_NOSE));  // the disc of the snout
    }
}

// ---- small animals: models of their own (units: the chicken's body is ~10 long)
enum { BM_BODY, BM_LIGHT, BM_DARK, BM_BEAK, BM_EYE, BM_RED, BM_LEG, BM_HEAD, BM_COUNT };
inline std::vector<Mat> smallMats(int kind) {
    std::vector<Mat> m(BM_COUNT);
    auto M = [](u32 c, float fur = 0.f) { return Mat{c, shadeC(c, 0.8f), shadeC(c, 1.12f), fur, false}; };
    if (kind == DA_CHICKEN) {
        m[BM_BODY] = M(HEX(0xe8e0d2), 0.5f); m[BM_LIGHT] = M(HEX(0xfaf6ee), 0.5f); m[BM_DARK] = M(HEX(0xc88a52), 0.5f);
        m[BM_BEAK] = M(HEX(0xf2b030)); m[BM_RED] = M(HEX(0xd8322a)); m[BM_LEG] = M(HEX(0xe8a838)); m[BM_HEAD] = m[BM_BODY];
    } else if (kind == DA_DUCK) {
        m[BM_BODY] = M(HEX(0x9a7a56), 0.5f); m[BM_LIGHT] = M(HEX(0xd8c4a0), 0.5f); m[BM_DARK] = M(HEX(0x5a4430), 0.5f);
        m[BM_BEAK] = M(HEX(0xf0c030)); m[BM_RED] = M(HEX(0x3a8a5a)); m[BM_LEG] = M(HEX(0xf08a28)); m[BM_HEAD] = M(HEX(0x2e7a50), 0.3f);
    } else {  // rabbit
        m[BM_BODY] = M(HEX(0xb8a088), 0.8f); m[BM_LIGHT] = M(HEX(0xf4efe6), 0.8f); m[BM_DARK] = M(HEX(0x7a6450), 0.8f);
        m[BM_BEAK] = M(HEX(0xe8a0a0)); m[BM_RED] = M(HEX(0xe8a0a0)); m[BM_LEG] = M(HEX(0xa08868), 0.8f); m[BM_HEAD] = m[BM_BODY];
    }
    m[BM_EYE] = Mat{HEX(0x1a1410), 0, 0, 0.f, true};
    return m;
}
inline void bird(std::vector<Prim>& o, int kind, int st, int f) {
    using namespace m3;
    const bool duck = kind == DA_DUCK;
    const float ph = st == DS_WALK ? f * 1.5708f : 0.f;
    const float bob = st == DS_WALK ? 0.55f * std::fabs(std::sin(ph)) : 0.f;
    const float peck = st == DS_EAT ? (f ? 1.f : 0.35f) : 0.f;
    const float bz = duck ? 4.6f : 7.2f;
    const float nz = peck > 0 ? 0.f : 1.f;
    o.push_back(ball({0, 0, bz + bob}, duck ? V{6.6f, 4.1f, 3.8f} : V{5.4f, 3.7f, 4.3f}, BM_BODY));
    o.push_back(ball({duck ? 3.0f : 2.7f, 0, bz - 0.5f + bob}, duck ? V{3.8f, 3.4f, 3.4f} : V{3.4f, 3.2f, 3.8f}, BM_LIGHT));  // breast
    o.push_back(ball({-0.5f, 0, bz - 1.4f + bob}, {4.6f, 3.2f, 2.2f}, BM_LIGHT));                                              // belly
    for (int s = -1; s <= 1; s += 2) o.push_back(oriented({-0.6f, s * (duck ? 3.7f : 3.5f), bz + 0.5f + bob}, V{1, 0, 0}, V{0, 1, 0}, {duck ? 4.4f : 3.6f, 1.0f, duck ? 2.4f : 2.8f}, BM_DARK));  // wings
    // tail: fanned feathers pointing up and back
    for (int i = -1; i <= 1; i++) o.push_back(oriented({-(duck ? 6.2f : 5.4f), i * 0.9f, bz + (duck ? 1.4f : 3.6f) + bob}, V{-0.6f, 0, 0.8f}, V{0, 1, 0}, {duck ? 2.4f : 3.6f, 0.8f, duck ? 1.2f : 1.6f}, i == 0 ? BM_DARK : BM_BODY));
    // neck and head
    const float hx = (duck ? 5.6f : 4.8f) + peck * 2.6f + (st == DS_WALK ? 0.7f * std::sin(ph * 2.f) : 0.f);
    const float hz = (duck ? 9.4f : 13.6f) - peck * (duck ? 4.4f : 8.2f) + bob;
    const V nb{duck ? 4.2f : 3.4f, 0, bz + 1.6f + bob}, hc{hx, 0, hz};
    o.push_back(along(nb, hc, duck ? 1.9f : 1.7f, BM_BODY, 1.25f));
    o.push_back(ball(hc, duck ? V{2.6f, 2.1f, 2.1f} : V{2.5f, 2.0f, 2.2f}, BM_HEAD));
    if (duck) o.push_back(ball(hc + V{0, 0, -1.7f}, {2.2f, 2.2f, 0.7f}, BM_LIGHT));  // white collar
    o.push_back(oriented(hc + V{duck ? 3.2f : 2.5f, 0, duck ? -0.6f : -0.5f}, V{1, 0, 0}, V{0, 0, 1}, duck ? V{2.4f, 1.5f, 0.8f} : V{1.7f, 0.9f, 0.9f}, BM_BEAK));  // beak / bill
    if (!duck) {
        o.push_back(ball(hc + V{0.3f, 0, 2.3f}, {1.1f, 0.6f, 1.5f}, BM_RED));          // comb
        o.push_back(ball(hc + V{0.4f, 0, 2.7f}, {0.9f, 0.5f, 1.0f}, BM_RED));
        o.push_back(ball(hc + V{2.2f, 0, -1.9f}, {0.8f, 0.6f, 1.3f}, BM_RED));          // wattle
    }
    for (int s = -1; s <= 1; s += 2) o.push_back(ball(hc + V{1.0f, s * 1.75f, 0.5f}, {0.6f, 0.5f, 0.6f}, BM_EYE));
    // legs: thin, alternating
    for (int s = -1; s <= 1; s += 2) {
        const float sw = st == DS_WALK ? std::sin(ph * 2.f + (s > 0 ? 0.f : 3.14159f)) : 0.f;
        const float lift = st == DS_WALK ? std::max(0.f, std::cos(ph * 2.f + (s > 0 ? 0.f : 3.14159f))) * 1.3f : 0.f;
        const V top{0.5f, s * 1.9f, bz - 2.2f + bob}, foot{0.8f + sw * 2.2f, s * 1.9f, 0.5f + lift};
        const float ll = duck ? 0.9f : 0.7f;
        o.push_back(along(top, foot, ll, BM_LEG, 1.1f));
        o.push_back(ball(foot + V{0.9f, 0, 0}, duck ? V{2.2f, 1.7f, 0.5f} : V{1.8f, 1.0f, 0.5f}, BM_LEG));
    }
}
inline void rabbit(std::vector<Prim>& o, int st, int f) {
    using namespace m3;
    static const float AIR[4] = {0.f, 2.4f, 3.0f, 1.0f}, STRETCH[4] = {0.f, 1.f, 0.6f, 0.f};
    const float air = st == DS_WALK ? AIR[f] : 0.f, str = st == DS_WALK ? STRETCH[f] : 0.f;
    const float peck = st == DS_EAT ? (f ? 1.f : 0.4f) : 0.f;
    const float bz = 4.2f + air;
    o.push_back(ball({0, 0, bz}, {4.6f + str * 0.8f, 3.1f, 3.4f}, BM_BODY));
    o.push_back(ball({-2.8f, 0, bz + 0.2f}, {3.2f, 3.2f, 3.4f}, BM_BODY));
    for (int s = -1; s <= 1; s += 2) o.push_back(ball({-2.6f - str * 1.4f, s * 2.3f, bz - 1.0f}, {3.0f + str, 1.5f, 2.6f}, BM_BODY));  // haunches
    o.push_back(ball({-5.6f, 0, bz + 0.8f}, {1.9f, 1.9f, 1.9f}, BM_LIGHT));  // the cotton tail
    o.push_back(ball({2.0f, 0, bz - 1.2f}, {3.0f, 2.4f, 2.0f}, BM_LIGHT));
    const V hc{4.6f + peck * 1.4f + str * 0.8f, 0, 6.4f + air - peck * 3.2f};
    o.push_back(along({2.8f, 0, bz + 1.2f}, hc, 2.1f, BM_BODY, 1.2f));
    o.push_back(ball(hc, {2.7f, 2.1f, 2.3f}, BM_HEAD));
    o.push_back(ball(hc + V{2.0f, 0, -0.5f}, {1.3f, 1.2f, 1.1f}, BM_LIGHT));
    o.push_back(ball(hc + V{3.1f, 0, 0.0f}, {0.6f, 0.7f, 0.6f}, BM_BEAK));  // nose
    for (int s = -1; s <= 1; s += 2) {
        o.push_back(ball(hc + V{1.0f, s * 1.85f, 0.6f}, {0.6f, 0.5f, 0.6f}, BM_EYE));
        const float lean = st == DS_WALK ? -0.5f * str : 0.f;
        o.push_back(along(hc + V{-0.6f, s * 0.9f, 1.6f}, hc + V{-1.4f + lean, s * 1.5f, 6.8f - peck * 1.2f}, 0.95f, BM_BODY, 1.15f, 0.45f));
        o.push_back(along(hc + V{-0.5f, s * 0.9f, 2.0f}, hc + V{-1.3f + lean, s * 1.45f, 6.2f - peck * 1.2f}, 0.5f, BM_RED, 1.0f, 0.4f));
        // legs: tucked haunch feet, reaching forelegs
        o.push_back(ball({-3.0f - str * 2.2f, s * 2.4f, 0.8f + air * 0.6f}, {3.0f, 1.0f, 0.8f}, BM_LEG));
        o.push_back(along({1.8f, s * 1.5f, bz - 1.6f}, {2.6f + str * 1.6f, s * 1.5f, 0.6f + air * (st == DS_WALK ? 0.8f : 0.f)}, 0.9f, BM_LEG, 1.1f));
    }
}

struct DomImg { Img im; int ax = 0, ay = 0; float sc = 1.f; };
inline void paint(int kind, int v, int st, int f, DomImg& o, float* sideLen = nullptr) {
    static const float ALPHA[AV_VIEWS] = {1.5708f, 0.7854f, 0.f, -0.7854f, -1.5708f, -2.3562f, 3.14159f, 2.3562f};
    std::vector<Prim> prims;
    std::vector<Mat> ms;
    float pxu, ox, oy;
    int cell;
    if (kind == DA_COW || kind == DA_SHEEP || kind == DA_PIG || kind == DA_GOAT) {
        const m3::Q3 Q = kind == DA_COW ? cowQ() : kind == DA_SHEEP ? sheepQ() : kind == DA_PIG ? pigQ() : goatQ();
        BPose P = quadPose(AN_IDLE, st == DS_IDLE ? f * 2 : 0);
        if (st == DS_WALK) {
            P = quadPose(AN_WALK, 0);
            P.phase = f * 6.28318f / 4.f;
            P.bob = 0.45f * std::cos(P.phase * 2.f);
            P.headDz = 0.3f * std::cos(P.phase * 2.f);
            P.tail = (f & 1) ? 1.f : 0.f;
        } else if (st == DS_EAT) {
            P.headDz = -(kind == DA_PIG ? 5.5f : 8.f) - (f ? 0.8f : 0.f);
            P.headDx = f ? 1.2f : 0.f;
            P.pitch = -0.4f;
        }
        m3::buildQuad(Q, P, 1.f, 1.f, 1.f, false, prims);
        quadExtras(kind, Q, P, prims);
        ms = m3::quadMats(specFor(kind), 0.f, Q.legDark);
        pxu = Q.pxu; ox = Q.ox; oy = Q.oy; cell = 64;
        if (kind == DA_COW) { pxu = 1.0f; cell = 72; ox = 36; oy = 56; }
    } else {
        pxu = kind == DA_RABBIT ? 3.2f : 3.4f; cell = 56; ox = 28; oy = 46;
        if (kind == DA_RABBIT) rabbit(prims, st, f); else bird(prims, kind, st, f);
        ms = smallMats(kind);
    }
    o.im = m3::render(prims, ms, ALPHA[v], 0.70f, pxu, cell, cell, ox, oy, 3);
    styleActor(o.im, OUT, 0);
    int x0 = o.im.w, y0 = o.im.h, x1 = -1, y1 = -1;
    for (int y = 0; y < o.im.h; y++)
        for (int x = 0; x < o.im.w; x++)
            if (cA(o.im.p[(size_t)y * o.im.w + x])) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
    o.ax = (int)ox; o.ay = (int)oy;
    if (sideLen) *sideLen = x1 >= x0 ? (float)(x1 - x0 + 1) : 1.f;
    if (x1 >= x0 && y1 >= y0) {
        Img c(x1 - x0 + 1, y1 - y0 + 1);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) c.p[(size_t)(y - y0) * c.w + (x - x0)] = o.im.p[(size_t)y * o.im.w + x];
        o.im = c; o.ax -= x0; o.ay -= y0;
    }
}
// target length of the adult in game pixels (side view); the baby is drawn at a fraction of this
inline float targetLen(int k) {
    static const float T[DA_COUNT] = {12.f, 31.f, 22.f, 23.f, 21.f, 13.f, 10.f};
    return T[k];
}
}  // namespace dart2

inline void buildDomBank(SDL_Renderer* r, DomBank& bank) {
    struct Job { int k, v, st, f; };
    struct Out { dart2::DomImg d; };
    std::vector<Job> jobs;
    for (int k = 0; k < DA_COUNT; k++)
        for (int v = 0; v < AV_VIEWS; v++)
            for (int st = 0; st < DS_COUNT; st++)
                for (int f = 0; f < DS_FRAMES[st]; f++) jobs.push_back({k, v, st, f});
    std::vector<Out> outs(jobs.size());
    float side[DA_COUNT] = {0};
    for (int k = 0; k < DA_COUNT; k++) {  // the scale of each species comes from its side view
        dart2::DomImg tmp;
        dart2::paint(k, 2, DS_IDLE, 0, tmp, &side[k]);
    }
    std::atomic<size_t> next{0};
    const unsigned nt = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    std::vector<std::thread> th;
    for (unsigned t = 0; t < nt; t++)
        th.emplace_back([&] {
            for (size_t j; (j = next++) < jobs.size();) dart2::paint(jobs[j].k, jobs[j].v, jobs[j].st, jobs[j].f, outs[j].d);
        });
    for (auto& t : th) t.join();
    for (size_t j = 0; j < jobs.size(); j++) {
        const Job& jb = jobs[j];
        Spr sp = makeSpr(r, outs[j].d.im, outs[j].d.ax, outs[j].d.ay, true);
        sp.sc = dart2::targetLen(jb.k) / std::max(1.f, side[jb.k]);
        bank.s[jb.k][jb.v][jb.st][jb.f] = sp;
    }
    bank.built = true;
}
