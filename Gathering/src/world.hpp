// world.hpp - procedural world: terrain, lakes, forests, the village; plus collision queries.
#pragma once
#include "sprites.hpp"

constexpr int TS = 16, MW = 88, MH = 60;
constexpr int LW = 400, LH = 225;  // logical (pixel-art) resolution

struct V2 {
    float x = 0, y = 0;
    V2() = default;
    V2(float x_, float y_) : x(x_), y(y_) {}
    V2 operator+(V2 o) const { return {x + o.x, y + o.y}; }
    V2 operator-(V2 o) const { return {x - o.x, y - o.y}; }
    V2 operator*(float k) const { return {x * k, y * k}; }
    V2& operator+=(V2 o) {
        x += o.x;
        y += o.y;
        return *this;
    }
    float len() const { return sqrtf(x * x + y * y); }
    V2 norm() const {
        float l = len();
        return l > 1e-5f ? V2(x / l, y / l) : V2(0, 0);
    }
};
inline float dist(V2 a, V2 b) { return (a - b).len(); }

enum Tile : uint8_t { T_GRASS, T_DARK, T_DIRT, T_WATER };
enum PropKind { P_OAK0, P_OAK1, P_PINE, P_ROCK0, P_ROCK1, P_HUT, P_FIRE, P_CRATE, P_BARREL, P_SIGN };
struct Prop {
    int kind;
    float x, y;  // ground anchor
    float ph;
};
struct Decor {
    int kind;  // 0-2 flowers, 3 tuft, 4 mushroom, 5 pebbles, 6-7 bush
    float x, y, ph;
};

inline float vnoise(float x, float y, uint32_t seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    auto h = [&](int a, int b) { return (hash2(a, b, seed) & 0xFFFF) / 65535.f; };
    float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    float a = h(xi, yi) + (h(xi + 1, yi) - h(xi, yi)) * u;
    float b = h(xi, yi + 1) + (h(xi + 1, yi + 1) - h(xi, yi + 1)) * u;
    return a + (b - a) * v;
}
inline float fbm(float x, float y, uint32_t seed) {
    return (vnoise(x, y, seed) * 0.5f + vnoise(x * 2, y * 2, seed + 11) * 0.25f + vnoise(x * 4, y * 4, seed + 23) * 0.125f) / 0.875f;
}

struct World {
    uint8_t tile[MH][MW];
    bool solid[MH][MW];
    std::vector<Prop> props;
    std::vector<Decor> decor;
    V2 village, hut, door, keeper, fire, spawn;
    uint32_t seed = 7;

    float pixW() const { return MW * TS; }
    float pixH() const { return MH * TS; }

