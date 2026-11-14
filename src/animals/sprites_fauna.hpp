// sprites_fauna.hpp - procedural art for every animal species, every form and every rarity tier.
// Included from sprites.hpp after specWolf. Each species has a QuadSpec (even the non-quadrupeds use
// it as a palette + dimensions record); the painter is picked by AnimalDef::art. Rarity tiers recolour the spec
// ("looks") and scale it, so a rare albino wolf is the same painter with different colours and a bigger body.
#pragma once
#include "animals/animal_anim.hpp"  // animalClips(): which species are drawn by the rig

// ================================================================= spec helper
inline QuadSpec mkQ(int W, int H, float cx, float bw, float bh, int legLen, int legW, u32 body, u32 light, u32 belly, u32 dark, u32 hoof,
                    u32 snout, u32 eye, float hr, float hdx, float hdy, float snoutLen, int ear, int tail) {
    QuadSpec s{};
    s.W = W; s.H = H; s.cx = cx; s.bw = bw; s.bh = bh; s.legLen = legLen; s.legW = legW;
    s.body = body; s.light = light; s.belly = belly; s.dark = dark; s.hoof = hoof; s.snout = snout; s.eye = eye;
    s.hr = hr; s.hdx = hdx; s.hdy = hdy; s.snoutLen = snoutLen; s.ear = ear; s.tail = tail;
    return s;
}

