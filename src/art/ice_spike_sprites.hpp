// ice_spike_sprites.hpp - ice spikes: pale blue-white clusters of pointed shards that grow out of the frozen ground of the snowfield.
//
// Five sizes (a few small shards ... a tall spire with a crown of lower ones), two looks each: an upright cluster and a sprawling one that lies
// over to one side and has a few extra low shards. The size doubles as the growth stage: a spike that was mined out comes back at the smallest
// size and grows one size at a time (updateHarvest), so the same five pictures serve for the scenery and for the growing.
// Drawn from the tables below with no random numbers; the colours are the snowfield's ice, lighter and whiter than the mineable frost crystal.
#pragma once
#include <algorithm>
#include <vector>
#include "core/gfx.hpp"

constexpr int ICE_SPIKE_STAGES = 5;
constexpr int ICE_SPIKE_LOOKS = 2;

namespace icespike {

struct Shard { int dx, h, w, lean; };  // offset from the cluster's middle, height, base width, how far the tip leans (all pixels)

// the upright look; the sprawling look is made from it (see mkIceSpike)
static const std::vector<Shard> UPRIGHT[ICE_SPIKE_STAGES] = {
    {{-4, 5, 4, -1}, {0, 9, 5, 0}, {4, 4, 4, 1}},
    {{-6, 6, 5, -1}, {-1, 14, 7, 0}, {4, 9, 5, 1}, {7, 5, 4, 1}},
    {{-8, 7, 5, -2}, {-4, 13, 6, -1}, {0, 20, 8, 0}, {5, 14, 6, 1}, {9, 8, 5, 2}},
    {{-10, 8, 6, -2}, {-6, 15, 7, -1}, {-1, 27, 9, 0}, {4, 19, 7, 1}, {9, 13, 6, 2}, {12, 7, 5, 2}},
    {{-13, 8, 6, -2}, {-8, 17, 8, -2}, {-3, 25, 9, -1}, {2, 35, 11, 1}, {8, 22, 8, 2}, {12, 12, 6, 2}},
};

constexpr u32 LIT = HEX(0xf6fcff), PALE = HEX(0xd2eafb), MID = HEX(0x93c8f0), DEEP = HEX(0x5a8fcc), EDGE = HEX(0x2f4682);
constexpr u32 SNOW_L = HEX(0xffffff), SNOW_S = HEX(0xdbe7f6), SNOW_D = HEX(0xb9cde4);

// one shard, from its base row upwards: lit on the left, in shadow on the right, with a bright tip and a faint crack down the middle
inline void spire(Img& im, int bx, int by, int h, int w, int lean, int seed) {
    for (int i = 0; i < h; i++) {
        const float t = (float)i / (float)h;
        const float half = std::max(0.55f, w * 0.5f * std::min(1.f, (1.f - t) / 0.5f));  // straight sides, then a pointed tip
        const float cx = bx + lean * t * t * 1.6f;  // it bends over towards the tip
        const int x0 = (int)floorf(cx - half + 0.5f), x1 = (int)floorf(cx + half - 0.5f);
        for (int x = x0; x <= x1; x++) {
            const float u = (x + 0.5f - cx) / half;  // -1 on the left edge, +1 on the right
            u32 c = u < -0.4f ? LIT : (u < 0.15f ? PALE : (u < 0.62f ? MID : DEEP));
            if (t > 0.88f) c = u > 0.45f ? MID : LIT;                                     // the tip catches the light
            else if (t < 0.16f && u >= -0.4f) c = u < 0.2f ? MID : DEEP;                  // the foot sits in shade
            if (x1 > x0 && x == (x0 + x1) / 2 && (i + seed) % 6 == 3 && t > 0.2f && t < 0.8f) c = MID;  // a hairline crack
            if (x == x0 && x1 > x0 && cA(im.get(x - 1, by - i)) && t < 0.8f) c = DEEP;  // where it stands in front of its neighbour: a dark seam between them
            im.set(x, by - i, c);
        }
    }
}

}  // namespace icespike

constexpr int ICE_SPIKE_ANCHOR_PAD = 3;
// size of a spike's picture and where its foot stands (bottom centre), by stage
inline void iceSpikeSize(int stage, int& w, int& h, int& ax, int& ay) {
    int reach = 0, tall = 0;
    for (const icespike::Shard& s : icespike::UPRIGHT[stage]) {
        reach = std::max(reach, std::abs(s.dx) + s.w / 2 + 2);
        tall = std::max(tall, s.h);
    }
    w = reach * 2 + 3;
    h = tall + 7;
    ax = w / 2;
    ay = h - ICE_SPIKE_ANCHOR_PAD;
}

inline Img mkIceSpike(int stage, int look) {
    using namespace icespike;
    int w, h, ax, ay;
    iceSpikeSize(stage, w, h, ax, ay);
    Img im(w, h);
    const int by = ay;  // the row the foot stands on
    std::vector<Shard> sh = UPRIGHT[stage];
    if (look == 1) {  // the sprawling look: mirrored, the tall shard lying over, the others lower, and a few extra low shards between them
        size_t tallest = 0;
        for (size_t i = 0; i < sh.size(); i++)
            if (sh[i].h > sh[tallest].h) tallest = i;
        for (size_t i = 0; i < sh.size(); i++) {
            sh[i].dx = -sh[i].dx;
            sh[i].lean = -sh[i].lean * 2 + (i == tallest ? 2 : 0);
            sh[i].h = (int)(sh[i].h * (i == tallest ? 0.92f : 0.78f)) + (i % 2);
        }
        const std::vector<Shard> base = sh;
        for (size_t i = 0; i + 1 < base.size() && sh.size() < base.size() + 2 && stage >= 1; i += 2)
            sh.push_back({(base[i].dx + base[i + 1].dx) / 2, std::max(3, base[i].h / 2), std::max(3, base[i].w - 1), (base[i].lean + base[i + 1].lean) / 2});
    }
    // the snow banked round the foot, behind the shards
    const int reach = (w - 5) / 2;
    im.ellipse((float)ax, (float)by, reach + 0.5f, 2.2f, SNOW_S);
    im.ellipse((float)ax - 1.f, (float)by - 0.5f, reach - 1.5f, 1.6f, SNOW_L);
    std::sort(sh.begin(), sh.end(), [](const Shard& a, const Shard& b) { return a.h < b.h; });  // low ones first, the tallest in front
    int n = 0;
    for (const Shard& s : sh) spire(im, ax + s.dx, by - 1 - (std::abs(s.dx) > reach / 2 ? 0 : 1), s.h, s.w + 1, s.lean, ++n * 3 + stage + look);
    // a few chips of ice on the snow in front
    for (int i = 0; i < 2 + stage / 2; i++) {
        const int x = ax - reach + 1 + ((i * 7 + stage * 3 + look * 5) % std::max(3, 2 * reach - 1)), y = by + (i % 2);
        if (cA(im.get(x, y))) continue;
        im.set(x, y, LIT);
        im.set(x + 1, y, MID);
    }
    // the front of the snowbank
    for (int x = ax - reach; x <= ax + reach; x += 3) im.set(x, by + 1, SNOW_D);
    im.outline(EDGE);
    return im;
}
