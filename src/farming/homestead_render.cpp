// homestead_render.cpp - drawing the Homestead: ground pieces, y-sorted buildings / crops / animals, roofs, the build ghost, lights, portal effect.
#include <algorithm>
#include <cmath>
#include "core/game.hpp"

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
namespace {
inline bool onScreen(int tx, int ty, int w, int h, int camX, int camY) {
    return (tx + w) * TS >= camX - 8 && tx * TS <= camX + LW + 8 && (ty + h) * TS >= camY - 40 && ty * TS <= camY + LH + 70;
}
}  // namespace

void Game::ensureDomArt() {
    if (A.Dom.built || !ren) return;
    buildDomBank(ren, A.Dom);
    A.Dom.built = true;
}

void Game::drawHomesteadGround() {
    for (auto& pc : hs.pieces) {
        const HPieceDef& D = HPDEF[pc.kind];
        if (!onScreen(pc.tx, pc.ty, pc.w(), pc.h(), camX, camY)) continue;
        if (D.layer == HL_GROUND) {
            const int sx = pc.tx * TS - camX, sy = pc.ty * TS - camY;
            switch (pc.kind) {
                case HP_FLOOR_WOOD: sprS(A.H.floorWood[pc.rot & 1], sx, sy); break;
                case HP_FLOOR_STONE: sprS(A.H.floorStone, sx, sy); break;
                case HP_FOUNDATION: sprS(A.H.foundation, sx, sy); break;
                case HP_BRIDGE: sprS(A.H.bridge[pc.rot & 1], sx, sy); break;
                case HP_PLOT: sprS(A.H.plot[pc.water > 0.3f ? 1 : 0], sx, sy); break;
            }
        } else if (pc.kind == HP_PEN) {  // trampled dirt inside the paddock
            for (int y = pc.ty + 1; y < pc.ty + pc.h() - 1; y++)
                for (int x = pc.tx + 1; x < pc.tx + pc.w() - 1; x++) fillRect(x * TS - camX, y * TS - camY, TS, TS, RGBA(120, 88, 50, 70));
        }
    }
}

