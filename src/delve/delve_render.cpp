// delve_render.cpp - drawing the delves: monsters, floor warnings, traps, chests, the darkness with its pools of light, the boss bar.
#include <algorithm>
#include "core/game.hpp"

namespace {
float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
}

// a filled, flattened circle (the floor is seen from above at an angle) in screen space
static void discS(Game& g, int cx, int cy, int rx, u32 col) {
    const int ry = std::max(1, (int)(rx * 0.72f));
    for (int dy = -ry; dy <= ry; dy++) {
        const float k = 1.f - (float)(dy * dy) / (float)(ry * ry);
        const int half = (int)(rx * sqrtf(std::max(0.f, k)));
        g.fillRect(cx - half, cy + dy, half * 2 + 1, 1, col);
    }
}
static void ringS(Game& g, int cx, int cy, int rx, u32 col) {
    const int ry = std::max(1, (int)(rx * 0.72f));
    for (int i = 0; i < 48; i++) {
        const float a = i * 6.2832f / 48.f;
        g.fillRect(cx + (int)(cosf(a) * rx), cy + (int)(sinf(a) * ry), 1, 1, col);
    }
}

// floor level: trap plates, the warning circles of coming attacks, scorch marks
void Game::drawDelveFloor() {
    for (const DTrap& t : dtraps) {
        if (t.pos.x < camX - 20 || t.pos.x > camX + LW + 20 || t.pos.y < camY - 20 || t.pos.y > camY + LH + 20) continue;
        int st = 0;
        if (t.kind == TR_SPIKE) st = t.t < 1.8f ? 0 : (t.t < 2.2f ? 1 : (t.t < 2.9f ? 2 : 0));
        else st = t.t < 1.6f ? 0 : (t.t < 2.2f ? 1 : (t.t < 3.0f ? 2 : 0));
        sprW(t.kind == TR_SPIKE ? A.D.spike[st] : A.D.vent[st], t.pos.x, t.pos.y);
    }
    for (const Hazard& h : hazards) {
        const int sx = (int)h.pos.x - camX, sy = (int)h.pos.y - camY;
        if (sx < -120 || sx > LW + 120 || sy < -120 || sy > LH + 120) continue;
        const bool ice = h.kind != HZ_ROCK;
        if (!h.fired) {
            const float k = clampf(h.t / h.delay, 0.f, 1.f);
            const u32 base = ice ? HEX(0x6ac8ff) : HEX(0xe0a060);
            discS(*this, sx, sy, (int)h.r, withA(base, (int)(28 + 60 * k)));
            ringS(*this, sx, sy, (int)h.r, withA(HEX(0xff6a5a), 200));
            discS(*this, sx, sy, std::max(1, (int)(h.r * k)), withA(HEX(0xff6a5a), (int)(30 + 50 * k)));  // fills as the moment nears
            if (h.kind == HZ_ROCK && h.delay - h.t < 0.45f) {  // the stone falls
                const float fall = (h.delay - h.t) / 0.45f;
                sprW(A.D.rockChunk, h.pos.x, h.pos.y - fall * 90.f);
            }
        } else {
            const float k = clampf((h.t - h.delay) / h.after, 0.f, 1.f);
            if (h.kind == HZ_ROCK) discS(*this, sx, sy, (int)(h.r * 0.8f), withA(HEX(0x3a3128), (int)(110 * (1 - k))));
            else {
                discS(*this, sx, sy, (int)(h.r * (0.7f + 0.3f * k)), withA(HEX(0xdff6ff), (int)(120 * (1 - k))));
                if (h.r < 40.f) sprW(A.D.iceSpike[std::min(2, (int)(k * 3.f))], h.pos.x, h.pos.y);
                else {  // the big ones throw up a ring of spikes
                    for (int i = 0; i < 9; i++) {
                        const float a = i * 0.698f;
                        sprW(A.D.iceSpike[std::min(2, (int)(k * 3.f))], h.pos.x + cosf(a) * h.r * 0.8f, h.pos.y + sinf(a) * h.r * 0.58f);
                    }
                }
            }
        }
    }
}

