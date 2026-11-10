// quad3d.hpp - a posed four-legged 3D animal for the model renderer (model3d.hpp).
//
// The skeleton is real: a spine that bends with the pitch of the body, a neck that rises from the shoulders and carries a skull with a muzzle
// and a hinged lower jaw, ears, a jointed tail, and four legs solved with two-bone inverse kinematics (shoulder -> elbow -> wrist, hip -> stifle ->
// hock) so the feet land where the gait puts them and the knees bend the right way. Each bone is drawn as a few ellipsoids (muscle masses on
// the shoulder, ribs, haunch), coloured from the species' QuadSpec so rarity tiers and second forms (the arctic wolf) recolour for free.
// Units: 1 unit = one pixel of the animal at its in-game size; X forward, Y to the animal's left, Z up.
#pragma once
#include "animals/model3d.hpp"

namespace m3 {

enum { MT_BODY, MT_LIGHT, MT_DARK, MT_SNOUT, MT_NOSE, MT_EYE, MT_INNER, MT_LEG, MT_IVORY, MT_ANTLER, MT_COUNT };
enum { QF_TUSKS = 1, QF_HOOFED = 4, QF_MANE = 8 };

struct Q3 {
    float hipX, shX, backZ;     // hip and shoulder x, height of the spine at the hips
    V rumpR, ribR, chestR;      // torso radii (x, y = half width, z = half height)
    float chestLift;            // the withers stand this much higher than the hips
    float neckLen, neckAng0, neckR0, neckR1;
    V skull;                    // x = length, y = half width, z = half height
    float muzzleLen, muzzleW, muzzleH;
    float earH, earW, earSplay;
    float tailLen, tailR, tailAng0, tailCurl;
    int tailSegs;
    float fL1, fL2, fPaw, hL1, hL2, hPaw, legR, stance;
    float pxu, ox, oy;          // pixels per unit, where the ground point under the animal lands in the 64x64 canvas
    int flags;                  // QF_*
    float legDark;              // how much darker the legs are than the body (0 .. 1)
};

inline void ik2(V J, V T, float L1, float L2, float bend, V& knee, V& ankle) {
    const float dx = T.x - J.x, dz = T.z - J.z;
    float d = std::sqrt(dx * dx + dz * dz);
    d = std::max(std::fabs(L1 - L2) + 0.05f, std::min(L1 + L2 - 0.05f, d));
    const float th0 = std::atan2(dz, dx);
    const float cosA = std::max(-1.f, std::min(1.f, (L1 * L1 + d * d - L2 * L2) / (2.f * L1 * d)));
    const float A = std::acos(cosA);
    const V k1{J.x + L1 * std::cos(th0 + A), J.y, J.z + L1 * std::sin(th0 + A)};
    const V k2{J.x + L1 * std::cos(th0 - A), J.y, J.z + L1 * std::sin(th0 - A)};
    knee = (bend > 0) == (k1.x > k2.x) ? k1 : k2;
    ankle = V{J.x + std::cos(th0) * d, J.y, J.z + std::sin(th0) * d};
}

inline std::vector<Mat> quadMats(const QuadSpec& sp, float tint, float legDark = 0.f) {
    auto T = [&](u32 c) { return tint > 0.f ? mixc(c, HEX(0xe03a30), std::min(1.f, tint)) : c; };
    std::vector<Mat> m(MT_COUNT);
    m[MT_BODY] = {T(sp.body), T(sp.belly), T(mixc(sp.body, sp.dark, 0.38f)), 0.34f, false};
    m[MT_LIGHT] = {T(sp.light), T(sp.belly), T(sp.light), 0.38f, false};
    m[MT_DARK] = {T(sp.dark), T(sp.dark), T(sp.dark), 0.25f, false};
    m[MT_SNOUT] = {T(sp.snout), T(mixc(sp.snout, sp.belly, 0.5f)), T(mixc(sp.snout, sp.body, 0.45f)), 0.f, false};
    m[MT_NOSE] = {T(sp.hoof), T(sp.hoof), T(sp.hoof), 0.f, false};
    m[MT_EYE] = {tint > 0.5f ? HEX(0xffe0d8) : sp.eye, 0, 0, 0.f, true};
    m[MT_INNER] = {T(mixc(sp.dark, HEX(0xc08878), 0.45f)), 0, 0, 0.f, false};
    const u32 leg = mixc(mixc(sp.body, sp.light, 0.55f), sp.dark, legDark);
    m[MT_LEG] = {T(leg), T(legDark > 0.f ? leg : sp.belly), T(leg), 0.22f, false};
    m[MT_IVORY] = {T(HEX(0xf2ead2)), T(HEX(0xc8bea0)), T(HEX(0xfffaf0)), 0.f, false};
    m[MT_ANTLER] = {T(HEX(0xd8a284)), T(HEX(0xba7d5b)), T(HEX(0xe8bd9c)), 0.f, false};
    return m;
}

// Builds the posed model. `strideMul` etc. are the species' gait scale (RigShape).
inline void buildQuad(const Q3& Q, const BPose& P, float strideMul, float liftMul, float airMul, bool antlers, std::vector<Prim>& out) {
    const float lie = std::min(1.f, std::max(P.lie, P.roll / 1.5708f));
    const float breathe = 1.f + 0.03f * P.breathe, sq = 1.f - 0.07f * P.squash;
    const float pitchA = P.pitch * 0.22f;
    const float air = P.air * airMul * 0.9f;
    float hipZ = Q.backZ * (1.f - lie) + lie * (Q.rumpR.z * 0.78f) + P.bob * 0.8f + air;
    float shZ = hipZ + Q.chestLift * (1.f - 0.5f * lie);
    const float half = (Q.shX - Q.hipX) * 0.5f;
    hipZ -= std::sin(pitchA) * half;
    shZ += std::sin(pitchA) * half;
    auto B = [&](V c, V r, int mat) { out.push_back(ball(c, r, mat)); };

    // ---- torso
    B({Q.hipX - 0.5f, 0, hipZ}, {Q.rumpR.x, Q.rumpR.y * breathe, Q.rumpR.z * sq}, MT_BODY);
    B({(Q.hipX + Q.shX) * 0.5f + 0.5f, 0, (hipZ + shZ) * 0.5f - 0.3f}, {Q.ribR.x, Q.ribR.y * breathe, Q.ribR.z * sq}, MT_BODY);
    B({Q.shX, 0, shZ - 0.3f}, {Q.chestR.x, Q.chestR.y * breathe, Q.chestR.z * sq}, MT_BODY);
    B({Q.shX + 1.2f, 0, shZ - Q.chestR.z * 0.5f}, {Q.chestR.x * 0.8f, Q.chestR.y * 0.8f, Q.chestR.z * 0.62f}, MT_LIGHT);
    for (int s = -1; s <= 1; s += 2)
        B({Q.shX - 0.6f, s * Q.chestR.y * 0.80f, shZ + Q.chestR.z * 0.52f}, {3.2f, 1.5f, 3.0f}, MT_BODY);  // shoulder blades

    if (Q.flags & QF_MANE)  // bristles along the spine
        for (int i = 0; i < 9; i++) {
            const float t = i / 8.f;
            B({Q.hipX + (Q.shX - Q.hipX + 2.f) * t, 0, hipZ + (shZ - hipZ) * t + Q.ribR.z * 0.92f + 0.5f}, {1.7f, 1.0f, 2.5f}, MT_DARK);
        }

    // ---- legs: LH, LF, RH, RF
    const bool moving = P.stride > 0.01f, gallop = P.stride * strideMul > 4.6f;
    static const float WALK[4] = {0.f, 0.25f, 0.5f, 0.75f}, RUN[4] = {0.5f, 0.0f, 0.6f, 0.1f};
    const float A = moving ? P.stride * strideMul * 1.15f : 0.f, Lf = moving ? P.lift * liftMul * 0.85f : 0.f;
    const float limp = std::max(0.f, P.limp);
    for (int i = 0; i < 4; i++) {
        const bool front = (i & 1) != 0;
        const float s = i < 2 ? 1.f : -1.f;
        const float ph = P.phase + (gallop ? RUN[i] : WALK[i]) * 6.28318f;
        const float L1 = front ? Q.fL1 : Q.hL1, L2 = front ? Q.fL2 : Q.hL2, paw = front ? Q.fPaw : Q.hPaw;
        const V J = front ? V{Q.shX + 0.3f, s * Q.stance, shZ - Q.chestR.z * 0.55f} : V{Q.hipX, s * Q.stance, hipZ - Q.rumpR.z * 0.5f};
        float fx = (front ? Q.shX + 0.4f : Q.hipX - 0.2f) + A * std::cos(ph) * (front ? 1.f : 0.9f);
        float fz = air + Lf * std::max(0.f, -std::sin(ph));
        float footY = s * Q.stance;
        if (lie > 0.f) {
            fx += front ? 6.5f * lie : 2.0f * lie;
            fz = air;
        }
        V ankleT{fx + (front ? -0.3f : -1.2f), footY, fz + paw};
        if (limp > 0.f) {  // death: the legs hang loose
            ankleT = lerp(ankleT, V{J.x + (front ? 2.5f : -2.0f), footY + s * 2.5f, J.z - (L1 + L2) * 0.75f}, std::min(1.f, limp));
            fz = ankleT.z - paw;
            fx = ankleT.x + (front ? 0.3f : 1.2f);
        }
        V knee, ankle;
        ik2(J, ankleT, L1, L2, front ? -1.f : 1.f, knee, ankle);
        out.push_back(along(J, knee, Q.legR * (front ? 1.35f : 1.5f), MT_LEG));
        out.push_back(along(knee, ankle, Q.legR * 0.95f, MT_LEG));
        const V pawC{fx + 0.9f, footY, fz + 0.9f};
        out.push_back(along(ankle, pawC + V{0, 0, 0.6f}, Q.legR * 0.8f, MT_LEG, 1.0f));
        if (Q.flags & QF_HOOFED) B(pawC + V{0.2f, 0, -0.1f}, {1.25f, 0.95f, 1.1f}, MT_NOSE);
        else B(pawC, {1.8f, 1.1f, 0.95f}, MT_LEG);
        if (!front) {  // haunch: the thigh muscle
            const V top = J + V{-0.4f, 0, 1.6f};
            out.push_back(along(top, knee, Q.rumpR.z * 0.62f, MT_BODY, 1.0f, 0.62f));
        } else
            B(lerp(J, knee, 0.3f) + V{0, 0, 1.0f}, {2.8f, 1.5f, 3.2f}, MT_BODY);
    }

    // ---- neck and head
    const V Nb{Q.shX + Q.chestR.x * 0.5f, 0, shZ + Q.chestR.z * 0.55f};
    const float ang = std::max(-1.25f, std::min(1.45f, Q.neckAng0 + 0.11f * P.headDz + 0.38f * P.pitch));
    const V Hc = Nb + V{std::cos(ang), 0, std::sin(ang)} * Q.neckLen + V{P.headDx * 0.7f, 0, 0};
    const V mid = lerp(Nb, Hc, 0.5f);
    out.push_back(along(Nb, mid, Q.neckR0, MT_BODY, 1.3f, 0.9f));
    out.push_back(along(mid, Hc, Q.neckR1, MT_BODY, 1.3f, 0.9f));
    B(Nb + V{0.8f, 0, -0.6f}, {Q.neckR0 * 1.45f, Q.neckR0 * 1.35f, Q.neckR0 * 1.5f}, MT_LIGHT);  // the ruff
    const float hang = ang - 0.52f;
    const V fh{std::cos(hang), 0, std::sin(hang)}, uh{-std::sin(hang), 0, std::cos(hang)};
    const V Y{0, 1, 0};
    out.push_back(oriented(Hc, fh, Y, {Q.skull.x, Q.skull.z, Q.skull.y}, MT_BODY));
    const V mc = Hc + fh * (Q.skull.x * 0.78f + Q.muzzleLen * 0.5f) + uh * (-Q.skull.z * 0.28f);
    out.push_back(oriented(mc, fh, Y, {Q.muzzleLen * 0.5f + 1.0f, Q.muzzleH, Q.muzzleW}, MT_SNOUT));
    const float jaw = std::max(0.f, P.slash) * 0.9f + (P.slash < 0.f && P.tuskUp > 0.5f ? 0.4f : 0.f);
    const V jc = Hc + fh * (Q.skull.x * 0.72f + Q.muzzleLen * 0.42f) + uh * (-Q.skull.z * 0.58f - jaw * 1.9f) - fh * (jaw * 0.5f);
    out.push_back(oriented(jc, fh, Y, {Q.muzzleLen * 0.5f + 0.2f, Q.muzzleH * 0.55f, Q.muzzleW * 0.85f}, MT_LIGHT));
    B(Hc + fh * (Q.skull.x * 0.78f + Q.muzzleLen + 0.9f) + uh * (-Q.skull.z * 0.14f), {1.0f, 0.85f, 1.05f}, MT_NOSE);
    if (Q.flags & QF_TUSKS)
        for (int s = -1; s <= 1; s += 2) {
            const V b0 = mc + uh * (-Q.muzzleH * 0.55f) + Y * (s * Q.muzzleW * 0.85f) + fh * (Q.muzzleLen * 0.15f);
            const V b1 = b0 + fh * 1.6f + uh * 1.6f + Y * (s * 0.7f);
            out.push_back(along(b0, b1 + fh * 0.4f, 0.85f, MT_IVORY, 1.0f));
            out.push_back(along(b1, b1 + fh * 0.8f + uh * 2.6f, 0.6f, MT_IVORY, 1.0f));
        }
    if (antlers)
        for (int s = -1; s <= 1; s += 2) {
            const V a0 = Hc + uh * (Q.skull.z * 0.85f) - fh * 0.5f + Y * (s * Q.skull.y * 0.55f);
            const V a1 = a0 + uh * 4.5f + Y * (s * 1.6f) - fh * 1.0f, a2 = a1 + uh * 4.5f + Y * (s * 2.6f) + fh * 1.2f, a3 = a2 + uh * 3.5f + Y * (s * 0.8f) + fh * 1.5f;
            out.push_back(along(a0, a1, 0.7f, MT_ANTLER, 1.0f));
            out.push_back(along(a1, a2, 0.62f, MT_ANTLER, 1.0f));
            out.push_back(along(a2, a3, 0.5f, MT_ANTLER, 1.0f));
            out.push_back(along(a1, a1 + fh * 3.2f + uh * 2.0f, 0.42f, MT_ANTLER, 1.0f));
            out.push_back(along(a2, a2 + fh * 3.0f + uh * 1.8f, 0.4f, MT_ANTLER, 1.0f));
            out.push_back(along(a1, a1 - fh * 2.4f + uh * 2.6f, 0.38f, MT_ANTLER, 1.0f));
            out.push_back(along(a2, a2 - fh * 2.2f + uh * 2.4f, 0.36f, MT_ANTLER, 1.0f));
        }
    for (int s = -1; s <= 1; s += 2) {
        B(Hc + fh * (Q.skull.x * 0.55f) + uh * (Q.skull.z * 0.3f) + V{0, s * Q.skull.y * 0.86f, 0}, {0.8f, 0.7f, 0.7f}, MT_EYE);
        const float flick = P.ear * 0.5f * (s > 0 ? 1.f : 0.3f);
        const V eb = Hc - fh * (Q.skull.x * 0.25f) + uh * (Q.skull.z * 0.8f) + V{0, s * Q.skull.y * 0.6f, 0};
        const V ed = norm(uh * 1.f + Y * (s * Q.earSplay) - fh * (0.25f + flick));
        out.push_back(along(eb, eb + ed * Q.earH, Q.earW * 0.5f, MT_BODY, 1.0f, 0.42f));
        out.push_back(along(eb + fh * 0.4f + Y * (s * 0.25f), eb + ed * (Q.earH * 0.88f) + fh * 0.4f, Q.earW * 0.3f, MT_INNER, 1.0f, 0.4f));
    }

    // ---- tail
    V tp{Q.hipX - Q.rumpR.x * 0.9f, 0, hipZ + 1.0f};
    float th = Q.tailAng0 - 0.55f * P.tail;
    const float segLen = Q.tailLen / Q.tailSegs;
    for (int i = 0; i < Q.tailSegs; i++) {
        const V dir{-std::cos(th), 0, -std::sin(th)};
        const V nxt = tp + dir * segLen;
        const float rr = Q.tailR * (0.85f + 0.3f * std::sin(3.14159f * (i + 0.5f) / Q.tailSegs)) * (i == Q.tailSegs - 1 ? 0.8f : 1.f);
        out.push_back(along(tp, nxt, rr, i == Q.tailSegs - 1 ? MT_DARK : MT_BODY, 1.2f));
        tp = nxt;
        th += Q.tailCurl;
    }

    // ---- a lunge moves the whole body; death rolls it onto its side and puts it on the ground
    if (P.bodyDx != 0.f)
        for (Prim& p : out) p.c.x += P.bodyDx * 0.8f;
    if (P.roll > 0.01f) {
        const float cr = std::cos(P.roll), sr = std::sin(P.roll);
        auto rot = [&](V v) { return V{v.x, v.y * cr - v.z * sr, v.y * sr + v.z * cr}; };
        float minz = 1e9f;
        for (Prim& p : out) {
            p.c = rot(p.c);
            for (int i = 0; i < 3; i++) p.ax[i] = rot(p.ax[i]);
            const float ez = std::sqrt(std::pow(p.ax[0].z * p.r.x, 2) + std::pow(p.ax[1].z * p.r.y, 2) + std::pow(p.ax[2].z * p.r.z, 2));
            minz = std::min(minz, p.c.z - ez);
        }
        for (Prim& p : out) p.c.z -= minz;
    }
}

}  // namespace m3
