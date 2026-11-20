// game_map.cpp - the full-map screen (M): the explored part of the current map, landmarks, gates and where to look next.
#include <cstdio>
#include "core/game.hpp"
#include "npcs/town.hpp"
#include "world/gates.hpp"

// compass word for a direction (only used in the hint text)
static const char* compassWord(float dx, float dy) {
    const float ax = fabsf(dx), ay = fabsf(dy);
    if (ay < ax * 0.4f) return dx > 0 ? "EAST" : "WEST";
    if (ax < ay * 0.4f) return dy > 0 ? "SOUTH" : "NORTH";
    if (dy > 0) return dx > 0 ? "SOUTH-EAST" : "SOUTH-WEST";
    return dx > 0 ? "NORTH-EAST" : "NORTH-WEST";
}

// Small pixel icons for the maps. Letters: r the accent colour, w cream wall, d dark, y gold, g green, c light, b blue.
int Game::bldMapIcon(int k) {
    switch (k) {
        case BLD_SMITH: return MI_ANVIL;
        case BLD_TANNER: return MI_LEATHER;
        case BLD_WORKSHOP: return MI_TOOLS;
        case BLD_SHOP: return MI_COIN;
        case BLD_FLOWER: return MI_FLOWER;
        case BLD_SALON: return MI_SCISSORS;
        default: return MI_HOUSE;
    }
}

void Game::mapIcon(int kind, int cx, int cy, int sz, u32 col) {
    static const char* const HOUSE7[7] = {"...r...", "..rrr..", ".rrrrr.", "rrrrrrr", ".wwwww.", ".wwdww.", ".wwdww."};
    static const char* const HOUSE5[5] = {"..r..", ".rrr.", "rrrrr", ".wdw.", ".wdw."};
    static const char* const PORT7[7] = {"..rrr..", ".r...r.", "r..c..r", "r.ccc.r", "r..c..r", ".r...r.", "..rrr.."};
    static const char* const PORT5[5] = {".rrr.", "r...r", "r.c.r", "r...r", ".rrr."};
    static const char* const BADGE7[7] = {"..yyy..", ".yydyy.", "yyydyyy", "yyydyyy", "yyyyyyy", ".yydyy.", "..yyy.."};  // "!"
    static const char* const BADGE5[5] = {".yyy.", "yydyy", "yydyy", "yyyyy", ".ydy."};
    static const char* const ASK7[7] = {"..ggg..", ".ggdgg.", "ggggdgg", "gggdggg", "ggggggg", ".ggdgg.", "..ggg.."};  // "?"
    static const char* const ASK5[5] = {".ggg.", "gdddg", "gggdg", "ggggg", ".gdg."};
    static const char* const ELD7[7] = {"..bbb..", ".bbbbb.", ".bcbcb.", ".bbbbb.", "..bbb..", ".bbbbb.", "bbbbbbb"};
    static const char* const ELD5[5] = {".bbb.", "bcbcb", ".bbb.", "bbbbb", "b.b.b"};
    static const char* const LAIR7[7] = {"r.....r", "rr.r.rr", "rrrrrrr", "rwrrrwr", ".rrrrr.", ".rwrwr.", "..r.r.."};  // a horned skull
    static const char* const LAIR5[5] = {"r...r", "rrrrr", "rwrwr", ".rrr.", ".r.r."};
    static const char* const ANVIL7[7] = {"rrrrrr.", "rrrrrrr", ".rrrrr.", "..rrr..", "..rrr..", ".rrrrr.", "rrrrrrr"};
    static const char* const ANVIL5[5] = {"rrrr.", "rrrrr", ".rrr.", "..r..", ".rrr."};
    static const char* const LEATH7[7] = {"r.rrr.r", "rrrrrrr", ".rrrrr.", "rrrrrrr", ".rrrrr.", "rr...rr", "r.....r"};  // a hide
    static const char* const LEATH5[5] = {"r.r.r", "rrrrr", ".rrr.", "rrrrr", "r...r"};
    static const char* const TOOLS7[7] = {".rrrrr.", "rrrrrrr", ".rrrrr.", "..www..", "..www..", "..www..", "..www.."};  // a hammer
    static const char* const TOOLS5[5] = {"rrrrr", "rrrrr", ".rrr.", "..w..", "..w.."};
    static const char* const COIN7[7] = {".yyyyy.", "yyydyyy", "yydyyyy", "yyydyyy", "yyyydyy", "yyydyyy", ".yyyyy."};  // a dollar coin
    static const char* const COIN5[5] = {".yyy.", "yydyy", "ydddy", "yydyy", ".yyy."};
    static const char* const FLOW7[7] = {"..rrr..", ".rryrr.", "..rrr..", "...g...", "..gg...", "...g...", "...g..."};
    static const char* const FLOW5[5] = {".rrr.", "rryrr", ".rrr.", "..g..", ".gg.."};
    static const char* const SCIS7[7] = {"w.....w", ".w...w.", "..w.w..", "...w...", "..r.r..", ".rr.rr.", ".r...r."};  // scissors
    static const char* const SCIS5[5] = {"w...w", ".w.w.", "..w..", ".r.r.", "rr.rr"};
    const bool big = sz >= 7;
    const char* const* pat = nullptr;
    switch (kind) {
        case MI_HOUSE: pat = big ? HOUSE7 : HOUSE5; break;
        case MI_PORTAL: pat = big ? PORT7 : PORT5; break;
        case MI_ANVIL: pat = big ? ANVIL7 : ANVIL5; break;
        case MI_LEATHER: pat = big ? LEATH7 : LEATH5; break;
        case MI_TOOLS: pat = big ? TOOLS7 : TOOLS5; break;
        case MI_COIN: pat = big ? COIN7 : COIN5; break;
        case MI_FLOWER: pat = big ? FLOW7 : FLOW5; break;
        case MI_SCISSORS: pat = big ? SCIS7 : SCIS5; break;
        case MI_LAIR: pat = big ? LAIR7 : LAIR5; break;
        case MI_QUEST_NEW: pat = big ? BADGE7 : BADGE5; break;
        case MI_QUEST_READY: pat = big ? ASK7 : ASK5; break;
        default: pat = big ? ELD7 : ELD5; break;
    }
    const int n = big ? 7 : 5;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            u32 c;
            switch (pat[y][x]) {
                case '.': continue;
                case 'r': c = col; break;
                case 'w': c = HEX(0xefe2c0); break;
                case 'd': c = HEX(0x3a2616); break;
                case 'y': c = C_GOLD; break;
                case 'g': c = C_GREEN; break;
                case 'c': c = HEX(0xf4f0ea); break;
                default: c = C_BLUE; break;
            }
            fillRect(cx - n / 2 + x, cy - n / 2 + y, 1, 1, c);
        }
}

