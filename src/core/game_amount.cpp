// game_amount.cpp - the amount slider: a small popup over the pack or a storage chest for taking PART of a stack.
// A draggable bar plus -, +, HALF and MAX; the action is a drop (pack) or a move to / from the chest.
#include <algorithm>
#include "core/game.hpp"

// A 1..maxv slider bar at (tx, ty) tw wide: draws the track, fill and knob; a press on it starts a drag that sets val (drag is the caller's flag).
void Game::sliderBar(int tx, int ty, int tw, int& val, int maxv, bool& drag) {
    const bool usable = maxv > 1;
    fillRect(tx, ty, tw, 4, HEX(0x1a120b));
    const float frac = usable ? (val - 1) / (float)(maxv - 1) : 0.f;
    if (usable) fillRect(tx, ty, (int)(tw * frac), 4, HEX(0x8ec2f0));
    fillRect(tx + (int)((tw - 3) * frac), ty - 2, 4, 8, !usable ? HEX(0x5a4a3a) : drag ? C_GOLD : HEX(0xe8dcc0));
    if (!usable) {
        drag = false;
        return;
    }
    if (in.uiClick && inRect(tx - 3, ty - 5, tw + 6, 14)) {
        drag = true;
        in.uiClick = false;
        in.lPressed = false;
    }
    if (!in.l) drag = false;
    if (drag) val = std::max(1, std::min(maxv, 1 + (int)lroundf((in.mx - tx) / (float)(tw - 1) * (maxv - 1))));
}

namespace {
constexpr int PW = 176, PH = 78;
}

void Game::amountOpen(int ctx, int slot, int fromPack) {
    Inventory* inv = ctx == AMT_CHEST ? (fromPack ? &p.inv : openInv()) : &p.inv;
    if (!inv || slot < 0 || slot >= inv->capacity() || inv->slots[slot].n < 2) return;
    amtCtx = ctx;
    amtSlot = slot;
    amtFromPack = fromPack;
    amtMax = inv->slots[slot].n;
    amtVal = std::max(1, amtMax / 2);  // starts at half: the usual reason to split
    amtDrag = false;
}

void Game::amountOpenSell(int id, int q, int n) {
    if (n < 2) return;
    amtCtx = AMT_SELL;
    amtId = id;
    amtQ = q;
    amtMax = n;
    amtVal = std::max(1, n / 2);
    amtDrag = false;
}

void Game::amountUpdate() {
    if (in.escPressed || in.invToggle) {
        amountClose();
        in.escPressed = false;  // closes only the slider, not the screen under it
        in.invToggle = false;
        return;
    }
    amtVal = std::max(1, std::min(amtMax, amtVal + (in.right && !navPrev[3] ? 1 : 0) - (in.left && !navPrev[2] ? 1 : 0) - in.wheel));
    navPrev[2] = in.left;
    navPrev[3] = in.right;
}

void Game::amountDraw() {
    if (amtCtx == AMT_NONE) return;
    Inventory* inv = amtCtx == AMT_CHEST ? (amtFromPack ? &p.inv : openInv()) : &p.inv;
    Slot sold;  // AMT_SELL: the shop row, shown like a slot
    if (amtCtx == AMT_SELL) {
        const bool mat = amtId < 0;
        const int have = mat ? p.mat(-amtId - 1) : sellable(amtId, amtQ);
        if (have < 1 || state != ST_SHOP) {  // sold out, or the shop was left
            amountClose();
            return;
        }
        sold = Slot{mat ? itemMat(-amtId - 1) : amtId, have, amtQ};
    } else if (!inv || amtSlot < 0 || amtSlot >= inv->capacity() || inv->slots[amtSlot].empty()) {  // the stack is gone: nothing to split
        amountClose();
        return;
    }
    const Slot& sl = amtCtx == AMT_SELL ? sold : inv->slots[amtSlot];
    amtMax = sl.n;
    amtVal = std::max(1, std::min(amtMax, amtVal));
    const ItemDef& D = item(sl.item);
    const int x = (LW - PW) / 2, y = (LH - PH) / 2;
    fillRect(0, 0, LW, LH, RGBA(0, 0, 0, 90));
    panel(x, y, PW, PH, HEX(0x4a3524));
    fillRect(x + 3, y + 3, PW - 6, 12, UI_BG);
    const char* verb = amtCtx == AMT_DROP ? "DROP" : amtCtx == AMT_SELL ? "SELL" : amtFromPack ? "STORE" : "TAKE";
    text(x + 8, y + 6, std::string(verb) + " HOW MANY?", C_GOLD);
    // the item
    if (itemHasQuality(sl.item)) qualityFrame(x + 6, y + 18, 15, 15, sl.q);
    icon(itemIcon(sl.item), x + 14, y + 26, 12);
    text(x + 25, y + 20, D.name, itemHasQuality(sl.item) && sl.q > Q_NORMAL ? qualityCol(sl.q) : C_TEXT, 1, false);
    text(x + 25, y + 28, "OF " + S(amtMax) + "   " + kgNum(D.weight * amtVal) + " KG", C_DIM, 1, false);
    // the slider
    sliderBar(x + 10, y + 40, PW - 20, amtVal, amtMax, amtDrag);
    const std::string vs = S(amtVal);
    text(x + PW - 8 - textW(vs, 2), y + 20, vs, C_GOLD, 2);  // the chosen amount, big, at the right
    // buttons
    int bx = x + 8;
    const int by = y + 52;
    if (button(bx, by, 16, 13, "-", amtVal > 1, HEX(0x4a3a2a))) amtVal--;
    bx += 18;
    if (button(bx, by, 16, 13, "+", amtVal < amtMax, HEX(0x4a3a2a))) amtVal++;
    bx += 18;
    if (button(bx, by, 28, 13, "1/2", amtMax > 1, HEX(0x4a3a2a))) amtVal = std::max(1, amtMax / 2);
    bx += 30;
    if (button(bx, by, 28, 13, "MAX", true, HEX(0x4a3a2a))) amtVal = amtMax;
    bx = x + PW - 8 - 28;
    if (button(bx, by, 28, 13, "X", true, HEX(0x7a3a2a))) {
        amountClose();
        return;
    }
    if (button(bx - 32, by, 30, 13, "OK", true, HEX(0x5d7a2e)) || in.enterPressed) {
        const int n = amtVal, slot = amtSlot, ctx = amtCtx, fromPack = amtFromPack;
        amountClose();
        in.enterPressed = false;
        if (ctx == AMT_SELL) {
            if (amtId < 0) sellMat(-amtId - 1, n);
            else sellItem(amtId, amtQ, n);
        } else if (ctx == AMT_DROP) dropSlot(slot, n);
        else chestMove(fromPack, slot, n);
    }
}
