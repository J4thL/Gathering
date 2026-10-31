// salon.cpp - Salona Mia's salon: change hairstyle, hair colour, skin tone and tunic colour with a live preview. CONFIRM pays the fee and
// redraws the hunter's real sprites (all directions and walk frames); CANCEL (or leaving) throws the preview away and costs nothing.
// Only the hunter's HunterLook changes: pack, equipment, stats, skills, quests and coins (apart from the fee) are never touched.
#include <algorithm>
#include "game.hpp"

bool Game::applyLook(const HunterLook& want) {
    const HunterLook l = clampLook(want);
    p.look = l;
    if (ren) rebuildPlayerSprites(ren, A, l);
    return true;
}

// CONFIRM: pays the fee and applies the preview. Refused (nothing changes, nothing is charged) with no change or too few coins.
bool Game::confirmSalon() {
    if (salonLook == p.look) return false;
    if (p.coins < SALON_FEE) {
        msg("NOT ENOUGH COINS", C_RED);
        audio.play(S_ERROR);
        return false;
    }
    p.coins -= SALON_FEE;
    applyLook(salonLook);
    audio.play(S_COIN);
    msg("A NEW LOOK!", C_GOLD);
    ftext(p.pos + V2(0, -26), "-" + S(SALON_FEE) + " COINS", C_GOLD, 1.2f);
    save();
    return true;
}

void Game::openSalon() {
    salonLook = p.look;
}

void Game::drawSalon() {
    const int X = 36, Y = 12, W = 328, H = 201;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, "SALON", C_GOLD);
    {
        std::string c = S(p.coins);
        const int cw = textW(c);
        icon(A.coin, X + W - 10 - cw - 8, Y + 10, 10);
        text(X + W - 8 - cw, Y + 7, c, C_GOLD);
    }
    // ---- preview: the hunter as they would look, turning slowly
    panel(X + 6, Y + 22, 80, 110, HEX(0x2b1d12));
    {
        if (!salonPrev[0].ok() || salonPrevLook != salonLook) {
            for (auto& s : salonPrev) {
                if (s.t) SDL_DestroyTexture(s.t);
                if (s.flash) SDL_DestroyTexture(s.flash);
                s = Spr();
            }
            for (int d = 0; d < 3; d++) salonPrev[d] = makeSpr(ren, mkPlayer(d, 0, salonLook), 9, 24, false);
            salonPrevLook = salonLook;
        }
        const int d = ((int)(timeAcc * 0.5f)) % 4;  // front, side, back, side
        const int dir = d == 0 ? 0 : d == 2 ? 1 : 2;
        const Spr& s = salonPrev[dir];
        SDL_Rect dst{X + 6 + 40 - s.w * 2 + 0, Y + 30, s.w * 4, s.h * 4};
        SDL_RenderCopyEx(ren, s.t, nullptr, &dst, 0, nullptr, d == 3 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        textC(X + 46, Y + 124, "PREVIEW", C_DIM, 1, false);
    }
    panel(X + 6, Y + 138, 80, 46, HEX(0x2b1d12));
    {
        const Spr& k = A.T.npc[BLD_SALON][((int)(timeAcc * 1.6f)) & 1];
        SDL_Rect d{X + 6 + 10, Y + 140, k.w * 2 - 8, k.h * 2 - 8};
        SDL_RenderCopy(ren, k.t, nullptr, &d);
        textC(X + 60, Y + 150, "SALONA", C_DIM, 1, false);
        textC(X + 60, Y + 159, "MIA", C_DIM, 1, false);
        const char* lines[] = {"LOOKING GOOD!", "SIT, SIT!", "A FRESH START!"};
        textC(X + 46, Y + 174, lines[((int)(titleT + timeAcc * 0.15f)) % 3], C_TEXT, 1, false);
    }
    // ---- options
    const int x0 = X + 94, cw = 226;
    auto label = [&](int y, const char* a, const char* b) {
        text(x0, y, a, C_GOLD);
        text(x0 + cw - textW(b), y, b, C_TEXT);
    };
    auto swatches = [&](int y, int n, const u32* cols, int sel, int& out) {
        const int sz = 16, gap = 4;
        for (int i = 0; i < n; i++) {
            const int sx = x0 + i * (sz + gap);
            fillRect(sx - 1, y - 1, sz + 2, sz + 2, i == sel ? C_GOLD : HEX(0x1a120b));
            fillRect(sx, y, sz, sz, cols[i]);
            if (inRect(sx, y, sz, sz) && in.uiClick) {
                in.uiClick = false;
                in.lPressed = false;
                audio.play(S_CLICK);
                out = i;
            }
        }
    };
    int y = Y + 22;
    label(y, "HAIRSTYLE", HAIR_NAME[salonLook.hair]);
    {
        const int bw = (cw - 4 * 3) / 5;
        for (int i = 0; i < LOOK_HAIR_N; i++)
            if (button(x0 + i * (bw + 3), y + 11, bw, 14, HAIR_NAME[i], true, salonLook.hair == i ? HEX(0x92693a) : HEX(0x5a4028))) salonLook.hair = i;
    }
    y += 34;
    label(y, "HAIR COLOUR", HAIRCOL_NAME[salonLook.color]);
    swatches(y + 11, LOOK_COLOR_N, HAIRCOL, salonLook.color, salonLook.color);
    y += 36;
    label(y, "SKIN", SKIN_NAME[salonLook.skin]);
    swatches(y + 11, LOOK_SKIN_N, SKINCOL, salonLook.skin, salonLook.skin);
    y += 36;
    {
        u32 oc[LOOK_OUTFIT_N];
        for (int i = 0; i < LOOK_OUTFIT_N; i++) oc[i] = OUTFITCOL[i].base;
        label(y, "TUNIC", OUTFIT_NAME[salonLook.outfit]);
        swatches(y + 11, LOOK_OUTFIT_N, oc, salonLook.outfit, salonLook.outfit);
    }
    y += 38;
    const bool changed = salonLook != p.look;
    const bool afford = p.coins >= SALON_FEE;
    const std::string lab = !changed ? "NO CHANGES YET" : afford ? "CONFIRM - " + S(SALON_FEE) + " COINS" : "NEED " + S(SALON_FEE) + " COINS";
    if (button(x0, y, 140, 17, lab, changed && afford, HEX(0x5d7a2e))) {
        confirmSalon();
    }
    if (button(x0 + 146, y, 80, 17, "CANCEL", changed, HEX(0x7a3a2e))) salonLook = p.look;
    textC(X + W / 2, Y + H - 10, "ESC OR E: LEAVE     CONFIRM APPLIES THE LOOK FOR GOOD     CANCEL UNDOES THE PREVIEW", C_DIM, 1, false);
}
