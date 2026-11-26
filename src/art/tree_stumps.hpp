// tree_stumps.hpp - what every kind of tree leaves behind when it is felled. One stump per tree look, so an old oak leaves a wide
// mossy stump with root flares, a sapling a tiny one, a dead tree a grey splintered one, a snow pine a snow-capped one, a palm a
// fibrous one, a birch a pale one with dark bark marks, a cactus a green cut base and a fallen log a scatter of chips.
// Painted in code like the rest of the art. Needs Img, HEX and OUT (the outline colour) from the includes before it.
#pragma once
#include <algorithm>

enum StumpKind {
    STUMP_OAK,        // the three leafy oaks (lime, deep, soft)
    STUMP_OLDOAK,     // the big old oak: wide, root flares, moss
    STUMP_SAPLING,    // the young oak
    STUMP_DEADOAK,    // the dead oak: grey, cracked, splintered
    STUMP_PINE,       // the forest pine: reddish wood
    STUMP_PALM,       // the desert palm: pale fibrous wood, ring bands
    STUMP_SNOWPINE,   // the snow spruce: a cap of snow on the cut face
    STUMP_DEADSPRUCE, // the swamp's dead trees: grey and splintered
    STUMP_SNOWDEAD,   // the snowfield's dead spruces: the same, under snow
    STUMP_BIRCH,      // the snowfield's birches: pale bark, dark marks
    STUMP_CACTUS,     // the cactus: a green cut base with spines
    STUMP_LOG,        // the fallen log: chips of bark and wood
    STUMP_COUNT
};

struct StumpArt {
    Img img;
    int ax = 0, ay = 0;  // the anchor: the foot of the stump
};

struct StumpLook {
    int bw, bh;                  // width and height of the cut trunk
    u32 bark, barkLt, barkDk;    // the bark: middle, lit side (left), shaded side (right)
    u32 face, ring, core;        // the cut face, its growth rings and its heart
    int streaks;                 // dark bark streaks
    bool roots = false;          // root flares at the foot
    bool moss = false;           // moss on the shaded side and the roots
    bool jagged = false;         // splinters standing up round the cut
    bool cracked = false;        // cracks across the face
    bool snow = false;           // a pad of snow on the face and drifts at the foot
    bool bands = false;          // horizontal ring bands on the bark (palm)
    u32 mark = 0;                // dark bark marks (birch), 0 = none
    u32 spine = 0;               // pale spines (cactus), 0 = none
};

