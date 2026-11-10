// game_render.cpp - 2D drawing primitives, title / pause menus and the frame render / present.
#include <cstdio>
#include "core/game.hpp"
#include "core/logic_util.hpp"

// ================================================================= 2D primitives
void Game::fillRect(int x, int y, int w, int h, u32 c) {
    SDL_SetRenderDrawBlendMode(ren, cA(c) == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, cR(c), cG(c), cB(c), cA(c));
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(ren, &r);
}

void Game::sprS(const Spr& s, int sx, int sy, bool flip, float alpha, double angle, bool pivot) {
    if (!s.t) return;
    int ax = flip ? s.w - 1 - s.ax : s.ax;
    const int dax = (int)lroundf(ax * s.sc), day = (int)lroundf(s.ay * s.sc);
    SDL_Rect d{sx - dax, sy - day, (int)lroundf(s.w * s.sc), (int)lroundf(s.h * s.sc)};
    SDL_Point c{dax, day};
    SDL_SetTextureAlphaMod(s.t, (Uint8)(clampf(alpha, 0, 1) * 255));
    SDL_RenderCopyEx(ren, s.t, nullptr, &d, angle, pivot ? &c : nullptr, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(s.t, 255);
}

void Game::icon(const Spr& s, int cx, int cy, int size, float alpha) {
    if (!s.t) return;
    SDL_Rect d{cx - size / 2, cy - size / 2, size, size};
    SDL_SetTextureAlphaMod(s.t, (Uint8)(clampf(alpha, 0, 1) * 255));
    SDL_RenderCopy(ren, s.t, nullptr, &d);
    SDL_SetTextureAlphaMod(s.t, 255);
}

void Game::text(int x, int y, const std::string& s, u32 col, int sc, bool shadow, float alpha) {
    int cx = x;
    int so = std::max(1, sc / 2);
    for (char ch : s) {
        int idx = glyphIndex(ch);
        SDL_Rect src{idx * 4, 0, 3, 5};
        if (shadow) {
            SDL_Rect dd{cx + so, y + so, 3 * sc, 5 * sc};
            SDL_SetTextureColorMod(A.font, 20, 12, 6);
            SDL_SetTextureAlphaMod(A.font, (Uint8)(alpha * 230));
            SDL_RenderCopy(ren, A.font, &src, &dd);
        }
        SDL_Rect d{cx, y, 3 * sc, 5 * sc};
        SDL_SetTextureColorMod(A.font, cR(col), cG(col), cB(col));
        SDL_SetTextureAlphaMod(A.font, (Uint8)(alpha * 255));
        SDL_RenderCopy(ren, A.font, &src, &d);
        cx += 4 * sc;
    }
    SDL_SetTextureColorMod(A.font, 255, 255, 255);
    SDL_SetTextureAlphaMod(A.font, 255);
}
void Game::textC(int cx, int y, const std::string& s, u32 col, int sc, bool shadow) { text(cx - textW(s, sc) / 2, y, s, col, sc, shadow); }
void Game::textOutlined(int x, int y, const std::string& s, u32 col, float alpha) {
    int cx = x;
    for (char ch : s) {
        int idx = glyphIndex(ch);
        SDL_Rect src{idx * 4, 0, 3, 5};
        SDL_SetTextureColorMod(A.font, 20, 12, 6);
        SDL_SetTextureAlphaMod(A.font, (Uint8)(alpha * 255));
        const int off[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (auto& o : off) {
            SDL_Rect d{cx + o[0], y + o[1], 3, 5};
            SDL_RenderCopy(ren, A.font, &src, &d);
        }
        SDL_Rect d{cx, y, 3, 5};
        SDL_SetTextureColorMod(A.font, cR(col), cG(col), cB(col));
        SDL_RenderCopy(ren, A.font, &src, &d);
        cx += 4;
    }
    SDL_SetTextureColorMod(A.font, 255, 255, 255);
    SDL_SetTextureAlphaMod(A.font, 255);
}

void Game::panel(int x, int y, int w, int h, u32 fill) {
    fillRect(x + 1, y, w - 2, h, UI_BORDER);
    fillRect(x, y + 1, w, h - 2, UI_BORDER);
    fillRect(x + 1, y + 1, w - 2, h - 2, fill);
    fillRect(x + 1, y + 1, w - 2, 1, shadeC(fill, 1.35f));
    fillRect(x + 1, y + 1, 1, h - 2, shadeC(fill, 1.2f));
    fillRect(x + 1, y + h - 2, w - 2, 1, shadeC(fill, 0.7f));
    fillRect(x + w - 2, y + 1, 1, h - 2, shadeC(fill, 0.8f));
}

void Game::bar(int x, int y, int w, int h, float f, u32 fill, u32 back) {
    fillRect(x - 1, y - 1, w + 2, h + 2, UI_BORDER);
    fillRect(x, y, w, h, back);
    int fw = (int)(w * clampf(f, 0, 1));
    if (fw > 0) {
        fillRect(x, y, fw, h, fill);
        fillRect(x, y, fw, 1, shadeC(fill, 1.35f));
        if (h > 2) fillRect(x, y + h - 1, fw, 1, shadeC(fill, 0.7f));
    }
}

bool Game::button(int x, int y, int w, int h, const std::string& label, bool enabled, u32 base) {
    bool hov = enabled && inRect(x, y, w, h);
    bool down = hov && in.l;
    panel(x, y + (down ? 1 : 0), w, h, !enabled ? UI_BTN_OFF : hov ? UI_BTN_HOV : base);
    textC(x + w / 2, y + (h - 5) / 2 + (down ? 1 : 0), label, enabled ? C_TEXT : HEX(0x6e5f4a));
    if (hov && in.uiClick) {
        in.uiClick = false;   // only one button may react to a click
        in.lPressed = false;  // don't also start a weapon throw
        audio.play(S_CLICK);
        return true;
    }
    return false;
}

int Game::chip(int x, int y, const Spr& ic, int have, int need) {
    icon(ic, x + 6, y + 5, 12);
    std::string t = S(have) + "/" + S(need);
    text(x + 14, y + 3, t, have >= need ? C_GREEN : C_RED);
    return x + 14 + textW(t) + 6;
}

// ================================================================= menus
void Game::drawTitle() {
    fillRect(0, 0, LW, LH, RGBA(10, 8, 20, 70));
    fillRect(0, 0, LW, 70, RGBA(10, 8, 20, 60));
    int bob = (int)(sinf(titleT * 1.6f) * 2);
    textC(LW / 2, 22 + bob, "WILDWOOD", HEX(0xf6d96a), 6);
    textC(LW / 2, 56 + bob, "HUNTER", HEX(0xf3e9d2), 6);
    textC(LW / 2, 92, "A TOP-DOWN HUNTING ADVENTURE", C_TEXT);
    int bx = LW / 2 - 55, by = 116;
    if (button(bx, by, 110, 20, "NEW GAME", true, HEX(0x5d7a2e))) {
        newGame();
        state = ST_PLAY;
    }
    by += 25;
    if (button(bx, by, 110, 20, "CONTINUE", hasSave)) {
        if (load()) state = ST_PLAY;
    }
    by += 25;
    if (button(bx, by, 110, 20, "QUIT", true, HEX(0x7a3a2a))) running = false;
    textC(LW / 2, LH - 22, "HUNT  GATHER  CRAFT  TRADE", C_GOLD, 1);
    textC(LW / 2, LH - 12, "WASD MOVE   MOUSE AIM   LMB THROW   E INTERACT", C_DIM, 1);
}

void Game::drawPause() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 150));
    panel(LW / 2 - 90, 40, 180, 140, HEX(0x4a3524));
    textC(LW / 2, 50, "PAUSED", C_GOLD, 3);
    int bx = LW / 2 - 60, by = 80;
    if (button(bx, by, 120, 18, "RESUME", true, HEX(0x5d7a2e))) state = ST_PLAY;
    by += 23;
    if (button(bx, by, 120, 18, audio.muted ? "SOUND: OFF" : "SOUND: ON")) audio.muted = !audio.muted;
    by += 23;
    if (button(bx, by, 120, 18, "SAVE AND MAIN MENU", true, HEX(0x7a3a2a))) {
        save();
        state = ST_TITLE;
    }
    textC(LW / 2, by + 28, "KILLS " + S(kills) + "    COINS EARNED " + S(sold), C_DIM, 1, false);
    textC(LW / 2, by + 38, "F11: FULLSCREEN", C_DIM, 1, false);
}

