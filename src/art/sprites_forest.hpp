// sprites_forest.hpp - the three forest floors of the home forest (see homeFloor() in worldgen.hpp): leaf litter, moss and pine needles,
// and the water of the lakes and brooks. Floors are 16x16 tiles that wrap, painted with a generator of their
// own (seeded from a constant, never from the shared sprite stream), so no other sprite changes when these are touched. Each floor also
// feeds the edge blending (tile_edges.hpp). The four floors are kept apart in colour on purpose: leaf litter is warm amber, needles a
// cool grey-brown, roots near-black earth and moss a minty teal-green, so none is mistaken for another or for the yellow-green meadow.
#pragma once
#include <algorithm>
#include <vector>
#include <unordered_map>
#include "art/tile_edges.hpp"
#include "art/tiles_user.hpp"

namespace forestfloor {
inline void px(Img& im, int x, int y, u32 c) { im.set(((x % 16) + 16) % 16, ((y % 16) + 16) % 16, c); }

// warm amber floor under oaks, strewn with fallen leaves: small two-pixel leaves in orange, rust, gold and olive, a dark stem pixel on each
inline Img mkLeafLitter(int variant) {
    Rng g(7100 + variant * 211);
    Img im(16, 16);
    im.fill(HEX(0x84602e));
    for (int i = 0; i < 16; i++) px(im, g.irange(0, 15), g.irange(0, 15), HEX(0x6e4e24));  // damp dark specks
    static const u32 LEAF[4] = {HEX(0xc8792a), HEX(0xa8452a), HEX(0xb89a3a), HEX(0x7a8a34)};
    const int n = 9 + variant;
    for (int i = 0; i < n; i++) {
        const int x = g.irange(0, 15), y = g.irange(0, 15);
        const u32 c = LEAF[g.irange(0, 3)];
        const bool tall = g.chance(0.35f);
        px(im, x, y, c);
        if (tall) px(im, x, y + 1, c);
        else px(im, x + 1, y, c);
        px(im, tall ? x : x + 2, tall ? y + 2 : y, HEX(0x4a3416));  // the stem
        if (g.chance(0.4f)) px(im, x, y - 1, LEAF[2]);              // a lit edge
    }
    return im;
}

// cool grey-brown carpet of fallen pine needles: fine light and dark strokes lying every which way, now and then a cone
inline Img mkNeedles(int variant) {
    Rng g(7300 + variant * 197);
    Img im(16, 16);
    im.fill(HEX(0x65503d));
    for (int i = 0; i < 20 + 2 * variant; i++) {
        const int x = g.irange(0, 15), y = g.irange(0, 15);
        const u32 c = g.chance(0.55f) ? HEX(0x8c7358) : HEX(0x45362a);
        const int dir = g.irange(0, 2);  // \ / or -
        px(im, x, y, c);
        px(im, x + (dir == 1 ? -1 : 1), y + (dir == 2 ? 0 : 1), c);
        if (g.chance(0.3f)) px(im, x + (dir == 1 ? -2 : 2), y + (dir == 2 ? 0 : 2), c);
    }
    if (variant == 2) {  // a pine cone
        px(im, 10, 9, HEX(0x5c3d24)); px(im, 11, 9, HEX(0x5c3d24));
        px(im, 10, 10, HEX(0x8a5a34)); px(im, 11, 10, HEX(0x6e4a2c)); px(im, 12, 10, HEX(0x5c3d24));
        px(im, 11, 11, HEX(0x5c3d24));
    }
    return im;
}

// damp, minty teal-green moss: rounded clumps, lit on their upper left, with dark pits between them and a cool speckle
inline Img mkMoss(int variant) {
    Rng g(7500 + variant * 173);
    Img im(16, 16);
    im.fill(HEX(0x2f6b46));
    for (int i = 0; i < 12; i++) px(im, g.irange(0, 15), g.irange(0, 15), HEX(0x1f4d36));  // pits
    const int n = 5 + variant;
    for (int i = 0; i < n; i++) {  // a clump: a 3x2 or 3x3 blob with a lighter top
        const int x = g.irange(0, 15), y = g.irange(0, 15), h = g.chance(0.5f) ? 2 : 3;
        for (int yy = 0; yy < h; yy++)
            for (int xx = 0; xx < 3; xx++) {
                if ((yy == 0 || yy == h - 1) && (xx == 0 && g.chance(0.5f))) continue;  // round the corners off a little
                px(im, x + xx, y + yy, yy == 0 ? HEX(0x7acc8c) : HEX(0x4a9a5a));
            }
        px(im, x, y, HEX(0xa8e8a8));
    }
    for (int i = 0; i < 8; i++) px(im, g.irange(0, 15), g.irange(0, 15), HEX(0x3f8a74));  // cool speckle
    return im;
}

// trodden brown earth round a lake: packed soil with lighter and darker grains, damp dark patches, a few pebbles and the odd reed tuft
inline Img mkBank(int variant) {
    Rng g(7900 + variant * 191);
    Img im(16, 16);
    im.fill(HEX(0x6a4a2a));
    for (int i = 0; i < 26; i++) px(im, g.irange(0, 15), g.irange(0, 15), HEX(0x7e5a38));  // dry grains
    for (int i = 0; i < 22; i++) px(im, g.irange(0, 15), g.irange(0, 15), HEX(0x56381f));  // dark grains
    for (int i = 0; i < 3; i++) {                                                          // damp patches
        const int x = g.irange(0, 14), y = g.irange(0, 14);
        px(im, x, y, HEX(0x4c3219)); px(im, x + 1, y, HEX(0x4c3219)); px(im, x, y + 1, HEX(0x56381f));
    }
    for (int i = 0; i < 3; i++) px(im, g.irange(0, 15), g.irange(0, 15), g.chance(0.5f) ? HEX(0xb4a88a) : HEX(0x8a7e66));  // pebbles
    if (variant != 1) {  // a reed tuft
        const int x = g.irange(2, 13), y = g.irange(5, 14);
        px(im, x, y, HEX(0x5c7a34)); px(im, x, y - 1, HEX(0x7ea044)); px(im, x, y - 2, HEX(0xa0c45c));
        px(im, x + 1, y - 1, HEX(0x7ea044)); px(im, x - 1, y, HEX(0x5c7a34));
    }
    return im;
}
// the same earth right at the waterline: wet and dark, with a few bluish-grey pebbles (the grey of the water's own ripples) and a sheen of damp
inline Img mkBankWet(int variant) {
    Rng g(8300 + variant * 173);
    Img im(16, 16);
    im.fill(HEX(0x4f3a26));
    for (int i = 0; i < 20; i++) px(im, g.irange(0, 15), g.irange(0, 15), HEX(0x614830));  // damp grains
    for (int i = 0; i < 26; i++) px(im, g.irange(0, 15), g.irange(0, 15), HEX(0x3c2b1b));  // dark grains
    for (int i = 0; i < 4; i++) {                                                          // wet patches
        const int x = g.irange(0, 14), y = g.irange(0, 14);
        px(im, x, y, HEX(0x32231a)); px(im, x + 1, y, HEX(0x32231a)); px(im, x, y + 1, HEX(0x3c2b1b));
    }
    for (int i = 0; i < 4; i++) px(im, g.irange(0, 15), g.irange(0, 15), g.chance(0.5f) ? HEX(0x7c9baf) : HEX(0x6d93ad));  // wet pebbles
    for (int i = 0; i < 2; i++) { const int x = g.irange(0, 14), y = g.irange(0, 15); px(im, x, y, HEX(0x8aa8b4)); px(im, x + 1, y, HEX(0x6d93ad)); }  // a sheen
    return im;
}
}  // namespace forestfloor