// ================================================================= species specs (facing right)
inline QuadSpec fauSpec0(int kind, int form) {
    switch (kind) {
        case A_RABBIT: return specRabbit();
        case A_DEER: return specDeer(form == 1);
        case A_BOAR: return specBoar();
        case A_WOLF: {
            QuadSpec s = specWolf();
            if (form == 1) {  // arctic wolf
                s.body = HEX(0xe6eaee); s.light = HEX(0xf8fafc); s.belly = HEX(0xffffff); s.dark = HEX(0xaab3be);
                s.hoof = HEX(0x59606c); s.snout = HEX(0xe0e4e8);
            }
            return s;
        }
        // ---- desert
        case A_HARE: {
            QuadSpec s = mkQ(26, 20, 10, 11, 7, 4, 1, HEX(0xc9a574), HEX(0xdcbf94), HEX(0xf0e4cc), HEX(0x9c7c52), HEX(0x6d563c), HEX(0xf0e4cc), HEX(0x241a14), 3.2f, 0.5f, 0.5f, 0, 1, 1);
            return s;
        }
        case A_FENNEC: return mkQ(30, 22, 12, 12, 5.5f, 5, 1, HEX(0xe8cfa0), HEX(0xf4e2bc), HEX(0xfff4dc), HEX(0xb8946a), HEX(0x6d563c), HEX(0xf4e2bc), HEX(0x241a14), 3, 2.5f, -1, 3, 3, 3);
        case A_COYOTE: return mkQ(36, 26, 14, 15, 6.5f, 8, 2, HEX(0xa08a6a), HEX(0xb9a584), HEX(0xe0d2b6), HEX(0x6f5c42), HEX(0x2e2118), HEX(0xd0c4a8), HEX(0xf2c94c), 3.3f, 3, -1, 4.5f, 3, 3);
        case A_DUNELION: {
            QuadSpec s = mkQ(54, 34, 22, 22, 9.5f, 9, 3, HEX(0xd9b26a), HEX(0xeacb8c), HEX(0xf2e2b8), HEX(0x8a6430), HEX(0x5a4020), HEX(0xe8cfa0), HEX(0xf2c94c), 4.4f, 3.5f, -1.5f, 3, 2, 5);
            s.mane = 1;
            return s;
        }
        case A_RATTLER: {
            QuadSpec s = mkQ(36, 14, 18, 26, 4, 0, 0, HEX(0xb9a06a), HEX(0xd2bd88), HEX(0xeee0b0), HEX(0x6e5636), HEX(0x6e5636), HEX(0xe56b7a), HEX(0xf2c94c), 2.6f, 0, 0, 0, 0, 9);
            return s;
        }
        case A_ROADRUNNER: {
            QuadSpec s = mkQ(40, 30, 17, 15, 6, 10, 1, HEX(0x8a7a5e), HEX(0xa89878), HEX(0xe0d6bc), HEX(0x4e4234), HEX(0xc9a05a), HEX(0xd8b050), HEX(0xf2c94c), 3, 1, -2.5f, 5, 1, 9);
            s.horns = 1;  // bird type: roadrunner
            s.spots = true;
            return s;
        }
        case A_LIZARD: return mkQ(30, 12, 13, 10, 4, 0, 0, HEX(0xb59a5a), HEX(0xd0b878), HEX(0xeee0b0), HEX(0x7a6232), HEX(0x7a6232), HEX(0xe56b7a), HEX(0x241a14), 2.2f, 0, 0, 0, 0, 0);
        // ---- swamp
        case A_FROG: return mkQ(14, 12, 6, 8, 5, 0, 0, HEX(0x4f8a3a), HEX(0x6fae4c), HEX(0xd8e8a8), HEX(0x2f5a26), HEX(0x2f5a26), HEX(0xe56b7a), HEX(0xf2c94c), 2.2f, 0, 0, 0, 0, 0);
        case A_DUCK: {
            QuadSpec s = mkQ(32, 22, 13, 14, 8, 3, 1, HEX(0x8c8570), HEX(0xa8a088), HEX(0xd8d2c0), HEX(0x5e5848), HEX(0xe8963a), HEX(0xe8c040), HEX(0x241a14), 3.1f, 2.5f, -3.5f, 3.5f, 0, 3);
            s.horn = HEX(0x2f7a4a);  // green head
            s.horns = 0;
            return s;
        }
        case A_MARSHDEER: {
            QuadSpec s = mkQ(34, 38, 13, 16, 8, 10, 2, HEX(0x8a7650), HEX(0xa08c62), HEX(0xd2c4a2), HEX(0x5e4f35), HEX(0x2e2118), HEX(0xcdbb94), HEX(0x241a14), 3.0f, 3.2f, -6, 3.2f, 2, 2);
            s.antlers = form == 1 ? 1 : 0;
            s.spots = form != 1;
            return s;
        }
        case A_MOCCASIN: return mkQ(36, 14, 18, 26, 4, 0, 0, HEX(0x5a5238), HEX(0x756c48), HEX(0xd2c690), HEX(0x2f2a1a), HEX(0x2f2a1a), HEX(0xe56b7a), HEX(0xf2c94c), 2.6f, 0, 0, 0, 0, 0);
        case A_GATOR: return mkQ(64, 20, 28, 26, 7, 2, 2, HEX(0x4a6a3a), HEX(0x65884a), HEX(0xc8c088), HEX(0x2f4526), HEX(0x2f4526), HEX(0x4a6a3a), HEX(0xf2c94c), 3.4f, 0, 0, 12, 0, 0);
        case A_PANTHER: return mkQ(48, 28, 21, 20, 8.5f, 7, 2, HEX(0x2f2d3a), HEX(0x44415a), HEX(0x232030), HEX(0x1a1822), HEX(0x14121a), HEX(0x3a3846), HEX(0xb6f23a), 4, 3, -1, 2.8f, 2, 5);
        // ---- snow
        case A_ARCTICFOX: return mkQ(30, 22, 12, 12, 5.5f, 5, 1, HEX(0xf4f6f8), HEX(0xffffff), HEX(0xffffff), HEX(0xc3ccd6), HEX(0x8a95a2), HEX(0xdde3ea), HEX(0x241a14), 3, 2.5f, -1, 3, 4, 3);
        case A_SNOWHARE: return mkQ(26, 20, 10, 11, 7, 4, 1, HEX(0xeef1f4), HEX(0xffffff), HEX(0xffffff), HEX(0xbfc8d2), HEX(0x8a95a2), HEX(0xdde3ea), HEX(0x241a14), 3.2f, 0.5f, 0.5f, 0, 1, 1);
        case A_REINDEER: {
            QuadSpec s = mkQ(38, 44, 15, 19, 9, 10, 2, HEX(0x8d7860), HEX(0xa6917a), HEX(0xe9e2d6), HEX(0x65523f), HEX(0x2e2118), HEX(0xd9c9b0), HEX(0x241a14), 3.4f, 3.8f, -6, 3.5f, 2, 2);
            s.antlers = form == 1 ? 2 : 1;
            return s;
        }
        case A_MOOSE: {
            QuadSpec s = mkQ(54, 52, 22, 26, 12, 15, 3, HEX(0x4e3b2c), HEX(0x654d3a), HEX(0x3a2b20), HEX(0x2a1f16), HEX(0x1a1410), HEX(0x6a5846), HEX(0xf2c94c), 5, 5, -7, 6, 2, 4);
            if (form == 1) {
                s.horns = 3;
                s.horn = HEX(0xcdb98a);
            }
            return s;
        }
        case A_LYNX: {
            QuadSpec s = mkQ(38, 28, 15, 15, 7, 7, 2, HEX(0xb59a76), HEX(0xcdb590), HEX(0xeee2c8), HEX(0x7a6446), HEX(0x4a3a28), HEX(0xd9c9a8), HEX(0xf2c94c), 3.6f, 2.5f, -1, 2.5f, 5, 4);
            s.spots = true;
            return s;
        }
        case A_POLARBEAR: return mkQ(56, 38, 22, 27, 14, 6, 4, HEX(0xf1f2ee), HEX(0xffffff), HEX(0xdfe4e6), HEX(0xb7c0c8), HEX(0x2a2a30), HEX(0xe6e2da), HEX(0x241a14), 5.4f, 4, 1, 5, 4, 4);
        // ---- mountain
        case A_GOAT: {
            QuadSpec s = mkQ(34, 34, 13, 16, 8, 10, 2, HEX(0xeeeae0), HEX(0xfffcf4), HEX(0xdcd6c6), HEX(0xb8b09c), HEX(0x2e2e32), HEX(0xdcd6c6), HEX(0xf2c94c), 3.2f, 3.5f, -4.5f, 3.2f, 2, 4);
            s.horns = 1;
            return s;
        }
        case A_BIGHORN: {
            QuadSpec s = mkQ(40, 36, 15, 19, 9.5f, 9, 2, HEX(0x8a7459), HEX(0xa38b6c), HEX(0xe6dac0), HEX(0x5f4d38), HEX(0x2e2118), HEX(0xe6dac0), HEX(0xf2c94c), 3.8f, 3.5f, -3, 3.2f, 4, 4);
            s.horns = 2;
            s.horn = HEX(0xd9c9a0);
            return s;
        }
        case A_EAGLE: {
            QuadSpec s = mkQ(44, 36, 18, 14, 9, 6, 1, HEX(0x5a4228), HEX(0x7a5e3a), HEX(0xa0825a), HEX(0x3a2a18), HEX(0xf2c94c), HEX(0xf2c94c), HEX(0xf2c94c), 3.6f, 1.5f, -6, 4, 0, 8);
            s.horn = HEX(0xe8d6a0);  // pale head
            s.horns = 2;             // bird type: eagle
            return s;
        }
        case A_MARMOT: return mkQ(26, 18, 10, 11, 6.5f, 3, 2, HEX(0x8a6e52), HEX(0xa68a6a), HEX(0xd8c3a2), HEX(0x5e4834), HEX(0x3a2a1c), HEX(0xd8c3a2), HEX(0x241a14), 3, 1, 0.5f, 1.5f, 4, 4);
        case A_MTLION: {
            QuadSpec s = mkQ(52, 32, 21, 21, 9, 8, 3, HEX(0xb89a72), HEX(0xcfb48c), HEX(0xeee0c0), HEX(0x7a6244), HEX(0x4a3a28), HEX(0xdcc8a4), HEX(0xf2c94c), 4.2f, 3.5f, -1.5f, 2.8f, 2, 5);
            return s;
        }
        default: return specRabbit();
    }
}