void Game::updateWorldMap(float dt) {
    updateFx(dt);
    if (in.escPressed || in.mapToggle) {
        state = ST_PLAY;
        audio.play(S_CLICK);
    }
}

void Game::drawWorldMap() {
    const int K = 3;  // screen pixels per minimap cell
    const int mw = MM_W * K, mh = MM_H * K;
    const int px = (LW - mw) / 2 - 2, py = 18;
    fillRect(0, 0, LW, LH, RGBA(0, 0, 0, 150));
    panel(px, py, mw + 4, mh + 4, HEX(0x2b1d12));
    const int ox = px + 2, oy = py + 2;
    const BiomeDef& B = BIOME[curBiome];
    textC(LW / 2, 6, std::string(MAPS[world.mapId].name) + " MAP", B.col, 1, true);
    SDL_Rect d{ox, oy, mw, mh};
    SDL_SetTextureAlphaMod(miniTex, 255);
    SDL_RenderCopy(ren, miniTex, nullptr, &d);
    // fog of war: unexplored cells are covered (runs of cells per row)
    const bool fog = seen.size() == (size_t)MM_W * MM_H;
    if (fog)
        for (int cy = 0; cy < MM_H; cy++)
            for (int cx = 0; cx < MM_W;) {
                if (seen[(size_t)cy * MM_W + cx] == '1') { cx++; continue; }
                int e = cx;
                while (e < MM_W && seen[(size_t)cy * MM_W + e] == '0') e++;
                fillRect(ox + cx * K, oy + cy * K, (e - cx) * K, K, HEX(0x1a1410));
                cx = e;
            }
    auto sx = [&](float wx) { return ox + (int)(wx / TS / MM_DIV * K); };
    auto sy = [&](float wy) { return oy + (int)(wy / TS / MM_DIV * K); };
    // the shop and the elder (home only)
    if (world.mapId == B_HOME) {
        mapIcon(MI_COIN, sx(world.hut.x), sy(world.hut.y), 7, C_GOLD);
        text(sx(world.hut.x) + 6, sy(world.hut.y) - 3, "SHOP", C_GOLD);
        for (int k = 0; k < BLD_COUNT; k++) {
            const int bx = sx(bldDoorX(k, COX)), by = sy(bldBaseY(k, COY) - 8.f);
            mapIcon(bldMapIcon(k), bx, by, 7, BUILDINGS[k].roof);
            if (inRect(bx - 4, by - 4, 9, 9)) {
                const int lx = std::max(ox + 1, std::min(ox + mw - textW(BUILDINGS[k].name) - 1, bx - textW(BUILDINGS[k].name) / 2));
                text(lx, by + 6, BUILDINGS[k].name, C_TEXT);
            }
        }
        const int ex = sx(world.questGiver.x), ey = sy(world.questGiver.y);
        mapIcon(!questActive ? MI_QUEST_NEW : questReady() ? MI_QUEST_READY : MI_ELDER, ex, ey, 7, C_BLUE);
        text(ex + 6, ey - 3, !questActive ? "QUEST" : questReady() ? "TURN IN" : "ELDER", !questActive ? C_GOLD : questReady() ? C_GREEN : C_BLUE);
    }
    // gates, named for where they lead
    for (int i = 0; i < MAPS[world.mapId].nPortals; i++) {
        const Portal& pt = MAPS[world.mapId].portals[i];
        const int gx = ox + (pt.zone.x0 + pt.zone.x1) / 2 * K / MM_DIV, gy = oy + (pt.zone.y0 + pt.zone.y1) / 2 * K / MM_DIV;
        mapIcon(MI_PORTAL, gx, gy, 7, BIOME[pt.toMap].col);
        const std::string lb = std::string("TO ") + MAPS[pt.toMap].shortName + (gateOpen[pt.toMap] ? "" : " (SEALED)");
        const int lx = std::max(ox + 1, std::min(ox + mw - textW(lb) - 1, gx - textW(lb) / 2));
        text(lx, std::min(oy + mh - 8, gy + 5), lb, BIOME[pt.toMap].col);
    }
    // a boss lair that has been found and whose boss is back
    if (inWilds() && bossForBiome(world.mapId) && bossSeen[world.mapId] && bossCool[world.mapId] <= 0) {
        const V2 lp = bossLairPos();
        const int lx0 = sx(lp.x), ly0 = sy(lp.y);
        mapIcon(MI_LAIR, lx0, ly0, 7, C_RED);
        const std::string lb = std::string("LAIR: ") + bossForBiome(world.mapId)->name;
        text(std::max(ox + 1, std::min(ox + mw - textW(lb) - 1, lx0 - textW(lb) / 2)), ly0 + 5, lb, C_RED);
    }
    // discovered landmarks with their names
    for (auto& l : world.landmarks) {
        if (!l.found) continue;
        fillRect(sx(l.x) - 1, sy(l.y) - 1, 3, 3, BIOME[l.biome].col);
        const std::string nm = l.name;
        const int lx = std::max(ox + 1, std::min(ox + mw - textW(nm) - 1, sx(l.x) - textW(nm) / 2));
        text(lx, sy(l.y) + 3, nm, C_TEXT);
    }
    // the gate shortcut: every gate and whether it is open; click one to see what it asks for
    text(4, oy + 2, "GATES", C_GOLD);
    for (int gi = 0; gi < GATE_N; gi++) {
        const GateReq& gr = GATES[gi];
        const int gy2 = oy + 12 + gi * 26;
        const bool open = gateOpen[gr.map];
        if (button(4, gy2, 70, 22, std::string(MAPS[gr.map].name) + (open ? " OPEN" : " SEALED"), true, open ? HEX(0x3a5a2e) : HEX(0x5a3a2a))) openGateView(gr.map);
    }
    for (auto& a : animals)  // animals the scouting companion sniffed out
        if (!a.dead && !a.gone && a.scoutT > 0 && (a.scoutT > 2.f || (((int)(timeAcc * 4)) & 1))) fillRect(sx(a.pos.x), sy(a.pos.y), 3, 3, ANIMAL[a.kind].aggro ? C_RED : C_GREEN);
    // carcasses that still hold materials
    for (auto& a : animals)
        if (a.dead && a.totalRemain > 0) {
            fillRect(sx(a.pos.x), sy(a.pos.y), 2, 2, C_RED);
            if (a.tracked && ((int)(timeAcc * 2)) & 1) {  // a pulsing ring marks the quarry that bled out of sight
                const int rx = sx(a.pos.x) - 3, ry = sy(a.pos.y) - 3;
                fillRect(rx, ry, 8, 1, C_GOLD); fillRect(rx, ry + 7, 8, 1, C_GOLD);
                fillRect(rx, ry, 1, 8, C_GOLD); fillRect(rx + 7, ry, 1, 8, C_GOLD);
            }
        }
    // the hunter
    if (((int)(timeAcc * 3)) & 1) fillRect(sx(p.pos.x) - 2, sy(p.pos.y) - 2, 5, 5, HEX(0xffffff));
    fillRect(sx(p.pos.x) - 1, sy(p.pos.y) - 1, 3, 3, HEX(0x2d5e35));
    // footer: how much is found and where to look next
    const int total = (int)world.landmarks.size();
    const std::string cnt = "LANDMARKS " + S(landmarksFound()) + "/" + S(total);
    if (total > 0) text(ox, oy + mh + 8, cnt, B.col);
    const Landmark* near = nullptr;
    float best = 1e9f;
    for (auto& l : world.landmarks) {
        if (l.found) continue;
        const float dd = dist(p.pos, V2(l.x, l.y));
        if (dd < best) { best = dd; near = &l; }
    }
    if (near) {
        const std::string hint = std::string("NEAREST UNDISCOVERED: ") + compassWord(near->x - p.pos.x, near->y - p.pos.y);
        text(ox + mw - textW(hint), oy + mh + 8, hint, C_GOLD);
    } else if (total > 0)
        text(ox + mw - textW("EVERY LANDMARK FOUND"), oy + mh + 8, "EVERY LANDMARK FOUND", C_GREEN);
    textC(LW / 2, oy + mh + 18, "M / ESC CLOSE   TAB MINIMAP ON / OFF", C_DIM, 1, true);
}