void Game::drawHsPiece(const HPiece& pc, int sub, float alpha) {
    const HPieceDef& D = HPDEF[pc.kind];
    const float x0 = pc.tx * (float)TS, y0 = pc.ty * (float)TS, w = pc.w() * (float)TS, h = pc.h() * (float)TS;
    auto fenceLike = [&](int x, int y) {
        const HPiece* q = hsAt(x, y, HL_OBJECT);
        return q && (q->kind == HP_FENCE || q->kind == HP_GATE || q->kind == HP_PEN);
    };
    auto isWall = [&](int x, int y) {
        const HPiece* q = hsAt(x, y, HL_OBJECT);
        return q && q->kind == pc.kind;
    };
    auto vertical = [&](int x, int y) {
        const bool ew = fenceLike(x - 1, y) || fenceLike(x + 1, y), ns = fenceLike(x, y - 1) || fenceLike(x, y + 1);
        return ew ? false : (ns ? true : (pc.rot & 1) != 0);
    };
    switch (pc.kind) {
        case HP_WALL_WOOD: case HP_WALL_STONE: {
            const int m = pc.kind == HP_WALL_STONE ? 1 : 0;
            sprW(A.shadow[0], x0 + 9, y0 + 15, false, 0.5f * alpha);
            sprW(A.H.wall[m][isWall(pc.tx, pc.ty - 1) ? 1 : 0], x0 + 8, y0 + 16, false, alpha);
            break;
        }
        case HP_FENCE: sprW(A.H.fence[vertical(pc.tx, pc.ty) ? 1 : 0], x0 + 8, y0 + 16, false, alpha); break;
        case HP_GATE: sprW(A.H.gate[vertical(pc.tx, pc.ty) ? 1 : 0][pc.open ? 1 : 0], x0 + 8, y0 + 16, false, alpha); break;
        case HP_DOOR: sprW(A.H.door[pc.open ? 1 : 0], x0 + 8, y0 + 16, false, alpha); break;
        case HP_PEN: {
            auto ring = [&](int x, int y) {
                if (x == pc.tx + (pc.w() - 1) / 2 && y == pc.ty + pc.h() - 1) return;  // the gap
                const bool vert = (x == pc.tx || x == pc.tx + pc.w() - 1) && y > pc.ty && y < pc.ty + pc.h() - 1;
                sprW(A.H.fence[vert ? 1 : 0], x * (float)TS + 8, y * (float)TS + 16, false, alpha);
            };
            if (sub > 0) ring((sub - 1) % MW, (sub - 1) / MW);
            else if (sub < 0)
                for (int y = pc.ty; y < pc.ty + pc.h(); y++)
                    for (int x = pc.tx; x < pc.tx + pc.w(); x++)
                        if (x == pc.tx || y == pc.ty || x == pc.tx + pc.w() - 1 || y == pc.ty + pc.h() - 1) ring(x, y);
            break;
        }
        case HP_SHED: case HP_HOUSE: case HP_BARN: case HP_COOP: case HP_WORKSHOP: case HP_WAREHOUSE:
            sprW(A.shadow[2], x0 + w / 2 + 6, y0 + h - 1, false, 0.55f * alpha);
            sprW(A.H.building[pc.kind][pc.rot & 1], x0 + w / 2, y0 + h, false, alpha);
            if (pc.kind == HP_WORKSHOP && alpha >= 1.f) sprW(A.fire[(int)(timeAcc * 9) % 3], x0 + w - 10, y0 + h - 4);
            break;
        case HP_GREENHOUSE: sprW(A.H.greenhouse[pc.rot & 1], x0 + 1, y0 + h, false, alpha); break;
        case HP_WELL: sprW(A.shadow[2], x0 + 18, y0 + h - 1, false, 0.5f * alpha); sprW(A.H.well, x0 + 16, y0 + h, false, alpha); break;
        case HP_CAMPFIRE: sprW(A.shadow[0], x0 + 8, y0 + 14, false, 0.6f * alpha); sprW(A.R.campfire[(int)(timeAcc * 9 + pc.id) % 3], x0 + 8, y0 + 14, false, alpha); break;
        case HP_BENCH: sprW(A.shadow[0], x0 + w / 2, y0 + 14, false, 0.6f * alpha); sprW(A.T.bench, x0 + w / 2, y0 + 14, false, alpha); break;
        case HP_PLANTER: sprW(A.shadow[0], x0 + 8, y0 + 14, false, 0.6f * alpha); sprW(A.T.planter, x0 + 8, y0 + 14, false, alpha); break;
        case HP_BARREL: sprW(A.shadow[0], x0 + 8, y0 + 14, false, 0.7f * alpha); sprW(A.barrel, x0 + 8, y0 + 14, false, alpha); break;
        case HP_SCARECROW: sprW(A.shadow[0], x0 + 8, y0 + 14, false, 0.6f * alpha); sprW(A.H.scarecrow, x0 + 8, y0 + 15, false, alpha); break;
        case HP_SIGN: sprW(A.sign, x0 + 8, y0 + 14, false, alpha); break;
        case HP_LAMP: sprW(A.T.lamp, x0 + 8, y0 + 14, false, alpha); break;
        case HP_TORCH: sprW(A.D.torch[((int)(timeAcc * 8.f + pc.id)) % 3], x0 + 8, y0 + 14, false, alpha); break;
        case HP_ROOF: sprW(A.H.roof, x0, y0, false, alpha); break;
        default: break;
    }
    (void)D;
}

void Game::drawHsCrop(const HPiece& pc) {
    if (pc.crop < 0 || pc.crop >= CR_COUNT) return;
    const float x = pc.tx * (float)TS + 8, y = pc.ty * (float)TS + 14;
    sprW(A.H.crop[pc.crop][std::max(0, std::min(CROP_STAGES - 1, pc.stage))], x, y, false, 1.f, sinf(timeAcc * 1.7f + pc.id) * (pc.stage >= 2 ? 1.5f : 0.f), true);
    if (pc.stage >= CROP_STAGES - 1 && fmodf(timeAcc * 0.9f + pc.id * 0.37f, 2.4f) < 0.2f)  // ripe: a glint
        fillRect((int)x - camX + 4, (int)y - camY - 12, 2, 2, RGBA(255, 240, 150, 230));
}