// ================================================================= looks (rarity recolours)
inline void lookOp(u32& c, int look, int slot) {  // slot: 0 body,1 light,2 belly,3 dark,4 hoof/legs,5 snout,6 eye
    if (!c) return;
    switch (look) {
        case LK_DARK: c = shadeC(c, slot == 3 ? 0.62f : (slot == 2 ? 0.78f : 0.72f)); break;
        case LK_PALE: c = mixc(c, HEX(0xffffff), slot == 3 ? 0.25f : 0.35f); break;
        case LK_RUSTY: {
            static const u32 t[7] = {HEX(0xb8602e), HEX(0xd68a4a), HEX(0xf0d2a8), HEX(0x7a3a1a), HEX(0x4a2a18), HEX(0xe8c090), 0};
            if (slot < 6) c = mixc(c, t[slot], 0.5f);
            break;
        }
        default: break;
    }
}
inline void applyLook(QuadSpec& s, int look) {
    if (look == LK_NONE) return;
    if (look == LK_DARK || look == LK_PALE || look == LK_RUSTY) {
        lookOp(s.body, look, 0); lookOp(s.light, look, 1); lookOp(s.belly, look, 2); lookOp(s.dark, look, 3);
        lookOp(s.hoof, look, 4); lookOp(s.snout, look, 5); lookOp(s.horn, look, 0);
        return;
    }
    struct Pal {
        u32 body, light, belly, dark, hoof, snout, eye;
    } P{};
    switch (look) {
        case LK_ALBINO: P = {HEX(0xf1ece4), HEX(0xffffff), HEX(0xffffff), HEX(0xd2c8bc), HEX(0xb9aca0), HEX(0xe9b4b0), HEX(0xe0344a)}; break;
        case LK_MELANISTIC: P = {HEX(0x2a2a31), HEX(0x3b3b45), HEX(0x1f1f25), HEX(0x15151a), HEX(0x0e0e12), HEX(0x3a3a40), HEX(0xffd24a)}; break;
        case LK_GOLD: P = {HEX(0xe0b440), HEX(0xffe28a), HEX(0xfff0b8), HEX(0xa87a1c), HEX(0x7a5510), HEX(0xf2d890), HEX(0xfff7d0)}; break;
        case LK_GHOST: P = {HEX(0xc9e6f0), HEX(0xe8fbff), HEX(0xffffff), HEX(0x8fb4c4), HEX(0x6f94a4), HEX(0xdff4fa), HEX(0x6ff0ff)}; break;
        case LK_FROST: P = {HEX(0xa9cdea), HEX(0xdcf0ff), HEX(0xf2fbff), HEX(0x6f93b8), HEX(0x4e6e92), HEX(0xcfe6f6), HEX(0xffffff)}; break;
        default: P = {HEX(0xd9522a), HEX(0xff9a4a), HEX(0xffd08a), HEX(0x7a2412), HEX(0x4a140a), HEX(0xffb070), HEX(0xffe45a)}; break;  // EMBER
    }
    s.body = P.body; s.light = P.light; s.belly = P.belly; s.dark = P.dark; s.hoof = P.hoof; s.snout = P.snout; s.eye = P.eye;
    if (s.horn) s.horn = (s.horns == 1 || s.horns == 2 || s.horns == 3) && s.antlers == 0 && s.mane == 0 && s.horns != 0 ? s.horn : P.light;
}