void Game::drawDChest(const DChest& c) {
    sprW(A.shadow[0], c.pos.x, c.pos.y, false, 0.7f);
    sprW(A.D.chest[std::max(0, std::min(3, c.tier))][0], c.pos.x, c.pos.y);
    const float g = fmodf(timeAcc * 0.9f + c.pos.x * 0.03f, 2.4f);
    if (g < 0.25f) {
        const int gx = (int)c.pos.x - camX + 4, gy = (int)c.pos.y - camY - 14;
        fillRect(gx, gy - 1, 1, 3, RGBA(255, 255, 255, 220));
        fillRect(gx - 1, gy, 3, 1, RGBA(255, 255, 255, 220));
    }
    if (c.tier == 3) sprS(A.glowCool, (int)c.pos.x - camX, (int)c.pos.y - camY - 8, false, 0.5f + 0.2f * sinf(timeAcc * 3.f));
}

void Game::drawMonster(const Monster& m) {
    const MonsterDef& D = MONDEF(m.kind);
    if (m.pos.x < camX - 60 || m.pos.x > camX + LW + 60 || m.pos.y < camY - 70 || m.pos.y > camY + LH + 60) return;
    int st = MS_IDLE, fr = 0;
    const bool moving = m.vel.len() > 3.f && m.state == MN_CHASE;
    switch (m.state) {
        case MN_IDLE: st = MS_IDLE; fr = (int)(m.animT * 2.2f) & 1; break;
        case MN_CHASE: st = moving ? MS_WALK : MS_IDLE; fr = moving ? ((int)(m.animT * (m.kind == MK_BAT ? 14.f : 9.f)) & 3) : ((int)(m.animT * 2.2f) & 1); break;
        case MN_ATK: case MN_CAST: {
            float wind = D.windup;
            if (m.kind == MK_KING) wind = m.atkType == 0 ? 0.7f : m.atkType == 3 ? 0.6f : m.atkType == 1 ? 0.9f : m.atkType == 2 ? 0.8f : m.atkType == 4 ? 1.0f : m.atkType == 5 ? 0.8f : 1.1f;
            const bool cast = m.state == MN_CAST && monsterHasCast(m.kind);
            st = cast ? MS_CAST : MS_ATK;
            fr = m.stT < wind * 0.55f ? 0 : (m.stT < wind + 0.25f ? 1 : 2);
            break;
        }
        case MN_HURT: st = MS_HURT; fr = 0; break;
        case MN_DEAD: st = MS_DEATH; fr = std::min(3, (int)(m.deadT / 0.16f)); break;
    }
    if (!A.M.built) return;
    const Spr& s = A.M.get(m.kind, std::max(0, std::min((int)F_COUNT - 1, m.face8)), st, fr);
    if (!s.ok()) return;
    float alpha = 1.f;
    if (m.state == MN_DEAD && m.deadT > 22.f) alpha = clampf((30.f - m.deadT) / 8.f, 0.f, 1.f);
    const int shadow = m.kind == MK_KING || m.kind == MK_GOLEM || m.kind == MK_ICEGOLEM ? 2 : (m.kind == MK_BAT ? 0 : 1);
    sprW(A.shadow[shadow], m.pos.x, m.pos.y + 1, false, (m.state == MN_DEAD ? 0.5f : 0.8f) * alpha * (m.kind == MK_BAT ? 0.7f : 1.f));
    sprW(s, m.pos.x, m.pos.y, false, alpha);
    if (m.flash > 0 && s.flash) {
        Spr f = s;
        f.t = s.flash;
        sprW(f, m.pos.x, m.pos.y, false, m.flash / 0.16f * 0.85f * alpha);
    }
    if (m.elite && m.state != MN_DEAD) {  // a gold glint marks the elites
        const float pulse = 0.25f + 0.15f * sinf(timeAcc * 4.f + m.id);
        sprS(A.glowCool, (int)m.pos.x - camX, (int)m.pos.y - camY - (int)D.bodyH, false, pulse);
    }
    if (m.state != MN_DEAD && m.kind != MK_KING && (m.hpShow > 0 || m.elite)) {
        const int bx = (int)m.pos.x - camX - 10, by = (int)m.pos.y - camY - (int)(D.bodyH * 2 + 8);
        bar(bx, by, 20, 2, (float)m.hp / m.maxhp, m.elite ? HEX(0xe8b83a) : HEX(0xd8443f), HEX(0x3a1a18));
        if (m.hpShow > 0 || m.elite) textC((int)m.pos.x - camX, by - 7, std::string(m.elite ? "ELITE " : "") + D.name, m.elite ? C_GOLD : C_DIM, 1, true);
    }
}

