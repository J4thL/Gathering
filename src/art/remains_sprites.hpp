// remains_sprites.hpp - what is left on the ground after a rock or an ore deposit has been mined out.
//
// Rocks leave a heap of broken stone, sized to what stood there (tiny rock, small rock, boulder) and in the colours of the biome (ROCK_PALS).
// Ore deposits leave their own mark: a few lumps of the ore itself lying between chips of the host rock (copper, iron, gold, silver, amber),
// black lumps of coal, or the broken shards of a crystal, a gem or the eternal ice. Every kind comes in REMAINS_VARIANTS looks;
// which one lies where is fixed by the spot (remainsVariant), so a deposit always leaves the same heap and two neighbours differ.
// The pictures are made from fixed seeds and never touch the game's random streams.
#pragma once
#include <algorithm>
#include <vector>
#include "core/data.hpp"
#include "art/rock_sprites.hpp"

constexpr int REMAINS_VARIANTS = 3;
constexpr int REMAINS_ROCK_CLASSES = 3;  // tiny, small, boulder: the same classes as rockClass()

inline int remainsVariant(float x, float y) { return (int)(hash2((int)x >> 2, (int)y >> 2, 0x5E4Au) % (uint32_t)REMAINS_VARIANTS); }

namespace remains {

struct R {  // a tiny deterministic generator (the art is made once, at start-up)
    uint32_t s;
    uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; }
    int n(int m) { return (int)(next() % (uint32_t)m); }
    float f() { return (next() & 0xFFFF) / 65535.f; }
};

struct Chunk { float x, y, rx, ry; };

// a heap of stone chunks, placed at random inside a flat ellipse; chunks further back are drawn first so the front ones overlap them
inline void heap(Img& im, R& r, float cx, float cy, float hw, float hh, int count, float rmin, float rmax, u32 l, u32 m, u32 d, std::vector<Chunk>* out = nullptr) {
    std::vector<Chunk> cs;
    for (int tries = 0; tries < 60 && (int)cs.size() < count; tries++) {
        const float a = r.f() * 6.2832f, k = sqrtf(r.f()) * 0.9f;
        Chunk c{cx + cosf(a) * hw * k, cy + sinf(a) * hh * k, rmin + r.f() * (rmax - rmin), 0.f};
        c.ry = c.rx * (0.72f + r.f() * 0.2f);
        bool ok = true;
        for (const Chunk& o : cs) {  // not two chunks on top of each other
            const float dx = o.x - c.x, dy = (o.y - c.y) * 1.6f;
            if (dx * dx + dy * dy < (o.rx + c.rx) * (o.rx + c.rx) * 0.62f) { ok = false; break; }
        }
        if (ok) cs.push_back(c);
    }
    std::sort(cs.begin(), cs.end(), [](const Chunk& a, const Chunk& b) { return a.y < b.y; });
    const u32 edge = RGBA((int)(cR(d) * 0.55f), (int)(cG(d) * 0.55f), (int)(cB(d) * 0.58f), 255);  // each chunk has its own dark edge, so a heap reads as separate stones
    for (const Chunk& c : cs) {
        im.ellipse(c.x, c.y + 0.3f, c.rx + 0.9f, c.ry + 0.9f, edge);
        im.ellipseShaded(c.x, c.y, c.rx, c.ry, l, m, d);
        if (c.rx >= 2.2f) im.set((int)(c.x - c.rx * 0.35f), (int)(c.y - c.ry * 0.45f), l);  // a lit edge
    }
    if (out) *out = cs;
}

// loose pebbles: one lit pixel with a dark one under it
inline void pebbles(Img& im, R& r, float cx, float cy, float hw, float hh, int count, u32 l, u32 d) {
    for (int i = 0; i < count; i++) {
        const int x = (int)(cx + (r.f() * 2.f - 1.f) * hw), y = (int)(cy + (r.f() * 2.f - 1.f) * hh);
        if (cA(im.get(x, y)) || cA(im.get(x, y + 1))) continue;
        im.set(x, y, l);
        im.set(x, y + 1, d);
    }
}

}  // namespace remains