inline QuadSpec scaleSpec(QuadSpec s, float k) {
    s.W = (int)ceilf(s.W * k);
    s.H = (int)ceilf(s.H * k);
    s.cx *= k; s.bw *= k; s.bh *= k;
    s.legLen = (int)roundf(s.legLen * k);
    if (s.legW >= 2) s.legW = std::max(2, (int)roundf(s.legW * k));
    s.hr *= k; s.hdx *= k; s.hdy *= k; s.snoutLen *= k;
    return s;
}

inline QuadSpec fauSpec(int kind, int form, int tier) {
    QuadSpec s = fauSpec0(kind, form);
    if (tier > 0) {
        applyLook(s, ANIMAL[kind].looks[tier - 1]);
        s = scaleSpec(s, TIER[tier].scale);
    }
    return s;
}

// ================================================================= non-quadruped painters
inline Img drawSnake(const QuadSpec& s, int frame, bool dead) {
    Img im(s.W, s.H);
    int n = (int)s.bw;
    float baseY = s.H - 4.5f;
    float fx = frame * 1.5708f;
    float headY = baseY, headX = s.cx + s.bw / 2;
    for (int i = 0; i < n; i++) {
        float t = i / (float)(n - 1);  // 0 tail .. 1 head
        float x = s.cx - s.bw / 2 + i;
        float y = baseY - 0.5f - 1.7f * sinf(t * 7.f + fx) * (0.35f + 0.65f * t);
        float th = 0.9f + 1.9f * std::min(1.f, t * 2.4f);
        im.ellipse(x, y, th, th, s.body);
        im.set((int)x, (int)(y - th * 0.6f), s.light);
        im.set((int)x, (int)(y + th * 0.7f), s.belly);
        if (i % 5 == 2) {  // dark diamond band
            im.set((int)x, (int)(y - th * 0.3f), s.dark);
            im.set((int)x + 1, (int)(y - th * 0.3f), s.dark);
        }
        if (i == n - 1) headY = y;
    }
    if (s.tail == 9) {  // rattle
        float x = s.cx - s.bw / 2;
        float y = baseY - 0.5f - 1.7f * sinf(fx) * 0.35f;
        for (int k = 1; k <= 3; k++) im.ellipse(x - k * 1.6f, y, 1.1f, 1.3f, k % 2 ? HEX(0xe8dcc0) : HEX(0xb8aa88));
    }
    float hx = headX + 1.5f;
    im.ellipse(hx, headY, s.hr + 0.8f, s.hr * 0.85f, s.body);
    im.ellipse(hx - 0.5f, headY - 1, s.hr * 0.6f, s.hr * 0.35f, s.light);
    int ex = (int)(hx + 0.8f), ey = (int)(headY - 0.8f);
    im.set(ex, ey, s.eye);
    if (dead) {
        im.set(ex + 1, ey + 1, s.eye);
        im.set(ex - 1, ey - 1, s.eye);
    }
    if (frame & 1) {  // tongue flick
        int tx = (int)(hx + s.hr + 1);
        im.line(tx, (int)headY, tx + 2, (int)headY, s.snout);
        im.set(tx + 3, (int)headY - 1, s.snout);
        im.set(tx + 3, (int)headY + 1, s.snout);
    }
    im.outline(OUT);
    if (dead) im = im.flippedV();
    return im;
}