void Game::render() {
    SDL_SetRenderTarget(ren, canvas);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    renderWorld(true);
    switch (state) {
        case ST_TITLE: drawTitle(); break;
        case ST_PLAY: drawHud(); drawHsHud(); drawPortalFx(); break;
        case ST_BUILD: drawHud(); drawBuildMenu(); break;
        case ST_FARM: drawHud(); drawFarmMenu(); break;
        case ST_ANIMAL: drawHud(); drawAnimalMenu(); break;
        case ST_MARKET: drawHud(); drawMarket(); break;
        case ST_SHOP: drawHud(); drawShop(); break;
        case ST_SERVICE: drawHud(); drawService(); break;
        case ST_PAUSE: drawHud(); drawPause(); break;
        case ST_QUEST: drawHud(); drawQuestPanel(); break;
        case ST_INVENTORY: drawHud(); drawInventory(); break;
        case ST_CHEST: drawHud(); drawChest(); break;
    }
    if (state == ST_PLAY && !p.dead) drawReticle();
    else sprS(A.cursor, (int)in.mx, (int)in.my);
    if (travelFade > 0) fillRect(0, 0, LW, LH, RGBA(0, 0, 0, (int)(255.f * std::min(1.f, travelFade / 0.6f))));  // fade in after a trip
}

void Game::present() {
    SDL_SetRenderTarget(ren, nullptr);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    int ww, wh;
    SDL_GetRendererOutputSize(ren, &ww, &wh);
    float sc = std::min((float)ww / LW, (float)wh / LH);
    if (sc >= 1.f) sc = floorf(sc);
    int dw = (int)(LW * sc), dh = (int)(LH * sc);
    offX = (float)((ww - dw) / 2);
    offY = (float)((wh - dh) / 2);
    presScale = sc;
    SDL_Rect dst{(int)offX, (int)offY, dw, dh};
    SDL_RenderCopy(ren, canvas, nullptr, &dst);
    SDL_RenderPresent(ren);
}

void Game::saveCanvas(const char* path) {
    SDL_SetRenderTarget(ren, canvas);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, LW, LH, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
}