// ---- rocks ------------------------------------------------------------------------------------------------------------------------------------
// cls: 0 tiny, 1 small, 2 boulder; variant: 0..REMAINS_VARIANTS-1; theme: the biome's colours (ROCK_PALS)
inline void rockRemainsSize(int cls, int& w, int& h, int& ax, int& ay) {
    switch (cls) {
        case 0: w = 11, h = 7, ax = 5, ay = 5; break;
        case 1: w = 15, h = 9, ax = 7, ay = 7; break;
        default: w = 23, h = 13, ax = 11, ay = 11; break;
    }
}
inline Img mkRockRemains(int cls, int variant, int theme) {
    int w, h, ax, ay;
    rockRemainsSize(cls, w, h, ax, ay);
    Img im(w, h);
    remains::R r{0x9E3779B1u * (uint32_t)(cls * 7 + variant + 1)};
    const u32 L = HEX(0xc4c7d2), M = HEX(0x9a9ca8), D = HEX(0x6a6c78);
    const u32 L2 = HEX(0xb4b6c0), M2 = HEX(0x8a8c98), D2 = HEX(0x5c5e6a);
    const float cx = w * 0.5f - 0.5f;
    if (cls == 0) {
        remains::heap(im, r, cx, h - 3.5f, 3.2f, 1.2f, 2 + variant % 2, 1.5f, 2.1f, L2, M2, D2);
    } else if (cls == 1) {
        remains::heap(im, r, cx, h - 4.2f, 4.8f, 1.6f, 3 + variant % 2, 1.9f, 3.0f, variant == 1 ? L2 : L, variant == 1 ? M2 : M, variant == 1 ? D2 : D);
    } else {  // a boulder leaves a real heap: a few big blocks and smaller chips round them
        remains::heap(im, r, cx, h - 5.8f, 8.6f, 2.8f, 5 + variant, 2.8f, 4.4f, L, M, D);
        remains::heap(im, r, cx, h - 3.4f, 9.6f, 1.8f, 4, 1.5f, 2.2f, L2, M2, D2);
    }
    remains::pebbles(im, r, cx, h - 2.f, w * 0.42f, 1.f, cls == 0 ? 1 : 2 + cls, M, D);
    im.outline(OUT);
    return retintRock(im, theme);
}

// ---- ore and crystal --------------------------------------------------------------------------------------------------------------------------
struct OreRemainsLook {
    u32 l, m, d;   // light, mid and dark of the ore
    int kind;      // 0 nuggets, 1 lumps (coal), 2 shards (crystals, gems, ice)
    u32 spark;     // the glint on the best lump (0 = none)
    u32 pool;      // a flat smear on the ground (the frost round broken ice), 0 = none
};
// by NodeKind; the frost crystal (a prop, not a node) is oreRemainsLook(-1)
inline const OreRemainsLook* oreRemainsLook(int nodeKind) {
    static const OreRemainsLook IRON{HEX(0xe4a45a), HEX(0xb8683a), HEX(0x7a3e24), 0, HEX(0xffd8a0), 0};
    static const OreRemainsLook COPPER{HEX(0xf4b070), HEX(0xd8844a), HEX(0x9a5430), 0, HEX(0xffe0b8), 0};
    static const OreRemainsLook GOLD{HEX(0xfff09a), HEX(0xf2c94c), HEX(0xb08a20), 0, HEX(0xffffff), 0};
    static const OreRemainsLook AMBER{HEX(0xffd878), HEX(0xf0b040), HEX(0xa86a18), 0, HEX(0xfff4c8), 0};
    static const OreRemainsLook SILVER{HEX(0xffffff), HEX(0xdfe6f2), HEX(0x8a94aa), 0, HEX(0xffffff), 0};
    static const OreRemainsLook COAL{HEX(0x5a5a68), HEX(0x34343c), HEX(0x1e1e26), 1, HEX(0x8a8a9c), 0};
    static const OreRemainsLook CAVEC{HEX(0xe4c4ff), HEX(0xc890ff), HEX(0x7a48b8), 2, HEX(0xffffff), 0};
    static const OreRemainsLook GEM{HEX(0xffb0c8), HEX(0xff6a9a), HEX(0xb83a64), 2, HEX(0xffffff), 0};
    static const OreRemainsLook ICE{HEX(0xe8faff), HEX(0x9ad8ff), HEX(0x5a98c8), 2, HEX(0xffffff), HEX(0xcdeeff)};
    static const OreRemainsLook FROST{HEX(0xd8f6ff), HEX(0xa8e4ff), HEX(0x5aa8d8), 2, HEX(0xffffff), HEX(0xdff4ff)};
    switch (nodeKind) {
        case RN_ORE: return &IRON;
        case RN_COPPER: return &COPPER;
        case RN_GOLD: return &GOLD;
        case RN_AMBER: return &AMBER;
        case RN_SILVER: return &SILVER;
        case RN_COAL: return &COAL;
        case RN_CAVECRYSTAL: return &CAVEC;
        case RN_GEM: return &GEM;
        case RN_ETERNALICE: return &ICE;
        case -1: return &FROST;
        default: return nullptr;
    }
}
constexpr int ORE_REMAINS_W = 15, ORE_REMAINS_H = 10, ORE_REMAINS_AX = 7, ORE_REMAINS_AY = 8;