void Game::drawHsAnimal(const HAnimal& a) {
    ensureDomArt();
    const DomDef& D = DOMS[a.kind];
    const bool adult = a.adult(D);
    const int st = a.state == 1 ? DS_WALK : a.state == 2 ? DS_EAT : DS_IDLE;
    const int fr = (int)(a.animT * (st == DS_WALK ? 6.f : 2.f)) % DS_FRAMES[st];
    Spr s = A.Dom.get(a.kind, a.face8, st, fr);
    if (!s.ok()) return;
    if (!adult) s.sc *= 0.55f + 0.45f * std::min(1.f, a.age / D.growSec);
    sprW(A.shadow[a.kind == DA_COW ? 2 : (D.small ? 0 : 1)], a.px, a.py + 1, false, 0.6f);
    sprW(s, a.px, a.py, false, 1.f);
    if (a.stock > 0) icon(A.matIcon[D.product], (int)a.px - camX, (int)a.py - camY - (int)(26 * s.sc) - 2 + (int)(sinf(timeAcc * 4.f) * 1.5f), 10);
}

void Game::drawHsRoofs() {
    if (!inHomestead()) return;
    const int ptx = (int)floorf(p.pos.x / TS), pty = (int)floorf(p.pos.y / TS);
    const bool under = hsAt(ptx, pty, HL_ROOF) != nullptr;
    for (auto& pc : hs.pieces) {
        if (pc.kind != HP_ROOF || !onScreen(pc.tx, pc.ty, 1, 1, camX, camY)) continue;
        float al = 0.95f;
        if (under && abs(pc.tx - ptx) <= 6 && abs(pc.ty - pty) <= 5) al = 0.28f;
        drawHsPiece(pc, 0, al);
    }
}

void Game::drawHsOverlay() {
    if (!inHomestead() || !bm.on) return;
    // a faint grid
    for (int x = camX / TS; x <= (camX + LW) / TS + 1; x++) fillRect(x * TS - camX, 0, 1, LH, RGBA(255, 255, 255, 14));
    for (int y = camY / TS; y <= (camY + LH) / TS + 1; y++) fillRect(0, y * TS - camY, LW, 1, RGBA(255, 255, 255, 14));
    if (bm.demolish) {
        HPiece* pc = hsAt(bm.tx, bm.ty, HL_OBJECT);
        if (!pc) pc = hsAt(bm.tx, bm.ty, HL_ROOF);
        if (!pc) pc = hsAt(bm.tx, bm.ty, HL_GROUND);
        const int x = pc ? pc->tx : bm.tx, y = pc ? pc->ty : bm.ty, w = pc ? pc->w() : 1, h = pc ? pc->h() : 1;
        fillRect(x * TS - camX, y * TS - camY, w * TS, h * TS, RGBA(230, 60, 50, 90));
        return;
    }
    HPiece g;
    g.kind = bm.kind;
    g.tx = bm.tx;
    g.ty = bm.ty;
    g.rot = bm.rot & 1;
    const u32 col = bm.valid ? RGBA(70, 220, 90, 90) : RGBA(230, 60, 50, 100);
    const HPieceDef& D = HPDEF[g.kind];
    if (D.layer == HL_GROUND || g.kind == HP_ROOF) fillRect(g.tx * TS - camX, g.ty * TS - camY, g.w() * TS, g.h() * TS, col);
    drawHsPiece(g, g.kind == HP_PEN ? -1 : 0, 0.6f);
    if (D.layer != HL_GROUND && g.kind != HP_ROOF) fillRect(g.tx * TS - camX, g.ty * TS - camY, g.w() * TS, g.h() * TS, col);
}