inline StumpArt mkTreeStump(const StumpLook& L, unsigned seed) {
    const int pad = L.roots ? 3 : 1;
    const int ry = L.bw >= 8 ? std::max(2, L.bw / 4) : (L.bw >= 5 ? 2 : 1);  // half the height of the cut face
    const int W = L.bw + 2 * pad + 2;
    const int H = L.bh + 2 * ry + 5;
    Img im(W, H);
    auto hh = [&](int a, int b) {
        unsigned h = seed ^ ((unsigned)a * 374761393u) ^ ((unsigned)b * 668265263u);
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    };
    const int cx = W / 2;
    const int x0 = cx - L.bw / 2, x1 = x0 + L.bw;  // trunk columns are x0 .. x1-1
    const int topY = 1 + ry + (L.jagged ? 2 : 0);   // centre row of the cut face
    const int footY = topY + L.bh;                  // last full row of the trunk
    const float rxB = L.bw * 0.5f;

    // ---- the trunk: a rectangle with a rounded foot, lit on the left and shaded on the right
    for (int y = topY; y <= footY + ry; y++)
        for (int x = x0; x < x1; x++) {
            if (y > footY) {
                const float dx = (x + 0.5f - cx) / rxB, dy = (y + 0.5f - footY) / (ry * 0.8f + 0.01f);
                if (dx * dx + dy * dy > 1.f) continue;
            }
            const float dx = (x + 0.5f - cx) / rxB;
            im.set(x, y, dx < -0.45f ? L.barkLt : (dx > 0.45f ? L.barkDk : L.bark));
        }
    // ---- bark streaks
    for (int k = 0; k < L.streaks; k++) {
        const int sx = x0 + 1 + (int)(hh(k, 1) % (unsigned)std::max(1, L.bw - 2));
        const int sy = topY + ry + (int)(hh(k, 2) % (unsigned)std::max(1, L.bh - 1));
        const int len = 2 + (int)(hh(k, 3) % 3u);
        for (int i = 0; i < len && sy + i <= footY; i++) im.set(sx, sy + i, L.barkDk);
    }
    // ---- ring bands round the trunk (palm)
    if (L.bands)
        for (int y = topY + ry + 1; y <= footY; y += 2)
            for (int x = x0; x < x1; x++) im.set(x, y, x < x0 + L.bw / 4 ? L.bark : L.barkDk);
    // ---- dark marks (birch): short dashes across the trunk
    if (L.mark)
        for (int y = topY + ry; y <= footY; y += 2)
            if (hh(y, 7) & 1u) {
                const int mx = x0 + (int)(hh(y, 8) % (unsigned)std::max(1, L.bw - 2));
                im.rect(mx, y, 2 + (int)(hh(y, 9) & 1u), 1, L.mark);
            }
    // ---- spines (cactus)
    if (L.spine)
        for (int y = topY + ry; y <= footY; y += 2)
            for (int x = x0 + ((y & 2) ? 1 : 0); x < x1; x += 3) im.set(x, y, L.spine);
    // ---- root flares
    if (L.roots)
        for (int s = 0; s < 2; s++) {
            const int dir = s ? 1 : -1, edge = s ? x1 - 1 : x0;
            const u32 c = s ? L.barkDk : L.barkLt;
            for (int i = 1; i <= pad; i++) {
                im.set(edge + dir * i, footY, c);
                if (i < pad) im.set(edge + dir * i, footY - 1, c);
                if (i < pad - 1) im.set(edge + dir * i, footY - 2, c);
            }
            im.set(edge + dir * (pad - 1), footY + 1, L.barkDk);  // the root tip dips into the ground
        }
    // ---- moss on the shaded side and over the roots
    if (L.moss) {
        const u32 m1 = HEX(0x6a8a3a), m2 = HEX(0x4e7a2e);
        for (int y = topY + ry; y <= footY + 1; y++)
            for (int x = x0 - pad; x < x1 + pad; x++)
                if (im.get(x, y) && (hh(x, y) % 5u) == 0u && (x > x1 - 3 || x < x0 + 2 || y >= footY - 1)) im.set(x, y, (hh(y, x) & 1u) ? m1 : m2);
    }
    // ---- the cut face
    const float fx = rxB + 0.4f, fy = ry + 0.4f;
    if (L.jagged) {  // splinters standing up round the cut
        for (int x = x0; x < x1; x++) {
            const int h = (int)(hh(x, 11) % 3u);  // 0..2 px above the face
            for (int i = 1; i <= h + 1; i++) im.set(x, topY - ry - i + 1, (x & 1) ? L.barkLt : L.bark);
        }
    }
    im.ellipse((float)cx, (float)topY, fx, fy, L.ring);
    im.ellipse((float)cx, (float)topY + 0.2f, fx - 1.2f, fy - 0.8f, L.face);
    if (L.bw >= 9) {
        im.ellipse((float)cx, (float)topY, fx * 0.55f, fy * 0.55f, L.ring);
        im.ellipse((float)cx, (float)topY + 0.2f, fx * 0.55f - 1.f, fy * 0.55f - 0.7f, L.face);
    }
    if (L.bw >= 15) im.ellipse((float)cx, (float)topY, fx * 0.25f, fy * 0.3f, L.ring);
    im.set(cx, topY, L.core);
    if (L.cracked) {
        im.line(cx, topY, cx + L.bw / 3 + 1, topY + ry - 1, L.core);
        im.line(cx, topY, cx - L.bw / 4, topY - ry + 1, L.core);
    }
    // ---- snow: a pad on the cut face, drifts against the foot
    if (L.snow) {
        const u32 S1 = HEX(0xf4f8fc), S2 = HEX(0xdbe6f2);
        im.ellipse((float)cx, (float)topY - 0.2f, fx + 0.2f, fy + 0.2f, S1);
        im.ellipse((float)cx, (float)topY + 0.5f, fx * 0.7f, fy * 0.55f, S2);
        im.set(cx - 2, topY - 1, HEX(0xffffff)); im.set(cx + 2, topY, HEX(0xffffff));
        im.rect(x0, footY - 1, 3, 1, S2); im.rect(x1 - 3, footY, 3, 1, S1);
        im.rect(x0 - 1, footY + 1, 2, 1, S1); im.rect(x1 - 1, footY + 1, 2, 1, S2);
    }
    im.outline(OUT);
    return {im, cx, footY - 1};
}