// a broken crystal: a pointed shard, lit on the left and in shadow on the right, standing (or leaning) on its stump
inline void shard(Img& im, int bx, int by, int h, int halfw, int lean, const OreRemainsLook& o) {
    for (int i = 0; i < h; i++) {
        const float t = (float)i / (float)h;  // 0 at the base
        const int hw = std::max(0, (int)roundf(halfw * (1.f - t * t)));
        const int cxl = bx + (int)roundf(lean * t);
        for (int x = -hw; x <= hw; x++) im.set(cxl + x, by - i, x < 0 ? o.l : (x > 0 ? o.d : o.m));
    }
    im.set(bx + lean, by - h, o.l);  // the tip
}

inline Img mkOreRemains(const OreRemainsLook& o, int variant) {
    Img im(ORE_REMAINS_W, ORE_REMAINS_H);
    remains::R r{0xA511E9B3u * (uint32_t)(variant + 3) + (uint32_t)o.m};
    const float cx = ORE_REMAINS_W * 0.5f - 0.5f, cy = ORE_REMAINS_H - 3.8f;
    if (o.pool) im.ellipse(cx, ORE_REMAINS_H - 2.5f, 6.2f, 1.7f, o.pool);
    // chips of the host rock the vein sat in
    remains::heap(im, r, cx, cy + 0.4f, 5.2f, 1.4f, 3, 1.6f, 2.5f, HEX(0xa4a6b2), HEX(0x7a7c88), HEX(0x4c4e5a));
    if (o.kind == 2) {
        const int n = 2 + (variant == 1);  // two or three broken shards of different heights
        const int hs[3][3] = {{5, 3, 0}, {4, 6, 3}, {6, 4, 0}}, xs[3][3] = {{-2, 2, 0}, {-3, 0, 3}, {-1, 3, 0}}, ls[3][3] = {{-1, 1, 0}, {-1, 0, 1}, {1, -1, 0}};
        for (int i = 0; i < n; i++)
            shard(im, (int)cx + xs[variant][i], ORE_REMAINS_H - 3 - (i == 1 ? 0 : 1), hs[variant][i], i == 0 ? 2 : 1, ls[variant][i], o);
    } else if (o.kind == 1) {  // coal: matte black lumps, one with a dull shine
        std::vector<remains::Chunk> cs;
        remains::heap(im, r, cx, cy - 0.2f, 4.6f, 1.5f, 4, 1.7f, 2.7f, o.l, o.m, o.d, &cs);
        if (!cs.empty()) im.set((int)(cs.back().x - 1), (int)(cs.back().y - 1), o.spark);
    } else {  // nuggets: rounded lumps of the metal with a glint
        std::vector<remains::Chunk> cs;
        remains::heap(im, r, cx, cy - 0.4f, 4.4f, 1.4f, 3 + (variant == 2), 1.2f, 2.0f, o.l, o.m, o.d, &cs);
        for (size_t i = 0; i < cs.size(); i += 2) im.set((int)(cs[i].x - cs[i].rx * 0.4f), (int)(cs[i].y - cs[i].ry * 0.5f), o.spark);
    }
    remains::pebbles(im, r, cx, ORE_REMAINS_H - 2.f, 6.f, 1.f, 2, o.m, o.d);
    im.outline(OUT);
    return im;
}
