// game_map.cpp - the full-map screen (M): the explored part of the current map, landmarks, gates and where to look next.
#include <cstdio>
#include "core/game.hpp"

// compass word for a direction (only used in the hint text)
static const char* compassWord(float dx, float dy) {
    const float ax = fabsf(dx), ay = fabsf(dy);
    if (ay < ax * 0.4f) return dx > 0 ? "EAST" : "WEST";
    if (ax < ay * 0.4f) return dy > 0 ? "SOUTH" : "NORTH";
    if (dy > 0) return dx > 0 ? "SOUTH-EAST" : "SOUTH-WEST";
    return dx > 0 ? "NORTH-EAST" : "NORTH-WEST";
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
        fillRect(sx(world.hut.x) - 2, sy(world.hut.y) - 2, 5, 5, C_GOLD);
        text(sx(world.hut.x) + 5, sy(world.hut.y) - 3, "SHOP", C_GOLD);
        fillRect(sx(world.questGiver.x) - 1, sy(world.questGiver.y) - 1, 4, 4, C_BLUE);
        text(sx(world.questGiver.x) + 5, sy(world.questGiver.y) - 3, "ELDER", C_BLUE);
    }
    // gates, named for where they lead
    for (int i = 0; i < MAPS[world.mapId].nPortals; i++) {
        const Portal& pt = MAPS[world.mapId].portals[i];
        const int gx = ox + (pt.zone.x0 + pt.zone.x1) / 2 * K / MM_DIV, gy = oy + (pt.zone.y0 + pt.zone.y1) / 2 * K / MM_DIV;
        fillRect(gx - 2, gy - 2, 5, 5, BIOME[pt.toMap].col);
        const std::string lb = std::string("TO ") + MAPS[pt.toMap].shortName + (gateOpen[pt.toMap] ? "" : " (SEALED)");
        const int lx = std::max(ox + 1, std::min(ox + mw - textW(lb) - 1, gx - textW(lb) / 2));
        text(lx, std::min(oy + mh - 8, gy + 5), lb, BIOME[pt.toMap].col);
    }
    // discovered landmarks with their names
    for (auto& l : world.landmarks) {
        if (!l.found) continue;
        fillRect(sx(l.x) - 1, sy(l.y) - 1, 3, 3, BIOME[l.biome].col);
        const std::string nm = l.name;
        const int lx = std::max(ox + 1, std::min(ox + mw - textW(nm) - 1, sx(l.x) - textW(nm) / 2));
        text(lx, sy(l.y) + 3, nm, C_TEXT);
    }
    // carcasses that still hold materials
    for (auto& a : animals)
        if (a.dead && a.totalRemain > 0) fillRect(sx(a.pos.x), sy(a.pos.y), 2, 2, C_RED);
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