inline Img drawFrog(const QuadSpec& s, int frame, bool dead) {
    Img im(s.W, s.H);
    int ground = s.H - 2;
    bool leap = frame == 1 || frame == 2;
    float cy = ground - s.bh / 2 - (leap ? 1.5f : 0.f), cx = s.cx;
    // hind leg
    if (leap) {
        im.line((int)(cx - 1), (int)cy + 1, (int)(cx - 6), ground, s.dark);
        im.line((int)(cx - 1), (int)cy + 2, (int)(cx - 6), ground - 1, s.dark);
        im.set((int)(cx - 7), ground, s.dark);
    } else {
        im.ellipse(cx - s.bw * 0.28f, cy + 1.2f, 3.f, 2.4f, s.dark);
        im.line((int)(cx - s.bw * 0.3f), ground, (int)(cx - s.bw * 0.05f), ground, s.dark);
    }
    // body
    for (int y = 0; y < s.H; y++)
        for (int x = 0; x < s.W; x++) {
            float dx = (x + 0.5f - cx) / (s.bw / 2), dy = (y + 0.5f - cy) / (s.bh / 2);
            if (dx * dx + dy * dy > 1.f) continue;
            float t = (dy + 1.f) * 0.5f;
            im.set(x, y, t < 0.3f ? s.light : (t > 0.7f ? s.belly : s.body));
        }
    // spots
    im.set((int)(cx - 2), (int)(cy - 1), s.dark);
    im.set((int)(cx), (int)(cy - 2), s.dark);
    im.set((int)(cx + 1), (int)(cy), s.dark);
    // head bump + eyes
    float hx = cx + s.bw / 2 - 1.f;
    im.ellipse(hx, cy - 0.5f, 2.8f, 2.3f, s.body);
    im.ellipse(hx - 0.5f, cy - 2.8f, 1.6f, 1.6f, s.light);
    im.set((int)hx, (int)(cy - 3), s.eye);
    if (dead) {
        im.set((int)hx, (int)(cy - 3), s.eye);
        im.set((int)hx + 1, (int)(cy - 2), s.eye);
        im.set((int)hx - 1, (int)(cy - 4), s.eye);
    }
    im.line((int)(hx), (int)(cy + 0.8f), (int)(hx + 3), (int)(cy + 0.8f), s.dark);  // mouth
    im.line((int)(cx + 2), (int)(cy + 2), (int)(cx + 4), ground, s.body);              // front leg
    im.outline(OUT);
    if (dead) im = im.flippedV();
    return im;
}

