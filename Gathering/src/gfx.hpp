// gfx.hpp - tiny software pixel-art canvas, bitmap font and texture helpers.
#pragma once
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using u32 = uint32_t;

constexpr u32 HEX(u32 rgb) { return 0xFF000000u | rgb; }
inline int cA(u32 c) { return (c >> 24) & 255; }
inline int cR(u32 c) { return (c >> 16) & 255; }
inline int cG(u32 c) { return (c >> 8) & 255; }
inline int cB(u32 c) { return c & 255; }
inline u32 RGBA(int r, int g, int b, int a = 255) {
    return ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}
inline u32 withA(u32 c, int a) { return (c & 0x00FFFFFFu) | ((u32)a << 24); }
inline u32 mixc(u32 a, u32 b, float t) {
    return RGBA((int)(cR(a) + (cR(b) - cR(a)) * t), (int)(cG(a) + (cG(b) - cG(a)) * t),
                (int)(cB(a) + (cB(b) - cB(a)) * t), (int)(cA(a) + (cA(b) - cA(a)) * t));
}
inline u32 shadeC(u32 c, float f) {
    auto cl = [](float v) { return (int)std::min(255.f, std::max(0.f, v)); };
    return RGBA(cl(cR(c) * f), cl(cG(c) * f), cl(cB(c) * f), cA(c));
}

// ---------------------------------------------------------------- RNG
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 1) {}
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float f() { return (next() & 0xFFFFFF) / 16777216.0f; }
    float range(float a, float b) { return a + (b - a) * f(); }
    int irange(int a, int b) { return a + (int)(next() % (uint32_t)(b - a + 1)); }
    bool chance(float p) { return f() < p; }
};

inline uint32_t hash2(int x, int y, uint32_t seed = 0) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// ---------------------------------------------------------------- 3x5 font
struct Glyph {
    char c;
    uint8_t r[5];
};
static const Glyph FONT[] = {
    {' ', {0, 0, 0, 0, 0}}, {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}}, {'C', {3, 4, 4, 4, 3}},
    {'D', {6, 5, 5, 5, 6}}, {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}}, {'G', {3, 4, 5, 5, 3}},
    {'H', {5, 5, 7, 5, 5}}, {'I', {7, 2, 2, 2, 7}}, {'J', {1, 1, 1, 5, 2}}, {'K', {5, 5, 6, 5, 5}},
    {'L', {4, 4, 4, 4, 7}}, {'M', {5, 7, 7, 5, 5}}, {'N', {7, 5, 5, 5, 5}}, {'O', {7, 5, 5, 5, 7}},
    {'P', {6, 5, 6, 4, 4}}, {'Q', {2, 5, 5, 7, 3}}, {'R', {6, 5, 6, 5, 5}}, {'S', {3, 4, 2, 1, 6}},
    {'T', {7, 2, 2, 2, 2}}, {'U', {5, 5, 5, 5, 7}}, {'V', {5, 5, 5, 5, 2}}, {'W', {5, 5, 7, 7, 5}},
    {'X', {5, 5, 2, 5, 5}}, {'Y', {5, 5, 2, 2, 2}}, {'Z', {7, 1, 2, 4, 7}}, {'0', {7, 5, 5, 5, 7}},
    {'1', {2, 6, 2, 2, 7}}, {'2', {6, 1, 2, 4, 7}}, {'3', {6, 1, 2, 1, 6}}, {'4', {5, 5, 7, 1, 1}},
    {'5', {7, 4, 6, 1, 6}}, {'6', {3, 4, 7, 5, 7}}, {'7', {7, 1, 2, 2, 2}}, {'8', {7, 5, 7, 5, 7}},
    {'9', {7, 5, 7, 1, 6}}, {'.', {0, 0, 0, 0, 2}}, {',', {0, 0, 0, 2, 4}}, {':', {0, 2, 0, 2, 0}},
    {'!', {2, 2, 2, 0, 2}}, {'?', {6, 1, 2, 0, 2}}, {'-', {0, 0, 7, 0, 0}}, {'+', {0, 2, 7, 2, 0}},
    {'/', {1, 1, 2, 4, 4}}, {'%', {5, 1, 2, 4, 5}}, {'(', {1, 2, 2, 2, 1}}, {')', {4, 2, 2, 2, 4}},
    {'[', {3, 2, 2, 2, 3}}, {']', {6, 2, 2, 2, 6}}, {'\'', {2, 2, 0, 0, 0}}, {'"', {5, 5, 0, 0, 0}},
    {'>', {4, 2, 1, 2, 4}}, {'<', {1, 2, 4, 2, 1}}, {'=', {0, 7, 0, 7, 0}}, {'*', {0, 5, 2, 5, 0}},
    {'#', {5, 7, 5, 7, 5}}, {'_', {0, 0, 0, 0, 7}}, {'&', {2, 5, 2, 5, 3}}, {'@', {2, 5, 7, 4, 3}},
};
constexpr int FONT_N = sizeof(FONT) / sizeof(FONT[0]);

