// model3d.hpp - a tiny software renderer that turns a posed 3D model (ellipsoids) into detailed pixel art.
//
// The animals are built as miniature 3D models (bones -> ellipsoid body parts, see quad3d.hpp), posed, then rendered here with an
// orthographic camera that looks down at the ground (the game stays top-down 2D). Each of the eight directions is rendered by turning the
// model, so nothing is mirrored and the light always comes from the same side of the screen.
//   - analytic ray / ellipsoid hits, a z-buffer, 3x3 supersampling;
//   - one fixed light (top-left, in front), a five-step colour ramp per material with ordered dithering between the steps;
//   - fur flecks that stick to the body part, darker backs and lighter undersides, a rim darkening towards the edges;
//   - a one pixel contour (the OUT marker, finished by styleActor).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "gfx.hpp"

namespace m3 {

struct V {
    float x = 0, y = 0, z = 0;
};
inline V operator+(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V operator-(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V operator*(V a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(V a) { return sqrtf(dot(a, a)); }
inline V norm(V a) {
    const float l = len(a);
    return l > 1e-6f ? a * (1.f / l) : V{1, 0, 0};
}
inline V lerp(V a, V b, float t) { return a + (b - a) * t; }

// A material: the colour of the surface and the colour where it faces down (belly) / up (back), and how furry it is.
struct Mat {
    u32 base = 0, under = 0, top = 0;
    float fur = 0.f;   // 0..1: how many fur flecks
    bool glow = false; // eyes: no shading
};

// One ellipsoid: centre, three orthonormal axes, radii along them.
struct Prim {
    V c;
    V ax[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    V r{1, 1, 1};
    int mat = 0;
};

inline void frame(V u0, V hint, V out[3]) {
    out[0] = norm(u0);
    V u1 = cross(out[0], hint);
    if (len(u1) < 1e-3f) u1 = cross(out[0], V{0, 0, 1});
    if (len(u1) < 1e-3f) u1 = cross(out[0], V{1, 0, 0});
    out[1] = norm(u1);
    out[2] = cross(out[0], out[1]);
}
inline Prim ball(V c, V r, int mat) {
    Prim p;
    p.c = c;
    p.r = r;
    p.mat = mat;
    return p;
}
// an ellipsoid whose long axis runs from a to b (radius `rr` across); `ext` > 1 makes neighbouring segments overlap
inline Prim along(V a, V b, float rr, int mat, float ext = 1.12f, float flat = 1.f) {
    Prim p;
    p.c = (a + b) * 0.5f;
    frame(b - a, V{0, 1, 0}, p.ax);
    p.r = {len(b - a) * 0.5f * ext, rr, rr * flat};
    p.mat = mat;
    return p;
}
// an ellipsoid with its long axis along `u0` and its flat side towards `hint`
inline Prim oriented(V c, V u0, V hint, V r, int mat) {
    Prim p;
    p.c = c;
    frame(u0, hint, p.ax);
    p.r = r;
    p.mat = mat;
    return p;
}

inline u32 rampColour(u32 base, int i) {
    switch (i) {
        case 0: return mixc(shadeC(base, 0.42f), RGBA(28, 26, 64), 0.18f);
        case 1: return mixc(shadeC(base, 0.68f), RGBA(40, 36, 80), 0.07f);
        case 2: return base;
        case 3: return mixc(base, RGBA(255, 243, 214), 0.20f);
        default: return mixc(base, RGBA(255, 248, 232), 0.42f);
    }
}

inline uint32_t hash3(int a, int b, int c) {
    uint32_t h = (uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ (uint32_t)c * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

// Renders `prims` seen from the direction `alpha` (the angle of the animal's nose on the screen: 0 = right, 90 deg = towards the viewer),
// with the camera pitched down by `phi`. `pxu` pixels per model unit; the model origin (the ground point under the animal) lands on (ax, ay).
// The result has a one pixel OUT-coloured contour.
inline Img render(const std::vector<Prim>& prims, const std::vector<Mat>& mats, float alpha, float phi, float pxu, int CW, int CH, float ax, float ay, int S = 3) {
    const float ca = cosf(alpha), sa = sinf(alpha), cp = cosf(phi), sp = sinf(phi);
    const V r1{ca, sa, 0}, r2{sa * sp, -ca * sp, -cp}, r3{sa * cp, -ca * cp, sp};  // world -> screen x, screen y (down), depth (towards the viewer)
    auto cam = [&](V w) { return V{dot(r1, w), dot(r2, w), dot(r3, w)}; };
    const V L = norm(V{-0.42f, -0.72f, 0.56f});
    const int GW = CW * S, GH = CH * S;
    std::vector<float> zb((size_t)GW * GH, -1e9f);
    std::vector<u32> cb((size_t)GW * GH, 0u);
    // colour ramps per material: [mat][0 base,1 under,2 top][5]
    std::vector<u32> ramp(mats.size() * 15);
    for (size_t m = 0; m < mats.size(); m++) {
        const u32 cols[3] = {mats[m].base, mats[m].under ? mats[m].under : mats[m].base, mats[m].top ? mats[m].top : mats[m].base};
        for (int k = 0; k < 3; k++)
            for (int i = 0; i < 5; i++) ramp[m * 15 + k * 5 + i] = rampColour(cols[k], i);
    }
    static const int BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    for (size_t pi = 0; pi < prims.size(); pi++) {
        const Prim& P = prims[pi];
        const Mat& M = mats[P.mat];
        const V cc = cam(P.c) * pxu;
        V u[3];
        float rr[3];
        for (int i = 0; i < 3; i++) {
            u[i] = cam(P.ax[i]);
            rr[i] = std::max(0.05f, P.r.x * (i == 0) + P.r.y * (i == 1) + P.r.z * (i == 2)) * pxu;
        }
        const float ex = sqrtf(u[0].x * u[0].x * rr[0] * rr[0] + u[1].x * u[1].x * rr[1] * rr[1] + u[2].x * u[2].x * rr[2] * rr[2]);
        const float ey = sqrtf(u[0].y * u[0].y * rr[0] * rr[0] + u[1].y * u[1].y * rr[1] * rr[1] + u[2].y * u[2].y * rr[2] * rr[2]);
        const int gx0 = std::max(0, (int)floorf((ax + cc.x - ex) * S)), gx1 = std::min(GW - 1, (int)ceilf((ax + cc.x + ex) * S));
        const int gy0 = std::max(0, (int)floorf((ay + cc.y - ey) * S)), gy1 = std::min(GH - 1, (int)ceilf((ay + cc.y + ey) * S));
        for (int gy = gy0; gy <= gy1; gy++)
            for (int gx = gx0; gx <= gx1; gx++) {
                const float X = (gx + 0.5f) / S - ax, Y = (gy + 0.5f) / S - ay;
                const float dx = X - cc.x, dy = Y - cc.y;
                float A = 0, B = 0, Cq = -1.f, pp[3], qq[3];
                for (int i = 0; i < 3; i++) {
                    pp[i] = dx * u[i].x + dy * u[i].y;
                    qq[i] = u[i].z;
                    const float ir2 = 1.f / (rr[i] * rr[i]);
                    A += qq[i] * qq[i] * ir2;
                    B += 2.f * pp[i] * qq[i] * ir2;
                    Cq += pp[i] * pp[i] * ir2;
                }
                const float disc = B * B - 4.f * A * Cq;
                if (disc < 0.f || A < 1e-9f) continue;
                const float t = (-B + sqrtf(disc)) / (2.f * A);
                const float dep = cc.z + t;
                float& zref = zb[(size_t)gy * GW + gx];
                if (dep <= zref) continue;
                zref = dep;
                if (M.glow) {
                    cb[(size_t)gy * GW + gx] = M.base;
                    continue;
                }
                // surface normal (screen space) and local coordinates on the unit sphere
                V n{0, 0, 0}, lc{0, 0, 0};
                float loc[3];
                for (int i = 0; i < 3; i++) {
                    loc[i] = (pp[i] + qq[i] * t) / rr[i];
                    const float k = loc[i] / rr[i];
                    n = n + u[i] * k;
                }
                n = norm(n);
                const float lit = std::max(0.f, dot(n, L));
                float shade = 0.16f + 0.84f * lit;
                shade *= 0.80f + 0.20f * std::min(1.f, std::max(0.f, (n.z - 0.05f) / 0.5f));  // darker towards the silhouette
                shade += ((BAYER[gy & 3][gx & 3] + 0.5f) / 16.f - 0.5f) * 0.10f;
                int lvl = shade < 0.22f ? 0 : shade < 0.45f ? 1 : shade < 0.68f ? 2 : shade < 0.88f ? 3 : 4;
                if (M.fur > 0.f) {
                    const uint32_t h = hash3((int)floorf(loc[0] * 7.f + 20.f) + (int)pi * 13, (int)floorf(loc[1] * 7.f + 20.f), (int)floorf(loc[2] * 7.f + 20.f));
                    if ((h & 255) < M.fur * 255.f) lvl = std::max(0, std::min(4, lvl + ((h >> 8) & 1 ? 1 : -1)));
                }
                const float nwz = -cp * n.y + sp * n.z;  // world "up" component of the normal
                int which = 0;
                if (M.under && nwz < -0.28f) which = 1;
                else if (M.top && nwz > 0.42f) which = 2;
                cb[(size_t)gy * GW + gx] = ramp[(size_t)P.mat * 15 + which * 5 + lvl];
            }
    }
    // down to the final resolution: coverage decides the shape, the most common colour decides the pixel
    Img im(CW, CH);
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++) {
            u32 cols[16];
            int cnt[16], n = 0, hit = 0;
            for (int sy = 0; sy < S; sy++)
                for (int sx = 0; sx < S; sx++) {
                    const u32 c = cb[(size_t)(y * S + sy) * GW + (x * S + sx)];
                    if (!c) continue;
                    hit++;
                    int k = 0;
                    while (k < n && cols[k] != c) k++;
                    if (k == n && n < 16) { cols[n] = c; cnt[n] = 0; n++; }
                    if (k < n) cnt[k]++;
                }
            if (hit * 2 < S * S || !n) continue;
            int best = 0;
            for (int k = 1; k < n; k++)
                if (cnt[k] > cnt[best]) best = k;
            im.p[(size_t)y * CW + x] = cols[best];
        }
    // one pixel contour
    Img o = im;
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++) {
            if (cA(im.p[(size_t)y * CW + x])) continue;
            bool near = false;
            for (int d = 0; d < 4 && !near; d++) {
                const int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3);
                near = im.in(nx, ny) && cA(im.get(nx, ny));
            }
            if (near) o.p[(size_t)y * CW + x] = HEX(0x241a14);  // = OUT (sprites.hpp), recoloured by styleActor
        }
    return o;
}

}  // namespace m3
