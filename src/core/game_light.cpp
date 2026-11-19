// game_light.cpp - light and weather outdoors: the night darkness with pools of light that buildings and rocks cast shadows from, dawn and
// dusk colour, flickering flames, tree and rock shadows that swing with the sun, aurora over the snow, wisps in the swamp, glowing
// crystals and ore, heat shimmer in the desert, and storms with rain and lightning. (The delves keep their own darkness, delve_render.cpp.)
#include <algorithm>
#include <cmath>
#include "core/game.hpp"
#include "core/logic_util.hpp"
#include "core/shadow_casters.hpp"
#include "npcs/town.hpp"

namespace {
inline float flick(float t, float seed) {  // a flame's brightness: slow breathing, quick flutter, and a rare pop
    return 0.88f + 0.08f * sinf(t * 9.f + seed) + 0.04f * sinf(t * 23.f + seed * 1.7f) + 0.03f * sinf(t * 41.f + seed * 0.3f);
}
inline float smooth01(float a, float b, float x) {
    const float t = clampf((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// When the lights are on in a town building (0..1): they come on at dusk and go out at bedtime, which differs from shop to shop
// (the salon keeps its lights on latest); the smith, tanner and flower shop are early risers and light up again before dawn.
float windowsLit(int kind, uint32_t h, float tod, float night) {
    static const float bed[BLD_COUNT] = {0.78f, 0.72f, 0.80f, 0.76f, 0.84f, 0.90f};
    static const float rise[BLD_COUNT] = {0.93f, 0.94f, 2.f, 2.f, 0.95f, 2.f};  // 2 = sleeps in
    const float on = smooth01(0.35f, 0.7f, night);
    const float jitter = ((int)(h % 7) - 3) * 0.008f;
    float lit;
    if (tod >= 0.5f) {
        const float out = smooth01(bed[kind] + jitter, bed[kind] + jitter + 0.02f, tod);
        const float up = smooth01(rise[kind] + jitter - 0.012f, rise[kind] + jitter, tod);
        lit = std::max(1.f - out, up);
    } else {
        lit = rise[kind] < 1.f ? 1.f : 0.f;  // past midnight: the early risers are up
    }
    return on * lit;
}
}  // namespace

// The weather schedule (see WX_CELL in game.hpp): a pure function of the world seed and the world clock.
bool Game::weatherAt(int kind, double t, float* age, float* left) const {
    if (kind <= WX_CLEAR || kind >= WX_COUNT || t < 0.0) return false;
    const WeatherDef& W = WEATHER[kind];
    const long long cell = (long long)floor(t / WX_CELL);
    const int c = (int)(cell & 0x7fffffff);
    const uint32_t h1 = hash2(c, kind * 7919 + 13, worldSeed ^ 0x57A7E5u), h2 = hash2(c, kind * 7919 + 29, worldSeed ^ 0x9E3779B9u);
    if ((h1 & 0xffff) / 65535.f >= W.chance / (kind == WX_STORM ? seasonStorm() : 1.f)) return false;                                      // a quiet cell
    const float dur = W.durLo + ((h1 >> 16) & 0xff) / 255.f * (W.durHi - W.durLo);              // how long
    const float first = cell == 0 ? 100.f : 0.f;                                                // a new game starts with 100 s of calm
    const float start = first + (h2 & 0xffff) / 65535.f * std::max(0.f, WX_CELL - dur - first);  // when in the cell it begins
    const float local = (float)(t - (double)cell * WX_CELL);
    if (local < start || local >= start + dur) return false;
    if (age) *age = local - start;
    if (left) *left = start + dur - local;
    return true;
}

// ================================================================== update: weather and the lamp list
void Game::updateLight(float dt) {
    lampList.clear();
    const bool outside = interior < 0 && delve < 0 && state != ST_TITLE;
    // ---- weather state machine: what can blow here (rain in the green parts and the mountains, sand in the desert, snow in the snowfields)
    const float rainW = bw[B_HOME] + bw[B_HOMESTEAD] + bw[B_SWAMP] + bw[B_MOUNTAIN] * 0.8f;
    int allowed = WX_CLEAR;
    if (outside) {
        if (rainW >= 0.5f) allowed = WX_STORM;
        else if (bw[B_DESERT] >= 0.5f) allowed = WX_SANDSTORM;
        else if (bw[B_SNOW] >= 0.5f) allowed = WX_BLIZZARD;
    }
    // the schedule says whether that kind is blowing right now; leaving (or entering a house) just means you are not under it
    if (allowed != WX_CLEAR && weatherAt(allowed, dayT, &wxAge, &wxT)) wxKind = allowed;
    else wxKind = WX_CLEAR, wxT = wxAge = 0.f;
    // strengths ease towards their targets: a storm opens as a drizzle (the lead), then breaks
    float rainTarget = 0.f, galeTarget = 0.f;
    if (wxKind == WX_STORM) {
        const WeatherDef& W = WEATHER[WX_STORM];
        rainTarget = (wxAge < W.lead) ? 0.35f : 1.f;
    } else if (wxKind == WX_SANDSTORM || wxKind == WX_BLIZZARD)
        galeTarget = 1.f;
    stormK += clampf(rainTarget - stormK, -0.35f * dt, 0.2f * dt);
    galeK += clampf(galeTarget - galeK, -0.4f * dt, 0.25f * dt);
    // ---- lightning
    if (stormK > 0.6f) {
        flashNext -= dt;
        if (flashNext <= 0) {
            flashT = 0.45f;
            flashNext = wrng.range(3.5f, 11.f);
            if (wrng.chance(0.35f)) flashNext = 0.25f;  // a second flash right after the first
            thunderT = wrng.range(0.4f, 2.2f);          // the thunder comes later: the bolt is a way off
        }
    }
    flashT = std::max(0.f, flashT - dt);
    if (thunderT >= 0) {
        thunderT -= dt;
        if (thunderT < 0) {
            audio.play(S_FELL, 0.35f, 0.2f);
            thunderT = -1.f;
        }
    }
    // ---- the fixed lights of this map (the player's own light is added when drawing)
    if (!outside) return;
    if (world.mapId == B_HOME) {
        lampList.push_back({world.fire.x, world.fire.y - 12.f, 84.f, 1.f, 0, true, false});
        for (int wx : {-19, 13}) lampList.push_back({world.hut.x + wx, world.hut.y - 20.f, 30.f, 0.9f, 0, true, true});
    }
    winList.clear();
    if (world.mapId == B_HOME) {  // the town: every house has a window each side of its door, lit by routine
        const float night = 1.f - daylight(), tt = tod();
        if (night > 0.3f || tt < 0.1f)
            for (const Prop& pr : world.props) {
                if (pr.kind < P_BLD0 || pr.kind > P_BLD5) continue;
                const int bk = pr.kind - P_BLD0;
                const float lit = windowsLit(bk, hash2((int)(pr.x / TS), (int)(pr.y / TS), 4242), tt, night);
                if (lit < 0.02f) continue;
                for (int side : {-32, 32}) {  // the glass is 12 x 10 px, 32 px left and right of the door, 29 px above the foot
                    winList.push_back({pr.x + side, pr.y - 29.f, 0.f, lit, bk, false, false});
                    lampList.push_back({pr.x + side, pr.y - 27.f, bk == BLD_SALON ? 44.f : 38.f, 0.85f * lit, 0, false, false, true});
                }
            }
    }
    for (auto& lt : world.lanterns) lampList.push_back({lt.x, lt.y, 34.f + 22.f * clampf(lt.r, 0.f, 1.5f), 0.95f, 0, true, true});
    for (auto& st : structs)
        if (st.kind == PL_CAMPFIRE) lampList.push_back({st.pos().x, st.pos().y - 8.f, 62.f * (1.f - 0.25f * rainK()), 1.f - 0.3f * rainK(), 0, true, false});
    if (inHomestead())
        for (auto& pc : hs.pieces) {
            const HPieceDef& D = HPDEF[pc.kind];
            if (D.light <= 0) continue;
            const float cx = pc.tx * (float)TS + pc.w() * 8.f, cy = pc.ty * (float)TS + pc.h() * 8.f - (pc.kind == HP_LAMP ? 14.f : 2.f);
            lampList.push_back({cx, cy, D.light * 1.05f, 0.95f, 0, true, pc.kind == HP_HOUSE || pc.kind == HP_LAMP});
        }
    if (world.hportal.x > -9999.f) lampList.push_back({world.hportal.x, world.hportal.y - 26.f, 56.f, 0.8f, 1, false, false});
}

// how strongly the fixed lights (not the hunter) light a spot: monsters slow down in firelight
float Game::lampLight(V2 pos) const {
    if (interior >= 0 || delve >= 0) return 0.f;
    const float night = 1.f - daylight();
    float lit = 0.f;
    for (const LightSrc& l : lampList) {
        if (l.night && night < 0.3f) continue;
        const float d = dist(pos, V2(l.x, l.y));
        if (d < l.r * 0.8f) lit += (1.f - d / (l.r * 0.8f)) * l.k;
    }
    return std::min(1.f, lit);
}

// ================================================================== sun, moon and cloud shadows
// Everything that stands on the ground casts the shape of its own sprite: the picture is stood on the ground and leant over along the sun's
// direction (like Unity's Shadow Caster 2D, but for the sun), drawn dark and soft, fading towards its far end. It uses the sprite's current
// frame, so a swaying tree sways its shadow and a walking hunter walks his. The sun crosses the sky from the right (east) to the left
// (west) of the map, so shadows swing from left to right through the day: long near dawn and dusk, short at noon. At night the moon takes
// over (opposite the sun, much fainter and bluer), and clouds drift their own soft shadows over the land by day.
//
// How a frame is built (all plain SDL2 render targets and blend modes):
//   1. shadowTex (full size): every caster is drawn into it with a MAXIMUM blend where the renderer has one, so two shadows that cross
//      make one shadow, never a darker patch. The layer only records where shadow lies (its alpha).
//   2. That is shrunk to half size (shadowSoftTex) and read back as the shadow mask: ground cover and creatures are darkened where
//      standing things (not creatures) cast a shadow on them. The mask is read every other frame; it is kept in world coordinates.
//   3. Creatures, then, are cast into the same layer. They are left out of the mask so they never darken themselves.
//   4. The layer is turned white (an ADD of white keeps its alpha), shrunk again, and drawn into shadowModTex over white with the shadow
//      colour as its colour mod: a picture that is white where it is lit and the shadow colour where it is shaded.
//   5. That picture is MULTIPLIED over the ground (SDL_BLENDMODE_MOD), so shadowed grass stays grass with its texture, only cooler and
//      darker, rather than being painted over with black. The shadow colour follows the biome and the hour.
namespace {
// a tiling cloud picture: periodic value noise, 128 x 128 texels, white with the cloud in its alpha
SDL_Texture* makeCloudTex(SDL_Renderer* ren) {
    constexpr int N = 128;
    std::vector<uint32_t> px((size_t)N * N);
    auto lat = [](int ix, int iy, int cells, uint32_t seed) {
        const int x = ((ix % cells) + cells) % cells, y = ((iy % cells) + cells) % cells;
        return (float)(hash2(x, y, seed) & 1023u) / 1023.f;
    };
    auto vnoise = [&](float u, float v, int cells, uint32_t seed) {
        const float fx = u * cells, fy = v * cells;
        const int ix = (int)floorf(fx), iy = (int)floorf(fy);
        float tx = fx - ix, ty = fy - iy;
        tx = tx * tx * (3.f - 2.f * tx);
        ty = ty * ty * (3.f - 2.f * ty);
        const float a = lat(ix, iy, cells, seed), b = lat(ix + 1, iy, cells, seed), c = lat(ix, iy + 1, cells, seed), d = lat(ix + 1, iy + 1, cells, seed);
        const float top = a + (b - a) * tx, bot = c + (d - c) * tx;
        return top + (bot - top) * ty;
    };
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            const float u = (x + 0.5f) / N, v = (y + 0.5f) / N;
            const float n = 0.50f * vnoise(u, v, 4, 11) + 0.30f * vnoise(u, v, 8, 23) + 0.20f * vnoise(u, v, 16, 37);
            px[(size_t)y * N + x] = ((uint32_t)(smooth01(0.47f, 0.72f, n) * 255.f) << 24) | 0x00FFFFFFu;
        }
    const char* old = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Texture* t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, N, N);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, old ? old : "0");
    if (t) {
        SDL_UpdateTexture(t, nullptr, px.data(), N * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    return t;
}

// puts a sprite's texture into the blend mode the shadow layer wants for as long as it is in scope, then puts back what it had
struct TexBlendGuard {
    SDL_Texture* t;
    SDL_BlendMode old = SDL_BLENDMODE_BLEND;
    bool on;
    TexBlendGuard(SDL_Texture* tex, bool enable, SDL_BlendMode m) : t(tex), on(enable) {
        if (on) {
            SDL_GetTextureBlendMode(t, &old);
            SDL_SetTextureBlendMode(t, m);
        }
    }
    ~TexBlendGuard() {
        if (on) SDL_SetTextureBlendMode(t, old);
    }
};
}  // namespace

void Game::drawGroundShadows() {
    const bool prevMask = shadowMaskOn;
    shadowMaskOn = false;  // set again below once this frame's shadow layer exists
    if (interior >= 0 || delve >= 0) return;
    const float t = tod();
    const float sunRaw = sinf(t * 6.2832f);
    const bool moonLit = sunRaw < 0.02f;  // night: the moon is half a day behind the sun, so it stands where the sun stood twelve hours ago
    const float night = 1.f - daylight();
    const float sun = moonLit ? -sunRaw : sunRaw;                  // height of whichever light is up, 0..1
    const float lightT = moonLit ? fmodf(t + 0.5f, 1.f) : t;       // its own time of day: that is what swings the shadows
    const float golden = moonLit ? 0.f : smooth01(0.5f, 0.2f, sun);  // golden hour: the shadows stretch and darken
    float alpha;
    if (!moonLit) {
        const float alphaBase = clampf(daylight() * (0.18f + 0.30f * std::min(1.f, sun * 1.6f)) * (1.f - 0.6f * stormK), 0.f, 0.5f);
        // how dark the finished shadow layer is; it fades in with the sunrise and out with the sunset instead of popping on and off
        alpha = std::min(0.95f, std::min(0.55f, alphaBase * (1.f + 0.3f * golden)) * 0.9f * smooth01(0.02f, 0.16f, sun) * shadowBoost);
    } else {
        // moonlight: faint, and only once it is properly dark (the dusk shadows of the sun have just faded out)
        alpha = std::min(0.5f, 0.34f * smooth01(0.3f, 0.85f, night) * smooth01(0.02f, 0.2f, sun) * (1.f - 0.7f * stormK) * shadowBoost);
    }
    if (alpha < 0.02f) return;

    // ---- the shadow colour: what a full shadow multiplies the ground by. Cool in the green lands, violet on sand, blue on snow, murky in the swamp
    static const float TINT[B_COUNT][3] = {
        {0.13f, 0.15f, 0.36f},  // home
        {0.30f, 0.17f, 0.36f},  // desert
        {0.09f, 0.20f, 0.24f},  // swamp
        {0.20f, 0.33f, 0.62f},  // snow
        {0.17f, 0.19f, 0.36f},  // mountain
        {0.13f, 0.15f, 0.36f},  // homestead
    };
    {
        float ws = 0.f, c[3] = {0.f, 0.f, 0.f};
        for (int b = 0; b < B_COUNT; b++) {
            const float w = std::max(0.f, bw[b]);
            ws += w;
            for (int k = 0; k < 3; k++) c[k] += w * TINT[b][k];
        }
        for (int k = 0; k < 3; k++) c[k] = ws > 0.01f ? c[k] / ws : TINT[B_HOME][k];
        const float violet[3] = {0.30f, 0.17f, 0.38f}, grey[3] = {0.30f, 0.32f, 0.42f}, moonC[3] = {0.10f, 0.16f, 0.40f};
        for (int k = 0; k < 3; k++) {
            c[k] += (violet[k] - c[k]) * 0.35f * golden;  // low sun: richer, more violet shadows
            c[k] += (grey[k] - c[k]) * 0.4f * stormK;     // storm light: flat and grey
            c[k] += (moonC[k] - c[k]) * (moonLit ? 0.85f : 0.f);
            shadowTint[k] = clampf(c[k], 0.f, 1.f);
        }
    }

    // ---- the layers
    SDL_Texture* prevTarget = SDL_GetRenderTarget(ren);
    if (!shadowTex) {
        const char* old = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");  // linear: the layer is shrunk below
        shadowTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, LW, LH);
        shadowSoftTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, LW / 2, LH / 2);
        shadowModTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, LW / 2, LH / 2);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, old ? old : "0");
        if (shadowTex) SDL_SetTextureBlendMode(shadowTex, SDL_BLENDMODE_BLEND);
        if (shadowSoftTex) SDL_SetTextureBlendMode(shadowSoftTex, SDL_BLENDMODE_BLEND);
        if (shadowModTex) SDL_SetTextureBlendMode(shadowModTex, SDL_BLENDMODE_MOD);
        cloudTex = makeCloudTex(ren);
    }
    if (!shadowTex || !shadowSoftTex || !shadowModTex) return;

    // Two shadows that cross must make one shadow: the layer is drawn with a MAXIMUM blend wherever the renderer can (OpenGL, Direct3D and
    // Metal can; the software renderer cannot, and then the ordinary blend is used, which only darkens the far tips where shadows cross).
    static int maxBlendOk = -1;
    static SDL_BlendMode maxBm = SDL_BLENDMODE_BLEND;
    if (maxBlendOk < 0) {
        const SDL_BlendMode m = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MAXIMUM, SDL_BLENDFACTOR_ONE,
                                                         SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MAXIMUM);
        SDL_BlendMode oldDraw = SDL_BLENDMODE_NONE;
        SDL_GetRenderDrawBlendMode(ren, &oldDraw);
        maxBlendOk = SDL_SetRenderDrawBlendMode(ren, m) == 0 && SDL_SetTextureBlendMode(shadowTex, m) == 0 ? 1 : 0;
        SDL_SetRenderDrawBlendMode(ren, oldDraw);
        SDL_SetTextureBlendMode(shadowTex, SDL_BLENDMODE_BLEND);
        if (maxBlendOk) maxBm = m;
    }
    const bool useMax = maxBlendOk == 1;
    const SDL_BlendMode castBm = useMax ? maxBm : SDL_BLENDMODE_BLEND;

    SDL_SetRenderTarget(ren, shadowTex);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
    SDL_RenderClear(ren);
    // ground direction of the shadow (south = down the screen) and its sideways direction; the view is slanted, so the ground is squashed
    const float ang = 3.14159f - lightT * 6.2832f;
    const float dx = cosf(ang), dy = sinf(ang < 0 ? -ang : ang);
    const float px = dy, py = -dx;
    const float K = clampf(0.42f / std::max(sun, 0.14f), 0.55f, 2.4f) * (1.f + 0.15f * golden) * 0.7f;  // shadow length per px of height
    constexpr float SQ = 0.65f;
    const SDL_Color nearC{10, 8, 30, 255}, farC{10, 8, 30, 115};  // full strength: the layer is faded as a whole below
    static const int idx[6] = {0, 1, 2, 1, 3, 2};

    // a fan of triangles from v[0] (the centre) round v[1..nv-1] (the rim), untextured
    auto fanDraw = [&](SDL_Vertex* v, int nv) {
        static int fan[64 * 3];
        const int rim = std::min(nv - 1, 63);
        for (int i = 0; i < rim; i++) fan[i * 3] = 0, fan[i * 3 + 1] = 1 + i, fan[i * 3 + 2] = 1 + (i + 1) % rim;
        SDL_BlendMode oldBm = SDL_BLENDMODE_NONE;  // untextured geometry uses the renderer's draw blend mode: set it, then put it back
        SDL_GetRenderDrawBlendMode(ren, &oldBm);
        SDL_SetRenderDrawBlendMode(ren, castBm);
        SDL_RenderGeometry(ren, nullptr, v, rim + 1, fan, rim * 3);
        SDL_SetRenderDrawBlendMode(ren, oldBm);
    };
    // a soft dark oval on the ground: the contact shadow under a foot, a wheel, a paw
    auto blob = [&](float cx, float cy, float rx, float ry, float a) {
        constexpr int N = 14;
        SDL_Vertex v[N + 1];
        v[0].position = SDL_FPoint{cx, cy};
        v[0].color = SDL_Color{nearC.r, nearC.g, nearC.b, (Uint8)(clampf(225.f * a, 0.f, 255.f))};
        v[0].tex_coord = SDL_FPoint{0.f, 0.f};
        for (int i = 0; i < N; i++) {
            const float a2 = 6.2832f * i / N;
            v[1 + i].position = SDL_FPoint{cx + cosf(a2) * rx, cy + sinf(a2) * ry};
            v[1 + i].color = SDL_Color{nearC.r, nearC.g, nearC.b, (Uint8)(clampf(70.f * a, 0.f, 255.f))};
            v[1 + i].tex_coord = SDL_FPoint{0.f, 0.f};
        }
        fanDraw(v, N + 1);
    };

    // draws one sprite's shadow, its foot at screen (fx, fy)
    // pinned: keep the shadow's base on the object's base line (wide things: a rock, a big oak, a building).
    // crx/cry (+ cdx/cdy from the foot): marks an object that stands on a small footprint (a tree, a post, a barrel); `foot` says how the root
    // of its shadow is drawn (shadow_casters.hpp, FootKind):
    //   FOOT_BASE (default) the object's own base: its lowest few pixel rows, flattened onto the ground. A big oak's roots are cast from
    //                       their own bottom edge and swept along the sun up to the canopy's shadow, so the shadow is one piece.
    //   FOOT_NONE           nothing: the sprite-shaped shadow just starts at the foot.
    //   FOOT_OVAL           the old look: an oval footprint (crx/cry, cdx/cdy, or a rectangle with rectFoot) swept along the sun.
    // cols: cast column by column from the sprite's own bottom edge (rocks, buildings), see below.
    auto cast = [&](const Spr& s, float fx, float fy, bool flip, float aMul, float lenMul, bool pinned = false, float crx = 0.f,
                    float cry = 0.f, float cdx = 0.f, float cdy = 0.f, bool rectFoot = false, bool cols = false,
                    int foot = shadowdef::FOOT_BASE) {
        if (!s.ok()) return;
        const float L = K * lenMul;
        const float sc = s.sc;  // sprites that are drawn smaller than their picture (the big animal pictures)
        const float reach = (s.h * L + s.w) * sc + 8.f;
        if (fx < -reach || fx > LW + reach || fy < -reach || fy > LH + reach) return;
        const TexBlendGuard guard(s.t, useMax, castBm);
        // corners of the sprite: left/right by top/bottom, as (sideways u from the foot, height v above it)
        const float us[4] = {-(float)s.ax * sc, (float)(s.w - s.ax) * sc, -(float)s.ax * sc, (float)(s.w - s.ax) * sc};
        const float vs[4] = {(float)s.ay * sc, (float)s.ay * sc, (float)(s.ay - s.h) * sc, (float)(s.ay - s.h) * sc};
        // A slim thing (a tree) fans out with the sun: its width is laid across the shadow's direction. A wide thing (a building, a boulder)
        // would have its base line turned away from the object that way, and the shadow would come loose at both ends of the base. So
        // the wider the sprite, the more its width stays on the screen's horizontal, which keeps the shadow's base on the object's base.
        // Then the sun may run along the wall (dawn, dusk) and the shadow would fold flat, so the height also leans a little downwards.
        const float pin = pinned ? 1.f : clampf(((float)s.w * sc - 24.f) / 40.f, 0.f, 1.f);
        const float ux = px + (1.f - px) * pin, uy = py * (1.f - pin);
        const float vy = dy + pin * 0.3f * fabsf(dx);
        if (cols && s.bot) {  // column by column: every part of the sprite casts from its own lowest pixel
            // A rock or a building rests on the ground along its whole bottom edge. Along each run of columns, the shadow is one
            // continuous mesh: at every column boundary there is a foot (where the sprite itself ends there, the lower of the two
            // neighbours) and a top leant over along the sun from that foot. Neighbouring columns share these points, so where the
            // base steps or curves the mesh follows it without a gap, and the shadow starts on the object's real outline everywhere.
            static std::vector<SDL_Vertex> cv;
            static std::vector<int> ci;
            cv.clear();
            ci.clear();
            const std::vector<short>& bt = *s.bot;
            const int W = std::min(s.w, (int)bt.size());
            for (int x0 = 0; x0 < W;) {
                if (bt[x0] <= 0) { x0++; continue; }
                int x1 = x0 + 1;
                while (x1 < W && bt[x1] > 0) x1++;  // columns x0 .. x1-1 have something in them
                const int base = (int)cv.size();
                for (int x = x0; x <= x1; x++) {
                    const int bb = std::max(x > x0 ? (int)bt[x - 1] : 0, x < x1 ? (int)bt[x] : 0);  // the edge under the lower neighbour
                    float u = (float)(x - s.ax) * sc;
                    if (flip) u = -u;
                    const float tu = (float)x / (float)s.w;
                    const float cy0 = fy + (float)(bb - s.ay) * sc;  // screen row of the foot: where the sprite is drawn at that edge
                    for (int k = 0; k < 2; k++) {  // k = 0 the top of the sprite leant over the ground, k = 1 the foot
                        const float hgt = k == 0 ? (float)bb * sc : 0.f;
                        SDL_Vertex q;
                        q.position = SDL_FPoint{fx + u + dx * hgt * L, cy0 + vy * hgt * L * SQ};
                        q.color = k == 0 ? farC : nearC;
                        if (aMul != 1.f) q.color.a = (Uint8)(q.color.a * aMul);
                        q.tex_coord = SDL_FPoint{flip ? 1.f - tu : tu, k == 0 ? 0.f : (float)bb / (float)s.h};
                        cv.push_back(q);
                    }
                }
                for (int x = 0; x < x1 - x0; x++) {  // a quad between boundary x and x+1: top, top, foot, foot
                    const int t0 = base + 2 * x, t1 = base + 2 * x + 2, f0 = t0 + 1, f1 = t1 + 1;
                    const int quad[6] = {t0, t1, f0, t1, f1, f0};
                    for (int k = 0; k < 6; k++) ci.push_back(quad[k]);
                }
                x0 = x1;
            }
            if (!cv.empty()) SDL_RenderGeometry(ren, s.t, cv.data(), (int)cv.size(), ci.data(), (int)ci.size());
            return;
        }
        SDL_Vertex v[4];
        for (int i = 0; i < 4; i++) {
            const float u = flip ? -us[i] : us[i];
            const float gx = dx * vs[i] * L + ux * u, gy = vy * vs[i] * L + uy * u;
            v[i].position = SDL_FPoint{fx + gx, fy + gy * SQ};
            const bool far = i < 2;
            v[i].color = far ? farC : nearC;
            if (aMul != 1.f) v[i].color.a = (Uint8)(v[i].color.a * aMul);
            const float tu = (i & 1) ? 1.f : 0.f;
            v[i].tex_coord = SDL_FPoint{flip ? 1.f - tu : tu, far ? 0.f : 1.f};
        }
        SDL_RenderGeometry(ren, s.t, v, 4, idx, 6);
        if (crx > 0.f && cry > 0.f && foot == shadowdef::FOOT_BASE && s.bot) {  // a big oak: the roots' own bottom edge, swept along the sun
            // Only the columns that reach the ground (the roots and the trunk) cast from their bottom edge, not the canopy's overhang. That
            // band starts 3 px up, under the roots, and 2 px wider at both ends, so the soft edge of the shadow layer leaves no light seam
            // at the roots. It is swept up to the lowest edge of the leaves: there the canopy's own shadow begins, so nothing is left
            // between them (only the thin trunk would cast there).
            static std::vector<SDL_Vertex> rv;
            static std::vector<int> ri;
            rv.clear();
            ri.clear();
            const std::vector<short>& bt0 = *s.bot;
            const int W = std::min(s.w, (int)bt0.size());
            const int rootMin = s.h - 8;
            int canopyBottom = 0;
            for (int x = 0; x < W; x++)
                if (bt0[x] > 0 && bt0[x] < rootMin) canopyBottom = std::max(canopyBottom, (int)bt0[x]);
            const float lowS = canopyBottom > 0 ? clampf((float)(s.ay - canopyBottom + 3), 6.f, 0.7f * (float)s.ay) * sc : 0.45f * (float)s.ay * sc;
            const float swx = dx * lowS * L, swy = vy * lowS * L * SQ;
            auto bt = [&](int x) { return bt0[x] >= rootMin ? (int)bt0[x] : 0; };
            for (int x0 = 0; x0 < W;) {
                if (bt(x0) <= 0) { x0++; continue; }
                int x1 = x0 + 1;
                while (x1 < W && bt(x1) > 0) x1++;
                const int base = (int)rv.size();
                for (int x = x0; x <= x1; x++) {
                    const int bb = std::max(x > x0 ? bt(x - 1) : 0, x < x1 ? bt(x) : 0);
                    float u = (float)(x - s.ax) * sc;
                    if (flip) u = -u;
                    const float ext = (x == x0 ? -2.f : x == x1 ? 2.f : 0.f) * sc;
                    u += flip ? -ext : ext;
                    const float cy0 = fy + (float)(bb - s.ay) * sc - 3.f;
                    for (int k = 0; k < 2; k++) {  // k = 0 the foot, k = 1 the same point swept along the sun
                        SDL_Vertex q;
                        q.position = SDL_FPoint{fx + u + (k ? swx : 0.f), cy0 + (k ? swy : 0.f)};
                        q.color = SDL_Color{nearC.r, nearC.g, nearC.b, (Uint8)((k ? 150 : 215) * aMul)};
                        q.tex_coord = SDL_FPoint{0.f, 0.f};
                        rv.push_back(q);
                    }
                }
                for (int x = 0; x < x1 - x0; x++) {
                    const int f0 = base + 2 * x, f1 = f0 + 2, t0 = f0 + 1, t1 = f1 + 1;
                    const int quad[6] = {f0, f1, t0, f1, t1, t0};
                    for (int k = 0; k < 6; k++) ri.push_back(quad[k]);
                }
                x0 = x1;
            }
            if (!rv.empty()) {
                SDL_BlendMode oldBm = SDL_BLENDMODE_NONE;
                SDL_GetRenderDrawBlendMode(ren, &oldBm);
                SDL_SetRenderDrawBlendMode(ren, castBm);
                SDL_RenderGeometry(ren, nullptr, rv.data(), (int)rv.size(), ri.data(), (int)ri.size());
                SDL_SetRenderDrawBlendMode(ren, oldBm);
            }
        } else if (crx > 0.f && cry > 0.f && foot == shadowdef::FOOT_BASE) {  // any other object: its lowest pixel rows, flattened onto the ground
            const float r = std::min(6.f, std::max(3.f, s.h * 0.08f));
            const float rows[4] = {(float)s.h - r, (float)s.h - r, (float)s.h, (float)s.h};
            SDL_Vertex b[4];
            for (int i = 0; i < 4; i++) {
                const float u = flip ? -us[i] : us[i];
                b[i].position = SDL_FPoint{fx + u + dx * 1.5f, fy + (rows[i] - (float)s.ay) * sc * 0.6f + 0.5f};
                b[i].color = SDL_Color{nearC.r, nearC.g, nearC.b, (Uint8)(215 * aMul)};
                const float tu = (i & 1) ? 1.f : 0.f;
                b[i].tex_coord = SDL_FPoint{flip ? 1.f - tu : tu, rows[i] / (float)s.h};
            }
            SDL_RenderGeometry(ren, s.t, b, 4, idx, 6);
        } else if (crx > 0.f && cry > 0.f && foot == shadowdef::FOOT_OVAL) {  // the old oval footprint: the footprint swept along the shadow
            // The shadow of the lowest part of the body (about the lower 45% of the height) is the footprint moved on along the sun. The root
            // is the hull of the two shapes: it starts exactly on the footprint, follows the sun's direction and length, and the
            // sprite-shaped shadow above carries on from it. No gap can open at either end of the base.
            constexpr int SEG = 20, MAXP = 2 * SEG + 8;
            struct P2 { float x, y; };
            const float ecx = fx + (flip ? -cdx : cdx), ecy = fy + cdy;
            const float low = 0.45f * (float)s.ay * sc;
            const float swx = dx * low * L, swy = vy * low * L * SQ;
            P2 pts[MAXP], hull[MAXP + 1];
            int n = 0;
            auto add = [&](float x, float y) {
                pts[n++] = P2{ecx + x, ecy + y};
                pts[n++] = P2{ecx + x + swx, ecy + y + swy};
            };
            if (rectFoot) {
                add(-crx, -cry), add(crx, -cry), add(crx, cry), add(-crx, cry);
            } else {
                for (int i = 0; i < SEG; i++) add(cosf(6.2832f * i / SEG) * crx, sinf(6.2832f * i / SEG) * cry);
            }
            std::sort(pts, pts + n, [](const P2& p, const P2& q) { return p.x < q.x || (p.x == q.x && p.y < q.y); });
            auto cross = [](const P2& o, const P2& p, const P2& q) { return (p.x - o.x) * (q.y - o.y) - (p.y - o.y) * (q.x - o.x); };
            int h = 0;  // convex hull of both shapes (monotone chain)
            for (int i = 0; i < n; i++) {
                while (h >= 2 && cross(hull[h - 2], hull[h - 1], pts[i]) <= 0.f) h--;
                hull[h++] = pts[i];
            }
            for (int i = n - 2, t = h + 1; i >= 0; i--) {
                while (h >= t && cross(hull[h - 2], hull[h - 1], pts[i]) <= 0.f) h--;
                hull[h++] = pts[i];
            }
            h--;  // the last point is the first again
            if (h >= 3) {
                SDL_Vertex e[MAXP + 2];
                float mx = 0.f, my = 0.f;
                for (int i = 0; i < h; i++) mx += hull[i].x, my += hull[i].y;
                e[0].position = SDL_FPoint{mx / h, my / h};
                e[0].color = SDL_Color{nearC.r, nearC.g, nearC.b, (Uint8)(215 * aMul)};
                e[0].tex_coord = SDL_FPoint{0.f, 0.f};
                for (int i = 0; i < h; i++) {
                    e[1 + i].position = SDL_FPoint{hull[i].x, hull[i].y};
                    e[1 + i].color = SDL_Color{nearC.r, nearC.g, nearC.b, (Uint8)(150 * aMul)};
                    e[1 + i].tex_coord = SDL_FPoint{0.f, 0.f};
                }
                fanDraw(e, h + 1);
            }
        }
    };

    // ordinary things that stand on a small footprint (a post, a cactus, a barrel, a gravestone): a slim shadow that starts on the object's base
    auto standing = [&](const Spr& s, float fx, float fy, float aMul = 0.9f, float lenMul = 1.f, int foot = shadowdef::FOOT_BASE) {
        const float crx = std::max(2.f, s.w * s.sc * 0.16f);
        cast(s, fx, fy, false, aMul, lenMul, false, crx, std::max(1.f, crx * 0.45f), 0.f, -1.f, false, false, foot);
    };
    // wide things with a flat base (a hut, a wall, an arch): the base stays on the object's own base line
    auto wide = [&](const Spr& s, float fx, float fy, float aMul = 0.8f, float lenMul = 0.7f) { cast(s, fx, fy, false, aMul, lenMul, true); };

    // ---- the things that stand in the world: trees, rocks and buildings first
    {
        const float yLo = camY - 170.f, yHi = camY + LH + 40.f;  // shadows only ever fall down the screen, so nothing below the view can reach it
        auto first = std::lower_bound(world.props.begin(), world.props.end(), yLo, [](const Prop& a, float y) { return a.y < y; });
        for (auto it = first; it != world.props.end() && it->y <= yHi; ++it) {
            const size_t i = (size_t)(it - world.props.begin());
            const Prop& pr = *it;
            const float fx = pr.x - camX, fy = pr.y + 1.f - camY;
            if (fx < -200 || fx > LW + 200) continue;
            auto frame = [&](float fps, int n) { return ((int)(timeAcc * fps + pr.ph * n / 6.2832f) % n + n) % n; };
            const int rs = propRsId[i];
            if (rs >= 0 && (propSt[i].gone || propSt[i].fade > 0)) continue;  // felled or broken: no shadow
            const shadowdef::PropShadow* d = shadowdef::propShadow(pr.kind);  // the table: shadow_casters.hpp
            if (!d) continue;
            const shadowdef::PropCtx pc{A, pr, windT};  // the trees sway by the wind clock, and so do their shadows
            if (d->shape == shadowdef::SH_OAK) {
                const int ov = oakVariantFor((int)(pr.x / TS), (int)(pr.y / TS));
                const OakVar& V = OAK_VAR[ov];  // the big oaks stand on wide roots: the shadow joins them along the whole footprint
                cast(A.oak[ov][pc.frame(OAK_FPS, OAK_FRAMES)], fx, fy, false, 1.f, 1.f, true, V.sw * 0.5f, V.sh * 0.5f, (float)V.sdx, (float)V.sdy, false, false, d->foot);
                continue;
            }
            const Spr& sp = d->spr(pc);
            const float py = fy + d->yOff;
            switch (d->shape) {
                case shadowdef::SH_STANDING: standing(sp, fx, py, d->aMul, d->lenMul, d->foot); break;
                case shadowdef::SH_WIDE: wide(sp, fx, py, d->aMul, d->lenMul); break;
                case shadowdef::SH_COLS: cast(sp, fx, py, false, d->aMul, d->lenMul, true, 0.f, 0.f, 0.f, 0.f, false, true); break;
                case shadowdef::SH_TRUNK:
                    cast(sp, fx, py, d->flip && d->flip(pr), d->aMul, d->lenMul, false, std::max(d->minW, sp.w * d->fw), std::max(d->minH, sp.w * d->fh), 0.f, -1.f, false, false, d->foot);
                    break;
                default: break;
            }
        }
    }
    // placed structures (chests...)
    for (const Structure& st : structs) {
        const shadowdef::StructShadow* d = shadowdef::structShadow(st.kind);
        if (!d) continue;
        const V2 c = st.pos();
        standing(d->spr(shadowdef::StructCtx{A}), floorf(c.x) - camX, floorf(c.y) - camY, d->aMul, d->lenMul, d->foot);
    }
    // the homestead (walls, houses, barns, wells...)
    if (inHomestead()) {
        for (const HPiece& pc : hs.pieces) {
            if (!(pc.tx + pc.w() >= camX / TS - 2 && pc.tx <= (camX + LW) / TS + 2 && pc.ty + pc.h() >= camY / TS - 8 && pc.ty <= (camY + LH) / TS + 3)) continue;
            const shadowdef::HomeShadow* d = shadowdef::homeShadow(pc.kind);
            if (!d) continue;
            const float x0 = pc.tx * (float)TS - camX, y0 = pc.ty * (float)TS - camY, w = pc.w() * (float)TS, h = pc.h() * (float)TS;
            const float fx = x0 + d->foot.ox + d->foot.ow * w, fy = y0 + d->foot.oy + d->foot.oh * h;
            const Spr& sp = d->spr(shadowdef::HomeCtx{*this, A, pc});
            if (d->shape == shadowdef::SH_WIDE) wide(sp, fx, fy, d->aMul, d->lenMul);
            else standing(sp, fx, fy, d->aMul, d->lenMul, d->footKind);
        }
    }

    // ---- read the layer back as the shadow mask, before the creatures are cast: only things that stand still darken what walks through them
    // The finished layer is shrunk to half size (the soft edges); the same picture is then the mask. Every other frame is enough.
    auto toSoft = [&]() {
        SDL_SetRenderTarget(ren, shadowSoftTex);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
        SDL_RenderClear(ren);
        SDL_SetTextureBlendMode(shadowTex, SDL_BLENDMODE_NONE);  // copy, not blend: the shrunken layer keeps its own alpha
        SDL_SetTextureAlphaMod(shadowTex, 255);
        SDL_Rect half{0, 0, LW / 2, LH / 2};
        SDL_RenderCopy(ren, shadowTex, nullptr, &half);
        SDL_SetTextureBlendMode(shadowTex, SDL_BLENDMODE_BLEND);
    };
    const bool readNow = !prevMask || ((shadowFrame++ & 1) == 0);
    if (readNow) {
        toSoft();  // the render target is now the half-size layer
        const int mw = LW >> 1, mh = LH >> 1;
        shadowMask.resize((size_t)mw * mh);
        const SDL_Rect all{0, 0, mw, mh};
        if (SDL_RenderReadPixels(ren, &all, SDL_PIXELFORMAT_RGBA8888, shadowMask.data(), mw * 4) == 0) {
            shadowMaskW = mw, shadowMaskH = mh, shadowMaskShift = 1;
            shadowMaskCamX = camX, shadowMaskCamY = camY;
            shadowMaskOn = true;
        }
        SDL_SetRenderTarget(ren, shadowTex);
    } else {
        shadowMaskOn = prevMask;  // keep the last mask (it is anchored in the world, not on the screen)
    }
    shadowMaskAlpha = alpha;

    // ---- creatures: the hunter, villagers, animals. Each casts its own current frame, with a contact oval under the feet.
    // airH: how high it is off the ground (a bird): then only its oval is drawn, it slides along the sun and fades with the height
    auto creature = [&](const Spr& s, float fx, float fy, bool flip, float airH = 0.f, float aFade = 1.f) {
        if (!s.ok() || aFade <= 0.02f) return;
        if (fx < -60 || fx > LW + 60 || fy < -60 || fy > LH + 90) return;
        const float lift = 1.f / (1.f + airH * 0.06f);
        const float rx = std::max(3.5f, s.w * s.sc * 0.34f) * (0.55f + 0.45f * lift), ry = std::max(2.f, rx * 0.5f);
        // the oval sits a little in front of the feet, so more than its upper half shows below the sprite
        blob(fx + dx * airH * K * 0.5f, fy + ry * 0.45f + dy * airH * K * 0.5f * SQ, rx, ry, aFade * (0.4f + 0.6f * lift));
        if (airH < 5.f) cast(s, fx, fy, flip, 0.9f * aFade, 0.85f);
    };
    if (!p.dead && state != ST_TITLE) {
        const FacingView fv = facingView(p.face8);
        const int st = std::max(0, std::min((int)HS_COUNT - 1, p.anim.state));
        const int pose = std::min(p.anim.frame(), hunterPoseCount(st) - 1);
        creature(A.hunter[fv.view][st][pose], floorf(p.pos.x) - camX, floorf(p.pos.y) - camY, fv.mirror);
    }
    for (const Animal& a : animals) {
        const float ax = floorf(a.pos.x) - camX, ay = floorf(a.pos.y) - camY;
        if (ax < -60 || ax > LW + 60 || ay < -60 || ay > LH + 90) continue;
        if (a.dead) {  // the carcass lies where it fell (and fades as it goes)
            creature(A.animal[a.kind][a.variant + 2 * a.tier][4], ax, ay - 6.f, a.face < 0, 0.f, a.fade);
            continue;
        }
        const AnimalDef& D = ANIMAL[a.kind];
        const float spd = a.vel.len();
        const RichAnimal* rich = A.rich[a.kind].get();
        float air = a.air, hop = (D.hop && spd > 1) ? fabsf(sinf(a.animT * 3.14159f * 2)) * 3.f : 0.f;
        const Spr* sp;
        bool mirror = a.face < 0;
        if (rich) {
            const AnimalClips* ac = animalClips(a.kind);
            const int st = std::max(0, std::min((int)AN_COUNT - 1, a.anim.state));
            sp = &rich->s[a.variant + 2 * a.tier][std::max(0, std::min((int)AV_VIEWS - 1, a.face8))][st][std::min(a.anim.frame(), ac->poses[st] - 1)];
            mirror = false;
            hop = 0.f;
            if (!(D.flags & BH_FLIES)) air = 0.f;
        } else {
            const int fr = (a.st == AS_REST || a.st == AS_FEED) ? 0 : (spd > 1 ? ((int)a.animT) & 3 : 0);
            sp = &A.animal[a.kind][a.variant + 2 * a.tier][fr];
        }
        creature(*sp, ax, ay, mirror, air + hop * 0.5f);
    }
    if (world.mapId == B_HOME)
        for (const Villager& v : villagers)
            creature(A.T.villager[v.look % 5][v.moving ? ((int)(v.animT * 8.f)) & 3 : 0], floorf(v.pos.x) - camX, floorf(v.pos.y) - camY, v.faceLeft);
    {
        const V2& k = world.keeper;
        creature(A.keeper[((int)(timeAcc * 1.6f)) & 1], floorf(k.x) - camX, floorf(k.y) - camY, p.pos.x < k.x);
        const V2& q = world.questGiver;
        creature(A.quester[elderMoving ? ((int)(elderAnimT * 5.f)) & 1 : ((int)(timeAcc * 1.3f)) & 1], floorf(q.x) - camX, floorf(q.y) - camY,
                 elderMoving ? elderFaceLeft : p.pos.x < q.x);
        for (size_t i = 0; i < world.npcs.size(); i++) {
            const World::Npc& n = world.npcs[i];
            creature(A.T.npc[n.kind][((int)(timeAcc * 1.5f + i)) & 1], floorf(n.pos.x) - camX, floorf(n.pos.y) - camY, p.pos.x < n.pos.x);
        }
    }
    if (inHomestead())
        for (const HAnimal& a : hs.animals) {
            ensureDomArt();
            const DomDef& D = DOMS[a.kind];
            const int st = a.state == 1 ? DS_WALK : a.state == 2 ? DS_EAT : DS_IDLE;
            Spr s = A.Dom.get(a.kind, a.face8, st, (int)(a.animT * (st == DS_WALK ? 6.f : 2.f)) % DS_FRAMES[st]);
            if (!a.adult(D)) s.sc *= 0.55f + 0.45f * std::min(1.f, a.age / D.growSec);  // a young one is drawn smaller; so is its shadow
            creature(s, floorf(a.px) - camX, floorf(a.py) - camY, false);
        }

    // ---- the layer is finished. Make it white (it only has to say where the shadow lies), shrink it, and turn it into the multiplier
    {
        SDL_BlendMode oldBm = SDL_BLENDMODE_NONE;
        SDL_GetRenderDrawBlendMode(ren, &oldBm);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);  // ADD keeps the alpha and pushes the colour up to white
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderFillRect(ren, nullptr);
        SDL_SetRenderDrawBlendMode(ren, oldBm);
    }
    toSoft();
    SDL_SetRenderTarget(ren, shadowModTex);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderClear(ren);  // white = lit: multiplying by white changes nothing
    SDL_SetTextureColorMod(shadowSoftTex, (Uint8)(shadowTint[0] * 255.f), (Uint8)(shadowTint[1] * 255.f), (Uint8)(shadowTint[2] * 255.f));
    SDL_SetTextureAlphaMod(shadowSoftTex, (Uint8)(255.f * alpha));
    SDL_RenderCopy(ren, shadowSoftTex, nullptr, nullptr);  // over white: the shadow colour, as strong as the shadow is
    SDL_SetTextureColorMod(shadowSoftTex, 255, 255, 255);
    SDL_SetTextureAlphaMod(shadowSoftTex, 255);
    // ---- clouds: big soft patches that drift with the wind (they stand still on the land, the land does not follow the camera)
    constexpr bool CLOUD_SHADOWS = true;
    if (CLOUD_SHADOWS && cloudTex && !moonLit && shadowBoost == 1.f) {
        const float cover = clampf(bw[B_HOME] + bw[B_HOMESTEAD] + bw[B_SWAMP] + bw[B_MOUNTAIN] * 0.9f + bw[B_SNOW] * 0.7f + bw[B_DESERT] * 0.2f, 0.f, 1.f);
        const float cs = 0.17f * cover * (1.f - stormK) * smooth01(0.05f, 0.35f, sun);
        if (cs > 0.01f) {
            constexpr float TILE = 384.f;  // world px: one 128 x 128 picture drawn at 3x
            const float ox = fmodf(timeAcc * 5.f - camX, TILE), oy = fmodf(timeAcc * 2.f - camY, TILE);
            const float x0 = (ox > 0 ? ox - TILE : ox) * 0.5f, y0 = (oy > 0 ? oy - TILE : oy) * 0.5f;  // half size: this layer is half the screen
            SDL_SetTextureColorMod(cloudTex, (Uint8)(shadowTint[0] * 255.f), (Uint8)(shadowTint[1] * 255.f), (Uint8)(shadowTint[2] * 255.f));
            SDL_SetTextureAlphaMod(cloudTex, (Uint8)(255.f * cs));
            // the tiles meet exactly (each one ends where the next begins): an overlap would be darkened twice and show as a seam
            for (int j = 0; j < 3; j++)
                for (int i = 0; i < 3; i++) {
                    const int ex0 = (int)floorf(x0 + i * TILE * 0.5f), ex1 = (int)floorf(x0 + (i + 1) * TILE * 0.5f);
                    const int ey0 = (int)floorf(y0 + j * TILE * 0.5f), ey1 = (int)floorf(y0 + (j + 1) * TILE * 0.5f);
                    const SDL_Rect d{ex0, ey0, ex1 - ex0, ey1 - ey0};
                    if (d.x >= LW / 2 || d.y >= LH / 2) continue;
                    SDL_RenderCopy(ren, cloudTex, nullptr, &d);
                }
            SDL_SetTextureColorMod(cloudTex, 255, 255, 255);
            SDL_SetTextureAlphaMod(cloudTex, 255);
        }
    }
    SDL_SetRenderTarget(ren, prevTarget);
    if (prevTarget == canvas) SDL_RenderSetScale(ren, (float)LS, (float)LS);  // (switching targets reset the scale)
    SDL_Rect whole{0, 0, LW, LH};
    SDL_RenderCopy(ren, shadowModTex, nullptr, &whole);  // multiplied over the ground (the texture's blend mode is MOD)
}