inline Img drawLizard(const QuadSpec& s, int frame, bool dead) {
    Img im(s.W, s.H);
    int ground = s.H - 2;
    float cy = ground - 2.2f, cx = s.cx;
    float fx = frame * 1.5708f;
    // tail
    for (int i = 0; i < 15; i++) {
        float t = i / 14.f;
        float x = cx - s.bw / 2 - i * 1.15f, y = cy + 0.5f + sinf(i * 0.55f + fx) * 1.1f;
        float r = 1.7f * (1.f - t) + 0.45f;
        im.ellipse(x, y, r, r, i % 4 == 3 ? s.dark : s.body);
    }
    // legs (alternate)
    static const int sh[4] = {0, 1, 0, -1};
    for (int k = 0; k < 4; k++) {
        float lx = cx + (k < 2 ? 2.5f : -2.5f);
        int ph = (frame + k) & 3;
        int dx = sh[ph] + (k & 1 ? 1 : -1);
        im.line((int)lx, (int)cy, (int)lx + dx, ground, s.dark);
        im.set((int)lx + dx + 1, ground, s.dark);
    }
    im.ellipse(cx, cy, s.bw / 2, s.bh / 2 + 0.6f, s.body);
    im.ellipse(cx, cy - 1.3f, s.bw / 2 - 1, 1.2f, s.light);
    im.ellipse(cx, cy + 1.4f, s.bw / 2 - 1, 1.0f, s.belly);
    for (int i = -3; i <= 3; i += 2) im.set((int)(cx + i), (int)(cy - 1.5f), s.dark);  // back markings
    // head
    float hx = cx + s.bw / 2 + 1.6f;
    im.ellipse(hx, cy - 0.3f, 3.6f, 2.2f, s.body);
    im.ellipse(hx + 0.5f, cy - 1.2f, 2.2f, 0.9f, s.light);
    int ex = (int)(hx + 0.5f), ey = (int)(cy - 1.2f);
    im.set(ex, ey, s.eye);
    if (dead) {
        im.set(ex + 1, ey + 1, s.eye);
        im.set(ex - 1, ey - 1, s.eye);
    }
    im.outline(OUT);
    if (dead) im = im.flippedV();
    return im;
}

inline Img drawGator(const QuadSpec& s, int frame, bool dead) {
    Img im(s.W, s.H);
    int ground = s.H - 2;
    float cy = ground - s.bh / 2 - 1.5f, cx = s.cx;
    float fx = frame * 1.5708f;
    // tail (tapering, swaying)
    for (int i = 0; i < 20; i++) {
        float t = i / 19.f;
        float x = cx - s.bw / 2 + 1 - i * 1.25f, y = cy + 0.5f + sinf(i * 0.4f + fx) * 1.6f * t;
        float r = (s.bh / 2) * (1.f - t * 0.82f) + 0.4f;
        im.ellipse(x, y, r, r, i % 3 == 0 ? s.dark : s.body);
    }
    // legs
    static const int sh[4] = {0, 1, 0, -1};
    for (int k = 0; k < 2; k++) {
        float lx = cx + (k ? 7.f : -7.f);
        int ph = (frame + k * 2) & 3;
        im.rect((int)lx - 1 + sh[ph], (int)cy + 1, 3, ground - (int)cy, s.dark);
        im.rect((int)lx - 2 + sh[ph], ground, 5, 1, s.dark);
    }
    // body
    for (int y = 0; y < s.H; y++)
        for (int x = 0; x < s.W; x++) {
            float dx = (x + 0.5f - cx) / (s.bw / 2), dy = (y + 0.5f - cy) / (s.bh / 2);
            if (dx * dx + dy * dy > 1.f) continue;
            float t = (dy + 1.f) * 0.5f;
            u32 c = t < 0.3f ? s.light : (t > 0.72f ? s.belly : s.body);
            if (t < 0.55f && ((x + y * 3) % 4 == 0)) c = s.dark;  // scutes
            im.set(x, y, c);
        }
    for (int x = (int)(cx - s.bw / 2 + 2); x < (int)(cx + s.bw / 2 - 1); x += 3) {  // ridge bumps
        im.set(x, (int)(cy - s.bh / 2) - 1, s.dark);
        im.set(x + 1, (int)(cy - s.bh / 2) - 1, s.dark);
    }
    // head with a long snout
    float hx = cx + s.bw / 2 - 1.f;
    float sl = s.snoutLen;
    im.ellipse(hx + sl * 0.5f, cy + 0.6f, sl * 0.62f + 2.5f, s.bh * 0.38f, s.body);
    im.ellipse(hx + sl * 0.5f, cy - 0.5f, sl * 0.55f + 2.f, s.bh * 0.2f, s.light);
    im.ellipse(hx + 1.5f, cy - s.bh * 0.35f, 2.f, 1.5f, s.light);  // eye bump
    int ex = (int)(hx + 2), ey = (int)(cy - s.bh * 0.35f - 0.5f);
    im.set(ex, ey, s.eye);
    if (dead) {
        im.set(ex + 1, ey + 1, s.eye);
        im.set(ex - 1, ey - 1, s.eye);
    }
    im.line((int)(hx + 2), (int)(cy + s.bh * 0.15f), (int)(hx + sl + 3), (int)(cy + s.bh * 0.15f), s.dark);  // mouth line
    for (int i = 0; i < (int)sl; i += 2) im.set((int)(hx + 3 + i), (int)(cy + s.bh * 0.15f) + 1, HEX(0xf3eccb));  // teeth
    im.set((int)(hx + sl + 2), (int)(cy - 1), s.dark);                                                           // nostril
    im.outline(OUT);
    if (dead) im = im.flippedV();
    return im;
}