    bool water(int tx, int ty) const { return tx >= 0 && ty >= 0 && tx < MW && ty < MH && tile[ty][tx] == T_WATER; }
    bool solidTile(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= MW || ty >= MH) return true;
        return solid[ty][tx] || tile[ty][tx] == T_WATER;
    }
    bool blocked(float x, float y, float r) const {
        int x0 = (int)floorf((x - r) / TS), x1 = (int)floorf((x + r) / TS);
        int y0 = (int)floorf((y - r) / TS), y1 = (int)floorf((y + r) / TS);
        for (int ty = y0; ty <= y1; ty++)
            for (int tx = x0; tx <= x1; tx++)
                if (solidTile(tx, ty)) return true;
        return false;
    }
    void markSolidAt(float x, float y) {
        int tx = (int)(x / TS), ty = (int)(y / TS);
        if (tx >= 0 && ty >= 0 && tx < MW && ty < MH) solid[ty][tx] = true;
    }

    void generate(uint32_t sd) {
        seed = sd;
        props.clear();
        decor.clear();
        memset(solid, 0, sizeof(solid));
        Rng r(seed * 7919u + 13);
        const float vx = 44, vy = 30;
        village = V2(vx * TS + 8, vy * TS + 8);

        struct Lake {
            float cx, cy, rx, ry;
        } lakes[] = {{16, 13, 9, 5.5f}, {70, 46, 11, 6.5f}, {72, 12, 7, 4.5f}, {13, 47, 8, 5.5f}, {34, 53, 5, 3.5f}, {58, 8, 3.5f, 2.5f}};
        for (int ty = 0; ty < MH; ty++)
            for (int tx = 0; tx < MW; tx++) {
                uint8_t t = T_GRASS;
                if (fbm(tx * 0.08f, ty * 0.08f, seed + 1) > 0.6f) t = T_DARK;
                for (auto& l : lakes) {
                    float dx = (tx - l.cx) / l.rx, dy = (ty - l.cy) / l.ry;
                    float d = dx * dx + dy * dy + (fbm(tx * 0.22f, ty * 0.22f, seed + 7) - 0.5f) * 0.8f;
                    if (d < 1.f) t = T_WATER;
                }
                tile[ty][tx] = t;
            }
        // village plaza + dirt paths
        for (int ty = 0; ty < MH; ty++)
            for (int tx = 0; tx < MW; tx++) {
                float d = hypotf(tx - vx, (ty - vy) * 1.15f);
                float th = 6.3f + (vnoise(tx * 0.5f, ty * 0.5f, seed + 3) - 0.5f) * 2.2f;
                if (d < th) tile[ty][tx] = T_DIRT;
            }
        int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0; k < 4; k++)
            for (int i = 5; i < 30; i++) {
                float wig = sinf(i * 0.37f + k * 2.1f) * 1.6f;
                int px = (int)(vx + dirs[k][0] * i + (dirs[k][0] ? 0 : wig));
                int py = (int)(vy + dirs[k][1] * i + (dirs[k][1] ? 0 : wig));
                for (int a = 0; a < 2; a++)
                    for (int b = 0; b < 2; b++) {
                        int x = px + a, y = py + b;
                        if (x > 1 && y > 1 && x < MW - 2 && y < MH - 2 && tile[y][x] != T_WATER) tile[y][x] = T_DIRT;
                    }
            }

        // village props
        int htx = 42, hty = 25;
        hut = V2((htx + 1.5f) * TS, (hty + 2) * TS);
        for (int y = hty; y < hty + 2; y++)
            for (int x = htx; x < htx + 3; x++) solid[y][x] = true;
        props.push_back({P_HUT, hut.x, hut.y, 0});
        door = V2(hut.x, hut.y + 12);
        keeper = V2(hut.x + 40, hut.y + 8);
        fire = V2(hut.x + 76, hut.y + 40);
        spawn = V2(hut.x, hut.y + 48);
        props.push_back({P_FIRE, fire.x, fire.y, 0});
        markSolidAt(fire.x, fire.y - 3);
        props.push_back({P_CRATE, hut.x - 40, hut.y + 4, 0});
        markSolidAt(hut.x - 40, hut.y + 2);
        props.push_back({P_BARREL, hut.x - 56, hut.y + 6, 0});
        markSolidAt(hut.x - 56, hut.y + 4);
        props.push_back({P_SIGN, hut.x - 26, hut.y + 20, 0});
        markSolidAt(hut.x - 26, hut.y + 18);
        props.push_back({P_CRATE, hut.x - 38, hut.y - 10, 0});

        // trees, rocks, decor
        for (int ty = 0; ty < MH; ty++)
            for (int tx = 0; tx < MW; tx++) {
                uint8_t t = tile[ty][tx];
                bool border = tx < 2 || ty < 2 || tx >= MW - 2 || ty >= MH - 2;
                if (border) {
                    solid[ty][tx] = true;
                    props.push_back({r.chance(0.6f) ? P_PINE : P_OAK0, tx * TS + 8.f + r.range(-2, 2), ty * TS + 15.f, r.f() * 6.28f});
                    continue;
                }
                if (t == T_WATER || solid[ty][tx]) continue;
                float dv = hypotf(tx - vx, ty - vy);
                if (t == T_DIRT || dv < 9) {
                    if (t != T_DIRT && r.chance(0.04f))
                        decor.push_back({r.chance(0.5f) ? 3 : r.irange(0, 2), tx * TS + r.range(2, 14), ty * TS + r.range(6, 15), r.f() * 6.28f});
                    continue;
                }
                float ff = fbm(tx * 0.08f, ty * 0.08f, seed + 1);
                float pTree = 0.014f + 0.36f * std::min(1.f, std::max(0.f, (ff - 0.5f) / 0.2f));
                if (r.chance(pTree)) {
                    int kind = (t == T_DARK) ? (r.chance(0.55f) ? P_PINE : P_OAK0) : (r.chance(0.12f) ? P_PINE : (r.chance(0.5f) ? P_OAK0 : P_OAK1));
                    props.push_back({kind, tx * TS + 8.f + r.range(-2, 2), ty * TS + 15.f, r.f() * 6.28f});
                    solid[ty][tx] = true;
                    continue;
                }
                if (r.chance(0.007f)) {
                    props.push_back({r.chance(0.6f) ? P_ROCK0 : P_ROCK1, tx * TS + 8.f, ty * TS + 14.f, 0});
                    solid[ty][tx] = true;
                    continue;
                }
                if (r.chance(0.011f)) {
                    decor.push_back({6 + r.irange(0, 1), tx * TS + 8.f, ty * TS + 15.f, r.f() * 6.28f});
                    continue;
                }
                if (r.chance(0.085f)) {
                    float roll = r.f();
                    int kind = roll < 0.3f ? r.irange(0, 2) : roll < 0.82f ? 3 : roll < 0.9f ? (t == T_DARK ? 4 : 3) : 5;
                    decor.push_back({kind, tx * TS + r.range(2, 14), ty * TS + r.range(6, 15), r.f() * 6.28f});
                }
            }
        std::sort(decor.begin(), decor.end(), [](const Decor& a, const Decor& b) { return a.y < b.y; });
    }

    // 1 pixel per tile map
    Img minimap() const {
        Img im(MW, MH);
        for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) {
                u32 c = HEX(0x5aa63f);
                switch (tile[y][x]) {
                    case T_DARK: c = HEX(0x3e8a30); break;
                    case T_DIRT: c = HEX(0xb08556); break;
                    case T_WATER: c = HEX(0x3c7fc7); break;
                }
                im.set(x, y, c);
            }
        for (auto& p : props) {
            int x = (int)(p.x / TS), y = (int)(p.y / TS);
            if (p.kind == P_OAK0 || p.kind == P_OAK1 || p.kind == P_PINE) im.set(x, y, HEX(0x24602a));
            else if (p.kind == P_ROCK0 || p.kind == P_ROCK1) im.set(x, y, HEX(0x8d929e));
        }
        return im;
    }
};