// things that hang or fly above the hunter's head: the support beams and the ice bolts
void Game::drawDelveOverlay() {
    for (const MProj& b : mprojs) {
        sprW(A.shadow[0], b.pos.x, b.pos.y + 9, false, 0.4f);
        sprW(A.D.iceBolt, b.pos.x, b.pos.y, false, 1.f, atan2f(b.dir.y, b.dir.x) * 57.2958, true);
        sprS(A.glowCool, (int)b.pos.x - camX, (int)b.pos.y - camY, false, 0.7f);
    }
}

// ------------------------------------------------------------------ the dark
void Game::drawDelveDark() {
    constexpr int CS = 4;
    constexpr int GW = LW / CS, GH = (LH + CS - 1) / CS;
    if (!darkTex) {
        const char* old = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");  // smooth up-scaling: the light fades instead of showing blocks
        darkTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, GW, GH);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, old ? old : "0");
        if (darkTex) SDL_SetTextureBlendMode(darkTex, SDL_BLENDMODE_BLEND);
    }
    if (!darkTex) return;
    const bool frost = delveIsFrost(delve);
    struct L { float x, y, r, k; int tone; };
    static std::vector<L> ls;
    ls.clear();
    for (const DLight& l : world.lights)
        if (l.x > camX - l.rad && l.x < camX + LW + l.rad && l.y > camY - l.rad && l.y < camY + LH + l.rad) {
            const float fl = 0.88f + 0.12f * sinf(timeAcc * 9.f + l.x * 0.7f) + 0.04f * sinf(timeAcc * 23.f + l.y);
            ls.push_back({l.x, l.y - 14.f, l.rad * (l.tone == 0 ? fl : 1.f), 1.f, l.tone});
        }
    float pr = 74.f * (p.has(CG_LANTERN) ? 1.6f : 1.f) * (p.nightT > 0 ? 1.4f : 1.f);
    ls.push_back({p.pos.x, p.pos.y - 8.f, pr, 1.f, 2});
    for (const Hazard& h : hazards)
        if (h.fired && h.t < h.delay + 0.25f) ls.push_back({h.pos.x, h.pos.y, h.r * 2.4f, 0.8f, 1});
    for (const MProj& b : mprojs) ls.push_back({b.pos.x, b.pos.y, 34.f, 0.9f, 1});
    for (const Monster& m : mons)  // the glowing eyes and cores
        if (m.state != MN_DEAD && (m.kind == MK_KING || m.kind == MK_MAGE)) ls.push_back({m.pos.x, m.pos.y - 14.f, m.kind == MK_KING ? 70.f : 34.f, 0.8f, 1});
    const float base = frost ? 0.84f : 0.88f;
    static uint32_t px[GW * GH];
    for (int cy = 0; cy < GH; cy++)
        for (int cx = 0; cx < GW; cx++) {
            const float wx = camX + cx * CS + CS / 2, wy = camY + cy * CS + CS / 2;
            float lit = 0.f;
            for (const L& l : ls) {
                const float dx = wx - l.x, dy = (wy - l.y) * 1.25f;
                const float d2 = dx * dx + dy * dy, r2 = l.r * l.r;
                if (d2 >= r2) continue;
                const float t = 1.f - sqrtf(d2) / l.r;
                lit += t * t * (3.f - 2.f * t) * l.k;
            }
            lit = std::min(1.f, lit);
            const int a = (int)(255.f * base * (1.f - lit * 0.97f));
            px[cy * GW + cx] = frost ? (((uint32_t)a << 24) | (4u << 16) | (8u << 8) | 22u) : (((uint32_t)a << 24) | (6u << 16) | (5u << 8) | 12u);
        }
    SDL_UpdateTexture(darkTex, nullptr, px, GW * 4);
    SDL_Rect dst{0, 0, GW * CS, GH * CS};
    SDL_RenderCopy(ren, darkTex, nullptr, &dst);
    // the warm / cold glows of the torches, braziers and crystals on top
    SDL_Texture* g = A.glowWarm.t;
    for (const L& l : ls) {
        if (l.tone == 2) continue;
        const int sz = (int)(l.r * 1.5f);
        SDL_SetTextureColorMod(g, l.tone == 0 ? 255 : 120, l.tone == 0 ? 255 : 190, l.tone == 0 ? 255 : 255);
        SDL_SetTextureAlphaMod(g, (Uint8)(110 * l.k));
        SDL_Rect d{(int)l.x - camX - sz / 2, (int)l.y - camY - sz / 2, sz, sz};
        SDL_RenderCopy(ren, g, nullptr, &d);
    }
    SDL_SetTextureColorMod(g, 255, 255, 255);
    SDL_SetTextureAlphaMod(g, 255);
}