// paints the forest floors and registers them for edge blending
inline void buildForestFloors(SDL_Renderer* r, Spr leaf[3], Spr needles[3], Spr moss[3], Spr bank[3], TileBlend& tb) {
    using namespace forestfloor;
    for (int i = 0; i < 3; i++) {
        const Img a = mkLeafLitter(i), b = mkNeedles(i), c = mkMoss(i);
        leaf[i] = makeSpr(r, a, 0, 0);
        needles[i] = makeSpr(r, b, 0, 0);
        moss[i] = makeSpr(r, c, 0, 0);
        const Img k = mkBank(i);
        bank[i] = makeSpr(r, k, 0, 0);
        tb.src[BL_BANK][i] = k;
        tb.src[BL_LEAF][i] = a;
        tb.src[BL_NEEDLES][i] = b;
        tb.src[BL_MOSS][i] = c;
    }
    tb.nvar[BL_LEAF] = tb.nvar[BL_NEEDLES] = tb.nvar[BL_MOSS] = tb.nvar[BL_BANK] = 3;
}

// ================================================================= water
// Lakes and brooks are one water, baked per tile (and kept): the author's ripple pattern (WATER_ROWS: short grey dashes, a few cyan sparkles and
// white glints on a flat blue) in four depths of light blue, so the look matches the rest of the tile art. How deep a pixel is follows the
// depth of this tile and its 8 neighbours, blended smoothly across tile borders (with a little noise so the rings are ragged): depth 0 is the
// shallows, ~1.75 the normal water (the author's own colours), 3.4 the deep. Where water meets dry land the tile is baked with the land
// showing through (shallows), a pale wash, a white foam line, and a ragged waterline. The ripples scroll sideways one frame at a time on
// world coordinates, so they carry on across tile borders; deeper water drifts the other way. 32 frames of 2 px make one 64 px loop.
constexpr int WATER_PER = 8;      // tiles per noise period (128 px)
constexpr int WATER_FRAMES = 32;  // frames of the ripple loop (2 px each)
struct WaterArt {
    static constexpr int P = WATER_PER * 16;
    std::vector<float> wob;  // 0..1 noise, evenly spread, for ragged borders
    std::unordered_map<uint64_t, Spr> cache;