// the fallen log leaves no stump: only chips of bark and wood, a curl of bark and a little moss
inline StumpArt mkLogRemains() {
    Img im(18, 9);
    const u32 W1 = HEX(0xe0b878), W2 = HEX(0xc89a58), B1 = HEX(0x8a5a2c), B2 = HEX(0x6a4220), B3 = HEX(0x4a2c14), M1 = HEX(0x6a8a3a), M2 = HEX(0x4e7a2e);
    im.rect(3, 4, 3, 2, B1); im.rect(3, 4, 3, 1, B2);        // a long bark strip
    im.rect(6, 5, 2, 1, B2);
    im.rect(10, 3, 2, 2, W1); im.set(11, 3, W2);              // fresh pale chips
    im.rect(13, 5, 2, 1, W1); im.set(14, 5, W2);
    im.rect(7, 2, 2, 1, W2);
    im.rect(1, 6, 2, 1, W1);
    im.set(9, 6, B3); im.set(5, 3, B3); im.set(12, 6, B2); im.set(15, 4, B2);
    im.rect(4, 6, 3, 1, M1); im.set(5, 7, M2); im.rect(11, 6, 1, 1, M2);  // moss where the log lay
    im.outline(OUT);
    return {im, 9, 6};
}

inline StumpArt mkStumpArt(int kind) {
    // StumpLook: bw, bh, bark, barkLt, barkDk, face, ring, core, streaks, then the flags (roots, moss, jagged, cracked, snow, bands, mark, spine)
    switch (kind) {
        case STUMP_OAK:
            return mkTreeStump({10, 6, HEX(0x6b4439), HEX(0x95604f), HEX(0x402922), HEX(0xe0b878), HEX(0xc89a58), HEX(0x8a5a2c), 3}, 0x0a01u);
        case STUMP_OLDOAK: {
            StumpLook l{18, 8, HEX(0x6b4439), HEX(0x95604f), HEX(0x402922), HEX(0xd8ac68), HEX(0xb88a48), HEX(0x7a4a24), 7};
            l.roots = true; l.moss = true;
            return mkTreeStump(l, 0x0a02u);
        }
        case STUMP_SAPLING:
            return mkTreeStump({4, 3, HEX(0x95604f), HEX(0xc07b66), HEX(0x6b4439), HEX(0xe8c888), HEX(0xd0a060), HEX(0x8a5a2c), 1}, 0x0a03u);
        case STUMP_DEADOAK: {
            StumpLook l{8, 4, HEX(0x705a4c), HEX(0x96806e), HEX(0x4c3a31), HEX(0x7a6a5c), HEX(0x5e4e44), HEX(0x2e221e), 3};
            l.jagged = true; l.cracked = true;
            return mkTreeStump(l, 0x0a04u);
        }
        case STUMP_PINE:
            return mkTreeStump({8, 6, HEX(0x7a4a36), HEX(0x9a6048), HEX(0x45281e), HEX(0xeab57a), HEX(0xd08c58), HEX(0xa8602c), 4}, 0x0a05u);
        case STUMP_PALM: {
            StumpLook l{6, 4, HEX(0xa8884a), HEX(0xc8a860), HEX(0x7a6030), HEX(0xd8c890), HEX(0xb09a5c), HEX(0x6a5a30), 0};
            l.jagged = true; l.bands = true;
            return mkTreeStump(l, 0x0a06u);
        }
        case STUMP_SNOWPINE: {
            StumpLook l{8, 6, HEX(0x6a4a34), HEX(0x8a6444), HEX(0x3c2618), HEX(0xe8b880), HEX(0xcf9058), HEX(0xa8602c), 3};
            l.snow = true;
            return mkTreeStump(l, 0x0a07u);
        }
        case STUMP_DEADSPRUCE: {
            StumpLook l{6, 3, HEX(0x5a4a3a), HEX(0x7a6a5a), HEX(0x3a2f24), HEX(0x6a5c4e), HEX(0x4e4034), HEX(0x2a2018), 2};
            l.jagged = true; l.cracked = true;
            return mkTreeStump(l, 0x0a08u);
        }
        case STUMP_SNOWDEAD: {
            StumpLook l{6, 3, HEX(0x5a4a3a), HEX(0x7a6a5a), HEX(0x3a2f24), HEX(0x6a5c4e), HEX(0x4e4034), HEX(0x2a2018), 2};
            l.jagged = true; l.snow = true;
            return mkTreeStump(l, 0x0a09u);
        }
        case STUMP_BIRCH: {
            StumpLook l{6, 4, HEX(0xd9d5cc), HEX(0xf0ede6), HEX(0xb4afa5), HEX(0xe8d8b8), HEX(0xc8b890), HEX(0x9a8a68), 0};
            l.mark = HEX(0x38342f); l.snow = true;
            return mkTreeStump(l, 0x0a0au);
        }
        case STUMP_CACTUS: {
            StumpLook l{6, 4, HEX(0x4f9a3e), HEX(0x72bb62), HEX(0x2f6a2c), HEX(0xb8d890), HEX(0x8ec06a), HEX(0x6a9a48), 0};
            l.spine = HEX(0xf2ecc8);
            return mkTreeStump(l, 0x0a0bu);
        }
        default: return mkLogRemains();
    }
}