// ================================================================== darkness, pools of light, colour, glows
void Game::drawLighting(float night) {
    const bool outdoors = interior < 0;
    if (outdoors && delve < 0) {  // the season's light: a faint wash over the world
        static const u32 WASH[SEASON_COUNT] = {0, RGBA(255, 210, 90, 18), RGBA(230, 120, 40, 34), RGBA(180, 210, 255, 44)};
        if (WASH[season()]) fillRect(0, 0, LW, LH, WASH[season()]);
    }
    const float flash = flashT > 0 ? clampf(flashT / 0.45f, 0.f, 1.f) : 0.f;
    // nightsight tonic turns night into dusk; storms darken the day
    float dark = p.nightT > 0 ? night * 0.3f : night;
    const float night2 = dark;
    dark = clampf(dark + stormK * 0.32f * (1.f - dark * 0.5f), 0.f, 1.f);

    // ---- dawn / dusk colour, and the colour of the night in each part of the world
    const float sun = sinf(tod() * 6.2832f);
    const bool rising = cosf(tod() * 6.2832f) > 0;
    const float warm = outdoors ? clampf(1.f - fabsf(sun) * 3.2f, 0.f, 1.f) * (1.f - 0.6f * stormK) : 0.f;
    if (warm > 0.01f) {
        if (rising) fillRect(0, 0, LW, LH, RGBA(255, 150, 140, (int)(warm * 40)));                          // dawn: rose and gold
        else {
            fillRect(0, 0, LW, LH, RGBA(255, 112, 44, (int)(warm * 50)));                                  // sunset: orange
            const float dusk = smooth01(0.f, -0.32f, sun);
            if (dusk > 0.01f) fillRect(0, 0, LW, LH, RGBA(120, 56, 150, (int)(dusk * warm * 46)));          // then violet
        }
    }
    // golden hour: low sun, a rich amber over everything before the sunset colours take over; blue hour: the cold light just after
    if (outdoors) {
        const float golden = rising ? smooth01(0.55f, 0.18f, sun) * 0.6f : smooth01(0.5f, 0.2f, sun);
        const float goldMask = smooth01(0.f, 0.1f, sun);  // only while the sun is above the horizon
        if (golden * goldMask > 0.01f) fillRect(0, 0, LW, LH, RGBA(255, 196, 96, (int)(30 * golden * goldMask * (1.f - stormK))));
        const float blueHr = smooth01(-0.02f, -0.12f, sun) * (1.f - smooth01(-0.3f, -0.5f, sun));
        if (blueHr > 0.01f) fillRect(0, 0, LW, LH, RGBA(40, 76, 180, (int)((rising ? 28 : 46) * blueHr * (1.f - stormK))));
    }
    float nr = 14, ng = 22, nb = 70, wsum = bw[B_HOME] + 1e-4f;
    auto mixNight = [&](int b, float r, float g, float bl) { nr += bw[b] * (r - 14); ng += bw[b] * (g - 22); nb += bw[b] * (bl - 70); };
    mixNight(B_SNOW, 22, 46, 112);    // cold deep blue
    mixNight(B_SWAMP, 8, 40, 46);     // green-black
    mixNight(B_DESERT, 46, 26, 78);   // violet
    mixNight(B_MOUNTAIN, 20, 24, 58); // grey-blue
    (void)wsum;

    // ---- the lights on screen
    static std::vector<LightSrc> ls;
    ls.clear();
    auto onScr = [&](const LightSrc& l) { return l.x > camX - l.r && l.x < camX + LW + l.r && l.y > camY - l.r && l.y < camY + LH + l.r; };
    for (const LightSrc& l : lampList) {
        if (!onScr(l)) continue;
        LightSrc c = l;
        if (c.night) c.k *= smooth01(0.15f, 0.55f, dark);  // windows and lamps come on as it gets dark
        c.r *= flick(timeAcc, l.x * 0.7f + l.y * 0.3f) * (c.tone == 0 ? 1.f : 1.f);
        ls.push_back(c);
    }
    if (outdoors && state != ST_TITLE) {
        // the hunter carries a light at night: wider with a lantern or a nightsight tonic
        float pr = 44.f * (p.has(CG_LANTERN) ? 1.6f : 1.f) * (p.nightT > 0 ? 1.35f : 1.f);
        ls.push_back({p.pos.x, p.pos.y - 8.f, pr * (0.97f + 0.03f * sinf(timeAcc * 5.f)), 0.9f, 2, true, false});
    }

    // ---- the darkness map: one cell is 4 x 4 px, smoothly scaled up
    constexpr int CS = 4;
    constexpr int GW = LW / CS, GH = (LH + CS - 1) / CS;
    const float baseA = outdoors ? dark * 150.f : 0.f;
    if (baseA * (1.f - 0.9f * flash) > 2.f) {
        if (!darkTex) {
            const char* old = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
            darkTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, GW, GH);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, old ? old : "0");
            if (darkTex) SDL_SetTextureBlendMode(darkTex, SDL_BLENDMODE_BLEND);
        }
        if (darkTex) {
            static uint32_t px[GW * GH];
            const float a0 = baseA * (1.f - 0.85f * flash);
            const uint32_t rgb = ((uint32_t)clampf(nr, 0, 255) << 16) | ((uint32_t)clampf(ng, 0, 255) << 8) | (uint32_t)clampf(nb, 0, 255);
            for (int cy = 0; cy < GH; cy++)
                for (int cx = 0; cx < GW; cx++) {
                    const float wx = camX + cx * CS + CS / 2, wy = camY + cy * CS + CS / 2;
                    float lit = 0.f;
                    for (const LightSrc& l : ls) {
                        if (l.south && wy < l.y - 4.f) continue;  // a window lights the ground in front of the house, not behind it
                        const float dx = wx - l.x, dy = (wy - l.y) * 1.2f;
                        const float d2 = dx * dx + dy * dy, r2 = l.r * l.r;
                        if (d2 >= r2) continue;
                        if (l.occl && l.r > 30.f && world.lightBlocked(l.x, l.y, wx, wy)) continue;  // in the shadow of a building or a rock
                        const float t = 1.f - sqrtf(d2) / l.r;
                        lit += t * t * (3.f - 2.f * t) * l.k;
                    }
                    lit = std::min(1.f, lit);
                    px[cy * GW + cx] = ((uint32_t)(a0 * (1.f - lit * 0.93f)) << 24) | rgb;
                }
            SDL_UpdateTexture(darkTex, nullptr, px, GW * 4);
            SDL_Rect dst{0, 0, GW * CS, GH * CS};
            SDL_RenderCopy(ren, darkTex, nullptr, &dst);
        }
    }

    // ---- warm glows on top of the pools (additive)
    SDL_Texture* g = A.glowWarm.t;
    const float strength = 0.28f + 0.72f * night2;
    {
        const float fl = 0.85f + 0.15f * sinf(timeAcc * 11.f) + 0.05f * sinf(timeAcc * 27.f);
        SDL_SetTextureAlphaMod(g, (Uint8)(255 * clampf(strength * fl, 0, 1)));
        const int sz = (int)(150 * fl);
        SDL_Rect d{(int)world.fire.x - camX - sz / 2, (int)world.fire.y - 12 - camY - sz / 2, sz, sz};
        SDL_RenderCopy(ren, g, nullptr, &d);
        if (night2 > 0.05f) {
            SDL_SetTextureAlphaMod(g, (Uint8)(190 * night2));
            for (int wx : {-19, 13}) {
                SDL_Rect w{(int)world.hut.x + wx - camX - 22, (int)world.hut.y - 20 - camY - 22, 44, 44};
                SDL_RenderCopy(ren, g, nullptr, &w);
            }
            SDL_Rect pl{(int)p.pos.x - camX - 34, (int)p.pos.y - 8 - camY - 34, 68, 68};
            SDL_SetTextureAlphaMod(g, (Uint8)(110 * night2));
            if (state != ST_TITLE) SDL_RenderCopy(ren, g, nullptr, &pl);
        }
        for (auto& lt : world.lanterns) {
            if (lt.x < camX - 60 || lt.x > camX + LW + 60 || lt.y < camY - 60 || lt.y > camY + LH + 60) continue;
            float lf = 0.85f + 0.15f * sinf(timeAcc * 9.f + lt.x);
            SDL_SetTextureAlphaMod(g, (Uint8)(clampf(lt.r * lf * (0.3f + 0.7f * night2), 0, 1) * 255));
            int sz = (int)(78 * lf);
            SDL_Rect d{(int)lt.x - camX - sz / 2, (int)lt.y - camY - sz / 2, sz, sz};
            SDL_RenderCopy(ren, g, nullptr, &d);
        }
        for (auto& st : structs) {  // placed campfires light the night
            if (st.kind != PL_CAMPFIRE) continue;
            const V2 c = st.pos();
            if (c.x < camX - 70 || c.x > camX + LW + 70 || c.y < camY - 70 || c.y > camY + LH + 70) continue;
            float lf = 0.85f + 0.15f * sinf(timeAcc * 10.f + c.x) + 0.05f * sinf(timeAcc * 23.f);
            SDL_SetTextureAlphaMod(g, (Uint8)(clampf((0.4f + 0.6f * night2) * lf, 0, 1) * 255));
            int sz = (int)(120 * lf);
            SDL_Rect d{(int)c.x - camX - sz / 2, (int)c.y - 8 - camY - sz / 2, sz, sz};
            SDL_RenderCopy(ren, g, nullptr, &d);
        }
        drawHsLights(night2);
        // lit windows: the glass glows and a halo spills out of it; the salon is warmest, the flower shop rosy
        for (const LightSrc& w : winList) {
            if (w.x < camX - 40 || w.x > camX + LW + 40 || w.y < camY - 40 || w.y > camY + LH + 40) continue;
            const bool salon = w.tone == BLD_SALON, flower = w.tone == BLD_FLOWER, smith = w.tone == BLD_SMITH;
            const u32 col = salon ? RGBA(255, 176, 150, 0) : flower ? RGBA(255, 198, 176, 0) : smith ? RGBA(255, 150, 70, 0) : RGBA(255, 214, 128, 0);
            fillRect((int)w.x - 6 - camX, (int)w.y - 5 - camY, 12, 10, RGBA(cR(col), cG(col), cB(col), (int)(205 * w.k)));
            fillRect((int)w.x - 6 - camX, (int)w.y - camY, 12, 1, RGBA(120, 80, 40, (int)(150 * w.k)));  // the cross bars stay dark
            fillRect((int)w.x - camX, (int)w.y - 5 - camY, 1, 10, RGBA(120, 80, 40, (int)(150 * w.k)));
            SDL_SetTextureColorMod(g, cR(col), cG(col), cB(col));
            SDL_SetTextureAlphaMod(g, (Uint8)(150 * w.k * (0.5f + 0.5f * clampf(dark * 1.4f, 0.f, 1.f))));
            SDL_Rect d{(int)w.x - camX - 26, (int)w.y - camY - 22, 52, 52};
            SDL_RenderCopy(ren, g, nullptr, &d);
            SDL_SetTextureColorMod(g, 255, 255, 255);
        }
        SDL_SetTextureAlphaMod(g, 255);
        // sparks and embers rising from the fires, brighter against the dark
        auto sparks = [&](float fx, float fy, float seed, int n, float lift) {
            if (fx < camX - 40 || fx > camX + LW + 40 || fy < camY - lift - 20 || fy > camY + LH + 40) return;
            const float vis = 0.55f + 0.45f * clampf(dark * 1.3f, 0.f, 1.f);
            for (int i = 0; i < n; i++) {
                const uint32_t h = hash2(i, (int)seed, 5150);
                const float rate = 0.35f + (h & 15) * 0.02f;
                const float ph = fmodf(timeAcc * rate + (h >> 4 & 255) / 255.f, 1.f);  // 0 at the flame, 1 when it has burnt out
                const float sx = fx + sinf(timeAcc * 1.7f + (h >> 12 & 63)) * 3.f * ph + ((int)(h >> 20 & 15) - 7) * 0.7f * (0.3f + ph) + ph * 4.f * sinf(seed + i);
                const float sy = fy - ph * lift;
                const int r = 255, gg = (int)(220 - 150 * ph), b = (int)(120 - 100 * ph);  // yellow, then orange, then red
                const int a = (int)(235 * (1.f - ph) * (1.f - ph * 0.3f) * vis * (0.6f + 0.4f * sinf(timeAcc * 18.f + i * 3.f)));
                if (a < 8) continue;
                fillRect((int)sx - camX, (int)sy - camY, ph < 0.35f && (h & 1) ? 2 : 1, 1 + (ph < 0.2f), RGBA(r, gg, std::max(b, 0), a));
            }
        };
        sparks(world.fire.x, world.fire.y - 10.f, 1.f, 12, 38.f);
        for (auto& st : structs)
            if (st.kind == PL_CAMPFIRE) sparks(st.pos().x, st.pos().y - 6.f, (float)(st.tx * 31 + st.ty), 8, 30.f);
    }
    if (!outdoors) return;

    // ---- things that glow on their own: crystals, rune stones, cave mouths, rich ore, items lying on the ground
    const float need = 0.3f + 0.7f * clampf(dark * 1.2f, 0.f, 1.f);  // easier to spot in the dark, but still a little glow by day
    SDL_Texture* gc = A.glowCool.t;
    auto glow = [&](float x, float y, int sz, u32 col, float a, bool warmTex) {
        if (x < camX - sz || x > camX + LW + sz || y < camY - sz || y > camY + LH + sz) return;
        SDL_Texture* t = warmTex ? A.glowWarm.t : gc;
        SDL_SetTextureColorMod(t, cR(col), cG(col), cB(col));
        SDL_SetTextureAlphaMod(t, (Uint8)(255 * clampf(a, 0.f, 1.f)));
        SDL_Rect d{(int)x - camX - sz / 2, (int)y - camY - sz / 2, sz, sz};
        SDL_RenderCopy(ren, t, nullptr, &d);
    };
    for (size_t i = 0; i < world.props.size(); i++) {
        const Prop& pr = world.props[i];
        const float pulse = 0.75f + 0.25f * sinf(timeAcc * 2.2f + pr.x * 0.1f);
        switch (pr.kind) {
            case P_CRYSTAL: glow(pr.x, pr.y - 10, 44, HEX(0x7fe8ff), need * pulse * 0.9f, false); break;
            case P_STANDING: glow(pr.x, pr.y - 20, 40, HEX(0x6ea8ff), need * pulse * 0.75f, false); break;  // the runes
            case P_CAVE: glow(pr.x, pr.y - 6, 70, HEX(0xffa850), (0.3f + 0.7f * dark) * (0.85f + 0.15f * sinf(timeAcc * 7.f + pr.x)), true); break;  // a rim of light at the mouth
            case P_ICEHUT: if (dark > 0.3f) glow(pr.x, pr.y - 10, 60, HEX(0x9fd8ff), dark * 0.6f, false); break;
            default: break;
        }
    }
    for (size_t i = 0; i < world.nodes.size(); i++) {
        const ResNode& n = world.nodes[i];
        if (n.x < camX - 40 || n.x > camX + LW + 40 || n.y < camY - 40 || n.y > camY + LH + 40) continue;
        const int rs = nodeRes(n.kind);
        u32 col;
        switch (rs) {
            case RS_CRYSTAL: col = HEX(0x7fe8ff); break;
            case RS_GOLD: col = HEX(0xffd84a); break;
            case RS_AMBER: col = HEX(0xffa030); break;
            default: continue;
        }
        if (!harvestUp(true, (int)i)) continue;
        glow(n.x, n.y - 6, 30, col, need * (0.7f + 0.3f * sinf(timeAcc * 3.f + n.x)), rs != RS_CRYSTAL);
    }
    for (const GroundItem& gi : items)
        if (gi.item >= 0) glow(gi.pos.x, gi.pos.y - 3, 20, HEX(0xfff0b0), (0.25f + 0.75f * dark) * (0.7f + 0.3f * sinf(timeAcc * 4.f + gi.pos.x)), false);
    SDL_SetTextureColorMod(gc, 255, 255, 255);
    SDL_SetTextureColorMod(A.glowWarm.t, 255, 255, 255);
    SDL_SetTextureAlphaMod(gc, 255);
    SDL_SetTextureAlphaMod(A.glowWarm.t, 255);

    // ---- lightning: a white flash over the whole screen
    if (flash > 0.f) fillRect(0, 0, LW, LH, RGBA(225, 235, 255, (int)(flash * flash * 150)));
}

