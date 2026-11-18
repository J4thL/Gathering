// game_gates.cpp - the sealed gates: walking into one opens the offering panel; bringing what it asks for opens it for good.
#include "core/game.hpp"
#include "world/gates.hpp"

bool Game::gateMet(const GateReq& g, bool* skillOk, bool* prereqOk) const {
    const bool pr = g.prereq < 0 || gateOpen[g.prereq];
    const bool sk = p.sk.level(SK_HUNTING) >= g.huntLevel;
    bool mats = true;
    for (int i = 0; i < g.nMat; i++)
        if (p.mat(g.mats[i].mat) < g.mats[i].n) mats = false;
    if (skillOk) *skillOk = sk;
    if (prereqOk) *prereqOk = pr;
    return pr && sk && mats;
}

// walking into a sealed gate: stop at it and show what it asks for
void Game::openGatePanel(int toMap, int side) {
    gateTarget = toMap;
    gateView = false;
    state = ST_GATE;
    const V2 back = side == 0 ? V2(-24, 0) : side == 3 ? V2(24, 0) : side == 1 ? V2(0, -24) : V2(0, 24);
    p.pos += back;
    travelCd = 0.4f;
    audio.play(S_CLICK, 0.5f);
}

bool Game::offerAtGate(int toMap) {
    const GateReq* g = gateFor(toMap);
    if (!g || gateOpen[toMap] || !gateMet(*g)) return false;
    for (int i = 0; i < g->nMat; i++) p.inv.remove(itemMat(g->mats[i].mat), g->mats[i].n);
    gateOpen[toMap] = true;
    msg(std::string("THE ") + MAPS[toMap].name + " GATE OPENS", BIOME[toMap].col);
    audio.play(S_CRAFT);
    shakeAmt = std::max(shakeAmt, 2.f);
    save();
    return true;
}

void Game::updateGatePanel(float dt) {
    updateFx(dt);
    if (in.escPressed || in.ePressed) {
        state = gateView ? ST_MAP : ST_PLAY;
        audio.play(S_CLICK);
    } else if (!gateView && in.enterPressed && offerAtGate(gateTarget)) state = ST_PLAY;
}

void Game::drawGatePanel() {
    const GateReq* g = gateFor(gateTarget);
    if (!g) {
        state = ST_PLAY;
        return;
    }
    fillRect(0, 0, LW, LH, RGBA(10, 6, 4, 140));
    const int X = 80, Y = 40, W = 240, H = 140;
    panel(X, Y, W, H, HEX(0x4a3524));
    fillRect(X + 3, Y + 3, W - 6, 14, UI_BG);
    text(X + 8, Y + 7, std::string(MAPS[g->map].name) + " GATE - " + (gateOpen[g->map] ? "OPEN" : "SEALED"), BIOME[g->map].col);
    bool skillOk = false, prereqOk = false;
    const bool ok = gateMet(*g, &skillOk, &prereqOk);
    int y = Y + 24;
    if (!prereqOk) {
        text(X + 10, y, std::string("OPEN THE ") + MAPS[g->prereq].name + " GATE FIRST", C_RED);
        y += 12;
    }
    text(X + 10, y, "THE GATE ASKS FOR AN OFFERING", C_TEXT);
    y += 14;
    const std::string hs = std::string("HUNTING LEVEL ") + S(g->huntLevel);
    text(X + 10, y, hs, skillOk ? C_GREEN : C_RED);
    text(X + W - 10 - textW("YOU: " + S(p.sk.level(SK_HUNTING))), y, "YOU: " + S(p.sk.level(SK_HUNTING)), C_DIM);
    y += 12;
    for (int i = 0; i < g->nMat; i++) {
        const int m = g->mats[i].mat, have = p.mat(m), need = g->mats[i].n;
        icon(A.matIcon[m], X + 16, y + 4, 10);
        text(X + 26, y, MAT_NAME[m], have >= need ? C_GREEN : C_TEXT);
        const std::string hv = S(have) + "/" + S(need);
        text(X + W - 10 - textW(hv), y, hv, have >= need ? C_GREEN : C_RED);
        y += 12;
    }
    if (gateOpen[g->map]) text(X + 10, Y + H - 19, "THIS GATE IS OPEN", C_GREEN);
    else if (gateView) text(X + 10, Y + H - 19, "MAKE THE OFFERING AT THE GATE", C_DIM);
    else if (button(X + 10, Y + H - 22, 100, 14, "MAKE THE OFFERING", ok, HEX(0x5d7a2e)) && offerAtGate(gateTarget)) state = ST_PLAY;
    if (button(X + W - 70, Y + H - 22, 60, 14, "BACK", true, HEX(0x7a3a2a))) state = gateView ? ST_MAP : ST_PLAY;
}
