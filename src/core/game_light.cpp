// game_light.cpp - light and weather outdoors: the night darkness with pools of light that buildings and rocks cast shadows from, dawn and
// dusk colour, flickering flames, tree and rock shadows that swing with the sun, aurora over the snow, wisps in the swamp, glowing
// crystals and ore, heat shimmer in the desert, and storms with rain and lightning. (The delves keep their own darkness, delve_render.cpp.)
#include <algorithm>
#include <cmath>
#include "core/game.hpp"
#include "core/logic_util.hpp"
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

// ================================================================== update: storms and the lamp list
void Game::updateLight(float dt) {
    lampList.clear();
    const bool outside = interior < 0 && delve < 0 && state != ST_TITLE;
    // ---- storms: now and then, in the green parts of the world and the mountains
    const float rainW = bw[B_HOME] + bw[B_SWAMP] + bw[B_MOUNTAIN] * 0.8f;
    if (!outside || rainW < 0.5f) {
        stormT = 0;  // it blows over when the hunter leaves
    } else {
        stormNext -= dt;
        if (stormT <= 0 && stormNext <= 0) {
            stormT = wrng.range(40.f, 90.f);
            stormNext = wrng.range(160.f, 420.f);
        }
        if (stormT > 0) stormT -= dt;
    }
    const float target = (stormT > 0 && outside) ? 1.f : 0.f;
    stormK += clampf(target - stormK, -0.35f * dt, 0.2f * dt);
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
        if (st.kind == PL_CAMPFIRE) lampList.push_back({st.pos().x, st.pos().y - 8.f, 62.f, 1.f, 0, true, false});
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

// ================================================================== tree and rock shadows
// The sun crosses the sky from east (left) to west (right): shadows point the other way, short at noon, long near dawn and dusk.
void Game::drawGroundShadows() {
    if (interior >= 0 || delve >= 0) return;
    const float t = tod();
    const float sun = sinf(t * 6.2832f);
    if (sun < 0.06f) return;  // night: no sun, no shadows
    const float day = daylight();
    const float alphaBase = clampf(day * (0.18f + 0.30f * std::min(1.f, sun * 1.6f)) * (1.f - 0.6f * stormK), 0.f, 0.5f);
    const float alpha = std::min(0.55f, alphaBase * (1.f + 0.3f * smooth01(0.5f, 0.2f, sun)));
    if (alpha < 0.02f) return;
    // direction on the ground (down = south); the view is slanted, so the sideways reach is flattened
    const float ang = (3.14159f - t * 6.2832f);                      // sun at the east (t=0): points west; noon: points south; west: east
    const float dirx = cosf(ang), diry = sinf(ang < 0 ? -ang : ang);  // always a little southward
    const float golden = smooth01(0.5f, 0.2f, sun);  // golden hour: the shadows stretch and darken
    const float len = clampf(0.42f / std::max(sun, 0.14f), 0.55f, 2.7f) * (1.f + 0.15f * golden);
    const float rot = atan2f(diry * 0.55f, dirx) * 57.2958f;
    SDL_Texture* tx = A.shadowSoft.t;
    SDL_SetTextureAlphaMod(tx, (Uint8)(255 * alpha));
    for (size_t i = 0; i < world.props.size(); i++) {
        const Prop& pr = world.props[i];
        float h, w;
        switch (pr.kind) {
            case P_OAK0: case P_OAK1: {
                const OakVar& v = OAK_VAR[oakVariantFor((int)(pr.x / TS), (int)(pr.y / TS))];
                h = v.h * 0.7f; w = v.w * 0.30f;
                break;
            }
            case P_PINE: h = PINE_H * 0.7f; w = 13.f; break;
            case P_SNOWPINE: h = SNOW_SPRUCE_H * 0.7f; w = 13.f; break;
            case P_DEADTREE0: case P_DEADTREE1: case P_BARETREE: h = DEAD_SPRUCE_H * 0.7f; w = 6.f; break;
            case P_BOULDER: h = 26.f; w = 18.f; break;
            case P_ROCK1: h = 14.f; w = 11.f; break;
            default: continue;
        }
        const float sx = pr.x - camX, sy = pr.y - camY;
        const float reach = h * len + 20.f;
        if (sx < -reach || sx > LW + reach || sy < -reach || sy > LH + reach) continue;
        const int rs = propRsId[i];
        if (rs >= 0 && (propSt[i].gone || propSt[i].fade > 0)) continue;  // felled: no shadow
        const float L = h * len, W = w * (0.9f + 0.25f * (len - 1.f));
        SDL_Rect dst{(int)sx, (int)(sy - W * 0.5f), (int)L, std::max(2, (int)W)};
        SDL_Point c{0, (int)(W * 0.5f)};
        SDL_RenderCopyEx(ren, tx, nullptr, &dst, rot, &c, SDL_FLIP_NONE);
    }
    SDL_SetTextureAlphaMod(tx, 255);
}

// ================================================================== darkness, pools of light, colour, glows
void Game::drawLighting(float night) {
    const bool outdoors = interior < 0;
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
            const float x = fmodf((h >> 6 & 1023) * (LW + 80) / 1023.f - timeAcc * 70.f + 4000.f, (float)(LW + 80)) - 20.f;
            const float y = fmodf((h >> 16 & 1023) * LH / 1023.f + timeAcc * sp, (float)LH);
            fillRect((int)x, (int)y, 1, 3 + (int)(h >> 28 & 1), RGBA(195, 215, 250, (int)(175 * stormK)));
            fillRect((int)x - 1, (int)y + 3, 1, 1, RGBA(185, 205, 245, (int)(60 * stormK)));
        }
    }
}