    static inline void octave(std::vector<float>& n, int cx, int cy, float weight, uint32_t seed) {
        const int lw = P / cx, lh = P / cy;
        Rng g(seed);
        std::vector<float> lat((size_t)lw * lh);
        for (float& v : lat) v = g.f();
        auto sm = [](float t) { return t * t * (3.f - 2.f * t); };
        for (int y = 0; y < P; y++)
            for (int x = 0; x < P; x++) {
                const int x0 = x / cx, y0 = y / cy;
                const float fx = sm((x % cx) / (float)cx), fy = sm((y % cy) / (float)cy);
                auto L = [&](int i, int j) { return lat[(size_t)(j % lh) * lw + (i % lw)]; };
                const float top = L(x0, y0) * (1 - fx) + L(x0 + 1, y0) * fx, bot = L(x0, y0 + 1) * (1 - fx) + L(x0 + 1, y0 + 1) * fx;
                n[(size_t)y * P + x] += weight * (top * (1 - fy) + bot * fy);
            }
    }

    void init() {
        wob.assign((size_t)P * P, 0.f);
        octave(wob, 16, 16, 0.6f, 4101);
        octave(wob, 8, 8, 0.3f, 4102);
        octave(wob, 4, 4, 0.1f, 4103);
        {
            std::vector<float> s(wob);
            std::sort(s.begin(), s.end());
            for (float& v : wob) v = (float)(std::lower_bound(s.begin(), s.end(), v) - s.begin()) / (float)(s.size() - 1);
        }
    }

    // the author's ripple pixel at (x, y): 'a' base, 'b' / 'c' grey dashes, 'd' cyan sparkle, 'e' white glint (wraps)
    static inline char pat(int x, int y) { return WATER_ROWS[((y % WATER_TEX_H) + WATER_TEX_H) % WATER_TEX_H][((x % WATER_TEX_W) + WATER_TEX_W) % WATER_TEX_W]; }
    static inline int patIdx(char c) { return c == 'b' ? 1 : c == 'c' ? 2 : c == 'd' ? 3 : c == 'e' ? 4 : 0; }