// ================================================================== sky effects: aurora, shimmer, wisps, rain
void Game::drawSkyFx(float night2) {
    if (interior >= 0 || delve >= 0 || state == ST_TITLE) return;
    const float day = daylight();

    // ---- aurora over the snowfields, on clear nights
    const float au = bw[B_SNOW] * smooth01(0.35f, 0.8f, night2) * (1.f - stormK);
    if (au > 0.03f) {
        for (int rb = 0; rb < 3; rb++) {
            for (int x = 0; x < LW; x += 3) {
                const float fx = (float)x;
                const float y = 22.f + rb * 15.f + sinf(fx * 0.021f + timeAcc * 0.35f + rb * 1.7f) * 13.f + sinf(fx * 0.055f - timeAcc * 0.22f) * 5.f;
                const float hgt = 22.f + 8.f * sinf(fx * 0.03f + timeAcc * 0.4f + rb);
                const float inten = au * (0.55f + 0.45f * sinf(fx * 0.04f + timeAcc * 0.6f + rb * 2.f));
                const float u = 0.5f + 0.5f * sinf(fx * 0.012f + rb * 0.9f + timeAcc * 0.1f);  // green to violet along the ribbon
                const int r = (int)(70 + 110 * u), gg = (int)(255 - 120 * u), b = (int)(150 + 105 * u);
                for (int s = 0; s < 3; s++) {  // brightest at the lower edge, fading upwards
                    const float k = (s + 1) / 3.f;
                    fillRect(x, (int)(y - hgt * (1.f - k)), 3, std::max(1, (int)(hgt / 3.f)), RGBA(r, gg, b, (int)(30 * inten * k)));
                }
            }
        }
    }

    // ---- heat shimmer in the desert at midday
    const float sh = bw[B_DESERT] * smooth01(0.6f, 0.95f, day) * (1.f - stormK);
    if (sh > 0.05f) {
        for (int k = 0; k < 9; k++) {
            const float y0 = k * (LH / 9.f) + sinf(timeAcc * 0.6f + k * 1.3f) * 5.f;
            for (int x = 0; x < LW; x += 8) {
                const float dy = sinf(x * 0.07f + timeAcc * 2.2f + k * 1.9f) * 2.f;
                fillRect(x, (int)(y0 + dy), 8, 1, RGBA(255, 244, 205, (int)(15 * sh)));
            }
        }
    }

    // ---- will-o'-the-wisps drifting through the swamp at night
    const float wi = bw[B_SWAMP] * smooth01(0.3f, 0.7f, night2);
    if (wi > 0.05f) {
        SDL_Texture* t = A.glowCool.t;
        SDL_SetTextureColorMod(t, 120, 255, 190);
        const int cell = 110;
        const int gx0 = camX / cell - 1, gy0 = camY / cell - 1, gx1 = (camX + LW) / cell + 1, gy1 = (camY + LH) / cell + 1;
        for (int gy = gy0; gy <= gy1; gy++)
            for (int gx = gx0; gx <= gx1; gx++) {
                const uint32_t h = hash2(gx, gy, 9091);
                if ((h & 3) != 0) continue;  // one cell in four has a wisp
                const float ph = (h >> 8 & 255) * 0.0246f;
                const float x = gx * cell + (h & 63) + sinf(timeAcc * 0.35f + ph) * 26.f;
                const float y = gy * cell + (h >> 4 & 63) + cosf(timeAcc * 0.28f + ph * 1.3f) * 14.f - 10.f;
                const float b = (0.55f + 0.45f * sinf(timeAcc * 1.6f + ph * 3.f)) * wi;
                const int sz = 34;
                SDL_SetTextureAlphaMod(t, (Uint8)(210 * b));
                SDL_Rect d{(int)x - camX - sz / 2, (int)y - camY - sz / 2, sz, sz};
                SDL_RenderCopy(ren, t, nullptr, &d);
            }
        SDL_SetTextureColorMod(t, 255, 255, 255);
        SDL_SetTextureAlphaMod(t, 255);
    }

    // ---- rain: streaks that fall slanted, plus a cool grey cast over everything
    if (stormK > 0.02f) {
        fillRect(0, 0, LW, LH, RGBA(40, 52, 76, (int)(46 * stormK)));
        const int n = (int)(230 * stormK) + 10;
        for (int i = 0; i < n; i++) {
            const uint32_t h = hash2(i, 31, 777);
            const float sp = 230.f + (h & 63);
            const float x = fmodf((h >> 6 & 1023) * (LW + 80) / 1023.f + rainDrift + 4000.f, (float)(LW + 80)) - 20.f;  // carried sideways by the wind
            const float y = fmodf((h >> 16 & 1023) * LH / 1023.f + timeAcc * sp, (float)LH);
            fillRect((int)x, (int)y, 1, 3 + (int)(h >> 28 & 1), RGBA(195, 215, 250, (int)(175 * stormK)));
            fillRect((int)x + 1, (int)y + 3, 1, 1, RGBA(185, 205, 245, (int)(60 * stormK)));  // the streak leans downwind
        }
        const int splashes = (int)(70 * stormK);  // drops landing: a dot, then a small flat ring, somewhere new every fifth of a second
        for (int i = 0; i < splashes; i++) {
            const float cyc = timeAcc * 5.f + i * 0.618f;
            const int cn = (int)cyc;
            const uint32_t h = hash2(i, cn, 991);
            const int x = (int)(h % LW), y = (int)((h >> 10) % LH);
            if (cyc - cn < 0.5f) fillRect(x, y, 1, 1, RGBA(215, 230, 252, (int)(150 * stormK)));
            else fillRect(x - 1, y, 3, 1, RGBA(205, 222, 250, (int)(85 * stormK)));
        }
    }

    // ---- sandstorm / blizzard: a thick haze over everything (the streaks are the weather particles, see drawBiomeFx in game_hud.cpp)
    if (galeK > 0.02f) {
        if (bw[B_DESERT] >= bw[B_SNOW]) fillRect(0, 0, LW, LH, RGBA(206, 164, 96, (int)(95 * galeK)));
        else fillRect(0, 0, LW, LH, RGBA(226, 234, 246, (int)(120 * galeK)));
    }
}