// ---------------------------------------------------------------- the crawlers seen from the front and the back
// The side art above only faces east / west. These two views (down the screen = towards the hunter, up = away) lie the body along
// the screen's vertical: head first at the bottom (towards the viewer, with eyes) or at the top (away, back of the head).
inline Img drawCrawlerNS(int art, const QuadSpec& s, int frame, bool dead, bool back) {
    const bool snake = art == ART_SNAKE, frog = art == ART_FROG, gator = art == ART_GATOR;
    const int len = frog ? 12 : (int)(s.W * (snake ? 0.8f : 0.9f));
    const int bwid = std::max(4, (int)(s.bh * (snake ? 0.9f : 1.5f)));
    Img im(std::max(bwid + 14, 16), len + 8);
    const float cx = im.w / 2.f, top = 3.f, bot = (float)im.h - 3.f;
    const float ph = frame * 1.5708f;
    auto headAt = [&](float y) { return back ? y : (bot - (y - top)); };  // body is drawn tail-at-top then flipped for the back view
    // the body is built from the tail end (far from the head) to the head: y runs 0..1
    const int segs = std::max(8, len);
    auto sx = [&](float t) { return cx + (snake ? sinf(t * 9.f + ph) * 2.4f : (frog ? 0.f : sinf(t * 7.f + ph) * 0.9f)); };
    auto yAt = [&](float t) { return back ? bot - t * (bot - top) : top + t * (bot - top); };  // t 1 = head end
    if (frog) {
        const float yy = back ? bot - 5 : top + 6;
        // hind legs folded at the sides, front legs under the chin
        im.ellipse(cx - 5.5f, yy + (back ? -1.f : 4.f), 2.6f, 3.4f, s.dark);
        im.ellipse(cx + 5.5f, yy + (back ? -1.f : 4.f), 2.6f, 3.4f, s.dark);
        im.ellipse(cx, yy + 2, 5.5f, 5.0f, s.body);
        im.ellipse(cx, yy + 4.5f, 3.6f, 2.8f, back ? s.body : s.belly);
        im.ellipse(cx, yy - 2.5f, 4.2f, 3.0f, s.light);
        im.ellipse(cx - 2.8f, yy - 4.5f, 1.8f, 1.8f, s.body);  // eye bumps
        im.ellipse(cx + 2.8f, yy - 4.5f, 1.8f, 1.8f, s.body);
        if (!back) {
            im.set((int)(cx - 3), (int)(yy - 5), dead ? s.dark : s.eye);
            im.set((int)(cx + 2), (int)(yy - 5), dead ? s.dark : s.eye);
        }
        im.ellipse(cx - 3.5f, yy + 7.5f, 1.8f, 1.2f, s.dark);  // front feet
        im.ellipse(cx + 3.5f, yy + 7.5f, 1.8f, 1.2f, s.dark);
    } else {
        // tail and body: a tapering chain of discs, light on the back and belly colour at the edges
        for (int i = 0; i < segs; i++) {
            const float t = i / (float)(segs - 1);                       // 0 tail tip .. 1 head
            const float w = (snake ? 0.55f + 0.45f * sinf(std::min(1.f, t * 1.4f) * 1.5708f) : (t < 0.45f ? 0.25f + t * 1.1f : 1.f)) * bwid * 0.5f;
            const float x = sx(t), y = yAt(t);
            im.ellipse(x, y, std::max(1.2f, w), 1.7f, s.body);
            if (w > 2.2f) im.ellipse(x, y, std::max(0.8f, w * 0.5f), 1.2f, s.light);
            if (i % 3 == 0 && w > 2.f) im.set((int)x, (int)y, s.dark);  // a scale mark
        }
        if (!snake) {  // four splayed legs, alternating with the walk
            for (int k = 0; k < 2; k++)
                for (int sd = -1; sd <= 1; sd += 2) {
                    const float t = k == 0 ? 0.78f : 0.42f, y = yAt(t) + ((frame + k + (sd > 0)) & 1 ? -1.f : 1.f);
                    const float x = sx(t) + sd * (bwid * 0.5f + 2.2f);
                    im.line((int)(sx(t) + sd * bwid * 0.35f), (int)yAt(t), (int)x, (int)y, s.dark);
                    im.set((int)x + sd, (int)y, s.dark);
                }
        }
        // head
        const float hy = yAt(1.f), hr = snake ? s.hr * 1.2f : s.hr * (gator ? 1.3f : 1.1f);
        const float hdir = back ? -1.f : 1.f;  // which way the snout points on screen
        im.ellipse(sx(1.f), hy + hdir * 0.8f, hr, hr * (gator ? 1.5f : 1.1f), s.body);
        im.ellipse(sx(1.f), hy + hdir * 0.4f, hr * 0.6f, hr * 0.5f, s.light);
        if (!back) {
            const float ex = hr * 0.55f;
            im.set((int)(sx(1.f) - ex), (int)(hy - 1), dead ? s.dark : s.eye);
            im.set((int)(sx(1.f) + ex), (int)(hy - 1), dead ? s.dark : s.eye);
            if (snake && !dead && frame & 1) im.line((int)sx(1.f), (int)(hy + hr), (int)sx(1.f), (int)(hy + hr + 2), HEX(0xe56b7a));  // a flicking tongue
            if (gator) im.rect((int)(sx(1.f) - 1), (int)(hy + hr * 1.2f), 2, 1, s.dark);  // nostrils
        }
        if (snake && !back && s.tail == 9) im.rect((int)(sx(0.f) - 1), (int)yAt(0.f) - 2, 2, 3, s.dark);  // rattle
        if (snake && back && s.tail == 9) im.rect((int)(sx(0.f) - 1), (int)yAt(0.f) + 1, 2, 3, s.dark);
    }
    (void)headAt;
    im.outline(OUT);
    if (dead) im = im.flippedV();
    return im;
}