inline int glyphIndex(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (int i = 0; i < FONT_N; i++)
        if (FONT[i].c == c) return i;
    return 0;
}

// ---------------------------------------------------------------- Img
struct Img {
    int w = 0, h = 0;
    std::vector<u32> p;
    Img() = default;
    Img(int w_, int h_) : w(w_), h(h_), p((size_t)w_ * h_, 0u) {}
    bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    u32 get(int x, int y) const { return in(x, y) ? p[(size_t)y * w + x] : 0u; }
    void set(int x, int y, u32 c) {
        if (in(x, y)) p[(size_t)y * w + x] = c;
    }
    void fill(u32 c) { std::fill(p.begin(), p.end(), c); }
    void rect(int x, int y, int rw, int rh, u32 c) {
        for (int j = 0; j < rh; j++)
            for (int i = 0; i < rw; i++) set(x + i, y + j, c);
    }
    void ellipse(float cx, float cy, float rx, float ry, u32 c) {
        if (rx <= 0 || ry <= 0) return;
        int x0 = (int)floorf(cx - rx - 1), x1 = (int)ceilf(cx + rx + 1);
        int y0 = (int)floorf(cy - ry - 1), y1 = (int)ceilf(cy + ry + 1);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
                if (dx * dx + dy * dy <= 1.0f) set(x, y, c);
            }
    }
    // three-tone ellipse lit from the top-left
    void ellipseShaded(float cx, float cy, float rx, float ry, u32 light, u32 mid, u32 dark) {
        int x0 = (int)floorf(cx - rx - 1), x1 = (int)ceilf(cx + rx + 1);
        int y0 = (int)floorf(cy - ry - 1), y1 = (int)ceilf(cy + ry + 1);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
                if (dx * dx + dy * dy > 1.0f) continue;
                float t = (dx * 0.6f + dy * 0.9f) * 0.5f;
                set(x, y, t < -0.22f ? light : (t > 0.22f ? dark : mid));
            }
    }
    void line(int x0, int y0, int x1, int y1, u32 c) {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            set(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
    void blit(const Img& s, int dx, int dy) {
        for (int y = 0; y < s.h; y++)
            for (int x = 0; x < s.w; x++) {
                u32 c = s.p[(size_t)y * s.w + x];
                if (cA(c)) set(dx + x, dy + y, c);
            }
    }
    Img flippedH() const {
        Img o(w, h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) o.set(w - 1 - x, y, get(x, y));
        return o;
    }
    Img flippedV() const {
        Img o(w, h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) o.set(x, h - 1 - y, get(x, y));
        return o;
    }
    Img silhouette(u32 c) const {
        Img o(w, h);
        for (int i = 0; i < w * h; i++)
            if (cA(p[i])) o.p[i] = c;
        return o;
    }
    // 1px outline around opaque pixels (needs a transparent margin)
    void outline(u32 c) {
        Img o = *this;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                if (cA(get(x, y))) continue;
                if (cA(get(x - 1, y)) || cA(get(x + 1, y)) || cA(get(x, y - 1)) || cA(get(x, y + 1)))
                    o.set(x, y, c);
            }
        *this = o;
    }
    void text(int x, int y, const std::string& s, u32 col) {
        for (char ch : s) {
            const Glyph& g = FONT[glyphIndex(ch)];
            for (int r = 0; r < 5; r++)
                for (int cI = 0; cI < 3; cI++)
                    if ((g.r[r] >> (2 - cI)) & 1) set(x + cI, y + r, col);
            x += 4;
        }
    }
};

// ---------------------------------------------------------------- textures / sprites
inline SDL_Texture* toTex(SDL_Renderer* r, const Img& im) {
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, im.w, im.h);
    SDL_UpdateTexture(t, nullptr, im.p.data(), im.w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

struct Spr {
    SDL_Texture* t = nullptr;
    SDL_Texture* flash = nullptr;  // solid-white silhouette used for hit flashes
    int w = 0, h = 0, ax = 0, ay = 0;
    bool ok() const { return t != nullptr; }
};

inline Spr makeSpr(SDL_Renderer* r, const Img& im, int ax = -1, int ay = -1, bool withFlash = false) {
    Spr s;
    s.t = toTex(r, im);
    s.w = im.w;
    s.h = im.h;
    s.ax = ax < 0 ? im.w / 2 : ax;
    s.ay = ay < 0 ? im.h - 1 : ay;
    if (withFlash) s.flash = toTex(r, im.silhouette(HEX(0xFFFFFF)));
    return s;
}