void Game::drawHsLights(float night) {
    SDL_Texture* g = A.glowWarm.t;
    const float strength = 0.3f + 0.7f * night;
    if (world.hportal.x > -9999.f && world.hportal.x > camX - 80 && world.hportal.x < camX + LW + 80) {  // the portal always glows
        const float pul = 0.8f + 0.2f * sinf(timeAcc * 2.f);
        SDL_SetTextureAlphaMod(A.glowCool.t, (Uint8)(255 * clampf(0.5f + 0.5f * night, 0, 1) * pul));
        const int sz = (int)(120 * pul);
        SDL_Rect d{(int)world.hportal.x - camX - sz / 2, (int)world.hportal.y - 26 - camY - sz / 2, sz, sz};
        SDL_RenderCopy(ren, A.glowCool.t, nullptr, &d);
        SDL_SetTextureAlphaMod(A.glowCool.t, 255);
    }
    if (!inHomestead()) return;
    for (auto& pc : hs.pieces) {
        const HPieceDef& D = HPDEF[pc.kind];
        if (D.light <= 0 || !onScreen(pc.tx, pc.ty, pc.w(), pc.h(), camX, camY)) continue;
        const float fl = 0.85f + 0.15f * sinf(timeAcc * 10.f + pc.id) + 0.05f * sinf(timeAcc * 23.f + pc.id);
        const float k = (pc.kind == HP_HOUSE || pc.kind == HP_LAMP) ? 0.25f + 0.75f * night : strength;
        SDL_SetTextureAlphaMod(g, (Uint8)(255 * clampf(k * fl, 0, 1)));
        const int sz = (int)(D.light * 2.f * fl);
        const float cx = pc.tx * (float)TS + pc.w() * 8.f, cy = pc.ty * (float)TS + pc.h() * 8.f - (pc.kind == HP_LAMP ? 14.f : 2.f);
        SDL_Rect d{(int)cx - camX - sz / 2, (int)cy - camY - sz / 2, sz, sz};
        SDL_RenderCopy(ren, g, nullptr, &d);
    }
    SDL_SetTextureAlphaMod(g, 255);
}

void Game::drawPortalFx() {
    if (portalFx <= 0) return;
    const float a = clampf(portalFx / 1.4f, 0, 1);
    fillRect(0, 0, LW, LH, RGBA(200, 180, 255, (int)(255 * a * a * a)));
    const int cx = (int)p.pos.x - camX, cy = (int)p.pos.y - 10 - camY;
    for (int r = 0; r < 3; r++) {
        const float rad = (1.f - a) * 90.f + r * 14.f;
        const int al = (int)(220 * a * (1.f - r * 0.25f));
        for (int i = 0; i < 72; i++) {
            const float t = i * 6.2832f / 72.f + timeAcc * (r % 2 ? 1.f : -1.f);
            fillRect(cx + (int)(cosf(t) * rad), cy + (int)(sinf(t) * rad * 0.7f), 2, 2, RGBA(170 + r * 25, 150, 255, al));
        }
    }
}

void Game::drawHsHud() {
    if (!inHomestead()) return;
    if (bestTool(TT_CAN, nullptr) > 0) {  // the watering can's water
        panel(4, 46, 60, 12, HEX(0x33241a));
        icon(itemIcon(itemTool(TL_CAN)), 11, 52, 10);
        bar(18, 49, 42, 6, hs.can / CAN_CAPACITY, HEX(0x4a90d8));
    }
    if (bm.on) {
        const u32 col = bm.demolish ? C_RED : C_GOLD;
        panel(LW / 2 - 150, LH - 34, 300, 30, HEX(0x33241a));
        if (bm.demolish) text(LW / 2 - 144, LH - 30, "DEMOLISH - CLICK A PIECE (60% BACK)", col);
        else {
            const HPieceDef& D = HPDEF[bm.kind];
            text(LW / 2 - 144, LH - 30, std::string(D.name) + "  " + S((bm.rot & 1) ? D.h : D.w) + "X" + S((bm.rot & 1) ? D.w : D.h), col);
            if (!bm.valid && !bm.why.empty()) text(LW / 2 - 144, LH - 22, bm.why, C_RED);
        }
        textC(LW / 2, LH - 13, "CLICK PLACE  R ROTATE  X DEMOLISH  B MENU  ESC EXIT", C_DIM, 1, false);
    }
}