inline Img paintAnimal(int kind, const QuadSpec& s, int frame, bool dead) {
    switch (ANIMAL[kind].art) {
        case ART_SNAKE: return drawSnake(s, frame, dead);
        case ART_FROG: return drawFrog(s, frame, dead);
        case ART_LIZARD: return drawLizard(s, frame, dead);
        case ART_GATOR: return drawGator(s, frame, dead);
        default: return Img(1, 1);  // (the species with a full animation set are drawn by the rig: sprites_rig.hpp)
    }
}

// Builds a.animal[kind][form + 2 * tier][frame] for every species, form and tier.
template <class AssetsT>
inline void buildFauna(SDL_Renderer* r, AssetsT& a) {
    for (int k = 0; k < A_COUNT; k++) {
        if (animalClips(k)) continue;  // the species with a full animation set are built by buildRichAnimals
        for (int form = 0; form < ANIMAL[k].forms; form++)
            for (int tier = 0; tier < TR_COUNT; tier++) {
                QuadSpec sp = fauSpec(k, form, tier);
                Spr* out = a.animal[k][form + 2 * tier];
                for (int f = 0; f < 5; f++) {
                    Img im = paintAnimal(k, sp, f < 4 ? f : 0, f == 4);
                    styleActor(im, OUT, 1);
                    out[f] = makeSpr(r, im, (int)sp.cx, f < 4 ? sp.H - 2 : 1, true);
                }
                if (ANIMAL[k].art != ART_QUAD && ANIMAL[k].art != ART_BIRD)  // the crawlers also get a front (down) and a back (up) view
                    for (int back = 0; back < 2; back++)
                        for (int f = 0; f < 5; f++) {
                            Img im = drawCrawlerNS(ANIMAL[k].art, sp, f < 4 ? f : 0, f == 4, back == 1);
                            styleActor(im, OUT, 1);
                            a.animalNS[k][form + 2 * tier][back][f] = makeSpr(r, im, im.w / 2, f < 4 ? im.h - 2 : im.h / 2, true);
                        }
            }
    }
}