    // One baked tile. dep[3][3]: depth of this tile and its 8 neighbours (0 = shore / brook ... 3.4 = deep; dry land counts 0);
    // land: bit k set where neighbour k (N, NE, E, SE, S, SW, W, NW) is dry land.
    Img bake(int tx, int ty, int frame, const float dep[3][3], int land) const {
        static const u32 PAL[4][5] = {  // by depth: base, grey dash b, grey dash c, cyan, white (the second row is the author's own water)
            {HEX(0x4f93c4), HEX(0xa9cfe0), HEX(0x94bfd6), HEX(0x35d0e4), HEX(0xe6f8f8)},
            {HEX(0x3978a8), HEX(0x7c9baf), HEX(0x6d93ad), HEX(0x28ccdf), HEX(0xdff6f5)},
            {HEX(0x306ea2), HEX(0x6e94b0), HEX(0x6089a8), HEX(0x26bcd4), HEX(0xcfeaf0)},
            {HEX(0x265f94), HEX(0x5f8aaa), HEX(0x517ea0), HEX(0x22a8c4), HEX(0xa8d4de)}};
        const bool N = land & 1, NE = land & 2, E = land & 4, SE = land & 8, S = land & 16, SW = land & 32, W = land & 64, NW = land & 128;
        Img im(16, 16);
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                const int wx = tx * 16 + x, wy = ty * 16 + y;
                const float nz = wob[(size_t)(wy % P) * P + (wx % P)];
                // depth: bilinear between the centres of the 3x3 tiles
                const float sx = (x + 0.5f) / 16.f + 0.5f, sy = (y + 0.5f) / 16.f + 0.5f;
                const int ix = sx >= 1.f ? 1 : 0, iy = sy >= 1.f ? 1 : 0;
                const float fx = sx - ix, fy = sy - iy;
                const float d = (dep[iy][ix] * (1 - fx) + dep[iy][ix + 1] * fx) * (1 - fy) + (dep[iy + 1][ix] * (1 - fx) + dep[iy + 1][ix + 1] * fx) * fy;
                const float t = d + (nz - 0.5f) * 0.6f;
                const int lvl = t < 1.25f ? 0 : t < 2.25f ? 1 : t < 3.25f ? 2 : 3;
                // shallow and normal water scroll one way, the deeper water flipped and shifted the other, so the two layers do not repeat each other
                const bool deeper = lvl >= 2;
                const int px = deeper ? wx - frame * 2 + 32 : wx + frame * 2, py = deeper ? 15 - (wy & 15) : (wy & 15);
                u32 c = PAL[lvl][patIdx(pat(px, py))];
                if (land) {  // distance to the nearest dry land, in pixels
                    const float cx = x + 0.5f, cy = y + 0.5f;
                    float dist = 99.f;
                    if (N) dist = std::min(dist, cy);
                    if (S) dist = std::min(dist, 16.f - cy);
                    if (W) dist = std::min(dist, cx);
                    if (E) dist = std::min(dist, 16.f - cx);
                    if (NW && !N && !W) dist = std::min(dist, hypotf(cx, cy));
                    if (NE && !N && !E) dist = std::min(dist, hypotf(16.f - cx, cy));
                    if (SW && !S && !W) dist = std::min(dist, hypotf(cx, 16.f - cy));
                    if (SE && !S && !E) dist = std::min(dist, hypotf(16.f - cx, 16.f - cy));
                    dist += (nz - 0.5f) * 2.4f;  // ragged waterline
                    if (dist < 1.4f) { im.set(x, y, RGBA(79, 58, 38, 240)); continue; }     // the wet bank's earth carries on into the water tile
                    if (dist < 2.4f) c = HEX(0xe6f6fa);                                    // the foam line
                    else if (dist < 3.4f) c = (HEX(0xb4e2f0) & 0x00ffffffu) | 0xd8000000u;  // pale wash
                    else if (dist < 5.4f) c = (c & 0x00ffffffu) | 0x80000000u;             // shallows: the land shows through
                    else if (dist < 7.4f) c = (c & 0x00ffffffu) | 0xc0000000u;
                }
                im.set(x, y, c);
            }
        return im;
    }

    // The bank: an overlay for a dry tile that touches water. water: bit k set where neighbour k (N, NE, E, SE, S, SW, W, NW) is water.
    // A wide band of earth along the water with a ragged inner edge: dark and damp at the waterline, drying lighter further out and then
    // fading into the ground, with blue-grey wet pebbles and the odd reed tuft. The ground below is the tile's own, so it works on grass,
    // moss, leaves, sand or mud alike.
    Img bakeBank(int tx, int ty, int water) const {
        const bool N = water & 1, NE = water & 2, E = water & 4, SE = water & 8, S = water & 16, SW = water & 32, W = water & 64, NW = water & 128;
        Img im(16, 16);
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                const int X = (tx * 16 + x) % P, Y = (ty * 16 + y) % P;
                const float nz = wob[(size_t)Y * P + X];
                const float cx = x + 0.5f, cy = y + 0.5f;
                float d = 99.f;
                if (N) d = std::min(d, cy);
                if (S) d = std::min(d, 16.f - cy);
                if (W) d = std::min(d, cx);
                if (E) d = std::min(d, 16.f - cx);
                if (NW && !N && !W) d = std::min(d, hypotf(cx, cy));
                if (NE && !N && !E) d = std::min(d, hypotf(16.f - cx, cy));
                if (SW && !S && !W) d = std::min(d, hypotf(cx, 16.f - cy));
                if (SE && !S && !E) d = std::min(d, hypotf(16.f - cx, 16.f - cy));
                d += (nz - 0.5f) * 3.2f;  // ragged
                const uint32_t h = hash2(X, Y, 77);
                const u32 wet = (h >> 12 & 7) == 0 ? HEX(0x614830) : (h >> 12 & 7) == 1 ? HEX(0x3c2b1b) : HEX(0x4f3a26);
                const u32 dry = (h >> 12 & 7) == 0 ? HEX(0x7e5a38) : (h >> 12 & 7) == 1 ? HEX(0x56381f) : HEX(0x6a4a2a);
                u32 c = 0;
                if (d < 2.6f) c = (wet & 0x00ffffffu) | 0xf0000000u;
                else if (d < 4.6f) c = (wet & 0x00ffffffu) | 0xd0000000u;
                else if (d < 6.4f) c = (dry & 0x00ffffffu) | 0x88000000u;
                else if (d < 8.0f) c = (dry & 0x00ffffffu) | 0x38000000u;
                if (d < 8.0f && d > 0.3f) {
                    if (h % 100 < 4) c = (h >> 8 & 1) ? HEX(0x7c9baf) : HEX(0x6d93ad);  // wet pebbles
                    else if (h % 100 == 4 && d < 5.0f) c = HEX(0x32231a);                // a dark clod
                }
                im.set(x, y, c);
            }
        // reed tufts: a few green blades standing on the bank
        for (int y = 3; y < 16; y++)
            for (int x = 1; x < 15; x++) {
                const int X = (tx * 16 + x) % P, Y = (ty * 16 + y) % P;
                if (hash2(X, Y, 91) % 1000 >= 14 || wob[(size_t)Y * P + X] > 0.995f) continue;
                if (cA(im.get(x, y)) < 90 || im.get(x, y) == HEX(0x7c9baf) || im.get(x, y) == HEX(0x6d93ad)) continue;  // only on the soil
                im.set(x, y, HEX(0x5c7a34)); im.set(x, y - 1, HEX(0x7ea044)); im.set(x, y - 2, HEX(0xa0c45c));
                if (hash2(X, Y, 92) & 1) { im.set(x + 1, y - 1, HEX(0x7ea044)); im.set(x - 1, y, HEX(0x5c7a34)); }
            }
        return im;
    }
    std::unordered_map<uint64_t, Spr> bankCache;
    const Spr* getBank(SDL_Renderer* r, int tx, int ty, int water) {
        const uint64_t key = (uint64_t)(tx & 7) | (uint64_t)(ty & 7) << 3 | (uint64_t)(water & 255) << 6;
        auto it = bankCache.find(key);
        if (it == bankCache.end()) it = bankCache.emplace(key, makeSpr(r, bakeBank(tx & 7, ty & 7, water), 0, 0)).first;
        return &it->second;
    }

    // a water tile at world (tx, ty) for ripple frame `frame` (0 .. WATER_FRAMES - 1); `stream` marks a brook tile
    const Spr* get(SDL_Renderer* r, int tx, int ty, int frame, const float dep[3][3], int land, bool stream) {
        uint64_t key = (uint64_t)(tx & 7) | (uint64_t)(ty & 7) << 3 | (uint64_t)(frame & (WATER_FRAMES - 1)) << 6 | (uint64_t)(stream ? 1 : 0) << 11 | (uint64_t)(land & 255) << 12;
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++) key |= (uint64_t)((int)(dep[j][i] * 2.f + 0.5f) & 15) << (20 + 4 * (j * 3 + i));  // depth in half-tile steps
        auto it = cache.find(key);
        if (it == cache.end()) {
            if (cache.size() > 24000) {  // keep memory bounded on very long walks: start over
                for (auto& kv : cache) {
                    if (kv.second.t) SDL_DestroyTexture(kv.second.t);
                }
                cache.clear();
            }
            it = cache.emplace(key, makeSpr(r, bake(tx & 7, ty & 7, frame & (WATER_FRAMES - 1), dep, land), 0, 0)).first;
        }
        return &it->second;
    }
};
