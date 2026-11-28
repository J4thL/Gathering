// game_render.cpp - 2D drawing primitives, title / pause menus and the frame render / present.
#include <cstdio>
#include "core/game.hpp"
#include "core/logic_util.hpp"
#include "devui/dev_ui.hpp"

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

float Game::windLean(const Spr& s) const {
    // downwind (windDir.x: the lean shows sideways only), harder as the wind blows, and more in a gust; proportional to the height of the sprite
    return windDir.x * clampf(wind, 0.f, 2.f) * (0.5f + windGust) * s.ay * s.sc * 0.045f;
}

void Game::sprWLean(const Spr& s, float wx, float wy, float leanPx, bool flip) {
    if (!s.t) return;
    if (fabsf(leanPx) < 0.5f) {
        sprW(s, wx, wy, flip);
        return;
    }
    const int dax = flip ? (int)lroundf((s.w - 1 - s.ax) * s.sc) : (int)lroundf(s.ax * s.sc), day = (int)lroundf(s.ay * s.sc);
    const int X = (int)floorf(wx) - camX - dax, Y = (int)floorf(wy) - camY - day;
    const int dw = std::max(1, (int)lroundf(s.w * s.sc));
    constexpr int STRIP = 3;
    for (int y0 = 0; y0 < s.h; y0 += STRIP) {
        const int sh = std::min(STRIP, s.h - y0);
        const float above = std::max(0.f, s.ay - (y0 + sh * 0.5f)) / std::max(1.f, (float)s.ay);  // 1 at the top of the sprite, 0 at its foot
        SDL_Rect src{0, y0, s.w, sh};
        const int ya = (int)lroundf(y0 * s.sc), yb = (int)lroundf((y0 + sh) * s.sc);
        SDL_Rect dst{X + (int)lroundf(leanPx * above), Y + ya, dw, std::max(1, yb - ya)};
        SDL_RenderCopyEx(ren, s.t, &src, &dst, 0, nullptr, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
}

void Game::sprWSquash(const Spr& s, float wx, float wy, float sx, float sy, double angle, float alpha) {
    if (!s.t) return;
    const int dax = (int)lroundf(s.ax * s.sc), day = (int)lroundf(s.ay * s.sc);
    const int dw = std::max(1, (int)lroundf(s.w * s.sc * sx)), dh = std::max(1, (int)lroundf(s.h * s.sc * sy));
    const int fx = (int)lroundf(dax * sx), fy = (int)lroundf(day * sy);  // the foot stays where it was
    SDL_Rect d{(int)floorf(wx) - camX - fx, (int)floorf(wy) - camY - fy, dw, dh};
    SDL_Point c{fx, fy};
    SDL_SetTextureAlphaMod(s.t, (Uint8)(clampf(alpha, 0, 1) * 255));
    SDL_RenderCopyEx(ren, s.t, nullptr, &d, angle, &c, SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(s.t, 255);
}

void Game::icon(const Spr& s, int cx, int cy, int size, float alpha) {
    if (!s.t) return;
    SDL_Rect d{cx - size / 2, cy - size / 2, size, size};
    SDL_SetTextureAlphaMod(s.t, (Uint8)(clampf(alpha, 0, 1) * 255));
    SDL_RenderCopy(ren, s.t, nullptr, &d);
    SDL_SetTextureAlphaMod(s.t, 255);
}

// ---- the pack skin: dark panel fills become tan insets or copper frames, light text turns dark brown
static int lumaOf(u32 c) { return (cR(c) * 3 + cG(c) * 6 + cB(c)) / 10; }
u32 Game::skinFill(u32 c) const {
    if (!packSkin) return c;
    const int l = lumaOf(c);
    return l < 48 ? HEX(0xd9d5ca) : l < 70 ? HEX(0xa5a094) : l < 100 ? HEX(0xeeeae0) : c;  // inset / frame / highlight; brighter colours are already skin colours
}
static u32 skinText(u32 c) {
    const int l = lumaOf(c), f = l > 180 ? 28 : 50;  // pale text gets very dark, coloured text a little less
    return HEX((u32)(((cR(c) * f / 100) << 16) | ((cG(c) * f / 100) << 8) | (cB(c) * f / 100)));
}

void Game::text(int x, int y, const std::string& s, u32 col, int sc, bool shadow, float alpha) {
    if (packSkin) { col = skinText(col); shadow = false; }
    int cx = x;
    int so = std::max(1, sc / 2);
    for (char ch : s) {
        int idx = glyphIndex(ch);
        SDL_Rect src{idx * 4 * LS, 0, 3 * LS, 5 * LS};  // the font atlas is stored at LS x resolution
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
        SDL_Rect src{idx * 4 * LS, 0, 3 * LS, 5 * LS};  // the font atlas is stored at LS x resolution
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

void Game::qualityFrame(int x, int y, int w, int h, int q) {
    const u32 c = qualityCol(q);
    fillRect(x, y, w, 1, c);
    fillRect(x, y + h - 1, w, 1, c);
    fillRect(x, y + 1, 1, h - 2, c);
    fillRect(x + w - 1, y + 1, 1, h - 2, c);
}

void Game::panel(int x, int y, int w, int h, u32 fill) {
    const u32 UI_BORDER = packSkin ? HEX(0x6b665c) : ::UI_BORDER;
    fill = skinFill(fill);
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
    if (packSkin) {  // tan buttons, tinted by the colour they would have had, lighter under the mouse
        const u32 tint = base == HEX(0x8a6a3a) ? HEX(0xf6f3ea) : base == UI_BTN ? HEX(0xd9d5ca) : HEX(((cR(base) + 0xe0) / 2 << 16) | ((cG(base) + 0xa8) / 2 << 8) | ((cB(base) + 0x78) / 2));
        const u32 fill = !enabled ? HEX(0xb08a68) : hov ? HEX(0xf8dcae) : tint;
        panel(x, y + (down ? 1 : 0), w, h, fill);
        textC(x + w / 2, y + (h - 5) / 2 + (down ? 1 : 0), label, enabled ? C_TEXT : HEX(0x9a7a5a));
    } else {
        panel(x, y + (down ? 1 : 0), w, h, !enabled ? UI_BTN_OFF : hov ? UI_BTN_HOV : base);
        textC(x + w / 2, y + (h - 5) / 2 + (down ? 1 : 0), label, enabled ? C_TEXT : HEX(0x6e5f4a));
    }
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
    // a vignette: the top and bottom edges darken a little, so the logo and the hints stand out from the moving world behind
    for (int i = 0; i < 10; i++) {
        const int a = (10 - i) * 8;
        fillRect(0, i * 3, LW, 3, RGBA(8, 6, 16, a));
        fillRect(0, LH - (i + 1) * 3, LW, 3, RGBA(8, 6, 16, a));
    }
    // drifting motes of light rising through the scene
    for (int i = 0; i < 26; i++) {
        const float sp = 5.f + (i % 5) * 2.5f, rise = 9.f + (i % 7) * 2.2f;
        const float x = fmodf(i * 47.f + titleT * sp, LW + 20.f) - 10.f + sinf(titleT * 0.9f + i) * 6.f;
        const float y = LH + 6.f - fmodf(i * 31.f + titleT * rise, LH + 12.f);
        const float tw = 0.5f + 0.5f * sinf(titleT * 2.1f + i * 1.7f);
        fillRect((int)x, (int)y, i % 6 == 0 ? 2 : 1, i % 6 == 0 ? 2 : 1, RGBA(255, 226, 140, (int)(40 + 110 * tw)));
    }
    int bob = (int)(sinf(titleT * 1.6f) * 2);
    textC(LW / 2, 22 + bob, "WILDWOOD", mixc(HEX(0xf6d96a), HEX(0xfff6c0), 0.5f + 0.5f * sinf(titleT * 1.3f)), 6);  // (the gold slowly breathes)
    textC(LW / 2, 56 + bob, "HUNTER", HEX(0xf3e9d2), 6);
    textC(LW / 2, 92, "A TOP-DOWN HUNTING ADVENTURE", C_TEXT);
    if (shapeWait > 0) {  // the world is being built (a moment: every new world is checked and repaired first)
        panel(LW / 2 - 90, 90, 180, 46, HEX(0x4a3524));
        textC(LW / 2, 98, "SHAPING THE WORLD...", C_GOLD, 2);
        textC(LW / 2, 120, "CHECKING THE PATHS AND TERRAIN", C_DIM, 1, false);
        return;
    }
    if (newGameAsk) {  // a saved game exists: say what a new game does before doing it
        panel(LW / 2 - 110, 74, 220, 86, HEX(0x4a3524));
        textC(LW / 2, 82, "START A NEW GAME?", C_GOLD, 2);
        textC(LW / 2, 104, "YOU HAVE A SAVED GAME. A NEW GAME BUILDS A NEW", C_TEXT, 1, false);
        textC(LW / 2, 114, "WORLD AND REPLACES THE SAVE WHEN YOU SAVE OR QUIT.", C_TEXT, 1, false);
        if (button(LW / 2 - 100, 128, 96, 18, "YES, NEW GAME", true, HEX(0x7a3a2a))) {
            newGameAsk = false;
            beginNewGame();
        }
        if (button(LW / 2 + 4, 128, 96, 18, "CANCEL", true, HEX(0x5d7a2e))) newGameAsk = false;
        textC(LW / 2, 150, "ENTER: YES    ESC: CANCEL", C_DIM, 1, false);
        return;
    }
    int bx = LW / 2 - 55, by = 110;
    // the menu works with the keyboard (up / down / Enter) and the mouse; the mouse only takes the highlight when it moves
    static float lastMx = -1.f, lastMy = -1.f;
    const bool mouseMoved = in.mx != lastMx || in.my != lastMy;
    lastMx = in.mx;
    lastMy = in.my;
    auto item = [&](int idx, int y, const char* label, bool enabled, u32 col) {
        if (enabled && mouseMoved && inRect(bx, y, 110, 20)) menuSel = idx;
        const bool hit = button(bx, y, 110, 20, label, enabled, col);
        if (menuSel == idx && enabled) {
            const int nudge = (int)(sinf(titleT * 5.f) * 1.5f);
            text(bx - 9 - nudge, y + 7, ">", C_GOLD);
            text(bx + 113 + nudge, y + 7, "<", C_GOLD);
        }
        return hit;
    };
    if (item(0, by, "NEW GAME", true, HEX(0x5d7a2e))) requestNewGame();
    by += 22;
    if (item(1, by, "CONTINUE", hasSave, UI_BTN)) {
        if (load()) state = ST_PLAY;
    }
    by += 22;
    if (item(2, by, "SETTINGS", true, UI_BTN)) openSettings();
    by += 22;
    if (item(3, by, "QUIT", true, HEX(0x7a3a2a))) running = false;
    if (hasSave && saveInfo.ok) {  // what is on the save: so a player can tell their games apart before loading one
        const int cx = bx + 122, cy = 104, cw = 128, ch = 66;
        panel(cx, cy, cw, ch, HEX(0x4a3524));
        fillRect(cx + 3, cy + 3, cw - 6, 11, UI_BG);
        text(cx + 7, cy + 6, "SAVED GAME", C_GOLD);
        const BiomeDef& bd = BIOME[saveInfo.biome];
        fillRect(cx + 6, cy + 18, 3, 9, bd.col);
        text(cx + 12, cy + 17, "DAY " + S(saveInfo.day) + "  " + SEASON_NAME[saveInfo.season], C_TEXT);
        text(cx + 12, cy + 27, bd.name, bd.col);
        text(cx + 7, cy + 40, "KILLS " + S(saveInfo.kills), C_TEXT);
        text(cx + 7, cy + 51, "COINS " + S(saveInfo.coins), C_GOLD);
        text(cx + 68, cy + 40, "MAPS " + S(saveInfo.maps), C_DIM);
        text(cx + 68, cy + 51, "GATES " + S(saveInfo.gates), C_DIM);
    }
    textC(LW / 2, LH - 22, "HUNT  GATHER  CRAFT  TRADE", C_GOLD, 1);
    text(LW - 78, LH - 12, "F11 FULLSCREEN", C_DIM, 1, false);
    textC(LW / 2, LH - 12, "MOVE " + keyName(K_UP) + keyName(K_LEFT) + keyName(K_DOWN) + keyName(K_RIGHT) + "   MOUSE AIM   LMB THROW   " + keyName(K_USE) + " INTERACT", C_DIM, 1);
}

void Game::drawPause() {
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 150));
    panel(LW / 2 - 90, 34, 180, 156, HEX(0x4a3524));
    textC(LW / 2, 44, "PAUSED", C_GOLD, 3);
    int bx = LW / 2 - 60, by = 68;
    if (button(bx, by, 120, 17, "RESUME", true, HEX(0x5d7a2e))) state = ST_PLAY;
    by += 21;
    if (button(bx, by, 120, 17, audio.muted ? "SOUND: OFF" : "SOUND: ON")) {
        audio.muted = !audio.muted;
        saveSettings();
    }
    by += 21;
    if (button(bx, by, 120, 17, "SETTINGS")) openSettings();
    by += 21;
    if (button(bx, by, 120, 17, "HELP")) openHelp();
    by += 21;
    if (button(bx, by, 120, 17, "SAVE AND MAIN MENU", true, HEX(0x7a3a2a))) {
        save();
        state = ST_TITLE;
    }
    textC(LW / 2, by + 24, "KILLS " + S(kills) + "    COINS EARNED " + S(sold), C_DIM, 1, false);
    textC(LW / 2, by + 34, "F11: FULLSCREEN     WORLD " + std::to_string(worldSeed), C_DIM, 1, false);
}

// draw into the canvas in logical coordinates: with hi-res on, the canvas holds LS x LS real pixels for each logical one
void Game::targetCanvas() {
    SDL_SetRenderTarget(ren, canvas);  // (switching to a texture target resets SDL's scale to 1)
    SDL_RenderSetScale(ren, (float)LS, (float)LS);
}

void Game::render() {
    targetCanvas();  // (not a bare SDL_SetRenderTarget: that leaves the scale at 1 and hi-res draws into the top-left quarter)
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    renderWorld(true);
    if (debugAI) drawAIDebug();
    switch (state) {
        case ST_TITLE: drawTitle(); break;
        case ST_PLAY: drawHud(); drawHsHud(); drawPortalFx(); break;
        case ST_BUILD: drawHud(); drawBuildMenu(); break;
        case ST_ANIMAL: drawHud(); drawAnimalMenu(); break;
        case ST_MACHINE: drawHud(); drawMachineMenu(); break;
        case ST_SHOP: {
            drawHud();
            const bool modal = amtCtx != AMT_NONE, clk = in.uiClick;  // the sell slider takes every click while open
            if (modal) in.uiClick = false;
            drawShop();
            if (modal) in.uiClick = clk;
            amountDraw();
            break;
        }
        case ST_SERVICE: drawHud(); drawService(); break;
        case ST_PAUSE: drawHud(); drawPause(); break;
        case ST_SETTINGS: drawHud(); drawSettings(); break;
        case ST_HELP: drawHud(); drawHelp(); break;
        case ST_QUEST: drawHud(); drawQuestPanel(); break;
        case ST_GATE: drawHud(); drawGatePanel(); break;
        case ST_INVENTORY: case ST_CHEST: {
            drawHud();
            // while the amount slider is open it takes every click: the screen under it is drawn but does not react
            const bool modal = amtCtx != AMT_NONE, clk = in.uiClick;
            if (modal) in.uiClick = false;
            if (state == ST_INVENTORY) drawInventory();
            else drawChest();
            if (modal) in.uiClick = clk;
            amountDraw();
            break;
        }
        case ST_MAP: drawWorldMap(); break;  // (over the dimmed world, without the HUD)
    }
    if (state == ST_PLAY && !p.dead) drawReticle();
    else sprS(A.cursor, (int)in.mx, (int)in.my);
    if (cfg.showFps) {  // the frame counter (Settings > Video)
        const std::string fs = "FPS " + S((int)(fpsAvg + 0.5f));
        fillRect(LW - textW(fs) - 8, 2, textW(fs) + 6, 10, RGBA(10, 6, 4, 160));
        text(LW - textW(fs) - 5, 4, fs, fpsAvg < 30.f ? C_RED : C_GOLD, 1, false);
    }
    if (travelFade > 0) fillRect(0, 0, LW, LH, RGBA(0, 0, 0, (int)(255.f * std::min(1.f, travelFade / 0.6f))));  // fade in after a trip
}

void Game::present() {
    SDL_SetRenderTarget(ren, nullptr);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    int ww, wh;
    SDL_GetRendererOutputSize(ren, &ww, &wh);
    float sc;
    if (LS > 1) {  // the canvas has LS x the logical pixels: whole multiples of it stay perfectly sharp, anything between is filtered (linear)
        sc = std::min((float)ww / (LW * LS), (float)wh / (LH * LS));
        if (sc >= 1.f && fabsf(sc - roundf(sc)) < 0.02f) sc = roundf(sc);
        sc *= LS;  // real pixels per logical pixel
    } else {
        sc = std::min((float)ww / LW, (float)wh / LH);
        if (sc >= 1.f) sc = floorf(sc);
    }
    int dw = (int)(LW * sc), dh = (int)(LH * sc);
    offX = (float)((ww - dw) / 2);
    offY = (float)((wh - dh) / 2);
    presScale = sc;
    SDL_Rect dst{(int)offX, (int)offY, dw, dh};
    SDL_RenderCopy(ren, canvas, nullptr, &dst);
    devui::draw(*this);  // the F1 developer panel, over the window (nothing unless built with GATHERING_DEV_UI)
    SDL_RenderPresent(ren);
}

void Game::saveCanvas(const char* path) {
    SDL_SetRenderTarget(ren, canvas);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, LW, LH, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
}