// ------------------------------------------------------------------ HUD pieces
void Game::drawBossBar() {
    if (bossIdx < 0 || bossIdx >= (int)mons.size()) return;
    const Monster& m = mons[bossIdx];
    if (m.state == MN_DEAD || !m.awake) return;
    const int w = 220, x = LW / 2 - w / 2, y = 6;
    panel(x - 4, y - 2, w + 8, 24, HEX(0x1c2a3c));
    textC(LW / 2, y + 1, "THE FROSTBOUND KING", HEX(0xcfeeff), 1, true);
    bar(x, y + 10, w, 7, (float)m.hp / m.maxhp, m.phase == 3 ? HEX(0xe05a5a) : (m.phase == 2 ? HEX(0xe0a03a) : HEX(0x5ab8ff)), HEX(0x14202e));
    fillRect(x + w * 2 / 3, y + 10, 1, 7, HEX(0x0c141e));  // where the phases change
    fillRect(x + w / 3, y + 10, 1, 7, HEX(0x0c141e));
    textC(LW / 2, y + 11, S(std::max(0, m.hp)) + " / " + S(m.maxhp), C_TEXT, 1, true);
}

void Game::drawDelveBanner() {
    if (delveBanT <= 0 || state != ST_PLAY || delve < 0) return;
    const float k = std::min(1.f, (3.5f - delveBanT) * 5.f), out = std::min(1.f, delveBanT * 2.f);
    if (out < 0.35f) return;
    const std::string name = DELVE_NAME[delve];
    std::string sub;
    if (delve == DV_MINE) sub = "DARK - FALLING ROCKS - BRING A PICKAXE";
    else if (delve == DV_FROST3) sub = "THE KING AWAITS BEYOND THE DOOR";
    else sub = "LEVEL " + S(delve - DV_FROST0 + 1) + " OF 4";
    const int w = std::max(textW(name, 2), textW(sub)) + 24, h = 33;
    const int x = LW / 2 - w / 2, y = 14 - (int)((1.f - k) * 40);
    const u32 col = delveIsFrost(delve) ? HEX(0x9fdcff) : HEX(0xe0a060);
    panel(x, y, w, h, HEX(0x2b1d12));
    fillRect(x + 2, y + 2, w - 4, 2, col);
    textC(LW / 2, y + 8, name, col, 2);
    textC(LW / 2, y + 22, sub, C_DIM);
}
