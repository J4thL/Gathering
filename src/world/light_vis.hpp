// light_vis.hpp - visibility polygons for point lights (fires, lanterns, the hunter's torch).
// The obstacles near a light are simple hulls: the boundary of every building / rock / cliff tile (World::occ) and a small ellipse at the foot of every
// tree trunk. A ray is cast from the light through VIS_N directions and stops at the first hull edge it meets; the table of stop distances IS the visibility
// polygon (star-shaped round the light). The darkness map leaves a cell unlit when it lies beyond the table's distance in its direction, and the warm glow
// of the light is drawn as a triangle fan over the table, so nothing leaks into the shadow behind a trunk or a wall.
#pragma once
#include <cmath>
#include <vector>
#include "world/world.hpp"
#include "animals/fauna.hpp"
#include "core/shadow_casters.hpp"

constexpr int VIS_N = 128;  // directions per light (a ray every 2.8 degrees: 4 px apart at 84 px)

struct VisSeg { float x0, y0, x1, y1; };
struct VisEntry {
    float x = 0, y = 0, R = 0;  // the light and how far the table looks (a ray that finds nothing stores R)
    float d[VIS_N];
    // the distance the light reaches towards (wx, wy); R when nothing is in the way
    float reach(float wx, float wy) const {
        float a = atan2f(wy - y, wx - x);
        if (a < 0) a += 6.2831853f;
        return d[(int)(a * (VIS_N / 6.2831853f)) % VIS_N];
    }
    bool open(int i) const { return d[i] >= R - 0.01f; }
};

inline void visEllipse(std::vector<VisSeg>& out, float cx, float cy, float rx, float ry) {
    constexpr int N = 8;
    float px = cx + rx, py = cy;
    for (int i = 1; i <= N; i++) {
        const float a = i * 6.2831853f / N, x = cx + cosf(a) * rx, y = cy + sinf(a) * ry;
        out.push_back({px, py, x, y});
        px = x, py = y;
    }
}

// The hulls within R of a light at (lx, ly). `alive(i)`: false for a prop that has been felled or broken (it casts nothing).
template <typename Alive>
void visGather(const World& w, Alive alive, float lx, float ly, float R, std::vector<VisSeg>& out) {
    out.clear();
    // buildings, boulders, cliffs: the outline of each solid block of tiles (a lamp on a wall lights outwards: tiles within 14 px of the light are skipped)
    const int tx0 = std::max(0, (int)((lx - R) / TS) - 1), tx1 = std::min(MW - 1, (int)((lx + R) / TS) + 1);
    const int ty0 = std::max(0, (int)((ly - R) / TS) - 1), ty1 = std::min(MH - 1, (int)((ly + R) / TS) + 1);
    auto occAt = [&](int x, int y) { return x >= 0 && y >= 0 && x < MW && y < MH && w.occ[y][x]; };
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            if (!w.occ[ty][tx]) continue;
            const float x0 = tx * (float)TS, y0 = ty * (float)TS, x1 = x0 + TS, y1 = y0 + TS;
            const float nx = std::max(x0, std::min(lx, x1)), ny = std::max(y0, std::min(ly, y1));
            if ((nx - lx) * (nx - lx) + (ny - ly) * (ny - ly) < 14.f * 14.f) continue;
            if (!occAt(tx, ty - 1)) out.push_back({x0, y0, x1, y0});
            if (!occAt(tx, ty + 1)) out.push_back({x0, y1, x1, y1});
            if (!occAt(tx - 1, ty)) out.push_back({x0, y0, x0, y1});
            if (!occAt(tx + 1, ty)) out.push_back({x1, y0, x1, y1});
        }
    // tree trunks: a small ellipse at the foot (the ground is squashed on screen); the canopy is far above a torch and blocks nothing
    auto first = std::lower_bound(w.props.begin(), w.props.end(), ly - R - 4.f, [](const Prop& a, float y) { return a.y < y; });
    for (auto it = first; it != w.props.end() && it->y <= ly + R + 12.f; ++it) {
        if (std::fabs(it->x - lx) > R + 8.f) continue;
        const shadowdef::PropShadow* d = shadowdef::propShadow(it->kind);
        if (!d || (d->shape != shadowdef::SH_OAK && d->shape != shadowdef::SH_TRUNK)) continue;
        if (!alive((size_t)(it - w.props.begin()))) continue;
        const float rx = d->shape == shadowdef::SH_OAK ? 6.f : 3.6f, ry = rx * 0.65f, cx = it->x, cy = it->y - 2.f;
        const float ex = (lx - cx) / rx, ey = (ly - cy) / ry;
        if (ex * ex + ey * ey < 1.6f) continue;  // the light is at the trunk itself: it lights it from outside, nothing to cast
        visEllipse(out, cx, cy, rx, ry);
    }
}

// Cast VIS_N rays from (lx, ly) against the hulls and fill the table.
inline void visCast(const std::vector<VisSeg>& segs, float lx, float ly, float R, VisEntry& v) {
    v.x = lx, v.y = ly, v.R = R;
    for (int i = 0; i < VIS_N; i++) {
        const float a = (i + 0.5f) * 6.2831853f / VIS_N, rx = cosf(a), ry = sinf(a);
        float best = R;
        for (const VisSeg& s : segs) {
            const float ex = s.x1 - s.x0, ey = s.y1 - s.y0, den = rx * ey - ry * ex;
            if (std::fabs(den) < 1e-6f) continue;
            const float qx = s.x0 - lx, qy = s.y0 - ly;
            const float t = (qx * ey - qy * ex) / den, u = (qx * ry - qy * rx) / den;
            if (t > 0.01f && t < best && u >= 0.f && u <= 1.f) best = t;
        }
        v.d[i] = best;
    }
}
